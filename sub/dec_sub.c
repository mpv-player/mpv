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
#include "misc/charset_conv.h"
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
    struct sd init_sd;

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

// Thread-safety of the returned object: all functions are thread-safe,
// except sub_get_bitmaps() and sub_get_text(). Decoder backends (sd_*)
// do not need to acquire locks.
struct dec_sub *sub_create(struct mpv_global *global)
{
    struct dec_sub *sub = talloc_zero(NULL, struct dec_sub);
    sub->log = mp_log_new(sub, global->log, "sub");
    sub->opts = global->opts;
    sub->init_sd.opts = sub->opts;

    mpthread_mutex_init_recursive(&sub->lock);

    return sub;
}

static void sub_uninit(struct dec_sub *sub)
{
    sub_reset(sub);
    if (sub->sd)
        sub->sd->driver->uninit(sub->sd);
    talloc_free(sub->sd);
    sub->sd = NULL;
}

void sub_destroy(struct dec_sub *sub)
{
    if (!sub)
        return;
    sub_uninit(sub);
    pthread_mutex_destroy(&sub->lock);
    talloc_free(sub);
}

bool sub_is_initialized(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);
    bool r = !!sub->sd;
    pthread_mutex_unlock(&sub->lock);
    return r;
}

void sub_set_video_fps(struct dec_sub *sub, double fps)
{
    pthread_mutex_lock(&sub->lock);
    sub->init_sd.video_fps = fps;
    pthread_mutex_unlock(&sub->lock);
}

void sub_set_ass_renderer(struct dec_sub *sub, struct ass_library *ass_library,
                          struct ass_renderer *ass_renderer,
                          pthread_mutex_t *ass_lock)
{
    pthread_mutex_lock(&sub->lock);
    sub->init_sd.ass_library = ass_library;
    sub->init_sd.ass_renderer = ass_renderer;
    sub->init_sd.ass_lock = ass_lock;
    pthread_mutex_unlock(&sub->lock);
}

static int sub_init_decoder(struct dec_sub *sub, struct sd *sd)
{
    sd->driver = NULL;
    for (int n = 0; sd_list[n]; n++) {
        if (sd->sh->codec && sd_list[n]->supports_format(sd->sh->codec)) {
            sd->driver = sd_list[n];
            break;
        }
    }

    if (!sd->driver)
        return -1;

    sd->log = mp_log_new(sd, sub->log, sd->driver->name);
    if (sd->driver->init(sd) < 0)
        return -1;

    return 0;
}

void sub_init_from_sh(struct dec_sub *sub, struct sh_stream *sh)
{
    assert(!sub->sd);
    assert(sh && sh->sub);

    pthread_mutex_lock(&sub->lock);

    sub->sh = sh;

    struct sd init_sd = sub->init_sd;
    init_sd.sh = sh;

    struct sd *sd = talloc(NULL, struct sd);
    *sd = init_sd;

    if (sub_init_decoder(sub, sd) < 0) {
        if (sd->driver && sd->driver->uninit)
            sd->driver->uninit(sd);
        talloc_free(sd);
        MP_ERR(sub, "Could not find subtitle decoder for format '%s'.\n",
               sh->codec ? sh->codec : "<unknown>");
        pthread_mutex_unlock(&sub->lock);
        return;
    }

    sub->sd = sd;
    pthread_mutex_unlock(&sub->lock);
}

static struct demux_packet *recode_packet(struct mp_log *log,
                                          struct demux_packet *in,
                                          const char *charset)
{
    struct demux_packet *pkt = NULL;
    bstr in_buf = {in->buffer, in->len};
    bstr conv = mp_iconv_to_utf8(log, in_buf, charset, MP_ICONV_VERBOSE);
    if (conv.start && conv.start != in_buf.start) {
        pkt = talloc_ptrtype(NULL, pkt);
        talloc_steal(pkt, conv.start);
        *pkt = (struct demux_packet) {
            .buffer = conv.start,
            .len = conv.len,
            .pts = in->pts,
            .pos = in->pos,
            .duration = in->duration,
            .avpacket = in->avpacket, // questionable, but gives us sidedata
        };
    }
    return pkt;
}

static void decode_chain_recode(struct dec_sub *sub, struct demux_packet *packet)
{
    if (sub->sd) {
        struct demux_packet *recoded = NULL;
        if (sub->sh && sub->sh->sub->charset)
            recoded = recode_packet(sub->log, packet, sub->sh->sub->charset);
        sub->sd->driver->decode(sub->sd, recoded ? recoded : packet);
        talloc_free(recoded);
    }
}

void sub_decode(struct dec_sub *sub, struct demux_packet *packet)
{
    pthread_mutex_lock(&sub->lock);
    decode_chain_recode(sub, packet);
    pthread_mutex_unlock(&sub->lock);
}

static void add_sub_list(struct dec_sub *sub, struct packet_list *subs)
{
    for (int n = 0; n < subs->num_packets; n++)
        decode_chain_recode(sub, subs->packets[n]);
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
bool sub_read_all_packets(struct dec_sub *sub, struct sh_stream *sh)
{
    assert(sh && sh->sub);

    pthread_mutex_lock(&sub->lock);

    // Converters are assumed to always accept packets in advance
    if (!(sub->sd && sub->sd->driver->accept_packets_in_advance)) {
        pthread_mutex_unlock(&sub->lock);
        return false;
    }

    struct packet_list *subs = talloc_zero(NULL, struct packet_list);

    for (;;) {
        struct demux_packet *pkt = demux_read_packet(sh);
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
    if (sub->sd && sub->sd->driver->accepts_packet)
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
    if (sub->sd && opts->sub_visibility && sub->sd->driver->get_bitmaps)
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
    if (sub->sd && opts->sub_visibility && sub->sd->driver->get_text)
        text = sub->sd->driver->get_text(sub->sd, pts);
    pthread_mutex_unlock(&sub->lock);
    return text;
}

void sub_reset(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);
    if (sub->sd && sub->sd->driver->reset)
        sub->sd->driver->reset(sub->sd);
    pthread_mutex_unlock(&sub->lock);
}

int sub_control(struct dec_sub *sub, enum sd_ctrl cmd, void *arg)
{
    int r = CONTROL_UNKNOWN;
    pthread_mutex_lock(&sub->lock);
    if (sub->sd && sub->sd->driver->control)
        r = sub->sd->driver->control(sub->sd, cmd, arg);
    pthread_mutex_unlock(&sub->lock);
    return r;
}
