/*
    Driver for S3 Savage Series

    Copyright (C) 2004 by Reza Jelveh

    Based on the X11 driver and nvidia vid 

    Thanks to Alex Deucher for Support

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

    Changes:
    2004-11-09
      Initial version

    To Do:
			
*/


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>

#include "vidix.h"
#include "vidixlib.h"
#include "fourcc.h"
#include "../libdha/libdha.h"
#include "../libdha/pci_ids.h"
#include "../libdha/pci_names.h"
#include "../config.h"

#include "savage_regs.h"


#define VF_STREAMS_ON   0x0001
#define BASE_PAD 0xf
#define FRAMEBUFFER_SIZE 1024*2000*4
/**************************************
   S3 streams processor
**************************************/

#define EXT_MISC_CTRL2              0x67

/* New streams */

/* CR67[2] = 1 : enable stream 1 */
#define ENABLE_STREAM1              0x04
/* CR67[1] = 1 : enable stream 2 */
#define ENABLE_STREAM2              0x02
/* mask to clear CR67[2,1] */
#define NO_STREAMS                  0xF9
/* CR67[3] = 1 : Mem-mapped regs */
#define USE_MM_FOR_PRI_STREAM       0x08

#define HDM_SHIFT	16
#define HDSCALE_4	(2 << HDM_SHIFT)
#define HDSCALE_8	(3 << HDM_SHIFT)
#define HDSCALE_16	(4 << HDM_SHIFT)
#define HDSCALE_32	(5 << HDM_SHIFT)
#define HDSCALE_64	(6 << HDM_SHIFT)

/* Old Streams */

#define ENABLE_STREAMS_OLD	    0x0c
#define NO_STREAMS_OLD		    0xf3
/* CR69[0] = 1 : Mem-mapped regs */
#define USE_MM_FOR_PRI_STREAM_OLD   0x01

void SavageStreamsOn(void);

/*
 * There are two different streams engines used in the Savage line.
 * The old engine is in the 3D, 4, Pro, and Twister.
 * The new engine is in the 2000, MX, IX, and Super.
 */


/* streams registers for old engine */
#define PSTREAM_CONTROL_REG		0x8180
#define COL_CHROMA_KEY_CONTROL_REG	0x8184
#define SSTREAM_CONTROL_REG		0x8190
#define CHROMA_KEY_UPPER_BOUND_REG	0x8194
#define SSTREAM_STRETCH_REG		0x8198
#define COLOR_ADJUSTMENT_REG		0x819C
#define BLEND_CONTROL_REG		0x81A0
#define PSTREAM_FBADDR0_REG		0x81C0
#define PSTREAM_FBADDR1_REG		0x81C4
#define PSTREAM_STRIDE_REG		0x81C8
#define DOUBLE_BUFFER_REG		0x81CC
#define SSTREAM_FBADDR0_REG		0x81D0
#define SSTREAM_FBADDR1_REG		0x81D4
#define SSTREAM_STRIDE_REG		0x81D8
#define SSTREAM_VSCALE_REG		0x81E0
#define SSTREAM_VINITIAL_REG		0x81E4
#define SSTREAM_LINES_REG		0x81E8
#define STREAMS_FIFO_REG		0x81EC
#define PSTREAM_WINDOW_START_REG	0x81F0
#define PSTREAM_WINDOW_SIZE_REG		0x81F4
#define SSTREAM_WINDOW_START_REG	0x81F8
#define SSTREAM_WINDOW_SIZE_REG		0x81FC
#define FIFO_CONTROL			0x8200
#define PSTREAM_FBSIZE_REG		0x8300
#define SSTREAM_FBSIZE_REG		0x8304
#define SSTREAM_FBADDR2_REG		0x8308

#define OS_XY(x,y)	(((x+1)<<16)|(y+1))
#define OS_WH(x,y)	(((x-1)<<16)|(y))

#define PCI_COMMAND_MEM 0x2
#define MAX_FRAMES 3
/**
 * @brief Information on PCI device.
 */
pciinfo_t pci_info;

uint8_t *vio;
uint8_t mclk_save[3];

#define outb(reg,val)	OUTPORT8(reg,val)
#define inb(reg)	INPORT8(reg)
#define outw(reg,val)	OUTPORT16(reg,val)
#define inw(reg)	INPORT16(reg)
#define outl(reg,val)	OUTPORT32(reg,val)
#define inl(reg)	INPORT32(reg)


/*
 * PCI-Memory IO access macros.
 */
#define VID_WR08(p,i,val)  (((uint8_t *)(p))[(i)]=(val))
#define VID_RD08(p,i)	   (((uint8_t *)(p))[(i)])

#define VID_WR32(p,i,val)  (((uint32_t *)(p))[(i)/4]=(val))
#define VID_RD32(p,i)	   (((uint32_t *)(p))[(i)/4])

#ifndef USE_RMW_CYCLES
/*
 * Can be used to inhibit READ-MODIFY-WRITE cycles. On by default.
 */

#define MEM_BARRIER() __asm__ __volatile__ ("" : : : "memory")

#undef	VID_WR08
#define VID_WR08(p,i,val) ({ MEM_BARRIER(); ((uint8_t *)(p))[(i)]=(val); })
#undef	VID_RD08
#define VID_RD08(p,i)     ({ MEM_BARRIER(); ((uint8_t *)(p))[(i)]; })

#undef	VID_WR16
#define VID_WR16(p,i,val) ({ MEM_BARRIER(); ((uint16_t *)(p))[(i)/2]=(val); })
#undef	VID_RD16
#define VID_RD16(p,i)     ({ MEM_BARRIER(); ((uint16_t *)(p))[(i)/2]; })

#undef	VID_WR32
#define VID_WR32(p,i,val) ({ MEM_BARRIER(); ((uint32_t *)(p))[(i)/4]=(val); })
#undef	VID_RD32
#define VID_RD32(p,i)     ({ MEM_BARRIER(); ((uint32_t *)(p))[(i)/4]; })
#endif /* USE_RMW_CYCLES */

#define VID_AND32(p,i,val) VID_WR32(p,i,VID_RD32(p,i)&(val))
#define VID_OR32(p,i,val)  VID_WR32(p,i,VID_RD32(p,i)|(val))
#define VID_XOR32(p,i,val) VID_WR32(p,i,VID_RD32(p,i)^(val))


/* from x driver */

