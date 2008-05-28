/*
 * Copyright (C) 2003 Alban Bedel
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

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10)
#include <linux/malloc.h>
#else
#include <linux/slab.h>
#endif

#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/agp_backend.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>

#include "tdfx_vid.h"
#include "3dfx.h"


#define TDFX_VID_MAJOR 178

MODULE_AUTHOR("Albeu");
MODULE_DESCRIPTION("A driver for Banshee targeted for video app");

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#ifndef min
#define min(x,y) (((x)<(y))?(x):(y))
#endif

static struct pci_dev *pci_dev;

static uint8_t *tdfx_mmio_base = 0;
static uint32_t tdfx_mem_base = 0;
static uint32_t tdfx_io_base = 0;

static int tdfx_ram_size = 0;

static int tdfx_vid_in_use = 0;

static drm_agp_t *drm_agp = NULL;
static agp_kern_info agp_info;
static agp_memory *agp_mem = NULL;

static __initdata int tdfx_map_io = 1;
static __initdata unsigned long map_start = 0; //0x7300000;
static __initdata unsigned long map_max = (10*1024*1024);

MODULE_PARM(tdfx_map_io,"i");
MODULE_PARM_DESC(tdfx_map_io, "Set to 0 to use the page fault handler (you need to patch agpgart_be.c to allow the mapping in user space)\n");
MODULE_PARM(map_start,"l");
MODULE_PARM_DESC(map_start,"Use a block of physical mem instead of the agp arerture.");
MODULE_PARM(map_max,"l");
MODULE_PARM_DESC(map_max, "Maximum amout of physical memory (in bytes) that can be used\n");

static inline u32 tdfx_inl(unsigned int reg) {
  return readl(tdfx_mmio_base + reg);
}

static inline void tdfx_outl(unsigned int reg, u32 val) {
  writel(val,tdfx_mmio_base  + reg);
}

static inline void banshee_make_room(int size) {
  while((tdfx_inl(STATUS) & 0x1f) < size);
}
 
static inline void banshee_wait_idle(void) {
  int i = 0;

  banshee_make_room(1);
  tdfx_outl(COMMAND_3D, COMMAND_3D_NOP);

  while(1) {
    i = (tdfx_inl(STATUS) & STATUS_BUSY) ? 0 : i + 1;
    if(i == 3) break;
  }
}

static unsigned long get_lfb_size(void) {
  u32 draminit0 = 0;
  u32 draminit1 = 0;
  //  u32 miscinit1 = 0;
  u32 lfbsize   = 0;
  int sgram_p     = 0;

  draminit0 = tdfx_inl(DRAMINIT0);  
  draminit1 = tdfx_inl(DRAMINIT1);

  if ((pci_dev->device == PCI_DEVICE_ID_3DFX_BANSHEE) ||
      (pci_dev->device == PCI_DEVICE_ID_3DFX_VOODOO3)) {
    sgram_p = (draminit1 & DRAMINIT1_MEM_SDRAM) ? 0 : 1;
  
    lfbsize = sgram_p ?
      (((draminit0 & DRAMINIT0_SGRAM_NUM)  ? 2 : 1) * 
       ((draminit0 & DRAMINIT0_SGRAM_TYPE) ? 8 : 4) * 1024 * 1024) :
      16 * 1024 * 1024;
  } else {
    /* Voodoo4/5 */
    u32 chips, psize, banks;

    chips = ((draminit0 & (1 << 26)) == 0) ? 4 : 8;
    psize = 1 << ((draminit0 & 0x38000000) >> 28);
    banks = ((draminit0 & (1 << 30)) == 0) ? 2 : 4;
    lfbsize = chips * psize * banks;
    lfbsize <<= 20;
  }

#if 0
  /* disable block writes for SDRAM (why?) */
  miscinit1 = tdfx_inl(MISCINIT1);
  miscinit1 |= sgram_p ? 0 : MISCINIT1_2DBLOCK_DIS;
  miscinit1 |= MISCINIT1_CLUT_INV;

  banshee_make_room(1); 
  tdfx_outl(MISCINIT1, miscinit1);
#endif

  return lfbsize;
}

