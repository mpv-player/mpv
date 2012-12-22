/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * You can alternatively redistribute this file and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>

#include "config.h"
#include "talloc.h"
#include "core/mp_msg.h"
#include "core/subopt-helper.h"
#include "vo.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "geometry.h"
#include "sub/sub.h"

#include "gl_common.h"
#include "gl_osd.h"
#include "aspect.h"
#include "video/memcpy_pic.h"

//for gl_priv.use_yuv
#define MASK_ALL_YUV (~(1 << YUV_CONVERSION_NONE))
#define MASK_NOT_COMBINERS (~((1 << YUV_CONVERSION_NONE) | (1 << YUV_CONVERSION_COMBINERS)))
#define MASK_GAMMA_SUPPORT (MASK_NOT_COMBINERS & ~(1 << YUV_CONVERSION_FRAGMENT))

struct gl_priv {
    MPGLContext *glctx;
    GL *gl;

    int allow_sw;

    int use_osd;
    int scaled_osd;
    struct mpgl_osd *osd;
    int osd_color;

    int use_ycbcr;
    int use_yuv;
    struct mp_csp_details colorspace;
    int is_yuv;
    int lscale;
    int cscale;
    float filter_strength;
    float noise_strength;
    int yuvconvtype;
    int use_rectangle;
    int err_shown;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_format;
    int many_fmts;
    int have_texture_rg;
    int max_tex_component_size;
    int ati_hack;
    int force_pbo;
    int use_glFinish;
    int swap_interval;
    GLenum target;
    GLint texfmt;
    GLenum gl_format;
    GLenum gl_type;
    GLuint buffer;
    GLuint buffer_uv[2];
    int buffersize;
    int buffersize_uv;
    void *bufferptr;
    void *bufferptr_uv[2];
    GLuint fragprog;
    GLuint default_texs[22];
    char *custom_prog;
    char *custom_tex;
    int custom_tlin;
    int custom_trect;
    int mipmap_gen;
    int stereo_mode;

    struct mp_csp_equalizer video_eq;

    int texture_width;
    int texture_height;
    int mpi_flipped;
    int vo_flipped;
    int ass_border_x, ass_border_y;

    unsigned int slice_height;
};

static void resize(struct vo *vo, int x, int y)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    mp_msg(MSGT_VO, MSGL_V, "[gl] Resize: %dx%d\n", x, y);
    gl->Viewport(0, 0, x, y);

    gl->MatrixMode(GL_PROJECTION);
    gl->LoadIdentity();
    p->ass_border_x = p->ass_border_y = 0;
    if (aspect_scaling()) {
        int new_w, new_h;
        GLdouble scale_x, scale_y;
        aspect(vo, &new_w, &new_h, A_WINZOOM);
        panscan_calc_windowed(vo);
        new_w += vo->panscan_x;
        new_h += vo->panscan_y;
        scale_x = (GLdouble)new_w / (GLdouble)x;
        scale_y = (GLdouble)new_h / (GLdouble)y;
        gl->Scaled(scale_x, scale_y, 1);
        p->ass_border_x = (vo->dwidth - new_w) / 2;
        p->ass_border_y = (vo->dheight - new_h) / 2;
    }
    gl->Ortho(0, p->image_width, p->image_height, 0, -1, 1);

    gl->MatrixMode(GL_MODELVIEW);
    gl->LoadIdentity();

    gl->Clear(GL_COLOR_BUFFER_BIT);
    vo->want_redraw = true;
}

static void texSize(struct vo *vo, int w, int h, int *texw, int *texh)
{
    struct gl_priv *p = vo->priv;

    if (p->use_rectangle) {
        *texw = w;
        *texh = h;
    } else {
        *texw = 32;
        while (*texw < w)
            *texw *= 2;
        *texh = 32;
        while (*texh < h)
            *texh *= 2;
    }
    if (p->ati_hack)
        *texw = (*texw + 511) & ~511;
}

//! maximum size of custom fragment program
#define MAX_CUSTOM_PROG_SIZE (1024 * 1024)
static void update_yuvconv(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    int xs, ys, depth;
    struct mp_csp_params cparams = { .colorspace = p->colorspace };
    mp_csp_copy_equalizer_values(&cparams, &p->video_eq);
    gl_conversion_params_t params = {
        p->target, p->yuvconvtype, cparams,
        p->texture_width, p->texture_height, 0, 0, p->filter_strength,
        p->noise_strength
    };
    mp_get_chroma_shift(p->image_format, &xs, &ys, &depth);
    params.chrom_texw = params.texw >> xs;
    params.chrom_texh = params.texh >> ys;
    params.csp_params.input_bits = depth;
    params.csp_params.texture_bits = depth+7 & ~7;
    glSetupYUVConversion(gl, &params);
    if (p->custom_prog) {
        FILE *f = fopen(p->custom_prog, "rb");
        if (!f) {
            mp_msg(MSGT_VO, MSGL_WARN,
                   "[gl] Could not read customprog %s\n", p->custom_prog);
        } else {
            char *prog = calloc(1, MAX_CUSTOM_PROG_SIZE + 1);
            fread(prog, 1, MAX_CUSTOM_PROG_SIZE, f);
            fclose(f);
            loadGPUProgram(gl, GL_FRAGMENT_PROGRAM, prog);
            free(prog);
        }
        gl->ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM, 0,
                                  1.0 / p->texture_width,
                                  1.0 / p->texture_height,
                                  p->texture_width, p->texture_height);
    }
    if (p->custom_tex) {
        FILE *f = fopen(p->custom_tex, "rb");
        if (!f) {
            mp_msg(MSGT_VO, MSGL_WARN,
                   "[gl] Could not read customtex %s\n", p->custom_tex);
        } else {
            int width, height, maxval;
            gl->ActiveTexture(GL_TEXTURE3);
            if (glCreatePPMTex(gl, p->custom_trect ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D,
                               0, p->custom_tlin ? GL_LINEAR : GL_NEAREST,
                               f, &width, &height, &maxval)) {
                gl->ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM, 1,
                                          1.0 / width, 1.0 / height,
                                          width, height);
            } else
                mp_msg(MSGT_VO, MSGL_WARN,
                       "[gl] Error parsing customtex %s\n", p->custom_tex);
            fclose(f);
            gl->ActiveTexture(GL_TEXTURE0);
        }
    }
}

