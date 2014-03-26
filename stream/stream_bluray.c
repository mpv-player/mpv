/*
 * Copyright (C) 2010 Benjamin Zores <ben@geexbox.org>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Blu-ray parser/reader using libbluray
 *  Use 'git clone git://git.videolan.org/libbluray' to get it.
 *
 * TODO:
 *  - Add libbdnav support for menus navigation
 *  - Add AACS/BD+ protection detection
 *  - Add descrambled keys database support (KEYDB.cfg)
 *
 */

#include <string.h>
#include <assert.h>

#include <libbluray/bluray.h>
#include <libbluray/meta_data.h>
#include <libavutil/common.h>

#include "config.h"
#include "talloc.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/m_option.h"
#include "stream.h"
#include "osdep/timer.h"

#define BLURAY_SECTOR_SIZE     6144

#define BLURAY_DEFAULT_ANGLE      0
#define BLURAY_DEFAULT_CHAPTER    0
#define BLURAY_DEFAULT_TITLE     -1

// 90khz ticks
#define BD_TIMEBASE (90000)
#define BD_TIME_TO_MP(x) ((x) / (double)(BD_TIMEBASE))
#define BD_TIME_FROM_MP(x) ((uint64_t)(x * BD_TIMEBASE))

char *bluray_device  = NULL;
int   bluray_angle   = 0;

struct bluray_priv_s {
    BLURAY *bd;
    BLURAY_TITLE_INFO *title_info;
    int num_titles;
    int current_angle;
    int current_title;

    int cfg_title;
    char *cfg_device;
};

static struct bluray_priv_s bluray_stream_priv_dflts = {
    .cfg_title = BLURAY_DEFAULT_TITLE
};

#define OPT_BASE_STRUCT struct bluray_priv_s
static const m_option_t bluray_stream_opts_fields[] = {
    OPT_CHOICE_OR_INT("title", cfg_title, 0, 0, 99999,
                      ({"longest", BLURAY_DEFAULT_TITLE})),
    OPT_STRING("device", cfg_device, 0),
    {0}
};

static void bluray_stream_close(stream_t *s)
{
    struct bluray_priv_s *b = s->priv;

    if (b->title_info)
        bd_free_title_info(b->title_info);
    bd_close(b->bd);
}

static int bluray_stream_seek(stream_t *s, int64_t pos)
{
    struct bluray_priv_s *b = s->priv;
    int64_t p;

    p = bd_seek(b->bd, pos);
    if (p == -1)
        return 0;

    return 1;
}

static void handle_event(stream_t *s, const BD_EVENT *ev)
{
    struct bluray_priv_s *b = s->priv;
    switch (ev->event) {
    case BD_EVENT_TITLE:
        if (ev->param == BLURAY_TITLE_FIRST_PLAY)
            b->current_title = bd_get_current_title(b->bd);
        else
            b->current_title = ev->param;
        if (b->title_info) {
            bd_free_title_info(b->title_info);
            b->title_info = NULL;
        }
        b->title_info = bd_get_title_info(b->bd, b->current_title,
                                          b->current_angle);
        break;
    case BD_EVENT_ANGLE:
        b->current_angle = ev->param;
        if (b->title_info) {
            bd_free_title_info(b->title_info);
            b->title_info = bd_get_title_info(b->bd, b->current_title,
                                              b->current_angle);
        }
        break;
    case BD_EVENT_IDLE:
        mp_sleep_us(5000);
        break;
    default:
        MP_TRACE(s, "Unhandled event: %d %d\n", ev->event, ev->param);
        break;
    }
}

