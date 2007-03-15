/*qt video encoder using win32 libs
  released under gnu gpl
  (C)Sascha Sommer                 */

#define MAX_IDSIZE 0x6F

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "libavutil/common.h"
#include "mpbswap.h"

#ifdef WIN32_LOADER 
#include "loader/ldt_keeper.h"
#endif 

#include "loader/qtx/qtxsdk/components.h"
#include "loader/wine/windef.h"

#include "codec-cfg.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "stream/stream.h"
#include "libmpdemux/muxer.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

HMODULE   WINAPI LoadLibraryA(LPCSTR);
FARPROC   WINAPI GetProcAddress(HMODULE,LPCSTR);
int       WINAPI FreeLibrary(HMODULE);
static HINSTANCE qtime_qts; //handle to preloaded quicktime.qts
static HMODULE handler;

static OSErr        (*FindCodec)(CodecType              cType,
                        CodecComponent         specCodec,
                        CompressorComponent *  compressor,
                        DecompressorComponent * decompressor);
static OSErr        (*InitializeQTML)(long flags);
static PixMapHandle (*GetGWorldPixMap)(GWorldPtr offscreenGWorld);
static OSErr        (*QTNewGWorldFromPtr)(GWorldPtr *gw,
			            OSType pixelFormat,
			            const Rect *boundsRect,
			            CTabHandle cTable,
                        /*GDHandle*/void* aGDevice, /*unused anyway*/
                        GWorldFlags flags,
                        void *baseAddr,
                        long rowBytes);
static OSErr        (*NewHandleClear)(Size byteCount);
static OSErr        (*CompressSequenceBegin) (
     ImageSequence             *seqID,
     PixMapHandle              src,
     PixMapHandle              prev,
     const Rect                *srcRect,
     const Rect                *prevRect,
     short                     colorDepth,
     CodecType                 cType,
     CompressorComponent       codec,
     CodecQ                    spatialQuality,
     CodecQ                    temporalQuality,
     long                      keyFrameRate,
     CTabHandle                ctable,
     CodecFlags                flags,
     ImageDescriptionHandle    desc );

static OSErr (*CompressSequenceFrame) (
     ImageSequence                 seqID,
     PixMapHandle                  src,
     const Rect                    *srcRect,
     CodecFlags                    flags,
     Ptr                           data,
     long                          *dataSize,
     UInt8                         *similarity,
     ICMCompletionProcRecordPtr    asyncCompletionProc );

static    OSErr (*GetMaxCompressionSize)(PixMapHandle src,
     const Rect *srcRect,
     short colorDepth,
     CodecQ quality,
     CodecType cType,
     CompressorComponent codec,
     long *size );
static    OSErr (*CDSequenceEnd)( ImageSequence seqID );
static    Component (*FindNextComponent)(Component prev,ComponentDescription* desc);
static    long (*CountComponents)(ComponentDescription* desc);
static    OSErr (*GetComponentInfo)(Component prev,ComponentDescription* desc,Handle h1,Handle h2,Handle h3);



//static int format=mmioFOURCC('S','V','Q','1');
static int format=mmioFOURCC('S','V','Q','3');



//static void *frame_in;    //input frame
static void *frame_prev;  //previous frame
static void *frame_comp;  //compressed frame
static GWorldPtr frame_GWorld_in = NULL;//a GWorld is some kind of description for a drawing environment
static GWorldPtr frame_GWorld_prev = NULL;
static Rect FrameRect;

static CompressorComponent compressor;
static DecompressorComponent decompressor;
static ImageDescriptionHandle desc;
static ImageSequence seq;





struct vf_priv_s {
    muxer_stream_t* mux;
    //dv_encoder_t* enc;

};
#define mux_v (vf->priv->mux)

//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    OSErr cres;
    ComponentDescription cdesc;
    mux_v->bih->biWidth=width;
    mux_v->bih->biHeight=height;
    mux_v->bih->biSizeImage=width*height*2;
    mux_v->aspect = (float)d_width/d_height;



    memset(&cdesc,0,sizeof(cdesc));
    cdesc.componentType= (((unsigned char)'i')<<24)|
			(((unsigned char)'m')<<16)|
			(((unsigned char)'c')<<8)|
			(((unsigned char)'o'));

    cdesc.componentSubType=bswap_32(format);
    cdesc.componentManufacturer=0;
    cdesc.componentFlags=0;
    cdesc.componentFlagsMask=0;


    mp_msg(MSGT_MENCODER,MSGL_DBG2,"Count = %ld\n",CountComponents(&cdesc));
    compressor=FindNextComponent(NULL,&cdesc);
    if(!compressor){
	mp_msg(MSGT_MENCODER,MSGL_ERR,"Cannot find requested component\n");
	return(0);
    }
    mp_msg(MSGT_MENCODER,MSGL_DBG2,"Found it! ID = %p\n",compressor);

