/*
   (C) 2002 - library implementation by Nick Kyrshev
   XFree86 3.3.3 scanpci.c, modified for GATOS/win/gfxdump by Øyvind Aabling.
 */
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
 
#include "libdha.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#ifdef __unix__
#include <unistd.h>
#endif
#include "AsmMacros.h"
/* OS depended stuff */
#if defined (linux)
#include "sysdep/pci_linux.c"
#elif defined (__FreeBSD__) || defined (__FreeBSD_kernel__) || defined(__DragonFly__)
#include "sysdep/pci_freebsd.c"
#elif defined (__386BSD__)
#include "sysdep/pci_386bsd.c"
#elif defined (__NetBSD__)
#include "sysdep/pci_netbsd.c"
#elif defined (__OpenBSD__)
#include "sysdep/pci_openbsd.c"
#elif defined (__bsdi__)
#include "sysdep/pci_bsdi.c"
#elif defined (Lynx)
#include "sysdep/pci_lynx.c"
#elif defined (MACH386)
#include "sysdep/pci_mach386.c"
#elif defined (__SVR4)
#if !defined(SVR4)
#define SVR4
#endif
#include "sysdep/pci_svr4.c"
#elif defined (SCO)
#include "sysdep/pci_sco.c"
#elif defined (ISC)
#include "sysdep/pci_isc.c"
#elif defined (__EMX__)
#include "sysdep/pci_os2.c"
#elif defined (_WIN32) || defined(__CYGWIN__)
#include "sysdep/pci_win32.c"
#ifdef __MINGW32__
#define ENOTSUP 134		/* Not supported */
#endif
#endif

#if 0
#if defined(__SUNPRO_C) || defined(sun) || defined(__sun)
#include <sys/psw.h>
#else
#include <sys/seg.h>
#endif
#include <sys/v86.h>
#endif
 
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
 
