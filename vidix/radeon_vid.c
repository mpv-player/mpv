/*
   radeon_vid - VIDIX based video driver for Radeon and Rage128 chips
   Copyrights 2002 Nick Kurshev. This file is based on sources from
   GATOS (gatos.sf.net) and X11 (www.xfree86.org)
   Licence: GPL

   31.12.2002 added support for fglrx drivers by Marcel Naziri (zwobbl@zwobbl.de)
   6.04.2004 fixes to allow compiling vidix without X11 (broken in original patch)
   PPC support by Alex Beregszaszi
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "../config.h"
#include "../libavutil/common.h"
#include "../mpbswap.h"
#include "../libdha/pci_ids.h"
#include "../libdha/pci_names.h"
#include "vidix.h"
#include "vidixlib.h"
#include "fourcc.h"
#include "../libdha/libdha.h"
#include "radeon.h"

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

#ifdef RAGE128
#define RADEON_MSG "[rage128]"
#define X_ADJUST 0
#else
#define RADEON_MSG "[radeon]"
#define X_ADJUST (is_shift_required ? 8 : 0)
#ifndef RADEON
#define RADEON
#endif
#endif

static int __verbose = 0;
#ifdef RADEON
static int is_shift_required = 0;
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
  uint32_t vid_buf_base_adrs_y[VID_PLAY_MAXFRAMES];
  uint32_t vid_buf_base_adrs_u[VID_PLAY_MAXFRAMES];
  uint32_t vid_buf_base_adrs_v[VID_PLAY_MAXFRAMES];
  uint32_t vid_nbufs;

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
  uint32_t ckey_cntl;
  
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
static int RadeonFamily=100;
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
  DECLARE_VREG(IDCT_CONTROL),
  DECLARE_VREG(CONFIG_CNTL)
};

#ifdef HAVE_X11
static uint32_t firegl_shift = 0;
#endif
static void * radeon_mmio_base = 0;
static void * radeon_mem_base = 0; 
static int32_t radeon_overlay_off = 0;
static uint32_t radeon_ram_size = 0;
/* Restore on exit */
static uint32_t SAVED_OV0_GRAPHICS_KEY_CLR = 0;
static uint32_t SAVED_OV0_GRAPHICS_KEY_MSK = 0;
static uint32_t SAVED_OV0_VID_KEY_CLR = 0;
static uint32_t SAVED_OV0_VID_KEY_MSK = 0;
static uint32_t SAVED_OV0_KEY_CNTL = 0;
#ifdef WORDS_BIGENDIAN
static uint32_t SAVED_CONFIG_CNTL = 0;
#if defined(RAGE128)
#define APER_0_BIG_ENDIAN_16BPP_SWAP (1<<0)
#define APER_0_BIG_ENDIAN_32BPP_SWAP (2<<0)
#else
#define RADEON_SURFACE_CNTL                 0x0b00
#define RADEON_NONSURF_AP0_SWP_16BPP (1 << 20)
#define RADEON_NONSURF_AP0_SWP_32BPP (1 << 21)
#endif
#endif

#define GETREG(TYPE,PTR,OFFZ)		(*((volatile TYPE*)((PTR)+(OFFZ))))
#define SETREG(TYPE,PTR,OFFZ,VAL)	(*((volatile TYPE*)((PTR)+(OFFZ))))=VAL

#define INREG8(addr)		GETREG(uint8_t,(uint8_t*)(radeon_mmio_base),addr)
#define OUTREG8(addr,val)	SETREG(uint8_t,(uint8_t*)(radeon_mmio_base),addr,val)

static inline uint32_t INREG (uint32_t addr) {
	uint32_t tmp = GETREG(uint32_t,(uint8_t*)(radeon_mmio_base),addr);
	return le2me_32(tmp);
}
//#define OUTREG(addr,val)	SETREG(uint32_t,(uint8_t*)(radeon_mmio_base),addr,val)
#define OUTREG(addr,val)	SETREG(uint32_t,(uint8_t*)(radeon_mmio_base),addr,le2me_32(val))
#define OUTREGP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INREG(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTREG(addr, _tmp);					\
	} while (0)

static __inline__ uint32_t INPLL(uint32_t addr)
{
	OUTREG8(CLOCK_CNTL_INDEX, addr & 0x0000001f);
	return (INREG(CLOCK_CNTL_DATA));
}

#define OUTPLL(addr,val)	OUTREG8(CLOCK_CNTL_INDEX, (addr & 0x0000001f) | 0x00000080); \
				OUTREG(CLOCK_CNTL_DATA, val)
#define OUTPLLP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INPLL(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTPLL(addr, _tmp);					\
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

static uint32_t radeon_get_xres( void )
{
  /* FIXME: currently we extract that from CRTC!!!*/
  uint32_t xres,h_total;
  h_total = INREG(CRTC_H_TOTAL_DISP);
  xres = (h_total >> 16) & 0xffff;
  return (xres + 1)*8;
}

static uint32_t radeon_get_yres( void )
{
  /* FIXME: currently we extract that from CRTC!!!*/
  uint32_t yres,v_total;
  v_total = INREG(CRTC_V_TOTAL_DISP);
  yres = (v_total >> 16) & 0xffff;
  return yres + 1;
}

/* get flat panel x resolution*/
static uint32_t radeon_get_fp_xres( void ){
  uint32_t xres=(INREG(FP_HORZ_STRETCH)&0x00fff000)>>16;
  xres=(xres+1)*8;
  return xres;
}

/* get flat panel y resolution*/
static uint32_t radeon_get_fp_yres( void ){
  uint32_t yres=(INREG(FP_VERT_STRETCH)&0x00fff000)>>12;
  return yres+1;
}

static void radeon_wait_vsync(void)
{
    int i;

    OUTREG(GEN_INT_STATUS, VSYNC_INT_AK);
    for (i = 0; i < 2000000; i++) 
    {
	if (INREG(GEN_INT_STATUS) & VSYNC_INT) break;
    }
}

#ifdef RAGE128
static void _radeon_engine_idle(void);
static void _radeon_fifo_wait(unsigned);
#define radeon_engine_idle()		_radeon_engine_idle()
#define radeon_fifo_wait(entries)	_radeon_fifo_wait(entries)
/* Flush all dirty data in the Pixel Cache to memory. */
static __inline__ void radeon_engine_flush ( void )
{
    unsigned i;

    OUTREGP(PC_NGUI_CTLSTAT, PC_FLUSH_ALL, ~PC_FLUSH_ALL);
    for (i = 0; i < 2000000; i++) {
	if (!(INREG(PC_NGUI_CTLSTAT) & PC_BUSY)) break;
    }
}

/* Reset graphics card to known state. */
static void radeon_engine_reset( void )
{
    uint32_t clock_cntl_index;
    uint32_t mclk_cntl;
    uint32_t gen_reset_cntl;

    radeon_engine_flush();

    clock_cntl_index = INREG(CLOCK_CNTL_INDEX);
    mclk_cntl        = INPLL(MCLK_CNTL);

    OUTPLL(MCLK_CNTL, mclk_cntl | FORCE_GCP | FORCE_PIPE3D_CP);

    gen_reset_cntl   = INREG(GEN_RESET_CNTL);

    OUTREG(GEN_RESET_CNTL, gen_reset_cntl | SOFT_RESET_GUI);
    INREG(GEN_RESET_CNTL);
    OUTREG(GEN_RESET_CNTL,
	gen_reset_cntl & (uint32_t)(~SOFT_RESET_GUI));
    INREG(GEN_RESET_CNTL);

    OUTPLL(MCLK_CNTL,        mclk_cntl);
    OUTREG(CLOCK_CNTL_INDEX, clock_cntl_index);
    OUTREG(GEN_RESET_CNTL,   gen_reset_cntl);
}
#else

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

static void _radeon_engine_idle(void);
static void _radeon_fifo_wait(unsigned);
#define radeon_engine_idle()		_radeon_engine_idle()
#define radeon_fifo_wait(entries)	_radeon_fifo_wait(entries)

