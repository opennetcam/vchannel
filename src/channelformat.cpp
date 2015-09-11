/**
 * channelformat.cpp
 *
 * DESCRIPTION:
 * This is the base class for compression-dependent AV formats.
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

#include <QtDebug>
#include <QDir>
#include <QStatusBar>
#include <QTimer>
#include <QLabel>

#include "avformat.h"
#include "recordschedule.h"
#include "vchannel.h"

extern QString directory;
extern bool usetcp;

extern QStatusBar *statusbar;
extern VChannel   *vchannel;

extern RecordSchedule recordschedule;

/*
 *  ChannelFormat  class
 *  This class is format-dependant
 */


ChannelFormat::ChannelFormat( QString ext ):
        samples(0), frames(0),
        width(0), height(0)
{
    QDEBUG << __FUNCTION__;
    framecount = 100;
    elapsed = 0;
    // save the file extension
    fileextension = ext;
}

ChannelFormat::~ChannelFormat()
{
    QDEBUG << __FUNCTION__;
}

// to be implemented to capture each frame
int ChannelFormat::recordFrame(const unsigned char * /*frame*/, int /*size*/, STREAMS /*stream*/, bool /* writeon*/ )
{
    int ret = 1;

    Q_ASSERT_X(0,__FUNCTION__, "is not implemented");

    return ret;
}

// TODO
// this must be implemented to write the assigned format
bool ChannelFormat::writeAv(QString filename, STREAMS streams, QDateTime &, qint64  )
{
    QDEBUG << __FUNCTION__ << filename;
    QDEBUG << "stream: " << streams;

    Q_ASSERT_X(0,__FUNCTION__, "is not implemented");

    return false;
}


// this is used to clean up the buffer frames
//
void ChannelFormat::deleteFrames()
{
    int buffers = dataList.count();

    QDEBUG << "delete all frames:" << buffers;

    for(uint ii=0; ii<(uint)buffers; ii++)
    {
        QByteArray *frame = dataList.at(ii);
        // clean up the buffer
        if( frame ) delete frame;
    }

    dataList.clear();

    // reset the state
    frames = 0;
    samples = 0;
    framecount = 100;
    elapsed = 0;

}

const QByteArray *ChannelFormat::frame()
{
	if( dataList.isEmpty() )
		return NULL;
	return dataList.last();
}
