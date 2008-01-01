#ifndef SHMEM_H
#define SHMEM_H

void* shmem_alloc(int size);
void shmem_free(void* p,int size);

#endif /* SHMEM_H */
