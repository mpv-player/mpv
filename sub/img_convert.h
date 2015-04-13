#ifndef MPLAYER_SUB_IMG_CONVERT_H
#define MPLAYER_SUB_IMG_CONVERT_H

#include <stdbool.h>

struct osd_conv_cache;
struct sub_bitmaps;
struct mp_rect;

struct osd_conv_cache *osd_conv_cache_new(void);

// These functions convert from one OSD format to another. On success, they copy
// the converted image data into c, and change imgs to point to the data.
bool osd_conv_idx_to_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs);
bool osd_conv_ass_to_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs);
// Sub postprocessing
bool osd_conv_blur_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs,
                        double gblur);
bool osd_scale_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs);
bool osd_conv_idx_to_gray(struct osd_conv_cache *c, struct sub_bitmaps *imgs);

bool mp_sub_bitmaps_bb(struct sub_bitmaps *imgs, struct mp_rect *out_bb);

// Intentionally limit the maximum number of bounding rects to something low.
// This prevents the algorithm from degrading to O(N^2).
// Most subtitles yield a very low number of bounding rects (<5).
#define MP_SUB_BB_LIST_MAX 15

int mp_get_sub_bb_list(struct sub_bitmaps *sbs, struct mp_rect *out_rc_list,
                       int rc_list_count);

#endif
