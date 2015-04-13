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

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#include <sndio.h>
#include "audio_in.h"
#include "common/msg.h"

int ai_sndio_setup(audio_in_t *ai)
{
    struct sio_par par;

    sio_initpar(&par);

    par.bits = 16;
    par.sig = 1;
    par.le = SIO_LE_NATIVE;
    par.rchan = ai->req_channels;
    par.rate = ai->req_samplerate;
    par.appbufsz = ai->req_samplerate;  /* 1 sec */

   if (!sio_setpar(ai->sndio.hdl, &par) || !sio_getpar(ai->sndio.hdl, &par)) {
        MP_ERR(ai, "could not configure sndio audio");
        return -1;
    }

    ai->channels = par.rchan;
    ai->samplerate = par.rate;
    ai->samplesize = par.bits;
    ai->bytes_per_sample = par.bps;
    ai->blocksize = par.round * par.bps;

    return 0;
}

int ai_sndio_init(audio_in_t *ai)
{
    int err;

    if ((ai->sndio.hdl = sio_open(ai->sndio.device, SIO_REC, 0)) == NULL) {
        MP_ERR(ai, "could not open sndio audio");
        return -1;
    }

    err = ai_sndio_setup(ai);

    return err;
}
