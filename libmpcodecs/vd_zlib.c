#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef HAVE_ZLIB
#include "mp_msg.h"

#include <zlib.h>

#include "vd_internal.h"

static vd_info_t info = {
	"zlib decoder (avizlib)",
	"zlib",
	VFM_ZLIB,
	"Alex",
	"based on vd_ijpg.c",
	"uses zlib, supports only BGR24 (as AVIzlib)"
};

LIBVD_EXTERN(zlib)

typedef struct {
    int width;
    int height;
    int depth;
    z_stream zstrm;
} vd_zlib_ctx;

// to set/get/query special features/parameters
static int control(sh_video_t *sh, int cmd, void *arg, ...)
{
    vd_zlib_ctx *ctx = sh->context;
    switch(cmd)
    {
	case VDCTRL_QUERY_FORMAT:
	{
	    if (*((int*)arg) == (IMGFMT_BGR|ctx->depth))
		return(CONTROL_TRUE);
	    else
		return(CONTROL_FALSE);
	}
    }
    return(CONTROL_UNKNOWN);
}

// init driver
static int init(sh_video_t *sh)
{
    int zret;
    vd_zlib_ctx *ctx;
    
    ctx = sh->context = malloc(sizeof(vd_zlib_ctx));
    if (!ctx)
	return(0);
    memset(ctx, 0, sizeof(vd_zlib_ctx));

    ctx->width = sh->bih->biWidth;
    ctx->height = sh->bih->biHeight;
    ctx->depth = sh->bih->biBitCount;

    ctx->zstrm.zalloc = (alloc_func)NULL;
    ctx->zstrm.zfree = (free_func)NULL;
    ctx->zstrm.opaque = (voidpf)NULL;

    zret = inflateInit(&ctx->zstrm);
    if (zret != Z_OK)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[vd_zlib] inflate init error: %d\n",
	    zret);
	return(NULL);
    }

    if (!mpcodecs_config_vo(sh, ctx->width, ctx->height, IMGFMT_BGR|ctx->depth))
	return(NULL);


    return(1);
}

// uninit driver
static void uninit(sh_video_t *sh)
{
    vd_zlib_ctx *ctx = sh->context;

    inflateEnd(&ctx->zstrm);
    if (ctx)
	free(ctx);
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);
				  
// decode a frame
static mp_image_t* decode(sh_video_t *sh, void* data, int len, int flags)
{
    mp_image_t *mpi;
    vd_zlib_ctx *ctx = sh->context;
    int zret;
    int decomp_size = ctx->width*ctx->height*((ctx->depth+7)/8);
    z_stream *zstrm = &ctx->zstrm;

    if (len <= 0)
	return(NULL); // skipped frame

    mpi = mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0, ctx->width, ctx->height);
    if (!mpi) return(NULL);

    zstrm->next_in = data;
    zstrm->avail_in = len;
    zstrm->next_out = mpi->planes[0];
    zstrm->avail_out = decomp_size;

    mp_dbg(MSGT_DECVIDEO, MSGL_DBG2, "[vd_zlib] input: %p (%d bytes), output: %p (%d bytes)\n",
	zstrm->next_in, zstrm->avail_in, zstrm->next_out, zstrm->avail_out);
    
    zret = inflate(zstrm, Z_NO_FLUSH);
    if ((zret != Z_OK) && (zret != Z_STREAM_END))
    {
	mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[vd_zlib] inflate error: %d\n",
	    zret);
	return(NULL);
    }
    
    if (decomp_size != (int)zstrm->total_out)
    {
	mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[vd_zlib] decoded size differs (%d != %d)\n",
	    decomp_size, zstrm->total_out);
	return(NULL);
    }

    return mpi;
}
#endif
