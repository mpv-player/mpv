#ifndef MP_TAGS_H
#define MP_TAGS_H

#include "bstr/bstr.h"

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
void mp_tags_merge(struct mp_tags *tags, struct mp_tags *src);
struct AVDictionary;
void mp_tags_copy_from_av_dictionary(struct mp_tags *tags,
                                     struct AVDictionary *av_dict);

#endif
