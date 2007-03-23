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
*/
#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "gtf.h"
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
#include <fcntl.h>

#include <vbe.h>

#include "video_out.h"
#include "video_out_internal.h"

#include "fastmemcpy.h"
#include "sub.h"
#include "libavutil/common.h"
#include "mpbswap.h"
#include "aspect.h"
#include "vesa_lvo.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#endif
#include "mp_msg.h"

#include "libswscale/swscale.h"
#include "libmpcodecs/vf_scale.h"


#ifdef HAVE_PNG
extern vo_functions_t video_out_png;
#endif

extern char *monitor_hfreq_str;
extern char *monitor_vfreq_str;
extern char *monitor_dotclock_str;

#define MAX_BUFFERS 3

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#define UNUSED(x) ((void)(x)) /**< Removes warning about unused arguments */

static vo_info_t info = 
{
	"VESA VBE 2.0 video output",
	"vesa",
	"Nick Kurshev <nickols_k@mail.ru>",
        "Requires ROOT privileges"
};

LIBVO_EXTERN(vesa)

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

static struct SwsContext * sws = NULL;

static int32_t x_offset,y_offset; /* to center image on screen */
static unsigned init_mode=0; /* mode before run of mplayer */
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
static int lvo_opened = 0;
#ifdef CONFIG_VIDIX
static const char *vidix_name = NULL;
static int vidix_opened = 0;
static vidix_grkey_t gr_key;
#endif

/* Neomagic TV out */
static int neomagic_tvout = 0;
static int neomagic_tvnorm = NEO_PAL;
 
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

#define PRINT_VBE_ERR(name,err) { mp_msg(MSGT_VO,MSGL_WARN, "vo_vesa: %s returns: %s\n",name,vbeErrToStr(err)); fflush(stdout); }

static void vesa_term( void )
{
  int err;
  if(lvo_opened) { vlvo_term();  lvo_opened = 0; }
#ifdef CONFIG_VIDIX
  else if(vidix_opened) { vidix_term();  vidix_opened = 0; }
#endif
  if(init_state) if((err=vbeRestoreState(init_state)) != VBE_OK) PRINT_VBE_ERR("vbeRestoreState",err);
  init_state=NULL;
  if(init_mode) if((err=vbeSetMode(init_mode,NULL)) != VBE_OK) PRINT_VBE_ERR("vbeSetMode",err);
  init_mode=0;
  if(HAS_DGA()) vbeUnmapVideoBuffer((unsigned long)win.ptr,win.high);
  if(dga_buffer && !HAS_DGA()) free(dga_buffer);
  vbeDestroy();
  if(sws) sws_freeContext(sws);
  sws=NULL;
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
    mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_FatalErrorOccurred);
    abort();
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
static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
    uint8_t *dst[3]= {dga_buffer, NULL, NULL};
    int dstStride[3];
    if( mp_msg_test(MSGT_VO,MSGL_DBG3) )
	mp_msg(MSGT_VO,MSGL_DBG3, "vo_vesa: draw_slice was called: w=%u h=%u x=%u y=%u\n",w,h,x,y);
    dstStride[0]=dstride*((dstBpp+7)/8);
    dstStride[1]=
    dstStride[2]=dstStride[0]>>1;
    if(HAS_DGA()) dst[0] += y_offset*SCREEN_LINE_SIZE(PIXEL_SIZE())+x_offset*PIXEL_SIZE();
    sws_scale_ordered(sws,image,stride,y,h,dst,dstStride);
    flip_trigger = 1;
    return 0;
}

/* Please comment it out if you want have OSD within movie */
/*#define OSD_OUTSIDE_MOVIE 1*/

