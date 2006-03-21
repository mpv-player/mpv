#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

//===========================================================================//

#define uint32 unsigned long
#define uint16 unsigned short
#define uint8 unsigned char

static uint32 colorMask = 0xF7DEF7DE;
static uint32 lowPixelMask = 0x08210821;
static uint32 qcolorMask = 0xE79CE79C;
static uint32 qlowpixelMask = 0x18631863;
static uint32 redblueMask = 0xF81F;
static uint32 greenMask = 0x7E0;
static int PixelsPerMask = 2;
static int xsai_depth = 0;

#define makecol(r,g,b) (r+(g<<8)+(b<<16))
#define makecol_depth(d,r,g,b) (r+(g<<8)+(b<<16))

int Init_2xSaI(int d)
{

	int minr = 0, ming = 0, minb = 0;
	int i;
	
//	if (d != 15 && d != 16 && d != 24 && d != 32)
//		return -1;

	/* Get lowest color bit */	
	for (i = 0; i < 255; i++) {
		if (!minr)
			minr = makecol(i, 0, 0);
		if (!ming)
			ming = makecol(0, i, 0);
		if (!minb)
			minb = makecol(0, 0, i);
	}

	colorMask = (makecol_depth(d, 255, 0, 0) - minr) | (makecol_depth(d, 0, 255, 0) - ming) | (makecol_depth(d, 0, 0, 255) - minb);
	lowPixelMask = minr | ming | minb;
	qcolorMask = (makecol_depth(d, 255, 0, 0) - 3 * minr) | (makecol_depth(d, 0, 255, 0) - 3 * ming) | (makecol_depth(d, 0, 0, 255) - 3 * minb);
	qlowpixelMask = (minr * 3) | (ming * 3) | (minb * 3);
	redblueMask = makecol_depth(d, 255, 0, 255);
	greenMask = makecol_depth(d, 0, 255, 0);

	PixelsPerMask = (d <= 16) ? 2 : 1;
	
	if (PixelsPerMask == 2) {
		colorMask |= (colorMask << 16);
		qcolorMask |= (qcolorMask << 16);
		lowPixelMask |= (lowPixelMask << 16);
		qlowpixelMask |= (qlowpixelMask << 16);
	}

//	TRACE("Color Mask:       0x%lX\n", colorMask);
//	TRACE("Low Pixel Mask:   0x%lX\n", lowPixelMask);
//	TRACE("QColor Mask:      0x%lX\n", qcolorMask);
//	TRACE("QLow Pixel Mask:  0x%lX\n", qlowpixelMask);
	
	xsai_depth = d;

	return 0;
}


static int GetResult1(uint32 A, uint32 B, uint32 C, uint32 D)
{
	int x = 0;
	int y = 0;
	int r = 0;
	if (A == C)
		x += 1;
	else if (B == C)
		y += 1;
	if (A == D)
		x += 1;
	else if (B == D)
		y += 1;
	if (x <= 1)
		r += 1;
	if (y <= 1)
		r -= 1;
	return r;
}

static int GetResult2(uint32 A, uint32 B, uint32 C, uint32 D, uint32 E)
{
	int x = 0;
	int y = 0;
	int r = 0;
	if (A == C)
		x += 1;
	else if (B == C)
		y += 1;
	if (A == D)
		x += 1;
	else if (B == D)
		y += 1;
	if (x <= 1)
		r -= 1;
	if (y <= 1)
		r += 1;
	return r;
}


#define GET_RESULT(A, B, C, D) ((A != C || A != D) - (B != C || B != D))

#define INTERPOLATE(A, B) (((A & colorMask) >> 1) + ((B & colorMask) >> 1) + (A & B & lowPixelMask))

#define Q_INTERPOLATE(A, B, C, D) ((A & qcolorMask) >> 2) + ((B & qcolorMask) >> 2) + ((C & qcolorMask) >> 2) + ((D & qcolorMask) >> 2) \
	+ ((((A & qlowpixelMask) + (B & qlowpixelMask) + (C & qlowpixelMask) + (D & qlowpixelMask)) >> 2) & qlowpixelMask)


static unsigned char *src_line[4];
static unsigned char *dst_line[2];

