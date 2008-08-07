/*
 * VIDIX driver for ATI Rage128 and Radeon chipsets.
 *
 * This file is based on sources from
 *   GATOS (gatos.sf.net) and X11 (www.xfree86.org)
 *
 * Copyright (C) 2002 Nick Kurshev
 * support for fglrx drivers by Marcel Naziri (zwobbl@zwobbl.de)
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "config.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "pci_ids.h"
#include "pci_names.h"
#include "vidix.h"
#include "fourcc.h"
#include "dha.h"
#include "radeon.h"

#if !defined(RAGE128) && defined(CONFIG_X11)
#include <X11/Xlib.h>
static uint32_t firegl_shift = 0;
#endif

#ifdef RAGE128
#define RADEON_MSG "[rage128]"
#define X_ADJUST 0
#else
#define RADEON_MSG "[radeon]"
#define X_ADJUST (((besr.chip_flags&R_OVL_SHIFT)==R_OVL_SHIFT)?8:0)
#ifndef RADEON
#define RADEON
#endif
#endif

#define RADEON_ASSERT(msg) printf(RADEON_MSG"################# FATAL:"msg);

#define VERBOSE_LEVEL 0
static int verbosity = 0;
typedef struct bes_registers_s
{
  /* base address of yuv framebuffer */
  uint32_t yuv_base;
  uint32_t fourcc;
  uint32_t surf_id;
  int load_prg_start;
  int horz_pick_nearest;
  int vert_pick_nearest;
  int swap_uv; /* for direct support of bgr fourccs */
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
  uint32_t four_tap_coeff[5];
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
  uint32_t merge_cntl;
  
  int deinterlace_on;
  uint32_t deinterlace_pattern;
  
  unsigned chip_flags;
} bes_registers_t;

typedef struct video_registers_s
{
  const char * sname;
  uint32_t name;
  uint32_t value;
}video_registers_t;

static bes_registers_t besr;
#define DECLARE_VREG(name) { #name, name, 0 }
static const video_registers_t vregs[] = 
{
  DECLARE_VREG(VIDEOMUX_CNTL),
  DECLARE_VREG(VIPPAD_MASK),
  DECLARE_VREG(VIPPAD1_A),
  DECLARE_VREG(VIPPAD1_EN),
  DECLARE_VREG(VIPPAD1_Y),
  DECLARE_VREG(OV0_Y_X_START),
  DECLARE_VREG(OV0_Y_X_END),
  DECLARE_VREG(OV1_Y_X_START),
  DECLARE_VREG(OV1_Y_X_END),
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
#ifdef RAGE128
  DECLARE_VREG(BM_FRAME_BUF_OFFSET),
  DECLARE_VREG(BM_SYSTEM_MEM_ADDR),
  DECLARE_VREG(BM_COMMAND),
  DECLARE_VREG(BM_STATUS),
  DECLARE_VREG(BM_QUEUE_STATUS),
  DECLARE_VREG(BM_QUEUE_FREE_STATUS),
  DECLARE_VREG(BM_CHUNK_0_VAL),
  DECLARE_VREG(BM_CHUNK_1_VAL),
  DECLARE_VREG(BM_VIP0_BUF),
  DECLARE_VREG(BM_VIP0_ACTIVE),
  DECLARE_VREG(BM_VIP1_BUF),
  DECLARE_VREG(BM_VIP1_ACTIVE),
  DECLARE_VREG(BM_VIP2_BUF),
  DECLARE_VREG(BM_VIP2_ACTIVE),
  DECLARE_VREG(BM_VIP3_BUF),
  DECLARE_VREG(BM_VIP3_ACTIVE),
  DECLARE_VREG(BM_VIDCAP_BUF0),
  DECLARE_VREG(BM_VIDCAP_BUF1),
  DECLARE_VREG(BM_VIDCAP_BUF2),
  DECLARE_VREG(BM_VIDCAP_ACTIVE),
  DECLARE_VREG(BM_GUI),
  DECLARE_VREG(BM_ABORT)
#else
  DECLARE_VREG(DISP_MERGE_CNTL),
  DECLARE_VREG(DMA_GUI_TABLE_ADDR),
  DECLARE_VREG(DMA_GUI_SRC_ADDR),
  DECLARE_VREG(DMA_GUI_DST_ADDR),
  DECLARE_VREG(DMA_GUI_COMMAND),
  DECLARE_VREG(DMA_GUI_STATUS),
  DECLARE_VREG(DMA_GUI_ACT_DSCRPTR),
  DECLARE_VREG(DMA_VID_SRC_ADDR),
  DECLARE_VREG(DMA_VID_DST_ADDR),
  DECLARE_VREG(DMA_VID_COMMAND),
  DECLARE_VREG(DMA_VID_STATUS),
  DECLARE_VREG(DMA_VID_ACT_DSCRPTR),
#endif
};

#define R_FAMILY	0x000000FF
#define R_100		0x00000001
#define R_120		0x00000002
#define R_150		0x00000004
#define R_200		0x00000008
#define R_250		0x00000010
#define R_280		0x00000020
#define R_300		0x00000040
#define R_350		0x00000080
#define R_370		0x00000100
#define R_380		0x00000200
#define R_420		0x00000400
#define R_430		0x00000800
#define R_480		0x00001000
#define R_OVL_SHIFT	0x01000000
#define R_INTEGRATED	0x02000000
#define R_PCIE		0x04000000

typedef struct ati_card_ids_s
{
    unsigned short id;
    unsigned flags;
}ati_card_ids_t;

static const ati_card_ids_t ati_card_ids[] = 
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
 { DEVICE_ATI_RAGE_128_PA_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PB_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PC_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PD_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PE_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PF_PRO, 0 },
/* Rage128 Pro VR */
 { DEVICE_ATI_RAGE_128_PG_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PH_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PI_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PJ_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PK_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PL_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PM_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PN_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PO_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PP_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PQ_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PR_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PS_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PT_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PU_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PV_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PW_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PX_PRO, 0 },
/* Rage128 GL */
 { DEVICE_ATI_RAGE_128_RE_SG, 0 },
 { DEVICE_ATI_RAGE_128_RF_SG, 0 },
 { DEVICE_ATI_RAGE_128_RG, 0 },
 { DEVICE_ATI_RAGE_128_RK_VR, 0 },
 { DEVICE_ATI_RAGE_128_RL_VR, 0 },
 { DEVICE_ATI_RAGE_128_SE_4X, 0 },
 { DEVICE_ATI_RAGE_128_SF_4X, 0 },
 { DEVICE_ATI_RAGE_128_SG_4X, 0 },
 { DEVICE_ATI_RAGE_128_SH, 0 },
 { DEVICE_ATI_RAGE_128_SK_4X, 0 },
 { DEVICE_ATI_RAGE_128_SL_4X, 0 },
 { DEVICE_ATI_RAGE_128_SM_4X, 0 },
 { DEVICE_ATI_RAGE_128_4X, 0 },
 { DEVICE_ATI_RAGE_128_PRO, 0 },
 { DEVICE_ATI_RAGE_128_PRO2, 0 },
 { DEVICE_ATI_RAGE_128_PRO3, 0 },
/* these seem to be based on rage 128 instead of mach64 */
 { DEVICE_ATI_RAGE_MOBILITY_M3, 0 },
 { DEVICE_ATI_RAGE_MOBILITY_M32, 0 },
#else
/* Radeon1 (indeed: Rage 256 Pro ;) */
 { DEVICE_ATI_RADEON_R100_QD,		R_100|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_R100_QE,		R_100|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_R100_QF,		R_100|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_R100_QG,		R_100|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_IGP_320,		R_150|R_OVL_SHIFT|R_INTEGRATED },
 { DEVICE_ATI_RADEON_MOBILITY_U1,	R_150|R_OVL_SHIFT|R_INTEGRATED },
 { DEVICE_ATI_RADEON_RV100_QY,		R_120|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_RV100_QZ,		R_120|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_MOBILITY_M7,	R_150|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_RV200_LX,		R_150|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_MOBILITY_M6,	R_120|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_MOBILITY_M62,	R_120|R_OVL_SHIFT },
/* Radeon2 (indeed: Rage 512 Pro ;) */
 { DEVICE_ATI_R200_BB_RADEON,		R_200 },
 { DEVICE_ATI_R200_BC_RADEON,		R_200 },
 { DEVICE_ATI_RADEON_R200_QH,		R_200 },
 { DEVICE_ATI_RADEON_R200_QI,		R_200 },
 { DEVICE_ATI_RADEON_R200_QJ,		R_200 },
 { DEVICE_ATI_RADEON_R200_QK,		R_200 },
 { DEVICE_ATI_RADEON_R200_QL,		R_200 },
 { DEVICE_ATI_RADEON_R200_QM,		R_200 },
 { DEVICE_ATI_RADEON_R200_QN,		R_200 },
 { DEVICE_ATI_RADEON_R200_QO,		R_200 },
 { DEVICE_ATI_RADEON_R200_QH2,		R_200 },
 { DEVICE_ATI_RADEON_R200_QI2,		R_200 },
 { DEVICE_ATI_RADEON_R200_QJ2,		R_200 },
 { DEVICE_ATI_RADEON_R200_QK2,		R_200 },
 { DEVICE_ATI_RADEON_R200_QL2,		R_200 },
 { DEVICE_ATI_RADEON_RV200_QW,		R_150|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_RV200_QX,		R_150|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_IGP330_340_350,R_200|R_INTEGRATED },
 { DEVICE_ATI_RADEON_IGP_330M_340M_350M,R_200|R_INTEGRATED },
 { DEVICE_ATI_RADEON_RV250_IG,		R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_7000_IGP,		R_250|R_OVL_SHIFT|R_INTEGRATED },
 { DEVICE_ATI_RADEON_MOBILITY_7000,	R_250|R_OVL_SHIFT|R_INTEGRATED },
 { DEVICE_ATI_RADEON_RV250_ID,		R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_RV250_IE,		R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_RV250_IF,		R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_RV250_IG,		R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_R250_LD,		R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_R250_LE,		R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_R250_MOBILITY,	R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_R250_LG,		R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RV250_RADEON_9000,	R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RADEON_RV250_RADEON2,	R_250|R_OVL_SHIFT },
 { DEVICE_ATI_RV280_RADEON_9200,	R_280 },
 { DEVICE_ATI_RV280_RADEON_92002,	R_280 },
 { DEVICE_ATI_RV280_RADEON_92003,	R_280 },
 { DEVICE_ATI_RV280_RADEON_92004,	R_280 },
 { DEVICE_ATI_RV280_RADEON_92005,	R_280 },
 { DEVICE_ATI_RV280_RADEON_92006,	R_280 },
 { DEVICE_ATI_RV280_RADEON_92007,	R_280 },
 { DEVICE_ATI_M9_5C61_RADEON,		R_280 },
 { DEVICE_ATI_M9_5C63_RADEON,		R_280 },
