#ifndef MPGL_FORMATS_H_
#define MPGL_FORMATS_H_

#include "common.h"

struct gl_format {
    const char *name;           // symbolic name for user interaction/debugging
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
    MPGL_TYPE_UNORM = RA_CTYPE_UNORM,   // normalized integer (fixed point) formats
    MPGL_TYPE_UINT  = RA_CTYPE_UINT,    // full integer formats
    MPGL_TYPE_FLOAT = RA_CTYPE_FLOAT,   // float formats (both full and half)
};

extern const struct gl_format gl_formats[];

int gl_format_feature_flags(GL *gl);
int gl_format_type(const struct gl_format *format);
GLenum gl_integer_format_to_base(GLenum format);
int gl_component_size(GLenum type);
int gl_format_components(GLenum format);
int gl_bytes_per_pixel(GLenum format, GLenum type);

#endif
