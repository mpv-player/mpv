#ifndef DS_GUIDS_H
#define DS_GUIDS_H

#include "com.h"
#include "wine/module.h"
#include "wine/windef.h"
#include "wine/vfw.h"

//#define Debug if(1)
#define Debug if(0)

typedef struct __attribute__((__packed__)) _MediaType
{
    GUID	majortype;		//0x0
    GUID	subtype;		//0x10
    int		bFixedSizeSamples;	//0x20
    int		bTemporalCompression;	//0x24
    unsigned long lSampleSize;		//0x28
    GUID	formattype;		//0x2c
    IUnknown*	pUnk;			//0x3c
    unsigned long cbFormat;		//0x40
    char*	pbFormat;		//0x44
} AM_MEDIA_TYPE;

typedef long long REFERENCE_TIME;

typedef struct __attribute__((__packed__)) RECT32
{
    int left, top, right, bottom;
} RECT32;

typedef struct __attribute__((__packed__)) tagVIDEOINFOHEADER
{
    RECT32            rcSource;          // The bit we really want to use
    RECT32            rcTarget;          // Where the video should go
    unsigned long     dwBitRate;         // Approximate bit data rate
    unsigned long     dwBitErrorRate;    // Bit error rate for this stream
    REFERENCE_TIME    AvgTimePerFrame;   // Average time per frame (100ns units)
    BITMAPINFOHEADER  bmiHeader;
    //int               reserved[3];
} VIDEOINFOHEADER;

typedef GUID CLSID;
typedef GUID IID;

extern const GUID IID_IBaseFilter;
extern const GUID IID_IEnumPins;
extern const GUID IID_IEnumMediaTypes;
extern const GUID IID_IMemInputPin;
extern const GUID IID_IMemAllocator;
extern const GUID IID_IMediaSample;
extern const GUID IID_DivxHidden;
extern const GUID IID_Iv50Hidden;
extern const GUID CLSID_DivxDecompressorCF;
extern const GUID IID_IDivxFilterInterface;
extern const GUID CLSID_IV50_Decoder;
extern const GUID CLSID_MemoryAllocator;
extern const GUID MEDIATYPE_Video;
extern const GUID GUID_NULL;
extern const GUID FORMAT_VideoInfo;
extern const GUID MEDIASUBTYPE_RGB1;
extern const GUID MEDIASUBTYPE_RGB4;
extern const GUID MEDIASUBTYPE_RGB8;
extern const GUID MEDIASUBTYPE_RGB565;
extern const GUID MEDIASUBTYPE_RGB555;
extern const GUID MEDIASUBTYPE_RGB24;
extern const GUID MEDIASUBTYPE_RGB32;
extern const GUID MEDIASUBTYPE_YUYV;
extern const GUID MEDIASUBTYPE_IYUV;
extern const GUID MEDIASUBTYPE_YVU9;
extern const GUID MEDIASUBTYPE_Y411;
extern const GUID MEDIASUBTYPE_Y41P;
extern const GUID MEDIASUBTYPE_YUY2;
extern const GUID MEDIASUBTYPE_YVYU;
extern const GUID MEDIASUBTYPE_UYVY;
extern const GUID MEDIASUBTYPE_Y211;
extern const GUID MEDIASUBTYPE_YV12;
extern const GUID MEDIASUBTYPE_I420;
extern const GUID MEDIASUBTYPE_IF09;

extern const GUID FORMAT_WaveFormatEx;
extern const GUID MEDIATYPE_Audio;
extern const GUID MEDIASUBTYPE_PCM;

#endif /* DS_GUIDS_H */
