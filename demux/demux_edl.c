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
#include <inttypes.h>
#include <math.h>

#include "mpv_talloc.h"

#include "demux.h"
#include "timeline.h"
#include "common/msg.h"
#include "options/path.h"
#include "misc/bstr.h"
#include "common/common.h"
#include "stream/stream.h"

#define HEADER "# mpv EDL v0\n"

struct tl_part {
    char *filename;             // what is stream_open()ed
    double offset;              // offset into the source file
    bool offset_set;
    bool chapter_ts;
    double length;              // length of the part (-1 if rest of the file)
    char *title;
};

struct tl_parts {
    bool dash;
    char *init_fragment_url;
    struct tl_part *parts;
    int num_parts;
};

struct priv {
    bstr data;
    bool allow_any;
};

// Parse a time (absolute file time or duration). Currently equivalent to a
// number. Return false on failure.
static bool parse_time(bstr str, double *out_time)
{
    bstr rest;
    double time = bstrtod(str, &rest);
    if (!str.len || rest.len || !isfinite(time))
        return false;
    *out_time = time;
    return true;
}

#define MAX_PARAMS 10

/* Returns a list of parts, or NULL on parse error.
 * Syntax (without file header or URI prefix):
 *    url      ::= <entry> ( (';' | '\n') <entry> )*
 *    entry    ::= <param> ( <param> ',' )*
 *    param    ::= [<string> '='] (<string> | '%' <number> '%' <bytes>)
 */
static struct tl_parts *parse_edl(bstr str)
{
    struct tl_parts *tl = talloc_zero(NULL, struct tl_parts);
    while (str.len) {
        if (bstr_eatstart0(&str, "#")) {
            bstr_split_tok(str, "\n", &(bstr){0}, &str);
            continue;
        }
        if (bstr_eatstart0(&str, "\n") || bstr_eatstart0(&str, ";"))
            continue;
        bool is_header = bstr_eatstart0(&str, "!");
        struct tl_part p = { .length = -1 };
        bstr param_names[MAX_PARAMS];
        bstr param_vals[MAX_PARAMS];
        int nparam = 0;
        while (1) {
            bstr name, val;
            // Check if it's of the form "name=..."
            int next = bstrcspn(str, "=%,;\n");
            if (next > 0 && next < str.len && str.start[next] == '=') {
                name = bstr_splice(str, 0, next);
                str = bstr_cut(str, next + 1);
            } else {
                const char *names[] = {"file", "start", "length"}; // implied name
                name = bstr0(nparam < 3 ? names[nparam] : "-");
            }
            if (bstr_eatstart0(&str, "%")) {
                int len = bstrtoll(str, &str, 0);
                if (!bstr_startswith0(str, "%") || (len > str.len - 1))
                    goto error;
                val = bstr_splice(str, 1, len + 1);
                str = bstr_cut(str, len + 1);
            } else {
                next = bstrcspn(str, ",;\n");
                val = bstr_splice(str, 0, next);
                str = bstr_cut(str, next);
            }
            // Interpret parameters. Explicitly ignore unknown ones.
            if (bstr_equals0(name, "file")) {
                p.filename = bstrto0(tl, val);
            } else if (bstr_equals0(name, "start")) {
                if (!parse_time(val, &p.offset))
                    goto error;
                p.offset_set = true;
            } else if (bstr_equals0(name, "length")) {
                if (!parse_time(val, &p.length))
                    goto error;
            } else if (bstr_equals0(name, "timestamps")) {
                if (bstr_equals0(val, "chapters"))
                    p.chapter_ts = true;
            } else if (bstr_equals0(name, "title")) {
                p.title = bstrto0(tl, val);
            }
            if (nparam >= MAX_PARAMS)
                goto error;
            param_names[nparam] = name;
            param_vals[nparam] = val;
            nparam++;
            if (!bstr_eatstart0(&str, ","))
                break;
        }
        if (is_header) {
            if (tl->num_parts)
                goto error; // can't have header once an entry was defined
            bstr type = param_vals[0]; // value, because no "="
            if (bstr_equals0(type, "mp4_dash")) {
                tl->dash = true;
                if (nparam > 1 && bstr_equals0(param_names[1], "init"))
                    tl->init_fragment_url = bstrto0(tl, param_vals[1]);
            }
            continue;
        }
        if (!p.filename)
            goto error;
        MP_TARRAY_APPEND(tl, tl->parts, tl->num_parts, p);
    }
    if (!tl->num_parts)
        goto error;
    return tl;
error:
    talloc_free(tl);
    return NULL;
}

