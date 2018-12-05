#include "common/common.h"

#include "node.h"

// Init a node with the given format. If parent is not NULL, it is set as
// parent allocation according to m_option_type_node rules (which means
// the mpv_node_list allocs are used for chaining the TA allocations).
// format == MPV_FORMAT_NONE will simply initialize it with all-0.
void node_init(struct mpv_node *dst, int format, struct mpv_node *parent)
{
    // Other formats need to be initialized manually.
    assert(format == MPV_FORMAT_NODE_MAP || format == MPV_FORMAT_NODE_ARRAY ||
           format == MPV_FORMAT_FLAG || format == MPV_FORMAT_INT64 ||
           format == MPV_FORMAT_DOUBLE || format == MPV_FORMAT_BYTE_ARRAY ||
           format == MPV_FORMAT_NONE);

    void *ta_parent = NULL;
    if (parent) {
        assert(parent->format == MPV_FORMAT_NODE_MAP ||
               parent->format == MPV_FORMAT_NODE_ARRAY);
        ta_parent = parent->u.list;
    }

    *dst = (struct mpv_node){ .format = format };
    if (format == MPV_FORMAT_NODE_MAP || format == MPV_FORMAT_NODE_ARRAY)
        dst->u.list = talloc_zero(ta_parent, struct mpv_node_list);
    if (format == MPV_FORMAT_BYTE_ARRAY)
        dst->u.ba = talloc_zero(ta_parent, struct mpv_byte_array);
}

// Add an entry to a MPV_FORMAT_NODE_ARRAY.
// m_option_type_node memory management rules apply.
struct mpv_node *node_array_add(struct mpv_node *dst, int format)
{
    struct mpv_node_list *list = dst->u.list;
    assert(dst->format == MPV_FORMAT_NODE_ARRAY && dst->u.list);
    MP_TARRAY_GROW(list, list->values, list->num);
    node_init(&list->values[list->num], format, dst);
    return &list->values[list->num++];
}

// Add an entry to a MPV_FORMAT_NODE_MAP. Keep in mind that this does
// not check for already existing entries under the same key.
// m_option_type_node memory management rules apply.
struct mpv_node *node_map_add(struct mpv_node *dst, const char *key, int format)
{
    assert(key);

    struct mpv_node_list *list = dst->u.list;
    assert(dst->format == MPV_FORMAT_NODE_MAP && dst->u.list);
    MP_TARRAY_GROW(list, list->values, list->num);
    MP_TARRAY_GROW(list, list->keys, list->num);
    list->keys[list->num] = talloc_strdup(list, key);
    node_init(&list->values[list->num], format, dst);
    return &list->values[list->num++];
}

// Add a string entry to a MPV_FORMAT_NODE_MAP. Keep in mind that this does
// not check for already existing entries under the same key.
// m_option_type_node memory management rules apply.
void node_map_add_string(struct mpv_node *dst, const char *key, const char *val)
{
    assert(val);

    struct mpv_node *entry = node_map_add(dst, key, MPV_FORMAT_NONE);
    entry->format = MPV_FORMAT_STRING;
    entry->u.string = talloc_strdup(dst->u.list, val);
}

void node_map_add_int64(struct mpv_node *dst, const char *key, int64_t v)
{
    node_map_add(dst, key, MPV_FORMAT_INT64)->u.int64 = v;
}

void node_map_add_double(struct mpv_node *dst, const char *key, double v)
{
    node_map_add(dst, key, MPV_FORMAT_DOUBLE)->u.double_ = v;
}

void node_map_add_flag(struct mpv_node *dst, const char *key, bool v)
{
    node_map_add(dst, key, MPV_FORMAT_FLAG)->u.flag = v;
}

mpv_node *node_map_get(mpv_node *src, const char *key)
{
    if (src->format != MPV_FORMAT_NODE_MAP)
        return NULL;

    for (int i = 0; i < src->u.list->num; i++) {
        if (strcmp(key, src->u.list->keys[i]) == 0)
            return &src->u.list->values[i];
    }

    return NULL;
}

// Note: for MPV_FORMAT_NODE_MAP, this (incorrectly) takes the order into
//       account, instead of treating it as set.
bool equal_mpv_value(const void *a, const void *b, mpv_format format)
{
    switch (format) {
    case MPV_FORMAT_NONE:
        return true;
    case MPV_FORMAT_STRING:
    case MPV_FORMAT_OSD_STRING:
        return strcmp(*(char **)a, *(char **)b) == 0;
    case MPV_FORMAT_FLAG:
        return *(int *)a == *(int *)b;
    case MPV_FORMAT_INT64:
        return *(int64_t *)a == *(int64_t *)b;
    case MPV_FORMAT_DOUBLE:
        return *(double *)a == *(double *)b;
    case MPV_FORMAT_NODE:
        return equal_mpv_node(a, b);
    case MPV_FORMAT_BYTE_ARRAY: {
        const struct mpv_byte_array *a_r = a, *b_r = b;
        if (a_r->size != b_r->size)
            return false;
        return memcmp(a_r->data, b_r->data, a_r->size) == 0;
    }
    case MPV_FORMAT_NODE_ARRAY:
    case MPV_FORMAT_NODE_MAP:
    {
        mpv_node_list *l_a = *(mpv_node_list **)a, *l_b = *(mpv_node_list **)b;
        if (l_a->num != l_b->num)
            return false;
        for (int n = 0; n < l_a->num; n++) {
            if (format == MPV_FORMAT_NODE_MAP) {
                if (strcmp(l_a->keys[n], l_b->keys[n]) != 0)
                    return false;
            }
            if (!equal_mpv_node(&l_a->values[n], &l_b->values[n]))
                return false;
        }
        return true;
    }
    }
    abort(); // supposed to be able to handle all defined types
}

// Remarks see equal_mpv_value().
bool equal_mpv_node(const struct mpv_node *a, const struct mpv_node *b)
{
    if (a->format != b->format)
        return false;
    return equal_mpv_value(&a->u, &b->u, a->format);
}
