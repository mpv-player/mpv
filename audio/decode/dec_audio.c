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

// At least ad_mpg123 needs to be able to decode this many samples at once
#define DECODE_MAX_UNIT 1152

// At least 8192 samples, plus hack for ad_mpg123
#define DECODE_BUFFER_SAMPLES (8192 + DECODE_MAX_UNIT)

// Drop audio buffer and reinit it (after format change)
static void reinit_audio_buffer(sh_audio_t *sh)
{
    mp_audio_buffer_reinit_fmt(sh->decode_buffer, sh->sample_format,
                               &sh->channels, sh->samplerate);
    mp_audio_buffer_preallocate_min(sh->decode_buffer, DECODE_BUFFER_SAMPLES);
}

static int init_audio_codec(sh_audio_t *sh_audio, const char *decoder)
{
    assert(!sh_audio->initialized);
    resync_audio_stream(sh_audio);
    if (!sh_audio->ad_driver->preinit(sh_audio)) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_ERR, "Audio decoder preinit failed.\n");
        return 0;
    }

    if (!sh_audio->ad_driver->init(sh_audio, decoder)) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_V, "Audio decoder init failed.\n");
        uninit_audio(sh_audio); // free buffers
        return 0;
    }

    sh_audio->initialized = 1;

    if (mp_chmap_is_empty(&sh_audio->channels) || !sh_audio->samplerate ||
        !sh_audio->sample_format)
    {
        mp_tmsg(MSGT_DECAUDIO, MSGL_ERR, "Audio decoder did not specify "
                "audio format!\n");
        uninit_audio(sh_audio); // free buffers
        return 0;
    }

    sh_audio->decode_buffer = mp_audio_buffer_create(NULL);
    reinit_audio_buffer(sh_audio);

    return 1;
}

struct mp_decoder_list *mp_audio_decoder_list(void)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    for (int i = 0; ad_drivers[i] != NULL; i++)
        ad_drivers[i]->add_decoders(list);
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
    for (int i = 0; ad_drivers[i] != NULL; i++) {
        if (strcmp(ad_drivers[i]->name, name) == 0)
            return ad_drivers[i];
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
               "AUDIO: %d Hz, %d ch, %s\n",
               sh_audio->samplerate, sh_audio->channels.num,
               af_fmt_to_str(sh_audio->sample_format));
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
    talloc_free(sh_audio->decode_buffer);
    sh_audio->decode_buffer = NULL;
}


int init_audio_filters(sh_audio_t *sh_audio, int in_samplerate,
                       int *out_samplerate, struct mp_chmap *out_channels,
                       int *out_format)
{
    if (!sh_audio->afilter)
        sh_audio->afilter = af_new(sh_audio->opts);
    struct af_stream *afs = sh_audio->afilter;

    // input format: same as codec's output format:
    afs->input.rate = in_samplerate;
    mp_audio_set_channels(&afs->input, &sh_audio->channels);
    mp_audio_set_format(&afs->input, sh_audio->sample_format);

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
        sh_audio->afilter = NULL;
        return 0;   // failed :(
    }

    *out_samplerate = afs->output.rate;
    *out_channels = afs->output.channels;
    *out_format = afs->output.format;

    return 1;
}

// Filter len bytes of input, put result into outbuf.
static int filter_n_bytes(sh_audio_t *sh, struct mp_audio_buffer *outbuf,
                          int len)
{
    int error = 0;

    struct mp_audio config;
    mp_audio_buffer_get_format(sh->decode_buffer, &config);

    while (mp_audio_buffer_samples(sh->decode_buffer) < len) {
        int maxlen = mp_audio_buffer_get_write_available(sh->decode_buffer);
        if (maxlen < DECODE_MAX_UNIT)
            break;
        struct mp_audio buffer;
        mp_audio_buffer_get_write_buffer(sh->decode_buffer, maxlen, &buffer);
        buffer.samples = 0;
        error = sh->ad_driver->decode_audio(sh, &buffer, maxlen);
        if (error < 0)
            break;
        // Commit the data just read as valid data
        mp_audio_buffer_finish_write(sh->decode_buffer, buffer.samples);
        // Format change
        if (sh->samplerate != config.rate ||
            !mp_chmap_equals(&sh->channels, &config.channels) ||
            sh->sample_format != config.format)
        {
            // If there are still samples left in the buffer, let them drain
            // first, and don't signal a format change to the caller yet.
            if (mp_audio_buffer_samples(sh->decode_buffer) > 0)
                break;
            reinit_audio_buffer(sh);
            error = -2;
            break;
        }
    }

    // Filter
    struct mp_audio filter_input;
    mp_audio_buffer_peek(sh->decode_buffer, &filter_input);
    filter_input.rate = sh->afilter->input.rate; // due to playback speed change
    len = MPMIN(filter_input.samples, len);
    filter_input.samples = len;

    struct mp_audio *filter_output = af_play(sh->afilter, &filter_input);
    if (!filter_output)
        return -1;
    mp_audio_buffer_append(outbuf, filter_output);

    // remove processed data from decoder buffer:
    mp_audio_buffer_skip(sh->decode_buffer, len);

    return error;
}

/* Try to get at least minsamples decoded+filtered samples in outbuf
 * (total length including possible existing data).
 * Return 0 on success, -1 on error/EOF (not distinguished).
 * In the former case outbuf has at least minsamples buffered on return.
 * In case of EOF/error it might or might not be. */
int decode_audio(sh_audio_t *sh_audio, struct mp_audio_buffer *outbuf,
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
    double filter_multiplier = af_calc_filter_multiplier(sh_audio->afilter);

    int prev_buffered = -1;
    while (minsamples >= 0) {
        int buffered = mp_audio_buffer_samples(outbuf);
        if (minsamples < buffered || buffered == prev_buffered)
            break;
        prev_buffered = buffered;

        int decsamples = (minsamples - buffered) / filter_multiplier;
        // + some extra for possible filter buffering
        decsamples += 1 << unitsize;

        if (huge_filter_buffer) {
            /* Some filter must be doing significant buffering if the estimated
             * input length didn't produce enough output from filters.
             * Feed the filters 2k bytes at a time until we have enough output.
             * Very small amounts could make filtering inefficient while large
             * amounts can make MPlayer demux the file unnecessarily far ahead
             * to get audio data and buffer video frames in memory while doing
             * so. However the performance impact of either is probably not too
             * significant as long as the value is not completely insane. */
            decsamples = 2000;
        }

        /* if this iteration does not fill buffer, we must have lots
         * of buffering in filters */
        huge_filter_buffer = 1;

        int res = filter_n_bytes(sh_audio, outbuf, decsamples);
        if (res < 0)
            return res;
    }
    return 0;
}

void resync_audio_stream(sh_audio_t *sh_audio)
{
    sh_audio->pts = MP_NOPTS_VALUE;
    sh_audio->pts_offset = 0;
    if (!sh_audio->initialized)
        return;
    sh_audio->ad_driver->control(sh_audio, ADCTRL_RESYNC_STREAM, NULL);
}
