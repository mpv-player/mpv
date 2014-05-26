#ifndef MP_HWDEC_H_
#define MP_HWDEC_H_

// Used to communicate hardware decoder API handles from VO to video decoder.
// The VO can set the context pointer for supported APIs.
struct mp_hwdec_info {
    struct mp_vdpau_ctx *vdpau_ctx;
    struct mp_vaapi_ctx *vaapi_ctx;
    // Can be used to lazily load a requested API.
    // api_name is e.g. "vdpau" (like the fields above, without "_ctx")
    // Can be NULL, is idempotent, caller checks _ctx fields for success/access.
    void (*load_api)(struct mp_hwdec_info *info, const char *api_name);
    void *load_api_ctx;
};

// Trivial helper to call info->load_api().
// Implemented in vd_lavc.c.
void hwdec_request_api(struct mp_hwdec_info *info, const char *api_name);

#endif
