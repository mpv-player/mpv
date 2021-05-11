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
#include <limits.h>
#include <math.h>

#include "mpv_talloc.h"

#include "demux.h"
#include "timeline.h"
#include "common/msg.h"
#include "options/path.h"
#include "misc/bstr.h"
#include "common/common.h"
#include "common/tags.h"
#include "stream/stream.h"

#define HEADER "# mpv EDL v0\n"

struct tl_part {
    char *filename;             // what is stream_open()ed
    double offset;              // offset into the source file
    bool offset_set;
    bool chapter_ts;
    bool is_layout;
    double length;              // length of the part (-1 if rest of the file)
    char *title;
};

struct tl_parts {
    bool disable_chapters;
    bool dash, no_clip, delay_open;
    char *init_fragment_url;
    struct sh_stream **sh_meta;
    int num_sh_meta;
    struct tl_part *parts;
    int num_parts;
    struct tl_parts *next;
};

struct tl_root {
    struct tl_parts **pars;
    int num_pars;
    struct mp_tags *tags;
};

struct priv {
    bstr data;
};

// Static allocation out of laziness.
#define NUM_MAX_PARAMS 20

struct parse_ctx {
    struct mp_log *log;
    bool error;
    bstr param_vals[NUM_MAX_PARAMS];
    bstr param_names[NUM_MAX_PARAMS];
    int num_params;
};

// This returns a value with bstr.start==NULL if nothing found. If the parameter
// was specified, bstr.str!=NULL, even if the string is empty (bstr.len==0).
// The parameter is removed from the list if found.
static bstr get_param(struct parse_ctx *ctx, const char *name)
{
    bstr bname = bstr0(name);
    for (int n = 0; n < ctx->num_params; n++) {
        if (bstr_equals(ctx->param_names[n], bname)) {
            bstr res = ctx->param_vals[n];
            int count = ctx->num_params;
            MP_TARRAY_REMOVE_AT(ctx->param_names, count, n);
            count = ctx->num_params;
            MP_TARRAY_REMOVE_AT(ctx->param_vals, count, n);
            ctx->num_params -= 1;
            if (!res.start)
                res = bstr0("");  // keep guarantees
            return res;
        }
    }
    return (bstr){0};
}

// Same as get_param(), but return C string. Return NULL if missing.
static char *get_param0(struct parse_ctx *ctx, void *ta_ctx, const char *name)
{
    return bstrdup0(ta_ctx, get_param(ctx, name));
}

// Optional int parameter. Returns the parsed integer, or def if the parameter
// is missing or on error (sets ctx.error on error).
static int get_param_int(struct parse_ctx *ctx, const char *name, int def)
{
    bstr val = get_param(ctx, name);
    if (val.start) {
        bstr rest;
        long long ival = bstrtoll(val, &rest, 0);
        if (!val.len || rest.len || ival < INT_MIN || ival > INT_MAX) {
            MP_ERR(ctx, "Invalid integer: '%.*s'\n", BSTR_P(val));
            ctx->error = true;
            return def;
        }
        return ival;
    }
    return def;
}

// Optional time parameter. Currently a number.
// Returns true: parameter was present and valid, *t is set
// Returns false: parameter was not present (or broken => ctx.error set)
static bool get_param_time(struct parse_ctx *ctx, const char *name, double *t)
{
    bstr val = get_param(ctx, name);
    if (val.start) {
        bstr rest;
        double time = bstrtod(val, &rest);
        if (!val.len || rest.len || !isfinite(time)) {
            MP_ERR(ctx, "Invalid time string: '%.*s'\n", BSTR_P(val));
            ctx->error = true;
            return false;
        }
        *t = time;
        return true;
    }
    return false;
}

static struct tl_parts *add_part(struct tl_root *root)
{
    struct tl_parts *tl = talloc_zero(root, struct tl_parts);
    MP_TARRAY_APPEND(root, root->pars, root->num_pars, tl);
    return tl;
}

static struct sh_stream *get_meta(struct tl_parts *tl, int index)
{
    for (int n = 0; n < tl->num_sh_meta; n++) {
        if (tl->sh_meta[n]->index == index)
            return tl->sh_meta[n];
    }
    struct sh_stream *sh = demux_alloc_sh_stream(STREAM_TYPE_COUNT);
    talloc_steal(tl, sh);
    MP_TARRAY_APPEND(tl, tl->sh_meta, tl->num_sh_meta, sh);
    return sh;
}

/* Returns a list of parts, or NULL on parse error.
 * Syntax (without file header or URI prefix):
 *    url      ::= <entry> ( (';' | '\n') <entry> )*
 *    entry    ::= <param> ( <param> ',' )*
 *    param    ::= [<string> '='] (<string> | '%' <number> '%' <bytes>)
 */
