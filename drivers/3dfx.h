/* 
 *    3dfx.h
 *
 *	Copyright (C) Colin Cross Apr 2000
 *	
 *  mpeg2dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */


#define VOODOO_IO_REG_OFFSET     ((unsigned long int)0x0000000)
#define VOODOO_YUV_REG_OFFSET    ((unsigned long int)0x0080100)
#define VOODOO_AGP_REG_OFFSET    ((unsigned long int)0x0080000)
#define VOODOO_2D_REG_OFFSET     ((unsigned long int)0x0100000)
#define VOODOO_YUV_PLANE_OFFSET  ((unsigned long int)0x0C00000)

#define VOODOO_BLT_FORMAT_YUYV   (8<<16)
#define VOODOO_BLT_FORMAT_16     (3<<16)

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

