
/* this file contains libvo's common functions, variables used by
   many/all drivers. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
//#include <sys/mman.h>

#include "config.h"
#include "options.h"
#include "talloc.h"
#include "video_out.h"
#include "aspect.h"
#include "geometry.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "osdep/shmem.h"
#ifdef CONFIG_X11
#include "x11_common.h"
#endif

int xinerama_screen = -1;
int xinerama_x;
int xinerama_y;

int vo_nomouse_input = 0;
int vo_grabpointer = 1;
int vo_doublebuffering = 1;
int vo_vsync = 0;
int vo_fs = 0;
int vo_fsmode = 0;
float vo_panscan = 0.0f;
int vo_adapter_num=0;
int vo_refresh_rate=0;
int vo_keepaspect=1;
int vo_rootwin=0;
int vo_border=1;
int WinID = -1;

int vo_pts=0; // for hw decoding
float vo_fps=0;

char *vo_subdevice = NULL;
int vo_directrendering=0;

int vo_colorkey = 0x0000ff00; // default colorkey is green
			      // (0xff000000 means that colorkey has been disabled)

//
// Externally visible list of all vo drivers
//
extern struct vo_driver video_out_mga;
extern struct vo_driver video_out_xmga;
extern struct vo_driver video_out_x11;
extern struct vo_driver video_out_xover;
extern struct vo_driver video_out_xvmc;
extern struct vo_driver video_out_xv;
extern struct vo_driver video_out_gl;
extern struct vo_driver video_out_gl2;
extern struct vo_driver video_out_dga;
extern struct vo_driver video_out_sdl;
extern struct vo_driver video_out_3dfx;
extern struct vo_driver video_out_tdfxfb;
extern struct vo_driver video_out_s3fb;
extern struct vo_driver video_out_wii;
extern struct vo_driver video_out_null;
extern struct vo_driver video_out_zr;
extern struct vo_driver video_out_zr2;
extern struct vo_driver video_out_bl;
extern struct vo_driver video_out_fbdev;
extern struct vo_driver video_out_fbdev2;
extern struct vo_driver video_out_svga;
extern struct vo_driver video_out_png;
extern struct vo_driver video_out_ggi;
extern struct vo_driver video_out_aa;
extern struct vo_driver video_out_caca;
extern struct vo_driver video_out_mpegpes;
extern struct vo_driver video_out_yuv4mpeg;
extern struct vo_driver video_out_direct3d;
extern struct vo_driver video_out_directx;
extern struct vo_driver video_out_dxr2;
extern struct vo_driver video_out_dxr3;
extern struct vo_driver video_out_ivtv;
extern struct vo_driver video_out_v4l2;
extern struct vo_driver video_out_jpeg;
extern struct vo_driver video_out_gif89a;
extern struct vo_driver video_out_vesa;
extern struct vo_driver video_out_directfb;
extern struct vo_driver video_out_dfbmga;
extern struct vo_driver video_out_xvidix;
extern struct vo_driver video_out_winvidix;
extern struct vo_driver video_out_cvidix;
extern struct vo_driver video_out_tdfx_vid;
extern struct vo_driver video_out_xvr100;
extern struct vo_driver video_out_tga;
extern struct vo_driver video_out_macosx;
extern struct vo_driver video_out_quartz;
extern struct vo_driver video_out_pnm;
extern struct vo_driver video_out_md5sum;

const struct vo_driver *video_out_drivers[] =
{
#ifdef CONFIG_XVR100
        &video_out_xvr100,
#endif
#ifdef CONFIG_TDFX_VID
        &video_out_tdfx_vid,
#endif
#ifdef CONFIG_DIRECTX
        &video_out_directx,
#endif
#ifdef CONFIG_DIRECT3D
        &video_out_direct3d,
#endif
#ifdef CONFIG_COREVIDEO
	&video_out_macosx,
#endif
#ifdef CONFIG_QUARTZ
	&video_out_quartz,
#endif
#ifdef CONFIG_XMGA
        &video_out_xmga,
#endif
#ifdef CONFIG_MGA
        &video_out_mga,
#endif
#ifdef CONFIG_TDFXFB
        &video_out_tdfxfb,
#endif
#ifdef CONFIG_S3FB
        &video_out_s3fb,
#endif
#ifdef CONFIG_WII
        &video_out_wii,
#endif
#ifdef CONFIG_3DFX
        &video_out_3dfx,
#endif
#ifdef CONFIG_XV
        &video_out_xv,
#endif
#ifdef CONFIG_X11
        &video_out_x11,
        &video_out_xover,
#endif
#ifdef CONFIG_GL
        &video_out_gl,
        &video_out_gl2,
#endif
#ifdef CONFIG_DGA
        &video_out_dga,
#endif
#ifdef CONFIG_SDL
        &video_out_sdl,
#endif
#ifdef CONFIG_GGI
	&video_out_ggi,
#endif
#ifdef CONFIG_FBDEV
	&video_out_fbdev,
	&video_out_fbdev2,
#endif
#ifdef CONFIG_SVGALIB
	&video_out_svga,
#endif
#ifdef CONFIG_AA
	&video_out_aa,
#endif
#ifdef CONFIG_CACA
	&video_out_caca,
#endif
#ifdef CONFIG_DXR2
	&video_out_dxr2,
#endif
#ifdef CONFIG_DXR3
	&video_out_dxr3,
#endif
#ifdef CONFIG_IVTV
        &video_out_ivtv,
#endif
#ifdef CONFIG_V4L2_DECODER
        &video_out_v4l2,
#endif
#ifdef CONFIG_ZR
	&video_out_zr,
	&video_out_zr2,
#endif
#ifdef CONFIG_BL
	&video_out_bl,
#endif
#ifdef CONFIG_VESA
	&video_out_vesa,
#endif
#ifdef CONFIG_DIRECTFB
	&video_out_directfb,
#endif
#ifdef CONFIG_DFBMGA
        &video_out_dfbmga,
#endif
#ifdef CONFIG_VIDIX
#ifdef CONFIG_X11
	&video_out_xvidix,
#endif
#if defined(__MINGW32__) || defined(__CYGWIN__)
    &video_out_winvidix,
#endif
    &video_out_cvidix,
#endif
        &video_out_null,
	// should not be auto-selected
#ifdef CONFIG_XVMC
        &video_out_xvmc,
#endif
	&video_out_mpegpes,
#ifdef CONFIG_YUV4MPEG
	&video_out_yuv4mpeg,
#endif
#ifdef CONFIG_PNG
	&video_out_png,
#endif
#ifdef CONFIG_JPEG
	&video_out_jpeg,
#endif
#ifdef CONFIG_GIF
	&video_out_gif89a,
#endif
#ifdef CONFIG_TGA
        &video_out_tga,
#endif
#ifdef CONFIG_PNM
    &video_out_pnm,
#endif
#ifdef CONFIG_MD5SUM
    &video_out_md5sum,
#endif
        NULL
};


static int vo_preinit(struct vo *vo, const char *arg)
{
    return vo->driver->preinit(vo, arg);
}

int vo_control(struct vo *vo, uint32_t request, void *data)
{
    return vo->driver->control(vo, request, data);
}

int vo_draw_frame(struct vo *vo, uint8_t *src[])
{
    if (!vo->config_ok)
        return 0;
    return vo->driver->draw_frame(vo, src);
}

int vo_draw_slice(struct vo *vo, uint8_t *src[], int stride[], int w, int h, int x, int y)
{
    return vo->driver->draw_slice(vo, src, stride, w, h, x, y);
}

void vo_draw_osd(struct vo *vo, struct osd_state *osd)
{
    if (!vo->config_ok)
        return;
    vo->driver->draw_osd(vo, osd);
}

void vo_flip_page(struct vo *vo)
{
    if (!vo->config_ok)
        return;
    vo->driver->flip_page(vo);
}

void vo_check_events(struct vo *vo)
{
    if (!vo->config_ok)
        return;
    vo->driver->check_events(vo);
}

void vo_destroy(struct vo *vo)
{
    vo->driver->uninit(vo);
    talloc_free(vo);
}

void list_video_out(void)
{
    int i = 0;
    mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_AvailableVideoOutputDrivers);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_OUTPUTS\n");
    while (video_out_drivers[i]) {
        const vo_info_t *info = video_out_drivers[i++]->info;
        mp_msg(MSGT_GLOBAL, MSGL_INFO,"\t%s\t%s\n", info->short_name, info->name);
    }
    mp_msg(MSGT_GLOBAL, MSGL_INFO,"\n");
}

struct vo *init_best_video_out(struct MPOpts *opts, struct vo_x11_state *x11,
                               struct mp_fifo *key_fifo,
                               struct input_ctx *input_ctx)
{
    char **vo_list = opts->video_driver_list;
    int i;
    struct vo *vo = talloc_ptrtype(NULL, vo);
    struct vo initial_values = {
        .opts = opts,
        .x11 = x11,
        .key_fifo = key_fifo,
        .input_ctx = input_ctx,
    };
    // first try the preferred drivers, with their optional subdevice param:
    if (vo_list && vo_list[0])
        while (vo_list[0][0]) {
            char *name = strdup(vo_list[0]);
            vo_subdevice = strchr(name,':');
            if (!strcmp(name, "pgm"))
                mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_VO_PGM_HasBeenReplaced);
            if (!strcmp(name, "md5"))
                mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_VO_MD5_HasBeenReplaced);
            if (vo_subdevice) {
                vo_subdevice[0] = 0;
                ++vo_subdevice;
            }
            for (i = 0; video_out_drivers[i]; i++) {
                const struct vo_driver *video_driver = video_out_drivers[i];
                const vo_info_t *info = video_driver->info;
                if (!strcmp(info->short_name, name)) {
                    // name matches, try it
                    *vo = initial_values;
                    vo->driver = video_driver;
                    if (!vo_preinit(vo, vo_subdevice)) {
                        free(name);
                        return vo; // success!
                    }
		}
	    }
            // continue...
            free(name);
            ++vo_list;
            if (!(vo_list[0]))
                return NULL; // do NOT fallback to others
	}
    // now try the rest...
    vo_subdevice = NULL;
    for (i = 0; video_out_drivers[i]; i++) {
        const struct vo_driver *video_driver = video_out_drivers[i];
        *vo = initial_values;
        vo->driver = video_driver;
        if (!vo_preinit(vo, vo_subdevice))
            return vo; // success!
    }
    free(vo);
    return NULL;
}

int vo_config(struct vo *vo, uint32_t width, uint32_t height,
                     uint32_t d_width, uint32_t d_height, uint32_t flags,
                     char *title, uint32_t format)
{
    struct MPOpts *opts = vo->opts;
    panscan_init(vo);
    aspect_save_orig(vo, width, height);
    aspect_save_prescale(vo, d_width, d_height);

    if (vo_control(vo, VOCTRL_UPDATE_SCREENINFO, NULL) == VO_TRUE) {
        aspect(vo, &d_width, &d_height, A_NOZOOM);
        vo->dx = (int)(opts->vo_screenwidth - d_width) / 2;
        vo->dy = (int)(opts->vo_screenheight - d_height) / 2;
        geometry(&vo->dx, &vo->dy, &d_width, &d_height,
                 opts->vo_screenwidth, opts->vo_screenheight);
        vo->dx += xinerama_x;
        vo->dy += xinerama_y;
        vo->dwidth = d_width;
        vo->dheight = d_height;
    }

    int ret = vo->driver->config(vo, width, height, d_width, d_height, flags,
                                 title, format);
    vo->config_ok = (ret == 0);
    vo->config_count += vo->config_ok;
    return ret;
}

#if defined(CONFIG_FBDEV)||defined(CONFIG_VESA)
/* Borrowed from vo_fbdev.c
Monitor ranges related functions*/

