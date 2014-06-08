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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "talloc.h"

#include "common/msg.h"
#include "common/encode.h"
#include "options/options.h"
#include "common/common.h"

#include "audio/mixer.h"
#include "audio/audio.h"
#include "audio/audio_buffer.h"
#include "audio/decode/dec_audio.h"
#include "audio/filter/af.h"
#include "audio/out/ao.h"
#include "demux/demux.h"
#include "video/decode/dec_video.h"

#include "core.h"
#include "command.h"

static int build_afilter_chain(struct MPContext *mpctx)
{
    struct dec_audio *d_audio = mpctx->d_audio;
    struct MPOpts *opts = mpctx->opts;

    if (!d_audio)
        return 0;

    struct mp_audio in_format;
    mp_audio_buffer_get_format(d_audio->decode_buffer, &in_format);

    struct mp_audio out_format;
    ao_get_format(mpctx->ao, &out_format);

    int new_srate;
    if (af_control_any_rev(d_audio->afilter, AF_CONTROL_SET_PLAYBACK_SPEED,
                           &opts->playback_speed))
        new_srate = in_format.rate;
    else {
        new_srate = in_format.rate * opts->playback_speed;
        if (new_srate != out_format.rate)
            opts->playback_speed = new_srate / (double)in_format.rate;
    }
    return audio_init_filters(d_audio, new_srate,
                &out_format.rate, &out_format.channels, &out_format.format);
}

static int recreate_audio_filters(struct MPContext *mpctx)
{
    assert(mpctx->d_audio);

    // init audio filters:
    if (!build_afilter_chain(mpctx)) {
        MP_ERR(mpctx, "Couldn't find matching filter/ao format!\n");
        return -1;
    }

    mixer_reinit_audio(mpctx->mixer, mpctx->ao, mpctx->d_audio->afilter);

    return 0;
}

int reinit_audio_filters(struct MPContext *mpctx)
{
    struct dec_audio *d_audio = mpctx->d_audio;
    if (!d_audio)
        return 0;

    af_uninit(mpctx->d_audio->afilter);
    if (af_init(mpctx->d_audio->afilter) < 0)
        return -1;
    if (recreate_audio_filters(mpctx) < 0)
        return -1;

    return 1;
}

