/*
  vo_ggi.c - General Graphics Interface (GGI) Renderer for MPlayer

  (C) Alex Beregszaszi <alex@naxine.org>
  
  Uses libGGI - http://www.ggi-project.org/

  Thanks to Andreas Beck for his patches.
  Many thanks to Atmosfear, he hacked this driver to working with Planar
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

#undef GET_DB_INFO

#define GGI_FRAMES 4

/* do not make conversions from planar formats */
#undef GGI_YUV_SUPPORT

#ifndef GGI_YUV_SUPPORT
#include "../postproc/rgb2rgb.h"
#endif

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

    uint8_t bpp;
    uint8_t bppmul;

    uint8_t mode;
    #define YUV 0
    #define RGB 1
    #define BGR 2
    
    /* YUV */
    int framePlaneY, framePlaneUV, framePlaneYUY;
    int stridePlaneY, stridePlaneUV, stridePlaneYUY;
    
    /* RGB */
    int framePlaneRGB;
    int stridePlaneRGB;
    
    /* original */
    int srcwidth, srcheight;
    int srcformat;
    int srcdepth;
    int srctype;
    
    /* destination */
    int dstwidth, dstheight;
    
    /* source image format */
    int format;
} ggi_conf;

static int ggi_setmode(uint32_t d_width, uint32_t d_height, int d_depth)
{
    ggi_mode mode =
    {
	GGI_FRAMES,		/* frames */
	{ GGI_AUTO, GGI_AUTO },	/* visible */
	{ GGI_AUTO, GGI_AUTO },	/* virt */
	{ GGI_AUTO, GGI_AUTO },	/* size */
	GT_AUTO,		/* graphtype */
	{ GGI_AUTO, GGI_AUTO }	/* dots per pixel */
    };
    ggi_color pal[256];

    mp_msg(MSGT_VO, MSGL_V, "[ggi] mode requested: %dx%d (%d depth)\n",
        d_width, d_height, d_depth);
    
//    mode.size.x = vo_screenwidth;
//    mode.size.y = vo_screenheight;
    mode.visible.x = d_width;
    mode.visible.y = d_height;
    
    switch(d_depth)
    {
	case 1:
	    mode.graphtype = GT_1BIT;
	    break;
	case 2:
	    mode.graphtype = GT_2BIT;
	    break;
	case 4:
	    mode.graphtype = GT_4BIT;
	    break;
	case 8:
	    mode.graphtype = GT_8BIT;
	    break;
	case 15:
	    mode.graphtype = GT_15BIT;
	    break;
	case 16:
	    mode.graphtype = GT_16BIT;
	    break;
	case 24:
	    mode.graphtype = GT_24BIT;
	    break;
	case 32:
	    mode.graphtype = GT_32BIT;
	    break;
	default:
	    mp_msg(MSGT_VO, MSGL_ERR, "[ggi] unknown bit depth - using auto\n");
	    mode.graphtype = GT_AUTO;
    }

    /* FIXME */    
    ggiCheckMode(ggi_conf.vis, &mode);
    
    if (ggiSetMode(ggi_conf.vis, &mode) != 0)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] unable to set mode\n");
	uninit();
	return(-1);
    }
    
    if (ggiGetMode(ggi_conf.vis, &mode) != 0)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] unable to get mode\n");
	uninit();
	return(-1);
    }

    ggi_conf.gmode = mode;
    
    vo_screenwidth = mode.visible.x;
    vo_screenheight = mode.visible.y;
    vo_depthonscreen = d_depth;
//    vo_depthonscreen = GT_DEPTH(mode.graphtype);
//    ggi_bpp = GT_SIZE(mode.graphtype);
    ggi_conf.bpp = vo_depthonscreen;

#ifdef GET_DB_INFO
    {
	const ggi_directbuffer *db = ggiDBGetBuffer(ggi_conf.vis, 0);
	
	if (db)
	{
	    vo_depthonscreen = db->buffer.plb.pixelformat->depth;
	    ggi_conf.bpp = db->buffer.plb.pixelformat->size / 8;
	}
    }
