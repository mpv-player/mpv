#include "common/common.h"
#include "formats.h"

enum {
    // --- GL type aliases (for readability)
    T_U8        = GL_UNSIGNED_BYTE,
    T_U16       = GL_UNSIGNED_SHORT,
    T_FL        = GL_FLOAT,
};

// List of allowed formats, and their usability for bilinear filtering and FBOs.
// This is limited to combinations that are useful for our renderer.
const struct gl_format gl_formats[] = {
    // These are used for desktop GL 3+, and GLES 3+ with GL_EXT_texture_norm16.
    {"r8",      GL_R8,       GL_RED,             T_U8,  F_CF | F_GL3 | F_GL2F | F_ES3},
    {"rg8",     GL_RG8,      GL_RG,              T_U8,  F_CF | F_GL3 | F_GL2F | F_ES3},
    {"rgb8",    GL_RGB8,     GL_RGB,             T_U8,  F_CF | F_GL3 | F_GL2F | F_ES3},
    {"rgba8",   GL_RGBA8,    GL_RGBA,            T_U8,  F_CF | F_GL3 | F_GL2F | F_ES3},
    {"r16",     GL_R16,      GL_RED,             T_U16, F_CF | F_GL3 | F_GL2F | F_EXT16},
    {"rg16",    GL_RG16,     GL_RG,              T_U16, F_CF | F_GL3 | F_GL2F | F_EXT16},
    {"rgb16",   GL_RGB16,    GL_RGB,             T_U16, F_CF | F_GL3 | F_GL2F},
    {"rgba16",  GL_RGBA16,   GL_RGBA,            T_U16, F_CF | F_GL3 | F_GL2F | F_EXT16},

    // Specifically not color-renderable.
    {"rgb16",   GL_RGB16,    GL_RGB,             T_U16, F_TF | F_EXT16},

    // GL2 legacy. Ignores possibly present FBO extensions (no CF flag set).
    {"l8",    GL_LUMINANCE8, GL_LUMINANCE,       T_U8,  F_TF | F_GL2},
    {"la8", GL_LUMINANCE8_ALPHA8, GL_LUMINANCE_ALPHA, T_U8,  F_TF | F_GL2},
    {"rgb8",    GL_RGB8,     GL_RGB,             T_U8,  F_TF | F_GL2},
    {"rgba8",   GL_RGBA8,    GL_RGBA,            T_U8,  F_TF | F_GL2},
    {"l16",  GL_LUMINANCE16, GL_LUMINANCE,       T_U16, F_TF | F_GL2},
    {"la16", GL_LUMINANCE16_ALPHA16, GL_LUMINANCE_ALPHA, T_U16, F_TF | F_GL2},
    {"rgb16",   GL_RGB16,    GL_RGB,             T_U16, F_TF | F_GL2},
    {"rgba16",  GL_RGBA16,   GL_RGBA,            T_U16, F_TF | F_GL2},

    // ES2 legacy
    {"l" ,      GL_LUMINANCE,GL_LUMINANCE,       T_U8,  F_TF | F_ES2},
    {"la",GL_LUMINANCE_ALPHA,GL_LUMINANCE_ALPHA, T_U8,  F_TF | F_ES2},
    {"rgb",     GL_RGB,      GL_RGB,             T_U8,  F_TF | F_ES2},
    {"rgba",    GL_RGBA,     GL_RGBA,            T_U8,  F_TF | F_ES2},

    // Non-normalized integer formats.
    // Follows ES 3.0 as to which are color-renderable.
    {"r8ui",    GL_R8UI,     GL_RED_INTEGER,     T_U8,  F_CR | F_GL3 | F_ES3},
    {"rg8ui",   GL_RG8UI,    GL_RG_INTEGER,      T_U8,  F_CR | F_GL3 | F_ES3},
    {"rgb8ui",  GL_RGB8UI,   GL_RGB_INTEGER,     T_U8,         F_GL3 | F_ES3},
    {"rgba8ui", GL_RGBA8UI,  GL_RGBA_INTEGER,    T_U8,  F_CR | F_GL3 | F_ES3},
    {"r16ui",   GL_R16UI,    GL_RED_INTEGER,     T_U16, F_CR | F_GL3 | F_ES3},
    {"rg16ui",  GL_RG16UI,   GL_RG_INTEGER,      T_U16, F_CR | F_GL3 | F_ES3},
    {"rgb16ui", GL_RGB16UI,  GL_RGB_INTEGER,     T_U16,        F_GL3 | F_ES3},
    {"rgba16ui",GL_RGBA16UI, GL_RGBA_INTEGER,    T_U16, F_CR | F_GL3 | F_ES3},

    // On GL3+ or GL2.1 with GL_ARB_texture_float, floats work fully.
    {"r16f",    GL_R16F,     GL_RED,             T_FL,  F_F16 | F_CF | F_GL3 | F_GL2F},
    {"rg16f",   GL_RG16F,    GL_RG,              T_FL,  F_F16 | F_CF | F_GL3 | F_GL2F},
    {"rgb16f",  GL_RGB16F,   GL_RGB,             T_FL,  F_F16 | F_CF | F_GL3 | F_GL2F},
    {"rgba16f", GL_RGBA16F,  GL_RGBA,            T_FL,  F_F16 | F_CF | F_GL3 | F_GL2F},
    {"r32f",    GL_R32F,     GL_RED,             T_FL,          F_CF | F_GL3 | F_GL2F},
    {"rg32f",   GL_RG32F,    GL_RG,              T_FL,          F_CF | F_GL3 | F_GL2F},
    {"rgb32f",  GL_RGB32F,   GL_RGB,             T_FL,          F_CF | F_GL3 | F_GL2F},
    {"rgba32f", GL_RGBA32F,  GL_RGBA,            T_FL,          F_CF | F_GL3 | F_GL2F},

    // Note: we simply don't support float anything on ES2, despite extensions.
    // We also don't bother with non-filterable float formats, and we ignore
    // 32 bit float formats that are not blendable when rendering to them.

    // On ES3.2+, both 16 bit floats work fully (except 3-component formats).
    // F_EXTF16 implies extensions that also enable 16 bit floats fully.
    {"r16f",    GL_R16F,     GL_RED,             T_FL,  F_F16 | F_CF | F_ES32 | F_EXTF16},
    {"rg16f",   GL_RG16F,    GL_RG,              T_FL,  F_F16 | F_CF | F_ES32 | F_EXTF16},
    {"rgb16f",  GL_RGB16F,   GL_RGB,             T_FL,  F_F16 | F_TF | F_ES32 | F_EXTF16},
    {"rgba16f", GL_RGBA16F,  GL_RGBA,            T_FL,  F_F16 | F_CF | F_ES32 | F_EXTF16},

    // On ES3.0+, 16 bit floats are texture-filterable.
    // Don't bother with 32 bit floats; they exist but are neither CR nor TF.
    {"r16f",    GL_R16F,     GL_RED,             T_FL,  F_F16 | F_TF | F_ES3},
    {"rg16f",   GL_RG16F,    GL_RG,              T_FL,  F_F16 | F_TF | F_ES3},
    {"rgb16f",  GL_RGB16F,   GL_RGB,             T_FL,  F_F16 | F_TF | F_ES3},
    {"rgba16f", GL_RGBA16F,  GL_RGBA,            T_FL,  F_F16 | F_TF | F_ES3},

    // These might be useful as FBO formats.
    {"rgb10_a2",GL_RGB10_A2, GL_RGBA,
     GL_UNSIGNED_INT_2_10_10_10_REV,                    F_CF | F_GL3 | F_ES3},
    {"rgba12",  GL_RGBA12,   GL_RGBA,            T_U16, F_CF | F_GL2 | F_GL3},
    {"rgb10",   GL_RGB10,    GL_RGB,             T_U16, F_CF | F_GL2 | F_GL3},

    // Special formats.
    {"rgb565",  GL_RGB8,     GL_RGB,
     GL_UNSIGNED_SHORT_5_6_5,                           F_TF | F_GL2 | F_GL3},
    {"appleyp", GL_RGB,      GL_RGB_422_APPLE,
     GL_UNSIGNED_SHORT_8_8_APPLE,                       F_TF | F_APPL},

    {0}
};

