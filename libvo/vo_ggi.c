/*
  vo_ggi.c - General Graphics Interface (GGI) Renderer for MPlayer

  (C) Alex Beregszaszi
  
  Uses libGGI - http://www.ggi-project.org/

  TODO:
   * check on many devices
   * implement gamma handling (VAA isn't obsoleted?)
 
  BUGS:
   * palettized playback has bad colors, probably swapped palette?
   * fbdev & DR produces two downscaled images
   * fbdev & FLIP (& DR) produces no image

  Thanks to Andreas Beck for his patches.

  Many thanks to Atmosfear, he hacked this driver to work with Planar
  formats, and he fixed the RGB handling.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mp_msg.h"

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "fastmemcpy.h"

#include <ggi/ggi.h>

/* maximum buffers */
#undef GGI_FLIP

static vo_info_t info = 
{
	"General Graphics Interface (GGI) output",
	"ggi",
	"Alex Beregszaszi",
	"major"
};

LIBVO_EXTERN (ggi)

static struct ggi_conf_s {
    char *driver;
    
    ggi_visual_t parentvis;
    ggi_visual_t vis;
    ggi_mode gmode;
    
    /* source image format */
    int srcwidth;
    int srcheight;
    int srcformat;
    int srcdepth;
    int srcbpp;

    /* dirty region */
    struct {
	int x1, y1;
	int x2, y2;
    } flushregion;
    
    /* destination */
    int dstwidth;
    int dstheight;
    
    int async;
    
    int voflags;
} ggi_conf;


static void set_graphtype(uint32_t format, ggi_mode *mode)
{
    switch(format)
    {
	case IMGFMT_RGB4:
	    mode->graphtype = GT_4BIT;
            break;
	case IMGFMT_BGR4:
	    mode->graphtype = GT_4BIT;
            GT_SETSUBSCHEME(mode->graphtype, GT_SUB_HIGHBIT_RIGHT);
            break;
	case IMGFMT_RGB8:
	case IMGFMT_BGR8:
	    mode->graphtype = GT_8BIT;
	    break;
	case IMGFMT_RGB15:
	case IMGFMT_BGR15:
	    mode->graphtype = GT_15BIT;
	    break;
	case IMGFMT_RGB16:
	case IMGFMT_BGR16:
	    mode->graphtype = GT_16BIT;
	    break;
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	    mode->graphtype = GT_24BIT;
	    break;
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
	    mode->graphtype = GT_32BIT;
	    break;
    }

    return;
}

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
    uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
    int i;
    int rc;

    ggi_mode mode = {
	1,			/* frames */
	{ 0, 0 },		/* top, left corner */
	{ width, height },	/* bottem, right corner */
	{ GGI_AUTO, GGI_AUTO },	/* size */
	GT_AUTO,		/* graphtype */
	{ GGI_AUTO, GGI_AUTO }	/* dots per pixel */
    };
    ggi_mode parentmode =
    {
	1,			/* frames */
	{ width, height },	/* visible */
	{ GGI_AUTO, GGI_AUTO },	/* virt */
	{ GGI_AUTO, GGI_AUTO },	/* size */
	GT_AUTO,		/* graphtype */
	{ GGI_AUTO, GGI_AUTO }	/* dots per pixel */
    };

    set_graphtype(format, &parentmode);

#if 0
    printf("[ggi] mode: ");
    ggiPrintMode(&parentmode);
    printf("\n");
#endif

    ggiCheckMode(ggi_conf.parentvis, &parentmode);

    if (ggiSetMode(ggi_conf.parentvis, &parentmode) < 0)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] unable to set display mode\n");
	return(-1);
    }

    if (ggiGetMode(ggi_conf.parentvis, &parentmode) < 0)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] unable to get display mode\n");
	return(-1);
    }

    if ((parentmode.graphtype == GT_INVALID) || (parentmode.graphtype == GT_AUTO))
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] not supported depth/bpp\n");
	return(-1);
    }

    /* calculate top, left corner */
    mode.visible.x = (parentmode.virt.x - width) / 2;
    mode.visible.y = (parentmode.virt.y - height) / 2;

    /* calculate bottom, right corner */
    mode.virt.x = mode.visible.x + width;
    mode.virt.y = mode.visible.y + height;

    ggiCheckMode(ggi_conf.vis, &mode);

    if (ggiSetMode(ggi_conf.vis, &mode) < 0)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] unable to set video mode\n");
	return(-1);
    }

    if (ggiGetMode(ggi_conf.vis, &mode) < 0)
    {
	mp_msg(MSGT_VO, MSGL_ERR, "[ggi] unable to get video mode\n");
	return(-1);
    }


    ggi_conf.gmode = mode;

