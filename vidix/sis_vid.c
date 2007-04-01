/**
    VIDIX driver for SiS 300 and 310/325 series chips.

    Copyright 2003 Jake Page, Sugar Media.

    Based on SiS Xv driver:
    Copyright 2002-2003 by Thomas Winischhofer, Vienna, Austria.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

    2003/10/08 integrated into mplayer/vidix architecture -- Alex Beregszaszi
**/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "vidix.h"
#include "vidixlib.h"
#include "fourcc.h"
#include "../libdha/libdha.h"
#include "../libdha/pci_ids.h"
#include "../libdha/pci_names.h"
#include "../config.h"

#include "sis_regs.h"
#include "sis_defs.h"


/** Random defines **/

#define WATCHDOG_DELAY  500000	/* Watchdog counter for retrace waiting */
#define IMAGE_MIN_WIDTH         32	/* Min and max source image sizes */
#define IMAGE_MIN_HEIGHT        24
#define IMAGE_MAX_WIDTH        720
#define IMAGE_MAX_HEIGHT       576
#define IMAGE_MAX_WIDTH_M650  1920
#define IMAGE_MAX_HEIGHT_M650 1080

#define OVERLAY_MIN_WIDTH       32	/* Minimum overlay sizes */
#define OVERLAY_MIN_HEIGHT      24

#define DISPMODE_SINGLE1 0x1	/* TW: CRT1 only */
#define DISPMODE_SINGLE2 0x2	/* TW: CRT2 only */
#define DISPMODE_MIRROR  0x4	/* TW: CRT1 + CRT2 MIRROR */

#define VMODE_INTERLACED       0x1
#define VMODE_DOUBLESCAN       0x2

typedef struct {
    short x1, y1, x2, y2;
} BoxRec;

typedef struct {
    int pixelFormat;

    uint16_t pitch;
    uint16_t origPitch;

    uint8_t keyOP;
    uint16_t HUSF;
    uint16_t VUSF;
    uint8_t IntBit;
    uint8_t wHPre;

    uint16_t srcW;
    uint16_t srcH;

    BoxRec dstBox;

    uint32_t PSY;
    uint32_t PSV;
    uint32_t PSU;
    uint8_t bobEnable;

    uint8_t contrastCtrl;
    uint8_t contrastFactor;

    uint8_t lineBufSize;

     uint8_t(*VBlankActiveFunc) ();

    uint16_t SCREENheight;

} SISOverlayRec, *SISOverlayPtr;


/** static variable definitions **/
static int sis_probed = 0;
static pciinfo_t pci_info;
unsigned int sis_verbose = 0;

static void *sis_mem_base;
/* static void *sis_reg_base; */
unsigned short sis_iobase;

unsigned int sis_vga_engine = UNKNOWN_VGA;
static unsigned int sis_displaymode = DISPMODE_SINGLE1;
static unsigned int sis_has_two_overlays = 0;
static unsigned int sis_bridge_is_slave = 0;
static unsigned int sis_shift_value = 1;
static unsigned int sis_vmode = 0;
unsigned int sis_vbflags = DISPTYPE_DISP1;
unsigned int sis_overlay_on_crt1 = 1;
int sis_crt1_off = -1;
unsigned int sis_detected_crt2_devices;
unsigned int sis_force_crt2_type = CRT2_DEFAULT;
int sis_device_id = -1;

static int sis_format;
static int sis_Yoff = 0;
static int sis_Voff = 0;
static int sis_Uoff = 0;
static int sis_screen_width = 640;
static int sis_screen_height = 480;

static int sis_frames[VID_PLAY_MAXFRAMES];

static vidix_grkey_t sis_grkey;

static vidix_capability_t sis_cap = {
    "SiS 300/310/325 Video Driver",
    "Jake Page",
    TYPE_OUTPUT,
    {0, 0, 0, 0},
    2048,
    2048,
    4,
    4,
    -1,
    FLAG_UPSCALER | FLAG_DOWNSCALER | FLAG_EQUALIZER,
    VENDOR_SIS,
    -1,
    {0, 0, 0, 0}
};

static vidix_video_eq_t sis_equal = {
    VEQ_CAP_BRIGHTNESS | VEQ_CAP_CONTRAST,
    200, 0, 0, 0, 0, 0, 0, 0
};

static unsigned short sis_card_ids[] = {
    DEVICE_SIS_300,
    DEVICE_SIS_315H,
    DEVICE_SIS_315,
    DEVICE_SIS_315PRO,
    DEVICE_SIS_330,
    DEVICE_SIS_540_VGA,
    DEVICE_SIS_550_VGA,
    DEVICE_SIS_630_VGA,
    DEVICE_SIS_650_VGA
};

/** function declarations **/

extern void sis_init_video_bridge(void);


static void set_overlay(SISOverlayPtr pOverlay, int index);
static void close_overlay(void);
static void calc_scale_factor(SISOverlayPtr pOverlay,
			      int index, int iscrt2);
static void set_line_buf_size(SISOverlayPtr pOverlay);
static void merge_line_buf(int enable);
static void set_format(SISOverlayPtr pOverlay);
static void set_colorkey(void);

static void set_brightness(uint8_t brightness);
static void set_contrast(uint8_t contrast);
static void set_saturation(char saturation);
static void set_hue(uint8_t hue);
#if 0
static void set_alpha(uint8_t alpha);
#endif

/* IO Port access functions */
static uint8_t getvideoreg(uint8_t reg)
{
    uint8_t ret;
    inSISIDXREG(SISVID, reg, ret);
    return (ret);
}

static void setvideoreg(uint8_t reg, uint8_t data)
{
    outSISIDXREG(SISVID, reg, data);
}

static void setvideoregmask(uint8_t reg, uint8_t data, uint8_t mask)
{
    uint8_t old;

    inSISIDXREG(SISVID, reg, old);
    data = (data & mask) | (old & (~mask));
    outSISIDXREG(SISVID, reg, data);
}

static void setsrregmask(uint8_t reg, uint8_t data, uint8_t mask)
{
    uint8_t old;

    inSISIDXREG(SISSR, reg, old);
    data = (data & mask) | (old & (~mask));
    outSISIDXREG(SISSR, reg, data);
}

/* vblank checking*/
static uint8_t vblank_active_CRT1(void)
{
    /* this may be too simplistic? */
    return (inSISREG(SISINPSTAT) & 0x08);
}

static uint8_t vblank_active_CRT2(void)
{
    uint8_t ret;
    if (sis_vga_engine == SIS_315_VGA) {
	inSISIDXREG(SISPART1, Index_310_CRT2_FC_VR, ret);
    } else {
	inSISIDXREG(SISPART1, Index_CRT2_FC_VR, ret);
    }
    return ((ret & 0x02) ^ 0x02);
}

static int find_chip(unsigned chip_id)
{
    unsigned i;
    for (i = 0; i < sizeof(sis_card_ids) / sizeof(unsigned short); i++) {
	if (chip_id == sis_card_ids[i])
	    return i;
    }
    return -1;
}

