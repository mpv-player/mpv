/*
   (C) 2002 - library implementation by Nick Kyrshev
   XFree86 3.3.3 scanpci.c, modified for GATOS/win/gfxdump by Øyvind Aabling.
 */
 
#include "libdha.h"
 
#include <errno.h>
#include <string.h>
#include "AsmMacros.h"
#ifdef __unix__
#include <unistd.h>
#include <sys/mman.h>
#elif defined ( _WIN32 )
#include <windows.h>
#else
#include <dos.h>
#endif
 
#define outb	pcioutb
#define outl	pcioutl
#define inb	pciinb
#define inl	pciinl
 
/* $XConsortium: scanpci.c /main/25 1996/10/27 11:48:40 kaleb $ */
/*
 *  name:             scanpci.c
 *
 *  purpose:          This program will scan for and print details of
 *                    devices on the PCI bus.
 
 *  author:           Robin Cutshaw (robin@xfree86.org)
 *
 *  supported O/S's:  SVR4, UnixWare, SCO, Solaris,
 *                    FreeBSD, NetBSD, 386BSD, BSDI BSD/386,
 *                    Linux, Mach/386, ISC
 *                    DOS (WATCOM 9.5 compiler)
 *
 *  compiling:        [g]cc scanpci.c -o scanpci
 *                    for SVR4 (not Solaris), UnixWare use:
 *                        [g]cc -DSVR4 scanpci.c -o scanpci
 *                    for DOS, watcom 9.5:
 *                        wcc386p -zq -omaxet -7 -4s -s -w3 -d2 name.c
 *                        and link with PharLap or other dos extender for exe
 *
 */
 
/* $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $ */
 
/*
 * Copyright 1995 by Robin Cutshaw <robin@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of the above listed copyright holder(s)
 * not be used in advertising or publicity pertaining to distribution of
 * the software without specific, written prior permission.  The above listed
 * copyright holder(s) make(s) no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM(S) ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
 
#if defined(__SVR4)
#if !defined(SVR4)
#define SVR4
#endif
#endif
 
#ifdef __EMX__
#define INCL_DOSFILEMGR
#include <os2.h>
#endif
 
#include <stdio.h>
#include <sys/types.h>
#if defined(SVR4)
#if defined(sun)
#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif
#endif
#include <sys/proc.h>
#include <sys/tss.h>
#if defined(NCR)
#define __STDC
#include <sys/sysi86.h>
#undef __STDC
#else
#include <sys/sysi86.h>
#endif
#if defined(__SUNPRO_C) || defined(sun) || defined(__sun)
#include <sys/psw.h>
#else
#include <sys/seg.h>
#endif
#include <sys/v86.h>
#endif
#if defined(__FreeBSD__) || defined(__386BSD__)
#include <sys/file.h>
#include <machine/console.h>
#ifndef GCCUSESGAS
#define GCCUSESGAS
#endif
#endif
#if defined(__NetBSD__)
#include <sys/param.h>
#include <sys/file.h>
#include <machine/sysarch.h>
#ifndef GCCUSESGAS
#define GCCUSESGAS
#endif
#endif
#if defined(__bsdi__)
#include <sys/file.h>
#include <sys/ioctl.h>
#include <i386/isa/pcconsioctl.h>
#ifndef GCCUSESGAS
#define GCCUSESGAS
#endif
#endif
#if defined(SCO) || defined(ISC)
#ifndef ISC
#include <sys/console.h>
#endif
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/region.h>
#include <sys/proc.h>
#include <sys/tss.h>
#include <sys/sysi86.h>
#include <sys/v86.h>
#endif
#if defined(Lynx_22)
#ifndef GCCUSESGAS
#define GCCUSESGAS
#endif
#endif
 
 
#if defined(__WATCOMC__)
 
#include <stdlib.h>
static void outl(unsigned port, unsigned data);
#pragma aux outl =  "out    dx, eax" parm [dx] [eax];
static void outb(unsigned port, unsigned data);
#pragma aux outb = "out    dx, al" parm [dx] [eax];
static unsigned inl(unsigned port);
#pragma aux inl = "in     eax, dx" parm [dx];
static unsigned inb(unsigned port);
#pragma aux inb = "xor    eax,eax" "in     al, dx" parm [dx];
 
#else /* __WATCOMC__ */
 
#if defined(__GNUC__)
 
#if !defined(__alpha__) && !defined(__powerpc__)
#if defined(GCCUSESGAS)
#define OUTB_GCC "outb %0,%1"
#define OUTL_GCC "outl %0,%1"
#define INB_GCC  "inb %1,%0"
#define INL_GCC  "inl %1,%0"
#else
#define OUTB_GCC "out%B0 (%1)"
#define OUTL_GCC "out%L0 (%1)"
#define INB_GCC "in%B0 (%1)"
#define INL_GCC "in%L0 (%1)"
#endif /* GCCUSESGAS */
 
static void outb(unsigned short port, unsigned char val) {
     __asm__ __volatile__(OUTB_GCC : :"a" (val), "d" (port)); }
static void outl(unsigned short port, unsigned long val) {
     __asm__ __volatile__(OUTL_GCC : :"a" (val), "d" (port)); }
static unsigned char inb(unsigned short port) { unsigned char ret;
     __asm__ __volatile__(INB_GCC : "=a" (ret) : "d" (port)); return ret; }
