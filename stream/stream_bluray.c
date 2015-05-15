/*
 * Copyright (C) 2010 Benjamin Zores <ben@geexbox.org>
 *
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

/*
 * Blu-ray parser/reader using libbluray
 *  Use 'git clone git://git.videolan.org/libbluray' to get it.
 *
 * TODO:
 *  - Add descrambled keys database support (KEYDB.cfg)
 *
 */

#include <string.h>
#include <strings.h>
#include <assert.h>

#include <libbluray/bluray.h>
#include <libbluray/meta_data.h>
#include <libbluray/overlay.h>
#include <libbluray/keys.h>
#include <libbluray/bluray-version.h>
#include <libavutil/common.h>

#include "config.h"
#include "talloc.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "options/options.h"
#include "options/path.h"
#include "stream.h"
#include "osdep/timer.h"
#include "discnav.h"
#include "sub/osd.h"
#include "sub/img_convert.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"

#define BLURAY_SECTOR_SIZE     6144

#define BLURAY_DEFAULT_ANGLE      0
#define BLURAY_DEFAULT_CHAPTER    0
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

struct bluray_overlay {
    struct sub_bitmap *image;
    bool clean, hidden;
    int x, y, w, h;
};

struct bluray_priv_s {
    BLURAY *bd;
    BLURAY_TITLE_INFO *title_info;
    int num_titles;
    int current_angle;
    int current_title;
    int current_playlist;

    int cfg_title;
    char *cfg_device;

    // overlay stuffs
    struct bluray_overlay overlays[2], ol_flushed[2];
    struct mp_image_pool *pool;

    // navigation stuffs
    uint64_t next_event;
    uint32_t still_length;
    int mousex, mousey;
    bool in_menu, use_nav, nav_enabled, popup_enabled;
};

static const struct bluray_priv_s bluray_stream_priv_dflts = {
    .cfg_title = BLURAY_DEFAULT_TITLE,
};

static const struct bluray_priv_s bdnav_stream_priv_dflts = {
    .cfg_title = BLURAY_DEFAULT_TITLE,
    .use_nav = true,
};

#define OPT_BASE_STRUCT struct bluray_priv_s
static const m_option_t bluray_stream_opts_fields[] = {
    OPT_CHOICE_OR_INT("title", cfg_title, 0, 0, 99999,
                      ({"longest", BLURAY_DEFAULT_TITLE})),
    OPT_STRING("device", cfg_device, 0),
    {0}
};

static const m_option_t bdnav_stream_opts_fields[] = {
    OPT_CHOICE_OR_INT("title", cfg_title, 0, 0, 99999,
                      ({"menu", BLURAY_MENU_TITLE},
                       {"first", BLURAY_DEFAULT_TITLE})),
    OPT_STRING("device", cfg_device, 0),
    {0}
};

static void destruct(struct bluray_priv_s *priv)
{
    if (priv->title_info)
        bd_free_title_info(priv->title_info);
    bd_close(priv->bd);
    talloc_free(priv->pool);
}

inline static int play_title(struct bluray_priv_s *priv, int title)
{
    if (priv->use_nav) {
        if (title == priv->num_titles - 1)
            title = BLURAY_TITLE_FIRST_PLAY;
        return bd_play_title(priv->bd, title);
    } else
        return bd_select_title(priv->bd, title);
}

static void overlay_release(struct bluray_overlay *overlay)
{
    if (overlay->image)
        talloc_free(overlay->image);
    *overlay = (struct bluray_overlay) { .clean = true };
}

static void overlay_alloc(struct bluray_priv_s *priv,
                          struct bluray_overlay *overlay,
                          int x, int y, int w, int h)
{
    assert(overlay->image == NULL);
    struct sub_bitmap *image = talloc_zero(NULL, struct sub_bitmap);
    overlay->w = image->w = image->dw = w;
    overlay->h = image->h = image->dh = h;
    overlay->x = image->x = x;
    overlay->y = image->y = y;
    struct mp_image *mpi = mp_image_pool_get(priv->pool, IMGFMT_RGBA, w, h);
    mpi = talloc_steal(image, mpi);
    assert(image->w > 0 && image->h > 0 && mpi != NULL);
    image->stride = mpi->stride[0];
    image->bitmap = mpi->planes[0];
    overlay->image = image;
    overlay->clean = true;
    overlay->hidden = false;
}

