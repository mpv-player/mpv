/*
 * Matrox MGA G200/G400 YUV Video Interface module Version 0.1.0
 * BES == Back End Scaler
 *
 * Copyright (C) 1999 Aaron Holtzman
 *
 * Module skeleton based on gutted agpgart module by
 * Jeff Hartmann <slicer@ionet.net>
 * YUY2 support (see config.format) added by A'rpi/ESP-team
 * double buffering added by A'rpi/ESP-team
 * brightness/contrast introduced by eyck
 * multiple card support by Attila Kinali <attila@kinali.ch>
 *
 * This file is part of mga_vid.
 *
 * mga_vid is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mga_vid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mga_vid; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

//It's entirely possible this major conflicts with something else
//use the 'major' parameter to override the default major number (178)
/* mknod /dev/mga_vid c 178 0 */

//#define CRTC2

// Set this value, if autodetection fails! (video ram size in megabytes)
// #define MGA_MEMORY_SIZE 16

//#define MGA_ALLOW_IRQ

#define MGA_VSYNC_POS 2

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10)
#include <linux/malloc.h>
#else
#include <linux/slab.h>
#endif

#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/init.h>

#include "mga_vid.h"

#ifdef CONFIG_MTRR 
#include <asm/mtrr.h>
#endif

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>

#define TRUE 1
#define FALSE 0

#define DEFAULT_MGA_VID_MAJOR 178

#ifndef PCI_DEVICE_ID_MATROX_G200_PCI 
#define PCI_DEVICE_ID_MATROX_G200_PCI 0x0520
#endif

#ifndef PCI_DEVICE_ID_MATROX_G200_AGP 
#define PCI_DEVICE_ID_MATROX_G200_AGP 0x0521
#endif

#ifndef PCI_DEVICE_ID_MATROX_G400 
#define PCI_DEVICE_ID_MATROX_G400 0x0525
#endif

#ifndef PCI_DEVICE_ID_MATROX_G550 
#define PCI_DEVICE_ID_MATROX_G550 0x2527
#endif

#ifndef PCI_SUBSYSTEM_ID_MATROX_G400_DH_16MB
#define PCI_SUBSYSTEM_ID_MATROX_G400_DH_16MB 0x2159
#endif

#ifndef PCI_SUBSYSTEM_ID_MATROX_G400_16MB_SGRAM
#define PCI_SUBSYSTEM_ID_MATROX_G400_16MB_SGRAM 0x19d8
#endif

#ifndef PCI_SUBSYSTEM_ID_MATROX_G400_16MB_SDRAM
#define PCI_SUBSYSTEM_ID_MATROX_G400_16MB_SDRAM 0x0328
#endif

MODULE_AUTHOR("Aaron Holtzman <aholtzma@engr.uvic.ca>");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#define PARAM_BRIGHTNESS "brightness="
#define PARAM_CONTRAST "contrast="
#define PARAM_BLACKIE "blackie="

// set PARAM_BUFF_SIZE to just below 4k because some kernel versions
// store additional information in the memory page which leads to
// the allocation of an additional page if exactly 4k is used
#define PARAM_BUFF_SIZE 4000

#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#include <linux/ctype.h>

static unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base)
{
        unsigned long result = 0,value;

        if (!base) {
                base = 10;
                if (*cp == '0') {
                        base = 8;
                        cp++;
                        if ((*cp == 'x') && isxdigit(cp[1])) {
                                cp++;
                                base = 16;
                        }
                }
        }
        while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
            ? toupper(*cp) : *cp)-'A'+10) < base) {
                result = result*base + value;
                cp++;
        }
        if (endp)
                *endp = (char *)cp;
        return result;
}

static long simple_strtol(const char *cp,char **endp,unsigned int base)
{
        if(*cp=='-')
                return -simple_strtoul(cp+1,endp,base);
        return simple_strtoul(cp,endp,base);
}
#endif


typedef struct bes_registers_s
{
	//BES Control
	uint32_t besctl;
	//BES Global control
	uint32_t besglobctl;
	//Luma control (brightness and contrast)
	uint32_t beslumactl;
	//Line pitch
	uint32_t bespitch;

	//Buffer A-1 Chroma 3 plane org
	uint32_t besa1c3org;
	//Buffer A-1 Chroma org
	uint32_t besa1corg;
	//Buffer A-1 Luma org
	uint32_t besa1org;

	//Buffer A-2 Chroma 3 plane org
	uint32_t besa2c3org;
	//Buffer A-2 Chroma org
	uint32_t besa2corg;
	//Buffer A-2 Luma org
	uint32_t besa2org;

	//Buffer B-1 Chroma 3 plane org
	uint32_t besb1c3org;
	//Buffer B-1 Chroma org
	uint32_t besb1corg;
	//Buffer B-1 Luma org
	uint32_t besb1org;

	//Buffer B-2 Chroma 3 plane org
	uint32_t besb2c3org;
	//Buffer B-2 Chroma org
	uint32_t besb2corg;
	//Buffer B-2 Luma org
	uint32_t besb2org;

	//BES Horizontal coord
	uint32_t beshcoord;
	//BES Horizontal inverse scaling [5.14]
	uint32_t beshiscal;
	//BES Horizontal source start [10.14] (for scaling)
	uint32_t beshsrcst;
	//BES Horizontal source ending [10.14] (for scaling) 
	uint32_t beshsrcend;
	//BES Horizontal source last 
	uint32_t beshsrclst;

	
	//BES Vertical coord
	uint32_t besvcoord;
	//BES Vertical inverse scaling [5.14]
	uint32_t besviscal;
	//BES Field 1 vertical source last position
	uint32_t besv1srclst;
	//BES Field 1 weight start
	uint32_t besv1wght;
	//BES Field 2 vertical source last position
	uint32_t besv2srclst;
	//BES Field 2 weight start
	uint32_t besv2wght;


	//configurable stuff
	int blackie;

} bes_registers_t;

#ifdef CRTC2
typedef struct crtc2_registers_s
{
	uint32_t c2ctl;
	uint32_t c2datactl;
	uint32_t c2misc;
	uint32_t c2hparam;
	uint32_t c2hsync;
	uint32_t c2offset;
	uint32_t c2pl2startadd0;
	uint32_t c2pl2startadd1;
	uint32_t c2pl3startadd0;
	uint32_t c2pl3startadd1;
	uint32_t c2preload;
	uint32_t c2spicstartadd0;
	uint32_t c2spicstartadd1;
	uint32_t c2startadd0;
	uint32_t c2startadd1;
	uint32_t c2subpiclut;
	uint32_t c2vcount;
	uint32_t c2vparam;
	uint32_t c2vsync;
} crtc2_registers_t;
#endif





//All register offsets are converted to word aligned offsets (32 bit)
//because we want all our register accesses to be 32 bits
#define VCOUNT      0x1e20

#define PALWTADD      0x3c00 // Index register for X_DATAREG port
#define X_DATAREG     0x3c0a

#define XMULCTRL      0x19
#define BPP_8         0x00
#define BPP_15        0x01
#define BPP_16        0x02
#define BPP_24        0x03
#define BPP_32_DIR    0x04
#define BPP_32_PAL    0x07

#define XCOLMSK       0x40
#define X_COLKEY      0x42
#define XKEYOPMODE    0x51
#define XCOLMSK0RED   0x52
#define XCOLMSK0GREEN 0x53
#define XCOLMSK0BLUE  0x54
#define XCOLKEY0RED   0x55
#define XCOLKEY0GREEN 0x56
#define XCOLKEY0BLUE  0x57

#ifdef CRTC2

/*CRTC2 registers*/
#define XMISCCTRL  0x1e
#define C2CTL       0x3c10 
#define C2DATACTL   0x3c4c
#define C2MISC      0x3c44
#define C2HPARAM    0x3c14
#define C2HSYNC     0x3c18
#define C2OFFSET    0x3c40
#define C2PL2STARTADD0 0x3c30  // like BESA1CORG
#define C2PL2STARTADD1 0x3c34  // like BESA2CORG
#define C2PL3STARTADD0 0x3c38  // like BESA1C3ORG
#define C2PL3STARTADD1 0x3c3c  // like BESA2C3ORG
#define C2PRELOAD   0x3c24
#define C2SPICSTARTADD0 0x3c54
#define C2SPICSTARTADD1 0x3c58
#define C2STARTADD0 0x3c28  // like BESA1ORG
#define C2STARTADD1 0x3c2c  // like BESA2ORG
#define C2SUBPICLUT 0x3c50
#define C2VCOUNT    0x3c48
#define C2VPARAM    0x3c1c
#define C2VSYNC     0x3c20

#endif

// Backend Scaler registers
#define BESCTL      0x3d20
#define BESGLOBCTL  0x3dc0
#define BESLUMACTL  0x3d40
#define BESPITCH    0x3d24

#define BESA1C3ORG  0x3d60
#define BESA1CORG   0x3d10
#define BESA1ORG    0x3d00