static void draw_alpha_32(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
   int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
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
   int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
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
   int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
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
   int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
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
 if( mp_msg_test(MSGT_VO,MSGL_DBG3) )
	mp_msg(MSGT_VO,MSGL_DBG3, "vo_vesa: draw_osd was called\n");
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
  if( mp_msg_test(MSGT_VO,MSGL_DBG3) )
	mp_msg(MSGT_VO,MSGL_DBG3, "vo_vesa: flip_page was called\n");
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
      mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_FatalErrorOccurred);
      abort();
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
static int draw_frame(uint8_t *src[])
{
    if( mp_msg_test(MSGT_VO,MSGL_DBG3) )
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_vesa: draw_frame was called\n");
    if(sws)
    {
	int dstride=HAS_DGA()?video_mode_info.XResolution:dstW;
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
	sws_scale_ordered(sws,src,srcStride,0,srcH,dst,dstStride);
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
   if(strcmp(sd,"neotv_pal") == 0)   { neomagic_tvout = 1; neomagic_tvnorm = NEO_PAL; }
   else
   if(strcmp(sd,"neotv_ntsc") == 0)   { neomagic_tvout = 1; neomagic_tvnorm = NEO_NTSC; }
   else
   if(memcmp(sd,"lvo:",4) == 0) lvo_name = &sd[4]; /* lvo_name will be valid within init() */
#ifdef CONFIG_VIDIX
   else
   if(memcmp(sd,"vidix",5) == 0) vidix_name = &sd[5]; /* vidix_name will be valid within init() */
#endif
   else { mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_UnknownSubdevice, sd); return 0xFFFFFFFFUL; }
   return flags;
}

static int query_format(uint32_t format)
{
    if( mp_msg_test(MSGT_VO,MSGL_DBG3) )
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_vesa: query_format was called: %x (%s)\n",format,vo_format_name(format));
#ifdef CONFIG_VIDIX
    if(vidix_name)return(vidix_query_fourcc(format));
#endif
    if (format == IMGFMT_MPEGPES)
	return 0;
    // FIXME: this is just broken...
    return VFCAP_CSP_SUPPORTED | VFCAP_OSD | VFCAP_SWSCALE | VFCAP_ACCEPT_STRIDE; /* due new SwScale code */
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
  if( mp_msg_test(MSGT_VO,MSGL_V) )
    mp_msg(MSGT_VO,MSGL_V, "vo_vesa: Can use up to %u video buffers\n",total);
  i = 0;
  offset = 0;
  total = min(total,nbuffs);
  while(i < total) { multi_buff[i++] = offset; offset += screen_size; }
  if(!i)
    mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_YouHaveTooLittleVideoMemory, screen_size, vsize);
  return i;
}


static int set_refresh(unsigned x, unsigned y, unsigned mode,struct VesaCRTCInfoBlock *crtc_pass)
{
    unsigned pixclk;
    float H_freq;
    
    range_t *monitor_hfreq = NULL;
    range_t *monitor_vfreq = NULL;
    range_t *monitor_dotclock = NULL;

    monitor_hfreq = str2range(monitor_hfreq_str);
    monitor_vfreq = str2range(monitor_vfreq_str);
    monitor_dotclock = str2range(monitor_dotclock_str);
    
		if (!monitor_hfreq || !monitor_vfreq || !monitor_dotclock) {
			mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_YouHaveToSpecifyTheCapabilitiesOfTheMonitor);
			return 0;
		}

    H_freq = range_max(monitor_hfreq)/1000;
    
//    printf("H_freq MAX %f\n",H_freq);
    
    do
    {
    H_freq -= 0.01;
    GTF_calcTimings(x,y,H_freq,GTF_HF,0, 0,crtc_pass);		      
//    printf("PixelCLK %d\n",(unsigned)crtc_pass->PixelClock);    
    }
    while ( (!in_range(monitor_vfreq,crtc_pass->RefreshRate/100)||
	    !in_range(monitor_hfreq,H_freq*1000))&&(H_freq>0));
    
    pixclk = crtc_pass->PixelClock;
//    printf("PIXclk before %d\n",pixclk);
    vbeGetPixelClock(&mode,&pixclk); 
//    printf("PIXclk after %d\n",pixclk);
    GTF_calcTimings(x,y,pixclk/1000000,GTF_PF,0,0,crtc_pass);
