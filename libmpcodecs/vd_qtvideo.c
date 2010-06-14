/*
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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "config.h"
#include "mp_msg.h"
#include "mpbswap.h"
#include "vd_internal.h"

#ifdef CONFIG_QUICKTIME
#include <QuickTime/ImageCodec.h>
#define dump_ImageDescription(x)
#else
#include "loader/ldt_keeper.h"
#include "loader/qtx/qtxsdk/components.h"
#include "loader/wine/winbase.h"
#include "loader/wine/windef.h"
#endif

static const vd_info_t info = {
	"Quicktime Video decoder",
	"qtvideo",
	"A'rpi",
	"Sascha Sommer",
	"win32"
};

LIBVD_EXTERN(qtvideo)

static mp_image_t* mpi;
static Rect OutBufferRect;              //the dimensions of our GWorld

static GWorldPtr OutBufferGWorld = NULL;//a GWorld is some kind of description for a drawing environment
static ImageDescriptionHandle framedescHandle;
static ImageSequence imageSeq;

#ifndef CONFIG_QUICKTIME
static    HINSTANCE qtime_qts; // handle to the preloaded quicktime.qts
static    HMODULE handler;
static    OSErr (*InitializeQTML)(long flags);
static    OSErr (*EnterMovies)(void);
static    void (*ExitMovies)(void);
static    OSErr (*DecompressSequenceBegin)(ImageSequence *seqID,
                                           ImageDescriptionHandle desc,
                                           CGrafPtr port,
                                           /*GDHandle*/void* gdh,
                                           const Rect *srcRect,
                                           MatrixRecordPtr matrix,
                                           short mode,
                                           RgnHandle mask,
                                           CodecFlags flags,
                                           CodecQ accuracy,
                                           DecompressorComponent codec);
static   OSErr (*DecompressSequenceFrameS)(ImageSequence seqID,
                                           Ptr data,
                                           long dataSize,
                                           CodecFlags inFlags,
                                           CodecFlags *outFlags,
                                           ICMCompletionProcRecordPtr asyncCompletionProc);
static    PixMapHandle    (*GetGWorldPixMap)(GWorldPtr offscreenGWorld);
static    OSErr           (*QTNewGWorldFromPtr)(GWorldPtr *gw,
			       OSType pixelFormat,
			       const Rect *boundsRect,
			       CTabHandle cTable,
                               /*GDHandle*/void* aGDevice, //unused anyway
                               GWorldFlags flags,
                               void *baseAddr,
                               long rowBytes);
static    Handle          (*NewHandleClear)(Size byteCount);
static    void            (*DisposeHandle)(Handle h);
static    void            (*DisposeGWorld)(GWorldPtr offscreenGWorld);
static    OSErr           (*CDSequenceEnd)(ImageSequence seqID);
#endif /* #ifndef CONFIG_QUICKTIME */

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    OSErr result = 1;

    if (sh->ImageDesc == NULL) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,"sh->ImageDesc not set, cannot use binary QuickTime codecs (try -demuxer mov?)\n");
        return 0;
    }

