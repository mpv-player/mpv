
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../config.h"
#ifdef xHAVE_MMX
 #include "../../main/libvo/mmx.h"
 #include "../../main/libvo/fastmemcpy.h"
#endif
#include "wsconv.h"

wsTConvFunc wsConvFunc = NULL;

// ---

#define COPY_RGB_24(src,dst) dst[0]=src[0];dst[1]=src[1];dst[2]=src[2]

#define SWAP_RGB_24(src,dst) dst[1]=src[0];dst[1]=src[1];dst[2]=src[0]

void BGR8880_to_RGB555_c( const unsigned char * in_pixels, unsigned char * out_pixels, unsigned num_pixels)
{
 unsigned short pixel;
 int i;
 for(i = 0; i < num_pixels / 4; i++)
  {
   PACK_RGB15(in_pixels[0],in_pixels[1],in_pixels[2],pixel);
   *(unsigned short*)out_pixels = pixel;
   in_pixels += 4;
   out_pixels += 2;
  }
}

void BGR8880_to_BGR555_c( const unsigned char * in_pixels, unsigned char * out_pixels, unsigned num_pixels)
{
 unsigned short pixel;
 int i;
 for(i = 0; i < num_pixels / 4; i++)
  {
   PACK_RGB15(in_pixels[2],in_pixels[1],in_pixels[0],pixel);
   *(unsigned short*)out_pixels = pixel;
   in_pixels += 4;
   out_pixels += 2;
  }
}

void BGR8880_to_RGB565_c( const unsigned char * in_pixels, unsigned char * out_pixels, unsigned num_pixels)
{
 unsigned short pixel;
 int i;
 for(i = 0; i < num_pixels / 4; i++)
  {
   PACK_RGB16(in_pixels[0],in_pixels[1],in_pixels[2],pixel);
   *(unsigned short*)out_pixels = pixel;
   in_pixels += 4;
   out_pixels += 2;
  }
}

void BGR8880_to_BGR565_c( const unsigned char * in_pixels, unsigned char * out_pixels, unsigned num_pixels)
{
 unsigned short pixel;
 int i;
 for(i = 0; i < num_pixels / 4; i++)
  {
   PACK_RGB16(in_pixels[2],in_pixels[1],in_pixels[0],pixel);
   *(unsigned short*)out_pixels = pixel;
   in_pixels += 4;
   out_pixels += 2;
  }
}

void BGR8880_to_RGB888_c( const unsigned char * in_pixels, unsigned char * out_pixels,unsigned num_pixels )
{
 int i;
 for(i = 0; i < num_pixels / 4; i++)
  {
   COPY_RGB_24(in_pixels,out_pixels);
   in_pixels += 4;
   out_pixels += 3;
  }
}

void BGR8880_to_BGR888_c( const unsigned char * in_pixels, unsigned char * out_pixels,unsigned num_pixels )
{
 int i;
 for(i = 0; i < num_pixels / 4; i++)
  {
   SWAP_RGB_24(in_pixels,out_pixels);
   in_pixels += 4;
   out_pixels += 3;
  }
}

void BGR8880_to_BGR8880_c( const unsigned char * in_pixels, unsigned char * out_pixels,unsigned num_pixels )
{
 int i;
 for(i = 0; i < num_pixels / 4; i++)
  {
   SWAP_RGB_24(in_pixels,out_pixels);
   in_pixels += 4;
   out_pixels += 4;
  }
}

void BGR8880_to_RGB8880_c( const unsigned char * in_pixels, unsigned char * out_pixels,unsigned num_pixels )
{ memcpy( out_pixels,in_pixels,num_pixels ); }

