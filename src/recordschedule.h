/**
 * FILE:		recordschedule.h
 *
 * DESCRIPTION:
 * This is the class holding a recording schedule.
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

#ifndef RECORDSCHEDULE_H
#define RECORDSCHEDULE_H

#include "../include/common.h"

class RecordSchedule
{
public:
    RecordSchedule();
    void setSchedule(QString s);
    void setMode( RECORD_MODE m) { mode = m; }
    void setEvent();
    void setMotion();
    void clearEvent() { qdtMotion = qdtEvent = QDateTime(); }
    QString schedule();
    bool isValid() { return (id != -1 && days != DAY_NONE && end > start ); }
    bool isScheduled(QDateTime st = QDateTime(), QDateTime en = QDateTime());
    QString eventType(QDateTime st = QDateTime(), QDateTime en = QDateTime());
    QDateTime lastEvent() { return qdtEvent; }
    QDateTime lastMotion() { return qdtMotion; }

    int id;
    RECORD_MODE mode;
    quint8 days;
    QTime start;
    QTime end;
    QDateTime qdtEvent;
    QDateTime qdtMotion;
};

#endif // RECORDSCHEDULE_H
