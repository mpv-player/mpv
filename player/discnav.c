/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdbool.h>

#include "mpv_talloc.h"

#include "common/common.h"
#include "common/msg.h"
#include "input/input.h"
#include "player/command.h"

#include "stream/stream.h"
#include "demux/demux.h"
#include "sub/dec_sub.h"
#include "sub/osd.h"
#include "sub/osd_state.h"
#include "video/mp_image.h"
#include "video/out/vo.h"

#include "core.h"

struct disc_nav_state {
    // True while the disc-menu input section is enabled (a menu is on screen).
    bool overlay_visible;

    // Blu-ray HDMV menu staging. libbluray hands us a pre-composited BGRA IG
    // plane via STREAM_CTRL_GET_NAV_OVERLAY; we copy it into bd_image and
    // forward through osd_set_bitmaps (OSDTYPE_DISC_MENU).
    struct mp_image *bd_image;
    uint32_t bd_last_change_id;
    struct mp_osd_res bd_last_vo_res;

    // Last menu-overlay change_id we forced an OSD redraw for.
    uint32_t last_overlay_change_id;
    bool overlay_change_seen;

    // Last observed disc-nav discontinuity counter (bumped by the stream
    // backend on user nav actions and on libbluray/libdvdnav-internal
    // playlist/title/cell transitions).
    uint32_t last_discontinuity_id;
    bool discontinuity_seen;

    // DVD-only: dvd_subtitle track we force-selected for the menu, and the
    // selection it displaced (restored on menu close).
    struct track *menu_selected_track;
    struct track *menu_saved_track;

    // Last disc-driven audio/sub/angle we acted on.
    int last_audio_id;
    int last_sub_id;
    bool last_sub_visible;
    int last_angle;
    bool track_sync_seen;
    // Last disc-discontinuity id we acted on for track sync.
    uint32_t last_track_disc_id;
};

static bool is_dvd_sub_track(struct track *t)
{
    return t && t->type == STREAM_SUB && t->stream && t->stream->codec &&
           t->stream->codec->codec &&
           strcmp(t->stream->codec->codec, "dvd_subtitle") == 0;
}

static struct disc_nav_state *get_state(struct MPContext *mpctx)
{
    if (!mpctx->disc_nav)
        mpctx->disc_nav = talloc_zero(mpctx, struct disc_nav_state);
    return mpctx->disc_nav;
}

void disc_nav_destroy(struct MPContext *mpctx)
{
    if (!mpctx->disc_nav)
        return;
    mp_image_unrefp(&mpctx->disc_nav->bd_image);
    TA_FREEP(&mpctx->disc_nav);
}

struct stream *disc_nav_get_stream(struct MPContext *mpctx)
{
    if (!mpctx->demuxer || !mpctx->demuxer->stream)
        return NULL;
    struct stream *s = mpctx->demuxer->stream;
    if (!s->info || !s->info->name)
        return NULL;
    const char *n = s->info->name;
    if (strcmp(n, "dvdnav") == 0 || strcmp(n, "ifo_dvdnav") == 0 ||
        strcmp(n, "bd") == 0 || strcmp(n, "bdmv/bluray") == 0)
    {
        return s;
    }
    return NULL;
}

bool disc_nav_mouse_pos_to_src(struct MPContext *mpctx, int src_w, int src_h,
                               int *out_x, int *out_y)
{
    struct vo *vo = mpctx->video_out;
    if (!vo || !vo->config_ok || src_w <= 0 || src_h <= 0)
        return false;
    int wx, wy, hover;
    mp_input_get_mouse_pos(mpctx->input, &wx, &wy, &hover);
    struct mp_rect src, dst;
    struct mp_osd_res osd; // mandatory out param; ignored
    vo_get_src_dst_rects(vo, &src, &dst, &osd);
    int dw = dst.x1 - dst.x0;
    int dh = dst.y1 - dst.y0;
    if (dw <= 0 || dh <= 0)
        return false;
    double fx = (wx - dst.x0) / (double)dw;
    double fy = (wy - dst.y0) / (double)dh;
    if (fx < 0 || fx > 1 || fy < 0 || fy > 1)
        return false;
    *out_x = (int)(fx * src_w);
    *out_y = (int)(fy * src_h);
    return true;
}

