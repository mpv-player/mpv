
#include "fastmemcpy.h"
#include "cpudetect.h"
#include "libswscale/swscale.h"
#include "libswscale/rgb2rgb.h"
#include "libmpcodecs/vf_scale.h"
#include "mp_msg.h"
#include "help_mp.h"

// mga_vid drawing functions
static void set_window( void );		/* forward declaration to kill warnings */
#ifdef VO_XMGA
static void mDrawColorKey( void );	/* forward declaration to kill warnings */
#ifdef HAVE_XINERAMA
extern int xinerama_screen;
#endif
#endif

static int mga_next_frame=0;

static mga_vid_config_t mga_vid_config;
static uint8_t *vid_data, *frames[4];
static int f = -1;

static uint32_t               drwX,drwY,drwWidth,drwHeight;
#ifdef VO_XMGA
static uint32_t               drwBorderWidth,drwDepth;
#endif
static uint32_t               drwcX,drwcY,dwidth,dheight;

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
    uint32_t bespitch = (mga_vid_config.src_width + 31) & ~31;
    x0+=mga_vid_config.src_width*(vo_panscan_x>>1)/(vo_dwidth+vo_panscan_x);
    switch(mga_vid_config.format){
    case MGA_VID_FORMAT_YV12:
    case MGA_VID_FORMAT_IYUV:
    case MGA_VID_FORMAT_I420:
        vo_draw_alpha_yv12(w,h,src,srca,stride,vid_data+bespitch*y0+x0,bespitch);
        break;
    case MGA_VID_FORMAT_YUY2:
        vo_draw_alpha_yuy2(w,h,src,srca,stride,vid_data+2*(bespitch*y0+x0),2*bespitch);
        break;
    case MGA_VID_FORMAT_UYVY:
        vo_draw_alpha_yuy2(w,h,src,srca,stride,vid_data+2*(bespitch*y0+x0)+1,2*bespitch);
        break;
    }
}

static void draw_osd(void)
{
//    vo_draw_text(mga_vid_config.src_width,mga_vid_config.src_height,draw_alpha);
    vo_draw_text(mga_vid_config.src_width-mga_vid_config.src_width*vo_panscan_x/(vo_dwidth+vo_panscan_x),mga_vid_config.src_height,draw_alpha);
}


//static void
//write_slice_g200(uint8_t *y,uint8_t *cr, uint8_t *cb,uint32_t slice_num)

static void
draw_slice_g200(uint8_t *image[], int stride[], int width,int height,int x,int y)
{
	uint8_t *dest;
	uint32_t bespitch = (mga_vid_config.src_width + 31) & ~31;

	dest = vid_data + bespitch*y + x;
	mem2agpcpy_pic(dest, image[0], width, height, bespitch, stride[0]);

        width/=2;height/=2;x/=2;y/=2;

	dest = vid_data + bespitch*mga_vid_config.src_height + bespitch*y + 2*x;

	interleaveBytes(image[1],image[2],dest,
		width, height,
		stride[1], stride[2], bespitch);
}

static void
draw_slice_g400(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *dest;
    uint8_t *dest2;
    uint32_t bespitch,bespitch2;

    bespitch = (mga_vid_config.src_width + 31) & ~31;
    bespitch2 = bespitch/2;

    dest = vid_data + bespitch * y + x;
    mem2agpcpy_pic(dest, image[0], w, h, bespitch, stride[0]);
    
    w/=2;h/=2;x/=2;y/=2;

    dest = vid_data + bespitch*mga_vid_config.src_height + bespitch2 * y + x;
    dest2= dest + bespitch2*mga_vid_config.src_height / 2; 

  if(mga_vid_config.format==MGA_VID_FORMAT_YV12){
    // mga_vid's YV12 assumes Y,U,V order (insteda of Y,V,U) :(
    mem2agpcpy_pic(dest, image[1], w, h, bespitch2, stride[1]);
    mem2agpcpy_pic(dest2,image[2], w, h, bespitch2, stride[2]);
  } else {
    mem2agpcpy_pic(dest, image[2], w, h, bespitch2, stride[2]);
    mem2agpcpy_pic(dest2,image[1], w, h, bespitch2, stride[1]);
  }

}

