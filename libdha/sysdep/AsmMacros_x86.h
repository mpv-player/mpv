/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/util/AsmMacros.h,v 1.1 2001/11/16 21:13:34 tsi Exp $
   Modified for readability by Nick Kurshev
*/

#ifndef __ASM_MACROS_X86_H
#define __ASM_MACROS_X86_H

#if defined (WINNT)
#error This stuff is not ported on your system
#else

static __inline__ void outb(short port,char val)
{
   __asm__ __volatile__("outb %0,%1" : :"a" (val), "d" (port));
}

static __inline__ void outw(short port,short val)
{
   __asm__ __volatile__("outw %0,%1" : :"a" (val), "d" (port));
}

static __inline__ void outl(short port,unsigned int val)
{
   __asm__ __volatile__("outl %0,%1" : :"a" (val), "d" (port));
}

static __inline__ unsigned int inb(short port)
{
   unsigned char ret;
   __asm__ __volatile__("inb %1,%0" :
       "=a" (ret) :
       "d" (port));
   return ret;
}

static __inline__ unsigned int inw(short port)
{
   unsigned short ret;
   __asm__ __volatile__("inw %1,%0" :
       "=a" (ret) :
       "d" (port));
   return ret;
}

static __inline__ unsigned int inl(short port)
{
   unsigned int ret;
   __asm__ __volatile__("inl %1,%0" :
       "=a" (ret) :
       "d" (port));
   return ret;
}

static __inline__ void intr_disable()
{
  __asm__ __volatile__("cli");
}

static __inline__ void intr_enable()
{
  __asm__ __volatile__("sti");
}

#endif

#endif