static int sis_probe(int verbose, int force)
{
    pciinfo_t lst[MAX_PCI_DEVICES];
    unsigned i, num_pci;
    int err;

    sis_verbose = verbose;
    force = force;
    err = pci_scan(lst, &num_pci);
    if (err) {
	printf("[SiS] Error occurred during pci scan: %s\n", strerror(err));
	return err;
    } else {
	err = ENXIO;
	for (i = 0; i < num_pci; i++) {
	    if (lst[i].vendor == VENDOR_SIS) {
		int idx;
		const char *dname;
		idx = find_chip(lst[i].device);
		if (idx == -1)
		    continue;
		dname = pci_device_name(VENDOR_SIS, lst[i].device);
		dname = dname ? dname : "Unknown chip";
		if (sis_verbose > 0)
		    printf("[SiS] Found chip: %s (0x%X)\n",
			   dname, lst[i].device);
		sis_device_id = sis_cap.device_id = lst[i].device;
		err = 0;
		memcpy(&pci_info, &lst[i], sizeof(pciinfo_t));

		sis_has_two_overlays = 0;
		switch (sis_cap.device_id) {
		case DEVICE_SIS_300:
		case DEVICE_SIS_630_VGA:
		    sis_has_two_overlays = 1;
		case DEVICE_SIS_540_VGA:
		    sis_vga_engine = SIS_300_VGA;
		    break;
		case DEVICE_SIS_330:
		case DEVICE_SIS_550_VGA:
		    sis_has_two_overlays = 1;
		case DEVICE_SIS_315H:
		case DEVICE_SIS_315:
		case DEVICE_SIS_315PRO:
		case DEVICE_SIS_650_VGA:
		    /* M650 & 651 have 2 overlays */
		    /* JCP: I think this works, but not really tested yet */
		    {
			unsigned char CR5F;
			unsigned char tempreg1, tempreg2;

			inSISIDXREG(SISCR, 0x5F, CR5F);
			CR5F &= 0xf0;
			andSISIDXREG(SISCR, 0x5c, 0x07);
			inSISIDXREG(SISCR, 0x5c, tempreg1);
			tempreg1 &= 0xf8;
			setSISIDXREG(SISCR, 0x5c, 0x07, 0xf8);
			inSISIDXREG(SISCR, 0x5c, tempreg2);
			tempreg2 &= 0xf8;
			if ((!tempreg1) || (tempreg2)) {
			    if (CR5F & 0x80) {
				sis_has_two_overlays = 1;
			    }
			} else {
			    sis_has_two_overlays = 1;	/* ? */
			}
			if (sis_has_two_overlays) {
			    if (sis_verbose > 0)
				printf
				    ("[SiS] detected M650/651 with 2 overlays\n");
			}
		    }
		    sis_vga_engine = SIS_315_VGA;
		    break;
		default:
		    /* should never get here */
		    sis_vga_engine = UNKNOWN_VGA;
		    break;
		}
	    }
	}
    }
    if (err && sis_verbose) {
	printf("[SiS] Can't find chip\n");
    } else {
	sis_probed = 1;
    }

    return err;
}

static int sis_init(void)
{
    uint8_t sr_data, cr_data, cr_data2;
    char *env_overlay_crt;

    if (!sis_probed) {
	printf("[SiS] driver was not probed but is being initialized\n");
	return (EINTR);
    }

    /* JCP: this is WRONG.  Need to coordinate w/ sisfb to use correct mem */
    /* map 16MB scary hack for now. */
    sis_mem_base = map_phys_mem(pci_info.base0, 0x1000000);
    /* sis_reg_base = map_phys_mem(pci_info.base1, 0x20000); */
    sis_iobase = pci_info.base2 & 0xFFFC;

    /* would like to use fb ioctl  - or some other method - here to get
       current resolution. */
    inSISIDXREG(SISCR, 0x12, cr_data);
    inSISIDXREG(SISCR, 0x07, cr_data2);
    sis_screen_height =
	((cr_data & 0xff) | ((uint16_t) (cr_data2 & 0x02) << 7) |
	 ((uint16_t) (cr_data2 & 0x40) << 3) | ((uint16_t) (cr_data & 0x02)
						<< 9)) + 1;

    inSISIDXREG(SISSR, 0x0b, sr_data);
    inSISIDXREG(SISCR, 0x01, cr_data);
    sis_screen_width = (((cr_data & 0xff) |
			 ((uint16_t) (sr_data & 0x0C) << 6)) + 1) * 8;

    inSISIDXREG(SISSR, Index_SR_Graphic_Mode, sr_data);
    if (sr_data & 0x20)		/* interlaced mode */
	sis_vmode |= VMODE_INTERLACED;

#if 0				/* getting back false data here... */
    /* CR9 bit 7 set = double scan active */
    inSISIDXREG(SISCR, 0x09, cr_data);
    if (cr_data & 0x40) {
	sis_vmode |= VMODE_DOUBLESCAN;
    }
#endif

    /* JCP: eventually I'd like to replace this with a call to sisfb
       SISFB_GET_INFO ioctl to get video bridge info.  Not for now,
       since it requires a very new and not widely distributed version. */
    sis_init_video_bridge();

    env_overlay_crt = getenv("VIDIX_CRT");
    if (env_overlay_crt) {
	int crt = atoi(env_overlay_crt);
	if (crt == 1 || crt == 2) {
	    sis_overlay_on_crt1 = (crt == 1);
	    if (sis_verbose > 0) {
		printf
		    ("[SiS] override: using overlay on CRT%d from VIDIX_CRT\n",
		     crt);
	    }
	}
    }

    return 0;
}

static void sis_destroy(void)
{
    /* unmap_phys_mem(sis_reg_base, 0x20000); */
    /* JCP: see above, hence also a hack. */
    unmap_phys_mem(sis_mem_base, 0x1000000);
}

static int sis_get_caps(vidix_capability_t * to)
{
    memcpy(to, &sis_cap, sizeof(vidix_capability_t));
    return 0;
}

static int is_supported_fourcc(uint32_t fourcc)
{
    switch (fourcc) {
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_UYVY:
    case IMGFMT_YUY2:
    case IMGFMT_RGB15:
    case IMGFMT_RGB16:
	return 1;
    default:
	return 0;
    }
}

static int sis_query_fourcc(vidix_fourcc_t * to)
{
    if (is_supported_fourcc(to->fourcc)) {
	to->depth = VID_DEPTH_8BPP | VID_DEPTH_16BPP | VID_DEPTH_32BPP;
	to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK | VID_CAP_COLORKEY;
	return 0;
    } else
	to->depth = to->flags = 0;
    return ENOSYS;
}

static int bridge_in_slave_mode(void)
{
    unsigned char usScratchP1_00;

    if (!(sis_vbflags & VB_VIDEOBRIDGE))
	return 0;

    inSISIDXREG(SISPART1, 0x00, usScratchP1_00);
    if (((sis_vga_engine == SIS_300_VGA)
	 && (usScratchP1_00 & 0xa0) == 0x20)
	|| ((sis_vga_engine == SIS_315_VGA)
	    && (usScratchP1_00 & 0x50) == 0x10)) {
	return 1;
    } else {
	return 0;
    }
}

/* This does not handle X dual head mode, since 1) vidix doesn't support it
   and 2) it doesn't make sense for other gfx drivers */
