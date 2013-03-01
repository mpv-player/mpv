#ifndef MP_GL_LCMS_H
#define MP_GL_LCMS_H

extern const struct m_sub_options mp_icc_conf;

struct mp_icc_opts {
    char *profile;
    char *cache;
    char *size_str;
    int intent;
};

struct lut3d;
struct lut3d *mp_load_icc(struct mp_icc_opts *opts);

#endif
