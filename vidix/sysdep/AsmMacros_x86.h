/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/util/AsmMacros.h,v 1.1 2001/11/16 21:13:34 tsi Exp $
   Modified for readability by Nick Kurshev
*/
/*
 * (c) Copyright 1993,1994 by David Wexelblat <dwex@xfree86.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * DAVID WEXELBLAT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of David Wexelblat shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from David Wexelblat.
 */
/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 * This software is furnished under license and may be used and copied only in
 * accordance with the following terms and conditions.  Subject to these
 * conditions, you may download, copy, install, use, modify and distribute
 * this software in source and/or binary form. No title or ownership is
 * transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce and retain
 *    this copyright notice and list of conditions as they appear in the source
 *    file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of Digital
 *    Equipment Corporation. Neither the "Digital Equipment Corporation" name
 *    nor any trademark or logo of Digital Equipment Corporation may be used
 *    to endorse or promote products derived from this software without the
 *    prior written permission of Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied warranties,
 *    including but not limited to, any implied warranties of merchantability,
 *    fitness for a particular purpose, or non-infringement are disclaimed. In
 *    no event shall DIGITAL be liable for any damages whatsoever, and in
 *    particular, DIGITAL shall not be liable for special, indirect,
 *    consequential, or incidental damages or damages for
 *    lost profits, loss of revenue or loss of use, whether such damages arise
 *    in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise, even if
 *    advised of the possibility of such damage.
 */

#ifndef MPLAYER_ASMMACROS_X86_H
#define MPLAYER_ASMMACROS_X86_H

#include "config.h"

#ifdef CONFIG_DHAHELPER
#include <sys/ioctl.h>
#include "vidix/dhahelper/dhahelper.h"

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
   __asm__ volatile("outb %0,%1" : :"a" (val), "d" (port));
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
   __asm__ volatile("outw %0,%1" : :"a" (val), "d" (port));
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
   __asm__ volatile("outl %0,%1" : :"a" (val), "d" (port));
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
   __asm__ volatile("inb %1,%0" :
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
   __asm__ volatile("inw %1,%0" :
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
   __asm__ volatile("inl %1,%0" :
       "=a" (ret) :
       "d" (port));
   return ret;
}

static __inline__ void intr_disable(void)
{
#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
	return;
#endif
  __asm__ volatile("cli");
}

static __inline__ void intr_enable(void)
{
#ifdef CONFIG_SVGAHELPER
    if (svgahelper_initialized == 1)
	return;
#endif
  __asm__ volatile("sti");
}

#endif /* MPLAYER_ASMMACROS_X86_H */