static void radeon_engine_reset( void )
{
	uint32_t clock_cntl_index, mclk_cntl, rbbm_soft_reset;

	radeon_engine_flush ();

	clock_cntl_index = INREG(CLOCK_CNTL_INDEX);
	mclk_cntl = INPLL(MCLK_CNTL);

	OUTPLL(MCLK_CNTL, (mclk_cntl |
			   FORCEON_MCLKA |
			   FORCEON_MCLKB |
			   FORCEON_YCLKA |
			   FORCEON_YCLKB |
			   FORCEON_MC |
			   FORCEON_AIC));
	rbbm_soft_reset = INREG(RBBM_SOFT_RESET);

	OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset |
				SOFT_RESET_CP |
				SOFT_RESET_HI |
				SOFT_RESET_SE |
				SOFT_RESET_RE |
				SOFT_RESET_PP |
				SOFT_RESET_E2 |
				SOFT_RESET_RB |
				SOFT_RESET_HDP);
	INREG(RBBM_SOFT_RESET);
	OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset & (uint32_t)
				~(SOFT_RESET_CP |
				  SOFT_RESET_HI |
				  SOFT_RESET_SE |
				  SOFT_RESET_RE |
				  SOFT_RESET_PP |
				  SOFT_RESET_E2 |
				  SOFT_RESET_RB |
				  SOFT_RESET_HDP));
	INREG(RBBM_SOFT_RESET);

	OUTPLL(MCLK_CNTL, mclk_cntl);
	OUTREG(CLOCK_CNTL_INDEX, clock_cntl_index);
	OUTREG(RBBM_SOFT_RESET, rbbm_soft_reset);

	return;
}
#endif
static void radeon_engine_restore( void )
{
#ifndef RAGE128
    int pitch64;
    uint32_t xres,yres,bpp;
    radeon_fifo_wait(1);
    xres = radeon_get_xres();
    yres = radeon_get_yres();
    bpp = radeon_vid_get_dbpp();
    /* turn of all automatic flushing - we'll do it all */
    OUTREG(RB2D_DSTCACHE_MODE, 0);

    pitch64 = ((xres * (bpp / 8) + 0x3f)) >> 6;

    radeon_fifo_wait(1);
    OUTREG(DEFAULT_OFFSET, (INREG(DEFAULT_OFFSET) & 0xC0000000) |
				  (pitch64 << 22));

    radeon_fifo_wait(1);
#if defined(WORDS_BIGENDIAN)
#ifdef RADEON
    OUTREGP(DP_DATATYPE, HOST_BIG_ENDIAN_EN, ~HOST_BIG_ENDIAN_EN);
#endif
#else
    OUTREGP(DP_DATATYPE, 0, ~HOST_BIG_ENDIAN_EN);
#endif

    radeon_fifo_wait(1);
    OUTREG(DEFAULT_SC_BOTTOM_RIGHT, (DEFAULT_SC_RIGHT_MAX
				    | DEFAULT_SC_BOTTOM_MAX));
    radeon_fifo_wait(1);
    OUTREG(DP_GUI_MASTER_CNTL, (INREG(DP_GUI_MASTER_CNTL)
				       | GMC_BRUSH_SOLID_COLOR
				       | GMC_SRC_DATATYPE_COLOR));

    radeon_fifo_wait(7);
    OUTREG(DST_LINE_START,    0);
    OUTREG(DST_LINE_END,      0);
    OUTREG(DP_BRUSH_FRGD_CLR, 0xffffffff);
    OUTREG(DP_BRUSH_BKGD_CLR, 0x00000000);
    OUTREG(DP_SRC_FRGD_CLR,   0xffffffff);
    OUTREG(DP_SRC_BKGD_CLR,   0x00000000);
    OUTREG(DP_WRITE_MASK,     0xffffffff);

    radeon_engine_idle();
#endif
}
#ifdef RAGE128
static void _radeon_fifo_wait (unsigned entries)
{
    unsigned i;

    for(;;)
    {
	for (i=0; i<2000000; i++)
		if ((INREG(GUI_STAT) & GUI_FIFOCNT_MASK) >= entries)
			return;
	radeon_engine_reset();
	radeon_engine_restore();
    }
}

static void _radeon_engine_idle ( void )
{
    unsigned i;

    /* ensure FIFO is empty before waiting for idle */
    radeon_fifo_wait (64);
    for(;;)
    {
	for (i=0; i<2000000; i++) {
		if ((INREG(GUI_STAT) & GUI_ACTIVE) == 0) {
			radeon_engine_flush ();
			return;
		}
	}
	radeon_engine_reset();
	radeon_engine_restore();
    }
}
#else
static void _radeon_fifo_wait (unsigned entries)
{
    unsigned i;

    for(;;)
    {
	for (i=0; i<2000000; i++)
		if ((INREG(RBBM_STATUS) & RBBM_FIFOCNT_MASK) >= entries)
			return;
	radeon_engine_reset();
	radeon_engine_restore();
    }
}
static void _radeon_engine_idle ( void )
{
    int i;

    /* ensure FIFO is empty before waiting for idle */
    radeon_fifo_wait (64);
    for(;;)
    {
	for (i=0; i<2000000; i++) {
		if (((INREG(RBBM_STATUS) & RBBM_ACTIVE)) == 0) {
			radeon_engine_flush ();
			return;
		}
	}
	radeon_engine_reset();
	radeon_engine_restore();
    }
}
#endif

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
 *            red_intensity - intense of red component                      *
 *            green_intensity - intense of green component                  *
 *            blue_intensity - intense of blue component                    *
 *            ref - index to the table of refernce transforms               *
 *   Outputs: NONE                                                          *
 ****************************************************************************/

