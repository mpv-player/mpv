#pragma once

#include "common/common.h"
#include "misc/bstr.h"

// Handle for a rendering API backend.
struct ra {
    struct ra_fns *fns;
    void *priv;

    int glsl_version;       // GLSL version (e.g. 300 => 3.0)
    bool glsl_es;           // use ES dialect

    struct mp_log *log;

    // RA_CAP_* bit field. The RA backend must set supported features at init
    // time.
    uint64_t caps;

    // Maximum supported width and height of a 2D texture. Set by the RA backend
    // at init time.
    int max_texture_wh;

    // Maximum shared memory for compute shaders. Set by the RA backend at init
    // time.
    size_t max_shmem;

    // Set of supported texture formats. Must be added by RA backend at init time.
    struct ra_format **formats;
    int num_formats;

    // GL-specific: if set, accelerate texture upload by using an additional
    // buffer (i.e. uses more memory). Does not affect uploads done by
    // ra_tex_create (if initial_data is set). Set by the RA user.
    bool use_pbo;
};

enum {
    RA_CAP_TEX_1D         = 1 << 0, // supports 1D textures (as shader inputs)
    RA_CAP_TEX_3D         = 1 << 1, // supports 3D textures (as shader inputs)
    RA_CAP_BLIT           = 1 << 2, // supports ra_fns.blit
    RA_CAP_COMPUTE        = 1 << 3, // supports compute shaders
    RA_CAP_PBO            = 1 << 4, // supports ra.use_pbo
    RA_CAP_BUF_RW         = 1 << 5, // supports RA_VARTYPE_BUF_RW
    RA_CAP_NESTED_ARRAY   = 1 << 6, // supports nested arrays
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
    bool ordered;           // components are sequential in memory, and returned
                            // by the shader in memory order
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
    bool external_oes;      // hack for GL_TEXTURE_EXTERNAL_OES idiocy
    // If non-NULL, the texture will be created with these contents, and is
    // considered immutable afterwards (no upload, mapping, or rendering to it).
    void *initial_data;
};

// Conflates the following typical GPU API concepts:
// - texture itself
// - sampler state
// - staging buffers for texture upload
// - framebuffer objects
// - wrappers for swapchain framebuffers
// - synchronization needed for upload/rendering/etc.
struct ra_tex {
    // All fields are read-only after creation.
    struct ra_tex_params params;
    void *priv;
};

// Buffer type hint. Setting this may result in more or less efficient
// operation, although it shouldn't technically prohibit anything
enum ra_buf_type {
    RA_BUF_TYPE_INVALID,
    RA_BUF_TYPE_TEX_UPLOAD, // texture upload buffer (pixel buffer object)
    RA_BUF_TYPE_SHADER_STORAGE // shader buffer, used for RA_VARTYPE_BUF_RW
};

struct ra_buf_params {
    enum ra_buf_type type;
    size_t size;
    // Creates a read-writable persistent mapping (ra_buf.data)
    bool host_mapped;
    // If non-NULL, the buffer will be created with these contents. Otherwise,
    // the initial data is undefined.
    void *initial_data;
};

// A generic buffer, which can be used for many purposes (texture upload,
// storage buffer, uniform buffer, etc.)
struct ra_buf {
    // All fields are read-only after creation.
    struct ra_buf_params params;
    void *data; // for persistently mapped buffers, points to the first byte
    void *priv;
};

// Type of a shader uniform variable, or a vertex attribute. In all cases,
// vectors are matrices are done by having more than 1 value.
enum ra_vartype {
    RA_VARTYPE_INVALID,
    RA_VARTYPE_INT,             // C: int, GLSL: int, ivec*
    RA_VARTYPE_FLOAT,           // C: float, GLSL: float, vec*, mat*
    RA_VARTYPE_TEX,             // C: ra_tex*, GLSL: various sampler types
                                // ra_tex.params.render_src must be true
    RA_VARTYPE_IMG_W,           // C: ra_tex*, GLSL: various image types
                                // write-only (W) image for compute shaders
    RA_VARTYPE_BYTE_UNORM,      // C: uint8_t, GLSL: int, vec* (vertex data only)
    RA_VARTYPE_BUF_RW,          // C: ra_buf*, GLSL: buffer block
};

// Represents a uniform, texture input parameter, and similar things.
struct ra_renderpass_input {
    const char *name;       // name as used in the shader
    enum ra_vartype type;
    // The total number of values is given by dim_v * dim_m.
    int dim_v;              // vector dimension (1 for non-vector and non-matrix)
    int dim_m;              // additional matrix dimension (dim_v x dim_m)
    // Vertex data: byte offset of the attribute into the vertex struct
    // RA_VARTYPE_TEX: texture unit
    // RA_VARTYPE_IMG_W: image unit
    // RA_VARTYPE_BUF_* buffer binding point
    // Other uniforms: unused
    int binding;
};

