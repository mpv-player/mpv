/*
 * BES YUV video overlay driver for Radeon/Rage128Pro/Rage128 cards
 *
 * Copyright (C) 2001 Nick Kurshev
 *
 * This file is partly based on mga_vid and sis_vid from MPlayer.
 * Code from CVS of GATOS project and X11 trees was also used.
 *
 * SPECIAL THANKS TO: Hans-Peter Raschke for active testing and hacking
 * Rage128(pro) stuff of this driver.
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

#define RADEON_VID_VERSION "1.2.1"

/*
  It's entirely possible this major conflicts with something else
  mknod /dev/radeon_vid c 178 0
  or
  mknod /dev/rage128_vid c 178 0
  for Rage128/Rage128Pro chips (although it doesn't matter)
  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  TESTED and WORKING formats: YUY2, UYVY, IYUV, I420, YV12
  -----------------------------------------------------------
  TODO:
  Highest priority: fbvid.h compatibility
  High priority: Fixing BUGS
  Middle priority: RGB/BGR 2-32, YVU9, IF09 support
  Low priority: CLPL, IYU1, IYU2, UYNV, CYUV, YUNV, YVYU, Y41P, Y211, Y41T,
		      ^^^^
		Y42T, V422, V655, CLJR, YUVP, UYVP, Mpeg PES (mpeg-1,2) support
  ...........................................................
  BUGS and LACKS:
    Color and video keys don't work
*/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/byteorder/swab.h>

#include "radeon_vid.h"
#include "radeon.h"

#ifdef CONFIG_MTRR 
#include <asm/mtrr.h>
#endif

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>

#define TRUE 1
#define FALSE 0

#define RADEON_VID_MAJOR 178


MODULE_AUTHOR("Nick Kurshev <nickols_k@mail.ru>");
#ifdef RAGE128
MODULE_DESCRIPTION("Accelerated YUV BES driver for Rage128. Version: "RADEON_VID_VERSION);
#else
MODULE_DESCRIPTION("Accelerated YUV BES driver for Radeons. Version: "RADEON_VID_VERSION);
#endif
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#ifdef CONFIG_MTRR 
MODULE_PARM(mtrr, "i");
MODULE_PARM_DESC(mtrr, "Tune MTRR (touch=1(default))");
static int mtrr __initdata = 1;
static struct { int vram; int vram_valid; } smtrr;
#endif
MODULE_PARM(swap_fourcc, "i");
MODULE_PARM_DESC(swap_fourcc, "Swap fourcc (don't swap=0(default))");
static int swap_fourcc __initdata = 0;

#ifdef RAGE128
#define RVID_MSG "rage128_vid: "
#define X_ADJUST 0
#else
#define RVID_MSG "radeon_vid: "
#define X_ADJUST 8
#ifndef RADEON
#define RADEON
#endif
#endif

#undef DEBUG
#if DEBUG
#define RTRACE		printk
#else
#define RTRACE(...)	((void)0)
#endif

#ifndef min
#define min(a,b) (a < b ? a : b)
#endif

#ifndef RAGE128
#if defined(__i386__)
/* Ugly but only way */
#undef AVOID_FPU
static inline double FastSin(double x)
{
   register double res;
   __asm__ volatile("fsin":"=t"(res):"0"(x));
   return res;
}
#undef sin
#define sin(x) FastSin(x)

static inline double FastCos(double x)
{
   register double res;
   __asm__ volatile("fcos":"=t"(res):"0"(x));
   return res;
}
#undef cos
#define cos(x) FastCos(x)
#else
#include "generic_math.h"
#endif /*__386__*/
#endif /*RAGE128*/

#if !defined( RAGE128 ) && !defined( AVOID_FPU )
#define RADEON_FPU 1
#endif

typedef struct bes_registers_s
{
  /* base address of yuv framebuffer */
  uint32_t yuv_base;
  uint32_t fourcc;
  uint32_t dest_bpp;
  /* YUV BES registers */
  uint32_t reg_load_cntl;
  uint32_t h_inc;
  uint32_t step_by;
  uint32_t y_x_start;
  uint32_t y_x_end;
  uint32_t v_inc;
  uint32_t p1_blank_lines_at_top;
  uint32_t p23_blank_lines_at_top;
  uint32_t vid_buf_pitch0_value;
  uint32_t vid_buf_pitch1_value;
  uint32_t p1_x_start_end;
  uint32_t p2_x_start_end;
  uint32_t p3_x_start_end;
  uint32_t base_addr;
  uint32_t vid_buf0_base_adrs;
  /* These ones are for auto flip: maybe in the future */
  uint32_t vid_buf1_base_adrs;
  uint32_t vid_buf2_base_adrs;
  uint32_t vid_buf3_base_adrs;
  uint32_t vid_buf4_base_adrs;
  uint32_t vid_buf5_base_adrs;

  uint32_t p1_v_accum_init;
  uint32_t p1_h_accum_init;
  uint32_t p23_v_accum_init;
  uint32_t p23_h_accum_init;
  uint32_t scale_cntl;
  uint32_t exclusive_horz;
  uint32_t auto_flip_cntl;
  uint32_t filter_cntl;
  uint32_t key_cntl;
  uint32_t test;
  /* Configurable stuff */
  int double_buff;
  
  int brightness;
  int saturation;
  
  int ckey_on;
  uint32_t graphics_key_clr;
  uint32_t graphics_key_msk;
  
  int deinterlace_on;
  uint32_t deinterlace_pattern;
  
} bes_registers_t;

typedef struct video_registers_s
{
#ifdef DEBUG
  const char * sname;
#endif
  uint32_t name;
  uint32_t value;
}video_registers_t;

