/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/
#include <errno.h>
#ifdef __i386__
#include <sys/perm.h>
#else
#include <sys/io.h>
#endif

static __inline__ int enable_os_io(void)
{
    if (iopl(3) != 0)
	return(errno);
    return(0);
}

static __inline__ int disable_os_io(void)
{
    if (iopl(0) != 0)
	return(errno);
    return(0);
}
