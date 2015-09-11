/*
 * vchannel.cpp
 *
 *  DESCRIPTION:
 *   This is the main class for the vchannel component
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
 *  -----------------------------------------------------------------------
 */

#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QPoint>
#include <QtNetwork>

#include "../include/common.h"
#include "recordschedule.h"
#include "vchannel.h"
#include "ui_vchannel.h"
#include "rtpsocket.h"


using namespace command_line_arguments;

extern VChannel   *vchannel;
extern QStatusBar *statusbar;
RecordSchedule recordschedule;
extern QString globalstatus;

//
// class myTcpSocket
//
myTcpSocket::myTcpSocket(QObject * parent ) : QTcpSocket(parent)
{
    // client timer allows the connection to be closed if the client dies
    clientTimer.setSingleShot(true);
    // allow 1 sec between requests to keep open
    clientTimer.setInterval(1000);
    connect(&clientTimer, SIGNAL(timeout()), this, SLOT(deleteLater()));
//    connect( &clientTimer,SIGNAL(timeout()), this, SLOT(disconnectFromHostImplementation()) );
//    connect( &clientTimer,SIGNAL(timeout()), parent, SLOT(disconnected()) );
}

myTcpSocket::~myTcpSocket()
{
    QDEBUG << "myTcpSocket deleted" << this;
}

//
// class myTcpServer
//
myTcpServer::myTcpServer(quint16 port, QObject * parent ) : QTcpServer(parent)
{
    listen(QHostAddress::Any, port);
}

void myTcpServer::incomingConnection(int socket)
{
    myTcpSocket* s = new myTcpSocket(this);
    connect(s, SIGNAL(readyRead()), vchannel, SLOT(readIncomingCommand()));
    connect(s, SIGNAL(disconnected()), vchannel, SLOT(disconnected()));
    connect(s, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(error(QAbstractSocket::SocketError)) );

    s->setSocketDescriptor(socket);
    QDEBUG << "vchannel: incomingConnection on:" << socket;
    s->keepalive();
}

void myTcpServer::error ( QAbstractSocket::SocketError socketError )
{
    qWarning() << "vchannel: TCP Server error: " << socketError;
}

//
// class VChannel
//

