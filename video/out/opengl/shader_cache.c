#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <libavutil/sha.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/mem.h>

#include "osdep/io.h"

#include "common/common.h"
#include "options/path.h"
#include "stream/stream.h"
#include "shader_cache.h"
#include "formats.h"
#include "ra_gl.h"
#include "gl_utils.h"

// Force cache flush if more than this number of shaders is created.
#define SC_MAX_ENTRIES 48

enum uniform_type {
    UT_invalid,
    UT_i,
    UT_f,
    UT_m,
};

union uniform_val {
    GLfloat f[9];
    GLint i[4];
};

struct sc_uniform {
    char *name;
    enum uniform_type type;
    const char *glsl_type;
    int size;
    GLint loc;
    union uniform_val v;
    // Set for sampler uniforms.
    GLenum tex_target;
    GLuint tex_handle;
    // Set for image uniforms
    GLuint img_handle;
    GLenum img_access;
    GLenum img_iformat;
};

struct sc_buffer {
    char *name;
    char *format;
    GLuint binding;
    GLuint ssbo;
};

struct sc_cached_uniform {
    GLint loc;
    union uniform_val v;
};

struct sc_entry {
    GLuint gl_shader;
    struct sc_cached_uniform *uniforms;
    int num_uniforms;
    bstr frag;
    bstr vert;
    bstr comp;
    struct gl_timer *timer;
    struct gl_vao vao;
};

struct gl_shader_cache {
    GL *gl;
    struct mp_log *log;

    // permanent
    char **exts;
    int num_exts;

    // this is modified during use (gl_sc_add() etc.) and reset for each shader
    bstr prelude_text;
    bstr header_text;
    bstr text;
    int next_texture_unit;
    int next_image_unit;
    int next_buffer_binding;
    struct gl_vao *vao; // deprecated

    struct sc_entry *entries;
    int num_entries;

    struct sc_entry *current_shader; // set by gl_sc_generate()

    struct sc_uniform *uniforms;
    int num_uniforms;
    struct sc_buffer *buffers;
    int num_buffers;

    const struct gl_vao_entry *vertex_entries;
    size_t vertex_size;

    // For checking that the user is calling gl_sc_reset() properly.
    bool needs_reset;

    bool error_state; // true if an error occurred

    // temporary buffers (avoids frequent reallocations)
    bstr tmp[5];

    // For the disk-cache.
    char *cache_dir;
    struct mpv_global *global; // can be NULL
};

struct gl_shader_cache *gl_sc_create(GL *gl, struct mp_log *log)
{
    struct gl_shader_cache *sc = talloc_ptrtype(NULL, sc);
    *sc = (struct gl_shader_cache){
        .gl = gl,
        .log = log,
    };
    gl_sc_reset(sc);
    return sc;
}

// Reset the previous pass. This must be called after
// Unbind all GL state managed by sc - the current program and texture units.
void gl_sc_reset(struct gl_shader_cache *sc)
{
    GL *gl = sc->gl;

    if (sc->needs_reset) {
        gl_timer_stop(gl);
        gl->UseProgram(0);

        for (int n = 0; n < sc->num_uniforms; n++) {
            struct sc_uniform *u = &sc->uniforms[n];
            if (u->type == UT_i && u->tex_target) {
                gl->ActiveTexture(GL_TEXTURE0 + u->v.i[0]);
                gl->BindTexture(u->tex_target, 0);
            }
            if (u->type == UT_i && u->img_access) {
                gl->BindImageTexture(u->v.i[0], 0, 0, GL_FALSE, 0,
                                     u->img_access, u->img_iformat);
            }
        }
        gl->ActiveTexture(GL_TEXTURE0);

        for (int n = 0; n < sc->num_buffers; n++) {
            struct sc_buffer *b = &sc->buffers[n];
            gl->BindBufferBase(GL_SHADER_STORAGE_BUFFER, b->binding, 0);
        }
    }

    sc->prelude_text.len = 0;
    sc->header_text.len = 0;
    sc->text.len = 0;
    for (int n = 0; n < sc->num_uniforms; n++)
        talloc_free(sc->uniforms[n].name);
    sc->num_uniforms = 0;
    for (int n = 0; n < sc->num_buffers; n++) {
        talloc_free(sc->buffers[n].name);
        talloc_free(sc->buffers[n].format);
    }
    sc->num_buffers = 0;
    sc->next_texture_unit = 1; // not 0, as 0 is "free for use"
    sc->next_image_unit = 1;
    sc->next_buffer_binding = 1;
    sc->vertex_entries = NULL;
    sc->vertex_size = 0;
    sc->current_shader = NULL;
    sc->needs_reset = false;
}

