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

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <mpv/client.h>

#include "common/common.h"
#include "common/msg.h"
#include "demux.h"
#include "demux/packet.h"
#include "misc/bstr.h"
#include "misc/json.h"
#include "misc/node.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "stream/stream.h"

#define PROBE_SIZE 512

#define OPT_BASE_STRUCT struct demux_json3_opts
struct demux_json3_opts {
    bool youtube_styling;
    int word_timing;    // -1: auto (follows youtube_styling)
    int multi_line;     // -1: auto (follows youtube_styling)
    int scroll_ms;
};

#define OPT_AUTO_BOOL(field) \
    OPT_CHOICE(field, {"auto", -1}, {"no", 0}, {"yes", 1})

const struct m_sub_options demux_json3_conf = {
    .opts = (const m_option_t[]) {
        {"youtube-styling", OPT_BOOL(youtube_styling)},
        {"word-timing", OPT_AUTO_BOOL(word_timing)},
        {"multi-line", OPT_AUTO_BOOL(multi_line)},
        {"scroll-ms", OPT_INT(scroll_ms), M_RANGE(0, 1000)},
        {0}
    },
    .size = sizeof(struct demux_json3_opts),
    .defaults = &(const struct demux_json3_opts){
        .youtube_styling = true,
        .word_timing = -1,
        .multi_line = -1,
        .scroll_ms = 250,
    },
    .change_flags = UPDATE_DEMUXER,
};

static bool auto_bool(int val, bool auto_val)
{
    return val < 0 ? auto_val : val;
}

static const char ass_header_plain[] =
    "[Script Info]\n"
    "; Converted from YouTube json3 by mpv\n"
    "ScriptType: v4.00+\n"
    "ScaledBorderAndShadow: yes\n"
    "\n"
    "[Events]\n"
    "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

#define YT_FONTSIZE 44
#define YT_PADDING 6

#define YT_X_LEFT 480
#define YT_X_RIGHT 1440
#define YT_Y_BOTTOM 994
#define YT_ROW_H (YT_FONTSIZE + 2 * YT_PADDING)
#define YT_Y_TOP (YT_Y_BOTTOM - YT_ROW_H)

#define YT_WIN_X0 (YT_X_LEFT - YT_PADDING)
#define YT_WIN_X1 (YT_X_RIGHT + YT_PADDING)
#define YT_WIN_Y0 (YT_Y_TOP - YT_FONTSIZE - YT_PADDING)
#define YT_WIN_Y1 (YT_Y_BOTTOM + YT_PADDING)
// y where the bottom-row and top-row slots meet
#define YT_SEAM (YT_Y_TOP + YT_PADDING)

// overlap the two rows to avoid a seam on non-integer scaling factors
#define YT_OVERLAP 1
#define YT_YBORD (YT_PADDING + YT_OVERLAP)

#define YT_STYLE_NAME "YouTube"

static char *yt_header(void *ta)
{
    return talloc_asprintf(ta,
        "[Script Info]\n"
        "; Converted from YouTube json3 by mpv\n"
        "ScriptType: v4.00+\n"
        "PlayResX: 1920\n"
        "PlayResY: 1080\n"
        "WrapStyle: 2\n"
        "ScaledBorderAndShadow: yes\n"
        "\n"
        "[V4+ Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
        "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, "
        "ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, "
        "MarginL, MarginR, MarginV, Encoding\n"
        "Style: " YT_STYLE_NAME ",sans-serif,%d,&H00FFFFFF,&H00FFFFFF,"
        "&H40080808,&H40080808,0,0,0,0,100,100,0,0,3,%d,0,1,%d,%d,%d,1\n"
        "\n"
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, "
        "Effect, Text\n",
        YT_FONTSIZE, YT_PADDING, YT_X_LEFT, 1920 - YT_X_RIGHT,
        1080 - YT_Y_BOTTOM);
}

