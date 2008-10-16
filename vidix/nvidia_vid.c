/*
 * VIDIX driver for nVidia chipsets.
 *
 * Copyright (C) 2003-2004 Sascha Sommer
 * This file is based on sources from RIVATV (rivatv.sf.net)
 * Multi buffer support and TNT2 fixes by Dmitry Baryshkov.
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
#include <unistd.h>


#include "config.h"
#include "vidix.h"
#include "fourcc.h"
#include "dha.h"
#include "pci_ids.h"
#include "pci_names.h"
#include "libavutil/common.h"
#include "mpbswap.h"


static pciinfo_t pci_info;


#define MAX_FRAMES 3
#define NV04_BES_SIZE 1024*2000*4


static vidix_capability_t nvidia_cap = {
    "NVIDIA RIVA OVERLAY DRIVER",
    "Sascha Sommer <saschasommer@freenet.de>",
    TYPE_OUTPUT,
    { 0, 0, 0, 0 },
    2046,
    2046,
    4,
    4,
    -1,
    FLAG_UPSCALER|FLAG_DOWNSCALER,
    VENDOR_NVIDIA2,
    -1,
    { 0, 0, 0, 0 }
};

#define NV_ARCH_03  0x03
#define NV_ARCH_04  0x04
#define NV_ARCH_10  0x10
#define NV_ARCH_20  0x20
#define NV_ARCH_30  0x30
#define NV_ARCH_40  0x40

// since no useful information whatsoever is passed
// to the equalizer functions we need this
static struct {
  uint32_t lum; // luminance (brightness + contrast)
  uint32_t chrom; // chrominance (saturation + hue)
  uint8_t red_off; // for NV03/NV04
  uint8_t green_off;
  uint8_t blue_off;
  vidix_video_eq_t vals;
} eq;

struct nvidia_cards {
  unsigned short chip_id;
  unsigned short arch;
};


static struct nvidia_cards nvidia_card_ids[] = {
  /*NV03*/
  {DEVICE_NVIDIA2_RIVA128, NV_ARCH_03},
  {DEVICE_NVIDIA2_RIVA128ZX,NV_ARCH_03},
  /*NV04*/
  {DEVICE_NVIDIA_NV4_RIVA_TNT,NV_ARCH_04},
  {DEVICE_NVIDIA_NV5_RIVA_TNT2_TNT2,NV_ARCH_04},
  {DEVICE_NVIDIA_NV5_RIVA_TNT2,NV_ARCH_04},  
  {DEVICE_NVIDIA_NV5_RIVA_TNT22,NV_ARCH_04},  
  {DEVICE_NVIDIA_NV5_RIVA_TNT23,NV_ARCH_04},  
  {DEVICE_NVIDIA_NV6_VANTA_VANTA_LT,NV_ARCH_04},
  {DEVICE_NVIDIA_NV5M64_RIVA_TNT2,NV_ARCH_04},
  {DEVICE_NVIDIA_NV6_VANTA,NV_ARCH_04},
  {DEVICE_NVIDIA_NV6_VANTA2,NV_ARCH_04},
  {DEVICE_NVIDIA2_TNT,NV_ARCH_04},
  {DEVICE_NVIDIA2_TNT2,NV_ARCH_04},  
  {DEVICE_NVIDIA2_VTNT2,NV_ARCH_04},  
  {DEVICE_NVIDIA2_UTNT2	,NV_ARCH_04},
  {DEVICE_NVIDIA2_ITNT2,NV_ARCH_04},
  {DEVICE_NVIDIA_NV5_ALADDIN_TNT2,NV_ARCH_04},
  /*NV10*/
  {DEVICE_NVIDIA_NV18_GEFORCE_PCX,NV_ARCH_10},
  {DEVICE_NVIDIA_NV10_GEFORCE_256,NV_ARCH_10},
  {DEVICE_NVIDIA_NV10DDR_GEFORCE_256,NV_ARCH_10},
  {DEVICE_NVIDIA_NV10GL_QUADRO,NV_ARCH_10},
  {DEVICE_NVIDIA_NV11_GEFORCE2_MX_MX,NV_ARCH_10},
  {DEVICE_NVIDIA_NV11DDR_GEFORCE2_MX,NV_ARCH_10},
  {DEVICE_NVIDIA_NV11_GEFORCE2_GO,NV_ARCH_10},
  {DEVICE_NVIDIA_NV11GL_QUADRO2_MXR_EX_GO,NV_ARCH_10},
  {DEVICE_NVIDIA_NV15_GEFORCE2_GTS_PRO,NV_ARCH_10},
  {DEVICE_NVIDIA_NV15DDR_GEFORCE2_TI,NV_ARCH_10},
  {DEVICE_NVIDIA_NV15BR_GEFORCE2_ULTRA,NV_ARCH_10},
  {DEVICE_NVIDIA_NV15GL_QUADRO2_PRO,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_MX,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_MX2,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_MX3,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_MX4,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_440,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_420,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_4202,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_460,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17GL_QUADRO4_550,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_4203,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17GL_QUADRO4_200_400,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17GL_QUADRO4_5502,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17GL_QUADRO4_550,NV_ARCH_10},
  {DEVICE_NVIDIA_NV17_GEFORCE4_410,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18_GEFORCE4_MX,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18_GEFORCE4_MX2,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18_GEFORCE4_MX3,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18_GEFORCE4_MX4,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18_GEFORCE4_MX5,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18M_GEFORCE4_448,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18M_GEFORCE4_488,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18GL_QUADRO_FX,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18GL_QUADRO4_580,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18GL_QUADRO4_NVS,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18GL_QUADRO4_380,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18M_GEFORCE4_4482,NV_ARCH_10},
  {DEVICE_NVIDIA_NVCRUSH11_GEFORCE2_MX,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18_GEFORCE4_MX5,NV_ARCH_10},
  {DEVICE_NVIDIA_NV18_GEFORCE_PCX,NV_ARCH_10}, 
  /*NV20*/
  {DEVICE_NVIDIA_NV20_GEFORCE3,NV_ARCH_20},
  {DEVICE_NVIDIA_NV20_GEFORCE3_TI,NV_ARCH_20},
  {DEVICE_NVIDIA_NV20_GEFORCE3_TI2,NV_ARCH_20},
  {DEVICE_NVIDIA_NV20DCC_QUADRO_DCC,NV_ARCH_20},
  {DEVICE_NVIDIA_NV25_GEFORCE4_TI,NV_ARCH_20},
  {DEVICE_NVIDIA_NV25_GEFORCE4_TI2,NV_ARCH_20},
  {DEVICE_NVIDIA_NV25_GEFORCE4_TI3,NV_ARCH_20},
  {DEVICE_NVIDIA_NV25_GEFORCE4_TI4,NV_ARCH_20},
  {DEVICE_NVIDIA_NV25GL_QUADRO4_900,NV_ARCH_20},
  {DEVICE_NVIDIA_NV25GL_QUADRO4_750,NV_ARCH_20},
  {DEVICE_NVIDIA_NV25GL_QUADRO4_700,NV_ARCH_20},
  {DEVICE_NVIDIA_NV28_GEFORCE4_TI,NV_ARCH_20},
  {DEVICE_NVIDIA_NV28_GEFORCE4_TI2,NV_ARCH_20},
  {DEVICE_NVIDIA_NV28_GEFORCE4_TI3,NV_ARCH_20},
  {DEVICE_NVIDIA_NV28_GEFORCE4_TI4,NV_ARCH_20},
  {DEVICE_NVIDIA_NV28GL_QUADRO4_980,NV_ARCH_20},
  {DEVICE_NVIDIA_NV28GL_QUADRO4_780,NV_ARCH_20},
  {DEVICE_NVIDIA_NV28GLM_QUADRO4_700,NV_ARCH_20},
  /*NV30*/
  {DEVICE_NVIDIA_NV30_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV30_GEFORCE_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV30_GEFORCE_FX3,NV_ARCH_30},
  {DEVICE_NVIDIA_NV30GL_QUADRO_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV30GL_QUADRO_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31_GEFORCE_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31_GEFORCE_FX3,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31M,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31M_PRO,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31M_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31M_GEFORCE_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NVIDIA_QUADRO_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31GLM,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31GLM_PRO,NV_ARCH_30},
  {DEVICE_NVIDIA_NV31GLM_PRO2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34_GEFORCE_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34_GEFORCE_FX3,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34_GEFORCE_FX4,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34M_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34M_GEFORCE_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34_GEFORCE_FX5,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34_GEFORCE_FX6,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34M_GEFORCE_FX3,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34M_GEFORCE_FX4,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34GL_QUADRO_NVS,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34GL_QUADRO_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34GLM_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34_GEFORCE_FX7,NV_ARCH_30},
  {DEVICE_NVIDIA_NV34GL,NV_ARCH_30},
  {DEVICE_NVIDIA_NV35_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV35_GEFORCE_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV35_GEFORCE_FX3,NV_ARCH_30},
  {DEVICE_NVIDIA_NV38_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV35_GEFORCE_FX4,NV_ARCH_30},
  {DEVICE_NVIDIA_NV35GL_QUADRO_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV35GL_QUADRO_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV35_GEFORCE_PCX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_1_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_2_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_4_GEFORCE_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_5,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_GEFORCE_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_GEFORCE_FX3,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_GEFORCE_PCX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36M_PRO,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36MAP,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_QUADRO_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36GL_QUADRO_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36GL,NV_ARCH_30},
  {DEVICE_NVIDIA_NV36_GEFORCE_PCX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV35_GEFORCE_PCX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV37GL_QUADRO_FX,NV_ARCH_30},
  {DEVICE_NVIDIA_NV37GL_QUADRO_FX2,NV_ARCH_30},
  {DEVICE_NVIDIA_NV38GL_QUADRO_FX,NV_ARCH_30},
  /* NV40: GeForce 6x00 to 7x00 */
  {DEVICE_NVIDIA_NV40_GEFORCE_6800,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_68002,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_2_GEFORCE_6800,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_3,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_68003,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_68004,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_68005,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_68006,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_68007,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_68008,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_68009,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_680010,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_680011,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_680012,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_68008,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40GL,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40GL_QUADRO_FX,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40GL_QUADRO_FX2,NV_ARCH_40},
  {DEVICE_NVIDIA_NV41_GEFORCE_6800,NV_ARCH_40},
  {DEVICE_NVIDIA_NV41_1_GEFORCE_6800,NV_ARCH_40},
  {DEVICE_NVIDIA_NV41_2_GEFORCE_6800,NV_ARCH_40},
  {DEVICE_NVIDIA_NV41_8_GEFORCE_GO,NV_ARCH_40},
  {DEVICE_NVIDIA_NV41_9_GEFORCE_GO,NV_ARCH_40},
  {DEVICE_NVIDIA_NV41_QUADRO_FX,NV_ARCH_40},
  {DEVICE_NVIDIA_NV41_QUADRO_FX2,NV_ARCH_40},
  {DEVICE_NVIDIA_NV41GL_QUADRO_FX,NV_ARCH_40},
  {DEVICE_NVIDIA_NV40_GEFORCE_6800_GEFORCE,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_6600_GEFORCE,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_6600_GEFORCE2,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_6200,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_62002,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_6600,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_66002,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_66003,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_66004,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_66005,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_GO,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_GO2,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_GO3,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_GO4,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_GO5,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_GO6,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43_GEFORCE_6610,NV_ARCH_40},
  {DEVICE_NVIDIA_NV43GL_QUADRO_FX,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_6100_NFORCE,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_6100_NFORCE2,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_6100_NFORCE3,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_6100_NFORCE4,NV_ARCH_40},
  {DEVICE_NVIDIA_C51G_GEFORCE_6100,NV_ARCH_40},
  {DEVICE_NVIDIA_C51PV_GEFORCE_6150,NV_ARCH_40},
  {DEVICE_NVIDIA_NV44_GEFORCE_6200,NV_ARCH_40},
  {DEVICE_NVIDIA_NV44_GEFORCE_62002,NV_ARCH_40},
  {DEVICE_NVIDIA_NV44_GEFORCE_62003,NV_ARCH_40},
  {DEVICE_NVIDIA_NV44_GEFORCE_GO,NV_ARCH_40},
  {DEVICE_NVIDIA_NV44_QUADRO_NVS,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_GO_6200,NV_ARCH_40},
  {DEVICE_NVIDIA_NV44A_GEFORCE_6200,NV_ARCH_40},
  {DEVICE_NVIDIA_NV45GL_QUADRO_FX,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_GO_7200,NV_ARCH_40},
  {DEVICE_NVIDIA_QUADRO_NVS_110M,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_GO_7400,NV_ARCH_40},
  {DEVICE_NVIDIA_QUADRO_NVS_110M2,NV_ARCH_40},
  {DEVICE_NVIDIA_QUADRO_FX_350,NV_ARCH_40},
  {DEVICE_NVIDIA_G70_GEFORCE_7300,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_7300_GS,NV_ARCH_40},
  {DEVICE_NVIDIA_G70_GEFORCE_7600,NV_ARCH_40},
  {DEVICE_NVIDIA_G70_GEFORCE_76002,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_7600_GS,NV_ARCH_40},
  {DEVICE_NVIDIA_G70_GEFORCE_GO,NV_ARCH_40},
  {DEVICE_NVIDIA_QUADRO_FX_560,NV_ARCH_40},
  {DEVICE_NVIDIA_G70_GEFORCE_7800,NV_ARCH_40},
  {DEVICE_NVIDIA_G70_GEFORCE_78002,NV_ARCH_40},
  {DEVICE_NVIDIA_G70_GEFORCE_78003,NV_ARCH_40},
  {DEVICE_NVIDIA_G70_GEFORCE_78004,NV_ARCH_40},
  {DEVICE_NVIDIA_G70_GEFORCE_78005,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_GO_7800,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_7900_GTX,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_7900_GT,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_7900_GS,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_GO_7900,NV_ARCH_40},
  {DEVICE_NVIDIA_GEFORCE_GO_79002,NV_ARCH_40},
  {DEVICE_NVIDIA_GE_FORCE_GO,NV_ARCH_40},
  {DEVICE_NVIDIA_G70GL_QUADRO_FX4500,NV_ARCH_40},
  {DEVICE_NVIDIA_G71_QUADRO_FX,NV_ARCH_40},
  {DEVICE_NVIDIA_G71_QUADRO_FX2,NV_ARCH_40}
};


