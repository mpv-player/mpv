/*
 * VIDIX driver for Matrox chipsets.
 *
 * Copyright (C) 2002 Alex Beregszaszi
 * Original sources from Aaron Holtzman (C) 1999.
 * module skeleton based on gutted agpgart module by Jeff Hartmann
 *   <slicer@ionet.net>
 * YUY2 support and double buffering added by A'rpi/ESP-team
 * brightness/contrast support by Nick Kurshev/Dariush Pietrzak (eyck)
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

/* TODO:
 *   - fix memory size detection (current reading pci userconfig isn't
 *     working as requested - returns the max avail. ram on arch?)
 *   - translate all non-english comments to english
 */

//#define CRTC2

// Set this value, if autodetection fails! (video ram size in megabytes)
//#define MGA_MEMORY_SIZE 16

/* No irq support in userspace implemented yet, do not enable this! */
/* disable irq */
#define MGA_ALLOW_IRQ 0

#define MGA_VSYNC_POS 2

#undef MGA_PCICONFIG_MEMDETECT

#define MGA_DEFAULT_FRAMES 4

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "vidix.h"
#include "fourcc.h"
#include "dha.h"
#include "pci_ids.h"
#include "pci_names.h"

#ifdef __MINGW32__
#define ENOTSUP 134
#endif

#if    !defined(ENOTSUP) && defined(EOPNOTSUPP)
#define ENOTSUP EOPNOTSUPP
#endif

/* from radeon_vid */
#define GETREG(TYPE,PTR,OFFZ)		(*((volatile TYPE*)((PTR)+(OFFZ))))
#define SETREG(TYPE,PTR,OFFZ,VAL)	(*((volatile TYPE*)((PTR)+(OFFZ))))=VAL

#define readb(addr)		GETREG(uint8_t,(uint32_t)(addr),0)
#define writeb(val,addr)	SETREG(uint8_t,(uint32_t)(addr),0,val)
#define readl(addr)		GETREG(uint32_t,(uint32_t)(addr),0)
#define writel(val,addr)	SETREG(uint32_t,(uint32_t)(addr),0,val)

static int mga_verbose = 0;

/* for device detection */
static int probed = 0;
static pciinfo_t pci_info;

/* internal booleans */
static int mga_vid_in_use = 0;
static int is_g400 = 0;
static int vid_src_ready = 0;
static int vid_overlay_on = 0;

/* mapped physical addresses */
static uint8_t *mga_mmio_base = 0;
static uint8_t *mga_mem_base = 0;

static int mga_src_base = 0; /* YUV buffer position in video memory */

static uint32_t mga_ram_size = 0; /* how much megabytes videoram we have */

/* Graphic keys */
static vidix_grkey_t mga_grkey;

static int colkey_saved = 0;
static int colkey_on = 0;
static unsigned char colkey_color[4];
static unsigned char colkey_mask[4];

/* for IRQ */
static int mga_irq = -1;

static int mga_next_frame = 0;

static vidix_capability_t mga_cap =
{
    "Matrox MGA G200/G4x0/G5x0 YUV Video",
    "Aaron Holtzman, Arpad Gereoffy, Alex Beregszaszi, Nick Kurshev",
    TYPE_OUTPUT,
    { 0, 0, 0, 0 },
    2048,
    2048,
    4,
    4,
    -1,
    FLAG_UPSCALER | FLAG_DOWNSCALER | FLAG_EQUALIZER,
    VENDOR_MATROX,
    -1, /* will be set in vixProbe */
    { 0, 0, 0, 0}
};

/* MATROX BES registers */
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

} bes_registers_t;
static bes_registers_t regs;

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
static crtc2_registers_t cregs;
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
#endif /* CRTC2 */

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


#ifdef CRTC2
static void crtc2_frame_sel(int frame)
{
switch(frame) {
case 0:	
	cregs.c2pl2startadd0=regs.besa1corg;
	cregs.c2pl3startadd0=regs.besa1c3org;
	cregs.c2startadd0=regs.besa1org;
	break;
case 1:
	cregs.c2pl2startadd0=regs.besa2corg;
	cregs.c2pl3startadd0=regs.besa2c3org;
	cregs.c2startadd0=regs.besa2org;
	break;
case 2:
	cregs.c2pl2startadd0=regs.besb1corg;
	cregs.c2pl3startadd0=regs.besb1c3org;
	cregs.c2startadd0=regs.besb1org;
	break;
case 3:
	cregs.c2pl2startadd0=regs.besb2corg;
	cregs.c2pl3startadd0=regs.besb2c3org;
	cregs.c2startadd0=regs.besb2org;
	break;
}
	writel(cregs.c2startadd0, mga_mmio_base + C2STARTADD0);
	writel(cregs.c2pl2startadd0, mga_mmio_base + C2PL2STARTADD0);
	writel(cregs.c2pl3startadd0, mga_mmio_base + C2PL3STARTADD0);
}
#endif

