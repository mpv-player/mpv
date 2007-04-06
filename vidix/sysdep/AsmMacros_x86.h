/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/util/AsmMacros.h,v 1.1 2001/11/16 21:13:34 tsi Exp $
   Modified for readability by Nick Kurshev
*/

#ifndef __ASM_MACROS_X86_H
#define __ASM_MACROS_X86_H

//#if defined (WINNT)
//#error This stuff is not ported on your system
//#else

#include "config.h"

#ifdef CONFIG_DHAHELPER
#include <sys/ioctl.h>
#include "../kernelhelper/dhahelper.h"

extern int dhahelper_fd;
extern int dhahelper_initialized;
#endif

#ifdef CONFIG_SVGAHELPER
#include <sys/ioctl.h>
#include <svgalib_helper.h>

#ifndef SVGALIB_HELPER_IOC_MAGIC
/* svgalib 1.9.18+ compatibility ::atmos */
#define SVGALIB_HELPER_IOCSOUTB	SVGAHELPER_OUTB
#define SVGALIB_HELPER_IOCSOUTW	SVGAHELPER_OUTW
#define SVGALIB_HELPER_IOCSOUTL	SVGAHELPER_OUTL
#define SVGALIB_HELPER_IOCGINB	SVGAHELPER_INB
#define SVGALIB_HELPER_IOCGINW	SVGAHELPER_INW
#define SVGALIB_HELPER_IOCGINL	SVGAHELPER_INL
#endif

extern int svgahelper_fd;
extern int svgahelper_initialized;

static __inline__ void svga_outb(short port, char value)
{
    io_t iov;
    
    iov.val = value;
    iov.port = port;
    ioctl(svgahelper_fd, SVGALIB_HELPER_IOCSOUTB, &iov);
}

static __inline__ void svga_outw(short port, char value)
{
    io_t iov;
    
    iov.val = value;
    iov.port = port;
    ioctl(svgahelper_fd, SVGALIB_HELPER_IOCSOUTW, &iov);
}

static __inline__ void svga_outl(short port, unsigned int value)
{
    io_t iov;
    
    iov.val = value;
    iov.port = port;
    ioctl(svgahelper_fd, SVGALIB_HELPER_IOCSOUTL, &iov);
}

static __inline__ unsigned int svga_inb(short port)
{
    io_t iov;
    
    iov.port = port;
    ioctl(svgahelper_fd, SVGALIB_HELPER_IOCGINB, &iov);
    
    return iov.val;
}

static __inline__ unsigned int svga_inw(short port)
{
    io_t iov;
    
    iov.port = port;
    ioctl(svgahelper_fd, SVGALIB_HELPER_IOCGINW, &iov);
    
    return iov.val;
}

static __inline__ unsigned int svga_inl(short port)
{
    io_t iov;
    
    iov.port = port;
    ioctl(svgahelper_fd, SVGALIB_HELPER_IOCGINL, &iov);
    
    return iov.val;
}
#endif /* CONIFG_SVGAHELPER */

static __inline__ void outb(short port,char val)
{
#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
    {
	svga_outb(port, val);
	return;
    }
#endif

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
#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
    {
	svga_outw(port, val);
	return;
    }
#endif

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
#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
    {
	svga_outl(port, val);
	return;
    }
#endif

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
   unsigned char ret = 0;

#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
    {
	return svga_inb(port);
    }
#endif

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
   unsigned short ret = 0;

#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
    {
	return svga_inw(port);
    }
#endif

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
   unsigned int ret = 0;

#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
    {
	return svga_inl(port);
    }
#endif

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
#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
	return;
#endif
  __asm__ __volatile__("cli");
}

static __inline__ void intr_enable()
{
#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
	return;
#endif
  __asm__ __volatile__("sti");
}

#endif

//#endif
