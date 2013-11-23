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

#include <libavutil/mem.h>

#include "demux/codec_tags.h"

#include "config.h"
#include "mpvcore/codecs.h"
#include "mpvcore/mp_msg.h"
#include "mpvcore/bstr.h"

#include "stream/stream.h"
#include "demux/demux.h"

#include "demux/stheader.h"

#include "dec_audio.h"
#include "ad.h"
#include "audio/format.h"
#include "audio/audio.h"
#include "audio/audio_buffer.h"

#include "audio/filter/af.h"

extern const struct ad_functions ad_mpg123;
extern const struct ad_functions ad_lavc;
extern const struct ad_functions ad_spdif;

static const struct ad_functions * const ad_drivers[] = {
#if HAVE_MPG123
    &ad_mpg123,
#endif
    &ad_lavc,
    &ad_spdif,
    NULL
};

// ad_mpg123 needs to be able to decode 1152 samples at once
// ad_spdif needs up to 8192
#define DECODE_MAX_UNIT MPMAX(8192, 1152)

// At least 8192 samples, plus hack for ad_mpg123 and ad_spdif
#define DECODE_BUFFER_SAMPLES (8192 + DECODE_MAX_UNIT)

// Drop audio buffer and reinit it (after format change)
static void reinit_audio_buffer(struct dec_audio *da)
{
    struct sh_audio *sh = da->header->audio;
    mp_audio_buffer_reinit_fmt(da->decode_buffer, sh->sample_format,
                               &sh->channels, sh->samplerate);
    mp_audio_buffer_preallocate_min(da->decode_buffer, DECODE_BUFFER_SAMPLES);
}

static void uninit_decoder(struct dec_audio *d_audio)
{
    if (d_audio->initialized) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_V, "Uninit audio decoder.\n");
        d_audio->ad_driver->uninit(d_audio);
        d_audio->initialized = 0;
    }
    talloc_free(d_audio->priv);
    d_audio->priv = NULL;
}

static int init_audio_codec(struct dec_audio *d_audio, const char *decoder)
{
    assert(!d_audio->initialized);
    audio_resync_stream(d_audio);
    if (!d_audio->ad_driver->preinit(d_audio)) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_ERR, "Audio decoder preinit failed.\n");
        return 0;
    }

    if (!d_audio->ad_driver->init(d_audio, decoder)) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_V, "Audio decoder init failed.\n");
        uninit_decoder(d_audio);
        return 0;
    }

    d_audio->initialized = 1;

    struct sh_audio *sh = d_audio->header->audio;
    if (mp_chmap_is_empty(&sh->channels) || !sh->samplerate ||
        !sh->sample_format)
    {
        mp_tmsg(MSGT_DECAUDIO, MSGL_ERR, "Audio decoder did not specify "
                "audio format!\n");
        uninit_decoder(d_audio);
        return 0;
    }

    d_audio->decode_buffer = mp_audio_buffer_create(NULL);
    reinit_audio_buffer(d_audio);

    return 1;
}

struct mp_decoder_list *audio_decoder_list(void)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    for (int i = 0; ad_drivers[i] != NULL; i++)
        ad_drivers[i]->add_decoders(list);
    return list;
}

static struct mp_decoder_list *audio_select_decoders(const char *codec,
                                                     char *selection)
{
    struct mp_decoder_list *list = audio_decoder_list();
    struct mp_decoder_list *new = mp_select_decoders(list, codec, selection);
    talloc_free(list);
    return new;
}

static const struct ad_functions *find_driver(const char *name)
{
    for (int i = 0; ad_drivers[i] != NULL; i++) {
        if (strcmp(ad_drivers[i]->name, name) == 0)
            return ad_drivers[i];
    }
    return NULL;
}

