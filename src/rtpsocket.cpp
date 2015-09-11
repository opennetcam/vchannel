/**
 * FILE:		rtpsocket.cpp
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

#include <QLabel>
#include <QStatusBar>

#include "../include/common.h"
#include "vchannel.h"
#include "recordschedule.h"
#include "rtpsocket.h"

using namespace command_line_arguments;

extern RecordSchedule recordschedule;
extern VChannel   *vchannel;

//
// RTCP class
RtcpPacket::RtcpPacket(quint32 s, QString name): myssrc(s)
{
    QDEBUG << "RtcpPacket:" << s << name;

    for(int ii=0;ii<3;ii++)
    {
        ssrc[ii]      = 0;
        sequence[ii]  = 0;
        timestamp[ii] = 0;
        payload[ii]   = 0;
        bad_sequence[ii] = 0;
        lost[ii] = 0;
        expected[ii]=0;
    }

    // random ssrc
    myssrc = s+qrand()+(qrand()<<16);

    header[0] = (2 << 6) +       // version
                0 +            // padding
                1;             // ssrc headers
    header[1] = 0;             // packet type
    header[2] = header[3] = 0; // length

    // create cname packet
    int size = 4+2+(name.length()&0xff);
    // round up to nearest 4 byte word
    size = (size+3)/4;

    header[1] = 202;  // source description
    header[2] = size/256;
    header[3] = size&0xff;

    cname.fill( '\0',4+size*4);
    cname.replace(0,4,(const char*)header,4);
    cname.replace(4,4,(const char*)&myssrc,4);
    cname[8] = 1; // NAME
    cname[9] = name.length()&0xff;
    cname.replace(10,name.length()&0xff,(const char*)name.toLatin1());

    QDEBUG << "CNAME:" << cname;
}

//
// Receiver Report
QByteArray RtcpPacket::packetRR()
{
    QByteArray rr;

    int size = 7;

    // todo do this for all 3 RRs
    unsigned char fraction_lost = (expected[0]==0 ? 0:(256 * lost[0]) / expected[0]);

    header[0] = (2 << 6) +       // version
                0 +            // padding
                1;             // reports
    header[1] = 201 ;  // receiver report
    header[2] = size/256;
    header[3] = size&0xff;

    rr.fill('\0',4+size*4);
    rr.replace(0,4,(const char*)header,4);
    rr.replace(4,4,(const char*)&myssrc,4);     // identifier

    // SSRC group
    rr[8] = (ssrc[0] >> 24) & 0xff;
    rr[9] = (ssrc[0] >> 16) & 0xff;
    rr[10] =(ssrc[0] >> 8) & 0xff;
    rr[11] =ssrc[0] & 0xff;

    // ssrc contents
    rr[12] = fraction_lost;

    // lost packets
    if( fraction_lost ) {
        QDEBUG << "Receiver Report: fraction lost" << fraction_lost << "lost" << lost[0] << "sequence" << sequence[0];
    }
    rr[13] = (lost[0] >> 16) & 0xff;
    rr[14] = (lost[0] >> 8) & 0xff;
    rr[15] = lost[0] & 0xff;

    // highest sequence
    rr[16] = (sequence[0] >> 24) & 0xff;
    rr[17] = (sequence[0] >> 16) & 0xff;
    rr[18] = (sequence[0] >> 8) & 0xff;
    rr[19] = sequence[0] & 0xff;

    // timing and jitter not implemented
    lost[0] = lost[1] = lost[2] = 0;
    expected[0] = expected[1] = expected[2] = 0;
    return rr;
}



//
// RTP class
RtpSocket::RtpSocket(RtspSocket *parent, AvFormat *av) :
    QUdpSocket(parent),
    initialized(false), label(NULL), jpegvideo(NULL), h264video(NULL),
    pcmaudio(NULL), rtcppacket(NULL), packetSize(0), rtcpSocket(NULL),
    ipdatagram(NULL), ipsz(1500),mediaformat(-1)
{
    QDEBUG << "RtpSocket";
    quint32 uid = QUdpSocket().localAddress().toIPv4Address();
    rtcppacket = new RtcpPacket(uid,qsname);
    avformat = av;
    Q_ASSERT(avformat);
    sdp = parent->session();
}
RtpSocket::~RtpSocket()
{
    closeSocket();
    if( rtcpSocket ) rtcpSocket->close();
    if( jpegvideo ) delete jpegvideo;
    if( h264video ) delete h264video;
    if( pcmaudio ) delete pcmaudio;
    if( rtcppacket ) delete rtcppacket;
    if( ipdatagram ) free ( ipdatagram );
}

bool RtpSocket::init(int port)
{
    QDEBUG << "RtpSocket Init";
    if( initialized ) return true;
    mediaformat = sdp->video()->mediaformat();
    QDEBUG << "mediaformat=" << mediaformat << endl;
    QByteArray parms = sdp->video()->fmtp( "sprop-parameter-sets");
    if( !parms.isEmpty() )
    {
    	QDEBUG << parms << endl;
    	int pos = parms.indexOf(',');
    	if( pos > 0 )
    	{
      		sps=QByteArray::fromBase64(parms.left(pos));

      		QDEBUG << parms.left(pos) << "spslen=" << sps.count();
      		char tmp[1024];  char *t=tmp;  for(  int ii=0; ii<sps.count() && ii < (1024/3);ii++) {
					sprintf(t,"%02x ",(unsigned char)sps[ii]); t += 3; } QDEBUG << tmp << endl;

			pps=QByteArray::fromBase64(parms.mid(pos+1));
            t=tmp;
            for(  int ii=0; ii<pps.count() && ii < (1024/3);ii++) {
					sprintf(t,"%02x ",(unsigned char)pps[ii]);
					t += 3;
			}
	        QDEBUG << tmp << endl;
    	}

    }
    if( usetcp )
        // do nothing
        return true;

    if ( bind(port) )
    {
        connect(this, SIGNAL(readyRead()),this, SLOT(readPendingDatagrams()));
        if( !rtcpSocket )
            rtcpSocket = new QUdpSocket(this);
        if( rtcpSocket && rtcpSocket->bind(port+1) )
        {
            connect(rtcpSocket, SIGNAL(readyRead()),this, SLOT(readRTCPDatagrams()));
            initialized = true;
            return true;
        }
    }
    return false;
}

void RtpSocket::closeSocket()
{
    close();
    if( rtcpSocket )
        rtcpSocket->close();


    initialized = false;
}


void RtpSocket::readRTCPDatagrams()
{
    QDEBUG << "readRTCPDatagrams";
    // todo: decode the packet
}

bool even = true;
void RtpSocket::readPendingDatagrams()
{
    // for debugging
    // g_mainwindow->outputLine(QString("readPendingDatagrams"));

    int newsz = 0;
    if( ipdatagram== NULL )
    {
        QDEBUG << "allocating input buffer=" << ipsz ;
        ipdatagram = (char*)malloc(ipsz);
    }
    while ( (newsz = pendingDatagramSize()) > 0 )
    {
        // in the interests of efficiency,
        // manage this buffer directly
        if( newsz > ipsz )
        {
            QDEBUG << "resizing input buffer=" << newsz << " was:" << ipsz ;
            ipsz = newsz;
            ipdatagram = (char*)realloc(ipdatagram, ipsz);
        }
        if( ipdatagram )
        {
            qint64 datacnt = readDatagram(ipdatagram, (qint64)ipsz, &sender, &senderPort);
            decodeDatagrams(ipdatagram,datacnt);
        }
        if( !hasPendingDatagrams() )
                return;
    }
}

// interpret the data stream
void RtpSocket::decodeDatagrams(const char *datagram, qint64 datacnt)
{
    const char * data = datagram;
    if( data && datacnt > 12 )
    {
        QByteArray header = QByteArray(data,12);
        data += 12;
        datacnt -= 12;
        Q_ASSERT(datacnt >= 0 );

         // decode the RTP header
        /* RFC 3550
        The RTP header has the following format:
         0               1               2               3
         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |V=2|P|X| CC    |M|      PT     | sequence number               |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        | timestamp                                                     |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        | synchronization source (SSRC) identifier                      |
        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
        | contributing source (CSRC) identifiers                        |
        | ....                                                          |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        The first twelve octets are present in every RTP packet, while the list of CSRC identifers is present
        only when inserted by a mixer.
        */
        // not used
        // unsigned char h0 = (unsigned char)header.at(0);
        quint8 pload = (quint8)header.at(1);
        // extract the msb as the marker
        bool marker = ((pload & 0x80)==0x80);
        pload = pload & 0x7f;

        // get the latest sequence number
        quint16 seq = (quint8)header.at(2)*256 + (quint8)header.at(3);

        quint32 tstamp = ( ( ( (quint8)header.at(4))*256 + (quint8)header.at(5) )*256+
                    (quint8)header.at(6) )*256 + (quint8)header.at(7);

        quint32 ss = ( ( ( (quint8)header.at(8))*256 + (quint8)header.at(9) )*256+
                     (quint8)header.at(10) )*256 + (quint8)header.at(11);

        // QDEBUG<< "version" << (h0>>6) << " payload=" << payload << "seq=" << (uint)sequence << "ts=" << timestamp;

        // check the payload for packets we understand
        bool validpacket = false;
        switch( pload )
        {
        case 0: // pcm G.711 ulaw
// if( marker ) QDEBUG << "G.711" << seq << " " << marker;
            if( pcmaudio==NULL )
                pcmaudio = new pcmAudio(this);
            validpacket = true;
            break;

        case 26: // jpeg
            {
                if( jpegvideo==NULL )
                    jpegvideo = new jpegVideo();
                // parse RTP header for JPEG image
                int count = jpegvideo->parseRtpHeader(data, datacnt);
                if( count == 0 )
                {
                    message.clear();
                } else
                {
                    data += count;
                    datacnt -= count;
                    Q_ASSERT( datacnt >= 0 );
                    validpacket = true;
                }
            }
            break;

        default:
			{
				if( mediaformat == pload )
				{
					// QDEBUG << "H.264 payload" << pload;
					if( h264video==NULL )
					{
						h264video = new h264Video( /*sps.constData(),sps.count(),pps.constData(),pps.count()*/);
						Q_ASSERT(h264video);
			            if( h264video->extractFrame(sps.constData(), sps.count() )  )
			            {
							if( avformat )
							{
								avformat->setImageSize(h264video->width(),h264video->height());
								avformat->recordFrame((const unsigned char*)h264video->frame(), h264video->size(), STREAMS_VIDEO );
							}
			            	h264video->writeFrame(  (const char*)h264video->frame(),h264video->size() ) ;
						}
			            if( h264video->extractFrame(pps.constData(), pps.count() )  )
			            {
							if( avformat  )
							{
								avformat->setImageSize(h264video->width(),h264video->height());
								avformat->recordFrame((const unsigned char*)h264video->frame(), h264video->size(), STREAMS_VIDEO );
							}

			            	h264video->writeFrame(  (const char*)h264video->frame(),h264video->size() ) ;
						}
					}
					validpacket = true;
				}
				break;

				QDEBUG << "Unknown payload" << pload;
				break;
			}
        }

        // save values for use in the RTCP receiver record
        Q_ASSERT(rtcppacket);
        if( rtcppacket )
            for( int ii=0; ii<3; ii++ )
                if( rtcppacket->ssrc[ii] == 0 || rtcppacket->ssrc[ii] == ss )
                {
                    if( rtcppacket->ssrc[ii] == 0 )
                    {
                        rtcppacket->ssrc[ii] = ss;
                        QDEBUG << "SSRC[" << ii << "]==>" << rtcppacket->ssrc[ii];
                    }
                    rtcppacket->timestamp[ii] = tstamp;
                    rtcppacket->payload[ii] = pload;
                    // check that seq is incrementing,
                    // handle the case of wrap around
                    qint32 diff = seq - rtcppacket->sequence[ii];
                    if( validpacket && (diff > 0 || diff <= -65535) )
                    {
                        rtcppacket->sequence[ii] = seq;
                        if( diff > 0 )
                            rtcppacket->expected[ii] += diff;
                        if( diff > 1 )
						{
							if( pload == 26 ) jpegvideo->setSequence(-1);
                            rtcppacket->lost[ii] = rtcppacket->lost[ii] + diff - 1;
						}
                    } else
                    {
                        if( rtcppacket->bad_sequence[ii] < 0xFFFF ) rtcppacket->bad_sequence[ii]++;
                        // drop out of sequence packets
                        // QDEBUG << "packet dropped: " << seq;
                        return;
                    }
                    // break as we have found the record to populate
                    break;
                }
