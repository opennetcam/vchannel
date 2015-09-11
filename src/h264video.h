/*
 * h264video.h
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

#ifndef H264VIDEO_H_
#define H264VIDEO_H_
#include <QImage>
#include "../include/common.h"

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

extern "C"
{
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
}
#include "avformat.h"

extern QByteArray sps;
extern QByteArray pps;

class h264Video {
public:
	h264Video();
	virtual ~h264Video();

	bool extractFrame(const char *hdr, qint64 size);
	bool isH264iframe() { return iframe; }
	bool writeFrame(const char* frm, int size );
	bool convertFrameToRGB(AVFrame *src_frame, int width, int height, enum AVPixelFormat pix_fmt );
	const unsigned char * frame() { return (const unsigned char*)qba.constData(); }
	int size() { return qba.size(); }
	int width() { return dst_w; }
	int height() { return dst_h; }
	int format() { return dst_fmt; }
	bool gotImage() { return picture_ok; }
	QImage &img() {return myimage; }
	void resetSync() { sync_ok = false; }

	const unsigned char *imageRGB() { return (const unsigned char*)frameRGB?frameRGB->data[0]:NULL; }
	int imageHeight() { return dst_h; }
	int imageWidth() { return dst_w; }
	int imageSize() { return frameRGB?frameRGB->linesize[0]*dst_h :0; }

protected:
    int fragment_type;
    int nal_type;
    int start_bit;
    int end_bit;
    bool iframe;
    bool sync_ok;				// flag when the first keyframe is found
    bool picture_ok;           // flag when first picture is decoded
    int video_index;
    int waitkey;
    int dst_fmt;
    int dst_w;
    int dst_h;

    AVFormatContext *formatContext ;
    AVFrame *picture;
    AVCodecParserContext *parser;
	AVPacket pkt;
	AVFrame *frameRGB;
	QImage myimage;
	SwsContext *img_convert_ctx_temp;

    QByteArray qba;

};

// TODO move to separate file
class Mp4ChannelFormat : ChannelFormat
{
public:
	Mp4ChannelFormat();
    ~Mp4ChannelFormat();
    bool writeAv(QString filename, STREAMS streams, QDateTime & datetime, qint64 duration  );
    int recordFrame(const unsigned char *frame, int size, STREAMS stream,bool writeon );
    void deleteFrames();
    void setFormat(int fmt) { dst_fmt = fmt;}

private:
    int dst_fmt;
    // custom ioformat for buffered IO
    AVIOContext* avio_ctx;
};

quint32 BE(quint32 s);
quint16 BE16(quint16 s);

// mp4 box definitions
class Mp4Box
{
public:
	Mp4Box(const char boxtype[4])  { headersize=sizeof(quint32)*2; \
									 size = BE(headersize); \
									 strncpy(type,boxtype,4);}
	const char* header() { return (const char*)&size; }
	quint32 headersize;
	// header starts here
	quint32 size;
	char type[4];
};

class Mp4FullBox : public Mp4Box
{
public:
	Mp4FullBox(const char boxtype[4], quint8 v, const char *f ) : Mp4Box(boxtype), version(v) \
		{ flags[0] = f[0]; flags[1]=f[1]; flags[2]=f[2];\
		  headersize += 4;
		  size = BE(headersize);}
	quint8 version;
	quint8 flags[3];
};

class Mp4FileTypeBox : public Mp4Box
{
public:
	Mp4FileTypeBox(const char boxtype[4], const char majbrand[4], quint32 v ) : Mp4Box(boxtype) \
		{ minor_version = BE(v); strncpy(major_brand,majbrand,4); \
		 headersize += 4+sizeof(quint32); size = BE( headersize ); }
	char major_brand[4];
	quint32 minor_version;
};


class Mp4MovieHeaderBox : public Mp4FullBox
{
public:
	Mp4MovieHeaderBox(const char boxtype[4], quint32 ct, quint32 ts, quint32 du ) : Mp4FullBox(boxtype, 0, "\0\0\0" )  \
		{ creation_time = modification_time = BE(ct); timescale = BE(ts); duration = BE(du); \
		  rate = BE(0x00010000); volume = BE16(0x0100); reserved1=0,reserved2[0]=reserved2[1]=0; \
		  matrix[0] = BE(0x00010000); matrix[1] = matrix[2] = matrix[3] = 0; \
		  matrix[4] = BE(0x00010000); matrix[5] = matrix[6] = matrix[7] = 0;  \
		  matrix[8] = BE(0x40000000); \
		  pre_defined[0] = pre_defined[1] = pre_defined[2] = pre_defined[3] = pre_defined[4] = pre_defined[5] = 0; \
		  next_track_id = 0; \
		  headersize += 24*sizeof(quint32); \
		  size = BE( headersize ); }
	quint32 creation_time;
	quint32 modification_time;
	quint32 timescale;
	quint32 duration;
	quint32 rate;
	quint16 volume;
	quint16 reserved1;
	quint32 reserved2[2];
	quint32 matrix[9];
	quint32 pre_defined[6];
	quint32 next_track_id;
};

/*
 *  flags is a 24-bit integer with flags; the following values are defined:
	Track_enabled: Indicates that the track is enabled. Flag value is 0x000001. A disabled track (the low
	bit is zero) is treated as if it were not present.
	Track_in_movie: Indicates that the track is used in the presentation. Flag value is 0x000002.
	Track_in_preview: Indicates that the track is used when previewing the presentation. Flag value is
	0x000004.

	* some definitions:
	* aligned(8) class Box (unsigned int(32) boxtype,
		optional unsigned int(8)[16] extended_type) {
			unsigned int(32) size;
			unsigned int(32) type = boxtype;
			if (size==1) {
				unsigned int(64) largesize;
			} else if (size==0) {
				// box extends to end of file
			}
			if (boxtype==‘uuid’) {
				unsigned int(8)[16] usertype = extended_type;
			}
		}
	* aligned(8) class FullBox(unsigned int(32) boxtype, unsigned int(8) v, bit(24) f)
		extends Box(boxtype) {
		unsigned int(8)
		version = v;
		bit(24)
		flags = f;
	}
	* aligned(8) class FileTypeBox
		extends Box(‘ftyp’) {
		unsigned int(32) major_brand;
		unsigned int(32) minor_version;
		unsigned int(32) compatible_brands[];
	}
	*        aligned(8) class MovieHeaderBox extends FullBox(‘mvhd’, version, 0) {
				if (version==1) {
					unsigned int(64) creation_time;
					unsigned int(64) modification_time;
					unsigned int(32) timescale;
					unsigned int(64) duration;
				} else { // version==0
					unsigned int(32) creation_time;
					unsigned int(32) modification_time;
					unsigned int(32) timescale;
					unsigned int(32) duration;
				}
				template int(32) rate = 0x00010000; // typically 1.0
				template int(16) volume = 0x0100;
				// typically, full volume
				const bit(16) reserved = 0;
				const unsigned int(32)[2] reserved = 0;
				template int(32)[9] matrix =
				{ 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 };
				// Unity matrix
				bit(32)[6] pre_defined = 0;
				unsigned int(32) next_track_ID;
			}
		*	       aligned(8) class TrackHeaderBox
							extends FullBox(‘tkhd’, version, flags){
								if (version==1) {
									unsigned int(64) creation_time;
									unsigned int(64) modification_time;
									unsigned int(32) track_ID;
									const unsigned int(32) reserved = 0;
									unsigned int(64) duration;
								} else { // version==0
									unsigned int(32) creation_time;
									unsigned int(32) modification_time;
									unsigned int(32) track_ID;
									const unsigned int(32) reserved = 0;
									unsigned int(32) duration;
								}
								const unsigned int(32)[2] reserved = 0;
								template int(16) layer = 0;
								template int(16) alternate_group = 0;
								template int(16) volume = {if track_is_audio 0x0100 else 0};
								const unsigned int(16) reserved = 0;
								template int(32)[9] matrix=
								{ 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 };
								// unity matrix
								unsigned int(32) width;
								unsigned int(32) height;
							}
			*            aligned(8) class EditListBox extends FullBox(‘elst’, version, 0) {
								unsigned int(32) entry_count;
								for (i=1; i <= entry_count; i++) {
									if (version==1) {
										unsigned int(64) segment_duration;
										int(64) media_time;
									} else { // version==0
										unsigned int(32) segment_duration;
										int(32) media_time;
									}
									int(16) media_rate_integer;
									int(16) media_rate_fraction = 0;
								}
							}
			*
			*          aligned(8) class MediaHeaderBox extends FullBox(‘mdhd’, version, 0) {
							if (version==1) {
							unsigned int(64) creation_time;
							unsigned int(64) modification_time;
							unsigned int(32) timescale;
							unsigned int(64) duration;
							} else { // version==0
							unsigned int(32) creation_time;
							unsigned int(32) modification_time;
							unsigned int(32) timescale;
							unsigned int(32) duration;
							}
							bit(1)
							pad = 0;
							unsigned int(5)[3]
							language;
							// ISO-639-2/T language code
							unsigned int(16) pre_defined = 0;
							}
			*			aligned(8) class HandlerBox extends FullBox(‘hdlr’, version = 0, 0) {
								unsigned int(32) pre_defined = 0;
								unsigned int(32) handler_type;
								const unsigned int(32)[3] reserved = 0;
								string name;
							}
			*            aligned(8) class VideoMediaHeaderBox
							extends FullBox(‘vmhd’, version = 0, 1) {
								template unsigned int(16) graphicsmode = 0;
								// copy, see below
								template unsigned int(16)[3] opcolor = {0, 0, 0};
							}
		*              aligned(8) class DataReferenceBox
							extends FullBox(‘dref’, version = 0, 0) {
								unsigned int(32) entry_count;
								for (i=1; i <= entry_count; i++) {
									DataEntryBox(entry_version, entry_flags) data_entry;
								}
							}
			*               aligned(8) class DataEntryUrlBox (bit(24) flags)
								extends FullBox(‘url ’, version = 0, flags) {
									string location;
								}
			*            aligned(8) class SampleDescriptionBox (unsigned int(32) handler_type)
							extends FullBox('stsd', 0, 0){
								int i ;
								unsigned int(32) entry_count;
								for (i = 1 ; i <= entry_count ; i++){
									switch (handler_type){
										case ‘soun’: // for audio tracks
											AudioSampleEntry();
											break;
										case ‘vide’: // for video tracks
											VisualSampleEntry();
											break;
										case ‘hint’: // Hint track
											HintSampleEntry();
											break;
										case ‘meta’: // Metadata track
											MetadataSampleEntry();
											break;
										}
									}
								}
							}
			*            aligned(8) class TimeToSampleBox
							extends FullBox(’stts’, version = 0, 0) {
								unsigned int(32) entry_count;
								int i;
								for (i=0; i < entry_count; i++) {
									unsigned int(32) sample_count;
									unsigned int(32) sample_delta;
								}
							}
			*            aligned(8) class SyncSampleBox
							extends FullBox(‘stss’, version = 0, 0) {
								unsigned int(32) entry_count;
								int i;
								for (i=0; i < entry_count; i++) {
									unsigned int(32) sample_number;
								}
							}
			*            aligned(8) class SampleToChunkBox
							extends FullBox(‘stsc’, version = 0, 0) {
								unsigned int(32) entry_count;
								for (i=1; i <= entry_count; i++) {
									unsigned int(32) first_chunk;
									unsigned int(32) samples_per_chunk;
									unsigned int(32) sample_description_index;
								}
							}
			*            aligned(8) class SampleSizeBox extends FullBox(‘stsz’, version = 0, 0) {
								unsigned int(32) sample_size;
								unsigned int(32) sample_count;
								if (sample_size==0) {
									for (i=1; i <= sample_count; i++) {
										unsigned int(32) entry_size;
									}
								}
							}
			*            aligned(8) class ChunkOffsetBox
							extends FullBox(‘stco’, version = 0, 0) {
								unsigned int(32) entry_count;
								for (i=1; i <= entry_count; i++) {
									unsigned int(32) chunk_offset;
								}
							}
			*            aligned(8) class HandlerBox extends FullBox(‘hdlr’, version = 0, 0) {
								unsigned int(32) pre_defined = 0;
								unsigned int(32) handler_type;
								const unsigned int(32)[3] reserved = 0;
								string 	name;
							}

 */
