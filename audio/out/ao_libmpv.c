/*
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/common.h>

#include "mpv_talloc.h"

#include "ao.h"
#include "ao_libmpv.h"
#include "audio/format.h"
#include "common/msg.h"
#include "internal.h"
#include "options/m_option.h"
#include "osdep/timer.h"

struct priv {
    bool init;
};

int audio_callback(struct ao *ao, void *buffer, int len)
{
    struct priv *priv;

    if (ao == NULL || ao->priv == NULL)
        return -4;

    priv = ao->priv;

    /* Audio callback not initialised or selected. The libmpv audio output
     * driver must be selected prior to the calling of audio_configure and
     * audio_callback.
     */
    if (priv->init == false)
    {
        MP_ERR(ao, "libmpv audio output not initialized\n");
        return -4;
    }

    if (ao->format == AF_FORMAT_UNKNOWN)
    {
        MP_ERR(ao, "format not selected for audio buffer\n");
        return -10;
    }

    if (len % ao->sstride) {
        MP_ERR(ao, "audio callback not sample aligned. Len: %d, Sample Size: %d\n",
                len, ao->sstride);
    }

    // Time this buffer will take, plus assume 1 period (1 callback invocation)
    // fixed latency.
    double delay = 2 * len / (double)ao->bps;

    return ao_read_data(ao, &buffer, len / ao->sstride,
            mp_time_us() + 1000000LL * delay);
}

static void uninit(struct ao *ao)
{
    struct priv *priv = ao->priv;
    priv->init = false;
    return;
}

static int init(struct ao *ao)
{
    struct priv *priv = ao->priv;
    priv->init = true;

    /* Required as planar audio causes arithmetic exceptions in pull API. */
    ao->format = af_fmt_from_planar(ao->format);

    struct mp_chmap_sel sel = {0};
    mp_chmap_sel_add_waveext_def(&sel);

    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        mp_chmap_from_channels(&ao->channels, 2);

    return 1;
}

static void resume (struct ao *ao)
{
    return;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_audio_cb = {
    .description = "Audio callback for libmpv",
    .name      = "libmpv",
    .init      = init,
    .uninit    = uninit,
    .resume    = resume,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .init = false,
    },
    .options = NULL,
    .options_prefix = NULL,
};