static void draw_osd(struct vo *vo, struct osd_state *osd)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;
    assert(p->osd);

    if (!p->use_osd)
        return;

    if (!p->scaled_osd) {
        gl->MatrixMode(GL_PROJECTION);
        gl->PushMatrix();
        gl->LoadIdentity();
        gl->Ortho(0, vo->dwidth, vo->dheight, 0, -1, 1);
    }

    gl->Color4ub((p->osd_color >> 16) & 0xff, (p->osd_color >> 8) & 0xff,
                 p->osd_color & 0xff, 0xff - (p->osd_color >> 24));

    struct mp_osd_res res = {
        .w = vo->dwidth,
        .h = vo->dheight,
        .display_par = vo->monitor_par,
        .video_par = vo->aspdat.par,
    };
    if (p->scaled_osd) {
        res.w = p->image_width;
        res.h = p->image_height;
    } else if (aspect_scaling()) {
        res.ml = res.mr = p->ass_border_x;
        res.mt = res.mb = p->ass_border_y;
    }

    mpgl_osd_draw_legacy(p->osd, osd, res);

    if (!p->scaled_osd)
        gl->PopMatrix();
}

/**
 * \brief uninitialize OpenGL context, freeing textures, buffers etc.
 */
static void uninitGl(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    if (!gl)
        return;

    int i = 0;
    if (gl->DeletePrograms && p->fragprog)
        gl->DeletePrograms(1, &p->fragprog);
    p->fragprog = 0;
    while (p->default_texs[i] != 0)
        i++;
    if (i)
        gl->DeleteTextures(i, p->default_texs);
    p->default_texs[0] = 0;
    if (p->osd)
        mpgl_osd_destroy(p->osd);
    p->osd = NULL;
    p->buffer = 0;
    p->buffersize = 0;
    p->bufferptr = NULL;
    if (gl->DeleteBuffers && p->buffer_uv[0])
        gl->DeleteBuffers(2, p->buffer_uv);
    p->buffer_uv[0] = p->buffer_uv[1] = 0;
    p->buffersize_uv = 0;
    p->bufferptr_uv[0] = p->bufferptr_uv[1] = 0;
    p->err_shown = 0;
}

static void autodetectGlExtensions(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    const char *extensions = gl->GetString(GL_EXTENSIONS);
    const char *vendor     = gl->GetString(GL_VENDOR);
    const char *version    = gl->GetString(GL_VERSION);
    const char *renderer   = gl->GetString(GL_RENDERER);
    int is_ati = vendor && strstr(vendor, "ATI") != NULL;
    int ati_broken_pbo = 0;
    mp_msg(MSGT_VO, MSGL_V, "[gl] Running on OpenGL '%s' by '%s', version '%s'\n",
           renderer, vendor, version);
    if (is_ati && strncmp(version, "2.1.", 4) == 0) {
        int ver = atoi(version + 4);
        mp_msg(MSGT_VO, MSGL_V, "[gl] Detected ATI driver version: %i\n", ver);
        ati_broken_pbo = ver && ver < 8395;
    }
    if (p->ati_hack == -1)
        p->ati_hack = ati_broken_pbo;
    if (p->force_pbo == -1) {
        p->force_pbo = 0;
        if (extensions && strstr(extensions, "_pixel_buffer_object"))
            p->force_pbo = is_ati;
    }
    p->have_texture_rg = extensions && strstr(extensions, "GL_ARB_texture_rg");
    if (p->use_rectangle == -1) {
        p->use_rectangle = 0;
        if (extensions) {
//      if (strstr(extensions, "_texture_non_power_of_two"))
            if (strstr(extensions, "_texture_rectangle"))
                p->use_rectangle = renderer
                    && strstr(renderer, "Mesa DRI R200") ? 1 : 0;
        }
    }
    if (p->use_osd == -1)
        p->use_osd = gl->BindTexture != NULL;
    if (p->use_yuv == -1)
        p->use_yuv = glAutodetectYUVConversion(gl);

    int eq_caps = 0;
    int yuv_mask = (1 << p->use_yuv);
    if (!(yuv_mask & MASK_NOT_COMBINERS)) {
        // combiners
        eq_caps = (1 << MP_CSP_EQ_HUE) | (1 << MP_CSP_EQ_SATURATION);
    } else if (yuv_mask & MASK_ALL_YUV) {
        eq_caps = MP_CSP_EQ_CAPS_COLORMATRIX;
        if (yuv_mask & MASK_GAMMA_SUPPORT)
            eq_caps |= MP_CSP_EQ_CAPS_GAMMA;
    }
    p->video_eq.capabilities = eq_caps;

    {
        int target = p->use_rectangle == 1 ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
        GLint gl_texfmt;
        GLenum gl_format, gl_type;
        glFindFormat(IMGFMT_420P16, p->have_texture_rg, NULL, &gl_texfmt,
                     &gl_format, &gl_type);
        glCreateClearTex(gl, target, gl_texfmt, gl_format, gl_type,
                         GL_LINEAR, 64, 64, 0);
        int tex_size_token = p->have_texture_rg ? GL_TEXTURE_RED_SIZE
                                                : GL_TEXTURE_INTENSITY_SIZE;
        GLint size = 8;
        gl->GetTexLevelParameteriv(target, 0, tex_size_token, &size);
        mp_msg(MSGT_VO, MSGL_V, "[gl] 16 bit texture depth: %d.\n", size);
        p->max_tex_component_size = size;
    }

    if (is_ati && (p->lscale == 1 || p->lscale == 2 || p->cscale == 1 || p->cscale == 2))
        mp_msg(MSGT_VO, MSGL_WARN, "[gl] Selected scaling mode may be broken on"
               " ATI cards.\n"
               "Tell _them_ to fix GL_REPEAT if you have issues.\n");
    mp_msg(MSGT_VO, MSGL_V, "[gl] Settings after autodetection: ati-hack = %i, "
           "force-pbo = %i, rectangle = %i, yuv = %i\n",
           p->ati_hack, p->force_pbo, p->use_rectangle, p->use_yuv);
}

