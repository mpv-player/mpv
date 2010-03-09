/*
 * VIDIX driver for SiS chipsets.
 * Video bridge detection for SiS 300 and 310/325 series.
 *
 * Copyright (C) 2003 Jake Page, Sugar Media
 * Based on SiS Xv driver
 * Copyright 2002-2003 by Thomas Winischhofer, Vienna, Austria
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dha.h"
#include "sis_bridge.h"
#include "sis_regs.h"
#include "sis_defs.h"


static void sis_ddc2_delay(unsigned short delaytime)
{
    unsigned short i;
    int temp;

    for (i = 0; i < delaytime; i++) {
	inSISIDXREG(SISSR, 0x05, temp);
    }
}


static int sis_do_sense(int tempbl, int tempbh, int tempcl, int tempch)
{
    int temp;

    outSISIDXREG(SISPART4, 0x11, tempbl);
    temp = tempbh | tempcl;
    setSISIDXREG(SISPART4, 0x10, 0xe0, temp);
    //usleep(200000);
    sis_ddc2_delay(0x1000);
    tempch &= 0x7f;
    inSISIDXREG(SISPART4, 0x03, temp);
    temp ^= 0x0e;
    temp &= tempch;
    return temp == tempch;
}


/* sense connected devices on 30x bridge */
static void sis_sense_30x(void)
{
    unsigned char backupP4_0d, backupP2_00, biosflag;
    unsigned char testsvhs_tempbl, testsvhs_tempbh;
    unsigned char testsvhs_tempcl, testsvhs_tempch;
    unsigned char testcvbs_tempbl, testcvbs_tempbh;
    unsigned char testcvbs_tempcl, testcvbs_tempch;
    unsigned char testvga2_tempbl, testvga2_tempbh;
    unsigned char testvga2_tempcl, testvga2_tempch;
    int myflag, result = 0, i, j, haveresult;

    inSISIDXREG(SISPART4, 0x0d, backupP4_0d);
    outSISIDXREG(SISPART4, 0x0d, (backupP4_0d | 0x04));

    inSISIDXREG(SISPART2, 0x00, backupP2_00);
    outSISIDXREG(SISPART2, 0x00, (backupP2_00 | 0x1c));

    sis_do_sense(0, 0, 0, 0);

    if (sis_vga_engine == SIS_300_VGA) {
	    testvga2_tempbh = 0x00;
	    testvga2_tempbl = 0xd1;
	    testsvhs_tempbh = 0x00;
	    testsvhs_tempbl = 0xb9;
	    testcvbs_tempbh = 0x00;
	    testcvbs_tempbl = 0xb3;
	    biosflag = 0;

	if (sis_vbflags & (VB_301B | VB_302B | VB_301LV | VB_302LV)) {
	    testvga2_tempbh = 0x01;
	    testvga2_tempbl = 0x90;
	    testsvhs_tempbh = 0x01;
	    testsvhs_tempbl = 0x6b;
	    testcvbs_tempbh = 0x01;
	    testcvbs_tempbl = 0x74;
	}
	inSISIDXREG(SISPART4, 0x01, myflag);
	if (myflag & 0x04) {
	    testvga2_tempbh = 0x00;
	    testvga2_tempbl = 0xfd;
	    testsvhs_tempbh = 0x00;
	    testsvhs_tempbl = 0xdd;
	    testcvbs_tempbh = 0x00;
	    testcvbs_tempbl = 0xee;
	}
	testvga2_tempch = 0x0e;
	testvga2_tempcl = 0x08;
	testsvhs_tempch = 0x06;
	testsvhs_tempcl = 0x04;
	testcvbs_tempch = 0x08;
	testcvbs_tempcl = 0x04;

	if (sis_device_id == DEVICE_SIS_300) {
	    inSISIDXREG(SISSR, 0x3b, myflag);
	    if (!(myflag & 0x01)) {
		testvga2_tempbh = 0x00;
		testvga2_tempbl = 0x00;
		testvga2_tempch = 0x00;
		testvga2_tempcl = 0x00;
	    }
	}
    } else {
	    testvga2_tempbh = 0x00;
	    testvga2_tempbl = 0xd1;
	    testsvhs_tempbh = 0x00;
	    testsvhs_tempbl = 0xb9;
	    testcvbs_tempbh = 0x00;
	    testcvbs_tempbl = 0xb3;
	    biosflag = 0;

	if (sis_vbflags & (VB_301B | VB_302B | VB_301LV | VB_302LV)) {
		if (sis_vbflags & (VB_301B | VB_302B)) {
		    testvga2_tempbh = 0x01;
		    testvga2_tempbl = 0x90;
		    testsvhs_tempbh = 0x01;
		    testsvhs_tempbl = 0x6b;
		    testcvbs_tempbh = 0x01;
		    testcvbs_tempbl = 0x74;
		} else {
		    testvga2_tempbh = 0x00;
		    testvga2_tempbl = 0x00;
		    testsvhs_tempbh = 0x02;
		    testsvhs_tempbl = 0x00;
		    testcvbs_tempbh = 0x01;
		    testcvbs_tempbl = 0x00;
		}
	}
	if (sis_vbflags & (VB_301 | VB_301B | VB_302B)) {
	    inSISIDXREG(SISPART4, 0x01, myflag);
	    if (myflag & 0x04) {
		testvga2_tempbh = 0x00;
		testvga2_tempbl = 0xfd;
		testsvhs_tempbh = 0x00;
		testsvhs_tempbl = 0xdd;
		testcvbs_tempbh = 0x00;
		testcvbs_tempbl = 0xee;
	    }
	}
	if (sis_vbflags & (VB_301LV | VB_302LV)) {
	    /* TW: No VGA2 or SCART on LV bridges */
	    testvga2_tempbh = 0x00;
	    testvga2_tempbl = 0x00;
	    testvga2_tempch = 0x00;
	    testvga2_tempcl = 0x00;
	    testsvhs_tempch = 0x04;
	    testsvhs_tempcl = 0x08;
	    testcvbs_tempch = 0x08;
	    testcvbs_tempcl = 0x08;
	} else {
	    testvga2_tempch = 0x0e;
	    testvga2_tempcl = 0x08;
	    testsvhs_tempch = 0x06;
	    testsvhs_tempcl = 0x04;
	    testcvbs_tempch = 0x08;
	    testcvbs_tempcl = 0x04;
	}
    }

    /* XXX: ?? andSISIDXREG(SISCR, 0x32, ~0x14); */
    /* pSiS->postVBCR32 &= ~0x14; */

    /* scan for VGA2/SCART */
    if (testvga2_tempch || testvga2_tempcl ||
	testvga2_tempbh || testvga2_tempbl) {

	haveresult = 0;
	for (j = 0; j < 10; j++) {
	    result = 0;
	    for (i = 0; i < 3; i++) {
		if (sis_do_sense(testvga2_tempbl, testvga2_tempbh,
				 testvga2_tempcl, testvga2_tempch))
		    result++;
	    }
	    if ((result == 0) || (result >= 2))
		break;
	}
	if (result) {
	    if (biosflag & 0x01) {
		if (sis_verbose > 1) {
		    printf
			("[SiS] SiS30x: Detected TV connected to SCART output\n");
		}
		sis_vbflags |= TV_SCART;
		orSISIDXREG(SISCR, 0x32, 0x04);
		/*pSiS->postVBCR32 |= 0x04; */
	    } else {
		if (sis_verbose > 1) {
		    printf
			("[SiS] SiS30x: Detected secondary VGA connection\n");
		}
		sis_vbflags |= VGA2_CONNECTED;
		orSISIDXREG(SISCR, 0x32, 0x10);
		/*pSiS->postVBCR32 |= 0x10; */
	    }
	}
    }

    /* scanning for TV */

    /* XXX: ?? andSISIDXREG(SISCR, 0x32, ~0x03); */
    /* pSiS->postVBCR32 &= ~0x03; */

    result = sis_do_sense(testsvhs_tempbl, testsvhs_tempbh,
			  testsvhs_tempcl, testsvhs_tempch);


    haveresult = 0;
    for (j = 0; j < 10; j++) {
	result = 0;
	for (i = 0; i < 3; i++) {
	    if (sis_do_sense(testsvhs_tempbl, testsvhs_tempbh,
			     testsvhs_tempcl, testsvhs_tempch))
		result++;
	}
	if ((result == 0) || (result >= 2))
	    break;
    }
    if (result) {
	if (sis_verbose > 1) {
	    printf
		("[SiS] SiS30x: Detected TV connected to SVIDEO output\n");
	}
	/* TW: So we can be sure that there IS a SVIDEO output */
	sis_vbflags |= TV_SVIDEO;
	orSISIDXREG(SISCR, 0x32, 0x02);
	//pSiS->postVBCR32 |= 0x02;
    }

    if ((biosflag & 0x02) || (!(result))) {
	haveresult = 0;
	for (j = 0; j < 10; j++) {
	    result = 0;
	    for (i = 0; i < 3; i++) {
		if (sis_do_sense(testcvbs_tempbl, testcvbs_tempbh,
				 testcvbs_tempcl, testcvbs_tempch))
		    result++;
	    }
	    if ((result == 0) || (result >= 2))
		break;
	}
	if (result) {
	    if (sis_verbose > 1) {
		printf
		    ("[SiS] SiS30x: Detected TV connected to COMPOSITE output\n");
	    }
	    sis_vbflags |= TV_AVIDEO;
	    orSISIDXREG(SISCR, 0x32, 0x01);
	    //pSiS->postVBCR32 |= 0x01;
	}
    }

    sis_do_sense(0, 0, 0, 0);

    outSISIDXREG(SISPART2, 0x00, backupP2_00);
    outSISIDXREG(SISPART4, 0x0d, backupP4_0d);
}


