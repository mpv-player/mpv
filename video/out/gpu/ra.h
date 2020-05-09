#pragma once

#include "common/common.h"
#include "misc/bstr.h"

// Handle for a rendering API backend.
struct ra {
    struct ra_fns *fns;
    void *priv;

    int glsl_version;       // GLSL version (e.g. 300 => 3.0)
    bool glsl_es;           // use ES dialect
    bool glsl_vulkan;       // use vulkan dialect

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

    // Maximum push constant size. Set by the RA backend at init time.
    size_t max_pushc_size;

    // Set of supported texture formats. Must be added by RA backend at init time.
    // If there are equivalent formats with different caveats, the preferred
    // formats should have a lower index. (E.g. GLES3 should put rg8 before la.)
    struct ra_format **formats;
    int num_formats;

    // Accelerate texture uploads via an extra PBO even when
    // RA_CAP_DIRECT_UPLOAD is supported. This is basically only relevant for
    // OpenGL. Set by the RA user.
    bool use_pbo;

    // Array of native resources. For the most part an "escape" mechanism, and
    // usually does not contain parameters required for basic functionality.
    struct ra_native_resource *native_resources;
    int num_native_resources;
};

// For passing through windowing system specific parameters and such. The
// names are always internal (except for legacy opengl-cb uses; the libmpv
// render API uses mpv_render_param_type and maps them to names internally).
// For example, a name="x11" entry has a X11 display as (Display*)data.
struct ra_native_resource {
    const char *name;
    void *data;
};

// Add a ra_native_resource entry. Both name and data pointers must stay valid
// until ra termination.
void ra_add_native_resource(struct ra *ra, const char *name, void *data);

// Search ra->native_resources, returns NULL on failure.
void *ra_get_native_resource(struct ra *ra, const char *name);

enum {
    RA_CAP_TEX_1D         = 1 << 0, // supports 1D textures (as shader inputs)
    RA_CAP_TEX_3D         = 1 << 1, // supports 3D textures (as shader inputs)
    RA_CAP_BLIT           = 1 << 2, // supports ra_fns.blit
    RA_CAP_COMPUTE        = 1 << 3, // supports compute shaders
    RA_CAP_DIRECT_UPLOAD  = 1 << 4, // supports tex_upload without ra_buf
    RA_CAP_BUF_RO         = 1 << 5, // supports RA_VARTYPE_BUF_RO
    RA_CAP_BUF_RW         = 1 << 6, // supports RA_VARTYPE_BUF_RW
    RA_CAP_NESTED_ARRAY   = 1 << 7, // supports nested arrays
    RA_CAP_GLOBAL_UNIFORM = 1 << 8, // supports using "naked" uniforms (not UBO)
    RA_CAP_GATHER         = 1 << 9, // supports textureGather in GLSL
    RA_CAP_FRAGCOORD      = 1 << 10, // supports reading from gl_FragCoord
    RA_CAP_PARALLEL_COMPUTE  = 1 << 11, // supports parallel compute shaders
    RA_CAP_NUM_GROUPS     = 1 << 12, // supports gl_NumWorkGroups
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
                            // by the shader in memory order (the shader can
                            // return arbitrary values for unused components)
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
    bool storable;          // can be used for storage images
    bool dummy_format;      // is not a real ra_format but a fake one (e.g. FBO).
                            // dummy formats cannot be used to create textures

    // If not 0, the format represents some sort of packed fringe format, whose
    // shader representation is given by the special_imgfmt_desc pointer.
    int special_imgfmt;
    const struct ra_imgfmt_desc *special_imgfmt_desc;

    // This gives the GLSL image format corresponding to the format, if any.
    // (e.g. rgba16ui)
    const char *glsl_format;
};

