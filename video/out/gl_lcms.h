#ifndef MP_GL_LCMS_H
#define MP_GL_LCMS_H

#include <stdbool.h>

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
bool mp_icc_set_profile(struct mp_icc_opts *opts, char *profile);
struct lut3d *mp_load_icc(struct mp_icc_opts *opts, struct mp_log *log,
                          struct mpv_global *global);

#endif
