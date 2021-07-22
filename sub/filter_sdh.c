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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>

#include "misc/ctype.h"
#include "common/common.h"
#include "common/msg.h"
#include "options/options.h"
#include "sd.h"

// Filter for removing subtitle additions for deaf or hard-of-hearing (SDH)
// This is for English, but may in part work for others too.
// The intention is that it can always be active so may not remove
// all SDH parts.
// It is for filtering ASS encoded subtitles

struct buffer {
    char *string;
    int length;
    int pos;
};

static void init_buf(struct buffer *buf, int length)
{
    buf->string = talloc_size(NULL, length);
    buf->pos = 0;
    buf->length = length;
}

static inline int append(struct sd_filter *sd, struct buffer *buf, char c)
{
    if (buf->pos >= 0 && buf->pos < buf->length) {
        buf->string[buf->pos++] = c;
    } else {
        // ensure that terminating \0 is always written
        if (c == '\0')
            buf->string[buf->length - 1] = c;
    }
    return c;
}


// copy ass override tags, if they exist att current position,
// from source string to destination buffer stopping at first
// character following last sequence of '{text}'
//
// Parameters:
//     rpp       read pointer pointer to source string, updated on return
//     buf       write buffer
//
// on return the read pointer is updated to the position after
// the tags.
static void copy_ass(struct sd_filter *sd, char **rpp, struct buffer *buf)
{
    char *rp = *rpp;

    while (rp[0] == '{') {
        while (*rp) {
            char tmp = append(sd, buf, rp[0]);
            rp++;
            if (tmp == '}')
                break;
        }
    }
    *rpp = rp;

    return;
}

static bool skip_bracketed(struct sd_filter *sd, char **rpp, struct buffer *buf);

// check for speaker label, like MAN:
// normal subtitles may include mixed case text with : after so
// only upper case is accepted and lower case l which for some
// looks like upper case I unless filter_harder - then
// lower case is also acceptable
//
// Parameters:
//     rpp       read pointer pointer to source string, updated on return
//     buf       write buffer
//
// scan in source string and copy ass tags to destination string
// skipping speaker label if it exists
//
// if no label was found read pointer and write position in buffer
// will be unchanged
// otherwise they point to next position after label and next write position
static void skip_speaker_label(struct sd_filter *sd, char **rpp, struct buffer *buf)
{
    int filter_harder = sd->opts->sub_filter_SDH_harder;
    char *rp = *rpp;
    int old_pos = buf->pos;

    copy_ass(sd, &rp, buf);
    // copy any leading "- "
    if (rp[0] == '-') {
        append(sd, buf, rp[0]);
        rp++;
    }
    copy_ass(sd, &rp, buf);
    while (rp[0] == ' ') {
        append(sd, buf, rp[0]);
        rp++;
        copy_ass(sd, &rp, buf);
    }
    // skip past valid data searching for :
    while (*rp && rp[0] != ':') {
        if (rp[0] == '{') {
            copy_ass(sd, &rp, buf);
        } else if (rp[0] == '[') {
            // not uncommon with [xxxx]: which should also be skipped
            if (!skip_bracketed(sd, &rp, buf)) {
                buf->pos = old_pos;
                return;
            }
        } else if ((mp_isalpha(rp[0]) &&
                    (filter_harder || mp_isupper(rp[0]) || rp[0] == 'l')) ||
                   mp_isdigit(rp[0]) ||
                   rp[0] == ' ' || rp[0] == '\'' ||
                   (filter_harder && (rp[0] == '(' || rp[0] == ')')) ||
                   rp[0] == '#' || rp[0] == '.' || rp[0] == ',') {
            rp++;
        } else {
            buf->pos = old_pos;
            return;
         }
    }
    if (!*rp) {
        // : was not found
        buf->pos = old_pos;
        return;
    }
    rp++; // skip :
    copy_ass(sd, &rp, buf);
    if (!*rp) {
        // end of data
    } else if (rp[0] == '\\' && rp[1] == 'N') {
        // line end follows - skip it as line is empty
        rp += 2;
    } else if (rp[0] == ' ') {
        while (rp[0] == ' ') {
            rp++;
        }
        if (rp[0] == '\\' && rp[1] == 'N') {
            // line end follows - skip it as line is empty
            rp += 2;
        }
    } else {
        // non space follows - no speaker label
        buf->pos = old_pos;
        return;
    }
    *rpp = rp;

    return;
}

