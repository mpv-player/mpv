#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "vd_internal.h"

#include "loader/wine/driver.h"
#include "loader/wine/vfw.h"

static vd_info_t info = {
#ifdef BUILD_VFWEX
	"Win32/VfWex video codecs",
	"vfwex",
#else
	"Win32/VfW video codecs",
	"vfw",
#endif
	"A'rpi & Alex",
	"avifile.sf.net",
	"win32 codecs"
};

#ifdef BUILD_VFWEX
LIBVD_EXTERN(vfwex)
#else
LIBVD_EXTERN(vfw)
#endif

typedef struct {
    BITMAPINFOHEADER *o_bih;
    HIC handle;
    unsigned char *palette;
} vd_vfw_ctx;

extern int divx_quality;

static int vfw_set_postproc(sh_video_t* sh, int quality)
{
    vd_vfw_ctx *priv = sh->context;
    // Works only with opendivx/divx4 based DLL
    return ICSendMessage(priv->handle, ICM_USER+80, (long)(&quality), 0);
}

static void set_csp(BITMAPINFOHEADER *o_bih,unsigned int outfmt){
    int yuv = 0;

	switch (outfmt)
	{
	/* planar format */
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	    o_bih->biBitCount=12;
	    yuv=1;
	    break;
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	    o_bih->biBitCount=9;
	    yuv=1;
	    break;
	/* packed format */
	case IMGFMT_YUY2:
        case IMGFMT_UYVY:
        case IMGFMT_YVYU:
    	    o_bih->biBitCount=16;
	    yuv=1;
	    break;
	/* rgb/bgr format */
	case IMGFMT_RGB8:
	case IMGFMT_BGR8:
	    o_bih->biBitCount=8;
	    break;
	case IMGFMT_RGB15:
	case IMGFMT_RGB16:
	case IMGFMT_BGR15:
	case IMGFMT_BGR16:
	    o_bih->biBitCount=16;
	    break;
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	    o_bih->biBitCount=24;
	    break;
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
	    o_bih->biBitCount=32;
	    break;
	default:
	    mp_msg(MSGT_WIN32,MSGL_ERR,"Unsupported image format: %s\n", vo_format_name(outfmt));
	    return;
	}

	o_bih->biSizeImage = abs(o_bih->biWidth * o_bih->biHeight * (o_bih->biBitCount/8));

// Note: we cannot rely on sh->outfmtidx here, it's undefined at this stage!!!
//	if (yuv && !(sh->codec->outflags[sh->outfmtidx] & CODECS_FLAG_YUVHACK))
	if (yuv)
	    o_bih->biCompression = outfmt;
	else
	    o_bih->biCompression = 0;
}

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    vd_vfw_ctx *priv = sh->context;
    switch(cmd){
    case VDCTRL_QUERY_MAX_PP_LEVEL:
	return 9;
    case VDCTRL_SET_PP_LEVEL:
	vfw_set_postproc(sh,10*(*((int*)arg)));
	return CONTROL_OK;
#if 1
    // FIXME: make this optional...
    case VDCTRL_QUERY_FORMAT:
      {
	HRESULT ret;
	if(!(sh->codec->outflags[sh->outfmtidx]&CODECS_FLAG_QUERY))
	    return CONTROL_UNKNOWN;	// do not query!
	set_csp(priv->o_bih,*((int*)arg));
#ifdef BUILD_VFWEX
	ret = ICDecompressQueryEx(priv->handle, sh->bih, priv->o_bih);
#else
	ret = ICDecompressQuery(priv->handle, sh->bih, priv->o_bih);
#endif
	if (ret)
	{
	    mp_msg(MSGT_WIN32, MSGL_DBG2, "ICDecompressQuery failed:: Error %d\n", (int)ret);
	    return CONTROL_FALSE;
	}
	return CONTROL_TRUE;
      }
#endif
    }
    return CONTROL_UNKNOWN;
}