/* Radeon3 (indeed: Rage 1024 Pro ;) */
 { DEVICE_ATI_R300_AG_FIREGL,		R_300 },
 { DEVICE_ATI_RADEON_R300_ND,		R_300 },
 { DEVICE_ATI_RADEON_R300_NE,		R_300 },
 { DEVICE_ATI_RADEON_R300_NG,		R_300 },
 { DEVICE_ATI_R300_AD_RADEON,		R_300 },
 { DEVICE_ATI_R300_AE_RADEON,		R_300 },
 { DEVICE_ATI_R300_AF_RADEON,		R_300 },
 { DEVICE_ATI_RADEON_9100_IGP2,		R_300|R_OVL_SHIFT|R_INTEGRATED },
 { DEVICE_ATI_RS300M_AGP_RADEON,	R_300|R_INTEGRATED },
 { DEVICE_ATI_RS482_RADEON_XPRESS,	R_350|R_INTEGRATED },
 { DEVICE_ATI_R350_AH_RADEON,		R_350 },
 { DEVICE_ATI_R350_AI_RADEON,		R_350 },
 { DEVICE_ATI_R350_AJ_RADEON,		R_350 },
 { DEVICE_ATI_R350_AK_FIRE,		R_350 },
 { DEVICE_ATI_RADEON_R350_RADEON2,	R_350 },
 { DEVICE_ATI_RADEON_R350_RADEON3,	R_350 },
 { DEVICE_ATI_RV350_NJ_RADEON,		R_350 },
 { DEVICE_ATI_R350_NK_FIRE,		R_350 },
 { DEVICE_ATI_RV350_AP_RADEON,		R_350 },
 { DEVICE_ATI_RV350_AQ_RADEON,		R_350 },
 { DEVICE_ATI_RV350_AR_RADEON,		R_350 },
 { DEVICE_ATI_RV350_AS_RADEON,		R_350 },
 { DEVICE_ATI_RV350_AT_FIRE,		R_350 },
 { DEVICE_ATI_RV350_AU_FIRE,		R_350 },
 { DEVICE_ATI_RV350_AV_FIRE,		R_350 },
 { DEVICE_ATI_RV350_AW_FIRE,		R_350 },
 { DEVICE_ATI_RV350_MOBILITY_RADEON,	R_350 },
 { DEVICE_ATI_RV350_NF_RADEON,		R_300 },
 { DEVICE_ATI_RV350_NJ_RADEON,		R_300 },
 { DEVICE_ATI_RV350_AS_RADEON2,		R_350 },
 { DEVICE_ATI_M10_NQ_RADEON,		R_350 },
 { DEVICE_ATI_M10_NQ_RADEON2,		R_350 },
 { DEVICE_ATI_RV350_MOBILITY_RADEON2,	R_350 },
 { DEVICE_ATI_M10_NS_RADEON,		R_350 },
 { DEVICE_ATI_M10_NT_FIREGL,		R_350 },
 { DEVICE_ATI_M11_NV_FIREGL,		R_350 },
 { DEVICE_ATI_RV370_5B60_RADEON,	R_370|R_PCIE  },
 { DEVICE_ATI_RV370_SAPPHIRE_X550,	R_370 },
 { DEVICE_ATI_RV370_5B64_FIREGL,	R_370|R_PCIE  },
 { DEVICE_ATI_RV370_5B65_FIREGL,	R_370|R_PCIE  },
 { DEVICE_ATI_M24_1P_RADEON,		R_370  },
 { DEVICE_ATI_M22_RADEON_MOBILITY,	R_370  },
 { DEVICE_ATI_M24_1T_FIREGL,		R_370  },
 { DEVICE_ATI_M24_RADEON_MOBILITY,	R_370  },
 { DEVICE_ATI_RV370_RADEON_X300SE,	R_370  },
 { DEVICE_ATI_RV370_SECONDARY_SAPPHIRE,	R_370  },
 { DEVICE_ATI_RV370_5B64_FIREGL2,	R_370  },
 { DEVICE_ATI_RV380_0X3E50_RADEON,	R_380|R_PCIE  },
 { DEVICE_ATI_RV380_0X3E54_FIREGL,	R_380|R_PCIE  },
 { DEVICE_ATI_RV380_RADEON_X600,	R_380|R_PCIE  },
 { DEVICE_ATI_RV380_RADEON_X6002,	R_380  },
 { DEVICE_ATI_RV380_RADEON_X6003,	R_380  },
 { DEVICE_ATI_RV410_FIREGL_V5000,	R_420  },
 { DEVICE_ATI_RV410_FIREGL_V3300,	R_420  },
 { DEVICE_ATI_RV410_RADEON_X700XT,	R_420  },
 { DEVICE_ATI_RV410_RADEON_X700,	R_420|R_PCIE  },
 { DEVICE_ATI_RV410_RADEON_X700SE,	R_420  },
 { DEVICE_ATI_RV410_RADEON_X7002,	R_420|R_PCIE  },
 { DEVICE_ATI_RV410_RADEON_X7003,	R_420  },
 { DEVICE_ATI_RV410_RADEON_X7004,	R_420|R_PCIE  },
 { DEVICE_ATI_RV410_RADEON_X7005,	R_420|R_PCIE  },
 { DEVICE_ATI_M26_MOBILITY_FIREGL,	R_420  },
 { DEVICE_ATI_M26_MOBILITY_FIREGL2,	R_420  },
 { DEVICE_ATI_M26_RADEON_MOBILITY,	R_420  },
 { DEVICE_ATI_M26_RADEON_MOBILITY2,	R_420  },
 { DEVICE_ATI_RADEON_MOBILITY_X700,	R_420  },
 { DEVICE_ATI_R420_JH_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R420_JI_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R420_JJ_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R420_JK_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R420_JL_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R420_JM_FIREGL,		R_420|R_PCIE  },
 { DEVICE_ATI_M18_JN_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R420_JP_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R420_RADEON_X800,		R_420|R_PCIE  },
 { DEVICE_ATI_R420_RADEON_X8002,	R_420|R_PCIE  },
 { DEVICE_ATI_R420_RADEON_X8003,	R_420|R_PCIE  },
 { DEVICE_ATI_R420_RADEON_X8004,	R_420|R_PCIE  },
 { DEVICE_ATI_R420_RADEON_X8005,	R_420|R_PCIE  },
 { DEVICE_ATI_R420_JM_FIREGL,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_5F57_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_5F57_RADEON2,	R_420|R_PCIE  },
 { DEVICE_ATI_R423_UH_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_UI_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_UJ_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_UK_RADEON,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_FIRE_GL,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_UQ_FIREGL,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_UR_FIREGL,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_UT_FIREGL,		R_420|R_PCIE  },
 { DEVICE_ATI_R423_UI_RADEON2,		R_420|R_PCIE  },
 { DEVICE_ATI_R423GL_SE_ATI_FIREGL,	R_420|R_PCIE  },
 { DEVICE_ATI_R423_RADEON_X800XT,	R_420|R_PCIE  },
 { DEVICE_ATI_RADEON_R423_UK,		R_420|R_PCIE  },
 { DEVICE_ATI_M28_RADEON_MOBILITY,	R_420  },
 { DEVICE_ATI_M28_MOBILITY_FIREGL,	R_420  },
 { DEVICE_ATI_MOBILITY_RADEON_X800,	R_420  },
 { DEVICE_ATI_R430_RADEON_X800,		R_430|R_PCIE  },
 { DEVICE_ATI_R430_RADEON_X8002,	R_430|R_PCIE  },
 { DEVICE_ATI_R430_RADEON_X8003,	R_430|R_PCIE  },
 { DEVICE_ATI_R430_RADEON_X8004,	R_430|R_PCIE  },
 { DEVICE_ATI_R480_RADEON_X800,		R_480  },
 { DEVICE_ATI_R480_RADEON_X8002,	R_480  },
 { DEVICE_ATI_R480_RADEON_X850XT,	R_480  },
 { DEVICE_ATI_R480_RADEON_X850PRO,	R_480  },
 { DEVICE_ATI_R481_RADEON_X850XT_PE,	R_480|R_PCIE  },
 { DEVICE_ATI_R480_RADEON_X850XT2,	R_480  },
 { DEVICE_ATI_R480_RADEON_X850PRO2,	R_480  },
 { DEVICE_ATI_R481_RADEON_X850XT_PE2,	R_480|R_PCIE  },
 { DEVICE_ATI_R480_RADEON_X850XT3,	R_480|R_PCIE  },
 { DEVICE_ATI_R480_RADEON_X850XT4,	R_480|R_PCIE  },
 { DEVICE_ATI_R480_RADEON_X850XT5,	R_480|R_PCIE  },
 { DEVICE_ATI_R480_RADEON_X850XT6,	R_480|R_PCIE  },
#endif
};


static void * radeon_mmio_base = 0;
static void * radeon_mem_base = 0; 
static int32_t radeon_overlay_off = 0;
static uint32_t radeon_ram_size = 0;

#define GETREG(TYPE,PTR,OFFZ)		(*((volatile TYPE*)((PTR)+(OFFZ))))
#define SETREG(TYPE,PTR,OFFZ,VAL)	(*((volatile TYPE*)((PTR)+(OFFZ))))=VAL

#define INREG8(addr)		GETREG(uint8_t,(uint8_t *)(radeon_mmio_base),addr)
#define OUTREG8(addr,val)	SETREG(uint8_t,(uint8_t *)(radeon_mmio_base),addr,val)
static inline uint32_t INREG (uint32_t addr) {
    uint32_t tmp = GETREG(uint32_t,(uint8_t *)(radeon_mmio_base),addr);
    return le2me_32(tmp);
}
#define OUTREG(addr,val)	SETREG(uint32_t,(uint8_t *)(radeon_mmio_base),addr,le2me_32(val))
#define OUTREGP(addr,val,mask)						\
	do {								\
		unsigned int _tmp = INREG(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTREG(addr, _tmp);					\
	} while (0)

static __inline__ uint32_t INPLL(uint32_t addr)
{
	OUTREG8(CLOCK_CNTL_INDEX, addr & 0x0000001f);
	return INREG(CLOCK_CNTL_DATA);
}

#define OUTPLL(addr,val)	OUTREG8(CLOCK_CNTL_INDEX, (addr & 0x0000001f) | 0x00000080); \
				OUTREG(CLOCK_CNTL_DATA, val)
#define OUTPLLP(addr,val,mask)						\
	do {								\
		unsigned int _tmp = INPLL(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTPLL(addr, _tmp);					\
	} while (0)

#ifndef RAGE128
enum radeon_montype
{
    MT_NONE,
    MT_CRT, /* CRT-(cathode ray tube) analog monitor. (15-pin VGA connector) */
    MT_LCD, /* Liquid Crystal Display */
    MT_DFP, /* DFP-digital flat panel monitor. (24-pin DVI-I connector) */
    MT_CTV, /* Composite TV out (not in VE) */
    MT_STV  /* S-Video TV out (probably in VE only) */
};

typedef struct radeon_info_s
{
	int hasCRTC2;
	int crtDispType;
	int dviDispType;
}rinfo_t;

static rinfo_t rinfo;

static char * GET_MON_NAME(int type)
{
  char *pret;
  switch(type)
  {
    case MT_NONE: pret = "no"; break;
    case MT_CRT:  pret = "CRT"; break;
    case MT_DFP:  pret = "DFP"; break;
    case MT_LCD:  pret = "LCD"; break;
    case MT_CTV:  pret = "CTV"; break;
    case MT_STV:  pret = "STV"; break;
    default:	  pret = "Unknown";
  }
  return pret;
}

static void radeon_get_moninfo (rinfo_t *rinfo)
{
	unsigned int tmp;

	tmp = INREG(RADEON_BIOS_4_SCRATCH);

	if (rinfo->hasCRTC2) {
		/* primary DVI port */
		if (tmp & 0x08)
			rinfo->dviDispType = MT_DFP;
		else if (tmp & 0x4)
			rinfo->dviDispType = MT_LCD;
		else if (tmp & 0x200)
			rinfo->dviDispType = MT_CRT;
		else if (tmp & 0x10)
			rinfo->dviDispType = MT_CTV;
		else if (tmp & 0x20)
			rinfo->dviDispType = MT_STV;

		/* secondary CRT port */
		if (tmp & 0x2)
			rinfo->crtDispType = MT_CRT;
		else if (tmp & 0x800)
			rinfo->crtDispType = MT_DFP;
		else if (tmp & 0x400)
			rinfo->crtDispType = MT_LCD;
		else if (tmp & 0x1000)
			rinfo->crtDispType = MT_CTV;
		else if (tmp & 0x2000)
			rinfo->crtDispType = MT_STV;
	} else {
		rinfo->dviDispType = MT_NONE;

		tmp = INREG(FP_GEN_CNTL);

		if (tmp & FP_EN_TMDS)
			rinfo->crtDispType = MT_DFP;
		else
			rinfo->crtDispType = MT_CRT;
	}
}
#endif

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
  uint32_t xres,h_total;
#ifndef RAGE128
  if(rinfo.hasCRTC2 && 
       (rinfo.dviDispType == MT_CTV || rinfo.dviDispType == MT_STV))
	h_total = INREG(CRTC2_H_TOTAL_DISP);
  else
#endif
	h_total = INREG(CRTC_H_TOTAL_DISP);
  xres = (h_total >> 16) & 0xffff;
  return (xres + 1)*8;
}

static uint32_t radeon_get_yres( void )
{
  uint32_t yres,v_total;
#ifndef RAGE128
  if(rinfo.hasCRTC2 && 
       (rinfo.dviDispType == MT_CTV || rinfo.dviDispType == MT_STV))
	v_total = INREG(CRTC2_V_TOTAL_DISP);
  else
#endif
	v_total = INREG(CRTC_V_TOTAL_DISP);
  yres = (v_total >> 16) & 0xffff;
  return yres + 1;
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
    mclk_cntl	     = INPLL(MCLK_CNTL);

    OUTPLL(MCLK_CNTL, mclk_cntl | FORCE_GCP | FORCE_PIPE3D_CP);

    gen_reset_cntl   = INREG(GEN_RESET_CNTL);

    OUTREG(GEN_RESET_CNTL, gen_reset_cntl | SOFT_RESET_GUI);
    INREG(GEN_RESET_CNTL);
    OUTREG(GEN_RESET_CNTL,
	gen_reset_cntl & (uint32_t)(~SOFT_RESET_GUI));
    INREG(GEN_RESET_CNTL);

    OUTPLL(MCLK_CNTL,	     mclk_cntl);
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
    OUTREGP(DP_DATATYPE,
	    HOST_BIG_ENDIAN_EN, ~HOST_BIG_ENDIAN_EN);
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
static const REF_TRANSFORM trans[2] =
{
	{1.1678, 0.0, 1.6007, -0.3929, -0.8154, 2.0232, 0.0}, /* BT.601 */
	{1.1678, 0.0, 1.7980, -0.2139, -0.5345, 2.1186, 0.0}  /* BT.709 */
};
/****************************************************************************
 * SetTransform								    *
 *  Function: Calculates and sets color space transform from supplied	    *
 *	      reference transform, gamma, brightness, contrast, hue and	    *
 *	      saturation.						    *
 *    Inputs: bright - brightness					    *
 *	      cont - contrast						    *
 *	      sat - saturation						    *
 *	      hue - hue							    *
 *	      red_intensity - intense of red component			    *
 *	      green_intensity - intense of green component		    *
 *	      blue_intensity - intense of blue component		    *
 *	      ref - index to the table of refernce transforms		    *
 *   Outputs: NONE							    *
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
   
	dwOvROff = ((int)(OvROff * 2.0)) & 0x1fff;
	dwOvGOff = (int)(OvGOff * 2.0) & 0x1fff;
	dwOvBOff = (int)(OvBOff * 2.0) & 0x1fff;
	/* Whatever docs say about R200 having 3.8 format instead of 3.11
	   as in Radeon is a lie */

		dwOvLuma =(((int)(OvLuma * 2048.0))&0x7fff)<<17;
		dwOvRCb = (((int)(OvRCb * 2048.0))&0x7fff)<<1;
		dwOvRCr = (((int)(OvRCr * 2048.0))&0x7fff)<<17;
		dwOvGCb = (((int)(OvGCb * 2048.0))&0x7fff)<<1;
		dwOvGCr = (((int)(OvGCr * 2048.0))&0x7fff)<<17;
		dwOvBCb = (((int)(OvBCb * 2048.0))&0x7fff)<<1;
		dwOvBCr = (((int)(OvBCr * 2048.0))&0x7fff)<<17;

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
static const GAMMA_SETTINGS r200_def_gamma[18] = 
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

static const GAMMA_SETTINGS r100_def_gamma[6] = 
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
    if((besr.chip_flags & R_100)==R_100||
	(besr.chip_flags & R_120)==R_120||
	(besr.chip_flags & R_150)==R_150){
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
	OUTREG(OV0_LIN_TRANS_A, 0x12a20000);
	OUTREG(OV0_LIN_TRANS_B, 0x198a190e);
	OUTREG(OV0_LIN_TRANS_C, 0x12a2f9da);
	OUTREG(OV0_LIN_TRANS_D, 0xf2fe0442);
	OUTREG(OV0_LIN_TRANS_E, 0x12a22046);
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
  besr.saturation = 0x0F;
  besr.brightness = 0;
  OUTREG(OV0_COLOUR_CNTL,0x000F0F00UL); /* Default brihgtness and saturation for Rage128 */
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

static int find_chip(unsigned chip_id)
{
  unsigned i;
  for(i = 0;i < sizeof(ati_card_ids)/sizeof(ati_card_ids_t);i++)
  {
    if(chip_id == ati_card_ids[i].id) return i;
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

#if !defined(RAGE128) && defined(CONFIG_X11)
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
        printf(RADEON_MSG" Output may not work correctly, check your DRI configration!");
      }
      printf("\n");
    }
  }
}
#endif

static int radeon_probe(int verbose, int force)
{
  pciinfo_t lst[MAX_PCI_DEVICES];
  unsigned i,num_pci;
  int err;
  verbosity = verbose;
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
	memset(&besr,0,sizeof(bes_registers_t));
	if(force > PROBE_NORMAL)
	{
	    printf(RADEON_MSG" Driver was forced. Was found %sknown chip\n",idx == -1 ? "un" : "");
	    if(idx == -1)
#ifdef RAGE128
		printf(RADEON_MSG" Assuming it as Rage128\n");
#else
		printf(RADEON_MSG" Assuming it as Radeon1\n");
#endif
	    besr.chip_flags=R_100|R_OVL_SHIFT;
	}
#if !defined(RAGE128) && defined(CONFIG_X11)
        probe_fireGL_driver();
#endif
	if(idx != -1) besr.chip_flags=ati_card_ids[idx].flags;
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

typedef struct saved_regs_s
{
    uint32_t ov0_vid_key_clr;
    uint32_t ov0_vid_key_msk;
    uint32_t ov0_graphics_key_clr;
    uint32_t ov0_graphics_key_msk;
    uint32_t ov0_key_cntl;
    uint32_t disp_merge_cntl;
}saved_regs_t;
static saved_regs_t savreg;

static void save_regs( void )
{
    radeon_fifo_wait(6);
    savreg.ov0_vid_key_clr	= INREG(OV0_VID_KEY_CLR);
    savreg.ov0_vid_key_msk	= INREG(OV0_VID_KEY_MSK);
    savreg.ov0_graphics_key_clr = INREG(OV0_GRAPHICS_KEY_CLR);
    savreg.ov0_graphics_key_msk = INREG(OV0_GRAPHICS_KEY_MSK);
    savreg.ov0_key_cntl		= INREG(OV0_KEY_CNTL);
    savreg.disp_merge_cntl	= INREG(DISP_MERGE_CNTL);
}

static void restore_regs( void )
{
    radeon_fifo_wait(6);
    OUTREG(OV0_VID_KEY_CLR,savreg.ov0_vid_key_clr);
    OUTREG(OV0_VID_KEY_MSK,savreg.ov0_vid_key_msk);
    OUTREG(OV0_GRAPHICS_KEY_CLR,savreg.ov0_graphics_key_clr);
    OUTREG(OV0_GRAPHICS_KEY_MSK,savreg.ov0_graphics_key_msk);
    OUTREG(OV0_KEY_CNTL,savreg.ov0_key_cntl);
    OUTREG(DISP_MERGE_CNTL,savreg.disp_merge_cntl);
}

static int radeon_init(void)
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
      printf(RADEON_MSG" Working around buggy Radeon Mobility M6 (0 vs. 8MB ram)\n");
      radeon_ram_size = 8192*1024;
  }
  else if (radeon_ram_size == 0 &&
           (def_cap.device_id == DEVICE_ATI_RS482_RADEON_XPRESS))
  {
      printf(RADEON_MSG" Working around buggy RS482 Radeon Xpress 200 Memory Detection\n");
      radeon_ram_size = (INREG(CONFIG_MEMSIZE) + 0x100000) << 2;
      radeon_ram_size &=  CONFIG_MEMSIZE_MASK;
  } 
#else
  /* Rage Mobility (rage128) also has memsize bug */
  if (radeon_ram_size == 0 &&
      (def_cap.device_id == DEVICE_ATI_RAGE_MOBILITY_M3 ||
       def_cap.device_id == DEVICE_ATI_RAGE_MOBILITY_M32))
  {
      printf(RADEON_MSG" Working around Rage Mobility M3 (0 vs. 8MB ram)\n");
      radeon_ram_size = 8192*1024;
  }
#endif
  if((radeon_mem_base = map_phys_mem(pci_info.base0,radeon_ram_size))==(void *)-1) return ENOMEM;
  radeon_vid_make_default();
  printf(RADEON_MSG" Video memory = %uMb\n",radeon_ram_size/0x100000);
  err = mtrr_set_type(pci_info.base0,radeon_ram_size,MTRR_TYPE_WRCOMB);
  if(!err) printf(RADEON_MSG" Set write-combining type of video memory\n");
#ifndef RAGE128
  {
    memset(&rinfo,0,sizeof(rinfo_t));
    if((besr.chip_flags&R_100) != R_100) rinfo.hasCRTC2 = 1;
    
    radeon_get_moninfo(&rinfo);
	if(rinfo.hasCRTC2) {
	    printf(RADEON_MSG" DVI port has %s monitor connected\n",GET_MON_NAME(rinfo.dviDispType));
	    printf(RADEON_MSG" CRT port has %s monitor connected\n",GET_MON_NAME(rinfo.crtDispType));
	}
	else
	    printf(RADEON_MSG" CRT port has %s monitor connected\n",GET_MON_NAME(rinfo.crtDispType));
  }
#endif
  save_regs();
  return 0;  
}

