/*
   radeon_vid - VIDIX based video driver for Radeon and Rage128 chips
   Copyrights 2002 Nick Kurshev. This file is based on sources from
   GATOS (gatos.sf.net) and X11 (www.xfree86.org)
   Licence: GPL
*/

#include "../../libdha/pci_ids.h"
#include "../../libdha/pci_names.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include "../vidix.h"
#include "../fourcc.h"
#include "../../libdha/libdha.h"
#include "radeon.h"

#ifdef RAGE128
#define RADEON_MSG "Rage128_vid:"
#define X_ADJUST 0
#else
#define RADEON_MSG "Radeon_vid:"
#define X_ADJUST 8
#ifndef RADEON
#define RADEON
#endif
#endif

static int __verbose = 0;

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
  const char * sname;
  uint32_t name;
  uint32_t value;
}video_registers_t;

static bes_registers_t besr;
#ifndef RAGE128
static int IsR200=0;
#endif
#define DECLARE_VREG(name) { #name, name, 0 }
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

static void * radeon_mmio_base = 0;
static void * radeon_mem_base = 0; 
static int32_t radeon_overlay_off = 0;
static uint32_t radeon_ram_size = 0;

#define GETREG(TYPE,PTR,OFFZ)		(*((volatile TYPE*)((PTR)+(OFFZ))))
#define SETREG(TYPE,PTR,OFFZ,VAL)	(*((volatile TYPE*)((PTR)+(OFFZ))))=VAL

#define INREG8(addr)		GETREG(uint8_t,(uint32_t)(radeon_mmio_base),addr)
#define OUTREG8(addr,val)	SETREG(uint8_t,(uint32_t)(radeon_mmio_base),addr,val)
#define INREG(addr)		GETREG(uint32_t,(uint32_t)(radeon_mmio_base),addr)
#define OUTREG(addr,val)	SETREG(uint32_t,(uint32_t)(radeon_mmio_base),addr,val)
#define OUTREGP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INREG(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTREG(addr, _tmp);					\
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


static __inline__ void _radeon_fifo_wait (unsigned entries)
{
	int i;

	for (i=0; i<2000000; i++)
		if ((INREG(RBBM_STATUS) & 0x7f) >= entries)
			return;
}


static __inline__ void _radeon_engine_idle ( void )
{
	int i;

	/* ensure FIFO is empty before waiting for idle */
	_radeon_fifo_wait (64);

	for (i=0; i<2000000; i++) {
		if (((INREG(RBBM_STATUS) & GUI_ACTIVE)) == 0) {
			radeon_engine_flush ();
			return;
		}
	}
}

#define radeon_engine_idle()		_radeon_engine_idle()
#define radeon_fifo_wait(entries)	_radeon_fifo_wait(entries)


#ifndef RAGE128
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

	uint32_t dwOvLuma, dwOvROff, dwOvGOff, dwOvBOff;
	uint32_t dwOvRCb, dwOvRCr;
	uint32_t dwOvGCb, dwOvGCr;
	uint32_t dwOvBCb, dwOvBCr;

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


unsigned vixGetVersion( void ) { return VIDIX_VERSION; }

static unsigned short ati_card_ids[] = 
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
 DEVICE_ATI_RAGE_128_PA_PRO,
 DEVICE_ATI_RAGE_128_PB_PRO,
 DEVICE_ATI_RAGE_128_PC_PRO,
 DEVICE_ATI_RAGE_128_PD_PRO,
 DEVICE_ATI_RAGE_128_PE_PRO,
 DEVICE_ATI_RAGE_128_PF_PRO,
/* Rage128 Pro VR */
 DEVICE_ATI_RAGE_128_PG_PRO,
 DEVICE_ATI_RAGE_128_PH_PRO,
 DEVICE_ATI_RAGE_128_PI_PRO,
 DEVICE_ATI_RAGE_128_PJ_PRO,
 DEVICE_ATI_RAGE_128_PK_PRO,
 DEVICE_ATI_RAGE_128_PL_PRO,
 DEVICE_ATI_RAGE_128_PM_PRO,
 DEVICE_ATI_RAGE_128_PN_PRO,
 DEVICE_ATI_RAGE_128_PO_PRO,
 DEVICE_ATI_RAGE_128_PP_PRO,
 DEVICE_ATI_RAGE_128_PQ_PRO,
 DEVICE_ATI_RAGE_128_PR_PRO,
 DEVICE_ATI_RAGE_128_PS_PRO,
 DEVICE_ATI_RAGE_128_PT_PRO,
 DEVICE_ATI_RAGE_128_PU_PRO,
 DEVICE_ATI_RAGE_128_PV_PRO,
 DEVICE_ATI_RAGE_128_PW_PRO,
 DEVICE_ATI_RAGE_128_PX_PRO,
