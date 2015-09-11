/**
 * FILE:		rtspsocket.cpp
 *
 * DESCRIPTION:
 * This is the class for implementing the Real Time Streaming Protocol.
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

#include <QDebug>
#include <QObject>
#include <QStatusBar>
#include <QLabel>

#include "vchannel.h"
#include "authentication.h"
#include "sessiondescription.h"

#include "rtspsocket.h"
#include "rtpsocket.h"

using namespace command_line_arguments;

extern QStatusBar *statusbar;
extern VChannel   *vchannel;
extern QString globalstatus;

const char *strstate[state_max_rtsp] = { "stateInit","stateOptions","stateDescribe","stateSetupVideo",
                                        "stateSetupAudio","statePlay","statePlaying","stateTeardown",
                                        "statePause","stateRecord","stateEnd","stateError"};

// RtspSocket implements the RTSP protocol exchange
//
RtspSocket::RtspSocket(QLabel *parent) :
    QObject(parent), devorder(0), state(stateInit), previousState(stateInit), cseq(0),
    audioEnabled(false), tcpSocket(NULL),
    optDescribe(false), optSetup(false), optPlay(false),
    optPause(false),optRecord(false), optTeardown(false),
    avformat(NULL),rtpVideo(NULL),rtpAudio(NULL),rtpcounter(0), restart(0)
{
    tcpSocket = new QTcpSocket(this);
    if ( tcpSocket )
    {
        // unlimited buffering
        tcpSocket->setReadBufferSize(0);
        tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        connect(tcpSocket, SIGNAL(connected()), this, SLOT(slotConnected()) );
        connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(slotDisconnected()) );
        connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(slotReadyRead()) );
        connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(slotError(QAbstractSocket::SocketError)));
    }

    // watchdog timer detects loss of incoming packets
    watchdog = new QTimer(this);
    connect(watchdog, SIGNAL(timeout()), this, SLOT(processTimeout()));
    // use the address to seed the rnd
    qsrand((int)(tcpSocket&&0xffff));
    int rnd = qrand()%512;
    watchdog->setInterval(1000+rnd); // ~3 sec timeout randomized
}

RtspSocket::~RtspSocket()
{
    if( avformat )
    {
        delete avformat;
        avformat = NULL;
    }
    if( rtpVideo )
    {
//        rtpVideo->abort();
        delete rtpVideo;
        rtpVideo = NULL;
    }
    if( rtpAudio )
    {
//        rtpAudio->abort();
        delete rtpAudio;
        rtpAudio = NULL;
    }
    if ( tcpSocket )
    {
        tcpSocket->abort();
        delete tcpSocket;
        tcpSocket = NULL;
    }
    if( watchdog )
        watchdog->stop();
}

void RtspSocket::processTimeout()
{
    QDEBUG << "watchdog timeout"  << _url.host() << "state=" << state << "rtpcounter=" << rtpcounter;
    // stream detect
    if( state == statePlaying && rtpcounter > 0 )
    {

        rtpcounter = 0;

        // don't send rtcp with tcp
        // todo: revisit this later
        if( !usetcp )
        {
            // send RTCP CNAME
            QDEBUG << "send RTCP";
            if( rtpVideo )
            {
                QByteArray qba = sdp.video()->transport("server_port").toAscii();
                int port = qba.left(qba.indexOf('-')).toInt()+1;
                rtpVideo->sendRtcp( QHostAddress(_url.host()),port );
            }
        }
        watchdog->start();
    } else
    {
        if( state == statePlaying )
        {
            QString qs = tr("Connection lost to %1").arg(_url.host());
            qWarning() << qs;
            if( statusbar )
                statusbar->showMessage(tr("Connection lost to %1").arg(_url.host()));

            if( vchannel )
            {
                vchannel->setDeviceState(DEVICE_ERROR);
                vchannel->sendEventMessage(qs);
            }
        }

        watchdog->stop();
        state = stateError;

        // set the url to restart
        QDEBUG << strState() << "restart:" << restart;
        // wait 2 secs to restart
        QTimer::singleShot(2000, this, SLOT(slotStateMachine()) );
    }
}

void RtspSocket::stream(QUrl url,QString chid, bool aud)
{
    qDebug("Start Streaming");
    audioEnabled = aud;
    channelid = chid;

    if(url.port() == -1 || url.port()==0 )
        url.setPort(554);

    QDEBUG << url.toString();
    qWarning() << tr("Stream: ") + url.toString();

    if( state == statePlay || state == statePlaying || state == stateTeardown )
    {
        state = stateTeardown;
        // set this after the teardown message
        _url = url;
    } else
    {
        _url = url;
        // restart the state machine
        state = stateInit;
    }
    slotStateMachine();
}

bool RtspSocket::stop()
{
    qWarning() << "Stop Streaming"  << _url.host();
    // return true when we are stopped
    if( state==state_max_rtsp )
        return true;

    // end the state machine
    state = stateTeardown;
    slotStateMachine();
    return false;
}


bool RtspSocket::sendOPTIONS()
{
    QDEBUG << "sendOPTIONS"  << _url.host();
    // close any existing connection


    // make a new connection
    qsRequest.clear();
    qsRequest += QString("OPTIONS %1 RTSP/1.0\r\n").arg(_url.toString());
    qsRequest += QString("CSeq: %1\r\n").arg(state);
    if( !qsauth.isEmpty() )
         qsRequest += QString("Authorization: Basic %1\r\n").arg(qsauth);
    qsRequest += "User-Agent: MyNetEye 1.0 (Live Streaming)\r\n";
    qsRequest += "\r\n";
    tcpSocket->connectToHost( _url.host(), _url.port() );
    return true;
}
bool RtspSocket::interpretOPTIONS(QList<QByteArray> & qbl )
{
    QDEBUG << "interpretOPTIONS"  << _url.host();
    for(int ii=0; ii< qbl.count(); ii++)
    {
        if( qbl.at(ii).trimmed().startsWith("Public:") )
        {
            QString qs = qbl.at(ii).trimmed().mid(7);
            optDescribe = (qs.contains("DESCRIBE") );
            optSetup = (qs.contains("SETUP") );
            optPlay = (qs.contains("PLAY") );
            optPause = (qs.contains("PAUSE") );
            optRecord = (qs.contains("RECORD") );
            optTeardown = (qs.contains("TEARDOWN") );
            QDEBUG << "options: " << optDescribe << optSetup << optPlay << optPause << optRecord  << optTeardown ;
            return true;
        }
    }
    return false;
}


bool RtspSocket::sendDESCRIBE()
{
    QDEBUG << "sendDESCRIBE" << _url.host();
    if( !optDescribe) return false;

    // clear the session descriptor
    sdp.clear();

    // DESCRIBE parameters
    qsRequest.clear();
    qsRequest += QString("DESCRIBE %1 RTSP/1.0\r\n").arg(_url.toString());
    qsRequest += QString("CSeq: %1\r\n").arg(state);
    if( !qsauth.isEmpty() )
        qsRequest += QString("Authorization: Basic %1\r\n").arg(qsauth);
    qsRequest += "Accept: application/sdp\r\n";
    qsRequest += "User-Agent: MyNetEye 1.0 (Live Streaming)\r\n";
    qsRequest += "\r\n";
//    tcpSocket->connectToHost( _url.host(), _url.port() );
    return writeData();
}

bool RtspSocket::interpretDESCRIBE( QList<QByteArray> & qbl )
{
    QDEBUG << "interpretDESCRIBE " << content_type  << _url.host();
    if( content_type.contains("application/sdp") )
    {
        sdp.Interpret(qbl, devorder);
        return true;
    }
    return false;
}

bool RtspSocket::sendSETUP(SessionMedia *session, SessionMedia *media)
{
    QDEBUG << "\nsendSETUP"  << _url.host();
    if ( !optSetup ) return false;

    // SETUP parameters
    qsRequest.clear();
    QString qsurl = session->control();
//    if( !qsurl.isEmpty() /* && qsurl != "*" */ )
        qsurl += media->control();
    if( !qsurl.startsWith("rtsp") )
        qsurl = content_base.isEmpty()?_url.toString():content_base + qsurl;
    qsRequest += QString("SETUP %1 RTSP/1.0\r\n").arg(qsurl);
    qsRequest += QString("CSeq: %1\r\n").arg(++cseq);
    if( !qsauth.isEmpty() )
        qsRequest += QString("Authorization: Basic %1\r\n").arg(qsauth);
    qsRequest += "User-Agent: MyNetEye 1.0 (Live Streaming)\r\n";

    if( usetcp )
        qsRequest += QString("Transport: %1/TCP;unicast;client_port=%2\r\n").arg(media->protocol()).arg(media->transport("client_port"));
    else
        qsRequest += QString("Transport: %1/UDP;unicast;client_port=%2\r\n").arg(media->protocol()).arg(media->transport("client_port"));

    qsRequest += "\r\n";
    return writeData();
}