static void overlay_close_all(struct bluray_priv_s *priv)
{
    for (int i = 0; i < 2; i++)
        overlay_release(&priv->overlays[i]);
}

static void overlay_close(struct bluray_priv_s *priv,
                          const BD_OVERLAY *const bo)
{
    overlay_release(&priv->overlays[bo->plane]);
}

static inline uint32_t conv_rgba(const BD_PG_PALETTE_ENTRY *p)
{
    uint32_t rgba;
    uint8_t *out = (uint8_t*)&rgba;
    const int y = p->Y,  cb = (int)p->Cb - 128,  cr = (int)p->Cr - 128;
    // CAUTION: inaccurate but fast, broken in big endian
#define CONV(a) (MPCLAMP((a), 0, 255)*p->T >> 8)
    out[0] = CONV(y + cb + (cb >> 1) + (cb >> 2) + (cb >> 6));
    out[1] = CONV(y - ((cb >> 2) + (cb >> 4) + (cb >> 5))
                    - ((cr >> 3) + (cr >> 4) + (cr >> 5)));
    out[2] = CONV(y + cr + (cr >> 2) + (cr >> 3) + (cr >> 5));
    out[3] = p->T;
#undef CONV
    return rgba;
}

static void overlay_process(void *data, const BD_OVERLAY *const bo)
{
    stream_t *s = data;
    struct bluray_priv_s *priv = s->priv;
    if (!bo) {
        overlay_close_all(priv);
        return;
    }
    struct bluray_overlay *overlay = &priv->overlays[bo->plane];
    switch (bo->cmd) {
    case BD_OVERLAY_INIT:
        overlay_alloc(priv, overlay, bo->x, bo->y, bo->w, bo->h);
        break;
    case BD_OVERLAY_CLOSE:
        overlay_close(priv, bo);
        break;
    case BD_OVERLAY_CLEAR:
        if (!overlay->clean) {
            memset(overlay->image->bitmap, 0,
                   overlay->image->stride*overlay->h);
            overlay->clean = true;
        }
        break;
    case BD_OVERLAY_DRAW: {
        if (!bo->img)
            break;
        overlay->hidden = false;
        overlay->clean = false;
        struct sub_bitmap *img = overlay->image;
        uint32_t *const origin = img->bitmap;
        const BD_PG_RLE_ELEM *in = bo->img;
        for (int y = 0; y < bo->h; y++) {
            uint32_t *out = origin + (img->stride/4) * (y + bo->y) + bo->x;
            for (int x = 0; x < bo->w; ) {
                uint32_t c = 0;
                if (bo->palette[in->color].T) {
                    c = conv_rgba(&bo->palette[in->color]);
                    for (int i = 0; i < in->len; i++)
                        *out++ = c;
                } else {
                    memset(out, 0, in->len*4);
                    out += in->len;
                }
                x += in->len;
                ++in;
            }
        }
        break;
    }
    case BD_OVERLAY_WIPE: {
        uint32_t *const origin = overlay->image->bitmap;
        for (int y = 0; y < bo->h; y++)
            memset(origin + overlay->w * (y + bo->y) + bo->x, 0, 4 * bo->w);
        break;
    }
    case BD_OVERLAY_HIDE:
        priv->overlays[bo->plane].hidden = true;
        break;
    case BD_OVERLAY_FLUSH: {
        struct bluray_overlay *in = overlay;
        struct bluray_overlay *out = &priv->ol_flushed[bo->plane];
        if (out->image && (out->image->stride != in->image->stride ||
                           out->image->h != in->image->h))
            overlay_release(out);
        if (!out->image)
            overlay_alloc(priv, out, in->x, in->y, in->w, in->h);
        const int len = in->image->stride*in->image->h;
        memcpy(out->image->bitmap, in->image->bitmap, len);
        out->clean = in->clean;
        out->hidden = in->hidden;
        priv->next_event |= 1 << MP_NAV_EVENT_OVERLAY;
        break;
    } default:
        break;
    }
}

