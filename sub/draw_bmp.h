#ifndef MPLAYER_DRAW_BMP_H
#define MPLAYER_DRAW_BMP_H

#include "sub/sub.h"

struct mp_image;
struct sub_bitmaps;
struct mp_csp_details;
struct mp_draw_sub_cache;
void mp_draw_sub_bitmaps(struct mp_draw_sub_cache **cache, struct mp_image *dst,
                         struct sub_bitmaps *sbs);

extern const bool mp_draw_sub_formats[SUBBITMAP_COUNT];

struct mp_draw_sub_backup;
struct mp_draw_sub_backup *mp_draw_sub_backup_new(void);
void mp_draw_sub_backup_add(struct mp_draw_sub_backup *backup,
                            struct mp_image *img, struct sub_bitmaps *sbs);
void mp_draw_sub_backup_reset(struct mp_draw_sub_backup *backup);
bool mp_draw_sub_backup_restore(struct mp_draw_sub_backup *backup,
                                struct mp_image *buffer);

#endif /* MPLAYER_DRAW_BMP_H */

// vim: ts=4 sw=4 et tw=80
