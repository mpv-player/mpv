#ifndef __LIBWIN32_H
#define __LIBWIN32_H

#define VFW_E_INVALIDMEDIATYPE  	0x80040200
#define VFW_E_INVALIDSUBTYPE    	0x80040201
#define VFW_E_ALREADY_CONNECTED         0x80040204
#define VFW_E_FILTER_ACTIVE             0x80040205
#define VFW_E_NO_ACCEPTABLE_TYPES       0x80040207
#define VFW_E_NOT_CONNECTED             0x80040209
#define VFW_E_NO_ALLOCATOR              0x8004020A
#define VFW_E_NOT_RUNNING               0x80040226
#define VFW_E_TYPE_NOT_ACCEPTED         0x8004022A
#define VFW_E_SAMPLE_REJECTED           0x8004022B

#include <sys/types.h>
#include <inttypes.h>

#ifndef NOAVIFILE_HEADERS
#include <audiodecoder.h>
#include <audioencoder.h>
#include <videodecoder.h>
#include <videoencoder.h>
#include <except.h>
#include <fourcc.h>

#else
// code for mplayer team

#define FATAL(a)  // you don't need exception - if you want - just fill more code
#include <wine/mmreg.h>
#include <wine/winreg.h>
#include <wine/vfw.h>
#include <com.h>
#include <string>

typedef unsigned int fourcc_t;
struct FatalError
{
    FatalError();
    void PrintAll() {}
};

struct CodecInfo
{
    std::string dll;
    GUID guid;
};

struct CImage { // public  your_libvo_mem
    char* ptr;
    char* Data() { return ptr; }  // pointer to memory block
	/* if you support such surface: */
    static bool Supported(fourcc_t csp, int bits) { return true; }
};

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
#define fccYVU9 mmioFOURCC('I', 'Y', 'U', '9')

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


struct BitmapInfo : public BITMAPINFOHEADER
{
    int colors[3];

    void SetBitFields16(){	
	biSize=sizeof(BITMAPINFOHEADER)+12;
	biCompression=3;//BI_BITFIELDS
	biBitCount=16;
	biSizeImage=abs((int)(2*biWidth*biHeight));
	colors[0]=0xF800;
	colors[1]=0x07E0;
	colors[2]=0x001F;
    }	
    void SetBitFields15(){	
	biSize=sizeof(BITMAPINFOHEADER)+12;
	biCompression=3;//BI_BITFIELDS
	biBitCount=16;
	biSizeImage=abs((int)(2*biWidth*biHeight));
	colors[0]=0x7C00;
	colors[1]=0x03E0;
	colors[2]=0x001F;
    }	
    void SetRGB(){
	biSize = sizeof(BITMAPINFOHEADER);
	biCompression = 0;	//BI_RGB
	//biHeight = labs(biHeight);
	biSizeImage = labs(biWidth * biHeight) * ((biBitCount + 7) / 8);
    }
    void SetBits(int bits) { 
        switch (bits){
	    case 15: SetBitFields15();break;
	    case 16: SetBitFields16();break;
	    default: biBitCount = bits; SetRGB();break;
        }
    }
    void SetSpace(int csp,int bits) {
	biSize = sizeof(BITMAPINFOHEADER);
	biCompression=csp;
	biBitCount=bits;
	biSizeImage=labs(biBitCount*biWidth*biHeight)>>3;
    }
    void SetSpace(int csp) {
	int bits=0;
	switch(csp){
	case fccYUV:
	    bits=24;break;
	case fccYUY2:
	case fccUYVY:
	case fccYVYU:
	    bits=16;break;
	case fccYV12:
	case fccIYUV:
	case fccI420:
	    bits=12;break;
	}
	if (csp != 0 && csp != 3 && biHeight > 0)
    	    biHeight *= -1; // YUV formats uses should have height < 0
	SetSpace(csp,bits);
    }

};

struct IAudioDecoder
{
    WAVEFORMATEX in_fmt;
    const CodecInfo& record;
    IAudioDecoder(const CodecInfo& r, const WAVEFORMATEX* w) : record(r)
    {
        in_fmt = *w;
    }
};

struct IAudioEncoder
{
    IAudioEncoder(const CodecInfo&, WAVEFORMATEX*) {}
    // you do not need this one...
};

struct IVideoDecoder
{
    int VBUFSIZE;
    int QMARKHI;
    int QMARKLO;
    int DMARKHI;
    int DMARKLO;

    enum CAPS
    {
	CAP_NONE = 0,
	CAP_YUY2 = 1,
	CAP_YV12 = 2,
	CAP_IYUV = 4,
	CAP_UYVY = 8,
	CAP_YVYU = 16,
	CAP_I420 = 32,
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
    IVideoDecoder(const CodecInfo& info, const BITMAPINFOHEADER& format) : record(info)
    {
        // implement init part
     unsigned bihs = (format.biSize < (int) sizeof(BITMAPINFOHEADER)) ?
	  sizeof(BITMAPINFOHEADER) : format.biSize;
     m_bh = (BITMAPINFOHEADER*) new char[bihs];
     memcpy(m_bh, &format, bihs);
     m_State = STOP;
     //m_pFrame = 0;
     m_Mode = DIRECT;
     m_iDecpos = 0;
     m_iPlaypos = -1;
     m_fQuality = 0.0f;
     m_bCapable16b = true;

    }
    virtual ~IVideoDecoder(){};
    virtual void StartInternal()=0;
    virtual void StopInternal()=0;
    void Stop(){ StopInternal();}
    void Start(){StartInternal();}

    const CodecInfo& record;
    DecodingMode m_Mode;	// should we do precaching (or even change Quality on the fly)
    DecodingState m_State;
    int m_iDecpos;
    int m_iPlaypos;
    float m_fQuality;           // quality for the progress bar 0..1(best)
    bool m_bCapable16b;

    BITMAPINFOHEADER* m_bh;	// format of input data (might be larger - e.g. huffyuv)
    BitmapInfo m_decoder;	// format of decoder output
    BitmapInfo m_obh;		// format of returned frames
};

struct IRtConfig
{
};



#endif

#endif
