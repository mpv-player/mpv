/*
  vo_ggi.c - General Graphics Interface (GGI) Renderer for MPlayer

  (C) Alex Beregszaszi <alex@naxine.org>
  
  Uses libGGI - http://www.ggi-project.org/

  Many thanks to Atmosfear, he hacked this driver to working with Planar
  formats, and he fixed the RGB handling.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "fastmemcpy.h"

#include <ggi/ggi.h>

#undef GGI_OST
#undef GII_BUGGY_KEYCODES
#define GGI_OSD

#undef get_db_info

/* do not make conversions from planar formats */
#undef GGI_PLANAR_NOCONV

#ifndef GGI_PLANAR_NOCONV
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

extern int verbose;

/* idea stolen from vo_sdl.c :) */
static struct ggi_conf_s {
    char *driver;
    
    ggi_visual_t vis;
    ggi_directbuffer *buffer;

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
    int width, height;
    
    /* destination */
    int dstwidth, dstheight;
    
    /* source image format */
    int format;

    /* direct buffer */    
    uint8_t *dbuff;
    
    /* i.e. need lock */
    int need_acquire;

#ifdef GGI_OST
    ggi_pixel white;
    ggi_pixel black;
#endif
} ggi_conf;

static int ggi_setmode(uint32_t d_width, uint32_t d_height, int d_depth)
{
    ggi_mode mode =
    {
	1,			/* frames */
	{ GGI_AUTO, GGI_AUTO },	/* visible */
	{ GGI_AUTO, GGI_AUTO },	/* virt */
	{ GGI_AUTO, GGI_AUTO },	/* size */
	GT_AUTO,		/* graphtype */
	{ GGI_AUTO, GGI_AUTO }	/* dots per pixel */
    };
    ggi_color pal[256];

    if (verbose)
	printf("ggi-setmode: requested: %dx%d (%d depth)\n",
	    d_width, d_height, d_depth);
    
    mode.size.x = vo_screenwidth;
    mode.size.y = vo_screenheight;
    mode.visible.x = mode.virt.x = d_width;
    mode.visible.y = mode.virt.y = d_height;
    
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
	    printf("ggi-setmode: unknown bit depth - using auto\n");
	    mode.graphtype = GT_AUTO;
    }
    
    ggiCheckMode(ggi_conf.vis, &mode);
    
    if (ggiSetMode(ggi_conf.vis, &mode) != 0)
    {
	printf("ggi-setmode: unable to set mode\n");
	ggiClose(ggi_conf.vis);
	ggiExit();
	return(-1);
    }
    
    if (ggiGetMode(ggi_conf.vis, &mode) != 0)
    {
	printf("ggi-setmode: unable to get mode\n");
	ggiClose(ggi_conf.vis);
	ggiExit();
	return(-1);
    }
    
    ggi_conf.width = mode.virt.x;
    ggi_conf.height = mode.virt.y;
    vo_screenwidth = mode.visible.x;
    vo_screenheight = mode.visible.y;
    vo_depthonscreen = d_depth;
//    vo_depthonscreen = GT_DEPTH(mode.graphtype);
//    ggi_bpp = GT_SIZE(mode.graphtype);
    ggi_conf.bpp = vo_depthonscreen;

#ifdef get_db_info
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

    if (verbose)
	printf("ggi-setmode: %dx%d (virt: %dx%d) (size: %dx%d) screen depth: %d, bpp: %d\n",
	    vo_screenwidth, vo_screenheight, ggi_conf.width, ggi_conf.height,
	    mode.size.x, mode.size.y,
	    vo_depthonscreen, ggi_conf.bpp);

    ggi_conf.bppmul = (ggi_conf.bpp+7)/8;

    return(0);
}

typedef struct ggi_aspect_ret_s { int w, h, x, y; } ggi_aspect_ret;

