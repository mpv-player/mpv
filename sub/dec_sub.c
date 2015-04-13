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
extern const struct sd_functions sd_movtext;
extern const struct sd_functions sd_srt;
extern const struct sd_functions sd_microdvd;
extern const struct sd_functions sd_lavf_srt;
extern const struct sd_functions sd_lavc_conv;

static const struct sd_functions *const sd_list[] = {
#if HAVE_LIBASS
    &sd_ass,
#endif
    &sd_lavc,
    &sd_movtext,
    &sd_srt,
    &sd_lavf_srt,
    &sd_microdvd,
    &sd_lavc_conv,
    NULL
};

#define MAX_NUM_SD 3

struct dec_sub {
    pthread_mutex_t lock;

    struct mp_log *log;
    struct MPOpts *opts;
    struct sd init_sd;

    double video_fps;
    const char *charset;

    struct sd *sd[MAX_NUM_SD];
    int num_sd;
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

    mpthread_mutex_init_recursive(&sub->lock);

    return sub;
}

static void sub_uninit(struct dec_sub *sub)
{
    sub_reset(sub);
    for (int n = 0; n < sub->num_sd; n++) {
        if (sub->sd[n]->driver->uninit)
            sub->sd[n]->driver->uninit(sub->sd[n]);
        talloc_free(sub->sd[n]);
    }
    sub->num_sd = 0;
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
    bool r = !!sub->num_sd;
    pthread_mutex_unlock(&sub->lock);
    return r;
}

static struct sd *sub_get_last_sd(struct dec_sub *sub)
{
    return sub->num_sd ? sub->sd[sub->num_sd - 1] : NULL;
}

void sub_set_video_res(struct dec_sub *sub, int w, int h)
{
    pthread_mutex_lock(&sub->lock);
    sub->init_sd.sub_video_w = w;
    sub->init_sd.sub_video_h = h;
    pthread_mutex_unlock(&sub->lock);
}

void sub_set_video_fps(struct dec_sub *sub, double fps)
{
    pthread_mutex_lock(&sub->lock);
    sub->video_fps = fps;
    pthread_mutex_unlock(&sub->lock);
}

void sub_set_extradata(struct dec_sub *sub, void *data, int data_len)
{
    pthread_mutex_lock(&sub->lock);
    sub->init_sd.extradata = data_len ? talloc_memdup(sub, data, data_len) : NULL;
    sub->init_sd.extradata_len = data_len;
    pthread_mutex_unlock(&sub->lock);
}

void sub_set_ass_renderer(struct dec_sub *sub, struct ass_library *ass_library,
                          struct ass_renderer *ass_renderer)
{
    pthread_mutex_lock(&sub->lock);
    sub->init_sd.ass_library = ass_library;
    sub->init_sd.ass_renderer = ass_renderer;
    pthread_mutex_unlock(&sub->lock);
}

static void print_chain(struct dec_sub *sub)
{
    MP_VERBOSE(sub, "Subtitle filter chain: ");
    for (int n = 0; n < sub->num_sd; n++) {
        struct sd *sd = sub->sd[n];
        MP_VERBOSE(sub, "%s%s (%s)", n > 0 ? " -> " : "",
               sd->driver->name, sd->codec);
    }
    MP_VERBOSE(sub, "\n");
}

