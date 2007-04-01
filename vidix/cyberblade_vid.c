/*
    Driver for CyberBlade/i1 - Version 0.1.4

    Copyright (C) 2002 by Alastair M. Robinson.
    Official homepage: http://www.blackfiveservices.co.uk/EPIAVidix.shtml

    Based on Permedia 3 driver by Måns Rullgård

    Thanks to Gilles Frattini for bugfixes

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

    Changes:
    18/01/03
      MMIO is no longer used, sidestepping cache issues on EPIA-800
      TV-Out modes are now better supported - this should be the end
        of the magenta stripes :)
      Brightness/Contrast controls disabled for the time being - they were
        seriously degrading picture quality, especially with TV-Out.

    To Do:
    Implement Hue/Saturation controls
    Support / Test multiple frames
    Test colour-key code more extensively
*/

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

#include "cyberblade_regs.h"

pciinfo_t pci_info;

char save_colourkey[6];
char *cyberblade_mem;

#ifdef DEBUG_LOGFILE
FILE *logfile=0;
#define LOGWRITE(x) {if(logfile) fprintf(logfile,x);}
#else
#define LOGWRITE(x)
#endif

/* Helper functions for reading registers. */    

static void CROUTW(int reg,int val)
{
	CROUTB(reg,val&255);
	CROUTB(reg+1,(val>>8)&255);
}

static void SROUTW(int reg,int val)
{
	SROUTB(reg,val&255);
	SROUTB(reg+1,(val>>8)&255);
}

void DumpRegisters(void)
{
#ifdef DEBUG_LOGFILE
        int reg,val;
        if(logfile)
        {
                LOGWRITE("CRTC Register Dump:\n")
                for(reg=0;reg<256;++reg)
                {
                        val=CRINB(reg);
                        fprintf(logfile,"CR0x%2x: 0x%2x\n",reg,val);
                }
                LOGWRITE("SR Register Dump:\n")
                for(reg=0;reg<256;++reg)
                {
                        val=SRINB(reg);
                        fprintf(logfile,"SR0x%2x: 0x%2x\n",reg,val);
                }
        }
#endif
}
/* --- */

static vidix_capability_t cyberblade_cap =
{
	"Trident CyberBlade i1 driver",
	"Alastair M. Robinson <blackfive@fakenhamweb.co.uk>",
	TYPE_OUTPUT,
	{ 0, 0, 0, 0 },
	1024,
	1024,
	4,
	4,
	-1,
	FLAG_UPSCALER|FLAG_DOWNSCALER,
	VENDOR_TRIDENT,
	-1,
	{ 0, 0, 0, 0 }
};

static unsigned short cyberblade_card_ids[] =
{
	DEVICE_TRIDENT_CYBERBLADE_I7,
	DEVICE_TRIDENT_CYBERBLADE_I7D,
	DEVICE_TRIDENT_CYBERBLADE_I1,
	DEVICE_TRIDENT_CYBERBLADE_I12,
	DEVICE_TRIDENT_CYBERBLADE_I13,
	DEVICE_TRIDENT_CYBERBLADE_XPAI1
};


static int find_chip(unsigned chip_id)
{
  unsigned i;
  for(i = 0;i < sizeof(cyberblade_card_ids)/sizeof(unsigned short);i++)
  {
    if(chip_id == cyberblade_card_ids[i]) return i;
  }
  return -1;
}

static int cyberblade_probe(int verbose, int force)
{
	pciinfo_t lst[MAX_PCI_DEVICES];
	unsigned i,num_pci;
	int err;
	err = pci_scan(lst,&num_pci);
	if(err)
	{
		printf("[cyberblade] Error occurred during pci scan: %s\n",strerror(err));
		return err;
	}
	else
	{
		err = ENXIO;
		for(i=0; i < num_pci; i++)
		{
			if(lst[i].vendor == VENDOR_TRIDENT)
			{
				int idx;
				const char *dname;
				idx = find_chip(lst[i].device);
				if(idx == -1)
					continue;
				dname = pci_device_name(VENDOR_TRIDENT, lst[i].device);
				dname = dname ? dname : "Unknown chip";
				printf("[cyberblade] Found chip: %s\n", dname);
				if ((lst[i].command & PCI_COMMAND_IO) == 0)
				{
					printf("[cyberblade] Device is disabled, ignoring\n");
					continue;
				}
				cyberblade_cap.device_id = lst[i].device;
				err = 0;
				memcpy(&pci_info, &lst[i], sizeof(pciinfo_t));
				break;
			}
		}
	}

	if(err && verbose) printf("[cyberblade] Can't find chip\n");
		return err;
}