bool RtspSocket::interpretSETUP( QList<QByteArray> & qbl, SessionMedia *media )
{
    bool ret = false;
    QDEBUG << "interpretSETUP"  << _url.host();
    for(int ii=0; ii< qbl.count(); ii++)
    {
        // capture the transport
        if( qbl.at(ii).trimmed().startsWith("Transport:") )
        {
            media->setTransport(qbl.at(ii).trimmed().mid(strlen("Transport:")).trimmed());
        } else
        // look for the Session
        if( qbl.at(ii).trimmed().startsWith("Session:") )
        {
            if( session_id.isEmpty() )
            {
                QList<QByteArray> list = qbl.at(ii).trimmed().split(';');
                if( !list.isEmpty() )
                {
                    session_id = list.first().trimmed().mid(strlen("Session:")).trimmed();
                    QDEBUG << "Session=" << session_id;
                    ret=true;
                }
            } else
            if( session_id == qbl.at(ii).trimmed().mid(strlen("Session:")).trimmed() )
                ret=true;
            else
                qWarning() << "Session id mismatch" << session_id;
        }
    }
    return ret;
}

bool RtspSocket::sendPLAY(SessionMedia *session, SessionMedia *media)
{
    QDEBUG << "sendPLAY"  << _url.host();
    if ( !optPlay ) return false;
    rtpcounter = 0;

    QString qsurl = session->control();
    if( !qsurl.isEmpty() /* && qsurl != "*" */ )
        qsurl += media->control();
    if( !qsurl.startsWith("rtsp") )
        qsurl = content_base.isEmpty()?_url.toString():content_base + qsurl;
    QDEBUG << "url=" << qsurl;
    // PLAY parameters
    qsRequest.clear();
    qsRequest += QString("PLAY %1 RTSP/1.0\r\n").arg(qsurl);
    qsRequest += QString("CSeq: %1\r\n").arg(++cseq);
    qsRequest += QString("Session: %1\r\n").arg(session_id);
    qsRequest += "User-Agent: MyNetEye 1.0 (Live Streaming)\r\n";
    qsRequest += "\r\n";
    return writeData();
 }