static int tdfx_vid_find_card(void)
{
  struct pci_dev *dev = NULL;
  //  unsigned int card_option;

  if((dev = pci_find_device(PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_BANSHEE, NULL)))
    printk(KERN_INFO "tdfx_vid: Found VOODOO BANSHEE\n");
  else if((dev = pci_find_device(PCI_VENDOR_ID_3DFX, PCI_DEVICE_ID_3DFX_VOODOO3, NULL)))
    printk(KERN_INFO "tdfx_vid: Found VOODOO 3 \n");
  else
    return 0;

  
  pci_dev = dev;

#if LINUX_VERSION_CODE >= 0x020300
  tdfx_mmio_base = ioremap_nocache(dev->resource[0].start,1 << 24);
  tdfx_mem_base =  dev->resource[1].start;
  tdfx_io_base = dev->resource[2].start;
#else
  tdfx_mmio_base = ioremap_nocache(dev->base_address[1] & PCI_BASE_ADDRESS_MEM_MASK,0x4000);
  tdfx_mem_base =  dev->base_address[1] & PCI_BASE_ADDRESS_MEM_MASK;
  tdfx_io_base = dev->base_address[2] & PCI_BASE_ADDRESS_MEM_MASK;
#endif
  printk(KERN_INFO "tdfx_vid: MMIO at 0x%p\n", tdfx_mmio_base);
  tdfx_ram_size = get_lfb_size();

  printk(KERN_INFO "tdfx_vid: Found %d MB (%d bytes) of memory\n",
	 tdfx_ram_size / 1024 / 1024,tdfx_ram_size);

  
#if 0
  {
    int temp;
    printk("List resources -----------\n");
    for(temp=0;temp<DEVICE_COUNT_RESOURCE;temp++){
      struct resource *res=&pci_dev->resource[temp];
      if(res->flags){
	int size=(1+res->end-res->start)>>20;
	printk(KERN_DEBUG "res %d:  start: 0x%X   end: 0x%X  (%d MB) flags=0x%X\n",temp,res->start,res->end,size,res->flags);
	if(res->flags&(IORESOURCE_MEM|IORESOURCE_PREFETCH)){
	  if(size>tdfx_ram_size && size<=64) tdfx_ram_size=size;
	}
      }
    }
  }
#endif


  return 1;
}

static int agp_init(void) {

  drm_agp = (drm_agp_t*)inter_module_get("drm_agp");

  if(!drm_agp) {
    printk(KERN_ERR "tdfx_vid: Unable to get drm_agp pointer\n");
    return 0;
  }

  if(drm_agp->acquire()) {
    printk(KERN_ERR "tdfx_vid: Unable to acquire the agp backend\n");
    drm_agp = NULL;
    return 0;
  }

  drm_agp->copy_info(&agp_info);
#if 0
  printk(KERN_DEBUG "AGP Version : %d %d\n"
	 "AGP Mode: %#X\nAperture Base: %p\nAperture Size: %d\n"
	 "Max memory = %d\nCurrent mem = %d\nCan use perture : %s\n"
	 "Page mask = %#X\n",
	 agp_info.version.major,agp_info.version.minor,
	 agp_info.mode,agp_info.aper_base,agp_info.aper_size,
	 agp_info.max_memory,agp_info.current_memory,
	 agp_info.cant_use_aperture ? "no" : "yes",
	 agp_info.page_mask);
#endif
  drm_agp->enable(agp_info.mode);

  
  printk(KERN_INFO "AGP Enabled\n");

  return 1;
}
    
static void agp_close(void) {

  if(!drm_agp) return;

  if(agp_mem) {
    drm_agp->unbind_memory(agp_mem);
    drm_agp->free_memory(agp_mem);
    agp_mem = NULL;
  }
      

  drm_agp->release();
  inter_module_put("drm_agp");
}

static int agp_move(tdfx_vid_agp_move_t* m) {
  u32 src = 0;
  u32 src_h,src_l;

  if(!(agp_mem||map_start))
    return -EAGAIN;

  if(m->move2 > 3) {
    printk(KERN_DEBUG "tdfx_vid: AGP move invalid destination %d\n",
	   m->move2);
    return -EAGAIN;
  }

  if(map_start)
    src =  map_start + m->src;
  else
    src =  agp_info.aper_base +  m->src;

  src_l = (u32)src;
  src_h = (m->width | (m->src_stride << 14)) & 0x0FFFFFFF;

  //  banshee_wait_idle();
  banshee_make_room(6);
  tdfx_outl(AGPHOSTADDRESSHIGH,src_h);
  tdfx_outl(AGPHOSTADDRESSLOW,src_l);

  tdfx_outl(AGPGRAPHICSADDRESS, m->dst);
  tdfx_outl(AGPGRAPHICSSTRIDE, m->dst_stride);
  tdfx_outl(AGPREQSIZE,m->src_stride*m->height);

  tdfx_outl(AGPMOVECMD,m->move2 << 3);
  banshee_wait_idle();

  return 0;
}

#if 0
static void setup_fifo(u32 offset,ssize_t pages) {
  long addr = agp_info.aper_base + offset;
  u32 size = pages | 0x700; // fifo on, in agp mem, disable hole cnt

  banshee_wait_idle();

  tdfx_outl(CMDBASEADDR0,addr >> 4);
  tdfx_outl(CMDRDPTRL0, addr << 4);
  tdfx_outl(CMDRDPTRH0, addr >> 28);
  tdfx_outl(CMDAMIN0, (addr - 4) & 0xFFFFFF);
  tdfx_outl(CMDAMAX0, (addr - 4) & 0xFFFFFF);
  tdfx_outl(CMDFIFODEPTH0, 0);
  tdfx_outl(CMDHOLECNT0, 0);
  tdfx_outl(CMDBASESIZE0,size);

  banshee_wait_idle();
  
}
#endif

