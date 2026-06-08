/*
 * Copyright (C) 2010 Benjamin Zores <ben@geexbox.org>
 *
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

/*
 * Blu-ray parser/reader using libbluray
 *  Use 'git clone git://git.videolan.org/libbluray' to get it.
 *
 * TODO:
 *  - Add descrambled keys database support (KEYDB.cfg)
 *
 */

#include <string.h>
#include <assert.h>

#include <libbluray/bluray.h>
#include <libbluray/meta_data.h>
#include <libbluray/overlay.h>
#include <libbluray/keys.h>
#include <libbluray/bluray-version.h>
#include <libbluray/log_control.h>
#include <libavutil/common.h>

#include "config.h"
#include "mpv_talloc.h"
#include "common/common.h"
#include "common/msg.h"
#include "misc/thread_tools.h"
#include "options/m_config.h"
#include "options/options.h"
#include "options/path.h"
#include "osdep/threads.h"
#include "stream.h"
#include "osdep/io.h"
#include "osdep/timer.h"
#include "sub/osd.h"
#include "sub/img_convert.h"
#include "video/csputils.h"
#include "video/mp_image.h"

#define BLURAY_SECTOR_SIZE     6144

#define BLURAY_DEFAULT_ANGLE      0
#define BLURAY_DEFAULT_CHAPTER    0
#define BLURAY_PLAYLIST_TITLE    -3
#define BLURAY_DEFAULT_TITLE     -2
#define BLURAY_MENU_TITLE        -1

// 90khz ticks
#define BD_TIMEBASE (90000)
#define BD_TIME_TO_MP(x) ((x) / (double)(BD_TIMEBASE))
#define BD_TIME_FROM_MP(x) ((uint64_t)(x * BD_TIMEBASE))

// copied from aacs.h in libaacs
#define AACS_ERROR_CORRUPTED_DISC -1 /* opening or reading of AACS files failed */
#define AACS_ERROR_NO_CONFIG      -2 /* missing config file */
#define AACS_ERROR_NO_PK          -3 /* no matching processing key */
#define AACS_ERROR_NO_CERT        -4 /* no valid certificate */
#define AACS_ERROR_CERT_REVOKED   -5 /* certificate has been revoked */
#define AACS_ERROR_MMC_OPEN       -6 /* MMC open failed (no MMC drive ?) */
#define AACS_ERROR_MMC_FAILURE    -7 /* MMC failed */
#define AACS_ERROR_NO_DK          -8 /* no matching device key */


#define OPT_BASE_STRUCT struct mp_bluray_opts
const struct m_sub_options stream_bluray_conf = {
    .opts = (const struct m_option[]) {
        {"device", OPT_STRING(bluray_device), .flags = M_OPT_FILE},
        {"angle", OPT_INT(angle), M_RANGE(1, 999)},
        {0},
    },
    .size = sizeof(struct mp_bluray_opts),
    .defaults = &(const struct mp_bluray_opts){
        .angle = 1,
    },
};

// One overlay plane (BGRA, premultiplied alpha).
struct bd_overlay_plane {
    uint32_t *work;
    uint32_t *publish;
    int w, h;           // current allocation size (0 if unallocated)
    bool visible;       // publish has any non-zero alpha pixel
};

struct bluray_priv_s {
    BLURAY *bd;
    BLURAY_TITLE_INFO *title_info;
    int num_titles;
    int current_angle;
    int current_title;
    int current_playlist;

    // Cached map from filtered title index (0..num_titles-1) to mpls_id.
    uint32_t *title_to_playlist;

    int cfg_title;
    int cfg_playlist;
    char *cfg_device;

    struct mp_bluray_opts *opts;
    struct m_config_cache *opts_cache;

    // Disc-menu support (enabled when cfg_title == BLURAY_MENU_TITLE).
    // The HDMV graphics controller emits IG-plane primitives through
    // bd_register_overlay_proc (YUV+RLE). BD-J titles bypass it entirely
    // and emit fully-rendered ARGB on both PG and IG planes through
    // bd_register_argb_overlay_proc. Discs that mix HDMV first-play with
    // BD-J menus (or vice versa) need both callbacks registered.
    bool hdmv_mode;
    struct bd_overlay_plane ig;
    struct bd_overlay_plane pg;
    mp_mutex overlay_lock;           // guards plane fields + visibility flags

    bool menu_event_active;          // BD_EVENT_MENU == 1
    bool popup_supported;            // BD_EVENT_POPUP == 1
    uint32_t nav_change_id;          // bumped on FLUSH/HIDE/MENU/POPUP events
    uint32_t discontinuity_id;       // bumped on actions that may hop (SELECT...)
    bool data_delivered;             // any byte returned from fill_buffer yet

    // Disc-driven audio/sub selection, mirrored from BD_EVENT_AUDIO_STREAM
    // and BD_EVENT_PG_TEXTST{,_STREAM}. The numbers are 1-based libbluray
    // stream indices; we resolve to MPEG-TS PIDs via title_info on demand.
    // 0 = unknown (no event seen yet), 0xff/0xfff = "none" sentinel from BD.
    int audio_stream_num;
    int sub_stream_num;
    bool sub_visible;

    int mouse_x, mouse_y;
};

// Lazy (re-)allocation for an overlay plane's working+publish buffer pair.
static bool bd_overlay_ensure(struct bluray_priv_s *priv,
                              struct bd_overlay_plane *p, int w, int h)
{
    if (w <= 0 || h <= 0)
        return false;
    if (p->work && w <= p->w && h <= p->h)
        return true;
    int nw = MPMAX(w, p->w);
    int nh = MPMAX(h, p->h);
    size_t bytes = (size_t)nw * nh * 4;
    uint32_t *work = talloc_realloc(priv, p->work,    uint32_t, nw * nh);
    uint32_t *pub  = talloc_realloc(priv, p->publish, uint32_t, nw * nh);
    if (!work || !pub)
        return false;
    memset(work, 0, bytes);
    memset(pub,  0, bytes);
    p->work    = work;
    p->publish = pub;
    p->w = nw;
    p->h = nh;
    return true;
}

