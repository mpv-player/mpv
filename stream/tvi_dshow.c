/*
 * TV support under Win32
 *
 * (C) 2007 Vladimir Voroshilov <voroshil@gmail.com>
 *
 * Based on tvi_dummy.c with help of tv.c, tvi_v4l2.c code.
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 *     WARNING: This is alpha code!
 *
 *     Abilities:
 *     * Watching TV under Win32 using WDM Capture driver and DirectShow
 *     * Grabbing synchronized audio/video with mencoder (synchronization is beeing done by DirectShow)
 *     * If device driver provides IAMStreamConfig interface, user can choose width/height with "-tv height=<h>:width=<w>"
 *     * Adjusting BRIGHTNESS,HUE,SATURATION,CONTRAST if supported by device
 *     * Selecting Tuner,S-Video,... as media source
 *     * User can select used video capture device, passing -tv device=<dev#>
 *     * User can select used audio input, passing -tv audioid=<input#>
 *
 *     options which will not be implemented (probably sometime in future, if possible):
 *     * alsa
 *     * mjpeg
 *     * decimation=<1|2|4>
 *     * quality=<0\-100>
 *     * forceaudio
 *     * forcechan=<1\-2>
 *     * [volume|bass|treble|balance]
 *
 *     Works with:
 *       - LifeView FlyTV Prime 34FM (SAA7134 based) with driver from Ivan Uskov
 *     Partially works with:
 *       - ATI 9200 VIVO based card
 *       - ATI AIW 7500
 *       - nVidia Ti-4400
 * 
 *     Known bugs:
 *     * stream goes with 24.93 FPS (NTSC), while reporting 25 FPS (PAL) ?!
 *     * direct set frequency call does not work ('Insufficient Buffer' error)
 *     * audio stream goes with about 1 sample/sec rate when capturing sound from audio card
 *
 *     TODO:
 *     * check audio with small buffer on vivo !!!
 *     * norm for IAMVideoDecoder and for IAMTVtuner - differs !!
 *     * check how to change TVFormat on VIVO card without tuner
 *     * Flip image upside-down for RGB formats.
 *     *
 *     * remove debug sleep()
 *     * Add some notes to methods' parameters
 *     * refactor console messages
 *     * check using header files and keep only needed
 *     * add additional comments to methods' bodies
 *
 */


/// \ingroup tvi_dshow

#include "config.h"

#include <stdio.h>
#include "libmpcodecs/img_format.h"
#include "libaf/af_format.h"
#include "help_mp.h"
#include "osdep/timer.h"


#include "tv.h"
#include "mp_msg.h"
#include "frequencies.h"


#include "tvi_dshow.h"

static tvi_handle_t *tvi_init_dshow(tv_param_t* tv_param);

/*
*---------------------------------------------------------------------------------------
*
*   Data structures
*
*---------------------------------------------------------------------------------------
*/
/** 
    information about this file
*/
const tvi_info_t tvi_info_dshow = {
    tvi_init_dshow,
    "DirectShow TV",
    "dshow",
    "Vladimir Voroshilov",
    "Very experimental!! Use with caution"
};


/**
ringbuffer related info
*/
typedef struct {
    CRITICAL_SECTION *pMutex;	///< pointer to critical section (mutex)
    char **ringbuffer;		///< ringbuffer array
    double*dpts;		///< samples' timestamps

    int buffersize;		///< size of buffer in blocks
    int blocksize;		///< size of individual block
    int head;			///< index of first valid sample
    int tail;			///< index of last valid sample
    int count;			///< count of valid samples in ringbuffer
    double tStart;              ///< pts of first sample (first sample should have pts 0)
} grabber_ringbuffer_t;

typedef enum { unknown, video, audio, vbi } stream_type;

/**
     CSampleGrabberCD definition
*/
typedef struct CSampleGrabberCB {
    ISampleGrabberCBVtbl *lpVtbl;
    int refcount;
    GUID interfaces[2];
    grabber_ringbuffer_t *pbuf;
} CSampleGrabberCB;

/**
    Chain related structure
 */
typedef struct {
    stream_type type;                  ///< stream type
    const GUID* majortype;                   ///< GUID of major mediatype (video/audio/vbi)
    const GUID* pin_category;          ///< pin category (pointer to one of PIN_CATEGORY_*)

    IBaseFilter *pCaptureFilter;       ///< capture device filter
    IAMStreamConfig *pStreamConfig;    ///< for configuring stream
    ISampleGrabber *pSG;               ///< ISampleGrabber interface of SampleGrabber filter
    IBaseFilter *pSGF;                 ///< IBaseFilter interface of SampleGrabber filter
    IPin *pCapturePin;                 ///< output capture pin
    IPin *pSGIn;                       ///< input pin of SampleGrabber filter

    grabber_ringbuffer_t *rbuf;        ///< sample frabber data
    CSampleGrabberCB* pCSGCB;          ///< callback object

    AM_MEDIA_TYPE *pmt;                ///< stream properties.
    int nFormatUsed;                   ///< index of used format
    AM_MEDIA_TYPE **arpmt;             ///< available formats
    void** arStreamCaps;               ///< VIDEO_STREAM_CONFIG_CAPS or AUDIO_STREAM_CONFIG_CAPS
} chain_t;

typedef struct {
    int dev_index;              ///< capture device index in device list (defaul: 0, first available device)
    int adev_index;             ///< audio capture device index in device list (default: -1, not used)
    int immediate_mode;         ///< immediate mode (no sound capture)
    int state;                  ///< state: 1-filter graph running, 0-filter graph stopped
    int direct_setfreq_call;    ///< 0-find nearest channels from system channel list(workaround),1-direct call to set frequency
    int direct_getfreq_call;    ///< 0-find frequncy from frequency table (workaround),1-direct call to get frequency

    int fcc;                    ///< used video format code (FourCC)
    int width;                  ///< picture width (default: auto) 
    int height;                 ///< picture height (default: auto)

    int channels;               ///< number of audio channels (default: auto)
    int samplerate;             ///< audio samplerate (default: auto)

    long *freq_table;           ///< frequency table (in Hz)
    int freq_table_len;         ///< length of freq table
    int first_channel;          ///< channel number of first entry in freq table
    int input;                  ///< used input

    chain_t* chains[3];                     ///< chains' data (0-video, 1-audio, 2-vbi)

    IAMTVTuner *pTVTuner;                   ///< interface for tuner device
    IGraphBuilder *pGraph;                  ///< filter graph
    ICaptureGraphBuilder2 *pBuilder;        ///< graph builder
    IMediaControl *pMediaControl;           ///< interface for controlling graph (start, stop,...)
    IAMVideoProcAmp *pVideoProcAmp;         ///< for adjusting hue,saturation,etc
    IAMCrossbar *pCrossbar;                 ///< for selecting input (Tuner,Composite,S-Video,...)
    DWORD dwRegister;                       ///< allow graphedit to connect to our graph
    void *priv_vbi;                         ///< private VBI data structure
    tt_stream_props tsp;                    ///< data for VBI initialization

    tv_param_t* tv_param;                   ///< TV parameters
} priv_t;

#include "tvi_def.h"

/**
 country table entry structure (for loading freq table stored in kstvtuner.ax

 \note
 structure have to be 2-byte aligned and have 10-byte length!!
*/
typedef struct __attribute__((__packed__)) {
    WORD CountryCode;		///< Country code
    WORD CableFreqTable;	///< index of resource with frequencies for cable channels
    WORD BroadcastFreqTable;	///< index of resource with frequencies for broadcast channels
    DWORD VideoStandard;	///< used video standard 
} TRCCountryList;
/**
    information about image formats
*/
typedef struct {
    uint32_t fmt;		///< FourCC
    const GUID *subtype;	///< DirectShow's subtype
    int nBits;			///< number of bits
    int nCompression;		///< complression
    int tail;			///< number of additional bytes followed VIDEOINFOHEADER structure
} img_fmt;

/*
*---------------------------------------------------------------------------------------
*
*   Methods forward declaration
*
*---------------------------------------------------------------------------------------
*/
static HRESULT init_ringbuffer(grabber_ringbuffer_t * rb, int buffersize,
			       int blocksize);
static HRESULT show_filter_info(IBaseFilter * pFilter);
#if 0 
//defined in current MinGW release
HRESULT STDCALL GetRunningObjectTable(DWORD, IRunningObjectTable **);
HRESULT STDCALL CreateItemMoniker(LPCOLESTR, LPCOLESTR, IMoniker **);
#endif
static CSampleGrabberCB *CSampleGrabberCB_Create(grabber_ringbuffer_t *
						 pbuf);
static int set_crossbar_input(priv_t * priv, int input);
static int subtype2imgfmt(const GUID * subtype);

/*
*---------------------------------------------------------------------------------------
*
*  Global constants and variables
*
*---------------------------------------------------------------------------------------
*/
/**
    lookup tables for physical connector types
 */
static const struct {
    long type;
    char *name;
} tv_physcon_types[]={
    {PhysConn_Video_Tuner,           "Tuner"          },
    {PhysConn_Video_Composite,       "Composite"      },
    {PhysConn_Video_SVideo,          "S-Video"        },
    {PhysConn_Video_RGB,             "RGB"            },
    {PhysConn_Video_YRYBY,           "YRYBY"          },
    {PhysConn_Video_SerialDigital,   "SerialDigital"  },
    {PhysConn_Video_ParallelDigital, "ParallelDigital"},
    {PhysConn_Video_VideoDecoder,    "VideoDecoder"   },
    {PhysConn_Video_VideoEncoder,    "VideoEncoder"   },
    {PhysConn_Video_SCART,           "SCART"          },
    {PhysConn_Video_Black,           "Blaack"         },
    {PhysConn_Audio_Tuner,           "Tuner"          },
    {PhysConn_Audio_Line,            "Line"           },
    {PhysConn_Audio_Mic,             "Mic"            },
    {PhysConn_Audio_AESDigital,      "AESDiital"      },
    {PhysConn_Audio_SPDIFDigital,    "SPDIFDigital"   },
    {PhysConn_Audio_AudioDecoder,    "AudioDecoder"   },
    {PhysConn_Audio_SCSI,            "SCSI"           },
    {PhysConn_Video_SCSI,            "SCSI"           },
    {PhysConn_Audio_AUX,             "AUX"            },
    {PhysConn_Video_AUX,             "AUX"            },
    {PhysConn_Audio_1394,            "1394"           },
    {PhysConn_Video_1394,            "1394"           },
    {PhysConn_Audio_USB,             "USB"            },
    {PhysConn_Video_USB,             "USB"            },
    {-1,                              NULL            }
};

static const struct {
    char *chanlist_name;
    int country_code;
} tv_chanlist2country[]={
    {"us-bcast",     1},
    {"russia",       7},
    {"argentina",   54},
    {"japan-bcast", 81},
    {"china-bcast", 86},
    {"southafrica", 27},
    {"australia",   61},
    {"ireland",    353},
    {"france",      33},
    {"italy",       39},
    {"newzealand",  64},
    //directshow table uses eastern europe freq table for russia
    {"europe-east",  7},
    //directshow table uses western europe freq table for germany
    {"europe-west", 49},
    /* cable channels */
    {"us-cable",     1},
    {"us-cable-hrc", 1},
    {"japan-cable", 81},
    //default is USA
    {NULL,           1} 
};

/**
    array, contains information about various supported (i hope) image formats
*/
static const img_fmt img_fmt_list[] = {
    {IMGFMT_YUY2,  &MEDIASUBTYPE_YUY2,   16, IMGFMT_YUY2,  0},
    {IMGFMT_YV12,  &MEDIASUBTYPE_YV12,   12, IMGFMT_YV12,  0},
    {IMGFMT_IYUV,  &MEDIASUBTYPE_IYUV,   12, IMGFMT_IYUV,  0},
    {IMGFMT_I420,  &MEDIASUBTYPE_I420,   12, IMGFMT_I420,  0},
    {IMGFMT_UYVY,  &MEDIASUBTYPE_UYVY,   16, IMGFMT_UYVY,  0},
    {IMGFMT_YVYU,  &MEDIASUBTYPE_YVYU,   16, IMGFMT_YVYU,  0},
    {IMGFMT_YVU9,  &MEDIASUBTYPE_YVU9,    9, IMGFMT_YVU9,  0},
    {IMGFMT_BGR32, &MEDIASUBTYPE_RGB32,  32,           0,  0},
    {IMGFMT_BGR24, &MEDIASUBTYPE_RGB24,  24,           0,  0},
    {IMGFMT_BGR16, &MEDIASUBTYPE_RGB565, 16,           3, 12},
    {IMGFMT_BGR15, &MEDIASUBTYPE_RGB555, 16,           3, 12},
    {0,            &GUID_NULL,            0,           0,  0}
};

#define TV_NORMS_COUNT 19
static const struct {
    long index;
    char *name;
} tv_norms[TV_NORMS_COUNT] = {
    {
    AnalogVideo_NTSC_M, "ntsc-m"}, {
    AnalogVideo_NTSC_M_J, "ntsc-mj"}, {
    AnalogVideo_NTSC_433, "ntsc-433"}, {
    AnalogVideo_PAL_B, "pal-b"}, {
    AnalogVideo_PAL_D, "pal-d"}, {
    AnalogVideo_PAL_G, "pal-g"}, {
    AnalogVideo_PAL_H, "pal-h"}, {
    AnalogVideo_PAL_I, "pal-i"}, {
    AnalogVideo_PAL_M, "pal-m"}, {
    AnalogVideo_PAL_N, "pal-n"}, {
    AnalogVideo_PAL_60, "pal-60"}, {
    AnalogVideo_SECAM_B, "secam-b"}, {
    AnalogVideo_SECAM_D, "secam-d"}, {
    AnalogVideo_SECAM_G, "secam-g"}, {
    AnalogVideo_SECAM_H, "secam-h"}, {
    AnalogVideo_SECAM_K, "secam-k"}, {
    AnalogVideo_SECAM_K1, "secam-k1"}, {
    AnalogVideo_SECAM_L, "secam-l"}, {
    AnalogVideo_SECAM_L1, "secam-l1"}
};
static long tv_available_norms[TV_NORMS_COUNT];
static int tv_available_norms_count = 0;


static long *tv_available_inputs;
static int tv_available_inputs_count = 0;

/*
*---------------------------------------------------------------------------------------
*
*  Various GUID definitions
*
*---------------------------------------------------------------------------------------
*/
/// CLSID definitions (used for CoCreateInstance call)
DEFINE_GUID(CLSID_SampleGrabber, 0xC1F400A0, 0x3F08, 0x11d3, 0x9F, 0x0B,
	    0x00, 0x60, 0x08, 0x03, 0x9E, 0x37);
DEFINE_GUID(CLSID_NullRenderer, 0xC1F400A4, 0x3F08, 0x11d3, 0x9F, 0x0B,
	    0x00, 0x60, 0x08, 0x03, 0x9E, 0x37);
DEFINE_GUID(CLSID_SystemDeviceEnum, 0x62BE5D10, 0x60EB, 0x11d0, 0xBD, 0x3B,
	    0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86);
DEFINE_GUID(CLSID_CaptureGraphBuilder2, 0xBF87B6E1, 0x8C27, 0x11d0, 0xB3,
	    0xF0, 0x00, 0xAA, 0x00, 0x37, 0x61, 0xC5);
DEFINE_GUID(CLSID_VideoInputDeviceCategory, 0x860BB310, 0x5D01, 0x11d0,
	    0xBD, 0x3B, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86);