static struct tl_root *parse_edl(bstr str, struct mp_log *log)
{
    struct tl_root *root = talloc_zero(NULL, struct tl_root);
    root->tags = talloc_zero(root, struct mp_tags);
    struct tl_parts *tl = add_part(root);
    while (str.len) {
        if (bstr_eatstart0(&str, "#")) {
            bstr_split_tok(str, "\n", &(bstr){0}, &str);
            continue;
        }
        if (bstr_eatstart0(&str, "\n") || bstr_eatstart0(&str, ";"))
            continue;
        bool is_header = bstr_eatstart0(&str, "!");
        struct parse_ctx ctx = { .log = log };
        int nparam = 0;
        while (1) {
            bstr name, val;
            // Check if it's of the form "name=..."
            int next = bstrcspn(str, "=%,;\n");
            if (next > 0 && next < str.len && str.start[next] == '=') {
                name = bstr_splice(str, 0, next);
                str = bstr_cut(str, next + 1);
            } else if (is_header) {
                const char *names[] = {"type"}; // implied name
                name = bstr0(nparam < 1 ? names[nparam] : "-");
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
            if (ctx.num_params >= NUM_MAX_PARAMS) {
                mp_err(log, "Too many parameters, ignoring '%.*s'.\n",
                       BSTR_P(name));
            } else {
                ctx.param_names[ctx.num_params] = name;
                ctx.param_vals[ctx.num_params] = val;
                ctx.num_params += 1;
            }
            nparam++;
            if (!bstr_eatstart0(&str, ","))
                break;
        }
        if (is_header) {
            bstr f_type = get_param(&ctx, "type");
            if (bstr_equals0(f_type, "mp4_dash")) {
                tl->dash = true;
                tl->init_fragment_url = get_param0(&ctx, tl, "init");
            } else if (bstr_equals0(f_type, "no_clip")) {
                tl->no_clip = true;
            } else if (bstr_equals0(f_type, "new_stream")) {
                // (Special case: ignore "redundant" headers at the start for
                // general symmetry.)
                if (root->num_pars > 1 || tl->num_parts)
                    tl = add_part(root);
            } else if (bstr_equals0(f_type, "no_chapters")) {
                tl->disable_chapters = true;
            } else if (bstr_equals0(f_type, "track_meta")) {
                int index = get_param_int(&ctx, "index", -1);
                struct sh_stream *sh = index < 0 && tl->num_sh_meta
                    ? tl->sh_meta[tl->num_sh_meta - 1]
                    : get_meta(tl, index);
                sh->lang = get_param0(&ctx, sh, "lang");
                sh->title = get_param0(&ctx, sh, "title");
                sh->hls_bitrate = get_param_int(&ctx, "byterate", 0) * 8;
                bstr flags = get_param(&ctx, "flags");
                bstr flag;
                while (bstr_split_tok(flags, "+", &flag, &flags) || flag.len) {
                    if (bstr_equals0(flag, "default")) {
                        sh->default_track = true;
                    } else if (bstr_equals0(flag, "forced")) {
                        sh->forced_track = true;
                    } else {
                        mp_warn(log, "Unknown flag: '%.*s'\n", BSTR_P(flag));
                    }
                }
            } else if (bstr_equals0(f_type, "delay_open")) {
                struct sh_stream *sh = get_meta(tl, tl->num_sh_meta);
                bstr mt = get_param(&ctx, "media_type");
                if (bstr_equals0(mt, "video")) {
                    sh->type = sh->codec->type = STREAM_VIDEO;
                } else if (bstr_equals0(mt, "audio")) {
                    sh->type = sh->codec->type = STREAM_AUDIO;
                } else if (bstr_equals0(mt, "sub")) {
                    sh->type = sh->codec->type = STREAM_SUB;
                } else {
                    mp_err(log, "Invalid or missing !delay_open media type.\n");
                    goto error;
                }
                sh->codec->codec = get_param0(&ctx, sh, "codec");
                if (!sh->codec->codec)
                    sh->codec->codec = "null";
                sh->codec->disp_w = get_param_int(&ctx, "w", 0);
                sh->codec->disp_h = get_param_int(&ctx, "h", 0);
                sh->codec->fps = get_param_int(&ctx, "fps", 0);
                sh->codec->samplerate = get_param_int(&ctx, "samplerate", 0);
                tl->delay_open = true;
            } else if (bstr_equals0(f_type, "global_tags")) {
                for (int n = 0; n < ctx.num_params; n++) {
                    mp_tags_set_bstr(root->tags, ctx.param_names[n],
                                     ctx.param_vals[n]);
                }
                ctx.num_params = 0;
            } else {
                mp_err(log, "Unknown header: '%.*s'\n", BSTR_P(f_type));
                goto error;
            }
        } else {
            struct tl_part p = { .length = -1 };
            p.filename = get_param0(&ctx, tl, "file");
            p.offset_set = get_param_time(&ctx, "start", &p.offset);
            get_param_time(&ctx, "length", &p.length);
            bstr ts = get_param(&ctx, "timestamps");
            if (bstr_equals0(ts, "chapters")) {
                p.chapter_ts = true;
            } else if (ts.start && !bstr_equals0(ts, "seconds")) {
                mp_warn(log, "Unknown timestamp type: '%.*s'\n", BSTR_P(ts));
            }
            p.title = get_param0(&ctx, tl, "title");
            bstr layout = get_param(&ctx, "layout");
            if (layout.start) {
                if (bstr_equals0(layout, "this")) {
                    p.is_layout = true;
                } else {
                    mp_warn(log, "Unknown layout param: '%.*s'\n", BSTR_P(layout));
                }
            }
            if (!p.filename) {
                mp_err(log, "Missing filename in segment.'\n");
                goto error;
            }
            MP_TARRAY_APPEND(tl, tl->parts, tl->num_parts, p);
        }
        if (ctx.error)
            goto error;
        for (int n = 0; n < ctx.num_params; n++) {
            mp_warn(log, "Unknown or duplicate parameter: '%.*s'\n",
                    BSTR_P(ctx.param_names[n]));
        }
    }
    assert(root->num_pars);
    for (int n = 0; n < root->num_pars; n++) {
        if (root->pars[n]->num_parts < 1) {
            mp_err(log, "EDL specifies no segments.'\n");
            goto error;
        }
    }
    return root;
error:
    mp_err(log, "EDL parsing failed.\n");
    talloc_free(root);
    return NULL;
}

static struct demuxer *open_source(struct timeline *root,
                                   struct timeline_par *tl, char *filename)
{
    for (int n = 0; n < tl->num_parts; n++) {
        struct demuxer *d = tl->parts[n].source;
        if (d && d->filename && strcmp(d->filename, filename) == 0)
            return d;
    }
    struct demuxer_params params = {
        .init_fragment = tl->init_fragment,
        .stream_flags = root->stream_origin,
    };
    struct demuxer *d = demux_open_url(filename, &params, root->cancel,
                                       root->global);
    if (d) {
        MP_TARRAY_APPEND(root, root->sources, root->num_sources, d);
    } else {
        MP_ERR(root, "EDL: Could not open source file '%s'.\n", filename);
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

static struct timeline_par *build_timeline(struct timeline *root,
                                           struct tl_root *edl_root,
                                           struct tl_parts *parts)
{
    struct timeline_par *tl = talloc_zero(root, struct timeline_par);
    MP_TARRAY_APPEND(root, root->pars, root->num_pars, tl);

    tl->track_layout = NULL;
    tl->dash = parts->dash;
    tl->no_clip = parts->no_clip;
    tl->delay_open = parts->delay_open;

    // There is no copy function for sh_stream, so just steal it.
    for (int n = 0; n < parts->num_sh_meta; n++) {
        MP_TARRAY_APPEND(tl, tl->sh_meta, tl->num_sh_meta,
                         talloc_steal(tl, parts->sh_meta[n]));
        parts->sh_meta[n] = NULL;
    }
    parts->num_sh_meta = 0;

    if (parts->init_fragment_url && parts->init_fragment_url[0]) {
        MP_VERBOSE(root, "Opening init fragment...\n");
        stream_t *s = stream_create(parts->init_fragment_url,
                                    STREAM_READ | root->stream_origin,
                                    root->cancel, root->global);
        if (s) {
            root->is_network |= s->is_network;
            root->is_streaming |= s->streaming;
            tl->init_fragment = stream_read_complete(s, tl, 1000000);
        }
        free_stream(s);
        if (!tl->init_fragment.len) {
            MP_ERR(root, "Could not read init fragment.\n");
            goto error;
        }
        struct demuxer_params params = {
            .init_fragment = tl->init_fragment,
            .stream_flags = root->stream_origin,
        };
        tl->track_layout = demux_open_url("memory://", &params, root->cancel,
                                          root->global);
        if (!tl->track_layout) {
            MP_ERR(root, "Could not demux init fragment.\n");
            goto error;
        }
        MP_TARRAY_APPEND(root, root->sources, root->num_sources, tl->track_layout);
    }

    tl->parts = talloc_array_ptrtype(tl, tl->parts, parts->num_parts);
    double starttime = 0;
    for (int n = 0; n < parts->num_parts; n++) {
        struct tl_part *part = &parts->parts[n];
        struct demuxer *source = NULL;

        if (tl->dash) {
            part->offset = starttime;
            if (part->length <= 0)
                MP_WARN(root, "Segment %d has unknown duration.\n", n);
            if (part->offset_set)
                MP_WARN(root, "Offsets are ignored.\n");

            if (!tl->track_layout)
                tl->track_layout = open_source(root, tl, part->filename);
        } else if (tl->delay_open) {
            if (n == 0 && !part->offset_set) {
                part->offset = starttime;
                part->offset_set = true;
            }
            if (part->chapter_ts || (part->length < 0 && !tl->no_clip)) {
                MP_ERR(root, "Invalid specification for delay_open stream.\n");
                goto error;
            }
        } else {
            MP_VERBOSE(root, "Opening segment %d...\n", n);

            source = open_source(root, tl, part->filename);
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
                    MP_WARN(root, "EDL: source file '%s' has unknown duration.\n",
                            part->filename);
                    end_time = 1;
                }
                part->length = end_time - part->offset;
            } else if (end_time >= 0) {
                double end_part = part->offset + part->length;
                if (end_part > end_time) {
                    MP_WARN(root, "EDL: entry %d uses %f "
                            "seconds, but file has only %f seconds.\n",
                            n, end_part, end_time);
                }
            }

            if (!parts->disable_chapters) {
                // Add a chapter between each file.
                struct demux_chapter ch = {
                    .pts = starttime,
                    .metadata = talloc_zero(tl, struct mp_tags),
                };
                mp_tags_set_str(ch.metadata, "title",
                                part->title ? part->title : part->filename);
                MP_TARRAY_APPEND(root, root->chapters, root->num_chapters, ch);

                // Also copy the source file's chapters for the relevant parts
                copy_chapters(&root->chapters, &root->num_chapters, source,
                              part->offset, part->length, starttime);
            }
        }

        tl->parts[n] = (struct timeline_part) {
            .start = starttime,
            .end = starttime + part->length,
            .source_start = part->offset,
            .source = source,
            .url = talloc_strdup(tl, part->filename),
        };

        starttime = tl->parts[n].end;

        if (source && !tl->track_layout && part->is_layout)
            tl->track_layout = source;

        tl->num_parts++;
    }

