#ifndef MPLAYER_GL_OSD_H
#define MPLAYER_GL_OSD_H

#include <stdbool.h>
#include <inttypes.h>

#include "gl_common.h"
#include "sub/sub.h"

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
    GL *gl;
    bool use_pbo;
    struct mpgl_osd_part *parts[MAX_OSD_PARTS];
    const struct osd_fmt_entry *fmt_table;
    void *scratch;
};

struct mpgl_osd *mpgl_osd_init(GL *gl, bool legacy);
void mpgl_osd_destroy(struct mpgl_osd *ctx);

bool mpgl_osd_query_format(struct mpgl_osd *ctx, int osd_format);

void mpgl_osd_draw_legacy(struct mpgl_osd *ctx, struct sub_bitmaps *b);
struct mpgl_osd_part *mpgl_osd_generate(struct mpgl_osd *ctx,
                                        struct sub_bitmaps *b);

void mpgl_osd_gl_set_state(struct mpgl_osd *ctx, struct mpgl_osd_part *p);
void mpgl_osd_gl_unset_state(struct mpgl_osd *ctx, struct mpgl_osd_part *p);

#endif
