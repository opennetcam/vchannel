#-------------------------------------------------
#
# Project created by QtCreator
#
#-------------------------------------------------

QT       += network core gui
LIBS += -L/usr/lib -ljpeg \
        -L/usr/lib -lavformat \
        -L/usr/lib -lavcodec \
        -L/usr/lib -lswscale \
        -L/usr/lib -lavutil

TARGET = ../bin/vchannel-d
TEMPLATE = app
CONFIG += debug

SOURCES += main.cpp\
        vchannel.cpp \
    rtspsocket.cpp \
    rtpsocket.cpp \
    pcmaudio.cpp \
    avformat.cpp \
    channelformat.cpp \
    aviformat.cpp \
    authentication.cpp \
    sessiondescription.cpp \
    recordschedule.cpp \
    jpegvideo.cpp \
    h264video.cpp

HEADERS  += vchannel.h \
    rtspsocket.h \
    rtpsocket.h \
    pcmaudio.h \
    channelformat.h \
    avformat.h \
    aviformat.h \
    avifmt.h \
    authentication.h \
    sessiondescription.h \
    recordschedule.h \
    jpegvideo.h \
    h264video.h \
    ../include/common.h

FORMS    += vchannel.ui \
    authdialog.ui

OTHER_FILES += \
    ../man/manpage.txt

RESOURCES += \
    resources.qrc













