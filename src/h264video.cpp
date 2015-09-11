/*
 * h264video.cpp
 *
 *  DESCRIPTION:
 *   This is the class for implementing h264 conversions for RTP
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

#include <QDebug>
#include <QByteArray>
#include <QImage>
#include <QColor>
#include <QStatusBar>

#include <math.h>

#include "../include/common.h"
#include "vchannel.h"
#include "h264video.h"

/*
 * Note that the approach to this module is to provide only as much as is
 * necessary to achieve the basic functionality required.
 * If more functionality is needed, this module can be expanded
 */

// Note: there are differences between ffmpeg and avconv, making it
// tricky to support both
extern "C"
{
//	TODO recommended by ffmpeg, but not avconv
// #include <libavutil/frame.h>

	#include <libavutil/frame.h>
	#include <libavformat/avio.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
};

// for older versions of avconv
//#ifndef av_frame_alloc
//#define av_frame_alloc avcodec_alloc_frame
//#define av_frame_free  avcodec_free_frame
//#endif

using namespace command_line_arguments;

// make the codec global to all classes
AVCodec *global_codec=NULL;
AVCodecContext  *codecContext=NULL;
unsigned int buffer_index = 0;
int buffer_offset = 0;

extern QStatusBar *statusbar;
extern QString globalstatus;

// global to h264
QByteArray pps;
QByteArray sps;

h264Video::h264Video() :
			fragment_type(0), nal_type(0), start_bit(0),end_bit(0),
			iframe(false), sync_ok(false),picture_ok(false), video_index(-1),
			dst_fmt(PIX_FMT_RGB24),dst_w(0),dst_h(0),
			formatContext(NULL)/*,codecContext(NULL)*/
{
    QDEBUG << "h264Video";
    frameRGB = NULL;

    av_register_all();
    avcodec_register_all();
    // was codec = avcodec_find_decoder(CODEC_ID_H264);
    global_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    Q_ASSERT(global_codec);
    codecContext = avcodec_alloc_context3(global_codec);
    Q_ASSERT(codecContext);
//    int ret = avcodec_get_context_defaults3(codecContext, codec);
//    QDEBUG << "avcodec_get_context_defaults3 returned=" << ret;
    codecContext->flags |= CODEC_FLAG_LOW_DELAY;
    codecContext->flags2 |= CODEC_FLAG2_CHUNKS;
    codecContext->thread_count = 1;
    //codecContext->thread_type = FF_THREAD_SLICE;
    //codecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	QDEBUG << "codecContext type=" << codecContext->codec_type ;
	QDEBUG << "codecContext id=" << codecContext->codec_id;
	QDEBUG << "codecContext width=" << codecContext->width;
	QDEBUG << "codecContext height=" << codecContext->height;

    picture = av_frame_alloc();
    Q_ASSERT(picture);
    codecContext->skip_loop_filter = AVDISCARD_ALL; // skiploopfilter=all
    if( avcodec_open2(codecContext, global_codec, NULL) < 0 )
    {
    	qWarning() << "codec open failed" << endl;
    	Q_ASSERT(0);
    }

    parser = av_parser_init(codecContext->codec_id);
    Q_ASSERT(parser);
    parser->flags |= PARSER_FLAG_ONCE;
	av_init_packet( &pkt );

    waitkey = 0;

	// for development debugging only
    // clean file
//	QFile file("img264.mp4");
//	file.open(QIODevice::WriteOnly);
//	file.close();

}

h264Video::~h264Video() {
    QDEBUG << "~h264Video";

    if( frameRGB ) {
    	av_frame_free(&frameRGB);
    }

    if( codecContext ) {
    	avcodec_close(codecContext);
    	av_free(codecContext);
    }

    if( picture ) {
    	av_free(picture);
    }

    video_index = -1;
    if ( formatContext ) {
        av_free( formatContext );
    }
}