#endif

    if (GT_SCHEME(mode.graphtype) == GT_PALETTE)
    {
	ggiSetColorfulPalette(ggi_conf.vis);
	ggiGetPalette(ggi_conf.vis, 0, 1 << ggi_conf.bpp, pal);
    }

#if 0
    printf("[ggi] mode: ");
    ggiPrintMode(&ggi_conf.gmode);
    printf("\n");
#endif

    mp_msg(MSGT_VO, MSGL_INFO, "[ggi] screen: %dx%dx%d frames: %d\n",
	vo_screenwidth, vo_screenheight, vo_depthonscreen, ggi_conf.gmode.frames);

    ggi_conf.bppmul = (ggi_conf.bpp+7)/8;

    return(0);
}


static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
    uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t *info)
{
    int i;

    vo_depthonscreen = 32;
    mp_msg(MSGT_VO, MSGL_INFO, "[ggi] This driver has got bugs, if you can, fix them.\n");

    /* source image parameters */
    ggi_conf.srcwidth = width;
    ggi_conf.srcheight = height;
    ggi_conf.srcformat = format;
    if (IMGFMT_IS_RGB(ggi_conf.srcformat))
    {
	ggi_conf.srcdepth = IMGFMT_RGB_DEPTH(ggi_conf.srcformat);
	ggi_conf.srctype = RGB;
    }
    else
    if (IMGFMT_IS_BGR(ggi_conf.srcformat))
    {
	ggi_conf.srcdepth = IMGFMT_BGR_DEPTH(ggi_conf.srcformat);
	ggi_conf.srctype = BGR;
    }
    else
    switch(ggi_conf.srcformat)
    {
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
#ifdef GGI_YUV_SUPPORT
	    ggi_conf.srcdepth = 12;
	    ggi_conf.srctype = YUV;
#else
	    ggi_conf.bpp = vo_depthonscreen;
	    ggi_conf.mode = RGB;
	    yuv2rgb_init(vo_depthonscreen, MODE_RGB);
#endif
	    break;
	case IMGFMT_UYVY:
	case IMGFMT_YUY2:
#ifdef GGI_YUV_SUPPORT
	    ggi_conf.srcdepth = 16;
	    ggi_conf.srctype = YUV;
#else
	    ggi_conf.bpp = vo_depthonscreen;
	    ggi_conf.mode = RGB;
	    yuv2rgb_init(vo_depthonscreen, MODE_RGB);
#endif
	    break;
	default:
	    mp_msg(MSGT_VO, MSGL_FATAL, "[ggi] Unknown image format: %s\n",
		vo_format_name(ggi_conf.srcformat));
	    uninit();
	    return(-1);
    }

    ggi_conf.format = format;

    ggi_conf.framePlaneRGB = width * height * ((ggi_conf.bpp+7)/8); /* fix it! */
    ggi_conf.stridePlaneRGB = width * ((ggi_conf.bpp+7)/8);
#ifdef GGI_YUV_SUPPORT
    ggi_conf.framePlaneY = width * height;
    ggi_conf.framePlaneUV = (width * height) >> 2;
    ggi_conf.framePlaneYUY = width * height * 2;
    ggi_conf.stridePlaneY = width;
    ggi_conf.stridePlaneUV = width/2;
    ggi_conf.stridePlaneYUY = width * 2;
#endif
//    ggi_conf.width = width;
//    ggi_conf.height = height;
    ggi_conf.dstwidth = d_width ? d_width : width;
    ggi_conf.dstheight = d_height ? d_height : height;

    {
#if 0
	ggi_aspect_ret asp;

	if (width != d_width || height != d_height)
	    asp = aspect_size(width, height, d_width, d_height);
	else
	{
	    asp.w = width;
	    asp.h = height;
	}
#endif
	
	if (ggi_setmode(width, height, vo_depthonscreen) != 0)
	{
	    printf("ggi-init: setmode returned with error\n");
	    return(-1);
	}
    }

    mp_msg(MSGT_VO, MSGL_INFO, "[ggi] input: %d bpp %s - screen depth: %d\n",
	ggi_conf.bpp, vo_format_name(ggi_conf.format), vo_depthonscreen);

//    ggiSetFlags(ggi_conf.vis, GGIFLAG_ASYNC);

    {
	ggi_directbuffer *DB;
	
	for (i = 0; i < GGI_FRAMES; i++)
	    ggi_conf.buffer[i] = NULL;

	ggi_conf.frames = ggi_conf.currframe = 0;
	
	for (i = 0; DB = ggiDBGetBuffer(ggi_conf.vis, i); i++)
	{
	    if (!(DB->type & GGI_DB_SIMPLE_PLB) ||
		(DB->page_size != 0) ||
		(DB->write == NULL) ||
		(DB->noaccess != 0) ||
		(DB->align != 0) ||
		(DB->layout != blPixelLinearBuffer))
		continue;
	
	    ggi_conf.buffer[DB->frame] = DB;
	    ggi_conf.frames++;
	}
    }
    

    if (ggi_conf.buffer[0] == NULL)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] direct buffer is not available\n");
	uninit();
	return(-1);
    }
    
    for (i = 0; i < ggi_conf.frames; i++)
    {
	if (ggi_conf.buffer[i] == NULL)
	{
	    mp_msg(MSGT_VO, MSGL_ERR, "[ggi] direct buffer for doublbuffering is not available\n");
	    uninit();
	    return(-1);
	}
    }

    ggiSetDisplayFrame(ggi_conf.vis, ggi_conf.currframe);
    ggiSetWriteFrame(ggi_conf.vis, ggi_conf.currframe);    

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