static void bd_overlay_clear(struct bd_overlay_plane *p)
{
    if (p->work)
        memset(p->work, 0, (size_t)p->w * p->h * 4);
}

static void bd_overlay_hide(struct bluray_priv_s *priv, struct bd_overlay_plane *p)
{
    p->visible = false;
    priv->nav_change_id++;
}

static void bd_overlay_flush(struct bluray_priv_s *priv, struct bd_overlay_plane *p)
{
    if (!p->work)
        return;
    size_t n = (size_t)p->w * p->h;
    memcpy(p->publish, p->work, n * 4);
    bool any = false;
    for (size_t i = 0; i < n; i++) {
        if (p->publish[i] & 0xFF000000) {
            any = true;
            break;
        }
    }
    p->visible = any;
    priv->nav_change_id++;
}

static enum pl_color_system bd_overlay_csp(struct bluray_priv_s *priv)
{
    const BLURAY_TITLE_INFO *ti = priv->title_info;
    if (!ti || !ti->clip_count || !ti->clips[0].video_stream_count)
        return PL_COLOR_SYSTEM_BT_709;
    const BLURAY_STREAM_INFO *vs = &ti->clips[0].video_streams[0];
    switch (vs->format) {
    case BLURAY_VIDEO_FORMAT_2160P: // UHD: always BT.2020
        return PL_COLOR_SYSTEM_BT_2020_NC;
    case BLURAY_VIDEO_FORMAT_480I:  // ITU-R BT.601
    case BLURAY_VIDEO_FORMAT_576I:
    case BLURAY_VIDEO_FORMAT_480P:
    case BLURAY_VIDEO_FORMAT_576P:
        return PL_COLOR_SYSTEM_BT_601;
    default:
        return PL_COLOR_SYSTEM_BT_709;
    }
}

static void bd_palette_to_bgra(const BD_PG_PALETTE_ENTRY *pg, uint32_t out[256],
                               enum pl_color_system csp)
{
    struct mp_csp_params params = MP_CSP_PARAMS_DEFAULTS;
    params.repr.sys = csp;
    params.repr.levels = PL_COLOR_LEVELS_LIMITED;
    params.levels_out = PL_COLOR_LEVELS_FULL;
    struct pl_transform3x3 yuv2rgb;
    mp_get_csp_matrix(&params, &yuv2rgb);

    for (int i = 0; i < 256; i++) {
        int yuv[3] = { pg[i].Y, pg[i].Cb, pg[i].Cr };
        int rgb[3];
        mp_map_fixp_color(&yuv2rgb, 8, yuv, 8, rgb);
        int T = pg[i].T;
        // Pre-multiply RGB by alpha so the OSD layer can composite directly.
        int r = rgb[0] * T / 255;
        int g = rgb[1] * T / 255;
        int b = rgb[2] * T / 255;
        out[i] = ((uint32_t)T << 24) | ((uint32_t)r << 16) |
                 ((uint32_t)g << 8)  |  (uint32_t)b;
    }
    // palette index 0xFF is always transparent.
    out[0xFF] = 0;
}

// Composite one RLE-encoded sub-bitmap into the IG plane at (ov->x, ov->y).
static void bd_overlay_draw_rle(struct bd_overlay_plane *p, const BD_OVERLAY *ov,
                                enum pl_color_system csp)
{
    if (!ov->img || !ov->palette || ov->w <= 0 || ov->h <= 0)
        return;
    uint32_t pal[256];
    bd_palette_to_bgra(ov->palette, pal, csp);

    const BD_PG_RLE_ELEM *rle = ov->img;
    for (int y = 0; y < ov->h; y++) {
        int dst_y = ov->y + y;
        bool in_plane = dst_y >= 0 && dst_y < p->h;
        uint32_t *dst_row = in_plane ? p->work + (size_t)dst_y * p->w : NULL;
        int x = 0;
        while (x < ov->w) {
            int len = rle->len;
            int color = rle->color;
            rle++;
            if (len == 0)
                continue; // stray EOL, skip
            int dst_x = ov->x + x;
            int run = len;
            if (dst_x < 0) {
                int skip = MPMIN(-dst_x, run);
                run -= skip;
                dst_x += skip;
            }
            if (dst_x + run > p->w)
                run = p->w - dst_x;
            if (dst_row && run > 0) {
                uint32_t c = pal[color & 0xFF];
                for (int i = 0; i < run; i++)
                    dst_row[dst_x + i] = c;
            }
            x += len;
        }
        if (rle->len == 0)
            rle++;
    }
}

// Called by libbluray's HDMV graphics controller for every overlay primitive
// on either plane. We only render the IG (menu) plane; PG (subtitles) flows
// through the regular demuxer/sd_lavc pipeline.
static void bd_yuv_overlay_cb(void *handle, const struct bd_overlay_s *ov)
{
    struct bluray_priv_s *priv = handle;
    if (!ov)
        return;
    if (ov->plane != BD_OVERLAY_IG)
        return;

    struct bd_overlay_plane *p = &priv->ig;
    mp_mutex_lock(&priv->overlay_lock);
    switch (ov->cmd) {
    case BD_OVERLAY_INIT:
        bd_overlay_ensure(priv, p, ov->w, ov->h);
        bd_overlay_clear(p);
        bd_overlay_hide(priv, p);
        break;
    case BD_OVERLAY_CLOSE:
    case BD_OVERLAY_HIDE:
        bd_overlay_hide(priv, p);
        break;
    case BD_OVERLAY_CLEAR:
        bd_overlay_clear(p);
        break;
    case BD_OVERLAY_WIPE:
        if (p->work) {
            for (int y = 0; y < ov->h; y++) {
                int dy = ov->y + y;
                if (dy < 0 || dy >= p->h)
                    continue;
                int dx = MPMAX(ov->x, 0);
                int run = MPMIN(ov->w, p->w - dx);
                if (run > 0)
                    memset(p->work + (size_t)dy * p->w + dx, 0, run * 4);
            }
        }
        break;
    case BD_OVERLAY_DRAW:
        if (p->work)
            bd_overlay_draw_rle(p, ov, bd_overlay_csp(priv));
        break;
    case BD_OVERLAY_FLUSH:
        bd_overlay_flush(priv, p);
        break;
    default:
        break;
    }
    mp_mutex_unlock(&priv->overlay_lock);
}