size_t ra_render_pass_input_data_size(struct ra_renderpass_input *input);

enum ra_blend {
    RA_BLEND_ZERO,
    RA_BLEND_ONE,
    RA_BLEND_SRC_ALPHA,
    RA_BLEND_ONE_MINUS_SRC_ALPHA,
};

enum ra_renderpass_type {
    RA_RENDERPASS_TYPE_INVALID,
    RA_RENDERPASS_TYPE_RASTER,  // vertex+fragment shader
    RA_RENDERPASS_TYPE_COMPUTE, // compute shader
};

// Static part of a rendering pass. It conflates the following:
//  - compiled shader and its list of uniforms
//  - vertex attributes and its shader mappings
//  - blending parameters
// (For Vulkan, this would be shader module + pipeline state.)
// Upon creation, the values of dynamic values such as uniform contents (whose
// initial values are not provided here) are required to be 0.
struct ra_renderpass_params {
    enum ra_renderpass_type type;

    // Uniforms, including texture/sampler inputs.
    struct ra_renderpass_input *inputs;
    int num_inputs;

    // Highly implementation-specific byte array storing a compiled version
    // of the program. Can be used to speed up shader compilation. A backend
    // xan read this in renderpass_create, or set this on the newly created
    // ra_renderpass params field.
    bstr cached_program;

    // --- type==RA_RENDERPASS_TYPE_RASTER only

    // Describes the format of the vertex data.
    struct ra_renderpass_input *vertex_attribs;
    int num_vertex_attribs;
    int vertex_stride;

    // Shader text, in GLSL. (Yes, you need a GLSL compiler.)
    // These are complete shaders, including prelude and declarations.
    const char *vertex_shader;
    const char *frag_shader;

    // Target blending mode. If enable_blend is false, the blend_ fields can
    // be ignored.
    bool enable_blend;
    enum ra_blend blend_src_rgb;
    enum ra_blend blend_dst_rgb;
    enum ra_blend blend_src_alpha;
    enum ra_blend blend_dst_alpha;

    // --- type==RA_RENDERPASS_TYPE_COMPUTE only

    // Shader text, like vertex_shader/frag_shader.
    const char *compute_shader;
};

struct ra_renderpass_params *ra_render_pass_params_copy(void *ta_parent,
        const struct ra_renderpass_params *params);

// Conflates the following typical GPU API concepts:
// - various kinds of shaders
// - rendering pipelines
// - descriptor sets, uniforms, other bindings
// - all synchronization necessary
// - the current values of all uniforms (this one makes it relatively stateful
//   from an API perspective)
struct ra_renderpass {
    // All fields are read-only after creation.
    struct ra_renderpass_params params;
    void *priv;
};

// An input value (see ra_renderpass_input).
struct ra_renderpass_input_val {
    int index;  // index into ra_renderpass_params.inputs[]
    void *data; // pointer to data according to ra_renderpass_input
                // (e.g. type==RA_VARTYPE_FLOAT+dim_v=3,dim_m=3 => float[9])
};

// Parameters for performing a rendering pass (basically the dynamic params).
// These change potentially every time.
struct ra_renderpass_run_params {
    struct ra_renderpass *pass;

    // Generally this lists parameters only which changed since the last
    // invocation and need to be updated. The ra_renderpass instance is
    // supposed to keep unchanged values from the previous run.
    // For non-primitive types like textures, these entries are always added,
    // even if they do not change.
    struct ra_renderpass_input_val *values;
    int num_values;

    // --- pass->params.type==RA_RENDERPASS_TYPE_RASTER only

    // target->params.render_dst must be true.
    struct ra_tex *target;
    struct mp_rect viewport;
    struct mp_rect scissors;

    // (The primitive type is always a triangle list.)
    void *vertex_data;
    int vertex_count;   // number of vertex elements, not bytes

    // --- pass->params.type==RA_RENDERPASS_TYPE_COMPUTE only

    // Number of work groups to be run in X/Y/Z dimensions.
    int compute_groups[3];
};

enum {
    // Flags for the texture_upload flags parameter.
    RA_TEX_UPLOAD_DISCARD = 1 << 0, // discard pre-existing data not in the region
};