// check for bracketed text, like [SOUND]
// and skip it while preserving ass tags
// any characters are allowed, brackets are seldom used in normal text
//
// Parameters:
//     rpp       read pointer pointer to source string, updated on return
//     buf       write buffer
//
// scan in source string
// the first character in source string must by the starting '['
// and copy ass tags to destination string but
// skipping bracketed text if it looks like SDH
//
// return true if bracketed text was removed.
// if not valid SDH read pointer and write buffer position will be unchanged
// otherwise they point to next position after text and next write position
static bool skip_bracketed(struct sd_filter *sd, char **rpp, struct buffer *buf)
{
    char *rp = *rpp;
    int old_pos = buf->pos;

    rp++; // skip past '['
    // skip past valid data searching for ]
    while (*rp && rp[0] != ']') {
        if (rp[0] == '{') {
            copy_ass(sd, &rp, buf);
        } else {
            rp++;
        }
    }
    if (!*rp) {
        // ] was not found
        buf->pos = old_pos;
        return false;
    }
    rp++; // skip ]
    // skip trailing spaces
    while (rp[0] == ' ') {
        rp++;
    }
    *rpp = rp;

    return true;
}

// check for paranthesed text, like (SOUND)
// and skip it while preserving ass tags
// normal subtitles may include mixed case text in parentheses so
// only upper case is accepted and lower case l which for some
// looks like upper case I but if requested harder filtering
// both upper and lower case is accepted
//
// Parameters:
//     rpp       read pointer pointer to source string, updated on return
//     buf       write buffer
//
// scan in source string
// the first character in source string must be the starting '('
// and copy ass tags to destination string but
// skipping paranthesed text if it looks like SDH
//
// return true if paranthesed text was removed.
// if not valid SDH read pointer and write buffer position will be unchanged
// otherwise they point to next position after text and next write position
static bool skip_parenthesed(struct sd_filter *sd, char **rpp, struct buffer *buf)
{
    int filter_harder = sd->opts->sub_filter_SDH_harder;
    char *rp = *rpp;
    int old_pos = buf->pos;

    rp++; // skip past '('
    // skip past valid data searching for )
    bool only_digits = true;
    while (*rp && rp[0] != ')') {
        if (rp[0] == '{') {
            copy_ass(sd, &rp, buf);
        } else if ((mp_isalpha(rp[0]) &&
                    (filter_harder || mp_isupper(rp[0]) || rp[0] == 'l')) ||
                   mp_isdigit(rp[0]) ||
                   rp[0] == ' ' || rp[0] == '\'' || rp[0] == '#' ||
                   rp[0] == '.' || rp[0] == ',' ||
                   rp[0] == '-' || rp[0] == '"' || rp[0] == '\\') {
            if (!mp_isdigit(rp[0]))
                only_digits = false;
            rp++;
        } else {
            buf->pos = old_pos;
            return false;
        }
    }
    if (!*rp) {
        // ) was not found
        buf->pos = old_pos;
        return false;
    }
    if (only_digits) {
        // number within parentheses is probably not SDH
        buf->pos = old_pos;
        return false;
    }
    rp++; // skip )
    // skip trailing spaces
    while (rp[0] == ' ') {
        rp++;
    }
    *rpp = rp;

    return true;
}

// remove leading hyphen and following spaces in write buffer
//
// Parameters:
//     start_pos start position i buffer
//     buf       buffer to remove in
//
// when removing characters the following are moved back
//
static void remove_leading_hyphen_space(struct sd_filter *sd, int start_pos,
                                        struct buffer *buf)
{
    int old_pos = buf->pos;
    if (start_pos < 0 || start_pos >= old_pos)
        return; 
    append(sd, buf, '\0');  // \0 terminate for reading

    // move past leading ass tags
    while (buf->string[start_pos] == '{') {
        while (buf->string[start_pos] && buf->string[start_pos] != '}') {
            start_pos++;
        }
        if (buf->string[start_pos])
            start_pos++; // skip past '}'
    }

    // if there is not a leading '-' no removing will be done
    if (buf->string[start_pos] != '-') {
        buf->pos = old_pos;
        return;
    }

    char *rp = &buf->string[start_pos];  // read from here
    buf->pos = start_pos; // start writing here
    rp++; // skip '-'
    copy_ass(sd, &rp, buf);
    while (rp[0] == ' ') {
        rp++; // skip ' '
        copy_ass(sd, &rp, buf);
    }
    while (*rp) {
        // copy the rest
        append(sd, buf, rp[0]);
        rp++;
    }
}