// Return an or-ed combination of all F_ flags that apply.
int gl_format_feature_flags(GL *gl)
{
    return (gl->version == 210 ? F_GL2 : 0)
         | (gl->version >= 300 ? F_GL3 : 0)
         | (gl->es == 200 ? F_ES2 : 0)
         | (gl->es >= 300 ? F_ES3 : 0)
         | (gl->es >= 320 ? F_ES32 : 0)
         | (gl->mpgl_caps & MPGL_CAP_EXT16 ? F_EXT16 : 0)
         | ((gl->es >= 300 &&
            (gl->mpgl_caps & MPGL_CAP_EXT_CR_HFLOAT)) ? F_EXTF16 : 0)
         | ((gl->version == 210 &&
            (gl->mpgl_caps & MPGL_CAP_ARB_FLOAT) &&
            (gl->mpgl_caps & MPGL_CAP_TEX_RG) &&
            (gl->mpgl_caps & MPGL_CAP_FB)) ? F_GL2F : 0)
         | (gl->mpgl_caps & MPGL_CAP_APPLE_RGB_422 ? F_APPL : 0);
}

// Return the entry for the given internal format. Return NULL if unsupported.
const struct gl_format *gl_find_internal_format(GL *gl, GLint internal_format)
{
    int features = gl_format_feature_flags(gl);
    for (int n = 0; gl_formats[n].type; n++) {
        const struct gl_format *f = &gl_formats[n];
        if (f->internal_format == internal_format && (f->flags & features))
            return f;
    }
    return NULL;
}