/* stolen from vo_sdl.c */
#define MONITOR_ASPECT 4.0/3.0
static ggi_aspect_ret aspect_size(int srcw, int srch, int dstw, int dsth)
{
    ggi_aspect_ret ret;
    float float_h;

    if (verbose)
	printf("ggi-aspectsize: src: %dx%d dst: %dx%d\n",
	    srcw, srch, dstw, dsth);

    float_h = ((float)dsth / (float)srcw * (float)srch) * ((float)dsth /
	   ((float)dstw / (MONITOR_ASPECT)));

    if (float_h > dsth)
    {
	ret.w = (int)((float)dsth / (float)float_h) * dstw;
	ret.h = dsth;
	ret.x = (dstw - ret.w) / 2;
	ret.y = 0;
    }
    else
    {
	ret.h = (int)float_h;
	ret.w = dstw;
	ret.x = 0;
	ret.y = (dsth - ret.h) / 2;
    }

    printf("ggi-aspectsize: %dx%d (x: %d, y: %d)\n", ret.w, ret.h, ret.x, ret.y);
    return(ret);
}

static uint32_t init(uint32_t width, uint32_t height, uint32_t d_width,
    uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
    vo_depthonscreen = 32;
    printf("ggi-init: This driver has got bugs, if you can, fix them.\n");

    if (ggiInit() != 0)
    {
	printf("ggi-init: unable to initialize GGI\n");
	return(-1);
    }
    
    ggi_conf.driver = NULL;
    
    if ((ggi_conf.vis = ggiOpen(ggi_conf.driver)) == NULL)
    {
	printf("ggi-init: unable to open GGI for %s output\n",
	    (ggi_conf.driver == NULL) ? "default" : ggi_conf.driver);
	ggiExit();
	return(-1);
    }
    
    printf("ggi-init: using %s GGI output\n",
	(ggi_conf.driver == NULL) ? "default" : ggi_conf.driver);

    switch(format)
    {
	case IMGFMT_RGB8:
	    ggi_conf.bpp = 8;
	    ggi_conf.mode = RGB;
	    break;
	case IMGFMT_RGB15:
	    ggi_conf.bpp = 15;
	    ggi_conf.mode = RGB;
	    break;
	case IMGFMT_RGB16:
	    ggi_conf.bpp = 16;
	    ggi_conf.mode = RGB;
	    break;
	case IMGFMT_RGB24:
	    ggi_conf.bpp = 24;
	    ggi_conf.mode = RGB;
	    break;
	case IMGFMT_RGB32:
	    ggi_conf.bpp = 32;
	    ggi_conf.mode = RGB;
	    break;
	case IMGFMT_BGR8:
	    ggi_conf.bpp = 8;
	    ggi_conf.mode = BGR;
	    break;
	case IMGFMT_BGR15:
	    ggi_conf.bpp = 15;
	    ggi_conf.mode = BGR;
	    break;
	case IMGFMT_BGR16:
	    ggi_conf.bpp = 16;
	    ggi_conf.mode = BGR;
	    break;
	case IMGFMT_BGR24:
	    ggi_conf.bpp = 24;
	    ggi_conf.mode = BGR;
	    break;
	case IMGFMT_BGR32:
	    ggi_conf.bpp = 32;
	    ggi_conf.mode = BGR;
	    break;
	case IMGFMT_YV12: /* rgb, 24bit */
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	    ggi_conf.bpp = 16;
	    ggi_conf.mode = YUV;
#ifndef GGI_PLANAR_NOCONV
	    yuv2rgb_init(32/*vo_depthonscreen*/, MODE_RGB);
#endif
	    break;
	case IMGFMT_YUY2:
	    ggi_conf.bpp = 24;
	    ggi_conf.mode = YUV;
#ifndef GGI_PLANAR_NOCONV
	    yuv2rgb_init(32, MODE_RGB);
#endif
	    break;
	default:
	    printf("ggi-init: no suitable image format found (requested: %s)\n",
		vo_format_name(format));
	    return(-1);
    }

    ggi_conf.format = format;

    ggi_conf.framePlaneRGB = width * height * ((ggi_conf.bpp+7)/8); /* fix it! */
    ggi_conf.stridePlaneRGB = width * ((ggi_conf.bpp+7)/8);
#ifdef GGI_PLANAR_NOCONV
    ggi_conf.framePlaneY = width * height;
    ggi_conf.framePlaneUV = (width * height) >> 2;
    ggi_conf.framePlaneYUY = width * height * 2;
    ggi_conf.stridePlaneY = width;
    ggi_conf.stridePlaneUV = width/2;
    ggi_conf.stridePlaneYUY = width * 2;
#endif
    ggi_conf.width = width;
    ggi_conf.height = height;
    ggi_conf.dstwidth = d_width ? d_width : width;
    ggi_conf.dstheight = d_height ? d_height : height;

    {
	ggi_aspect_ret asp;

	if (width != d_width || height != d_height)
	    asp = aspect_size(width, height, d_width, d_height);
	else
	{
	    asp.w = width;
	    asp.h = height;
	}
	
	if (ggi_setmode(asp.w, asp.h, vo_depthonscreen) != 0)
	{
	    printf("ggi-init: setmode returned with error\n");
	    return(-1);
	}
    }

    printf("ggi-init: input: %d bpp %s - screen depth: %d\n", ggi_conf.bpp,
	vo_format_name(ggi_conf.format), vo_depthonscreen);

    ggiSetFlags(ggi_conf.vis, GGIFLAG_ASYNC);

    ggi_conf.buffer = (ggi_directbuffer *)ggiDBGetBuffer(ggi_conf.vis, 0);

    if (ggi_conf.buffer == NULL)
    {
	printf("ggi-init: double buffering is not available\n");
	ggiClose(ggi_conf.vis);
	ggiExit();
	return(-1);
    }
	
    if (!(ggi_conf.buffer->type & GGI_DB_SIMPLE_PLB) ||
	(ggi_conf.buffer->page_size != 0) ||
	(ggi_conf.buffer->write == NULL) ||
	(ggi_conf.buffer->noaccess != 0) ||
	(ggi_conf.buffer->align != 0) ||
	(ggi_conf.buffer->layout != blPixelLinearBuffer))
    {
	printf("ggi-init: incorrect video memory type\n");
	ggiClose(ggi_conf.vis);
	ggiExit();
	return(-1);
    }
    
    if (ggi_conf.buffer->resource != NULL)
	ggi_conf.need_acquire = 1;
    
    if (verbose && ggi_conf.need_acquire)
	printf("ggi-init: ggi needs acquire\n");
    
    ggi_conf.dbuff = ggi_conf.buffer->write;

#ifdef GGI_OST
    /* just for fun */
    {
	ggi_color col;

	/* set black */
	col.r = col.g = col.b = 0x0000;
	ggi_conf.black = ggiMapColor(ggi_conf.vis, &col);

	/* set white */
	col.r = col.g = col.b = 0xffff;
	ggi_conf.white = ggiMapColor(ggi_conf.vis, &col);

	ggiSetGCForeground(ggi_conf.vis, ggi_conf.white);
	ggiSetGCBackground(ggi_conf.vis, ggi_conf.black);
    }
#endif

    return(0);
}