static int bump_fifo(u16 size) {

  banshee_wait_idle();
  tdfx_outl(CMDBUMP0 , size);
  banshee_wait_idle();

  return 0;
}

static void tdfx_vid_get_config(tdfx_vid_config_t* cfg) {
  u32 in;

  cfg->version = TDFX_VID_VERSION;
  cfg->ram_size = tdfx_ram_size;

  in = tdfx_inl(VIDSCREENSIZE);
  cfg->screen_width = in & 0xFFF;
  cfg->screen_height = (in >> 12) & 0xFFF;
  in = (tdfx_inl(VIDPROCCFG)>> 18)& 0x7;
  switch(in) {
  case 0:
    cfg->screen_format = TDFX_VID_FORMAT_BGR8;
    break;
  case 1:
    cfg->screen_format = TDFX_VID_FORMAT_BGR16;
    break;
  case 2:
    cfg->screen_format = TDFX_VID_FORMAT_BGR24;
    break;
  case 3:
    cfg->screen_format = TDFX_VID_FORMAT_BGR32;
    break;
  default:
    printk(KERN_INFO "tdfx_vid: unknown screen format %d\n",in);
    cfg->screen_format = 0;
    break;
  }
  cfg->screen_stride = tdfx_inl(VIDDESKSTRIDE) & 0x7FFF;
  cfg->screen_start = tdfx_inl(VIDDESKSTART);
}

inline static u32 tdfx_vid_make_format(int src,u16 stride,u32 fmt) {
  u32 r = stride & 0xFFF3;
  u32 tdfx_fmt = 0;

  // src and dest formats
  switch(fmt) {
  case TDFX_VID_FORMAT_BGR8:
    tdfx_fmt = 1;
    break;
  case TDFX_VID_FORMAT_BGR16:
    tdfx_fmt = 3;
    break;
  case TDFX_VID_FORMAT_BGR24:
    tdfx_fmt = 4;
    break;
  case TDFX_VID_FORMAT_BGR32:
    tdfx_fmt = 5;
    break;
  }

  if(!src && !tdfx_fmt) {
    printk(KERN_INFO "tdfx_vid: Invalid destination format %#X\n",fmt);
    return 0;
  }

  if(src && !tdfx_fmt) {
    // src only format
    switch(fmt){
    case TDFX_VID_FORMAT_BGR1:
      tdfx_fmt = 0;
      break;
    case TDFX_VID_FORMAT_BGR15: // To check
      tdfx_fmt = 2;
      break;
    case TDFX_VID_FORMAT_YUY2:
      tdfx_fmt = 8;
      break;
    case TDFX_VID_FORMAT_UYVY:
      tdfx_fmt = 9;
      break;
    default:
      printk(KERN_INFO "tdfx_vid: Invalid source format %#X\n",fmt);
      return 0;
    }
  }

  r |= tdfx_fmt << 16;

  return r;
}

