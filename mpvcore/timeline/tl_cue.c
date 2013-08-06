/*
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ctype.h>

#include "talloc.h"

#include "mpvcore/mp_core.h"
#include "mpvcore/mp_msg.h"
#include "demux/demux.h"
#include "mpvcore/path.h"
#include "mpvcore/bstr.h"
#include "mpvcore/mp_common.h"
#include "stream/stream.h"

// used by demuxer_cue.c
bool mp_probe_cue(struct bstr data);

#define SECS_PER_CUE_FRAME (1.0/75.0)

enum cue_command {
    CUE_ERROR = -1,     // not a valid CUE command, or an unknown extension
    CUE_EMPTY,          // line with whitespace only
    CUE_UNUSED,         // valid CUE command, but ignored by this code
    CUE_FILE,
    CUE_TRACK,
    CUE_INDEX,
    CUE_TITLE,
};

static const struct {
    enum cue_command command;
    const char *text;
} cue_command_strings[] = {
    { CUE_FILE, "FILE" },
    { CUE_TRACK, "TRACK" },
    { CUE_INDEX, "INDEX" },
    { CUE_TITLE, "TITLE" },
    { CUE_UNUSED, "CATALOG" },
    { CUE_UNUSED, "CDTEXTFILE" },
    { CUE_UNUSED, "FLAGS" },
    { CUE_UNUSED, "ISRC" },
    { CUE_UNUSED, "PERFORMER" },
    { CUE_UNUSED, "POSTGAP" },
    { CUE_UNUSED, "PREGAP" },
    { CUE_UNUSED, "REM" },
    { CUE_UNUSED, "SONGWRITER" },
    { CUE_UNUSED, "MESSAGE" },
    { -1 },
};

struct cue_track {
    double pregap_start;        // corresponds to INDEX 00
    double start;               // corresponds to INDEX 01
    struct bstr filename;
    int source;
    struct bstr title;
};

static enum cue_command read_cmd(struct bstr *data, struct bstr *out_params)
{
    struct bstr line = bstr_strip_linebreaks(bstr_getline(*data, data));
    line = bstr_lstrip(line);
    if (line.len == 0)
        return CUE_EMPTY;
    for (int n = 0; cue_command_strings[n].command != -1; n++) {
        struct bstr name = bstr0(cue_command_strings[n].text);
        if (bstr_startswith(line, name)) {
            struct bstr rest = bstr_cut(line, name.len);
            if (rest.len && !strchr(WHITESPACE, rest.start[0]))
                continue;
            if (out_params)
                *out_params = rest;
            return cue_command_strings[n].command;
        }
    }
    return CUE_ERROR;
}

static bool eat_char(struct bstr *data, char ch)
{
    if (data->len && data->start[0] == ch) {
        *data = bstr_cut(*data, 1);
        return true;
    } else {
        return false;
    }
}

static struct bstr read_quoted(struct bstr *data)
{
    *data = bstr_lstrip(*data);
    if (!eat_char(data, '"'))
        return (struct bstr) {0};
    int end = bstrchr(*data, '"');
    if (end < 0)
        return (struct bstr) {0};
    struct bstr res = bstr_splice(*data, 0, end);
    *data = bstr_cut(*data, end + 1);
    return res;
}

// Read a 2 digit unsigned decimal integer.
// Return -1 on failure.
static int read_int_2(struct bstr *data)
{
    *data = bstr_lstrip(*data);
    if (data->len && data->start[0] == '-')
        return -1;
    struct bstr s = *data;
    int res = (int)bstrtoll(s, &s, 10);
    if (data->len == s.len || data->len - s.len > 2)
        return -1;
    *data = s;
    return res;
}

static double read_time(struct bstr *data)
{
    struct bstr s = *data;
    bool ok = true;
    double t1 = read_int_2(&s);
    ok = eat_char(&s, ':') && ok;
    double t2 = read_int_2(&s);
    ok = eat_char(&s, ':') && ok;
    double t3 = read_int_2(&s);
    ok = ok && t1 >= 0 && t2 >= 0 && t3 >= 0;
    return ok ? t1 * 60.0 + t2 + t3 * SECS_PER_CUE_FRAME : 0;
}

static struct bstr skip_utf8_bom(struct bstr data)
{
    return bstr_startswith0(data, "\xEF\xBB\xBF") ? bstr_cut(data, 3) : data;
}

// Check if the text in data is most likely CUE data. This is used by the
// demuxer code to check the file type.
// data is the start of the probed file, possibly cut off at a random point.
bool mp_probe_cue(struct bstr data)
{
    bool valid = false;
    data = skip_utf8_bom(data);
    for (;;) {
        enum cue_command cmd = read_cmd(&data, NULL);
        // End reached. Since the line was most likely cut off, don't use the
        // result of the last parsing call.
        if (data.len == 0)
            break;
        if (cmd == CUE_ERROR)
            return false;
        if (cmd != CUE_EMPTY)
            valid = true;
    }
    return valid;
}

static void add_source(struct MPContext *mpctx, struct demuxer *d)
{
    MP_TARRAY_APPEND(NULL, mpctx->sources, mpctx->num_sources, d);
}

static bool try_open(struct MPContext *mpctx, char *filename)
{
    struct bstr bfilename = bstr0(filename);
    // Avoid trying to open itself or another .cue file. Best would be
    // to check the result of demuxer auto-detection, but the demuxer
    // API doesn't allow this without opening a full demuxer.
    if (bstr_case_endswith(bfilename, bstr0(".cue"))
        || bstrcasecmp(bstr0(mpctx->demuxer->filename), bfilename) == 0)
        return false;

    struct stream *s = stream_open(filename, mpctx->opts);
    if (!s)
        return false;
    struct demuxer *d = demux_open(s, NULL, NULL, mpctx->opts);
    // Since .bin files are raw PCM data with no headers, we have to explicitly
    // open them. Also, try to avoid to open files that are most likely not .bin
    // files, as that would only play noise. Checking the file extension is
    // fragile, but it's about the only way we have.
    // TODO: maybe also could check if the .bin file is a multiple of the Audio
    //       CD sector size (2352 bytes)
    if (!d && bstr_case_endswith(bfilename, bstr0(".bin"))) {
        mp_msg(MSGT_CPLAYER, MSGL_WARN, "CUE: Opening as BIN file!\n");
        d = demux_open(s, "rawaudio", NULL, mpctx->opts);
    }
    if (d) {
        add_source(mpctx, d);
        return true;
    }
    mp_msg(MSGT_CPLAYER, MSGL_ERR, "Could not open source '%s'!\n", filename);
    free_stream(s);
    return false;
}

static bool open_source(struct MPContext *mpctx, struct bstr filename)
{
    void *ctx = talloc_new(NULL);
    bool res = false;

    struct bstr dirname = mp_dirname(mpctx->demuxer->filename);

    struct bstr base_filename = bstr0(mp_basename(bstrdup0(ctx, filename)));
    if (!base_filename.len) {
        mp_msg(MSGT_CPLAYER, MSGL_WARN,
               "CUE: Invalid audio filename in .cue file!\n");
    } else {
        char *fullname = mp_path_join(ctx, dirname, base_filename);
        if (try_open(mpctx, fullname)) {
            res = true;
            goto out;
        }
    }

    // Try an audio file with the same name as the .cue file (but different
    // extension).
    // Rationale: this situation happens easily if the audio file or both files
    // are renamed.

    struct bstr cuefile =
        bstr_strip_ext(bstr0(mp_basename(mpctx->demuxer->filename)));

    DIR *d = opendir(bstrdup0(ctx, dirname));
    if (!d)
        goto out;
    struct dirent *de;
    while ((de = readdir(d))) {
        char *dename0 = de->d_name;
        struct bstr dename = bstr0(dename0);
        if (bstr_case_startswith(dename, cuefile)) {
            mp_msg(MSGT_CPLAYER, MSGL_WARN, "CUE: No useful audio filename "
                    "in .cue file found, trying with '%s' instead!\n",
                    dename0);
            if (try_open(mpctx, mp_path_join(ctx, dirname, dename))) {
                res = true;
                break;
            }
        }
    }
    closedir(d);

out:
    talloc_free(ctx);
    if (!res)
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "CUE: Could not open audio file!\n");
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

void build_cue_timeline(struct MPContext *mpctx)
{
    void *ctx = talloc_new(NULL);

    struct bstr data = mpctx->demuxer->file_contents;
    data = skip_utf8_bom(data);

    struct cue_track *tracks = NULL;
    size_t track_count = 0;

    struct bstr filename = {0};
    // Global metadata, and copied into new tracks.
    struct cue_track proto_track = {0};
    struct cue_track *cur_track = &proto_track;

    while (data.len) {
        struct bstr param;
        switch (read_cmd(&data, &param)) {
        case CUE_ERROR:
            mp_msg(MSGT_CPLAYER, MSGL_ERR, "CUE: error parsing input file!\n");
            goto out;
        case CUE_TRACK: {
            track_count++;
            tracks = talloc_realloc(ctx, tracks, struct cue_track, track_count);
            cur_track = &tracks[track_count - 1];
            *cur_track = proto_track;
            break;
        }
        case CUE_TITLE:
            cur_track->title = read_quoted(&param);
            break;
        case CUE_INDEX: {
            int type = read_int_2(&param);
            double time = read_time(&param);
            if (type == 1) {
                cur_track->start = time;
                cur_track->filename = filename;
            } else if (type == 0) {
                cur_track->pregap_start = time;
            }
            break;
        }
        case CUE_FILE:
            // NOTE: FILE comes before TRACK, so don't use cur_track->filename
            filename = read_quoted(&param);
            break;
        }
    }

    if (track_count == 0) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "CUE: no tracks found!\n");
        goto out;
    }

    // Remove duplicate file entries. This might be too sophisticated, since
    // CUE files usually use either separate files for every single track, or
    // only one file for all tracks.

    struct bstr *files = 0;
    size_t file_count = 0;

    for (size_t n = 0; n < track_count; n++) {
        struct cue_track *track = &tracks[n];
        track->source = -1;
        for (size_t file = 0; file < file_count; file++) {
            if (bstrcmp(files[file], track->filename) == 0) {
                track->source = file;
                break;
            }
        }
        if (track->source == -1) {
            file_count++;
            files = talloc_realloc(ctx, files, struct bstr, file_count);
            files[file_count - 1] = track->filename;
            track->source = file_count - 1;
        }
    }

    add_source(mpctx, mpctx->demuxer);

    for (size_t i = 0; i < file_count; i++) {
        if (!open_source(mpctx, files[i]))
            goto out;
    }

    struct timeline_part *timeline = talloc_array_ptrtype(NULL, timeline,
                                                          track_count + 1);
    struct chapter *chapters = talloc_array_ptrtype(NULL, chapters,
                                                    track_count);
    double starttime = 0;
    for (int i = 0; i < track_count; i++) {
        struct demuxer *source = mpctx->sources[1 + tracks[i].source];
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
            mp_msg(MSGT_CPLAYER, MSGL_WARN,
                   "CUE: Can't get duration of source file!\n");
            // xxx: do something more reasonable
            duration = 0.0;
        }
        timeline[i] = (struct timeline_part) {
            .start = starttime,
            .source_start = tracks[i].start,
            .source = source,
        };
        chapters[i] = (struct chapter) {
            .start = timeline[i].start,
            // might want to include other metadata here
            .name = bstrdup0(chapters, tracks[i].title),
        };
        starttime += duration;
    }

    // apparently we need this to give the last part a non-zero length
    timeline[track_count] = (struct timeline_part) {
        .start = starttime,
        // perhaps unused by the timeline code
        .source_start = 0,
        .source = timeline[0].source,
    };

    mpctx->timeline = timeline;
    // the last part is not included it in the count
    mpctx->num_timeline_parts = track_count + 1 - 1;
    mpctx->chapters = chapters;
    mpctx->num_chapters = track_count;

out:
    talloc_free(ctx);
}
