/* 
 *  video_out_vesa.c
 *
 *	Copyright (C) Nick Kurshev <nickols_k@mail.ru> - Oct 2001
 *
 *  You can redistribute this file under terms and conditions
 *  GNU General Public licence v2.
 *  This file is partly based on vbetest.c from lrmi distributive.
 */

/*
  TODO:
  - DGA support (need volunteers who have corresponding hardware)
  - hw YUV support (need volunteers who have corresponding hardware)
  - double (triple) buffering (if it will really speedup playback).
  - refresh rate support (need additional info from mplayer)
*/
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "fastmemcpy.h"
#include "yuv2rgb.h"

#include "linux/lrmi.h"
#include "linux/vbelib.h"
#include "bswap.h"

LIBVO_EXTERN(vesa)
extern int verbose;

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif


static vo_info_t vo_info = 
{
	"VESA VBE 2.0 video output",
	"vesa",
	"Nick Kurshev <nickols_k@mail.ru>",
        "Requires ROOT privileges"
};

/* driver data */

/*
   TODO: for linear framebuffer mode:
   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   win.ptr = linear address of frame buffer;
   win.low = 0;
   win.high = vide_memory_size;
*/
struct win_frame
{
  uint8_t   *ptr;   /* pointer to window's frame memory */
  uint32_t   low;   /* lowest boundary of frame */
  uint32_t   high;  /* highest boundary of frame */
  uint8_t    idx;   /* indicates index of relocatable frame (A or B) */
};

static uint32_t image_width, image_height; /* source image dimension */
static uint32_t x_offset,y_offset; /* to center image on screen */
static unsigned init_mode; /* mode before run of mplayer */
static void *init_state = NULL; /* state before run of mplayer */
static struct win_frame win; /* real-mode window to video memory */
static void *yuv_buffer = NULL; /* for yuv2rgb and sw_scaling */
static unsigned video_mode; /* selected video mode for playback */
static struct VesaModeInfoBlock video_mode_info;

#define MOVIE_MODE (MODE_ATTR_COLOR | MODE_ATTR_GRAPHICS)
#define FRAME_MODE (MODE_WIN_RELOCATABLE | MODE_WIN_READABLE | MODE_WIN_WRITEABLE)
static char * vbeErrToStr(int err)
{
  char *retval;
  static char sbuff[80];
  if((err & VBE_VESA_ERROR_MASK) == VBE_VESA_ERROR_MASK)
  {
    sprintf(sbuff,"VESA failed = 0x4f%x",(err & VBE_VESA_ERRCODE_MASK)>>8);
    retval = sbuff;
  }
  else
  switch(err)
  { 
    case VBE_OK: retval = "No error"; break;
    case VBE_VM86_FAIL: retval = "vm86() syscall failed"; break;
    case VBE_OUT_OF_DOS_MEM: retval = "Out of DOS memory"; break;
    case VBE_OUT_OF_MEM: retval = "Out of memory"; break;
    default: sprintf(sbuff,"Uknown error: %i",err); retval=sbuff; break;
  }
  return retval;
}

#define PRINT_VBE_ERR(name,err) { printf("vo_vesa: %s returns: %s\n",name,vbeErrToStr(err)); fflush(stdout); }

static void vesa_term( void )
{
  int err;
  if((err=vbeRestoreState(init_state)) != VBE_OK) PRINT_VBE_ERR("vbeRestoreState",err);
  if((err=vbeSetMode(init_mode,NULL)) != VBE_OK) PRINT_VBE_ERR("vbeSetMode",err);
  free(yuv_buffer);
  vbeDestroy();
}

#define VALID_WIN_FRAME(offset) (offset >= win.low && offset < win.high)
#define VIDEO_PTR(offset) (win.ptr + offset - win.low)

