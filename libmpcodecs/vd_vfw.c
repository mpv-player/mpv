#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef USE_WIN32DLL

#include "loader.h"
//#include "wine/mmreg.h"
#include "wine/vfw.h"
#include "wine/avifmt.h"

#include "vd_internal.h"

#include "libvo/img_format.h"

static vd_info_t info = {
	"Win32/VfW video codecs",
	"vfw",
	"A'rpi",
	"A'rpi & Alex",
	"win32 codecs"
};

LIBVD_EXTERN(vfw)

typedef struct {
    BITMAPINFOHEADER *o_bih;
    HIC handle;
} vd_vfw_ctx;

extern int divx_quality;

static int vfw_set_postproc(sh_video_t* sh, int quality)
{
    vd_vfw_ctx *priv = sh->context;
    // Works only with opendivx/divx4 based DLL
    return ICSendMessage(priv->handle, ICM_USER+80, (long)(&quality) ,NULL);
}

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd){
    case VDCTRL_QUERY_MAX_PP_LEVEL:
	return 9;
    case VDCTRL_SET_PP_LEVEL:
	vfw_set_postproc(sh,10*(*((int*)arg)));
	return CONTROL_OK;
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    HRESULT ret;
    int yuv=0;
    unsigned int outfmt=sh->codec->outfmt[sh->outfmtidx];
    int i, o_bih_len;
    vd_vfw_ctx *priv;
  
    priv = malloc(sizeof(vd_vfw_ctx));
    if (!priv)
	return 0;
    memset(priv, 0, sizeof(vd_vfw_ctx));
    sh->context = priv;

    mp_msg(MSGT_WIN32,MSGL_V,"======= Win32 (VFW) VIDEO Codec init =======\n");

    win32_codec_name = sh->codec->dll;
//    sh->hic = ICOpen( 0x63646976, sh->bih->biCompression, ICMODE_FASTDECOMPRESS);
    priv->handle = ICOpen( 0x63646976, sh->bih->biCompression, ICMODE_DECOMPRESS);
    if(!priv->handle){
	mp_msg(MSGT_WIN32,MSGL_ERR,"ICOpen failed! unknown codec / wrong parameters?\n");
	return 0;
    }

//    sh->bih->biBitCount=32;

    o_bih_len = ICDecompressGetFormatSize(priv->handle, sh->bih);
  
    priv->o_bih = malloc(o_bih_len);
    memset(priv->o_bih, 0, o_bih_len);
//    priv->o_bih->biSize = o_bih_len;

    printf("ICDecompressGetFormatSize ret: %d\n", o_bih_len);

    ret = ICDecompressGetFormat(priv->handle, sh->bih, priv->o_bih);
    if(ret < 0){
	mp_msg(MSGT_WIN32,MSGL_ERR,"ICDecompressGetFormat failed: Error %d\n", (int)ret);
	for (i=0; i < o_bih_len; i++) mp_msg(MSGT_WIN32, MSGL_DBG2, "%02x ", priv->o_bih[i]);
	return 0;
    }
    mp_msg(MSGT_WIN32,MSGL_V,"ICDecompressGetFormat OK\n");

// QPEG fsck !?!
//  priv->o_bih->biSize = o_bih_len;

//  printf("ICM_DECOMPRESS_QUERY=0x%X",ICM_DECOMPRESS_QUERY);

//  sh->o_bih.biWidth=sh_video->bih.biWidth;
//  sh->o_bih.biCompression = 0x32315659; //  mmioFOURCC('U','Y','V','Y');
//  ret=ICDecompressGetFormatSize(sh_video->hic,&sh_video->o_bih);
//  sh->o_bih.biCompression = 3; //0x32315659;
//  sh->o_bih.biCompression = mmioFOURCC('U','Y','V','Y');
//  sh->o_bih.biCompression = mmioFOURCC('U','Y','V','Y');
//  sh->o_bih.biCompression = mmioFOURCC('Y','U','Y','2');
//  sh->o_bih.biPlanes=3;
//  sh->o_bih.biBitCount=16;

#if 0
    // workaround for pegasus MJPEG:
    if(!sh_video->o_bih.biWidth) sh_video->o_bih.biWidth=sh_video->bih->biWidth;
    if(!sh_video->o_bih.biHeight) sh_video->o_bih.biHeight=sh_video->bih->biHeight;
    if(!sh_video->o_bih.biPlanes) sh_video->o_bih.biPlanes=sh_video->bih->biPlanes;
#endif

    switch (outfmt)
    {
    /* planar format */
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	priv->o_bih->biBitCount=12;
	yuv=1;
	break;
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
	priv->o_bih->biBitCount=9;
	yuv=1;
	break;
    /* packed format */
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
    case IMGFMT_YVYU:
	priv->o_bih->biBitCount=16;
	yuv=1;
	break;
    /* rgb/bgr format */
    case IMGFMT_RGB8:
    case IMGFMT_BGR8:
	priv->o_bih->biBitCount=8;
	break;
    case IMGFMT_RGB15:
    case IMGFMT_RGB16:
    case IMGFMT_BGR15:
    case IMGFMT_BGR16:
	priv->o_bih->biBitCount=16;
	break;
    case IMGFMT_RGB24:
    case IMGFMT_BGR24:
	priv->o_bih->biBitCount=24;
	break;
    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
	priv->o_bih->biBitCount=32;
	break;
    default:
	mp_msg(MSGT_WIN32,MSGL_ERR,"unsupported image format: 0x%x\n", outfmt);
	return 0;
    }

    priv->o_bih->biSizeImage = priv->o_bih->biWidth * priv->o_bih->biHeight * (priv->o_bih->biBitCount/8);
  
    if (!(sh->codec->outflags[sh->outfmtidx]&CODECS_FLAG_FLIP)) {
	priv->o_bih->biHeight=-sh->bih->biHeight; // flip image!
    }

    if (yuv && !(sh->codec->outflags[sh->outfmtidx] & CODECS_FLAG_YUVHACK))
	priv->o_bih->biCompression = outfmt;
    else
	 priv->o_bih->biCompression = 0;

    if(verbose)
    {
	printf("Input format:\n");
	printf("  biSize %ld\n", sh->bih->biSize);
	printf("  biWidth %ld\n", sh->bih->biWidth);
	printf("  biHeight %ld\n", sh->bih->biHeight);
	printf("  biPlanes %d\n", sh->bih->biPlanes);
	printf("  biBitCount %d\n", sh->bih->biBitCount);
	printf("  biCompression 0x%lx ('%.4s')\n", sh->bih->biCompression, (char *)&sh->bih->biCompression);
	printf("  biSizeImage %ld\n", sh->bih->biSizeImage);
	printf("Output format:\n");
	printf("  biSize %ld\n", priv->o_bih->biSize);
	printf("  biWidth %ld\n", priv->o_bih->biWidth);
	printf("  biHeight %ld\n", priv->o_bih->biHeight);
	printf("  biPlanes %d\n", priv->o_bih->biPlanes);
	printf("  biBitCount %d\n", priv->o_bih->biBitCount);
	printf("  biCompression 0x%lx ('%.4s')\n", priv->o_bih->biCompression, (char *)&priv->o_bih->biCompression);
	printf("  biSizeImage %ld\n", priv->o_bih->biSizeImage);
    }

    if (ICDecompressQuery(priv->handle, sh->bih, priv->o_bih))
    {
	mp_msg(MSGT_WIN32,MSGL_ERR,"ICDecompressQuery failed: Error %d\n", (int)ret);
//	return 0;
    } else
	mp_msg(MSGT_WIN32,MSGL_V,"ICDecompressQuery OK\n");

    if (ICDecompressBegin(priv->handle, sh->bih, priv->o_bih))
    {
	mp_msg(MSGT_WIN32,MSGL_ERR,"ICDecompressBegin failed: Error %d\n", (int)ret);
//	return 0;
    }

    if (yuv && sh->codec->outflags[sh->outfmtidx] & CODECS_FLAG_YUVHACK)
	priv->o_bih->biCompression = outfmt;

    ICSendMessage(priv->handle, ICM_USER+80, (long)(&divx_quality) ,NULL);

    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YUY2)) return 0;
    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32 video codec init OK!\n");
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    HRESULT ret;
    vd_vfw_ctx *priv = sh->context;
    
    ret = ICDecompressEnd(priv->handle);
    if (ret)
    {
	mp_msg(MSGT_WIN32, MSGL_WARN, "ICDecompressEnd failed: %d\n", ret);
	return(0);
    }

    ret = ICClose(priv->handle);
    if (ret)
    {
	mp_msg(MSGT_WIN32, MSGL_WARN, "ICClose failed: %d\n", ret);
	return(0);
    }
    
    free(priv->o_bih);
    free(priv);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    vd_vfw_ctx *priv = sh->context;
    mp_image_t* mpi;
    HRESULT ret;

    if(len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh, 
	(sh->codec->outflags[sh->outfmtidx] & CODECS_FLAG_STATIC) ?
	MP_IMGTYPE_STATIC : MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_WIDTH, 
	sh->disp_w, sh->disp_h);
    if(!mpi){	// temporary!
	printf("couldn't allocate image for cinepak codec\n");
	return NULL;
    }

    // set buffer:
    sh->our_out_buffer=mpi->planes[0];

    // set stride:  (trick discovered by Andreas Ackermann - thanx!)
    sh->bih->biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);
    priv->o_bih->biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);

    sh->bih->biSizeImage = len;
    
    ret = ICDecompress(priv->handle, 
	  ( (sh->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( ((flags&3)==2 && !(sh->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ),
	   sh->bih, data, priv->o_bih, (flags&3) ? 0 : mpi->planes[0]);

    if ((int)ret){
      mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error decompressing frame, err=%d\n",ret);
      return NULL;
    }
    
    // export palette:
    if(mpi->imgfmt==IMGFMT_RGB8 || mpi->imgfmt==IMGFMT_BGR8){
	if(priv->o_bih->biSize>40)
	{
	    mpi->planes[1]=((unsigned char*)priv->o_bih)+40;
	    mp_dbg(MSGT_DECVIDEO, MSGL_DBG2, "Found and copied palette\n");
	}
	else
	    mpi->planes[1]=NULL;
    }
    
    return mpi;
}
#endif
