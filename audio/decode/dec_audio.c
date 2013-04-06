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
#include <assert.h>

#include "demux/codec_tags.h"

#include "config.h"
#include "core/codecs.h"
#include "core/mp_msg.h"
#include "core/bstr.h"

#include "stream/stream.h"
#include "demux/demux.h"

#include "demux/stheader.h"

#include "dec_audio.h"
#include "ad.h"
#include "audio/format.h"

#include "audio/filter/af.h"

int fakemono = 0;

struct af_cfg af_cfg = {0}; // Configuration for audio filters

static int init_audio_codec(sh_audio_t *sh_audio, const char *decoder)
{
    assert(!sh_audio->initialized);
    resync_audio_stream(sh_audio);
    sh_audio->samplesize = 4;
    sh_audio->sample_format = AF_FORMAT_FLOAT_NE;
    sh_audio->audio_out_minsize = 8192; // default, preinit() may change it
    if (!sh_audio->ad_driver->preinit(sh_audio)) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_ERR, "Audio decoder preinit failed.\n");
        return 0;
    }

    /* allocate audio in buffer: */
    if (sh_audio->audio_in_minsize > 0) {
        sh_audio->a_in_buffer_size = sh_audio->audio_in_minsize;
        mp_tmsg(MSGT_DECAUDIO, MSGL_V,
                "dec_audio: Allocating %d bytes for input buffer.\n",
                sh_audio->a_in_buffer_size);
        sh_audio->a_in_buffer = av_mallocz(sh_audio->a_in_buffer_size);
    }

    const int base_size = 65536;
    // At least 64 KiB plus rounding up to next decodable unit size
    sh_audio->a_buffer_size = base_size + sh_audio->audio_out_minsize;

    mp_tmsg(MSGT_DECAUDIO, MSGL_V,
            "dec_audio: Allocating %d + %d = %d bytes for output buffer.\n",
            sh_audio->audio_out_minsize, base_size,
            sh_audio->a_buffer_size);

    sh_audio->a_buffer = av_mallocz(sh_audio->a_buffer_size);
    if (!sh_audio->a_buffer)
        abort();
    sh_audio->a_buffer_len = 0;

    if (!sh_audio->ad_driver->init(sh_audio, decoder)) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_V, "Audio decoder init failed.\n");
        uninit_audio(sh_audio); // free buffers
        return 0;
    }

    sh_audio->initialized = 1;

    if (mp_chmap_is_empty(&sh_audio->channels) || !sh_audio->samplerate) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_ERR, "Audio decoder did not specify "
                "audio format!\n");
        uninit_audio(sh_audio); // free buffers
        return 0;
    }

    if (!sh_audio->o_bps)
        sh_audio->o_bps = sh_audio->channels.num * sh_audio->samplerate
                          * sh_audio->samplesize;
    return 1;
}

struct mp_decoder_list *mp_audio_decoder_list(void)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    for (int i = 0; mpcodecs_ad_drivers[i] != NULL; i++)
        mpcodecs_ad_drivers[i]->add_decoders(list);
    return list;
}

static struct mp_decoder_list *mp_select_audio_decoders(const char *codec,
                                                        char *selection)
{
    struct mp_decoder_list *list = mp_audio_decoder_list();
    struct mp_decoder_list *new = mp_select_decoders(list, codec, selection);
    talloc_free(list);
    return new;
}

static const struct ad_functions *find_driver(const char *name)
{
    for (int i = 0; mpcodecs_ad_drivers[i] != NULL; i++) {
        if (strcmp(mpcodecs_ad_drivers[i]->name, name) == 0)
            return mpcodecs_ad_drivers[i];
    }
    return NULL;
}

