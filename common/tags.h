#ifndef MP_TAGS_H
#define MP_TAGS_H

#include <stdint.h>

#include "misc/bstr.h"

struct mp_tags {
    char **keys;
    char **values;
    int num_keys;
};

void mp_tags_set_str(struct mp_tags *tags, const char *key, const char *value);
void mp_tags_set_bstr(struct mp_tags *tags, bstr key, bstr value);
void mp_tags_remove_str(struct mp_tags *tags, const char *key);
void mp_tags_remove_bstr(struct mp_tags *tags, bstr key);
char *mp_tags_get_str(struct mp_tags *tags, const char *key);
char *mp_tags_get_bstr(struct mp_tags *tags, bstr key);
void mp_tags_clear(struct mp_tags *tags);
struct mp_tags *mp_tags_dup(void *tparent, struct mp_tags *tags);
void mp_tags_replace(struct mp_tags *dst, struct mp_tags *src);
struct mp_tags *mp_tags_filtered(void *tparent, struct mp_tags *tags, char **list);
void mp_tags_merge(struct mp_tags *tags, struct mp_tags *src);
struct AVDictionary;
void mp_tags_copy_from_av_dictionary(struct mp_tags *tags,
                                     struct AVDictionary *av_dict);

#endif