int audio_init_best_codec(struct dec_audio *d_audio, char *audio_decoders)
{
    assert(!d_audio->initialized);

    struct mp_decoder_entry *decoder = NULL;
    struct mp_decoder_list *list =
        audio_select_decoders(d_audio->header->codec, audio_decoders);

    mp_print_decoders(MSGT_DECAUDIO, MSGL_V, "Codec list:", list);

    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *sel = &list->entries[n];
        const struct ad_functions *driver = find_driver(sel->family);
        if (!driver)
            continue;
        mp_tmsg(MSGT_DECAUDIO, MSGL_V, "Opening audio decoder %s:%s\n",
                sel->family, sel->decoder);
        d_audio->ad_driver = driver;
        if (init_audio_codec(d_audio, sel->decoder)) {
            decoder = sel;
            break;
        }
        d_audio->ad_driver = NULL;
        mp_tmsg(MSGT_DECAUDIO, MSGL_WARN, "Audio decoder init failed for "
                "%s:%s\n", sel->family, sel->decoder);
    }

    if (d_audio->initialized) {
        d_audio->decoder_desc =
            talloc_asprintf(d_audio, "%s [%s:%s]", decoder->desc, decoder->family,
                            decoder->decoder);
        mp_msg(MSGT_DECAUDIO, MSGL_INFO, "Selected audio codec: %s\n",
               d_audio->decoder_desc);
        mp_msg(MSGT_DECAUDIO, MSGL_V,
               "AUDIO: %d Hz, %d ch, %s\n",
               d_audio->header->audio->samplerate, d_audio->header->audio->channels.num,
               af_fmt_to_str(d_audio->header->audio->sample_format));
        mp_msg(MSGT_IDENTIFY, MSGL_INFO,
               "ID_AUDIO_BITRATE=%d\nID_AUDIO_RATE=%d\n" "ID_AUDIO_NCH=%d\n",
               d_audio->i_bps * 8, d_audio->header->audio->samplerate,
               d_audio->header->audio->channels.num);
    } else {
        mp_msg(MSGT_DECAUDIO, MSGL_ERR,
               "Failed to initialize an audio decoder for codec '%s'.\n",
               d_audio->header->codec ? d_audio->header->codec : "<unknown>");
    }

    talloc_free(list);
    return d_audio->initialized;
}

void audio_uninit(struct dec_audio *d_audio)
{
    if (!d_audio)
        return;
    if (d_audio->afilter) {
        mp_msg(MSGT_DECAUDIO, MSGL_V, "Uninit audio filters...\n");
        af_destroy(d_audio->afilter);
        d_audio->afilter = NULL;
    }
    uninit_decoder(d_audio);
    talloc_free(d_audio->decode_buffer);
    talloc_free(d_audio);
}


int audio_init_filters(struct dec_audio *d_audio, int in_samplerate,
                       int *out_samplerate, struct mp_chmap *out_channels,
                       int *out_format)
{
    if (!d_audio->afilter)
        d_audio->afilter = af_new(d_audio->opts);
    struct af_stream *afs = d_audio->afilter;

    // input format: same as codec's output format:
    mp_audio_buffer_get_format(d_audio->decode_buffer, &afs->input);
    // Sample rate can be different when adjusting playback speed
    afs->input.rate = in_samplerate;

    // output format: same as ao driver's input format (if missing, fallback to input)
    afs->output.rate = *out_samplerate;
    mp_audio_set_channels(&afs->output, out_channels);
    mp_audio_set_format(&afs->output, *out_format);

    char *s_from = mp_audio_config_to_str(&afs->input);
    char *s_to = mp_audio_config_to_str(&afs->output);
    mp_tmsg(MSGT_DECAUDIO, MSGL_V,
            "Building audio filter chain for %s -> %s...\n", s_from, s_to);
    talloc_free(s_from);
    talloc_free(s_to);

    // let's autoprobe it!
    if (af_init(afs) != 0) {
        af_destroy(afs);
        d_audio->afilter = NULL;
        return 0;   // failed :(
    }

    *out_samplerate = afs->output.rate;
    *out_channels = afs->output.channels;
    *out_format = afs->output.format;

    return 1;
}

