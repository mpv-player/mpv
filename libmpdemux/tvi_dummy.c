/*
    Only a sample!
*/

#include "config.h"

#ifdef USE_TV

#include <stdio.h>
#include "../libvo/img_format.h"
#include "tv.h"

/* information about this file */
static tvi_info_t info = {
	"NULL-TV",
	"dummy",
	"alex",
	NULL
};

/* private data's */
typedef struct {
    int width;
    int height;
} priv_t;

#include "tvi_def.h"

/* handler creator - entry point ! */
tvi_handle_t *tvi_init_dummy(char *device)
{
    return(new_handle());
}

/* initialisation */
static int init(priv_t *priv)
{
    priv->width = 320;
    priv->height = 200;
    return(1);
}

/* that's the real start, we'got the format parameters (checked with control) */
static int start(priv_t *priv)
{
    return(1);
}

static int uninit(priv_t *priv)
{
    return(1);
}

static int control(priv_t *priv, int cmd, void *arg)
{
    switch(cmd)
    {
	case TVI_CONTROL_IS_VIDEO:
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_FORMAT:
//	    (int)*(void **)arg = IMGFMT_YV12;
	    *(int *)arg = IMGFMT_YV12;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_FORMAT:
	{
//	    int req_fmt = (int)*(void **)arg;
	    int req_fmt = *(int *)arg;
	    if (req_fmt != IMGFMT_YV12)
		return(TVI_CONTROL_FALSE);
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_VID_SET_WIDTH:
	    priv->width = (int)*(void **)arg;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_WIDTH:
	    (int)*(void **)arg = priv->width;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_HEIGHT:
	    priv->height = (int)*(void **)arg;
	    return(TVI_CONTROL_TRUE);	    
	case TVI_CONTROL_VID_GET_HEIGHT:
	    (int)*(void **)arg = priv->height;
	    return(TVI_CONTROL_TRUE);	    
	case TVI_CONTROL_VID_CHK_WIDTH:
	case TVI_CONTROL_VID_CHK_HEIGHT:
	    return(TVI_CONTROL_TRUE);
    }
    return(TVI_CONTROL_UNKNOWN);
}

#ifdef HAVE_TV_BSDBT848
static double grabimmediate_video_frame(priv_t *priv, char *buffer, int len)
{
    memset(buffer, 0xCC, len);
    return(1);
}
#endif

static double grab_video_frame(priv_t *priv, char *buffer, int len)
{
    memset(buffer, 0x42, len);
    return(1);
}

static int get_video_framesize(priv_t *priv)
{
    /* YV12 */
    return(priv->width*priv->height*12/8);
}

static double grab_audio_frame(priv_t *priv, char *buffer, int len)
{
    memset(buffer, 0x42, len);
    return(1);
}

static int get_audio_framesize(priv_t *priv)
{
    return(1);
}

#endif /* USE_TV */