static inline void __vbeSwitchBank(unsigned long offset)
{
  unsigned long gran;
  unsigned new_offset;
  int err;
  gran = video_mode_info.WinGranularity*1024;
  new_offset = offset / gran;
  if((err=vbeSetWindow(win.idx,new_offset)) != VBE_OK)
  {
    PRINT_VBE_ERR("vbeSetWindow",err);
    printf("vo_vesa: Fatal error occured! Can't continue\n");
    vesa_term();
    exit(-1);
  }
  win.low = new_offset * gran;
  win.high = win.low + video_mode_info.WinSize*1024;
}

static void __vbeSetPixel(int x, int y, int r, int g, int b)
{
	int x_res = video_mode_info.XResolution;
	int y_res = video_mode_info.YResolution;
	int shift_r = video_mode_info.RedFieldPosition;
	int shift_g = video_mode_info.GreenFieldPosition;
	int shift_b = video_mode_info.BlueFieldPosition;
	int pixel_size = (video_mode_info.BitsPerPixel+7)/8;
	int bpl = video_mode_info.BytesPerScanLine;
	int color, offset;

	if (x < 0 || x >= x_res || y < 0 || y >= y_res)	return;
	r >>= 8 - video_mode_info.RedMaskSize;
	g >>= 8 - video_mode_info.GreenMaskSize;
	b >>= 8 - video_mode_info.BlueMaskSize;
	color = (r << shift_r) | (g << shift_g) | (b << shift_b);
	offset = y * bpl + (x * pixel_size);
        if(!VALID_WIN_FRAME(offset)) __vbeSwitchBank(offset);
	memcpy(VIDEO_PTR(offset), &color, pixel_size);
}

/*
  Copies line of frame to video memory. Data should be in the same format as video
  memory.
*/
static void __vbeCopyBlock(unsigned long offset,uint8_t *image,unsigned long size)
{
   unsigned long delta,src_idx = 0;
   while(size)
   {
     if(!VALID_WIN_FRAME(offset)) __vbeSwitchBank(offset);
     delta = min(size,win.high - offset);
     memcpy(VIDEO_PTR(offset),&image[src_idx],delta);
     src_idx += delta;
     offset += delta;
     size -= delta;
   }
}

static void __vbeCopyBlockSwap(unsigned long offset,uint8_t *image,unsigned long size)
{
   unsigned byte_len;
   uint8_t ch;
   while(size)
   {
       switch(video_mode_info.BitsPerPixel)
       {
	case 8: byte_len = 1; break;
	default:
	case 15:
		printf("vo_vesa: Can't swap non byte aligned data\n");
		vesa_term();
		exit(-1);
	case 16: *(image + offset) = ByteSwap16(*(image + offset));
		 byte_len = 2; break;
	case 24: ch = *(image+offset);
		 *(image+offset) = *(image+offset+3);
                 *(image+offset+3) = ch;
		 byte_len = 3; break;
	case 32: *(image + offset) = ByteSwap32(*(image + offset));
		 byte_len = 4; break;
       }
       __vbeCopyBlock(offset,image,byte_len);
       size   -= byte_len;
       image  += byte_len;
       offset += byte_len;
   }
}

/*
  Copies frame to video memory. Data should be in the same format as video
  memory.
*/
static void __vbeCopyData(uint8_t *image)
{
   unsigned long i,image_offset,offset;
   unsigned pixel_size,image_line_size,screen_line_size,x_shift;
   pixel_size = (video_mode_info.BitsPerPixel+7)/8;
   screen_line_size = video_mode_info.XResolution*pixel_size;
   image_line_size = image_width*pixel_size;
   x_shift = x_offset*pixel_size;
   for(i=y_offset;i<image_height;i++)
   {
     offset = i*screen_line_size+x_shift;
     image_offset = i*image_line_size;
     __vbeCopyBlock(offset,&image[image_offset],image_line_size);
   }
}
/* is called for yuv only */
static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	yuv2rgb(yuv_buffer, image[0], image[1], image[2], w, h,
		image_width * ((video_mode_info.BitsPerPixel+7)/8),
		stride[0], stride[1]);
	__vbeCopyData((uint8_t *)yuv_buffer);
	return 0;
}

