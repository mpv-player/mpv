/*
 * null audio output driver
 *
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

#include "talloc.h"

#include "config.h"
#include "osdep/timer.h"
#include "audio/format.h"
#include "ao.h"

struct priv {
    double last_time;
    // All values are in samples
    float buffered;
    int buffersize;
    int outburst;
};

static void drain(struct ao *ao)
{
    struct priv *priv = ao->priv;

    double now = mp_time_sec();
    priv->buffered -= (now - priv->last_time) * ao->samplerate;
    if (priv->buffered < 0)
        priv->buffered = 0;
    priv->last_time = now;
}

static int init(struct ao *ao)
{
    struct priv *priv = talloc_zero(ao, struct priv);
    ao->priv = priv;

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_any(&sel);
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        return -1;

    // Minimal unit of audio samples that can be written at once. If play() is
    // called with sizes not aligned to this, a rounded size will be returned.
    // (This is not needed by the AO API, but many AOs behave this way.)
    priv->outburst = 256;

    // A "buffer" for about 0.2 seconds of audio
    int bursts = (int)(ao->samplerate * 0.2 + 1) / priv->outburst;
    priv->buffersize = priv->outburst * bursts;

    priv->last_time = mp_time_sec();

    return 0;
}

// close audio device
static void uninit(struct ao *ao, bool cut_audio)
{
}

// stop playing and empty buffers (for seeking/pause)
static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;
    priv->buffered = 0;
}

// stop playing, keep buffers (for pause)
static void pause(struct ao *ao)
{
    // for now, just call reset();
    reset(ao);
}

// resume playing, after audio_pause()
static void resume(struct ao *ao)
{
}

static int get_space(struct ao *ao)
{
    struct priv *priv = ao->priv;

    drain(ao);
    return priv->buffersize - priv->buffered;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *priv = ao->priv;

    int maxbursts = (priv->buffersize - priv->buffered) / priv->outburst;
    int playbursts = samples / priv->outburst;
    int bursts = playbursts > maxbursts ? maxbursts : playbursts;
    priv->buffered += bursts * priv->outburst;
    return bursts * priv->outburst;
}

static float get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;

    drain(ao);
    return priv->buffered / (double)ao->samplerate;
}

const struct ao_driver audio_out_null = {
    .description = "Null audio output",
    .name      = "null",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = pause,
    .resume    = resume,
};