static unsigned long inl(unsigned short port) { unsigned long ret;
     __asm__ __volatile__(INL_GCC : "=a" (ret) : "d" (port)); return ret; }
 
#endif /* !defined(__alpha__) && !defined(__powerpc__) */
#else  /* __GNUC__ */
 
#if defined(__STDC__) && (__STDC__ == 1)
# if !defined(NCR)
#  define asm __asm
# endif
#endif
 
#if defined(__SUNPRO_C)
/*
 * This section is a gross hack in if you tell anyone that I wrote it,
 * I'll deny it.  :-)
 * The leave/ret instructions are the big hack to leave %eax alone on return.
 */
static unsigned char inb(int port) {
		asm("	movl 8(%esp),%edx");
		asm("	subl %eax,%eax");
		asm("	inb  (%dx)");
		asm("	leave");
		asm("	ret");
	}
 
static unsigned short inw(int port) {
		asm("	movl 8(%esp),%edx");
		asm("	subl %eax,%eax");
		asm("	inw  (%dx)");
		asm("	leave");
		asm("	ret");
	}
 
static unsigned long inl(int port) {
		asm("	movl 8(%esp),%edx");
		asm("	inl  (%dx)");
		asm("	leave");
		asm("	ret");
	}
 
static void outb(int port, unsigned char value) {
		asm("	movl 8(%esp),%edx");
		asm("	movl 12(%esp),%eax");
		asm("	outb (%dx)");
	}
 
static void outw(int port, unsigned short value) {
		asm("	movl 8(%esp),%edx");
		asm("	movl 12(%esp),%eax");
		asm("	outw (%dx)");
	}
 
static void outl(int port, unsigned long value) {
		asm("	movl 8(%esp),%edx");
		asm("	movl 12(%esp),%eax");
		asm("	outl (%dx)");
	}
#else
 
#if defined(SVR4)
# if !defined(__USLC__)
#  define __USLC__
# endif
#endif
 
#ifdef __unix__
#ifndef SCO325
# include <sys/inline.h>
#else
# include "scoasm.h"
#endif
#endif
 
#endif /* SUNPRO_C */
 
#endif /* __GNUC__ */
#endif /* __WATCOMC__ */
 
#undef outb
#undef outl
#undef inb
#undef inl
 
#if defined(__GLIBC__) && __GLIBC__ >= 2
#if defined(linux)
#ifdef __i386__
#include <sys/perm.h>
#else
#include <sys/io.h>
#endif
#endif
#endif
 
#if defined(__alpha__)
#if defined(linux)
#include <asm/unistd.h>
#define BUS(tag) (((tag)>>16)&0xff)
#define DFN(tag) (((tag)>>8)&0xff)
#else
Generate compiler error - scanpci unsupported on non-linux alpha platforms
#endif /* linux */
#endif /* __alpha__ */
#if defined(Lynx) && defined(__powerpc__)
/* let's mimick the Linux Alpha stuff for LynxOS so we don't have
 * to change too much code
 */
#include <smem.h>
 
static unsigned char *pciConfBase;
 
static __inline__ unsigned long
static swapl(unsigned long val)
{
	unsigned char *p = (unsigned char *)&val;
	return ((p[3] << 24) | (p[2] << 16) | (p[1] << 8) | (p[0] << 0));
}
 
 
#define BUS(tag) (((tag)>>16)&0xff)
#define DFN(tag) (((tag)>>8)&0xff)
 
#define PCIBIOS_DEVICE_NOT_FOUND	0x86
#define PCIBIOS_SUCCESSFUL		0x00
 
static int pciconfig_read(
          unsigned char bus,
          unsigned char dev,
          unsigned char offset,
          int len,		/* unused, alway 4 */
          unsigned long *val)
{
	unsigned long _val;
	unsigned long *ptr;
 
	dev >>= 3;
	if (bus || dev >= 16) {
		*val = 0xFFFFFFFF;
		return PCIBIOS_DEVICE_NOT_FOUND;
	} else {
		ptr = (unsigned long *)(pciConfBase + ((1<<dev) | offset));
		_val = swapl(*ptr);
	}
	*val = _val;
	return PCIBIOS_SUCCESSFUL;
}
 
static int pciconfig_write(
          unsigned char bus,
          unsigned char dev,
          unsigned char offset,
          int len,		/* unused, alway 4 */
          unsigned long val)
{
	unsigned long _val;
	unsigned long *ptr;
 
	dev >>= 3;
	_val = swapl(val);
	if (bus || dev >= 16) {
		return PCIBIOS_DEVICE_NOT_FOUND;
	} else {
		ptr = (unsigned long *)(pciConfBase + ((1<<dev) | offset));
		*ptr = _val;
	}
	return PCIBIOS_SUCCESSFUL;
}
#endif
 
