#ifndef MPLAYER_DRAW_BMP_H
#define MPLAYER_DRAW_BMP_H

#include "osd.h"

struct mp_rect;
struct mp_image;
struct mpv_global;
struct mp_draw_sub_cache;

struct mp_draw_sub_cache *mp_draw_sub_alloc(void *ta_parent, struct mpv_global *g);

// Render the sub-bitmaps in sbs_list to dst. sbs_list must have been rendered
// for an OSD resolution equivalent to dst's size (UB if not).
// Warning: if dst is a format with alpha, and dst is not set to MP_ALPHA_PREMUL
//          (not done by default), this will be extremely slow.
// Warning: the caller is responsible for ensuring that dst is writable.
//  cache: allocated instance; caches non-changing OSD parts etc.
//  dst: image to draw to
//  sbs_list: source sub-bitmaps
//  returns: success
bool mp_draw_sub_bitmaps(struct mp_draw_sub_cache *cache, struct mp_image *dst,
                         struct sub_bitmap_list *sbs_list);

char *mp_draw_sub_get_dbg_info(struct mp_draw_sub_cache *c);

// Return a RGBA overlay with subtitles. The returned image uses IMGFMT_BGRA and
// premultiplied alpha, and the size specified by sbs_list.w/h.
// This can return a list of active (act_) and modified (mod_) rectangles.
// Active rectangles are regions that contain visible OSD pixels. Modified
// rectangles are regions that were changed since the last call. This function
// always makes the act region a subset of the mod region. Rectangles within a
// list never overlap with rectangles within the same list.
// If num_mod_rcs==0 is returned, this function guarantees that the act region
// did not change since the last call.
// If the user-provided lists are too small (max_*_rcs too small), multiple
// rectangles are merged until they fit in the list.
// You can pass max_act_rcs=0, which implies you render the whole overlay.
//  cache: allocated instance; keeps track of changed regions
//  sbs_list: source sub-bitmaps
//  act_rcs: caller allocated list of non-transparent rectangles
//  max_act_rcs: number of allocated items in act_rcs
//  num_act_rcs: set to the number of valid items in act_rcs
//  mod_rcs, max_mod_rcs, num_mod_rcs: modified rectangles
//  returns: internal OSD overlay owned by cache, NULL on error
//           read only, valid until the next call on cache
struct mp_image *mp_draw_sub_overlay(struct mp_draw_sub_cache *cache,
                                     struct sub_bitmap_list *sbs_list,
                                     struct mp_rect *act_rcs,
                                     int max_act_rcs,
                                     int *num_act_rcs,
                                     struct mp_rect *mod_rcs,
                                     int max_mod_rcs,
                                     int *num_mod_rcs);

extern const bool mp_draw_sub_formats[SUBBITMAP_COUNT];

#endif /* MPLAYER_DRAW_BMP_H */

// vim: ts=4 sw=4 et tw=80
