/*
 * vchannel.h
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
 *
 *  -----------------------------------------------------------------------
 */

#ifndef VCHANNEL_H
#define VCHANNEL_H

#include <QtCore>
#include <QMainWindow>
#include <QResizeEvent>
#include <QPushButton>
#include <QLabel>

#define DEFAULT_WIDTH 360
#define DEFAULT_HEIGHT 300

namespace command_line_arguments {

	extern QString directory;
	extern QString qsname;
	extern QString qscname;
	extern QString qshardware;
	extern QString qsurl;
	extern QString qseventaddress;
	extern QString qsauth;
	extern QString record_settings;

	extern int     px;                  // window position
	extern int     py;                  // window position
	extern int     pw;       			// window size
	extern int     ph;      			// window size

	extern int     ndevice;
	extern int     naudio;          	// audio (default off)
	extern int     nborder;          	// border (default on)
	extern int     nevents;          	// events (default off)
	extern int     nlock;          		// lock window (default off)
	extern bool    usetcp;           	// use RTP/TCP rather than RTP/UDP
	extern int     threshold;         	// threshold for motion (0-100)
	extern int     sensitivity;         // number of pixels to change for motion (0-100 % of total)
	extern int     mx;                  // motion window
	extern int     my;                  // motion window
	extern int     mw;                 	// motion window
	extern int     mh;                 	// motion window
}

#include "rtspsocket.h"

class myTcpSocket : public QTcpSocket
{
    Q_OBJECT
public:
    myTcpSocket(QObject * parent = 0);
    ~myTcpSocket();
    void keepalive() { clientTimer.start();}

private:
    QTimer     clientTimer;
};

class myTcpServer : public QTcpServer
{
    Q_OBJECT
public:
    myTcpServer(quint16 port, QObject * parent = 0);

    void incomingConnection(int socket);

protected slots:
    void error ( QAbstractSocket::SocketError socketError );
};



namespace Ui {
    class VChannel;
}

class VChannel : public QMainWindow {
    Q_OBJECT
public:
    VChannel(QWidget *parent = 0);
    ~VChannel();
    void closeEvent(QCloseEvent *event);
    void resizeEvent ( QResizeEvent * event );
    void sendEventMessage(QString message);
    void setDeviceState(int s) { if(s && s < DEVICE_MAX) devicestate = (DEVICE_STATE) s; }

protected:
    void tcpRequest(myTcpSocket * clientConnection, QByteArray &qba,int start=0 );
    bool httpRequest(myTcpSocket * clientConnection, QByteArray &qba );
    void changeEvent(QEvent *e);

protected slots:
    void streamStartStop();
    void on_audio_mute();
    void on_minimizerestore();
    void eventConnected();
    void vchannelConnected();
    void readIncomingCommand();
    void disconnected();
    void eventError(QAbstractSocket::SocketError err);
    void vchannelError(QAbstractSocket::SocketError err);
    void slotAlign();

private:
    Ui::VChannel *ui;
    QPushButton *pbaudio;
    QPushButton * pbmin;

    RtspSocket *rtspsocket;
    QTcpSocket *tcpsocket;
    QTcpSocket *newsocket;
    QTcpServer *tcpServer;

    QString qsevent;
    QString newevent;
    QUrl url;
    QPoint currentpos;
    int restoreHeight;
    int devicestate;
    int closecounter;
};

#endif // VCHANNEL_H