#define VGAIN8(addr) VID_RD08(info->control_base+0x8000, addr)
#define VGAIN16(addr) VID_RD16(info->control_base+0x8000, addr)
#define VGAIN(addr) VID_RD32(info->control_base+0x8000, addr)

#define VGAOUT8(addr,val) VID_WR08(info->control_base+0x8000, addr, val)
#define VGAOUT16(addr,val) VID_WR16(info->control_base+0x8000, addr, val)
#define VGAOUT(addr,val) VID_WR32(info->control_base+0x8000, addr, val)

#define INREG(addr) VID_RD32(info->control_base, addr)
#define OUTREG(addr,val) VID_WR32(info->control_base, addr, val)
#define INREG8(addr) VID_RD08(info->control_base, addr)
#define OUTREG8(addr,val) VID_WR08(info->control_base, addr, val)
#define INREG16(addr) VID_RD16(info->control_base, addr)
#define OUTREG16(addr,val) VID_WR16(info->control_base, addr, val)

#define ALIGN_TO(v, n) (((v) + (n-1)) & ~(n-1))


void debugout(unsigned int addr, unsigned int val);


struct savage_chip {
	volatile uint32_t *PMC;	   /* general control			*/
	volatile uint32_t *PME;	   /* multimedia port			*/
	volatile uint32_t *PFB;	   /* framebuffer control		*/
	volatile uint32_t *PVIDEO; /* overlay control			*/
	volatile uint8_t  *PCIO;   /* SVGA (CRTC, ATTR) registers	*/
	volatile uint8_t  *PVIO;   /* SVGA (MISC, GRAPH, SEQ) registers */
	volatile uint32_t *PRAMIN; /* instance memory			*/
	volatile uint32_t *PRAMHT; /* hash table			*/
	volatile uint32_t *PRAMFC; /* fifo context table		*/
	volatile uint32_t *PRAMRO; /* fifo runout table			*/
	volatile uint32_t *PFIFO;  /* fifo control region		*/
	volatile uint32_t *FIFO;   /* fifo channels (USER)		*/
	volatile uint32_t *PGRAPH; /* graphics engine                   */

	int arch;		   /* compatible NV_ARCH_XX define */
	unsigned long fbsize;		   /* framebuffer size		   */
	void (* lock) (struct savage_chip *, int);
};
typedef struct savage_chip savage_chip;


struct savage_info {
    unsigned int use_colorkey;    
    unsigned int colorkey; /* saved xv colorkey*/
    unsigned int vidixcolorkey; /*currently used colorkey*/
    unsigned int depth; 
    unsigned int bpp; 
    unsigned int videoFlags;
    unsigned int format;
    unsigned int pitch;
    unsigned int blendBase;
    unsigned int lastKnownPitch;
    unsigned int displayWidth, displayHeight;
    unsigned int brightness,hue,saturation,contrast;
    unsigned int src_w,src_h;
    unsigned int drw_w,drw_h;  /*scaled width && height*/
    unsigned int wx,wy;                /*window x && y*/
    unsigned int screen_x;            /*screen width*/
    unsigned int screen_y;            /*screen height*/
    unsigned long buffer_size;		 /* size of the image buffer	       */
    struct savage_chip chip;	 /* NV architecture structure		       */
    void* video_base;		 /* virtual address of control region	       */
    void* control_base;		 /* virtual address of fb region	       */
    unsigned long picture_base;	 /* direct pointer to video picture	       */
    unsigned long picture_offset;	 /* offset of video picture in frame buffer    */
//	struct savage_dma dma;           /* DMA structure                              */
    unsigned int cur_frame;
    unsigned int num_frames;             /* number of buffers                          */
    int bps;			/* bytes per line */
  void (*SavageWaitIdle) ();
  void (*SavageWaitFifo) (int space);
};
typedef struct savage_info savage_info;


static savage_info* info;


/**
 * @brief Unichrome driver vidix capabilities.
 */
static vidix_capability_t savage_cap = {
  "Savage/ProSavage/Twister vidix",
  "Reza Jelveh <reza.jelveh@tuhh.de>",
  TYPE_OUTPUT,
  {0, 0, 0, 0},
  4096,
  4096,
  4,
  4,
  -1,
  FLAG_UPSCALER | FLAG_DOWNSCALER,
  VENDOR_S3_INC,
  -1,
  {0, 0, 0, 0}
};

struct savage_cards {
  unsigned short chip_id;
  unsigned short arch;
};


static
unsigned int GetBlendForFourCC( int id )
{
    switch( id ) {
	case IMGFMT_YUY2:
	case IMGFMT_YV12:
	case IMGFMT_I420:
	    return 1;
	case IMGFMT_Y211:
	    return 4;
	case IMGFMT_RGB15:
	    return 3;
	case IMGFMT_RGB16:
	    return 5;
        default:
	    return 0;
    }
}

/**
 * @brief list of card IDs compliant with the Unichrome driver .
 */
static struct savage_cards savage_card_ids[] = {
	/*[ProSavage PN133] AGP4X VGA Controller (Twister)*/
	{ PCI_CHIP_S3TWISTER_P, 	       		S3_PROSAVAGE },
	/*[ProSavage KN133] AGP4X VGA Controller (TwisterK)*/
	{ PCI_CHIP_S3TWISTER_K, 	       		S3_PROSAVAGE },
	/*ProSavage DDR*/
	{ PCI_CHIP_PROSAVAGE_DDR	, 		       		S3_PROSAVAGE },
	/*[ProSavageDDR P4M266 K] */
	{ PCI_CHIP_PROSAVAGE_DDRK	, 			S3_PROSAVAGE },
};

static void SavageSetColorOld(void)
{


  if( 
  (info->format == IMGFMT_RGB15) ||
	(info->format == IMGFMT_RGB16)
    )
    {
  OUTREG( COLOR_ADJUSTMENT_REG, 0 );
    }
    else
    {
        /* Change 0..255 into 0..15 */
  long sat = info->saturation * 16 / 256;
  double hue = info->hue * 0.017453292;
  unsigned long hs1 = ((long)(sat * cos(hue))) & 0x1f;
  unsigned long hs2 = ((long)(sat * sin(hue))) & 0x1f;

  OUTREG( COLOR_ADJUSTMENT_REG, 
      0x80008000 |
      (info->brightness + 128) |
      ((info->contrast & 0xf8) << (12-7)) | 
      (hs1 << 16) |
      (hs2 << 24)
  );
  debugout( COLOR_ADJUSTMENT_REG, 
      0x80008000 |
      (info->brightness + 128) |
      ((info->contrast & 0xf8) << (12-7)) | 
      (hs1 << 16) |
      (hs2 << 24)
  );
  
  }
}