DEFINE_GUID(CLSID_AudioInputDeviceCategory, 0x33d9a762, 0x90c8, 0x11d0,
	    0xbd, 0x43, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(CLSID_FilterGraph, 0xe436ebb3, 0x524f, 0x11ce, 0x9f, 0x53,
	    0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(CLSID_SystemClock, 0xe436ebb1, 0x524f, 0x11ce, 0x9f, 0x53,
	    0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
#ifdef NOT_USED
DEFINE_GUID(CLSID_CaptureGraphBuilder, 0xBF87B6E0, 0x8C27, 0x11d0, 0xB3,
	    0xF0, 0x00, 0xAA, 0x00, 0x37, 0x61, 0xC5);
DEFINE_GUID(CLSID_VideoPortManager, 0x6f26a6cd, 0x967b, 0x47fd, 0x87, 0x4a,
	    0x7a, 0xed, 0x2c, 0x9d, 0x25, 0xa2);
DEFINE_GUID(IID_IPin, 0x56a86891, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00, 0x20,
	    0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(IID_ICaptureGraphBuilder, 0xbf87b6e0, 0x8c27, 0x11d0, 0xb3,
	    0xf0, 0x00, 0xaa, 0x00, 0x37, 0x61, 0xc5);
DEFINE_GUID(IID_IFilterGraph, 0x56a8689f, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00,
	    0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(PIN_CATEGORY_PREVIEW, 0xfb6c4282, 0x0353, 0x11d1, 0x90, 0x5f,
	    0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
#endif

/// IID definitions (used for QueryInterface call)
DEFINE_GUID(IID_IReferenceClock, 0x56a86897, 0x0ad4, 0x11ce, 0xb0, 0x3a,
	    0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(IID_IAMBufferNegotiation, 0x56ED71A0, 0xAF5F, 0x11D0, 0xB3, 0xF0,
            0x00, 0xAA, 0x00, 0x37, 0x61, 0xC5);
DEFINE_GUID(IID_IKsPropertySet, 0x31efac30, 0x515c, 0x11d0, 0xa9, 0xaa,
	    0x00, 0xaa, 0x00, 0x61, 0xbe, 0x93);
DEFINE_GUID(IID_ISampleGrabber, 0x6B652FFF, 0x11FE, 0x4fce, 0x92, 0xAD,
	    0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F);
DEFINE_GUID(IID_ISampleGrabberCB, 0x0579154A, 0x2B53, 0x4994, 0xB0, 0xD0,
	    0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85);
DEFINE_GUID(IID_ICaptureGraphBuilder2, 0x93e5a4e0, 0x2d50, 0x11d2, 0xab,
	    0xfa, 0x00, 0xa0, 0xc9, 0xc6, 0xe3, 0x8d);
DEFINE_GUID(IID_ICreateDevEnum, 0x29840822, 0x5b84, 0x11d0, 0xbd, 0x3b,
	    0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86);
DEFINE_GUID(IID_IGraphBuilder, 0x56a868a9, 0x0ad4, 0x11ce, 0xb0, 0x3a,
	    0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(IID_IAMVideoProcAmp, 0xC6E13360, 0x30AC, 0x11d0, 0xA1, 0x8C,
	    0x00, 0xA0, 0xC9, 0x11, 0x89, 0x56);
DEFINE_GUID(IID_IVideoWindow, 0x56a868b4, 0x0ad4, 0x11ce, 0xb0, 0x3a, 0x00,
	    0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(IID_IMediaControl, 0x56a868b1, 0x0ad4, 0x11ce, 0xb0, 0x3a,
	    0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID(IID_IAMTVTuner, 0x211A8766, 0x03AC, 0x11d1, 0x8D, 0x13, 0x00,
	    0xAA, 0x00, 0xBD, 0x83, 0x39);
DEFINE_GUID(IID_IAMCrossbar, 0xc6e13380, 0x30ac, 0x11d0, 0xa1, 0x8c, 0x00,
	    0xa0, 0xc9, 0x11, 0x89, 0x56);
DEFINE_GUID(IID_IAMStreamConfig, 0xc6e13340, 0x30ac, 0x11d0, 0xa1, 0x8c,
	    0x00, 0xa0, 0xc9, 0x11, 0x89, 0x56);
DEFINE_GUID(IID_IAMAudioInputMixer, 0x54C39221, 0x8380, 0x11d0, 0xB3, 0xF0,
	    0x00, 0xAA, 0x00, 0x37, 0x61, 0xC5);
DEFINE_GUID(IID_IAMTVAudio, 0x83EC1C30, 0x23D1, 0x11d1, 0x99, 0xE6, 0x00,
	    0xA0, 0xC9, 0x56, 0x02, 0x66);
DEFINE_GUID(IID_IAMAnalogVideoDecoder, 0xC6E13350, 0x30AC, 0x11d0, 0xA1,
	    0x8C, 0x00, 0xA0, 0xC9, 0x11, 0x89, 0x56);
DEFINE_GUID(IID_IPropertyBag, 0x55272a00, 0x42cb, 0x11ce, 0x81, 0x35, 0x00,
	    0xaa, 0x00, 0x4b, 0xb8, 0x51);
DEFINE_GUID(PIN_CATEGORY_CAPTURE, 0xfb6c4281, 0x0353, 0x11d1, 0x90, 0x5f,
	    0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
DEFINE_GUID(PIN_CATEGORY_VIDEOPORT, 0xfb6c4285, 0x0353, 0x11d1, 0x90, 0x5f,
	    0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
DEFINE_GUID(PIN_CATEGORY_PREVIEW, 0xfb6c4282, 0x0353, 0x11d1, 0x90, 0x5f, 
            0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
DEFINE_GUID(PIN_CATEGORY_VBI, 0xfb6c4284, 0x0353, 0x11d1, 0x90, 0x5f,
            0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
DEFINE_GUID(PROPSETID_TUNER, 0x6a2e0605, 0x28e4, 0x11d0, 0xa1, 0x8c, 0x00,
	    0xa0, 0xc9, 0x11, 0x89, 0x56);
DEFINE_GUID(MEDIATYPE_VBI,   0xf72a76e1, 0xeb0a, 0x11d0, 0xac, 0xe4, 0x00,
            0x00, 0xc0, 0xcc, 0x16, 0xba);

#define INSTANCEDATA_OF_PROPERTY_PTR(x) (((KSPROPERTY*)(x)) + 1)
#define INSTANCEDATA_OF_PROPERTY_SIZE(x) (sizeof((x)) - sizeof(KSPROPERTY))

#define DEVICE_NAME_MAX_LEN 2000

/*---------------------------------------------------------------------------------------
*  Methods, called only from this file
*---------------------------------------------------------------------------------------*/

void set_buffer_preference(int nDiv,WAVEFORMATEX* pWF,IPin* pOutPin,IPin* pInPin){
    ALLOCATOR_PROPERTIES prop;
    IAMBufferNegotiation* pBN;
    HRESULT hr;

    prop.cbAlign = -1;
    prop.cbBuffer = pWF->nAvgBytesPerSec/nDiv;
    if (!prop.cbBuffer)
        prop.cbBuffer = 1;
    prop.cbBuffer += pWF->nBlockAlign - 1;
    prop.cbBuffer -= prop.cbBuffer % pWF->nBlockAlign;
    prop.cbPrefix = -1;
    prop.cBuffers = -1;    

    hr=OLE_QUERYINTERFACE(pOutPin,IID_IAMBufferNegotiation,pBN);
    if(FAILED(hr))
        mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: pOutPin->QueryInterface(IID_IAMBufferNegotiation) Error: 0x%x\n",(unsigned int)hr);
    else{
        hr=OLE_CALL_ARGS(pBN,SuggestAllocatorProperties,&prop);
        if(FAILED(hr))
            mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow:pOutPin->SuggestAllocatorProperties Error:0x%x\n",(unsigned int)hr);
        OLE_RELEASE_SAFE(pBN);
    }
    hr=OLE_QUERYINTERFACE(pInPin,IID_IAMBufferNegotiation,pBN);
    if(FAILED(hr))
        mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: pInPin->QueryInterface(IID_IAMBufferNegotiation) Error: 0x%x",(unsigned int)hr);
    else{
        hr=OLE_CALL_ARGS(pBN,SuggestAllocatorProperties,&prop);
        if(FAILED(hr))
            mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: pInPit->SuggestAllocatorProperties Error:0x%x\n",(unsigned int)hr);
        OLE_RELEASE_SAFE(pBN);
    }
}
/*
*---------------------------------------------------------------------------------------
*
*  CSampleGrabberCD class. Used for receiving samples from DirectShow.
*
*---------------------------------------------------------------------------------------
*/
/// CSampleGrabberCD destructor
static void CSampleGrabberCB_Destroy(CSampleGrabberCB * This)
{
    free(This->lpVtbl);
    free(This);
}

/// CSampleGrabberCD IUnknown interface methods implementation
static long STDCALL CSampleGrabberCB_QueryInterface(ISampleGrabberCB *
						    This,
						    const GUID * riid,
						    void **ppvObject)
{
    CSampleGrabberCB *me = (CSampleGrabberCB *) This;
    GUID *r;
    unsigned int i = 0;
    Debug printf("CSampleGrabberCB_QueryInterface(%p) called\n", This);
    if (!ppvObject)
	return E_POINTER;
    for (r = me->interfaces;
	 i < sizeof(me->interfaces) / sizeof(me->interfaces[0]); r++, i++)
	if (!memcmp(r, riid, sizeof(*r))) {
	    OLE_CALL(This, AddRef);
	    *ppvObject = This;
	    return 0;
	}
    Debug printf("Query failed! (GUID: 0x%x)\n", *(unsigned int *) riid);
    return E_NOINTERFACE;
}

static long STDCALL CSampleGrabberCB_AddRef(ISampleGrabberCB * This)
{
    CSampleGrabberCB *me = (CSampleGrabberCB *) This;
    Debug printf("CSampleGrabberCB_AddRef(%p) called (ref:%d)\n", This,
        	 me->refcount);
    return ++(me->refcount);
}

static long STDCALL CSampleGrabberCB_Release(ISampleGrabberCB * This)
{
    CSampleGrabberCB *me = (CSampleGrabberCB *) This;
    Debug printf("CSampleGrabberCB_Release(%p) called (new ref:%d)\n",
		 This, me->refcount - 1);
    if (--(me->refcount) == 0)
	CSampleGrabberCB_Destroy(me);
    return 0;
}


HRESULT STDCALL CSampleGrabberCB_BufferCB(ISampleGrabberCB * This,
					  double SampleTime,
					  BYTE * pBuffer, long lBufferLen)
{
    CSampleGrabberCB *this = (CSampleGrabberCB *) This;
    grabber_ringbuffer_t *rb = this->pbuf;

    if (!lBufferLen)
	return E_FAIL;

    if (!rb->ringbuffer) {
	rb->buffersize /= lBufferLen;
	if (init_ringbuffer(rb, rb->buffersize, lBufferLen) != S_OK)
	    return E_FAIL;
    }
    mp_msg(MSGT_TV, MSGL_DBG4,
	   "tvi_dshow: BufferCB(%p): len=%ld ts=%f\n", This, lBufferLen, SampleTime);
    EnterCriticalSection(rb->pMutex);
    if (rb->count >= rb->buffersize) {
	rb->head = (rb->head + 1) % rb->buffersize;
	rb->count--;
    }

    memcpy(rb->ringbuffer[rb->tail], pBuffer,
	   lBufferLen < rb->blocksize ? lBufferLen : rb->blocksize);
    rb->dpts[rb->tail] =  SampleTime;
    rb->tail = (rb->tail + 1) % rb->buffersize;
    rb->count++;
    LeaveCriticalSection(rb->pMutex);

    return S_OK;
}

/// wrapper. directshow does the same when BufferCB callback is requested
HRESULT STDCALL CSampleGrabberCB_SampleCB(ISampleGrabberCB * This,
					  double SampleTime,
					  LPMEDIASAMPLE pSample)
{
    char* buf;
    long len;
    long long tStart,tEnd;
    HRESULT hr;
    grabber_ringbuffer_t *rb = ((CSampleGrabberCB*)This)->pbuf;

    len=OLE_CALL(pSample,GetSize);
    tStart=tEnd=0;
    hr=OLE_CALL_ARGS(pSample,GetTime,&tStart,&tEnd);
    if(FAILED(hr)){
        return hr;
    }
    mp_msg(MSGT_TV, MSGL_DBG4,"tvi_dshow: SampleCB(%p): %d/%d %f\n", This,rb->count,rb->buffersize,1e-7*tStart);
    hr=OLE_CALL_ARGS(pSample,GetPointer,(void*)&buf);
    if(FAILED(hr)){
        return hr;
    }
    hr=CSampleGrabberCB_BufferCB(This,1e-7*tStart,buf,len);
    return hr;

}

/// main grabbing routine
static CSampleGrabberCB *CSampleGrabberCB_Create(grabber_ringbuffer_t *
						 pbuf)
{
    CSampleGrabberCB *This = malloc(sizeof(CSampleGrabberCB));
    if (!This)
	return NULL;

    This->lpVtbl = malloc(sizeof(ISampleGrabberVtbl));
    if (!This->lpVtbl) {
	CSampleGrabberCB_Destroy(This);
	return NULL;
    }
    This->refcount = 1;
    This->lpVtbl->QueryInterface = CSampleGrabberCB_QueryInterface;
    This->lpVtbl->AddRef = CSampleGrabberCB_AddRef;
    This->lpVtbl->Release = CSampleGrabberCB_Release;
    This->lpVtbl->SampleCB = CSampleGrabberCB_SampleCB;
    This->lpVtbl->BufferCB = CSampleGrabberCB_BufferCB;

    This->interfaces[0] = IID_IUnknown;
    This->interfaces[1] = IID_ISampleGrabberCB;

    This->pbuf = pbuf;

    return This;
}

/*
*---------------------------------------------------------------------------------------
*
*  ROT related methods (register, unregister)
*
*---------------------------------------------------------------------------------------
*/
/** 
Registering graph in ROT. User will be able to connect to graph from GraphEdit.
*/
static HRESULT AddToRot(IUnknown * pUnkGraph, DWORD * pdwRegister)
{
    IMoniker *pMoniker;
    IRunningObjectTable *pROT;
    WCHAR wsz[256];
    HRESULT hr;

    if (FAILED(GetRunningObjectTable(0, &pROT))) {
	return E_FAIL;
    }
    wsprintfW(wsz, L"FilterGraph %08x pid %08x", (DWORD_PTR) pUnkGraph,
	      GetCurrentProcessId());
    hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr)) {
	hr = OLE_CALL_ARGS(pROT, Register, ROTFLAGS_REGISTRATIONKEEPSALIVE,
		       pUnkGraph, pMoniker, pdwRegister);
	OLE_RELEASE_SAFE(pMoniker);
    }
    OLE_RELEASE_SAFE(pROT);
    return hr;
}

/// Unregistering graph in ROT
static void RemoveFromRot(DWORD dwRegister)
{
    IRunningObjectTable *pROT;
    if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) {
	OLE_CALL_ARGS(pROT, Revoke, dwRegister);
	OLE_RELEASE_SAFE(pROT);
    }
}

/*
*---------------------------------------------------------------------------------------
*
*  ringbuffer related methods (init, destroy)
*
*---------------------------------------------------------------------------------------
*/
/**
 * \brief ringbuffer destroying routine
 *
 * \param rb pointer to empty (just allocated) ringbuffer structure
 *
 * \note routine does not frees memory, allocated for grabber_rinbuffer_s structure
 */
static void destroy_ringbuffer(grabber_ringbuffer_t * rb)
{
    int i;

    if (!rb)
	return;

    if (rb->ringbuffer) {
	for (i = 0; i < rb->buffersize; i++)
	    if (rb->ringbuffer[i])
		free(rb->ringbuffer[i]);
	free(rb->ringbuffer);
	rb->ringbuffer = NULL;
    }
    if (rb->dpts) {
	free(rb->dpts);
	rb->dpts = NULL;
    }
    if (rb->pMutex) {
	DeleteCriticalSection(rb->pMutex);
	free(rb->pMutex);
	rb->pMutex = NULL;
    }

    rb->blocksize = 0;
    rb->buffersize = 0;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

/**
 * \brief ringbuffer initialization
 *
 * \param rb pointer to empty (just allocated) ringbuffer structure
 * \param buffersize size of buffer in blocks
 * \param blocksize size of buffer's block
 *
 * \return S_OK if success 
 * \return E_OUTOFMEMORY not enough memory
 *
 * \note routine does not allocates memory for grabber_rinbuffer_s structure
 */
static HRESULT init_ringbuffer(grabber_ringbuffer_t * rb, int buffersize,
			       int blocksize)
{
    int i;

    if (!rb)
	return E_OUTOFMEMORY;

    rb->buffersize = buffersize < 2 ? 2 : buffersize;
    rb->blocksize = blocksize;

    mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Capture buffer: %d blocks of %d bytes.\n",
	   rb->buffersize, rb->blocksize);

    rb->ringbuffer = (char **) malloc(rb->buffersize * sizeof(char *));
    if (!rb)
	return E_POINTER;
    memset(rb->ringbuffer, 0, rb->buffersize * sizeof(char *));

    for (i = 0; i < rb->buffersize; i++) {
	rb->ringbuffer[i] = (char *) malloc(rb->blocksize * sizeof(char));
	if (!rb->ringbuffer[i]) {
	    destroy_ringbuffer(rb);
	    return E_OUTOFMEMORY;
	}
    }
    rb->dpts = (double*) malloc(rb->buffersize * sizeof(double));
    if (!rb->dpts) {
	destroy_ringbuffer(rb);
	return E_OUTOFMEMORY;
    }
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->tStart = -1;
    rb->pMutex = (CRITICAL_SECTION *) malloc(sizeof(CRITICAL_SECTION));
    if (!rb->pMutex) {
	destroy_ringbuffer(rb);
	return E_OUTOFMEMORY;
    }
    InitializeCriticalSection(rb->pMutex);
    return S_OK;
}

/*
*---------------------------------------------------------------------------------------
*
*  Tuner related methods (frequency, capabilities, etc
*
*---------------------------------------------------------------------------------------
*/
/**
 * \brief returns string with name for givend PsysCon_* constant
 *
 * \param lPhysicalType constant from PhysicalConnectorType enumeration
 *
 * \return pointer to string with apropriate name
 *
 * \note
 * Caller should not free returned pointer
 */
static char *physcon2str(const long lPhysicalType)
{
    int i;
    for(i=0; tv_physcon_types[i].name; i++)
        if(tv_physcon_types[i].type==lPhysicalType)
            return tv_physcon_types[i].name;
    return "Unknown";
};

/**
 *  \brief converts MPlayer's chanlist to system country code.
 *
 * \param chanlist MPlayer's chanlist name
 *
 * \return system country code
 *
 * \remarks
 * After call to IAMTVTuner::put_CountryCode with returned value tuner switches to frequency table used in specified 
 * country (which is usually larger then MPlayer's one, so workaround will work fine).
 *
 * \todo
 * Resolve trouble with cable channels (DirectShow's tuners must be switched between broadcast and cable channels modes.
 */
static int chanlist2country(char *chanlist)
{
    int i;
    for(i=0; tv_chanlist2country[i].chanlist_name; i++)
        if (!strcmp(chanlist, tv_chanlist2country[i].chanlist_name))
            break;
    return tv_chanlist2country[i].country_code;
}

/**
 * \brief loads specified resource from module and return pointer to it
 *
 * \param hDLL valid module desriptor
 * \param index index of resource. resource with name "#<index>" will be loaded
 *
 * \return pointer to loader resource or NULL if error occured
 */
static void *GetRC(HMODULE hDLL, int index)
{
    char szRCDATA[10];
    char szName[10];
    HRSRC hRes;
    HGLOBAL hTable;

    snprintf(szRCDATA, 10, "#%d", (int)RT_RCDATA);
    snprintf(szName, 10, "#%d", index);

    hRes = FindResource(hDLL, szName, szRCDATA);
    if (!hRes) {
	return NULL;
    }
    hTable = LoadResource(hDLL, hRes);
    if (!hTable) {
	return NULL;
    }
    return LockResource(hTable);
}

/**
 * \brief loads frequency table for given country from kstvtune.ax
 *
 * \param[in] nCountry - country code
 * \param[in] nInputType (TunerInputCable or TunerInputAntenna)
 * \param[out] pplFreqTable - address of variable that receives pointer to array, containing frequencies
 * \param[out] pnLen length of array 
 * \param[out] pnFirst - channel number of first entry in array (nChannelMax)
 *
 * \return S_OK if success
 * \return E_POINTER pplFreqTable==NULL || plFirst==NULL || pnLen==NULL
 * \return E_FAIL error occured during load
 *
 * \remarks 
 * - array must be freed by caller
 * - MSDN says that it is not neccessery to unlock or free resource. It will be done after unloading DLL
 */
static HRESULT load_freq_table(int nCountry, int nInputType,
			       long **pplFreqTable, int *pnLen,
			       int *pnFirst)
{
    HMODULE hDLL;
    long *plFreqTable;
    TRCCountryList *pCountryList;
    int i, index;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: load_freq_table called %d (%s)\n",nCountry,nInputType == TunerInputAntenna ? "broadcast" : "cable");
    /* ASSERT(sizeof(TRCCountryList)==10); // need properly aligned structure */

    if (!pplFreqTable || !pnFirst || !pnLen)
	return E_POINTER;
    if (!nCountry)
	return E_FAIL;

    hDLL = LoadLibrary("kstvtune.ax");
    if (!hDLL) {
	return E_FAIL;
    }
    pCountryList = GetRC(hDLL, 9999);
    if (!pCountryList) {
	FreeLibrary(hDLL);
	return E_FAIL;
    }
    for (i = 0; pCountryList[i].CountryCode != 0; i++)
	if (pCountryList[i].CountryCode == nCountry)
	    break;
    if (pCountryList[i].CountryCode == 0) {
	FreeLibrary(hDLL);
	return E_FAIL;
    }
    if (nInputType == TunerInputCable)
	index = pCountryList[i].CableFreqTable;
    else
	index = pCountryList[i].BroadcastFreqTable;

    plFreqTable = GetRC(hDLL, index);	//First element is number of first channel, second - number of last channel
    if (!plFreqTable) {
	FreeLibrary(hDLL);
	return E_FAIL;
    }
    *pnFirst = plFreqTable[0];
    *pnLen = (int) (plFreqTable[1] - plFreqTable[0] + 1);
    *pplFreqTable = (long *) malloc((*pnLen) * sizeof(long));
    if (!*pplFreqTable) {
	FreeLibrary(hDLL);
	return E_FAIL;
    }
    for (i = 0; i < *pnLen; i++) {
	(*pplFreqTable)[i] = plFreqTable[i + 2];
	mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: load_freq_table #%d => (%ld)\n",i+*pnFirst,(*pplFreqTable)[i]);
    }
    FreeLibrary(hDLL);
    return S_OK;
}

/**
 * \brief tunes to given frequency through IKsPropertySet call
 *
 * \param pTVTuner IAMTVTuner interface of capture device
 * \param lFreq frequency to tune (in Hz)
 *
 * \return S_OK success
 * \return apropriate error code otherwise
 *
 * \note
 * Due to either bug in driver or error in following code calll to IKsProperty::Set 
 * in this methods always fail with error 0x8007007a.
 *
 * \todo test code on other machines and an error
 */
static HRESULT set_frequency_direct(IAMTVTuner * pTVTuner, long lFreq)
{
    HRESULT hr;
    DWORD dwSupported = 0;
    DWORD cbBytes = 0;
    KSPROPERTY_TUNER_MODE_CAPS_S mode_caps;
    KSPROPERTY_TUNER_FREQUENCY_S frequency;
    IKsPropertySet *pKSProp;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: set_frequency_direct called\n");

    memset(&mode_caps, 0, sizeof(mode_caps));
    memset(&frequency, 0, sizeof(frequency));

    hr = OLE_QUERYINTERFACE(pTVTuner, IID_IKsPropertySet, pKSProp);
    if (FAILED(hr))
	return hr;		//no IKsPropertySet interface

    mode_caps.Mode = AMTUNER_MODE_TV;
    hr = OLE_CALL_ARGS(pKSProp, QuerySupported, &PROPSETID_TUNER,
		   KSPROPERTY_TUNER_MODE_CAPS, &dwSupported);
    if (FAILED(hr)) {
	OLE_RELEASE_SAFE(pKSProp);
	return hr;
    }

    if (!dwSupported & KSPROPERTY_SUPPORT_GET) {
	OLE_RELEASE_SAFE(pKSProp);
	return E_FAIL;		//PROPSETID_TINER not supported
    }

    hr = OLE_CALL_ARGS(pKSProp, Get, &PROPSETID_TUNER,
		   KSPROPERTY_TUNER_MODE_CAPS,
		   INSTANCEDATA_OF_PROPERTY_PTR(&mode_caps),
		   INSTANCEDATA_OF_PROPERTY_SIZE(mode_caps),
		   &mode_caps, sizeof(mode_caps), &cbBytes);

    frequency.Frequency = lFreq;

    if (mode_caps.Strategy == KS_TUNER_STRATEGY_DRIVER_TUNES)
	frequency.TuningFlags = KS_TUNER_TUNING_FINE;
    else
	frequency.TuningFlags = KS_TUNER_TUNING_EXACT;

    if (lFreq < mode_caps.MinFrequency || lFreq > mode_caps.MaxFrequency) {
	OLE_RELEASE_SAFE(pKSProp);
	return E_FAIL;
    }

    hr = OLE_CALL_ARGS(pKSProp, Set, &PROPSETID_TUNER,
		   KSPROPERTY_TUNER_FREQUENCY,
		   INSTANCEDATA_OF_PROPERTY_PTR(&frequency),
		   INSTANCEDATA_OF_PROPERTY_SIZE(frequency),
		   &frequency, sizeof(frequency));
    if (FAILED(hr)) {
	OLE_RELEASE_SAFE(pKSProp);
	return hr;
    }

    OLE_RELEASE_SAFE(pKSProp);

    return S_OK;
}

/**
 * \brief find channel with nearest frequency and set it
 *
 * \param priv driver's private data
 * \param lFreq frequency in Hz
 *
 * \return S_OK if success
 * \return E_FAIL if error occured
 */
static HRESULT set_nearest_freq(priv_t * priv, long lFreq)
{
    HRESULT hr;
    int i;
    long lFreqDiff=-1;
    int nChannel;
    TunerInputType tunerInput;
    long lInput;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: set_nearest_freq called: %ld\n", lFreq);
    if(priv->freq_table_len == -1 && !priv->freq_table) {

        hr = OLE_CALL_ARGS(priv->pTVTuner, get_ConnectInput, &lInput);
        if(FAILED(hr)){ //Falling back to 0
            lInput=0;
        }

        hr = OLE_CALL_ARGS(priv->pTVTuner, get_InputType, lInput, &tunerInput);

        if (load_freq_table(chanlist2country(priv->tv_param->chanlist), tunerInput, &(priv->freq_table), &(priv->freq_table_len), &(priv->first_channel)) != S_OK) {//FIXME
            priv->freq_table_len=0;
            priv->freq_table=NULL;
            mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_UnableExtractFreqTable);
            return E_FAIL;
        };
        mp_msg(MSGT_TV, MSGL_V, MSGTR_TVI_DS_FreqTableLoaded, tunerInput == TunerInputAntenna ? "broadcast" : "cable",
            chanlist2country(priv->tv_param->chanlist), priv->freq_table_len);
    }

    if (priv->freq_table_len <= 0)
	return E_FAIL;

    //FIXME: rewrite search algo
    nChannel = -1;
    for (i = 0; i < priv->freq_table_len; i++) {
	if (nChannel == -1 || labs(lFreq - priv->freq_table[i]) < lFreqDiff) {
	    nChannel = priv->first_channel + i;
	    lFreqDiff = labs(lFreq - priv->freq_table[i]);
	}
	mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: set_nearest_freq #%d (%ld) => %d (%ld)\n",i+priv->first_channel,priv->freq_table[i], nChannel,lFreqDiff);
    }
    if (nChannel == -1) {
	mp_msg(MSGT_TV,MSGL_ERR, MSGTR_TVI_DS_UnableFindNearestChannel);
	return E_FAIL;
    }
    mp_msg(MSGT_TV, MSGL_V, "tvi_dshow: set_nearest_freq #%d (%ld)\n",nChannel,priv->freq_table[nChannel - priv->first_channel]);
    hr = OLE_CALL_ARGS(priv->pTVTuner, put_Channel, nChannel,
		   AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT);
    if (FAILED(hr)) {
	mp_msg(MSGT_TV,MSGL_ERR,MSGTR_TVI_DS_UnableToSetChannel, (unsigned int)hr);
	return E_FAIL;
    }
    return S_OK;
}

/**
 * \brief setting frequency. decides whether use direct call/workaround
 *
 * \param priv driver's private data
 * \param lFreq frequency in Hz
 *
 * \return TVI_CONTROL_TRUE if success
 * \return TVI_CONTROL_FALSE if error occured
 *
 * \todo check for freq boundary
 */
static int set_frequency(priv_t * priv, long lFreq)
{
    HRESULT hr;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: set_frequency called: %ld\n", lFreq);
    if (!priv->pTVTuner)
	return TVI_CONTROL_FALSE;
    if (priv->direct_setfreq_call) {	//using direct call to set frequency
	hr = set_frequency_direct(priv->pTVTuner, lFreq);
	if (FAILED(hr)) {
	    mp_msg(MSGT_TV, MSGL_V, MSGTR_TVI_DS_DirectSetFreqFailed);
	    priv->direct_setfreq_call = 0;
	}
    }
    if (!priv->direct_setfreq_call) {
	hr = set_nearest_freq(priv, lFreq);
    }
    if (FAILED(hr))
	return TVI_CONTROL_FALSE;
#ifdef DEPRECATED
    priv->pGrabber->ClearBuffer(priv->pGrabber);
#endif
    return TVI_CONTROL_TRUE;
}

/**
 * \brief return current frequency  from tuner (in Hz)
 *
 * \param pTVTuner IAMTVTuner interface of tuner
 * \param plFreq address of variable that receives current frequency
 *
 * \return S_OK success
 * \return E_POINTER pTVTuner==NULL || plFreq==NULL
 * \return apropriate error code otherwise
 */
static HRESULT get_frequency_direct(IAMTVTuner * pTVTuner, long *plFreq)
{
    HRESULT hr;
    KSPROPERTY_TUNER_STATUS_S TunerStatus;
    DWORD cbBytes;
    IKsPropertySet *pKSProp;
    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: get_frequency_direct called\n");

    if (!plFreq)
	return E_POINTER;

    hr = OLE_QUERYINTERFACE(pTVTuner, IID_IKsPropertySet, pKSProp);
    if (FAILED(hr)) {
	mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Get freq QueryInterface failed\n");
	return hr;
    }

    hr = OLE_CALL_ARGS(pKSProp, Get, &PROPSETID_TUNER,
		   KSPROPERTY_TUNER_STATUS,
		   INSTANCEDATA_OF_PROPERTY_PTR(&TunerStatus),
		   INSTANCEDATA_OF_PROPERTY_SIZE(TunerStatus),
		   &TunerStatus, sizeof(TunerStatus), &cbBytes);
    if (FAILED(hr)) {
	mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Get freq Get failure\n");
	return hr;
    }
    *plFreq = TunerStatus.CurrentFrequency;
    return S_OK;
}

/**
 * \brief gets current frequency 
 *
 * \param priv driver's private data structure 
 * \param plFreq - pointer to long int to store frequency to (in Hz)
 *
 * \return TVI_CONTROL_TRUE if success, TVI_CONTROL_FALSE otherwise
 */
static int get_frequency(priv_t * priv, long *plFreq)
{
    HRESULT hr;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: get_frequency called\n");

    if (!plFreq || !priv->pTVTuner)
	return TVI_CONTROL_FALSE;

    if (priv->direct_getfreq_call) {	//using direct call to get frequency
	hr = get_frequency_direct(priv->pTVTuner, plFreq);
	if (FAILED(hr)) {
	    mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TVI_DS_DirectGetFreqFailed);
	    priv->direct_getfreq_call = 0;
	}
    }
    if (!priv->direct_getfreq_call) {
        hr=OLE_CALL_ARGS(priv->pTVTuner, get_VideoFrequency, plFreq);
	if (FAILED(hr))
	    return TVI_CONTROL_FALSE;

    }
    return TVI_CONTROL_TRUE;
}

/**
 * \brief get tuner capabilities
 *
 * \param priv driver's private data
 */
static void get_capabilities(priv_t * priv)
{
    long lAvailableFormats;
    HRESULT hr;
    int i;
    long lInputPins, lOutputPins, lRelated, lPhysicalType;
    IEnumPins *pEnum;
    char tmp[200];
    IPin *pPin = 0;
    PIN_DIRECTION ThisPinDir;
    PIN_INFO pi;
    IAMAudioInputMixer *pIAMixer;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: get_capabilities called\n");
    if (priv->pTVTuner) {

	mp_msg(MSGT_TV, MSGL_V, MSGTR_TVI_DS_SupportedNorms);
	hr = OLE_CALL_ARGS(priv->pTVTuner, get_AvailableTVFormats,
		       &lAvailableFormats);
	if (FAILED(hr))
	    tv_available_norms_count = 0;
	else {
	    for (i = 0; i < TV_NORMS_COUNT; i++) {
		if (lAvailableFormats & tv_norms[i].index) {
		    tv_available_norms[tv_available_norms_count] = i;
		    mp_msg(MSGT_TV, MSGL_V, " %d=%s;",
			   tv_available_norms_count + 1, tv_norms[i].name);
		    tv_available_norms_count++;
		}
	    }
	}
	mp_msg(MSGT_TV, MSGL_INFO, "\n");
    }
    if (priv->pCrossbar) {
	OLE_CALL_ARGS(priv->pCrossbar, get_PinCounts, &lOutputPins,
		  &lInputPins);

	tv_available_inputs = (long *) malloc(sizeof(long) * lInputPins);
	tv_available_inputs_count = 0;

	mp_msg(MSGT_TV, MSGL_V, MSGTR_TVI_DS_AvailableVideoInputs);
	for (i = 0; i < lInputPins; i++) {
	    OLE_CALL_ARGS(priv->pCrossbar, get_CrossbarPinInfo, 1, i,
		      &lRelated, &lPhysicalType);

	    if (lPhysicalType < 0x1000) {
		tv_available_inputs[tv_available_inputs_count++] = i;
		mp_msg(MSGT_TV, MSGL_V, " %d=%s;",
		       tv_available_inputs_count - 1,
		       physcon2str(lPhysicalType));
	    }
	}
	mp_msg(MSGT_TV, MSGL_INFO, "\n");

	set_crossbar_input(priv, 0);
    }

    if (priv->adev_index != -1) {
	hr = OLE_CALL_ARGS(priv->chains[1]->pCaptureFilter, EnumPins, &pEnum);
	if (FAILED(hr))
	    return;
	mp_msg(MSGT_TV, MSGL_V, MSGTR_TVI_DS_AvailableAudioInputs);
	i = 0;
	while (OLE_CALL_ARGS(pEnum, Next, 1, &pPin, NULL) == S_OK) {
	    memset(&pi, 0, sizeof(pi));
	    memset(tmp, 0, 200);
	    OLE_CALL_ARGS(pPin, QueryDirection, &ThisPinDir);
	    if (ThisPinDir == PINDIR_INPUT) {
		OLE_CALL_ARGS(pPin, QueryPinInfo, &pi);
		wtoa(pi.achName, tmp, 200);
		OLE_RELEASE_SAFE(pi.pFilter);
		mp_msg(MSGT_TV, MSGL_V, " %d=%s", i, tmp);
		mp_msg(MSGT_TV, MSGL_DBG3, " (%p)", pPin);
		hr = OLE_QUERYINTERFACE(pPin, IID_IAMAudioInputMixer,pIAMixer);
		if (SUCCEEDED(hr)) {
		    if (i == priv->tv_param->audio_id) {
			OLE_CALL_ARGS(pIAMixer, put_Enable, TRUE);
                        if(priv->tv_param->volume>0)
			    OLE_CALL_ARGS(pIAMixer, put_MixLevel, 0.01 * priv->tv_param->volume);
#if 0
                        else
			    OLE_CALL_ARGS(pIAMixer, put_MixLevel, 1.0);
#endif
			mp_msg(MSGT_TV, MSGL_V, MSGTR_TVI_DS_InputSelected);
		    } else {
			OLE_CALL_ARGS(pIAMixer, put_Enable, FALSE);
#if 0
			OLE_CALL_ARGS(pIAMixer, put_MixLevel, 0.0);
#endif
		    }
		    OLE_RELEASE_SAFE(pIAMixer);
		}
		mp_msg(MSGT_TV, MSGL_V, ";");
		OLE_RELEASE_SAFE(pPin);
		i++;
	    }
	}
	mp_msg(MSGT_TV, MSGL_INFO, "\n");
	OLE_RELEASE_SAFE(pEnum);
    }
}

/*
*---------------------------------------------------------------------------------------
*
*  Filter related methods
*
*---------------------------------------------------------------------------------------
*/
/**
 * \brief building in graph audio/video capture chain
 *
 * \param priv           driver's private data
 * \param pCaptureFilter pointer to capture device's IBaseFilter interface
 * \param pbuf           ringbuffer data structure
 * \param pmt            media type for chain (AM_MEDIA_TYPE)
 *
 * \note routine does not frees memory, allocated for grabber_rinbuffer_s structure
 */
static HRESULT build_sub_graph(priv_t * priv, chain_t * chain, const GUID* ppin_category)
{
    HRESULT hr;
    int nFormatProbed = 0;

    IPin *pSGOut;
    IPin *pNRIn=NULL;

    IBaseFilter *pNR = NULL;

    hr=S_OK;

    //No supported formats
    if(!chain->arpmt[0])
        return E_FAIL;

    do{
        hr = OLE_CALL_ARGS(priv->pBuilder, FindPin,
    		   (IUnknown *) chain->pCaptureFilter,
    		   PINDIR_OUTPUT, ppin_category,
    		   chain->majortype, FALSE, 0, &chain->pCapturePin);
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: FindPin(pCapturePin) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }
        /* Addinf SampleGrabber filter for video stream */
        hr = CoCreateInstance((GUID *) & CLSID_SampleGrabber, NULL,CLSCTX_INPROC_SERVER, &IID_IBaseFilter,(void *) &chain->pSGF);
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: CoCreateInstance(SampleGrabber) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }
        hr = OLE_CALL_ARGS(priv->pGraph, AddFilter, chain->pSGF, L"Sample Grabber");
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: AddFilter(SampleGrabber) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }
        hr = OLE_CALL_ARGS(priv->pBuilder, FindPin, (IUnknown *) chain->pSGF,PINDIR_INPUT, NULL, NULL, FALSE, 0, &chain->pSGIn);
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: FindPin(pSGIn) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }
        hr = OLE_CALL_ARGS(priv->pBuilder, FindPin, (IUnknown *) chain->pSGF,PINDIR_OUTPUT, NULL, NULL, FALSE, 0, &pSGOut);
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: FindPin(pSGOut) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }

        /* creating ringbuffer for video samples */
        chain->pCSGCB = CSampleGrabberCB_Create(chain->rbuf);
        if(!chain->pCSGCB){
            mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: CSampleGrabberCB_Create(pbuf) call failed. Error:0x%x\n", (unsigned int)E_OUTOFMEMORY);
            break;
        }

        /* initializing SampleGrabber filter */
        hr = OLE_QUERYINTERFACE(chain->pSGF, IID_ISampleGrabber, chain->pSG);
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: QueryInterface(IID_ISampleGrabber) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }
    //    hr = OLE_CALL_ARGS(pSG, SetCallback, (ISampleGrabberCB *) pCSGCB, 1);	//we want to receive copy of sample's data
        hr = OLE_CALL_ARGS(chain->pSG, SetCallback, (ISampleGrabberCB *) chain->pCSGCB, 0);	//we want to receive sample

        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: SetCallback(pSG) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }
        hr = OLE_CALL_ARGS(chain->pSG, SetOneShot, FALSE);	//... for all frames
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: SetOneShot(pSG) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }
        hr = OLE_CALL_ARGS(chain->pSG, SetBufferSamples, FALSE);	//... do not buffer samples in sample grabber
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: SetBufferSamples(pSG) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }

        if(priv->tv_param->normalize_audio_chunks && chain->type==audio){
            set_buffer_preference(20,(WAVEFORMATEX*)(chain->arpmt[nFormatProbed]->pbFormat),chain->pCapturePin,chain->pSGIn);
        }

        for(nFormatProbed=0; chain->arpmt[nFormatProbed]; nFormatProbed++)
        {
            DisplayMediaType("Probing format", chain->arpmt[nFormatProbed]);
            hr = OLE_CALL_ARGS(chain->pSG, SetMediaType, chain->arpmt[nFormatProbed]);	//set desired mediatype
            if(FAILED(hr)){
                mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: SetMediaType(pSG) call failed. Error:0x%x\n", (unsigned int)hr);
                continue;
            }
            /* connecting filters together: VideoCapture --> SampleGrabber */
            hr = OLE_CALL_ARGS(priv->pGraph, Connect, chain->pCapturePin, chain->pSGIn);
            if(FAILED(hr)){
                mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: Unable to create pCapturePin<->pSGIn connection. Error:0x%x\n", (unsigned int)hr);
                continue;
            }
	    break;
        }

        if(!chain->arpmt[nFormatProbed])
        {
            mp_msg(MSGT_TV, MSGL_WARN, "tvi_dshow: Unable to negotiate media format\n");
            hr = E_FAIL;
            break;
        }

        hr = OLE_CALL_ARGS(chain->pCapturePin, ConnectionMediaType, chain->pmt);
        if(FAILED(hr))
        {
            mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TVI_DS_GetActualMediatypeFailed, (unsigned int)hr);
        }

        if(priv->tv_param->hidden_video_renderer){
            IEnumFilters* pEnum;
            IBaseFilter* pFilter;

            hr=OLE_CALL_ARGS(priv->pBuilder,RenderStream,NULL,NULL,(IUnknown*)chain->pCapturePin,NULL,NULL);

            OLE_CALL_ARGS(priv->pGraph, EnumFilters, &pEnum);
            while (OLE_CALL_ARGS(pEnum, Next, 1, &pFilter, NULL) == S_OK) {
                LPVIDEOWINDOW pVideoWindow;
                hr = OLE_QUERYINTERFACE(pFilter, IID_IVideoWindow, pVideoWindow);
                if (SUCCEEDED(hr))
                {
                    OLE_CALL_ARGS(pVideoWindow,put_Visible,/* OAFALSE*/ 0);
                    OLE_CALL_ARGS(pVideoWindow,put_AutoShow,/* OAFALSE*/ 0);
                    OLE_RELEASE_SAFE(pVideoWindow);
                }
                OLE_RELEASE_SAFE(pFilter);
            }
            OLE_RELEASE_SAFE(pEnum);
        }else
        {
#if 0
            /*
               Code below is disabled, because terminating chain with NullRenderer leads to jerky video.
               Perhaps, this happens because  NullRenderer filter discards each received
               frame while discarded frames causes live source filter to dramatically reduce frame rate. 
            */
            /* adding sink for video stream */
            hr = CoCreateInstance((GUID *) & CLSID_NullRenderer, NULL,CLSCTX_INPROC_SERVER, &IID_IBaseFilter,(void *) &pNR);
            if(FAILED(hr)){
                mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: CoCreateInstance(NullRenderer) call failed. Error:0x%x\n", (unsigned int)hr);
                break;
            }
            hr = OLE_CALL_ARGS(priv->pGraph, AddFilter, pNR, L"Null Renderer");
            if(FAILED(hr)){
                mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: AddFilter(NullRenderer) call failed. Error:0x%x\n", (unsigned int)hr);
                break;
            }
            hr = OLE_CALL_ARGS(priv->pBuilder, FindPin, (IUnknown *) pNR,PINDIR_INPUT, NULL, NULL, FALSE, 0, &pNRIn);
            if(FAILED(hr)){
                mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: FindPin(pNRIn) call failed. Error:0x%x\n", (unsigned int)hr);
                break;
            }
            /*
               Prevent ending VBI chain with NullRenderer filter, because this causes VBI pin disconnection
            */
            if(memcmp(&(arpmt[nFormatProbed]->majortype),&MEDIATYPE_VBI,16)){
                /* connecting filters together: SampleGrabber --> NullRenderer */
                hr = OLE_CALL_ARGS(priv->pGraph, Connect, pSGOut, pNRIn);
                if(FAILED(hr)){
                    mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: Unable to create pSGOut<->pNRIn connection. Error:0x%x\n", (unsigned int)hr);
                    break;
                }
            }
#endif
        }

        hr = S_OK;
    } while(0);

    OLE_RELEASE_SAFE(pSGOut);
    OLE_RELEASE_SAFE(pNR);
    OLE_RELEASE_SAFE(pNRIn);

    return hr;
}

