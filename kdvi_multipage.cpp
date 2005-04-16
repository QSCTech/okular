#include <config.h>
#include <kaction.h>
#include <kaboutdata.h>
#include <kaboutdialog.h>
#include <kapplication.h>
#include <kbugreport.h>
#include <kconfigdialog.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstdaction.h>
#include <ktip.h>
#include <qtimer.h>

#include <kparts/part.h>
#include <kparts/genericfactory.h>

#include "../config.h"
#include "../kviewshell/marklist.h"
#include "documentPagePixmap.h"
#include "documentWidget.h"
#include "fontpool.h"
#include "kdvi_multipage.h"
#include "kviewpart.h"
#include "performanceMeasurement.h"
#include "prefs.h"
#include "kprinterwrapper.h"
#include "dviWidget.h"

#include "optionDialogFontsWidget.h"
#include "optionDialogSpecialWidget.h"

#include <qlabel.h>

//#define KDVI_MULTIPAGE_DEBUG

#ifdef PERFORMANCE_MEASUREMENT
// These objects are explained in the file "performanceMeasurement.h"
QTime performanceTimer;
int  performanceFlag = 0;
#endif

typedef KParts::GenericFactory<KDVIMultiPage> KDVIMultiPageFactory;
K_EXPORT_COMPONENT_FACTORY(kdvipart, KDVIMultiPageFactory)

KDVIMultiPage::KDVIMultiPage(QWidget *parentWidget, const char *widgetName, QObject *parent,
                             const char *name, const QStringList& args)
  : KMultiPage(parentWidget, widgetName, parent, name), DVIRenderer(parentWidget)
{
#ifdef PERFORMANCE_MEASUREMENT
  performanceTimer.start();
#endif

  timer_id = -1;
  setInstance(KDVIMultiPageFactory::instance());

  printer = 0;

  // Points to the same object as renderer to avoid downcasting.
  // FIXME: Remove when the API of the Renderer-class is finished.
  DVIRenderer.setName("DVI renderer");
  setRenderer(&DVIRenderer);

  docInfoAction    = new KAction(i18n("Document &Info"), 0, &DVIRenderer, SLOT(showInfo()), actionCollection(), "info_dvi");

  embedPSAction      = new KAction(i18n("Embed External PostScript Files..."), 0, this, SLOT(slotEmbedPostScript()), actionCollection(), "embed_postscript");

  new KAction(i18n("Enable All Warnings && Messages"), 0, this, SLOT(doEnableWarnings()), actionCollection(), "enable_msgs");
  exportPSAction     = new KAction(i18n("PostScript..."), 0, &DVIRenderer, SLOT(exportPS()), actionCollection(), "export_postscript");
  exportPDFAction    = new KAction(i18n("PDF..."), 0, &DVIRenderer, SLOT(exportPDF()), actionCollection(), "export_pdf");
  exportTextAction   = new KAction(i18n("Text..."), 0, this, SLOT(doExportText()), actionCollection(), "export_text");

  KStdAction::tipOfDay(this, SLOT(showTip()), actionCollection(), "help_tipofday");

  setXMLFile("kdvi_part.rc");

  readSettings();
  preferencesChanged();

  enableActions(false);
  // Show tip of the day, when the first main window is shown.
  QTimer::singleShot(0,this,SLOT(showTipOnStart()));
}

