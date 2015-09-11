/*
 * h264video.h
 *
 *  DESCRIPTION:
 *   This is the class for implementing h264 conversions for RTP
 *   -----------------------------------------------------------------------
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
 *  -----------------------------------------------------------------------
 */

#ifndef AVFORMAT_H
#define AVFORMAT_H

#include <QtGlobal>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QTime>

#include <string.h>
#include "channelformat.h"

class AvFormat : QObject
{
    Q_OBJECT
public:
    AvFormat(ChannelFormat * ch0, ChannelFormat * ch1);
    ~AvFormat();
    void setImageSize(int w, int h);
    void setChannelID(QString id, STREAMS s );
    void stop() { stopstreaming = true; }
    const QByteArray *frame() { \
    	if(channel==0 && avchannel0) return avchannel0->frame(); \
    	if(channel==1 && avchannel1) return avchannel1->frame(); \
    	return NULL;}
    void switchChannel();
    // to be re-implemented depending on the av format
    virtual bool writeFinal();
    virtual int recordFrame(const unsigned char *frame, int size, STREAMS stream );

public slots:
    void writeChannel();

private:
    QString channelid;
    STREAMS streams;
    QDateTime datetime;
    QDateTime prevdatetime;
    bool stopstreaming;
    bool writeon;

    volatile int channel;
    ChannelFormat *avchannel0;
    ChannelFormat *avchannel1;
};

#endif // AVFORMAT_H