static int mga_frame_select(unsigned int frame)
{
    mga_next_frame = frame;
    if (mga_verbose>1) printf("[mga] frameselect: %d\n", mga_next_frame);
#if MGA_ALLOW_IRQ
    if (mga_irq == -1)
#endif
    {
	//we don't need the vcount protection as we're only hitting
	//one register (and it doesn't seem to be double buffered)
	regs.besctl = (regs.besctl & ~0x07000000) + (mga_next_frame << 25);
	writel( regs.besctl, mga_mmio_base + BESCTL ); 

//	writel( regs.besglobctl + ((readl(mga_mmio_base + VCOUNT)+2)<<16),
	writel( regs.besglobctl + (MGA_VSYNC_POS<<16),
			mga_mmio_base + BESGLOBCTL);
#ifdef CRTC2
	crtc2_frame_sel(mga_next_frame);
#endif
    }

    return 0;
}


static void mga_vid_write_regs(int restore)
{
	//Make sure internal registers don't get updated until we're done
	writel( (readl(mga_mmio_base + VCOUNT)-1)<<16,
			mga_mmio_base + BESGLOBCTL);

	// color or coordinate keying
	
	if(restore && colkey_saved){
	    // restore it
	    colkey_saved=0;

		// Set color key registers:
		writeb( XKEYOPMODE, mga_mmio_base + PALWTADD);
		writeb( colkey_on, mga_mmio_base + X_DATAREG);
		
		writeb( XCOLKEY0RED, mga_mmio_base + PALWTADD);
		writeb( colkey_color[0], mga_mmio_base + X_DATAREG);
		writeb( XCOLKEY0GREEN, mga_mmio_base + PALWTADD);
		writeb( colkey_color[1], mga_mmio_base + X_DATAREG);
		writeb( XCOLKEY0BLUE, mga_mmio_base + PALWTADD);
		writeb( colkey_color[2], mga_mmio_base + X_DATAREG);
		writeb( X_COLKEY, mga_mmio_base + PALWTADD);
		writeb( colkey_color[3], mga_mmio_base + X_DATAREG);

		writeb( XCOLMSK0RED, mga_mmio_base + PALWTADD);
		writeb( colkey_mask[0], mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK0GREEN, mga_mmio_base + PALWTADD);
		writeb( colkey_mask[1], mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK0BLUE, mga_mmio_base + PALWTADD);
		writeb( colkey_mask[2], mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK, mga_mmio_base + PALWTADD);
		writeb( colkey_mask[3], mga_mmio_base + X_DATAREG);

	} else if(!colkey_saved){
	    // save it
	    colkey_saved=1;
		// Get color key registers:
		writeb( XKEYOPMODE, mga_mmio_base + PALWTADD);
		colkey_on=(unsigned char)readb(mga_mmio_base + X_DATAREG) & 1;
		
		writeb( XCOLKEY0RED, mga_mmio_base + PALWTADD);
		colkey_color[0]=(unsigned char)readb(mga_mmio_base + X_DATAREG);
		writeb( XCOLKEY0GREEN, mga_mmio_base + PALWTADD);
		colkey_color[1]=(unsigned char)readb(mga_mmio_base + X_DATAREG);
		writeb( XCOLKEY0BLUE, mga_mmio_base + PALWTADD);
		colkey_color[2]=(unsigned char)readb(mga_mmio_base + X_DATAREG);
		writeb( X_COLKEY, mga_mmio_base + PALWTADD);
		colkey_color[3]=(unsigned char)readb(mga_mmio_base + X_DATAREG);

		writeb( XCOLMSK0RED, mga_mmio_base + PALWTADD);
		colkey_mask[0]=(unsigned char)readb(mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK0GREEN, mga_mmio_base + PALWTADD);
		colkey_mask[1]=(unsigned char)readb(mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK0BLUE, mga_mmio_base + PALWTADD);
		colkey_mask[2]=(unsigned char)readb(mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK, mga_mmio_base + PALWTADD);
		colkey_mask[3]=(unsigned char)readb(mga_mmio_base + X_DATAREG);
	}
	
if(!restore){
	writeb( XKEYOPMODE, mga_mmio_base + PALWTADD);
	writeb( mga_grkey.ckey.op == CKEY_TRUE, mga_mmio_base + X_DATAREG);
	if ( mga_grkey.ckey.op == CKEY_TRUE )
	{
		uint32_t r=0, g=0, b=0;

		writeb( XMULCTRL, mga_mmio_base + PALWTADD);
		switch (readb (mga_mmio_base + X_DATAREG)) 
		{
			case BPP_8:
				/* Need to look up the color index, just using
														 color 0 for now. */
			break;

			case BPP_15:
				r = mga_grkey.ckey.red   >> 3;
				g = mga_grkey.ckey.green >> 3;
				b = mga_grkey.ckey.blue  >> 3;
			break;

			case BPP_16:
				r = mga_grkey.ckey.red   >> 3;
				g = mga_grkey.ckey.green >> 2;
				b = mga_grkey.ckey.blue  >> 3;
			break;

			case BPP_24:
			case BPP_32_DIR:
			case BPP_32_PAL:
				r = mga_grkey.ckey.red;
				g = mga_grkey.ckey.green;
				b = mga_grkey.ckey.blue;
			break;
		}

		// Enable colorkeying
		writeb( XKEYOPMODE, mga_mmio_base + PALWTADD);
		writeb( 1, mga_mmio_base + X_DATAREG);

		// Disable color keying on alpha channel 
		writeb( XCOLMSK, mga_mmio_base + PALWTADD);
		writeb( 0x00, mga_mmio_base + X_DATAREG);
		writeb( X_COLKEY, mga_mmio_base + PALWTADD);
		writeb( 0x00, mga_mmio_base + X_DATAREG);


		// Set up color key registers
		writeb( XCOLKEY0RED, mga_mmio_base + PALWTADD);
		writeb( r, mga_mmio_base + X_DATAREG);
		writeb( XCOLKEY0GREEN, mga_mmio_base + PALWTADD);
		writeb( g, mga_mmio_base + X_DATAREG);
		writeb( XCOLKEY0BLUE, mga_mmio_base + PALWTADD);
		writeb( b, mga_mmio_base + X_DATAREG);

		// Set up color key mask registers
		writeb( XCOLMSK0RED, mga_mmio_base + PALWTADD);
		writeb( 0xff, mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK0GREEN, mga_mmio_base + PALWTADD);
		writeb( 0xff, mga_mmio_base + X_DATAREG);
		writeb( XCOLMSK0BLUE, mga_mmio_base + PALWTADD);
		writeb( 0xff, mga_mmio_base + X_DATAREG);
	}
	else
	{
		// Disable colorkeying
		writeb( XKEYOPMODE, mga_mmio_base + PALWTADD);
		writeb( 0, mga_mmio_base + X_DATAREG);
	}
}

	// Backend Scaler
	writel( regs.besctl,      mga_mmio_base + BESCTL); 
	if(is_g400)
		writel( regs.beslumactl,  mga_mmio_base + BESLUMACTL); 
	writel( regs.bespitch,    mga_mmio_base + BESPITCH); 

	writel( regs.besa1org,    mga_mmio_base + BESA1ORG);
	writel( regs.besa1corg,   mga_mmio_base + BESA1CORG);
	writel( regs.besa2org,    mga_mmio_base + BESA2ORG);
	writel( regs.besa2corg,   mga_mmio_base + BESA2CORG);
	writel( regs.besb1org,    mga_mmio_base + BESB1ORG);
	writel( regs.besb1corg,   mga_mmio_base + BESB1CORG);
	writel( regs.besb2org,    mga_mmio_base + BESB2ORG);
	writel( regs.besb2corg,   mga_mmio_base + BESB2CORG);
	if(is_g400) 
	{
		writel( regs.besa1c3org,  mga_mmio_base + BESA1C3ORG);
		writel( regs.besa2c3org,  mga_mmio_base + BESA2C3ORG);
		writel( regs.besb1c3org,  mga_mmio_base + BESB1C3ORG);
		writel( regs.besb2c3org,  mga_mmio_base + BESB2C3ORG);
	}

	writel( regs.beshcoord,   mga_mmio_base + BESHCOORD);
	writel( regs.beshiscal,   mga_mmio_base + BESHISCAL);
	writel( regs.beshsrcst,   mga_mmio_base + BESHSRCST);
	writel( regs.beshsrcend,  mga_mmio_base + BESHSRCEND);
	writel( regs.beshsrclst,  mga_mmio_base + BESHSRCLST);
	
	writel( regs.besvcoord,   mga_mmio_base + BESVCOORD);
	writel( regs.besviscal,   mga_mmio_base + BESVISCAL);

	writel( regs.besv1srclst, mga_mmio_base + BESV1SRCLST);
	writel( regs.besv1wght,   mga_mmio_base + BESV1WGHT);
	writel( regs.besv2srclst, mga_mmio_base + BESV2SRCLST);
	writel( regs.besv2wght,   mga_mmio_base + BESV2WGHT);
	
	//update the registers somewhere between 1 and 2 frames from now.
	writel( regs.besglobctl + ((readl(mga_mmio_base + VCOUNT)+2)<<16),
			mga_mmio_base + BESGLOBCTL);

	if (mga_verbose > 1)
	{
	    printf("[mga] wrote BES registers\n");
	    printf("[mga] BESCTL = 0x%08x\n",
			readl(mga_mmio_base + BESCTL));
	    printf("[mga] BESGLOBCTL = 0x%08x\n",
			readl(mga_mmio_base + BESGLOBCTL));
	    printf("[mga] BESSTATUS= 0x%08x\n",
			readl(mga_mmio_base + BESSTATUS));
	}
#ifdef CRTC2
	writel(((readl(mga_mmio_base + C2CTL) & ~0x03e00000) + (cregs.c2ctl & 0x03e00000)),	mga_mmio_base + C2CTL);
	writel(((readl(mga_mmio_base + C2DATACTL) & ~0x000000ff) + (cregs.c2datactl & 0x000000ff)), mga_mmio_base + C2DATACTL);
	// ctrc2
	// disable CRTC2 acording to specs
	writel(cregs.c2misc, mga_mmio_base + C2MISC);

	if (mga_verbose > 1) printf("[mga] c2offset = %d\n",cregs.c2offset);

	writel(cregs.c2offset, mga_mmio_base + C2OFFSET);
	writel(cregs.c2startadd0, mga_mmio_base + C2STARTADD0);
	writel(cregs.c2pl2startadd0, mga_mmio_base + C2PL2STARTADD0);
	writel(cregs.c2pl3startadd0, mga_mmio_base + C2PL3STARTADD0);
	writel(cregs.c2spicstartadd0, mga_mmio_base + C2SPICSTARTADD0);
#endif	
}

