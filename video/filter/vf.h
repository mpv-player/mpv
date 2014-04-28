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
#include "common/common.h"

#include "video/vfcap.h"

struct MPOpts;
struct mpv_global;
struct vf_instance;
struct vf_priv_s;
struct m_obj_settings;

typedef struct vf_info {
    const char *description;
    const char *name;
    int (*open)(struct vf_instance *vf);
    int priv_size;
    const void *priv_defaults;
    const struct m_option *options;
    void (*print_help)(struct mp_log *log);
} vf_info_t;

typedef struct vf_instance {
    const vf_info_t *info;

    // Initialize the filter. The filter must set *out to the same image
    // params as the images the filter functions will return for the given
    // *in format.
    // Note that by default, only formats reported as supported by query_format
    // will be allowed for *in.
    // Returns >= 0 on success, < 0 on error.
    int (*reconfig)(struct vf_instance *vf, struct mp_image_params *in,
                    struct mp_image_params *out);

    // Legacy variant, use reconfig instead.
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
    // Warning: this is called with mpi==NULL if there is no more input at
    //          all (i.e. the video has reached end of file condition). This
    //          can be used to output delayed or otherwise remaining images.
    int (*filter_ext)(struct vf_instance *vf, struct mp_image *mpi);

    void (*uninit)(struct vf_instance *vf);

    char *label;
    bool autoinserted;

    struct mp_image_params fmt_in, fmt_out;

    struct mp_image_pool *out_pool;
    struct vf_priv_s *priv;
    struct mp_log *log;
    struct mp_hwdec_info *hwdec;

    struct mp_image **out_queued;
    int num_out_queued;

    // Caches valid output formats.
    uint8_t last_outfmts[IMGFMT_END - IMGFMT_START];

    struct vf_instance *next;
} vf_instance_t;

// A chain of video filters
struct vf_chain {
    int initialized; // 0: no, 1: yes, -1: attempted to, but failed

    struct vf_instance *first, *last;

    struct mp_image_params output_params;
    uint8_t allowed_output_formats[IMGFMT_END - IMGFMT_START];

    struct mp_log *log;
    struct MPOpts *opts;
    struct mpv_global *global;
    struct mp_hwdec_info *hwdec;
};

typedef struct vf_seteq {
    const char *item;
    int value;
} vf_equalizer_t;

enum vf_ctrl {
    VFCTRL_SEEK_RESET = 1,   // reset on picture and PTS discontinuities
    VFCTRL_SET_EQUALIZER,    // set color options (brightness,contrast etc)
    VFCTRL_GET_EQUALIZER,    // get color options (brightness,contrast etc)
    VFCTRL_SCREENSHOT,       // Take screenshot, arg is voctrl_screenshot_args
    VFCTRL_INIT_OSD,         // Filter OSD renderer present?
    VFCTRL_SET_DEINTERLACE,  // Set deinterlacing status
    VFCTRL_GET_DEINTERLACE,  // Get deinterlacing status
    VFCTRL_GET_METADATA,     // Get frame metadata from lavfi filters (e.g., cropdetect)
    /* Hack to make the OSD state object available to vf_sub which
     * access OSD/subtitle state outside of normal OSD draw time. */
    VFCTRL_SET_OSD_OBJ,
};

struct vf_chain *vf_new(struct mpv_global *global);
void vf_destroy(struct vf_chain *c);
int vf_reconfig(struct vf_chain *c, const struct mp_image_params *params);
int vf_control_any(struct vf_chain *c, int cmd, void *arg);
int vf_control_by_label(struct vf_chain *c, int cmd, void *arg, bstr label);
int vf_filter_frame(struct vf_chain *c, struct mp_image *img);
struct mp_image *vf_output_queued_frame(struct vf_chain *c, bool eof);
void vf_seek_reset(struct vf_chain *c);
struct vf_instance *vf_append_filter(struct vf_chain *c, const char *name,
                                     char **args);
void vf_remove_filter(struct vf_chain *c, struct vf_instance *vf);
int vf_append_filter_list(struct vf_chain *c, struct m_obj_settings *list);
struct vf_instance *vf_find_by_label(struct vf_chain *c, const char *label);
void vf_print_filter_chain(struct vf_chain *c, int msglevel);

// Filter internal API
struct mp_image *vf_alloc_out_image(struct vf_instance *vf);
void vf_make_out_image_writeable(struct vf_instance *vf, struct mp_image *img);
void vf_add_output_frame(struct vf_instance *vf, struct mp_image *img);

// default wrappers:
int vf_next_config(struct vf_instance *vf,
                   int width, int height, int d_width, int d_height,
                   unsigned int flags, unsigned int outfmt);
int vf_next_query_format(struct vf_instance *vf, unsigned int fmt);


// Helpers

void vf_rescale_dsize(int *d_width, int *d_height, int old_w, int old_h,
                      int new_w, int new_h);
void vf_set_dar(int *d_width, int *d_height, int w, int h, double dar);

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
