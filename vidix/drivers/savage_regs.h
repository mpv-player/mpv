/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/savage/savage_regs.h,v 1.10 2001/11/04 22:17:48 alanh Exp $ */

#ifndef _SAVAGE_REGS_H
#define _SAVAGE_REGS_H

/* These are here until xf86PciInfo.h is updated. */

#ifndef PCI_CHIP_S3TWISTER_P
#define PCI_CHIP_S3TWISTER_P	0x8d01
#endif
#ifndef PCI_CHIP_S3TWISTER_K
#define PCI_CHIP_S3TWISTER_K	0x8d02
#endif
#ifndef PCI_CHIP_SUPSAV_MX128
#define PCI_CHIP_SUPSAV_MX128		0x8c22
#define PCI_CHIP_SUPSAV_MX64		0x8c24
#define PCI_CHIP_SUPSAV_MX64C		0x8c26
#define PCI_CHIP_SUPSAV_IX128SDR	0x8c2a
#define PCI_CHIP_SUPSAV_IX128DDR	0x8c2b
#define PCI_CHIP_SUPSAV_IX64SDR		0x8c2c
#define PCI_CHIP_SUPSAV_IX64DDR		0x8c2d
#define PCI_CHIP_SUPSAV_IXCSDR		0x8c2e
#define PCI_CHIP_SUPSAV_IXCDDR		0x8c2f
#endif
#ifndef PCI_CHIP_PROSAVAGE_DDR
#define PCI_CHIP_PROSAVAGE_DDR	0x8d03
#define PCI_CHIP_PROSAVAGE_DDRK	0x8d04
#endif

#define S3_SAVAGE3D_SERIES(chip)  ((chip>=S3_SAVAGE3D) && (chip<=S3_SAVAGE_MX))

#define S3_SAVAGE4_SERIES(chip)   ((chip==S3_SAVAGE4) || (chip==S3_PROSAVAGE))

#define	S3_SAVAGE_MOBILE_SERIES(chip)	((chip==S3_SAVAGE_MX) || (chip==S3_SUPERSAVAGE))

#define S3_SAVAGE_SERIES(chip)    ((chip>=S3_SAVAGE3D) && (chip<=S3_SAVAGE2000))


/* Chip tags.  These are used to group the adapters into 
 * related families.
 */


enum S3CHIPTAGS {
    S3_UNKNOWN = 0,
    S3_SAVAGE3D,
    S3_SAVAGE_MX,
    S3_SAVAGE4,
    S3_PROSAVAGE,
    S3_SUPERSAVAGE,
    S3_SAVAGE2000,
    S3_LAST
};

typedef struct {
    unsigned int mode, refresh;
    unsigned char SR08, SR0E, SR0F;
    unsigned char SR10, SR11, SR12, SR13, SR15, SR18, SR1B, SR29, SR30;
    unsigned char SR54[8];
    unsigned char Clock;
    unsigned char CR31, CR32, CR33, CR34, CR36, CR3A, CR3B, CR3C;
    unsigned char CR40, CR41, CR42, CR43, CR45;
    unsigned char CR50, CR51, CR53, CR55, CR58, CR5B, CR5D, CR5E;
    unsigned char CR60, CR63, CR65, CR66, CR67, CR68, CR69, CR6D, CR6F;
    unsigned char CR86, CR88;
    unsigned char CR90, CR91, CRB0;
    unsigned int  STREAMS[22];	/* yuck, streams regs */
    unsigned int  MMPR0, MMPR1, MMPR2, MMPR3;
} SavageRegRec, *SavageRegPtr;



#define BIOS_BSIZE			1024
#define BIOS_BASE			0xc0000

#define SAVAGE_NEWMMIO_REGBASE_S3	0x1000000  /* 16MB */
#define SAVAGE_NEWMMIO_REGBASE_S4	0x0000000 
#define SAVAGE_NEWMMIO_REGSIZE		0x0080000	/* 512kb */
#define SAVAGE_NEWMMIO_VGABASE		0x8000

#define BASE_FREQ			14.31818	

#define FIFO_CONTROL_REG		0x8200
#define MIU_CONTROL_REG			0x8204
#define STREAMS_TIMEOUT_REG		0x8208
#define MISC_TIMEOUT_REG		0x820c

/* Stream Processor 1 */

/* Primary Stream 1 Frame Buffer Address 0 */
#define PRI_STREAM_FBUF_ADDR0           0x81c0
/* Primary Stream 1 Frame Buffer Address 0 */
#define PRI_STREAM_FBUF_ADDR1           0x81c4
/* Primary Stream 1 Stride */
#define PRI_STREAM_STRIDE               0x81c8
/* Primary Stream 1 Frame Buffer Size */
#define PRI_STREAM_BUFFERSIZE           0x8214

