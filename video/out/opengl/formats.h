#ifndef MPGL_FORMATS_H_
#define MPGL_FORMATS_H_

#include "common.h"

struct gl_format {
    GLint internal_format;      // glTexImage argument
    GLenum format;              // glTexImage argument
    GLenum type;                // e.g. GL_UNSIGNED_SHORT
    int flags;                  // F_* flags
};

enum {
    // --- gl_format.flags

    // Version flags. If at least 1 flag matches, the format entry is considered
    // supported on the current GL context.
    F_GL2       = 1 << 0, // GL2.1-only
    F_GL3       = 1 << 1, // GL3.0 or later
    F_ES2       = 1 << 2, // ES2-only
    F_ES3       = 1 << 3, // ES3.0 or later
    F_ES32      = 1 << 4, // ES3.2 or later
    F_EXT16     = 1 << 5, // ES with GL_EXT_texture_norm16
    F_EXTF16    = 1 << 6, // GL_EXT_color_buffer_half_float
    F_GL2F      = 1 << 7, // GL2.1-only with texture_rg + texture_float + FBOs
    F_APPL      = 1 << 8, // GL_APPLE_rgb_422

    // Feature flags. They are additional and signal presence of features.
    F_CR        = 1 << 16, // color-renderable
    F_TF        = 1 << 17, // texture-filterable with GL_LINEAR
    F_CF        = F_CR | F_TF,
    F_F16       = 1 << 18, // uses half-floats (16 bit) internally, even though
                           // the format is still GL_FLOAT (32 bit)

    // --- Other constants.
    MPGL_TYPE_UNORM = 1,    // normalized integer (fixed point) formats
    MPGL_TYPE_UINT  = 2,    // full integer formats
    MPGL_TYPE_FLOAT = 3,    // float formats (both full and half)
};

int gl_format_feature_flags(GL *gl);
const struct gl_format *gl_find_internal_format(GL *gl, GLint internal_format);
const struct gl_format *gl_find_format(GL *gl, int type, int flags,
                                       int bytes_per_component, int n_components);
const struct gl_format *gl_find_unorm_format(GL *gl, int bytes_per_component,
                                             int n_components);
const struct gl_format *gl_find_uint_format(GL *gl, int bytes_per_component,
                                            int n_components);
const struct gl_format *gl_find_float16_format(GL *gl, int n_components);
int gl_format_type(const struct gl_format *format);
bool gl_format_is_regular(const struct gl_format *format);
GLenum gl_integer_format_to_base(GLenum format);
bool gl_is_integer_format(GLenum format);
int gl_component_size(GLenum type);
int gl_format_components(GLenum format);
int gl_bytes_per_pixel(GLenum format, GLenum type);

struct gl_imgfmt_desc {
    int num_planes;
    const struct gl_format *planes[4];
    // Chroma pixel size (1x1 is 4:4:4)
    uint8_t chroma_w, chroma_h;
    // Component storage size in bits (possibly padded). For formats with
    // different sizes per component, this is arbitrary. For padded formats
    // like P010 or YUV420P10, padding is included.
    int component_bits;
    // Like mp_regular_imgfmt.component_pad.
    int component_pad;
    // For each texture and each texture output (rgba order) describe what
    // component it returns.
    // The values are like the values in mp_regular_imgfmt_plane.components[].
    // Access as components[plane_nr][component_index]. Set unused items to 0.
    // This pretends GL_RG is used instead of GL_LUMINANCE_ALPHA. The renderer
    // fixes this later.
    uint8_t components[4][4];
};

bool gl_get_imgfmt_desc(GL *gl, int imgfmt, struct gl_imgfmt_desc *out);

#endif