static inline uint32_t bd_argb_premul(uint32_t src)
{
    uint32_t a = (src >> 24) & 0xFF;
    if (a == 0)
        return 0;
    if (a == 255)
        return src;
    uint32_t r = (src >> 16) & 0xFF;
    uint32_t g = (src >>  8) & 0xFF;
    uint32_t b =  src        & 0xFF;
    r = (r * a + 127) / 255;
    g = (g * a + 127) / 255;
    b = (b * a + 127) / 255;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static void bd_overlay_draw_argb(struct bd_overlay_plane *p,
                                 const BD_ARGB_OVERLAY *ov)
{
    if (!ov->argb || ov->w <= 0 || ov->h <= 0)
        return;
    int sx = 0, sy = 0;
    int dx = ov->x, dy = ov->y;
    int w = ov->w, h = ov->h;
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (dx + w > p->w) w = p->w - dx;
    if (dy + h > p->h) h = p->h - dy;
    if (w <= 0 || h <= 0)
        return;
    for (int y = 0; y < h; y++) {
        const uint32_t *src = ov->argb + (size_t)(sy + y) * ov->stride + sx;
        uint32_t *dst = p->work + (size_t)(dy + y) * p->w + dx;
        for (int x = 0; x < w; x++)
            dst[x] = bd_argb_premul(src[x]);
    }
}

// Called by libbluray when a BD-J title paints into either the PG or IG plane.
static void bd_argb_overlay_cb(void *handle, const BD_ARGB_OVERLAY *ov)
{
    struct bluray_priv_s *priv = handle;
    if (!ov)
        return;
    if (ov->plane != BD_OVERLAY_PG && ov->plane != BD_OVERLAY_IG)
        return;

    struct bd_overlay_plane *p = ov->plane == BD_OVERLAY_IG ? &priv->ig
                                                            : &priv->pg;
    mp_mutex_lock(&priv->overlay_lock);
    switch (ov->cmd) {
    case BD_ARGB_OVERLAY_INIT:
        bd_overlay_ensure(priv, p, ov->w, ov->h);
        bd_overlay_clear(p);
        bd_overlay_hide(priv, p);
        break;
    case BD_ARGB_OVERLAY_CLOSE:
        bd_overlay_hide(priv, p);
        break;
    case BD_ARGB_OVERLAY_DRAW:
        if (p->work)
            bd_overlay_draw_argb(p, ov);
        break;
    case BD_ARGB_OVERLAY_FLUSH:
        bd_overlay_flush(priv, p);
        break;
    default:
        break;
    }
    mp_mutex_unlock(&priv->overlay_lock);
}

inline static int play_playlist(struct bluray_priv_s *priv, int playlist)
{
    return bd_select_playlist(priv->bd, playlist);
}

inline static int play_title(struct bluray_priv_s *priv, int title)
{
    return bd_select_title(priv->bd, title);
}

static void bluray_stream_close(stream_t *s)
{
    struct bluray_priv_s *priv = s->priv;
    if (!priv)
        return;

    if (priv->title_info)
        bd_free_title_info(priv->title_info);
    if (priv->bd) {
        if (priv->hdmv_mode) {
            bd_register_overlay_proc(priv->bd, NULL, NULL);
            bd_register_argb_overlay_proc(priv->bd, NULL, NULL, NULL);
        }
        bd_close(priv->bd);
    }
    if (priv->hdmv_mode)
        mp_mutex_destroy(&priv->overlay_lock);
}

static void handle_event(stream_t *s, const BD_EVENT *ev)
{
    struct bluray_priv_s *b = s->priv;
    if (b->hdmv_mode)
        MP_VERBOSE(s, "bdnav: event %d param %u\n", ev->event, ev->param);
    switch (ev->event) {
    case BD_EVENT_MENU:
        // ev->param: 1 if the disc is currently in an HDMV menu, 0 otherwise.
        if (b->hdmv_mode) {
            mp_mutex_lock(&b->overlay_lock);
            b->menu_event_active = ev->param != 0;
            b->nav_change_id++;
            mp_mutex_unlock(&b->overlay_lock);
        }
        break;
    case BD_EVENT_STILL:
        if (ev->param)
            bd_read_skip_still(b->bd);
        break;
    case BD_EVENT_STILL_TIME:
        bd_read_skip_still(b->bd);
        break;
    case BD_EVENT_END_OF_TITLE:
        break;
    case BD_EVENT_PLAYLIST:
        b->current_playlist = ev->param;
        b->current_title = bd_get_current_title(b->bd);
        if (b->title_to_playlist) {
            for (int i = 0; i < b->num_titles; i++) {
                if (b->title_to_playlist[i] == (uint32_t)ev->param) {
                    b->current_title = i;
                    break;
                }
            }
        }
        if (b->title_info)
            bd_free_title_info(b->title_info);
        b->title_info = bd_get_playlist_info(b->bd, b->current_playlist,
                                             b->current_angle);
        if (b->hdmv_mode) {
            mp_mutex_lock(&b->overlay_lock);
            b->discontinuity_id++;
            mp_mutex_unlock(&b->overlay_lock);
        }
        break;
    case BD_EVENT_TITLE:
        b->current_title = bd_get_current_title(b->bd);
        if (b->title_info) {
            bd_free_title_info(b->title_info);
            b->title_info = NULL;
        }
        if (b->hdmv_mode) {
            mp_mutex_lock(&b->overlay_lock);
            b->discontinuity_id++;
            mp_mutex_unlock(&b->overlay_lock);
        }
        break;
    case BD_EVENT_ANGLE:
        b->current_angle = ev->param;
        if (b->title_info) {
            bd_free_title_info(b->title_info);
            b->title_info = bd_get_playlist_info(b->bd, b->current_playlist,
                                                 b->current_angle);
        }
        if (b->hdmv_mode) {
            mp_mutex_lock(&b->overlay_lock);
            b->nav_change_id++;
            mp_mutex_unlock(&b->overlay_lock);
        }
        break;
    case BD_EVENT_AUDIO_STREAM:
        b->audio_stream_num = ev->param;
        if (b->hdmv_mode) {
            mp_mutex_lock(&b->overlay_lock);
            b->nav_change_id++;
            mp_mutex_unlock(&b->overlay_lock);
        }
        break;
    case BD_EVENT_PG_TEXTST_STREAM:
        b->sub_stream_num = ev->param;
        if (b->hdmv_mode) {
            mp_mutex_lock(&b->overlay_lock);
            b->nav_change_id++;
            mp_mutex_unlock(&b->overlay_lock);
        }
        break;
    case BD_EVENT_PG_TEXTST:
        b->sub_visible = ev->param != 0;
        if (b->hdmv_mode) {
            mp_mutex_lock(&b->overlay_lock);
            b->nav_change_id++;
            mp_mutex_unlock(&b->overlay_lock);
        }
        break;
    case BD_EVENT_POPUP:
        // ev->param: 1 if popup menu is currently available, 0 otherwise.
        if (b->hdmv_mode) {
            mp_mutex_lock(&b->overlay_lock);
            b->popup_supported = ev->param != 0;
            b->nav_change_id++;
            mp_mutex_unlock(&b->overlay_lock);
        }
        break;
#if BLURAY_VERSION >= BLURAY_VERSION_CODE(0, 5, 0)
    case BD_EVENT_DISCONTINUITY:
        break;
#endif
    default:
        MP_TRACE(s, "Unhandled event: %d %d\n", ev->event, ev->param);
        break;
    }
}

static int bluray_stream_fill_buffer(stream_t *s, void *buf, int len)
{
    struct bluray_priv_s *b = s->priv;
    BD_EVENT event;

    if (b->hdmv_mode) {
        // bd_read() doesn't drive the HDMV VM, so the disc's first-play
        // bytecode would never run and we'd be stuck with "no valid title"
        // forever. bd_read_ext() runs the VM between event drains and also
        // delivers one event per call, which we hand off to handle_event.
        while (bd_get_event(b->bd, &event))
            handle_event(s, &event);
        int total = 0;
        int events_seen = 0;
        // Loop briefly to absorb event-only returns (where bd_read_ext
        // returns 0 with a freshly produced event) before reporting EOF.
        // If an event bumps discontinuity_id (PLAYLIST/TITLE) *after* we
        // have already delivered data to the slave demuxer, stop here even
        // if no data was read: the next bd_read_ext would deliver data from
        // the new playlist, but the slave must be reopened first so it
        // parses with the correct codec context.
        for (int i = 0; i < 200; i++) {
            uint32_t disc_before = b->discontinuity_id;
            int n = bd_read_ext(b->bd, (uint8_t *)buf + total, len - total, &event);
            if (n < 0) {
                MP_VERBOSE(s, "bdnav: bd_read_ext err iter=%d\n", i);
                return -1;
            }
            if (event.event != BD_EVENT_NONE) {
                handle_event(s, &event);
                events_seen++;
            }
            if (n > 0) {
                total += n;
                break;
            }
            if (b->data_delivered && b->discontinuity_id != disc_before)
                break;
            if (mp_cancel_test(s->cancel))
                return 0;
            mp_sleep_ns(MP_TIME_MS_TO_NS(5));
        }
        if (total > 0)
            b->data_delivered = true;
        if (total == 0)
            MP_VERBOSE(s, "bdnav: fill returned 0 (events=%d)\n", events_seen);
        return total;
    }

    while (bd_get_event(b->bd, &event))
        handle_event(s, &event);
    return bd_read(b->bd, buf, len);
}

static int bluray_stream_control(stream_t *s, int cmd, void *arg)
{
    struct bluray_priv_s *b = s->priv;

    switch (cmd) {
    case STREAM_CTRL_GET_NUM_CHAPTERS: {
        const BLURAY_TITLE_INFO *ti = b->title_info;
        if (!ti)
            return STREAM_UNSUPPORTED;
        *((unsigned int *) arg) = ti->chapter_count;
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_CHAPTER_TIME: {
        const BLURAY_TITLE_INFO *ti = b->title_info;
        if (!ti)
            return STREAM_UNSUPPORTED;
        int chapter = *(double *)arg;
        double time = MP_NOPTS_VALUE;
        if (chapter >= 0 && chapter < ti->chapter_count)
            time = BD_TIME_TO_MP(ti->chapters[chapter].start);
        if (time == MP_NOPTS_VALUE)
            return STREAM_ERROR;
        *(double *)arg = time;
        return STREAM_OK;
    }
    case STREAM_CTRL_SET_CURRENT_TITLE: {
        const uint32_t title = *((unsigned int*)arg);
        // demux_disc appends a synthetic "Disc Menu" edition at index num_titles.
        if (title == b->num_titles) {
            if (!b->hdmv_mode)
                return STREAM_UNSUPPORTED;
            bd_menu_call(b->bd, -1);
            return STREAM_OK;
        }
        if (title >= b->num_titles || !play_title(b, title))
            return STREAM_UNSUPPORTED;
        b->current_title = title;
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_CURRENT_TITLE: {
        *((unsigned int *) arg) = b->current_title;
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_NUM_TITLES: {
        *((unsigned int *)arg) = b->num_titles;
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_TIME_LENGTH: {
        const BLURAY_TITLE_INFO *ti = b->title_info;
        if (!ti)
            return STREAM_UNSUPPORTED;
        *((double *) arg) = BD_TIME_TO_MP(ti->duration);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_CURRENT_TIME: {
        *((double *) arg) = BD_TIME_TO_MP(bd_tell_time(b->bd));
        return STREAM_OK;
    }
    case STREAM_CTRL_SEEK_TO_TIME: {
        double pts = *((double *) arg);
        bd_seek_time(b->bd, BD_TIME_FROM_MP(pts));
        stream_drop_buffers(s);
        // API makes it hard to determine seeking success
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_NUM_ANGLES: {
        const BLURAY_TITLE_INFO *ti = b->title_info;
        if (!ti)
            return STREAM_UNSUPPORTED;
        *((int *) arg) = ti->angle_count;
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_ANGLE: {
        *((int *) arg) = b->current_angle + 1;
        return STREAM_OK;
    }
    case STREAM_CTRL_SET_ANGLE: {
        const BLURAY_TITLE_INFO *ti = b->title_info;
        if (!ti)
            return STREAM_UNSUPPORTED;
        int angle = *((int *) arg);
        if (angle < 1 || angle > ti->angle_count)
            return STREAM_UNSUPPORTED;
        b->current_angle = angle - 1;
        bd_seamless_angle_change(b->bd, b->current_angle);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_TITLE_LENGTH: {
        int title = *(double *)arg;
        if (!b->bd || title < 0 || title >= b->num_titles)
            return STREAM_UNSUPPORTED;
        BLURAY_TITLE_INFO *ti = bd_get_title_info(b->bd, title, 0);
        if (!ti)
            return STREAM_UNSUPPORTED;
        *(double *)arg = BD_TIME_TO_MP(ti->duration);
        bd_free_title_info(ti);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_TITLE_PLAYLIST: {
        int title = *(double *)arg;
        if (!b->bd || title < 0 || title >= b->num_titles)
            return STREAM_UNSUPPORTED;
        BLURAY_TITLE_INFO *ti = bd_get_title_info(b->bd, title, 0);
        if (!ti)
            return STREAM_UNSUPPORTED;
        *(double *)arg = ti->playlist;
        bd_free_title_info(ti);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_LANG: {
        const BLURAY_TITLE_INFO *ti = b->title_info;
        if (ti && ti->clip_count) {
            struct stream_lang_req *req = arg;
            BLURAY_STREAM_INFO *si = NULL;
            int count = 0;
            switch (req->type) {
            case STREAM_AUDIO:
                count = ti->clips[0].audio_stream_count;
                si = ti->clips[0].audio_streams;
                break;
            case STREAM_SUB:
                count = ti->clips[0].pg_stream_count;
                si = ti->clips[0].pg_streams;
                break;
            }
            for (int n = 0; n < count; n++) {
                BLURAY_STREAM_INFO *i = &si[n];
                if (i->pid == req->id) {
                    snprintf(req->name, sizeof(req->name), "%.4s", i->lang);
                    return STREAM_OK;
                }
            }
        }
        return STREAM_ERROR;
    }
    case STREAM_CTRL_GET_DISC_NAME: {
        const struct meta_dl *meta = bd_get_meta(b->bd);
        if (!meta || !meta->di_name || !meta->di_name[0])
            break;
        *(char**)arg = talloc_strdup(NULL, meta->di_name);
        return STREAM_OK;
    }
    case STREAM_CTRL_NAV_CMD: {
        if (!b->hdmv_mode)
            return STREAM_UNSUPPORTED;
        struct stream_nav_cmd *nav = arg;
        uint32_t key = BD_VK_NONE;
        switch (nav->action) {
        case STREAM_NAV_UP:
            key = BD_VK_UP;
            break;
        case STREAM_NAV_DOWN:
            key = BD_VK_DOWN;
            break;
        case STREAM_NAV_LEFT:
            key = BD_VK_LEFT;
            break;
        case STREAM_NAV_RIGHT:
            key = BD_VK_RIGHT;
            break;
        case STREAM_NAV_SELECT:
            key = BD_VK_ENTER;
            break;
        case STREAM_NAV_MENU_ROOT:
        case STREAM_NAV_MENU_TITLE:
            // BD doesn't distinguish "title menu", both map to disc root.
            bd_menu_call(b->bd, -1);
            return STREAM_OK;
        case STREAM_NAV_MENU_POPUP:
            key = BD_VK_POPUP;
            break;
        case STREAM_NAV_PREV_MENU:
            // No dedicated "previous menu" key; popup-toggle is the closest
            // equivalent and behaves like "dismiss current menu" on most
            // discs when already in popup.
            key = BD_VK_POPUP;
            break;
        case STREAM_NAV_MOUSE_MOVE:
            b->mouse_x = nav->x;
            b->mouse_y = nav->y;
            bd_mouse_select(b->bd, -1, nav->x, nav->y);
            return STREAM_OK;
        case STREAM_NAV_MOUSE_CLICK:
            b->mouse_x = nav->x;
            b->mouse_y = nav->y;
            bd_mouse_select(b->bd, -1, nav->x, nav->y);
            key = BD_VK_MOUSE_ACTIVATE;
            break;
        }
        if (key != BD_VK_NONE)
            bd_user_input(b->bd, -1, key);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_NAV_STATE: {
        struct stream_nav_state *st = arg;
        int audio_pid = -1;
        int sub_pid = -1;
        const BLURAY_TITLE_INFO *ti = b->title_info;
        if (ti && ti->clip_count) {
            const BLURAY_CLIP_INFO *ci = &ti->clips[0];
            if (b->audio_stream_num >= 1 &&
                b->audio_stream_num <= ci->audio_stream_count)
            {
                audio_pid = ci->audio_streams[b->audio_stream_num - 1].pid;
            }
            if (b->sub_stream_num >= 1 &&
                b->sub_stream_num <= ci->pg_stream_count)
            {
                sub_pid = ci->pg_streams[b->sub_stream_num - 1].pid;
            }
        }
        if (!b->hdmv_mode) {
            // Even outside HDMV we can carry disc-driven audio/sub/angle so
            // the player tracks the disc author's defaults on a plain-title
            // playback.
            *st = (struct stream_nav_state){
                .active_audio_id = audio_pid,
                .active_sub_id = sub_pid,
                .sub_visible = b->sub_visible,
                .angle = b->current_angle + 1,
                .num_angles = ti ? ti->angle_count : 0,
            };
            return STREAM_OK;
        }
        mp_mutex_lock(&b->overlay_lock);
        // BD_EVENT_MENU isn't fired by BD-J (it's HDMV-only), so treat any
        // visible BD-J plane as an active menu too.
        bool any_overlay = b->ig.visible || b->pg.visible;
        bool visible = (b->menu_event_active && b->ig.visible) || any_overlay;
        *st = (struct stream_nav_state){
            .nav_active = true,
            .menu_active = visible,
            .has_popup = b->popup_supported,
            .src_w = MPMAX(b->ig.w, b->pg.w),
            .src_h = MPMAX(b->ig.h, b->pg.h),
            .change_id = b->nav_change_id,
            .discontinuity_id = b->discontinuity_id,
            .active_audio_id = audio_pid,
            .active_sub_id = sub_pid,
            .sub_visible = b->sub_visible,
            .angle = b->current_angle + 1,
            .num_angles = ti ? ti->angle_count : 0,
        };
        mp_mutex_unlock(&b->overlay_lock);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_NAV_OVERLAY: {
        if (!b->hdmv_mode)
            return STREAM_UNSUPPORTED;
        struct stream_nav_overlay_req *req = arg;
        if (!req->dst || req->w <= 0 || req->h <= 0)
            return STREAM_ERROR;
        mp_mutex_lock(&b->overlay_lock);
        int copy_w = MPMIN(req->w, MPMAX(b->ig.w, b->pg.w));
        int copy_h = MPMIN(req->h, MPMAX(b->ig.h, b->pg.h));
        const uint32_t *ig_src = b->ig.visible ? b->ig.publish : NULL;
        const uint32_t *pg_src = b->pg.visible ? b->pg.publish : NULL;
        for (int y = 0; y < copy_h; y++) {
            uint32_t *dst = (uint32_t *)(req->dst + y * req->stride);
            const uint32_t *ig_row = (ig_src && y < b->ig.h)
                ? ig_src + (size_t)y * b->ig.w : NULL;
            const uint32_t *pg_row = (pg_src && y < b->pg.h)
                ? pg_src + (size_t)y * b->pg.w : NULL;
            int ig_lim = ig_row ? b->ig.w : 0;
            int pg_lim = pg_row ? b->pg.w : 0;
            for (int x = 0; x < copy_w; x++) {
                uint32_t ig = (x < ig_lim) ? ig_row[x] : 0;
                uint32_t pg = (x < pg_lim) ? pg_row[x] : 0;
                uint32_t ia = (ig >> 24) & 0xFF;
                if (ia == 0) {
                    dst[x] = pg;
                } else if (ia == 0xFF || !pg) {
                    dst[x] = ig;
                } else {
                    // out = ig + pg * (1 - ig.a)
                    uint32_t inv = 255 - ia;
                    uint32_t na = ia                  + ((pg >> 24) & 0xFF) * inv / 255;
                    uint32_t nr = ((ig >> 16) & 0xFF) + ((pg >> 16) & 0xFF) * inv / 255;
                    uint32_t ng = ((ig >>  8) & 0xFF) + ((pg >>  8) & 0xFF) * inv / 255;
                    uint32_t nb = ( ig        & 0xFF) + ( pg        & 0xFF) * inv / 255;
                    dst[x] = (MPMIN(na, 255u) << 24) | (MPMIN(nr, 255u) << 16) |
                             (MPMIN(ng, 255u) <<  8) |  MPMIN(nb, 255u);
                }
            }
        }
        req->change_id = b->nav_change_id;
        req->w = copy_w;
        req->h = copy_h;
        mp_mutex_unlock(&b->overlay_lock);
        return STREAM_OK;
    }
    default:
        break;
    }

    return STREAM_UNSUPPORTED;
}

static const char *aacs_strerr(int err)
{
    switch (err) {
    case AACS_ERROR_CORRUPTED_DISC: return "opening or reading of AACS files failed";
    case AACS_ERROR_NO_CONFIG:      return "missing config file";
    case AACS_ERROR_NO_PK:          return "no matching processing key";
    case AACS_ERROR_NO_CERT:        return "no valid certificate";
    case AACS_ERROR_CERT_REVOKED:   return "certificate has been revoked";
    case AACS_ERROR_MMC_OPEN:       return "MMC open failed (maybe no MMC drive?)";
    case AACS_ERROR_MMC_FAILURE:    return "MMC failed";
    case AACS_ERROR_NO_DK:          return "no matching device key";
    default:                        return "unknown error";
    }
}

static bool check_disc_info(stream_t *s)
{
    struct bluray_priv_s *b = s->priv;
    const BLURAY_DISC_INFO *info = bd_get_disc_info(b->bd);

    // check Blu-ray
    if (!info->bluray_detected) {
        MP_ERR(s, "Given stream is not a Blu-ray.\n");
        return false;
    }

    // check AACS
    if (info->aacs_detected) {
        if (!info->libaacs_detected) {
            MP_ERR(s, "AACS encryption detected but cannot find libaacs.\n");
            return false;
        }
        if (!info->aacs_handled) {
            MP_ERR(s, "AACS error: %s\n", aacs_strerr(info->aacs_error_code));
            return false;
        }
    }

    // check BD+
    if (info->bdplus_detected) {
        if (!info->libbdplus_detected) {
            MP_ERR(s, "BD+ encryption detected but cannot find libbdplus.\n");
            return false;
        }
        if (!info->bdplus_handled) {
            MP_ERR(s, "Cannot decrypt BD+ encryption.\n");
            return false;
        }
    }

    return true;
}

static void select_initial_title(stream_t *s, int title_guess) {
    struct bluray_priv_s *b = s->priv;

    if (b->cfg_title == BLURAY_PLAYLIST_TITLE) {
        if (!play_playlist(b, b->cfg_playlist))
            MP_WARN(s, "Couldn't start playlist '%05d'.\n", b->cfg_playlist);
        b->current_title = bd_get_current_title(b->bd);
    } else {
        int title = -1;
        if (b->cfg_title != BLURAY_DEFAULT_TITLE )
            title = b->cfg_title;
        else
            title = title_guess;
        if (title < 0)
            return;

        if (play_title(b, title))
            b->current_title = title;
        else {
            MP_WARN(s, "Couldn't start title '%d'.\n", title);
            b->current_title = bd_get_current_title(b->bd);
        }
    }
}

static int bluray_stream_open_internal(stream_t *s)
{
    struct bluray_priv_s *b = s->priv;

    struct m_config_cache *opts_cache =
        m_config_cache_alloc(s, s->global, &stream_bluray_conf);

    b->opts_cache = opts_cache;
    b->opts = opts_cache->opts;

    int ret = 0;
    char *device = NULL;
    /* find the requested device */
    if (b->cfg_device && b->cfg_device[0]) {
        device = b->cfg_device;
    } else if (b->opts->bluray_device && b->opts->bluray_device[0]) {
        device = b->opts->bluray_device;
    } else {
        device = DEFAULT_OPTICAL_DEVICE;
    }

    if (!device || !device[0]) {
        MP_ERR(s, "No Blu-ray device/location was specified ...\n");
        ret = STREAM_UNSUPPORTED;
        goto err;
    }

    if (!mp_msg_test(s->log, MSGL_DEBUG))
        bd_set_debug_mask(0);

    /* open device */
    char *device_tmp = mp_get_user_path(NULL, s->global, device);
    BLURAY *bd = bd_open(device_tmp, NULL);
    talloc_free(device_tmp);
    if (!bd) {
        MP_ERR(s, "Couldn't open Blu-ray device: %s\n", device);
        ret = STREAM_UNSUPPORTED;
        goto err;
    }
    b->bd = bd;

    if (!check_disc_info(s)) {
        ret = STREAM_UNSUPPORTED;
        goto err;
    }

    /* check for available titles on disc */
    b->num_titles = bd_get_titles(bd, TITLES_RELEVANT, 0);
    if (!b->num_titles) {
        MP_ERR(s, "Can't find any Blu-ray-compatible title here.\n");
        ret = STREAM_UNSUPPORTED;
        goto err;
    }

    MP_INFO(s, "List of available titles:\n");

    /* parse titles information */
    b->title_to_playlist = talloc_array(b, uint32_t, b->num_titles);
    for (int i = 0; i < b->num_titles; i++) {
        b->title_to_playlist[i] = (uint32_t)-1;
        /* the information we're accessing (duration, playlist, angle count)
         * doesn't depend on the angle */
        BLURAY_TITLE_INFO *ti = bd_get_title_info(bd, i, 0);
        if (!ti)
            continue;

        b->title_to_playlist[i] = ti->playlist;

        char *time = mp_format_time(ti->duration / 90000, false);
        MP_INFO(s, "idx: %3d duration: %s angles: %2d (playlist: %05d.mpls)\n",
                    i, time, ti->angle_count, ti->playlist);
        talloc_free(time);

        bd_free_title_info(ti);
    }

    // these should be set before any callback
    b->current_angle = -1;
    b->current_title = -1;

    // initialize libbluray event queue
    bd_get_event(bd, NULL);

    const BLURAY_DISC_INFO *info = bd_get_disc_info(bd);
    MP_VERBOSE(s, "First play: %i, Top menu: %i, "
                  "HDMV Titles: %i, BD-J Titles: %i, Other: %i\n",
               info->first_play_supported, info->top_menu_supported,
               info->num_hdmv_titles, info->num_bdj_titles,
               info->num_unsupported_titles);

    b->hdmv_mode = b->cfg_title == BLURAY_MENU_TITLE;

    // BD-J menus require a usable Java VM and libbluray.jar.
    if (b->hdmv_mode && info->bdj_detected && !info->bdj_handled) {
        MP_WARN(s, "BD-J menus not supported. Playing without menus. "
                   "Java VM: %d, libbluray.jar: %d\n",
                info->libjvm_detected, info->bdj_handled);
        b->hdmv_mode = false;
        b->cfg_title = BLURAY_DEFAULT_TITLE;
    }

    MP_VERBOSE(s, "bdnav: cfg_title=%d hdmv_mode=%d\n", b->cfg_title, b->hdmv_mode);
    if (b->hdmv_mode) {
        mp_mutex_init(&b->overlay_lock);
        bd_register_overlay_proc(bd, b, bd_yuv_overlay_cb);
        if (info->num_bdj_titles)
            bd_register_argb_overlay_proc(bd, b, bd_argb_overlay_cb, NULL);
        if (!bd_play(bd)) {
            MP_ERR(s, "Couldn't start Blu-ray HDMV playback.\n");
            ret = STREAM_UNSUPPORTED;
            goto err;
        }
        b->current_title = bd_get_current_title(bd);
        MP_VERBOSE(s, "bdnav: HDMV entered; current title=%d\n",
                   b->current_title);
    } else {
        select_initial_title(s, bd_get_main_title(bd));
    }

    // Angle selection is only valid once a playlist has been picked.
    if (!b->hdmv_mode) {
        if (!bd_select_angle(bd, b->opts->angle - 1))
            MP_WARN(s, "Couldn't select angle '%d'.\n", b->opts->angle - 1);
    }

    b->current_angle = bd_get_current_angle(bd);

    s->fill_buffer = bluray_stream_fill_buffer;
    s->close       = bluray_stream_close;
    s->control     = bluray_stream_control;
    s->priv        = b;
    s->demuxer     = "+disc";

    MP_VERBOSE(s, "Blu-ray successfully opened.\n");

    return STREAM_OK;

err:
    bluray_stream_close(s);
    return ret;
}

static int bluray_stream_open(stream_t *s)
{
    struct bluray_priv_s *b = talloc_zero(s, struct bluray_priv_s);
    s->priv = b;

    bstr title, bdevice, rest = { .len = 0 };
    bstr_split_tok(bstr0(s->path), "/", &title, &bdevice);

    b->cfg_title = BLURAY_DEFAULT_TITLE;

    struct MPOpts *opts = mp_get_config_group(s, s->global, &mp_opt_root);
    int edition_id = opts->edition_id;
    bool disc_menu = opts->disc_menu;
    talloc_free(opts);

    if (edition_id >= 0) {
        b->cfg_title = edition_id;
    } else if (title.len == 0 || bstr_equals0(title, "longest") ||
               bstr_equals0(title, "first"))
    {
        b->cfg_title = disc_menu ? BLURAY_MENU_TITLE : BLURAY_DEFAULT_TITLE;
    } else if (bstr_equals0(title, "menu")) {
        b->cfg_title = BLURAY_MENU_TITLE;
    } else if (bstr_equals0(title, "mpls")) {
        bstr_split_tok(bdevice, "/", &title, &bdevice);
        long long pl = bstrtoll(title, &rest, 10);
        if (rest.len) {
            MP_ERR(s, "number expected: '%.*s'\n", BSTR_P(rest));
            return STREAM_ERROR;
        } else if (pl < 0 || 99999 < pl) {
            MP_ERR(s, "invalid playlist: '%.*s', must be in the range 0-99999\n",
                            BSTR_P(title));
            return STREAM_ERROR;
        }
        b->cfg_playlist = pl;
        b->cfg_title    = BLURAY_PLAYLIST_TITLE;
    } else if (title.len) {
        long long t = bstrtoll(title, &rest, 10);
        if (rest.len) {
            MP_ERR(s, "number expected: '%.*s'\n", BSTR_P(rest));
            return STREAM_ERROR;
        } else if (t < 0 || 99999 < t) {
            MP_ERR(s, "invalid title: '%.*s', must be in the range 0-99999\n",
                            BSTR_P(title));
            return STREAM_ERROR;
        }
        b->cfg_title = t;
    }

    b->cfg_device = bstrto0(b, bdevice);

    return bluray_stream_open_internal(s);
}

const stream_info_t stream_info_bluray = {
    .name = "bd",
    .open = bluray_stream_open,
    .protocols = (const char*const[]){ "bd", "br", "bluray", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};

static bool check_bdmv(const char *path)
{
    if (strcasecmp(mp_basename(path), "MovieObject.bdmv"))
        return false;

    FILE *temp = fopen(path, "rb");
    if (!temp)
        return false;

    char data[50];
    bool ret = false;

    if (fread(data, 50, 1, temp) == 1) {
        bstr bdata = {data, 50};
        ret = bstr_startswith0(bdata, "MOBJ0100") || // AVCHD
              bstr_startswith0(bdata, "MOBJ0200") || // Blu-ray
              bstr_startswith0(bdata, "MOBJ0300");   // UHD BD
    }

    fclose(temp);
    return ret;
}

// Destructively remove the current trailing path component.
static void remove_prefix(char *path)
{
    size_t len = strlen(path);
#if HAVE_DOS_PATHS
    const char *seps = "/\\";
#else
    const char *seps = "/";
#endif
    while (len > 0 && !strchr(seps, path[len - 1]))
        len--;
    while (len > 0 && strchr(seps, path[len - 1]))
        len--;
    path[len] = '\0';
}

static int bdmv_dir_stream_open(stream_t *stream)
{
    struct bluray_priv_s *priv = talloc_ptrtype(stream, priv);
    stream->priv = priv;
    struct MPOpts *opts = mp_get_config_group(NULL, stream->global, &mp_opt_root);
    int default_title = opts->edition_id >= 0 ? opts->edition_id
                                              : opts->disc_menu ? BLURAY_MENU_TITLE
                                                                : BLURAY_DEFAULT_TITLE;
    *priv = (struct bluray_priv_s){
        .cfg_title = default_title,
    };
    talloc_free(opts);

    if (!stream->access_references)
        goto unsupported;

    char *path = mp_file_get_path(priv, bstr0(stream->url));
    if (!path)
        goto unsupported;

    // We allow the path to point to a directory containing BDMV/, a
    // directory containing MovieObject.bdmv, or that file itself.
    if (!check_bdmv(path)) {
        // On UNIX, just assume the filename has always this case.
        char *npath = mp_path_join(priv, path, "MovieObject.bdmv");
        if (!check_bdmv(npath)) {
            npath = mp_path_join(priv, path, "BDMV/MovieObject.bdmv");
            if (!check_bdmv(npath))
                goto unsupported;
        }
        path = npath;
    }

    // Go up by 2 levels.
    remove_prefix(path);
    remove_prefix(path);
    priv->cfg_device = path;
    if (strlen(priv->cfg_device) <= 1)
        goto unsupported;

    MP_INFO(stream, "BDMV detected. Redirecting to bluray://\n");
    return bluray_stream_open_internal(stream);

unsupported:
    talloc_free(priv);
    stream->priv = NULL;
    return STREAM_UNSUPPORTED;
}

const stream_info_t stream_info_bdmv_dir = {
    .name = "bdmv/bluray",
    .open = bdmv_dir_stream_open,
    .protocols = (const char*const[]){ "file", "", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};