static inline bool set_event_type(struct bluray_priv_s *priv, int type,
                                struct mp_nav_event *event)
{
    if (!(priv->next_event & (1 << type)))
        return false;
    priv->next_event &= ~(1 << type);
    event->event = type;
    return true;
}

static void fill_next_event(stream_t *s, struct mp_nav_event **ret)
{
    struct bluray_priv_s *priv = s->priv;
    struct mp_nav_event e = {0};
    // this should be checked before any other events
    if (!set_event_type(priv, MP_NAV_EVENT_RESET_ALL, &e))
        for (int n = 0; n < 30 && !set_event_type(priv, n, &e); n++) ;
    switch (e.event) {
    case MP_NAV_EVENT_NONE:
        return;
    case MP_NAV_EVENT_OVERLAY: {
        for (int i = 0; i < 2; i++) {
            struct bluray_overlay *o = &priv->ol_flushed[i];
            e.u.overlay.images[i] = NULL;
            if (!o->clean && !o->hidden) {
                e.u.overlay.images[i] = o->image;
                o->image = NULL;
            }
        }
        break;
    }
    case MP_NAV_EVENT_MENU_MODE:
        e.u.menu_mode.enable = priv->in_menu;
        break;
    case MP_NAV_EVENT_STILL_FRAME:
        e.u.still_frame.seconds = priv->still_length;
        break;
    }
    *ret = talloc(NULL, struct mp_nav_event);
    **ret = e;
}

static void bluray_stream_close(stream_t *s)
{
    destruct(s->priv);
}

static void handle_event(stream_t *s, const BD_EVENT *ev)
{
    static const int reset_flags = (1 << MP_NAV_EVENT_RESET_ALL)
                                 | (1 << MP_NAV_EVENT_RESET);
    struct bluray_priv_s *b = s->priv;
    switch (ev->event) {
    case BD_EVENT_MENU:
        b->in_menu = ev->param;
        b->next_event |= 1 << MP_NAV_EVENT_MENU_MODE;
        break;
    case BD_EVENT_STILL:
        b->still_length = ev->param ? -1 : 0;
        if (b->nav_enabled)
            b->next_event |= 1 << MP_NAV_EVENT_STILL_FRAME;
        break;
    case BD_EVENT_STILL_TIME:
        b->still_length = ev->param ? -1 : ev->param*1000;
        if (b->nav_enabled)
            b->next_event |= 1 << MP_NAV_EVENT_STILL_FRAME;
        else
            bd_read_skip_still(b->bd);
        break;
    case BD_EVENT_END_OF_TITLE:
        overlay_close_all(b);
        break;
    case BD_EVENT_PLAYLIST:
        b->next_event = reset_flags;
        b->current_playlist = ev->param;
        if (!b->use_nav)
            b->current_title = bd_get_current_title(b->bd);
        if (b->title_info)
            bd_free_title_info(b->title_info);
        b->title_info = bd_get_playlist_info(b->bd, b->current_playlist,
                                             b->current_angle);
        break;
    case BD_EVENT_TITLE:
        b->next_event = reset_flags;
        if (ev->param == BLURAY_TITLE_FIRST_PLAY) {
            if (b->use_nav)
                b->current_title = b->num_titles - 1;
            else
                b->current_title = bd_get_current_title(b->bd);
        } else
            b->current_title = ev->param;
        if (b->title_info) {
            bd_free_title_info(b->title_info);
            b->title_info = NULL;
        }
        break;
    case BD_EVENT_ANGLE:
        b->current_angle = ev->param;
        if (b->title_info) {
            bd_free_title_info(b->title_info);
            b->title_info = bd_get_playlist_info(b->bd, b->current_playlist,
                                                 b->current_angle);
        }
        break;
    case BD_EVENT_POPUP:
        b->popup_enabled = ev->param;
        break;
#if BLURAY_VERSION >= BLURAY_VERSION_CODE(0, 5, 0)
    case BD_EVENT_DISCONTINUITY:
        b->next_event = reset_flags;
        break;
#endif
    default:
        MP_TRACE(s, "Unhandled event: %d %d\n", ev->event, ev->param);
        break;
    }
}

