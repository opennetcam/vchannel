/**
 *
 * FILE:		aviformat.h
 *
 * DESCRIPTION:
 * This is the class for writing and reading AVI format.
 * -----------------------------------------------------------------------
 *    Copyright (C) 2010-2015 OpenNetcam Project.
 *
 *   This  software is released under the following license:
 *        - GNU General Public License (GPL) version 3 for use with the
 *          Qt Open Source Edition (http://www.qt.io)
 *
 *    Permission to use, copy, modify, and distribute this software and its
 *    documentation for any purpose and without fee is hereby granted
 *    in accordance with the provisions of the GPLv3 which is available at:
 *    http://www.gnu.org/licenses/gpl.html
 *
 *    This software is provided "as is" without express or implied warranty.
 *
 * -----------------------------------------------------------------------
 */

/* AVI format description:
 *
 * THIS DOCUMENT IS IN THE PUBLIC DOMAIN, YOU ARE FREE TO COPY AND MODIFY IT AS YOU SEE FIT
 *
 * BETA DOCUMENTATION OF RIFF-AVI FILE FORMAT -- DO NOT TRUST FOR ACCURACY -- DOESN'T COVER OPENDML EXTENTIONS
 * YOU HAVE BEEN WARNED
 *
 * Tree view of RIFF data chunks(ie. map of subchunks).  LIST chunk will be added in next revision(the documentation I based this on didn't cover LIST chunks, oddly enough):
 *
 * RIFF				RIFF HEADER
 * |-AVI 				AVI CHUNK
 *   |-hdrl			MAIN AVI HEADER
 *   | |-avih			AVI HEADER
 *   | |-strl			STREAM LIST[One per stream]
 *   | | |-strh			STREAM HEADER[Requiered after above]
 *   | | |-strf			STREAM FORAMT
 *   | | |-strd			OPTIONAL -- STREAM DATA
 *   | | |-strn			OPTIONAL -- STREAM NAME
 *   |-movi			MOVIE DATA
 *   | |-rec 			RECORD DATA[SEE BELOW]
 *   |   |-[data subchunks]	RAW DATA[SEE BELOW]
 *   |-idx1			AVI INDEX
 *     |-[index data]		DATA
 *
 */


#ifndef AVIFORMAT_H
#define AVIFORMAT_H

#include <QtGlobal>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QTime>

#include <string.h>
#include "avformat.h"

class AviChannelFormat : ChannelFormat
{
public:
    AviChannelFormat();
    ~AviChannelFormat();
    bool writeAv(QString filename, STREAMS streams, QDateTime &, qint64 );
    int recordFrame(const unsigned char *frame, int size, STREAMS stream,bool writeon );
    void deleteFrames();

private:
    quint32 maxRiffSize;
    quint32 jpgSize;

    QList<quint32> videoListMarker;
    QList<quint32> audioListMarker;
};

#endif // AVIFORMAT_H
