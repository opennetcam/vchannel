#ifndef AVIFMT_H
#define AVIFMT_H

#include <QtGlobal>

/* 4 bytes */
typedef qint16  WORD;
typedef quint32 DWORD;
typedef qint32  LONG;

/* 1 byte */
typedef quint8 BYTE;

#define SWAP2(x) (((x>>8) & 0x00ff) |\
                  ((x<<8) & 0xff00))


#define SWAP4(x) (((((quint32)x)>>24) & 0x000000ff) |\
                  ((((quint32)x)>>8)  & 0x0000ff00) |\
                  ((((quint32)x)<<8)  & 0x00ff0000) |\
                  ((((quint32)x)<<24) & 0xff000000))

#define TODWORD(a,b,c,d) ( (((quint32)d <<24) & 0xff000000 ) | \
                           (((quint32)c <<16) & 0x00ff0000 ) | \
                           (((quint32)b <<8)  & 0x0000ff00 ) | \
                           ( (quint32)a       & 0x000000ff ) )

//#if __BYTE_ORDER == __BIG_ENDIAN
# define LI2(a) SWAP2((a))
# define LI4(a) SWAP4((a))
# define BI2(a) (a)
# define BI4(a) (a)
//#else
//# define LI2(a) (a)
//# define LI4(a) (a)
//# define BI2(a) SWAP2((a))
//# define BI4(a) SWAP4((a))
//#endif

// AVI tags
char TAG_RIFF[4] = {'R','I','F','F'};
char TAG_AVI[4]  = {'A','V','I',' '};
char TAG_LIST[4] = {'L','I','S','T'};
char TAG_JUNK[4] = {'J','U','N','K'};
char TAG_hdrl[4] = {'h','d','r','l'};
char TAG_avih[4] = {'a','v','i','h'};
char TAG_strl[4] = {'s','t','r','l'};
char TAG_strh[4] = {'s','t','r','h'};
char TAG_strf[4] = {'s','t','r','f'};
char TAG_indx[4] = {'i','n','d','x'};
char TAG_INFO[4] = {'I','N','F','O'};
char TAG_movi[4] = {'m','o','v','i'};
char TAG_odml[4] = {'o','d','m','l'};
char TAG_idx1[4] = {'i','d','x','1'};
char TAG_00db[4] = {'0','0','d','c'};
char TAG_01wb[4] = {'0','1','w','b'};

/* for use in AVI_avih.flags */
const DWORD AVIF_HASINDEX = 0x00000010;	/* index at end of file */
const DWORD AVIF_MUSTUSEINDEX = 0x00000020;
const DWORD AVIF_ISINTERLEAVED = 0x00000100;
const DWORD AVIF_TRUSTCKTYPE = 0x00000800;
const DWORD AVIF_WASCAPTUREFILE = 0x00010000;
const DWORD AVIF_COPYRIGHTED = 0x00020000;


struct AVI_avih
{
  DWORD	us_per_frame;       /* frame display rate (or 0L) */
  DWORD max_bytes_per_sec;	/* max. transfer rate */
  DWORD padding;            /* pad to multiples of this size; */
                            /* normally 2K */
  DWORD flags;
  DWORD tot_frames;         /* # frames in file */
  DWORD init_frames;
  DWORD streams;
  DWORD buff_sz;
  DWORD width;
  DWORD height;
  DWORD TimeScale;
  DWORD DataRate;
  DWORD StartTime;
  DWORD DataLength;
};


struct AVI_strh
{
  unsigned char type[4];        /* stream type */
  unsigned char handler[4];
  DWORD flags;
  DWORD priority;
  //DWORD language;
  DWORD init_frames;            /* initial frames (???) */
  DWORD scale;
  DWORD rate;
  DWORD start;
  DWORD length;
  DWORD buff_sz;                /* suggested buffer size */
  DWORD quality;
  DWORD sample_sz;
  DWORD rect;
};

typedef struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG biWidth;
  LONG biHeight;
  WORD biPlanes;
  WORD biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG biXPelsPerMeter;
  LONG biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct {
  WORD  wFormatTag;
  WORD  nChannels;
  DWORD nSamplesPerSec;
  DWORD nAvgBytesPerSec;
  WORD  nBlockAlign;
  WORD  wBitsPerSample;
  WORD  cbSize;
}WAVEFORMATEX;

struct AVI_odml
{
    unsigned char id[4];
    DWORD sz;
    DWORD frames;
};

/*
  AVI_list_hdr

  spc: a very ubiquitous AVI struct, used in list structures
       to specify type and length
*/

struct AVI_list_hdr
{
  unsigned char id[4];   /* "LIST" */
  DWORD sz;              /* size of owning struct minus 8 */
  unsigned char type[4]; /* type of list */
};


struct AVI_list_odml
{
  struct AVI_list_hdr list_hdr;

  unsigned char id[4];
  DWORD sz;
  DWORD frames;
};


struct AVI_list_strl
{
  struct AVI_list_hdr list_hdr;

  /* chunk strh */
  unsigned char strh_id[4];
  DWORD strh_sz;
  struct AVI_strh strh;

  /* chunk strf */
  unsigned char strf_id[4];
  DWORD strf_sz;
  BITMAPINFOHEADER v;

  /* list odml */
  //struct AVI_list_odml list_odml;
};


struct AVI_list_hdrl
{
  struct AVI_list_hdr list_hdr;

  /* chunk avih */
  unsigned char avih_id[4];
  DWORD avih_sz;
  struct AVI_avih avih;

  /* list strl */
  struct AVI_list_strl strl;
};


// version for audio
struct AVI_list_strl_a
{
  struct AVI_list_hdr list_hdr;

  /* chunk strh */
  unsigned char strh_id[4];
  DWORD strh_sz;
  struct AVI_strh strh;

  /* chunk strf */
  unsigned char strf_id[4];
  DWORD strf_sz;
  WAVEFORMATEX a;
};



struct AVI_list_hdrl_a
{
  struct AVI_list_hdr list_hdr;

  /* chunk avih */
  unsigned char avih_id[4];
  DWORD avih_sz;
  struct AVI_avih avih;

  /* list strl */
  struct AVI_list_strl_a strl;
};



#endif // AVIFMT_H