int init_best_audio_codec(sh_audio_t *sh_audio, char *audio_decoders)
{
    assert(!sh_audio->initialized);

    struct mp_decoder_entry *decoder = NULL;
    struct mp_decoder_list *list =
        mp_select_audio_decoders(sh_audio->gsh->codec, audio_decoders);

    mp_print_decoders(MSGT_DECAUDIO, MSGL_V, "Codec list:", list);

    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *sel = &list->entries[n];
        const struct ad_functions *driver = find_driver(sel->family);
        if (!driver)
            continue;
        mp_tmsg(MSGT_DECAUDIO, MSGL_V, "Opening audio decoder %s:%s\n",
                sel->family, sel->decoder);
        sh_audio->ad_driver = driver;
        if (init_audio_codec(sh_audio, sel->decoder)) {
            decoder = sel;
            break;
        }
        sh_audio->ad_driver = NULL;
        mp_tmsg(MSGT_DECAUDIO, MSGL_WARN, "Audio decoder init failed for "
                "%s:%s\n", sel->family, sel->decoder);
    }

    if (sh_audio->initialized) {
        sh_audio->gsh->decoder_desc =
            talloc_asprintf(NULL, "%s [%s:%s]", decoder->desc, decoder->family,
                            decoder->decoder);
        mp_msg(MSGT_DECAUDIO, MSGL_INFO, "Selected audio codec: %s\n",
               sh_audio->gsh->decoder_desc);
        mp_msg(MSGT_DECAUDIO, MSGL_V,
               "AUDIO: %d Hz, %d ch, %s, %3.1f kbit/%3.2f%% (ratio: %d->%d)\n",
               sh_audio->samplerate, sh_audio->channels.num,
               af_fmt2str_short(sh_audio->sample_format),
               sh_audio->i_bps * 8 * 0.001,
               ((float) sh_audio->i_bps / sh_audio->o_bps) * 100.0,
               sh_audio->i_bps, sh_audio->o_bps);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_AUDIO_BITRATE=%d\nID_AUDIO_RATE=%d\n" "ID_AUDIO_NCH=%d\n",
               sh_audio->i_bps * 8, sh_audio->samplerate, sh_audio->channels.num);
    } else {
        mp_msg(MSGT_DECAUDIO, MSGL_ERR,
               "Failed to initialize an audio decoder for codec '%s'.\n",
               sh_audio->gsh->codec ? sh_audio->gsh->codec : "<unknown>");
    }

    talloc_free(list);
    return sh_audio->initialized;
}

void uninit_audio(sh_audio_t *sh_audio)
{
    if (sh_audio->afilter) {
        mp_msg(MSGT_DECAUDIO, MSGL_V, "Uninit audio filters...\n");
        af_uninit(sh_audio->afilter);
        af_destroy(sh_audio->afilter);
        sh_audio->afilter = NULL;
    }
    if (sh_audio->initialized) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_V, "Uninit audio.\n");
        sh_audio->ad_driver->uninit(sh_audio);
        sh_audio->initialized = 0;
    }
    talloc_free(sh_audio->gsh->decoder_desc);
    sh_audio->gsh->decoder_desc = NULL;
    av_freep(&sh_audio->a_buffer);
    av_freep(&sh_audio->a_in_buffer);
}


int init_audio_filters(sh_audio_t *sh_audio, int in_samplerate,
                       int *out_samplerate, struct mp_chmap *out_channels,
                       int *out_format)
{
    struct af_stream *afs = sh_audio->afilter;
    if (!afs)
        afs = af_new(sh_audio->opts);
    // input format: same as codec's output format:
    afs->input.rate = in_samplerate;
    mp_audio_set_channels(&afs->input, &sh_audio->channels);
    mp_audio_set_format(&afs->input, sh_audio->sample_format);

    // output format: same as ao driver's input format (if missing, fallback to input)
    afs->output.rate = *out_samplerate;
    mp_audio_set_channels(&afs->output, out_channels);
    mp_audio_set_format(&afs->output, *out_format);

    // filter config:
    memcpy(&afs->cfg, &af_cfg, sizeof(struct af_cfg));

    mp_tmsg(MSGT_DECAUDIO, MSGL_V,
            "Building audio filter chain for %dHz/%dch/%s -> %dHz/%dch/%s...\n",
            afs->input.rate, afs->input.nch,
            af_fmt2str_short(afs->input.format), afs->output.rate,
            afs->output.nch, af_fmt2str_short(afs->output.format));

    // let's autoprobe it!
    if (0 != af_init(afs)) {
        sh_audio->afilter = NULL;
        af_destroy(afs);
        return 0;   // failed :(
    }

    *out_samplerate = afs->output.rate;
    *out_channels = afs->output.channels;
    *out_format = afs->output.format;

    // ok!
    sh_audio->afilter = (void *) afs;
    return 1;
}

static void set_min_out_buffer_size(struct bstr *outbuf, int len)
{
    size_t oldlen = talloc_get_size(outbuf->start);
    if (oldlen < len) {
        assert(outbuf->start);  // talloc context should be already set
        mp_msg(MSGT_DECAUDIO, MSGL_V, "Increasing filtered audio buffer size "
               "from %zd to %d\n", oldlen, len);
        outbuf->start = talloc_realloc_size(NULL, outbuf->start, len);
    }
}