static GLint get_scale_type(struct vo *vo, int chroma)
{
    struct gl_priv *p = vo->priv;

    int nearest = (chroma ? p->cscale : p->lscale) & 64;
    if (nearest)
        return p->mipmap_gen ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
    return p->mipmap_gen ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR;
}

// Return the high byte of the value that represents white in chroma (U/V)
static int get_chroma_clear_val(int bit_depth)
{
    return 1 << (bit_depth - 1 & 7);
}

/**
 * \brief Initialize a (new or reused) OpenGL context.
 * set global gl-related variables to their default values
 */
static int initGl(struct vo *vo, uint32_t d_width, uint32_t d_height)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    GLint scale_type = get_scale_type(vo, 0);
    autodetectGlExtensions(vo);
    p->target = p->use_rectangle == 1 ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
    p->yuvconvtype = SET_YUV_CONVERSION(p->use_yuv) |
                     SET_YUV_LUM_SCALER(p->lscale) |
                     SET_YUV_CHROM_SCALER(p->cscale);

    texSize(vo, p->image_width, p->image_height,
            &p->texture_width, &p->texture_height);

    gl->Disable(GL_BLEND);
    gl->Disable(GL_DEPTH_TEST);
    gl->DepthMask(GL_FALSE);
    gl->Disable(GL_CULL_FACE);
    gl->Enable(p->target);
    gl->DrawBuffer(GL_BACK);
    gl->TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    mp_msg(MSGT_VO, MSGL_V, "[gl] Creating %dx%d texture...\n",
           p->texture_width, p->texture_height);

    glCreateClearTex(gl, p->target, p->texfmt, p->gl_format,
                     p->gl_type, scale_type,
                     p->texture_width, p->texture_height, 0);

    if (p->mipmap_gen)
        gl->TexParameteri(p->target, GL_GENERATE_MIPMAP, GL_TRUE);

    if (p->is_yuv) {
        int i;
        int xs, ys, depth;
        scale_type = get_scale_type(vo, 1);
        mp_get_chroma_shift(p->image_format, &xs, &ys, &depth);
        int clear = get_chroma_clear_val(depth);
        gl->GenTextures(21, p->default_texs);
        p->default_texs[21] = 0;
        for (i = 0; i < 7; i++) {
            gl->ActiveTexture(GL_TEXTURE1 + i);
            gl->BindTexture(GL_TEXTURE_2D, p->default_texs[i]);
            gl->BindTexture(GL_TEXTURE_RECTANGLE, p->default_texs[i + 7]);
            gl->BindTexture(GL_TEXTURE_3D, p->default_texs[i + 14]);
        }
        gl->ActiveTexture(GL_TEXTURE1);
        glCreateClearTex(gl, p->target, p->texfmt, p->gl_format,
                         p->gl_type, scale_type,
                         p->texture_width >> xs, p->texture_height >> ys,
                         clear);
        if (p->mipmap_gen)
            gl->TexParameteri(p->target, GL_GENERATE_MIPMAP, GL_TRUE);
        gl->ActiveTexture(GL_TEXTURE2);
        glCreateClearTex(gl, p->target, p->texfmt, p->gl_format,
                         p->gl_type, scale_type,
                         p->texture_width >> xs, p->texture_height >> ys,
                         clear);
        if (p->mipmap_gen)
            gl->TexParameteri(p->target, GL_GENERATE_MIPMAP, GL_TRUE);
        gl->ActiveTexture(GL_TEXTURE0);
        gl->BindTexture(p->target, 0);
    }
    if (p->is_yuv || p->custom_prog) {
        if ((MASK_NOT_COMBINERS & (1 << p->use_yuv)) || p->custom_prog) {
            if (!gl->GenPrograms || !gl->BindProgram)
                mp_msg(MSGT_VO, MSGL_ERR,
                       "[gl] fragment program functions missing!\n");
            else {
                gl->GenPrograms(1, &p->fragprog);
                gl->BindProgram(GL_FRAGMENT_PROGRAM, p->fragprog);
            }
        }
        update_yuvconv(vo);
    }

    p->osd = mpgl_osd_init(gl, true);
    p->osd->scaled = p->scaled_osd;

    resize(vo, d_width, d_height);

    gl->ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    if (gl->SwapInterval && p->swap_interval >= 0)
        gl->SwapInterval(p->swap_interval);
    return 1;
}