/**
 * \brief configures crossbar for grabbing video stream from given input
 *
 * \param priv driver's private data
 * \param input index of available video input to get data from
 *
 * \return TVI_CONTROL_TRUE success
 * \return TVI_CONTROL_FALSE error
 */
static int set_crossbar_input(priv_t * priv, int input)
{
    HRESULT hr;
    int i, nVideoDecoder, nAudioDecoder;
    long lInput, lInputRelated, lRelated, lPhysicalType, lOutputPins,
	lInputPins;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: Configuring crossbar\n");
    if (!priv->pCrossbar || input < 0
	|| input >= tv_available_inputs_count)
	return TVI_CONTROL_FALSE;

    OLE_CALL_ARGS(priv->pCrossbar, get_PinCounts, &lOutputPins, &lInputPins);

    lInput = tv_available_inputs[input];

    if (lInput < 0 || lInput >= lInputPins)
	return TVI_CONTROL_FALSE;

    OLE_CALL_ARGS(priv->pCrossbar, get_CrossbarPinInfo, 1 /* input */ , lInput,
	      &lInputRelated, &lPhysicalType);

    nVideoDecoder = nAudioDecoder = -1;
    for (i = 0; i < lOutputPins; i++) {
	OLE_CALL_ARGS(priv->pCrossbar, get_CrossbarPinInfo, 0 /*output */ , i,
		  &lRelated, &lPhysicalType);
	if (lPhysicalType == PhysConn_Video_VideoDecoder)
	    nVideoDecoder = i;
	if (lPhysicalType == PhysConn_Audio_AudioDecoder)
	    nAudioDecoder = i;
    }
    if (nVideoDecoder >= 0) {
	//connecting given input with video decoder
	hr = OLE_CALL_ARGS(priv->pCrossbar, Route, nVideoDecoder, lInput);
	if (hr != S_OK) {
	    mp_msg(MSGT_TV,MSGL_ERR,MSGTR_TVI_DS_UnableConnectInputVideoDecoder, (unsigned int)hr);
	    return TVI_CONTROL_FALSE;
	}
    }
    if (nAudioDecoder >= 0 && lInputRelated >= 0) {
	hr = OLE_CALL_ARGS(priv->pCrossbar, Route, nAudioDecoder,
		       lInputRelated);
	if (hr != S_OK) {
	    mp_msg(MSGT_TV,MSGL_ERR,MSGTR_TVI_DS_UnableConnectInputAudioDecoder, (unsigned int)hr);
	    return TVI_CONTROL_FALSE;
	}
    }
    return TVI_CONTROL_TRUE;
}