static int filter_n_bytes(sh_audio_t *sh, struct bstr *outbuf, int len)
{
    assert(len - 1 + sh->audio_out_minsize <= sh->a_buffer_size);

    int error = 0;

    // Decode more bytes if needed
    int old_samplerate = sh->samplerate;
    struct mp_chmap old_channels = sh->channels;
    int old_sample_format = sh->sample_format;
    while (sh->a_buffer_len < len) {
        unsigned char *buf = sh->a_buffer + sh->a_buffer_len;
        int minlen = len - sh->a_buffer_len;
        int maxlen = sh->a_buffer_size - sh->a_buffer_len;
        int ret = sh->ad_driver->decode_audio(sh, buf, minlen, maxlen);
        int format_change = sh->samplerate != old_samplerate
                            || !mp_chmap_equals(&sh->channels, &old_channels)
                            || sh->sample_format != old_sample_format;
        if (ret <= 0 || format_change) {
            error = format_change ? -2 : -1;
            // samples from format-changing call get discarded too
            len = sh->a_buffer_len;
            break;
        }
        sh->a_buffer_len += ret;
    }

    // Filter
    struct mp_audio filter_input = {
        .audio = sh->a_buffer,
        .len = len,
        .rate = sh->samplerate,
    };
    mp_audio_set_format(&filter_input, sh->sample_format);
    mp_audio_set_channels(&filter_input, &sh->channels);

    struct mp_audio *filter_output = af_play(sh->afilter, &filter_input);
    if (!filter_output)
        return -1;
    set_min_out_buffer_size(outbuf, outbuf->len + filter_output->len);
    memcpy(outbuf->start + outbuf->len, filter_output->audio,
           filter_output->len);
    outbuf->len += filter_output->len;

    // remove processed data from decoder buffer:
    sh->a_buffer_len -= len;
    memmove(sh->a_buffer, sh->a_buffer + len, sh->a_buffer_len);

    return error;
}

/* Try to get at least minlen decoded+filtered bytes in outbuf
 * (total length including possible existing data).
 * Return 0 on success, -1 on error/EOF (not distinguished).
 * In the former case outbuf->len is always >= minlen on return.
 * In case of EOF/error it might or might not be.
 * Outbuf.start must be talloc-allocated, and will be reallocated
 * if needed to fit all filter output. */
int decode_audio(sh_audio_t *sh_audio, struct bstr *outbuf, int minlen)
{
    // Indicates that a filter seems to be buffering large amounts of data
    int huge_filter_buffer = 0;
    // Decoded audio must be cut at boundaries of this many bytes
    int unitsize = sh_audio->channels.num * sh_audio->samplesize * 16;

    /* Filter output size will be about filter_multiplier times input size.
     * If some filter buffers audio in big blocks this might only hold
     * as average over time. */
    double filter_multiplier = af_calc_filter_multiplier(sh_audio->afilter);

    /* If the decoder set audio_out_minsize then it can do the equivalent of
     * "while (output_len < target_len) output_len += audio_out_minsize;",
     * so we must guarantee there is at least audio_out_minsize-1 bytes
     * more space in the output buffer than the minimum length we try to
     * decode. */
    int max_decode_len = sh_audio->a_buffer_size - sh_audio->audio_out_minsize;
    if (!unitsize)
        return -1;
    max_decode_len -= max_decode_len % unitsize;

    while (minlen >= 0 && outbuf->len < minlen) {
        // + some extra for possible filter buffering
        int declen = (minlen - outbuf->len) / filter_multiplier + (unitsize << 5); 
        if (huge_filter_buffer)
            /* Some filter must be doing significant buffering if the estimated
             * input length didn't produce enough output from filters.
             * Feed the filters 2k bytes at a time until we have enough output.
             * Very small amounts could make filtering inefficient while large
             * amounts can make MPlayer demux the file unnecessarily far ahead
             * to get audio data and buffer video frames in memory while doing
             * so. However the performance impact of either is probably not too
             * significant as long as the value is not completely insane. */
            declen = 2000;
        declen -= declen % unitsize;
        if (declen > max_decode_len)
            declen = max_decode_len;
        else
            /* if this iteration does not fill buffer, we must have lots
             * of buffering in filters */
            huge_filter_buffer = 1;
        int res = filter_n_bytes(sh_audio, outbuf, declen);
        if (res < 0)
            return res;
    }
    return 0;
}

void decode_audio_prepend_bytes(struct bstr *outbuf, int count, int byte)
{
    set_min_out_buffer_size(outbuf, outbuf->len + count);
    memmove(outbuf->start + count, outbuf->start, outbuf->len);
    memset(outbuf->start, byte, count);
    outbuf->len += count;
}


void resync_audio_stream(sh_audio_t *sh_audio)
{
    sh_audio->a_in_buffer_len = 0;      // clear audio input buffer
    sh_audio->pts = MP_NOPTS_VALUE;
    if (!sh_audio->initialized)
        return;
    sh_audio->ad_driver->control(sh_audio, ADCTRL_RESYNC_STREAM, NULL);
}

void skip_audio_frame(sh_audio_t *sh_audio)
{
    if (!sh_audio->initialized)
        return;
    if (sh_audio->ad_driver->control(sh_audio, ADCTRL_SKIP_FRAME, NULL)
        == CONTROL_TRUE)
        return;
    // default skip code:
    ds_fill_buffer(sh_audio->ds);       // skip block
}