static bool create_window(struct vo *vo, uint32_t d_width, uint32_t d_height,
                          uint32_t flags)
{
    struct gl_priv *p = vo->priv;

    if (p->stereo_mode == GL_3D_QUADBUFFER)
        flags |= VOFLAG_STEREO;

    int mpgl_caps = MPGL_CAP_GL_LEGACY;
    if (!p->allow_sw)
        mpgl_caps |= MPGL_CAP_NO_SW;
    return mpgl_create_window(p->glctx, mpgl_caps, d_width, d_height, flags);
}

static int config(struct vo *vo, uint32_t width, uint32_t height,
                  uint32_t d_width, uint32_t d_height, uint32_t flags,
                  uint32_t format)
{
    struct gl_priv *p = vo->priv;

    int xs, ys;
    p->image_height = height;
    p->image_width = width;
    p->image_format = format;
    p->is_yuv = mp_get_chroma_shift(p->image_format, &xs, &ys, NULL) > 0;
    p->is_yuv |= (xs << 8) | (ys << 16);
    glFindFormat(format, p->have_texture_rg, NULL, &p->texfmt, &p->gl_format,
                 &p->gl_type);

    p->vo_flipped = !!(flags & VOFLAG_FLIPPING);

    if (vo->config_count)
        uninitGl(vo);

    if (!create_window(vo, d_width, d_height, flags))
        return -1;

    initGl(vo, vo->dwidth, vo->dheight);

    return 0;
}

static void check_events(struct vo *vo)
{
    struct gl_priv *p = vo->priv;

    int e = p->glctx->check_events(vo);
    if (e & VO_EVENT_REINIT) {
        uninitGl(vo);
        initGl(vo, vo->dwidth, vo->dheight);
    }
    if (e & VO_EVENT_RESIZE)
        resize(vo, vo->dwidth, vo->dheight);
    if (e & VO_EVENT_EXPOSE)
        vo->want_redraw = true;
}

static void do_render(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

//  Enable(GL_TEXTURE_2D);
//  BindTexture(GL_TEXTURE_2D, texture_id);

    gl->Color4f(1, 1, 1, 1);
    if (p->is_yuv || p->custom_prog)
        glEnableYUVConversion(gl, p->target, p->yuvconvtype);
    if (p->stereo_mode) {
        glEnable3DLeft(gl, p->stereo_mode);
        glDrawTex(gl, 0, 0, p->image_width, p->image_height,
                  0, 0, p->image_width >> 1, p->image_height,
                  p->texture_width, p->texture_height,
                  p->use_rectangle == 1, p->is_yuv,
                  p->mpi_flipped ^ p->vo_flipped);
        glEnable3DRight(gl, p->stereo_mode);
        glDrawTex(gl, 0, 0, p->image_width, p->image_height,
                  p->image_width >> 1, 0, p->image_width >> 1,
                  p->image_height, p->texture_width, p->texture_height,
                  p->use_rectangle == 1, p->is_yuv,
                  p->mpi_flipped ^ p->vo_flipped);
        glDisable3D(gl, p->stereo_mode);
    } else {
        glDrawTex(gl, 0, 0, p->image_width, p->image_height,
                  0, 0, p->image_width, p->image_height,
                  p->texture_width, p->texture_height,
                  p->use_rectangle == 1, p->is_yuv,
                  p->mpi_flipped ^ p->vo_flipped);
    }
    if (p->is_yuv || p->custom_prog)
        glDisableYUVConversion(gl, p->target, p->yuvconvtype);
}

static void flip_page(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    if (p->use_glFinish)
        gl->Finish();
    p->glctx->swapGlBuffers(p->glctx);
    if (aspect_scaling())
        gl->Clear(GL_COLOR_BUFFER_BIT);
}