static void set_dispmode(void)
{
    sis_bridge_is_slave = 0;

    if (bridge_in_slave_mode())
	sis_bridge_is_slave = 1;

    if ((sis_vbflags & VB_DISPMODE_MIRROR) ||
	(sis_bridge_is_slave && (sis_vbflags & DISPTYPE_DISP2))) {
	if (sis_has_two_overlays)
	    sis_displaymode = DISPMODE_MIRROR;	/* TW: CRT1+CRT2 (2 overlays) */
	else if (!sis_overlay_on_crt1)
	    sis_displaymode = DISPMODE_SINGLE2;
	else
	    sis_displaymode = DISPMODE_SINGLE1;
    } else {
	if (sis_vbflags & DISPTYPE_DISP1) {
	    sis_displaymode = DISPMODE_SINGLE1;	/* TW: CRT1 only */
	} else {
	    sis_displaymode = DISPMODE_SINGLE2;	/* TW: CRT2 only */
	}
    }
}

static void set_disptype_regs(void)
{
    switch (sis_displaymode) {
    case DISPMODE_SINGLE1:	/* TW: CRT1 only */
	if (sis_verbose > 2) {
	    printf("[SiS] Setting up overlay on CRT1\n");
	}
	if (sis_has_two_overlays) {
	    setsrregmask(0x06, 0x00, 0xc0);
	    setsrregmask(0x32, 0x00, 0xc0);
	} else {
	    setsrregmask(0x06, 0x00, 0xc0);
	    setsrregmask(0x32, 0x00, 0xc0);
	}
	break;
    case DISPMODE_SINGLE2:	/* TW: CRT2 only */
	if (sis_verbose > 2) {
	    printf("[SiS] Setting up overlay on CRT2\n");
	}
	if (sis_has_two_overlays) {
	    setsrregmask(0x06, 0x80, 0xc0);
	    setsrregmask(0x32, 0x80, 0xc0);
	} else {
	    setsrregmask(0x06, 0x40, 0xc0);
	    setsrregmask(0x32, 0x40, 0xc0);
	}
	break;
    case DISPMODE_MIRROR:	/* TW: CRT1 + CRT2 */
    default:
	if (sis_verbose > 2) {
	    printf("[SiS] Setting up overlay on CRT1 AND CRT2!\n");
	}
	setsrregmask(0x06, 0x80, 0xc0);
	setsrregmask(0x32, 0x80, 0xc0);
	break;
    }
}

static void init_overlay(void)
{
    /* Initialize first overlay (CRT1) */

    /* Write-enable video registers */
    setvideoregmask(Index_VI_Control_Misc2, 0x80, 0x81);

    /* Disable overlay */
    setvideoregmask(Index_VI_Control_Misc0, 0x00, 0x02);

    /* Disable bobEnable */
    setvideoregmask(Index_VI_Control_Misc1, 0x02, 0x02);

    /* Reset scale control and contrast */
    setvideoregmask(Index_VI_Scale_Control, 0x60, 0x60);
    setvideoregmask(Index_VI_Contrast_Enh_Ctrl, 0x04, 0x1F);

    setvideoreg(Index_VI_Disp_Y_Buf_Preset_Low, 0x00);
    setvideoreg(Index_VI_Disp_Y_Buf_Preset_Middle, 0x00);
    setvideoreg(Index_VI_UV_Buf_Preset_Low, 0x00);
    setvideoreg(Index_VI_UV_Buf_Preset_Middle, 0x00);
    setvideoreg(Index_VI_Disp_Y_UV_Buf_Preset_High, 0x00);
    setvideoreg(Index_VI_Play_Threshold_Low, 0x00);
    setvideoreg(Index_VI_Play_Threshold_High, 0x00);

    /* may not want to init these here, could already be set to other
       values by app? */
    setvideoregmask(Index_VI_Control_Misc2, 0x00, 0x01);
    setvideoregmask(Index_VI_Contrast_Enh_Ctrl, 0x04, 0x07);
    setvideoreg(Index_VI_Brightness, 0x20);
    if (sis_vga_engine == SIS_315_VGA) {
	setvideoreg(Index_VI_Hue, 0x00);
	setvideoreg(Index_VI_Saturation, 0x00);
    }

    /* Initialize second overlay (CRT2) */
    if (sis_has_two_overlays) {
	/* Write-enable video registers */
	setvideoregmask(Index_VI_Control_Misc2, 0x81, 0x81);

	/* Disable overlay */
	setvideoregmask(Index_VI_Control_Misc0, 0x00, 0x02);

	/* Disable bobEnable */
	setvideoregmask(Index_VI_Control_Misc1, 0x02, 0x02);

	/* Reset scale control and contrast */
	setvideoregmask(Index_VI_Scale_Control, 0x60, 0x60);
	setvideoregmask(Index_VI_Contrast_Enh_Ctrl, 0x04, 0x1F);

	setvideoreg(Index_VI_Disp_Y_Buf_Preset_Low, 0x00);
	setvideoreg(Index_VI_Disp_Y_Buf_Preset_Middle, 0x00);
	setvideoreg(Index_VI_UV_Buf_Preset_Low, 0x00);
	setvideoreg(Index_VI_UV_Buf_Preset_Middle, 0x00);
	setvideoreg(Index_VI_Disp_Y_UV_Buf_Preset_High, 0x00);
	setvideoreg(Index_VI_Play_Threshold_Low, 0x00);
	setvideoreg(Index_VI_Play_Threshold_High, 0x00);

	setvideoregmask(Index_VI_Control_Misc2, 0x01, 0x01);
	setvideoregmask(Index_VI_Contrast_Enh_Ctrl, 0x04, 0x07);
	setvideoreg(Index_VI_Brightness, 0x20);
	if (sis_vga_engine == SIS_315_VGA) {
	    setvideoreg(Index_VI_Hue, 0x00);
	    setvideoreg(Index_VI_Saturation, 0x00);
	}
    }
}

static int sis_set_eq(const vidix_video_eq_t * eq);