static struct demuxer *open_source(struct timeline *tl, char *filename)
{
    for (int n = 0; n < tl->num_sources; n++) {
        struct demuxer *d = tl->sources[n];
        if (strcmp(d->stream->url, filename) == 0)
            return d;
    }
    struct demuxer_params params = {
        .init_fragment = tl->init_fragment,
    };
    struct demuxer *d = demux_open_url(filename, &params, tl->cancel, tl->global);
    if (d) {
        MP_TARRAY_APPEND(tl, tl->sources, tl->num_sources, d);
    } else {
        MP_ERR(tl, "EDL: Could not open source file '%s'.\n", filename);
    }
    return d;
}

static double demuxer_chapter_time(struct demuxer *demuxer, int n)
{
    if (n < 0 || n >= demuxer->num_chapters)
        return -1;
    return demuxer->chapters[n].pts;
}

// Append all chapters from src to the chapters array.
// Ignore chapters outside of the given time range.
static void copy_chapters(struct demux_chapter **chapters, int *num_chapters,
                          struct demuxer *src, double start, double len,
                          double dest_offset)
{
    for (int n = 0; n < src->num_chapters; n++) {
        double time = demuxer_chapter_time(src, n);
        if (time >= start && time <= start + len) {
            struct demux_chapter ch = {
                .pts = dest_offset + time - start,
                .metadata = mp_tags_dup(*chapters, src->chapters[n].metadata),
            };
            MP_TARRAY_APPEND(NULL, *chapters, *num_chapters, ch);
        }
    }
}

static void resolve_timestamps(struct tl_part *part, struct demuxer *demuxer)
{
    if (part->chapter_ts) {
        double start = demuxer_chapter_time(demuxer, part->offset);
        double length = part->length;
        double end = length;
        if (end >= 0)
            end = demuxer_chapter_time(demuxer, part->offset + part->length);
        if (end >= 0 && start >= 0)
            length = end - start;
        part->offset = start;
        part->length = length;
    }
    if (!part->offset_set)
        part->offset = demuxer->start_time;
}