static const vo_info_t *get_info(void)
{
    return &vo_info;
}

static uint32_t draw_frame(uint8_t *src[])
{
    if (ggi_conf.need_acquire)
	ggiResourceAcquire(ggi_conf.buffer->resource, GGI_ACTYPE_WRITE);

//    ggiSetDisplayFrame(ggi_conf.vis, ggi_conf.buffer->frame);
//    ggiSetWriteFrame(ggi_conf.vis, ggi_conf.buffer->frame);

#ifdef GGI_PLANAR_NOCONV
    switch(ggi_conf.format)
    {
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	    memcpy(ggi_conf.dbuff, src[0], ggi_conf.framePlaneY);
	    ggi_conf.dbuff += ggi_conf.framePlaneY;
	    memcpy(ggi_conf.dbuff, src[2], ggi_conf.framePlaneUV);
	    ggi_conf.dbuff += ggi_conf.framePlaneUV;
	    memcpy(ggi_conf.dbuff, src[1], ggi_conf.framePlaneUV);
	    printf("yv12 img written");
	    break;
	default:
	    memcpy(ggi_conf.dbuff, src[0], ggi_conf.framePlaneRGB);
    }
#else
    memcpy(ggi_conf.dbuff, src[0], ggi_conf.framePlaneRGB);
#endif

    if (ggi_conf.need_acquire)
	ggiResourceRelease(ggi_conf.buffer->resource);
    return(0);
}