static void sis_detect_crt1(void)
{
    unsigned char CR32;
    unsigned char CRT1Detected = 0;
    unsigned char OtherDevices = 0;

    if (!(sis_vbflags & VB_VIDEOBRIDGE)) {
	sis_crt1_off = 0;
	return;
    }

    inSISIDXREG(SISCR, 0x32, CR32);

    if (CR32 & 0x20)
	CRT1Detected = 1;
    if (CR32 & 0x5F)
	OtherDevices = 1;

    if (sis_crt1_off == -1) {
	if (!CRT1Detected) {
	    /* BIOS detected no CRT1. */
	    /* If other devices exist, switch it off */
	    if (OtherDevices)
		sis_crt1_off = 1;
	    else
		sis_crt1_off = 0;
	} else {
	    /* BIOS detected CRT1, leave/switch it on */
	    sis_crt1_off = 0;
	}
    }
    if (sis_verbose > 0) {
	printf("[SiS] %sCRT1 connection detected\n",
	       sis_crt1_off ? "No " : "");
    }
}

static void sis_detect_tv(void)
{
    unsigned char SR16, SR38, CR32, CR38 = 0, CR79;
    int temp = 0;

    if (!(sis_vbflags & VB_VIDEOBRIDGE))
	return;

    inSISIDXREG(SISCR, 0x32, CR32);
    inSISIDXREG(SISSR, 0x16, SR16);
    inSISIDXREG(SISSR, 0x38, SR38);
    switch (sis_vga_engine) {
    case SIS_300_VGA:
	if (sis_device_id == DEVICE_SIS_630_VGA)
	    temp = 0x35;
	break;
    case SIS_315_VGA:
	temp = 0x38;
	break;
    }
    if (temp) {
	inSISIDXREG(SISCR, temp, CR38);
    }

    if (CR32 & 0x47)
	sis_vbflags |= CRT2_TV;

    if (CR32 & 0x04)
	sis_vbflags |= TV_SCART;
    else if (CR32 & 0x02)
	sis_vbflags |= TV_SVIDEO;
    else if (CR32 & 0x01)
	sis_vbflags |= TV_AVIDEO;
    else if (CR32 & 0x40)
	sis_vbflags |= (TV_SVIDEO | TV_HIVISION);
    else if ((CR38 & 0x04) && (sis_vbflags & (VB_301LV | VB_302LV)))
	sis_vbflags |= TV_HIVISION_LV;
    else if ((CR38 & 0x04) && (sis_vbflags & VB_CHRONTEL))
	sis_vbflags |= (TV_CHSCART | TV_PAL);
    else if ((CR38 & 0x08) && (sis_vbflags & VB_CHRONTEL))
	sis_vbflags |= (TV_CHHDTV | TV_NTSC);

    if (sis_vbflags & (TV_SCART | TV_SVIDEO | TV_AVIDEO | TV_HIVISION)) {
	if (sis_vga_engine == SIS_300_VGA) {
	    /* TW: Should be SR38 here as well, but this
	     *     does not work. Looks like a BIOS bug (2.04.5c).
	     */
	    if (SR16 & 0x20)
		sis_vbflags |= TV_PAL;
	    else
		sis_vbflags |= TV_NTSC;
	} else if ((sis_device_id == DEVICE_SIS_550_VGA)) {
	    inSISIDXREG(SISCR, 0x79, CR79);
	    if (CR79 & 0x08) {
		inSISIDXREG(SISCR, 0x79, CR79);
		CR79 >>= 5;
	    }
	    if (CR79 & 0x01) {
		sis_vbflags |= TV_PAL;
		if (CR38 & 0x40)
		    sis_vbflags |= TV_PALM;
		else if (CR38 & 0x80)
		    sis_vbflags |= TV_PALN;
	    } else
		sis_vbflags |= TV_NTSC;
	} else if ((sis_device_id == DEVICE_SIS_650_VGA)) {
	    inSISIDXREG(SISCR, 0x79, CR79);
	    if (CR79 & 0x20) {
		sis_vbflags |= TV_PAL;
		if (CR38 & 0x40)
		    sis_vbflags |= TV_PALM;
		else if (CR38 & 0x80)
		    sis_vbflags |= TV_PALN;
	    } else
		sis_vbflags |= TV_NTSC;
	} else {		/* 315, 330 */
	    if (SR38 & 0x01) {
		sis_vbflags |= TV_PAL;
		if (CR38 & 0x40)
		    sis_vbflags |= TV_PALM;
		else if (CR38 & 0x80)
		    sis_vbflags |= TV_PALN;
	    } else
		sis_vbflags |= TV_NTSC;
	}
    }

    if (sis_vbflags &
	(TV_SCART | TV_SVIDEO | TV_AVIDEO | TV_HIVISION | TV_CHSCART |
	 TV_CHHDTV)) {
	if (sis_verbose > 0) {
	    printf("[SiS] %sTV standard %s\n",
		   (sis_vbflags & (TV_CHSCART | TV_CHHDTV)) ? "Using " :
		   "Detected default ",
		   (sis_vbflags & TV_NTSC) ? ((sis_vbflags & TV_CHHDTV) ?
					      "480i HDTV" : "NTSC")
		   : ((sis_vbflags & TV_PALM) ? "PALM"
		      : ((sis_vbflags & TV_PALN) ? "PALN" : "PAL")));
	}
    }

}


