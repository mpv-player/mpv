/*
   VESA VBE 2.0 compatible structures and definitions.
   You can redistribute this file under terms and conditions
   GNU General Public licence v2.
   Written by Nick Kurshev <nickols_k@mail.ru>
*/
#ifndef __VESA_VBELIB_INCLUDED__
#define __VESA_VBELIB_INCLUDED__ 1

/* Note: every pointer within structures is 32-bit protected mode pointer.
   So you don't need to convert it from real mode. */

typedef struct tagFarPtr
{
  unsigned short off;
  unsigned short seg;
}FarPtr;

#define VBE_DAC_8BIT       (1 << 0)
#define VBE_NONVGA_CRTC    (1 << 1)
#define VBE_SNOWED_RAMDAC  (1 << 2)
#define VBE_STEREOSCOPIC   (1 << 3)
#define VBE_STEREO_EVC     (1 << 4)

struct VbeInfoBlock {
  char          VESASignature[4]; /* 'VESA' 4 byte signature */
  short         VESAVersion;      /* VBE version number */
  char *        OemStringPtr;     /* Pointer to OEM string */
  long          Capabilities;     /* Capabilities of video card */
  unsigned short* VideoModePtr;   /* Pointer to supported modes */
  short         TotalMemory;      /* Number of 64kb memory blocks */
  /* VBE 2.0 and above */
  short         OemSoftwareRev;
  char *        OemVendorNamePtr;
  char *        OemProductNamePtr;
  char *        OemProductRevPtr;
  char          reserved[222];
  char          OemData[256];     /* Pad to 512 byte block size */
}__attribute__ ((packed));

static inline FarPtr VirtToPhys(void *ptr)
{
  FarPtr retval;
  retval.seg = ((unsigned long)ptr) >> 4;
  retval.off = ((unsigned long)ptr) & 0x0f;
  return retval;
}

static inline unsigned short VirtToPhysSeg(void *ptr)
{
  return ((unsigned long)ptr) >> 4;
}

static inline unsigned short VirtToPhysOff(void *ptr)
{
  return ((unsigned long)ptr) & 0x0f;
}

static inline void * PhysToVirt(FarPtr ptr)
{
  return (void *)((ptr.seg << 4) | ptr.off);
}

static inline void * PhysToVirtSO(unsigned short seg,unsigned short off)
{
  return (void *)((seg << 4) | off);
}

#define MODE_ATTR_MODE_SUPPORTED (1 << 0)
#define MODE_ATTR_TTY 		(1 << 2)
#define MODE_ATTR_COLOR 	(1 << 3)
#define MODE_ATTR_GRAPHICS 	(1 << 4)
#define MODE_ATTR_NOT_VGA 	(1 << 5)
#define MODE_ATTR_NOT_WINDOWED 	(1 << 6)
#define MODE_ATTR_LINEAR 	(1 << 7)
#define MODE_ATTR_DOUBLESCAN 	(1 << 8)
#define MODE_ATTR_INTERLACE 	(1 << 9)
#define MODE_ATTR_TRIPLEBUFFER 	(1 << 10)
#define MODE_ATTR_STEREOSCOPIC 	(1 << 11)
#define MODE_ATTR_DUALDISPLAY 	(1 << 12)

#define MODE_WIN_RELOCATABLE 	(1 << 0)
#define MODE_WIN_READABLE 	(1 << 1)
#define MODE_WIN_WRITEABLE 	(1 << 2)

/* SuperVGA mode information block */
struct VesaModeInfoBlock {
  unsigned short ModeAttributes;      /* Mode attributes */
  unsigned char  WinAAttributes;      /* Window A attributes */
  unsigned char  WinBAttributes;      /* Window B attributes */
  unsigned short WinGranularity;      /* Window granularity in k */
  unsigned short WinSize;             /* Window size in k */
  unsigned short WinASegment;         /* Window A segment */
  unsigned short WinBSegment;         /* Window B segment */
  FarPtr         WinFuncPtr;          /* 16-bit far pointer to window function */
  unsigned short BytesPerScanLine;    /* Bytes per scanline */
  /*  VBE 1.2 and above */
  unsigned short XResolution;         /* Horizontal resolution */
  unsigned short YResolution;         /* Vertical resolution */
  unsigned char  XCharSize;           /* Character cell width */
  unsigned char  YCharSize;           /* Character cell height */
  unsigned char  NumberOfPlanes;      /* Number of memory planes */
  unsigned char  BitsPerPixel;        /* Bits per pixel */
  unsigned char  NumberOfBanks;       /* Number of CGA style banks */
  unsigned char  MemoryModel;         /* Memory model type */
  unsigned char  BankSize;            /* Size of CGA style banks */
  unsigned char  NumberOfImagePages;  /* Number of images pages */
  unsigned char  res1;                /* Reserved */
  /* Direct Color fields (required for direct/6 and YUV/7 memory models) */
  unsigned char  RedMaskSize;         /* Size of direct color red mask */
  unsigned char  RedFieldPosition;    /* Bit posn of lsb of red mask */
  unsigned char  GreenMaskSize;       /* Size of direct color green mask */
  unsigned char  GreenFieldPosition;  /* Bit posn of lsb of green mask */
  unsigned char  BlueMaskSize;        /* Size of direct color blue mask */
  unsigned char  BlueFieldPosition;   /* Bit posn of lsb of blue mask */
  unsigned char  RsvdMaskSize;        /* Size of direct color res mask */
  unsigned char  RsvdFieldPosition;   /* Bit posn of lsb of res mask */
  unsigned char  DirectColorModeInfo; /* Direct color mode attributes */
  unsigned char  res2[216];           /* Pad to 256 byte block size */
  /* VBE 2.0 and above */
  unsigned long  PhysBasePtr;         /* physical address for flat memory frame buffer. (Should be converted to linear before using) */
  unsigned short res3[3];             /* Reserved - always set to 0 */
  /* VBE 3.0 and above */
  unsigned short LinBytesPerScanLine; /* bytes per scan line for linear modes */
  unsigned char  BnkNumberOfImagePages;/* number of images for banked modes */
  unsigned char  LinNumberOfImagePages;/* number of images for linear modes */
  unsigned char  LinRedMaskSize;      /* size of direct color red mask (linear modes) */
  unsigned char  LinRedFieldPosition; /* bit position of lsb of red mask (linear modes) */
  unsigned char  LinGreenMaskSize;    /* size of direct color green mask (linear modes) */
  unsigned char  LinGreenFieldPosition;/* bit position of lsb of green mask (linear modes) */
  unsigned char  LinBlueMaskSize;     /* size of direct color blue mask (linear modes) */
  unsigned char  LinBlueFieldPosition;/* bit position of lsb of blue mask (linear modes) */
  unsigned char  LinRsvdMaskSize;     /* size of direct color reserved mask (linear modes) */
  unsigned char  LinRsvdFieldPosition;/* bit position of lsb of reserved mask (linear modes) */
  unsigned long  MaxPixelClock;       /* maximum pixel clock (in Hz) for graphics mode */
  char           res4[189];           /* remainder of ModeInfoBlock */
}__attribute__ ((packed));

