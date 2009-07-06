/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/glint_regs.h,v 1.31 2001/12/08 16:01:52 alanh Exp $ */

/*
 * glint register file
 *
 * Copyright by Stefan Dirsch, Dirk Hohndel, Alan Hourihane
 * Authors: Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *          Dirk Hohndel, <hohndel@suse.de>
 *          Stefan Dirsch, <sndirsch@suse.de>
 *          Simon P., <sim@suse.de>
 *
 * this work is sponsored by S.u.S.E. GmbH, Fuerth, Elsa GmbH, Aachen and
 * Siemens Nixdorf Informationssysteme
 *
 */

#ifndef MPLAYER_GLINT_REGS_H
#define MPLAYER_GLINT_REGS_H

/**********************************************
*  GLINT 500TX Configuration Region Registers *
***********************************************/

/* Device Identification */
#define CFGVendorId						0x0000
#define PCI_VENDOR_3DLABS					0x3D3D
#define PCI_VENDOR_TI						0x104C
#define CFGDeviceId						0x0002

#define CFGRevisionId						0x08
#define CFGClassCode						0x09
#define CFGHeaderType						0x0E

/* Device Control/Status */
#define CFGCommand							0x04
#define CFGStatus							0x06

/* Miscellaneous Functions */
#define CFGBist								0x0f
#define CFGLatTimer							0x0d
#define CFGCacheLine							0x0c
#define CFGMaxLat							0x3f
#define CFGMinGrant							0x3e
#define CFGIntPin							0x3d
#define CFGIntLine							0x3c

/* Base Adresses */
#define CFGBaseAddr0							0x10
#define CFGBaseAddr1							0x14
#define CFGBaseAddr2							0x18
#define CFGBaseAddr3							0x1C
#define CFGBaseAddr4							0x20
#define CFGRomAddr							0x30



/**********************************
 * GLINT 500TX Region 0 Registers *
 **********************************/

/* Control Status Registers */
#define ResetStatus							0x0000
#define IntEnable							0x0008
#define IntFlags							0x0010
#define InFIFOSpace							0x0018
#define OutFIFOWords							0x0020
#define DMAAddress							0x0028
#define DMACount							0x0030
#define ErrorFlags							0x0038
#define VClkCtl								0x0040
#define TestRegister							0x0048
#define Aperture0							0x0050
#define Aperture1							0x0058
#define DMAControl							0x0060
#define FIFODis								0x0068

/* GLINT PerMedia Region 0 additional Registers */
#define ChipConfig							0x0070
#define   SCLK_SEL_MASK		(3 << 10)
#define   SCLK_SEL_MCLK_HALF	(3 << 10)
#define ByDMAControl							0x00D8

/* GLINT 500TX LocalBuffer Registers */
#define LBMemoryCtl							0x1000
#define   LBNumBanksMask	0x00000001
#define    LBNumBanks1		(0)
#define    LBNumBanks2		(1)
#define   LBPageSizeMask        0x00000006
#define    LBPageSize256	(0<<1)
#define    LBPageSize512	(1<<1)
#define    LBPageSize1024	(2<<1)
#define    LBPageSize2048	(3<<1)
#define   LBRASCASLowMask	0x00000018
#define    LBRASCASLow2		(0<<3)
#define    LBRASCASLow3		(1<<3)
#define    LBRASCASLow4		(2<<3)
#define    LBRASCASLow5		(3<<3)
#define   LBRASPrechargeMask	0x00000060
#define    LBRASPrecharge2	(0<<5)
#define    LBRASPrecharge3	(1<<5)
#define    LBRASPrecharge4	(2<<5)
#define    LBRASPrecharge5	(3<<5)
#define   LBCASLowMask		0x00000180
#define    LBCASLow1		(0<<7)
#define    LBCASLow2		(1<<7)
#define    LBCASLow3		(2<<7)
#define    LBCASLow4		(3<<7)
#define   LBPageModeMask	0x00000200
#define    LBPageModeEnabled	(0<<9)
#define    LBPageModeDisabled	(1<<9)
#define   LBRefreshCountMask    0x0003fc00
#define   LBRefreshCountShift   10

#define LBMemoryEDO							0x1008
#define   LBEDOMask		0x00000001
#define    LBEDODisabled	(0)
#define    LBEDOEnabled		(1)
#define   LBEDOBankSizeMask	0x0000000e
#define    LBEDOBankSizeDiabled	(0<<1)
#define    LBEDOBankSize256K	(1<<1)
#define    LBEDOBankSize512K	(2<<1)
#define    LBEDOBankSize1M	(3<<1)
#define    LBEDOBankSize2M	(4<<1)
#define    LBEDOBankSize4M	(5<<1)
#define    LBEDOBankSize8M	(6<<1)
#define    LBEDOBankSize16M	(7<<1)
#define   LBTwoPageDetectorMask	0x00000010
#define    LBSinglePageDetector	(0<<4)
#define    LBTwoPageDetector	(1<<4)

/* GLINT PerMedia Memory Control Registers */
#define PMReboot							0x1000
#define PMRomControl							0x1040
#define PMBootAddress							0x1080
#define PMMemConfig							0x10C0
    #define RowCharge8    1 << 10
    #define TimeRCD8      1 <<  7
    #define TimeRC8       0x6 << 3
    #define TimeRP8       1
    #define CAS3Latency8  0 << 16
    #define BootAdress8   0x10
    #define NumberBanks8  0x3 << 29
    #define RefreshCount8 0x41 << 21
    #define TimeRASMin8   1 << 13
    #define DeadCycle8    1 << 17
    #define BankDelay8    0 << 18
    #define Burst1Cycle8  1 << 31
    #define SDRAM8        0 << 4

    #define RowCharge6    1 << 10
    #define TimeRCD6      1 <<  7
    #define TimeRC6       0x6 << 3
    #define TimeRP6       0x2
    #define CAS3Latency6  1 << 16
    #define BootAdress6   0x60
    #define NumberBanks6  0x2 << 29
    #define RefreshCount6 0x41 << 21
    #define TimeRASMin6   1 << 13
    #define DeadCycle6    1 << 17
    #define BankDelay6    0 << 18
    #define Burst1Cycle6  1 << 31
    #define SDRAM6        0 << 4

    #define RowCharge4    0 << 10
    #define TimeRCD4      0 <<  7
    #define TimeRC4       0x4 << 3
    #define TimeRP4       1
    #define CAS3Latency4  0 << 16
    #define BootAdress4   0x10
    #define NumberBanks4  1 << 29
    #define RefreshCount4 0x30 << 21
    #define TimeRASMin4   1 << 13
    #define DeadCycle4    0 << 17
    #define BankDelay4    0 << 18
    #define Burst1Cycle4  1 << 31
    #define SDRAM4        0 << 4

/* Permedia 2 Control */
#define MemControl							0x1040

#define PMBypassWriteMask						0x1100
#define PMFramebufferWriteMask					        0x1140
#define PMCount								0x1180

/* Framebuffer Registers */
#define FBMemoryCtl							0x1800
#define FBModeSel							0x1808
#define FBGCWrMask							0x1810
#define FBGCColorLower							0x1818
#define FBTXMemCtl							0x1820
#define FBWrMaskk							0x1830
#define FBGCColorUpper							0x1838

/* Core FIFO */
#define OutputFIFO							0x2000

/* 500TX Internal Video Registers */
#define VTGHLimit							0x3000
#define VTGHSyncStart							0x3008
#define VTGHSyncEnd							0x3010
#define VTGHBlankEnd							0x3018
#define VTGVLimit							0x3020
#define VTGVSyncStart							0x3028
#define VTGVSyncEnd							0x3030
#define VTGVBlankEnd							0x3038
#define VTGHGateStart							0x3040
#define VTGHGateEnd							0x3048
#define VTGVGateStart							0x3050
#define VTGVGateEnd							0x3058
#define VTGPolarity							0x3060
#define VTGFrameRowAddr							0x3068
#define VTGVLineNumber							0x3070
#define VTGSerialClk							0x3078
#define VTGModeCtl							0x3080

