
#ifndef _FONTPOOL_H
#define _FONTPOOL_H

#include <qlist.h>
#include <qstringlist.h>

#include "font.h"


#define NumberOfMFModes 3
#define DefaultMFMode 1

extern const char *MFModes[];
extern const char *MFModenames[];
extern int         MFResolutions[];


/**
 *  A list of fonts and a compilation of utility functions
 *
 * This class holds a list of fonts and is able to perform a number of
 * functions on each of the fonts. The main use of this class is that
 * it is able to control a concurrently running "kpsewhich" programm
 * which is used to locate and load the fonts.
 *
 * @author Stefan Kebekus   <kebekus@kde.org>
 *
 *
 **/

class fontPool : public QList<struct font> {
 public:

  /** Method used to set the MetafontMode for the PK font files. This
   * data is used when loading fonts. Currently, a change here will be
   * applied only to those font which were not yet loaded ---expect
   * funny results when changing the data in the mid-work. The integer
   * argument must be smaller than NumberOfMFModes, which is defined
   * in fontpool.h and refers to a Mode/Resolutin pair in the lists
   * MFModes and MFResolutins which are also defined in fontpool.h and
   * implemented in fontpool.cpp. Returns the mode number of the mode
   * which was actually set ---if an invalid argument is given, this
   * will be the DefaultMFMode as defined in fontPool.h */
  unsigned int setMetafontMode( unsigned int );

  /** Says whether fonts will be generated by running MetaFont, or a
      similar programm. If (flag == 0), fonts will not be generated,
      otherwise they will. */
  void setMakePK( int flag );
  
  /** This method adds a font to the list. If the font is not
   * currently loaded, it's file will be located and font::load_font
   * will be called. Since this is done using a concurrently running
   * process, there is no guarantee that the loading is already
   * performed when the method returns.  */
  void          appendx(const font *fontp);

  /** Prints very basic debugging information about the fonts in the
   * pool to the kdDebug output stream. */
  void          status(); 

 private:
  int          makepk;
  unsigned int MetafontMode;
};

#endif //ifndef _FONTPOOL_H
