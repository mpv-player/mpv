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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>

#include "config.h"
#include "demux/demux.h"
#include "sd.h"
#include "dec_sub.h"
#include "options/m_config.h"
#include "options/options.h"
#include "common/global.h"
#include "common/msg.h"
#include "common/recorder.h"
#include "misc/dispatch.h"
#include "osdep/threads.h"

extern const struct sd_functions sd_ass;
extern const struct sd_functions sd_lavc;

static const struct sd_functions *const sd_list[] = {
    &sd_lavc,
    &sd_ass,
    NULL
};

struct dec_sub {
    pthread_mutex_t lock;

    struct mp_log *log;
    struct mpv_global *global;
    struct mp_subtitle_opts *opts;
    struct m_config_cache *opts_cache;

    struct mp_recorder_sink *recorder_sink;

    struct attachment_list *attachments;

    struct sh_stream *sh;
    int play_dir;
    int order;
    double last_pkt_pts;
    bool preload_attempted;
    double video_fps;
    double sub_speed;

    struct mp_codec_params *codec;
    double start, end;

    double last_vo_pts;
    struct sd *sd;

    struct demux_packet *new_segment;
};

static void update_subtitle_speed(struct dec_sub *sub)
{
    struct mp_subtitle_opts *opts = sub->opts;
    sub->sub_speed = 1.0;

    if (sub->video_fps > 0 && sub->codec->frame_based > 0) {
        MP_VERBOSE(sub, "Frame based format, dummy FPS: %f, video FPS: %f\n",
                   sub->codec->frame_based, sub->video_fps);
        sub->sub_speed *= sub->codec->frame_based / sub->video_fps;
    }

    if (opts->sub_fps && sub->video_fps)
        sub->sub_speed *= opts->sub_fps / sub->video_fps;

    sub->sub_speed *= opts->sub_speed;
}

// Return the subtitle PTS used for a given video PTS.
static double pts_to_subtitle(struct dec_sub *sub, double pts)
{
    struct mp_subtitle_opts *opts = sub->opts;

    if (pts != MP_NOPTS_VALUE)
        pts = (pts * sub->play_dir - opts->sub_delay) / sub->sub_speed;

    return pts;
}

static double pts_from_subtitle(struct dec_sub *sub, double pts)
{
    struct mp_subtitle_opts *opts = sub->opts;

    if (pts != MP_NOPTS_VALUE)
        pts = (pts * sub->sub_speed + opts->sub_delay) * sub->play_dir;

    return pts;
}

static void wakeup_demux(void *ctx)
{
    struct mp_dispatch_queue *q = ctx;
    mp_dispatch_interrupt(q);
}

void sub_destroy(struct dec_sub *sub)
{
    if (!sub)
        return;
    demux_set_stream_wakeup_cb(sub->sh, NULL, NULL);
    if (sub->sd) {
        sub_reset(sub);
        sub->sd->driver->uninit(sub->sd);
    }
    talloc_free(sub->sd);
    pthread_mutex_destroy(&sub->lock);
    talloc_free(sub);
}

static struct sd *init_decoder(struct dec_sub *sub)
{
    for (int n = 0; sd_list[n]; n++) {
        const struct sd_functions *driver = sd_list[n];
        struct sd *sd = talloc(NULL, struct sd);
        *sd = (struct sd){
            .global = sub->global,
            .log = mp_log_new(sd, sub->log, driver->name),
            .opts = sub->opts,
            .driver = driver,
            .attachments = sub->attachments,
            .codec = sub->codec,
            .preload_ok = true,
        };

        if (sd->driver->init(sd) >= 0)
            return sd;

        talloc_free(sd);
    }

    MP_ERR(sub, "Could not find subtitle decoder for format '%s'.\n",
           sub->codec->codec);
    return NULL;
}

// Thread-safety of the returned object: all functions are thread-safe,
// except sub_get_bitmaps() and sub_get_text(). Decoder backends (sd_*)
// do not need to acquire locks.
// Ownership of attachments goes to the callee, and is released with
// talloc_free() (even on failure).
struct dec_sub *sub_create(struct mpv_global *global, struct sh_stream *sh,
                           struct attachment_list *attachments, int order)
{
    assert(sh && sh->type == STREAM_SUB);

    struct dec_sub *sub = talloc(NULL, struct dec_sub);
    *sub = (struct dec_sub){
        .log = mp_log_new(sub, global->log, "sub"),
        .global = global,
        .opts_cache = m_config_cache_alloc(sub, global, &mp_subtitle_sub_opts),
        .sh = sh,
        .codec = sh->codec,
        .attachments = talloc_steal(sub, attachments),
        .play_dir = 1,
        .order = order,
        .last_pkt_pts = MP_NOPTS_VALUE,
        .last_vo_pts = MP_NOPTS_VALUE,
        .start = MP_NOPTS_VALUE,
        .end = MP_NOPTS_VALUE,
    };
    sub->opts = sub->opts_cache->opts;
    mpthread_mutex_init_recursive(&sub->lock);