/* Permedia Video Control Registers */
#define PMScreenBase							0x3000
#define PMScreenStride							0x3008
#define PMHTotal							0x3010
#define PMHgEnd								0x3018
#define PMHbEnd								0x3020
#define PMHsStart							0x3028
#define PMHsEnd								0x3030
#define PMVTotal							0x3038
#define PMVbEnd								0x3040
#define PMVsStart							0x3048
#define PMVsEnd								0x3050
#define PMVideoControl							0x3058
#define PMInterruptLine							0x3060
#define PMDDCData							0x3068
#define   DataIn             						(1<<0)
#define   ClkIn              						(1<<1)
#define   DataOut            						(1<<2)
#define   ClkOut             						(1<<3)
#define PMLineCount							0x3070
#define PMFifoControl							0x3078

/* Permedia 2 RAMDAC Registers */
#define PM2DACWriteAddress						0x4000
#define PM2DACIndexReg							0x4000
#define PM2DACData							0x4008
#define PM2DACReadMask							0x4010
#define PM2DACReadAddress						0x4018
#define PM2DACCursorColorAddress				        0x4020
#define PM2DACCursorColorData					        0x4028
#define PM2DACIndexData							0x4050
#define PM2DACCursorData						0x4058
#define PM2DACCursorXLsb						0x4060
#define PM2DACCursorXMsb						0x4068
#define PM2DACCursorYLsb						0x4070
#define PM2DACCursorYMsb						0x4078
#define PM2DACCursorControl						0x06
#define PM2DACIndexCMR							0x18
#define   PM2DAC_TRUECOLOR				0x80
#define   PM2DAC_RGB					0x20
#define   PM2DAC_GRAPHICS				0x10
#define   PM2DAC_PACKED					0x09
#define   PM2DAC_8888					0x08
#define   PM2DAC_565					0x06
#define   PM2DAC_4444					0x05
#define   PM2DAC_5551					0x04
#define   PM2DAC_2321					0x03
#define   PM2DAC_2320					0x02
#define   PM2DAC_332					0x01
#define   PM2DAC_CI8					0x00
#define PM2DACIndexMDCR							0x19
#define PM2DACIndexPalettePage					        0x1c
#define PM2DACIndexMCR							0x1e
#define PM2DACIndexClockAM						0x20
#define PM2DACIndexClockAN						0x21
#define PM2DACIndexClockAP						0x22
#define PM2DACIndexClockBM						0x23
#define PM2DACIndexClockBN						0x24
#define PM2DACIndexClockBP						0x25
#define PM2DACIndexClockCM						0x26
#define PM2DACIndexClockCN						0x27
#define PM2DACIndexClockCP						0x28
#define PM2DACIndexClockStatus						0x29
#define PM2DACIndexMemClockM						0x30
#define PM2DACIndexMemClockN						0x31
#define PM2DACIndexMemClockP						0x32
#define PM2DACIndexMemClockStatus					0x33
#define PM2DACIndexColorKeyControl					0x40
#define PM2DACIndexColorKeyOverlay					0x41
#define PM2DACIndexColorKeyRed						0x42
#define PM2DACIndexColorKeyGreen					0x43
#define PM2DACIndexColorKeyBlue						0x44

/* Permedia 2V extensions */
#define PM2VDACRDMiscControl						0x000
#define PM2VDACRDSyncControl						0x001
#define PM2VDACRDDACControl						0x002
#define PM2VDACRDPixelSize						0x003
#define PM2VDACRDColorFormat						0x004
#define PM2VDACRDCursorMode						0x005
#define PM2VDACRDCursorXLow						0x007
#define PM2VDACRDCursorXHigh						0x008
#define PM2VDACRDCursorYLow						0x009
#define PM2VDACRDCursorYHigh						0x00A
#define PM2VDACRDCursorHotSpotX						0x00B
#define PM2VDACRDCursorHotSpotY						0x00C
#define PM2VDACRDOverlayKey						0x00D
#define PM2VDACRDPan							0x00E
#define PM2VDACRDSense							0x00F
#define PM2VDACRDCheckControl						0x018
#define PM2VDACIndexClockControl					0x200
#define PM2VDACRDDClk0PreScale						0x201
#define PM2VDACRDDClk0FeedbackScale					0x202
#define PM2VDACRDDClk0PostScale						0x203
#define PM2VDACRDDClk1PreScale						0x204
#define PM2VDACRDDClk1FeedbackScale					0x205
#define PM2VDACRDDClk1PostScale						0x206
#define PM2VDACRDMClkControl						0x20D
#define PM2VDACRDMClkPreScale						0x20E
#define PM2VDACRDMClkFeedbackScale					0x20F
#define PM2VDACRDMClkPostScale						0x210
#define PM2VDACRDCursorPalette						0x303
#define PM2VDACRDCursorPattern						0x400
#define PM2VDACIndexRegLow						0x4020
#define PM2VDACIndexRegHigh						0x4028
#define PM2VDACIndexData						0x4030
#define PM2VDACRDIndexControl						0x4038

/* Permedia 2 Video Streams Unit Registers */
#define   VSBIntFlag            					(1<<8)
#define   VSAIntFlag            					(1<<9)

#define VSConfiguration							0x5800
#define   VS_UnitMode_ROM						0
#define   VS_UnitMode_AB8						3
#define   VS_UnitMode_Mask						7
#define   VS_GPBusMode_A        					(1<<3)
#define   VS_HRefPolarityA      					(1<<9)
#define   VS_VRefPolarityA      					(1<<10)
#define   VS_VActivePolarityA   					(1<<11)
#define   VS_UseFieldA          					(1<<12)
#define   VS_FieldPolarityA						(1<<13)
#define   VS_FieldEdgeA         					(1<<14)
#define   VS_VActiveVBIA						(1<<15)
#define   VS_InterlaceA         					(1<<16)
#define   VS_ReverseDataA       					(1<<17)
#define   VS_HRefPolarityB      					(1<<18)
#define   VS_VRefPolarityB      					(1<<19)
#define   VS_VActivePolarityB   					(1<<20)
#define   VS_UseFieldB							(1<<21)
#define   VS_FieldPolarityB						(1<<22)
#define   VS_FieldEdgeB							(1<<23)
#define   VS_VActiveVBIB						(1<<24)
#define   VS_InterlaceB							(1<<25)
#define   VS_ColorSpaceB_RGB						(1<<26)
#define   VS_ReverseDataB						(1<<27)
#define   VS_DoubleEdgeB						(1<<28)

#define VSStatus							0x5808
#define   VS_FieldOne0A							(1<<9)
#define   VS_FieldOne1A							(1<<10)
#define   VS_FieldOne2A							(1<<11)
#define   VS_InvalidInterlaceA						(1<<12)
#define   VS_FieldOne0B							(1<<17)
#define   VS_FieldOne1B							(1<<18)
#define   VS_FieldOne2B							(1<<19)
#define   VS_InvalidInterlaceB						(1<<20)

#define VSSerialBusControl						0x5810