static void sc_flush_cache(struct gl_shader_cache *sc)
{
    MP_VERBOSE(sc, "flushing shader cache\n");

    for (int n = 0; n < sc->num_entries; n++) {
        struct sc_entry *e = &sc->entries[n];
        sc->gl->DeleteProgram(e->gl_shader);
        talloc_free(e->vert.start);
        talloc_free(e->frag.start);
        talloc_free(e->comp.start);
        talloc_free(e->uniforms);
        gl_timer_free(e->timer);
        gl_vao_uninit(&e->vao);
    }
    sc->num_entries = 0;
}

void gl_sc_destroy(struct gl_shader_cache *sc)
{
    if (!sc)
        return;
    gl_sc_reset(sc);
    sc_flush_cache(sc);
    talloc_free(sc);
}

bool gl_sc_error_state(struct gl_shader_cache *sc)
{
    return sc->error_state;
}

void gl_sc_reset_error(struct gl_shader_cache *sc)
{
    sc->error_state = false;
}

void gl_sc_enable_extension(struct gl_shader_cache *sc, char *name)
{
    for (int n = 0; n < sc->num_exts; n++) {
        if (strcmp(sc->exts[n], name) == 0)
            return;
    }
    MP_TARRAY_APPEND(sc, sc->exts, sc->num_exts, talloc_strdup(sc, name));
}

#define bstr_xappend0(sc, b, s) bstr_xappend(sc, b, bstr0(s))

void gl_sc_add(struct gl_shader_cache *sc, const char *text)
{
    bstr_xappend0(sc, &sc->text, text);
}

void gl_sc_addf(struct gl_shader_cache *sc, const char *textf, ...)
{
    va_list ap;
    va_start(ap, textf);
    bstr_xappend_vasprintf(sc, &sc->text, textf, ap);
    va_end(ap);
}

void gl_sc_hadd(struct gl_shader_cache *sc, const char *text)
{
    bstr_xappend0(sc, &sc->header_text, text);
}

void gl_sc_haddf(struct gl_shader_cache *sc, const char *textf, ...)
{
    va_list ap;
    va_start(ap, textf);
    bstr_xappend_vasprintf(sc, &sc->header_text, textf, ap);
    va_end(ap);
}

void gl_sc_hadd_bstr(struct gl_shader_cache *sc, struct bstr text)
{
    bstr_xappend(sc, &sc->header_text, text);
}

void gl_sc_paddf(struct gl_shader_cache *sc, const char *textf, ...)
{
    va_list ap;
    va_start(ap, textf);
    bstr_xappend_vasprintf(sc, &sc->prelude_text, textf, ap);
    va_end(ap);
}

static struct sc_uniform *find_uniform(struct gl_shader_cache *sc,
                                       const char *name)
{
    for (int n = 0; n < sc->num_uniforms; n++) {
        if (strcmp(sc->uniforms[n].name, name) == 0)
            return &sc->uniforms[n];
    }
    // not found -> add it
    struct sc_uniform new = {
        .loc = -1,
        .name = talloc_strdup(NULL, name),
    };
    MP_TARRAY_APPEND(sc, sc->uniforms, sc->num_uniforms, new);
    return &sc->uniforms[sc->num_uniforms - 1];
}

static struct sc_buffer *find_buffer(struct gl_shader_cache *sc,
                                     const char *name)
{
    for (int n = 0; n < sc->num_buffers; n++) {
        if (strcmp(sc->buffers[n].name, name) == 0)
            return &sc->buffers[n];
    }
    // not found -> add it
    struct sc_buffer new = {
        .name = talloc_strdup(NULL, name),
    };
    MP_TARRAY_APPEND(sc, sc->buffers, sc->num_buffers, new);
    return &sc->buffers[sc->num_buffers - 1];
}

