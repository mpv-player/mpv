/*
 * Original author: Albeu
 *
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

#include "config.h"

#include <cdio/cdio.h>

#if CDIO_API_VERSION < 6
#define OLD_API
#endif

#ifdef OLD_API
#include <cdio/cdda.h>
#include <cdio/paranoia.h>
#else
#include <cdio/paranoia/cdda.h>
#include <cdio/paranoia/paranoia.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "mpv_talloc.h"

#include "stream.h"
#include "options/m_option.h"
#include "options/m_config.h"
#include "options/options.h"

#include "common/msg.h"

#include "config.h"
#if !HAVE_GPL
#error GPL only
#endif

typedef struct cdda_params {
    cdrom_drive_t *cd;
    cdrom_paranoia_t *cdp;
    int sector;
    int start_sector;
    int end_sector;
    uint8_t *data;
    size_t data_pos;

    // options
    int speed;
    int paranoia_mode;
    int sector_size;
    int search_overlap;
    int toc_bias;
    int toc_offset;
    int skip;
    char *device;
    int span[2];
    int cdtext;
} cdda_priv;

#define OPT_BASE_STRUCT struct cdda_params
const struct m_sub_options stream_cdda_conf = {
    .opts = (const m_option_t[]) {
        {"speed", OPT_INT(speed), M_RANGE(1, 100)},
        {"paranoia", OPT_INT(paranoia_mode), M_RANGE(0, 2)},
        {"sector-size", OPT_INT(sector_size), M_RANGE(1, 100)},
        {"overlap", OPT_INT(search_overlap), M_RANGE(0, 75)},
        {"toc-bias", OPT_INT(toc_bias)},
        {"toc-offset", OPT_INT(toc_offset)},
        {"skip", OPT_FLAG(skip)},
        {"span-a", OPT_INT(span[0])},
        {"span-b", OPT_INT(span[1])},
        {"cdtext", OPT_FLAG(cdtext)},
        {"span", OPT_REMOVED("use span-a/span-b")},
        {0}
    },
    .size = sizeof(struct cdda_params),
    .defaults = &(const struct cdda_params){
        .search_overlap = -1,
        .skip = 1,
    },
};

static const char *const cdtext_name[] = {
#ifdef OLD_API
    [CDTEXT_ARRANGER] = "Arranger",
    [CDTEXT_COMPOSER] = "Composer",
    [CDTEXT_MESSAGE]  =  "Message",
    [CDTEXT_ISRC] =  "ISRC",
    [CDTEXT_PERFORMER] = "Performer",
    [CDTEXT_SONGWRITER] =  "Songwriter",
    [CDTEXT_TITLE] =  "Title",
    [CDTEXT_UPC_EAN] = "UPC_EAN",
#else
    [CDTEXT_FIELD_ARRANGER] = "Arranger",
    [CDTEXT_FIELD_COMPOSER] = "Composer",
    [CDTEXT_FIELD_MESSAGE]  =  "Message",
    [CDTEXT_FIELD_ISRC] =  "ISRC",
    [CDTEXT_FIELD_PERFORMER] = "Performer",
    [CDTEXT_FIELD_SONGWRITER] =  "Songwriter",
    [CDTEXT_FIELD_TITLE] =  "Title",
    [CDTEXT_FIELD_UPC_EAN] = "UPC_EAN",
#endif
};

static void print_cdtext(stream_t *s, int track)
{
    cdda_priv* p = (cdda_priv*)s->priv;
    if (!p->cdtext)
        return;
#ifdef OLD_API
    cdtext_t *text = cdio_get_cdtext(p->cd->p_cdio, track);
#else
    cdtext_t *text = cdio_get_cdtext(p->cd->p_cdio);
#endif
    int header = 0;
    if (text) {
        for (int i = 0; i < sizeof(cdtext_name) / sizeof(cdtext_name[0]); i++) {
            const char *name = cdtext_name[i];
#ifdef OLD_API
            const char *value = cdtext_get_const(i, text);
#else
            const char *value = cdtext_get_const(text, i, track);
#endif
            if (name && value) {
                if (!header)
                    MP_INFO(s, "CD-Text (%s):\n", track ? "track" : "CD");
                header = 1;
                MP_INFO(s, "  %s: '%s'\n", name, value);
            }
        }
    }
}

static void print_track_info(stream_t *s, int track)
{
    MP_INFO(s, "Switched to track %d\n", track);
    print_cdtext(s, track);
}

static void cdparanoia_callback(long int inpos, paranoia_cb_mode_t function)
{
}

static int fill_buffer(stream_t *s, void *buffer, int max_len)
{
    cdda_priv *p = (cdda_priv *)s->priv;
    int i;

    if (!p->data || p->data_pos >= CDIO_CD_FRAMESIZE_RAW) {
        if ((p->sector < p->start_sector) || (p->sector > p->end_sector))
            return 0;

        p->data_pos = 0;
        p->data = (uint8_t *)paranoia_read(p->cdp, cdparanoia_callback);
        if (!p->data)
            return 0;

        p->sector++;
    }

    size_t copy = MPMIN(CDIO_CD_FRAMESIZE_RAW - p->data_pos, max_len);
    memcpy(buffer, p->data + p->data_pos, copy);
    p->data_pos += copy;

    for (i = 0; i < p->cd->tracks; i++) {
        if (p->cd->disc_toc[i].dwStartSector == p->sector - 1) {
            print_track_info(s, i + 1);
            break;
        }
    }

    return copy;
}

static int seek(stream_t *s, int64_t newpos)
{
    cdda_priv *p = (cdda_priv *)s->priv;
    int sec;
    int current_track = 0, seeked_track = 0;
    int seek_to_track = 0;
    int i;

    newpos += p->start_sector * CDIO_CD_FRAMESIZE_RAW;

    sec = newpos / CDIO_CD_FRAMESIZE_RAW;
    if (newpos < 0 || sec > p->end_sector) {
        p->sector = p->end_sector + 1;
        return 0;
    }

    for (i = 0; i < p->cd->tracks; i++) {
        if (p->sector >= p->cd->disc_toc[i].dwStartSector
            && p->sector < p->cd->disc_toc[i + 1].dwStartSector)
            current_track = i;
        if (sec >= p->cd->disc_toc[i].dwStartSector
            && sec < p->cd->disc_toc[i + 1].dwStartSector)
        {
            seeked_track = i;
            seek_to_track = sec == p->cd->disc_toc[i].dwStartSector;
        }
    }
    if (current_track != seeked_track && !seek_to_track)
        print_track_info(s, seeked_track + 1);

    p->sector = sec;

    paranoia_seek(p->cdp, sec, SEEK_SET);
    return 1;
}

static void close_cdda(stream_t *s)
{
    cdda_priv *p = (cdda_priv *)s->priv;
    paranoia_free(p->cdp);
    cdda_close(p->cd);
}

static int get_track_by_sector(cdda_priv *p, unsigned int sector)
{
    int i;
    for (i = p->cd->tracks; i >= 0; --i)
        if (p->cd->disc_toc[i].dwStartSector <= sector)
            break;
    return i;
}

static int control(stream_t *stream, int cmd, void *arg)
{
    cdda_priv *p = stream->priv;
    switch (cmd) {
    case STREAM_CTRL_GET_NUM_CHAPTERS:
    {
        int start_track = get_track_by_sector(p, p->start_sector);
        int end_track = get_track_by_sector(p, p->end_sector);
        if (start_track == -1 || end_track == -1)
            return STREAM_ERROR;
        *(unsigned int *)arg = end_track + 1 - start_track;
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_CHAPTER_TIME:
    {
        int track = *(double *)arg;
        int start_track = get_track_by_sector(p, p->start_sector);
        int end_track = get_track_by_sector(p, p->end_sector);
        track += start_track;
        if (track > end_track)
            return STREAM_ERROR;
        int64_t sector = p->cd->disc_toc[track].dwStartSector;
        int64_t pos = sector * CDIO_CD_FRAMESIZE_RAW;
        // Assume standard audio CD: 44.1khz, 2 channels, s16 samples
        *(double *)arg = pos / (44100.0 * 2 * 2);
        return STREAM_OK;
    }
    }
    return STREAM_UNSUPPORTED;
}

static int64_t get_size(stream_t *st)
{
    cdda_priv *p = st->priv;
    return (p->end_sector + 1 - p->start_sector) * CDIO_CD_FRAMESIZE_RAW;
}

static int open_cdda(stream_t *st)
{
    st->priv = mp_get_config_group(st, st->global, &stream_cdda_conf);
    cdda_priv *priv = st->priv;
    cdda_priv *p = priv;
    int mode = p->paranoia_mode;
    int offset = p->toc_offset;
    cdrom_drive_t *cdd = NULL;
    int last_track;

    char *global_device;
    mp_read_option_raw(st->global, "cdrom-device", &m_option_type_string,
                       &global_device);
    talloc_steal(st, global_device);

    if (st->path[0]) {
        p->device = st->path;
    } else if (global_device && global_device[0]) {
        p->device = global_device;
    } else {
        p->device = DEFAULT_CDROM_DEVICE;
    }

#if defined(__NetBSD__)
    cdd = cdda_identify_scsi(p->device, p->device, 0, NULL);
#else
    cdd = cdda_identify(p->device, 0, NULL);
#endif

    if (!cdd) {
        MP_ERR(st, "Can't open CDDA device.\n");
        return STREAM_ERROR;
    }

    cdda_verbose_set(cdd, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

    if (p->sector_size)
        cdd->nsectors = p->sector_size;

    if (cdda_open(cdd) != 0) {
        MP_ERR(st, "Can't open disc.\n");
        cdda_close(cdd);
        return STREAM_ERROR;
    }

    priv->cd = cdd;

    if (p->toc_bias)
        offset -= cdda_track_firstsector(cdd, 1);

    if (offset) {
        for (int n = 0; n < cdd->tracks + 1; n++)
            cdd->disc_toc[n].dwStartSector += offset;
    }

    if (p->speed > 0)
        cdda_speed_set(cdd, p->speed);

    last_track = cdda_tracks(cdd);
    if (p->span[0] > last_track)
        p->span[0] = last_track;
    if (p->span[1] < p->span[0])
        p->span[1] = p->span[0];
    if (p->span[1] > last_track)
        p->span[1] = last_track;
    if (p->span[0])
        priv->start_sector = cdda_track_firstsector(cdd, p->span[0]);
    else
        priv->start_sector = cdda_disc_firstsector(cdd);

    if (p->span[1])
        priv->end_sector = cdda_track_lastsector(cdd, p->span[1]);
    else
        priv->end_sector = cdda_disc_lastsector(cdd);

    priv->cdp = paranoia_init(cdd);
    if (priv->cdp == NULL) {
        cdda_close(cdd);
        free(priv);
        return STREAM_ERROR;
    }

    if (mode == 0)
        mode = PARANOIA_MODE_DISABLE;
    else if (mode == 1)
        mode = PARANOIA_MODE_OVERLAP;
    else
        mode = PARANOIA_MODE_FULL;

    if (p->skip)
        mode &= ~PARANOIA_MODE_NEVERSKIP;
    else
        mode |= PARANOIA_MODE_NEVERSKIP;

    if (p->search_overlap > 0)
        mode |= PARANOIA_MODE_OVERLAP;
    else if (p->search_overlap == 0)
        mode &= ~PARANOIA_MODE_OVERLAP;

    paranoia_modeset(priv->cdp, mode);

    if (p->search_overlap > 0)
        paranoia_overlapset(priv->cdp, p->search_overlap);

    paranoia_seek(priv->cdp, priv->start_sector, SEEK_SET);
    priv->sector = priv->start_sector;

    st->priv = priv;

    st->fill_buffer = fill_buffer;
    st->seek = seek;
    st->seekable = true;
    st->control = control;
    st->get_size = get_size;
    st->close = close_cdda;

    st->streaming = true;

    st->demuxer = "+disc";

    print_cdtext(st, 0);

    return STREAM_OK;
}

const stream_info_t stream_info_cdda = {
    .name = "cdda",
    .open = open_cdda,
    .protocols = (const char*const[]){"cdda", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};
