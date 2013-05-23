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

#ifndef MPLAYER_VF_H
#define MPLAYER_VF_H

#include <stdbool.h>

#include "video/mp_image.h"
#include "core/mp_common.h"

#include "video/vfcap.h"

struct MPOpts;
struct vf_instance;
struct vf_priv_s;

typedef struct vf_info {
    const char *info;
    const char *name;
    const char *author;
    const char *comment;
    int (*vf_open)(struct vf_instance *vf, char *args);
    // Ptr to a struct dscribing the options
    const void *opts;
} vf_info_t;

struct vf_format {
    int configured;
    int w, h, dw, dh, flags, fmt;
};

typedef struct vf_instance {
    const vf_info_t *info;
    // funcs:
    int (*config)(struct vf_instance *vf,
                  int width, int height, int d_width, int d_height,
                  unsigned int flags, unsigned int outfmt);
    int (*control)(struct vf_instance *vf, int request, void *data);
    int (*query_format)(struct vf_instance *vf, unsigned int fmt);

    // Filter mpi and return the result. The input mpi reference is owned by
    // the filter, the returned reference is owned by the caller.
    // Return NULL if the output frame is skipped.
    struct mp_image *(*filter)(struct vf_instance *vf, struct mp_image *mpi);

    // Like filter(), but can return an error code ( >= 0 means success). This
    // callback is also more practical when the filter can return multiple
    // output images. Use vf_add_output_frame() to queue output frames.
    int (*filter_ext)(struct vf_instance *vf, struct mp_image *mpi);

    void (*uninit)(struct vf_instance *vf);

    char *label;

    // data:
    struct vf_format fmt_in, fmt_out;
    struct vf_instance *next;

    struct mp_image_pool *out_pool;
    struct vf_priv_s *priv;
    struct MPOpts *opts;

    struct mp_image **out_queued;
    int num_out_queued;
} vf_instance_t;

typedef struct vf_seteq {
    const char *item;
    int value;
} vf_equalizer_t;

#define VFCTRL_SEEK_RESET 1 // reset on picture and PTS discontinuities
#define VFCTRL_QUERY_MAX_PP_LEVEL 4 // query max postprocessing level (if any)
#define VFCTRL_SET_PP_LEVEL 5       // set postprocessing level
#define VFCTRL_SET_EQUALIZER 6 // set color options (brightness,contrast etc)
#define VFCTRL_GET_EQUALIZER 8 // get color options (brightness,contrast etc)
#define VFCTRL_HWDEC_DECODER_RENDER 9 // vdpau hw decoding
#define VFCTRL_HWDEC_ALLOC_SURFACE 10 // vdpau hw decoding
#define VFCTRL_SCREENSHOT      14  // Take screenshot, arg is voctrl_screenshot_args
#define VFCTRL_INIT_OSD        15  // Filter OSD renderer present?
#define VFCTRL_SET_DEINTERLACE 18  // Set deinterlacing status
#define VFCTRL_GET_DEINTERLACE 19  // Get deinterlacing status
/* Hack to make the OSD state object available to vf_sub which
 * access OSD/subtitle state outside of normal OSD draw time. */
#define VFCTRL_SET_OSD_OBJ 20
#define VFCTRL_SET_YUV_COLORSPACE 22 // arg is struct mp_csp_details*
#define VFCTRL_GET_YUV_COLORSPACE 23 // arg is struct mp_csp_details*


struct mp_image *vf_alloc_out_image(struct vf_instance *vf);
void vf_make_out_image_writeable(struct vf_instance *vf, struct mp_image *img);
void vf_add_output_frame(struct vf_instance *vf, struct mp_image *img);

int vf_filter_frame(struct vf_instance *vf, struct mp_image *img);
struct mp_image *vf_chain_output_queued_frame(struct vf_instance *vf);
void vf_chain_seek_reset(struct vf_instance *vf);

vf_instance_t *vf_open_plugin(struct MPOpts *opts,
        const vf_info_t * const *filter_list, vf_instance_t *next,
        const char *name, char **args);
struct vf_instance *vf_open_plugin_noerr(struct MPOpts *opts,
        const vf_info_t *const *filter_list, vf_instance_t *next,
        const char *name, char **args, int *retcode);
vf_instance_t *vf_open_filter(struct MPOpts *opts, vf_instance_t *next,
                              const char *name, char **args);
vf_instance_t *vf_add_before_vo(vf_instance_t **vf, char *name, char **args);

unsigned int vf_match_csp(vf_instance_t **vfp, const unsigned int *list,
                          unsigned int preferred);

// default wrappers:
int vf_next_config(struct vf_instance *vf,
                   int width, int height, int d_width, int d_height,
                   unsigned int flags, unsigned int outfmt);
int vf_next_control(struct vf_instance *vf, int request, void *data);
int vf_next_query_format(struct vf_instance *vf, unsigned int fmt);

struct m_obj_settings;
vf_instance_t *append_filters(vf_instance_t *last,
                              struct m_obj_settings *vf_settings);

vf_instance_t *vf_find_by_label(vf_instance_t *chain, const char *label);

void vf_uninit_filter(vf_instance_t *vf);
void vf_uninit_filter_chain(vf_instance_t *vf);

int vf_config_wrapper(struct vf_instance *vf,
                      int width, int height, int d_width, int d_height,
                      unsigned int flags, unsigned int outfmt);
void vf_print_filter_chain(int msglevel, struct vf_instance *vf);

void vf_rescale_dsize(int *d_width, int *d_height, int old_w, int old_h,
                      int new_w, int new_h);

static inline int norm_qscale(int qscale, int type)
{
    switch (type) {
    case 0: // MPEG-1
        return qscale;
    case 1: // MPEG-2
        return qscale >> 1;
    case 2: // H264
        return qscale >> 2;
    case 3: // VP56
        return (63 - qscale + 2) >> 2;
    }
    return qscale;
}

struct vf_detc_pts_buf {
    double inpts_prev, outpts_prev;
    double lastdelta;
};
void vf_detc_init_pts_buf(struct vf_detc_pts_buf *p);
/* Adjust pts when detelecining.
 * skip_frame: do not render this frame
 * reset_pattern: set to 1 if the telecine pattern has reset due to scene cut
 */
double vf_detc_adjust_pts(struct vf_detc_pts_buf *p, double pts,
                          bool reset_pattern, bool skip_frame);
double vf_softpulldown_adjust_pts(struct vf_detc_pts_buf *p, double pts,
                                  bool reset_pattern, bool skip_frame,
                                  int last_frame_duration);

#endif /* MPLAYER_VF_H */
