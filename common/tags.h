/*
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

#ifndef MP_TAGS_H
#define MP_TAGS_H

#include "misc/bstr.h"

struct mp_tags {
    char **keys;
    char **values;
    int num_keys;
};

void mp_tags_set_str(struct mp_tags *tags, const char *key, const char *value);
void mp_tags_set_bstr(struct mp_tags *tags, bstr key, bstr value);
char *mp_tags_get_str(struct mp_tags *tags, const char *key);
char *mp_tags_get_bstr(struct mp_tags *tags, bstr key);
void mp_tags_clear(struct mp_tags *tags);
struct mp_tags *mp_tags_dup(void *tparent, struct mp_tags *tags);
struct mp_tags *mp_tags_filtered(void *tparent, struct mp_tags *tags, char **list);
void mp_tags_merge(struct mp_tags *tags, struct mp_tags *src);
struct AVDictionary;
void mp_tags_copy_from_av_dictionary(struct mp_tags *tags,
                                     struct AVDictionary *av_dict);

#endif
