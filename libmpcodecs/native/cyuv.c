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
int i, xpos, ypos, cur_Y = 0, cur_U = 0, cur_V = 0;
char *delta_y_tbl, *delta_c_tbl, *ptr;

	delta_y_tbl = buf + 16;
	delta_c_tbl = buf + 32;
	ptr = buf + (16 * 3);

	for(ypos = 0; ypos < height; ypos++)
		for(xpos = 0; xpos < width; xpos += 4)
			{
			if(xpos == 0)		/* first pixels in scanline */
				{
				cur_U = *(ptr++);
				cur_Y = (cur_U & 0x0f) << 4;
				cur_U = cur_U & 0xf0;
				if (format == IMGFMT_YUY2)
					{
					*frame++ = cur_Y;
					*frame++ = cur_U;
					}
				else
					{
					*frame++ = cur_U;
					*frame++ = cur_Y;
					}

				cur_V = *(ptr++);
				cur_Y = (cur_Y + delta_y_tbl[cur_V & 0x0f]) & 0xff;
				cur_V = cur_V & 0xf0;
				if (format == IMGFMT_YUY2)
					{
					*frame++ = cur_Y;
					*frame++ = cur_V;
					}
				else
					{
					*frame++ = cur_V;
					*frame++ = cur_Y;
					}
				}
			else			/* subsequent pixels in scanline */
				{
				i = *(ptr++);
				cur_U = (cur_U + delta_c_tbl[i >> 4]) & 0xff;
				cur_Y = (cur_Y + delta_y_tbl[i & 0x0f]) & 0xff;
				if (format == IMGFMT_YUY2)
					{
					*frame++ = cur_Y;
					*frame++ = cur_U;
					}
				else
					{
					*frame++ = cur_U;
					*frame++ = cur_Y;
					}

				i = *(ptr++);
				cur_V = (cur_V + delta_c_tbl[i >> 4]) & 0xff;
				cur_Y = (cur_Y + delta_y_tbl[i & 0x0f]) & 0xff;
				if (format == IMGFMT_YUY2)
					{
					*frame++ = cur_Y;
					*frame++ = cur_V;
					}
				else
					{
					*frame++ = cur_V;
					*frame++ = cur_Y;
					}
				}

			i = *(ptr++);
			cur_Y = (cur_Y + delta_y_tbl[i & 0x0f]) & 0xff;
			if (format == IMGFMT_YUY2)
				{
				*frame++ = cur_Y;
				*frame++ = cur_U;
				}
			else
				{
				*frame++ = cur_U;
				*frame++ = cur_Y;
				}

			cur_Y = (cur_Y + delta_y_tbl[i >> 4]) & 0xff;
			if (format == IMGFMT_YUY2)
				{
				*frame++ = cur_Y;
				*frame++ = cur_V;
				}
			else
				{
				*frame++ = cur_V;
				*frame++ = cur_Y;
				}
			}
}

