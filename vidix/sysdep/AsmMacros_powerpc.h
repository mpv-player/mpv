/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/util/AsmMacros.h,v 1.1 2001/11/16 21:13:34 tsi Exp $
   Modified for readability by Nick Kurshev
*/

#ifndef __ASM_MACROS_POWERPC_H
#define __ASM_MACROS_POWERPC_H

#if defined(Lynx) || defined(__OpenBSD__)

extern unsigned char *ioBase;

static __inline__ volatile void eieio()
{
	__asm__ __volatile__ ("eieio");
}

static __inline__ void outb(short port, unsigned char value)
{
	*(unsigned char *)(ioBase + port) = value; eieio();
}

static __inline__ void outw(short port, unsigned short value)
{
	*(unsigned short *)(ioBase + port) = value; eieio();
}

static __inline__ void outl(short port, unsigned short value)
{
	*(unsigned long *)(ioBase + port) = value; eieio();
}

static __inline__ unsigned char inb(short port)
{
	unsigned char val;
	val = *((unsigned char *)(ioBase + port)); eieio();
	return(val);
}

static __inline__ unsigned short inw(short port)
{
	unsigned short val;
	val = *((unsigned short *)(ioBase + port)); eieio();
	return(val);
}

static __inline__ unsigned long inl(short port)
{
	unsigned long val;
	val = *((unsigned long *)(ioBase + port)); eieio();
	return(val);
}

#define intr_disable()
#define intr_enable()

#endif

#endif
