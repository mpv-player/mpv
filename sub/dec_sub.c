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

    struct sd *sd;
};

struct packet_list {
    struct demux_packet **packets;
    int num_packets;
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
    assert(demuxer && sh && sh->sub);

    struct mp_log *log = mp_log_new(NULL, global->log, "sub");

    for (int n = 0; sd_list[n]; n++) {
        const struct sd_functions *driver = sd_list[n];
        struct dec_sub *sub = talloc_zero(NULL, struct dec_sub);
        sub->log = talloc_steal(sub, log),
        sub->opts = global->opts;
        sub->sh = sh;
        mpthread_mutex_init_recursive(&sub->lock);

        sub->sd = talloc(NULL, struct sd);
        *sub->sd = (struct sd){
            .global = global,
            .log = mp_log_new(sub->sd, sub->log, driver->name),
            .opts = sub->opts,
            .driver = driver,
            .demuxer = demuxer,
            .sh = sh,
        };

        if (sh->codec && sub->sd->driver->init(sub->sd) >= 0)
            return sub;

        ta_set_parent(log, NULL);
        talloc_free(sub->sd);
        talloc_free(sub);
    }

    mp_err(log, "Could not find subtitle decoder for format '%s'.\n",
           sh->codec ? sh->codec : "<unknown>");
    talloc_free(log);
    return NULL;
}

void sub_decode(struct dec_sub *sub, struct demux_packet *packet)
{
    pthread_mutex_lock(&sub->lock);
    sub->sd->driver->decode(sub->sd, packet);
    pthread_mutex_unlock(&sub->lock);
}

static void add_sub_list(struct dec_sub *sub, struct packet_list *subs)
{
    for (int n = 0; n < subs->num_packets; n++)
        sub->sd->driver->decode(sub->sd, subs->packets[n]);
}

static void add_packet(struct packet_list *subs, struct demux_packet *pkt)
{
    pkt = demux_copy_packet(pkt);
    if (pkt) {
        talloc_steal(subs, pkt);
        MP_TARRAY_APPEND(subs, subs->packets, subs->num_packets, pkt);
    }
}

// Read all packets from the demuxer and decode/add them. Returns false if
// there are circumstances which makes this not possible.
bool sub_read_all_packets(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);

    // Converters are assumed to always accept packets in advance
    if (!sub->sd->driver->accept_packets_in_advance) {
        pthread_mutex_unlock(&sub->lock);
        return false;
    }

    struct packet_list *subs = talloc_zero(NULL, struct packet_list);

    for (;;) {
        struct demux_packet *pkt = demux_read_packet(sub->sh);
        if (!pkt)
            break;
        add_packet(subs, pkt);
        talloc_free(pkt);
    }

    add_sub_list(sub, subs);

    pthread_mutex_unlock(&sub->lock);
    talloc_free(subs);
    return true;
}

bool sub_accepts_packet_in_advance(struct dec_sub *sub)
{
    bool res = true;
    pthread_mutex_lock(&sub->lock);
    if (sub->sd->driver->accepts_packet)
        res &= sub->sd->driver->accepts_packet(sub->sd);
    pthread_mutex_unlock(&sub->lock);
    return res;
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