static void draw_osd(void)
{
/* nothing to do for now */
}

static void flip_page(void)
{
/*Is not required*/
}

/* is called for rgb only */
static uint32_t draw_frame(uint8_t *src[])
{
	__vbeCopyData(src[0]);
	return 0;
}

static uint32_t query_format(uint32_t format)
{
  uint32_t retval;
	switch(format)
	{
#if 0
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV:
#endif
		case IMGFMT_RGB8:
		case IMGFMT_RGB15:
		case IMGFMT_RGB16:
		case IMGFMT_RGB24:
		case IMGFMT_RGB32:
		case IMGFMT_BGR8:
		case IMGFMT_BGR15:
		case IMGFMT_BGR16:
		case IMGFMT_BGR24:
		case IMGFMT_BGR32:
				retval = 1; break;
		default:
			if(verbose)
				printf("vo_vesa: unknown format: %x = %s\n",format,vo_format_name(format));
			retval = 0;
	}
	return retval;
}

static char *model2str(unsigned char type)
{
  char *retval;
  switch(type)
  {
    case memText: retval = "Text"; break;
    case memCGA:  retval="CGA"; break;
    case memHercules: retval="Hercules"; break;
    case memPL: retval="Planar"; break;
    case memPK: retval="Packed pixel"; break;
    case mem256: retval="256"; break;
    case memRGB: retval="Direct color RGB"; break;
    case memYUV: retval="Direct color YUV"; break;
    default: retval="Unknown"; break;
  }
  return retval;
}

/* fullscreen:
 * bit 0 (0x01) means fullscreen (-fs)
 * bit 1 (0x02) means mode switching (-vm)
 * bit 2 (0x04) enables software scaling (-zoom)
 * bit 3 (0x08) enables flipping (-flip)
 */