//    printf("Flags: %x\n",(unsigned) crtc_pass->Flags);
/*    
    printf("hTotal %d\n",crtc_pass->hTotal);
    printf("hSyncStart %d\n",crtc_pass->hSyncStart);
    printf("hSyncEnd %d\n",crtc_pass->hSyncEnd);
    
    printf("vTotal %d\n",crtc_pass->vTotal);
    printf("vSyncStart %d\n",crtc_pass->vSyncStart);
    printf("vSyncEnd %d\n",crtc_pass->vSyncEnd);
    
    printf("RR %d\n",crtc_pass->RefreshRate);
    printf("PixelCLK %d\n",(unsigned)crtc_pass->PixelClock);*/
    
    if (!in_range(monitor_vfreq,crtc_pass->RefreshRate/100)||
	!in_range(monitor_hfreq,H_freq*1000)) {
        mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_UnableToFitTheMode);
	return 0;
    }

    return 1;
}

/* fullscreen:
 * bit 0 (0x01) means fullscreen (-fs)
 * bit 1 (0x02) means mode switching (-vm)
 * bit 2 (0x04) enables software scaling (-zoom)
 * bit 3 (0x08) enables flipping (-flip) (NK: and for what?)
 */

static int
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
  static struct VbeInfoBlock vib;
  static int vib_set;
  struct VesaModeInfoBlock vmib;
  struct VesaCRTCInfoBlock crtc_pass;
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
	  mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_VESA_DetectedInternalFatalError);
	  return -1;
	}
	if(subdev_flags == 0xFFFFFFFFUL) return -1;
	if(flags & VOFLAG_FLIPPING)
	{
	  mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_SwitchFlipIsNotSupported);
	}
	if(flags & VOFLAG_SWSCALE) use_scaler = 1;
	if(flags & VOFLAG_FULLSCREEN)
	{
	  if(use_scaler) use_scaler = 2;
	  else          fs_mode = 1;
	} 
	if((err=vbeInit()) != VBE_OK) { PRINT_VBE_ERR("vbeInit",err); return -1; }
	memcpy(vib.VESASignature,"VBE2",4);
	if(!vib_set && (err=vbeGetControllerInfo(&vib)) != VBE_OK)
	{
	  PRINT_VBE_ERR("vbeGetControllerInfo",err);
	  mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_VESA_PossibleReasonNoVbe2BiosFound);
	  return -1;
	}
	vib_set = 1;
	/* Print general info here */
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_FoundVesaVbeBiosVersion,
		(int)(vib.VESAVersion >> 8) & 0xff,
		(int)(vib.VESAVersion & 0xff),
		(int)(vib.OemSoftwareRev & 0xffff));
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_VideoMemory,vib.TotalMemory*64);
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_Capabilites
		,vib.Capabilities & VBE_DAC_8BIT ? "8-bit DAC," : "6-bit DAC,"
		,vib.Capabilities & VBE_NONVGA_CRTC ? "non-VGA CRTC,":"VGA CRTC,"
		,vib.Capabilities & VBE_SNOWED_RAMDAC ? "snowed RAMDAC,":"normal RAMDAC,"
		,vib.Capabilities & VBE_STEREOSCOPIC ? "stereoscopic,":"no stereoscopic,"
		,vib.Capabilities & VBE_STEREO_EVC ? "Stereo EVC":"no stereo");
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_BelowWillBePrintedOemInfo);
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_YouShouldSee5OemRelatedLines);
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_OemInfo,vib.OemStringPtr);
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_OemRevision,vib.OemSoftwareRev);
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_OemVendor,vib.OemVendorNamePtr);
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_OemProductName,vib.OemProductNamePtr);
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_OemProductRev,vib.OemProductRevPtr);
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_Hint);
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
	if( mp_msg_test(MSGT_VO,MSGL_V) )
	{
	  mp_msg(MSGT_VO,MSGL_V, "vo_vesa: Requested mode: %ux%u@%u (%s)\n",width,height,bpp,vo_format_name(format));
	  mp_msg(MSGT_VO,MSGL_V, "vo_vesa: Total modes found: %u\n",num_modes);
	  mode_ptr = vib.VideoModePtr;
	  mp_msg(MSGT_VO,MSGL_V, "vo_vesa: Mode list:");
	  for(i = 0;i < num_modes;i++)
	  {
	    mp_msg(MSGT_VO,MSGL_V, " %04X",mode_ptr[i]);
	  }
	  mp_msg(MSGT_VO,MSGL_V, "\nvo_vesa: Modes in detail:\n");
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
			continue;
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
		if( mp_msg_test(MSGT_VO,MSGL_V) )
		{
		  mp_msg(MSGT_VO,MSGL_V, "vo_vesa: Mode (%03u): mode=%04X %ux%u@%u attr=%04X\n"
			 "vo_vesa:             #planes=%u model=%u(%s) #pages=%u\n"
			 "vo_vesa:             winA=%X(attr=%u) winB=%X(attr=%u) winSize=%u winGran=%u\n"
			 "vo_vesa:             direct_color=%u DGA_phys_addr=%08lX\n"
			 ,i,mode_ptr[i],vmib.XResolution,vmib.YResolution,vmib.BitsPerPixel,vmib.ModeAttributes
			 ,vmib.NumberOfPlanes,vmib.MemoryModel,model2str(vmib.MemoryModel),vmib.NumberOfImagePages
			 ,vmib.WinASegment,vmib.WinAAttributes,vmib.WinBSegment,vmib.WinBAttributes,vmib.WinSize,vmib.WinGranularity
			 ,vmib.DirectColorModeInfo,vmib.PhysBasePtr);
		  if(vmib.MemoryModel == 6 || vmib.MemoryModel == 7)
			mp_msg(MSGT_VO,MSGL_V, "vo_vesa:             direct_color_info = %u:%u:%u:%u\n"
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
		if( mp_msg_test(MSGT_VO,MSGL_V) ) {
			mp_msg(MSGT_VO,MSGL_V, "vo_vesa: Initial video mode: %x\n",init_mode); }
		if((err=vbeGetModeInfo(video_mode,&video_mode_info)) != VBE_OK)
		{
			PRINT_VBE_ERR("vbeGetModeInfo",err);
			return -1;
		}
		dstBpp = video_mode_info.BitsPerPixel;
 		mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_UsingVesaMode
			,best_mode_idx,video_mode,video_mode_info.XResolution
			,video_mode_info.YResolution,dstBpp);
		if(subdev_flags & SUBDEV_NODGA) video_mode_info.PhysBasePtr = 0;
		if(use_scaler || fs_mode)
		{
		      /* software scale */
		      if(use_scaler > 1
#ifdef CONFIG_VIDIX
				|| vidix_name
#endif			  
			  )
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
		    sws = sws_getContextFromCmdLine(srcW,srcH,srcFourcc,dstW,dstH,dstFourcc);
		    if(!sws)
		    {
 			mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_CantInitializeSwscaler);
			return -1;
		    }
		    else if( mp_msg_test(MSGT_VO,MSGL_V) ) {
			mp_msg(MSGT_VO,MSGL_V, "vo_vesa: Using SW BES emulator\n"); }
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
		      mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_CantUseDga);
		    else
		    {
		      video_base = win.ptr = lfb;
		      win.low = 0UL;
		      win.high = vsize;
		      win.idx = -1; /* HAS_DGA() is on */
		      video_mode |= VESA_MODE_USE_LINEAR;
 		      mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_UsingDga
			     ,video_mode_info.PhysBasePtr
			     ,vsize);
		      if( mp_msg_test(MSGT_VO,MSGL_V) ) {
			printf(" at %08lXh",(unsigned long)lfb); }
		      printf("\n");
		      if(!(multi_size = fillMultiBuffer(vsize,2))) return -1;
		      if(vo_doublebuffering && multi_size < 2)
 			mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_CantUseDoubleBuffering);
		    }
		}
		if(win.idx == -2)
		{
 		   mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_VESA_CantFindNeitherDga);
		   return -1;
		}
		if(!HAS_DGA())
		{
		  if(subdev_flags & SUBDEV_FORCEDGA)
		  {
 			mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_VESA_YouveForcedDga);
			return -1;
		  }
		  if(!(win_seg = win.idx == 0 ? video_mode_info.WinASegment:video_mode_info.WinBSegment))
		  {
 		    mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_VESA_CantFindValidWindowAddress);
		    return -1;
		  }
		  win.ptr = PhysToVirtSO(win_seg,0);
		  win.low = 0L;
		  win.high= video_mode_info.WinSize*1024;
 		  mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_UsingBankSwitchingMode
			 ,(unsigned long)win.ptr,(unsigned long)win.high);
		}
		if(video_mode_info.XResolution > dstW)
		    x_offset = (video_mode_info.XResolution - dstW) / 2;
		else x_offset = 0;
		if(video_mode_info.YResolution > dstH)
		    y_offset = (video_mode_info.YResolution - dstH) / 2;
		else y_offset = 0;
		if( mp_msg_test(MSGT_VO,MSGL_V) )
 		  mp_msg(MSGT_VO,MSGL_V, "vo_vesa: image: %ux%u screen = %ux%u x_offset = %u y_offset = %u\n"
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
 		      mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_VESA_CantAllocateTemporaryBuffer);
		      return -1;
		    }
		    if( mp_msg_test(MSGT_VO,MSGL_V) ) {
			mp_msg(MSGT_VO,MSGL_V, "vo_vesa: dga emulator was allocated = %p\n",dga_buffer); }
		  }
		}
		if((err=vbeSaveState(&init_state)) != VBE_OK)
		{
			PRINT_VBE_ERR("vbeSaveState",err);
		}
		
		/* TODO: 
		         user might pass refresh value,
			 GTF constants might be read from monitor
			 for best results, I don't have a spec (RM)
		*/
		
		if (((int)(vib.VESAVersion >> 8) & 0xff) > 2) {
		
		if (set_refresh(video_mode_info.XResolution,video_mode_info.YResolution,video_mode,&crtc_pass))
		video_mode = video_mode | 0x800;
		
		}
		
		;
		
		if ((err=vbeSetMode(video_mode,&crtc_pass)) != VBE_OK)
		{
			PRINT_VBE_ERR("vbeSetMode",err);
			return -1;
		}
		
		if (neomagic_tvout) {
		    err = vbeSetTV(video_mode,neomagic_tvnorm);
		    if (err!=0x4f) {
 		    mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_VESA_SorryUnsupportedMode);
		    }
		    else {
 		    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_OhYouReallyHavePictureOnTv);
		    } 
		}
		/* Now we are in video mode!!!*/
		/* Below 'return -1' is impossible */
		if( mp_msg_test(MSGT_VO,MSGL_V) )
		{
		  mp_msg(MSGT_VO,MSGL_V, "vo_vesa: Graphics mode was activated\n");
		  fflush(stdout);
		}
		if(lvo_name)
		{
		  if(vlvo_init(width,height,x_offset,y_offset,dstW,dstH,format,dstBpp) != 0)
		  {
 		    mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_VESA_CantInitialozeLinuxVideoOverlay);
		    vesa_term();
		    return -1;
		  }
		  else mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_UsingVideoOverlay,lvo_name);
		  lvo_opened = 1;
		}
