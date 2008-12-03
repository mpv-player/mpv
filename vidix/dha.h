/*
 * VIDIX Direct Hardware Access (DHA).
 * Copyright (C) 2002 Nick Kurshev
 *
 * 1996/10/27   - Robin Cutshaw (robin@xfree86.org)
 *                XFree86 3.3.3 implementation
 * 1999         - Øyvind Aabling.
 *                Modified for GATOS/win/gfxdump.
 *
 * 2002         - library implementation by Nick Kurshev
 *              - dhahelper and some changes by Alex Beregszaszi
 *
 * supported OSes: SVR4, UnixWare, SCO, Solaris,
 *                 FreeBSD, NetBSD, 386BSD, BSDI BSD/386,
 *                 Linux, Mach/386, ISC
 *                 DOS (WATCOM 9.5 compiler), Win9x (with mapdev.vxd)
 * original location: www.linuxvideo.org/gatos
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_DHA_H
#define MPLAYER_DHA_H

#ifndef DEV_SVGA
#define DEV_SVGA "/dev/svga"
#endif

#define MAX_DEV_PER_VENDOR_CFG1 64
#define MAX_PCI_DEVICES_PER_BUS 32
#define MAX_PCI_DEVICES         256
#define PCI_MULTIFUNC_DEV	0x80
#define PCI_COMMAND_IO          0x1     /* Enable response to I/O space */

typedef struct pciinfo_s
{
  int		bus,card,func;			/* PCI/AGP bus:card:func */
  unsigned short command;                       /* Device control register */
  unsigned short vendor,device;			/* Card vendor+device ID */
  unsigned	base0,base1,base2,baserom;	/* Memory and I/O base addresses */
//  unsigned	base0_limit, base1_limit, base2_limit, baserom_limit;
}pciinfo_t;

/* needed for mga_vid */
int pci_config_read(unsigned char bus, unsigned char dev, unsigned char func,
			unsigned char cmd, int len, unsigned long *val);
			/* Fill array pci_list which must have size MAX_PCI_DEVICES
			   and return 0 if sucessful */
int pci_scan(pciinfo_t *pci_list,unsigned *num_card);


	    /* Enables/disables accessing to IO space from application side.
	       Should return 0 if o'k or errno on error. */
int enable_app_io(void);
int disable_app_io(void);

unsigned char  INPORT8(unsigned idx);
unsigned short INPORT16(unsigned idx);
unsigned       INPORT32(unsigned idx);
#define INPORT(idx) INPORT32(idx)
void          OUTPORT8(unsigned idx, unsigned char val);
void          OUTPORT16(unsigned idx, unsigned short val);
void          OUTPORT32(unsigned idx, unsigned val);
#define OUTPORT(idx,val) OUTPORT32(idx,val)

void *  map_phys_mem(unsigned long base, unsigned long size);
void    unmap_phys_mem(void *ptr, unsigned long size);

/*  These are the region types  */
#define MTRR_TYPE_UNCACHABLE 0
#define MTRR_TYPE_WRCOMB     1
#define MTRR_TYPE_WRTHROUGH  4
#define MTRR_TYPE_WRPROT     5
#define MTRR_TYPE_WRBACK     6
int mtrr_set_type(unsigned base, unsigned size, int type);

#endif /* MPLAYER_DHA_H */