static int
draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{

#if 0
	printf("vo: %p/%d %p/%d %p/%d  %dx%d/%d;%d  \n",
	    src[0],stride[0],
	    src[1],stride[1],
	    src[2],stride[2],
	    w,h,x,y);
#endif

	if (mga_vid_config.card_type == MGA_G200)
            draw_slice_g200(src,stride,w,h,x,y);
	else
            draw_slice_g400(src,stride,w,h,x,y);
	return 0;
}

static void
vo_mga_flip_page(void)
{

//    printf("-- flip to %d --\n",mga_next_frame);

#if 1
	ioctl(f,MGA_VID_FSEL,&mga_next_frame);
	mga_next_frame=(mga_next_frame+1)%mga_vid_config.num_frames;
	vid_data=frames[mga_next_frame];
#endif

}

static int
draw_frame(uint8_t *src[])
{
    mp_msg(MSGT_VO,MSGL_WARN,"!!! mga::draw_frame() called !!!\n");
    return 0;
}

static uint32_t get_image(mp_image_t *mpi){
    uint32_t bespitch = (mga_vid_config.src_width + 31) & ~31;
    uint32_t bespitch2 = bespitch/2;
//    printf("mga: get_image() called\n");
    if(mpi->type==MP_IMGTYPE_STATIC && mga_vid_config.num_frames>1) return VO_FALSE; // it is not static
    if(mpi->flags&MP_IMGFLAG_READABLE) return VO_FALSE; // slow video ram
    if(mga_vid_config.card_type == MGA_G200 && mpi->flags&MP_IMGFLAG_PLANAR) return VO_FALSE;
//    printf("width=%d vs. bespitch=%d, flags=0x%X  \n",mpi->width,bespitch,mpi->flags);
    if((mpi->width==bespitch) ||
       (mpi->flags&(MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_ACCEPT_WIDTH))){
       // we're lucky or codec accepts stride => ok, let's go!
       if(mpi->flags&MP_IMGFLAG_PLANAR){
	   mpi->planes[0]=vid_data;
	   if(mpi->flags&MP_IMGFLAG_SWAPPED){
	       mpi->planes[1]=vid_data + bespitch*mga_vid_config.src_height;
	       mpi->planes[2]=mpi->planes[1] + bespitch2*mga_vid_config.src_height/2;
	   } else {
	       mpi->planes[2]=vid_data + bespitch*mga_vid_config.src_height;
	       mpi->planes[1]=mpi->planes[2] + bespitch2*mga_vid_config.src_height/2;
	   }
	   mpi->width=mpi->stride[0]=bespitch;
	   mpi->stride[1]=mpi->stride[2]=bespitch2;
       } else {
           mpi->planes[0]=vid_data;
	   mpi->width=bespitch;
	   mpi->stride[0]=mpi->width*(mpi->bpp/8);
       }
       mpi->flags|=MP_IMGFLAG_DIRECT;
//	printf("mga: get_image() SUCCESS -> Direct Rendering ENABLED\n");
       return VO_TRUE;
    }
    return VO_FALSE;
}

static uint32_t
draw_image(mp_image_t *mpi){
    uint32_t bespitch = (mga_vid_config.src_width + 31) & ~31;

    // if -dr or -slices then do nothing:
    if(mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK)) return VO_TRUE;

    if(mpi->flags&MP_IMGFLAG_PLANAR){
	// copy planar:
        draw_slice(mpi->planes,mpi->stride,mpi->w,mpi->h,mpi->x,mpi->y);
    } else {
	// copy packed:
	mem2agpcpy_pic(vid_data, mpi->planes[0],	// dst,src
		    mpi->w*(mpi->bpp/8), mpi->h,	// w,h
		    bespitch*2, mpi->stride[0]);	// dstride,sstride
    }
    return VO_TRUE;
}

static int
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD|VFCAP_HWSCALE_UP|VFCAP_HWSCALE_DOWN|VFCAP_ACCEPT_STRIDE;
    }
    return 0;
}

