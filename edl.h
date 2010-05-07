/*
 * EDL version 0.6
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

#ifndef MPLAYER_EDL_H
#define MPLAYER_EDL_H

#define EDL_SKIP 0
#define EDL_MUTE 1

#define EDL_MUTE_START 1
#define EDL_MUTE_END 0

struct edl_record {
    float start_sec;
    float stop_sec;
    float length_sec;
    short action;
    struct edl_record* next;
    struct edl_record* prev;
};

typedef struct edl_record* edl_record_ptr;

extern char *edl_filename; // file to extract EDL entries from (-edl)
extern char *edl_output_filename; // file to put EDL entries in (-edlout)

void free_edl(edl_record_ptr next_edl_record); // free's entire EDL list.
edl_record_ptr edl_parse_file(void); // fills EDL stack

#endif /* MPLAYER_EDL_H */