#define VSABase          						0x5900
#define   VSA_Video             					(1<<0)
#define   VSA_VBI               					(1<<1)
#define   VSA_BufferCtl         					(1<<2)
#define   VSA_MirrorX           					(1<<7)
#define   VSA_MirrorY           					(1<<8)
#define   VSA_Discard_None      					(0<<9)
#define   VSA_Discard_FieldOne  					(1<<9)
#define   VSA_Discard_FieldTwo  					(2<<9)
#define   VSA_CombineFields     					(1<<11)
#define   VSA_LockToStreamB     					(1<<12)
#define VSBBase								0x5A00
#define   VSB_Video             					(1<<0)
#define   VSB_VBI               					(1<<1)
#define   VSB_BufferCtl         					(1<<2)
#define   VSB_CombineFields     					(1<<3)
#define   VSB_RGBOrder          					(1<<11)
#define   VSB_GammaCorrect      					(1<<12)
#define   VSB_LockToStreamA     					(1<<13)

#define VSControl							0x0000
#define VSInterrupt            						0x0008
#define VSCurrentLine          						0x0010
#define VSVideoAddressHost     						0x0018
#define VSVideoAddressIndex    						0x0020
#define VSVideoAddress0        						0x0028
#define VSVideoAddress1        						0x0030
#define VSVideoAddress2        						0x0038
#define VSVideoStride          						0x0040
#define VSVideoStartLine       						0x0048
#define VSVideoEndLine     						0x0050
#define VSVideoStartData       						0x0058
#define VSVideoEndData         						0x0060
#define VSVBIAddressHost       						0x0068
#define VSVBIAddressIndex      						0x0070
#define VSVBIAddress0          						0x0078
#define VSVBIAddress1          						0x0080
#define VSVBIAddress2          						0x0088
#define VSVBIStride            						0x0090
#define VSVBIStartLine         						0x0098
#define VSVBIEndLine           						0x00A0
#define VSVBIStartData         						0x00A8
#define VSVBIEndData           						0x00B0
#define VSFifoControl          						0x00B8

/**********************************
 * GLINT Delta Region 0 Registers *
 **********************************/

/* Control Status Registers */
#define DResetStatus							0x0800
#define DIntEnable							0x0808
#define DIntFlags							0x0810
#define DErrorFlags							0x0838
#define DTestRegister							0x0848
#define DFIFODis							0x0868



/**********************************
 * GLINT Gamma Region 0 Registers *
 **********************************/

/* Control Status Registers */
#define GInFIFOSpace							0x0018
#define GDMAAddress							0x0028
#define GDMACount							0x0030
#define GDMAControl							0x0060
#define GOutDMA								0x0080
#define GOutDMACount							0x0088
#define GResetStatus							0x0800
#define GIntEnable							0x0808
#define GIntFlags							0x0810
#define GErrorFlags							0x0838
#define GTestRegister							0x0848
#define GFIFODis							0x0868

#define GChipConfig							0x0870
#define   GChipAGPCapable		1 << 0
#define   GChipAGPSideband		1 << 1
#define   GChipMultiGLINTApMask		3 << 19
#define   GChipMultiGLINTAp_0M		0 << 19
#define   GChipMultiGLINTAp_16M		1 << 19
#define   GChipMultiGLINTAp_32M		2 << 19
#define   GChipMultiGLINTAp_64M		3 << 19

#define GCSRAperture							0x0878
#define   GCSRSecondaryGLINTMapEn	1 << 0

#define GPageTableAddr							0x0c00
#define GPageTableLength						0x0c08
#define GDelayTimer							0x0c38
#define GCommandMode							0x0c40
#define GCommandIntEnable						0x0c48
#define GCommandIntFlags						0x0c50
#define GCommandErrorFlags						0x0c58
#define GCommandStatus							0x0c60
#define GCommandFaultingAddr						0x0c68
#define GVertexFaultingAddr						0x0c70
#define GWriteFaultingAddr						0x0c88
#define GFeedbackSelectCount						0x0c98
#define GGammaProcessorMode						0x0cb8
#define GVGAShadow							0x0d00
#define GMultGLINTAperture						0x0d08
#define GMultGLINT1							0x0d10
#define GMultGLINT2							0x0d18

/************************
 * GLINT Core Registers *
 ************************/

#define GLINT_TAG(major,offset)		(((major) << 7) | ((offset) << 3))
#define GLINT_TAG_ADDR(major,offset)	(0x8000 | GLINT_TAG((major),(offset)))

#define UNIT_DISABLE							0
#define UNIT_ENABLE							1

#define StartXDom				GLINT_TAG_ADDR(0x00,0x00)
#define dXDom					GLINT_TAG_ADDR(0x00,0x01)
#define StartXSub				GLINT_TAG_ADDR(0x00,0x02)
#define dXSub					GLINT_TAG_ADDR(0x00,0x03)
#define StartY					GLINT_TAG_ADDR(0x00,0x04)
#define dY					GLINT_TAG_ADDR(0x00,0x05)
#define GLINTCount				GLINT_TAG_ADDR(0x00,0x06)
#define Render					GLINT_TAG_ADDR(0x00,0x07)
	#define AreaStippleEnable		0x00001
	#define LineStippleEnable		0x00002
	#define ResetLineStipple		0x00004
	#define FastFillEnable			0x00008
	#define PrimitiveLine			0
	#define PrimitiveTrapezoid		0x00040
	#define PrimitivePoint			0x00080
	#define PrimitiveRectangle		0x000C0
	#define AntialiasEnable         	0x00100
	#define AntialiasingQuality     	0x00200
	#define UsePointTable			0x00400
	#define SyncOnBitMask			0x00800
	#define SyncOnHostData			0x01000
	#define TextureEnable           	0x02000
	#define FogEnable               	0x04000
	#define CoverageEnable			0x08000
	#define SubPixelCorrectionEnable	0x10000
	#define SpanOperation			0x40000
	#define XPositive			1<<21
	#define YPositive			1<<22


#define ContinueNewLine				GLINT_TAG_ADDR(0x00,0x08)
#define ContinueNewDom				GLINT_TAG_ADDR(0x00,0x09)
#define ContinueNewSub				GLINT_TAG_ADDR(0x00,0x0a)
#define Continue				GLINT_TAG_ADDR(0x00,0x0b)
#define FlushSpan				GLINT_TAG_ADDR(0x00,0x0c)
#define BitMaskPattern				GLINT_TAG_ADDR(0x00,0x0d)

#define PointTable0				GLINT_TAG_ADDR(0x01,0x00)
#define PointTable1				GLINT_TAG_ADDR(0x01,0x01)
#define PointTable2				GLINT_TAG_ADDR(0x01,0x02)
#define PointTable3				GLINT_TAG_ADDR(0x01,0x03)
#define RasterizerMode				GLINT_TAG_ADDR(0x01,0x04)
#define		RMMultiGLINT			1<<17
#define		BitMaskPackingEachScanline	1<<9
#define		ForceBackgroundColor		1<<6
#define		InvertBitMask			1<<1
#define YLimits					GLINT_TAG_ADDR(0x01,0x05)
#define ScanLineOwnership			GLINT_TAG_ADDR(0x01,0x06)
#define WaitForCompletion			GLINT_TAG_ADDR(0x01,0x07)
#define PixelSize				GLINT_TAG_ADDR(0x01,0x08)
#define XLimits					GLINT_TAG_ADDR(0x01,0x09) /* PM only */

#define RectangleOrigin				GLINT_TAG_ADDR(0x01,0x0A) /* PM2 only */
#define RectangleSize				GLINT_TAG_ADDR(0x01,0x0B) /* PM2 only */

#define PackedDataLimits			GLINT_TAG_ADDR(0x02,0x0a) /* PM only */

#define ScissorMode				GLINT_TAG_ADDR(0x03,0x00)
        #define                                 SCI_USER          0x01
        #define                                 SCI_SCREEN        0x02
        #define                                 SCI_USERANDSCREEN 0x03