#define BESA2C3ORG  0x3d64 
#define BESA2CORG   0x3d14
#define BESA2ORG    0x3d04

#define BESB1C3ORG  0x3d68
#define BESB1CORG   0x3d18
#define BESB1ORG    0x3d08

#define BESB2C3ORG  0x3d6C
#define BESB2CORG   0x3d1C
#define BESB2ORG    0x3d0C

#define BESHCOORD   0x3d28
#define BESHISCAL   0x3d30
#define BESHSRCEND  0x3d3C
#define BESHSRCLST  0x3d50
#define BESHSRCST   0x3d38
#define BESV1WGHT   0x3d48
#define BESV2WGHT   0x3d4c
#define BESV1SRCLST 0x3d54
#define BESV2SRCLST 0x3d58
#define BESVISCAL   0x3d34
#define BESVCOORD   0x3d2c
#define BESSTATUS   0x3dc4

#define CRTCX	    0x1fd4
#define CRTCD	    0x1fd5
#define	IEN	    0x1e1c
#define ICLEAR	    0x1e18
#define STATUS      0x1e14


// global devfs handle for /dev/mga_vid
#ifdef CONFIG_DEVFS_FS
static devfs_handle_t dev_handle = NULL;
#endif

// card local config
typedef struct mga_card_s {

// local devfs handle for /dev/mga_vidX
#ifdef CONFIG_DEVFS_FS
	devfs_handle_t dev_handle;
#endif

	uint8_t *param_buff; // buffer for read()
	uint32_t param_buff_size;
	uint32_t param_buff_len;
	bes_registers_t regs;
#ifdef CRTC2
	crtc2_registers_t cregs;
#endif
	uint32_t vid_in_use;
	uint32_t is_g400;
	uint32_t vid_src_ready;
	uint32_t vid_overlay_on;

	uint8_t *mmio_base;
	uint32_t mem_base; 
	int src_base;	// YUV buffer position in video memory
	uint32_t ram_size;	// how much megabytes videoram we have
	uint32_t top_reserved;	// reserved space for console font (matroxfb + fastfont)

	int brightness;	// initial brightness
	int contrast;	// initial contrast

	struct pci_dev *pci_dev;

	mga_vid_config_t config; 
	int configured; // set to 1 when the card is configured over ioctl

	int colkey_saved;
	int colkey_on;
	unsigned char colkey_color[4];
	unsigned char colkey_mask[4];

	int irq; // = -1
	int next_frame; 
} mga_card_t;

#define MGA_MAX_CARDS 16
// this is used as init value for the parameter arrays
// it should have exactly MGA_MAX_CARDS elements
#define MGA_MAX_CARDS_INIT_ARRAY {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
static unsigned int mga_cards_num=0;
static mga_card_t * mga_cards[MGA_MAX_CARDS] = MGA_MAX_CARDS_INIT_ARRAY;

// module parameters
static int major = DEFAULT_MGA_VID_MAJOR;
static int mga_ram_size[MGA_MAX_CARDS] = MGA_MAX_CARDS_INIT_ARRAY;
static int mga_brightness[MGA_MAX_CARDS] = MGA_MAX_CARDS_INIT_ARRAY;
static int mga_contrast[MGA_MAX_CARDS] = MGA_MAX_CARDS_INIT_ARRAY;
static int mga_top_reserved[MGA_MAX_CARDS] = MGA_MAX_CARDS_INIT_ARRAY;

MODULE_PARM(mga_ram_size, "1-" __MODULE_STRING(MGA_MAX_CARDS) "i");
MODULE_PARM(mga_top_reserved, "1-" __MODULE_STRING(MGA_MAX_CARDS) "i");
MODULE_PARM(mga_brightness, "1-" __MODULE_STRING(MGA_MAX_CARDS) "i");
MODULE_PARM(mga_contrast, "1-" __MODULE_STRING(MGA_MAX_CARDS) "i");
MODULE_PARM(major, "i");

#ifdef CRTC2
static void crtc2_frame_sel(mga_card_t * card, int frame)
{
switch(frame) {
case 0:	
	card->cregs.c2pl2startadd0=card->regs.besa1corg;
	card->cregs.c2pl3startadd0=card->regs.besa1c3org;
	card->cregs.c2startadd0=card->regs.besa1org;
	break;
case 1:
	card->cregs.c2pl2startadd0=card->regs.besa2corg;
	card->cregs.c2pl3startadd0=card->regs.besa2c3org;
	card->cregs.c2startadd0=card->regs.besa2org;
	break;
case 2:
	card->cregs.c2pl2startadd0=card->regs.besb1corg;
	card->cregs.c2pl3startadd0=card->regs.besb1c3org;
	card->cregs.c2startadd0=card->regs.besb1org;
	break;
case 3:
	card->cregs.c2pl2startadd0=card->regs.besb2corg;
	card->cregs.c2pl3startadd0=card->regs.besb2c3org;
	card->cregs.c2startadd0=card->regs.besb2org;
	break;
}
	writel(card->cregs.c2startadd0, card->mmio_base + C2STARTADD0);
	writel(card->cregs.c2pl2startadd0, card->mmio_base + C2PL2STARTADD0);
	writel(card->cregs.c2pl3startadd0, card->mmio_base + C2PL3STARTADD0);
}
#endif

static void mga_vid_frame_sel(mga_card_t * card, int frame)
{
    if ( card->irq != -1 ) {
	card->next_frame=frame;
    } else {

	//we don't need the vcount protection as we're only hitting
	//one register (and it doesn't seem to be double buffered)
	card->regs.besctl = (card->regs.besctl & ~0x07000000) + (frame << 25);
	writel( card->regs.besctl, card->mmio_base + BESCTL ); 

//	writel( card->regs.besglobctl + ((readl(card->mmio_base + VCOUNT)+2)<<16),
	writel( card->regs.besglobctl + (MGA_VSYNC_POS<<16),
			card->mmio_base + BESGLOBCTL);
#ifdef CRTC2
	crtc2_frame_sel(card, frame);
#endif

    }
}