static bes_registers_t besr;
#ifndef RAGE128
static int IsR200=0;
#endif
#ifdef DEBUG
#define DECLARE_VREG(name) { #name, name, 0 }
#else
#define DECLARE_VREG(name) { name, 0 }
#endif
#ifdef DEBUG
static video_registers_t vregs[] = 
{
  DECLARE_VREG(VIDEOMUX_CNTL),
  DECLARE_VREG(VIPPAD_MASK),
  DECLARE_VREG(VIPPAD1_A),
  DECLARE_VREG(VIPPAD1_EN),
  DECLARE_VREG(VIPPAD1_Y),
  DECLARE_VREG(OV0_Y_X_START),
  DECLARE_VREG(OV0_Y_X_END),
  DECLARE_VREG(OV0_PIPELINE_CNTL),
  DECLARE_VREG(OV0_EXCLUSIVE_HORZ),
  DECLARE_VREG(OV0_EXCLUSIVE_VERT),
  DECLARE_VREG(OV0_REG_LOAD_CNTL),
  DECLARE_VREG(OV0_SCALE_CNTL),
  DECLARE_VREG(OV0_V_INC),
  DECLARE_VREG(OV0_P1_V_ACCUM_INIT),
  DECLARE_VREG(OV0_P23_V_ACCUM_INIT),
  DECLARE_VREG(OV0_P1_BLANK_LINES_AT_TOP),
  DECLARE_VREG(OV0_P23_BLANK_LINES_AT_TOP),
#ifdef RADEON
  DECLARE_VREG(OV0_BASE_ADDR),
#endif
  DECLARE_VREG(OV0_VID_BUF0_BASE_ADRS),
  DECLARE_VREG(OV0_VID_BUF1_BASE_ADRS),
  DECLARE_VREG(OV0_VID_BUF2_BASE_ADRS),
  DECLARE_VREG(OV0_VID_BUF3_BASE_ADRS),
  DECLARE_VREG(OV0_VID_BUF4_BASE_ADRS),
  DECLARE_VREG(OV0_VID_BUF5_BASE_ADRS),
  DECLARE_VREG(OV0_VID_BUF_PITCH0_VALUE),
  DECLARE_VREG(OV0_VID_BUF_PITCH1_VALUE),
  DECLARE_VREG(OV0_AUTO_FLIP_CNTL),
  DECLARE_VREG(OV0_DEINTERLACE_PATTERN),
  DECLARE_VREG(OV0_SUBMIT_HISTORY),
  DECLARE_VREG(OV0_H_INC),
  DECLARE_VREG(OV0_STEP_BY),
  DECLARE_VREG(OV0_P1_H_ACCUM_INIT),
  DECLARE_VREG(OV0_P23_H_ACCUM_INIT),
  DECLARE_VREG(OV0_P1_X_START_END),
  DECLARE_VREG(OV0_P2_X_START_END),
  DECLARE_VREG(OV0_P3_X_START_END),
  DECLARE_VREG(OV0_FILTER_CNTL),
  DECLARE_VREG(OV0_FOUR_TAP_COEF_0),
  DECLARE_VREG(OV0_FOUR_TAP_COEF_1),
  DECLARE_VREG(OV0_FOUR_TAP_COEF_2),
  DECLARE_VREG(OV0_FOUR_TAP_COEF_3),
  DECLARE_VREG(OV0_FOUR_TAP_COEF_4),
  DECLARE_VREG(OV0_FLAG_CNTL),
#ifdef RAGE128
  DECLARE_VREG(OV0_COLOUR_CNTL),
#else
  DECLARE_VREG(OV0_SLICE_CNTL),
#endif
  DECLARE_VREG(OV0_VID_KEY_CLR),
  DECLARE_VREG(OV0_VID_KEY_MSK),
  DECLARE_VREG(OV0_GRAPHICS_KEY_CLR),
  DECLARE_VREG(OV0_GRAPHICS_KEY_MSK),
  DECLARE_VREG(OV0_KEY_CNTL),
  DECLARE_VREG(OV0_TEST),
  DECLARE_VREG(OV0_LIN_TRANS_A),
  DECLARE_VREG(OV0_LIN_TRANS_B),
  DECLARE_VREG(OV0_LIN_TRANS_C),
  DECLARE_VREG(OV0_LIN_TRANS_D),
  DECLARE_VREG(OV0_LIN_TRANS_E),
  DECLARE_VREG(OV0_LIN_TRANS_F),
  DECLARE_VREG(OV0_GAMMA_0_F),
  DECLARE_VREG(OV0_GAMMA_10_1F),
  DECLARE_VREG(OV0_GAMMA_20_3F),
  DECLARE_VREG(OV0_GAMMA_40_7F),
  DECLARE_VREG(OV0_GAMMA_380_3BF),
  DECLARE_VREG(OV0_GAMMA_3C0_3FF),
  DECLARE_VREG(SUBPIC_CNTL),
  DECLARE_VREG(SUBPIC_DEFCOLCON),
  DECLARE_VREG(SUBPIC_Y_X_START),
  DECLARE_VREG(SUBPIC_Y_X_END),
  DECLARE_VREG(SUBPIC_V_INC),
  DECLARE_VREG(SUBPIC_H_INC),
  DECLARE_VREG(SUBPIC_BUF0_OFFSET),
  DECLARE_VREG(SUBPIC_BUF1_OFFSET),
  DECLARE_VREG(SUBPIC_LC0_OFFSET),
  DECLARE_VREG(SUBPIC_LC1_OFFSET),
  DECLARE_VREG(SUBPIC_PITCH),
  DECLARE_VREG(SUBPIC_BTN_HLI_COLCON),
  DECLARE_VREG(SUBPIC_BTN_HLI_Y_X_START),
  DECLARE_VREG(SUBPIC_BTN_HLI_Y_X_END),
  DECLARE_VREG(SUBPIC_PALETTE_INDEX),
  DECLARE_VREG(SUBPIC_PALETTE_DATA),
  DECLARE_VREG(SUBPIC_H_ACCUM_INIT),
  DECLARE_VREG(SUBPIC_V_ACCUM_INIT),
  DECLARE_VREG(IDCT_RUNS),
  DECLARE_VREG(IDCT_LEVELS),
  DECLARE_VREG(IDCT_AUTH_CONTROL),
  DECLARE_VREG(IDCT_AUTH),
  DECLARE_VREG(IDCT_CONTROL)
};
#endif
static uint32_t radeon_vid_in_use = 0;

static uint8_t *radeon_mmio_base = 0;
static uint32_t radeon_mem_base = 0; 
static int32_t radeon_overlay_off = 0;
static uint32_t radeon_ram_size = 0;
#define PARAM_BUFF_SIZE 4096
static uint8_t *radeon_param_buff = NULL;
static uint32_t radeon_param_buff_size=0;
static uint32_t radeon_param_buff_len=0; /* real length of buffer */
static mga_vid_config_t radeon_config; 

static char *fourcc_format_name(int format)
{
    switch(format)
    {
	case IMGFMT_RGB8: return "RGB 8-bit";
	case IMGFMT_RGB15: return "RGB 15-bit";
	case IMGFMT_RGB16: return "RGB 16-bit";
	case IMGFMT_RGB24: return "RGB 24-bit";
	case IMGFMT_RGB32: return "RGB 32-bit";
	case IMGFMT_BGR8: return "BGR 8-bit";
	case IMGFMT_BGR15: return "BGR 15-bit";
	case IMGFMT_BGR16: return "BGR 16-bit";
	case IMGFMT_BGR24: return "BGR 24-bit";
	case IMGFMT_BGR32: return "BGR 32-bit";
	case IMGFMT_YVU9: return "Planar YVU9";
	case IMGFMT_IF09: return "Planar IF09";
	case IMGFMT_YV12: return "Planar YV12";
	case IMGFMT_I420: return "Planar I420";
	case IMGFMT_IYUV: return "Planar IYUV";
	case IMGFMT_CLPL: return "Planar CLPL";
	case IMGFMT_Y800: return "Planar Y800";
	case IMGFMT_Y8: return "Planar Y8";
	case IMGFMT_IUYV: return "Packed IUYV";
	case IMGFMT_IY41: return "Packed IY41";
	case IMGFMT_IYU1: return "Packed IYU1";
	case IMGFMT_IYU2: return "Packed IYU2";
	case IMGFMT_UYNV: return "Packed UYNV";
	case IMGFMT_cyuv: return "Packed CYUV";
	case IMGFMT_Y422: return "Packed Y422";
	case IMGFMT_YUY2: return "Packed YUY2";
	case IMGFMT_YUNV: return "Packed YUNV";
	case IMGFMT_UYVY: return "Packed UYVY";
//	case IMGFMT_YVYU: return "Packed YVYU";
	case IMGFMT_Y41P: return "Packed Y41P";
	case IMGFMT_Y211: return "Packed Y211";
	case IMGFMT_Y41T: return "Packed Y41T";
	case IMGFMT_Y42T: return "Packed Y42T";
	case IMGFMT_V422: return "Packed V422";
	case IMGFMT_V655: return "Packed V655";
	case IMGFMT_CLJR: return "Packed CLJR";
	case IMGFMT_YUVP: return "Packed YUVP";
	case IMGFMT_UYVP: return "Packed UYVP";
	case IMGFMT_MPEGPES: return "Mpeg PES";
    }
    return "Unknown";
}


/*
 * IO macros
 */

#define INREG8(addr)		readb((radeon_mmio_base)+addr)
#define OUTREG8(addr,val)	writeb(val, (radeon_mmio_base)+addr)
#define INREG(addr)		readl((radeon_mmio_base)+addr)
#define OUTREG(addr,val)	writel(val, (radeon_mmio_base)+addr)
#define OUTREGP(addr,val,mask)  					\
	do {								\
		unsigned int tmp = INREG(addr);				\
		tmp &= (mask);						\
		tmp |= (val);						\
		OUTREG(addr, tmp);					\
	} while (0)

static uint32_t radeon_vid_get_dbpp( void )
{
  uint32_t dbpp,retval;
  dbpp = (INREG(CRTC_GEN_CNTL)>>8)& 0xF;
  switch(dbpp)
  {
    case DST_8BPP: retval = 8; break;
    case DST_15BPP: retval = 15; break;
    case DST_16BPP: retval = 16; break;
    case DST_24BPP: retval = 24; break;
    default: retval=32; break;
  }
  return retval;
}

static int radeon_is_dbl_scan( void )
{
  return (INREG(CRTC_GEN_CNTL))&CRTC_DBL_SCAN_EN;
}

static int radeon_is_interlace( void )
{
  return (INREG(CRTC_GEN_CNTL))&CRTC_INTERLACE_EN;
}

static __inline__ void radeon_engine_flush ( void )
{
	int i;

	/* initiate flush */
	OUTREGP(RB2D_DSTCACHE_CTLSTAT, RB2D_DC_FLUSH_ALL,
	        ~RB2D_DC_FLUSH_ALL);

	for (i=0; i < 2000000; i++) {
		if (!(INREG(RB2D_DSTCACHE_CTLSTAT) & RB2D_DC_BUSY))
			break;
	}
}


static __inline__ void radeon_fifo_wait (int entries)
{
	int i;

	for (i=0; i<2000000; i++)
		if ((INREG(RBBM_STATUS) & 0x7f) >= entries)
			return;
}


static __inline__ void radeon_engine_idle ( void )
{
	int i;

	/* ensure FIFO is empty before waiting for idle */
	radeon_fifo_wait (64);

	for (i=0; i<2000000; i++) {
		if (((INREG(RBBM_STATUS) & GUI_ACTIVE)) == 0) {
			radeon_engine_flush ();
			return;
		}
	}
}

