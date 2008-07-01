/*
 * Copyright (C) Colin Cross Apr 2000
 * changed by zsteva Aug/Sep 2001, see vo_3dfx.c
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

#ifndef MPLAYER_3DFX_H
#define MPLAYER_3DFX_H

#define VOODOO_IO_REG_OFFSET     ((unsigned long int)0x0000000)
#define VOODOO_YUV_REG_OFFSET    ((unsigned long int)0x0080100)
#define VOODOO_AGP_REG_OFFSET    ((unsigned long int)0x0080000)
#define VOODOO_2D_REG_OFFSET     ((unsigned long int)0x0100000)
#define VOODOO_YUV_PLANE_OFFSET  ((unsigned long int)0x0C00000)

#define VOODOO_BLT_FORMAT_YUYV   (8<<16)
#define VOODOO_BLT_FORMAT_UYVY	 (9<<16)
#define VOODOO_BLT_FORMAT_16     (3<<16)
#define VOODOO_BLT_FORMAT_24	 (4<<16)
#define VOODOO_BLT_FORMAT_32	 (5<<16)

#define VOODOO_YUV_STRIDE        (1024>>2)

struct voodoo_yuv_fb_t {
  uint32_t Y[0x0040000];
  uint32_t U[0x0040000];
  uint32_t V[0x0040000];
};

struct voodoo_yuv_reg_t {
  uint32_t yuvBaseAddr;
  uint32_t yuvStride;
};

struct voodoo_2d_reg_t {
  uint32_t status;
  uint32_t intCtrl;
  uint32_t clip0Min;
  uint32_t clip0Max;
  uint32_t dstBaseAddr;
  uint32_t dstFormat;
  uint32_t srcColorkeyMin;
  uint32_t srcColorkeyMax;
  uint32_t dstColorkeyMin;
  uint32_t dstColorkeyMax;
  signed long bresError0;
  signed long bresError1;
  uint32_t rop;
  uint32_t srcBaseAddr;
  uint32_t commandExtra;
  uint32_t lineStipple;
  uint32_t lineStyle;
  uint32_t pattern0Alias;
  uint32_t pattern1Alias;;
  uint32_t clip1Min;
  uint32_t clip1Max;
  uint32_t srcFormat;
  uint32_t srcSize;
  uint32_t srcXY;
  uint32_t colorBack;
  uint32_t colorFore;
  uint32_t dstSize;
  uint32_t dstXY;
  uint32_t command;
  uint32_t RESERVED1;
  uint32_t RESERVED2;
  uint32_t RESERVED3;
  uint8_t  launchArea[128];
};


struct voodoo_io_reg_t {
  uint32_t status;
  uint32_t pciInit0;
  uint32_t sipMonitor;
  uint32_t lfbMemoryConfig;
  uint32_t miscInit0;
  uint32_t miscInit1;
  uint32_t dramInit0;
  uint32_t dramInit1;
  uint32_t agpInit;
  uint32_t tmuGbeInit;
  uint32_t vgaInit0;
  uint32_t vgaInit1;
  uint32_t dramCommand;
  uint32_t dramData;
  uint32_t RESERVED1;
  uint32_t RESERVED2;

  uint32_t pllCtrl0;
  uint32_t pllCtrl1;
  uint32_t pllCtrl2;
  uint32_t dacMode;
  uint32_t dacAddr;
  uint32_t dacData;

  uint32_t rgbMaxDelta;
  uint32_t vidProcCfg;
  uint32_t hwCurPatAddr;
  uint32_t hwCurLoc;
  uint32_t hwCurC0;
  uint32_t hwCurC1;
  uint32_t vidInFormat;
  uint32_t vidInStatus;
  uint32_t vidSerialParallelPort;
  uint32_t vidInXDecimDeltas;
  uint32_t vidInDecimInitErrs;
  uint32_t vidInYDecimDeltas;
  uint32_t vidPixelBufThold;
  uint32_t vidChromaMin;
  uint32_t vidChromaMax;
  uint32_t vidCurrentLine;
  uint32_t vidScreenSize;
  uint32_t vidOverlayStartCoords;
  uint32_t vidOverlayEndScreenCoord;
  uint32_t vidOverlayDudx;
  uint32_t vidOverlayDudxOffsetSrcWidth;
  uint32_t vidOverlayDvdy;

  uint32_t vga_registers_not_mem_mapped[12];
  uint32_t vidOverlayDvdyOffset;
  uint32_t vidDesktopStartAddr;
  uint32_t vidDesktopOverlayStride;
  uint32_t vidInAddr0;
  uint32_t vidInAddr1;
  uint32_t vidInAddr2;
  uint32_t vidInStride;
  uint32_t vidCurrOverlayStartAddr;
};


struct pioData_t {
  short port;
  short size;
  int device;
  void *value;
};

typedef struct pioData_t pioData;
typedef struct voodoo_2d_reg_t voodoo_2d_reg;
typedef struct voodoo_io_reg_t voodoo_io_reg;
typedef struct voodoo_yuv_reg_t voodoo_yuv_reg;
typedef struct voodoo_yuv_fb_t voodoo_yuv_fb;


/* from linux/driver/video/tdfxfb.c, definition for 3dfx registers.
 *
 * author: Hannu Mallat <hmallat@cc.hut.fi>
 */