#define ScissorMinXY				GLINT_TAG_ADDR(0x03,0x01)
#define ScissorMaxXY				GLINT_TAG_ADDR(0x03,0x02)
#define ScreenSize				GLINT_TAG_ADDR(0x03,0x03)
#define AreaStippleMode				GLINT_TAG_ADDR(0x03,0x04)
	/* 0:				*/
	/* NoMirrorY			*/
	/* NoMirrorX			*/
	/* NoInvertPattern		*/
	/* YAddress_1bit		*/
	/* XAddress_1bit		*/
	/* UNIT_DISABLE			*/

	#define ASM_XAddress_2bit					1 << 1
	#define ASM_XAddress_3bit					2 << 1
	#define ASM_XAddress_4bit					3 << 1
	#define ASM_XAddress_5bit					4 << 1
	#define ASM_YAddress_2bit					1 << 4
	#define ASM_YAddress_3bit					2 << 4
	#define ASM_YAddress_4bit					3 << 4
	#define ASM_YAddress_5bit					4 << 4
	#define ASM_InvertPattern					1 << 17
	#define ASM_MirrorX						1 << 18
	#define ASM_MirrorY						1 << 19

#define LineStippleMode				GLINT_TAG_ADDR(0x03,0x05)
#define LoadLineStippleCounters			GLINT_TAG_ADDR(0x03,0x06)
#define UpdateLineStippleCounters		GLINT_TAG_ADDR(0x03,0x07)
#define SaveLineStippleState			GLINT_TAG_ADDR(0x03,0x08)
#define WindowOrigin				GLINT_TAG_ADDR(0x03,0x09)

#define AreaStipplePattern0			GLINT_TAG_ADDR(0x04,0x00)
#define AreaStipplePattern1			GLINT_TAG_ADDR(0x04,0x01)
#define AreaStipplePattern2			GLINT_TAG_ADDR(0x04,0x02)
#define AreaStipplePattern3			GLINT_TAG_ADDR(0x04,0x03)
#define AreaStipplePattern4			GLINT_TAG_ADDR(0x04,0x04)
#define AreaStipplePattern5			GLINT_TAG_ADDR(0x04,0x05)
#define AreaStipplePattern6			GLINT_TAG_ADDR(0x04,0x06)
#define AreaStipplePattern7			GLINT_TAG_ADDR(0x04,0x07)

#define TextureAddressMode			GLINT_TAG_ADDR(0x07,0x00)
#define SStart					GLINT_TAG_ADDR(0x07,0x01)
#define dSdx					GLINT_TAG_ADDR(0x07,0x02)
#define dSdyDom					GLINT_TAG_ADDR(0x07,0x03)
#define TStart					GLINT_TAG_ADDR(0x07,0x04)
#define dTdx					GLINT_TAG_ADDR(0x07,0x05)
#define dTdyDom					GLINT_TAG_ADDR(0x07,0x06)
#define QStart					GLINT_TAG_ADDR(0x07,0x07)
#define dQdx					GLINT_TAG_ADDR(0x07,0x08)
#define dQdyDom					GLINT_TAG_ADDR(0x07,0x09)
#define LOD					GLINT_TAG_ADDR(0x07,0x0A)
#define dSdy					GLINT_TAG_ADDR(0x07,0x0B)
#define dTdy					GLINT_TAG_ADDR(0x07,0x0C)
#define dQdy					GLINT_TAG_ADDR(0x07,0x0D)

#define TextureReadMode				GLINT_TAG_ADDR(0x09,0x00)
#define TextureFormat				GLINT_TAG_ADDR(0x09,0x01)
  #define Texture_4_Components 3 << 3
  #define Texture_Texel        0

#define TextureCacheControl			GLINT_TAG_ADDR(0x09,0x02)
  #define TextureCacheControlEnable     2
  #define TextureCacheControlInvalidate 1

#define GLINTBorderColor			GLINT_TAG_ADDR(0x09,0x05)

#define TexelLUTIndex				GLINT_TAG_ADDR(0x09,0x08)
#define TexelLUTData				GLINT_TAG_ADDR(0x09,0x09)
#define TexelLUTAddress				GLINT_TAG_ADDR(0x09,0x0A)
#define TexelLUTTransfer			GLINT_TAG_ADDR(0x09,0x0B)
#define TextureFilterMode			GLINT_TAG_ADDR(0x09,0x0C)
#define TextureChromaUpper			GLINT_TAG_ADDR(0x09,0x0D)
#define TextureChromaLower			GLINT_TAG_ADDR(0x09,0x0E)

#define TxBaseAddr0				GLINT_TAG_ADDR(0x0A,0x00)
#define TxBaseAddr1				GLINT_TAG_ADDR(0x0A,0x01)
#define TxBaseAddr2				GLINT_TAG_ADDR(0x0A,0x02)
#define TxBaseAddr3				GLINT_TAG_ADDR(0x0A,0x03)
#define TxBaseAddr4				GLINT_TAG_ADDR(0x0A,0x04)
#define TxBaseAddr5				GLINT_TAG_ADDR(0x0A,0x05)
#define TxBaseAddr6				GLINT_TAG_ADDR(0x0A,0x06)
#define TxBaseAddr7				GLINT_TAG_ADDR(0x0A,0x07)
#define TxBaseAddr8				GLINT_TAG_ADDR(0x0A,0x08)
#define TxBaseAddr9				GLINT_TAG_ADDR(0x0A,0x09)
#define TxBaseAddr10				GLINT_TAG_ADDR(0x0A,0x0A)
#define TxBaseAddr11				GLINT_TAG_ADDR(0x0A,0x0B)

#define PMTextureBaseAddress			GLINT_TAG_ADDR(0x0b,0x00)
#define PMTextureMapFormat			GLINT_TAG_ADDR(0x0b,0x01)
#define PMTextureDataFormat			GLINT_TAG_ADDR(0x0b,0x02)

#define Texel0					GLINT_TAG_ADDR(0x0c,0x00)
#define Texel1					GLINT_TAG_ADDR(0x0c,0x01)
#define Texel2					GLINT_TAG_ADDR(0x0c,0x02)
#define Texel3					GLINT_TAG_ADDR(0x0c,0x03)
#define Texel4					GLINT_TAG_ADDR(0x0c,0x04)
#define Texel5					GLINT_TAG_ADDR(0x0c,0x05)
#define Texel6					GLINT_TAG_ADDR(0x0c,0x06)
#define Texel7					GLINT_TAG_ADDR(0x0c,0x07)
#define Interp0					GLINT_TAG_ADDR(0x0c,0x08)
#define Interp1					GLINT_TAG_ADDR(0x0c,0x09)
#define Interp2					GLINT_TAG_ADDR(0x0c,0x0a)
#define Interp3					GLINT_TAG_ADDR(0x0c,0x0b)
#define Interp4					GLINT_TAG_ADDR(0x0c,0x0c)
#define TextureFilter				GLINT_TAG_ADDR(0x0c,0x0d)
#define PMTextureReadMode			GLINT_TAG_ADDR(0x0c,0x0e)
#define TexelLUTMode				GLINT_TAG_ADDR(0x0c,0x0f)

#define TextureColorMode			GLINT_TAG_ADDR(0x0d,0x00)
  #define TextureTypeOpenGL 0
  #define TextureTypeApple  1 << 4
  #define TextureKsDDA      1 << 5 /* only Apple-Mode */
  #define TextureKdDDA      1 << 6 /* only Apple-Mode */

#define TextureEnvColor				GLINT_TAG_ADDR(0x0d,0x01)
#define FogMode					GLINT_TAG_ADDR(0x0d,0x02)
	/* 0:				*/
	/* FOG RGBA			*/
	/* UNIT_DISABLE			*/

	#define FOG_CI				0x0002

