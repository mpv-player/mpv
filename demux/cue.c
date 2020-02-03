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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include "mpv_talloc.h"

#include "misc/bstr.h"
#include "common/common.h"
#include "common/tags.h"

#include "cue.h"

#define SECS_PER_CUE_FRAME (1.0/75.0)

enum cue_command {
    CUE_ERROR = -1,     // not a valid CUE command, or an unknown extension
    CUE_EMPTY,          // line with whitespace only
    CUE_UNUSED,         // valid CUE command, but ignored by this code
    CUE_FILE,
    CUE_TRACK,
    CUE_INDEX,
    CUE_TITLE,
    CUE_PERFORMER,
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
    { CUE_PERFORMER, "PERFORMER" },
    { CUE_UNUSED, "POSTGAP" },
    { CUE_UNUSED, "PREGAP" },
    { CUE_UNUSED, "REM" },
    { CUE_UNUSED, "SONGWRITER" },
    { CUE_UNUSED, "MESSAGE" },
    { -1 },
};

static const uint8_t spaces[] = {' ', '\f', '\n', '\r', '\t', '\v', 0xA0};

static struct bstr lstrip_whitespace(struct bstr data)
{
    while (data.len) {
        bstr rest = data;
        int code = bstr_decode_utf8(data, &rest);
        if (code < 0) {
            // Tolerate Latin1 => probing works (which doesn't convert charsets).
            code = data.start[0];
            rest.start += 1;
            rest.len -= 1;
        }
        for (size_t n = 0; n < MP_ARRAY_SIZE(spaces); n++) {
            if (spaces[n] == code) {
                data = rest;
                goto next;
            }
        }
        break;
    next: ;
    }
    return data;
}

static enum cue_command read_cmd(struct bstr *data, struct bstr *out_params)
{
    struct bstr line = bstr_strip_linebreaks(bstr_getline(*data, data));
    line = lstrip_whitespace(line);
    if (line.len == 0)
        return CUE_EMPTY;
    for (int n = 0; cue_command_strings[n].command != -1; n++) {
        struct bstr name = bstr0(cue_command_strings[n].text);
        if (bstr_case_startswith(line, name)) {
            struct bstr rest = bstr_cut(line, name.len);
            struct bstr par = lstrip_whitespace(rest);
            if (rest.len && par.len == rest.len)
                continue;
            if (out_params)
                *out_params = par;
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

static char *read_quoted(void *talloc_ctx, struct bstr *data)
{
    *data = lstrip_whitespace(*data);
    if (!eat_char(data, '"'))
        return NULL;
    int end = bstrchr(*data, '"');
    if (end < 0)
        return NULL;
    struct bstr res = bstr_splice(*data, 0, end);
    *data = bstr_cut(*data, end + 1);
    return bstrto0(talloc_ctx, res);
}

static struct bstr strip_quotes(struct bstr data)
{
    bstr s = data;
    if (bstr_eatstart0(&s, "\"") && bstr_eatend0(&s, "\""))
        return s;
    return data;
}

// Read an unsigned decimal integer.
// Optionally check if it is 2 digit.
// Return -1 on failure.
static int read_int(struct bstr *data, bool two_digit)
{
    *data = lstrip_whitespace(*data);
    if (data->len && data->start[0] == '-')
        return -1;
    struct bstr s = *data;
    int res = (int)bstrtoll(s, &s, 10);
    if (data->len == s.len || (two_digit && data->len - s.len > 2))
        return -1;
    *data = s;
    return res;
}

static double read_time(struct bstr *data)
{
    struct bstr s = *data;
    bool ok = true;
    double t1 = read_int(&s, false);
    ok = eat_char(&s, ':') && ok;
    double t2 = read_int(&s, true);
    ok = eat_char(&s, ':') && ok;
    double t3 = read_int(&s, true);
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

struct cue_file *mp_parse_cue(struct bstr data)
{
    struct cue_file *f = talloc_zero(NULL, struct cue_file);
    f->tags = talloc_zero(f, struct mp_tags);

    data = skip_utf8_bom(data);

    char *filename = NULL;
    // Global metadata, and copied into new tracks.
    struct cue_track proto_track = {0};
    struct cue_track *cur_track = NULL;

    while (data.len) {
        struct bstr param;
        int cmd = read_cmd(&data, &param);
        switch (cmd) {
        case CUE_ERROR:
            talloc_free(f);
            return NULL;
        case CUE_TRACK: {
            MP_TARRAY_GROW(f, f->tracks, f->num_tracks);
            f->num_tracks += 1;
            cur_track = &f->tracks[f->num_tracks - 1];
            *cur_track = proto_track;
            cur_track->tags = talloc_zero(f, struct mp_tags);
            break;
        }
        case CUE_TITLE:
        case CUE_PERFORMER: {
            static const char *metanames[] = {
                [CUE_TITLE] = "title",
                [CUE_PERFORMER] = "performer",
            };
            struct mp_tags *tags = cur_track ? cur_track->tags : f->tags;
            mp_tags_set_bstr(tags, bstr0(metanames[cmd]), strip_quotes(param));
            break;
        }
        case CUE_INDEX: {
            int type = read_int(&param, true);
            double time = read_time(&param);
            if (cur_track) {
                if (type == 1) {
                    cur_track->start = time;
                    cur_track->filename = filename;
                } else if (type == 0) {
                    cur_track->pregap_start = time;
                }
            }
            break;
        }
        case CUE_FILE:
            // NOTE: FILE comes before TRACK, so don't use cur_track->filename
            filename = read_quoted(f, &param);
            break;
        }
    }

    return f;
}

int mp_check_embedded_cue(struct cue_file *f)
{
    char *fn0 = f->tracks[0].filename;
    for (int n = 1; n < f->num_tracks; n++) {
        char *fn = f->tracks[n].filename;
        // both filenames have the same address (including NULL)
        if (fn0 == fn)
            continue;
        // only one filename is NULL, or the strings don't match
        if (!fn0 || !fn || strcmp(fn0, fn) != 0)
            return -1;
    }
    return 0;
}