#if MGA_ALLOW_IRQ
static void enable_irq(void)
{
	long int cc;

	cc = readl(mga_mmio_base + IEN);

	writeb( 0x11, mga_mmio_base + CRTCX);
	
	writeb(0x20, mga_mmio_base + CRTCD );  /* clear 0, enable off */
	writeb(0x00, mga_mmio_base + CRTCD );  /* enable on */
	writeb(0x10, mga_mmio_base + CRTCD );  /* clear = 1 */
	
	writel( regs.besglobctl , mga_mmio_base + BESGLOBCTL);
    	
	return;
}

static void disable_irq(void)
{
	writeb( 0x11, mga_mmio_base + CRTCX);
	writeb(0x20, mga_mmio_base + CRTCD );  /* clear 0, enable off */

	return;
}

void mga_handle_irq(int irq, void *dev_id/*, struct pt_regs *pregs*/) {
	long int cc;

	if ( irq != -1 ) {

		cc = readl(mga_mmio_base + STATUS);
		if ( ! (cc & 0x10) ) return;  /* vsyncpen */
	} 

	regs.besctl = (regs.besctl & ~0x07000000) + (mga_next_frame << 25);
	writel( regs.besctl, mga_mmio_base + BESCTL ); 

#ifdef CRTC2
// sem pridat vyber obrazku !!!!	
	crtc2_frame_sel(mga_next_frame);
#endif
	
	if ( irq != -1 ) {
		writeb( 0x11, mga_mmio_base + CRTCX);
		writeb( 0, mga_mmio_base + CRTCD );
		writeb( 0x10, mga_mmio_base + CRTCD );
	}

	return;

}
#endif /* MGA_ALLOW_IRQ */

