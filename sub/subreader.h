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

#ifndef MPLAYER_SUBREADER_H
#define MPLAYER_SUBREADER_H

#include <stdio.h>
#include <stdbool.h>

#include "config.h"

extern int suboverlap_enabled;
extern int sub_no_text_pp;  // disable text post-processing
extern int sub_match_fuzziness;

// subtitle formats
#define SUB_INVALID   -1
#define SUB_MICRODVD  0
#define SUB_SUBRIP    1
#define SUB_SUBVIEWER 2
#define SUB_SAMI      3
#define SUB_VPLAYER   4
#define SUB_RT        5
#define SUB_SSA       6
#define SUB_PJS       7
#define SUB_MPSUB     8
#define SUB_AQTITLE   9
#define SUB_SUBVIEWER2 10
#define SUB_SUBRIP09 11
#define SUB_JACOSUB  12
#define SUB_MPL2     13

// One of the SUB_* constant above
extern int sub_format;

#define SUB_MAX_TEXT 12
#define SUB_ALIGNMENT_BOTTOMLEFT       1
#define SUB_ALIGNMENT_BOTTOMCENTER     2
#define SUB_ALIGNMENT_BOTTOMRIGHT      3
#define SUB_ALIGNMENT_MIDDLELEFT       4
#define SUB_ALIGNMENT_MIDDLECENTER     5
#define SUB_ALIGNMENT_MIDDLERIGHT      6
#define SUB_ALIGNMENT_TOPLEFT          7
#define SUB_ALIGNMENT_TOPCENTER        8
#define SUB_ALIGNMENT_TOPRIGHT         9

typedef struct subtitle {

    int lines;

    unsigned long start;
    unsigned long end;

    char *text[SUB_MAX_TEXT];
    unsigned char alignment;
} subtitle;

typedef struct sub_data {
    const char *codec;
    subtitle *subtitles;
    char *filename;
    int sub_uses_time;
    int sub_num;          // number of subtitle structs
    int sub_errs;
} sub_data;

struct MPOpts;
sub_data* sub_read_file (char *filename, float pts, struct MPOpts *opts);
subtitle* subcp_recode (subtitle *sub);
// enca_fd is the file enca uses to determine the codepage.
// setting to NULL disables enca.
struct stream;
void subcp_open (struct stream *st); /* for demux_ogg.c */
void subcp_close (void); /* for demux_ogg.c */
#ifdef CONFIG_ENCA
const char* guess_buffer_cp(unsigned char* buffer, int buflen, const char *preferred_language, const char *fallback);
const char* guess_cp(struct stream *st, const char *preferred_language, const char *fallback);
#endif

#endif /* MPLAYER_SUBREADER_H */
