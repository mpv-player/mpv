#ifndef MPLAYER_GL_OSD_H
#define MPLAYER_GL_OSD_H

#include <stdbool.h>
#include <inttypes.h>

#include "gl_common.h"
#include "sub/sub.h"

struct mpgl_osd_part {
    enum sub_bitmap_format format;
    int bitmap_id, bitmap_pos_id;
    bool active;
    GLuint texture;
    int w, h;
    GLuint buffer;
    int num_vertices;
    void *vertices;
    struct bitmap_packer *packer;
};

struct mpgl_osd {
    GL *gl;
    bool use_pbo;
    bool scaled;
    struct mpgl_osd_part *parts[MAX_OSD_PARTS];
    const struct osd_fmt_entry *fmt_table;
    bool formats[SUBBITMAP_COUNT];
    void *scratch;
};

struct mpgl_osd *mpgl_osd_init(GL *gl, bool legacy);
void mpgl_osd_destroy(struct mpgl_osd *ctx);

void mpgl_osd_set_gl_state(struct mpgl_osd *ctx, struct mpgl_osd_part *p);
void mpgl_osd_unset_gl_state(struct mpgl_osd *ctx, struct mpgl_osd_part *p);

void mpgl_osd_draw_legacy(struct mpgl_osd *ctx, struct osd_state *osd,
                          struct mp_osd_res res);
void mpgl_osd_draw_cb(struct mpgl_osd *ctx,
                      struct osd_state *osd,
                      struct mp_osd_res res,
                      void (*cb)(void *ctx, struct mpgl_osd_part *part,
                                 struct sub_bitmaps *imgs),
                      void *cb_ctx);
void mpgl_osd_redraw_cb(struct mpgl_osd *ctx,
                        void (*cb)(void *ctx, struct mpgl_osd_part *part,
                                   struct sub_bitmaps *imgs),
                        void *cb_ctx);

#endif
