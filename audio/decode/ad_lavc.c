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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "talloc.h"

#include "config.h"
#include "core/av_common.h"
#include "core/codecs.h"
#include "core/mp_msg.h"
#include "core/options.h"
#include "core/av_opts.h"

#include "ad_internal.h"
#include "audio/reorder_ch.h"
#include "audio/fmt-conversion.h"

#include "compat/mpbswap.h"
#include "compat/libav.h"

LIBAD_EXTERN(lavc)

struct priv {
    AVCodecContext *avctx;
    AVFrame *avframe;
    uint8_t *output;
    uint8_t *output_packed; // used by deplanarize to store packed audio samples
    int output_left;
    int unitsize;
    int previous_data_left;  // input demuxer packet data
    bool force_channel_map;
};

#define OPT_BASE_STRUCT struct MPOpts

const m_option_t ad_lavc_decode_opts_conf[] = {
    OPT_FLOATRANGE("ac3drc", ad_lavc_param.ac3drc, 0, 0, 2),
    OPT_FLAG("downmix", ad_lavc_param.downmix, 0),
    OPT_STRING("o", ad_lavc_param.avopt, 0),
    {0}
};

struct pcm_map
{
    int tag;
    const char *codecs[5]; // {any, 1byte, 2bytes, 3bytes, 4bytes}
};

// NOTE: some of these are needed to make rawaudio with demux_mkv and others
//       work. ffmpeg does similar mapping internally, not part of the public
//       API. Some of these might be dead leftovers for demux_mov support.
static const struct pcm_map tag_map[] = {
    // Microsoft PCM
    {0x0,           {NULL, "pcm_u8", "pcm_s16le", "pcm_s24le", "pcm_s32le"}},
    {0x1,           {NULL, "pcm_u8", "pcm_s16le", "pcm_s24le", "pcm_s32le"}},
    // MS PCM, Extended
    {0xfffe,        {NULL, "pcm_u8", "pcm_s16le", "pcm_s24le", "pcm_s32le"}},
    // IEEE float
    {0x3,           {"pcm_f32le"}},
    // 'raw '
    {0x20776172,    {"pcm_s16be", [1] = "pcm_u8"}},
    // 'twos'/'sowt'
    {0x736F7774,    {"pcm_s16be", [1] = "pcm_s8"}},
    {0x74776F73,    {"pcm_s16be", [1] = "pcm_s8"}},
    // 'fl32'/'FL32'
    {0x32336c66,    {"pcm_f32be"}},
    {0x32334C46,    {"pcm_f32be"}},
    // '23lf'/'lpcm'
    {0x666c3332,    {"pcm_f32le"}},
    {0x6D63706C,    {"pcm_f32le"}},
    // 'in24', bigendian int24
    {0x34326e69,    {"pcm_s24be"}},
    // '42ni', little endian int24, MPlayer internal fourCC
    {0x696e3234,    {"pcm_s24le"}},
    // 'in32', bigendian int32
    {0x32336e69,    {"pcm_s32be"}},
    // '23ni', little endian int32, MPlayer internal fourCC
    {0x696e3332,    {"pcm_s32le"}},
    {-1},
};

// For demux_rawaudio.c; needed because ffmpeg doesn't have these sample
// formats natively.
static const struct pcm_map af_map[] = {
    {AF_FORMAT_U8,              {"pcm_u8"}},
    {AF_FORMAT_S8,              {"pcm_u8"}},
    {AF_FORMAT_U16_LE,          {"pcm_u16le"}},
    {AF_FORMAT_U16_BE,          {"pcm_u16be"}},
    {AF_FORMAT_S16_LE,          {"pcm_s16le"}},
    {AF_FORMAT_S16_BE,          {"pcm_s16be"}},
    {AF_FORMAT_U24_LE,          {"pcm_u24le"}},
    {AF_FORMAT_U24_BE,          {"pcm_u24be"}},
    {AF_FORMAT_S24_LE,          {"pcm_s24le"}},
    {AF_FORMAT_S24_BE,          {"pcm_s24be"}},
    {AF_FORMAT_U32_LE,          {"pcm_u32le"}},
    {AF_FORMAT_U32_BE,          {"pcm_u32be"}},
    {AF_FORMAT_S32_LE,          {"pcm_s32le"}},
    {AF_FORMAT_S32_BE,          {"pcm_s32be"}},
    {AF_FORMAT_FLOAT_LE,        {"pcm_f32le"}},
    {AF_FORMAT_FLOAT_BE,        {"pcm_f32be"}},
    {-1},
};