const char *mp_sampler_type(GLenum texture_target)
{
    switch (texture_target) {
    case GL_TEXTURE_1D:         return "sampler1D";
    case GL_TEXTURE_2D:         return "sampler2D";
    case GL_TEXTURE_RECTANGLE:  return "sampler2DRect";
    case GL_TEXTURE_EXTERNAL_OES: return "samplerExternalOES";
    case GL_TEXTURE_3D:         return "sampler3D";
    default: abort();
    }
}

void gl_sc_uniform_tex(struct gl_shader_cache *sc, char *name, GLenum target,
                       GLuint texture)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_i;
    u->size = 1;
    u->glsl_type = mp_sampler_type(target);
    u->v.i[0] = sc->next_texture_unit++;
    u->tex_target = target;
    u->tex_handle = texture;
}

void gl_sc_uniform_tex_ui(struct gl_shader_cache *sc, char *name, GLuint texture)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_i;
    u->size = 1;
    u->glsl_type = sc->gl->es ? "highp usampler2D" : "usampler2D";
    u->v.i[0] = sc->next_texture_unit++;
    u->tex_target = GL_TEXTURE_2D;
    u->tex_handle = texture;
}

void gl_sc_uniform_texture(struct gl_shader_cache *sc, char *name,
                           struct ra_tex *tex)
{
    struct ra_tex_gl *tex_gl = tex->priv;
    if (tex->params.format->ctype == RA_CTYPE_UINT) {
        gl_sc_uniform_tex_ui(sc, name, tex_gl->texture);
    } else {
        gl_sc_uniform_tex(sc, name, tex_gl->target, tex_gl->texture);
    }
}

static const char *mp_image2D_type(GLenum access)
{
    switch (access) {
    case GL_WRITE_ONLY: return "writeonly image2D";
    case GL_READ_ONLY:  return "readonly image2D";
    case GL_READ_WRITE: return "image2D";
    default: abort();
    }
}

void gl_sc_uniform_image2D(struct gl_shader_cache *sc, const char *name,
                           GLuint texture, GLuint iformat, GLenum access)
{
    gl_sc_enable_extension(sc, "GL_ARB_shader_image_load_store");

    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_i;
    u->size = 1;
    u->glsl_type = mp_image2D_type(access);
    u->v.i[0] = sc->next_image_unit++;
    u->img_handle = texture;
    u->img_access = access;
    u->img_iformat = iformat;
}

void gl_sc_ssbo(struct gl_shader_cache *sc, char *name, GLuint ssbo,
                char *format, ...)
{
    gl_sc_enable_extension(sc, "GL_ARB_shader_storage_buffer_object");

    struct sc_buffer *b = find_buffer(sc, name);
    b->binding = sc->next_buffer_binding++;
    b->ssbo = ssbo;
    b->format = format;

    va_list ap;
    va_start(ap, format);
    b->format = ta_vasprintf(sc, format, ap);
    va_end(ap);
}

void gl_sc_uniform_f(struct gl_shader_cache *sc, char *name, GLfloat f)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_f;
    u->size = 1;
    u->glsl_type = "float";
    u->v.f[0] = f;
}

void gl_sc_uniform_i(struct gl_shader_cache *sc, char *name, GLint i)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_i;
    u->size = 1;
    u->glsl_type = "int";
    u->v.i[0] = i;
}

void gl_sc_uniform_vec2(struct gl_shader_cache *sc, char *name, GLfloat f[2])
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_f;
    u->size = 2;
    u->glsl_type = "vec2";
    u->v.f[0] = f[0];
    u->v.f[1] = f[1];
}

void gl_sc_uniform_vec3(struct gl_shader_cache *sc, char *name, GLfloat f[3])
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_f;
    u->size = 3;
    u->glsl_type = "vec3";
    u->v.f[0] = f[0];
    u->v.f[1] = f[1];
    u->v.f[2] = f[2];
}

static void transpose2x2(float r[2 * 2])
{
    MPSWAP(float, r[0+2*1], r[1+2*0]);
}

void gl_sc_uniform_mat2(struct gl_shader_cache *sc, char *name,
                        bool transpose, GLfloat *v)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_m;
    u->size = 2;
    u->glsl_type = "mat2";
    for (int n = 0; n < 4; n++)
        u->v.f[n] = v[n];
    if (transpose)
        transpose2x2(&u->v.f[0]);
}

