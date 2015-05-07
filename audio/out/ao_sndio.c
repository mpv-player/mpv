/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
 * Copyright (c) 2013 Christian Neukirchen <chneukirchen@gmail.com>
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
#define SILENCE_NMAX 0x1000
    char silence[SILENCE_NMAX];
    struct pollfd *pfd;
    char *dev;
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
        vol->left = vol->right = p->vol * 100 / SIO_MAXVOL;
        break;
    case AOCONTROL_SET_VOLUME:
        if (!p->havevol)
            return CONTROL_FALSE;
        sio_setvol(p->hdl, vol->left * SIO_MAXVOL / 100);
        break;
    case AOCONTROL_HAS_SOFT_VOLUME:
        return CONTROL_TRUE;
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
    {1, {MP_SPEAKER_ID_FL}},                    // mono
    MP_CHMAP2(FL, FR),                          // stereo
    {0},                                        // 2.1
    MP_CHMAP4(FL, FR, BL, BR),                  // 4.0
    {0},                                        // 5.0
    MP_CHMAP6(FL, FR, BL, BR, FC, LFE),         // 5.1
    {0},                                        // 6.1
    MP_CHMAP8(FL, FR, BL, BR, FC, LFE, SL, SR), // 7.1
    /* above is the fixed channel assignment for sndio, since we need to fill
       all channels and cannot insert silence, not all layouts are supported.
       NOTE: MP_SPEAKER_ID_NA could be used to add padding channels. Keep
             in mind that they don't actually contain silence. */
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
        {AF_FORMAT_S8,   8, 1},
        {AF_FORMAT_U16, 16, 0},
        {AF_FORMAT_S16, 16, 1},
        {AF_FORMAT_U24, 24, 0},
        {AF_FORMAT_S24, 24, 1},
        {AF_FORMAT_U32, 32, 0},
        {AF_FORMAT_S32, 32, 1},
    };
    const struct af_to_par *ap;
    int i;

    p->hdl = sio_open(p->dev, SIO_PLAY, 0);
    if (p->hdl == NULL) {
        MP_ERR(ao, "can't open sndio %s\n", p->dev);
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
    p->par.appbufsz = p->par.rate * 250 / 1000;    /* 250ms buffer */
    p->par.round = p->par.rate * 10 / 1000;    /*  10ms block size */
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
    if (p->par.bits == 8 && p->par.bps == 1) {
        ao->format = p->par.sig ? AF_FORMAT_S8 : AF_FORMAT_U8;
    } else if (p->par.bits == 16 && p->par.bps == 2) {
        ao->format = p->par.sig ? AF_FORMAT_S16 : AF_FORMAT_U16;
    } else if ((p->par.bits == 24 || p->par.msb) && p->par.bps == 3) {
        ao->format = p->par.sig ? AF_FORMAT_S24 : AF_FORMAT_U24;
    } else if ((p->par.bits == 32 || p->par.msb) && p->par.bps == 4) {
        ao->format = p->par.sig ? AF_FORMAT_S32 : AF_FORMAT_U32;
    } else {
        MP_ERR(ao, "couldn't set format\n");
        goto error;
    }

    p->havevol = sio_onvol(p->hdl, volcb, p);
    sio_onmove(p->hdl, movecb, p);
    if (!sio_start(p->hdl))
        MP_ERR(ao, "init: couldn't start\n");

    p->pfd = calloc (sio_nfds(p->hdl), sizeof (struct pollfd));
    if (!p->pfd)
        goto error;

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
}

/*
 * stop playing and empty buffers (for seeking/pause)
 */
static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->playing) {
        MP_WARN(ao, "Blocking until remaining audio is played... (sndio design bug).\n");

        p->playing = false;

        if (!sio_stop(p->hdl))
            MP_ERR(ao, "reset: couldn't stop\n");
        p->delay = 0;
        if (!sio_start(p->hdl))
            MP_ERR(ao, "reset: couldn't start\n");
    }
}

/*
 * play given number of samples until sio_write() blocks
 */
static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *p = ao->priv;
    int n;

    n = sio_write(p->hdl, data[0], samples * ao->sstride) / ao->sstride;
    p->delay += n;
    p->playing = true;
    /* on AOPLAY_FINAL_CHUNK, just let it underrun */
    return n;
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

/*
 * how many samples can be played without blocking
 */
static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;

    update(ao);

    int samples = p->par.bufsz - p->delay;
    return samples / p->par.round * p->par.round;
}

/*
 * return: delay in seconds between first and last sample in buffer
 */
static double get_delay(struct ao *ao)
{
    struct priv *p = ao->priv;

    update(ao);

    return p->delay / (double)p->par.rate;
}

/*
 * stop playing, keep buffers (for pause)
 */
static void audio_pause(struct ao *ao)
{
    reset(ao);
}

/*
 * resume playing, after audio_pause()
 */
static void audio_resume(struct ao *ao)
{
    return;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_sndio = {
    .description = "sndio audio output",
    .name      = "sndio",
    .init      = init,
    .uninit    = uninit,
    .control   = control,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .reset     = reset,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_STRING("device", dev, 0, OPTDEF_STR(SIO_DEVANY)),
        {0}
    },
};