struct ra_tex_params {
    int dimensions;         // 1-3 for 1D-3D textures
    // Size of the texture. 1D textures require h=d=1, 2D textures require d=1.
    int w, h, d;
    const struct ra_format *format;
    bool render_src;        // must be useable as source texture in a shader
    bool render_dst;        // must be useable as target texture in a shader
    bool storage_dst;       // must be usable as a storage image (RA_VARTYPE_IMG_W)
    bool blit_src;          // must be usable as a blit source
    bool blit_dst;          // must be usable as a blit destination
    bool host_mutable;      // texture may be updated with tex_upload
    bool downloadable;      // texture can be read with tex_download
    // When used as render source texture.
    bool src_linear;        // if false, use nearest sampling (whether this can
                            // be true depends on ra_format.linear_filter)
    bool src_repeat;        // if false, clamp texture coordinates to edge
                            // if true, repeat texture coordinates
    bool non_normalized;    // hack for GL_TEXTURE_RECTANGLE OSX idiocy
                            // always set to false, except in OSX code
    bool external_oes;      // hack for GL_TEXTURE_EXTERNAL_OES idiocy
    // If non-NULL, the texture will be created with these contents. Using
    // this does *not* require setting host_mutable. Otherwise, the initial
    // data is undefined.
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

struct ra_tex_upload_params {
    struct ra_tex *tex; // Texture to upload to
    bool invalidate;    // Discard pre-existing data not in the region uploaded
    // Uploading from buffer:
    struct ra_buf *buf; // Buffer to upload from (mutually exclusive with `src`)
    size_t buf_offset;  // Start of data within buffer (bytes)
    // Uploading directly: (Note: If RA_CAP_DIRECT_UPLOAD is not set, then this
    // will be internally translated to a tex_upload buffer by the RA)
    const void *src;    // Address of data
    // For 2D textures only:
    struct mp_rect *rc; // Region to upload. NULL means entire image
    ptrdiff_t stride;   // The size of a horizontal line in bytes (*not* texels!)
};

struct ra_tex_download_params {
    struct ra_tex *tex; // Texture to download from
    // Downloading directly (set by caller, data written to by callee):
    void *dst;          // Address of data (packed with no alignment)
    ptrdiff_t stride;   // The size of a horizontal line in bytes (*not* texels!)
};

// Buffer usage type. This restricts what types of operations may be performed
// on a buffer.
enum ra_buf_type {
    RA_BUF_TYPE_INVALID,
    RA_BUF_TYPE_TEX_UPLOAD,     // texture upload buffer (pixel buffer object)
    RA_BUF_TYPE_SHADER_STORAGE, // shader buffer (SSBO), for RA_VARTYPE_BUF_RW
    RA_BUF_TYPE_UNIFORM,        // uniform buffer (UBO), for RA_VARTYPE_BUF_RO
    RA_BUF_TYPE_VERTEX,         // not publicly usable (RA-internal usage)
    RA_BUF_TYPE_SHARED_MEMORY,  // device memory for sharing with external API
};

struct ra_buf_params {
    enum ra_buf_type type;
    size_t size;
    bool host_mapped;  // create a read-writable persistent mapping (ra_buf.data)
    bool host_mutable; // contents may be updated via buf_update()
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
                                // ra_tex.params.storage_dst must be true
    RA_VARTYPE_BYTE_UNORM,      // C: uint8_t, GLSL: int, vec* (vertex data only)
    RA_VARTYPE_BUF_RO,          // C: ra_buf*, GLSL: uniform buffer block
                                // buf type must be RA_BUF_TYPE_UNIFORM
    RA_VARTYPE_BUF_RW,          // C: ra_buf*, GLSL: shader storage buffer block
                                // buf type must be RA_BUF_TYPE_SHADER_STORAGE
    RA_VARTYPE_COUNT
};

// Returns the host size of a ra_vartype, or 0 for abstract vartypes (e.g. tex)
size_t ra_vartype_size(enum ra_vartype type);

// Represents a uniform, texture input parameter, and similar things.
struct ra_renderpass_input {
    const char *name;       // name as used in the shader
    enum ra_vartype type;
    // The total number of values is given by dim_v * dim_m.
    int dim_v;              // vector dimension (1 for non-vector and non-matrix)
    int dim_m;              // additional matrix dimension (dim_v x dim_m)
    // Vertex data: byte offset of the attribute into the vertex struct
    size_t offset;
    // RA_VARTYPE_TEX: texture unit
    // RA_VARTYPE_IMG_W: image unit
    // RA_VARTYPE_BUF_* buffer binding point
    // Other uniforms: unused
    // Bindings must be unique within each namespace, as specified by
    // desc_namespace()
    int binding;
};

// Represents the layout requirements of an input value
struct ra_layout {
    size_t align;  // the alignment requirements (always a power of two)
    size_t stride; // the delta between two rows of an array/matrix
    size_t size;   // the total size of the input
};

// Returns the host layout of a render pass input. Returns {0} for renderpass
// inputs without a corresponding host representation (e.g. textures/buffers)
struct ra_layout ra_renderpass_input_layout(struct ra_renderpass_input *input);

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
    size_t push_constants_size; // must be <= ra.max_pushc_size and a multiple of 4