static void transpose3x3(float r[3 * 3])
{
    MPSWAP(float, r[0+3*1], r[1+3*0]);
    MPSWAP(float, r[0+3*2], r[2+3*0]);
    MPSWAP(float, r[1+3*2], r[2+3*1]);
}

void gl_sc_uniform_mat3(struct gl_shader_cache *sc, char *name,
                        bool transpose, GLfloat *v)
{
    struct sc_uniform *u = find_uniform(sc, name);
    u->type = UT_m;
    u->size = 3;
    u->glsl_type = "mat3";
    for (int n = 0; n < 9; n++)
        u->v.f[n] = v[n];
    if (transpose)
        transpose3x3(&u->v.f[0]);
}

// Tell the shader generator (and later gl_sc_draw_data()) about the vertex
// data layout and attribute names. The entries array is terminated with a {0}
// entry. The array memory must remain valid indefinitely (for now).
void gl_sc_set_vertex_format(struct gl_shader_cache *sc,
                             const struct gl_vao_entry *entries,
                             size_t vertex_size)
{
    sc->vertex_entries = entries;
    sc->vertex_size = vertex_size;
}

static const char *vao_glsl_type(const struct gl_vao_entry *e)
{
    // pretty dumb... too dumb, but works for us
    switch (e->num_elems) {
    case 1: return "float";
    case 2: return "vec2";
    case 3: return "vec3";
    case 4: return "vec4";
    default: abort();
    }
}

// Assumes program is current (gl->UseProgram(program)).
static void update_uniform(GL *gl, struct sc_entry *e, struct sc_uniform *u, int n)
{
    struct sc_cached_uniform *un = &e->uniforms[n];
    GLint loc = un->loc;
    if (loc < 0)
        return;
    switch (u->type) {
    case UT_i:
        assert(u->size == 1);
        if (memcmp(un->v.i, u->v.i, sizeof(u->v.i)) != 0) {
            memcpy(un->v.i, u->v.i, sizeof(u->v.i));
            gl->Uniform1i(loc, u->v.i[0]);
        }
        // For samplers: set the actual texture.
        if (u->tex_target) {
            gl->ActiveTexture(GL_TEXTURE0 + u->v.i[0]);
            gl->BindTexture(u->tex_target, u->tex_handle);
        }
        if (u->img_handle) {
            gl->BindImageTexture(u->v.i[0], u->img_handle, 0, GL_FALSE, 0,
                                 u->img_access, u->img_iformat);
        }
        break;
    case UT_f:
        if (memcmp(un->v.f, u->v.f, sizeof(u->v.f)) != 0) {
            memcpy(un->v.f, u->v.f, sizeof(u->v.f));
            switch (u->size) {
            case 1: gl->Uniform1f(loc, u->v.f[0]); break;
            case 2: gl->Uniform2f(loc, u->v.f[0], u->v.f[1]); break;
            case 3: gl->Uniform3f(loc, u->v.f[0], u->v.f[1], u->v.f[2]); break;
            case 4: gl->Uniform4f(loc, u->v.f[0], u->v.f[1], u->v.f[2],
                                  u->v.f[3]); break;
            default: abort();
            }
        }
        break;
    case UT_m:
        if (memcmp(un->v.f, u->v.f, sizeof(u->v.f)) != 0) {
            memcpy(un->v.f, u->v.f, sizeof(u->v.f));
            switch (u->size) {
            case 2: gl->UniformMatrix2fv(loc, 1, GL_FALSE, &u->v.f[0]); break;
            case 3: gl->UniformMatrix3fv(loc, 1, GL_FALSE, &u->v.f[0]); break;
            default: abort();
            }
        }
        break;
    default:
        abort();
    }
}

void gl_sc_set_cache_dir(struct gl_shader_cache *sc, struct mpv_global *global,
                         const char *dir)
{
    talloc_free(sc->cache_dir);
    sc->cache_dir = talloc_strdup(sc, dir);
    sc->global = global;
}

static const char *shader_typestr(GLenum type)
{
    switch (type) {
    case GL_VERTEX_SHADER:   return "vertex";
    case GL_FRAGMENT_SHADER: return "fragment";
    case GL_COMPUTE_SHADER:  return "compute";
    default: abort();
    }
}

