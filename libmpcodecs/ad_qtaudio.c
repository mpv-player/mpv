#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "config.h"

#if defined(USE_QTX_CODECS) || defined(MACOSX)

#include "ad_internal.h"
#include "bswap.h"

#ifdef WIN32_LOADER
#include "ldt_keeper.h"
#endif

#ifdef MACOSX
#include <QuickTime/QuickTimeComponents.h>
#endif

static ad_info_t info =  {
	"QuickTime Audio Decoder",
	"qtaudio",
	"A'rpi",
	"Sascha Sommer",
	"uses win32 quicktime DLLs"
};

LIBAD_EXTERN(qtaudio)

#ifdef USE_QTX_CODECS
typedef struct OpaqueSoundConverter*    SoundConverter;
typedef unsigned long                   OSType;
typedef unsigned long                   UnsignedFixed;
typedef uint8_t                          Byte;
typedef struct SoundComponentData {
    long                            flags;
    OSType                          format;
    short                           numChannels;
    short                           sampleSize;
    UnsignedFixed                   sampleRate;
    long                            sampleCount;
    Byte *                          buffer;
    long                            reserved;
}SoundComponentData;

typedef int (__cdecl* LPFUNC1)(long flag);
typedef int (__cdecl* LPFUNC2)(const SoundComponentData *, const SoundComponentData *,SoundConverter *);
typedef int (__cdecl* LPFUNC3)(SoundConverter sc);
typedef int (__cdecl* LPFUNC4)(void);
typedef int (__cdecl* LPFUNC5)(SoundConverter sc, OSType selector,void * infoPtr);                          
typedef int (__cdecl* LPFUNC6)(SoundConverter sc, 
								unsigned long inputBytesTarget,
								unsigned long *inputFrames,
								unsigned long *inputBytes,
								unsigned long *outputBytes );
typedef int (__cdecl* LPFUNC7)(SoundConverter sc, 
								const void    *inputPtr, 
								unsigned long inputFrames,
								void          *outputPtr,
								unsigned long *outputFrames,
								unsigned long *outputBytes );
typedef int (__cdecl* LPFUNC8)(SoundConverter sc,
								void      *outputPtr,
                                unsigned long *outputFrames,
                                unsigned long *outputBytes);
typedef int (__cdecl* LPFUNC9)(SoundConverter         sc) ;                                

static HINSTANCE qtml_dll;
static LPFUNC1 InitializeQTML;
static LPFUNC2 SoundConverterOpen;
static LPFUNC3 SoundConverterClose;
static LPFUNC4 TerminateQTML;
static LPFUNC5 SoundConverterSetInfo;
static LPFUNC6 SoundConverterGetBufferSizes;
static LPFUNC7 SoundConverterConvertBuffer;
static LPFUNC8 SoundConverterEndConversion;
static LPFUNC9 SoundConverterBeginConversion;

#define siDecompressionParams 2002876005 // siDecompressionParams = FOUR_CHAR_CODE('wave')

HMODULE   WINAPI LoadLibraryA(LPCSTR);
FARPROC   WINAPI GetProcAddress(HMODULE,LPCSTR);
int       WINAPI FreeLibrary(HMODULE);

