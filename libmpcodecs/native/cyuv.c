/* ------------------------------------------------------------------------
 * Creative YUV Video Decoder
 *
 * Dr. Tim Ferguson, 2001.
 * For more details on the algorithm:
 *         http://www.csse.monash.edu.au/~timf/videocodec.html
 *
 * This is a very simple predictive coder.  A video frame is coded in YUV411
 * format.  The first pixel of each scanline is coded using the upper four
 * bits of its absolute value.  Subsequent pixels for the scanline are coded
 * using the difference between the last pixel and the current pixel (DPCM).
 * The DPCM values are coded using a 16 entry table found at the start of the
 * frame.  Thus four bits per component are used and are as follows:
 *     UY VY YY UY VY YY UY VY...
 * This code assumes the frame width will be a multiple of four pixels.  This
 * should probably be fixed.
 * ------------------------------------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "img_format.h"

/* ------------------------------------------------------------------------
 * This function decodes a buffer containing a CYUV encoded frame.
 *
 * buf - the input buffer to be decoded
 * size - the size of the input buffer
 * frame - the output frame buffer (UYVY format)
 * width - the width of the output frame
 * height - the height of the output frame
 * format - the requested output format
 */
void decode_cyuv(unsigned char *buf, int size, unsigned char *frame, int width, int height, int format)
{
unsigned int i, xpos, ypos;
unsigned char *delta_y_tbl, *delta_c_tbl, *ptr;

	delta_y_tbl = buf + 16;
	delta_c_tbl = buf + 32;
	ptr = buf + (16 * 3);

	for(ypos = 0; ypos < height; ypos++)
		for(xpos = 0; xpos < width; xpos += 2){
			unsigned char cur_Y1,cur_Y2,cur_U,cur_V;
			if(xpos&2){
			    i = *(ptr++);
			    cur_Y1 = (cur_Y2 + delta_y_tbl[i & 0x0f])/* & 0xff*/;
			    cur_Y2 = (cur_Y1 + delta_y_tbl[i >> 4])/* & 0xff*/;
			} else {
			    if(xpos == 0) {		/* first pixels in scanline */
				cur_U = *(ptr++);
				cur_Y1= (cur_U & 0x0f) << 4;
				cur_U = cur_U & 0xf0;
				cur_V = *(ptr++);
				cur_Y2= (cur_Y1 + delta_y_tbl[cur_V & 0x0f])/* & 0xff*/;
				cur_V = cur_V & 0xf0;
			    } else {	/* subsequent pixels in scanline */
				i = *(ptr++);
				cur_U = (cur_U + delta_c_tbl[i >> 4])/* & 0xff*/;
				cur_Y1= (cur_Y2 + delta_y_tbl[i & 0x0f])/* & 0xff*/;
				i = *(ptr++);
				cur_V = (cur_V + delta_c_tbl[i >> 4])/* & 0xff*/;
				cur_Y2= (cur_Y1 + delta_y_tbl[i & 0x0f])/* & 0xff*/;
			    }
			}

			if (format == IMGFMT_YUY2) {
				*frame++ = cur_Y1;
				*frame++ = cur_U;
				*frame++ = cur_Y2;
				*frame++ = cur_V;
			} else {
				*frame++ = cur_U;
				*frame++ = cur_Y1;
				*frame++ = cur_V;
				*frame++ = cur_Y2;
			}
		}

}

