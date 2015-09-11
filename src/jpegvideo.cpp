/**
 * FILE:		jpegvideo.cpp
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

#include <QDebug>

#include <setjmp.h>

#include "../include/common.h"
#include "vchannel.h"
#include "jpegvideo.h"

using namespace command_line_arguments;

// following parts are copied from: http://www.live555.com/liveMedia/public/
// "liveMedia"
// Copyright (c) 1996-2010 Live Networks, Inc.  All rights reserved.
// JPEG Video (RFC 2435) RTP Sources

#define BYTE unsigned char

enum {
    MARKER_SOF0	= 0xc0,		// start-of-frame, baseline scan
    MARKER_SOI	= 0xd8,		// start of image
    MARKER_EOI	= 0xd9,		// end of image
    MARKER_SOS	= 0xda,		// start of scan
    MARKER_DRI	= 0xdd,		// restart interval
    MARKER_DQT	= 0xdb,		// define quantization tables
    MARKER_DHT  = 0xc4,		// huffman tables
    MARKER_APP_FIRST	= 0xe0,
    MARKER_APP_LAST		= 0xef,
    MARKER_COMMENT		= 0xfe,
};

static unsigned char const lum_dc_codelens[] = {
  0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
};

static unsigned char const lum_dc_symbols[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static unsigned char const lum_ac_codelens[] = {
  0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d,
};

static unsigned char const lum_ac_symbols[] = {
  0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
  0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
  0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
  0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
  0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
  0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
  0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
  0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
  0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
  0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa,
};

static unsigned char const chm_dc_codelens[] = {
  0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};

static unsigned char const chm_dc_symbols[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static unsigned char const chm_ac_codelens[] = {
  0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77,
};

static unsigned char const chm_ac_symbols[] = {
  0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
  0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
  0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
  0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
  0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
  0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
  0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
  0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
  0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
  0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa,
};

static void createHuffmanHeader(unsigned char*& p,
                unsigned char const* codelens,
                int ncodes,
                unsigned char const* symbols,
                int nsymbols,
                int tableNo, int tableClass) {
  *p++ = 0xff; *p++ = MARKER_DHT;
  *p++ = 0;               /* length msb */
  *p++ = 3 + ncodes + nsymbols; /* length lsb */
  *p++ = (tableClass << 4) | tableNo;
  memcpy(p, codelens, ncodes);
  p += ncodes;
  memcpy(p, symbols, nsymbols);
  p += nsymbols;
}

static unsigned computeJPEGHeaderSize(unsigned qtlen, unsigned dri) {
  unsigned qtlen_half = qtlen/2; // in case qtlen is odd; shouldn't happen
  return 495 + qtlen_half*2 + (dri > 0 ? 6 : 0);
}