void Super2xSaI_ex(uint8 *src, uint32 src_pitch, 
		   uint8 *dst, uint32 dst_pitch,
		   uint32 width, uint32 height, int sbpp) {

	unsigned int x, y;
	unsigned long color[16];

	/* Point to the first 3 lines. */
	src_line[0] = src;
	src_line[1] = src;
	src_line[2] = src + src_pitch;
	src_line[3] = src + src_pitch * 2;
	
	x = 0, y = 0;
	
	if (PixelsPerMask == 2) {
		unsigned short *sbp;
		sbp = (unsigned short*)src_line[0];
		color[0] = *sbp;       color[1] = color[0];   color[2] = color[0];    color[3] = color[0];
		color[4] = color[0];   color[5] = color[0];   color[6] = *(sbp + 1);  color[7] = *(sbp + 2);
		sbp = (unsigned short*)src_line[2];
		color[8] = *sbp;     color[9] = color[8];     color[10] = *(sbp + 1); color[11] = *(sbp + 2);
		sbp = (unsigned short*)src_line[3];
		color[12] = *sbp;    color[13] = color[12];   color[14] = *(sbp + 1); color[15] = *(sbp + 2);
	}
	else {
		unsigned long *lbp;
		lbp = (unsigned long*)src_line[0];
		color[0] = *lbp;       color[1] = color[0];   color[2] = color[0];    color[3] = color[0];
		color[4] = color[0];   color[5] = color[0];   color[6] = *(lbp + 1);  color[7] = *(lbp + 2);
		lbp = (unsigned long*)src_line[2];
		color[8] = *lbp;     color[9] = color[8];     color[10] = *(lbp + 1); color[11] = *(lbp + 2);
		lbp = (unsigned long*)src_line[3];
		color[12] = *lbp;    color[13] = color[12];   color[14] = *(lbp + 1); color[15] = *(lbp + 2);
	}

	for (y = 0; y < height; y++) {

		dst_line[0] = dst + dst_pitch*2*y;
		dst_line[1] = dst + dst_pitch*(2*y+1);
	
		/* Todo: x = width - 2, x = width - 1 */
		
		for (x = 0; x < width; x++) {
			unsigned long product1a, product1b, product2a, product2b;

//---------------------------------------  B0 B1 B2 B3    0  1  2  3
//                                         4  5* 6  S2 -> 4  5* 6  7
//                                         1  2  3  S1    8  9 10 11
//                                         A0 A1 A2 A3   12 13 14 15
//--------------------------------------
			if (color[9] == color[6] && color[5] != color[10]) {
				product2b = color[9];
				product1b = product2b;
			}
			else if (color[5] == color[10] && color[9] != color[6]) {
				product2b = color[5];
				product1b = product2b;
			}
			else if (color[5] == color[10] && color[9] == color[6]) {
				int r = 0;

				r += GET_RESULT(color[6], color[5], color[8], color[13]);
				r += GET_RESULT(color[6], color[5], color[4], color[1]);
				r += GET_RESULT(color[6], color[5], color[14], color[11]);
				r += GET_RESULT(color[6], color[5], color[2], color[7]);

				if (r > 0)
					product1b = color[6];
				else if (r < 0)
					product1b = color[5];
				else
					product1b = INTERPOLATE(color[5], color[6]);
					
				product2b = product1b;

			}
			else {
				if (color[6] == color[10] && color[10] == color[13] && color[9] != color[14] && color[10] != color[12])
					product2b = Q_INTERPOLATE(color[10], color[10], color[10], color[9]);
				else if (color[5] == color[9] && color[9] == color[14] && color[13] != color[10] && color[9] != color[15])
					product2b = Q_INTERPOLATE(color[9], color[9], color[9], color[10]);
				else
					product2b = INTERPOLATE(color[9], color[10]);

				if (color[6] == color[10] && color[6] == color[1] && color[5] != color[2] && color[6] != color[0])
					product1b = Q_INTERPOLATE(color[6], color[6], color[6], color[5]);
				else if (color[5] == color[9] && color[5] == color[2] && color[1] != color[6] && color[5] != color[3])
					product1b = Q_INTERPOLATE(color[6], color[5], color[5], color[5]);
				else
					product1b = INTERPOLATE(color[5], color[6]);
			}

			if (color[5] == color[10] && color[9] != color[6] && color[4] == color[5] && color[5] != color[14])
				product2a = INTERPOLATE(color[9], color[5]);
			else if (color[5] == color[8] && color[6] == color[5] && color[4] != color[9] && color[5] != color[12])
				product2a = INTERPOLATE(color[9], color[5]);
			else
				product2a = color[9];

			if (color[9] == color[6] && color[5] != color[10] && color[8] == color[9] && color[9] != color[2])
				product1a = INTERPOLATE(color[9], color[5]);
			else if (color[4] == color[9] && color[10] == color[9] && color[8] != color[5] && color[9] != color[0])
				product1a = INTERPOLATE(color[9], color[5]);
			else
				product1a = color[5];
	
			if (PixelsPerMask == 2) {
				*((unsigned long *) (&dst_line[0][x * 4])) = product1a | (product1b << 16);
				*((unsigned long *) (&dst_line[1][x * 4])) = product2a | (product2b << 16);
			}
			else {
				*((unsigned long *) (&dst_line[0][x * 8])) = product1a;
				*((unsigned long *) (&dst_line[0][x * 8 + 4])) = product1b;
				*((unsigned long *) (&dst_line[1][x * 8])) = product2a;
				*((unsigned long *) (&dst_line[1][x * 8 + 4])) = product2b;
			}
			
			/* Move color matrix forward */
			color[0] = color[1]; color[4] = color[5]; color[8] = color[9];   color[12] = color[13];
			color[1] = color[2]; color[5] = color[6]; color[9] = color[10];  color[13] = color[14];
			color[2] = color[3]; color[6] = color[7]; color[10] = color[11]; color[14] = color[15];
			
			if (x < width - 3) {
				x += 3;
				if (PixelsPerMask == 2) {
					color[3] = *(((unsigned short*)src_line[0]) + x);					
					color[7] = *(((unsigned short*)src_line[1]) + x);
					color[11] = *(((unsigned short*)src_line[2]) + x);
					color[15] = *(((unsigned short*)src_line[3]) + x);
				}
				else {
					color[3] = *(((unsigned long*)src_line[0]) + x);
					color[7] = *(((unsigned long*)src_line[1]) + x);
					color[11] = *(((unsigned long*)src_line[2]) + x);
					color[15] = *(((unsigned long*)src_line[3]) + x);
				}
				x -= 3;
			}
		}

		/* We're done with one line, so we shift the source lines up */
		src_line[0] = src_line[1];
		src_line[1] = src_line[2];
		src_line[2] = src_line[3];		

		/* Read next line */
		if (y + 3 >= height)
			src_line[3] = src_line[2];
		else
			src_line[3] = src_line[2] + src_pitch;
			
		/* Then shift the color matrix up */
		if (PixelsPerMask == 2) {
			unsigned short *sbp;
			sbp = (unsigned short*)src_line[0];
			color[0] = *sbp;     color[1] = color[0];    color[2] = *(sbp + 1);  color[3] = *(sbp + 2);
			sbp = (unsigned short*)src_line[1];
			color[4] = *sbp;     color[5] = color[4];    color[6] = *(sbp + 1);  color[7] = *(sbp + 2);
			sbp = (unsigned short*)src_line[2];
			color[8] = *sbp;     color[9] = color[9];    color[10] = *(sbp + 1); color[11] = *(sbp + 2);
			sbp = (unsigned short*)src_line[3];
			color[12] = *sbp;    color[13] = color[12];  color[14] = *(sbp + 1); color[15] = *(sbp + 2);
		}
		else {
			unsigned long *lbp;
			lbp = (unsigned long*)src_line[0];
			color[0] = *lbp;     color[1] = color[0];    color[2] = *(lbp + 1);  color[3] = *(lbp + 2);
			lbp = (unsigned long*)src_line[1];
			color[4] = *lbp;     color[5] = color[4];    color[6] = *(lbp + 1);  color[7] = *(lbp + 2);
			lbp = (unsigned long*)src_line[2];
			color[8] = *lbp;     color[9] = color[9];    color[10] = *(lbp + 1); color[11] = *(lbp + 2);
			lbp = (unsigned long*)src_line[3];
			color[12] = *lbp;    color[13] = color[12];  color[14] = *(lbp + 1); color[15] = *(lbp + 2);
		}
		
	} // y loop
	
}


//===========================================================================//

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    Init_2xSaI(outfmt&255);

    return vf_next_config(vf,2*width,2*height,2*d_width,2*d_height,flags,outfmt);
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
    mp_image_t *dmpi;

    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	2*mpi->w, 2*mpi->h);

    Super2xSaI_ex(mpi->planes[0], mpi->stride[0],
		  dmpi->planes[0], dmpi->stride[0],
		  mpi->w, mpi->h, mpi->bpp/8);
    
    return vf_next_put_image(vf,dmpi, pts);
}

//===========================================================================//

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    switch(fmt){
//    case IMGFMT_BGR15:
//    case IMGFMT_BGR16:
    case IMGFMT_BGR32:
	return vf_next_query_format(vf,fmt);
    }
    return 0;
}

static int open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->put_image=put_image;
    vf->query_format=query_format;
    return 1;
}

vf_info_t vf_info_2xsai = {
    "2xSai BGR bitmap 2x scaler",
    "2xsai",
    "A'rpi",
    "http://elektron.its.tudelft.nl/~dalikifa/",
    open,
    NULL
};

//===========================================================================//