static void mga_vid_write_regs(mga_card_t * card, int restore)
{
	//Make sure internal registers don't get updated until we're done
	writel( (readl(card->mmio_base + VCOUNT)-1)<<16,
			card->mmio_base + BESGLOBCTL);

	// color or coordinate keying
	
	if(restore && card->colkey_saved){
	    // restore it
	    card->colkey_saved=0;

#ifdef MP_DEBUG
		printk("mga_vid: Restoring colorkey (ON: %d  %02X:%02X:%02X)\n",
			card->colkey_on,card->colkey_color[0],card->colkey_color[1],card->colkey_color[2]);
#endif		

		// Set color key registers:
		writeb( XKEYOPMODE, card->mmio_base + PALWTADD);
		writeb( card->colkey_on, card->mmio_base + X_DATAREG);
		
		writeb( XCOLKEY0RED, card->mmio_base + PALWTADD);
		writeb( card->colkey_color[0], card->mmio_base + X_DATAREG);
		writeb( XCOLKEY0GREEN, card->mmio_base + PALWTADD);
		writeb( card->colkey_color[1], card->mmio_base + X_DATAREG);
		writeb( XCOLKEY0BLUE, card->mmio_base + PALWTADD);
		writeb( card->colkey_color[2], card->mmio_base + X_DATAREG);
		writeb( X_COLKEY, card->mmio_base + PALWTADD);
		writeb( card->colkey_color[3], card->mmio_base + X_DATAREG);

		writeb( XCOLMSK0RED, card->mmio_base + PALWTADD);
		writeb( card->colkey_mask[0], card->mmio_base + X_DATAREG);
		writeb( XCOLMSK0GREEN, card->mmio_base + PALWTADD);
		writeb( card->colkey_mask[1], card->mmio_base + X_DATAREG);
		writeb( XCOLMSK0BLUE, card->mmio_base + PALWTADD);
		writeb( card->colkey_mask[2], card->mmio_base + X_DATAREG);
		writeb( XCOLMSK, card->mmio_base + PALWTADD);
		writeb( card->colkey_mask[3], card->mmio_base + X_DATAREG);

	} else if(!card->colkey_saved){
	    // save it
	    card->colkey_saved=1;
		// Get color key registers:
		writeb( XKEYOPMODE, card->mmio_base + PALWTADD);
		card->colkey_on=(unsigned char)readb(card->mmio_base + X_DATAREG) & 1;
		
		writeb( XCOLKEY0RED, card->mmio_base + PALWTADD);
		card->colkey_color[0]=(unsigned char)readb(card->mmio_base + X_DATAREG);
		writeb( XCOLKEY0GREEN, card->mmio_base + PALWTADD);
		card->colkey_color[1]=(unsigned char)readb(card->mmio_base + X_DATAREG);
		writeb( XCOLKEY0BLUE, card->mmio_base + PALWTADD);
		card->colkey_color[2]=(unsigned char)readb(card->mmio_base + X_DATAREG);
		writeb( X_COLKEY, card->mmio_base + PALWTADD);
		card->colkey_color[3]=(unsigned char)readb(card->mmio_base + X_DATAREG);

		writeb( XCOLMSK0RED, card->mmio_base + PALWTADD);
		card->colkey_mask[0]=(unsigned char)readb(card->mmio_base + X_DATAREG);
		writeb( XCOLMSK0GREEN, card->mmio_base + PALWTADD);
		card->colkey_mask[1]=(unsigned char)readb(card->mmio_base + X_DATAREG);
		writeb( XCOLMSK0BLUE, card->mmio_base + PALWTADD);
		card->colkey_mask[2]=(unsigned char)readb(card->mmio_base + X_DATAREG);
		writeb( XCOLMSK, card->mmio_base + PALWTADD);
		card->colkey_mask[3]=(unsigned char)readb(card->mmio_base + X_DATAREG);

#ifdef MP_DEBUG
		printk("mga_vid: Saved colorkey (ON: %d  %02X:%02X:%02X)\n",
			card->colkey_on, card->colkey_color[0], card->colkey_color[1], card->colkey_color[2]);
#endif		

	}
	
if(!restore){
	writeb( XKEYOPMODE, card->mmio_base + PALWTADD);
	writeb( card->config.colkey_on, card->mmio_base + X_DATAREG);
	if ( card->config.colkey_on ) 
	{
		uint32_t r=0, g=0, b=0;

		writeb( XMULCTRL, card->mmio_base + PALWTADD);
		switch (readb (card->mmio_base + X_DATAREG)) 
		{
			case BPP_8:
				/* Need to look up the color index, just using color 0 for now. */
			break;

			case BPP_15:
				r = card->config.colkey_red   >> 3;
				g = card->config.colkey_green >> 3;
				b = card->config.colkey_blue  >> 3;
			break;

			case BPP_16:
				r = card->config.colkey_red   >> 3;
				g = card->config.colkey_green >> 2;
				b = card->config.colkey_blue  >> 3;
			break;

			case BPP_24:
			case BPP_32_DIR:
			case BPP_32_PAL:
				r = card->config.colkey_red;
				g = card->config.colkey_green;
				b = card->config.colkey_blue;
			break;
		}

		// Disable color keying on alpha channel 
		writeb( XCOLMSK, card->mmio_base + PALWTADD);
		writeb( 0x00, card->mmio_base + X_DATAREG);
		writeb( X_COLKEY, card->mmio_base + PALWTADD);
		writeb( 0x00, card->mmio_base + X_DATAREG);


		// Set up color key registers
		writeb( XCOLKEY0RED, card->mmio_base + PALWTADD);
		writeb( r, card->mmio_base + X_DATAREG);
		writeb( XCOLKEY0GREEN, card->mmio_base + PALWTADD);
		writeb( g, card->mmio_base + X_DATAREG);
		writeb( XCOLKEY0BLUE, card->mmio_base + PALWTADD);
		writeb( b, card->mmio_base + X_DATAREG);

		// Set up color key mask registers
		writeb( XCOLMSK0RED, card->mmio_base + PALWTADD);
		writeb( 0xff, card->mmio_base + X_DATAREG);
		writeb( XCOLMSK0GREEN, card->mmio_base + PALWTADD);
		writeb( 0xff, card->mmio_base + X_DATAREG);
		writeb( XCOLMSK0BLUE, card->mmio_base + PALWTADD);
		writeb( 0xff, card->mmio_base + X_DATAREG);
	}

}

	// Backend Scaler
	writel( card->regs.besctl,      card->mmio_base + BESCTL); 
	if(card->is_g400)
		writel( card->regs.beslumactl,  card->mmio_base + BESLUMACTL); 
	writel( card->regs.bespitch,    card->mmio_base + BESPITCH); 

	writel( card->regs.besa1org,    card->mmio_base + BESA1ORG);
	writel( card->regs.besa1corg,   card->mmio_base + BESA1CORG);
	writel( card->regs.besa2org,    card->mmio_base + BESA2ORG);
	writel( card->regs.besa2corg,   card->mmio_base + BESA2CORG);
	writel( card->regs.besb1org,    card->mmio_base + BESB1ORG);
	writel( card->regs.besb1corg,   card->mmio_base + BESB1CORG);
	writel( card->regs.besb2org,    card->mmio_base + BESB2ORG);
	writel( card->regs.besb2corg,   card->mmio_base + BESB2CORG);
	if(card->is_g400) 
	{
		writel( card->regs.besa1c3org,  card->mmio_base + BESA1C3ORG);
		writel( card->regs.besa2c3org,  card->mmio_base + BESA2C3ORG);
		writel( card->regs.besb1c3org,  card->mmio_base + BESB1C3ORG);
		writel( card->regs.besb2c3org,  card->mmio_base + BESB2C3ORG);
	}

	writel( card->regs.beshcoord,   card->mmio_base + BESHCOORD);
	writel( card->regs.beshiscal,   card->mmio_base + BESHISCAL);
	writel( card->regs.beshsrcst,   card->mmio_base + BESHSRCST);
	writel( card->regs.beshsrcend,  card->mmio_base + BESHSRCEND);
	writel( card->regs.beshsrclst,  card->mmio_base + BESHSRCLST);
	
	writel( card->regs.besvcoord,   card->mmio_base + BESVCOORD);
	writel( card->regs.besviscal,   card->mmio_base + BESVISCAL);

	writel( card->regs.besv1srclst, card->mmio_base + BESV1SRCLST);
	writel( card->regs.besv1wght,   card->mmio_base + BESV1WGHT);
	writel( card->regs.besv2srclst, card->mmio_base + BESV2SRCLST);
	writel( card->regs.besv2wght,   card->mmio_base + BESV2WGHT);
	
	//update the registers somewhere between 1 and 2 frames from now.
	writel( card->regs.besglobctl + ((readl(card->mmio_base + VCOUNT)+2)<<16),
			card->mmio_base + BESGLOBCTL);

#if 0
	printk(KERN_DEBUG "mga_vid: wrote BES registers\n");
	printk(KERN_DEBUG "mga_vid: BESCTL = 0x%08x\n",
			readl(card->mmio_base + BESCTL));
	printk(KERN_DEBUG "mga_vid: BESGLOBCTL = 0x%08x\n",
			readl(card->mmio_base + BESGLOBCTL));
	printk(KERN_DEBUG "mga_vid: BESSTATUS= 0x%08x\n",
			readl(card->mmio_base + BESSTATUS));
#endif
#ifdef CRTC2
//	printk("c2ctl:0x%08x c2datactl:0x%08x\n", readl(card->mmio_base + C2CTL), readl(card->mmio_base + C2DATACTL));
//	printk("c2misc:0x%08x\n", readl(card->mmio_base + C2MISC));
//	printk("c2ctl:0x%08x c2datactl:0x%08x\n", card->cregs.c2ctl, card->cregs.c2datactl);

//	writel(card->cregs.c2ctl,	card->mmio_base + C2CTL);

	writel(((readl(card->mmio_base + C2CTL) & ~0x03e00000) + (card->cregs.c2ctl & 0x03e00000)), card->mmio_base + C2CTL);
	writel(((readl(card->mmio_base + C2DATACTL) & ~0x000000ff) + (card->cregs.c2datactl & 0x000000ff)), card->mmio_base + C2DATACTL);
	// ctrc2
	// disable CRTC2 acording to specs
//	writel(card->cregs.c2ctl & 0xfffffff0, card->mmio_base + C2CTL);
 // je to treba ???
//	writeb((readb(card->mmio_base + XMISCCTRL) & 0x19) | 0xa2, card->mmio_base + XMISCCTRL); // MAFC - mfcsel & vdoutsel
//	writeb((readb(card->mmio_base + XMISCCTRL) & 0x19) | 0x92, card->mmio_base + XMISCCTRL);
//	writeb((readb(card->mmio_base + XMISCCTRL) & ~0xe9) + 0xa2, card->mmio_base + XMISCCTRL);
//	writel(card->cregs.c2datactl, card->mmio_base + C2DATACTL);
//	writel(card->cregs.c2hparam, card->mmio_base + C2HPARAM);
//	writel(card->cregs.c2hsync, card->mmio_base + C2HSYNC);
//	writel(card->cregs.c2vparam, card->mmio_base + C2VPARAM);
//	writel(card->cregs.c2vsync, card->mmio_base + C2VSYNC);
	writel(card->cregs.c2misc, card->mmio_base + C2MISC);

#ifdef MP_DEBUG
	printk("c2offset = %d\n",card->cregs.c2offset);
#endif	

	writel(card->cregs.c2offset, card->mmio_base + C2OFFSET);
	writel(card->cregs.c2startadd0, card->mmio_base + C2STARTADD0);
//	writel(card->cregs.c2startadd1, card->mmio_base + C2STARTADD1);
	writel(card->cregs.c2pl2startadd0, card->mmio_base + C2PL2STARTADD0);
//	writel(card->cregs.c2pl2startadd1, card->mmio_base + C2PL2STARTADD1);
	writel(card->cregs.c2pl3startadd0, card->mmio_base + C2PL3STARTADD0);
//	writel(card->cregs.c2pl3startadd1, card->mmio_base + C2PL3STARTADD1);
	writel(card->cregs.c2spicstartadd0, card->mmio_base + C2SPICSTARTADD0);
//	writel(card->cregs.c2spicstartadd1, card->mmio_base + C2SPICSTARTADD1);
//	writel(card->cregs.c2subpiclut, card->mmio_base + C2SUBPICLUT);
//	writel(card->cregs.c2preload, card->mmio_base + C2PRELOAD);
	// finaly enable everything
//	writel(card->cregs.c2ctl,	card->mmio_base + C2CTL);
//	printk("c2ctl:0x%08x c2datactl:0x%08x\n",readl(card->mmio_base + C2CTL),readl(card->mmio_base + C2DATACTL));
//	printk("c2misc:0x%08x\n", readl(card->mmio_base + C2MISC));
#endif	
}