static void SavageSetColorKeyOld(void)
{
    int red, green, blue;

    /* Here, we reset the colorkey and all the controls. */

    red = (info->vidixcolorkey & 0x00FF0000) >> 16;
    green = (info->vidixcolorkey & 0x0000FF00) >> 8;
    blue = info->vidixcolorkey & 0x000000FF;

    if( !info->vidixcolorkey ) {
      printf("SavageSetColorKey disabling colorkey\n");
      OUTREG( COL_CHROMA_KEY_CONTROL_REG, 0 );
      OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 0 );
      OUTREG( BLEND_CONTROL_REG, 0 );
    }
    else {
	switch (info->depth) {
		// FIXME: isnt fixed yet
	case 8:
	    OUTREG( COL_CHROMA_KEY_CONTROL_REG,
		0x37000000 | (info->vidixcolorkey & 0xFF) );
	    OUTREG( CHROMA_KEY_UPPER_BOUND_REG,
		0x00000000 | (info->vidixcolorkey & 0xFF) );
	    break;
	case 15:
			/* 15 bpp 555 */
      red&=0x1f;
      green&=0x1f;
      blue&=0x1f;
	    OUTREG( COL_CHROMA_KEY_CONTROL_REG, 
		0x05000000 | (red<<19) | (green<<11) | (blue<<3) );
	    OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 
		0x00000000 | (red<<19) | (green<<11) | (blue<<3) );
	    break;
	case 16:
			/* 16 bpp 565 */
      red&=0x1f;
      green&=0x3f;
      blue&=0x1f;
	    OUTREG( COL_CHROMA_KEY_CONTROL_REG, 
		0x16000000 | (red<<19) | (green<<10) | (blue<<3) );
	    OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 
		0x00020002 | (red<<19) | (green<<10) | (blue<<3) );
	    break;
	case 24:
			/* 24 bpp 888 */
	    OUTREG( COL_CHROMA_KEY_CONTROL_REG, 
		0x17000000 | (red<<16) | (green<<8) | (blue) );
	    OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 
		0x00000000 | (red<<16) | (green<<8) | (blue) );
	    break;
	}    

	/* We use destination colorkey */
	OUTREG( BLEND_CONTROL_REG, 0x05000000 );
  }
}


static void
SavageDisplayVideoOld(void)
{
    int vgaCRIndex, vgaCRReg, vgaIOBase;
    unsigned int ssControl;
    int cr92;


    vgaIOBase = 0x3d0;
    vgaCRIndex = vgaIOBase + 4;
    vgaCRReg = vgaIOBase + 5;

//    if( psav->videoFourCC != id )
//	SavageStreamsOff(pScrn);

    if( !info->videoFlags & VF_STREAMS_ON )
    {
				SavageStreamsOn();
	//	SavageResetVideo();
        SavageSetColorOld();
				SavageSetColorKeyOld();
    }
   



    /* Set surface format. */

		OUTREG(SSTREAM_CONTROL_REG,GetBlendForFourCC(info->format) << 24 | info->src_w);

		debugout(SSTREAM_CONTROL_REG,GetBlendForFourCC(info->format) << 24 | info->src_w);

    /* Calculate horizontal scale factor. */

    //FIXME: enable scaling
    OUTREG(SSTREAM_STRETCH_REG, (info->src_w << 15) / info->drw_w );
//    debugout(SSTREAM_STRETCH_REG, 1 << 15);

    OUTREG(SSTREAM_LINES_REG, info->src_h );
    debugout(SSTREAM_LINES_REG, info->src_h );


    OUTREG(SSTREAM_VINITIAL_REG, 0 );
    debugout(SSTREAM_VINITIAL_REG, 0 );
    /* Calculate vertical scale factor. */

//    OUTREG(SSTREAM_VSCALE_REG, 1 << 15);
    OUTREG(SSTREAM_VSCALE_REG, VSCALING(info->src_h,info->drw_h) );
    debugout(SSTREAM_VSCALE_REG, VSCALING(info->src_h,info->drw_h) );
//    OUTREG(SSTREAM_VSCALE_REG, (info->src_h << 15) / info->drw_h );

    /* Set surface location and stride. */

    OUTREG(SSTREAM_FBADDR0_REG, info->picture_offset  );
    debugout(SSTREAM_FBADDR0_REG, info->picture_offset  );

    OUTREG(SSTREAM_FBADDR1_REG, 0 );
    debugout(SSTREAM_FBADDR1_REG, 0 );
    
    OUTREG(SSTREAM_STRIDE_REG, info->pitch );
    debugout(SSTREAM_STRIDE_REG, info->pitch );

    OUTREG(SSTREAM_WINDOW_START_REG, OS_XY(info->wx, info->wy) );
    debugout(SSTREAM_WINDOW_START_REG, OS_XY(info->wx, info->wy) );
    OUTREG(SSTREAM_WINDOW_SIZE_REG, OS_WH(info->drw_w, info->drw_h) );
    debugout(SSTREAM_WINDOW_SIZE_REG, OS_WH(info->drw_w, info->drw_h) );



    ssControl = 0;

    if( info->src_w > (info->drw_w << 1) )
    {
	/* BUGBUG shouldn't this be >=?  */
	if( info->src_w <= (info->drw_w << 2) )
	    ssControl |= HDSCALE_4;
	else if( info->src_w > (info->drw_w << 3) )
	    ssControl |= HDSCALE_8;
	else if( info->src_w > (info->drw_w << 4) )
	    ssControl |= HDSCALE_16;
	else if( info->src_w > (info->drw_w << 5) )
	    ssControl |= HDSCALE_32;
	else if( info->src_w > (info->drw_w << 6) )
	    ssControl |= HDSCALE_64;
    }

    ssControl |= info->src_w;
    ssControl |= (1 << 24);

    //FIXME: enable scaling
    OUTREG(SSTREAM_CONTROL_REG, ssControl);
    debugout(SSTREAM_CONTROL_REG, ssControl);

		// FIXME: this should actually be enabled
		
    info->pitch = (info->pitch + 7) / 8;
    VGAOUT8(vgaCRIndex, 0x92);
    cr92 = VGAIN8(vgaCRReg);
    VGAOUT8(vgaCRReg, (cr92 & 0x40) | (info->pitch >> 8) | 0x80);
    VGAOUT8(vgaCRIndex, 0x93);
    VGAOUT8(vgaCRReg, info->pitch);
    OUTREG(STREAMS_FIFO_REG, 2 | 25 << 5 | 32 << 11);
		
    
    

}