// This is an opaque type provided by the implementation, but we want to at
// least give it a saner name than void* for code readability purposes.
typedef void ra_timer;

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

    // Copy from CPU RAM to the texture. This is an extremely common operation.
    // Unlike with OpenGL, the src data has to have exactly the same format as
    // the texture, and no conversion is supported.
    // region can be NULL - if it's not NULL, then the provided pointer only
    // contains data for the given region. Only part of the texture data is
    // updated, and ptr points to the first pixel in the region. If
    // RA_TEX_UPLOAD_DISCARD is set, data outside of the region can return to
    // an uninitialized state. The region is always strictly within the texture
    // and has a size >0 in both dimensions. 2D textures only.
    // For 1D textures, stride is ignored, and region must be NULL.
    // For 3D textures, stride is not supported. All data is fully packed with
    // no padding, and stride is ignored, and region must be NULL.
    // If buf is not NULL, then src must be within the provided buffer. The
    // operation is implied to have dramatically better performance, but
    // requires correct flushing and fencing operations by the caller to deal
    // with asynchronous host/GPU behavior. If any of these conditions are not
    // met, undefined behavior will result.
    void (*tex_upload)(struct ra *ra, struct ra_tex *tex,
                       const void *src, ptrdiff_t stride,
                       struct mp_rect *region, uint64_t flags,
                       struct ra_buf *buf);

    // Create a buffer. This can be used as a persistently mapped buffer,
    // a uniform buffer, a shader storage buffer or possibly others.
    // Not all usage types must be supported; may return NULL if unavailable.
    struct ra_buf *(*buf_create)(struct ra *ra,
                                 const struct ra_buf_params *params);

    void (*buf_destroy)(struct ra *ra, struct ra_buf *buf);

    // Essentially a fence: once the GPU uses the mapping for read-access (e.g.
    // by starting a texture upload), the host must not write to the mapped
    // data until an internal object has been signalled. This call returns
    // whether it was signalled yet. If true, write accesses are allowed again.
    // Optional, may be NULL if unavailable. This is only usable for buffers
    // which have been persistently mapped.
    bool (*poll_mapped_buffer)(struct ra *ra, struct ra_buf *buf);

    // Clear the dst with the given color (rgba) and within the given scissor.
    // dst must have dst->params.render_dst==true. Content outside of the
    // scissor is preserved.
    void (*clear)(struct ra *ra, struct ra_tex *dst, float color[4],
                  struct mp_rect *scissor);

    // Copy a sub-rectangle from one texture to another. The source/dest region
    // is always within the texture bounds. Areas outside the dest region are
    // preserved. The formats of the textures must be losely compatible. The
    // dst texture can be a swapchain framebuffer, but src can not. Only 2D
    // textures are supported.
    // Both textures must have tex->params.render_dst==true (even src, which is
    // an odd GL requirement).
    // Rectangles with negative width/height lead to flipping, different src/dst
    // sizes lead to point scaling. Coordinates are always in pixels.
    // Optional. Only available if RA_CAP_BLIT is set (if it's not set, it must
    // not be called, even if it's non-NULL).
    void (*blit)(struct ra *ra, struct ra_tex *dst, struct ra_tex *src,
                 struct mp_rect *dst_rc, struct mp_rect *src_rc);

    // Compile a shader and create a pipeline. This is a rare operation.
    // The params pointer and anything it points to must stay valid until
    // renderpass_destroy.
    struct ra_renderpass *(*renderpass_create)(struct ra *ra,
                                    const struct ra_renderpass_params *params);

    void (*renderpass_destroy)(struct ra *ra, struct ra_renderpass *pass);

    // Perform a render pass, basically drawing a list of triangles to a FBO.
    // This is an extremely common operation.
    void (*renderpass_run)(struct ra *ra,
                           const struct ra_renderpass_run_params *params);

    // Create a timer object. Returns NULL on failure, or if timers are
    // unavailable.
    ra_timer *(*timer_create)(struct ra *ra);

    void (*timer_destroy)(struct ra *ra, ra_timer *timer);

    // Start recording a timer. Note that valid usage requires you to pair
    // every start with a stop. Trying to start a timer twice, or trying to
    // stop a timer before having started it, consistutes invalid usage.
    void (*timer_start)(struct ra *ra, ra_timer *timer);

    // Stop recording a timer. This also returns any results that have been
    // measured since the last usage of this ra_timer. It's important to note
    // that GPU timer measurement are asynchronous, so this function does not
    // always produce a value - and the values it does produce are typically
    // delayed by a few frames. When no value is available, this returns 0.
    uint64_t (*timer_stop)(struct ra *ra, ra_timer *timer);

    // Hint that possibly queued up commands should be sent to the GPU. Optional.
    void (*flush)(struct ra *ra);

    // Optional.
    void (*debug_marker)(struct ra *ra, const char *msg);
};

struct ra_tex *ra_tex_create(struct ra *ra, const struct ra_tex_params *params);
void ra_tex_free(struct ra *ra, struct ra_tex **tex);

struct ra_buf *ra_buf_create(struct ra *ra, const struct ra_buf_params *params);
void ra_buf_free(struct ra *ra, struct ra_buf **buf);

void ra_free(struct ra **ra);

const struct ra_format *ra_find_unorm_format(struct ra *ra,
                                             int bytes_per_component,
                                             int n_components);
const struct ra_format *ra_find_uint_format(struct ra *ra,
                                            int bytes_per_component,
                                            int n_components);
const struct ra_format *ra_find_float16_format(struct ra *ra, int n_components);
const struct ra_format *ra_find_named_format(struct ra *ra, const char *name);

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