static int mga_vid_set_config(mga_card_t * card)
{
	int x, y, sw, sh, dw, dh;
	int besleft, bestop, ifactor, ofsleft, ofstop, baseadrofs, weight, weights;
	mga_vid_config_t *config = &card->config;
	int frame_size = card->config.frame_size;

#ifdef CRTC2
#define right_margin 0
#define left_margin 18
#define hsync_len 46
#define lower_margin 10
#define vsync_len 4
#define upper_margin 39

	unsigned int hdispend = (config->src_width + 31) & ~31;
	unsigned int hsyncstart = hdispend + (right_margin & ~7);
	unsigned int hsyncend = hsyncstart + (hsync_len & ~7);
	unsigned int htotal = hsyncend + (left_margin & ~7);
	unsigned int vdispend = config->src_height;
	unsigned int vsyncstart = vdispend + lower_margin;
	unsigned int vsyncend = vsyncstart + vsync_len;
	unsigned int vtotal = vsyncend + upper_margin;
#endif 
	x = config->x_org;
	y = config->y_org;
	sw = config->src_width;
	sh = config->src_height;
	dw = config->dest_width;
	dh = config->dest_height;

#ifdef MP_DEBUG
	printk(KERN_DEBUG "mga_vid: Setting up a %dx%d+%d+%d video window (src %dx%d) format %X\n",
	       dw, dh, x, y, sw, sh, config->format);
#endif	

	if(sw<4 || sh<4 || dw<4 || dh<4){
	    printk(KERN_ERR "mga_vid: Invalid src/dest dimenstions\n");
	    return -1;
	}

	//FIXME check that window is valid and inside desktop
	
	//Setup the BES registers for a three plane 4:2:0 video source 

	card->regs.besglobctl = 0;

switch(config->format){
    case MGA_VID_FORMAT_YV12:	
    case MGA_VID_FORMAT_I420:	
    case MGA_VID_FORMAT_IYUV:	
	card->regs.besctl = 1   // BES enabled
                    + (0<<6)    // even start polarity
                    + (1<<10)   // x filtering enabled
                    + (1<<11)   // y filtering enabled
                    + (1<<16)   // chroma upsampling
                    + (1<<17)   // 4:2:0 mode
                    + (1<<18);  // dither enabled
#if 0
	if(card->is_g400)
	{
		//zoom disabled, zoom filter disabled, 420 3 plane format, proc amp
		//disabled, rgb mode disabled 
		card->regs.besglobctl = (1<<5);
	}
	else
	{
		//zoom disabled, zoom filter disabled, Cb samples in 0246, Cr
		//in 1357, BES register update on besvcnt
	        card->regs.besglobctl = 0;
	}
#endif
        break;

    case MGA_VID_FORMAT_YUY2:	
	card->regs.besctl = 1   // BES enabled
                    + (0<<6)    // even start polarity
                    + (1<<10)   // x filtering enabled
                    + (1<<11)   // y filtering enabled
                    + (1<<16)   // chroma upsampling
                    + (0<<17)   // 4:2:2 mode
                    + (1<<18);  // dither enabled

	card->regs.besglobctl = 0;        // YUY2 format selected
        break;

    case MGA_VID_FORMAT_UYVY:	
	card->regs.besctl = 1   // BES enabled
                    + (0<<6)    // even start polarity
                    + (1<<10)   // x filtering enabled
                    + (1<<11)   // y filtering enabled
                    + (1<<16)   // chroma upsampling
                    + (0<<17)   // 4:2:2 mode
                    + (1<<18);  // dither enabled

	card->regs.besglobctl = 1<<6;        // UYVY format selected
        break;

    default:
	printk(KERN_ERR "mga_vid: Unsupported pixel format: 0x%X\n",config->format);
	return -1;
}

	// setting black&white mode 
	card->regs.besctl|=(card->regs.blackie<<20); 

	//Enable contrast and brightness control
	card->regs.besglobctl |= (1<<5) + (1<<7);
	
	// brightness (-128..127) && contrast (0..255)
	card->regs.beslumactl = (card->brightness << 16) | ((card->contrast+0x80)&0xFFFF);

	//Setup destination window boundaries
	besleft = x > 0 ? x : 0;
	bestop = y > 0 ? y : 0;
	card->regs.beshcoord = (besleft<<16) + (x + dw-1);
	card->regs.besvcoord = (bestop<<16) + (y + dh-1);
	
	//Setup source dimensions
	card->regs.beshsrclst  = (sw - 1) << 16;
	card->regs.bespitch = (sw + 31) & ~31 ; 
	
	//Setup horizontal scaling
	ifactor = ((sw-1)<<14)/(dw-1);
	ofsleft = besleft - x;
		
	card->regs.beshiscal = ifactor<<2;
	card->regs.beshsrcst = (ofsleft*ifactor)<<2;
	card->regs.beshsrcend = card->regs.beshsrcst + (((dw - ofsleft - 1) * ifactor) << 2);
	
	//Setup vertical scaling
	ifactor = ((sh-1)<<14)/(dh-1);
	ofstop = bestop - y;

	card->regs.besviscal = ifactor<<2;

	baseadrofs = ( (ofstop * card->regs.besviscal) >>16) * card->regs.bespitch;
	//frame_size = ((sw + 31) & ~31) * sh + (((sw + 31) & ~31) * sh) / 2;
	card->regs.besa1org = (uint32_t) card->src_base + baseadrofs;
	card->regs.besa2org = (uint32_t) card->src_base + baseadrofs + 1*frame_size;
	card->regs.besb1org = (uint32_t) card->src_base + baseadrofs + 2*frame_size;
	card->regs.besb2org = (uint32_t) card->src_base + baseadrofs + 3*frame_size;

if(config->format==MGA_VID_FORMAT_YV12
 ||config->format==MGA_VID_FORMAT_IYUV
 ||config->format==MGA_VID_FORMAT_I420
 ){
        // planar YUV frames:
	if (card->is_g400) 
		baseadrofs = ( ( (ofstop * card->regs.besviscal ) / 4 ) >> 16 ) * card->regs.bespitch;
	else 
		baseadrofs = ( ( ( ofstop * card->regs.besviscal ) / 2 ) >> 16 ) * card->regs.bespitch;

    if(config->format==MGA_VID_FORMAT_YV12 || !card->is_g400){
	card->regs.besa1corg = (uint32_t) card->src_base + baseadrofs + card->regs.bespitch * sh ;
	card->regs.besa2corg = (uint32_t) card->src_base + baseadrofs + 1*frame_size + card->regs.bespitch * sh;
	card->regs.besb1corg = (uint32_t) card->src_base + baseadrofs + 2*frame_size + card->regs.bespitch * sh;
	card->regs.besb2corg = (uint32_t) card->src_base + baseadrofs + 3*frame_size + card->regs.bespitch * sh;
	card->regs.besa1c3org = card->regs.besa1corg + ( (card->regs.bespitch * sh) / 4);
	card->regs.besa2c3org = card->regs.besa2corg + ( (card->regs.bespitch * sh) / 4);
	card->regs.besb1c3org = card->regs.besb1corg + ( (card->regs.bespitch * sh) / 4);
	card->regs.besb2c3org = card->regs.besb2corg + ( (card->regs.bespitch * sh) / 4);
    } else {
	card->regs.besa1c3org = (uint32_t) card->src_base + baseadrofs + card->regs.bespitch * sh ;
	card->regs.besa2c3org = (uint32_t) card->src_base + baseadrofs + 1*frame_size + card->regs.bespitch * sh;
	card->regs.besb1c3org = (uint32_t) card->src_base + baseadrofs + 2*frame_size + card->regs.bespitch * sh;
	card->regs.besb2c3org = (uint32_t) card->src_base + baseadrofs + 3*frame_size + card->regs.bespitch * sh;
	card->regs.besa1corg = card->regs.besa1c3org + ((card->regs.bespitch * sh) / 4);
	card->regs.besa2corg = card->regs.besa2c3org + ((card->regs.bespitch * sh) / 4);
	card->regs.besb1corg = card->regs.besb1c3org + ((card->regs.bespitch * sh) / 4);
	card->regs.besb2corg = card->regs.besb2c3org + ((card->regs.bespitch * sh) / 4);
    }

}

	weight = ofstop * (card->regs.besviscal >> 2);
	weights = weight < 0 ? 1 : 0;
	card->regs.besv2wght = card->regs.besv1wght = (weights << 16) + ((weight & 0x3FFF) << 2);
	card->regs.besv2srclst = card->regs.besv1srclst = sh - 1 - (((ofstop * card->regs.besviscal) >> 16) & 0x03FF);

#ifdef CRTC2
	// pridat hlavni registry - tj. casovani ...


switch(config->format){
    case MGA_VID_FORMAT_YV12:	
    case MGA_VID_FORMAT_I420:	
    case MGA_VID_FORMAT_IYUV:	
	card->cregs.c2ctl = 1   // CRTC2 enabled
		    + (1<<1)	// external clock
		    + (0<<2)	// external clock
		    + (1<<3)	// pixel clock enable - not needed ???
		    + (0<<4)	// high prioryty req
		    + (1<<5)	// high prioryty req
		    + (0<<6)	// high prioryty req
		    + (1<<8)	// high prioryty req max
		    + (0<<9)	// high prioryty req max
		    + (0<<10)	// high prioryty req max
                    + (0<<20)   // CRTC1 to DAC
                    + (1<<21)   // 420 mode
                    + (1<<22)   // 420 mode
                    + (1<<23)   // 420 mode
                    + (0<<24)   // single chroma line for 420 mode - need to be corrected
                    + (0<<25)   /*/ interlace mode - need to be corrected*/
                    + (0<<26)   // field legth polariry
                    + (0<<27)   // field identification polariry
                    + (1<<28)   // VIDRST detection mode
                    + (0<<29)   // VIDRST detection mode
                    + (1<<30)   // Horizontal counter preload
                    + (1<<31)   // Vertical counter preload
		    ;
	card->cregs.c2datactl = 1 // disable dither - propably not needed, we are already in YUV mode
		    + (1<<1)	// Y filter enable
		    + (1<<2)	// CbCr filter enable
		    + (0<<3)	// subpicture enable (disabled)
		    + (0<<4)	// NTSC enable (disabled - PAL)
		    + (0<<5)	// C2 static subpicture enable (disabled)
		    + (0<<6)	// C2 subpicture offset division (disabled)
		    + (0<<7)	// 422 subformat selection !
/*		    + (0<<8)	// 15 bpp high alpha
		    + (0<<9)	// 15 bpp high alpha
		    + (0<<10)	// 15 bpp high alpha
		    + (0<<11)	// 15 bpp high alpha
		    + (0<<12)	// 15 bpp high alpha
		    + (0<<13)	// 15 bpp high alpha
		    + (0<<14)	// 15 bpp high alpha
		    + (0<<15)	// 15 bpp high alpha
		    + (0<<16)	// 15 bpp low alpha
		    + (0<<17)	// 15 bpp low alpha
		    + (0<<18)	// 15 bpp low alpha
		    + (0<<19)	// 15 bpp low alpha
		    + (0<<20)	// 15 bpp low alpha
		    + (0<<21)	// 15 bpp low alpha
		    + (0<<22)	// 15 bpp low alpha
		    + (0<<23)	// 15 bpp low alpha
		    + (0<<24)	// static subpicture key
		    + (0<<25)	// static subpicture key
		    + (0<<26)	// static subpicture key
		    + (0<<27)	// static subpicture key
		    + (0<<28)	// static subpicture key
*/		    ;
        break;

    case MGA_VID_FORMAT_YUY2:	
	card->cregs.c2ctl = 1   // CRTC2 enabled
		    + (1<<1)	// external clock
		    + (0<<2)	// external clock
		    + (1<<3)	// pixel clock enable - not needed ???
		    + (0<<4)	// high prioryty req - acc to spec
		    + (1<<5)	// high prioryty req
		    + (0<<6)	// high prioryty req
				// 7 reserved
		    + (1<<8)	// high prioryty req max
		    + (0<<9)	// high prioryty req max
		    + (0<<10)	// high prioryty req max
				// 11-19 reserved
                    + (0<<20)   // CRTC1 to DAC
                    + (1<<21)   // 422 mode
                    + (0<<22)   // 422 mode
                    + (1<<23)   // 422 mode
                    + (0<<24)   // single chroma line for 420 mode - need to be corrected
                    + (0<<25)   /*/ interlace mode - need to be corrected*/
                    + (0<<26)   // field legth polariry
                    + (0<<27)   // field identification polariry
                    + (1<<28)   // VIDRST detection mode
                    + (0<<29)   // VIDRST detection mode
                    + (1<<30)   // Horizontal counter preload
                    + (1<<31)   // Vertical counter preload
		    ;
	card->cregs.c2datactl = 1 // disable dither - propably not needed, we are already in YUV mode
		    + (1<<1)	// Y filter enable
		    + (1<<2)	// CbCr filter enable
		    + (0<<3)	// subpicture enable (disabled)
		    + (0<<4)	// NTSC enable (disabled - PAL)
		    + (0<<5)	// C2 static subpicture enable (disabled)
		    + (0<<6)	// C2 subpicture offset division (disabled)
		    + (0<<7)	// 422 subformat selection !
/*		    + (0<<8)	// 15 bpp high alpha
		    + (0<<9)	// 15 bpp high alpha
		    + (0<<10)	// 15 bpp high alpha
		    + (0<<11)	// 15 bpp high alpha
		    + (0<<12)	// 15 bpp high alpha
		    + (0<<13)	// 15 bpp high alpha
		    + (0<<14)	// 15 bpp high alpha
		    + (0<<15)	// 15 bpp high alpha
		    + (0<<16)	// 15 bpp low alpha
		    + (0<<17)	// 15 bpp low alpha
		    + (0<<18)	// 15 bpp low alpha
		    + (0<<19)	// 15 bpp low alpha
		    + (0<<20)	// 15 bpp low alpha
		    + (0<<21)	// 15 bpp low alpha
		    + (0<<22)	// 15 bpp low alpha
		    + (0<<23)	// 15 bpp low alpha
		    + (0<<24)	// static subpicture key
		    + (0<<25)	// static subpicture key
		    + (0<<26)	// static subpicture key
		    + (0<<27)	// static subpicture key
		    + (0<<28)	// static subpicture key
*/			;
          break;

    case MGA_VID_FORMAT_UYVY:	
	card->cregs.c2ctl = 1   // CRTC2 enabled
		    + (1<<1)	// external clock
		    + (0<<2)	// external clock
		    + (1<<3)	// pixel clock enable - not needed ???
		    + (0<<4)	// high prioryty req
		    + (1<<5)	// high prioryty req
		    + (0<<6)	// high prioryty req
		    + (1<<8)	// high prioryty req max
		    + (0<<9)	// high prioryty req max
		    + (0<<10)	// high prioryty req max
                    + (0<<20)   // CRTC1 to DAC
                    + (1<<21)   // 422 mode
                    + (0<<22)   // 422 mode
                    + (1<<23)   // 422 mode
                    + (1<<24)   // single chroma line for 420 mode - need to be corrected
                    + (1<<25)   /*/ interlace mode - need to be corrected*/
                    + (0<<26)   // field legth polariry
                    + (0<<27)   // field identification polariry
                    + (1<<28)   // VIDRST detection mode
                    + (0<<29)   // VIDRST detection mode
                    + (1<<30)   // Horizontal counter preload
                    + (1<<31)   // Vertical counter preload
		    ;
	card->cregs.c2datactl = 0 // enable dither - propably not needed, we are already in YUV mode
		    + (1<<1)	// Y filter enable
		    + (1<<2)	// CbCr filter enable
		    + (0<<3)	// subpicture enable (disabled)
		    + (0<<4)	// NTSC enable (disabled - PAL)
		    + (0<<5)	// C2 static subpicture enable (disabled)
		    + (0<<6)	// C2 subpicture offset division (disabled)
		    + (1<<7)	// 422 subformat selection !
/*		    + (0<<8)	// 15 bpp high alpha
		    + (0<<9)	// 15 bpp high alpha
		    + (0<<10)	// 15 bpp high alpha
		    + (0<<11)	// 15 bpp high alpha
		    + (0<<12)	// 15 bpp high alpha
		    + (0<<13)	// 15 bpp high alpha
		    + (0<<14)	// 15 bpp high alpha
		    + (0<<15)	// 15 bpp high alpha
		    + (0<<16)	// 15 bpp low alpha
		    + (0<<17)	// 15 bpp low alpha
		    + (0<<18)	// 15 bpp low alpha
		    + (0<<19)	// 15 bpp low alpha
		    + (0<<20)	// 15 bpp low alpha
		    + (0<<21)	// 15 bpp low alpha
		    + (0<<22)	// 15 bpp low alpha
		    + (0<<23)	// 15 bpp low alpha
		    + (0<<24)	// static subpicture key
		    + (0<<25)	// static subpicture key
		    + (0<<26)	// static subpicture key
		    + (0<<27)	// static subpicture key
		    + (0<<28)	// static subpicture key
*/		    ;
        break;

    default:
	printk(KERN_ERR "mga_vid: Unsupported pixel format: 0x%X\n",config->format);
	return -1;
    }

	card->cregs.c2hparam = ( (hdispend - 8) << 16) | (htotal - 8);
	card->cregs.c2hsync = ( (hsyncend - 8) << 16) | (hsyncstart - 8);
	
	card->cregs.c2misc=0	// CRTCV2 656 togg f0
		    +(0<<1) // CRTCV2 656 togg f0
		    +(0<<2) // CRTCV2 656 togg f0
		    +(0<<4) // CRTCV2 656 togg f1
		    +(0<<5) // CRTCV2 656 togg f1
		    +(0<<6) // CRTCV2 656 togg f1
		    +(0<<8) // Hsync active high
		    +(0<<9) // Vsync active high
		    // 16-27 c2vlinecomp - nevim co tam dat
		    ;
	card->cregs.c2offset=(card->regs.bespitch << 1);

	card->cregs.c2pl2startadd0=card->regs.besa1corg;
//	card->cregs.c2pl2startadd1=card->regs.besa2corg;
	card->cregs.c2pl3startadd0=card->regs.besa1c3org;
//	card->cregs.c2pl3startadd1=card->regs.besa2c3org;
		    
	card->cregs.c2preload=(vsyncstart << 16) | (hsyncstart); // from 
	
	card->cregs.c2spicstartadd0=0; // not used
//	card->cregs.c2spicstartadd1=0; // not used
	
	card->cregs.c2startadd0=card->regs.besa1org;
//	card->cregs.c2startadd1=card->regs.besa2org;
	
	card->cregs.c2subpiclut=0; //not used
	
	card->cregs.c2vparam = ( (vdispend - 1) << 16) | (vtotal - 1);
	card->cregs.c2vsync = ( (vsyncend - 1) << 16) | (vsyncstart - 1);

	
#endif

	mga_vid_write_regs(card, 0);
	return 0;
}