//	cres= FindCodec (fourcc,anyCodec,&compressor,&decompressor );
//	printf("FindCodec returned:%i compressor: 0x%X decompressor: 0x%X\n",cres&0xFFFF,compressor,decompressor);

    return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data){

    return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    if(fmt==IMGFMT_YUY2) return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
    return 0;
}

static int codec_inited = 0;

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){

    OSErr cres;
    long framesizemax;
    UInt8 similarity=0;
    long compressedsize;
    OSType in_format=kYUVSPixelFormat;
    int width = mpi->width;
    int height = mpi->height;
    int stride = width*2;
if(!codec_inited){
    FrameRect.top=0;
    FrameRect.left=0;
    FrameRect.right=width;
    FrameRect.bottom=height;
    cres = QTNewGWorldFromPtr(
        &frame_GWorld_in,
        in_format,
        &FrameRect,
        0,
        0,
        0,
        mpi->planes[0],
        stride);
    mp_msg(MSGT_MENCODER,MSGL_DBG2,"NewGWorldFromPtr returned:%i\n",cres&0xFFFF);
    //dunno what todo about this
    frame_prev = malloc(stride * height);
    cres = QTNewGWorldFromPtr(
        &frame_GWorld_prev,
        in_format,
        &FrameRect,
        0,
        0,
        0,
        frame_prev,
        stride);
    mp_msg(MSGT_MENCODER,MSGL_DBG2,"height:%i width:%i stride:%i\n",height,width,stride);
    mp_msg(MSGT_MENCODER,MSGL_DBG2,"NewGWorldFromPtr returned:%i\n",cres&0xFFFF);
    cres=  GetMaxCompressionSize (
       GetGWorldPixMap(frame_GWorld_in),
       &FrameRect,
       24,
       codecNormalQuality,
       bswap_32(format),
       compressor,
       &framesizemax );
    mp_msg(MSGT_MENCODER,MSGL_DBG2,"GetMaxCompressionSize returned:%i : MaxSize:%li\n",cres&0xFFFF,framesizemax);
    frame_comp=malloc(framesizemax);

    desc = (ImageDescriptionHandle)NewHandleClear(MAX_IDSIZE); //memory where the desc will be stored
    (*desc)->idSize=MAX_IDSIZE;

    cres= CompressSequenceBegin (
       &seq,
       GetGWorldPixMap( frame_GWorld_in),
       GetGWorldPixMap( frame_GWorld_prev),
       &FrameRect,
       &FrameRect,
       24, // color depth
       bswap_32(format), // fourcc
       compressor,  // codec component
       codecNormalQuality, //codecNormalQuality,
       codecMaxQuality, //codecNormalQuality,
       10*30, // keyframe rate
       0,
       0,
       desc);
    mp_msg(MSGT_MENCODER,MSGL_DBG2,"CompressSequenceBegin returned:%i\n",cres&0xFFFF);
    mp_msg(MSGT_MENCODER,MSGL_DBG2,"Sequence ID:%i\n",seq);

    dump_ImageDescription(*desc);
    codec_inited++;
}
    cres = CompressSequenceFrame (
    	seq,
        GetGWorldPixMap(frame_GWorld_in),
        &FrameRect,
        0,
        (char*)mux_v->buffer,
        &compressedsize,
        &similarity,
        0);

    if(cres&0xFFFF)mp_msg(MSGT_MENCODER,MSGL_DBG2,"CompressSequenceFrame returned:%i\n",cres&0xFFFF);
#if 0
    printf("Size %i->%i   \n",stride*height,compressedsize);
    printf("Ratio: %i:1\n",(stride*height)/compressedsize);
#endif
    muxer_write_chunk(mux_v, compressedsize , similarity?0:0x10, MP_NOPTS_VALUE, MP_NOPTS_VALUE);

    if(((*desc)->idSize)>MAX_IDSIZE){
	mp_msg(MSGT_MENCODER,MSGL_ERR,"FATAL! idSize=%d too big, increase MAX_IDSIZE in ve_qtvideo.c!\n",((*desc)->idSize));
    } else {
	// according to QT docs, imagedescription may be changed while encoding
	// a frame (even its size may (and does!) change!)
	memcpy(mux_v->bih+1,*desc,(*desc)->idSize);
    }

    return 1;
}