static int sis_config_playback(vidix_playback_t * info)
{
    SISOverlayRec overlay;
    int srcOffsetX = 0, srcOffsetY = 0;
    int sx, sy;
    int index = 0, iscrt2 = 0;
    int total_size;

    short src_w, drw_w;
    short src_h, drw_h;
    short src_x, drw_x;
    short src_y, drw_y;
    long dga_offset;
    int pitch;
    unsigned int i;

    if (!is_supported_fourcc(info->fourcc))
	return -1;

    /* set chipset/engine.dependent config info */
    /*  which CRT to use, etc.? */
    switch (sis_vga_engine) {
    case SIS_315_VGA:
	sis_shift_value = 1;
	sis_equal.cap |= VEQ_CAP_SATURATION | VEQ_CAP_HUE;
	break;
    case SIS_300_VGA:
    default:
	sis_shift_value = 2;
	break;
    }

    sis_displaymode = DISPMODE_SINGLE1;	/* xV driver code in set_dispmode() */
    set_dispmode();

    set_disptype_regs();

    init_overlay();

    /* get basic dimension info */
    src_x = info->src.x;
    src_y = info->src.y;
    src_w = info->src.w;
    src_h = info->src.h;

    drw_x = info->dest.x;
    drw_y = info->dest.y;
    drw_w = info->dest.w;
    drw_h = info->dest.h;

    switch (info->fourcc) {
    case IMGFMT_YV12:
    case IMGFMT_I420:
	pitch = (src_w + 7) & ~7;
	total_size = (pitch * src_h * 3) >> 1;
	break;
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
    case IMGFMT_RGB15:
    case IMGFMT_RGB16:
	pitch = ((src_w << 1) + 3) & ~3;
	total_size = pitch * src_h;
	break;
    default:
	return -1;
    }

    /* "allocate" memory for overlay! */
    /* start at 8MB = sisfb's "dri reserved space" -
       really shouldn't hardcode though */
    /* XXX: JCP - this can use the sisfb FBIO_ALLOC ioctl to safely
       allocate "video heap" memory... */
    dga_offset = 0x800000;

    /* use 7MB for now.  need to calc/get real info from sisfb? */
    /* this can result in a LOT of frames - probably not necessary */
    info->num_frames = 0x700000 / (total_size * 2);
    if (info->num_frames > VID_PLAY_MAXFRAMES)
	info->num_frames = VID_PLAY_MAXFRAMES;

    info->dga_addr = sis_mem_base + dga_offset;
    info->dest.pitch.y = 16;
    info->dest.pitch.u = 16;
    info->dest.pitch.v = 16;
    info->offset.y = 0;
    info->offset.u = 0;
    info->offset.v = 0;
    info->frame_size = (total_size * 2);	/* why times 2 ? */
    for (i = 0; i < info->num_frames; i++) {
	info->offsets[i] = info->frame_size * i;
	/* save ptrs to mem buffers */
	sis_frames[i] = (dga_offset + info->offsets[i]);
    }

    memset(&overlay, 0, sizeof(overlay));
    overlay.pixelFormat = sis_format = info->fourcc;
    overlay.pitch = overlay.origPitch = pitch;


    overlay.keyOP = (sis_grkey.ckey.op == CKEY_TRUE ?
		     VI_ROP_DestKey : VI_ROP_Always);

    overlay.bobEnable = 0x00;

    overlay.SCREENheight = sis_screen_height;

    /* probably will not support X virtual screen > phys very well? */
    overlay.dstBox.x1 = drw_x;	/* - pScrn->frameX0; */
    overlay.dstBox.x2 = drw_x + drw_w;	/* - pScrn->frameX0; ??? */
    overlay.dstBox.y1 = drw_y;	/*  - pScrn->frameY0; */
    overlay.dstBox.y2 = drw_y + drw_h;	/* - pScrn->frameY0; ??? */

    if ((overlay.dstBox.x1 > overlay.dstBox.x2) ||
	(overlay.dstBox.y1 > overlay.dstBox.y2))
	return -1;

    if ((overlay.dstBox.x2 < 0) || (overlay.dstBox.y2 < 0))
	return -1;

    if (overlay.dstBox.x1 < 0) {
	srcOffsetX = src_w * (-overlay.dstBox.x1) / drw_w;
	overlay.dstBox.x1 = 0;
    }
    if (overlay.dstBox.y1 < 0) {
	srcOffsetY = src_h * (-overlay.dstBox.y1) / drw_h;
	overlay.dstBox.y1 = 0;
    }

    switch (info->fourcc) {
    case IMGFMT_YV12:
	info->dest.pitch.y = 16;
	sx = (src_x + srcOffsetX) & ~7;
	sy = (src_y + srcOffsetY) & ~1;
	info->offset.y = sis_Yoff = sx + sy * pitch;
	/* JCP: NOTE reversed u & v here!  Not sure why this is needed.
	   maybe mplayer & sis define U & V differently?? */
	info->offset.u = sis_Voff =
	    src_h * pitch + ((sx + sy * pitch / 2) >> 1);
	info->offset.v = sis_Uoff =
	    src_h * pitch * 5 / 4 + ((sx + sy * pitch / 2) >> 1);

	overlay.PSY = (sis_frames[0] + sis_Yoff) >> sis_shift_value;
	overlay.PSV = (sis_frames[0] + sis_Voff) >> sis_shift_value;
	overlay.PSU = (sis_frames[0] + sis_Uoff) >> sis_shift_value;
	break;
    case IMGFMT_I420:
	sx = (src_x + srcOffsetX) & ~7;
	sy = (src_y + srcOffsetY) & ~1;
	info->offset.y = sis_Yoff = sx + sy * pitch;
	/* JCP: see above... */
	info->offset.u = sis_Voff =
	    src_h * pitch * 5 / 4 + ((sx + sy * pitch / 2) >> 1);
	info->offset.v = sis_Uoff =
	    src_h * pitch + ((sx + sy * pitch / 2) >> 1);

	overlay.PSY = (sis_frames[0] + sis_Yoff) >> sis_shift_value;
	overlay.PSV = (sis_frames[0] + sis_Voff) >> sis_shift_value;
	overlay.PSU = (sis_frames[0] + sis_Uoff) >> sis_shift_value;
	break;
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
    case IMGFMT_RGB16:
    case IMGFMT_RGB15:
    default:
	sx = (src_x + srcOffsetX) & ~1;
	sy = (src_y + srcOffsetY);
	info->offset.y = sis_Yoff = sx * 2 + sy * pitch;

	overlay.PSY = (sis_frames[0] + sis_Yoff) >> sis_shift_value;
	break;
    }

    /* FIXME: is it possible that srcW < 0? */
    overlay.srcW = src_w - (sx - src_x);
    overlay.srcH = src_h - (sy - src_y);

    /* JCP: what to do about this? */
#if 0
    if ((pPriv->oldx1 != overlay.dstBox.x1) ||
	(pPriv->oldx2 != overlay.dstBox.x2) ||
	(pPriv->oldy1 != overlay.dstBox.y1) ||
	(pPriv->oldy2 != overlay.dstBox.y2)) {
	pPriv->mustwait = 1;
	pPriv->oldx1 = overlay.dstBox.x1;
	pPriv->oldx2 = overlay.dstBox.x2;
	pPriv->oldy1 = overlay.dstBox.y1;
	pPriv->oldy2 = overlay.dstBox.y2;
    }
#endif

    /* set merge line buffer */
    merge_line_buf(overlay.srcW > 384);

    /* calculate line buffer length */
    set_line_buf_size(&overlay);

    if (sis_displaymode == DISPMODE_SINGLE2) {
	if (sis_has_two_overlays) {
	    /* TW: On chips with two overlays we use
	     * overlay 2 for CRT2 */
	    index = 1;
	    iscrt2 = 1;
	} else {
	    /* TW: On chips with only one overlay we
	     * use that only overlay for CRT2 */
	    index = 0;
	    iscrt2 = 1;
	}
	overlay.VBlankActiveFunc = vblank_active_CRT2;
	/* overlay.GetScanLineFunc = get_scanline_CRT2; */
    } else {
	index = 0;
	iscrt2 = 0;
	overlay.VBlankActiveFunc = vblank_active_CRT1;
	/* overlay.GetScanLineFunc = get_scanline_CRT1; */
    }

    /* calc scale factor (to use below) */
    calc_scale_factor(&overlay, index, iscrt2);

    /* Select video1 (used for CRT1) or video2 (used for CRT2) */
    setvideoregmask(Index_VI_Control_Misc2, index, 0x01);

    set_format(&overlay);

    set_colorkey();

    sis_set_eq(&sis_equal);

    /* set up video overlay registers */
    set_overlay(&overlay, index);

    /* prevent badness if bits are not at default setting */
    setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x01);
    setvideoregmask(Index_VI_Control_Misc2, 0x00, 0x04);

    /* JCP:  Xv driver implementation loops back over above code to
       setup mirror CRT2 */

    return 0;
}