#define FogColor				GLINT_TAG_ADDR(0x0d,0x03)
#define FStart					GLINT_TAG_ADDR(0x0d,0x04)
#define dFdx					GLINT_TAG_ADDR(0x0d,0x05)
#define dFdyDom					GLINT_TAG_ADDR(0x0d,0x06)
#define KsStart					GLINT_TAG_ADDR(0x0d,0x09)
#define dKsdx					GLINT_TAG_ADDR(0x0d,0x0a)
#define dKsdyDom				GLINT_TAG_ADDR(0x0d,0x0b)
#define KdStart					GLINT_TAG_ADDR(0x0d,0x0c)
#define dKdStart				GLINT_TAG_ADDR(0x0d,0x0d)
#define dKddyDom				GLINT_TAG_ADDR(0x0d,0x0e)

#define RStart					GLINT_TAG_ADDR(0x0f,0x00)
#define dRdx					GLINT_TAG_ADDR(0x0f,0x01)
#define dRdyDom					GLINT_TAG_ADDR(0x0f,0x02)
#define GStart					GLINT_TAG_ADDR(0x0f,0x03)
#define dGdx					GLINT_TAG_ADDR(0x0f,0x04)
#define dGdyDom					GLINT_TAG_ADDR(0x0f,0x05)
#define BStart					GLINT_TAG_ADDR(0x0f,0x06)
#define dBdx					GLINT_TAG_ADDR(0x0f,0x07)
#define dBdyDom					GLINT_TAG_ADDR(0x0f,0x08)
#define AStart					GLINT_TAG_ADDR(0x0f,0x09)
#define dAdx					GLINT_TAG_ADDR(0x0f,0x0a)
#define dAdyDom					GLINT_TAG_ADDR(0x0f,0x0b)
#define ColorDDAMode				GLINT_TAG_ADDR(0x0f,0x0c)
	/* 0:					*/
	#define CDDA_FlatShading			                0
	/* UNIT_DISABLE			*/
	#define CDDA_GouraudShading					0x0002


#define ConstantColor				GLINT_TAG_ADDR(0x0f,0x0d)
#define GLINTColor				GLINT_TAG_ADDR(0x0f,0x0e)
#define AlphaTestMode				GLINT_TAG_ADDR(0x10,0x00)
#define AntialiasMode				GLINT_TAG_ADDR(0x10,0x01)
#define AlphaBlendMode				GLINT_TAG_ADDR(0x10,0x02)
	/* 0:					*/
	/* SrcZERO				*/
	/* DstZERO				*/
	/* ColorFormat8888			*/
	/* AlphaBuffer present			*/
	/* ColorOrderBGR			*/
	/* TypeOpenGL				*/
	/* DstFBData				*/
	/* UNIT_DISABLE				*/

	#define ABM_SrcONE					1 << 1
	#define ABM_SrcDST_COLOR				2 << 1
	#define ABM_SrcONE_MINUS_DST_COLOR			3 << 1
	#define ABM_SrcSRC_ALPHA				4 << 1
	#define ABM_SrcONE_MINUS_SRC_ALPHA			5 << 1
	#define ABM_SrcDST_ALPHA				6 << 1
	#define ABM_SrcONE_MINUS_DST_ALPHA			7 << 1
	#define ABM_SrcSRC_ALPHA_SATURATE			8 << 1
	#define ABM_DstONE					1 << 5
	#define ABM_DstSRC_COLOR				2 << 5
	#define ABM_DstONE_MINUS_SRC_COLOR			3 << 5
	#define ABM_DstSRC_ALPHA				4 << 5
	#define ABM_DstONE_MINUS_SRC_ALPHA			5 << 5
	#define ABM_DstDST_ALPHA				6 << 5
	#define ABM_DstONE_MINUS_DST_ALPHA			7 << 5
	#define ABM_ColorFormat5555				1 << 8
	#define ABM_ColorFormat4444				2 << 8
	#define ABM_ColorFormat4444_Front			3 << 8
	#define ABM_ColorFormat4444_Back			4 << 8
	#define ABM_ColorFormat332_Front			5 << 8
	#define ABM_ColorFormat332_Back				6 << 8
	#define ABM_ColorFormat121_Front			7 << 8
	#define ABM_ColorFormat121_Back				8 << 8
	#define ABM_ColorFormat555_Back				13 << 8
	#define ABM_ColorFormat_CI8				14 << 8
	#define ABM_ColorFormat_CI4				15 << 8
	#define ABM_NoAlphaBuffer				0x1000
	#define ABM_ColorOrderRGB				0x2000
	#define ABM_TypeQuickDraw3D				0x4000
	#define ABM_DstFBSourceData				0x8000

#define DitherMode				GLINT_TAG_ADDR(0x10,0x03)
	/* 0:					*/
	/* ColorOrder BGR		*/
	/* AlphaDitherDefault	*/
	/* ColorFormat8888		*/
	/* TruncateMode 		*/
	/* DitherDisable		*/
	/* UNIT_DISABLE			*/

	#define DTM_DitherEnable				1 << 1
	#define DTM_ColorFormat5555				1 << 2
	#define DTM_ColorFormat4444				2 << 2
	#define DTM_ColorFormat4444_Front			3 << 2
	#define DTM_ColorFormat4444_Back			4 << 2
	#define DTM_ColorFormat332_Front			5 << 2
	#define DTM_ColorFormat332_Back				6 << 2
	#define DTM_ColorFormat121_Front			7 << 2
	#define DTM_ColorFormat121_Back				8 << 2
	#define DTM_ColorFormat555_Back				13 << 2
	#define DTM_ColorFormat_CI8				14 << 2
	#define DTM_ColorFormat_CI4				15 << 2
	#define DTM_ColorOrderRGB				1 << 10
	#define DTM_NoAlphaDither				1 << 14
	#define DTM_RoundMode					1 << 15

#define FBSoftwareWriteMask			GLINT_TAG_ADDR(0x10,0x04)
#define LogicalOpMode				GLINT_TAG_ADDR(0x10,0x05)
        #define Use_ConstantFBWriteData 0x40


#define FBWriteData				GLINT_TAG_ADDR(0x10,0x06)
#define RouterMode				GLINT_TAG_ADDR(0x10,0x08)
        #define ROUTER_Depth_Texture 1
        #define ROUTER_Texture_Depth 0


#define LBReadMode				GLINT_TAG_ADDR(0x11,0x00)
	/* 0:				*/
	/* SrcNoRead			*/
	/* DstNoRead			*/
	/* DataLBDefault		*/
	/* WinTopLeft			*/
	/* NoPatch			*/
	/* ScanlineInterval1 		*/

	#define LBRM_SrcEnable				1 << 9
	#define LBRM_DstEnable				1 << 10
	#define LBRM_DataLBStencil			1 << 16
	#define LBRM_DataLBDepth			2 << 16
	#define LBRM_WinBottomLeft			1 << 18
	#define LBRM_DoPatch				1 << 19

	#define LBRM_ScanlineInt2			1 << 20
	#define LBRM_ScanlineInt4			2 << 20
	#define LBRM_ScanlineInt8			3 << 20


