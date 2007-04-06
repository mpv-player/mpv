/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/util/AsmMacros.h,v 1.1 2001/11/16 21:13:34 tsi Exp $
   Modified for readability by Nick Kurshev
*/

#ifndef __ASM_MACROS_ALPHA_H
#define __ASM_MACROS_ALPHA_H
#if defined (linux)
#include <sys/io.h>
#elif defined (__FreeBSD__)
#include <sys/types.h>
extern void outb(u_int32_t port, u_int8_t val);
extern void outw(u_int32_t port, u_int16_t val);
extern void outl(u_int32_t port, u_int32_t val);
extern u_int8_t inb(u_int32_t port);
extern u_int16_t inw(u_int32_t port);
extern u_int32_t inl(u_int32_t port);
#else
#error This stuff is not ported on your system
#endif

#define intr_disable()
#define intr_enable()

#endif
