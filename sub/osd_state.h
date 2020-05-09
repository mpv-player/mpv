#ifndef MP_OSD_STATE_H_
#define MP_OSD_STATE_H_

#include <pthread.h>

#include "osd.h"

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
    int res_x, res_y;
    bool changed;
    struct mp_osd_res vo_res; // last known value
};

struct osd_object {
    int type; // OSDTYPE_*
    bool is_sub;

    // OSDTYPE_OSD
    bool osd_changed;
    char *text;
    struct osd_progbar_state progbar_state;

    // OSDTYPE_SUB/OSDTYPE_SUB2
    struct dec_sub *sub;

    // OSDTYPE_EXTERNAL
    struct osd_external **externals;
    int num_externals;

    // OSDTYPE_EXTERNAL2
    struct sub_bitmaps *external2;

    // VO cache state
    int vo_change_id;
    struct mp_osd_res vo_res;
    bool vo_had_output;

    // Internally used by osd_libass.c
    bool changed;
    struct ass_state ass;
    struct mp_ass_packer *ass_packer;
    struct sub_bitmap_copy_cache *copy_cache;
    struct ass_image **ass_imgs;
};

struct osd_external {
    struct osd_external_ass ov;
    struct ass_state ass;
};

struct osd_state {
    pthread_mutex_t lock;

    struct osd_object *objs[MAX_OSD_PARTS];

    bool render_subs_in_filter;
    double force_video_pts;

    bool want_redraw;
    bool want_redraw_notification;

    struct m_config_cache *opts_cache;
    struct mp_osd_render_opts *opts;
    struct mpv_global *global;
    struct mp_log *log;
    struct stats_ctx *stats;

    struct mp_draw_sub_cache *draw_cache;
};

// defined in osd_libass.c
struct sub_bitmaps *osd_object_get_bitmaps(struct osd_state *osd,
                                           struct osd_object *obj, int format);
void osd_init_backend(struct osd_state *osd);
void osd_destroy_backend(struct osd_state *osd);

#endif
