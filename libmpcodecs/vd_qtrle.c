#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"Quicktime Animation (RLE) decoder",
	"qtrle",
	VFM_QTRLE,
	"A'rpi",
	"Mike Melanson",
	"native codec"
};

LIBVD_EXTERN(qtrle)

typedef struct {
    int depth;
    void *palette;
} vd_qtrle_ctx;

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    vd_qtrle_ctx *ctx = sh->context;
    switch(cmd)
    {
	case VDCTRL_QUERY_FORMAT:
	{
	    int req_format = *((int*)arg);
	    
	    /* qtrle24 supports 32bit output too */
	    if ((req_format == (IMGFMT_BGR|ctx->depth)) ||
		((IMGFMT_BGR_DEPTH(req_format) == 32) && (ctx->depth == 24)))
		return(CONTROL_TRUE);
	    else
		return(CONTROL_FALSE);
	}
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    vd_qtrle_ctx *ctx;
    
    ctx = sh->context = malloc(sizeof(vd_qtrle_ctx));
    if (!ctx)
	return(0);
    memset(ctx, 0, sizeof(vd_qtrle_ctx));
    
    if (!sh->bih)
	return(0);
    ctx->depth = sh->bih->biBitCount;

    switch(ctx->depth)
    {
	case 2:
	case 4:
	case 8:
	    if (sh->bih->biSize > 40)
	    {
		ctx->palette = malloc(sh->bih->biSize-40);
		memcpy(ctx->palette, sh->bih+40, sh->bih->biSize-40);
	    }
	    break;
	case 16:
	    ctx->depth--; /* this is the trick ;) */
	    break;
	case 24:
	    break;
	default:
	    mp_msg(MSGT_DECVIDEO,MSGL_ERR,
		"*** FYI: This Quicktime file is using %d-bit RLE Animation\n" \
		"encoding, which is not yet supported by MPlayer. But if you upload\n" \
	        "this Quicktime file to the MPlayer FTP, the team could look at it.\n",
		ctx->depth);
	    return(0);
    }

    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR|ctx->depth);
}

// uninit driver
static void uninit(sh_video_t *sh){
    vd_qtrle_ctx *ctx = sh->context;

    if (ctx->palette)
	free(ctx->palette);
    free(ctx);
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

void qt_decode_rle(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height,
  int encoded_bpp,
  int bytes_per_pixel);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    vd_qtrle_ctx *ctx = sh->context;
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_STATIC, MP_IMGFLAG_PRESERVE, 
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    qt_decode_rle(
        data,len, mpi->planes[0],
        sh->disp_w, sh->disp_h,
        sh->bih->biBitCount,
        mpi->bpp/8);

    if (ctx->palette)
	mpi->planes[1] = ctx->palette;
    
    return mpi;
}