static void yt_row_tags(void *ta, bstr *dst, int ybord, int clip_y0, int clip_y1)
{
    bstr_xappend_asprintf(ta, dst, "\\an1\\q2\\ybord%d\\clip(%d,%d,%d,%d)",
                          ybord, YT_WIN_X0, clip_y0, YT_WIN_X1, clip_y1);
}

static void yt_row_clip_anim(void *ta, bstr *dst, int64_t dur, int clip_y0,
                             int clip_y1)
{
    bstr_xappend_asprintf(ta, dst, "\\t(0,%" PRId64 ",\\clip(%d,%d,%d,%d))",
                          dur, YT_WIN_X0, clip_y0, YT_WIN_X1, clip_y1);
}

struct word {
    char *text;
    int64_t offset;
};

struct line {
    int64_t start;
    int64_t end;
    int win;
    bool append;
    bool is_break;
    struct word *words;
    int num_words;
};

struct win_state {
    int win;
    bool rolling;
    bstr row;
    int64_t row_end;
    bstr prev_row;
    int64_t prev_row_end;

    // "youtube" style: the top row is a separate event, emitted once the
    // time of the next scroll (which ends it) is known
    bool top_pending;
    bstr top;
    int64_t top_start;
    int64_t top_end;
    int64_t scroll_start; // start of the last scroll animation
};

struct ass_event {
    double pts;
    double duration;
    char *data;
};

struct priv {
    struct ass_event *events;
    int num_events;
    int pos;
    const char *style;
    int scroll_ms;      // "youtube" style: duration of the scroll animation
};

static bool probe_json3(bstr data)
{
    data = bstr_lstrip(data);
    if (!bstr_startswith0(data, "{"))
        return false;
    return bstr_find0(data, "\"wireMagic\"") >= 0 &&
           bstr_find0(data, "\"pb3\"") >= 0;
}

static int64_t node_get_int(struct mpv_node *map, const char *key, int64_t def)
{
    struct mpv_node *n = node_map_get(map, key);
    if (!n)
        return def;
    if (n->format == MPV_FORMAT_INT64)
        return n->u.int64;
    if (n->format == MPV_FORMAT_DOUBLE && isfinite(n->u.double_))
        return llrint(n->u.double_);
    return def;
}

static const char *node_get_string(struct mpv_node *map, const char *key)
{
    struct mpv_node *n = node_map_get(map, key);
    return n && n->format == MPV_FORMAT_STRING ? n->u.string : NULL;
}

static void append_ass_text(void *ta, bstr *dst, const char *s)
{
    for (; *s; s++) {
        switch (*s) {
        case '{':  bstr_xappend0(ta, dst, "\\{"); break;
        case '}':  bstr_xappend0(ta, dst, "\\}"); break;
        case '\n': bstr_xappend0(ta, dst, "\\N"); break;
        case '\r': break;
        default:   bstr_xappend(ta, dst, (bstr){(unsigned char *)s, 1});
        }
    }
}

static bool is_break_text(const char *s)
{
    bool nl = false;
    for (; *s; s++) {
        if (*s == '\n') {
            nl = true;
        } else if (*s != ' ' && *s != '\t' && *s != '\r') {
            return false;
        }
    }
    return nl;
}

static struct line *parse_lines(void *ta, struct mpv_node *root, int *out_num)
{
    struct line *lines = NULL;
    int num_lines = 0;

    struct mpv_node *events = node_map_get(root, "events");
    if (!events || events->format != MPV_FORMAT_NODE_ARRAY)
        goto done;

