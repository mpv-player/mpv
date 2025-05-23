#ifndef MP_MISC_NODE_H_
#define MP_MISC_NODE_H_

#include <stdbool.h>
#include <stdint.h>

struct bstr;
struct mpv_node;

void node_init(struct mpv_node *dst, int format, struct mpv_node *parent);
struct mpv_node *node_array_add(struct mpv_node *dst, int format);
struct mpv_node *node_map_add(struct mpv_node *dst, const char *key, int format);
struct mpv_node *node_map_badd(struct mpv_node *dst, struct bstr key, int format);
void node_map_add_string(struct mpv_node *dst, const char *key, const char *val);
void node_map_add_bstr(struct mpv_node *dst, const char *key, struct bstr val);
void node_map_add_int64(struct mpv_node *dst, const char *key, int64_t v);
void node_map_add_double(struct mpv_node *dst, const char *key, double v);
void node_map_add_flag(struct mpv_node *dst, const char *key, bool v);
struct mpv_node *node_map_get(struct mpv_node *src, const char *key);
struct mpv_node *node_map_bget(struct mpv_node *src, struct bstr key);
bool equal_mpv_value(const void *a, const void *b, int format);
bool equal_mpv_node(const struct mpv_node *a, const struct mpv_node *b);

#endif
