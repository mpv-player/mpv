
#include "fastmemcpy.h"

// mga_vid drawing functions

static int mga_next_frame=0;

static mga_vid_config_t mga_vid_config;
static uint8_t *vid_data, *frames[4];
static int f;

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
        src = image[0];
	for(h=0; h < height; h++) 
	{
		memcpy(dest, src, width);
		src += stride[0];
		dest += bespitch;
	}

        width/=2;height/=2;x/=2;y/=2;

	dest = vid_data + bespitch*mga_vid_config.src_height + bespitch*y + 2*x;
        src = image[1];
        src2 = image[2];
	for(h=0; h < height; h++)
	{
		for(w=0; w < width; w++)
		{
			dest[2*w+0] = src[w];
			dest[2*w+1] = src2[w];
		}
		dest += bespitch;
                src += stride[1];
                src2+= stride[2];
	}
}

static void
draw_slice_g400(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *src;
    uint8_t *dest;
    uint32_t bespitch,bespitch2;
    int i;

    bespitch = (mga_vid_config.src_width + 31) & ~31;
    bespitch2 = bespitch/2;

    dest = vid_data + bespitch * y + x;
    src = image[0];
    for(i=0;i<h;i++){
        memcpy(dest,src,w);
        src+=stride[0];
        dest += bespitch;
    }
    
    w/=2;h/=2;x/=2;y/=2;
    
    dest = vid_data + bespitch*mga_vid_config.src_height + bespitch2 * y + x;
    src = image[1];
    for(i=0;i<h;i++){
        memcpy(dest,src,w);
        src+=stride[1];
        dest += bespitch2;
    }

    dest = vid_data + bespitch*mga_vid_config.src_height
                    + bespitch*mga_vid_config.src_height / 4 
                    + bespitch2 * y + x;
    src = image[2];
    for(i=0;i<h;i++){
        memcpy(dest,src,w);
        src+=stride[2];
        dest += bespitch2;
    }

}

static uint32_t
draw_slice(uint8_t *src[], int stride[], int w,int h,int x,int y)
{
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
	uint8_t *dest;
	uint32_t bespitch,h;
        int len=2*mga_vid_config.src_width;

	dest = vid_data;
	bespitch = (mga_vid_config.src_width + 31) & ~31;
        
//        y+=2*mga_vid_config.src_width*mga_vid_config.src_height;

	for(h=0; h < mga_vid_config.src_height; h++) 
	{
//		y -= 2*mga_vid_config.src_width;
		memcpy(dest, y, len);
		y += len;
		dest += 2*bespitch;
	}
}


static uint32_t
draw_frame(uint8_t *src[])
{
    switch(mga_vid_config.format){
    case MGA_VID_FORMAT_YUY2:
        write_frame_yuy2(src[0]);break;
    case MGA_VID_FORMAT_UYVY:
        write_frame_yuy2(src[0]);break;
    }
    return 0;
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
        return 1;
    }
    return 0;
}

static int mga_init(){
	char *frame_mem;

	mga_vid_config.num_frames=4;
	mga_vid_config.version=MGA_VID_VERSION;
	if (ioctl(f,MGA_VID_CONFIG,&mga_vid_config))
	{
		perror("Error in mga_vid_config ioctl()");
                printf("Your mga_vid driver version is incompatible with this MPlayer version!\n");
		return -1;
	}
	ioctl(f,MGA_VID_ON,0);

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