static int sis_playback_on(void)
{
    setvideoregmask(Index_VI_Control_Misc0, 0x02, 0x02);
    return 0;
}

static int sis_playback_off(void)
{
    unsigned char sridx, cridx;
    sridx = inSISREG(SISSR);
    cridx = inSISREG(SISCR);
    close_overlay();
    outSISREG(SISSR, sridx);
    outSISREG(SISCR, cridx);

    return 0;
}

static int sis_frame_select(unsigned int frame)
{
    uint8_t data;
    int index = 0;
    uint32_t PSY;

    if (sis_displaymode == DISPMODE_SINGLE2 && sis_has_two_overlays) {
	index = 1;
    }

    PSY = (sis_frames[frame] + sis_Yoff) >> sis_shift_value;

    /* Unlock address registers */
    data = getvideoreg(Index_VI_Control_Misc1);
    setvideoreg(Index_VI_Control_Misc1, data | 0x20);
    /* TEST: Is this required? */
    setvideoreg(Index_VI_Control_Misc1, data | 0x20);
    /* TEST end */
    /* TEST: Is this required? */
    if (sis_vga_engine == SIS_315_VGA)
	setvideoreg(Index_VI_Control_Misc3, 0x00);
    /* TEST end */

    /* set Y start address */
    setvideoreg(Index_VI_Disp_Y_Buf_Start_Low, (uint8_t) (PSY));
    setvideoreg(Index_VI_Disp_Y_Buf_Start_Middle, (uint8_t) ((PSY) >> 8));
    setvideoreg(Index_VI_Disp_Y_Buf_Start_High, (uint8_t) ((PSY) >> 16));
    /* set 310/325 series overflow bits for Y plane */
    if (sis_vga_engine == SIS_315_VGA) {
	setvideoreg(Index_VI_Y_Buf_Start_Over,
		    ((uint8_t) ((PSY) >> 24) & 0x01));
    }

    /* Set U/V data if using plane formats */
    if ((sis_format == IMGFMT_YV12) || (sis_format == IMGFMT_I420)) {

	uint32_t PSU, PSV;

	PSU = (sis_frames[frame] + sis_Uoff) >> sis_shift_value;
	PSV = (sis_frames[frame] + sis_Voff) >> sis_shift_value;

	/* set U/V start address */
	setvideoreg(Index_VI_U_Buf_Start_Low, (uint8_t) PSU);
	setvideoreg(Index_VI_U_Buf_Start_Middle, (uint8_t) (PSU >> 8));
	setvideoreg(Index_VI_U_Buf_Start_High, (uint8_t) (PSU >> 16));

	setvideoreg(Index_VI_V_Buf_Start_Low, (uint8_t) PSV);
	setvideoreg(Index_VI_V_Buf_Start_Middle, (uint8_t) (PSV >> 8));
	setvideoreg(Index_VI_V_Buf_Start_High, (uint8_t) (PSV >> 16));

	/* 310/325 series overflow bits */
	if (sis_vga_engine == SIS_315_VGA) {
	    setvideoreg(Index_VI_U_Buf_Start_Over,
			((uint8_t) (PSU >> 24) & 0x01));
	    setvideoreg(Index_VI_V_Buf_Start_Over,
			((uint8_t) (PSV >> 24) & 0x01));
	}
    }

    if (sis_vga_engine == SIS_315_VGA) {
	/* Trigger register copy for 310 series */
	setvideoreg(Index_VI_Control_Misc3, 1 << index);
    }

    /* Lock the address registers */
    setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x20);

    return 0;
}

static int sis_get_gkeys(vidix_grkey_t * grkey)
{
    memcpy(grkey, &sis_grkey, sizeof(vidix_grkey_t));
    return 0;
}

static int sis_set_gkeys(const vidix_grkey_t * grkey)
{
    memcpy(&sis_grkey, grkey, sizeof(vidix_grkey_t));
    set_colorkey();
    return 0;
}

static int sis_get_eq(vidix_video_eq_t * eq)
{
    memcpy(eq, &sis_equal, sizeof(vidix_video_eq_t));
    return 0;
}

static int sis_set_eq(const vidix_video_eq_t * eq)
{
    int br, sat, cr, hue;
    if (eq->cap & VEQ_CAP_BRIGHTNESS)
	sis_equal.brightness = eq->brightness;
    if (eq->cap & VEQ_CAP_CONTRAST)
	sis_equal.contrast = eq->contrast;
    if (eq->cap & VEQ_CAP_SATURATION)
	sis_equal.saturation = eq->saturation;
    if (eq->cap & VEQ_CAP_HUE)
	sis_equal.hue = eq->hue;
    if (eq->cap & VEQ_CAP_RGB_INTENSITY) {
	sis_equal.red_intensity = eq->red_intensity;
	sis_equal.green_intensity = eq->green_intensity;
	sis_equal.blue_intensity = eq->blue_intensity;
    }
    sis_equal.flags = eq->flags;

    cr = (sis_equal.contrast + 1000) * 7 / 2000;
    if (cr < 0)
	cr = 0;
    if (cr > 7)
	cr = 7;

    br = sis_equal.brightness * 127 / 1000;
    if (br < -128)
	br = -128;
    if (br > 127)
	br = 127;

    sat = (sis_equal.saturation * 7) / 1000;
    if (sat < -7)
	sat = -7;
    if (sat > 7)
	sat = 7;

    hue = sis_equal.hue * 7 / 1000;
    if (hue < -8)
	hue = -8;
    if (hue > 7)
	hue = 7;

    set_brightness(br);
    set_contrast(cr);
    if (sis_vga_engine == SIS_315_VGA) {
	set_saturation(sat);
	set_hue(hue);
    }

    return 0;
}