static int tdfx_vid_blit(tdfx_vid_blit_t* blit) {
  u32 src_fmt,dst_fmt,cmd = 2;
  u32 cmin,cmax,srcbase,srcxy,srcfmt,srcsize;
  u32 dstbase,dstxy,dstfmt,dstsize = 0;
  u32 cmd_extra = 0,src_ck[2],dst_ck[2],rop123=0;
  
  //printk(KERN_INFO "tdfx_vid: Make src fmt 0x%x\n",blit->src_format);
  src_fmt = tdfx_vid_make_format(1,blit->src_stride,blit->src_format);
  if(!src_fmt)
    return 0;
  //printk(KERN_INFO "tdfx_vid: Make dst fmt 0x%x\n", blit->dst_format);
  dst_fmt = tdfx_vid_make_format(0,blit->dst_stride,blit->dst_format);
  if(!dst_fmt)
    return 0;
  blit->colorkey &= 0x3;
  // Be nice if user just want a simple blit
  if((!blit->colorkey) && (!blit->rop[0]))
    blit->rop[0] = TDFX_VID_ROP_COPY;
  // No stretch : fix me the cmd should be 1 but it
  // doesn't work. Maybe some other regs need to be set
  // as non-stretch blit have more options
  if(((!blit->dst_w) && (!blit->dst_h)) || 
     ((blit->dst_w == blit->src_w) && (blit->dst_h == blit->src_h)))
    cmd = 2;
  
  // Save the regs otherwise fb get crazy
  // we can perhaps avoid some ...
  banshee_wait_idle();
  cmin = tdfx_inl(CLIP0MIN);
  cmax = tdfx_inl(CLIP0MAX);
  srcbase = tdfx_inl(SRCBASE);
  srcxy = tdfx_inl(SRCXY);
  srcfmt = tdfx_inl(SRCFORMAT);
  srcsize = tdfx_inl(SRCSIZE);
  dstbase = tdfx_inl(DSTBASE);
  dstxy = tdfx_inl(DSTXY);
  dstfmt = tdfx_inl(DSTFORMAT);
  if(cmd == 2)
    dstsize = tdfx_inl(DSTSIZE);
  if(blit->colorkey & TDFX_VID_SRC_COLORKEY) {
    src_ck[0] = tdfx_inl(SRCCOLORKEYMIN);
    src_ck[1] = tdfx_inl(SRCCOLORKEYMAX);
    tdfx_outl(SRCCOLORKEYMIN,blit->src_colorkey[0]);
    tdfx_outl(SRCCOLORKEYMAX,blit->src_colorkey[1]);
  }
  if(blit->colorkey & TDFX_VID_DST_COLORKEY) {
    dst_ck[0] = tdfx_inl(DSTCOLORKEYMIN);
    dst_ck[1] = tdfx_inl(DSTCOLORKEYMAX);
    tdfx_outl(SRCCOLORKEYMIN,blit->dst_colorkey[0]);
    tdfx_outl(SRCCOLORKEYMAX,blit->dst_colorkey[1]);   
  }
  if(blit->colorkey) {
    cmd_extra = tdfx_inl(COMMANDEXTRA_2D);
    rop123 = tdfx_inl(ROP123);
    tdfx_outl(COMMANDEXTRA_2D, blit->colorkey);
    tdfx_outl(ROP123,(blit->rop[1] | (blit->rop[2] << 8) | blit->rop[3] << 16));
    
  }
  // Get rid of the clipping at the moment
  tdfx_outl(CLIP0MIN,0);
  tdfx_outl(CLIP0MAX,0x0fff0fff);

  // Setup the src
  tdfx_outl(SRCBASE,blit->src & 0x00FFFFFF);
  tdfx_outl(SRCXY,XYREG(blit->src_x,blit->src_y));
  tdfx_outl(SRCFORMAT,src_fmt);
  tdfx_outl(SRCSIZE,XYREG(blit->src_w,blit->src_h));

  // Setup the dst
  tdfx_outl(DSTBASE,blit->dst & 0x00FFFFFF);
  tdfx_outl(DSTXY,XYREG(blit->dst_x,blit->dst_y));
  tdfx_outl(DSTFORMAT,dst_fmt);
  if(cmd == 2)
    tdfx_outl(DSTSIZE,XYREG(blit->dst_w,blit->dst_h));

  // Send the command
  tdfx_outl(COMMAND_2D,cmd | 0x100 | (blit->rop[0] << 24));
  banshee_wait_idle();

  // Now restore the regs to make fb happy
  tdfx_outl(CLIP0MIN, cmin);
  tdfx_outl(CLIP0MAX, cmax);
  tdfx_outl(SRCBASE, srcbase);
  tdfx_outl(SRCXY, srcxy);
  tdfx_outl(SRCFORMAT, srcfmt);
  tdfx_outl(SRCSIZE, srcsize);
  tdfx_outl(DSTBASE, dstbase);
  tdfx_outl(DSTXY, dstxy);
  tdfx_outl(DSTFORMAT, dstfmt);
  if(cmd == 2)
    tdfx_outl(DSTSIZE, dstsize);
  if(blit->colorkey & TDFX_VID_SRC_COLORKEY) {
    tdfx_outl(SRCCOLORKEYMIN,src_ck[0]);
    tdfx_outl(SRCCOLORKEYMAX,src_ck[1]);
  }
  if(blit->colorkey & TDFX_VID_DST_COLORKEY) {
    tdfx_outl(SRCCOLORKEYMIN,dst_ck[0]);
    tdfx_outl(SRCCOLORKEYMAX,dst_ck[1]);   
  }
  if(blit->colorkey) {
    tdfx_outl(COMMANDEXTRA_2D,cmd_extra);
    tdfx_outl(ROP123,rop123);
  }
  return 1;
}

static int tdfx_vid_set_yuv(unsigned long arg) {
  tdfx_vid_yuv_t yuv;

  if(copy_from_user(&yuv,(tdfx_vid_yuv_t*)arg,sizeof(tdfx_vid_yuv_t))) {
    printk(KERN_DEBUG "tdfx_vid:failed copy from userspace\n");
    return -EFAULT;
  }
  banshee_make_room(2);
  tdfx_outl(YUVBASEADDRESS,yuv.base & 0x01FFFFFF);
  tdfx_outl(YUVSTRIDE, yuv.stride & 0x3FFF);
  
  banshee_wait_idle();
  
  return 0;
}