/**
 * \brief adjusts video control (hue,saturation,contrast,brightess)
 *
 * \param priv driver's private data
 * \param control which control to adjust
 * \param value new value for control (0-100)
 *
 * \return TVI_CONTROL_TRUE success
 * \return TVI_CONTROL_FALSE error
 */
static int set_control(priv_t * priv, int control, int value)
{
    long lMin, lMax, lStepping, lDefault, lFlags, lValue;
    HRESULT hr;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: set_control called\n");
    if (value < -100 || value > 100 || !priv->pVideoProcAmp)
	return TVI_CONTROL_FALSE;

    hr = OLE_CALL_ARGS(priv->pVideoProcAmp, GetRange, control,
		   &lMin, &lMax, &lStepping, &lDefault, &lFlags);
    if (FAILED(hr) || lFlags != VideoProcAmp_Flags_Manual)
	return TVI_CONTROL_FALSE;

    lValue = lMin + (value + 100) * (lMax - lMin) / 200;
    /*
       Workaround for ATI AIW 7500. The driver reports: max=255, stepping=256
     */
    if (lStepping > lMax) {
	mp_msg(MSGT_TV, MSGL_DBG3,
	       "tvi_dshow: Stepping (%ld) is bigger than max value (%ld) for control %d. Assuming 1\n",
	       lStepping, lMax,control);
	lStepping = 1;
    }
    lValue -= lValue % lStepping;
    hr = OLE_CALL_ARGS(priv->pVideoProcAmp, Set, control, lValue,
		   VideoProcAmp_Flags_Manual);
    if (FAILED(hr))
	return TVI_CONTROL_FALSE;

    return TVI_CONTROL_TRUE;
}

/**
 * \brief get current value of video control (hue,saturation,contrast,brightess)
 *
 * \param priv driver's private data
 * \param control which control to adjust
 * \param pvalue address of variable thar receives current value
 *
 * \return TVI_CONTROL_TRUE success
 * \return TVI_CONTROL_FALSE error
 */
static int get_control(priv_t * priv, int control, int *pvalue)
{
    long lMin, lMax, lStepping, lDefault, lFlags, lValue;
    HRESULT hr;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: get_control called\n");
    if (!pvalue || !priv->pVideoProcAmp)
	return TVI_CONTROL_FALSE;

    hr = OLE_CALL_ARGS(priv->pVideoProcAmp, GetRange, control,
		   &lMin, &lMax, &lStepping, &lDefault, &lFlags);
    if (FAILED(hr))
	return TVI_CONTROL_FALSE;
    if (lMin == lMax) {
	*pvalue = lMin;
	return TVI_CONTROL_TRUE;
    }

    hr = OLE_CALL_ARGS(priv->pVideoProcAmp, Get, control, &lValue, &lFlags);
    if (FAILED(hr))
	return TVI_CONTROL_FALSE;

    *pvalue = 200 * (lValue - lMin) / (lMax - lMin) - 100;

    return TVI_CONTROL_TRUE;
}

/**
 * \brief create AM_MEDIA_TYPE structure, corresponding to given FourCC code and width/height/fps
 * \param fcc FourCC code for video format
 * \param width picture width
 * \param height pciture height
 * \param fps frames per second (required for bitrate calculation)
 *
 * \return pointer to AM_MEDIA_TYPE structure if success, NULL - otherwise
 */
static AM_MEDIA_TYPE* create_video_format(int fcc, int width, int height, int fps)
{
    int i;
    AM_MEDIA_TYPE mt;
    VIDEOINFOHEADER vHdr;

    /* Check given fcc in lookup table*/
    for(i=0; img_fmt_list[i].fmt && img_fmt_list[i].fmt!=fcc; i++) /* NOTHING */;
    if(!img_fmt_list[i].fmt)
        return NULL;

    memset(&mt, 0, sizeof(AM_MEDIA_TYPE));
    memset(&vHdr, 0, sizeof(VIDEOINFOHEADER));

    vHdr.bmiHeader.biSize = sizeof(vHdr.bmiHeader);
    vHdr.bmiHeader.biWidth = width;
    vHdr.bmiHeader.biHeight = height;
    //FIXME: is biPlanes required too?
    //vHdr.bmiHeader.biPlanes = img_fmt_list[i].nPlanes;
    vHdr.bmiHeader.biBitCount = img_fmt_list[i].nBits;
    vHdr.bmiHeader.biCompression = img_fmt_list[i].nCompression;
    vHdr.bmiHeader.biSizeImage = width * height * img_fmt_list[i].nBits / 8;
    vHdr.dwBitRate = vHdr.bmiHeader.biSizeImage * 8 * fps;

    mt.pbFormat = (char*)&vHdr;
    mt.cbFormat = sizeof(vHdr);

    mt.majortype = MEDIATYPE_Video;
    mt.subtype = *img_fmt_list[i].subtype;
    mt.formattype = FORMAT_VideoInfo;

    mt.bFixedSizeSamples = 1;
    mt.bTemporalCompression = 0;
    mt.lSampleSize = vHdr.bmiHeader.biSizeImage;

    return CreateMediaType(&mt);
}

/**
 * \brief extracts fcc,width,height from AM_MEDIA_TYPE
 *
 * \param pmt pointer to AM_MEDIA_TYPE to extract data from
 * \param pfcc address of variable that receives FourCC
 * \param pwidth address of variable that receives width
 * \param pheight address of variable that recevies height
 *
 * \return 1 if data extracted successfully, 0 - otherwise
 */