static int createJPEGHeader(char* buf, unsigned type,
                 unsigned w, unsigned h,
                 const char* qtables, unsigned qtlen,
                 unsigned char dri)
{
  unsigned char *ptr = (unsigned char*)buf;
  unsigned numQtables = qtlen > 64 ? 2 : 1;

  // MARKER_SOI:
  *ptr++ = 0xFF; *ptr++ = MARKER_SOI;

  // MARKER_APP_FIRST:
  *ptr++ = 0xFF; *ptr++ = MARKER_APP_FIRST;
  *ptr++ = 0x00; *ptr++ = 0x10; // size of chunk
  *ptr++ = 'J'; *ptr++ = 'F'; *ptr++ = 'I'; *ptr++ = 'F'; *ptr++ = 0x00;
  *ptr++ = 0x01; *ptr++ = 0x01; // JFIF format version (1.1)
  *ptr++ = 0x00; // no units
  *ptr++ = 0x00; *ptr++ = 0x01; // Horizontal pixel aspect ratio
  *ptr++ = 0x00; *ptr++ = 0x01; // Vertical pixel aspect ratio
  *ptr++ = 0x00; *ptr++ = 0x00; // no thumbnail

  // MARKER_DRI:
  if (dri > 0) {
    *ptr++ = 0xFF; *ptr++ = MARKER_DRI;
    *ptr++ = 0x00; *ptr++ = 0x04; // size of chunk
    *ptr++ = (BYTE)(dri >> 8); *ptr++ = (BYTE)(dri); // restart interval
  }

  // MARKER_DQT (luma):
  unsigned tableSize = numQtables == 1 ? qtlen : qtlen/2;
  *ptr++ = 0xFF; *ptr++ = MARKER_DQT;
  *ptr++ = 0x00; *ptr++ = tableSize + 3; // size of chunk
  *ptr++ = 0x00; // precision(0), table id(0)
  memcpy(ptr, qtables, tableSize);
  qtables += tableSize;
  ptr += tableSize;

  if (numQtables > 1) {
    unsigned tableSize = qtlen - qtlen/2;
    // MARKER_DQT (chroma):
    *ptr++ = 0xFF; *ptr++ = MARKER_DQT;
    *ptr++ = 0x00; *ptr++ = tableSize + 3; // size of chunk
    *ptr++ = 0x01; // precision(0), table id(1)
    memcpy(ptr, qtables, tableSize);
    qtables += tableSize;
    ptr += tableSize;
  }

  // MARKER_SOF0:
  *ptr++ = 0xFF; *ptr++ = MARKER_SOF0;
  *ptr++ = 0x00; *ptr++ = 0x11; // size of chunk
  *ptr++ = 0x08; // sample precision
  *ptr++ = (BYTE)(h >> 8);
  *ptr++ = (BYTE)(h); // number of lines (must be a multiple of 8)
  *ptr++ = (BYTE)(w >> 8);
  *ptr++ = (BYTE)(w); // number of columns (must be a multiple of 8)
  *ptr++ = 0x03; // number of components
  *ptr++ = 0x01; // id of component
  *ptr++ = type ? 0x22 : 0x21; // sampling ratio (h,v)
  *ptr++ = 0x00; // quant table id
  *ptr++ = 0x02; // id of component
  *ptr++ = 0x11; // sampling ratio (h,v)
  *ptr++ = numQtables == 1 ? 0x00 : 0x01; // quant table id
  *ptr++ = 0x03; // id of component
  *ptr++ = 0x11; // sampling ratio (h,v)
  *ptr++ = 0x01; // quant table id

  createHuffmanHeader(ptr, lum_dc_codelens, sizeof lum_dc_codelens,
              lum_dc_symbols, sizeof lum_dc_symbols, 0, 0);
  createHuffmanHeader(ptr, lum_ac_codelens, sizeof lum_ac_codelens,
              lum_ac_symbols, sizeof lum_ac_symbols, 0, 1);
  createHuffmanHeader(ptr, chm_dc_codelens, sizeof chm_dc_codelens,
              chm_dc_symbols, sizeof chm_dc_symbols, 1, 0);
  createHuffmanHeader(ptr, chm_ac_codelens, sizeof chm_ac_codelens,
              chm_ac_symbols, sizeof chm_ac_symbols, 1, 1);

  // MARKER_SOS:
  *ptr++ = 0xFF;  *ptr++ = MARKER_SOS;
  *ptr++ = 0x00; *ptr++ = 0x0C; // size of chunk
  *ptr++ = 0x03; // number of components
  *ptr++ = 0x01; // id of component
  *ptr++ = 0x00; // huffman table id (DC, AC)
  *ptr++ = 0x02; // id of component
  *ptr++ = 0x11; // huffman table id (DC, AC)
  *ptr++ = 0x03; // id of component
  *ptr++ = 0x11; // huffman table id (DC, AC)
  *ptr++ = 0x00; // start of spectral
  *ptr++ = 0x3F; // end of spectral
  *ptr++ = 0x00; // successive approximation bit position (high, low)

  // size of the data is returned
  return (ptr-(unsigned char*)buf);
}

