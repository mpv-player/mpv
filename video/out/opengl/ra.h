#pragma once

#include "common/common.h"

// Handle for a rendering API backend.
struct ra {
    struct ra_fns *fns;
    void *priv;

    struct mp_log *log;

    // RA_CAP_* bit field. The RA backend must set supported features at init
    // time.
    uint64_t caps;

    // Set of supported texture formats. Must be added by RA backend at init time.
    struct ra_format **formats;
    int num_formats;
};

enum {
    RA_CAP_TEX_1D = 0 << 0,     // supports 1D textures (as shader source textures)
    RA_CAP_TEX_3D = 0 << 1,     // supports 3D textures (as shader source textures)
};

enum ra_ctype {
    RA_CTYPE_UNKNOWN = 0,   // also used for inconsistent multi-component formats
    RA_CTYPE_UNORM,         // unsigned normalized integer (fixed point) formats
    RA_CTYPE_UINT,          // full integer formats
    RA_CTYPE_FLOAT,         // float formats (signed, any bit size)
};

// All formats must be useable as texture formats. All formats must be byte
// aligned (all pixels start and end on a byte boundary), at least as far CPU
// transfers are concerned.
struct ra_format {
    // All fields are read-only after creation.
    const char *name;       // symbolic name for user interaction/debugging
    void *priv;
    enum ra_ctype ctype;    // data type of each component
    int num_components;     // component count, 0 if not applicable, max. 4
    int component_size[4];  // in bits, all entries 0 if not applicable
    int component_depth[4]; // bits in use for each component, 0 if not applicable
                            // (_must_ be set if component_size[] includes padding,
                            //  and the real procession as seen by shader is lower)
    int pixel_size;         // in bytes, total pixel size (0 if opaque)
    bool luminance_alpha;   // pre-GL_ARB_texture_rg hack for 2 component textures
                            // if this is set, shader must use .ra instead of .rg
                            // only applies to 2-component textures
    bool linear_filter;     // linear filtering available from shader
    bool renderable;        // can be used for render targets

    // If not 0, the format represents some sort of packed fringe format, whose
    // shader representation is given by the special_imgfmt_desc pointer.
    int special_imgfmt;
    const struct ra_imgfmt_desc *special_imgfmt_desc;
};

struct ra_tex_params {
    int dimensions;         // 1-3 for 1D-3D textures
    // Size of the texture. 1D textures require h=d=1, 2D textures require d=1.
    int w, h, d;
    const struct ra_format *format;
    bool render_src;        // must be useable as source texture in a shader
    bool render_dst;        // must be useable as target texture in a shader
                            // this requires creation of a FBO
    // When used as render source texture.
    bool src_linear;        // if false, use nearest sampling (whether this can
                            // be true depends on ra_format.linear_filter)
    bool src_repeat;        // if false, clamp texture coordinates to edge
                            // if true, repeat texture coordinates
    bool non_normalized;    // hack for GL_TEXTURE_RECTANGLE OSX idiocy
                            // always set to false, except in OSX code
    // If non-NULL, the texture will be created with these contents, and is
    // considered immutable afterwards (no upload, mapping, or rendering to it).
    void *initial_data;
};

struct ra_tex {
    // All fields are read-only after creation.
    struct ra_tex_params params;
    void *priv;
    // Set by user, GL only: attempt to accelerate upload with PBOs.
    bool use_pbo;
};

// A persistent mapping, which can be used for texture upload.
struct ra_mapped_buffer {
    // All fields are read-only after creation. The data is read/write, but
    // requires explicit fence usage.
    void *priv;
    void *data;             // pointer to first usable byte
    size_t size;            // total size of the mapping, starting at data
    size_t preferred_align; // preferred stride/start alignment for optimal copy
};

// Rendering API entrypoints. (Note: there are some additional hidden features
// you need to take care of. For example, hwdec mapping will be provided
// separately from ra, but might need to call into ra private code.)
struct ra_fns {
    void (*destroy)(struct ra *ra);

    // Create a texture (with undefined contents). Return NULL on failure.
    // This is a rare operation, and normally textures and even FBOs for
    // temporary rendering intermediate data are cached.
    struct ra_tex *(*tex_create)(struct ra *ra,
                                 const struct ra_tex_params *params);

    void (*tex_destroy)(struct ra *ra, struct ra_tex *tex);

    // Copy from CPU RAM to the texture. The image dimensions are as specified
    // in tex->params.
    // This is an extremely common operation.
    // Unlike with OpenGL, the src data has to have exactly the same format as
    // the texture, and no conversion is supported.
    // tex->params.require_upload must be true.
    // For 1D textures, stride is ignored.
    // For 3D textures, stride is not supported. All data is fully packed with
    // no padding, and stride is ignored.
    // If buf is not NULL, then src must be within the provided buffer. The
    // operation is implied to have dramatically better performance, but
    // requires correct flushing and fencing operations by the caller to deal
    // with asynchronous host/GPU behavior. If any of these conditions are not
    // met, undefined behavior will result.
    void (*tex_upload)(struct ra *ra, struct ra_tex *tex,
                       const void *src, ptrdiff_t stride,
                       struct ra_mapped_buffer *buf);

    // Create a persistently mapped buffer for tex_upload.
    // Optional, can be NULL or return NULL if unavailable.
    struct ra_mapped_buffer *(*create_mapped_buffer)(struct ra *ra, size_t size);

    void (*destroy_mapped_buffer)(struct ra *ra, struct ra_mapped_buffer *buf);

    // Essentially a fence: once the GPU uses the mapping for read-access (e.g.
    // by starting a texture upload), the host must not write to the mapped
    // data until an internal object has been signalled. This call returns
    // whether it was signalled yet. If true, write accesses are allowed again.
    // Optional, only available if flush_mapping is.
    bool (*poll_mapped_buffer)(struct ra *ra, struct ra_mapped_buffer *buf);
};

struct ra_tex *ra_tex_create(struct ra *ra, const struct ra_tex_params *params);
void ra_tex_free(struct ra *ra, struct ra_tex **tex);

const struct ra_format *ra_find_unorm_format(struct ra *ra,
                                             int bytes_per_component,
                                             int n_components);
const struct ra_format *ra_find_uint_format(struct ra *ra,
                                            int bytes_per_component,
                                            int n_components);
const struct ra_format *ra_find_float16_format(struct ra *ra, int n_components);

struct ra_imgfmt_desc {
    int num_planes;
    const struct ra_format *planes[4];
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
    // For ra_format.luminance_alpha, this returns 1/2 ("rg") instead of 1/4
    // ("ra"). the logic is that the texture format has 2 channels, thus the
    // data must be returned in the first two components. The renderer fixes
    // this later.
    uint8_t components[4][4];
};

bool ra_get_imgfmt_desc(struct ra *ra, int imgfmt, struct ra_imgfmt_desc *out);

void ra_dump_tex_formats(struct ra *ra, int msgl);
void ra_dump_imgfmt_desc(struct ra *ra, const struct ra_imgfmt_desc *desc,
                         int msgl);
void ra_dump_img_formats(struct ra *ra, int msgl);