/* Secondary stream 1 Color/Chroma Key Control */
#define SEC_STREAM_CKEY_LOW             0x8184
/* Secondary stream 1 Chroma Key Upper Bound */
#define SEC_STREAM_CKEY_UPPER           0x8194
/* Blend Control of Secondary Stream 1 & 2 */
#define BLEND_CONTROL                   0x8190
/* Secondary Stream 1 Color conversion/Adjustment 1 */
#define SEC_STREAM_COLOR_CONVERT1       0x8198
/* Secondary Stream 1 Color conversion/Adjustment 2 */
#define SEC_STREAM_COLOR_CONVERT2       0x819c
/* Secondary Stream 1 Color conversion/Adjustment 3 */
#define SEC_STREAM_COLOR_CONVERT3       0x81e4
/* Secondary Stream 1 Horizontal Scaling */
#define SEC_STREAM_HSCALING             0x81a0
/* Secondary Stream 1 Frame Buffer Size */
#define SEC_STREAM_BUFFERSIZE           0x81a8
/* Secondary Stream 1 Horizontal Scaling Normalization (2K only) */
#define SEC_STREAM_HSCALE_NORMALIZE	0x81ac
/* Secondary Stream 1 Horizontal Scaling */
#define SEC_STREAM_VSCALING             0x81e8
/* Secondary Stream 1 Frame Buffer Address 0 */
#define SEC_STREAM_FBUF_ADDR0           0x81d0
/* Secondary Stream 1 Frame Buffer Address 1 */
#define SEC_STREAM_FBUF_ADDR1           0x81d4
/* Secondary Stream 1 Frame Buffer Address 2 */
#define SEC_STREAM_FBUF_ADDR2           0x81ec
/* Secondary Stream 1 Stride */
#define SEC_STREAM_STRIDE               0x81d8
/* Secondary Stream 1 Window Start Coordinates */
#define SEC_STREAM_WINDOW_START         0x81f8
/* Secondary Stream 1 Window Size */
#define SEC_STREAM_WINDOW_SZ            0x81fc
/* Secondary Streams Tile Offset */
#define SEC_STREAM_TILE_OFF             0x821c
/* Secondary Stream 1 Opaque Overlay Control */
#define SEC_STREAM_OPAQUE_OVERLAY       0x81dc


/* Stream Processor 2 */

/* Primary Stream 2 Frame Buffer Address 0 */
#define PRI_STREAM2_FBUF_ADDR0          0x81b0
/* Primary Stream 2 Frame Buffer Address 1 */
#define PRI_STREAM2_FBUF_ADDR1          0x81b4
/* Primary Stream 2 Stride */
#define PRI_STREAM2_STRIDE              0x81b8
/* Primary Stream 2 Frame Buffer Size */
#define PRI_STREAM2_BUFFERSIZE          0x8218

/* Secondary Stream 2 Color/Chroma Key Control */
#define SEC_STREAM2_CKEY_LOW            0x8188
/* Secondary Stream 2 Chroma Key Upper Bound */
#define SEC_STREAM2_CKEY_UPPER          0x818c
/* Secondary Stream 2 Horizontal Scaling */
#define SEC_STREAM2_HSCALING            0x81a4
/* Secondary Stream 2 Horizontal Scaling */
#define SEC_STREAM2_VSCALING            0x8204
/* Secondary Stream 2 Frame Buffer Size */
#define SEC_STREAM2_BUFFERSIZE          0x81ac
/* Secondary Stream 2 Frame Buffer Address 0 */
#define SEC_STREAM2_FBUF_ADDR0          0x81bc
/* Secondary Stream 2 Frame Buffer Address 1 */
#define SEC_STREAM2_FBUF_ADDR1          0x81e0
/* Secondary Stream 2 Frame Buffer Address 2 */
#define SEC_STREAM2_FBUF_ADDR2          0x8208
/* Multiple Buffer/LPB and Secondary Stream 2 Stride */
#define SEC_STREAM2_STRIDE_LPB          0x81cc
/* Secondary Stream 2 Color conversion/Adjustment 1 */
#define SEC_STREAM2_COLOR_CONVERT1      0x81f0
/* Secondary Stream 2 Color conversion/Adjustment 2 */
#define SEC_STREAM2_COLOR_CONVERT2      0x81f4
/* Secondary Stream 2 Color conversion/Adjustment 3 */
#define SEC_STREAM2_COLOR_CONVERT3      0x8200
/* Secondary Stream 2 Window Start Coordinates */
#define SEC_STREAM2_WINDOW_START        0x820c
/* Secondary Stream 2 Window Size */
#define SEC_STREAM2_WINDOW_SZ           0x8210
/* Secondary Stream 2 Opaque Overlay Control */
#define SEC_STREAM2_OPAQUE_OVERLAY      0x8180


