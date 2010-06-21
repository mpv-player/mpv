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

#ifndef MPLAYER_CDD_H
#define MPLAYER_CDD_H

#include <sys/types.h>

typedef struct cd_track {
	char *name;
	unsigned int track_nb;
	unsigned int min;
	unsigned int sec;
	unsigned int msec;
	unsigned long frame_begin;
	unsigned long frame_length;
	struct cd_track *prev;
	struct cd_track *next;
} cd_track_t;

typedef struct {
	char *artist;
	char *album;
	char *genre;
	unsigned int nb_tracks;
	unsigned int min;
	unsigned int sec;
	unsigned msec;
	cd_track_t *first;
	cd_track_t *last;
	cd_track_t *current;
} cd_info_t;

cd_info_t* 	cd_info_new(void);
void		cd_info_free(cd_info_t *cd_info);
cd_track_t*	cd_info_add_track(cd_info_t *cd_info, char *track_name, unsigned int track_nb, unsigned int min, unsigned int sec, unsigned int msec, unsigned long frame_begin, unsigned long frame_length);
cd_track_t*	cd_info_get_track(cd_info_t *cd_info, unsigned int track_nb);

void 		cd_info_debug(cd_info_t *cd_info);

int             cdd_identify(const char *dev);
int             cddb_resolve(const char *dev, char **xmcd_file);
cd_info_t*      cddb_parse_xmcd(char *xmcd_file);

#endif /* MPLAYER_CDD_H */
