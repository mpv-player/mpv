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
    note: triple buffering requires VBE 3.0 - need volunteers.
  - refresh rate support (need additional info from mplayer)
*/
#include "config.h"

#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "video_out.h"
#include "video_out_internal.h"

#include "fastmemcpy.h"
#include "sub.h"
#include "linux/vbelib.h"
#include "bswap.h"
#include "aspect.h"
#include "vesa_lvo.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#endif

#include "../postproc/swscale.h"

LIBVO_EXTERN(vesa)

#ifdef HAVE_PNG
extern vo_functions_t video_out_png;
#endif

extern int verbose;

#define MAX_BUFFERS 3

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#define UNUSED(x) ((void)(x)) /**< Removes warning about unused arguments */

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

static uint32_t srcW=0,srcH=0,srcBpp,srcFourcc; /* source image description */
static uint32_t dstBpp,dstW, dstH,dstFourcc; /* destinition image description */

static SwsContext * sws = NULL;

static int32_t x_offset,y_offset; /* to center image on screen */
static unsigned init_mode; /* mode before run of mplayer */
static void *init_state = NULL; /* state before run of mplayer */
static struct win_frame win; /* real-mode window to video memory */
static uint8_t *dga_buffer = NULL; /* for yuv2rgb and sw_scaling */
static unsigned video_mode; /* selected video mode for playback */
static struct VesaModeInfoBlock video_mode_info;
static int flip_trigger = 0;
static void (*draw_alpha_fnc)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

/* multibuffering */
uint8_t*  video_base; /* should be never changed */
uint32_t  multi_buff[MAX_BUFFERS]; /* contains offsets of buffers */
uint8_t   multi_size=0; /* total number of buffers */
uint8_t   multi_idx=0; /* active buffer */

/* Linux Video Overlay */
static const char *lvo_name = NULL;
#ifdef CONFIG_VIDIX
static const char *vidix_name = NULL;
#endif

#define HAS_DGA()  (win.idx == -1)
#define MOVIE_MODE (MODE_ATTR_COLOR | MODE_ATTR_GRAPHICS)
#define FRAME_MODE (MODE_WIN_RELOCATABLE | MODE_WIN_WRITEABLE)