VChannel::VChannel(QWidget *parent) :
        QMainWindow(parent, (nborder==0 ?  (Qt::CustomizeWindowHint | Qt::MSWindowsFixedSizeDialogHint): Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint) ),
    ui(new Ui::VChannel), pbaudio(NULL), rtspsocket(NULL), tcpsocket(NULL), newsocket(NULL),
    tcpServer(NULL),
    restoreHeight(0),devicestate(DEVICE_IDLE),closecounter(0)
{
    // make sure it deletes on close
    setAttribute(Qt::WA_DeleteOnClose);
    //setAttribute(Qt::WA_QuitOnClose);

    QCoreApplication::setOrganizationName(ORGANIZATION_SHORT);
    QCoreApplication::setOrganizationDomain(ORGANIZATION_DOMAIN_NAME);
    QCoreApplication::setApplicationName(QString("vchannel_")+ qsname);

    ui->setupUi(this);
    url = QUrl(qsurl);
    // load settings
    if( !qsname.isEmpty() )
    {
        QSettings settings;
#ifdef QT_NO_DEBUG
        debugsetting = settings.value(QString("Debug"), 0 ).toInt();
#else
        debugsetting = settings.value(QString("Debug"), 1 ).toInt();
#endif
        if( debugsetting )
            qWarning("Debug logging enabled");
        else
            qWarning("Set registry entry 'Debug' to 1 for more detail logs");


        settings.beginGroup(qsname);
#ifdef QUERY_PLAYING_STATE
        if( settings.value("state").toString() != "playing" ||
            QMessageBox::Ok == QMessageBox::question(NULL,
                      tr("New Video Display"), tr("%1 may already be playing\n Do you want to continue?").arg(qsname),
                      QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel ) )
        {
#endif
        	QSize sz(pw,ph);
        	// if both height and width are 0 (default),
        	// read the settings to get the previous values
            if( pw == 0 && ph == 0 )
            {
            	sz = settings.value("size", QSize(DEFAULT_WIDTH,DEFAULT_HEIGHT)).toSize();
            	// set restore first to get resize to work correctly
            	restoreHeight = settings.value("restore",sz.height()).toInt();
            }

            if( restoreHeight )
            {
                resize( sz.width(),0 );

            } else
            {
                resize(sz);
            }
            // if both x and y are 0 (default),
            // calculate the default position of the window
            // otherwise, use the positions as set in the settings
            QPoint point(px,py);
            if( py == 0 && py == 0 )
            {
            	px = 10+(ndevice/10)*DEFAULT_WIDTH;
            	py = DEFAULT_HEIGHT+(32*(ndevice%10));
            	point = settings.value("pos", QPoint( px, py )).toPoint();
            }
        	move(point);
            settings.setValue("state", "playing");

#ifdef QUERY_PLAYING_STATE
        } else
        {

            settings.endGroup();
            exit(0);
            return;
        }
#endif
        settings.endGroup();
    }

    if( !qscname.isEmpty() )
        setWindowTitle(qscname);

    statusbar = this->statusBar();
    // add icon button to statusbar
    if( !qshardware.isEmpty() )
    {
        QString qsimage = ":/images/cam4210.jpg";
        if( qshardware.contains("1201") || qshardware.contains("1001") ) qsimage = ":/images/cam1201.jpg";
        else if( qshardware.contains("5211") ) qsimage = ":/images/cam5211.jpg";
        else if( qshardware.contains("6231") ) qsimage = ":/images/cam6231.jpg";
        QLabel *iconlabel = new QLabel(qsname,this);
        QPixmap pixmap(qsimage);
        iconlabel->setPixmap(pixmap.scaled(16,16));
        statusbar->addPermanentWidget(iconlabel);
        iconlabel = new QLabel(qscname,this);
        iconlabel->setToolTip(qsname);
        statusbar->addPermanentWidget(iconlabel);
    }
    // add audio button
    if( naudio )
    {
        // add audio button to statusbar
        pbaudio = new QPushButton(this);
        pbaudio->setMaximumSize(16,16);
        pbaudio->setFlat(true);
        pbaudio->setToolTip(tr("Mute/unmute sound"));
        QPixmap pixmap = QPixmap(":/images/audoff.png").scaled(16,16);
        pbaudio->setIcon(QIcon(pixmap));
        connect(pbaudio, SIGNAL(clicked(bool)), this, SLOT(on_audio_mute()) );
        statusbar->addPermanentWidget(pbaudio);
    }

    // add close button to statusbar
    if( nborder == 0 )
    {
        QPixmap pixmap = QPixmap(":/images/close.png").scaled(16,16);
        QPushButton *pbclose = new QPushButton(QIcon(pixmap),"",this);
        pbclose->setFlat(true);
        pbclose->setMaximumSize(16,16);
        pbclose->setToolTip(tr("Stop recording and close this display window"));
        connect(pbclose, SIGNAL(clicked(bool)), this, SLOT(close()) );
        statusbar->addPermanentWidget(pbclose);
    }

    // add min/max button to statusbar
    if( nborder == 0 )
    {
        QPixmap pixmap = (restoreHeight==0) ?
                    QPixmap(":/images/minimize.png").scaled(16,16):
                    QPixmap(":/images/normal.png").scaled(16,16);
        pbmin = new QPushButton(QIcon(pixmap),"",this);
        pbmin->setFlat(true);
        pbmin->setMaximumSize(16,16);
        pbmin->setToolTip(tr("minimize/restore the display"));
        connect(pbmin, SIGNAL(clicked(bool)), this, SLOT(on_minimizerestore()) );
        statusbar->addPermanentWidget(pbmin);
    }

    // show/hide the resize grip
    statusBar()->setSizeGripEnabled(nlock==0);

    // fix the window in position
    if( nlock != 0 )
    {
    	if( restoreHeight != 0 )
    	{
			setMaximumSize(width(),0);
			setMinimumSize(width(),0);
    	} else
    	{
			setMaximumSize(width(),height());
			setMinimumSize(width(),height());
    	}
    }

    // set up event sender
    if( nevents )
    {
        tcpsocket = new QTcpSocket(this);
        Q_ASSERT(tcpsocket);
        if( tcpsocket )
        {
            connect(tcpsocket,SIGNAL(connected()),this, SLOT(eventConnected()) );
            connect(tcpsocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(eventError(QAbstractSocket::SocketError)));
        }
    }

    // set up a server for command messages
    tcpServer = new myTcpServer(EVENT_PORT+ndevice+1, this);
    Q_ASSERT(tcpServer);
    if( tcpServer && !tcpServer->isListening())
    {
        QTimer::singleShot(5000, this, SLOT(close()) );
        QMessageBox::critical(this, tr("VChannel"),
                                   tr("Unable to start the server: %1.")
                                   .arg(tcpServer->errorString()));
        close();
        return;
    }

    devicestate = DEVICE_STARTING;

    // start streaming
    if( !rtspsocket )
        rtspsocket = new RtspSocket(ui->label);
    Q_ASSERT(rtspsocket);
    if( rtspsocket )
    {
        if( ndevice == -1 )
            ndevice = QTime::currentTime().msec()%100;
        rtspsocket->setDevOrder(ndevice);
    }

    // set the record schedule times
    recordschedule.setSchedule(record_settings);

    // start streaming
    streamStartStop();
}