static void radeon_destroy(void)
{
  restore_regs();
  unmap_phys_mem(radeon_mem_base,radeon_ram_size);
  unmap_phys_mem(radeon_mmio_base,0xFFFF);
}

static int radeon_get_caps(vidix_capability_t *to)
{
  memcpy(to,&def_cap,sizeof(vidix_capability_t));
  return 0; 
}

/*
  Full list of fourcc which are supported by Win2K radeon driver:
  YUY2, UYVY, DDES, OGLT, OGL2, OGLS, OGLB, OGNT, OGNZ, OGNS,
  IF09, YVU9, IMC4, M2IA, IYUV, VBID, DXT1, DXT2, DXT3, DXT4, DXT5
*/
typedef struct fourcc_desc_s
{
    uint32_t fourcc;
    unsigned max_srcw;
}fourcc_desc_t;

static const fourcc_desc_t supported_fourcc[] = 
{
  { IMGFMT_Y800, 1567 },
  { IMGFMT_YVU9, 1567 },
  { IMGFMT_IF09, 1567 },
  { IMGFMT_YV12, 1567 },
  { IMGFMT_I420, 1567 },
  { IMGFMT_IYUV, 1567 }, 
  { IMGFMT_UYVY, 1551 },
  { IMGFMT_YUY2, 1551 },
  { IMGFMT_YVYU, 1551 },
  { IMGFMT_RGB15, 1551 },
  { IMGFMT_BGR15, 1551 },
  { IMGFMT_RGB16, 1551 },
  { IMGFMT_BGR16, 1551 },
  { IMGFMT_RGB32, 775 },
  { IMGFMT_BGR32, 775 }
};

__inline__ static int is_supported_fourcc(uint32_t fourcc)
{
  unsigned i;
  for(i=0;i<sizeof(supported_fourcc)/sizeof(fourcc_desc_t);i++)
  {
    if(fourcc==supported_fourcc[i].fourcc)
      return 1;
  }
  return 0;
}

static int radeon_query_fourcc(vidix_fourcc_t *to)
{
    if(is_supported_fourcc(to->fourcc))
    {
	to->depth = VID_DEPTH_ALL;
	to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK | VID_CAP_COLORKEY |
		    VID_CAP_BLEND;
	return 0;
    }
    else  to->depth = to->flags = 0;
    return ENOSYS;
}

static double H_scale_ratio;
static void radeon_vid_dump_regs( void )
{
  size_t i;
  printf(RADEON_MSG"*** Begin of DRIVER variables dump ***\n");
  printf(RADEON_MSG"radeon_mmio_base=%p\n",radeon_mmio_base);
  printf(RADEON_MSG"radeon_mem_base=%p\n",radeon_mem_base);
  printf(RADEON_MSG"radeon_overlay_off=%08X\n",radeon_overlay_off);
  printf(RADEON_MSG"radeon_ram_size=%08X\n",radeon_ram_size);
  printf(RADEON_MSG"video mode: %ux%u@%u\n",radeon_get_xres(),radeon_get_yres(),radeon_vid_get_dbpp());
  printf(RADEON_MSG"H_scale_ratio=%8.2f\n",H_scale_ratio);
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
#ifdef RAGE128    
    OUTREG(OV0_KEY_CNTL, GRAPHIC_KEY_FN_NE);
#else
    OUTREG(OV0_KEY_CNTL, GRAPHIC_KEY_FN_EQ);
#endif
    OUTREG(OV0_TEST, 0);
}

static void radeon_vid_display_video( void )
{
    int bes_flags,force_second;
    radeon_fifo_wait(2);
    OUTREG(OV0_REG_LOAD_CNTL,		REG_LD_CTL_LOCK);
    radeon_engine_idle();
    while(!(INREG(OV0_REG_LOAD_CNTL)&REG_LD_CTL_LOCK_READBACK));
    radeon_fifo_wait(15);

    force_second=0;

    /* Shutdown capturing */
    OUTREG(FCP_CNTL, FCP_CNTL__GND);
    OUTREG(CAP0_TRIG_CNTL, 0);

    OUTREG(VID_BUFFER_CONTROL, (1<<16) | 0x01);
    OUTREG(DISP_TEST_DEBUG_CNTL, 0);

    OUTREG(OV0_AUTO_FLIP_CNTL,OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD);

    if(besr.deinterlace_on) OUTREG(OV0_DEINTERLACE_PATTERN,besr.deinterlace_pattern);
#ifdef RAGE128
    OUTREG(OV0_COLOUR_CNTL, (besr.brightness & 0x7f) |
			    (besr.saturation << 8) |
			    (besr.saturation << 16));
#endif
    radeon_fifo_wait(2);
    OUTREG(OV0_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
    OUTREG(OV0_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
    OUTREG(OV0_KEY_CNTL,besr.ckey_cntl);

    OUTREG(OV0_H_INC,			besr.h_inc);
    OUTREG(OV0_STEP_BY,			besr.step_by);
    if(force_second)
    {
	OUTREG(OV1_Y_X_START,		besr.y_x_start);
	OUTREG(OV1_Y_X_END,		besr.y_x_end);
    }
    else
    {
	OUTREG(OV0_Y_X_START,		besr.y_x_start);
	OUTREG(OV0_Y_X_END,		besr.y_x_end);
    }
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

    bes_flags = SCALER_ENABLE |
		SCALER_SMART_SWITCH |
		SCALER_Y2R_TEMP |
		SCALER_PIX_EXPAND;
    if(besr.double_buff) bes_flags |= SCALER_DOUBLE_BUFFER;
    if(besr.deinterlace_on) bes_flags |= SCALER_ADAPTIVE_DEINT;
    if(besr.horz_pick_nearest) bes_flags |= SCALER_HORZ_PICK_NEAREST;
    if(besr.vert_pick_nearest) bes_flags |= SCALER_VERT_PICK_NEAREST;
#ifdef RAGE128
    bes_flags |= SCALER_BURST_PER_PLANE;
#endif
    bes_flags |= (besr.surf_id << 8) & SCALER_SURFAC_FORMAT;
    if(besr.load_prg_start) bes_flags |= SCALER_PRG_LOAD_START;
    if(force_second)	bes_flags |= SCALER_USE_OV1;
    else		bes_flags &= ~SCALER_USE_OV1;
    OUTREG(OV0_SCALE_CNTL,		bes_flags);
    radeon_fifo_wait(6);
    OUTREG(OV0_FILTER_CNTL,besr.filter_cntl);
    OUTREG(OV0_FOUR_TAP_COEF_0,besr.four_tap_coeff[0]);
    OUTREG(OV0_FOUR_TAP_COEF_1,besr.four_tap_coeff[1]);
    OUTREG(OV0_FOUR_TAP_COEF_2,besr.four_tap_coeff[2]);
    OUTREG(OV0_FOUR_TAP_COEF_3,besr.four_tap_coeff[3]);
    OUTREG(OV0_FOUR_TAP_COEF_4,besr.four_tap_coeff[4]);
    if(besr.swap_uv) OUTREG(OV0_TEST,INREG(OV0_TEST)|OV0_SWAP_UV);
    OUTREG(OV0_REG_LOAD_CNTL,		0);
    if(verbosity > VERBOSE_LEVEL) printf(RADEON_MSG"we wanted: scaler=%08X\n",bes_flags);
    if(verbosity > VERBOSE_LEVEL) radeon_vid_dump_regs();
}

/* Goal of this function: hide RGB background and provide black screen around movie.
   Useful in '-vo fbdev:vidix -fs -zoom' mode.
   Reverse effect to colorkey */
#ifdef RAGE128
static void radeon_vid_exclusive( void )
{
/* this function works only with Rage128.
   Radeon should has something the same */
    unsigned screenw,screenh;
    screenw = radeon_get_xres();
    screenh = radeon_get_yres();
    radeon_fifo_wait(2);
    OUTREG(OV0_EXCLUSIVE_VERT,(((screenh-1)<<16)&EXCL_VERT_END_MASK));
    OUTREG(OV0_EXCLUSIVE_HORZ,(((screenw/8+1)<<8)&EXCL_HORZ_END_MASK)|EXCL_HORZ_EXCLUSIVE_EN);
}

static void radeon_vid_non_exclusive( void )
{
    OUTREG(OV0_EXCLUSIVE_HORZ,0);
}
#endif

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
	case IMGFMT_IF09:
	case IMGFMT_YVU9:
		if(spy >= 64 && spu == spy/4 && spv == spy/4)	pitch = spy;
		else						pitch = 64;
		break;
	default:
		if(spy >= 16)	pitch = spy;
		else		pitch = 16;
		break;
  }
  return pitch;
}