    for (int i = 0; i < events->u.list->num; i++) {
        struct mpv_node *ev = &events->u.list->values[i];
        if (ev->format != MPV_FORMAT_NODE_MAP)
            continue;
        struct mpv_node *segs = node_map_get(ev, "segs");
        if (!segs || segs->format != MPV_FORMAT_NODE_ARRAY)
            continue;

        struct line line = {
            .start = node_get_int(ev, "tStartMs", 0),
            .win = node_get_int(ev, "wWinId", -1),
            .append = node_get_int(ev, "aAppend", 0) != 0,
        };
        line.end = line.start + MPMAX(node_get_int(ev, "dDurationMs", 0), 0);

        bstr raw = {0};
        for (int n = 0; n < segs->u.list->num; n++) {
            struct mpv_node *seg = &segs->u.list->values[n];
            if (seg->format != MPV_FORMAT_NODE_MAP)
                continue;
            const char *utf8 = node_get_string(seg, "utf8");
            if (!utf8)
                continue;
            bstr_xappend0(ta, &raw, utf8);

            bstr text = {0};
            append_ass_text(ta, &text, utf8);
            struct word w = {
                .text = text.start ? (char *)text.start : talloc_strdup(ta, ""),
                .offset = MPMAX(node_get_int(seg, "tOffsetMs", 0), 0),
            };
            MP_TARRAY_APPEND(ta, line.words, line.num_words, w);
        }

        if (!raw.len)
            continue;
        line.is_break = line.append && is_break_text(raw.start);
        if (!line.is_break && line.end <= line.start)
            continue;
        MP_TARRAY_APPEND(ta, lines, num_lines, line);
    }

done:
    *out_num = num_lines;
    return lines;
}

static struct win_state *get_win(void *ta, struct win_state **wins, int *num,
                                 int win)
{
    for (int n = 0; n < *num; n++) {
        if ((*wins)[n].win == win)
            return &(*wins)[n];
    }
    MP_TARRAY_APPEND(ta, *wins, *num, (struct win_state){
        .win = win,
        .scroll_start = INT64_MIN / 2, // no scroll yet
    });
    return &(*wins)[*num - 1];
}

static void add_event(struct priv *p, int64_t start, int64_t end, bstr text)
{
    if (end <= start || !text.len)
        return;
    struct ass_event ev = {
        .pts = start / 1000.0,
        .duration = (end - start) / 1000.0,
        .data = talloc_asprintf(p, "%d,0,%s,,0,0,0,,%.*s",
                                p->num_events, p->style, BSTR_P(text)),
    };
    MP_TARRAY_APPEND(p, p->events, p->num_events, ev);
}

// Writes the override tags of the event covering [start, end] and returns
// the end of the event
typedef int64_t (*step_fn)(void *ta, bstr *dst, int64_t start, int64_t end,
                           void *ctx);

static void add_line_events(struct priv *p, void *ta, bstr prefix,
                            struct line *line, int64_t start, int64_t end,
                            bool timing, step_fn fn, void *fn_ctx)
{
    if (end <= start)
        return;

    int64_t step_start = start;
    int step_first = 0;
    while (step_first < line->num_words) {
        int64_t step_end = end;
        int next = line->num_words;
        if (timing) {
            for (int n = step_first + 1; n < line->num_words; n++) {
                int64_t t = start + line->words[n].offset;
                if (t > step_start && t < end) {
                    step_end = t;
                    next = n;
                    break;
                }
            }
        }

        bstr text = {0};
        int64_t ev_end = step_end;
        if (fn) {
            ev_end = fn(ta, &text, step_start, step_end, fn_ctx);
            if (ev_end <= step_start || ev_end > step_end)
                ev_end = step_end;
        }
        bstr_xappend(ta, &text, prefix);
        for (int n = 0; n < next; n++)
            bstr_xappend0(ta, &text, line->words[n].text);
        if (next < line->num_words) {
            bstr_xappend0(ta, &text, "{\\alpha&HFF&}");
            for (int n = next; n < line->num_words; n++)
                bstr_xappend0(ta, &text, line->words[n].text);
        }
        add_event(p, step_start, ev_end, text);

        step_start = ev_end;
        if (ev_end == step_end)
            step_first = next;
    }
}