void reinit_audio_chain(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct track *track = mpctx->current_track[0][STREAM_AUDIO];
    struct sh_stream *sh = init_demux_stream(mpctx, track);
    if (!sh) {
        uninit_player(mpctx, INITIALIZED_AO);
        goto no_audio;
    }

    mp_notify(mpctx, MPV_EVENT_AUDIO_RECONFIG, NULL);

    if (!(mpctx->initialized_flags & INITIALIZED_ACODEC)) {
        mpctx->initialized_flags |= INITIALIZED_ACODEC;
        assert(!mpctx->d_audio);
        mpctx->d_audio = talloc_zero(NULL, struct dec_audio);
        mpctx->d_audio->log = mp_log_new(mpctx->d_audio, mpctx->log, "!ad");
        mpctx->d_audio->global = mpctx->global;
        mpctx->d_audio->opts = opts;
        mpctx->d_audio->header = sh;
        mpctx->d_audio->metadata = mpctx->demuxer->metadata;
        mpctx->d_audio->replaygain_data = mpctx->demuxer->replaygain_data;
        if (!audio_init_best_codec(mpctx->d_audio, opts->audio_decoders))
            goto init_error;
    }
    assert(mpctx->d_audio);

    struct mp_audio in_format;
    mp_audio_buffer_get_format(mpctx->d_audio->decode_buffer, &in_format);

    if (mpctx->ao_decoder_fmt && (mpctx->initialized_flags & INITIALIZED_AO) &&
        !mp_audio_config_equals(mpctx->ao_decoder_fmt, &in_format) &&
        opts->gapless_audio < 0)
    {
        uninit_player(mpctx, INITIALIZED_AO);
    }

    int ao_srate = opts->force_srate;
    int ao_format = opts->audio_output_format;
    struct mp_chmap ao_channels = {0};
    if (mpctx->initialized_flags & INITIALIZED_AO) {
        struct mp_audio out_format;
        ao_get_format(mpctx->ao, &out_format);
        ao_srate    = out_format.rate;
        ao_format   = out_format.format;
        ao_channels = out_format.channels;
    } else {
        if (!AF_FORMAT_IS_SPECIAL(in_format.format))
            ao_channels = opts->audio_output_channels; // automatic downmix
    }

    // Determine what the filter chain outputs. build_afilter_chain() also
    // needs this for testing whether playback speed is changed by resampling
    // or using a special filter.
    if (!audio_init_filters(mpctx->d_audio,  // preliminary init
                            // input:
                            in_format.rate,
                            // output:
                            &ao_srate, &ao_channels, &ao_format)) {
        MP_ERR(mpctx, "Error at audio filter chain pre-init!\n");
        goto init_error;
    }

    if (!(mpctx->initialized_flags & INITIALIZED_AO)) {
        mpctx->initialized_flags |= INITIALIZED_AO;
        mp_chmap_remove_useless_channels(&ao_channels,
                                         &opts->audio_output_channels);
        mpctx->ao = ao_init_best(mpctx->global, mpctx->input,
                                 mpctx->encode_lavc_ctx, ao_srate, ao_format,
                                 ao_channels);
        struct ao *ao = mpctx->ao;
        if (!ao) {
            MP_ERR(mpctx, "Could not open/initialize audio device -> no sound.\n");
            goto init_error;
        }

        struct mp_audio fmt;
        ao_get_format(ao, &fmt);

        mpctx->ao_buffer = mp_audio_buffer_create(ao);
        mp_audio_buffer_reinit(mpctx->ao_buffer, &fmt);

        mpctx->ao_decoder_fmt = talloc(NULL, struct mp_audio);
        *mpctx->ao_decoder_fmt = in_format;

        char *s = mp_audio_config_to_str(&fmt);
        MP_INFO(mpctx, "AO: [%s] %s\n", ao_get_name(ao), s);
        talloc_free(s);
        MP_VERBOSE(mpctx, "AO: Description: %s\n", ao_get_description(ao));
        update_window_title(mpctx, true);
    }

    if (recreate_audio_filters(mpctx) < 0)
        goto init_error;

    mpctx->syncing_audio = true;
    return;

init_error:
    uninit_player(mpctx, INITIALIZED_ACODEC | INITIALIZED_AO);
no_audio:
    mp_deselect_track(mpctx, track);
    MP_INFO(mpctx, "Audio: no audio\n");
}

// Return pts value corresponding to the end point of audio written to the
// ao so far.
double written_audio_pts(struct MPContext *mpctx)
{
    struct dec_audio *d_audio = mpctx->d_audio;
    if (!d_audio)
        return MP_NOPTS_VALUE;

    struct mp_audio in_format;
    mp_audio_buffer_get_format(d_audio->decode_buffer, &in_format);

    // first calculate the end pts of audio that has been output by decoder
    double a_pts = d_audio->pts;
    if (a_pts == MP_NOPTS_VALUE)
        return MP_NOPTS_VALUE;

    // d_audio->pts is the timestamp of the latest input packet with
    // known pts that the decoder has decoded. d_audio->pts_bytes is
    // the amount of bytes the decoder has written after that timestamp.
    a_pts += d_audio->pts_offset / (double)in_format.rate;

    // Now a_pts hopefully holds the pts for end of audio from decoder.
    // Subtract data in buffers between decoder and audio out.

    // Decoded but not filtered
    a_pts -= mp_audio_buffer_seconds(d_audio->decode_buffer);

    // Data buffered in audio filters, measured in seconds of "missing" output
    double buffered_output = af_calc_delay(d_audio->afilter);

    // Data that was ready for ao but was buffered because ao didn't fully
    // accept everything to internal buffers yet
    buffered_output += mp_audio_buffer_seconds(mpctx->ao_buffer);

    // Filters divide audio length by playback_speed, so multiply by it
    // to get the length in original units without speedup or slowdown
    a_pts -= buffered_output * mpctx->opts->playback_speed;

    return a_pts + mpctx->video_offset;
}

// Return pts value corresponding to currently playing audio.
double playing_audio_pts(struct MPContext *mpctx)
{
    double pts = written_audio_pts(mpctx);
    if (pts == MP_NOPTS_VALUE)
        return pts;
    return pts - mpctx->opts->playback_speed * ao_get_delay(mpctx->ao);
}

