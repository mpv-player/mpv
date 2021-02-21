#ifndef MPGL_HWDEC_H_
#define MPGL_HWDEC_H_

#include "video/mp_image.h"
#include "ra.h"
#include "video/hwdec.h"

struct ra_hwdec {
    const struct ra_hwdec_driver *driver;
    struct mp_log *log;
    struct mpv_global *global;
    struct ra *ra;
    struct mp_hwdec_devices *devs;
    // GLSL extensions required to sample textures from this.
    const char **glsl_extensions;
    // For free use by hwdec driver
    void *priv;
    // For working around the vdpau vs. vaapi mess.
    bool probing;
    // Used in overlay mode only.
    float overlay_colorkey[4];
};

struct ra_hwdec_mapper {
    const struct ra_hwdec_mapper_driver *driver;
    struct mp_log *log;
    struct ra *ra;
    void *priv;
    struct ra_hwdec *owner;
    // Input frame parameters. (Set before init(), immutable.)
    struct mp_image_params src_params;
    // Output frame parameters (represents the format the textures return). Must
    // be set by init(), immutable afterwards,
    struct mp_image_params dst_params;

    // The currently mapped source image (or the image about to be mapped in
    // ->map()). NULL if unmapped. The mapper can also clear this reference if
    // the mapped textures contain a full copy.
    struct mp_image *src;

    // The mapped textures and metadata about them. These fields change if a
    // new frame is mapped (or unmapped), but otherwise remain constant.
    // The common code won't mess with these, so you can e.g. set them in the
    // .init() callback.
    struct ra_tex *tex[4];
};

// This can be used to map frames of a specific hw format as GL textures.
struct ra_hwdec_mapper_driver {
    // Used to create ra_hwdec_mapper.priv.
    size_t priv_size;

    // Init the mapper implementation. At this point, the field src_params,
    // fns, devs, priv are initialized.
    int (*init)(struct ra_hwdec_mapper *mapper);
    // Destroy the mapper. unmap is called before this.
    void (*uninit)(struct ra_hwdec_mapper *mapper);

    // Map mapper->src as texture, and set mapper->frame to textures using it.
    // It is expected that that the textures remain valid until the next unmap
    // or uninit call.
    // The function is allowed to unref mapper->src if it's not needed (i.e.
    // this function creates a copy).
    // The underlying format can change, so you might need to do some form
    // of change detection. You also must reject unsupported formats with an
    // error.
    // On error, returns negative value on error and remains unmapped.
    int (*map)(struct ra_hwdec_mapper *mapper);
    // Unmap the frame. Does nothing if already unmapped. Optional.
    void (*unmap)(struct ra_hwdec_mapper *mapper);
};

struct ra_hwdec_driver {
    // Name of the interop backend. This is used for informational purposes and
    // for use with debugging options.
    const char *name;
    // Used to create ra_hwdec.priv.
    size_t priv_size;
    // One of the hardware surface IMGFMT_ that must be passed to map_image later.
    // Terminated with a 0 entry. (Extend the array size as needed.)
    const int imgfmts[3];

    // Create the hwdec device. It must add it to hw->devs, if applicable.
    int (*init)(struct ra_hwdec *hw);
    void (*uninit)(struct ra_hwdec *hw);

    // This will be used to create a ra_hwdec_mapper from ra_hwdec.
    const struct ra_hwdec_mapper_driver *mapper;

    // The following function provides an alternative API. Each ra_hwdec_driver
    // must have either provide a mapper or overlay_frame (not both or none), and
    // if overlay_frame is set, it operates in overlay mode. In this mode,
    // OSD etc. is rendered via OpenGL, but the video is rendered as a separate
    // layer below it.
    // Non-overlay mode is strictly preferred, so try not to use overlay mode.
    // Set the given frame as overlay, replacing the previous one. This can also
    // just change the position of the overlay.
    // hw_image==src==dst==NULL is passed to clear the overlay.
    int (*overlay_frame)(struct ra_hwdec *hw, struct mp_image *hw_image,
                         struct mp_rect *src, struct mp_rect *dst, bool newframe);
};

extern const struct ra_hwdec_driver *const ra_hwdec_drivers[];

struct ra_hwdec *ra_hwdec_load_driver(struct ra *ra, struct mp_log *log,
                                      struct mpv_global *global,
                                      struct mp_hwdec_devices *devs,
                                      const struct ra_hwdec_driver *drv,
                                      bool is_auto);

int ra_hwdec_validate_opt(struct mp_log *log, const m_option_t *opt,
                          struct bstr name, const char **value);

void ra_hwdec_uninit(struct ra_hwdec *hwdec);

bool ra_hwdec_test_format(struct ra_hwdec *hwdec, int imgfmt);

struct ra_hwdec_mapper *ra_hwdec_mapper_create(struct ra_hwdec *hwdec,
                                               struct mp_image_params *params);
void ra_hwdec_mapper_free(struct ra_hwdec_mapper **mapper);
void ra_hwdec_mapper_unmap(struct ra_hwdec_mapper *mapper);
int ra_hwdec_mapper_map(struct ra_hwdec_mapper *mapper, struct mp_image *img);

#endif
