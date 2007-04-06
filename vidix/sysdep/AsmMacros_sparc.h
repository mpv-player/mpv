/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/util/AsmMacros.h,v 1.1 2001/11/16 21:13:34 tsi Exp $
   Modified for readability by Nick Kurshev
*/

#ifndef __ASM_MACROS_SPARC_H
#define __ASM_MACROS_SPARC_H

#ifndef ASI_PL
#define ASI_PL 0x88
#endif

static __inline__ void outb(unsigned long port, char val)
{
  __asm__ __volatile__("stba %0, [%1] %2" : : "r" (val), "r" (port), "i" (ASI_PL));
}

static __inline__ void outw(unsigned long port, char val)
{
  __asm__ __volatile__("stha %0, [%1] %2" : : "r" (val), "r" (port), "i" (ASI_PL));
}

static __inline__ void outl(unsigned long port, char val)
{
  __asm__ __volatile__("sta %0, [%1] %2" : : "r" (val), "r" (port), "i" (ASI_PL));
}

static __inline__ unsigned int inb(unsigned long port)
{
   unsigned char ret;
   __asm__ __volatile__("lduba [%1] %2, %0" : "=r" (ret) : "r" (port), "i" (ASI_PL));
   return ret;
}

static __inline__ unsigned int inw(unsigned long port)
{
   unsigned char ret;
   __asm__ __volatile__("lduha [%1] %2, %0" : "=r" (ret) : "r" (port), "i" (ASI_PL));
   return ret;
}

static __inline__ unsigned int inl(unsigned long port)
{
   unsigned char ret;
   __asm__ __volatile__("lda [%1] %2, %0" : "=r" (ret) : "r" (port), "i" (ASI_PL));
   return ret;
}

#define intr_disable()
#define intr_enable()

#endif