static int extract_video_format(AM_MEDIA_TYPE * pmt, int *pfcc,
				int *pwidth, int *pheight)
{
    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: extract_video_format called\n");
    if (!pmt)
	return 0;
    if (!pmt->pbFormat)
	return 0;
    if (memcmp(&(pmt->formattype), &FORMAT_VideoInfo, 16) != 0)
	return 0;
    if (pfcc)
	*pfcc = subtype2imgfmt(&(pmt->subtype));
    if (pwidth)
	*pwidth = ((VIDEOINFOHEADER *) pmt->pbFormat)->bmiHeader.biWidth;
    if (pheight)
	*pheight = ((VIDEOINFOHEADER *) pmt->pbFormat)->bmiHeader.biHeight;
    return 1;
}

/**
 * \brief extracts samplerate,bits,channels from AM_MEDIA_TYPE
 *
 * \param pmt pointer to AM_MEDIA_TYPE to extract data from
 * \param pfcc address of variable that receives samplerate
 * \param pwidth address of variable that receives number of bits per sample
 * \param pheight address of variable that recevies number of channels
 *
 * \return 1 if data extracted successfully, 0 - otherwise
 */
static int extract_audio_format(AM_MEDIA_TYPE * pmt, int *psamplerate,
				int *pbits, int *pchannels)
{
    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: extract_audio_format called\n");
    if (!pmt)
	return 0;
    if (!pmt->pbFormat)
	return 0;
    if (memcmp(&(pmt->formattype), &FORMAT_WaveFormatEx, 16) != 0)
	return 0;
    if (psamplerate)
	*psamplerate = ((WAVEFORMATEX *) pmt->pbFormat)->nSamplesPerSec;
    if (pbits)
	*pbits = ((WAVEFORMATEX *) pmt->pbFormat)->wBitsPerSample;
    if (pchannels)
	*pchannels = ((WAVEFORMATEX *) pmt->pbFormat)->nChannels;
    return 1;
}

/**
 * \brief checks if AM_MEDIA_TYPE compatible with given samplerate,bits,channels
 *
 * \param pmt pointer to AM_MEDIA_TYPE for check
 * \param samplerate audio samplerate
 * \param bits bits per sample
 * \param channels number of audio channels
 *
 * \return 1 if AM_MEDIA_TYPE compatible
 * \return 0 if not
 */
static int check_audio_format(AM_MEDIA_TYPE * pmt, int samplerate,
			      int bits, int channels)
{
    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: check_audio_format called\n");
    if (!pmt)
	return 0;
    if (memcmp(&(pmt->majortype), &MEDIATYPE_Audio, 16) != 0)
	return 0;
    if (memcmp(&(pmt->subtype), &MEDIASUBTYPE_PCM, 16) != 0)
	return 0;
    if (memcmp(&(pmt->formattype), &FORMAT_WaveFormatEx, 16) != 0)
	return 0;
    if (!pmt->pbFormat)
	return 0;
    if (((WAVEFORMATEX *) pmt->pbFormat)->nSamplesPerSec != samplerate)
	return 0;
    if (((WAVEFORMATEX *) pmt->pbFormat)->wBitsPerSample != bits)
	return 0;
    if (channels > 0
	&& ((WAVEFORMATEX *) pmt->pbFormat)->nChannels != channels)
	return 0;

    return 1;
}

/**
 * \brief checks if AM_MEDIA_TYPE compatible with given fcc,width,height
 *
 * \param pmt pointer to AM_MEDIA_TYPE for check
 * \param fcc FourCC (compression)
 * \param width width of picture
 * \param height height of picture
 *
 * \return 1 if AM_MEDIA_TYPE compatible
 & \return 0 if not
 *
 * \note 
 * width and height are currently not used
 *
 * \todo 
 * add width/height check
 */
static int check_video_format(AM_MEDIA_TYPE * pmt, int fcc, int width,
			      int height)
{
    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: check_video_format called\n");
    if (!pmt)
	return 0;
    if (memcmp(&(pmt->majortype), &MEDIATYPE_Video, 16) != 0)
	return 0;
    if (subtype2imgfmt(&(pmt->subtype)) != fcc)
	return 0;
    return 1;
}

/**
 * \brief converts DirectShow subtype to MPlayer's IMGFMT
 *
 * \param subtype DirectShow subtype for video format
 *
 * \return MPlayer's IMGFMT or 0 if error occured
 */
static int subtype2imgfmt(const GUID * subtype)
{
    int i;
    for (i = 0; img_fmt_list[i].fmt; i++) {
	if (memcmp(subtype, img_fmt_list[i].subtype, 16) == 0)
	    return img_fmt_list[i].fmt;
    }
    return 0;
}

/**
 * \brief prints filter name and it pins
 *
 * \param pFilter - IBaseFilter to get data from
 *
 * \return S_OK if success, error code otherwise
 */
static HRESULT show_filter_info(IBaseFilter * pFilter)
{
    char tmp[200];
    FILTER_INFO fi;
    LPENUMPINS pEnum = 0;
    IPin *pPin = 0;
    PIN_DIRECTION ThisPinDir;
    PIN_INFO pi;
    HRESULT hr;
    int i;

    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: show_filter_info called\n");
    memset(&fi, 0, sizeof(fi));
    memset(tmp, 0, 200);

    OLE_CALL_ARGS(pFilter, QueryFilterInfo, &fi);
    OLE_RELEASE_SAFE(fi.pGraph);
    wtoa(fi.achName, tmp, 200);
    mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: BaseFilter (%p): Name=%s, Graph=%p output pins:",
	   pFilter, tmp, fi.pGraph);
    hr = OLE_CALL_ARGS(pFilter, EnumPins, &pEnum);
    if (FAILED(hr))
	return hr;
    i = 0;
    while (OLE_CALL_ARGS(pEnum, Next, 1, &pPin, NULL) == S_OK) {
	memset(&pi, 0, sizeof(pi));
	memset(tmp, 0, 200);
	OLE_CALL_ARGS(pPin, QueryDirection, &ThisPinDir);
	if (ThisPinDir == PINDIR_OUTPUT) {
	    OLE_CALL_ARGS(pPin, QueryPinInfo, &pi);
	    wtoa(pi.achName, tmp, 200);
	    OLE_RELEASE_SAFE(pi.pFilter);
	    mp_msg(MSGT_TV, MSGL_DBG2, " %d=%s", i, tmp);
	    mp_msg(MSGT_TV, MSGL_DBG3, " (%p)", pPin);
	    mp_msg(MSGT_TV, MSGL_DBG2, ";");
	    OLE_RELEASE_SAFE(pPin);
	    i++;
	}
    }
    mp_msg(MSGT_TV, MSGL_DBG2, "\n");
    OLE_RELEASE_SAFE(pEnum);
    return S_OK;
}

/**
 * \brief gets device's frendly in ANSI encoding
 *
 * \param pM IMoniker interface, got in enumeration process
 * \param category device category 
 *
 * \return TVI_CONTROL_TRUE if operation succeded, TVI_CONTROL_FALSE - otherwise
 */
static int get_device_name(IMoniker * pM, char *pBuf, int nLen)
{
    HRESULT hr;
    VARIANT var;
    IPropertyBag *pPropBag;
    hr = OLE_CALL_ARGS(pM, BindToStorage, 0, 0, &IID_IPropertyBag,(void *) &pPropBag);
    if (FAILED(hr)) {
	mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Call to BindToStorage failed\n");
	return TVI_CONTROL_FALSE;
    }
    var.vt = VT_BSTR;
    hr = OLE_CALL_ARGS(pPropBag, Read, L"Description", (LPVARIANT) & var,
		   NULL);
    if (FAILED(hr)) {
	hr = OLE_CALL_ARGS(pPropBag, Read, L"FriendlyName", (LPVARIANT) & var,
		       NULL);
    }
    OLE_RELEASE_SAFE(pPropBag);
    if (SUCCEEDED(hr)) {
	wtoa(var.bstrVal, pBuf, nLen);
        return TVI_CONTROL_TRUE;
    }
    return TVI_CONTROL_FALSE;
}

/**
 * \brief find capture device at given index
 *
 * \param index device index to search for (-1 mean only print available)
 * \param category device category 
 *
 * \return IBaseFilter interface for capture device with given index
 * 
 * Sample values for category:
 *  CLSID_VideoInputDeviceCategory - Video Capture Sources
 *  CLSID_AudioInputDeviceCategory - Audio Capture Sources
 * See DirectShow SDK documentation for other possible values
 */
static IBaseFilter *find_capture_device(int index, REFCLSID category)
{
    IBaseFilter *pFilter = NULL;
    ICreateDevEnum *pDevEnum = NULL;
    IEnumMoniker *pClassEnum = NULL;
    IMoniker *pM;
    HRESULT hr;
    ULONG cFetched;
    int i;
    char tmp[DEVICE_NAME_MAX_LEN + 1];
    hr = CoCreateInstance((GUID *) & CLSID_SystemDeviceEnum, NULL,
			  CLSCTX_INPROC_SERVER, &IID_ICreateDevEnum,
			  (void *) &pDevEnum);
    if (FAILED(hr)) {
	mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Unable to create device enumerator\n");
	return NULL;
    }

    hr = OLE_CALL_ARGS(pDevEnum, CreateClassEnumerator, category, &pClassEnum, 0);
    OLE_RELEASE_SAFE(pDevEnum);
    if (FAILED(hr)) {
	mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Unable to create class enumerator\n");
	return NULL;
    }
    if (hr == S_FALSE) {
	mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: No capture devices found\n");
	return NULL;
    }

    OLE_CALL(pClassEnum,Reset);
    for (i = 0; OLE_CALL_ARGS(pClassEnum, Next, 1, &pM, &cFetched) == S_OK; i++) {
	if(get_device_name(pM, tmp, DEVICE_NAME_MAX_LEN)!=TVI_CONTROL_TRUE)
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_UnableGetDeviceName, i);
        else
	    mp_msg(MSGT_TV, MSGL_V, MSGTR_TVI_DS_DeviceName, i, tmp);
	if (index != -1 && i == index) {
	    mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TVI_DS_UsingDevice, index, tmp);
	    hr = OLE_CALL_ARGS(pM, BindToObject, 0, 0, &IID_IBaseFilter,(void *) &pFilter);
	    if (FAILED(hr))
		pFilter = NULL;
	}
	OLE_RELEASE_SAFE(pM);
    }
    if (index != -1 && !pFilter) {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_DeviceNotFound,
	       index);
    }
    OLE_RELEASE_SAFE(pClassEnum);

    return pFilter;
}

/**
 * \brief get array of available formats through call to IAMStreamConfig::GetStreamCaps
 *
 * \praram[in] chain chain data structure
 *
 * \return S_OK success
 * \return E_POINTER one of parameters is NULL
 * \return E_FAIL required size of buffer is unknown for given media type
 * \return E_OUTOFMEMORY not enough memory
 * \return other error code from called methods
 *
 * \remarks
 * last items of chain->arpmt and chain->arStreamCaps will be NULL
 */
static HRESULT get_available_formats_stream(chain_t *chain)
{
    AM_MEDIA_TYPE **arpmt;
    void **pBuf=NULL;

    HRESULT hr;
    int i, count, size;
    int done;

    mp_msg(MSGT_TV, MSGL_DBG4,
	   "tvi_dshow: get_available_formats_stream called\n");

    if (!chain->pStreamConfig)
	return E_POINTER;

    hr=OLE_CALL_ARGS(chain->pStreamConfig, GetNumberOfCapabilities, &count, &size);
    if (FAILED(hr)) {
	mp_msg(MSGT_TV, MSGL_DBG4,
	       "tvi_dshow: Call to GetNumberOfCapabilities failed (get_available_formats_stream)\n");
	return hr;
    }
    if (chain->type == video){
	if (size != sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
	    mp_msg(MSGT_TV, MSGL_DBG4,
		   "tvi_dshow: Wrong video structure size for GetNumberOfCapabilities (get_available_formats_stream)\n");
	    return E_FAIL;
	}
    } else if (chain->type == audio){
	if (size != sizeof(AUDIO_STREAM_CONFIG_CAPS)) {
	    mp_msg(MSGT_TV, MSGL_DBG4,
		       "tvi_dshow: Wrong audio structure size for GetNumberOfCapabilities (get_available_formats_stream)\n");
	    return E_FAIL;
	}
    } else {
		mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_UnsupportedMediaType,"get_available_formats_stream");
		return E_FAIL;
    }
    done = 0;

    arpmt = (AM_MEDIA_TYPE **) malloc((count + 1) * sizeof(AM_MEDIA_TYPE *));
    if (arpmt) {
	memset(arpmt, 0, (count + 1) * sizeof(AM_MEDIA_TYPE *));

	pBuf = (void **) malloc((count + 1) * sizeof(void *));
	if (pBuf) {
	    memset(pBuf, 0, (count + 1) * sizeof(void *));

	    for (i = 0; i < count; i++) {
		pBuf[i] = malloc(size);

		if (!pBuf[i])
		    break;

		hr = OLE_CALL_ARGS(chain->pStreamConfig, GetStreamCaps, i,
			       &(arpmt[i]), pBuf[i]);
		if (FAILED(hr))
		    break;
	    }
	    if (i == count) {
		chain->arpmt = arpmt;
		chain->arStreamCaps = pBuf;
		done = 1;
	    }
	}
    }
    if (!done) {
	for (i = 0; i < count; i++) {
	    if (pBuf && pBuf[i])
		free(pBuf[i]);
	    if (arpmt && arpmt[i])
		DeleteMediaType(arpmt[i]);
	}
	if (pBuf)
	    free(pBuf);
	if (arpmt)
	    free(arpmt);
	if (hr != S_OK) {
	    mp_msg(MSGT_TV, MSGL_DBG4, "tvi_dshow: Call to GetStreamCaps failed (get_available_formats_stream)\n");
	    return hr;
	} else
	    return E_OUTOFMEMORY;
    }
    return S_OK;
}

/**
 * \brief returns allocates an array and store available media formats for given pin type to it 
 * 
 * \param pBuilder ICaptureGraphBuilder2 interface of graph builder
 * \param chain chain data structure
 *
 * \return S_OK success
 * \return E_POINTER one of given pointers is null
 * \return apropriate error code otherwise
 */
static HRESULT get_available_formats_pin(ICaptureGraphBuilder2 * pBuilder,
					 chain_t *chain)
{
    IEnumMediaTypes *pEnum;
    int i, count, size;
    ULONG cFetched;
    AM_MEDIA_TYPE *pmt;
    HRESULT hr;
    void **pBuf;
    AM_MEDIA_TYPE **arpmt;	//This will be real array
    VIDEO_STREAM_CONFIG_CAPS *pVideoCaps;
    AUDIO_STREAM_CONFIG_CAPS *pAudioCaps;
    int p1, p2, p3;

    mp_msg(MSGT_TV, MSGL_DBG4,
	   "tvi_dshow: get_available_formats_pin called\n");
    if (!pBuilder || !chain->pCaptureFilter)
	return E_POINTER;

    if (!chain->pCapturePin)
    {
        hr = OLE_CALL_ARGS(pBuilder, FindPin,
    		   (IUnknown *) chain->pCaptureFilter,
    		   PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE,
    		   chain->majortype, FALSE, 0, &chain->pCapturePin);

        if (!chain->pCapturePin)
            return E_POINTER;
    }
    if (chain->type == video) {
	size = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    } else if (chain->type == audio) {
	size = sizeof(AUDIO_STREAM_CONFIG_CAPS);
    } else {
	mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_UnsupportedMediaType,"get_available_formats_pin");
	return E_FAIL;
    }

    hr = OLE_CALL_ARGS(chain->pCapturePin, EnumMediaTypes, &pEnum);
    if (FAILED(hr)) {
	mp_msg(MSGT_TV, MSGL_DBG4,
	       "tvi_dshow: Call to EnumMediaTypes failed (get_available_formats_pin)\n");
	return hr;
    }
    for (i = 0; OLE_CALL_ARGS(pEnum, Next, 1, &pmt, &cFetched) == S_OK; i++) {
	if (!pmt)
	    break;
    }
    OLE_CALL(pEnum,Reset);

    count = i;
    arpmt =
	(AM_MEDIA_TYPE **) malloc((count + 1) * sizeof(AM_MEDIA_TYPE *));
    if (!arpmt)
	return E_OUTOFMEMORY;
    memset(arpmt, 0, (count + 1) * sizeof(AM_MEDIA_TYPE *));

    for (i = 0;
	 i < count
	 && OLE_CALL_ARGS(pEnum, Next, 1, &(arpmt[i]), &cFetched) == S_OK;
	 i++);

    OLE_RELEASE_SAFE(pEnum);


    pBuf = (void **) malloc((count + 1) * sizeof(void *));
    if (!pBuf) {
	for (i = 0; i < count; i++)
	    if (arpmt[i])
		DeleteMediaType(arpmt[i]);
	free(arpmt);
	return E_OUTOFMEMORY;
    }
    memset(pBuf, 0, (count + 1) * sizeof(void *));

    for (i = 0; i < count; i++) {
	pBuf[i] = malloc(size);
	if (!pBuf[i])
	    break;
	memset(pBuf[i], 0, size);

	if (chain->type == video) {
	    pVideoCaps = (VIDEO_STREAM_CONFIG_CAPS *) pBuf[i];
	    extract_video_format(arpmt[i], NULL, &p1, &p2);
	    pVideoCaps->MaxOutputSize.cx = pVideoCaps->MinOutputSize.cx =
		p1;
	    pVideoCaps->MaxOutputSize.cy = pVideoCaps->MinOutputSize.cy =
		p2;
	} else {
	    pAudioCaps = (AUDIO_STREAM_CONFIG_CAPS *) pBuf[i];
	    extract_audio_format(arpmt[i], &p1, &p2, &p3);
	    pAudioCaps->MaximumSampleFrequency =
		pAudioCaps->MinimumSampleFrequency = p1;
	    pAudioCaps->MaximumBitsPerSample =
		pAudioCaps->MinimumBitsPerSample = p2;
	    pAudioCaps->MaximumChannels = pAudioCaps->MinimumChannels = p3;
	}

    }
    if (i != count) {
	for (i = 0; i < count; i++) {
	    if (arpmt[i])
		DeleteMediaType(arpmt[i]);
	    if (pBuf[i])
		free(pBuf[i]);
	}
	free(arpmt);
	free(pBuf);
	return E_OUTOFMEMORY;
    }
    chain->arpmt = arpmt;
    chain->arStreamCaps = pBuf;

    return S_OK;
}

