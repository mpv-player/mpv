/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
 * Copyright (c) 2013 Christian Neukirchen <chneukirchen@gmail.com>
 * Copyright (c) 2020 Rozhuk Ivan <rozhuk.im@gmail.com>
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
    struct sio_hdl *hdl;
    struct sio_par par;
    int delay;
    bool playing;
    int vol;
    int havevol;
    struct pollfd *pfd;
};


static const struct mp_chmap sndio_layouts[MP_NUM_CHANNELS + 1] = {
    {0},                                        /* empty */
    {1, {MP_SPEAKER_ID_FL}},                    /* mono */
    MP_CHMAP2(FL, FR),                          /* stereo */
    {0},                                        /* 2.1 */
    MP_CHMAP4(FL, FR, BL, BR),                  /* 4.0 */
    {0},                                        /* 5.0 */
    MP_CHMAP6(FL, FR, BL, BR, FC, LFE),         /* 5.1 */
    {0},                                        /* 6.1 */
    MP_CHMAP8(FL, FR, BL, BR, FC, LFE, SL, SR), /* 7.1 */
    /* Above is the fixed channel assignment for sndio, since we need to
     * fill all channels and cannot insert silence, not all layouts are
     * supported.
     * NOTE: MP_SPEAKER_ID_NA could be used to add padding channels. */
};

static void uninit(struct ao *ao);


/* Make libsndio call movecb(). */
static void process_events(struct ao *ao)
{
    struct priv *p = ao->priv;
    
    if (!p->playing)
        return;
    int n = sio_pollfd(p->hdl, p->pfd, POLLOUT);
    while (poll(p->pfd, n, 0) < 0 && errno == EINTR) {}

    sio_revents(p->hdl, p->pfd);
}

/* Call-back invoked to notify of the hardware position. */
static void movecb(void *addr, int delta)
{
    struct ao *ao = addr;
    struct priv *p = ao->priv;

    p->delay -= delta;
}

/* Call-back invoked to notify about volume changes. */
static void volcb(void *addr, unsigned newvol)
{
    struct ao *ao = addr;
    struct priv *p = ao->priv;

    p->vol = newvol;
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct mp_chmap_sel sel = {0};
    size_t i;
    struct af_to_par {
        int format, bits, sig;
    };
    static const struct af_to_par af_to_par[] = {
        {AF_FORMAT_U8,   8, 0},
        {AF_FORMAT_S16, 16, 1},
        {AF_FORMAT_S32, 32, 1},
    };
    const struct af_to_par *ap;
    const char *device = ((ao->device) ? ao->device : SIO_DEVANY);

    /* Opening device. */
    MP_VERBOSE(ao, "Using '%s' audio device.\n", device);
    p->hdl = sio_open(device, SIO_PLAY, 0);
    if (p->hdl == NULL) {
        MP_ERR(ao, "Can't open audio device %s.\n", device);
        goto err_out;
    }

    sio_initpar(&p->par);

    /* Selecting sound format. */
    ao->format = af_fmt_from_planar(ao->format);
    for (i = 0, ap = af_to_par;; i++, ap++) {
        if (i == MP_ARRAY_SIZE(af_to_par)) {
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

    /* Channels count. */
    for (i = 0; i < MP_ARRAY_SIZE(sndio_layouts); i++) {
        mp_chmap_sel_add_map(&sel, &sndio_layouts[i]);
    }
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        goto err_out;

    p->par.pchan = ao->channels.num;
#ifdef __FreeBSD__
    /* OSS wrapper have bad defaults, overwrite it. */
    p->par.appbufsz = ((p->par.rate * 25) / 1000); /* 25 ms. */
#endif
    if (!sio_setpar(p->hdl, &p->par)) {
        MP_ERR(ao, "couldn't set params\n");
        goto err_out;
    }

    /* Get current sound params. */
    if (!sio_getpar(p->hdl, &p->par)) {
        MP_ERR(ao, "couldn't get params\n");
        goto err_out;
    }
    if (p->par.bps > 1 && p->par.le != SIO_LE_NATIVE) {
        MP_ERR(ao, "swapped endian output not supported\n");
        goto err_out;
    }

    /* Update sound params. */
    if (p->par.bits == 8 && p->par.bps == 1 && !p->par.sig) {
        ao->format = AF_FORMAT_U8;
    } else if (p->par.bits == 16 && p->par.bps == 2 && p->par.sig) {
        ao->format = AF_FORMAT_S16;
    } else if ((p->par.bits == 32 || p->par.msb) && p->par.bps == 4 && p->par.sig) {
        ao->format = AF_FORMAT_S32;
    } else {
        MP_ERR(ao, "couldn't set format\n");
        goto err_out;
    }

    p->havevol = sio_onvol(p->hdl, volcb, ao);
    sio_onmove(p->hdl, movecb, ao);

    p->pfd = calloc(sio_nfds(p->hdl), sizeof(struct pollfd));
    if (!p->pfd)
        goto err_out;

    ao->device_buffer = p->par.bufsz;
    MP_VERBOSE(ao, "bufsz = %i, appbufsz = %i, round = %i\n",
        p->par.bufsz, p->par.appbufsz, p->par.round);

    p->delay = 0;
    p->playing = false;
    if (!sio_start(p->hdl)) {
        MP_ERR(ao, "start: sio_start() fail.\n");
        goto err_out;
    }

    return 0;

err_out:
    uninit(ao);
    return -1;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->hdl) {
        sio_close(p->hdl);
        p->hdl = NULL;
    }
    free(p->pfd);
    p->pfd = NULL;
    p->playing = false;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;
    ao_control_vol_t *vol = arg;

    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        if (!p->havevol)
            return CONTROL_FALSE;
        vol->left = vol->right = p->vol * 100 / SIO_MAXVOL;
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

static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;

    process_events(ao);
    p->delay = 0;
    p->playing = false;

    /* XXX: some times may block here then sndiod used. */
    if (!sio_stop(p->hdl)) {
        MP_ERR(ao, "reset: couldn't sio_stop()\n");
reinit:
        /* Without this device will never work again. */
        MP_WARN(ao, "Force reinitialize audio device.\n");
        uninit(ao);
        init(ao);
        return;
    }
    if (!sio_start(p->hdl)) {
        MP_ERR(ao, "reset: sio_start() fail.\n");
        goto reinit;
    }
}

static void start(struct ao *ao)
{
    struct priv *p = ao->priv;

    p->playing = true;
    process_events(ao);
}

static bool audio_write(struct ao *ao, void **data, int samples)
{
    struct priv *p = ao->priv;
    const size_t size = (samples * ao->sstride);
    size_t rc;

    rc = sio_write(p->hdl, data[0], size);
    if (rc != size) {
        MP_WARN(ao, "audio_write: unexpected partial write: required: %zu, written: %zu.\n",
            size, rc);
        reset(ao);
        p->playing = false;
        return false;
    }
    p->delay += samples;
    process_events(ao);

    return true;
}

static void get_state(struct ao *ao, struct mp_pcm_state *state)
{
    struct priv *p = ao->priv;

    process_events(ao);

    state->free_samples = (ao->device_buffer - p->delay);
    state->queued_samples = p->delay;
    state->delay = (p->delay / (double)p->par.rate);
    state->playing = p->playing;
}

const struct ao_driver audio_out_sndio = {
    .name      = "sndio",
    .description = "sndio audio output",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .reset     = reset,
    .start     = start,
    .write     = audio_write,
    .get_state = get_state,
    .priv_size = sizeof(struct priv),
};