static bool get_image(struct vo *vo, mp_image_t *mpi, int *th, bool *cplane)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    bool common_plane = false;

    int needed_size;
    if (!gl->GenBuffers || !gl->BindBuffer || !gl->BufferData || !gl->MapBuffer) {
        if (!p->err_shown)
            mp_msg(MSGT_VO, MSGL_ERR, "[gl] extensions missing for dr\n"
                   "Expect a _major_ speed penalty\n");
        p->err_shown = 1;
        return false;
    }
    int width = mpi->w, height = mpi->h;
    if (p->ati_hack) {
        width = p->texture_width;
        height = p->texture_height;
    }
    mpi->stride[0] = width * mpi->bpp / 8;
    needed_size = mpi->stride[0] * height;
    if (!p->buffer)
        gl->GenBuffers(1, &p->buffer);
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer);
    if (needed_size > p->buffersize) {
        p->buffersize = needed_size;
        gl->BufferData(GL_PIXEL_UNPACK_BUFFER, p->buffersize,
                        NULL, GL_DYNAMIC_DRAW);
    }
    if (!p->bufferptr)
        p->bufferptr = gl->MapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    mpi->planes[0] = p->bufferptr;
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    if (!mpi->planes[0]) {
        if (!p->err_shown)
            mp_msg(MSGT_VO, MSGL_ERR, "[gl] could not acquire buffer for dr\n"
                   "Expect a _major_ speed penalty\n");
        p->err_shown = 1;
        return false;
    }
    if (p->is_yuv) {
        // planar YUV
        int xs, ys, component_bits;
        mp_get_chroma_shift(p->image_format, &xs, &ys, &component_bits);
        int bp = (component_bits + 7) / 8;
        common_plane = true;
        mpi->stride[0] = width * bp;
        mpi->planes[1] = mpi->planes[0] + mpi->stride[0] * height;
        mpi->stride[1] = (width >> xs) * bp;
        mpi->planes[2] = mpi->planes[1] + mpi->stride[1] * (height >> ys);
        mpi->stride[2] = (width >> xs) * bp;
        if (p->ati_hack) {
            common_plane = false;
            if (!p->buffer_uv[0])
                gl->GenBuffers(2, p->buffer_uv);
            int buffer_size = mpi->stride[1] * height;
            if (buffer_size > p->buffersize_uv) {
                gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[0]);
                gl->BufferData(GL_PIXEL_UNPACK_BUFFER, buffer_size, NULL,
                               GL_DYNAMIC_DRAW);
                gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[1]);
                gl->BufferData(GL_PIXEL_UNPACK_BUFFER, buffer_size, NULL,
                               GL_DYNAMIC_DRAW);
                p->buffersize_uv = buffer_size;
            }
            if (!p->bufferptr_uv[0]) {
                gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[0]);
                p->bufferptr_uv[0] = gl->MapBuffer(GL_PIXEL_UNPACK_BUFFER,
                                                   GL_WRITE_ONLY);
                gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[1]);
                p->bufferptr_uv[1] = gl->MapBuffer(GL_PIXEL_UNPACK_BUFFER,
                                                   GL_WRITE_ONLY);
            }
            mpi->planes[1] = p->bufferptr_uv[0];
            mpi->planes[2] = p->bufferptr_uv[1];
        }
    }
    *th = height;
    *cplane = common_plane;
    return true;
}

static void clear_border(struct vo *vo, uint8_t *dst, int start, int stride,
                         int height, int full_height, int value)
{
    int right_border = stride - start;
    int bottom_border = full_height - height;
    while (height > 0) {
        if (right_border > 0)
            memset(dst + start, value, right_border);
        dst += stride;
        height--;
    }
    if (bottom_border > 0)
        memset(dst, value, stride * bottom_border);
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    int slice = p->slice_height;
    int stride[3];
    unsigned char *planes[3];
    mp_image_t mpi2 = *mpi;
    int w = mpi->w, h = mpi->h;
    int th = h;
    bool common_plane = false;
    bool pbo = false;
    mpi2.flags = 0;
    if (p->force_pbo && !p->bufferptr
        && get_image(vo, &mpi2, &th, &common_plane))
    {
        int bp = mpi->bpp / 8;
        int xs, ys, component_bits;
        mp_get_chroma_shift(p->image_format, &xs, &ys, &component_bits);
        if (p->is_yuv)
            bp = (component_bits + 7) / 8;
        memcpy_pic(mpi2.planes[0], mpi->planes[0], mpi->w * bp, mpi->h,
                   mpi2.stride[0], mpi->stride[0]);
        int uv_bytes = (mpi->w >> xs) * bp;
        if (p->is_yuv) {
            memcpy_pic(mpi2.planes[1], mpi->planes[1], uv_bytes, mpi->h >> ys,
                       mpi2.stride[1], mpi->stride[1]);
            memcpy_pic(mpi2.planes[2], mpi->planes[2], uv_bytes, mpi->h >> ys,
                       mpi2.stride[2], mpi->stride[2]);
        }
        if (p->ati_hack) {
            // since we have to do a full upload we need to clear the borders
            clear_border(vo, mpi2.planes[0], mpi->w * bp, mpi2.stride[0],
                         mpi->h, th, 0);
            if (p->is_yuv) {
                int clear = get_chroma_clear_val(component_bits);
                clear_border(vo, mpi2.planes[1], uv_bytes, mpi2.stride[1],
                             mpi->h >> ys, th >> ys, clear);
                clear_border(vo, mpi2.planes[2], uv_bytes, mpi2.stride[2],
                             mpi->h >> ys, th >> ys, clear);
            }
        }
        mpi = &mpi2;
        pbo = true;
    }
    stride[0] = mpi->stride[0];
    stride[1] = mpi->stride[1];
    stride[2] = mpi->stride[2];
    planes[0] = mpi->planes[0];
    planes[1] = mpi->planes[1];
    planes[2] = mpi->planes[2];
    p->mpi_flipped = stride[0] < 0;
    if (pbo) {
        intptr_t base = (intptr_t)planes[0];
        if (p->ati_hack) {
            w = p->texture_width;
            h = p->texture_height;
        }
        if (p->mpi_flipped)
            base += (mpi->h - 1) * stride[0];
        planes[0] -= base;
        planes[1] -= base;
        planes[2] -= base;
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer);
        gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        p->bufferptr = NULL;
        if (!common_plane)
            planes[0] = planes[1] = planes[2] = NULL;
        slice = 0; // always "upload" full texture
    }
    glUploadTex(gl, p->target, p->gl_format, p->gl_type, planes[0],
                stride[0], 0, 0, w, h, slice);
    if (p->is_yuv) {
        int xs, ys;
        mp_get_chroma_shift(p->image_format, &xs, &ys, NULL);
        if (pbo && !common_plane) {
            gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[0]);
            gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            p->bufferptr_uv[0] = NULL;
        }
        gl->ActiveTexture(GL_TEXTURE1);
        glUploadTex(gl, p->target, p->gl_format, p->gl_type, planes[1],
                    stride[1], 0, 0, w >> xs, h >> ys, slice);
        if (pbo && !common_plane) {
            gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, p->buffer_uv[1]);
            gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            p->bufferptr_uv[1] = NULL;
        }
        gl->ActiveTexture(GL_TEXTURE2);
        glUploadTex(gl, p->target, p->gl_format, p->gl_type, planes[2],
                    stride[2], 0, 0, w >> xs, h >> ys, slice);
        gl->ActiveTexture(GL_TEXTURE0);
    }
    if (pbo) {
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    do_render(vo);
}

