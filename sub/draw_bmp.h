#ifndef MPLAYER_DRAW_BMP_H
#define MPLAYER_DRAW_BMP_H

#include "osd.h"

struct mp_image;
struct mp_draw_sub_cache;
void mp_draw_sub_bitmaps(struct mp_draw_sub_cache **cache, struct mp_image *dst,
                         struct sub_bitmap_list *sbs_list);

extern const bool mp_draw_sub_formats[SUBBITMAP_COUNT];

#endif /* MPLAYER_DRAW_BMP_H */

// vim: ts=4 sw=4 et tw=80