static void Calc_H_INC_STEP_BY (
	int fieldvalue_OV0_SURFACE_FORMAT,
	double H_scale_ratio,
	int DisallowFourTapVertFiltering,
	int DisallowFourTapUVVertFiltering,
	uint32_t *val_OV0_P1_H_INC,
	uint32_t *val_OV0_P1_H_STEP_BY,
	uint32_t *val_OV0_P23_H_INC,
	uint32_t *val_OV0_P23_H_STEP_BY,
	int *P1GroupSize,
	int *P1StepSize,
	int *P23StepSize )
{

    double ClocksNeededFor16Pixels;

    switch (fieldvalue_OV0_SURFACE_FORMAT)
    {
	case 3:
	case 4: /*16BPP (ARGB1555 and RGB565) */
	    /* All colour components are fetched in pairs */
	    *P1GroupSize = 2;
	    /* We don't support four tap in this mode because G's are split between two bytes. In theory we could support it if */
	    /* we saved part of the G when fetching the R, and then filter the G, followed by the B in the following cycles. */
	    if (H_scale_ratio>=.5)
	    {
		/* We are actually generating two pixels (but 3 colour components) per tick. Thus we don't have to skip */
		/* until we reach .5. P1 and P23 are the same. */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 1;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 1;
		*P1StepSize = 1;
		*P23StepSize = 1;
	    }
	    else if (H_scale_ratio>=.25)
	    {
		/* Step by two */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 2;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 2;
		*P1StepSize = 2;
		*P23StepSize = 2;
	    }
	    else if (H_scale_ratio>=.125)
	    {
		/* Step by four */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 3;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 3;
		*P1StepSize = 4;
		*P23StepSize = 4;
	    }
	    else if (H_scale_ratio>=.0625)
	    {
		/* Step by eight */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 4;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 4;
		*P1StepSize = 8;
		*P23StepSize = 8;
	    }
	    else if (H_scale_ratio>=0.03125)
	    {
		/* Step by sixteen */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 5;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 5;
		*P1StepSize = 16;
		*P23StepSize = 16;
	    }
	    else
	    {
		H_scale_ratio=0.03125;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 5;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 5;
		*P1StepSize = 16;
		*P23StepSize = 16;
	    }
	    break;
	case 6: /*32BPP RGB */
	    if (H_scale_ratio>=1.5 && !DisallowFourTapVertFiltering)
	    {
		/* All colour components are fetched in pairs */
		*P1GroupSize = 2;
		/* With four tap filtering, we can generate two colour components every clock, or two pixels every three */
		/* clocks. This means that we will have four tap filtering when scaling 1.5 or more. */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 0;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 0;
		*P1StepSize = 1;
		*P23StepSize = 1;
	    }
	    else if (H_scale_ratio>=0.75)
	    {
		/* Four G colour components are fetched at once */
		*P1GroupSize = 4;
		/* R and B colour components are fetched in pairs */
		/* With two tap filtering, we can generate four colour components every clock. */
		/* This means that we will have two tap filtering when scaling 1.0 or more. */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 1;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 1;
		*P1StepSize = 1;
		*P23StepSize = 1;
	    }
	    else if (H_scale_ratio>=0.375)
	    {
		/* Step by two. */
		/* Four G colour components are fetched at once */
		*P1GroupSize = 4;
		/* R and B colour components are fetched in pairs */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 2;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 2;
		*P1StepSize = 2;
		*P23StepSize = 2;
	    }
	    else if (H_scale_ratio>=0.25)
	    {
		/* Step by two. */
		/* Four G colour components are fetched at once */
		*P1GroupSize = 4;
		/* R and B colour components are fetched in pairs */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 2;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 3;
		*P1StepSize = 2;
		*P23StepSize = 4;
	    }
	    else if (H_scale_ratio>=0.1875)
	    {
		/* Step by four */
		/* Four G colour components are fetched at once */
		*P1GroupSize = 4;
		/* R and B colour components are fetched in pairs */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 3;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 3;
		*P1StepSize = 4;
		*P23StepSize = 4;
	    }
	    else if (H_scale_ratio>=0.125)
	    {
		/* Step by four */
		/* Four G colour components are fetched at once */
		*P1GroupSize = 4;
		/* R and B colour components are fetched in pairs */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 3;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 4;
		*P1StepSize = 4;
		*P23StepSize = 8;
	    }
	    else if (H_scale_ratio>=0.09375)
	    {
		/* Step by eight */
		/* Four G colour components are fetched at once */
		*P1GroupSize = 4;
		/* R and B colour components are fetched in pairs */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 4;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 4;
		*P1StepSize = 8;
		*P23StepSize = 8;
	    }
	    else if (H_scale_ratio>=0.0625)
	    {
		/* Step by eight */
		/* Four G colour components are fetched at once */
		*P1GroupSize = 4;
		/* R and B colour components are fetched in pairs */
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 5;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 5;
		*P1StepSize = 16;
		*P23StepSize = 16;
	    }
	    else
	    {
		H_scale_ratio=0.0625;
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 5;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 5;
		*P1StepSize = 16;
		*P23StepSize = 16;
	    }
	    break;
	case 9:
	    /*ToDo_Active: In mode 9 there is a possibility that HScale ratio may be set to an illegal value, so we have extra conditions in the if statement. For consistancy, these conditions be added to the other modes as well. */
	    /* four tap on both (unless Y is too wide) */
	    if ((H_scale_ratio>=(ClocksNeededFor16Pixels=8+2+2) / 16.0) &&
	       ((uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5)<=0x3000) &&
	       ((uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5)<=0x2000) &&
	       !DisallowFourTapVertFiltering && !DisallowFourTapUVVertFiltering)
	    {	/*0.75 */
		/* Colour components are fetched in pairs */
		*P1GroupSize = 2;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 0;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 0;
		*P1StepSize = 1;
		*P23StepSize = 1;
	    }
	    /* two tap on Y (because it is too big for four tap), four tap on UV */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=4+2+2) / 16.0) &&
		    ((uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5)<=0x3000) &&
		    ((uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5)<=0x2000) &&
		    DisallowFourTapVertFiltering && !DisallowFourTapUVVertFiltering)
	    {	/*0.75 */
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 1;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 0;
		*P1StepSize = 1;
		*P23StepSize = 1;
	    }
	    /* We scale the Y with the four tap filters, but UV's are generated
	       with dual two tap configuration. */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=8+1+1) / 16.0) &&
		    ((uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5)<=0x3000) &&
		    ((uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5)<=0x2000) &&
		    !DisallowFourTapVertFiltering)
	    {	/*0.625 */
		*P1GroupSize = 2;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 0;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 1;
		*P1StepSize = 1;
		*P23StepSize = 1;
	    }
	    /* We scale the Y, U, and V with the two tap filters */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=4+1+1) / 16.0) &&
		    ((uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5)<=0x3000) &&
		    ((uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5)<=0x2000))
	    {	/*0.375 */
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 1;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 1;
		*P1StepSize = 1;
		*P23StepSize = 1;
	    }
	    /* We scale step the U and V by two to allow more bandwidth for fetching Y's,
	       thus we won't drop Y's yet. */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=4+.5+.5) / 16.0) &&
		    ((uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5)<=0x3000) &&
		    ((uint16_t)((1/(H_scale_ratio*4*2)) * (1<<0xc) + 0.5)<=0x2000))
	    {	/*>=0.3125 and >.333333~ */
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 1;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4*2)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 2;
		*P1StepSize = 1;
		*P23StepSize = 2;
	    }
	    /* We step the Y, U, and V by two. */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=2+.5+.5) / 16.0)	&&
		    ((uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5)<=0x3000) &&
		    ((uint16_t)((1/(H_scale_ratio*4*2)) * (1<<0xc) + 0.5)<=0x2000))
	    {
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 2;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4*2)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 2;
		*P1StepSize = 2;
		*P23StepSize = 2;
	    }
	    /* We step the Y by two and the U and V by four. */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=2+.25+.25) / 16.0) &&
		    ((uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5)<=0x3000) &&
		    ((uint16_t)((1/(H_scale_ratio*4*4)) * (1<<0xc) + 0.5)<=0x2000))
	    {
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 2;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4*4)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 3;
		*P1StepSize = 2;
		*P23StepSize = 4;
	    }
	    /* We step the Y, U, and V by four. */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=1+.25+.25) / 16.0) &&
		    ((uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5)<=0x3000) &&
		    ((uint16_t)((1/(H_scale_ratio*4*4)) * (1<<0xc) + 0.5)<=0x2000))
	    {
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 3;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4*4)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 3;
		*P1StepSize = 4;
		*P23StepSize = 4;
	    }
	    /* We would like to step the Y by four and the U and V by eight, but we can't mix step by 3 and step by 4 for packed modes */

	    /* We step the Y, U, and V by eight. */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=.5+.125+.125) / 16.0) &&
		    ((uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5)<=0x3000) &&
		    ((uint16_t)((1/(H_scale_ratio*4*8)) * (1<<0xc) + 0.5)<=0x2000))
	    {
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 4;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4*8)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 4;
		*P1StepSize = 8;
		*P23StepSize = 8;
	    }
	    /* We step the Y by eight and the U and V by sixteen. */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=.5+.0625+.0625) / 16.0) &&
	    ((uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5)<=0x3000) &&
	    ((uint16_t)((1/(H_scale_ratio*4*16)) * (1<<0xc) + 0.5)<=0x2000))
	    {
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 4;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4*16)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 5;
		*P1StepSize = 8;
		*P23StepSize = 16;
	    }
	    /* We step the Y, U, and V by sixteen. */
	    else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=.25+.0625+.0625) / 16.0) &&
		    ((uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5)<=0x3000) &&
		    ((uint16_t)((1/(H_scale_ratio*4*16)) * (1<<0xc) + 0.5)<=0x2000))
	    {
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 5;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4*16)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 5;
		*P1StepSize = 16;
		*P23StepSize = 16;
	    }
	    else
	    {
		H_scale_ratio=(ClocksNeededFor16Pixels=.25+.0625+.0625) / 16;
		*P1GroupSize = 4;
		*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		*val_OV0_P1_H_STEP_BY = 5;
		*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*4*16)) * (1<<0xc) + 0.5);
		*val_OV0_P23_H_STEP_BY = 5;
		*P1StepSize = 16;
		*P23StepSize = 16;
	    }
	    break;
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:    /* YUV12, VYUY422, YUYV422, YOverPkCRCB12, YWovenWithPkCRCB12 */
		/* We scale the Y, U, and V with the four tap filters */
		/* four tap on both (unless Y is too wide) */
		if ((H_scale_ratio>=(ClocksNeededFor16Pixels=8+4+4) / 16.0) &&
		    !DisallowFourTapVertFiltering && !DisallowFourTapUVVertFiltering)
		{	/*0.75 */
		    *P1GroupSize = 2;
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 0;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 0;
		    *P1StepSize = 1;
		    *P23StepSize = 1;
		}
		/* two tap on Y (because it is too big for four tap), four tap on UV */
		else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=4+4+4) / 16.0) &&
			DisallowFourTapVertFiltering && !DisallowFourTapUVVertFiltering)
		{   /*0.75 */
		    *P1GroupSize = 4;
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 1;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 0;
		    *P1StepSize = 1;
		    *P23StepSize = 1;
		}
		/* We scale the Y with the four tap filters, but UV's are generated
		   with dual two tap configuration. */
		else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=8+2+2) / 16.0) &&
			  !DisallowFourTapVertFiltering)
		{   /*0.625 */
		    *P1GroupSize = 2;
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 0;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 1;
		    *P1StepSize = 1;
		    *P23StepSize = 1;
		}
		/* We scale the Y, U, and V with the two tap filters */
		else if (H_scale_ratio>=(ClocksNeededFor16Pixels=4+2+2) / 16.0)
		{   /*0.375 */
		    *P1GroupSize = 4;
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 1;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 1;
		    *P1StepSize = 1;
		    *P23StepSize = 1;
		}
		/* We scale step the U and V by two to allow more bandwidth for
		   fetching Y's, thus we won't drop Y's yet. */
		else if (H_scale_ratio>=(ClocksNeededFor16Pixels=4+1+1) / 16.0)
		{   /*0.312 */
		    *P1GroupSize = 4;
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 1;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*2)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 2;
		    *P1StepSize = 1;
		    *P23StepSize = 2;
		}
		/* We step the Y, U, and V by two. */
		else if (H_scale_ratio>=(ClocksNeededFor16Pixels=2+1+1) / 16.0)
		{
		    *P1GroupSize = 4;
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 2;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*2)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 2;
		    *P1StepSize = 2;
		    *P23StepSize = 2;
		}
		/* We step the Y by two and the U and V by four. */
		else if (H_scale_ratio>=(ClocksNeededFor16Pixels=2+.5+.5) / 16.0)
		{
		    *P1GroupSize = 4;
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*2)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 2;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*4)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 3;
		    *P1StepSize = 2;
		    *P23StepSize = 4;
		}
		/* We step the Y, U, and V by four. */
		else if (H_scale_ratio>=(ClocksNeededFor16Pixels=1+.5+.5) / 16.0)
		{
		    *P1GroupSize = 4;
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 3;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*4)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 3;
		    *P1StepSize = 4;
		    *P23StepSize = 4;
		}
		/* We step the Y by four and the U and V by eight. */
		else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=1+.25+.25) / 16.0) &&
			 (fieldvalue_OV0_SURFACE_FORMAT==10))
		{
		    *P1GroupSize = 4;
		    /* Can't mix step by 3 and step by 4 for packed modes */
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*4)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 3;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*8)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 4;
		    *P1StepSize = 4;
		    *P23StepSize = 8;
		}
		/* We step the Y, U, and V by eight. */
		else if (H_scale_ratio>=(ClocksNeededFor16Pixels=.5+.25+.25) / 16.0)
		{
		    *P1GroupSize = 4;
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 4;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*8)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 4;
		    *P1StepSize = 8;
		    *P23StepSize = 8;
		}
		/* We step the Y by eight and the U and V by sixteen. */
		else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=.5+.125+.125) / 16.0) && (fieldvalue_OV0_SURFACE_FORMAT==10))
		{
		    *P1GroupSize = 4;
		    /* Step by 5 not supported for packed modes */
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 4;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*16)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 5;
		    *P1StepSize = 8;
		    *P23StepSize = 16;
		}
		/* We step the Y, U, and V by sixteen. */
		else if ((H_scale_ratio>=(ClocksNeededFor16Pixels=.25+.125+.125) / 16.0) &&
			 (fieldvalue_OV0_SURFACE_FORMAT==10))
		{
		    *P1GroupSize = 4;
		    /* Step by 5 not supported for packed modes */
		    *val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
		    *val_OV0_P1_H_STEP_BY = 5;
		    *val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*16)) * (1<<0xc) + 0.5);
		    *val_OV0_P23_H_STEP_BY = 5;
		    *P1StepSize = 16;
		    *P23StepSize = 16;
		}
		else
		{
		    if (fieldvalue_OV0_SURFACE_FORMAT==10)
		    {
			H_scale_ratio=(ClocksNeededFor16Pixels=.25+.125+.125) / 16;
			*P1GroupSize = 4;
			*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*16)) * (1<<0xc) + 0.5);
			*val_OV0_P1_H_STEP_BY = 5;
			*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*16)) * (1<<0xc) + 0.5);
			*val_OV0_P23_H_STEP_BY = 5;
			*P1StepSize = 16;
			*P23StepSize = 16;
		    }
		    else
		    {
			H_scale_ratio=(ClocksNeededFor16Pixels=.5+.25+.25) / 16;
			*P1GroupSize = 4;
			*val_OV0_P1_H_INC = (uint16_t)((1/(H_scale_ratio*8)) * (1<<0xc) + 0.5);
			*val_OV0_P1_H_STEP_BY = 4;
			*val_OV0_P23_H_INC = (uint16_t)((1/(H_scale_ratio*2*8)) * (1<<0xc) + 0.5);
			*val_OV0_P23_H_STEP_BY = 4;
			*P1StepSize = 8;
			*P23StepSize = 8;
		    }
		}
		break;
	default:    break;

    }
    besr.h_inc	 = (*(val_OV0_P1_H_INC)&0x3fff) | ((*(val_OV0_P23_H_INC)&0x3fff)<<16);
    besr.step_by = (*(val_OV0_P1_H_STEP_BY)&0x7) | ((*(val_OV0_P23_H_STEP_BY)&0x7)<<8);
}