// Find the first supported format with a specific gl_format.type
static const struct gl_format *gl_find_gl_type_format(GL *gl, GLenum type)
{
    int features = gl_format_feature_flags(gl);
    for (int i = 0; gl_formats[i].type; i++) {
        const struct gl_format *f = &gl_formats[i];
        if (f->type == type && (f->flags & features))
            return f;
    }
    return NULL;
}

// type: one of MPGL_TYPE_*
// flags: bitset of F_*, all flags must be present
const struct gl_format *gl_find_format(GL *gl, int type, int flags,
                                       int bytes_per_component, int n_components)
{
    if (!bytes_per_component || !n_components || !type)
        return NULL;
    int features = gl_format_feature_flags(gl);
    for (int n = 0; gl_formats[n].type; n++) {
        const struct gl_format *f = &gl_formats[n];
        if ((f->flags & features) &&
            ((f->flags & flags) == flags) &&
            gl_format_type(f) == type &&
            gl_component_size(f->type) == bytes_per_component &&
            gl_format_components(f->format) == n_components)
            return f;
    }
    return NULL;
}

// Return a texture-filterable unsigned normalized fixed point format.
const struct gl_format *gl_find_unorm_format(GL *gl, int bytes_per_component,
                                             int n_components)
{
    return gl_find_format(gl, MPGL_TYPE_UNORM, F_TF, bytes_per_component,
                          n_components);
}

// Return an unsigned integer format.
const struct gl_format *gl_find_uint_format(GL *gl, int bytes_per_component,
                                            int n_components)
{
    return gl_find_format(gl, MPGL_TYPE_UINT, 0, bytes_per_component,
                          n_components);
}

// Return a 16 bit float format. Note that this will return a GL_FLOAT format
// with 32 bit per component; just the internal representation is smaller.
// Some GL versions will allow upload with GL_HALF_FLOAT as well.
const struct gl_format *gl_find_float16_format(GL *gl, int n_components)
{
    return gl_find_format(gl, MPGL_TYPE_FLOAT, F_F16, 4, n_components);
}

int gl_format_type(const struct gl_format *format)
{
    if (!format)
        return 0;
    if (format->type == GL_FLOAT)
        return MPGL_TYPE_FLOAT;
    if (gl_integer_format_to_base(format->format))
        return MPGL_TYPE_UINT;
    return MPGL_TYPE_UNORM;
}

// Return base internal format of an integer format, or 0 if it's not integer.
// "format" is like in struct gl_format.
GLenum gl_integer_format_to_base(GLenum format)
{
    switch (format) {
    case GL_RED_INTEGER:        return GL_RED;
    case GL_RG_INTEGER:         return GL_RG;
    case GL_RGB_INTEGER:        return GL_RGB;
    case GL_RGBA_INTEGER:       return GL_RGBA;
    }
    return 0;
}

// Return whether it's a non-normalized integer format.
// "format" is like in struct gl_format.
bool gl_is_integer_format(GLenum format)
{
    return !!gl_integer_format_to_base(format);
}

// Return the number of bytes per component this format implies.
// Returns 0 for formats with non-byte alignments and formats which
// merge multiple components (like GL_UNSIGNED_SHORT_5_6_5).
// "type" is like in struct gl_format.
int gl_component_size(GLenum type)
{
    switch (type) {
    case GL_UNSIGNED_BYTE:                      return 1;
    case GL_UNSIGNED_SHORT:                     return 2;
    case GL_FLOAT:                              return 4;
    }
    return 0;
}