static void mga_fullscreen()
{
	uint32_t w,h;
	if ( !vo_fs ) {
		vo_fs=VO_TRUE;
		w=vo_screenwidth; h=vo_screenheight;
		aspect(&w,&h,A_ZOOM);
	} else {
		vo_fs=VO_FALSE;
		w=vo_dwidth; h=vo_dheight;
		aspect(&w,&h,A_NOZOOM);
	}
	mga_vid_config.dest_width = w;
	mga_vid_config.dest_height= h;
	mga_vid_config.x_org=(vo_screenwidth-w)/2;
	mga_vid_config.y_org=(vo_screenheight-h)/2;
	if ( ioctl( f,MGA_VID_CONFIG,&mga_vid_config ) )
		mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_MGA_ErrorInConfigIoctl );
}

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_GET_IMAGE:
    return get_image(data);
  case VOCTRL_DRAW_IMAGE:
    return draw_image(data);
  case VOCTRL_SET_EQUALIZER:
    {
     va_list ap;
     short value;
     uint32_t luma,prev;

     if ( strcmp( data,"brightness" ) && strcmp( data,"contrast" ) ) return VO_FALSE;

     if (ioctl(f,MGA_VID_GET_LUMA,&prev)) {
	perror("Error in mga_vid_config ioctl()");
    mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule);    
	return VO_FALSE;
     }

//     printf("GET: 0x%4X 0x%4X  \n",(prev>>16),(prev&0xffff));

     va_start(ap, data);
     value = va_arg(ap, int);
     va_end(ap);
     
//     printf("value: %d -> ",value);
     value=((value+100)*255)/200-128; // maps -100=>-128 and +100=>127
//     printf("%d  \n",value);

     if(!strcmp(data,"contrast"))
         luma = (prev&0xFFFF0000)|(value&0xFFFF);
     else
         luma = (prev&0xFFFF)|(value<<16);
     
     if (ioctl(f,MGA_VID_SET_LUMA,luma)) {
	perror("Error in mga_vid_config ioctl()");
        mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_MGA_CouldNotSetLumaValuesFromTheKernelModule);
	return VO_FALSE;
     }

     return VO_TRUE;
    }

  case VOCTRL_GET_EQUALIZER:
    {
     va_list ap;
     int * value;
     short val;
     uint32_t luma;
     
     if ( strcmp( data,"brightness" ) && strcmp( data,"contrast" ) ) return VO_FALSE;

     if (ioctl(f,MGA_VID_GET_LUMA,&luma)) {
	perror("Error in mga_vid_config ioctl()");
        mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule);
	return VO_FALSE;
     }
     
     if ( !strcmp( data,"contrast" ) )
	 val=(luma & 0xFFFF);
     else
	 val=(luma >> 16);
	 
     va_start(ap, data);
     value = va_arg(ap, int*);
     va_end(ap);

     *value = (val*200)/255;

     return VO_TRUE;
    }
														       
#ifndef VO_XMGA
  case VOCTRL_FULLSCREEN:
    if (vo_screenwidth && vo_screenheight)
	mga_fullscreen();
    else
	mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_MGA_ScreenWidthHeightUnknown);
    return VO_TRUE;
  case VOCTRL_GET_PANSCAN:
      if ( !vo_fs ) return VO_FALSE;
      return VO_TRUE;
#endif

#if defined( VO_XMGA ) && defined( HAVE_NEW_GUI )
  case VOCTRL_GUISUPPORT:
    return VO_TRUE;
#endif

#ifdef VO_XMGA
  case VOCTRL_ONTOP:
      vo_x11_ontop();
      return VO_TRUE;
  case VOCTRL_GET_PANSCAN:
      if ( !inited || !vo_fs ) return VO_FALSE;
      return VO_TRUE;
  case VOCTRL_FULLSCREEN:
      vo_x11_fullscreen();
      vo_panscan_amount=0;
    /* indended, fallthrough to update panscan on fullscreen/windowed switch */
#endif
  case VOCTRL_SET_PANSCAN:
      if ( vo_fs && ( vo_panscan != vo_panscan_amount ) ) // || ( !vo_fs && vo_panscan_amount ) )
       {
//        int old_y = vo_panscan_y;
	panscan_calc();
//        if ( old_y != vo_panscan_y ) 
	set_window();
       }
      return VO_TRUE;
  }
  return VO_NOTIMPL;
}


