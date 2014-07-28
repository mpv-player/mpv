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
#include <limits.h>
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

static int recreate_audio_filters(struct MPContext *mpctx)
{
    struct dec_audio *d_audio = mpctx->d_audio;
    struct MPOpts *opts = mpctx->opts;

    assert(d_audio);

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
    if (!audio_init_filters(d_audio, new_srate,
                &out_format.rate, &out_format.channels, &out_format.format))
    {
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

    mpctx->audio_status = STATUS_SYNCING;

    if (!(mpctx->initialized_flags & INITIALIZED_ACODEC)) {
        mpctx->initialized_flags |= INITIALIZED_ACODEC;
        assert(!mpctx->d_audio);
        mpctx->d_audio = talloc_zero(NULL, struct dec_audio);
        mpctx->d_audio->log = mp_log_new(mpctx->d_audio, mpctx->log, "!ad");
        mpctx->d_audio->global = mpctx->global;
        mpctx->d_audio->opts = opts;
        mpctx->d_audio->header = sh;
        mpctx->d_audio->replaygain_data = sh->audio->replaygain_data;
        if (!audio_init_best_codec(mpctx->d_audio, opts->audio_decoders))
            goto init_error;
    }
    assert(mpctx->d_audio);

    if (!mpctx->ao_buffer)
        mpctx->ao_buffer = mp_audio_buffer_create(mpctx);

    struct mp_audio in_format;
    mp_audio_buffer_get_format(mpctx->d_audio->decode_buffer, &in_format);

    if (!mp_audio_config_valid(&in_format)) {
        // We don't know the audio format yet - so configure it later as we're
        // resyncing. fill_audio_buffers() will call this function again.
        mpctx->sleeptime = 0;
        return;
    }

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

    // Determine what the filter chain outputs. recreate_audio_filters() also
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

    if (!mp_audio_config_valid(&in_format) || !d_audio->afilter)
        return MP_NOPTS_VALUE;;

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
    if (pts == MP_NOPTS_VALUE || !mpctx->ao)
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
    encode_lavc_set_audio_pts(mpctx->encode_lavc_ctx, playing_audio_pts(mpctx));
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

// Return the number of samples that must be skipped or prepended to reach the
// target audio pts after a seek (for A/V sync or hr-seek).
// Return value (*skip):
//   >0: skip this many samples
//   =0: don't do anything
//   <0: prepend this many samples of silence
// Returns false if PTS is not known yet.
static bool get_sync_samples(struct MPContext *mpctx, int *skip)
{
    struct MPOpts *opts = mpctx->opts;
    *skip = 0;

    if (mpctx->audio_status != STATUS_SYNCING)
        return true;

    struct mp_audio out_format = {0};
    ao_get_format(mpctx->ao, &out_format);
    double play_samplerate = out_format.rate / opts->playback_speed;

    bool is_pcm = !(out_format.format & AF_FORMAT_SPECIAL_MASK); // no spdif
    if (!opts->initial_audio_sync || !is_pcm) {
        mpctx->audio_status = STATUS_FILLING;
        return true;
    }

    double written_pts = written_audio_pts(mpctx);
    if (written_pts == MP_NOPTS_VALUE && !mp_audio_buffer_samples(mpctx->ao_buffer))
        return false; // no audio read yet

    bool sync_to_video = mpctx->d_video && mpctx->sync_audio_to_video;

    double sync_pts = MP_NOPTS_VALUE;
    if (sync_to_video) {
        if (mpctx->video_next_pts != MP_NOPTS_VALUE) {
            sync_pts = mpctx->video_next_pts;
        } else if (mpctx->video_status < STATUS_READY) {
            return false; // wait until we know a video PTS
        }
    } else if (mpctx->hrseek_active) {
        sync_pts = mpctx->hrseek_pts;
    }
    if (sync_pts == MP_NOPTS_VALUE) {
        mpctx->audio_status = STATUS_FILLING;
        return true; // syncing disabled
    }

    if (sync_to_video)
        sync_pts += mpctx->delay - mpctx->audio_delay;

    double ptsdiff = written_pts - sync_pts;
    // Missing timestamp, or PTS reset, or just broken.
    if (written_pts == MP_NOPTS_VALUE || fabs(ptsdiff) > 300) {
        MP_WARN(mpctx, "Failed audio resync.\n");
        mpctx->audio_status = STATUS_FILLING;
        return true;
    }

    *skip = -ptsdiff * play_samplerate;
    return true;
}

int fill_audio_out_buffers(struct MPContext *mpctx, double endpts)
{
    struct MPOpts *opts = mpctx->opts;
    struct dec_audio *d_audio = mpctx->d_audio;

    assert(d_audio);

    if (!d_audio->afilter || !mpctx->ao) {
        // Probe the initial audio format. Returns AD_OK (and does nothing) if
        // the format is already known.
        int r = initial_audio_decode(mpctx->d_audio);
        if (r == AD_WAIT)
            return -1; // continue later when new data is available
        if (r != AD_OK) {
            MP_ERR(mpctx, "Error initializing audio.\n");
            struct track *track = mpctx->current_track[0][STREAM_AUDIO];
            mp_deselect_track(mpctx, track);
            return -2;
        }
        reinit_audio_chain(mpctx);
        return -1; // try again next iteration
    }

    // if paused, just initialize things (audio format & pts)
    int playsize = 1;
    if (!mpctx->paused)
        playsize = ao_get_space(mpctx->ao);

    int skip = 0;
    bool sync_known = get_sync_samples(mpctx, &skip);
    if (skip > 0) {
        playsize = MPMIN(skip + 1, MPMAX(playsize, 2500)); // buffer extra data
    } else if (skip < 0) {
        playsize = MPMAX(1, playsize + skip); // silence will be prepended
    }

    int status = AD_OK;
    if (playsize > mp_audio_buffer_samples(mpctx->ao_buffer)) {
        status = audio_decode(d_audio, mpctx->ao_buffer, playsize);
        if (status == AD_WAIT)
            return -1;
        if (status == AD_NEW_FMT) {
            /* The format change isn't handled too gracefully. A more precise
             * implementation would require draining buffered old-format audio
             * while displaying video, then doing the output format switch.
             */
            if (mpctx->opts->gapless_audio < 1)
                uninit_player(mpctx, INITIALIZED_AO);
            reinit_audio_chain(mpctx);
            mpctx->sleeptime = 0;
            return -1; // retry on next iteration
        }
    }

    bool end_sync = status != AD_OK; // (on error/EOF, start playback immediately)
    if (skip >= 0) {
        int max = mp_audio_buffer_samples(mpctx->ao_buffer);
        mp_audio_buffer_skip(mpctx->ao_buffer, MPMIN(skip, max));
        // If something is left, we definitely reached the target time.
        end_sync |= sync_known && skip < max;
    } else if (skip < 0) {
        if (-skip < 1000000) { // heuristic against making the buffer too large
            mp_audio_buffer_prepend_silence(mpctx->ao_buffer, -skip);
        } else {
            MP_ERR(mpctx, "Audio starts too late: sync. failed.\n");
            ao_reset(mpctx->ao);
        }
        end_sync = true;
    }

    if (mpctx->audio_status == STATUS_SYNCING) {
        if (end_sync)
            mpctx->audio_status = STATUS_FILLING;
        mpctx->sleeptime = 0;
        return -1; // continue on next iteration
    }

    assert(mpctx->audio_status >= STATUS_FILLING);

    // Even if we're done decoding and syncing, let video start first - this is
    // required, because sending audio to the AO already starts playback.
    if (mpctx->audio_status == STATUS_FILLING && mpctx->sync_audio_to_video &&
        mpctx->video_status <= STATUS_READY)
    {
        mpctx->audio_status = STATUS_READY;
        return -1;
    }

    bool audio_eof = status == AD_EOF;
    bool partial_fill = false;
    int playflags = 0;

    struct mp_audio out_format = {0};
    ao_get_format(mpctx->ao, &out_format);
    double play_samplerate = out_format.rate / opts->playback_speed;

    if (endpts != MP_NOPTS_VALUE) {
        double samples = (endpts - written_audio_pts(mpctx) - mpctx->audio_delay)
                         * play_samplerate;
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

    audio_eof &= partial_fill;

    if (audio_eof) {
        // With gapless audio, delay this to ao_uninit. There must be only
        // 1 final chunk, and that is handled when calling ao_uninit().
        if (opts->gapless_audio)
            playflags |= AOPLAY_FINAL_CHUNK;
    }

    if (mpctx->paused)
        playsize = 0;

    struct mp_audio data;
    mp_audio_buffer_peek(mpctx->ao_buffer, &data);
    data.samples = MPMIN(data.samples, playsize);
    int played = write_to_ao(mpctx, &data, playflags, written_audio_pts(mpctx));
    assert(played >= 0 && played <= data.samples);
    mp_audio_buffer_skip(mpctx->ao_buffer, played);

    mpctx->audio_status = STATUS_PLAYING;
    if (audio_eof) {
        mpctx->audio_status = STATUS_DRAINING;
        // Wait until the AO has played all queued data. In the gapless case,
        // we trigger EOF immediately, and let it play asynchronously.
        if (ao_eof_reached(mpctx->ao) || opts->gapless_audio)
            mpctx->audio_status = STATUS_EOF;
    }

    return 0;
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
