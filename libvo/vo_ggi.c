/*
  vo_ggi.c - General Graphics Interface (GGI) Renderer for MPlayer

  (C) Alex Beregszaszi <alex@naxine.org>
  
  Uses libGGI - http://www.ggi-project.org/

  TODO:
   * implement non-directbuffer support
   * improve directbuffer draw_frame (memcpy)
   * check on many devices
   * implement gamma handling
   * implement direct rendering support

  Thanks to Andreas Beck for his patches.
  Many thanks to Atmosfear, he hacked this driver to work with Planar
  formats, and he fixed the RGB handling.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mp_msg.h"

#include "../config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "fastmemcpy.h"

#include <ggi/ggi.h>

/* maximum buffers */
#define GGI_FRAMES 4

#include "../postproc/rgb2rgb.h"

LIBVO_EXTERN (ggi)

static vo_info_t vo_info = 
{
	"General Graphics Interface (GGI) output",
	"ggi",
	"Alex Beregszaszi <alex@naxine.org>",
	"under developement"
};

static struct ggi_conf_s {
    char *driver;
    
    ggi_visual_t vis;
    ggi_directbuffer *buffer[GGI_FRAMES];
    ggi_mode gmode;
    
    int frames;
    int currframe;
    
    /* source image format */
    int srcwidth;
    int srcheight;
    int srcformat;
    int srcdepth;
    
    /* destination */
    int dstwidth;
    int dstheight;
    
    int async;
} ggi_conf;

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
    uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t *info)
{
    ggi_mode mode =
    {
	GGI_FRAMES,		/* frames */
	{ width, height },	/* visible */
	{ GGI_AUTO, GGI_AUTO },	/* virt */
	{ GGI_AUTO, GGI_AUTO },	/* size */
	GT_AUTO,		/* graphtype */
	{ GGI_AUTO, GGI_AUTO }	/* dots per pixel */
    };
    int i;
    ggi_directbuffer *DB;
    
    switch(format)
    {
	case IMGFMT_RGB|8:
	case IMGFMT_BGR|8:
	    mode.graphtype = GT_8BIT;
	    break;
	case IMGFMT_RGB|15:
	case IMGFMT_BGR|15:
	    mode.graphtype = GT_15BIT;
	    break;
	case IMGFMT_RGB|16:
	case IMGFMT_BGR|16:
	    mode.graphtype = GT_16BIT;
	    break;
	case IMGFMT_RGB|24:
	case IMGFMT_BGR|24:
	    mode.graphtype = GT_24BIT;
	    break;
	case IMGFMT_RGB|32:
	case IMGFMT_BGR|32:
	    mode.graphtype = GT_32BIT;
	    break;
    }

#if 1
    printf("[ggi] mode: ");
    ggiPrintMode(&mode);
    printf("\n");
#endif

    ggiCheckMode(ggi_conf.vis, &mode);
    
    switch(format)
    {
	case IMGFMT_YV12:
    	    mode.graphtype = GT_32BIT;
    	    if (!ggiSetMode(ggi_conf.vis, &mode))
    		break;
	    mode.graphtype = GT_24BIT;
	    if (!ggiSetMode(ggi_conf.vis, &mode))
		break;
	    mode.graphtype = GT_16BIT;
	    if (!ggiSetMode(ggi_conf.vis, &mode))
		break;
	    mode.graphtype = GT_15BIT;
	    if (!ggiSetMode(ggi_conf.vis, &mode))
		break;
    }

    if (ggiSetMode(ggi_conf.vis, &mode))
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] unable to set mode\n");
	return(-1);
    }

    if (ggiGetMode(ggi_conf.vis, &mode) != 0)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] unable to get mode\n");
	return(-1);
    }

    if ((mode.graphtype == GT_INVALID) || (mode.graphtype == GT_AUTO))
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] not supported depth/bpp\n");
	return(-1);
    }

    ggi_conf.gmode = mode;

#if 1
    printf("[ggi] mode: ");
    ggiPrintMode(&mode);
    printf("\n");
