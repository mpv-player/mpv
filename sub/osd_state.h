#ifndef MP_OSD_STATE_H_
#define MP_OSD_STATE_H_

#include <pthread.h>

#include "osd.h"

#define OSD_CONV_CACHE_MAX 4

struct osd_object {
    int type; // OSDTYPE_*
    bool is_sub;

    bool force_redraw;

    // OSDTYPE_SUB/OSDTYPE_SUB2/OSDTYPE_OSD/OSDTYPE_EXTERNAL
    char *text;

    // OSDTYPE_PROGBAR
    struct osd_progbar_state progbar_state;

    // OSDTYPE_SUB/OSDTYPE_SUB2
    struct osd_sub_state sub_state;

    // OSDTYPE_EXTERNAL
    int external_res_x, external_res_y;

    // OSDTYPE_EXTERNAL2
    struct sub_bitmaps *external2;

    // OSDTYPE_NAV_HIGHLIGHT
    void *highlight_priv;

    // caches for OSD conversion (internal to render_object())
    struct osd_conv_cache *cache[OSD_CONV_CACHE_MAX];
    struct sub_bitmaps cached;

    // VO cache state
    int vo_change_id;
    struct mp_osd_res vo_res;

    // Internally used by osd_libass.c
    struct sub_bitmap *parts_cache;
    struct ass_track *osd_track;
    struct ass_renderer *osd_render;
    struct ass_library *osd_ass_library;
};

struct osd_state {
    pthread_mutex_t lock;

    struct osd_object *objs[MAX_OSD_PARTS];

    bool render_subs_in_filter;

    bool want_redraw;

    struct MPOpts *opts;
    struct mpv_global *global;
    struct mp_log *log;

    struct mp_draw_sub_cache *draw_cache;
};

#endif
