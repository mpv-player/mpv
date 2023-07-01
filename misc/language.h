/*
 * Language code utility functions
 *
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

#ifndef MP_LANGUAGE_H
#define MP_LANGUAGE_H

#define LANGUAGE_SCORE_BITS 16
#define LANGUAGE_SCORE_MAX (1 << LANGUAGE_SCORE_BITS)

// Where applicable, l1 is the user-specified code and l2 is the code being checked against it
int mp_match_lang_single(const char *l1, const char *l2);

char **mp_get_user_langs(void);

#endif /* MP_LANGUAGE_H */