static void set_overlay(SISOverlayPtr pOverlay, int index)
{
    uint16_t pitch = 0;
    uint8_t h_over = 0, v_over = 0;
    uint16_t top, bottom, left, right;
    uint16_t screenX = sis_screen_width;
    uint16_t screenY = sis_screen_height;
    uint8_t data;
    uint32_t watchdog;

    top = pOverlay->dstBox.y1;
    bottom = pOverlay->dstBox.y2;
    if (bottom > screenY) {
	bottom = screenY;
    }

    left = pOverlay->dstBox.x1;
    right = pOverlay->dstBox.x2;
    if (right > screenX) {
	right = screenX;
    }

    /* JCP: these aren't really tested... */
    /* TW: DoubleScan modes require Y coordinates * 2 */
    if (sis_vmode & VMODE_DOUBLESCAN) {
	top <<= 1;
	bottom <<= 1;
    }
    /* TW: Interlace modes require Y coordinates / 2 */
    if (sis_vmode & VMODE_INTERLACED) {
	top >>= 1;
	bottom >>= 1;
    }

    h_over = (((left >> 8) & 0x0f) | ((right >> 4) & 0xf0));
    v_over = (((top >> 8) & 0x0f) | ((bottom >> 4) & 0xf0));

    pitch = pOverlay->pitch >> sis_shift_value;

    /* set line buffer size */
    setvideoreg(Index_VI_Line_Buffer_Size, pOverlay->lineBufSize);

    /* set color key mode */
    setvideoregmask(Index_VI_Key_Overlay_OP, pOverlay->keyOP, 0x0F);

    /* TW: We don't have to wait for vertical retrace in all cases */
    /* JCP: be safe for now. */
    if (1 /*pPriv->mustwait */ ) {
	watchdog = WATCHDOG_DELAY;
	while (pOverlay->VBlankActiveFunc() && --watchdog);
	watchdog = WATCHDOG_DELAY;
	while ((!pOverlay->VBlankActiveFunc()) && --watchdog);
	if (!watchdog && sis_verbose > 0) {
	    printf("[SiS]: timed out waiting for vertical retrace\n");
	}
    }

    /* Unlock address registers */
    data = getvideoreg(Index_VI_Control_Misc1);
    setvideoreg(Index_VI_Control_Misc1, data | 0x20);
    /* TEST: Is this required? */
    setvideoreg(Index_VI_Control_Misc1, data | 0x20);
    /* TEST end */

    /* TEST: Is this required? */
    if (sis_vga_engine == SIS_315_VGA)
	setvideoreg(Index_VI_Control_Misc3, 0x00);
    /* TEST end */

    /* Set Y buf pitch */
    setvideoreg(Index_VI_Disp_Y_Buf_Pitch_Low, (uint8_t) (pitch));
    setvideoregmask(Index_VI_Disp_Y_UV_Buf_Pitch_Middle,
		    (uint8_t) (pitch >> 8), 0x0f);

    /* Set Y start address */
    setvideoreg(Index_VI_Disp_Y_Buf_Start_Low, (uint8_t) (pOverlay->PSY));
    setvideoreg(Index_VI_Disp_Y_Buf_Start_Middle,
		(uint8_t) ((pOverlay->PSY) >> 8));
    setvideoreg(Index_VI_Disp_Y_Buf_Start_High,
		(uint8_t) ((pOverlay->PSY) >> 16));

    /* set 310/325 series overflow bits for Y plane */
    if (sis_vga_engine == SIS_315_VGA) {
	setvideoreg(Index_VI_Disp_Y_Buf_Pitch_High,
		    (uint8_t) (pitch >> 12));
	setvideoreg(Index_VI_Y_Buf_Start_Over,
		    ((uint8_t) ((pOverlay->PSY) >> 24) & 0x01));
    }

    /* Set U/V data if using plane formats */
    if ((pOverlay->pixelFormat == IMGFMT_YV12) ||
	(pOverlay->pixelFormat == IMGFMT_I420)) {

	uint32_t PSU, PSV;

	PSU = pOverlay->PSU;
	PSV = pOverlay->PSV;

	/* Set U/V pitch */
	setvideoreg(Index_VI_Disp_UV_Buf_Pitch_Low,
		    (uint8_t) (pitch >> 1));
	setvideoregmask(Index_VI_Disp_Y_UV_Buf_Pitch_Middle,
			(uint8_t) (pitch >> 5), 0xf0);

	/* set U/V start address */
	setvideoreg(Index_VI_U_Buf_Start_Low, (uint8_t) PSU);
	setvideoreg(Index_VI_U_Buf_Start_Middle, (uint8_t) (PSU >> 8));
	setvideoreg(Index_VI_U_Buf_Start_High, (uint8_t) (PSU >> 16));

	setvideoreg(Index_VI_V_Buf_Start_Low, (uint8_t) PSV);
	setvideoreg(Index_VI_V_Buf_Start_Middle, (uint8_t) (PSV >> 8));
	setvideoreg(Index_VI_V_Buf_Start_High, (uint8_t) (PSV >> 16));

	/* 310/325 series overflow bits */
	if (sis_vga_engine == SIS_315_VGA) {
	    setvideoreg(Index_VI_Disp_UV_Buf_Pitch_High,
			(uint8_t) (pitch >> 13));
	    setvideoreg(Index_VI_U_Buf_Start_Over,
			((uint8_t) (PSU >> 24) & 0x01));
	    setvideoreg(Index_VI_V_Buf_Start_Over,
			((uint8_t) (PSV >> 24) & 0x01));
	}
    }

    if (sis_vga_engine == SIS_315_VGA) {
	/* Trigger register copy for 310 series */
	setvideoreg(Index_VI_Control_Misc3, 1 << index);
    }

    /* set scale factor */
    setvideoreg(Index_VI_Hor_Post_Up_Scale_Low,
		(uint8_t) (pOverlay->HUSF));
    setvideoreg(Index_VI_Hor_Post_Up_Scale_High,
		(uint8_t) ((pOverlay->HUSF) >> 8));
    setvideoreg(Index_VI_Ver_Up_Scale_Low, (uint8_t) (pOverlay->VUSF));
    setvideoreg(Index_VI_Ver_Up_Scale_High,
		(uint8_t) ((pOverlay->VUSF) >> 8));

    setvideoregmask(Index_VI_Scale_Control, (pOverlay->IntBit << 3)
		    | (pOverlay->wHPre), 0x7f);

    /* set destination window position */
    setvideoreg(Index_VI_Win_Hor_Disp_Start_Low, (uint8_t) left);
    setvideoreg(Index_VI_Win_Hor_Disp_End_Low, (uint8_t) right);
    setvideoreg(Index_VI_Win_Hor_Over, (uint8_t) h_over);

    setvideoreg(Index_VI_Win_Ver_Disp_Start_Low, (uint8_t) top);
    setvideoreg(Index_VI_Win_Ver_Disp_End_Low, (uint8_t) bottom);
    setvideoreg(Index_VI_Win_Ver_Over, (uint8_t) v_over);

    setvideoregmask(Index_VI_Control_Misc1, pOverlay->bobEnable, 0x1a);

    /* Lock the address registers */
    setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x20);
}