#if !defined(__powerpc__)
struct pci_config_reg {
    /* start of official PCI config space header */
    union {
        unsigned long device_vendor;
	struct {
	    unsigned short vendor;
	    unsigned short device;
	} dv;
    } dv_id;
#define _device_vendor dv_id.device_vendor
#define _vendor dv_id.dv.vendor
#define _device dv_id.dv.device
    union {
        unsigned long status_command;
	struct {
	    unsigned short command;
	    unsigned short status;
	} sc;
    } stat_cmd;
#define _status_command stat_cmd.status_command
#define _command stat_cmd.sc.command
#define _status  stat_cmd.sc.status
    union {
        unsigned long class_revision;
	struct {
	    unsigned char rev_id;
	    unsigned char prog_if;
	    unsigned char sub_class;
	    unsigned char base_class;
	} cr;
    } class_rev;
#define _class_revision class_rev.class_revision
#define _rev_id     class_rev.cr.rev_id
#define _prog_if    class_rev.cr.prog_if
#define _sub_class  class_rev.cr.sub_class
#define _base_class class_rev.cr.base_class
    union {
        unsigned long bist_header_latency_cache;
	struct {
	    unsigned char cache_line_size;
	    unsigned char latency_timer;
	    unsigned char header_type;
	    unsigned char bist;
	} bhlc;
    } bhlc;
#define _bist_header_latency_cache bhlc.bist_header_latency_cache
#define _cache_line_size bhlc.bhlc.cache_line_size
#define _latency_timer   bhlc.bhlc.latency_timer
#define _header_type     bhlc.bhlc.header_type
#define _bist            bhlc.bhlc.bist
    union {
	struct {
	    unsigned long dv_base0;
	    unsigned long dv_base1;
	    unsigned long dv_base2;
	    unsigned long dv_base3;
	    unsigned long dv_base4;
	    unsigned long dv_base5;
	} dv;
	struct {
	    unsigned long bg_rsrvd[2];
	    unsigned char primary_bus_number;
	    unsigned char secondary_bus_number;
	    unsigned char subordinate_bus_number;
	    unsigned char secondary_latency_timer;
	    unsigned char io_base;
	    unsigned char io_limit;
	    unsigned short secondary_status;
	    unsigned short mem_base;
	    unsigned short mem_limit;
	    unsigned short prefetch_mem_base;
	    unsigned short prefetch_mem_limit;
	} bg;
    } bc;
#define	_base0				bc.dv.dv_base0
#define	_base1				bc.dv.dv_base1
#define	_base2				bc.dv.dv_base2
#define	_base3				bc.dv.dv_base3
#define	_base4				bc.dv.dv_base4
#define	_base5				bc.dv.dv_base5
#define	_primary_bus_number		bc.bg.primary_bus_number
#define	_secondary_bus_number		bc.bg.secondary_bus_number
#define	_subordinate_bus_number		bc.bg.subordinate_bus_number
#define	_secondary_latency_timer	bc.bg.secondary_latency_timer
#define _io_base			bc.bg.io_base
#define _io_limit			bc.bg.io_limit
#define _secondary_status		bc.bg.secondary_status
#define _mem_base			bc.bg.mem_base
#define _mem_limit			bc.bg.mem_limit
#define _prefetch_mem_base		bc.bg.prefetch_mem_base
#define _prefetch_mem_limit		bc.bg.prefetch_mem_limit
    unsigned long rsvd1;
    unsigned long rsvd2;
    unsigned long _baserom;
    unsigned long rsvd3;
    unsigned long rsvd4;
    union {
        unsigned long max_min_ipin_iline;
	struct {
	    unsigned char int_line;
	    unsigned char int_pin;
	    unsigned char min_gnt;
	    unsigned char max_lat;
	} mmii;
    } mmii;
#define _max_min_ipin_iline mmii.max_min_ipin_iline
#define _int_line mmii.mmii.int_line
#define _int_pin  mmii.mmii.int_pin
#define _min_gnt  mmii.mmii.min_gnt
#define _max_lat  mmii.mmii.max_lat
    /* I don't know how accurate or standard this is (DHD) */
    union {
	unsigned long user_config;
	struct {
	    unsigned char user_config_0;
	    unsigned char user_config_1;
	    unsigned char user_config_2;
	    unsigned char user_config_3;
	} uc;
    } uc;
#define _user_config uc.user_config
#define _user_config_0 uc.uc.user_config_0
#define _user_config_1 uc.uc.user_config_1
#define _user_config_2 uc.uc.user_config_2
#define _user_config_3 uc.uc.user_config_3
    /* end of official PCI config space header */
    unsigned long _pcibusidx;
    unsigned long _pcinumbus;
    unsigned long _pcibuses[16];
    unsigned short _configtype;   /* config type found                   */
    unsigned short _ioaddr;       /* config type 1 - private I/O addr    */
    unsigned long _cardnum;       /* config type 2 - private card number */
};
#else
/* ppc is big endian, swapping bytes is not quite enough
 * to interpret the PCI config registers...
 */