static int loader_init()
{

#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
#endif
    qtml_dll = LoadLibraryA("qtmlClient.dll");
    if( qtml_dll == NULL )
    {
        printf("failed loading dll\n" );
	return 1;
    }
#if 1
    InitializeQTML = (LPFUNC1)GetProcAddress(qtml_dll,"InitializeQTML");
	if ( InitializeQTML == NULL )
    {
        printf("failed geting proc address InitializeQTML\n");
		return 1;
    }
    SoundConverterOpen = (LPFUNC2)GetProcAddress(qtml_dll,"SoundConverterOpen");
	if ( SoundConverterOpen == NULL )
    {
        printf("failed getting proc address SoundConverterOpen\n");
		return 1;
    }
	SoundConverterClose = (LPFUNC3)GetProcAddress(qtml_dll,"SoundConverterClose");
	if ( SoundConverterClose == NULL )
    {
        printf("failed getting proc address SoundConverterClose\n");
		return 1;
    }
	TerminateQTML = (LPFUNC4)GetProcAddress(qtml_dll,"TerminateQTML");
	if ( TerminateQTML == NULL )
    {
        printf("failed getting proc address TerminateQTML\n");
		return 1;
    }
	SoundConverterSetInfo = (LPFUNC5)GetProcAddress(qtml_dll,"SoundConverterSetInfo");
	if ( SoundConverterSetInfo == NULL )
    {
        printf("failed getting proc address SoundConverterSetInfo\n");
		return 1;
    }
	SoundConverterGetBufferSizes = (LPFUNC6)GetProcAddress(qtml_dll,"SoundConverterGetBufferSizes");
	if ( SoundConverterGetBufferSizes == NULL )
    {
        printf("failed getting proc address SoundConverterGetBufferSizes\n");
		return 1;
    }
	SoundConverterConvertBuffer = (LPFUNC7)GetProcAddress(qtml_dll,"SoundConverterConvertBuffer");
	if ( SoundConverterConvertBuffer == NULL )
    {
        printf("failed getting proc address SoundConverterConvertBuffer1\n");
		return 1;
    }
	SoundConverterEndConversion = (LPFUNC8)GetProcAddress(qtml_dll,"SoundConverterEndConversion");
	if ( SoundConverterEndConversion == NULL )
    {
        printf("failed getting proc address SoundConverterEndConversion\n");
		return 1;
    }
	SoundConverterBeginConversion = (LPFUNC9)GetProcAddress(qtml_dll,"SoundConverterBeginConversion");
	if ( SoundConverterBeginConversion == NULL )
    {
        printf("failed getting proc address SoundConverterBeginConversion\n");
		return 1;
    }
	printf("Standard init done you may now call supported functions\n");
#endif
    printf("loader_init DONE???\n");
	return 0;
}
#endif /* USE_QTX_CODECS */

static SoundConverter			   myConverter = NULL;
static SoundComponentData		   InputFormatInfo,OutputFormatInfo;

static int InFrameSize;
static int OutFrameSize;

static int preinit(sh_audio_t *sh){
    int error;
    unsigned long FramesToGet=0; //how many frames the demuxer has to get
    unsigned long InputBufferSize=0; //size of the input buffer
    unsigned long OutputBufferSize=0; //size of the output buffer
    unsigned long WantedBufferSize=0; //the size you want your buffers to be

    printf("win32 libquicktime loader (c) Sascha Sommer\n");

#ifdef MACOSX
    EnterMovies();
#else
    if(loader_init()) return 0; // failed to load DLL
#endif
    
    printf("loader_init DONE!\n");

#if 1
    error = InitializeQTML(6+16);
    printf("InitializeQTML:%i\n",error);
    if(error) return 0;
    
	OutputFormatInfo.flags = InputFormatInfo.flags = 0;
	OutputFormatInfo.sampleCount = InputFormatInfo.sampleCount = 0;
	OutputFormatInfo.buffer = InputFormatInfo.buffer = NULL;
	OutputFormatInfo.reserved = InputFormatInfo.reserved = 0;
	OutputFormatInfo.numChannels = InputFormatInfo.numChannels = sh->wf->nChannels;
	InputFormatInfo.sampleSize = sh->wf->wBitsPerSample;
	OutputFormatInfo.sampleSize = 16;
	OutputFormatInfo.sampleRate = InputFormatInfo.sampleRate = sh->wf->nSamplesPerSec;
	InputFormatInfo.format =  bswap_32(sh->format); //1363430706;///*1768775988;//*/1902406962;//qdm2//1768775988;//FOUR_CHAR_CODE('ima4');
	OutputFormatInfo.format = 1313820229;// FOUR_CHAR_CODE('NONE');

    error = SoundConverterOpen(&InputFormatInfo, &OutputFormatInfo, &myConverter);
    printf("SoundConverterOpen:%i\n",error);
    if(error) return 0;

    if(sh->codecdata){
	error = SoundConverterSetInfo(myConverter,siDecompressionParams,sh->codecdata);
	printf("SoundConverterSetInfo:%i\n",error);
//	if(error) return 0;
    }

    WantedBufferSize=OutputFormatInfo.numChannels*OutputFormatInfo.sampleRate*2;
    error = SoundConverterGetBufferSizes(myConverter,
	WantedBufferSize,&FramesToGet,&InputBufferSize,&OutputBufferSize);
    printf("SoundConverterGetBufferSizes:%i\n",error);
    printf("WantedBufferSize = %li\n",WantedBufferSize);
    printf("InputBufferSize  = %li\n",InputBufferSize);
    printf("OutputBufferSize = %li\n",OutputBufferSize);
    printf("FramesToGet = %li\n",FramesToGet);
    
    InFrameSize=(InputBufferSize+FramesToGet-1)/FramesToGet;
    OutFrameSize=OutputBufferSize/FramesToGet;

    printf("FrameSize: %i -> %i\n",InFrameSize,OutFrameSize);

    error = SoundConverterBeginConversion(myConverter);
    printf("SoundConverterBeginConversion:%i\n",error);
    if(error) return 0;

    sh->audio_out_minsize=OutputBufferSize;
    sh->audio_in_minsize=InputBufferSize;
  
    sh->channels=sh->wf->nChannels;
    sh->samplerate=sh->wf->nSamplesPerSec;
    sh->samplesize=2; //(sh->wf->wBitsPerSample+7)/8;

    sh->i_bps=sh->wf->nAvgBytesPerSec;
//InputBufferSize*WantedBufferSize/OutputBufferSize;

#endif

   if(sh->format==0x3343414D){
       // MACE 3:1
       sh->ds->ss_div = 2*3; // 1 samples/packet
       sh->ds->ss_mul = sh->channels*2*1; // 1 bytes/packet
   } else
   if(sh->format==0x3643414D){
       // MACE 6:1
       sh->ds->ss_div = 2*6; // 1 samples/packet
       sh->ds->ss_mul = sh->channels*2*1; // 1 bytes/packet
   }

  return 1; // return values: 1=OK 0=ERROR
}