#if 0
static void __init radeon_vid_save_state( void )
{
  size_t i;
  for(i=0;i<sizeof(vregs)/sizeof(video_registers_t);i++)
	vregs[i].value = INREG(vregs[i].name);
}

static void __exit radeon_vid_restore_state( void )
{
  size_t i;
  radeon_fifo_wait(2);
  OUTREG(OV0_REG_LOAD_CNTL,		REG_LD_CTL_LOCK);
  radeon_engine_idle();
  while(!(INREG(OV0_REG_LOAD_CNTL)&REG_LD_CTL_LOCK_READBACK));
  radeon_fifo_wait(15);
  for(i=0;i<sizeof(vregs)/sizeof(video_registers_t);i++)
  {
	radeon_fifo_wait(1);
	OUTREG(vregs[i].name,vregs[i].value);
  }
  OUTREG(OV0_REG_LOAD_CNTL,		0);
}
#endif
#ifdef DEBUG
static void radeon_vid_dump_regs( void )
{
  size_t i;
  printk(RVID_MSG"*** Begin of OV0 registers dump ***\n");
  for(i=0;i<sizeof(vregs)/sizeof(video_registers_t);i++)
	printk(RVID_MSG"%s = %08X\n",vregs[i].sname,INREG(vregs[i].name));
  printk(RVID_MSG"*** End of OV0 registers dump ***\n");
}
#endif

#ifdef RADEON_FPU
/* Reference color space transform data */
typedef struct tagREF_TRANSFORM
{
	float RefLuma;
	float RefRCb;
	float RefRCr;
	float RefGCb;
	float RefGCr;
	float RefBCb;
	float RefBCr;
} REF_TRANSFORM;

/* Parameters for ITU-R BT.601 and ITU-R BT.709 colour spaces */
REF_TRANSFORM trans[2] =
{
	{1.1678, 0.0, 1.6007, -0.3929, -0.8154, 2.0232, 0.0}, /* BT.601 */
	{1.1678, 0.0, 1.7980, -0.2139, -0.5345, 2.1186, 0.0}  /* BT.709 */
};
/****************************************************************************
 * SetTransform                                                             *
 *  Function: Calculates and sets color space transform from supplied       *
 *            reference transform, gamma, brightness, contrast, hue and     *
 *            saturation.                                                   *
 *    Inputs: bright - brightness                                           *
 *            cont - contrast                                               *
 *            sat - saturation                                              *
 *            hue - hue                                                     *
 *            ref - index to the table of refernce transforms               *
 *   Outputs: NONE                                                          *
 ****************************************************************************/

static void radeon_set_transform(float bright, float cont, float sat,
				 float hue, unsigned ref)
{
	float OvHueSin, OvHueCos;
	float CAdjLuma, CAdjOff;
	float CAdjRCb, CAdjRCr;
	float CAdjGCb, CAdjGCr;
	float CAdjBCb, CAdjBCr;
	float OvLuma, OvROff, OvGOff, OvBOff;
	float OvRCb, OvRCr;
	float OvGCb, OvGCr;
	float OvBCb, OvBCr;
	float Loff = 64.0;
	float Coff = 512.0f;

	u32 dwOvLuma, dwOvROff, dwOvGOff, dwOvBOff;
	u32 dwOvRCb, dwOvRCr;
	u32 dwOvGCb, dwOvGCr;
	u32 dwOvBCb, dwOvBCr;

	if (ref >= 2) return;

	OvHueSin = sin((double)hue);
	OvHueCos = cos((double)hue);

	CAdjLuma = cont * trans[ref].RefLuma;
	CAdjOff = cont * trans[ref].RefLuma * bright * 1023.0;

	CAdjRCb = sat * -OvHueSin * trans[ref].RefRCr;
	CAdjRCr = sat * OvHueCos * trans[ref].RefRCr;
	CAdjGCb = sat * (OvHueCos * trans[ref].RefGCb - OvHueSin * trans[ref].RefGCr);
	CAdjGCr = sat * (OvHueSin * trans[ref].RefGCb + OvHueCos * trans[ref].RefGCr);
	CAdjBCb = sat * OvHueCos * trans[ref].RefBCb;
	CAdjBCr = sat * OvHueSin * trans[ref].RefBCb;
    
#if 0 /* default constants */
        CAdjLuma = 1.16455078125;

	CAdjRCb = 0.0;
	CAdjRCr = 1.59619140625;
	CAdjGCb = -0.39111328125;
	CAdjGCr = -0.8125;
	CAdjBCb = 2.01708984375;
	CAdjBCr = 0;
#endif
	OvLuma = CAdjLuma;
	OvRCb = CAdjRCb;
	OvRCr = CAdjRCr;
	OvGCb = CAdjGCb;
	OvGCr = CAdjGCr;
	OvBCb = CAdjBCb;
	OvBCr = CAdjBCr;
	OvROff = CAdjOff -
		OvLuma * Loff - (OvRCb + OvRCr) * Coff;
	OvGOff = CAdjOff - 
		OvLuma * Loff - (OvGCb + OvGCr) * Coff;
	OvBOff = CAdjOff - 
		OvLuma * Loff - (OvBCb + OvBCr) * Coff;
#if 0 /* default constants */
	OvROff = -888.5;
	OvGOff = 545;
	OvBOff = -1104;
#endif 
   
	dwOvROff = ((int)(OvROff * 2.0)) & 0x1fff;
	dwOvGOff = (int)(OvGOff * 2.0) & 0x1fff;
	dwOvBOff = (int)(OvBOff * 2.0) & 0x1fff;
	if(!IsR200)
	{
		dwOvLuma =(((int)(OvLuma * 2048.0))&0x7fff)<<17;
		dwOvRCb = (((int)(OvRCb * 2048.0))&0x7fff)<<1;
		dwOvRCr = (((int)(OvRCr * 2048.0))&0x7fff)<<17;
		dwOvGCb = (((int)(OvGCb * 2048.0))&0x7fff)<<1;
		dwOvGCr = (((int)(OvGCr * 2048.0))&0x7fff)<<17;
		dwOvBCb = (((int)(OvBCb * 2048.0))&0x7fff)<<1;
		dwOvBCr = (((int)(OvBCr * 2048.0))&0x7fff)<<17;
	}
	else
	{
		dwOvLuma = (((int)(OvLuma * 256.0))&0x7ff)<<20;
		dwOvRCb = (((int)(OvRCb * 256.0))&0x7ff)<<4;
		dwOvRCr = (((int)(OvRCr * 256.0))&0x7ff)<<20;
		dwOvGCb = (((int)(OvGCb * 256.0))&0x7ff)<<4;
		dwOvGCr = (((int)(OvGCr * 256.0))&0x7ff)<<20;
		dwOvBCb = (((int)(OvBCb * 256.0))&0x7ff)<<4;
		dwOvBCr = (((int)(OvBCr * 256.0))&0x7ff)<<20;
	}

	OUTREG(OV0_LIN_TRANS_A, dwOvRCb | dwOvLuma);
	OUTREG(OV0_LIN_TRANS_B, dwOvROff | dwOvRCr);
	OUTREG(OV0_LIN_TRANS_C, dwOvGCb | dwOvLuma);
	OUTREG(OV0_LIN_TRANS_D, dwOvGOff | dwOvGCr);
	OUTREG(OV0_LIN_TRANS_E, dwOvBCb | dwOvLuma);
	OUTREG(OV0_LIN_TRANS_F, dwOvBOff | dwOvBCr);
}
#endif

#ifndef RAGE128
/* Gamma curve definition */
typedef struct 
{
	unsigned int gammaReg;
	unsigned int gammaSlope;
	unsigned int gammaOffset;
}GAMMA_SETTINGS;

/* Recommended gamma curve parameters */
GAMMA_SETTINGS r200_def_gamma[18] = 
{
	{OV0_GAMMA_0_F, 0x100, 0x0000},
	{OV0_GAMMA_10_1F, 0x100, 0x0020},
	{OV0_GAMMA_20_3F, 0x100, 0x0040},
	{OV0_GAMMA_40_7F, 0x100, 0x0080},
	{OV0_GAMMA_80_BF, 0x100, 0x0100},
	{OV0_GAMMA_C0_FF, 0x100, 0x0100},
	{OV0_GAMMA_100_13F, 0x100, 0x0200},
	{OV0_GAMMA_140_17F, 0x100, 0x0200},
	{OV0_GAMMA_180_1BF, 0x100, 0x0300},
	{OV0_GAMMA_1C0_1FF, 0x100, 0x0300},
	{OV0_GAMMA_200_23F, 0x100, 0x0400},
	{OV0_GAMMA_240_27F, 0x100, 0x0400},
	{OV0_GAMMA_280_2BF, 0x100, 0x0500},
	{OV0_GAMMA_2C0_2FF, 0x100, 0x0500},
	{OV0_GAMMA_300_33F, 0x100, 0x0600},
	{OV0_GAMMA_340_37F, 0x100, 0x0600},
	{OV0_GAMMA_380_3BF, 0x100, 0x0700},
	{OV0_GAMMA_3C0_3FF, 0x100, 0x0700}
};

