
#ifndef __MY_BMP
#define __MY_BMP

#include "../bitmap.h"

/*
    0.1  : BMP type.
    2.5  : File size.
    6.7  : Res.
    8.9  : Res.
   10.13 : Offset of bitmap.
   14.17 : Header size.
   18.21 : X size.
   22.25 : Y size.
   26.27 : Number of planes.
   28.29 : Number of bits per pixel.
   30.33 : Compression flag.
   34.37 : Image data size in bytes.
   38.41 : Res
   42.45 : Res
   46.49 : Res
   50.53 : Res
*/

extern int bmpRead( unsigned char * fname,txSample * bF );

#endif