#ifdef CONFIG_VIDIX
		else
		if(vidix_name)
		{
		  if(vidix_init(width,height,x_offset,y_offset,dstW,
				dstH,format,dstBpp,
				video_mode_info.XResolution,video_mode_info.YResolution) != 0)
		  {
		    mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_VESA_CantInitializeVidixDriver);
		    vesa_term();
		    return -1;
		  }
		  else mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_UsingVidix);
		  vidix_start();

		  /* set colorkey */       
		  if (vidix_grkey_support())
		  {
		    vidix_grkey_get(&gr_key);
		    gr_key.key_op = KEYS_PUT;
#if 0
		    if (!(vo_colorkey & 0xFF000000))
		    {
			gr_key.ckey.op = CKEY_TRUE;
			gr_key.ckey.red = (vo_colorkey & 0x00FF0000) >> 16;
			gr_key.ckey.green = (vo_colorkey & 0x0000FF00) >> 8;
			gr_key.ckey.blue = vo_colorkey & 0x000000FF;
		    } else
#endif
			gr_key.ckey.op = CKEY_FALSE;
		    vidix_grkey_set(&gr_key);
		  }         
		  vidix_opened = 1;
		}
#endif
	}
	else
	{
 	  mp_msg(MSGT_VO,MSGL_ERR, MSGTR_LIBVO_VESA_CantFindModeFor,width,height,bpp);
	  return -1;
	}
	if( mp_msg_test(MSGT_VO,MSGL_V) )
	{
	  mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_VESA_InitializationComplete);
	  fflush(stdout);
	}
	if(HAS_DGA() && vo_doublebuffering)
	{
            if (VBE_OK != vbeSetDisplayStart(0, vo_vsync))
            {
              mp_msg(MSGT_VO,MSGL_WARN, "[VO_VESA] Can't use double buffering: changing displays failed.\n");
              multi_size = 1;
            }
	    for(i=0;i<multi_size;i++)
	    {
		win.ptr = dga_buffer = video_base + multi_buff[i];
                clear_screen();	/* Clear screen for stupid BIOSes */
		if( mp_msg_test(MSGT_VO,MSGL_DBG2) ) paintBkGnd();
	    }
	}
	else
	{
            clear_screen();	/* Clear screen for stupid BIOSes */
	    if( mp_msg_test(MSGT_VO,MSGL_DBG2) )
	    {
	        int x;
	        x = (video_mode_info.XResolution/video_mode_info.XCharSize)/2-strlen(title)/2;
	        if(x < 0) x = 0;
	        paintBkGnd();
	        vbeWriteString(x,0,7,title);
	    }
	}
	return 0;
}