/*
*---------------------------------------------------------------------------------------
*
*  Public methods
*
*---------------------------------------------------------------------------------------
*/
/**
 * \brief fills given buffer with audio data (usually one block)
 *
 * \param priv driver's private data structure 
 * \param buffer buffer to store data to
 * \param len buffer's size in bytes (usually one block size)
 *
 * \return audio pts if audio present, 1 - otherwise
 */
static double grab_audio_frame(priv_t * priv, char *buffer, int len)
{
    int bytes = 0;
    int i;
    double pts;
    grabber_ringbuffer_t *rb = priv->chains[1]->rbuf;
    grabber_ringbuffer_t *vrb = priv->chains[0]->rbuf;

    if (!rb || !rb->ringbuffer)
	return 1;

    if(vrb && vrb->tStart<0){
        memset(buffer,0,len);
        return 0;
    }
    if(vrb && rb->tStart<0)
        rb->tStart=vrb->tStart;

    if (len < rb->blocksize)
	bytes = len;
    else
	bytes = rb->blocksize;

    mp_msg(MSGT_TV, MSGL_DBG3,"tvi_dshow: FillBuffer (audio) called. %d blocks in buffer, %d bytes requested\n",
	   rb->count, len);
    if(!rb->count){
        mp_msg(MSGT_TV,MSGL_DBG4,"tvi_dshow: waiting for frame\n");
        for(i=0;i<1000 && !rb->count;i++) usec_sleep(1000);
        if(!rb->count){
            mp_msg(MSGT_TV,MSGL_DBG4,"tvi_dshow: waiting timeout\n");
            return 0;
        }
        mp_msg(MSGT_TV,MSGL_DBG4,"tvi_dshow: got frame!\n");
    }

    EnterCriticalSection(rb->pMutex);
      pts=rb->dpts[rb->head]-rb->tStart;
      memcpy(buffer, rb->ringbuffer[rb->head], bytes);
      rb->head = (rb->head + 1) % rb->buffersize;
      rb->count--;
    LeaveCriticalSection(rb->pMutex);
    return pts;
}

/**
 * \brief returns audio frame size
 *
 * \param priv driver's private data structure 
 *
 * \return audio block size if audio enabled and 1 - otherwise
 */
static int get_audio_framesize(priv_t * priv)
{
    if (!priv->chains[1]->rbuf)
	return 1;		//no audio       
    mp_msg(MSGT_TV,MSGL_DBG3,"get_audio_framesize: %d\n",priv->chains[1]->rbuf->blocksize);
    return priv->chains[1]->rbuf->blocksize;
}

#ifdef CONFIG_TV_TELETEXT
static int vbi_get_props(priv_t* priv,tt_stream_props* ptsp)
{
    if(!priv || !ptsp)
        return TVI_CONTROL_FALSE;

//STUBS!!!
    ptsp->interlaced=0;
    ptsp->offset=256;

    ptsp->sampling_rate=27e6;
    ptsp->samples_per_line=720;

    ptsp->count[0]=16;
    ptsp->count[1]=16;
//END STUBS!!!
    ptsp->bufsize = ptsp->samples_per_line * (ptsp->count[0] + ptsp->count[1]);

    mp_msg(MSGT_TV,MSGL_V,"vbi_get_props: sampling_rate=%d,offset:%d,samples_per_line: %d\n interlaced:%s, count=[%d,%d]\n",    
        ptsp->sampling_rate,
        ptsp->offset,
        ptsp->samples_per_line,
        ptsp->interlaced?"Yes":"No",
        ptsp->count[0],
        ptsp->count[1]);

    return TVI_CONTROL_TRUE;
}

static void vbi_grabber(priv_t* priv)
{
    grabber_ringbuffer_t *rb = priv->chains[2]->rbuf;
    int i;
    unsigned char* buf;
    if (!rb || !rb->ringbuffer)
	return;

    buf=calloc(1,rb->blocksize);
    for(i=0; i<23 && rb->count; i++){
        memcpy(buf,rb->ringbuffer[rb->head],rb->blocksize);
        teletext_control(priv->priv_vbi,TV_VBI_CONTROL_DECODE_PAGE,&buf);
        rb->head = (rb->head + 1) % rb->buffersize;
        rb->count--;
    }
    free(buf);
}
#endif /* CONFIG_TV_TELETEXT */

/**
 * \brief fills given buffer with video data (usually one frame)
 *
 * \param priv driver's private data structure 
 * \param buffer buffer to store data to
 * \param len buffer's size in bytes (usually one frame size)
 *
 * \return frame size if video present, 0 - otherwise
 */
static double grab_video_frame(priv_t * priv, char *buffer, int len)
{
    int bytes = 0;
    int i;
    double pts;
    grabber_ringbuffer_t *rb = priv->chains[0]->rbuf;

    if (!rb || !rb->ringbuffer)
	return 1;
    if (len < rb->blocksize)
	bytes = len;
    else
	bytes = rb->blocksize;

    mp_msg(MSGT_TV, MSGL_DBG3,"tvi_dshow: FillBuffer (video) called. %d blocks in buffer, %d bytes requested\n",
	   rb->count, len);
    if(!rb->count){
        mp_msg(MSGT_TV,MSGL_DBG4,"tvi_dshow: waiting for frame\n");
        for(i=0;i<1000 && !rb->count;i++) usec_sleep(1000);
        if(!rb->count){
            mp_msg(MSGT_TV,MSGL_DBG4,"tvi_dshow: waiting timeout\n");
            return 0;
        }
        mp_msg(MSGT_TV,MSGL_DBG4,"tvi_dshow: got frame!\n");
    }
    EnterCriticalSection(rb->pMutex);
      if(rb->tStart<0)
          rb->tStart=rb->dpts[rb->head];
      pts=rb->dpts[rb->head]-rb->tStart;
      memcpy(buffer, rb->ringbuffer[rb->head], bytes);
      rb->head = (rb->head + 1) % rb->buffersize;
      rb->count--;
    LeaveCriticalSection(rb->pMutex);

#ifdef CONFIG_TV_TELETEXT
    vbi_grabber(priv);
#endif
    return pts;
}

/**
 * \brief returns frame size
 *
 * \param priv driver's private data structure 
 *
 * \return frame size if video present, 0 - otherwise
 */
static int get_video_framesize(priv_t * priv)
{
//      if(!priv->pmtVideo) return 1; //no video       
//      return priv->pmtVideo->lSampleSize;
    if (!priv->chains[0]->rbuf)
	return 1;		//no video       
mp_msg(MSGT_TV,MSGL_DBG3,"geT_video_framesize: %d\n",priv->chains[0]->rbuf->blocksize);
    return priv->chains[0]->rbuf->blocksize;
}

/**
 * \brief calculate audio buffer size
 * \param video_buf_size size of video buffer in bytes
 * \param video_bitrate video bit rate
 * \param audio_bitrate audio bit rate
 * \return audio buffer isze in bytes
 *
 * \remarks length of video buffer and resulted audio buffer calculated in
 * seconds will be the same.
 */
static inline int audio_buf_size_from_video(int video_buf_size, int video_bitrate, int audio_bitrate)
{
    int audio_buf_size = audio_bitrate * (video_buf_size / video_bitrate);
    mp_msg(MSGT_TV,MSGL_DBG2,"tvi_dshow: Audio capture buffer: %d * %d / %d = %d\n",
            audio_bitrate,video_buf_size,video_bitrate,audio_buf_size);
    return audio_buf_size;
}

/**
 * \brief common chain initialization routine
 * \param chain chain data structure
 *
 * \note pCaptureFilter member should be initialized before call to this routine
 */
static HRESULT init_chain_common(ICaptureGraphBuilder2 *pBuilder, chain_t *chain)
{
    HRESULT hr;
    int i;

    if(!chain->pCaptureFilter)
        return E_POINTER;

    show_filter_info(chain->pCaptureFilter);

    hr = OLE_CALL_ARGS(pBuilder, FindPin,
            (IUnknown *) chain->pCaptureFilter,
            PINDIR_OUTPUT, chain->pin_category,
            chain->majortype, FALSE, 0, &chain->pCapturePin);

    if (FAILED(hr)) {
        mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: FindPin(pCapturePin) call failed. Error:0x%x\n", (unsigned int)hr);
        return hr;
    }

    hr = OLE_CALL_ARGS(pBuilder, FindInterface,
            chain->pin_category,
            chain->majortype,
            chain->pCaptureFilter,
            &IID_IAMStreamConfig,
            (void **) &(chain->pStreamConfig));
    if (FAILED(hr))
        chain->pStreamConfig = NULL;

    /* 
       Getting available video formats (last pointer in array  will be NULL) 
       First tryin to call IAMStreamConfig::GetStreamCaos. this will give us additional information such as
       min/max picture dimensions, etc. If this call fails trying IPIn::EnumMediaTypes with default
       min/max values.
    */
    hr = get_available_formats_stream(chain);
    if (FAILED(hr)) {
        mp_msg(MSGT_TV, MSGL_DBG2, "Unable to use IAMStreamConfig for retriving available formats (Error:0x%x). Using EnumMediaTypes instead\n", (unsigned int)hr);
        hr = get_available_formats_pin(pBuilder, chain);
        if(FAILED(hr)){
            return hr;
        }
    }
    chain->nFormatUsed = 0;

    //If argument to CreateMediaType is NULL then result will be NULL too.
    chain->pmt = CreateMediaType(chain->arpmt[0]);

    for (i = 0; chain->arpmt[i]; i++)
        DisplayMediaType("Available format", chain->arpmt[i]);

    return S_OK;
}
/**
 * \brief build video stream chain in graph
 * \param priv private data structure
 *
 * \return S_OK if chain was built successfully, apropriate error code otherwise
 */
static HRESULT build_video_chain(priv_t *priv)
{
    HRESULT hr;

    if(priv->chains[0]->rbuf)
        return S_OK;

    if (priv->chains[0]->pStreamConfig) {
	hr = OLE_CALL_ARGS(priv->chains[0]->pStreamConfig, SetFormat, priv->chains[0]->pmt);
	if (FAILED(hr)) {
	    mp_msg(MSGT_TV,MSGL_ERR,MSGTR_TVI_DS_UnableSelectVideoFormat, (unsigned int)hr);
	}
    }

    priv->chains[0]->rbuf=calloc(1,sizeof(grabber_ringbuffer_t));
    if(!priv->chains[0]->rbuf)
        return E_OUTOFMEMORY;

    if (priv->tv_param->buffer_size >= 0) {
	priv->chains[0]->rbuf->buffersize = priv->tv_param->buffer_size;
    } else {
	priv->chains[0]->rbuf->buffersize = 16;
    }

    priv->chains[0]->rbuf->buffersize *= 1024 * 1024;
    hr=build_sub_graph(priv, priv->chains[0], &PIN_CATEGORY_CAPTURE);
    if(FAILED(hr)){
        mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_UnableBuildVideoSubGraph,(unsigned int)hr);
        return hr;
    }
    return S_OK;
}

/**
 * \brief build audio stream chain in graph
 * \param priv private data structure
 *
 * \return S_OK if chain was built successfully, apropriate error code otherwise
 */
static HRESULT build_audio_chain(priv_t *priv)
{
    HRESULT hr;

    if(priv->chains[1]->rbuf)
        return S_OK;

    if(priv->immediate_mode)
        return S_OK;

    if (priv->chains[1]->pStreamConfig) {
	hr = OLE_CALL_ARGS(priv->chains[1]->pStreamConfig, SetFormat,
		       priv->chains[1]->pmt);
	if (FAILED(hr)) {
	    mp_msg(MSGT_TV,MSGL_ERR,MSGTR_TVI_DS_UnableSelectAudioFormat, (unsigned int)hr);
	}
    }

    if(priv->chains[1]->pmt){
        priv->chains[1]->rbuf=calloc(1,sizeof(grabber_ringbuffer_t));
        if(!priv->chains[1]->rbuf)
            return E_OUTOFMEMORY;

        /* let the audio buffer be the same size (in seconds) than video one */
        priv->chains[1]->rbuf->buffersize=audio_buf_size_from_video(
                priv->chains[0]->rbuf->buffersize,
                (((VIDEOINFOHEADER *) priv->chains[0]->pmt->pbFormat)->dwBitRate),
                (((WAVEFORMATEX *) (priv->chains[1]->pmt->pbFormat))->nAvgBytesPerSec));

        hr=build_sub_graph(priv, priv->chains[1],&PIN_CATEGORY_CAPTURE);
        if(FAILED(hr)){
            mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_UnableBuildAudioSubGraph,(unsigned int)hr);
            return 0;
        }
    }
    return S_OK;
}

/**
 * \brief build VBI stream chain in graph
 * \param priv private data structure
 *
 * \return S_OK if chain was built successfully, apropriate error code otherwise
 */
static HRESULT build_vbi_chain(priv_t *priv)
{
#ifdef CONFIG_TV_TELETEXT
    HRESULT hr;

    if(priv->chains[2]->rbuf)
        return S_OK;

    if(priv->tv_param->tdevice)
    {
        priv->chains[2]->rbuf=calloc(1,sizeof(grabber_ringbuffer_t));
        if(!priv->chains[2]->rbuf)
            return E_OUTOFMEMORY;

        init_ringbuffer(priv->chains[2]->rbuf,24,priv->tsp.bufsize);

        hr=build_sub_graph(priv, priv->chains[2],&PIN_CATEGORY_VBI);
        if(FAILED(hr)){
            mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_UnableBuildVBISubGraph,(unsigned int)hr);
            return 0;
        }
    }
#endif
    return S_OK;
}

/**
 * \brief playback/capture real start
 *
 * \param priv driver's private data structure 
 *
 * \return 1 if success, 0 - otherwise
 *
 * TODO: move some code from init() here
 */                          
static int start(priv_t * priv)
{
    HRESULT hr;

    hr = build_video_chain(priv);
    if(FAILED(hr))
        return 0;

    hr = build_audio_chain(priv);
    if(FAILED(hr))
        return 0;

    hr = build_vbi_chain(priv);
    if(FAILED(hr))
        return 0;

    /*
       Graph is ready to capture. Starting graph.
     */
    if (mp_msg_test(MSGT_TV, MSGL_DBG2)) {
	mp_msg(MSGT_TV, MSGL_DBG2, "Debug pause 10sec\n");
	usec_sleep(10000000);
	mp_msg(MSGT_TV, MSGL_DBG2, "Debug pause end\n");
    }
    if (!priv->pMediaControl) {
        mp_msg(MSGT_TV,MSGL_ERR,MSGTR_TVI_DS_UnableGetMediaControlInterface,(unsigned int)E_POINTER);
        return 0;
    }
    hr = OLE_CALL(priv->pMediaControl, Run);
    if (FAILED(hr)) {
	mp_msg(MSGT_TV,MSGL_ERR,MSGTR_TVI_DS_UnableStartGraph, (unsigned int)hr);
	return 0;
    }
    mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Graph is started.\n");
    priv->state = 1;

    return 1;
}

/**
 * \brief driver initialization
 *
 * \param priv driver's private data structure 
 *
 * \return 1 if success, 0 - otherwise
 */