// Filter ASS formatted string for SDH
//
// Parameters:
//     data         ASS line
//     length       length of ASS line
//     toff         Text offset from data. required: 0 <= toff <= length
//
// Returns  a talloc allocated string with filtered ASS data (may be the same
// content as original if no SDH was found) which must be released
// by caller using talloc_free.
//
// Returns NULL if filtering resulted in all of ASS data being removed so no
// subtitle should be output
static char *filter_SDH(struct sd_filter *sd, char *data, int length, ptrdiff_t toff)
{
    struct buffer writebuf;
    struct buffer *buf = &writebuf;
    init_buf(buf, length + 1); // with room for terminating '\0'

    // pre-text headers into buf, rp is the (null-terminated) remaining text
    char *ass = talloc_strndup(NULL, data, length), *rp = ass;
    while (rp - ass < toff)
        append(sd, buf, *rp++);

    bool contains_text = false;  // true if non SDH text was found
    bool line_with_text = false; // if last line contained text
    int wp_line_start = buf->pos; // write pos to start of last line
    int wp_line_end   = buf->pos; // write pos to end of previous line with text (\N)

    // go through the lines in the text
    // they are separated by \N
    while (*rp) {
        line_with_text = false;
        wp_line_start = buf->pos;

        // skip any speaker label
        skip_speaker_label(sd, &rp, buf);

        // go through the rest of the line looking for SDH in () or []
        while (*rp && !(rp[0] == '\\' && rp[1] == 'N')) {
            copy_ass(sd, &rp, buf);
            if (rp[0] == '[') {
                if (!skip_bracketed(sd, &rp, buf)) {
                    append(sd, buf, rp[0]);
                    rp++;
                    line_with_text =  true;
                }
            } else if (rp[0] == '(') {
                if (!skip_parenthesed(sd, &rp, buf)) {
                    append(sd, buf, rp[0]);
                    rp++;
                    line_with_text =  true;
                }
            } else if (*rp && rp[0] != '\\') {
                if ((rp[0] > 32 && rp[0] < 127 && rp[0] != '-') ||
                    (unsigned char)rp[0] >= 0xC0)
                {
                    line_with_text =  true;
                }
                append(sd, buf, rp[0]);
                rp++;
            } else if (rp[0] == '\\' && rp[1] != 'N') {
                append(sd, buf, rp[0]);
                rp++;
            }
        }
        // either end of data or ASS line end defined by separating \N
        if (*rp) {
            // ASS line end
            if (line_with_text) {
                contains_text = true;
                wp_line_end = buf->pos;
                append(sd, buf, rp[0]); // copy backslash
                append(sd, buf, rp[1]); // copy N
                rp += 2; // move read pointer past \N
            } else {
                // no text in line, remove leading hyphen and spaces
                remove_leading_hyphen_space(sd, wp_line_start, buf);
                // and join with next line
                rp += 2; // move read pointer past \N
            }
        }
    }
    // if no normal text i last line - remove last line
    // by moving write pointer to start of last line
    if (!line_with_text) {
        buf->pos = wp_line_end;
    } else {
        contains_text = true;
    }
    talloc_free(ass);

    if (contains_text) {
        // the ASS data contained normal text after filtering
        append(sd, buf, '\0'); // '\0' terminate
        return buf->string;
    } else {
        // all data removed by filtering
        talloc_free(buf->string);
        return NULL;
    }
}

static bool sdh_init(struct sd_filter *ft)
{
    if (strcmp(ft->codec, "ass") != 0)
        return false;

    if (!ft->opts->sub_filter_SDH)
        return false;

    if (!ft->event_format) {
        MP_VERBOSE(ft, "SDH filtering not possible - format missing\n");
        return false;
    }

    return true;
}

static struct demux_packet *sdh_filter(struct sd_filter *ft,
                                       struct demux_packet *pkt)
{
    bstr text = sd_ass_pkt_text(ft, pkt, sd_ass_fmt_offset(ft->event_format));
    if (!text.start || !text.len || pkt->len >= INT_MAX)
        return pkt;  // we don't touch it

    ptrdiff_t toff = text.start - pkt->buffer;
    char *line = filter_SDH(ft, (char *)pkt->buffer, (int)pkt->len, toff);
    if (!line)
        return NULL;
    if (0 == bstrcmp0((bstr){(char *)pkt->buffer, pkt->len}, line)) {
        talloc_free(line);
        return pkt;  // unmodified, no need to allocate new packet
    }

    // Stupidly, this copies it again. One could possibly allocate the packet
    // for writing in the first place (new_demux_packet()) and use
    // demux_packet_shorten().
    struct demux_packet *npkt = new_demux_packet_from(line, strlen(line));
    if (npkt)
        demux_packet_copy_attribs(npkt, pkt);

    talloc_free(line);
    return npkt;
}

const struct sd_filter_functions sd_filter_sdh = {
    .init   = sdh_init,
    .filter = sdh_filter,
};
