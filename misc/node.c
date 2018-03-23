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
