/*
 * mach64.h
 * This	software has been released under the terms of the GNU Public
 * license. See	http://www.gnu.org/copyleft/gpl.html for details.
 *
 * It's	based on radeonfb, X11,	GATOS sources
*/

#ifndef __MACH64_INCLUDED
#define __MACH64_INCLUDED 1

/* Note: this model of accessing to IO space is based on MMIO technology.
This means that this sources don't support ISA and VLB cards */
#define BlockIOTag(val) (val)
#define IOPortTag(sparce,val) (val)

/* MDA/[M]CGA/EGA/VGA I/O ports */
#define GENVS			0x0102u		/* Write (and Read on uC only) */

#define R_GENLPS		0x03b9u		/* Read */

#define GENHP			0x03bfu

#define ATTRX			0x03c0u
#define ATTRD			0x03c1u
#define GENS0			0x03c2u		/* Read */
#define GENMO			0x03c2u		/* Write */
#define GENENB			0x03c3u		/* Read */
#define SEQX			0x03c4u
#define SEQD			0x03c5u
#define VGA_DAC_MASK		0x03c6u
#define VGA_DAC_READ		0x03c7u
#define VGA_DAC_WRITE		0x03c8u
#define VGA_DAC_DATA		0x03c9u
#define R_GENFC			0x03cau		/* Read */
/*	?			0x03cbu */
#define R_GENMO			0x03ccu		/* Read */
/*	?			0x03cdu */
#define GRAX			0x03ceu
#define GRAD			0x03cfu

#define GENB			0x03d9u

#define GENLPS			0x03dcu		/* Write */
#define KCX			0x03ddu
#define KCD			0x03deu

#define GENENA			0x46e8u		/* Write */

/* I/O port base numbers */
#define MonochromeIOBase	0x03b0u
#define ColourIOBase		0x03d0u

/* Other MDA/[M]CGA/EGA/VGA I/O ports */
/*	?(_IOBase)		((_IOBase) + 0x00u) */	/* CRTX synonym */
/*	?(_IOBase)		((_IOBase) + 0x01u) */	/* CRTD synonym */
/*	?(_IOBase)		((_IOBase) + 0x02u) */	/* CRTX synonym */
/*	?(_IOBase)		((_IOBase) + 0x03u) */	/* CRTD synonym */
#define CRTX(_IOBase)		((_IOBase) + 0x04u)
#define CRTD(_IOBase)		((_IOBase) + 0x05u)
/*	?(_IOBase)		((_IOBase) + 0x06u) */
/*	?(_IOBase)		((_IOBase) + 0x07u) */
#define GENMC(_IOBase)		((_IOBase) + 0x08u)
/*	?(_IOBase)		((_IOBase) + 0x09u) */	/* R_GENLPS/GENB */
#define GENS1(_IOBase)		((_IOBase) + 0x0au)	/* Read */
#define GENFC(_IOBase)		((_IOBase) + 0x0au)	/* Write */
#define GENLPC(_IOBase)		((_IOBase) + 0x0bu)
/*	?(_IOBase)		((_IOBase) + 0x0cu) */	/* /GENLPS */
/*	?(_IOBase)		((_IOBase) + 0x0du) */	/* /KCX */
/*	?(_IOBase)		((_IOBase) + 0x0eu) */	/* /KCD */
/*	?(_IOBase)		((_IOBase) + 0x0fu) */	/* GENHP/ */

/* 8514/A VESA approved register definitions */
#define DISP_STAT		0x02e8u		/* Read */
#define SENSE				0x0001u	/* Presumably belong here */
#define VBLANK				0x0002u
#define HORTOG				0x0004u
#define H_TOTAL			0x02e8u		/* Write */
#define IBM_DAC_MASK		0x02eau
#define IBM_DAC_READ		0x02ebu
#define IBM_DAC_WRITE		0x02ecu
#define IBM_DAC_DATA		0x02edu
#define H_DISP			0x06e8u		/* Write */
#define H_SYNC_STRT		0x0ae8u		/* Write */
#define H_SYNC_WID		0x0ee8u		/* Write */
#define HSYNCPOL_POS			0x0000u
#define HSYNCPOL_NEG			0x0020u
#define H_POLARITY_POS			HSYNCPOL_POS	/* Sigh */
#define H_POLARITY_NEG			HSYNCPOL_NEG	/* Sigh */
#define V_TOTAL			0x12e8u		/* Write */
#define V_DISP			0x16e8u		/* Write */
#define V_SYNC_STRT		0x1ae8u		/* Write */
#define V_SYNC_WID		0x1ee8u		/* Write */
#define VSYNCPOL_POS			0x0000u
#define VSYNCPOL_NEG			0x0020u
#define V_POLARITY_POS			VSYNCPOL_POS	/* Sigh */
#define V_POLARITY_NEG			VSYNCPOL_NEG	/* Sigh */
#define DISP_CNTL		0x22e8u		/* Write */
#define ODDBNKENAB			0x0001u
#define MEMCFG_2			0x0000u
#define MEMCFG_4			0x0002u
#define MEMCFG_6			0x0004u
#define MEMCFG_8			0x0006u
#define DBLSCAN				0x0008u
#define INTERLACE			0x0010u
#define DISPEN_NC			0x0000u
#define DISPEN_ENAB			0x0020u
#define DISPEN_DISAB			0x0040u
#define R_H_TOTAL		0x26e8u		/* Read */
/*	?			0x2ae8u */
/*	?			0x2ee8u */
/*	?			0x32e8u */
/*	?			0x36e8u */
/*	?			0x3ae8u */
/*	?			0x3ee8u */
#define SUBSYS_STAT		0x42e8u		/* Read */
#define VBLNKFLG			0x0001u
#define PICKFLAG			0x0002u
#define INVALIDIO			0x0004u
#define GPIDLE				0x0008u
#define MONITORID_MASK			0x0070u
/*	MONITORID_?				0x0000u */
#define MONITORID_8507				0x0010u
#define MONITORID_8514				0x0020u
/*	MONITORID_?				0x0030u */
/*	MONITORID_?				0x0040u */
#define MONITORID_8503				0x0050u
#define MONITORID_8512				0x0060u
#define MONITORID_8513				0x0060u
#define MONITORID_NONE				0x0070u
#define _8PLANE				0x0080u
#define SUBSYS_CNTL		0x42e8u		/* Write */
#define RVBLNKFLG			0x0001u
#define RPICKFLAG			0x0002u
#define RINVALIDIO			0x0004u
#define RGPIDLE				0x0008u
#define IVBLNKFLG			0x0100u
#define IPICKFLAG			0x0200u
#define IINVALIDIO			0x0400u
#define IGPIDLE				0x0800u
#define CHPTEST_NC			0x0000u
#define CHPTEST_NORMAL			0x1000u
#define CHPTEST_ENAB			0x2000u
#define GPCTRL_NC			0x0000u
#define GPCTRL_ENAB			0x4000u
#define GPCTRL_RESET			0x8000u
#define ROM_PAGE_SEL		0x46e8u		/* Write */
#define ADVFUNC_CNTL		0x4ae8u		/* Write */
#define DISABPASSTHRU			0x0001u
#define CLOKSEL				0x0004u
/*	?			0x4ee8u */
#define EXT_CONFIG_0		0x52e8u		/* C & T 82C480 */
#define EXT_CONFIG_1		0x56e8u		/* C & T 82C480 */
#define EXT_CONFIG_2		0x5ae8u		/* C & T 82C480 */
#define EXT_CONFIG_3		0x5ee8u		/* C & T 82C480 */
/*	?			0x62e8u */
/*	?			0x66e8u */
/*	?			0x6ae8u */
/*	?			0x6ee8u */
/*	?			0x72e8u */
/*	?			0x76e8u */
/*	?			0x7ae8u */
/*	?			0x7ee8u */
#define CUR_Y			0x82e8u
#define CUR_X			0x86e8u
#define DESTY_AXSTP		0x8ae8u		/* Write */
#define DESTX_DIASTP		0x8ee8u		/* Write */
#define ERR_TERM		0x92e8u
#define MAJ_AXIS_PCNT		0x96e8u		/* Write */
#define GP_STAT			0x9ae8u		/* Read */
#define GE_STAT			0x9ae8u		/* Alias */
#define DATARDY				0x0100u
#define DATA_READY			DATARDY	/* Alias */
#define GPBUSY				0x0200u
#define CMD			0x9ae8u		/* Write */
#define WRTDATA				0x0001u
#define PLANAR				0x0002u
#define LASTPIX				0x0004u
#define LINETYPE			0x0008u
#define DRAW				0x0010u
#define INC_X				0x0020u
#define YMAJAXIS			0x0040u
#define INC_Y				0x0080u
#define PCDATA				0x0100u
#define _16BIT				0x0200u
#define CMD_NOP				0x0000u
#define CMD_OP_MSK			0xf000u
#define BYTSEQ					0x1000u
#define CMD_LINE				0x2000u
#define CMD_RECT				0x4000u
#define CMD_RECTV1				0x6000u
#define CMD_RECTV2				0x8000u
#define CMD_LINEAF				0xa000u
#define CMD_BITBLT				0xc000u
#define SHORT_STROKE		0x9ee8u		/* Write */
#define SSVDRAW				0x0010u
#define VECDIR_000			0x0000u
#define VECDIR_045			0x0020u
#define VECDIR_090			0x0040u
#define VECDIR_135			0x0060u
#define VECDIR_180			0x0080u
#define VECDIR_225			0x00a0u
#define VECDIR_270			0x00c0u
#define VECDIR_315			0x00e0u
#define BKGD_COLOR		0xa2e8u		/* Write */
#define FRGD_COLOR		0xa6e8u		/* Write */
#define WRT_MASK		0xaae8u		/* Write */
#define RD_MASK			0xaee8u		/* Write */
#define COLOR_CMP		0xb2e8u		/* Write */
#define BKGD_MIX		0xb6e8u		/* Write */
/*					0x001fu	See MIX_* definitions below */
#define BSS_BKGDCOL			0x0000u
#define BSS_FRGDCOL			0x0020u
#define BSS_PCDATA			0x0040u
#define BSS_BITBLT			0x0060u
#define FRGD_MIX		0xbae8u		/* Write */
/*					0x001fu	See MIX_* definitions below */
#define FSS_BKGDCOL			0x0000u
#define FSS_FRGDCOL			0x0020u
#define FSS_PCDATA			0x0040u
#define FSS_BITBLT			0x0060u
#define MULTIFUNC_CNTL		0xbee8u		/* Write */
#define MIN_AXIS_PCNT			0x0000u
#define SCISSORS_T			0x1000u
#define SCISSORS_L			0x2000u
#define SCISSORS_B			0x3000u
#define SCISSORS_R			0x4000u
#define M32_MEM_CNTL			0x5000u
#define HORCFG_4				0x0000u
#define HORCFG_5				0x0001u
#define HORCFG_8				0x0002u
#define HORCFG_10				0x0003u
#define VRTCFG_2				0x0000u
#define VRTCFG_4				0x0004u
#define VRTCFG_6				0x0008u
#define VRTCFG_8				0x000cu
#define BUFSWP					0x0010u
#define PATTERN_L			0x8000u
#define PATTERN_H			0x9000u
#define PIX_CNTL			0xa000u
#define PLANEMODE				0x0004u
#define COLCMPOP_F				0x0000u
#define COLCMPOP_T				0x0008u
#define COLCMPOP_GE				0x0010u
#define COLCMPOP_LT				0x0018u
#define COLCMPOP_NE				0x0020u
#define COLCMPOP_EQ				0x0028u
#define COLCMPOP_LE				0x0030u
#define COLCMPOP_GT				0x0038u
#define MIXSEL_FRGDMIX				0x0000u
#define MIXSEL_PATT				0x0040u
#define MIXSEL_EXPPC				0x0080u
#define MIXSEL_EXPBLT				0x00c0u
/*	?			0xc2e8u */
/*	?			0xc6e8u */
/*	?			0xcae8u */
/*	?			0xcee8u */
/*	?			0xd2e8u */
/*	?			0xd6e8u */
/*	?			0xdae8u */
/*	?			0xdee8u */
#define PIX_TRANS		0xe2e8u
/*	?			0xe6e8u */
/*	?			0xeae8u */
/*	?			0xeee8u */
/*	?			0xf2e8u */
/*	?			0xf6e8u */
/*	?			0xfae8u */
/*	?			0xfee8u */

