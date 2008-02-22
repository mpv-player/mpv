#ifndef MPLAYER_MMAP_ANON_H
#define MPLAYER_MMAP_ANON_H

#include <sys/types.h>

void *mmap_anon(void *, size_t, int, int, off_t);

#endif /* MPLAYER_MMAP_ANON_H */