static uint32_t
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
  struct VbeInfoBlock vib;
  struct VesaModeInfoBlock vmib;
  size_t i,num_modes;
  unsigned short *mode_ptr,win_seg;
  unsigned bpp,best_x = UINT_MAX,best_y=UINT_MAX,best_mode_idx = UINT_MAX;
  int err;
	image_width = width;
	image_height = height;
	if(fullscreen & (0x1|0x4|0x8))
	{
	  printf("vo_vesa: switches: -fs, -zoom, -flip are not supported\n");
	}
	if((err=vbeInit()) != VBE_OK) { PRINT_VBE_ERR("vbeInit",err); return -1; }
	memcpy(vib.VESASignature,"VBE2",4);
	if((err=vbeGetControllerInfo(&vib)) != VBE_OK)
	{
	  PRINT_VBE_ERR("vbeGetControllerInfo",err);
	  printf("vo_vesa: possible reason: No VBE2 BIOS found\n");
	  return -1;
	}
	/* Print general info here */
	printf("vo_vesa: Found VESA VBE BIOS Version %x.%x Revision: %x\n",
		(int)(vib.VESAVersion >> 8) & 0xff,
		(int)(vib.VESAVersion & 0xff),
		(int)(vib.OemSoftwareRev & 0xffff));
	printf("vo_vesa: Video memory: %u Kb\n",vib.TotalMemory*64);
	printf("vo_vesa: VESA Capabilities: %s %s %s %s %s\n"
		,vib.Capabilities & VBE_DAC_8BIT ? "8-bit DAC," : "6-bit DAC,"
		,vib.Capabilities & VBE_NONVGA_CRTC ? "non-VGA CRTC,":"VGA CRTC,"
		,vib.Capabilities & VBE_SNOWED_RAMDAC ? "snowed RAMDAC,":"normal RAMDAC,"
		,vib.Capabilities & VBE_STEREOSCOPIC ? "stereoscopic,":"no stereoscopic,"
		,vib.Capabilities & VBE_STEREO_EVC ? "Stereo EVC":"no stereo");
	printf("vo_vesa: !!! Below will be printed OEM info. !!!\n");
	printf("vo_vesa: You should watch 5 OEM related lines below else you've broken vm86\n");
	printf("vo_vesa: OEM info: %s\n",vib.OemStringPtr);
	printf("vo_vesa: OEM Revision: %x\n",vib.OemSoftwareRev);
	printf("vo_vesa: OEM vendor: %s\n",vib.OemVendorNamePtr);
	printf("vo_vesa: OEM Product Name: %s\n",vib.OemProductNamePtr);
	printf("vo_vesa: OEM Product Rev: %s\n",vib.OemProductRevPtr);
	printf("vo_vesa: Hint: To get workable TV-Out you should have plugged tv-connector in\n"
	       "vo_vesa: before booting PC since VESA BIOS initializes itself only during POST\n");
	/* Find best mode here */
	num_modes = 0;
	mode_ptr = vib.VideoModePtr;
	while(*mode_ptr++ != 0xffff) num_modes++;
	switch(format)
	{
		case IMGFMT_BGR8:
		case IMGFMT_RGB8:  bpp = 8; break;
		case IMGFMT_BGR15:
                case IMGFMT_RGB15: bpp = 15; break;
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV:
			yuv2rgb_init(video_mode_info.BitsPerPixel, MODE_RGB);
		default:
		case IMGFMT_BGR16:
		case IMGFMT_RGB16: bpp = 16; break;
		case IMGFMT_BGR24:
		case IMGFMT_RGB24: bpp = 24; break;
		case IMGFMT_BGR32:
		case IMGFMT_RGB32: bpp = 32; break;
	}
	if(verbose)
	{
	  printf("vo_vesa: Requested mode: %ux%u@%x bpp=%u\n",width,height,format,bpp);
	  printf("vo_vesa: Total modes found: %u\n",num_modes);
	  mode_ptr = vib.VideoModePtr;
	  printf("vo_vesa: Mode list:");
	  for(i = 0;i < num_modes;i++)
	  {
	    printf(" %04X",mode_ptr[i]);
	  }
	  printf("\nvo_vesa: Modes in detail:\n");
	}
	mode_ptr = vib.VideoModePtr;
        for(i=0;i < num_modes;i++)
	{
		if((err=vbeGetModeInfo(mode_ptr[i],&vmib)) != VBE_OK)
		{
			PRINT_VBE_ERR("vbeGetModeInfo",err);
			return -1;
		}
		if(vmib.XResolution >= image_width &&
		   vmib.YResolution >= image_height &&
		   (vmib.ModeAttributes & MOVIE_MODE) == MOVIE_MODE &&
		   vmib.BitsPerPixel == bpp &&
		   vmib.MemoryModel == memRGB)
		   {
			if(vmib.XResolution <= best_x &&
			   vmib.YResolution <= best_y)
			   {
				best_x = vmib.XResolution;
				best_y = vmib.YResolution;
				best_mode_idx = i;
			   }
		   }
		if(verbose)
		{
		  printf("vo_vesa: Mode (%03u): mode=%04X %ux%u@%u attr=%x\n"
			 "vo_vesa:             #planes=%u model=%u(%s) #pages=%u\n"
			 "vo_vesa:             winA=%X(attr=%u) winB=%X(attr=%u) winSize=%u winGran=%u\n"
			 "vo_vesa:             direct_color=%u DGA_phys_addr=%08X\n"
			 ,i,mode_ptr[i],vmib.XResolution,vmib.YResolution,vmib.BitsPerPixel,vmib.ModeAttributes
			 ,vmib.NumberOfPlanes,vmib.MemoryModel,model2str(vmib.MemoryModel),vmib.NumberOfImagePages
			 ,vmib.WinASegment,vmib.WinAAttributes,vmib.WinBSegment,vmib.WinBAttributes,vmib.WinSize,vmib.WinGranularity
			 ,vmib.DirectColorModeInfo,vmib.PhysBasePtr);
		  if(vmib.MemoryModel == 6 || vmib.MemoryModel == 7)
			printf("vo_vesa:             direct_color_info = %u:%u:%u:%u\n"
				,vmib.RedMaskSize,vmib.GreenMaskSize,vmib.BlueMaskSize,vmib.RsvdMaskSize);
		  fflush(stdout);
		}
	}
	if(best_mode_idx != UINT_MAX)
	{
		video_mode = vib.VideoModePtr[best_mode_idx];
		printf("vo_vesa: Using VESA mode (%u) = %x\n",best_mode_idx,video_mode);
		fflush(stdout);
		if((err=vbeGetMode(&init_mode)) != VBE_OK)
		{
			PRINT_VBE_ERR("vbeGetMode",err);
			return -1;
		}
		if(verbose) printf("vo_vesa: Initial video mode: %x\n",init_mode);
		if((err=vbeGetModeInfo(video_mode,&video_mode_info)) != VBE_OK)
		{
			PRINT_VBE_ERR("vbeGetModeInfo",err);
			return -1;
		}
		if(!(yuv_buffer = malloc(video_mode_info.XResolution*video_mode_info.YResolution*4)))
		{
		  printf("vo_vesa: Can't allocate temporary buffer\n");
		  return -1;
		}
		if((video_mode_info.WinAAttributes & FRAME_MODE) == FRAME_MODE)
		   win.idx = 0; /* frame A */
		else
		if((video_mode_info.WinBAttributes & FRAME_MODE) == FRAME_MODE)
		   win.idx = 1; /* frame B */
		else { printf("vo_vesa: Can't find usable frame of window\n"); return -1; }
		if(!(win_seg = win.idx == 0 ? video_mode_info.WinASegment:video_mode_info.WinBSegment))
		{
		  printf("vo_vesa: Can't find valid window address\n");
		  if(video_mode_info.ModeAttributes & MODE_ATTR_LINEAR)
		  	printf("vo_vesa: Your BIOS supports DGA access which is not implemented for now\n");
		  return -1;
		}
		win.ptr = PhysToVirtSO(win_seg,0);
		win.low = 0L;
		win.high= video_mode_info.WinSize*1024;
		x_offset = (video_mode_info.XResolution - image_width) / 2;
		y_offset = (video_mode_info.YResolution - image_height) / 2;
		if((err=vbeSaveState(&init_state)) != VBE_OK)
		{
			PRINT_VBE_ERR("vbeSaveState",err);
			return -1;
		}
		if((err=vbeSetMode(video_mode,NULL)) != VBE_OK)
		{
			PRINT_VBE_ERR("vbeSetMode",err);
			return -1;
		}
		/* Now we are in video mode!!!*/
		if(verbose)
		{
		  printf("vo_vesa: Graphics mode was activated\n");
		  fflush(stdout);
		}
	}
	else
	{
	  printf("vo_vesa: Can't find mode for: %ux%u@%u\n",width,height,bpp);
	  return -1;
	}
	if(verbose)
	{
	  printf("vo_vesa: VESA initialization complete\n");
	  fflush(stdout);
	}
	if(verbose)
	{
		int x_res = video_mode_info.XResolution;
		int y_res = video_mode_info.YResolution;
		int x, y;

		for (y = 0; y < y_res; ++y)
			{
			for (x = 0; x < x_res; ++x)
				{
				int r, g, b;
				if ((x & 16) ^ (y & 16))
					{
					r = x * 255 / x_res;
					g = y * 255 / y_res;
					b = 255 - x * 255 / x_res;
					}
				else
					{
					r = 255 - x * 255 / x_res;
					g = y * 255 / y_res;
					b = 255 - y * 255 / y_res;
					}

				__vbeSetPixel(x, y, r, g, b);
				}
			}
	}
	return 0;
}

static const vo_info_t*
get_info(void)
{
	return &vo_info;
}

static void
uninit(void)
{
	vesa_term();
}


static void check_events(void)
{
/* Nothing to do */
}
