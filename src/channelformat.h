/**
 * channelformat.h
 *
 * DESCRIPTION:
 * This is the base class for compression dependant AV formats.
 * On its own this class does very little.
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

#ifndef CHANNELFORMAT_H
#define CHANNELFORMAT_H

#include <QtGlobal>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QTime>

#include <string.h>

enum STREAMS { STREAMS_NONE=0, STREAMS_VIDEO, STREAMS_AV, STREAMS_AUDIO,  STREAMS_MAX };


class ChannelFormat
{
public:
    ChannelFormat(QString ext);
    virtual ~ChannelFormat();
    void setImageSize(int w, int h) { width = w; height = h;}
    virtual bool writeAv(QString filename, STREAMS streams, QDateTime & datetime, qint64 duration );
    virtual int recordFrame(const unsigned char *frame, int size, STREAMS stream,bool writeon );
    virtual void deleteFrames();
    const QByteArray *frame();
	long timelength() { return timer.elapsed(); }
	QString &fileExt() { return fileextension; }


protected:
	quint32 samples;     // audio samples
    int frames;
    int framecount;
    long elapsed;
    quint32 width;
    quint32 height;

    QTime timer;
    QString fileextension;

    QList<QByteArray *> dataList;

};

#endif // CHANNELFORMAT_H