/* ********************************************************* */
/* ** Setup Black Bordering */
/* ********************************************************* */

static void ComputeBorders( vidix_playback_t *config, int VertUVSubSample )
{
	double tempBLANK_LINES_AT_TOP;
	unsigned TopLine,BottomLine,SourceLinesUsed,TopUVLine,BottomUVLine,SourceUVLinesUsed;
	uint32_t val_OV0_P1_ACTIVE_LINES_M1,val_OV0_P1_BLNK_LN_AT_TOP_M1;
	uint32_t val_OV0_P23_ACTIVE_LINES_M1,val_OV0_P23_BLNK_LN_AT_TOP_M1;

	if (floor(config->src.y)<0) {
	    tempBLANK_LINES_AT_TOP = -floor(config->src.y);
	    TopLine = 0;
	}
	else {
	    tempBLANK_LINES_AT_TOP = 0;
	    TopLine = (int)floor(config->src.y);
	}
	/* Round rSrcBottom up and subtract one */
	if (ceil(config->src.y+config->src.h) > config->src.h)
	{
	    BottomLine = config->src.h - 1;
	}
	else
	{
	    BottomLine = (int)ceil(config->src.y+config->src.h) - 1;
	}

	if (BottomLine >= TopLine)
	{
	    SourceLinesUsed = BottomLine - TopLine + 1;
	}
	else
	{
	    /*CYCACC_ASSERT(0, "SourceLinesUsed less than or equal to zero.") */
	    SourceLinesUsed = 1;
	}

	{
	    int SourceHeightInPixels;
	    SourceHeightInPixels = BottomLine - TopLine + 1;
	}

	val_OV0_P1_ACTIVE_LINES_M1 = SourceLinesUsed - 1;
	val_OV0_P1_BLNK_LN_AT_TOP_M1 = ((int)tempBLANK_LINES_AT_TOP-1) & 0xfff;

	TopUVLine = ((int)(config->src.y/VertUVSubSample) < 0)	?  0: (int)(config->src.y/VertUVSubSample);   /* Round rSrcTop down */
	BottomUVLine = (ceil(((config->src.y+config->src.h)/VertUVSubSample)) > (config->src.h/VertUVSubSample))
	? (config->src.h/VertUVSubSample)-1 : (unsigned int)ceil(((config->src.y+config->src.h)/VertUVSubSample))-1;

	if (BottomUVLine >= TopUVLine)
	{
	    SourceUVLinesUsed = BottomUVLine - TopUVLine + 1;
	}
	else
	{
	    /*CYCACC_ASSERT(0, "SourceUVLinesUsed less than or equal to zero.") */
	    SourceUVLinesUsed = 1;
	}
	val_OV0_P23_ACTIVE_LINES_M1 = SourceUVLinesUsed - 1;
	val_OV0_P23_BLNK_LN_AT_TOP_M1 = ((int)(tempBLANK_LINES_AT_TOP/VertUVSubSample)-1) & 0x7ff;
	besr.p1_blank_lines_at_top = (val_OV0_P1_BLNK_LN_AT_TOP_M1  & 0xfff) |
				     ((val_OV0_P1_ACTIVE_LINES_M1   & 0xfff) << 16);
	besr.p23_blank_lines_at_top = (val_OV0_P23_BLNK_LN_AT_TOP_M1 & 0x7ff) |
				     ((val_OV0_P23_ACTIVE_LINES_M1   & 0x7ff) << 16);
}


static void ComputeXStartEnd(
	    int is_400,
	    uint32_t LeftPixel,uint32_t LeftUVPixel,
	    uint32_t MemWordsInBytes,uint32_t BytesPerPixel,
	    uint32_t SourceWidthInPixels, uint32_t P1StepSize,
	    uint32_t BytesPerUVPixel,uint32_t SourceUVWidthInPixels,
	    uint32_t P23StepSize, uint32_t *p1_x_start, uint32_t *p2_x_start )
{
    uint32_t val_OV0_P1_X_START,val_OV0_P2_X_START,val_OV0_P3_X_START;
    uint32_t val_OV0_P1_X_END,val_OV0_P2_X_END,val_OV0_P3_X_END;
    /* ToDo_Active: At the moment we are not using iOV0_VID_BUF?_START_PIX, but instead		// are using iOV0_P?_X_START and iOV0_P?_X_END. We should use "start pix" and	    // "width" to derive the start and end. */

    val_OV0_P1_X_START = (int)LeftPixel % (MemWordsInBytes/BytesPerPixel);
    val_OV0_P1_X_END = (int)((val_OV0_P1_X_START + SourceWidthInPixels - 1) / P1StepSize) * P1StepSize;

    val_OV0_P2_X_START = val_OV0_P2_X_END = 0;
    switch (besr.surf_id)
    {
	case 9:
	case 10:
	case 13:
	case 14:    /* ToDo_Active: The driver must insure that the initial value is */
		    /* a multiple of a power of two when decimating */
		    val_OV0_P2_X_START = (int)LeftUVPixel %
					    (MemWordsInBytes/BytesPerUVPixel);
		    val_OV0_P2_X_END = (int)((val_OV0_P2_X_START +
			      SourceUVWidthInPixels - 1) / P23StepSize) * P23StepSize;
		    break;
	case 11:
	case 12:    val_OV0_P2_X_START = (int)LeftUVPixel % (MemWordsInBytes/(BytesPerPixel*2));
		    val_OV0_P2_X_END = (int)((val_OV0_P2_X_START + SourceUVWidthInPixels - 1) / P23StepSize) * P23StepSize;
		    break;
	case 3:
	case 4:	    val_OV0_P2_X_START = val_OV0_P1_X_START;
		    /* This value is needed only to allow proper setting of */
		    /* val_OV0_PRESHIFT_P23_TO */
		    /* val_OV0_P2_X_END = 0; */
		    break;
	case 6:	    val_OV0_P2_X_START = (int)LeftPixel % (MemWordsInBytes/BytesPerPixel);
		    val_OV0_P2_X_END = (int)((val_OV0_P1_X_START + SourceWidthInPixels - 1) / P23StepSize) * P23StepSize;
		    break;
	default:    /* insert debug statement here. */
		    RADEON_ASSERT("unknown fourcc\n");
		    break;
    }
    val_OV0_P3_X_START = val_OV0_P2_X_START;
    val_OV0_P3_X_END = val_OV0_P2_X_END;
    
    besr.p1_x_start_end = (val_OV0_P1_X_END&0x7ff) | ((val_OV0_P1_X_START&0x7ff)<<16);
    besr.p2_x_start_end = (val_OV0_P2_X_END&0x7ff) | ((val_OV0_P2_X_START&0x7ff)<<16);
    besr.p3_x_start_end = (val_OV0_P3_X_END&0x7ff) | ((val_OV0_P3_X_START&0x7ff)<<16);
    if(is_400)
    {
	besr.p2_x_start_end = 0;
	besr.p3_x_start_end = 0;
    }
    *p1_x_start = val_OV0_P1_X_START;
    *p2_x_start = val_OV0_P2_X_START;
}

