/*
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
#include "osdep/timer.h"

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

// Use pitch correction only for speed adjustments by the user, not minor sync
// correction ones.
static int get_speed_method(struct MPContext *mpctx)
{
    return mpctx->opts->pitch_correction && mpctx->opts->playback_speed != 1.0
        ? AF_CONTROL_SET_PLAYBACK_SPEED : AF_CONTROL_SET_PLAYBACK_SPEED_RESAMPLE;
}

// Try to reuse the existing filters to change playback speed. If it works,
// return true; if filter recreation is needed, return false.
static bool update_speed_filters(struct MPContext *mpctx)
{
    struct af_stream *afs = mpctx->d_audio->afilter;
    double speed = mpctx->audio_speed;

    if (afs->initialized < 1)
        return false;

    // Make sure only exactly one filter changes speed; resetting them all
    // and setting 1 filter is the easiest way to achieve this.
    af_control_all(afs, AF_CONTROL_SET_PLAYBACK_SPEED, &(double){1});
    af_control_all(afs, AF_CONTROL_SET_PLAYBACK_SPEED_RESAMPLE, &(double){1});

    if (speed == 1.0)
        return !af_find_by_label(afs, "playback-speed");

    // Compatibility: if the user uses --af=scaletempo, always use this
    // filter to change speed. Don't insert a second filter (any) either.
    if (!af_find_by_label(afs, "playback-speed") &&
        af_control_any_rev(afs, AF_CONTROL_SET_PLAYBACK_SPEED, &speed))
        return true;

    return !!af_control_any_rev(afs, get_speed_method(mpctx), &speed);
}

// Update speed, and insert/remove filters if necessary.
static void recreate_speed_filters(struct MPContext *mpctx)
{
    struct af_stream *afs = mpctx->d_audio->afilter;

    if (update_speed_filters(mpctx))
        return;

    if (af_remove_by_label(afs, "playback-speed") < 0)
        goto fail;

    if (mpctx->audio_speed == 1.0)
        return;

    int method = get_speed_method(mpctx);
    char *filter = method == AF_CONTROL_SET_PLAYBACK_SPEED
                 ? "scaletempo" : "lavrresample";

    if (!af_add(afs, filter, "playback-speed", NULL))
        goto fail;

    if (!update_speed_filters(mpctx))
        goto fail;

    return;

fail:
    mpctx->opts->playback_speed = 1.0;
    mpctx->speed_factor_a = 1.0;
    mpctx->audio_speed = 1.0;
    mp_notify(mpctx, MP_EVENT_CHANGE_ALL, NULL);
}

static int recreate_audio_filters(struct MPContext *mpctx)
{
    assert(mpctx->d_audio);

    struct af_stream *afs = mpctx->d_audio->afilter;
    if (afs->initialized < 1 && af_init(afs) < 0)
        goto fail;

    recreate_speed_filters(mpctx);
    if (afs->initialized < 1 && af_init(afs) < 0)
        goto fail;

    mixer_reinit_audio(mpctx->mixer, mpctx->ao, afs);

    return 0;

fail:
    MP_ERR(mpctx, "Couldn't find matching filter/ao format!\n");
    return -1;
}

int reinit_audio_filters(struct MPContext *mpctx)
{
    struct dec_audio *d_audio = mpctx->d_audio;
    if (!d_audio)
        return 0;

    af_uninit(mpctx->d_audio->afilter);
    return recreate_audio_filters(mpctx) < 0 ? -1 : 1;
}

// Call this if opts->playback_speed or mpctx->speed_factor_* change.
void update_playback_speed(struct MPContext *mpctx)
{
    mpctx->audio_speed = mpctx->opts->playback_speed * mpctx->speed_factor_a;
    mpctx->video_speed = mpctx->opts->playback_speed * mpctx->speed_factor_v;

    if (!mpctx->d_audio || mpctx->d_audio->afilter->initialized < 1)
        return;

    if (!update_speed_filters(mpctx))
        recreate_audio_filters(mpctx);
}

void reset_audio_state(struct MPContext *mpctx)
{
    if (mpctx->d_audio)
        audio_reset_decoding(mpctx->d_audio);
    if (mpctx->ao_buffer)
        mp_audio_buffer_clear(mpctx->ao_buffer);
    mpctx->audio_status = mpctx->d_audio ? STATUS_SYNCING : STATUS_EOF;
    mpctx->delay = 0;
    mpctx->audio_drop_throttle = 0;
    mpctx->audio_stat_start = 0;
}

void uninit_audio_out(struct MPContext *mpctx)
{
    if (mpctx->ao) {
        // Note: with gapless_audio, stop_play is not correctly set
        if (mpctx->opts->gapless_audio || mpctx->stop_play == AT_END_OF_FILE)
            ao_drain(mpctx->ao);
        mixer_uninit_audio(mpctx->mixer);
        ao_uninit(mpctx->ao);

        mp_notify(mpctx, MPV_EVENT_AUDIO_RECONFIG, NULL);
    }
    mpctx->ao = NULL;
    talloc_free(mpctx->ao_decoder_fmt);
    mpctx->ao_decoder_fmt = NULL;
}

void uninit_audio_chain(struct MPContext *mpctx)
{
    if (mpctx->d_audio) {
        mixer_uninit_audio(mpctx->mixer);
        audio_uninit(mpctx->d_audio);
        mpctx->d_audio = NULL;
        talloc_free(mpctx->ao_buffer);
        mpctx->ao_buffer = NULL;
        mpctx->audio_status = STATUS_EOF;
        reselect_demux_streams(mpctx);

        mp_notify(mpctx, MPV_EVENT_AUDIO_RECONFIG, NULL);
    }
}

void reinit_audio_chain(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct track *track = mpctx->current_track[0][STREAM_AUDIO];
    struct sh_stream *sh = track ? track->stream : NULL;
    if (!sh) {
        uninit_audio_out(mpctx);
        goto no_audio;
    }

    mp_notify(mpctx, MPV_EVENT_AUDIO_RECONFIG, NULL);

    if (!mpctx->d_audio) {
        mpctx->d_audio = talloc_zero(NULL, struct dec_audio);
        mpctx->d_audio->log = mp_log_new(mpctx->d_audio, mpctx->log, "!ad");
        mpctx->d_audio->global = mpctx->global;
        mpctx->d_audio->opts = opts;
        mpctx->d_audio->header = sh;
        mpctx->d_audio->pool = mp_audio_pool_create(mpctx->d_audio);
        mpctx->d_audio->afilter = af_new(mpctx->global);
        mpctx->d_audio->afilter->replaygain_data = sh->audio->replaygain_data;
        mpctx->d_audio->spdif_passthrough = true;
        mpctx->ao_buffer = mp_audio_buffer_create(NULL);
        if (!audio_init_best_codec(mpctx->d_audio))
            goto init_error;
        reset_audio_state(mpctx);

        if (mpctx->ao) {
            struct mp_audio fmt;
            ao_get_format(mpctx->ao, &fmt);
            mp_audio_buffer_reinit(mpctx->ao_buffer, &fmt);
        }
    }
    assert(mpctx->d_audio);

    struct mp_audio in_format = mpctx->d_audio->decode_format;

    if (!mp_audio_config_valid(&in_format)) {
        // We don't know the audio format yet - so configure it later as we're
        // resyncing. fill_audio_buffers() will call this function again.
        mpctx->sleeptime = 0;
        return;
    }

    // Weak gapless audio: drain AO on decoder format changes
    if (mpctx->ao_decoder_fmt && mpctx->ao && opts->gapless_audio < 0 &&
        !mp_audio_config_equals(mpctx->ao_decoder_fmt, &in_format))
    {
        uninit_audio_out(mpctx);
    }

    struct af_stream *afs = mpctx->d_audio->afilter;

    afs->output = (struct mp_audio){0};
    if (mpctx->ao) {
        ao_get_format(mpctx->ao, &afs->output);
    } else if (af_fmt_is_pcm(in_format.format)) {
        afs->output.rate = opts->force_srate;
        mp_audio_set_format(&afs->output, opts->audio_output_format);
        mp_audio_set_channels(&afs->output, &opts->audio_output_channels);
    }

    // filter input format: same as codec's output format:
    afs->input = in_format;

    // Determine what the filter chain outputs. recreate_audio_filters() also
    // needs this for testing whether playback speed is changed by resampling
    // or using a special filter.
    if (af_init(afs) < 0) {
        MP_ERR(mpctx, "Error at audio filter chain pre-init!\n");
        goto init_error;
    }

    if (!mpctx->ao) {
        bool spdif_fallback = af_fmt_is_spdif(afs->output.format) &&
                              mpctx->d_audio->spdif_passthrough;
        bool ao_null_fallback = opts->ao_null_fallback && !spdif_fallback;

        mp_chmap_remove_useless_channels(&afs->output.channels,
                                         &opts->audio_output_channels);
        mp_audio_set_channels(&afs->output, &afs->output.channels);

        mpctx->ao = ao_init_best(mpctx->global, ao_null_fallback, mpctx->input,
                                 mpctx->encode_lavc_ctx, afs->output.rate,
                                 afs->output.format, afs->output.channels);

        struct mp_audio fmt = {0};
        if (mpctx->ao)
            ao_get_format(mpctx->ao, &fmt);

        // Verify passthrough format was not changed.
        if (mpctx->ao && af_fmt_is_spdif(afs->output.format)) {
            if (!mp_audio_config_equals(&afs->output, &fmt)) {
                MP_ERR(mpctx, "Passthrough format unsupported.\n");
                ao_uninit(mpctx->ao);
                mpctx->ao = NULL;
            }
        }

        if (!mpctx->ao) {
            // If spdif was used, try to fallback to PCM.
            if (spdif_fallback) {
                mpctx->d_audio->spdif_passthrough = false;
                mpctx->d_audio->spdif_failed = true;
                if (!audio_init_best_codec(mpctx->d_audio))
                    goto init_error;
                reset_audio_state(mpctx);
                reinit_audio_chain(mpctx);
                return;
            }

            MP_ERR(mpctx, "Could not open/initialize audio device -> no sound.\n");
            mpctx->error_playing = MPV_ERROR_AO_INIT_FAILED;
            goto init_error;
        }

        mp_audio_buffer_reinit(mpctx->ao_buffer, &fmt);
        afs->output = fmt;
        if (!mp_audio_config_equals(&afs->output, &afs->filter_output))
            afs->initialized = 0;

        mpctx->ao_decoder_fmt = talloc(NULL, struct mp_audio);
        *mpctx->ao_decoder_fmt = in_format;

        MP_INFO(mpctx, "AO: [%s] %s\n", ao_get_name(mpctx->ao),
                mp_audio_config_to_str(&fmt));
        MP_VERBOSE(mpctx, "AO: Description: %s\n", ao_get_description(mpctx->ao));
        update_window_title(mpctx, true);
    }

    if (recreate_audio_filters(mpctx) < 0)
        goto init_error;

    update_playback_speed(mpctx);

    return;

init_error:
    uninit_audio_chain(mpctx);
    uninit_audio_out(mpctx);
no_audio:
    if (track)
        error_on_track(mpctx, track);
}

// Return pts value corresponding to the end point of audio written to the
// ao so far.
double written_audio_pts(struct MPContext *mpctx)
{
    struct dec_audio *d_audio = mpctx->d_audio;
    if (!d_audio)
        return MP_NOPTS_VALUE;

    struct mp_audio in_format = d_audio->decode_format;

    if (!mp_audio_config_valid(&in_format) || d_audio->afilter->initialized < 1)
        return MP_NOPTS_VALUE;

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
    if (d_audio->waiting)
        a_pts -= d_audio->waiting->samples / (double)in_format.rate;

    // Data buffered in audio filters, measured in seconds of "missing" output
    double buffered_output = af_calc_delay(d_audio->afilter);

    // Data that was ready for ao but was buffered because ao didn't fully
    // accept everything to internal buffers yet
    buffered_output += mp_audio_buffer_seconds(mpctx->ao_buffer);

    // Filters divide audio length by audio_speed, so multiply by it
    // to get the length in original units without speedup or slowdown
    a_pts -= buffered_output * mpctx->audio_speed;

    return a_pts;
}

// Return pts value corresponding to currently playing audio.
double playing_audio_pts(struct MPContext *mpctx)
{
    double pts = written_audio_pts(mpctx);
    if (pts == MP_NOPTS_VALUE || !mpctx->ao)
        return pts;
    return pts - mpctx->audio_speed * ao_get_delay(mpctx->ao);
}

static int write_to_ao(struct MPContext *mpctx, struct mp_audio *data, int flags)
{
    if (mpctx->paused)
        return 0;
    struct ao *ao = mpctx->ao;
    struct mp_audio out_format;
    ao_get_format(ao, &out_format);
#if HAVE_ENCODING
    encode_lavc_set_audio_pts(mpctx->encode_lavc_ctx, playing_audio_pts(mpctx));
#endif
    if (data->samples == 0)
        return 0;
    double real_samplerate = out_format.rate / mpctx->audio_speed;
    int played = ao_play(mpctx->ao, data->planes, data->samples, flags);
    assert(played <= data->samples);
    if (played > 0) {
        mpctx->shown_aframes += played;
        mpctx->delay += played / real_samplerate;
        mpctx->written_audio += played / (double)out_format.rate;
        return played;
    }
    return 0;
}

static void dump_audio_stats(struct MPContext *mpctx)
{
    if (!mp_msg_test(mpctx->log, MSGL_STATS))
        return;
    if (mpctx->audio_status != STATUS_PLAYING || !mpctx->ao || mpctx->paused) {
        mpctx->audio_stat_start = 0;
        return;
    }

    double delay = ao_get_delay(mpctx->ao);
    if (!mpctx->audio_stat_start) {
        mpctx->audio_stat_start = mp_time_us();
        mpctx->written_audio = delay;
    }
    double current_audio = mpctx->written_audio - delay;
    double current_time = (mp_time_us() - mpctx->audio_stat_start) / 1e6;
    MP_STATS(mpctx, "value %f ao-dev", current_audio - current_time);
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
    double play_samplerate = out_format.rate / mpctx->audio_speed;

    if (!opts->initial_audio_sync) {
        mpctx->audio_status = STATUS_FILLING;
        return true;
    }

    double written_pts = written_audio_pts(mpctx);
    if (written_pts == MP_NOPTS_VALUE && !mp_audio_buffer_samples(mpctx->ao_buffer))
        return false; // no audio read yet

    bool sync_to_video = mpctx->d_video && mpctx->sync_audio_to_video &&
                         mpctx->video_status != STATUS_EOF;

    double sync_pts = MP_NOPTS_VALUE;
    if (sync_to_video) {
        if (mpctx->video_status < STATUS_READY)
            return false; // wait until we know a video PTS
        if (mpctx->video_next_pts != MP_NOPTS_VALUE)
            sync_pts = mpctx->video_next_pts - opts->audio_delay;
    } else if (mpctx->hrseek_active) {
        sync_pts = mpctx->hrseek_pts;
    }
    if (sync_pts == MP_NOPTS_VALUE) {
        mpctx->audio_status = STATUS_FILLING;
        return true; // syncing disabled
    }

    double ptsdiff = written_pts - sync_pts;
    // Missing timestamp, or PTS reset, or just broken.
    if (written_pts == MP_NOPTS_VALUE || fabs(ptsdiff) > 3600) {
        MP_WARN(mpctx, "Failed audio resync.\n");
        mpctx->audio_status = STATUS_FILLING;
        return true;
    }

    int align = af_format_sample_alignment(out_format.format);
    *skip = (int)(-ptsdiff * play_samplerate) / align * align;
    return true;
}

void fill_audio_out_buffers(struct MPContext *mpctx, double endpts)
{
    struct MPOpts *opts = mpctx->opts;
    struct dec_audio *d_audio = mpctx->d_audio;

    dump_audio_stats(mpctx);

    if (mpctx->ao && ao_query_and_reset_events(mpctx->ao, AO_EVENT_RELOAD)) {
        ao_reset(mpctx->ao);
        uninit_audio_out(mpctx);
        if (d_audio) {
            if (mpctx->d_audio->spdif_failed) {
                mpctx->d_audio->spdif_failed = false;
                mpctx->d_audio->spdif_passthrough = true;
                if (!audio_init_best_codec(mpctx->d_audio)) {
                    MP_ERR(mpctx, "Error reinitializing audio.\n");
                    error_on_track(mpctx, mpctx->current_track[0][STREAM_AUDIO]);
                    return;
                }
            }
            mpctx->audio_status = STATUS_SYNCING;
        }
    }

    if (!d_audio)
        return;

    if (d_audio->afilter->initialized < 1 || !mpctx->ao) {
        // Probe the initial audio format. Returns AD_OK (and does nothing) if
        // the format is already known.
        int r = initial_audio_decode(mpctx->d_audio);
        if (r == AD_WAIT)
            return; // continue later when new data is available
        if (r != AD_OK) {
            mpctx->d_audio->init_retries += 1;
            if (mpctx->d_audio->init_retries >= 50) {
                MP_ERR(mpctx, "Error initializing audio.\n");
                error_on_track(mpctx, mpctx->current_track[0][STREAM_AUDIO]);
                return;
            }
        }
        reinit_audio_chain(mpctx);
        mpctx->sleeptime = 0;
        return; // try again next iteration
    }

    struct mp_audio out_format = {0};
    ao_get_format(mpctx->ao, &out_format);
    double play_samplerate = out_format.rate / mpctx->audio_speed;
    int align = af_format_sample_alignment(out_format.format);

    // If audio is infinitely fast, somehow try keeping approximate A/V sync.
    if (mpctx->audio_status == STATUS_PLAYING && ao_untimed(mpctx->ao) &&
        mpctx->video_status != STATUS_EOF && mpctx->delay > 0)
        return;

    int playsize = ao_get_space(mpctx->ao);

    int skip = 0;
    bool sync_known = get_sync_samples(mpctx, &skip);
    if (skip > 0) {
        playsize = MPMIN(skip + 1, MPMAX(playsize, 2500)); // buffer extra data
    } else if (skip < 0) {
        playsize = MPMAX(1, playsize + skip); // silence will be prepended
    }

    int skip_duplicate = 0; // >0: skip, <0: duplicate
    double drop_limit =
        (opts->sync_max_audio_change + opts->sync_max_video_change) / 100;
    if (mpctx->display_sync_active && opts->video_sync == VS_DISP_ADROP &&
        fabs(mpctx->last_av_difference) >= opts->sync_audio_drop_size &&
        mpctx->audio_drop_throttle < drop_limit &&
        mpctx->audio_status == STATUS_PLAYING)
    {
        int samples = ceil(opts->sync_audio_drop_size * play_samplerate);
        samples = (samples + align / 2) / align * align;

        skip_duplicate = mpctx->last_av_difference >= 0 ? -samples : samples;

        playsize = MPMAX(playsize, samples);

        mpctx->audio_drop_throttle += 1 - drop_limit - samples / play_samplerate;
    }

    playsize = playsize / align * align;

    int status = AD_OK;
    bool working = false;
    if (playsize > mp_audio_buffer_samples(mpctx->ao_buffer)) {
        status = audio_decode(d_audio, mpctx->ao_buffer, playsize);
        if (status == AD_WAIT)
            return;
        if (status == AD_NEW_FMT) {
            /* The format change isn't handled too gracefully. A more precise
             * implementation would require draining buffered old-format audio
             * while displaying video, then doing the output format switch.
             */
            if (mpctx->opts->gapless_audio < 1)
                uninit_audio_out(mpctx);
            reinit_audio_chain(mpctx);
            mpctx->sleeptime = 0;
            return; // retry on next iteration
        }
        if (status == AD_ERR)
            mpctx->sleeptime = 0;
        working = true;
    }

    // If EOF was reached before, but now something can be decoded, try to
    // restart audio properly. This helps with video files where audio starts
    // later. Retrying is needed to get the correct sync PTS.
    if (mpctx->audio_status >= STATUS_DRAINING && status == AD_OK) {
        mpctx->audio_status = STATUS_SYNCING;
        return; // retry on next iteration
    }

    bool end_sync = false;
    if (skip >= 0) {
        int max = mp_audio_buffer_samples(mpctx->ao_buffer);
        mp_audio_buffer_skip(mpctx->ao_buffer, MPMIN(skip, max));
        // If something is left, we definitely reached the target time.
        end_sync |= sync_known && skip < max;
    } else if (skip < 0) {
        if (-skip > playsize) { // heuristic against making the buffer too large
            ao_reset(mpctx->ao); // some AOs repeat data on underflow
            mpctx->audio_status = STATUS_DRAINING;
            mpctx->delay = 0;
            return;
        }
        mp_audio_buffer_prepend_silence(mpctx->ao_buffer, -skip);
        end_sync = true;
    }

    if (skip_duplicate) {
        int max = mp_audio_buffer_samples(mpctx->ao_buffer);
        if (abs(skip_duplicate) > max)
            skip_duplicate = skip_duplicate >= 0 ? max : -max;
        mpctx->last_av_difference += skip_duplicate / play_samplerate;
        if (skip_duplicate >= 0) {
            mp_audio_buffer_skip(mpctx->ao_buffer, skip_duplicate);
            MP_STATS(mpctx, "drop-audio");
        } else {
            mp_audio_buffer_duplicate(mpctx->ao_buffer, -skip_duplicate);
            MP_STATS(mpctx, "duplicate-audio");
        }
        MP_VERBOSE(mpctx, "audio skip_duplicate=%d\n", skip_duplicate);
    }

    if (mpctx->audio_status == STATUS_SYNCING) {
        if (end_sync)
            mpctx->audio_status = STATUS_FILLING;
        if (status != AD_OK && !mp_audio_buffer_samples(mpctx->ao_buffer))
            mpctx->audio_status = STATUS_EOF;
        if (working || end_sync)
            mpctx->sleeptime = 0;
        return; // continue on next iteration
    }

    assert(mpctx->audio_status >= STATUS_FILLING);

    // Even if we're done decoding and syncing, let video start first - this is
    // required, because sending audio to the AO already starts playback.
    if (mpctx->audio_status == STATUS_FILLING && mpctx->sync_audio_to_video &&
        mpctx->video_status <= STATUS_READY)
    {
        mpctx->audio_status = STATUS_READY;
        return;
    }

    bool audio_eof = status == AD_EOF;
    bool partial_fill = false;
    int playflags = 0;

    if (endpts != MP_NOPTS_VALUE) {
        double samples = (endpts - written_audio_pts(mpctx) - opts->audio_delay)
                         * play_samplerate;
        if (playsize > samples) {
            playsize = MPMAX((int)samples / align * align, 0);
            audio_eof = true;
            partial_fill = true;
        }
    }

    if (playsize > mp_audio_buffer_samples(mpctx->ao_buffer)) {
        playsize = mp_audio_buffer_samples(mpctx->ao_buffer);
        partial_fill = true;
    }

    audio_eof &= partial_fill;

    // With gapless audio, delay this to ao_uninit. There must be only
    // 1 final chunk, and that is handled when calling ao_uninit().
    if (audio_eof && !opts->gapless_audio)
        playflags |= AOPLAY_FINAL_CHUNK;

    struct mp_audio data;
    mp_audio_buffer_peek(mpctx->ao_buffer, &data);
    if (audio_eof || data.samples >= align)
        data.samples = data.samples / align * align;
    data.samples = MPMIN(data.samples, mpctx->paused ? 0 : playsize);
    int played = write_to_ao(mpctx, &data, playflags);
    assert(played >= 0 && played <= data.samples);
    mp_audio_buffer_skip(mpctx->ao_buffer, played);

    mpctx->audio_drop_throttle =
        MPMAX(0, mpctx->audio_drop_throttle - played / play_samplerate);

    dump_audio_stats(mpctx);

    mpctx->audio_status = STATUS_PLAYING;
    if (audio_eof && !playsize) {
        mpctx->audio_status = STATUS_DRAINING;
        // Wait until the AO has played all queued data. In the gapless case,
        // we trigger EOF immediately, and let it play asynchronously.
        if (ao_eof_reached(mpctx->ao) || opts->gapless_audio)
            mpctx->audio_status = STATUS_EOF;
    }
}

// Drop data queued for output, or which the AO is currently outputting.
void clear_audio_output_buffers(struct MPContext *mpctx)
{
    if (mpctx->ao)
        ao_reset(mpctx->ao);
    if (mpctx->ao_buffer)
        mp_audio_buffer_clear(mpctx->ao_buffer);
}