/* todo: detect missing packets in a frame and then tag the frame to be 
 *       discarded until the next start of frame.
 *       The marker is true for the last packet in a frame.
 */

        //
        // marker is suggested for start of valid video and audio after silence
        // but it should be decoded anyway
        if( pload == 26 )
        {
        	if( jpegvideo->isSequence() )
			{
                message += QByteArray(data, datacnt);
				jpegvideo->incSequence();
			}
            if( marker )
            {
                if( jpegvideo->isSequence() && jpegvideo->rtpToJfif(message) )
                {
                    int newsize = jpegvideo->count();
                    // try and eliminate corrupted packets
                    if( newsize < packetSize + 512 && newsize > packetSize - 512 )
                    {
                        if( avformat )
                        {
                            avformat->setImageSize(jpegvideo->width(),jpegvideo->height());
                            avformat->recordFrame(jpegvideo->jfif(), newsize, STREAMS_VIDEO );
                            ((RtspSocket*)parent())->updateRtpCounter();
                        }
//                        if( even )
                            displayImage(jpegvideo->jfif(), newsize, jpegvideo->width(), jpegvideo->height() );
//                        even = !even ;
                    }
                    packetSize = newsize;
                }
				jpegvideo->setSequence(0);
                message.clear();
            }
        }
        else
        // handle h264 packet
        if( pload == mediaformat )
        {

        	// extractFrame returns true when frame has been fully captured
            if( h264video->extractFrame(data, datacnt )  )
            {
            	int ret = 0;
				if( avformat  && datacnt )
				{
            		//avformat->setImageSize(h264video->width(),h264video->height());
					ret = avformat->recordFrame((const unsigned char*)h264video->frame(), h264video->size(), STREAMS_VIDEO );
				}
            	if( h264video->writeFrame(  (const char*)h264video->frame(),h264video->size() ) )
            	{
					if( h264video->gotImage() ) {
                        displayImage(h264video->imageRGB(), h264video->imageSize(), h264video->imageWidth(), h264video->imageHeight() );
					}
            	}

            	// repeat the sps and pps when we switch buffers
            	if( ret == 0 )
            	{
            		h264video->resetSync();
		            if( h264video->extractFrame(sps.constData(), sps.count() )  )
		            {
						if( avformat )
						{
							avformat->recordFrame((const unsigned char*)h264video->frame(), h264video->size(), STREAMS_VIDEO );
						}
					}
		            if( h264video->extractFrame(pps.constData(), pps.count() )  )
		            {
						if( avformat  )
						{
							avformat->recordFrame((const unsigned char*)h264video->frame(), h264video->size(), STREAMS_VIDEO );
						}
					}
            	}
				((RtspSocket*)parent())->updateRtpCounter();
			}
         }
        else
        if( pload == 0 )
        {
                message += QByteArray( data, datacnt );
            // todo
            // play the audio message
            pcmaudio->setData(message, avformat);
            ((RtspSocket*)parent())->updateRtpCounter();
            message.clear();
        }
        else
        {
            qWarning() << "Error: unable to process image data";
            message.clear();
        }
    } else
    {
        QDEBUG << "Bad packet:" << datagram;
    }
}

