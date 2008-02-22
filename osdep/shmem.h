#ifndef MPLAYER_SHMEM_H
#define MPLAYER_SHMEM_H

void* shmem_alloc(int size);
void shmem_free(void* p,int size);

#endif /* MPLAYER_SHMEM_H */
