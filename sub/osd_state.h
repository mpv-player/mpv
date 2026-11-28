#ifndef MP_OSD_STATE_H_
#define MP_OSD_STATE_H_

#include <stdatomic.h>

#include "osd.h"
#include "osdep/threads.h"

enum mp_osdtype {
    OSDTYPE_SUB,
    OSDTYPE_SUB2, // IDs must be numerically successive

    OSDTYPE_DISC_MENU,

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
    // The rendered track, until its events change
    struct ass_image *imgs;
    struct mp_osd_res imgs_res;
    bool imgs_valid;
};

struct osd_object {
    int type; // OSDTYPE_*
    bool is_sub;

    // OSDTYPE_OSD
    char *text;
    struct osd_progbar_state progbar_state;

    // OSDTYPE_SUB/OSDTYPE_SUB2
    struct dec_sub *sub;

    // OSDTYPE_EXTERNAL
    struct osd_external **externals;
    int num_externals;

    // OSDTYPE_EXTERNAL2 / OSDTYPE_DISC_MENU
    struct sub_bitmaps *image_overlay;

    // OSDTYPE_OSD / OSDTYPE_EXTERNAL: every change of what they show gets a
    // new state_id, result is the render of the state result_id
    int state_id;
    struct sub_bitmaps *result;
    struct mp_osd_res result_res;
    int result_format;
    int result_id;
    int result_change_id;

    int rendering;  // Number of threads that render the object.
    int waiting;    // Number of threads that wait for thread to render it
    bool job;       // A job that renders the object ahead is queued or running

    // VO cache state
    int vo_change_id;
    int last_change_id;
    struct mp_osd_res vo_res;
    bool vo_had_output;

    // Internally used by osd_libass.c
    char *render_text;
    struct osd_progbar_state render_progbar;
    bool changed;
    struct ass_state ass;
    struct mp_sub_packer *sub_packer;
    struct sub_bitmap_copy_cache *copy_cache;
    struct ass_image **ass_imgs;
};

struct osd_external {
    struct osd_external_ass ov;
    bool ov_changed;
    // The copy of ov the track is built from
    char *data;
    int res_x, res_y;
    bool hidden;
    bool update;
    struct ass_state ass;
};

struct osd_state {
    mp_mutex lock;
    mp_cond wakeup;
    struct mp_thread_pool *pool;

    struct osd_object *objs[MAX_OSD_PARTS];

    bool render_subs_in_filter;
    _Atomic double force_video_pts;

    bool want_redraw;
    bool want_redraw_notification;
    void (*wakeup_cb)(void *ctx);
    void *wakeup_ctx;

    struct m_config_cache *opts_cache;
    struct mp_osd_render_opts *opts;
    struct mpv_global *global;
    struct mp_log *log;
    struct stats_ctx *stats;

    struct mp_draw_sub_cache *draw_cache;
};

// defined in osd_libass.c
void osd_object_prepare(struct osd_state *osd, struct osd_object *obj);
struct sub_bitmaps *osd_object_render(struct osd_state *osd,
                                      struct osd_object *obj,
                                      struct mp_osd_res *res, int format);
void osd_destroy_backend(struct osd_state *osd);

void osd_wait_object_idle(struct osd_state *osd, struct osd_object *obj);
void osd_object_changed(struct osd_state *osd, struct osd_object *obj);

#endif
