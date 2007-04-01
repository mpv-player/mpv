/*
 * Copyright 1992-2000 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Alan Hourihane, alanh@fairlite.demon.co.uk
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_regs.h,v 1.22 2002/01/11 13:06:30 alanh Exp $ */

#define DEBUG 1

#define NTSC 14.31818
#define PAL  17.73448

/* General Registers */
#define SPR	0x1F		/* Software Programming Register (videoram) */

/* 3C4 */
#define RevisionID 0x09
#define ConfPort1 0x0C
#define ConfPort2 0x0C
#define NewMode2 0x0D
#define OldMode2 0x00 /* Should be 0x0D - dealt with in trident_dac.c */
#define OldMode1 0x0E
#define NewMode1 0x0E
#define Protection 0x11
#define MCLKLow 0x16
#define MCLKHigh 0x17
#define ClockLow 0x18
#define ClockHigh 0x19
#define SSetup 0x20
#define SKey 0x37
#define SPKey 0x57

/* 3x4 */
#define Offset 0x13
#define Underline 0x14
#define CRTCMode 0x17
#define CRTCModuleTest 0x1E
#define FIFOControl 0x20
#define LinearAddReg 0x21
#define DRAMTiming 0x23
#define New32 0x23
#define RAMDACTiming 0x25
#define CRTHiOrd 0x27
#define AddColReg 0x29
#define InterfaceSel 0x2A
#define HorizOverflow 0x2B
#define GETest 0x2D
#define Performance 0x2F
#define GraphEngReg 0x36
#define I2C 0x37
#define PixelBusReg 0x38
#define PCIReg 0x39
#define DRAMControl 0x3A
#define MiscContReg 0x3C
#define CursorXLow 0x40
#define CursorXHigh 0x41
#define CursorYLow 0x42
#define CursorYHigh 0x43
#define CursorLocLow 0x44
#define CursorLocHigh 0x45
#define CursorXOffset 0x46
#define CursorYOffset 0x47
#define CursorFG1 0x48
#define CursorFG2 0x49
#define CursorFG3 0x4A
#define CursorFG4 0x4B
#define CursorBG1 0x4C
#define CursorBG2 0x4D
#define CursorBG3 0x4E
#define CursorBG4 0x4F
#define CursorControl 0x50
#define PCIRetry 0x55
#define PreEndControl 0x56
#define PreEndFetch 0x57
#define PCIMaster 0x60
#define Enhancement0 0x62
#define NewEDO 0x64

/* --- Additions by AMR for Vidix support --- */
#define VideoWin1_HScale 0x80
#define VideoWin1_VScale 0x82
#define VideoWin1_Start 0x86
#define VideoWin1_Stop 0x8a
#define Video_Flags 0x8e
#define VideoWin1_Y_BPR 0x90
#define VideoWin1_Y_Offset 0x92
#define Video_LineBufferThreshold 0x95
#define Video_LineBufferLevel 0x96
#define Video_Flags2 0x97
/* --- */

#define TVinterface 0xC0
#define TVMode 0xC1
#define ClockControl 0xCF


/* 3CE */
#define MiscExtFunc 0x0F
#define MiscIntContReg 0x2F
#define CyberControl 0x30
#define CyberEnhance 0x31
#define FPConfig     0x33
#define VertStretch  0x52
#define HorStretch   0x53
#define BiosMode     0x5c
#define BiosNewMode1 0x5a
#define BiosNewMode2 0x5c
#define BiosReg      0x5d

/* --- IO Macros by AMR --- */

#define CRINB(reg) (OUTPORT8(0x3d4,reg), INPORT8(0x3d5))
#define SRINB(reg) (OUTPORT8(0x3c4,reg), INPORT8(0x3c5))
#define CROUTB(reg,val) (OUTPORT8(0x3d4,reg), OUTPORT8(0x3d5,val))
#define SROUTB(reg,val) (OUTPORT8(0x3c4,reg), OUTPORT8(0x3c5,val))

/* --- */