bool RtspSocket::interpretPLAY( QList<QByteArray> & qbl )
{
    QDEBUG << "interpretPLAY"  << _url.host();
    for(int ii=0; ii< qbl.count(); ii++)
    {
        // split up the parameters (separated by ';')
        QList<QByteArray> ql = qbl.at(ii).trimmed().split(';');
        // look for the Session
        if( ql.at(0).trimmed().startsWith("Session:") )
        {
            if( session_id == ql.at(0).trimmed().mid(strlen("Session:")).trimmed() )
            {
                // success, reset the restart counter
                restart = 0;
                // clear the buffers
                content_len = 0;
                qsResponse.clear();
                tcpSocket->readAll();

                return true;
            } else
                qWarning() << "Session id mismatch";
        }
    }
    return false;
}

bool RtspSocket::sendPAUSE()
{
    qDebug("Todo sendPAUSE");
    if ( !optPause ) return false;

    return false;
}
bool RtspSocket::interpretPAUSE()
{
    qDebug("interpretPAUSE");
    return true;
}

bool RtspSocket::sendRECORD()
{
    qDebug("Todo sendRECORD");
    if ( !optRecord ) return false;

    return false;
}
bool RtspSocket::interpretRECORD()
{
    qDebug("interpretRECORD");
    return true;
}

bool RtspSocket::sendTEARDOWN()
{
    QDEBUG << "sendTEARDOWN"  << _url.host();
    if ( !optTeardown ) return false;

    // TEARDOWN parameters
    qsRequest.clear();
    qsRequest += QString("TEARDOWN %1 RTSP/1.0\r\n").arg(_url.toString());
    qsRequest += QString("CSeq: %1\r\n").arg(++cseq);
    qsRequest += QString("Session: %1\r\n").arg(session_id);
    qsRequest += "User-Agent: MyNetEye 1.0 (Live Streaming)\r\n";
    qsRequest += "\r\n";
    return writeData();
}

