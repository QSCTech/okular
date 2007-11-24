/***************************************************************************
 *   Copyright (C) 2007 by Tobias Koenig <tokoe@kde.org>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "generator_fb.h"

#include "converter.h"

#include <kaboutdata.h>

OKULAR_EXPORT_PLUGIN(FictionBookGenerator)

FictionBookGenerator::FictionBookGenerator()
    : Okular::TextDocumentGenerator( new FictionBook::Converter )
{
    // ### TODO fill after the KDE 4.0 unfreeze
    KAboutData *about = new KAboutData(
         "okular_fictionbook",
         "okular_fictionbook",
         KLocalizedString(),
         "0.1",
         KLocalizedString(),
         KAboutData::License_GPL,
         KLocalizedString()
    );
    setAboutData( about );
}