    if (tl->no_clip && tl->num_parts > 1)
        MP_WARN(root, "Multiple parts with no_clip. Undefined behavior ahead.\n");

    if (!tl->track_layout) {
        // Use a heuristic to select the "broadest" part as layout.
        for (int n = 0; n < parts->num_parts; n++) {
            struct demuxer *s = tl->parts[n].source;
            if (!s)
                continue;
            if (!tl->track_layout ||
                demux_get_num_stream(s) > demux_get_num_stream(tl->track_layout))
                tl->track_layout = s;
        }
    }

    if (!tl->track_layout && !tl->delay_open)
        goto error;
    if (!root->meta)
        root->meta = tl->track_layout;

    // Not very sane, since demuxer fields are supposed to be treated read-only
    // from outside, but happens to work in this case, so who cares.
    if (root->meta)
        mp_tags_merge(root->meta->metadata, edl_root->tags);

    assert(tl->num_parts == parts->num_parts);
    return tl;

error:
    root->num_pars = 0;
    return NULL;
}

static void fix_filenames(struct tl_parts *parts, char *source_path)
{
    if (bstr_equals0(mp_split_proto(bstr0(source_path), NULL), "edl"))
        return;
    struct bstr dirname = mp_dirname(source_path);
    for (int n = 0; n < parts->num_parts; n++) {
        struct tl_part *part = &parts->parts[n];
        if (!mp_is_url(bstr0(part->filename))) {
            part->filename =
                mp_path_join_bstr(parts, dirname, bstr0(part->filename));
        }
    }
}

static void build_mpv_edl_timeline(struct timeline *tl)
{
    struct priv *p = tl->demuxer->priv;

    struct tl_root *root = parse_edl(p->data, tl->log);
    if (!root) {
        MP_ERR(tl, "Error in EDL.\n");
        return;
    }

    bool all_dash = true;
    bool all_no_clip = true;
    bool all_single = true;

    for (int n = 0; n < root->num_pars; n++) {
        struct tl_parts *parts = root->pars[n];
        fix_filenames(parts, tl->demuxer->filename);
        struct timeline_par *par = build_timeline(tl, root, parts);
        if (!par)
            break;
        all_dash &= par->dash;
        all_no_clip &= par->no_clip;
        all_single &= par->num_parts == 1;
    }

    if (all_dash) {
        tl->format = "dash";
    } else if (all_no_clip && all_single) {
        tl->format = "multi";
    } else {
        tl->format = "edl";
    }

    talloc_free(root);
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
        return 0;
    }
    if (check >= DEMUX_CHECK_UNSAFE) {
        char header[sizeof(HEADER) - 1];
        int len = stream_read_peek(s, header, sizeof(header));
        if (len != strlen(HEADER) || memcmp(header, HEADER, len) != 0)
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