static int sub_init_decoder(struct dec_sub *sub, struct sd *sd)
{
    sd->driver = NULL;
    for (int n = 0; sd_list[n]; n++) {
        if (sd_list[n]->supports_format(sd->codec)) {
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
    assert(!sub->num_sd);
    assert(sh && sh->sub);

    pthread_mutex_lock(&sub->lock);

    if (sh->sub->extradata && !sub->init_sd.extradata)
        sub_set_extradata(sub, sh->sub->extradata, sh->sub->extradata_len);
    struct sd init_sd = sub->init_sd;
    init_sd.codec = sh->codec;

    while (sub->num_sd < MAX_NUM_SD) {
        struct sd *sd = talloc(NULL, struct sd);
        *sd = init_sd;
        sd->opts = sub->opts;
        if (sub_init_decoder(sub, sd) < 0) {
            talloc_free(sd);
            break;
        }
        sub->sd[sub->num_sd] = sd;
        sub->num_sd++;
        // Try adding new converters until a decoder is reached
        if (sd->driver->get_bitmaps || sd->driver->get_text) {
            print_chain(sub);
            pthread_mutex_unlock(&sub->lock);
            return;
        }
        init_sd = (struct sd) {
            .codec = sd->output_codec,
            .converted_from = sd->codec,
            .extradata = sd->output_extradata,
            .extradata_len = sd->output_extradata_len,
            .ass_library = sub->init_sd.ass_library,
            .ass_renderer = sub->init_sd.ass_renderer,
        };
    }

    sub_uninit(sub);
    MP_ERR(sub, "Could not find subtitle decoder for format '%s'.\n",
           sh->codec ? sh->codec : "<unknown>");
    pthread_mutex_unlock(&sub->lock);
}

static struct demux_packet *get_decoded_packet(struct sd *sd)
{
    return sd->driver->get_converted ? sd->driver->get_converted(sd) : NULL;
}

static void decode_chain(struct sd **sd, int num_sd, struct demux_packet *packet)
{
    if (num_sd == 0)
        return;
    struct sd *dec = sd[0];
    dec->driver->decode(dec, packet);
    if (num_sd > 1) {
        while (1) {
            struct demux_packet *next = get_decoded_packet(dec);
            if (!next)
                break;
            decode_chain(sd + 1, num_sd - 1, next);
        }
    }
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
            .duration = in->duration,
            .avpacket = in->avpacket, // questionable, but gives us sidedata
        };
    }
    return pkt;
}

static void decode_chain_recode(struct dec_sub *sub, struct sd **sd, int num_sd,
                                struct demux_packet *packet)
{
    if (num_sd > 0) {
        struct demux_packet *recoded = NULL;
        if (sub->charset)
            recoded = recode_packet(sub->log, packet, sub->charset);
        decode_chain(sd, num_sd, recoded ? recoded : packet);
        talloc_free(recoded);
    }
}

void sub_decode(struct dec_sub *sub, struct demux_packet *packet)
{
    pthread_mutex_lock(&sub->lock);
    decode_chain_recode(sub, sub->sd, sub->num_sd, packet);
    pthread_mutex_unlock(&sub->lock);
}

static const char *guess_sub_cp(struct mp_log *log, struct packet_list *subs,
                                const char *usercp)
{
    if (!mp_charset_requires_guess(usercp))
        return usercp;

    // Concat all subs into a buffer. We can't probably do much better without
    // having the original data (which we don't, not anymore).
    int max_size = 2 * 1024 * 1024;
    const char *sep = "\n\n"; // In utf-16: U+0A0A GURMUKHI LETTER UU
    int sep_len = strlen(sep);
    int num_pkt = 0;
    int size = 0;
    for (int n = 0; n < subs->num_packets; n++) {
        struct demux_packet *pkt = subs->packets[n];
        if (size + pkt->len > max_size)
            break;
        size += pkt->len + sep_len;
        num_pkt++;
    }
    bstr text = {talloc_size(NULL, size), 0};
    for (int n = 0; n < num_pkt; n++) {
        struct demux_packet *pkt = subs->packets[n];
        memcpy(text.start + text.len, pkt->buffer, pkt->len);
        memcpy(text.start + text.len + pkt->len, sep, sep_len);
        text.len += pkt->len + sep_len;
    }
    const char *guess = mp_charset_guess(log, text, usercp, 0);
    talloc_free(text.start);
    return guess;
}

static void multiply_timings(struct packet_list *subs, double factor)
{
    for (int n = 0; n < subs->num_packets; n++) {
        struct demux_packet *pkt = subs->packets[n];
        if (pkt->pts != MP_NOPTS_VALUE)
            pkt->pts *= factor;
        if (pkt->duration > 0)
            pkt->duration *= factor;
    }
}

#define MS_TS(f_ts) ((long long)((f_ts) * 1000 + 0.5))