static mp_image_t *get_screenshot(struct vo *vo)
{
    struct gl_priv *p = vo->priv;
    GL *gl = p->gl;

    mp_image_t *image = mp_image_alloc(p->image_format, p->texture_width,
                                                        p->texture_height);

    glDownloadTex(gl, p->target, p->gl_format, p->gl_type, image->planes[0],
                  image->stride[0]);

    if (p->is_yuv) {
        gl->ActiveTexture(GL_TEXTURE1);
        glDownloadTex(gl, p->target, p->gl_format, p->gl_type, image->planes[1],
                      image->stride[1]);
        gl->ActiveTexture(GL_TEXTURE2);
        glDownloadTex(gl, p->target, p->gl_format, p->gl_type, image->planes[2],
                      image->stride[2]);
        gl->ActiveTexture(GL_TEXTURE0);
    }
    mp_image_set_size(image, p->image_width, p->image_height);
    mp_image_set_display_size(image, vo->aspdat.prew, vo->aspdat.preh);

    mp_image_set_colorspace_details(image, &p->colorspace);

    return image;
}

static int query_format(struct vo *vo, uint32_t format)
{
    struct gl_priv *p = vo->priv;

    int depth;
    int caps = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_FLIP;
    if (p->use_osd)
        caps |= VFCAP_OSD;
    if (format == IMGFMT_RGB24 || format == IMGFMT_RGBA)
        return caps;
    if (p->use_yuv && mp_get_chroma_shift(format, NULL, NULL, &depth) &&
        (depth == 8 || depth == 16 ||
         p->max_tex_component_size >= 16 && glYUVLargeRange(p->use_yuv)) &&
        (IMGFMT_IS_YUVP16_NE(format) || !IMGFMT_IS_YUVP16(format)))
        return caps;
    // HACK, otherwise we get only b&w with some filters (e.g. -vf eq)
    // ideally MPlayer should be fixed instead not to use Y800 when it has the choice
    if (!p->use_yuv && (format == IMGFMT_Y8 || format == IMGFMT_Y800))
        return 0;
    if (!p->use_ycbcr && (format == IMGFMT_UYVY || format == IMGFMT_YVYU))
        return 0;
    if (p->many_fmts &&
        glFindFormat(format, p->have_texture_rg, NULL, NULL, NULL, NULL))
        return caps;
    return 0;
}

static void uninit(struct vo *vo)
{
    struct gl_priv *p = vo->priv;

    uninitGl(vo);
    free(p->custom_prog);
    p->custom_prog = NULL;
    free(p->custom_tex);
    p->custom_tex = NULL;
    mpgl_uninit(p->glctx);
    p->glctx = NULL;
    p->gl = NULL;
}

static int backend_valid(void *arg)
{
    return mpgl_find_backend(*(const char **)arg) >= 0;
}

