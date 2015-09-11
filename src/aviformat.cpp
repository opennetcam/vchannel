/**
 * aviformat.cpp
 *
 * DESCRIPTION:
 * This is the class for writing and reading AVI format.
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

#include <QtDebug>
#include <QDir>
#include <QStatusBar>
#include <QTimer>
#include <QLabel>

#include "vchannel.h"
#include "avifmt.h"
#include "aviformat.h"
#include "recordschedule.h"

using namespace command_line_arguments;

extern QStatusBar *statusbar;
extern VChannel   *vchannel;
extern QString globalstatus;
extern RecordSchedule recordschedule;

/*
 * AviChannelFormat
 */

AviChannelFormat::AviChannelFormat(): ChannelFormat(".avi"),
        maxRiffSize(2147483648LL),
        jpgSize(0)
{
    QDEBUG << __FUNCTION__;
}

AviChannelFormat::~AviChannelFormat()
{
    QDEBUG << __FUNCTION__;
}

int AviChannelFormat::recordFrame(const unsigned char *frame, int size, STREAMS stream, bool writeon )
{
    int ret = 1;
    Q_ASSERT(frame);
    if( frame==NULL ) return -1;
    Q_ASSERT(size);
    if( size==0 ) return -1;

    // start the timer at the first frame
    if( frames==0 && samples==0 )
    {
        timer.start();
        QDEBUG << "timer start";
    }

    QByteArray *f = new QByteArray((const char*)frame,size);
    if(f)
    {
        if( stream == STREAMS_VIDEO )
        {
            // round up to 4 bytes
            int tmp = ((4-(size%4)) % 4);
            while(tmp--) f->append('\0');
            jpgSize += f->length();

            videoListMarker.append( dataList.count() );
            dataList.append(f);
            frames++;

            if( framecount == 0 )
            {
                long ee = timer.elapsed() - elapsed;
                if( ee > 0  )
                {
                    int fps = 1000000/ee;
                    framecount = 100;
                    elapsed = timer.elapsed();
					globalstatus = QString("%1.%2 fps %3 MJPEG").arg(fps/10).arg(fps%10).arg(usetcp?"T":"U");
                    if( statusbar && fps )
                        statusbar->showMessage(globalstatus);
                }
            } else
                framecount--;


        } else
        if( stream == STREAMS_AUDIO )
        {
            audioListMarker.append( dataList.count() );
            dataList.append(f);
            samples+= f->length();
        }
        else
        {
            //undefined
            Q_ASSERT(0);
        }
        if( timer.elapsed() > (writeon?RECORD_FILETIME_WRITEON:RECORD_FILETIME_NOWRITE) )
        {
            ret = 0; // SWITCH_CHANNELS
        }
    } else
    {
        QDEBUG << "unable to allocate buffer";
    }

    return ret;
}