KAboutData* KDVIMultiPage::createAboutData()
{
  KAboutData* about = new KAboutData("kdvi", I18N_NOOP("KDVI"), "1.3",
                      I18N_NOOP("A previewer for Device Independent files (DVI files) produced by the TeX typesetting system."),
                     KAboutData::License_GPL,
                     "Markku Hinhala, Stephan Kebekus",
                     I18N_NOOP("This program displays Device Independent (DVI) files which are produced by the TeX typesetting system.\n"
                     "KDVI 1.3 is based on original code from KDVI version 0.43 and xdvik."));

  about->addAuthor ("Stefan Kebekus",
                    I18N_NOOP("Current Maintainer."),
                    "kebekus@kde.org",
                    "http://www.mi.uni-koeln.de/~kebekus");

  about->addAuthor ("Markku Hinhala", I18N_NOOP("Author of kdvi 0.4.3"));
  about->addAuthor ("Nicolai Langfeldt", I18N_NOOP("Maintainer of xdvik"));
  about->addAuthor ("Paul Vojta", I18N_NOOP("Author of xdvi"));
  about->addCredit ("Philipp Lehmann", I18N_NOOP("Testing and bug reporting."));
  about->addCredit ("Wilfried Huss", I18N_NOOP("Re-organisation of source code."));

  return about;
}

void KDVIMultiPage::slotEmbedPostScript(void)
{
  DVIRenderer.embedPostScript();
  emit askingToCheckActions();
}


void KDVIMultiPage::setEmbedPostScriptAction(void)
{
  if ((DVIRenderer.dviFile == 0) || (DVIRenderer.dviFile->numberOfExternalPSFiles == 0))
    embedPSAction->setEnabled(false);
  else
    embedPSAction->setEnabled(true);
}


void KDVIMultiPage::slotSave()
{
  // Try to guess the proper ending...
  QString formats;
  QString ending;
  int rindex = m_file.findRev(".");
  if (rindex == -1) {
    ending = QString::null;
    formats = QString::null;
  } else {
    ending = m_file.mid(rindex); // e.g. ".dvi"
    formats = fileFormats().grep(ending).join("\n");
  }

  QString fileName = KFileDialog::getSaveFileName(QString::null, formats, 0, i18n("Save File As"));

  if (fileName.isEmpty())
    return;

  // Add the ending to the filename. I hope the user likes it that
  // way.
  if (!ending.isEmpty() && fileName.find(ending) == -1)
    fileName = fileName+ending;

  if (QFile(fileName).exists()) {
    int r = KMessageBox::warningYesNo (0, i18n("The file %1\nexists. Do you want to overwrite that file?").arg(fileName),
                       i18n("Overwrite File"));
    if (r == KMessageBox::No)
      return;
  }

  // TODO: error handling...
  if ((DVIRenderer.dviFile != 0) && (DVIRenderer.dviFile->dvi_Data() != 0))
    DVIRenderer.dviFile->saveAs(fileName);
  
  return;
}


void KDVIMultiPage::slotSave_defaultFilename()
{
  // TODO: error handling...
  if (DVIRenderer.dviFile != 0)
    DVIRenderer.dviFile->saveAs(m_file);
  return;
}


bool KDVIMultiPage::isModified()
{
  if ((DVIRenderer.dviFile == 0) || (DVIRenderer.dviFile->dvi_Data() == 0))
    return false;
  else
    return DVIRenderer.dviFile->isModified;
}


KDVIMultiPage::~KDVIMultiPage()
{
  if (timer_id != -1)
    killTimer(timer_id);
  timer_id = -1;
  writeSettings();
  Prefs::writeConfig();
  delete printer;
}


bool KDVIMultiPage::openFile()
{
  document_history.clear();
  emit setStatusBarText(i18n("Loading file %1").arg(m_file));

  bool r = DVIRenderer.setFile(m_file);
  setEmbedPostScriptAction();
  if (!r)
    emit setStatusBarText(QString::null);

  //############
  //An dieser Stelle m�sste der Zoom-Faktor neu berechnet werden?

  generateDocumentWidgets();
  emit numberOfPages(DVIRenderer.totalPages());
  enableActions(r);

  QString reference = url().ref();
  if (!reference.isEmpty())
    gotoPage(DVIRenderer.parseReference(reference));
  return r;
}


QStringList KDVIMultiPage::fileFormats()
{
  QStringList r;
  r << i18n("*.dvi *.DVI|TeX Device Independent Files (*.dvi)");
  return r;
}


