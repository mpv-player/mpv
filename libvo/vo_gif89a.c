/* 
 * vo_gif89a.c  Generate gif89a output in file out.gif
 *
 * Originally based on vo_png.c
 *
 * Stolen (C) 2002 by GifWhore <joey@yunamusic.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"

#include "../postproc/rgb2rgb.h"

#include <gif_lib.h>

#define GIFWHORE_version 0.90

LIBVO_EXTERN (gif89a)

static vo_info_t vo_info = 
{
	"GIF89a (out.gif)",
	"gif89a",
	"GifWhore <joey@yunamusic.com>",
	""
};

extern int verbose;
extern int vo_config_count;

static int image_width;
static int image_height;
static int image_format;
static uint8_t *image_data=NULL;
static unsigned int scale_srcW = 0, scale_srcH = 0;

static int reverse_map = 0;
static unsigned char framenum = 0;
static int gif_frameskip;
static int gif_framedelay;
static int target_fps = 0;

GifFileType *newgif=NULL;

/*
 * TODO
 * OSD!!!
 */

static uint32_t config
	(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, 
		uint32_t fullscreen, char *title, uint32_t format, const vo_tune_info_t *info) {
    char filename[] = "out.gif";
    ColorMapObject *Cmap;
    char LB[] = {
	'N','E','T','S',
	'C','A','P','E',
	'2','.','0' };
    char LB2[] = { 1, 0x00, 0x00 };

    if (target_fps == 0) target_fps = 5;
    gif_frameskip = (vo_fps + 0.25) / target_fps;
    gif_framedelay = 100 / target_fps;
    
    if ((width != d_width) || (height != d_height)) {
	    image_width = (d_width + 7) & ~7;
	    image_height = d_height;
	    scale_srcW = width;
	    scale_srcH = height;
	    SwScale_Init();
    } else {
	    image_width = width;
	    image_height = height;
    }
    image_format = format;

    Cmap = MakeMapObject(256, NULL);

    switch(format) {
	case IMGFMT_BGR32:
	case IMGFMT_BGR24:
	     reverse_map = 1;
	break;     
	case IMGFMT_RGB32:
	case IMGFMT_RGB24:
	break;     
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	     reverse_map = 1;
	     yuv2rgb_init(24, MODE_RGB);
	     image_data = malloc(image_width*image_height*3);
	break;
	default:
	     return 1;     
    }

    if (vo_config_count > 0)
        return 0;
    
    EGifSetGifVersion("89a");
    newgif = EGifOpenFileName(filename, 0);
    if (newgif == NULL)
    {
	    fprintf(stderr, "error opening file for output.\n");
	    return(1);
    }
    EGifPutScreenDesc(newgif, image_width, image_height, 256, 0, Cmap);
    EGifPutExtensionFirst(newgif, 0xFF, 11, LB);
    EGifPutExtensionLast(newgif, 0, 3, LB2);
    
    return 0;
}

static const vo_info_t* get_info(void)
{
    return &vo_info;
}

static uint32_t draw_frame(uint8_t * src[])
{
  uint8_t *use_data;
  ColorMapObject *Cmap;
  uint8_t Colors[256 * 3];
  int z;
  char CB[] = { (char)(gif_framedelay >> 8), (char)(gif_framedelay & 0xff), 0, 0};

  if ((framenum++ % gif_frameskip)) return(0);
  
  if ((image_format == IMGFMT_BGR32) || (image_format == IMGFMT_RGB32))
  {
    rgb32to24(src[0], image_data, image_width * image_height * 4);
    src[0] = image_data;
  }

  Cmap = MakeMapObject(256, NULL);
  use_data = (uint8_t *)malloc(image_width * image_height);
  if (gif_reduce(image_width, image_height, src[0], use_data, Colors)) return(0);
  
  if (reverse_map)
  {
    for (z = 0; z < 256; z++) {
      Cmap->Colors[z].Blue = Colors[(z * 3) + 0];
      Cmap->Colors[z].Green = Colors[(z * 3) + 1];
      Cmap->Colors[z].Red = Colors[(z * 3) + 2];
    }
  }
  else
  {
    for (z = 0; z < 256; z++) {
      Cmap->Colors[z].Red = Colors[(z * 3) + 0];
      Cmap->Colors[z].Green = Colors[(z * 3) + 1];
      Cmap->Colors[z].Blue = Colors[(z * 3) + 2];
    }
  }
  
  EGifPutExtension(newgif, 0xF9, 0x04, CB);
  EGifPutImageDesc(newgif, 0, 0, image_width, image_height, 0, Cmap);
  EGifPutLine(newgif, use_data, image_width * image_height);
  FreeMapObject(Cmap);
  free(use_data);

  return (0);
}