#endif

    vo_depthonscreen = GT_DEPTH(mode.graphtype);
    vo_screenwidth = mode.visible.x;
    vo_screenheight = mode.visible.y;
    
    vo_dx = vo_dy = 0;
    vo_dwidth = mode.virt.x;
    vo_dheight = mode.virt.y;
    vo_dbpp = GT_SIZE(mode.graphtype);

    ggi_conf.srcwidth = width;
    ggi_conf.srcheight = height;
    ggi_conf.srcformat = format;

    if (IMGFMT_IS_RGB(ggi_conf.srcformat))
    {
	ggi_conf.srcdepth = IMGFMT_RGB_DEPTH(ggi_conf.srcformat);
    }
    else
    if (IMGFMT_IS_BGR(ggi_conf.srcformat))
    {
	ggi_conf.srcdepth = IMGFMT_BGR_DEPTH(ggi_conf.srcformat);
    }
    else
    switch(ggi_conf.srcformat)
    {
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	    ggi_conf.srcdepth = vo_dbpp;
	    yuv2rgb_init(ggi_conf.srcdepth, MODE_RGB);
	    break;
	default:
	    mp_msg(MSGT_VO, MSGL_FATAL, "[ggi] Unknown image format: %s\n",
		vo_format_name(ggi_conf.srcformat));
	    return(-1);
    }

    vo_dwidth = ggi_conf.dstwidth = ggi_conf.gmode.virt.x;
    vo_dheight = ggi_conf.dstheight = ggi_conf.gmode.virt.y;

    ggi_conf.frames = ggiDBGetNumBuffers(ggi_conf.vis);
    if (ggi_conf.frames > GGI_FRAMES)
	ggi_conf.frames = GGI_FRAMES;
    
    ggi_conf.currframe = 0;
    if (!ggi_conf.frames)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] direct buffer unavailable\n");
	return(-1);
    }

    for (i = 0; i < ggi_conf.frames; i++)
        ggi_conf.buffer[i] = NULL;

    /* get available number of buffers */
    for (i = 0; DB = (ggi_directbuffer *)ggiDBGetBuffer(ggi_conf.vis, i),
	i < ggi_conf.frames; i++)
    {
        if (!(DB->type & GGI_DB_SIMPLE_PLB) ||
    	    (DB->page_size != 0) ||
    	    (DB->write == NULL) ||
	    (DB->noaccess != 0) ||
	    (DB->align != 0) ||
	    (DB->layout != blPixelLinearBuffer))
	    continue;
	
	ggi_conf.buffer[DB->frame] = DB;
    }

    if (ggi_conf.buffer[0] == NULL)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] direct buffer unavailable\n");
	return(-1);
    }
    
    for (i = 0; i < ggi_conf.frames; i++)
    {
	if (ggi_conf.buffer[i] == NULL)
	{
	    ggi_conf.frames = i-1;
	    break;
	}
    }

    ggiSetDisplayFrame(ggi_conf.vis, ggi_conf.currframe);
    ggiSetWriteFrame(ggi_conf.vis, ggi_conf.currframe);    

    mp_msg(MSGT_VO, MSGL_INFO, "[ggi] input: %dx%dx%d, output: %dx%dx%d, frames: %d\n",
	ggi_conf.srcwidth, ggi_conf.srcheight, ggi_conf.srcdepth, 
	vo_dwidth, vo_dheight, vo_dbpp, ggi_conf.frames);

    if (ggiGetFlags(ggi_conf.vis) & GGIFLAG_ASYNC)
    {
	mp_msg(MSGT_VO, MSGL_INFO, "[ggi] using asynchron mode\n");
	ggi_conf.async = 1;
    }

    return(0);
}

static const vo_info_t *get_info(void)
{
    return &vo_info;
}