static void SavageInitStreamsOld(void)
{
    /*unsigned long jDelta;*/
    unsigned long format = 0;

    /*
     * For the OLD streams engine, several of these registers
     * cannot be touched unless streams are on.  Seems backwards to me;
     * I'd want to set 'em up, then cut 'em loose.
     */


	/*jDelta = pScrn->displayWidth * (pScrn->bitsPerPixel + 7) / 8;*/
	switch( info->depth ) {
	    case  8: format = 0 << 24; break;
	    case 15: format = 3 << 24; break;
	    case 16: format = 5 << 24; break;
	    case 24: format = 7 << 24; break;
	}
#warning enable this again
	OUTREG(PSTREAM_FBSIZE_REG, 
		info->screen_y * info->screen_x * (info->bpp >> 3));
    
    OUTREG( PSTREAM_WINDOW_START_REG, OS_XY(0,0) );
    OUTREG( PSTREAM_WINDOW_SIZE_REG, OS_WH(info->screen_x, info->screen_y) );
    OUTREG( PSTREAM_FBADDR1_REG, 0 ); 
    /*OUTREG( PSTREAM_STRIDE_REG, jDelta );*/
    OUTREG( PSTREAM_CONTROL_REG, format );
    OUTREG( PSTREAM_FBADDR0_REG, 0 );
		
    /*OUTREG( PSTREAM_FBSIZE_REG, jDelta * pScrn->virtualY >> 3 );*/

    OUTREG( COL_CHROMA_KEY_CONTROL_REG, 0 );
    OUTREG( SSTREAM_CONTROL_REG, 0 );
    OUTREG( CHROMA_KEY_UPPER_BOUND_REG, 0 );
    OUTREG( SSTREAM_STRETCH_REG, 0 );
    OUTREG( COLOR_ADJUSTMENT_REG, 0 );
    OUTREG( BLEND_CONTROL_REG, 1 << 24 );
    OUTREG( DOUBLE_BUFFER_REG, 0 );
    OUTREG( SSTREAM_FBADDR0_REG, 0 );
    OUTREG( SSTREAM_FBADDR1_REG, 0 );
    OUTREG( SSTREAM_FBADDR2_REG, 0 );
    OUTREG( SSTREAM_FBSIZE_REG, 0 );
    OUTREG( SSTREAM_STRIDE_REG, 0 );
    OUTREG( SSTREAM_VSCALE_REG, 0 );
    OUTREG( SSTREAM_LINES_REG, 0 );
    OUTREG( SSTREAM_VINITIAL_REG, 0 );
#warning is this needed?
    OUTREG( SSTREAM_WINDOW_START_REG, OS_XY(0xfffe, 0xfffe) );
    OUTREG( SSTREAM_WINDOW_SIZE_REG, OS_WH(10,2) );

}

void 
SavageStreamsOn(void)
{
     unsigned char jStreamsControl;
     unsigned short vgaCRIndex = 0x3d0 + 4;
     unsigned short vgaCRReg = 0x3d0 + 5;

//    xf86ErrorFVerb(STREAMS_TRACE, "SavageStreamsOn\n" );

    /* Sequence stolen from streams.c in M7 NT driver */


		enable_app_io ();

    /* Unlock extended registers. */

	/* FIXME: it looks like mmaped io is broken with vgaout16  */
    VGAOUT16(vgaCRIndex, 0x4838 );
    VGAOUT16(vgaCRIndex, 0xa039);
    VGAOUT16(0x3c4, 0x0608);

		
	
    VGAOUT8( vgaCRIndex, EXT_MISC_CTRL2 );

    if( S3_SAVAGE_MOBILE_SERIES(info->chip.arch) )
    {
//	SavageInitStreamsNew( pScrn );

	jStreamsControl = VGAIN8( vgaCRReg ) | ENABLE_STREAM1;

	    /* Wait for VBLANK. */	
	    VerticalRetraceWait();
	    /* Fire up streams! */
	    VGAOUT16( vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2 );
	/* These values specify brightness, contrast, saturation and hue. */
	    OUTREG( SEC_STREAM_COLOR_CONVERT1, 0x0000C892 );
	    OUTREG( SEC_STREAM_COLOR_CONVERT2, 0x00039F9A );
	    OUTREG( SEC_STREAM_COLOR_CONVERT3, 0x01F1547E );
    }
    else if (info->chip.arch == S3_SAVAGE2000)
    {
//	SavageInitStreams2000( pScrn );

	jStreamsControl = VGAIN8( vgaCRReg ) | ENABLE_STREAM1;

	/* Wait for VBLANK. */	
	VerticalRetraceWait();
	/* Fire up streams! */
	VGAOUT16( vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2 );
	/* These values specify brightness, contrast, saturation and hue. */
	OUTREG( SEC_STREAM_COLOR_CONVERT0_2000, 0x0000C892 );
	OUTREG( SEC_STREAM_COLOR_CONVERT1_2000, 0x00033400 );
	OUTREG( SEC_STREAM_COLOR_CONVERT2_2000, 0x000001CF );
	OUTREG( SEC_STREAM_COLOR_CONVERT3_2000, 0x01F1547E );
    }
    else
    {
	jStreamsControl = VGAIN8( vgaCRReg ) | ENABLE_STREAMS_OLD;

	/* Wait for VBLANK. */
	
	VerticalRetraceWait();

	/* Fire up streams! */

	VGAOUT16( vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2 );

	SavageInitStreamsOld( );
    }

    /* Wait for VBLANK. */
    
    VerticalRetraceWait();

    /* Turn on secondary stream TV flicker filter, once we support TV. */

    /* SR70 |= 0x10 */

    info->videoFlags |= VF_STREAMS_ON;

}