bool RtspSocket::interpretTEARDOWN( QList<QByteArray> & qbl )
{
    QDEBUG << "interpretTEARDOWN" << _url.host();
//    return true;
    for(int ii=0; ii< qbl.count(); ii++)
    {
        // look for the Session
        if( qbl.at(ii).trimmed().startsWith("Session:") )
        {
            if( session_id == qbl.at(ii).trimmed().mid(strlen("Session:")).trimmed() )
            {
                QDEBUG << "End session:" << session_id;
                break;
            } else
                qWarning() << "Session id mismatch";
        }
    }
    // clear the session
    session_id.clear();

    // don't need the url any more
    QDEBUG << "Restart=" << restart;
    if( restart == 0 || restart > MAX_RESTART_RETRIES )
        _url = QUrl();

    return true;
}

bool RtspSocket::sendERRORQUERY()
{
    QDEBUG << "sendERRORQUERY (OPTIONS)"  << _url.host();
    rtpcounter = 0;
    restart++;

    // OPTIONS parameters
    // ping the other side...
    qsRequest.clear();
    qsRequest += QString("OPTIONS %1 RTSP/1.0\r\n").arg(_url.toString());
    qsRequest += QString("CSeq: %1\r\n").arg(++cseq);
    qsRequest += "User-Agent: MyNetEye 1.0 (Live Streaming)\r\n";
    qsRequest += "\r\n";
    return writeData();
}

bool RtspSocket::interpretERRORQUERY( QList<QByteArray> & qbl )
{
    QDEBUG << "interpretERRORQUERY" << _url.host();

    for(int ii=0; ii< qbl.count(); ii++)
    {
        // look for the Session
        if( qbl.at(ii).trimmed().startsWith("Public:") )
        {
             QDEBUG << "restart successful" ;
             if( interpretOPTIONS(qbl) )
             {
                 state = stateOptions;
                 return true;
             }
             // todo
             // we may need to check for other states,
             // but for now error recovery sends only
             // OPTIONS requests
        }
    }
    qWarning() << "unknown response";
    tcpSocket->abort();
    return false;
}

    // function to write data to the tcp socket
bool RtspSocket::writeData()
{
    QDEBUG << "writeData";
    // clear received parms
    cseq = 0;
    content_len = 0;
    content_type.clear();

    // check that we are connected before trying to write
    if ( tcpSocket->state() != QAbstractSocket::ConnectedState )
    {
        QDEBUG << "writeData: Not connected";
        return false;
    }

    // send the data, if any
    if( qsRequest.isEmpty() || (tcpSocket->write(qsRequest) != -1) )
    {
        qDebug("tcp writing: %s", qsRequest.left(254).data());
        return true;
    }

    QDEBUG << "write error!" ;
    return false;
}


// SLOTS
void RtspSocket::slotConnected()
{
    QDEBUG << QString("Connected: bytes avail=%1 state=%2").arg((int)tcpSocket->bytesAvailable()).arg((int)tcpSocket->state());
    QString qsip = tcpSocket->peerAddress().toString();
    QDEBUG << QString("connectedTcp : %1").arg(qsip);

    // call write function, ignore the write result
    writeData();
}


void RtspSocket::slotDisconnected()
{
    QByteArray response;
    // QString address = tcpSocket->peerAddress().toString();
    response = tcpSocket->readAll();
    QDEBUG << "disconnected received :  " << response.length() << "bytes";
    QDEBUG <<"state=" << strState();

    if( ! response.isEmpty() )
        qWarning() << tr("Disconected:")+QString(response);

    switch( state )
    {
//        case stateInit:          break;
//        case stateOptions:       break;
//        case stateDescribe:      break;
//        case stateSetupVideo:    break;
//        case stateSetupAudio:    break;
//        case statePlay:          break;
//        case stateEnd:           break;
          case stateError:
        QDEBUG << "already in error state: retrying...";
          QTimer::singleShot( 1000, this, SLOT(slotStateMachine()) );
            break;

          case statePlaying:
            state = stateError;
            // this should cause the error recovery to try again
            QDEBUG << "Error state : retrying..." ;
            QTimer::singleShot( 1000, this, SLOT(slotStateMachine()) );
            break;
//        case statePause:
//        case stateRecord:
          default:
            break;
    }
}