/* ATI Mach8 & Mach32 register definitions */
#define OVERSCAN_COLOR_8	0x02eeu		/* Write */	/* Mach32 */
#define OVERSCAN_BLUE_24	0x02efu		/* Write */	/* Mach32 */
#define OVERSCAN_GREEN_24	0x06eeu		/* Write */	/* Mach32 */
#define OVERSCAN_RED_24		0x06efu		/* Write */	/* Mach32 */
#define CURSOR_OFFSET_LO	0x0aeeu		/* Write */	/* Mach32 */
#define CURSOR_OFFSET_HI	0x0eeeu		/* Write */	/* Mach32 */
#define CONFIG_STATUS_1		0x12eeu		/* Read */
#define CLK_MODE			0x0001u			/* Mach8 */
#define BUS_16				0x0002u			/* Mach8 */
#define MC_BUS				0x0004u			/* Mach8 */
#define EEPROM_ENA			0x0008u			/* Mach8 */
#define DRAM_ENA			0x0010u			/* Mach8 */
#define MEM_INSTALLED			0x0060u			/* Mach8 */
#define ROM_ENA				0x0080u			/* Mach8 */
#define ROM_PAGE_ENA			0x0100u			/* Mach8 */
#define ROM_LOCATION			0xfe00u			/* Mach8 */
#define _8514_ONLY			0x0001u			/* Mach32 */
#define BUS_TYPE			0x000eu			/* Mach32 */
#define ISA_16_BIT				0x0000u		/* Mach32 */
#define EISA					0x0002u		/* Mach32 */
#define MICRO_C_16_BIT				0x0004u		/* Mach32 */
#define MICRO_C_8_BIT				0x0006u		/* Mach32 */
#define LOCAL_386SX				0x0008u		/* Mach32 */
#define LOCAL_386DX				0x000au		/* Mach32 */
#define LOCAL_486				0x000cu		/* Mach32 */
#define PCI					0x000eu		/* Mach32 */
#define MEM_TYPE			0x0070u			/* Mach32 */
#define CHIP_DIS			0x0080u			/* Mach32 */
#define TST_VCTR_ENA			0x0100u			/* Mach32 */
#define DACTYPE				0x0e00u			/* Mach32 */
#define MC_ADR_DECODE			0x1000u			/* Mach32 */
#define CARD_ID				0xe000u			/* Mach32 */
#define HORZ_CURSOR_POSN	0x12eeu		/* Write */	/* Mach32 */
#define CONFIG_STATUS_2		0x16eeu		/* Read */
#define SHARE_CLOCK			0x0001u			/* Mach8 */
#define HIRES_BOOT			0x0002u			/* Mach8 */
#define EPROM_16_ENA			0x0004u			/* Mach8 */
#define WRITE_PER_BIT			0x0008u			/* Mach8 */
#define FLASH_ENA			0x0010u			/* Mach8 */
#define SLOW_SEQ_EN			0x0001u			/* Mach32 */
#define MEM_ADDR_DIS			0x0002u			/* Mach32 */
#define ISA_16_ENA			0x0004u			/* Mach32 */
#define KOR_TXT_MODE_ENA		0x0008u			/* Mach32 */
#define LOCAL_BUS_SUPPORT		0x0030u			/* Mach32 */
#define LOCAL_BUS_CONFIG_2		0x0040u			/* Mach32 */
#define LOCAL_BUS_RD_DLY_ENA		0x0080u			/* Mach32 */
#define LOCAL_DAC_EN			0x0100u			/* Mach32 */
#define LOCAL_RDY_EN			0x0200u			/* Mach32 */
#define EEPROM_ADR_SEL			0x0400u			/* Mach32 */
#define GE_STRAP_SEL			0x0800u			/* Mach32 */
#define VESA_RDY			0x1000u			/* Mach32 */
#define Z4GB				0x2000u			/* Mach32 */
#define LOC2_MDRAM			0x4000u			/* Mach32 */
#define VERT_CURSOR_POSN	0x16eeu		/* Write */	/* Mach32 */
#define FIFO_TEST_DATA		0x1aeeu		/* Read */	/* Mach32 */
#define CURSOR_COLOR_0		0x1aeeu		/* Write */	/* Mach32 */
#define CURSOR_COLOR_1		0x1aefu		/* Write */	/* Mach32 */
#define HORZ_CURSOR_OFFSET	0x1eeeu		/* Write */	/* Mach32 */
#define VERT_CURSOR_OFFSET	0x1eefu		/* Write */	/* Mach32 */
#define PCI_CNTL		0x22eeu				/* Mach32-PCI */
#define CRT_PITCH		0x26eeu		/* Write */
#define CRT_OFFSET_LO		0x2aeeu		/* Write */
#define CRT_OFFSET_HI		0x2eeeu		/* Write */
#define LOCAL_CNTL		0x32eeu				/* Mach32 */
#define FIFO_OPT		0x36eeu		/* Write */	/* Mach8 */
#define MISC_OPTIONS		0x36eeu				/* Mach32 */
#define W_STATE_ENA			0x0000u			/* Mach32 */
#define HOST_8_ENA			0x0001u			/* Mach32 */
#define MEM_SIZE_ALIAS			0x000cu			/* Mach32 */
#define MEM_SIZE_512K				0x0000u		/* Mach32 */
#define MEM_SIZE_1M				0x0004u		/* Mach32 */
#define MEM_SIZE_2M				0x0008u		/* Mach32 */
#define MEM_SIZE_4M				0x000cu		/* Mach32 */
#define DISABLE_VGA			0x0010u			/* Mach32 */
#define _16_BIT_IO			0x0020u			/* Mach32 */
#define DISABLE_DAC			0x0040u			/* Mach32 */
#define DLY_LATCH_ENA			0x0080u			/* Mach32 */
#define TEST_MODE			0x0100u			/* Mach32 */
#define BLK_WR_ENA			0x0400u			/* Mach32 */
#define _64_DRAW_ENA			0x0800u			/* Mach32 */
#define FIFO_TEST_TAG		0x3aeeu		/* Read */	/* Mach32 */
#define EXT_CURSOR_COLOR_0	0x3aeeu		/* Write */	/* Mach32 */
#define EXT_CURSOR_COLOR_1	0x3eeeu		/* Write */	/* Mach32 */
#define MEM_BNDRY		0x42eeu				/* Mach32 */
#define MEM_PAGE_BNDRY			0x000fu			/* Mach32 */
#define MEM_BNDRY_ENA			0x0010u			/* Mach32 */
#define SHADOW_CTL		0x46eeu		/* Write */
#define CLOCK_SEL		0x4aeeu
/*	DISABPASSTHRU			0x0001u	See ADVFUNC_CNTL */
#define VFIFO_DEPTH_1			0x0100u			/* Mach32 */
#define VFIFO_DEPTH_2			0x0200u			/* Mach32 */
#define VFIFO_DEPTH_3			0x0300u			/* Mach32 */
#define VFIFO_DEPTH_4			0x0400u			/* Mach32 */
#define VFIFO_DEPTH_5			0x0500u			/* Mach32 */
#define VFIFO_DEPTH_6			0x0600u			/* Mach32 */
#define VFIFO_DEPTH_7			0x0700u			/* Mach32 */
#define VFIFO_DEPTH_8			0x0800u			/* Mach32 */
#define VFIFO_DEPTH_9			0x0900u			/* Mach32 */
#define VFIFO_DEPTH_A			0x0a00u			/* Mach32 */
#define VFIFO_DEPTH_B			0x0b00u			/* Mach32 */
#define VFIFO_DEPTH_C			0x0c00u			/* Mach32 */
#define VFIFO_DEPTH_D			0x0d00u			/* Mach32 */
#define VFIFO_DEPTH_E			0x0e00u			/* Mach32 */
#define VFIFO_DEPTH_F			0x0f00u			/* Mach32 */
#define COMPOSITE_SYNC			0x1000u
/*	?			0x4eeeu */
#define ROM_ADDR_1		0x52eeu
#define BIOS_BASE_SEGMENT		0x007fu			/* Mach32 */
/*	?				0xff80u */		/* Mach32 */
#define ROM_ADDR_2		0x56eeu		/* Sick ... */
#define SHADOW_SET		0x5aeeu		/* Write */
#define MEM_CFG			0x5eeeu				/* Mach32 */
#define MEM_APERT_SEL			0x0003u			/* Mach32 */
#define MEM_APERT_PAGE			0x000cu			/* Mach32 */
#define MEM_APERT_LOC			0xfff0u			/* Mach32 */
#define EXT_GE_STATUS		0x62eeu		/* Read */	/* Mach32 */
#define HORZ_OVERSCAN		0x62eeu		/* Write */	/* Mach32 */
#define VERT_OVERSCAN		0x66eeu		/* Write */	/* Mach32 */
#define MAX_WAITSTATES		0x6aeeu
#define GE_OFFSET_LO		0x6eeeu		/* Write */
#define BOUNDS_LEFT		0x72eeu		/* Read */
#define GE_OFFSET_HI		0x72eeu		/* Write */
#define BOUNDS_TOP		0x76eeu		/* Read */
#define GE_PITCH		0x76eeu		/* Write */
#define BOUNDS_RIGHT		0x7aeeu		/* Read */
#define EXT_GE_CONFIG		0x7aeeu		/* Write */	/* Mach32 */
#define MONITOR_ALIAS			0x0007u			/* Mach32 */
/*	MONITOR_?				0x0000u */	/* Mach32 */
#define MONITOR_8507				0x0001u		/* Mach32 */
#define MONITOR_8514				0x0002u		/* Mach32 */
/*	MONITOR_?				0x0003u */	/* Mach32 */
/*	MONITOR_?				0x0004u */	/* Mach32 */
#define MONITOR_8503				0x0005u		/* Mach32 */
#define MONITOR_8512				0x0006u		/* Mach32 */
#define MONITOR_8513				0x0006u		/* Mach32 */
#define MONITOR_NONE				0x0007u		/* Mach32 */
#define ALIAS_ENA			0x0008u			/* Mach32 */
#define PIXEL_WIDTH_4			0x0000u			/* Mach32 */
#define PIXEL_WIDTH_8			0x0010u			/* Mach32 */
#define PIXEL_WIDTH_16			0x0020u			/* Mach32 */
#define PIXEL_WIDTH_24			0x0030u			/* Mach32 */
#define RGB16_555			0x0000u			/* Mach32 */
#define RGB16_565			0x0040u			/* Mach32 */
#define RGB16_655			0x0080u			/* Mach32 */
#define RGB16_664			0x00c0u			/* Mach32 */
#define MULTIPLEX_PIXELS		0x0100u			/* Mach32 */
#define RGB24				0x0000u			/* Mach32 */
#define RGBx24				0x0200u			/* Mach32 */
#define BGR24				0x0400u			/* Mach32 */
#define xBGR24				0x0600u			/* Mach32 */
#define DAC_8_BIT_EN			0x4000u			/* Mach32 */
#define ORDER_16BPP_565			RGB16_565		/* Mach32 */
#define BOUNDS_BOTTOM		0x7eeeu		/* Read */
#define MISC_CNTL		0x7eeeu		/* Write */	/* Mach32 */
#define PATT_DATA_INDEX		0x82eeu
/*	?			0x86eeu */
/*	?			0x8aeeu */
#define R_EXT_GE_CONFIG		0x8eeeu		/* Read */	/* Mach32 */
#define PATT_DATA		0x8eeeu		/* Write */
#define R_MISC_CNTL		0x92eeu		/* Read */	/* Mach32 */
#define BRES_COUNT		0x96eeu
#define EXT_FIFO_STATUS		0x9aeeu		/* Read */
#define LINEDRAW_INDEX		0x9aeeu		/* Write */
/*	?			0x9eeeu */
#define LINEDRAW_OPT		0xa2eeu
#define BOUNDS_RESET			0x0100u
#define CLIP_MODE_0			0x0000u	/* Clip exception disabled */
#define CLIP_MODE_1			0x0200u	/* Line segments */
#define CLIP_MODE_2			0x0400u	/* Polygon boundary lines */
#define CLIP_MODE_3			0x0600u	/* Patterned lines */
#define DEST_X_START		0xa6eeu		/* Write */
#define DEST_X_END		0xaaeeu		/* Write */
#define DEST_Y_END		0xaeeeu		/* Write */
#define R_H_TOTAL_DISP		0xb2eeu		/* Read */	/* Mach32 */
#define SRC_X_STRT		0xb2eeu		/* Write */
#define R_H_SYNC_STRT		0xb6eeu		/* Read */	/* Mach32 */
#define ALU_BG_FN		0xb6eeu		/* Write */
#define R_H_SYNC_WID		0xbaeeu		/* Read */	/* Mach32 */
#define ALU_FG_FN		0xbaeeu		/* Write */
#define SRC_X_END		0xbeeeu		/* Write */
#define R_V_TOTAL		0xc2eeu		/* Read */
#define SRC_Y_DIR		0xc2eeu		/* Write */
#define R_V_DISP		0xc6eeu		/* Read */	/* Mach32 */
#define EXT_SHORT_STROKE	0xc6eeu		/* Write */
#define R_V_SYNC_STRT		0xcaeeu		/* Read */	/* Mach32 */
#define SCAN_X			0xcaeeu		/* Write */
#define VERT_LINE_CNTR		0xceeeu		/* Read */	/* Mach32 */
#define DP_CONFIG		0xceeeu		/* Write */
#define READ_WRITE			0x0001u
#define DATA_WIDTH			0x0200u
#define DATA_ORDER			0x1000u
#define FG_COLOR_SRC_FG			0x2000u
#define FG_COLOR_SRC_BLIT		0x6000u
#define R_V_SYNC_WID		0xd2eeu		/* Read */
#define PATT_LENGTH		0xd2eeu		/* Write */
#define PATT_INDEX		0xd6eeu		/* Write */
#define READ_SRC_X		0xdaeeu		/* Read */	/* Mach32 */
#define EXT_SCISSOR_L		0xdaeeu		/* Write */
#define READ_SRC_Y		0xdeeeu		/* Read */	/* Mach32 */
#define EXT_SCISSOR_T		0xdeeeu		/* Write */
#define EXT_SCISSOR_R		0xe2eeu		/* Write */
#define EXT_SCISSOR_B		0xe6eeu		/* Write */
/*	?			0xeaeeu */
#define DEST_COMP_FN		0xeeeeu		/* Write */
#define DEST_COLOR_CMP_MASK	0xf2eeu		/* Write */	/* Mach32 */
/*	?			0xf6eeu */
#define CHIP_ID			0xfaeeu		/* Read */	/* Mach32 */
#define CHIP_CODE_0			0x001fu			/* Mach32 */
#define CHIP_CODE_1			0x03e0u			/* Mach32 */
#define CHIP_CLASS			0x0c00u			/* Mach32 */
#define CHIP_REV			0xf000u			/* Mach32 */
#define LINEDRAW		0xfeeeu		/* Write */

/* ATI Mach64 register definitions */
#define CRTC_H_TOTAL_DISP	IOPortTag(0x00u, 0x00u)
#define CRTC_H_TOTAL			0x000001fful
/*	?				0x0000fe00ul */
#define CRTC_H_DISP			0x01ff0000ul
/*	?				0xfe000000ul */
#define CRTC_H_SYNC_STRT_WID	IOPortTag(0x01u, 0x01u)
#define CRTC_H_SYNC_STRT		0x000000fful
#define CRTC_H_SYNC_DLY			0x00000700ul
/*	?				0x00000800ul */
#define CRTC_H_SYNC_STRT_HI		0x00001000ul
/*	?				0x0000e000ul */
#define CRTC_H_SYNC_WID			0x001f0000ul
#define CRTC_H_SYNC_POL			0x00200000ul
/*	?				0xffc00000ul */
#define CRTC_V_TOTAL_DISP	IOPortTag(0x02u, 0x02u)
#define CRTC_V_TOTAL			0x000007fful
/*	?				0x0000f800ul */
#define CRTC_V_DISP			0x07ff0000ul
/*	?				0xf8000000ul */
#define CRTC_V_SYNC_STRT_WID	IOPortTag(0x03u, 0x03u)
#define CRTC_V_SYNC_STRT		0x000007fful
/*	?				0x0000f800ul */
#define CRTC_V_SYNC_WID			0x001f0000ul
#define CRTC_V_SYNC_POL			0x00200000ul
/*	?				0xffc00000ul */
#define CRTC_VLINE_CRNT_VLINE	IOPortTag(0x04u, 0x04u)
#define CRTC_VLINE			0x000007fful
/*	?				0x0000f800ul */
#define CRTC_CRNT_VLINE			0x07ff0000ul
/*	?				0xf8000000ul */
#define CRTC_OFF_PITCH		IOPortTag(0x05u, 0x05u)
#define CRTC_OFFSET			0x000ffffful
#define CRTC_OFFSET_VGA			0x0003fffful
#define CRTC_OFFSET_LOCK		0x00100000ul	/* XC/XL */
/*	?				0x00200000ul */
#define CRTC_PITCH			0xffc00000ul
#define CRTC_INT_CNTL		IOPortTag(0x06u, 0x06u)
#define CRTC_VBLANK			0x00000001ul
#define CRTC_VBLANK_INT_EN		0x00000002ul
#define CRTC_VBLANK_INT			0x00000004ul
#define CRTC_VLINE_INT_EN		0x00000008ul
#define CRTC_VLINE_INT			0x00000010ul
#define CRTC_VLINE_SYNC			0x00000020ul
#define CRTC_FRAME			0x00000040ul
#define CRTC_SNAPSHOT_INT_EN		0x00000080ul	/* GTPro */
#define CRTC_SNAPSHOT_INT		0x00000100ul	/* GTPro */
#define CRTC_I2C_INT_EN			0x00000200ul	/* GTPro */
#define CRTC_I2C_INT			0x00000400ul	/* GTPro */
#define CRTC2_VBLANK			0x00000800ul	/* LTPro */
#define CRTC2_VBLANK_INT_EN		0x00001000ul	/* LTPro */
#define CRTC2_VBLANK_INT		0x00002000ul	/* LTPro */
#define CRTC2_VLINE_INT_EN		0x00004000ul	/* LTPro */
#define CRTC2_VLINE_INT			0x00008000ul	/* LTPro */
#define CRTC_CAPBUF0_INT_EN		0x00010000ul	/* VT/GT */
#define CRTC_CAPBUF0_INT		0x00020000ul	/* VT/GT */
#define CRTC_CAPBUF1_INT_EN		0x00040000ul	/* VT/GT */
#define CRTC_CAPBUF1_INT		0x00080000ul	/* VT/GT */
#define CRTC_OVERLAY_EOF_INT_EN		0x00100000ul	/* VT/GT */
#define CRTC_OVERLAY_EOF_INT		0x00200000ul	/* VT/GT */
#define CRTC_ONESHOT_CAP_INT_EN		0x00400000ul	/* VT/GT */
#define CRTC_ONESHOT_CAP_INT		0x00800000ul	/* VT/GT */
#define CRTC_BUSMASTER_EOL_INT_EN	0x01000000ul	/* VTB/GTB/LT */
#define CRTC_BUSMASTER_EOL_INT		0x02000000ul	/* VTB/GTB/LT */
#define CRTC_GP_INT_EN			0x04000000ul	/* VTB/GTB/LT */
#define CRTC_GP_INT			0x08000000ul	/* VTB/GTB/LT */
#define CRTC2_VLINE_SYNC		0x10000000ul	/* LTPro */
#define CRTC_SNAPSHOT2_INT_EN		0x20000000ul	/* LTPro */
#define CRTC_SNAPSHOT2_INT		0x40000000ul	/* LTPro */
#define CRTC_VBLANK_BIT2_INT		0x80000000ul	/* GTPro */
#define CRTC_INT_ENS	/* *** UPDATE ME *** */		\
		(					\
			CRTC_VBLANK_INT_EN |		\
			CRTC_VLINE_INT_EN |		\
			CRTC_SNAPSHOT_INT_EN |		\
			CRTC_I2C_INT_EN |		\
			CRTC2_VBLANK_INT_EN |		\
			CRTC2_VLINE_INT_EN |		\
			CRTC_CAPBUF0_INT_EN |		\
			CRTC_CAPBUF1_INT_EN |		\
			CRTC_OVERLAY_EOF_INT_EN |	\
			CRTC_ONESHOT_CAP_INT_EN |	\
			CRTC_BUSMASTER_EOL_INT_EN |	\
			CRTC_GP_INT_EN |		\
			CRTC_SNAPSHOT2_INT_EN |		\
			0				\
		)
#define CRTC_INT_ACKS	/* *** UPDATE ME *** */		\
		(					\
			CRTC_VBLANK_INT |		\
			CRTC_VLINE_INT |		\
			CRTC_SNAPSHOT_INT |		\
			CRTC_I2C_INT |			\
			CRTC2_VBLANK_INT |		\
			CRTC2_VLINE_INT |		\
			CRTC_CAPBUF0_INT |		\
			CRTC_CAPBUF1_INT |		\
			CRTC_OVERLAY_EOF_INT |		\
			CRTC_ONESHOT_CAP_INT |		\
			CRTC_BUSMASTER_EOL_INT |	\
			CRTC_GP_INT |			\
			CRTC_SNAPSHOT2_INT |		\
			CRTC_VBLANK_BIT2_INT |		\
			0				\
		)
#define CRTC_GEN_CNTL		IOPortTag(0x07u, 0x07u)
#define CRTC_DBL_SCAN_EN		0x00000001ul
#define CRTC_INTERLACE_EN		0x00000002ul
#define CRTC_HSYNC_DIS			0x00000004ul
#define CRTC_VSYNC_DIS			0x00000008ul
#define CRTC_CSYNC_EN			0x00000010ul
#define CRTC_PIX_BY_2_EN		0x00000020ul
#define CRTC2_DBL_SCAN_EN		0x00000020ul	/* LTPro */
#define CRTC_DISPLAY_DIS		0x00000040ul
#define CRTC_VGA_XOVERSCAN		0x00000080ul
#define CRTC_PIX_WIDTH			0x00000700ul
#define CRTC_BYTE_PIX_ORDER		0x00000800ul
#define CRTC_VSYNC_INT_EN		0x00001000ul	/* XC/XL */
#define CRTC_VSYNC_INT			0x00002000ul	/* XC/XL */
#define CRTC_FIFO_OVERFILL		0x0000c000ul	/* VT/GT */
#define CRTC2_VSYNC_INT_EN		0x00004000ul	/* XC/XL */
#define CRTC2_VSYNC_INT			0x00008000ul	/* XC/XL */
#define CRTC_FIFO_LWM			0x000f0000ul
#define CRTC_HVSYNC_IO_DRIVE		0x00010000ul	/* XC/XL */
#define CRTC2_PIX_WIDTH			0x000e0000ul	/* LTPro */
#define CRTC_VGA_128KAP_PAGING		0x00100000ul	/* VT/GT */
#define CRTC_DISPREQ_ONLY		0x00200000ul	/* VT/GT */
#define CRTC_VFC_SYNC_TRISTATE		0x00200000ul	/* VTB/GTB/LT */
#define CRTC2_EN			0x00200000ul	/* LTPro */
#define CRTC_LOCK_REGS			0x00400000ul	/* VT/GT */
#define CRTC_SYNC_TRISTATE		0x00800000ul	/* VT/GT */
#define CRTC_EXT_DISP_EN		0x01000000ul
#define CRTC_EN				0x02000000ul
#define CRTC_DISP_REQ_EN		0x04000000ul
#define CRTC_VGA_LINEAR			0x08000000ul
#define CRTC_VSYNC_FALL_EDGE		0x10000000ul
#define CRTC_VGA_TEXT_132		0x20000000ul
#define CRTC_CNT_EN			0x40000000ul
#define CRTC_CUR_B_TEST			0x80000000ul
#define CRTC_INT_ENS_X	/* *** UPDATE ME *** */		\
		(					\
			CRTC_VSYNC_INT_EN |		\
			CRTC2_VSYNC_INT_EN |		\
			0				\
		)
#define CRTC_INT_ACKS_X	/* *** UPDATE ME *** */		\
		(					\
			CRTC_VSYNC_INT |		\
			CRTC2_VSYNC_INT |		\
			0				\
		)
