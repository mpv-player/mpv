#ifndef MPLAYER_SUB_IMG_CONVERT_H
#define MPLAYER_SUB_IMG_CONVERT_H

#include <stdbool.h>

struct osd_conv_cache;
struct sub_bitmaps;

struct osd_conv_cache *osd_conv_cache_new(void);

// These functions convert from one OSD format to another. On success, they copy
// the converted image data into c, and change imgs to point to the data.
bool osd_conv_idx_to_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs);
bool osd_conv_ass_to_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs);
// Sub postprocessing
bool osd_conv_blur_rgba(struct osd_conv_cache *c, struct sub_bitmaps *imgs,
                        double gblur);
bool osd_conv_idx_to_gray(struct osd_conv_cache *c, struct sub_bitmaps *imgs);

#endif