static int cyberblade_init(void)
{
	cyberblade_mem = map_phys_mem(pci_info.base0, 0x800000); 
	enable_app_io();
	save_colourkey[0]=SRINB(0x50);
	save_colourkey[1]=SRINB(0x51);
	save_colourkey[2]=SRINB(0x52);
	save_colourkey[3]=SRINB(0x54);
	save_colourkey[4]=SRINB(0x55);
	save_colourkey[5]=SRINB(0x56);
#ifdef DEBUG_LOGFILE
	logfile=fopen("/tmp/cyberblade_vidix.log","w");
#endif
	return 0;
}

static void cyberblade_destroy(void)
{
	int protect;
#ifdef DEBUG_LOGFILE
	if(logfile)
		fclose(logfile);
#endif
	protect=SRINB(0x11);
	SROUTB(0x11, 0x92);
	CROUTB(0x8E, 0xc4); /* Disable overlay */
	SROUTB(0x50,save_colourkey[0]);
	SROUTB(0x51,save_colourkey[1]);
	SROUTB(0x52,save_colourkey[2]);
	SROUTB(0x54,save_colourkey[3]);
	SROUTB(0x55,save_colourkey[4]);
	SROUTB(0x56,save_colourkey[5]);
	SROUTB(0x11, protect);
	disable_app_io();
	unmap_phys_mem(cyberblade_mem, 0x800000); 
}


static int cyberblade_get_caps(vidix_capability_t *to)
{
	memcpy(to, &cyberblade_cap, sizeof(vidix_capability_t));
	return 0;
}


static int is_supported_fourcc(uint32_t fourcc)
{
	switch(fourcc)
	{
		case IMGFMT_YUY2:
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_YVU9:
		case IMGFMT_BGR16:
			return 1;
		default:
			return 0;
	}
}

static int cyberblade_query_fourcc(vidix_fourcc_t *to)
{
	if(is_supported_fourcc(to->fourcc))
	{
		to->depth = VID_DEPTH_1BPP | VID_DEPTH_2BPP |
			VID_DEPTH_4BPP | VID_DEPTH_8BPP |
			VID_DEPTH_12BPP| VID_DEPTH_15BPP|
			VID_DEPTH_16BPP| VID_DEPTH_24BPP|
			VID_DEPTH_32BPP;
		to->flags = VID_CAP_EXPAND | VID_CAP_SHRINK | VID_CAP_COLORKEY;
		return 0;
	}
	else
		to->depth = to->flags = 0;
	return ENOSYS;
}


static int frames[VID_PLAY_MAXFRAMES];

static vidix_grkey_t cyberblade_grkey;

static int cyberblade_get_gkeys(vidix_grkey_t *grkey)
{
	memcpy(grkey, &cyberblade_grkey, sizeof(vidix_grkey_t));
	return(0);
}

static int cyberblade_set_gkeys(const vidix_grkey_t *grkey)
{
	int pixfmt=CRINB(0x38);
	int protect;
	memcpy(&cyberblade_grkey, grkey, sizeof(vidix_grkey_t));

	protect=SRINB(0x11);
	SROUTB(0x11, 0x92);

	if(pixfmt&0x28) /* 32 or 24 bpp */
	{
		SROUTB(0x50, cyberblade_grkey.ckey.blue); /* Colour Key */
		SROUTB(0x51, cyberblade_grkey.ckey.green); /* Colour Key */
		SROUTB(0x52, cyberblade_grkey.ckey.red); /* Colour Key */
		SROUTB(0x54, 0xff); /* Colour Key Mask */
		SROUTB(0x55, 0xff); /* Colour Key Mask */
		SROUTB(0x56, 0xff); /* Colour Key Mask */
	}
	else
	{
		int tmp=((cyberblade_grkey.ckey.blue & 0xF8)>>3)
			| ((cyberblade_grkey.ckey.green & 0xfc)<<3)
			| ((cyberblade_grkey.ckey.red & 0xf8)<<8);
		SROUTB(0x50, tmp&0xff); /* Colour Key */
		SROUTB(0x51, (tmp>>8)&0xff); /* Colour Key */
		SROUTB(0x52, 0); /* Colour Key */
		SROUTB(0x54, 0xff); /* Colour Key Mask */
		SROUTB(0x55, 0xff); /* Colour Key Mask */
		SROUTB(0x56, 0x00); /* Colour Key Mask */
	}
	SROUTB(0x11,protect);
	return(0);
}


static vidix_video_eq_t equal =
{
	VEQ_CAP_BRIGHTNESS | VEQ_CAP_SATURATION | VEQ_CAP_HUE,
	300, 100, 0, 0, 0, 0, 0, 0
};