static int mga_config_playback(vidix_playback_t *config)
{
	unsigned int i;
	int x, y, sw, sh, dw, dh;
	int besleft, bestop, ifactor, ofsleft, ofstop, baseadrofs, weight, weights;
#ifdef CRTC2
#define right_margin 0
#define left_margin 18
#define hsync_len 46
#define lower_margin 10
#define vsync_len 4
#define upper_margin 39

	unsigned int hdispend = (config->src.w + 31) & ~31;
	unsigned int hsyncstart = hdispend + (right_margin & ~7);
	unsigned int hsyncend = hsyncstart + (hsync_len & ~7);
	unsigned int htotal = hsyncend + (left_margin & ~7);
	unsigned int vdispend = config->src.h;
	unsigned int vsyncstart = vdispend + lower_margin;
	unsigned int vsyncend = vsyncstart + vsync_len;
	unsigned int vtotal = vsyncend + upper_margin;
#endif 

    if ((config->num_frames < 1) || (config->num_frames > 4))
    {
	printf("[mga] illegal num_frames: %d, setting to %d\n",
	    config->num_frames, MGA_DEFAULT_FRAMES);
	config->num_frames = MGA_DEFAULT_FRAMES;
    }

    x = config->dest.x;
    y = config->dest.y;
    sw = config->src.w;
    sh = config->src.h;
    dw = config->dest.w;
    dh = config->dest.h;
    
    config->dest.pitch.y=32;
    config->dest.pitch.u=config->dest.pitch.v=32;

    if (mga_verbose) printf("[mga] Setting up a %dx%d-%dx%d video window (src %dx%d) format %X\n",
           dw, dh, x, y, sw, sh, config->fourcc);

    if ((sw < 4) || (sh < 4) || (dw < 4) || (dh < 4))
    {
        printf("[mga] Invalid src/dest dimensions\n");
        return EINVAL;
    }

    //FIXME check that window is valid and inside desktop

    sw+=sw&1;
    switch(config->fourcc)
    {
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	    sh+=sh&1;
	    config->frame_size = ((sw + 31) & ~31) * sh + (((sw + 31) & ~31) * sh) / 2;
	    break;
	case IMGFMT_YUY2:
	case IMGFMT_UYVY:
	    config->frame_size = ((sw + 31) & ~31) * sh * 2;
	    break;
	default:
	    printf("[mga] Unsupported pixel format: %x\n", config->fourcc);
	    return ENOTSUP;
    }

    config->offsets[0] = 0;
    for (i = 1; i < config->num_frames+1; i++)
	config->offsets[i] = i*config->frame_size;

    config->offset.y=0;
    if(config->fourcc == IMGFMT_I420 || config->fourcc == IMGFMT_IYUV)
    {
	config->offset.u=((sw + 31) & ~31) * sh;
	config->offset.v=config->offset.u+((sw + 31) & ~31) * sh /4;
    }
    else {
	config->offset.v=((sw + 31) & ~31) * sh;
	config->offset.u=config->offset.v+((sw + 31) & ~31) * sh /4;
    }

    mga_src_base = (mga_ram_size*0x100000-config->num_frames*config->frame_size);
    if (mga_src_base < 0)
    {
    	printf("[mga] not enough memory for frames!\n");
    	return EFAULT;
    }
    mga_src_base &= (~0xFFFF); /* 64k boundary */
    if (mga_verbose > 1) printf("[mga] YUV buffer base: %#x\n", mga_src_base);

    config->dga_addr = mga_mem_base + mga_src_base;

    /* for G200 set Interleaved UV planes */
    if (!is_g400)
	config->flags = VID_PLAY_INTERLEAVED_UV | INTERLEAVING_UV;
	
    //Setup the BES registers for a three plane 4:2:0 video source 

    regs.besglobctl = 0;

    switch(config->fourcc)
    {
	case IMGFMT_YV12:	
	case IMGFMT_I420:	
	case IMGFMT_IYUV:	
	regs.besctl = 1         // BES enabled
                    + (0<<6)    // even start polarity
                    + (1<<10)   // x filtering enabled
                    + (1<<11)   // y filtering enabled
                    + (1<<16)   // chroma upsampling
                    + (1<<17)   // 4:2:0 mode
                    + (1<<18);  // dither enabled
        break;

    case IMGFMT_YUY2:	
	regs.besctl = 1         // BES enabled
                    + (0<<6)    // even start polarity
                    + (1<<10)   // x filtering enabled
                    + (1<<11)   // y filtering enabled
                    + (1<<16)   // chroma upsampling
                    + (0<<17)   // 4:2:2 mode
                    + (1<<18);  // dither enabled

	regs.besglobctl = 0;        // YUY2 format selected
        break;

    case IMGFMT_UYVY:	
	regs.besctl = 1         // BES enabled
                    + (0<<6)    // even start polarity
                    + (1<<10)   // x filtering enabled
                    + (1<<11)   // y filtering enabled
                    + (1<<16)   // chroma upsampling
                    + (0<<17)   // 4:2:2 mode
                    + (1<<18);  // dither enabled

	regs.besglobctl = 1<<6;        // UYVY format selected
        break;

    }

	//Disable contrast and brightness control
	regs.besglobctl |= (1<<5) + (1<<7);
	regs.beslumactl = (0x7f << 16) + (0x80<<0);
	regs.beslumactl = 0x80<<0;

	//Setup destination window boundaries
	besleft = x > 0 ? x : 0;
	bestop = y > 0 ? y : 0;
	regs.beshcoord = (besleft<<16) + (x + dw-1);
	regs.besvcoord = (bestop<<16) + (y + dh-1);
	
	//Setup source dimensions
	regs.beshsrclst  = (sw - 1) << 16;
	regs.bespitch = (sw + 31) & ~31 ; 
	
	//Setup horizontal scaling
	ifactor = ((sw-1)<<14)/(dw-1);
	ofsleft = besleft - x;
		
	regs.beshiscal = ifactor<<2;
	regs.beshsrcst = (ofsleft*ifactor)<<2;
	regs.beshsrcend = regs.beshsrcst + (((dw - ofsleft - 1) * ifactor) << 2);
	
	//Setup vertical scaling
	ifactor = ((sh-1)<<14)/(dh-1);
	ofstop = bestop - y;

	regs.besviscal = ifactor<<2;

	baseadrofs = ((ofstop*regs.besviscal)>>16)*regs.bespitch;
	regs.besa1org = (uint32_t) mga_src_base + baseadrofs;
	regs.besa2org = (uint32_t) mga_src_base + baseadrofs + 1*config->frame_size;
	regs.besb1org = (uint32_t) mga_src_base + baseadrofs + 2*config->frame_size;
	regs.besb2org = (uint32_t) mga_src_base + baseadrofs + 3*config->frame_size;

if(config->fourcc==IMGFMT_YV12
 ||config->fourcc==IMGFMT_IYUV
 ||config->fourcc==IMGFMT_I420
 ){
        // planar YUV frames:
	if (is_g400) 
		baseadrofs = (((ofstop*regs.besviscal)/4)>>16)*regs.bespitch;
	else 
		baseadrofs = (((ofstop*regs.besviscal)/2)>>16)*regs.bespitch;

    if(config->fourcc==IMGFMT_YV12){
	regs.besa1corg = (uint32_t) mga_src_base + baseadrofs + regs.bespitch * sh ;
	regs.besa2corg = (uint32_t) mga_src_base + baseadrofs + 1*config->frame_size + regs.bespitch * sh;
	regs.besb1corg = (uint32_t) mga_src_base + baseadrofs + 2*config->frame_size + regs.bespitch * sh;
	regs.besb2corg = (uint32_t) mga_src_base + baseadrofs + 3*config->frame_size + regs.bespitch * sh;
	regs.besa1c3org = regs.besa1corg + ((regs.bespitch * sh) / 4);
	regs.besa2c3org = regs.besa2corg + ((regs.bespitch * sh) / 4);
	regs.besb1c3org = regs.besb1corg + ((regs.bespitch * sh) / 4);
	regs.besb2c3org = regs.besb2corg + ((regs.bespitch * sh) / 4);
    } else {
	regs.besa1c3org = (uint32_t) mga_src_base + baseadrofs + regs.bespitch * sh ;
	regs.besa2c3org = (uint32_t) mga_src_base + baseadrofs + 1*config->frame_size + regs.bespitch * sh;
	regs.besb1c3org = (uint32_t) mga_src_base + baseadrofs + 2*config->frame_size + regs.bespitch * sh;
	regs.besb2c3org = (uint32_t) mga_src_base + baseadrofs + 3*config->frame_size + regs.bespitch * sh;
	regs.besa1corg = regs.besa1c3org + ((regs.bespitch * sh) / 4);
	regs.besa2corg = regs.besa2c3org + ((regs.bespitch * sh) / 4);
	regs.besb1corg = regs.besb1c3org + ((regs.bespitch * sh) / 4);
	regs.besb2corg = regs.besb2c3org + ((regs.bespitch * sh) / 4);
    }

}

    weight = ofstop * (regs.besviscal >> 2);
    weights = weight < 0 ? 1 : 0;
    regs.besv2wght = regs.besv1wght = (weights << 16) + ((weight & 0x3FFF) << 2);
    regs.besv2srclst = regs.besv1srclst = sh - 1 - (((ofstop * regs.besviscal) >> 16) & 0x03FF);

#ifdef CRTC2
	// pridat hlavni registry - tj. casovani ...


switch(config->fourcc){
    case IMGFMT_YV12:	
    case IMGFMT_I420:	
    case IMGFMT_IYUV:	
	cregs.c2ctl = 1         // CRTC2 enabled
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
	cregs.c2datactl = 1         // disable dither - propably not needed, we are already in YUV mode
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

    case IMGFMT_YUY2:	
	cregs.c2ctl = 1         // CRTC2 enabled
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
	cregs.c2datactl = 1         // disable dither - propably not needed, we are already in YUV mode
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

    case IMGFMT_UYVY:	
	cregs.c2ctl = 1         // CRTC2 enabled
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
	cregs.c2datactl = 0         // enable dither - propably not needed, we are already in YUV mode
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
    }

	cregs.c2hparam=((hdispend - 8) << 16) | (htotal - 8);
	cregs.c2hsync=((hsyncend - 8) << 16) | (hsyncstart - 8);
	
	cregs.c2misc=0	// CRTCV2 656 togg f0
		    +(0<<1) // CRTCV2 656 togg f0
		    +(0<<2) // CRTCV2 656 togg f0
		    +(0<<4) // CRTCV2 656 togg f1
		    +(0<<5) // CRTCV2 656 togg f1
		    +(0<<6) // CRTCV2 656 togg f1
		    +(0<<8) // Hsync active high
		    +(0<<9) // Vsync active high
		    // 16-27 c2vlinecomp - nevim co tam dat
		    ;
	cregs.c2offset=(regs.bespitch << 1);

	cregs.c2pl2startadd0=regs.besa1corg;
	cregs.c2pl3startadd0=regs.besa1c3org;
		    
	cregs.c2preload=(vsyncstart << 16) | (hsyncstart); // from 
	
	cregs.c2spicstartadd0=0; // not used
	
    cregs.c2startadd0=regs.besa1org;
	
    cregs.c2subpiclut=0; //not used
	
    cregs.c2vparam=((vdispend - 1) << 16) | (vtotal - 1);
    cregs.c2vsync=((vsyncend - 1) << 16) | (vsyncstart - 1);
#endif /* CRTC2 */

    mga_vid_write_regs(0);
    return 0;
}