#if 0
    printf("[ggi] mode: ");
    ggiPrintMode(&mode);
    printf("\n");
#endif

    vo_depthonscreen = GT_DEPTH(parentmode.graphtype);
    vo_screenwidth = parentmode.visible.x;
    vo_screenheight = parentmode.visible.y;
    
    vo_dx = vo_dy = 0;
    vo_dwidth = mode.virt.x;
    vo_dheight = mode.virt.y;
    vo_dbpp = GT_SIZE(parentmode.graphtype);

    ggi_conf.srcwidth = width;
    ggi_conf.srcheight = height;
    ggi_conf.srcformat = format;
    
    ggi_conf.voflags = flags;

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
    {
        mp_msg(MSGT_VO, MSGL_FATAL, "[ggi] Unknown image format: %s\n",
    	    vo_format_name(ggi_conf.srcformat));
	return(-1);
    }

    vo_dwidth = ggi_conf.dstwidth = ggi_conf.gmode.virt.x;
    vo_dheight = ggi_conf.dstheight = ggi_conf.gmode.virt.y;

    ggiSetFlags(ggi_conf.vis, GGIFLAG_ASYNC);

    if (GT_SCHEME(mode.graphtype) == GT_PALETTE)
	ggiSetColorfulPalette(ggi_conf.vis);

    if (ggiGetFlags(ggi_conf.vis) & GGIFLAG_ASYNC)
	ggi_conf.async = 1;

    mp_msg(MSGT_VO, MSGL_INFO, "[ggi] input: %dx%dx%d, output: %dx%dx%d\n",
	ggi_conf.srcwidth, ggi_conf.srcheight, ggi_conf.srcdepth, 
	vo_dwidth, vo_dheight, vo_dbpp);
    mp_msg(MSGT_VO, MSGL_INFO, "[ggi] async mode: %s\n",
	ggi_conf.async ? "yes" : "no");

    ggi_conf.srcbpp = (ggi_conf.srcdepth+7)/8;

    ggi_conf.flushregion.x1 = ggi_conf.flushregion.y1 = 0;
    ggi_conf.flushregion.x2 = ggi_conf.dstwidth;
    ggi_conf.flushregion.y2 = ggi_conf.dstheight;

    return(0);
}

static uint32_t get_image(mp_image_t *mpi)
{
    /* GGI DirectRendering supports (yet) only BGR/RGB modes */
    if (
#if 1
	(IMGFMT_IS_RGB(mpi->imgfmt) &&
	    (IMGFMT_RGB_DEPTH(mpi->imgfmt) != vo_dbpp)) ||
	(IMGFMT_IS_BGR(mpi->imgfmt) &&
	    (IMGFMT_BGR_DEPTH(mpi->imgfmt) != vo_dbpp)) ||
#else
	(mpi->imgfmt != ggi_conf.srcformat) ||
#endif
	((mpi->type != MP_IMGTYPE_STATIC) && (mpi->type != MP_IMGTYPE_TEMP)) ||
	(mpi->flags & MP_IMGFLAG_PLANAR) ||
	(mpi->flags & MP_IMGFLAG_YUV) ||
	(mpi->width != ggi_conf.srcwidth) ||
	(mpi->height != ggi_conf.srcheight)
    )
    {
	return(VO_FALSE);
    }

    mpi->planes[1] = mpi->planes[2] = NULL;
    mpi->stride[1] = mpi->stride[2] = 0;
    
    mpi->planes[0] = NULL;
    mpi->stride[0] = ggi_conf.srcwidth * ggi_conf.srcbpp;
    mpi->flags |= MP_IMGFLAG_DIRECT;

#ifdef GGI_FLIP
    if (ggi_conf.voflags & VOFLAG_FLIPPING)
    {
	mpi->stride[0] = -mpi->stride[0];
    }
#endif

    return(VO_TRUE);
}