GAMMA_SETTINGS r100_def_gamma[6] = 
{
	{OV0_GAMMA_0_F, 0x100, 0x0000},
	{OV0_GAMMA_10_1F, 0x100, 0x0020},
	{OV0_GAMMA_20_3F, 0x100, 0x0040},
	{OV0_GAMMA_40_7F, 0x100, 0x0080},
	{OV0_GAMMA_380_3BF, 0x100, 0x0100},
	{OV0_GAMMA_3C0_3FF, 0x100, 0x0100}
};

static void make_default_gamma_correction( void )
{
    size_t i;
    if(!IsR200){
	OUTREG(OV0_LIN_TRANS_A, 0x12A00000);
	OUTREG(OV0_LIN_TRANS_B, 0x199018FE);
	OUTREG(OV0_LIN_TRANS_C, 0x12A0F9B0);
	OUTREG(OV0_LIN_TRANS_D, 0xF2F0043B);
	OUTREG(OV0_LIN_TRANS_E, 0x12A02050);
	OUTREG(OV0_LIN_TRANS_F, 0x0000174E);
	for(i=0; i<6; i++){
		OUTREG(r100_def_gamma[i].gammaReg,
		       (r100_def_gamma[i].gammaSlope<<16) |
		        r100_def_gamma[i].gammaOffset);
	}
    }
    else{
	OUTREG(OV0_LIN_TRANS_A, 0x12a00000);
	OUTREG(OV0_LIN_TRANS_B, 0x1990190e);
	OUTREG(OV0_LIN_TRANS_C, 0x12a0f9c0);
	OUTREG(OV0_LIN_TRANS_D, 0xf3000442);
	OUTREG(OV0_LIN_TRANS_E, 0x12a02040);
	OUTREG(OV0_LIN_TRANS_F, 0x175f);

	/* Default Gamma,
	   Of 18 segments for gamma cure, all segments in R200 are programmable,
	   while only lower 4 and upper 2 segments are programmable in Radeon*/
	for(i=0; i<18; i++){
		OUTREG(r200_def_gamma[i].gammaReg,
		       (r200_def_gamma[i].gammaSlope<<16) |
		        r200_def_gamma[i].gammaOffset);
	}
    }
}
#endif

static void radeon_vid_stop_video( void )
{
    radeon_engine_idle();
    OUTREG(OV0_SCALE_CNTL, SCALER_SOFT_RESET);
    OUTREG(OV0_EXCLUSIVE_HORZ, 0);
    OUTREG(OV0_AUTO_FLIP_CNTL, 0);   /* maybe */
    OUTREG(OV0_FILTER_CNTL, FILTER_HARDCODED_COEF);
    OUTREG(OV0_KEY_CNTL, GRAPHIC_KEY_FN_NE);
    OUTREG(OV0_TEST, 0);
}

