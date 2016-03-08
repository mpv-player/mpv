#ifndef MP_OSD_STATE_H_
#define MP_OSD_STATE_H_

#include <pthread.h>

#include "osd.h"

#define OSD_CONV_CACHE_MAX 4

enum mp_osdtype {
    OSDTYPE_SUB,
    OSDTYPE_SUB2, // IDs must be numerically successive

    OSDTYPE_OSD,

    OSDTYPE_EXTERNAL,
    OSDTYPE_EXTERNAL2,

    OSDTYPE_COUNT
};

struct ass_state {
    struct mp_log *log;
    struct ass_track *track;
    struct ass_renderer *render;
    struct ass_library *library;
};

struct osd_object {
    int type; // OSDTYPE_*
    bool is_sub;

    bool force_redraw;

    // OSDTYPE_OSD
    char *text;

    // OSDTYPE_OSD
    struct osd_progbar_state progbar_state;

    // OSDTYPE_SUB/OSDTYPE_SUB2
    struct dec_sub *sub;

    // OSDTYPE_EXTERNAL
    struct osd_external *externals;
    int num_externals;

    // OSDTYPE_EXTERNAL2
    struct sub_bitmaps *external2;

    // caches for OSD conversion (internal to render_object())
    struct osd_conv_cache *cache[OSD_CONV_CACHE_MAX];
    struct sub_bitmaps cached;

    // VO cache state
    int vo_change_id;
    struct mp_osd_res vo_res;

    // Internally used by osd_libass.c
    struct sub_bitmaps parts_cache;
    struct ass_state ass;
};

struct osd_external {
    void *id;
    char *text;
    int res_x, res_y;
    struct ass_state ass;
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

void osd_changed_unlocked(struct osd_state *osd, int obj);

#endif
