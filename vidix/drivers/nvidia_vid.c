#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "../vidix.h"
#include "../fourcc.h"
#include "../../libdha/libdha.h"
#include "../../libdha/pci_ids.h"
#include "../../libdha/pci_names.h"

//#include "nvidia.h"

static void *mmio_base = 0;
static void *mem_base = 0;
static int32_t overlay_offset = 0;
static uint32_t ram_size = 0;

#define CARD_FLAGS_NONE		0x00
#define CARD_FLAGS_NOTSUPPORTED	0x01

struct nv_card_id_s
{
    const unsigned int id ;
    const char name[17];
    const int core;
    const int flags;
};

static const struct nv_card_id_s nv_card_ids[]=
{
    { DEVICE_NVIDIA_RIVA_TNT2_NV5, "nVidia TNT2 (NV5) ", 5, CARD_FLAGS_NOTSUPPORTED},
    { DEVICE_NVIDIA_VANTA_NV6, "nVidia Vanta (NV6.1)", 6, CARD_FLAGS_NOTSUPPORTED},
    { DEVICE_NVIDIA_VANTA_NV62, "nVidia Vanta (NV6.2)", 6, CARD_FLAGS_NOTSUPPORTED},
};

static int find_chip(unsigned int chip_id)
{
    unsigned int i;
    
    for (i = 0; i < sizeof(nv_card_ids)/sizeof(struct nv_card_id_s); i++)
	if (chip_id == nv_card_ids[i].id)
	    return(i);
    return(-1);
}

static pciinfo_t pci_info;
static int probed = 0;

/* VIDIX exports */

static vidix_capability_t nvidia_cap =
{
    "NVIDIA driver for VIDIX",
    TYPE_OUTPUT,
    0,
    1,
    0,
    0,
    1024,
    768,
    4,
    4,
    -1,
    FLAG_NONE,
    VENDOR_NVIDIA,
    0,
    { 0, 0, 0, 0 }
};

unsigned int vixGetVersion(void)
{
    return(VIDIX_VERSION);
}

int vixProbe(int verbose)
{
    pciinfo_t lst[MAX_PCI_DEVICES];
    unsigned int i, num_pci;
    int err;
    
    printf("[nvidia] probe\n");

    err = pci_scan(lst, &num_pci);
    if (err)
    {
	printf("Error occured during pci scan: %s\n", strerror(err));
	return err;
    }
    else
    {
	err = ENXIO;
	
	for (i = 0; i < num_pci; i++)
	{
	    if (lst[i].vendor == VENDOR_NVIDIA)
	    {
		int idx;
		
		idx = find_chip(lst[i].device);
		if (idx == -1)
		    continue;
		if (nv_card_ids[idx].flags & CARD_FLAGS_NOTSUPPORTED)
		{
		    printf("Found chip: %s, but not supported!\n",
			nv_card_ids[idx].name);
		    continue;
		}
		else
		
		    printf("Found chip: %s\n", nv_card_ids[idx].name);
		nvidia_cap.device_id = nv_card_ids[idx].id;
		err = 0;
		memcpy(&pci_info, &lst[i], sizeof(pciinfo_t));
		probed = 1;

		printf("bus:card:func = %x:%x:%x\n",
		    pci_info.bus, pci_info.card, pci_info.func);
		printf("vendor:device = %x:%x\n",
		    pci_info.vendor, pci_info.device);
		printf("base0:base1:base2:baserom = %x:%x:%x:%x\n",
		    pci_info.base0, pci_info.base1, pci_info.base2,
		    pci_info.baserom);
		break;
	    }
	}
    }

    if (err)
	printf("No chip found\n");
    return(err);
}

int vixInit(void)
{
    printf("[nvidia] init\n");
    
    if (!probed)
    {
	printf("Driver was not probed but is being initialized\n");
	return(EINTR);
    }
    
    mmio_base = map_phys_mem(pci_info.base0, 0xFFFF);
    if (mmio_base == (void *)-1)
	return(ENOMEM);

    printf("mmio_base: %p\n", mmio_base);
    return ENXIO;
}

void vixDestroy(void)
{
    printf("[nvidia] destory\n");
}

int vixGetCapability(vidix_capability_t *to)
{
    memcpy(to, &nvidia_cap, sizeof(vidix_capability_t));
    return(0);
}

int vixQueryFourcc(vidix_fourcc_t *to)
{
    printf("[nvidia] query fourcc (%x)\n", to->fourcc);
    return 0;
}

int vixConfigPlayback(vidix_playback_t *info)
{
    printf("[nvidia] config playback\n");
    
    info->num_frames = 2;
    info->frame_size = info->src.w*info->src.h+(info->src.w*info->src.h)/2;
    info->dga_addr = malloc(info->num_frames*info->frame_size);
    return 0;
}

int vixPlaybackOn(void)
{
    printf("[nvidia] playback on\n");
    return 0;
}

int vixPlaybackOff(void)
{
    printf("[nvidia] playback off\n");
    return 0;
}