static const char *find_pcm_decoder(const struct pcm_map *map, int format,
                                    int bits_per_sample)
{
    int bytes = (bits_per_sample + 7) / 8;
    for (int n = 0; map[n].tag != -1; n++) {
        const struct pcm_map *entry = &map[n];
        if (entry->tag == format) {
            const char *dec = NULL;
            if (bytes >= 1 && bytes <= 4)
                dec = entry->codecs[bytes];
            if (!dec)
                dec = entry->codecs[0];
            if (dec)
                return dec;
        }
    }
    return NULL;
}

static int preinit(sh_audio_t *sh)
{
    return 1;
}

/* Prefer playing audio with the samplerate given in container data
 * if available, but take number the number of channels and sample format
 * from the codec, since if the codec isn't using the correct values for
 * those everything breaks anyway.
 */
static int setup_format(sh_audio_t *sh_audio,
                        const AVCodecContext *lavc_context)
{
    struct priv *priv = sh_audio->context;
    int sample_format        =
        af_from_avformat(av_get_packed_sample_fmt(lavc_context->sample_fmt));
    bool broken_srate        = false;
    int samplerate           = lavc_context->sample_rate;
    int container_samplerate = sh_audio->container_out_samplerate;
    if (!container_samplerate && sh_audio->wf)
        container_samplerate = sh_audio->wf->nSamplesPerSec;
    if (lavc_context->codec_id == AV_CODEC_ID_AAC
        && samplerate == 2 * container_samplerate)
        broken_srate = true;
    else if (container_samplerate)
        samplerate = container_samplerate;

    struct mp_chmap lavc_chmap;
    mp_chmap_from_lavc(&lavc_chmap, lavc_context->channel_layout);
    // No channel layout or layout disagrees with channel count
    if (lavc_chmap.num != lavc_context->channels)
        mp_chmap_from_channels(&lavc_chmap, lavc_context->channels);
    if (priv->force_channel_map) {
        if (lavc_chmap.num == sh_audio->channels.num)
            lavc_chmap = sh_audio->channels;
    }

    if (!mp_chmap_equals(&lavc_chmap, &sh_audio->channels) ||
        samplerate != sh_audio->samplerate ||
        sample_format != sh_audio->sample_format) {
        sh_audio->channels = lavc_chmap;
        sh_audio->samplerate = samplerate;
        sh_audio->sample_format = sample_format;
        sh_audio->samplesize = af_fmt2bits(sh_audio->sample_format) / 8;
        if (broken_srate)
            mp_msg(MSGT_DECAUDIO, MSGL_WARN,
                   "Ignoring broken container sample rate for AAC with SBR\n");
        return 1;
    }
    return 0;
}

static void set_from_wf(AVCodecContext *avctx, WAVEFORMATEX *wf)
{
    avctx->channels = wf->nChannels;
    avctx->sample_rate = wf->nSamplesPerSec;
    avctx->bit_rate = wf->nAvgBytesPerSec * 8;
    avctx->block_align = wf->nBlockAlign;
    avctx->bits_per_coded_sample = wf->wBitsPerSample;

    if (wf->cbSize > 0) {
        avctx->extradata = av_mallocz(wf->cbSize + FF_INPUT_BUFFER_PADDING_SIZE);
        avctx->extradata_size = wf->cbSize;
        memcpy(avctx->extradata, wf + 1, avctx->extradata_size);
    }
}