#ifndef CONFIG_QUICKTIME
#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
#endif

    //preload quicktime.qts to avoid the problems caused by the hardcoded path inside the dll
    qtime_qts = LoadLibraryA("QuickTime.qts");
    if(!qtime_qts){
    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"unable to load QuickTime.qts\n" );
    return 0;
    }

    handler = LoadLibraryA("qtmlClient.dll");
    if(!handler){
    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"unable to load qtmlClient.dll\n");
    return 0;
    }

    InitializeQTML = (OSErr (*)(long))GetProcAddress(handler, "InitializeQTML");
    EnterMovies = (OSErr (*)(void))GetProcAddress(handler, "EnterMovies");
    ExitMovies = (void (*)(void))GetProcAddress(handler, "ExitMovies");
    DecompressSequenceBegin = (OSErr (*)(ImageSequence*,ImageDescriptionHandle,CGrafPtr,void *,const Rect *,MatrixRecordPtr,short,RgnHandle,CodecFlags,CodecQ,DecompressorComponent))GetProcAddress(handler, "DecompressSequenceBegin");
    DecompressSequenceFrameS = (OSErr (*)(ImageSequence,Ptr,long,CodecFlags,CodecFlags*,ICMCompletionProcRecordPtr))GetProcAddress(handler, "DecompressSequenceFrameS");
    GetGWorldPixMap = (PixMapHandle (*)(GWorldPtr))GetProcAddress(handler, "GetGWorldPixMap");
    QTNewGWorldFromPtr = (OSErr(*)(GWorldPtr *,OSType,const Rect *,CTabHandle,void*,GWorldFlags,void *,long))GetProcAddress(handler, "QTNewGWorldFromPtr");
    NewHandleClear = (OSErr(*)(Size))GetProcAddress(handler, "NewHandleClear");
    DisposeHandle = (void (*)(Handle))GetProcAddress(handler, "DisposeHandle");
    DisposeGWorld = (void (*)(GWorldPtr))GetProcAddress(handler, "DisposeGWorld");
    CDSequenceEnd = (OSErr (*)(ImageSequence))GetProcAddress(handler, "CDSequenceEnd");

    if(!InitializeQTML || !EnterMovies || !DecompressSequenceBegin || !DecompressSequenceFrameS){
	mp_msg(MSGT_DECVIDEO,MSGL_ERR,"invalid qtmlClient.dll!\n");
	return 0;
    }

    result=InitializeQTML(kInitializeQTMLDisableDirectSound |
                          kInitializeQTMLUseGDIFlag |
                          kInitializeQTMLDisableDDClippers);
    mp_msg(MSGT_DECVIDEO,MSGL_DBG2,"InitializeQTML returned %d\n",result);
#endif /* CONFIG_QUICKTIME */

    result=EnterMovies();
    mp_msg(MSGT_DECVIDEO,MSGL_DBG2,"EnterMovies returned %d\n",result);

    //make a yuy2 gworld
    OutBufferRect.top=0;
    OutBufferRect.left=0;
    OutBufferRect.right=sh->disp_w;
    OutBufferRect.bottom=sh->disp_h;

    //Fill the imagedescription for our SVQ3 frame
    //we can probably get this from Demuxer
    if(!sh->ImageDesc) sh->ImageDesc=(sh->bih+1); // hack for SVQ3-in-AVI
    mp_msg(MSGT_DECVIDEO,MSGL_DBG2,"ImageDescription size: %d\n",((ImageDescription*)(sh->ImageDesc))->idSize);
    framedescHandle=(ImageDescriptionHandle)NewHandleClear(((ImageDescription*)(sh->ImageDesc))->idSize);
    memcpy(*framedescHandle,sh->ImageDesc,((ImageDescription*)(sh->ImageDesc))->idSize);
    dump_ImageDescription(*framedescHandle);

    (**framedescHandle).cType = bswap_32(sh->format);
    sh->context = (void *)kYUVSPixelFormat;
    {
	int imgfmt = sh->codec->outfmt[sh->outfmtidx];
	int qt_imgfmt;
    switch(imgfmt)
    {
	case IMGFMT_YUY2:
	    qt_imgfmt = kYUVSPixelFormat;
	    break;
	case IMGFMT_YVU9:
	    qt_imgfmt = 0x73797639; //kYVU9PixelFormat;
	    break;
	case IMGFMT_YV12:
	    qt_imgfmt = 0x79343230;
	    break;
	case IMGFMT_UYVY:
	    qt_imgfmt = k2vuyPixelFormat;
	    break;
	case IMGFMT_YVYU:
	    qt_imgfmt = kYVYU422PixelFormat;
	    imgfmt = IMGFMT_YUY2;
	    break;
	case IMGFMT_RGB16:
	    qt_imgfmt = k16LE555PixelFormat;
	    break;
	case IMGFMT_BGR24:
	    qt_imgfmt = k24BGRPixelFormat;
	    break;
	case IMGFMT_BGR32:
	    qt_imgfmt = k32BGRAPixelFormat;
	    break;
	case IMGFMT_RGB32:
	    qt_imgfmt = k32RGBAPixelFormat;
	    break;
	default:
	    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"Unknown requested csp\n");
	    return 0;
    }
    mp_msg(MSGT_DECVIDEO,MSGL_DBG2,"imgfmt: %s qt_imgfmt: %.4s\n", vo_format_name(imgfmt), (char *)&qt_imgfmt);
    sh->context = (void *)qt_imgfmt;
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,imgfmt)) return 0;
    }

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE,
	sh->disp_w, sh->disp_h);
    if(!mpi) return 0;

    result = QTNewGWorldFromPtr(
        &OutBufferGWorld,
	(OSType)sh->context,
        &OutBufferRect,   //we should benchmark if yvu9 is faster for svq3, too
        0,
        0,
        0,
        mpi->planes[0],
        mpi->stride[0]);
    if (result) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,"QTNewGWorldFromPtr result=%d\n",result);
        return 0;
    }

    result = DecompressSequenceBegin(&imageSeq, framedescHandle, (CGrafPtr)OutBufferGWorld,
                                     NULL, NULL, NULL, srcCopy,  NULL, 0,
                                     codecNormalQuality, 0);
    if(result) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,"DecompressSequenceBegin result=%d\n",result);
        return 0;
    }

    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    if(OutBufferGWorld) {
        DisposeGWorld(OutBufferGWorld);
        OutBufferGWorld = NULL;
    }
    if(framedescHandle) {
        DisposeHandle((Handle)framedescHandle);
        framedescHandle = NULL;
    }
    if(imageSeq) {
        CDSequenceEnd(imageSeq);
        imageSeq = 0;
    }
    ExitMovies();
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    OSErr result = 1;
    CodecFlags ignore;

    if(len<=0) return NULL; // skipped frame