/* TW: Overlay MUST NOT be switched off while beam is over it */
static void close_overlay(void)
{
    uint32_t watchdog;

    if ((sis_displaymode == DISPMODE_SINGLE2) ||
	(sis_displaymode == DISPMODE_MIRROR)) {
	if (sis_has_two_overlays) {
	    setvideoregmask(Index_VI_Control_Misc2, 0x01, 0x01);
	    watchdog = WATCHDOG_DELAY;
	    while (vblank_active_CRT2() && --watchdog);
	    watchdog = WATCHDOG_DELAY;
	    while ((!vblank_active_CRT2()) && --watchdog);
	    setvideoregmask(Index_VI_Control_Misc0, 0x00, 0x02);
	    watchdog = WATCHDOG_DELAY;
	    while (vblank_active_CRT2() && --watchdog);
	    watchdog = WATCHDOG_DELAY;
	    while ((!vblank_active_CRT2()) && --watchdog);
	} else if (sis_displaymode == DISPMODE_SINGLE2) {
	    setvideoregmask(Index_VI_Control_Misc2, 0x00, 0x01);
	    watchdog = WATCHDOG_DELAY;
	    while (vblank_active_CRT1() && --watchdog);
	    watchdog = WATCHDOG_DELAY;
	    while ((!vblank_active_CRT1()) && --watchdog);
	    setvideoregmask(Index_VI_Control_Misc0, 0x00, 0x02);
	    watchdog = WATCHDOG_DELAY;
	    while (vblank_active_CRT1() && --watchdog);
	    watchdog = WATCHDOG_DELAY;
	    while ((!vblank_active_CRT1()) && --watchdog);
	}
    }
    if ((sis_displaymode == DISPMODE_SINGLE1) ||
	(sis_displaymode == DISPMODE_MIRROR)) {
	setvideoregmask(Index_VI_Control_Misc2, 0x00, 0x01);
	watchdog = WATCHDOG_DELAY;
	while (vblank_active_CRT1() && --watchdog);
	watchdog = WATCHDOG_DELAY;
	while ((!vblank_active_CRT1()) && --watchdog);
	setvideoregmask(Index_VI_Control_Misc0, 0x00, 0x02);
	watchdog = WATCHDOG_DELAY;
	while (vblank_active_CRT1() && --watchdog);
	watchdog = WATCHDOG_DELAY;
	while ((!vblank_active_CRT1()) && --watchdog);
    }
}


static void
calc_scale_factor(SISOverlayPtr pOverlay, int index, int iscrt2)
{
    uint32_t i = 0, mult = 0;
    int flag = 0;

    int dstW = pOverlay->dstBox.x2 - pOverlay->dstBox.x1;
    int dstH = pOverlay->dstBox.y2 - pOverlay->dstBox.y1;
    int srcW = pOverlay->srcW;
    int srcH = pOverlay->srcH;
    /*    uint16_t LCDheight = pSiS->LCDheight; */
    int srcPitch = pOverlay->origPitch;
    int origdstH = dstH;

    /* get rid of warnings for now */
    index = index;
    iscrt2 = iscrt2;

#if 0				/* JCP: don't bother with this for now. */
    /* TW: Stretch image due to idiotic LCD "auto"-scaling on LVDS (and 630+301B) */
    if (pSiS->VBFlags & CRT2_LCD) {
	if (sis_bridge_is_slave) {
	    if (pSiS->VBFlags & VB_LVDS) {
		dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
	    } else if ((sis_vga_engine == SIS_300_VGA) &&
		       (pSiS->
			VBFlags & (VB_301B | VB_302B | VB_301LV |
				   VB_302LV))) {
		dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
	    }
	} else if (iscrt2) {
	    if (pSiS->VBFlags & VB_LVDS) {
		dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
		if (sis_displaymode == DISPMODE_MIRROR)
		    flag = 1;
	    } else if ((sis_vga_engine == SIS_300_VGA) &&
		       (pSiS->
			VBFlags & (VB_301B | VB_302B | VB_301LV |
				   VB_302LV))) {
		dstH = (dstH * LCDheight) / pOverlay->SCREENheight;
		if (sis_displaymode == DISPMODE_MIRROR)
		    flag = 1;
	    }
	}
    }
#endif

    /* TW: For double scan modes, we need to double the height
     *     (Perhaps we also need to scale LVDS, but I'm not sure.)
     *     On 310/325 series, we need to double the width as well.
     *     Interlace mode vice versa.
     */
    if (sis_vmode & VMODE_DOUBLESCAN) {
	dstH = origdstH << 1;
	flag = 0;
	if (sis_vga_engine == SIS_315_VGA) {
	    dstW <<= 1;
	}
    }
    if (sis_vmode & VMODE_INTERLACED) {
	dstH = origdstH >> 1;
	flag = 0;
    }

    if (dstW < OVERLAY_MIN_WIDTH)
	dstW = OVERLAY_MIN_WIDTH;
    if (dstW == srcW) {
	pOverlay->HUSF = 0x00;
	pOverlay->IntBit = 0x05;
	pOverlay->wHPre = 0;
    } else if (dstW > srcW) {
	dstW += 2;
	pOverlay->HUSF = (srcW << 16) / dstW;
	pOverlay->IntBit = 0x04;
	pOverlay->wHPre = 0;
    } else {
	int tmpW = dstW;

	/* TW: It seems, the hardware can't scale below factor .125 (=1/8) if the
	   pitch isn't a multiple of 256.
	   TODO: Test this on the 310/325 series!
	 */
	if ((srcPitch % 256) || (srcPitch < 256)) {
	    if (((dstW * 1000) / srcW) < 125)
		dstW = tmpW = ((srcW * 125) / 1000) + 1;
	}

	i = 0;
	pOverlay->IntBit = 0x01;
	while (srcW >= tmpW) {
	    tmpW <<= 1;
	    i++;
	}
	pOverlay->wHPre = (uint8_t) (i - 1);
	dstW <<= (i - 1);
	if ((srcW % dstW))
	    pOverlay->HUSF = ((srcW - dstW) << 16) / dstW;
	else
	    pOverlay->HUSF = 0x00;
    }

    if (dstH < OVERLAY_MIN_HEIGHT)
	dstH = OVERLAY_MIN_HEIGHT;
    if (dstH == srcH) {
	pOverlay->VUSF = 0x00;
	pOverlay->IntBit |= 0x0A;
    } else if (dstH > srcH) {
	dstH += 0x02;
	pOverlay->VUSF = (srcH << 16) / dstH;
	pOverlay->IntBit |= 0x08;
    } else {
	uint32_t realI;

	i = realI = srcH / dstH;
	pOverlay->IntBit |= 0x02;

	if (i < 2) {
	    pOverlay->VUSF = ((srcH - dstH) << 16) / dstH;
	    /* TW: Needed for LCD-scaling modes */
	    if ((flag) && (mult = (srcH / origdstH)) >= 2)
		pOverlay->pitch /= mult;
	} else {
#if 0
	    if (((pOverlay->bobEnable & 0x08) == 0x00) &&
		(((srcPitch * i) >> 2) > 0xFFF)) {
		pOverlay->bobEnable |= 0x08;
		srcPitch >>= 1;
	    }
#endif
	    if (((srcPitch * i) >> 2) > 0xFFF) {
		i = (0xFFF * 2 / srcPitch);
		pOverlay->VUSF = 0xFFFF;
	    } else {
		dstH = i * dstH;
		if (srcH % dstH)
		    pOverlay->VUSF = ((srcH - dstH) << 16) / dstH;
		else
		    pOverlay->VUSF = 0x00;
	    }
	    /* set video frame buffer offset */
	    pOverlay->pitch = (uint16_t) (srcPitch * i);
	}
    }
}