static int mga_init(int width,int height,unsigned int format){

        switch(format){
        case IMGFMT_YV12:
	    width+=width&1;height+=height&1;
	    mga_vid_config.frame_size = ((width + 31) & ~31) * height + (((width + 31) & ~31) * height) / 2;
            mga_vid_config.format=MGA_VID_FORMAT_I420; break;
        case IMGFMT_I420:
        case IMGFMT_IYUV:
	    width+=width&1;height+=height&1;
	    mga_vid_config.frame_size = ((width + 31) & ~31) * height + (((width + 31) & ~31) * height) / 2;
            mga_vid_config.format=MGA_VID_FORMAT_YV12; break;
        case IMGFMT_YUY2:
	    mga_vid_config.frame_size = ((width + 31) & ~31) * height * 2;
            mga_vid_config.format=MGA_VID_FORMAT_YUY2; break;
        case IMGFMT_UYVY:
	    mga_vid_config.frame_size = ((width + 31) & ~31) * height * 2;
            mga_vid_config.format=MGA_VID_FORMAT_UYVY; break;
        default: 
            mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_MGA_InvalidOutputFormat,format);
            return (-1);
        }

	mga_vid_config.src_width = width;
	mga_vid_config.src_height= height;
	if(!mga_vid_config.dest_width)
	    mga_vid_config.dest_width = width;
	if(!mga_vid_config.dest_height)
	    mga_vid_config.dest_height= height;

	mga_vid_config.colkey_on=0;
	
	mga_vid_config.num_frames=(vo_directrendering && !vo_doublebuffering)?1:3;
	mga_vid_config.version=MGA_VID_VERSION;

	if(width > 1024 && height > 1024)
	{
		mp_msg(MSGT_VO,MSGL_ERR, MGSTR_LIBVO_MGA_ResolutionTooHigh);
		return (-1);
	} else if(height <= 1024)
	{
		// try whether we have a G550
		int ret;
		if(ret = ioctl(f,MGA_VID_CONFIG,&mga_vid_config))
		{
			if(mga_vid_config.card_type != MGA_G550)
			{
				// we don't have a G550, so our resolution is too high
				mp_msg(MSGT_VO,MSGL_ERR, MGSTR_LIBVO_MGA_ResolutionTooHigh);
				return (-1);
			} else {
				// there is a deeper problem
				// we have a G550, but still couldn't configure mga_vid
				perror("Error in mga_vid_config ioctl()");
				mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_MGA_IncompatibleDriverVersion);
				return -1;
			}
			// if we arrived here, then we could successfully configure mga_vid
			// at this high resolution
		}
	} else {
		// configure mga_vid in case resolution is < 1024x1024 too
		if (ioctl(f,MGA_VID_CONFIG,&mga_vid_config))
		{
			perror("Error in mga_vid_config ioctl()");
			mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_MGA_IncompatibleDriverVersion);
			return -1;
		}
	}
	
	mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_MGA_UsingBuffers,mga_vid_config.num_frames);

	frames[0] = (char*)mmap(0,mga_vid_config.frame_size*mga_vid_config.num_frames,PROT_WRITE,MAP_SHARED,f,0);
	frames[1] = frames[0] + 1*mga_vid_config.frame_size;
	frames[2] = frames[0] + 2*mga_vid_config.frame_size;
	frames[3] = frames[0] + 3*mga_vid_config.frame_size;
	mga_next_frame = 0;
	vid_data = frames[mga_next_frame];

	//clear the buffer
	memset(frames[0],0x80,mga_vid_config.frame_size*mga_vid_config.num_frames);

#ifndef VO_XMGA
	ioctl(f,MGA_VID_ON,0);
#endif

  return 0;
}

static int mga_uninit(){
  if(f>=0){
	ioctl( f,MGA_VID_OFF,0 );
	munmap(frames[0],mga_vid_config.frame_size*mga_vid_config.num_frames);
	close(f);
	f = -1;
  }
  return 0;
}

