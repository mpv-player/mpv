/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <libavutil/common.h>

#include "formats.h"
#include "utils.h"
#include "osd.h"

// glBlendFuncSeparate() arguments
static const int blend_factors[SUBBITMAP_COUNT][4] = {
    [SUBBITMAP_LIBASS] = {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                          GL_ONE,       GL_ONE_MINUS_SRC_ALPHA},
    [SUBBITMAP_RGBA] =   {GL_ONE,       GL_ONE_MINUS_SRC_ALPHA,
                          GL_ONE,       GL_ONE_MINUS_SRC_ALPHA},
};

struct vertex {
    float position[2];
    float texcoord[2];
    uint8_t ass_color[4];
};

static const struct gl_vao_entry vertex_vao[] = {
    {"position",    2, GL_FLOAT,         false, offsetof(struct vertex, position)},
    {"texcoord" ,   2, GL_FLOAT,         false, offsetof(struct vertex, texcoord)},
    {"ass_color",   4, GL_UNSIGNED_BYTE, true,  offsetof(struct vertex, ass_color)},
    {0}
};

struct mpgl_osd_part {
    enum sub_bitmap_format format;
    int change_id;
    GLuint texture;
    int w, h;
    struct gl_pbo_upload pbo;
    int num_subparts;
    int prev_num_subparts;
    struct sub_bitmap *subparts;
    struct vertex *vertices;
};

struct mpgl_osd {
    struct mp_log *log;
    struct osd_state *osd;
    GL *gl;
    bool use_pbo;
    struct mpgl_osd_part *parts[MAX_OSD_PARTS];
    const struct gl_format *fmt_table[SUBBITMAP_COUNT];
    bool formats[SUBBITMAP_COUNT];
    struct gl_vao vao;
    int64_t change_counter;
    // temporary
    int stereo_mode;
    struct mp_osd_res osd_res;
    void *scratch;
};

struct mpgl_osd *mpgl_osd_init(GL *gl, struct mp_log *log, struct osd_state *osd)
{
    struct mpgl_osd *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct mpgl_osd) {
        .log = log,
        .osd = osd,
        .gl = gl,
        .scratch = talloc_zero_size(ctx, 1),
    };

    ctx->fmt_table[SUBBITMAP_LIBASS] = gl_find_unorm_format(gl, 1, 1);
    ctx->fmt_table[SUBBITMAP_RGBA]   = gl_find_unorm_format(gl, 1, 4);

    for (int n = 0; n < MAX_OSD_PARTS; n++)
        ctx->parts[n] = talloc_zero(ctx, struct mpgl_osd_part);

    for (int n = 0; n < SUBBITMAP_COUNT; n++)
        ctx->formats[n] = !!ctx->fmt_table[n];

    gl_vao_init(&ctx->vao, gl, sizeof(struct vertex), vertex_vao);

    return ctx;
}

void mpgl_osd_destroy(struct mpgl_osd *ctx)
{
    if (!ctx)
        return;

    GL *gl = ctx->gl;

    gl_vao_uninit(&ctx->vao);

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct mpgl_osd_part *p = ctx->parts[n];
        gl->DeleteTextures(1, &p->texture);
        gl_pbo_upload_uninit(&p->pbo);
    }
    talloc_free(ctx);
}

void mpgl_osd_set_options(struct mpgl_osd *ctx, bool pbo)
{
    ctx->use_pbo = pbo;
}

static int next_pow2(int v)
{
    for (int x = 0; x < 30; x++) {
        if ((1 << x) >= v)
            return 1 << x;
    }
    return INT_MAX;
}

static bool upload_osd(struct mpgl_osd *ctx, struct mpgl_osd_part *osd,
                       struct sub_bitmaps *imgs)
{
    GL *gl = ctx->gl;
    bool ok = false;

    assert(imgs->packed);

    int req_w = next_pow2(imgs->packed_w);
    int req_h = next_pow2(imgs->packed_h);

    const struct gl_format *fmt = ctx->fmt_table[imgs->format];
    assert(fmt);