static int bluray_stream_fill_buffer(stream_t *s, char *buf, int len)
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

        return 1;
    }

    case STREAM_CTRL_GET_CHAPTER_TIME: {
        const BLURAY_TITLE_INFO *ti = b->title_info;
        int chapter = *(double *)arg;
        double time = MP_NOPTS_VALUE;

        if (!ti)
            return STREAM_UNSUPPORTED;

        if (chapter >= 0 || chapter < ti->chapter_count) {
            time = BD_TIME_TO_MP(ti->chapters[chapter].start);
        }

        if (time != MP_NOPTS_VALUE) {
            *(double *)arg = time;
            return STREAM_OK;
        }
        return STREAM_ERROR;
    }
    case STREAM_CTRL_SET_CURRENT_TITLE: {
        const uint32_t title = *((unsigned int*)arg);
        if (title < b->num_titles && bd_select_title(b->bd, title)) {
            b->current_title = title;
            return STREAM_OK;
        }
        return STREAM_ERROR;
    }
    case STREAM_CTRL_GET_CURRENT_TITLE: {
        *((unsigned int *) arg) = b->current_title;
        return 1;
    }
    case STREAM_CTRL_GET_NUM_TITLES: {
        *((unsigned int *)arg) = b->num_titles;
        return 1;
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
        // Reset mpv internal stream position.
        stream_seek(s, bd_tell(b->bd));
        // API makes it hard to determine seeking success
        return STREAM_OK;
    }

    case STREAM_CTRL_GET_NUM_ANGLES: {
        const BLURAY_TITLE_INFO *ti = b->title_info;

        if (!ti)
            return STREAM_UNSUPPORTED;

        *((int *) arg) = ti->angle_count;

        return 1;
    }

    case STREAM_CTRL_GET_ANGLE: {
        *((int *) arg) = b->current_angle;
        return 1;
    }

    case STREAM_CTRL_SET_ANGLE: {
        const BLURAY_TITLE_INFO *ti = b->title_info;
        int angle = *((int *) arg);

        if (!ti)
            return STREAM_UNSUPPORTED;

        if (angle < 0 || angle > ti->angle_count) {
            return STREAM_UNSUPPORTED;
        }

        b->current_angle = angle;
        bd_seamless_angle_change(b->bd, angle);

        return 1;
    }

    case STREAM_CTRL_GET_LANG: {
        struct stream_lang_req *req = arg;
        const BLURAY_TITLE_INFO *ti = b->title_info;
        if (ti && ti->clip_count) {
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
    case STREAM_CTRL_GET_START_TIME:
    {
        *((double *)arg) = 0;
        return STREAM_OK;
    }
    case STREAM_CTRL_MANAGES_TIMELINE:
        return STREAM_OK;
    case STREAM_CTRL_GET_DISC_NAME:
    {
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

static int bluray_stream_open(stream_t *s, int mode)
{
    struct bluray_priv_s *b = s->priv;

    BLURAY_TITLE_INFO *info = NULL;
    BLURAY *bd;

    int title, title_guess;
    uint64_t title_size;

    unsigned int angle = 0;
    uint64_t max_duration = 0;

    char *device = NULL;
    int i;

    /* find the requested device */
    if (b->cfg_device)
        device = b->cfg_device;
    else if (bluray_device)
        device = bluray_device;

    if (!device) {
        MP_ERR(s, "No Blu-ray device/location was specified ...\n");
        return STREAM_UNSUPPORTED;
    }

    /* open device */
    bd = bd_open(device, NULL);
    if (!bd) {
        MP_ERR(s, "Couldn't open Blu-ray device: %s\n",
               device);
        return STREAM_UNSUPPORTED;
    }

    /* check for available titles on disc */
    b->num_titles = bd_get_titles(bd, TITLES_RELEVANT, angle);
    MP_SMODE(s, "ID_BLURAY_TITLES=%d\n", b->num_titles);
    if (!b->num_titles) {
        MP_ERR(s, "Can't find any Blu-ray-compatible title here.\n");
        bd_close(bd);
        return STREAM_UNSUPPORTED;
    }

    /* parse titles information */
    title_guess = BLURAY_DEFAULT_TITLE;
    for (i = 0; i < b->num_titles; i++) {
        BLURAY_TITLE_INFO *ti;
        int sec, msec;

        ti = bd_get_title_info(bd, i, angle);
        if (!ti)
            continue;

        sec  = ti->duration / 90000;
        msec = (ti->duration - sec) % 1000;

        MP_SMODE(s, "ID_BLURAY_TITLE_%d_CHAPTERS=%d\n", i, ti->chapter_count);
        MP_SMODE(s, "ID_BLURAY_TITLE_%d_ANGLE=%d\n", i, ti->angle_count);
        MP_SMODE(s, "ID_BLURAY_TITLE_%d_LENGTH=%d.%03d\n", i, sec, msec);

        /* try to guess which title may contain the main movie */
        if (ti->duration > max_duration) {
            max_duration = ti->duration;
            title_guess = i;
        }

        bd_free_title_info(ti);
    }

    // initialize libbluray event queue
    bd_get_event(bd, NULL);

    /* Select current title */
    if (b->cfg_title != BLURAY_DEFAULT_TITLE)
        title = b->cfg_title;
    else
        title = title_guess;
    title = FFMIN(title, b->num_titles - 1);

    bd_select_title(bd, title);

    title_size = bd_get_title_size(bd);
    MP_SMODE(s, "ID_BLURAY_CURRENT_TITLE=%d\n", title);

    /* Get current title information */
    info = bd_get_title_info(bd, title, angle);
    if (!info)
        goto err_no_info;

    /* Select angle */
    angle = bluray_angle ? bluray_angle : BLURAY_DEFAULT_ANGLE;
    angle = FFMIN(angle, info->angle_count);

    if (angle)
        bd_select_angle(bd, angle);

    MP_SMODE(s, "ID_BLURAY_CURRENT_ANGLE=%d\n", angle + 1);

    bd_free_title_info(info);

err_no_info:
    s->fill_buffer = bluray_stream_fill_buffer;
    s->seek        = bluray_stream_seek;
    s->close       = bluray_stream_close;
    s->control     = bluray_stream_control;

    b->bd              = bd;
    b->current_angle   = angle;
    b->current_title   = title;

    s->type        = STREAMTYPE_BLURAY;
    s->end_pos     = title_size;
    s->sector_size = BLURAY_SECTOR_SIZE;
    s->flags       = MP_STREAM_SEEK;
    s->priv        = b;

    MP_VERBOSE(s, "Blu-ray successfully opened.\n");

    return STREAM_OK;
}

const stream_info_t stream_info_bluray = {
    .name = "bd",
    .open = bluray_stream_open,
    .protocols = (const char*[]){ "bd", "br", "bluray", NULL },
    .priv_defaults = &bluray_stream_priv_dflts,
    .priv_size = sizeof(struct bluray_priv_s),
    .options = bluray_stream_opts_fields,
    .url_options = (const char*[]){
        "hostname=title",
        "filename=device",
        NULL
    },
};