static int find_chip(unsigned chip_id){
  unsigned i;
  for(i = 0;i < sizeof(nvidia_card_ids)/sizeof(struct nvidia_cards);i++)
  {
    if(chip_id == nvidia_card_ids[i].chip_id)return i;
  }
  return -1;
}

static int nv_probe(int verbose, int force){
    pciinfo_t lst[MAX_PCI_DEVICES];
    unsigned i,num_pci;
    int err;

    if (force)
	    printf("[nvidia_vid]: warning: forcing not supported yet!\n");
    err = pci_scan(lst,&num_pci);
    if(err){
	printf("[nvidia_vid] Error occurred during pci scan: %s\n",strerror(err));
	return err;
    }
    else {
	err = ENXIO;
	for(i=0; i < num_pci; i++){
	    if(lst[i].vendor == VENDOR_NVIDIA2 || lst[i].vendor == VENDOR_NVIDIA){
		int idx;
		const char *dname;
		idx = find_chip(lst[i].device);
		if(idx == -1)
		    continue;
		dname = pci_device_name(lst[i].vendor, lst[i].device);
		dname = dname ? dname : "Unknown chip";
		printf("[nvidia_vid] Found chip: %s\n", dname);
		if ((lst[i].command & PCI_COMMAND_IO) == 0){
			printf("[nvidia_vid] Device is disabled, ignoring\n");
			continue;
		}
		nvidia_cap.device_id = lst[i].device;
		err = 0;
		memcpy(&pci_info, &lst[i], sizeof(pciinfo_t));
		break;
	    }
	}
    }
    if(err && verbose) printf("[nvidia_vid] Can't find chip\n");
    return err;
}




