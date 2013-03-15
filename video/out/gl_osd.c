/*
 * This file is part of mplayer.
 *
 * mplayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <assert.h>
#include <libavutil/common.h>

#include "bitmap_packer.h"

#include "gl_osd.h"

struct osd_fmt_entry {
    GLint internal_format;
    GLint format;
    GLenum type;
};

// glBlendFunc() arguments
static const int blend_factors[SUBBITMAP_COUNT][2] = {
    [SUBBITMAP_LIBASS] = {GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA},
    [SUBBITMAP_RGBA] =   {GL_ONE,       GL_ONE_MINUS_SRC_ALPHA},
};

static const struct osd_fmt_entry osd_to_gl3_formats[SUBBITMAP_COUNT] = {
    [SUBBITMAP_LIBASS] = {GL_RED,   GL_RED,   GL_UNSIGNED_BYTE},
    [SUBBITMAP_RGBA] =   {GL_RGBA,  GL_BGRA,  GL_UNSIGNED_BYTE},
};

static const struct osd_fmt_entry osd_to_gl_legacy_formats[SUBBITMAP_COUNT] = {
    [SUBBITMAP_LIBASS] = {GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE},
    [SUBBITMAP_RGBA] =   {GL_RGBA,  GL_BGRA,  GL_UNSIGNED_BYTE},
};

struct mpgl_osd *mpgl_osd_init(GL *gl, bool legacy)
{
    GLint max_texture_size;
    gl->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

    struct mpgl_osd *ctx = talloc_ptrtype(NULL, ctx);
    *ctx = (struct mpgl_osd) {
        .gl = gl,
        .fmt_table = legacy ? osd_to_gl_legacy_formats : osd_to_gl3_formats,
        .scratch = talloc_zero_size(ctx, 1),
    };

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct mpgl_osd_part *p = talloc_ptrtype(ctx, p);
        *p = (struct mpgl_osd_part) {
            .packer = talloc_struct(p, struct bitmap_packer, {
                .w_max = max_texture_size,
                .h_max = max_texture_size,
            }),
        };
        ctx->parts[n] = p;
    }

    for (int n = 0; n < SUBBITMAP_COUNT; n++)
        ctx->formats[n] = ctx->fmt_table[n].type != 0;

    return ctx;
}

void mpgl_osd_destroy(struct mpgl_osd *ctx)
{
    GL *gl = ctx->gl;

    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct mpgl_osd_part *p = ctx->parts[n];
        gl->DeleteTextures(1, &p->texture);
        if (gl->DeleteBuffers)
            gl->DeleteBuffers(1, &p->buffer);
    }
    talloc_free(ctx);
}

static bool upload_pbo(struct mpgl_osd *ctx, struct mpgl_osd_part *osd,
                       struct sub_bitmaps *imgs)
{
    GL *gl = ctx->gl;
    bool success = true;
    struct osd_fmt_entry fmt = ctx->fmt_table[imgs->format];
    int pix_stride = glFmt2bpp(fmt.format, fmt.type);

    if (!osd->buffer) {
        gl->GenBuffers(1, &osd->buffer);
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, osd->buffer);
        gl->BufferData(GL_PIXEL_UNPACK_BUFFER, osd->w * osd->h * pix_stride,
                        NULL, GL_DYNAMIC_COPY);
        gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, osd->buffer);
    char *data = gl->MapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (!data) {
        success = false;
    } else {
        struct pos bb[2];
        packer_get_bb(osd->packer, bb);
        size_t stride = osd->w * pix_stride;
        packer_copy_subbitmaps(osd->packer, imgs, data, pix_stride, stride);
        if (!gl->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER))
            success = false;
        glUploadTex(gl, GL_TEXTURE_2D, fmt.format, fmt.type, NULL, stride,
                    bb[0].x, bb[0].y, bb[1].x - bb[0].x, bb[1].y - bb[0].y,
                    0);
    }
    gl->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    if (!success) {
        mp_msg(MSGT_VO, MSGL_FATAL, "[gl] Error: can't upload subtitles! "
                                    "Remove the 'pbo' suboption.\n");
    }

    return success;
}

static void upload_tex(struct mpgl_osd *ctx, struct mpgl_osd_part *osd,
                       struct sub_bitmaps *imgs)
{
    struct osd_fmt_entry fmt = ctx->fmt_table[imgs->format];
    if (osd->packer->padding) {
        struct pos bb[2];
        packer_get_bb(osd->packer, bb);
        glClearTex(ctx->gl, GL_TEXTURE_2D, fmt.format, fmt.type,
                   bb[0].x, bb[0].y, bb[1].x - bb[0].y, bb[1].y - bb[0].y,
                   0, &ctx->scratch);
    }
    for (int n = 0; n < osd->packer->count; n++) {
        struct sub_bitmap *s = &imgs->parts[n];
        struct pos p = osd->packer->result[n];

        glUploadTex(ctx->gl, GL_TEXTURE_2D, fmt.format, fmt.type,
                    s->bitmap, s->stride, p.x, p.y, s->w, s->h, 0);
    }
}

static bool upload_osd(struct mpgl_osd *ctx, struct mpgl_osd_part *osd,
                       struct sub_bitmaps *imgs)
{
    GL *gl = ctx->gl;

    // assume 2x2 filter on scaling
    osd->packer->padding = ctx->scaled || imgs->scaled;
    int r = packer_pack_from_subbitmaps(osd->packer, imgs);
    if (r < 0) {
        mp_msg(MSGT_VO, MSGL_ERR, "[gl] OSD bitmaps do not fit on "
               "a surface with the maximum supported size %dx%d.\n",
               osd->packer->w_max, osd->packer->h_max);
        return false;
    }

    struct osd_fmt_entry fmt = ctx->fmt_table[imgs->format];
    assert(fmt.type != 0);

    if (!osd->texture)
        gl->GenTextures(1, &osd->texture);

    gl->BindTexture(GL_TEXTURE_2D, osd->texture);

    if (osd->packer->w > osd->w || osd->packer->h > osd->h
        || osd->format != imgs->format)
    {
        osd->format = imgs->format;
        osd->w = FFMAX(32, osd->packer->w);
        osd->h = FFMAX(32, osd->packer->h);

        gl->TexImage2D(GL_TEXTURE_2D, 0, fmt.internal_format, osd->w, osd->h,
                       0, fmt.format, fmt.type, NULL);

        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if (gl->DeleteBuffers)
            gl->DeleteBuffers(1, &osd->buffer);
        osd->buffer = 0;
    }

    bool uploaded = false;
    if (ctx->use_pbo)
        uploaded = upload_pbo(ctx, osd, imgs);
    if (!uploaded)
        upload_tex(ctx, osd, imgs);

    gl->BindTexture(GL_TEXTURE_2D, 0);

    return true;
}

static struct mpgl_osd_part *osd_generate(struct mpgl_osd *ctx,
                                          struct sub_bitmaps *imgs)
{
    if (imgs->num_parts == 0 || !ctx->formats[imgs->format])
        return NULL;

    struct mpgl_osd_part *osd = ctx->parts[imgs->render_index];

    if (imgs->bitmap_pos_id != osd->bitmap_pos_id) {
        if (imgs->bitmap_id != osd->bitmap_id) {
            if (!upload_osd(ctx, osd, imgs))
                osd->packer->count = 0;
        }

        osd->bitmap_id = imgs->bitmap_id;
        osd->bitmap_pos_id = imgs->bitmap_pos_id;
        osd->num_vertices = 0;
    }

    return osd->packer->count ? osd : NULL;
}

void mpgl_osd_set_gl_state(struct mpgl_osd *ctx, struct mpgl_osd_part *p)
{
    GL *gl = ctx->gl;

    gl->BindTexture(GL_TEXTURE_2D, p->texture);
    gl->Enable(GL_BLEND);
    gl->BlendFunc(blend_factors[p->format][0], blend_factors[p->format][1]);
}

void mpgl_osd_unset_gl_state(struct mpgl_osd *ctx, struct mpgl_osd_part *p)
{
    GL *gl = ctx->gl;

    gl->Disable(GL_BLEND);
    gl->BindTexture(GL_TEXTURE_2D, 0);
}

static void reset(struct mpgl_osd *ctx)
{
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct mpgl_osd_part *p = ctx->parts[n];
        p->active = false;
    }
}

struct draw_cb_closure {
    struct mpgl_osd *ctx;
    void (*cb)(void *ctx, struct mpgl_osd_part *part, struct sub_bitmaps *imgs);
    void *cb_ctx;
};

static void draw_cb(void *pctx, struct sub_bitmaps *imgs)
{
    struct draw_cb_closure *c = pctx;
    struct mpgl_osd_part *part = osd_generate(c->ctx, imgs);
    if (!part)
        return;
    part->active = true;
    c->cb(c->cb_ctx, part, imgs);
}

void mpgl_osd_draw_cb(struct mpgl_osd *ctx,
                      struct osd_state *osd,
                      struct mp_osd_res res,
                      void (*cb)(void *ctx, struct mpgl_osd_part *part,
                                 struct sub_bitmaps *imgs),
                      void *cb_ctx)
{
    struct draw_cb_closure c = {ctx, cb, cb_ctx};
    reset(ctx);
    osd_draw(osd, res, osd->vo_pts, 0, ctx->formats, draw_cb, &c);
}

void mpgl_osd_redraw_cb(struct mpgl_osd *ctx,
                        void (*cb)(void *ctx, struct mpgl_osd_part *part,
                                   struct sub_bitmaps *imgs),
                        void *cb_ctx)
{
    for (int n = 0; n < MAX_OSD_PARTS; n++) {
        struct mpgl_osd_part *p = ctx->parts[n];
        if (p->active)
            cb(cb_ctx, p, NULL);
    }
}

struct vertex {
    float position[2];
    uint8_t color[4];
    float texcoord[2];
};

static void draw_legacy_cb(void *pctx, struct sub_bitmaps *imgs)
{
    struct mpgl_osd *ctx = pctx;
    struct mpgl_osd_part *osd = osd_generate(ctx, imgs);
    if (!osd)
        return;

    if (!osd->num_vertices) {
        // 2 triangles primitives per quad = 6 vertices per quad
        // not using GL_QUADS, as it is deprecated in OpenGL 3.x and later
        osd->vertices = talloc_realloc(osd, osd->vertices, struct vertex,
                                       osd->packer->count * 6);

        struct vertex *va = osd->vertices;
        float tex_w = osd->w;
        float tex_h = osd->h;

        for (int n = 0; n < osd->packer->count; n++) {
            struct sub_bitmap *b = &imgs->parts[n];
            struct pos p = osd->packer->result[n];

            uint32_t c = imgs->format == SUBBITMAP_LIBASS
                         ? b->libass.color : 0xFFFFFF00;
            uint8_t color[4] = { c >> 24, (c >> 16) & 0xff,
                                 (c >> 8) & 0xff, 255 - (c & 0xff) };

            float x0 = b->x;
            float y0 = b->y;
            float x1 = b->x + b->dw;
            float y1 = b->y + b->dh;
            float tx0 = p.x / tex_w;
            float ty0 = p.y / tex_h;
            float tx1 = (p.x + b->w) / tex_w;
            float ty1 = (p.y + b->h) / tex_h;

#define COLOR_INIT {color[0], color[1], color[2], color[3]}
            struct vertex *v = &va[osd->num_vertices];
            v[0] = (struct vertex) { {x0, y0}, COLOR_INIT, {tx0, ty0} };
            v[1] = (struct vertex) { {x0, y1}, COLOR_INIT, {tx0, ty1} };
            v[2] = (struct vertex) { {x1, y0}, COLOR_INIT, {tx1, ty0} };
            v[3] = (struct vertex) { {x1, y1}, COLOR_INIT, {tx1, ty1} };
            v[4] = v[2];
            v[5] = v[1];
#undef COLOR_INIT
            osd->num_vertices += 6;
        }
    }

    GL *gl = ctx->gl;

    struct vertex *va = osd->vertices;
    size_t stride = sizeof(va[0]);

    gl->VertexPointer(2, GL_FLOAT, stride, &va[0].position[0]);
    gl->ColorPointer(4, GL_UNSIGNED_BYTE, stride, &va[0].color[0]);
    gl->TexCoordPointer(2, GL_FLOAT, stride, &va[0].texcoord[0]);

    gl->EnableClientState(GL_VERTEX_ARRAY);
    gl->EnableClientState(GL_TEXTURE_COORD_ARRAY);
    gl->EnableClientState(GL_COLOR_ARRAY);

    mpgl_osd_set_gl_state(ctx, osd);
    gl->DrawArrays(GL_TRIANGLES, 0, osd->num_vertices);
    mpgl_osd_unset_gl_state(ctx, osd);

    gl->DisableClientState(GL_VERTEX_ARRAY);
    gl->DisableClientState(GL_TEXTURE_COORD_ARRAY);
    gl->DisableClientState(GL_COLOR_ARRAY);
}

void mpgl_osd_draw_legacy(struct mpgl_osd *ctx, struct osd_state *osd,
                          struct mp_osd_res res)
{
    osd_draw(osd, res, osd->vo_pts, 0, ctx->formats, draw_legacy_cb, ctx);
}
