
#include "fastmemcpy.h"
#include "../mmx_defs.h"
#include "../postproc/rgb2rgb.h"

// mga_vid drawing functions

static int mga_next_frame=0;

static mga_vid_config_t mga_vid_config;
static uint8_t *vid_data, *frames[4];
static int f = -1;

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
    int x,y;
    uint32_t bespitch = (mga_vid_config.src_width + 31) & ~31;
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

static int mga_set_video_eq( const vidix_video_eq_t *info)
{

	uint32_t luma;
	float factor = 256.0 / 2000;

	luma = ((int)(info->brightness * factor) << 16) +
		   ((int)(info->contrast * factor) & 0xFFFF);
	if (ioctl(f,MGA_VID_SET_LUMA,luma)) {
		perror("Error in mga_vid_config ioctl()");
                printf("Could not set luma values in the kernel module!\n");
		return -1;
	}
    return 0;

}

static int mga_get_video_eq( vidix_video_eq_t *info)
{

	uint32_t luma;
	float factor = 2000.0 / 256;

	if (ioctl(f,MGA_VID_GET_LUMA,&luma)) {
		perror("Error in mga_vid_config ioctl()");
                printf("Could not get luma values from the kernel module!\n");
		return -1;
	}
	info->brightness = (luma >> 16) * factor;
	info->cap |= VEQ_CAP_BRIGHTNESS;

	info->contrast = (luma & 0xFFFF) * factor;
	info->cap |= VEQ_CAP_CONTRAST;

    return 0;
}

//static void
//write_slice_g200(uint8_t *y,uint8_t *cr, uint8_t *cb,uint32_t slice_num)

static void
draw_slice_g200(uint8_t *image[], int stride[], int width,int height,int x,int y)
{
	uint8_t *src;
	uint8_t *src2;
	uint8_t *dest;
	uint32_t bespitch,h,w;

	bespitch = (mga_vid_config.src_width + 31) & ~31;

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
    uint8_t *src;
    uint8_t *dest;
    uint8_t *dest2;
    uint32_t bespitch,bespitch2;
    int i;

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

static uint32_t
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


static void
write_frame_yuy2(uint8_t *y)
{
        int len=2*mga_vid_config.src_width;
	uint32_t bespitch = (mga_vid_config.src_width + 31) & ~31;

	mem2agpcpy_pic(vid_data, y, len, mga_vid_config.src_height, 2*bespitch, len);
}

static uint32_t
draw_frame(uint8_t *src[])
{
    switch(mga_vid_config.format){
    case MGA_VID_FORMAT_YUY2:
    case MGA_VID_FORMAT_UYVY:
        write_frame_yuy2(src[0]);break;
    }
    return 0;
}

static uint32_t
get_image(mp_image_t *mpi){
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
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
//    case IMGFMT_RGB|24:
//    case IMGFMT_BGR|24:
        return 3 | VFCAP_OSD|VFCAP_HWSCALE_UP|VFCAP_HWSCALE_DOWN;
    }
    return 0;
}

static void query_vaa(vo_vaa_t *vaa)
{
  memset(vaa,0,sizeof(vo_vaa_t));
  vaa->get_video_eq = mga_get_video_eq;
  vaa->set_video_eq = mga_set_video_eq;
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
	if (vo_screenwidth && vo_screenheight) {
		mga_vid_config.x_org=(vo_screenwidth-w)/2;
		mga_vid_config.y_org=(vo_screenheight-h)/2;
	} else {
		mga_vid_config.x_org= 0;
		mga_vid_config.y_org= 0;
	}
	if ( ioctl( f,MGA_VID_CONFIG,&mga_vid_config ) )
		printf( "Error in mga_vid_config ioctl (wrong mga_vid.o version?)" );
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_VAA:
    query_vaa((vo_vaa_t*)data);
    return VO_TRUE;
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_GET_IMAGE:
    return get_image(data);
  case VOCTRL_FULLSCREEN:
#ifdef VO_XMGA
    vo_x11_fullscreen();
#else
    mga_fullscreen();
#endif
    return VO_TRUE;
#if defined( VO_XMGA ) && defined( HAVE_NEW_GUI )
  case VOCTRL_GUISUPPORT:
    return VO_TRUE;
#endif
  }
  return VO_NOTIMPL;
}


static int mga_init(){
	char *frame_mem;

	mga_vid_config.num_frames=(vo_directrendering && !vo_doublebuffering)?1:3;
	mga_vid_config.version=MGA_VID_VERSION;
	if (ioctl(f,MGA_VID_CONFIG,&mga_vid_config))
	{
		perror("Error in mga_vid_config ioctl()");
                printf("Your mga_vid driver version is incompatible with this MPlayer version!\n");
		return -1;
	}
	ioctl(f,MGA_VID_ON,0);
	
	printf("[mga] Using %d buffers.\n",mga_vid_config.num_frames);

	frames[0] = (char*)mmap(0,mga_vid_config.frame_size*mga_vid_config.num_frames,PROT_WRITE,MAP_SHARED,f,0);
	frames[1] = frames[0] + 1*mga_vid_config.frame_size;
	frames[2] = frames[0] + 2*mga_vid_config.frame_size;
	frames[3] = frames[0] + 3*mga_vid_config.frame_size;
	mga_next_frame = 0;
	vid_data = frames[mga_next_frame];

	//clear the buffer
	memset(frames[0],0x80,mga_vid_config.frame_size*mga_vid_config.num_frames);

  return 0;

}

static int mga_uninit(){
	ioctl( f,MGA_VID_OFF,0 );
	munmap(frames[0],mga_vid_config.frame_size*mga_vid_config.num_frames);
	close(f);
	f = -1;
}

static uint32_t preinit(const char *arg)
{
  char *devname=vo_subdevice?vo_subdevice:"/dev/mga_vid";

	f = open(devname,O_RDWR);
	if(f == -1)
	{
		perror("open");
		printf("Couldn't open %s\n",devname); 
		return(-1);
	}
  return 0;
}