static void
uninit(void)
{
    // not inited
    vesa_term();
    if( mp_msg_test(MSGT_VO,MSGL_DBG3) )
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_vesa: uninit was called\n");
}


static void check_events(void)
{
    if( mp_msg_test(MSGT_VO,MSGL_DBG3) )
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_vesa: check_events was called\n");
/* Nothing to do */
}

static int preinit(const char *arg)
{
  int pre_init_err = 0;
  int fd;
  if( mp_msg_test(MSGT_VO,MSGL_DBG2) )
        mp_msg(MSGT_VO,MSGL_DBG2, "vo_vesa: preinit(%s) was called\n",arg);
  if( mp_msg_test(MSGT_VO,MSGL_DBG3) )
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_vesa: subdevice %s is being initialized\n",arg);
  subdev_flags = 0;
  lvo_name = NULL;
#ifdef CONFIG_VIDIX
  vidix_name = NULL;
#endif
  if(arg) subdev_flags = parseSubDevice(arg);
  if(lvo_name) pre_init_err = vlvo_preinit(lvo_name);
#ifdef CONFIG_VIDIX
  else if(vidix_name) pre_init_err = vidix_preinit(vidix_name,&video_out_vesa);
#endif
  // check if we can open /dev/mem (it will be opened later in config(), but if we
  // detect now that we can't we can exit cleanly)
  fd = open("/dev/mem", O_RDWR);
  if (fd < 0)
  	return -1;
  else
  	close(fd);
  if( mp_msg_test(MSGT_VO,MSGL_DBG3) )
        mp_msg(MSGT_VO,MSGL_DBG3, "vo_subdevice: initialization returns: %i\n",pre_init_err);
  return pre_init_err;
}

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }

#ifdef CONFIG_VIDIX
  if (vidix_name) {
    switch (request) {
    case VOCTRL_SET_EQUALIZER:
    {
      va_list ap;
      int value;
    
      va_start(ap, data);
      value = va_arg(ap, int);
      va_end(ap);

      return vidix_control(request, data, (int *)value);
    }
    case VOCTRL_GET_EQUALIZER:
    {
      va_list ap;
      int *value;
    
      va_start(ap, data);
      value = va_arg(ap, int*);
      va_end(ap);

      return vidix_control(request, data, value);
    }
    }
    return vidix_control(request, data);
  }
#endif

  return VO_NOTIMPL;
}
