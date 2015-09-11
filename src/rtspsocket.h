/**
 * FILE:		rtspsocket.h
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

#ifndef RTSPSOCKET_H
#define RTSPSOCKET_H

#include <QLabel>

#include "../include/common.h"
#include "sessiondescription.h"
#include "avformat.h"
#include "aviformat.h"

enum {
      stateInit=0,
      stateOptions,
      stateDescribe,
      stateSetupVideo,
      stateSetupAudio,
      statePlay,
      statePlaying,
      stateTeardown,
      statePause,
      stateRecord,
      stateEnd,
      stateError,
      state_max_rtsp
};

extern const char *strstate[state_max_rtsp];

class RtpSocket;
class AvFormat;

class RtspSocket : public QObject
{
Q_OBJECT
public:
    explicit RtspSocket(QLabel *parent = 0);
    ~RtspSocket();
    void setDevOrder(int d) { devorder = d; }
    void stream(QUrl url, QString chid, bool aud=false);
    bool stop();
    bool isPlaying() { return (state==statePlaying); }
    bool sendOPTIONS();
    bool sendDESCRIBE();
    bool sendSETUP( SessionMedia *session, SessionMedia *media );
    bool sendPLAY(SessionMedia *session, SessionMedia *media);
    bool sendPAUSE();
    bool sendRECORD();
    bool sendTEARDOWN();
    bool sendERRORQUERY();
    bool interpretOPTIONS( QList<QByteArray> & qbl );
    bool interpretDESCRIBE( QList<QByteArray> & qbl );
    bool interpretSETUP( QList<QByteArray> & qbl, SessionMedia *media );
    bool interpretPLAY( QList<QByteArray> & qbl );
    bool interpretPAUSE();
    bool interpretRECORD();
    bool interpretTEARDOWN( QList<QByteArray> & qbl );
    bool interpretERRORQUERY( QList<QByteArray> & qbl );
    bool startRtp();
    bool stopRtp();
    bool writeData();
    const char * strState() { return strstate[state]; }
    bool isWatch() { if( watchdog && watchdog->isActive() ) return true; return false; }
    const QByteArray *frame() { if(avformat) return avformat->frame(); return NULL; }
    RtpSocket *rtpSocket() { if(rtpVideo) return rtpVideo; return NULL; }
    SessionDescription * session() { return &sdp; }

    void updateRtpCounter() { rtpcounter++; }


signals:

public slots:
    void slotConnected();
    void slotDisconnected();
    void slotReadyRead();
    void slotRecover();
    void slotStateMachine();
    void slotAuthenticationRequired ( QNetworkReply * reply, QAuthenticator * authenticator );
    void slotError(QAbstractSocket::SocketError err);
    void processTimeout();

private:
    int     devorder;
    QUrl    _url;
    int     state;
    int     previousState;
    int     cseq;
    bool    audioEnabled;
    QTcpSocket *tcpSocket;
    QByteArray qsRequest;
    QByteArray qsResponse;
    int     content_len;
    QString content_type;
    QString content_base;
    QString session_id;
    QString channelid;

    // options
    bool optDescribe;
    bool optSetup;
    bool optPlay;
    bool optPause;
    bool optRecord;
    bool optTeardown;

    // sdp
    SessionDescription sdp;

    // rtp
    AvFormat *avformat;
    RtpSocket *rtpVideo;
    RtpSocket *rtpAudio;

    // timer
    QTimer *watchdog;
    int    rtpcounter;
    int restart;
};

#endif // RTSPSOCKET_H
