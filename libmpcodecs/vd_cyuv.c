/* ------------------------------------------------------------------------
 * Creative YUV Video Decoder
 *
 * Dr. Tim Ferguson, 2001.
 * For more details on the algorithm:
 *         http://www.csse.monash.edu.au/~timf/videocodec.html
 *
 * Code restructured, and adopted to MPlayer's mpi by A'rpi, 2002.
 *
 * This is a very simple predictive coder.  A video frame is coded in YUV411
 * format.  The first pixel of each scanline is coded using the upper four
 * bits of its absolute value.  Subsequent pixels for the scanline are coded
 * using the difference between the last pixel and the current pixel (DPCM).
 * The DPCM values are coded using a 16 entry table found at the start of the
 * frame.  Thus four bits per component are used and are as follows:
 *     UY VY YY UY VY YY UY VY...
 * ------------------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"

static vd_info_t info = {
	"Creative YUV decoder",
	"cyuv",
	"A'rpi",
	"Dr. Tim Ferguson",
	"native codec"
};

LIBVD_EXTERN(cyuv)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_411P);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    unsigned int xpos, ypos;
    unsigned char *delta_y_tbl = ((unsigned char*)data)+16;
    unsigned char *delta_c_tbl = ((unsigned char*)data)+32;
    unsigned char *ptr = ((unsigned char*)data)+48;

    if(len<=48) return NULL; // skipped/broken frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	sh->disp_w, sh->disp_h);
    if(!mpi) return NULL;

    for(ypos = 0; ypos < mpi->h; ypos++){
	    unsigned int i;
	    unsigned char cur_Y1,cur_Y2,cur_U,cur_V;
	    unsigned char *frame=mpi->planes[0]+mpi->stride[0]*ypos;
	    unsigned char *uframe=mpi->planes[1]+mpi->stride[1]*ypos;
	    unsigned char *vframe=mpi->planes[2]+mpi->stride[2]*ypos;

	    for(xpos = 0; xpos < mpi->w; xpos += 2){

			if(xpos&2){
			    i = *(ptr++);
			    cur_Y1 = (cur_Y2 + delta_y_tbl[i & 0x0f])/* & 0xff*/;
			    cur_Y2 = (cur_Y1 + delta_y_tbl[i >> 4])/* & 0xff*/;
			} else {
			    if(xpos == 0) {	/* first pixels in scanline */
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
			
			// ok, store the pixels:
			switch(mpi->imgfmt){
			case IMGFMT_YUY2:
				*frame++ = cur_Y1;
				*frame++ = cur_U;
				*frame++ = cur_Y2;
				*frame++ = cur_V;
				break;
			case IMGFMT_UYVY:
				*frame++ = cur_U;
				*frame++ = cur_Y1;
				*frame++ = cur_V;
				*frame++ = cur_Y2;
				break;
			case IMGFMT_422P:
				*uframe++ = cur_U;
				*vframe++ = cur_V;
				*frame++ = cur_Y1;
				*frame++ = cur_Y2;
				break;
			case IMGFMT_411P:
				if(!(xpos&2)){
				    *uframe++ = cur_U;
				    *vframe++ = cur_V;
				}
				*frame++ = cur_Y1;
				*frame++ = cur_Y2;
				break;
			}
	    } // xpos
    } // ypos

    return mpi;
}