    if (!osd->texture)
        gl->GenTextures(1, &osd->texture);

    gl->BindTexture(GL_TEXTURE_2D, osd->texture);

    if (req_w > osd->w || req_h > osd->h || osd->format != imgs->format) {
        osd->format = imgs->format;
        osd->w = FFMAX(32, req_w);
        osd->h = FFMAX(32, req_h);

        MP_VERBOSE(ctx, "Reallocating OSD texture to %dx%d.\n", osd->w, osd->h);

        GLint max_wh;
        gl->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_wh);

        if (osd->w > max_wh || osd->h > max_wh) {
            MP_ERR(ctx, "OSD bitmaps do not fit on a surface with the maximum "
                   "supported size %dx%d.\n", max_wh, max_wh);
            goto done;
        }

        gl->TexImage2D(GL_TEXTURE_2D, 0, fmt->internal_format, osd->w, osd->h,
                       0, fmt->format, fmt->type, NULL);

        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    gl_pbo_upload_tex(&osd->pbo, gl, ctx->use_pbo, GL_TEXTURE_2D, fmt->format,
                      fmt->type, osd->w, osd->h, imgs->packed->planes[0],
                      imgs->packed->stride[0], 0, 0,
                      imgs->packed_w, imgs->packed_h);
    ok = true;

done:
    gl->BindTexture(GL_TEXTURE_2D, 0);
    return ok;
}

static void gen_osd_cb(void *pctx, struct sub_bitmaps *imgs)
{
    struct mpgl_osd *ctx = pctx;

    if (imgs->num_parts == 0 || !ctx->formats[imgs->format])
        return;

    struct mpgl_osd_part *osd = ctx->parts[imgs->render_index];

    bool ok = true;
    if (imgs->change_id != osd->change_id) {
        if (!upload_osd(ctx, osd, imgs))
            ok = false;

        osd->change_id = imgs->change_id;
        ctx->change_counter += 1;
    }
    osd->num_subparts = ok ? imgs->num_parts : 0;

    MP_TARRAY_GROW(osd, osd->subparts, osd->num_subparts);
    memcpy(osd->subparts, imgs->parts,
           osd->num_subparts * sizeof(osd->subparts[0]));
}

static void write_quad(struct vertex *va, struct gl_transform t,
                       float x0, float y0, float x1, float y1,
                       float tx0, float ty0, float tx1, float ty1,
                       float tex_w, float tex_h, const uint8_t color[4])
{
    gl_transform_vec(t, &x0, &y0);
    gl_transform_vec(t, &x1, &y1);

#define COLOR_INIT {color[0], color[1], color[2], color[3]}
    va[0] = (struct vertex){ {x0, y0}, {tx0 / tex_w, ty0 / tex_h}, COLOR_INIT };
    va[1] = (struct vertex){ {x0, y1}, {tx0 / tex_w, ty1 / tex_h}, COLOR_INIT };
    va[2] = (struct vertex){ {x1, y0}, {tx1 / tex_w, ty0 / tex_h}, COLOR_INIT };
    va[3] = (struct vertex){ {x1, y1}, {tx1 / tex_w, ty1 / tex_h}, COLOR_INIT };
    va[4] = va[2];
    va[5] = va[1];
#undef COLOR_INIT
}

static int generate_verts(struct mpgl_osd_part *part, struct gl_transform t)
{
    int num_vertices = part->num_subparts * 6;
    MP_TARRAY_GROW(part, part->vertices, num_vertices);

    for (int n = 0; n < part->num_subparts; n++) {
        struct sub_bitmap *b = &part->subparts[n];
        struct vertex *va = part->vertices;

        // NOTE: the blend color is used with SUBBITMAP_LIBASS only, so it
        //       doesn't matter that we upload garbage for the other formats
        uint32_t c = b->libass.color;
        uint8_t color[4] = { c >> 24, (c >> 16) & 0xff,
                            (c >> 8) & 0xff, 255 - (c & 0xff) };

        write_quad(&va[n * 6], t,
                   b->x, b->y, b->x + b->dw, b->y + b->dh,
                   b->src_x, b->src_y, b->src_x + b->w, b->src_y + b->h,
                   part->w, part->h, color);
    }

    return num_vertices;
}