static uint32_t draw_frame(uint8_t *src[])
{
    int x, y, size;
    unsigned char *ptr, *ptr2, *spt;
    
    ggiResourceAcquire(ggi_conf.buffer[ggi_conf.currframe]->resource,
	GGI_ACTYPE_WRITE);

    ggiSetWriteFrame(ggi_conf.vis, ggi_conf.currframe);

    ptr = ggi_conf.buffer[ggi_conf.currframe]->write;
    spt = src[0];
    size = ggi_conf.buffer[ggi_conf.currframe]->buffer.plb.pixelformat->size/8;
    
    for (y = 0; y < ggi_conf.srcheight; y++)
    {
	ptr2 = ptr;
	for (x = 0; x < ggi_conf.srcwidth; x++)
	{
	    ptr2[0] = *spt++;
	    ptr2[1] = *spt++;
	    ptr2[2] = *spt++;
	    switch(ggi_conf.srcformat)
	    {
		case IMGFMT_BGR32:
		case IMGFMT_RGB32:
		    spt++;
		    break;
	    }
	    ptr2 += size;
	}
	ptr += ggi_conf.buffer[ggi_conf.currframe]->buffer.plb.stride;
    }

    ggiResourceRelease(ggi_conf.buffer[ggi_conf.currframe]->resource);
    return(0);
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
    unsigned char *srca, int stride)
{
    switch(vo_dbpp)
    {
    case 32:
        vo_draw_alpha_rgb32(w, h, src, srca, stride, 
    	    ggi_conf.buffer[ggi_conf.currframe]->write+4*(ggi_conf.dstwidth*y0+x0), 4*ggi_conf.dstwidth);
	break;
    case 24:
        vo_draw_alpha_rgb24(w, h, src, srca, stride, 
	    ggi_conf.buffer[ggi_conf.currframe]->write+3*(ggi_conf.dstwidth*y0+x0), 3*ggi_conf.dstwidth);
	break;
    case 16:
	vo_draw_alpha_rgb16(w, h, src, srca, stride, 
	    ggi_conf.buffer[ggi_conf.currframe]->write+2*(ggi_conf.dstwidth*y0+x0), 2*ggi_conf.dstwidth);
	break;
    case 15:
	vo_draw_alpha_rgb15(w, h, src, srca, stride, 
	    ggi_conf.buffer[ggi_conf.currframe]->write+2*(ggi_conf.dstwidth*y0+x0), 2*ggi_conf.dstwidth);
	break;
    }
}

static void draw_osd(void)
{
    vo_draw_text(ggi_conf.srcwidth, ggi_conf.srcheight, draw_alpha);
}

static void flip_page(void)
{
    ggiSetDisplayFrame(ggi_conf.vis, ggi_conf.currframe);
    mp_dbg(MSGT_VO, MSGL_DBG2, "flip_page: current write frame: %d, display frame: %d\n",
	ggiGetWriteFrame(ggi_conf.vis), ggiGetDisplayFrame(ggi_conf.vis));

    ggi_conf.currframe = (ggi_conf.currframe+1) % ggi_conf.frames;
    
    if (ggi_conf.async)
	ggiFlush(ggi_conf.vis);
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h,
    int x, int y)
{
    ggiResourceAcquire(ggi_conf.buffer[ggi_conf.currframe]->resource,
	GGI_ACTYPE_WRITE);

    ggiSetWriteFrame(ggi_conf.vis, ggi_conf.currframe);

    yuv2rgb(((uint8_t *) ggi_conf.buffer[ggi_conf.currframe]->write)+
	ggi_conf.buffer[ggi_conf.currframe]->buffer.plb.stride*y+
	x*(ggi_conf.buffer[ggi_conf.currframe]->buffer.plb.pixelformat->size/8),
	src[0], src[1], src[2], w, h, ggi_conf.buffer[ggi_conf.currframe]->buffer.plb.stride,
	stride[0], stride[1]);

    ggiResourceRelease(ggi_conf.buffer[ggi_conf.currframe]->resource);
    return(0);
}

static uint32_t query_format(uint32_t format)
{
    switch(format)
    {
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	    return(0x6);
	case IMGFMT_RGB8:
	case IMGFMT_RGB15:
	case IMGFMT_RGB16:
        case IMGFMT_RGB24:
	case IMGFMT_RGB32:
	case IMGFMT_BGR8:
	case IMGFMT_BGR15:
	case IMGFMT_BGR16:
	case IMGFMT_BGR24:
	case IMGFMT_BGR32:
	    return(0x5);
    }
    return(0);
}