static void radeon_vid_display_video( void )
{
    int bes_flags;
    radeon_fifo_wait(2);
    OUTREG(OV0_REG_LOAD_CNTL,		REG_LD_CTL_LOCK);
    radeon_engine_idle();
    while(!(INREG(OV0_REG_LOAD_CNTL)&REG_LD_CTL_LOCK_READBACK));
    radeon_fifo_wait(15);
    OUTREG(OV0_AUTO_FLIP_CNTL,OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD);
    OUTREG(OV0_AUTO_FLIP_CNTL,(INREG(OV0_AUTO_FLIP_CNTL)^OV0_AUTO_FLIP_CNTL_SOFT_EOF_TOGGLE));
    OUTREG(OV0_AUTO_FLIP_CNTL,(INREG(OV0_AUTO_FLIP_CNTL)^OV0_AUTO_FLIP_CNTL_SOFT_EOF_TOGGLE));

    OUTREG(OV0_DEINTERLACE_PATTERN,besr.deinterlace_pattern);
#ifdef RAGE128
    OUTREG(OV0_COLOUR_CNTL, (besr.brightness & 0x7f) |
			    (besr.saturation << 8) |
			    (besr.saturation << 16));
#endif
    radeon_fifo_wait(2);
    if(besr.ckey_on)
    {
	OUTREG(OV0_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
	OUTREG(OV0_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
	OUTREG(OV0_KEY_CNTL,GRAPHIC_KEY_FN_EQ|VIDEO_KEY_FN_FALSE|CMP_MIX_OR);
    }
    else
    {
	OUTREG(OV0_GRAPHICS_KEY_MSK, 0ULL);
	OUTREG(OV0_GRAPHICS_KEY_CLR, 0ULL);
	OUTREG(OV0_KEY_CNTL,GRAPHIC_KEY_FN_NE);
    }

    OUTREG(OV0_H_INC,			besr.h_inc);
    OUTREG(OV0_STEP_BY,			besr.step_by);
    OUTREG(OV0_Y_X_START,		besr.y_x_start);
    OUTREG(OV0_Y_X_END,			besr.y_x_end);
    OUTREG(OV0_V_INC,			besr.v_inc);
    OUTREG(OV0_P1_BLANK_LINES_AT_TOP,	besr.p1_blank_lines_at_top);
    OUTREG(OV0_P23_BLANK_LINES_AT_TOP,	besr.p23_blank_lines_at_top);
    OUTREG(OV0_VID_BUF_PITCH0_VALUE,	besr.vid_buf_pitch0_value);
    OUTREG(OV0_VID_BUF_PITCH1_VALUE,	besr.vid_buf_pitch1_value);
    OUTREG(OV0_P1_X_START_END,		besr.p1_x_start_end);
    OUTREG(OV0_P2_X_START_END,		besr.p2_x_start_end);
    OUTREG(OV0_P3_X_START_END,		besr.p3_x_start_end);
#ifdef RADEON
    OUTREG(OV0_BASE_ADDR,		besr.base_addr);
#endif
    OUTREG(OV0_VID_BUF0_BASE_ADRS,	besr.vid_buf0_base_adrs);
    OUTREG(OV0_VID_BUF1_BASE_ADRS,	besr.vid_buf1_base_adrs);
    OUTREG(OV0_VID_BUF2_BASE_ADRS,	besr.vid_buf2_base_adrs);
    radeon_fifo_wait(9);
    OUTREG(OV0_VID_BUF3_BASE_ADRS,	besr.vid_buf3_base_adrs);
    OUTREG(OV0_VID_BUF4_BASE_ADRS,	besr.vid_buf4_base_adrs);
    OUTREG(OV0_VID_BUF5_BASE_ADRS,	besr.vid_buf5_base_adrs);
    OUTREG(OV0_P1_V_ACCUM_INIT,		besr.p1_v_accum_init);
    OUTREG(OV0_P1_H_ACCUM_INIT,		besr.p1_h_accum_init);
    OUTREG(OV0_P23_H_ACCUM_INIT,	besr.p23_h_accum_init);
    OUTREG(OV0_P23_V_ACCUM_INIT,	besr.p23_v_accum_init);

#ifdef RADEON
    bes_flags = SCALER_ENABLE |
                SCALER_SMART_SWITCH;
//		SCALER_HORZ_PICK_NEAREST;
#else
    bes_flags = SCALER_ENABLE |
                SCALER_SMART_SWITCH |
		SCALER_Y2R_TEMP |
		SCALER_PIX_EXPAND;
#endif
    if(besr.double_buff) bes_flags |= SCALER_DOUBLE_BUFFER;
    if(besr.deinterlace_on) bes_flags |= SCALER_ADAPTIVE_DEINT;
#ifdef RAGE128
    bes_flags |= SCALER_BURST_PER_PLANE;
#endif
    switch(besr.fourcc)
    {
        case IMGFMT_RGB15:
        case IMGFMT_BGR15: bes_flags |= SCALER_SOURCE_15BPP; break;
        case IMGFMT_RGB16:
	case IMGFMT_BGR16: bes_flags |= SCALER_SOURCE_16BPP; break;
        case IMGFMT_RGB24:
        case IMGFMT_BGR24: bes_flags |= SCALER_SOURCE_24BPP; break;
        case IMGFMT_RGB32:
	case IMGFMT_BGR32: bes_flags |= SCALER_SOURCE_32BPP; break;
        /* 4:1:0*/
	case IMGFMT_IF09:
        case IMGFMT_YVU9:  bes_flags |= SCALER_SOURCE_YUV9; break;
        /* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:  bes_flags |= SCALER_SOURCE_YUV12;
			   break;
        /* 4:2:2 */
	case IMGFMT_UYVY:  bes_flags |= SCALER_SOURCE_YVYU422; break;
	case IMGFMT_YUY2:
	default:           bes_flags |= SCALER_SOURCE_VYUY422; break;
    }
    OUTREG(OV0_SCALE_CNTL,		bes_flags);
    OUTREG(OV0_REG_LOAD_CNTL,		0);
#ifdef DEBUG
    radeon_vid_dump_regs();
#endif
}

void radeon_vid_set_color_key(int ckey_on, uint8_t R, uint8_t G, uint8_t B)
{
    besr.ckey_on = ckey_on;
    besr.graphics_key_msk=(1ULL<<radeon_vid_get_dbpp()) - 1;
    besr.graphics_key_clr=(R<<16)|(G<<8)|(B)|(0x00 << 24);
}


#define XXX_SRC_X   0
#define XXX_SRC_Y   0

static int radeon_vid_init_video( mga_vid_config_t *config )
{
    uint32_t tmp,src_w,src_h,pitch,h_inc,step_by,left,leftUV,top;
    int is_420;
RTRACE(RVID_MSG"usr_config: version = %x format=%x card=%x ram=%u src(%ux%u) dest(%u:%ux%u:%u) frame_size=%u num_frames=%u\n"
	,(uint32_t)config->version
	,(uint32_t)config->format
	,(uint32_t)config->card_type
	,(uint32_t)config->ram_size
	,(uint32_t)config->src_width
	,(uint32_t)config->src_height
	,(uint32_t)config->x_org
	,(uint32_t)config->y_org
	,(uint32_t)config->dest_width
	,(uint32_t)config->dest_height
	,(uint32_t)config->frame_size
	,(uint32_t)config->num_frames);
    radeon_vid_stop_video();
    left = XXX_SRC_X << 16;
    top = XXX_SRC_Y << 16;
    src_h = config->src_height;
    src_w = config->src_width;
    switch(config->format)
    {
        case IMGFMT_RGB15:
        case IMGFMT_BGR15:
        case IMGFMT_RGB16:
	case IMGFMT_BGR16:
        case IMGFMT_RGB24:
        case IMGFMT_BGR24:
        case IMGFMT_RGB32:
	case IMGFMT_BGR32:
	/* 4:1:0 */
	case IMGFMT_IF09:
        case IMGFMT_YVU9:
	/* 4:2:0 */	
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	case IMGFMT_I420:
	/* 4:2:2 */
	case IMGFMT_UYVY:
	case IMGFMT_YUY2:
				break;
	default:
		printk(RVID_MSG"Unsupported pixel format: 0x%X\n",config->format);
		return -1;
    }
    is_420 = 0;
    if(config->format == IMGFMT_YV12 ||
       config->format == IMGFMT_I420 ||
       config->format == IMGFMT_IYUV) is_420 = 1;
    switch(config->format)
    {
	/* 4:1:0 */
        case IMGFMT_YVU9:
        case IMGFMT_IF09:
	/* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	case IMGFMT_I420: pitch = (src_w + 31) & ~31; break;
	/* 4:2:2 */
        default:
	case IMGFMT_UYVY:
	case IMGFMT_YUY2:
        case IMGFMT_RGB15:
        case IMGFMT_BGR15:
        case IMGFMT_RGB16:
	case IMGFMT_BGR16: pitch = ((src_w*2) + 15) & ~15; break;
        case IMGFMT_RGB24:
        case IMGFMT_BGR24: pitch = ((src_w*3) + 15) & ~15; break;
        case IMGFMT_RGB32:
	case IMGFMT_BGR32: pitch = ((src_w*4) + 15) & ~15; break;
    }
    if(radeon_is_dbl_scan()) config->dest_height *= 2;
    else
    if(radeon_is_interlace()) config->dest_height /= 2;
    besr.dest_bpp = radeon_vid_get_dbpp();
    besr.fourcc = config->format;
    besr.v_inc = (src_h << 20) / config->dest_height;
    h_inc = (src_w << 12) / config->dest_width;
    step_by = 1;

    while(h_inc >= (2 << 12)) {
	step_by++;
	h_inc >>= 1;
    }

    /* keep everything in 16.16 */
    besr.base_addr = radeon_mem_base;
    if(is_420)
    {
        uint32_t d1line,d2line,d3line;
	d1line = top*pitch;
	d2line = src_h*pitch+(d1line>>1);
	d3line = d2line+((src_h*pitch)>>2);
	d1line += (left >> 16) & ~15;
	d2line += (left >> 17) & ~15;
	d3line += (left >> 17) & ~15;
        besr.vid_buf0_base_adrs=((radeon_overlay_off+d1line)&VIF_BUF0_BASE_ADRS_MASK);
        besr.vid_buf1_base_adrs=((radeon_overlay_off+d2line)&VIF_BUF1_BASE_ADRS_MASK)|VIF_BUF1_PITCH_SEL;
        besr.vid_buf2_base_adrs=((radeon_overlay_off+d3line)&VIF_BUF2_BASE_ADRS_MASK)|VIF_BUF2_PITCH_SEL;
	if(besr.fourcc == IMGFMT_I420 || besr.fourcc == IMGFMT_IYUV)
	{
	  uint32_t tmp;
	  tmp = besr.vid_buf1_base_adrs;
	  besr.vid_buf1_base_adrs = besr.vid_buf2_base_adrs;
	  besr.vid_buf2_base_adrs = tmp;
	}
    }
    else
    {
      besr.vid_buf0_base_adrs = radeon_overlay_off;
      besr.vid_buf0_base_adrs += ((left & ~7) << 1)&VIF_BUF0_BASE_ADRS_MASK;
      besr.vid_buf1_base_adrs = besr.vid_buf0_base_adrs;
      besr.vid_buf2_base_adrs = besr.vid_buf0_base_adrs;
    }
    besr.vid_buf3_base_adrs = besr.vid_buf0_base_adrs+config->frame_size;
    besr.vid_buf4_base_adrs = besr.vid_buf1_base_adrs+config->frame_size;
    besr.vid_buf5_base_adrs = besr.vid_buf2_base_adrs+config->frame_size;

    tmp = (left & 0x0003ffff) + 0x00028000 + (h_inc << 3);
    besr.p1_h_accum_init = ((tmp <<  4) & 0x000f8000) |
			   ((tmp << 12) & 0xf0000000);

    tmp = ((left >> 1) & 0x0001ffff) + 0x00028000 + (h_inc << 2);
    besr.p23_h_accum_init = ((tmp <<  4) & 0x000f8000) |
			    ((tmp << 12) & 0x70000000);
    tmp = (top & 0x0000ffff) + 0x00018000;
    besr.p1_v_accum_init = ((tmp << 4) & OV0_P1_V_ACCUM_INIT_MASK)
			    |(OV0_P1_MAX_LN_IN_PER_LN_OUT & 1);

    tmp = ((top >> 1) & 0x0000ffff) + 0x00018000;
    besr.p23_v_accum_init = is_420 ? ((tmp << 4) & OV0_P23_V_ACCUM_INIT_MASK)
			    |(OV0_P23_MAX_LN_IN_PER_LN_OUT & 1) : 0;

    leftUV = (left >> 17) & 15;
    left = (left >> 16) & 15;
    besr.h_inc = h_inc | ((h_inc >> 1) << 16);
    besr.step_by = step_by | (step_by << 8);
    besr.y_x_start = (config->x_org+X_ADJUST) | (config->y_org << 16);
    besr.y_x_end = (config->x_org + config->dest_width+X_ADJUST) | ((config->y_org + config->dest_height) << 16);
    besr.p1_blank_lines_at_top = P1_BLNK_LN_AT_TOP_M1_MASK|((src_h-1)<<16);
    if(is_420)
    {
	src_h = (src_h + 1) >> 1;
	besr.p23_blank_lines_at_top = P23_BLNK_LN_AT_TOP_M1_MASK|((src_h-1)<<16);
    }
    else besr.p23_blank_lines_at_top = 0;
    besr.vid_buf_pitch0_value = pitch;
    besr.vid_buf_pitch1_value = is_420 ? pitch>>1 : pitch;
    besr.p1_x_start_end = (src_w+left-1)|(left<<16);
    src_w>>=1;
    besr.p2_x_start_end = (src_w+left-1)|(leftUV<<16);
    besr.p3_x_start_end = besr.p2_x_start_end;
    return 0;
}

static void radeon_vid_frame_sel(int frame)
{
    uint32_t off0,off1,off2;
    if(!besr.double_buff) return;
    if(frame%2)
    {
      off0 = besr.vid_buf3_base_adrs;
      off1 = besr.vid_buf4_base_adrs;
      off2 = besr.vid_buf5_base_adrs;
    }
    else
    {
      off0 = besr.vid_buf0_base_adrs;
      off1 = besr.vid_buf1_base_adrs;
      off2 = besr.vid_buf2_base_adrs;
    }
    OUTREG(OV0_REG_LOAD_CNTL,		REG_LD_CTL_LOCK);
    while(!(INREG(OV0_REG_LOAD_CNTL)&REG_LD_CTL_LOCK_READBACK));
    OUTREG(OV0_VID_BUF0_BASE_ADRS,	off0);
    OUTREG(OV0_VID_BUF1_BASE_ADRS,	off1);
    OUTREG(OV0_VID_BUF2_BASE_ADRS,	off2);
    OUTREG(OV0_REG_LOAD_CNTL,		0);
}

static void radeon_vid_make_default(void)
{
#ifdef RAGE128
  OUTREG(OV0_COLOUR_CNTL,0x00101000UL); /* Default brihgtness and saturation for Rage128 */
#else
  make_default_gamma_correction();
#endif
  besr.deinterlace_pattern = 0x900AAAAA;
  OUTREG(OV0_DEINTERLACE_PATTERN,besr.deinterlace_pattern);
  besr.deinterlace_on=1;
  besr.double_buff=1;
}


static void radeon_vid_preset(void)
{
#ifdef RAGE128
  unsigned tmp;
  tmp = INREG(OV0_COLOUR_CNTL);
  besr.saturation = (tmp>>8)&0x1f;
  besr.brightness = tmp & 0x7f;
#endif
  besr.graphics_key_clr = INREG(OV0_GRAPHICS_KEY_CLR);
  besr.deinterlace_pattern = INREG(OV0_DEINTERLACE_PATTERN);
}

static int video_on = 0;

static int radeon_vid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int frame;

	switch(cmd)
	{
		case MGA_VID_CONFIG:
			RTRACE(RVID_MSG"radeon_mmio_base = %p\n",radeon_mmio_base);
			RTRACE(RVID_MSG"radeon_mem_base = %08x\n",radeon_mem_base);
			RTRACE(RVID_MSG"Received configuration\n");

 			if(copy_from_user(&radeon_config,(mga_vid_config_t*) arg,sizeof(mga_vid_config_t)))
			{
				printk(RVID_MSG"failed copy from userspace\n");
				return -EFAULT;
			}
			if(radeon_config.version != MGA_VID_VERSION){
				printk(RVID_MSG"incompatible version! driver: %X  requested: %X\n",MGA_VID_VERSION,radeon_config.version);
				return -EFAULT;
			}

			if(radeon_config.frame_size==0 || radeon_config.frame_size>1024*768*2){
				printk(RVID_MSG"illegal frame_size: %d\n",radeon_config.frame_size);
				return -EFAULT;
			}

			if(radeon_config.num_frames<1){
				printk(RVID_MSG"illegal num_frames: %d\n",radeon_config.num_frames);
				return -EFAULT;
			}
			if(radeon_config.num_frames==1) besr.double_buff=0;
			if(!besr.double_buff) radeon_config.num_frames=1;
			else                  radeon_config.num_frames=2;
			radeon_config.card_type = 0;
			radeon_config.ram_size = radeon_ram_size;
			radeon_overlay_off = radeon_ram_size*0x100000 - radeon_config.frame_size*radeon_config.num_frames;
			radeon_overlay_off &= 0xffff0000;
			if(radeon_overlay_off < 0){
			    printk(RVID_MSG"not enough video memory. Need: %u has: %u\n",radeon_config.frame_size*radeon_config.num_frames,radeon_ram_size*0x100000);
			    return -EFAULT;
			}
			RTRACE(RVID_MSG"using video overlay at offset %08X\n",radeon_overlay_off);
			if (copy_to_user((mga_vid_config_t *) arg, &radeon_config, sizeof(mga_vid_config_t)))
			{
				printk(RVID_MSG"failed copy to userspace\n");
				return -EFAULT;
			}
			radeon_vid_set_color_key(radeon_config.colkey_on,
						 radeon_config.colkey_red,
						 radeon_config.colkey_green,
						 radeon_config.colkey_blue);
			if(swap_fourcc) radeon_config.format = swab32(radeon_config.format); 
			printk(RVID_MSG"configuring for '%s' fourcc\n",fourcc_format_name(radeon_config.format));
			return radeon_vid_init_video(&radeon_config);
		break;

		case MGA_VID_ON:
			RTRACE(RVID_MSG"Video ON (ioctl)\n");
			radeon_vid_display_video();
			video_on = 1;
		break;

		case MGA_VID_OFF:
			RTRACE(RVID_MSG"Video OFF (ioctl)\n");
			if(video_on) radeon_vid_stop_video();
			video_on = 0;
		break;

		case MGA_VID_FSEL:
			if(copy_from_user(&frame,(int *) arg,sizeof(int)))
			{
				printk(RVID_MSG"FSEL failed copy from userspace\n");
				return -EFAULT;
			}
			radeon_vid_frame_sel(frame);
		break;

	        default:
			printk(RVID_MSG"Invalid ioctl\n");
			return -EINVAL;
	}

	return 0;
}

struct ati_card_id_s
{
  const int id;
  const char name[17];
};

const struct ati_card_id_s ati_card_ids[]=
{
#ifdef RAGE128
 /*
    This driver should be compatible with Rage128 (pro) chips.
    (include adaptive deinterlacing!!!).
    Moreover: the same logic can be used with Mach64 chips.
    (I mean: mach64xx, 3d rage, 3d rage IIc, 3D rage pro, 3d rage mobility).
    but they are incompatible by i/o ports. So if enthusiasts will want
    then they can redefine OUTREG and INREG macros and redefine OV0_*
    constants. Also it seems that mach64 chips supports only: YUY2, YV12, UYVY
    fourccs (422 and 420 formats only).
  */
/* Rage128 Pro GL */
 { PCI_DEVICE_ID_ATI_Rage128_PA, "R128Pro PA" },
 { PCI_DEVICE_ID_ATI_Rage128_PB, "R128Pro PB" },
 { PCI_DEVICE_ID_ATI_Rage128_PC, "R128Pro PC" },
 { PCI_DEVICE_ID_ATI_Rage128_PD, "R128Pro PD" },
 { PCI_DEVICE_ID_ATI_Rage128_PE, "R128Pro PE" },
 { PCI_DEVICE_ID_ATI_RAGE128_PF, "R128Pro PF" },
/* Rage128 Pro VR */
 { PCI_DEVICE_ID_ATI_RAGE128_PG, "R128Pro PG" },
 { PCI_DEVICE_ID_ATI_RAGE128_PH, "R128Pro PH" },
 { PCI_DEVICE_ID_ATI_RAGE128_PI, "R128Pro PI" },
 { PCI_DEVICE_ID_ATI_RAGE128_PJ, "R128Pro PJ" },
 { PCI_DEVICE_ID_ATI_RAGE128_PK, "R128Pro PK" },
 { PCI_DEVICE_ID_ATI_RAGE128_PL, "R128Pro PL" },
 { PCI_DEVICE_ID_ATI_RAGE128_PM, "R128Pro PM" },
 { PCI_DEVICE_ID_ATI_RAGE128_PN, "R128Pro PN" },
 { PCI_DEVICE_ID_ATI_RAGE128_PO, "R128Pro PO" },
 { PCI_DEVICE_ID_ATI_RAGE128_PP, "R128Pro PP" },
 { PCI_DEVICE_ID_ATI_RAGE128_PQ, "R128Pro PQ" },
 { PCI_DEVICE_ID_ATI_RAGE128_PR, "R128Pro PR" },
 { PCI_DEVICE_ID_ATI_RAGE128_TR, "R128Pro TR" },
 { PCI_DEVICE_ID_ATI_RAGE128_PS, "R128Pro PS" },
 { PCI_DEVICE_ID_ATI_RAGE128_PT, "R128Pro PT" },
 { PCI_DEVICE_ID_ATI_RAGE128_PU, "R128Pro PU" },
 { PCI_DEVICE_ID_ATI_RAGE128_PV, "R128Pro PV" },
 { PCI_DEVICE_ID_ATI_RAGE128_PW, "R128Pro PW" },
 { PCI_DEVICE_ID_ATI_RAGE128_PX, "R128Pro PX" },
/* Rage128 GL */
 { PCI_DEVICE_ID_ATI_RAGE128_RE, "R128 RE" },
 { PCI_DEVICE_ID_ATI_RAGE128_RF, "R128 RF" },
 { PCI_DEVICE_ID_ATI_RAGE128_RG, "R128 RG" },
 { PCI_DEVICE_ID_ATI_RAGE128_RH, "R128 RH" },
 { PCI_DEVICE_ID_ATI_RAGE128_RI, "R128 RI" },
/* Rage128 VR */
 { PCI_DEVICE_ID_ATI_RAGE128_RK, "R128 RK" },
 { PCI_DEVICE_ID_ATI_RAGE128_RL, "R128 RL" },
 { PCI_DEVICE_ID_ATI_RAGE128_RM, "R128 RM" },
 { PCI_DEVICE_ID_ATI_RAGE128_RN, "R128 RN" },
 { PCI_DEVICE_ID_ATI_RAGE128_RO, "R128 RO" },
/* Rage128 M3 */
 { PCI_DEVICE_ID_ATI_RAGE128_LE, "R128 M3 LE" },
 { PCI_DEVICE_ID_ATI_RAGE128_LF, "R128 M3 LF" },
/* Rage128 Pro Ultra */
 { PCI_DEVICE_ID_ATI_RAGE128_U1, "R128Pro U1" },
 { PCI_DEVICE_ID_ATI_RAGE128_U2, "R128Pro U2" },
 { PCI_DEVICE_ID_ATI_RAGE128_U3, "R128Pro U3" }
#else
/* Radeons (indeed: Rage 256 Pro ;) */
 { PCI_DEVICE_ID_RADEON_QD, "Radeon QD " },
 { PCI_DEVICE_ID_RADEON_QE, "Radeon QE " },
 { PCI_DEVICE_ID_RADEON_QF, "Radeon QF " },
 { PCI_DEVICE_ID_RADEON_QG, "Radeon QG " },
 { PCI_DEVICE_ID_RADEON_QY, "Radeon VE QY " },
 { PCI_DEVICE_ID_RADEON_QZ, "Radeon VE QZ " },
 { PCI_DEVICE_ID_RADEON_LY, "Radeon M6 LY " },
 { PCI_DEVICE_ID_RADEON_LZ, "Radeon M6 LZ " },
 { PCI_DEVICE_ID_RADEON_LW, "Radeon M7 LW " },
 { PCI_DEVICE_ID_R200_QL,   "Radeon2 8500 QL " },
 { PCI_DEVICE_ID_R200_BB,   "Radeon2 8500 AIW" },
 { PCI_DEVICE_ID_RV200_QW,  "Radeon2 7500 QW " }
#endif
};

static int detected_chip;

static int __init radeon_vid_config_card(void)
{
	struct pci_dev *dev = NULL;
	size_t i;

	for(i=0;i<sizeof(ati_card_ids)/sizeof(struct ati_card_id_s);i++)
	  if((dev=pci_find_device(PCI_VENDOR_ID_ATI, ati_card_ids[i].id, NULL)))
		break;
	if(!dev)
	{
		printk(RVID_MSG"No supported cards found\n");
		return FALSE;
	}

	radeon_mmio_base = ioremap_nocache(pci_resource_start (dev, 2),RADEON_REGSIZE);
	radeon_mem_base =  dev->resource[0].start;

	RTRACE(RVID_MSG"MMIO at 0x%p\n", radeon_mmio_base);
	RTRACE(RVID_MSG"Frame Buffer at 0x%08x\n", radeon_mem_base);

	/* video memory size */
	radeon_ram_size = INREG(CONFIG_MEMSIZE);

	/* mem size is bits [28:0], mask off the rest. Range: from 1Mb up to 512 Mb */
	radeon_ram_size &=  CONFIG_MEMSIZE_MASK;
	radeon_ram_size /= 0x100000;
	detected_chip = i;
	printk(RVID_MSG"Found %s (%uMb memory)\n",ati_card_ids[i].name,radeon_ram_size);
#ifndef RAGE128	
	if(ati_card_ids[i].id == PCI_DEVICE_ID_R200_QL || 
	   ati_card_ids[i].id == PCI_DEVICE_ID_R200_BB || 
	   ati_card_ids[i].id == PCI_DEVICE_ID_RV200_QW) IsR200 = 1;
#endif
	return TRUE;
}

#define PARAM_BRIGHTNESS "brightness="
#define PARAM_SATURATION "saturation="
#define PARAM_CONTRAST "contrast="
#define PARAM_HUE "hue="
#define PARAM_DOUBLE_BUFF "double_buff="
#define PARAM_DEINTERLACE "deinterlace="
#define PARAM_DEINTERLACE_PATTERN "deinterlace_pattern="
#ifdef RADEON_FPU
static int ovBrightness=0, ovSaturation=0, ovContrast=0, ovHue=0, ov_trans_idx=0;
#endif

static void radeon_param_buff_fill( void )
{
    unsigned len,saturation;
    int8_t brightness;
    brightness = besr.brightness & 0x7f;
    /* FIXME: It's probably x86 specific convertion. But it doesn't matter
       for general logic - only for printing value */
    if(brightness > 63) brightness = (((~besr.brightness) & 0x3f)+1) * (-1);
    saturation = besr.saturation;
    len = 0;
    len += sprintf(&radeon_param_buff[len],"Interface version: %04X\nDriver version: %s\n",MGA_VID_VERSION,RADEON_VID_VERSION);
    len += sprintf(&radeon_param_buff[len],"Chip: %s\n",ati_card_ids[detected_chip].name);
    len += sprintf(&radeon_param_buff[len],"Memory: %x:%x\n",radeon_mem_base,radeon_ram_size*0x100000);
    len += sprintf(&radeon_param_buff[len],"MMIO: %p\n",radeon_mmio_base);
    len += sprintf(&radeon_param_buff[len],"Overlay offset: %x\n",radeon_overlay_off);
#ifdef CONFIG_MTRR
    len += sprintf(&radeon_param_buff[len],"Tune MTRR: %s\n",mtrr?"on":"off");
#endif
    if(besr.ckey_on) len += sprintf(&radeon_param_buff[len],"Last used color_key=%X (mask=%X)\n",besr.graphics_key_clr,besr.graphics_key_msk);
    len += sprintf(&radeon_param_buff[len],"Swapped fourcc: %s\n",swap_fourcc?"on":"off");
    len += sprintf(&radeon_param_buff[len],"Last BPP: %u\n",besr.dest_bpp);
    len += sprintf(&radeon_param_buff[len],"Last fourcc: %s\n\n",fourcc_format_name(besr.fourcc));
    len += sprintf(&radeon_param_buff[len],"Configurable stuff:\n");
    len += sprintf(&radeon_param_buff[len],"~~~~~~~~~~~~~~~~~~~\n");
    len += sprintf(&radeon_param_buff[len],PARAM_DOUBLE_BUFF"%s\n",besr.double_buff?"on":"off");
#ifdef RAGE128
    len += sprintf(&radeon_param_buff[len],PARAM_BRIGHTNESS"%i\n",(int)brightness);
    len += sprintf(&radeon_param_buff[len],PARAM_SATURATION"%u\n",saturation);
#else
#ifdef RADEON_FPU
    len += sprintf(&radeon_param_buff[len],PARAM_BRIGHTNESS"%i\n",ovBrightness);
    len += sprintf(&radeon_param_buff[len],PARAM_SATURATION"%i\n",ovSaturation);
    len += sprintf(&radeon_param_buff[len],PARAM_CONTRAST"%i\n",ovContrast);
    len += sprintf(&radeon_param_buff[len],PARAM_HUE"%i\n",ovHue);
#endif
#endif
    len += sprintf(&radeon_param_buff[len],PARAM_DEINTERLACE"%s\n",besr.deinterlace_on?"on":"off");
    len += sprintf(&radeon_param_buff[len],PARAM_DEINTERLACE_PATTERN"%X\n",besr.deinterlace_pattern);
    radeon_param_buff_len = len;
}

static ssize_t radeon_vid_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    uint32_t size;
    if(!radeon_param_buff) return -ESPIPE;
    if(!(*ppos)) radeon_param_buff_fill();
    if(*ppos >= radeon_param_buff_len) return 0;
    size = min(count,radeon_param_buff_len-(uint32_t)(*ppos));
    memcpy(buf,radeon_param_buff,size);
    *ppos += size;
    return size;
}

