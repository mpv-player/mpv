/*
 *  video_out_vesa.c
 *
 *	Copyright (C) Nick Kurshev <nickols_k@mail.ru> - Oct 2001
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 *  This file is partly based on vbetest.c from lrmi distributive.
 */

/*
  TODO:
  - hw YUV support (need volunteers who have corresponding hardware)
  - triple buffering (if it will really speedup playback).
    note: triple buffering requires VBE 3.0.
  - refresh rate support (need additional info from mplayer)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#ifdef HAVE_MEMALIGN
#include <malloc.h>
#endif

#include "fastmemcpy.h"
#include "yuv2rgb.h"
#include "sub.h"
#include "linux/vbelib.h"
#include "bswap.h"

#include "../postproc/swscale.h"
#include "../postproc/rgb2rgb.h"

LIBVO_EXTERN(vesa)
extern int verbose;

#define MAX_BUFFERS 3

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

struct win_frame
{
  uint8_t   *ptr;   /* pointer to window's frame memory */
  uint32_t   low;   /* lowest boundary of frame */
  uint32_t   high;  /* highest boundary of frame */
  char       idx;   /* indicates index of relocatable frame (A=0 or B=1)
                       special case for DGA: idx=-1
		       idx=-2 indicates invalid frame, exists only in init() */
};

static void (*cpy_blk_fnc)(unsigned long,uint8_t *,unsigned long) = NULL;

static int vesa_zoom=0; /* software scaling */
static unsigned int scale_xinc=0;
static unsigned int scale_yinc=0;

static uint32_t image_bpp,image_width, image_height; /* source image dimension */
static int32_t x_offset,y_offset; /* to center image on screen */
static unsigned init_mode; /* mode before run of mplayer */
static void *init_state = NULL; /* state before run of mplayer */
static struct win_frame win; /* real-mode window to video memory */
static uint8_t *dga_buffer = NULL; /* for yuv2rgb and sw_scaling */
static unsigned video_mode; /* selected video mode for playback */
static struct VesaModeInfoBlock video_mode_info;
static int flip_trigger = 0;
static void (*draw_alpha_fnc)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);
static void (*rgb2rgb_fnc)(uint8_t *src,uint8_t *dst,uint32_t src_size);

/* multibuffering */
uint8_t*  video_base; /* should be never changed */
uint32_t  multi_buff[MAX_BUFFERS]; /* contains offsets of buffers */
uint8_t   multi_size=0; /* total number of buffers */
uint8_t   multi_idx=0; /* active buffer */

#define HAS_DGA()  (win.idx == -1)
#define MOVIE_MODE (MODE_ATTR_COLOR | MODE_ATTR_GRAPHICS)
#define FRAME_MODE (MODE_WIN_RELOCATABLE | MODE_WIN_WRITEABLE)

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
    case VBE_BROKEN_BIOS: retval = "Broken BIOS or DOS TSR"; break;
    default: sprintf(sbuff,"Unknown or internal error: %i",err); retval=sbuff; break;
  }
  return retval;
}

#define PRINT_VBE_ERR(name,err) { printf("vo_vesa: %s returns: %s\n",name,vbeErrToStr(err)); fflush(stdout); }