static uint32_t preinit(const char *arg)
{
    if (ggiInit() != 0)
    {
	mp_msg(MSGT_VO, MSGL_FATAL, "[ggi] unable to initialize GGI\n");
	return(-1);
    }

    if ((char *)arg)
	ggi_conf.driver = strdup(arg);
    else
	ggi_conf.driver = NULL;
    
    if ((ggi_conf.vis = ggiOpen(ggi_conf.driver)) == NULL)
    {
	mp_msg(MSGT_VO, MSGL_FATAL, "[ggi] unable to open '%s' output\n",
	    (ggi_conf.driver == NULL) ? "default" : ggi_conf.driver);
	ggiExit();
	return(-1);
    }
    
    mp_msg(MSGT_VO, MSGL_V, "[ggi] using '%s' output\n",
	(ggi_conf.driver == NULL) ? "default" : ggi_conf.driver);

    return 0;
}

static void uninit(void)
{
    if (ggi_conf.driver)
	free(ggi_conf.driver);
    ggiClose(ggi_conf.vis);
    ggiExit();
}

#ifdef GGI_GAMMA
/* GAMMA handling */
static int ggi_get_video_eq(vidix_video_eq_t *info)
{
    memset(info, 0, sizeof(vidix_video_eq_t));
}

static void query_vaa(vo_vaa_t *vaa)
{
    memset(vaa, 0, sizeof(vo_vaa_t));
    vaa->get_video_eq = ggi_get_video_eq;
    vaa->set_video_eq = ggi_set_video_eq;
}
#endif

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
#ifdef GGI_GAMMA
  case VOCTRL_QUERY_VAA:
    query_vaa((vo_vaa_t*)data);
    return VO_TRUE;
#endif
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}

/* EVENT handling */
#include "../linux/keycodes.h"
extern void mplayer_put_key(int code);

static void check_events(void)
{
    struct timeval tv = {0, 0};
    ggi_event event;
    ggi_event_mask mask;

    if ((mask = ggiEventPoll(ggi_conf.vis, emAll, &tv)))
    if (ggiEventRead(ggi_conf.vis, &event, emAll) != 0)
    {
	mp_dbg(MSGT_VO, MSGL_DBG3, "type: %4x, origin: %4x, sym: %4x, label: %4x, button=%4x\n",
	    event.any.origin, event.any.type, event.key.sym, event.key.label, event.key.button);

	if (event.key.type == evKeyPress)
	{
	    switch(event.key.sym)
	    {
		case GIIK_PAsterisk: /* PStar */
		case GIIUC_Asterisk:
		    mplayer_put_key('*');
		    break;
		case GIIK_PSlash:
		case GIIUC_Slash:
		    mplayer_put_key('/');
		    break;
		case GIIK_PPlus:
		case GIIUC_Plus:
		    mplayer_put_key('+');
		    break;
		case GIIK_PMinus:
		case GIIUC_Minus:
		    mplayer_put_key('-');
		    break;
		case GIIUC_o:
		case GIIUC_O:
		    mplayer_put_key('o');
		    break;
		case GIIUC_g:
		case GIIUC_G:
		    mplayer_put_key('g');
		    break;
		case GIIUC_z:
		case GIIUC_Z:
		    mplayer_put_key('z');
		    break;
		case GIIUC_x:
		case GIIUC_X:
		    mplayer_put_key('x');
		    break;
		case GIIUC_m:
		case GIIUC_M:
		    mplayer_put_key('m');
		    break;
		case GIIUC_d:
		case GIIUC_D:
		    mplayer_put_key('d');
		    break;
		case GIIUC_q:
		case GIIUC_Q:
		    mplayer_put_key('q');
		    break;
		case GIIUC_h:
		case GIIUC_H:
		    mplayer_put_key('h');
		    break;
		case GIIUC_l:
		case GIIUC_L:
		    mplayer_put_key('l');
		    break;
		case GIIUC_Space:
		case GIIUC_p:
		case GIIUC_P:
		    mplayer_put_key('p');
		    break;
		case GIIK_Up:
		    mplayer_put_key(KEY_UP);
		    break;
		case GIIK_Down:
		    mplayer_put_key(KEY_DOWN);
		    break;
		case GIIK_Left:
		    mplayer_put_key(KEY_LEFT);
		    break;
		case GIIK_Right:
		    mplayer_put_key(KEY_RIGHT);
		    break;
		case GIIK_PageUp:
		    mplayer_put_key(KEY_PAGE_UP);
		    break;
		case GIIK_PageDown:
		    mplayer_put_key(KEY_PAGE_DOWN);
		    break;
		default:
		    break;
	    }
	}
    }
    return;
}
