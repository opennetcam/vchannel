/*
 * main.cpp
 *
 *  DESCRIPTION:
 *   This is the main program
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
#include <QtGui>
#include <QString>
#ifndef _WIN32
#include <syslog.h>
#endif

#include "../include/common.h"
#include "vchannel.h"


namespace command_line_arguments {

#ifdef _WIN32
 	 QString directory  = "c:\\";
#else
	QString directory   = "/tmp/";
#endif

	QString qsname      = "vchannel";
	QString qscname      = qsname;
	QString qshardware;
	QString qsurl       = "";
	QString qseventaddress;
	QString qsauth;
	QString record_settings = RECORD_SETTINGS_ALL;

	int     px = 0;                   // window position
	int     py = 0;                   // window position
	int     pw = 0;                   // window size
	int     ph = 0;                   // window size

	int     ndevice     = -1;
	int     naudio      = 0;          // audio (default off)
	int     nborder     = 1;          // border (default on)
	int     nevents     = 0;          // events (default off)
	int     nlock       = 0;          // lock window (default off)
	bool    usetcp = false;           // use RTP/TCP rather than RTP/UDP
	int     threshold   = 50;         // threshold for motion (0-100)
	int     sensitivity = 50;         // number of pixels to change for motion (0-100 % of total)
	int     mx = 0;                   // motion window
	int     my = 0;                   // motion window
	int     mw = 100;                 // motion window
	int     mh = 100;                 // motion window
}

int 	debugsetting = 0;

#define PROGRAM_NAME "vchannel"

using namespace command_line_arguments;

QStatusBar *statusbar = NULL;
VChannel   *vchannel  = NULL;

#ifdef _WIN32
QString dirlog = QDir::homePath()+QString("\\logs\\");
#else
QString dirlog = QDir::homePath()+QString("/logs/");
#endif

#ifndef _WIN32
// Linux systems use syslog
void SyslogOutput( QtMsgType type, const char *msg )
{
    if (msg==NULL )
        return;
    switch( type )
    {
    case QtDebugMsg:    if( debugsetting > 0 ) syslog(LOG_DEBUG, "%03d:%s", ndevice,msg ); break;
    case QtWarningMsg:  syslog(LOG_WARNING, "%03d:%s", ndevice,msg );break;
    case QtCriticalMsg: syslog(LOG_ERR, "%03d:%s", ndevice,msg );break;
    case QtFatalMsg:    syslog(LOG_CRIT, "%03d:%s", ndevice,msg ); abort(); break;
    }
}
#endif

// log to file
void DebugOutput( QtMsgType type, const char *msg )
{
    if (msg==NULL )
        return;

    QFile fout(dirlog+QString(PROGRAM_NAME "%1_%2.log").arg(ndevice).arg(QDate::currentDate().toString("yyMMdd")));
    // do not log unless the debug setting is > 0
    if ( debugsetting==0 && type == QtDebugMsg )
        return;

    if ( fout.open( QIODevice::WriteOnly | QIODevice::Append ) )
    {
        QTextStream ts( &fout );
        ts << QTime::currentTime().toString() << ':' << msg << "\n";
        if ( type == QtFatalMsg )
            ts << QTime::currentTime().toString() << ':' << QString("application terminated") << "\n";
        fout.close();
    }
    if ( type == QtFatalMsg )
        abort();
}

int main(int argc, char *argv[])
{
	// read the command line arguments
    if( argc > 1 )
    for( int ii=1; ii < argc; ii++ ){
        QString arg = argv[ii];
        arg = arg.toLower();
        if( arg == "--name" || arg == "-n"  )
            qsname = argv[++ii];
        else
        if( arg == "--cname" || arg == "-c"  )
            qscname = argv[++ii];
        else
        if( arg == "--url" || arg == "-u"  )
            qsurl = argv[++ii];
        else
        if( arg == "--tcp" || arg == "-t"  )
        {
            usetcp = true;
            QString s = argv[ii+1];
            if( !s.isEmpty() && !s.startsWith('-') ) qsurl = argv[++ii];
        }
        else
        if( arg == "--audio" || arg == "-a"  )
            naudio = QString(argv[++ii]).toInt();
        else
        if( arg == "--output" || arg == "-o"  ) {
            directory = QString(argv[++ii]).replace("&nbsp;"," ");           // handle spaces in the path
            // ensure it ends with a '/'
            if( !directory.isEmpty())
            {
#ifdef _WIN32
                directory.replace('/','\\');
                if( !directory.endsWith('\\') ) directory += "\\";
#else
                if( !directory.endsWith('/') ) directory += "/";
#endif
            }
        } else
        if( arg == "--device" || arg == "-d"  )
            ndevice = QString(argv[++ii]).toInt();
        else
        if( arg == "--hardware" || arg == "-w"  )
            qshardware = argv[++ii];
        else
		if( arg == "--noborder" || arg == "-b"  ) {
			nborder = 0;
			nlock = 1;
		} else
        if( arg == "--record" || arg == "-r"  )
            record_settings = argv[++ii];
        else
        if( arg == "--events" || arg == "-e"  )
        {
            qseventaddress = QString(argv[++ii]).trimmed();
            nevents = 1;
        }
        else
        if( arg == "--basic" || arg == "-s"  )
        {
            qsauth = QString(argv[++ii]).trimmed();
        }
        else
        if( arg == "--position" || arg == "-p"  )
        {
            QString position = argv[++ii];
            // separate into comma separated integers
            QStringList qslposition = position.split('-');

            if( qslposition.count()>1 )
            {
            	px = qslposition.at(0).toInt();
                py = qslposition.at(1).toInt();
                pw = qslposition.at(2).toInt();
                ph = qslposition.at(3).toInt();
            }
        }
        else
        if( arg == "--motion" || arg == "-m"  )
        {
            QString motion = argv[++ii];
            // separate into comma separated integers
            QStringList qslmotion = motion.split('-');

            if( qslmotion.count()>0 )
            {
                sensitivity = qslmotion.at(0).toInt();
                if( qslmotion.count()>1 )
                {
                    threshold = qslmotion.at(1).toInt();
                    if( qslmotion.count()>5 )
                    {
                        mx = qslmotion.at(2).toInt();
                        my = qslmotion.at(3).toInt();
                        mw = qslmotion.at(4).toInt()-mx;
                        mh = qslmotion.at(5).toInt()-my;
                    }
                }
            }
        }
        else
        if( arg == "--version" || arg == "-v"  )
        {
            printf("vchannel version %s:%s", STR_VERSION, __DATE__ );
#ifdef QT_NO_DEBUG
            printf("\n");
#else
            printf(" (DEBUG) \n");
#endif
                exit(0);
        }
        else
        if( arg == "--help" || arg == "-h"  )
        {
            printf("syntax: vchannel <options>\n");
            printf("        --noborder,-b         : omit window border, fix window position and size\n");
            printf("        --position,-p <x>-<y>-<w>-<h>         : window position\n");
            printf("        --device,-d   <num>                   : unique number of this device (reqd.)\n");
            printf("        --name,-n     <name>                  : unique camera name (reqd.)\n");
            printf("        --cname,-c    <name>                  : common name\n");
            printf("        --url,-u      <url>                   : url to stream using rtsp & rtp/udp (default)\n");
            printf("        --tcp,-t      <url>                   : url to stream using rtsp & rtp/tcp\n");
            printf("        --audio,-a    <mode>                  : audio mode (MJPEG only)\n");
            printf("        --output,-o   <dir>                   : output directory\n");
            printf("        --hardware,-w <hw>                    : hardware designation\n");
            printf("        --record,-r   <i>-<m>-<d>-<h:m>-<h:m> : record schedule\n");
            printf("        --events,-e   <ipaddr>                : send event messages to server\n");
            printf("        --motion,-m   <s>-<t>-<x>-<y>-<w>-<h> : motion detection/window settings\n");
            printf("        --basic,-s    <auth>                  : basic security authorization\n");
            printf("        --version,-v                          : version display\n");
            printf("        --help,-h                             : this summary\n");
            exit (0);
        }
    }

    QDir dir(dirlog);
#ifndef _WIN32
    if( !dir.exists() )
    {
        openlog (PROGRAM_NAME, LOG_CONS, LOG_USER);
        qInstallMsgHandler( SyslogOutput );
    }
#else
    if( !dir.exists() && !dir.mkdir(dirlog) )
            qWarning() << QString("Unable to create log directory: %1").arg(dirlog);
#endif
#ifdef QT_NO_DEBUG
    else
    {
        QFile fout(dirlog+QString(PROGRAM_NAME "%1_%2.log").arg(ndevice).arg(QDate::currentDate().toString("yyMMdd")));
        if ( fout.open( QIODevice::WriteOnly | QIODevice::Append ) )
        {
            fout.close();
            qInstallMsgHandler( DebugOutput );
        } else
			qWarning() << QString("Unable to create log file: %1").arg(fout.fileName());
    }
#endif
    QApplication a(argc, argv);
    // register program startup
    qWarning() << (const char*)QDateTime::currentDateTime().toString().toAscii() << ": VChannel v[" __DATE__ "]" << ndevice <<":" << qshardware << qscname << "startup" ;
    qWarning() <<  "name:" << (const char*)qsname.toAscii();

    QApplication::setWindowIcon(QIcon(":/images/cam.jpg"));
    VChannel w;
    vchannel = &w;

    // put debug output here so that it takes advantage of VChannel initialization
    QDEBUG <<  "cname:" << qscname;
    QDEBUG <<  "url:" << qsurl;
    QDEBUG <<  "device:" << ndevice;
    QDEBUG <<  "audio:" << naudio;
    QDEBUG <<  "hardware:" << qshardware;
    QDEBUG <<  "output directory:" << directory;
    QDEBUG <<  "lock:" << nlock;

    if( qsname.isEmpty() || ndevice == -1 ) {
    	printf("vchannel: parameters missing - enter 'vchannel --help' for details\n");
    	exit(0);
    }
    if( qsurl.isEmpty() ) {
    	printf("vchannel: URL not set - enter 'vchannel --help' for details\n");
    	exit(0);
    }
    w.show();
    int ex = a.exec();
    qWarning() << "program exit" << ex;
    return ex;
}
