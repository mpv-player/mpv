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

#if !HAVE_GPL
#error GPL only
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <libavutil/mathematics.h>

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>
#include <dvdread/ifo_types.h>

#include "osdep/io.h"

#include "common/common.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "options/options.h"
#include "options/path.h"
#include "stream.h"

#define TITLE_LONGEST -1

#define DVD_BLOCK_SIZE 2048

#define DVDA_TIMEBASE 90000
#define DVDA_TIME_TO_S(x) ((x) / (double)(DVDA_TIMEBASE))
#define DVDA_TIME_FROM_S(x) ((int64_t)((x) * DVDA_TIMEBASE))

// One audio track (mapped to an mpv chapter) within a title.
struct dvda_track {
    int64_t time;               // start time relative to title start (ticks)
    int64_t duration;           // 90 kHz PTS ticks
    uint32_t start_sector;      // within the ATS AOB space
    uint32_t end_sector;        // inclusive
};

// One ATS title (mapped to an mpv title/edition).
struct dvda_title {
    int ats;                    // audio title set number (1-based)
    int64_t duration;           // 90 kHz PTS ticks
    uint32_t start_sector;
    uint32_t end_sector;        // inclusive
    struct dvda_track *tracks;
    int num_tracks;
};

struct priv {
    dvd_reader_t *dvd;
    dvd_file_t *file;           // AOBs of the currently open ATS
    int open_ats;               // ATS number the file handle belongs to

    struct dvda_title *titles;
    int num_titles;
    int title;                  // current title index
    uint32_t cur_sector;

    int track;                  // requested title, or TITLE_LONGEST
    char *device;
    struct dvda_opts *opts;
};

struct dvda_opts {
    char *device;
};

#define OPT_BASE_STRUCT struct dvda_opts

const struct m_sub_options dvda_conf = {
    .opts = (const struct m_option[]){
        {"device", OPT_STRING(device), .flags = M_OPT_FILE},
        {0}
    },
    .size = sizeof(struct dvda_opts),
};

// Read the track layout of every title in every audio title set.
static bool read_disc_structure(stream_t *stream)
{
    struct priv *priv = stream->priv;

    ifo_handle_t *amg = ifoOpenVMGI(priv->dvd);
    if (!amg || amg->ifo_format != IFO_AUDIO || !amg->amgi_mat) {
        MP_ERR(stream, "Could not read AUDIO_TS.IFO.\n");
        if (amg)
            ifoClose(amg);
        return false;
    }
    int num_ats = amg->amgi_mat->amg_nr_of_title_sets;
    ifoClose(amg);

    for (int ats = 1; ats <= num_ats; ats++) {
        ifo_handle_t *ifo = ifoOpen(priv->dvd, ats);
        if (!ifo)
            continue;
        if (ifo->ifo_format != IFO_AUDIO || !ifo->atsi_title_table) {
            ifoClose(ifo);
            continue;
        }
        atsi_title_table_t *tt = ifo->atsi_title_table;
        for (int n = 0; n < tt->nr_titles; n++) {
            atsi_title_record_t *rec = &tt->atsi_title_row_tables[n];
            int num_tracks = MPMIN(rec->nr_tracks, rec->nr_pointer_records);
            if (num_tracks <= 0)
                continue;

            struct dvda_title t = {
                .ats = ats,
                .duration = rec->length_pts,
                .start_sector = rec->atsi_track_pointer_rows[0].start_sector,
                .num_tracks = num_tracks,
                .tracks = talloc_array(priv, struct dvda_track, num_tracks),
            };
            t.end_sector = t.start_sector;
            int64_t time = 0;
            for (int i = 0; i < num_tracks; i++) {
                atsi_track_timestamp_t *ts = &rec->atsi_track_timestamp_rows[i];
                atsi_track_pointer_t *ptr = &rec->atsi_track_pointer_rows[i];
                t.tracks[i] = (struct dvda_track){
                    .time = time,
                    .duration = ts->length_pts_of_track,
                    .start_sector = ptr->start_sector,
                    .end_sector = ptr->end_sector,
                };
                time += t.tracks[i].duration;
                if (ptr->end_sector > t.end_sector)
                    t.end_sector = ptr->end_sector;
            }
            MP_DBG(stream, "title %d: ats=%d tracks=%d sectors=%"PRIu32
                   "..%"PRIu32" duration=%.2f\n", priv->num_titles, ats,
                   num_tracks, t.start_sector, t.end_sector, DVDA_TIME_TO_S(t.duration));
            for (int i = 0; i < num_tracks; i++)
                MP_DBG(stream, "  track %d: t=%.2f dur=%.2f sectors=%"PRIu32
                       "..%"PRIu32"\n", i, t.tracks[i].time,
                       DVDA_TIME_TO_S(t.tracks[i].duration), t.tracks[i].start_sector,
                       t.tracks[i].end_sector);
            MP_TARRAY_APPEND(priv, priv->titles, priv->num_titles, t);
        }
        ifoClose(ifo);
    }

    return priv->num_titles > 0;
}

