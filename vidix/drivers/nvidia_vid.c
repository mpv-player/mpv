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

#include "nvidia.h"

static void *ctrl_base = 0;
static void *fb_base = 0;
static int32_t overlay_offset = 0;
static uint32_t ram_size = 0;

static unsigned int *PFB;
static unsigned int *PCIO;
static unsigned int *PGRAPH;
static unsigned int *PRAMIN;
static unsigned int *FIFO;
static unsigned int *PMC;

typedef unsigned char U008;

#define NV_WR08(p,i,d)	(((U008 *)(p))[i]=(d))

unsigned int nv_fifo_space = 0;

void CRTCout(unsigned char index, unsigned char val)
{
    NV_WR08(PCIO, 0x3d4, index);
    NV_WR08(PCIO, 0x3d5, val);
}

volatile RivaScaledImage *ScaledImage;

#define CARD_FLAGS_NONE		0x00
#define CARD_FLAGS_NOTSUPPORTED	0x01

struct nv_card_id_s
{
    const unsigned int id ;
    const char name[32];
    const int core;
    const int flags;
};

static const struct nv_card_id_s nv_card_id;

static const struct nv_card_id_s nv_card_ids[]=
{
    { DEVICE_NVIDIA_RIVA_TNT2_NV5, "nVidia TNT2 (NV5) ", 5, CARD_FLAGS_NOTSUPPORTED},
    { DEVICE_NVIDIA_VANTA_NV6, "nVidia Vanta (NV6.1)", 6, CARD_FLAGS_NOTSUPPORTED},
    { DEVICE_NVIDIA_VANTA_NV62, "nVidia Vanta (NV6.2)", 6, CARD_FLAGS_NOTSUPPORTED}
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
    "alex",
    TYPE_OUTPUT,
    { 0, 0, 0, 0 },
    2046,
    2047,
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

int vixProbe(int verbose,int force)
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
		
		memcpy(&nv_card_id, &nv_card_ids[idx], sizeof(struct nv_card_id_s));
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
    int card_option;
    
    printf("[nvidia] init\n");
    
    pci_config_read(pci_info.bus, pci_info.card, pci_info.func, 0x40,
	4, &card_option);
    printf("card_option: %x\n", card_option);
    
    if (!probed)
    {
	printf("Driver was not probed but is being initialized\n");
	return(EINTR);
    }
    
    ctrl_base = map_phys_mem(pci_info.base0, 0x00800000);
    if (ctrl_base == (void *)-1)
	return(ENOMEM);
    fb_base = map_phys_mem(pci_info.base1, 0x01000000);
    if (fb_base == (void *)-1)
	return(ENOMEM);

    printf("ctrl_base: %p, fb_base: %p\n", ctrl_base, fb_base);

    PFB = 	ctrl_base+0x00100000;
    PGRAPH =	ctrl_base+0x00400000;
    PRAMIN =	ctrl_base+0x00710000;
    FIFO =	ctrl_base+0x00800000;
    PCIO =	ctrl_base+0x00601000;
    PMC = 	ctrl_base+0x00000000;
    printf("pfb: %p, pgraph: %p, pramin: %p, fifo: %p, pcio: %p\n",
	PFB, PGRAPH, PRAMIN, FIFO, PCIO);
    
    ScaledImage = FIFO+0x8000/4;
    printf("ScaledImage: %p\n", ScaledImage);

    /* unlock */
    CRTCout(0x11, 0xff);

    printf("fifo_free: %d\n", ScaledImage->fifo_free);

    RIVA_FIFO_FREE(ScaledImage, 10);
    
    dump_scaledimage(ScaledImage);
    
    /* create scaled image object */
    *(PRAMIN+0x518) = 0x0100A037;
    *(PRAMIN+0x519) = 0x00000C02;
    
    /* put scaled image object into subchannel */
    *(FIFO+0x2000) = 0x80000011;

    /* ram size detection */
    switch(nv_card_id.core)
    {
	case 5:
	{
	    if (*(PFB+0x0) & 0x00000100)
	    {
		printf("first ver\n");
		ram_size = ((*(PFB+0x0) >> 12) & 0x0f) * 1024 * 2 + 1024 * 2;
	    }
	    else
	    {
		printf("second ver (code: %d)\n",
		    *(PFB+0x0) & 0x00000003);
		switch(*(PFB+0x0) & 0x00000003)
		{
		    case 0:
			ram_size = 1024*32;
			break;
		    case 1:
			ram_size = 1024*4;
			break;
		    case 2:
			ram_size = 1024*8;
			break;
		    case 3:
			ram_size = 1024*16;
			break;
		    default:
			printf("Unknown ram size code: %d\n",
			    *(PFB+0x0) & 0x00000003);
			break;
		}
	    }
	    break;
	}
	default:
	    printf("Unknown core: %d\n", nv_card_id.core);
    }

    printf("ram_size: %d\n", ram_size);
    return 0;
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
    to->flags = 0;
    to->depth = VID_DEPTH_32BPP;
    return 0;
}

int vixConfigPlayback(vidix_playback_t *info)
{
    int fb_pixel_size = 32/8;
    int fb_line_len = 1280*4;
    char buffer = 0;
    int offset = 0;
    int x,y,h,w;
    int bpp = 32 >> 3;
    int size;

    printf("[nvidia] config playback\n");
    
    x = info->src.x;
    y = info->src.y;
    h = info->src.h;
    w = info->src.w;
    
    w = (w + 1) & ~1;
    
    size = h * (((w << 1) + 63) & ~63) / bpp;
    
    
    PMC[(0x8900/4)+buffer] = offset;
    PMC[(0x8928/4)+buffer] = (h << 16) | w;
    PMC[(0x8930/4)+buffer] = ((y << 4) & 0xffff0000) | (x >> 12);
    PMC[(0x8938/4)+buffer] = (w << 20) / info->dest.w;
    PMC[(0x8938/4)+buffer] = (h << 20) / info->dest.h;
    
    info->dga_addr = fb_base + (info->dest.w - info->src.w) * fb_pixel_size /
		    2 + (info->dest.h - info->src.h) * fb_line_len / 2;
    
    info->num_frames = 1;
    info->frame_size = info->src.w*info->src.h+(info->src.w*info->src.h)/2;
    info->offsets[0] = 0;
    info->offset.y = 0;
    info->offset.v = ((info->src.w + 31) & ~31) * info->src.h;
    info->offset.u = info->offset.v+((info->src.w + 31) & ~31) * info->src.h / 4;
//    info->dga_addr = malloc(info->num_frames*info->frame_size);
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