static void savage_getscreenproperties(struct savage_info *info){
  unsigned char bpp=0;

  uint32_t vgaIOBase, vgaCRIndex, vgaCRReg;

  vgaIOBase = 0x3d0;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;


  /* a little reversed from x driver source code */
  VGAOUT8(vgaCRIndex, 0x67);
  bpp = VGAIN8(vgaCRReg);


  switch (bpp&0xf0) {
  case 0x00:
  case 0x10:
      info->depth=8;
      info->bpp=8;
      break;
  case 0x20:
  case 0x30:
      info->depth=15;
      info->bpp=16;
      break;
  case 0x40:
  case 0x50:
      info->depth=16;
      info->bpp=16;
      break;
  case 0x70:
  case 0xd0:
      info->depth=24;
      info->bpp=32;
      break;


  }


  VGAOUT8(vgaCRIndex, 0x1);
  info->screen_x = (1 + VGAIN8(vgaCRReg))  <<3;
  /*get screen height*/
  /* get first 8 bits in VT_DISPLAY_END*/
  VGAOUT8(0x03D4, 0x12);
  info->screen_y = VGAIN8(0x03D5);
  VGAOUT8(0x03D4,0x07);
  /* get 9th bit in CRTC_OVERFLOW*/
  info->screen_y |= (VGAIN8(0x03D5) &0x02)<<7;
  /* and the 10th in CRTC_OVERFLOW*/
  info->screen_y |=(VGAIN8(0x03D5) &0x40)<<3;
  ++info->screen_y;

	printf("screen_x = %d, screen_y = %d, bpp = %d\n",info->screen_x,info->screen_y,info->bpp);
}


static void SavageStreamsOff(void)
{
    unsigned char jStreamsControl;
    unsigned short vgaCRIndex = 0x3d0 + 4;
    unsigned short vgaCRReg = 0x3d0 + 5;


    /* Unlock extended registers. */

    VGAOUT16(vgaCRIndex, 0x4838);
    VGAOUT16(vgaCRIndex, 0xa039);
    VGAOUT16(0x3c4, 0x0608);

    VGAOUT8( vgaCRIndex, EXT_MISC_CTRL2 );
    if( S3_SAVAGE_MOBILE_SERIES(info->chip.arch)  ||
        (info->chip.arch == S3_SUPERSAVAGE) ||
        (info->chip.arch == S3_SAVAGE2000) )
	jStreamsControl = VGAIN8( vgaCRReg ) & NO_STREAMS;
    else
	jStreamsControl = VGAIN8( vgaCRReg ) & NO_STREAMS_OLD;

    /* Wait for VBLANK. */

    VerticalRetraceWait();

    /* Kill streams. */

    VGAOUT16(vgaCRIndex, (jStreamsControl << 8) | EXT_MISC_CTRL2 );

    VGAOUT16(vgaCRIndex, 0x0093 );
    VGAOUT8( vgaCRIndex, 0x92 );
    VGAOUT8( vgaCRReg, VGAIN8(vgaCRReg) & 0x40 );

    info->videoFlags &= ~VF_STREAMS_ON;
}

/**
 * @brief Find chip index in Unichrome compliant devices list.
 *
 * @param chip_id PCI device ID.
 *
 * @returns index position in savage_card_ids if successful.
 *          -1 if chip_id is not a compliant chipset ID.
 */

static int find_chip(unsigned chip_id){
  unsigned i;
  for(i = 0;i < sizeof(savage_card_ids)/sizeof(struct savage_cards);i++)
  {
    if(chip_id == savage_card_ids[i].chip_id)return i;
  }
  return -1;
}

/**
 * @brief Probe hardware to find some useable chipset.
 *
 * @param verbose specifies verbose level.
 * @param force specifies force mode : driver should ignore
 *              device_id (danger but useful for new devices)
 *
 * @returns 0 if it can handle something in PC.
 *          a negative error code otherwise.
 */

static int savage_probe(int verbose, int force){
    pciinfo_t lst[MAX_PCI_DEVICES];
    unsigned i,num_pci;
    int err;

    if (force)
	    printf("[savage_vid]: warning: forcing not supported yet!\n");
    err = pci_scan(lst,&num_pci);
    if(err){
	printf("[savage_vid] Error occurred during pci scan: %s\n",strerror(err));
	return err;
    }
    else {
	err = ENXIO;
	for(i=0; i < num_pci; i++){
	    if(lst[i].vendor == VENDOR_S3_INC) {
		int idx;
		const char *dname;
		idx = find_chip(lst[i].device);
		if(idx == -1)
		    continue;
		dname = pci_device_name(lst[i].vendor, lst[i].device);
		dname = dname ? dname : "Unknown chip";
		printf("[savage_vid] Found chip: %s\n", dname);
		// FIXME: whats wrong here?
		if ((lst[i].command & PCI_COMMAND_IO ) == 0){
			printf("[savage_vid] Device is disabled, ignoring\n");
			continue;
		}
		savage_cap.device_id = lst[i].device;
		err = 0;
		memcpy(&pci_info, &lst[i], sizeof(pciinfo_t));
		break;
	    }
	}
    }
    if(err && verbose) printf("[savage_vid] Can't find chip\n");
    return err;
}

/**
 * @brief Initializes driver.
 *
 * @returns 0 if ok.
 *          a negative error code otherwise.
 */
