/**
 * FILE:		sessiondescription.cpp
 * DESCRIPTION:
 * This is the class for implementing the Session Description Protocol.
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

#include <QDebug>
#include <QObject>
#include "../include/common.h"
#include "sessiondescription.h"

// The session classes implement the SDP interpretation
//
// SessionMedia class
//
SessionMedia::SessionMedia()
{
    QDEBUG << "SessionMedia";
}

void SessionMedia::setMedia(QByteArray & m)
{
    QDEBUG << "setMedia" << m;
    QList<QByteArray> list = m.split(' ');
    switch(list.count())
    {
    case 4:
        format = list.at(3).toInt();
    case 3:
        qsProtocol = list.at(2);
    case 2:
        port = list.at(1).toInt();
    case 1:
        media = list.at(0);
        break;
    }
    int port = 2*devorder;
    // choose the client ports
    if( media.contains("video"))
    {
        port += 61014;
        qmTransport.insert("client_port",QString("%1-%2").arg(port).arg(port+1));
    } else
    if( media.contains("audio"))
    {
        port += 51990;
        qmTransport.insert("client_port",QString("%1-%2").arg(port).arg(port+1));
    }
    QDEBUG << "format=" << format << " protocol=" << qsProtocol << " port=" << port << " media=" << media ;
}

void SessionMedia::setAttribute(QByteArray a)
{
    QDEBUG << "setAttribute" << a;
    if( a.startsWith("rtpmap:"))
    {
        QList<QByteArray> list = a.mid(strlen("rtpmap:")).split(' ');
        if( list.count()>1 )
            rtpmap.insert(list.at(0).toInt(), list.at(1) );
        QDEBUG << "rtpmap=" << rtpmap.begin().key() << "->" << rtpmap.begin().value();
    } else
    if( a.startsWith("control:"))
    {
        qsControl = a.mid(strlen("control:"));
        QDEBUG << "control=" << qsControl;
    } else
    if( a.startsWith("fmtp:") )
    {
    	// separate the fmtp:xx packetization...
		int pos = a.indexOf(' ');
		if( pos > 0 )
		{
			QByteArray list = a.mid(pos+1).trimmed();
			if( list.count()>1 )
			{
				QList<QByteArray> list1 = list.split(';');
				for( int ii=0; ii < list1.size();ii++ )
				{
					if( list1.at(ii) != "" )
					{
						int pos = list1.at(ii).indexOf('=');
						if( pos > 0 )
							qmFmtp.insert( list1.at(ii).left(pos).trimmed(), list1.at(ii).mid(pos+1).trimmed() );
					}
				}
			}
        }
        foreach( QString key, qmFmtp.keys() )
        {
        	QDEBUG << key << "->" << qmFmtp.value(key) << endl;
        }
    }
}

void SessionMedia::setTransport(QByteArray t)
{
    QDEBUG << "setTransport " << t;
    QList<QByteArray> list = t.split(';');
    for( int ii=0; ii<list.count();ii++ )
    {
        QList<QByteArray> pair = list.at(ii).split('=');
        if( pair.count()>1 )
            qmTransport.insert(pair.at(0), pair.at(1) );
        else
            qmTransport.insert(list.at(ii), NULL );
        QDEBUG <<  pair.at(0) << "->" << transport(pair.at(0) ) ;
    }
}

void SessionMedia::clear()
{
    media.clear();
    port=0;
    qsProtocol.clear();
    format=0;
    qsControl.clear();
    rtpmap.clear();
    qmTransport.clear();
}

// SessionDescription class
//
SessionDescription::SessionDescription():version(-1)
{
    QDEBUG << "SessionDescription";
}

void SessionDescription::Interpret(QList<QByteArray> &  qbl, int devorder)
{
    QDEBUG << "SessionDescription::Interpret";

    for( int ii=0; ii<qbl.count(); ii++)
    {
        QByteArray qba = qbl.at(ii).trimmed();
        if( qba.startsWith("v=") )
        {
            version = qba.mid(2).toInt();
            QDEBUG << "version=" << version;
        } else
        if( qba.startsWith("o=") )
        {
            originator = qba.mid(2).trimmed();
            QDEBUG << "originator=" << originator;
        } else
        if( qba.startsWith("c=") )
        {
            connection = qba.mid(2).trimmed();
            QDEBUG << "connection=" << connection;
        } else
        if( qba.startsWith("s=") )                     // session
        {
            QByteArray media = qba.mid(2).trimmed();
            if( media.startsWith("NVT"))
            {
                QDEBUG << "session=" << media;
                // set up session media
                videoMedia.setDevOrder(devorder);
                videoMedia.setMedia(media);

                // look for attributes
                for(; ii < qbl.count(); ii++ )
                {
                    qba = qbl.at(ii+1).trimmed();
                    if( qba.startsWith("a=") )
                    {
                        sessionMedia.setAttribute(qba.mid(2).trimmed());
                        QDEBUG << "set" << qba;
                    } else
                    if( qba.startsWith("c=") )
                    {
                        QDEBUG << "ignore" << qba;
                    } else
                    if( qba.startsWith("i=") )
                    {
                        QDEBUG << "ignore" << qba;
                    } else
                    if( qba.startsWith("t=") )
                    {
                        QDEBUG << "ignore" << qba;
                    } else
                        break;
                }
            }
        }
        if( qba.startsWith("m=") )                     // media
        {
            QByteArray media = qba.mid(2).trimmed();
            if( media.startsWith("video"))
            {
                QDEBUG << "video=" <<media;
                // set up video media
                videoMedia.setDevOrder(devorder);
                videoMedia.setMedia(media);

                // look for attributes
                for(; ii < qbl.count(); ii++ )
                {
                    qba = qbl.at(ii+1).trimmed();
                    if( qba.startsWith("a=") )
                    {
                        videoMedia.setAttribute(qba.mid(2).trimmed());
                        QDEBUG << "set" << qba.mid(2).trimmed();
                    } else
                    if( qba.startsWith("b=") )
                    {
                        QDEBUG << "ignore" << qba;
                    } else
                    if( qba.startsWith("c=") )
                    {
                        QDEBUG << "ignore" << qba;
                    } else
                        break;
                }
            } else
            if( media.startsWith("audio"))
            {
                QDEBUG << "audio=" <<media;
                // set up audio media
                audioMedia.setDevOrder(devorder);
                audioMedia.setMedia(media);

                // look for attributes
                for(; ii < qbl.count(); ii++ )
                {
                    qba = qbl.at(ii+1).trimmed();
                    if( qba.startsWith("a=") )
                    {
                        audioMedia.setAttribute(qba.mid(2).trimmed());
                    } else
                    if( qba.startsWith("c=") )
                    {
                        QDEBUG << "ignore" << qba;
                    } else
                        break;
                }
            }
        }
    }
}

// reset all the values
void SessionDescription::clear()
{
    version=-1;
    connection.clear();
    originator.clear();
    name.clear();

    sessionMedia.clear();
    videoMedia.clear();
    audioMedia.clear();
}