#define DSP_CONFIG		BlockIOTag(0x08u)	/* VTB/GTB/LT */
#define DSP_XCLKS_PER_QW		0x00003ffful
/*	?				0x00004000ul */
#define DSP_FLUSH_WB			0x00008000ul
#define DSP_LOOP_LATENCY		0x000f0000ul
#define DSP_PRECISION			0x00700000ul
/*	?				0xff800000ul */
#define DSP_ON_OFF		BlockIOTag(0x09u)	/* VTB/GTB/LT */
#define DSP_OFF				0x000007fful
/*	?				0x0000f800ul */
#define DSP_ON				0x07ff0000ul
/*	?				0xf8000000ul */
#define TIMER_CONFIG		BlockIOTag(0x0au)	/* VTB/GTB/LT */
#define MEM_BUF_CNTL		BlockIOTag(0x0bu)	/* VTB/GTB/LT */
#define SHARED_CNTL		BlockIOTag(0x0cu)	/* VTB/GTB/LT */
#define SHARED_MEM_CONFIG	BlockIOTag(0x0du)	/* VTB/GTB/LT */
#define MEM_ADDR_CONFIG		BlockIOTag(0x0du)	/* GTPro */
#define SHARED_CNTL_CTD		BlockIOTag(0x0eu)	/* CTD */
/*	?				0x00fffffful */
#define CTD_FIFO5			0x01000000ul
/*	?				0xfe000000ul */
#define CRT_TRAP		BlockIOTag(0x0eu)	/* VTB/GTB/LT */
#define DSTN_CONTROL		BlockIOTag(0x0fu)	/* LT */
#define I2C_CNTL_0		BlockIOTag(0x0fu)	/* GTPro */
#define OVR_CLR			IOPortTag(0x08u, 0x10u)
#define OVR_CLR_8			0x000000fful
#define OVR_CLR_B			0x0000ff00ul
#define OVR_CLR_G			0x00ff0000ul
#define OVR_CLR_R			0xff000000ul
#define OVR_WID_LEFT_RIGHT	IOPortTag(0x09u, 0x11u)
#define OVR_WID_LEFT			0x0000003ful	/* 0x0f on <LT */
/*	?				0x0000ffc0ul */
#define OVR_WID_RIGHT			0x003f0000ul	/* 0x0f0000 on <LT */
/*	?				0xffc00000ul */
#define OVR_WID_TOP_BOTTOM	IOPortTag(0x0au, 0x12u)
#define OVR_WID_TOP			0x000001fful	/* 0x00ff on <LT */
/*	?				0x0000fe00ul */
#define OVR_WID_BOTTOM			0x01ff0000ul	/* 0x00ff0000 on <LT */
/*	?				0xfe000000ul */
#define VGA_DSP_CONFIG		BlockIOTag(0x13u)	/* VTB/GTB/LT */
#define VGA_DSP_XCLKS_PER_QW		DSP_XCLKS_PER_QW
/*	?				0x000fc000ul */
#define VGA_DSP_PREC_PCLKBY2		0x00700000ul
/*	?				0x00800000ul */
#define VGA_DSP_PREC_PCLK		0x07000000ul
/*	?				0xf8000000ul */
#define VGA_DSP_ON_OFF		BlockIOTag(0x14u)	/* VTB/GTB/LT */
#define VGA_DSP_OFF			DSP_OFF
/*	?				0x0000f800ul */
#define VGA_DSP_ON			DSP_ON
/*	?				0xf8000000ul */
#define DSP2_CONFIG		BlockIOTag(0x15u)	/* LTPro */
#define DSP2_ON_OFF		BlockIOTag(0x16u)	/* LTPro */
#define EXT_CRTC_GEN_CNTL	BlockIOTag(0x17u)	/* VT-A4 (W) */
#define CRTC2_OFF_PITCH		BlockIOTag(0x17u)	/* LTPro */
#define CUR_CLR0		IOPortTag(0x0bu, 0x18u)
#define CUR_CLR1		IOPortTag(0x0cu, 0x19u)
/* These are for both CUR_CLR0 and CUR_CLR1 */
#define CUR_CLR_I			0x000000fful
#define CUR_CLR_B			0x0000ff00ul
#define CUR_CLR_G			0x00ff0000ul
#define CUR_CLR_R			0xff000000ul
#define CUR_CLR				(CUR_CLR_R | CUR_CLR_G | CUR_CLR_B)
#define CUR_OFFSET		IOPortTag(0x0du, 0x1au)
#define CUR_HORZ_VERT_POSN	IOPortTag(0x0eu, 0x1bu)
#define CUR_HORZ_POSN			0x000007fful
/*	?				0x0000f800ul */
#define CUR_VERT_POSN			0x07ff0000ul
/*	?				0xf8000000ul */
#define CUR_HORZ_VERT_OFF	IOPortTag(0x0fu, 0x1cu)
#define CUR_HORZ_OFF			0x0000007ful
/*	?				0x0000ff80ul */
#define CUR_VERT_OFF			0x007f0000ul
/*	?				0xff800000ul */
#define CONFIG_PANEL		BlockIOTag(0x1du)	/* LT */
#define PANEL_FORMAT			0x00000007ul
/*	?				0x00000008ul */
#define PANEL_TYPE			0x000000f0ul
#define NO_OF_GREY			0x00000700ul
#define MOD_GEN				0x00001800ul
#define EXT_LVDS_CLK			0x00001800ul	/* LTPro */
#define BLINK_RATE			0x00006000ul
#define BLINK_RATE_PRO			0x00002000ul	/* LTPro */
#define DONT_SHADOW_HEND		0x00004000ul	/* LTPro */
#define DONT_USE_F32KHZ			0x00008000ul
#define LCD_IO_DRIVE			0x00008000ul	/* XC/XL */
#define FP_POL				0x00010000ul
#define LP_POL				0x00020000ul
#define DTMG_POL			0x00040000ul
#define SCK_POL				0x00080000ul
#define DITHER_SEL			0x00300000ul
#define INVERSE_VIDEO_EN		0x00400000ul
#define BL_CLK_SEL			0x01800000ul
#define BL_LEVEL			0x0e000000ul
#define BL_CLK_SEL_PRO			0x00800000ul	/* LTPro */
#define BL_LEVEL_PRO			0x03000000ul	/* LTPro */
#define BIAS_LEVEL_PRO			0x0c000000ul	/* LTPro */
#define HSYNC_DELAY			0xf0000000ul
#define TV_OUT_INDEX		BlockIOTag(0x1du)	/* LTPro */
#define TV_REG_INDEX			0x000000fful
#define TV_ON				0x00000100ul
/*	?				0xfffffe00ul */
#define GP_IO			IOPortTag(0x1eu, 0x1eu)	/* VT/GT */
#define GP_IO_CNTL		BlockIOTag(0x1fu)	/* VT/GT */
#define HW_DEBUG		BlockIOTag(0x1fu)	/* VTB/GTB/LT */
#define FAST_SRCCOPY_DIS		0x00000001ul
#define BYPASS_SUBPIC_DBF		0x00000001ul	/* XL/XC */
#define SRC_AUTONA_FIX_DIS		0x00000002ul
#define SYNC_PD_EN			0x00000002ul	/* Mobility */
#define DISP_QW_FIX_DIS			0x00000004ul
#define GUIDST_WB_EXP_DIS		0x00000008ul
#define CYC_ALL_FIX_DIS			0x00000008ul	/* GTPro */
#define AGPPLL_FIX_EN			0x00000008ul	/* Mobility */
#define SRC_AUTONA_ALWAYS_EN		0x00000010ul
#define GUI_BEATS_HOST_P		0x00000010ul	/* GTPro */
#define DRV_CNTL_DQMB_WEB		0x00000020ul
#define FAST_FILL_SCISSOR_DIS		0x00000020ul	/* GT2c/VT4 */
#define INTER_BLIT_FIX_DIS		0x00000020ul	/* GTPro */
#define DRV_CNTL_MA			0x00000040ul
#define AUTO_BLKWRT_COLOR_DIS		0x00000040ul	/* GT2c/VT4 */
#define INTER_PRIM_DIS			0x00000040ul	/* GTPro */
#define DRV_CNTL_MD			0x00000080ul
#define CHG_DEV_ID			0x00000100ul
#define SRC_TRACK_DST_FIX_DIS		0x00000200ul
#define HCLK_FB_SKEW			0x00000380ul	/* GT2c/VT4 */
#define SRC_TRACK_DST_FIX_DIS_P		0x00000080ul	/* GTPro */
#define AUTO_BLKWRT_COLOR_DIS_P		0x00000100ul	/* GTPro */
#define INTER_LINE_OVERLAP_DIS		0x00000200ul	/* GTPro */
#define MEM_OE_PULLBACK			0x00000400ul
#define DBL_BUFFER_EN			0x00000400ul	/* GTPro */
#define MEM_WE_FIX_DIS			0x00000800ul
#define MEM_OE_PULLBACK_B		0x00000800ul	/* GT2c/VT4 */
#define CMDFIFO_SIZE_DIS_P		0x00000800ul	/* GTPro */
#define RD_EN_FIX_DIS			0x00001000ul
#define MEM_WE_FIX_DIS_B		0x00001000ul
#define AUTO_FF_DIS			0x00001000ul	/* GTPro */
#define CMDFIFO_SIZE_DIS		0x00002000ul	/* GT2c/VT4 */
#define AUTO_BLKWRT_DIS			0x00002000ul	/* GTPro */
#define GUI_BEATS_HOST			0x00004000ul	/* GT2c/VT4 */
#define ORED_INVLD_RB_CACHE		0x00004000ul	/* GTPro */
#define BLOCK_DBL_BUF			0x00008000ul	/* GTPro */
#define R2W_TURNAROUND_DELAY		0x00020000ul	/* GT2c/VT4 */
#define ENA_32BIT_DATA_BUS		0x00040000ul	/* GT2c/VT4 */
#define HCLK_FB_SKEW_P			0x00070000ul	/* GTPro */
#define ENA_FLASH_ROM			0x00080000ul	/* GT2c/VT4 */
#define DISABLE_SWITCH_FIX		0x00080000ul	/* GTPro */
#define MCLK_START_EN			0x00080000ul	/* LTPro */
#define SEL_VBLANK_BDL_BUF		0x00100000ul	/* GTPro */
#define CMDFIFO_64EN			0x00200000ul	/* GTPro */
#define BM_FIX_DIS			0x00400000ul	/* GTPro */
#define Z_SWITCH_EN			0x00800000ul	/* LTPro */
#define FLUSH_HOST_WB			0x01000000ul	/* GTPro */
#define HW_DEBUG_WRITE_MSK_FIX_DIS	0x02000000ul	/* LTPro */
#define Z_NO_WRITE_EN			0x04000000ul	/* LTPro */
#define DISABLE_PCLK_RESET_P		0x08000000ul	/* LTPro */
#define PM_D3_SUPPORT_ENABLE_P		0x10000000ul	/* LTPro */
#define STARTCYCLE_FIX_ENABLE		0x20000000ul	/* LTPro */
#define DONT_RST_CHAREN			0x20000000ul	/* XL/XC */
#define C3_FIX_ENABLE			0x40000000ul	/* LTPro */
#define BM_HOSTRA_EN			0x40000000ul	/* XL/XC */
#define PKGBGAb				0x80000000ul	/* XL/XC */
#define AUTOEXP_HORZ_FIX		0x80000000ul	/* Mobility */
#define SCRATCH_REG0		IOPortTag(0x10u, 0x20u)
#define SCRATCH_REG1		IOPortTag(0x11u, 0x21u)
/*	BIOS_BASE_SEGMENT		0x0000007ful */	/* As above */
/*	?				0x00000f80ul */
#define BIOS_INIT_DAC_SUBTYPE		0x0000f000ul
/*	?				0xffff0000ul */
#define SCRATCH_REG2		BlockIOTag(0x22u)	/* LT */
#define SCRATCH_REG3		BlockIOTag(0x23u)	/* GTPro */
#define CLOCK_CNTL		IOPortTag(0x12u, 0x24u)
#define CLOCK_BIT			0x00000004ul	/* For ICS2595 */
#define CLOCK_PULSE			0x00000008ul	/* For ICS2595 */
#define CLOCK_SELECT			0x0000000ful
#define CLOCK_DIVIDER			0x00000030ul
#define CLOCK_STROBE			0x00000040ul
#define CLOCK_DATA			0x00000080ul
/*	?				0x00000100ul */
#define PLL_WR_EN			0x00000200ul	/* For internal PLL */
#define PLL_ADDR			0x0000fc00ul	/* For internal PLL */
#define PLL_DATA			0x00ff0000ul	/* For internal PLL */
/*	?				0xff000000ul */
#define CONFIG_STAT64_1		BlockIOTag(0x25u)	/* GTPro */
#define CFG_SUBSYS_DEV_ID		0x000000fful
#define CFG_SUBSYS_VEN_ID		0x00ffff00ul
/*	?				0x1f000000ul */
#define CFG_DIMM_TYPE			0xe0000000ul
#define CFG_PCI_SUBSYS_DEV_ID		0x0000fffful	/* XC/XL */
#define CFG_PCI_SUBSYS_VEN_ID		0xffff0000ul	/* XC/XL */
#define CONFIG_STAT64_2		BlockIOTag(0x26u)	/* GTPro */
#define CFG_DIMM_TYPE_3			0x00000001ul
/*	?				0x0000001eul */
#define CFG_ROMWRTEN			0x00000020ul
#define CFG_AGPVCOGAIN			0x000000c0ul
#define CFG_PCI_TYPE			0x00000100ul
#define CFG_AGPSKEW			0x00000e00ul
#define CFG_X1CLKSKEW			0x00007000ul
#define CFG_PANEL_ID_P			0x000f8000ul	/* LTPro */
/*	?				0x00100000ul */
#define CFG_PREFETCH_EN			0x00200000ul
#define CFG_ID_DISABLE			0x00400000ul
#define CFG_PRE_TESTEN			0x00800000ul
/*	?				0x01000000ul */
#define CFG_PCI5VEN			0x02000000ul	/* LTPro */
#define CFG_VGA_DISABLE			0x04000000ul
#define CFG_ENINTB			0x08000000ul
/*	?				0x10000000ul */
#define CFG_ROM_REMAP_2			0x20000000ul
#define CFG_IDSEL			0x40000000ul
/*	?				0x80000000ul */
#define TV_OUT_DATA		BlockIOTag(0x27u)	/* LTPro */
#define BUS_CNTL		IOPortTag(0x13u, 0x28u)
#	define BUS_WS			0x0000000ful
#	define BUS_DBL_RESYNC		0x00000001ul	/* VTB/GTB/LT */
#	define BUS_MSTR_RESET		0x00000002ul	/* VTB/GTB/LT */
#	define BUS_FLUSH_BUF		0x00000004ul	/* VTB/GTB/LT */
#	define BUS_STOP_REQ_DIS		0x00000008ul	/* VTB/GTB/LT */
#	define BUS_ROM_WS		0x000000f0ul
#	define BUS_APER_REG_DIS		0x00000010ul	/* VTB/GTB/LT */
#	define BUS_EXTRA_PIPE_DIS	0x00000020ul	/* VTB/GTB/LT */
#	define BUS_MASTER_DIS		0x00000040ul	/* VTB/GTB/LT */
#	define BUS_ROM_WRT_EN		0x00000080ul	/* GTPro */
#	define BUS_ROM_PAGE		0x00000f00ul
#	define BUS_MINOR_REV_ID		0x00000700ul	/* LTPro */
#	define BUS_EXT_REG_EN		0x08000000ul
/*		First silicom - Prototype (A11)	0x00000000ul */
/*		Metal mask spin (A12 & A13)	0x00000100ul */
/*		All layer spin (A21)		0x00000200ul */
/*		Fast metal spin (A22) - Prod.	0x00000300ul */
/*		All layer spin (A31)		0x00000700ul */
/*	?				0x00000800ul */	/* LTPro */
#define BUS_CHIP_HIDDEN_REV		0x00000300ul	/* XC/XL */
/*	?				0x00001c00ul */	/* XC/XL */
#define BUS_ROM_DIS			0x00001000ul
#define BUS_IO_16_EN			0x00002000ul	/* GX */
#define BUS_PCI_READ_RETRY_EN		0x00002000ul	/* VTB/GTB/LT */
#define BUS_DAC_SNOOP_EN		0x00004000ul
#define BUS_PCI_RETRY_EN		0x00008000ul	/* VT/GT */
#define BUS_PCI_WRT_RETRY_EN		0x00008000ul	/* VTB/GTB/LT */
#define BUS_FIFO_WS			0x000f0000ul
#define BUS_RETRY_WS			0x000f0000ul	/* VTB/GTB/LT */
#define BUS_FIFO_ERR_INT_EN		0x00100000ul
#define BUS_MSTR_RD_MULT		0x00100000ul	/* VTB/GTB/LT */
#define BUS_FIFO_ERR_INT		0x00200000ul
#define BUS_MSTR_RD_LINE		0x00200000ul	/* VTB/GTB/LT */
#define BUS_HOST_ERR_INT_EN		0x00400000ul
#define BUS_SUSPEND			0x00400000ul	/* GTPro */
#define BUS_HOST_ERR_INT		0x00800000ul
#define BUS_LAT16X			0x00800000ul	/* GTPro */
#define BUS_PCI_DAC_WS			0x07000000ul
#define BUS_RD_DISCARD_EN		0x01000000ul	/* VTB/GTB/LT */
#define BUS_RD_ABORT_EN			0x02000000ul	/* VTB/GTB/LT */
#define BUS_MSTR_WS			0x04000000ul	/* VTB/GTB/LT */
#define BUS_PCI_DAC_DLY			0x08000000ul
#define BUS_EXT_REG_EN			0x08000000ul	/* VT/GT */
#define BUS_PCI_MEMW_WS			0x10000000ul
#define BUS_MSTR_DISCONNECT_EN		0x10000000ul	/* VTB/GTB/LT */
#define BUS_PCI_BURST_DEC		0x20000000ul	/* GX/CX */
#define BUS_BURST			0x20000000ul	/* 264xT */
#define BUS_WRT_BURST			0x20000000ul	/* VTB/GTB/LT */
#define BUS_RDY_READ_DLY		0xc0000000ul
#define BUS_READ_BURST			0x40000000ul	/* VTB/GTB/LT */
#define BUS_RDY_READ_DLY_B		0x80000000ul	/* VTB/GTB/LT */
#define LCD_INDEX		BlockIOTag(0x29u)	/* LTPro */
#define LCD_REG_INDEX			0x0000003ful
/*	?				0x000000c0ul */
#define LCD_DISPLAY_DIS			0x00000100ul
#define LCD_SRC_SEL			0x00000200ul
#define LCD_SRC_SEL_CRTC1			0x00000000ul
#define LCD_SRC_SEL_CRTC2			0x00000200ul
#define LCD_CRTC2_DISPLAY_DIS		0x00000400ul
#define LCD_GUI_ACTIVE			0x00000800ul	/* XC/XL */
/*	?				0x00fff000ul */
#define LCD_MONDET_SENSE		0x01000000ul	/* XC/XL */
#define LCD_MONDET_INT_POL		0x02000000ul	/* XC/XL */
#define LCD_MONDET_INT_EN		0x04000000ul	/* XC/XL */
#define LCD_MONDET_INT			0x08000000ul	/* XC/XL */
#define LCD_MONDET_EN			0x10000000ul	/* XC/XL */
#define LCD_EN_PL			0x20000000ul	/* XC/XL */
/*	?				0xc0000000ul */
#define HFB_PITCH_ADDR		BlockIOTag(0x2au)	/* LT */
#define LCD_DATA		BlockIOTag(0x2au)	/* LTPro */
#define EXT_MEM_CNTL		BlockIOTag(0x2bu)	/* VTB/GTB/LT */
#define MEM_CNTL		IOPortTag(0x14u, 0x2cu)
#define CTL_MEM_SIZE			0x00000007ul
/*	?				0x00000008ul */
#define CTL_MEM_REFRESH			0x00000078ul	/* VT/GT */
#define CTL_MEM_SIZEB			0x0000000ful	/* VTB/GTB/LT */
#define CTL_MEM_RD_LATCH_EN		0x00000010ul
#define CTL_MEM_RD_LATCH_DLY		0x00000020ul
#define CTL_MEM_LATENCY			0x00000030ul	/* VTB/GTB/LT */
#define CTL_MEM_SD_LATCH_EN		0x00000040ul
#define CTL_MEM_SD_LATCH_DLY		0x00000080ul
#define CTL_MEM_LATCH			0x000000c0ul	/* VTB/GTB/LT */
#define CTL_MEM_WDOE_CNTL		0x000000c0ul	/* XC/XL */
#define CTL_MEM_FULL_PLS		0x00000100ul
#define CTL_MEM_CYC_LNTH_AUX		0x00000180ul	/* VT/GT */
#define CTL_MEM_TRP			0x00000300ul	/* VTB/GTB/LT */
#define CTL_MEM_CYC_LNTH		0x00000600ul
#define CTL_MEM_REFRESH_RATE		0x00001800ul	/* 264xT */
#define CTL_MEM_TRCD			0x00000c00ul	/* VTB/GTB/LT */
#define CTL_MEM_WR_RDY_SEL		0x00000800ul	/* GX/CX */
#define CTL_MEM_EXT_RMW_CYC_EN		0x00001000ul	/* GX/CX */
#define CTL_MEM_TCRD			0x00001000ul	/* VTB/GTB/LT */
#define CTL_MEM_DLL_RESET		0x00002000ul	/* VT/GT */
#define CTL_MEM_TR2W			0x00002000ul	/* GTPro */
#define CTL_MEM_ACTV_PRE		0x0000c000ul	/* VT/GT */
#define CTL_MEM_CAS_PHASE		0x00004000ul	/* GTPro */
#define CTL_MEM_OE_PULLBACK		0x00008000ul	/* GTPro */
#define CTL_MEM_TWR			0x0000c000ul	/* XC/XL */
#define CTL_MEM_BNDRY			0x00030000ul
#define CTL_MEM_BNDRY_0K			0x00000000ul
#define CTL_MEM_BNDRY_256K			0x00010000ul
#define CTL_MEM_BNDRY_512K			0x00020000ul
#define CTL_MEM_BNDRY_1024K			0x00030000ul
#define CTL_MEM_DLL_GAIN_CNTL		0x00030000ul	/* VT/GT */
#define CTL_MEM_BNDRY_EN		0x00040000ul
#define CTL_MEM_SDRAM_RESET		0x00040000ul	/* VT/GT */
#define CTL_MEM_TRAS			0x00070000ul	/* VTB/GTB/LT */
#define CTL_MEM_TILE_SELECT		0x00180000ul	/* VT/GT */
#define CTL_MEM_REFRESH_DIS		0x00080000ul	/* VTB/GTB/LT */
#define CTL_MEM_LOW_LATENCY_MODE	0x00200000ul	/* VT/GT */
#define CTL_MEM_CDE_PULLBACK		0x00400000ul	/* VT/GT */
#define CTL_MEM_REFRESH_RATE_B		0x00f00000ul	/* VTB/GTB/LT */
#define CTL_MEM_PIX_WIDTH		0x07000000ul
#define CTL_MEM_LOWER_APER_ENDIAN	0x03000000ul	/* VTB/GTB/LT */
#define CTL_MEM_OE_SELECT		0x18000000ul	/* VT/GT */
#define CTL_MEM_UPPER_APER_ENDIAN	0x0c000000ul	/* VTB/GTB/LT */
/*	?				0xe0000000ul */
#define CTL_MEM_PAGE_SIZE		0x30000000ul	/* VTB/GTB/LT */
#define MEM_VGA_WP_SEL		IOPortTag(0x15u, 0x2du)
#define MEM_VGA_WPS0			0x0000fffful
#define MEM_VGA_WPS1			0xffff0000ul
#define MEM_VGA_RP_SEL		IOPortTag(0x16u, 0x2eu)
#define MEM_VGA_RPS0			0x0000fffful
#define MEM_VGA_RPS1			0xffff0000ul
#define LT_GIO			BlockIOTag(0x2fu)	/* LT */
#define I2C_CNTL_1		BlockIOTag(0x2fu)	/* GTPro */
#define DAC_REGS		IOPortTag(0x17u, 0x30u)	/* 4 separate bytes */
#define M64_DAC_WRITE			(DAC_REGS + 0)
#define M64_DAC_DATA			(DAC_REGS + 1)
#define M64_DAC_MASK			(DAC_REGS + 2)
#define M64_DAC_READ			(DAC_REGS + 3)
#define DAC_CNTL		IOPortTag(0x18u, 0x31u)
#define DAC_EXT_SEL			0x00000003ul
#define DAC_EXT_SEL_RS2				0x000000001ul
#define DAC_EXT_SEL_RS3				0x000000002ul
#define DAC_RANGE_CTL			0x00000003ul	/* VTB/GTB/LT */
#define DAC_BLANKING			0x00000004ul	/* 264xT */
#define DAC_CMP_DIS			0x00000008ul	/* 264xT */
#define DAC1_CLK_SEL			0x00000010ul	/* LTPro */
#define DAC_PALETTE_ACCESS_CNTL		0x00000020ul	/* LTPro */
#define DAC_PALETTE2_SNOOP_EN		0x00000040ul	/* LTPro */
#define DAC_CMP_OUTPUT			0x00000080ul	/* 264xT */
#define DAC_8BIT_EN			0x00000100ul
#define DAC_PIX_DLY			0x00000600ul
#define DAC_DIRECT			0x00000400ul	/* VTB/GTB/LT */
#define DAC_BLANK_ADJ			0x00001800ul
#define DAC_PAL_CLK_SEL			0x00000800ul	/* VTB/GTB/LT */
#define DAC_CRT_SENSE			0x00000800ul	/* XC/XL */
#define DAC_CRT_DETECTION_ON		0x00001000ul	/* XC/XL */
#define DAC_VGA_ADR_EN			0x00002000ul
#define DAC_FEA_CON_EN			0x00004000ul	/* 264xT */
#define DAC_PDMN			0x00008000ul	/* 264xT */
#define DAC_TYPE			0x00070000ul
/*	?				0x00f80000ul */
#define DAC_MON_ID_STATE0		0x01000000ul	/* GX-E+/CX */
#define DAC_GIO_STATE_1			0x01000000ul	/* 264xT */
#define DAC_MON_ID_STATE1		0x02000000ul	/* GX-E+/CX */
#define DAC_GIO_STATE_0			0x02000000ul	/* 264xT */
#define DAC_MON_ID_STATE2		0x04000000ul	/* GX-E+/CX */
#define DAC_GIO_STATE_4			0x04000000ul	/* 264xT */
#define DAC_MON_ID_DIR0			0x08000000ul	/* GX-E+/CX */
#define DAC_GIO_DIR_1			0x08000000ul	/* 264xT */
#define DAC_MON_ID_DIR1			0x10000000ul	/* GX-E+/CX */
#define DAC_GIO_DIR_0			0x10000000ul	/* 264xT */
#define DAC_MON_ID_DIR2			0x20000000ul	/* GX-E+/CX */
#define DAC_GIO_DIR_4			0x20000000ul	/* 264xT */
#define DAC_MAN_CMP_STATE		0x40000000ul	/* GX-E+ */
#define DAC_RW_WS			0x80000000ul	/* VT/GT */
#define HORZ_STRETCHING		BlockIOTag(0x32u)	/* LT */
#define HORZ_STRETCH_BLEND		0x00000ffful
#define HORZ_STRETCH_RATIO		0x0000fffful
#define HORZ_STRETCH_LOOP		0x00070000ul
#define HORZ_STRETCH_LOOP09			0x00000000ul
#define HORZ_STRETCH_LOOP11			0x00010000ul
#define HORZ_STRETCH_LOOP12			0x00020000ul
#define HORZ_STRETCH_LOOP14			0x00030000ul
#define HORZ_STRETCH_LOOP15			0x00040000ul
/*	?					0x00050000ul */
/*	?					0x00060000ul */
/*	?					0x00070000ul */
/*	?				0x00080000ul */
#define HORZ_PANEL_SIZE			0x0ff00000ul	/* XC/XL */
/*	?				0x10000000ul */
#define AUTO_HORZ_RATIO			0x20000000ul	/* XC/XL */
#define HORZ_STRETCH_MODE		0x40000000ul
#define HORZ_STRETCH_EN			0x80000000ul
#define EXT_DAC_REGS		BlockIOTag(0x32u)	/* GTPro */
#define VERT_STRETCHING		BlockIOTag(0x33u)	/* LT */
#define VERT_STRETCH_RATIO0		0x000003fful
#define VERT_STRETCH_RATIO1		0x000ffc00ul
#define VERT_STRETCH_RATIO2		0x3ff00000ul
#define VERT_STRETCH_USE0		0x40000000ul
#define VERT_STRETCH_EN			0x80000000ul
#define GEN_TEST_CNTL		IOPortTag(0x19u, 0x34u)
#define GEN_EE_DATA_OUT			0x00000001ul	/* GX/CX */
#define GEN_GIO2_DATA_OUT		0x00000001ul	/* 264xT */
#define GEN_EE_CLOCK			0x00000002ul	/* GX/CX */
/*	?				0x00000002ul */	/* 264xT */
#define GEN_EE_CHIP_SEL			0x00000004ul	/* GX/CX */
#define GEN_GIO3_DATA_OUT		0x00000004ul	/* 264xT */
#define GEN_EE_DATA_IN			0x00000008ul	/* GX/CX */
#define GEN_GIO2_DATA_IN		0x00000008ul	/* 264xT */
#define GEN_EE_EN			0x00000010ul	/* GX/CX */
#define GEN_GIO2_ENABLE			0x00000010ul	/* 264xT */
#define GEN_ICON2_ENABLE		0x00000010ul	/* XC/XL */
#define GEN_OVR_OUTPUT_EN		0x00000020ul	/* GX/CX */
#define GEN_GIO2_WRITE			0x00000020ul	/* 264xT */
#define GEN_CUR2_ENABLE			0x00000020ul	/* XC/XL */
#define GEN_OVR_POLARITY		0x00000040ul	/* GX/CX */
#define GEN_ICON_ENABLE			0x00000040ul	/* XC/XL */
#define GEN_CUR_EN			0x00000080ul
#define GEN_GUI_EN			0x00000100ul	/* GX/CX */
#define GEN_GUI_RESETB			0x00000100ul	/* 264xT */
#define GEN_BLOCK_WR_EN			0x00000200ul	/* GX */
/*	?				0x00000200ul */	/* CX/264xT */
#define GEN_SOFT_RESET			0x00000200ul	/* VTB/GTB/LT */
#define GEN_MEM_TRISTATE		0x00000400ul	/* GTPro */
/*	?				0x00000800ul */
#define GEN_TEST_VECT_MODE		0x00003000ul	/* VT/GT */
/*	?				0x0000c000ul */
#define GEN_TEST_FIFO_EN		0x00010000ul	/* GX/CX */
#define GEN_TEST_GUI_REGS_EN		0x00020000ul	/* GX/CX */
#define GEN_TEST_VECT_EN		0x00040000ul	/* GX/CX */
#define GEN_TEST_CRC_STR		0x00080000ul	/* GX-C/-D */
/*	?				0x00080000ul */	/* GX-E+/CX */
#define GEN_TEST_MODE_T			0x000f0000ul	/* 264xT */
#define GEN_TEST_MODE			0x00700000ul	/* GX/CX */
#define GEN_TEST_CNT_EN			0x00100000ul	/* 264xT */
#define GEN_TEST_CRC_EN			0x00200000ul	/* 264xT */
/*	?				0x00400000ul */	/* 264xT */
/*	?				0x00800000ul */
#define GEN_TEST_MEM_WR			0x01000000ul	/* GX-C/-D */
#define GEN_TEST_MEM_STROBE		0x02000000ul	/* GX-C/-D */
#define GEN_TEST_DST_SS_EN		0x04000000ul	/* GX/CX */
#define GEN_TEST_DST_SS_STROBE		0x08000000ul	/* GX/CX */
#define GEN_TEST_SRC_SS_EN		0x10000000ul	/* GX/CX */
#define GEN_TEST_SRC_SS_STROBE		0x20000000ul	/* GX/CX */
#define GEN_TEST_CNT_VALUE		0x3f000000ul	/* 264xT */
#define GEN_TEST_CC_EN			0x40000000ul	/* GX/CX */
#define GEN_TEST_CC_STROBE		0x80000000ul	/* GX/CX */
/*	?				0xc0000000ul */	/* 264xT */
#define GEN_DEBUG_MODE			0xff000000ul	/* VTB/GTB/LT */
#define LCD_GEN_CTRL		BlockIOTag(0x35u)	/* LT */
#define CRT_ON				0x00000001ul
#define LCD_ON				0x00000002ul
#define HORZ_DIVBY2_EN			0x00000004ul
#define DONT_DS_ICON			0x00000008ul
#define LOCK_8DOT			0x00000010ul
#define ICON_ENABLE			0x00000020ul
#define DONT_SHADOW_VPAR		0x00000040ul
#define V2CLK_PM_EN			0x00000080ul
#define RST_FM				0x00000100ul
#define DISABLE_PCLK_RESET		0x00000200ul	/* XC/XL */
#define DIS_HOR_CRT_DIVBY2		0x00000400ul
#define SCLK_SEL			0x00000800ul
#define SCLK_DELAY			0x0000f000ul
#define TVCLK_PM_EN			0x00010000ul
#define VCLK_DAC_PM_EN			0x00020000ul
#define VCLK_LCD_OFF			0x00040000ul
#define SELECT_WAIT_4MS			0x00080000ul
#define XTALIN_PM_EN			0x00080000ul	/* XC/XL */
#define V2CLK_DAC_PM_EN			0x00100000ul
#define LVDS_EN				0x00200000ul
#define LVDS_PLL_EN			0x00400000ul
#define LVDS_PLL_RESET			0x00800000ul
#define LVDS_RESERVED_BITS		0x07000000ul
#define CRTC_RW_SELECT			0x08000000ul	/* LTPro */
#define USE_SHADOWED_VEND		0x10000000ul
#define USE_SHADOWED_ROWCUR		0x20000000ul
#define SHADOW_EN			0x40000000ul
#define SHADOW_RW_EN			0x80000000ul
#define CUSTOM_MACRO_CNTL	BlockIOTag(0x35u)	/* GTPro */
#define POWER_MANAGEMENT	BlockIOTag(0x36u)	/* LT */
#define PWR_MGT_ON			0x00000001ul
#define PWR_MGT_MODE			0x00000006ul
#define AUTO_PWRUP_EN			0x00000008ul
#define ACTIVITY_PIN_ON			0x00000010ul
#define STANDBY_POL			0x00000020ul
#define SUSPEND_POL			0x00000040ul
#define SELF_REFRESH			0x00000080ul
#define ACTIVITY_PIN_EN			0x00000100ul
#define KEYBD_SNOOP			0x00000200ul
#define USE_F32KHZ			0x00000400ul	/* LTPro */
#define DONT_USE_XTALIN			0x00000400ul	/* XC/XL */
#define TRISTATE_MEM_EN			0x00000800ul	/* LTPro */
#define LCDENG_TEST_MODE		0x0000f000ul
#define STANDBY_COUNT			0x000f0000ul
#define SUSPEND_COUNT			0x00f00000ul
#define BAISON				0x01000000ul
#define BLON				0x02000000ul
#define DIGON				0x04000000ul
#define PM_D3_SUPPORT_ENABLE		0x08000000ul	/* XC/XL */
#define STANDBY_NOW			0x10000000ul
#define SUSPEND_NOW			0x20000000ul
#define PWR_MGT_STATUS			0xc0000000ul
#define CONFIG_CNTL		IOPortTag(0x1au, 0x37u)
#define CFG_MEM_AP_SIZE			0x00000003ul
#define CFG_MEM_VGA_AP_EN		0x00000004ul
/*	?				0x00000008ul */
#define CFG_MEM_AP_LOC			0x00003ff0ul
/*	?				0x0000c000ul */
#define CFG_CARD_ID			0x00070000ul
#define CFG_VGA_DIS			0x00080000ul
/*	?				0x00f00000ul */
#define CFG_CDE_WINDOW			0x3f000000ul	/* VT/GT */
/*	?				0xc0000000ul */
#define CONFIG_CHIP_ID		IOPortTag(0x1bu, 0x38u)	/* Read */
#define CFG_CHIP_TYPE0			0x000000fful
#define CFG_CHIP_TYPE1			0x0000ff00ul
#define CFG_CHIP_TYPE			0x0000fffful
#define CFG_CHIP_CLASS			0x00ff0000ul
#define CFG_CHIP_REV			0xff000000ul
#define CFG_CHIP_VERSION		0x07000000ul	/* 264xT */
#define CFG_CHIP_FOUNDRY		0x38000000ul	/* 264xT */
#define CFG_CHIP_REVISION		0xc0000000ul	/* 264xT */
#define CONFIG_STATUS64_0	IOPortTag(0x1cu, 0x39u)	/* Read (R/W (264xT)) */
#define CFG_BUS_TYPE			0x00000007ul	/* GX/CX */
#define CFG_MEM_TYPE_T			0x00000007ul	/* 264xT */
#define CFG_MEM_TYPE			0x00000038ul	/* GX/CX */
#define CFG_DUAL_CAS_EN_T		0x00000008ul	/* 264xT */
#define CFG_ROM_128K_EN			0x00000008ul	/* VTB/GTB/LT */
#define CFG_ROM_REMAP			0x00000008ul	/* GTPro */
#define CFG_VGA_EN_T			0x00000010ul	/* VT/GT */
#define CFG_CLOCK_EN			0x00000020ul	/* 264xT */
#define CFG_DUAL_CAS_EN			0x00000040ul	/* GX/CX */
#define CFG_VMC_SENSE			0x00000040ul	/* VT/GT */
#define CFG_SHARED_MEM_EN		0x00000040ul	/* VTB/GTB/LT */
#define CFG_LOCAL_BUS_OPTION		0x00000180ul	/* GX/CX */
#define CFG_VFC_SENSE			0x00000080ul	/* VT/GT */
#define CFG_INIT_DAC_TYPE		0x00000e00ul	/* GX/CX */
#define CFG_INIT_CARD_ID		0x00007000ul	/* GX-C/-D */
#define CFG_BLK_WR_SIZE			0x00001000ul	/* GX-E+ */
#define CFG_INT_QSF_EN			0x00002000ul	/* GX-E+ */
/*	?				0x00004000ul */	/* GX-E+ */
/*	?				0x00007000ul */	/* CX */
#define CFG_TRI_BUF_DIS			0x00008000ul	/* GX/CX */
#define CFG_BOARD_ID			0x0000ff00ul	/* VT/GT */
#define CFG_EXT_RAM_ADDR		0x003f0000ul	/* GX/CX */
#define CFG_PANEL_ID			0x001f0000ul	/* LT */
#define CFG_MACROVISION_EN		0x00200000ul	/* GTPro */
#define CFG_ROM_DIS			0x00400000ul	/* GX/CX */
#define CFG_PCI33EN			0x00400000ul	/* GTPro */
#define CFG_VGA_EN			0x00800000ul	/* GX/CX */
#define CFG_FULLAGP			0x00800000ul	/* GTPro */
#define CFG_ARITHMOS_ENABLE		0x00800000ul	/* XC/XL */
#define CFG_LOCAL_BUS_CFG		0x01000000ul	/* GX/CX */
#define CFG_CHIP_EN			0x02000000ul	/* GX/CX */
#define CFG_LOCAL_READ_DLY_DIS		0x04000000ul	/* GX/CX */
#define CFG_ROM_OPTION			0x08000000ul	/* GX/CX */
#define CFG_BUS_OPTION			0x10000000ul	/* GX/CX */
#define CFG_LOCAL_DAC_WR_EN		0x20000000ul	/* GX/CX */
#define CFG_VLB_RDY_DIS			0x40000000ul	/* GX/CX */
#define CFG_AP_4GBYTE_DIS		0x80000000ul	/* GX/CX */
#define CONFIG_STATUS64_1	IOPortTag(0x1du, 0x3au)	/* Read */
#define CFG_PCI_DAC_CFG			0x00000001ul	/* GX/CX */
/*	?				0x0000001eul */	/* GX/CX */
#define CFG_1C8_IO_SEL			0x00000020ul	/* GX/CX */
/*	?				0xffffffc0ul */	/* GX/CX */
#define CRC_SIG				0xfffffffful	/* 264xT */
#define MPP_CONFIG		BlockIOTag(0x3bu)	/* VTB/GTB/LT */
#define MPP_STROBE_CONFIG	BlockIOTag(0x3cu)	/* VTB/GTB/LT */
#define MPP_ADDR		BlockIOTag(0x3du)	/* VTB/GTB/LT */
#define MPP_DATA		BlockIOTag(0x3eu)	/* VTB/GTB/LT */
#define TVO_CNTL		BlockIOTag(0x3fu)	/* VTB/GTB/LT */
/*	GP_IO			IOPortTag(0x1eu, 0x1eu) */	/* See above */
/*	CRTC_H_TOTAL_DISP	IOPortTag(0x1fu, 0x00u) */	/* Duplicate */
#define DST_OFF_PITCH		BlockIOTag(0x40u)
#define DST_OFFSET			0x000ffffful
/*	?				0x00300000ul */
#define DST_PITCH			0xffc00000ul
#define DST_X			BlockIOTag(0x41u)
#define DST_Y			BlockIOTag(0x42u)
#define DST_Y_X			BlockIOTag(0x43u)
#define DST_WIDTH		BlockIOTag(0x44u)
#define DST_HEIGHT		BlockIOTag(0x45u)
#define DST_HEIGHT_WIDTH	BlockIOTag(0x46u)
#define DST_X_WIDTH		BlockIOTag(0x47u)
#define DST_BRES_LNTH		BlockIOTag(0x48u)
#define DST_BRES_ERR		BlockIOTag(0x49u)
#define DST_BRES_INC		BlockIOTag(0x4au)
#define DST_BRES_DEC		BlockIOTag(0x4bu)
#define DST_CNTL		BlockIOTag(0x4cu)
#define DST_X_DIR			0x00000001ul
#define DST_Y_DIR			0x00000002ul
#define DST_Y_MAJOR			0x00000004ul
#define DST_X_TILE			0x00000008ul
#define DST_Y_TILE			0x00000010ul
#define DST_LAST_PEL			0x00000020ul
#define DST_POLYGON_EN			0x00000040ul
#define DST_24_ROT_EN			0x00000080ul
#define DST_24_ROT			0x00000700ul
#define DST_BRES_SIGN			0x00000800ul	/* GX/CX */
#define DST_BRES_ZERO			0x00000800ul	/* CT */
#define DST_POLYGON_RTEDGE_DIS		0x00001000ul	/* CT */
#define TRAIL_X_DIR			0x00002000ul	/* GT */
#define TRAP_FILL_DIR			0x00004000ul	/* GT */
#define TRAIL_BRES_SIGN			0x00008000ul	/* GT */
/*	?				0x00010000ul */
#define BRES_SIGN_AUTO			0x00020000ul	/* GT */
/*	?				0x00040000ul */
#define ALPHA_OVERLAP_ENB		0x00080000ul	/* GTPro */
#define SUB_PIX_ON			0x00100000ul	/* GTPro */
/*	?				0xffe00000ul */
/*	DST_Y_X			BlockIOTag(0x4du) */	/* Duplicate */
#define TRAIL_BRES_ERR		BlockIOTag(0x4eu)	/* GT */
#define TRAIL_BRES_INC		BlockIOTag(0x4fu)	/* GT */
#define TRAIL_BRES_DEC		BlockIOTag(0x50u)	/* GT */
#define LEAD_BRES_LNTH		BlockIOTag(0x51u)	/* GT */
#define Z_OFF_PITCH		BlockIOTag(0x52u)	/* GT */
#define Z_CNTL			BlockIOTag(0x53u)	/* GT */
#define ALPHA_TST_CNTL		BlockIOTag(0x54u)	/* GTPro */
/*	?			BlockIOTag(0x55u) */
#define SECONDARY_STW_EXP	BlockIOTag(0x56u)	/* GTPro */
#define SECONDARY_S_X_INC	BlockIOTag(0x57u)	/* GTPro */
#define SECONDARY_S_Y_INC	BlockIOTag(0x58u)	/* GTPro */
#define SECONDARY_S_START	BlockIOTag(0x59u)	/* GTPro */
#define SECONDARY_W_X_INC	BlockIOTag(0x5au)	/* GTPro */
#define SECONDARY_W_Y_INC	BlockIOTag(0x5bu)	/* GTPro */
#define SECONDARY_W_START	BlockIOTag(0x5cu)	/* GTPro */
#define SECONDARY_T_X_INC	BlockIOTag(0x5du)	/* GTPro */
#define SECONDARY_T_Y_INC	BlockIOTag(0x5eu)	/* GTPro */
#define SECONDARY_T_START	BlockIOTag(0x5fu)	/* GTPro */
#define SRC_OFF_PITCH		BlockIOTag(0x60u)
#define SRC_OFFSET			0x000ffffful
/*	?				0x00300000ul */
#define SRC_PITCH			0xffc00000ul
#define SRC_X			BlockIOTag(0x61u)
#define SRC_Y			BlockIOTag(0x62u)
#define SRC_Y_X			BlockIOTag(0x63u)
#define SRC_WIDTH1		BlockIOTag(0x64u)
#define SRC_HEIGHT1		BlockIOTag(0x65u)
#define SRC_HEIGHT1_WIDTH1	BlockIOTag(0x66u)
#define SRC_X_START		BlockIOTag(0x67u)
#define SRC_Y_START		BlockIOTag(0x68u)
#define SRC_Y_X_START		BlockIOTag(0x69u)
#define SRC_WIDTH2		BlockIOTag(0x6au)
#define SRC_HEIGHT2		BlockIOTag(0x6bu)
#define SRC_HEIGHT2_WIDTH2	BlockIOTag(0x6cu)
#define SRC_CNTL		BlockIOTag(0x6du)
#	define SRC_PATT_EN		0x00000001ul
#	define SRC_PATT_ROT_EN		0x00000002ul
#	define SRC_LINEAR_EN		0x00000004ul
#	define SRC_BYTE_ALIGN		0x00000008ul
#	define SRC_LINE_X_DIR		0x00000010ul
#	define SRC_8X8X8_BRUSH		0x00000020ul	/* VTB/GTB */
#	define FAST_FILL_EN		0x00000040ul	/* VTB/GTB */
#	define SRC_TRACK_DST		0x00000080ul	/* VTB/GTB */
#	define BUS_MASTER_EN		0x00000100ul	/* VTB/GTB */
#	define BUS_MASTER_SYNC		0x00000200ul	/* VTB/GTB */
#	define BUS_MASTER_OP		0x00000c00ul	/* VTB/GTB */
#	define BM_OP_FRAME_TO_SYSTEM	(0 << 10)
#	define BM_OP_SYSTEM_TO_FRAME	(1 << 10)
#	define BM_OP_REG_TO_SYSTEM	(2 << 10)
#	define BM_OP_SYSTEM_TO_REG	(3 << 10)
#	define SRC_8X8X8_BRUSH_LOADED	0x00001000ul	/* VTB/GTB */
#	define COLOR_REG_WRITE_EN	0x00002000ul	/* VTB/GTB */
#	define BLOCK_WRITE_EN		0x00004000ul	/* VTB/GTB */
/*	?				0xffff8000ul */
/*	?			BlockIOTag(0x6eu) */
/*	?			BlockIOTag(0x6fu) */
#define SCALE_Y_OFF		BlockIOTag(0x70u)	/* GT */
#define SCALE_OFF		BlockIOTag(0x70u)	/* GTPro */
#define SECONDARY_SCALE_OFF	BlockIOTag(0x70u)	/* GTPro */
#define TEX_0_OFF		BlockIOTag(0x70u)	/* GT */
#define TEX_1_OFF		BlockIOTag(0x71u)	/* GT */
#define TEX_2_OFF		BlockIOTag(0x72u)	/* GT */
#define TEX_3_OFF		BlockIOTag(0x73u)	/* GT */
#define TEX_4_OFF		BlockIOTag(0x74u)	/* GT */
#define TEX_5_OFF		BlockIOTag(0x75u)	/* GT */
#define TEX_6_OFF		BlockIOTag(0x76u)	/* GT */
#define SCALE_WIDTH		BlockIOTag(0x77u)	/* GT */
#define TEX_7_OFF		BlockIOTag(0x77u)	/* GT */
#define SCALE_HEIGHT		BlockIOTag(0x78u)	/* GT */
#define TEX_8_OFF		BlockIOTag(0x78u)	/* GT */
#define TEX_9_OFF		BlockIOTag(0x79u)	/* GT */
#define TEX_10_OFF		BlockIOTag(0x7au)	/* GT */
#define S_Y_INC			BlockIOTag(0x7bu)	/* GT */
#define SCALE_Y_PITCH		BlockIOTag(0x7bu)	/* GT */
#define SCALE_X_INC		BlockIOTag(0x7cu)	/* GT */
#define RED_X_INC		BlockIOTag(0x7cu)	/* GT */
#define GREEN_X_INC		BlockIOTag(0x7du)	/* GT */
#define SCALE_Y_INC		BlockIOTag(0x7du)	/* GT */
#define SCALE_VACC		BlockIOTag(0x7eu)	/* GT */
#define SCALE_3D_CNTL		BlockIOTag(0x7fu)	/* GT */
#define HOST_DATA_0		BlockIOTag(0x80u)
#define HOST_DATA_1		BlockIOTag(0x81u)
#define HOST_DATA_2		BlockIOTag(0x82u)
#define HOST_DATA_3		BlockIOTag(0x83u)
#define HOST_DATA_4		BlockIOTag(0x84u)
#define HOST_DATA_5		BlockIOTag(0x85u)
#define HOST_DATA_6		BlockIOTag(0x86u)
#define HOST_DATA_7		BlockIOTag(0x87u)
#define HOST_DATA_8		BlockIOTag(0x88u)
#define HOST_DATA_9		BlockIOTag(0x89u)
#define HOST_DATA_A		BlockIOTag(0x8au)
#define HOST_DATA_B		BlockIOTag(0x8bu)
#define HOST_DATA_C		BlockIOTag(0x8cu)
#define HOST_DATA_D		BlockIOTag(0x8du)
#define HOST_DATA_E		BlockIOTag(0x8eu)
#define HOST_DATA_F		BlockIOTag(0x8fu)
#define HOST_CNTL		BlockIOTag(0x90u)
#define HOST_BYTE_ALIGN			0x00000001ul
#define HOST_BIG_ENDIAN_EN		0x00000002ul	/* GX-E/CT */
/*	?				0xfffffffcul */
#define BM_HOSTDATA		BlockIOTag(0x91u)	/* VTB/GTB */
#define BM_ADDR			BlockIOTag(0x92u)	/* VTB/GTB */
#define BM_DATA			BlockIOTag(0x92u)	/* VTB/GTB */
#define BM_GUI_TABLE_CMD	BlockIOTag(0x93u)	/* GTPro */
#	define CIRCULAR_BUF_SIZE_16KB	(0 << 0)
#	define CIRCULAR_BUF_SIZE_32KB	(1 << 0)
#	define CIRCULAR_BUF_SIZE_64KB	(2 << 0)
#	define CIRCULAR_BUF_SIZE_128KB	(3 << 0)
#	define LAST_DESCRIPTOR		(1 << 31)
/*	?			BlockIOTag(0x94u) */
/*	?			BlockIOTag(0x95u) */
/*	?			BlockIOTag(0x96u) */
/*	?			BlockIOTag(0x97u) */
/*	?			BlockIOTag(0x98u) */
/*	?			BlockIOTag(0x99u) */
/*	?			BlockIOTag(0x9au) */
/*	?			BlockIOTag(0x9bu) */
/*	?			BlockIOTag(0x9cu) */
/*	?			BlockIOTag(0x9du) */
/*	?			BlockIOTag(0x9eu) */
/*	?			BlockIOTag(0x9fu) */
#define PAT_REG0		BlockIOTag(0xa0u)
#define PAT_REG1		BlockIOTag(0xa1u)
#define PAT_CNTL		BlockIOTag(0xa2u)
#define PAT_MONO_EN			0x00000001ul
#define PAT_CLR_4x2_EN			0x00000002ul
#define PAT_CLR_8x1_EN			0x00000004ul
/*	?				0xfffffff8ul */
/*	?			BlockIOTag(0xa3u) */
/*	?			BlockIOTag(0xa4u) */
/*	?			BlockIOTag(0xa5u) */
/*	?			BlockIOTag(0xa6u) */
/*	?			BlockIOTag(0xa7u) */
#define SC_LEFT			BlockIOTag(0xa8u)
#define SC_RIGHT		BlockIOTag(0xa9u)
#define SC_LEFT_RIGHT		BlockIOTag(0xaau)
#define SC_TOP			BlockIOTag(0xabu)
#define SC_BOTTOM		BlockIOTag(0xacu)
#define SC_TOP_BOTTOM		BlockIOTag(0xadu)
#define USR1_DST_OFF_PITCH	BlockIOTag(0xaeu)	/* LTPro */
#define USR2_DST_OFF_PITCH	BlockIOTag(0xafu)	/* LTPro */
#define DP_BKGD_CLR		BlockIOTag(0xb0u)
#define DP_FRGD_CLR		BlockIOTag(0xb1u)
#define DP_WRITE_MASK		BlockIOTag(0xb2u)
#define DP_CHAIN_MASK		BlockIOTag(0xb3u)
#define DP_CHAIN_1BPP			0x00000000ul	/* Irrelevant */
#define DP_CHAIN_4BPP			0x00008888ul
#define DP_CHAIN_8BPP			0x00008080ul
#define DP_CHAIN_8BPP_332		0x00009292ul
#define DP_CHAIN_15BPP_1555		0x00004210ul
#define DP_CHAIN_16BPP_565		0x00008410ul
#define DP_CHAIN_24BPP_888		0x00008080ul
#define DP_CHAIN_32BPP_8888		0x00008080ul
/*	?				0xffff0000ul */
#define DP_PIX_WIDTH		BlockIOTag(0xb4u)
#define DP_DST_PIX_WIDTH		0x0000000ful
#define COMPOSITE_PIX_WIDTH		0x000000f0ul	/* GTPro */
#define DP_SRC_PIX_WIDTH		0x00000f00ul
/*	?				0x00001000ul */
#define DP_HOST_TRIPLE_EN		0x00002000ul	/* GT2c/VT4 */
#define DP_SRC_AUTONA_FIX_DIS		0x00004000ul	/* GTB */
#define DP_FAST_SRCCOPY_DIS		0x00008000ul	/* GTB */
#define DP_HOST_PIX_WIDTH		0x000f0000ul
#define DP_CI4_RGB_INDEX		0x00f00000ul	/* GTB */
#define DP_BYTE_PIX_ORDER		0x01000000ul
#define DP_CONVERSION_TEMP		0x02000000ul	/* GTB */
#define DP_CI4_RGB_LOW_NIBBLE		0x04000000ul	/* GTB */
#define DP_C14_RGB_HIGH_NIBBLE		0x08000000ul	/* GTB */
#define DP_SCALE_PIX_WIDTH		0xf0000000ul	/* GTB */
#define DP_MIX			BlockIOTag(0xb5u)
#	define BKGD_MIX_NOT_D		(0 << 0)
#	define BKGD_MIX_ZERO		(1 << 0)
#	define BKGD_MIX_ONE		(2 << 0)
#	define BKGD_MIX_D	(3 << 0)
#	define BKGD_MIX_NOT_S		(4 << 0)
#	define BKGD_MIX_D_XOR_S		(5 << 0)
#	define BKGD_MIX_NOT_D_XOR_S	(6 << 0)
#	define BKGD_MIX_S	(7 << 0)
#	define BKGD_MIX_NOT_D_OR_NOT_S	(8 << 0)
#	define BKGD_MIX_D_OR_NOT_S	(9 << 0)
#	define BKGD_MIX_NOT_D_OR_S	(10 << 0)
#	define BKGD_MIX_D_OR_S		(11 << 0)
#	define BKGD_MIX_D_AND_S		(12 << 0)
#	define BKGD_MIX_NOT_D_AND_S	(13 << 0)
#	define BKGD_MIX_D_AND_NOT_S	(14 << 0)
#	define BKGD_MIX_NOT_D_AND_NOT_S	(15 << 0)
#	define BKGD_MIX_D_PLUS_S_DIV2	(23 << 0)
#	define FRGD_MIX_NOT_D		(0 << 16)
#	define FRGD_MIX_ZERO		(1 << 16)
#	define FRGD_MIX_ONE		(2 << 16)
#	define FRGD_MIX_D		(3 << 16)
#	define FRGD_MIX_NOT_S		(4 << 16)
#	define FRGD_MIX_D_XOR_S		(5 << 16)
#	define FRGD_MIX_NOT_D_XOR_S	(6 << 16)
#	define FRGD_MIX_S		(7 << 16)
#	define FRGD_MIX_NOT_D_OR_NOT_S	(8 << 16)
#	define FRGD_MIX_D_OR_NOT_S	(9 << 16)
#	define FRGD_MIX_NOT_D_OR_S	(10 << 16)
#	define FRGD_MIX_D_OR_S		(11 << 16)
#	define FRGD_MIX_D_AND_S		(12 << 16)
#	define FRGD_MIX_NOT_D_AND_S	(13 << 16)
#	define FRGD_MIX_D_AND_NOT_S	(14 << 16)
#	define FRGD_MIX_NOT_D_AND_NOT_S	(15 << 16)
#	define FRGD_MIX_D_PLUS_S_DIV2	(23 << 16)
#define DP_SRC			BlockIOTag(0xb6u)
#	define BKGD_SRC_BKGD_CLR	(0 << 0)
#	define BKGD_SRC_FRGD_CLR	(1 << 0)
#	define BKGD_SRC_HOST		(2 << 0)
#	define BKGD_SRC_BLIT		(3 << 0)
#	define BKGD_SRC_PATTERN		(4 << 0)
#	define BKGD_SRC_3D		(5 << 0)
#	define FRGD_SRC_BKGD_CLR	(0 << 8)
#	define FRGD_SRC_FRGD_CLR	(1 << 8)
#	define FRGD_SRC_HOST		(2 << 8)
#	define FRGD_SRC_BLIT		(3 << 8)
#	define FRGD_SRC_PATTERN		(4 << 8)
#	define FRGD_SRC_3D		(5 << 8)
#	define MONO_SRC_ONE		(0 << 16)
#	define MONO_SRC_PATTERN		(1 << 16)
#	define MONO_SRC_HOST		(2 << 16)
#	define MONO_SRC_BLIT		(3 << 16)
#define DP_FRGD_CLR_MIX		BlockIOTag(0xb7u)	/* VTB/GTB */
#define DP_FRGD_BKGD_CLR	BlockIOTag(0xb8u)	/* VTB/GTB */
/*	?			BlockIOTag(0xb9u) */
#define DST_X_Y			BlockIOTag(0xbau)	/* VTB/GTB */
#define DST_WIDTH_HEIGHT	BlockIOTag(0xbbu)	/* VTB/GTB */
#define USR_DST_PITCH		BlockIOTag(0xbcu)	/* GTPro */
/*	?			BlockIOTag(0xbdu) */
#define DP_SET_GUI_ENGINE2	BlockIOTag(0xbeu)	/* GTPro */
#define DP_SET_GUI_ENGINE	BlockIOTag(0xbfu)	/* VTB/GTB */
#define CLR_CMP_CLR		BlockIOTag(0xc0u)
#define CLR_CMP_MSK		BlockIOTag(0xc1u)
#define CLR_CMP_CNTL		BlockIOTag(0xc2u)
#define CLR_CMP_FN			0x00000007ul
#define CLR_CMP_FN_FALSE			0x00000000ul
#define CLR_CMP_FN_TRUE				0x00000001ul
/*	?					0x00000002ul */
/*	?					0x00000003ul */
#define CLR_CMP_FN_NOT_EQUAL			0x00000004ul
#define CLR_CMP_FN_EQUAL			0x00000005ul
/*	?					0x00000006ul */
/*	?					0x00000007ul */
/*	?				0x00fffff8ul */
#define CLR_CMP_SRC			0x03000000ul
#define CLR_CMP_SRC_DST				0x00000000ul
#define CLR_CMP_SRC_2D				0x01000000ul
#define CLR_CMP_SRC_TEXEL			0x02000000ul
/*	?					0x03000000ul */
/*	?				0xfc000000ul */
/*	?			BlockIOTag(0xc3u) */
#define FIFO_STAT		BlockIOTag(0xc4u)
#define FIFO_STAT_BITS			0x0000fffful
/*	?				0x7fff0000ul */
#define FIFO_ERR			0x80000000ul
/*	?			BlockIOTag(0xc5u) */
/*	?			BlockIOTag(0xc6u) */
/*	?			BlockIOTag(0xc7u) */
#define CONTEXT_MASK		BlockIOTag(0xc8u)
/*	?			BlockIOTag(0xc9u) */
/*	?			BlockIOTag(0xcau) */
#define CONTEXT_LOAD_CNTL	BlockIOTag(0xcbu)
#define CONTEXT_LOAD_PTR		0x00007ffful
/*	?				0x00008000ul */
#define CONTEXT_LOAD_CMD		0x00030000ul
#define CONTEXT_LOAD_NONE			0x00000000ul
#define CONTEXT_LOAD_ONLY			0x00010000ul
#define CONTEXT_LOAD_FILL			0x00020000ul
#define CONTEXT_LOAD_LINE			0x00030000ul
/*	?				0x7ffc0000ul */
#define CONTEXT_LOAD_DIS		0x80000000ul
#define GUI_TRAJ_CNTL		BlockIOTag(0xccu)
/*	?			BlockIOTag(0xcdu) */
#define GUI_STAT		BlockIOTag(0xceu)
#define GUI_ACTIVE			0x00000001ul
/*	?				0x000000feul */
#define DSTX_LT_SCISSOR_LEFT		0x00000100ul
#define DSTX_GT_SCISSOR_RIGHT		0x00000200ul
#define DSTY_LT_SCISSOR_TOP		0x00000400ul
#define DSTY_GT_SCISSOR_BOTTOM		0x00000800ul
/*	?				0x0000f000ul */
#define GUI_FIFO			0x03ff0000ul	/* VTB/GTB */
/*	?				0xfc000000ul */
/*	?			BlockIOTag(0xcfu) */
#define S_X_INC2		BlockIOTag(0xd0u)	/* GTB */
#define TEX_PALETTE_INDEX	BlockIOTag(0xd0u)	/* GTPro */
#define S_Y_INC2		BlockIOTag(0xd1u)	/* GTB */
#define STW_EXP			BlockIOTag(0xd1u)	/* GTPro */
#define S_XY_INC2		BlockIOTag(0xd2u)	/* GTB */
#define LOG_MAX_INC		BlockIOTag(0xd2u)	/* GTPro */
#define S_XINC_START		BlockIOTag(0xd3u)	/* GTB */
/*	S_Y_INC			BlockIOTag(0xd4u) */	/* Duplicate */
/*	SCALE_Y_PITCH		BlockIOTag(0xd4u) */	/* Duplicate */
#define S_START			BlockIOTag(0xd5u)	/* GTB */
#define T_X_INC2		BlockIOTag(0xd6u)	/* GTB */
#define W_X_INC			BlockIOTag(0xd6u)	/* GTPro */
#define T_Y_INC2		BlockIOTag(0xd7u)	/* GTB */
#define W_Y_INC			BlockIOTag(0xd7u)	/* GTPro */
#define T_XY_INC2		BlockIOTag(0xd8u)	/* GTB */
#define W_START			BlockIOTag(0xd8u)	/* GTPro */
#define T_XINC_START		BlockIOTag(0xd9u)	/* GTB */
#define T_Y_INC			BlockIOTag(0xdau)	/* GTB */
#define SECONDARY_SCALE_PITCH	BlockIOTag(0xdau)	/* GTPro */
#define T_START			BlockIOTag(0xdbu)	/* GTB */
#define TEX_SIZE_PITCH		BlockIOTag(0xdcu)	/* GTB */
#define TEX_CNTL		BlockIOTag(0xddu)	/* GTPro */
#define SECONDARY_TEX_OFFSET	BlockIOTag(0xdeu)	/* GTPro */
#define TEX_PAL_WR		BlockIOTag(0xdfu)	/* GTB */
#define TEX_PALETTE		BlockIOTag(0xdfu)	/* GTPro */
#define SCALE_PITCH_BOTH	BlockIOTag(0xe0u)	/* GTPro */
#define SECONDARY_SCALE_OFF_ACC	BlockIOTag(0xe1u)	/* GTPro */
#define SCALE_OFF_ACC		BlockIOTag(0xe2u)	/* GTPro */
#define SCALE_DST_Y_X		BlockIOTag(0xe3u)	/* GTPro */
/*	?			BlockIOTag(0xe4u) */
/*	?			BlockIOTag(0xe5u) */
#define COMPOSITE_SHADOW_ID	BlockIOTag(0xe6u)	/* GTPro */
#define SECONDARY_SCALE_X_INC	BlockIOTag(0xe7u)	/* GTPro */
#define SPECULAR_RED_X_INC	BlockIOTag(0xe7u)	/* GTPro */
#define SPECULAR_RED_Y_INC	BlockIOTag(0xe8u)	/* GTPro */
#define SPECULAR_RED_START	BlockIOTag(0xe9u)	/* GTPro */
#define SECONDARY_SCALE_HACC	BlockIOTag(0xe9u)	/* GTPro */
#define SPECULAR_GREEN_X_INC	BlockIOTag(0xeau)	/* GTPro */
#define SPECULAR_GREEN_Y_INC	BlockIOTag(0xebu)	/* GTPro */
#define SPECULAR_GREEN_START	BlockIOTag(0xecu)	/* GTPro */
#define SPECULAR_BLUE_X_INC	BlockIOTag(0xedu)	/* GTPro */
#define SPECULAR_BLUE_Y_INC	BlockIOTag(0xeeu)	/* GTPro */
#define SPECULAR_BLUE_START	BlockIOTag(0xefu)	/* GTPro */
/*	SCALE_X_INC		BlockIOTag(0xf0u) */	/* Duplicate */
/*	RED_X_INC		BlockIOTag(0xf0u) */	/* Duplicate */
#define RED_Y_INC		BlockIOTag(0xf1u)	/* GTB */
#define SCALE_HACC		BlockIOTag(0xf2u)	/* GTB */
#define RED_START		BlockIOTag(0xf2u)	/* GTB */
/*	GREEN_X_INC		BlockIOTag(0xf3u) */	/* Duplicate */
/*	SCALE_Y_INC		BlockIOTag(0xf3u) */	/* Duplicate */
#define GREEN_Y_INC		BlockIOTag(0xf4u)	/* GTB */
#define SECONDARY_SCALE_Y_INC	BlockIOTag(0xf4u)	/* GTPro */
#define SECONDARY_SCALE_VACC	BlockIOTag(0xf5u)	/* GTPro */
#define GREEN_START		BlockIOTag(0xf5u)	/* GTB */
#define BLUE_X_INC		BlockIOTag(0xf6u)	/* GTB */
#define SCALE_XUV_INC		BlockIOTag(0xf6u)	/* GTB */
#define BLUE_Y_INC		BlockIOTag(0xf7u)	/* GTB */
#define BLUE_START		BlockIOTag(0xf8u)	/* GTB */
#define SCALE_UV_HACC		BlockIOTag(0xf8u)	/* GTB */
#define Z_X_INC			BlockIOTag(0xf9u)	/* GTB */
#define Z_Y_INC			BlockIOTag(0xfau)	/* GTB */
#define Z_START			BlockIOTag(0xfbu)	/* GTB */
#define ALPHA_FOG_X_INC		BlockIOTag(0xfcu)	/* GTB */
#define ALPHA_FOG_Y_INC		BlockIOTag(0xfdu)	/* GTB */
#define ALPHA_FOG_START		BlockIOTag(0xfeu)	/* GTB */
/*	?			BlockIOTag(0xffu) */
#define OVERLAY_Y_X_START	BlockIOTag(0x100u)
#define OVERLAY_Y_X_END		BlockIOTag(0x101u)
#define OVERLAY_VIDEO_KEY_CLR	BlockIOTag(0x102u)
#define OVERLAY_VIDEO_KEY_MSK	BlockIOTag(0x103u)
#define OVERLAY_GRAPHICS_KEY_CLR BlockIOTag(0x104u)
#define OVERLAY_GRAPHICS_KEY_MSK BlockIOTag(0x105u)
#define OVERLAY_KEY_CNTL	BlockIOTag(0x106u)
#	define VIDEO_KEY_FN_MASK		0x00000007L
#	define VIDEO_KEY_FN_FALSE		0x00000000L
#	define VIDEO_KEY_FN_TRUE		0x00000001L
#	define VIDEO_KEY_FN_NE			0x00000004L
#	define VIDEO_KEY_FN_EQ			0x00000005L // EQ and NE are exchanged relative to radeon
#	define GRAPHIC_KEY_FN_MASK		0x00000070L
#	define GRAPHIC_KEY_FN_FALSE		0x00000000L
#	define GRAPHIC_KEY_FN_TRUE		0x00000010L
#	define GRAPHIC_KEY_FN_NE		0x00000040L
#	define GRAPHIC_KEY_FN_EQ		0x00000050L // EQ and NE are exchanged relative to radeon
#	define CMP_MIX_MASK			0x00000100L
#	define CMP_MIX_OR			0x00000000L
#	define CMP_MIX_AND			0x00000100L   
/*	?			BlockIOTag(0x107u) */
#define OVERLAY_SCALE_INC	BlockIOTag(0x108u)
#define OVERLAY_SCALE_CNTL	BlockIOTag(0x109u)
#define SCALER_HEIGHT_WIDTH	BlockIOTag(0x10au)
#define OVERLAY_TEST		BlockIOTag(0x10bu)
#define SCALER_THRESHOLD	BlockIOTag(0x10cu)
#define SCALER_BUF0_OFFSET	BlockIOTag(0x10du)	/* VTB/GTB */
#define SCALER_BUF1_OFFSET	BlockIOTag(0x10eu)	/* VTB/GTB */
#define SCALER_BUF_PITCH	BlockIOTag(0x10fu)	/* VTB/GTB */
#define CAPTURE_Y_X		BlockIOTag(0x110u)
#define CAPTURE_START_END	BlockIOTag(0x110u)	/* VTB/GTB */
#define CAPTURE_HEIGHT_WIDTH	BlockIOTag(0x111u)
#define CAPTURE_X_WIDTH		BlockIOTag(0x111u)	/* VTB/GTB */
#define VIDEO_FORMAT		BlockIOTag(0x112u)
#define VIDEO_CONFIG		BlockIOTag(0x113u)
#define VBI_START_END		BlockIOTag(0x113u)	/* VTB/GTB */
#define CAPTURE_CONFIG		BlockIOTag(0x114u)
#define TRIG_CNTL		BlockIOTag(0x115u)
#define VIDEO_SYNC_TEST		BlockIOTag(0x116u)
#define OVERLAY_EXCLUSIVE_HORZ	BlockIOTag(0x116u)	/* VTB/GTB */
#define EXT_CRTC_GEN_CNTL_R	BlockIOTag(0x117u)	/* VT-A4 (R) */
#define OVERLAY_EXCLUSIVE_VERT	BlockIOTag(0x117u)	/* VTB/GTB */
#define VMC_CONFIG		BlockIOTag(0x118u)
#define VBI_WIDTH		BlockIOTag(0x118u)	/* VTB/GTB */
#define VMC_STATUS		BlockIOTag(0x119u)
#define CAPTURE_DEBUG		BlockIOTag(0x119u)	/* VTB/GTB */
#define VMC_CMD			BlockIOTag(0x11au)
#define VIDEO_SYNC_TEST_B	BlockIOTag(0x11au)	/* VTB/GTB */
#define VMC_ARG0		BlockIOTag(0x11bu)
#define VMC_ARG1		BlockIOTag(0x11cu)
#define SNAPSHOT_VH_COUNTS	BlockIOTag(0x11cu)	/* GTPro */
#define VMC_SNOOP_ARG0		BlockIOTag(0x11du)
#define SNAPSHOT_F_COUNT	BlockIOTag(0x11du)	/* GTPro */
#define VMC_SNOOP_ARG1		BlockIOTag(0x11eu)
#define N_VIF_COUNT		BlockIOTag(0x11eu)	/* GTPro */
#define SNAPSHOT_VIF_COUNT	BlockIOTag(0x11fu)	/* GTPro */
#define BUF0_OFFSET		BlockIOTag(0x120u)
#define CAPTURE_BUF0_OFFSET	BlockIOTag(0x120u)	/* VTB/GTB */
#define CAPTURE_BUF1_OFFSET	BlockIOTag(0x121u)	/* VTB/GTB */
#define ONESHOT_BUF_OFFSET	BlockIOTag(0x122u)	/* VTB/GTB */
#define BUF0_PITCH		BlockIOTag(0x123u)
/*	?			BlockIOTag(0x124u) */
/*	?			BlockIOTag(0x125u) */
#define BUF1_OFFSET		BlockIOTag(0x126u)
/*	?			BlockIOTag(0x127u) */
/*	?			BlockIOTag(0x128u) */
#define BUF1_PITCH		BlockIOTag(0x129u)
/*	?			BlockIOTag(0x12au) */
#define BUF0_CAP_ODD_OFFSET	BlockIOTag(0x12bu)
#define BUF1_CAP_ODD_OFFSET	BlockIOTag(0x12cu)
#define SNAPSHOT2_VH_COUNTS	BlockIOTag(0x12cu)	/* LTPro */
#define SNAPSHOT2_F_COUNT	BlockIOTag(0x12du)	/* LTPro */
#define N_VIF2_COUNT		BlockIOTag(0x12eu)	/* LTPro */
#define SNAPSHOT2_VIF_COUNT	BlockIOTag(0x12fu)	/* LTPro */
#define VMC_STRM_DATA_0		BlockIOTag(0x130u)
/*	MPP_CONFIG		BlockIOTag(0x130u) */	/* See 0x3bu */
#define VMC_STRM_DATA_1		BlockIOTag(0x131u)
/*	MPP_STROBE_SEQ		BlockIOTag(0x131u) */	/* See 0x3cu */
#define VMC_STRM_DATA_2		BlockIOTag(0x132u)
/*	MPP_ADDR		BlockIOTag(0x132u) */	/* See 0x3du */
#define VMC_STRM_DATA_3		BlockIOTag(0x133u)
/*	MPP_DATA		BlockIOTag(0x133u) */	/* See 0x3eu */
#define VMC_STRM_DATA_4		BlockIOTag(0x134u)
#define VMC_STRM_DATA_5		BlockIOTag(0x135u)
#define VMC_STRM_DATA_6		BlockIOTag(0x136u)
#define VMC_STRM_DATA_7		BlockIOTag(0x137u)
#define VMC_STRM_DATA_8		BlockIOTag(0x138u)
#define VMC_STRM_DATA_9		BlockIOTag(0x139u)
#define VMC_STRM_DATA_A		BlockIOTag(0x13au)
#define VMC_STRM_DATA_B		BlockIOTag(0x13bu)
#define VMC_STRM_DATA_C		BlockIOTag(0x13cu)
#define VMC_STRM_DATA_D		BlockIOTag(0x13du)
#define VMC_STRM_DATA_E		BlockIOTag(0x13eu)
#define VMC_STRM_DATA_F		BlockIOTag(0x13fu)
/*	TVO_CNTL		BlockIOTag(0x140u) */	/* See 0x3fu */
/*	?			BlockIOTag(0x141u) */
/*	?			BlockIOTag(0x142u) */
/*	?			BlockIOTag(0x143u) */
/*	?			BlockIOTag(0x144u) */
/*	?			BlockIOTag(0x145u) */
/*	?			BlockIOTag(0x146u) */
/*	?			BlockIOTag(0x147u) */
/*	?			BlockIOTag(0x148u) */
/*	?			BlockIOTag(0x149u) */
/*	?			BlockIOTag(0x14au) */
/*	?			BlockIOTag(0x14bu) */
/*	?			BlockIOTag(0x14cu) */
/*	?			BlockIOTag(0x14du) */
/*	?			BlockIOTag(0x14eu) */
/*	?			BlockIOTag(0x14fu) */
/*	?			BlockIOTag(0x150u) */
#define CRT_HORZ_VERT_LOAD	BlockIOTag(0x151u)	/* VTB/GTB */
#define AGP_BASE		BlockIOTag(0x152u)	/* GTPro */
#define AGP_CNTL		BlockIOTag(0x153u)	/* GTPro */
#define SCALER_COLOUR_CNTL	BlockIOTag(0x154u)	/* GTPro */
#define SCALER_H_COEFF0		BlockIOTag(0x155u)	/* GTPro */
#define SCALER_H_COEFF1		BlockIOTag(0x156u)	/* GTPro */
#define SCALER_H_COEFF2		BlockIOTag(0x157u)	/* GTPro */
#define SCALER_H_COEFF3		BlockIOTag(0x158u)	/* GTPro */
#define SCALER_H_COEFF4		BlockIOTag(0x159u)	/* GTPro */
/*	?			BlockIOTag(0x15au) */
/*	?			BlockIOTag(0x15bu) */
#define GUI_CMDFIFO_DEBUG	BlockIOTag(0x15cu)	/* GT2c/VT4 */
#define GUI_CMDFIFO_DATA	BlockIOTag(0x15du)	/* GT2c/VT4 */
#define GUI_CNTL		BlockIOTag(0x15eu)	/* GT2c/VT4 */
#	define CMDFIFO_SIZE_MASK	0x00000003ul
#	define CMDFIFO_SIZE_192		0x00000000ul
#	define CMDFIFO_SIZE_128		0x00000001ul
#	define CMDFIFO_SIZE_64		0x00000002ul
/*	?				0x0000fffcul */
#	define IDCT_PRSR_MODE		0x00010000ul	/* XL/XC */
#	define IDCT_BLOCK_GUI_INITIATOR	0x00020000ul	/* XL/XC */
/*	?				0xfffc0000ul */
/*	?			BlockIOTag(0x15fu) */
/* BUS MASTERING */
#define BM_FRAME_BUF_OFFSET	BlockIOTag(0x160u)	/* VTB/GTB */
#define BM_SYSTEM_MEM_ADDR	BlockIOTag(0x161u)	/* VTB/GTB */
#define BM_COMMAND		BlockIOTag(0x162u)	/* VTB/GTB */
#define BM_STATUS		BlockIOTag(0x163u)	/* VTB/GTB */
/*	?			BlockIOTag(0x164u) */
/*	?			BlockIOTag(0x165u) */
/*	?			BlockIOTag(0x166u) */
/*	?			BlockIOTag(0x167u) */
/*	?			BlockIOTag(0x168u) */
/*	?			BlockIOTag(0x169u) */
/*	?			BlockIOTag(0x16au) */
/*	?			BlockIOTag(0x16bu) */
/*	?			BlockIOTag(0x16cu) */
/*	?			BlockIOTag(0x16du) */
#define BM_GUI_TABLE		BlockIOTag(0x16eu)	/* VTB/GTB */
#define BM_SYSTEM_TABLE		BlockIOTag(0x16fu)	/* VTB/GTB */
#	define DMA_GUI_COMMAND__BYTE_COUNT_MASK			0x001fffff
#	define DMA_GUI_COMMAND__HOLD_VIDEO_OFFSET		0x40000000
#	define DMA_GUI_COMMAND__EOL				0x80000000
#	define SYSTEM_TRIGGER_SYSTEM_TO_VIDEO				0x0
#	define SYSTEM_TRIGGER_VIDEO_TO_SYSTEM				0x1
#	define SYSTEM_TRIGGER_VIDEO_TO_SYSTEM_AFTER_BUF0_READY		0x2
#	define SYSTEM_TRIGGER_VIDEO_TO_SYSTEM_AFTER_BUF1_READY		0x3
#	define SYSTEM_TRIGGER_VIDEO_TO_SYSTEM_AFTER_SNAPSHOT_READY	0x4
/*	?			BlockIOTag(0x170u) */
/*	?			BlockIOTag(0x171u) */
/*	?			BlockIOTag(0x172u) */
/*	?			BlockIOTag(0x173u) */
/*	?			BlockIOTag(0x174u) */
#define SCALER_BUF0_OFFSET_V	BlockIOTag(0x175u)	/* GTPro */
#define SCALER_BUF0_OFFSET_U	BlockIOTag(0x176u)	/* GTPro */
#define SCALER_BUF1_OFFSET_V	BlockIOTag(0x177u)	/* GTPro */
#define SCALER_BUF1_OFFSET_U	BlockIOTag(0x178u)	/* GTPro */
/*	?			BlockIOTag(0x179u) */
/*	?			BlockIOTag(0x17au) */
/*	?			BlockIOTag(0x17bu) */
/*	?			BlockIOTag(0x17cu) */
/*	?			BlockIOTag(0x17du) */
/*	?			BlockIOTag(0x17eu) */
/*	?			BlockIOTag(0x17fu) */
/*	?			BlockIOTag(0x180u) */
/*	?			BlockIOTag(0x181u) */
/*	?			BlockIOTag(0x182u) */
/*	?			BlockIOTag(0x183u) */
/*	?			BlockIOTag(0x184u) */
/*	?			BlockIOTag(0x185u) */
/*	?			BlockIOTag(0x186u) */
/*	?			BlockIOTag(0x187u) */
/*	?			BlockIOTag(0x188u) */
/*	?			BlockIOTag(0x189u) */
/*	?			BlockIOTag(0x18au) */
/*	?			BlockIOTag(0x18bu) */
/*	?			BlockIOTag(0x18cu) */
/*	?			BlockIOTag(0x18du) */
/*	?			BlockIOTag(0x18eu) */
/*	?			BlockIOTag(0x18fu) */
#define VERTEX_1_S		BlockIOTag(0x190u)	/* GTPro */
#define VERTEX_1_T		BlockIOTag(0x191u)	/* GTPro */
#define VERTEX_1_W		BlockIOTag(0x192u)	/* GTPro */
#define VERTEX_1_SPEC_ARGB	BlockIOTag(0x193u)	/* GTPro */
#define VERTEX_1_Z		BlockIOTag(0x194u)	/* GTPro */
#define VERTEX_1_ARGB		BlockIOTag(0x195u)	/* GTPro */
#define VERTEX_1_X_Y		BlockIOTag(0x196u)	/* GTPro */
#define ONE_OVER_AREA		BlockIOTag(0x197u)	/* GTPro */
#define VERTEX_2_S		BlockIOTag(0x198u)	/* GTPro */
#define VERTEX_2_T		BlockIOTag(0x199u)	/* GTPro */
#define VERTEX_2_W		BlockIOTag(0x19au)	/* GTPro */
#define VERTEX_2_SPEC_ARGB	BlockIOTag(0x19bu)	/* GTPro */
#define VERTEX_2_Z		BlockIOTag(0x19cu)	/* GTPro */
#define VERTEX_2_ARGB		BlockIOTag(0x19du)	/* GTPro */
#define VERTEX_2_X_Y		BlockIOTag(0x19eu)	/* GTPro */
/*	ONE_OVER_AREA		BlockIOTag(0x19fu) */	/* Duplicate */
#define VERTEX_3_S		BlockIOTag(0x1a0u)	/* GTPro */
#define VERTEX_3_T		BlockIOTag(0x1a1u)	/* GTPro */
#define VERTEX_3_W		BlockIOTag(0x1a2u)	/* GTPro */
#define VERTEX_3_SPEC_ARGB	BlockIOTag(0x1a3u)	/* GTPro */
#define VERTEX_3_Z		BlockIOTag(0x1a4u)	/* GTPro */
#define VERTEX_3_ARGB		BlockIOTag(0x1a5u)	/* GTPro */
#define VERTEX_3_X_Y		BlockIOTag(0x1a6u)	/* GTPro */
/*	ONE_OVER_AREA		BlockIOTag(0x1a7u) */	/* Duplicate */
#define VERTEX_3_SECONDARY_S	BlockIOTag(0x1a8u)	/* GTPro */
#define VERTEX_3_SECONDARY_T	BlockIOTag(0x1a9u)	/* GTPro */
#define VERTEX_3_SECONDARY_W	BlockIOTag(0x1aau)	/* GTPro */
/*	VERTEX_1_S		BlockIOTag(0x1abu) */	/* Duplicate */
/*	VERTEX_1_T		BlockIOTag(0x1acu) */	/* Duplicate */
/*	VERTEX_1_W		BlockIOTag(0x1adu) */	/* Duplicate */
/*	VERTEX_2_S		BlockIOTag(0x1aeu) */	/* Duplicate */
/*	VERTEX_2_T		BlockIOTag(0x1afu) */	/* Duplicate */
/*	VERTEX_2_W		BlockIOTag(0x1b0u) */	/* Duplicate */
/*	VERTEX_3_S		BlockIOTag(0x1b1u) */	/* Duplicate */
/*	VERTEX_3_T		BlockIOTag(0x1b2u) */	/* Duplicate */
/*	VERTEX_3_W		BlockIOTag(0x1b3u) */	/* Duplicate */
/*	VERTEX_1_SPEC_ARGB	BlockIOTag(0x1b4u) */	/* Duplicate */
/*	VERTEX_2_SPEC_ARGB	BlockIOTag(0x1b5u) */	/* Duplicate */
/*	VERTEX_3_SPEC_ARGB	BlockIOTag(0x1b6u) */	/* Duplicate */
/*	VERTEX_1_Z		BlockIOTag(0x1b7u) */	/* Duplicate */
/*	VERTEX_2_Z		BlockIOTag(0x1b8u) */	/* Duplicate */
/*	VERTEX_3_Z		BlockIOTag(0x1b9u) */	/* Duplicate */
/*	VERTEX_1_ARGB		BlockIOTag(0x1bau) */	/* Duplicate */
/*	VERTEX_2_ARGB		BlockIOTag(0x1bbu) */	/* Duplicate */
/*	VERTEX_3_ARGB		BlockIOTag(0x1bcu) */	/* Duplicate */
/*	VERTEX_1_X_Y		BlockIOTag(0x1bdu) */	/* Duplicate */
/*	VERTEX_2_X_Y		BlockIOTag(0x1beu) */	/* Duplicate */
/*	VERTEX_3_X_Y		BlockIOTag(0x1bfu) */	/* Duplicate */
#define ONE_OVER_AREA_UC	BlockIOTag(0x1c0u)	/* GTPro */
#define SETUP_CNTL		BlockIOTag(0x1c1u)	/* GTPro */
/*	?			BlockIOTag(0x1c2u) */
/*	?			BlockIOTag(0x1c3u) */
/*	?			BlockIOTag(0x1c4u) */
/*	?			BlockIOTag(0x1c5u) */
/*	?			BlockIOTag(0x1c6u) */
/*	?			BlockIOTag(0x1c7u) */
/*	?			BlockIOTag(0x1c8u) */
/*	?			BlockIOTag(0x1c9u) */
#define VERTEX_1_SECONDARY_S	BlockIOTag(0x1cau)	/* GTPro */
#define VERTEX_1_SECONDARY_T	BlockIOTag(0x1cbu)	/* GTPro */
#define VERTEX_1_SECONDARY_W	BlockIOTag(0x1ccu)	/* GTPro */
#define VERTEX_2_SECONDARY_S	BlockIOTag(0x1cdu)	/* GTPro */
#define VERTEX_2_SECONDARY_T	BlockIOTag(0x1ceu)	/* GTPro */
#define VERTEX_2_SECONDARY_W	BlockIOTag(0x1cfu)	/* GTPro */
/*	?			BlockIOTag(0x1d0u) */
/*	?			BlockIOTag(0x1d1u) */
/*	?			BlockIOTag(0x1d2u) */
/*	?			BlockIOTag(0x1d3u) */
/*	?			BlockIOTag(0x1d4u) */
/*	?			BlockIOTag(0x1d5u) */
/*	?			BlockIOTag(0x1d6u) */
/*	?			BlockIOTag(0x1d7u) */
/*	?			BlockIOTag(0x1d8u) */
/*	?			BlockIOTag(0x1d9u) */
/*	?			BlockIOTag(0x1dau) */
/*	?			BlockIOTag(0x1dbu) */
/*	?			BlockIOTag(0x1dcu) */
/*	?			BlockIOTag(0x1ddu) */
/*	?			BlockIOTag(0x1deu) */
/*	?			BlockIOTag(0x1dfu) */
/*	?			BlockIOTag(0x1e0u) */
/*	?			BlockIOTag(0x1e1u) */
/*	?			BlockIOTag(0x1e2u) */
/*	?			BlockIOTag(0x1e3u) */
/*	?			BlockIOTag(0x1e4u) */
/*	?			BlockIOTag(0x1e5u) */
/*	?			BlockIOTag(0x1e6u) */
/*	?			BlockIOTag(0x1e7u) */
/*	?			BlockIOTag(0x1e8u) */
/*	?			BlockIOTag(0x1e9u) */
/*	?			BlockIOTag(0x1eau) */
/*	?			BlockIOTag(0x1ebu) */
/*	?			BlockIOTag(0x1ecu) */
/*	?			BlockIOTag(0x1edu) */
/*	?			BlockIOTag(0x1eeu) */
/*	?			BlockIOTag(0x1efu) */
/*	?			BlockIOTag(0x1f0u) */
/*	?			BlockIOTag(0x1f1u) */
/*	?			BlockIOTag(0x1f2u) */
/*	?			BlockIOTag(0x1f3u) */
/*	?			BlockIOTag(0x1f4u) */
/*	?			BlockIOTag(0x1f5u) */
/*	?			BlockIOTag(0x1f6u) */
/*	?			BlockIOTag(0x1f7u) */
/*	?			BlockIOTag(0x1f8u) */
/*	?			BlockIOTag(0x1f9u) */
/*	?			BlockIOTag(0x1fau) */
/*	?			BlockIOTag(0x1fbu) */
/*	?			BlockIOTag(0x1fcu) */
/*	?			BlockIOTag(0x1fdu) */
/*	?			BlockIOTag(0x1feu) */
/*	?			BlockIOTag(0x1ffu) */