class Mp4TrackHeaderBox : public Mp4FullBox
{
public:
	Mp4TrackHeaderBox(const char boxtype[4], quint32 ct, quint32 tid, quint32 du, quint32 w, quint32 h ) : Mp4FullBox(boxtype, 0, "\0\0\3" )  \
		{ creation_time = modification_time = BE(ct); track_id = BE(tid); duration = BE(du); \
		  layer = 0; alternate_group=0; volume = 0;  \
		  reserved1=reserved2[0]=reserved2[1]=reserved3=0; \
		  matrix[0] = BE(0x00010000); matrix[1] = matrix[2] = matrix[3] = 0; \
		  matrix[4] = BE(0x00010000); matrix[5] = matrix[6] = matrix[7] = 0;  \
		  matrix[8] = BE(0x40000000); \
		  width=BE(w<<16); height=BE(h<<16); \
		  headersize += 20*sizeof(quint32); \
		  size = BE( headersize ); }
	quint32 creation_time;
	quint32 modification_time;
	quint32 track_id;
	quint32 reserved1;
	quint32 duration;
	quint32 reserved2[2];
	quint16 layer;
	quint16 alternate_group;
	quint16 volume;
	quint16 reserved3;
	quint32 matrix[9];
	quint32 width;
	quint32 height;
};

