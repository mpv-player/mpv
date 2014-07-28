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
#include "common/codecs.h"
#include "common/msg.h"
#include "bstr/bstr.h"

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

// Drop audio buffer and reinit it (after format change)
// Returns whether the format was valid at all.
static bool reinit_audio_buffer(struct dec_audio *da)
{
    if (!mp_audio_config_valid(&da->decoded)) {
        MP_ERR(da, "Audio decoder did not specify audio "
               "format, or requested an unsupported configuration!\n");
        return false;
    }
    mp_audio_buffer_reinit(da->decode_buffer, &da->decoded);
    return true;
}

static void uninit_decoder(struct dec_audio *d_audio)
{
    if (d_audio->ad_driver) {
        MP_VERBOSE(d_audio, "Uninit audio decoder.\n");
        d_audio->ad_driver->uninit(d_audio);
    }
    d_audio->ad_driver = NULL;
    talloc_free(d_audio->priv);
    d_audio->priv = NULL;
}

static int init_audio_codec(struct dec_audio *d_audio, const char *decoder)
{
    if (!d_audio->ad_driver->init(d_audio, decoder)) {
        MP_VERBOSE(d_audio, "Audio decoder init failed.\n");
        d_audio->ad_driver = NULL;
        uninit_decoder(d_audio);
        return 0;
    }

    d_audio->decode_buffer = mp_audio_buffer_create(NULL);
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
    assert(!d_audio->ad_driver);
    audio_reset_decoding(d_audio);

    struct mp_decoder_entry *decoder = NULL;
    struct mp_decoder_list *list =
        audio_select_decoders(d_audio->header->codec, audio_decoders);

    mp_print_decoders(d_audio->log, MSGL_V, "Codec list:", list);

    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *sel = &list->entries[n];
        const struct ad_functions *driver = find_driver(sel->family);
        if (!driver)
            continue;
        MP_VERBOSE(d_audio, "Opening audio decoder %s:%s\n",
                   sel->family, sel->decoder);
        d_audio->ad_driver = driver;
        if (init_audio_codec(d_audio, sel->decoder)) {
            decoder = sel;
            break;
        }
        MP_WARN(d_audio, "Audio decoder init failed for "
                "%s:%s\n", sel->family, sel->decoder);
    }

    if (d_audio->ad_driver) {
        d_audio->decoder_desc =
            talloc_asprintf(d_audio, "%s [%s:%s]", decoder->desc, decoder->family,
                            decoder->decoder);
        MP_VERBOSE(d_audio, "Selected audio codec: %s\n", d_audio->decoder_desc);
    } else {
        MP_ERR(d_audio, "Failed to initialize an audio decoder for codec '%s'.\n",
               d_audio->header->codec ? d_audio->header->codec : "<unknown>");
    }

    talloc_free(list);
    return !!d_audio->ad_driver;
}

