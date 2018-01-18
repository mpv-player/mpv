/*
 * audio filter for runtime AC-3 encoding with libavcodec.
 *
 * Copyright (C) 2007 Ulion <ulion A gmail P com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/common.h>
#include <libavutil/bswap.h>
#include <libavutil/mem.h>

#include "audio/aframe.h"
#include "audio/chmap_sel.h"
#include "audio/fmt-conversion.h"
#include "audio/format.h"
#include "common/av_common.h"
#include "common/common.h"
#include "filters/f_autoconvert.h"
#include "filters/f_utils.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "options/m_option.h"


#define AC3_MAX_CHANNELS 6
#define AC3_MAX_CODED_FRAME_SIZE 3840
#define AC3_FRAME_SIZE (6  * 256)
const uint16_t ac3_bitrate_tab[19] = {
    32, 40, 48, 56, 64, 80, 96, 112, 128,
    160, 192, 224, 256, 320, 384, 448, 512, 576, 640
};

struct f_opts {
    int add_iec61937_header;
    int bit_rate;
    int min_channel_num;
    char *encoder;
    char **avopts;
};

struct priv {
    struct f_opts *opts;

    struct mp_pin *in_pin;
    struct mp_aframe *cur_format;
    struct mp_aframe *in_frame;
    struct mp_aframe_pool *out_pool;

    struct AVCodec        *lavc_acodec;
    struct AVCodecContext *lavc_actx;
    int bit_rate;
    int out_samples;    // upper bound on encoded output per AC3 frame
};

static bool reinit(struct mp_filter *f)
{
    struct priv *s = f->priv;

    mp_aframe_reset(s->cur_format);

    static const int default_bit_rate[AC3_MAX_CHANNELS+1] = \
        {0, 96000, 192000, 256000, 384000, 448000, 448000};

    if (s->opts->add_iec61937_header) {
        s->out_samples = AC3_FRAME_SIZE;
    } else {
        s->out_samples = AC3_MAX_CODED_FRAME_SIZE /
                         mp_aframe_get_sstride(s->in_frame);
    }

    int format = mp_aframe_get_format(s->in_frame);
    int rate = mp_aframe_get_rate(s->in_frame);
    struct mp_chmap chmap = {0};
    mp_aframe_get_chmap(s->in_frame, &chmap);

    int bit_rate = s->bit_rate;
    if (!bit_rate && chmap.num < AC3_MAX_CHANNELS + 1)
        bit_rate = default_bit_rate[chmap.num];

    avcodec_close(s->lavc_actx);

    // Put sample parameters
    s->lavc_actx->sample_fmt = af_to_avformat(format);
    s->lavc_actx->channels = chmap.num;
    s->lavc_actx->channel_layout = mp_chmap_to_lavc(&chmap);
    s->lavc_actx->sample_rate = rate;
    s->lavc_actx->bit_rate = bit_rate;

    if (avcodec_open2(s->lavc_actx, s->lavc_acodec, NULL) < 0) {
        MP_ERR(f, "Couldn't open codec %s, br=%d.\n", "ac3", bit_rate);
        return false;
    }

    if (s->lavc_actx->frame_size < 1) {
        MP_ERR(f, "encoder didn't specify input frame size\n");
        return false;
    }

    mp_aframe_config_copy(s->cur_format, s->in_frame);
    return true;
}

static void reset(struct mp_filter *f)
{
    struct priv *s = f->priv;

    TA_FREEP(&s->in_frame);
}

static void destroy(struct mp_filter *f)
{
    struct priv *s = f->priv;

    reset(f);
    avcodec_free_context(&s->lavc_actx);
}

static void swap_16(uint16_t *ptr, size_t size)
{
    for (size_t n = 0; n < size; n++)
        ptr[n] = av_bswap16(ptr[n]);
}

static void process(struct mp_filter *f)
{
    struct priv *s = f->priv;

    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    bool err = true;
    struct mp_aframe *out = NULL;
    AVPacket pkt = {0};
    av_init_packet(&pkt);

    // Send input as long as it wants.
    while (1) {
        if (avcodec_is_open(s->lavc_actx)) {
            int lavc_ret = avcodec_receive_packet(s->lavc_actx, &pkt);
            if (lavc_ret >= 0)
                break;
            if (lavc_ret < 0 && lavc_ret != AVERROR(EAGAIN)) {
                MP_FATAL(f, "Encode failed (receive).\n");
                goto done;
            }
        }
        AVFrame *frame = NULL;
        struct mp_frame input = mp_pin_out_read(s->in_pin);
        // The following code assumes no sample data buffering in the encoder.
        if (input.type == MP_FRAME_EOF) {
            mp_pin_in_write(f->ppins[1], input);
            return;
        } else if (input.type == MP_FRAME_AUDIO) {
            TA_FREEP(&s->in_frame);
            s->in_frame = input.data;
            frame = mp_frame_to_av(input, NULL);
            if (!frame)
                goto done;
            if (mp_aframe_get_channels(s->in_frame) < s->opts->min_channel_num) {
                // Just pass it through.
                s->in_frame = NULL;
                mp_pin_in_write(f->ppins[1], input);
                return;
            }
            if (!mp_aframe_config_equals(s->in_frame, s->cur_format)) {
                if (!reinit(f))
                    goto done;
            }
        } else if (input.type) {
            goto done;
        } else {
            return; // no data yet
        }
        int lavc_ret = avcodec_send_frame(s->lavc_actx, frame);
        av_frame_free(&frame);
        if (lavc_ret < 0 && lavc_ret != AVERROR(EAGAIN)) {
            MP_FATAL(f, "Encode failed (send).\n");
            goto done;
        }
    }

    if (!s->in_frame)
        goto done;

    out = mp_aframe_create();
    mp_aframe_set_format(out, AF_FORMAT_S_AC3);
    mp_aframe_set_chmap(out, &(struct mp_chmap)MP_CHMAP_INIT_STEREO);
    mp_aframe_set_rate(out, 48000);

    if (mp_aframe_pool_allocate(s->out_pool, out, s->out_samples) < 0)
        goto done;

    int sstride = mp_aframe_get_sstride(out);

    mp_aframe_copy_attributes(out, s->in_frame);

    int frame_size = pkt.size;
    int header_len = 0;
    char hdr[8];

    if (s->opts->add_iec61937_header && pkt.size > 5) {
        int bsmod = pkt.data[5] & 0x7;
        int len = frame_size;

        frame_size = AC3_FRAME_SIZE * 2 * 2;
        header_len = 8;

        AV_WL16(hdr,     0xF872);   // iec 61937 syncword 1
        AV_WL16(hdr + 2, 0x4E1F);   // iec 61937 syncword 2
        hdr[5] = bsmod;             // bsmod
        hdr[4] = 0x01;              // data-type ac3
        AV_WL16(hdr + 6, len << 3); // number of bits in payload
    }

    if (frame_size > s->out_samples * sstride)
        abort();

    uint8_t **planes = mp_aframe_get_data_rw(out);
    if (!planes)
        goto done;
    char *buf = planes[0];
    memcpy(buf, hdr, header_len);
    memcpy(buf + header_len, pkt.data, pkt.size);
    memset(buf + header_len + pkt.size, 0,
           frame_size - (header_len + pkt.size));
    swap_16((uint16_t *)(buf + header_len), pkt.size / 2);
    mp_aframe_set_size(out, frame_size / sstride);
    mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_AUDIO, out));
    out = NULL;

    err = 0;
done:
    av_packet_unref(&pkt);
    talloc_free(out);
    if (err)
        mp_filter_internal_mark_failed(f);
}

static const struct mp_filter_info af_lavcac3enc_filter = {
    .name = "lavcac3enc",
    .priv_size = sizeof(struct priv),
    .process = process,
    .reset = reset,
    .destroy = destroy,
};

static struct mp_filter *af_lavcac3enc_create(struct mp_filter *parent,
                                              void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &af_lavcac3enc_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *s = f->priv;
    s->opts = talloc_steal(s, options);
    s->cur_format = talloc_steal(s, mp_aframe_create());
    s->out_pool = mp_aframe_pool_create(s);

    s->lavc_acodec = avcodec_find_encoder_by_name(s->opts->encoder);
    if (!s->lavc_acodec) {
        MP_ERR(f, "Couldn't find encoder %s.\n", s->opts->encoder);
        goto error;
    }

    s->lavc_actx = avcodec_alloc_context3(s->lavc_acodec);
    if (!s->lavc_actx) {
        MP_ERR(f, "Audio LAVC, couldn't allocate context!\n");
        goto error;
    }

    if (mp_set_avopts(f->log, s->lavc_actx, s->opts->avopts) < 0)
        goto error;

    // For this one, we require the decoder to expert lists of all supported
    // parameters. (Not all decoders do that, but the ones we're interested
    // in do.)
    if (!s->lavc_acodec->sample_fmts ||
        !s->lavc_acodec->channel_layouts)
    {
        MP_ERR(f, "Audio encoder doesn't list supported parameters.\n");
        goto error;
    }

    if (s->opts->bit_rate) {
        int i;
        for (i = 0; i < 19; i++) {
            if (ac3_bitrate_tab[i] == s->opts->bit_rate) {
                s->bit_rate = ac3_bitrate_tab[i] * 1000;
                break;
            }
        }
        if (i >= 19) {
            MP_WARN(f, "unable set unsupported bitrate %d, using default "
                    "bitrate (check manpage to see supported bitrates).\n",
                    s->opts->bit_rate);
        }
    }

    struct mp_autoconvert *conv = mp_autoconvert_create(f);
    if (!conv)
        abort();

    const enum AVSampleFormat *lf = s->lavc_acodec->sample_fmts;
    for (int i = 0; lf && lf[i] != AV_SAMPLE_FMT_NONE; i++) {
        int mpfmt = af_from_avformat(lf[i]);
        if (mpfmt)
            mp_autoconvert_add_afmt(conv, mpfmt);
    }

    const uint64_t *lch = s->lavc_acodec->channel_layouts;
    for (int n = 0; lch && lch[n]; n++) {
        struct mp_chmap chmap = {0};
        mp_chmap_from_lavc(&chmap, lch[n]);
        if (mp_chmap_is_valid(&chmap))
            mp_autoconvert_add_chmap(conv, &chmap);
    }

    // At least currently, the AC3 encoder doesn't export sample rates.
    mp_autoconvert_add_srate(conv, 48000);

    mp_pin_connect(conv->f->pins[0], f->ppins[0]);

    struct mp_filter *fs = mp_fixed_aframe_size_create(f, AC3_FRAME_SIZE, true);
    if (!fs)
        abort();

    mp_pin_connect(fs->pins[0], conv->f->pins[1]);
    s->in_pin = fs->pins[1];

    return f;

error:
    talloc_free(f);
    return NULL;
}

#define OPT_BASE_STRUCT struct f_opts

const struct mp_user_filter_entry af_lavcac3enc = {
    .desc = {
        .description = "runtime encode to ac3 using libavcodec",
        .name = "lavcac3enc",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT) {
            .add_iec61937_header = 1,
            .bit_rate = 640,
            .min_channel_num = 3,
            .encoder = "ac3",
        },
        .options = (const struct m_option[]) {
            OPT_FLAG("tospdif", add_iec61937_header, 0),
            OPT_CHOICE_OR_INT("bitrate", bit_rate, 0, 32, 640,
                            ({"auto", 0}, {"default", 0})),
            OPT_INTRANGE("minch", min_channel_num, 0, 2, 6),
            OPT_STRING("encoder", encoder, 0),
            OPT_KEYVALUELIST("o", avopts, 0),
            {0}
        },
    },
    .create = af_lavcac3enc_create,
};