void KDVIMultiPage::addConfigDialogs(KConfigDialog* configDialog)
{
  static optionDialogFontsWidget* fontConfigWidget = 0;
  
  fontConfigWidget = new optionDialogFontsWidget(scrollView());
  optionDialogSpecialWidget* specialConfigWidget = new optionDialogSpecialWidget(scrollView());
  
  configDialog->addPage(fontConfigWidget, Prefs::self(), i18n("TeX Fonts"), "fonts");
  configDialog->addPage(specialConfigWidget, Prefs::self(), i18n("DVI Specials"), "dvi");
  configDialog->setHelp("preferences", "kdvi");
  
  connect(configDialog, SIGNAL(settingsChanged()), this, SLOT(preferencesChanged()));
}


void KDVIMultiPage::preferencesChanged()
{
#ifdef  KDVI_MULTIPAGE_DEBUG
  kdDebug(4300) << "preferencesChanged" << endl;
#endif

  bool showPS = Prefs::showPS();
  bool useFontHints = Prefs::useFontHints();

  DVIRenderer.setPrefs( showPS, Prefs::editorCommand(), useFontHints);
}


bool KDVIMultiPage::print(const QStringList &pages, int current)
{
  // Make sure the KPrinter is available
  if (printer == 0) {
    printer = new KPrinter();
    if (printer == 0) {
      kdError(4300) << "Could not allocate printer structure" << endl;
      return false;
    }
  }
  
  // Feed the printer with useful defaults and information.
  printer->setPageSelection( KPrinter::ApplicationSide );
  printer->setCurrentPage( current+1 );
  printer->setMinMax( 1, DVIRenderer.totalPages() );
  printer->setFullPage( true );


  // If pages are marked, give a list of marked pages to the
  // printer. We try to be smart and optimize the list by using ranges
  // ("5-11") wherever possible. The user will be tankful for
  // that. Complicated? Yeah, but that's life.
  if (pages.isEmpty() == true)
    printer->setOption( "kde-range", "" );
  else {
    int commaflag = 0;
    QString range;
    QStringList::ConstIterator it = pages.begin();
    do{
      int val = (*it).toUInt()+1;
      if (commaflag == 1)
    range +=  QString(", ");
      else
    commaflag = 1;
      int endval = val;
      if (it != pages.end()) {
    QStringList::ConstIterator jt = it;
    jt++;
    do{
      int val2 = (*jt).toUInt()+1;
      if (val2 == endval+1)
        endval++;
      else
        break;
      jt++;
    } while( jt != pages.end() );
    it = jt;
      } else
    it++;
      if (endval == val)
    range +=  QString("%1").arg(val);
      else
    range +=  QString("%1-%2").arg(val).arg(endval);
    } while (it != pages.end() );
    printer->setOption( "kde-range", range );
  }

  // Show the printer options requestor
  if (!printer->setup(scrollView(), i18n("Print %1").arg(m_file.section('/', -1))))
    return false;
  // This funny method call is necessary for the KPrinter to return
  // proper results in printer->orientation() below. It seems that
  // KPrinter does some options parsing in that method.
  ((KDVIPrinterWrapper *)printer)->doPreparePrinting();
  if (printer->pageList().isEmpty()) {
    KMessageBox::error( scrollView(),
            i18n("The list of pages you selected was empty.\n"
                 "Maybe you made an error in selecting the pages, "
                 "e.g. by giving an invalid range like '7-2'.") );
    return false;
  }

  // Turn the results of the options requestor into a list arguments
  // which are used by dvips.
  QString dvips_options = QString::null;
  // Print in reverse order.
  if ( printer->pageOrder() == KPrinter::LastPageFirst )
    dvips_options += "-r ";
  // Print only odd pages.
  if ( printer->pageSet() == KPrinter::OddPages )
    dvips_options += "-A ";
  // Print only even pages.
  if ( printer->pageSet() == KPrinter::EvenPages )
    dvips_options += "-B ";
  // We use the printer->pageSize() method to find the printer page
  // size, and pass that information on to dvips. Unfortunately, dvips
  // does not understand all of these; what exactly dvips understands,
  // depends on its configuration files. Consequence: expect problems
  // with unusual paper sizes.
  switch( printer->pageSize() ) {
  case KPrinter::A4:
    dvips_options += "-t a4 ";
    break;
  case KPrinter::B5:
    dvips_options += "-t b5 ";
    break;
    case KPrinter::Letter:
      dvips_options += "-t letter ";
      break;
  case KPrinter::Legal:
    dvips_options += "-t legal ";
    break;
    case KPrinter::Executive:
      dvips_options += "-t executive ";
      break;
  case KPrinter::A0:
    dvips_options += "-t a0 ";
    break;
  case KPrinter::A1:
    dvips_options += "-t a1 ";
    break;
  case KPrinter::A2:
    dvips_options += "-t a2 ";
    break;
  case KPrinter::A3:
    dvips_options += "-t a3 ";
    break;
  case KPrinter::A5:
    dvips_options += "-t a5 ";
    break;
  case KPrinter::A6:
    dvips_options += "-t a6 ";
    break;
  case KPrinter::A7:
    dvips_options += "-t a7 ";
    break;
  case KPrinter::A8:
    dvips_options += "-t a8 ";
      break;
  case KPrinter::A9:
    dvips_options += "-t a9 ";
    break;
  case KPrinter::B0:
    dvips_options += "-t b0 ";
    break;
  case KPrinter::B1:
    dvips_options += "-t b1 ";
    break;
  case KPrinter::B10:
    dvips_options += "-t b10 ";
    break;
  case KPrinter::B2:
    dvips_options += "-t b2 ";
    break;
  case KPrinter::B3:
    dvips_options += "-t b3 ";
    break;
  case KPrinter::B4:
    dvips_options += "-t b4 ";
    break;
  case KPrinter::B6:
    dvips_options += "-t b6 ";
    break;
  case KPrinter::B7:
    dvips_options += "-t b7 ";
    break;
  case KPrinter::B8:
    dvips_options += "-t b8 ";
    break;
  case KPrinter::B9:
    dvips_options += "-t b9 ";
    break;
  case KPrinter::C5E:
    dvips_options += "-t c5e ";
    break;
  case KPrinter::Comm10E:
    dvips_options += "-t comm10e ";
    break;
  case KPrinter::DLE:
    dvips_options += "-t dle ";
    break;
  case KPrinter::Folio:
    dvips_options += "-t folio ";
    break;
  case KPrinter::Ledger:
    dvips_options += "-t ledger ";
    break;
  case KPrinter::Tabloid:
    dvips_options += "-t tabloid ";
    break;
  }
  // Orientation
  if ( printer->orientation() == KPrinter::Landscape )
    dvips_options += "-t landscape ";


  // List of pages to print.
  QValueList<int> pageList = printer->pageList();
  dvips_options += "-pp ";
  int commaflag = 0;
  for( QValueList<int>::ConstIterator it = pageList.begin(); it != pageList.end(); ++it ) {
    if (commaflag == 1)
      dvips_options +=  QString(",");
    else
      commaflag = 1;
    dvips_options += QString("%1").arg(*it);
  }

  // Now print. For that, export the DVI-File to PostScript. Note that
  // dvips will run concurrently to keep the GUI responsive, keep log
  // of dvips and allow abort. Giving a non-zero printer argument
  // means that the dvi-widget will print the file when dvips
  // terminates, and then delete the output file.
  KTempFile tf;
  DVIRenderer.exportPS(tf.name(), dvips_options, printer);

  // "True" may be a bit euphemistic. However, since dvips runs
  // concurrently, there is no way of telling the result of the
  // printing command at this stage.
  return true;
}


