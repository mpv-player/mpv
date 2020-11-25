/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
 * Copyright (c) 2013 Christian Neukirchen <chneukirchen@gmail.com>
 * Copyright (c) 2020 Érico Nogueira Rolim <ericonr@disroot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <sys/types.h>
#include <poll.h>
#include <errno.h>
#include <sndio.h>

#include "options/m_option.h"
#include "common/msg.h"

#include "audio/format.h"
#include "ao.h"
#include "internal.h"

struct priv {
    /* sndio control and interaction */
    struct sio_hdl *hdl;
    struct sio_par par;
    struct pollfd *pfd;
    /* temporary buffer for audio written before audio_start() */
    unsigned char *buffer;
    int buffer_pos;
    /* variable to keep track of hardware position */
    int delay;
    /* volume control */
    int vol;
    bool havevol;
    /* current state */
    bool playing;
};

/*
 * misc parameters (volume, etc...)
 */
static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    ao_control_vol_t *vol = arg;

    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        if (!p->havevol)
            return CONTROL_FALSE;
        vol->left = vol->right = p->vol * 100 / (float)SIO_MAXVOL;
        break;
    case AOCONTROL_SET_VOLUME:
        if (!p->havevol)
            return CONTROL_FALSE;
        sio_setvol(p->hdl, vol->left * SIO_MAXVOL / 100);
        break;
    default:
        return CONTROL_UNKNOWN;
    }
    return CONTROL_OK;
}

/*
 * call-back invoked to notify of the hardware position
 */
static void movecb(void *addr, int delta)
{
    struct priv *p = addr;
    p->delay -= delta;
}

/*
 * call-back invoked to notify about volume changes
 */
static void volcb(void *addr, unsigned newvol)
{
    struct priv *p = addr;
    p->vol = newvol;
}

static const struct mp_chmap sndio_layouts[MP_NUM_CHANNELS + 1] = {
    {0},                                        // empty
    MP_CHMAP_INIT_MONO,                         // mono
    MP_CHMAP_INIT_STEREO,                       // stereo
    {0},                                        // 2.1
    MP_CHMAP4(FL, FR, BL, BR),                  // 4.0
    {0},                                        // 5.0
    MP_CHMAP6(FL, FR, BL, BR, FC, LFE),         // 5.1
    {0},                                        // 6.1
    MP_CHMAP8(FL, FR, BL, BR, FC, LFE, SL, SR), // 7.1
    /* above is the fixed channel assignment for sndio, since we need to fill
       all channels and cannot insert silence, not all layouts are supported.
       NOTE: MP_SPEAKER_ID_NA could be used to add padding channels. */
};

/*
 * open device and setup parameters
 * return: 0=success -1=fail
 */
static int init(struct ao *ao)
{
    struct priv *p = ao->priv;

    struct af_to_par {
        int format, bits, sig;
    };
    static const struct af_to_par af_to_par[] = {
        {AF_FORMAT_U8,   8, 0},
        {AF_FORMAT_S16, 16, 1},
        {AF_FORMAT_S32, 32, 1},
    };
    const struct af_to_par *ap;
    int i;

    /* support audio device defined in the command line */
    const char *dev_name = ao->device ? ao->device : SIO_DEVANY;
    p->hdl = sio_open(dev_name, SIO_PLAY, 0);
    if (p->hdl == NULL) {
        MP_ERR(ao, "can't open sndio device '%s'\n", dev_name);
        goto error;
    }

    ao->format = af_fmt_from_planar(ao->format);

    sio_initpar(&p->par);
    for (i = 0, ap = af_to_par;; i++, ap++) {
        if (i == sizeof(af_to_par) / sizeof(struct af_to_par)) {
            MP_VERBOSE(ao, "unsupported format\n");
            p->par.bits = 16;
            p->par.sig = 1;
            p->par.le = SIO_LE_NATIVE;
            break;
        }
        if (ap->format == ao->format) {
            p->par.bits = ap->bits;
            p->par.sig = ap->sig;
            if (ap->bits > 8)
                p->par.le = SIO_LE_NATIVE;
            if (ap->bits != SIO_BPS(ap->bits))
                p->par.bps = ap->bits / 8;
            break;
        }
    }
    p->par.rate = ao->samplerate;

    struct mp_chmap_sel sel = {0};
    for (int n = 0; n < MP_NUM_CHANNELS+1; n++)
        mp_chmap_sel_add_map(&sel, &sndio_layouts[n]);

    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        goto error;

    p->par.pchan = ao->channels.num;
    p->par.round = p->par.rate * 10 / 1000; /*  10ms block size */
    /* other parameters are left for libsndio to decide */

    if (!sio_setpar(p->hdl, &p->par)) {
        MP_ERR(ao, "couldn't set params\n");
        goto error;
    }
    if (!sio_getpar(p->hdl, &p->par)) {
        MP_ERR(ao, "couldn't get params\n");
        goto error;
    }
    if (p->par.bps > 1 && p->par.le != SIO_LE_NATIVE) {
        MP_ERR(ao, "swapped endian output not supported\n");
        goto error;
    }
    if (p->par.bits == 8 && p->par.bps == 1 && !p->par.sig) {
        ao->format = AF_FORMAT_U8;
    } else if (p->par.bits == 16 && p->par.bps == 2 && p->par.sig) {
        ao->format = AF_FORMAT_S16;
    } else if ((p->par.bits == 32 || p->par.msb) && p->par.bps == 4 && p->par.sig) {
        ao->format = AF_FORMAT_S32;
    } else {
        MP_ERR(ao, "couldn't set format\n");
        goto error;
    }

    p->havevol = sio_onvol(p->hdl, volcb, p);
    sio_onmove(p->hdl, movecb, p);

    p->pfd = calloc (sio_nfds(p->hdl), sizeof (struct pollfd));
    if (!p->pfd)
        goto error;

    ao->device_buffer = p->par.bufsz;

    p->buffer = NULL;
    p->playing = false;

    return 0;

error:
    if (p->hdl)
      sio_close(p->hdl);

    return -1;
}