#define LBReadFormat				GLINT_TAG_ADDR(0x11,0x01)
        #define LBRF_DepthWidth15   0x03  /* only permedia */
        #define LBRF_DepthWidth16   0x00
        #define LBRF_DepthWidth24   0x01
        #define LBRF_DepthWidth32   0x02

        #define LBRF_StencilWidth0  (0 << 2)
        #define LBRF_StencilWidth4  (1 << 2)
        #define LBRF_StencilWidth8  (2 << 2)

        #define LBRF_StencilPos16   (0 << 4)
        #define LBRF_StencilPos20   (1 << 4)
        #define LBRF_StencilPos24   (2 << 4)
        #define LBRF_StencilPos28   (3 << 4)
        #define LBRF_StencilPos32   (4 << 4)

        #define LBRF_FrameCount0    (0 << 7)
        #define LBRF_FrameCount4    (1 << 7)
        #define LBRF_FrameCount8    (2 << 7)

        #define LBRF_FrameCountPos16  (0 << 9)
        #define LBRF_FrameCountPos20  (1 << 9)
        #define LBRF_FrameCountPos24  (2 << 9)
        #define LBRF_FrameCountPos28  (3 << 9)
        #define LBRF_FrameCountPos32  (4 << 9)
        #define LBRF_FrameCountPos36  (5 << 9)
        #define LBRF_FrameCountPos40  (6 << 9)

        #define LBRF_GIDWidth0 (0 << 12)
        #define LBRF_GIDWidth4 (1 << 12)

        #define LBRF_GIDPos16  (0 << 13)
        #define LBRF_GIDPos20  (1 << 13)
        #define LBRF_GIDPos24  (2 << 13)
        #define LBRF_GIDPos28  (3 << 13)
        #define LBRF_GIDPos32  (4 << 13)
        #define LBRF_GIDPos36  (5 << 13)
        #define LBRF_GIDPos40  (6 << 13)
        #define LBRF_GIDPos44  (7 << 13)
        #define LBRF_GIDPos48  (8 << 13)

        #define LBRF_Compact32  (1 << 17)



#define LBSourceOffset				GLINT_TAG_ADDR(0x11,0x02)
#define LBStencil				GLINT_TAG_ADDR(0x11,0x05)
#define LBDepth					GLINT_TAG_ADDR(0x11,0x06)
#define LBWindowBase				GLINT_TAG_ADDR(0x11,0x07)
#define LBWriteMode				GLINT_TAG_ADDR(0x11,0x08)
	#define LBWM_WriteEnable		0x1
	#define LBWM_UpLoad_LBDepth		0x2
	#define LBWM_UpLoad_LBStencil		0x4

#define LBWriteFormat				GLINT_TAG_ADDR(0x11,0x09)


#define TextureData				GLINT_TAG_ADDR(0x11,0x0d)
#define TextureDownloadOffset			GLINT_TAG_ADDR(0x11,0x0e)
#define LBWindowOffset				GLINT_TAG_ADDR(0x11,0x0f)

#define GLINTWindow				GLINT_TAG_ADDR(0x13,0x00)
        #define GWIN_UnitEnable          (1 << 0)
        #define GWIN_ForceLBUpdate       (1 << 3)
        #define GWIN_LBUpdateSourceREG   (1 << 4)
        #define GWIN_LBUpdateSourceLB    (0 << 4)
        #define GWIN_StencilFCP          (1 << 17)
        #define GWIN_DepthFCP            (1 << 18)
        #define GWIN_OverrideWriteFilter (1 << 19)

	/* ??? is this needed, set by permedia (2) modules */
        #define GWIN_DisableLBUpdate    0x40000

#define StencilMode				GLINT_TAG_ADDR(0x13,0x01)
#define StencilData				GLINT_TAG_ADDR(0x13,0x02)
#define GLINTStencil				GLINT_TAG_ADDR(0x13,0x03)
#define DepthMode				GLINT_TAG_ADDR(0x13,0x04)
	/* 0:				*/
	/* WriteDisable			*/
	/* SrcCompFragment		*/
	/* CompFuncNEVER		*/
	/* UNIT_DISABLE			*/

	#define DPM_WriteEnable			1 << 1
	#define DPM_SrcCompLBData		1 << 2
	#define DPM_SrcCompDregister		2 << 2
	#define DPM_SrcCompLBSourceData		3 << 2
	#define DPM_CompFuncLESS		1 << 4
	#define DPM_CompFuncEQUAL		2 << 4
	#define DPM_CompFuncLESS_OR_EQ		3 << 4
	#define DPM_CompFuncGREATER		4 << 4
	#define DPM_CompFuncNOT_EQ		5 << 4
	#define DPM_CompFuncGREATER_OR_EQ	6 << 4
	#define DPM_CompFuncALWAYS		7 << 4

#define GLINTDepth				GLINT_TAG_ADDR(0x13,0x05)
#define ZStartU					GLINT_TAG_ADDR(0x13,0x06)
#define ZStartL					GLINT_TAG_ADDR(0x13,0x07)
#define dZdxU					GLINT_TAG_ADDR(0x13,0x08)
#define dZdxL					GLINT_TAG_ADDR(0x13,0x09)
#define dZdyDomU				GLINT_TAG_ADDR(0x13,0x0a)
#define dZdyDomL				GLINT_TAG_ADDR(0x13,0x0b)
#define FastClearDepth				GLINT_TAG_ADDR(0x13,0x0c)

#define FBReadMode				GLINT_TAG_ADDR(0x15,0x00)
	/* 0:				*/
	/* SrcNoRead			*/
	/* DstNoRead			*/
	/* DataFBDefault		*/
	/* WinTopLeft			*/
	/* ScanlineInterval1 		*/

	#define FBRM_SrcEnable			1 << 9
	#define FBRM_DstEnable			1 << 10
	#define FBRM_DataFBColor		1 << 15
	#define FBRM_WinBottomLeft		1 << 16
	#define FBRM_Packed			1 << 19
	#define FBRM_ScanlineInt2		1 << 23
	#define FBRM_ScanlineInt4		2 << 23
	#define FBRM_ScanlineInt8		3 << 23


#define FBSourceOffset				GLINT_TAG_ADDR(0x15,0x01)
#define FBPixelOffset				GLINT_TAG_ADDR(0x15,0x02)
#define FBColor					GLINT_TAG_ADDR(0x15,0x03)
#define FBData					GLINT_TAG_ADDR(0x15,0x04)
#define FBSourceData				GLINT_TAG_ADDR(0x15,0x05)

#define FBWindowBase				GLINT_TAG_ADDR(0x15,0x06)
#define FBWriteMode				GLINT_TAG_ADDR(0x15,0x07)
	/* 0:			*/
	/* FBWM_NoColorUpload	*/
	/* FBWM_WriteDisable	*/
	#define FBWM_WriteEnable		1
	#define FBWM_UploadColor		1 << 3
/* Permedia3 extensions */
	#define FBWM_Enable0			1 << 12

#define FBHardwareWriteMask			GLINT_TAG_ADDR(0x15,0x08)
#define FBBlockColor				GLINT_TAG_ADDR(0x15,0x09)
#define FBReadPixel				GLINT_TAG_ADDR(0x15,0x0a) /* PM */
#define PatternRamMode				GLINT_TAG_ADDR(0x15,0x0f)

#define PatternRamData0				GLINT_TAG_ADDR(0x16,0x00)
#define PatternRamData1				GLINT_TAG_ADDR(0x16,0x01)
#define PatternRamData2				GLINT_TAG_ADDR(0x16,0x02)
#define PatternRamData3				GLINT_TAG_ADDR(0x16,0x03)
#define PatternRamData4				GLINT_TAG_ADDR(0x16,0x04)
#define PatternRamData5				GLINT_TAG_ADDR(0x16,0x05)
#define PatternRamData6				GLINT_TAG_ADDR(0x16,0x06)
#define PatternRamData7				GLINT_TAG_ADDR(0x16,0x07)

#define FilterMode				GLINT_TAG_ADDR(0x18,0x00)
	/* 0:				*/
	/* CullDepthTags		*/
	/* CullDepthData		*/
	/* CullStencilTags		*/
	/* CullStencilData		*/
	/* CullColorTag			*/
	/* CullColorData		*/
	/* CullSyncTag			*/
	/* CullSyncData			*/
	/* CullStatisticTag		*/
	/* CullStatisticData		*/

	#define FM_PassDepthTags					0x0010
	#define FM_PassDepthData					0x0020
	#define FM_PassStencilTags					0x0040
	#define FM_PassStencilData					0x0080
	#define FM_PassColorTag						0x0100
	#define FM_PassColorData					0x0200
	#define FM_PassSyncTag						0x0400
	#define FM_PassSyncData						0x0800
	#define FM_PassStatisticTag					0x1000
	#define FM_PassStatisticData					0x2000