static bool play_title(stream_t *stream, int title)
{
    struct priv *priv = stream->priv;

    if (title < 0 || title >= priv->num_titles)
        return false;

    struct dvda_title *t = &priv->titles[title];
    if (!priv->file || priv->open_ats != t->ats) {
        if (priv->file)
            DVDCloseFile(priv->file);
        priv->file = DVDOpenFile(priv->dvd, t->ats, DVD_READ_TITLE_VOBS);
        if (!priv->file) {
            MP_ERR(stream, "Could not open AOB files of title set %d.\n", t->ats);
            return false;
        }
        priv->open_ats = t->ats;
    }
    priv->title = title;
    priv->cur_sector = t->start_sector;
    return true;
}

static int fill_buffer(stream_t *stream, void *buf, int max_len)
{
    struct priv *priv = stream->priv;
    struct dvda_title *t = &priv->titles[priv->title];

    if (max_len < DVD_BLOCK_SIZE) {
        MP_FATAL(stream, "Short read size. Data corruption will follow. Please "
                         "provide a patch.\n");
        return -1;
    }

    if (priv->cur_sector > t->end_sector)
        return 0; // title end

    size_t blocks = MPMIN(max_len / DVD_BLOCK_SIZE,
                          t->end_sector - priv->cur_sector + 1);
    ssize_t r = DVDReadBlocks(priv->file, priv->cur_sector, blocks, buf);
    if (r <= 0) {
        MP_ERR(stream, "Error reading sector %"PRIu32".\n", priv->cur_sector);
        return 0;
    }
    priv->cur_sector += r;
    return r * DVD_BLOCK_SIZE;
}

// A track whose sectors fall outside the title's contiguous AOB range is a
// trailing marker.
static bool track_sectors_ok(struct dvda_title *t, struct dvda_track *tr)
{
    return tr->start_sector >= t->start_sector &&
           tr->end_sector <= t->end_sector &&
           tr->end_sector >= tr->start_sector;
}

// Map a sector position to title-relative playback time via the track it falls
// in. Track durations (and thus start times) come from length_pts.
static double sector_to_time(struct dvda_title *t, uint32_t sector)
{
    for (int i = 0; i < t->num_tracks; i++) {
        struct dvda_track *tr = &t->tracks[i];
        if (!track_sectors_ok(t, tr) || sector > tr->end_sector)
            continue;
        if (sector < tr->start_sector)
            return tr->time;
        return tr->time + av_rescale(sector - tr->start_sector, tr->duration,
                                     tr->end_sector - tr->start_sector + 1);
    }
    return t->duration;
}

static uint32_t time_to_sector(struct dvda_title *t, int64_t time)
{
    for (int i = t->num_tracks - 1; i >= 0; i--) {
        struct dvda_track *tr = &t->tracks[i];
        if (time < tr->time && i > 0)
            continue;
        if (!track_sectors_ok(t, tr))
            return t->end_sector;
        uint32_t span = tr->end_sector - tr->start_sector;
        int64_t off = time - tr->time;
        int64_t add = tr->duration > 0 && off > 0 ? av_rescale(off, span, tr->duration) : 0;
        return tr->start_sector + MPMIN(add, span);
    }
    return t->start_sector;
}