static int mga_playback_on(void)
{
    if (mga_verbose) printf("[mga] playback on\n");

    vid_src_ready = 1;
    if(vid_overlay_on)
    {
	regs.besctl |= 1;
    	mga_vid_write_regs(0);
    }
#if MGA_ALLOW_IRQ
    if (mga_irq != -1)
	enable_irq();
#endif
    mga_next_frame=0;

    return 0;
}

static int mga_playback_off(void)
{
    if (mga_verbose) printf("[mga] playback off\n");

    vid_src_ready = 0;   
#if MGA_ALLOW_IRQ
    if (mga_irq != -1)
	disable_irq();
#endif
    regs.besctl &= ~1;
    regs.besglobctl &= ~(1<<6); /* UYVY format selected */
    mga_vid_write_regs(0);

    return 0;
}

static int mga_probe(int verbose,int force)
{
	pciinfo_t lst[MAX_PCI_DEVICES];
	unsigned int i, num_pci;
	int err;

	if (verbose) printf("[mga] probe\n");

	mga_verbose = verbose;

	is_g400 = -1;

	err = pci_scan(lst, &num_pci);
	if (err)
	{
	    printf("[mga] Error occurred during pci scan: %s\n", strerror(err));
	    return err;
	}

	if (mga_verbose)
	    printf("[mga] found %d pci devices\n", num_pci);
	
	for (i = 0; i < num_pci; i++)
	{
	    if (mga_verbose > 1)
		printf("[mga] pci[%d] vendor: %d device: %d\n",
		    i, lst[i].vendor, lst[i].device);
	    if (lst[i].vendor == VENDOR_MATROX)
	    {
#if 0
		if ((lst[i].command & PCI_COMMAND_IO) == 0)
		{
			printf("[mga] Device is disabled, ignoring\n");
			continue;
		}
#endif
		switch(lst[i].device)
		{
		    case DEVICE_MATROX_MGA_G550_AGP:
			printf("[mga] Found MGA G550\n");
			is_g400 = 1;
			goto card_found;
		    case DEVICE_MATROX_MGA_G400_G450:
			printf("[mga] Found MGA G400/G450\n");
			is_g400 = 1;
			goto card_found;
		    case DEVICE_MATROX_MGA_G200_AGP:
			printf("[mga] Found MGA G200 AGP\n");
			is_g400 = 0;
			goto card_found;
		    case DEVICE_MATROX_MGA_G200:
			printf("[mga] Found MGA G200 PCI\n");
			is_g400 = 0;
			goto card_found;
		}
	    }
	}

	if (is_g400 == -1)
	{
		if (verbose) printf("[mga] Can't find chip\n");
		return ENXIO;
	}

card_found:
	probed = 1;
	memcpy(&pci_info, &lst[i], sizeof(pciinfo_t));

	mga_cap.device_id = pci_info.device; /* set device id in capabilites */

	return 0;
}