static void ComputeAccumInit(
	    uint32_t val_OV0_P1_X_START,uint32_t val_OV0_P2_X_START,
	    uint32_t val_OV0_P1_H_INC,uint32_t val_OV0_P23_H_INC,
	    uint32_t val_OV0_P1_H_STEP_BY,uint32_t val_OV0_P23_H_STEP_BY,
	    uint32_t CRT_V_INC,
	    uint32_t P1GroupSize, uint32_t P23GroupSize,
	    uint32_t val_OV0_P1_MAX_LN_IN_PER_LN_OUT,
	    uint32_t val_OV0_P23_MAX_LN_IN_PER_LN_OUT)
{
    uint32_t val_OV0_P1_H_ACCUM_INIT,val_OV0_PRESHIFT_P1_TO;
    uint32_t val_OV0_P23_H_ACCUM_INIT,val_OV0_PRESHIFT_P23_TO;
    uint32_t val_OV0_P1_V_ACCUM_INIT,val_OV0_P23_V_ACCUM_INIT;
	/* 2.5 puts the kernal 50% of the way between the source pixel that is off screen */
	/* and the first on-screen source pixel. "(float)valOV0_P?_H_INC / (1<<0xc)" is */
	/* the distance (in source pixel coordinates) to the center of the first */
	/* destination pixel. Need to add additional pixels depending on how many pixels */
	/* are fetched at a time and how many pixels in a set are masked. */
	/* P23 values are always fetched in groups of two or four. If the start */
	/* pixel does not fall on the boundary, then we need to shift preshift for */
	/* some additional pixels */

	{
	    double ExtraHalfPixel;
	    double tempAdditionalShift;
	    double tempP1HStartPoint;
	    double tempP23HStartPoint;
	    double tempP1Init;
	    double tempP23Init;

	    if (besr.horz_pick_nearest) ExtraHalfPixel = 0.5;
	    else			ExtraHalfPixel = 0.0;
	    tempAdditionalShift = val_OV0_P1_X_START % P1GroupSize + ExtraHalfPixel;
	    tempP1HStartPoint = tempAdditionalShift + 2.5 + ((float)val_OV0_P1_H_INC / (1<<0xd));
	    tempP1Init = (double)((int)(tempP1HStartPoint * (1<<0x5) + 0.5)) / (1<<0x5);

	    /* P23 values are always fetched in pairs. If the start pixel is odd, then we */
	    /* need to shift an additional pixel */
	    /* Note that if the pitch is a multiple of two, and if we store fields using */
	    /* the traditional planer format where the V plane and the U plane share the */
	    /* same pitch, then OverlayRegFields->val_OV0_P2_X_START % P23Group */
	    /* OverlayRegFields->val_OV0_P3_X_START % P23GroupSize. Either way */
	    /* it is a requirement that the U and V start on the same polarity byte */
	    /* (even or odd). */
	    tempAdditionalShift = val_OV0_P2_X_START % P23GroupSize + ExtraHalfPixel;
	    tempP23HStartPoint = tempAdditionalShift + 2.5 + ((float)val_OV0_P23_H_INC / (1<<0xd));
	    tempP23Init = (double)((int)(tempP23HStartPoint * (1<<0x5) + 0.5)) / (1 << 0x5);
	    val_OV0_P1_H_ACCUM_INIT = (int)((tempP1Init - (int)tempP1Init) * (1<<0x5));
	    val_OV0_PRESHIFT_P1_TO = (int)tempP1Init;
	    val_OV0_P23_H_ACCUM_INIT = (int)((tempP23Init - (int)tempP23Init) * (1<<0x5));
	    val_OV0_PRESHIFT_P23_TO = (int)tempP23Init;
	}

	/* ************************************************************** */
	/* ** Calculate values for initializing the vertical accumulators */
	/* ************************************************************** */

	{
	    double ExtraHalfLine;
	    double ExtraFullLine;
	    double tempP1VStartPoint;
	    double tempP23VStartPoint;

	    if (besr.vert_pick_nearest) ExtraHalfLine = 0.5;
	    else			ExtraHalfLine = 0.0;

	    if (val_OV0_P1_H_STEP_BY==0)ExtraFullLine = 1.0;
	    else			ExtraFullLine = 0.0;

	    tempP1VStartPoint = 1.5 + ExtraFullLine + ExtraHalfLine + ((float)CRT_V_INC / (1<<0xd));
	    if (tempP1VStartPoint>2.5 + 2*ExtraFullLine)
	    {
		tempP1VStartPoint = 2.5 + 2*ExtraFullLine;
	    }
	    val_OV0_P1_V_ACCUM_INIT = (int)(tempP1VStartPoint * (1<<0x5) + 0.5);

	    if (val_OV0_P23_H_STEP_BY==0)ExtraFullLine = 1.0;
	    else			ExtraFullLine = 0.0;

	    switch (besr.surf_id)
	    {
		case 10:
		case 13:
		case 14:    tempP23VStartPoint = 1.5 + ExtraFullLine + ExtraHalfLine +
						((float)CRT_V_INC / (1<<0xe));
			    break;
		case 9:	    tempP23VStartPoint = 1.5 + ExtraFullLine + ExtraHalfLine +
						((float)CRT_V_INC / (1<<0xf));
			    break;
		case 3:
		case 4:
		case 6:
		case 11:
		case 12:    tempP23VStartPoint = 0;
			    break;
		default:    tempP23VStartPoint = 0xFFFF;/* insert debug statement here */
			    break;
	    }

	    if (tempP23VStartPoint>2.5 + 2*ExtraFullLine)
	    {
		tempP23VStartPoint = 2.5 + 2*ExtraFullLine;
	    }

	    val_OV0_P23_V_ACCUM_INIT = (int)(tempP23VStartPoint * (1<<0x5) + 0.5);
	}
    besr.p1_h_accum_init = ((val_OV0_P1_H_ACCUM_INIT&0x1f)<<15)  |((val_OV0_PRESHIFT_P1_TO&0xf)<<28);
    besr.p1_v_accum_init = (val_OV0_P1_MAX_LN_IN_PER_LN_OUT&0x3) |((val_OV0_P1_V_ACCUM_INIT&0x7ff)<<15);
    besr.p23_h_accum_init= ((val_OV0_P23_H_ACCUM_INIT&0x1f)<<15) |((val_OV0_PRESHIFT_P23_TO&0xf)<<28);
    besr.p23_v_accum_init= (val_OV0_P23_MAX_LN_IN_PER_LN_OUT&0x3)|((val_OV0_P23_V_ACCUM_INIT&0x3ff)<<15);
}

typedef struct RangeAndCoefSet {
    double Range;
    signed char CoefSet[5][4];
} RANGEANDCOEFSET;

/* Filter Setup Routine */
static void FilterSetup ( uint32_t val_OV0_P1_H_INC )
{
    static const RANGEANDCOEFSET ArrayOfSets[] = {
	{0.25, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13,   13,    3}, }},
	{0.26, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.27, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.28, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.29, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.30, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.31, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.32, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.33, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.34, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.35, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.36, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.37, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.38, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.39, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.40, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.41, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.42, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.43, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.44, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.45, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.46, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.47, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.48, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.49, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.50, {{ 7,	16,  9,	 0}, { 7,   16,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 3,	13, 13,	 3}, }},
	{0.51, {{ 7,	17,  8,	 0}, { 6,   17,	 9,  0}, { 5,	15, 11,	 1}, { 4,   15, 12,  1}, { 2,	14, 14,	 2}, }},
	{0.52, {{ 7,	17,  8,	 0}, { 6,   17,	 9,  0}, { 5,	16, 11,	 0}, { 3,   15, 13,  1}, { 2,	14, 14,	 2}, }},
	{0.53, {{ 7,	17,  8,	 0}, { 6,   17,	 9,  0}, { 5,	16, 11,	 0}, { 3,   15, 13,  1}, { 2,	14, 14,	 2}, }},
	{0.54, {{ 7,	17,  8,	 0}, { 6,   17,	 9,  0}, { 4,	17, 11,	 0}, { 3,   15, 13,  1}, { 2,	14, 14,	 2}, }},
	{0.55, {{ 7,	18,  7,	 0}, { 6,   17,	 9,  0}, { 4,	17, 11,	 0}, { 3,   15, 13,  1}, { 1,	15, 15,	 1}, }},
	{0.56, {{ 7,	18,  7,	 0}, { 5,   18,	 9,  0}, { 4,	17, 11,	 0}, { 2,   17, 13,  0}, { 1,	15, 15,	 1}, }},
	{0.57, {{ 7,	18,  7,	 0}, { 5,   18,	 9,  0}, { 4,	17, 11,	 0}, { 2,   17, 13,  0}, { 1,	15, 15,	 1}, }},
	{0.58, {{ 7,	18,  7,	 0}, { 5,   18,	 9,  0}, { 4,	17, 11,	 0}, { 2,   17, 13,  0}, { 1,	15, 15,	 1}, }},
	{0.59, {{ 7,	18,  7,	 0}, { 5,   18,	 9,  0}, { 4,	17, 11,	 0}, { 2,   17, 13,  0}, { 1,	15, 15,	 1}, }},
	{0.60, {{ 7,	18,  8, -1}, { 6,   17, 10, -1}, { 4,	17, 11,	 0}, { 2,   17, 13,  0}, { 1,	15, 15,	 1}, }},
	{0.61, {{ 7,	18,  8, -1}, { 6,   17, 10, -1}, { 4,	17, 11,	 0}, { 2,   17, 13,  0}, { 1,	15, 15,	 1}, }},
	{0.62, {{ 7,	18,  8, -1}, { 6,   17, 10, -1}, { 4,	17, 11,	 0}, { 2,   17, 13,  0}, { 1,	15, 15,	 1}, }},
	{0.63, {{ 7,	18,  8, -1}, { 6,   17, 10, -1}, { 4,	17, 11,	 0}, { 2,   17, 13,  0}, { 1,	15, 15,	 1}, }},
	{0.64, {{ 7,	18,  8, -1}, { 6,   17, 10, -1}, { 4,	17, 12, -1}, { 2,   17, 13,  0}, { 1,	15, 15,	 1}, }},
	{0.65, {{ 7,	18,  8, -1}, { 6,   17, 10, -1}, { 4,	17, 12, -1}, { 2,   17, 13,  0}, { 0,	16, 16,	 0}, }},
	{0.66, {{ 7,	18,  8, -1}, { 6,   18, 10, -2}, { 4,	17, 12, -1}, { 2,   17, 13,  0}, { 0,	16, 16,	 0}, }},
	{0.67, {{ 7,	20,  7, -2}, { 5,   19, 10, -2}, { 3,	18, 12, -1}, { 2,   17, 13,  0}, { 0,	16, 16,	 0}, }},
	{0.68, {{ 7,	20,  7, -2}, { 5,   19, 10, -2}, { 3,	19, 12, -2}, { 1,   18, 14, -1}, { 0,	16, 16,	 0}, }},
	{0.69, {{ 7,	20,  7, -2}, { 5,   19, 10, -2}, { 3,	19, 12, -2}, { 1,   18, 14, -1}, { 0,	16, 16,	 0}, }},
	{0.70, {{ 7,	20,  7, -2}, { 5,   20,	 9, -2}, { 3,	19, 12, -2}, { 1,   18, 14, -1}, { 0,	16, 16,	 0}, }},
	{0.71, {{ 7,	20,  7, -2}, { 5,   20,	 9, -2}, { 3,	19, 12, -2}, { 1,   18, 14, -1}, { 0,	16, 16,	 0}, }},
	{0.72, {{ 7,	20,  7, -2}, { 5,   20,	 9, -2}, { 2,	20, 12, -2}, { 0,   19, 15, -2}, {-1,	17, 17, -1}, }},
	{0.73, {{ 7,	20,  7, -2}, { 4,   21,	 9, -2}, { 2,	20, 12, -2}, { 0,   19, 15, -2}, {-1,	17, 17, -1}, }},
	{0.74, {{ 6,	22,  6, -2}, { 4,   21,	 9, -2}, { 2,	20, 12, -2}, { 0,   19, 15, -2}, {-1,	17, 17, -1}, }},
	{0.75, {{ 6,	22,  6, -2}, { 4,   21,	 9, -2}, { 1,	21, 12, -2}, { 0,   19, 15, -2}, {-1,	17, 17, -1}, }},
	{0.76, {{ 6,	22,  6, -2}, { 4,   21,	 9, -2}, { 1,	21, 12, -2}, { 0,   19, 15, -2}, {-1,	17, 17, -1}, }},
	{0.77, {{ 6,	22,  6, -2}, { 3,   22,	 9, -2}, { 1,	22, 12, -3}, { 0,   19, 15, -2}, {-2,	18, 18, -2}, }},
	{0.78, {{ 6,	21,  6, -1}, { 3,   22,	 9, -2}, { 1,	22, 12, -3}, { 0,   19, 15, -2}, {-2,	18, 18, -2}, }},
	{0.79, {{ 5,	23,  5, -1}, { 3,   22,	 9, -2}, { 0,	23, 12, -3}, {-1,   21, 15, -3}, {-2,	18, 18, -2}, }},
	{0.80, {{ 5,	23,  5, -1}, { 3,   23,	 8, -2}, { 0,	23, 12, -3}, {-1,   21, 15, -3}, {-2,	18, 18, -2}, }},
	{0.81, {{ 5,	23,  5, -1}, { 2,   24,	 8, -2}, { 0,	23, 12, -3}, {-1,   21, 15, -3}, {-2,	18, 18, -2}, }},
	{0.82, {{ 5,	23,  5, -1}, { 2,   24,	 8, -2}, { 0,	23, 12, -3}, {-1,   21, 15, -3}, {-3,	19, 19, -3}, }},
	{0.83, {{ 5,	23,  5, -1}, { 2,   24,	 8, -2}, { 0,	23, 11, -2}, {-2,   22, 15, -3}, {-3,	19, 19, -3}, }},
	{0.84, {{ 4,	25,  4, -1}, { 1,   25,	 8, -2}, { 0,	23, 11, -2}, {-2,   22, 15, -3}, {-3,	19, 19, -3}, }},
	{0.85, {{ 4,	25,  4, -1}, { 1,   25,	 8, -2}, { 0,	23, 11, -2}, {-2,   22, 15, -3}, {-3,	19, 19, -3}, }},
	{0.86, {{ 4,	24,  4,	 0}, { 1,   25,	 7, -1}, {-1,	24, 11, -2}, {-2,   22, 15, -3}, {-3,	19, 19, -3}, }},
	{0.87, {{ 4,	24,  4,	 0}, { 1,   25,	 7, -1}, {-1,	24, 11, -2}, {-2,   22, 15, -3}, {-3,	19, 19, -3}, }},
	{0.88, {{ 3,	26,  3,	 0}, { 0,   26,	 7, -1}, {-1,	24, 11, -2}, {-3,   23, 15, -3}, {-3,	19, 19, -3}, }},
	{0.89, {{ 3,	26,  3,	 0}, { 0,   26,	 7, -1}, {-1,	24, 11, -2}, {-3,   23, 15, -3}, {-3,	19, 19, -3}, }},
	{0.90, {{ 3,	26,  3,	 0}, { 0,   26,	 7, -1}, {-2,	25, 11, -2}, {-3,   23, 15, -3}, {-3,	19, 19, -3}, }},
	{0.91, {{ 3,	26,  3,	 0}, { 0,   27,	 6, -1}, {-2,	25, 11, -2}, {-3,   23, 15, -3}, {-3,	19, 19, -3}, }},
	{0.92, {{ 2,	28,  2,	 0}, { 0,   27,	 6, -1}, {-2,	25, 11, -2}, {-3,   23, 15, -3}, {-3,	19, 19, -3}, }},
	{0.93, {{ 2,	28,  2,	 0}, { 0,   26,	 6,  0}, {-2,	25, 10, -1}, {-3,   23, 15, -3}, {-3,	19, 19, -3}, }},
	{0.94, {{ 2,	28,  2,	 0}, { 0,   26,	 6,  0}, {-2,	25, 10, -1}, {-3,   23, 15, -3}, {-3,	19, 19, -3}, }},
	{0.95, {{ 1,	30,  1,	 0}, {-1,   28,	 5,  0}, {-3,	26, 10, -1}, {-3,   23, 14, -2}, {-3,	19, 19, -3}, }},
	{0.96, {{ 1,	30,  1,	 0}, {-1,   28,	 5,  0}, {-3,	26, 10, -1}, {-3,   23, 14, -2}, {-3,	19, 19, -3}, }},
	{0.97, {{ 1,	30,  1,	 0}, {-1,   28,	 5,  0}, {-3,	26, 10, -1}, {-3,   23, 14, -2}, {-3,	19, 19, -3}, }},
	{0.98, {{ 1,	30,  1,	 0}, {-2,   29,	 5,  0}, {-3,	27,  9, -1}, {-3,   23, 14, -2}, {-3,	19, 19, -3}, }},
	{0.99, {{ 0,	32,  0,	 0}, {-2,   29,	 5,  0}, {-3,	27,  9, -1}, {-4,   24, 14, -2}, {-3,	19, 19, -3}, }},
	{1.00, {{ 0,	32,  0,	 0}, {-2,   29,	 5,  0}, {-3,	27,  9, -1}, {-4,   24, 14, -2}, {-3,	19, 19, -3}, }}
    };

    double DSR;

    unsigned ArrayElement;

    DSR = (double)(1<<0xc)/val_OV0_P1_H_INC;
    if (DSR<.25) DSR=.25;
    if (DSR>1) DSR=1;

    ArrayElement = (int)((DSR-0.25) * 100);
    besr.four_tap_coeff[0] =	 (ArrayOfSets[ArrayElement].CoefSet[0][0] & 0xf) |
				((ArrayOfSets[ArrayElement].CoefSet[0][1] & 0x7f)<<8) |
				((ArrayOfSets[ArrayElement].CoefSet[0][2] & 0x7f)<<16) |
				((ArrayOfSets[ArrayElement].CoefSet[0][3] & 0xf)<<24);
    besr.four_tap_coeff[1] =	 (ArrayOfSets[ArrayElement].CoefSet[1][0] & 0xf) |
				((ArrayOfSets[ArrayElement].CoefSet[1][1] & 0x7f)<<8) |
				((ArrayOfSets[ArrayElement].CoefSet[1][2] & 0x7f)<<16) |
				((ArrayOfSets[ArrayElement].CoefSet[1][3] & 0xf)<<24);
    besr.four_tap_coeff[2] =	 (ArrayOfSets[ArrayElement].CoefSet[2][0] & 0xf) |
				((ArrayOfSets[ArrayElement].CoefSet[2][1] & 0x7f)<<8) |
				((ArrayOfSets[ArrayElement].CoefSet[2][2] & 0x7f)<<16) |
				((ArrayOfSets[ArrayElement].CoefSet[2][3] & 0xf)<<24);
    besr.four_tap_coeff[3] =	 (ArrayOfSets[ArrayElement].CoefSet[3][0] & 0xf) |
				((ArrayOfSets[ArrayElement].CoefSet[3][1] & 0x7f)<<8) |
				((ArrayOfSets[ArrayElement].CoefSet[3][2] & 0x7f)<<16) |
				((ArrayOfSets[ArrayElement].CoefSet[3][3] & 0xf)<<24);
    besr.four_tap_coeff[4] =	 (ArrayOfSets[ArrayElement].CoefSet[4][0] & 0xf) |
				((ArrayOfSets[ArrayElement].CoefSet[4][1] & 0x7f)<<8) |
				((ArrayOfSets[ArrayElement].CoefSet[4][2] & 0x7f)<<16) |
				((ArrayOfSets[ArrayElement].CoefSet[4][3] & 0xf)<<24);
