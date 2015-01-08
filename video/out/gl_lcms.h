#ifndef MP_GL_LCMS_H
#define MP_GL_LCMS_H

#include <stddef.h>
#include <stdbool.h>
#include "misc/bstr.h"

extern const struct m_sub_options mp_icc_conf;

struct mp_icc_opts {
    char *profile;
    int profile_auto;
    char *cache;
    char *size_str;
    int intent;
};

struct lut3d;
struct mp_log;
struct mpv_global;
struct gl_lcms;

struct gl_lcms *gl_lcms_init(void *talloc_ctx, struct mp_log *log,
                             struct mpv_global *global);
void gl_lcms_set_options(struct gl_lcms *p, struct mp_icc_opts *opts);
void gl_lcms_set_memory_profile(struct gl_lcms *p, bstr *profile);
bool gl_lcms_get_lut3d(struct gl_lcms *p, struct lut3d **);
bool gl_lcms_has_changed(struct gl_lcms *p);

#endif
