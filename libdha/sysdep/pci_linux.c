/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/
#include <errno.h>
#ifdef __i386__
#include <sys/perm.h>
#else
#ifndef __sparc__
#include <sys/io.h>
#endif
#endif

#include "config.h"

#ifdef CONFIG_DHAHELPER
#include <fcntl.h>
int dhahelper_initialized = 0;
int dhahelper_fd = 0;
#endif

static __inline__ int enable_os_io(void)
{
#ifdef CONFIG_DHAHELPER
    dhahelper_fd = open("/dev/dhahelper", O_RDWR);
    if (dhahelper_fd > 0)
    {
	dhahelper_initialized = 1;
	return(0);
    }
    dhahelper_initialized = -1;
#endif

    if (iopl(3) != 0)
	return(errno);
    return(0);
}

static __inline__ int disable_os_io(void)
{
#ifdef CONFIG_DHAHELPER
    if (dhahelper_initialized == 1)
	close(dhahelper_fd);
    else
#endif
    if (iopl(0) != 0)
	return(errno);
    return(0);
}
