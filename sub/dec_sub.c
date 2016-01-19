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
#include "options/options.h"
#include "common/global.h"
#include "common/msg.h"
#include "osdep/threads.h"

extern const struct sd_functions sd_ass;
extern const struct sd_functions sd_lavc;

static const struct sd_functions *const sd_list[] = {
    &sd_lavc,
#if HAVE_LIBASS
    &sd_ass,
#endif
    NULL
};

struct dec_sub {
    pthread_mutex_t lock;

    struct mp_log *log;
    struct MPOpts *opts;

    struct sh_stream *sh;
    double last_pkt_pts;

    struct sd *sd;
};

void sub_lock(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);
}

void sub_unlock(struct dec_sub *sub)
{
    pthread_mutex_unlock(&sub->lock);
}

void sub_destroy(struct dec_sub *sub)
{
    if (!sub)
        return;
    sub_reset(sub);
    sub->sd->driver->uninit(sub->sd);
    talloc_free(sub->sd);
    pthread_mutex_destroy(&sub->lock);
    talloc_free(sub);
}

// Thread-safety of the returned object: all functions are thread-safe,
// except sub_get_bitmaps() and sub_get_text(). Decoder backends (sd_*)
// do not need to acquire locks.
struct dec_sub *sub_create(struct mpv_global *global, struct demuxer *demuxer,
                           struct sh_stream *sh)
{
    assert(demuxer && sh && sh->type == STREAM_SUB);

    struct mp_log *log = mp_log_new(NULL, global->log, "sub");

    for (int n = 0; sd_list[n]; n++) {
        const struct sd_functions *driver = sd_list[n];
        struct dec_sub *sub = talloc_zero(NULL, struct dec_sub);
        sub->log = talloc_steal(sub, log),
        sub->opts = global->opts;
        sub->sh = sh;
        sub->last_pkt_pts = MP_NOPTS_VALUE;
        mpthread_mutex_init_recursive(&sub->lock);

        sub->sd = talloc(NULL, struct sd);
        *sub->sd = (struct sd){
            .global = global,
            .log = mp_log_new(sub->sd, sub->log, driver->name),
            .opts = sub->opts,
            .driver = driver,
            .demuxer = demuxer,
            .codec = sh->codec,
        };

        if (sh->codec && sub->sd->driver->init(sub->sd) >= 0)
            return sub;

        ta_set_parent(log, NULL);
        talloc_free(sub->sd);
        talloc_free(sub);
    }

    mp_err(log, "Could not find subtitle decoder for format '%s'.\n",
           sh->codec->codec);
    talloc_free(log);
    return NULL;
}

// Read all packets from the demuxer and decode/add them. Returns false if
// there are circumstances which makes this not possible.
bool sub_read_all_packets(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);

    if (!sub->sd->driver->accept_packets_in_advance) {
        pthread_mutex_unlock(&sub->lock);
        return false;
    }

    for (;;) {
        struct demux_packet *pkt = demux_read_packet(sub->sh);
        if (!pkt)
            break;
        sub->sd->driver->decode(sub->sd, pkt);
        talloc_free(pkt);
    }

    pthread_mutex_unlock(&sub->lock);
    return true;
}

// Read packets from the demuxer stream passed to sub_create(). Return true if
// enough packets were read, false if the player should wait until the demuxer
// signals new packets available (and then should retry).
bool sub_read_packets(struct dec_sub *sub, double video_pts)
{
    bool r = true;
    pthread_mutex_lock(&sub->lock);
    while (1) {
        bool read_more = true;
        if (sub->sd->driver->accepts_packet)
            read_more = sub->sd->driver->accepts_packet(sub->sd);

        if (!read_more)
            break;

        struct demux_packet *pkt;
        int st = demux_read_packet_async(sub->sh, &pkt);
        // Note: "wait" (st==0) happens with non-interleaved streams only, and
        // then we should stop the playloop until a new enough packet has been
        // seen (or the subtitle decoder's queue is full). This does not happen
        // for interleaved subtitle streams, which never return "wait" when
        // reading.
        if (st <= 0) {
            r = st < 0 || (sub->last_pkt_pts != MP_NOPTS_VALUE &&
                           sub->last_pkt_pts > video_pts);
            break;
        }

        sub->sd->driver->decode(sub->sd, pkt);
        sub->last_pkt_pts = pkt->pts;
        talloc_free(pkt);
    }
    pthread_mutex_unlock(&sub->lock);
    return r;
}

// You must call sub_lock/sub_unlock if more than 1 thread access sub.
// The issue is that *res will contain decoder allocated data, which might
// be deallocated on the next decoder access.
void sub_get_bitmaps(struct dec_sub *sub, struct mp_osd_res dim, double pts,
                     struct sub_bitmaps *res)
{
    struct MPOpts *opts = sub->opts;

    *res = (struct sub_bitmaps) {0};
    if (opts->sub_visibility && sub->sd->driver->get_bitmaps)
        sub->sd->driver->get_bitmaps(sub->sd, dim, pts, res);
}

// See sub_get_bitmaps() for locking requirements.
// It can be called unlocked too, but then only 1 thread must call this function
// at a time (unless exclusive access is guaranteed).
char *sub_get_text(struct dec_sub *sub, double pts)
{
    pthread_mutex_lock(&sub->lock);
    struct MPOpts *opts = sub->opts;
    char *text = NULL;
    if (opts->sub_visibility && sub->sd->driver->get_text)
        text = sub->sd->driver->get_text(sub->sd, pts);
    pthread_mutex_unlock(&sub->lock);
    return text;
}

void sub_reset(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);
    if (sub->sd->driver->reset)
        sub->sd->driver->reset(sub->sd);
    sub->last_pkt_pts = MP_NOPTS_VALUE;
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
    if (sub->sd->driver->control)
        r = sub->sd->driver->control(sub->sd, cmd, arg);
    pthread_mutex_unlock(&sub->lock);
    return r;
}