struct yt_row_ctx {
    struct priv *p;
    struct win_state *ws;
};

static int64_t yt_bottom_row_step(void *ta, bstr *dst, int64_t start,
                                  int64_t end, void *ctx)
{
    struct yt_row_ctx *c = ctx;
    struct win_state *ws = c->ws;
    int scroll_ms = c->p->scroll_ms;
    int64_t d = start - ws->scroll_start;
    if (d >= 0 && d < scroll_ms) {
        int64_t r = scroll_ms - d;
        int64_t shift = YT_ROW_H * r / scroll_ms;
        bstr_xappend0(ta, dst, "{");
        yt_row_tags(ta, dst, YT_YBORD, YT_SEAM + shift, YT_WIN_Y1);
        yt_row_clip_anim(ta, dst, r, YT_SEAM, YT_WIN_Y1);
        bstr_xappend_asprintf(ta, dst,
            "\\move(%d,%" PRId64 ",%d,%d,0,%" PRId64 ")}",
            YT_X_LEFT, YT_Y_BOTTOM - YT_OVERLAP + shift,
            YT_X_LEFT, YT_Y_BOTTOM - YT_OVERLAP, r);
        return MPMIN(end, ws->scroll_start + scroll_ms);
    }

    bstr_xappend0(ta, dst, "{");
    yt_row_tags(ta, dst, YT_YBORD, YT_SEAM, YT_WIN_Y1);
    bstr_xappend_asprintf(ta, dst, "\\pos(%d,%d)}",
                          YT_X_LEFT, YT_Y_BOTTOM - YT_OVERLAP);
    return end;
}

static int64_t yt_cue_step(void *ta, bstr *dst, int64_t start, int64_t end,
                           void *ctx)
{
    bstr_xappend_asprintf(ta, dst, "{\\an2\\pos(%d,%d)}",
                          (YT_X_LEFT + YT_X_RIGHT) / 2, YT_Y_BOTTOM);
    return end;
}

static void yt_flush_top(struct priv *p, void *ta, struct win_state *ws,
                         int64_t end_limit)
{
    if (!ws->top_pending)
        return;
    ws->top_pending = false;
    bstr text = {0};
    bstr_xappend0(ta, &text, "{");
    yt_row_tags(ta, &text, YT_YBORD, YT_WIN_Y0, YT_SEAM);
    bstr_xappend_asprintf(ta, &text, "\\pos(%d,%d)}",
                          YT_X_LEFT, YT_Y_TOP + YT_OVERLAP);
    bstr_xappend(ta, &text, ws->top);
    add_event(p, ws->top_start, MPMIN(ws->top_end, end_limit), text);
}

// starts a new scroll: slides the top row fully off-screen
// (or flushes it if it isn't visible anymore), then slides the bottom row up into the top slot
static void yt_scroll(struct priv *p, void *ta, struct win_state *ws, int64_t t)
{
    int scroll_ms = p->scroll_ms;
    int64_t t_end = t + scroll_ms;

    if (ws->top_pending && ws->top_end >= t) {
        bstr text = {0};
        bstr_xappend0(ta, &text, "{");
        yt_row_tags(ta, &text, YT_YBORD, YT_WIN_Y0, YT_SEAM);
        yt_row_clip_anim(ta, &text, scroll_ms, YT_WIN_Y0, YT_WIN_Y0);
        bstr_xappend_asprintf(ta, &text, "\\move(%d,%d,%d,%d,0,%d)}",
                              YT_X_LEFT, YT_Y_TOP + YT_OVERLAP,
                              YT_X_LEFT, YT_Y_TOP + YT_OVERLAP - YT_ROW_H,
                              scroll_ms);
        bstr_xappend(ta, &text, ws->top);
        add_event(p, t, t_end, text);
    }
    yt_flush_top(p, ta, ws, t);

    bstr text = {0};
    bstr_xappend0(ta, &text, "{");
    yt_row_tags(ta, &text, YT_PADDING + 2 * YT_OVERLAP, YT_SEAM, YT_WIN_Y1);
    yt_row_clip_anim(ta, &text, scroll_ms, YT_WIN_Y0, YT_SEAM);
    bstr_xappend_asprintf(ta, &text, "\\move(%d,%d,%d,%d,0,%d)}",
                          YT_X_LEFT, YT_Y_BOTTOM - YT_OVERLAP,
                          YT_X_LEFT, YT_Y_TOP + YT_OVERLAP, scroll_ms);
    bstr_xappend(ta, &text, ws->prev_row);
    add_event(p, t, MPMIN(t_end, ws->prev_row_end), text);

    ws->top_pending = true;
    ws->top = ws->prev_row;
    ws->top_start = t_end;
    ws->top_end = ws->prev_row_end;
    ws->scroll_start = t;
}

