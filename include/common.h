/*
 * common.h
 *
 *  DESCRIPTION:
 *  This is the common definitions file
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

#ifndef COMMON_H
#define COMMON_H


#include <QtDebug>
#include <QString>
#include <QTime>
#include <QStringList>
#include <QtNetwork>
#include <QUrl>

extern int debugsetting;
#define QDEBUG  if(debugsetting>0)qDebug()<<QDateTime::currentDateTime().toUTC().toTime_t()<<__FILE__<<__LINE__ <<__FUNCTION__
#define QDDEBUG  if(debugsetting>1)qDebug()<<__FILE__<<__LINE__ <<__FUNCTION__

#define ORGANIZATION_NAME "OpenNetCam"
#define ORGANIZATION_SHORT "opennetcam"
#define ORGANIZATION_DOMAIN_NAME "opennetcam.net"

// keep in sync!
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define STR_VERSION_MAJOR "1"
#define STR_VERSION_MINOR "0"

#define STR_VERSION STR_VERSION_MAJOR"."STR_VERSION_MINOR

#define STR_DATETIME_FRIENDLY "ddd, dd MMM yyyy hh:mm:ss"

// these must always correspond to the combobox
enum VIDEO_RES { VIDEO_MAX_RES=0, VIDEO_640x480, VIDEO_320x240, VIDEO_160x120, VIDEO_OFF };
enum VIDEO_COMPRESSION { VIDEO_COMP_NONE=0, VIDEO_COMP_JPEG, VIDEO_COMP_H264 };
enum AUDIO_SETTINGS { AUDIO_OFF=0, AUDIO_ON };

// auth configuration
enum { ConfigRememberAsk=0,ConfigRememberAll,ConfigRememberNone };

// record settings is an array of strings defined as follows:
// ID-MO-DA-SH:SM-EH:EM, .... where each item is a character as follows;
//  ID identifies the setting (there can be up to 255 settings)
//  MO (mode) is RECORD_MODE
//  DA (days) is the OR'd list of days (SUN=1,MON=2,TUES=4,WED=8,THU=16,FRI=32,SAT=64)
//  SHSM is start time HR:MI
//  EHEM is end time HR:MI
//

#define RECORD_SETTINGS_ALL "00-01-255-00:00-23:59"

enum RECORD_MODE { RECORD_OFF=0, RECORD_ALWAYS, RECORD_SCHEDULE, RECORD_MOTION, RECORD_SCHEDULE_MOTION, RECORD_SETTINGS_MAX };
enum DAY_OF_WEEK { DAY_NONE=0, DAY_SUN=1, DAY_MON=2, DAY_TUE=4, DAY_WED=8, DAY_THU=16, DAY_FRI=32, DAY_SAT=64, DAY_SPECIAL=128, DAY_ALL = 255 };
enum RECORD_LIMIT_ACTION { RECORD_LIMIT_ERASE=0, RECORD_LIMIT_STOP, RECORD_LIMIT_PAUSE, RECORD_LIMIT_ACTION_MAX };

enum EVENT_SETTINGS { EVENT_KEEP_ALL=0,
                      EVENT_DELETE_90,
                      EVENT_DELETE_60,
                      EVENT_DELETE_45,
                      EVENT_DELETE_30,
                      EVENT_DELETE_14,
                      EVENT_DELETE_7,
                      EVENT_DELETE_3,
                      EVENT_DELETE_2,
                      EVENT_DELETE_1,
                      EVENT_SETTINGS_MAX };

enum EVENT_TYPES { EVENT_TYPE_NONE=0,
                   EVENT_TYPE_MOTION,
                   EVENT_TYPE_USER,
                   EVENT_TYPE_ERROR,
                   EVENT_TYPE_RECORDING,
                   EVENT_TYPE_OTHER,
                   EVENT_TYPE_MAX };
enum AVI_TYPES { AVI_TYPE_NONE=0,
                   AVI_TYPE_MOTION,
                   AVI_TYPE_USER,
                   AVI_TYPE_OTHER,
                   AVI_TYPE_ARCHIVE,
                   AVI_TYPE_MAX };



#define RECORD_FILETIME_WRITEON 180000 // default 180000 == 3 mins
#define RECORD_FILETIME_NOWRITE 30000

#define MAX_RESTART_RETRIES 6

#define SECS_BETWEEN_EVENTS 20

#define HEARTBEAT_INTERVAL  30   // in minutes: 60

#define MAX_EVENT_LIST 100
#define EVENT_PORT 5555
#define INACTIVITY_SECONDS 180

// maximum number of devices we want to support
// this number can be arbitrarily large
#define MAX_DEVICES 44

// device state
enum DEVICE_STATE { DEVICE_IDLE, DEVICE_READY, DEVICE_STREAM, DEVICE_STARTING, DEVICE_ACTIVE, DEVICE_STOP, DEVICE_DORMANT, DEVICE_ERROR, DEVICE_TERMINATING, DEVICE_MAX };

// the order here is important because this is how they are
// displayed in the devices list
enum DEVICE_PERSISTENCE { DEVICE_NONE, DEVICE_STATIC, DEVICE_DISCOVER, DEVICE_EXCLUDE };

enum RECORD_STATUS { RSTATUS_NORMAL, RSTATUS_STOPPED };

enum SEND_MODE { SEND_MODE_TCP, SEND_MODE_HTTP_GET };


// messages
#define STR_MSG_CONNECTED     "Connected to Local Network Interface:%1 port:%2"
#define STR_MSG_MANUAL        "User-triggered"
#define STR_MSG_NODB          "Database is not available"
#define STR_MSG_DBINIT        "Database is being (re)initialized"
#define STR_MSG_DONE          "Database is done"
#define STR_MSG_SHUTDOWN      "Shutting down"
#define STR_REINIT_TABLE      "Re-initializing the %1 table - current data will be lost!"
#define STR_EVENT_EXPORT      "Exporting events"

// vchannel constants
#define STR_VCHANNEL      "vchannel"
#define STR_VCHANNELID    "device"
#define STR_VCHANNELSTATE "state"
#define STR_TAG_NONE      "N"
#define STR_TAG_MOTION    "M"
#define STR_TAG_EVENT      "E"
#define SECS_BEFORE_EVENT  10
#define SECS_AFTER_EVENT   10

//commands
#define STR_SHUTDOWN      "shutdown"
#define STR_STARTSTOP     "startstop"
#define STR_DEBUGON       "debugon"
#define STR_DEBUGOFF      "debugoff"
#define STR_LOCK          "lock"
#define STR_UNLOCK        "unlock"
#define STR_EVENT         "event"
#define STR_MOTION        "motion"
#define STR_NORECORD      "norecord"
#define STR_VIDEO         "video"
#define STR_VIDEO_TIME    "time"
#define STR_VIDEO_LENGTH  "length"
#define STR_VIDEO_TYPE    "type"
#define STR_DISCOVER      "discover"
#define STR_DISCOVER_ALL  "discoverAll"

// vcamera constants
#define STR_VCAMERA      "vcamera"
#define STR_VCAMERAID    "cam"

#define STR_UPDATESCHEDULE "updateschedule"

#define DEFAULT_USERNAME "admin"
#define DEFAULT_PASSWORD "admin"

// http constants
#define STR_HTTP         "HTTP/1.1"
#define STR_HTTP0        "HTTP/1.0"

#define STR_NL           "\r\n"
#define STR_SNAPSHOT     "/snap.jpg"
#define STR_THUMBNAIL    "/thumbnail.jpg"

#define STR_STATUSREQ    "/command.cgi"

#define STR_HTTP_IMAGE   "HTTP/1.1 200 OK" STR_NL \
"Content-Type: image/jpeg" STR_NL \
"Content-Length: %1" STR_NL \
"Date: %2" STR_NL \
"Cache-Control: no-cache" STR_NL \
"Server: channel 1.0" STR_NL


#define STR_KEEPALIVE "Connection: Keep-Alive"
#define STR_VCAMERA_CGI "/vcamera.cgi"
#define STR_VCOMMAND_CGI "/vcommand.cgi"

#define STR_HTTP_200 "HTTP/1.1 200 OK" STR_NL \
    "Content-Type: text/html" STR_NL \
    "Content-Length: %1" STR_NL \
    "Date: %2" STR_NL \
    "Cache-Control: no-cache" STR_NL \
    "Server: channel 1.0" STR_NL STR_NL

#define STR_HTTP0_200 "HTTP/1.0 200 OK" STR_NL \
    "Content-Type: text/html" STR_NL \
    "Content-Length: %1" STR_NL \
    "Date: %2" STR_NL \
    "Cache-Control: no-cache" STR_NL \
    "Server: channel 1.0" STR_NL STR_NL

#define STR_CONTENT_200 "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>" \
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n" \
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">" \
    "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">" \
    "<head><title>vchannel</title></head><body><h1>event received</h1></body></html>" STR_NL STR_NL

#define STR_HTTP_404   "HTTP/1.1 404 Not Found" STR_NL \
"Content-Type: text/html" STR_NL \
"Content-Length: %1" STR_NL \
"Date: %2" STR_NL \
"Cache-Control: no-cache" STR_NL \
"Server: channel 1.0" STR_NL STR_NL

#define STR_CONTENT_404 "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>" \
"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n" \
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">" \
"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">" \
"<head><title>404 - Not Found</title></head><body><h1>404 - Not Found</h1></body></html>" STR_NL STR_NL

#define STR_HTML_REPLY "<HTML><HEAD><TITLE>channel status</TITLE><HEAD><BODY>%1</BODY></HTML>"

#endif // COMMON_H
