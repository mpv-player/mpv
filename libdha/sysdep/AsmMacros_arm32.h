/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/util/AsmMacros.h,v 1.1 2001/11/16 21:13:34 tsi Exp $
   Modified for readability by Nick Kurshev
*/

#ifndef __ASM_MACROS_ARM32_H
#define __ASM_MACROS_ARM32_H
unsigned int IOPortBase;  /* Memory mapped I/O port area */

static __inline__ void outb(short port,char val)
{
	 if ((unsigned short)port >= 0x400) return;
	*(volatile unsigned char*)(((unsigned short)(port))+IOPortBase) = val;
}

static __inline__ void outw(short port,short val)
{
	 if ((unsigned short)port >= 0x400) return;
	*(volatile unsigned short*)(((unsigned short)(port))+IOPortBase) = val;
}

static __inline__ void outl(short port,int val)
{
	 if ((unsigned short)port >= 0x400) return;
	*(volatile unsigned long*)(((unsigned short)(port))+IOPortBase) = val;
}

static __inline__ unsigned int inb(short port)
{
	 if ((unsigned short)port >= 0x400) return((unsigned int)-1);
	return(*(volatile unsigned char*)(((unsigned short)(port))+IOPortBase));
}

static __inline__ unsigned int inw(short port)
{
	 if ((unsigned short)port >= 0x400) return((unsigned int)-1);
	return(*(volatile unsigned short*)(((unsigned short)(port))+IOPortBase));
}

static __inline__ unsigned int inl(short port)
{
	 if ((unsigned short)port >= 0x400) return((unsigned int)-1);
	return(*(volatile unsigned long*)(((unsigned short)(port))+IOPortBase));
}

#define intr_disable()
#define intr_enable()

#endif
