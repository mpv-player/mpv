#ifndef MPLAYER_LIBWIN32_H
#define MPLAYER_LIBWIN32_H

#define VFW_E_NOT_RUNNING               0x80040226

#include <inttypes.h>

//#define FATAL(a)  // you don't need exception - if you want - just fill more code
#include "wine/mmreg.h"
#include "wine/winreg.h"
#include "wine/vfw.h"
#include "com.h"

typedef uint32_t fourcc_t;

/*
typedef struct FatalError
{
    FatalError();
    void PrintAll(void) {}
}FatalError;
*/

typedef struct CodecInfo
{
    char* dll;
    GUID* guid;
}CodecInfo;


typedef struct CImage // public  your_libvo_mem
{
    char* ptr;
    
    /*char* (*Data)();
    {
	return 0;
	// pointer to memory block
    }*/
    /*int (*Supported)(fourcc_t csp, int bits);
    {
	return true;
	// if you support such surface 
    }*/
}CImage;


#if 0
struct BitmapInfo : public BITMAPINFOHEADER
{
    void SetBits(int b) { return; /*fixme*/ }
    void SetSpace(int b) { return; /*fixme*/ }
};
#endif

typedef struct IAudioDecoder
{
    WAVEFORMATEX in_fmt;
    CodecInfo  record;
    /*(*IAudioDecoder)( CodecInfo * r, const WAVEFORMATEX* w);
    {
        memcpy(&this->record,r,sizeof(CodecInfo));
        in_fmt = *w;
    }*/
}IAudioDecoder;

/*
struct IAudioEncoder
{
    IAudioEncoder(const CodecInfo&, WAVEFORMATEX*) {}
    // you do not need this one...
};
*/

    enum CAPS
    {
	CAP_NONE = 0,
	CAP_YUY2 = 1,
	CAP_YV12 = 2,
	CAP_IYUV = 4,
	CAP_UYVY = 8,
	CAP_YVYU = 16,
	CAP_I420 = 32,
	CAP_YVU9 = 64,
	CAP_IF09 = 128,
    };
    enum DecodingMode
    {
	DIRECT = 0,
	REALTIME,
	REALTIME_QUALITY_AUTO,
    };
    enum DecodingState
    {
	STOP = 0,
	START,
    };

typedef struct BitmapInfo
{
    long 	biSize;
    long  	biWidth;
    long  	biHeight;
    short 	biPlanes;
    short 	biBitCount;
    long 	biCompression;
    long 	biSizeImage;
    long  	biXPelsPerMeter;
    long  	biYPelsPerMeter;
    long 	biClrUsed;
    long 	biClrImportant;
    int 	colors[3];    
} BitmapInfo;

typedef struct IVideoDecoder
{
    int VBUFSIZE;
    int QMARKHI;
    int QMARKLO;
    int DMARKHI;
    int DMARKLO;

    /*
    IVideoDecoder(CodecInfo& info, const BITMAPINFOHEADER& format) : record(info)
    {
        // implement init part
    }
    virtual ~IVideoDecoder();
    void Stop()
    {
    }
    void Start()
    {
    }
    */
    const CodecInfo record;
    int m_Mode;	// should we do precaching (or even change Quality on the fly)
    int m_State;
    int m_iDecpos;
    int m_iPlaypos;
    float m_fQuality;           // quality for the progress bar 0..1(best)
    int m_bCapable16b;

    BITMAPINFOHEADER* m_bh;	// format of input data (might be larger - e.g. huffyuv)
    BitmapInfo m_decoder;	// format of decoder output
    BitmapInfo m_obh;		// format of returned frames
}IVideoDecoder;

/*
struct IRtConfig
{
};
*/

// might be minimalized to contain just those which are needed by DS_VideoDecoder

#ifndef mmioFOURCC
#define mmioFOURCC( ch0, ch1, ch2, ch3 )				\
		( (long)(unsigned char)(ch0) | ( (long)(unsigned char)(ch1) << 8 ) |	\
		( (long)(unsigned char)(ch2) << 16 ) | ( (long)(unsigned char)(ch3) << 24 ) )
#endif /* mmioFOURCC */

/* OpenDivX */
#define fccMP4S	mmioFOURCC('M', 'P', '4', 'S')
#define fccmp4s	mmioFOURCC('m', 'p', '4', 's')
#define fccDIVX	mmioFOURCC('D', 'I', 'V', 'X')
#define fccdivx	mmioFOURCC('d', 'i', 'v', 'x')
#define fccDIV1	mmioFOURCC('D', 'I', 'V', '1')
#define fccdiv1	mmioFOURCC('d', 'i', 'v', '1')

