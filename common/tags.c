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

#include <libavutil/dict.h>
#include "tags.h"
#include "bstr/bstr.h"

void mp_tags_set_str(struct mp_tags *tags, const char *key, const char *value)
{
    mp_tags_set_bstr(tags, bstr0(key), bstr0(value));
}

void mp_tags_set_bstr(struct mp_tags *tags, bstr key, bstr value)
{
    for (int n = 0; n < tags->num_keys; n++) {
        if (bstrcasecmp0(key, tags->keys[n]) == 0) {
            talloc_free(tags->values[n]);
            tags->values[n] = talloc_strndup(tags, value.start, value.len);
            return;
        }
    }

    MP_RESIZE_ARRAY(tags, tags->keys,   tags->num_keys + 1);
    MP_RESIZE_ARRAY(tags, tags->values, tags->num_keys + 1);
    tags->keys[tags->num_keys]   = talloc_strndup(tags, key.start,   key.len);
    tags->values[tags->num_keys] = talloc_strndup(tags, value.start, value.len);
    tags->num_keys++;
}

char *mp_tags_get_str(struct mp_tags *tags, const char *key)
{
    return mp_tags_get_bstr(tags, bstr0(key));
}

char *mp_tags_get_bstr(struct mp_tags *tags, bstr key)
{
    for (int n = 0; n < tags->num_keys; n++) {
        if (bstrcasecmp0(key, tags->keys[n]) == 0)
            return tags->values[n];
    }
    return NULL;
}

void mp_tags_clear(struct mp_tags *tags)
{
    *tags = (struct mp_tags){0};
    talloc_free_children(tags);
}

void mp_tags_merge(struct mp_tags *tags, struct mp_tags *src)
{
    for (int n = 0; n < src->num_keys; n++)
        mp_tags_set_str(tags, src->keys[n], src->values[n]);
}

void mp_tags_copy_from_av_dictionary(struct mp_tags *tags,
                                     struct AVDictionary *av_dict)
{
    AVDictionaryEntry *entry = NULL;
    while ((entry = av_dict_get(av_dict, "", entry, AV_DICT_IGNORE_SUFFIX)))
        mp_tags_set_str(tags, entry->key, entry->value);
}