static int
savage_init (void)
{
	int mtrr;
  unsigned char config1, tmp;

  static unsigned char RamSavage3D[] = { 8, 4, 4, 2 };
  static unsigned char RamSavage4[] =  { 2, 4, 8, 12, 16, 32, 64, 32 };
  static unsigned char RamSavageMX[] = { 2, 8, 4, 16, 8, 16, 4, 16 };
  static unsigned char RamSavageNB[] = { 0, 2, 4, 8, 16, 32, 16, 2 };

  int videoRam;

  uint32_t   vgaIOBase, vgaCRIndex, vgaCRReg ;

  unsigned char val;

  vgaIOBase = 0x3d0;
  vgaCRIndex = vgaIOBase + 4;
  vgaCRReg = vgaIOBase + 5;

	fprintf(stderr, "vixInit enter \n");
//	//getc(stdin);
	
  info = calloc(1,sizeof(savage_info));
  

  /* need this if we want direct outb and inb access? */
  enable_app_io ();

  /* 12mb + 32kb ? */
  /* allocate some space for control registers */
  info->chip.arch =  savage_card_ids[find_chip(pci_info.device)].arch;  

  if (info->chip.arch == S3_SAVAGE3D) {
      info->control_base = map_phys_mem(pci_info.base0+SAVAGE_NEWMMIO_REGBASE_S3, SAVAGE_NEWMMIO_REGSIZE);
  }
  else {
      info->control_base = map_phys_mem(pci_info.base0+SAVAGE_NEWMMIO_REGBASE_S4, SAVAGE_NEWMMIO_REGSIZE);
  }

//  info->chip.PCIO   = (uint8_t *)  (info->control_base + SAVAGE_NEWMMIO_VGABASE);

  // FIXME: enable mmio?
  val = VGAIN8 (0x3c3);
  VGAOUT8 (0x3c3, val | 0x01);
  val = VGAIN8 (0x3cc);
  VGAOUT8 (0x3c2, val | 0x01);

  if (info->chip.arch >= S3_SAVAGE4)
	{
		VGAOUT8 (0x3d4, 0x40);
		val = VGAIN8 (0x3d5);
		VGAOUT8 (0x3d5, val | 1);
	}



  /* unprotect CRTC[0-7] */
  VGAOUT8(vgaCRIndex, 0x11);
  tmp = VGAIN8(vgaCRReg);
//  printf("$########## tmp = %d\n",tmp);
  VGAOUT8(vgaCRReg, tmp & 0x7f);


  /* unlock extended regs */
  VGAOUT16(vgaCRIndex, 0x4838);
  VGAOUT16(vgaCRIndex, 0xa039);
  VGAOUT16(0x3c4, 0x0608);

  VGAOUT8(vgaCRIndex, 0x40);
  tmp = VGAIN8(vgaCRReg);
  VGAOUT8(vgaCRReg, tmp & ~0x01);

  /* unlock sys regs */
  VGAOUT8(vgaCRIndex, 0x38);
  VGAOUT8(vgaCRReg, 0x48);

  /* Unlock system registers. */
  VGAOUT16(vgaCRIndex, 0x4838);

  /* Next go on to detect amount of installed ram */

  VGAOUT8(vgaCRIndex, 0x36);            /* for register CR36 (CONFG_REG1), */
  config1 = VGAIN8(vgaCRReg);           /* get amount of vram installed */


  switch( info->chip.arch ) {
    case S3_SAVAGE3D:
      videoRam = RamSavage3D[ (config1 & 0xC0) >> 6 ] * 1024;
      break;

    case S3_SAVAGE4:
		/* 
			* The Savage4 has one ugly special case to consider.  On
			* systems with 4 banks of 2Mx32 SDRAM, the BIOS says 4MB
			* when it really means 8MB.  Why do it the same when you
			* can do it different...
			*/
			VGAOUT8(0x3d4, 0x68);  /* memory control 1 */
			if( (VGAIN8(0x3d5) & 0xC0) == (0x01 << 6) )
				RamSavage4[1] = 8;

			/*FALLTHROUGH*/

		case S3_SAVAGE2000:
			videoRam = RamSavage4[ (config1 & 0xE0) >> 5 ] * 1024;
			break;

		case S3_SAVAGE_MX:
			videoRam = RamSavageMX[ (config1 & 0x0E) >> 1 ] * 1024;
			break;

		case S3_PROSAVAGE:
			videoRam = RamSavageNB[ (config1 & 0xE0) >> 5 ] * 1024;
			break;

		default:
			/* How did we get here? */
			videoRam = 0;
			break;
	}


	printf("###### videoRam = %d\n",videoRam);
	info->chip.fbsize = videoRam * 1024;


  /* reset graphics engine to avoid memory corruption */
/*  VGAOUT8 (0x3d4, 0x66);
  cr66 = VGAIN8 (0x3d5);
  VGAOUT8 (0x3d5, cr66 | 0x02);
  udelay (10000);

  VGAOUT8 (0x3d4, 0x66);
  VGAOUT8 (0x3d5, cr66 & ~0x02); */ // clear reset flag
 /* udelay (10000); */

	/* This maps framebuffer @6MB, thus 2MB are left for video. */
	if (info->chip.arch == S3_SAVAGE3D) {
		info->video_base = map_phys_mem(pci_info.base0, info->chip.fbsize);
		info->picture_offset = 1024*768* 4 * ((info->chip.fbsize > 4194304)?2:1);
	}
	else {
		info->video_base = map_phys_mem(pci_info.base1, info->chip.fbsize);
		info->picture_offset = info->chip.fbsize - FRAMEBUFFER_SIZE;
//			info->picture_offset = 1024*1024* 4 * 2;
	}
	if ( info->video_base == NULL){
		printf("errno = %s\n",  strerror(errno));
		return -1; 
	}


	info->picture_base = (uint32_t) info->video_base + info->picture_offset;

	if ( info->chip.arch == S3_SAVAGE3D ){
		mtrr = mtrr_set_type(pci_info.base0, info->chip.fbsize, MTRR_TYPE_WRCOMB);
	}
	else{ 
		mtrr = mtrr_set_type(pci_info.base1, info->chip.fbsize, MTRR_TYPE_WRCOMB);
	}

	if (mtrr!= 0)
		printf("[savage_vid] unable to setup MTRR: %s\n", strerror(mtrr));
	else
		printf("[savage_vid] MTRR set up\n");

	/* This may trash your screen for resolutions greater than 1024x768, sorry. */
	

	savage_getscreenproperties(info);
//	return -1;
	info->videoFlags = 0;

	SavageStreamsOn();
	//getc(stdin);
	//FIXME ADD
  return 0;
}

/**
 * @brief Destroys driver.
 */
static void
savage_destroy (void)
{
	unmap_phys_mem(info->video_base, info->chip.fbsize);
	unmap_phys_mem(info->control_base, SAVAGE_NEWMMIO_REGSIZE);
	//FIXME ADD
}

/**
 * @brief Get chipset's hardware capabilities.
 *
 * @param to Pointer to the vidix_capability_t structure to be filled.
 *
 * @returns 0.
 */
static int
savage_get_caps (vidix_capability_t * to)
{
  memcpy (to, &savage_cap, sizeof (vidix_capability_t));
  return 0;
}

/**
 * @brief Report if the video FourCC is supported by hardware.
 *
 * @param fourcc input image format.
 *
 * @returns 1 if the fourcc is supported.
 *          0 otherwise.
 */
static int
is_supported_fourcc (uint32_t fourcc)
{
  switch (fourcc)
    {
//FIXME: YV12 isnt working properly yet			
//    case IMGFMT_YV12:
//    case IMGFMT_I420:
    case IMGFMT_UYVY:
    case IMGFMT_YVYU:
    case IMGFMT_YUY2:
    case IMGFMT_RGB15:
    case IMGFMT_RGB16:
//    case IMGFMT_BGR32:
      return 1;
    default:
      return 0;
    }
}