struct pci_config_reg {
    /* start of official PCI config space header */
    union {
        unsigned long device_vendor;
	struct {
	    unsigned short device;
	    unsigned short vendor;
	} dv;
    } dv_id;
#define _device_vendor dv_id.device_vendor
#define _vendor dv_id.dv.vendor
#define _device dv_id.dv.device
    union {
        unsigned long status_command;
	struct {
	    unsigned short status;
	    unsigned short command;
	} sc;
    } stat_cmd;
#define _status_command stat_cmd.status_command
#define _command stat_cmd.sc.command
#define _status  stat_cmd.sc.status
    union {
        unsigned long class_revision;
	struct {
	    unsigned char base_class;
	    unsigned char sub_class;
	    unsigned char prog_if;
	    unsigned char rev_id;
	} cr;
    } class_rev;
#define _class_revision class_rev.class_revision
#define _rev_id     class_rev.cr.rev_id
#define _prog_if    class_rev.cr.prog_if
#define _sub_class  class_rev.cr.sub_class
#define _base_class class_rev.cr.base_class
    union {
        unsigned long bist_header_latency_cache;
	struct {
	    unsigned char bist;
	    unsigned char header_type;
	    unsigned char latency_timer;
	    unsigned char cache_line_size;
	} bhlc;
    } bhlc;
#define _bist_header_latency_cache bhlc.bist_header_latency_cache
#define _cache_line_size bhlc.bhlc.cache_line_size
#define _latency_timer   bhlc.bhlc.latency_timer
#define _header_type     bhlc.bhlc.header_type
#define _bist            bhlc.bhlc.bist
    union {
	struct {
	    unsigned long dv_base0;
	    unsigned long dv_base1;
	    unsigned long dv_base2;
	    unsigned long dv_base3;
	    unsigned long dv_base4;
	    unsigned long dv_base5;
	} dv;
/* ?? */
	struct {
	    unsigned long bg_rsrvd[2];
 
	    unsigned char secondary_latency_timer;
	    unsigned char subordinate_bus_number;
	    unsigned char secondary_bus_number;
	    unsigned char primary_bus_number;
 
	    unsigned short secondary_status;
	    unsigned char io_limit;
	    unsigned char io_base;
 
	    unsigned short mem_limit;
	    unsigned short mem_base;
 
	    unsigned short prefetch_mem_limit;
	    unsigned short prefetch_mem_base;
	} bg;
    } bc;
#define	_base0				bc.dv.dv_base0
#define	_base1				bc.dv.dv_base1
#define	_base2				bc.dv.dv_base2
#define	_base3				bc.dv.dv_base3
#define	_base4				bc.dv.dv_base4
#define	_base5				bc.dv.dv_base5
#define	_primary_bus_number		bc.bg.primary_bus_number
#define	_secondary_bus_number		bc.bg.secondary_bus_number
#define	_subordinate_bus_number		bc.bg.subordinate_bus_number
#define	_secondary_latency_timer	bc.bg.secondary_latency_timer
#define _io_base			bc.bg.io_base
#define _io_limit			bc.bg.io_limit
#define _secondary_status		bc.bg.secondary_status
#define _mem_base			bc.bg.mem_base
#define _mem_limit			bc.bg.mem_limit
#define _prefetch_mem_base		bc.bg.prefetch_mem_base
#define _prefetch_mem_limit		bc.bg.prefetch_mem_limit
    unsigned long rsvd1;
    unsigned long rsvd2;
    unsigned long _baserom;
    unsigned long rsvd3;
    unsigned long rsvd4;
    union {
        unsigned long max_min_ipin_iline;
	struct {
	    unsigned char max_lat;
	    unsigned char min_gnt;
	    unsigned char int_pin;
	    unsigned char int_line;
	} mmii;
    } mmii;
#define _max_min_ipin_iline mmii.max_min_ipin_iline
#define _int_line mmii.mmii.int_line
#define _int_pin  mmii.mmii.int_pin
#define _min_gnt  mmii.mmii.min_gnt
#define _max_lat  mmii.mmii.max_lat
    /* I don't know how accurate or standard this is (DHD) */
    union {
	unsigned long user_config;
	struct {
	    unsigned char user_config_3;
	    unsigned char user_config_2;
	    unsigned char user_config_1;
	    unsigned char user_config_0;
	} uc;
    } uc;
#define _user_config uc.user_config
#define _user_config_0 uc.uc.user_config_0
#define _user_config_1 uc.uc.user_config_1
#define _user_config_2 uc.uc.user_config_2
#define _user_config_3 uc.uc.user_config_3
    /* end of official PCI config space header */
    unsigned long _pcibusidx;
    unsigned long _pcinumbus;
    unsigned long _pcibuses[16];
    unsigned short _ioaddr;       /* config type 1 - private I/O addr    */
    unsigned short _configtype;   /* config type found                   */
    unsigned long _cardnum;       /* config type 2 - private card number */
};
#endif

#define MAX_DEV_PER_VENDOR_CFG1 64
#define MAX_PCI_DEVICES_PER_BUS 32
#define MAX_PCI_DEVICES         64
#define NF ((void (*)())NULL), { 0.0, 0, 0, NULL }
#define PCI_MULTIFUNC_DEV	0x80
#if defined(__alpha__) || defined(__powerpc__)
#define PCI_ID_REG              0x00
#define PCI_CMD_STAT_REG        0x04
#define PCI_CLASS_REG           0x08
#define PCI_HEADER_MISC         0x0C
#define PCI_MAP_REG_START       0x10
#define PCI_MAP_ROM_REG         0x30
#define PCI_INTERRUPT_REG       0x3C
#define PCI_REG_USERCONFIG      0x40
#endif
 
static int pcibus=-1, pcicard=-1, pcifunc=-1 ;
/*static struct pci_device *pcidev=NULL ;*/
 
#if defined(__alpha__)
#define PCI_EN 0x00000000
#else
#define PCI_EN 0x80000000
#endif
 
#define	PCI_MODE1_ADDRESS_REG		0xCF8
#define	PCI_MODE1_DATA_REG		0xCFC
 
#define	PCI_MODE2_ENABLE_REG		0xCF8
#ifdef PC98
#define	PCI_MODE2_FORWARD_REG		0xCF9
#else
#define	PCI_MODE2_FORWARD_REG		0xCFA
#endif
 
