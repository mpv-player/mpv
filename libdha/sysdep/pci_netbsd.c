/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/
#include <errno.h>
#include <sys/param.h>
#include <sys/file.h>
#include <machine/sysarch.h>
#ifndef GCCUSESGAS
#define GCCUSESGAS
#endif

static int io_fd;

static __inline__ int enable_os_io(void)
{
    io_fd = -1 ;
#if !defined(USE_I386_IOPL)
    if ((io_fd = open("/dev/io", O_RDWR, 0)) < 0) {
	perror("/dev/io");
	return(errno);
    }
#else
    if (i386_iopl(1) < 0) {
	perror("i386_iopl");
	return(errno);
    }
#endif /* USE_I386_IOPL */
    return(0);
}

static __inline__ int disable_os_io(void)
{
#if !defined(USE_I386_IOPL)
    close(io_fd);
#else
    if (i386_iopl(0) < 0) {
	perror("i386_iopl");
	return(errno);
    }
#endif /* NetBSD1_1 */
    return(0);
}