#if 1
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
	    switch(ggi_conf.format)
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
#else
    memcpy(ggi_conf.buffer[ggi_conf.currframe]->write, src[0], ggi_conf.framePlaneRGB);
#endif

    ggiResourceRelease(ggi_conf.buffer[ggi_conf.currframe]->resource);
    return(0);
}

static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
    unsigned char *srca, int stride)
{
    switch(ggi_conf.format)
    {
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
#ifdef GGI_YUV_SUPPORT
	    vo_draw_alpha_yv12(w, h, src, srca, stride,
		ggi_conf.buffer[ggi_conf.currframe]->write+(ggi_conf.srcwidth*y0+x0),
		ggi_conf.srcwidth);
#else
	    switch (vo_depthonscreen)
	    {
		case 32:
        	    vo_draw_alpha_rgb32(w, h, src, srca, stride, 
			ggi_conf.buffer[ggi_conf.currframe]->write+4*(ggi_conf.srcwidth*y0+x0), 4*ggi_conf.srcwidth);
		    break;
		case 24:
        	    vo_draw_alpha_rgb24(w, h, src, srca, stride, 
	    		ggi_conf.buffer[ggi_conf.currframe]->write+3*(ggi_conf.srcwidth*y0+x0), 3*ggi_conf.srcwidth);
		    break;
		case 16:
        	    vo_draw_alpha_rgb16(w, h, src, srca, stride, 
			ggi_conf.buffer[ggi_conf.currframe]->write+2*(ggi_conf.srcwidth*y0+x0), 2*ggi_conf.srcwidth);
		    break;
		case 15:
        	    vo_draw_alpha_rgb15(w, h, src, srca, stride, 
			ggi_conf.buffer[ggi_conf.currframe]->write+2*(ggi_conf.srcwidth*y0+x0), 2*ggi_conf.srcwidth);
		    break;
	    }
#endif
	    break;
	case IMGFMT_YUY2:
	case IMGFMT_YVYU:
	    vo_draw_alpha_yuy2(w, h, src, srca, stride,
		ggi_conf.buffer[ggi_conf.currframe]->write+2*(ggi_conf.srcwidth*y0+x0),
		2*ggi_conf.srcwidth);
	    break;
	case IMGFMT_UYVY:
	    vo_draw_alpha_yuy2(w, h, src, srca, stride,
		ggi_conf.buffer[ggi_conf.currframe]->write+2*(ggi_conf.srcwidth*y0+x0)+1,
		2*ggi_conf.srcwidth);
	    break;
	case IMGFMT_RGB15:
        case IMGFMT_BGR15:
            vo_draw_alpha_rgb15(w, h, src, srca, stride, 
		ggi_conf.buffer[ggi_conf.currframe]->write+2*(ggi_conf.srcwidth*y0+x0), 2*ggi_conf.srcwidth);
            break;
        case IMGFMT_RGB16:
        case IMGFMT_BGR16:
            vo_draw_alpha_rgb16(w, h, src, srca, stride, 
		ggi_conf.buffer[ggi_conf.currframe]->write+2*(ggi_conf.srcwidth*y0+x0), 2*ggi_conf.srcwidth);
            break;
        case IMGFMT_RGB24:
        case IMGFMT_BGR24:
            vo_draw_alpha_rgb24(w, h, src, srca, stride, 
		ggi_conf.buffer[ggi_conf.currframe]->write+3*(ggi_conf.srcwidth*y0+x0), 3*ggi_conf.srcwidth);
            break;
        case IMGFMT_RGB32:
        case IMGFMT_BGR32:
            vo_draw_alpha_rgb32(w, h, src, srca, stride, 
		ggi_conf.buffer[ggi_conf.currframe]->write+4*(ggi_conf.srcwidth*y0+x0), 4*ggi_conf.srcwidth);
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
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h,
    int x, int y)
{
    ggiResourceAcquire(ggi_conf.buffer[ggi_conf.currframe]->resource,
	GGI_ACTYPE_WRITE);

    ggiSetWriteFrame(ggi_conf.vis, ggi_conf.currframe);

#ifndef GGI_YUV_SUPPORT
    yuv2rgb(((uint8_t *) ggi_conf.buffer[ggi_conf.currframe]->write)+
	ggi_conf.buffer[ggi_conf.currframe]->buffer.plb.stride*y+
	x*(ggi_conf.buffer[ggi_conf.currframe]->buffer.plb.pixelformat->size/8),
	src[0], src[1], src[2], w, h, ggi_conf.buffer[ggi_conf.currframe]->buffer.plb.stride,
	stride[0], stride[1]);
#else
    int i;

    ggi_conf.buffer[ggi_conf.currframe]->write += (ggi_conf.stridePlaneY * y + x);
    for (i = 0; i < h; i++)
    {
	memcpy(ggi_conf.buffer[ggi_conf.currframe]->write, src[0], w);
	src[0] += stride[0];
	ggi_conf.buffer[ggi_conf.currframe]->write += ggi_conf.stridePlaneY;
    }
    
    x /= 2;
    y /= 2;
    w /= 2;
    h /= 2;
    
    ggi_conf.buffer[ggi_conf.currframe]->write += ggi_conf.stridePlaneY + (ggi_conf.stridePlaneUV * y + x);
    for (i = 0; i < h; i++)
    {
	memcpy(ggi_conf.buffer[ggi_conf.currframe]->write, src[1], w);
	src[1] += stride[1];
	ggi_conf.buffer[ggi_conf.currframe]->write += ggi_conf.stridePlaneUV;
    }
#endif

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
/*	case IMGFMT_YUY2:
	case IMGFMT_YVYU:
	case IMGFMT_UYVY:*/
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

    if (arg)
	ggi_conf.driver = arg;
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
    ggiClose(ggi_conf.vis);
    ggiExit();
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}

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
