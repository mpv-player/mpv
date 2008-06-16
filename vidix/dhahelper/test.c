/*
 * dhahelper test program
 *
 * Copyright (C) 2002 Alex Beregszsaszi
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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

    return 0;
}
