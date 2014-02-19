/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <limits.h>
#include <assert.h>

#include "core.h"

#include "common/msg.h"
#include "common/common.h"
#include "input/input.h"

#include "stream/stream_dvdnav.h"

#include "sub/dec_sub.h"
#include "sub/osd.h"

#include "video/mp_image.h"
#include "video/decode/dec_video.h"

struct mp_nav_state {
    struct mp_log *log;

    int nav_still_frame;
    bool nav_eof;
    bool nav_menu;
    bool nav_draining;

    // Accessed by OSD (possibly separate thread)
    int hi_visible;
    int highlight[4]; // x0 y0 x1 y1
    int vidsize[2];
    int subsize[2];
    struct sub_bitmap *hi_elem;
};

// Allocate state and enable navigation features. Must happen before
// initializing cache, because the cache would read data. Since stream_dvdnav is
// in a mode which skips all transitions on reading data (before enabling
// navigation), this would skip some menu screens.
void mp_nav_init(struct MPContext *mpctx)
{
    assert(!mpctx->nav_state);

    // dvdnav is interactive
    if (mpctx->encode_lavc_ctx)
        return;

    struct mp_nav_cmd inp = {MP_NAV_CMD_ENABLE};
    if (stream_control(mpctx->stream, STREAM_CTRL_NAV_CMD, &inp) < 1)
        return;

    mpctx->nav_state = talloc_zero(NULL, struct mp_nav_state);
    mpctx->nav_state->log = mp_log_new(mpctx->nav_state, mpctx->log, "dvdnav");

    MP_VERBOSE(mpctx->nav_state, "enabling\n");

    mp_input_enable_section(mpctx->input, "dvdnav", 0);
    mp_input_set_section_mouse_area(mpctx->input, "dvdnav-menu",
                                    INT_MIN, INT_MIN, INT_MAX, INT_MAX);
}

void mp_nav_reset(struct MPContext *mpctx)
{
    struct mp_nav_state *nav = mpctx->nav_state;
    if (!nav)
        return;
    struct mp_nav_cmd inp = {MP_NAV_CMD_RESUME};
    stream_control(mpctx->stream, STREAM_CTRL_NAV_CMD, &inp);
    osd_set_nav_highlight(mpctx->osd, NULL);
    nav->hi_visible = 0;
    nav->nav_menu = false;
    nav->nav_draining = false;
    nav->nav_still_frame = 0;
    mp_input_disable_section(mpctx->input, "dvdnav-menu");
    // Prevent demuxer init code to seek to the "start"
    mpctx->stream->start_pos = stream_tell(mpctx->stream);
    stream_control(mpctx->stream, STREAM_CTRL_RESUME_CACHE, NULL);
}

void mp_nav_destroy(struct MPContext *mpctx)
{
    osd_set_nav_highlight(mpctx->osd, NULL);
    if (!mpctx->nav_state)
        return;
    mp_input_disable_section(mpctx->input, "dvdnav");
    mp_input_disable_section(mpctx->input, "dvdnav-menu");
    talloc_free(mpctx->nav_state);
    mpctx->nav_state = NULL;
}

void mp_nav_user_input(struct MPContext *mpctx, char *command)
{
    struct mp_nav_state *nav = mpctx->nav_state;
    if (!nav)
        return;
    if (strcmp(command, "mouse_move") == 0) {
        struct mp_image_params vid = {0};
        if (mpctx->d_video)
            vid = mpctx->d_video->decoder_output;
        struct mp_nav_cmd inp = {MP_NAV_CMD_MOUSE_POS};
        int x, y;
        mp_input_get_mouse_pos(mpctx->input, &x, &y);
        osd_coords_to_video(mpctx->osd, vid.w, vid.h, &x, &y);
        inp.u.mouse_pos.x = x;
        inp.u.mouse_pos.y = y;
        stream_control(mpctx->stream, STREAM_CTRL_NAV_CMD, &inp);
    } else {
        struct mp_nav_cmd inp = {MP_NAV_CMD_MENU};
        inp.u.menu.action = command;
        stream_control(mpctx->stream, STREAM_CTRL_NAV_CMD, &inp);
    }
}

