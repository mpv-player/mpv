/**
    SiS graphics misc definitions.

    Taken from SiS Xv driver:
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

**/

#ifndef VIDIX_SIS_DEFS_H
#define VIDIX_SIS_DEFS_H

/** PCI IDs **/
#define VENDOR_SIS            0x1039

#define DEVICE_SIS_300        0x0300
#define DEVICE_SIS_315H       0x0310
#define DEVICE_SIS_315        0x0315
#define DEVICE_SIS_315PRO     0x0325
#define DEVICE_SIS_330        0x0330
#define DEVICE_SIS_540        0x0540
#define DEVICE_SIS_540_VGA    0x5300
#define DEVICE_SIS_550        0x0550
#define DEVICE_SIS_550_VGA    0x5315
#define DEVICE_SIS_630        0x0630
#define DEVICE_SIS_630_VGA    0x6300
#define DEVICE_SIS_650        0x0650
#define DEVICE_SIS_650_VGA    0x6325
#define DEVICE_SIS_730        0x0730


/* TW: VBFlags */
#define CRT2_DEFAULT            0x00000001
#define CRT2_LCD                0x00000002	/* TW: Never change the order of the CRT2_XXX entries */
#define CRT2_TV                 0x00000004	/*     (see SISCycleCRT2Type())                       */
#define CRT2_VGA                0x00000008
#define CRT2_ENABLE		(CRT2_LCD | CRT2_TV | CRT2_VGA)
#define DISPTYPE_DISP2		CRT2_ENABLE
#define TV_NTSC                 0x00000010
#define TV_PAL                  0x00000020
#define TV_HIVISION             0x00000040
#define TV_HIVISION_LV          0x00000080
#define TV_TYPE                 (TV_NTSC | TV_PAL | TV_HIVISION | TV_HIVISION_LV)
#define TV_AVIDEO               0x00000100
#define TV_SVIDEO               0x00000200
#define TV_SCART                0x00000400
#define TV_INTERFACE            (TV_AVIDEO | TV_SVIDEO | TV_SCART | TV_CHSCART | TV_CHHDTV)
#define VB_USELCDA		0x00000800
#define TV_PALM                 0x00001000
#define TV_PALN                 0x00002000
#define TV_CHSCART              0x00008000
#define TV_CHHDTV               0x00010000
#define VGA2_CONNECTED          0x00040000
#define DISPTYPE_CRT1		0x00080000	/* TW: CRT1 connected and used */
#define DISPTYPE_DISP1		DISPTYPE_CRT1
#define VB_301                  0x00100000	/* Video bridge type */
#define VB_301B                 0x00200000
#define VB_302B                 0x00400000
#define VB_30xBDH		0x00800000	/* 30xB DH version (w/o LCD support) */
#define VB_LVDS                 0x01000000
#define VB_CHRONTEL             0x02000000
#define VB_301LV                0x04000000
#define VB_302LV                0x08000000
#define VB_30xLV                VB_301LV
#define VB_30xLVX               VB_302LV
#define VB_TRUMPION		0x10000000
#define VB_VIDEOBRIDGE		(VB_301|VB_301B|VB_302B|VB_301LV|VB_302LV| \
				 VB_LVDS|VB_CHRONTEL|VB_TRUMPION)	/* TW */
#define VB_SISBRIDGE            (VB_301|VB_301B|VB_302B|VB_301LV|VB_302LV)
#define SINGLE_MODE             0x20000000	/* TW: CRT1 or CRT2; determined by DISPTYPE_CRTx */
#define VB_DISPMODE_SINGLE	SINGLE_MODE	/* TW: alias */
#define MIRROR_MODE		0x40000000	/* TW: CRT1 + CRT2 identical (mirror mode) */
#define VB_DISPMODE_MIRROR	MIRROR_MODE	/* TW: alias */
#define DUALVIEW_MODE		0x80000000	/* TW: CRT1 + CRT2 independent (dual head mode) */
#define VB_DISPMODE_DUAL	DUALVIEW_MODE	/* TW: alias */
#define DISPLAY_MODE            (SINGLE_MODE | MIRROR_MODE | DUALVIEW_MODE)	/* TW */

/* SiS vga engine type */
#define UNKNOWN_VGA  0
#define SIS_300_VGA  1
#define SIS_315_VGA  2

extern unsigned int sis_verbose;
extern unsigned short sis_iobase;
extern unsigned int sis_vga_engine;
extern unsigned int sis_vbflags;
extern unsigned int sis_overlay_on_crt1;
extern int sis_crt1_off;
extern unsigned int sis_detected_crt2_devices;
extern unsigned int sis_force_crt2_type;
extern int sis_device_id;

#endif				/* VIDIX_SIS_DEFS_H */