/*
 * PCI-Memory IO access macros.
 */

#define MEM_BARRIER() __asm__ volatile ("" : : : "memory")

#undef	VID_WR08
#define VID_WR08(p,i,val) ({ MEM_BARRIER(); ((uint8_t *)(p))[(i)]=(val); })
#undef	VID_RD08
#define VID_RD08(p,i)     ({ MEM_BARRIER(); ((uint8_t *)(p))[(i)]; })

#undef	VID_WR32
#define VID_WR32(p,i,val) ({ MEM_BARRIER(); ((uint32_t *)(p))[(i)/4]=val; })
#undef	VID_RD32
#define VID_RD32(p,i)     ({ MEM_BARRIER(); ((uint32_t *)(p))[(i)/4]; })

#define VID_AND32(p,i,val) VID_WR32(p,i,VID_RD32(p,i)&(val))
#define VID_OR32(p,i,val)  VID_WR32(p,i,VID_RD32(p,i)|(val))
#define VID_XOR32(p,i,val) VID_WR32(p,i,VID_RD32(p,i)^(val))






struct rivatv_chip {
	volatile uint32_t *PMC;	   /* general control			*/
	volatile uint32_t *PME;	   /* multimedia port			*/
	volatile uint32_t *PFB;	   /* framebuffer control		*/
	volatile uint32_t *PVIDEO; /* overlay control			*/
	volatile uint8_t *PCIO;	   /* SVGA (CRTC, ATTR) registers	*/
	volatile uint8_t *PVIO;	   /* SVGA (MISC, GRAPH, SEQ) registers */
	volatile uint32_t *PRAMIN; /* instance memory			*/
	volatile uint32_t *PRAMHT; /* hash table			*/
	volatile uint32_t *PRAMFC; /* fifo context table		*/
	volatile uint32_t *PRAMRO; /* fifo runout table			*/
	volatile uint32_t *PFIFO;  /* fifo control region		*/
	volatile uint32_t *FIFO;   /* fifo channels (USER)		*/
	volatile uint32_t *PGRAPH; /* graphics engine                   */

	unsigned long fbsize;		   /* framebuffer size		   */
	int arch;		   /* compatible NV_ARCH_XX define */
	int realarch;		   /* real architecture		   */
	void (* lock) (struct rivatv_chip *, int);
};
typedef struct rivatv_chip rivatv_chip;


struct rivatv_info {
    unsigned int use_colorkey;    
    unsigned int colorkey; /* saved xv colorkey*/
    unsigned int vidixcolorkey; /*currently used colorkey*/
    unsigned int depth; 
    unsigned int format;
    unsigned int pitch;
    unsigned int width,height;
    unsigned int d_width,d_height;  /*scaled width && height*/
    unsigned int wx,wy;                /*window x && y*/
    unsigned int screen_x;            /*screen width*/
    unsigned int screen_y;            /*screen height*/
	unsigned long buffer_size;		 /* size of the image buffer	       */
	struct rivatv_chip chip;	 /* NV architecture structure		       */
	void* video_base;		 /* virtual address of control region	       */
	void* control_base;		 /* virtual address of fb region	       */
	void* picture_base;		 /* direct pointer to video picture	       */
	unsigned long picture_offset;	 /* offset of video picture in frame buffer    */
//	struct rivatv_dma dma;           /* DMA structure                              */
    unsigned int cur_frame;
	unsigned int num_frames;             /* number of buffers                          */
	int bps;			/* bytes per line */
};
typedef struct rivatv_info rivatv_info;

uint8_t nvReadVGA (struct rivatv_chip *chip, int index) {
	VID_WR08 (chip->PCIO, 0x3D4, index);
	return VID_RD08 (chip->PCIO, 0x3D5);
}

