/*
 *   shmem.c - Shared memory allocation
 *   
 *   based on mpg123's xfermem.c by
 *   Oliver Fromme  <oliver.fromme@heim3.tu-clausthal.de>
 *   Sun Apr  6 02:26:26 MET DST 1997
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <fcntl.h>

#ifdef AIX
#include <sys/select.h>
#endif

#include <sys/ipc.h>
#include <sys/shm.h>

extern int errno;

#if defined(MAP_ANONYMOUS) && !defined(MAP_ANON)
#define MAP_ANON MAP_ANONYMOUS
#endif

static int shmem_type=0;

void* shmem_alloc(int size){
void* p;
int devzero;
while(1){
  switch(shmem_type){
  case 0:  // ========= MAP_ANON|MAP_SHARED ==========
    p=mmap(0,size,PROT_READ|PROT_WRITE,MAP_ANON|MAP_SHARED,-1,0);
    if(p==MAP_FAILED) break; // failed
//    printf("shmem: %d bytes allocated using mmap anon (%X)\n",size,p);
    return p;
  case 1:  // ========= MAP_SHARED + /dev/zero ==========
	  if ((devzero = open("/dev/zero", O_RDWR, 0)) == -1) break;
    p=mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,devzero,0);
    if(p==MAP_FAILED) break; // failed
//    printf("shmem: %d bytes allocated using mmap /dev/zero (%X)\n",size,p);
    return p;
  case 2: { // ========= shmget() ==========
    struct shmid_ds shmemds;
    int shmemid;
    if ((shmemid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600)) == -1) break;
    if ((int)(p = shmat(shmemid, 0, 0)) == -1){
      perror ("shmat()");
      shmctl (shmemid, IPC_RMID, &shmemds);
      break;
    }
    if (shmctl(shmemid, IPC_RMID, &shmemds) == -1) {
      perror ("shmctl()");
      if (shmdt(p) == -1) perror ("shmdt()");
      break;
    }
//    printf("shmem: %d bytes allocated using shmget() & shmat() (%X)\n",size,p);
    return p;
	}
  default:
    printf("FATAL: Cannot alloate %d bytes shared memory :(\n",size);
    return NULL;
  }
  ++shmem_type;
}
}

void shmem_free(void* p){
  switch(shmem_type){
    case 2:
	    if (shmdt(p) == -1) perror ("shmdt()");
      break;
  }
}