// Explanation of the timerEvent.
//
// This is a dreadful hack. The problem we adress with this timer
// event is the following: the kviewshell has a KDirWatch object which
// looks at the DVI file and calls reload() when the object has
// changed. That works very nicely in principle, but in practise, when
// TeX runs for several seconds over a complicated file, this does not
// work at all. First, most of the time, while TeX is still writing,
// the file is invalid. Thus, reload() is very often called when the
// DVI file is bad. We solve this problem by checking the file
// first. If the file is bad, we do not reload. Second, when the file
// finally becomes good, it very often happens that KDirWatch does not
// notify us anymore. Whether this is a bug or a side effect of a
// feature of KDirWatch, I dare not say. We remedy that problem by
// using a timer: when reload() was called on a bad file, we
// automatically come back (via the timerEvent() function) every
// second and check if the file became good. If so, we stop the
// timer. It may well happen that KDirWatch calls us several times
// while we are waiting for the file to become good, but that does not
// do any harm.
//
// -- Stefan Kebekus.

void KDVIMultiPage::timerEvent( QTimerEvent * )
{
#ifdef KDVI_MULTIPAGE_DEBUG
  kdDebug(4300) << "Timer Event " << endl;
#endif
  reload();
}


void KDVIMultiPage::reload()
{
#ifdef KDVI_MULTIPAGE_DEBUG
  kdDebug(4300) << "Reload file " << m_file << endl;
#endif
  
  if (DVIRenderer.isValidFile(m_file)) {
    userSelection.clear();
    Q_INT32 pg = currentPageNumber();

    killTimer(timer_id);
    timer_id = -1;
    bool r = DVIRenderer.setFile(m_file);
    
    generateDocumentWidgets();
    emit numberOfPages(DVIRenderer.totalPages());
    enableActions(r);
    emit setStatusBarText(QString::null);
    markList()->setCurrentPageNumber(pg);
    emit pageInfo(DVIRenderer.totalPages(), pg );
  } else {
    if (timer_id == -1)
      timer_id = startTimer(1000);
  }
}


