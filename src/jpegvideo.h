/**
 * FILE:		jpegvideo.h
 *
 * DESCRIPTION:
 * This is the class for implementing jpeg conversions for RTP
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


#ifndef JPEGVIDEO_H
#define JPEGVIDEO_H

#ifndef _WIN32
#include "jpeglib.h"
#endif

#include <QByteArray>

class jpegVideo
{
public:
    jpegVideo();
    ~jpegVideo();
    bool rtpToJfif(QByteArray &jpg);
    int  parseRtpHeader(const char *header, int size );
    unsigned char * jfif() { return _jfif; }
    int count() { return size; }
    int width() { return (int)_width;}
    int height() { return (int)_height;}
        void setSequence(int s) { pktsequence = s; }
	bool isSequence() { if (pktsequence != -1) return true; return false; }
        void incSequence() { if (pktsequence != -1) pktsequence++; }

private:
    quint8  type;
    quint32 fragmentoffset;
    quint32 firstoffset;
    quint8  q;
    quint16  _width;
    quint16  _height;
    quint16 dri;
    int numQtables;
    quint16 qthlen;
    QByteArray qth;
    unsigned char* _jfif;
    int size;
    int _jfifsize;
	int pktsequence;
};

/* provide a class to analyze the jpeg image
 *
 */
typedef struct {
     int32_t filesize;
     char reserved[2];
     int32_t headersize;
     int32_t infoSize;
     int32_t width;
     int32_t depth;
     int16_t biPlanes;
     int16_t bits;
     int32_t biCompression;
     int32_t biSizeImage;
     int32_t biXPelsPerMeter;
     int32_t biYPelsPerMeter;
     int32_t biClrUsed;
     int32_t biClrImportant;
} BMPHEAD;

#ifndef _WIN32
class AnalyzeJpeg
{
public:
    AnalyzeJpeg();
    ~AnalyzeJpeg();
    bool analyze(const unsigned char * jfif, unsigned long jsize);
    bool readBmp(const unsigned char * bmp, unsigned long size, int w, int h, int bpp );
    bool writeBmp(const char * filename );
    bool writeJpeg();
    unsigned char *  data() { return raw_image; }
    unsigned long datasize() { return raw_image_size; }
    unsigned long size() { return blockSize; }
    int bytesPerLine() { return bpl; }
    int bytesPerPixel() { return bppx; }
    int h() { return height; }
    int w() { return width; }
    const unsigned char *jpg() { return jpeg_image; }
    int jpgSize() { return jpeg_size; }

private:
    unsigned char *raw_image;
    unsigned long raw_image_size;
    unsigned char *jpeg_image;
    unsigned long jpeg_size;
    int bpl;
    unsigned long blockSize;
    int height;
    int width;
    int bppx;
    J_COLOR_SPACE col_space;
};
#endif

#endif // JPEGVIDEO_H