static int preinit(struct vo *vo, const char *arg)
{
    struct gl_priv *p = talloc_zero(vo, struct gl_priv);
    vo->priv = p;

    *p = (struct gl_priv) {
        .many_fmts = 1,
        .use_osd = -1,
        .use_yuv = -1,
        .colorspace = MP_CSP_DETAILS_DEFAULTS,
        .filter_strength = 0.5,
        .use_rectangle = -1,
        .ati_hack = -1,
        .force_pbo = -1,
        .swap_interval = vo_vsync,
        .custom_prog = NULL,
        .custom_tex = NULL,
        .custom_tlin = 1,
        .osd_color = 0xffffff,
    };

    char *backend_arg = NULL;

    //essentially unused; for legacy warnings only
    int user_colorspace = 0;
    int levelconv = -1;
    int aspect = -1;

    const opt_t subopts[] = {
        {"manyfmts",     OPT_ARG_BOOL, &p->many_fmts,    NULL},
        {"osd",          OPT_ARG_BOOL, &p->use_osd,      NULL},
        {"scaled-osd",   OPT_ARG_BOOL, &p->scaled_osd,   NULL},
        {"ycbcr",        OPT_ARG_BOOL, &p->use_ycbcr,    NULL},
        {"slice-height", OPT_ARG_INT,  &p->slice_height, int_non_neg},
        {"rectangle",    OPT_ARG_INT,  &p->use_rectangle,int_non_neg},
        {"yuv",          OPT_ARG_INT,  &p->use_yuv,      int_non_neg},
        {"lscale",       OPT_ARG_INT,  &p->lscale,       int_non_neg},
        {"cscale",       OPT_ARG_INT,  &p->cscale,       int_non_neg},
        {"filter-strength", OPT_ARG_FLOAT, &p->filter_strength, NULL},
        {"noise-strength", OPT_ARG_FLOAT, &p->noise_strength, NULL},
        {"ati-hack",     OPT_ARG_BOOL, &p->ati_hack,     NULL},
        {"force-pbo",    OPT_ARG_BOOL, &p->force_pbo,    NULL},
        {"glfinish",     OPT_ARG_BOOL, &p->use_glFinish, NULL},
        {"swapinterval", OPT_ARG_INT,  &p->swap_interval,NULL},
        {"customprog",   OPT_ARG_MSTRZ,&p->custom_prog,  NULL},
        {"customtex",    OPT_ARG_MSTRZ,&p->custom_tex,   NULL},
        {"customtlin",   OPT_ARG_BOOL, &p->custom_tlin,  NULL},
        {"customtrect",  OPT_ARG_BOOL, &p->custom_trect, NULL},
        {"mipmapgen",    OPT_ARG_BOOL, &p->mipmap_gen,   NULL},
        {"osdcolor",     OPT_ARG_INT,  &p->osd_color,    NULL},
        {"stereo",       OPT_ARG_INT,  &p->stereo_mode,  NULL},
        {"sw",           OPT_ARG_BOOL, &p->allow_sw,     NULL},
        {"backend",      OPT_ARG_MSTRZ,&backend_arg,     backend_valid},
        // Removed options.
        // They are only parsed to notify the user about the replacements.
        {"aspect",       OPT_ARG_BOOL, &aspect,          NULL},
        {"colorspace",   OPT_ARG_INT,  &user_colorspace, NULL},
        {"levelconv",    OPT_ARG_INT,  &levelconv,       NULL},
        {NULL}
    };

    if (subopt_parse(arg, subopts) != 0) {
        mp_msg(MSGT_VO, MSGL_FATAL,
               "\n-vo opengl_old command line help:\n"
               "Example: mpv -vo opengl_old:slice-height=4\n"
               "\nOptions:\n"
               "  nomanyfmts\n"
               "    Disable extended color formats for OpenGL 1.2 and later\n"
               "  slice-height=<0-...>\n"
               "    Slice size for texture transfer, 0 for whole image\n"
               "  noosd\n"
               "    Do not use OpenGL OSD code\n"
               "  scaled-osd\n"
               "    Render OSD at movie resolution and scale it\n"
               "  rectangle=<0,1,2>\n"
               "    0: use power-of-two textures\n"
               "    1: use texture_rectangle\n"
               "    2: use texture_non_power_of_two\n"
               "  ati-hack\n"
               "    Workaround ATI bug with PBOs\n"
               "  force-pbo\n"
               "    Force use of PBO even if this involves an extra memcpy\n"
               "  glfinish\n"
               "    Call glFinish() before swapping buffers\n"
               "  swapinterval=<n>\n"
               "    Interval in displayed frames between to buffer swaps.\n"
               "    1 is equivalent to enable VSYNC, 0 to disable VSYNC.\n"
               "    Requires GLX_SGI_swap_control support to work.\n"
               "  ycbcr\n"
               "    also try to use the GL_MESA_ycbcr_texture extension\n"
               "  yuv=<n>\n"
               "    0: use software YUV to RGB conversion.\n"
               "    1: deprecated, will use yuv=2 (used to be nVidia register combiners).\n"
               "    2: use fragment program.\n"
               "    3: use fragment program with gamma correction.\n"
               "    4: use fragment program with gamma correction via lookup.\n"
               "    5: use ATI-specific method (for older cards).\n"
               "    6: use lookup via 3D texture.\n"
               "  lscale=<n>\n"
               "    0: use standard bilinear scaling for luma.\n"
               "    1: use improved bicubic scaling for luma.\n"
               "    2: use cubic in X, linear in Y direction scaling for luma.\n"
               "    3: as 1 but without using a lookup texture.\n"
               "    4: experimental unsharp masking (sharpening).\n"
               "    5: experimental unsharp masking (sharpening) with larger radius.\n"
               "  cscale=<n>\n"
               "    as lscale but for chroma (2x slower with little visible effect).\n"
               "  filter-strength=<value>\n"
               "    set the effect strength for some lscale/cscale filters\n"
               "  noise-strength=<value>\n"
               "    set how much noise to add. 1.0 is suitable for dithering to 6 bit.\n"
               "  customprog=<filename>\n"
               "    use a custom YUV conversion program\n"
               "  customtex=<filename>\n"
               "    use a custom YUV conversion lookup texture\n"
               "  nocustomtlin\n"
               "    use GL_NEAREST scaling for customtex texture\n"
               "  customtrect\n"
               "    use texture_rectangle for customtex texture\n"
               "  mipmapgen\n"
               "    generate mipmaps for the video image (use with TXB in customprog)\n"
               "  osdcolor=<0xAARRGGBB>\n"
               "    use the given color for the OSD\n"
               "  stereo=<n>\n"
               "    0: normal display\n"
               "    1: side-by-side to red-cyan stereo\n"
               "    2: side-by-side to green-magenta stereo\n"
               "    3: side-by-side to quadbuffer stereo\n"
               "  sw\n"
               "    allow using a software renderer, if such is detected\n"
               "  backend=<sys>\n"
               "    auto: auto-select (default)\n"
               "    cocoa: Cocoa/OSX\n"
               "    win: Win32/WGL\n"
               "    x11: X11/GLX\n"
               "\n");
        return -1;
    }
    if (user_colorspace != 0 || levelconv != -1) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] \"colorspace\" and \"levelconv\" "
               "suboptions have been removed. Use options --colormatrix and"
               " --colormatrix-input-range/--colormatrix-output-range instead.\n");
        return -1;
    }
    if (aspect != -1) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] \"noaspect\" suboption has been "
               "removed. Use --noaspect instead.\n");
        return -1;
    }
    if (p->use_yuv == 1) {
        mp_msg(MSGT_VO, MSGL_WARN, "[gl] yuv=1 (nVidia register combiners) have"
               " been removed, using yuv=2 instead.\n");
        p->use_yuv = 2;
    }

    int backend = backend_arg ? mpgl_find_backend(backend_arg) : GLTYPE_AUTO;
    free(backend_arg);

    p->glctx = mpgl_init(backend, vo);
    if (!p->glctx)
        goto err_out;
    p->gl = p->glctx->gl;

    if (p->use_yuv == -1) {
        if (!create_window(vo, 320, 200, VOFLAG_HIDDEN))
            goto err_out;
        autodetectGlExtensions(vo);
        // We created a window to test whether the GL context supports hardware
        // acceleration and so on. Destroy that window to make sure all state
        // associated with it is lost.
        uninitGl(vo);
        if (!mpgl_destroy_window(p->glctx))
            goto err_out;
    }
    if (p->many_fmts)
        mp_msg(MSGT_VO, MSGL_INFO, "[gl] using extended formats. "
               "Use -vo gl:nomanyfmts if playback fails.\n");
    mp_msg(MSGT_VO, MSGL_V, "[gl] Using %d as slice height "
           "(0 means image height).\n", p->slice_height);

    return 0;