class Mp4EditListBox : public Mp4FullBox
{
public:
	Mp4EditListBox(const char boxtype[4], quint32 du ) : Mp4FullBox(boxtype, 0, "\0\0\0" )  \
		{ segment_duration = BE(du); media_time=0; entry_count = BE(1); \
		  media_rate_integer = BE16(1); media_rate_fraction = 0; \
		  headersize += 4*sizeof(quint32); \
		  size = BE( headersize ); }
	quint32 entry_count;
	quint32 segment_duration;
	qint32 media_time;
	qint16 media_rate_integer;
	qint16 media_rate_fraction;
};


class Mp4MediaHeaderBox : public Mp4FullBox
{
public:
	Mp4MediaHeaderBox(const char boxtype[4], quint32 ct, quint32 ts, quint32 du ) : Mp4FullBox(boxtype, 0, "\0\0\0" )  \
		{ creation_time =  BE(ct); modification_time = 0; timescale = BE(ts); duration = BE(du); \
		  language = 0xc455; /*'und'*/ pre_defined=0; \
		  headersize += 5 * sizeof(quint32); \
		  size = BE( headersize ); }
	quint32 creation_time;
	quint32 modification_time;
	quint32 timescale;
	quint32 duration;
	quint16 language;
	quint16 pre_defined;
};