static int pcicards=0 ;
static pciinfo_t *pci_lst;
 
static void identify_card(struct pci_config_reg *pcr)
{
 
  if (pcicards>=MAX_PCI_DEVICES) return ;
 
  pci_lst[pcicards].bus     = pcibus ;
  pci_lst[pcicards].card    = pcicard ;
  pci_lst[pcicards].func    = pcifunc ;
  pci_lst[pcicards].vendor  = pcr->_vendor ;
  pci_lst[pcicards].device  = pcr->_device ;
  pci_lst[pcicards].base0   = 0xFFFFFFFF ;
  pci_lst[pcicards].base1   = 0xFFFFFFFF ;
  pci_lst[pcicards].base2   = 0xFFFFFFFF ;
  pci_lst[pcicards].baserom = 0x000C0000 ;
  if (pcr->_base0) pci_lst[pcicards].base0 = pcr->_base0 &
                     ((pcr->_base0&0x1) ? 0xFFFFFFFC : 0xFFFFFFF0) ;
  if (pcr->_base1) pci_lst[pcicards].base1 = pcr->_base1 &
                     ((pcr->_base1&0x1) ? 0xFFFFFFFC : 0xFFFFFFF0) ;
  if (pcr->_base2) pci_lst[pcicards].base2 = pcr->_base2 &
                     ((pcr->_base2&0x1) ? 0xFFFFFFFC : 0xFFFFFFF0) ;
  if (pcr->_baserom) pci_lst[pcicards].baserom = pcr->_baserom ;
 
  pcicards++;
}

static int io_fd;
#ifdef __EMX__
static USHORT callgate[3] = {0,0,0};
#endif
 
static void enable_os_io(void)
{
    io_fd = -1 ;
#if defined(SVR4) || defined(SCO) || defined(ISC)
#if defined(SI86IOPL)
    sysi86(SI86IOPL, 3);
#else
    sysi86(SI86V86, V86SC_IOPL, PS_IOPL);
#endif
#endif
#if defined(linux)
    iopl(3);
#endif
#if defined(__FreeBSD__)  || defined(__386BSD__) || defined(__bsdi__)
    if ((io_fd = open("/dev/console", O_RDWR, 0)) < 0) {
        perror("/dev/console");
        exit(1);
    }
#if defined(__FreeBSD__)  || defined(__386BSD__)
    if (ioctl(io_fd, KDENABIO, 0) < 0) {
        perror("ioctl(KDENABIO)");
        exit(1);
    }
#endif
#if defined(__bsdi__)
    if (ioctl(io_fd, PCCONENABIOPL, 0) < 0) {
        perror("ioctl(PCCONENABIOPL)");
        exit(1);
    }
#endif
#endif
#if defined(__NetBSD__)
#if !defined(USE_I386_IOPL)
    if ((io_fd = open("/dev/io", O_RDWR, 0)) < 0) {
	perror("/dev/io");
	exit(1);
    }
#else
    if (i386_iopl(1) < 0) {
	perror("i386_iopl");
	exit(1);
    }
#endif /* USE_I386_IOPL */
#endif /* __NetBSD__ */
#if defined(__OpenBSD__)
    if (i386_iopl(1) < 0) {
	perror("i386_iopl");
	exit(1);
    }
#endif /* __OpenBSD__ */
#if defined(MACH386)
    if ((io_fd = open("/dev/iopl", O_RDWR, 0)) < 0) {
        perror("/dev/iopl");
        exit(1);
    }
#endif
#ifdef __EMX__
    {
	HFILE hfd;
	ULONG dlen,action;
	APIRET rc;
	static char *ioDrvPath = "/dev/fastio$";
 
	if (DosOpen((PSZ)ioDrvPath, (PHFILE)&hfd, (PULONG)&action,
	   (ULONG)0, FILE_SYSTEM, FILE_OPEN,
	   OPEN_SHARE_DENYNONE|OPEN_FLAGS_NOINHERIT|OPEN_ACCESS_READONLY,
	   (ULONG)0) != 0) {
		fprintf(stderr,"Error opening fastio$ driver...\n");
		fprintf(stderr,"Please install xf86sup.sys in config.sys!\n");
		exit(42);
	}
	callgate[0] = callgate[1] = 0;
 
/* Get callgate from driver for fast io to ports and other stuff */
 
	rc = DosDevIOCtl(hfd, (ULONG)0x76, (ULONG)0x64,
		NULL, 0, NULL,
		(ULONG*)&callgate[2], sizeof(USHORT), &dlen);
	if (rc) {
		fprintf(stderr,"xf86-OS/2: EnableIOPorts failed, rc=%d, dlen=%d; emergency exit\n",
			rc,dlen);
		DosClose(hfd);
		exit(42);
	}
 
/* Calling callgate with function 13 sets IOPL for the program */
 
	asm volatile ("movl $13,%%ebx;.byte 0xff,0x1d;.long _callgate"
			: /*no outputs */
			: /*no inputs */
			: "eax","ebx","ecx","edx","cc");
 
        DosClose(hfd);
   }
#endif
#if defined(Lynx) && defined(__powerpc__)
    pciConfBase = (unsigned char *) smem_create("PCI-CONF",
    	    (char *)0x80800000, 64*1024, SM_READ|SM_WRITE);
    if (pciConfBase == (void *) -1)
        exit(1);
#endif
}
 
 
static void disable_os_io(void)
{
#if defined(SVR4) || defined(SCO) || defined(ISC)
#if defined(SI86IOPL)
    sysi86(SI86IOPL, 0);
#else
    sysi86(SI86V86, V86SC_IOPL, 0);
#endif
#endif
#if defined(linux)
    iopl(0);
#endif
#if defined(__FreeBSD__)  || defined(__386BSD__)
    if (ioctl(io_fd, KDDISABIO, 0) < 0) {
        perror("ioctl(KDDISABIO)");
	close(io_fd);
        exit(1);
    }
    close(io_fd);
#endif
#if defined(__NetBSD__)
#if !defined(USE_I386_IOPL)
    close(io_fd);
#else
    if (i386_iopl(0) < 0) {
	perror("i386_iopl");
	exit(1);
    }
#endif /* NetBSD1_1 */
#endif /* __NetBSD__ */
#if defined(__bsdi__)
    if (ioctl(io_fd, PCCONDISABIOPL, 0) < 0) {
        perror("ioctl(PCCONDISABIOPL)");
	close(io_fd);
        exit(1);
    }
    close(io_fd);
#endif
#if defined(MACH386)
    close(io_fd);
#endif
#if defined(Lynx) && defined(__powerpc__)
    smem_create(NULL, (char *) pciConfBase, 0, SM_DETACH);
    smem_remove("PCI-CONF");
    pciConfBase = NULL;
#endif
}
 
