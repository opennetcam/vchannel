/**
 * avformat.cpp
 *
 * DESCRIPTION:
 * These are the base classes for caching and writing AV formats.
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

#include <QtDebug>
#include <QDir>
#include <QStatusBar>
#include <QTimer>
#include <QLabel>

#include "vchannel.h"
#include "avformat.h"
#include "recordschedule.h"


using namespace command_line_arguments;

extern QStatusBar *statusbar;
extern VChannel   *vchannel;

// provide a global status record
QString globalstatus;

extern RecordSchedule recordschedule;

/*
 *  AvFormat class
 */

AvFormat::AvFormat(ChannelFormat * ch0, ChannelFormat * ch1 ): streams(STREAMS_NONE), stopstreaming(false), writeon(false)
{
    QDir dir(directory);

    if( !dir.exists() )
        if( !dir.mkdir(directory) )
        {
            qWarning() << QString("Error: Unable to create record directory path %1").arg(directory);
        }

    // create 2 channels to allow writing to disk while recording
    avchannel0 = ch0;
    Q_ASSERT(avchannel0);
    avchannel1 = ch1;
    Q_ASSERT(avchannel1);
    channel = -1;
}

AvFormat::~AvFormat()
{
	// ensure that the channels are deleted
	if( avchannel0 ) delete avchannel0;
	avchannel0 = NULL;
	if( avchannel1 ) delete avchannel1;
	avchannel1 = NULL;
}

void AvFormat::setImageSize(int w, int h)
{
	Q_ASSERT(avchannel0);
	Q_ASSERT(avchannel1);
    avchannel0->setImageSize(w,h);
    avchannel1->setImageSize(w,h);
}

void AvFormat::setChannelID(QString id, STREAMS s )
{
    channelid = id;
    streams = s;
    datetime = QDateTime::currentDateTime();
    prevdatetime = QDateTime::currentDateTime();
    channel = 0;
    stopstreaming = false;
}

bool AvFormat::writeFinal()
{
    datetime = prevdatetime;
    prevdatetime = QDateTime();
    if( channel == 0 )
        channel = 1;
    else
    if( channel == 1 )
        channel = 0;
    else
        return false;

    writeChannel();

    // stop recording
    channel = -1;
    return true;
}

void AvFormat::switchChannel()
{
	QDEBUG << __FUNCTION__  << channel;
    datetime = prevdatetime;
    prevdatetime = QDateTime::currentDateTime();
    if( channel == 0 )
        channel = 1;
    else
    if( channel == 1 )
        channel = 0;
    else
        return;
    QTimer::singleShot(1, this, SLOT(writeChannel()));
}

void AvFormat::writeChannel()
{
    // ensure that datetime is valid
    if( !datetime.isValid() )
        datetime = QDateTime::currentDateTime();

    QString path = directory + datetime.date().toString(Qt::ISODate);
#ifdef _WIN32
        path.replace('/','\\');
        if( !path.endsWith('\\') ) path += "\\";
#else
        if( !path.endsWith('/') ) path += "/";
#endif
        QDEBUG << __FUNCTION__ <<  "writeChannel: path:" << path ;

    QDir dir(path);
    writeon = false;
    qint64 duration = datetime.msecsTo(QDateTime::currentDateTime());
    QString ftime = QString("%1").arg(datetime.toTime_t());
    QString flength = QString("%1").arg(duration/1000);
    QString ftype   = QString("%1").arg(recordschedule.eventType(datetime));

    QString filename= path + "AV.";
    filename += channelid + "." +
            ftime +"."+
            flength + "." +
            ftype + avchannel0->fileExt();

    if( !dir.exists() && !dir.mkpath(path) )
    {
        qWarning() << QString("Error: Unable to create directory path %1").arg(path);
    } else
    {
        // check whether the schedules allows writing
        // see whether the start of recording overlaps
        writeon = recordschedule.isScheduled(datetime);
    }

    if( writeon)
    {
        QDEBUG << __FUNCTION__ <<  "write file:" << filename;
        bool res = false;

        if( channel == 0 )
        {
            res = avchannel1->writeAv(filename,streams, datetime, duration );
        } else
        if( channel == 1 )
        {
            res = avchannel0->writeAv(filename,streams, datetime, duration  );
        }

        //notify the main program of a new file
        if( res )
        {
            if( vchannel )
            {
                QString qs = "<" STR_VIDEO " time='" + ftime + "' length='" +
                		flength + "' type='" + ftype + "' >";
                qs += filename;
                qs += "</" STR_VIDEO ">";
                vchannel->sendEventMessage(qs);
            }
        }
    } else
    {
        QDEBUG << __FUNCTION__ << " writing not within schedule";

        // delete the frames used in the other channel
        if( channel == 0 )
            avchannel1->deleteFrames();
        else
        if( channel == 1 )
            avchannel0->deleteFrames();
    }
}


int AvFormat::recordFrame(const unsigned char *frame, int size, STREAMS stream )
{
	int ret = 0;
    if( stopstreaming ) return 1;


    if( channel == 0 )
        ret = avchannel0 -> recordFrame(frame, size, stream, writeon);
    else
    if( channel == 1 )
        ret = avchannel1 -> recordFrame(frame, size, stream, writeon);
    else // we are done - stop recording
        return -1;

    // when the first file reaches 3 mins, switch to alternate channel and save the file
    if( ret == 0 )   // todo number
        switchChannel();
        //QTimer::singleShot(10, this, SLOT(switchChannel()));
    return ret;
}