/*

unsigned char * map_5_to_8[32];
unsigned char * map_6_to_8[64];

#define POINTER_TO_GUINT16(a) *((unsigned short*)a)
#define RGB16_TO_R(pixel) map_5_to_8[pixel & RGB16_LOWER_MASK]
#define RGB16_TO_G(pixel) map_6_to_8[(pixel & RGB16_MIDDLE_MASK)>>5]
#define RGB16_TO_B(pixel) map_5_to_8[(pixel & RGB16_UPPER_MASK)>>11]
#define RGB16_LOWER_MASK  0x001f
#define RGB16_MIDDLE_MASK 0x07e0
#define RGB16_UPPER_MASK  0xf800

void RGB565_to_RGB888_c( const unsigned char * in_pixels, unsigned char * out_pixels,unsigned num_pixels)
{
 unsigned short in_pixel;
 int i;
 for(i = 0; i < num_pixels; i++)
  {
   in_pixel = POINTER_TO_GUINT16(in_pixels);
   out_pixels[0] = RGB16_TO_R(in_pixel);
   out_pixels[1] = RGB16_TO_G(in_pixel);
   out_pixels[2] = RGB16_TO_B(in_pixel);
   in_pixels += 2;
   out_pixels += 3;
  }
}

*/

// ---

#ifdef xHAVE_MMX

#define LOAD_32(in) movq_m2r(*in, mm0); in += 8;\
                    movq_m2r(*in, mm1); in += 8

#define PACK_32_TO_24 movq_r2r(mm0, mm2);\
                      pand_m2r(rgb32_l_mask,mm0);\
                      pand_m2r(rgb32_u_mask,mm2);\
                      psrlq_i2r(8, mm2);\
                      por_r2r(mm2,mm0);\
                      movq_r2r(mm1, mm2);\
                      pand_m2r(rgb32_l_mask,mm1);\
                      pand_m2r(rgb32_u_mask,mm2);\
                      psrlq_i2r(8, mm2);\
                      por_r2r(mm2,mm1);

#define WRITE_24(out) movq_r2m(mm0, *out); out+=6;\
                      movq_r2m(mm1, *out); out+=6;

#define WRITE_16(out) movq_r2m(mm0, *out); out+=8;

static mmx_t rgb32_l_mask; // Mask for the lower of  2 RGB24 pixels
static mmx_t rgb32_u_mask; // Mask for the upper of  2 RGB24 pixels

static mmx_t rgb32_r_mask; // Mask for the reds of   2 RGB32 pixels
static mmx_t rgb32_g_mask; // Mask for the greens of 2 RGB32 pixels
static mmx_t rgb32_b_mask; // Mask for the blues  of 2 RGB32 pixels

static mmx_t lower_dword_mask; // Mask for the lower doublewords
static mmx_t upper_dword_mask; // Mask for the upper doublewords

void BGR8880_to_RGB888_mmx(unsigned char * in_pixels,unsigned char * out_pixels,unsigned num_pixels)
{
 int imax = num_pixels/4;
 int i;

 for(i = 0; i < imax; i++)
  {
   LOAD_32(in_pixels);
   PACK_32_TO_24;
   WRITE_24(out_pixels);
  }
 emms();
}

#endif

// ---

void initConverter( void )
{
#ifdef xHAVE_MMX
// int i;

// for(i = 0; i < 64; i++) map_6_to_8[i] = (unsigned char)((float)i/63.0*255.0+0.5);
// for(i = 0; i < 32; i++) map_5_to_8[i] = (unsigned char)((float)i/31.0*255.0+0.5);

 rgb32_l_mask.q = 0x0000000000FFFFFFLL; // Mask for the lower of 2 RGB32 pixels
 rgb32_u_mask.q = 0x00FFFFFF00000000LL; // Mask for the upper of 2 RGB32 pixels

 rgb32_r_mask.q = 0x000000FF000000FFLL; // Mask for the reds of   2 RGB32 pixels
 rgb32_g_mask.q = 0x0000FF000000FF00LL; // Mask for the greens of 2 RGB32 pixels
 rgb32_b_mask.q = 0x00FF000000FF0000LL; // Mask for the blues  of 2 RGB32 pixels
#endif
}