/* Definitions for MEM_CNTL's CTL_MEM_?????_APER_ENDIAN fields */
#define CTL_MEM_APER_BYTE_ENDIAN	0x00u
#define CTL_MEM_APER_WORD_ENDIAN	0x01u
#define CTL_MEM_APER_LONG_ENDIAN	0x02u
/*	?				0x03u */

/* Definitions for an ICS2595's programme word */
#define ICS2595_CLOCK		0x000001f0ul
#define ICS2595_FB_DIV		0x0001fe00ul		/* Feedback divider */
#define ICS2595_POST_DIV	0x000c0000ul		/* Post-divider */
#define ICS2595_STOP		0x00300000ul		/* Stop bits */
#define ICS2595_TOGGLE		(ICS2595_POST_DIV | ICS2595_STOP)

/* Definitions for internal PLL registers on a 264xT */
#define PLL_MPLL_CNTL		0x00u
#define MPLL_PC_GAIN			0x07u
#define MPLL_VC_GAIN			0x18u
#define MPLL_D_CYC			0x60u
#define MPLL_RANGE			0x80u
#define VPLL_CNTL		0x01u
#define VPLL_PC_GAIN			0x07u
#define VPLL_VC_GAIN			0x18u
#define VPLL_D_CYC			0x60u
#define VPLL_RANGE			0x80u
#define PLL_REF_DIV		0x02u
#define PLL_GEN_CNTL		0x03u
#define PLL_OVERRIDE			0x01u
#define PLL_SLEEP			0x01u	/* GTPro */
#define PLL_MCLK_RESET			0x02u
#define PLL_OSC_EN			0x04u
#define PLL_EXT_CLK_EN			0x08u
#define PLL_MCLK_SRC_SEL		0x70u
#define PLL_EXT_CLK_CNTL		0x80u	/* CT/ET */
#define PLL_DLL_PWDN			0x80u	/* VTB/GTB/LT */
#define PLL_MCLK_FB_DIV		0x04u
#define PLL_VCLK_CNTL		0x05u
#define PLL_VCLK_SRC_SEL		0x03u
#define PLL_VCLK_RESET			0x04u
#define PLL_VCLK_INVERT			0x08u
#define PLL_ECP_DIV			0x30u	/* VT/GT */
#define PLL_ERATE_GT_XRATE		0x40u	/* VT/GT */
#define PLL_SCALER_LOCK_EN		0x80u	/* VT/GT */
#define PLL_VCLK_POST_DIV	0x06u
#define PLL_VCLK0_POST_DIV		0x03u
#define PLL_VCLK1_POST_DIV		0x0cu
#define PLL_VCLK2_POST_DIV		0x30u
#define PLL_VCLK3_POST_DIV		0xc0u
#define PLL_VCLK0_FB_DIV	0x07u
#define PLL_VCLK1_FB_DIV	0x08u
#define PLL_VCLK2_FB_DIV	0x09u
#define PLL_VCLK3_FB_DIV	0x0au
#define PLL_XCLK_CNTL		0x0bu		/* VT/GT */
#define PLL_XCLK_MCLK_RATIO		0x03u
#define PLL_XCLK_SRC_SEL		0x07u	/* VTB/GTB/LT */
#define PLL_MFB_TIMES_4_2B		0x08u
#define PLL_VCLK0_XDIV			0x10u
#define PLL_VCLK1_XDIV			0x20u
#define PLL_VCLK2_XDIV			0x40u
#define PLL_VCLK3_XDIV			0x80u
#define PLL_FCP_CNTL		0x0cu		/* VT/GT */
#define PLL_FCP_POST_DIV		0x0fu
#define PLL_FCP_SRC_SEL			0x70u
#define PLL_DCLK_BY2_EN			0x80u
#define PLL_DLL_CNTL		0x0cu		/* VTB/GTB/LT */
#define PLL_DLL_REF_SRC			0x03u
#define PLL_DLL_FB_SRC			0x0cu
#define PLL_DLL_GAIN			0x30u
#define PLL_DLL_RESET			0x40u
#define PLL_DLL_HCLK_OUT_EN		0x80u
#define PLL_VFC_CNTL		0x0du		/* VT/GT */
#define PLL_DCLK_INVB			0x01u
#define PLL_DCLKBY2_EN			0x02u
#define PLL_VFC_2PHASE			0x04u
#define PLL_VFC_DELAY			0x18u
#define PLL_VFC_DCLKBY2_SHIFT		0x20u
/*	?				0x40u */
#define PLL_TST_SRC_SEL_BIT5		0x80u	/* VTB/GTB/LT */
#define PLL_TEST_CNTL		0x0eu
#define PLL_TST_SRC_SEL			0x1fu
#define PLL_TST_DIVIDERS		0x20u
#define PLL_TST_MASK_READ		0x40u
#define PLL_TST_ANALOG_MON_EN		0x80u
#define PLL_TEST_COUNT		0x0fu
#define PLL_LVDSPLL_CNTL0	0x10u		/* LT */
#define PLL_FPDI_NS_TIMING		0x01u
#define PLL_CURR_LEVEL			0x0eu
#define PLL_LVDS_TEST_MODE		0xf0u
#define PLL_LVDSPLL_CNTL1	0x11u		/* LT */
#define PLL_LPPL_RANGE			0x01u
#define PLL_LPLL_DUTY			0x06u
#define PLL_LPLL_VC_GAIN		0x18u
#define PLL_LPLL_CP_GAIN		0xe0u
#define PLL_AGP1_CNTL		0x12u		/* GTPro */
#define PLL_AGP2_CNTL		0x13u		/* GTPro */
#define PLL_DLL2_CNTL		0x14u		/* GTPro */
#define PLL_SCLK_FB_DIV		0x15u		/* GTPro */
#define PLL_SPLL_CNTL1		0x16u		/* GTPro */
#define PLL_SPLL_CNTL2		0x17u		/* GTPro */
#define PLL_APLL_STRAPS		0x18u		/* GTPro */
#define PLL_EXT_VPLL_CNTL	0x19u		/* GTPro */
#define PLL_EXT_VPLL_REF_SRC		0x03u
#define PLL_EXT_VPLL_EN			0x04u
#define PLL_EXT_VPLL_VGA_EN		0x08u
#define PLL_EXT_VPLL_INSYNC		0x10u
/*	?				0x60u */
#define PLL_EXT_V2PLL_EN		0x80u
#define PLL_EXT_VPLL_REF_DIV	0x1au		/* GTPro */
#define PLL_EXT_VPLL_FB_DIV	0x1bu		/* GTPro */
#define PLL_EXT_VPLL_MSB	0x1cu		/* GTPro */
#define PLL_HTOTAL_CNTL		0x1du		/* GTPro */
#define PLL_BYTE_CLK_CNTL	0x1eu		/* GTPro */
#define PLL_TV_REF_DIV		0x1fu		/* LTPro */
#define PLL_TV_FB_DIV		0x20u		/* LTPro */
#define PLL_TV_CNTL		0x21u		/* LTPro */
#define PLL_TV_GEN_CNTL		0x22u		/* LTPro */
#define PLL_V2_CNTL		0x23u		/* LTPro */
#define PLL_V2_GEN_CNTL		0x24u		/* LTPro */
#define PLL_V2_REF_DIV		0x25u		/* LTPro */
#define PLL_V2_FB_DIV		0x26u		/* LTPro */
#define PLL_V2_MSB		0x27u		/* LTPro */
#define PLL_HTOTAL2_CNTL	0x28u		/* LTPro */
#define PLL_YCLK_CNTL		0x29u		/* XC/XL */
#define PM_DYN_CLK_CNTL		0x2au		/* XC/XL */
/*	?			0x2bu */
/*	?			0x2cu */
/*	?			0x2du */
/*	?			0x2eu */
/*	?			0x2fu */
/*	?			0x30u */
/*	?			0x31u */
/*	?			0x32u */
/*	?			0x33u */
/*	?			0x34u */
/*	?			0x35u */
/*	?			0x36u */
/*	?			0x37u */
/*	?			0x38u */
/*	?			0x39u */
/*	?			0x3au */
/*	?			0x3bu */
/*	?			0x3cu */
/*	?			0x3du */
/*	?			0x3eu */
/*	?			0x3fu */

