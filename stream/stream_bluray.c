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
#include <strings.h>
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
#include "options/m_config.h"
#include "options/path.h"
#include "stream.h"
#include "osdep/timer.h"
#include "sub/osd.h"
#include "sub/img_convert.h"
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

struct bluray_priv_s {
    BLURAY *bd;
    BLURAY_TITLE_INFO *title_info;
    int num_titles;
    int current_angle;
    int current_title;
    int current_playlist;

    int cfg_title;
    int cfg_playlist;
    char *cfg_device;

    bool use_nav;
};

static void destruct(struct bluray_priv_s *priv)
{
    if (priv->title_info)
        bd_free_title_info(priv->title_info);
    bd_close(priv->bd);
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
    destruct(s->priv);
}

static void handle_event(stream_t *s, const BD_EVENT *ev)
{
    struct bluray_priv_s *b = s->priv;
    switch (ev->event) {
    case BD_EVENT_MENU:
        break;
    case BD_EVENT_STILL:
        break;
    case BD_EVENT_STILL_TIME:
        bd_read_skip_still(b->bd);
        break;
    case BD_EVENT_END_OF_TITLE:
        break;
    case BD_EVENT_PLAYLIST:
        b->current_playlist = ev->param;
        b->current_title = bd_get_current_title(b->bd);
        if (b->title_info)
            bd_free_title_info(b->title_info);
        b->title_info = bd_get_playlist_info(b->bd, b->current_playlist,
                                             b->current_angle);
        break;
    case BD_EVENT_TITLE:
        if (ev->param == BLURAY_TITLE_FIRST_PLAY) {
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

    char *device = NULL;
    /* find the requested device */
    if (b->cfg_device && b->cfg_device[0]) {
        device = b->cfg_device;
    } else {
        mp_read_option_raw(s->global, "bluray-device", &m_option_type_string,
                           &device);
    }

    if (!device || !device[0]) {
        MP_ERR(s, "No Blu-ray device/location was specified ...\n");
        return STREAM_UNSUPPORTED;
    }

    if (!mp_msg_test(s->log, MSGL_DEBUG))
        bd_set_debug_mask(0);

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
        MP_FATAL(s, "BluRay menu support has been removed.\n");
        return STREAM_ERROR;
    } else {
        /* check for available titles on disc */
        b->num_titles = bd_get_titles(bd, TITLES_RELEVANT, 0);
        if (!b->num_titles) {
            MP_ERR(s, "Can't find any Blu-ray-compatible title here.\n");
            destruct(b);
            return STREAM_UNSUPPORTED;
        }

        MP_INFO(s, "List of available titles:\n");

        /* parse titles information */
        uint64_t max_duration = 0;
        for (int i = 0; i < b->num_titles; i++) {
            BLURAY_TITLE_INFO *ti = bd_get_title_info(bd, i, 0);
            if (!ti)
                continue;

            char *time = mp_format_time(ti->duration / 90000, false);
            MP_INFO(s, "idx: %3d duration: %s (playlist: %05d.mpls)\n",
                       i, time, ti->playlist);
            talloc_free(time);

            /* try to guess which title may contain the main movie */
            if (ti->duration > max_duration) {
                max_duration = ti->duration;
                title_guess = i;
            }

            bd_free_title_info(ti);
        }
    }

    // these should be set before any callback
    b->current_angle = -1;
    b->current_title = -1;

    // initialize libbluray event queue
    bd_get_event(bd, NULL);

    select_initial_title(s, title_guess);

    s->fill_buffer = bluray_stream_fill_buffer;
    s->close       = bluray_stream_close;
    s->control     = bluray_stream_control;
    s->priv        = b;
    s->demuxer     = "+disc";

    MP_VERBOSE(s, "Blu-ray successfully opened.\n");

    return STREAM_OK;
}

const stream_info_t stream_info_bdnav;

static int bluray_stream_open(stream_t *s)
{
    struct bluray_priv_s *b = talloc_zero(s, struct bluray_priv_s);
    s->priv = b;

    b->use_nav = s->info == &stream_info_bdnav;

    bstr title, bdevice, rest = { .len = 0 };
    bstr_split_tok(bstr0(s->path), "/", &title, &bdevice);

    b->cfg_title = BLURAY_DEFAULT_TITLE;

    if (bstr_equals0(title, "longest") || bstr_equals0(title, "first")) {
        b->cfg_title = BLURAY_DEFAULT_TITLE;
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

const stream_info_t stream_info_bdnav = {
    .name = "bdnav",
    .open = bluray_stream_open,
    .protocols = (const char*const[]){ "bdnav", "brnav", "bluraynav", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};

static bool check_bdmv(const char *path)
{
    if (strcasecmp(mp_basename(path), "MovieObject.bdmv"))
        return false;

    FILE *temp = fopen(path, "rb");
    if (!temp)
        return false;

    char data[50] = {0};

    fread(data, 50, 1, temp);
    fclose(temp);

    bstr bdata = {data, 50};

    return bstr_startswith0(bdata, "MOBJ0100") || // AVCHD
           bstr_startswith0(bdata, "MOBJ0200") || // Blu-ray
           bstr_startswith0(bdata, "MOBJ0300");   // UHD BD
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
    *priv = (struct bluray_priv_s){
        .cfg_title = BLURAY_DEFAULT_TITLE,
    };

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