static int preinit(const char *vo_subdevice)
{
  const char *devname=vo_subdevice?vo_subdevice:"/dev/mga_vid";
	sws_rgb2rgb_init(get_sws_cpuflags());

	f = open(devname,O_RDWR);
	if(f == -1)
	{
		perror("open");
		mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_MGA_CouldntOpen,devname);
		return(-1);
	}

#ifdef VO_XMGA
  if (!vo_init()) {
    close(f);
    return -1;
  }
#endif

  return 0;
}

static void set_window( void ){

#ifdef VO_XMGA
	 if ( WinID )
	  {
           XGetGeometry( mDisplay,vo_window,&mRoot,&drwX,&drwY,&drwWidth,&drwHeight,&drwBorderWidth,&drwDepth );
           mp_msg(MSGT_VO,MSGL_V,"[xmga] x: %d y: %d w: %d h: %d\n",drwX,drwY,drwWidth,drwHeight );
           drwX=0; drwY=0;
           XTranslateCoordinates( mDisplay,vo_window,mRoot,0,0,&drwcX,&drwcY,&mRoot );
           mp_msg(MSGT_VO,MSGL_V,"[xmga] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );

	  }
	  else
#endif
	  { drwX=drwcX=vo_dx; drwY=drwcY=vo_dy; drwWidth=vo_dwidth; drwHeight=vo_dheight; }

         aspect(&dwidth,&dheight,A_NOZOOM);
         if ( vo_fs )
          {
           aspect(&dwidth,&dheight,A_ZOOM);
           drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
           drwcX+=drwX;
           drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
           drwcY+=drwY;
           drwWidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
           drwHeight=(dheight > vo_screenheight?vo_screenheight:dheight);
           mp_msg(MSGT_VO,MSGL_V,"[xmga-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );
          }
	 vo_dwidth=drwWidth; vo_dheight=drwHeight;

#ifdef VO_XMGA
#ifdef HAVE_XINERAMA
		 if(XineramaIsActive(mDisplay))
		 {
		 	XineramaScreenInfo *screens;
		 	int num_screens;
		 	int i;

		 	screens = XineramaQueryScreens(mDisplay,&num_screens);

		 	/* find the screen we are on */
		 	i = 0;
		 	while(i<num_screens &&
		 	    ((screens[i].x_org < drwcX) ||
		 	     (screens[i].y_org < drwcY) ||
		 	     (screens[i].x_org + screens[i].width >= drwcX) ||
		 	     (screens[i].y_org + screens[i].height >= drwcY)))
		 	{
		 		i++;
		 	}

			if(i<num_screens)
			{
				/* save the screen we are on */
				xinerama_screen = i;
			} else {
				/* oops.. couldnt find the screen we are on
				 * because the upper left corner left the
				 * visual range. assume we are still on the
				 * same screen
				 */
				i = xinerama_screen;
		 	}

		 	/* set drwcX and drwcY to the right values */
		 	drwcX = drwcX - screens[i].x_org;
		 	drwcY = drwcY - screens[i].y_org;
		 	XFree(screens);
		 }

#endif

         mDrawColorKey();
#endif

         mga_vid_config.x_org=drwcX;
         mga_vid_config.y_org=drwcY;
         mga_vid_config.dest_width=drwWidth;
         mga_vid_config.dest_height=drwHeight;
	 if ( vo_panscan > 0.0f && vo_fs )
	  {
	   drwX-=vo_panscan_x>>1;
	   drwY-=vo_panscan_y>>1;
	   drwWidth+=vo_panscan_x;
	   drwHeight+=vo_panscan_y;

	   mga_vid_config.x_org-=vo_panscan_x>>1;
	   mga_vid_config.y_org-=vo_panscan_y>>1;
           mga_vid_config.dest_width=drwWidth;
           mga_vid_config.dest_height=drwHeight;
#ifdef VO_XMGA
	   mDrawColorKey();
#endif
	  }
	 if ( ioctl( f,MGA_VID_CONFIG,&mga_vid_config ) ) mp_msg(MSGT_VO,MSGL_WARN,"Error in mga_vid_config ioctl (wrong mga_vid.o version?)" );
}
