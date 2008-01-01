#ifndef MMAP_ANON_H
#define MMAP_ANON_H

#include <sys/types.h>

void *mmap_anon(void *, size_t, int, int, off_t);

#endif /* MMAP_ANON_H */
