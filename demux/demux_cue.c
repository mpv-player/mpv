/*
 * Original author: Uoti Urpala
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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <inttypes.h>

#include "talloc.h"

#include "misc/bstr.h"
#include "common/msg.h"
#include "demux/demux.h"
#include "options/path.h"
#include "common/common.h"
#include "stream/stream.h"
#include "timeline.h"

#include "cue.h"

#define PROBE_SIZE 512

struct priv {
    bstr data;
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

    struct demuxer *d = demux_open_url(filename, NULL, tl->cancel, tl->global);
    // Since .bin files are raw PCM data with no headers, we have to explicitly
    // open them. Also, try to avoid to open files that are most likely not .bin
    // files, as that would only play noise. Checking the file extension is
    // fragile, but it's about the only way we have.
    // TODO: maybe also could check if the .bin file is a multiple of the Audio
    //       CD sector size (2352 bytes)
    if (!d && bstr_case_endswith(bfilename, bstr0(".bin"))) {
        MP_WARN(tl, "CUE: Opening as BIN file!\n");
        struct demuxer_params p = {.force_format = "rawaudio"};
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

// return length of the source in seconds, or -1 if unknown
static double source_get_length(struct demuxer *demuxer)
{
    double get_time_ans;
    // <= 0 means DEMUXER_CTRL_NOTIMPL or DEMUXER_CTRL_DONTKNOW
    if (demuxer && demux_control(demuxer, DEMUXER_CTRL_GET_TIME_LENGTH,
                                 (void *) &get_time_ans) > 0)
    {
        return get_time_ans;
    } else {
        return -1;
    }
}

static void build_timeline(struct timeline *tl)
{
    struct priv *p = tl->demuxer->priv;

    void *ctx = talloc_new(NULL);

    add_source(tl, tl->demuxer);

    struct cue_file *f = mp_parse_cue(p->data);
    if (!f) {
        MP_ERR(tl, "CUE: error parsing input file!\n");
        goto out;
    }
    talloc_steal(ctx, f);

    struct cue_track *tracks = f->tracks;
    size_t track_count = f->num_tracks;

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
            duration = source_get_length(source);
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
            .source_start = tracks[i].start,
            .source = source,
        };
        chapters[i] = (struct demux_chapter) {
            .pts = timeline[i].start,
            .metadata = talloc_zero(tl, struct mp_tags),
        };
        // might want to include other metadata here
        mp_tags_set_str(chapters[i].metadata, "title", tracks[i].title);
        starttime += duration;
    }

    // apparently we need this to give the last part a non-zero length
    timeline[track_count] = (struct timeline_part) {
        .start = starttime,
        // perhaps unused by the timeline code
        .source_start = 0,
        .source = timeline[0].source,
    };

    tl->parts = timeline;
    // the last part is not included it in the count
    tl->num_parts = track_count + 1 - 1;
    tl->chapters = chapters;
    tl->num_chapters = track_count;
    tl->track_layout = tl->parts[0].source;

out:
    talloc_free(ctx);
}

static int try_open_file(struct demuxer *demuxer, enum demux_check check)
{
    struct stream *s = demuxer->stream;
    if (check >= DEMUX_CHECK_UNSAFE) {
        bstr d = stream_peek(s, PROBE_SIZE);
        if (d.len < 1 || !mp_probe_cue(d))
            return -1;
    }
    struct priv *p = talloc_zero(demuxer, struct priv);
    demuxer->priv = p;
    demuxer->fully_read = true;
    p->data = stream_read_complete(s, demuxer, 1000000);
    if (p->data.start == NULL)
        return -1;
    return 0;
}

const struct demuxer_desc demuxer_desc_cue = {
    .name = "cue",
    .desc = "CUE sheet",
    .open = try_open_file,
    .load_timeline = build_timeline,
};
