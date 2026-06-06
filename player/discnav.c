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
#include "video/mp_image.h"
#include "video/out/vo.h"

#include "core.h"

struct disc_nav_state {
    // True while the disc-menu input section is enabled (a menu is on screen).
    bool overlay_visible;

    // Blu-ray HDMV menu staging. libbluray hands us a pre-composited BGRA IG
    // plane via STREAM_CTRL_GET_NAV_OVERLAY; we copy it into bd_image and
    // forward through osd_set_external2.
    struct mp_image *bd_image;
    uint32_t bd_last_change_id;
    struct mp_osd_res bd_last_vo_res;

    // Last observed disc-nav discontinuity counter (bumped by the stream
    // backend on user nav actions and on libbluray/libdvdnav-internal
    // playlist/title/cell transitions).
    uint32_t last_discontinuity_id;
    bool discontinuity_seen;

    // DVD-only: when a menu opens with no DVD sub track selected, we
    // transiently select one so the menu graphic renders through the normal
    // sd_lavc path. menu_selected_track remembers what we selected so we
    // can deselect it again when the menu closes.
    struct track *menu_selected_track;
};

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

static void push_dvd_overlay(struct MPContext *mpctx,
                             struct stream_nav_state *nav, bool visible)
{
    struct mp_dvdnav_hli hli = {
        .show = visible,
        .change_id = visible ? nav->change_id : 0,
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
        if (t->type != STREAM_SUB || !t->d_sub || !t->stream ||
            !t->stream->codec ||
            strcmp(t->stream->codec->codec, "dvd_subtitle") != 0)
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
            osd_set_external2(mpctx->osd, NULL);
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
    osd_set_external2(mpctx->osd, &imgs);
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
}

// Make sure a dvd_subtitle track is selected while a menu is visible,
// so the SPU graphics decoded by sd_lavc reach the OSD.
static void ensure_menu_sub_selection(struct MPContext *mpctx, bool menu_on)
{
    struct disc_nav_state *st = get_state(mpctx);
    struct track *cur = mpctx->current_track[0][STREAM_SUB];

    // If the track list got rebuilt under us (e.g. another file/disc loaded
    // mid-session) the cached pointer could be stale. Trust only what we
    // can still see in mpctx->tracks.
    if (st->menu_selected_track) {
        bool still_present = false;
        for (int n = 0; n < mpctx->num_tracks; n++) {
            if (mpctx->tracks[n] == st->menu_selected_track) {
                still_present = true;
                break;
            }
        }
        if (!still_present)
            st->menu_selected_track = NULL;
    }

    if (menu_on) {
        if (cur || st->menu_selected_track)
            return;
        if (mpctx->opts->stream_id[0][STREAM_SUB] == -2)
            return;
        struct track *pick = NULL;
        for (int n = 0; n < mpctx->num_tracks; n++) {
            struct track *t = mpctx->tracks[n];
            if (t->type == STREAM_SUB && t->stream && t->stream->codec &&
                t->stream->codec->codec &&
                strcmp(t->stream->codec->codec, "dvd_subtitle") == 0)
            {
                pick = t;
                break;
            }
        }
        if (!pick)
            return;
        mp_switch_track_n(mpctx, 0, STREAM_SUB, pick, 0);
        st->menu_selected_track = pick;
    } else {
        if (!st->menu_selected_track)
            return;
        // Only revert if our override is still the active selection.
        if (cur == st->menu_selected_track)
            mp_switch_track_n(mpctx, 0, STREAM_SUB, NULL, 0);
        st->menu_selected_track = NULL;
    }
}

void disc_nav_update(struct MPContext *mpctx)
{
    struct stream *s = disc_nav_get_stream(mpctx);
    struct stream_nav_state nav = {0};
    bool have = s && stream_control(s, STREAM_CTRL_GET_NAV_STATE, &nav) >= 1;
    if (!have) {
        struct disc_nav_state *st = mpctx->disc_nav;
        if (!st)
            return;
        if (st->overlay_visible) {
            osd_set_external2(mpctx->osd, NULL);
            mp_input_disable_section(mpctx->input, "discnav");
            st->overlay_visible = false;
        }
        st->bd_last_change_id = 0;
        st->bd_last_vo_res = (struct mp_osd_res){0};
        st->menu_selected_track = NULL;
        return;
    }

    struct disc_nav_state *st = get_state(mpctx);

    check_async_discontinuity(mpctx, &nav);
    sync_current_edition(mpctx, s, &nav);

    bool is_bd = strcmp(s->info->name, "bd") == 0 || strcmp(s->info->name, "bdmv/bluray") == 0;
    bool visible = nav.menu_active && (is_bd || (nav.hl_w > 0 && nav.hl_h > 0));

    if (is_bd) {
        push_bd_overlay(mpctx, s, &nav, visible);
    } else {
        push_dvd_overlay(mpctx, &nav, visible);
        ensure_menu_sub_selection(mpctx, nav.menu_active);
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