    // Highly implementation-specific byte array storing a compiled version
    // of the program. Can be used to speed up shader compilation. A backend
    // xan read this in renderpass_create, or set this on the newly created
    // ra_renderpass params field.
    bstr cached_program;

    // --- type==RA_RENDERPASS_TYPE_RASTER only

    // Describes the format of the vertex data. When using ra.glsl_vulkan,
    // the order of this array must match the vertex attribute locations.
    struct ra_renderpass_input *vertex_attribs;
    int num_vertex_attribs;
    int vertex_stride;

    // Format of the target texture
    const struct ra_format *target_format;

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

    // If true, the contents of `target` not written to will become undefined
    bool invalidate_target;

    // --- type==RA_RENDERPASS_TYPE_COMPUTE only

    // Shader text, like vertex_shader/frag_shader.
    const char *compute_shader;
};

struct ra_renderpass_params *ra_renderpass_params_copy(void *ta_parent,
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
    void *push_constants; // must be set if params.push_constants_size > 0

    // --- pass->params.type==RA_RENDERPASS_TYPE_RASTER only

    // target->params.render_dst must be true, and target->params.format must
    // match pass->params.target_format.
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

    // Upload data to a texture. This is an extremely common operation. When
    // using a buffer, the contants of the buffer must exactly match the image
    // - conversions between bit depth etc. are not supported. The buffer *may*
    // be marked as "in use" while this operation is going on, and the contents
    // must not be touched again by the API user until buf_poll returns true.
    // Returns whether successful.
    bool (*tex_upload)(struct ra *ra, const struct ra_tex_upload_params *params);

    // Copy data from the texture to memory. ra_tex_params.downloadable must
    // have been set to true on texture creation.
    bool (*tex_download)(struct ra *ra, struct ra_tex_download_params *params);

    // Create a buffer. This can be used as a persistently mapped buffer,
    // a uniform buffer, a shader storage buffer or possibly others.
    // Not all usage types must be supported; may return NULL if unavailable.
    struct ra_buf *(*buf_create)(struct ra *ra,
                                 const struct ra_buf_params *params);

    void (*buf_destroy)(struct ra *ra, struct ra_buf *buf);

    // Update the contents of a buffer, starting at a given offset (*must* be a
    // multiple of 4) and up to a given size, with the contents of *data. This
    // is an extremely common operation. Calling this while the buffer is
    // considered "in use" is an error. (See: buf_poll)
    void (*buf_update)(struct ra *ra, struct ra_buf *buf, ptrdiff_t offset,
                       const void *data, size_t size);

    // Returns if a buffer is currently "in use" or not. Updating the contents
    // of a buffer (via buf_update or writing to buf->data) while it is still
    // in use is an error and may result in graphical corruption. Optional, if
    // NULL then all buffers are always usable.
    bool (*buf_poll)(struct ra *ra, struct ra_buf *buf);

    // Returns the layout requirements of a uniform buffer element. Optional,
    // but must be implemented if RA_CAP_BUF_RO is supported.
    struct ra_layout (*uniform_layout)(struct ra_renderpass_input *inp);

    // Returns the layout requirements of a push constant element. Optional,
    // but must be implemented if ra.max_pushc_size > 0.
    struct ra_layout (*push_constant_layout)(struct ra_renderpass_input *inp);

    // Returns an abstract namespace index for a given renderpass input type.
    // This will always be a value >= 0 and < RA_VARTYPE_COUNT. This is used to
    // figure out which inputs may share the same value of `binding`.
    int (*desc_namespace)(struct ra *ra, enum ra_vartype type);

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
    // The textures must have blit_src and blit_dst set, respectively.
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
    // unavailable for some reason. Optional.
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

    // Associates a marker with any past error messages, for debugging
    // purposes. Optional.
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
    // == planes[n].ctype (RA_CTYPE_UNKNOWN if not applicable)
    enum ra_ctype component_type;
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

const char *ra_fmt_glsl_format(const struct ra_format *fmt);

bool ra_get_imgfmt_desc(struct ra *ra, int imgfmt, struct ra_imgfmt_desc *out);

void ra_dump_tex_formats(struct ra *ra, int msgl);
void ra_dump_imgfmt_desc(struct ra *ra, const struct ra_imgfmt_desc *desc,
                         int msgl);
void ra_dump_img_formats(struct ra *ra, int msgl);