static int tdfx_vid_get_yuv(unsigned long arg) {
  tdfx_vid_yuv_t yuv;

  yuv.base = tdfx_inl(YUVBASEADDRESS) & 0x01FFFFFF;
  yuv.stride = tdfx_inl(YUVSTRIDE) & 0x3FFF;

  if(copy_to_user((tdfx_vid_yuv_t*)arg,&yuv,sizeof(tdfx_vid_yuv_t))) {
      printk(KERN_INFO "tdfx_vid:failed copy to userspace\n");
      return -EFAULT;
  }

  return 0;
}

static int tdfx_vid_set_overlay(unsigned long arg) {
  tdfx_vid_overlay_t ov;
  uint32_t screen_w,screen_h;
  uint32_t vidcfg,stride,vidbuf;
  int disp_w,disp_h;

  if(copy_from_user(&ov,(tdfx_vid_overlay_t*)arg,sizeof(tdfx_vid_overlay_t))) {
    printk(KERN_DEBUG "tdfx_vid:failed copy from userspace\n");
    return -EFAULT;
  }

  if(ov.dst_y < 0) {
    int shift;
    if(-ov.dst_y >= ov.src_height) {
      printk(KERN_DEBUG "tdfx_vid: Overlay outside of the screen ????\n");
      return -EFAULT;
    }
    shift = (-ov.dst_y)/(double)ov.dst_height*ov.src_height;
    ov.src[0] += shift*ov.src_stride;
    ov.src_height -= shift;
    ov.dst_height += ov.dst_y;
    ov.dst_y = 0;
  }

  if(ov.dst_x < 0) {
    int shift;
    if(-ov.dst_x >= ov.src_width) {
      printk(KERN_DEBUG "tdfx_vid: Overlay outside of the screen ????\n");
      return -EFAULT;
    }
    shift = (-ov.dst_x)/(double)ov.dst_width*ov.src_width;
    shift = ((shift+3)/2)*2;
    ov.src[0] += shift*2;
    ov.src_width -= shift;
    ov.dst_width += ov.dst_x;
    ov.dst_x = 0;
  }

  vidcfg = tdfx_inl(VIDPROCCFG);
  // clear the overlay fmt
  vidcfg &= ~(7 << 21);
  switch(ov.format) {
  case TDFX_VID_FORMAT_BGR15:
    vidcfg |= (1 << 21);
    break;
  case TDFX_VID_FORMAT_BGR16:
    vidcfg |= (7 << 21);
    break;
  case TDFX_VID_FORMAT_YUY2:
    vidcfg |= (5 << 21);
    break;
  case TDFX_VID_FORMAT_UYVY:
    vidcfg |= (6 << 21);
    break;
  default:
    printk(KERN_DEBUG "tdfx_vid: Invalid overlay fmt 0x%x\n",ov.format);
    return -EFAULT;
  }

  // YUV422 need 4 bytes aligned stride and address
  if((ov.format == TDFX_VID_FORMAT_YUY2 ||
      ov.format == TDFX_VID_FORMAT_UYVY)) {
    if((ov.src_stride & ~0x3) != ov.src_stride) {
      printk(KERN_DEBUG "tdfx_vid: YUV need a 4 bytes aligned stride %d\n",ov.src_stride);
      return -EFAULT;
    }
    if((ov.src[0] & ~0x3) != ov.src[0] || (ov.src[1] & ~0x3) != ov.src[1]){
      printk(KERN_DEBUG "tdfx_vid: YUV need a 4 bytes aligned address 0x%x 0x%x\n",ov.src[0],ov.src[1]);
      return -EFAULT;
    }
  }

  // Now we have a good input format
  // but first get the screen size to check a bit
  // if the size/position is valid
  screen_w = tdfx_inl(VIDSCREENSIZE);
  screen_h = (screen_w >> 12) & 0xFFF;
  screen_w &= 0xFFF;
  disp_w =  ov.dst_x + ov.dst_width >= screen_w ?
    screen_w - ov.dst_x : ov.dst_width;
  disp_h =  ov.dst_y + ov.dst_height >= screen_h ?
    screen_h - ov.dst_y : ov.dst_height;

  if(ov.dst_x >= screen_w || ov.dst_y >= screen_h ||
     disp_h <= 0 || disp_h > screen_h || disp_w <= 0 || disp_w > screen_w) {
    printk(KERN_DEBUG "tdfx_vid: Invalid overlay dimension and/or position\n");
    return -EFAULT;
  }
  // Setup the vidproc
  // H scaling
  if(ov.src_width < ov.dst_width)
    vidcfg |= (1<<14);
  else
    vidcfg &= ~(1<<14);
  // V scaling
  if(ov.src_height < ov.dst_height)
    vidcfg |= (1<<15);
  else
    vidcfg &= ~(1<<15);
  // Filtering can only be used in 1x mode
  if(!(vidcfg | (1<<26)))
    vidcfg |= (3<<16);
  else
    vidcfg &= ~(3<<16);
  // disable overlay stereo mode
  vidcfg &= ~(1<<2);
  // Colorkey on/off
  if(ov.use_colorkey) {
    // Colorkey inversion
    if(ov.invert_colorkey)
      vidcfg |= (1<<6);
    else
      vidcfg &= ~(1<<6);
    vidcfg |= (1<<5);
  } else
    vidcfg &= ~(1<<5);
  // Overlay isn't VidIn
  vidcfg &= ~(1<<9);
  // vidcfg |= (1<<8);
  tdfx_outl(VIDPROCCFG,vidcfg);

  // Start coord
  //printk(KERN_DEBUG "tdfx_vid: start %dx%d\n",ov.dst_x & 0xFFF,ov.dst_y & 0xFFF);
  tdfx_outl(VIDOVRSTARTCRD,(ov.dst_x & 0xFFF)|((ov.dst_y & 0xFFF)<<12));
  // End coord
  tdfx_outl(VIDOVRENDCRD, ((ov.dst_x + disp_w-1) & 0xFFF)|
	    (((ov.dst_y + disp_h-1) & 0xFFF)<<12));
  // H Scaling
  tdfx_outl(VIDOVRDUDX,( ((u32)ov.src_width) << 20) / ov.dst_width);
  // Src offset and width (in bytes)
  tdfx_outl(VIDOVRDUDXOFF,((ov.src_width<<1) & 0xFFF) << 19);
  // V Scaling
  tdfx_outl(VIDOVRDVDY, ( ((u32)ov.src_height) << 20) / ov.dst_height);
  //else
  //  tdfx_outl(VIDOVRDVDY,0);
  // V Offset
  tdfx_outl(VIDOVRDVDYOFF,0);
  // Overlay stride
  stride = tdfx_inl(VIDDESKSTRIDE) & 0xFFFF;
  tdfx_outl(VIDDESKSTRIDE,stride | (((u32)ov.src_stride) << 16));
  // Buffers address
  tdfx_outl(LEFTOVBUF, ov.src[0]);
  tdfx_outl(RIGHTOVBUF, ov.src[1]);

  // Send a swap buffer cmd if we are not on one of the 2 buffers
  vidbuf = tdfx_inl(VIDCUROVRSTART);
  if(vidbuf != ov.src[0] && vidbuf != ov.src[1]) {
    tdfx_outl(SWAPPENDING,0);
    tdfx_outl(SWAPBUFCMD, 1);
  }
  //printk(KERN_DEBUG "tdfx_vid: Buf0=0x%x Buf1=0x%x Current=0x%x\n",
  //	 ov.src[0],ov.src[1],tdfx_inl(VIDCUROVRSTART));
  // Colorkey
  if(ov.use_colorkey) {
    tdfx_outl(VIDCHRMIN,ov.colorkey[0]);
    tdfx_outl(VIDCHRMAX,ov.colorkey[1]);
  }

  return 0;
}