static void compile_attach_shader(struct gl_shader_cache *sc, GLuint program,
                                  GLenum type, const char *source)
{
    GL *gl = sc->gl;

    GLuint shader = gl->CreateShader(type);
    gl->ShaderSource(shader, 1, &source, NULL);
    gl->CompileShader(shader);
    GLint status = 0;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint log_length = 0;
    gl->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

    int pri = status ? (log_length > 1 ? MSGL_V : MSGL_DEBUG) : MSGL_ERR;
    const char *typestr = shader_typestr(type);
    if (mp_msg_test(sc->log, pri)) {
        MP_MSG(sc, pri, "%s shader source:\n", typestr);
        mp_log_source(sc->log, pri, source);
    }
    if (log_length > 1) {
        GLchar *logstr = talloc_zero_size(NULL, log_length + 1);
        gl->GetShaderInfoLog(shader, log_length, NULL, logstr);
        MP_MSG(sc, pri, "%s shader compile log (status=%d):\n%s\n",
               typestr, status, logstr);
        talloc_free(logstr);
    }
    if (gl->GetTranslatedShaderSourceANGLE && mp_msg_test(sc->log, MSGL_DEBUG)) {
        GLint len = 0;
        gl->GetShaderiv(shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &len);
        if (len > 0) {
            GLchar *sstr = talloc_zero_size(NULL, len + 1);
            gl->GetTranslatedShaderSourceANGLE(shader, len, NULL, sstr);
            MP_DBG(sc, "Translated shader:\n");
            mp_log_source(sc->log, MSGL_DEBUG, sstr);
        }
    }

    gl->AttachShader(program, shader);
    gl->DeleteShader(shader);

    if (!status)
        sc->error_state = true;
}

static void link_shader(struct gl_shader_cache *sc, GLuint program)
{
    GL *gl = sc->gl;
    gl->LinkProgram(program);
    GLint status = 0;
    gl->GetProgramiv(program, GL_LINK_STATUS, &status);
    GLint log_length = 0;
    gl->GetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

    int pri = status ? (log_length > 1 ? MSGL_V : MSGL_DEBUG) : MSGL_ERR;
    if (mp_msg_test(sc->log, pri)) {
        GLchar *logstr = talloc_zero_size(NULL, log_length + 1);
        gl->GetProgramInfoLog(program, log_length, NULL, logstr);
        MP_MSG(sc, pri, "shader link log (status=%d): %s\n", status, logstr);
        talloc_free(logstr);
    }

    if (!status)
        sc->error_state = true;
}

// either 'compute' or both 'vertex' and 'frag' are needed
static GLuint compile_program(struct gl_shader_cache *sc, struct bstr *vertex,
                              struct bstr *frag, struct bstr *compute)
{
    GL *gl = sc->gl;
    GLuint prog = gl->CreateProgram();
    if (compute)
        compile_attach_shader(sc, prog, GL_COMPUTE_SHADER, compute->start);
    if (vertex && frag) {
        compile_attach_shader(sc, prog, GL_VERTEX_SHADER, vertex->start);
        compile_attach_shader(sc, prog, GL_FRAGMENT_SHADER, frag->start);
        for (int n = 0; sc->vertex_entries[n].name; n++) {
            char *vname = mp_tprintf(80, "vertex_%s", sc->vertex_entries[n].name);
            gl->BindAttribLocation(prog, n, vname);
        }
    }
    link_shader(sc, prog);
    return prog;
}

static GLuint load_program(struct gl_shader_cache *sc, struct bstr *vertex,
                           struct bstr *frag, struct bstr *compute)
{
    GL *gl = sc->gl;

    MP_VERBOSE(sc, "new shader program:\n");
    if (sc->header_text.len) {
        MP_VERBOSE(sc, "header:\n");
        mp_log_source(sc->log, MSGL_V, sc->header_text.start);
        MP_VERBOSE(sc, "body:\n");
    }
    if (sc->text.len)
        mp_log_source(sc->log, MSGL_V, sc->text.start);

    if (!sc->cache_dir || !sc->cache_dir[0] || !gl->ProgramBinary)
        return compile_program(sc, vertex, frag, compute);