static void build_timeline(struct timeline *tl, struct tl_parts *parts)
{
    tl->track_layout = NULL;
    tl->dash = parts->dash;

    if (parts->init_fragment_url && parts->init_fragment_url[0]) {
        MP_VERBOSE(tl, "Opening init fragment...\n");
        stream_t *s = stream_create(parts->init_fragment_url, STREAM_READ,
                                    tl->cancel, tl->global);
        if (s)
            tl->init_fragment = stream_read_complete(s, tl, 1000000);
        free_stream(s);
        if (!tl->init_fragment.len) {
            MP_ERR(tl, "Could not read init fragment.\n");
            goto error;
        }
        struct demuxer_params params = {
            .init_fragment = tl->init_fragment,
        };
        tl->track_layout = demux_open_url("memory://", &params, tl->cancel,
                                          tl->global);
        if (!tl->track_layout) {
            MP_ERR(tl, "Could not demux init fragment.\n");
            goto error;
        }
    }

    tl->parts = talloc_array_ptrtype(tl, tl->parts, parts->num_parts + 1);
    double starttime = 0;
    for (int n = 0; n < parts->num_parts; n++) {
        struct tl_part *part = &parts->parts[n];
        struct demuxer *source = NULL;

        if (tl->dash) {
            part->offset = starttime;
            if (part->length <= 0)
                MP_WARN(tl, "Segment %d has unknown duration.\n", n);
            if (part->offset_set)
                MP_WARN(tl, "Offsets are ignored.\n");
            tl->demuxer->is_network = true;

            if (!tl->track_layout) {
                source = open_source(tl, part->filename);
                if (!source)
                    goto error;
            }
        } else {
            MP_VERBOSE(tl, "Opening segment %d...\n", n);

            source = open_source(tl, part->filename);
            if (!source)
                goto error;

            resolve_timestamps(part, source);

            double end_time = source->duration;
            if (end_time >= 0)
                end_time += source->start_time;

            // Unknown length => use rest of the file. If duration is unknown, make
            // something up.
            if (part->length < 0) {
                if (end_time < 0) {
                    MP_WARN(tl, "EDL: source file '%s' has unknown duration.\n",
                            part->filename);
                    end_time = 1;
                }
                part->length = end_time - part->offset;
            } else if (end_time >= 0) {
                double end_part = part->offset + part->length;
                if (end_part > end_time) {
                    MP_WARN(tl, "EDL: entry %d uses %f "
                            "seconds, but file has only %f seconds.\n",
                            n, end_part, end_time);
                }
            }

            // Add a chapter between each file.
            struct demux_chapter ch = {
                .pts = starttime,
                .metadata = talloc_zero(tl, struct mp_tags),
            };
            mp_tags_set_str(ch.metadata, "title", part->title ? part->title : part->filename);
            MP_TARRAY_APPEND(tl, tl->chapters, tl->num_chapters, ch);

            // Also copy the source file's chapters for the relevant parts
            copy_chapters(&tl->chapters, &tl->num_chapters, source, part->offset,
                          part->length, starttime);
        }

        tl->parts[n] = (struct timeline_part) {
            .start = starttime,
            .source_start = part->offset,
            .source = source,
            .url = talloc_strdup(tl, part->filename),
        };

        starttime += part->length;

        if (source) {
            tl->demuxer->is_network |= source->is_network;

            if (!tl->track_layout)
                tl->track_layout = source;
        }
    }
    tl->parts[parts->num_parts] = (struct timeline_part) {.start = starttime};
    tl->num_parts = parts->num_parts;
    return;

error:
    tl->num_parts = 0;
    tl->num_chapters = 0;
}

// For security, don't allow relative or absolute paths, only plain filenames.
// Also, make these filenames relative to the edl source file.
static void fix_filenames(struct tl_parts *parts, char *source_path)
{
    struct bstr dirname = mp_dirname(source_path);
    for (int n = 0; n < parts->num_parts; n++) {
        struct tl_part *part = &parts->parts[n];
        char *filename = mp_basename(part->filename); // plain filename only
        part->filename = mp_path_join_bstr(parts, dirname, bstr0(filename));
    }
}

static void build_mpv_edl_timeline(struct timeline *tl)
{
    struct priv *p = tl->demuxer->priv;

    struct tl_parts *parts = parse_edl(p->data);
    if (!parts) {
        MP_ERR(tl, "Error in EDL.\n");
        return;
    }
    MP_TARRAY_APPEND(tl, tl->sources, tl->num_sources, tl->demuxer);
    if (!p->allow_any)
        fix_filenames(parts, tl->demuxer->filename);
    build_timeline(tl, parts);
    talloc_free(parts);
}

static int try_open_file(struct demuxer *demuxer, enum demux_check check)
{
    if (!demuxer->access_references)
        return -1;

    struct priv *p = talloc_zero(demuxer, struct priv);
    demuxer->priv = p;
    demuxer->fully_read = true;

    struct stream *s = demuxer->stream;
    if (s->info && strcmp(s->info->name, "edl") == 0) {
        p->data = bstr0(s->path);
        // Source is edl:// and not .edl => allow arbitrary paths
        p->allow_any = true;
        return 0;
    }
    if (check >= DEMUX_CHECK_UNSAFE) {
        if (!bstr_equals0(stream_peek(s, strlen(HEADER)), HEADER))
            return -1;
    }
    p->data = stream_read_complete(s, demuxer, 1000000);
    if (p->data.start == NULL)
        return -1;
    bstr_eatstart0(&p->data, HEADER);
    demux_close_stream(demuxer);
    return 0;
}

const struct demuxer_desc demuxer_desc_edl = {
    .name = "edl",
    .desc = "Edit decision list",
    .open = try_open_file,
    .load_timeline = build_mpv_edl_timeline,
};