bool AviChannelFormat::writeAv(QString filename, STREAMS streams, QDateTime &, qint64  )
{
    QDEBUG << "WriteAvi: " <<filename;
    int size=0;  // for debugging
    quint32 riffSize = 0;

    if ( frames == 0 ) return false;

    int ms = timer.elapsed()+300;   // time elapsed in milliseconds
    int buffers = dataList.count();
    int us_per_frame = (1000*ms)/frames;

    if( streams == STREAMS_AV ) // todo: handle other cases
        riffSize = sizeof(AVI_list_hdrl) + 4 + sizeof(AVI_list_strl_a) + 4 + jpgSize + samples + buffers*24+16 ;  //todo
    else
        riffSize = sizeof(AVI_list_hdrl) + 4 + 4 + jpgSize +  buffers*24+16;  //todo

    if( riffSize >= maxRiffSize )
    {
        qWarning() << "File size exceeds maximum allowed: " << riffSize;
        return false;
    }

    QFile fout(filename);
    if ( fout.open( QIODevice::WriteOnly ) )
    {
         QDataStream out(&fout);   // we will serialize the data into the file

         // AVI RIFF header
         out.writeRawData(TAG_RIFF,sizeof(TAG_RIFF));  
         out << LI4(riffSize);          // todo - ensure this writes 4 bytes
         qDebug("RIFF %d",riffSize);

         out.writeRawData(TAG_AVI,sizeof(TAG_AVI));

         // list hdrl
		 out.writeRawData(TAG_LIST,sizeof(TAG_LIST));

		 if( streams==STREAMS_AV )
			 size = sizeof(AVI_list_hdrl) + sizeof(struct AVI_list_strl_a) - 8;
		 else
			size = sizeof(AVI_list_hdrl) - 8;
		 out << (LI4(size));
		 out.writeRawData(TAG_hdrl,sizeof(TAG_hdrl));

		 // chunk avih
		 out.writeRawData(TAG_avih,sizeof(TAG_avih));
		 struct AVI_avih avih;
		 avih.us_per_frame      = us_per_frame;   // uSec per frame
		 avih.max_bytes_per_sec = (1000*jpgSize)/ms;  // bytes per second
		 avih.padding           = 0;
		 avih.flags             = AVIF_HASINDEX | AVIF_WASCAPTUREFILE | AVIF_ISINTERLEAVED;
		 avih.tot_frames        = buffers;
		 avih.init_frames       = 0;
		 avih.streams           = (streams==STREAMS_AV?2:1);        // for audio as well change to 2
		 avih.buff_sz           = 100000;
		 avih.width             = width;
		 avih.height            = height;
		 avih.TimeScale         = 0;      // max 30 fps
		 avih.DataRate          = 0;
		 avih.StartTime         = 0;
		 avih.DataLength        = 0;
		 out << LI4(sizeof(avih));

		 out.writeRawData((const char*)&avih, sizeof(avih) );   // output includes the length

		 // video stream
		 if( streams == STREAMS_VIDEO || streams == STREAMS_AV )
		 {
			// list strl
			out.writeRawData(TAG_LIST,sizeof(TAG_LIST));
			out << LI4(sizeof(AVI_list_strl) - 8);
			out.writeRawData(TAG_strl,sizeof(TAG_strl));

			// chunk strh
			out.writeRawData(TAG_strh,sizeof(TAG_strh));
			struct AVI_strh strh;
			strh.type[0]='v';strh.type[1]='i';strh.type[2]='d';strh.type[3]='s';      /* stream type */
			strh.handler[0]='m';strh.handler[1]='j';strh.handler[2]='p';strh.handler[3]='g';
			strh.flags=0;
			strh.priority=0;
			//strh.language=0;
			strh.init_frames=0; // todo   us_per_frame;       /* initial frames (???) */
			strh.scale=16;
			strh.rate=strh.scale* (frames*1000)/ms;
			QDEBUG << "rate=" <<strh.rate;

			strh.start=0;
			strh.length=frames;
			strh.buff_sz=0;           /* suggested buffer size */
			strh.quality=0;
			strh.sample_sz=0;
			strh.rect = (width<<16)+height;
			out << LI4(sizeof(AVI_strh));
			out.writeRawData((const char*)&strh, sizeof(strh) );   // output includes the length
			//qDebug("strh %d",(int)sizeof(AVI_strh));
			// chunk strf
			out.writeRawData(TAG_strf,sizeof(TAG_strf));
			out << LI4(sizeof(BITMAPINFOHEADER));
			BITMAPINFOHEADER strf;
			strf.biSize=sizeof(BITMAPINFOHEADER);   // todo verify
			strf.biWidth=width;
			strf.biHeight=height;
			strf.biPlanes=1;
			strf.biBitCount=24;
			strf.biCompression=TODWORD('M','J','P','G');
			strf.biSizeImage=width*height*3;
			strf.biXPelsPerMeter=0;
			strf.biYPelsPerMeter=0;
			strf.biClrUsed=0;        /* used colors */
			strf.biClrImportant=0;        /* important colors */
			out.writeRawData((const char*)&strf, sizeof(strf) );   // output includes the length

//                        // list odml
//                        out.writeRawData(TAG_LIST,sizeof(TAG_LIST));
//                        out << LI4(sizeof(AVI_list_odml) - 8);
//                        out.writeRawData(TAG_odml,sizeof(TAG_odml));
//                        qDebug("LIST:odml %d",sizeof(AVI_list_odml) - 8);
//                        struct AVI_odml odml;
//                        odml.id[0]='d';odml.id[1]='m';odml.id[2]='l';odml.id[3]='h';
//                        odml.sz = 4;
//                        odml.frames = frames;
//                        out.writeRawData((const char*)&odml, sizeof(odml) );   // output includes the length
//                        qDebug("dmlh %d",sizeof(odml)-8);

		}

		 // audio stream
		 if( streams == STREAMS_AUDIO || streams == STREAMS_AV )
		 {
			// list strl
			out.writeRawData(TAG_LIST,sizeof(TAG_LIST));
			out << LI4(sizeof(AVI_list_strl_a) - 8 );
			out.writeRawData(TAG_strl,sizeof(TAG_strl));

			// chunk strh
			out.writeRawData(TAG_strh,sizeof(TAG_strh));
			struct AVI_strh strh;
			strh.type[0]='a';strh.type[1]='u';strh.type[2]='d';strh.type[3]='s';      /* stream type */
			strh.handler[0]='\0';strh.handler[1]='\0';strh.handler[2]='\0';strh.handler[3]='\0';
			strh.flags=0;
			strh.priority=0;
			//strh.language=0;
			strh.init_frames=0;       /* first audio frame */
			strh.scale=1;
			strh.rate=8000;
			strh.start=0;
			strh.length=samples;
			strh.buff_sz=0;           /* suggested buffer size */
			strh.quality=0;
			strh.sample_sz=2;
			strh.rect = 0;
			out << LI4(sizeof(AVI_strh));
			out.writeRawData((const char*)&strh, sizeof(strh) );   // output includes the length

			// chunk strf
			out.writeRawData(TAG_strf,sizeof(TAG_strf));
			out << LI4(sizeof(WAVEFORMATEX));
			WAVEFORMATEX strf;
			strf.wFormatTag = 0x0001; // PCM audio
			strf.cbSize = 0;     // extra format info
			strf.nBlockAlign = 2;
			strf.nChannels = 1;
			strf.nSamplesPerSec = 8000;
			strf.nAvgBytesPerSec = strf.nBlockAlign * strf.nSamplesPerSec;
			strf.wBitsPerSample =16;
			out.writeRawData((const char*)&strf, sizeof(strf) );   // output includes the length
		}
                 //                        // list odml
                 //                        out.writeRawData(TAG_LIST,sizeof(TAG_LIST));
                 //                        out << LI4(sizeof(AVI_list_odml) - 8);
                 //                        out.writeRawData(TAG_odml,sizeof(TAG_odml));
                 //                        qDebug("LIST:odml %d",sizeof(AVI_list_odml) - 8);
                 //                        struct AVI_odml odml;
                 //                        odml.id[0]='d';odml.id[1]='m';odml.id[2]='l';odml.id[3]='h';
                 //                        odml.sz = 4;
                 //                        odml.frames = frames;
                 //                        out.writeRawData((const char*)&odml, sizeof(odml) );   // output includes the length
                 //                        qDebug("dmlh %d",sizeof(odml)-8);

		// list movi
		out.writeRawData(TAG_LIST,sizeof(TAG_LIST));
		size = jpgSize + samples + 8*buffers + 4;
		out << LI4(size);
		out.writeRawData(TAG_movi,sizeof(TAG_movi));

		int aa = 0;
		int vv = 0;
		QDEBUG << "Saving buffers:" << buffers;
		for(uint ii=0; ii<(uint)buffers; ii++)
		{
			QByteArray *frame = dataList.at(ii);
			int sz = frame->size();

			if( audioListMarker.count()==0 || ( vv < videoListMarker.count() && videoListMarker.at(vv) == ii ) )
			{
				vv++;
				// video stream tag
				out.writeRawData(TAG_00db,sizeof(TAG_00db));
			} else
			if( aa < audioListMarker.count() && audioListMarker.at(aa) == ii )
			{
				aa++;
				// audio stream tag
				out.writeRawData(TAG_01wb,sizeof(TAG_01wb));
			}
			out << LI4(sz);
			out.writeRawData(frame->constData(),sz);
		}

		// write indices
		out.writeRawData(TAG_idx1,sizeof(TAG_idx1));
		size = 16*buffers;
		out << LI4(size);
		quint32 offset = 4;
		aa = 0;
		vv = 0;
		for(uint ii=0; ii<(uint)buffers; ii++)
		{
			QByteArray *frame = dataList.at(ii);
			if( audioListMarker.count()==0 || ( vv < videoListMarker.count() && videoListMarker.at(vv) == ii ) )
			{
				vv++;
				// video stream tag
				out.writeRawData(TAG_00db,sizeof(TAG_00db));
			} else
			if( aa < audioListMarker.count() && audioListMarker.at(aa) == ii )
			{
				aa++;
				// audio stream tag
				out.writeRawData(TAG_01wb,sizeof(TAG_01wb));
			}

			out << LI4(16);
			out << LI4(offset);
			out << LI4(frame->size());
			offset += frame->size() + 8;
			// clean up the buffer
			delete frame;
		}
		out.writeRawData("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",32);
		dataList.clear();
		audioListMarker.clear();
		videoListMarker.clear();
		frames = 0;
		samples = 0;
		jpgSize = 0;
		framecount = 100;
		elapsed = 0;

		fout.flush();
		fout.close();
		return true;
    } else
        qWarning() << QString("Error: failed to save video %1").arg(filename);

    return false;
}


// this is used to clean up the buffer frames
//
void AviChannelFormat::deleteFrames()
{
    int buffers = dataList.count();

    QDEBUG << "delete all frames:" << buffers;

    for(uint ii=0; ii<(uint)buffers; ii++)
    {
        QByteArray *frame = dataList.at(ii);
        // clean up the buffer
        if( frame ) delete frame;
    }

    dataList.clear();

    // reset the state
    frames = 0;
    samples = 0;
    jpgSize = 0;
    framecount = 100;
    elapsed = 0;

}
