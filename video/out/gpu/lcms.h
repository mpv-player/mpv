#ifndef MP_GL_LCMS_H
#define MP_GL_LCMS_H

#include <stddef.h>
#include <stdbool.h>
#include "misc/bstr.h"
#include "video/csputils.h"
#include <libavutil/buffer.h>

extern const struct m_sub_options mp_icc_conf;

struct mp_icc_opts {
    bool use_embedded;
    char *profile;
    bool profile_auto;
    bool cache;
    char *cache_dir;
    char *size_str;
    int intent;
    int contrast;
    bool icc_use_luma;
};

struct lut3d {
    uint16_t *data;
    int size[3];
};

struct mp_log;
struct mpv_global;
struct gl_lcms;

struct gl_lcms *gl_lcms_init(void *talloc_ctx, struct mp_log *log,
                             struct mpv_global *global,
                             struct mp_icc_opts *opts);
void gl_lcms_update_options(struct gl_lcms *p);
bool gl_lcms_set_memory_profile(struct gl_lcms *p, bstr profile);
bool gl_lcms_has_profile(struct gl_lcms *p);
bool gl_lcms_get_lut3d(struct gl_lcms *p, struct lut3d **,
                       enum pl_color_primaries prim, enum pl_color_transfer trc,
                       struct AVBufferRef *vid_profile);
bool gl_lcms_has_changed(struct gl_lcms *p, enum pl_color_primaries prim,
                         enum pl_color_transfer trc, struct AVBufferRef *vid_profile);

static inline bool gl_parse_3dlut_size(const char *arg, int *p1, int *p2, int *p3)
{
    if (!strcmp(arg, "auto")) {
        *p1 = *p2 = *p3 = 0;
        return true;
    }
    if (sscanf(arg, "%dx%dx%d", p1, p2, p3) != 3)
        return false;
    for (int n = 0; n < 3; n++) {
        int s = ((int[]) { *p1, *p2, *p3 })[n];
        if (s < 2 || s > 512)
            return false;
    }
    return true;
}

#endif
