#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

#ifdef USE_LIBLZO
#include <lzo1x.h>
#else
#include "native/minilzo.h"
#define lzo_malloc malloc
#define lzo_free free
#endif

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
    lzo_byte *wrkmem;
    int codec;
} lzo_context_t;

// to set/get/query special features/parameters
static int control (sh_video_t *sh, int cmd, void* arg, ...)
{
    lzo_context_t *priv = sh->context;
    //printf("[%s] Query!! (%s)\n", MOD_NAME, (codec==IMGFMT_BGR24)?"BGR":"none");
    //printf("[%s] Query!! (%s)\n", MOD_NAME, (codec==IMGFMT_YV12)?"YV12":"none");
    switch(cmd){
    case VDCTRL_QUERY_FORMAT:
	if( (*((int*)arg)) == IMGFMT_BGR24 && priv->codec == IMGFMT_BGR24) return CONTROL_TRUE;
	if( (*((int*)arg)) == IMGFMT_YV12 && priv->codec == IMGFMT_YV12) return CONTROL_TRUE;
	return CONTROL_FALSE;
    }
    return CONTROL_UNKNOWN;
}


// init driver
static int init(sh_video_t *sh)
{
    lzo_context_t *priv;

    if (lzo_init() != LZO_E_OK) {
	mp_msg (MSGT_DECVIDEO, MSGL_ERR, "[%s] lzo_init() failed\n", MOD_NAME);
	return 0; 
    }

    priv = malloc(sizeof(lzo_context_t));
    if (!priv)
    {
	mp_msg (MSGT_DECVIDEO, MSGL_ERR, "[%s] memory allocation failed\n", MOD_NAME);
	return 0;
    }
    priv->codec = -1;
    sh->context = priv;

    priv->wrkmem = (lzo_bytep) lzo_malloc(LZO1X_1_MEM_COMPRESS);

    if (priv->wrkmem == NULL) {
	mp_msg (MSGT_DECVIDEO, MSGL_ERR, "[%s] Cannot alloc work memory\n", MOD_NAME);
	return 0;
    }

    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh)
{
    lzo_context_t *priv = sh->context;
    
    if (priv)
    {
	if (priv->wrkmem)
	    lzo_free(priv->wrkmem);
	free(priv);
    }

    sh->context = NULL;
}

//mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags)
{
    static int init_done = 0;
    int r;
    mp_image_t* mpi;
    int w;
    lzo_context_t *priv = sh->context;

    if (len <= 0) {
	    return NULL; // skipped frame
    }
    

    if (!init_done) {
	lzo_byte *tmp = lzo_malloc(sh->bih->biSizeImage);
	
	// decompress one frame to see if its
	// either YV12 or RGB24
	mp_msg (MSGT_DECVIDEO, MSGL_V, "[%s] 2 depth %d, format %d data %p len (%d) (%d)\n",
	    MOD_NAME, sh->bih->biBitCount, sh->format, data, len, sh->bih->biSizeImage
	    );

	/* decompress the frame */
	w = sh->bih->biSizeImage;
	r = lzo1x_decompress_safe (data, len, tmp, &w, priv->wrkmem);
	free(tmp);

	if (r != LZO_E_OK) {
	    /* this should NEVER happen */
	    mp_msg (MSGT_DECVIDEO, MSGL_ERR, 
		    "[%s] internal error - decompression failed: %d\n", MOD_NAME, r);
	    return NULL;
	}

	if        (w == (sh->bih->biSizeImage))   {
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

	if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,priv->codec)) return NULL;
	init_done++;
    }

    mpi = mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0,
	sh->disp_w, sh->disp_h);


    if (!mpi) {
	    mp_msg (MSGT_DECVIDEO, MSGL_ERR, "[%s] mpcodecs_get_image failed\n", MOD_NAME);
	    return NULL;
    }

    w = (mpi->w * mpi->h * mpi->bpp) / 8;
    r = lzo1x_decompress_safe (data, len, mpi->planes[0], &w, priv->wrkmem);
    if (r != LZO_E_OK) {
	/* this should NEVER happen */
	mp_msg (MSGT_DECVIDEO, MSGL_ERR, 
		"[%s] internal error - decompression failed: %d\n", MOD_NAME, r);
      return NULL;
    }

    mp_msg (MSGT_DECVIDEO, MSGL_DBG2, 
		"[%s] decompressed %lu bytes into %lu bytes\n", MOD_NAME,
		(long) len, (long)w);

    return mpi;
}

/* vim: sw=4
   */
