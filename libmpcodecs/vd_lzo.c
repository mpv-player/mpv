#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"
#include "libavutil/lzo.h"

#define MOD_NAME "DecLZO"

static vd_info_t info = {
	"LZO compressed Video",
	"lzo",
	"Tilmann Bitterberg",
	"Transcode development team <http://www.theorie.physik.uni-goettingen.de/~ostreich/transcode/>",
	"based on liblzo: http://www.oberhumer.com/opensource/lzo/"
};

LIBVD_EXTERN(lzo)

typedef struct {
    uint8_t *buffer;
    int bufsz;
    int codec;
} lzo_context_t;

// to set/get/query special features/parameters
static int control (sh_video_t *sh, int cmd, void* arg, ...)
{
    lzo_context_t *priv = sh->context;
    switch(cmd){
    case VDCTRL_QUERY_FORMAT:
	if (*(int *)arg == priv->codec) return CONTROL_TRUE;
	return CONTROL_FALSE;
    }
    return CONTROL_UNKNOWN;
}


// init driver
static int init(sh_video_t *sh)
{
    lzo_context_t *priv;

    if (sh->bih->biSizeImage <= 0) {
	mp_msg (MSGT_DECVIDEO, MSGL_ERR, "[%s] Invalid frame size\n", MOD_NAME);
	return 0; 
    }

    priv = malloc(sizeof(lzo_context_t));
    if (!priv)
    {
	mp_msg (MSGT_DECVIDEO, MSGL_ERR, "[%s] memory allocation failed\n", MOD_NAME);
	return 0;
    }
    priv->bufsz = sh->bih->biSizeImage;
    priv->buffer = malloc(priv->bufsz + AV_LZO_OUTPUT_PADDING);
    priv->codec = -1;
    sh->context = priv;

    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh)
{
    lzo_context_t *priv = sh->context;
    
    if (priv)
    {
	free(priv->buffer);
	free(priv);
    }

    sh->context = NULL;
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags)
{
    int r;
    mp_image_t* mpi;
    lzo_context_t *priv = sh->context;
    int w = priv->bufsz;

    if (len <= 0) {
	    return NULL; // skipped frame
    }
    
    r = av_lzo1x_decode(priv->buffer, &w, data, &len);
    if (r) {
	/* this should NEVER happen */
	mp_msg (MSGT_DECVIDEO, MSGL_ERR, 
		"[%s] internal error - decompression failed: %d\n", MOD_NAME, r);
      return NULL;
    }

    if (priv->codec == -1) {
	// detect RGB24 vs. YV12 via decoded size
	mp_msg (MSGT_DECVIDEO, MSGL_V, "[%s] 2 depth %d, format %d data %p len (%d) (%d)\n",
	    MOD_NAME, sh->bih->biBitCount, sh->format, data, len, sh->bih->biSizeImage
	    );

	if (w == 0) {
	    priv->codec = IMGFMT_BGR24;
	    mp_msg (MSGT_DECVIDEO, MSGL_V, "[%s] codec choosen is BGR24\n", MOD_NAME);
	} else if (w == (sh->bih->biSizeImage)/2) {
	    priv->codec = IMGFMT_YV12;
	    mp_msg (MSGT_DECVIDEO, MSGL_V, "[%s] codec choosen is YV12\n", MOD_NAME);
	} else {
	    priv->codec = -1;
	    mp_msg(MSGT_DECVIDEO,MSGL_ERR,"[%s] Unsupported out_fmt\n", MOD_NAME);
	    return NULL;
	}

	if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,priv->codec)) {
	    priv->codec = -1;
	    return NULL;
	}
    }

    mpi = mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0,
	sh->disp_w, sh->disp_h);


    if (!mpi) {
	    mp_msg (MSGT_DECVIDEO, MSGL_ERR, "[%s] mpcodecs_get_image failed\n", MOD_NAME);
	    return NULL;
    }

    mpi->planes[0] = priv->buffer;
    if (priv->codec == IMGFMT_BGR24)
        mpi->stride[0] = 3 * sh->disp_w;
    else {
        mpi->stride[0] = sh->disp_w;
        mpi->planes[2] = priv->buffer + sh->disp_w*sh->disp_h;
        mpi->stride[2] = sh->disp_w / 2;
        mpi->planes[1] = priv->buffer + sh->disp_w*sh->disp_h*5/4;
        mpi->stride[1] = sh->disp_w / 2;
    }

    mp_msg (MSGT_DECVIDEO, MSGL_DBG2, 
		"[%s] decompressed %lu bytes into %lu bytes\n", MOD_NAME,
		(long) len, (long)w);

    return mpi;
}

/* vim: sw=4
   */
