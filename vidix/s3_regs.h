/*
 * S3 chipsets registers definition.
 *
 * Copyright (C) 2004 Reza Jelveh
 * Thanks to Alex Deucher for Support
 * Trio/Virge support by Michael Kostylev
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

#ifndef MPLAYER_SAVAGE_REGS_H
#define MPLAYER_SAVAGE_REGS_H

#define S3_SAVAGE3D_SERIES(chip) ((chip>=S3_SAVAGE3D) && (chip<=S3_SAVAGE_MX))
#define S3_SAVAGE4_SERIES(chip) ((chip==S3_SAVAGE4) || (chip==S3_PROSAVAGE))
#define	S3_SAVAGE_MOBILE_SERIES(chip) ((chip==S3_SAVAGE_MX) || (chip==S3_SUPERSAVAGE))
#define S3_SAVAGE_SERIES(chip) ((chip>=S3_SAVAGE3D) && (chip<=S3_SAVAGE2000))

/* 
 * Chip tags.  These are used to group the adapters into 
 * related families.
 */
enum S3CHIPTAGS {
    S3_UNKNOWN = 0,
    S3_TRIO64V,
    S3_VIRGE,
    S3_SAVAGE3D,
    S3_SAVAGE_MX,
    S3_SAVAGE4,
    S3_PROSAVAGE,
    S3_SUPERSAVAGE,
    S3_SAVAGE2000,
    S3_LAST
};

#define BIOS_BSIZE			1024
#define BIOS_BASE			0xc0000

#define S3_NEWMMIO_REGBASE		0x1000000	/* 16MB */
#define S3_NEWMMIO_REGSIZE		0x0010000	/* 64KB */
#define S3_NEWMMIO_REGSIZE_SAVAGE	0x0080000	/* 512KB */

#define BASE_FREQ			14.31818	

/*
 * There are two different streams engines used in the S3 line.
 * The old engine is in the Trio64, Virge,
 * Savage3D, Savage4, SavagePro, and SavageTwister.
 * The new engine is in the Savage2000, SavageMX,
 * SavageIX, and SuperSavage.
 */

/* Old engine registers */
#define PSTREAM_CONTROL_REG            0x8180
#define COL_CHROMA_KEY_CONTROL_REG     0x8184
#define SSTREAM_CONTROL_REG            0x8190
#define CHROMA_KEY_UPPER_BOUND_REG     0x8194
#define SSTREAM_STRETCH_REG            0x8198
#define COLOR_ADJUSTMENT_REG           0x819C
#define BLEND_CONTROL_REG              0x81A0
#define PSTREAM_FBADDR0_REG            0x81C0
#define PSTREAM_FBADDR1_REG            0x81C4
#define PSTREAM_STRIDE_REG             0x81C8
#define DOUBLE_BUFFER_REG              0x81CC
#define SSTREAM_FBADDR0_REG            0x81D0
#define SSTREAM_FBADDR1_REG            0x81D4
#define SSTREAM_STRIDE_REG             0x81D8
#define OPAQUE_OVERLAY_CONTROL_REG     0x81DC
#define K1_VSCALE_REG                  0x81E0
#define SSTREAM_VSCALE_REG             0x81E0
#define K2_VSCALE_REG                  0x81E4
#define SSTREAM_VINITIAL_REG           0x81E4
#define DDA_VERT_REG                   0x81E8
#define SSTREAM_LINES_REG              0x81E8
#define STREAMS_FIFO_REG               0x81EC
#define PSTREAM_WINDOW_START_REG       0x81F0
#define PSTREAM_WINDOW_SIZE_REG        0x81F4
#define SSTREAM_WINDOW_START_REG       0x81F8
#define SSTREAM_WINDOW_SIZE_REG        0x81FC
#define FIFO_CONTROL                   0x8200
#define PSTREAM_FBSIZE_REG             0x8300
#define SSTREAM_FBSIZE_REG             0x8304
#define SSTREAM_FBADDR2_REG            0x8308

/* New engine registers */
#define PRI_STREAM_FBUF_ADDR0          0x81c0
#define PRI_STREAM_FBUF_ADDR1          0x81c4
#define PRI_STREAM_STRIDE              0x81c8
#define PRI_STREAM_BUFFERSIZE          0x8214
#define SEC_STREAM_CKEY_LOW            0x8184
#define SEC_STREAM_CKEY_UPPER          0x8194
#define BLEND_CONTROL                  0x8190
#define SEC_STREAM_COLOR_CONVERT1      0x8198
#define SEC_STREAM_COLOR_CONVERT2      0x819c
#define SEC_STREAM_COLOR_CONVERT3      0x81e4
#define SEC_STREAM_HSCALING            0x81a0
#define SEC_STREAM_BUFFERSIZE          0x81a8
#define SEC_STREAM_HSCALE_NORMALIZE    0x81ac
#define SEC_STREAM_VSCALING            0x81e8
#define SEC_STREAM_FBUF_ADDR0          0x81d0
#define SEC_STREAM_FBUF_ADDR1          0x81d4
#define SEC_STREAM_FBUF_ADDR2          0x81ec
#define SEC_STREAM_STRIDE              0x81d8
#define SEC_STREAM_WINDOW_START        0x81f8
#define SEC_STREAM_WINDOW_SZ           0x81fc
#define SEC_STREAM_TILE_OFF            0x821c
#define SEC_STREAM_OPAQUE_OVERLAY      0x81dc