/* Rage128 GL */
 DEVICE_ATI_RAGE_128_RE_SG,
 DEVICE_ATI_RAGE_128_RF_SG,
 DEVICE_ATI_RAGE_128_RG,
 DEVICE_ATI_RAGE_128_RK_VR,
 DEVICE_ATI_RAGE_128_RL_VR,
 DEVICE_ATI_RAGE_128_SE_4X,
 DEVICE_ATI_RAGE_128_SF_4X,
 DEVICE_ATI_RAGE_128_SG_4X,
 DEVICE_ATI_RAGE_128_4X,
 DEVICE_ATI_RAGE_128_SK_4X,
 DEVICE_ATI_RAGE_128_SL_4X,
 DEVICE_ATI_RAGE_128_SM_4X,
 DEVICE_ATI_RAGE_128_4X2,
 DEVICE_ATI_RAGE_128_PRO,
 DEVICE_ATI_RAGE_128_PRO2,
 DEVICE_ATI_RAGE_128_PRO3
#else
/* Radeons (indeed: Rage 256 Pro ;) */
 DEVICE_ATI_RADEON_8500_DV,
 DEVICE_ATI_RADEON_MOBILITY_M6,
 DEVICE_ATI_RADEON_MOBILITY_M62,
 DEVICE_ATI_RADEON_MOBILITY_M63,
 DEVICE_ATI_RADEON_QD,
 DEVICE_ATI_RADEON_QE,
 DEVICE_ATI_RADEON_QF,
 DEVICE_ATI_RADEON_QG,
 DEVICE_ATI_RADEON_QL,
 DEVICE_ATI_RADEON_QW,
 DEVICE_ATI_RADEON_VE_QY,
 DEVICE_ATI_RADEON_VE_QZ
#endif
};

static int find_chip(unsigned chip_id)
{
  unsigned i;
  for(i = 0;i < sizeof(ati_card_ids)/sizeof(unsigned short);i++)
  {
    if(chip_id == ati_card_ids[i]) return i;
  }
  return -1;
}

pciinfo_t pci_info;
static int probed=0;

vidix_capability_t def_cap = 
{
#ifdef RAGE128
    "BES driver for rage128 cards",
#else
    "BES driver for radeon cards",
#endif
    TYPE_OUTPUT | TYPE_FX,
    { 0, 0, 0, 0 },
    1024,
    768,
    4,
    4,
    -1,
    FLAG_UPSCALER | FLAG_DOWNSCALER,
    VENDOR_ATI,
    0,
    { 0, 0, 0, 0}
};


int vixProbe( int verbose,int force )
{
  pciinfo_t lst[MAX_PCI_DEVICES];
  unsigned i,num_pci;
  int err;
  __verbose = verbose;
  err = pci_scan(lst,&num_pci);
  if(err)
  {
    printf(RADEON_MSG" Error occured during pci scan: %s\n",strerror(err));
    return err;
  }
  else
  {
    err = ENXIO;
    for(i=0;i<num_pci;i++)
    {
      if(lst[i].vendor == VENDOR_ATI)
      {
        int idx;
	const char *dname;
	idx = find_chip(lst[i].device);
	if(idx == -1 && force == PROBE_NORMAL) continue;
	dname = pci_device_name(VENDOR_ATI,lst[i].device);
	dname = dname ? dname : "Unknown chip";
	printf(RADEON_MSG" Found chip: %s\n",dname);
#ifndef RAGE128	
	if(idx != -1)
	    if(ati_card_ids[idx] == DEVICE_ATI_RADEON_QL || 
		ati_card_ids[idx] == DEVICE_ATI_RADEON_8500_DV || 
		ati_card_ids[idx] == DEVICE_ATI_RADEON_QW) IsR200 = 1;
#endif
	def_cap.device_id = lst[i].device;
	err = 0;
	memcpy(&pci_info,&lst[i],sizeof(pciinfo_t));
	probed=1;
	break;
      }
    }
  }
  if(err && verbose) printf(RADEON_MSG" Can't find chip\n");
  return err;
}

int vixInit( void )
{
  if(!probed) 
  {
    printf(RADEON_MSG" Driver was not probed but is being initializing\n");
    return EINTR;
  }    
  if((radeon_mmio_base = map_phys_mem(pci_info.base2,0xFFFF))==(void *)-1) return ENOMEM;
  radeon_ram_size = INREG(CONFIG_MEMSIZE);
  /* mem size is bits [28:0], mask off the rest. Range: from 1Mb up to 512 Mb */
  radeon_ram_size &=  CONFIG_MEMSIZE_MASK;
  if((radeon_mem_base = map_phys_mem(pci_info.base0,radeon_ram_size))==(void *)-1) return ENOMEM;
  memset(&besr,0,sizeof(bes_registers_t));
  radeon_vid_make_default();
  printf(RADEON_MSG" Video memory = %uMb\n",radeon_ram_size/0x100000);
  return 0;  
}