static int mga_init(void)
{
    unsigned int card_option = 0;
    int err;
    
    if (mga_verbose) printf("[mga] init\n");

    mga_vid_in_use = 0;

    printf("Matrox MGA G200/G400/G450 YUV Video interface v2.01 (c) Aaron Holtzman & A'rpi\n");
#ifdef CRTC2
    printf("Driver compiled with TV-out (second-head) support\n");
#endif

    if (!probed)
    {
	printf("[mga] driver was not probed but is being initializing\n");
	return EINTR;
    }

#ifdef MGA_PCICONFIG_MEMDETECT
    pci_config_read(pci_info.bus, pci_info.card, pci_info.func,
        0x40, 4, &card_option);
    if (mga_verbose > 1) printf("[mga] OPTION word: 0x%08X  mem: 0x%02X  %s\n", card_option,
    	(card_option>>10)&0x17, ((card_option>>14)&1)?"SGRAM":"SDRAM");
#endif

    if (mga_ram_size)
    {
    	printf("[mga] RAMSIZE forced to %d MB\n", mga_ram_size);
    }
    else
    {
#ifdef MGA_MEMORY_SIZE
        mga_ram_size = MGA_MEMORY_SIZE;
        printf("[mga] hard-coded RAMSIZE is %d MB\n", (unsigned int) mga_ram_size);
#else
        if (is_g400)
	{
	    switch((card_option>>10)&0x17)
	    {
	        // SDRAM:
	        case 0x00:
	        case 0x04:  mga_ram_size = 16; break;
	        case 0x03:  mga_ram_size = 32; break;
	        // SGRAM:
	        case 0x10:
	        case 0x14:  mga_ram_size = 32; break;
	        case 0x11:
	        case 0x12:  mga_ram_size = 16; break;
	        default:
	    	    mga_ram_size = 16;
		    printf("[mga] Couldn't detect RAMSIZE, assuming 16MB!\n");
	    }
	}
	else
	{
	    switch((card_option>>10)&0x17)
	    {
		default: mga_ram_size = 8;
	    }
	} 

        printf("[mga] detected RAMSIZE is %d MB\n", (unsigned int) mga_ram_size);
#endif
    }

    if (mga_ram_size)
    {
	if ((mga_ram_size < 4) || (mga_ram_size > 64))
	{
	    printf("[mga] invalid RAMSIZE: %d MB\n", mga_ram_size);
	    return EINVAL;
	}
    }

    if (mga_verbose > 1) printf("[mga] hardware addresses: mmio: %#x, framebuffer: %#x\n",
        pci_info.base1, pci_info.base0);

    mga_mmio_base = map_phys_mem(pci_info.base1,0x4000);
    mga_mem_base = map_phys_mem(pci_info.base0,mga_ram_size*1024*1024);

    if (mga_verbose > 1) printf("[mga] MMIO at %p, IRQ: %d, framebuffer: %p\n",
        mga_mmio_base, mga_irq, mga_mem_base);
    err = mtrr_set_type(pci_info.base0,mga_ram_size*1024*1024,MTRR_TYPE_WRCOMB);
    if(!err) printf("[mga] Set write-combining type of video memory\n");
#if MGA_ALLOW_IRQ
    if (mga_irq != -1)
    {
    	int tmp = request_irq(mga_irq, mga_handle_irq, SA_INTERRUPT | SA_SHIRQ, "Syncfb Time Base", &mga_irq);
    	if (tmp)
	{
    	    printf("syncfb (mga): cannot register irq %d (Err: %d)\n", mga_irq, tmp);
    	    mga_irq=-1;
	}
	else
	{
	    printf("syncfb (mga): registered irq %d\n", mga_irq);
	}
    }
    else
    {
	printf("syncfb (mga): No valid irq was found\n");
	mga_irq=-1;
    }
#else
	printf("syncfb (mga): IRQ disabled in mga_vid.c\n");
	mga_irq=-1;
#endif

    return 0;
}