static int tdfx_vid_overlay_on(void) {
  uint32_t vidcfg = tdfx_inl(VIDPROCCFG);
  //return 0;
  if(vidcfg & (1<<8)) { // Overlay is already on
    //printk(KERN_DEBUG "tdfx_vid: Overlay is already on.\n");
    return -EFAULT;
  }
  vidcfg |= (1<<8);
  tdfx_outl(VIDPROCCFG,vidcfg);
  return 0;
}

static int tdfx_vid_overlay_off(void) {
  uint32_t vidcfg = tdfx_inl(VIDPROCCFG);

  if(vidcfg & (1<<8)) {
    vidcfg &= ~(1<<8);
    tdfx_outl(VIDPROCCFG,vidcfg);
    return 0;
  }

  printk(KERN_DEBUG "tdfx_vid: Overlay is already off.\n");
  return -EFAULT;
}


static int tdfx_vid_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  tdfx_vid_agp_move_t move;
  tdfx_vid_config_t cfg;
  tdfx_vid_blit_t blit;
  u16 int16;

  switch(cmd) {
  case TDFX_VID_AGP_MOVE:
    if(copy_from_user(&move,(tdfx_vid_agp_move_t*)arg,sizeof(tdfx_vid_agp_move_t))) {
      printk(KERN_INFO "tdfx_vid:failed copy from userspace\n");
      return -EFAULT;
    }
    return agp_move(&move);
  case TDFX_VID_BUMP0:
    if(copy_from_user(&int16,(u16*)arg,sizeof(u16))) {
      printk(KERN_INFO "tdfx_vid:failed copy from userspace\n");
      return -EFAULT;
    }
    return bump_fifo(int16);
  case TDFX_VID_BLIT:
    if(copy_from_user(&blit,(tdfx_vid_blit_t*)arg,sizeof(tdfx_vid_blit_t))) {
      printk(KERN_INFO "tdfx_vid:failed copy from userspace\n");
      return -EFAULT;
    }
    if(!tdfx_vid_blit(&blit)) {
      printk(KERN_INFO "tdfx_vid: Blit failed\n");
      return -EFAULT;
    }
    return 0;
  case TDFX_VID_GET_CONFIG:
    if(copy_from_user(&cfg,(tdfx_vid_config_t*)arg,sizeof(tdfx_vid_config_t))) {
      printk(KERN_INFO "tdfx_vid:failed copy from userspace\n");
      return -EFAULT;
    }
    tdfx_vid_get_config(&cfg);
    if(copy_to_user((tdfx_vid_config_t*)arg,&cfg,sizeof(tdfx_vid_config_t))) {
      printk(KERN_INFO "tdfx_vid:failed copy to userspace\n");
      return -EFAULT;
    }
    return 0;
  case TDFX_VID_SET_YUV:
    return tdfx_vid_set_yuv(arg);
  case TDFX_VID_GET_YUV:
    return tdfx_vid_get_yuv(arg);
  case TDFX_VID_SET_OVERLAY:
    return tdfx_vid_set_overlay(arg);
  case TDFX_VID_OVERLAY_ON:
    return tdfx_vid_overlay_on();
  case TDFX_VID_OVERLAY_OFF:
    return tdfx_vid_overlay_off();
  default:
    printk(KERN_ERR "tdfx_vid: Invalid ioctl %d\n",cmd);
    return -EINVAL;
  } 
  return 0;
}