static void set_line_buf_size(SISOverlayPtr pOverlay)
{
    uint8_t preHIDF;
    uint32_t i;
    uint32_t line = pOverlay->srcW;

    if ((pOverlay->pixelFormat == IMGFMT_YV12) ||
	(pOverlay->pixelFormat == IMGFMT_I420)) {
	preHIDF = pOverlay->wHPre & 0x07;
	switch (preHIDF) {
	case 3:
	    if ((line & 0xffffff00) == line)
		i = (line >> 8);
	    else
		i = (line >> 8) + 1;
	    pOverlay->lineBufSize = (uint8_t) (i * 32 - 1);
	    break;
	case 4:
	    if ((line & 0xfffffe00) == line)
		i = (line >> 9);
	    else
		i = (line >> 9) + 1;
	    pOverlay->lineBufSize = (uint8_t) (i * 64 - 1);
	    break;
	case 5:
	    if ((line & 0xfffffc00) == line)
		i = (line >> 10);
	    else
		i = (line >> 10) + 1;
	    pOverlay->lineBufSize = (uint8_t) (i * 128 - 1);
	    break;
	case 6:
	    if ((line & 0xfffff800) == line)
		i = (line >> 11);
	    else
		i = (line >> 11) + 1;
	    pOverlay->lineBufSize = (uint8_t) (i * 256 - 1);
	    break;
	default:
	    if ((line & 0xffffff80) == line)
		i = (line >> 7);
	    else
		i = (line >> 7) + 1;
	    pOverlay->lineBufSize = (uint8_t) (i * 16 - 1);
	    break;
	}
    } else {			/* YUV2, UYVY */
	if ((line & 0xffffff8) == line)
	    i = (line >> 3);
	else
	    i = (line >> 3) + 1;
	pOverlay->lineBufSize = (uint8_t) (i - 1);
    }
}

static void merge_line_buf(int enable)
{
    if (enable) {
	switch (sis_displaymode) {
	case DISPMODE_SINGLE1:
	    if (sis_has_two_overlays) {
		/* dual line merge */
		setvideoregmask(Index_VI_Control_Misc2, 0x10, 0x11);
		setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x04);
	    } else {
		setvideoregmask(Index_VI_Control_Misc2, 0x10, 0x11);
		setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x04);
	    }
	    break;
	case DISPMODE_SINGLE2:
	    if (sis_has_two_overlays) {
		/* line merge */
		setvideoregmask(Index_VI_Control_Misc2, 0x01, 0x11);
		setvideoregmask(Index_VI_Control_Misc1, 0x04, 0x04);
	    } else {
		setvideoregmask(Index_VI_Control_Misc2, 0x10, 0x11);
		setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x04);
	    }
	    break;
	case DISPMODE_MIRROR:
	default:
	    /* line merge */
	    setvideoregmask(Index_VI_Control_Misc2, 0x00, 0x11);
	    setvideoregmask(Index_VI_Control_Misc1, 0x04, 0x04);
	    if (sis_has_two_overlays) {
		/* line merge */
		setvideoregmask(Index_VI_Control_Misc2, 0x01, 0x11);
		setvideoregmask(Index_VI_Control_Misc1, 0x04, 0x04);
	    }
	    break;
	}
    } else {
	switch (sis_displaymode) {
	case DISPMODE_SINGLE1:
	    setvideoregmask(Index_VI_Control_Misc2, 0x00, 0x11);
	    setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x04);
	    break;
	case DISPMODE_SINGLE2:
	    if (sis_has_two_overlays) {
		setvideoregmask(Index_VI_Control_Misc2, 0x01, 0x11);
		setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x04);
	    } else {
		setvideoregmask(Index_VI_Control_Misc2, 0x00, 0x11);
		setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x04);
	    }
	    break;
	case DISPMODE_MIRROR:
	default:
	    setvideoregmask(Index_VI_Control_Misc2, 0x00, 0x11);
	    setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x04);
	    if (sis_has_two_overlays) {
		setvideoregmask(Index_VI_Control_Misc2, 0x01, 0x11);
		setvideoregmask(Index_VI_Control_Misc1, 0x00, 0x04);
	    }
	    break;
	}
    }
}


static void set_format(SISOverlayPtr pOverlay)
{
    uint8_t fmt;

    switch (pOverlay->pixelFormat) {
    case IMGFMT_YV12:
    case IMGFMT_I420:
	fmt = 0x0c;
	break;
    case IMGFMT_YUY2:
	fmt = 0x28;
	break;
    case IMGFMT_UYVY:
	fmt = 0x08;
	break;
    case IMGFMT_RGB15:		/* D[5:4] : 00 RGB555, 01 RGB 565 */
	fmt = 0x00;
	break;
    case IMGFMT_RGB16:
	fmt = 0x10;
	break;
    default:
	fmt = 0x00;
	break;
    }
    setvideoregmask(Index_VI_Control_Misc0, fmt, 0x7c);
}

static void set_colorkey(void)
{
    uint8_t r, g, b;

    b = (uint8_t) sis_grkey.ckey.blue;
    g = (uint8_t) sis_grkey.ckey.green;
    r = (uint8_t) sis_grkey.ckey.red;

    /* set color key mode */
    setvideoregmask(Index_VI_Key_Overlay_OP,
		    sis_grkey.ckey.op == CKEY_TRUE ?
		    VI_ROP_DestKey : VI_ROP_Always, 0x0F);

    /* set colorkey values */
    setvideoreg(Index_VI_Overlay_ColorKey_Blue_Min, (uint8_t) b);
    setvideoreg(Index_VI_Overlay_ColorKey_Green_Min, (uint8_t) g);
    setvideoreg(Index_VI_Overlay_ColorKey_Red_Min, (uint8_t) r);

    setvideoreg(Index_VI_Overlay_ColorKey_Blue_Max, (uint8_t) b);
    setvideoreg(Index_VI_Overlay_ColorKey_Green_Max, (uint8_t) g);
    setvideoreg(Index_VI_Overlay_ColorKey_Red_Max, (uint8_t) r);
}

static void set_brightness(uint8_t brightness)
{
    setvideoreg(Index_VI_Brightness, brightness);
}

static void set_contrast(uint8_t contrast)
{
    setvideoregmask(Index_VI_Contrast_Enh_Ctrl, contrast, 0x07);
}

/* Next 3 functions are 310/325 series only */

static void set_saturation(char saturation)
{
    uint8_t temp = 0;

    if (saturation < 0) {
	temp |= 0x88;
	saturation = -saturation;
    }
    temp |= (saturation & 0x07);
    temp |= ((saturation & 0x07) << 4);

    setvideoreg(Index_VI_Saturation, temp);
}

static void set_hue(uint8_t hue)
{
    setvideoreg(Index_VI_Hue, (hue & 0x08) ? (hue ^ 0x07) : hue);
}

#if 0
/* JCP: not used (I don't think it's correct anyway) */
static void set_alpha(uint8_t alpha)
{
    uint8_t data;

    data = getvideoreg(Index_VI_Key_Overlay_OP);
    data &= 0x0F;
    setvideoreg(Index_VI_Key_Overlay_OP, data | (alpha << 4));
}
#endif

VDXDriver sis_drv = {
  "sis",
  NULL,
    
  .probe = sis_probe,
  .get_caps = sis_get_caps,
  .query_fourcc = sis_query_fourcc,
  .init = sis_init,
  .destroy = sis_destroy,
  .config_playback = sis_config_playback,
  .playback_on = sis_playback_on,
  .playback_off = sis_playback_off,
  .frame_sel = sis_frame_select,
  .get_eq = sis_get_eq,
  .set_eq = sis_set_eq,
  .get_gkey = sis_get_gkeys,
  .set_gkey = sis_set_gkeys,
};