static int cmp_event(const void *pa, const void *pb)
{
    const struct ass_event *a = pa, *b = pb;
    if (a->pts != b->pts)
        return a->pts < b->pts ? -1 : 1;
    return 0;
}

static void convert_lines(struct demuxer *demuxer, struct line *lines,
                          int num_lines, struct demux_json3_opts *opts)
{
    struct priv *p = demuxer->priv;
    void *tmp = talloc_new(NULL);
    struct win_state *wins = NULL;
    int num_wins = 0;
    bool yt = opts->youtube_styling;
    bool timing = auto_bool(opts->word_timing, yt);
    bool multi_line = auto_bool(opts->multi_line, yt);

    for (int i = 0; i < num_lines; i++) {
        if (lines[i].append)
            get_win(tmp, &wins, &num_wins, lines[i].win)->rolling = true;
    }

    for (int i = 0; i < num_lines; i++) {
        struct line *line = &lines[i];
        struct win_state *ws = get_win(tmp, &wins, &num_wins, line->win);

        if (!ws->rolling) {
            add_line_events(p, tmp, (bstr){0}, line, line->start, line->end,
                            timing, yt ? yt_cue_step : NULL, NULL);
            continue;
        }

        if (line->is_break) {
            ws->prev_row = ws->row;
            ws->prev_row_end = ws->row_end;
            ws->row = (bstr){0};
            ws->row_end = 0;
            continue;
        }

        if (ws->row.len && ws->row_end <= line->start) {
            ws->row = (bstr){0};
            ws->row_end = 0;
        }

        int64_t end = line->end;
        for (int j = i + 1; j < num_lines; j++) {
            if (lines[j].win == line->win && !lines[j].is_break) {
                end = MPMIN(end, lines[j].start);
                break;
            }
        }

        // keep showing the previous row above the new one until it expires
        bool has_prev = multi_line && ws->prev_row.len &&
                        ws->prev_row_end > line->start;

        bstr prefix = {0};
        if (yt) {
            if (!ws->row.len) {
                if (has_prev) {
                    yt_scroll(p, tmp, ws, line->start);
                } else {
                    yt_flush_top(p, tmp, ws, line->start);
                }
                ws->prev_row = (bstr){0};
            }
        } else if (has_prev) {
            bstr_xappend(tmp, &prefix, ws->prev_row);
            bstr_xappend0(tmp, &prefix, "\\N");
        }
        bstr_xappend(tmp, &prefix, ws->row);
        struct yt_row_ctx ctx = {p, ws};
        add_line_events(p, tmp, prefix, line, line->start, end,
                        timing, yt ? yt_bottom_row_step : NULL, &ctx);

        for (int n = 0; n < line->num_words; n++)
            bstr_xappend0(tmp, &ws->row, line->words[n].text);
        ws->row_end = MPMAX(ws->row_end, line->end);
    }

    for (int n = 0; n < num_wins; n++)
        yt_flush_top(p, tmp, &wins[n], INT64_MAX);

    talloc_free(tmp);

    if (p->num_events)
        qsort(p->events, p->num_events, sizeof(p->events[0]), cmp_event);
}