extern void print_video_header(BITMAPINFOHEADER *h, int verbose_level);

// init driver
static int init(sh_video_t *sh){
    HRESULT ret;
//    unsigned int outfmt=sh->codec->outfmt[sh->outfmtidx];
    int i, o_bih_len;
    vd_vfw_ctx *priv;
  
    /* Hack for VSSH codec: new dll can't decode old files
     * In my samples old files have no extradata, so use that info
     * to decide what dll should be used (here and in vd_dshow).
     */
    if (!strcmp(sh->codec->dll, "vssh264.dll") && (sh->bih->biSize > 40))
      return 0;

    priv = malloc(sizeof(vd_vfw_ctx));
    if (!priv)
	return 0;
    memset(priv, 0, sizeof(vd_vfw_ctx));
    sh->context = priv;

    mp_msg(MSGT_WIN32,MSGL_V,"======= Win32 (VFW) VIDEO Codec init =======\n");


//    win32_codec_name = sh->codec->dll;
//    sh->hic = ICOpen( 0x63646976, sh->bih->biCompression, ICMODE_FASTDECOMPRESS);
//    priv->handle = ICOpen( 0x63646976, sh->bih->biCompression, ICMODE_DECOMPRESS);
    priv->handle = ICOpen( (long)(sh->codec->dll), sh->bih->biCompression, ICMODE_DECOMPRESS);
    if(!priv->handle){
	mp_msg(MSGT_WIN32,MSGL_ERR,"ICOpen failed! unknown codec / wrong parameters?\n");
	return 0;
    }

//    sh->bih->biBitCount=32;

    o_bih_len = ICDecompressGetFormatSize(priv->handle, sh->bih);
  
    priv->o_bih = malloc(o_bih_len);
    memset(priv->o_bih, 0, o_bih_len);

    mp_msg(MSGT_WIN32,MSGL_V,"ICDecompressGetFormatSize ret: %d\n", o_bih_len);

    ret = ICDecompressGetFormat(priv->handle, sh->bih, priv->o_bih);
    if(ret < 0){
	mp_msg(MSGT_WIN32,MSGL_ERR,"ICDecompressGetFormat failed: Error %d\n", (int)ret);
	for (i=0; i < o_bih_len; i++) mp_msg(MSGT_WIN32, MSGL_DBG2, "%02x ", priv->o_bih[i]);
	return 0;
    }
    mp_msg(MSGT_WIN32,MSGL_V,"ICDecompressGetFormat OK\n");

#if 0
    // workaround for pegasus MJPEG:
    if(!sh_video->o_bih.biWidth) sh_video->o_bih.biWidth=sh_video->bih->biWidth;
    if(!sh_video->o_bih.biHeight) sh_video->o_bih.biHeight=sh_video->bih->biHeight;
    if(!sh_video->o_bih.biPlanes) sh_video->o_bih.biPlanes=sh_video->bih->biPlanes;
#endif

    // ok let libvo and vd core to handshake and decide the optimal csp:
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YUY2)) return 0;

    if (!(sh->codec->outflags[sh->outfmtidx]&CODECS_FLAG_FLIP)) {
	priv->o_bih->biHeight=-sh->bih->biHeight; // flip image!
    }

    // ok, let's set the choosen colorspace:
    set_csp(priv->o_bih,sh->codec->outfmt[sh->outfmtidx]);

    // fake it to RGB for broken DLLs (divx3)
    if(sh->codec->outflags[sh->outfmtidx] & CODECS_FLAG_YUVHACK)
	priv->o_bih->biCompression = 0;

    // sanity check:
#if 1
#ifdef BUILD_VFWEX
    ret = ICDecompressQueryEx(priv->handle, sh->bih, priv->o_bih);
#else
    ret = ICDecompressQuery(priv->handle, sh->bih, priv->o_bih);