#ifdef GGI_OSD
static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src,
    unsigned char *srca, int stride)
{
    switch(ggi_conf.format)
    {
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
#ifdef GGI_PLANAR_NOCONV
	    vo_draw_alpha_yv12(w, h, src, srca, stride,
		ggi_conf.dbuff+(ggi_conf.width*y0+x0),
		ggi_conf.width);
#else
	    switch (vo_depthonscreen)
	    {
		case 32:
        	    vo_draw_alpha_rgb32(w, h, src, srca, stride, 
			ggi_conf.dbuff+4*(ggi_conf.width*y0+x0), 4*ggi_conf.width);
		    break;
		case 24:
        	    vo_draw_alpha_rgb24(w, h, src, srca, stride, 
	    		ggi_conf.dbuff+3*(ggi_conf.width*y0+x0), 3*ggi_conf.width);
		    break;
		case 16:
        	    vo_draw_alpha_rgb16(w, h, src, srca, stride, 
			ggi_conf.dbuff+2*(ggi_conf.width*y0+x0), 2*ggi_conf.width);
		    break;
		case 15:
        	    vo_draw_alpha_rgb15(w, h, src, srca, stride, 
			ggi_conf.dbuff+2*(ggi_conf.width*y0+x0), 2*ggi_conf.width);
		    break;
	    }
#endif
	    break;
	case IMGFMT_YUY2:
	case IMGFMT_YVYU:
	    vo_draw_alpha_yuy2(w, h, src, srca, stride,
		ggi_conf.dbuff+2*(ggi_conf.width*y0+x0),
		2*ggi_conf.width);
	    break;
	case IMGFMT_UYVY:
	    vo_draw_alpha_yuy2(w, h, src, srca, stride,
		ggi_conf.dbuff+2*(ggi_conf.width*y0+x0)+1,
		2*ggi_conf.width);
	    break;
	case IMGFMT_RGB15:
        case IMGFMT_BGR15:
            vo_draw_alpha_rgb15(w, h, src, srca, stride, 
		ggi_conf.dbuff+2*(ggi_conf.width*y0+x0), 2*ggi_conf.width);
            break;
        case IMGFMT_RGB16:
        case IMGFMT_BGR16:
            vo_draw_alpha_rgb16(w, h, src, srca, stride, 
		ggi_conf.dbuff+2*(ggi_conf.width*y0+x0), 2*ggi_conf.width);
            break;
        case IMGFMT_RGB24:
        case IMGFMT_BGR24:
            vo_draw_alpha_rgb24(w, h, src, srca, stride, 
		ggi_conf.dbuff+3*(ggi_conf.width*y0+x0), 3*ggi_conf.width);
            break;
        case IMGFMT_RGB32:
        case IMGFMT_BGR32:
            vo_draw_alpha_rgb32(w, h, src, srca, stride, 
		ggi_conf.dbuff+4*(ggi_conf.width*y0+x0), 4*ggi_conf.width);
	    break;
    }
}
#endif