static int bluray_stream_fill_buffer(stream_t *s, char *buf, int len)
{
    struct bluray_priv_s *b = s->priv;
    assert(!b->use_nav);
    BD_EVENT event;
    while (bd_get_event(b->bd, &event))
        handle_event(s, &event);
    return bd_read(b->bd, buf, len);
}

static int bdnav_stream_fill_buffer(stream_t *s, char *buf, int len)
{
    struct bluray_priv_s *b = s->priv;
    assert(b->use_nav);
    BD_EVENT event;
    int read = -1;
    for (;;) {
        read = bd_read_ext(b->bd, buf, len, &event);
        if (read < 0)
            return read;
        if (read == 0) {
            if (event.event == BD_EVENT_NONE)
                return 0; // end of stream
            handle_event(s, &event);
        } else
            break;
    }
    return read;
}

static bd_vk_key_e translate_nav_menu_action(const char *cmd)
{
    if (strcmp(cmd, "mouse") == 0)
        return BD_VK_MOUSE_ACTIVATE;
    if (strcmp(cmd, "up") == 0)
        return BD_VK_UP;
    if (strcmp(cmd, "down") == 0)
        return BD_VK_DOWN;
    if (strcmp(cmd, "left") == 0)
        return BD_VK_LEFT;
    if (strcmp(cmd, "right") == 0)
        return BD_VK_RIGHT;
    if (strcmp(cmd, "select") == 0)
        return BD_VK_ENTER;
    return BD_VK_NONE;
}

