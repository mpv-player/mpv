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

static int fd;

static void *mmio_base = 0;
static void *mem_base = 0;
static int32_t overlay_offset = 0;
static uint32_t ram_size = 0;

static int probed = 0;

/* VIDIX exports */

static vidix_capability_t genfb_cap =
{
    "General Framebuffer",
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
    FLAG_UPSCALER|FLAG_DOWNSCALER,
    -1,
    -1,
    { 0, 0, 0, 0 }
};

unsigned int vixGetVersion(void)
{
    return(VIDIX_VERSION);
}

int vixProbe(int verbose)
{
    int err = 0;
    
    printf("[genfb] probe\n");

    fd = open("/dev/fb0", O_RDWR);
    if (fd < 0)
    {
	printf("Error occured durint open: %s\n", strerror(errno));
	err = errno;
    }
    
    probed = 1;

    return(err);
}

int vixInit(void)
{
    printf("[genfb] init\n");
    
    if (!probed)
    {
	printf("Driver was not probed but is being initialized\n");
	return(EINTR);
    }

    return(0);
}

void vixDestroy(void)
{
    printf("[genfb] destory\n");
    return;
}

int vixGetCapability(vidix_capability_t *to)
{
    memcpy(to, &genfb_cap, sizeof(vidix_capability_t));
    return(0);
}

int vixQueryFourcc(vidix_fourcc_t *to)
{
    printf("[genfb] query fourcc (%x)\n", to->fourcc);

    to->depth = VID_DEPTH_1BPP | VID_DEPTH_2BPP |
		VID_DEPTH_4BPP | VID_DEPTH_8BPP |
		VID_DEPTH_12BPP | VID_DEPTH_15BPP |
		VID_DEPTH_16BPP | VID_DEPTH_24BPP |
		VID_DEPTH_32BPP;

    to->flags = 0;
    return(0);
}

int vixConfigPlayback(vidix_playback_t *info)
{
    printf("[genfb] config playback\n");

    info->num_frames = 2;
    info->frame_size = info->src.w*info->src.h+(info->src.w*info->src.h)/2;
    info->dga_addr = malloc(info->num_frames*info->frame_size);   
    printf("[genfb] frame_size: %d, dga_addr: %x\n",
	info->frame_size, info->dga_addr);

    return(0);
}

int vixPlaybackOn(void)
{
    printf("[genfb] playback on\n");
    return(0);
}

int vixPlaybackOff(void)
{
    printf("[genfb] playback off\n");
    return(0);
}

int vixPlaybackFrameSelect(unsigned int frame)
{
    return(0);
}