static void mga_destroy(void)
{
    if (mga_verbose) printf("[mga] destroy\n");

    /* FIXME turn off BES */
    vid_src_ready = 0;   
    regs.besctl &= ~1;
    regs.besglobctl &= ~(1<<6);  // UYVY format selected
    mga_vid_write_regs(1);
    mga_vid_in_use = 0;

#if MGA_ALLOW_IRQ
    if (mga_irq != -1)
    	free_irq(mga_irq, &mga_irq);
#endif

    if (mga_mmio_base)
        unmap_phys_mem(mga_mmio_base, 0x4000);
    if (mga_mem_base)
        unmap_phys_mem(mga_mem_base, mga_ram_size);
    return;
}

static int mga_query_fourcc(vidix_fourcc_t *to)
{
    if (mga_verbose) printf("[mga] query fourcc (%x)\n", to->fourcc);

    switch(to->fourcc)
    {
	case IMGFMT_YV12:
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YUY2:
	case IMGFMT_UYVY:
	    break;
	default:
	    to->depth = to->flags = 0;
	    return ENOTSUP;
    }
    
    to->depth = VID_DEPTH_12BPP |
		VID_DEPTH_15BPP | VID_DEPTH_16BPP |
		VID_DEPTH_24BPP | VID_DEPTH_32BPP;
    to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK | VID_CAP_COLORKEY;
    return 0;
}

