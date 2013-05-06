/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ctype.h>

#include "talloc.h"

#include "core/mp_core.h"
#include "core/mp_msg.h"
#include "demux/demux.h"
#include "core/path.h"
#include "core/bstr.h"
#include "core/mp_common.h"
#include "stream/stream.h"


struct edl_source {
    struct bstr id;
    char *filename;
    int lineno;
};

struct edl_time {
    int64_t start;
    int64_t end;
    bool implied_start;
    bool implied_end;
};

struct edl_part {
    struct edl_time tl;
    struct edl_time src;
    int64_t duration;
    int id;
    int lineno;
};

static int find_edl_source(struct edl_source *sources, int num_sources,
                           struct bstr name)
{
    for (int i = 0; i < num_sources; i++)
        if (!bstrcmp(sources[i].id, name))
            return i;
    return -1;
}

void build_edl_timeline(struct MPContext *mpctx)
{
    const struct bstr file_prefix = bstr0("<");
    void *tmpmem = talloc_new(NULL);

    struct bstr *lines = bstr_splitlines(tmpmem, mpctx->demuxer->file_contents);
    int linec = MP_TALLOC_ELEMS(lines);
    struct bstr header = bstr0("mplayer EDL file, version ");
    if (!linec || !bstr_startswith(lines[0], header)) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Bad EDL header!\n");
        goto out;
    }
    struct bstr version = bstr_strip(bstr_cut(lines[0], header.len));
    if (bstrcmp(bstr0("2"), version)) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Unsupported EDL file version!\n");
        goto out;
    }
    int num_sources = 0;
    int num_parts = 0;
    for (int i = 1; i < linec; i++) {
        if (bstr_startswith(lines[i], file_prefix)) {
            num_sources++;
        } else {
            int comment = bstrchr(lines[i], '#');
            if (comment >= 0)
                lines[i] = bstr_splice(lines[i], 0, comment);
            if (bstr_strip(lines[i]).len)
                num_parts++;
        }
    }
    if (!num_parts) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "No parts in timeline!\n");
        goto out;
    }

    // Parse source filename definitions

    struct edl_source *edl_ids = talloc_array_ptrtype(tmpmem, edl_ids,
                                                      num_sources);
    num_sources = 0;
    for (int i = 1; i < linec; i++) {
        struct bstr line = lines[i];
        if (!bstr_startswith(line, file_prefix))
            continue;
        line = bstr_cut(line, file_prefix.len);
        struct bstr id = bstr_split(line, WHITESPACE, &line);
        if (find_edl_source(edl_ids, num_sources, id) >= 0) {
            mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Repeated ID on line %d!\n",
                   i+1);
            goto out;
        }
        if (!isalpha(*id.start)) {
            mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Invalid ID on line %d!\n",
                   i+1);
            goto out;
        }
        char *filename = mp_basename(bstrdup0(tmpmem, bstr_strip(line)));
        if (!strlen(filename)) {
            mp_msg(MSGT_CPLAYER, MSGL_ERR,
                   "EDL: Invalid filename on line %d!\n", i+1);
            goto out;
        }
        struct bstr dirname = mp_dirname(mpctx->demuxer->filename);
        char *fullname = mp_path_join(tmpmem, dirname, bstr0(filename));
        edl_ids[num_sources++] = (struct edl_source){id, fullname, i+1};
    }

    // Parse timeline part definitions

    struct edl_part *parts = talloc_array_ptrtype(tmpmem, parts, num_parts);
    int total_parts = num_parts;
    num_parts = 0;
    for (int i = 1; i < linec; i++) {
        struct bstr line = bstr_strip(lines[i]);
        if (!line.len || bstr_startswith(line, file_prefix))
            continue;
        parts[num_parts] = (struct edl_part){{-1, -1}, {-1, -1}, 0, -1};
        parts[num_parts].lineno = i + 1;
        for (int s = 0; s < 2; s++) {
            struct edl_time *p = !s ? &parts[num_parts].tl :
                &parts[num_parts].src;
            while (1) {
                struct bstr t = bstr_split(line, WHITESPACE, &line);
                if (!t.len) {
                    if (!s && num_parts < total_parts - 1) {
                        mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: missing source "
                               "identifier on line %d (not last)!\n", i+1);
                        goto out;
                    }
                    break;
                }
                if (isalpha(*t.start)) {
                    if (s)
                        goto bad;
                    parts[num_parts].id = find_edl_source(edl_ids, num_sources,
                                                          t);
                    if (parts[num_parts].id < 0) {
                        mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Undefined source "
                               "identifier on line %d!\n", i+1);
                        goto out;
                    }
                    break;
                }
                while (t.len) {
                    struct bstr next;
                    struct bstr arg = bstr_split(t, "+-", &next);
                    if (!arg.len) {
                        next = bstr_split(line, WHITESPACE, &line);
                        arg = bstr_split(next, "+-", &next);
                    }
                    if (!arg.len)
                        goto bad;
                    int64_t val;
                    if (!bstrcmp(arg, bstr0("*")))
                        val = -1;
                    else if (isdigit(*arg.start)) {
                        val = bstrtoll(arg, &arg, 10) * 1000000000;
                        if (arg.len && *arg.start == '.') {
                            int len = arg.len - 1;
                            arg = bstr_splice(arg, 1, 10);
                            int64_t val2 = bstrtoll(arg, &arg, 10);
                            if (arg.len)
                                goto bad;
                            for (; len < 9; len++)
                                val2 *= 10;
                            val += val2;
                        }
                    } else
                        goto bad;
                    int c = *t.start;
                    if (isdigit(c) || c == '*') {
                        if (val < 0)
                            p->implied_start = true;
                        else
                            p->start = val;
                    } else if (c == '-') {
                        if (val < 0)
                            p->implied_end = true;
                        else
                            p->end = val;
                    } else if (c == '+') {
                        if (val < 0)
                            goto bad;
                        if (val == 0) {
                            mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: zero duration "
                                   "on line %d!\n", i+1);
                            goto out;
                        }
                        parts[num_parts].duration = val;
                    } else
                        goto bad;
                    t = next;
                }
            }
        }
        num_parts++;
        continue;
    bad:
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Malformed line %d!\n", i+1);
        goto out;
    }

    // Fill in implied start/stop/duration values

    int64_t *times = talloc_zero_array(tmpmem, int64_t, num_sources);
    while (1) {
        int64_t time = 0;
        for (int i = 0; i < num_parts; i++) {
            for (int s = 0; s < 2; s++) {
                struct edl_time *p = s ? &parts[i].tl : &parts[i].src;
                if (!s && parts[i].id == -1)
                    continue;
                int64_t *t = s ? &time : times + parts[i].id;
                p->implied_start |= s && *t >= 0;
                if (p->implied_start && p->start >= 0 && *t >= 0
                    && p->start != *t) {
                    mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Inconsistent line "
                           "%d!\n", parts[i].lineno);
                    goto out;
                }
                if (p->start >= 0)
                    *t = p->start;
                if (p->implied_start)
                    p->start = *t;
                if (*t >= 0 && parts[i].duration)
                    *t += parts[i].duration;
                else
                    *t = -1;
                if (p->end >= 0)
                    *t = p->end;
            }
        }
        for (int i = 0; i < num_sources; i++)
            times[i] = -1;
        time = -1;
        for (int i = num_parts - 1; i >= 0; i--) {
            for (int s = 0; s < 2; s++) {
                struct edl_time *p = s ? &parts[i].tl : &parts[i].src;
                if (!s && parts[i].id == -1)
                    continue;
                int64_t *t = s ? &time : times + parts[i].id;
                p->implied_end |= s && *t >= 0;
                if (p->implied_end && p->end >= 0 && *t >=0 && p->end != *t) {
                    mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Inconsistent line "
                           "%d!\n", parts[i].lineno);
                    goto out;
                }
                if (p->end >= 0)
                    *t = p->end;
                if (p->implied_end)
                    p->end = *t;
                if (*t >= 0 && parts[i].duration) {
                    *t -= parts[i].duration;
                    if (*t < 0) {
                        mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Negative time "
                               "on line %d!\n", parts[i].lineno);
                        goto out;
                    }
                } else
                    *t = -1;
                if (p->start >= 0)
                    *t = p->start;
            }
        }
        int missing_duration = -1;
        int missing_srcstart = -1;
        bool anything_done = false;
        for (int i = 0; i < num_parts; i++) {
            int64_t duration = parts[i].duration;
            if (parts[i].tl.start >= 0 && parts[i].tl.end >= 0) {
                int64_t duration2 = parts[i].tl.end - parts[i].tl.start;
                if (duration && duration != duration2)
                    goto incons;
                duration = duration2;
                if (duration <= 0)
                    goto neg;
            }
            if (parts[i].src.start >= 0 && parts[i].src.end >= 0) {
                int64_t duration2 = parts[i].src.end - parts[i].src.start;
                if (duration && duration != duration2) {
                incons:
                    mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Inconsistent line "
                           "%d!\n", i+1);
                    goto out;
                }
                duration = duration2;
                if (duration <= 0) {
                neg:
                    mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: duration <= 0 on "
                           "line %d!\n", parts[i].lineno);
                    goto out;
                }
            }
            if (parts[i].id == -1)
                continue;
            if (!duration)
                missing_duration = i;
            else if (!parts[i].duration)
                anything_done = true;
            parts[i].duration = duration;
            if (duration && parts[i].src.start < 0) {
                if (parts[i].src.end < 0)
                    missing_srcstart = i;
                else
                    parts[i].src.start = parts[i].src.end - duration;
            }
        }
        if (!anything_done) {
            if (missing_duration >= 0) {
                mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Could not determine "
                        "duration for line %d!\n",
                        parts[missing_duration].lineno);
                goto out;
            }
            if (missing_srcstart >= 0) {
                mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: no source start time for "
                       "line %d!\n", parts[missing_srcstart].lineno);
                    goto out;
            }
            break;
        }
    }

    // Open source files

    struct demuxer **sources = talloc_array_ptrtype(NULL, sources,
                                                    num_sources + 1);
    mpctx->sources = sources;
    sources[0] = mpctx->demuxer;
    mpctx->num_sources = 1;

    for (int i = 0; i < num_sources; i++) {
        int format = 0;
        struct stream *s = open_stream(edl_ids[i].filename, &mpctx->opts,
                                       &format);
        if (!s)
            goto openfail;
        struct demuxer *d = demux_open(&mpctx->opts, s, format,
                                       mpctx->opts.audio_id,
                                       mpctx->opts.video_id,
                                       mpctx->opts.sub_id,
                                       edl_ids[i].filename);
        if (!d) {
            free_stream(s);
        openfail:
            mp_msg(MSGT_CPLAYER, MSGL_ERR, "EDL: Could not open source "
                   "file on line %d!\n", edl_ids[i].lineno);
            goto out;
        }
        sources[mpctx->num_sources] = d;
        mpctx->num_sources++;
    }

    // Write final timeline structure

    struct timeline_part *timeline = talloc_array_ptrtype(NULL, timeline,
                                                          num_parts + 1);
    int64_t starttime = 0;
    for (int i = 0; i < num_parts; i++) {
        timeline[i].start = starttime / 1e9;
        starttime += parts[i].duration;
        timeline[i].source_start = parts[i].src.start / 1e9;
        timeline[i].source = sources[parts[i].id + 1];
    }
    if (parts[num_parts - 1].id != -1) {
        timeline[num_parts].start = starttime / 1e9;
        num_parts++;
    }
    mpctx->timeline = timeline;
    mpctx->num_timeline_parts = num_parts - 1;

 out:
    talloc_free(tmpmem);
}