typedef enum {
  memText= 0,
  memCGA = 1,
  memHercules = 2,
  memPL  = 3, /* Planar memory model */
  memPK  = 4, /* Packed pixel memory model */
  mem256 = 5,
  memRGB = 6, /* Direct color RGB memory model */
  memYUV = 7, /* Direct color YUV memory model */
} memModels;

struct VesaCRTCInfoBlock {
  unsigned short hTotal;     /* Horizontal total in pixels */
  unsigned short hSyncStart; /* Horizontal sync start in pixels */
  unsigned short hSyncEnd;   /* Horizontal sync end in pixels */
  unsigned short vTotal;     /* Vertical total in lines */
  unsigned short vSyncStart; /* Vertical sync start in lines */
  unsigned short vSyncEnd;   /* Vertical sync end in lines */
  unsigned char  Flags;      /* Flags (Interlaced, Double Scan etc) */
  unsigned long  PixelClock; /* Pixel clock in units of Hz */
  unsigned short RefreshRate;/* Refresh rate in units of 0.01 Hz*/
  unsigned char  Reserved[40];/* remainder of CRTCInfoBlock*/
}__attribute__ ((packed));

#define VESA_CRTC_DOUBLESCAN 0x01
#define VESA_CRTC_INTERLACED 0x02
#define VESA_CRTC_HSYNC_NEG  0x04
#define VESA_CRTC_VSYNC_NEG  0x08

#define VESA_MODE_CRTC_REFRESH (1 << 11)
#define VESA_MODE_USE_LINEAR   (1 << 14)
#define VESA_MODE_NOT_CLEAR    (1 << 15)

/* This will contain accesible 32-bit protmode pointers */
struct VesaProtModeInterface
{
  void (*SetWindowCall)(void);
  void (*SetDisplayStart)(void);
  void (*SetPaletteData)(void);
  unsigned short * iopl_ports;
};

/*
  All functions below return:
  0      if succesful
  0xffff if vm86 syscall error occurs
  0x4fxx if VESA error occurs
*/

#define VBE_OK                 0
#define VBE_VM86_FAIL         -1
#define VBE_OUT_OF_DOS_MEM    -2
#define VBE_OUT_OF_MEM        -3
#define VBE_BROKEN_BIOS       -4
#define VBE_VESA_ERROR_MASK   0x004f
#define VBE_VESA_ERRCODE_MASK 0xff00

extern int vbeInit( void );
extern int vbeDestroy( void );

extern int vbeGetControllerInfo(struct VbeInfoBlock *);
extern int vbeGetModeInfo(unsigned mode,struct VesaModeInfoBlock *);
extern int vbeSetMode(unsigned mode,struct VesaCRTCInfoBlock *);
extern int vbeGetMode(unsigned *mode);
extern int vbeSaveState(void **data); /* note never copy this data */
extern int vbeRestoreState(void *data);
extern int vbeGetWindow(unsigned *win_num); /* win_A=0 or win_B=1 */
extern int vbeSetWindow(unsigned win_num,unsigned win_gran);
/*
   Func 0x06:
   Support of logical scan line length is not implemented.
   We assume that logical scan line length == physical scan line length.
   (Logical display memory == displayed area).
*/ 
/*
   Func 0x07:
   Support of disply start is not implemented.
   We assume that display start always == 0, 0.
*/ 
/*
   Func 0x08-0x09:
   Support of palette currently is not implemented.
*/ 
extern int vbeGetProtModeInfo(struct VesaProtModeInterface *);

/* Standard VGA stuff */
int vbeWriteString(int x, int y, int attr, char *str);

#endif