static int mga_get_caps(vidix_capability_t *to)
{
    memcpy(to, &mga_cap, sizeof(vidix_capability_t));
    return 0;
}

static int mga_get_gkeys(vidix_grkey_t *grkey)
{
    memcpy(grkey, &mga_grkey, sizeof(vidix_grkey_t));
    return 0;
}

static int mga_set_gkeys(const vidix_grkey_t *grkey)
{
    memcpy(&mga_grkey, grkey, sizeof(vidix_grkey_t));
    mga_vid_write_regs(0);
    return 0;
}

static int mga_set_eq( const vidix_video_eq_t * eq)
{
    /* contrast and brightness control isn't supported on G200 - alex */
    if (!is_g400)
    {
	if (mga_verbose) printf("[mga] equalizer isn't supported with G200\n");
	return ENOTSUP;
    }

    // only brightness&contrast are supported:
    if(!(eq->cap & (VEQ_CAP_BRIGHTNESS|VEQ_CAP_CONTRAST)))
	return ENOTSUP;
    
    //regs.beslumactl = readl(mga_mmio_base + BESLUMACTL);
    if (eq->cap & VEQ_CAP_BRIGHTNESS) { 
	regs.beslumactl &= 0xFFFF;
	regs.beslumactl |= (eq->brightness*255/2000)<<16;
    }
    if (eq->cap & VEQ_CAP_CONTRAST) {
	regs.beslumactl &= 0xFFFF0000;
	regs.beslumactl |= (128+eq->contrast*255/2000)&0xFFFF;
    }
    writel(regs.beslumactl,mga_mmio_base + BESLUMACTL);

    return 0;
}

static int mga_get_eq( vidix_video_eq_t * eq)
{
    /* contrast and brightness control isn't supported on G200 - alex */
    if (!is_g400)
    {
	if (mga_verbose) printf("[mga] equalizer isn't supported with G200\n");
	return ENOTSUP;
    }

    eq->brightness = (signed short int)(regs.beslumactl >> 16) * 1000 / 128;
    eq->contrast = (signed short int)(regs.beslumactl & 0xFFFF) * 1000 / 128 - 1000;
    eq->cap = VEQ_CAP_BRIGHTNESS | VEQ_CAP_CONTRAST;
    
    printf("MGA GET_EQ: br=%d c=%d  \n",eq->brightness,eq->contrast);

    return 0;
}

#ifndef CRTC2
VDXDriver mga_drv = {
  "mga",
#else
VDXDriver mga_crtc2_drv = {
  "mga_crtc2",
#endif
  NULL,
    
  .probe = mga_probe,
  .get_caps = mga_get_caps,
  .query_fourcc = mga_query_fourcc,
  .init = mga_init,
  .destroy = mga_destroy,
  .config_playback = mga_config_playback,
  .playback_on = mga_playback_on,
  .playback_off = mga_playback_off,
  .frame_sel = mga_frame_select,
  .get_eq = mga_get_eq,
  .set_eq = mga_set_eq,
  .get_gkey = mga_get_gkeys,
  .set_gkey = mga_set_gkeys,
};
