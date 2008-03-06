#ifndef MPLAYER_VD_H
#define MPLAYER_VD_H

#include "mp_image.h"
#include "mpc_info.h"
#include "libmpdemux/stheader.h"

typedef mp_codec_info_t vd_info_t;

/* interface of video decoder drivers */
typedef struct vd_functions_s
{
	vd_info_t *info;
        int (*init)(sh_video_t *sh);
        void (*uninit)(sh_video_t *sh);
        int (*control)(sh_video_t *sh,int cmd,void* arg, ...);
        mp_image_t* (*decode)(sh_video_t *sh,void* data,int len,int flags);
} vd_functions_t;

// NULL terminated array of all drivers
extern vd_functions_t* mpcodecs_vd_drivers[];

extern int vd_use_slices;

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
