/*
 * Subtitles converter to SSA/ASS in order to allow special formatting
 *
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "mp_msg.h"
#include "subassconvert.h"
#include "bstr.h"
#include "libavutil/common.h"

struct line {
    char *buf;
    int bufsize;
    int len;
};

#ifdef __GNUC__
static void append_text(struct line *dst, char *fmt, ...) __attribute__ ((format(printf, 2, 3)));
#endif

static void append_text(struct line *dst, char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int ret = vsnprintf(dst->buf + dst->len, dst->bufsize - dst->len, fmt, va);
    if (ret < 0)
        goto out;
    dst->len += ret;
    if (dst->len > dst->bufsize)
        dst->len = dst->bufsize;
 out:
    va_end(va);
}

static int indexof(const char *s, int c)
{
    char *f = strchr(s, c);
    return f ? (f - s) : -1;
}



/*
 *      SubRip
 *
 *      Support basic tags (italic, bold, underline, strike-through)
 *      and font tag with size, color and face attributes.
 *
 */

struct font_tag {
    int size;
    uint32_t color;
    struct bstr face;
    bool has_size : 1;
    bool has_color : 1;
    bool has_face : 1;
};

static const struct tag_conv {
    char *from;
    char *to;
} subrip_basic_tags[] = {
    {"<i>", "{\\i1}"}, {"</i>", "{\\i0}"},
    {"<b>", "{\\b1}"}, {"</b>", "{\\b0}"},
    {"<u>", "{\\u1}"}, {"</u>", "{\\u0}"},
    {"<s>", "{\\s1}"}, {"</s>", "{\\s0}"},
    {"{", "\\{"}, {"}", "\\}"},
    {"\r\n", "\\N"}, {"\n", "\\N"}, {"\r", "\\N"},
};

static const struct {
    char *s;
    uint32_t v;
} subrip_web_colors[] = {
    /* 16 named HTML colors in BGR format */
    {"red",     0x0000ff}, {"blue",   0xff0000}, {"lime",   0x00ff00},
    {"aqua",    0xffff00}, {"purple", 0x800080}, {"yellow", 0x00ffff},
    {"fuchsia", 0xff00ff}, {"white",  0xffffff}, {"gray",   0x808080},
    {"maroon",  0x000080}, {"olive",  0x008080}, {"black",  0x000000},
    {"silver",  0xc0c0c0}, {"teal",   0x808000}, {"green",  0x008000},
    {"navy",    0x800000}
};

#define SUBRIP_MAX_STACKED_FONT_TAGS    16