VChannel::~VChannel()
{
    qWarning() << "shutdown" << qsname;

    if( newsocket ) newsocket->deleteLater();
    newsocket = NULL;

    if( tcpsocket ) tcpsocket->deleteLater();
    tcpsocket = NULL;

    if( tcpServer ) tcpServer->deleteLater();
    tcpServer = NULL;

    delete ui;

    exit(0);
}

void VChannel::streamStartStop()
{
    // start streaming
    if( !rtspsocket ) return;

    if( url.isValid() && !rtspsocket->isPlaying())
    {
        QDEBUG << "vchannel: streamStartStop: stream" ;
        statusBar()->showMessage(tr("Streaming"));
        rtspsocket->stream(qsurl, qsname, (naudio!=0) );
    } else
    {
        QDEBUG << "vchannel: streamStartStop: stop" ;
        statusBar()->showMessage(tr("Stopped"));
        rtspsocket->stop();
    }
}

void VChannel::sendEventMessage(QString message )
{
    if( newsocket == NULL )
    {
        newsocket = new QTcpSocket(this);
        if( newsocket )
        {
            connect(newsocket,SIGNAL(connected()),this, SLOT(vchannelConnected()) );
            connect(newsocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(vchannelError(QAbstractSocket::SocketError)));
        }
    }
    Q_ASSERT(newsocket);

    if( newsocket == NULL ) return;

    // send the message to the local host (for now)
    if( nevents )
    {
        QDEBUG << "vchannel: sendEventMessage:" << message;
        if( newsocket->state() == QAbstractSocket::UnconnectedState )
        {
            if( qseventaddress.isEmpty() )
            {
                QDEBUG << "vchannel: connect to: localhost";
                newsocket->connectToHost( QHostAddress::LocalHost, EVENT_PORT );
            }
            else
            {
                QDEBUG << "vchannel: connect to:" << qseventaddress;
                newsocket->connectToHost( qseventaddress, EVENT_PORT );
            }
            newevent = QString("<" STR_VCHANNEL " " STR_VCHANNELID "=\"%1\" " STR_VCHANNELSTATE "=\"%2\">%3</" STR_VCHANNEL ">").
                    arg(ndevice).
                    arg(devicestate).
                    arg(message);
            QDEBUG << "message=" << message ;
            return;
        } else
            qWarning() << "event lost:" << message ;
    }
    // delete the socket if send failed
    delete newsocket;
    newsocket = NULL;
}

void VChannel::resizeEvent ( QResizeEvent * event )
{

    int width = event->size().width();
    int height = event->size().height();

    // normal state
    if( restoreHeight == 0 )
    {
        QDEBUG << "resize:" << width << height;
        ui->centralWidget->resize(width,height);
        ui->label->resize(width,height);
    } else
    {
        QDEBUG << "resize min" << width;
        ui->centralWidget->resize(width,0);
        ui->label->resize(width,0);
    }
}