static int init(priv_t * priv)
{
    HRESULT hr;
    int result = 0;
    long lInput, lTunerInput;
    IEnumFilters *pEnum;
    IBaseFilter *pFilter;
    IPin *pVPOutPin;
    int i;

    priv->state=0;

    CoInitialize(NULL);

    for(i=0; i<3;i++)
        priv->chains[i] = calloc(1, sizeof(chain_t));

    priv->chains[0]->type=video;
    priv->chains[0]->majortype=&MEDIATYPE_Video;
    priv->chains[0]->pin_category=&PIN_CATEGORY_CAPTURE;
    priv->chains[1]->type=audio;
    priv->chains[1]->majortype=&MEDIATYPE_Audio;
    priv->chains[1]->pin_category=&PIN_CATEGORY_CAPTURE;
    priv->chains[2]->type=vbi;
    priv->chains[2]->majortype=&MEDIATYPE_VBI;
    priv->chains[2]->pin_category=&PIN_CATEGORY_VBI;

    do{
        hr = CoCreateInstance((GUID *) & CLSID_FilterGraph, NULL,
    			  CLSCTX_INPROC_SERVER, &IID_IGraphBuilder,
    			  (void **) &priv->pGraph);
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: CoCreateInstance(FilterGraph) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }
        //Debug
        if (mp_msg_test(MSGT_TV, MSGL_DBG2)) {
            AddToRot((IUnknown *) priv->pGraph, &(priv->dwRegister));
        }

        hr = CoCreateInstance((GUID *) & CLSID_CaptureGraphBuilder2, NULL,
    			  CLSCTX_INPROC_SERVER, &IID_ICaptureGraphBuilder2,
    			  (void **) &priv->pBuilder);
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: CoCreateInstance(CaptureGraphBuilder) call failed. Error:0x%x\n", (unsigned int)hr);
            break;
        }
    
        hr = OLE_CALL_ARGS(priv->pBuilder, SetFiltergraph, priv->pGraph);
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_ERR, "tvi_dshow: SetFiltergraph call failed. Error:0x%x\n",(unsigned int)hr);
            break;
        }
    
        mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Searching for available video capture devices\n");
        priv->chains[0]->pCaptureFilter = find_capture_device(priv->dev_index, &CLSID_VideoInputDeviceCategory);
        if(!priv->chains[0]->pCaptureFilter){
            mp_msg(MSGT_TV,MSGL_ERR, MSGTR_TVI_DS_NoVideoCaptureDevice);
            break;
        }
        hr = OLE_CALL_ARGS(priv->pGraph, AddFilter, priv->chains[0]->pCaptureFilter, NULL);
        if(FAILED(hr)){
            mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Unable to add video capture device to Directshow graph. Error:0x%x\n", (unsigned int)hr);
            break;
        }
        mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Searching for available audio capture devices\n");
        if (priv->adev_index != -1) {
        	priv->chains[1]->pCaptureFilter = find_capture_device(priv->adev_index, &CLSID_AudioInputDeviceCategory);	//output available audio edevices
                if(!priv->chains[1]->pCaptureFilter){
                    mp_msg(MSGT_TV,MSGL_ERR, MSGTR_TVI_DS_NoAudioCaptureDevice);
                    break;
                }

        	hr = OLE_CALL_ARGS(priv->pGraph, AddFilter, priv->chains[1]->pCaptureFilter, NULL);
                if(FAILED(hr)){
        	    mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: Unable to add audio capture device to Directshow graph. Error:0x%x\n", (unsigned int)hr);
                    break;
                }
        } else
        	hr = OLE_QUERYINTERFACE(priv->chains[0]->pCaptureFilter, IID_IBaseFilter, priv->chains[1]->pCaptureFilter);

 	/* increase refrence counter for capture filter ad store pointer into vbi chain structure too */
      	hr = OLE_QUERYINTERFACE(priv->chains[0]->pCaptureFilter, IID_IBaseFilter, priv->chains[2]->pCaptureFilter);

        hr = OLE_QUERYINTERFACE(priv->chains[0]->pCaptureFilter, IID_IAMVideoProcAmp,priv->pVideoProcAmp);
        if (FAILED(hr) && hr != E_NOINTERFACE)
            mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Get IID_IAMVideoProcAmp failed (0x%x).\n", (unsigned int)hr);

        if (hr != S_OK) {
            mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TVI_DS_VideoAdjustigNotSupported);
            priv->pVideoProcAmp = NULL;
        }

        hr = OLE_CALL_ARGS(priv->pBuilder, FindInterface,
        		   &PIN_CATEGORY_CAPTURE,
        		   priv->chains[0]->majortype,
        		   priv->chains[0]->pCaptureFilter,
        		   &IID_IAMCrossbar, (void **) &(priv->pCrossbar));
        if (FAILED(hr)) {
            mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TVI_DS_SelectingInputNotSupported);
            priv->pCrossbar = NULL;
        }

        if (priv->tv_param->amode >= 0) {
	    IAMTVAudio *pTVAudio;
	    hr = OLE_CALL_ARGS(priv->pBuilder, FindInterface, NULL, NULL,priv->chains[0]->pCaptureFilter,&IID_IAMTVAudio, (void *) &pTVAudio);
            if (hr == S_OK) {
                switch (priv->tv_param->amode) {
                case 0:
                    hr = OLE_CALL_ARGS(pTVAudio, put_TVAudioMode, AMTVAUDIO_MODE_MONO);
                    break;
                case 1:
                    hr = OLE_CALL_ARGS(pTVAudio, put_TVAudioMode, AMTVAUDIO_MODE_STEREO);
                    break;
                case 2:
                    hr = OLE_CALL_ARGS(pTVAudio, put_TVAudioMode,
			       AMTVAUDIO_MODE_LANG_A);
                    break;
                case 3:
                    hr = OLE_CALL_ARGS(pTVAudio, put_TVAudioMode,
			       AMTVAUDIO_MODE_LANG_B);
                    break;
                }
                OLE_RELEASE_SAFE(pTVAudio);
                if (FAILED(hr))
                    mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TVI_DS_UnableSetAudioMode, priv->tv_param->amode,(unsigned int)hr);
            }
        }

        // Video chain initialization
        hr = init_chain_common(priv->pBuilder, priv->chains[0]);
        if(FAILED(hr))
            break;

        /*
           Audio chain initialization
           Since absent audio stream is not fatal,
           at least one NULL pointer should be kept in format arrays
           (to avoid another additional check everywhere for array presence).
        */
        hr = init_chain_common(priv->pBuilder, priv->chains[1]);
        if(FAILED(hr))
        {
            mp_msg(MSGT_TV, MSGL_V, "tvi_dshow: Unable to initialize audio chain (Error:0x%x). Audio disabled\n", (unsigned long)hr);
            priv->chains[1]->arpmt=calloc(1, sizeof(AM_MEDIA_TYPE*));
            priv->chains[1]->arStreamCaps=calloc(1, sizeof(void*));
        }

        /*
           VBI chain initialization
           Since absent VBI stream is not fatal,
           at least one NULL pointer should be kept in format arrays
           (to avoid another additional check everywhere for array presence).
        */
        hr = init_chain_common(priv->pBuilder, priv->chains[2]);
        if(FAILED(hr))
        {
            mp_msg(MSGT_TV, MSGL_V, "tvi_dshow: Unable to initialize VBI chain (Error:0x%x). Teletext disabled\n", (unsigned long)hr);
            priv->chains[2]->arpmt=calloc(1, sizeof(AM_MEDIA_TYPE*));
            priv->chains[2]->arStreamCaps=calloc(1, sizeof(void*));
        }

        if (!priv->chains[0]->pStreamConfig)
            mp_msg(MSGT_TV, MSGL_INFO, MSGTR_TVI_DS_ChangingWidthHeightNotSupported);

        if (!priv->chains[0]->arpmt[priv->chains[0]->nFormatUsed]
            || !extract_video_format(priv->chains[0]->arpmt[priv->chains[0]->nFormatUsed],
				 &(priv->fcc), &(priv->width),
				 &(priv->height))) {
            mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_ErrorParsingVideoFormatStruct);
            break;
        }

        if (priv->chains[1]->arpmt[priv->chains[1]->nFormatUsed]) {
            if (!extract_audio_format(priv->chains[1]->pmt, &(priv->samplerate), NULL, NULL)) {
                mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_ErrorParsingAudioFormatStruct);
                DisplayMediaType("audio format failed",priv->chains[1]->arpmt[priv->chains[1]->nFormatUsed]);
                break;
            }
        }

        hr = OLE_QUERYINTERFACE(priv->pGraph, IID_IMediaControl,priv->pMediaControl);
        if(FAILED(hr)){
            mp_msg(MSGT_TV,MSGL_ERR, MSGTR_TVI_DS_UnableGetMediaControlInterface,(unsigned int)hr);
            break;
        }
        hr = OLE_CALL_ARGS(priv->pBuilder, FindInterface,
		   &PIN_CATEGORY_CAPTURE, NULL,
		   priv->chains[0]->pCaptureFilter,
		   &IID_IAMTVTuner, (void **) &(priv->pTVTuner));

        if (!priv->pTVTuner) {
            mp_msg(MSGT_TV, MSGL_DBG2, "tvi_dshow: Unable to access IAMTVTuner (0x%x)\n", (unsigned int)hr);
        }

        // shows Tuner capabilities
        get_capabilities(priv);

        if (priv->pTVTuner) {
            hr = OLE_CALL_ARGS(priv->pTVTuner, put_CountryCode,
		       chanlist2country(priv->tv_param->chanlist));
            if(FAILED(hr)){
                mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: Call to put_CountryCode failed. Error:0x%x\n",(unsigned int)hr);
            }

            hr = OLE_CALL_ARGS(priv->pTVTuner, put_Mode, AMTUNER_MODE_TV);
            if(FAILED(hr)){
                mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: Call to put_Mode failed. Error:0x%x\n",(unsigned int)hr);
                break;
            }

            hr = OLE_CALL_ARGS(priv->pTVTuner, get_ConnectInput, &lInput);
            if(FAILED(hr)){
                mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: Call to get_ConnectInput failed. Error:0x%x\n",(unsigned int)hr);
                break;
            }

            /* small hack */
            lTunerInput = strstr(priv->tv_param->chanlist, "cable") ? TunerInputCable : TunerInputAntenna;

            hr = OLE_CALL_ARGS(priv->pTVTuner, put_InputType, lInput, lTunerInput);
            if(FAILED(hr)){
                mp_msg(MSGT_TV,MSGL_DBG2, "tvi_dshow: Call to put_InputType failed. Error:0x%x\n",(unsigned int)hr);
                break;
            }

        }

        /**
         for VIVO  cards we should check if preview pin is available on video capture device.
         If it is not, we have to connect Video Port Manager filter to VP pin of capture device filter.
         Otherwise we will get 0x8007001f (Device is not functioning properly) when attempting to start graph
        */
        hr = OLE_CALL_ARGS(priv->pBuilder, FindPin,
		   (IUnknown *) priv->chains[0]->pCaptureFilter,
		   PINDIR_OUTPUT,
		   &PIN_CATEGORY_VIDEOPORT, NULL, FALSE,
		   0, (IPin **) & pVPOutPin);
        if (SUCCEEDED(hr)) {
            hr = OLE_CALL_ARGS(priv->pGraph, Render, pVPOutPin);
            OLE_RELEASE_SAFE(pVPOutPin);

            if (FAILED(hr)) {
                mp_msg(MSGT_TV,MSGL_ERR, MSGTR_TVI_DS_UnableTerminateVPPin, (unsigned int)hr);
                break;
            }
        }

        OLE_CALL_ARGS(priv->pGraph, EnumFilters, &pEnum);
        while (OLE_CALL_ARGS(pEnum, Next, 1, &pFilter, NULL) == S_OK) {
            LPVIDEOWINDOW pVideoWindow;
            hr = OLE_QUERYINTERFACE(pFilter, IID_IVideoWindow, pVideoWindow);
            if (SUCCEEDED(hr))
            {
                if(priv->tv_param->hidden_vp_renderer){
                    OLE_CALL_ARGS(pVideoWindow,put_Visible,/* OAFALSE*/ 0);
                    OLE_CALL_ARGS(pVideoWindow,put_AutoShow,/* OAFALSE*/ 0);
                }else
                {
                    OLE_CALL_ARGS(priv->pGraph, RemoveFilter, pFilter);
                }
                OLE_RELEASE_SAFE(pVideoWindow);
            }
            OLE_RELEASE_SAFE(pFilter);
        }
        OLE_RELEASE_SAFE(pEnum);
        if(priv->tv_param->system_clock)
        {
            LPREFERENCECLOCK rc;
            IBaseFilter* pBF;
            hr = CoCreateInstance((GUID *) & CLSID_SystemClock, NULL,
			      CLSCTX_INPROC_SERVER, &IID_IReferenceClock,
			      (void *) &rc);

            OLE_QUERYINTERFACE(priv->pBuilder,IID_IBaseFilter,pBF);
            OLE_CALL_ARGS(pBF,SetSyncSource,rc);
        }
#ifdef CONFIG_TV_TELETEXT
       if(vbi_get_props(priv,&(priv->tsp))!=TVI_CONTROL_TRUE)
           break;
#endif
        result = 1;
    } while(0);

    if (!result){
        mp_msg(MSGT_TV,MSGL_ERR, MSGTR_TVI_DS_GraphInitFailure);
	uninit(priv);
    }
    return result;
}

/**
 * \brief chain uninitialization
 * \param chain chain data structure
 */
static void destroy_chain(chain_t *chain)
{
    int i;

    if(!chain)
        return;

    OLE_RELEASE_SAFE(chain->pStreamConfig);
    OLE_RELEASE_SAFE(chain->pCaptureFilter);
    OLE_RELEASE_SAFE(chain->pCSGCB);
    OLE_RELEASE_SAFE(chain->pCapturePin);
    OLE_RELEASE_SAFE(chain->pSGIn);
    OLE_RELEASE_SAFE(chain->pSG);
    OLE_RELEASE_SAFE(chain->pSGF);

    if (chain->pmt)
	DeleteMediaType(chain->pmt);

    if (chain->arpmt) {
	for (i = 0; chain->arpmt[i]; i++) {
	    DeleteMediaType(chain->arpmt[i]);
	}
	free(chain->arpmt);
    }

    if (chain->arStreamCaps) {
	for (i = 0; chain->arStreamCaps[i]; i++) {
	    free(chain->arStreamCaps[i]);
	}
	free(chain->arStreamCaps);
    }

    if (chain->rbuf) {
	destroy_ringbuffer(chain->rbuf);
	free(chain->rbuf);
	chain->rbuf = NULL;
    }
    free(chain);
}
/**
 * \brief driver uninitialization
 *
 * \param priv driver's private data structure 
 *
 * \return always 1
 */
static int uninit(priv_t * priv)
{
    int i;
    if (!priv)
	return 1;
    //Debug
    if (priv->dwRegister) {
        RemoveFromRot(priv->dwRegister);
    }
#ifdef CONFIG_TV_TELETEXT
    teletext_control(priv->priv_vbi,TV_VBI_CONTROL_STOP,(void*)1);
#endif
    //stop audio grabber thread

    if (priv->state && priv->pMediaControl) {
	OLE_CALL(priv->pMediaControl, Stop);
    }
    OLE_RELEASE_SAFE(priv->pMediaControl);
    priv->state = 0;

    if (priv->pGraph) {
	if (priv->chains[0]->pCaptureFilter)
	    OLE_CALL_ARGS(priv->pGraph, RemoveFilter, priv->chains[0]->pCaptureFilter);
	if (priv->chains[1]->pCaptureFilter)
	    OLE_CALL_ARGS(priv->pGraph, RemoveFilter, priv->chains[1]->pCaptureFilter);
    }
    OLE_RELEASE_SAFE(priv->pCrossbar);
    OLE_RELEASE_SAFE(priv->pVideoProcAmp);
    OLE_RELEASE_SAFE(priv->pGraph);
    OLE_RELEASE_SAFE(priv->pBuilder);
    if(priv->freq_table){
        priv->freq_table_len=-1;
        free(priv->freq_table);
        priv->freq_table=NULL;
    }

    for(i=0; i<3;i++)
    {
        destroy_chain(priv->chains[i]);
        priv->chains[i] = NULL;
    }
    CoUninitialize();
    return 1;
}

/**
 * \brief driver pre-initialization
 *
 * \param device string, containing device name in form "x[.y]", where x is video capture device
 *               (default: 0, first available); y (if given)  sets audio capture device
 *
 * \return 1 if success,0 - otherwise
 */
static tvi_handle_t *tvi_init_dshow(tv_param_t* tv_param)
{
    tvi_handle_t *h;
    priv_t *priv;
    int a;

    h = new_handle();
    if (!h)
	return NULL;

    priv = h->priv;

    memset(priv, 0, sizeof(priv_t));
    priv->direct_setfreq_call = 1;	//first using direct call. if it fails, workaround will be enabled
    priv->direct_getfreq_call = 1;	//first using direct call. if it fails, workaround will be enabled
    priv->adev_index = -1;
    priv->freq_table_len=-1;
    priv->tv_param=tv_param;

    if (tv_param->device) {
	if (sscanf(tv_param->device, "%d", &a) == 1) {
	    priv->dev_index = a;
	} else {
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_WrongDeviceParam, tv_param->device);
	    free_handle(h);
	    return NULL;
	}
	if (priv->dev_index < 0) {
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_WrongDeviceIndex, a);
	    free_handle(h);
	    return NULL;
	}
    }
    if (tv_param->adevice) {
	if (sscanf(tv_param->adevice, "%d", &a) == 1) {
	    priv->adev_index = a;
	} else {
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_WrongADeviceParam, tv_param->adevice);
	    free_handle(h);
	    return NULL;
	}
	if (priv->dev_index < 0) {
	    mp_msg(MSGT_TV, MSGL_ERR, MSGTR_TVI_DS_WrongADeviceIndex, a);
	    free_handle(h);
	    return NULL;
	}
    }
    return h;
}