// Push the current menu/highlight state to the dvd_subtitle decoders.
static void push_dvd_overlay(struct MPContext *mpctx,
                             struct stream_nav_state *nav, bool visible)
{
    struct mp_dvdnav_hli hli = {
        .show = visible,
        .menu_active = nav->menu_active,
        .change_id = nav->change_id,
    };
    if (visible) {
        hli.x = nav->hl_x;
        hli.y = nav->hl_y;
        hli.w = nav->hl_w;
        hli.h = nav->hl_h;
        memcpy(hli.palette, nav->hl_palette, sizeof(hli.palette));
    }
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *t = mpctx->tracks[n];
        if (!is_dvd_sub_track(t) || !t->d_sub)
            continue;
        sub_control(t->d_sub, SD_CTRL_APPLY_DVDNAV, &hli);
    }
}

static void push_bd_overlay(struct MPContext *mpctx, struct stream *s,
                            struct stream_nav_state *nav, bool visible)
{
    struct disc_nav_state *st = get_state(mpctx);

    if (!visible) {
        if (st->overlay_visible)
            osd_set_bitmaps(mpctx->osd, OSDTYPE_DISC_MENU, NULL);
        st->bd_last_change_id = 0;
        st->bd_last_vo_res = (struct mp_osd_res){0};
        return;
    }

    if (nav->src_w <= 0 || nav->src_h <= 0)
        return;

    // Skip the work when nothing the renderer cares about changed.
    struct mp_osd_res vo_res = osd_get_vo_res(mpctx->osd);
    if (st->overlay_visible &&
        st->bd_last_change_id == nav->change_id &&
        osd_res_equals(vo_res, st->bd_last_vo_res))
        return;

    if (!st->bd_image ||
        st->bd_image->w != nav->src_w || st->bd_image->h != nav->src_h)
    {
        mp_image_unrefp(&st->bd_image);
        st->bd_image = mp_image_alloc(IMGFMT_BGRA, nav->src_w, nav->src_h);
        if (!st->bd_image)
            return;
        talloc_steal(st, st->bd_image);
    }

    struct mp_image *img = st->bd_image;
    struct stream_nav_overlay_req req = {
        .w      = img->w,
        .h      = img->h,
        .stride = img->stride[0],
        .dst    = img->planes[0],
    };
    if (stream_control(s, STREAM_CTRL_GET_NAV_OVERLAY, &req) < 1)
        return;

    struct sub_bitmap part = {
        .bitmap = img->planes[0],
        .stride = img->stride[0],
        .w  = req.w,
        .h  = req.h,
        .dw = req.w,
        .dh = req.h,
    };
    struct sub_bitmaps imgs = {
        .format    = SUBBITMAP_BGRA,
        .parts     = &part,
        .num_parts = 1,
        .packed    = img,
        .packed_w  = img->w,
        .packed_h  = img->h,
        .change_id = nav->change_id ? (int)nav->change_id : 1,
    };
    osd_rescale_bitmaps(&imgs, nav->src_w, nav->src_h, vo_res, 0);
    osd_set_bitmaps(mpctx->osd, OSDTYPE_DISC_MENU, &imgs);
    st->bd_last_change_id = nav->change_id;
    st->bd_last_vo_res = vo_res;
}

// Sync demuxer->edition with what the disc is actually playing.
static void sync_current_edition(struct MPContext *mpctx, struct stream *s,
                                 struct stream_nav_state *nav)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer || demuxer->num_editions <= 0 || !demuxer->desc ||
        strcmp(demuxer->desc->name, "disc") != 0)
        return;

    int desired = demuxer->edition;
    if (nav->menu_active) {
        desired = demuxer->num_editions - 1;
    } else {
        unsigned title;
        if (stream_control(s, STREAM_CTRL_GET_CURRENT_TITLE, &title) >= 1 &&
            (int)title < demuxer->num_editions - 1)
        {
            desired = (int)title;
        }
    }
    if (desired != demuxer->edition) {
        MP_VERBOSE(mpctx, "discnav: current-edition %d->%d "
                   "(menu_active=%d)\n",
                   demuxer->edition, desired, nav->menu_active);
        demuxer->edition = desired;
        mp_notify_property(mpctx, "current-edition");
    }
}

