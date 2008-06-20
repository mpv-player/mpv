/*
 * VIDIX driver for ATI Mach64 and 3DRage chipsets.
 *
 * Copyright (C) 2002 Nick Kurshev
 * This file is based on sources from
 *  GATOS (gatos.sf.net) and X11 (www.xfree86.org)
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
#include <fcntl.h>

#include "config.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "vidix.h"
#include "fourcc.h"
#include "dha.h"
#include "pci_ids.h"
#include "pci_names.h"

#include "mach64.h"

#define UNUSED(x) ((void)(x)) /**< Removes warning about unused arguments */

static void *mach64_mmio_base = 0;
static void *mach64_mem_base = 0;
static int32_t mach64_overlay_offset = 0;
static uint32_t mach64_ram_size = 0;
static uint32_t mach64_buffer_base[10][3];
static int num_mach64_buffers=-1;
static int supports_planar=0;
static int supports_lcd_v_stretch=0;

static pciinfo_t pci_info;
static int probed = 0;
static int verbosity = 0;

#define VERBOSE_LEVEL 1

typedef struct bes_registers_s
{
  /* base address of yuv framebuffer */
  uint32_t yuv_base;
  uint32_t fourcc;
  /* YUV BES registers */
  uint32_t reg_load_cntl;
  uint32_t scale_inc;
  uint32_t y_x_start;
  uint32_t y_x_end;
  uint32_t vid_buf_pitch;
  uint32_t height_width;

  uint32_t scale_cntl;
  uint32_t exclusive_horz;
  uint32_t auto_flip_cntl;
  uint32_t filter_cntl;
  uint32_t key_cntl;
  uint32_t test;
  /* Configurable stuff */
  
  int brightness;
  int saturation;
  
  int ckey_on;
  uint32_t graphics_key_clr;
  uint32_t graphics_key_msk;
  
  int deinterlace_on;
  uint32_t deinterlace_pattern;
  
} bes_registers_t;

static bes_registers_t besr;

typedef struct video_registers_s
{
  const char * sname;
  uint32_t name;
  uint32_t value;
}video_registers_t;

/* Graphic keys */
static vidix_grkey_t mach64_grkey;

#define DECLARE_VREG(name) { #name, name, 0 }
static video_registers_t vregs[] = 
{
  DECLARE_VREG(OVERLAY_SCALE_INC),
  DECLARE_VREG(OVERLAY_Y_X_START),
  DECLARE_VREG(OVERLAY_Y_X_END),
  DECLARE_VREG(OVERLAY_SCALE_CNTL),
  DECLARE_VREG(OVERLAY_EXCLUSIVE_HORZ),
  DECLARE_VREG(OVERLAY_EXCLUSIVE_VERT),
  DECLARE_VREG(OVERLAY_TEST),
  DECLARE_VREG(SCALER_BUF_PITCH),
  DECLARE_VREG(SCALER_HEIGHT_WIDTH),
  DECLARE_VREG(SCALER_BUF0_OFFSET),
  DECLARE_VREG(SCALER_BUF0_OFFSET_U),
  DECLARE_VREG(SCALER_BUF0_OFFSET_V),
  DECLARE_VREG(SCALER_BUF1_OFFSET),
  DECLARE_VREG(SCALER_BUF1_OFFSET_U),
  DECLARE_VREG(SCALER_BUF1_OFFSET_V),
  DECLARE_VREG(SCALER_H_COEFF0),
  DECLARE_VREG(SCALER_H_COEFF1),
  DECLARE_VREG(SCALER_H_COEFF2),
  DECLARE_VREG(SCALER_H_COEFF3),
  DECLARE_VREG(SCALER_H_COEFF4),
  DECLARE_VREG(SCALER_COLOUR_CNTL),
  DECLARE_VREG(SCALER_THRESHOLD),
  DECLARE_VREG(VIDEO_FORMAT),
  DECLARE_VREG(VIDEO_CONFIG),
  DECLARE_VREG(VIDEO_SYNC_TEST),
  DECLARE_VREG(VIDEO_SYNC_TEST_B)
};

/* have to restore it on exit */
static uint32_t SAVED_OVERLAY_GRAPHICS_KEY_CLR;

/* VIDIX exports */

/* MMIO space*/
#define GETREG(TYPE,PTR,OFFZ)		(*((volatile TYPE*)((PTR)+(OFFZ))))
#define SETREG(TYPE,PTR,OFFZ,VAL)	(*((volatile TYPE*)((PTR)+(OFFZ))))=VAL

#define INREG8(addr)		GETREG(uint8_t,(uint8_t *)mach64_mmio_base,((addr)^0x100)<<2)
#define OUTREG8(addr,val)	SETREG(uint8_t,(uint8_t *)mach64_mmio_base,((addr)^0x100)<<2,val)

