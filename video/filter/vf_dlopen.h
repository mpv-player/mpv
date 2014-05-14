#ifndef VF_DLOPEN_H
#define VF_DLOPEN_H

// when doing a two-way compatible change, don't change these
// when doing a backwards compatible change, bump minor version
// when doing an incompatible change, bump major version and zero minor version
#define VF_DLOPEN_MAJOR_VERSION 1
#define VF_DLOPEN_MINOR_VERSION 0

#if VF_DLOPEN_MINOR_VERSION > 0
# define VF_DLOPEN_CHECK_VERSION(ctx) \
    do { \
        if (ctx->major_version != VF_DLOPEN_MAJOR_VERSION || \
            ctx->minor_version < VF_DLOPEN_MINOR_VERSION) \
            return -1; \
    } while (0)
#else
// workaround for "comparison is always false" warning
# define VF_DLOPEN_CHECK_VERSION(ctx) \
    do { \
        if (ctx->major_version != VF_DLOPEN_MAJOR_VERSION) \
            return -1; \
    } while (0)
#endif

// some common valid pixel format names:
// "gray": 8 bit grayscale
// "yuv420p": planar YUV, U and V planes have an xshift and yshift of 1
// "rgb24": packed RGB24
struct vf_dlopen_formatpair {
    const char *from;
    const char *to; // if NULL, this means identical format as source
};

#define FILTER_MAX_OUTCNT 16

struct vf_dlopen_picdata {
    unsigned int planes;
    unsigned char *plane[4];
    signed int planestride[4];
    unsigned int planewidth[4];
    unsigned int planeheight[4];
    unsigned int planexshift[4];
    unsigned int planeyshift[4];
    double pts;
};

struct vf_dlopen_context {
    unsigned short major_version;
    unsigned short minor_version;

    void *priv;

    struct vf_dlopen_formatpair *format_mapping;
    // {NULL, NULL} terminated list of supported format pairs
    // if NULL, anything goes

    int (*config)(struct vf_dlopen_context *ctx);  // -1 = error
    // image config is put into the in_* members before calling this
    // fills in the out_* members (which are preinitialized for an identity vf_dlopen_context)

    int (*put_image)(struct vf_dlopen_context *ctx);  // returns number of images written, or negative on error
    // before this is called, inpic_* and outpic_* are filled

    void (*uninit)(struct vf_dlopen_context *ctx);

    unsigned int in_width;
    unsigned int in_height;
    unsigned int in_d_width;
    unsigned int in_d_height;
    const char *in_fmt;
    unsigned int out_width;
    unsigned int out_height;
    unsigned int out_d_width;
    unsigned int out_d_height;
    const char *out_fmt;
    unsigned int out_cnt;

    struct vf_dlopen_picdata inpic;
    char *inpic_qscale;
    unsigned int inpic_qscalestride;
    unsigned int inpic_qscaleshift;

    struct vf_dlopen_picdata outpic[FILTER_MAX_OUTCNT];
};
typedef int (vf_dlopen_getcontext_func)(struct vf_dlopen_context *ctx, int argc, const char **argv); // negative on error
vf_dlopen_getcontext_func vf_dlopen_getcontext;

#endif