static char * vbeErrToStr(int err)
{
  char *retval;
  static char sbuff[80];
  if((err & VBE_VESA_ERROR_MASK) == VBE_VESA_ERROR_MASK)
  {
    sprintf(sbuff,"VESA failed = 0x4f%02x",(err & VBE_VESA_ERRCODE_MASK)>>8);
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
  if(lvo_name) vlvo_term();
#ifdef CONFIG_VIDIX
  else if(vidix_name) vidix_term();
#endif
  if((err=vbeRestoreState(init_state)) != VBE_OK) PRINT_VBE_ERR("vbeRestoreState",err);
  if((err=vbeSetMode(init_mode,NULL)) != VBE_OK) PRINT_VBE_ERR("vbeSetMode",err);
  if(HAS_DGA()) vbeUnmapVideoBuffer((unsigned long)win.ptr,win.high);
  if(dga_buffer && !HAS_DGA()) free(dga_buffer);
  vbeDestroy();
  if(sws) freeSwsContext(sws);
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
    vesa_term();
    PRINT_VBE_ERR("vbeSetWindow",err);
    printf("vo_vesa: Fatal error occured! Can't continue\n");
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
	int pixel_size = (dstBpp+7)/8;
	int bpl = video_mode_info.BytesPerScanLine;
	int color;
	unsigned offset;

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

#define PIXEL_SIZE() ((dstBpp+7)/8)
#define SCREEN_LINE_SIZE(pixel_size) (video_mode_info.XResolution*(pixel_size) )
#define IMAGE_LINE_SIZE(pixel_size) (dstW*(pixel_size))

static void __vbeCopyData(uint8_t *image)
{
   unsigned long i,j,image_offset,offset;
   unsigned pixel_size,image_line_size,screen_line_size,x_shift;
   pixel_size = PIXEL_SIZE();
   screen_line_size = SCREEN_LINE_SIZE(pixel_size);
   image_line_size = IMAGE_LINE_SIZE(pixel_size);
   if(dstW == video_mode_info.XResolution)
   {
     /* Special case for zooming */
     (*cpy_blk_fnc)(y_offset*screen_line_size,image,image_line_size*dstH);
   }
   else
   {
     x_shift = x_offset*pixel_size;
     for(j=0,i=y_offset;j<dstH;i++,j++)
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
    unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
    uint8_t *dst[3]= {dga_buffer, NULL, NULL};
    int dstStride[3];
    if(verbose > 2)
	printf("vo_vesa: draw_slice was called: w=%u h=%u x=%u y=%u\n",w,h,x,y);
    dstStride[0]=dstride*((dstBpp+7)/8);
    dstStride[1]=
    dstStride[2]=dstStride[0]>>1;
    if(HAS_DGA()) dst[0] += y_offset*SCREEN_LINE_SIZE(PIXEL_SIZE())+x_offset*PIXEL_SIZE();
    sws->swScale(sws,image,stride,y,h,dst,dstStride);
    flip_trigger = 1;
    return 0;
}

/* Please comment it out if you want have OSD within movie */
#define OSD_OUTSIDE_MOVIE 1

static void draw_alpha_32(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
   unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
#ifndef OSD_OUTSIDE_MOVIE
   if(HAS_DGA())
   {
	x0 += x_offset;
	y0 += y_offset;
   }
#endif
   vo_draw_alpha_rgb32(w,h,src,srca,stride,dga_buffer+4*(y0*dstride+x0),4*dstride);
}

static void draw_alpha_24(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
   unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
#ifndef OSD_OUTSIDE_MOVIE
   if(HAS_DGA())
   {
	x0 += x_offset;
	y0 += y_offset;
   }
#endif
   vo_draw_alpha_rgb24(w,h,src,srca,stride,dga_buffer+3*(y0*dstride+x0),3*dstride);
}

static void draw_alpha_16(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
   unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
#ifndef OSD_OUTSIDE_MOVIE
   if(HAS_DGA())
   {
	x0 += x_offset;
	y0 += y_offset;
   }
#endif
   vo_draw_alpha_rgb16(w,h,src,srca,stride,dga_buffer+2*(y0*dstride+x0),2*dstride);
}

static void draw_alpha_15(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
   unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
#ifndef OSD_OUTSIDE_MOVIE
   if(HAS_DGA())
   {
	x0 += x_offset;
	y0 += y_offset;
   }
#endif
   vo_draw_alpha_rgb15(w,h,src,srca,stride,dga_buffer+2*(y0*dstride+x0),2*dstride);
}

static void draw_alpha_null(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
  UNUSED(x0);
  UNUSED(y0);
  UNUSED(w);
  UNUSED(h);
  UNUSED(src);
  UNUSED(srca);
  UNUSED(stride);
}


static void draw_osd(void)
{
 uint32_t w,h;
 if(verbose > 2)
	printf("vo_vesa: draw_osd was called\n");
 {
#ifdef OSD_OUTSIDE_MOVIE
   w = HAS_DGA()?video_mode_info.XResolution:dstW;
   h = HAS_DGA()?video_mode_info.YResolution:dstH;
#else
   w = dstW;
   h = dstH;
#endif
   if(dga_buffer) vo_draw_text(w,h,draw_alpha_fnc);
 }
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
  if(vo_doublebuffering && multi_size > 1)
  {
    int err;
    if((err=vbeSetDisplayStart(multi_buff[multi_idx],vo_vsync)) != VBE_OK)
    {
      vesa_term();
      PRINT_VBE_ERR("vbeSetDisplayStart",err);
      printf("vo_vesa: Fatal error occured! Can't continue\n");
      exit(EXIT_FAILURE);
    }
    multi_idx = multi_idx ? 0 : 1;
    win.ptr = dga_buffer = video_base + multi_buff[multi_idx];
  }
/*
  else
  if(tripple_buffering)
  {
   vbeSetScheduledDisplayStart(multi_buff[multi_idx],vo_vsync);
   multi_idx++;
   if(multi_idx > 2) multi_idx = 0;
   win.ptr = dga_buffer = video_base + multi_buff[multi_idx];
  }
*/
}

/* is called for rgb only */
static uint32_t draw_frame(uint8_t *src[])
{
    if(verbose > 2)
        printf("vo_vesa: draw_frame was called\n");
    if(sws)
    {
	unsigned int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
	int srcStride[1];
	uint8_t *dst[3]= {dga_buffer, NULL, NULL};
	int dstStride[3];
	dstStride[0]=dstride*((dstBpp+7)/8);
	dstStride[1]=
	dstStride[2]=dstStride[0]>>1;
	if(srcFourcc == IMGFMT_RGB32 || srcFourcc == IMGFMT_BGR32)
	    srcStride[0] = srcW*4;
	else
	if(srcFourcc == IMGFMT_RGB24 || srcFourcc == IMGFMT_BGR24)
	    srcStride[0] = srcW*3;
	else
	    srcStride[0] = srcW*2;
	if(HAS_DGA()) dst[0] += y_offset*SCREEN_LINE_SIZE(PIXEL_SIZE())+x_offset*PIXEL_SIZE();
	sws->swScale(sws,src,srcStride,0,srcH,dst,dstStride);
	flip_trigger=1;
    }
    return 0;
}

#define SUBDEV_NODGA     0x00000001UL
#define SUBDEV_FORCEDGA  0x00000002UL
static uint32_t subdev_flags = 0xFFFFFFFEUL;
static uint32_t parseSubDevice(const char *sd)
{
   uint32_t flags;
   flags = 0;
   if(strcmp(sd,"nodga") == 0) { flags |= SUBDEV_NODGA; flags &= ~(SUBDEV_FORCEDGA); }
   else
   if(strcmp(sd,"dga") == 0)   { flags &= ~(SUBDEV_NODGA); flags |= SUBDEV_FORCEDGA; }
   else
   if(memcmp(sd,"lvo:",4) == 0) lvo_name = &sd[4]; /* lvo_name will be valid within init() */
#ifdef CONFIG_VIDIX
   else
   if(memcmp(sd,"vidix",5) == 0) vidix_name = &sd[5]; /* vidix_name will be valid within init() */
#endif
   else { printf("vo_vesa: Unknown subdevice: '%s'\n", sd); return 0xFFFFFFFFUL; }
   return flags;
}

static uint32_t query_format(uint32_t format)
{
    if(verbose > 2)
        printf("vo_vesa: query_format was called: %x (%s)\n",format,vo_format_name(format));
    return 1 | VFCAP_OSD | VFCAP_SWSCALE; /* due new SwScale code */
}

static void paintBkGnd( void )
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

static void clear_screen( void )
{
    int x_res = video_mode_info.XResolution;
    int y_res = video_mode_info.YResolution;
    int x, y;

    for (y = 0; y < y_res; ++y)
	for (x = 0; x < x_res; ++x)
	    __vbeSetPixel(x, y, 0, 0, 0);
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
  screen_size = video_mode_info.XResolution*video_mode_info.YResolution*((dstBpp+7)/8);
  if(screen_size%64) screen_size=((screen_size/64)*64)+64;
  total = vsize / screen_size;
  if(verbose) printf("vo_vesa: Can use up to %u video buffers\n",total);
  i = 0;
  offset = 0;
  total = min(total,nbuffs);
  while(i < total) { multi_buff[i++] = offset; offset += screen_size; }
  if(!i)
    printf("vo_vesa: Your have too small size of video memory for this mode:\n"
	   "vo_vesa: Requires: %08lX exists: %08lX\n", screen_size, vsize);
  return i;
}


/* fullscreen:
 * bit 0 (0x01) means fullscreen (-fs)
 * bit 1 (0x02) means mode switching (-vm)
 * bit 2 (0x04) enables software scaling (-zoom)
 * bit 3 (0x08) enables flipping (-flip) (NK: and for what?)
 */
static uint32_t
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format,const vo_tune_info_t *info)
{
  struct VbeInfoBlock vib;
  struct VesaModeInfoBlock vmib;
  size_t i,num_modes;
  uint32_t w,h;
  unsigned short *mode_ptr,win_seg;
  unsigned bpp,best_x = UINT_MAX,best_y=UINT_MAX,best_mode_idx = UINT_MAX;
  int err,fs_mode,use_scaler=0;
	srcW = dstW = width;
	srcH = dstH = height;
	fs_mode = 0;
        if(subdev_flags == 0xFFFFFFFEUL)
	{
	  printf("vo_vesa: detected internal fatal error: init is called before preinit\n");
	  return -1;
	}
	if(subdev_flags == 0xFFFFFFFFUL) return -1;
	if(flags & 0x8)
	{
	  printf("vo_vesa: switch -flip is not supported\n");
	}
	if(flags & 0x04) use_scaler = 1;
	if(flags & 0x01)
	{
	  if(use_scaler) use_scaler = 2;
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
	switch(format)
	{
		case IMGFMT_BGR8:
		case IMGFMT_RGB8:  bpp = 8; break;
		case IMGFMT_BGR15:
                case IMGFMT_RGB15: bpp = 15; break;
		case IMGFMT_BGR16:
		case IMGFMT_RGB16: bpp = 16; break;
		case IMGFMT_BGR24:
		case IMGFMT_RGB24: bpp = 24; break;
		case IMGFMT_BGR32:
		case IMGFMT_RGB32: bpp = 32; break;
		default:	   bpp = 16; break;
	}
	srcBpp = bpp;
	srcFourcc = format;
	if(vo_dbpp) bpp = vo_dbpp;
	switch(bpp)
	{
	  case 15: draw_alpha_fnc = draw_alpha_15;
		   dstFourcc = IMGFMT_BGR15;
		   break;
	  case 16: draw_alpha_fnc = draw_alpha_16;
		   dstFourcc = IMGFMT_BGR16;
		   break;
	  case 24: draw_alpha_fnc = draw_alpha_24;
		   dstFourcc = IMGFMT_BGR24;
		   break;
	  case 32: draw_alpha_fnc = draw_alpha_32;
		   dstFourcc = IMGFMT_BGR32;
		   break;
	  default: draw_alpha_fnc = draw_alpha_null;
		   dstFourcc = IMGFMT_BGR16;
		   break;
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
	if(use_scaler)
	{
	    dstW = d_width;
	    dstH = d_height;
	}
	if(vo_screenwidth) w = vo_screenwidth;
	else w = max(dstW,width);
	if(vo_screenheight) h = vo_screenheight;
	else h = max(dstH,height);
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
			,video_mode_info.YResolution,dstBpp);
		dstBpp = video_mode_info.BitsPerPixel;
		if(subdev_flags & SUBDEV_NODGA) video_mode_info.PhysBasePtr = 0;
		if(use_scaler || fs_mode)
		{
		      /* software scale */
		      if(use_scaler > 1)
		      {
		        aspect_save_orig(width,height);
			aspect_save_prescale(d_width,d_height);
			aspect_save_screenres(video_mode_info.XResolution,video_mode_info.YResolution);
			aspect(&dstW,&dstH,A_ZOOM);
		      }
		      else
		      if(fs_mode)
		      {
			dstW = video_mode_info.XResolution;
			dstH = video_mode_info.YResolution;
		      }
		      use_scaler = 1;
		}
		if(!lvo_name
#ifdef CONFIG_VIDIX
		&& !vidix_name
#endif
		) 
		{
		    sws = getSwsContextFromCmdLine(srcW,srcH,srcFourcc,dstW,dstH,dstFourcc);
		    if(!sws)
		    {
			printf("vo_vesa: Can't initialize SwScaler\n");
			return -1;
		    }
		    else if(verbose) printf("vo_vesa: Using SW BES emulator\n");
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
		  if(subdev_flags & SUBDEV_FORCEDGA)
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
		if(video_mode_info.XResolution > dstW)
		    x_offset = (video_mode_info.XResolution - dstW) / 2;
		else x_offset = 0;
		if(video_mode_info.YResolution > dstH)
		    y_offset = (video_mode_info.YResolution - dstH) / 2;
		else y_offset = 0;
		if(verbose)
		  printf("vo_vesa: image: %ux%u screen = %ux%u x_offset = %u y_offset = %u\n"
			,dstW,dstH
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
		  if(!lvo_name
#ifdef CONFIG_VIDIX
		   && !vidix_name
#endif
		  )
		  {
		    if(!(dga_buffer = memalign(64,video_mode_info.XResolution*video_mode_info.YResolution*dstBpp)))
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
		if(lvo_name)
		{
		  if(vlvo_init(width,height,x_offset,y_offset,dstW,dstH,format,dstBpp) != 0)
		  {
		    printf("vo_vesa: Can't initialize Linux Video Overlay\n");
		    lvo_name = NULL;
		    vesa_term();
		    return -1;
		  }
		  else printf("vo_vesa: Using video overlay: %s\n",lvo_name);
		}
#ifdef CONFIG_VIDIX
		else
		if(vidix_name)
		{
		  if(vidix_init(width,height,x_offset,y_offset,dstW,
				dstH,format,dstBpp,
				video_mode_info.XResolution,video_mode_info.YResolution,info) != 0)
		  {
		    printf("vo_vesa: Can't initialize VIDIX driver\n");
		    vidix_name = NULL;
		    vesa_term();
		    return -1;
		  }
		  else printf("vo_vesa: Using VIDIX\n");
		  vidix_start();
		}
#endif
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
	/* Clear screen for stupid BIOSes */
	clear_screen();
	if(HAS_DGA() && vo_doublebuffering)
	{
	    for(i=0;i<MAX_BUFFERS;i++)
	    {
		win.ptr = dga_buffer = video_base + multi_buff[i];
		if(verbose>1) paintBkGnd();
	    }
	}
	else
	{
	    if(verbose>1) paintBkGnd();
	    {
	        int x;
	        x = (video_mode_info.XResolution/video_mode_info.XCharSize)/2-strlen(title)/2;
	        if(x < 0) x = 0;
	        vbeWriteString(x,0,7,title);
	    }
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
    vesa_term();
    if(verbose > 2)
        printf("vo_vesa: uninit was called\n");
}


static void check_events(void)
{
    if(verbose > 2)
        printf("vo_vesa: check_events was called\n");
/* Nothing to do */
}

static uint32_t preinit(const char *arg)
{
  int pre_init_err = 0;
  if(verbose>1) printf("vo_vesa: preinit(%s) was called\n",arg);
  if(verbose > 2)
        printf("vo_vesa: subdevice %s is being initialized\n",arg);
  subdev_flags = 0;
  if(arg) subdev_flags = parseSubDevice(arg);
  if(lvo_name) pre_init_err = vlvo_preinit(lvo_name);
#ifdef CONFIG_VIDIX
  else if(vidix_name) pre_init_err = vidix_preinit(vidix_name,&video_out_vesa);
#endif
  if(verbose > 2)
        printf("vo_subdevice: initialization returns: %i\n",pre_init_err);
  return pre_init_err;
}

#ifdef HAVE_PNG
static int vesa_screenshot(const char *fname)
{
    uint32_t i,n;
    uint8_t *ptrs[video_mode_info.YResolution];
    if(video_out_png.preinit(NULL)) 
    {
	printf("\nvo_vesa: can't preinit vo_png\n");
	return 1;
    }
    if(!video_out_png.control(VOCTRL_QUERY_FORMAT, &dstFourcc))
    {
	printf("\nvo_vesa: vo_png doesn't support: %s fourcc\n",vo_format_name(dstFourcc));
	return 1;
    }
    if(video_out_png.config(HAS_DGA()?video_mode_info.XResolution:dstW,
			    HAS_DGA()?video_mode_info.YResolution:dstH,
			    HAS_DGA()?video_mode_info.XResolution:dstW,
			    HAS_DGA()?video_mode_info.YResolution:dstH,
			    0,NULL,dstFourcc,NULL))
    {
	printf("\nvo_vesa: can't configure vo_png\n");
	return 1;
    }
    n = HAS_DGA()?video_mode_info.YResolution:dstH;
    for(i=0;i<n;i++)
		ptrs[i] = &dga_buffer[(HAS_DGA()?video_mode_info.XResolution:dstW)*i*PIXEL_SIZE()];
    if(video_out_png.draw_frame(ptrs))
    {
	printf("\nvo_vesa: vo_png: error during dumping\n");
	return 1;
    }
    
    video_out_png.uninit();
    if(verbose) printf("\nvo_vesa: png output has been created\n");
    return 0;
}


static char _home_name[FILENAME_MAX + 1];
static char * __get_home_filename(const char *progname)
{
    char *p = getenv("HOME");

    if (p == NULL || strlen(p) < 2) {
	struct passwd *psw = getpwuid(getuid());
	if (psw != NULL) p = psw->pw_dir;
    }	

    if (p == NULL || strlen(p) > FILENAME_MAX - (strlen(progname) + 4))
	p = "/tmp";

    strcpy(_home_name, p);
    strcat(_home_name, "/.");
    return strcat(_home_name, progname);
}
#endif

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
#ifdef HAVE_PNG
  case VOCTRL_SCREENSHOT:
    return vesa_screenshot(__get_home_filename("mplayer_vesa_dump.png"));
    break;
#endif
  }
  return VO_NOTIMPL;
}