//===========================================================================//

static int vf_open(vf_instance_t *vf, char* args){
    OSErr cres = 1;
    vf->config=config;
    vf->default_caps=VFCAP_CONSTANT;
    vf->control=control;
    vf->query_format=query_format;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    memset(vf->priv,0,sizeof(struct vf_priv_s));
    vf->priv->mux=(muxer_stream_t*)args;

    mux_v->bih=calloc(1, sizeof(BITMAPINFOHEADER)+MAX_IDSIZE);
    mux_v->bih->biSize=sizeof(BITMAPINFOHEADER)+MAX_IDSIZE;
    mux_v->bih->biWidth=0;
    mux_v->bih->biHeight=0;
    mux_v->bih->biCompression=format;
    mux_v->bih->biPlanes=1;
    mux_v->bih->biBitCount=24;


#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
#endif
    //preload quicktime.qts to avoid the problems caused by the hardcoded path inside the dll
    qtime_qts = LoadLibraryA("QuickTime.qts");
    if(!qtime_qts){
        mp_msg(MSGT_MENCODER,MSGL_ERR,"unable to load QuickTime.qts\n" );
        return 0;
    }
    
    handler = LoadLibraryA("qtmlClient.dll");
    if(!handler){
        mp_msg(MSGT_MENCODER,MSGL_ERR,"unable to load qtmlClient.dll\n");
        return 0;
    }
    InitializeQTML = (OSErr (*)(long))GetProcAddress(handler, "InitializeQTML");
    GetGWorldPixMap = (PixMapHandle (*)(GWorldPtr))GetProcAddress(handler, "GetGWorldPixMap");
    QTNewGWorldFromPtr = (OSErr(*)(GWorldPtr *,OSType,const Rect *,CTabHandle,void*,GWorldFlags,void *,long))GetProcAddress(handler, "QTNewGWorldFromPtr");
    NewHandleClear = (OSErr(*)(Size))GetProcAddress(handler, "NewHandleClear");
    FindCodec = (OSErr (*)(CodecType,CodecComponent,CompressorComponent *,DecompressorComponent *))GetProcAddress(handler,"FindCodec");
    CompressSequenceBegin = (OSErr(*)(ImageSequence *,PixMapHandle,PixMapHandle,const Rect *,const Rect *,short,CodecType,CompressorComponent,CodecQ,CodecQ,long,CTabHandle,CodecFlags,ImageDescriptionHandle))GetProcAddress(handler,"CompressSequenceBegin");
    CompressSequenceFrame = (OSErr(*)(ImageSequence,PixMapHandle,const Rect *,CodecFlags,Ptr,long *,UInt8 *,ICMCompletionProcRecordPtr))GetProcAddress(handler,"CompressSequenceFrame");
    GetMaxCompressionSize = (OSErr(*)(PixMapHandle,const Rect *,short,CodecQ,CodecType,CompressorComponent,long *))GetProcAddress(handler,"GetMaxCompressionSize");
    CDSequenceEnd = (OSErr (*)(ImageSequence))GetProcAddress(handler,"CDSequenceEnd");
    FindNextComponent = (Component (*)(Component,ComponentDescription*))GetProcAddress(handler, "FindNextComponent");
    CountComponents = (long (*)(ComponentDescription*))GetProcAddress(handler, "CountComponents");
    GetComponentInfo = (OSErr (*)(Component,ComponentDescription*,Handle,Handle,Handle))GetProcAddress(handler, "GetComponentInfo");
    if(!InitializeQTML  ||!CompressSequenceBegin){
        mp_msg(MSGT_MENCODER,MSGL_ERR,"invalid qt DLL!\n");
        return 0;
    }
    //printf("%i,%i,%i\n",mmioFOURCC('S','V','Q','1'),'SVQ1',bswap_32(mmioFOURCC('S','V','Q','1')));
    cres=InitializeQTML(6+16);
    mp_msg(MSGT_MENCODER,MSGL_DBG2,"InitializeQTML returned %i\n",cres);
    return 1;
}

vf_info_t ve_info_qtvideo = {
    "Quicktime video encoder using win32 DLLs",
    "qtvideo",
    "Sascha Sommer",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
