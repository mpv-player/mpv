/*
 * Original author: Uoti Urpala
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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <inttypes.h>

#include "osdep/io.h"

#include "mpv_talloc.h"

#include "misc/bstr.h"
#include "misc/charset_conv.h"
#include "common/msg.h"
#include "demux/demux.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "options/path.h"
#include "common/common.h"
#include "stream/stream.h"
#include "timeline.h"

#include "cue.h"

#define PROBE_SIZE 512

#define OPT_BASE_STRUCT struct demux_cue_opts
struct demux_cue_opts {
    char *cue_cp;
};

const struct m_sub_options demux_cue_conf = {
        .opts = (const m_option_t[]) {
            {"codepage", OPT_STRING(cue_cp)},
            {0}
        },
        .size = sizeof(struct demux_cue_opts),
        .defaults = &(const struct demux_cue_opts) {
            .cue_cp = "auto"
        }
};

struct priv {
    struct cue_file *f;
    struct demux_cue_opts *opts;
};

static void add_source(struct timeline *tl, struct demuxer *d)
{
    MP_TARRAY_APPEND(tl, tl->sources, tl->num_sources, d);
}

static bool try_open(struct timeline *tl, char *filename)
{
    struct bstr bfilename = bstr0(filename);
    // Avoid trying to open itself or another .cue file. Best would be
    // to check the result of demuxer auto-detection, but the demuxer
    // API doesn't allow this without opening a full demuxer.
    if (bstr_case_endswith(bfilename, bstr0(".cue"))
        || bstrcasecmp(bstr0(tl->demuxer->filename), bfilename) == 0)
        return false;

    struct demuxer_params p = {
        .stream_flags = tl->stream_origin,
    };

    struct demuxer *d = demux_open_url(filename, &p, tl->cancel, tl->global);
    // Since .bin files are raw PCM data with no headers, we have to explicitly
    // open them. Also, try to avoid to open files that are most likely not .bin
    // files, as that would only play noise. Checking the file extension is
    // fragile, but it's about the only way we have.
    // TODO: maybe also could check if the .bin file is a multiple of the Audio
    //       CD sector size (2352 bytes)
    if (!d && bstr_case_endswith(bfilename, bstr0(".bin"))) {
        MP_WARN(tl, "CUE: Opening as BIN file!\n");
        p.force_format = "rawaudio";
        d = demux_open_url(filename, &p, tl->cancel, tl->global);
    }
    if (d) {
        add_source(tl, d);
        return true;
    }
    MP_ERR(tl, "Could not open source '%s'!\n", filename);
    return false;
}

static bool open_source(struct timeline *tl, char *filename)
{
    void *ctx = talloc_new(NULL);
    bool res = false;

    struct bstr dirname = mp_dirname(tl->demuxer->filename);

    struct bstr base_filename = bstr0(mp_basename(filename));
    if (!base_filename.len) {
        MP_WARN(tl, "CUE: Invalid audio filename in .cue file!\n");
    } else {
        char *fullname = mp_path_join_bstr(ctx, dirname, base_filename);
        if (try_open(tl, fullname)) {
            res = true;
            goto out;
        }
    }

    // Try an audio file with the same name as the .cue file (but different
    // extension).
    // Rationale: this situation happens easily if the audio file or both files
    // are renamed.

    struct bstr cuefile =
        bstr_strip_ext(bstr0(mp_basename(tl->demuxer->filename)));

    DIR *d = opendir(bstrdup0(ctx, dirname));
    if (!d)
        goto out;
    struct dirent *de;
    while ((de = readdir(d))) {
        char *dename0 = de->d_name;
        struct bstr dename = bstr0(dename0);
        if (bstr_case_startswith(dename, cuefile)) {
            MP_WARN(tl, "CUE: No useful audio filename "
                    "in .cue file found, trying with '%s' instead!\n",
                    dename0);
            if (try_open(tl, mp_path_join_bstr(ctx, dirname, dename))) {
                res = true;
                break;
            }
        }
    }
    closedir(d);

out:
    talloc_free(ctx);
    if (!res)
        MP_ERR(tl, "CUE: Could not open audio file!\n");
    return res;
}

static void build_timeline(struct timeline *tl)
{
    struct priv *p = tl->demuxer->priv;

    void *ctx = talloc_new(NULL);

    add_source(tl, tl->demuxer);

    struct cue_track *tracks = NULL;
    size_t track_count = 0;

    for (size_t n = 0; n < p->f->num_tracks; n++) {
        struct cue_track *track = &p->f->tracks[n];
        if (track->filename) {
            MP_TARRAY_APPEND(ctx, tracks, track_count, *track);
        } else {
            MP_WARN(tl->demuxer, "No file specified for track entry %zd. "
                    "It will be removed\n", n + 1);
        }
    }

    if (track_count == 0) {
        MP_ERR(tl, "CUE: no tracks found!\n");
        goto out;
    }

    // Remove duplicate file entries. This might be too sophisticated, since
    // CUE files usually use either separate files for every single track, or
    // only one file for all tracks.

    char **files = 0;
    size_t file_count = 0;

    for (size_t n = 0; n < track_count; n++) {
        struct cue_track *track = &tracks[n];
        track->source = -1;
        for (size_t file = 0; file < file_count; file++) {
            if (strcmp(files[file], track->filename) == 0) {
                track->source = file;
                break;
            }
        }
        if (track->source == -1) {
            file_count++;
            files = talloc_realloc(ctx, files, char *, file_count);
            files[file_count - 1] = track->filename;
            track->source = file_count - 1;
        }
    }

    for (size_t i = 0; i < file_count; i++) {
        if (!open_source(tl, files[i]))
            goto out;
    }

    struct timeline_part *timeline = talloc_array_ptrtype(tl, timeline,
                                                          track_count + 1);
    struct demux_chapter *chapters = talloc_array_ptrtype(tl, chapters,
                                                          track_count);
    double starttime = 0;
    for (int i = 0; i < track_count; i++) {
        struct demuxer *source = tl->sources[1 + tracks[i].source];
        double duration;
        if (i + 1 < track_count && tracks[i].source == tracks[i + 1].source) {
            duration = tracks[i + 1].start - tracks[i].start;
        } else {
            duration = source->duration;
            // Two cases: 1) last track of a single-file cue, or 2) any track of
            // a multi-file cue. We need to do this for 1) only because the
            // timeline needs to be terminated with the length of the last
            // track.
            duration -= tracks[i].start;
        }
        if (duration < 0) {
            MP_WARN(tl, "CUE: Can't get duration of source file!\n");
            // xxx: do something more reasonable
            duration = 0.0;
        }
        timeline[i] = (struct timeline_part) {
            .start = starttime,
            .end = starttime + duration,
            .source_start = tracks[i].start,
            .source = source,
        };
        chapters[i] = (struct demux_chapter) {
            .pts = timeline[i].start,
            .metadata = mp_tags_dup(tl, tracks[i].tags),
        };
        starttime = timeline[i].end;
    }

    struct timeline_par *par = talloc_ptrtype(tl, par);
    *par = (struct timeline_par){
        .parts = timeline,
        .num_parts = track_count,
        .track_layout = timeline[0].source,
    };

    tl->chapters = chapters;
    tl->num_chapters = track_count;
    MP_TARRAY_APPEND(tl, tl->pars, tl->num_pars, par);
    tl->meta = par->track_layout;
    tl->format = "cue";

out:
    talloc_free(ctx);
}

static int try_open_file(struct demuxer *demuxer, enum demux_check check)
{
    if (!demuxer->access_references)
        return -1;

    struct stream *s = demuxer->stream;
    if (check >= DEMUX_CHECK_UNSAFE) {
        char probe[PROBE_SIZE];
        int len = stream_read_peek(s, probe, sizeof(probe));
        if (len < 1 || !mp_probe_cue((bstr){probe, len}))
            return -1;
    }
    struct priv *p = talloc_zero(demuxer, struct priv);
    demuxer->priv = p;
    demuxer->fully_read = true;
    p->opts = mp_get_config_group(p, demuxer->global, &demux_cue_conf);
    struct demux_cue_opts *cue_opts = p->opts;

    bstr data = stream_read_complete(s, p, 1000000);
    if (data.start == NULL)
        return -1;
    const char *charset = mp_charset_guess(p, demuxer->log, data, cue_opts->cue_cp, 0);
    if (charset && !mp_charset_is_utf8(charset)) {
        MP_INFO(demuxer, "Using CUE charset: %s\n", charset);
        bstr utf8 = mp_iconv_to_utf8(demuxer->log, data, charset, MP_ICONV_VERBOSE);
        if (utf8.start && utf8.start != data.start) {
            ta_steal(data.start, utf8.start);
            data = utf8;
        }
    }
    p->f = mp_parse_cue(data);
    talloc_steal(p, p->f);
    if (!p->f) {
        MP_ERR(demuxer, "error parsing input file!\n");
        return -1;
    }

    demux_close_stream(demuxer);

    mp_tags_merge(demuxer->metadata, p->f->tags);
    return 0;
}

const struct demuxer_desc demuxer_desc_cue = {
    .name = "cue",
    .desc = "CUE sheet",
    .open = try_open_file,
    .load_timeline = build_timeline,
};
