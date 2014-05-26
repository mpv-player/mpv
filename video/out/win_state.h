#ifndef MP_WIN_STATE_H_
#define MP_WIN_STATE_H_

#include "common/common.h"

struct vo;

enum {
    // By user settings, the window manager's chosen window position should
    // be overridden.
    VO_WIN_FORCE_POS = (1 << 0),
};

struct vo_win_geometry {
    // Bitfield of VO_WIN_* flags
    int flags;
    // Position & size of the window. In xinerama coordinates, i.e. they're
    // relative to the virtual desktop encompassing all screens, not the
    // current screen.
    struct mp_rect win;
    // Aspect ratio of the current monitor.
    // (calculated from screen size and options.)
    double monitor_par;
};

void vo_calc_window_geometry(struct vo *vo, const struct mp_rect *screen,
                             struct vo_win_geometry *out_geo);
void vo_apply_window_geometry(struct vo *vo, const struct vo_win_geometry *geo);

#endif
