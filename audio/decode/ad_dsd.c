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

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "audio/aframe.h"
#include "audio/chmap.h"
#include "audio/chmap_avchannel.h"
#include "audio/format.h"
#include "common/av_common.h"
#include "common/codecs.h"
#include "common/common.h"
#include "common/msg.h"
#include "demux/packet.h"
#include "demux/stheader.h"
#include "filters/f_decoder_wrapper.h"
#include "filters/filter_internal.h"
#include "misc/bstr.h"
#include "osdep/compiler.h"

// Each output sample carries one marker byte and 16 DSD bits per channel:
// (marker << 24) | (dsd0 << 16) | (dsd1 << 8), with the oldest DSD bit in
// the MSB and the marker alternating between 0x05 and 0xfa every sample.
// See <https://www.dsd-guide.com/dop-open-standard>

struct priv {
    bool planar;
    int rate;               // output PCM rate (DSD byte rate / 2)
    struct mp_chmap chmap;
    struct mp_aframe *fmt;
    struct mp_aframe_pool *pool;
    uint8_t bits[256];      // maps input byte to MSB-first DSD bits
    bool marker;
    bool have_hold;
    uint8_t hold[MP_NUM_CHANNELS]; // odd trailing byte carried between packets
    bool warned_align;

    // DST decompression (lossless DST -> DSD)
    AVCodecContext *avctx;
    AVPacket *avpkt;
    AVFrame *avframe;

    struct mp_decoder public;
};

static uint8_t reverse_bits8(uint8_t v)
{
#if __has_builtin(__builtin_bitreverse8)
    return __builtin_bitreverse8(v);
#else
    v = (v & 0xf0) >> 4 | (v & 0x0f) << 4;
    v = (v & 0xcc) >> 2 | (v & 0x33) << 2;
    v = (v & 0xaa) >> 1 | (v & 0x55) << 1;
    return v;
#endif
}

// Per-channel input byte stream: the held byte (if any), then packet data.
static inline uint8_t in_byte(struct priv *p, const uint8_t *plane, int step,
                              int c, int i)
{
    if (p->have_hold) {
        if (i == 0)
            return p->hold[c];
        i -= 1;
    }
    return p->bits[plane[(size_t)i * step]];
}

// Convert per_ch DSD bytes per channel to DoP and output the result.
// planes[c] points at the first byte of channel c, consecutive bytes of one
// channel are step bytes apart.
static void emit_frame(struct mp_filter *da, const uint8_t *const *planes,
                       int step, int per_ch, double pts)
{
    struct priv *p = da->priv;
    int nch = p->chmap.num;
    int total = per_ch + (p->have_hold ? 1 : 0);
    int frames = total / 2;

    if (!frames) {
        if (per_ch) {
            for (int c = 0; c < nch; c++)
                p->hold[c] = in_byte(p, planes[c], step, c, total - 1);
            p->have_hold = true;
        }
        mp_filter_internal_mark_progress(da);
        return;
    }

    if (pts != MP_NOPTS_VALUE && p->have_hold)
        pts -= 0.5 / p->rate;

    struct mp_aframe *out = mp_aframe_new_ref(p->fmt);
    if (mp_aframe_pool_allocate(p->pool, out, frames) < 0)
        goto fail;

    uint8_t **data = mp_aframe_get_data_rw(out);
    if (!data)
        goto fail;
    uint32_t *dst = (uint32_t *)data[0];

    for (int f = 0; f < frames; f++) {
        uint32_t marker = p->marker ? 0xfa : 0x05;
        p->marker = !p->marker;
        for (int c = 0; c < nch; c++) {
            uint32_t b0 = in_byte(p, planes[c], step, c, f * 2);
            uint32_t b1 = in_byte(p, planes[c], step, c, f * 2 + 1);
            *dst++ = marker << 24 | b0 << 16 | b1 << 8;
        }
    }

    bool hold = total & 1;
    if (hold) {
        uint8_t tmp[MP_NUM_CHANNELS];
        for (int c = 0; c < nch; c++)
            tmp[c] = in_byte(p, planes[c], step, c, total - 1);
        memcpy(p->hold, tmp, nch);
    }
    p->have_hold = hold;

    mp_aframe_set_pts(out, pts);
    mp_pin_in_write(da->ppins[1], MAKE_FRAME(MP_FRAME_AUDIO, out));
    return;

fail:
    talloc_free(out);
    mp_filter_internal_mark_failed(da);
}