static int init(sh_audio_t *sh_audio, const char *decoder)
{
    struct MPOpts *mpopts = sh_audio->opts;
    struct ad_lavc_param *opts = &mpopts->ad_lavc_param;
    AVCodecContext *lavc_context;
    AVCodec *lavc_codec;

    struct priv *ctx = talloc_zero(NULL, struct priv);
    sh_audio->context = ctx;

    if (sh_audio->wf && strcmp(decoder, "pcm") == 0) {
        decoder = find_pcm_decoder(tag_map, sh_audio->format,
                                   sh_audio->wf->wBitsPerSample);
    } else if (sh_audio->wf && strcmp(decoder, "mp-pcm") == 0) {
        decoder = find_pcm_decoder(af_map, sh_audio->format, 0);
        ctx->force_channel_map = true;
    }

    lavc_codec = avcodec_find_decoder_by_name(decoder);
    if (!lavc_codec) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_ERR,
                "Cannot find codec '%s' in libavcodec...\n", decoder);
        uninit(sh_audio);
        return 0;
    }

    lavc_context = avcodec_alloc_context3(lavc_codec);
    ctx->avctx = lavc_context;
    ctx->avframe = avcodec_alloc_frame();
    lavc_context->codec_type = AVMEDIA_TYPE_AUDIO;
    lavc_context->codec_id = lavc_codec->id;

    if (opts->downmix) {
        lavc_context->request_channels = mpopts->audio_output_channels.num;
        lavc_context->request_channel_layout =
            mp_chmap_to_lavc(&mpopts->audio_output_channels);
    }

    // Always try to set - option only exists for AC3 at the moment
    av_opt_set_double(lavc_context, "drc_scale", opts->ac3drc,
                      AV_OPT_SEARCH_CHILDREN);

    if (opts->avopt) {
        if (parse_avopts(lavc_context, opts->avopt) < 0) {
            mp_msg(MSGT_DECVIDEO, MSGL_ERR,
                   "ad_lavc: setting AVOptions '%s' failed.\n", opts->avopt);
            uninit(sh_audio);
            return 0;
        }
    }

    lavc_context->codec_tag = sh_audio->format;
    lavc_context->sample_rate = sh_audio->samplerate;
    lavc_context->bit_rate = sh_audio->i_bps * 8;
    lavc_context->channel_layout = mp_chmap_to_lavc(&sh_audio->channels);

    if (sh_audio->wf)
        set_from_wf(lavc_context, sh_audio->wf);

    // demux_mkv, demux_mpg
    if (sh_audio->codecdata_len && sh_audio->codecdata &&
            !lavc_context->extradata) {
        lavc_context->extradata = av_malloc(sh_audio->codecdata_len +
                                            FF_INPUT_BUFFER_PADDING_SIZE);
        lavc_context->extradata_size = sh_audio->codecdata_len;
        memcpy(lavc_context->extradata, (char *)sh_audio->codecdata,
               lavc_context->extradata_size);
    }

    if (sh_audio->gsh->lav_headers)
        mp_copy_lav_codec_headers(lavc_context, sh_audio->gsh->lav_headers);

    /* open it */
    if (avcodec_open2(lavc_context, lavc_codec, NULL) < 0) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_ERR, "Could not open codec.\n");
        uninit(sh_audio);
        return 0;
    }
    mp_msg(MSGT_DECAUDIO, MSGL_V, "INFO: libavcodec \"%s\" init OK!\n",
           lavc_codec->name);

    // Decode at least 1 byte:  (to get header filled)
    for (int tries = 0;;) {
        int x = decode_audio(sh_audio, sh_audio->a_buffer, 1,
                             sh_audio->a_buffer_size);
        if (x > 0) {
            sh_audio->a_buffer_len = x;
            break;
        }
        if (++tries >= 5) {
            mp_msg(MSGT_DECAUDIO, MSGL_ERR,
                   "ad_lavc: initial decode failed\n");
            uninit(sh_audio);
            return 0;
        }
    }

    sh_audio->i_bps = lavc_context->bit_rate / 8;
    if (sh_audio->wf && sh_audio->wf->nAvgBytesPerSec)
        sh_audio->i_bps = sh_audio->wf->nAvgBytesPerSec;

    int af_sample_fmt =
        af_from_avformat(av_get_packed_sample_fmt(lavc_context->sample_fmt));
    if (af_sample_fmt == AF_FORMAT_UNKNOWN) {
        uninit(sh_audio);
        return 0;
    }
    return 1;
}

static void uninit(sh_audio_t *sh)
{
    struct priv *ctx = sh->context;
    if (!ctx)
        return;
    AVCodecContext *lavc_context = ctx->avctx;

    if (lavc_context) {
        if (avcodec_close(lavc_context) < 0)
            mp_tmsg(MSGT_DECVIDEO, MSGL_ERR, "Could not close codec.\n");
        av_freep(&lavc_context->extradata);
        av_freep(&lavc_context);
    }
    avcodec_free_frame(&ctx->avframe);
    talloc_free(ctx);
    sh->context = NULL;
}