static void sis_detect_crt2(void)
{
    unsigned char CR32;

    if (!(sis_vbflags & VB_VIDEOBRIDGE))
	return;

    /* CRT2-VGA not supported on LVDS and 30xLV */
    if (sis_vbflags & (VB_LVDS | VB_301LV | VB_302LV))
	return;

    inSISIDXREG(SISCR, 0x32, CR32);

    if (CR32 & 0x10)
	sis_vbflags |= CRT2_VGA;
}


/* Preinit: detect video bridge and sense connected devs */
static void sis_detect_video_bridge(void)
{
    int temp, temp1, temp2;


    sis_vbflags = 0;

    if (sis_vga_engine != SIS_300_VGA && sis_vga_engine != SIS_315_VGA)
	return;

    inSISIDXREG(SISPART4, 0x00, temp);
    temp &= 0x0F;
    if (temp == 1) {
	inSISIDXREG(SISPART4, 0x01, temp1);
	temp1 &= 0xff;
	if (temp1 >= 0xE0) {
	    sis_vbflags |= VB_302LV;
	    //pSiS->sishw_ext.ujVBChipID = VB_CHIP_302LV;
	    if (sis_verbose > 1) {
		printf
		    ("[SiS] Detected SiS302LV video bridge (ID 1; Revision 0x%x)\n",
		     temp1);
	    }

	} else if (temp1 >= 0xD0) {
	    sis_vbflags |= VB_301LV;
	    //pSiS->sishw_ext.ujVBChipID = VB_CHIP_301LV;
	    if (sis_verbose > 1) {
		printf
		    ("[SiS] Detected SiS301LV video bridge (ID 1; Revision 0x%x)\n",
		     temp1);
	    }
	} else if (temp1 >= 0xB0) {
	    sis_vbflags |= VB_301B;
	    //pSiS->sishw_ext.ujVBChipID = VB_CHIP_301B;
	    inSISIDXREG(SISPART4, 0x23, temp2);
	    if (!(temp2 & 0x02))
		sis_vbflags |= VB_30xBDH;
	    if (sis_verbose > 1) {
		printf
		    ("[SiS] Detected SiS301B%s video bridge (Revision 0x%x)\n",
		     (temp2 & 0x02) ? "" : " (DH)", temp1);
	    }
	} else {
	    sis_vbflags |= VB_301;
	    //pSiS->sishw_ext.ujVBChipID = VB_CHIP_301;
	    if (sis_verbose > 1) {
		printf
		    ("[SiS] Detected SiS301 video bridge (Revision 0x%x)\n",
		     temp1);
	    }
	}

	sis_sense_30x();

    } else if (temp == 2) {

	inSISIDXREG(SISPART4, 0x01, temp1);
	temp1 &= 0xff;
	if (temp1 >= 0xE0) {
	    sis_vbflags |= VB_302LV;
	    //pSiS->sishw_ext.ujVBChipID = VB_CHIP_302LV;
	    if (sis_verbose > 1) {
		printf
		    ("[SiS] Detected SiS302LV video bridge (ID 2; Revision 0x%x)\n",
		     temp1);
	    }
	} else if (temp1 >= 0xD0) {
	    sis_vbflags |= VB_301LV;
	    //pSiS->sishw_ext.ujVBChipID = VB_CHIP_301LV;
	    if (sis_verbose > 1) {
		printf
		    ("[SiS] Detected SiS301LV video bridge (ID 2; Revision 0x%x)\n",
		     temp1);
	    }
	} else {
	    sis_vbflags |= VB_302B;
	    //pSiS->sishw_ext.ujVBChipID = VB_CHIP_302B;
	    inSISIDXREG(SISPART4, 0x23, temp2);
	    if (!(temp & 0x02))
		sis_vbflags |= VB_30xBDH;
	    if (sis_verbose > 1) {
		printf
		    ("[SiS] Detected SiS302B%s video bridge (Revision 0x%x)\n",
		     (temp2 & 0x02) ? "" : " (DH)", temp1);
	    }
	}

	sis_sense_30x();

    } else if (temp == 3) {
	if (sis_verbose > 1) {
	    printf("[SiS] Detected SiS303 video bridge - not supported\n");
	}
    } else {
	/* big scary mess of code to handle unknown or Chrontel LVDS */
	/* skipping it for now */
	if (sis_verbose > 1) {
	    printf
		("[SiS] Detected Chrontel video bridge - not supported\n");
	}
    }

    /* this is probably not relevant to video overlay driver... */
    /* detects if brdige uses LCDA for low res text modes */
    if (sis_vga_engine == SIS_315_VGA) {
	if (sis_vbflags & (VB_302B | VB_301LV | VB_302LV)) {
		inSISIDXREG(SISCR, 0x34, temp);
		if (temp <= 0x13) {
		    inSISIDXREG(SISCR, 0x38, temp);
		    if ((temp & 0x03) == 0x03) {
			//pSiS->SiS_Pr->SiS_UseLCDA = TRUE;
			sis_vbflags |= VB_USELCDA;
		    } else {
			inSISIDXREG(SISCR, 0x30, temp);
			if (temp & 0x20) {
			    inSISIDXREG(SISPART1, 0x13, temp);
			    if (temp & 0x40) {
				//pSiS->SiS_Pr->SiS_UseLCDA = TRUE;
				sis_vbflags |= VB_USELCDA;
			    }
			}
		    }
		}
	    if (sis_vbflags & VB_USELCDA) {
		/* printf("Bridge uses LCDA for low resolution and text modes\n"); */
	    }
	}
    }


}


