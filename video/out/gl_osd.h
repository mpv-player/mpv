#ifndef MPLAYER_GL_OSD_H
#define MPLAYER_GL_OSD_H

#include <stdbool.h>
#include <inttypes.h>

#include "gl_common.h"
#include "sub/osd.h"

struct mpgl_osd_part {
    enum sub_bitmap_format format;
    int bitmap_id, bitmap_pos_id;
    GLuint texture;
    int w, h;
    GLuint buffer;
    int num_vertices;
    void *vertices;
    struct bitmap_packer *packer;
};

struct mpgl_osd {
    struct mp_log *log;
    struct osd_state *osd;
    GL *gl;
    bool use_pbo;
    bool scaled;
    struct mpgl_osd_part *parts[MAX_OSD_PARTS];
    const struct osd_fmt_entry *fmt_table;
    bool formats[SUBBITMAP_COUNT];
    void *scratch;
};

struct mpgl_osd *mpgl_osd_init(GL *gl, struct mp_log *log, struct osd_state *osd);
void mpgl_osd_destroy(struct mpgl_osd *ctx);

struct mpgl_osd_part *mpgl_osd_generate(struct mpgl_osd *ctx,
                                        struct sub_bitmaps *b);

void mpgl_osd_set_gl_state(struct mpgl_osd *ctx, struct mpgl_osd_part *p);
void mpgl_osd_unset_gl_state(struct mpgl_osd *ctx, struct mpgl_osd_part *p);

void mpgl_osd_draw_legacy(struct mpgl_osd *ctx, double pts,
                          struct mp_osd_res res);

#endif