    // Try to load it from a disk cache, or compiling + saving it.

    GLuint prog = 0;
    void *tmp = talloc_new(NULL);
    char *dir = mp_get_user_path(tmp, sc->global, sc->cache_dir);

    struct AVSHA *sha = av_sha_alloc();
    if (!sha)
        abort();
    av_sha_init(sha, 256);

    if (vertex)
        av_sha_update(sha, vertex->start, vertex->len + 1);
    if (frag)
        av_sha_update(sha, frag->start, frag->len + 1);
    if (compute)
        av_sha_update(sha, compute->start, compute->len + 1);

    // In theory, the array could change order, breaking old binaries.
    for (int n = 0; sc->vertex_entries[n].name; n++) {
        av_sha_update(sha, sc->vertex_entries[n].name,
                      strlen(sc->vertex_entries[n].name) + 1);
    }

    uint8_t hash[256 / 8];
    av_sha_final(sha, hash);
    av_free(sha);

    char hashstr[256 / 8 * 2 + 1];
    for (int n = 0; n < 256 / 8; n++)
        snprintf(hashstr + n * 2, sizeof(hashstr) - n * 2, "%02X", hash[n]);

    const char *header = "mpv shader cache v1\n";
    size_t header_size = strlen(header) + 4;

    char *filename = mp_path_join(tmp, dir, hashstr);
    if (stat(filename, &(struct stat){0}) == 0) {
        MP_VERBOSE(sc, "Trying to load shader from disk...\n");
        struct bstr cachedata = stream_read_file(filename, tmp, sc->global,
                                                 1000000000); // 1 GB
        if (cachedata.len > header_size) {
            GLenum format = AV_RL32(cachedata.start + header_size - 4);
            prog = gl->CreateProgram();
            gl_check_error(gl, sc->log, "before loading program");
            gl->ProgramBinary(prog, format, cachedata.start + header_size,
                                            cachedata.len - header_size);
            gl->GetError(); // discard potential useless error
            GLint status = 0;
            gl->GetProgramiv(prog, GL_LINK_STATUS, &status);
            if (!status) {
                gl->DeleteProgram(prog);
                prog = 0;
            }
        }
        MP_VERBOSE(sc, "Loading cached shader %s.\n", prog ? "ok" : "failed");
    }

    if (!prog) {
        prog = compile_program(sc, vertex, frag, compute);

        GLint size = 0;
        gl->GetProgramiv(prog, GL_PROGRAM_BINARY_LENGTH, &size);
        uint8_t *buffer = talloc_size(tmp, size + header_size);
        GLsizei actual_size = 0;
        GLenum binary_format = 0;
        gl->GetProgramBinary(prog, size, &actual_size, &binary_format,
                             buffer + header_size);
        memcpy(buffer, header, header_size - 4);
        AV_WL32(buffer + header_size - 4, binary_format);

        if (actual_size) {
            mp_mkdirp(dir);

            MP_VERBOSE(sc, "Writing shader cache file: %s\n", filename);
            FILE *out = fopen(filename, "wb");
            if (out) {
                fwrite(buffer, header_size + actual_size, 1, out);
                fclose(out);
            }
        }
    }

    talloc_free(tmp);
    return prog;
}

#define ADD(x, ...) bstr_xappend_asprintf(sc, (x), __VA_ARGS__)
#define ADD_BSTR(x, s) bstr_xappend(sc, (x), (s))

// 1. Generate vertex and fragment shaders from the fragment shader text added
//    with gl_sc_add(). The generated shader program is cached (based on the
//    text), so actual compilation happens only the first time.
// 2. Update the uniforms and textures set with gl_sc_uniform_*.
// 3. Make the new shader program current (glUseProgram()).
// After that, you render, and then you call gc_sc_reset(), which does:
// 1. Unbind the program and all textures.
// 2. Reset the sc state and prepare for a new shader program. (All uniforms
//    and fragment operations needed for the next program have to be re-added.)
// The return value is a mp_pass_perf containing performance metrics for the
// execution of the generated shader. (Note: execution is measured up until
// the corresponding gl_sc_reset call)
// 'type' can be either GL_FRAGMENT_SHADER or GL_COMPUTE_SHADER
struct mp_pass_perf gl_sc_generate(struct gl_shader_cache *sc, GLenum type)
{
    GL *gl = sc->gl;