static void vesa_term( void )
{
  int err;
  if((err=vbeRestoreState(init_state)) != VBE_OK) PRINT_VBE_ERR("vbeRestoreState",err);
  if((err=vbeSetMode(init_mode,NULL)) != VBE_OK) PRINT_VBE_ERR("vbeSetMode",err);
  if(HAS_DGA()) vbeUnmapVideoBuffer((unsigned long)win.ptr,win.high);
  if(dga_buffer && !HAS_DGA()) free(dga_buffer);
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
  if(HAS_DGA()) { err = -1; goto show_err; }
  if((err=vbeSetWindow(win.idx,new_offset)) != VBE_OK)
  {
    show_err:
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
  Copies part of frame to video memory. Data should be in the same format
  as video memory.
*/
static void __vbeCopyBlockFast(unsigned long offset,uint8_t *image,unsigned long size)
{
  memcpy(&win.ptr[offset],image,size);
}

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

/*
  Copies frame to video memory. Data should be in the same format as video
  memory.
*/

#define PIXEL_SIZE() ((video_mode_info.BitsPerPixel+7)/8)
#define SCREEN_LINE_SIZE(pixel_size) (video_mode_info.XResolution*pixel_size) 
#define IMAGE_LINE_SIZE(pixel_size) (image_width*pixel_size)

static void __vbeCopyData(uint8_t *image)
{
   unsigned long i,j,image_offset,offset;
   unsigned pixel_size,image_line_size,screen_line_size,x_shift;
   pixel_size = PIXEL_SIZE();
   screen_line_size = SCREEN_LINE_SIZE(pixel_size);
   image_line_size = IMAGE_LINE_SIZE(pixel_size);
   if(image_width == video_mode_info.XResolution)
   {
     /* Special case for zooming */
     (*cpy_blk_fnc)(y_offset*screen_line_size,image,image_line_size*image_height);
   }
   else
   {
     x_shift = x_offset*pixel_size;
     for(j=0,i=y_offset;j<image_height;i++,j++)
     {
       offset = i*screen_line_size+x_shift;
       image_offset = j*image_line_size;
       (*cpy_blk_fnc)(offset,&image[image_offset],image_line_size);
     }
   }
}

/* is called for yuv only */
static uint32_t draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    if(verbose > 2)
	printf("vo_vesa: draw_slice was called: w=%u h=%u x=%u y=%u\n",w,h,x,y);
    if(vesa_zoom)
    {
	 uint8_t *dst[3]= {dga_buffer, NULL, NULL};
	 int dst_stride;
	 if(HAS_DGA()) dst[0] += y_offset*SCREEN_LINE_SIZE(PIXEL_SIZE())+x_offset*PIXEL_SIZE();
	 dst_stride = PIXEL_SIZE()*(HAS_DGA()?video_mode_info.XResolution:image_width);
	 SwScale_YV12slice(image,stride,y,h,dst,dst_stride,
			     image_width, video_mode_info.BitsPerPixel,
	    		     scale_xinc, scale_yinc);
    }
    else
    {
	uint8_t *yuv_slice;
	int rgb_stride;
	yuv_slice=dga_buffer;
	if(HAS_DGA()) yuv_slice += y_offset*SCREEN_LINE_SIZE(PIXEL_SIZE())+x_offset*PIXEL_SIZE();
	rgb_stride = HAS_DGA()?video_mode_info.XResolution:image_width;
	yuv_slice+=(rgb_stride*y+x)*PIXEL_SIZE();
	rgb_stride *= PIXEL_SIZE();
	yuv2rgb(yuv_slice, image[0], image[1], image[2], w, h,
		rgb_stride, stride[0], stride[1]);
    }
    flip_trigger = 1;
    return 0;
}

static void draw_alpha_32(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
   unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:image_width;
   vo_draw_alpha_rgb32(w,h,src,srca,stride,dga_buffer+4*(y0*dstride+x0),4*dstride);
}

static void draw_alpha_24(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
   unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:image_width;
   vo_draw_alpha_rgb24(w,h,src,srca,stride,dga_buffer+3*(y0*dstride+x0),3*dstride);
}

static void draw_alpha_16(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
   unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:image_width;
   vo_draw_alpha_rgb16(w,h,src,srca,stride,dga_buffer+2*(y0*dstride+x0),2*dstride);
}

static void draw_alpha_15(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
   unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:image_width;
   vo_draw_alpha_rgb15(w,h,src,srca,stride,dga_buffer+2*(y0*dstride+x0),2*dstride);
}

static void draw_alpha_null(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
}


static void draw_osd(void)
{
 uint32_t w,h;
 if(verbose > 2)
	printf("vo_vesa: draw_osd was called\n");
 w = HAS_DGA()?video_mode_info.XResolution:image_width;
 h = HAS_DGA()?video_mode_info.YResolution:image_height;
 if(dga_buffer) vo_draw_text(w,h,draw_alpha_fnc);
}

static void flip_page(void)
{
  if(verbose > 2)
	printf("vo_vesa: flip_page was called\n");
  if(flip_trigger) 
  {
    if(!HAS_DGA()) __vbeCopyData(dga_buffer);
    flip_trigger = 0;
  }
#if 0
  if(vo_doublebuffering && multi_size > 1)
  {
   vbeSetDisplayStart(multi_buff[multi_idx],1);
   multi_idx = multi_idx ? 0 : 1;
   win.ptr = dga_buffer = video_base + multi_buff[multi_idx];
  }
#endif
/*
  else
  if(tripple_buffering)
  {
   vbeSetScheduledDisplayStart(multi_buffer[multi_idx],1);
   multi_idx++;
   if(multi_idx > 2) multi_idx = 0;
   win.ptr = dga_buffer = video_base + multi_buffer[multi_idx];
  }
*/
}

/* is called for rgb only */
static uint32_t draw_frame(uint8_t *src[])
{
  uint8_t *data = src[0];
    if(verbose > 2)
        printf("vo_vesa: draw_frame was called\n");
    if(rgb2rgb_fnc)
    {
      if(HAS_DGA()) 
      {
	size_t i, psize, ssize, dsize;
	uint8_t *dest, *sptr;
	psize = PIXEL_SIZE();
	dsize = SCREEN_LINE_SIZE(psize);
	ssize = IMAGE_LINE_SIZE(image_bpp);
	dest = dga_buffer/* + y_offset*dsize + x_offset*psize*/;
	sptr = src[0];
	for(i=0;i<image_height;i++)
	{
	   (*rgb2rgb_fnc)(sptr,dest,ssize);
	   sptr += ssize;
	   dest += dsize;
	}
      }
      else
      {
	(*rgb2rgb_fnc)(src[0],dga_buffer,image_width*image_height*image_bpp);
	data = dga_buffer;
      }
      if(verbose > 2)
          printf("vo_vesa: rgb2rgb_fnc was called\n");
    } 
    if(!rgb2rgb_fnc || !HAS_DGA()) __vbeCopyData(data);
    return 0;
}

static uint32_t query_format(uint32_t format)
{
  uint32_t retval;
    if(verbose > 2)
        printf("vo_vesa: query_format was called: %x (%s)\n",format,vo_format_name(format));
	switch(format)
	{
		case IMGFMT_YV12:
#if 0 /* Should be tested better */
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

static void vesa_aspect(uint32_t width,uint32_t height,  
		    	uint32_t xres,uint32_t yres,
			uint32_t *image_width,uint32_t *image_height)
{
  float aspect_factor;
  aspect_factor = (float)width / height;
  *image_width = xres;
  *image_height = xres /aspect_factor;
  if(verbose) printf("vo_vesa: aspect factor = %f(%ux%u) *image=%ux%u screen=%ux%u\n",aspect_factor,width,height,*image_width,*image_height,xres,yres);
  if((*image_height) > yres)
  {
    *image_height = yres;
    *image_width = yres * aspect_factor;
    if(verbose) printf("vo_vesa: Y > X therefore *image=%ux%u\n",*image_width,*image_height);
  }
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

unsigned fillMultiBuffer( unsigned long vsize, unsigned nbuffs )
{
  unsigned long screen_size, offset;
  unsigned total,i;
  screen_size = video_mode_info.XResolution*video_mode_info.YResolution*((video_mode_info.BitsPerPixel+7)/8);
  if(screen_size%64) screen_size=((screen_size/64)*64)+64;
  total = vsize / screen_size;
  if(verbose) printf("vo_vesa: Can use %u video buffers\n",total);
  i = 0;
  offset = 0;
  total = min(total,nbuffs);
  while(i < total) { multi_buff[i++] = offset; offset += screen_size; }
  if(!i)
    printf("vo_vesa: Your have too small size of video memory for this mode:\n"
	   "vo_vesa: Requires: %08lX exists: %08lX\n", screen_size, vsize);
  return i;
}

#define SUBDEV_NODGA     0x00000001UL
#define SUBDEV_FORCEDGA  0x00000002UL
uint32_t parseSubDevice(const char *sd)
{
   uint32_t flags;
   flags = 0;
   if(strcmp(sd,"nodga") == 0) { flags |= SUBDEV_NODGA; flags &= ~(SUBDEV_FORCEDGA); }
   else
   if(strcmp(sd,"dga") == 0)   { flags &= ~(SUBDEV_NODGA); flags |= SUBDEV_FORCEDGA; }
   else if(verbose) printf("vo_vesa: Unknown subcommand: %s\n", sd);
   return flags;
}

/* fullscreen:
 * bit 0 (0x01) means fullscreen (-fs)
 * bit 1 (0x02) means mode switching (-vm)
 * bit 2 (0x04) enables software scaling (-zoom)
 * bit 3 (0x08) enables flipping (-flip) (NK: and for what?)
 */
static uint32_t
init(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
  struct VbeInfoBlock vib;
  struct VesaModeInfoBlock vmib;
  size_t i,num_modes;
  uint32_t w,h, sd_flags;
  unsigned short *mode_ptr,win_seg;
  unsigned bpp,best_x = UINT_MAX,best_y=UINT_MAX,best_mode_idx = UINT_MAX;
  int err,fs_mode,yuv_fmt;
	image_width = width;
	image_height = height;
	fs_mode = 0;
	rgb2rgb_fnc = NULL;
        sd_flags = 0;
        if(vo_subdevice) sd_flags = parseSubDevice(vo_subdevice);
	if(flags & 0x8)
	{
	  printf("vo_vesa: switch -flip is not supported\n");
	}
	if(flags & 0x04) vesa_zoom = 1;
	if(flags & 0x01)
	{
	  if(vesa_zoom) vesa_zoom = 2;
	  else          fs_mode = 1;
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
	yuv_fmt = 0;
	switch(format)
	{
		case IMGFMT_BGR8:
		case IMGFMT_RGB8:  bpp = 8; break;
		case IMGFMT_BGR15:
                case IMGFMT_RGB15: bpp = 15; break;
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV: yuv_fmt = 1;
		default:
		case IMGFMT_BGR16:
		case IMGFMT_RGB16: bpp = 16; break;
		case IMGFMT_BGR24:
		case IMGFMT_RGB24: bpp = 24; break;
		case IMGFMT_BGR32:
		case IMGFMT_RGB32: bpp = 32; break;
	}
	image_bpp = bpp;
	if(vo_dbpp) bpp = vo_dbpp;
	if(yuv_fmt) yuv2rgb_init(bpp, MODE_RGB);
	switch(bpp)
	{
	  case 15: draw_alpha_fnc = draw_alpha_15; break;
	  case 16: draw_alpha_fnc = draw_alpha_16; break;
	  case 24: draw_alpha_fnc = draw_alpha_24; break;
	  case 32: draw_alpha_fnc = draw_alpha_32; break;
	  default: draw_alpha_fnc = draw_alpha_null; break;
	}
	if(verbose)
	{
	  printf("vo_vesa: Requested mode: %ux%u@%u (%s)\n",width,height,bpp,vo_format_name(format));
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
	if(vesa_zoom)
	{
	    image_width = d_width;
	    image_height= d_height;
	}
	if(vo_screenwidth) w = vo_screenwidth;
	else w = max(image_width,width);
	if(vo_screenheight) h = vo_screenheight;
	else h = max(image_height,height);
        for(i=0;i < num_modes;i++)
	{
		if((err=vbeGetModeInfo(mode_ptr[i],&vmib)) != VBE_OK)
		{
			PRINT_VBE_ERR("vbeGetModeInfo",err);
			return -1;
		}
		if(vmib.XResolution >= w &&
		   vmib.YResolution >= h &&
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
		  printf("vo_vesa: Mode (%03u): mode=%04X %ux%u@%u attr=%04X\n"
			 "vo_vesa:             #planes=%u model=%u(%s) #pages=%u\n"
			 "vo_vesa:             winA=%X(attr=%u) winB=%X(attr=%u) winSize=%u winGran=%u\n"
			 "vo_vesa:             direct_color=%u DGA_phys_addr=%08lX\n"
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
		printf("vo_vesa: Using VESA mode (%u) = %x [%ux%u@%u]\n"
			,best_mode_idx,video_mode,video_mode_info.XResolution
			,video_mode_info.YResolution,video_mode_info.BitsPerPixel);
		if(sd_flags & SUBDEV_NODGA) video_mode_info.PhysBasePtr = 0;
		if( vesa_zoom || fs_mode )
		{
		  if( format==IMGFMT_YV12 )
		  {
		      /* software scale */
		      if(vesa_zoom > 1)
		      vesa_aspect(image_width,image_height,  
		    		  video_mode_info.XResolution,video_mode_info.YResolution,
				  &image_width,&image_height);
		      else
		      if(fs_mode)
		      {
			image_width = video_mode_info.XResolution;
			image_height = video_mode_info.YResolution;
			vesa_zoom = 1;
		      }
		      scale_xinc=(width << 16) / image_width - 2;  /* needed for proper rounding */
	    	      scale_yinc=(height << 16) / image_height + 2;
		      SwScale_Init();
		      if(verbose) printf("vo_vesa: Using SCALE\n");
		  }      
    		  else
		  {
		      printf("vo_vesa: Can't apply zooming to non YV12 formats\n");
		      return -1;
		  }
		}
		if(format != IMGFMT_YV12 && image_bpp != video_mode_info.BitsPerPixel)
		{
		  if(image_bpp == 24 && video_mode_info.BitsPerPixel == 32) rgb2rgb_fnc = rgb24to32;
		  else 
		  if(image_bpp == 32 && video_mode_info.BitsPerPixel == 24) rgb2rgb_fnc = rgb32to24;
		  else 
		  if(image_bpp == 15 && video_mode_info.BitsPerPixel == 16) rgb2rgb_fnc = rgb15to16;
		  else 
		  {
		    printf("vo_vesa: Can't convert %u to %u\n",image_bpp,video_mode_info.BitsPerPixel);
		    return -1;
		  }
		  printf("vo_vesa: using %u-bpp to %u-bpp sw convertor\n",image_bpp,video_mode_info.BitsPerPixel);
		}		
		if((video_mode_info.WinAAttributes & FRAME_MODE) == FRAME_MODE)
		   win.idx = 0; /* frame A */
		else
		if((video_mode_info.WinBAttributes & FRAME_MODE) == FRAME_MODE)
		   win.idx = 1; /* frame B */
		else win.idx = -2;
		/* Try use DGA instead */
		if(video_mode_info.PhysBasePtr && vib.TotalMemory && (video_mode_info.ModeAttributes & MODE_ATTR_LINEAR))
		{
		    void *lfb;
		    unsigned long vsize;
		    vsize = vib.TotalMemory*64*1024;
		    lfb = vbeMapVideoBuffer(video_mode_info.PhysBasePtr,vsize);
		    if(lfb == NULL)
		      printf("vo_vesa: Can't use DGA. Force bank switching mode. :(\n");
		    else
		    {
		      video_base = win.ptr = lfb;
		      win.low = 0UL;
		      win.high = vsize;
		      win.idx = -1; /* HAS_DGA() is on */
		      video_mode |= VESA_MODE_USE_LINEAR;
		      printf("vo_vesa: Using DGA (physical resources: %08lXh, %08lXh)"
			     ,video_mode_info.PhysBasePtr
			     ,vsize);
		      if(verbose) printf(" at %08lXh",(unsigned long)lfb);
		      printf("\n");
		      if(!(multi_size = fillMultiBuffer(vsize,2))) return -1;
		      if(vo_doublebuffering && multi_size < 2)
			printf("vo_vesa: Can't use double buffering: not enough video memory\n");
		    }
		}
		if(win.idx == -2)
		{
		   printf("vo_vesa: Can't find neither DGA nor relocatable window's frame.\n");
		   return -1;
		}
		if(!HAS_DGA())
		{
		  if(sd_flags & SUBDEV_FORCEDGA)
		  {
			printf("vo_vesa: you've forced DGA. Exiting\n");
			return -1;
		  }
		  if(!(win_seg = win.idx == 0 ? video_mode_info.WinASegment:video_mode_info.WinBSegment))
		  {
		    printf("vo_vesa: Can't find valid window address\n");
		    return -1;
		  }
		  win.ptr = PhysToVirtSO(win_seg,0);
		  win.low = 0L;
		  win.high= video_mode_info.WinSize*1024;
		  printf("vo_vesa: Using bank switching mode (physical resources: %08lXh, %08lXh)\n"
			 ,(unsigned long)win.ptr,(unsigned long)win.high);
		}
		if(video_mode_info.XResolution > image_width)
		    x_offset = (video_mode_info.XResolution - image_width) / 2;
		else x_offset = 0;
		if(video_mode_info.YResolution > image_height)
		    y_offset = (video_mode_info.YResolution - image_height) / 2;
		else y_offset = 0;
		if(verbose)
		  printf("vo_vesa: image: %ux%u screen = %ux%u x_offset = %u y_offset = %u\n"
			,image_width,image_height
			,video_mode_info.XResolution,video_mode_info.YResolution
			,x_offset,y_offset);
		if(HAS_DGA())
		{
		  dga_buffer = win.ptr; /* Trickly ;) */
		  cpy_blk_fnc = __vbeCopyBlockFast;
		}
		else
		{
		  cpy_blk_fnc = __vbeCopyBlock;
		  if(yuv_fmt || rgb2rgb_fnc)
		  {
#ifdef HAVE_MEMALIGN
		    if(!(dga_buffer = memalign(64,video_mode_info.XResolution*video_mode_info.YResolution*video_mode_info.BitsPerPixel)))
#else
		    if(!(dga_buffer = malloc(video_mode_info.XResolution*video_mode_info.YResolution*video_mode_info.BitsPerPixel)))
#endif
		    {
		      printf("vo_vesa: Can't allocate temporary buffer\n");
		      return -1;
		    }
		    if(verbose) printf("vo_vesa: dga emulator was allocated = %p\n",dga_buffer);
		  }
		}
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
		/* Below 'return -1' is impossible */
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
	/*if(1)*/
	{
	  int x;
	  x = video_mode_info.XCharSize ?
	      (video_mode_info.XResolution/video_mode_info.XCharSize)/2-strlen(title)/2 :
	      0;
	  if(x < 0) x = 0;
	  vbeWriteString(x,0,7,title);
	}
	return 0;
}

static const vo_info_t*
get_info(void)
{
    if(verbose > 2)
        printf("vo_vesa: get_info was called\n");
	return &vo_info;
}

static void
uninit(void)
{
    if(verbose > 2)
        printf("vo_vesa: uninit was called\n");
	vesa_term();
}


static void check_events(void)
{
    if(verbose > 2)
        printf("vo_vesa: check_events was called\n");
/* Nothing to do */
}