// The default 'luma' and 'chroma' quantizer tables, in zigzag order:
static unsigned char const defaultQuantizers[128] = {
  // luma table:
  16, 11, 12, 14, 12, 10, 16, 14,
  13, 14, 18, 17, 16, 19, 24, 40,
  26, 24, 22, 22, 24, 49, 35, 37,
  29, 40, 58, 51, 61, 60, 57, 51,
  56, 55, 64, 72, 92, 78, 64, 68,
  87, 69, 55, 56, 80, 109, 81, 87,
  95, 98, 103, 104, 103, 62, 77, 113,
  121, 112, 100, 120, 92, 101, 103, 99,
  // chroma table:
  17, 18, 18, 24, 21, 24, 47, 26,
  26, 47, 99, 66, 56, 66, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99,
  99, 99, 99, 99, 99, 99, 99, 99
};

// end copy

jpegVideo::jpegVideo():
            type(0), fragmentoffset(0xfff),firstoffset(0),
            q(0), _width(0),_height(0), dri(0),
            numQtables(0), qthlen(0),_jfif(NULL),size(0),_jfifsize(0),
			pktsequence(0)
{
    QDEBUG << "jpegVideo";
}

jpegVideo::~jpegVideo()
{
    QDEBUG << "~jpegVideo";
    if( _jfif ) free(_jfif);
    _jfif = NULL;
}

int  jpegVideo::parseRtpHeader(const char *hdr, int size)
{
    if( !hdr ) return 0;
    if( size <= 0 ) return 0;

    QByteArray header = QByteArray(hdr,size);

    // keep count of where we are
    int cnt = 0;

    /* RFC 2435
    Each packet contains a special JPEG header which immediately follows
    the RTP header.  The first 8 bytes of this header, called the "main
    JPEG header", are as follows:

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | Type-specific |              Fragment Offset                  |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      Type     |       Q       |     Width     |     Height    |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
    // analyze the main header
    cnt++;
     unsigned int newoffset = ( ( (quint8)header.at(cnt) )*256+(quint8)header.at(cnt+1) )*256 + (quint8)header.at(cnt+2);
     cnt+=3;
     if( newoffset == 0 )
     {
        fragmentoffset = 0;
     } else
     if( newoffset > 0 && fragmentoffset == 0 )
     {
         fragmentoffset = newoffset;
         firstoffset = newoffset + 200;
     } else
     if( newoffset > fragmentoffset && (newoffset - fragmentoffset < firstoffset) )
        fragmentoffset = newoffset;
     else
     {
         if ( fragmentoffset != 0x0fff )
             QDEBUG << newoffset << " vs " << fragmentoffset << "/" << firstoffset << " Fragment offset error -> data loss";
         fragmentoffset = 0x0fff; // set to maximum value so that we wait for the next packet
         return 0;
     }
    type = (quint8)header.at(cnt++);
    q =    (quint8)header.at(cnt++);
    _width =(quint8)header.at(cnt++)*8;  // 8-pixel multiples
    _height =(quint8)header.at(cnt++)*8; // 8-pixel multiples
    // for debugging
    // QDEBUG << "frag=" << fragmentoffset << " type=" << type << " Q=" << q << " w=" << _width << " h=" << _height;
    dri = 0;
    numQtables=0;

    // check for restart marker header
    // marker appears on every header
    if( type >= 64 && type < 128 )
    {
        // extract the restart marker header

        /* RFC 2435
            0                   1                   2                   3
            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           |       Restart Interval        |F|L|       Restart Count       |
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           The Restart Interval field specifies the number of MCUs that appear
           between restart markers.  It is identical to the 16 bit value that
           would appear in the DRI marker segment of a JFIF header.  This value
           MUST NOT be zero.

           If the restart intervals in a frame are not guaranteed to be aligned
           with packet boundaries, the F (first) and L (last) bits MUST be set
           to 1 and the Restart Count MUST be set to 0x3FFF.  This indicates
           that a receiver MUST reassemble the entire frame before decoding it.
        */
        dri = (quint8)header.at(cnt)*256 + (quint8)header.at(cnt+1);
        cnt+=2;
        // ignore the Restart Count (for now)
#ifndef QT_NO_DEBUG
        //int f_flag = (header.at(cnt)& 0x80) >> 7;
        //int l_flag = (header.at(cnt) & 0x40) >> 6;
        //int restart = ((quint8)header.at(cnt) & 0x3f)*256 + (quint8)header.at(cnt+1);
        //QDEBUG << "dri=" << dri << "F=" << f_flag << "L=" << l_flag << "restart" << restart;
#endif
        cnt+=2;
    }
    // check for quantization header
    // this must come after any possible restart marker header
    if( q >= 128 && fragmentoffset==0 )
    {
        /* quantization table header

            0                   1                   2                   3
            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           |      MBZ      |   Precision   |             Length            |
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
           |                    Quantization Table Data                    |
           |                              ...                              |
           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */
        //quint8 mbz = (quint8)header.at(cnt);             // not used
        cnt++;
        //quint8 precision = (quint8)header.at(cnt);       // not used
        cnt++;

        qthlen = ((quint8)header.at(cnt))*256 + (quint8)header.at(cnt+1);
        cnt+=2;
#ifndef QT_NO_DEBUG
        // for debugging
        // QDEBUG << "mbz=" << mbz << " precision=" << precision << " qth length=" << qthlen;
#endif
        qth = header.mid(cnt,qthlen);
        cnt += qthlen;
        switch(type)
        {
        case 0: case 64:
        case 1: case 65:
            numQtables = 2;
            break;
        default:
            numQtables = -1; // undefined
        }
#ifndef QT_NO_DEBUG
        // for debugging
        //QDEBUG << "numQtables=" << numQtables << " dri=" << dri ;
#endif
    }
    return cnt;
}

