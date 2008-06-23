/*
 * VIDIX driver for 3DLabs Glint R3 and Permedia 3 chipsets.
 * Copyright (C) 2002 Måns Rullgård
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
#include <inttypes.h>
#include <unistd.h>

#include "config.h"
#include "vidix.h"
#include "fourcc.h"
#include "dha.h"
#include "pci_ids.h"
#include "pci_names.h"

#include "pm3_regs.h"

#if 0
#define TRACE_ENTER() fprintf(stderr, "%s: enter\n", __FUNCTION__)
#define TRACE_EXIT() fprintf(stderr, "%s: exit\n", __FUNCTION__)
#else
#define TRACE_ENTER()
#define TRACE_EXIT()
#endif

static pciinfo_t pci_info;

void *pm3_reg_base;
static void *pm3_mem;

static vidix_capability_t pm3_cap =
{
    "3DLabs GLINT R3/Permedia3 driver",
    "Måns Rullgård <mru@users.sf.net>",
    TYPE_OUTPUT,
    { 0, 0, 0, 0 },
    2048,
    2048,
    4,
    4,
    -1,
    FLAG_UPSCALER|FLAG_DOWNSCALER,
    VENDOR_3DLABS,
    -1,
    { 0, 0, 0, 0 }
};

static unsigned short pm3_card_ids[] = 
{
    DEVICE_3DLABS_GLINT_R3
};

static int find_chip(unsigned chip_id)
{
  unsigned i;
  for(i = 0;i < sizeof(pm3_card_ids)/sizeof(unsigned short);i++)
  {
    if(chip_id == pm3_card_ids[i]) return i;
  }
  return -1;
}

static int pm3_probe(int verbose, int force)
{
    pciinfo_t lst[MAX_PCI_DEVICES];
    unsigned i,num_pci;
    int err;

    err = pci_scan(lst,&num_pci);
    if(err)
    {
	printf("[pm3] Error occurred during pci scan: %s\n",strerror(err));
	return err;
    }
    else
    {
	err = ENXIO;
	for(i=0; i < num_pci; i++)
	{
	    if(lst[i].vendor == VENDOR_3DLABS)
	    {
		int idx;
		const char *dname;
		idx = find_chip(lst[i].device);
		if(idx == -1)
		    continue;
		dname = pci_device_name(VENDOR_3DLABS, lst[i].device);
		dname = dname ? dname : "Unknown chip";
		printf("[pm3] Found chip: %s\n", dname);
#if 0
		if ((lst[i].command & PCI_COMMAND_IO) == 0)
		{
			printf("[pm3] Device is disabled, ignoring\n");
			continue;
		}
#endif
		pm3_cap.device_id = lst[i].device;
		err = 0;
		memcpy(&pci_info, &lst[i], sizeof(pciinfo_t));
		break;
	    }
	}
    }
    if(err && verbose) printf("[pm3] Can't find chip\n");
    return err;
}

#define PRINT_REG(reg)							\
{									\
    long _foo = READ_REG(reg);						\
    printf("[pm3] " #reg " (%x) = %#lx (%li)\n", reg, _foo, _foo);	\
}

static int pm3_init(void)
{
    pm3_reg_base = map_phys_mem(pci_info.base0, 0x20000);
    pm3_mem = map_phys_mem(pci_info.base2, 0x2000000);
    return 0;
}

static void pm3_destroy(void)
{
    unmap_phys_mem(pm3_reg_base, 0x20000);
    unmap_phys_mem(pm3_mem, 0x2000000);
}

static int pm3_get_caps(vidix_capability_t *to)
{
    memcpy(to, &pm3_cap, sizeof(vidix_capability_t));
    return 0;
}

static int is_supported_fourcc(uint32_t fourcc)
{
    switch(fourcc){
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
	return 1;
    default:
	return 0;
    }
}

static int pm3_query_fourcc(vidix_fourcc_t *to)
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

#define FORMAT_RGB8888	PM3VideoOverlayMode_COLORFORMAT_RGB8888 
#define FORMAT_RGB4444	PM3VideoOverlayMode_COLORFORMAT_RGB4444
#define FORMAT_RGB5551	PM3VideoOverlayMode_COLORFORMAT_RGB5551
#define FORMAT_RGB565	PM3VideoOverlayMode_COLORFORMAT_RGB565
#define FORMAT_RGB332	PM3VideoOverlayMode_COLORFORMAT_RGB332
#define FORMAT_BGR8888	PM3VideoOverlayMode_COLORFORMAT_BGR8888
#define FORMAT_BGR4444	PM3VideoOverlayMode_COLORFORMAT_BGR4444
#define FORMAT_BGR5551	PM3VideoOverlayMode_COLORFORMAT_BGR5551
#define FORMAT_BGR565	PM3VideoOverlayMode_COLORFORMAT_BGR565
#define FORMAT_BGR332	PM3VideoOverlayMode_COLORFORMAT_BGR332
#define FORMAT_CI8	PM3VideoOverlayMode_COLORFORMAT_CI8
#define FORMAT_VUY444	PM3VideoOverlayMode_COLORFORMAT_VUY444
#define FORMAT_YUV444	PM3VideoOverlayMode_COLORFORMAT_YUV444
#define FORMAT_VUY422	PM3VideoOverlayMode_COLORFORMAT_VUY422
#define FORMAT_YUV422	PM3VideoOverlayMode_COLORFORMAT_YUV422

/* Notice, have to check that we don't overflow the deltas here ... */
static void
compute_scale_factor(
    short* src_w, short* dst_w,
    uint32_t* shrink_delta, uint32_t* zoom_delta)
{
    /* NOTE: If we don't return reasonable values here then the video
     * unit can potential shut off and won't display an image until re-enabled.
     * Seems as though the zoom_delta is o.k, and I've not had the problem.
     * The 'shrink_delta' is prone to this the most - FIXME ! */

    if (*src_w >= *dst_w) {
	*src_w &= ~0x3;
	*dst_w &= ~0x3;
	*shrink_delta = (((*src_w << 16) / *dst_w) + 0x0f) & 0x0ffffff0;
	*zoom_delta = 1<<16;
	if ( ((*shrink_delta * *dst_w) >> 16) & 0x03 )
	    *shrink_delta += 0x10;
    } else {
	*src_w &= ~0x3;
	*dst_w &= ~0x3;
	*zoom_delta = (((*src_w << 16) / *dst_w) + 0x0f) & 0x0001fff0;
	*shrink_delta = 1<<16;
	if ( ((*zoom_delta * *dst_w) >> 16) & 0x03 )
	    *zoom_delta += 0x10;
    }
}