static ssize_t tdfx_vid_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t tdfx_vid_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{

	return 0;
}

static void tdfx_vid_mopen(struct vm_area_struct *vma) {
  int i;
  struct page *page;
  unsigned long phys;

  printk(KERN_DEBUG "tdfx_vid: mopen\n");
  
  for(i = 0 ; i < agp_mem->page_count ; i++) {
    phys = agp_mem->memory[i] & ~(0x00000fff);
    page = virt_to_page(phys_to_virt(phys));
    if(!page) {
      printk(KERN_DEBUG "tdfx_vid: Can't get the page %d\%d\n",i,agp_mem->page_count);
      return;
    }
    get_page(page);
  }
  MOD_INC_USE_COUNT;
}

static void tdfx_vid_mclose(struct vm_area_struct *vma)  {
  int i;
  struct page *page;
  unsigned long phys;

  printk(KERN_DEBUG "tdfx_vid: mclose\n");

  for(i = 0 ; i < agp_mem->page_count ; i++) {
    phys = agp_mem->memory[i] & ~(0x00000fff);
    page = virt_to_page(phys_to_virt(phys));
    if(!page) {
      printk(KERN_DEBUG "tdfx_vid: Can't get the page %d\%d\n",i,agp_mem->page_count);
      return;
    }
    put_page(page);
  }

  MOD_DEC_USE_COUNT;
}

static struct page *tdfx_vid_nopage(struct vm_area_struct *vma,
				    unsigned long address, 
				    int write_access) {
  unsigned long off;
  uint32_t n;
  struct page *page;
  unsigned long phys;

  off = address - vma->vm_start + (vma->vm_pgoff<<PAGE_SHIFT);
  n = off / PAGE_SIZE;

  if(n >= agp_mem->page_count) {
    printk(KERN_DEBUG "tdfx_vid: Too far away\n");
    return (struct page *)0UL;
  }
  phys = agp_mem->memory[n] & ~(0x00000fff);
  page = virt_to_page(phys_to_virt(phys));
  if(!page) {
    printk(KERN_DEBUG "tdfx_vid: Can't get the page\n");
    return (struct page *)0UL;
  }
  return page;
}

/* memory handler functions */
static struct vm_operations_struct tdfx_vid_vm_ops = {
  open:   tdfx_vid_mopen, /* mmap-open */
  close:  tdfx_vid_mclose,/* mmap-close */
  nopage: tdfx_vid_nopage, /* no-page fault handler */
};