void mp_handle_nav(struct MPContext *mpctx)
{
    struct mp_nav_state *nav = mpctx->nav_state;
    if (!nav)
        return;
    while (1) {
        struct mp_nav_event *ev = NULL;
        stream_control(mpctx->stream, STREAM_CTRL_GET_NAV_EVENT, &ev);
        if (!ev)
            break;
        switch (ev->event) {
        case MP_NAV_EVENT_DRAIN: {
            nav->nav_draining = true;
            MP_VERBOSE(nav, "drain requested\n");
            break;
        }
        case MP_NAV_EVENT_RESET_ALL: {
            mpctx->stop_play = PT_RELOAD_DEMUXER;
            MP_VERBOSE(nav, "reload\n");
            break;
        }
        case MP_NAV_EVENT_RESET: {
            nav->nav_still_frame = 0;
            break;
        }
        case MP_NAV_EVENT_EOF:
            nav->nav_eof = true;
            break;
        case MP_NAV_EVENT_STILL_FRAME: {
            int len = ev->u.still_frame.seconds;
            MP_VERBOSE(nav, "wait for %d seconds\n", len);
            if (len > 0 && nav->nav_still_frame == 0)
                nav->nav_still_frame = len;
            break;
        }
        case MP_NAV_EVENT_MENU_MODE:
            nav->nav_menu = ev->u.menu_mode.enable;
            if (nav->nav_menu) {
                mp_input_enable_section(mpctx->input, "dvdnav-menu",
                                        MP_INPUT_ON_TOP);
            } else {
                mp_input_disable_section(mpctx->input, "dvdnav-menu");
            }
            break;
        case MP_NAV_EVENT_HIGHLIGHT: {
            MP_VERBOSE(nav, "highlight: %d %d %d - %d %d\n",
                       ev->u.highlight.display,
                       ev->u.highlight.sx, ev->u.highlight.sy,
                       ev->u.highlight.ex, ev->u.highlight.ey);
            osd_set_nav_highlight(mpctx->osd, NULL);
            nav->highlight[0] = ev->u.highlight.sx;
            nav->highlight[1] = ev->u.highlight.sy;
            nav->highlight[2] = ev->u.highlight.ex;
            nav->highlight[3] = ev->u.highlight.ey;
            nav->hi_visible = ev->u.highlight.display;
            int sizes[2] = {0};
            if (mpctx->d_sub[0])
                sub_control(mpctx->d_sub[0], SD_CTRL_GET_RESOLUTION, sizes);
            if (sizes[0] < 1 || sizes[1] < 1) {
                struct mp_image_params vid = {0};
                if (mpctx->d_video)
                    vid = mpctx->d_video->decoder_output;
                sizes[0] = vid.w;
                sizes[1] = vid.h;
            }
            for (int n = 0; n < 2; n++)
                nav->vidsize[n] = sizes[n];
            osd_set_nav_highlight(mpctx->osd, mpctx);
            break;
        }
        default: ; // ignore
        }
        talloc_free(ev);
    }
    if (mpctx->stop_play == AT_END_OF_FILE) {
        if (nav->nav_still_frame > 0) {
            // gross hack
            mpctx->time_frame += nav->nav_still_frame;
            mpctx->playing_last_frame = true;
            nav->nav_still_frame = -2;
        } else if (nav->nav_still_frame == -2) {
            struct mp_nav_cmd inp = {MP_NAV_CMD_SKIP_STILL};
            stream_control(mpctx->stream, STREAM_CTRL_NAV_CMD, &inp);
        }
    }
    if (nav->nav_draining && mpctx->stop_play == AT_END_OF_FILE) {
        MP_VERBOSE(nav, "execute drain\n");
        struct mp_nav_cmd inp = {MP_NAV_CMD_DRAIN_OK};
        stream_control(mpctx->stream, STREAM_CTRL_NAV_CMD, &inp);
        nav->nav_draining = false;
        stream_control(mpctx->stream, STREAM_CTRL_RESUME_CACHE, NULL);
    }
    // E.g. keep displaying still frames
    if (mpctx->stop_play == AT_END_OF_FILE && !nav->nav_eof)
        mpctx->stop_play = KEEP_PLAYING;
}

// Render "fake" highlights, because using actual dvd sub highlight elements
// is too hard, and would require extra libavcodec to begin with.
// Note: a proper solution would introduce something like
//       SD_CTRL_APPLY_DVDNAV, which would crop the vobsub frame,
//       and apply the current CLUT.
void mp_nav_get_highlight(void *priv, struct mp_osd_res res,
                          struct sub_bitmaps *out_imgs)
{
    struct MPContext *mpctx = priv;
    struct mp_nav_state *nav = mpctx ? mpctx->nav_state : NULL;
    if (!nav)
        return;
    struct sub_bitmap *sub = nav->hi_elem;
    if (!sub)
        sub = talloc_zero(nav, struct sub_bitmap);

    nav->hi_elem = sub;
    int sizes[2] = {nav->vidsize[0], nav->vidsize[1]};
    if (sizes[0] < 1 || sizes[1] < 1)
        return;
    if (sizes[0] != nav->subsize[0] || sizes[1] != nav->subsize[1]) {
        talloc_free(sub->bitmap);
        sub->bitmap = talloc_array(sub, uint32_t, sizes[0] * sizes[1]);
        memset(sub->bitmap, 0x80, talloc_get_size(sub->bitmap));
    }

    sub->x = nav->highlight[0];
    sub->y = nav->highlight[1];
    sub->w = MPCLAMP(nav->highlight[2] - sub->x, 0, sizes[0]);
    sub->h = MPCLAMP(nav->highlight[3] - sub->y, 0, sizes[1]);
    sub->stride = sub->w * 4;
    out_imgs->format = SUBBITMAP_RGBA;
    out_imgs->parts = sub;
    out_imgs->num_parts = sub->w > 0 && sub->h > 0 && nav->hi_visible;
    osd_rescale_bitmaps(out_imgs, sizes[0], sizes[1], res, -1);
}