static int init(sh_audio_t *sh_audio){

    return 1; // return values: 1=OK 0=ERROR
}

static void uninit(sh_audio_t *sh){
    int error;
    unsigned long ConvertedFrames=0;
    unsigned long ConvertedBytes=0;
    error=SoundConverterEndConversion(myConverter,NULL,&ConvertedFrames,&ConvertedBytes);
    printf("SoundConverterEndConversion:%i\n",error);
    error = SoundConverterClose(myConverter);
    printf("SoundConverterClose:%i\n",error);
//    error = TerminateQTML();
//    printf("TerminateQTML:%i\n",error);
//    FreeLibrary( qtml_dll );
//    qtml_dll = NULL;
//    printf("qt dll loader uninit done\n");
#ifdef MACOSX
    ExitMovies();
#endif
}

static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen){
    int error;
    unsigned long FramesToGet=0; //how many frames the demuxer has to get
    unsigned long InputBufferSize=0; //size of the input buffer
    unsigned long ConvertedFrames=0;
    unsigned long ConvertedBytes=0;
    
    FramesToGet=minlen/OutFrameSize;
    if(FramesToGet*OutFrameSize<minlen &&
       (FramesToGet+1)*OutFrameSize<=maxlen) ++FramesToGet;
    if(FramesToGet*InFrameSize>sh->a_in_buffer_size)
	FramesToGet=sh->a_in_buffer_size/InFrameSize;

    InputBufferSize=FramesToGet*InFrameSize;

//    printf("FramesToGet = %li  (%li -> %li bytes)\n",FramesToGet,
//	InputBufferSize, FramesToGet*OutFrameSize);

    if(InputBufferSize>sh->a_in_buffer_len){
	int x=demux_read_data(sh->ds,&sh->a_in_buffer[sh->a_in_buffer_len],
	    InputBufferSize-sh->a_in_buffer_len);
	if(x>0) sh->a_in_buffer_len+=x;
	if(InputBufferSize>sh->a_in_buffer_len)
	    FramesToGet=sh->a_in_buffer_len/InFrameSize; // not enough data!
    }
    
//    printf("\nSoundConverterConvertBuffer(myConv=%p,inbuf=%p,frames=%d,outbuf=%p,&convframes=%p,&convbytes=%p)\n",
//	myConverter,sh->a_in_buffer,FramesToGet,buf,&ConvertedFrames,&ConvertedBytes);
    error = SoundConverterConvertBuffer(myConverter,sh->a_in_buffer,
	FramesToGet,buf,&ConvertedFrames,&ConvertedBytes);
//    printf("SoundConverterConvertBuffer:%i\n",error);
//    printf("ConvertedFrames = %li\n",ConvertedFrames);
//    printf("ConvertedBytes = %li\n",ConvertedBytes);
    
//    InputBufferSize=(ConvertedBytes/OutFrameSize)*InFrameSize; // FIXME!!
    InputBufferSize=FramesToGet*InFrameSize;
    sh->a_in_buffer_len-=InputBufferSize;
    if(sh->a_in_buffer_len<0) sh->a_in_buffer_len=0; // should not happen...
    else if(sh->a_in_buffer_len>0){
	memcpy(sh->a_in_buffer,&sh->a_in_buffer[InputBufferSize],sh->a_in_buffer_len);
    }

    return ConvertedBytes;
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...){
    // various optional functions you MAY implement:
  return CONTROL_UNKNOWN;
}

#endif