#ifndef PCI_DEVICE_ID_3DFX_VOODOO5
#define PCI_DEVICE_ID_3DFX_VOODOO5	0x0009
#endif

/* membase0 register offsets */
#define STATUS				0x00
#define PCIINIT0			0x04
#define SIPMONITOR			0x08
#define LFBMEMORYCONFIG		0x0c
#define MISCINIT0			0x10
#define MISCINIT1			0x14
#define DRAMINIT0			0x18
#define DRAMINIT1			0x1c
#define AGPINIT				0x20
#define TMUGBEINIT			0x24
#define VGAINIT0			0x28
#define VGAINIT1			0x2c
#define DRAMCOMMAND			0x30
#define DRAMDATA			0x34
/* reserved             	0x38 */
/* reserved             	0x3c */
#define PLLCTRL0			0x40
#define PLLCTRL1			0x44
#define PLLCTRL2			0x48
#define DACMODE				0x4c
#define DACADDR				0x50
#define DACDATA				0x54
#define RGBMAXDELTA			0x58
#define VIDPROCCFG			0x5c
#define HWCURPATADDR		0x60
#define HWCURLOC			0x64
#define HWCURC0				0x68
#define HWCURC1				0x6c
#define VIDINFORMAT			0x70
#define VIDINSTATUS			0x74
#define VIDSERPARPORT		0x78
#define VIDINXDELTA			0x7c
#define VIDININITERR		0x80
#define VIDINYDELTA			0x84
#define VIDPIXBUFTHOLD		0x88
#define VIDCHRMIN			0x8c
#define VIDCHRMAX			0x90
#define VIDCURLIN			0x94
#define VIDSCREENSIZE		0x98
#define VIDOVRSTARTCRD		0x9c
#define VIDOVRENDCRD		0xa0
#define VIDOVRDUDX			0xa4
#define VIDOVRDUDXOFF		0xa8
#define VIDOVRDVDY			0xac
/*  ... */
#define VIDOVRDVDYOFF		0xe0
#define VIDDESKSTART		0xe4
#define VIDDESKSTRIDE		0xe8
#define VIDINADDR0			0xec
#define VIDINADDR1			0xf0
#define VIDINADDR2			0xf4
#define VIDINSTRIDE			0xf8
#define VIDCUROVRSTART		0xfc

#define INTCTRL			(0x00100000 + 0x04)
#define CLIP0MIN		(0x00100000 + 0x08)
#define CLIP0MAX		(0x00100000 + 0x0c)
#define DSTBASE			(0x00100000 + 0x10)
#define DSTFORMAT		(0x00100000 + 0x14)
#define SRCCOLORKEYMIN		(0x00100000 + 0x18)
#define SRCCOLORKEYMAX		(0x00100000 + 0x1c)
#define DSTCOLORKEYMIN		(0x00100000 + 0x20)
#define DSTCOLORKEYMAX		(0x00100000 + 0x24)
#define ROP123			(0x00100000 + 0x30)
#define SRCBASE			(0x00100000 + 0x34)
#define COMMANDEXTRA_2D	(0x00100000 + 0x38)
#define CLIP1MIN		(0x00100000 + 0x4c)
#define CLIP1MAX		(0x00100000 + 0x50)
#define SRCFORMAT		(0x00100000 + 0x54)
#define SRCSIZE			(0x00100000 + 0x58)
#define SRCXY			(0x00100000 + 0x5c)
#define COLORBACK		(0x00100000 + 0x60)
#define COLORFORE		(0x00100000 + 0x64)
#define DSTSIZE			(0x00100000 + 0x68)
#define DSTXY			(0x00100000 + 0x6c)
#define COMMAND_2D		(0x00100000 + 0x70)
#define LAUNCH_2D		(0x00100000 + 0x80)

#define COMMAND_3D		(0x00200000 + 0x120)

#define SWAPBUFCMD		(0x00200000 + 0x128)
#define SWAPPENDING		(0x00200000 + 0x24C)
#define LEFTOVBUF		(0x00200000 + 0x250)
#define RIGHTOVBUF		(0x00200000 + 0x254)
#define FBISWAPBUFHIST		(0x00200000 + 0x258)

/* register bitfields (not all, only as needed) */

#define BIT(x) (1UL << (x))

/* COMMAND_2D reg. values */
#define TDFXFB_ROP_COPY         0xcc     // src
#define TDFXFB_ROP_INVERT       0x55     // NOT dst
#define TDFXFB_ROP_XOR          0x66     // src XOR dst
#define TDFXFB_ROP_OR           0xee     // src | dst

#define AUTOINC_DSTX                    BIT(10)
#define AUTOINC_DSTY                    BIT(11)


#define COMMAND_2D_S2S_BITBLT			0x01      // screen to screen
#define COMMAND_2D_S2S_STRECH_BLT		0x02 // BLT + Strech
#define COMMAND_2D_H2S_BITBLT                   0x03       // host to screen
#define COMMAND_2D_FILLRECT			0x05

