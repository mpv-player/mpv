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
#include "sub.h"

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

static int reverse_map = 0;
static unsigned char framenum = 0;
static int gif_frameskip;
static int gif_framedelay;
static int target_fps = 0;

GifFileType *newgif=NULL;

static uint32_t config
	(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, 
		uint32_t fullscreen, char *title, uint32_t format) {
    char filename[] = "out.gif";
    ColorMapObject *Cmap;
#ifdef HAVE_GIF_4
    char LB[] = {
	'N','E','T','S',
	'C','A','P','E',
	'2','.','0' };
    char LB2[] = { 1, 0x00, 0x00 };
#endif

    if (target_fps == 0) target_fps = 5;
    gif_frameskip = (vo_fps + 0.25) / target_fps;
    gif_framedelay = 100 / target_fps;
    
    image_width = width;
    image_height = height;
    image_format = format;

    Cmap = MakeMapObject(256, NULL);

    switch(format) {
	case IMGFMT_BGR24:
	     reverse_map = 1;
	break;     
	case IMGFMT_RGB24:
	break;     
	case IMGFMT_YV12:
	     yuv2rgb_init(24, MODE_RGB);
	     image_data = malloc(image_width*image_height*3);
	break;
	default:
	     return 1;     
    }

    if (vo_config_count > 0)
        return 0;
    
    // this line causes crashes in certain earlier versions of libungif.
    // i don't know exactly which, but certainly all those before v4.
    // if you have problems, you need to upgrade your gif library.
#ifdef HAVE_GIF_4
    EGifSetGifVersion("89a");
#else
    fprintf(stderr, "vo_gif89a: Your version of libgif/libungif needs to be upgraded.\n");
    fprintf(stderr, "vo_gif89a: Some functionality has been disabled.\n");
#endif
    newgif = EGifOpenFileName(filename, 0);
    if (newgif == NULL)
    {
	    fprintf(stderr, "error opening file for output.\n");
	    return(1);
    }
    EGifPutScreenDesc(newgif, image_width, image_height, 256, 0, Cmap);
#ifdef HAVE_GIF_4
    // version 3 of libgif/libungif does not support multiple control blocks.
    // for this version, looping will be disabled.
    EGifPutExtensionFirst(newgif, 0xFF, 11, LB);
    EGifPutExtensionLast(newgif, 0, 3, LB2);
#endif
    
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

#ifdef USE_OSD
static void draw_alpha(int x0, int y0, int w, int h, unsigned char *src, unsigned char *srca, int stride)
{
  vo_draw_alpha_rgb24(w, h, src, srca, stride, image_data + 3 * (y0 * image_width + x0), 3 * image_width);
}
#endif

static void draw_osd(void)
{
#ifdef USE_OSD
  vo_draw_text(image_width, image_height, draw_alpha);
#endif
}

static void flip_page (void)
{
  uint8_t *use_data;
  ColorMapObject *Cmap;
  uint8_t Colors[256 * 3];
  int z;
  char CB[] = { (char)(gif_framedelay >> 8), (char)(gif_framedelay & 0xff), 0, 0};
  
  if (image_format == IMGFMT_YV12) {

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
  uint8_t *dst = image_data + (image_width * y + x) * 3;
  yuv2rgb(dst,src[0],src[1],src[2],w,h,image_width*3,stride[0],stride[1]);
  return 0;
}

static uint32_t
query_format(uint32_t format)
{
    switch(format){
    case IMGFMT_YV12:
	return VFCAP_CSP_SUPPORTED | VFCAP_TIMER | VFCAP_ACCEPT_STRIDE;
    case IMGFMT_RGB|24:
    case IMGFMT_BGR|24:
        return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD | VFCAP_TIMER;
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

