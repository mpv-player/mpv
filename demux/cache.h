#pragma once

#include <stdint.h>

struct demux_packet;
struct mp_log;
struct mpv_global;

struct demux_cache;

struct demux_cache *demux_cache_create(struct mpv_global *global,
                                       struct mp_log *log);

int64_t demux_cache_write(struct demux_cache *cache, struct demux_packet *pkt);
struct demux_packet *demux_cache_read(struct demux_cache *cache, uint64_t pos);
uint64_t demux_cache_get_size(struct demux_cache *cache);