    sub->sd = init_decoder(sub);
    if (sub->sd) {
        update_subtitle_speed(sub);
        return sub;
    }

    sub_destroy(sub);
    return NULL;
}

// Called locked.
static void update_segment(struct dec_sub *sub)
{
    if (sub->new_segment && sub->last_vo_pts != MP_NOPTS_VALUE &&
        sub->last_vo_pts >= sub->new_segment->start)
    {
        MP_VERBOSE(sub, "Switch segment: %f at %f\n", sub->new_segment->start,
                   sub->last_vo_pts);

        sub->codec = sub->new_segment->codec;
        sub->start = sub->new_segment->start;
        sub->end = sub->new_segment->end;
        struct sd *new = init_decoder(sub);
        if (new) {
            sub->sd->driver->uninit(sub->sd);
            talloc_free(sub->sd);
            sub->sd = new;
            update_subtitle_speed(sub);
            sub_control(sub, SD_CTRL_SET_TOP, &sub->order);
        } else {
            // We'll just keep the current decoder, and feed it possibly
            // invalid data (not our fault if it crashes or something).
            MP_ERR(sub, "Can't change to new codec.\n");
        }
        sub->sd->driver->decode(sub->sd, sub->new_segment);
        talloc_free(sub->new_segment);
        sub->new_segment = NULL;
    }
}

bool sub_can_preload(struct dec_sub *sub)
{
    bool r;
    pthread_mutex_lock(&sub->lock);
    r = sub->sd->driver->accept_packets_in_advance && !sub->preload_attempted;
    pthread_mutex_unlock(&sub->lock);
    return r;
}

void sub_preload(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);

    struct mp_dispatch_queue *demux_waiter = mp_dispatch_create(NULL);
    demux_set_stream_wakeup_cb(sub->sh, wakeup_demux, demux_waiter);

    sub->preload_attempted = true;

    for (;;) {
        struct demux_packet *pkt = NULL;
        int r = demux_read_packet_async(sub->sh, &pkt);
        if (r == 0) {
            mp_dispatch_queue_process(demux_waiter, INFINITY);
            continue;
        }
        if (!pkt)
            break;
        sub->sd->driver->decode(sub->sd, pkt);
        talloc_free(pkt);
    }

    demux_set_stream_wakeup_cb(sub->sh, NULL, NULL);
    talloc_free(demux_waiter);

    pthread_mutex_unlock(&sub->lock);
}

static bool is_new_segment(struct dec_sub *sub, struct demux_packet *p)
{
    return p->segmented &&
        (p->start != sub->start || p->end != sub->end || p->codec != sub->codec);
}

// Read packets from the demuxer stream passed to sub_create(). Return true if
// enough packets were read, false if the player should wait until the demuxer
// signals new packets available (and then should retry).
bool sub_read_packets(struct dec_sub *sub, double video_pts)
{
    bool r = true;
    pthread_mutex_lock(&sub->lock);
    video_pts = pts_to_subtitle(sub, video_pts);
    while (1) {
        bool read_more = true;
        if (sub->sd->driver->accepts_packet)
            read_more = sub->sd->driver->accepts_packet(sub->sd, video_pts);

        if (!read_more)
            break;

        if (sub->new_segment && sub->new_segment->start < video_pts) {
            sub->last_vo_pts = video_pts;
            update_segment(sub);
        }

        if (sub->new_segment)
            break;

        // (Use this mechanism only if sub_delay matters to avoid corner cases.)
        double min_pts = sub->opts->sub_delay < 0 ? video_pts : MP_NOPTS_VALUE;

        struct demux_packet *pkt;
        int st = demux_read_packet_async_until(sub->sh, min_pts, &pkt);
        // Note: "wait" (st==0) happens with non-interleaved streams only, and
        // then we should stop the playloop until a new enough packet has been
        // seen (or the subtitle decoder's queue is full). This usually does not
        // happen for interleaved subtitle streams, which never return "wait"
        // when reading, unless min_pts is set.
        if (st <= 0) {
            r = st < 0 || (sub->last_pkt_pts != MP_NOPTS_VALUE &&
                           sub->last_pkt_pts > video_pts);
            break;
        }

        if (sub->recorder_sink)
            mp_recorder_feed_packet(sub->recorder_sink, pkt);

        sub->last_pkt_pts = pkt->pts;

        if (is_new_segment(sub, pkt)) {
            sub->new_segment = pkt;
            // Note that this can be delayed to a much later point in time.
            update_segment(sub);
            break;
        }

        if (!(sub->preload_attempted && sub->sd->preload_ok))
            sub->sd->driver->decode(sub->sd, pkt);

        talloc_free(pkt);
    }
    pthread_mutex_unlock(&sub->lock);
    return r;
}