void subassconvert_subrip(const char *orig, char *dest, int dest_buffer_size)
{
    /* line is not const to avoid warnings with strtol, etc.
     * orig content won't be changed */
    char *line = (char *)orig;
    struct line new_line = {
        .buf = dest,
        .bufsize = dest_buffer_size,
    };
    struct font_tag font_stack[SUBRIP_MAX_STACKED_FONT_TAGS + 1];
    font_stack[0] = (struct font_tag){0}; // type with all defaults
    int sp = 0;

    while (*line && new_line.len < new_line.bufsize - 1) {
        char *orig_line = line;

        for (int i = 0; i < FF_ARRAY_ELEMS(subrip_basic_tags); i++) {
            const struct tag_conv *tag = &subrip_basic_tags[i];
            int from_len = strlen(tag->from);
            if (strncmp(line, tag->from, from_len) == 0) {
                append_text(&new_line, "%s", tag->to);
                line += from_len;
            }
        }

        if (strncmp(line, "</font>", 7) == 0) {
            /* Closing font tag */
            line += 7;

            if (sp > 0) {
                struct font_tag *tag = &font_stack[sp];
                struct font_tag *last_tag = &tag[-1];
                sp--;

                if (tag->has_size) {
                    if (!last_tag->has_size)
                        append_text(&new_line, "{\\fs}");
                    else if (last_tag->size != tag->size)
                        append_text(&new_line, "{\\fs%d}", last_tag->size);
                }

                if (tag->has_color) {
                    if (!last_tag->has_color)
                        append_text(&new_line, "{\\c}");
                    else if (last_tag->color != tag->color)
                        append_text(&new_line, "{\\c&H%06X&}", last_tag->color);
                }

                if (tag->has_face) {
                    if (!last_tag->has_face)
                        append_text(&new_line, "{\\fn}");
                    else if (bstrcmp(last_tag->face, tag->face) != 0)
                        append_text(&new_line, "{\\fn%.*s}",
                                    BSTR_P(last_tag->face));
                }
            }
        } else if (strncmp(line, "<font ", 6) == 0
                   && sp + 1 < FF_ARRAY_ELEMS(font_stack)) {
            /* Opening font tag */
            char *potential_font_tag_start = line;
            int len_backup = new_line.len;
            struct font_tag *tag = &font_stack[sp + 1];
            bool has_valid_attr = false;

            *tag = tag[-1]; // keep values from previous tag
            line += 6;

            while (*line && *line != '>') {
                if (strncmp(line, "size=\"", 6) == 0) {
                    line += 6;
                    tag->size = strtol(line, &line, 10);
                    if (*line != '"')
                        break;
                    append_text(&new_line, "{\\fs%d}", tag->size);
                    tag->has_size = true;
                    has_valid_attr = true;
                } else if (strncmp(line, "color=\"", 7) == 0) {
                    line += 7;
                    if (*line == '#') {
                        // #RRGGBB format
                        line++;
                        tag->color = strtol(line, &line, 16) & 0x00ffffff;
                        if (*line != '"')
                            break;
                        tag->color = ((tag->color & 0xff) << 16)
                            | (tag->color & 0xff00)
                            | ((tag->color & 0xff0000) >> 16);
                    } else {
                        // Standard web colors
                        int len = indexof(line, '"');
                        if (len <= 0)
                            break;
                        for (int i = 0; i < FF_ARRAY_ELEMS(subrip_web_colors); i++) {
                            char *color = subrip_web_colors[i].s;
                            if (strlen(color) == len
                                && strncasecmp(line, color, len) == 0) {
                                tag->color = subrip_web_colors[i].v;
                                goto foundcolor;
                            }
                        }

                        /* We didn't find any matching color */
                        mp_tmsg(MSGT_SUBREADER, MSGL_WARN,
                                "SubRip: unknown font color in subtitle: %s\n", orig);
                        append_text(&new_line, "{\\c}");
                        line += len + 1;
                        continue;

                    foundcolor:
                        line += len;
                    }
                    append_text(&new_line, "{\\c&H%06X&}", tag->color);
                    tag->has_color = true;
                    has_valid_attr = true;
                } else if (strncmp(line, "face=\"", 6) == 0) {
                    /* Font face attribute */
                    line += 6;
                    int len = indexof(line, '"');
                    if (len <= 0)
                        break;
                    tag->face.start = line;
                    tag->face.len = len;
                    line += len;
                    append_text(&new_line, "{\\fn%.*s}", BSTR_P(tag->face));
                    tag->has_face = true;
                    has_valid_attr = true;
                }
                line++;
            }

            if (!has_valid_attr || *line != '>') { /* Not valid font tag */
                line = potential_font_tag_start;
                new_line.len = len_backup;
            } else {
                sp++;
                line++;
            }
        }

        /* Tag conversion code didn't match */
        if (line == orig_line)
            new_line.buf[new_line.len++] = *line++;
    }
    new_line.buf[new_line.len] = 0;
}


/*
 *      MicroDVD
 *
 *      Based on the specifications found here:
 *      https://trac.videolan.org/vlc/ticket/1825#comment:6
 */

struct microdvd_tag {
    char key;
    int persistent;
    uint32_t data1;
    uint32_t data2;
    struct bstr data_string;
};

#define MICRODVD_PERSISTENT_OFF     0
#define MICRODVD_PERSISTENT_ON      1
#define MICRODVD_PERSISTENT_OPENED  2

// Color, Font, Size, cHarset, stYle, Position, cOordinate
#define MICRODVD_TAGS               "cfshyYpo"

static void microdvd_set_tag(struct microdvd_tag *tags, struct microdvd_tag tag)
{
    int tag_index = indexof(MICRODVD_TAGS, tag.key);

    if (tag_index < 0)
        return;
    memcpy(&tags[tag_index], &tag, sizeof(tag));
}

// italic, bold, underline, strike-through
#define MICRODVD_STYLES             "ibus"

