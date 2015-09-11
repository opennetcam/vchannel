/**
 * FILE:		rtpsocket.h
 *
 * DESCRIPTION:
 * This is the class for implementing RTP over UDP
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

#ifndef RTPSOCKET_H
#define RTPSOCKET_H

#include <QUdpSocket>
#include <QLabel>

#include "../include/common.h"
#include "jpegvideo.h"
#include "h264video.h"
#include "pcmaudio.h"
#include "avformat.h"
#include "aviformat.h"
#include "rtspsocket.h"


// class for creating RTCP packets
class RtcpPacket
{
public:
    RtcpPacket(quint32 s, QString cn);
    QByteArray packetCNAME() { return cname; }
    QByteArray packetRR() ;
//    QByteArray packetBYE();

private:
    QByteArray cname;
    quint8 header[4];
    quint32 myssrc;

public:
    quint32 ssrc[3];
    quint8  payload[3];
    quint32 sequence[3];
    quint32 timestamp[3];
    quint16 bad_sequence[3];
    quint32 lost[3];
    quint16 expected[3];
};

// RTP class
class RtpSocket : public QUdpSocket
{
Q_OBJECT
public:
    explicit RtpSocket(RtspSocket *parent = 0,
                       AvFormat *av=NULL);
    ~RtpSocket();
    bool init(int port);
    void closeSocket();
    void setLabel(QLabel *l) { label = l;}
    int detectMotion(const unsigned char *data, unsigned int size, bool isJpeg, int w=0, int h=0, int bpp=0 );
    QImage & image() { return qimg; }
    void displayImage(const unsigned char * imagedata, int size, int w, int h);
    void sendRtcp(QHostAddress host,int port);
    void decodeDatagrams(const char *datagram, qint64 datacnt);
#ifndef _WIN32
    const char *thumb() { return thumb_data; }
    int thumb_size() { return thumb_sz; }
#else
    const char *thumb() { return thumbnail.constData(); }
    int thumb_size() { return thumbnail.length(); }
#endif
signals:

public slots:
    void readPendingDatagrams();
    void readRTCPDatagrams();

private:
    bool initialized;
    QLabel *label;
    jpegVideo *jpegvideo;
    h264Video *h264video;
    pcmAudio  *pcmaudio;
    RtcpPacket *rtcppacket;
    QByteArray message;
    int packetSize;
    AvFormat *avformat;
    quint16 senderPort;
    QHostAddress sender;
    QUdpSocket *rtcpSocket;
    char* ipdatagram;
    int ipsz;
    int mediaformat;
    SessionDescription *sdp;


//#ifdef _WIN32
    QByteArray thumbnail;
//#endif
    const char *thumb_data;
    int thumb_sz;
    QDateTime      lastdecode;
    QImage qimg;
};

#endif // RTPSOCKET_H
