
#ifndef __WSCONV_H
#define __WSCONV_H

#include "../../config.h"

#define PACK_RGB16(r,g,b,pixel) pixel=(b>>3);\
                                pixel<<=6;\
                                pixel|=(g>>2);\
                                pixel<<=5;\
                                pixel|=(r>>3)

#define PACK_RGB15(r,g,b,pixel) pixel=(b>>3);\
                                pixel<<=5;\
                                pixel|=(g>>3);\
                                pixel<<=5;\
                                pixel|=(r>>3)

typedef void(*wsTConvFunc)( const unsigned char * in_pixels, unsigned char * out_pixels, unsigned num_pixels );
extern wsTConvFunc wsConvFunc;

extern void BGR8880_to_RGB555_c( const unsigned char * in_pixels, unsigned char * out_pixels, int num_pixels );
extern void BGR8880_to_BGR555_c( const unsigned char * in_pixels, unsigned char * out_pixels, int num_pixels );
extern void BGR8880_to_RGB565_c( const unsigned char * in_pixels, unsigned char * out_pixels, int num_pixels );
extern void BGR8880_to_BGR565_c( const unsigned char * in_pixels, unsigned char * out_pixels, int num_pixels );
extern void BGR8880_to_RGB888_c( const unsigned char * in_pixels, unsigned char * out_pixels, int num_pixels );
extern void BGR8880_to_BGR888_c( const unsigned char * in_pixels, unsigned char * out_pixels, int num_pixels );
extern void BGR8880_to_BGR8880_c( const unsigned char * in_pixels, unsigned char * out_pixels,int num_pixels );
extern void BGR8880_to_RGB8880_c( const unsigned char * in_pixels, unsigned char * out_pixels,int num_pixels );

#ifdef xHAVE_MMX
 extern void BGR8880_to_RGB888_mmx( const unsigned char * in_pixels,unsigned char * out_pixels,unsigned num_pixels);
#endif

extern void RGB565_to_RGB888_c( const unsigned char * in_pixels, unsigned char * out_pixels,unsigned num_pixels);

extern void initConverter( void );

#endif

