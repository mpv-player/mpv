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

#ifndef MPLAYER_STREAM_DVD_H
#define MPLAYER_STREAM_DVD_H

#include "config.h"
#include <stdint.h>
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#include "stream.h"
#include "m_option.h"

typedef struct {
  dvd_reader_t *dvd;
  dvd_file_t *title;
  ifo_handle_t *vmg_file;
  tt_srpt_t *tt_srpt;
  ifo_handle_t *vts_file;
  vts_ptt_srpt_t *vts_ptt_srpt;
  pgc_t *cur_pgc;
//
  int cur_title;
  int cur_cell;
  int last_cell;
  int cur_pack;
  int cell_last_pack;
  int cur_pgc_idx;
// Navi:
  int packs_left;
  dsi_t dsi_pack;
  int angle_seek;
  unsigned int *cell_times_table;
// audio datas
  int nr_of_channels;
  stream_language_t audio_streams[32];
// subtitles
  int nr_of_subtitles;
  stream_language_t subtitles[32];
} dvd_priv_t;

int dvd_number_of_subs(stream_t *stream);
int dvd_lang_from_aid(stream_t *stream, int id);
int dvd_lang_from_sid(stream_t *stream, int id);
int dvd_aid_from_lang(stream_t *stream, unsigned char* lang);
int dvd_sid_from_lang(stream_t *stream, unsigned char* lang);
int dvd_chapter_from_cell(dvd_priv_t *dvd,int title,int cell);
int dvd_parse_chapter_range(const m_option_t *conf, const char *range);

#endif /* MPLAYER_STREAM_DVD_H */