static int control(sh_audio_t *sh, int cmd, void *arg, ...)
{
    struct priv *ctx = sh->context;
    switch (cmd) {
    case ADCTRL_RESYNC_STREAM:
        avcodec_flush_buffers(ctx->avctx);
        ds_clear_parser(sh->ds);
        ctx->previous_data_left = 0;
        ctx->output_left = 0;
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static av_always_inline void deplanarize(struct sh_audio *sh)
{
    struct priv *priv = sh->context;

    uint8_t **planes  = priv->avframe->extended_data;
    size_t bps        = av_get_bytes_per_sample(priv->avctx->sample_fmt);
    size_t nb_samples = priv->avframe->nb_samples;
    size_t channels   = priv->avctx->channels;
    size_t size       = bps * nb_samples * channels;

    if (talloc_get_size(priv->output_packed) != size)
        priv->output_packed =
            talloc_realloc_size(priv, priv->output_packed, size);

    reorder_to_packed(priv->output_packed, planes, bps, channels, nb_samples);

    priv->output = priv->output_packed;
}

static int decode_new_packet(struct sh_audio *sh)
{
    struct priv *priv = sh->context;
    AVCodecContext *avctx = priv->avctx;
    double pts = MP_NOPTS_VALUE;
    int insize;
    bool packet_already_used = priv->previous_data_left;
    struct demux_packet *mpkt = ds_get_packet2(sh->ds,
                                               priv->previous_data_left);
    unsigned char *start;
    if (!mpkt) {
        assert(!priv->previous_data_left);
        start = NULL;
        insize = 0;
        ds_parse(sh->ds, &start, &insize, pts, 0);
        if (insize <= 0)
            return -1;  // error or EOF
    } else {
        assert(mpkt->len >= priv->previous_data_left);
        if (!priv->previous_data_left) {
            priv->previous_data_left = mpkt->len;
            pts = mpkt->pts;
        }
        insize = priv->previous_data_left;
        start = mpkt->buffer + mpkt->len - priv->previous_data_left;
        int consumed = ds_parse(sh->ds, &start, &insize, pts, 0);
        priv->previous_data_left -= consumed;
        priv->previous_data_left = FFMAX(priv->previous_data_left, 0);
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = start;
    pkt.size = insize;
    if (mpkt && mpkt->avpacket) {
        pkt.side_data = mpkt->avpacket->side_data;
        pkt.side_data_elems = mpkt->avpacket->side_data_elems;
    }
    if (pts != MP_NOPTS_VALUE && !packet_already_used) {
        sh->pts = pts;
        sh->pts_bytes = 0;
    }
    int got_frame = 0;
    int ret = avcodec_decode_audio4(avctx, priv->avframe, &got_frame, &pkt);
    // LATM may need many packets to find mux info
    if (ret == AVERROR(EAGAIN))
        return 0;
    if (ret < 0) {
        mp_msg(MSGT_DECAUDIO, MSGL_V, "lavc_audio: error\n");
        return -1;
    }
    // The "insize >= ret" test is sanity check against decoder overreads
    if (!sh->parser && insize >= ret)
        priv->previous_data_left = insize - ret;
    if (!got_frame)
        return 0;
    uint64_t unitsize = (uint64_t)av_get_bytes_per_sample(avctx->sample_fmt) *
                        avctx->channels;
    if (unitsize > 100000)
        abort();
    priv->unitsize = unitsize;
    uint64_t output_left = unitsize * priv->avframe->nb_samples;
    if (output_left > 500000000)
        abort();
    priv->output_left = output_left;
    if (av_sample_fmt_is_planar(avctx->sample_fmt) && avctx->channels > 1) {
        deplanarize(sh);
    } else {
        priv->output = priv->avframe->data[0];
    }
    mp_dbg(MSGT_DECAUDIO, MSGL_DBG2, "Decoded %d -> %d  \n", insize,
           priv->output_left);
    return 0;
}


static int decode_audio(sh_audio_t *sh_audio, unsigned char *buf, int minlen,
                        int maxlen)
{
    struct priv *priv = sh_audio->context;
    AVCodecContext *avctx = priv->avctx;

    int len = -1;
    while (len < minlen) {
        if (!priv->output_left) {
            if (decode_new_packet(sh_audio) < 0)
                break;
            continue;
        }
        if (setup_format(sh_audio, avctx))
            return len;
        int size = (minlen - len + priv->unitsize - 1);
        size -= size % priv->unitsize;
        size = FFMIN(size, priv->output_left);
        if (size > maxlen)
            abort();
        memcpy(buf, priv->output, size);
        priv->output += size;
        priv->output_left -= size;
        if (len < 0)
            len = size;
        else
            len += size;
        buf += size;
        maxlen -= size;
        sh_audio->pts_bytes += size;
    }
    return len;
}

static void add_decoders(struct mp_decoder_list *list)
{
    mp_add_lavc_decoders(list, AVMEDIA_TYPE_AUDIO);
    mp_add_decoder(list, "lavc", "pcm", "pcm", "Raw PCM");
    mp_add_decoder(list, "lavc", "mp-pcm", "mp-pcm", "Raw PCM");
}