static int write_to_ao(struct MPContext *mpctx, struct mp_audio *data, int flags,
                       double pts)
{
    if (mpctx->paused)
        return 0;
    struct ao *ao = mpctx->ao;
    struct mp_audio out_format;
    ao_get_format(ao, &out_format);
    mpctx->ao_pts = pts;
#if HAVE_ENCODING
    encode_lavc_set_audio_pts(mpctx->encode_lavc_ctx, mpctx->ao_pts);
#endif
    if (data->samples == 0)
        return 0;
    double real_samplerate = out_format.rate / mpctx->opts->playback_speed;
    int played = ao_play(mpctx->ao, data->planes, data->samples, flags);
    assert(played <= data->samples);
    if (played > 0) {
        mpctx->shown_aframes += played;
        mpctx->delay += played / real_samplerate;
        // Keep correct pts for remaining data - could be used to flush
        // remaining buffer when closing ao.
        mpctx->ao_pts += played / real_samplerate;
        return played;
    }
    return 0;
}

static int write_silence_to_ao(struct MPContext *mpctx, int samples, int flags,
                               double pts)
{
    struct mp_audio tmp = {0};
    mp_audio_buffer_get_format(mpctx->ao_buffer, &tmp);
    tmp.samples = samples;
    char *p = talloc_size(NULL, tmp.samples * tmp.sstride);
    for (int n = 0; n < tmp.num_planes; n++)
        tmp.planes[n] = p;
    mp_audio_fill_silence(&tmp, 0, tmp.samples);
    int r = write_to_ao(mpctx, &tmp, 0, pts);
    talloc_free(p);
    return r;
}

#define ASYNC_PLAY_DONE -3
static int audio_start_sync(struct MPContext *mpctx, int playsize)
{
    struct ao *ao = mpctx->ao;
    struct MPOpts *opts = mpctx->opts;
    struct dec_audio *d_audio = mpctx->d_audio;
    int res;

    assert(d_audio);

    struct mp_audio out_format;
    ao_get_format(ao, &out_format);

    // Timing info may not be set without
    res = audio_decode(d_audio, mpctx->ao_buffer, 1);
    if (res < 0)
        return res;

    int samples;
    bool did_retry = false;
    double written_pts;
    double real_samplerate = out_format.rate / opts->playback_speed;
    bool hrseek = mpctx->hrseek_active;   // audio only hrseek
    mpctx->hrseek_active = false;
    while (1) {
        written_pts = written_audio_pts(mpctx);
        double ptsdiff;
        if (hrseek)
            ptsdiff = written_pts - mpctx->hrseek_pts;
        else
            ptsdiff = written_pts - mpctx->video_next_pts - mpctx->delay
                      + mpctx->audio_delay;
        samples = ptsdiff * real_samplerate;

        // ogg demuxers give packets without timing
        if (written_pts <= 1 && d_audio->pts == MP_NOPTS_VALUE) {
            if (!did_retry) {
                // Try to read more data to see packets that have pts
                res = audio_decode(d_audio, mpctx->ao_buffer, out_format.rate);
                if (res < 0)
                    return res;
                did_retry = true;
                continue;
            }
            samples = 0;
        }

        if (fabs(ptsdiff) > 300 || isnan(ptsdiff))   // pts reset or just broken?
            samples = 0;

        if (samples > 0)
            break;

        mpctx->syncing_audio = false;
        int skip_samples = -samples;
        int a = MPMIN(skip_samples, MPMAX(playsize, 2500));
        res = audio_decode(d_audio, mpctx->ao_buffer, a);
        if (skip_samples <= mp_audio_buffer_samples(mpctx->ao_buffer)) {
            mp_audio_buffer_skip(mpctx->ao_buffer, skip_samples);
            if (res < 0)
                return res;
            return audio_decode(d_audio, mpctx->ao_buffer, playsize);
        }
        mp_audio_buffer_clear(mpctx->ao_buffer);
        if (res < 0)
            return res;
    }
    if (hrseek)
        // Don't add silence in audio-only case even if position is too late
        return 0;
    if (samples >= playsize) {
        /* This case could fall back to the one below with
         * samples = playsize, but then silence would keep accumulating
         * in ao_buffer if the AO accepts less data than it asks for
         * in playsize. */
        write_silence_to_ao(mpctx, playsize, 0,
                            written_pts - samples / real_samplerate);
        return ASYNC_PLAY_DONE;
    }
    mpctx->syncing_audio = false;
    mp_audio_buffer_prepend_silence(mpctx->ao_buffer, samples);
    return audio_decode(d_audio, mpctx->ao_buffer, playsize);
}