static uint32_t draw_frame(uint8_t *src[])
{
    ggiPutBox(ggi_conf.vis, 0, 0, ggi_conf.dstwidth, ggi_conf.dstheight, src[0]);

    ggi_conf.flushregion.x1 = ggi_conf.flushregion.y1 = 0;
    ggi_conf.flushregion.x2 = ggi_conf.dstwidth;
    ggi_conf.flushregion.y2 = ggi_conf.dstheight;

    return(0);
}

static void draw_osd(void)
{
    return;
}

static void flip_page(void)
{
    ggiFlushRegion(ggi_conf.vis, ggi_conf.flushregion.x1, ggi_conf.flushregion.y1,
		ggi_conf.flushregion.x2 - ggi_conf.flushregion.x1,
		ggi_conf.flushregion.y2 - ggi_conf.flushregion.y1);
    ggi_conf.flushregion.x1 = ggi_conf.flushregion.x2 = ggi_conf.dstwidth / 2;
    ggi_conf.flushregion.y1 = ggi_conf.flushregion.y2 = ggi_conf.dstheight / 2;
}

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h,
    int x, int y)
{
    ggiPutBox(ggi_conf.vis, x, y, w, h, src[0]);

    if (x < ggi_conf.flushregion.x1)
	    ggi_conf.flushregion.x1 = x;
    if (y < ggi_conf.flushregion.y1)
	    ggi_conf.flushregion.y1 = y;
    if ((x + w) > ggi_conf.flushregion.x2)
	    ggi_conf.flushregion.x2 = x + w;
    if ((y + h) > ggi_conf.flushregion.y2)
	    ggi_conf.flushregion.y2 = y + h;

    return(1);
}

static uint32_t query_format(uint32_t format)
{
    ggi_mode mode;
    uint32_t vfcap;

    vfcap = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_ACCEPT_STRIDE;

    if ((!vo_depthonscreen || !vo_dbpp) && ggi_conf.parentvis)
    {
	if (ggiGetMode(ggi_conf.parentvis, &mode) == 0)
	{
	    vo_depthonscreen = GT_DEPTH(mode.graphtype);
	    vo_dbpp = GT_SIZE(mode.graphtype);
	}
    }

    if ((IMGFMT_IS_BGR(format) && (IMGFMT_BGR_DEPTH(format) == vo_dbpp)) ||
	(IMGFMT_IS_RGB(format) && (IMGFMT_RGB_DEPTH(format) == vo_dbpp)))
    {
	return vfcap;
    }

    if (IMGFMT_IS_BGR(format) || IMGFMT_IS_RGB(format))
    {
	set_graphtype(format, &mode);

	if (ggiCheckMode(ggi_conf.parentvis, &mode) < 0) {
		return 0;
	} else {
		return vfcap;
	}
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
    
    ggi_conf.parentvis = ggiOpen(ggi_conf.driver);
    if (ggi_conf.parentvis == NULL)
    {
	mp_msg(MSGT_VO, MSGL_FATAL, "[ggi] unable to open '%s' output\n",
	    (ggi_conf.driver == NULL) ? "default" : ggi_conf.driver);
	ggiExit();
	return(-1);
    }

    ggi_conf.vis = ggiOpen("display-sub", ggi_conf.parentvis);
    if (ggi_conf.vis == NULL)
    {
	mp_msg(MSGT_VO, MSGL_FATAL, "[ggi] unable to open the video output\n");
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
    ggiClose(ggi_conf.parentvis);
    ggiExit();
}

static uint32_t control(uint32_t request, void *data, ...)
{
    switch(request)
    {
	case VOCTRL_QUERY_FORMAT:
	    return query_format(*((uint32_t*)data));
	case VOCTRL_GET_IMAGE:
	    return get_image(data);
    }
    return VO_NOTIMPL;
}

/* EVENT handling */
#include "osdep/keycodes.h"
extern void mplayer_put_key(int code);

static void check_events(void)
{
    struct timeval tv = {0, 0};
    ggi_event event;
    ggi_event_mask mask;

    if ((mask = ggiEventPoll(ggi_conf.parentvis, emAll, &tv)))
    if (ggiEventRead(ggi_conf.parentvis, &event, emAll) != 0)
    {
	mp_dbg(MSGT_VO, MSGL_DBG3, "type: %4x, origin: %4x, sym: %4x, label: %4x, button=%4x\n",
	    event.any.origin, event.any.type, event.key.sym, event.key.label, event.key.button);

	switch (event.any.type) {
	case evKeyPress:
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
	    } /* switch */

            break;
	} /* switch */
    } /* if */
    return;
}
