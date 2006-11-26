#ifndef _OSDEP_MMAP_ANON_H_
#define _OSDEP_MMAP_ANON_H_

#include <sys/types.h>

void *mmap_anon(void *, size_t, int, int, off_t);

#endif /* _OSDEP_MMAP_ANON_H_ */