void audio_uninit(struct dec_audio *d_audio)
{
    if (!d_audio)
        return;
    if (d_audio->afilter) {
        MP_VERBOSE(d_audio, "Uninit audio filters...\n");
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
        d_audio->afilter = af_new(d_audio->global);
    struct af_stream *afs = d_audio->afilter;

    // input format: same as codec's output format:
    mp_audio_buffer_get_format(d_audio->decode_buffer, &afs->input);
    // Sample rate can be different when adjusting playback speed
    afs->input.rate = in_samplerate;

    // output format: same as ao driver's input format (if missing, fallback to input)
    afs->output.rate = *out_samplerate;
    mp_audio_set_channels(&afs->output, out_channels);
    mp_audio_set_format(&afs->output, *out_format);

    afs->replaygain_data = d_audio->replaygain_data;

    char *s_from = mp_audio_config_to_str(&afs->input);
    char *s_to = mp_audio_config_to_str(&afs->output);
    MP_VERBOSE(d_audio, "Building audio filter chain for %s -> %s...\n", s_from, s_to);
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

/* Decode packets until we know the audio format. Then reinit the buffer.
 * Returns AD_OK on success, negative AD_* code otherwise.
 * Also returns AD_OK if already initialized (and does nothing).
 */
int initial_audio_decode(struct dec_audio *da)
{
    while (!mp_audio_config_valid(&da->decoded)) {
        if (da->decoded.samples > 0)
            return AD_ERR; // invalid format, rather than uninitialized
        int ret = da->ad_driver->decode_packet(da);
        if (ret < 0)
            return ret;
    }
    if (mp_audio_buffer_samples(da->decode_buffer) > 0) // avoid accidental flush
        return AD_OK;
    return reinit_audio_buffer(da) ? AD_OK : AD_ERR;
}

// Filter len bytes of input, put result into outbuf.
static int filter_n_bytes(struct dec_audio *da, struct mp_audio_buffer *outbuf,
                          int len)
{
    bool format_change = false;
    int error = 0;

    assert(len > 0); // would break EOF logic below

    while (mp_audio_buffer_samples(da->decode_buffer) < len) {
        // Check for a format change
        struct mp_audio config;
        mp_audio_buffer_get_format(da->decode_buffer, &config);
        format_change = !mp_audio_config_equals(&da->decoded, &config);
        if (format_change) {
            error = AD_EOF; // drain remaining data left in the current buffer
            break;
        }
        if (da->decoded.samples > 0) {
            int copy = MPMIN(da->decoded.samples, len);
            struct mp_audio append = da->decoded;
            append.samples = copy;
            mp_audio_buffer_append(da->decode_buffer, &append);
            mp_audio_skip_samples(&da->decoded, copy);
            da->pts_offset += copy;
            continue;
        }
        error = da->ad_driver->decode_packet(da);
        if (error < 0)
            break;
    }

    if (error == AD_WAIT)
        return error;

    // Filter
    struct mp_audio filter_data;
    mp_audio_buffer_peek(da->decode_buffer, &filter_data);
    filter_data.rate = da->afilter->input.rate; // due to playback speed change
    len = MPMIN(filter_data.samples, len);
    filter_data.samples = len;
    bool eof = error == AD_EOF && filter_data.samples == 0;

    if (af_filter(da->afilter, &filter_data, eof ? AF_FILTER_FLAG_EOF : 0) < 0)
        return AD_ERR;

    mp_audio_buffer_append(outbuf, &filter_data);
    if (error == AD_EOF && filter_data.samples > 0)
        error = 0; // don't end playback yet

    // remove processed data from decoder buffer:
    mp_audio_buffer_skip(da->decode_buffer, len);

    // if format was changed, and all data was drained, execute the format change
    if (format_change && eof) {
        error = AD_NEW_FMT;
        if (!reinit_audio_buffer(da))
            error = AD_ERR; // switch to invalid format
    }

    return error;
}

/* Try to get at least minsamples decoded+filtered samples in outbuf
 * (total length including possible existing data).
 * Return 0 on success, or negative AD_* error code.
 * In the former case outbuf has at least minsamples buffered on return.
 * In case of EOF/error it might or might not be. */
int audio_decode(struct dec_audio *d_audio, struct mp_audio_buffer *outbuf,
                 int minsamples)
{
    if (!d_audio->afilter)
        return AD_ERR;

    // Indicates that a filter seems to be buffering large amounts of data
    int huge_filter_buffer = 0;

    /* Filter output size will be about filter_multiplier times input size.
     * If some filter buffers audio in big blocks this might only hold
     * as average over time. */
    double filter_multiplier = af_calc_filter_multiplier(d_audio->afilter);

    int prev_buffered = -1;
    int res = 0;
    MP_STATS(d_audio, "start audio");
    while (res >= 0 && minsamples >= 0) {
        int buffered = mp_audio_buffer_samples(outbuf);
        if (minsamples < buffered || buffered == prev_buffered)
            break;
        prev_buffered = buffered;

        int decsamples = (minsamples - buffered) / filter_multiplier;
        // + some extra for possible filter buffering, and avoid 0
        decsamples += 512;

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

        res = filter_n_bytes(d_audio, outbuf, decsamples);
    }
    MP_STATS(d_audio, "end audio");
    return res;
}

void audio_reset_decoding(struct dec_audio *d_audio)
{
    if (d_audio->ad_driver)
        d_audio->ad_driver->control(d_audio, ADCTRL_RESET, NULL);
    if (d_audio->afilter)
        af_control_all(d_audio->afilter, AF_CONTROL_RESET, NULL);
    d_audio->pts = MP_NOPTS_VALUE;
    d_audio->pts_offset = 0;
    if (d_audio->decode_buffer)
        mp_audio_buffer_clear(d_audio->decode_buffer);
}