static void process_dst(struct mp_filter *da, struct demux_packet *mpkt)
{
    struct priv *p = da->priv;
    int nch = p->chmap.num;

    mp_set_av_packet(p->avpkt, mpkt, NULL);
    int ret = avcodec_send_packet(p->avctx, p->avpkt);
    if (ret >= 0)
        ret = avcodec_receive_frame(p->avctx, p->avframe);
    if (ret < 0) {
        MP_WARN(da, "Error decoding DST frame.\n");
        mp_filter_internal_mark_progress(da);
        return;
    }

    if (p->avframe->format != AV_SAMPLE_FMT_U8 ||
        p->avframe->ch_layout.nb_channels != nch)
    {
        MP_ERR(da, "Unexpected DST decoder output.\n");
        av_frame_unref(p->avframe);
        mp_filter_internal_mark_failed(da);
        return;
    }

    const uint8_t *planes[MP_NUM_CHANNELS];
    for (int c = 0; c < nch; c++)
        planes[c] = p->avframe->data[0] + c;
    emit_frame(da, planes, nch, p->avframe->nb_samples, mpkt->pts);

    av_frame_unref(p->avframe);
}

static void process(struct mp_filter *da)
{
    struct priv *p = da->priv;

    if (!mp_pin_can_transfer_data(da->ppins[1], da->ppins[0]))
        return;

    struct mp_frame inframe = mp_pin_out_read(da->ppins[0]);
    if (inframe.type == MP_FRAME_EOF) {
        // A held byte is half a DoP frame; nothing useful can be output.
        p->have_hold = false;
        mp_pin_in_write(da->ppins[1], inframe);
        return;
    } else if (inframe.type != MP_FRAME_PACKET) {
        if (inframe.type) {
            MP_ERR(da, "unknown frame type\n");
            mp_filter_internal_mark_failed(da);
        }
        return;
    }

    struct demux_packet *mpkt = inframe.data;

    if (p->avctx) {
        process_dst(da, mpkt);
    } else {
        int nch = p->chmap.num;
        int per_ch = (int)MPMIN(mpkt->len / nch, INT_MAX / 2);

        if (mpkt->len % nch && !p->warned_align) {
            MP_WARN(da, "Packet not aligned to channel count.\n");
            p->warned_align = true;
        }

        const uint8_t *planes[MP_NUM_CHANNELS];
        for (int c = 0; c < nch; c++)
            planes[c] = mpkt->buffer + (p->planar ? c * (size_t)per_ch : c);
        emit_frame(da, planes, p->planar ? 1 : nch, per_ch, mpkt->pts);
    }

    talloc_free(mpkt);
}

static void reset(struct mp_filter *da)
{
    struct priv *p = da->priv;

    p->have_hold = false;
    p->marker = false;
    if (p->avctx)
        avcodec_flush_buffers(p->avctx);
}

static void destroy(struct mp_filter *da)
{
    struct priv *p = da->priv;

    avcodec_free_context(&p->avctx);
    mp_free_av_packet(&p->avpkt);
    av_frame_free(&p->avframe);
}

static const struct {
    const char *codec;
    bool lsbf, planar, dst;
} dsd_codecs[] = {
    {"dsd_lsbf",        true,  false, false},
    {"dsd_lsbf_planar", true,  true,  false},
    {"dsd_msbf",        false, false, false},
    {"dsd_msbf_planar", false, true,  false},
    {"dst",             false, false, true},
};

