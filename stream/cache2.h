#ifndef MPLAYER_CACHE2_H
#define MPLAYER_CACHE2_H

#include "stream.h"

void cache_uninit(stream_t *s);
int cache_do_control(stream_t *stream, int cmd, void *arg);

#endif /* MPLAYER_CACHE2_H */