static void draw_part(struct mpgl_osd *ctx, int index, struct gl_transform t)
{
    GL *gl = ctx->gl;
    struct mpgl_osd_part *part = ctx->parts[index];

    int num_vertices = generate_verts(part, t);
    if (!num_vertices)
        return;

    gl->Enable(GL_BLEND);
    gl->BindTexture(GL_TEXTURE_2D, part->texture);

    const int *factors = &blend_factors[part->format][0];
    gl->BlendFuncSeparate(factors[0], factors[1], factors[2], factors[3]);

    gl_vao_draw_data(&ctx->vao, GL_TRIANGLES, part->vertices, num_vertices);

    gl->BindTexture(GL_TEXTURE_2D, 0);
    gl->Disable(GL_BLEND);
}

// number of screen divisions per axis (x=0, y=1) for the current 3D mode
static void get_3d_side_by_side(int stereo_mode, int div[2])
{
    div[0] = div[1] = 1;
    switch (stereo_mode) {
    case MP_STEREO3D_SBS2L:
    case MP_STEREO3D_SBS2R: div[0] = 2; break;
    case MP_STEREO3D_AB2R:
    case MP_STEREO3D_AB2L:  div[1] = 2; break;
    }
}

void mpgl_osd_draw_part(struct mpgl_osd *ctx, int vp_w, int vp_h, int index)
{
    int div[2];
    get_3d_side_by_side(ctx->stereo_mode, div);

    ctx->gl->Viewport(0, 0, vp_w, abs(vp_h));

    for (int x = 0; x < div[0]; x++) {
        for (int y = 0; y < div[1]; y++) {
            struct gl_transform t;
            gl_transform_ortho(&t, 0, vp_w, 0, vp_h);

            float a_x = ctx->osd_res.w * x;
            float a_y = ctx->osd_res.h * y;
            t.t[0] += a_x * t.m[0][0] + a_y * t.m[1][0];
            t.t[1] += a_x * t.m[0][1] + a_y * t.m[1][1];

            draw_part(ctx, index, t);
        }
    }
}

enum sub_bitmap_format mpgl_osd_get_part_format(struct mpgl_osd *ctx, int index)
{
    assert(index >= 0 && index < MAX_OSD_PARTS);
    return ctx->parts[index]->format;
}

struct gl_vao *mpgl_osd_get_vao(struct mpgl_osd *ctx)
{
    return &ctx->vao;
}

static void set_res(struct mpgl_osd *ctx, struct mp_osd_res res, int stereo_mode)
{
    int div[2];
    get_3d_side_by_side(stereo_mode, div);

    res.w /= div[0];
    res.h /= div[1];
    ctx->osd_res = res;
}

void mpgl_osd_generate(struct mpgl_osd *ctx, struct mp_osd_res res, double pts,
                       int stereo_mode, int draw_flags)
{
    for (int n = 0; n < MAX_OSD_PARTS; n++)
        ctx->parts[n]->num_subparts = 0;

    set_res(ctx, res, stereo_mode);

    osd_draw(ctx->osd, ctx->osd_res, pts, draw_flags, ctx->formats, gen_osd_cb, ctx);
    ctx->stereo_mode = stereo_mode;

    // Parts going away does not necessarily result in gen_osd_cb() being called
    // (not even with num_parts==0), so check this separately.
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct mpgl_osd_part *part = ctx->parts[n];
        if (part->num_subparts !=  part->prev_num_subparts)
            ctx->change_counter += 1;
        part->prev_num_subparts = part->num_subparts;
    }
}

// See osd_resize() for remarks. This function is an optional optimization too.
void mpgl_osd_resize(struct mpgl_osd *ctx, struct mp_osd_res res, int stereo_mode)
{
    set_res(ctx, res, stereo_mode);
    osd_resize(ctx->osd, ctx->osd_res);
}

int64_t mpgl_get_change_counter(struct mpgl_osd *ctx)
{
    return ctx->change_counter;
}
