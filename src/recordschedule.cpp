/**
 * FILE:		recordschedule.cpp
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

#include "../include/common.h"
#include "recordschedule.h"

////////////////////////////////////////////////////////////////
// RecordSchedule class
//
RecordSchedule::RecordSchedule() : id(0),mode(RECORD_OFF),days(DAY_NONE)
{

}

void RecordSchedule::setSchedule(QString s)
{
    QDEBUG << "RecordSchedule::setSchedule" << s;
    // split the parts
    QStringList qsl = s.split('-');
    // not a valid schedule
    if( qsl.count() != 5 ) return;

    id = qsl.at(0).toInt();
    mode = (RECORD_MODE)qsl.at(1).toInt();
    days = (quint8)qsl.at(2).toInt();
    start= QTime::fromString(qsl.at(3),"HH:mm");
    end=QTime::fromString(qsl.at(4),"HH:mm");
}


void RecordSchedule::setMotion()
{
    qdtMotion = QDateTime::currentDateTime();
    qWarning() << "Notification: Motion" << qdtEvent.toString(Qt::ISODate);
}

void RecordSchedule::setEvent()
{
    qdtEvent = QDateTime::currentDateTime();
    qWarning() << "Notification: Event" << qdtEvent.toString(Qt::ISODate);
}

QString RecordSchedule::schedule()
{
    // check for a valid string
    if( id == -1 )
        return QString();

    QString s = QString("%1-%2-%3-%4-%5").arg(id).arg(mode).arg(days).arg(start.toString("HH:mm")).arg(end.toString("HH:mm"));
    qWarning() << "Notification: Schedule changed " << s;
    return s;
}

bool RecordSchedule::isScheduled(QDateTime st, QDateTime en)
{
	bool ret= false;
    QDEBUG << "isScheduled" << mode;

    if( en.isNull() ) en = QDateTime::currentDateTime();
    if( st.isNull() ) st.setTime_t(0);

    if( mode == RECORD_ALWAYS ) return true;

    if( mode == RECORD_SCHEDULE ||
        mode == RECORD_SCHEDULE_MOTION )
    {
        int day = st.date().dayOfWeek() -1;
        QDEBUG << "days" << day << " <- " << days;
        if( (1 << day) & days )
        {
            QDEBUG << start.toString() << st.time().toString() << end.toString();
            if( start <= st.time() && end > st.time() )
            {
                if( mode == RECORD_SCHEDULE )
                    ret=true;
                else
                if( qdtMotion.isValid() &&
                        qdtMotion.addSecs(SECS_AFTER_EVENT) >= st &&
                        qdtMotion.addSecs(-SECS_BEFORE_EVENT) < en )
                    ret=true;
            }
        }
    }
    else
    if( mode == RECORD_MOTION &&
            qdtMotion.isValid() &&
            qdtMotion.addSecs(SECS_AFTER_EVENT) >= st &&
            qdtMotion.addSecs(-SECS_BEFORE_EVENT) < en )
        ret=true;
    else
    if( qdtEvent.isValid() &&
            qdtEvent.addSecs(SECS_AFTER_EVENT) >= st &&
            qdtEvent.addSecs(-SECS_BEFORE_EVENT) < en )
        ret=true;
    return ret;
}

//
// get the type of recording event
// N (normal - no event)
// M (motion event)
// E (other event)
//
QString RecordSchedule::eventType(QDateTime st, QDateTime en)
{
    QString qs= STR_TAG_NONE;

    if( en.isNull() ) en = QDateTime::currentDateTime();
    if( st.isNull() ) st.setTime_t(0);

    if( qdtMotion.isValid() )
    {
        if( qdtMotion.addSecs(SECS_AFTER_EVENT) >= st &&
            qdtMotion.addSecs(-SECS_BEFORE_EVENT) < en )
            qs = STR_TAG_MOTION;
    } else
    if( qdtEvent.isValid() )
    {
        if( qdtEvent.addSecs(SECS_AFTER_EVENT) >= st &&
            qdtEvent.addSecs(-SECS_BEFORE_EVENT) < en )
            qs = STR_TAG_EVENT;
    }

    return qs;
}