bool jpegVideo::rtpToJfif(QByteArray &jpg)
{
    if( jpg.length() > 0 )
    {
        int headersize = computeJPEGHeaderSize(qthlen,dri);
        if( _jfif == NULL )
        {
            _jfifsize = headersize + jpg.count()+2+10000;
            QDEBUG << "malloc jfif:" << _jfifsize;
            _jfif = (unsigned char*)malloc(_jfifsize);
        }
        if( _jfif )
        {
            if( _jfifsize < (headersize + jpg.count()+2) )
            {
                _jfifsize = headersize + jpg.count()+2+10000;
                QDEBUG << "free/malloc jfif:" << _jfifsize;
                free(_jfif);
                _jfif = (unsigned char*)malloc(_jfifsize);
            }
            size = createJPEGHeader((char*)_jfif, type,
                             _width, _height,
                             qth.constData(), qthlen,
                             dri);
            memcpy( _jfif+size, jpg.constData(), jpg.count() );
            size += jpg.count();

//            // MARKER_COMMENT
//            // make space for a 10-byte comment
//            _jfif[size++] = 0xff;
//            _jfif[size++] = MARKER_COMMENT;
//            _jfif[size++] = 0;
//            _jfif[size++] = 12;
//            for(int ii=0; ii< 12-2; ii++)
//                _jfif[size++] = 'x';
            _jfif[size++] = 0xff;
            _jfif[size++] = MARKER_EOI;
            return true;
        }
        return false;
    }
    return false;
}

// motion detection is not available in Windows
#ifndef _WIN32
/*
 *  JPEG error routing to be able to return to code
 */
struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void) my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


/* provide a class to analyze the jpeg image
 *
 */

AnalyzeJpeg::AnalyzeJpeg() :
		raw_image(NULL), raw_image_size(0),
		jpeg_image(NULL), jpeg_size(0),
		bpl(0), blockSize(0), height(0),width(0),bppx(1),col_space(JCS_RGB)
{

}

AnalyzeJpeg::~AnalyzeJpeg()
{
    if( raw_image ) free( raw_image );
    if( jpeg_size ) free( jpeg_image );
}