// Catch async playlist/title hops driven by the disc itself (HDMV bytecode,
// dvdnav HOP_CHANNEL, etc.).
static void check_async_discontinuity(struct MPContext *mpctx,
                                      struct stream_nav_state *nav)
{
    struct disc_nav_state *st = get_state(mpctx);
    if (!mpctx->demuxer)
        return;
    if (!st->discontinuity_seen) {
        st->last_discontinuity_id = nav->discontinuity_id;
        st->discontinuity_seen = true;
        return;
    }
    if (nav->discontinuity_id == st->last_discontinuity_id)
        return;
    MP_VERBOSE(mpctx, "discnav: async discontinuity %u->%u, flushing\n",
               st->last_discontinuity_id, nav->discontinuity_id);
    st->last_discontinuity_id = nav->discontinuity_id;
    reset_playback_state(mpctx);
    demux_flush(mpctx->demuxer);
    // Re-queue the retained menu subpicture the flush may have destroyed.
    demux_nav_refresh(mpctx->demuxer);
}

static struct track *find_track_by_demuxer_id(struct MPContext *mpctx,
                                              enum stream_type type, int id)
{
    if (id < 0)
        return NULL;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *t = mpctx->tracks[n];
        if (t->type == type && t->stream && t->stream->demuxer_id == id)
            return t;
    }
    return NULL;
}

static void sync_disc_track_selection(struct MPContext *mpctx, struct stream *s,
                                      struct stream_nav_state *nav)
{
    struct disc_nav_state *st = get_state(mpctx);
    bool first = !st->track_sync_seen;
    bool hopped = nav->discontinuity_id != st->last_track_disc_id;
    st->last_track_disc_id = nav->discontinuity_id;
    st->track_sync_seen = true;

    if (!first && (hopped || nav->active_audio_id != st->last_audio_id) &&
        mpctx->opts->stream_id[0][STREAM_AUDIO] != -2)
    {
        struct track *t = find_track_by_demuxer_id(mpctx, STREAM_AUDIO,
                                                  nav->active_audio_id);
        if (t) {
            if (mpctx->current_track[0][STREAM_AUDIO] != t) {
                MP_VERBOSE(mpctx, "discnav: disc audio -> demuxer_id 0x%x\n",
                           nav->active_audio_id);
                mp_switch_track_n(mpctx, 0, STREAM_AUDIO, t, 0);
            }
            st->last_audio_id = nav->active_audio_id;
        } else if (nav->active_audio_id >= 0) {
            MP_TRACE(mpctx, "discnav: disc audio 0x%x not in tracks yet\n",
                     nav->active_audio_id);
        }
    } else if (first) {
        st->last_audio_id = nav->active_audio_id;
    }

    if (!first && (hopped ||
                   nav->active_sub_id != st->last_sub_id ||
                   nav->sub_visible != st->last_sub_visible) &&
        !nav->menu_active && !st->menu_selected_track &&
        mpctx->opts->stream_id[0][STREAM_SUB] != -2)
    {
        if (nav->sub_visible) {
            struct track *t = find_track_by_demuxer_id(mpctx, STREAM_SUB,
                                                      nav->active_sub_id);
            if (t) {
                if (mpctx->current_track[0][STREAM_SUB] != t) {
                    MP_VERBOSE(mpctx, "discnav: disc sub -> demuxer_id 0x%x\n",
                               nav->active_sub_id);
                    mp_switch_track_n(mpctx, 0, STREAM_SUB, t, 0);
                }
                st->last_sub_id = nav->active_sub_id;
                st->last_sub_visible = true;
            } else if (nav->active_sub_id >= 0) {
                MP_TRACE(mpctx, "discnav: disc sub 0x%x not in tracks yet\n",
                         nav->active_sub_id);
            }
        } else {
            if (mpctx->current_track[0][STREAM_SUB]) {
                MP_VERBOSE(mpctx, "discnav: disc sub -> off\n");
                mp_switch_track_n(mpctx, 0, STREAM_SUB, NULL, 0);
            }
            st->last_sub_id = nav->active_sub_id;
            st->last_sub_visible = false;
        }
    } else if (first) {
        st->last_sub_id = nav->active_sub_id;
        st->last_sub_visible = nav->sub_visible;
    }

    if (first) {
        st->last_angle = nav->angle;
    } else if (nav->angle > 0 && nav->angle != st->last_angle) {
        MP_VERBOSE(mpctx, "discnav: disc angle %d -> %d (of %d)\n",
                   st->last_angle, nav->angle, nav->num_angles);
        st->last_angle = nav->angle;
        mp_notify_property(mpctx, "angle");
    }
}