//
// slotSendRTCP
//
void RtpSocket::sendRtcp(QHostAddress host, int port)
{
  if( rtcppacket && !sender.isNull() )
  {
      // don't send rtcp with tcp streaming
      if( !usetcp && rtcpSocket)
          rtcpSocket->writeDatagram ( rtcppacket->packetRR()+rtcppacket->packetCNAME(), host, port );
  }
}


int cntr=0;
#ifndef _WIN32
AnalyzeJpeg rawimage1;
AnalyzeJpeg rawimage2;

int backgroundnoise = 0;
int steadystate = 0;
#endif


/*
 * detectMotion
 *
 * returns: 1 if motion detected
 * 		    0 if motiion not detected
 * 		   -1 if it is too soon since the last check
 */
int RtpSocket::detectMotion(const unsigned char *data, unsigned int size, bool isJpeg, int w, int h, int bpp )
{
	// motion detection not available for Windows
	// decode the image and check for motion by comparing 2 images 1 sec apart
	if( lastdecode < QDateTime::currentDateTime() )
	{
		bool detected = 0;

		if( data && size )
		{
#ifndef _WIN32
			if( cntr %2 )
			{
				if( isJpeg ) {
					rawimage2.analyze( data, size );
				} else {
					rawimage2.readBmp( data, size,w,h, bpp );
				}

			} else
			{
				if( isJpeg ) {
					rawimage1.analyze( data, size );
				} else {
					rawimage1.readBmp( data, size, w, h, bpp );
				}
			}

			// QDEBUG << "i1:" << rawimage1.size() << "i2:" << rawimage2.size() << "mw" << mw << "mh" << mh ;

			if( mw && mh && lastdecode.isValid() )
			{
				int deviation = 0;
				unsigned long bg = 0;

				unsigned long max = rawimage1.size();
				if( max > rawimage2.size() ) max = rawimage2.size();
				// QDEBUG << "image area=" << max << w << "x" << h;


				// get pointers to the data
				unsigned char* d1 = rawimage1.data();
				unsigned char* d2 = rawimage2.data();

				// get the size of the motion window in pixels
				max = (mw*mh*max)/10000;

				// find the pixel offsets from the y offset to the y+h offset
				for( int yy=(my*rawimage1.h())/100; yy < ((my+mh)*rawimage1.h())/100; yy++ )
				{
					// find the starting pixel of the line from the x offset
					int ls = yy*rawimage1.bytesPerLine()+(mx*rawimage1.w()*rawimage1.bytesPerPixel())/100 ;

					// scan the line, one pixel at a time until the x+w offset
					for( int xx=0; xx < (mw*rawimage1.w())/100; xx++ )
					{
						// take each pixel a byte at a time
						for( int bb = 0; bb < rawimage1.bytesPerPixel(); bb++ )
						{
							// find the byte to compare
							int ii=ls++;

							// find the distance between the bytes
							unsigned char diff = (d1[ii]>d2[ii])? d1[ii] - d2[ii] : d2[ii]-d1[ii];

							if( cntr %2 ) {
								d1[ii] = diff;
							} else {
								d2[ii] = diff; // (bb==0)?0:(bb==1)?0:(bb==2)?0:0; //diff; //
							}

							bg += diff;
							// sensitivity is the amount each pixel must change to be registered
							// hence the higher the number the less sensitive is the detection
							if( diff > ((100-sensitivity)*255)/100 + backgroundnoise )
								deviation++;
						}
					}
				}
				// for debugging
				if( debugsetting > 1 )
				{
					// write out bmp image
					if( cntr%2 ) {
						rawimage2.writeBmp( QString("/tmp/img%1-2.bmp").arg(cntr).toLatin1()  );
					} else {
						rawimage1.writeBmp( QString("/tmp/img%1-1.bmp").arg(cntr).toLatin1()  );
					}
				}
				// calculate threshold as a percentage of the image area (x * y)
				int thres = (max * threshold * threshold)/1000000;
				// record the average background noise
				backgroundnoise = (backgroundnoise + bg/max)/2;
				QDEBUG << "last=" << lastdecode << " deviation=" << deviation << " thres=" << thres << " steady=" << steadystate << " sens=" << (sensitivity*255)/100 << " noise=" << backgroundnoise ;
				if( deviation > thres + steadystate )
				{
					// motion detected
					QDEBUG << "MOTION DETECTED";
					recordschedule.setMotion();  //todo

					detected = 1;

					if( vchannel )
					{
						vchannel->statusBar()->showMessage( QString(STR_MOTION " on %1").arg(qscname) );

						QString qs = QString("<" STR_EVENT ">" STR_MOTION " on %1 </" STR_EVENT ">").arg(qscname);
						vchannel->sendEventMessage(qs);
						if( debugsetting > 1 )
						{
							// write out jpeg image
							QFile file(QString("/tmp/img%1.jpg").arg(cntr));
							if (file.open(QIODevice::WriteOnly))
							{
								QDataStream out(&file);
								// write thumbnail
								out.writeRawData( (const char *)thumbnail.constData(),thumbnail.size() );
								file.close();
							}
						}
					}
				}
				steadystate = (steadystate + deviation)/2;
			}

			if( cntr%2 )
			{
				if( rawimage2.writeJpeg() )
				{
					thumb_data = (const char*)rawimage2.jpg();
					thumb_sz = rawimage2.jpgSize();
				}
			} else
			{
				if( rawimage1.writeJpeg() )
				{
					thumb_data = (const char*)rawimage1.jpg();
					thumb_sz = rawimage1.jpgSize();
				}
			}

			++cntr;
			if( cntr >= 24 ) cntr = 0;
		}
#endif

		lastdecode = QDateTime::currentDateTime().addMSecs(500);

		return detected;
	}
	return -1;
}