static void draw_osd(void)
{
}

static void flip_page (void)
{
  uint8_t *use_data;
  ColorMapObject *Cmap;
  uint8_t Colors[256 * 3];
  int z;
  char CB[] = { (char)(gif_framedelay >> 8), (char)(gif_framedelay & 0xff), 0, 0};
  
  if ((image_format == IMGFMT_YV12) || (image_format == IMGFMT_IYUV) || (image_format == IMGFMT_I420)) {

    if ((framenum++ % gif_frameskip)) return;

    Cmap = MakeMapObject(256, NULL);
    use_data = (uint8_t *)malloc(image_width * image_height);
    if (gif_reduce(image_width, image_height, image_data, use_data, Colors)) return;

    if (reverse_map)
    {
      for (z = 0; z < 256; z++) {
        Cmap->Colors[z].Blue = Colors[(z * 3) + 0];
        Cmap->Colors[z].Green = Colors[(z * 3) + 1];
        Cmap->Colors[z].Red = Colors[(z * 3) + 2];
      }
    }
    else
    {
      for (z = 0; z < 256; z++) {
        Cmap->Colors[z].Red = Colors[(z * 3) + 0];
        Cmap->Colors[z].Green = Colors[(z * 3) + 1];
        Cmap->Colors[z].Blue = Colors[(z * 3) + 2];
      }
    }
  
    EGifPutExtension(newgif, 0xF9, 0x04, CB);
    EGifPutImageDesc(newgif, 0, 0, image_width, image_height, 0, Cmap);
    EGifPutLine(newgif, use_data, image_width * image_height);
    FreeMapObject(Cmap);
    free(use_data);
  }
}

static uint32_t draw_slice( uint8_t *src[],int stride[],int w,int h,int x,int y )
{
  /* hack: swap planes for I420 ;) -- alex */
  if ((image_format == IMGFMT_IYUV) || (image_format == IMGFMT_I420))
  {
    uint8_t *src_i420[3];
    
    src_i420[0] = src[0];
    src_i420[1] = src[2];
    src_i420[2] = src[1];
    src = src_i420;
  }

  if (scale_srcW) {
    uint8_t *dst[3] = {image_data, NULL, NULL};
    SwScale_YV12slice(src,stride,y,h,
		      dst, image_width*3, 24,
		      scale_srcW, scale_srcH, image_width, image_height);
  } else {
    uint8_t *dst = image_data + (image_width * y + x) * 3;
    yuv2rgb(dst,src[0],src[1],src[2],w,h,image_width*3,stride[0],stride[1]);
  }
  return 0;
}

static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_IYUV:
    case IMGFMT_I420:
    case IMGFMT_YV12:
    case IMGFMT_RGB|32:
    case IMGFMT_BGR|32:
    case IMGFMT_RGB|24:
    case IMGFMT_BGR|24:
        return 1 | VFCAP_SWSCALE | VFCAP_TIMER | VFCAP_ACCEPT_STRIDE;
    }
    return 0;
}

static void
uninit(void)
{
	char temp[256];
	
	if (image_data) { free(image_data); image_data=NULL; }

	if (vo_config_count > 0) {
		sprintf(temp, "gifwhore v%2.2f (c) %s\r\n",
			GIFWHORE_version, "joey@yunamusic.com");
		EGifPutComment(newgif, temp);
		EGifCloseFile(newgif);
	}
}


static void check_events(void)
{
}

int gif_reduce(int width, int height,
               unsigned char *source,
               unsigned char *destination,
               unsigned char *palette)
{
	GifColorType cmap[256];
	unsigned char Ra[width * height];
	unsigned char Ga[width * height];
	unsigned char Ba[width * height];
	unsigned char *R, *G, *B;
	int Size = 256;
	int i;

	R = Ra; G = Ga; B = Ba;
	for (i = 0; i < width * height; i++)
	{
		*R++ = *source++;
		*G++ = *source++;
		*B++ = *source++;
	}
	
	R = Ra; G = Ga; B = Ba;
	if (QuantizeBuffer(width, height, &Size,
			R, G, B,
			destination, cmap) == GIF_ERROR)
	{
		fprintf(stderr, "vo_gif89a: Quantize failed!\n");
		return(-1);
	}
	
	for (i = 0; i < Size; i++)
	{
		*palette++ = cmap[i].Red;
		*palette++ = cmap[i].Green;
		*palette++ = cmap[i].Blue;
	}
	
	return(0);
}

static uint32_t preinit(const char *arg)
{
  int i = 0;
  if (arg) i = atoi(arg);
  if (i > vo_fps) i = vo_fps;
  if (i < 1) i = 5;
  target_fps = i;
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