/*main(int argc, char *argv[])*/
int pci_scan(pciinfo_t *pci_list,unsigned *num_pci)
{
    unsigned long tmplong1, tmplong2, config_cmd;
    unsigned char tmp1, tmp2;
    unsigned int idx;
    struct pci_config_reg pcr;
    int do_mode1_scan = 0, do_mode2_scan = 0;
    int func, hostbridges=0;
    
    pci_lst = pci_list;
 
    enable_os_io();
 
#if !defined(__alpha__) && !defined(__powerpc__)
    pcr._configtype = 0;
 
    outb(PCI_MODE2_ENABLE_REG, 0x00);
    outb(PCI_MODE2_FORWARD_REG, 0x00);
    tmp1 = inb(PCI_MODE2_ENABLE_REG);
    tmp2 = inb(PCI_MODE2_FORWARD_REG);
    if ((tmp1 == 0x00) && (tmp2 == 0x00)) {
	pcr._configtype = 2;
        /*printf("PCI says configuration type 2\n");*/
    } else {
        tmplong1 = inl(PCI_MODE1_ADDRESS_REG);
        outl(PCI_MODE1_ADDRESS_REG, PCI_EN);
        tmplong2 = inl(PCI_MODE1_ADDRESS_REG);
        outl(PCI_MODE1_ADDRESS_REG, tmplong1);
        if (tmplong2 == PCI_EN) {
	    pcr._configtype = 1;
            /*printf("PCI says configuration type 1\n");*/
	} else {
            /*printf("No PCI !\n");*/
	    disable_os_io();
	    /*exit(1);*/
	    return ENODEV ;
	}
    }
#else
    pcr._configtype = 1;
#endif
 
    /* Try pci config 1 probe first */
 
    if ((pcr._configtype == 1) || do_mode1_scan) {
    /*printf("\nPCI probing configuration type 1\n");*/
 
    pcr._ioaddr = 0xFFFF;
 
    pcr._pcibuses[0] = 0;
    pcr._pcinumbus = 1;
    pcr._pcibusidx = 0;
    idx = 0;
 
    do {
        /*printf("Probing for devices on PCI bus %d:\n\n", pcr._pcibusidx);*/
 
        for (pcr._cardnum = 0x0; pcr._cardnum < MAX_PCI_DEVICES_PER_BUS;
		pcr._cardnum += 0x1) {
	  func = 0;
	  do { /* loop over the different functions, if present */
#if !defined(__alpha__) && !defined(__powerpc__)
	    config_cmd = PCI_EN | (pcr._pcibuses[pcr._pcibusidx]<<16) |
                                  (pcr._cardnum<<11) | (func<<8);
 
            outl(PCI_MODE1_ADDRESS_REG, config_cmd);         /* ioreg 0 */
            pcr._device_vendor = inl(PCI_MODE1_DATA_REG);
#else
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
		PCI_ID_REG, 4, &pcr._device_vendor);
#endif
 
            if ((pcr._vendor == 0xFFFF) || (pcr._device == 0xFFFF))
                break;   /* nothing there */
 
	    /*printf("\npci bus 0x%x cardnum 0x%02x function 0x%04x: vendor 0x%04x device 0x%04x\n",
	        pcr._pcibuses[pcr._pcibusidx], pcr._cardnum, func,
		pcr._vendor, pcr._device);*/
	    pcibus = pcr._pcibuses[pcr._pcibusidx] ;
	    pcicard = pcr._cardnum ; pcifunc = func ;
 
#if !defined(__alpha__) && !defined(__powerpc__)
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x04);
	    pcr._status_command  = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x08);
	    pcr._class_revision  = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x0C);
	    pcr._bist_header_latency_cache = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x10);
	    pcr._base0  = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x14);
	    pcr._base1  = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x18);
	    pcr._base2  = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x1C);
	    pcr._base3  = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x20);
	    pcr._base4  = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x24);
	    pcr._base5  = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x30);
	    pcr._baserom = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x3C);
	    pcr._max_min_ipin_iline = inl(PCI_MODE1_DATA_REG);
            outl(PCI_MODE1_ADDRESS_REG, config_cmd | 0x40);
	    pcr._user_config = inl(PCI_MODE1_DATA_REG);
