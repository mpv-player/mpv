/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/

static __inline__ void enable_os_io(void)
{
    if (i386_iopl(1) < 0) {
	perror("i386_iopl");
	exit(1);
    }
}

static __inline__ void disable_os_io(void)
{
 /* Nothing to do */
}