static void handle_nav_command(stream_t *s, struct mp_nav_cmd *ev)
{
    struct bluray_priv_s *priv = s->priv;
    switch (ev->event) {
    case MP_NAV_CMD_ENABLE:
        priv->nav_enabled = true;
        break;
    case MP_NAV_CMD_MENU: {
        const int64_t pts = mp_time_us();
        const char *action = ev->u.menu.action;
        bd_vk_key_e key = translate_nav_menu_action(action);
        if (key != BD_VK_NONE) {
            if (key == BD_VK_MOUSE_ACTIVATE)
                ev->mouse_on_button = bd_mouse_select(priv->bd, pts,
                                                      priv->mousex,
                                                      priv->mousey);
            bd_user_input(priv->bd, pts, key);
        } else if (strcmp(action, "menu") == 0) {
            if (priv->popup_enabled)
                bd_user_input(priv->bd, pts, BD_VK_POPUP);
            else
                bd_menu_call(priv->bd, pts);
        }
        break;
    } case MP_NAV_CMD_MOUSE_POS:
        priv->mousex = ev->u.mouse_pos.x;
        priv->mousey = ev->u.mouse_pos.y;
        ev->mouse_on_button = bd_mouse_select(priv->bd, mp_time_us(),
                                              priv->mousex,
                                              priv->mousey);
        break;
    case MP_NAV_CMD_SKIP_STILL:
        bd_read_skip_still(priv->bd);
        break;
    }
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
        if (chapter >= 0 || chapter < ti->chapter_count)
            time = BD_TIME_TO_MP(ti->chapters[chapter].start);
        if (time == MP_NOPTS_VALUE)
            return STREAM_ERROR;
        *(double *)arg = time;
        return STREAM_OK;
    }
    case STREAM_CTRL_SET_CURRENT_TITLE: {
        const uint32_t title = *((unsigned int*)arg);
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
        *((int *) arg) = b->current_angle;
        return STREAM_OK;
    }
    case STREAM_CTRL_SET_ANGLE: {
        const BLURAY_TITLE_INFO *ti = b->title_info;
        if (!ti)
            return STREAM_UNSUPPORTED;
        int angle = *((int *) arg);
        if (angle < 0 || angle > ti->angle_count)
            return STREAM_UNSUPPORTED;
        b->current_angle = angle;
        bd_seamless_angle_change(b->bd, angle);
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
    case STREAM_CTRL_NAV_CMD:
        if (!b->use_nav)
            return STREAM_UNSUPPORTED;
        handle_nav_command(s, arg);
        return STREAM_OK;
    case STREAM_CTRL_GET_NAV_EVENT: {
        struct mp_nav_event **ev = arg;
        if (ev)
            fill_next_event(s, ev);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_SIZE:
        *(int64_t *)arg = bd_get_title_size(b->bd);
        return STREAM_OK;
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

    int title = -1;
    if (b->use_nav) {
        if (b->cfg_title == BLURAY_MENU_TITLE)
            title = 0; // BLURAY_TITLE_TOP_MENU
        else if (b->cfg_title == BLURAY_DEFAULT_TITLE)
            title = b->num_titles - 1;
        else
            title = b->cfg_title;
    } else {
        if (b->cfg_title != BLURAY_DEFAULT_TITLE )
            title = b->cfg_title;
        else
            title = title_guess;
    }
    if (title < 0)
        return;

    if (play_title(b, title))
        b->current_title = title;
    else {
        MP_WARN(s, "Couldn't start title '%d'.\n", title);
        if (!b->use_nav) // cannot query title info in nav
            b->current_title = bd_get_current_title(b->bd);
    }
}

static void select_initial_angle(stream_t *s) {
    struct bluray_priv_s *b = s->priv;
    if (!b->use_nav) // no way to figure out current title info
        return;
    BLURAY_TITLE_INFO *info = bd_get_title_info(b->bd, b->current_title, 0);
    if (!info)
        return;
    /* Select angle */
    unsigned int angle = s->opts->bluray_angle;
    if (!angle)
        angle = BLURAY_DEFAULT_ANGLE;
    angle = FFMIN(angle, info->angle_count);
    if (angle)
        bd_select_angle(b->bd, angle);
    b->current_angle = bd_get_current_angle(b->bd);
    bd_free_title_info(info);
}

static int bluray_stream_open(stream_t *s)
{
    struct bluray_priv_s *b = s->priv;

    const char *device = NULL;
    /* find the requested device */
    if (b->cfg_device && b->cfg_device[0])
        device = b->cfg_device;
    else if (s->opts->bluray_device && s->opts->bluray_device[0])
        device = s->opts->bluray_device;

    if (!device) {
        MP_ERR(s, "No Blu-ray device/location was specified ...\n");
        return STREAM_UNSUPPORTED;
    }

    /* open device */
    BLURAY *bd = bd_open(device, NULL);
    if (!bd) {
        MP_ERR(s, "Couldn't open Blu-ray device: %s\n", device);
        return STREAM_UNSUPPORTED;
    }
    b->bd = bd;

    if (!check_disc_info(s)) {
        destruct(b);
        return STREAM_UNSUPPORTED;
    }

    int title_guess = BLURAY_DEFAULT_TITLE;
    if (b->use_nav) {
        const BLURAY_DISC_INFO *disc_info = bd_get_disc_info(b->bd);
        b->num_titles = disc_info->num_hdmv_titles + disc_info->num_bdj_titles;
        ++b->num_titles; // for BLURAY_TITLE_TOP_MENU
        ++b->num_titles; // for BLURAY_TITLE_FIRST_PLAY
    } else {
        /* check for available titles on disc */
        b->num_titles = bd_get_titles(bd, TITLES_RELEVANT, 0);
        if (!b->num_titles) {
            MP_ERR(s, "Can't find any Blu-ray-compatible title here.\n");
            destruct(b);
            return STREAM_UNSUPPORTED;
        }

        /* parse titles information */
        uint64_t max_duration = 0;
        for (int i = 0; i < b->num_titles; i++) {
            BLURAY_TITLE_INFO *ti = bd_get_title_info(bd, i, 0);
            if (!ti)
                continue;

            /* try to guess which title may contain the main movie */
            if (ti->duration > max_duration) {
                max_duration = ti->duration;
                title_guess = i;
            }

            bd_free_title_info(ti);
        }
    }

    // these should be set before any callback
    b->pool = mp_image_pool_new(6);
    b->current_angle = -1;
    b->current_title = -1;

    // initialize libbluray event queue
    bd_get_event(bd, NULL);

    if (b->use_nav) {
        if (!bd_play(bd)) {
            destruct(b);
            return STREAM_ERROR;
        }
        bd_register_overlay_proc(bd, s, overlay_process);
    }

    select_initial_title(s, title_guess);
    select_initial_angle(s);

    if (b->use_nav)
        s->fill_buffer = bdnav_stream_fill_buffer;
    else
        s->fill_buffer = bluray_stream_fill_buffer;
    s->close       = bluray_stream_close;
    s->control     = bluray_stream_control;
    s->type        = STREAMTYPE_BLURAY;
    s->sector_size = BLURAY_SECTOR_SIZE;
    s->priv        = b;
    s->demuxer     = "+disc";

    MP_VERBOSE(s, "Blu-ray successfully opened.\n");

    return STREAM_OK;
}

const stream_info_t stream_info_bluray = {
    .name = "bd",
    .open = bluray_stream_open,
    .protocols = (const char*const[]){ "bd", "br", "bluray", NULL },
    .priv_defaults = &bluray_stream_priv_dflts,
    .priv_size = sizeof(struct bluray_priv_s),
    .options = bluray_stream_opts_fields,
    .url_options = (const char*const[]){
        "hostname=title",
        "filename=device",
        NULL
    },
};

const stream_info_t stream_info_bdnav = {
    .name = "bdnav",
    .open = bluray_stream_open,
    .protocols = (const char*const[]){ "bdnav", "brnav", "bluraynav", NULL },
    .priv_defaults = &bdnav_stream_priv_dflts,
    .priv_size = sizeof(struct bluray_priv_s),
    .options = bdnav_stream_opts_fields,
    .url_options = (const char*const[]){
        "hostname=title",
        "filename=device",
        NULL
    },
};

static bool check_bdmv(const char *path)
{
    if (strcasecmp(mp_basename(path), "MovieObject.bdmv"))
        return false;

    FILE *temp = fopen(path, "rb");
    if (!temp)
        return false;

    bool r = false;

    const char *sig1 = "MOBJ020";
    const char *sig2 = "MOBJ0100";
    char data[50];

    if (fread(data, 50, 1, temp) == 1) {
        r = memcmp(data, sig1, strlen(sig1)) == 0 ||
            memcmp(data, sig2, strlen(sig2)) == 0;
    }

    fclose(temp);
    return r;
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
    *priv = bluray_stream_priv_dflts;

    char *path = mp_file_get_path(priv, bstr0(stream->url));
    if (!path)
        goto unsupported;

    // We allow the path to point to a directory containing BDMV/, a
    // directory containing MovieObject.bdmv, or that file itself.
    if (!check_bdmv(path)) {
        // On UNIX, just assume the filename has always this case.
        char *npath = mp_path_join(priv, bstr0(path), bstr0("MovieObject.bdmv"));
        if (!check_bdmv(npath)) {
            npath = mp_path_join(priv, bstr0(path), bstr0("BDMV/MovieObject.bdmv"));
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
    return bluray_stream_open(stream);

unsupported:
    talloc_free(priv);
    stream->priv = NULL;
    return STREAM_UNSUPPORTED;
}

const stream_info_t stream_info_bdmv_dir = {
    .name = "bdmv/bluray",
    .open = bdmv_dir_stream_open,
    .protocols = (const char*const[]){ "file", "", NULL },
};
