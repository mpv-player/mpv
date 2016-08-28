#ifndef MP_MISC_NODE_H_
#define MP_MISC_NODE_H_

#include "libmpv/client.h"

void node_init(struct mpv_node *dst, int format, struct mpv_node *parent);
struct mpv_node *node_array_add(struct mpv_node *dst, int format);
struct mpv_node *node_map_add(struct mpv_node *dst, const char *key, int format);
void node_map_add_string(struct mpv_node *dst, const char *key, const char *val);

#endif