void nvWriteVGA (struct rivatv_chip *chip, int index, int data) {
	VID_WR08 (chip->PCIO, 0x3D4, index);
	VID_WR08 (chip->PCIO, 0x3D5, data);
}

//framebuffer size funcs
static unsigned long rivatv_fbsize_nv03 (struct rivatv_chip *chip){
	if (VID_RD32 (chip->PFB, 0) & 0x00000020) {
		if (((VID_RD32 (chip->PMC, 0) & 0xF0) == 0x20)
		    && ((VID_RD32 (chip->PMC, 0) & 0x0F) >= 0x02)) {
			/* SDRAM 128 ZX. */
			return (1 << (VID_RD32 (chip->PFB, 0) & 0x03)) * 1024 * 1024;
		}
		else {
			return 1024 * 1024 * 8;
		}
	}
	else {
		/* SGRAM 128. */
		switch (VID_RD32(chip->PFB, 0) & 0x00000003) {
		case 0:
			return 1024 * 1024 * 8;
			break;
		case 2:
			return 1024 * 1024 * 4;
			break;
		default:
			return 1024 * 1024 * 2;
			break;
		}
	}
}
static unsigned long rivatv_fbsize_nv04 (struct rivatv_chip *chip){
	if (VID_RD32 (chip->PFB, 0) & 0x00000100) {
		return ((VID_RD32 (chip->PFB, 0) >> 12) & 0x0F) * 1024 * 1024 * 2
			+ 1024 * 1024 * 2;
	} else {
		switch (VID_RD32 (chip->PFB, 0) & 0x00000003) {
		case 0:
			return 1024 * 1024 * 32;
			break;
		case 1:
			return 1024 * 1024 * 4;
			break;
		case 2:
			return 1024 * 1024 * 8;
			break;
		case 3:
		default:
			return 1024 * 1024 * 16;
			break;
		}
	}
}

static unsigned long rivatv_fbsize_nv10 (struct rivatv_chip *chip){
	return VID_RD32 (chip->PFB, 0x20C) & 0xFFF00000;
}

//lock funcs
static void rivatv_lock_nv03 (struct rivatv_chip *chip, int LockUnlock){
	VID_WR08 (chip->PVIO, 0x3C4, 0x06);
	VID_WR08 (chip->PVIO, 0x3C5, LockUnlock ? 0x99 : 0x57);
}

static void rivatv_lock_nv04 (struct rivatv_chip *chip, int LockUnlock){
	rivatv_lock_nv03 (chip, LockUnlock);
	nvWriteVGA (chip, 0x1F, LockUnlock ? 0x99 : 0x57);
}




/* Enable PFB (Framebuffer), PVIDEO (Overlay unit) and PME (Mediaport) if neccessary. */
static void  rivatv_enable_PMEDIA (struct rivatv_info *info){
	uint32_t reg;

	/* switch off interrupts once for a while */
//	VID_WR32 (info->chip.PME, 0x200140, 0x00);
//	VID_WR32 (info->chip.PMC, 0x000140, 0x00);

	reg = VID_RD32 (info->chip.PMC, 0x000200);

	/* NV3 (0x10100010): NV03_PMC_ENABLE_PMEDIA, NV03_PMC_ENABLE_PFB, NV03_PMC_ENABLE_PVIDEO */

	if ((reg & 0x10100010) != 0x10100010) {
		printf("PVIDEO and PFB disabled, enabling...\n");
		VID_OR32 (info->chip.PMC, 0x000200, 0x10100010);
	}

	/* save the current colorkey */
    switch (info->chip.arch ) {
	  case NV_ARCH_10:
	  case NV_ARCH_20:
      case NV_ARCH_30:
      case NV_ARCH_40:
        /* NV_PVIDEO_COLOR_KEY */
	    info->colorkey = VID_RD32 (info->chip.PVIDEO, 0xB00);
        break;
      case NV_ARCH_03:
	  case NV_ARCH_04:
        /* NV_PVIDEO_KEY */
	    info->colorkey = VID_RD32 (info->chip.PVIDEO, 0x240);
        break;
    }       
    

	/* re-enable interrupts again */
//	VID_WR32 (info->chip.PMC, 0x000140, 0x01);
//	VID_WR32 (info->chip.PME, 0x200140, 0x01);
}

/* Stop overlay video. */
static void rivatv_overlay_stop (struct rivatv_info *info) {
	switch (info->chip.arch ) {
	case NV_ARCH_10:
	case NV_ARCH_20:
        case NV_ARCH_30:
        case NV_ARCH_40:
		/* NV_PVIDEO_COLOR_KEY */
		/* Xv-Extension-Hack: Restore previously saved value. */
		VID_WR32 (info->chip.PVIDEO, 0xB00, info->colorkey);
		/* NV_PVIDEO_STOP */
		VID_OR32 (info->chip.PVIDEO, 0x704, 0x11);
		/* NV_PVIDEO_BUFFER */
		VID_AND32 (info->chip.PVIDEO, 0x700, ~0x11);
		/* NV_PVIDEO_INTR_EN_BUFFER */
//		VID_AND32 (info->chip.PVIDEO, 0x140, ~0x11);
		break;
	case NV_ARCH_03:
	case NV_ARCH_04:
		/* NV_PVIDEO_KEY */
		VID_WR32 (info->chip.PVIDEO, 0x240, info->colorkey);
		/* NV_PVIDEO_OVERLAY_VIDEO_OFF */
		VID_AND32 (info->chip.PVIDEO, 0x244, ~0x01);
		/* NV_PVIDEO_INTR_EN_0_NOTIFY */
//		VID_AND32 (info->chip.PVIDEO, 0x140, ~0x01);
		/* NV_PVIDEO_OE_STATE */
		VID_WR32 (info->chip.PVIDEO, 0x224, 0);
		/* NV_PVIDEO_SU_STATE */
		VID_WR32 (info->chip.PVIDEO, 0x228, 0);
		/* NV_PVIDEO_RM_STATE */
		VID_WR32 (info->chip.PVIDEO, 0x22C, 0);
		break;
	}
}

/* Get pan offset of the physical screen. */
static uint32_t rivatv_overlay_pan (struct rivatv_info *info){
	uint32_t pan;
	info->chip.lock (&info->chip, 0);
	pan  =  nvReadVGA (&info->chip, 0x0D);
	pan |=  nvReadVGA (&info->chip, 0x0C) << 8;
	pan |= (nvReadVGA (&info->chip, 0x19) & 0x1F) << 16;
	pan |= (nvReadVGA (&info->chip, 0x2D) & 0x60) << 16;
	return pan << 2;
}

