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
#include <dvdread/version.h>

#include "osdep/io.h"
#include "osdep/threads.h"

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

struct dvda_still {
    int pvob;                   // 0-based global still picture (P_VOB) index
    int64_t onset;              // display time, 90 kHz PTS ticks
};

// One audio track (mapped to an mpv chapter) within a title.
struct dvda_track {
    int64_t time;               // start time relative to title start (ticks)
    int64_t duration;           // 90 kHz PTS ticks
    uint32_t start_sector;      // within the ATS AOB space
    uint32_t end_sector;        // inclusive
    struct dvda_still *stills;  // still pictures shown during this track
    int num_stills;
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

    dvd_file_t *sv_file;        // AUDIO_SV.VOB
    uint32_t *pvob_sectors;     // still picture (P_VOB) -> sector in AUDIO_SV.VOB
    int num_pvobs;              // number of still pictures in the lookup
    int *asvu_first_pvob;       // 1-based first P_VOB of each ASVU
    int num_asvus;
    uint32_t pvobs_end;         // end of the still-picture area (sectors)
    uint8_t *still_data;        // the most recently extracted still (MPEG-2 video ES)
    int still_size;
    int still_pvob;             // P_VOB index of the cached still, -1 if none

    int track;                  // requested title, or TITLE_LONGEST

    mp_mutex still_lock;        // guards the still-picture state below
    int page;                   // forced still page (--dvda-page), -1 = time-based
    char *device;
    struct mp_log *lib_log;     // used by libdvdread's log callback
    bool probing;               // open is an .iso auto-detection probe
    struct dvda_opts *opts;
};

#define OPT_BASE_STRUCT struct dvda_opts

const struct m_sub_options dvda_conf = {
    .opts = (const struct m_option[]){
        {"device", OPT_STRING(device), .flags = M_OPT_FILE},
        {"page", OPT_INT(page), M_RANGE(-1, 2048)},
        {0}
    },
    .size = sizeof(struct dvda_opts),
    .defaults = &(const struct dvda_opts){
        .page = -1,
    },
};

// Compatibility with current libdvdread release (7.1.1). Drop this once
// 7.1.2+ is released and we can require it. There were many internal libdvdread
// fixes, the current release support is working, but limited. When ASV_MAX_NR
// is defined, we are at newer than 7.1.1 (master).
#if DVDREAD_VERSION > DVDREAD_VERSION_CODE(7, 1, 1) || defined(ASV_MAX_NR)
#define ASVS_ASV_EA(m)          ((m)->asv_ea)
#define ASVU_FIRST_ABS_ASVN(gi) ((gi)->first_abs_asvn)
#define ASVU_ASV_NS(gi)         ((gi)->asv_ns)
#define ASVU_SA(gi)             ((gi)->asvu_sa)
#define ASV_SRPT_OFFSET(e)      ((e).offset)
#else
#define ASVS_ASV_EA(m)          ((m)->p_vobs_ea)
#define ASVU_FIRST_ABS_ASVN(gi) ((gi)->start_p_vob_number)
#define ASVU_ASV_NS(gi)         ((gi)->p_vob_ns)
#define ASVU_SA(gi)             ((gi)->ref_start_sector)
#define ASV_SRPT_OFFSET(e)      ((e) & 0x3fff)
#endif

// Build the global still-picture -> AUDIO_SV.VOB sector table and open the still
// VOB. The ASVS groups still pictures (P_VOBs) into units (ASVUs). Each asv_srpt
// entry gives a P_VOB's start sector as an offset from its unit's start sector
// (the top two bits are copy control and not part of the offset).
static void read_asvs(stream_t *stream)
{
    struct priv *priv = stream->priv;

    priv->sv_file = DVDOpenFile(priv->dvd, 1, DVD_READ_MENU_VOBS);
    ifo_handle_t *asvs = ifoOpenASVS(priv->dvd);
    if (!asvs || !asvs->asvs_mat || !priv->sv_file) {
        if (asvs)
            ifoClose(asvs);
        return;
    }

    asvs_mat_t *m = asvs->asvs_mat;
    priv->pvobs_end = ASVS_ASV_EA(m);
    int f = 0;
    for (int g = 0; g < m->asvs_nr_of_asvus; g++) {
        asvu_gi_t *grp = &m->asvu_gi[g];
        MP_TARRAY_APPEND(priv, priv->asvu_first_pvob, priv->num_asvus,
                         ASVU_FIRST_ABS_ASVN(grp));
        for (int j = 0; j < ASVU_ASV_NS(grp); j++, f++) {
            uint32_t sector = ASVU_SA(grp) + ASV_SRPT_OFFSET(m->asv_srpt[f]);
            MP_TARRAY_APPEND(priv, priv->pvob_sectors, priv->num_pvobs, sector);
        }
    }

    MP_VERBOSE(stream, "ASVS: %d still pictures in %d units\n", priv->num_pvobs,
               priv->num_asvus);
    ifoClose(asvs);
}