#define	Sync_tag							0x0188

#define StatisticMode				GLINT_TAG_ADDR(0x18,0x01)
#define MinRegion				GLINT_TAG_ADDR(0x18,0x02)
#define MaxRegion				GLINT_TAG_ADDR(0x18,0x03)
#define ResetPickResult				GLINT_TAG_ADDR(0x18,0x04)
#define MitHitRegion				GLINT_TAG_ADDR(0x18,0x05)
#define MaxHitRegion				GLINT_TAG_ADDR(0x18,0x06)
#define PickResult				GLINT_TAG_ADDR(0x18,0x07)
#define GlintSync				GLINT_TAG_ADDR(0x18,0x08)

#define FBBlockColorU				GLINT_TAG_ADDR(0x18,0x0d)
#define FBBlockColorL				GLINT_TAG_ADDR(0x18,0x0e)
#define SuspendUntilFrameBlank			GLINT_TAG_ADDR(0x18,0x0f)

#define KsRStart				GLINT_TAG_ADDR(0x19,0x00)
#define dKsRdx					GLINT_TAG_ADDR(0x19,0x01)
#define dKsRdyDom				GLINT_TAG_ADDR(0x19,0x02)
#define KsGStart				GLINT_TAG_ADDR(0x19,0x03)
#define dKsGdx					GLINT_TAG_ADDR(0x19,0x04)
#define dKsGdyDom				GLINT_TAG_ADDR(0x19,0x05)
#define KsBStart				GLINT_TAG_ADDR(0x19,0x06)
#define dKsBdx					GLINT_TAG_ADDR(0x19,0x07)
#define dKsBdyDom				GLINT_TAG_ADDR(0x19,0x08)

#define KdRStart				GLINT_TAG_ADDR(0x1A,0x00)
#define dKdRdx					GLINT_TAG_ADDR(0x1A,0x01)
#define dKdRdyDom				GLINT_TAG_ADDR(0x1A,0x02)
#define KdGStart				GLINT_TAG_ADDR(0x1A,0x03)
#define dKdGdx					GLINT_TAG_ADDR(0x1A,0x04)
#define dKdGdyDom				GLINT_TAG_ADDR(0x1A,0x05)
#define KdBStart				GLINT_TAG_ADDR(0x1A,0x06)
#define dKdBdx					GLINT_TAG_ADDR(0x1A,0x07)
#define dKdBdyDom				GLINT_TAG_ADDR(0x1A,0x08)

#define FBSourceBase				GLINT_TAG_ADDR(0x1B,0x00)
#define FBSourceDelta				GLINT_TAG_ADDR(0x1B,0x01)
#define Config					GLINT_TAG_ADDR(0x1B,0x02)
#define		CFBRM_SrcEnable		1<<0
#define		CFBRM_DstEnable		1<<1
#define		CFBRM_Packed		1<<2
#define		CWM_Enable		1<<3
#define		CCDDA_Enable		1<<4
#define		CLogOp_Enable		1<<5
#define ContextDump                             GLINT_TAG_ADDR(0x1B,0x08)
#define ContextRestore                          GLINT_TAG_ADDR(0x1B,0x09)
#define ContextData                             GLINT_TAG_ADDR(0x1B,0x0a)

#define TexelLUT0				GLINT_TAG_ADDR(0x1D,0x00)
#define TexelLUT1				GLINT_TAG_ADDR(0x1D,0x01)
#define TexelLUT2				GLINT_TAG_ADDR(0x1D,0x02)
#define TexelLUT3				GLINT_TAG_ADDR(0x1D,0x03)
#define TexelLUT4				GLINT_TAG_ADDR(0x1D,0x04)
#define TexelLUT5				GLINT_TAG_ADDR(0x1D,0x05)
#define TexelLUT6				GLINT_TAG_ADDR(0x1D,0x06)
#define TexelLUT7				GLINT_TAG_ADDR(0x1D,0x07)
#define TexelLUT8				GLINT_TAG_ADDR(0x1D,0x08)
#define TexelLUT9				GLINT_TAG_ADDR(0x1D,0x09)
#define TexelLUT10				GLINT_TAG_ADDR(0x1D,0x0A)
#define TexelLUT11				GLINT_TAG_ADDR(0x1D,0x0B)
#define TexelLUT12				GLINT_TAG_ADDR(0x1D,0x0C)
#define TexelLUT13				GLINT_TAG_ADDR(0x1D,0x0D)
#define TexelLUT14				GLINT_TAG_ADDR(0x1D,0x0E)
#define TexelLUT15				GLINT_TAG_ADDR(0x1D,0x0F)

#define YUVMode                                 GLINT_TAG_ADDR(0x1E,0x00)
#define ChromaUpper                             GLINT_TAG_ADDR(0x1E,0x01)
#define ChromaLower                             GLINT_TAG_ADDR(0x1E,0x02)
#define ChromaTestMode                          GLINT_TAG_ADDR(0x1E,0x03)
#define AlphaMapUpperBound                      GLINT_TAG_ADDR(0x1E,0x03) /* PM2 */
#define AlphaMapLowerBound                      GLINT_TAG_ADDR(0x1E,0x04) /* PM2 */


/******************************
 * GLINT Delta Core Registers *
 ******************************/

#define V0FixedTag	GLINT_TAG_ADDR(0x20,0x00)
#define V1FixedTag	GLINT_TAG_ADDR(0x21,0x00)
#define V2FixedTag	GLINT_TAG_ADDR(0x22,0x00)
#define V0FloatTag	GLINT_TAG_ADDR(0x23,0x00)
#define V1FloatTag	GLINT_TAG_ADDR(0x24,0x00)
#define V2FloatTag	GLINT_TAG_ADDR(0x25,0x00)

#define VPAR_s		0x00
#define VPAR_t		0x08
#define VPAR_q		0x10
#define VPAR_Ks		0x18
#define VPAR_Kd		0x20

/* have changed colors in ramdac !
#define VPAR_R		0x28
#define VPAR_G		0x30
#define VPAR_B		0x38
#define VPAR_A		0x40
*/
#define VPAR_B		0x28
#define VPAR_G		0x30
#define VPAR_R		0x38
#define VPAR_A		0x40

#define VPAR_f		0x48

#define VPAR_x		0x50
#define VPAR_y		0x58
#define VPAR_z		0x60

#define DeltaModeTag				GLINT_TAG_ADDR(0x26,0x00)
	/* 0:				*/
	/* GLINT_300SX			*/

	/* DeltaMode Register Bit Field Assignments */
	#define DM_GLINT_300SX					0x0000
	#define DM_GLINT_500TX					0x0001
	#define DM_PERMEDIA					0x0002
	#define DM_Depth_16BPP					(1 << 2)
	#define DM_Depth_24BPP					(2 << 2)
	#define DM_Depth_32BPP					(3 << 2)
	#define DM_FogEnable					0x0010
	#define DM_TextureEnable				0x0020
	#define DM_SmoothShadingEnable				0x0040
	#define DM_DepthEnable					0x0080
	#define DM_SpecularTextureEnable			0x0100
	#define DM_DiffuseTextureEnable				0x0200
	#define DM_SubPixelCorrectionEnable			0x0400
	#define DM_DiamondExit					0x0800
	#define DM_NoDraw					0x1000
	#define DM_ClampEnable					0x2000
	#define DM_ClampedTexParMode				0x4000
	#define DM_NormalizedTexParMode				0xC000


        #define DDCMD_AreaStrippleEnable                        0x0001
	#define DDCMD_LineStrippleEnable                        0x0002
	#define DDCMD_ResetLineStripple                         1 << 2
        #define DDCMD_FastFillEnable                            1 << 3
        /*  2 Bits reserved */
	#define DDCMD_PrimitiveType_Point                       2 << 6
	#define DDCMD_PrimitiveType_Line                        0 << 6
	#define DDCMD_PrimitiveType_Trapezoid                   1 << 6
	#define DDCMD_AntialiasEnable				1 << 8
     	#define DDCMD_AntialiasingQuality			1 << 9
        #define DDCMD_UsePointTable                             1 << 10
	#define DDCMD_SyncOnBitMask                             1 << 11
	#define DDCMD_SyncOnHostDate                            1 << 12
     	#define DDCMD_TextureEnable			        1 << 13
	#define DDCMD_FogEnable                                 1 << 14
	#define DDCMD_CoverageEnable                            1 << 15
	#define DDCMD_SubPixelCorrectionEnable                  1 << 16



