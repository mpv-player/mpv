/*
   mach64_vid - VIDIX based video driver for Mach64 and 3DRage chips
   Copyrights 2002 Nick Kurshev. This file is based on sources from
   GATOS (gatos.sf.net) and X11 (www.xfree86.org)
   Licence: GPL
   WARNING: THIS DRIVER WAS UNFINISHED! PLEASE DON'T TRY TO USE THAT!!!
   IT'S JUST SCRATCH FOR VOLUNTEERS!!!
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
static int32_t overlay_offset = 0;
static uint32_t mach64_ram_size = 0;

pciinfo_t pci_info;
static int probed = 0;
static int __verbose = 0;
/* VIDIX exports */

/* MMIO space*/
#define GETREG(TYPE,PTR,OFFZ)		(*((volatile TYPE*)((PTR)+(OFFZ))))
#define SETREG(TYPE,PTR,OFFZ,VAL)	(*((volatile TYPE*)((PTR)+(OFFZ))))=VAL

#define INREG8(addr)		GETREG(uint8_t,(uint32_t)(mach64_mmio_base),(addr^0x100)<<2)
#define OUTREG8(addr,val)	SETREG(uint8_t,(uint32_t)(mach64_mmio_base),(addr^0x100)<<2,val)
#define INREG(addr)		GETREG(uint32_t,(uint32_t)(mach64_mmio_base),(addr^0x100)<<2)
#define OUTREG(addr,val)	SETREG(uint32_t,(uint32_t)(mach64_mmio_base),(addr^0x100)<<2,val)

#define OUTREGP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INREG(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTREG(addr, _tmp);					\
	} while (0)

static __inline__ uint32_t INPLL(uint32_t addr)
{
	OUTREG8(0x491, (addr & 0x0000001f));
	return (INREG(0x492));
}

#define OUTPLL(addr,val)	OUTREG8(0x491, (addr & 0x0000001f) | 0x00000080); \
				OUTREG(0x492, val)
#define OUTPLLP(addr,val,mask)  					\
	do {								\
		unsigned int _tmp = INPLL(addr);			\
		_tmp &= (mask);						\
		_tmp |= (val);						\
		OUTPLL(addr, _tmp);					\
	} while (0)

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

int vixInit(void)
{
  int err;
  if(!probed)
  {
    printf("[mach64] Driver was not probed but is being initializing\n");
    return EINTR;
  }
  if((mach64_mmio_base = map_phys_mem(pci_info.base2,0x4096))==(void *)-1) return ENOMEM;
  mach64_ram_size = INREG(MEM_CNTL) & CTL_MEM_SIZEB;
  if (mach64_ram_size < 8) mach64_ram_size = (mach64_ram_size + 1) * 512;
  else if (mach64_ram_size < 12) mach64_ram_size = (mach64_ram_size - 3) * 1024;
  else mach64_ram_size = (mach64_ram_size - 7) * 2048;
  mach64_ram_size *= 0x400; /* KB -> bytes */
  if((mach64_mem_base = map_phys_mem(pci_info.base0,mach64_ram_size))==(void *)-1) return ENOMEM;
//  memset(&besr,0,sizeof(bes_registers_t));
//  mach64_vid_make_default();
  printf("[mach64] Video memory = %uMb\n",mach64_ram_size/0x100000);
  err = mtrr_set_type(pci_info.base0,mach64_ram_size,MTRR_TYPE_WRCOMB);
  if(!err) printf("[mach64] Set write-combining type of video memory\n");
  return 0;
}

void vixDestroy(void)
{
  unmap_phys_mem(mach64_mem_base,mach64_ram_size);
  unmap_phys_mem(mach64_mmio_base,0x4096);
}

int vixGetCapability(vidix_capability_t *to)
{
    memcpy(to, &mach64_cap, sizeof(vidix_capability_t));
    return 0;
}

int vixQueryFourcc(vidix_fourcc_t *to)
{
    UNUSED(to);
    return ENOSYS;
}

int vixConfigPlayback(vidix_playback_t *info)
{
    UNUSED(info);
    return ENOSYS;
}

int vixPlaybackOn(void)
{
    return ENOSYS;
}

int vixPlaybackOff(void)
{
    return ENOSYS;
}

int vixPlaybackFrameSelect(unsigned int frame)
{
    UNUSED(frame);
    return ENOSYS;
}