/* savage 2000 */
#define SEC_STREAM_COLOR_CONVERT0_2000       0x8198
#define SEC_STREAM_COLOR_CONVERT1_2000       0x819c
#define SEC_STREAM_COLOR_CONVERT2_2000       0x81e0
#define SEC_STREAM_COLOR_CONVERT3_2000       0x81e4

#define SUBSYS_STAT_REG			0x8504

#define SRC_BASE			0xa4d4
#define DEST_BASE			0xa4d8
#define CLIP_L_R			0xa4dc
#define CLIP_T_B			0xa4e0
#define DEST_SRC_STR			0xa4e4
#define MONO_PAT_0			0xa4e8
#define MONO_PAT_1			0xa4ec

/* Constants for CR69. */

#define CRT_ACTIVE	0x01
#define LCD_ACTIVE	0x02
#define TV_ACTIVE	0x04
#define CRT_ATTACHED	0x10
#define LCD_ATTACHED	0x20
#define TV_ATTACHED	0x40


/*
 * reads from SUBSYS_STAT
 */
#define STATUS_WORD0            (INREG(0x48C00))
#define ALT_STATUS_WORD0        (INREG(0x48C60))
#define MAXLOOP			0xffffff
#define IN_SUBSYS_STAT()	(INREG(SUBSYS_STAT_REG))

#define MAXFIFO		0x7f00

/*
 * NOTE: don't remove 'VGAIN8(vgaCRIndex);'.
 * If not present it will cause lockups on Savage4.
 * Ask S3, why.
 */
/*#define VerticalRetraceWait() \
{ \
        VGAIN8(0x3d0+4); \
	VGAOUT8(0x3d0+4, 0x17); \
	if (VGAIN8(0x3d0+5) & 0x80) { \
		while ((VGAIN8(0x3d0 + 0x0a) & 0x08) == 0x08) ; \
		while ((VGAIN8(0x3d0 + 0x0a) & 0x08) == 0x00) ; \
	} \
}
*/

#define VerticalRetraceWait()           \
do {                                    \
	VGAIN8(0x3d4);            \
	VGAOUT8(0x3d4, 0x17);     \
	if (VGAIN8(0x3d5) & 0x80) {  \
		int i = 0x10000;                \
		while ((VGAIN8(0x3da) & 0x08) == 0x08 && i--) ; \
		i = 0x10000;                                                  \
		while ((VGAIN8(0x3da) & 0x08) == 0x00 && i--) ; \
	} \
} while (0)


#define	I2C_REG		0xa0
#define InI2CREG(a)	\
{ \
    VGAOUT8(0x3d0 + 4, I2C_REG);	\
    a = VGAIN8(0x3d0 + 5);		\
}

#define OutI2CREG(a)	\
{ \
    VGAOUT8(0x3d0 + 4, I2C_REG);	\
    VGAOUT8(0x3d0 + 5, a);		\
}
 
#define HZEXP_COMP_1		0x54
#define HZEXP_BORDER		0x58
#define HZEXP_FACTOR_IGA1	0x59

#define VTEXP_COMP_1		0x56
#define VTEXP_BORDER		0x5a
#define VTEXP_FACTOR_IGA1	0x5b

#define EC1_CENTER_ON	0x10
#define EC1_EXPAND_ON	0x0c

#define MODE_24 24

#if (MODE_24 == 32)
# define  BYTES_PP24 4
#else
# define BYTES_PP24 3
#endif

#define OVERLAY_DEPTH 16

#define  STREAMS_MODE32 0x7
#define  STREAMS_MODE24 0x6
#define  STREAMS_MODE16 0x5 /* @@@ */


#define DEPTH_BPP(depth) (depth == 24 ? (BYTES_PP24 << 3) : (depth + 7) & ~0x7)
#define DEPTH_2ND(depth) (depth > 8 ? depth\
                              : OVERLAY_DEPTH)
#define SSTREAMS_MODE(bpp) (bpp > 16 ? (bpp > 24 ? STREAMS_MODE32 :\
				       STREAMS_MODE24) : STREAMS_MODE16)

#define HSCALING_Shift    0
#define HSCALING_Mask     (((1L << 16)-1) << HSCALING_Shift)
#define HSCALING(w0,w1)   ((((unsigned int)(((double)w0/(double)w1) * (1 << 15))) \
                               << HSCALING_Shift) \
                           & HSCALING_Mask)
                                                                                                                    
#define VSCALING_Shift    0
#define VSCALING_Mask     (((1L << 20)-1) << VSCALING_Shift)
#define VSCALING(h0,h1)   ((((unsigned int) (((double)h0/(double)h1) * (1 << 15))) \
                               << VSCALING_Shift) \
                           & VSCALING_Mask)


#endif /* _SAVAGE_REGS_H */