/*
 * close device
 */
static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->hdl)
        sio_close(p->hdl);

    free(p->pfd);
    free(p->buffer);
}

/*
 * stop playing and empty buffers (for seeking/pause)
 */
static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->playing) {
        MP_WARN(ao, "Blocking until remaining audio is played... (sndio design bug).\n");

        /* reset needs to bring the driver back to the state right after init,
         * so we shouldn't call sio_start() in it */
        if (!sio_stop(p->hdl))
            MP_ERR(ao, "reset: couldn't stop\n");

        p->playing = false;
        p->delay = 0;
    }

    /* discard temporary buffer directly */
    p->buffer_pos = 0;
}

static void audio_start(struct ao *ao)
{
    struct priv *p = ao->priv;
    int n;

    /* don't need to do anything if already playing */
    if (p->playing)
        return;

    if (!sio_start(p->hdl)) {
        MP_ERR(ao, "start: couldn't start\n");
        return;
    }

    p->playing = true;

    /* if temporary buffer was used, flush it */
    if (p->buffer) {
        int buffer_old = p->buffer_pos;
        while (p->buffer_pos > 0) {
            n = sio_write(
                    p->hdl,
                    p->buffer + ao->sstride * (buffer_old - p->buffer_pos),
                    p->buffer_pos * ao->sstride
                ) / ao->sstride;

            if (sio_eof(p->hdl)) {
                MP_WARN(ao, "start: couldn't flush temporary buffer\n");
                p->buffer_pos = 0;
                return;
            }

            p->buffer_pos -= n;
        }
    }

}

static bool audio_write(struct ao *ao, void **data, int samples)
{
    struct priv *p = ao->priv;
    int n;

    /* if audio_start has been called, write directly to sndio,
     * otherwise store in temporary buffer */
    if (p->playing) {
        n = sio_write(p->hdl, data[0], samples * ao->sstride) / ao->sstride;
        p->delay += n;
    } else {
        /* allocate the temporary buffer on demand */
        if (!p->buffer) {
            p->buffer_pos = 0;
            p->buffer = malloc(ao->device_buffer * ao->sstride);

            if (!p->buffer) {
                MP_ERR(ao, "write: couldn't allocate temporary buffer\n");
                return false;
            }
        }

        if ((p->buffer_pos + samples) > ao->device_buffer) {
            MP_ERR(ao, "write: temporary buffer full\n");
            return false;
        }

        memcpy(p->buffer + (p->buffer_pos * ao->sstride), data[0], samples * ao->sstride);
        p->buffer_pos += samples;
        /* it's as if the buffer is getting occupied */
        p->delay += samples;

        /* at this point, always succeeds */
        return true;
    }

    /* on AOPLAY_FINAL_CHUNK, just let it underrun */
    return !sio_eof(p->hdl);
}

/*
 * make libsndio call movecb()
 */
static void update(struct ao *ao)
{
    struct priv *p = ao->priv;
    int n = sio_pollfd(p->hdl, p->pfd, POLLOUT);
    while (poll(p->pfd, n, 0) < 0 && errno == EINTR) {}
    sio_revents(p->hdl, p->pfd);
}


static void get_state(struct ao *ao, struct mp_pcm_state *state)
{
    struct priv *p = ao->priv;

    update(ao);

    /* delay in seconds between first and last sample in buffer */
    state->delay = p->delay / (double)p->par.rate;
    /* how many samples we can play without blocking, rounded to best size */
    state->free_samples = (p->par.bufsz - p->delay) / p->par.round * p->par.round;
    /* how many samples are already in the buffer to be played */
    state->queued_samples = p->delay;

    state->playing = p->playing;
}

const struct ao_driver audio_out_sndio = {
    .description = "sndio audio output",
    .name      = "sndio",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .get_state = get_state,
    .start     = audio_start,
    .write     = audio_write,
    .reset     = reset,
    /* no need for .set_pause, since core emulates that with .reset */
    .priv_size = sizeof(struct priv),
};