#ifdef MGA_ALLOW_IRQ

static void enable_irq(mga_card_t * card){
	long int cc;

	cc = readl(card->mmio_base + IEN);
//	printk(KERN_ALERT "*** !!! IRQREG = %d\n", (int)(cc&0xff));

	writeb(0x11, card->mmio_base + CRTCX);
	
	writeb(0x20, card->mmio_base + CRTCD);  /* clear 0, enable off */
	writeb(0x00, card->mmio_base + CRTCD);  /* enable on */
	writeb(0x10, card->mmio_base + CRTCD);  /* clear = 1 */
	
	writel(card->regs.besglobctl , card->mmio_base + BESGLOBCTL);

}

static void disable_irq(mga_card_t * card){

	writeb(0x11, card->mmio_base + CRTCX);
	writeb(0x20, card->mmio_base + CRTCD);  /* clear 0, enable off */

}

static void mga_handle_irq(int irq, void *dev_id, struct pt_regs *pregs) {
//	static int frame=0;
//	static int counter=0;
	long int cc;
	mga_card_t * card = dev_id;

//	printk(KERN_DEBUG "vcount = %d\n",readl(mga_mmio_base + VCOUNT));

	//printk("mga_interrupt #%d\n", irq);

	// check whether the interrupt is really for us (irq sharing)
	if ( irq != -1 ) {
		cc = readl(card->mmio_base + STATUS);
		if ( ! (cc & 0x10) ) return;  /* vsyncpen */
// 		debug_irqcnt++;
	} 

//    if ( debug_irqignore ) {
//	debug_irqignore = 0;

//	frame=(frame+1)&1;
	card->regs.besctl = (card->regs.besctl & ~0x07000000) + (card->next_frame << 25);
	writel( card->regs.besctl, card->mmio_base + BESCTL ); 

#ifdef CRTC2
// sem pridat vyber obrazku !!!!	
// i han echt kei ahnig was das obe heisse söll
	crtc2_frame_sel(card->next_frame);
#endif
	
#if 0
	++counter;
	if(!(counter&63)){
	    printk("mga irq counter = %d\n",counter);
	}
#endif

//    } else {
//	debug_irqignore = 1;
//    }

	if ( irq != -1 ) {
		writeb( 0x11, card->mmio_base + CRTCX);
		writeb( 0, card->mmio_base + CRTCD );
		writeb( 0x10, card->mmio_base + CRTCD );
	}

//	writel( card->regs.besglobctl, card->mmio_base + BESGLOBCTL);


	return;

}