    // gl_sc_reset() must be called after ending the previous render process,
    // and before starting a new one.
    assert(!sc->needs_reset);

    // gl_sc_set_vertex_format() must always be called
    assert(sc->vertex_entries);

    for (int n = 0; n < MP_ARRAY_SIZE(sc->tmp); n++)
        sc->tmp[n].len = 0;

    // set up shader text (header + uniforms + body)
    bstr *header = &sc->tmp[0];
    ADD(header, "#version %d%s\n", gl->glsl_version, gl->es >= 300 ? " es" : "");
    if (type == GL_COMPUTE_SHADER) {
        // This extension cannot be enabled in fragment shader. Enable it as
        // an exception for compute shader.
        ADD(header, "#extension GL_ARB_compute_shader : enable\n");
    }
    for (int n = 0; n < sc->num_exts; n++)
        ADD(header, "#extension %s : enable\n", sc->exts[n]);
    if (gl->es) {
        ADD(header, "precision mediump float;\n");
        ADD(header, "precision mediump sampler2D;\n");
        if (gl->mpgl_caps & MPGL_CAP_3D_TEX)
            ADD(header, "precision mediump sampler3D;\n");
    }

    if (gl->glsl_version >= 130) {
        ADD(header, "#define texture1D texture\n");
        ADD(header, "#define texture3D texture\n");
    } else {
        ADD(header, "#define texture texture2D\n");
    }

    // Additional helpers.
    ADD(header, "#define LUT_POS(x, lut_size)"
                " mix(0.5 / (lut_size), 1.0 - 0.5 / (lut_size), (x))\n");

    char *vert_in = gl->glsl_version >= 130 ? "in" : "attribute";
    char *vert_out = gl->glsl_version >= 130 ? "out" : "varying";
    char *frag_in = gl->glsl_version >= 130 ? "in" : "varying";

    struct bstr *vert = NULL, *frag = NULL, *comp = NULL;

    if (type == GL_FRAGMENT_SHADER) {
        // vertex shader: we don't use the vertex shader, so just setup a
        // dummy, which passes through the vertex array attributes.
        bstr *vert_head = &sc->tmp[1];
        ADD_BSTR(vert_head, *header);
        bstr *vert_body = &sc->tmp[2];
        ADD(vert_body, "void main() {\n");
        bstr *frag_vaos = &sc->tmp[3];
        for (int n = 0; sc->vertex_entries[n].name; n++) {
            const struct gl_vao_entry *e = &sc->vertex_entries[n];
            const char *glsl_type = vao_glsl_type(e);
            if (strcmp(e->name, "position") == 0) {
                // setting raster pos. requires setting gl_Position magic variable
                assert(e->num_elems == 2 && e->type == GL_FLOAT);
                ADD(vert_head, "%s vec2 vertex_position;\n", vert_in);
                ADD(vert_body, "gl_Position = vec4(vertex_position, 1.0, 1.0);\n");
            } else {
                ADD(vert_head, "%s %s vertex_%s;\n", vert_in, glsl_type, e->name);
                ADD(vert_head, "%s %s %s;\n", vert_out, glsl_type, e->name);
                ADD(vert_body, "%s = vertex_%s;\n", e->name, e->name);
                ADD(frag_vaos, "%s %s %s;\n", frag_in, glsl_type, e->name);
            }
        }
        ADD(vert_body, "}\n");
        vert = vert_head;
        ADD_BSTR(vert, *vert_body);

        // fragment shader; still requires adding used uniforms and VAO elements
        frag = &sc->tmp[4];
        ADD_BSTR(frag, *header);
        if (gl->glsl_version >= 130)
            ADD(frag, "out vec4 out_color;\n");
        ADD_BSTR(frag, *frag_vaos);
        for (int n = 0; n < sc->num_uniforms; n++) {
            struct sc_uniform *u = &sc->uniforms[n];
            ADD(frag, "uniform %s %s;\n", u->glsl_type, u->name);
        }

        ADD_BSTR(frag, sc->prelude_text);
        ADD_BSTR(frag, sc->header_text);

        ADD(frag, "void main() {\n");
        // we require _all_ frag shaders to write to a "vec4 color"
        ADD(frag, "vec4 color = vec4(0.0, 0.0, 0.0, 1.0);\n");
        ADD_BSTR(frag, sc->text);
        if (gl->glsl_version >= 130) {
            ADD(frag, "out_color = color;\n");
        } else {
            ADD(frag, "gl_FragColor = color;\n");
        }
        ADD(frag, "}\n");
    }