#define RTFSaturation(a)   (1.0 + ((a)*1.0)/1000.0)
#define RTFBrightness(a)   (((a)*1.0)/2000.0)
#define RTFContrast(a)   (1.0 + ((a)*1.0)/1000.0)
#define RTFHue(a)   (((a)*3.1416)/1000.0)
#define RadeonSetParm(a,b,c,d) if((b)>=(c)&&(b)<=(d)) { (a)=(b);\
	radeon_set_transform(RTFBrightness(ovBrightness),RTFContrast(ovContrast)\
			    ,RTFSaturation(ovSaturation),RTFHue(ovHue),ov_trans_idx); }


static ssize_t radeon_vid_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
#ifdef RAGE128
    if(memcmp(buf,PARAM_BRIGHTNESS,min(count,strlen(PARAM_BRIGHTNESS))) == 0)
    {
      long brightness;
      brightness=simple_strtol(&buf[strlen(PARAM_BRIGHTNESS)],NULL,10);
      if(brightness >= -64 && brightness <= 63) 
      {
        besr.brightness = brightness;
	OUTREG(OV0_COLOUR_CNTL, (brightness & 0x7f) |
				(besr.saturation << 8) |
				(besr.saturation << 16));
      }
    }
    else
    if(memcmp(buf,PARAM_SATURATION,min(count,strlen(PARAM_SATURATION))) == 0)
    {
      long saturation;
      saturation=simple_strtol(&buf[strlen(PARAM_SATURATION)],NULL,10);
      if(saturation >= 0 && saturation <= 31)
	OUTREG(OV0_COLOUR_CNTL, (besr.brightness & 0x7f) |
				(saturation << 8) |
				(saturation << 16));
    }
    else
