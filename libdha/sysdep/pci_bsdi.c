/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/
#include <sys/file.h>
#include <sys/ioctl.h>
#include <i386/isa/pcconsioctl.h>
#ifndef GCCUSESGAS
#define GCCUSESGAS
#endif

static int io_fd;

static __inline__ void enable_os_io(void)
{
    io_fd = -1 ;
    if ((io_fd = open("/dev/console", O_RDWR, 0)) < 0) {
        perror("/dev/console");
        exit(1);
    }
    if (ioctl(io_fd, PCCONENABIOPL, 0) < 0) {
        perror("ioctl(PCCONENABIOPL)");
        exit(1);
    }
}

static __inline__ void disable_os_io(void)
{
    if (ioctl(io_fd, PCCONDISABIOPL, 0) < 0) {
        perror("ioctl(PCCONDISABIOPL)");
	close(io_fd);
        exit(1);
    }
    close(io_fd);
}