// codec is the libavcodec name of the source audio codec.
// pref is the ","-separated list from --audio-spdif; the "dsd" entry enables
// DoP pass-through for all raw DSD and DST codecs.
struct mp_decoder_list *select_dsd_codec(const char *codec, const char *pref)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);

    if (!codec)
        return list;

    bool found = false;
    for (int n = 0; n < MP_ARRAY_SIZE(dsd_codecs); n++)
        found |= strcmp(dsd_codecs[n].codec, codec) == 0;
    if (!found)
        return list;

    bstr sel = bstr0(pref);
    while (sel.len) {
        bstr entry;
        bstr_split_tok(sel, ",", &entry, &sel);
        if (bstr_equals0(entry, "dsd")) {
            mp_add_decoder(list, codec, "dop",
                           "DSD to DSD-over-PCM (DoP) pass-through decoder");
            break;
        }
    }
    return list;
}

static const struct mp_filter_info ad_dsd_filter = {
    .name = "ad_dsd",
    .priv_size = sizeof(struct priv),
    .process = process,
    .reset = reset,
    .destroy = destroy,
};

static bool init_dst_decoder(struct mp_filter *da)
{
    struct priv *p = da->priv;

    const AVCodec *dec = avcodec_find_decoder(AV_CODEC_ID_DST);
    if (!dec)
        return false;

    p->avctx = avcodec_alloc_context3(dec);
    p->avpkt = av_packet_alloc();
    p->avframe = av_frame_alloc();
    if (!p->avctx || !p->avpkt || !p->avframe)
        return false;

    p->avctx->sample_rate = p->rate * 2;
    mp_chmap_to_av_layout(&p->avctx->ch_layout, &p->chmap);

    if (av_opt_set_int(p->avctx, "raw_dsd", 1, AV_OPT_SEARCH_CHILDREN) < 0) {
        MP_ERR(da, "FFmpeg DST decoder does not support raw DSD output.\n");
        return false;
    }

    if (avcodec_open2(p->avctx, dec, NULL) < 0 ||
        p->avctx->sample_fmt != AV_SAMPLE_FMT_U8)
        return false;

    return true;
}

static struct mp_decoder *create(struct mp_filter *parent,
                                 struct mp_codec_params *codec,
                                 const char *decoder)
{
    struct mp_filter *da = mp_filter_create(parent, &ad_dsd_filter);
    if (!da)
        return NULL;

    mp_filter_add_pin(da, MP_PIN_IN, "in");
    mp_filter_add_pin(da, MP_PIN_OUT, "out");

    da->log = mp_log_new(da, parent->log, NULL);

    struct priv *p = da->priv;
    p->public.f = da;

    bool lsbf = false, dst = false, found = false;
    for (int n = 0; n < MP_ARRAY_SIZE(dsd_codecs); n++) {
        if (codec->codec && strcmp(dsd_codecs[n].codec, codec->codec) == 0) {
            lsbf = dsd_codecs[n].lsbf;
            p->planar = dsd_codecs[n].planar;
            dst = dsd_codecs[n].dst;
            found = true;
            break;
        }
    }

    // codec->samplerate is the DSD byte rate. DSD64 (2.8224 MHz) is the
    // lowest rate DoP is defined for.
    if (!found || codec->samplerate < 352800 || codec->samplerate % 2 ||
        !mp_chmap_is_valid(&codec->channels))
    {
        MP_ERR(da, "Codec parameters unsuitable for DoP.\n");
        goto fail;
    }

    p->rate = codec->samplerate / 2;
    p->chmap = codec->channels;

    if (dst && !init_dst_decoder(da))
        goto fail;

    for (int n = 0; n < 256; n++)
        p->bits[n] = lsbf ? reverse_bits8(n) : n;

    p->fmt = mp_aframe_create();
    talloc_steal(p, p->fmt);
    mp_aframe_set_chmap(p->fmt, &p->chmap);
    mp_aframe_set_format(p->fmt, AF_FORMAT_S_DOP);
    mp_aframe_set_rate(p->fmt, p->rate);

    p->pool = mp_aframe_pool_create(p);

    return &p->public;

fail:
    talloc_free(da);
    return NULL;
}

const struct mp_decoder_fns ad_dsd = {
    .create = create,
};
