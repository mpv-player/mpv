#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
 
#include "dhahelper.h"

int main(int argc, char *argv[])
{
    int fd;
    int ret;

    fd = open("/dev/dhahelper", O_RDWR);

    ioctl(fd, DHAHELPER_GET_VERSION, &ret);

    printf("api version: %d\n", ret);
    if (ret != API_VERSION)
	printf("incompatible api!\n");

    {
 	dhahelper_memory_t mem;

	mem.operation = MEMORY_OP_MAP;
	//mem.start = 0xe0000000;
	mem.start = 0xe4000008;
 	mem.offset = 0;
 	mem.size = 0x4000;
	mem.ret = 0;

	ret = ioctl(fd, DHAHELPER_MEMORY, &mem);

	printf("ret: %s\n", strerror(errno));

	mem.ret = (int)mmap(NULL, (size_t)mem.size, PROT_READ, MAP_SHARED, fd, (off_t)0);
	printf("allocated to %x\n", mem.ret); 

	if (argc > 1)
	    if (mem.ret != 0)
	    {
 		int i;
 
		for (i = 0; i < 256; i++)
		    printf("[%x] ", *(int *)(mem.ret+i));
		printf("\n");
	    }

	munmap((void *)mem.ret, mem.size);

	mem.operation = MEMORY_OP_UNMAP;
	mem.start = mem.ret;

	ioctl(fd, DHAHELPER_MEMORY, &mem);
    }

    return(0);
}