bool AnalyzeJpeg::analyze(const unsigned char * jfif, unsigned long jsize )
{
    if( jfif == NULL ) return false;
    if( jsize == 0 ) return false;

    unsigned long location = 0;

    /* these are standard libjpeg structures for reading(decompression) */
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;

    /* libjpeg data structure for storing one row, that is, scanline of an image */
    JSAMPROW row_pointer[1];
    row_pointer[0] = NULL;

    /* here we set up the standard libjpeg error handler */
    cinfo.err = jpeg_std_error( &jerr.pub );
    jerr.pub.error_exit = my_error_exit;
    /* Establish the setjmp return context for my_error_exit to use. */
    if (::setjmp(jerr.setjmp_buffer))
    {
        /* If we get here, the JPEG code has signaled an error.
         * We need to clean up the JPEG object, close the input file, and return.
         */
        jpeg_destroy_decompress(&cinfo);
        if( row_pointer[0] )
            free( row_pointer[0] );
        return false;
    }

    /* setup decompression process and source, then read JPEG header */
    jpeg_create_decompress( &cinfo );

    /* this makes the library read from infile */
    jpeg_mem_src( &cinfo, (unsigned char*)jfif, jsize );

    /* reading the image header which contains image information */
    jpeg_read_header( &cinfo, TRUE );

    width = cinfo.image_width;
    height = cinfo.image_height;

    // set the desired output parameters
    // set the height to 240 px and grayscale images
    cinfo.scale_num = (height < (8*DEFAULT_WIDTH)) ? (8*DEFAULT_WIDTH)/height : 1;
    cinfo.scale_denom = 8;
    cinfo.out_color_space = JCS_RGB; // JCS_GRAYSCALE;  // JCS_RGB; //
    cinfo.do_fancy_upsampling = false;

//    QDEBUG << "JPEG File Information: ";
//    QDEBUG << "Image width and height:" << width << " pixels and " << height << " pixels.";

    /* Start decompression jpeg here */
    jpeg_start_decompress( &cinfo );

//    QDEBUG << "Output Image:" << cinfo.output_width << " x " << cinfo.output_height << " components: " << cinfo.num_components;

    /* allocate memory to hold the uncompressed image */
    if( raw_image == NULL )
    {
    	raw_image_size = width*height*cinfo.num_components;
    	Q_ASSERT(raw_image_size>0);
        raw_image = (unsigned char*)malloc( raw_image_size );
    }

    /* now actually read the jpeg into the raw buffer */
    row_pointer[0] = (unsigned char *)malloc( width*cinfo.num_components );

    /* read one scan line at a time */
    while( cinfo.output_scanline < cinfo.output_height )
    {
        jpeg_read_scanlines( &cinfo, row_pointer, 1 );
        for( unsigned int i=0; i<cinfo.output_width*cinfo.output_components;i++)
            raw_image[location++] = row_pointer[0][i];
    }

    bpl = cinfo.output_width * cinfo.output_components;
    /* round up to a dword boundary */
    if( cinfo.output_components > 1 && (bpl & cinfo.output_components) )
    {
        bpl |= cinfo.output_components;
        ++bpl;
    }
    blockSize=(long)bpl*cinfo.output_height;
    height = cinfo.output_height;
    width = cinfo.output_width;
    bppx = cinfo.out_color_components;
    col_space = cinfo.out_color_space;
    //for debugging
    if( debugsetting > 1 )
    {
        QDEBUG << "Color components per pixel: " << cinfo.out_color_components;
        QDEBUG << "Color space: " << cinfo.out_color_space;
        QDEBUG << "bytes per line" << bpl << " x " << height << " lines";
        QDEBUG << "block size=" << blockSize;
        QDEBUG << "components=" << cinfo.num_components << "o/p=" << cinfo.output_components;
        QDEBUG << "scan number=" << cinfo.output_scan_number;
        QDEBUG << "height=" << cinfo.output_height;
    }
    /* clean up */
    jpeg_finish_decompress( &cinfo );
    jpeg_destroy_decompress( &cinfo );
    free( row_pointer[0] );

    return true;
}

bool AnalyzeJpeg::readBmp(const unsigned char * bmp, unsigned long size, int w, int h, int bpp)
{
	if( bmp && size )
	{
		if( raw_image == NULL )
		{
			raw_image_size = size;
			raw_image = (unsigned char*)malloc( raw_image_size );
		} else
		if( raw_image_size < size )
		{
			raw_image_size = size;
			raw_image = (unsigned char*)realloc( raw_image, raw_image_size );
		}
		if( raw_image )
		{
			memcpy(raw_image, bmp, raw_image_size);

			width = w;
			height = h;
			bppx = bpp/8;
			col_space = JCS_RGB;
			blockSize = w * h ;
			bpl = bppx*w;
			return true;
		}
	}
	return false;
}