void KDVIMultiPage::enableActions(bool b)
{
  KMultiPage::enableActions(b);

  docInfoAction->setEnabled(b);
  exportPSAction->setEnabled(b);
  exportPDFAction->setEnabled(b);
  exportTextAction->setEnabled(b);

  setEmbedPostScriptAction();
}


void KDVIMultiPage::doEnableWarnings(void)
{
  KMessageBox::information (scrollView(), i18n("All messages and warnings will now be shown."));
  KMessageBox::enableAllMessages();
  KTipDialog::setShowOnStart(true);
}


void KDVIMultiPage::showTip(void)
{
  KTipDialog::showTip(scrollView(), "kdvi/tips", true);
}


void KDVIMultiPage::showTipOnStart(void)
{
  KTipDialog::showTip(scrollView(), "kdvi/tips");
}


documentWidget* KDVIMultiPage::createDocumentWidget()
{
  QSize rendererSuggestedSize = pageCache.sizeOfPageInPixel(/*page+*/1); // FIXME
  QSize widgetSize;
  if (rendererSuggestedSize.isEmpty()) {
    widgetSize.setWidth(100);
    widgetSize.setHeight(100);
  } else
    widgetSize = rendererSuggestedSize;


  // TODO: handle different sizes per page.
  DVIWidget* documentWidget = new DVIWidget(scrollView()->viewport(), scrollView(), widgetSize, &pageCache,
                              &userSelection, "singlePageWidget" );

  // Handle source links
  connect(documentWidget, SIGNAL(SRCLink(const QString&,QMouseEvent *, documentWidget *)), getRenderer(),
          SLOT(handleSRCLink(const QString &,QMouseEvent *, documentWidget *)));

  return documentWidget;
}

#include "kdvi_multipage.moc"