    if (type == GL_COMPUTE_SHADER) {
        comp = &sc->tmp[4];
        ADD_BSTR(comp, *header);

        for (int n = 0; n < sc->num_uniforms; n++) {
            struct sc_uniform *u = &sc->uniforms[n];
            ADD(comp, "uniform %s %s;\n", u->glsl_type, u->name);
        }

        for (int n = 0; n < sc->num_buffers; n++) {
            struct sc_buffer *b = &sc->buffers[n];
            ADD(comp, "layout(std430, binding=%d) buffer %s { %s };\n",
                b->binding, b->name, b->format);
        }

        ADD_BSTR(comp, sc->prelude_text);
        ADD_BSTR(comp, sc->header_text);

        ADD(comp, "void main() {\n");
        ADD(comp, "vec4 color = vec4(0.0, 0.0, 0.0, 1.0);\n"); // convenience
        ADD_BSTR(comp, sc->text);
        ADD(comp, "}\n");
    }

    struct sc_entry *entry = NULL;
    for (int n = 0; n < sc->num_entries; n++) {
        struct sc_entry *cur = &sc->entries[n];
        if (frag && !bstr_equals(cur->frag, *frag))
            continue;
        if (vert && !bstr_equals(cur->vert, *vert))
            continue;
        if (comp && !bstr_equals(cur->comp, *comp))
            continue;
        entry = cur;
        break;
    }
    if (!entry) {
        if (sc->num_entries == SC_MAX_ENTRIES)
            sc_flush_cache(sc);
        MP_TARRAY_GROW(sc, sc->entries, sc->num_entries);
        entry = &sc->entries[sc->num_entries++];
        *entry = (struct sc_entry){
            .vert = vert ? bstrdup(NULL, *vert) : (struct bstr){0},
            .frag = frag ? bstrdup(NULL, *frag) : (struct bstr){0},
            .comp = comp ? bstrdup(NULL, *comp) : (struct bstr){0},
            .timer = gl_timer_create(gl),
        };
    }
    // build shader program and cache the locations of the uniform variables
    if (!entry->gl_shader) {
        entry->gl_shader = load_program(sc, vert, frag, comp);
        entry->num_uniforms = 0;
        for (int n = 0; n < sc->num_uniforms; n++) {
            struct sc_cached_uniform un = {
                .loc = gl->GetUniformLocation(entry->gl_shader,
                                              sc->uniforms[n].name),
            };
            MP_TARRAY_APPEND(sc, entry->uniforms, entry->num_uniforms, un);
        }
        assert(!entry->vao.vao);
        gl_vao_init(&entry->vao, gl, sc->vertex_size, sc->vertex_entries);
    }

    gl->UseProgram(entry->gl_shader);

    assert(sc->num_uniforms == entry->num_uniforms);

    for (int n = 0; n < sc->num_uniforms; n++)
        update_uniform(gl, entry, &sc->uniforms[n], n);
    for (int n = 0; n < sc->num_buffers; n++) {
        struct sc_buffer *b = &sc->buffers[n];
        gl->BindBufferBase(GL_SHADER_STORAGE_BUFFER, b->binding, b->ssbo);
    }

    gl->ActiveTexture(GL_TEXTURE0);

    gl_timer_start(entry->timer);
    sc->needs_reset = true;
    sc->current_shader = entry;

    return gl_timer_measure(entry->timer);
}

// Draw the vertex data (as described by the gl_vao_entry entries) in ptr
// to the screen. num is the number of vertexes. prim is usually GL_TRIANGLES.
// gl_sc_generate() must have been called before this. Some additional setup
// might be needed (like setting the viewport).
void gl_sc_draw_data(struct gl_shader_cache *sc, GLenum prim, void *ptr,
                     size_t num)
{
    assert(ptr);
    assert(sc->current_shader);

    gl_vao_draw_data(&sc->current_shader->vao, prim, ptr, num);
}