#define COMMAND_2D_DO_IMMED		        BIT(8) // Do it immediatly



#define COMMAND_3D_NOP				0x00
#define STATUS_RETRACE				BIT(6)
#define STATUS_BUSY					BIT(9)
#define MISCINIT1_CLUT_INV			BIT(0)
#define MISCINIT1_2DBLOCK_DIS		BIT(15)
#define DRAMINIT0_SGRAM_NUM			BIT(26)
#define DRAMINIT0_SGRAM_TYPE		BIT(27)
#define DRAMINIT1_MEM_SDRAM			BIT(30)
#define VGAINIT0_VGA_DISABLE		BIT(0)
#define VGAINIT0_EXT_TIMING			BIT(1)
#define VGAINIT0_8BIT_DAC			BIT(2)
#define VGAINIT0_EXT_ENABLE			BIT(6)
#define VGAINIT0_WAKEUP_3C3			BIT(8)
#define VGAINIT0_LEGACY_DISABLE		BIT(9)
#define VGAINIT0_ALT_READBACK		BIT(10)
#define VGAINIT0_FAST_BLINK			BIT(11)
#define VGAINIT0_EXTSHIFTOUT		BIT(12)
#define VGAINIT0_DECODE_3C6			BIT(13)
#define VGAINIT0_SGRAM_HBLANK_DISABLE	BIT(22)
#define VGAINIT1_MASK				0x1fffff
#define VIDCFG_VIDPROC_ENABLE		BIT(0)
#define VIDCFG_CURS_X11				BIT(1)
#define VIDCFG_HALF_MODE			BIT(4)
#define VIDCFG_DESK_ENABLE			BIT(7)
#define VIDCFG_CLUT_BYPASS			BIT(10)
#define VIDCFG_2X					BIT(26)
#define VIDCFG_HWCURSOR_ENABLE		BIT(27)
#define VIDCFG_PIXFMT_SHIFT				18
#define DACMODE_2X					BIT(0)

/* AGP registers */
#define AGPREQSIZE          (0x0080000 + 0x00)
#define AGPHOSTADDRESSLOW   (0x0080000 + 0x04)
#define AGPHOSTADDRESSHIGH  (0x0080000 + 0x08)
#define AGPGRAPHICSADDRESS  (0x0080000 + 0x0C)
#define AGPGRAPHICSSTRIDE   (0x0080000 + 0x10)
#define AGPMOVECMD          (0x0080000 + 0x14)

/* FIFO registers */
#define CMDBASEADDR0        (0x0080000 + 0x20)
#define CMDBASESIZE0        (0x0080000 + 0x24)
#define CMDBUMP0            (0x0080000 + 0x28)
#define CMDRDPTRL0          (0x0080000 + 0x2C)
#define CMDRDPTRH0          (0x0080000 + 0x30)
#define CMDAMIN0            (0x0080000 + 0x34)
#define CMDAMAX0            (0x0080000 + 0x38)
#define CMDFIFODEPTH0       (0x0080000 + 0x44)
#define CMDHOLECNT0         (0x0080000 + 0x48)


/* YUV reisters */
#define YUVBASEADDRESS (0x0080000 + 0x100)
#define YUVSTRIDE      (0x0080000 + 0x104)

/* VGA rubbish, need to change this for multihead support */
#define MISC_W 	0x3c2
#define MISC_R 	0x3cc
#define SEQ_I 	0x3c4
#define SEQ_D	0x3c5
#define CRT_I	0x3d4
#define CRT_D	0x3d5
#define ATT_IW	0x3c0
#define RAMDAC_R 0x3c7
#define RAMDAC_W 0x3c8
#define RAMDAC_D 0x3c9
#define IS1_R	0x3da
#define GRA_I	0x3ce
#define GRA_D	0x3cf

#ifndef FB_ACCEL_3DFX_BANSHEE 
#define FB_ACCEL_3DFX_BANSHEE 31
#endif

#define TDFXF_HSYNC_ACT_HIGH	0x01
#define TDFXF_HSYNC_ACT_LOW		0x02
#define TDFXF_VSYNC_ACT_HIGH	0x04
#define TDFXF_VSYNC_ACT_LOW		0x08
#define TDFXF_LINE_DOUBLE		0x10
#define TDFXF_VIDEO_ENABLE		0x20

#define TDFXF_HSYNC_MASK		0x03
#define TDFXF_VSYNC_MASK		0x0c

#define XYREG(x,y)		(((((unsigned long)y) & 0x1FFF) << 16) | (((unsigned long)x) & 0x1FFF))

//#define TDFXFB_DEBUG 
#ifdef TDFXFB_DEBUG
#define DPRINTK(a,b...) printk(KERN_DEBUG "fb: %s: " a, __FUNCTION__ , ## b)
#else
#define DPRINTK(a,b...)
#endif 

/* ------------------------------------------------------------------------- */

#endif /* MPLAYER_3DFX_H */
