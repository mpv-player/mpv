/*
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

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "config.h"
#include "demux/stheader.h"
#include "sd.h"
#include "sub.h"
#include "dec_sub.h"
#include "subreader.h"
#include "core/options.h"
#include "core/mp_msg.h"

extern const struct sd_functions sd_ass;
extern const struct sd_functions sd_lavc;
extern const struct sd_functions sd_spu;
extern const struct sd_functions sd_movtext;
extern const struct sd_functions sd_srt;
extern const struct sd_functions sd_microdvd;
extern const struct sd_functions sd_lavc_conv;

static const struct sd_functions *sd_list[] = {
#ifdef CONFIG_ASS
    &sd_ass,
#endif
    &sd_lavc,
    &sd_spu,
    &sd_movtext,
    &sd_srt,
    &sd_microdvd,
    &sd_lavc_conv,
    NULL
};

#define MAX_NUM_SD 3

struct dec_sub {
    struct MPOpts *opts;
    struct sd init_sd;

    struct sd *sd[MAX_NUM_SD];
    int num_sd;
};

struct dec_sub *sub_create(struct MPOpts *opts)
{
    struct dec_sub *sub = talloc_zero(NULL, struct dec_sub);
    sub->opts = opts;
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
    talloc_free(sub);
}

bool sub_is_initialized(struct dec_sub *sub)
{
    return !!sub->num_sd;
}

struct sd *sub_get_last_sd(struct dec_sub *sub)
{
    return sub->num_sd ? sub->sd[sub->num_sd - 1] : NULL;
}

void sub_set_video_res(struct dec_sub *sub, int w, int h)
{
    sub->init_sd.sub_video_w = w;
    sub->init_sd.sub_video_h = h;
}

void sub_set_extradata(struct dec_sub *sub, void *data, int data_len)
{
    sub->init_sd.extradata = data_len ? talloc_memdup(sub, data, data_len) : NULL;
    sub->init_sd.extradata_len = data_len;
}

void sub_set_ass_renderer(struct dec_sub *sub, struct ass_library *ass_library,
                          struct ass_renderer *ass_renderer)
{
    sub->init_sd.ass_library = ass_library;
    sub->init_sd.ass_renderer = ass_renderer;
}

static void print_chain(struct dec_sub *sub)
{
    mp_msg(MSGT_OSD, MSGL_V, "Subtitle filter chain: ");
    for (int n = 0; n < sub->num_sd; n++) {
        struct sd *sd = sub->sd[n];
        mp_msg(MSGT_OSD, MSGL_V, "%s%s (%s)", n > 0 ? " -> " : "", 
               sd->driver->name, sd->codec);
    }
    mp_msg(MSGT_OSD, MSGL_V, "\n");
}

// Subtitles read with subreader.c
static void read_sub_data(struct dec_sub *sub, struct sub_data *subdata)
{
    assert(sub_accept_packets_in_advance(sub));
    char *temp = NULL;

    struct sd *sd = sub_get_last_sd(sub);

    sd->no_remove_duplicates = true;

    for (int i = 0; i < subdata->sub_num; i++) {
        subtitle *st = &subdata->subtitles[i];
        // subdata is in 10 ms ticks, pts is in seconds
        double t = subdata->sub_uses_time ? 0.01 : (1 / subdata->fallback_fps);

        int len = 0;
        for (int j = 0; j < st->lines; j++)
            len += st->text[j] ? strlen(st->text[j]) : 0;

        len += 2 * st->lines;   // '\N', including the one after the last line
        len += 6;               // {\anX}
        len += 1;               // '\0'

        if (talloc_get_size(temp) < len) {
            talloc_free(temp);
            temp = talloc_array(NULL, char, len);
        }

        char *p = temp;
        char *end = p + len;

        if (st->alignment)
            p += snprintf(p, end - p, "{\\an%d}", st->alignment);

        for (int j = 0; j < st->lines; j++)
            p += snprintf(p, end - p, "%s\\N", st->text[j]);

        if (st->lines > 0)
            p -= 2;             // remove last "\N"
        *p = 0;

        struct demux_packet pkt = {0};
        pkt.pts = st->start * t;
        pkt.duration = (st->end - st->start) * t;
        pkt.buffer = temp;
        pkt.len = strlen(temp);

        sub_decode(sub, &pkt);
    }

    // Hack for broken FFmpeg packet format: make sd_ass keep the subtitle
    // events on reset(), even though broken FFmpeg ASS packets were received
    // (from sd_lavc_conv.c). Normally, these events are removed on seek/reset,
    // but this is obviously unwanted in this case.
    if (sd && sd->driver->fix_events)
        sd->driver->fix_events(sd);

    sd->no_remove_duplicates = false;

    talloc_free(temp);
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

    if (sd->driver->init(sd) < 0)
        return -1;

    return 0;
}

void sub_init_from_sh(struct dec_sub *sub, struct sh_sub *sh)
{
    assert(!sub->num_sd);

    if (sh->extradata && !sub->init_sd.extradata)
        sub_set_extradata(sub, sh->extradata, sh->extradata_len);
    struct sd init_sd = sub->init_sd;
    init_sd.codec = sh->gsh->codec;
    init_sd.ass_track = sh->track;

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
            if (sh->sub_data)
                read_sub_data(sub, sh->sub_data);
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
    mp_msg(MSGT_OSD, MSGL_ERR, "Could not find subtitle decoder for format '%s'.\n",
           sh->gsh->codec ? sh->gsh->codec : "<unknown>");
}

bool sub_accept_packets_in_advance(struct dec_sub *sub)
{
    // Converters are assumed to always accept packets in advance
    struct sd *sd = sub_get_last_sd(sub);
    return sd && sd->driver->accept_packets_in_advance;
}

static void decode_next(struct dec_sub *sub, int n, struct demux_packet *packet)
{
    struct sd *sd = sub->sd[n];
    sd->driver->decode(sd, packet);
    if (n + 1 >= sub->num_sd || !sd->driver->get_converted)
        return;
    while (1) {
        struct demux_packet *next =
            sd->driver->get_converted ? sd->driver->get_converted(sd) : NULL;
        if (!next)
            break;
        decode_next(sub, n + 1, next);
    }
}

void sub_decode(struct dec_sub *sub, struct demux_packet *packet)
{
    if (sub->num_sd > 0)
        decode_next(sub, 0, packet);
}

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
    struct sd *sd = sub_get_last_sd(sub);
    return sd && sd->driver->get_text;
}

char *sub_get_text(struct dec_sub *sub, double pts)
{
    struct MPOpts *opts = sub->opts;
    struct sd *sd = sub_get_last_sd(sub);
    char *text = NULL;
    if (sd && opts->sub_visibility) {
        if (sd->driver->get_text)
            text = sd->driver->get_text(sd, pts);
    }
    return text;
}

void sub_reset(struct dec_sub *sub)
{
    for (int n = 0; n < sub->num_sd; n++) {
        if (sub->sd[n]->driver->reset)
            sub->sd[n]->driver->reset(sub->sd[n]);
    }
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
    mp_msg(MSGT_OSD, MSGL_ERR, "Subtitle too big.\n");
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