bool AnalyzeJpeg::writeBmp(const char * filename )
{
    BMPHEAD bh;
    int bytes_per_pixel = bppx;
    QDEBUG << width << "x" << height << "bpp=" << bppx;
    memset ((char *)&bh,0,sizeof(BMPHEAD));

    //bh.filesize  =   calculated size of file
    //bh.reserved  = two zero bytes
    bh.headersize  = sizeof(BMPHEAD)+4;
    bh.infoSize  =  0x28L;
    bh.width     = width ;
    bh.depth     = height;
    bh.biPlanes  = 1;
    bh.bits      = 8*bytes_per_pixel ;
    bh.biCompression = 0L;   // no compression


    int bytesPerLine = width * bytes_per_pixel;
    /* round up to a dword boundary */
    if (bppx > 1 && bytesPerLine & 0x0003)
    {
       bytesPerLine |= 0x0003;
       ++bytesPerLine;
    }

    bh.filesize=bh.headersize+(long)bytesPerLine*bh.depth;

    QDEBUG << "writeBmp : " << bytesPerLine << " w=" << width << " h=" << height << "bpp=" << bppx;

    FILE *bmpfile = fopen(filename, "wb");
    if (bmpfile == NULL)
    {
       QDEBUG << "Error opening output file" << filename;
      return false;
    }
    fwrite("BM",1,2,bmpfile);
    fwrite((char *)&bh, 1, sizeof (bh), bmpfile);

    char *linebuf;

    linebuf = (char *) calloc(1, bytesPerLine);
    if (linebuf == NULL)
    {
        qWarning() << "WriteBmp: Error allocating memory";
        return false;
    }

    for (int line = height-1; line >= 0; line --)
    {
       /* fill line linebuf with the image data for that line */
        for(int x =0 ; x < width; x++ )
        {
            for( int b = 0; b < bytes_per_pixel; b++ )
            {
                // switch the order of RGB
                *( linebuf+x*bytes_per_pixel+b ) = *(raw_image + (x+line*width)*bytes_per_pixel+ (b==0?1:b==1?0:2) );
            }
        }

       fwrite(linebuf, 1, bytesPerLine, bmpfile);
    }
    free(linebuf);
    fclose(bmpfile);

    return true;
}
/*
 * compress the raw image to a jpeg thumbnail (240 pixel high)
 * Returns a pointer to the thumbnail image or NULL
 */
bool AnalyzeJpeg::writeJpeg()
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    /* this is a pointer to one row of image data */
    JSAMPROW row_pointer[1];


    ///// FILE *outfile = fopen( filename, "wb" );

    cinfo.err = jpeg_std_error( &jerr );
    jpeg_create_compress(&cinfo);

    //// jpeg_stdio_dest(&cinfo, outfile);

    if( jpeg_size ) {
        free( jpeg_image );
        jpeg_size = 0;
    }

    jpeg_mem_dest( &cinfo, &jpeg_image, &jpeg_size );

    /* Setting the parameters of the output file here */
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = bppx;  // bytes_per_pixel;
    cinfo.in_color_space = col_space;
    /* default compression parameters, we shouldn't be worried about these */
    jpeg_set_defaults( &cinfo );

    QDEBUG << "";
    /* Now do the compression .. */
    jpeg_start_compress( &cinfo, TRUE );
    /* like reading a file, this time write one row at a time */
    while( cinfo.next_scanline < cinfo.image_height )
    {
        row_pointer[0] = &raw_image[ cinfo.next_scanline * cinfo.image_width *  cinfo.input_components];
        jpeg_write_scanlines( &cinfo, row_pointer, 1 );
    }
    /* similar to read file, clean up after we're done compressing */
    jpeg_finish_compress( &cinfo );
    jpeg_destroy_compress( &cinfo );

    /* success code is 1! */
    return true;
}

#endif