static int tdfx_vid_mmap(struct file *file, struct vm_area_struct *vma)
{
  size_t size;
#ifdef MP_DEBUG
  printk(KERN_DEBUG "tdfx_vid: mapping agp memory into userspace\n");
#endif

  size = (vma->vm_end-vma->vm_start + PAGE_SIZE - 1) / PAGE_SIZE;

  if(map_start) { // Ok we map directly in the physcal ram
    if(size*PAGE_SIZE > map_max) {
      printk(KERN_ERR "tdfx_vid: Not enouth mem\n");
      return -EAGAIN;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,3)
    if(remap_page_range(vma, vma->vm_start,map_start,
			vma->vm_end - vma->vm_start, vma->vm_page_prot)) 
#else
    if(remap_page_range(vma->vm_start, (unsigned long)map_start,
			vma->vm_end - vma->vm_start, vma->vm_page_prot)) 
#endif
      {
	printk(KERN_ERR "tdfx_vid: error mapping video memory\n");
	return -EAGAIN;
      }
    printk(KERN_INFO "Physical mem 0x%lx mapped in userspace\n",map_start);
    return 0;
  }

  if(agp_mem)
    return -EAGAIN;

  agp_mem = drm_agp->allocate_memory(size,AGP_NORMAL_MEMORY);
  if(!agp_mem) {
    printk(KERN_ERR "Failed to allocate AGP memory\n");
    return -ENOMEM;
  }

  if(drm_agp->bind_memory(agp_mem,0)) {
    printk(KERN_ERR "Failed to bind the AGP memory\n");
    drm_agp->free_memory(agp_mem);
    agp_mem = NULL;
    return -ENOMEM;
  }

  printk(KERN_INFO "%d pages of AGP mem allocated (%ld/%ld bytes) :)))\n",
	 size,vma->vm_end-vma->vm_start,size*PAGE_SIZE);


  if(tdfx_map_io) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,3)
    if(remap_page_range(vma, vma->vm_start,agp_info.aper_base,
			vma->vm_end - vma->vm_start, vma->vm_page_prot)) 
#else
    if(remap_page_range(vma->vm_start, (unsigned long)agp_info.aper_base,
			vma->vm_end - vma->vm_start, vma->vm_page_prot)) 
#endif
      {
	printk(KERN_ERR "tdfx_vid: error mapping video memory\n");
	return -EAGAIN;
      }
  } else {
    // Never swap it out
    vma->vm_flags |= VM_LOCKED | VM_IO;
    vma->vm_ops = &tdfx_vid_vm_ops;
    vma->vm_ops->open(vma);
    printk(KERN_INFO "Page fault handler ready !!!!!\n");
  }

  return 0;
}


static int tdfx_vid_release(struct inode *inode, struct file *file)
{
#ifdef MP_DEBUG
  printk(KERN_DEBUG "tdfx_vid: Video OFF (release)\n");
#endif

  // Release the agp mem
  if(agp_mem) {
    drm_agp->unbind_memory(agp_mem);
    drm_agp->free_memory(agp_mem);
    agp_mem = NULL;
  }
  
  tdfx_vid_in_use = 0;

  MOD_DEC_USE_COUNT;
  return 0;
}

static long long tdfx_vid_lseek(struct file *file, long long offset, int origin)
{
	return -ESPIPE;
}					 

static int tdfx_vid_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,2)
	int minor = MINOR(inode->i_rdev.value);
#else
	int minor = MINOR(inode->i_rdev);
#endif

	if(minor != 0)
	 return -ENXIO;

	if(tdfx_vid_in_use == 1) 
		return -EBUSY;

	tdfx_vid_in_use = 1;
	MOD_INC_USE_COUNT;
	return 0;
}

#if LINUX_VERSION_CODE >= 0x020400
static struct file_operations tdfx_vid_fops =
{
  llseek:  tdfx_vid_lseek,
  read:	   tdfx_vid_read,
  write:   tdfx_vid_write,
  ioctl:   tdfx_vid_ioctl,
  mmap:	   tdfx_vid_mmap,
  open:    tdfx_vid_open,
  release: tdfx_vid_release
};
#else
static struct file_operations tdfx_vid_fops =
{
  tdfx_vid_lseek,
  tdfx_vid_read,
  tdfx_vid_write,
  NULL,
  NULL,
  tdfx_vid_ioctl,
  tdfx_vid_mmap,
  tdfx_vid_open,
  NULL,
  tdfx_vid_release
};
#endif


int init_module(void)
{
  tdfx_vid_in_use = 0;

  if(register_chrdev(TDFX_VID_MAJOR, "tdfx_vid", &tdfx_vid_fops)) {
    printk(KERN_ERR "tdfx_vid: unable to get major: %d\n", TDFX_VID_MAJOR);
    return -EIO;
  }

  if(!agp_init()) {
    printk(KERN_ERR "tdfx_vid: AGP init failed\n");
    unregister_chrdev(TDFX_VID_MAJOR, "tdfx_vid");
    return -EINVAL;
  }

  if (!tdfx_vid_find_card()) {
    printk(KERN_ERR "tdfx_vid: no supported devices found\n");
    agp_close();
    unregister_chrdev(TDFX_VID_MAJOR, "tdfx_vid");
    return -EINVAL;
  }

  

  return 0;

}

void cleanup_module(void)
{
  if(tdfx_mmio_base)
    iounmap(tdfx_mmio_base);
  agp_close();
  printk(KERN_INFO "tdfx_vid: Cleaning up module\n");
  unregister_chrdev(TDFX_VID_MAJOR, "tdfx_vid");
}