void vixDestroy( void )
{
  unmap_phys_mem(radeon_mem_base,radeon_ram_size);
  unmap_phys_mem(radeon_mmio_base,0x7FFF);
}

int vixGetCapability(vidix_capability_t *to)
{
  memcpy(to,&def_cap,sizeof(vidix_capability_t));
  return 0; 
}

uint32_t supported_fourcc[] = 
{
  IMGFMT_YV12, IMGFMT_I420, IMGFMT_IYUV, 
  IMGFMT_UYVY, IMGFMT_YUY2
};

__inline__ static int is_supported_fourcc(uint32_t fourcc)
{
  unsigned i;
  for(i=0;i<sizeof(supported_fourcc)/sizeof(uint32_t);i++)
  {
    if(fourcc==supported_fourcc[i]) return 1;
  }
  return 0;
}

int vixQueryFourcc(vidix_fourcc_t *to)
{
    if(is_supported_fourcc(to->fourcc))
    {
	to->depth = VID_DEPTH_1BPP | VID_DEPTH_2BPP |
		    VID_DEPTH_4BPP | VID_DEPTH_8BPP |
		    VID_DEPTH_12BPP| VID_DEPTH_15BPP|
		    VID_DEPTH_16BPP| VID_DEPTH_24BPP|
		    VID_DEPTH_32BPP;
	to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK;
	return 0;
    }
    else  to->depth = to->flags = 0;
    return ENOSYS;
}

static void radeon_vid_dump_regs( void )
{
  size_t i;
  printf(RADEON_MSG"*** Begin of DRIVER variables dump ***\n");
  printf(RADEON_MSG"radeon_mmio_base=%p\n",radeon_mmio_base);
  printf(RADEON_MSG"radeon_mem_base=%p\n",radeon_mem_base);
  printf(RADEON_MSG"radeon_overlay_off=%08X\n",radeon_overlay_off);
  printf(RADEON_MSG"radeon_ram_size=%08X\n",radeon_ram_size);
  printf(RADEON_MSG"*** Begin of OV0 registers dump ***\n");
  for(i=0;i<sizeof(vregs)/sizeof(video_registers_t);i++)
	printf(RADEON_MSG"%s = %08X\n",vregs[i].sname,INREG(vregs[i].name));
  printf(RADEON_MSG"*** End of OV0 registers dump ***\n");
}

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

    bes_flags = SCALER_ENABLE |
                SCALER_SMART_SWITCH |
#ifdef RADEON
		SCALER_HORZ_PICK_NEAREST;
#else
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
    if(__verbose > 1) radeon_vid_dump_regs();
}