#ifdef WIN32_LOADER
    Setup_FS_Segment();
#endif

    result = DecompressSequenceFrameS(imageSeq, data, len, 0, &ignore, NULL);
    if(result) {
        mp_msg(MSGT_DECVIDEO,MSGL_ERR,"DecompressSequenceFrameS result=0x%d\n",result);
        return NULL;
    }

if((int)sh->context==0x73797639){	// Sorenson 16-bit YUV -> std YVU9
    int i;

    PixMap dstPixMap = **GetGWorldPixMap(OutBufferGWorld);
    short *src0=(short *)((char*)dstPixMap.baseAddr+0x20);

    for(i=0;i<mpi->h;i++){
	int x;
	unsigned char* dst=mpi->planes[0]+i*mpi->stride[0];
	unsigned short* src=src0+i*((mpi->w+15)&(~15));
	for(x=0;x<mpi->w;x++) dst[x]=src[x];
    }
    src0+=((mpi->w+15)&(~15))*((mpi->h+15)&(~15));
    for(i=0;i<mpi->h/4;i++){
	int x;
	unsigned char* dst=mpi->planes[1]+i*mpi->stride[1];
	unsigned short* src=src0+i*(((mpi->w+63)&(~63))/4);
	for(x=0;x<mpi->w/4;x++) dst[x]=src[x];
	src+=((mpi->w+63)&(~63))/4;
    }
    src0+=(((mpi->w+63)&(~63))/4)*(((mpi->h+63)&(~63))/4);
    for(i=0;i<mpi->h/4;i++){
	int x;
	unsigned char* dst=mpi->planes[2]+i*mpi->stride[2];
	unsigned short* src=src0+i*(((mpi->w+63)&(~63))/4);
	for(x=0;x<mpi->w/4;x++) dst[x]=src[x];
	src+=((mpi->w+63)&(~63))/4;
    }

}


    return mpi;
}
