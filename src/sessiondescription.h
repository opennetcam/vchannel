/**
 * FILE:		sessiondescription.h
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

#ifndef SESSIONDESCRIPTION_H
#define SESSIONDESCRIPTION_H
#include "../include/common.h"

// Session Media Class
class SessionMedia
{
public:
    SessionMedia();
    void setDevOrder(int d) { devorder = d; }
    void setMedia(QByteArray & m);
    void setAttribute(QByteArray a);
    void setTransport(QByteArray t);
    bool isEmpty() { return qsControl.isEmpty() ;}
    void clear();

    QString control() { return qsControl; }
    QString protocol() { return qsProtocol; }
    QString transport(QString key){ return qmTransport.value(key); }
    int mediaformat() { return format; }
    QByteArray fmtp(QString key){ return qmFmtp.value(key); }

private:
    int devorder;       // unique device identifier
    QString media;
    int port;
    QString qsProtocol;
    int format;
    QString qsControl;
    QMap<int,QString> rtpmap;
    QMap<QString,QString> qmTransport;
    QMap<QString,QByteArray> qmFmtp;
};

// Session Description Class
class SessionDescription
{
public:
    SessionDescription();
    void Interpret(QList<QByteArray> &  qbl, int devorder);
    SessionMedia *session() { return &sessionMedia; }
    SessionMedia *video() { return &videoMedia; }
    SessionMedia *audio() { return &audioMedia; }
    void clear();

private:
    int version;
    QString connection;
    QString originator;
    QString name;

    SessionMedia sessionMedia;
    SessionMedia videoMedia;
    SessionMedia audioMedia;
};

#endif // SESSIONDESCRIPTION_H