static inline uint32_t INREG (uint32_t addr) {
	uint32_t tmp = GETREG(uint32_t,(uint8_t *)mach64_mmio_base,((addr)^0x100)<<2);
	return le2me_32(tmp);
}
#define OUTREG(addr,val)	SETREG(uint32_t,(uint8_t *)mach64_mmio_base,((addr)^0x100)<<2,le2me_32(val))

#define OUTREGP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INREG(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTREG(addr, _tmp);					\
	} while (0)

static __inline__ int ATIGetMach64LCDReg(int _Index)
{
        OUTREG8(LCD_INDEX, _Index);
        return INREG(LCD_DATA);
}

static __inline__ uint32_t INPLL(uint32_t addr)
{
    uint32_t res;
    uint32_t in;
    
    in= INREG(CLOCK_CNTL);
    in &= ~((PLL_WR_EN | PLL_ADDR)); //clean some stuff
    OUTREG(CLOCK_CNTL, in | (addr<<10));
    
    /* read the register value */
    res = (INREG(CLOCK_CNTL)>>16)&0xFF;
    return res;
}

static __inline__ void OUTPLL(uint32_t addr,uint32_t val)
{
//FIXME buggy but its not used
    /* write addr byte */
    OUTREG8(CLOCK_CNTL + 1, (addr << 2) | PLL_WR_EN);
    /* write the register value */
    OUTREG(CLOCK_CNTL + 2, val);
    OUTREG8(CLOCK_CNTL + 1, (addr << 2) & ~PLL_WR_EN);
}

#define OUTPLLP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INPLL(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTPLL(addr, _tmp);					\
	} while (0)

static void mach64_fifo_wait(unsigned n) 
{
    while ((INREG(FIFO_STAT) & 0xffff) > ((uint32_t)(0x8000 >> n)));
}

static void mach64_wait_for_idle( void ) 
{
    mach64_fifo_wait(16);
    while ((INREG(GUI_STAT) & 1)!= 0);
}

static void mach64_wait_vsync( void )
{
    int i;

    for(i=0; i<2000000; i++)
	if( (INREG(CRTC_INT_CNTL)&CRTC_VBLANK)==0 ) break;
    for(i=0; i<2000000; i++)
	if( (INREG(CRTC_INT_CNTL)&CRTC_VBLANK) ) break;

}

static vidix_capability_t mach64_cap =
{
    "BES driver for Mach64/3DRage cards",
    "Nick Kurshev and Michael Niedermayer",
    TYPE_OUTPUT,
    { 0, 0, 0, 0 },
    2048,
    2048,
    4,
    4,
    -1,
    FLAG_UPSCALER|FLAG_DOWNSCALER,
    VENDOR_ATI,
    -1,
    { 0, 0, 0, 0 }
};

static uint32_t mach64_vid_get_dbpp( void )
{
  uint32_t dbpp,retval;
  dbpp = (INREG(CRTC_GEN_CNTL)>>8)& 0x7;
  switch(dbpp)
  {
    case 1: retval = 4; break;
    case 2: retval = 8; break;
    case 3: retval = 15; break;
    case 4: retval = 16; break;
    case 5: retval = 24; break;
    default: retval=32; break;
  }
  return retval;
}

static int mach64_is_dbl_scan( void )
{
  return INREG(CRTC_GEN_CNTL) & CRTC_DBL_SCAN_EN;
}

static int mach64_is_interlace( void )
{
  return INREG(CRTC_GEN_CNTL) & CRTC_INTERLACE_EN;
}

static uint32_t mach64_get_xres( void )
{
  /* FIXME: currently we extract that from CRTC!!!*/
  uint32_t xres,h_total;
  h_total = INREG(CRTC_H_TOTAL_DISP);
  xres = (h_total >> 16) & 0xffff;
  return (xres + 1)*8;
}

static uint32_t mach64_get_yres( void )
{
  /* FIXME: currently we extract that from CRTC!!!*/
  uint32_t yres,v_total;
  v_total = INREG(CRTC_V_TOTAL_DISP);
  yres = (v_total >> 16) & 0xffff;
  return yres + 1;
}