static char *microdvd_load_tags(struct microdvd_tag *tags, char *s)
{
    while (*s == '{') {
        char *start = s;
        char tag_char = *(s + 1);
        struct microdvd_tag tag = {0};

        if (!tag_char || *(s + 2) != ':')
            break;
        s += 3;

        switch (tag_char) {

        /* Style */
        case 'Y':
            tag.persistent = MICRODVD_PERSISTENT_ON;
        case 'y':
            while (*s && *s != '}') {
                int style_index = indexof(MICRODVD_STYLES, *s);

                if (style_index >= 0)
                    tag.data1 |= (1 << style_index);
                s++;
            }
            if (*s != '}')
                break;
            /* We must distinguish persistent and non-persistent styles
             * to handle this kind of style tags: {y:ib}{Y:us} */
            tag.key = tag_char;
            break;

        /* Color */
        case 'C':
            tag.persistent = MICRODVD_PERSISTENT_ON;
        case 'c':
            tag.data1 = strtol(s, &s, 16) & 0x00ffffff;
            if (*s != '}')
                break;
            tag.key = 'c';
            break;

        /* Font name */
        case 'F':
            tag.persistent = MICRODVD_PERSISTENT_ON;
        case 'f':
        {
            int len = indexof(s, '}');
            if (len < 0)
                break;
            tag.data_string.start = s;
            tag.data_string.len = len;
            s += len;
            tag.key = 'f';
            break;
        }

        /* Font size */
        case 'S':
            tag.persistent = MICRODVD_PERSISTENT_ON;
        case 's':
            tag.data1 = strtol(s, &s, 10);
            if (*s != '}')
                break;
            tag.key = 's';
            break;

        /* Charset */
        case 'H':
        {
            //TODO: not yet handled, just parsed.
            int len = indexof(s, '}');
            if (len < 0)
                break;
            tag.data_string.start = s;
            tag.data_string.len = len;
            s += len;
            tag.key = 'h';
            break;
        }

        /* Position */
        case 'P':
            tag.persistent = MICRODVD_PERSISTENT_ON;
            tag.data1 = (*s++ == '1');
            if (*s != '}')
                break;
            tag.key = 'p';
            break;

        /* Coordinates */
        case 'o':
            tag.persistent = MICRODVD_PERSISTENT_ON;
            tag.data1 = strtol(s, &s, 10);
            if (*s != ',')
                break;
            s++;
            tag.data2 = strtol(s, &s, 10);
            if (*s != '}')
                break;
            tag.key = 'o';
            break;

        default:    /* Unknown tag, we consider it to be text */
            break;
        }

        if (tag.key == 0)
            return start;

        microdvd_set_tag(tags, tag);
        s++;
    }
    return s;
}

static void microdvd_open_tags(struct line *new_line, struct microdvd_tag *tags)
{
    for (int i = 0; i < sizeof(MICRODVD_TAGS) - 1; i++) {
        if (tags[i].persistent == MICRODVD_PERSISTENT_OPENED)
            continue;
        switch (tags[i].key) {
        case 'Y':
        case 'y':
            for (int sidx = 0; sidx < sizeof(MICRODVD_STYLES) - 1; sidx++)
                if (tags[i].data1 & (1 << sidx))
                    append_text(new_line, "{\\%c1}", MICRODVD_STYLES[sidx]);
            break;

        case 'c':
            append_text(new_line, "{\\c&H%06X&}", tags[i].data1);
            break;

        case 'f':
            append_text(new_line, "{\\fn%.*s}", BSTR_P(tags[i].data_string));
            break;

        case 's':
            append_text(new_line, "{\\fs%d}", tags[i].data1);
            break;

        case 'p':
            if (tags[i].data1 == 0)
                append_text(new_line, "{\\an8}");
            break;

        case 'o':
            append_text(new_line, "{\\pos(%d,%d)}",
                        tags[i].data1, tags[i].data2);
            break;
        }
        if (tags[i].persistent == MICRODVD_PERSISTENT_ON)
            tags[i].persistent = MICRODVD_PERSISTENT_OPENED;
    }
}

static void microdvd_close_no_persistent_tags(struct line *new_line,
                                              struct microdvd_tag *tags)
{
    int i;

    for (i = sizeof(MICRODVD_TAGS) - 2; i; i--) {
        if (tags[i].persistent != MICRODVD_PERSISTENT_OFF)
            continue;
        switch (tags[i].key) {

        case 'y':
            for (int sidx = sizeof(MICRODVD_STYLES) - 2; sidx >= 0; sidx--)
                if (tags[i].data1 & (1 << sidx))
                    append_text(new_line, "{\\%c0}", MICRODVD_STYLES[sidx]);
            break;

        case 'c':
            append_text(new_line, "{\\c}");
            break;

        case 'f':
            append_text(new_line, "{\\fn}");
            break;

        case 's':
            append_text(new_line, "{\\fs}");
            break;
        }
        tags[i].key = 0;
    }
}

void subassconvert_microdvd(const char *orig, char *dest, int dest_buffer_size)
{
    /* line is not const to avoid warnings with strtol, etc.
     * orig content won't be changed */
    char *line = (char *)orig;
    struct line new_line = {
        .buf = dest,
        .bufsize = dest_buffer_size,
    };
    struct microdvd_tag tags[sizeof(MICRODVD_TAGS) - 1] = {{0}};

    while (*line) {
        line = microdvd_load_tags(tags, line);
        microdvd_open_tags(&new_line, tags);

        while (*line && *line != '|')
            new_line.buf[new_line.len++] = *line++;

        if (*line == '|') {
            microdvd_close_no_persistent_tags(&new_line, tags);
            append_text(&new_line, "\\N");
            line++;
        }
    }
    new_line.buf[new_line.len] = 0;
}
