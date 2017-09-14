#ifndef MP_GL_LCMS_H
#define MP_GL_LCMS_H

#include <stddef.h>
#include <stdbool.h>
#include "misc/bstr.h"
#include "video/csputils.h"
#include <libavutil/buffer.h>

extern const struct m_sub_options mp_icc_conf;

struct mp_icc_opts {
    int use_embedded;
    char *profile;
    int profile_auto;
    char *cache_dir;
    char *size_str;
    int intent;
    int contrast;
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
                       enum mp_csp_prim prim, enum mp_csp_trc trc,
                       struct AVBufferRef *vid_profile);
bool gl_lcms_has_changed(struct gl_lcms *p, enum mp_csp_prim prim,
                         enum mp_csp_trc trc, struct AVBufferRef *vid_profile);

#endif