/* Definitions for an LTPro's 32-bit LCD registers */
#define LCD_CONFIG_PANEL	0x00u	/* See LT's CONFIG_PANEL (0x1d) */
#define LCD_GEN_CNTL		0x01u	/* See LT's LCD_GEN_CTRL (0x35) */
#define LCD_DSTN_CONTROL	0x02u	/* See LT's DSTN_CONTROL (0x1f) */
#define LCD_HFB_PITCH_ADDR	0x03u	/* See LT's HFB_PITCH_ADDR (0x2a) */
#define LCD_HORZ_STRETCHING	0x04u	/* See LT's HORZ_STRETCHING (0x32) */
#define LCD_VERT_STRETCHING	0x05u	/* See LT's VERT_STRETCHING (0x33) */
#define LCD_EXT_VERT_STRETCH	0x06u
#define VERT_STRETCH_RATIO3		0x000003fful
#define FORCE_DAC_DATA			0x000000fful
#define FORCE_DAC_DATA_SEL		0x00000300ul
#define VERT_STRETCH_MODE		0x00000400ul
#define VERT_PANEL_SIZE			0x003ff800ul
#define AUTO_VERT_RATIO			0x00400000ul
#define USE_AUTO_FP_POS			0x00800000ul
#define USE_AUTO_LCD_VSYNC		0x01000000ul
/*	?				0xfe000000ul */
#define LCD_LT_GIO		0x07u	/* See LT's LT_GIO (0x2f) */
#define LCD_POWER_MANAGEMENT	0x08u	/* See LT's POWER_MANAGEMENT (0x36) */
#define LCD_ZVGPIO		0x09u
#define LCD_ICON_CLR0		0x0au			/* XC/XL */
#define LCD_ICON_CLR1		0x0bu			/* XC/XL */
#define LCD_ICON_OFFSET		0x0cu			/* XC/XL */
#define LCD_ICON_HORZ_VERT_POSN	0x0du			/* XC/XL */
#define LCD_ICON_HORZ_VERT_OFF	0x0eu			/* XC/XL */
#define LCD_ICON2_CLR0		0x0fu			/* XC/XL */
#define LCD_ICON2_CLR1		0x10u			/* XC/XL */
#define LCD_ICON2_OFFSET	0x11u			/* XC/XL */
#define LCD_ICON2_HORZ_VERT_POSN 0x12u			/* XC/XL */
#define LCD_ICON2_HORZ_VERT_OFF	0x13u			/* XC/XL */
#define LCD_MISC_CNTL		0x14u			/* XC/XL */
#define BL_MOD_LEVEL			0x000000fful
#define BIAS_MOD_LEVEL			0x0000ff00ul
#define BLMOD_EN			0x00010000ul
#define BIASMOD_EN			0x00020000ul
/*	?				0x00040000ul */
#define PWRSEQ_MODE			0x00080000ul
#define APC_EN				0x00100000ul
#define MONITOR_DET_EN			0x00200000ul
#define FORCE_DAC_DATA_SEL_X		0x00c00000ul
#define FORCE_DAC_DATA_X		0xff000000ul
#define LCD_TMDS_CNTL		0x15u			/* XC/XL */
#define LCD_TMDS_SYNC_CHAR_SETA 0x16u			/* XC/XL */
#define LCD_TMDS_SYNC_CHAR_SETB	0x17u			/* XC/XL */
#define LCD_TMDS_SRC		0x18u			/* XC/XL */
#define LCD_PLTSTBLK_CNTL	0x19u			/* XC/XL */
#define LCD_SYNC_GEN_CNTL	0x1au			/* XC/XL */
#define LCD_PATTERN_GEN_SEED	0x1bu			/* XC/XL */
#define LCD_APC_CNTL		0x1cu			/* XC/XL */
#define LCD_POWER_MANAGEMENT_2	0x1du			/* XC/XL */
#define LCD_XCLK_DISP_PM_EN		0x00000001ul
#define LCD_XCLK_DISP2_PM_EN		0x00000002ul	/* Mobility */
#define LCD_XCLK_VID_PM_EN		0x00000004ul
#define LCD_XCLK_SCL_PM_EN		0x00000008ul
#define LCD_XCLK_GUI_PM_EN		0x00000010ul
#define LCD_XCLK_SUB_PM_EN		0x00000020ul
/*	?				0x000000c0ul */
#define LCD_MCLK_PM_EN			0x00000100ul
#define LCD_SS_EN			0x00000200ul
#define LCD_BLON_DIGON_EN		0x00000400ul
/*	?				0x00000800ul */
#define LCD_PM_DYN_XCLK_SYNC		0x00003000ul
#define LCD_SEL_W4MS			0x00004000ul
/*	?				0x00008000ul */
#define LCD_PM_DYN_XCLK_EN		0x00010000ul
#define LCD_PM_XCLK_ALWAYS		0x00020000ul
#define LCD_PM_DYN_XCLK_STATUS		0x00040000ul
#define LCD_PCI_ACC_DIS			0x00080000ul
#define LCD_PM_DYN_XCLK_DISP		0x00100000ul
#define LCD_PM_DYN_XCLK_DISP2		0x00200000ul	/* Mobility */
#define LCD_PM_DYN_XCLK_VID		0x00400000ul
#define LCD_PM_DYN_XCLK_HFB		0x00800000ul
#define LCD_PM_DYN_XCLK_SCL		0x01000000ul
#define LCD_PM_DYN_XCLK_SUB		0x02000000ul
#define LCD_PM_DYN_XCLK_GUI		0x04000000ul
#define LCD_PM_DYN_XCLK_HOST		0x08000000ul
/*	?				0xf0000000ul */
#define LCD_PRI_ERR_PATTERN	0x1eu			/* XC/XL */
#define LCD_CUR_ERR_PATTERN	0x1fu			/* XC/XL */
#define LCD_PLTSTBLK_RPT	0x20u			/* XC/XL */
#define LCD_SYNC_RPT		0x21u			/* XC/XL */
#define LCD_CRC_PATTERN_RPT	0x22u			/* XC/XL */
#define LCD_PL_TRANSMITTER_CNTL	0x23u			/* XC/XL */
#define LCD_PL_PLL_CNTL		0x24u			/* XC/XL */
#define LCD_ALPHA_BLENDING	0x25u			/* XC/XL */
#define LCD_PORTRAIT_GEN_CNTL	0x26u			/* XC/XL */
#define LCD_APC_CTRL_IO		0x27u			/* XC/XL */
#define LCD_TEST_IO		0x28u			/* XC/XL */
/*	?			0x29u */
#define LCD_DP1_MEM_ACCESS	0x2au			/* XC/XL */
#define LCD_DP0_MEM_ACCESS	0x2bu			/* XC/XL */
#define LCD_DP0_DEBUG_A		0x2cu			/* XC/XL */
#define LCD_DP0_DEBUG_B		0x2du			/* XC/XL */
#define LCD_DP1_DEBUG_A		0x2eu			/* XC/XL */
#define LCD_DP1_DEBUG_B		0x2fu			/* XC/XL */
#define LCD_DPCTRL_DEBUG_A	0x30u			/* XC/XL */
#define LCD_DPCTRL_DEBUG_B	0x31u			/* XC/XL */
#define LCD_MEMBLK_DEBUG	0x32u			/* XC/XL */
#define LCD_APC_LUT_AB		0x33u			/* XC/XL */
#define LCD_APC_LUT_CD		0x34u			/* XC/XL */
#define LCD_APC_LUT_EF		0x35u			/* XC/XL */
#define LCD_APC_LUT_GH		0x36u			/* XC/XL */
#define LCD_APC_LUT_IJ		0x37u			/* XC/XL */
#define LCD_APC_LUT_KL		0x38u			/* XC/XL */
#define LCD_APC_LUT_MN		0x39u			/* XC/XL */
#define LCD_APC_LUT_OP		0x3au			/* XC/XL */
/*	?			0x3bu */
/*	?			0x3cu */
/*	?			0x3du */
/*	?			0x3eu */
/*	?			0x3fu */

