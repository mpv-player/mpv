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

#include "config.h"

#ifdef CONFIG_DHAHELPER
#include <sys/ioctl.h>
#include "../kernelhelper/dhahelper.h"

extern int dhahelper_fd;
extern int dhahelper_initialized;
#endif

static __inline__ void outb(short port,char val)
{
#ifdef CONFIG_DHAHELPER
    if (dhahelper_initialized == 1)
    {
	dhahelper_port_t _port;
	
	_port.operation = PORT_OP_WRITE;
	_port.addr = port;
	_port.size = 1;
	_port.value = val;
        if (ioctl(dhahelper_fd, DHAHELPER_PORT, &_port) == 0)
	    return;
    }
    else
#endif
   __asm__ __volatile__("outb %0,%1" : :"a" (val), "d" (port));
    return;
}

static __inline__ void outw(short port,short val)
{
#ifdef CONFIG_DHAHELPER
    if (dhahelper_initialized == 1)
    {
	dhahelper_port_t _port;
	
	_port.operation = PORT_OP_WRITE;
	_port.addr = port;
	_port.size = 2;
	_port.value = val;
        if (ioctl(dhahelper_fd, DHAHELPER_PORT, &_port) == 0)
	    return;
    }
    else
#endif
   __asm__ __volatile__("outw %0,%1" : :"a" (val), "d" (port));
    return;
}

static __inline__ void outl(short port,unsigned int val)
{
#ifdef CONFIG_DHAHELPER
    if (dhahelper_initialized == 1)
    {
	dhahelper_port_t _port;
	
	_port.operation = PORT_OP_WRITE;
	_port.addr = port;
	_port.size = 4;
	_port.value = val;
        if (ioctl(dhahelper_fd, DHAHELPER_PORT, &_port) == 0)
	    return;
    }
    else
#endif
   __asm__ __volatile__("outl %0,%1" : :"a" (val), "d" (port));
    return;
}

static __inline__ unsigned int inb(short port)
{
   unsigned char ret;
#ifdef CONFIG_DHAHELPER
    if (dhahelper_initialized == 1)
    {
	dhahelper_port_t _port;
	
	_port.operation = PORT_OP_READ;
	_port.addr = port;
	_port.size = 1;
        if (ioctl(dhahelper_fd, DHAHELPER_PORT, &_port) == 0)
	    return _port.value;
    }
    else
#endif
   __asm__ __volatile__("inb %1,%0" :
       "=a" (ret) :
       "d" (port));
   return ret;
}

static __inline__ unsigned int inw(short port)
{
   unsigned short ret;
#ifdef CONFIG_DHAHELPER
    if (dhahelper_initialized == 1)
    {
	dhahelper_port_t _port;
	
	_port.operation = PORT_OP_READ;
	_port.addr = port;
	_port.size = 2;
        if (ioctl(dhahelper_fd, DHAHELPER_PORT, &_port) == 0)
	    return _port.value;
    }
    else
#endif
   __asm__ __volatile__("inw %1,%0" :
       "=a" (ret) :
       "d" (port));
   return ret;
}

static __inline__ unsigned int inl(short port)
{
   unsigned int ret;
#ifdef CONFIG_DHAHELPER
    if (dhahelper_initialized == 1)
    {
	dhahelper_port_t _port;
	
	_port.operation = PORT_OP_READ;
	_port.addr = port;
	_port.size = 4;
        if (ioctl(dhahelper_fd, DHAHELPER_PORT, &_port) == 0)
	    return _port.value;
    }
    else
#endif
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