static void radeon_set_transform(float bright, float cont, float sat,
				 float hue, float red_intensity,
				 float green_intensity,float blue_intensity,
				 unsigned ref)
{
	float OvHueSin, OvHueCos;
	float CAdjLuma, CAdjOff;
	float RedAdj,GreenAdj,BlueAdj;
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
	RedAdj = cont * trans[ref].RefLuma * red_intensity * 1023.0;
	GreenAdj = cont * trans[ref].RefLuma * green_intensity * 1023.0;
	BlueAdj = cont * trans[ref].RefLuma * blue_intensity * 1023.0;

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
	OvROff = RedAdj + CAdjOff -
		OvLuma * Loff - (OvRCb + OvRCr) * Coff;
	OvGOff = GreenAdj + CAdjOff - 
		OvLuma * Loff - (OvGCb + OvGCr) * Coff;
	OvBOff = BlueAdj + CAdjOff - 
		OvLuma * Loff - (OvBCb + OvBCr) * Coff;
#if 0 /* default constants */
	OvROff = -888.5;
	OvGOff = 545;
	OvBOff = -1104;
#endif 
   
	dwOvROff = ((int)(OvROff * 2.0)) & 0x1fff;
	dwOvGOff = (int)(OvGOff * 2.0) & 0x1fff;
	dwOvBOff = (int)(OvBOff * 2.0) & 0x1fff;
	/* Whatever docs say about R200 having 3.8 format instead of 3.11
	   as in Radeon is a lie */
#if 0
	if(RadeonFamily == 100)
	{
#endif
		dwOvLuma =(((int)(OvLuma * 2048.0))&0x7fff)<<17;
		dwOvRCb = (((int)(OvRCb * 2048.0))&0x7fff)<<1;
		dwOvRCr = (((int)(OvRCr * 2048.0))&0x7fff)<<17;
		dwOvGCb = (((int)(OvGCb * 2048.0))&0x7fff)<<1;
		dwOvGCr = (((int)(OvGCr * 2048.0))&0x7fff)<<17;
		dwOvBCb = (((int)(OvBCb * 2048.0))&0x7fff)<<1;
		dwOvBCr = (((int)(OvBCr * 2048.0))&0x7fff)<<17;
#if 0
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
#endif
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
    if(RadeonFamily == 100) {
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
  OUTREG(OV0_COLOUR_CNTL,0x00101000UL); /* Default brightness and saturation for Rage128 */
#else
  make_default_gamma_correction();
#endif
  besr.deinterlace_pattern = 0x900AAAAA;
  OUTREG(OV0_DEINTERLACE_PATTERN,besr.deinterlace_pattern);
  besr.deinterlace_on=1;
  besr.double_buff=1;
  besr.ckey_on=0;
  besr.graphics_key_msk=0;
  besr.graphics_key_clr=0;
  besr.ckey_cntl = VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND;
}

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
 DEVICE_ATI_RAGE_128_SH,
 DEVICE_ATI_RAGE_128_SK_4X,
 DEVICE_ATI_RAGE_128_SL_4X,
 DEVICE_ATI_RAGE_128_SM_4X,
 DEVICE_ATI_RAGE_128_4X,
 DEVICE_ATI_RAGE_128_PRO,
 DEVICE_ATI_RAGE_128_PRO2,
 DEVICE_ATI_RAGE_128_PRO3,
/* these seem to be based on rage 128 instead of mach64 */
 DEVICE_ATI_RAGE_MOBILITY_M3,
 DEVICE_ATI_RAGE_MOBILITY_M32
#else
/* Radeons (indeed: Rage 256 Pro ;) */
 DEVICE_ATI_RADEON_R100_QD,
 DEVICE_ATI_RADEON_R100_QE,
 DEVICE_ATI_RADEON_R100_QF,
 DEVICE_ATI_RADEON_R100_QG,
 DEVICE_ATI_RADEON_RV100_QY,
 DEVICE_ATI_RADEON_RV100_QZ,
 DEVICE_ATI_RADEON_MOBILITY_M7,
 DEVICE_ATI_RADEON_RV200_LX,
 DEVICE_ATI_RADEON_MOBILITY_M6,
 DEVICE_ATI_RADEON_MOBILITY_M62,
 DEVICE_ATI_RADEON_MOBILITY_U1,
 DEVICE_ATI_R200_BB_RADEON,
 DEVICE_ATI_RADEON_R200_QH,
 DEVICE_ATI_RADEON_R200_QI,
 DEVICE_ATI_RADEON_R200_QJ,
 DEVICE_ATI_RADEON_R200_QK,
 DEVICE_ATI_RADEON_R200_QL,
 DEVICE_ATI_RADEON_R200_QM,
 DEVICE_ATI_RADEON_R200_QH2,
 DEVICE_ATI_RADEON_R200_QI2,
 DEVICE_ATI_RADEON_R200_QJ2,
 DEVICE_ATI_RADEON_R200_QK2,
 DEVICE_ATI_RADEON_RV200_QW,
 DEVICE_ATI_RADEON_RV200_QX,
 DEVICE_ATI_RADEON_RV250_ID,
 DEVICE_ATI_RADEON_RV250_IE,
 DEVICE_ATI_RADEON_RV250_IF,
 DEVICE_ATI_RADEON_RV250_IG,
 DEVICE_ATI_RADEON_R250_LD,
 DEVICE_ATI_RADEON_R250_LE,
 DEVICE_ATI_RADEON_R250_MOBILITY,
 DEVICE_ATI_RADEON_R250_LG,
 DEVICE_ATI_RV370_5B60_RADEON,
 DEVICE_ATI_M9_5C61_RADEON,
 DEVICE_ATI_M9_5C63_RADEON,
 DEVICE_ATI_RV280_RADEON_9200,
 DEVICE_ATI_RV280_RADEON_92002,
 DEVICE_ATI_RV280_RADEON_92003,
 DEVICE_ATI_RV280_RADEON_92004,
 DEVICE_ATI_RV280_RADEON_92005,
 DEVICE_ATI_RV280_RADEON_92006,
 DEVICE_ATI_RADEON_R300_ND,
 DEVICE_ATI_RADEON_R300_NE,
 DEVICE_ATI_RV350_NF_RADEON,
 DEVICE_ATI_RADEON_R300_NG,
 DEVICE_ATI_R300_AE_RADEON,
 DEVICE_ATI_R300_AF_RADEON,
 DEVICE_ATI_RV350_AP_RADEON,
 DEVICE_ATI_RV350_AQ_RADEON,
 DEVICE_ATI_RV350_AR_RADEON,
 DEVICE_ATI_RV350_AS_RADEON,
 DEVICE_ATI_R350_AH_RADEON,
 DEVICE_ATI_R350_AI_RADEON,
 DEVICE_ATI_RADEON_R350_RADEON2,
 DEVICE_ATI_RV350_NJ_RADEON,
 DEVICE_ATI_RV350_MOBILITY_RADEON,
 DEVICE_ATI_RV350_MOBILITY_RADEON2
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

static pciinfo_t pci_info;
static int probed=0;

static vidix_capability_t def_cap = 
{
#ifdef RAGE128
    "BES driver for Rage128 cards",
#else
    "BES driver for Radeon cards",
#endif
    "Nick Kurshev",
    TYPE_OUTPUT | TYPE_FX,
    { 0, 0, 0, 0 },
    2048,
    2048,
    4,
    4,
    -1,
    FLAG_UPSCALER | FLAG_DOWNSCALER | FLAG_EQUALIZER,
    VENDOR_ATI,
    0,
    { 0, 0, 0, 0}
};

#ifndef RAGE128
#ifdef HAVE_X11
static void probe_fireGL_driver(void) {
  Display *dp = XOpenDisplay ((void*)0);
  int n = 0;
  char **extlist;
  if (dp==NULL) {
       return;
  }
  extlist = XListExtensions (dp, &n);
  XCloseDisplay (dp);
  if (extlist) {
    int i;
    int ext_fgl = 0, ext_fglrx = 0;
    for (i = 0; i < n; i++) {
      if (!strcmp(extlist[i], "ATIFGLEXTENSION")) ext_fgl = 1;
      if (!strcmp(extlist[i], "ATIFGLRXDRI")) ext_fglrx = 1;
    }
    if (ext_fgl) {
      printf(RADEON_MSG" ATI FireGl driver detected");
      firegl_shift = 0x500000;
      if (!ext_fglrx) {
        printf(", but DRI seems not to be activated\n");
        printf(RADEON_MSG" Output may not work correctly, check your DRI configuration!");
      }
      printf("\n");
    }
  }
}
#endif
#endif

static int radeon_probe( int verbose,int force )
{
  pciinfo_t lst[MAX_PCI_DEVICES];
  unsigned i,num_pci;
  int err;
  __verbose = verbose;
  err = pci_scan(lst,&num_pci);
  if(err)
  {
    printf(RADEON_MSG" Error occurred during pci scan: %s\n",strerror(err));
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
#if 0
	if ((lst[i].command & PCI_COMMAND_IO) == 0)
	{
		printf("[radeon] Device is disabled, ignoring\n");
		continue;
	}
#endif
#ifndef RAGE128	
	if(idx != -1)
#ifdef HAVE_X11
	probe_fireGL_driver();
#endif
	{
          switch(ati_card_ids[idx]) {
            /* Original radeon */
            case DEVICE_ATI_RADEON_R100_QD:
            case DEVICE_ATI_RADEON_R100_QE:
            case DEVICE_ATI_RADEON_R100_QF:
            case DEVICE_ATI_RADEON_R100_QG:
              RadeonFamily = 100;
              break;
              
            /* Radeon VE / Radeon Mobility */
            case DEVICE_ATI_RADEON_RV100_QY:
            case DEVICE_ATI_RADEON_RV100_QZ:
            case DEVICE_ATI_RADEON_MOBILITY_M6:
            case DEVICE_ATI_RADEON_MOBILITY_M62:
	    case DEVICE_ATI_RADEON_MOBILITY_U1:
              RadeonFamily = 120;
              break;
              
            /* Radeon 7500 / Radeon Mobility 7500 */
            case DEVICE_ATI_RADEON_RV200_QW:
            case DEVICE_ATI_RADEON_RV200_QX: 
            case DEVICE_ATI_RADEON_MOBILITY_M7:
            case DEVICE_ATI_RADEON_RV200_LX:
              RadeonFamily = 150;
              break;
              
            /* Radeon 8500 */
            case DEVICE_ATI_R200_BB_RADEON:
            case DEVICE_ATI_RADEON_R200_QH:
            case DEVICE_ATI_RADEON_R200_QI:
            case DEVICE_ATI_RADEON_R200_QJ:
            case DEVICE_ATI_RADEON_R200_QK:
            case DEVICE_ATI_RADEON_R200_QL:
            case DEVICE_ATI_RADEON_R200_QM:
            case DEVICE_ATI_RADEON_R200_QH2:
            case DEVICE_ATI_RADEON_R200_QI2:
            case DEVICE_ATI_RADEON_R200_QJ2:
            case DEVICE_ATI_RADEON_R200_QK2:
              RadeonFamily = 200;
              break;
              
            /* Radeon 9000 */
            case DEVICE_ATI_RADEON_RV250_ID:
            case DEVICE_ATI_RADEON_RV250_IE:
            case DEVICE_ATI_RADEON_RV250_IF:
            case DEVICE_ATI_RADEON_RV250_IG:
            case DEVICE_ATI_RADEON_R250_LD:
            case DEVICE_ATI_RADEON_R250_LE:
            case DEVICE_ATI_RADEON_R250_MOBILITY:
            case DEVICE_ATI_RADEON_R250_LG:
            case DEVICE_ATI_M9_5C61_RADEON:
            case DEVICE_ATI_M9_5C63_RADEON:
              RadeonFamily = 250;
              break;
              
            /* Radeon 9200 */
            case DEVICE_ATI_RV280_RADEON_9200:
            case DEVICE_ATI_RV280_RADEON_92002:
            case DEVICE_ATI_RV280_RADEON_92003:
            case DEVICE_ATI_RV280_RADEON_92004:
            case DEVICE_ATI_RV280_RADEON_92005:
            case DEVICE_ATI_RV280_RADEON_92006:
              RadeonFamily = 280;
              break;

            /* Radeon 9700 */
            case DEVICE_ATI_RADEON_R300_ND:
            case DEVICE_ATI_RADEON_R300_NE:
            case DEVICE_ATI_RV350_NF_RADEON:
            case DEVICE_ATI_RADEON_R300_NG:
            case DEVICE_ATI_R300_AE_RADEON:
            case DEVICE_ATI_R300_AF_RADEON:
              RadeonFamily = 300;
              break;

            /* Radeon 9600/9800 */
            case DEVICE_ATI_RV370_5B60_RADEON:
            case DEVICE_ATI_RV350_AP_RADEON:
            case DEVICE_ATI_RV350_AQ_RADEON:
            case DEVICE_ATI_RV350_AR_RADEON:
            case DEVICE_ATI_RV350_AS_RADEON:
            case DEVICE_ATI_RADEON_R350_RADEON2:
            case DEVICE_ATI_R350_AH_RADEON:
            case DEVICE_ATI_R350_AI_RADEON:
            case DEVICE_ATI_RV350_NJ_RADEON:
            case DEVICE_ATI_RV350_MOBILITY_RADEON:
            case DEVICE_ATI_RV350_MOBILITY_RADEON2:
              RadeonFamily = 350;
              break;

            default:
              break;
          }
	}
#endif
	if(force > PROBE_NORMAL)
	{
	    printf(RADEON_MSG" Driver was forced. Was found %sknown chip\n",idx == -1 ? "un" : "");
	    if(idx == -1)
#ifdef RAGE128
		printf(RADEON_MSG" Assuming it as Rage128\n");
#else
		printf(RADEON_MSG" Assuming it as Radeon1\n");
#endif
	}
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

static void radeon_vid_dump_regs( void ); /* forward declaration */

static int radeon_init( void )
{
  int err;
  if(!probed) 
  {
    printf(RADEON_MSG" Driver was not probed but is being initializing\n");
    return EINTR;
  }    
  if((radeon_mmio_base = map_phys_mem(pci_info.base2,0xFFFF))==(void *)-1) return ENOMEM;
  radeon_ram_size = INREG(CONFIG_MEMSIZE);
  /* mem size is bits [28:0], mask off the rest. Range: from 1Mb up to 512 Mb */
  radeon_ram_size &=  CONFIG_MEMSIZE_MASK;
#ifdef RADEON
  /* according to XFree86 4.2.0, some production M6's return 0 for 8MB */
  if (radeon_ram_size == 0 &&
      (def_cap.device_id == DEVICE_ATI_RADEON_MOBILITY_M6 ||
       def_cap.device_id == DEVICE_ATI_RADEON_MOBILITY_M62))
  {
      printf(RADEON_MSG" Workarounding buggy Radeon Mobility M6 (0 vs. 8MB ram)\n");
      radeon_ram_size = 8192*1024;
  }
#else
  /* Rage Mobility (rage128) also has memsize bug */
  if (radeon_ram_size == 0 &&
      (def_cap.device_id == DEVICE_ATI_RAGE_MOBILITY_M3 ||
       def_cap.device_id == DEVICE_ATI_RAGE_128_RL_VR ||
       def_cap.device_id == DEVICE_ATI_RAGE_MOBILITY_M32))
  {
      printf(RADEON_MSG" Workarounding buggy Rage Mobility M3 (0 vs. 8MB ram)\n");
      radeon_ram_size = 8192*1024;
  }
#endif
  printf(RADEON_MSG" Video memory = %uMb\n",radeon_ram_size/0x100000);
#ifdef WIN32
  //mapping large areas of video ram will fail on windows
  if(radeon_ram_size > 16*1024*1024)radeon_ram_size=16*1024*1024;
#endif
  if((radeon_mem_base = map_phys_mem(pci_info.base0,radeon_ram_size))==(void *)-1) return ENOMEM;
  memset(&besr,0,sizeof(bes_registers_t));
  radeon_vid_make_default();
  err = mtrr_set_type(pci_info.base0,radeon_ram_size,MTRR_TYPE_WRCOMB);
  if(!err) printf(RADEON_MSG" Set write-combining type of video memory\n");

  radeon_fifo_wait(3);
  SAVED_OV0_GRAPHICS_KEY_CLR = INREG(OV0_GRAPHICS_KEY_CLR);
  SAVED_OV0_GRAPHICS_KEY_MSK = INREG(OV0_GRAPHICS_KEY_MSK);
  SAVED_OV0_VID_KEY_CLR = INREG(OV0_VID_KEY_CLR);
  SAVED_OV0_VID_KEY_MSK = INREG(OV0_VID_KEY_MSK);
  SAVED_OV0_KEY_CNTL = INREG(OV0_KEY_CNTL);
  printf(RADEON_MSG" Saved overlay colorkey settings\n");

#ifdef RADEON
  switch(RadeonFamily)
    {
    case 100:
    case 120:
    case 150:
    case 250:
    case 280:
      is_shift_required=1;
      break;
    default:
      break;
    }
#endif

/* XXX: hack, but it works for me (tm) */
#ifdef WORDS_BIGENDIAN
#if defined(RAGE128) 
    /* code from gatos */
    {
	SAVED_CONFIG_CNTL = INREG(CONFIG_CNTL);
	OUTREG(CONFIG_CNTL, SAVED_CONFIG_CNTL &
	    ~(APER_0_BIG_ENDIAN_16BPP_SWAP|APER_0_BIG_ENDIAN_32BPP_SWAP));
	    
//	printf("saved: %x, current: %x\n", SAVED_CONFIG_CNTL,
//	    INREG(CONFIG_CNTL));
    }
#else
    /*code from radeon_video.c*/
    {
    	SAVED_CONFIG_CNTL = INREG(RADEON_SURFACE_CNTL);
/*	OUTREG(RADEON_SURFACE_CNTL, (SAVED_CONFIG_CNTL |
		RADEON_NONSURF_AP0_SWP_32BPP) & ~RADEON_NONSURF_AP0_SWP_16BPP);
*/
	OUTREG(RADEON_SURFACE_CNTL, SAVED_CONFIG_CNTL & ~(RADEON_NONSURF_AP0_SWP_32BPP
						   | RADEON_NONSURF_AP0_SWP_16BPP));

/*
	OUTREG(RADEON_SURFACE_CNTL, (SAVED_CONFIG_CNTL | RADEON_NONSURF_AP0_SWP_32BPP)
				    & ~RADEON_NONSURF_AP0_SWP_16BPP);
*/
    }
#endif
#endif

  if(__verbose > 1) radeon_vid_dump_regs();
  return 0;  
}

static void radeon_destroy( void )
{
  /* remove colorkeying */
  radeon_fifo_wait(3);
  OUTREG(OV0_GRAPHICS_KEY_CLR, SAVED_OV0_GRAPHICS_KEY_CLR);
  OUTREG(OV0_GRAPHICS_KEY_MSK, SAVED_OV0_GRAPHICS_KEY_MSK);
  OUTREG(OV0_VID_KEY_CLR, SAVED_OV0_VID_KEY_CLR);
  OUTREG(OV0_VID_KEY_MSK, SAVED_OV0_VID_KEY_MSK);
  OUTREG(OV0_KEY_CNTL, SAVED_OV0_KEY_CNTL);
  printf(RADEON_MSG" Restored overlay colorkey settings\n");

#ifdef WORDS_BIGENDIAN
#if defined(RAGE128)
    OUTREG(CONFIG_CNTL, SAVED_CONFIG_CNTL);
//    printf("saved: %x, restored: %x\n", SAVED_CONFIG_CNTL,
//	INREG(CONFIG_CNTL));
#else
    OUTREG(RADEON_SURFACE_CNTL, SAVED_CONFIG_CNTL);
#endif
#endif

  unmap_phys_mem(radeon_mem_base,radeon_ram_size);
  unmap_phys_mem(radeon_mmio_base,0xFFFF);
}

static int radeon_get_caps(vidix_capability_t *to)
{
  memcpy(to,&def_cap,sizeof(vidix_capability_t));
  return 0; 
}

/*
  Full list of fourcc which are supported by Win2K redeon driver:
  YUY2, UYVY, DDES, OGLT, OGL2, OGLS, OGLB, OGNT, OGNZ, OGNS,
  IF09, YVU9, IMC4, M2IA, IYUV, VBID, DXT1, DXT2, DXT3, DXT4, DXT5
*/
static uint32_t supported_fourcc[] = 
{
  IMGFMT_Y800, IMGFMT_Y8, IMGFMT_YVU9, IMGFMT_IF09,
  IMGFMT_YV12, IMGFMT_I420, IMGFMT_IYUV, 
  IMGFMT_UYVY, IMGFMT_YUY2, IMGFMT_YVYU,
  IMGFMT_RGB15, IMGFMT_BGR15,
  IMGFMT_RGB16, IMGFMT_BGR16,
  IMGFMT_RGB32, IMGFMT_BGR32
};

inline static int is_supported_fourcc(uint32_t fourcc)
{
  unsigned int i;
  for(i=0;i<sizeof(supported_fourcc)/sizeof(uint32_t);i++)
  {
    if(fourcc==supported_fourcc[i]) return 1;
  }
  return 0;
}

static int radeon_query_fourcc(vidix_fourcc_t *to)
{
    if(is_supported_fourcc(to->fourcc))
    {
	to->depth = VID_DEPTH_1BPP | VID_DEPTH_2BPP |
		    VID_DEPTH_4BPP | VID_DEPTH_8BPP |
		    VID_DEPTH_12BPP| VID_DEPTH_15BPP|
		    VID_DEPTH_16BPP| VID_DEPTH_24BPP|
		    VID_DEPTH_32BPP;
	to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK | VID_CAP_COLORKEY;
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
  printf(RADEON_MSG"video mode: %ux%u@%u\n",radeon_get_xres(),radeon_get_yres(),radeon_vid_get_dbpp());
  printf(RADEON_MSG"flatpanel size: %ux%u\n",radeon_get_fp_xres(),radeon_get_fp_yres());
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
#ifdef RADEON
    OUTREG(OV0_KEY_CNTL, GRAPHIC_KEY_FN_EQ);
#else
    OUTREG(OV0_KEY_CNTL, GRAPHIC_KEY_FN_NE);
#endif
    OUTREG(OV0_TEST, 0);
}

static void radeon_vid_display_video( void )
{
    int bes_flags;
    /** workaround for Xorg-6.8 not saving the surface registers on bigendian architectures */
#ifdef WORDS_BIGENDIAN
#if defined(RAGE128) 
    /* code from gatos */
    {
	SAVED_CONFIG_CNTL = INREG(CONFIG_CNTL);
	OUTREG(CONFIG_CNTL, SAVED_CONFIG_CNTL &
	    ~(APER_0_BIG_ENDIAN_16BPP_SWAP|APER_0_BIG_ENDIAN_32BPP_SWAP));
	    
//	printf("saved: %x, current: %x\n", SAVED_CONFIG_CNTL,
//	    INREG(CONFIG_CNTL));
    }
#else
    /*code from radeon_video.c*/
    {
    	SAVED_CONFIG_CNTL = INREG(RADEON_SURFACE_CNTL);
/*	OUTREG(RADEON_SURFACE_CNTL, (SAVED_CONFIG_CNTL |
		RADEON_NONSURF_AP0_SWP_32BPP) & ~RADEON_NONSURF_AP0_SWP_16BPP);
*/
	OUTREG(RADEON_SURFACE_CNTL, SAVED_CONFIG_CNTL & ~(RADEON_NONSURF_AP0_SWP_32BPP
						   | RADEON_NONSURF_AP0_SWP_16BPP));

/*
	OUTREG(RADEON_SURFACE_CNTL, (SAVED_CONFIG_CNTL | RADEON_NONSURF_AP0_SWP_32BPP)
				    & ~RADEON_NONSURF_AP0_SWP_16BPP);
*/
    }
#endif
#endif


 
    radeon_fifo_wait(2);
    OUTREG(OV0_REG_LOAD_CNTL,		REG_LD_CTL_LOCK);
    radeon_engine_idle();
    while(!(INREG(OV0_REG_LOAD_CNTL)&REG_LD_CTL_LOCK_READBACK));
    radeon_fifo_wait(15);

    /* Shutdown capturing */
    OUTREG(FCP_CNTL, FCP_CNTL__GND);
    OUTREG(CAP0_TRIG_CNTL, 0);

    OUTREG(VID_BUFFER_CONTROL, (1<<16) | 0x01);
    OUTREG(DISP_TEST_DEBUG_CNTL, 0);

    OUTREG(OV0_AUTO_FLIP_CNTL,OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD);

    if(besr.deinterlace_on) OUTREG(OV0_DEINTERLACE_PATTERN,besr.deinterlace_pattern);
#ifdef RAGE128
    OUTREG(OV0_COLOUR_CNTL, (((besr.brightness*64)/1000) & 0x7f) |
                            (((besr.saturation*31+31000)/2000) << 8) |
                            (((besr.saturation*31+31000)/2000) << 16));
#endif
    radeon_fifo_wait(2);
    OUTREG(OV0_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
    OUTREG(OV0_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
    OUTREG(OV0_KEY_CNTL,besr.ckey_cntl);

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
    OUTREG(OV0_VID_BUF0_BASE_ADRS,	besr.vid_buf_base_adrs_y[0]);
    OUTREG(OV0_VID_BUF1_BASE_ADRS,	besr.vid_buf_base_adrs_v[0]);
    OUTREG(OV0_VID_BUF2_BASE_ADRS,	besr.vid_buf_base_adrs_u[0]);
    radeon_fifo_wait(9);
    OUTREG(OV0_VID_BUF3_BASE_ADRS,	besr.vid_buf_base_adrs_y[0]);
    OUTREG(OV0_VID_BUF4_BASE_ADRS,	besr.vid_buf_base_adrs_v[0]);
    OUTREG(OV0_VID_BUF5_BASE_ADRS,	besr.vid_buf_base_adrs_u[0]);
    OUTREG(OV0_P1_V_ACCUM_INIT,		besr.p1_v_accum_init);
    OUTREG(OV0_P1_H_ACCUM_INIT,		besr.p1_h_accum_init);
    OUTREG(OV0_P23_H_ACCUM_INIT,	besr.p23_h_accum_init);
    OUTREG(OV0_P23_V_ACCUM_INIT,	besr.p23_v_accum_init);

#ifdef RADEON
    bes_flags = SCALER_ENABLE |
                SCALER_SMART_SWITCH;
//		SCALER_HORZ_PICK_NEAREST |
//		SCALER_VERT_PICK_NEAREST |
#endif
    bes_flags = SCALER_ENABLE |
                SCALER_SMART_SWITCH |
		SCALER_Y2R_TEMP |
		SCALER_PIX_EXPAND;
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
/*
        case IMGFMT_RGB24:
        case IMGFMT_BGR24: bes_flags |= SCALER_SOURCE_24BPP; break;
*/
        case IMGFMT_RGB32:
	case IMGFMT_BGR32: bes_flags |= SCALER_SOURCE_32BPP; break;
        /* 4:1:0 */
	case IMGFMT_IF09:
        case IMGFMT_YVU9:  bes_flags |= SCALER_SOURCE_YUV9; break;
	/* 4:0:0 */
	case IMGFMT_Y800:
	case IMGFMT_Y8:
        /* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:  bes_flags |= SCALER_SOURCE_YUV12; break;
        /* 4:2:2 */
        case IMGFMT_YVYU:
	case IMGFMT_UYVY:  bes_flags |= SCALER_SOURCE_YVYU422; break;
	case IMGFMT_YUY2:
	default:           bes_flags |= SCALER_SOURCE_VYUY422; break;
    }
    OUTREG(OV0_SCALE_CNTL,		bes_flags);
    OUTREG(OV0_REG_LOAD_CNTL,		0);
    if(__verbose > 1) printf(RADEON_MSG"we wanted: scaler=%08X\n",bes_flags);
    if(__verbose > 1) radeon_vid_dump_regs();
}

static unsigned radeon_query_pitch(unsigned fourcc,const vidix_yuv_t *spitch)
{
  unsigned pitch,spy,spv,spu;
  spy = spv = spu = 0;
  switch(spitch->y)
  {
    case 16:
    case 32:
    case 64:
    case 128:
    case 256: spy = spitch->y; break;
    default: break;
  }
  switch(spitch->u)
  {
    case 16:
    case 32:
    case 64:
    case 128:
    case 256: spu = spitch->u; break;
    default: break;
  }
  switch(spitch->v)
  {
    case 16:
    case 32:
    case 64:
    case 128:
    case 256: spv = spitch->v; break;
    default: break;
  }
  switch(fourcc)
  {
	/* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	case IMGFMT_I420:
		if(spy > 16 && spu == spy/2 && spv == spy/2)	pitch = spy;
		else						pitch = 32;
		break;
	/* 4:1:0 */
	case IMGFMT_IF09:
	case IMGFMT_YVU9:
		if(spy > 32 && spu == spy/4 && spv == spy/4)	pitch = spy;
		else						pitch = 64;
		break;
	default:
		if(spy >= 16)	pitch = spy;
		else		pitch = 16;
		break;
  }
  return pitch;
}

static int radeon_vid_init_video( vidix_playback_t *config )
{
    uint32_t i,tmp,src_w,src_h,dest_w,dest_h,pitch,h_inc,step_by,left,leftUV,top;
    int is_400,is_410,is_420,is_rgb32,is_rgb,best_pitch,mpitch;
    radeon_vid_stop_video();
    left = config->src.x << 16;
    top =  config->src.y << 16;
    src_h = config->src.h;
    src_w = config->src.w;
    is_400 = is_410 = is_420 = is_rgb32 = is_rgb = 0;
    if(config->fourcc == IMGFMT_YV12 ||
       config->fourcc == IMGFMT_I420 ||
       config->fourcc == IMGFMT_IYUV) is_420 = 1;
    if(config->fourcc == IMGFMT_YVU9 ||
       config->fourcc == IMGFMT_IF09) is_410 = 1;
    if(config->fourcc == IMGFMT_Y800 ||
       config->fourcc == IMGFMT_Y8) is_400 = 1;
    if(config->fourcc == IMGFMT_RGB32 ||
       config->fourcc == IMGFMT_BGR32) is_rgb32 = 1;
    if(config->fourcc == IMGFMT_RGB32 ||
       config->fourcc == IMGFMT_BGR32 ||
       config->fourcc == IMGFMT_RGB24 ||
       config->fourcc == IMGFMT_BGR24 ||
       config->fourcc == IMGFMT_RGB16 ||
       config->fourcc == IMGFMT_BGR16 ||
       config->fourcc == IMGFMT_RGB15 ||
       config->fourcc == IMGFMT_BGR15) is_rgb = 1;
    best_pitch = radeon_query_pitch(config->fourcc,&config->src.pitch);
    mpitch = best_pitch-1;
    switch(config->fourcc)
    {
	/* 4:0:0 */
	case IMGFMT_Y800:
	case IMGFMT_Y8:
	/* 4:1:0 */
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	/* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	case IMGFMT_I420: pitch = (src_w + mpitch) & ~mpitch;
			  config->dest.pitch.y = 
			  config->dest.pitch.u = 
			  config->dest.pitch.v = best_pitch;
			  break;
	/* RGB 4:4:4:4 */
	case IMGFMT_RGB32:
	case IMGFMT_BGR32: pitch = (src_w*4 + mpitch) & ~mpitch;
			  config->dest.pitch.y = 
			  config->dest.pitch.u = 
			  config->dest.pitch.v = best_pitch;
			  break;
	/* 4:2:2 */
        default: /* RGB15, RGB16, YVYU, UYVY, YUY2 */
			  pitch = ((src_w*2) + mpitch) & ~mpitch;
			  config->dest.pitch.y =
			  config->dest.pitch.u =
			  config->dest.pitch.v = best_pitch;
			  break;
    }
    dest_w = config->dest.w;
    dest_h = config->dest.h;
    if(radeon_is_dbl_scan()) dest_h *= 2;
    besr.dest_bpp = radeon_vid_get_dbpp();
    besr.fourcc = config->fourcc;

    /* flat panel */
    if(INREG(FP_VERT_STRETCH)&VERT_STRETCH_ENABLE){
      besr.v_inc = (src_h * radeon_get_yres() / radeon_get_fp_yres() << 20) / dest_h;
    }
    else besr.v_inc = (src_h << 20) / dest_h;
    if(radeon_is_interlace()) besr.v_inc *= 2;
    h_inc = (src_w << 12) / dest_w;

    {
        unsigned int ecp_div;
        ecp_div = (INPLL(VCLK_ECP_CNTL) >> 8) & 3;
        h_inc <<= ecp_div;
    }


    step_by = 1;
    while(h_inc >= (2 << 12)) {
	step_by++;
	h_inc >>= 1;
    }

    /* keep everything in 16.16 */
    besr.base_addr = INREG(DISPLAY_BASE_ADDR);
    config->offsets[0] = 0;
    for(i=1;i<besr.vid_nbufs;i++)
	    config->offsets[i] = config->offsets[i-1]+config->frame_size;
    if(is_420 || is_410 || is_400)
    {
        uint32_t d1line,d2line,d3line;
	d1line = top*pitch;
	if(is_420)
	{
	    d2line = src_h*pitch+(d1line>>2);
	    d3line = d2line+((src_h*pitch)>>2);
	}
	else
	if(is_410)
	{
	    d2line = src_h*pitch+(d1line>>4);
	    d3line = d2line+((src_h*pitch)>>4);
	}
	else
	{
	    d2line = 0;
	    d3line = 0;
	}
	d1line += (left >> 16) & ~15;
	if(is_420)
	{
	    d2line += (left >> 17) & ~15;
	    d3line += (left >> 17) & ~15;
	}
	else
	if(is_410)
	{
	    d2line += (left >> 18) & ~15;
	    d3line += (left >> 18) & ~15;
	}
	config->offset.y = d1line & VIF_BUF0_BASE_ADRS_MASK;
	if(is_400)
	{
	    config->offset.v = 0;
	    config->offset.u = 0;
	}
	else
	{
	    config->offset.v = d2line & VIF_BUF1_BASE_ADRS_MASK;
	    config->offset.u = d3line & VIF_BUF2_BASE_ADRS_MASK;
	}
	for(i=0;i<besr.vid_nbufs;i++)
	{
	    besr.vid_buf_base_adrs_y[i]=((radeon_overlay_off+config->offsets[i]+config->offset.y)&VIF_BUF0_BASE_ADRS_MASK);
	    if(is_400)
	    {
		besr.vid_buf_base_adrs_v[i]=0;
		besr.vid_buf_base_adrs_u[i]=0;
	    }
	    else
	    {
		if (besr.fourcc == IMGFMT_I420 || besr.fourcc == IMGFMT_IYUV)
		{
		    besr.vid_buf_base_adrs_u[i]=((radeon_overlay_off+config->offsets[i]+config->offset.v)&VIF_BUF1_BASE_ADRS_MASK)|VIF_BUF1_PITCH_SEL;
		    besr.vid_buf_base_adrs_v[i]=((radeon_overlay_off+config->offsets[i]+config->offset.u)&VIF_BUF2_BASE_ADRS_MASK)|VIF_BUF2_PITCH_SEL;
		}
		else
		{
		    besr.vid_buf_base_adrs_v[i]=((radeon_overlay_off+config->offsets[i]+config->offset.v)&VIF_BUF1_BASE_ADRS_MASK)|VIF_BUF1_PITCH_SEL;
		    besr.vid_buf_base_adrs_u[i]=((radeon_overlay_off+config->offsets[i]+config->offset.u)&VIF_BUF2_BASE_ADRS_MASK)|VIF_BUF2_PITCH_SEL;
		}
	    }
	}
	config->offset.y = ((besr.vid_buf_base_adrs_y[0])&VIF_BUF0_BASE_ADRS_MASK) - radeon_overlay_off;
	if(is_400)
	{
	    config->offset.v = 0;
	    config->offset.u = 0;
	}
	else
	{
	    config->offset.v = ((besr.vid_buf_base_adrs_v[0])&VIF_BUF1_BASE_ADRS_MASK) - radeon_overlay_off;
	    config->offset.u = ((besr.vid_buf_base_adrs_u[0])&VIF_BUF2_BASE_ADRS_MASK) - radeon_overlay_off;
	}
    }
    else
    {
      config->offset.y = config->offset.u = config->offset.v = ((left & ~7) << 1)&VIF_BUF0_BASE_ADRS_MASK;
      for(i=0;i<besr.vid_nbufs;i++)
      {
	besr.vid_buf_base_adrs_y[i] =
	besr.vid_buf_base_adrs_u[i] =
	besr.vid_buf_base_adrs_v[i] = radeon_overlay_off + config->offsets[i] + config->offset.y;
      }
    }

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
    besr.p23_v_accum_init = (is_420||is_410) ?
			    ((tmp << 4) & OV0_P23_V_ACCUM_INIT_MASK)
			    |(OV0_P23_MAX_LN_IN_PER_LN_OUT & 1) : 0;

    leftUV = (left >> (is_410?18:17)) & 15;
    left = (left >> 16) & 15;
    if(is_rgb && !is_rgb32) h_inc<<=1;
    if(is_rgb32)
	besr.h_inc = (h_inc >> 1) | ((h_inc >> 1) << 16);
    else
    if(is_410)
	besr.h_inc = h_inc | ((h_inc >> 2) << 16);
    else
	besr.h_inc = h_inc | ((h_inc >> 1) << 16);
    besr.step_by = step_by | (step_by << 8);
    besr.y_x_start = (config->dest.x+X_ADJUST) | (config->dest.y << 16);
    besr.y_x_end = (config->dest.x + dest_w+X_ADJUST) | ((config->dest.y + dest_h) << 16);
    besr.p1_blank_lines_at_top = P1_BLNK_LN_AT_TOP_M1_MASK|((src_h-1)<<16);
    if(is_420 || is_410)
    {
	src_h = (src_h + 1) >> (is_410?2:1);
	besr.p23_blank_lines_at_top = P23_BLNK_LN_AT_TOP_M1_MASK|((src_h-1)<<16);
    }
    else besr.p23_blank_lines_at_top = 0;
    besr.vid_buf_pitch0_value = pitch;
    besr.vid_buf_pitch1_value = is_410 ? pitch>>2 : is_420 ? pitch>>1 : pitch;
    besr.p1_x_start_end = (src_w+left-1)|(left<<16);
    if (is_410||is_420) src_w>>=is_410?2:1;
    if(is_400)
    {
	besr.p2_x_start_end = 0;
	besr.p3_x_start_end = 0;
    }
    else
    {
	besr.p2_x_start_end = (src_w+left-1)|(leftUV<<16);
	besr.p3_x_start_end = besr.p2_x_start_end;
    }

    return 0;
}

static void radeon_compute_framesize(vidix_playback_t *info)
{
  unsigned pitch,awidth,dbpp;
  pitch = radeon_query_pitch(info->fourcc,&info->src.pitch);
  dbpp = radeon_vid_get_dbpp();
  switch(info->fourcc)
  {
    case IMGFMT_I420:
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
		awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*(info->src.h+info->src.h/2);
		break;
    case IMGFMT_Y800:
    case IMGFMT_Y8:
		awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*info->src.h;
		break;
    case IMGFMT_IF09:
    case IMGFMT_YVU9:
		awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*(info->src.h+info->src.h/8);
		break;
    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
		awidth = (info->src.w*4 + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*info->src.h;
		break;
    /* YUY2 YVYU, RGB15, RGB16 */
    default:	
		awidth = (info->src.w*2 + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*info->src.h;
		break;
  }
}

static int radeon_config_playback(vidix_playback_t *info)
{
  unsigned rgb_size,nfr;
  if(!is_supported_fourcc(info->fourcc)) return ENOSYS;
  if(info->num_frames>VID_PLAY_MAXFRAMES) info->num_frames=VID_PLAY_MAXFRAMES;
  if(info->num_frames==1) besr.double_buff=0;
  else                    besr.double_buff=1;
  radeon_compute_framesize(info);
    
  rgb_size = radeon_get_xres()*radeon_get_yres()*((radeon_vid_get_dbpp()+7)/8);
  nfr = info->num_frames;
  for(;nfr>0; nfr--)
  {
      radeon_overlay_off = radeon_ram_size - info->frame_size*nfr;
#ifdef HAVE_X11
      radeon_overlay_off -= firegl_shift;
#endif
      radeon_overlay_off &= 0xffff0000;
      if(radeon_overlay_off >= (int)rgb_size ) break;
  }
  if(nfr <= 3)
  {
   nfr = info->num_frames;
   for(;nfr>0; nfr--)
   {
      radeon_overlay_off = radeon_ram_size - info->frame_size*nfr;
#ifdef HAVE_X11
      radeon_overlay_off -= firegl_shift;
#endif
      radeon_overlay_off &= 0xffff0000;
      if(radeon_overlay_off > 0) break;
   }
  }
  if(nfr <= 0) return EINVAL;
  info->num_frames = nfr;
  besr.vid_nbufs = info->num_frames;
  info->dga_addr = (char *)radeon_mem_base + radeon_overlay_off;  
  radeon_vid_init_video(info);
  return 0;
}

static int radeon_playback_on( void )
{
  radeon_vid_display_video();
  return 0;
}

static int radeon_playback_off( void )
{
  radeon_vid_stop_video();
  return 0;
}

static int radeon_frame_select(unsigned frame)
{
    uint32_t off[6];
    int prev_frame= (frame-1+besr.vid_nbufs) % besr.vid_nbufs;
    /*
    buf3-5 always should point onto second buffer for better
    deinterlacing and TV-in
    */
    if(!besr.double_buff) return 0;
    if(frame > besr.vid_nbufs) frame = besr.vid_nbufs-1;
    if(prev_frame > (int)besr.vid_nbufs) prev_frame = besr.vid_nbufs-1;
    off[0] = besr.vid_buf_base_adrs_y[frame];
    off[1] = besr.vid_buf_base_adrs_v[frame];
    off[2] = besr.vid_buf_base_adrs_u[frame];
    off[3] = besr.vid_buf_base_adrs_y[prev_frame];
    off[4] = besr.vid_buf_base_adrs_v[prev_frame];
    off[5] = besr.vid_buf_base_adrs_u[prev_frame];
    radeon_fifo_wait(8);
    OUTREG(OV0_REG_LOAD_CNTL,		REG_LD_CTL_LOCK);
    radeon_engine_idle();
    while(!(INREG(OV0_REG_LOAD_CNTL)&REG_LD_CTL_LOCK_READBACK));
    OUTREG(OV0_VID_BUF0_BASE_ADRS,	off[0]);
    OUTREG(OV0_VID_BUF1_BASE_ADRS,	off[1]);
    OUTREG(OV0_VID_BUF2_BASE_ADRS,	off[2]);
    OUTREG(OV0_VID_BUF3_BASE_ADRS,	off[3]);
    OUTREG(OV0_VID_BUF4_BASE_ADRS,	off[4]);
    OUTREG(OV0_VID_BUF5_BASE_ADRS,	off[5]);
    OUTREG(OV0_REG_LOAD_CNTL,		0);
    if(besr.vid_nbufs == 2) radeon_wait_vsync();
    if(__verbose > 1) radeon_vid_dump_regs();
    return 0;
}

static vidix_video_eq_t equal =
{
 VEQ_CAP_BRIGHTNESS | VEQ_CAP_SATURATION
#ifndef RAGE128
 | VEQ_CAP_CONTRAST | VEQ_CAP_HUE | VEQ_CAP_RGB_INTENSITY
#endif
 ,
 0, 0, 0, 0, 0, 0, 0, 0 };

static int radeon_get_eq( vidix_video_eq_t * eq)
{
  memcpy(eq,&equal,sizeof(vidix_video_eq_t));
  return 0;
}

#ifndef RAGE128
#define RTFSaturation(a)   (1.0 + ((a)*1.0)/1000.0)
#define RTFBrightness(a)   (((a)*1.0)/2000.0)
#define RTFIntensity(a)    (((a)*1.0)/2000.0)
#define RTFContrast(a)   (1.0 + ((a)*1.0)/1000.0)
#define RTFHue(a)   (((a)*3.1416)/1000.0)
#define RTFCheckParam(a) {if((a)<-1000) (a)=-1000; if((a)>1000) (a)=1000;}
#endif

static int radeon_set_eq( const vidix_video_eq_t * eq)
{
#ifdef RAGE128
  int br,sat;
#else
  int itu_space;
#endif
    if(eq->cap & VEQ_CAP_BRIGHTNESS) equal.brightness = eq->brightness;
    if(eq->cap & VEQ_CAP_CONTRAST)   equal.contrast   = eq->contrast;
    if(eq->cap & VEQ_CAP_SATURATION) equal.saturation = eq->saturation;
    if(eq->cap & VEQ_CAP_HUE)        equal.hue        = eq->hue;
    if(eq->cap & VEQ_CAP_RGB_INTENSITY)
    {
      equal.red_intensity   = eq->red_intensity;
      equal.green_intensity = eq->green_intensity;
      equal.blue_intensity  = eq->blue_intensity;
    }
    equal.flags = eq->flags;
#ifdef RAGE128
    br = equal.brightness * 64 / 1000;
    if(br < -64) br = -64; if(br > 63) br = 63;
    sat = (equal.saturation + 1000) * 16 / 1000;
    if(sat < 0) sat = 0; if(sat > 31) sat = 31;
    OUTREG(OV0_COLOUR_CNTL, (br & 0x7f) | (sat << 8) | (sat << 16));
#else
  itu_space = equal.flags == VEQ_FLG_ITU_R_BT_709 ? 1 : 0;
  RTFCheckParam(equal.brightness);
  RTFCheckParam(equal.saturation);
  RTFCheckParam(equal.contrast);
  RTFCheckParam(equal.hue);
  RTFCheckParam(equal.red_intensity);
  RTFCheckParam(equal.green_intensity);
  RTFCheckParam(equal.blue_intensity);
  radeon_set_transform(RTFBrightness(equal.brightness),
		       RTFContrast(equal.contrast),
		       RTFSaturation(equal.saturation),
		       RTFHue(equal.hue),
		       RTFIntensity(equal.red_intensity),
		       RTFIntensity(equal.green_intensity),
		       RTFIntensity(equal.blue_intensity),
		       itu_space);
#endif
  return 0;
}

static int radeon_playback_set_deint (const vidix_deinterlace_t * info)
{
  unsigned sflg;
  switch(info->flags)
  {
    default:
    case CFG_NON_INTERLACED:
			    besr.deinterlace_on = 0;
			    break;
    case CFG_EVEN_ODD_INTERLACING:
    case CFG_INTERLACED:
			    besr.deinterlace_on = 1;
			    besr.deinterlace_pattern = 0x900AAAAA;
			    break;
    case CFG_ODD_EVEN_INTERLACING:
			    besr.deinterlace_on = 1;
			    besr.deinterlace_pattern = 0x00055555;
			    break;
    case CFG_UNIQUE_INTERLACING:
			    besr.deinterlace_on = 1;
			    besr.deinterlace_pattern = info->deinterlace_pattern;
			    break;
  }
  OUTREG(OV0_REG_LOAD_CNTL,		REG_LD_CTL_LOCK);
  radeon_engine_idle();
  while(!(INREG(OV0_REG_LOAD_CNTL)&REG_LD_CTL_LOCK_READBACK));
  radeon_fifo_wait(15);
  sflg = INREG(OV0_SCALE_CNTL);
  if(besr.deinterlace_on)
  {
    OUTREG(OV0_SCALE_CNTL,sflg | SCALER_ADAPTIVE_DEINT);
    OUTREG(OV0_DEINTERLACE_PATTERN,besr.deinterlace_pattern);
  }
  else OUTREG(OV0_SCALE_CNTL,sflg & (~SCALER_ADAPTIVE_DEINT));
  OUTREG(OV0_REG_LOAD_CNTL,		0);
  return 0;  
}

static int radeon_playback_get_deint (vidix_deinterlace_t * info)
{
  if(!besr.deinterlace_on) info->flags = CFG_NON_INTERLACED;
  else
  {
    info->flags = CFG_UNIQUE_INTERLACING;
    info->deinterlace_pattern = besr.deinterlace_pattern;
  }
  return 0;
}


/* Graphic keys */
static vidix_grkey_t radeon_grkey;

static void set_gr_key( void )
{
    if(radeon_grkey.ckey.op == CKEY_TRUE)
    {
	int dbpp=radeon_vid_get_dbpp();
	besr.ckey_on=1;

	switch(dbpp)
	{
	case 15:
#ifdef RADEON
		if(RadeonFamily > 100)
			besr.graphics_key_clr=
				  ((radeon_grkey.ckey.blue &0xF8))
				| ((radeon_grkey.ckey.green&0xF8)<<8)
				| ((radeon_grkey.ckey.red  &0xF8)<<16);
		else
#endif
		besr.graphics_key_clr=
			  ((radeon_grkey.ckey.blue &0xF8)>>3)
			| ((radeon_grkey.ckey.green&0xF8)<<2)
			| ((radeon_grkey.ckey.red  &0xF8)<<7);
		break;
	case 16:
#ifdef RADEON
		/* This test may be too general/specific */
		if(RadeonFamily > 100)
			besr.graphics_key_clr=
				  ((radeon_grkey.ckey.blue &0xF8))
				| ((radeon_grkey.ckey.green&0xFC)<<8)
				| ((radeon_grkey.ckey.red  &0xF8)<<16);
		else
#endif
		besr.graphics_key_clr=
			  ((radeon_grkey.ckey.blue &0xF8)>>3)
			| ((radeon_grkey.ckey.green&0xFC)<<3)
			| ((radeon_grkey.ckey.red  &0xF8)<<8);
		break;
	case 24:
		besr.graphics_key_clr=
			  ((radeon_grkey.ckey.blue &0xFF))
			| ((radeon_grkey.ckey.green&0xFF)<<8)
			| ((radeon_grkey.ckey.red  &0xFF)<<16);
		break;
	case 32:
		besr.graphics_key_clr=
			  ((radeon_grkey.ckey.blue &0xFF))
			| ((radeon_grkey.ckey.green&0xFF)<<8)
			| ((radeon_grkey.ckey.red  &0xFF)<<16);
		break;
	default:
		besr.ckey_on=0;
		besr.graphics_key_msk=0;
		besr.graphics_key_clr=0;
	}
#ifdef RAGE128
	besr.graphics_key_msk=(1<<dbpp)-1;
	besr.ckey_cntl = VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_NE|CMP_MIX_AND;
#else
	besr.graphics_key_msk=besr.graphics_key_clr;
	besr.ckey_cntl = VIDEO_KEY_FN_TRUE|CMP_MIX_AND|GRAPHIC_KEY_FN_EQ;
#endif
    }
    else
    {
	besr.ckey_on=0;
	besr.graphics_key_msk=0;
	besr.graphics_key_clr=0;
	besr.ckey_cntl = VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND;
    }
    radeon_fifo_wait(3);
    OUTREG(OV0_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
    OUTREG(OV0_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
    OUTREG(OV0_KEY_CNTL,besr.ckey_cntl);
}

static int radeon_get_gkey(vidix_grkey_t *grkey)
{
    memcpy(grkey, &radeon_grkey, sizeof(vidix_grkey_t));
    return(0);
}

static int radeon_set_gkey(const vidix_grkey_t *grkey)
{
    memcpy(&radeon_grkey, grkey, sizeof(vidix_grkey_t));
    set_gr_key();
    return(0);
}

#ifdef RAGE128
VDXDriver rage128_drv = {
  "rage128",
#else
VDXDriver radeon_drv = {
  "radeon",
#endif
  NULL,
    
  .probe = radeon_probe,
  .get_caps = radeon_get_caps,
  .query_fourcc = radeon_query_fourcc,
  .init = radeon_init,
  .destroy = radeon_destroy,
  .config_playback = radeon_config_playback,
  .playback_on = radeon_playback_on,
  .playback_off = radeon_playback_off,
  .frame_sel = radeon_frame_select,
  .get_eq = radeon_get_eq,
  .set_eq = radeon_set_eq,
  .get_deint = radeon_playback_get_deint,
  .set_deint = radeon_playback_set_deint,
  .get_gkey = radeon_get_gkey,
  .set_gkey = radeon_set_gkey,
};