#else
#ifdef RADEON_FPU
    if(memcmp(buf,PARAM_BRIGHTNESS,min(count,strlen(PARAM_BRIGHTNESS))) == 0)
    {
      int tmp;
      tmp=simple_strtol(&buf[strlen(PARAM_BRIGHTNESS)],NULL,10);
      RadeonSetParm(ovBrightness,tmp,-1000,1000);
    }
    else
    if(memcmp(buf,PARAM_SATURATION,min(count,strlen(PARAM_SATURATION))) == 0)
    {
      int tmp;
      tmp=simple_strtol(&buf[strlen(PARAM_SATURATION)],NULL,10);
      RadeonSetParm(ovSaturation,tmp,-1000,1000);
    }
    else
    if(memcmp(buf,PARAM_CONTRAST,min(count,strlen(PARAM_CONTRAST))) == 0)
    {
      int tmp;
      tmp=simple_strtol(&buf[strlen(PARAM_CONTRAST)],NULL,10);
      RadeonSetParm(ovContrast,tmp,-1000,1000);
    }
    else
    if(memcmp(buf,PARAM_HUE,min(count,strlen(PARAM_HUE))) == 0)
    {
      int tmp;
      tmp=simple_strtol(&buf[strlen(PARAM_HUE)],NULL,10);
      RadeonSetParm(ovHue,tmp,-1000,1000);
    }
    else
