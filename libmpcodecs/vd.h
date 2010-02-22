/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_VD_H
#define MPLAYER_VD_H

#include "mp_image.h"
#include "mpc_info.h"
#include "libmpdemux/stheader.h"

typedef mp_codec_info_t vd_info_t;

/* interface of video decoder drivers */
typedef struct vd_functions_s
{
        const vd_info_t *info;
        int (*init)(sh_video_t *sh);
        void (*uninit)(sh_video_t *sh);
        int (*control)(sh_video_t *sh,int cmd,void* arg, ...);
        mp_image_t* (*decode)(sh_video_t *sh,void* data,int len,int flags);
} vd_functions_t;

// NULL terminated array of all drivers
extern const vd_functions_t * const mpcodecs_vd_drivers[];

extern int flip;
extern int fullscreen;
extern int opt_screen_size_x;
extern int opt_screen_size_y;
extern int softzoom;
extern int vd_use_slices;
extern int vidmode;
extern float movie_aspect;
extern float screen_size_xy;

#define VDCTRL_QUERY_FORMAT 3 /* test for availabilty of a format */
#define VDCTRL_QUERY_MAX_PP_LEVEL 4 /* test for postprocessing support (max level) */
#define VDCTRL_SET_PP_LEVEL 5 /* set postprocessing level */
#define VDCTRL_SET_EQUALIZER 6 /* set color options (brightness,contrast etc) */
#define VDCTRL_GET_EQUALIZER 7 /* get color options (brightness,contrast etc) */
#define VDCTRL_RESYNC_STREAM 8 /* seeking */
#define VDCTRL_QUERY_UNSEEN_FRAMES 9 /* current decoder lag */

// callbacks:
int mpcodecs_config_vo(sh_video_t *sh, int w, int h, unsigned int preferred_outfmt);
mp_image_t* mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag, int w, int h);
void mpcodecs_draw_slice(sh_video_t *sh, unsigned char** src, int* stride, int w,int h, int x, int y);

#define VDFLAGS_DROPFRAME 3

#endif /* MPLAYER_VD_H */