// Remove overlaps and fill gaps between adjacent subtitle packets. This is done
// by adjusting the duration of the earlier packet. If the gaps or overlap are
// larger than the threshold, or if the durations are close to the threshold,
// don't change the events.
// The algorithm is maximally naive and doesn't work if there are multiple
// overlapping lines. (It's not worth the trouble.)
static void fix_overlaps_and_gaps(struct packet_list *subs)
{
    double threshold = 0.2;     // up to 200 ms overlaps or gaps are removed
    double keep = threshold * 2;// don't change timings if durations are smaller
    for (int i = 0; i < subs->num_packets - 1; i++) {
        struct demux_packet *cur = subs->packets[i];
        struct demux_packet *next = subs->packets[i + 1];
        if (cur->pts != MP_NOPTS_VALUE && cur->duration > 0 &&
            next->pts != MP_NOPTS_VALUE && next->duration > 0)
        {
            double end = cur->pts + cur->duration;
            if (fabs(next->pts - end) <= threshold && cur->duration >= keep &&
                next->duration >= keep)
            {
                // Conceptually: cur->duration = next->pts - cur->pts;
                // But make sure the rounding and conversion to integers in
                // sd_ass.c can't produce overlaps.
                cur->duration = (MS_TS(next->pts) - MS_TS(cur->pts)) / 1000.0;
            }
        }
    }
}

static void add_sub_list(struct dec_sub *sub, int at, struct packet_list *subs)
{
    struct sd *sd = sub_get_last_sd(sub);
    assert(sd);

    sd->no_remove_duplicates = true;

    for (int n = 0; n < subs->num_packets; n++)
        decode_chain_recode(sub, sub->sd + at, sub->num_sd - at, subs->packets[n]);

    // Hack for broken FFmpeg packet format: make sd_ass keep the subtitle
    // events on reset(), even if broken FFmpeg ASS packets were received
    // (from sd_lavc_conv.c). Normally, these events are removed on seek/reset,
    // but this is obviously unwanted in this case.
    if (sd->driver->fix_events)
        sd->driver->fix_events(sd);

    sd->no_remove_duplicates = false;
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
    struct MPOpts *opts = sub->opts;

    pthread_mutex_lock(&sub->lock);

    if (!sub_accept_packets_in_advance(sub) || sub->num_sd < 1) {
        pthread_mutex_unlock(&sub->lock);
        return false;
    }

    struct packet_list *subs = talloc_zero(NULL, struct packet_list);

    // In some cases, we want to put the packets through a decoder first.
    // Preprocess until sub->sd[preprocess].
    int preprocess = 0;

    // movtext is currently the only subtitle format that has text output,
    // but binary input. Do charset conversion after converting to text.
    if (sub->sd[0]->driver == &sd_movtext)
        preprocess = 1;

    // Broken Libav libavformat srt packet format (fix timestamps first).
    if (sub->sd[0]->driver == &sd_lavf_srt)
        preprocess = 1;

    for (;;) {
        struct demux_packet *pkt = demux_read_packet(sh);
        if (!pkt)
            break;
        if (preprocess) {
            decode_chain(sub->sd, preprocess, pkt);
            talloc_free(pkt);
            while (1) {
                pkt = get_decoded_packet(sub->sd[preprocess - 1]);
                if (!pkt)
                    break;
                add_packet(subs, pkt);
            }
        } else {
            add_packet(subs, pkt);
            talloc_free(pkt);
        }
    }

    if (opts->sub_cp && !sh->sub->is_utf8)
        sub->charset = guess_sub_cp(sub->log, subs, opts->sub_cp);

    if (sub->charset && sub->charset[0] && !mp_charset_is_utf8(sub->charset))
        MP_INFO(sub, "Using subtitle charset: %s\n", sub->charset);

    double sub_speed = 1.0;

    if (sub->video_fps && sh->sub->frame_based > 0) {
        MP_VERBOSE(sub, "Frame based format, dummy FPS: %f, video FPS: %f\n",
                   sh->sub->frame_based, sub->video_fps);
        sub_speed *= sh->sub->frame_based / sub->video_fps;
    }

    if (opts->sub_fps && sub->video_fps)
        sub_speed *= opts->sub_fps / sub->video_fps;

    sub_speed *= opts->sub_speed;

    if (sub_speed != 1.0)
        multiply_timings(subs, sub_speed);

    if (opts->sub_fix_timing)
        fix_overlaps_and_gaps(subs);

    if (sh->codec && strcmp(sh->codec, "microdvd") == 0) {
        // The last subtitle event in MicroDVD subs can have duration unset,
        // which means show the subtitle until end of video.
        // See FFmpeg FATE MicroDVD_capability_tester.sub
        if (subs->num_packets) {
            struct demux_packet *last = subs->packets[subs->num_packets - 1];
            if (last->duration <= 0)
                last->duration = 10; // arbitrary
        }
    }

    add_sub_list(sub, preprocess, subs);

    pthread_mutex_unlock(&sub->lock);
    talloc_free(subs);
    return true;
}