#endif
#endif
    if(memcmp(buf,PARAM_DOUBLE_BUFF,min(count,strlen(PARAM_DOUBLE_BUFF))) == 0)
    {
      if(memcmp(&buf[strlen(PARAM_DOUBLE_BUFF)],"on",2) == 0) besr.double_buff = 1;
      else besr.double_buff = 0;
    }
    else
    if(memcmp(buf,PARAM_DEINTERLACE,min(count,strlen(PARAM_DEINTERLACE))) == 0)
    {
      if(memcmp(&buf[strlen(PARAM_DEINTERLACE)],"on",2) == 0) besr.deinterlace_on = 1;
      else besr.deinterlace_on = 0;
    }
    else
    if(memcmp(buf,PARAM_DEINTERLACE_PATTERN,min(count,strlen(PARAM_DEINTERLACE_PATTERN))) == 0)
    {
      long dpat;
      dpat=simple_strtol(&buf[strlen(PARAM_DEINTERLACE_PATTERN)],NULL,16);
	OUTREG(OV0_DEINTERLACE_PATTERN, dpat);
    }
    else count = -EIO;
    radeon_vid_preset();
    return count;
}

static int radeon_vid_mmap(struct file *file, struct vm_area_struct *vma)
{

	RTRACE(RVID_MSG"mapping video memory into userspace\n");
	if(remap_page_range(vma->vm_start, radeon_mem_base + radeon_overlay_off,
		 vma->vm_end - vma->vm_start, vma->vm_page_prot)) 
	{
		printk(RVID_MSG"error mapping video memory\n");
		return -EAGAIN;
	}

	return 0;
}

static int radeon_vid_release(struct inode *inode, struct file *file)
{
	radeon_vid_in_use = 0;
	radeon_vid_stop_video();

	MOD_DEC_USE_COUNT;
	return 0;
}

static long long radeon_vid_lseek(struct file *file, long long offset, int origin)
{
	return -ESPIPE;
}					 

static int radeon_vid_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);

	if(minor != 0)
	 return -ENXIO;

	if(radeon_vid_in_use == 1) 
		return -EBUSY;

	radeon_vid_in_use = 1;
	MOD_INC_USE_COUNT;
	return 0;
}

#if LINUX_VERSION_CODE >= 0x020400
static struct file_operations radeon_vid_fops =
{
	llseek:		radeon_vid_lseek,
	read:		radeon_vid_read,
	write:		radeon_vid_write,
/*
	readdir:
	poll:
*/
	ioctl:		radeon_vid_ioctl,
	mmap:		radeon_vid_mmap,
	open:		radeon_vid_open,
/*
	flush:
*/
	release: 	radeon_vid_release
/*
	fsync:
	fasync:
	lock:
	readv:
	writev:
	sendpage:
	get_unmapped_area:
*/
};
#else
static struct file_operations radeon_vid_fops =
{
	radeon_vid_lseek,
	radeon_vid_read,
	radeon_vid_write,
	NULL,
	NULL,
	radeon_vid_ioctl,
	radeon_vid_mmap,
	radeon_vid_open,
	NULL,
	radeon_vid_release
};
#endif

/* 
 * Main Initialization Function 
 */

static int __init radeon_vid_initialize(void)
{
	radeon_vid_in_use = 0;
#ifdef RAGE128
	printk(RVID_MSG"Rage128/Rage128Pro video overlay driver v"RADEON_VID_VERSION" (C) Nick Kurshev\n");
#else
	printk(RVID_MSG"Radeon video overlay driver v"RADEON_VID_VERSION" (C) Nick Kurshev\n");
#endif
	if(register_chrdev(RADEON_VID_MAJOR, "radeon_vid", &radeon_vid_fops))
	{
		printk(RVID_MSG"unable to get major: %d\n", RADEON_VID_MAJOR);
		return -EIO;
	}

	if (!radeon_vid_config_card())
	{
		printk(RVID_MSG"can't configure this card\n");
		unregister_chrdev(RADEON_VID_MAJOR, "radeon_vid");
		return -EINVAL;
	}
	radeon_param_buff = kmalloc(PARAM_BUFF_SIZE,GFP_KERNEL);
	if(radeon_param_buff) radeon_param_buff_size = PARAM_BUFF_SIZE;
#if 0
	radeon_vid_save_state();
#endif
	radeon_vid_make_default();
	radeon_vid_preset();
#ifdef CONFIG_MTRR
	if (mtrr) {
		smtrr.vram = mtrr_add(radeon_mem_base,
				radeon_ram_size*0x100000, MTRR_TYPE_WRCOMB, 1);
		smtrr.vram_valid = 1;
		/* let there be speed */
		printk(RVID_MSG"MTRR set to ON\n");
	}
#endif /* CONFIG_MTRR */
	return 0;
}

int __init init_module(void)
{
	return radeon_vid_initialize();
}

void __exit cleanup_module(void)
{
#if 0
	radeon_vid_restore_state();
#endif
	if(radeon_mmio_base)
		iounmap(radeon_mmio_base);
	kfree(radeon_param_buff);
	RTRACE(RVID_MSG"Cleaning up module\n");
	unregister_chrdev(RADEON_VID_MAJOR, "radeon_vid");
#ifdef CONFIG_MTRR
        if (smtrr.vram_valid)
            mtrr_del(smtrr.vram, radeon_mem_base,
                     radeon_ram_size*0x100000);
#endif /* CONFIG_MTRR */
}