void RtpSocket::displayImage(const unsigned char * data, int size, int w, int h)
{
    if( label == NULL ) return;

    bool isJpeg = false;

    // check for JFIF image
    //todo
    if( data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff && data[3] == 0xe0 &&
        data[6] == 'J' && data[7] == 'F' && data[8] == 'I' && data[9] == 'F' ) {
    	isJpeg = true;
    }

	if( isJpeg )
	{
		if( !qimg.loadFromData((const uchar*)data, size) )
			return;
	} else
	{
		// convert the imagedata to a QImage
		uint8_t *src = (uint8_t *)data;

		qimg = QImage(w, h, QImage::Format_RGB32);
		int linesize = size/h;
		for (int y = 0; y < h; y++)
		{
			QRgb *scanLine = (QRgb *) qimg.scanLine(y);
			for (int x = 0; x < w; x++)
			{
				scanLine[x] = qRgb(src[3*x], src[3*x+1], src[3*x+2]);
			}
			src += linesize;
		}
	}

	// motion detection is not available for Windows
	// decode the image and check for motion by comparing 2 images 1 sec apart
	if( isJpeg ) {
		if( detectMotion(data, size, true, w, h, 0 ) == 1 ) {
			recordschedule.setMotion();
		}
	} else {
		QImage image = qimg.convertToFormat(QImage::Format_RGB888).scaledToHeight(240);
		if( detectMotion(image.constBits(), image.byteCount (), false, image.width(), image.height(), image.bitPlaneCount() ) == 1 ) {
			recordschedule.setMotion();
		}
	}

#ifdef _WIN32
	QPixmap pixmap = QPixmap::fromImage(qimg);
	if( !pixmap.isNull() && pixmap.width() == w && pixmap.height() == h )
	{
		// create a thumbnail
		QBuffer buffer(&thumbnail);
		buffer.open(QIODevice::WriteOnly);
		QPixmap pm = pixmap.scaledToHeight(240);
		pm.save(&buffer, "JPG");
		if( label->height() > 1 )
		{
			if( label->width() < w-16 )
				pixmap = pixmap.scaledToWidth(label->width());
			label->setPixmap(pixmap);
		}
	}
#else
	// do not display in the minimized state
	if( label->height() > 1 )
	{
		QPixmap pixmap = QPixmap::fromImage(qimg);
		if( !pixmap.isNull() )
		{
			if( label->width() < w-16 )
				pixmap = pixmap.scaledToWidth(label->width());

			label->setPixmap(pixmap);
		}
	}
#endif
}