/* Definitions for an LTPro's TV registers */
/*	?			0x00u */
/*	?			0x01u */
/*	?			0x02u */
/*	?			0x03u */
/*	?			0x04u */
/*	?			0x05u */
/*	?			0x06u */
/*	?			0x07u */
/*	?			0x08u */
/*	?			0x09u */
/*	?			0x0au */
/*	?			0x0bu */
/*	?			0x0cu */
/*	?			0x0du */
/*	?			0x0eu */
/*	?			0x0fu */
#define TV_MASTER_CNTL		0x10u
/*	?			0x11u */
#define TV_RGB_CNTL		0x12u
/*	?			0x13u */
#define TV_SYNC_CNTL		0x14u
/*	?			0x15u */
/*	?			0x16u */
/*	?			0x17u */
/*	?			0x18u */
/*	?			0x19u */
/*	?			0x1au */
/*	?			0x1bu */
/*	?			0x1cu */
/*	?			0x1du */
/*	?			0x1eu */
/*	?			0x1fu */
#define TV_HTOTAL		0x20u
#define TV_HDISP		0x21u
#define TV_HSIZE		0x22u
#define TV_HSTART		0x23u
#define TV_HCOUNT		0x24u
#define TV_VTOTAL		0x25u
#define TV_VDISP		0x26u
#define TV_VCOUNT		0x27u
#define TV_FTOTAL		0x28u
#define TV_FCOUNT		0x29u
#define TV_FRESTART		0x2au
#define TV_HRESTART		0x2bu
#define TV_VRESTART		0x2cu
/*	?			0x2du */
/*	?			0x2eu */
/*	?			0x2fu */
/*	?			0x30u */
/*	?			0x31u */
/*	?			0x32u */
/*	?			0x33u */
/*	?			0x34u */
/*	?			0x35u */
/*	?			0x36u */
/*	?			0x37u */
/*	?			0x38u */
/*	?			0x39u */
/*	?			0x3au */
/*	?			0x3bu */
/*	?			0x3cu */
/*	?			0x3du */
/*	?			0x3eu */
/*	?			0x3fu */
/*	?			0x40u */
/*	?			0x41u */
/*	?			0x42u */
/*	?			0x43u */
/*	?			0x44u */
/*	?			0x45u */
/*	?			0x46u */
/*	?			0x47u */
/*	?			0x48u */
/*	?			0x49u */
/*	?			0x4au */
/*	?			0x4bu */
/*	?			0x4cu */
/*	?			0x4du */
/*	?			0x4eu */
/*	?			0x4fu */
/*	?			0x50u */
/*	?			0x51u */
/*	?			0x52u */
/*	?			0x53u */
/*	?			0x54u */
/*	?			0x55u */
/*	?			0x56u */
/*	?			0x57u */
/*	?			0x58u */
/*	?			0x59u */
/*	?			0x5au */
/*	?			0x5bu */
/*	?			0x5cu */
/*	?			0x5du */
/*	?			0x5eu */
/*	?			0x5fu */
#define TV_HOST_READ_DATA	0x60u
#define TV_HOST_WRITE_DATA	0x61u
#define TV_HOST_RD_WT_CNTL	0x62u
/*	?			0x63u */
/*	?			0x64u */
/*	?			0x65u */
/*	?			0x66u */
/*	?			0x67u */
/*	?			0x68u */
/*	?			0x69u */
/*	?			0x6au */
/*	?			0x6bu */
/*	?			0x6cu */
/*	?			0x6du */
/*	?			0x6eu */
/*	?			0x6fu */
#define TV_VSCALER_CNTL		0x70u
#define TV_TIMING_CNTL		0x71u
#define TV_GAMMA_CNTL		0x72u
#define TV_Y_FALL_CNTL		0x73u
#define TV_Y_RISE_CNTL		0x74u
#define TV_Y_SAW_TOOTH_CNTL	0x75u
/*	?			0x76u */
/*	?			0x77u */
/*	?			0x78u */
/*	?			0x79u */
/*	?			0x7au */
/*	?			0x7bu */
/*	?			0x7cu */
/*	?			0x7du */
/*	?			0x7eu */
/*	?			0x7fu */
#define TV_MODULATOR_CNTL1	0x80u
#define TV_MODULATOR_CNTL2	0x81u
/*	?			0x82u */
/*	?			0x83u */
/*	?			0x84u */
/*	?			0x85u */
/*	?			0x86u */
/*	?			0x87u */
/*	?			0x88u */
/*	?			0x89u */
/*	?			0x8au */
/*	?			0x8bu */
/*	?			0x8cu */
/*	?			0x8du */
/*	?			0x8eu */
/*	?			0x8fu */
#define TV_PRE_DAC_MUX_CNTL	0x90u
/*	?			0x91u */
/*	?			0x92u */
/*	?			0x93u */
/*	?			0x94u */
/*	?			0x95u */
/*	?			0x96u */
/*	?			0x97u */
/*	?			0x98u */
/*	?			0x99u */
/*	?			0x9au */
/*	?			0x9bu */
/*	?			0x9cu */
/*	?			0x9du */
/*	?			0x9eu */
/*	?			0x9fu */
#define TV_DAC_CNTL		0xa0u
/*	?			0xa1u */
/*	?			0xa2u */
/*	?			0xa3u */
/*	?			0xa4u */
/*	?			0xa5u */
/*	?			0xa6u */
/*	?			0xa7u */
/*	?			0xa8u */
/*	?			0xa9u */
/*	?			0xaau */
/*	?			0xabu */
/*	?			0xacu */
/*	?			0xadu */
/*	?			0xaeu */
/*	?			0xafu */
#define TV_CRC_CNTL		0xb0u
#define TV_VIDEO_PORT_SIG	0xb1u
/*	?			0xb2u */
/*	?			0xb3u */
/*	?			0xb4u */
/*	?			0xb5u */
/*	?			0xb6u */
/*	?			0xb7u */
#define TV_VBI_CC_CNTL		0xb8u
#define TV_VBI_EDS_CNTL		0xb9u
#define TV_VBI_20BIT_CNTL	0xbau
/*	?			0xbbu */
/*	?			0xbcu */
#define TV_VBI_DTO_CNTL		0xbdu
#define TV_VBI_LEVEL_CNTL	0xbeu
/*	?			0xbfu */
#define TV_UV_ADR		0xc0u
#define TV_FIFO_TEST_CNTL	0xc1u
/*	?			0xc2u */
/*	?			0xc3u */
/*	?			0xc4u */
/*	?			0xc5u */
/*	?			0xc6u */
/*	?			0xc7u */
/*	?			0xc8u */
/*	?			0xc9u */
/*	?			0xcau */
/*	?			0xcbu */
/*	?			0xccu */
/*	?			0xcdu */
/*	?			0xceu */
/*	?			0xcfu */
/*	?			0xd0u */
/*	?			0xd1u */
/*	?			0xd2u */
/*	?			0xd3u */
/*	?			0xd4u */
/*	?			0xd5u */
/*	?			0xd6u */
/*	?			0xd7u */
/*	?			0xd8u */
/*	?			0xd9u */
/*	?			0xdau */
/*	?			0xdbu */
/*	?			0xdcu */
/*	?			0xddu */
/*	?			0xdeu */
/*	?			0xdfu */
/*	?			0xe0u */
/*	?			0xe1u */
/*	?			0xe2u */
/*	?			0xe3u */
/*	?			0xe4u */
/*	?			0xe5u */
/*	?			0xe6u */
/*	?			0xe7u */
/*	?			0xe8u */
/*	?			0xe9u */
/*	?			0xeau */
/*	?			0xebu */
/*	?			0xecu */
/*	?			0xedu */
/*	?			0xeeu */
/*	?			0xefu */
/*	?			0xf0u */
/*	?			0xf1u */
/*	?			0xf2u */
/*	?			0xf3u */
/*	?			0xf4u */
/*	?			0xf5u */
/*	?			0xf6u */
/*	?			0xf7u */
/*	?			0xf8u */
/*	?			0xf9u */
/*	?			0xfau */
/*	?			0xfbu */
/*	?			0xfcu */
/*	?			0xfdu */
/*	?			0xfeu */
/*	?			0xffu */