static unsigned radeon_query_pitch(unsigned fourcc)
{
  unsigned pitch;
  switch(fourcc)
  {
	/* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	case IMGFMT_I420: pitch = 32; break;
	default:	  pitch = 16; break;
  }
  return pitch;
}

static int radeon_vid_init_video( vidix_playback_t *config )
{
    uint32_t tmp,src_w,src_h,dest_w,dest_h,pitch,h_inc,step_by,left,leftUV,top;
    int is_420;
    radeon_vid_stop_video();
    left = config->src.x << 16;
    top =  config->src.y << 16;
    src_h = config->src.h;
    src_w = config->src.w;
    is_420 = 0;
    if(config->fourcc == IMGFMT_YV12 ||
       config->fourcc == IMGFMT_I420 ||
       config->fourcc == IMGFMT_IYUV) is_420 = 1;
    switch(config->fourcc)
    {
	/* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	case IMGFMT_I420: pitch = (src_w + 31) & ~31;
			  config->dest.pitch.y = 
			  config->dest.pitch.u = 
			  config->dest.pitch.v = 32;
			  break;
	/* 4:2:2 */
        default:
	case IMGFMT_UYVY:
	case IMGFMT_YUY2:
			  pitch = ((src_w*2) + 15) & ~15;
			  config->dest.pitch.y =
			  config->dest.pitch.u =
			  config->dest.pitch.v = 16;
			  break;
    }
    dest_w = config->dest.w;
    dest_h = config->dest.h;
    if(radeon_is_dbl_scan()) dest_h *= 2;
    else
    if(radeon_is_interlace()) dest_h /= 2;
    besr.dest_bpp = radeon_vid_get_dbpp();
    besr.fourcc = config->fourcc;
    besr.v_inc = (src_h << 20) / dest_h;
    h_inc = (src_w << 12) / dest_w;
    step_by = 1;

    while(h_inc >= (2 << 12)) {
	step_by++;
	h_inc >>= 1;
    }

    /* keep everything in 16.16 */
    besr.base_addr = INREG(DISPLAY_BASE_ADDR);
    if(is_420)
    {
        uint32_t d1line,d2line,d3line;
	d1line = top*pitch;
	d2line = src_h*pitch+(d1line>>1);
	d3line = d2line+((src_h*pitch)>>2);
	d1line += (left >> 16) & ~15;
	d2line += (left >> 17) & ~15;
	d3line += (left >> 17) & ~15;
	config->offset.y = d1line & VIF_BUF0_BASE_ADRS_MASK;
	config->offset.v = d2line & VIF_BUF1_BASE_ADRS_MASK;
	config->offset.u = d3line & VIF_BUF2_BASE_ADRS_MASK;
        besr.vid_buf0_base_adrs=(radeon_overlay_off+config->offset.y);
        besr.vid_buf1_base_adrs=(radeon_overlay_off+config->offset.v)|VIF_BUF1_PITCH_SEL;
        besr.vid_buf2_base_adrs=(radeon_overlay_off+config->offset.u)|VIF_BUF2_PITCH_SEL;
	if(besr.fourcc == IMGFMT_I420 || besr.fourcc == IMGFMT_IYUV)
	{
	  uint32_t tmp;
	  tmp = besr.vid_buf1_base_adrs;
	  besr.vid_buf1_base_adrs = besr.vid_buf2_base_adrs;
	  besr.vid_buf2_base_adrs = tmp;
	  tmp = config->offset.u;
	  config->offset.u = config->offset.v;
	  config->offset.v = tmp;
	}
    }
    else
    {
      besr.vid_buf0_base_adrs = radeon_overlay_off;
      config->offset.y = config->offset.u = config->offset.v = ((left & ~7) << 1)&VIF_BUF0_BASE_ADRS_MASK;
      besr.vid_buf0_base_adrs += config->offset.y;
      besr.vid_buf1_base_adrs = besr.vid_buf0_base_adrs;
      besr.vid_buf2_base_adrs = besr.vid_buf0_base_adrs;
    }
    config->offsets[0] = 0;
    config->offsets[1] = config->frame_size;
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
    besr.y_x_start = (config->dest.x+X_ADJUST) | (config->dest.y << 16);
    besr.y_x_end = (config->dest.x + dest_w+X_ADJUST) | ((config->dest.y + dest_h) << 16);
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

static void radeon_compute_framesize(vidix_playback_t *info)
{
  unsigned pitch,awidth;
  pitch = radeon_query_pitch(info->fourcc);
  awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
  switch(info->fourcc)
  {
    case IMGFMT_I420:
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
		info->frame_size = awidth*info->src.h+(awidth*info->src.h)/2;
		break;
    default:	info->frame_size = awidth*info->src.h*2;
		break;
  }
}

int vixConfigPlayback(vidix_playback_t *info)
{
  if(!is_supported_fourcc(info->fourcc)) return ENOSYS;
  if(info->num_frames>2) info->num_frames=2;
  radeon_compute_framesize(info);
  radeon_overlay_off = radeon_ram_size - info->frame_size*info->num_frames;
  radeon_overlay_off &= 0xffff0000;
  if(radeon_overlay_off < 0) return EINVAL;
  info->dga_addr = (char *)radeon_mem_base + radeon_overlay_off;
  radeon_vid_init_video(info);
  return 0;
}

int vixPlaybackOn( void )
{
  radeon_vid_display_video();
  return 0;
}

int vixPlaybackOff( void )
{
  radeon_vid_stop_video();
  return 0;
}

int vixPlaybackFrameSelect(unsigned frame)
{
    uint32_t off0,off1,off2;
/*    if(!besr.double_buff) return; */
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
    if(__verbose > 1) radeon_vid_dump_regs();
    return 0;
}

vidix_video_eq_t equal = { 0, 0, 0, 0, 0, 0, 0, 0 };

int 	vixPlaybackGetEq( vidix_video_eq_t * eq)
{
  memcpy(eq,&equal,sizeof(vidix_video_eq_t));
  return 0;
}

int 	vixPlaybackSetEq( const vidix_video_eq_t * eq)
{
#ifdef RAGE128
  int br,sat;
#endif
    memcpy(&equal,eq,sizeof(vidix_video_eq_t));
#ifdef RAGE128
    br = equal.brightness * 64 / 1000;
    sat = equal.saturation * 32 / 1000;
    if(sat < 0) sat = 0;
    OUTREG(OV0_COLOUR_CNTL, (br & 0x7f) | (sat << 8) | (sat << 16));
#else
  radeon_set_transform(equal.brightness,equal.contrast,
		       equal.saturation,equal.hue,0);
#endif
  return 0;
}

