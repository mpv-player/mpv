/*
   mach64_vid - VIDIX based video driver for Mach64 and 3DRage chips
   Copyrights 2002 Nick Kurshev. This file is based on sources from
   GATOS (gatos.sf.net) and X11 (www.xfree86.org)
   Licence: GPL
   WARNING: THIS DRIVER IS IN BETTA STAGE AND DOESN'T WORK WITH PLANAR FOURCCS!
*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <fcntl.h>

#include "../vidix.h"
#include "../fourcc.h"
#include "../../libdha/libdha.h"
#include "../../libdha/pci_ids.h"
#include "../../libdha/pci_names.h"

#include "mach64.h"

#define UNUSED(x) ((void)(x)) /**< Removes warning about unused arguments */

static void *mach64_mmio_base = 0;
static void *mach64_mem_base = 0;
static int32_t mach64_overlay_offset = 0;
static uint32_t mach64_ram_size = 0;

pciinfo_t pci_info;
static int probed = 0;
static int __verbose = 0;

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
  uint32_t vid_buf0_base_adrs;
  uint32_t vid_buf1_base_adrs;
  uint32_t vid_buf2_base_adrs;
  uint32_t vid_buf3_base_adrs;
  uint32_t vid_buf4_base_adrs;
  uint32_t vid_buf5_base_adrs;

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

static bes_registers_t besr;

typedef struct video_registers_s
{
  const char * sname;
  uint32_t name;
  uint32_t value;
}video_registers_t;

static bes_registers_t besr;
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

/* VIDIX exports */

/* MMIO space*/
#define GETREG(TYPE,PTR,OFFZ)		(*((volatile TYPE*)((PTR)+(OFFZ))))
#define SETREG(TYPE,PTR,OFFZ,VAL)	(*((volatile TYPE*)((PTR)+(OFFZ))))=VAL

#define INREG8(addr)		GETREG(uint8_t,(uint32_t)mach64_mmio_base,((addr)^0x100)<<2)
#define OUTREG8(addr,val)	SETREG(uint8_t,(uint32_t)mach64_mmio_base,((addr)^0x100)<<2,val)
#define INREG(addr)		GETREG(uint32_t,(uint32_t)mach64_mmio_base,((addr)^0x100)<<2)
#define OUTREG(addr,val)	SETREG(uint32_t,(uint32_t)mach64_mmio_base,((addr)^0x100)<<2,val)

#define OUTREGP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INREG(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTREG(addr, _tmp);					\
	} while (0)

static __inline__ uint32_t INPLL(uint32_t addr)
{
    uint32_t res;

    /* write addr byte */
    OUTREG8(CLOCK_CNTL + 1, (addr << 2));
    /* read the register value */
    res = INREG(CLOCK_CNTL + 2);
    return res;
}