err_out:
    uninit(vo);
    return -1;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct gl_priv *p = vo->priv;

    switch (request) {
    case VOCTRL_ONTOP:
        if (!p->glctx->ontop)
            break;
        p->glctx->ontop(vo);
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        p->glctx->fullscreen(vo);
        resize(vo, vo->dwidth, vo->dheight);
        return VO_TRUE;
    case VOCTRL_BORDER:
        if (!p->glctx->border)
            break;
        p->glctx->border(vo);
        resize(vo, vo->dwidth, vo->dheight);
        return VO_TRUE;
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
        resize(vo, vo->dwidth, vo->dheight);
        return VO_TRUE;
    case VOCTRL_GET_EQUALIZER:
        if (p->is_yuv) {
            struct voctrl_get_equalizer_args *args = data;
            return mp_csp_equalizer_get(&p->video_eq, args->name, args->valueptr)
                   >= 0 ? VO_TRUE : VO_NOTIMPL;
        }
        break;
    case VOCTRL_SET_EQUALIZER:
        if (p->is_yuv) {
            struct voctrl_set_equalizer_args *args = data;
            if (mp_csp_equalizer_set(&p->video_eq, args->name, args->value) < 0)
                return VO_NOTIMPL;
            update_yuvconv(vo);
            vo->want_redraw = true;
            return VO_TRUE;
        }
        break;
    case VOCTRL_SET_YUV_COLORSPACE: {
        bool supports_csp = (1 << p->use_yuv) & MASK_NOT_COMBINERS;
        if (vo->config_count && supports_csp) {
            p->colorspace = *(struct mp_csp_details *)data;
            update_yuvconv(vo);
            vo->want_redraw = true;
        }
        return VO_TRUE;
    }
    case VOCTRL_GET_YUV_COLORSPACE:
        *(struct mp_csp_details *)data = p->colorspace;
        return VO_TRUE;
    case VOCTRL_UPDATE_SCREENINFO:
        if (!p->glctx->update_xinerama_info)
            break;
        p->glctx->update_xinerama_info(vo);
        return VO_TRUE;
    case VOCTRL_REDRAW_FRAME:
        do_render(vo);
        return true;
    case VOCTRL_PAUSE:
        if (!p->glctx->pause)
            break;
        p->glctx->pause(vo);
        return VO_TRUE;
    case VOCTRL_RESUME:
        if (!p->glctx->resume)
            break;
        p->glctx->resume(vo);
        return VO_TRUE;
    case VOCTRL_SCREENSHOT: {
        struct voctrl_screenshot_args *args = data;
        if (args->full_window)
            args->out_image = glGetWindowScreenshot(p->gl);
        else
            args->out_image = get_screenshot(vo);
        return true;
    }
    }
    return VO_NOTIMPL;
}

const struct vo_driver video_out_opengl_old = {
    .info = &(const vo_info_t) {
        "OpenGL",
        "opengl-old",
        "Reimar Doeffinger <Reimar.Doeffinger@gmx.de>",
        ""
    },
    .preinit = preinit,
    .query_format = query_format,
    .config = config,
    .control = control,
    .draw_image = draw_image,
    .draw_osd = draw_osd,
    .flip_page = flip_page,
    .check_events = check_events,
    .uninit = uninit,
};