/* Compute and set colorkey depending on the colour depth. */
static void rivatv_overlay_colorkey (rivatv_info* info, unsigned int chromakey){
	uint32_t r, g, b, key = 0;

	r = (chromakey & 0x00FF0000) >> 16;
	g = (chromakey & 0x0000FF00) >> 8;
	b = chromakey & 0x000000FF;
	switch (info->depth) {
	case 15:
		key = ((r >> 3) << 10) | ((g >> 3) << 5) | ((b >> 3));

#if !defined(__MINGW32__) && !defined(__CYGWIN__)
        key = key | 0x00008000;
#endif       
		break;
	case 16: // XXX unchecked
		key = ((r >> 3) << 11) | ((g >> 2) << 5) | ((b >> 3));
#if !defined(__MINGW32__) && !defined(__CYGWIN__)
        key = key | 0x00008000;
#endif       
		break;
	case 24: // XXX unchecked, maybe swap order of masking - FIXME Can the card be in 24 bit mode anyway?
		key = (chromakey & 0x00FFFFFF) | 0x00800000;
		break;
	case 32:
		key = chromakey;
#if !defined(__MINGW32__) && !defined(__CYGWIN__)
        key = key | 0x80000000;
#endif       
		break;
	}
	//printf("[nvidia_vid] depth=%d %08X \n", info->depth, chromakey);
    switch (info->chip.arch) {
	  case NV_ARCH_10:
	  case NV_ARCH_20:
      case NV_ARCH_30:
      case NV_ARCH_40:
        VID_WR32 (info->chip.PVIDEO, 0xB00, key);
        break;
   	  case NV_ARCH_03:
	  case NV_ARCH_04:
        VID_WR32 (info->chip.PVIDEO, 0x240, key);
        break;
    }
}

static void nv_getscreenproperties(struct rivatv_info *info){
  uint32_t bpp=0,x;
  info->chip.lock(&info->chip, 0);
  /*get screen depth*/
  bpp = nvReadVGA (&info->chip, 0x28) & 0x3;
  if((bpp == 2) && (VID_RD32(info->chip.PVIDEO,0x600) & 0x00001000) == 0x0)info->depth=15;
  else info->depth = 0x04 << bpp;
  /*get screen width*/
  info->screen_x = nvReadVGA (&info->chip, 0x1);
  /* NV_PCRTC_HORIZ_EXTRA_DISPLAY_END_8 */
  info->screen_x |= (nvReadVGA (&info->chip, 0x2D) & 0x02) << 7;
  info->screen_x = (info->screen_x + 1) << 3;
  /*get screen height*/
  /* get first 8 bits in VT_DISPLAY_END*/
  info->screen_y = nvReadVGA (&info->chip, 0x12);
  /* get 9th bit in CRTC_OVERFLOW*/
  info->screen_y |= (nvReadVGA (&info->chip, 0x07) & 0x02) << 7;
  /* and the 10th in CRTC_OVERFLOW*/
  info->screen_y |= (nvReadVGA (&info->chip, 0x07) & 0x40) << 3;
  ++info->screen_y;

  if(info->chip.arch >= NV_ARCH_10){
    /* NV_PCRTC_EXTRA_VERT_DISPLAY_END_10 */
    info->screen_y |= (nvReadVGA (&info->chip, 0x25) & 0x02) << 9;
    /* NV_PCRTC_???_VERT_DISPLAY_END_11 */
    info->screen_y |= (nvReadVGA (&info->chip, 0x41) & 0x04) << 9;
  }

  /* NV_PCRTC_OFFSET */
  x  =  nvReadVGA (&info->chip, 0x13);
  /* NV_PCRTC_REPAINT0_OFFSET_10_8 */
  x |= (nvReadVGA (&info->chip, 0x19) & 0xE0) << 3;
  /* NV_PCRTC_EXTRA_OFFSET_11 */
  x |= (nvReadVGA (&info->chip, 0x25) & 0x20) << 6; x <<= 3;
  info->bps = x * bpp;
}




