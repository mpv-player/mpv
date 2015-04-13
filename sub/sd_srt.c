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
#include <strings.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "common/common.h"
#include "common/msg.h"
#include "misc/bstr.h"
#include "misc/ctype.h"
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

static void append_text_n(struct line *dst, char *start, size_t length)
{
    append_text(dst, "%.*s", (int)length, start);
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
    {"}", "\\}"},
    {"\r\n", "\\N"}, {"\n", "\\N"}, {"\r", "\\N"},
};

static const struct {
    char *s;
    uint32_t v;
} subrip_web_colors[] = {
    /* Named CSS3 colors in RGB format; a subset of those
       at http://www.w3.org/TR/css3-color/#svg-color */
    {"aliceblue",           0xF0F8FF},
    {"antiquewhite",        0xFAEBD7},
    {"aqua",                0x00FFFF},
    {"aquamarine",          0x7FFFD4},
    {"azure",               0xF0FFFF},
    {"beige",               0xF5F5DC},
    {"bisque",              0xFFE4C4},
    {"black",               0x000000},
    {"blanchedalmond",      0xFFEBCD},
    {"blue",                0x0000FF},
    {"blueviolet",          0x8A2BE2},
    {"brown",               0xA52A2A},
    {"burlywood",           0xDEB887},
    {"cadetblue",           0x5F9EA0},
    {"chartreuse",          0x7FFF00},
    {"chocolate",           0xD2691E},
    {"coral",               0xFF7F50},
    {"cornflowerblue",      0x6495ED},
    {"cornsilk",            0xFFF8DC},
    {"crimson",             0xDC143C},
    {"cyan",                0x00FFFF},
    {"darkblue",            0x00008B},
    {"darkcyan",            0x008B8B},
    {"darkgoldenrod",       0xB8860B},
    {"darkgray",            0xA9A9A9},
    {"darkgreen",           0x006400},
    {"darkgrey",            0xA9A9A9},
    {"darkkhaki",           0xBDB76B},
    {"darkmagenta",         0x8B008B},
    {"darkolivegreen",      0x556B2F},
    {"darkorange",          0xFF8C00},
    {"darkorchid",          0x9932CC},
    {"darkred",             0x8B0000},
    {"darksalmon",          0xE9967A},
    {"darkseagreen",        0x8FBC8F},
    {"darkslateblue",       0x483D8B},
    {"darkslategray",       0x2F4F4F},
    {"darkslategrey",       0x2F4F4F},
    {"darkturquoise",       0x00CED1},
    {"darkviolet",          0x9400D3},
    {"deeppink",            0xFF1493},
    {"deepskyblue",         0x00BFFF},
    {"dimgray",             0x696969},
    {"dimgrey",             0x696969},
    {"dodgerblue",          0x1E90FF},
    {"firebrick",           0xB22222},
    {"floralwhite",         0xFFFAF0},
    {"forestgreen",         0x228B22},
    {"fuchsia",             0xFF00FF},
    {"gainsboro",           0xDCDCDC},
    {"ghostwhite",          0xF8F8FF},
    {"gold",                0xFFD700},
    {"goldenrod",           0xDAA520},
    {"gray",                0x808080},
    {"green",               0x008000},
    {"greenyellow",         0xADFF2F},
    {"grey",                0x808080},
    {"honeydew",            0xF0FFF0},
    {"hotpink",             0xFF69B4},
    {"indianred",           0xCD5C5C},
    {"indigo",              0x4B0082},
    {"ivory",               0xFFFFF0},
    {"khaki",               0xF0E68C},
    {"lavender",            0xE6E6FA},
    {"lavenderblush",       0xFFF0F5},
    {"lawngreen",           0x7CFC00},
    {"lemonchiffon",        0xFFFACD},
    {"lightblue",           0xADD8E6},
    {"lightcoral",          0xF08080},
    {"lightcyan",           0xE0FFFF},
    {"lightgoldenrodyellow", 0xFAFAD2},
    {"lightgray",           0xD3D3D3},
    {"lightgreen",          0x90EE90},
    {"lightgrey",           0xD3D3D3},
    {"lightpink",           0xFFB6C1},
    {"lightsalmon",         0xFFA07A},
    {"lightseagreen",       0x20B2AA},
    {"lightskyblue",        0x87CEFA},
    {"lightslategray",      0x778899},
    {"lightslategrey",      0x778899},
    {"lightsteelblue",      0xB0C4DE},
    {"lightyellow",         0xFFFFE0},
    {"lime",                0x00FF00},
    {"limegreen",           0x32CD32},
    {"linen",               0xFAF0E6},
    {"magenta",             0xFF00FF},
    {"maroon",              0x800000},
    {"mediumaquamarine",    0x66CDAA},
    {"mediumblue",          0x0000CD},
    {"mediumorchid",        0xBA55D3},
    {"mediumpurple",        0x9370DB},
    {"mediumseagreen",      0x3CB371},
    {"mediumslateblue",     0x7B68EE},
    {"mediumspringgreen",   0x00FA9A},
    {"mediumturquoise",     0x48D1CC},
    {"mediumvioletred",     0xC71585},
    {"midnightblue",        0x191970},
    {"mintcream",           0xF5FFFA},
    {"mistyrose",           0xFFE4E1},
    {"moccasin",            0xFFE4B5},
    {"navajowhite",         0xFFDEAD},
    {"navy",                0x000080},
    {"oldlace",             0xFDF5E6},
    {"olive",               0x808000},
    {"olivedrab",           0x6B8E23},
    {"orange",              0xFFA500},
    {"orangered",           0xFF4500},
    {"orchid",              0xDA70D6},
    {"palegoldenrod",       0xEEE8AA},
    {"palegreen",           0x98FB98},
    {"paleturquoise",       0xAFEEEE},
    {"palevioletred",       0xDB7093},
    {"papayawhip",          0xFFEFD5},
    {"peachpuff",           0xFFDAB9},
    {"peru",                0xCD853F},
    {"pink",                0xFFC0CB},
    {"plum",                0xDDA0DD},
    {"powderblue",          0xB0E0E6},
    {"purple",              0x800080},
    {"red",                 0xFF0000},
    {"rosybrown",           0xBC8F8F},
    {"royalblue",           0x4169E1},
    {"saddlebrown",         0x8B4513},
    {"salmon",              0xFA8072},
    {"sandybrown",          0xF4A460},
    {"seagreen",            0x2E8B57},
    {"seashell",            0xFFF5EE},
    {"sienna",              0xA0522D},
    {"silver",              0xC0C0C0},
    {"skyblue",             0x87CEEB},
    {"slateblue",           0x6A5ACD},
    {"slategray",           0x708090},
    {"slategrey",           0x708090},
    {"snow",                0xFFFAFA},
    {"springgreen",         0x00FF7F},
    {"steelblue",           0x4682B4},
    {"tan",                 0xD2B48C},
    {"teal",                0x008080},
    {"thistle",             0xD8BFD8},
    {"tomato",              0xFF6347},
    {"turquoise",           0x40E0D0},
    {"violet",              0xEE82EE},
    {"wheat",               0xF5DEB3},
    {"white",               0xFFFFFF},
    {"whitesmoke",          0xF5F5F5},
    {"yellow",              0xFFFF00},
    {"yellowgreen",         0x9ACD32},
};