/* RFC 6184
 * All NAL units consist of a single NAL unit type octet, which also
   co-serves as the payload header of this RTP payload format.  A
   description of the payload of a NAL unit follows.

   The syntax and semantics of the NAL unit type octet are specified in
   [1], but the essential properties of the NAL unit type octet are
   summarized below.  The NAL unit type octet has the following format:

      +---------------+
      |0|1|2|3|4|5|6|7|
      +-+-+-+-+-+-+-+-+
      |F|NRI|  Type   |
      +---------------+

   The semantics of the components of the NAL unit type octet, as
   specified in the H.264 specification, are described briefly below.

   F:       1 bit
            forbidden_zero_bit.  The H.264 specification declares a
            value of 1 as a syntax violation.

   NRI:     2 bits
            nal_ref_idc.  A value of 00 indicates that the content of
            the NAL unit is not used to reconstruct reference pictures
            for inter picture prediction.  Such NAL units can be
            discarded without risking the integrity of the reference
            pictures.  Values greater than 00 indicate that the decoding
            of the NAL unit is required to maintain the integrity of the
            reference pictures.

   Type:    5 bits
            nal_unit_type.  This component specifies the NAL unit
            payload type as defined in Table 7-1 of [1] and later within
            this memo.  For a reference of all currently defined NAL
            unit types and their semantics, please refer to Section
            7.4.1 in [1].
 */

// extract frame and return true when whole frame is done
bool h264Video::extractFrame(const char *hdr, qint64 size )
{
	bool success = false;
	// save these for future
    fragment_type = hdr[0] & 0x1F;
    nal_type = hdr[1] & 0x1F;
    start_bit = hdr[1] & 0x80;
    end_bit = hdr[1] & 0x40;

    switch(fragment_type)
    {
		default: Q_ASSERT(0); // break;
		case 0 : QDDEBUG << "Frame" << fragment_type << "[unspecified]" << endl; break;
		case   1  : QDDEBUG << "Frame Coded slice" << endl;
		{
			qba.clear();
			qba.append("\0\0\0\1",4);
			qba.append(hdr,(int)size);
			success = true;
		}
		break;
		case   2  :
			QDDEBUG << "Frame Data Partition A" << endl;
			break;
		case   3  :
			QDDEBUG << "Frame Data Partition B" << endl;
			break;
		case   4  :
			QDDEBUG << "Frame Data Partition C" << endl;
			break;
		case   5  :
			QDDEBUG << "Frame IDR (Instantaneous Decoding Refresh) Picture" << endl;
			break;
		case   6  :
		{
			QDDEBUG << "Frame SEI (Supplemental Enhancement Information)" << endl;
			qba.clear();
			qba.append("\0\0\0\1",4);
			qba.append(hdr,(int)size);
			success = true;

			break;
		}
		case   7  :
			QDDEBUG << "Frame SPS (Sequence Parameter Set)" << endl;
		{
//			char tmp[1024];  char *t=tmp;  for(  int ii=0; ii<size && ii < (1024/3);ii++) {
//					sprintf(t,"%02x ",(unsigned char)hdr[ii]); t += 3; } QDEBUG << size << ":" << tmp << endl;

			//todo sps.setRawData(hdr, (int)size);
			qba.clear();
			qba.append("\0\0\0\1",4);
			qba.append(hdr,(int)size);
			success = true;

			break;
		}
		case   8  :
			QDDEBUG << "Frame PPS (Picture Parameter Set)" << endl;
		{
//			char tmp[1024];  char *t=tmp;  for(  int ii=0; ii<size && ii < (1024/3);ii++) {
//					sprintf(t,"%02x ",(unsigned char)hdr[ii]); t += 3; } QDEBUG << tmp << endl;

			// todo pps.setRawData(hdr,size);
			qba.clear();
			qba.append("\0\0\0\1",4);
			qba.append(hdr,(int)size);
			success = true;

			break;
		}
		case   9  :
			QDDEBUG << "Frame Access Unit Delimiter" << endl;
			break;
		case 10  :
			QDDEBUG << "Frame EoS (End of Sequence)" << endl;
			break;
		case 11  :
			QDDEBUG << "Frame EoS (End of Stream)" << endl;
			break;
		case 12  :
			QDDEBUG << "Frame Filter Data" << endl;
			break;
		case 28:
		{
			QDDEBUG << "Frame Image" << endl;
			// start of frame
			if( start_bit != 0 )
			{
				iframe= ( nal_type == 5 ) ? true : false;
				// QDEBUG << "Frame FU-A " << (iframe?"frame=I":"") << endl;
				qba.clear();

				// mark the first I-frame and record from then on
				if( !sync_ok && iframe) {
					QDEBUG << "first Iframe " << endl;
					sync_ok = true;
				}

				if( sync_ok) {
					char prefix[5] = { 00,00,00,01,00 };
					prefix[4] = ( hdr[0] & 0xE0 ) | (hdr[1] & 0x1F );
					// QDEBUG << "NAL=" << (int)prefix[4] << endl;
					qba.append( prefix,5 );
				}
			}
			if( sync_ok && size>2 ) {
				qba.append( hdr+2, size-2);
			}
			if( sync_ok && end_bit != 0 ) {
				// write the frame
				// return true if we decoded a frame
				success = true;
			} break;

		}
    }
    return success;
}

