
/* this file contains libvo's common functions, variables used by
   many/all drivers. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
//#include <sys/mman.h>

#include "config.h"
#include "video_out.h"
#include "aspect.h"
#include "geometry.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "osdep/shmem.h"

//int vo_flags=0;

int xinerama_screen = -1;
int xinerama_x;
int xinerama_y;

// currect resolution/bpp on screen:  (should be autodetected by vo_init())
int vo_depthonscreen=0;
int vo_screenwidth=0;
int vo_screenheight=0;

int vo_config_count=0;

// requested resolution/bpp:  (-x -y -bpp options)
int vo_dx=0;
int vo_dy=0;
int vo_dwidth=0;
int vo_dheight=0;
int vo_dbpp=0;

int vo_nomouse_input = 0;
int vo_grabpointer = 1;
int vo_doublebuffering = 1;
int vo_vsync = 0;
int vo_fs = 0;
int vo_fsmode = 0;
float vo_panscan = 0.0f;
int vo_ontop = 0;
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
extern vo_functions_t video_out_mga;
extern vo_functions_t video_out_xmga;
extern vo_functions_t video_out_x11;
extern vo_functions_t video_out_xover;
extern vo_functions_t video_out_xvmc;
extern vo_functions_t video_out_xv;
extern vo_functions_t video_out_gl;
extern vo_functions_t video_out_gl2;
extern vo_functions_t video_out_dga;
extern vo_functions_t video_out_sdl;
extern vo_functions_t video_out_3dfx;
extern vo_functions_t video_out_tdfxfb;
extern vo_functions_t video_out_s3fb;
extern vo_functions_t video_out_wii;
extern vo_functions_t video_out_null;
extern vo_functions_t video_out_zr;
extern vo_functions_t video_out_zr2;
extern vo_functions_t video_out_bl;
extern vo_functions_t video_out_fbdev;
extern vo_functions_t video_out_fbdev2;
extern vo_functions_t video_out_svga;
extern vo_functions_t video_out_png;
extern vo_functions_t video_out_ggi;
extern vo_functions_t video_out_aa;
extern vo_functions_t video_out_caca;
extern vo_functions_t video_out_mpegpes;
extern vo_functions_t video_out_yuv4mpeg;
extern vo_functions_t video_out_direct3d;
extern vo_functions_t video_out_directx;
extern vo_functions_t video_out_dxr2;
extern vo_functions_t video_out_dxr3;
extern vo_functions_t video_out_ivtv;
extern vo_functions_t video_out_v4l2;
extern vo_functions_t video_out_jpeg;
extern vo_functions_t video_out_gif89a;
extern vo_functions_t video_out_vesa;
extern vo_functions_t video_out_directfb;
extern vo_functions_t video_out_dfbmga;
extern vo_functions_t video_out_xvidix;
extern vo_functions_t video_out_winvidix;
extern vo_functions_t video_out_cvidix;
extern vo_functions_t video_out_tdfx_vid;
extern vo_functions_t video_out_xvr100;
extern vo_functions_t video_out_tga;
extern vo_functions_t video_out_macosx;
extern vo_functions_t video_out_quartz;
extern vo_functions_t video_out_pnm;
extern vo_functions_t video_out_md5sum;

const vo_functions_t* const video_out_drivers[] =
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
#if CONFIG_XVMC
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

void list_video_out(void){
      int i=0;
      mp_msg(MSGT_CPLAYER, MSGL_INFO, MSGTR_AvailableVideoOutputDrivers);
      mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_OUTPUTS\n");
      while (video_out_drivers[i]) {
        const vo_info_t *info = video_out_drivers[i++]->info;
        mp_msg(MSGT_GLOBAL, MSGL_INFO,"\t%s\t%s\n", info->short_name, info->name);
      }
      mp_msg(MSGT_GLOBAL, MSGL_INFO,"\n");
}

const vo_functions_t* init_best_video_out(char** vo_list){
    int i;
    // first try the preferred drivers, with their optional subdevice param:
    if(vo_list && vo_list[0])
      while(vo_list[0][0]){
        char* vo=strdup(vo_list[0]);
	vo_subdevice=strchr(vo,':');
	if (!strcmp(vo, "pgm"))
	    mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_VO_PGM_HasBeenReplaced);
	if (!strcmp(vo, "md5"))
	    mp_msg(MSGT_CPLAYER, MSGL_ERR, MSGTR_VO_MD5_HasBeenReplaced);
	if(vo_subdevice){
	    vo_subdevice[0]=0;
	    ++vo_subdevice;
	}
	for(i=0;video_out_drivers[i];i++){
	    const vo_functions_t* video_driver=video_out_drivers[i];
	    const vo_info_t *info = video_driver->info;
	    if(!strcmp(info->short_name,vo)){
		// name matches, try it
		if(!video_driver->preinit(vo_subdevice))
		{
		    free(vo);
		    return video_driver; // success!
		}
	    }
	}
        // continue...
	free(vo);
	++vo_list;
	if(!(vo_list[0])) return NULL; // do NOT fallback to others
      }
    // now try the rest...
    vo_subdevice=NULL;
    for(i=0;video_out_drivers[i];i++){
	const vo_functions_t* video_driver=video_out_drivers[i];
	if(!video_driver->preinit(vo_subdevice))
	    return video_driver; // success!
    }
    return NULL;
}

int config_video_out(const vo_functions_t *vo, uint32_t width, uint32_t height,
                     uint32_t d_width, uint32_t d_height, uint32_t flags,
                     char *title, uint32_t format) {
  panscan_init();
  aspect_save_orig(width,height);
  aspect_save_prescale(d_width,d_height);

  if (vo->control(VOCTRL_UPDATE_SCREENINFO, NULL) == VO_TRUE) {
  aspect(&d_width,&d_height,A_NOZOOM);
  vo_dx = (int)(vo_screenwidth - d_width) / 2;
  vo_dy = (int)(vo_screenheight - d_height) / 2;
  geometry(&vo_dx, &vo_dy, &d_width, &d_height,
           vo_screenwidth, vo_screenheight);
  vo_dx += xinerama_x;
  vo_dy += xinerama_y;
  vo_dwidth = d_width;
  vo_dheight = d_height;
  }

  return vo->config(width, height, d_width, d_height, flags, title, format);
}

/**
 * \brief lookup an integer in a table, table must have 0 as the last key
 * \param key key to search for
 * \result translation corresponding to key or "to" value of last mapping
 *         if not found.
 */
int lookup_keymap_table(const struct keymap *map, int key) {
  while (map->from && map->from != key) map++;
  return map->to;
}

#if defined(CONFIG_FBDEV) || defined(CONFIG_VESA)
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

