
/*
 * video_out_dga.c, X11 interface
 *
 *
 * Copyright ( C ) 2001, Andreas Ackermann. All Rights Reserved.
 *
 * <acki@acki-netz.de>
 *
 * note well: 
 *   
 * o this is alpha
 * o covers only common video card formats
 * o works only on intel architectures
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

//#include "fastmemcpy.h"

#include "linux/keycodes.h"
#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "../postproc/rgb2rgb.h"

LIBVO_EXTERN( fsdga )

#include <X11/Xlib.h>
#include <X11/extensions/xf86dga.h>

#include "x11_common.h"

static vo_info_t vo_info =
{
        "FullScreen DGA ( Direct Graphic Access )",
        "fsdga",
        "A'rpi/ESP-team & Andreas Ackermann <acki@acki-netz.de>",
        ""
};

static int       vo_dga_width;           // bytes per line in framebuffer
static int       vo_dga_vp_width;        // visible pixels per line in framebuffer
static int       vo_dga_vp_height;       // visible lines in framebuffer
static int       vo_dga_is_running = 0; 
static int       vo_dga_src_width;       // width of video in pixels
static int       vo_dga_src_height;      // height of video in pixels
static int       vo_dga_bpp;             // bytes per pixel in framebuffer
static int       vo_dga_src_offset=0;    // offset in src
static int       vo_dga_vp_offset=0;   // offset in dest
static int       vo_dga_bytes_per_line;  // longwords per line to copy
static int       vo_dga_src_skip;        // bytes to skip after copying one line 
                                  // (not supported yet) in src
static int       vo_dga_vp_skip;       // dto. for dest 
static int       vo_dga_lines;         // num of lines to copy
static int       vo_dga_src_format;                                 

static unsigned char     *vo_dga_base;
static Display  *vo_dga_dpy;


#if defined (HAVE_SSE) || defined (HAVE_3DNOW)
#define movntq "movntq" // use this for processors that have SSE or 3Dnow
#else
#define movntq "movq" // for MMX-only processors
#endif


#define rep_movsl(dest, src, numwords, d_add, count) \
__asm__ __volatile__( \
" \
xfer:                     \n\t\
                  movl %%edx, %%ecx \n\t \
                  cld\n\t \
                  rep\n\t \
                  movsl \n\t\
                  add %%eax, %%edi \n\t\
                  dec %%ebx \n\t\
                  jnz xfer \n\t\
" \
                  : \
                  : "a" (d_add), "b" (count), "S" (src), "D" (dest), "d" (numwords) \
                  : "memory" )

#if 0
                  : "S" (src), "D" (dest), "c" (numwords) \
	  movq (%%eax), %%mm0      \n\t \
          add $64, %%edx            \n\t \
	  movq 8(%%eax), %%mm1     \n\t \
          add $64, %%eax            \n\t \
	  movq -48(%%eax), %%mm2   \n\t \
          movq %%mm0, -64(%%edx)   \n\t \
	  movq -40(%%eax), %%mm3   \n\t \
          movq %%mm1, -56(%%edx)   \n\t \
	  movq -32(%%eax), %%mm4   \n\t \
          movq %%mm2, -48(%%edx)   \n\t \
	  movq -24(%%eax), %%mm5   \n\t \
          movq %%mm3, -40(%%edx)   \n\t \
	  movq -16(%%eax), %%mm6   \n\t \
          movq %%mm4, -32(%%edx)   \n\t \
	  movq -8(%%eax), %%mm7    \n\t \
          movq %%mm5, -24(%%edx)   \n\t \
          movq %%mm6, -16(%%edx)   \n\t \
          dec %%ecx                \n\t \
          movq %%mm7, -8(%%edx)    \n\t \
          jnz xfer                  \n\t \

#endif

#define mmx_movsl(dest, src, numwords) \
__asm__ __volatile__(  \
" \
                                  \n\t \
xfer:                              \n\t \
	  movq (%%eax), %%mm0      \n\t \
          add $64, %%edx            \n\t \
	  movq 8(%%eax), %%mm1     \n\t \
          add $64, %%eax            \n\t \
	  movq -48(%%eax), %%mm2   \n\t \
          movq %%mm0, -64(%%edx)   \n\t \
	  movq -40(%%eax), %%mm3   \n\t \
          movq %%mm1, -56(%%edx)   \n\t \
	  movq -32(%%eax), %%mm4   \n\t \
          movq %%mm2, -48(%%edx)   \n\t \
	  movq -24(%%eax), %%mm5   \n\t \
          movq %%mm3, -40(%%edx)   \n\t \
	  movq -16(%%eax), %%mm6   \n\t \
          movq %%mm4, -32(%%edx)   \n\t \
	  movq -8(%%eax), %%mm7    \n\t \
          movq %%mm5, -24(%%edx)   \n\t \
          movq %%mm6, -16(%%edx)   \n\t \
          dec %%ecx                \n\t \
          movq %%mm7, -8(%%edx)    \n\t \
          jnz xfer                  \n\t \
             \
" \
      : \
      : "a" (src), "d" (dest), "c" (numwords) \
      :  "memory" )

     // src <= eax
     // dst <= edx
     // num <= ecx  
 
static uint32_t draw_frame( uint8_t *src[] ){

  int vp_skip = vo_dga_vp_skip;
  int lpl = vo_dga_bytes_per_line >> 2; 
  int numlines = vo_dga_lines;     

  char *s, *d;

  if( vo_dga_src_format==IMGFMT_YV12 ){
    // We'll never reach this point, because YV12 codecs always calls draw_slice
    printf("vo_dga: draw_frame() doesn't support IMGFMT_YV12 (yet?)\n");
  }else{
    s = *src;
    d = (&((char *)vo_dga_base)[vo_dga_vp_offset]);
    rep_movsl(d, s, lpl, vo_dga_vp_skip, numlines );
  }

  return 0;
}

static void check_events(void)
{
    int e=vo_x11_check_events(vo_dga_dpy);
}

static void draw_osd(void)
{
}

static void flip_page( void ){
  //  printf("vo_dga: In flippage\n");
}

static unsigned int pix_buf_y[4][2048];
static unsigned int pix_buf_uv[2][2048*2];
static int dga_srcypos=0;
static int dga_ypos=0;
static int dga_last_ypos=-1;
static unsigned int dga_xinc,dga_yinc,dga_xinc2;

static unsigned char clip_table[768];

static    int yuvtab_2568[256];
static    int yuvtab_3343[256];
static    int yuvtab_0c92[256];
static    int yuvtab_1a1e[256];
static    int yuvtab_40cf[256];


static uint32_t draw_slice( uint8_t *srcptr[],int stride[],
                            int w,int h,int x,int y )
{
  
  if(y==0){
      dga_srcypos=-2*dga_yinc;
      dga_ypos=-2;
      dga_last_ypos=-2;
  } // reset counters
  
  while(1){
    unsigned char *dest=vo_dga_base+(vo_dga_width * dga_ypos)*vo_dga_bpp;
    int y0=2+(dga_srcypos>>16);
    int y1=1+(dga_srcypos>>17);
    int yalpha=(dga_srcypos&0xFFFF)>>8;
    int yalpha1=yalpha^255;
    int uvalpha=((dga_srcypos>>1)&0xFFFF)>>8;
    int uvalpha1=uvalpha^255;
    unsigned int *buf0=pix_buf_y[y0&3];
    unsigned int *buf1=pix_buf_y[((y0+1)&3)];
    unsigned int *uvbuf0=pix_buf_uv[y1&1];
    unsigned int *uvbuf1=pix_buf_uv[(y1&1)^1];
    int i;

    if(y0>=y+h) break;

    dga_ypos++; dga_srcypos+=dga_yinc;

    if(dga_last_ypos!=y0){
      unsigned char *src=srcptr[0]+(y0-y)*stride[0];
      unsigned int xpos=0;
      dga_last_ypos=y0;
      // this loop should be rewritten in MMX assembly!!!!
      for(i=0;i<vo_dga_vp_width;i++){
	register unsigned int xx=xpos>>8;
        register unsigned int xalpha=xpos&0xFF;
	buf1[i]=(src[xx]*(xalpha^255)+src[xx+1]*xalpha);
	xpos+=dga_xinc;
      }
      if(!(y0&1)){
        unsigned char *src1=srcptr[1]+(y1-y/2)*stride[1];
        unsigned char *src2=srcptr[2]+(y1-y/2)*stride[2];
        xpos=0;
        // this loop should be rewritten in MMX assembly!!!!
        for(i=0;i<vo_dga_vp_width;i++){
	  register unsigned int xx=xpos>>8;
          register unsigned int xalpha=xpos&0xFF;
	  uvbuf1[i]=(src1[xx]*(xalpha^255)+src1[xx+1]*xalpha);
	  uvbuf1[i+2048]=(src2[xx]*(xalpha^255)+src2[xx+1]*xalpha);
	  xpos+=dga_xinc2;
        }
      }
      if(!y0) continue;
    }

    // this loop should be rewritten in MMX assembly!!!!
    for(i=0;i<vo_dga_vp_width;i++){
	// linear interpolation && yuv2rgb in a single step:
	int Y=yuvtab_2568[((buf0[i]*yalpha1+buf1[i]*yalpha)>>16)];
	int U=((uvbuf0[i]*uvalpha1+uvbuf1[i]*uvalpha)>>16);
	int V=((uvbuf0[i+2048]*uvalpha1+uvbuf1[i+2048]*uvalpha)>>16);
	dest[0]=clip_table[((Y + yuvtab_3343[U]) >>13)];
	dest[1]=clip_table[((Y + yuvtab_0c92[V] + yuvtab_1a1e[U]) >>13)];
	dest[2]=clip_table[((Y + yuvtab_40cf[V]) >>13)];
	dest+=vo_dga_bpp;
    }
  
  }
  

  return 0;
};

static const vo_info_t* get_info( void )
{ return &vo_info; }

static uint32_t query_format( uint32_t format )
{
 printf("vo_dga: query_format\n");

 if( !vo_init() ) return 0; // Can't open X11
 printf("Format: %lx\n", (unsigned long) format);

 if( format==IMGFMT_YV12 ) return VFCAP_CSP_SUPPORTED;
 if( ( format&IMGFMT_BGR_MASK )==IMGFMT_BGR && 
     ( format&0xFF )==vo_depthonscreen ) return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW;
 return 0;
}


static void
uninit(void)
{
  vo_dga_is_running = 0;
  XUngrabPointer (vo_dga_dpy, CurrentTime);
  XUngrabKeyboard (vo_dga_dpy, CurrentTime);
  XF86DGADirectVideo (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 0);
  XCloseDisplay(vo_dga_dpy);
}




static uint32_t config( uint32_t width,  uint32_t height,
                      uint32_t d_width,uint32_t d_height,
                      uint32_t fullscreen,char *title,uint32_t format,const vo_tune_info_t *info )
{

  int bank, ram;
  int x_off, y_off;

  if( vo_dga_is_running )return -1;

  if( !vo_init() ){
    printf("vo_dga: vo_init() failed!\n");
    return 0; 
  }

  if((vo_dga_dpy = XOpenDisplay(0))==NULL)
  {
    printf ("vo_dga: Can't open display\n");
    return 1;
  } 

  XF86DGAGetVideo (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 
                  (char **)&vo_dga_base, &vo_dga_width, &bank, &ram);
  XF86DGAGetViewPortSize (vo_dga_dpy, XDefaultScreen (vo_dga_dpy),
			  &vo_dga_vp_width, &vo_dga_vp_height);

  
  // do some more checkings here ...
  if( format==IMGFMT_YV12 ) 
    yuv2rgb_init( vo_depthonscreen, MODE_RGB );

  vo_dga_src_format = format;
  vo_dga_src_width = width;
  vo_dga_src_height = height;
  vo_dga_bpp = (vo_depthonscreen+7) >> 3;

  printf("vo_dga: bytes/line: %d, screen res: %dx%d, depth: %d, base: %p, bpp: %d\n", 
          vo_dga_width, vo_dga_vp_width, 
          vo_dga_vp_height, vo_depthonscreen, vo_dga_base,
          vo_dga_bpp);
  printf("vo_dga: video res: %dx%d\n", vo_dga_src_width, vo_dga_src_height);

  if(vo_dga_src_width > vo_dga_vp_width ||
     vo_dga_src_height > vo_dga_vp_height){
    printf("vo_dga: Sorry, video larger than viewport is not yet supported!\n");
    // ugly, do something nicer in the future ...
    return 1;
  }

  x_off = (vo_dga_vp_width - vo_dga_src_width)>>1; 
  y_off = (vo_dga_vp_height - vo_dga_src_height)>>1;

  vo_dga_bytes_per_line = vo_dga_src_width * vo_dga_bpp; // todo
  vo_dga_lines = vo_dga_src_height;                      // todo


  vo_dga_src_offset = 0;
  vo_dga_vp_offset = (y_off * vo_dga_width + x_off ) * vo_dga_bpp;

  vo_dga_vp_skip = (vo_dga_width - vo_dga_src_width) * vo_dga_bpp;  // todo
    
  printf("vo_dga: vp_off=%d, vp_skip=%d, bpl=%d\n", 
         vo_dga_vp_offset, vo_dga_vp_skip, vo_dga_bytes_per_line);


  
  XF86DGASetViewPort (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 0, 0);
  XF86DGADirectVideo (vo_dga_dpy, XDefaultScreen(vo_dga_dpy), 
                      XF86DGADirectGraphics | XF86DGADirectMouse | 
                      XF86DGADirectKeyb);
  
  XGrabKeyboard (vo_dga_dpy, DefaultRootWindow(vo_dga_dpy), True, 
                 GrabModeAsync,GrabModeAsync, CurrentTime);
  XGrabPointer (vo_dga_dpy, DefaultRootWindow(vo_dga_dpy), True, 
                ButtonPressMask,GrabModeAsync, GrabModeAsync, 
                None, None, CurrentTime);
   
  // now clear screen

  memset(vo_dga_base, 0, vo_dga_width * vo_dga_vp_height * vo_dga_bpp);  

  dga_yinc=(vo_dga_src_height<<16)/vo_dga_vp_height;
  dga_xinc=(vo_dga_src_width<<8)/vo_dga_vp_width;
  dga_xinc2=dga_xinc>>1;
  
  { int i;
    for(i=0;i<256;i++){
        clip_table[i]=0;
        clip_table[i+256]=i;
        clip_table[i+512]=255;
	yuvtab_2568[i]=(0x2568*(i-16))+(256<<13);
	yuvtab_3343[i]=0x3343*(i-128);
	yuvtab_0c92[i]=-0x0c92*(i-128);
	yuvtab_1a1e[i]=-0x1a1e*(i-128);
	yuvtab_40cf[i]=0x40cf*(i-128);
    }
  }
  
  vo_dga_is_running = 1;
  return 0;
}

#if 0
int vo_dga_query_event(void){

  XEvent  myevent;
  char    text[10];
  KeySym  mykey;
  int     retval = 0;
  int     i;

  if( vo_dga_is_running ){  
     if(XPending(vo_dga_dpy)>0)
      {
	XNextEvent(vo_dga_dpy, &myevent);
	switch (myevent.type)
	  {
	  case ButtonPress:
	    /* Reaktion auf Knopfdruck ---> Textausgabe an der 
	       Mauscursorposition */ 
	   
	    retval = 'q';
            break;
	  case KeyPress:
	    /* Reaktion auf Tastendruck --> Testen ob Taste == "q",
	       falls ja: Programmende */
	    i=XLookupString(&myevent, text, 10, &mykey, 0);

            if (mykey&0xff00 != 0) mykey=mykey&0x00ff + 256;
	
            switch ( mykey )
            {
	    case wsLeft:      retval=KEY_LEFT; break;
	    case wsRight:     retval=KEY_RIGHT; break;
	    case wsUp:        retval=KEY_UP; break;
	    case wsDown:      retval=KEY_DOWN; break;
	    case wsSpace:     retval=' '; break;
	    case wsEscape:    retval=KEY_ESC; break;
	    case wsEnter:     retval=KEY_ENTER; break;
	    case wsq:
	    case wsQ:         retval='q'; break;
	    case wsp:
	    case wsP:         retval='p'; break;
	    case wsMinus:
	    case wsGrayMinus: retval='-'; break;
	    case wsPlus:
	    case wsGrayPlus:  retval='+'; break;
	    }
	    break;
	  }
      }
  }
  return retval;
}
#endif

static uint32_t preinit(const char *arg)
{
    if(arg) 
    {
	printf("vo_fsdga: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