#define HANDLER_NAME_SIZE 32
class Mp4HandlerBox : public Mp4FullBox
{
public:
	Mp4HandlerBox(const char boxtype[4], const char *name ) : Mp4FullBox(boxtype, 0, "\0\0\0" )  \
		{ pre_defined=0; strncpy(handler_type,"vide",4) ; \
		  reserved[0] = reserved[1] = reserved[2] = 0; memset((char*)handler_name,0, HANDLER_NAME_SIZE); \
		  strncpy(handler_name,name,HANDLER_NAME_SIZE-1) ; \
		  headersize += 5*sizeof(quint32)+strlen(handler_name)+1 ; \
	  	  size = BE( headersize ); }
	quint32 pre_defined;
	char    handler_type[4];
	quint32 reserved[3];
	char    handler_name[HANDLER_NAME_SIZE];  // allocate spare bytes
};

class Mp4VideoMediaHeaderBox : public Mp4FullBox
{
public:
	Mp4VideoMediaHeaderBox(const char boxtype[4]) : Mp4FullBox(boxtype, 0, "\0\0\1" )  \
		{ graphicsmode = opcolor[0] = opcolor[1] = opcolor[2] = 0;  \
		  headersize += 4*sizeof(quint16); \
		  size = BE( headersize ); }
	quint16 graphicsmode;
	quint16 opcolor[3];
};

class Mp4DataEntryUrlBox : public Mp4FullBox
{
public:
	Mp4DataEntryUrlBox(const char boxtype[4]) : Mp4FullBox(boxtype, 0, "\0\0\1" )  \
	  { size = BE( headersize ); }
	// no url data - for now
};

class Mp4DataReferenceBox : public Mp4FullBox
{
public:
	Mp4DataReferenceBox(const char boxtype[4], quint32 count ) : Mp4FullBox(boxtype, 0, "\0\0\0" )  \
		{ entry_count=BE(count); \
		headersize += sizeof(quint32); \
		size = BE( headersize ); }
	quint32 entry_count;
};

