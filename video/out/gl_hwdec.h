#ifndef MPGL_HWDEC_H_
#define MPGL_HWDEC_H_

#include "gl_common.h"
#include "video/hwdec.h"

struct mp_hwdec_info;

struct gl_hwdec {
    const struct gl_hwdec_driver *driver;
    struct mp_log *log;
    GL *gl;
    struct mp_hwdec_info *info;
    // For free use by hwdec driver
    void *priv;
    // For working around the vdpau vs. vaapi mess.
    bool reject_emulated;
    // hwdec backends must set this to an IMGFMT_ that has an equivalent
    // internal representation in gl_video.c as the hardware texture.
    // It's used to build the rendering chain, and also as screenshot format.
    int converted_imgfmt;
    // Normally this is GL_TEXTURE_2D, but the hwdec driver can set it to
    // GL_TEXTURE_RECTANGLE.
    GLenum gl_texture_target;
};

struct gl_hwdec_driver {
    // Same name as used by mp_hwdec_info->load_api()
    const char *api_name;
    // The hardware surface IMGFMT_ that must be passed to map_image later.
    int imgfmt;
    // Create the hwdec device. It must fill in hw->info, if applicable.
    // This also must set hw->converted_imgfmt.
    int (*create)(struct gl_hwdec *hw);
    // Prepare for rendering video. (E.g. create textures.)
    // Called on initialization, and every time the video size changes.
    int (*reinit)(struct gl_hwdec *hw, const struct mp_image_params *params);
    // Return textures that contain the given hw_image.
    // Note that the caller keeps a reference to hw_image until unmap_image
    // is called, so the hwdec driver doesn't need to do that.
    int (*map_image)(struct gl_hwdec *hw, struct mp_image *hw_image,
                     GLuint *out_textures);
    // Undo map_image(). The user of map_image() calls this when the textures
    // are not needed anymore.
    void (*unmap_image)(struct gl_hwdec *hw);
    // Return a mp_image downloaded from the GPU (optional)
    struct mp_image *(*download_image)(struct gl_hwdec *hw,
                                       struct mp_image *hw_image);
    void (*destroy)(struct gl_hwdec *hw);
};

struct gl_hwdec *gl_hwdec_load_api(struct mp_log *log, GL *gl,
                                   const char *api_name,
                                   struct mp_hwdec_info *info);

void gl_hwdec_uninit(struct gl_hwdec *hwdec);

#endif
