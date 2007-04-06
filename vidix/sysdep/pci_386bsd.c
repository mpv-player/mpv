/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/
#include <errno.h>
#include <sys/file.h>
#include <machine/console.h>
#ifndef GCCUSESGAS
#define GCCUSESGAS
#endif

static int io_fd;

static __inline__ int enable_os_io(void)
{
    io_fd = -1 ;
    if ((io_fd = open("/dev/console", O_RDWR, 0)) < 0) {
        perror("/dev/console");
	return(errno);
    }
    if (ioctl(io_fd, KDENABIO, 0) < 0) {
        perror("ioctl(KDENABIO)");
	return(errno);
    }
    return(0);
}

static __inline__ int disable_os_io(void)
{
    if (ioctl(io_fd, KDDISABIO, 0) < 0) {
        perror("ioctl(KDDISABIO)");
	close(io_fd);
	return(errno);
    }
    close(io_fd);
    return(0);
}