/* Start overlay video. */
static void rivatv_overlay_start (struct rivatv_info *info,int bufno){
    uint32_t base, size, offset, xscale, yscale, pan;
    uint32_t value;
	int x=info->wx, y=info->wy;
	int lwidth=info->d_width, lheight=info->d_height;

    size = info->buffer_size;
	base = info->picture_offset;
	offset = bufno*size;
    /*update depth & dimensions here because it may change with vo vesa or vo fbdev*/
    nv_getscreenproperties(info);

    if(info->depth){
    	/* get pan offset of the physical screen */
     	pan = rivatv_overlay_pan (info);
    	/* adjust window position depending on the pan offset */
    	if (info->bps != 0)
	{
	  x = info->wx - (pan % info->bps) * 8 / info->depth;
    	  y = info->wy - (pan / info->bps);
	}
    } else {
            // we can't adjust the window position correctly in textmode
            // setting y to 8 seems to work ok, though
            if(info->chip.arch < NV_ARCH_10 && y < 8) y = 8;
    }
    
	    /* adjust negative output window variables */
	    if (x < 0) {
		  lwidth = info->d_width + x;
		  offset += (-x * info->width / info->d_width) << 1;
//		offset += (-window->x * port->vld_width / window->width) << 1;
		  x = 0;
	    }
	    if (y < 0) {
		  lheight = info->d_height + y;
		  offset += (-y * info->height / info->d_height * info->width) << 1;
//		offset += (-window->y * port->vld_height / window->height * port->org_width) << 1;
	      y = 0;
	    }

	switch (info->chip.arch) {
	case NV_ARCH_10:
	case NV_ARCH_20:
	case NV_ARCH_30:
        case NV_ARCH_40:

		/* NV_PVIDEO_BASE */
		VID_WR32 (info->chip.PVIDEO, 0x900 + 0, base + offset);
		//VID_WR32 (info->chip.PVIDEO, 0x900 + 4, base);
		/* NV_PVIDEO_LIMIT */
		VID_WR32 (info->chip.PVIDEO, 0x908 + 0, base + offset + size - 1);
		//VID_WR32 (info->chip.PVIDEO, 0x908 + 4, base + size - 1);

		/* extra code for NV20 && NV30 architectures */
		if (info->chip.arch == NV_ARCH_20 || info->chip.arch == NV_ARCH_30 || info->chip.arch == NV_ARCH_40) {
			VID_WR32 (info->chip.PVIDEO, 0x800 + 0, base + offset);
			//VID_WR32 (info->chip.PVIDEO, 0x800 + 4, base);
			VID_WR32 (info->chip.PVIDEO, 0x808 + 0, base + offset + size - 1);
			//VID_WR32 (info->chip.PVIDEO, 0x808 + 4, base + size - 1);
		}

		/* NV_PVIDEO_LUMINANCE */
		VID_WR32 (info->chip.PVIDEO, 0x910 + 0, eq.lum);
		//VID_WR32 (info->chip.PVIDEO, 0x910 + 4, 0x00001000);
		/* NV_PVIDEO_CHROMINANCE */
		VID_WR32 (info->chip.PVIDEO, 0x918 + 0, eq.chrom);
		//VID_WR32 (info->chip.PVIDEO, 0x918 + 4, 0x00001000);

		/* NV_PVIDEO_OFFSET */
		VID_WR32 (info->chip.PVIDEO, 0x920 + 0, 0x0);
		//VID_WR32 (info->chip.PVIDEO, 0x920 + 4, offset + pitch);
		/* NV_PVIDEO_SIZE_IN */
		VID_WR32 (info->chip.PVIDEO, 0x928 + 0, ((info->height) << 16) | info->width);
		//VID_WR32 (info->chip.PVIDEO, 0x928 + 4, ((port->org_height/2) << 16) | port->org_width);
		/* NV_PVIDEO_POINT_IN */
		VID_WR32 (info->chip.PVIDEO, 0x930 + 0, 0x00000000);
		//VID_WR32 (info->chip.PVIDEO, 0x930 + 4, 0x00000000);
		/* NV_PVIDEO_DS_DX_RATIO */
		VID_WR32 (info->chip.PVIDEO, 0x938 + 0, (info->width << 20) / info->d_width);
		//VID_WR32 (info->chip.PVIDEO, 0x938 + 4, (port->org_width << 20) / window->width);
		/* NV_PVIDEO_DT_DY_RATIO */
		VID_WR32 (info->chip.PVIDEO, 0x940 + 0, ((info->height) << 20) / info->d_height);
		//VID_WR32 (info->chip.PVIDEO, 0x940 + 4, ((port->org_height/2) << 20) / window->height);

		/* NV_PVIDEO_POINT_OUT */
		VID_WR32 (info->chip.PVIDEO, 0x948 + 0, ((y + 0) << 16) | x);
		//VID_WR32 (info->chip.PVIDEO, 0x948 + 4, ((y + 0) << 16) | x);
		/* NV_PVIDEO_SIZE_OUT */
		VID_WR32 (info->chip.PVIDEO, 0x950 + 0, (lheight << 16) | lwidth);
		//VID_WR32 (info->chip.PVIDEO, 0x950 + 4, (height << 16) | width);

		/* NV_PVIDEO_FORMAT */
        value = info->pitch;       
	    if(info->use_colorkey)value |= 1 << 20; 
        if(info->format == IMGFMT_YUY2)value |= 1 << 16;
        VID_WR32 (info->chip.PVIDEO, 0x958 + 0, value);
	    //VID_WR32 (info->chip.PVIDEO, 0x958 + 4, (pitch << 1) | 0x00100000);

		/* NV_PVIDEO_INTR_EN_BUFFER */
//		VID_OR32 (info->chip.PVIDEO, 0x140, 0x01/*0x11*/);
		/* NV_PVIDEO_STOP */
		VID_WR32 (info->chip.PVIDEO, 0x704,0x0);
		/* NV_PVIDEO_BUFFER */
		VID_WR32 (info->chip.PVIDEO, 0x700, 0x01/*0x11*/);
		break;

	case NV_ARCH_03:
	case NV_ARCH_04:


		/* NV_PVIDEO_OE_STATE */
		VID_WR32 (info->chip.PVIDEO, 0x224, 0);
		/* NV_PVIDEO_SU_STATE */
		VID_WR32 (info->chip.PVIDEO, 0x228, 0);
		/* NV_PVIDEO_RM_STATE */
		VID_WR32 (info->chip.PVIDEO, 0x22C, 0);

		/* NV_PVIDEO_BUFF0_START_ADDRESS */
		VID_WR32 (info->chip.PVIDEO, 0x20C + 0, base + offset + 0);
		VID_WR32 (info->chip.PVIDEO, 0x20C + 4, base + offset + 0);
		/* NV_PVIDEO_BUFF0_PITCH_LENGTH */
		VID_WR32 (info->chip.PVIDEO, 0x214 + 0, info->pitch);
		VID_WR32 (info->chip.PVIDEO, 0x214 + 4, info->pitch);

		/* NV_PVIDEO_WINDOW_START */
		VID_WR32 (info->chip.PVIDEO, 0x230, (y << 16) | x);
		/* NV_PVIDEO_WINDOW_SIZE */
		VID_WR32 (info->chip.PVIDEO, 0x234, (lheight << 16) | lwidth);
		/* NV_PVIDEO_STEP_SIZE */
		yscale = ((info->height - 1) << 11) / (info->d_height - 1);
		xscale = ((info->width - 1) << 11) / (info->d_width - 1);
		VID_WR32 (info->chip.PVIDEO, 0x200, (yscale << 16) | xscale);

		/* NV_PVIDEO_RED_CSC_OFFSET */
		VID_WR32 (info->chip.PVIDEO, 0x280, eq.red_off);
		/* NV_PVIDEO_GREEN_CSC_OFFSET */
		VID_WR32 (info->chip.PVIDEO, 0x284, eq.green_off);
		/* NV_PVIDEO_BLUE_CSC_OFFSET */
		VID_WR32 (info->chip.PVIDEO, 0x288, eq.blue_off);
		/* NV_PVIDEO_CSC_ADJUST */
		VID_WR32 (info->chip.PVIDEO, 0x28C, 0x00000); /* No colour correction! */

		/* NV_PVIDEO_CONTROL_Y (BLUR_ON, LINE_HALF) */
		VID_WR32 (info->chip.PVIDEO, 0x204, 0x001);
		/* NV_PVIDEO_CONTROL_X (WEIGHT_HEAVY, SHARPENING_ON, SMOOTHING_ON) */
		VID_WR32 (info->chip.PVIDEO, 0x208, 0x111);     /*directx overlay 0x110 */

		/* NV_PVIDEO_FIFO_BURST_LENGTH */
		VID_WR32 (info->chip.PVIDEO, 0x23C, 0x03);
		/* NV_PVIDEO_FIFO_THRES_SIZE */
		VID_WR32 (info->chip.PVIDEO, 0x238, 0x38);   /*windows uses 0x40*/

		/* NV_PVIDEO_BUFF0_OFFSET */
		VID_WR32 (info->chip.PVIDEO, 0x21C + 0, 0);
		VID_WR32 (info->chip.PVIDEO, 0x21C + 4, 0);

		/* NV_PVIDEO_INTR_EN_0_NOTIFY_ENABLED */
//		VID_OR32 (info->chip.PVIDEO, 0x140, 0x01);                                 

		/* NV_PVIDEO_OVERLAY (KEY_ON, VIDEO_ON, FORMAT_CCIR) */
        value = 0x1; /*video on*/
        if(info->format==IMGFMT_YUY2)value |= 0x100;
        if(info->use_colorkey)value |=0x10;       
        VID_WR32 (info->chip.PVIDEO, 0x244, value);

		/* NV_PVIDEO_SU_STATE */
		VID_XOR32 (info->chip.PVIDEO, 0x228, 1 << 16);
		break;
	}
    /*set colorkey*/
    rivatv_overlay_colorkey(info,info->vidixcolorkey);
    
}