/* Savage 2000 registers */
#define SEC_STREAM_COLOR_CONVERT0_2000 0x8198
#define SEC_STREAM_COLOR_CONVERT1_2000 0x819c
#define SEC_STREAM_COLOR_CONVERT2_2000 0x81e0
#define SEC_STREAM_COLOR_CONVERT3_2000 0x81e4

/* Virge+ registers */
#define FIFO_CONTROL_REG               0x8200
#define MIU_CONTROL_REG                0x8204
#define STREAMS_TIMEOUT_REG            0x8208
#define MISC_TIMEOUT_REG               0x820c

/* VGA stuff */
#define vgaCRIndex                     0x3d4
#define vgaCRReg                       0x3d5

/* CRT Control registers */
#define EXT_MEM_CTRL1                  0x53
#define LIN_ADDR_CTRL                  0x58
#define EXT_MISC_CTRL2                 0x67

/* Old engine constants */
#define ENABLE_NEWMMIO                 0x08
#define ENABLE_LFB                     0x10
#define ENABLE_STREAMS_OLD             0x0c
#define NO_STREAMS_OLD                 0xf3

/* New engine constants */
#define ENABLE_STREAM1                 0x04
#define NO_STREAMS                     0xF9

#define VerticalRetraceWait() \
do { \
	VGAIN8(0x3d4); \
	VGAOUT8(0x3d4, 0x17); \
	if (VGAIN8(0x3d5) & 0x80) { \
		int i = 0x10000; \
		while ((VGAIN8(0x3da) & 0x08) == 0x08 && i--) ; \
		i = 0x10000; \
		while ((VGAIN8(0x3da) & 0x08) == 0x00 && i--) ; \
	} \
} while (0)

/* Scaling operations */
#define HSCALING_Shift    0
#define HSCALING_Mask     (((1L << 16)-1) << HSCALING_Shift)
#define HSCALING(w0,w1)   ((((unsigned int)(((double)w0/(double)w1) * (1 << 15))) << HSCALING_Shift) & HSCALING_Mask)
                                                                                                                    
#define VSCALING_Shift    0
#define VSCALING_Mask     (((1L << 20)-1) << VSCALING_Shift)
#define VSCALING(h0,h1)   ((((unsigned int) (((double)h0/(double)h1) * (1 << 15))) << VSCALING_Shift) & VSCALING_Mask)

/* Scaling factors */
#define HDM_SHIFT      16
#define HDSCALE_4      (2 << HDM_SHIFT)
#define HDSCALE_8      (3 << HDM_SHIFT)
#define HDSCALE_16     (4 << HDM_SHIFT)
#define HDSCALE_32     (5 << HDM_SHIFT)
#define HDSCALE_64     (6 << HDM_SHIFT)

/* Window parameters */
#define OS_XY(x,y)     (((x+1)<<16)|(y+1))
#define OS_WH(x,y)     (((x-1)<<16)|(y))

/* PCI stuff */

/* PCI-Memory IO access macros.  */
#define VID_WR08(p,i,val)  (((uint8_t *)(p))[(i)]=(val))
#define VID_RD08(p,i)      (((uint8_t *)(p))[(i)])

#define VID_WR32(p,i,val)  (((uint32_t *)(p))[(i)/4]=(val))
#define VID_RD32(p,i)      (((uint32_t *)(p))[(i)/4])

#ifndef USE_RMW_CYCLES

/* Can be used to inhibit READ-MODIFY-WRITE cycles. On by default. */
#define MEM_BARRIER() __asm__ volatile ("" : : : "memory")

#undef  VID_WR08
#define VID_WR08(p,i,val) ({ MEM_BARRIER(); ((uint8_t *)(p))[(i)]=(val); })
#undef  VID_RD08
#define VID_RD08(p,i)     ({ MEM_BARRIER(); ((uint8_t *)(p))[(i)]; })

#undef  VID_WR16
#define VID_WR16(p,i,val) ({ MEM_BARRIER(); ((uint16_t *)(p))[(i)/2]=(val); })
#undef  VID_RD16
#define VID_RD16(p,i)     ({ MEM_BARRIER(); ((uint16_t *)(p))[(i)/2]; })

#undef  VID_WR32
#define VID_WR32(p,i,val) ({ MEM_BARRIER(); ((uint32_t *)(p))[(i)/4]=(val); })
#undef  VID_RD32
#define VID_RD32(p,i)     ({ MEM_BARRIER(); ((uint32_t *)(p))[(i)/4]; })
#endif /* USE_RMW_CYCLES */

#define VID_AND32(p,i,val) VID_WR32(p,i,VID_RD32(p,i)&(val))
#define VID_OR32(p,i,val)  VID_WR32(p,i,VID_RD32(p,i)|(val))
#define VID_XOR32(p,i,val) VID_WR32(p,i,VID_RD32(p,i)^(val))

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

#endif /* MPLAYER_S3_REGS_H */