#define DrawTriangle				GLINT_TAG_ADDR(0x26,0x01)
#define RepeatTriangle				GLINT_TAG_ADDR(0x26,0x02)
#define DrawLine01				GLINT_TAG_ADDR(0x26,0x03)
#define DrawLine10				GLINT_TAG_ADDR(0x26,0x04)
#define RepeatLine				GLINT_TAG_ADDR(0x26,0x05)
#define BroadcastMask				GLINT_TAG_ADDR(0x26,0x0F)

/* Permedia 3 - Accelerator Extensions */
#define FillRectanglePosition					0x8348
#define FillRender2D						0x8350
#define FBDstReadBufAddr0					0xAE80
#define FBDstReadBufOffset0					0xAEA0
#define FBDstReadBufWidth0					0xAEC0
#define FBDstReadMode						0xAEE0
#define		FBDRM_Enable0		1<<8
#define		FBDRM_Blocking		1<<24
#define FBDstReadEnables					0xAEE8
#define FBSrcReadMode						0xAF00
#define		FBSRM_Blocking		1<<11
#define FBSrcReadBufAddr					0xAF08
#define FBSrcReadBufOffset0					0xAF10
#define FBSrcReadBufWidth					0xAF18
#define FBWriteBufAddr0						0xB000
#define FBWriteBufOffset0					0xB020
#define FBWriteBufWidth0					0xB040
#define FBBlockColorBack					0xB0A0
#define ForegroundColor						0xB0C0
#define BackgroundColor						0xB0C8
#define RectanglePosition					0xB600
#define Render2D						0xB640

/*  Colorformats */
#define BGR555  1
#define BGR565  16
#define CI8     14
#define CI4     15

#ifdef DEBUG
#define GLINT_WRITE_REG(v,r)					\
	GLINT_VERB_WRITE_REG(pGlint,v,r,__FILE__,__LINE__)
#define GLINT_READ_REG(r)					\
	GLINT_VERB_READ_REG(pGlint,r,__FILE__,__LINE__)
#else

#define GLINT_WRITE_REG(v,r) \
	MMIO_OUT32(pGlint->IOBase + pGlint->IOOffset,(unsigned long)(r), (v))
#define GLINT_READ_REG(r) \
	MMIO_IN32(pGlint->IOBase + pGlint->IOOffset,(unsigned long)(r))

#endif /* DEBUG */

#define GLINT_WAIT(n)						\
do{								\
	if (pGlint->InFifoSpace>=(n))				\
	    pGlint->InFifoSpace -= (n);				\
	else {							\
	    int tmp;						\
	    while((tmp=GLINT_READ_REG(InFIFOSpace))<(n));	\
	    /* Clamp value due to bugs in PM3 */		\
	    if (tmp > pGlint->FIFOSize)				\
		tmp = pGlint->FIFOSize;				\
	    pGlint->InFifoSpace = tmp - (n);			\
	}							\
}while(0)

#define GLINTDACDelay(x) do {                                   \
        int delay = x;                                          \
        unsigned char tmp;                                      \
	while(delay--){tmp = GLINT_READ_REG(InFIFOSpace);};     \
	} while(0)

#define GLINT_MASK_WRITE_REG(v,m,r)				\
	GLINT_WRITE_REG((GLINT_READ_REG(r)&(m))|(v),r)

#define GLINT_SLOW_WRITE_REG(v,r)				\
do{								\
	mem_barrier();						\
	GLINT_WAIT(pGlint->FIFOSize);	     			\
	mem_barrier();						\
        GLINT_WRITE_REG(v,r);					\
}while(0)

#define GLINT_SET_INDEX(index)					\
do{								\
	GLINT_SLOW_WRITE_REG(((index)>>8)&0xff,PM2VDACIndexRegHigh);	\
	GLINT_SLOW_WRITE_REG((index)&0xff,PM2VDACIndexRegLow);	\
} while(0)

#define REPLICATE(r)						\
{								\
	if (pScrn->bitsPerPixel == 16) {			\
		r &= 0xFFFF;					\
		r |= (r<<16);					\
	} else							\
	if (pScrn->bitsPerPixel == 8) { 			\
		r &= 0xFF;					\
		r |= (r<<8);					\
		r |= (r<<16);					\
	}							\
}

#ifndef XF86DRI
#define LOADROP(rop)						\
{								\
	if (pGlint->ROP != rop)	{				\
		GLINT_WRITE_REG(rop<<1|UNIT_ENABLE, LogicalOpMode);	\
		pGlint->ROP = rop;				\
	}							\
}
#else
#define LOADROP(rop) \
	{				\
		GLINT_WRITE_REG(rop<<1|UNIT_ENABLE, LogicalOpMode);	\
		pGlint->ROP = rop;				\
	}
#endif

#define CHECKCLIPPING						\
{								\
	if (pGlint->ClippingOn) {				\
		pGlint->ClippingOn = FALSE;			\
		GLINT_WAIT(1);					\
		GLINT_WRITE_REG(0, ScissorMode);		\
	}							\
}

#ifndef XF86DRI
#define DO_PLANEMASK(planemask)					\
{ 								\
	if (planemask != pGlint->planemask) {			\
		pGlint->planemask = planemask;			\
		REPLICATE(planemask); 				\
		GLINT_WRITE_REG(planemask, FBHardwareWriteMask);\
	}							\
}
#else
#define DO_PLANEMASK(planemask)					\
	{							\
		pGlint->planemask = planemask;			\
		REPLICATE(planemask); 				\
		GLINT_WRITE_REG(planemask, FBHardwareWriteMask);\
	}
#endif

/* Permedia Save/Restore functions */

#define STOREREG(address,value) 				\
    	pReg->glintRegs[address >> 3] = value;

#define SAVEREG(address) 					\
    	pReg->glintRegs[address >> 3] = GLINT_READ_REG(address);

#define RESTOREREG(address) 					\
    	GLINT_SLOW_WRITE_REG(pReg->glintRegs[address >> 3], address);

#define STOREDAC(address,value)					\
    	pReg->DacRegs[address] = value;

#define P2VOUT(address)						\
    Permedia2vOutIndReg(pScrn, address, 0x00, pReg->DacRegs[address]);

#define P2VIN(address)						\
    pReg->DacRegs[address] = Permedia2vInIndReg(pScrn, address);

/* RamDac Save/Restore functions, used by external DAC's */

#define STORERAMDAC(address,value)				\
    	ramdacReg->DacRegs[address] = value;

/* Multi Chip access */

#define ACCESSCHIP1()						\
    pGlint->IOOffset = 0;

#define ACCESSCHIP2()						\
    pGlint->IOOffset = 0x10000;

#endif /* MPLAYER_GLINT_REGS_H */