#endif
    if (ret)
    {
	mp_msg(MSGT_WIN32,MSGL_WARN,"ICDecompressQuery failed: Error %d\n", (int)ret);
//	return 0;
    } else
	mp_msg(MSGT_WIN32,MSGL_V,"ICDecompressQuery OK\n");
#endif

#ifdef BUILD_VFWEX
    ret = ICDecompressBeginEx(priv->handle, sh->bih, priv->o_bih);
#else
    ret = ICDecompressBegin(priv->handle, sh->bih, priv->o_bih);
#endif
    if (ret)
    {
	mp_msg(MSGT_WIN32,MSGL_WARN,"ICDecompressBegin failed: Error %d\n", (int)ret);
//	return 0;
    }

    // for broken codecs set it again:
    if(sh->codec->outflags[sh->outfmtidx] & CODECS_FLAG_YUVHACK)
	set_csp(priv->o_bih,sh->codec->outfmt[sh->outfmtidx]);

    mp_msg(MSGT_WIN32, MSGL_V, "Input format:\n");
    if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_video_header(sh->bih,MSGL_V);
    mp_msg(MSGT_WIN32, MSGL_V, "Output format:\n");
    if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_video_header(priv->o_bih,MSGL_V);

    // set postprocessing level in xvid/divx4 .dll
    ICSendMessage(priv->handle, ICM_USER+80, (long)(&divx_quality), 0);

    // don't do this palette mess always, it makes div3 dll crashing...
    if(sh->codec->outfmt[sh->outfmtidx]==IMGFMT_BGR8){
	if(ICDecompressGetPalette(priv->handle, sh->bih, priv->o_bih)){
	    priv->palette = (unsigned char*)(priv->o_bih+1);
	    mp_msg(MSGT_WIN32,MSGL_V,"ICDecompressGetPalette OK\n");
	} else {
	    if(sh->bih->biSize>=40+4*4)
		priv->palette = (unsigned char*)(sh->bih+1);
	}
    }

    mp_msg(MSGT_DECVIDEO,MSGL_V,"INFO: Win32 video codec init OK!\n");
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    HRESULT ret;
    vd_vfw_ctx *priv = sh->context;
    
#ifdef BUILD_VFWEX
    ret = ICDecompressEndEx(priv->handle);
#else
    ret = ICDecompressEnd(priv->handle);
#endif
    if (ret)
    {
	mp_msg(MSGT_WIN32, MSGL_WARN, "ICDecompressEnd failed: %ld\n", ret);
	return;
    }

    ret = ICClose(priv->handle);
    if (ret)
    {
	mp_msg(MSGT_WIN32, MSGL_WARN, "ICClose failed: %ld\n", ret);
	return;
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
	mp_msg(MSGT_DECVIDEO,MSGL_WARN,MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec);
	return NULL;
    }

    // set stride:  (trick discovered by Andreas Ackermann - thanx!)
    sh->bih->biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);
    priv->o_bih->biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);

    sh->bih->biSizeImage = len;

#ifdef BUILD_VFWEX
    ret = ICDecompressEx(priv->handle, 
#else
    ret = ICDecompress(priv->handle, 
#endif
	  ( (sh->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( ((flags&3)==2 && !(sh->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ),
	   sh->bih, data, priv->o_bih, (flags&3) ? 0 : mpi->planes[0]);

    if ((int)ret){
      mp_msg(MSGT_DECVIDEO,MSGL_WARN,"Error decompressing frame, err=%ld\n",ret);
      return NULL;
    }
    
    // export palette:
    if(mpi->imgfmt==IMGFMT_RGB8 || mpi->imgfmt==IMGFMT_BGR8){
	if (priv->palette)
	{
	    mpi->planes[1] = priv->palette;
	    mpi->flags |= MP_IMGFLAG_RGB_PALETTE;
	    mp_dbg(MSGT_DECVIDEO, MSGL_DBG2, "Found and copied palette\n");
	}
	else
	    mpi->planes[1]=NULL;
    }
        
    return mpi;
}