static rivatv_info* info;


      
      
static int nv_init(void){
	int mtrr;
  info = calloc(1,sizeof(rivatv_info));
  info->control_base = map_phys_mem(pci_info.base0, 0x00C00000 + 0x00008000);
  info->chip.arch =  nvidia_card_ids[find_chip(pci_info.device)].arch;  
  printf("[nvidia_vid] arch %x register base %p\n",info->chip.arch,info->control_base);
  info->chip.PFIFO  = (uint32_t *) (info->control_base + 0x00002000);
  info->chip.FIFO   = (uint32_t *) (info->control_base + 0x00800000);
  info->chip.PMC    = (uint32_t *) (info->control_base + 0x00000000);
  info->chip.PFB    = (uint32_t *) (info->control_base + 0x00100000);
  info->chip.PME    = (uint32_t *) (info->control_base + 0x00000000);
  info->chip.PCIO   = (uint8_t *)  (info->control_base + 0x00601000);
  info->chip.PVIO   = (uint8_t *)  (info->control_base + 0x000C0000);
  info->chip.PGRAPH = (uint32_t *) (info->control_base + 0x00400000);
  /* setup chip specific functions */
  switch (info->chip.arch) {
	case NV_ARCH_03:
		info->chip.lock = rivatv_lock_nv03;
		info->chip.fbsize = rivatv_fbsize_nv03 (&info->chip);
		info->chip.PVIDEO = (uint32_t *) (info->control_base + 0x00680000);
		break;
	case NV_ARCH_04:
		info->chip.lock = rivatv_lock_nv04;
		info->chip.fbsize = rivatv_fbsize_nv04 (&info->chip);
		info->chip.PRAMIN = (uint32_t *) (info->control_base + 0x00700000);
		info->chip.PVIDEO = (uint32_t *) (info->control_base + 0x00680000);
		break;
	case NV_ARCH_10:
	case NV_ARCH_20:
	case NV_ARCH_30:
        case NV_ARCH_40:
		info->chip.lock = rivatv_lock_nv04;
		info->chip.fbsize = rivatv_fbsize_nv10 (&info->chip);
		info->chip.PRAMIN = (uint32_t *) (info->control_base + 0x00700000);
		info->chip.PVIDEO = (uint32_t *) (info->control_base + 0x00008000);
		break;
  }
  switch (info->chip.arch) {
	case NV_ARCH_03:
    {
        /* This maps framebuffer @6MB, thus 2MB are left for video. */
	    info->video_base = map_phys_mem(pci_info.base1, info->chip.fbsize);
        /* This may trash your screen for resolutions greater than 1024x768, sorry. */
        info->picture_offset = 1024*768* 4 * ((info->chip.fbsize > 4194304)?2:1);
        info->picture_base = info->video_base + info->picture_offset;
        info->chip.PRAMIN = (uint32_t *) (info->video_base + 0x00C00000);
        break;
	}
    case NV_ARCH_04:	
	case NV_ARCH_10:
	case NV_ARCH_20:
    case NV_ARCH_30:
    case NV_ARCH_40:
	{
		info->video_base = map_phys_mem(pci_info.base1, info->chip.fbsize);
		info->picture_offset = info->chip.fbsize - NV04_BES_SIZE;
		if(info->chip.fbsize > 16*1024*1024)
			info->picture_offset -= NV04_BES_SIZE;
//		info->picture_base = (unsigned long)map_phys_mem(pci_info.base1+info->picture_offset,NV04_BES_SIZE);
		info->picture_base = info->video_base + info->picture_offset;
		break;
	}
  }

  printf("[nvidia_vid] detected memory size %u MB\n",(uint32_t)(info->chip.fbsize /1024/1024));

  if ((mtrr = mtrr_set_type(pci_info.base1, info->chip.fbsize, MTRR_TYPE_WRCOMB))!= 0)
	  printf("[nvidia_vid] unable to setup MTRR: %s\n", strerror(mtrr));
  else
	  printf("[nvidia_vid] MTRR set up\n");
  
  nv_getscreenproperties(info);
  if(!info->depth)printf("[nvidia_vid] text mode: %ux%u\n",info->screen_x,info->screen_y);
  else printf("[nvidia_vid] video mode: %ux%u@%u\n",info->screen_x,info->screen_y, info->depth);
 
   
  rivatv_enable_PMEDIA(info);
  info->cur_frame = 0;
  info->use_colorkey = 0;

  eq.lum = 0x00001000;
  eq.chrom = 0x00001000;
  memset(&eq.vals, 0, sizeof(vidix_video_eq_t));
  eq.vals.cap = VEQ_CAP_BRIGHTNESS;
  if (info->chip.arch > NV_ARCH_04)
    eq.vals.cap |= VEQ_CAP_CONTRAST | VEQ_CAP_SATURATION | VEQ_CAP_HUE;
  eq.red_off = 0x69;
  eq.green_off = 0x3e;
  eq.blue_off = 0x89;
  return 0;
}

static void nv_destroy(void){
  unmap_phys_mem(info->control_base ,0x00C00000 + 0x00008000);
  unmap_phys_mem(info->video_base, info->chip.fbsize);
  free(info);
}

static int nv_get_caps(vidix_capability_t *to){
    memcpy(to, &nvidia_cap, sizeof(vidix_capability_t));
    return 0;
}

inline static int is_supported_fourcc(uint32_t fourcc)
{
	if	(fourcc == IMGFMT_UYVY || fourcc == IMGFMT_YUY2)
		return 1;
	else
		return 0;
}

static int nv_query_fourcc(vidix_fourcc_t *to){
    if(is_supported_fourcc(to->fourcc)){
	to->depth = VID_DEPTH_ALL;
	to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK | VID_CAP_COLORKEY;
	return 0;
    }
    else  to->depth = to->flags = 0;
    return ENOSYS;
}