#endif

static int mga_vid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int frame, result;
	uint32_t tmp;
	mga_card_t * card = (mga_card_t *) file->private_data;

	switch(cmd) 
	{
		case MGA_VID_GET_VERSION:
			tmp = MGA_VID_VERSION;
			if (copy_to_user((uint32_t *) arg, &tmp, sizeof(uint32_t))) {
				printk(KERN_ERR "mga_vid: failed copy %p to userspace %p\n", &tmp, (uint32_t *) arg);
				return (-EFAULT);
			}
			break;

		case MGA_VID_CONFIG:
			//FIXME remove
//			printk(KERN_DEBUG "mga_vid: vcount = %d\n",readl(card->mmio_base + VCOUNT));
#ifdef MP_DEBUG
			printk(KERN_DEBUG "mga_vid: mmio_base = %p\n",card->mmio_base);
			printk(KERN_DEBUG "mga_vid: mem_base = %08x\n",card->mem_base);
			//FIXME remove

			printk(KERN_DEBUG "mga_vid: Received configuration\n");
#endif			

 			if(copy_from_user(&card->config,(mga_vid_config_t*) arg,sizeof(mga_vid_config_t)))
			{
				printk(KERN_ERR "mga_vid: failed copy from userspace\n");
				return -EFAULT;
			}
			if(card->config.version != MGA_VID_VERSION){
				printk(KERN_ERR "mga_vid: incompatible version! driver: %X  requested: %X\n",MGA_VID_VERSION,card->config.version);
				return -EFAULT;
			}

			if(card->config.frame_size==0 || card->config.frame_size>1024*768*2){
				printk(KERN_ERR "mga_vid: illegal frame_size: %d\n",card->config.frame_size);
				return -EFAULT;
			}

			if(card->config.num_frames<1 || card->config.num_frames>4){
				printk(KERN_ERR "mga_vid: illegal num_frames: %d\n",card->config.num_frames);
				return -EFAULT;
			}
			
			card->src_base = (card->ram_size * 0x100000 - card->config.num_frames * card->config.frame_size - card->top_reserved);
			if(card->src_base<0){
				printk(KERN_ERR "mga_vid: not enough memory for frames!\n");
				return -EFAULT;
			}
			card->src_base &= (~0xFFFF); // 64k boundary
#ifdef MP_DEBUG
			printk(KERN_DEBUG "mga YUV buffer base: 0x%X\n", card->src_base);
#endif			
			
			if (card->is_g400) 
			  card->config.card_type = MGA_G400;
			else
			  card->config.card_type = MGA_G200;
		       
			card->config.ram_size = card->ram_size;

			if (copy_to_user((mga_vid_config_t *) arg, &card->config, sizeof(mga_vid_config_t)))
			{
				printk(KERN_ERR "mga_vid: failed copy to userspace\n");
				return -EFAULT;
			}

			result = mga_vid_set_config(card);	
			if(!result) card->configured=1;
			return result;
		break;

		case MGA_VID_ON:
#ifdef MP_DEBUG
			printk(KERN_DEBUG "mga_vid: Video ON\n");
#endif			
			card->vid_src_ready = 1;
			if(card->vid_overlay_on)
			{
				card->regs.besctl |= 1;
				mga_vid_write_regs(card, 0);
			}
#ifdef MGA_ALLOW_IRQ
			if ( card->irq != -1 ) enable_irq(card);
#endif
			card->next_frame=0;
		break;

		case MGA_VID_OFF:
#ifdef MP_DEBUG
			printk(KERN_DEBUG "mga_vid: Video OFF (ioctl)\n");
#endif			
			card->vid_src_ready = 0;   
#ifdef MGA_ALLOW_IRQ
			if ( card->irq != -1 ) disable_irq(card);
#endif
			card->regs.besctl &= ~1;
                        card->regs.besglobctl &= ~(1<<6);  // UYVY format selected
			mga_vid_write_regs(card, 0);
		break;
			
		case MGA_VID_FSEL:
			if(copy_from_user(&frame,(int *) arg,sizeof(int)))
			{
				printk(KERN_ERR "mga_vid: FSEL failed copy from userspace\n");
				return -EFAULT;
			}

			mga_vid_frame_sel(card, frame);
		break;

		case MGA_VID_GET_LUMA:
			//tmp = card->regs.beslumactl;
			//tmp = (tmp&0xFFFF0000) | (((tmp&0xFFFF) - 0x80)&0xFFFF);
			tmp = (card->brightness << 16) | (card->contrast&0xFFFF);

			if (copy_to_user((uint32_t *) arg, &tmp, sizeof(uint32_t)))
			{
				printk(KERN_ERR "mga_vid: failed copy %p to userspace %p\n",
					   &tmp, (uint32_t *) arg);
				return -EFAULT;
			}
		break;
			
		case MGA_VID_SET_LUMA:
			tmp = arg;
			card->brightness=tmp>>16; card->contrast=tmp&0xFFFF;
			//card->regs.beslumactl = (tmp&0xFFFF0000) | ((tmp + 0x80)&0xFFFF);
			card->regs.beslumactl = (card->brightness << 16) | ((card->contrast+0x80)&0xFFFF);
			mga_vid_write_regs(card, 0);
		break;
			
	        default:
			printk(KERN_ERR "mga_vid: Invalid ioctl\n");
			return -EINVAL;
	}
       
	return 0;
}