static int cyberblade_get_eq( vidix_video_eq_t * eq)
{
  memcpy(eq,&equal,sizeof(vidix_video_eq_t));
  return 0;
}

static int cyberblade_set_eq( const vidix_video_eq_t * eq)
{
	int br,sat,cr,protect;
	if(eq->cap & VEQ_CAP_BRIGHTNESS) equal.brightness = eq->brightness;
	if(eq->cap & VEQ_CAP_CONTRAST) equal.contrast   = eq->contrast;
	if(eq->cap & VEQ_CAP_SATURATION) equal.saturation = eq->saturation;
	if(eq->cap & VEQ_CAP_HUE)        equal.hue        = eq->hue;
	if(eq->cap & VEQ_CAP_RGB_INTENSITY)
	{
		equal.red_intensity   = eq->red_intensity;
		equal.green_intensity = eq->green_intensity;
		equal.blue_intensity  = eq->blue_intensity;
	}
	equal.flags = eq->flags;

	cr = (equal.contrast) * 31 / 2000; cr+=16;
	if (cr < 0) cr = 0; if(cr > 7) cr = 7;
	cr=cr<<4 | cr;

	br = (equal.brightness+1000) * 63 / 2000;
	if (br < 0) br = 0; if(br > 63) br = 63;
	if(br>32) br-=32; else br+=32;

	sat = (equal.saturation + 1000) * 16 / 2000;
	if (sat < 0) sat = 0; if(sat > 31) sat = 31;

	protect=SRINB(0x11);
	SROUTB(0x11, 0x92);

	SROUTB(0xBC,cr);
	SROUTW(0xB0,(br<<10)|4);

	SROUTB(0x11, protect);

	return 0;
}


static int YOffs,UOffs,VOffs;