int pciconfig_read(
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
 
int pciconfig_write(
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
#define PCI_ID_REG              0x00
#define PCI_CMD_STAT_REG        0x04
#define PCI_CLASS_REG           0x08
#define PCI_HEADER_MISC         0x0C
#define PCI_MAP_REG_START       0x10
#define PCI_MAP_ROM_REG         0x30
#define PCI_INTERRUPT_REG       0x3C
#define PCI_REG_USERCONFIG      0x40
 
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

/* cpu depended stuff */
#ifndef CONFIG_SVGAHELPER
#if defined(__alpha__)
#include "sysdep/pci_alpha.c"
#elif defined(__ia64__)
#include "sysdep/pci_ia64.c"
#elif defined(__sparc__)
#include "sysdep/pci_sparc.c"
#elif defined( __arm32__ )
#include "sysdep/pci_arm32.c"
#elif defined(__powerpc__)
#include "sysdep/pci_powerpc.c"
#elif defined(__x86_64__)
/* Nothing here right now */
#else
#include "sysdep/pci_x86.c"
#endif
#endif
 
static pciinfo_t *pci_lst;
 
static void identify_card(struct pci_config_reg *pcr, int idx)
{
  /* local overflow test */ 
  if (idx>=MAX_PCI_DEVICES) return ;
 
  pci_lst[idx].bus     = pcibus ;
  pci_lst[idx].card    = pcicard ;
  pci_lst[idx].func    = pcifunc ;
  pci_lst[idx].command = pcr->_status_command & 0xFFFF;
  pci_lst[idx].vendor  = pcr->_vendor ;
  pci_lst[idx].device  = pcr->_device ;
  pci_lst[idx].base0   = 0xFFFFFFFF ;
  pci_lst[idx].base1   = 0xFFFFFFFF ;
  pci_lst[idx].base2   = 0xFFFFFFFF ;
  pci_lst[idx].baserom = 0x000C0000 ;
  if (pcr->_base0) pci_lst[idx].base0 = pcr->_base0 &
                     ((pcr->_base0&0x1) ? 0xFFFFFFFC : 0xFFFFFFF0) ;
  if (pcr->_base1) pci_lst[idx].base1 = pcr->_base1 &
                     ((pcr->_base1&0x1) ? 0xFFFFFFFC : 0xFFFFFFF0) ;
  if (pcr->_base2) pci_lst[idx].base2 = pcr->_base2 &
                     ((pcr->_base2&0x1) ? 0xFFFFFFFC : 0xFFFFFFF0) ;
  if (pcr->_baserom) pci_lst[idx].baserom = pcr->_baserom ;
}

/*main(int argc, char *argv[])*/
int pci_scan(pciinfo_t *pci_list,unsigned *num_pci)
{
    unsigned int idx = 0;
    struct pci_config_reg pcr;
    int do_mode1_scan = 0, do_mode2_scan = 0;
    int func, hostbridges=0;
    int ret = -1;
    
    pci_lst = pci_list;
    *num_pci = 0;
 
    ret = enable_os_io();
    if (ret != 0)
	return(ret);

    if((pcr._configtype = pci_config_type()) == 0xFFFF) return ENODEV;
 
    /* Try pci config 1 probe first */
 
    if ((pcr._configtype == 1) || do_mode1_scan) {
    /*printf("\nPCI probing configuration type 1\n");*/
 
    pcr._ioaddr = 0xFFFF;
 
    pcr._pcibuses[0] = 0;
    pcr._pcinumbus = 1;
    pcr._pcibusidx = 0;
 
    do {
        /*printf("Probing for devices on PCI bus %d:\n\n", pcr._pcibusidx);*/
 
        for (pcr._cardnum = 0x0; pcr._cardnum < MAX_PCI_DEVICES_PER_BUS;
		pcr._cardnum += 0x1) {
	  func = 0;
	  do { /* loop over the different functions, if present */
	    pcr._device_vendor = pci_get_vendor(pcr._pcibuses[pcr._pcibusidx], pcr._cardnum,
						func);
            if ((pcr._vendor == 0xFFFF) || (pcr._device == 0xFFFF))
                break;   /* nothing there */
 
	    /*printf("\npci bus 0x%x cardnum 0x%02x function 0x%04x: vendor 0x%04x device 0x%04x\n",
	        pcr._pcibuses[pcr._pcibusidx], pcr._cardnum, func,
		pcr._vendor, pcr._device);*/
	    pcibus = pcr._pcibuses[pcr._pcibusidx];
	    pcicard = pcr._cardnum;
	    pcifunc = func;
 
	    pcr._status_command = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_CMD_STAT_REG);
	    pcr._class_revision = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_CLASS_REG);
	    pcr._bist_header_latency_cache = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_HEADER_MISC);
	    pcr._base0 = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_MAP_REG_START);
	    pcr._base1 = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_MAP_REG_START+4);
	    pcr._base2 = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_MAP_REG_START+8);
	    pcr._base3 = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_MAP_REG_START+0x0C);
	    pcr._base4 = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_MAP_REG_START+0x10);
	    pcr._base5 = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_MAP_REG_START+0x14);
	    pcr._baserom = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_MAP_ROM_REG);
	    pcr._max_min_ipin_iline = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_INTERRUPT_REG);
	    pcr._user_config = pci_config_read_long(pcr._pcibuses[pcr._pcibusidx],
					pcr._cardnum,func,PCI_REG_USERCONFIG);
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
 
	    identify_card(&pcr, (*num_pci)++);
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
 
	    identify_card(&pcr, (*num_pci)++);
	}
    } while (++pcr._pcibusidx < pcr._pcinumbus);
 
    outb(PCI_MODE2_ENABLE_REG, 0x00);
    }
 
#endif /* !__alpha__ && !__powerpc__ */
 
    disable_os_io();
 
    return 0 ;
 
}

#if !defined(ENOTSUP)
#if defined(EOPNOTSUPP)
#define ENOTSUP EOPNOTSUPP
#else
#warning "ENOTSUP nor EOPNOTSUPP defined!"
#endif
#endif

int pci_config_read(unsigned char bus, unsigned char dev, unsigned char func,
		    unsigned char cmd, int len, unsigned long *val)
{
    int ret;
    
    if (len != 4)
    {
	fprintf(stderr,"pci_config_read: Reading non-dword not supported!\n");
	return(ENOTSUP);
    }
    
    ret = enable_os_io();
    if (ret != 0)
	return(ret);
    ret = pci_config_read_long(bus, dev, func, cmd);
    disable_os_io();

    *val = ret;
    return(0);
}

int enable_app_io( void )
{
  return enable_os_io();  
}

int disable_app_io( void )
{
  return disable_os_io();
}