static void draw_osd(void)
{
#ifdef GGI_OSD
    vo_draw_text(ggi_conf.width, ggi_conf.height, draw_alpha);
#endif
}

static void flip_page(void)
{
    ggiFlush(ggi_conf.vis);
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h,
    int x, int y)
{
    if (ggi_conf.need_acquire)
	ggiResourceAcquire(ggi_conf.buffer->resource, GGI_ACTYPE_WRITE);
#ifndef GGI_PLANAR_NOCONV
    yuv2rgb(((uint8_t *) ggi_conf.dbuff)+(ggi_conf.width*y+x)*ggi_conf.bppmul,
	src[0], src[1], src[2], w, h, ggi_conf.width*ggi_conf.bppmul, stride[0],
	stride[1]);
#else
    int i;

    ggi_conf.dbuff += (ggi_conf.stridePlaneY * y + x);
    for (i = 0; i < h; i++)
    {
	memcpy(ggi_conf.dbuff, src[0], w);
	src[0] += stride[0];
	ggi_conf.dbuff += ggi_conf.stridePlaneY;
    }
    
    x /= 2;
    y /= 2;
    w /= 2;
    h /= 2;
    
    ggi_conf.dbuff += ggi_conf.stridePlaneY + (ggi_conf.stridePlaneUV * y + x);
    for (i = 0; i < h; i++)
    {
	memcpy(ggi_conf.dbuff, src[1], w);
	src[1] += stride[1];
	ggi_conf.dbuff += ggi_conf.stridePlaneUV;
    }
#endif
    if (ggi_conf.need_acquire)
	ggiResourceRelease(ggi_conf.buffer->resource);
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

static void uninit(void)
{
    ggiResourceRelease(ggi_conf.buffer->resource);
    ggiClose(ggi_conf.vis);
    ggiExit();
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
#if 0 /* debug ;) */
	printf("type: %4x, origin: %4x, sym: %4x, label: %4x, button=%4x\n",
	    event.any.origin, event.any.type, event.key.sym, event.key.label, event.key.button);
#endif

	if (event.key.type == evKeyPress)
	{
#ifdef GII_BUGGY_KEYCODES
	    switch(event.key.button)
	    {
		case 0x37:
		    mplayer_put_key('*');
		    break;
		case 0x68:
		    mplayer_put_key('/');
		    break;
		case 0x4e:
		    mplayer_put_key('+');
		    break;
		case 0x4a:
		    mplayer_put_key('-');
		    break;
		case 0x18: /* o */
		    mplayer_put_key('o');
		    break;
		case 0x22: /* g */
		    mplayer_put_key('g');
		    break;
		case 0x15: /* z */
		    mplayer_put_key('z');
		    break;
		case 0x2d: /* x */
		    mplayer_put_key('x');
		    break;
		case 0x32: /* m */
		    mplayer_put_key('m');
		    break;
		case 0x20: /* d */
		    mplayer_put_key('d');
		    break;
		case 0x10: /* q */
		    mplayer_put_key('q');
		    break;
		case 0x39: /* space */
		case 0x19: /* p */
		    mplayer_put_key('p');
		    break;
		case 0x5a:
		    mplayer_put_key(KEY_UP);
		    break;
		case 0x60:
		    mplayer_put_key(KEY_DOWN);
		    break;
		case 0x5c:
		    mplayer_put_key(KEY_LEFT);
		    break;
		case 0x5e:
		    mplayer_put_key(KEY_RIGHT);
		    break;
		case 0x5b:
		    mplayer_put_key(KEY_PAGE_UP);
		    break;
		case 0x61:
		    mplayer_put_key(KEY_PAGE_DOWN);
		    break;
		default:
		    break;
	    }
#else
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
#endif
	}
    }
    return;
}

static uint32_t preinit(const char *arg)
{
  return 0;
}

static void query_vaa(vo_vaa_t *vaa)
{
  memset(vaa,0,sizeof(vo_vaa_t));
}