// only video types
class Mp4SampleBox : public Mp4FullBox
{
public:
	Mp4SampleBox(const char boxtype[4], quint32 cnt ) : Mp4FullBox(boxtype, 0, "\0\0\0" )  \
		{ entry_count= BE(cnt); \
		headersize += sizeof(quint32); \
		size = BE( headersize ); }
	quint32 entry_count;
};


class Mp4SampleSizeBox : public Mp4FullBox
{
public:
	Mp4SampleSizeBox(const char boxtype[4], quint32 sze, quint32 cnt ) : Mp4FullBox(boxtype, 0, "\0\0\0" )  \
		{ sample_size = BE(sze); sample_count= BE(cnt); \
		headersize += 2*sizeof(quint32); \
		size = BE( headersize ); }
	quint32 sample_size;
	quint32 sample_count;
};

class SampleTime
{
public:
	SampleTime(quint32 co,quint32 de ) { \
		sample_count=BE(co);sample_delta=BE(de); }
	quint32 sample_count;
	quint32 sample_delta;
};

class SampleChunk
{
public:
	SampleChunk(quint32 fi,quint32 sa, quint32 in ) { \
		first_chunk=BE(fi);samples_per_chunk=BE(sa);sample_description_index=BE(in); \
	}
	quint32 first_chunk;
	quint32 samples_per_chunk;
	quint32 sample_description_index;
};

#define COMPRESSORNAME_SIZE 32
class Mp4VisualSampleEntryBox : public Mp4Box
{
public:
	Mp4VisualSampleEntryBox(const char boxtype[4], quint16 w, quint16 h ) : Mp4Box(boxtype )  \
		{ reserved[0]=reserved[1]=reserved[2]=reserved[3]=reserved[4]=reserved[5]=0; \
		  data_reference_index=BE16(1); pre_defined=reserved1=pre_defined1[0]=pre_defined1[1]=pre_defined1[2]=0; \
		  width = BE16(w); height = BE16(h); \
		  horizresolution = BE(0x00480000); vertresolution = BE(0x00480000); \
		  reserved2=0; frame_count = BE16(1); memset(compressorname,0,COMPRESSORNAME_SIZE) ; \
		  depth=BE16(0x0018); pre_defined2=BE16(-1); \
		  headersize += 6+8*sizeof(quint16)+6*sizeof(quint32)+COMPRESSORNAME_SIZE; \
		  size = BE( headersize ); }
	quint8  reserved[6];
	quint16 data_reference_index;
	quint16 pre_defined;
	quint16 reserved1;
	quint32 pre_defined1[3];
	quint16 width;
	quint16 height;
	quint32 horizresolution;
	quint32 vertresolution;
	quint32 reserved2;
	quint16 frame_count;
	char    compressorname[COMPRESSORNAME_SIZE];
	quint16 depth;
	qint16 pre_defined2;
};

class AVCDecoderConfigurationRecord : public Mp4Box
{
public:
	AVCDecoderConfigurationRecord(const char boxtype[4],
								  const char* sps, int spslen,
								  const char* , int ppslen  ) : Mp4Box(boxtype)
			{ configurationVersion=1; AVCProfileIndication=sps[1];\
			  profile_compatibility=sps[2]; AVCLevelIndication=sps[3];\
			  lengthSizeMinusOne=3|0xfc; \
			  headersize += 5*sizeof(quint8); \
			  size = BE( headersize + 3 + spslen + 3 + ppslen ); }
	quint8  configurationVersion;
	quint8  AVCProfileIndication;
	quint8  profile_compatibility;
	quint8  AVCLevelIndication;
	quint8  lengthSizeMinusOne; // 2 bits - top 6 bits set to '1'
};



class Mp4TimeToSampleBox : public Mp4FullBox
{
public:
	Mp4TimeToSampleBox(const char boxtype[4] ) : Mp4FullBox(boxtype, 0, "\0\0\0" )  \
		{ entry_count=1; \
		headersize += sizeof(quint32); \
		size = BE( headersize ); }
	quint32 entry_count;
};

#endif /* H264VIDEO_H_ */