// align windows
void VChannel::slotAlign()
{
    QRect rect = geometry();
    //QDEBUG << "slotAlign: before:" << rect;
    // get the snapping direction right
    int x = rect.x()>0 ? ((rect.x()+20)/40)*40 : ((rect.x()-20)/40)*40;
    int y = rect.y()>0 ? ((rect.y()+15)/30)*30 : ((rect.y()-15)/30)*30;
    int w = ((rect.width()+20)/40)*40;
    int h = ((rect.height()+15)/30)*30;

    // stop if we are already there
    if( x == rect.x() && y == rect.y() && w == rect.width() && h == rect.height() )
            return;

    setGeometry(x,y,w,h);
    statusBar()->showMessage(tr("Posn:%1:%2 %3x%4").arg(x).arg(y).arg(w).arg(h));
    //QDEBUG << "slotAlign: after:" << geometry();
}

void VChannel::disconnected()
{
    myTcpSocket *clientConnection = (myTcpSocket*)sender();

    if( !clientConnection ) return;

    if( clientConnection->state() == QAbstractSocket::ConnectedState  )
    {

        clientConnection->disconnectFromHost();

        QDEBUG << "vchannel: tcp disconnected";
        clientConnection->deleteLater();
    }
}

void VChannel::readIncomingCommand()
{
    myTcpSocket *clientConnection = (myTcpSocket*)sender();

    Q_ASSERT(clientConnection);
    if( !clientConnection ) return;

    //qint64 bytesavail = clientConnection->bytesAvailable();
    //QDEBUG << "readIncomingCommand: bytes avail:" << bytesavail;

    // limited read
    QByteArray qba = clientConnection->read(1024);

    // interpret the command
    if( qba.isEmpty() ) return;
    // QDEBUG << qba;

    QString peer = clientConnection->peerAddress().toString();
    // QDEBUG << "Peer address:" << peer << "event address:" << qseventaddress;
    // guard against outside requests
    // this assumes a /24 subnet and allows all requesters within the subnet
    QString qsnetwork = qseventaddress;
    int last = qseventaddress.lastIndexOf('.');
    if( last != -1 )
    {
        qsnetwork =qsnetwork.left(last);
    }
    if( peer.startsWith("127.0.0.1") || (!qsnetwork.isEmpty() && peer.startsWith(qsnetwork)) )
    {
        // detect all vchannel messages
        int start = qba.indexOf("<" STR_VCHANNEL);
        if( start != -1 )
        {
            tcpRequest(clientConnection, qba );
        } else if( qba.left(64).contains(STR_HTTP) )
        {
            // keepalive if httpRequest
            // returns true
            // QDEBUG << "vchannel: httpRequest: " << clientConnection << " from " << clientConnection->peerAddress() << clientConnection->peerPort();
            if( httpRequest( clientConnection, qba ) )
                return;
        }
    } else
        qWarning() << "Command from unknown source:" << (const char*)peer.toAscii();

    QDEBUG << "vchannel: disconnect From Host" << __FILE__ << __LINE__;
    clientConnection->disconnectFromHost();
    clientConnection->deleteLater();
    clientConnection = NULL;
}

