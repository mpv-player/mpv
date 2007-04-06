/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/tss.h>
#if defined(NCR)
#define __STDC
#include <sys/sysi86.h>
#undef __STDC
#else
#include <sys/sysi86.h>
#endif

#if defined(sun)
# ifndef __EXTENSIONS__
# define __EXTENSIONS__
# endif
# include <sys/psw.h>
#endif

static __inline__ int enable_os_io(void)
{
#if defined(SI86IOPL)
    sysi86(SI86IOPL, 3);
#else
    sysi86(SI86V86, V86SC_IOPL, PS_IOPL);
#endif
    return(0);
}

static __inline__ int disable_os_io(void)
{
#if defined(SI86IOPL)
    sysi86(SI86IOPL, 0);
#else
    sysi86(SI86V86, V86SC_IOPL, 0);
#endif
    return(0);
}
