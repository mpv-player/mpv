#ifndef MPLAYER_SUB_IMG_CONVERT_H
#define MPLAYER_SUB_IMG_CONVERT_H

#include <stdbool.h>

struct sub_bitmaps;
struct sub_bitmap;
struct mp_rect;

// Sub postprocessing
void mp_blur_scale_rgba_sub_bitmap(struct sub_bitmap *d, double gblur,
                                   double scale);

bool mp_sub_bitmaps_bb(struct sub_bitmaps *imgs, struct mp_rect *out_bb);

// Intentionally limit the maximum number of bounding rects to something low.
// This prevents the algorithm from degrading to O(N^2).
// Most subtitles yield a very low number of bounding rects (<5).
#define MP_SUB_BB_LIST_MAX 15

int mp_get_sub_bb_list(struct sub_bitmaps *sbs, struct mp_rect *out_rc_list,
                       int rc_list_count);

#endif