#else
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_CMD_STAT_REG, 4, &pcr._status_command);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_CLASS_REG, 4, &pcr._class_revision);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_HEADER_MISC, 4, &pcr._bist_header_latency_cache);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_MAP_REG_START, 4, &pcr._base0);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_MAP_REG_START + 0x04, 4, &pcr._base1);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_MAP_REG_START + 0x08, 4, &pcr._base2);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_MAP_REG_START + 0x0C, 4, &pcr._base3);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_MAP_REG_START + 0x10, 4, &pcr._base4);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_MAP_REG_START + 0x14, 4, &pcr._base5);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_MAP_ROM_REG, 4, &pcr._baserom);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_INTERRUPT_REG, 4, &pcr._max_min_ipin_iline);
	    pciconfig_read(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum<<3,
			PCI_REG_USERCONFIG, 4, &pcr._user_config);
#endif
 
            /* check for pci-pci bridges */
#define PCI_CLASS_MASK 		0xff000000
#define PCI_SUBCLASS_MASK 	0x00ff0000
#define PCI_CLASS_BRIDGE 	0x06000000
#define PCI_SUBCLASS_BRIDGE_PCI	0x00040000
	    switch(pcr._class_revision & (PCI_CLASS_MASK|PCI_SUBCLASS_MASK)) {
		case PCI_CLASS_BRIDGE|PCI_SUBCLASS_BRIDGE_PCI:
		    if (pcr._secondary_bus_number > 0) {
		        pcr._pcibuses[pcr._pcinumbus++] = pcr._secondary_bus_number;
		    }
			break;
		case PCI_CLASS_BRIDGE:
		    if ( ++hostbridges > 1) {
			pcr._pcibuses[pcr._pcinumbus] = pcr._pcinumbus;
			pcr._pcinumbus++;
		    }
			break;
		default:
			break;
	    }
	    if((func==0) && ((pcr._header_type & PCI_MULTIFUNC_DEV) == 0)) {
	        /* not a multi function device */
		func = 8;
	    } else {
	        func++;
	    }
 
	    if (idx++ >= MAX_PCI_DEVICES)
	        continue;
 
	    identify_card(&pcr);
	  } while( func < 8 );
        }
    } while (++pcr._pcibusidx < pcr._pcinumbus);
    }
 
#if !defined(__alpha__) && !defined(__powerpc__)
    /* Now try pci config 2 probe (deprecated) */
 
    if ((pcr._configtype == 2) || do_mode2_scan) {
    outb(PCI_MODE2_ENABLE_REG, 0xF1);
    outb(PCI_MODE2_FORWARD_REG, 0x00); /* bus 0 for now */
 
    /*printf("\nPCI probing configuration type 2\n");*/
 
    pcr._pcibuses[0] = 0;
    pcr._pcinumbus = 1;
    pcr._pcibusidx = 0;
    idx = 0;
 
    do {
        for (pcr._ioaddr = 0xC000; pcr._ioaddr < 0xD000; pcr._ioaddr += 0x0100){
	    outb(PCI_MODE2_FORWARD_REG, pcr._pcibuses[pcr._pcibusidx]); /* bus 0 for now */
            pcr._device_vendor = inl(pcr._ioaddr);
	    outb(PCI_MODE2_FORWARD_REG, 0x00); /* bus 0 for now */
 
            if ((pcr._vendor == 0xFFFF) || (pcr._device == 0xFFFF))
                continue;
            if ((pcr._vendor == 0xF0F0) || (pcr._device == 0xF0F0))
                continue;  /* catch ASUS P55TP4XE motherboards */
 
	    /*printf("\npci bus 0x%x slot at 0x%04x, vendor 0x%04x device 0x%04x\n",
	        pcr._pcibuses[pcr._pcibusidx], pcr._ioaddr, pcr._vendor,
                pcr._device);*/
	    pcibus = pcr._pcibuses[pcr._pcibusidx] ;
	    pcicard = pcr._ioaddr ; pcifunc = 0 ;
 
	    outb(PCI_MODE2_FORWARD_REG, pcr._pcibuses[pcr._pcibusidx]); /* bus 0 for now */
            pcr._status_command = inl(pcr._ioaddr + 0x04);
            pcr._class_revision = inl(pcr._ioaddr + 0x08);
            pcr._bist_header_latency_cache = inl(pcr._ioaddr + 0x0C);
            pcr._base0 = inl(pcr._ioaddr + 0x10);
            pcr._base1 = inl(pcr._ioaddr + 0x14);
            pcr._base2 = inl(pcr._ioaddr + 0x18);
            pcr._base3 = inl(pcr._ioaddr + 0x1C);
            pcr._base4 = inl(pcr._ioaddr + 0x20);
            pcr._base5 = inl(pcr._ioaddr + 0x24);
            pcr._baserom = inl(pcr._ioaddr + 0x30);
            pcr._max_min_ipin_iline = inl(pcr._ioaddr + 0x3C);
            pcr._user_config = inl(pcr._ioaddr + 0x40);
	    outb(PCI_MODE2_FORWARD_REG, 0x00); /* bus 0 for now */
 
            /* check for pci-pci bridges (currently we only know Digital) */
            if ((pcr._vendor == 0x1011) && (pcr._device == 0x0001))
                if (pcr._secondary_bus_number > 0)
                    pcr._pcibuses[pcr._pcinumbus++] = pcr._secondary_bus_number;
 
	    if (idx++ >= MAX_PCI_DEVICES)
	        continue;
 
	    identify_card(&pcr);
	}
    } while (++pcr._pcibusidx < pcr._pcinumbus);
 
    outb(PCI_MODE2_ENABLE_REG, 0x00);
    }
 