static int frames[VID_PLAY_MAXFRAMES];

static long overlay_mode, overlay_control;

static int pm3_config_playback(vidix_playback_t *info)
{
    uint32_t shrink, zoom;
    short src_w, drw_w;
    short src_h, drw_h;
    long base0;
    int pitch;
    int format;
    unsigned int i;

    TRACE_ENTER();

    if(!is_supported_fourcc(info->fourcc))
	return -1;

    switch(info->fourcc){
    case IMGFMT_YUY2:
	format = FORMAT_YUV422;
	break;
    case IMGFMT_UYVY:
	format = FORMAT_VUY422;
	break;
    default:
	return -1;
    }

    src_w = info->src.w;
    src_h = info->src.h;

    drw_w = info->dest.w;
    drw_h = info->dest.h;

    pitch = src_w;

    /* Assume we have 16 MB to play with */
    info->num_frames = 0x1000000 / (pitch * src_h * 2);
    if(info->num_frames > VID_PLAY_MAXFRAMES)
	info->num_frames = VID_PLAY_MAXFRAMES;

    /* Start at 16 MB. Let's hope it's not in use. */
    base0 = 0x1000000;
    info->dga_addr = pm3_mem + base0;

    info->dest.pitch.y = 2;
    info->dest.pitch.u = 0;
    info->dest.pitch.v = 0;
    info->offset.y = 0;
    info->offset.v = 0;
    info->offset.u = 0;
    info->frame_size = pitch * src_h * 2;
    for(i = 0; i < info->num_frames; i++){
	info->offsets[i] = info->frame_size * i;
	frames[i] = (base0 + info->offsets[i]) >> 1;
    }

    compute_scale_factor(&src_w, &drw_w, &shrink, &zoom);

    WRITE_REG(PM3VideoOverlayBase0, base0 >> 1);
    WRITE_REG(PM3VideoOverlayStride, PM3VideoOverlayStride_STRIDE(pitch));
    WRITE_REG(PM3VideoOverlayWidth, PM3VideoOverlayWidth_WIDTH(src_w));
    WRITE_REG(PM3VideoOverlayHeight, PM3VideoOverlayHeight_HEIGHT(src_h));
    WRITE_REG(PM3VideoOverlayOrigin, 0);

    /* Scale the source to the destinationsize */
    if (src_h == drw_h) {
	WRITE_REG(PM3VideoOverlayYDelta, PM3VideoOverlayYDelta_NONE);
    } else {
	WRITE_REG(PM3VideoOverlayYDelta,
		  PM3VideoOverlayYDelta_DELTA(src_h, drw_h));
    }
    if (src_w == drw_w) {
    	WRITE_REG(PM3VideoOverlayShrinkXDelta, 1<<16);
    	WRITE_REG(PM3VideoOverlayZoomXDelta, 1<<16);
    } else {
    	WRITE_REG(PM3VideoOverlayShrinkXDelta, shrink);
    	WRITE_REG(PM3VideoOverlayZoomXDelta, zoom);
    }
    WRITE_REG(PM3VideoOverlayIndex, 0);

    /* Now set the ramdac video overlay region and mode */
    RAMDAC_SET_REG(PM3RD_VideoOverlayXStartLow, (info->dest.x & 0xff));
    RAMDAC_SET_REG(PM3RD_VideoOverlayXStartHigh, (info->dest.x & 0xf00)>>8);
    RAMDAC_SET_REG(PM3RD_VideoOverlayXEndLow, (info->dest.x+drw_w) & 0xff);
    RAMDAC_SET_REG(PM3RD_VideoOverlayXEndHigh,
		   ((info->dest.x+drw_w) & 0xf00)>>8);
    RAMDAC_SET_REG(PM3RD_VideoOverlayYStartLow, (info->dest.y & 0xff)); 
    RAMDAC_SET_REG(PM3RD_VideoOverlayYStartHigh, (info->dest.y & 0xf00)>>8);
    RAMDAC_SET_REG(PM3RD_VideoOverlayYEndLow, (info->dest.y+drw_h) & 0xff); 
    RAMDAC_SET_REG(PM3RD_VideoOverlayYEndHigh,
		   ((info->dest.y+drw_h) & 0xf00)>>8);

    RAMDAC_SET_REG(PM3RD_VideoOverlayKeyR, 0xff);
    RAMDAC_SET_REG(PM3RD_VideoOverlayKeyG, 0x00);
    RAMDAC_SET_REG(PM3RD_VideoOverlayKeyB, 0xff);

    overlay_mode =
	1 << 5 |
	format |
	PM3VideoOverlayMode_FILTER_FULL |
	PM3VideoOverlayMode_BUFFERSYNC_MANUAL |
	PM3VideoOverlayMode_FLIP_VIDEO;

    overlay_control = 
	PM3RD_VideoOverlayControl_KEY_COLOR |
	PM3RD_VideoOverlayControl_MODE_MAINKEY |
	PM3RD_VideoOverlayControl_DIRECTCOLOR_ENABLED;

    TRACE_EXIT();
    return 0;
}