/* DivX codecs */
#define fccDIV2 mmioFOURCC('D', 'I', 'V', '2')
#define fccdiv2 mmioFOURCC('d', 'i', 'v', '2')
#define fccDIV3 mmioFOURCC('D', 'I', 'V', '3')
#define fccdiv3 mmioFOURCC('d', 'i', 'v', '3')
#define fccDIV4 mmioFOURCC('D', 'I', 'V', '4')
#define fccdiv4 mmioFOURCC('d', 'i', 'v', '4')
#define fccDIV5 mmioFOURCC('D', 'I', 'V', '5')
#define fccdiv5 mmioFOURCC('d', 'i', 'v', '5')
#define fccDIV6 mmioFOURCC('D', 'I', 'V', '6')
#define fccdiv6 mmioFOURCC('d', 'i', 'v', '6')
#define fccMP41	mmioFOURCC('M', 'P', '4', '1')
#define fccmp41	mmioFOURCC('m', 'p', '4', '1')
#define fccMP43	mmioFOURCC('M', 'P', '4', '3')
#define fccmp43 mmioFOURCC('m', 'p', '4', '3')
/* old ms mpeg-4 codecs */
#define fccMP42	mmioFOURCC('M', 'P', '4', '2')
#define fccmp42	mmioFOURCC('m', 'p', '4', '2')
#define fccMPG4	mmioFOURCC('M', 'P', 'G', '4')
#define fccmpg4	mmioFOURCC('m', 'p', 'g', '4')
/* Windows media codecs */
#define fccWMV1 mmioFOURCC('W', 'M', 'V', '1')
#define fccwmv1 mmioFOURCC('w', 'm', 'v', '1')
#define fccWMV2 mmioFOURCC('W', 'M', 'V', '2')
#define fccwmv2 mmioFOURCC('w', 'm', 'v', '2')
#define fccMWV1 mmioFOURCC('M', 'W', 'V', '1')

/* Angel codecs */
#define fccAP41	mmioFOURCC('A', 'P', '4', '1')
#define fccap41	mmioFOURCC('a', 'p', '4', '1')
#define fccAP42	mmioFOURCC('A', 'P', '4', '2')
#define fccap42	mmioFOURCC('a', 'p', '4', '2')

/* other codecs	*/
#define fccIV31 mmioFOURCC('I', 'V', '3', '1')
#define fcciv31 mmioFOURCC('i', 'v', '3', '1')
#define fccIV32 mmioFOURCC('I', 'V', '3', '2')
#define fcciv32 mmioFOURCC('i', 'v', '3', '2')
#define fccIV41 mmioFOURCC('I', 'V', '4', '1')
#define fcciv41 mmioFOURCC('i', 'v', '4', '1')
#define fccIV50 mmioFOURCC('I', 'V', '5', '0')
#define fcciv50 mmioFOURCC('i', 'v', '5', '0')
#define fccI263 mmioFOURCC('I', '2', '6', '3')
#define fcci263 mmioFOURCC('i', '2', '6', '3')

#define fccMJPG mmioFOURCC('M', 'J', 'P', 'G')
#define fccmjpg mmioFOURCC('m', 'j', 'p', 'g')

#define fccHFYU mmioFOURCC('H', 'F', 'Y', 'U')

#define fcccvid mmioFOURCC('c', 'v', 'i', 'd')
#define fccdvsd mmioFOURCC('d', 'v', 's', 'd')

/* Ati codecs */
#define fccVCR2 mmioFOURCC('V', 'C', 'R', '2')
#define fccVCR1 mmioFOURCC('V', 'C', 'R', '1')
#define fccVYUY mmioFOURCC('V', 'Y', 'U', 'Y')
#define fccIYU9 mmioFOURCC('I', 'Y', 'U', '9') // it was defined as fccYVU9

/* Asus codecs */
#define fccASV1 mmioFOURCC('A', 'S', 'V', '1')
#define fccASV2 mmioFOURCC('A', 'S', 'V', '2')

/* Microsoft video */
#define fcccram mmioFOURCC('c', 'r', 'a', 'm')
#define fccCRAM mmioFOURCC('C', 'R', 'A', 'M')
#define fccMSVC mmioFOURCC('M', 'S', 'V', 'C')


#define fccMSZH mmioFOURCC('M', 'S', 'Z', 'H')

#define fccZLIB mmioFOURCC('Z', 'L', 'I', 'B')

#define fccTM20 mmioFOURCC('T', 'M', '2', '0')

#define fccYUV  mmioFOURCC('Y', 'U', 'V', ' ')
#define fccYUY2 mmioFOURCC('Y', 'U', 'Y', '2')
#define fccYV12 mmioFOURCC('Y', 'V', '1', '2')/* Planar mode: Y + V + U  (3 planes) */
#define fccI420 mmioFOURCC('I', '4', '2', '0')
#define fccIYUV mmioFOURCC('I', 'Y', 'U', 'V')/* Planar mode: Y + U + V  (3 planes) */
#define fccUYVY mmioFOURCC('U', 'Y', 'V', 'Y')/* Packed mode: U0+Y0+V0+Y1 (1 plane) */
#define fccYVYU mmioFOURCC('Y', 'V', 'Y', 'U')/* Packed mode: Y0+V0+Y1+U0 (1 plane) */
#define fccYVU9 mmioFOURCC('Y', 'V', 'U', '9')/* Planar 4:1:0 */
#define fccIF09 mmioFOURCC('I', 'F', '0', '9')/* Planar 4:1:0 + delta */

#endif /* MPLAYER_LIBWIN32_H */