void RtspSocket::slotReadyRead()
{
    // QDEBUG << "readyRead: bytes avail=" << (int)tcpSocket->bytesAvailable() << " state=" << tcpSocket->state();

    // QString address = tcpSocket->peerAddress().toString();

    // see whether this is a continuation packet
    if( content_len == 0)
    {
        if( state == statePlaying )
        {
            qsResponse = tcpSocket->read(4);
            // interleaved video/audio
            if( qsResponse[0] == (char)0x24 /* && qsResponse[1] == (char)0x00 */)
            {
                content_len = (unsigned char)qsResponse[2] * 256 + (unsigned char)qsResponse[3];
                if( content_len > (int)tcpSocket->bytesAvailable() )
                    return;
                qsResponse = tcpSocket->read(content_len);
                content_len = 0;
            }
        } else
            qsResponse = tcpSocket->readAll();
    } else
    {
        if( content_len > (int)tcpSocket->bytesAvailable() )
            return;
        qsResponse = tcpSocket->read(content_len);
        content_len = 0;
    }

    QList<QByteArray> qbl;
    if( state != statePlaying )
    {
        QDEBUG <<"\n" << (const char*)qsResponse.data() << "\n-----------------------";
        // break the response into separate string
        qbl = qsResponse.split('\r');

        // test for RTSP success
        if( qbl.at(0).trimmed().startsWith("RTSP") )
        {
            if ( !qbl.at(0).trimmed().contains("200") )
            {
                qWarning() << "Error: " << qbl.at(0).trimmed();

                if ( qbl.at(0).trimmed().contains("455") )
                {
                    QDEBUG << "State=" << state;
                    state = stateEnd;
                    slotStateMachine();
                } else
                if ( qbl.at(0).trimmed().contains("400") )
                {
                    QDEBUG << "Camera says 'Bad Request'' State=" << state;
                    state = stateEnd;
                    slotStateMachine();
                }
                return;
            }
            // decode the response
            for(int ii=0; ii< qbl.count(); ii++)
            {
                QDEBUG << "line:" <<ii <<" ::" << qbl.at(ii).trimmed();
                // look for the CSeq (state)
                if( qbl.at(ii).trimmed().startsWith("CSeq:") )
                {
                    cseq = qbl.at(ii).trimmed().mid(strlen("CSeq:")).toInt();
                } else
                // look for the Content-Length
                if( qbl.at(ii).trimmed().startsWith("Content-Length:") )
                {
                    content_len = qbl.at(ii).trimmed().mid(strlen("Content-Length:")).toInt();
                    // see how many bytes are remaining
                    // get the end of header position
                    int eoh= qsResponse.indexOf("\r\n\r\n")+4;
                    if( qsResponse.length() > eoh )
                    {
                        qsResponse = qsResponse.mid(eoh);
                        content_len -= (int)qsResponse.length();
                        QDEBUG << "remaining after the header =" << content_len;
                        if ( content_len < 0 ) content_len = 0;
                    }

                } else
                // look for the Content-Type
                if( qbl.at(ii).trimmed().startsWith("Content-Type:") )
                {
                    content_type = qbl.at(ii).trimmed().mid(strlen("Content-Type:")).trimmed();
                } else
                // look for the Content-Base
                if( qbl.at(ii).trimmed().startsWith("Content-Base:") )
                {
                    content_base = qbl.at(ii).trimmed().mid(strlen("Content-Base:")).trimmed();
                }
            }
        } else
        {
            // check for error condition
            if( state == stateError )
            {
                QDEBUG << "garbage!";
                tcpSocket->abort();
                content_len = 0;
                qbl.clear();
            }
        }
    }
    if( cseq != stateInit && content_len == 0 )
    {
        bool ret = false;

        switch( state )
        {
        case stateInit:          qDebug("stateInit");                       break;
        case stateOptions:       ret = interpretOPTIONS(qbl);               break;
        case stateDescribe:      ret = interpretDESCRIBE(qbl);              break;
        case stateSetupVideo:    ret = interpretSETUP(qbl, sdp.video());    break;
        case stateSetupAudio:    ret = interpretSETUP(qbl, sdp.audio());    break;
        case statePlay:          ret = interpretPLAY(qbl);                  break;
        case stateEnd:           ret = interpretTEARDOWN(qbl);              break;
        case stateError:         ret = interpretERRORQUERY(qbl);            break;
        case statePlaying:       if( rtpVideo )
                                    rtpVideo->decodeDatagrams(qsResponse.constData(), qsResponse.length());
                                 return;
        case statePause:
        case stateRecord:
        default:
            break;
        }
        if( ret ) {
            watchdog->start();
        } else
        {
            state = stateError;
            watchdog->stop();
        }
        QTimer::singleShot(10, this, SLOT(slotStateMachine()) );
    }
}

