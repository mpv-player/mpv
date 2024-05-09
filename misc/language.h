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

#include "misc/bstr.h"

// Result numerically higher => better match. 0 == no match.
int mp_match_lang(char **langs, const char *lang);
char **mp_get_user_langs(void);
bstr mp_guess_lang_from_filename(bstr name, int *lang_start);

#endif /* MP_LANGUAGE_H */