// Unref sub_bitmaps.rc to free the result. May return NULL.
struct sub_bitmaps *sub_get_bitmaps(struct dec_sub *sub, struct mp_osd_res dim,
                                    int format, double pts)
{
    pthread_mutex_lock(&sub->lock);

    struct mp_subtitle_opts *opts = sub->opts;

    pts = pts_to_subtitle(sub, pts);

    sub->last_vo_pts = pts;
    update_segment(sub);

    struct sub_bitmaps *res = NULL;

    if (!(sub->end != MP_NOPTS_VALUE && pts >= sub->end) &&
        opts->sub_visibility && sub->sd->driver->get_bitmaps)
        res = sub->sd->driver->get_bitmaps(sub->sd, dim, format, pts);

    pthread_mutex_unlock(&sub->lock);
    return res;
}

// The returned string is talloc'ed.
char *sub_get_text(struct dec_sub *sub, double pts, enum sd_text_type type)
{
    pthread_mutex_lock(&sub->lock);
    char *text = NULL;

    pts = pts_to_subtitle(sub, pts);

    sub->last_vo_pts = pts;
    update_segment(sub);

    if (sub->sd->driver->get_text)
        text = sub->sd->driver->get_text(sub->sd, pts, type);
    pthread_mutex_unlock(&sub->lock);
    return text;
}

struct sd_times sub_get_times(struct dec_sub *sub, double pts)
{
    pthread_mutex_lock(&sub->lock);
    struct sd_times res = { .start = MP_NOPTS_VALUE, .end = MP_NOPTS_VALUE };

    pts = pts_to_subtitle(sub, pts);

    sub->last_vo_pts = pts;
    update_segment(sub);

    if (sub->sd->driver->get_times)
        res = sub->sd->driver->get_times(sub->sd, pts);

    pthread_mutex_unlock(&sub->lock);
    return res;
}

void sub_reset(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);
    if (sub->sd->driver->reset)
        sub->sd->driver->reset(sub->sd);
    sub->last_pkt_pts = MP_NOPTS_VALUE;
    sub->last_vo_pts = MP_NOPTS_VALUE;
    talloc_free(sub->new_segment);
    sub->new_segment = NULL;
    pthread_mutex_unlock(&sub->lock);
}

void sub_select(struct dec_sub *sub, bool selected)
{
    pthread_mutex_lock(&sub->lock);
    if (sub->sd->driver->select)
        sub->sd->driver->select(sub->sd, selected);
    pthread_mutex_unlock(&sub->lock);
}

int sub_control(struct dec_sub *sub, enum sd_ctrl cmd, void *arg)
{
    int r = CONTROL_UNKNOWN;
    pthread_mutex_lock(&sub->lock);
    bool propagate = false;
    switch (cmd) {
    case SD_CTRL_SET_VIDEO_DEF_FPS:
        sub->video_fps = *(double *)arg;
        update_subtitle_speed(sub);
        break;
    case SD_CTRL_SUB_STEP: {
        double *a = arg;
        double arg2[2] = {a[0], a[1]};
        arg2[0] = pts_to_subtitle(sub, arg2[0]);
        if (sub->sd->driver->control)
            r = sub->sd->driver->control(sub->sd, cmd, arg2);
        if (r == CONTROL_OK)
            a[0] = pts_from_subtitle(sub, arg2[0]);
        break;
    case SD_CTRL_UPDATE_OPTS:
        if (m_config_cache_update(sub->opts_cache))
            update_subtitle_speed(sub);
        propagate = true;
        break;
    }
    default:
        propagate = true;
    }
    if (propagate && sub->sd->driver->control)
        r = sub->sd->driver->control(sub->sd, cmd, arg);
    pthread_mutex_unlock(&sub->lock);
    return r;
}

void sub_set_recorder_sink(struct dec_sub *sub, struct mp_recorder_sink *sink)
{
    pthread_mutex_lock(&sub->lock);
    sub->recorder_sink = sink;
    pthread_mutex_unlock(&sub->lock);
}

void sub_set_play_dir(struct dec_sub *sub, int dir)
{
    pthread_mutex_lock(&sub->lock);
    sub->play_dir = dir;
    pthread_mutex_unlock(&sub->lock);
}

bool sub_is_secondary_visible(struct dec_sub *sub)
{
    return !!sub->opts->sec_sub_visibility;
}