// filter out timer events that fire after we have recovered
void RtspSocket::slotRecover()
{
    QDEBUG << "slotRecover";
    if( state == stateError )
        slotStateMachine();
}

void RtspSocket::slotStateMachine()
{
    qDebug("RtspSocket slotStateMachine");
    QDEBUG << "stateMachine current:" << state;
    switch( state )
    {
    case stateInit:
        state=stateOptions;
        QDEBUG << "stateOptions";
        sendOPTIONS();
        watchdog->start();               // set timeout in case of no reply
        break;
    case stateOptions:
        state=stateDescribe;
        QDEBUG << "stateDescribe";
        sendDESCRIBE();
        watchdog->start();               // set timeout in case of no reply
        break;
    case stateDescribe:
        state=stateSetupVideo;
        QDEBUG << "stateSetupVideo";
        sendSETUP( sdp.session(), sdp.video() );
        watchdog->start();               // set timeout in case of no reply
        break;
    case stateSetupVideo:
        if( audioEnabled && !sdp.audio()->isEmpty() )
        {
            state=stateSetupAudio;
            QDEBUG << "stateSetupAudio";
            sendSETUP( sdp.session(), sdp.audio() );
            watchdog->start();               // set timeout in case of no reply
            break;
        }
        // if not fall through to start playing
    case stateSetupAudio:
        state=statePlay;
        QDEBUG << "statePlay";
        sendPLAY(sdp.session(), sdp.video());
        watchdog->start();               // set timeout in case of no reply
        break;
    case statePlay:
        startRtp();
        Q_ASSERT(rtpVideo);
        state=statePlaying;
        if( vchannel )
        {
            QString qs = QString("Start playing %1").arg(_url.host());
            vchannel->setDeviceState(DEVICE_ACTIVE);
            vchannel->sendEventMessage(qs);
        }
        QDEBUG << "playing....";
        break;
    case statePlaying:
        QDEBUG << "statePlaying";
        break;
    case statePause:
        QDEBUG << "statePause";
        state=stateTeardown;
        break;
    case stateRecord:
        QDEBUG << "stateRecord";
        state=stateTeardown;
        break;
    case stateTeardown:
        QDEBUG << "stateTeardown";
        if( ! sendTEARDOWN() )
        {
            // if we are disconnected, try to connect again
            tcpSocket->connectToHost( _url.host(), _url.port() );
        }
        state=stateEnd;
        QDEBUG << "Teardown sent";
        break;

    case stateError:
        QDEBUG << "stateError";
        if( restart < MAX_RESTART_RETRIES )
        {
            if( ! sendERRORQUERY() )
            {
                // if we are disconnected, try to connect again
                tcpSocket->connectToHost( _url.host(), _url.port() );
            }
            state = stateError;   // don't change state until response received
            // wait multiples of 20 secs to restart
            QTimer::singleShot(20000, this, SLOT(slotRecover()) );
            QDEBUG << "restart=" << restart << "- try again in 20 s";
        } else
        {
            state = stateEnd;
            QDEBUG << "error termination" ;
            vchannel->setDeviceState(DEVICE_TERMINATING);
            vchannel->sendEventMessage(tr("Error termination: %1").arg(_url.host()));
            QTimer::singleShot(100, this, SLOT(slotStateMachine()) );
        }
        break;

    case stateEnd:
        QDEBUG << "stateEnd";
        stopRtp();
        QDEBUG << "close socket";
        tcpSocket->close();  // todo
        session_id.clear();
        if( !_url.isEmpty() && restart < MAX_RESTART_RETRIES )
        {
            QDEBUG << "next url=" << _url.toString();
            state=stateOptions;
            QDEBUG << "stateOptions";
            sendOPTIONS();
            watchdog->start();               // set timeout in case of no reply
        } else
        {
            qWarning() << "Thats all folks!";
            state=state_max_rtsp;
			globalstatus = "Stopped";
            QTimer::singleShot(500, vchannel, SLOT(close()) );
        }
        break;
    default:
        QDEBUG << "state end";
        QDEBUG << "close socket";
        tcpSocket->close();  // todo
        break;
    }
}