/* Miscellaneous */

/* Current X, Y & Dest X, Y mask */
#define COORD_MASK	0x07ffu

/* Pixel widths */
#define PIX_WIDTH_1BPP			0x00u
#define PIX_WIDTH_4BPP			0x01u	/* CRTC2: 8bpp */
#define PIX_WIDTH_8BPP			0x02u	/* CRTC2: Undefined */
#define PIX_WIDTH_15BPP			0x03u
#define PIX_WIDTH_16BPP			0x04u
#define PIX_WIDTH_24BPP			0x05u
#define PIX_WIDTH_32BPP			0x06u
#define PIX_WIDTH_YUV422		0x07u	/* CRTC2 only */

/* Source definitions */
#define SRC_BKGD			0x00u
#define SRC_FRGD			0x01u
#define SRC_HOST			0x02u
#define SRC_BLIT			0x03u
#define SRC_PATTERN			0x04u
#define SRC_SCALER_3D			0x05u
/*	?				0x06u */
/*	?				0x07u */

/* The Mixes */
#define MIX_MASK			0x001fu

#define MIX_NOT_DST			0x0000u
#define MIX_0				0x0001u
#define MIX_1				0x0002u
#define MIX_DST				0x0003u
#define MIX_NOT_SRC			0x0004u
#define MIX_XOR				0x0005u
#define MIX_XNOR			0x0006u
#define MIX_SRC				0x0007u
#define MIX_NAND			0x0008u
#define MIX_NOT_SRC_OR_DST		0x0009u
#define MIX_SRC_OR_NOT_DST		0x000au
#define MIX_OR				0x000bu
#define MIX_AND				0x000cu
#define MIX_SRC_AND_NOT_DST		0x000du
#define MIX_NOT_SRC_AND_DST		0x000eu
#define MIX_NOR				0x000fu

#define MIX_MIN				0x0010u
#define MIX_DST_MINUS_SRC		0x0011u
#define MIX_SRC_MINUS_DST		0x0012u
#define MIX_PLUS			0x0013u
#define MIX_MAX				0x0014u
#define MIX_HALF__DST_MINUS_SRC		0x0015u
#define MIX_HALF__SRC_MINUS_DST		0x0016u
#define MIX_AVERAGE			0x0017u
#define MIX_DST_MINUS_SRC_SAT		0x0018u
#define MIX_SRC_MINUS_DST_SAT		0x001au
#define MIX_HALF__DST_MINUS_SRC_SAT	0x001cu
#define MIX_HALF__SRC_MINUS_DST_SAT	0x001eu
#define MIX_AVERAGE_SAT			0x001fu
#define MIX_FN_PAINT			MIX_SRC


#endif