#endif /* __alpha__ */
 
    disable_os_io();
    *num_pci = pcicards;
 
    return 0 ;
 
}

#if 0 
void
print_i128(struct pci_config_reg *pcr)
{
    /*
    if (pcr->_status_command)
        printf("  STATUS    0x%04x  COMMAND 0x%04x\n",
            pcr->_status, pcr->_command);
    if (pcr->_class_revision)
        printf("  CLASS     0x%02x 0x%02x 0x%02x  REVISION 0x%02x\n",
            pcr->_base_class, pcr->_sub_class, pcr->_prog_if, pcr->_rev_id);
    if (pcr->_bist_header_latency_cache)
        printf("  BIST      0x%02x  HEADER 0x%02x  LATENCY 0x%02x  CACHE 0x%02x\n",
            pcr->_bist, pcr->_header_type, pcr->_latency_timer,
            pcr->_cache_line_size);
    printf("  MW0_AD    0x%08x  addr 0x%08x  %spre-fetchable\n",
        pcr->_base0, pcr->_base0 & 0xFFC00000,
        pcr->_base0 & 0x8 ? "" : "not-");
    printf("  MW1_AD    0x%08x  addr 0x%08x  %spre-fetchable\n",
        pcr->_base1, pcr->_base1 & 0xFFC00000,
        pcr->_base1 & 0x8 ? "" : "not-");
    printf("  XYW_AD(A) 0x%08x  addr 0x%08x\n",
        pcr->_base2, pcr->_base2 & 0xFFC00000);
    printf("  XYW_AD(B) 0x%08x  addr 0x%08x\n",
        pcr->_base3, pcr->_base3 & 0xFFC00000);
    printf("  RBASE_G   0x%08x  addr 0x%08x\n",
        pcr->_base4, pcr->_base4 & 0xFFFF0000);
    printf("  IO        0x%08x  addr 0x%08x\n",
        pcr->_base5, pcr->_base5 & 0xFFFFFF00);
    printf("  RBASE_E   0x%08x  addr 0x%08x  %sdecode-enabled\n",
        pcr->_baserom, pcr->_baserom & 0xFFFF8000,
        pcr->_baserom & 0x1 ? "" : "not-");
    if (pcr->_max_min_ipin_iline)
        printf("  MAX_LAT   0x%02x  MIN_GNT 0x%02x  INT_PIN 0x%02x  INT_LINE 0x%02x\n",
            pcr->_max_lat, pcr->_min_gnt, pcr->_int_pin, pcr->_int_line);
    */
}
 
void
print_pcibridge(struct pci_config_reg *pcr)
{
    /*
    if (pcr->_status_command)
        printf("  STATUS    0x%04x  COMMAND 0x%04x\n",
            pcr->_status, pcr->_command);
    if (pcr->_class_revision)
        printf("  CLASS     0x%02x 0x%02x 0x%02x  REVISION 0x%02x\n",
            pcr->_base_class, pcr->_sub_class, pcr->_prog_if, pcr->_rev_id);
    if (pcr->_bist_header_latency_cache)
        printf("  BIST      0x%02x  HEADER 0x%02x  LATENCY 0x%02x  CACHE 0x%02x\n",
            pcr->_bist, pcr->_header_type, pcr->_latency_timer,
            pcr->_cache_line_size);
    printf("  PRIBUS 0x%02x SECBUS 0x%02x SUBBUS 0x%02x SECLT 0x%02x\n",
           pcr->_primary_bus_number, pcr->_secondary_bus_number,
	   pcr->_subordinate_bus_number, pcr->_secondary_latency_timer);
    printf("  IOBASE: 0x%02x00 IOLIM 0x%02x00 SECSTATUS 0x%04x\n",
	pcr->_io_base, pcr->_io_limit, pcr->_secondary_status);
    printf("  NOPREFETCH MEMBASE: 0x%08x MEMLIM 0x%08x\n",
	pcr->_mem_base, pcr->_mem_limit);
    printf("  PREFETCH MEMBASE: 0x%08x MEMLIM 0x%08x\n",
	pcr->_prefetch_mem_base, pcr->_prefetch_mem_limit);
    printf("  RBASE_E   0x%08x  addr 0x%08x  %sdecode-enabled\n",
        pcr->_baserom, pcr->_baserom & 0xFFFF8000,
        pcr->_baserom & 0x1 ? "" : "not-");
    if (pcr->_max_min_ipin_iline)
        printf("  MAX_LAT   0x%02x  MIN_GNT 0x%02x  INT_PIN 0x%02x  INT_LINE 0x%02x\n",
            pcr->_max_lat, pcr->_min_gnt, pcr->_int_pin, pcr->_int_line);
    */
}
#endif 