void RtspSocket::slotAuthenticationRequired ( QNetworkReply * , QAuthenticator * authenticator )
{
    QDEBUG << tr("User: ")+authenticator->user();
    if( !authenticator->user().isEmpty() )
    {
        if( authenticator->user() == _url.userName() && authenticator->password() == _url.password() )
        {
            _url.setUserName(DEFAULT_USERNAME);
            _url.setPassword(DEFAULT_PASSWORD);
        } else
        {
            Authentication auth;
            auth.setValues(_url.userName(), _url.password(), false);
            auth.show();
            if( auth.exec() )
            {
                _url.setUserName(auth.getUser());
                _url.setPassword(auth.getPass());
            }
        }
    }
    authenticator->setUser(_url.userName());
    authenticator->setPassword(_url.password());
}


void RtspSocket::slotError(QAbstractSocket::SocketError err)
{
    qWarning("errorTcp %d=%s",err,(const char*)tcpSocket->errorString().toLatin1());
    if ( err == QAbstractSocket::RemoteHostClosedError )
    {
        // this means that the host probably reset
        qWarning() << "vchannel resetting...";
    }
}

bool RtspSocket::startRtp()
{
    QDEBUG << "startRtp";
    STREAMS stream;

    if( avformat == NULL )
    {
		ChannelFormat * channel0 =  NULL;
		ChannelFormat * channel1 = NULL;
    	Q_ASSERT(sdp.video());
		switch( sdp.video()->mediaformat() )
		{
		case 26:  // MJPEG
			// create the recording channels needed by avformat, depending on the compression
			channel0 =  (ChannelFormat*)( new AviChannelFormat());
			channel1 = (ChannelFormat*)  (new AviChannelFormat());
			// create the class to manage these recording channels
			avformat = new AvFormat(channel0, channel1);
			break;
		default:
			// h264
			if( sdp.video()->mediaformat() >=96 && sdp.video()->mediaformat() < 128 )
			{
				// create the recording channels needed by avformat, depending on the compression
				channel0 =  (ChannelFormat*)( new Mp4ChannelFormat());
				channel1 = (ChannelFormat*)  (new Mp4ChannelFormat());
				// create the class to manage these recording channels
				avformat = new AvFormat(channel0, channel1);
			} else {
				qWarning() << "Unknown media format " << sdp.video()->mediaformat();
			}
			break;
		}
    }
	Q_ASSERT(avformat);

    if( rtpVideo == NULL )
        rtpVideo = new RtpSocket(this, avformat);
	Q_ASSERT(rtpVideo);
    if( rtpVideo )
    {
        QByteArray qba = sdp.video()->transport("client_port").toLatin1();
        int port = qba.left(qba.indexOf('-')).toInt();
        if( rtpVideo->init(port) )
        {
            QDEBUG << "rtp bind success port=" << port;
            stream = STREAMS_VIDEO;
        }
        rtpVideo->setLabel((QLabel*)parent());
    }

    // if we are using tcp then only one channel is needed for all streams
    if( !usetcp )
    {
        if( audioEnabled && rtpAudio == NULL )
            rtpAudio = new RtpSocket(this, avformat);
        if( rtpAudio )
        {
            QByteArray qba = sdp.audio()->transport("client_port").toLatin1();
            int port = qba.left(qba.indexOf('-')).toInt();
            if( rtpAudio->init(port) )
            {
                QDEBUG << "rtp bind success port=" << port;
                if( stream == STREAMS_VIDEO)
                    stream = STREAMS_AV;
                else
                    stream = STREAMS_AUDIO;
            }
        }
    } else
    {
        if( audioEnabled )
            if( stream == STREAMS_VIDEO )
                stream = STREAMS_AV;
    }

    if( avformat )
        avformat->setChannelID(channelid, stream);
    return true;
}

bool RtspSocket::stopRtp()
{
    QDEBUG << "stopRtp";


    if( avformat )
        avformat->stop();

    if( rtpVideo )
    {
       rtpVideo->closeSocket();
    }
    if( rtpAudio )
    {
       rtpAudio->closeSocket();
    }
    if( avformat )
    {
        // this causes the data to be written to disk
        avformat->writeFinal();
    }
    if( watchdog )
    {
        watchdog->stop();
    }
    return true;
}