// Filter len bytes of input, put result into outbuf.
static int filter_n_bytes(struct dec_audio *da, struct mp_audio_buffer *outbuf,
                          int len)
{
    int error = 0;

    struct mp_audio config;
    mp_audio_buffer_get_format(da->decode_buffer, &config);

    while (mp_audio_buffer_samples(da->decode_buffer) < len) {
        int maxlen = mp_audio_buffer_get_write_available(da->decode_buffer);
        if (maxlen < DECODE_MAX_UNIT)
            break;
        struct mp_audio buffer;
        mp_audio_buffer_get_write_buffer(da->decode_buffer, maxlen, &buffer);
        buffer.samples = 0;
        error = da->ad_driver->decode_audio(da, &buffer, maxlen);
        if (error < 0)
            break;
        // Commit the data just read as valid data
        mp_audio_buffer_finish_write(da->decode_buffer, buffer.samples);
        // Format change
        struct sh_audio *sh = da->header->audio;
        if (sh->samplerate != config.rate ||
            !mp_chmap_equals(&sh->channels, &config.channels) ||
            sh->sample_format != config.format)
        {
            // If there are still samples left in the buffer, let them drain
            // first, and don't signal a format change to the caller yet.
            if (mp_audio_buffer_samples(da->decode_buffer) > 0)
                break;
            error = -2;
            break;
        }
    }

    // Filter
    struct mp_audio filter_input;
    mp_audio_buffer_peek(da->decode_buffer, &filter_input);
    filter_input.rate = da->afilter->input.rate; // due to playback speed change
    len = MPMIN(filter_input.samples, len);
    filter_input.samples = len;

    struct mp_audio *filter_output = af_play(da->afilter, &filter_input);
    if (!filter_output)
        return -1;
    mp_audio_buffer_append(outbuf, filter_output);

    // remove processed data from decoder buffer:
    mp_audio_buffer_skip(da->decode_buffer, len);

    // Assume the filter chain is drained from old data at this point.
    // (If not, the remaining old data is discarded.)
    if (error == -2)
        reinit_audio_buffer(da);

    return error;
}

/* Try to get at least minsamples decoded+filtered samples in outbuf
 * (total length including possible existing data).
 * Return 0 on success, -1 on error/EOF (not distinguidaed).
 * In the former case outbuf has at least minsamples buffered on return.
 * In case of EOF/error it might or might not be. */
int audio_decode(struct dec_audio *d_audio, struct mp_audio_buffer *outbuf,
                 int minsamples)
{
    // Indicates that a filter seems to be buffering large amounts of data
    int huge_filter_buffer = 0;
    // Decoded audio must be cut at boundaries of this many samples
    // (Note: the reason for this is unknown, possibly a refactoring artifact)
    int unitsize = 16;

    /* Filter output size will be about filter_multiplier times input size.
     * If some filter buffers audio in big blocks this might only hold
     * as average over time. */
    double filter_multiplier = af_calc_filter_multiplier(d_audio->afilter);

    int prev_buffered = -1;
    while (minsamples >= 0) {
        int buffered = mp_audio_buffer_samples(outbuf);
        if (minsamples < buffered || buffered == prev_buffered)
            break;
        prev_buffered = buffered;

        int decsamples = (minsamples - buffered) / filter_multiplier;
        // + some extra for possible filter buffering
        decsamples += unitsize << 5;

        if (huge_filter_buffer) {
            /* Some filter must be doing significant buffering if the estimated
             * input length didn't produce enough output from filters.
             * Feed the filters 250 samples at a time until we have enough
             * output. Very small amounts could make filtering inefficient while
             * large amounts can make mpv demux the file unnecessarily far ahead
             * to get audio data and buffer video frames in memory while doing
             * so. However the performance impact of either is probably not too
             * significant as long as the value is not completely insane. */
            decsamples = 250;
        }

        /* if this iteration does not fill buffer, we must have lots
         * of buffering in filters */
        huge_filter_buffer = 1;

        int res = filter_n_bytes(d_audio, outbuf, decsamples);
        if (res < 0)
            return res;
    }
    return 0;
}

void audio_resync_stream(struct dec_audio *d_audio)
{
    d_audio->pts = MP_NOPTS_VALUE;
    d_audio->pts_offset = 0;
    if (!d_audio->initialized)
        return;
    d_audio->ad_driver->control(d_audio, ADCTRL_RESYNC_STREAM, NULL);
}