// returns the verical stretch factor in 16.16
static int mach64_get_vert_stretch(void)
{
    int lcd_index;
    int vert_stretching;
    int ext_vert_stretch;
    int ret;
    int yres= mach64_get_yres();

    if(!supports_lcd_v_stretch){
        if(verbosity > 0) printf("[mach64] vertical stretching not supported\n");
        return 1<<16;
    }

    lcd_index= INREG(LCD_INDEX);
    
    vert_stretching= ATIGetMach64LCDReg(LCD_VERT_STRETCHING);
    if(!(vert_stretching&VERT_STRETCH_EN)) ret= 1<<16;
    else
    {
    	int panel_size;
        
	ext_vert_stretch= ATIGetMach64LCDReg(LCD_EXT_VERT_STRETCH);
	panel_size= (ext_vert_stretch&VERT_PANEL_SIZE)>>11;
	panel_size++;
	
	ret= ((yres<<16) + (panel_size>>1))/panel_size;
    }
      
//    lcd_gen_ctrl = ATIGetMach64LCDReg(LCD_GEN_CNTL);
    
    OUTREG(LCD_INDEX, lcd_index);
    
    if(verbosity > 0) printf("[mach64] vertical stretching factor= %d\n", ret);
    
    return ret;
}

static void mach64_vid_make_default(void)
{
  mach64_fifo_wait(5);
  OUTREG(SCALER_COLOUR_CNTL,0x00101000);

  besr.ckey_on=0;
  besr.graphics_key_msk=0;
  besr.graphics_key_clr=0;

  OUTREG(OVERLAY_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
  OUTREG(OVERLAY_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
  OUTREG(OVERLAY_KEY_CNTL,VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_EQ|CMP_MIX_AND);

}

static void mach64_vid_dump_regs( void )
{
  size_t i;
  printf("[mach64] *** Begin of DRIVER variables dump ***\n");
  printf("[mach64] mach64_mmio_base=%p\n",mach64_mmio_base);
  printf("[mach64] mach64_mem_base=%p\n",mach64_mem_base);
  printf("[mach64] mach64_overlay_off=%08X\n",mach64_overlay_offset);
  printf("[mach64] mach64_ram_size=%08X\n",mach64_ram_size);
  printf("[mach64] video mode: %ux%u@%u\n",mach64_get_xres(),mach64_get_yres(),mach64_vid_get_dbpp());
  printf("[mach64] *** Begin of OV0 registers dump ***\n");
  for(i=0;i<sizeof(vregs)/sizeof(video_registers_t);i++)
  {
	mach64_wait_for_idle();
	printf("[mach64] %s = %08X\n",vregs[i].sname,INREG(vregs[i].name));
  }
  printf("[mach64] *** End of OV0 registers dump ***\n");
}


static unsigned short ati_card_ids[] = 
{
 DEVICE_ATI_215CT_MACH64_CT,
 DEVICE_ATI_210888CX_MACH64_CX,
 DEVICE_ATI_210888ET_MACH64_ET,
 DEVICE_ATI_MACH64_VT,
 DEVICE_ATI_210888GX_MACH64_GX,
 DEVICE_ATI_264LT_MACH64_LT,
 DEVICE_ATI_264VT_MACH64_VT,
 DEVICE_ATI_264VT3_MACH64_VT3,
 DEVICE_ATI_264VT4_MACH64_VT4,
 /**/
 DEVICE_ATI_3D_RAGE_PRO,
 DEVICE_ATI_3D_RAGE_PRO2,
 DEVICE_ATI_3D_RAGE_PRO3,
 DEVICE_ATI_3D_RAGE_PRO4,
 DEVICE_ATI_RAGE_XC,
 DEVICE_ATI_RAGE_XL_AGP,
 DEVICE_ATI_RAGE_XC_AGP,
 DEVICE_ATI_RAGE_XL,
 DEVICE_ATI_3D_RAGE_PRO5,
 DEVICE_ATI_3D_RAGE_PRO6,
 DEVICE_ATI_RAGE_XL2,
 DEVICE_ATI_RAGE_XC2,
 DEVICE_ATI_3D_RAGE_I_II,
 DEVICE_ATI_3D_RAGE_II,
 DEVICE_ATI_3D_RAGE_IIC,
 DEVICE_ATI_3D_RAGE_IIC2,
 DEVICE_ATI_3D_RAGE_IIC3,
 DEVICE_ATI_3D_RAGE_IIC4,
 DEVICE_ATI_3D_RAGE_LT,
 DEVICE_ATI_3D_RAGE_LT2,
 DEVICE_ATI_3D_RAGE_LT_G,
 DEVICE_ATI_3D_RAGE_LT3,
 DEVICE_ATI_RAGE_MOBILITY_P_M,
 DEVICE_ATI_RAGE_MOBILITY_L,
 DEVICE_ATI_3D_RAGE_LT4,
 DEVICE_ATI_3D_RAGE_LT5,
 DEVICE_ATI_RAGE_MOBILITY_P_M2,
 DEVICE_ATI_RAGE_MOBILITY_L2
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

static int mach64_probe(int verbose,int force)
{
  pciinfo_t lst[MAX_PCI_DEVICES];
  unsigned i,num_pci;
  int err;
  verbosity = verbose;
  err = pci_scan(lst,&num_pci);
  if(err)
  {
    printf("[mach64] Error occurred during pci scan: %s\n",strerror(err));
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
	printf("[mach64] Found chip: %s\n",dname);
#if 0
	if ((lst[i].command & PCI_COMMAND_IO) == 0)
	{
		printf("[mach64] Device is disabled, ignoring\n");
		continue;
	}
#endif
	if(force > PROBE_NORMAL)
	{
	    printf("[mach64] Driver was forced. Was found %sknown chip\n",idx == -1 ? "un" : "");
	    if(idx == -1)
		printf("[mach64] Assuming it as Mach64\n");
	}
	mach64_cap.device_id = lst[i].device;
	err = 0;
	memcpy(&pci_info,&lst[i],sizeof(pciinfo_t));
	probed=1;
	break;
      }
    }
  }
  if(err && verbose) printf("[mach64] Can't find chip\n");
  return err;
}

static void reset_regs( void )
{
  size_t i;
  for(i=0;i<sizeof(vregs)/sizeof(video_registers_t);i++)
  {
	mach64_fifo_wait(2);
	OUTREG(vregs[i].name,0);
  }
}


static int mach64_init(void)
{
  int err;
  if(!probed)
  {
    printf("[mach64] Driver was not probed but is being initializing\n");
    return EINTR;
  }
  
  if((mach64_mmio_base = map_phys_mem(pci_info.base2,0x1000))==(void *)-1) return ENOMEM;
  mach64_wait_for_idle();
  mach64_ram_size = INREG(MEM_CNTL) & CTL_MEM_SIZEB;
  if (mach64_ram_size < 8) mach64_ram_size = (mach64_ram_size + 1) * 512;
  else if (mach64_ram_size < 12) mach64_ram_size = (mach64_ram_size - 3) * 1024;
  else mach64_ram_size = (mach64_ram_size - 7) * 2048;
  mach64_ram_size *= 0x400; /* KB -> bytes */
  if((mach64_mem_base = map_phys_mem(pci_info.base0,mach64_ram_size))==(void *)-1) return ENOMEM;
  memset(&besr,0,sizeof(bes_registers_t));
  printf("[mach64] Video memory = %uMb\n",mach64_ram_size/0x100000);
  err = mtrr_set_type(pci_info.base0,mach64_ram_size,MTRR_TYPE_WRCOMB);
  if(!err) printf("[mach64] Set write-combining type of video memory\n");

  /* save this */
  mach64_wait_for_idle();
  SAVED_OVERLAY_GRAPHICS_KEY_CLR = INREG(OVERLAY_GRAPHICS_KEY_CLR);
  
  /* check if planar formats are supported */
  supports_planar=0;
  mach64_wait_for_idle();
  mach64_fifo_wait(2);
  if(INREG(SCALER_BUF0_OFFSET_U)) supports_planar=1;
  else
  {
	OUTREG(SCALER_BUF0_OFFSET_U,	-1);

	mach64_wait_vsync();
	mach64_wait_for_idle();
	mach64_fifo_wait(2);

	if(INREG(SCALER_BUF0_OFFSET_U)) 	supports_planar=1;
  }
  if(supports_planar)	printf("[mach64] Planar YUV formats are supported :)\n");
  else			printf("[mach64] Planar YUV formats are not supported :(\n");
  
  if(   mach64_cap.device_id==DEVICE_ATI_RAGE_MOBILITY_P_M
     || mach64_cap.device_id==DEVICE_ATI_RAGE_MOBILITY_P_M2
     || mach64_cap.device_id==DEVICE_ATI_RAGE_MOBILITY_L
     || mach64_cap.device_id==DEVICE_ATI_RAGE_MOBILITY_L2)
         supports_lcd_v_stretch=1;
  else
         supports_lcd_v_stretch=0;
  
  reset_regs();
  mach64_vid_make_default();

  if(verbosity > VERBOSE_LEVEL) mach64_vid_dump_regs();
  return 0;
}

static void mach64_destroy(void)
{
  /*restore this*/
  mach64_wait_for_idle();
  OUTREG(OVERLAY_GRAPHICS_KEY_CLR,SAVED_OVERLAY_GRAPHICS_KEY_CLR);
  
  unmap_phys_mem(mach64_mem_base,mach64_ram_size);
  unmap_phys_mem(mach64_mmio_base,0x1000);
}

static int mach64_get_caps(vidix_capability_t *to)
{
    memcpy(to, &mach64_cap, sizeof(vidix_capability_t));
    return 0;
}

static unsigned mach64_query_pitch(unsigned fourcc,const vidix_yuv_t *spitch)
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

static void mach64_compute_framesize(vidix_playback_t *info)
{
  unsigned pitch,awidth;
  pitch = mach64_query_pitch(info->fourcc,&info->src.pitch);
  switch(info->fourcc)
  {
    case IMGFMT_I420:
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
		awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*(info->src.h+info->src.h/2);
		break;
    case IMGFMT_YVU9:
		awidth = (info->src.w + (pitch-1)) & ~(pitch-1);
		info->frame_size = awidth*(info->src.h+info->src.h/8);
		break;
//    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
		awidth = (info->src.w*4 + (pitch-1)) & ~(pitch-1);
		info->frame_size = (awidth*info->src.h);
		break;
    /* YUY2 YVYU, RGB15, RGB16 */
    default:	
		awidth = (info->src.w*2 + (pitch-1)) & ~(pitch-1);
		info->frame_size = (awidth*info->src.h);
		break;
  }
  info->frame_size+=256; // so we have some space for alignment & such
  info->frame_size&=~16;
}

static void mach64_vid_stop_video( void )
{
    mach64_fifo_wait(14);
    OUTREG(OVERLAY_SCALE_CNTL, 0x80000000);
    OUTREG(OVERLAY_EXCLUSIVE_HORZ, 0);
    OUTREG(OVERLAY_EXCLUSIVE_VERT, 0);
    OUTREG(SCALER_H_COEFF0, 0x00002000);
    OUTREG(SCALER_H_COEFF1, 0x0D06200D);
    OUTREG(SCALER_H_COEFF2, 0x0D0A1C0D);
    OUTREG(SCALER_H_COEFF3, 0x0C0E1A0C);
    OUTREG(SCALER_H_COEFF4, 0x0C14140C);
    OUTREG(VIDEO_FORMAT, 0xB000B);
    OUTREG(OVERLAY_TEST, 0x0);
}

static void mach64_vid_display_video( void )
{
    uint32_t vf;
    mach64_fifo_wait(14);

    OUTREG(OVERLAY_Y_X_START,			besr.y_x_start);
    OUTREG(OVERLAY_Y_X_END,			besr.y_x_end);
    OUTREG(OVERLAY_SCALE_INC,			besr.scale_inc);
    OUTREG(SCALER_BUF_PITCH,			besr.vid_buf_pitch);
    OUTREG(SCALER_HEIGHT_WIDTH,			besr.height_width);
    OUTREG(SCALER_BUF0_OFFSET,			mach64_buffer_base[0][0]);
    OUTREG(SCALER_BUF0_OFFSET_U,		mach64_buffer_base[0][1]);
    OUTREG(SCALER_BUF0_OFFSET_V,		mach64_buffer_base[0][2]);
    OUTREG(SCALER_BUF1_OFFSET,			mach64_buffer_base[0][0]);
    OUTREG(SCALER_BUF1_OFFSET_U,		mach64_buffer_base[0][1]);
    OUTREG(SCALER_BUF1_OFFSET_V,		mach64_buffer_base[0][2]);
    mach64_wait_vsync();
    
    mach64_fifo_wait(4);
    OUTREG(OVERLAY_SCALE_CNTL, 0xC4000003);
// OVERLAY_SCALE_CNTL bits & what they seem to affect
// bit 0 no effect
// bit 1 yuv2rgb coeff related
// bit 2 horizontal interpolation if 0
// bit 3 vertical interpolation if 0
// bit 4 chroma encoding (0-> 128=neutral / 1-> 0->neutral)
// bit 5-6 gamma correction
// bit 7 nothing visible if set
// bit 8-27 no effect
// bit 28-31 nothing interresting just crashed my system when i played with them  :(

    mach64_wait_for_idle();
    vf = INREG(VIDEO_FORMAT);

// Bits 16-19 seem to select the format
// 0x0  dunno behaves strange
// 0x1  dunno behaves strange
// 0x2  dunno behaves strange
// 0x3  BGR15
// 0x4  BGR16
// 0x5  BGR16 (hmm, that need investigation, 2 BGR16 formats, i guess 1 will have only 5bits for green)
// 0x6  BGR32
// 0x7  BGR32 with somehow mixed even / odd pixels ?
// 0x8	YYYYUVUV
// 0x9	YVU9
// 0xA	YV12
// 0xB	YUY2
// 0xC	UYVY
// 0xD  UYVY (no difference is visible if i switch between C/D for every even/odd frame)
// 0xE  dunno behaves strange
// 0xF  dunno behaves strange
// Bit 28 all values are assumed to be 7 bit with chroma=64 for black (tested with YV12 & YUY2)
// the remaining bits seem to have no effect


    switch(besr.fourcc)
    {
	/* BGR formats */
	case IMGFMT_BGR15: OUTREG(VIDEO_FORMAT, 0x00030000);  break;
	case IMGFMT_BGR16: OUTREG(VIDEO_FORMAT, 0x00040000);  break;
	case IMGFMT_BGR32: OUTREG(VIDEO_FORMAT, 0x00060000);  break;
        /* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:  OUTREG(VIDEO_FORMAT, 0x000A0000);  break;

	case IMGFMT_YVU9:  OUTREG(VIDEO_FORMAT, 0x00090000);  break;
        /* 4:2:2 */
        case IMGFMT_YVYU:
	case IMGFMT_UYVY:  OUTREG(VIDEO_FORMAT, 0x000C0000); break;
	case IMGFMT_YUY2:
	default:           OUTREG(VIDEO_FORMAT, 0x000B0000); break;
    }
    if(verbosity > VERBOSE_LEVEL) mach64_vid_dump_regs();
}

static int mach64_vid_init_video( vidix_playback_t *config )
{
    uint32_t src_w,src_h,dest_w,dest_h,pitch,h_inc,v_inc,left,leftUV,top,ecp,y_pos;
    int is_420,best_pitch,mpitch;
    int src_offset_y, src_offset_u, src_offset_v;
    unsigned int i;

    mach64_vid_stop_video();
/* warning, if left or top are != 0 this will fail, as the framesize is too small then */
    left = config->src.x;
    top =  config->src.y;
    src_h = config->src.h;
    src_w = config->src.w;
    is_420 = 0;
    if(config->fourcc == IMGFMT_YV12 ||
       config->fourcc == IMGFMT_I420 ||
       config->fourcc == IMGFMT_IYUV) is_420 = 1;
    best_pitch = mach64_query_pitch(config->fourcc,&config->src.pitch);
    mpitch = best_pitch-1;
    switch(config->fourcc)
    {
	case IMGFMT_YVU9:
	/* 4:2:0 */
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	case IMGFMT_I420: pitch = (src_w + mpitch) & ~mpitch;
			  config->dest.pitch.y = 
			  config->dest.pitch.u = 
			  config->dest.pitch.v = best_pitch;
			  besr.vid_buf_pitch= pitch;
			  break;
	/* RGB 4:4:4:4 */
	case IMGFMT_RGB32:
	case IMGFMT_BGR32: pitch = (src_w*4 + mpitch) & ~mpitch;
			  config->dest.pitch.y = 
			  config->dest.pitch.u = 
			  config->dest.pitch.v = best_pitch;
			  besr.vid_buf_pitch= pitch>>2;
			  break;
	/* 4:2:2 */
        default: /* RGB15, RGB16, YVYU, UYVY, YUY2 */
			  pitch = ((src_w*2) + mpitch) & ~mpitch;
			  config->dest.pitch.y =
			  config->dest.pitch.u =
			  config->dest.pitch.v = best_pitch;
			  besr.vid_buf_pitch= pitch>>1;
			  break;
    }
    dest_w = config->dest.w;
    dest_h = config->dest.h;
    besr.fourcc = config->fourcc;
    ecp = (INPLL(PLL_VCLK_CNTL) & PLL_ECP_DIV) >> 4;

    if(verbosity > 0) printf("[mach64] ecp: %d\n", ecp);
    v_inc = src_h * mach64_get_vert_stretch();
    
    if(mach64_is_interlace()) v_inc<<=1;
    if(mach64_is_dbl_scan() ) v_inc>>=1;
    v_inc>>=4; // convert 16.16 -> 20.12
    v_inc/= dest_h;
    
    h_inc = (src_w << (12+ecp)) / dest_w;
    /* keep everything in 16.16 */
    config->offsets[0] = 0;
    for(i=1; i<config->num_frames; i++)
        config->offsets[i] = config->offsets[i-1] + config->frame_size;
    
	/*FIXME the left / top stuff is broken (= zoom a src rectangle from a larger one)
		1. the framesize isn't known as the outer src rectangle dimensions aren't known
		2. the mach64 needs aligned addresses so it can't work anyway
		   -> so we could shift the outer buffer to compensate that but that would mean
		      alignment problems for the code which writes into it
	*/
    
    if(is_420)
    {
	config->offset.y= 0;
	config->offset.u= (pitch*src_h + 15)&~15; 
	config->offset.v= (config->offset.u + (pitch*src_h>>2) + 15)&~15;

	if(besr.fourcc == IMGFMT_I420 || besr.fourcc == IMGFMT_IYUV)
	{
	  uint32_t tmp;
	  tmp = config->offset.u;
	  config->offset.u = config->offset.v;
	  config->offset.v = tmp;
	}
		
	src_offset_y= config->offset.y + top*pitch + left;
	src_offset_u= config->offset.u + (top*pitch>>2) + (left>>1);
	src_offset_v= config->offset.v + (top*pitch>>2) + (left>>1);
    }
    else if(besr.fourcc == IMGFMT_YVU9)
    {
	config->offset.y= 0;
	config->offset.u= (pitch*src_h + 15)&~15; 
	config->offset.v= (config->offset.u + (pitch*src_h>>4) + 15)&~15;
	
	src_offset_y= config->offset.y + top*pitch + left;
	src_offset_u= config->offset.u + (top*pitch>>4) + (left>>1);
	src_offset_v= config->offset.v + (top*pitch>>4) + (left>>1);
    }
    else if(besr.fourcc == IMGFMT_BGR32)
    {
      config->offset.y = config->offset.u = config->offset.v = 0;
      src_offset_y= src_offset_u= src_offset_v= top*pitch + (left << 2);
    }
    else
    {
      config->offset.y = config->offset.u = config->offset.v = 0;
      src_offset_y= src_offset_u= src_offset_v= top*pitch + (left << 1);
    }

    num_mach64_buffers= config->num_frames;
    for(i=0; i<config->num_frames; i++)
    {
	mach64_buffer_base[i][0]= (mach64_overlay_offset + config->offsets[i] + src_offset_y)&~15;
	mach64_buffer_base[i][1]= (mach64_overlay_offset + config->offsets[i] + src_offset_u)&~15;
	mach64_buffer_base[i][2]= (mach64_overlay_offset + config->offsets[i] + src_offset_v)&~15;
    }

    leftUV = (left >> 17) & 15;
    left = (left >> 16) & 15;
    besr.scale_inc = ( h_inc << 16 ) | v_inc;
    y_pos = config->dest.y;
    if(mach64_is_dbl_scan()) y_pos*=2;
    else
    if(mach64_is_interlace()) y_pos/=2;
    besr.y_x_start = y_pos | (config->dest.x << 16);
    y_pos =config->dest.y + dest_h;
    if(mach64_is_dbl_scan()) y_pos*=2;
    else
    if(mach64_is_interlace()) y_pos/=2;
    besr.y_x_end = y_pos | ((config->dest.x + dest_w) << 16);
    besr.height_width = ((src_w - left)<<16) | (src_h - top);

    return 0;
}

static int is_supported_fourcc(uint32_t fourcc)
{
    switch(fourcc)
    {
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_YVU9:
    case IMGFMT_IYUV:
	return supports_planar;
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
    case IMGFMT_BGR15:
    case IMGFMT_BGR16:
    case IMGFMT_BGR32:
	return 1;
    default:
	return 0;
    }
}

static int mach64_query_fourcc(vidix_fourcc_t *to)
{
    if(is_supported_fourcc(to->fourcc))
    {
	to->depth = VID_DEPTH_ALL;
	to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK | VID_CAP_COLORKEY;
	return 0;
    }
    else  to->depth = to->flags = 0;
    return ENOSYS;
}

static int mach64_config_playback(vidix_playback_t *info)
{
  if(!is_supported_fourcc(info->fourcc)) return ENOSYS;

  mach64_compute_framesize(info);

  if(info->num_frames>4) info->num_frames=4;
  for(;info->num_frames>0; info->num_frames--)
  {
      mach64_overlay_offset = mach64_ram_size - info->frame_size*info->num_frames;
      mach64_overlay_offset &= 0xffff0000;
      if(mach64_overlay_offset>0) break;
  }
  if(info->num_frames <= 0) return EINVAL;

  info->dga_addr = (char *)mach64_mem_base + mach64_overlay_offset;
  mach64_vid_init_video(info);
  return 0;
}

static int mach64_playback_on(void)
{
  mach64_vid_display_video();
  return 0;
}

static int mach64_playback_off(void)
{
  mach64_vid_stop_video();
  return 0;
}

static int mach64_frame_sel(unsigned int frame)
{
    uint32_t off[6];
    int i;
    int last_frame= (frame-1+num_mach64_buffers) % num_mach64_buffers;
//printf("Selecting frame %d\n", frame);    
    /*
    buf3-5 always should point onto second buffer for better
    deinterlacing and TV-in
    */
    if(num_mach64_buffers==1) return 0;

    for(i=0; i<3; i++)
    {
    	off[i]  = mach64_buffer_base[frame][i];
    	off[i+3]= mach64_buffer_base[last_frame][i];
    }

    mach64_wait_for_idle();
    mach64_fifo_wait(7);

    OUTREG(SCALER_BUF0_OFFSET,		off[0]);
    OUTREG(SCALER_BUF0_OFFSET_U,	off[1]);
    OUTREG(SCALER_BUF0_OFFSET_V,	off[2]);
    OUTREG(SCALER_BUF1_OFFSET,		off[3]);
    OUTREG(SCALER_BUF1_OFFSET_U,	off[4]);
    OUTREG(SCALER_BUF1_OFFSET_V,	off[5]);
    if(num_mach64_buffers==2) mach64_wait_vsync(); //only wait for vsync if we do double buffering
       
    if(verbosity > VERBOSE_LEVEL) mach64_vid_dump_regs();
    return 0;
}

static vidix_video_eq_t equal =
{
 VEQ_CAP_BRIGHTNESS | VEQ_CAP_SATURATION
 ,
 0, 0, 0, 0, 0, 0, 0, 0 };

static int mach64_get_eq( vidix_video_eq_t * eq)
{
  memcpy(eq,&equal,sizeof(vidix_video_eq_t));
  return 0;
}

static int mach64_set_eq( const vidix_video_eq_t * eq)
{
  int br,sat;
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
    br = equal.brightness * 64 / 1000;
    if(br < -64) br = -64; if(br > 63) br = 63;
    sat = (equal.saturation + 1000) * 16 / 1000;
    if(sat < 0) sat = 0; if(sat > 31) sat = 31;
    OUTREG(SCALER_COLOUR_CNTL, (br & 0x7f) | (sat << 8) | (sat << 16));
  return 0;
}

static int mach64_get_gkeys(vidix_grkey_t *grkey)
{
    memcpy(grkey, &mach64_grkey, sizeof(vidix_grkey_t));
    return 0;
}

static int mach64_set_gkeys(const vidix_grkey_t *grkey)
{
    memcpy(&mach64_grkey, grkey, sizeof(vidix_grkey_t));

    if(mach64_grkey.ckey.op == CKEY_TRUE)
    {
	besr.ckey_on=1;

	switch(mach64_vid_get_dbpp())
	{
	case 15:
		besr.graphics_key_msk=0x7FFF;
		besr.graphics_key_clr=
			  ((mach64_grkey.ckey.blue &0xF8)>>3)
			| ((mach64_grkey.ckey.green&0xF8)<<2)
			| ((mach64_grkey.ckey.red  &0xF8)<<7);
		break;
	case 16:
		besr.graphics_key_msk=0xFFFF;
		besr.graphics_key_clr=
			  ((mach64_grkey.ckey.blue &0xF8)>>3)
			| ((mach64_grkey.ckey.green&0xFC)<<3)
			| ((mach64_grkey.ckey.red  &0xF8)<<8);
		//besr.graphics_key_clr=le2me_32(besr.graphics_key_clr);
		break;
	case 24:
		besr.graphics_key_msk=0xFFFFFF;
		besr.graphics_key_clr=
			  ((mach64_grkey.ckey.blue &0xFF))
			| ((mach64_grkey.ckey.green&0xFF)<<8)
			| ((mach64_grkey.ckey.red  &0xFF)<<16);
		break;
	case 32:
		besr.graphics_key_msk=0xFFFFFF;
		besr.graphics_key_clr=
			  ((mach64_grkey.ckey.blue &0xFF))
			| ((mach64_grkey.ckey.green&0xFF)<<8)
			| ((mach64_grkey.ckey.red  &0xFF)<<16);
		break;
	default:
		besr.ckey_on=0;
		besr.graphics_key_msk=0;
		besr.graphics_key_clr=0;
	}
    }
    else
    {
	besr.ckey_on=0;
	besr.graphics_key_msk=0;
	besr.graphics_key_clr=0;
    }

    mach64_fifo_wait(4);
    OUTREG(OVERLAY_GRAPHICS_KEY_MSK, besr.graphics_key_msk);
    OUTREG(OVERLAY_GRAPHICS_KEY_CLR, besr.graphics_key_clr);
//    OUTREG(OVERLAY_VIDEO_KEY_MSK, 0);
//    OUTREG(OVERLAY_VIDEO_KEY_CLR, 0);
    if(besr.ckey_on)
    	OUTREG(OVERLAY_KEY_CNTL,VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_EQ|CMP_MIX_AND);
    else
    	OUTREG(OVERLAY_KEY_CNTL,VIDEO_KEY_FN_TRUE|GRAPHIC_KEY_FN_TRUE|CMP_MIX_AND);

    return 0;
}

VDXDriver mach64_drv = {
  "mach64",
  NULL,
  .probe = mach64_probe,
  .get_caps = mach64_get_caps,
  .query_fourcc = mach64_query_fourcc,
  .init = mach64_init,
  .destroy = mach64_destroy,
  .config_playback = mach64_config_playback,
  .playback_on = mach64_playback_on,
  .playback_off = mach64_playback_off,
  .frame_sel = mach64_frame_sel,
  .get_eq = mach64_get_eq,
  .set_eq = mach64_set_eq,
  .get_gkey = mach64_get_gkeys,
  .set_gkey = mach64_set_gkeys,
};