#define SUBRIP_MAX_STACKED_FONT_TAGS    16

/* Read the HTML-style attribute starting at *s, and skip *s past the value.
 * Set attr and val to the parsed attribute name and value.
 * Return 0 on success, or -1 if no valid attribute was found.
 */
static int read_attr(char **s, struct bstr *attr, struct bstr *val)
{
    char *eq = strchr(*s, '=');
    if (!eq)
        return -1;
    attr->start = *s;
    attr->len = eq - *s;
    for (int i = 0; i < attr->len; i++)
        if (!mp_isalnum(attr->start[i]))
            return -1;
    val->start = eq + 1;
    bool quoted = val->start[0] == '"';
    if (quoted)
        val->start++;
    unsigned char *end = strpbrk(val->start, quoted ? "\"" : " >");
    if (!end)
        return -1;
    val->len = end - val->start;
    *s = end + quoted;
    return 0;
}

static void convert_subrip(struct sd *sd, const char *orig,
                           char *dest, int dest_buffer_size)
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

        for (int i = 0; i < MP_ARRAY_SIZE(subrip_basic_tags); i++) {
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
                   && sp + 1 < MP_ARRAY_SIZE(font_stack)) {
            /* Opening font tag */
            char *potential_font_tag_start = line;
            int len_backup = new_line.len;
            struct font_tag *tag = &font_stack[sp + 1];
            bool has_valid_attr = false;

            *tag = tag[-1]; // keep values from previous tag
            line += 6;

            while (*line && *line != '>') {
                if (*line == ' ') {
                    line++;
                    continue;
                }
                struct bstr attr, val;
                if (read_attr(&line, &attr, &val) < 0)
                    break;
                if (!bstrcmp0(attr, "size")) {
                    tag->size = bstrtoll(val, &val, 10);
                    if (val.len)
                        break;
                    append_text(&new_line, "{\\fs%d}", tag->size);
                    tag->has_size = true;
                    has_valid_attr = true;
                } else if (!bstrcmp0(attr, "color")) {
                    int found = 0;

                    // Try to lookup the string in standard web colors
                    for (int i = 0; i < MP_ARRAY_SIZE(subrip_web_colors); i++) {
                        char *color = subrip_web_colors[i].s;
                        if (bstrcasecmp(val, bstr0(color)) == 0) {
                            uint32_t xcolor = subrip_web_colors[i].v;
                            tag->color = ((xcolor & 0xff) << 16)
                                | (xcolor & 0xff00)
                                | ((xcolor & 0xff0000) >> 16);
                            found = 1;
                        }
                    }

                    // If it's not a web color it must be a HEX RGB value
                    if (!found) {
                        // Remove the leading '#'
                        bstr_eatstart(&val, bstr0("#"));
                        // Sometimes there are two '#'
                        bstr_eatstart(&val, bstr0("#"));

                        // Parse RRGGBB format
                        tag->color = bstrtoll(val, &val, 16) & 0x00ffffff;
                        if (!val.len) {
                            tag->color = ((tag->color & 0xff) << 16)
                                | (tag->color & 0xff00)
                                | ((tag->color & 0xff0000) >> 16);
                            found = 1;
                        }
                    }

                    if (found) {
                        append_text(&new_line, "{\\c&H%06X&}", tag->color);
                        tag->has_color = true;
                    } else {
                        // We didn't find any matching color
                        MP_WARN(sd, "unknown font color in subtitle: >%s<\n",
                                orig);
                        append_text(&new_line, "{\\c}");
                    }

                    has_valid_attr = true;
                } else if (!bstrcmp0(attr, "face")) {
                    /* Font face attribute */
                    tag->face = val;
                    append_text(&new_line, "{\\fn%.*s}", BSTR_P(tag->face));
                    tag->has_face = true;
                    has_valid_attr = true;
                } else
                    MP_WARN(sd, "unrecognized attribute \"%.*s\" in font tag\n",
                            BSTR_P(attr));
            }

            if (!has_valid_attr || *line != '>') { /* Not valid font tag */
                line = potential_font_tag_start;
                new_line.len = len_backup;
            } else {
                sp++;
                line++;
            }
        } else if (*line == '{') {
            char *end = strchr(line, '}');
            if (line[1] == '\\' && end) {
                // Likely ASS tag, pass them through
                // Note that ASS tags like {something\an8} are legal too (i.e.
                // the first character after '{' doesn't have to be '\'), but
                // consider these fringe cases not worth supporting.
                append_text_n(&new_line, line, end - line + 1);
                line = end + 1;
            } else {
                append_text(&new_line, "\\{");
                line++;
            }
        }

        /* Tag conversion code didn't match */
        if (line == orig_line)
            new_line.buf[new_line.len++] = *line++;
    }
    new_line.buf[new_line.len] = 0;
}

static const char *const srt_ass_extradata =
    "[Script Info]\n"
    "ScriptType: v4.00+\n"
    "PlayResX: 384\n"
    "PlayResY: 288\n";

static bool supports_format(const char *format)
{
    return format && (strcmp(format, "subrip") == 0 ||
                      strcmp(format, "text") == 0);
}

static int init(struct sd *sd)
{
    sd->output_codec = "ass-text";
    sd->output_extradata = (char *)srt_ass_extradata;
    sd->output_extradata_len = strlen(sd->output_extradata);
    return 0;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    char dest[SD_MAX_LINE_LEN];
    // Assume input buffer is padded with 0
    convert_subrip(sd, packet->buffer, dest, sizeof(dest));
    sd_conv_add_packet(sd, dest, strlen(dest), packet->pts, packet->duration);
}

const struct sd_functions sd_srt = {
    .name = "srt",
    .supports_format = supports_format,
    .init = init,
    .decode = decode,
    .get_converted = sd_conv_def_get_converted,
    .reset = sd_conv_def_reset,
};
