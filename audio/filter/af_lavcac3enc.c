/*
 * audio filter for runtime AC-3 encoding with libavcodec.
 *
 * Copyright (C) 2007 Ulion <ulion A gmail P com>
 *
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

#include "config.h"

#include "common/common.h"
#include "af.h"
#include "audio/audio_buffer.h"
#include "audio/chmap_sel.h"
#include "audio/fmt-conversion.h"


#define AC3_MAX_CHANNELS 6
#define AC3_MAX_CODED_FRAME_SIZE 3840
#define AC3_FRAME_SIZE (6  * 256)
const uint16_t ac3_bitrate_tab[19] = {
    32, 40, 48, 56, 64, 80, 96, 112, 128,
    160, 192, 224, 256, 320, 384, 448, 512, 576, 640
};

// Data for specific instances of this filter
typedef struct af_ac3enc_s {
    struct AVCodec        *lavc_acodec;
    struct AVCodecContext *lavc_actx;
    int bit_rate;
    struct mp_audio *input;     // frame passed to libavcodec
    struct mp_audio *pending;   // unconsumed input data
    int in_samples;     // samples of input per AC3 frame
    int out_samples;    // upper bound on encoded output per AC3 frame

    int cfg_add_iec61937_header;
    int cfg_bit_rate;
    int cfg_min_channel_num;
    char *cfg_encoder;
} af_ac3enc_t;

// fmt carries the input format. Change it to the best next-possible format
// the encoder likely accepts.
static void select_encode_format(AVCodecContext *c, struct mp_audio *fmt)
{
    int formats[AF_FORMAT_COUNT];
    af_get_best_sample_formats(fmt->format, formats);

    for (int n = 0; formats[n]; n++) {
        const enum AVSampleFormat *lf = c->codec->sample_fmts;
        for (int i = 0; lf && lf[i] != AV_SAMPLE_FMT_NONE; i++) {
            int mpfmt = af_from_avformat(lf[i]);
            if (mpfmt && mpfmt == formats[n]) {
                mp_audio_set_format(fmt, mpfmt);
                goto done_fmt;
            }
        }
    }
done_fmt: ;

    int rate =
        af_select_best_samplerate(fmt->rate, c->codec->supported_samplerates);
    if (rate > 0)
        fmt->rate = rate;

    struct mp_chmap_sel sel = {0};
    const uint64_t *lch = c->codec->channel_layouts;
    for (int n = 0; lch && lch[n]; n++) {
        struct mp_chmap chmap = {0};
        mp_chmap_from_lavc(&chmap, lch[n]);
        mp_chmap_sel_add_map(&sel, &chmap);
    }
    struct mp_chmap res = fmt->channels;
    mp_chmap_sel_adjust(&sel, &res);
    if (!mp_chmap_is_empty(&res))
        mp_audio_set_channels(fmt, &res);
}

// Initialization and runtime control
static int control(struct af_instance *af, int cmd, void *arg)
{
    af_ac3enc_t *s  = af->priv;
    static const int default_bit_rate[AC3_MAX_CHANNELS+1] = \
        {0, 96000, 192000, 256000, 384000, 448000, 448000};

    switch (cmd){
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio orig_in = *in;

        if (!af_fmt_is_pcm(in->format) || in->nch < s->cfg_min_channel_num)
            return AF_DETACH;

        // At least currently, the AC3 encoder doesn't export sample rates.
        in->rate = 48000;
        select_encode_format(s->lavc_actx, in);

        af->data->rate = in->rate;
        mp_audio_set_format(af->data, AF_FORMAT_S_AC3);
        mp_audio_set_num_channels(af->data, 2);

        if (!mp_audio_config_equals(in, &orig_in))
            return AF_FALSE;

        if (s->cfg_add_iec61937_header) {
            s->out_samples = AC3_FRAME_SIZE;
        } else {
            s->out_samples = AC3_MAX_CODED_FRAME_SIZE / af->data->sstride;
        }

        mp_audio_copy_config(s->input, in);

        talloc_free(s->pending);
        s->pending = NULL;

        MP_DBG(af, "reinit: %d, %d, %d.\n", in->nch, in->rate, s->in_samples);

        int bit_rate = s->bit_rate ? s->bit_rate : default_bit_rate[in->nch];

        if (s->lavc_actx->channels != in->nch ||
            s->lavc_actx->sample_rate != in->rate ||
            s->lavc_actx->bit_rate != bit_rate)
        {
            avcodec_close(s->lavc_actx);

            // Put sample parameters
            s->lavc_actx->sample_fmt = af_to_avformat(in->format);
            s->lavc_actx->channels = in->nch;
            s->lavc_actx->channel_layout = mp_chmap_to_lavc(&in->channels);
            s->lavc_actx->sample_rate = in->rate;
            s->lavc_actx->bit_rate = bit_rate;

            if (avcodec_open2(s->lavc_actx, s->lavc_acodec, NULL) < 0) {
                MP_ERR(af, "Couldn't open codec %s, br=%d.\n", "ac3", bit_rate);
                return AF_ERROR;
            }

            if (s->lavc_actx->frame_size < 1) {
                MP_ERR(af, "encoder didn't specify input frame size\n");
                return AF_ERROR;
            }
        }
        s->in_samples = s->lavc_actx->frame_size;
        mp_audio_realloc(s->input, s->in_samples);
        s->input->samples = 0;
        return AF_OK;
    }
    case AF_CONTROL_RESET:
        if (avcodec_is_open(s->lavc_actx))
            avcodec_flush_buffers(s->lavc_actx);
        talloc_free(s->pending);
        s->pending = NULL;
        s->input->samples = 0;
        return AF_OK;
    }
    return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance* af)
{
    af_ac3enc_t *s = af->priv;

    if (s) {
        if(s->lavc_actx) {
            avcodec_close(s->lavc_actx);
            av_free(s->lavc_actx);
        }
        talloc_free(s->pending);
    }
}

static void update_delay(struct af_instance *af)
{
    af_ac3enc_t *s = af->priv;
    af->delay = ((s->pending ? s->pending->samples : 0) + s->input->samples) /
                (double)s->input->rate;
}

static int filter_frame(struct af_instance *af, struct mp_audio *audio)
{
    af_ac3enc_t *s = af->priv;

    // filter_output must have been called until no output was produced.
    if (s->pending && s->pending->samples)
        MP_ERR(af, "broken data flow\n");

    talloc_free(s->pending);
    s->pending = audio;
    update_delay(af);
    return 0;
}

static void swap_16(uint16_t *ptr, size_t size)
{
    for (size_t n = 0; n < size; n++)
        ptr[n] = av_bswap16(ptr[n]);
}

// Copy data from input frame to encode frame (because libavcodec wants a full
// AC3 frame for encoding, while filter input frames can be smaller or larger).
// Return true if the frame is complete.
static bool fill_buffer(struct af_instance *af)
{
    af_ac3enc_t *s = af->priv;

    af->delay = 0;

    if (s->pending) {
        int copy = MPMIN(s->in_samples - s->input->samples, s->pending->samples);
        s->input->samples += copy;
        mp_audio_copy(s->input, s->input->samples - copy, s->pending, 0, copy);
        mp_audio_skip_samples(s->pending, copy);
    }
    update_delay(af);
    return s->input->samples >= s->in_samples;
}

// Return <0 on error, 0 on need more input, 1 on success (and *frame set).
// To actually advance the read pointer, set s->input->samples=0 afterwards.
static int read_input_frame(struct af_instance *af, AVFrame *frame)
{
    af_ac3enc_t *s = af->priv;
    if (!fill_buffer(af))
        return 0; // need more input

    frame->nb_samples = s->in_samples;
    frame->format = s->lavc_actx->sample_fmt;
    frame->channel_layout = s->lavc_actx->channel_layout;
#if LIBAVUTIL_VERSION_MICRO >= 100
    frame->channels = s->lavc_actx->channels;
#endif
    assert(s->input->num_planes <= AV_NUM_DATA_POINTERS);
    frame->extended_data = frame->data;
    for (int n = 0; n < s->input->num_planes; n++)
        frame->data[n] = s->input->planes[n];
    frame->linesize[0] = s->input->samples * s->input->sstride;

    return 1;
}

static int filter_out(struct af_instance *af)
{
    af_ac3enc_t *s = af->priv;

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        MP_FATAL(af, "Could not allocate memory \n");
        return -1;
    }
    int err = -1;

    AVPacket pkt = {0};
    av_init_packet(&pkt);

#if HAVE_AVCODEC_NEW_CODEC_API
    // Send input as long as it wants.
    while (1) {
        err = read_input_frame(af, frame);
        if (err < 0)
            goto done;
        if (err == 0)
            break;
        err = -1;
        int lavc_ret = avcodec_send_frame(s->lavc_actx, frame);
        // On EAGAIN, we're supposed to read remaining output.
        if (lavc_ret == AVERROR(EAGAIN))
            break;
        if (lavc_ret < 0) {
            MP_FATAL(af, "Encode failed.\n");
            goto done;
        }
        s->input->samples = 0;
    }
    int lavc_ret = avcodec_receive_packet(s->lavc_actx, &pkt);
    if (lavc_ret == AVERROR(EAGAIN)) {
        // Need to buffer more input.
        err = 0;
        goto done;
    }
#else
    err = read_input_frame(af, frame);
    if (err < 0)
        goto done;
    if (err == 0)
        goto done;
    err = -1;
    int ok;
    int lavc_ret = avcodec_encode_audio2(s->lavc_actx, &pkt, frame, &ok);
    av_frame_free(&frame);
    s->input->samples = 0;
    if (lavc_ret < 0 || !ok) {
        MP_FATAL(af, "Encode failed.\n");
        goto done;
    }
#endif

    MP_DBG(af, "avcodec_encode_audio got %d, pending %d.\n",
           pkt.size, s->pending->samples);

    struct mp_audio *out =
        mp_audio_pool_get(af->out_pool, af->data, s->out_samples);
    if (!out)
        goto done;
    mp_audio_copy_attributes(out, s->pending);

    int frame_size = pkt.size;
    int header_len = 0;
    char hdr[8];

    if (s->cfg_add_iec61937_header && pkt.size > 5) {
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

    if (frame_size > out->samples * out->sstride)
        abort();

    char *buf = (char *)out->planes[0];
    memcpy(buf, hdr, header_len);
    memcpy(buf + header_len, pkt.data, pkt.size);
    memset(buf + header_len + pkt.size, 0,
           frame_size - (header_len + pkt.size));
    swap_16((uint16_t *)(buf + header_len), pkt.size / 2);
    out->samples = frame_size / out->sstride;
    af_add_output_frame(af, out);
    update_delay(af);

    err = 0;
done:
    av_packet_unref(&pkt);
    return err;
}

static int af_open(struct af_instance* af){

    af_ac3enc_t *s = af->priv;
    af->control=control;
    af->uninit=uninit;
    af->filter_frame = filter_frame;
    af->filter_out = filter_out;

    s->lavc_acodec = avcodec_find_encoder_by_name(s->cfg_encoder);
    if (!s->lavc_acodec) {
        MP_ERR(af, "Couldn't find encoder %s.\n", s->cfg_encoder);
        return AF_ERROR;
    }

    s->lavc_actx = avcodec_alloc_context3(s->lavc_acodec);
    if (!s->lavc_actx) {
        MP_ERR(af, "Audio LAVC, couldn't allocate context!\n");
        return AF_ERROR;
    }
    // For this one, we require the decoder to expert lists of all supported
    // parameters. (Not all decoders do that, but the ones we're interested
    // in do.)
    if (!s->lavc_acodec->sample_fmts ||
        !s->lavc_acodec->channel_layouts)
    {
        MP_ERR(af, "Audio encoder doesn't list supported parameters.\n");
        return AF_ERROR;
    }

    s->input = talloc_zero(s, struct mp_audio);

    if (s->cfg_bit_rate) {
        int i;
        for (i = 0; i < 19; i++) {
            if (ac3_bitrate_tab[i] == s->cfg_bit_rate) {
                s->bit_rate = ac3_bitrate_tab[i] * 1000;
                break;
            }
        }
        if (i >= 19) {
            MP_WARN(af, "unable set unsupported bitrate %d, use default "
                    "bitrate (check manpage to see supported bitrates).\n",
                    s->cfg_bit_rate);
        }
    }

    return AF_OK;
}

#define OPT_BASE_STRUCT struct af_ac3enc_s

const struct af_info af_info_lavcac3enc = {
    .info = "runtime encode to ac3 using libavcodec",
    .name = "lavcac3enc",
    .open = af_open,
    .priv_size = sizeof(struct af_ac3enc_s),
    .priv_defaults = &(const struct af_ac3enc_s){
        .cfg_add_iec61937_header = 1,
        .cfg_bit_rate = 640,
        .cfg_min_channel_num = 3,
        .cfg_encoder = "ac3",
    },
    .options = (const struct m_option[]) {
        OPT_FLAG("tospdif", cfg_add_iec61937_header, 0),
        OPT_CHOICE_OR_INT("bitrate", cfg_bit_rate, 0, 32, 640,
                          ({"auto", 0}, {"default", 0})),
        OPT_INTRANGE("minch", cfg_min_channel_num, 0, 2, 6),
        OPT_STRING("encoder", cfg_encoder, 0),
        {0}
    },
};