static int pm3_playback_on(void)
{
    TRACE_ENTER();

    WRITE_REG(PM3VideoOverlayMode,
	      overlay_mode | PM3VideoOverlayMode_ENABLE);
    RAMDAC_SET_REG(PM3RD_VideoOverlayControl,
		   overlay_control | PM3RD_VideoOverlayControl_ENABLE);
    WRITE_REG(PM3VideoOverlayUpdate,
	      PM3VideoOverlayUpdate_ENABLE);

    TRACE_EXIT();
    return 0;
}

static int pm3_playback_off(void)
{
    RAMDAC_SET_REG(PM3RD_VideoOverlayControl,
		   PM3RD_VideoOverlayControl_DISABLE);
    WRITE_REG(PM3VideoOverlayMode,
	      PM3VideoOverlayMode_DISABLE);

    RAMDAC_SET_REG(PM3RD_VideoOverlayKeyR, 0x01);
    RAMDAC_SET_REG(PM3RD_VideoOverlayKeyG, 0x01);
    RAMDAC_SET_REG(PM3RD_VideoOverlayKeyB, 0xfe);

    return 0;
}

static int pm3_frame_select(unsigned int frame)
{
    WRITE_REG(PM3VideoOverlayBase0, frames[frame]);
    return 0;
}

VDXDriver pm3_drv = {
  "pm3",
  NULL,
  .probe = pm3_probe,
  .get_caps = pm3_get_caps,
  .query_fourcc = pm3_query_fourcc,
  .init = pm3_init,
  .destroy = pm3_destroy,
  .config_playback = pm3_config_playback,
  .playback_on = pm3_playback_on,
  .playback_off = pm3_playback_off,
  .frame_sel = pm3_frame_select,
};