// Return the number of separate color components.
// "format" is like in struct gl_format.
int gl_format_components(GLenum format)
{
    switch (format) {
    case GL_RED:
    case GL_RED_INTEGER:
    case GL_LUMINANCE:
        return 1;
    case GL_RG:
    case GL_RG_INTEGER:
    case GL_LUMINANCE_ALPHA:
        return 2;
    case GL_RGB:
    case GL_RGB_INTEGER:
        return 3;
    case GL_RGBA:
    case GL_RGBA_INTEGER:
        return 4;
    }
    return 0;
}

// Return the number of bytes per pixel for the given format.
// Parameter names like in struct gl_format.
int gl_bytes_per_pixel(GLenum format, GLenum type)
{
    // Formats with merged components are special.
    switch (type) {
    case GL_UNSIGNED_INT_2_10_10_10_REV:        return 4;
    case GL_UNSIGNED_SHORT_5_6_5:               return 2;
    case GL_UNSIGNED_SHORT_8_8_APPLE:           return 2;
    case GL_UNSIGNED_SHORT_8_8_REV_APPLE:       return 2;
    }

    return gl_component_size(type) * gl_format_components(format);
}

// The format has cleanly separated components (on byte boundaries).
bool gl_format_is_regular(const struct gl_format *fmt)
{
    int bpp = gl_component_size(fmt->type) * gl_format_components(fmt->format);
    return bpp == gl_bytes_per_pixel(fmt->format, fmt->type);
}

// Like gl_find_unorm_format(), but takes bits (not bytes), and if no fixed
// point format is available, return an unsigned integer format.
static const struct gl_format *find_plane_format(GL *gl, int bytes, int n_channels)
{
    const struct gl_format *f = gl_find_unorm_format(gl, bytes, n_channels);
    if (f)
        return f;
    return gl_find_uint_format(gl, bytes, n_channels);
}

// Put a mapping of imgfmt to OpenGL textures into *out. Basically it selects
// the correct texture formats needed to represent an imgfmt in OpenGL, with
// textures using the same memory organization as on the CPU.
// Each plane is represented by a texture, and each texture has a RGBA
// component order. out->components describes the meaning of them.
// May return integer formats for >8 bit formats, if the driver has no
// normalized 16 bit formats.
// Returns false (and *out is not touched) if no format found.
bool gl_get_imgfmt_desc(GL *gl, int imgfmt, struct gl_imgfmt_desc *out)
{
    struct gl_imgfmt_desc res = {0};

    struct mp_regular_imgfmt regfmt;
    if (mp_get_regular_imgfmt(&regfmt, imgfmt)) {
        res.num_planes = regfmt.num_planes;
        res.component_bits = regfmt.component_size * 8;
        res.component_pad = regfmt.component_pad;
        for (int n = 0; n < regfmt.num_planes; n++) {
            struct mp_regular_imgfmt_plane *plane = &regfmt.planes[n];
            res.planes[n] = find_plane_format(gl, regfmt.component_size,
                                              plane->num_components);
            if (!res.planes[n])
                return false;
            for (int i = 0; i < plane->num_components; i++)
                res.components[n][i] = plane->components[i];
        }
        res.chroma_w = regfmt.chroma_w;
        res.chroma_h = regfmt.chroma_h;
        goto supported;
    }

    // Special formats for which OpenGL happens to have direct support.
    if (imgfmt == IMGFMT_RGB565) {
        res.num_planes = 1;
        res.planes[0] = gl_find_gl_type_format(gl, GL_UNSIGNED_SHORT_5_6_5);
        if (!res.planes[0])
            return false;
        for (int n = 0; n < 3; n++)
            res.components[0][n] = n + 1;
        res.chroma_w = res.chroma_h = 1;
        goto supported;
    }
    if (imgfmt == IMGFMT_UYVY) {
        res.num_planes = 1;
        res.planes[0] = gl_find_gl_type_format(gl, GL_UNSIGNED_SHORT_8_8_APPLE);
        if (!res.planes[0])
            return false;
        res.components[0][0] = 3;
        res.components[0][1] = 1;
        res.components[0][2] = 2;
        res.chroma_w = res.chroma_h = 1;
        goto supported;
    }

    // Unsupported format
    return false;

supported:

    *out = res;
    return true;
}