/**
 * \brief driver's ioctl handler
 *
 * \param priv driver's private data structure 
 * \param cmd ioctl command
 * \param arg ioct command's parameter
 *
 * \return TVI_CONTROL_TRUE if success 
 * \return TVI_CONTROL_FALSE if failure
 * \return TVI_CONTROL_UNKNOWN if unknowm cmd called
 */
static int control(priv_t * priv, int cmd, void *arg)
{
    switch (cmd) {
/* need rewrite */
    case TVI_CONTROL_VID_SET_FORMAT:
	{
	    int fcc, i,j;
	    void* tmp,*tmp2;
	    int result = TVI_CONTROL_TRUE;

	    if (priv->state)
		return TVI_CONTROL_FALSE;
	    fcc = *(int *) arg;

            if(!priv->chains[0]->arpmt)
                return TVI_CONTROL_FALSE;
	    for (i = 0; priv->chains[0]->arpmt[i]; i++)
		if (check_video_format
		    (priv->chains[0]->arpmt[i], fcc, priv->width, priv->height))
		    break;
	    if (!priv->chains[0]->arpmt[i])
	    {
		int fps = 0;
		VIDEOINFOHEADER* Vhdr = NULL;
		AM_MEDIA_TYPE *pmt;

		mp_msg(MSGT_TV, MSGL_V, "tvi_dshow: will try also use undeclared video format: %dx%d, %s\n",priv->width, priv->height, vo_format_name(fcc));

		if (priv->chains[0]->arpmt[0])
		    Vhdr = (VIDEOINFOHEADER *) priv->chains[0]->arpmt[0]->pbFormat;

		if(Vhdr && Vhdr->bmiHeader.biSizeImage)
		    fps = Vhdr->dwBitRate / (8 * Vhdr->bmiHeader.biSizeImage);

		pmt=create_video_format(fcc, priv->width, priv->height, fps);
		if(!pmt)
		{
		    mp_msg(MSGT_TV, MSGL_V, "tvi_dshow: Unable to create AM_MEDIA_TYPE structure for given format\n");
		    return TVI_CONTROL_FALSE;
		}
		priv->chains[0]->arpmt=realloc(priv->chains[0]->arpmt, (i+2)*sizeof(AM_MEDIA_TYPE*));
		priv->chains[0]->arpmt[i+1] = NULL;
		priv->chains[0]->arpmt[i] = pmt;

		priv->chains[0]->arStreamCaps=realloc(priv->chains[0]->arStreamCaps, (i+2)*sizeof(void*));
		priv->chains[0]->arpmt[i+1] = NULL;

		result = TVI_CONTROL_FALSE;
	    }


            tmp=priv->chains[0]->arpmt[i];
            tmp2=priv->chains[0]->arStreamCaps[i];
            for(j=i; j>0; j--)
            {
                priv->chains[0]->arpmt[j] = priv->chains[0]->arpmt[j-1];
                priv->chains[0]->arStreamCaps[j] = priv->chains[0]->arStreamCaps[j-1];
            }
            priv->chains[0]->arpmt[0] = tmp;
            priv->chains[0]->arStreamCaps[0] = tmp2;

	    priv->chains[0]->nFormatUsed = 0;

	    if (priv->chains[0]->pmt)
		DeleteMediaType(priv->chains[0]->pmt);
	    priv->chains[0]->pmt =
		CreateMediaType(priv->chains[0]->arpmt[priv->chains[0]->nFormatUsed]);
	    DisplayMediaType("VID_SET_FORMAT", priv->chains[0]->pmt);
	    /*
	       Setting width & height to preferred by driver values
	     */
	    extract_video_format(priv->chains[0]->arpmt[priv->chains[0]->nFormatUsed],
				 &(priv->fcc), &(priv->width),
				 &(priv->height));
	    return result;
	}
    case TVI_CONTROL_VID_GET_FORMAT:
	{
            if(!priv->chains[0]->pmt)
                return TVI_CONTROL_FALSE;
	    /*
	       Build video chain (for video format negotiation).
	       If this was done before, routine will do nothing.
	    */
	    build_video_chain(priv);
	    DisplayMediaType("VID_GET_FORMAT", priv->chains[0]->pmt);
	    if (priv->fcc) {
		*(int *) arg = priv->fcc;
		return TVI_CONTROL_TRUE;
	    } else
		return TVI_CONTROL_FALSE;
	}
    case TVI_CONTROL_VID_SET_WIDTH:
	{
	    VIDEO_STREAM_CONFIG_CAPS *pCaps;
	    VIDEOINFOHEADER *Vhdr;
	    int width = *(int *) arg;
	    if (priv->state)
		return TVI_CONTROL_FALSE;

	    pCaps = priv->chains[0]->arStreamCaps[priv->chains[0]->nFormatUsed];
	    if (!pCaps)
		return TVI_CONTROL_FALSE;
	    if (width < pCaps->MinOutputSize.cx
		|| width > pCaps->MaxOutputSize.cx)
		return TVI_CONTROL_FALSE;

	    if (width % pCaps->OutputGranularityX)
		return TVI_CONTROL_FALSE;

	    if (!priv->chains[0]->pmt || !priv->chains[0]->pmt->pbFormat)
		return TVI_CONTROL_FALSE;
	    Vhdr = (VIDEOINFOHEADER *) priv->chains[0]->pmt->pbFormat;
	    Vhdr->bmiHeader.biWidth = width;
	    priv->chains[0]->pmt->lSampleSize = Vhdr->bmiHeader.biSizeImage =
		labs(Vhdr->bmiHeader.biBitCount * Vhdr->bmiHeader.biWidth *
		     Vhdr->bmiHeader.biHeight) >> 3;

	    priv->width = width;

	    return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_VID_GET_WIDTH:
	{
	    if (priv->width) {
		*(int *) arg = priv->width;
		return TVI_CONTROL_TRUE;
	    } else
		return TVI_CONTROL_FALSE;
	}
    case TVI_CONTROL_VID_CHK_WIDTH:
	{
	    VIDEO_STREAM_CONFIG_CAPS *pCaps;
	    int width = *(int *) arg;
	    pCaps = priv->chains[0]->arStreamCaps[priv->chains[0]->nFormatUsed];
	    if (!pCaps)
		return TVI_CONTROL_FALSE;
	    if (width < pCaps->MinOutputSize.cx
		|| width > pCaps->MaxOutputSize.cx)
		return TVI_CONTROL_FALSE;

	    if (width % pCaps->OutputGranularityX)
		return TVI_CONTROL_FALSE;
	    return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_VID_SET_HEIGHT:
	{
	    VIDEO_STREAM_CONFIG_CAPS *pCaps;
	    VIDEOINFOHEADER *Vhdr;
	    int height = *(int *) arg;
	    if (priv->state)
		return TVI_CONTROL_FALSE;

	    pCaps = priv->chains[0]->arStreamCaps[priv->chains[0]->nFormatUsed];
	    if (!pCaps)
		return TVI_CONTROL_FALSE;
	    if (height < pCaps->MinOutputSize.cy
		|| height > pCaps->MaxOutputSize.cy)
		return TVI_CONTROL_FALSE;

	    if (height % pCaps->OutputGranularityY)
		return TVI_CONTROL_FALSE;

	    if (!priv->chains[0]->pmt || !priv->chains[0]->pmt->pbFormat)
		return TVI_CONTROL_FALSE;
	    Vhdr = (VIDEOINFOHEADER *) priv->chains[0]->pmt->pbFormat;

	    if (Vhdr->bmiHeader.biHeight < 0)
		Vhdr->bmiHeader.biHeight = -height;
	    else
		Vhdr->bmiHeader.biHeight = height;
	    priv->chains[0]->pmt->lSampleSize = Vhdr->bmiHeader.biSizeImage =
		labs(Vhdr->bmiHeader.biBitCount * Vhdr->bmiHeader.biWidth *
		     Vhdr->bmiHeader.biHeight) >> 3;

	    priv->height = height;
	    return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_VID_GET_HEIGHT:
	{
	    if (priv->height) {
		*(int *) arg = priv->height;
		return TVI_CONTROL_TRUE;
	    } else
		return TVI_CONTROL_FALSE;
	}
    case TVI_CONTROL_VID_CHK_HEIGHT:
	{
	    VIDEO_STREAM_CONFIG_CAPS *pCaps;
	    int height = *(int *) arg;
	    pCaps = priv->chains[0]->arStreamCaps[priv->chains[0]->nFormatUsed];
	    if (!pCaps)
		return TVI_CONTROL_FALSE;
	    if (height < pCaps->MinOutputSize.cy
		|| height > pCaps->MaxOutputSize.cy)
		return TVI_CONTROL_FALSE;

	    if (height % pCaps->OutputGranularityY)
		return TVI_CONTROL_FALSE;

	    return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_IS_AUDIO:
	if (!priv->chains[1]->pmt)
	    return TVI_CONTROL_FALSE;
	else
	    return TVI_CONTROL_TRUE;
    case TVI_CONTROL_IS_VIDEO:
	return TVI_CONTROL_TRUE;
    case TVI_CONTROL_AUD_GET_FORMAT:
	{
	    *(int *) arg = AF_FORMAT_S16_LE;
	    if (!priv->chains[1]->pmt)
		return TVI_CONTROL_FALSE;
	    else
		return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_AUD_GET_CHANNELS:
	{
	    *(int *) arg = priv->channels;
	    if (!priv->chains[1]->pmt)
		return TVI_CONTROL_FALSE;
	    else
		return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_AUD_SET_SAMPLERATE:
	{
	    int i, samplerate;
	    if (priv->state)
		return TVI_CONTROL_FALSE;
	    if (!priv->chains[1]->arpmt[0])
		return TVI_CONTROL_FALSE;

	    samplerate = *(int *) arg;;

	    for (i = 0; priv->chains[1]->arpmt[i]; i++)
		if (check_audio_format
		    (priv->chains[1]->arpmt[i], samplerate, 16, priv->channels))
		    break;
	    if (!priv->chains[1]->arpmt[i]) {	
                //request not found. failing back to first available
		mp_msg(MSGT_TV, MSGL_WARN, MSGTR_TVI_DS_SamplerateNotsupported, samplerate);
		i = 0;
	    }
	    if (priv->chains[1]->pmt)
		DeleteMediaType(priv->chains[1]->pmt);
	    priv->chains[1]->pmt = CreateMediaType(priv->chains[1]->arpmt[i]);
	    extract_audio_format(priv->chains[1]->arpmt[i], &(priv->samplerate),
				 NULL, &(priv->channels));
	    return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_AUD_GET_SAMPLERATE:
	{
	    *(int *) arg = priv->samplerate;
	    if (!priv->samplerate)
		return TVI_CONTROL_FALSE;
	    if (!priv->chains[1]->pmt)
		return TVI_CONTROL_FALSE;
	    else
		return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_AUD_GET_SAMPLESIZE:
	{
	    WAVEFORMATEX *pWF;
	    if (!priv->chains[1]->pmt)
		return TVI_CONTROL_FALSE;
	    if (!priv->chains[1]->pmt->pbFormat)
		return TVI_CONTROL_FALSE;
	    pWF = (WAVEFORMATEX *) priv->chains[1]->pmt->pbFormat;
	    *(int *) arg = pWF->wBitsPerSample / 8;
	    return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_IS_TUNER:
	{
	    if (!priv->pTVTuner)
		return TVI_CONTROL_FALSE;

	    return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_TUN_SET_NORM:
	{
	    IAMAnalogVideoDecoder *pVD;
	    long lAnalogFormat;
	    int i;
	    HRESULT hr;

	    i = *(int *) arg;
	    i--;
	    if (i < 0 || i >= tv_available_norms_count)
		return TVI_CONTROL_FALSE;
	    lAnalogFormat = tv_norms[tv_available_norms[i]].index;

	    hr = OLE_QUERYINTERFACE(priv->chains[0]->pCaptureFilter,IID_IAMAnalogVideoDecoder, pVD);
	    if (hr != S_OK)                            
		return TVI_CONTROL_FALSE;
	    hr = OLE_CALL_ARGS(pVD, put_TVFormat, lAnalogFormat);
	    OLE_RELEASE_SAFE(pVD);
	    if (FAILED(hr))
		return TVI_CONTROL_FALSE;
	    else
		return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_TUN_GET_NORM:
	{
	    long lAnalogFormat;
	    int i;
	    HRESULT hr;
	    IAMAnalogVideoDecoder *pVD;

	    hr = OLE_QUERYINTERFACE(priv->chains[0]->pCaptureFilter,IID_IAMAnalogVideoDecoder, pVD);
	    if (hr == S_OK) {
		hr = OLE_CALL_ARGS(pVD, get_TVFormat, &lAnalogFormat);
		OLE_RELEASE_SAFE(pVD);
	    }

	    if (FAILED(hr)) {	//trying another method
		if (!priv->pTVTuner)
		    return TVI_CONTROL_FALSE;
                hr=OLE_CALL_ARGS(priv->pTVTuner, get_TVFormat, &lAnalogFormat);
		if (FAILED(hr))
		    return TVI_CONTROL_FALSE;
	    }
	    for (i = 0; i < tv_available_norms_count; i++) {
		if (tv_norms[tv_available_norms[i]].index == lAnalogFormat) {
		    *(int *) arg = i + 1;
		    return TVI_CONTROL_TRUE;
		}
	    }
	    return TVI_CONTROL_FALSE;
	}
    case TVI_CONTROL_SPC_GET_NORMID:
	{
	    int i;
	    if (!priv->pTVTuner)
		return TVI_CONTROL_FALSE;
	    for (i = 0; i < tv_available_norms_count; i++) {
		if (!strcasecmp
		    (tv_norms[tv_available_norms[i]].name, (char *) arg)) {
		    *(int *) arg = i + 1;
		    return TVI_CONTROL_TRUE;
		}
	    }
	    return TVI_CONTROL_FALSE;
	}
    case TVI_CONTROL_SPC_SET_INPUT:
	{
	    return set_crossbar_input(priv, *(int *) arg);
	}
    case TVI_CONTROL_TUN_GET_FREQ:
	{
	    unsigned long lFreq;
	    int ret;
	    if (!priv->pTVTuner)
		return TVI_CONTROL_FALSE;

	    ret = get_frequency(priv, &lFreq);
	    lFreq = lFreq / (1000000/16);	//convert from Hz to 1/16 MHz units

	    *(unsigned long *) arg = lFreq;
	    return ret;
	}
    case TVI_CONTROL_TUN_SET_FREQ:
	{
	    unsigned long nFreq = *(unsigned long *) arg;
	    if (!priv->pTVTuner)
		return TVI_CONTROL_FALSE;
	    //convert to Hz
	    nFreq = (1000000/16) * nFreq;	//convert from 1/16 MHz units to Hz
	    return set_frequency(priv, nFreq);
	}
    case TVI_CONTROL_VID_SET_HUE:
	return set_control(priv, VideoProcAmp_Hue, *(int *) arg);
    case TVI_CONTROL_VID_GET_HUE:
	return get_control(priv, VideoProcAmp_Hue, (int *) arg);
    case TVI_CONTROL_VID_SET_CONTRAST:
	return set_control(priv, VideoProcAmp_Contrast, *(int *) arg);
    case TVI_CONTROL_VID_GET_CONTRAST:
	return get_control(priv, VideoProcAmp_Contrast, (int *) arg);
    case TVI_CONTROL_VID_SET_SATURATION:
	return set_control(priv, VideoProcAmp_Saturation, *(int *) arg);
    case TVI_CONTROL_VID_GET_SATURATION:
	return get_control(priv, VideoProcAmp_Saturation, (int *) arg);
    case TVI_CONTROL_VID_SET_BRIGHTNESS:
	return set_control(priv, VideoProcAmp_Brightness, *(int *) arg);
    case TVI_CONTROL_VID_GET_BRIGHTNESS:
	return get_control(priv, VideoProcAmp_Brightness, (int *) arg);

    case TVI_CONTROL_VID_GET_FPS:
	{
	    VIDEOINFOHEADER *Vhdr;
	    if (!priv->chains[0]->pmt)
		return TVI_CONTROL_FALSE;
	    if (!priv->chains[0]->pmt->pbFormat)
		return TVI_CONTROL_FALSE;
	    Vhdr = (VIDEOINFOHEADER *) priv->chains[0]->pmt->pbFormat;
	    *(float *) arg =
		(1.0 * Vhdr->dwBitRate) / (Vhdr->bmiHeader.biSizeImage * 8);
	    return TVI_CONTROL_TRUE;
	}
    case TVI_CONTROL_IMMEDIATE:
	priv->immediate_mode = 1;
	return TVI_CONTROL_TRUE;
#ifdef CONFIG_TV_TELETEXT
    case TVI_CONTROL_VBI_INIT:
    {
        void* ptr;
        ptr=&(priv->tsp);
        if(teletext_control(NULL,TV_VBI_CONTROL_START,&ptr)==TVI_CONTROL_TRUE)
            priv->priv_vbi=ptr;
        else
            priv->priv_vbi=NULL;
        return TVI_CONTROL_TRUE;
    }
    default:
        return teletext_control(priv->priv_vbi,cmd,arg);
#endif
    }
    return TVI_CONTROL_UNKNOWN;
}