static int demux_open_json3(struct demuxer *demuxer, enum demux_check check)
{
    struct stream *s = demuxer->stream;

    if (check >= DEMUX_CHECK_UNSAFE) {
        char probe[PROBE_SIZE];
        int len = stream_read_peek(s, probe, sizeof(probe));
        if (len < 1 || !probe_json3((bstr){probe, len}))
            return -1;
    }

    struct priv *p = talloc_zero(demuxer, struct priv);
    demuxer->priv = p;
    struct demux_json3_opts *opts =
        mp_get_config_group(p, demuxer->global, &demux_json3_conf);

    void *tmp = talloc_new(NULL);
    bstr data = stream_read_complete(s, tmp, 64 * 1024 * 1024);
    if (!data.start) {
        talloc_free(tmp);
        return -1;
    }

    char *src = data.start;
    struct mpv_node root = {0};
    if (json_parse(tmp, &root, &src, MAX_JSON_DEPTH) < 0 ||
        root.format != MPV_FORMAT_NODE_MAP)
    {
        MP_ERR(demuxer, "Failed to parse json3 subtitle file.\n");
        talloc_free(tmp);
        return -1;
    }

    bool yt = opts->youtube_styling;
    p->style = yt ? YT_STYLE_NAME : "Default";
    p->scroll_ms = opts->scroll_ms;

    int num_lines = 0;
    struct line *lines = parse_lines(tmp, &root, &num_lines);
    convert_lines(demuxer, lines, num_lines, opts);
    talloc_free(tmp);

    MP_VERBOSE(demuxer, "Converted %d json3 events to %d ASS events.\n",
               num_lines, p->num_events);

    struct sh_stream *sh = demux_alloc_sh_stream(STREAM_SUB);

    sh->codec->codec = yt ? "ass" : "ass-text";
    sh->codec->codec_desc = "YouTube json3";
    char *header = yt ? yt_header(sh) : talloc_strdup(sh, ass_header_plain);
    sh->codec->extradata = (unsigned char *)header;
    sh->codec->extradata_size = strlen(header);
    demux_add_sh_stream(demuxer, sh);

    demuxer->seekable = true;
    demuxer->fully_read = true;
    demux_close_stream(demuxer);

    return 0;
}

static bool demux_read_packet_json3(struct demuxer *demuxer,
                                  struct demux_packet **pkt)
{
    struct priv *p = demuxer->priv;

    if (p->pos >= p->num_events)
        return false;

    struct ass_event *ev = &p->events[p->pos];
    struct demux_packet *dp = new_demux_packet_from(demuxer->packet_pool,
                                                    ev->data, strlen(ev->data));
    if (!dp)
        return true;

    dp->stream = 0;
    dp->pts = ev->pts;
    dp->duration = ev->duration;
    dp->pos = p->pos;
    dp->keyframe = true;
    p->pos++;

    *pkt = dp;
    return true;
}

static void demux_seek_json3(struct demuxer *demuxer, double seek_pts, int flags)
{
    struct priv *p = demuxer->priv;

    if (flags & SEEK_FACTOR) {
        double end = 0;
        for (int n = 0; n < p->num_events; n++)
            end = MPMAX(end, p->events[n].pts + p->events[n].duration);
        seek_pts *= end;
    }

    p->pos = 0;
    while (p->pos < p->num_events) {
        struct ass_event *ev = &p->events[p->pos];
        if (ev->pts + ev->duration > seek_pts)
            break;
        p->pos++;
    }
}

const struct demuxer_desc demuxer_desc_json3 = {
    .name = "json3",
    .desc = "YouTube json3 subtitles",
    .open = demux_open_json3,
    .read_packet = demux_read_packet_json3,
    .seek = demux_seek_json3,
};
