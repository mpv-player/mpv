#ifndef MP_HWDEC_H_
#define MP_HWDEC_H_

#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>

#include "options/m_option.h"

struct mp_image_pool;

struct mp_conversion_filter {
    // Name of the conversion filter.
    const char *name;

    // Arguments for the conversion filter.
    char **args;
};

struct mp_hwdec_ctx {
    const char *driver_name; // NULL if unknown/not loaded

    // libavutil-wrapped context, if available.
    struct AVBufferRef *av_device_ref; // AVHWDeviceContext*

    // List of allowed IMGFMT_s, terminated with 0.
    // If NULL, all software formats are considered to be supported.
    const int *supported_formats;
    // HW format used by the hwdec
    int hw_imgfmt;

    // List of support software formats when doing hwuploads.
    // If NULL, all possible hwuploads are assumed to be supported.
    const int *supported_hwupload_formats;

    // Getter for conversion filter description, or NULL.
    // This will be used for hardware conversion of frame formats.
    // If available the talloc allocated mp_conversion_filter is returned,
    // Caller is responsible to free the allocation.
    struct mp_conversion_filter *(*get_conversion_filter)(int imgfmt);

    // The libavutil hwconfig to be used when querying constraints for the
    // conversion filter. Can be NULL if no special config is required.
    void *conversion_config;
};

// Used to communicate hardware decoder device handles from VO to video decoder.
struct mp_hwdec_devices;

struct mp_hwdec_devices *hwdec_devices_create(void);
void hwdec_devices_destroy(struct mp_hwdec_devices *devs);

struct mp_hwdec_ctx *hwdec_devices_get_by_imgfmt_and_type(struct mp_hwdec_devices *devs,
                                                          int hw_imgfmt,
                                                          enum AVHWDeviceType device_type);

// For code which still strictly assumes there is 1 (or none) device.
struct mp_hwdec_ctx *hwdec_devices_get_first(struct mp_hwdec_devices *devs);

// Return the n-th device. NULL if none.
struct mp_hwdec_ctx *hwdec_devices_get_n(struct mp_hwdec_devices *devs, int n);

// Add this to the list of internal devices. Adding the same pointer twice must
// be avoided.
void hwdec_devices_add(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx);

// Remove this from the list of internal devices. Idempotent/ignores entries
// not added yet. This is not thread-safe.
void hwdec_devices_remove(struct mp_hwdec_devices *devs, struct mp_hwdec_ctx *ctx);

struct hwdec_imgfmt_request {
    int imgfmt;
    bool probing;
};

// Can be used to enable lazy loading of an API with hwdec_devices_request().
// If used at all, this must be set/unset during initialization/uninitialization,
// as concurrent use with hwdec_devices_request() is a race condition.
void hwdec_devices_set_loader(struct mp_hwdec_devices *devs,
    void (*load_api)(void *ctx, struct hwdec_imgfmt_request *params),
    void *load_api_ctx);

// Cause VO to lazily load all devices for a specified img format, and will
// block until this is done (even if not available). Pass IMGFMT_NONE to load
// all available devices.
void hwdec_devices_request_for_img_fmt(struct mp_hwdec_devices *devs,
                                       struct hwdec_imgfmt_request *params);

// Return "," concatenated list (for introspection/debugging). Use talloc_free().
char *hwdec_devices_get_names(struct mp_hwdec_devices *devs);

struct mp_image;
struct mpv_global;

struct hwcontext_create_dev_params {
    bool probing;   // if true, don't log errors if unavailable
};

// Per AV_HWDEVICE_TYPE_* functions, queryable via hwdec_get_hwcontext_fns().
// All entries are strictly optional.
struct hwcontext_fns {
    int av_hwdevice_type;
    // Fill in special format-specific requirements.
    void (*refine_hwframes)(struct AVBufferRef *hw_frames_ctx);
    // Returns a AVHWDeviceContext*. Used for copy hwdecs.
    struct AVBufferRef *(*create_dev)(struct mpv_global *global,
                                      struct mp_log *log,
                                      struct hwcontext_create_dev_params *params);
    // Return whether this is using some sort of sub-optimal emulation layer.
    bool (*is_emulated)(struct AVBufferRef *hw_device_ctx);
};

// The parameter is of type enum AVHWDeviceType (as in int to avoid extensive
// recursive includes). May return NULL for unknown device types.
const struct hwcontext_fns *hwdec_get_hwcontext_fns(int av_hwdevice_type);

extern const struct hwcontext_fns hwcontext_fns_cuda;
extern const struct hwcontext_fns hwcontext_fns_d3d11;
extern const struct hwcontext_fns hwcontext_fns_drmprime;
extern const struct hwcontext_fns hwcontext_fns_dxva2;
extern const struct hwcontext_fns hwcontext_fns_vaapi;
extern const struct hwcontext_fns hwcontext_fns_vdpau;

#endif