// interpret incoming messages - HTTP
bool VChannel::httpRequest(myTcpSocket * clientConnection, QByteArray &qba )
{
    bool keepalive = false;

    // look for HTTP requests
    QList<QByteArray> qbl = qba.split('\n');

    for( int ii=0; ii< qbl.count(); ii++ )
    {
        if( qbl.at(ii).toLower().contains(QByteArray(STR_KEEPALIVE).toLower()))
        {
            keepalive = true;
            clientConnection->keepalive();
            break;
        }
    }

    // QDEBUG << "------\n" << qbl[0] ;

    QString header;
    QString strdate = QDateTime::currentDateTime().toString("ddd, dd MMM yyyy hh:mm:ss");
    int size = 0;

    // check for snapshot requests
    if( rtspsocket && rtspsocket->isPlaying() && qbl[0].contains(STR_SNAPSHOT) )
    {
    	if( rtspsocket->session()->video()->mediaformat() == 26 )  //MJPEG
    	{
			// for mjpeg get the image from the buffer
			const QByteArray *frame = rtspsocket?rtspsocket->frame():NULL;
			if( frame )
			{
				size = frame->length();
				QDEBUG <<"Jpeg frame length = " << size << " Datetime= " << strdate;
				QString header = QString(STR_HTTP_IMAGE).arg(size).arg(strdate);
				if( keepalive )
					header += STR_KEEPALIVE STR_NL ;
				header += STR_NL;
				QDEBUG << header;
				clientConnection->write( header.toLatin1() );
				clientConnection->write( *frame );

				// QDEBUG << "vchannel: keepalive=" << keepalive;
				return keepalive;
			}
    	} else
       	if( rtspsocket->session()->video()->mediaformat() == 96 ||   //H264
       		rtspsocket->session()->video()->mediaformat() == 97 ||
       		rtspsocket->session()->video()->mediaformat() == 98 )
    	{
       		// for h264 get the QImage and convert to JPG
			QByteArray snapshot;
			QBuffer buffer(&snapshot);
			buffer.open(QIODevice::WriteOnly);
			rtspsocket->rtpSocket()->image().save(&buffer, "JPG");

			const char *frame = snapshot.constData();
            if( frame )
            {
                size = snapshot.length();
                QDEBUG <<"Jpeg frame length = " << size << " Datetime= " << strdate;
                QString header = QString(STR_HTTP_IMAGE).arg(size).arg(strdate);
                if( keepalive )
                    header += STR_KEEPALIVE STR_NL ;
                header += STR_NL;
                QDEBUG << header;
                clientConnection->write( header.toLatin1() );
                clientConnection->write( frame, size );

                // QDEBUG << "vchannel: keepalive=" << keepalive;
                return keepalive;
            }

    	}
    } else
    if( rtspsocket && rtspsocket->isPlaying() && qbl[0].contains(STR_THUMBNAIL) )
    {
        RtpSocket *sock = rtspsocket->rtpSocket();
        if( sock )
        {
            const char *frame = sock->thumb();
            if( frame )
            {
                size = sock->thumb_size();
                // QDEBUG <<"Jpeg frame length = " << size << " Datetime= " << strdate;
                QString header = QString(STR_HTTP_IMAGE).arg(size).arg(strdate);
                if( keepalive )
                    header += STR_KEEPALIVE STR_NL ;
                header += STR_NL;
                // QDEBUG << header;
                clientConnection->write( header.toLatin1() );
                clientConnection->write( frame, size );

                // QDEBUG << "vchannel: keepalive=" << keepalive;
                return keepalive;
            }
        }
    } else
    // check for status requests
    if( qbl[0].contains(STR_STATUSREQ) )
    {
        if( qbl[0].contains("?" STR_SHUTDOWN ) )
        {
            qWarning() << "Shutdown command";
            globalstatus = "Shutting down";

            QTimer::singleShot(600, this, SLOT(close()));
        } else
        if( qbl[0].contains("?" STR_STARTSTOP ) )
        {
            // start/stop command
            streamStartStop();
        } else
        if( qbl[0].contains("?" STR_DEBUGON ) )
        {
            if( debugsetting < 3 ) debugsetting++;
        } else
        if( qbl[0].contains("?" STR_DEBUGOFF ) )
        {
            if( debugsetting>0 ) debugsetting--;
        }

        QString strtmp = QString("Status: %1 [ %2 ]<br/>").arg(ndevice).arg(globalstatus);
        strtmp += strdate + "<br/>";
        strtmp += qscname + "<br/>";
        if( recordschedule.lastEvent().isValid() ) {
            strtmp += "Last event at " + recordschedule.lastEvent().toString(STR_DATETIME_FRIENDLY) + "<br/>";
        }
        if( recordschedule.lastMotion().isValid() ) {
            strtmp += "Last motion detected at " + recordschedule.lastMotion().toString(STR_DATETIME_FRIENDLY) + "<br/>";
        }
        if( recordschedule.isScheduled(QDateTime::currentDateTime()) )
            strtmp += "Recording is active" "<br/>";
        else
            strtmp += "Recording is inactive" "<br/>";

        if( rtspsocket ) {
            strtmp += rtspsocket->strState();
            if( rtspsocket->isWatch() )
                strtmp += "<br/>"  "Watchdog is running";
            else
                strtmp += "<br/>"  "Watchdog is not running";
        } else {
            strtmp += "state = not running";
        }

        if( debugsetting > 0 )
            strtmp += "<br/>" "debug output is ON" ;

        QString statusreply = QString(STR_HTML_REPLY).arg(strtmp);
        size = statusreply.size();
        QDEBUG << "Status Request: response size=" << size;
        QString header = QString(STR_HTTP_200).arg(size).arg(strdate);
        QDEBUG << header + statusreply;
        clientConnection->write( header.toLatin1() );
        clientConnection->write( statusreply.toLatin1() );

        // close connection by returning false
        return keepalive;
    }

    // return error 404 - not Found
    size = strlen(STR_CONTENT_404);
    header = QString(STR_HTTP_404).arg(size).arg(strdate);
    // QDEBUG << header;
    clientConnection->write( header.toLatin1() );
    clientConnection->write( QByteArray(STR_CONTENT_404) );
    // close connection by returning false
    return false;
}