static void cards_init(mga_card_t * card, struct pci_dev * dev, int card_number, int is_g400);

// returns the number of found cards
static int mga_vid_find_card(void)
{
	struct pci_dev *dev = NULL;
	char *mga_dev_name;
	mga_card_t * card;

	while((dev = pci_find_device(PCI_VENDOR_ID_MATROX, PCI_ANY_ID, dev)))
	{
		mga_dev_name = "";
		mga_cards_num++;
		if(mga_cards_num == MGA_MAX_CARDS)
		{
			printk(KERN_WARNING "mga_vid: Trying to initialize more than %d cards\n",MGA_MAX_CARDS);
			mga_cards_num--;
			break;
		}

		card = kmalloc(sizeof(mga_card_t), GFP_KERNEL);
		if(!card) 
		{ 
			printk(KERN_ERR "mga_vid: memory allocation failed\n");
			mga_cards_num--;
			break;
		}
		
		mga_cards[mga_cards_num - 1] = card;
		
		switch(dev->device) {
		case PCI_DEVICE_ID_MATROX_G550:
			mga_dev_name = "MGA G550";
			printk(KERN_INFO "mga_vid: Found %s at %s [%s]\n", mga_dev_name, dev->slot_name, dev->name);
			cards_init(card, dev, mga_cards_num - 1, 1);
			break;
		case PCI_DEVICE_ID_MATROX_G400:
			mga_dev_name = "MGA G400/G450";
			printk(KERN_INFO "mga_vid: Found %s at %s [%s]\n", mga_dev_name, dev->slot_name, dev->name);
			cards_init(card, dev, mga_cards_num - 1, 1);
			break;
		case PCI_DEVICE_ID_MATROX_G200_AGP:
			mga_dev_name = "MGA G200 AGP";
			printk(KERN_INFO "mga_vid: Found %s at %s [%s]\n", mga_dev_name, dev->slot_name, dev->name);
			cards_init(card, dev, mga_cards_num - 1, 0);
			break;
		case PCI_DEVICE_ID_MATROX_G200_PCI:
			mga_dev_name = "MGA G200";
			printk(KERN_INFO "mga_vid: Found %s at %s [%s]\n", mga_dev_name, dev->slot_name, dev->name);
			cards_init(card, dev, mga_cards_num - 1, 0);
			break;
		default:
			mga_cards_num--;
			printk(KERN_INFO "mga_vid: ignoring matrox device (%d) at %s [%s]\n", dev->device, dev->slot_name, dev->name);
			break;
		}
	}
 	
	if(!mga_cards_num)
	{
		printk(KERN_ERR "mga_vid: No supported cards found\n");
	} else {
		printk(KERN_INFO "mga_vid: %d supported cards found\n", mga_cards_num);
	}
	
	return mga_cards_num;
}

static void mga_param_buff_fill( mga_card_t * card )
{
    unsigned len;
    unsigned size = card->param_buff_size;
    char * buf = card->param_buff;
    len = 0;
    len += snprintf(&buf[len],size-len,"Interface version: %04X\n",MGA_VID_VERSION);
    len += snprintf(&buf[len],size-len,"Memory: %x:%dM\n",card->mem_base,(unsigned int) card->ram_size);
    len += snprintf(&buf[len],size-len,"MMIO: %p\n",card->mmio_base);
    len += snprintf(&buf[len],size-len,"Configurable stuff:\n");
    len += snprintf(&buf[len],size-len,"~~~~~~~~~~~~~~~~~~~\n");
    len += snprintf(&buf[len],size-len,PARAM_BRIGHTNESS"%d\n",card->brightness);
    len += snprintf(&buf[len],size-len,PARAM_CONTRAST"%d\n",card->contrast);
    len += snprintf(&buf[len],size-len,PARAM_BLACKIE"%s\n",card->regs.blackie?"on":"off");
    card->param_buff_len = len;
    // check boundaries of mga_param_buff before writing to it!!!
}


static ssize_t mga_vid_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	uint32_t size;
	mga_card_t * card = (mga_card_t *) file->private_data;
	
	if(!card->param_buff) return -ESPIPE;
	if(!(*ppos)) mga_param_buff_fill(card);
	if(*ppos >= card->param_buff_len) return 0;
	size = min(count,card->param_buff_len-(uint32_t)(*ppos));
	memcpy(buf,card->param_buff,size);
	*ppos += size;
	return size;
}

static ssize_t mga_vid_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	mga_card_t * card = (mga_card_t *) file->private_data;

	if(memcmp(buf,PARAM_BRIGHTNESS,min(count,strlen(PARAM_BRIGHTNESS))) == 0)
	{
		short brightness;
		brightness=simple_strtol(&buf[strlen(PARAM_BRIGHTNESS)],NULL,10);
		if (brightness>127 || brightness<-128) { brightness=0;} 
//		printk(KERN_DEBUG "mga_vid: brightness modified ( %d ) \n",brightness);
		card->brightness=brightness;
	} else 
	if(memcmp(buf,PARAM_CONTRAST,min(count,strlen(PARAM_CONTRAST))) == 0)
	{
		short contrast;
		contrast=simple_strtol(&buf[strlen(PARAM_CONTRAST)],NULL,10);
		if (contrast>127 || contrast<-128) { contrast=0;} 
//		printk(KERN_DEBUG "mga_vid: contrast modified ( %d ) \n",contrast);
		card->contrast=contrast;
	} else 

        if(memcmp(buf,PARAM_BLACKIE,min(count,strlen(PARAM_BLACKIE))) == 0)
	{
		short blackie;
		blackie=simple_strtol(&buf[strlen(PARAM_BLACKIE)],NULL,10);
//		printk(KERN_DEBUG "mga_vid: shadow mode: ( %d ) \n",blackie);
		card->regs.blackie=(blackie>0)?1:0;
	} else count = -EIO;
	// TODO: reset settings
	return count;
}

static int mga_vid_mmap(struct file *file, struct vm_area_struct *vma)
{
	mga_card_t * card = (mga_card_t *) file->private_data;

#ifdef MP_DEBUG
	printk(KERN_DEBUG "mga_vid: mapping video memory into userspace\n");
#endif	

	if(!card->configured)
	{
		printk(KERN_ERR "mga_vid: card is not configured, cannot mmap\n");
		return -EAGAIN;
	}

	if(remap_page_range(vma->vm_start, card->mem_base + card->src_base,
		 vma->vm_end - vma->vm_start, vma->vm_page_prot)) 
	{
		printk(KERN_ERR "mga_vid: error mapping video memory\n");
		return -EAGAIN;
	}

	return 0;
}

static int mga_vid_release(struct inode *inode, struct file *file)
{
	mga_card_t * card;

	//Close the window just in case
#ifdef MP_DEBUG
	printk(KERN_DEBUG "mga_vid: Video OFF (release)\n");
#endif	

	card = (mga_card_t *) file->private_data;

	card->vid_src_ready = 0;   
	card->regs.besctl &= ~1;
        card->regs.besglobctl &= ~(1<<6);  // UYVY format selected
//	card->config.colkey_on=0; //!!!
	mga_vid_write_regs(card, 1);
	card->vid_in_use = 0;

	MOD_DEC_USE_COUNT;
	return 0;
}

static long long mga_vid_lseek(struct file *file, long long offset, int origin)
{
	return -ESPIPE;
}					 

static int mga_vid_open(struct inode *inode, struct file *file)
{
	mga_card_t * card;
	
	int minor = MINOR(inode->i_rdev);

	if(!file->private_data)
	{
		// we are not using devfs, use the minor
		// number to specify the card we are using

		// we don't have that many cards
		if(minor >= mga_cards_num)
		 return -ENXIO;

		file->private_data = mga_cards[minor];
#ifdef MP_DEBUG
		printk(KERN_DEBUG "mga_vid: Not using devfs\n");
#endif	
	}
#ifdef MP_DEBUG
	  else {
		printk(KERN_DEBUG "mga_vid: Using devfs\n");
	}
#endif	

	card = (mga_card_t *) file->private_data;

	if(card->vid_in_use == 1) 
		return -EBUSY;

	card->vid_in_use = 1;
	MOD_INC_USE_COUNT;
	return 0;
}