// Map the per-title still-picture table to per-track stills. libdvdread parses
// it into a per-program playback-info table (ats_pg_asv_pbi_srp) and the still
// records (dlist) it points at. Each entry names the ASVU (index) the program's
// stills live in and the [start,end] byte span of its records; each record gives
// the still number within that unit (asv_number) and its display time. The
// global still is the unit's start_p_vob_number plus asv_number.
static void parse_title_stills(stream_t *stream, struct dvda_title *t,
                               atsi_title_record_t *rec)
{
    struct priv *priv = stream->priv;
    if (!priv->num_pvobs || !rec->ats_pg_asv_pbi_srp || !rec->dlist)
        return;

    int nr_tracks = rec->nr_tracks;
    int recbase = nr_tracks * ATS_PG_ASV_PBI_SRP_SIZE;
    int title_idx = priv->num_titles;   // title currently being built

    for (int i = 0; i < t->num_tracks && i < nr_tracks; i++) {
        ats_pg_asv_pbi_srp_t *rr = &rec->ats_pg_asv_pbi_srp[i];
        int asvu = rr->asvu_n - 1;
        MP_TRACE(stream, "title %d track %d: stills in asvu %d, "
                 "records [%u,%u], timing mode %d\n", title_idx, i,
                 rr->asvu_n, rr->start_value, rr->end_value, rr->dmod.timing);
        if (rr->end_value < rr->start_value || rr->start_value < recbase ||
            asvu < 0 || asvu >= priv->num_asvus)
            continue;
        struct dvda_track *tr = &t->tracks[i];
        for (int k = (rr->start_value - recbase) / ASV_DLIST_SIZE;
             k <= (rr->end_value - recbase) / ASV_DLIST_SIZE; k++)
        {
            asv_dlist_t *fr = &rec->dlist[k];
            if (fr->asv_number == 0)
                continue;
            int pvob = priv->asvu_first_pvob[asvu] - 1 + fr->asv_number - 1;
            MP_TRACE(stream, "  rec[%d]: asv %d track %#x timing %d -> "
                     "still %d\n", k, fr->asv_number, fr->track_nr,
                     fr->display_timing, pvob);
            // With a shared range, the per-record track number selects the track.
            if (rr->dmod.timing == 0 && fr->track_nr != i + 1)
                continue;
            if (pvob < 0 || pvob >= priv->num_pvobs)
                continue;
            struct dvda_still s = { .pvob = pvob, .onset = fr->display_timing };
            MP_TARRAY_APPEND(priv, tr->stills, tr->num_stills, s);
        }
    }
}