// Make sure a dvd_subtitle track is selected while a menu is visible,
// so the SPU graphics decoded by sd_lavc reach the OSD.
static void ensure_menu_sub_selection(struct MPContext *mpctx, bool menu_on)
{
    struct disc_nav_state *st = get_state(mpctx);
    struct track *cur = mpctx->current_track[0][STREAM_SUB];

    // If the track list got rebuilt under us (e.g. another file/disc loaded
    // mid-session) the cached pointers could be stale. Trust only what we
    // can still see in mpctx->tracks.
    for (int i = 0; i < 2; i++) {
        struct track **slot = i ? &st->menu_saved_track : &st->menu_selected_track;
        if (!*slot)
            continue;
        bool still_present = false;
        for (int n = 0; n < mpctx->num_tracks; n++) {
            if (mpctx->tracks[n] == *slot) {
                still_present = true;
                break;
            }
        }
        if (!still_present)
            *slot = NULL;
    }

    if (menu_on) {
        // Re-checked every frame; other selectors (stream auto-select, slave
        // reopens) can change the sub under us.
        if (st->menu_selected_track && cur == st->menu_selected_track)
            return;
        // A dvd_subtitle track is already active; nothing to force.
        if (is_dvd_sub_track(cur))
            return;
        if (mpctx->opts->stream_id[0][STREAM_SUB] == -2)
            return;
        struct track *pick = NULL;
        for (int n = 0; n < mpctx->num_tracks; n++) {
            if (is_dvd_sub_track(mpctx->tracks[n])) {
                pick = mpctx->tracks[n];
                break;
            }
        }
        if (!pick)
            return;
        // Remember the displaced selection only on the first override.
        if (!st->menu_selected_track)
            st->menu_saved_track = cur;
        mp_switch_track_n(mpctx, 0, STREAM_SUB, pick, 0);
        st->menu_selected_track = pick;
    } else {
        if (!st->menu_selected_track)
            return;
        // Only revert if our override is still the active selection.
        if (cur == st->menu_selected_track)
            mp_switch_track_n(mpctx, 0, STREAM_SUB, st->menu_saved_track, 0);
        st->menu_selected_track = NULL;
        st->menu_saved_track = NULL;
    }
}

void disc_nav_update(struct MPContext *mpctx)
{
    struct stream *s = disc_nav_get_stream(mpctx);
    struct stream_nav_state nav = {0};
    bool have = s && stream_control(s, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1;
    bool still = have && nav.still_active;
    if (still != mpctx->disc_nav_still_frame)
        MP_VERBOSE(mpctx, "discnav: still_frame %d->%d\n",
                   mpctx->disc_nav_still_frame, still);
    mpctx->disc_nav_still_frame = still;
    if (!have) {
        struct disc_nav_state *st = mpctx->disc_nav;
        if (!st)
            return;
        if (st->overlay_visible) {
            osd_set_bitmaps(mpctx->osd, OSDTYPE_DISC_MENU, NULL);
            mp_input_disable_section(mpctx->input, "discnav");
            st->overlay_visible = false;
        }
        st->bd_last_change_id = 0;
        st->bd_last_vo_res = (struct mp_osd_res){0};
        st->menu_selected_track = NULL;
        st->menu_saved_track = NULL;
        st->overlay_change_seen = false;
        return;
    }

    struct disc_nav_state *st = get_state(mpctx);

    check_async_discontinuity(mpctx, &nav);
    sync_current_edition(mpctx, s, &nav);
    sync_disc_track_selection(mpctx, s, &nav);

    bool is_bd = strcmp(s->info->name, "bd") == 0 || strcmp(s->info->name, "bdmv/bluray") == 0;
    bool visible = nav.menu_active && (is_bd || (nav.hl_w > 0 && nav.hl_h > 0));

    if (is_bd) {
        push_bd_overlay(mpctx, s, &nav, visible);
    } else {
        push_dvd_overlay(mpctx, &nav, visible);
        ensure_menu_sub_selection(mpctx, nav.menu_active);
    }

    // The menu overlay updates independently of video. When it changes while
    // the video isn't producing frames (held on a still, or paused) nothing
    // would repaint it, so request an OSD redraw.
    if (!st->overlay_change_seen || nav.change_id != st->last_overlay_change_id) {
        st->overlay_change_seen = true;
        st->last_overlay_change_id = nav.change_id;
        osd_changed(mpctx->osd);
    }

    if (visible != st->overlay_visible) {
        MP_VERBOSE(mpctx, "discnav: overlay %s\n", visible ? "on" : "off");
        if (visible) {
            mp_input_enable_section(mpctx->input, "discnav", MP_INPUT_ON_TOP);
        } else {
            mp_input_disable_section(mpctx->input, "discnav");
        }
        st->overlay_visible = visible;
    }
}