static int nv_config_playback(vidix_playback_t *vinfo){
    uint32_t i;
//    printf("called %s\n", __FUNCTION__);
    if (! is_supported_fourcc(vinfo->fourcc))
	    return ENOSYS;

    info->width = vinfo->src.w;
    info->height = vinfo->src.h;

    info->d_width = vinfo->dest.w;
    info->d_height = vinfo->dest.h;
    info->wx = vinfo->dest.x;
    info->wy = vinfo->dest.y;
    info->format = vinfo->fourcc;

    printf("[nvidia_vid] setting up a %dx%d-%dx%d video window (src %dx%d), format 0x%X\n",
		    info->d_width, info->d_height, info->wx, info->wy, info->width, info->height, vinfo->fourcc);
    
    
    vinfo->dga_addr=info->picture_base;

    switch (vinfo->fourcc)
    {
	    case IMGFMT_YUY2:
	    case IMGFMT_UYVY:

		    vinfo->dest.pitch.y = 64;
		    vinfo->dest.pitch.u = 0;
		    vinfo->dest.pitch.v = 0;

		    vinfo->offset.y = 0;
		    vinfo->offset.v = 0;
		    vinfo->offset.u = 0;
		    info->pitch = ((info->width << 1) + (vinfo->dest.pitch.y-1)) & ~(vinfo->dest.pitch.y-1);
		    vinfo->frame_size = info->pitch * info->height;
		    break;
    }
    info->buffer_size = vinfo->frame_size;
    info->num_frames = vinfo->num_frames= (info->chip.fbsize - info->picture_offset)/vinfo->frame_size;
    if(vinfo->num_frames > MAX_FRAMES)vinfo->num_frames = MAX_FRAMES;
//    vinfo->num_frames = 1;
//    printf("[nvidia_vid] Number of frames %i\n",vinfo->num_frames);
    for(i=0;i <vinfo->num_frames;i++)vinfo->offsets[i] = vinfo->frame_size*i;
    return 0;
}

static int nv_playback_on(void){
    rivatv_overlay_start(info,info->cur_frame);
    return 0;
}

static int nv_playback_off(void){
    rivatv_overlay_stop(info);
    return 0;
}

static int nv_set_gkeys( const vidix_grkey_t * grkey){
  if (grkey->ckey.op == CKEY_FALSE)
  {
    info->use_colorkey = 0;
    printf("[nvidia_vid] colorkeying disabled\n");
  }
  else {
  info->use_colorkey = 1;
  info->vidixcolorkey = ((grkey->ckey.red<<16)|(grkey->ckey.green<<8)|grkey->ckey.blue);
  printf("[nvidia_vid] set colorkey 0x%x\n",info->vidixcolorkey);
  }
  if(info->d_width && info->d_height)rivatv_overlay_start(info,0);
  return 0;
}

static int nv_frame_sel(unsigned int frame){
//  printf("selecting buffer %d\n", frame);
  rivatv_overlay_start(info, frame);
  if (info->num_frames >= 1)
	  info->cur_frame = frame/*(frame+1)%info->num_frames*/;
  return 0;
}

static int nv_set_eq(const vidix_video_eq_t *eq_parm) {
  double angle;
  int16_t chrom_cos, chrom_sin;
  if (eq_parm->cap & VEQ_CAP_BRIGHTNESS)
    eq.vals.brightness = eq_parm->brightness;
  if (eq_parm->cap & VEQ_CAP_CONTRAST)
    eq.vals.contrast = eq_parm->contrast;
  if (eq_parm->cap & VEQ_CAP_SATURATION)
    eq.vals.saturation = eq_parm->saturation;
  if (eq_parm->cap & VEQ_CAP_HUE)
    eq.vals.hue = eq_parm->hue;
  eq.lum = (((eq.vals.brightness * 512 + 500) / 1000) << 16) |
           ((((eq.vals.contrast + 1000) * 8191 + 1000) / 2000) & 0xffff);
  angle = (double)eq.vals.hue / 1000.0 * 3.1415927;
  chrom_cos = ((eq.vals.saturation + 1000) * 8191 * cos(angle) + 1000) / 2000;
  chrom_sin = ((eq.vals.saturation + 1000) * 8191 * sin(angle) + 1000) / 2000;
  eq.chrom = chrom_sin << 16 | chrom_cos;
  eq.red_off = 0x69 - eq.vals.brightness * 62 / 1000;
  eq.green_off = 0x3e + eq.vals.brightness * 62 / 1000;
  eq.blue_off = 0x89 - eq.vals.brightness * 62 / 1000;
  return 0;
}

static int nv_get_eq(vidix_video_eq_t *eq_parm) {
  memcpy(eq_parm, &eq.vals, sizeof(vidix_video_eq_t));
  return 0;
}

VDXDriver nvidia_drv = {
  "nvidia",
  NULL,
  .probe = nv_probe,
  .get_caps = nv_get_caps,
  .query_fourcc = nv_query_fourcc,
  .init = nv_init,
  .destroy = nv_destroy,
  .config_playback = nv_config_playback,
  .playback_on = nv_playback_on,
  .playback_off = nv_playback_off,
  .frame_sel = nv_frame_sel,
  .get_eq = nv_get_eq,
  .set_eq = nv_set_eq,
  .set_gkey = nv_set_gkeys,
};


#if 0
//gcc -o nvidia_vid nvidia_vid.c -I ../ -lm ../vidix/libvidix.a

int main(int argc,char* argv[]){
  if(nv_probe(0,0)){
        printf("no supported chip found\n");
        return 1;
  }
  if(nv_init()){
	printf("could not init\n");
	return 1;
  }
  if(info->chip.arch >= NV_ARCH_10){
    printf("NV_PVIDEO_BASE (0x900) 0x%x\n",VID_RD32(info->chip.PVIDEO, 0x900));
    printf("NV_PVIDEO_LIMIT (0x908) 0x%x\n",VID_RD32(info->chip.PVIDEO, 0x908));
    printf("NV_PVIDEO_OFFSET (0x920) 0x%x\n",VID_RD32(info->chip.PVIDEO, 0x920));
    printf("NV_PVIDEO_FORMAT (0x958) 0x%x\n",VID_RD32(info->chip.PVIDEO, 0x958));
    printf("NV_PVIDEO_STOP (0x704) 0x%x\n",VID_RD32(info->chip.PVIDEO, 0x704));
    printf("NV_PVIDEO_BUFFER (0x700) 0x%x\n",VID_RD32(info->chip.PVIDEO, 0x700));
  }

  nv_destroy();
  return 0;
}

#endif