#if LINUX_VERSION_CODE >= 0x020400
static struct file_operations mga_vid_fops =
{
	llseek:		mga_vid_lseek,
	read:			mga_vid_read,
	write:		mga_vid_write,
	ioctl:		mga_vid_ioctl,
	mmap:			mga_vid_mmap,
	open:			mga_vid_open,
	release: 	mga_vid_release
};
#else
static struct file_operations mga_vid_fops =
{
	mga_vid_lseek,
	mga_vid_read,
	mga_vid_write,
	NULL,
	NULL,
	mga_vid_ioctl,
	mga_vid_mmap,
	mga_vid_open,
	NULL,
	mga_vid_release
};
#endif

static void cards_init(mga_card_t * card, struct pci_dev * dev, int card_number, int is_g400)
{
	unsigned int card_option;
// temp buffer for device filename creation used only by devfs
#ifdef CONFIG_DEVFS_FS
	char buffer[16];
#endif

	memset(card,0,sizeof(mga_card_t));
	card->irq = -1;

	card->pci_dev = dev;
	card->irq = dev->irq;
	card->is_g400 = is_g400;

	card->param_buff = kmalloc(PARAM_BUFF_SIZE,GFP_KERNEL);
	if(card->param_buff) card->param_buff_size = PARAM_BUFF_SIZE;

	card->brightness = mga_brightness[card_number];
	card->contrast = mga_contrast[card_number];
	card->top_reserved = mga_top_reserved[card_number];
	
#if LINUX_VERSION_CODE >= 0x020300
	card->mmio_base = ioremap_nocache(dev->resource[1].start,0x4000);
	card->mem_base =  dev->resource[0].start;
#else
	card->mmio_base = ioremap_nocache(dev->base_address[1] & PCI_BASE_ADDRESS_MEM_MASK,0x4000);
	card->mem_base =  dev->base_address[0] & PCI_BASE_ADDRESS_MEM_MASK;
#endif
	printk(KERN_INFO "mga_vid: MMIO at 0x%p IRQ: %d  framebuffer: 0x%08X\n", card->mmio_base, card->irq, card->mem_base);

	pci_read_config_dword(dev,  0x40, &card_option);
	printk(KERN_INFO "mga_vid: OPTION word: 0x%08X  mem: 0x%02X  %s\n", card_option,
		(card_option>>10)&0x17, ((card_option>>14)&1)?"SGRAM":"SDRAM");

	if (mga_ram_size[card_number]) {
		printk(KERN_INFO "mga_vid: RAMSIZE forced to %d MB\n", mga_ram_size[card_number]);
		card->ram_size=mga_ram_size[card_number];
	} else {

#ifdef MGA_MEMORY_SIZE
	    card->ram_size = MGA_MEMORY_SIZE;
	    printk(KERN_INFO "mga_vid: hard-coded RAMSIZE is %d MB\n", (unsigned int) card->ram_size);

#else

	    if (card->is_g400){
		switch((card_option>>10)&0x17){
		    // SDRAM:
		    case 0x00:
		    case 0x04:  card->ram_size = 16; break;
		    case 0x03:  
		    case 0x05:  card->ram_size = 32; break;
		    // SGRAM:
		    case 0x10:
		    case 0x14:  card->ram_size = 32; break;
		    case 0x11:
		    case 0x12:  card->ram_size = 16; break;
		    default:
			card->ram_size = 16;
			printk(KERN_INFO "mga_vid: Couldn't detect RAMSIZE, assuming 16MB!");
		}
		/* Check for buggy 16MB cards reporting 32 MB */
		if(card->ram_size != 16 &&
		   (dev->subsystem_device == PCI_SUBSYSTEM_ID_MATROX_G400_16MB_SDRAM ||
		    dev->subsystem_device == PCI_SUBSYSTEM_ID_MATROX_G400_16MB_SGRAM ||
		    dev->subsystem_device == PCI_SUBSYSTEM_ID_MATROX_G400_DH_16MB))
		{
		    printk(KERN_INFO "mga_vid: Detected 16MB card reporting %d MB RAMSIZE, overriding\n", card->ram_size);
		    card->ram_size = 16;
		}
	    }else{
		switch((card_option>>10)&0x17){
//		    case 0x10:
//		    case 0x13:  card->ram_size = 8; break;
		    default: card->ram_size = 8;
		}
	    } 
#if 0
//	    printk("List resources -----------\n");
	    for(temp=0;temp<DEVICE_COUNT_RESOURCE;temp++){
	        struct resource *res=&dev->resource[temp];
	        if(res->flags){
	          int size=(1+res->end-res->start)>>20;
	          printk(KERN_DEBUG "res %d:  start: 0x%X   end: 0x%X  (%d MB) flags=0x%X\n",temp,res->start,res->end,size,res->flags);
	          if(res->flags&(IORESOURCE_MEM|IORESOURCE_PREFETCH)){
	              if(size>card->ram_size && size<=64) card->ram_size=size;
	          }
	        }
	    }
#endif
	    printk(KERN_INFO "mga_vid: detected RAMSIZE is %d MB\n", (unsigned int) card->ram_size);
#endif
        }


#ifdef MGA_ALLOW_IRQ
	if ( card->irq != -1 ) {
		int tmp = request_irq(card->irq, mga_handle_irq, SA_INTERRUPT | SA_SHIRQ, "Syncfb Time Base", card);
		if ( tmp ) {
			printk(KERN_INFO "syncfb (mga): cannot register irq %d (Err: %d)\n", card->irq, tmp);
			card->irq=-1;
		} else {
			printk(KERN_DEBUG "syncfb (mga): registered irq %d\n", card->irq);
		}
	} else {
		printk(KERN_INFO "syncfb (mga): No valid irq was found\n");
		card->irq=-1;
	}
#else
		printk(KERN_INFO "syncfb (mga): IRQ disabled in mga_vid.c\n");
		card->irq=-1;
#endif

	// register devfs, let the kernel give us major and minor numbers
#ifdef CONFIG_DEVFS_FS
	snprintf(buffer, 16, "mga_vid%d", card_number);
	card->dev_handle = devfs_register(NULL, buffer, DEVFS_FL_AUTO_DEVNUM,
					0, 0,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IFCHR,
					&mga_vid_fops, card);
#endif

}

/* 
 * Main Initialization Function 
 */

static int mga_vid_initialize(void)
{
 	int i;

//	printk(KERN_INFO "Matrox MGA G200/G400 YUV Video interface v0.01 (c) Aaron Holtzman \n");
	printk(KERN_INFO "Matrox MGA G200/G400/G450/G550 YUV Video interface v2.01 (c) Aaron Holtzman & A'rpi\n");

	for(i = 0; i < MGA_MAX_CARDS; i++)
	{
		if (mga_ram_size[i]) {
			if (mga_ram_size[i]<4 || mga_ram_size[i]>64) {
				printk(KERN_ERR "mga_vid: invalid RAMSIZE: %d MB\n", mga_ram_size[i]);
				return -EINVAL;
			}
		}
	}
	
	if(register_chrdev(major, "mga_vid", &mga_vid_fops))
	{
		printk(KERN_ERR "mga_vid: unable to get major: %d\n", major);
		return -EIO;
	}

	if (!mga_vid_find_card())
	{
		printk(KERN_ERR "mga_vid: no supported devices found\n");
		unregister_chrdev(major, "mga_vid");
		return -EINVAL;
	}
#ifdef CONFIG_DEVFS_FS
	  else {
		// we assume that this always succeedes
		dev_handle = devfs_register(NULL, "mga_vid", DEVFS_FL_AUTO_DEVNUM,
		                            0,0,
		                            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IFCHR,
		                            &mga_vid_fops, mga_cards[0]);
	}
#endif

	return 0;
}

int init_module(void)
{
	return mga_vid_initialize();
}

void cleanup_module(void)
{
	int i;
	mga_card_t * card;

	for (i = 0; i < MGA_MAX_CARDS; i++)
	{
		card = mga_cards[i];
		if(card)
		{
#ifdef MGA_ALLOW_IRQ
			if (card->irq != -1)
				free_irq(card->irq, &(card->irq));
#endif

			if(card->mmio_base)
				iounmap(card->mmio_base);
			if(card->param_buff)
				kfree(card->param_buff);
#ifdef CONFIG_DEVFS_FS
			if(card->dev_handle) devfs_unregister(card->dev_handle);
#endif

			kfree(card);
			mga_cards[i]=NULL;
		}
	}

	//FIXME turn off BES
	printk(KERN_INFO "mga_vid: Cleaning up module\n");
#ifdef CONFIG_DEVFS_FS
	if(dev_handle) devfs_unregister(dev_handle);
#endif
	unregister_chrdev(major, "mga_vid");
}