/**
 * @brief Try to configure video memory for given fourcc.
 *
 * @param to Pointer to the vidix_fourcc_t structure to be filled.
 *
 * @returns 0 if ok.
 *          errno otherwise.
 */
static int
savage_query_fourcc (vidix_fourcc_t * to)
{
  if (is_supported_fourcc (to->fourcc))
    {
      to->depth = VID_DEPTH_1BPP | VID_DEPTH_2BPP |
	VID_DEPTH_4BPP | VID_DEPTH_8BPP |
	VID_DEPTH_12BPP | VID_DEPTH_15BPP |
	VID_DEPTH_16BPP | VID_DEPTH_24BPP | VID_DEPTH_32BPP;
      to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK | VID_CAP_COLORKEY;
      return 0;
    }
  else
    to->depth = to->flags = 0;

  return ENOSYS;
}

/**
 * @brief Get the GrKeys
 *
 * @param grkey Pointer to the vidix_grkey_t structure to be filled by driver.
 *
 * @return 0.
 */
/*int
vixGetGrKeys (vidix_grkey_t * grkey)
{

//  if(info->d_width && info->d_height)savage_overlay_start(info,0);

  return (0);
}
 * */

/**
 * @brief Set the GrKeys
 *
 * @param grkey Colorkey to be set.
 *
 * @return 0.
 */
static int
savage_set_gkeys (const vidix_grkey_t * grkey)
{
  if (grkey->ckey.op == CKEY_FALSE)
  {
    info->use_colorkey = 0;
    info->vidixcolorkey=0;
    printf("[savage_vid] colorkeying disabled\n");
  }
  else {
    info->use_colorkey = 1;
    info->vidixcolorkey = ((grkey->ckey.red<<16)|(grkey->ckey.green<<8)|grkey->ckey.blue);

    printf("[savage_vid] set colorkey 0x%x\n",info->vidixcolorkey);
  }
	//FIXME: freezes if streams arent enabled
  SavageSetColorKeyOld();
  return (0);
}

/**
 * @brief Unichrome driver equalizer capabilities.
 */
static vidix_video_eq_t equal = {
  VEQ_CAP_BRIGHTNESS | VEQ_CAP_SATURATION | VEQ_CAP_HUE,
  300, 100, 0, 0, 0, 0, 0, 0
};


/**
 * @brief Get the equalizer capabilities.
 *
 * @param eq Pointer to the vidix_video_eq_t structure to be filled by driver.
 *
 * @return 0.
 */
static int
savage_get_eq (vidix_video_eq_t * eq)
{
  memcpy (eq, &equal, sizeof (vidix_video_eq_t));
  return 0;
}

/**
 * @brief Set the equalizer capabilities for color correction
 *
 * @param eq equalizer capabilities to be set.
 *
 * @return 0.
 */
static int
savage_set_eq (const vidix_video_eq_t * eq)
{
  return 0;
}

/**
 * @brief Configure driver for playback. Driver should prepare BES.
 *
 * @param info configuration description for playback.
 *
 * @returns  0 in case of success.
 *          -1 otherwise.
 */
static int
savage_config_playback (vidix_playback_t * vinfo)
{
  int uv_size, swap_uv;
  unsigned int i;

  if (!is_supported_fourcc (vinfo->fourcc))
    return -1;



  info->src_w = vinfo->src.w;
  info->src_h = vinfo->src.h;

  info->drw_w = vinfo->dest.w;
  info->drw_h = vinfo->dest.h;
  
  info->wx = vinfo->dest.x;
  info->wy = vinfo->dest.y;
  info->format = vinfo->fourcc;

  info->lastKnownPitch = 0;
  info->brightness = 0;
  info->contrast = 128;
  info->saturation = 128;
  info->hue = 0;


  vinfo->dga_addr=(void*)(info->picture_base);


		  vinfo->offset.y = 0;
		  vinfo->offset.v = 0;
		  vinfo->offset.u = 0;

		  vinfo->dest.pitch.y = 32;
		  vinfo->dest.pitch.u = 32;
		  vinfo->dest.pitch.v = 32;
	//	  vinfo->dest.pitch.u = 0;
	//	  vinfo->dest.pitch.v = 0;
			

   info->pitch = ((info->src_w << 1) + 15) & ~15;

  swap_uv = 0;
  switch (vinfo->fourcc)
  {
	  case IMGFMT_YUY2:
	  case IMGFMT_UYVY:
			
		  info->pitch = ((info->src_w << 1) + (vinfo->dest.pitch.y-1)) & ~(vinfo->dest.pitch.y-1);

			info->pitch = info->src_w << 1;
      info->pitch = ALIGN_TO (info->src_w << 1, 32);
      uv_size = 0;
		  break;
	  case IMGFMT_YV12:
		swap_uv = 1;


	
		/*
			srcPitch = (info->src_w + 3) & ~3;
			vinfo->offset.u = srcPitch * info->src_h;
			srcPitch2 = ((info->src_w >> 1) + 3) & ~3;
			vinfo->offset.v = (srcPitch2 * (info->src_h >> 1)) + vinfo->offset.v;

			vinfo->dest.pitch.y=srcPitch ;
			vinfo->dest.pitch.v=srcPitch2 ;
			vinfo->dest.pitch.u=srcPitch2 ;
			*/
	

      info->pitch = ALIGN_TO (info->src_w, 32);
      uv_size = (info->pitch >> 1) * (info->src_h >> 1);

  vinfo->offset.y = 0;
  vinfo->offset.v = vinfo->offset.y + info->pitch * info->src_h;
  vinfo->offset.u = vinfo->offset.v + uv_size;
  vinfo->frame_size = vinfo->offset.u + uv_size;
/*  YOffs = info->offset.y;
  UOffs = (swap_uv ? vinfo->offset.v : vinfo->offset.u);
  VOffs = (swap_uv ? vinfo->offset.u : vinfo->offset.v);
	*/
//	  vinfo->offset.y = info->src_w;
//	  vinfo->offset.v = vinfo->offset.y + info->src_w /2 * info->src_h;
//	  vinfo->offset.u = vinfo->offset.v + (info->src_w >> 1) * (info->src_h >> 1) ;

		  break;
  }
			info->pitch |= ((info->pitch >> 1) << 16);

		  vinfo->frame_size = info->pitch * info->src_h;

			printf("$#### destination pitch = %u\n", info->pitch&0xffff);




  info->buffer_size = vinfo->frame_size;
  info->num_frames = vinfo->num_frames= (info->chip.fbsize - info->picture_offset)/vinfo->frame_size;
  if(vinfo->num_frames > MAX_FRAMES)vinfo->num_frames = MAX_FRAMES;
//    vinfo->num_frames = 1;
//    printf("[nvidia_vid] Number of frames %i\n",vinfo->num_frames);
  for(i=0;i <vinfo->num_frames;i++)vinfo->offsets[i] = vinfo->frame_size*i;

  return 0;
}