bool h264Video::writeFrame(const char* frm, int size )
{
        // for debugging only
//		if( size )
//		{
//			QFile file("img264.mp4");
//			file.open(QIODevice::Append);
//			QDataStream qds(&file);
//			int written = qds.writeRawData(frm,size);
//			Q_ASSERT(written>0);
//			file.close();
//			QDEBUG << "write:" << written << endl;
//		}

		// Init packet
    	int64_t pts=0;
    	int64_t dts=0;

		av_parser_parse2(parser, codecContext, &pkt.data,&pkt.size, (const unsigned char*)frm, size,
	                                   pts, dts, 0 /*AV_NOPTS_VALUE*/);

		if( size )
		{
			int got_picture=0;
			avcodec_decode_video2(codecContext, picture, &got_picture, &pkt);
			if( got_picture )
			{

				if( debugsetting > 1 || !picture_ok )
				{
					QDEBUG << "got picture" << endl;
					QDEBUG << "codecContext type=" << codecContext->codec_type ;
					QDEBUG << "codecContext id=" << codecContext->codec_id;
					QDEBUG << "codecContext width=" << codecContext->width;
					QDEBUG << "codecContext height=" << codecContext->height;
					QDEBUG << "codecContext pix_fmt=" << codecContext->pix_fmt;
					QDEBUG << "codecContext sample_aspect_ratio" << codecContext->sample_aspect_ratio.num << "/" << codecContext->sample_aspect_ratio.den;
					QDEBUG << "codecContext time_base" << codecContext->time_base.num << "/" << codecContext->time_base.den;
				}
				// Convert the image from its native format to RGB
				picture_ok = convertFrameToRGB(picture, codecContext->width,codecContext->height, codecContext->pix_fmt );

				qba.clear();
			    return true;
			}
		}
	    return false;
}

/*
 * convert the YUV420p frame to RGB
 */
bool h264Video::convertFrameToRGB(AVFrame *src_frame,  int width, int height, enum AVPixelFormat pix_fmt )
{
	dst_w = width;
	dst_h = height;

	// create a dest frame
	// TODO frameRGB = av_frame_alloc(); // intermediate pframe
	if( frameRGB==NULL )
	{
		frameRGB = av_frame_alloc();
		Q_ASSERT(frameRGB);
		int numBytes= avpicture_get_size( PIX_FMT_RGB24, width, height );
		uint8_t *buffer = (uint8_t*)malloc(numBytes);
		avpicture_fill((AVPicture*)frameRGB, buffer, PIX_FMT_RGB24, width,  height);
		//  cache following conversion context
		img_convert_ctx_temp = sws_getContext( width, height, pix_fmt,
			dst_w, dst_h, (PixelFormat)dst_fmt,
			SWS_BICUBIC, NULL, NULL, NULL);
	}

	sws_scale(img_convert_ctx_temp,
			  src_frame->data, src_frame->linesize, 0, height,
			  frameRGB->data,
			  frameRGB->linesize);

	return true;
}





/*
 * Mp4ChannelFormat
 *
 */