/* detect video bridge type and sense connected devices */
void sis_init_video_bridge(void)
{

    sis_detect_video_bridge();

    sis_detect_crt1();
    //sis_detect_lcd();
    sis_detect_tv();
    sis_detect_crt2();

    sis_detected_crt2_devices =
	sis_vbflags & (CRT2_LCD | CRT2_TV | CRT2_VGA);

    // force crt2 type
    if (sis_force_crt2_type == CRT2_DEFAULT) {
	if (sis_vbflags & CRT2_VGA)
	    sis_force_crt2_type = CRT2_VGA;
	else if (sis_vbflags & CRT2_LCD)
	    sis_force_crt2_type = CRT2_LCD;
	else if (sis_vbflags & CRT2_TV)
	    sis_force_crt2_type = CRT2_TV;
    }

    switch (sis_force_crt2_type) {
    case CRT2_TV:
	sis_vbflags = sis_vbflags & ~(CRT2_LCD | CRT2_VGA);
	if (sis_vbflags & VB_VIDEOBRIDGE)
	    sis_vbflags = sis_vbflags | CRT2_TV;
	else
	    sis_vbflags = sis_vbflags & ~(CRT2_TV);
	break;
    case CRT2_LCD:
	sis_vbflags = sis_vbflags & ~(CRT2_TV | CRT2_VGA);
	if ((sis_vbflags & VB_VIDEOBRIDGE) /* XXX: && (pSiS->VBLCDFlags) */
	    )
	    sis_vbflags = sis_vbflags | CRT2_LCD;
	else {
	    sis_vbflags = sis_vbflags & ~(CRT2_LCD);
	    if (sis_verbose > 0) {
		printf
		    ("[SiS] Can't force CRT2 to LCD, no panel detected\n");
	    }
	}
	break;
    case CRT2_VGA:
	if (sis_vbflags & VB_LVDS) {
	    if (sis_verbose > 0) {
		printf("[SiS] LVDS does not support secondary VGA\n");
	    }
	    break;
	}
	if (sis_vbflags & (VB_301LV | VB_302LV)) {
	    if (sis_verbose > 0) {
		printf
		    ("[SiS] SiS30xLV bridge does not support secondary VGA\n");
	    }
	    break;
	}
	sis_vbflags = sis_vbflags & ~(CRT2_TV | CRT2_LCD);
	if (sis_vbflags & VB_VIDEOBRIDGE)
	    sis_vbflags = sis_vbflags | CRT2_VGA;
	else
	    sis_vbflags = sis_vbflags & ~(CRT2_VGA);
	break;
    default:
	sis_vbflags &= ~(CRT2_TV | CRT2_LCD | CRT2_VGA);
    }

    /* CRT2 gamma correction?? */

    /* other force modes: */
    /*  have a 'force tv type' (svideo, composite, scart) option? */
    /*  have a 'force crt1 type' (to turn it off, etc??) */

    /* TW: Check if CRT1 used (or needed; this eg. if no CRT2 detected) */
    if (sis_vbflags & VB_VIDEOBRIDGE) {

	/* TW: No CRT2 output? Then we NEED CRT1!
	 *     We also need CRT1 if depth = 8 and bridge=LVDS|630+301B
	 */
	if ((!(sis_vbflags & (CRT2_VGA | CRT2_LCD | CRT2_TV))) || (	/*(pScrn->bitsPerPixel == 8) && */
								      ((sis_vbflags & (VB_LVDS | VB_CHRONTEL)) || ((sis_vga_engine == SIS_300_VGA) && (sis_vbflags & VB_301B))))) {
	    sis_crt1_off = 0;
	}
	/* TW: No CRT2 output? Then we can't use hw overlay on CRT2 */
	if (!(sis_vbflags & (CRT2_VGA | CRT2_LCD | CRT2_TV)))
	    sis_overlay_on_crt1 = 1;

    } else {			/* TW: no video bridge? */

	/* Then we NEED CRT1... */
	sis_crt1_off = 0;
	/* ... and can't use CRT2 for overlay output */
	sis_overlay_on_crt1 = 1;
    }

    /* tvstandard options ? */

    // determine using CRT1 or CRT2?
    /* -> NO dualhead right now... */
    if (sis_vbflags & DISPTYPE_DISP2) {
	if (sis_crt1_off) {
	    sis_vbflags |= VB_DISPMODE_SINGLE;
	    /* TW: No CRT1? Then we use the video overlay on CRT2 */
	    sis_overlay_on_crt1 = 0;
	} else			/* TW: CRT1 and CRT2 - mirror or dual head ----- */
	    sis_vbflags |= (VB_DISPMODE_MIRROR | DISPTYPE_CRT1);
    } else {			/* TW: CRT1 only ------------------------------- */
	sis_vbflags |= (VB_DISPMODE_SINGLE | DISPTYPE_CRT1);
    }

    if (sis_verbose > 0) {
	printf("[SiS] Using hardware overlay on CRT%d\n",
	       sis_overlay_on_crt1 ? 1 : 2);
    }

}