char *monitor_hfreq_str = NULL;
char *monitor_vfreq_str = NULL;
char *monitor_dotclock_str = NULL;

float range_max(range_t *r)
{
float max = 0;

	for (/* NOTHING */; (r->min != -1 && r->max != -1); r++)
		if (max < r->max) max = r->max;
	return max;
}


int in_range(range_t *r, float f)
{
	for (/* NOTHING */; (r->min != -1 && r->max != -1); r++)
		if (f >= r->min && f <= r->max)
			return 1;
	return 0;
}

range_t *str2range(char *s)
{
	float tmp_min, tmp_max;
	char *endptr = s;	// to start the loop
	range_t *r = NULL;
	int i;

	if (!s)
		return NULL;
	for (i = 0; *endptr; i++) {
		if (*s == ',')
			goto out_err;
		if (!(r = (range_t *) realloc(r, sizeof(*r) * (i + 2)))) {
			mp_msg(MSGT_GLOBAL, MSGL_WARN,"can't realloc 'r'\n");
			return NULL;
		}
		tmp_min = strtod(s, &endptr);
		if (*endptr == 'k' || *endptr == 'K') {
			tmp_min *= 1000.0;
			endptr++;
		} else if (*endptr == 'm' || *endptr == 'M') {
			tmp_min *= 1000000.0;
			endptr++;
		}
		if (*endptr == '-') {
			tmp_max = strtod(endptr + 1, &endptr);
			if (*endptr == 'k' || *endptr == 'K') {
				tmp_max *= 1000.0;
				endptr++;
			} else if (*endptr == 'm' || *endptr == 'M') {
				tmp_max *= 1000000.0;
				endptr++;
			}
			if (*endptr != ',' && *endptr)
				goto out_err;
		} else if (*endptr == ',' || !*endptr) {
			tmp_max = tmp_min;
		} else
			goto out_err;
		r[i].min = tmp_min;
		r[i].max = tmp_max;
		if (r[i].min < 0 || r[i].max < 0)
			goto out_err;
		s = endptr + 1;
	}
	r[i].min = r[i].max = -1;
	return r;
out_err:
	if (r)
		free(r);
	return NULL;
}

/* Borrowed from vo_fbdev.c END */
#endif