/*
    For more details, refer to Microsoft's draft of PC99.
*/
}

/* The minimal value of horizontal scale ratio when hard coded coefficients
   are suitable for the best quality. */
/* FIXME: Should it be 0.9 for Rage128 ??? */
static const double MinHScaleHard=0.75;

static int radeon_vid_init_video( vidix_playback_t *config )
{
    double V_scale_ratio;
    uint32_t i,src_w,src_h,dest_w,dest_h,pitch,left,leftUV,top,h_inc;
    uint32_t val_OV0_P1_H_INC=0,val_OV0_P1_H_STEP_BY=0,val_OV0_P23_H_INC=0,val_OV0_P23_H_STEP_BY=0;
    uint32_t val_OV0_P1_X_START,val_OV0_P2_X_START;
    uint32_t val_OV0_P1_MAX_LN_IN_PER_LN_OUT,val_OV0_P23_MAX_LN_IN_PER_LN_OUT;
    uint32_t CRT_V_INC;
    uint32_t BytesPerOctWord,LogMemWordsInBytes,MemWordsInBytes,LogTileWidthInMemWords;
    uint32_t TileWidthInMemWords,TileWidthInBytes,LogTileHeight,TileHeight;
    uint32_t PageSizeInBytes,OV0LB_Rows;
    uint32_t SourceWidthInMemWords,SourceUVWidthInMemWords;
    uint32_t SourceWidthInPixels,SourceUVWidthInPixels;
    uint32_t RightPixel,RightUVPixel,LeftPixel,LeftUVPixel;
    int is_400,is_410,is_420,best_pitch,mpitch;
    int horz_repl_factor,interlace_factor;
    int BytesPerPixel,BytesPerUVPixel,HorzUVSubSample,VertUVSubSample;
    int DisallowFourTapVertFiltering,DisallowFourTapUVVertFiltering;

    radeon_vid_stop_video();
    left = config->src.x << 16;
    top =  config->src.y << 16;
    src_h = config->src.h;
    src_w = config->src.w;
    is_400 = is_410 = is_420 = 0;
    if(config->fourcc == IMGFMT_YV12 ||
       config->fourcc == IMGFMT_I420 ||
       config->fourcc == IMGFMT_IYUV) is_420 = 1;
    if(config->fourcc == IMGFMT_YVU9 ||
       config->fourcc == IMGFMT_IF09) is_410 = 1;
    if(config->fourcc == IMGFMT_Y800) is_400 = 1;
    best_pitch = radeon_query_pitch(config->fourcc,&config->src.pitch);
    mpitch = best_pitch-1;
    BytesPerOctWord = 16;
    LogMemWordsInBytes = 4;
    MemWordsInBytes = 1<<LogMemWordsInBytes;
    LogTileWidthInMemWords = 2;
    TileWidthInMemWords = 1<<LogTileWidthInMemWords;
    TileWidthInBytes = 1<<(LogTileWidthInMemWords+LogMemWordsInBytes);
    LogTileHeight = 4;
    TileHeight = 1<<LogTileHeight;
    PageSizeInBytes = 64*MemWordsInBytes;
    OV0LB_Rows = 96;
    h_inc = 1;
    switch(config->fourcc)
    {
	/* 4:0:0*/
	case IMGFMT_Y800:
	/* 4:1:0*/
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
    besr.load_prg_start=0;
    besr.swap_uv=0;
    switch(config->fourcc)
    {
	case IMGFMT_RGB15:
			   besr.swap_uv=1;
	case IMGFMT_BGR15: besr.surf_id = SCALER_SOURCE_15BPP>>8;
			   besr.load_prg_start = 1;
			   break;
	case IMGFMT_RGB16:
			   besr.swap_uv=1;
	case IMGFMT_BGR16: besr.surf_id = SCALER_SOURCE_16BPP>>8;
			   besr.load_prg_start = 1;
			   break;
	case IMGFMT_RGB32:
			   besr.swap_uv=1;
	case IMGFMT_BGR32: besr.surf_id = SCALER_SOURCE_32BPP>>8;
			   besr.load_prg_start = 1;
			   break;
	/* 4:1:0*/
	case IMGFMT_IF09:
	case IMGFMT_YVU9:  besr.surf_id = SCALER_SOURCE_YUV9>>8;
			   break;
	/* 4:0:0*/
	case IMGFMT_Y800:
	/* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:  besr.surf_id = SCALER_SOURCE_YUV12>>8;
			   break;
	/* 4:2:2 */
	case IMGFMT_YVYU:
	case IMGFMT_UYVY:  besr.surf_id = SCALER_SOURCE_YVYU422>>8;
			   break;
	case IMGFMT_YUY2:
	default:	   besr.surf_id = SCALER_SOURCE_VYUY422>>8;
			   break;
    }
    switch (besr.surf_id)
    {
	case 3:
	case 4:
	case 11:
	case 12:    BytesPerPixel = 2;
		    break;
	case 6:	    BytesPerPixel = 4;
		    break;
	case 9:
	case 10:
	case 13:
	case 14:    BytesPerPixel = 1;
		    break;
	default:    BytesPerPixel = 0;/*insert a debug statement here. */
		    break;
    }
    switch (besr.surf_id)
    {
	case 3:
	case 4:	    BytesPerUVPixel = 0;
		    break;/* In RGB modes, the BytesPerUVPixel is don't care */
	case 11:
	case 12:    BytesPerUVPixel = 2;
		    break;
	case 6:	    BytesPerUVPixel = 0;
		    break;	/* In RGB modes, the BytesPerUVPixel is don't care */
	case 9:
	case 10:    BytesPerUVPixel = 1;
		    break;
	case 13:
	case 14:    BytesPerUVPixel = 2;
		    break;
	default:    BytesPerUVPixel = 0;/* insert a debug statement here. */
		    break;

    }
    switch (besr.surf_id)
    {
	case 3:
	case 4:
	case 6:	    HorzUVSubSample = 1;
		    break;
	case 9:	    HorzUVSubSample = 4;
		    break;
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:    HorzUVSubSample = 2;
		    break;
	default:    HorzUVSubSample = 0;/* insert debug statement here. */
		    break;
    }
    switch (besr.surf_id)
    {
	case 3:
	case 4:
	case 6:
	case 11:
	case 12:    VertUVSubSample = 1;
		    break;
	case 9:	    VertUVSubSample = 4;
		    break;
	case 10:
	case 13:
	case 14:    VertUVSubSample = 2;
		    break;
	default:    VertUVSubSample = 0;/* insert debug statment here. */
		    break;
    }
    DisallowFourTapVertFiltering = 0;	    /* Allow it by default */
    DisallowFourTapUVVertFiltering = 0;	    /* Allow it by default */
    LeftPixel = config->src.x;
    RightPixel = config->src.w-1;
    if(floor(config->src.x/HorzUVSubSample)<0)	LeftUVPixel = 0;
    else						LeftUVPixel = (int)floor(config->src.x/HorzUVSubSample);
    if(ceil((config->src.x+config->src.w)/HorzUVSubSample) > config->src.w/HorzUVSubSample)
		RightUVPixel = config->src.w/HorzUVSubSample - 1;
    else	RightUVPixel = (int)ceil((config->src.x+config->src.w)/HorzUVSubSample) - 1;
    /* Top, Bottom and Right Crops can be out of range. The driver will program the hardware
    // to create a black border at the top and bottom. This is useful for DVD letterboxing. */
    SourceWidthInPixels = (int)(config->src.w + 1);
    SourceUVWidthInPixels = (int)(RightUVPixel - LeftUVPixel + 1);

    SourceWidthInMemWords = (int)(ceil(RightPixel*BytesPerPixel / MemWordsInBytes) -
			    floor(LeftPixel*BytesPerPixel / MemWordsInBytes) + 1);
    /* SourceUVWidthInMemWords means Source_U_or_V_or_UV_WidthInMemWords depending on whether the UV is packed together of not. */
    SourceUVWidthInMemWords = (int)(ceil(RightUVPixel*BytesPerUVPixel /
			      MemWordsInBytes) - floor(LeftUVPixel*BytesPerUVPixel /
			      MemWordsInBytes) + 1);

    switch (besr.surf_id)
    {
	case 9:
	case 10:    if ((ceil(SourceWidthInMemWords/2)-1) * 2 > OV0LB_Rows-1)
		    {
			RADEON_ASSERT("ceil(SourceWidthInMemWords/2)-1) * 2 > OV0LB_Rows-1\n");
		    }
		    else if ((SourceWidthInMemWords-1) * 2 > OV0LB_Rows-1)
		    {
			DisallowFourTapVertFiltering = 1;
		    }

		    if ((ceil(SourceUVWidthInMemWords/2)-1) * 4 + 1 > OV0LB_Rows-1)
		    {
			/*CYCACC_ASSERT(0, "Image U plane width spans more octwords than supported by hardware.") */
		    }
		    else if ((SourceUVWidthInMemWords-1) * 4 + 1 > OV0LB_Rows-1)
		    {
			DisallowFourTapUVVertFiltering = 1;
		    }

		    if ((ceil(SourceUVWidthInMemWords/2)-1) * 4 + 3 > OV0LB_Rows-1)
		    {
			/*CYCACC_ASSERT(0, "Image V plane width spans more octwords than supported by hardware.") */
		    }
		    else if ((SourceUVWidthInMemWords-1) * 4 + 3 > OV0LB_Rows-1)
		    {
			DisallowFourTapUVVertFiltering = 1;
		    }
		    break;
	case 13:
	case 14:    if ((ceil(SourceWidthInMemWords/2)-1) * 2 > OV0LB_Rows-1)
		    {
			RADEON_ASSERT("ceil(SourceWidthInMemWords/2)-1) * 2 > OV0LB_Rows-1\n");
		    }
		    else if ((SourceWidthInMemWords-1) * 2 > OV0LB_Rows-1)
		    {
			DisallowFourTapVertFiltering = 1;
		    }

		    if ((ceil(SourceUVWidthInMemWords/2)-1) * 2 + 1 > OV0LB_Rows-1)
		    {
			/*CYCACC_ASSERT(0, "Image UV plane width spans more octwords than supported by hardware.") */
		    }
		    else if ((SourceUVWidthInMemWords-1) * 2 + 1 > OV0LB_Rows-1)
		    {
			DisallowFourTapUVVertFiltering = 1;
		    }
		    break;
	case 3:
	case 4:
	case 6:
	case 11:
	case 12:    if ((ceil(SourceWidthInMemWords/2)-1) > OV0LB_Rows-1)
		    {
			RADEON_ASSERT("(ceil(SourceWidthInMemWords/2)-1) > OV0LB_Rows-1\n")
		    }
		    else if ((SourceWidthInMemWords-1) > OV0LB_Rows-1)
		    {
			DisallowFourTapVertFiltering = 1;
		    }
		    break;
	default:    /* insert debug statement here. */
		    break;
    }
    dest_w = config->dest.w;
    dest_h = config->dest.h;
    if(radeon_is_dbl_scan()) dest_h *= 2;
    besr.dest_bpp = radeon_vid_get_dbpp();
    besr.fourcc = config->fourcc;
    if(radeon_is_interlace())	interlace_factor = 2;
    else			interlace_factor = 1;
    /* TODO: must be checked in doublescan mode!!! */
    if((besr.chip_flags&R_INTEGRATED)==R_INTEGRATED)
    {
	/* Force the overlay clock on for integrated chips */
        OUTPLL(VCLK_ECP_CNTL, (INPLL(VCLK_ECP_CNTL) | (1<<18)));
    }
    horz_repl_factor = 1 << (uint32_t)((INPLL(VCLK_ECP_CNTL) & 0x300) >> 8);
    H_scale_ratio = (double)ceil(((double)dest_w+1)/horz_repl_factor)/src_w;
    V_scale_ratio = (double)(dest_h+1)/src_h;
    if(H_scale_ratio < 0.5 && V_scale_ratio < 0.5)
    {
	val_OV0_P1_MAX_LN_IN_PER_LN_OUT = 3;
	val_OV0_P23_MAX_LN_IN_PER_LN_OUT = 2;
    }
    else
    if(H_scale_ratio < 1 && V_scale_ratio < 1)
    {
	val_OV0_P1_MAX_LN_IN_PER_LN_OUT = 2;
	val_OV0_P23_MAX_LN_IN_PER_LN_OUT = 1;
    }
    else
    {
	val_OV0_P1_MAX_LN_IN_PER_LN_OUT = 1;
	val_OV0_P23_MAX_LN_IN_PER_LN_OUT = 1;
    }
    /* N.B.: Indeed it has 6.12 format but shifted on 8 to the left!!! */
    besr.v_inc = (uint16_t)((1./V_scale_ratio)*(1<<12)*interlace_factor+0.5);
    CRT_V_INC = besr.v_inc/interlace_factor;
    besr.v_inc <<= 8;
    {
	int ThereIsTwoTapVerticalFiltering,DoNotUseMostRecentlyFetchedLine;
	int P1GroupSize = 0;
	int P23GroupSize;
	int P1StepSize = 0;
	int P23StepSize = 0;

	Calc_H_INC_STEP_BY(
	    besr.surf_id,
	    H_scale_ratio,
	    DisallowFourTapVertFiltering,
	    DisallowFourTapUVVertFiltering,
	    &val_OV0_P1_H_INC,
	    &val_OV0_P1_H_STEP_BY,
	    &val_OV0_P23_H_INC,
	    &val_OV0_P23_H_STEP_BY,
	    &P1GroupSize,
	    &P1StepSize,
	    &P23StepSize);

	if(H_scale_ratio > MinHScaleHard)
	{
	    h_inc = (src_w << 12) / dest_w;
	    besr.step_by = 0x0101;
	    switch (besr.surf_id)
	    {
		case 3:
		case 4:
		case 6:
			besr.h_inc = (h_inc)|(h_inc<<16);
			break;
		case 9:
			besr.h_inc = h_inc | ((h_inc >> 2) << 16);
			break;
		default:
			besr.h_inc = h_inc | ((h_inc >> 1) << 16);
			break;
	    }
	}

	P23GroupSize = 2;	/* Current vaue for all modes */

	besr.horz_pick_nearest=0;
	DoNotUseMostRecentlyFetchedLine=0;
	ThereIsTwoTapVerticalFiltering = (val_OV0_P1_H_STEP_BY!=0) || (val_OV0_P23_H_STEP_BY!=0);
	if (ThereIsTwoTapVerticalFiltering && DoNotUseMostRecentlyFetchedLine)
				besr.vert_pick_nearest = 1;
	else
				besr.vert_pick_nearest = 0;

	ComputeXStartEnd(is_400,LeftPixel,LeftUVPixel,MemWordsInBytes,BytesPerPixel,
		     SourceWidthInPixels,P1StepSize,BytesPerUVPixel,
		     SourceUVWidthInPixels,P23StepSize,&val_OV0_P1_X_START,&val_OV0_P2_X_START);

	if(H_scale_ratio > MinHScaleHard)
	{
	    unsigned tmp;
	    tmp = (left & 0x0003ffff) + 0x00028000 + (h_inc << 3);
	    besr.p1_h_accum_init = ((tmp <<  4) & 0x000f8000) |
				    ((tmp << 12) & 0xf0000000);

	    tmp = (top & 0x0000ffff) + 0x00018000;
	    besr.p1_v_accum_init = ((tmp << 4) & OV0_P1_V_ACCUM_INIT_MASK)
				    |(OV0_P1_MAX_LN_IN_PER_LN_OUT & 1);
	    tmp = ((left >> 1) & 0x0001ffff) + 0x00028000 + (h_inc << 2);
	    besr.p23_h_accum_init = ((tmp << 4) & 0x000f8000) |
				    ((tmp << 12) & 0x70000000);

	    tmp = ((top >> 1) & 0x0000ffff) + 0x00018000;
	    besr.p23_v_accum_init = (is_420||is_410) ? 
				    ((tmp << 4) & OV0_P23_V_ACCUM_INIT_MASK)
				    |(OV0_P23_MAX_LN_IN_PER_LN_OUT & 1) : 0;
	}
	else
	    ComputeAccumInit(	val_OV0_P1_X_START,val_OV0_P2_X_START,
				val_OV0_P1_H_INC,val_OV0_P23_H_INC,
				val_OV0_P1_H_STEP_BY,val_OV0_P23_H_STEP_BY,
				CRT_V_INC,P1GroupSize,P23GroupSize,
				val_OV0_P1_MAX_LN_IN_PER_LN_OUT,
				val_OV0_P23_MAX_LN_IN_PER_LN_OUT);
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
	else /* is_410 */
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
    leftUV = (left >> (is_410?18:17)) & 15;
    left = (left >> 16) & 15;
    besr.y_x_start = (config->dest.x+X_ADJUST) | (config->dest.y << 16);
    besr.y_x_end = (config->dest.x + dest_w+X_ADJUST) | ((config->dest.y + dest_h) << 16);
    ComputeBorders(config,VertUVSubSample);
    besr.vid_buf_pitch0_value = pitch;
    besr.vid_buf_pitch1_value = is_410 ? pitch>>2 : is_420 ? pitch>>1 : pitch;
    /* ********************************************************* */
    /* ** Calculate programmable coefficients as needed		 */
    /* ********************************************************* */

    /* ToDo_Active: When in pick nearest mode, we need to program the filter tap zero */
    /* coefficients to 0, 32, 0, 0. Or use hard coded coefficients. */
    if(H_scale_ratio > MinHScaleHard) besr.filter_cntl |= FILTER_HARDCODED_COEF;
    else
    {
	FilterSetup (val_OV0_P1_H_INC);
	/* ToDo_Active: Must add the smarts into the driver to decide what type of filtering it */
	/* would like to do. For now, we let the test application decide. */
	besr.filter_cntl = FILTER_PROGRAMMABLE_COEF;
	if(DisallowFourTapVertFiltering)
	    besr.filter_cntl |= FILTER_HARD_SCALE_VERT_Y;
	if(DisallowFourTapUVVertFiltering)
	    besr.filter_cntl |= FILTER_HARD_SCALE_VERT_UV;
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
    case IMGFMT_Y800:
		awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*info->src.h;
		break;
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
		awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*(info->src.h+info->src.h/8);
		break;
    case IMGFMT_I420:
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
		awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*(info->src.h+info->src.h/2);
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
  info->frame_size = (info->frame_size+4095)&~4095;
}

static int radeon_config_playback(vidix_playback_t *info)
{
  unsigned rgb_size,nfr;
  uint32_t radeon_video_size;
  if(!is_supported_fourcc(info->fourcc)) return ENOSYS;
  if(info->num_frames>VID_PLAY_MAXFRAMES) info->num_frames=VID_PLAY_MAXFRAMES;
  if(info->num_frames==1) besr.double_buff=0;
  else			  besr.double_buff=1;
  radeon_compute_framesize(info);
    
  rgb_size = radeon_get_xres()*radeon_get_yres()*((radeon_vid_get_dbpp()+7)/8);
  nfr = info->num_frames;
  radeon_video_size = radeon_ram_size;
  for(;nfr>0; nfr--)
  {
      radeon_overlay_off = radeon_video_size - info->frame_size*nfr;
#if !defined (RAGE128) && defined(CONFIG_X11)
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
      radeon_overlay_off = radeon_video_size - info->frame_size*nfr;
#if !defined (RAGE128) && defined(CONFIG_X11)
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

static int radeon_playback_on(void)
{
#ifdef RAGE128
  unsigned dw,dh;
#endif
  radeon_vid_display_video();
#ifdef RAGE128
  dh = (besr.y_x_end >> 16) - (besr.y_x_start >> 16);
  dw = (besr.y_x_end & 0xFFFF) - (besr.y_x_start & 0xFFFF);
  if(dw == radeon_get_xres() || dh == radeon_get_yres()) radeon_vid_exclusive();
  else radeon_vid_non_exclusive();
#endif
  return 0;
}

static int radeon_playback_off(void)
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
    if(verbosity > VERBOSE_LEVEL) radeon_vid_dump_regs();
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

static int radeon_get_eq(vidix_video_eq_t * eq)
{
  memcpy(eq,&equal,sizeof(vidix_video_eq_t));
  return 0;
}

#ifndef RAGE128
#define RTFSaturation(a)   (1.0 + ((a)*1.0)/1000.0)
#define RTFBrightness(a)   (((a)*1.0)/2000.0)
#define RTFIntensity(a)	   (((a)*1.0)/2000.0)
#define RTFContrast(a)	 (1.0 + ((a)*1.0)/1000.0)
#define RTFHue(a)   (((a)*3.1416)/1000.0)
#define RTFCheckParam(a) {if((a)<-1000) (a)=-1000; if((a)>1000) (a)=1000;}
#endif

static int radeon_set_eq(const vidix_video_eq_t * eq)
{
#ifdef RAGE128
  int br,sat;
#else
  int itu_space;
#endif
    if(eq->cap & VEQ_CAP_BRIGHTNESS) equal.brightness = eq->brightness;
    if(eq->cap & VEQ_CAP_CONTRAST)   equal.contrast   = eq->contrast;
    if(eq->cap & VEQ_CAP_SATURATION) equal.saturation = eq->saturation;
    if(eq->cap & VEQ_CAP_HUE)	     equal.hue	      = eq->hue;
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
    sat = (equal.saturation*31 + 31000) / 2000;
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

static int radeon_playback_set_deint(const vidix_deinterlace_t * info)
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

static int radeon_playback_get_deint(vidix_deinterlace_t * info)
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

static int set_gr_key( void )
{
    int result = 0;

    besr.merge_cntl = 0xff000000 | /* overlay alpha */
		      0x00ff0000;  /* graphic alpha */
    if(radeon_grkey.ckey.op == CKEY_TRUE)
    {
	int dbpp=radeon_vid_get_dbpp();
	besr.ckey_on=1;

	switch(dbpp)
	{
	case 15:
#ifndef RAGE128
		if((besr.chip_flags&R_100)!=R_100)
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
#ifndef RAGE128
		/* This test may be too general/specific */
		if((besr.chip_flags&R_100)!=R_100)
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
    else if(radeon_grkey.ckey.op == CKEY_ALPHA)
    {
	int dbpp=radeon_vid_get_dbpp();
	besr.ckey_on=1;

	switch(dbpp)
	{
	case 32:
		besr.ckey_on=1;
		besr.graphics_key_msk=0;
		besr.graphics_key_clr=0;
		besr.ckey_cntl = VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND;
		besr.merge_cntl |= 0x00000001; /* DISP_ALPHA_MODE_PER_PIXEL */
		break;
	default:
		besr.ckey_on=0;
		besr.graphics_key_msk=0;
		besr.graphics_key_clr=0;
		besr.ckey_cntl = VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND;
		result = 1;
	}
    }
    else
    {
	besr.ckey_on=0;
	besr.graphics_key_msk=0;
	besr.graphics_key_clr=0;
	besr.ckey_cntl = VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND;
	besr.merge_cntl |= 0x00000100;  /* DISP_RGB_OFFSET_EN */
    }
    radeon_fifo_wait(3);
    OUTREG(OV0_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
    OUTREG(OV0_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
    OUTREG(OV0_KEY_CNTL,besr.ckey_cntl);
    OUTREG(DISP_MERGE_CNTL, besr.merge_cntl);
    return result;
}

static int radeon_get_gkey(vidix_grkey_t *grkey)
{
    memcpy(grkey, &radeon_grkey, sizeof(vidix_grkey_t));
    return 0;
}

static int radeon_set_gkey(const vidix_grkey_t *grkey)
{
    memcpy(&radeon_grkey, grkey, sizeof(vidix_grkey_t));
    return set_gr_key();
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