/**
 * @brief Set playback on : driver should activate BES on this call.
 *
 * @return 0.
 */
static int
savage_playback_on (void)
{
 // FIXME: enable
  SavageDisplayVideoOld();
//FIXME ADD
  return 0;
}

/**
 * @brief Set playback off : driver should deactivate BES on this call.
 *
 * @return 0.
 */
static int
savage_playback_off (void)
{
	// otherwise we wont disable streams properly in new xorg
	// FIXME: shouldnt this be enabled?
//  SavageStreamsOn();
  SavageStreamsOff();
//  info->vidixcolorkey=0x0;

//  OUTREG( SSTREAM_WINDOW_START_REG, OS_XY(0xfffe, 0xfffe) );
//  SavageSetColorKeyOld();
//FIXME ADD
  return 0;
}

/**
 * @brief Driver should prepare and activate corresponded frame.
 *
 * @param frame the frame index.
 *
 * @return 0.
 *
 * @note This function is used only for double and triple buffering
 *       and never used for single buffering playback.
 */
#if 0
static int
savage_frame_select (unsigned int frame)
{
////FIXME ADD
//    savage_overlay_start(info, frame);
    //if (info->num_frames >= 1)
//	    info->cur_frame = frame//(frame+1)%info->num_frames;
//
//	savage4_waitidle(info); 
 	
   printf("vixPlaybackFrameSelect Leave\n" );
	 // FIXME: does this work to avoid tearing?
//   VerticalRetraceWait();
   
  return 0;
}

#endif 



void debugout(unsigned int addr, unsigned int val){
	return ;
    switch ( addr ){
	case PSTREAM_CONTROL_REG:
	    fprintf(stderr,"PSTREAM_CONTROL_REG");
	    break;
	case COL_CHROMA_KEY_CONTROL_REG:
	    fprintf(stderr,"COL_CHROMA_KEY_CONTROL_REG");
	    break;
	case SSTREAM_CONTROL_REG:
	    fprintf(stderr,"SSTREAM_CONTROL_REG");
	    break;
	case CHROMA_KEY_UPPER_BOUND_REG:
	    fprintf(stderr,"CHROMA_KEY_UPPER_BOUND_REG");
	    break;
	case SSTREAM_STRETCH_REG:
	    fprintf(stderr,"SSTREAM_STRETCH_REG");
	    break;
	case COLOR_ADJUSTMENT_REG:
	    fprintf(stderr,"COLOR_ADJUSTMENT_REG");
	    break;
	case BLEND_CONTROL_REG:
	    fprintf(stderr,"BLEND_CONTROL_REG");
	    break;
	case PSTREAM_FBADDR0_REG:
	    fprintf(stderr,"PSTREAM_FBADDR0_REG");
	    break;
	case PSTREAM_FBADDR1_REG:
	    fprintf(stderr,"PSTREAM_FBADDR1_REG");
	    break;
	case PSTREAM_STRIDE_REG:
	    fprintf(stderr,"PSTREAM_STRIDE_REG");
	    break;
	case DOUBLE_BUFFER_REG:
	    fprintf(stderr,"DOUBLE_BUFFER_REG");
	    break;
	case SSTREAM_FBADDR0_REG:
	    fprintf(stderr,"SSTREAM_FBADDR0_REG");
	    break;
	case SSTREAM_FBADDR1_REG:
	    fprintf(stderr,"SSTREAM_FBADDR1_REG");
	    break;
	case SSTREAM_STRIDE_REG:
	    fprintf(stderr,"SSTREAM_STRIDE_REG");
	    break;
	case SSTREAM_VSCALE_REG:
	    fprintf(stderr,"SSTREAM_VSCALE_REG");
	    break;
	case SSTREAM_VINITIAL_REG:
	    fprintf(stderr,"SSTREAM_VINITIAL_REG");
	    break;
	case SSTREAM_LINES_REG:
	    fprintf(stderr,"SSTREAM_LINES_REG");
	    break;
	case STREAMS_FIFO_REG:
	    fprintf(stderr,"STREAMS_FIFO_REG");
	    break;
	case PSTREAM_WINDOW_START_REG:
	    fprintf(stderr,"PSTREAM_WINDOW_START_REG");
	    break;
	case PSTREAM_WINDOW_SIZE_REG:
	    fprintf(stderr,"PSTREAM_WINDOW_SIZE_REG");
	    break;
	case SSTREAM_WINDOW_START_REG:
	    fprintf(stderr,"SSTREAM_WINDOW_START_REG");
	    break;
	case SSTREAM_WINDOW_SIZE_REG:
	    fprintf(stderr,"SSTREAM_WINDOW_SIZE_REG");
	    break;
	case FIFO_CONTROL:
	    fprintf(stderr,"FIFO_CONTROL");
	    break;
	case PSTREAM_FBSIZE_REG:
	    fprintf(stderr,"PSTREAM_FBSIZE_REG");
	    break;
	case SSTREAM_FBSIZE_REG:
	    fprintf(stderr,"SSTREAM_FBSIZE_REG");
	    break;
	case SSTREAM_FBADDR2_REG:
	    fprintf(stderr,"SSTREAM_FBADDR2_REG");
	    break;

    }
    fprintf(stderr,":\t\t 0x%08X = %u\n",val,val);
}

VDXDriver savage_drv = {
  "savage",
  NULL,
  .probe = savage_probe,
  .get_caps = savage_get_caps,
  .query_fourcc = savage_query_fourcc,
  .init = savage_init,
  .destroy = savage_destroy,
  .config_playback = savage_config_playback,
  .playback_on = savage_playback_on,
  .playback_off = savage_playback_off,
#if 0
  .frame_sel = savage_frame_select,
#endif
  .get_eq = savage_get_eq,
  .set_eq = savage_set_eq,
  .set_gkey = savage_set_gkeys,
};