// Read the track layout of every title in every audio title set.
static bool read_disc_structure(stream_t *stream)
{
    struct priv *priv = stream->priv;

    ifo_handle_t *amg = ifoOpenVMGI(priv->dvd);
    if (!amg || amg->ifo_format != IFO_AUDIO || !amg->amgi_mat) {
        if (!priv->probing)
            MP_ERR(stream, "Could not read AUDIO_TS.IFO.\n");
        if (amg)
            ifoClose(amg);
        return false;
    }
    priv->probing = false;
    int num_ats = amg->amgi_mat->amg_nr_of_title_sets;
    ifoClose(amg);

    read_asvs(stream);

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
            parse_title_stills(stream, &t, rec);

            MP_VERBOSE(stream, "title %d: ats=%d tracks=%d sectors=%"PRIu32
                       "..%"PRIu32" duration=%.2f\n", priv->num_titles, ats,
                       num_tracks, t.start_sector, t.end_sector,
                       DVDA_TIME_TO_S(t.duration));
            for (int i = 0; i < num_tracks; i++) {
                struct dvda_track *tr = &t.tracks[i];
                MP_DBG(stream, "  track %d: t=%.2f dur=%.2f sectors=%"PRIu32
                       "..%"PRIu32" stills=%d\n", i, DVDA_TIME_TO_S(tr->time),
                       DVDA_TIME_TO_S(tr->duration), tr->start_sector,
                       tr->end_sector, tr->num_stills);
            }
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

static int current_still_pvob(stream_t *stream, struct dvda_title *t, double time)
{
    struct priv *priv = stream->priv;
    for (int i = t->num_tracks - 1; i >= 0; i--) {
        struct dvda_track *tr = &t->tracks[i];
        if (time < tr->time && i > 0)
            continue;
        if (tr->num_stills == 0)
            return -1;
        struct dvda_still *sel = NULL;
        if (priv->page >= 0) {
            sel = &tr->stills[MPMIN(priv->page, tr->num_stills - 1)];
        } else {
            int64_t pos = time - tr->time;
            int64_t best_onset = -1;
            for (int s = 0; s < tr->num_stills; s++) {
                if (tr->stills[s].onset <= pos &&
                    tr->stills[s].onset > best_onset)
                {
                    best_onset = tr->stills[s].onset;
                    sel = &tr->stills[s];
                }
            }
        }
        return sel ? sel->pvob : -1;
    }
    return -1;
}

// Extract a still's MPEG-2 video elementary stream from AUDIO_SV.VOB into the
// stream's cache. Stills tile the picture area contiguously in P_VOB order, so a
// still spans from its start sector to the next still's start. The MPEG-2
// video (PES 0xE0) is demuxed out of the program stream and trimmed at its
// sequence_end_code to drop trailing padding.
static bool extract_still(stream_t *stream, int pvob)
{
    struct priv *priv = stream->priv;
    if (pvob < 0 || pvob >= priv->num_pvobs || !priv->sv_file)
        return false;

    uint32_t start = priv->pvob_sectors[pvob];
    uint32_t end = pvob + 1 < priv->num_pvobs ? priv->pvob_sectors[pvob + 1]
                                              : priv->pvobs_end + 1;
    // A still is a single MPEG-2 I-frame plus mux overhead. The sectors come
    // from IFO data, so treat an absurd span as corruption.
    if (end <= start || end - start > 16 * 1024 * 1024 / DVD_BLOCK_SIZE) {
        MP_WARN(stream, "still pvob %d: bad sector range %"PRIu32"..%"PRIu32
                "; skipping...\n", pvob, start, end);
        return false;
    }
    int nsec = end - start;

    uint8_t *ps = talloc_array(NULL, uint8_t, nsec * DVD_BLOCK_SIZE);
    ssize_t r = DVDReadBlocks(priv->sv_file, start, nsec, ps);
    if (r <= 0) {
        talloc_free(ps);
        return false;
    }
    int total = r * DVD_BLOCK_SIZE;

    // Demux the video elementary stream (PES stream id 0xE0) out of the MPEG
    // program stream.
    uint8_t *es = talloc_array(priv, uint8_t, total);
    int es_len = 0;
    for (int i = 0; i + 9 <= total; ) {
        if (ps[i] || ps[i + 1] || ps[i + 2] != 1) {
            i++;
            continue;
        }
        int id = ps[i + 3];
        if (id == 0xBA) {           // pack header (MPEG-2 is 14 bytes)
            i += 14;
            continue;
        }
        int len = (ps[i + 4] << 8) | ps[i + 5];
        if (id == 0xE0 && len >= 3) {   // video PES packet
            int hdrlen = ps[i + 8];
            int off = 3 + hdrlen;
            if (off < len && i + 6 + len <= total) {
                memcpy(es + es_len, &ps[i + 6 + off], len - off);
                es_len += len - off;
            }
        }
        i += 6 + len;
    }
    talloc_free(ps);

    // A still image ends with a sequence_end_code, drop any trailing data.
    for (int j = 0; j + 4 <= es_len; j++) {
        if (!es[j] && !es[j + 1] && es[j + 2] == 1 && es[j + 3] == 0xB7) {
            es_len = j + 4;
            break;
        }
    }

    talloc_free(priv->still_data);
    priv->still_data = es;
    priv->still_size = es_len;
    priv->still_pvob = pvob;
    return es_len > 0;
}

static int control(stream_t *stream, int cmd, void *arg)
{
    struct priv *priv = stream->priv;
    struct dvda_title *t = &priv->titles[priv->title];

    switch (cmd) {
    case STREAM_CTRL_GET_NUM_CHAPTERS:
        *(unsigned int *)arg = t->num_tracks;
        return STREAM_OK;
    case STREAM_CTRL_SET_STILL_PAGE:
        mp_mutex_lock(&priv->still_lock);
        priv->page = *(int *)arg;
        priv->still_pvob = -1;   // force re-extract of the shown still
        mp_mutex_unlock(&priv->still_lock);
        return STREAM_OK;
    case STREAM_CTRL_GET_STILL: {
        struct stream_still_req *req = arg;
        if (!priv->num_pvobs)
            return STREAM_UNSUPPORTED;
        req->has_stills = false;
        for (int i = 0; i < t->num_tracks; i++)
            req->has_stills |= t->tracks[i].num_stills > 0;
        mp_mutex_lock(&priv->still_lock);
        req->id = current_still_pvob(stream, t, DVDA_TIME_FROM_S(req->time));
        req->data = NULL;
        req->data_size = 0;
        if (req->id < 0) {
            mp_mutex_unlock(&priv->still_lock);
            return STREAM_OK;
        }
        if (req->id != priv->still_pvob || !priv->still_data)
            extract_still(stream, req->id);
        if (priv->still_pvob == req->id && priv->still_data) {
            req->data = priv->still_data;
            req->data_size = priv->still_size;
        }
        mp_mutex_unlock(&priv->still_lock);
        return STREAM_OK;
    }
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
    mp_mutex_destroy(&priv->still_lock);
    if (priv->file)
        DVDCloseFile(priv->file);
    if (priv->sv_file)
        DVDCloseFile(priv->sv_file);
    if (priv->dvd)
        DVDClose(priv->dvd);
}

static void dvda_log(void *p, dvd_logger_level_t level,
                     const char *fmt, va_list va)
{
    struct priv *priv = p;
    int lvl;
    switch (level) {
    case DVD_LOGGER_LEVEL_ERROR: lvl = MSGL_ERR;   break;
    case DVD_LOGGER_LEVEL_WARN:  lvl = MSGL_WARN;  break;
    case DVD_LOGGER_LEVEL_DEBUG: lvl = MSGL_DEBUG; break;
    case DVD_LOGGER_LEVEL_INFO:
    default:                     lvl = MSGL_V;     break;
    }
    if (priv->probing)
        lvl = MPMAX(lvl, MSGL_V);
    if (!mp_msg_test(priv->lib_log, lvl))
        return;
    mp_msg_va(priv->lib_log, lvl, fmt, va);
    mp_msg(priv->lib_log, lvl, "\n");
}

static int open_s_internal(stream_t *stream)
{
    struct priv *priv = stream->priv;
    char *filename;

    mp_mutex_init(&priv->still_lock);

    priv->opts = mp_get_config_group(stream, stream->global, &dvda_conf);
    priv->page = priv->opts->page;

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

    priv->lib_log = mp_log_new(stream, stream->log, "/libdvdread");
    const dvd_logger_cb logger_cb = { .pf_log = dvda_log };
    priv->dvd = DVDOpenAudio(priv, &logger_cb, path);
    if (!priv->dvd) {
        if (!priv->probing)
            MP_ERR(stream, "Couldn't open DVD-Audio device: %s\n", path);
        goto err;
    }

    if (!read_disc_structure(stream)) {
        if (!priv->probing)
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
    return priv->probing ? STREAM_UNSUPPORTED : STREAM_ERROR;
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

    // Hand the .iso to libdvdread as the device. Opening validates the AMG
    // IFO, so it doubles as the probe (see priv->probing).
    if (bstr_case_endswith(bstr0(path), bstr0(".iso"))) {
        priv->device = talloc_strdup(priv, path);
        priv->probing = stream->autoprobed;
        int r = open_s_internal(stream);
        if (r != STREAM_OK) {
            if (priv->probing)
                goto unsupported;
            return r;
        }
        MP_INFO(stream, "DVD-Audio ISO image detected. Redirecting to dvda://\n");
        return r;
    }

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