static int cyberblade_config_playback(vidix_playback_t *info)
{
	int src_w, drw_w;
	int src_h, drw_h;
	int hscale,vscale;
	long base0;
	int y_pitch = 0, uv_pitch = 0;
	int protect=0;
	int layout=0;
	unsigned int i;

	if(!is_supported_fourcc(info->fourcc))
		return -1;

	src_w = info->src.w;
	src_h = info->src.h;

	drw_w = info->dest.w;
	drw_h = info->dest.h;

	switch(info->fourcc)
	{
		case IMGFMT_YUY2:
		case IMGFMT_BGR16:
			y_pitch = (src_w*2 + 15) & ~15;
			uv_pitch = 0;
			YOffs=VOffs=UOffs=info->offset.y = info->offset.v = info->offset.u = 0;
			info->frame_size = y_pitch*src_h;
			layout=0x0; /* packed */
			break;
		case IMGFMT_YV12:
		case IMGFMT_I420:
			y_pitch = (src_w+15) & ~15;
			uv_pitch = ((src_w/2)+7) & ~7;
			YOffs=info->offset.y = 0;
			VOffs=info->offset.v = y_pitch*src_h;
			UOffs=info->offset.u = info->offset.v+(uv_pitch)*(src_h/2);
			info->frame_size = y_pitch*src_h + 2*uv_pitch*(src_h/2);
			layout=0x1; /* planar, 4:1:1 */
			break;
		case IMGFMT_YVU9:
			y_pitch = (src_w+15) & ~15;
			uv_pitch = ((src_w/4)+3) & ~3;
			YOffs=info->offset.y = 0;
			VOffs=info->offset.v = y_pitch*src_h;
			UOffs=info->offset.u = info->offset.v+(uv_pitch)*(src_h/4);
			info->frame_size = y_pitch*src_h + 2*uv_pitch*(src_h/4);
			layout=0x51; /* planar, 16:1:1 */
			break;
	}

	/* Assume we have 2 MB to play with */
	info->num_frames = 0x200000 / info->frame_size;
	if(info->num_frames > VID_PLAY_MAXFRAMES)
		info->num_frames = VID_PLAY_MAXFRAMES;

	/* Start at 6 MB. Let's hope it's not in use. */
	base0 = 0x600000;
	info->dga_addr = cyberblade_mem + base0;

	info->dest.pitch.y = 16;
	info->dest.pitch.u = 16;
	info->dest.pitch.v = 16;

	for(i = 0; i < info->num_frames; i++)
	{
		info->offsets[i] = info->frame_size * i;
		frames[i] = base0+info->offsets[i];
	}

	OUTPORT8(0x3d4,0x39);
	OUTPORT8(0x3d5,INPORT(0x3d5)|1);

	SRINB(0x0b); /* Select new mode */

	/* Unprotect hardware registers... */
	protect=SRINB(0x11);
	SROUTB(0x11, 0x92);

	SROUTB(0x57, 0xc0); /* Playback key function */
	SROUTB(0x21, 0x34); /* Signature control */
	SROUTB(0x37, 0x30); /* Video key mode */

        cyberblade_set_gkeys(&cyberblade_grkey);

	/* compute_scale_factor(&src_w, &drw_w, &shrink, &zoom); */
	{
		int HTotal,VTotal,HSync,VSync,Overflow,HDisp,VDisp;
		int HWinStart,VWinStart;
		int tx1,ty1,tx2,ty2;

		HTotal=CRINB(0x00);
		HSync=CRINB(0x04);
		VTotal=CRINB(0x06);
		VSync=CRINB(0x10);
		Overflow=CRINB(0x07);
		HTotal <<=3;
		HSync <<=3;
		VTotal |= (Overflow & 1) <<8;
		VTotal |= (Overflow & 0x20) <<4;
		VTotal +=4;
		VSync |= (Overflow & 4) <<6;
		VSync |= (Overflow & 0x80) <<2;

		if(CRINB(0xd1)&0x80)
		{
			int TVHTotal,TVVTotal,TVHSyncStart,TVVSyncStart,TVOverflow;
			LOGWRITE("[cyberblade] Using TV-CRTC\n");

    			HDisp=(1+CRINB(0x01))*8;
    			VDisp=1+CRINB(0x12);
    			Overflow=CRINB(0x07);
    			VDisp |= (Overflow & 2) <<7;
    			VDisp |= (Overflow & 0x40) << 3;
 
    			TVHTotal=CRINB(0xe0)*8;
    			TVVTotal=CRINB(0xe6);
    			TVOverflow=CRINB(0xe7);
    			if(TVOverflow&0x20) TVVTotal|=512;
    			if(TVOverflow&0x01) TVVTotal|=256;
    			TVHTotal+=40; TVVTotal+=2;
 
    			TVHSyncStart=CRINB(0xe4)*8;
    			TVVSyncStart=CRINB(0xf0);
    			if(TVOverflow&0x80) TVVSyncStart|=512;
			if(TVOverflow&0x04) TVVSyncStart|=256;
 
			HWinStart=(TVHTotal-HDisp)&15;
			HWinStart|=(HTotal-HDisp)&15;
			HWinStart+=(TVHTotal-TVHSyncStart)-49;
		}
		else
		{
			LOGWRITE("[cyberblade] Using Standard CRTC\n");
			HWinStart=(HTotal-HSync)+15;
		}
                VWinStart=(VTotal-VSync)-8;

		printf("[cyberblade] HTotal: 0x%x, HSStart: 0x%x\n",HTotal,HSync); 
		printf("  VTotal: 0x%x, VStart: 0x%x\n",VTotal,VSync);
		tx1=HWinStart+info->dest.x;
		ty1=VWinStart+info->dest.y;
		tx2=tx1+info->dest.w;
		ty2=ty1+info->dest.h;

		CROUTW(0x86,tx1);
		CROUTW(0x88,ty1);
		CROUTW(0x8a,tx2);
		CROUTW(0x8c,ty2+3);
	}

	if(src_w==drw_w)
		hscale=0;
	else if(src_w<drw_w)
	{
		hscale=((src_w<<10)/(drw_w-2)) & 0x1fff;
	}
	else
	{
		hscale=0x8000 | ((((src_w/drw_w)-1)&7)<<10) | (((drw_w<<10)/src_w) & 0x3ff);
	}

	vscale=(src_h<<10)/(drw_h);
	if(drw_h<src_h)
		vscale=0x8000|((drw_h<<10)/(src_h));

	/* Write scale factors to hardware */

	CROUTW(0x80,hscale); /* Horizontal Scale */
	CROUTW(0x82,vscale); /* Vertical Scale */

	/* Now set the start address and data layout */
	{
		int lb = (y_pitch+2) >> 2;
		CROUTB(0x95, ((lb & 0x100)>>1) | 0x08 ); /* Linebuffer level bit 8 & threshold */
		CROUTB(0x96, (lb & 0xFF)); /* Linebuffer level */

		CROUTB(0x97, 0x00); /* VDE Flags */
		CROUTB(0xBA, 0x00); /* Chroma key */
		CROUTB(0xBB, 0x00); /* Chroma key */
		CROUTB(0xBC, 0xFF); /* Chroma key */
		CROUTB(0xBD, 0xFF); /* Chroma key */
		CROUTB(0xBE, 0x04); /* Capture control */

		if(src_w > 384)
			layout|=4; /* 2x line buffers */
		SROUTB(0x97, layout);

		CROUTW(0x90,y_pitch); /* Y Bytes per row */
		SROUTW(0x9A,uv_pitch); /* UV Bytes per row */

		switch(info->fourcc)
		{
			case IMGFMT_BGR16:
				CROUTB(0x8F, 0x24); /* VDE Flags - Edge Recovery & CSC Bypass */
				CROUTB(0xBF, 0x02); /* Video format - RGB16 */
				SROUTB(0xBE, 0x0); /* HSCB disabled */
				break;
			default:
				CROUTB(0x8F, 0x20); /* VDE Flags - Edge Recovery */
				CROUTB(0xBF, 0x00); /* Video format - YUV */
				SROUTB(0xBE, 0x00); /* HSCB disable - was 0x03*/
				break;
		}

		CROUTB(0x92, ((base0+info->offset.y) >> 3) &0xff); /* Lower 8 bits of start address */
		CROUTB(0x93, ((base0+info->offset.y) >> 11) &0xff); /* Mid 8 bits of start address */
		CROUTB(0x94, ((base0+info->offset.y) >> 19) &0xf); /* Upper 4 bits of start address */
		SROUTB(0x80, ((base0+info->offset.v) >> 3) &0xff); /* Lower 8 bits of start address */
		SROUTB(0x81, ((base0+info->offset.v) >> 11) &0xff); /* Mid 8 bits of start address */
		SROUTB(0x82, ((base0+info->offset.v) >> 19) &0xf); /* Upper 4 bits of start address */
		SROUTB(0x83, ((base0+info->offset.u) >> 3) &0xff); /* Lower 8 bits of start address */
		SROUTB(0x84, ((base0+info->offset.u) >> 11) &0xff); /* Mid 8 bits of start address */
		SROUTB(0x85, ((base0+info->offset.u) >> 19) &0xf); /* Upper 4 bits of start address */
	}

	cyberblade_set_eq(&equal);

	/* Protect hardware registers again */
	SROUTB(0x11, protect);
	return 0;
}