bool sub_accept_packets_in_advance(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);
    // Converters are assumed to always accept packets in advance
    struct sd *sd = sub_get_last_sd(sub);
    bool r = sd && sd->driver->accept_packets_in_advance;
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
    struct sd *sd = sub_get_last_sd(sub);

    *res = (struct sub_bitmaps) {0};
    if (sd && opts->sub_visibility) {
        if (sd->driver->get_bitmaps)
            sd->driver->get_bitmaps(sd, dim, pts, res);
    }
}

bool sub_has_get_text(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);
    struct sd *sd = sub_get_last_sd(sub);
    bool r = sd && sd->driver->get_text;
    pthread_mutex_unlock(&sub->lock);
    return r;
}

// See sub_get_bitmaps() for locking requirements.
// It can be called unlocked too, but then only 1 thread must call this function
// at a time (unless exclusive access is guaranteed).
char *sub_get_text(struct dec_sub *sub, double pts)
{
    pthread_mutex_lock(&sub->lock);
    struct MPOpts *opts = sub->opts;
    struct sd *sd = sub_get_last_sd(sub);
    char *text = NULL;
    if (sd && opts->sub_visibility) {
        if (sd->driver->get_text)
            text = sd->driver->get_text(sd, pts);
    }
    pthread_mutex_unlock(&sub->lock);
    return text;
}

void sub_reset(struct dec_sub *sub)
{
    pthread_mutex_lock(&sub->lock);
    for (int n = 0; n < sub->num_sd; n++) {
        if (sub->sd[n]->driver->reset)
            sub->sd[n]->driver->reset(sub->sd[n]);
    }
    pthread_mutex_unlock(&sub->lock);
}

int sub_control(struct dec_sub *sub, enum sd_ctrl cmd, void *arg)
{
    int r = CONTROL_UNKNOWN;
    pthread_mutex_lock(&sub->lock);
    for (int n = 0; n < sub->num_sd; n++) {
        if (sub->sd[n]->driver->control) {
            r = sub->sd[n]->driver->control(sub->sd[n], cmd, arg);
            if (r != CONTROL_UNKNOWN)
                break;
        }
    }
    pthread_mutex_unlock(&sub->lock);
    return r;
}

#define MAX_PACKETS 10
#define MAX_BYTES 10000

struct sd_conv_buffer {
    struct demux_packet pkt[MAX_PACKETS];
    int num_pkt;
    int read_pkt;
    char buffer[MAX_BYTES];
    int cur_buffer;
};

void sd_conv_add_packet(struct sd *sd, void *data, int data_len, double pts,
                        double duration)
{
    if (!sd->sd_conv_buffer)
        sd->sd_conv_buffer = talloc_zero(sd, struct sd_conv_buffer);
    struct sd_conv_buffer *buf = sd->sd_conv_buffer;
    if (buf->num_pkt >= MAX_PACKETS || buf->cur_buffer + data_len + 1 > MAX_BYTES)
        goto out_of_space;
    if (buf->read_pkt == buf->num_pkt)
        sd_conv_def_reset(sd);
    assert(buf->read_pkt == 0); // no mixing of reading/adding allowed
    struct demux_packet *pkt = &buf->pkt[buf->num_pkt++];
    *pkt = (struct demux_packet) {
        .buffer = &buf->buffer[buf->cur_buffer],
        .len = data_len,
        .pts = pts,
        .duration = duration,
    };
    memcpy(pkt->buffer, data, data_len);
    pkt->buffer[data_len] = 0;
    buf->cur_buffer += data_len + 1;
    return;

out_of_space:
    MP_ERR(sd, "Subtitle too big.\n");
}

struct demux_packet *sd_conv_def_get_converted(struct sd *sd)
{
    struct sd_conv_buffer *buf = sd->sd_conv_buffer;
    if (buf && buf->read_pkt < buf->num_pkt)
        return &buf->pkt[buf->read_pkt++];
    return NULL;
}

void sd_conv_def_reset(struct sd *sd)
{
    struct sd_conv_buffer *buf = sd->sd_conv_buffer;
    if (buf) {
        buf->read_pkt = buf->num_pkt = 0;
        buf->cur_buffer = 0;
    }
}
