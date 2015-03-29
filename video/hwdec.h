#ifndef MP_HWDEC_H_
#define MP_HWDEC_H_

struct mp_image_pool;

// keep in sync with --hwdec option
enum hwdec_type {
    HWDEC_AUTO = -1,
    HWDEC_NONE = 0,
    HWDEC_VDPAU = 1,
    HWDEC_VDA = 2,
    HWDEC_VAAPI = 4,
    HWDEC_VAAPI_COPY = 5,
    HWDEC_DXVA2_COPY = 6,
    HWDEC_RPI = 7,
};

struct mp_hwdec_ctx {
    enum hwdec_type type;

    void *priv; // for free use by hwdec implementation

    // API-specific, not needed by all backends.
    struct mp_vdpau_ctx *vdpau_ctx;
    struct mp_vaapi_ctx *vaapi_ctx;

    // Optional.
    // Allocates a software image from the pool, downloads the hw image from
    // mpi, and returns it.
    // pool can be NULL (then just use straight allocation).
    // Return NULL on error or if mpi has the wrong format.
    struct mp_image *(*download_image)(struct mp_hwdec_ctx *ctx,
                                       struct mp_image *mpi,
                                       struct mp_image_pool *swpool);
};

// Used to communicate hardware decoder API handles from VO to video decoder.
// The VO can set the context pointer for supported APIs.
struct mp_hwdec_info {
    // (Since currently only 1 hwdec API is loaded at a time, this pointer
    // simply maps to the loaded one.)
    struct mp_hwdec_ctx *hwctx;

    // Can be used to lazily load a requested API.
    // api_name is e.g. "vdpau" (like the fields above, without "_ctx")
    // Can be NULL, is idempotent, caller checks hwctx fields for success/access.
    // Due to threading, the callback is the only code that is allowed to
    // change fields in this struct after initialization.
    void (*load_api)(struct mp_hwdec_info *info, const char *api_name);
    void *load_api_ctx;
};

// Trivial helper to call info->load_api().
// Implemented in vd_lavc.c.
void hwdec_request_api(struct mp_hwdec_info *info, const char *api_name);

#endif