static __inline__ void OUTPLL(uint32_t addr,uint32_t val)
{
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

static vidix_capability_t mach64_cap =
{
    "BES driver for Mach64/3DRage cards",
    "Nick Kurshev",
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

static void mach64_vid_make_default()
{
  mach64_fifo_wait(2);
  OUTREG(SCALER_COLOUR_CNTL,0x00101000);
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


unsigned int vixGetVersion(void)
{
    return(VIDIX_VERSION);
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
 DEVICE_ATI_RAGE_MOBILITY_M3,
 DEVICE_ATI_RAGE_MOBILITY_M32,
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

int vixProbe(int verbose,int force)
{
  pciinfo_t lst[MAX_PCI_DEVICES];
  unsigned i,num_pci;
  int err;
  __verbose = verbose;
  err = pci_scan(lst,&num_pci);
  if(err)
  {
    printf("[mach64] Error occured during pci scan: %s\n",strerror(err));
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


int vixInit(void)
{
  int err;
  if(!probed)
  {
    printf("[mach64] Driver was not probed but is being initializing\n");
    return EINTR;
  }
  if((mach64_mmio_base = map_phys_mem(pci_info.base2,0x4000))==(void *)-1) return ENOMEM;
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
  reset_regs();
  mach64_vid_make_default();
  if(__verbose > VERBOSE_LEVEL) mach64_vid_dump_regs();
  return 0;
}

void vixDestroy(void)
{
  unmap_phys_mem(mach64_mem_base,mach64_ram_size);
  unmap_phys_mem(mach64_mmio_base,0x4000);
}

int vixGetCapability(vidix_capability_t *to)
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
    case IMGFMT_RGB32:
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
    OUTREG(OVERLAY_GRAPHICS_KEY_MSK, 0);
    OUTREG(OVERLAY_GRAPHICS_KEY_CLR, 0);
    OUTREG(OVERLAY_KEY_CNTL, 0x50);
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
    OUTREG(SCALER_BUF0_OFFSET,			besr.vid_buf0_base_adrs);
    OUTREG(SCALER_BUF0_OFFSET_U,		besr.vid_buf1_base_adrs);
    OUTREG(SCALER_BUF0_OFFSET_V,		besr.vid_buf2_base_adrs);
    OUTREG(SCALER_BUF1_OFFSET,			besr.vid_buf3_base_adrs);
    OUTREG(SCALER_BUF1_OFFSET_U,		besr.vid_buf4_base_adrs);
    OUTREG(SCALER_BUF1_OFFSET_V,		besr.vid_buf5_base_adrs);
    OUTREG(OVERLAY_SCALE_CNTL, 0xC4000003);
// OVERLAY_SCALE_CNTL bits & what they seem to affect
// bit 0 no effect
// bit 1 yuv2rgb coeff related
// bit 2 horizontal interpolation if 0
// bit 3 vertical interpolation if 0
// bit 4 chroma related
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
// 0xD  UYVY (not again ... dont ask me, i dunno the difference)
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
        /* 4:2:2 */
        case IMGFMT_YVYU:
	case IMGFMT_UYVY:  OUTREG(VIDEO_FORMAT, 0x000C0000); break;
	case IMGFMT_YUY2:
	default:           OUTREG(VIDEO_FORMAT, 0x000B0000); break;
    }
    if(__verbose > VERBOSE_LEVEL) mach64_vid_dump_regs();
}

static int mach64_vid_init_video( vidix_playback_t *config )
{
    uint32_t src_w,src_h,dest_w,dest_h,pitch,h_inc,v_inc,left,leftUV,top,ecp,y_pos;
    int is_420,best_pitch,mpitch;
    mach64_vid_stop_video();
    left = config->src.x << 16;
    top =  config->src.y << 16;
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
    v_inc = (src_h << (12
		+(mach64_is_interlace()?1:0)
		-(mach64_is_dbl_scan()?1:0)
//		+(is_420?1:0)
		)) / dest_h;
    h_inc = (src_w << (12+ecp)) / dest_w;
    /* keep everything in 16.16 */
    config->offsets[0] = 0;
    config->offsets[1] = config->frame_size;
    if(is_420)
    {
        uint32_t d1line,d2line,d3line;
	d1line = top*pitch;
	d2line = src_h*pitch+(d1line>>2);
	d3line = d2line+((src_h*pitch)>>2);
	d1line += (left >> 16) & ~15;
	d2line += (left >> 17) & ~15;
	d3line += (left >> 17) & ~15;
	config->offset.y = d1line & ~15;
	config->offset.v = d2line & ~15;
	config->offset.u = d3line & ~15;
        besr.vid_buf0_base_adrs=((mach64_overlay_offset+config->offsets[0]+config->offset.y)&~15);
        besr.vid_buf1_base_adrs=((mach64_overlay_offset+config->offsets[0]+config->offset.v)&~15);
        besr.vid_buf2_base_adrs=((mach64_overlay_offset+config->offsets[0]+config->offset.u)&~15);
        besr.vid_buf3_base_adrs=((mach64_overlay_offset+config->offsets[1]+config->offset.y)&~15);
        besr.vid_buf4_base_adrs=((mach64_overlay_offset+config->offsets[1]+config->offset.v)&~15);
        besr.vid_buf5_base_adrs=((mach64_overlay_offset+config->offsets[1]+config->offset.u)&~15);
	config->offset.y = ((besr.vid_buf0_base_adrs)&~15) - mach64_overlay_offset;
	config->offset.v = ((besr.vid_buf1_base_adrs)&~15) - mach64_overlay_offset;
	config->offset.u = ((besr.vid_buf2_base_adrs)&~15) - mach64_overlay_offset;
	if(besr.fourcc == IMGFMT_I420 || besr.fourcc == IMGFMT_IYUV)
	{
	  uint32_t tmp;
	  tmp = config->offset.u;
	  config->offset.u = config->offset.v;
	  config->offset.v = tmp;
	}
    }
    else
    {
      besr.vid_buf0_base_adrs = mach64_overlay_offset;
      config->offset.y = config->offset.u = config->offset.v = ((left & ~7) << 1)&~15;
      besr.vid_buf0_base_adrs += config->offset.y;
      besr.vid_buf1_base_adrs = besr.vid_buf0_base_adrs;
      besr.vid_buf2_base_adrs = besr.vid_buf0_base_adrs;
      besr.vid_buf3_base_adrs = besr.vid_buf0_base_adrs+config->frame_size;
      besr.vid_buf4_base_adrs = besr.vid_buf0_base_adrs+config->frame_size;
      besr.vid_buf5_base_adrs = besr.vid_buf0_base_adrs+config->frame_size;
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


uint32_t supported_fourcc[] = 
{
  IMGFMT_YV12, IMGFMT_I420, IMGFMT_IYUV, 
  IMGFMT_UYVY, IMGFMT_YUY2, IMGFMT_YVYU,
  IMGFMT_BGR15,IMGFMT_BGR16,IMGFMT_BGR32
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

int vixConfigPlayback(vidix_playback_t *info)
{
  if(!is_supported_fourcc(info->fourcc)) return ENOSYS;
  if(info->num_frames>2) info->num_frames=2;
  if(info->num_frames==1) besr.double_buff=0;
  else                    besr.double_buff=1;
  mach64_compute_framesize(info);
  mach64_overlay_offset = mach64_ram_size - info->frame_size*info->num_frames;
  mach64_overlay_offset &= 0xffff0000;
  if(mach64_overlay_offset < 0) return EINVAL;
  info->dga_addr = (char *)mach64_mem_base + mach64_overlay_offset;
  mach64_vid_init_video(info);
  return 0;
}

int vixPlaybackOn(void)
{
  mach64_vid_display_video();
  return 0;
}

int vixPlaybackOff(void)
{
  mach64_vid_stop_video();
  return 0;
}

static void mach64_wait_vsync( void )
{
#warning MACH64 VSYNC WAS NOT IMPLEMENTED!!!
}

int vixPlaybackFrameSelect(unsigned int frame)
{
    uint32_t off[6];
    /*
    buf3-5 always should point onto second buffer for better
    deinterlacing and TV-in
    */
    if(!besr.double_buff) return 0;
    if((frame%2))
    {
      off[0] = besr.vid_buf3_base_adrs;
      off[1] = besr.vid_buf4_base_adrs;
      off[2] = besr.vid_buf5_base_adrs;
      off[3] = besr.vid_buf0_base_adrs;
      off[4] = besr.vid_buf1_base_adrs;
      off[5] = besr.vid_buf2_base_adrs;
    }
    else
    {
      off[0] = besr.vid_buf0_base_adrs;
      off[1] = besr.vid_buf1_base_adrs;
      off[2] = besr.vid_buf2_base_adrs;
      off[3] = besr.vid_buf3_base_adrs;
      off[4] = besr.vid_buf4_base_adrs;
      off[5] = besr.vid_buf5_base_adrs;
#if 0 // debuging code, can be removed
{
int x,y;
char *buf0= (char *)mach64_mem_base + mach64_overlay_offset;
char *buf1= (char *)mach64_mem_base + mach64_overlay_offset;
char *buf2= (char *)mach64_mem_base + mach64_overlay_offset;
buf0 += ((besr.vid_buf0_base_adrs)&~15) - mach64_overlay_offset;
buf1 += ((besr.vid_buf1_base_adrs)&~15) - mach64_overlay_offset;
buf2 += ((besr.vid_buf2_base_adrs)&~15) - mach64_overlay_offset;
/*for(y=0; y<480/4; y++)
{
	for(x=0; x<640/4; x++)
	{
		buf1[x + y*160]= 0; // buf1[2*x + y*160*4];
		buf2[x + y*160]= 0; //buf2[2*x + y*160*4];
	}
}*/
/*)for(y=479; y>0; y--)
{
	for(x=0; x<640; x++)
	{
		buf0[x*2 + y*1280+1]=
		buf0[x*2 + y*1280]= buf0[x + y*640];
	}
}*/
for(y=0; y<480; y++)
{
//	for(x=0; x<1280; x++) buf0[x + y*1280]=0;
	for(x=0; x<1280/4; x++)
	{
// 1-> gray0
//		buf0[x*2 + y*1280 +0] ^= buf0[x*2 + y*1280 +1];
//		buf0[x*2 + y*1280 +1] ^= buf0[x*2 + y*1280 +0];
//		buf0[x*2 + y*1280 +0] ^= buf0[x*2 + y*1280 +1];
		
		buf0[x*4 + y*1280 +1] =x; //buf0[x*4 + y*1280 +0]>>1;
		buf0[x*4 + y*1280 +3] =128; //buf0[x*4 + y*1280 +2]>>1;
		buf0[x*4 + y*1280 +0] =128;
		buf0[x*4 + y*1280 +2] =128;

//		buf0[x*8 + y*1280 +0]= 1;
//		buf0[x*2 + y*1280 +1]= 7;
//		buf0[x*2 + y*1280+6 ]= 255;
	}
// Y, Y, Y, Y, U, V, U, V
}
/*for(y=0; y<480; y++)
{
//	for(x=0; x<1280; x++) buf0[x + y*1280]=128;
	for(x=0; x<640; x++)
	{
		buf0[x + y*640 ]>>=1;
		buf0[x + y*640 ]|=128;
	}
}
for(y=0; y<480/2; y++)
{
//	for(x=0; x<1280; x++) buf0[x + y*1280]=128;
	for(x=0; x<640/2; x++)
	{
		buf1[x + y*320 ]>>=1;
		buf2[x + y*320 ]>>=1;
	}
}*/
}
#endif
    }

    mach64_wait_vsync();
    mach64_wait_for_idle();
    mach64_fifo_wait(7);
    OUTREG(SCALER_BUF0_OFFSET,		off[0]);
    OUTREG(SCALER_BUF0_OFFSET_U,	off[1]);
    OUTREG(SCALER_BUF0_OFFSET_V,	off[2]);
    OUTREG(SCALER_BUF1_OFFSET,		off[3]);
    OUTREG(SCALER_BUF1_OFFSET_U,	off[4]);
    OUTREG(SCALER_BUF1_OFFSET_V,	off[5]);
    if(__verbose > VERBOSE_LEVEL) mach64_vid_dump_regs();
    return 0;
}

vidix_video_eq_t equal =
{
 VEQ_CAP_BRIGHTNESS | VEQ_CAP_SATURATION
 ,
 0, 0, 0, 0, 0, 0, 0, 0 };

int 	vixPlaybackGetEq( vidix_video_eq_t * eq)
{
  memcpy(eq,&equal,sizeof(vidix_video_eq_t));
  return 0;
}

int 	vixPlaybackSetEq( const vidix_video_eq_t * eq)
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