// interpret incoming messages - TCP
void VChannel::tcpRequest(myTcpSocket * /* clientConnection*/, QByteArray &qba, int start )
{
    int deviceid = -1;

    int msg = qba.indexOf(">",start);
    if( msg != -1 )
    {
        // find the device attributes
        // general format of the xml message is;
        // <vchannel device="nn">
        //    ...message...
        // </vchannel>
        QByteArray qba2 = qba.mid(start+strlen("<" STR_VCHANNEL),msg-start-strlen("<" STR_VCHANNEL));
        if( qba2.length() > 0 )
        {
            // remove unwanted spaces within attribute pairs
            qba2.replace(" =","=");
            qba2.replace("= ","=");

            // now separate out the attribute pairs
            QList<QByteArray> qbl = qba2.trimmed().split(' ');
            for( int hh=0; hh<qbl.count(); hh++ )
            {
                QList<QByteArray> qbl2 = qbl[hh].split('=');
                if( qbl2.count()>1 )
                {
                    // get the device id
                    if( qbl2[0].trimmed().toLower() == STR_VCHANNELID )
                        deviceid = qbl2[1].replace("\"","").toInt();
                }
            }

            // update the device list
            if( deviceid != -1 )
            {
                // check the device number is correct
                if( deviceid != ndevice )
                {
                    qWarning() << "Command: Device mismatch -" << deviceid << "does not match" << ndevice;
                    return;
                }
            }
        }
        msg++;
        int end = qba.indexOf("</" STR_VCHANNEL,msg);
        if( end != -1 )
        {
            qba = qba.mid(msg,end-msg);

            // decode the command message
            if( qba.contains("<" STR_MOTION "/>") || qba.contains("<" STR_MOTION "></>"))
            {
                recordschedule.setMotion();
                QDEBUG << "vchannel: received motion event";
            } else
            if( qba.contains("<" STR_EVENT "/>") || qba.contains("<" STR_EVENT "></>"))
            {
                recordschedule.setEvent();
                QDEBUG << "vchannel: received event";
            } else
            if( qba.contains("<" STR_UPDATESCHEDULE ">") )
            {
                // read the new record settings
                int start = qba.indexOf("<" STR_UPDATESCHEDULE ">");
                start = qba.indexOf( ">", start )+1;
                int end =  qba.indexOf( "</" STR_UPDATESCHEDULE ">", start );
                record_settings = qba.mid(start, end-start);
                QDEBUG << "vchannel: received new schedule" << record_settings;
                recordschedule.setSchedule(record_settings);
            }
            else
            if( qba.contains("<" STR_NORECORD "/>") || qba.contains("<" STR_NORECORD "></>"))
            {
                recordschedule.setMode(RECORD_OFF);
                QDEBUG << "vchannel: received RECORD_OFF";
            } else
            if( qba.contains("<" STR_SHUTDOWN "/>") || qba.contains("<" STR_SHUTDOWN "></>"))
            {
                qWarning() << "Shutdown command";
                close();
            }
        }
    }
}