static int cyberblade_playback_on(void)
{
	LOGWRITE("Enable overlay\n");
	CROUTB(0x8E, 0xd4); /* VDE Flags*/

	return 0;
}


static int cyberblade_playback_off(void)
{
        LOGWRITE("Disable overlay\n"); 
	CROUTB(0x8E, 0xc4); /* VDE Flags*/

	return 0;
}


static int cyberblade_frame_sel(unsigned int frame)
{
	int protect;
        LOGWRITE("Frame select\n"); 
	protect=SRINB(0x11);
	SROUTB(0x11, 0x92);
	/* Set overlay address to that of selected frame */
	CROUTB(0x92, ((frames[frame]+YOffs) >> 3) &0xff); /* Lower 8 bits of start address */
	CROUTB(0x93, ((frames[frame]+YOffs) >> 11) &0xff); /* Mid 8 bits of start address */
	CROUTB(0x94, ((frames[frame]+YOffs) >> 19) &0xf); /* Upper 4 bits of start address */
	SROUTB(0x80, ((frames[frame]+VOffs) >> 3) &0xff); /* Lower 8 bits of start address */
	SROUTB(0x81, ((frames[frame]+VOffs) >> 11) &0xff); /* Mid 8 bits of start address */
	SROUTB(0x82, ((frames[frame]+VOffs) >> 19) &0xf); /* Upper 4 bits of start address */
	SROUTB(0x83, ((frames[frame]+UOffs) >> 3) &0xff); /* Lower 8 bits of start address */
	SROUTB(0x84, ((frames[frame]+UOffs) >> 11) &0xff); /* Mid 8 bits of start address */
	SROUTB(0x85, ((frames[frame]+UOffs) >> 19) &0xf); /* Upper 4 bits of start address */
	SROUTB(0x11, protect);
	return 0;
}

VDXDriver cyberblade_drv = {
  "cyberblade",
  NULL,
  .probe = cyberblade_probe,
  .get_caps = cyberblade_get_caps,
  .query_fourcc = cyberblade_query_fourcc,
  .init = cyberblade_init,
  .destroy = cyberblade_destroy,
  .config_playback = cyberblade_config_playback,
  .playback_on = cyberblade_playback_on,
  .playback_off = cyberblade_playback_off,
  .frame_sel = cyberblade_frame_sel,
  .get_eq = cyberblade_get_eq,
  .set_eq = cyberblade_set_eq,
  .get_gkey = cyberblade_get_gkeys,
  .set_gkey = cyberblade_set_gkeys,
};
