/*
 * NuppelVideo 0.05 file parser
 * for MPlayer
 * by Panagiotis Issaris <takis@lumumba.luc.ac.be>
 *
 * Reworked by alex
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "mpbswap.h"

#include "libvo/fastmemcpy.h"

#include "libmpdemux/nuppelvideo.h" 
#include "rtjpegn.h"
#include "libavutil/lzo.h"

#define KEEP_BUFFER

void decode_nuv( unsigned char *encoded, int encoded_size,
		unsigned char *decoded, int width, int height)
{
	int r;
	unsigned int out_len = width * height + ( width * height ) / 2;
	struct rtframeheader *encodedh = ( struct rtframeheader* ) encoded;
	static unsigned char *buffer = 0; /* for RTJpeg with LZO decompress */
#ifdef KEEP_BUFFER
	static unsigned char *previous_buffer = 0; /* to support Last-frame-copy */
#endif

//	printf("frametype: %c, comtype: %c, encoded_size: %d, width: %d, height: %d\n",
//	    encodedh->frametype, encodedh->comptype, encoded_size, width, height);

	le2me_rtframeheader(encodedh);
	switch(encodedh->frametype)
	{
	    case 'D':	/* additional data for compressors */
	    {
		/* tables are in encoded */
		if (encodedh->comptype == 'R')
		{
		    RTjpeg_init_decompress ( (unsigned long *)(encoded+12), width, height );
		    mp_msg(MSGT_DECVIDEO, MSGL_V, "Found RTjpeg tables (size: %d, width: %d, height: %d)\n",
			encoded_size-12, width, height);
		}
		break;
	    }
	    case 'V':
	    {
		int in_len = encodedh->packetlength;
#ifdef KEEP_BUFFER		
		if (!previous_buffer) 
			previous_buffer = ( unsigned char * ) malloc ( out_len + AV_LZO_OUTPUT_PADDING );
#endif

		switch(encodedh->comptype)
		{
		    case '0': /* raw YUV420 */
			fast_memcpy(decoded, encoded + 12, out_len);
			break;
		    case '1': /* RTJpeg */
			RTjpeg_decompressYUV420 ( ( __s8 * ) encoded + 12, decoded );
			break;
		    case '2': /* RTJpeg with LZO */
			if (!buffer) 
			    buffer = ( unsigned char * ) malloc ( out_len + AV_LZO_OUTPUT_PADDING );
			if (!buffer)
			{
			    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Nuppelvideo: error decompressing\n");
			    break;
			}
			r = av_lzo1x_decode ( buffer, &out_len, encoded + 12, &in_len );
			if ( r ) 
			{
			    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Nuppelvideo: error decompressing\n");
			    break;
			}
			RTjpeg_decompressYUV420 ( ( __s8 * ) buffer, decoded );
			break;
		    case '3': /* raw YUV420 with LZO */
			r = av_lzo1x_decode ( decoded, &out_len, encoded + 12, &in_len );
			if ( r ) 
			{
			    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Nuppelvideo: error decompressing\n");
			    break;
			}
			break;
		    case 'N': /* black frame */
			memset ( decoded, 0,  width * height );
			memset ( decoded + width * height, 127, width * height / 2);
			break;
		    case 'L': /* copy last frame */
#ifdef KEEP_BUFFER
			fast_memcpy ( decoded, previous_buffer, width*height*3/2);
#endif
			break;
		}

#ifdef KEEP_BUFFER
		fast_memcpy(previous_buffer, decoded, width*height*3/2);
#endif
		break;
	    }
	    default:
		mp_msg(MSGT_DECVIDEO, MSGL_V, "Nuppelvideo: unknwon frametype: %c\n",
		    encodedh->frametype);
	}
}