static int control(stream_t *stream, int cmd, void *arg)
{
    struct priv *priv = stream->priv;
    struct dvda_title *t = &priv->titles[priv->title];

    switch (cmd) {
    case STREAM_CTRL_GET_NUM_CHAPTERS:
        *(unsigned int *)arg = t->num_tracks;
        return STREAM_OK;
    case STREAM_CTRL_GET_CHAPTER_TIME: {
        double *ch = arg;
        int chapter = *ch;
        if (chapter < 0 || chapter >= t->num_tracks)
            break;
        *ch = DVDA_TIME_TO_S(t->tracks[chapter].time);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_TIME_LENGTH:
        *(double *)arg = DVDA_TIME_TO_S(t->duration);
        return STREAM_OK;
    case STREAM_CTRL_GET_CURRENT_TIME:
        *(double *)arg = DVDA_TIME_TO_S(sector_to_time(t, priv->cur_sector));
        return STREAM_OK;
    case STREAM_CTRL_SEEK_TO_TIME: {
        double *args = arg;
        priv->cur_sector = time_to_sector(t, DVDA_TIME_FROM_S(args[0]));
        stream_drop_buffers(stream);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_NUM_TITLES:
        *(unsigned int *)arg = priv->num_titles;
        return STREAM_OK;
    case STREAM_CTRL_GET_TITLE_LENGTH: {
        int title = *(double *)arg;
        if (title < 0 || title >= priv->num_titles)
            break;
        *(double *)arg = DVDA_TIME_TO_S(priv->titles[title].duration);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_CURRENT_TITLE:
        *(unsigned int *)arg = priv->title;
        return STREAM_OK;
    case STREAM_CTRL_SET_CURRENT_TITLE: {
        int title = *(unsigned int *)arg;
        if (!play_title(stream, title))
            break;
        stream_drop_buffers(stream);
        return STREAM_OK;
    }
    case STREAM_CTRL_GET_DISC_NAME: {
        char volid[32] = {0};
        if (DVDUDFVolumeInfo(priv->dvd, volid, sizeof(volid), NULL, 0) < 0 &&
            DVDISOVolumeInfo(priv->dvd, volid, sizeof(volid), NULL, 0) < 0)
            break;
        if (!volid[0])
            break;
        *(char **)arg = talloc_strdup(NULL, volid);
        return STREAM_OK;
    }
    }

    return STREAM_UNSUPPORTED;
}

static void stream_dvda_close(stream_t *stream)
{
    struct priv *priv = stream->priv;
    if (priv->file)
        DVDCloseFile(priv->file);
    if (priv->dvd)
        DVDClose(priv->dvd);
}

static void dvda_log(void *priv, dvd_logger_level_t level,
                     const char *fmt, va_list va)
{
    int lvl;
    switch (level) {
    case DVD_LOGGER_LEVEL_ERROR: lvl = MSGL_ERR;   break;
    case DVD_LOGGER_LEVEL_WARN:  lvl = MSGL_WARN;  break;
    case DVD_LOGGER_LEVEL_DEBUG: lvl = MSGL_DEBUG; break;
    case DVD_LOGGER_LEVEL_INFO:
    default:                     lvl = MSGL_V;     break;
    }
    if (!mp_msg_test(priv, lvl))
        return;
    mp_msg_va(priv, lvl, fmt, va);
    mp_msg(priv, lvl, "\n");
}

static int open_s_internal(stream_t *stream)
{
    struct priv *priv = stream->priv;
    char *filename;

    priv->opts = mp_get_config_group(stream, stream->global, &dvda_conf);

    if (priv->device && priv->device[0]) {
        filename = priv->device;
    } else if (priv->opts->device && priv->opts->device[0]) {
        filename = priv->opts->device;
    } else {
        filename = DEFAULT_OPTICAL_DEVICE;
    }

    char *path = mp_get_user_path(priv, stream->global, filename);
    if (!path)
        goto err;

    struct mp_log *log = mp_log_new(stream, stream->log, "/libdvdread");
    const dvd_logger_cb logger_cb = { .pf_log = dvda_log };
    priv->dvd = DVDOpenAudio(log, &logger_cb, path);
    if (!priv->dvd) {
        MP_ERR(stream, "Couldn't open DVD-Audio device: %s\n", path);
        goto err;
    }

    if (!read_disc_structure(stream)) {
        MP_ERR(stream, "No DVD-Audio titles found: %s\n", path);
        goto err;
    }

    if (priv->track == TITLE_LONGEST || priv->track >= priv->num_titles) {
        int64_t best_length = -1;
        int best_title = 0;
        for (int n = 0; n < priv->num_titles; n++) {
            MP_VERBOSE(stream, "title: %3d tracks: %2d duration: %.1f\n",
                       n, priv->titles[n].num_tracks,
                       DVDA_TIME_TO_S(priv->titles[n].duration));
            if (priv->titles[n].duration > best_length) {
                best_length = priv->titles[n].duration;
                best_title = n;
            }
        }
        priv->track = best_title;
        MP_INFO(stream, "Selecting title %d.\n", priv->track);
    }

    if (!play_title(stream, priv->track)) {
        MP_ERR(stream, "Couldn't select title %d.\n", priv->track);
        goto err;
    }

    stream->fill_buffer = fill_buffer;
    stream->control = control;
    stream->close = stream_dvda_close;
    stream->demuxer = "+disc";
    stream->lavf_type = "mpeg";

    return STREAM_OK;

err:
    stream_dvda_close(stream);
    return STREAM_ERROR;
}

static int open_s(stream_t *stream)
{
    struct priv *priv = talloc_zero(stream, struct priv);
    stream->priv = priv;

    bstr title, bdevice;
    bstr_split_tok(bstr0(stream->path), "/", &title, &bdevice);

    struct MPOpts *opts = mp_get_config_group(stream, stream->global, &mp_opt_root);
    int edition_id = opts->edition_id;
    talloc_free(opts);

    priv->track = TITLE_LONGEST;

    if (edition_id >= 0) {
        priv->track = edition_id;
    } else if (bstr_equals0(title, "longest") || bstr_equals0(title, "first")) {
        priv->track = TITLE_LONGEST;
    } else if (title.len) {
        bstr rest;
        priv->track = bstrtoll(title, &rest, 10);
        if (rest.len) {
            MP_ERR(stream, "number expected: '%.*s'\n", BSTR_P(rest));
            return STREAM_ERROR;
        }
    }

    priv->device = bstrto0(priv, bdevice);

    return open_s_internal(stream);
}

const stream_info_t stream_info_dvda = {
    .name = "dvda",
    .open = open_s,
    .protocols = (const char *const[]){ "dvda", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};

// Check if this is likely to be AUDIO_TS.IFO.
static bool check_ifo(const char *path)
{
    if (strcasecmp(mp_basename(path), "audio_ts.ifo"))
        return false;

    FILE *temp = fopen(path, "rb");
    if (!temp)
        return false;

    char data[12];
    bool r = fread(data, sizeof(data), 1, temp) == 1 &&
             memcmp(data, "DVDAUDIO-AMG", 12) == 0;

    fclose(temp);
    return r;
}

static int ifo_dvda_stream_open(stream_t *stream)
{
    struct priv *priv = talloc_zero(stream, struct priv);
    stream->priv = priv;

    if (!stream->access_references)
        goto unsupported;

    struct MPOpts *opts = mp_get_config_group(NULL, stream->global, &mp_opt_root);
    priv->track = opts->edition_id >= 0 ? opts->edition_id : TITLE_LONGEST;
    talloc_free(opts);

    char *path = mp_file_get_path(priv, bstr0(stream->url));
    if (!path)
        goto unsupported;

    // We allow the path to point to a directory containing AUDIO_TS/, a
    // directory containing AUDIO_TS.IFO, or that file itself.
    if (!check_ifo(path)) {
        // On UNIX, just assume the filename is always uppercase.
        char *npath = mp_path_join(priv, path, "AUDIO_TS.IFO");
        if (!check_ifo(npath)) {
            npath = mp_path_join(priv, path, "AUDIO_TS/AUDIO_TS.IFO");
            if (!check_ifo(npath))
                goto unsupported;
        }
        path = npath;
    }

    priv->device = bstrto0(priv, mp_dirname(path));

    MP_INFO(stream, ".IFO detected. Redirecting to dvda://\n");
    return open_s_internal(stream);

unsupported:
    talloc_free(priv);
    stream->priv = NULL;
    return STREAM_UNSUPPORTED;
}

const stream_info_t stream_info_ifo_dvda = {
    .name = "ifo_dvda",
    .open = ifo_dvda_stream_open,
    .protocols = (const char *const[]){ "file", "", NULL },
    .stream_origin = STREAM_ORIGIN_UNSAFE,
};
