/*
 * Subtitles converter to SSA/ASS in order to allow special formatting
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <libavutil/common.h>

#include "common/msg.h"
#include "misc/bstr.h"
#include "sd.h"

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

static void convert_microdvd(const char *orig, char *dest, int dest_buffer_size)
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

static const char *const microdvd_ass_extradata =
    "[Script Info]\n"
    "ScriptType: v4.00+\n"
    "PlayResX: 384\n"
    "PlayResY: 288\n";

static bool supports_format(const char *format)
{
    return format && strcmp(format, "microdvd") == 0;
}

static int init(struct sd *sd)
{
    sd->output_codec = "ass-text";
    sd->output_extradata = (char *)microdvd_ass_extradata;
    sd->output_extradata_len = strlen(sd->output_extradata);
    return 0;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    char dest[SD_MAX_LINE_LEN];
    // Assume input buffer is padded with 0
    convert_microdvd(packet->buffer, dest, sizeof(dest));
    sd_conv_add_packet(sd, dest, strlen(dest), packet->pts, packet->duration);
}

const struct sd_functions sd_microdvd = {
    .name = "microdvd",
    .supports_format = supports_format,
    .init = init,
    .decode = decode,
    .get_converted = sd_conv_def_get_converted,
    .reset = sd_conv_def_reset,
};