int fill_audio_out_buffers(struct MPContext *mpctx, double endpts)
{
    struct MPOpts *opts = mpctx->opts;
    struct ao *ao = mpctx->ao;
    int playsize;
    int playflags = 0;
    bool audio_eof = false;
    bool signal_eof = false;
    bool partial_fill = false;
    struct dec_audio *d_audio = mpctx->d_audio;
    struct mp_audio out_format;
    ao_get_format(ao, &out_format);
    // Can't adjust the start of audio with spdif pass-through.
    bool modifiable_audio_format = !(out_format.format & AF_FORMAT_SPECIAL_MASK);

    assert(d_audio);

    if (mpctx->paused)
        playsize = 1;   // just initialize things (audio pts at least)
    else
        playsize = ao_get_space(ao);

    // Coming here with hrseek_active still set means audio-only
    if (!mpctx->d_video || !mpctx->sync_audio_to_video)
        mpctx->syncing_audio = false;
    if (!opts->initial_audio_sync || !modifiable_audio_format) {
        mpctx->syncing_audio = false;
        mpctx->hrseek_active = false;
    }

    int res;
    if (mpctx->syncing_audio || mpctx->hrseek_active)
        res = audio_start_sync(mpctx, playsize);
    else
        res = audio_decode(d_audio, mpctx->ao_buffer, playsize);

    if (res < 0) {  // EOF, error or format change
        if (res == -2) {
            /* The format change isn't handled too gracefully. A more precise
             * implementation would require draining buffered old-format audio
             * while displaying video, then doing the output format switch.
             */
            if (mpctx->opts->gapless_audio < 1)
                uninit_player(mpctx, INITIALIZED_AO);
            reinit_audio_chain(mpctx);
            return -1;
        } else if (res == ASYNC_PLAY_DONE)
            return 0;
        else if (demux_stream_eof(d_audio->header))
            audio_eof = true;
    }

    if (endpts != MP_NOPTS_VALUE) {
        double samples = (endpts - written_audio_pts(mpctx) - mpctx->audio_delay)
                         * out_format.rate / opts->playback_speed;
        if (playsize > samples) {
            playsize = MPMAX(samples, 0);
            audio_eof = true;
            partial_fill = true;
        }
    }

    if (playsize > mp_audio_buffer_samples(mpctx->ao_buffer)) {
        playsize = mp_audio_buffer_samples(mpctx->ao_buffer);
        partial_fill = true;
    }
    if (!playsize)
        return partial_fill && audio_eof ? -2 : -partial_fill;

    if (audio_eof && partial_fill) {
        if (opts->gapless_audio) {
            // With gapless audio, delay this to ao_uninit. There must be only
            // 1 final chunk, and that is handled when calling ao_uninit().
            signal_eof = true;
        } else {
            playflags |= AOPLAY_FINAL_CHUNK;
        }
    }

    if (mpctx->paused)
        playsize = 0;

    struct mp_audio data;
    mp_audio_buffer_peek(mpctx->ao_buffer, &data);
    data.samples = MPMIN(data.samples, playsize);
    int played = write_to_ao(mpctx, &data, playflags, written_audio_pts(mpctx));
    assert(played >= 0 && played <= data.samples);

    mp_audio_buffer_skip(mpctx->ao_buffer, played);

    return signal_eof ? -2 : -partial_fill;
}

// Drop data queued for output, or which the AO is currently outputting.
void clear_audio_output_buffers(struct MPContext *mpctx)
{
    if (mpctx->ao) {
        ao_reset(mpctx->ao);
        mp_audio_buffer_clear(mpctx->ao_buffer);
    }
}

// Drop decoded data queued for filtering.
void clear_audio_decode_buffers(struct MPContext *mpctx)
{
    if (mpctx->d_audio)
        mp_audio_buffer_clear(mpctx->d_audio->decode_buffer);
}
