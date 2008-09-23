/*
 * VIDIX driver for 3DLabs Permedia 2 chipsets.
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
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "vidix.h"
#include "fourcc.h"
#include "dha.h"
#include "pci_ids.h"
#include "pci_names.h"

#include "glint_regs.h"

/* MBytes of video memory to use */
#define PM2_VIDMEM 6

#if 0
#define TRACE_ENTER() fprintf(stderr, "%s: enter\n", __FUNCTION__)
#define TRACE_EXIT() fprintf(stderr, "%s: exit\n", __FUNCTION__)
#else
#define TRACE_ENTER()
#define TRACE_EXIT()
#endif

#define WRITE_REG(offset,val)						\
    *(volatile unsigned long *)(((unsigned char *)(pm2_reg_base)) + offset) = (val)
#define READ_REG(offset)						    \
    *(volatile unsigned long *)(((unsigned char *)(pm2_reg_base)) + offset)

static pciinfo_t pci_info;

static void *pm2_reg_base;
static void *pm2_mem;

static int pm2_vidmem = PM2_VIDMEM;

static vidix_capability_t pm2_cap =
{
    "3DLabs Permedia2 driver",
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

static unsigned int pm2_card_ids[] =
{
    (VENDOR_3DLABS << 16) | DEVICE_3DLABS_PERMEDIA2,
    (VENDOR_TEXAS << 16) | DEVICE_TEXAS_TVP4020_PERMEDIA_2
};

static int find_chip(unsigned int vendor, uint32_t chip_id)
{
    unsigned int vci = (vendor << 16) | chip_id;
    unsigned i;
    for(i = 0; i < sizeof(pm2_card_ids)/sizeof(unsigned int); i++){
	if(vci == pm2_card_ids[i]) return i;
    }
    return -1;
}

static int pm2_probe(int verbose, int force)
{
    pciinfo_t lst[MAX_PCI_DEVICES];
    unsigned i,num_pci;
    int err;

    err = pci_scan(lst,&num_pci);
    if(err)
    {
	printf("[pm2] Error occurred during pci scan: %s\n",strerror(err));
	return err;
    }
    else
    {
	err = ENXIO;
	for(i=0; i < num_pci; i++)
	{
	    int idx;
	    const char *dname;
	    idx = find_chip(lst[i].vendor, lst[i].device);
	    if(idx == -1)
		continue;
	    dname = pci_device_name(lst[i].vendor, lst[i].device);
	    dname = dname ? dname : "Unknown chip";
	    printf("[pm2] Found chip: %s\n", dname);
	    pm2_cap.device_id = lst[i].device;
	    err = 0;
	    memcpy(&pci_info, &lst[i], sizeof(pciinfo_t));
	    break;
	}
    }
    if(err && verbose) printf("[pm2] Can't find chip.\n");
    return err;
}

#define PRINT_REG(reg)							\
{									\
    long _foo = READ_REG(reg);						\
    printf("[pm2] " #reg " (%x) = %#lx (%li)\n", reg, _foo, _foo);	\
}

static int pm2_init(void)
{
    char *vm;
    pm2_reg_base = map_phys_mem(pci_info.base0, 0x10000);
    pm2_mem = map_phys_mem(pci_info.base1, 1 << 23);
    if((vm = getenv("PM2_VIDMEM"))){
	pm2_vidmem = strtol(vm, NULL, 0);
    }
    return 0;
}

static void pm2_destroy(void)
{
    unmap_phys_mem(pm2_reg_base, 0x10000);
    unmap_phys_mem(pm2_mem, 1 << 23);
}

static int pm2_get_caps(vidix_capability_t *to)
{
    memcpy(to, &pm2_cap, sizeof(vidix_capability_t));
    return 0;
}

static int is_supported_fourcc(uint32_t fourcc)
{
    switch(fourcc){
    case IMGFMT_YUY2:
	return 1;
    default:
	return 0;
    }
}

static int pm2_query_fourcc(vidix_fourcc_t *to)
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

#define FORMAT_YUV422	((1 << 6) | 3 | (1 << 4))

#define PPROD(a,b,c) (a | (b << 3) | (c << 6))

static unsigned int ppcodes[][2] = {
    {0, 0},
    {32, PPROD(1, 0, 0)},
    {64, PPROD(1, 1, 0)},
    {96, PPROD(1, 1, 1)},
    {128, PPROD(2, 1, 1)},
    {160, PPROD(2, 2, 1)},
    {192, PPROD(2, 2, 2)},
    {224, PPROD(3, 2, 1)},
    {256, PPROD(3, 2, 2)},
    {288, PPROD(3, 3, 1)},
    {320, PPROD(3, 3, 2)},
    {384, PPROD(3, 3, 3)},
    {416, PPROD(4, 3, 1)},
    {448, PPROD(4, 3, 2)},
    {512, PPROD(4, 3, 3)},
    {544, PPROD(4, 4, 1)},
    {576, PPROD(4, 4, 2)},
    {640, PPROD(4, 4, 3)},
    {768, PPROD(4, 4, 4)},
    {800, PPROD(5, 4, 1)},
    {832, PPROD(5, 4, 2)},
    {896, PPROD(5, 4, 3)},
    {1024, PPROD(5, 4, 4)},
    {1056, PPROD(5, 5, 1)},
    {1088, PPROD(5, 5, 2)},
    {1152, PPROD(5, 5, 3)},
    {1280, PPROD(5, 5, 4)},
    {1536, PPROD(5, 5, 5)},
    {1568, PPROD(6, 5, 1)},
    {1600, PPROD(6, 5, 2)},
    {1664, PPROD(6, 5, 3)},
    {1792, PPROD(6, 5, 4)},
    {2048, PPROD(6, 5, 5)}
};

static int frames[VID_PLAY_MAXFRAMES];

static int pm2_config_playback(vidix_playback_t *info)
{
    unsigned int src_w, drw_w;
    unsigned int src_h, drw_h;
    long base0;
    unsigned int stride, sstr;
    unsigned int format;
    unsigned int i;
    unsigned int ppcode = 0, sppc = 0;
    unsigned int pitch = 0;

    TRACE_ENTER();

    switch(info->fourcc){
    case IMGFMT_YUY2:
	format = FORMAT_YUV422;
	break;
    default:
	return -1;
    }

    src_w = info->src.w;
    src_h = info->src.h;

    drw_w = info->dest.w;
    drw_h = info->dest.h;

    sstr = READ_REG(PMScreenStride) * 2;

    stride = 0;
    for(i = 1; i < sizeof(ppcodes) / sizeof(ppcodes[0]); i++){
	if((!stride) && (ppcodes[i][0] >= src_w)){
	    stride = ppcodes[i][0];
	    ppcode = ppcodes[i][1];
	    pitch = ppcodes[i][0] - ppcodes[i-1][0];
	}
	if(ppcodes[i][0] == sstr)
	    sppc = ppcodes[i][1];
    }

    if(!stride)
	return -1;

    info->num_frames = pm2_vidmem*1024*1024 / (stride * src_h * 2);
    if(info->num_frames > VID_PLAY_MAXFRAMES)
	info->num_frames = VID_PLAY_MAXFRAMES;

    /* Use end of video memory. Assume the card has 8 MB */
    base0 = (8 - pm2_vidmem)*1024*1024;
    info->dga_addr = pm2_mem + base0;

    info->dest.pitch.y = pitch*2;
    info->dest.pitch.u = 0;
    info->dest.pitch.v = 0;
    info->offset.y = 0;
    info->offset.v = 0;
    info->offset.u = 0;
    info->frame_size = stride * src_h * 2;

    for(i = 0; i < info->num_frames; i++){
	info->offsets[i] = info->frame_size * i;
	frames[i] = (base0 + info->offsets[i]) >> 1;
    }

    WRITE_REG(WindowOrigin, 0);
    WRITE_REG(dY, 1 << 16);
    WRITE_REG(RasterizerMode, 0);
    WRITE_REG(ScissorMode, 0);
    WRITE_REG(AreaStippleMode, 0);
    WRITE_REG(StencilMode, 0);
    WRITE_REG(TextureAddressMode, 1);

    WRITE_REG(dSdyDom, 0);
    WRITE_REG(dTdx, 0);

    WRITE_REG(PMTextureMapFormat, (1 << 19) | ppcode);
    WRITE_REG(PMTextureDataFormat, format);
    WRITE_REG(PMTextureReadMode, (1 << 17) | /* FilterMode */
	      (11 << 13) | (11 << 9) /* TextureSize log2 */ | 1);
    WRITE_REG(ColorDDAMode, 0);
    WRITE_REG(TextureColorMode, (0 << 4) /* RGB */ | (3 << 1) /* Copy */ | 1);
    WRITE_REG(AlphaBlendMode, 0);
    WRITE_REG(DitherMode, (1 << 10) | 1);
    WRITE_REG(LogicalOpMode, 0);
    WRITE_REG(FBReadMode, sppc);
    WRITE_REG(FBHardwareWriteMask, 0xFFFFFFFF);
    WRITE_REG(FBWriteMode, 1);
    WRITE_REG(YUVMode, 1);

    WRITE_REG(SStart, 0);
    WRITE_REG(TStart, 0);

    WRITE_REG(dSdx, (src_w << 20) / drw_w);
    WRITE_REG(dTdyDom, (src_h << 20) / drw_h);
    WRITE_REG(RectangleOrigin, info->dest.x | (info->dest.y << 16));
    WRITE_REG(RectangleSize, (drw_h << 16) | drw_w);

    TRACE_EXIT();
    return 0;
}

static int pm2_playback_on(void)
{
    TRACE_ENTER();

    TRACE_EXIT();
    return 0;
}

static int pm2_playback_off(void)
{
    WRITE_REG(YUVMode, 0);
    WRITE_REG(TextureColorMode, 0);
    WRITE_REG(TextureAddressMode, 0);
    WRITE_REG(TextureReadMode, 0);
    return 0;
}

static int pm2_frame_select(unsigned int frame)
{
    WRITE_REG(PMTextureBaseAddress, frames[frame]);
    WRITE_REG(Render, PrimitiveRectangle | XPositive | YPositive |
	      TextureEnable);
    return 0;
}

VDXDriver pm2_drv = {
  "pm2",
  NULL,
  .probe = pm2_probe,
  .get_caps = pm2_get_caps,
  .query_fourcc = pm2_query_fourcc,
  .init = pm2_init,
  .destroy = pm2_destroy,
  .config_playback = pm2_config_playback,
  .playback_on = pm2_playback_on,
  .playback_off = pm2_playback_off,
  .frame_sel = pm2_frame_select,
};