Mp4ChannelFormat::Mp4ChannelFormat(): ChannelFormat(".mp4"),
		avio_ctx(NULL)
{
    QDEBUG << __FUNCTION__;
}

Mp4ChannelFormat::~Mp4ChannelFormat()
{
    QDEBUG << __FUNCTION__;
}

int Mp4ChannelFormat::recordFrame(const unsigned char *frame, int size, STREAMS stream, bool writeon )
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

    // create a new frame buffer
	// TODO
	// why not steal the existing frame?
    QByteArray *f = new QByteArray((const char*)frame,size);
    if(f)
    {
        if( stream == STREAMS_VIDEO )
        {
			frames++;
        	// add the frame to the list
            dataList.append(f);


			// count only image frames
			char typ = f->at(4);
			switch( typ )
			{
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:

			case 6:
			case 7:
			case 8:
			case 0x67:
			case 0x68:
				break;
			case 0x65:
			case 0x41:
			default:
				if( framecount == 0 )
				{
					long ee = timer.elapsed() - elapsed;
					if( ee > 0  )
					{
						int fps = 1000000/ee;
						framecount = 100;
						elapsed = timer.elapsed();
						globalstatus = QString("%1.%2 fps %3 H264").arg(fps/10).arg(fps%10).arg(usetcp?"T":"U");
						if( statusbar && fps ) {
							statusbar->showMessage(globalstatus);
							QDEBUG << globalstatus;
						}
					}
				} else
					framecount--;
				break;
			}

        	if( timer.elapsed() > (writeon?RECORD_FILETIME_WRITEON:RECORD_FILETIME_NOWRITE) )
			{
				ret = 0; // SWITCH_CHANNELS
			}
		} else
		{
			QDEBUG << "unable to allocate buffer";
		}
    }
    return ret;
}

// helper functions to convert Little-endian ints to Big-endian ints
quint32 BE(quint32 s) {
	unsigned char *nn = (unsigned char *)&s;
	return (nn[3]+(nn[2]<<8)+(nn[1]<<16)+(nn[0]<<24));
}
quint16 BE16(quint16 s) {
	unsigned char *nn = (unsigned char *)&s;
	return (nn[1]+(nn[0]<<8));
}

// for debugging only
//#define OUTPUT_RAW_H264 1