void VChannel::closeEvent(QCloseEvent *event)
{
    QDEBUG << "vchannel: Close event";
    statusBar()->showMessage(tr("Shutting down"));

    if( tcpServer )
        tcpServer->close();

/*
    if( tcpsocket )
        tcpsocket->abort();

    if( clientConnection )
        clientConnection->abort();
*/
    if( rtspsocket && closecounter < 3 )
    {
        setDeviceState(DEVICE_READY);

        if( closecounter == 0 )
        {
            sendEventMessage(tr("Streaming stopped: %1").arg(qscname));

            if( !qsname.isEmpty() )
            {

                QSettings settings;
                settings.beginGroup(qsname);
                settings.setValue("size", size());
                settings.setValue("pos", pos());
                settings.setValue("restore", restoreHeight);
                settings.setValue("state", "shutdown");
                settings.endGroup();
            }
        }

        // wait for all activity to terminate before deleting
        if( rtspsocket->stop() )
        {
            delete rtspsocket;
            rtspsocket = NULL;
        }  else
        {
            event->ignore();
            QTimer::singleShot(1000, this, SLOT(close()) );
            closecounter++;
        }
    }
}

void VChannel::on_minimizerestore()
{
    Q_ASSERT(pbmin);
    if( pbmin == NULL ) return;

    // minimize
    if( restoreHeight == 0 )
    {
        QPixmap pixmap = QPixmap(":/images/normal.png").scaled(16,16);
        pbmin->setIcon(QIcon(pixmap));

        restoreHeight = height();

        move( x(), y()+ height()-statusBar()->height());

        if( nlock != 0 )
        {
			setMaximumSize(width(),0);
			setMinimumSize(width(),0);
        }
        resize( width(),0 );
    } else
    // restore
    {
        QPixmap pixmap = QPixmap(":/images/minimize.png").scaled(16,16);
        pbmin->setIcon(QIcon(pixmap));
        int h = restoreHeight-height();
        int h2 = restoreHeight;
        restoreHeight = 0;
        resize( width(),h2 );
        move( x(), y()-h);
        if( nlock != 0 )
        {
			setMaximumSize(width(),h2);
			setMinimumSize(width(),h2);
        }
    }
}



void VChannel::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void VChannel::on_audio_mute()
{
    Q_ASSERT(pbaudio);

    // turn on/off
    if( naudio == 1 )
    {
        QPixmap pixmap = QPixmap(":/images/audon.png").scaled(16,16);
        pbaudio->setIcon(QIcon(pixmap));
        naudio = 2;
    }else
    {
        QPixmap pixmap = QPixmap(":/images/audoff.png").scaled(16,16);
        pbaudio->setIcon(QIcon(pixmap));
        naudio = 1;
    }
}



//
// event send connected slot
//
void VChannel::eventConnected()
{
    QString qsip = tcpsocket->peerAddress().toString();
    qDebug("vchannel: eventConnected : %s",(const char*)qsip.toAscii());

    // send the data
    tcpsocket->write(qsevent.toLatin1());
    QDEBUG << "vchannel: tcp writing: " << qsevent;
    qsevent.clear();
    tcpsocket->disconnectFromHost();
}

//
// vchannel send connected slot
//
void VChannel::vchannelConnected()
{
    QString qsip = newsocket->peerAddress().toString();
    qDebug("vchannelConnected : %s",(const char*)qsip.toAscii());

    // send the data
    newsocket->write(newevent.toLatin1());
    QDEBUG << "vchannelConnected : tcp writing: " << newevent;
    newevent.clear();
    newsocket->disconnectFromHost();
}

//
// event send error slot
//
void VChannel::eventError(QAbstractSocket::SocketError err)
{
    if ( err == QAbstractSocket::RemoteHostClosedError )
            return;

    qWarning("errorTcp event %d=%s",err,(const char*)tcpsocket->errorString().toAscii());
}

//
// vchannel send error slot
//
void VChannel::vchannelError(QAbstractSocket::SocketError err)
{
    if ( err == QAbstractSocket::RemoteHostClosedError )
            return;

    qWarning() << "vchannel: errorTcp event for vchannel:" << err;
}
