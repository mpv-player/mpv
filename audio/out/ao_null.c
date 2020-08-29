/*
 * null audio output driver
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

/*
 * Note: this does much more than just ignoring audio output. It simulates
 *       (to some degree) an ideal AO.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mpv_talloc.h"

#include "config.h"
#include "osdep/timer.h"
#include "options/m_option.h"
#include "common/common.h"
#include "common/msg.h"
#include "audio/format.h"
#include "ao.h"
#include "internal.h"

struct priv {
    bool paused;
    double last_time;
    float buffered;     // samples
    int buffersize;     // samples
    bool playing;

    int untimed;
    float bufferlen;    // seconds
    float speed;        // multiplier
    float latency_sec;  // seconds
    float latency;      // samples
    int broken_eof;
    int broken_delay;

    // Minimal unit of audio samples that can be written at once. If play() is
    // called with sizes not aligned to this, a rounded size will be returned.
    // (This is not needed by the AO API, but many AOs behave this way.)
    int outburst;       // samples

    struct m_channels channel_layouts;
    int format;
};

static void drain(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (ao->untimed) {
        priv->buffered = 0;
        return;
    }

    if (priv->paused)
        return;

    double now = mp_time_sec();
    if (priv->buffered > 0) {
        priv->buffered -= (now - priv->last_time) * ao->samplerate * priv->speed;
        if (priv->buffered < 0)
            priv->buffered = 0;
    }
    priv->last_time = now;
}

static int init(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (priv->format)
        ao->format = priv->format;

    ao->untimed = priv->untimed;

    struct mp_chmap_sel sel = {.tmp = ao};
    if (priv->channel_layouts.num_chmaps) {
        for (int n = 0; n < priv->channel_layouts.num_chmaps; n++)
            mp_chmap_sel_add_map(&sel, &priv->channel_layouts.chmaps[n]);
    } else {
        mp_chmap_sel_add_any(&sel);
    }
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        mp_chmap_from_channels(&ao->channels, 2);

    priv->latency = priv->latency_sec * ao->samplerate;

    // A "buffer" for this many seconds of audio
    int bursts = (int)(ao->samplerate * priv->bufferlen + 1) / priv->outburst;
    ao->device_buffer = priv->outburst * bursts + priv->latency;

    priv->last_time = mp_time_sec();

    return 0;
}

// close audio device
static void uninit(struct ao *ao)
{
}

// stop playing and empty buffers (for seeking/pause)
static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;
    priv->buffered = 0;
    priv->playing = false;
}

static void start(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (priv->paused)
        MP_ERR(ao, "illegal state: start() while paused\n");

    drain(ao);
    priv->paused = false;
    priv->last_time = mp_time_sec();
    priv->playing = true;
}

static bool set_pause(struct ao *ao, bool paused)
{
    struct priv *priv = ao->priv;

    if (!priv->playing)
        MP_ERR(ao, "illegal state: set_pause() while not playing\n");

    if (priv->paused != paused) {

        drain(ao);
        priv->paused = paused;
        if (!priv->paused)
            priv->last_time = mp_time_sec();
    }

    return true;
}

static bool audio_write(struct ao *ao, void **data, int samples)
{
    struct priv *priv = ao->priv;

    if (priv->buffered <= 0)
        priv->buffered = priv->latency; // emulate fixed latency

    priv->buffered += samples;
    return true;
}

static void get_state(struct ao *ao, struct mp_pcm_state *state)
{
    struct priv *priv = ao->priv;

    drain(ao);

    state->free_samples = ao->device_buffer - priv->latency - priv->buffered;
    state->free_samples = state->free_samples / priv->outburst * priv->outburst;
    state->queued_samples = priv->buffered;

    // Note how get_state returns the delay in audio device time (instead of
    // adjusting for speed), since most AOs seem to also do that.
    state->delay = priv->buffered;

    // Drivers with broken EOF handling usually always report the same device-
    // level delay that is additional to the buffer time.
    if (priv->broken_eof && priv->buffered < priv->latency)
        state->delay = priv->latency;

    state->delay /= ao->samplerate;

    if (priv->broken_delay) { // Report only multiples of outburst
        double q = priv->outburst / (double)ao->samplerate;
        if (state->delay > 0)
            state->delay = (int)(state->delay / q) * q;
    }

    state->playing = priv->playing && priv->buffered > 0;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_null = {
    .description = "Null audio output",
    .name      = "null",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_state = get_state,
    .set_pause = set_pause,
    .write     = audio_write,
    .start     = start,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .bufferlen = 0.2,
        .outburst = 256,
        .speed = 1,
    },
    .options = (const struct m_option[]) {
        {"untimed", OPT_FLAG(untimed)},
        {"buffer", OPT_FLOAT(bufferlen), M_RANGE(0, 100)},
        {"outburst", OPT_INT(outburst), M_RANGE(1, 100000)},
        {"speed", OPT_FLOAT(speed), M_RANGE(0, 10000)},
        {"latency", OPT_FLOAT(latency_sec), M_RANGE(0, 100)},
        {"broken-eof", OPT_FLAG(broken_eof)},
        {"broken-delay", OPT_FLAG(broken_delay)},
        {"channel-layouts", OPT_CHANNELS(channel_layouts)},
        {"format", OPT_AUDIOFORMAT(format)},
        {0}
    },
    .options_prefix = "ao-null",
};