bool Mp4ChannelFormat::writeAv(QString filename, STREAMS /* streams */, QDateTime & datetime, qint64 duration )
{
	uint frame_count = 0;
	quint32 total_size = 0;
	uint total_written = 0;

    int buffers = dataList.count();

    // write out the raw file and convert it to mp4 later
    if( buffers > 0 )
	{
		QDEBUG << "Write Mp4: " << filename;
    	// traverse the frames and
    	// add up the total data size
    	{
#ifdef OUTPUT_RAW_H264
    		QString rawfilename = filename + ".raw";
			QFile file((const char*)rawfilename.toLatin1());
			file.open(QIODevice::WriteOnly);
			QDataStream qds(&file);
#endif
			uint ii=0;
			uint startframe = 0;
			for(; ii<(uint)buffers; ii++)
			{
				QByteArray *frame = dataList.at(ii);
				if( frame )
				{
#ifdef OUTPUT_RAW_H264
					int written = qds.writeRawData(
						frame->constData(),
						frame->size() );
					Q_ASSERT(written>0);
#endif
					// add up the total data size
					char typ = frame->at(4);
					switch( typ )
					{
					case 1:
						QDEBUG << "FCS";
						total_size += frame->size();
//						break;
					case 2:
					case 3:
					case 4:
					case 5:
						QDEBUG << "VCL";
						startframe = ii+1;
						break;
					case 6:
						QDEBUG << "SEI";
						total_size += frame->size();
						break;
					case 7:
						QDEBUG << "SPS";
						total_size += frame->size();
						break;
					case 8:
						QDEBUG << "PPS";
						total_size += frame->size();
						break;
					case 0x67:
					case 0x68:
						QDEBUG << "x67 / x68";
						total_size += frame->size();
						break;

					case 0x65:
					case 0x27:
						startframe = ii+1;
					case 0x41:
					default:
						// image frames
						if( startframe>0 )
						{
							char tmp[64];
							sprintf(tmp,"f: 0x%x (%d) size= %d",(int)typ, (int)typ, frame->size());
							QDEBUG << tmp;

							frame_count++;
							total_size += frame->size();
						} else
						{
							char tmp[64];
							sprintf(tmp,"skipping frame: 0x%x (%d) size= %d",(int)typ, (int)typ, frame->size());
							QDEBUG << tmp;
						}
						break;
					}
					// free the frame memory
					//todo delete frame;
				}
			}
#ifdef OUTPUT_RAW_H264
			file.close();
#endif
    	}
		// QDEBUG << "write:" << ii << "buffers to" << filename << endl;
    	QDEBUG << "frame count=" << frame_count << "total size=" << total_size;

    	if( frame_count == 0 ) {
    		qWarning() << "No frames in image - not saved";
    		return false;
    	}

// write out the mp4
   /* Include the following boxes, based on what ffmpeg does:
	*       ftyp
	*       free
	*       mdat
	*         <video data here>
	*       moov
	*        mvhd
	*	    trak
	*	     tkhd
	*        edts (optional)
	*          elst
	*        mdia
	*          mdhd
	*			 hdlr
	*          minf
	*            vmhd
	*            dinf
	*              dref
	*               url
	*          stbl
	*            stsd
	*            stts
	*            stss
	*            stsc
	*            stsz
	*            stco
	*        udta - user data
	*          meta
	*            hdlr
	*            ilst - itunes metadata
	*              ©too
	*                data
	*
	*/
	    	// get the time elapsed since Jan 1, 1904
	    	QDateTime qdtRef(QDate(1904,1,1),QTime(0,0));
	    	int creation_time = qdtRef.secsTo(datetime);
	    	// number of time units that pass in one second
	    	//Q_ASSERT(codecContext->time_base.num);
	    	int time_scale = 1000; // msecs
	    	// track_id
	    	int track_id = 1;
	    	// width & height
	    	int width = codecContext->width;
   			int height = codecContext->height;
   			QDEBUG << "width" << width;
   			QDEBUG << "height" << height;


			QFile file((const char*)filename.toLatin1());
			file.open(QIODevice::WriteOnly);
			QDataStream qds(&file);

			Mp4Box *mp4box = new Mp4FileTypeBox("ftyp","isom",0x200);
			QDEBUG << "file box size=" << BE(mp4box->size) << "headersize="<<mp4box->headersize;
			const char compatible_brands[] = "isomiso2avc1mp41" ;
			mp4box->size = BE(BE(mp4box->size)+strlen(compatible_brands));
			total_written += qds.writeRawData(mp4box->header(),mp4box->headersize);
			total_written += qds.writeRawData(compatible_brands,strlen(compatible_brands));
			delete mp4box;

			mp4box = new Mp4Box("free");
			total_written += qds.writeRawData(mp4box->header(),mp4box->headersize);
			delete mp4box;

			mp4box = new Mp4Box("mdat");
			mp4box->size = BE(BE(mp4box->size)+total_size);
			total_written += qds.writeRawData(mp4box->header(),mp4box->headersize);
			delete mp4box;

			// save the start of the chunk data
			quint32 chunk_offset = BE(total_written);

			// set up the sample count array
			quint32 *sample_count_array = new quint32[frame_count];
			uint frame_index =0;
			uint prev_frame_marker = total_written;
			uint ii=0;
			bool startframe = false;
			for(; ii<(uint)buffers; ii++)
			{
				QByteArray *frame = dataList.at(ii);
				if( frame )
				{
					char typ = frame->at(4);
					switch( typ )
					{
					case 1:
					case 2:
					case 3:
					case 4:
					case 5:
						startframe = true;
					case 6:
					case 7:
					case 8:
					case 0x67:
					case 0x68:
						{
						// replace the 1st 4 bytes with the size
						quint32 len = BE(frame->size()-4);
						// write frames
						quint32 written = qds.writeRawData((const char *)&len,4);
						written += qds.writeRawData(
							frame->constData()+4,
							frame->size()-4 );
						Q_ASSERT(written>0);
						total_written += written;
						}
						break;

					case 0x65:
					case 0x27:
						startframe = true;
					case 0x41:
					default:
						if( startframe)
						{
							// replace the 1st 4 bytes with the size
							quint32 len = BE(frame->size()-4);
							// write image frames
							quint32 written = qds.writeRawData((const char *)&len,4);
							written += qds.writeRawData(
								frame->constData()+4,
								frame->size()-4 );
							Q_ASSERT(written>0);
							total_written += written;
							// save the number of bytes in each sample
							// this is wierd, but we measure from the end of one
							// sample to the end of the next
							sample_count_array[frame_index++] = BE(total_written-prev_frame_marker);
							prev_frame_marker = total_written;
						}
						break;
					}

					// free the frame memory
					delete frame;
				}
			}
			// QDEBUG << "frames=" << frame_count;

			Mp4Box *mp4moviebox = new Mp4Box("moov");
				Mp4MovieHeaderBox *mp4MovieHeaderBox = new Mp4MovieHeaderBox("mvhd",
							creation_time,time_scale,duration );

				Mp4Box *mp4trackbox = new Mp4Box("trak");
					Mp4TrackHeaderBox *mp4TrackHeaderBox = new Mp4TrackHeaderBox("tkhd",
							creation_time,track_id,duration, width, height );
					Mp4Box *mp4editbox = new Mp4Box("edts");
						Mp4EditListBox *mp4editlistbox = new Mp4EditListBox("elst",duration);
						mp4editbox->size = BE( BE(mp4editbox->size) + BE(mp4editlistbox->size) );

					Mp4Box *mp4mediabox = new Mp4Box("mdia");
						Mp4MediaHeaderBox *mp4MediaHeaderBox = new Mp4MediaHeaderBox("mdhd",
								creation_time,time_scale,duration );
						Mp4HandlerBox *mp4HandlerBox = new Mp4HandlerBox("hdlr","VideoHandler" );

						Mp4Box *mp4mediainfobox = new Mp4Box("minf");
							Mp4VideoMediaHeaderBox *mp4VideoMediaHeaderBox = new Mp4VideoMediaHeaderBox("vmhd");

							Mp4Box *mp4datainfobox = new Mp4Box("dinf");
								Mp4DataReferenceBox *mp4DataReferenceBox = new Mp4DataReferenceBox("dref",1);
									// include 1 url
									Mp4DataEntryUrlBox *mp4DataEntryUrlBox = new Mp4DataEntryUrlBox("url ");
									// no url data
								mp4DataReferenceBox->size = BE( BE(mp4DataReferenceBox->size) + BE(mp4DataEntryUrlBox->size) );
								mp4datainfobox->size = BE( BE(mp4datainfobox->size) + BE(mp4DataReferenceBox->size) );

							Mp4Box *mp4sampletablebox = new Mp4Box("stbl");

								Mp4SampleBox *mp4sampledescriptionbox = new Mp4SampleBox("stsd",1);
								// include 1 entry
								Mp4VisualSampleEntryBox *mp4visualsampleentrybox = new Mp4VisualSampleEntryBox("avc1", width, height);
								 	 AVCDecoderConfigurationRecord *avcdecoderconfigurationrecord = new AVCDecoderConfigurationRecord( "avcC",
								 			sps.constData(), sps.length(), pps.constData(), pps.length());

									 	mp4visualsampleentrybox->size = BE( BE(mp4visualsampleentrybox->size) +
																		BE(avcdecoderconfigurationrecord->size) );
										QDEBUG << "avcdecoderconfigurationrecord size=" << BE(avcdecoderconfigurationrecord->size);

									mp4sampledescriptionbox->size = BE( BE(mp4sampledescriptionbox->size) +
												  	  	      	    BE(mp4visualsampleentrybox->size) );
									QDEBUG << "mp4visualsampleentrybox size=" << BE(mp4visualsampleentrybox->size);

 	 	 	 	 	 	 	 	 Mp4SampleBox *mp4timetosamplebox = new Mp4SampleBox("stts",1);
 	 	 	 	 	 	 	 	 mp4timetosamplebox->size = BE( BE(mp4timetosamplebox->size) +
 	 	 	 	 	 	 	 			 	 	 	 	 	 (1*2*sizeof(quint32)) );
 	 	 	 	 	 	 	 	 SampleTime *sampletime = new SampleTime(frame_count,duration/frame_count);

 	 	 	 	 	 	 	 	 Mp4SampleBox *mp4syncsamplebox = new Mp4SampleBox("stss",0);

 	 	 	 	 	 	 	 	 Mp4SampleBox *mp4sampletochunkbox = new Mp4SampleBox("stsc",1);
 	 	 	 	 	 	 	 	 mp4sampletochunkbox->size = BE( BE(mp4sampletochunkbox->size) +
 	 	 	 	 	 	 	 			 	 	 	 	 	 (1*sizeof(SampleChunk)) );
 	 	 	 	 	 	 	 	 SampleChunk *samplechunk = new SampleChunk(1,frame_count,1);

 	 	 	 	 	 	 	 	 Mp4SampleSizeBox *mp4samplesizebox = new Mp4SampleSizeBox("stsz",0,frame_count);
 	 	 	 	 	 	 	 	 mp4samplesizebox->size = BE( BE(mp4samplesizebox->size) +
 	 	 	 	 	 	 	 			 	 	 	 	 	 (frame_count*sizeof(quint32)) );
 	 	 	 	 	 	 	 	 Mp4SampleBox *mp4chunkoffsetbox = new Mp4SampleBox("stco",1);
 	 	 	 	 	 	 	 	 mp4chunkoffsetbox->size = BE( BE(mp4chunkoffsetbox->size) +
 	 	 	 	 	 	 	 			 	 	 	 	 	 (1*sizeof(quint32)) );
							mp4sampletablebox->size = BE( BE(mp4sampletablebox->size) +
													  BE(mp4sampledescriptionbox->size)+
													  BE(mp4timetosamplebox->size) +
													  BE(mp4syncsamplebox->size) +
													  BE(mp4sampletochunkbox->size) +
													  BE(mp4samplesizebox->size) +
													  BE(mp4chunkoffsetbox->size)
													  );

							QDEBUG << "mp4sampledescriptionbox size=" << BE(mp4sampledescriptionbox->size);

						mp4mediainfobox->size = BE( BE(mp4mediainfobox->size) +
											    BE(mp4VideoMediaHeaderBox->size) +
											    BE(mp4datainfobox->size)+
											    BE(mp4sampletablebox->size));

					mp4mediabox->size = BE( BE(mp4mediabox->size)+
							 	 	    BE(mp4MediaHeaderBox->size)+
							 	 	    BE(mp4HandlerBox->size)+
							 	 	    BE(mp4mediainfobox->size) );


				mp4trackbox->size = BE( BE(mp4trackbox->size)+
										BE(mp4TrackHeaderBox->size)+
										BE(mp4editbox->size)+
										BE(mp4mediabox->size) );

			mp4moviebox->size = BE( BE(mp4moviebox->size)+BE(mp4MovieHeaderBox->size)+BE(mp4trackbox->size));

			total_written += qds.writeRawData(mp4moviebox->header(),mp4moviebox->headersize );
			total_written += qds.writeRawData(mp4MovieHeaderBox->header(),mp4MovieHeaderBox->headersize);
			total_written += qds.writeRawData(mp4trackbox->header(),mp4trackbox->headersize);
			total_written += qds.writeRawData(mp4TrackHeaderBox->header(),mp4TrackHeaderBox->headersize);
			total_written += qds.writeRawData(mp4editbox->header(),mp4editbox->headersize);
			total_written += qds.writeRawData(mp4editlistbox->header(),mp4editlistbox->headersize);
			total_written += qds.writeRawData(mp4mediabox->header(),mp4mediabox->headersize);
			total_written += qds.writeRawData(mp4MediaHeaderBox->header(),mp4MediaHeaderBox->headersize);
			total_written += qds.writeRawData(mp4HandlerBox->header(),mp4HandlerBox->headersize);       // sized by the string
			total_written += qds.writeRawData(mp4mediainfobox->header(),mp4mediainfobox->headersize);
			total_written += qds.writeRawData(mp4VideoMediaHeaderBox->header(),mp4VideoMediaHeaderBox->headersize);
			total_written += qds.writeRawData(mp4datainfobox->header(),mp4datainfobox->headersize);
			total_written += qds.writeRawData(mp4DataReferenceBox->header(),mp4DataReferenceBox->headersize);
			total_written += qds.writeRawData(mp4DataEntryUrlBox->header(),mp4DataEntryUrlBox->headersize);
			total_written += qds.writeRawData(mp4sampletablebox->header(),mp4sampletablebox->headersize);
			total_written += qds.writeRawData(mp4sampledescriptionbox->header(),mp4sampledescriptionbox->headersize);
			total_written += qds.writeRawData(mp4visualsampleentrybox->header(),mp4visualsampleentrybox->headersize); // does not fall on 4-byte boundary
			total_written += qds.writeRawData(avcdecoderconfigurationrecord->header(),avcdecoderconfigurationrecord->headersize); // does not fall on 4-byte boundary

			qds << (quint8)0xe1;
			total_written += 1;
			quint16 spslen = BE16((quint16)sps.length());
			total_written += qds.writeRawData((const char*)&spslen,sizeof(quint16));
			total_written += qds.writeRawData((const char*)sps.constData(),sps.length());

			qds << (quint8)1;
			total_written += 1;
			quint16 ppslen = BE16((quint16)pps.length());
			total_written += qds.writeRawData((const char*)&ppslen,sizeof(quint16));
			total_written += qds.writeRawData((const char*)pps.constData(),pps.length());

			total_written += qds.writeRawData(mp4timetosamplebox->header(),mp4timetosamplebox->headersize);
			total_written += qds.writeRawData((const char*)sampletime,sizeof(SampleTime));

			total_written += qds.writeRawData(mp4syncsamplebox->header(),mp4syncsamplebox->headersize);

			total_written += qds.writeRawData(mp4sampletochunkbox->header(),mp4sampletochunkbox->headersize);
			total_written += qds.writeRawData((const char*)samplechunk,sizeof(SampleChunk));

			total_written += qds.writeRawData(mp4samplesizebox->header(),mp4samplesizebox->headersize);
			total_written += qds.writeRawData((const char*)sample_count_array,frame_count*sizeof(quint32));

			total_written += qds.writeRawData(mp4chunkoffsetbox->header(),mp4chunkoffsetbox->headersize);
			total_written += qds.writeRawData((const char*)&chunk_offset,sizeof(quint32));

			delete mp4chunkoffsetbox;
			delete [] sample_count_array;
			delete mp4samplesizebox;
			delete samplechunk;
			delete mp4sampletochunkbox;
			delete mp4syncsamplebox;
			delete sampletime;
			delete mp4timetosamplebox;
			delete avcdecoderconfigurationrecord;
			delete mp4visualsampleentrybox;
			delete mp4sampledescriptionbox;
			delete mp4sampletablebox;
			delete mp4DataEntryUrlBox;
			delete mp4DataReferenceBox;
			delete mp4datainfobox;
			delete mp4VideoMediaHeaderBox;
			delete mp4mediainfobox;
			delete mp4HandlerBox;
			delete mp4MediaHeaderBox;
			delete mp4mediabox;
			delete mp4editlistbox;
			delete mp4editbox;
			delete mp4TrackHeaderBox;
			delete mp4trackbox;
			delete mp4MovieHeaderBox;
			delete mp4moviebox;

			file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther);
			file.close();


		// empty the list
		dataList.clear();

		// reset the state
	    frames = 0;
	    samples = 0;

	    framecount = 100;
	    elapsed = 0;

	    return true;
	}
    return false;
}


// this is used to clean up the buffer frames
//
void Mp4ChannelFormat::deleteFrames()
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

    framecount = 100;
    elapsed = 0;

}
