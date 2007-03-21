/* vf_bmovl.c v0.9.1 - BitMap OVerLay videofilter for MPlayer
 *
 * (C) 2002 Per Wigren <wigren@home.se>
 * Licenced under the GNU General Public License
 *
 * Use MPlayer as a framebuffer to read bitmaps and commands from a FIFO
 * and display them in the window.
 *
 * Commands are:
 *
 * RGBA32 width height xpos ypos alpha clear
 *   * Followed by width*height*4 bytes of raw RGBA32 data.
 * ABGR32 width height xpos ypos alpha clear
 *   * Followed by width*height*4 bytes of raw ABGR32 data.
 * RGB24 width height xpos ypos alpha clear
 *   * Followed by width*height*3 bytes of raw RGB32 data.
 * BGR24 width height xpos ypos alpha clear
 *   * Followed by width*height*3 bytes of raw BGR32 data.
 *
 * ALPHA width height xpos ypos alpha
 *   * Change alpha for area
 * CLEAR width height xpos ypos
 *   * Clear area
 * OPAQUE
 *   * Disable all alpha transparency!
 *      Send "ALPHA 0 0 0 0 0" to enable again!
 * HIDE
 *   * Hide bitmap
 * SHOW
 *   * Show bitmap
 *
 * Arguments are:
 * width, height    Size of image/area
 * xpos, ypos       Start blitting at X/Y position
 * alpha            Set alpha difference. 0 means same as original.
 *                  255 makes everything opaque
 *                  -255 makes everything transparent
 *                  If you set this to -255 you can then send a sequence of
 *                  ALPHA-commands to set the area to -225, -200, -175 etc
 *                  for a nice fade-in-effect! ;)
 * clear            Clear the framebuffer before blitting. 1 means clear.
 *                  If 0, the image will just be blitted on top of the old
 *                  one, so you don't need to send 1,8MB of RGBA32 data
 *                  everytime a small part of the screen is updated.
 *
 * Arguments for the filter are hidden:opaque:fifo
 * For example 1:0:/tmp/myfifo.fifo will start the filter hidden, transparent
 * and use /tmp/myfifo.fifo as the fifo.
 *
 * If you find bugs, please send me patches! ;)
 *
 * This filter was developed for use in Freevo (http://freevo.sf.net), but
 * anyone is free to use it! ;)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include "config.h"
#include "mp_image.h"
#include "vf.h"
#include "img_format.h"

#include "mp_msg.h"
#include "libavutil/common.h"

#include "libvo/fastmemcpy.h"

#define IS_RAWIMG	0x100
#define IS_IMG		0x200

#define NONE		0x000
#define IMG_RGBA32	0x101
#define IMG_ABGR32	0x102
#define IMG_RGB24	0x103
#define IMG_BGR24	0x104
#define IMG_PNG		0x201
#define CMD_CLEAR	0x001
#define CMD_ALPHA	0x002

#define TRUE  1
#define FALSE 0

#define INRANGE(a,b,c)	( ((a) < (b)) ? (b) : ( ((a) > (c)) ? (c) : (a) ) )

#define rgb2y(R,G,B)  ( (( 263*R + 516*G + 100*B) >> 10) + 16  )
#define rgb2u(R,G,B)  ( ((-152*R - 298*G + 450*B) >> 10) + 128 )
#define rgb2v(R,G,B)  ( (( 450*R - 376*G -  73*B) >> 10) + 128 )

#define DBG(a) (mp_msg(MSGT_VFILTER, MSGL_DBG2, "DEBUG: %d\n", a))

struct vf_priv_s {
    int w, h, x1, y1, x2, y2;
	struct {
		unsigned char *y, *u, *v, *a, *oa;
	} bitmap;
    int stream_fd;
	fd_set stream_fdset;
	int opaque, hidden;
};

static int
query_format(struct vf_instance_s* vf, unsigned int fmt){
    if(fmt==IMGFMT_YV12) return VFCAP_CSP_SUPPORTED;
    return 0;
}


static int
config(struct vf_instance_s* vf,
       int width, int height, int d_width, int d_height,
       unsigned int flags, unsigned int outfmt)
{
	vf->priv->bitmap.y  = malloc( width*height );
	vf->priv->bitmap.u  = malloc( width*height/4 );
	vf->priv->bitmap.v  = malloc( width*height/4 );
	vf->priv->bitmap.a  = malloc( width*height );
	vf->priv->bitmap.oa = malloc( width*height );
	if(!( vf->priv->bitmap.y &&
	      vf->priv->bitmap.u &&
		  vf->priv->bitmap.v &&
		  vf->priv->bitmap.a &&
		  vf->priv->bitmap.oa )) {
		mp_msg(MSGT_VFILTER, MSGL_ERR, "vf_bmovl: Could not allocate memory for bitmap buffer: %s\n", strerror(errno) );
		return FALSE;
	}

	// Set default to black...
	memset( vf->priv->bitmap.u, 128, width*height/4 );
	memset( vf->priv->bitmap.v, 128, width*height/4 );

    vf->priv->w  = vf->priv->x1 = width;
    vf->priv->h  = vf->priv->y1 = height;
    vf->priv->y2 = vf->priv->x2 = 0;

    return vf_next_config(vf, width, height, d_width, d_height, flags, outfmt);
}

static void
uninit(struct vf_instance_s *vf)
{
	if(vf->priv) {
		free(vf->priv->bitmap.y);
		free(vf->priv->bitmap.u);
		free(vf->priv->bitmap.v);
		free(vf->priv->bitmap.a);
		free(vf->priv->bitmap.oa);
		if (vf->priv->stream_fd >= 0)
		  close(vf->priv->stream_fd);
		free(vf->priv);
	}
}

static int
_read_cmd(int fd, char *cmd, char *args) {
	int done=FALSE, pos=0;
	char tmp;

	while(!done) {
		if(! read( fd, &tmp, 1 ) ) return FALSE;
		if( (tmp>='A' && tmp<='Z') || (tmp>='0' && tmp<='9') )
			cmd[pos]=tmp;
		else if(tmp == ' ') {
			cmd[pos]='\0';
			done=TRUE;
		}
		else if(tmp == '\n') {
			cmd[pos]='\0';
			args[0]='\0';
			return TRUE;
		}
		if(pos++>20) {
			cmd[0]='\0';
			return TRUE;
		}
	}
	done=FALSE; pos=0;
	while(!done) {
		if(! read( fd, &tmp, 1 ) ) return FALSE;
		if( (tmp >= ' ') && (pos<100) ) args[pos]=tmp;
		else {
			args[pos]='\0';
			done=TRUE;
		}
		pos++;
	}
	return TRUE;
}
			

static int
put_image(struct vf_instance_s* vf, mp_image_t* mpi, double pts){
	int buf_x=0, buf_y=0, buf_pos=0;
	int have, got, want;
	int xpos=0, ypos=0, pos=0;
	unsigned char red=0, green=0, blue=0;
	int  alpha;
	mp_image_t* dmpi;

    dmpi = vf_get_image(vf->next, mpi->imgfmt, MP_IMGTYPE_TEMP,
						MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
						mpi->w, mpi->h);

    memcpy_pic( dmpi->planes[0], mpi->planes[0], mpi->width, mpi->height, dmpi->stride[0], mpi->stride[0] );
    memcpy_pic( dmpi->planes[1], mpi->planes[1], mpi->chroma_width, mpi->chroma_height, dmpi->stride[1], mpi->stride[1] );
    memcpy_pic( dmpi->planes[2], mpi->planes[2], mpi->chroma_width, mpi->chroma_height, dmpi->stride[2], mpi->stride[2] );

    if(vf->priv->stream_fd >= 0) {
		struct timeval tv;
		int ready;

		FD_SET( vf->priv->stream_fd, &vf->priv->stream_fdset );
		tv.tv_sec=0; tv.tv_usec=0;

		ready = select( vf->priv->stream_fd+1, &vf->priv->stream_fdset, NULL, NULL, &tv );
		if(ready > 0) {
			// We've got new data from the FIFO

			char cmd[20], args[100];
			int  imgw,imgh,imgx,imgy,clear,imgalpha,pxsz=1,command;
			unsigned char *buffer = NULL;

			if(! _read_cmd( vf->priv->stream_fd, cmd, args) ) {
				mp_msg(MSGT_VFILTER, MSGL_ERR, "\nvf_bmovl: Error reading commands: %s\n\n", strerror(errno));
				return FALSE;
			}
			mp_msg(MSGT_VFILTER, MSGL_DBG2, "\nDEBUG: Got: %s+%s\n", cmd, args);

			command=NONE;
			if     ( strncmp(cmd,"RGBA32",6)==0 ) { pxsz=4; command = IMG_RGBA32; }
			else if( strncmp(cmd,"ABGR32",6)==0 ) { pxsz=4; command = IMG_ABGR32; }
			else if( strncmp(cmd,"RGB24" ,5)==0 ) { pxsz=3; command = IMG_RGB24;  }
			else if( strncmp(cmd,"BGR24" ,5)==0 ) { pxsz=3; command = IMG_BGR24;  }
			else if( strncmp(cmd,"CLEAR" ,5)==0 ) { pxsz=1; command = CMD_CLEAR;  }
			else if( strncmp(cmd,"ALPHA" ,5)==0 ) { pxsz=1; command = CMD_ALPHA;  }
			else if( strncmp(cmd,"OPAQUE",6)==0 ) vf->priv->opaque=TRUE;
			else if( strncmp(cmd,"SHOW",  4)==0 ) vf->priv->hidden=FALSE;
			else if( strncmp(cmd,"HIDE",  4)==0 ) vf->priv->hidden=TRUE;
			else if( strncmp(cmd,"FLUSH" ,5)==0 ) return vf_next_put_image(vf, dmpi, MP_NOPTS_VALUE);
			else {
			    mp_msg(MSGT_VFILTER, MSGL_WARN, "\nvf_bmovl: Unknown command: '%s'. Ignoring.\n", cmd);
			    return vf_next_put_image(vf, dmpi, MP_NOPTS_VALUE);
			}

			if(command == CMD_ALPHA) {
				sscanf( args, "%d %d %d %d %d", &imgw, &imgh, &imgx, &imgy, &imgalpha);
				mp_msg(MSGT_VFILTER, MSGL_DBG2, "\nDEBUG: ALPHA: %d %d %d %d %d\n\n",
					imgw, imgh, imgx, imgy, imgalpha);
				if(imgw==0 && imgh==0) vf->priv->opaque=FALSE;
			}

			if(command & IS_RAWIMG) {
				sscanf( args, "%d %d %d %d %d %d",
					&imgw, &imgh, &imgx, &imgy, &imgalpha, &clear);
				mp_msg(MSGT_VFILTER, MSGL_DBG2, "\nDEBUG: RAWIMG: %d %d %d %d %d %d\n\n",
					imgw, imgh, imgx, imgy, imgalpha, clear);

			    buffer = malloc(imgw*imgh*pxsz);
			    if(!buffer) {
			    	mp_msg(MSGT_VFILTER, MSGL_WARN, "\nvf_bmovl: Couldn't allocate temporary buffer! Skipping...\n\n");
					return vf_next_put_image(vf, dmpi, MP_NOPTS_VALUE);
			    }
  				/* pipes/sockets might need multiple calls to read(): */
			    want = (imgw*imgh*pxsz);
			    have = 0;
			    while (have < want) {
				got = read( vf->priv->stream_fd, buffer+have, want-have );
				if (got == 0) {
			    	    mp_msg(MSGT_VFILTER, MSGL_WARN, "\nvf_bmovl: premature EOF...\n\n");
				    break;
				}
				if (got < 0) {
			    	    mp_msg(MSGT_VFILTER, MSGL_WARN, "\nvf_bmovl: read error: %s\n\n", strerror(errno));
				    break;
				}
				have += got;
			    }
			    mp_msg(MSGT_VFILTER, MSGL_DBG2, "Got %d bytes... (wanted %d)\n", have, want );

				if(clear) {
					memset( vf->priv->bitmap.y,   0, vf->priv->w*vf->priv->h );
					memset( vf->priv->bitmap.u, 128, vf->priv->w*vf->priv->h/4 );
					memset( vf->priv->bitmap.v, 128, vf->priv->w*vf->priv->h/4 );
					memset( vf->priv->bitmap.a,   0, vf->priv->w*vf->priv->h );
					memset( vf->priv->bitmap.oa,  0, vf->priv->w*vf->priv->h );
					vf->priv->x1 = dmpi->width;
					vf->priv->y1 = dmpi->height;
					vf->priv->x2 = vf->priv->y2 = 0;
				}
				// Define how much of our bitmap that contains graphics!
				vf->priv->x1 = av_clip(imgx, 0, vf->priv->x1);
				vf->priv->y1 = av_clip(imgy, 0, vf->priv->y1);
				vf->priv->x2 = av_clip(imgx + imgw, vf->priv->x2, vf->priv->w);
				vf->priv->y2 = av_clip(imgy + imgh, vf->priv->y2, vf->priv->h);
			}
			
			if( command == CMD_CLEAR ) {
				sscanf( args, "%d %d %d %d", &imgw, &imgh, &imgx, &imgy);
				mp_msg(MSGT_VFILTER, MSGL_DBG2, "\nDEBUG: CLEAR: %d %d %d %d\n\n", imgw, imgh, imgx, imgy);

				for( ypos=imgy ; (ypos < (imgy+imgh)) && (ypos < vf->priv->y2) ; ypos++ ) {
					memset( vf->priv->bitmap.y  + (ypos*vf->priv->w) + imgx, 0, imgw );
					memset( vf->priv->bitmap.a  + (ypos*vf->priv->w) + imgx, 0, imgw );
					memset( vf->priv->bitmap.oa + (ypos*vf->priv->w) + imgx, 0, imgw );
					if(ypos%2) {
						memset( vf->priv->bitmap.u + ((ypos/2)*dmpi->stride[1]) + (imgx/2), 128, imgw/2 );
						memset( vf->priv->bitmap.v + ((ypos/2)*dmpi->stride[2]) + (imgx/2), 128, imgw/2 );
					}
				}	// Recalculate area that contains graphics
				if( (imgx <= vf->priv->x1) && ( (imgw+imgx) >= vf->priv->x2) ) {
					if( (imgy <= vf->priv->y1) && ( (imgy+imgh) >= vf->priv->y1) )
						vf->priv->y1 = imgy+imgh;
					if( (imgy <= vf->priv->y2) && ( (imgy+imgh) >= vf->priv->y2) )
						vf->priv->y2 = imgy;
				}
				if( (imgy <= vf->priv->y1) && ( (imgy+imgh) >= vf->priv->y2) ) {
					if( (imgx <= vf->priv->x1) && ( (imgx+imgw) >= vf->priv->x1) )
						vf->priv->x1 = imgx+imgw;
					if( (imgx <= vf->priv->x2) && ( (imgx+imgw) >= vf->priv->x2) )
						vf->priv->x2 = imgx;
				}
				return vf_next_put_image(vf, dmpi, MP_NOPTS_VALUE);
			}

			for( buf_y=0 ; (buf_y < imgh) && (buf_y < (vf->priv->h-imgy)) ; buf_y++ ) {
			    for( buf_x=0 ; (buf_x < (imgw*pxsz)) && (buf_x < ((vf->priv->w+imgx)*pxsz)) ; buf_x += pxsz ) {
					if(command & IS_RAWIMG) buf_pos = (buf_y * imgw * pxsz) + buf_x;
					pos = ((buf_y+imgy) * vf->priv->w) + ((buf_x/pxsz)+imgx);

					switch(command) {
						case IMG_RGBA32:
							red   = buffer[buf_pos+0];
							green = buffer[buf_pos+1];
							blue  = buffer[buf_pos+2];
							alpha = buffer[buf_pos+3];
							break;
						case IMG_ABGR32:
							alpha = buffer[buf_pos+0];
							blue  = buffer[buf_pos+1];
							green = buffer[buf_pos+2];
							red   = buffer[buf_pos+3];
							break;
						case IMG_RGB24:
							red   = buffer[buf_pos+0];
							green = buffer[buf_pos+1];
							blue  = buffer[buf_pos+2];
							alpha = 0xFF;
		    				break;
						case IMG_BGR24:
							blue  = buffer[buf_pos+0];
							green = buffer[buf_pos+1];
							red   = buffer[buf_pos+2];
							alpha = 0xFF;
		    				break;
						case CMD_ALPHA:
							vf->priv->bitmap.a[pos] = INRANGE((vf->priv->bitmap.oa[pos]+imgalpha),0,255);
							break;
						default:
					   		mp_msg(MSGT_VFILTER, MSGL_ERR, "vf_bmovl: Internal error!\n");
							return FALSE;
					}
					if( command & IS_RAWIMG ) {
						vf->priv->bitmap.y[pos]  = rgb2y(red,green,blue);
						vf->priv->bitmap.oa[pos] = alpha;
						vf->priv->bitmap.a[pos]  = INRANGE((alpha+imgalpha),0,255);
						if((buf_y%2) && ((buf_x/pxsz)%2)) {
							pos = ( ((buf_y+imgy)/2) * dmpi->stride[1] ) + (((buf_x/pxsz)+imgx)/2);
							vf->priv->bitmap.u[pos] = rgb2u(red,green,blue);
							vf->priv->bitmap.v[pos] = rgb2v(red,green,blue);
						}
					}
				} // for buf_x
			} // for buf_y
			free (buffer);
		} else if(ready < 0) {
			mp_msg(MSGT_VFILTER, MSGL_WARN, "\nvf_bmovl: Error %d in fifo: %s\n\n", errno, strerror(errno));
		}
    }

	if(vf->priv->hidden) return vf_next_put_image(vf, dmpi, MP_NOPTS_VALUE);

	if(vf->priv->opaque) {	// Just copy buffer memory to screen
		for( ypos=vf->priv->y1 ; ypos < vf->priv->y2 ; ypos++ ) {
			memcpy( dmpi->planes[0] + (ypos*dmpi->stride[0]) + vf->priv->x1,
			        vf->priv->bitmap.y + (ypos*vf->priv->w) + vf->priv->x1,
					vf->priv->x2 - vf->priv->x1 );
			if(ypos%2) {
				memcpy( dmpi->planes[1] + ((ypos/2)*dmpi->stride[1]) + (vf->priv->x1/2),
				        vf->priv->bitmap.u + (((ypos/2)*(vf->priv->w)/2)) + (vf->priv->x1/2),
				        (vf->priv->x2 - vf->priv->x1)/2 );
				memcpy( dmpi->planes[2] + ((ypos/2)*dmpi->stride[2]) + (vf->priv->x1/2),
				        vf->priv->bitmap.v + (((ypos/2)*(vf->priv->w)/2)) + (vf->priv->x1/2),
				        (vf->priv->x2 - vf->priv->x1)/2 );
			}
		}
	} else { // Blit the bitmap to the videoscreen, pixel for pixel
	    for( ypos=vf->priv->y1 ; ypos < vf->priv->y2 ; ypos++ ) {
	        for ( xpos=vf->priv->x1 ; xpos < vf->priv->x2 ; xpos++ ) {
				pos = (ypos * dmpi->stride[0]) + xpos;

				alpha = vf->priv->bitmap.a[pos];

				if (alpha == 0) continue; // Completly transparent pixel

				if (alpha == 255) {	// Opaque pixel
					dmpi->planes[0][pos] = vf->priv->bitmap.y[pos];
					if ((ypos%2) && (xpos%2)) {
						pos = ( (ypos/2) * dmpi->stride[1] ) + (xpos/2);
						dmpi->planes[1][pos] = vf->priv->bitmap.u[pos];
						dmpi->planes[2][pos] = vf->priv->bitmap.v[pos];
					}
				} else { // Alphablended pixel
					dmpi->planes[0][pos] = 
						((255 - alpha) * (int)dmpi->planes[0][pos] + 
						alpha * (int)vf->priv->bitmap.y[pos]) >> 8;
					
					if ((ypos%2) && (xpos%2)) {
						pos = ( (ypos/2) * dmpi->stride[1] ) + (xpos/2);

						dmpi->planes[1][pos] = 
							((255 - alpha) * (int)dmpi->planes[1][pos] + 
							alpha * (int)vf->priv->bitmap.u[pos]) >> 8;
						
						dmpi->planes[2][pos] = 
							((255 - alpha) * (int)dmpi->planes[2][pos] + 
							alpha * (int)vf->priv->bitmap.v[pos]) >> 8;
					}
			    }
			} // for xpos
		} // for ypos
	} // if !opaque
    return vf_next_put_image(vf, dmpi, MP_NOPTS_VALUE);
} // put_image

static int
vf_open(vf_instance_t* vf, char* args)
{
    char filename[1000];

    vf->config = config;
    vf->put_image = put_image;
    vf->query_format = query_format;
	vf->uninit = uninit;

    vf->priv = malloc(sizeof(struct vf_priv_s));

	if(!args || sscanf(args, "%d:%d:%s", &vf->priv->hidden, &vf->priv->opaque, filename) < 3 ) {
        mp_msg(MSGT_VFILTER, MSGL_ERR, "vf_bmovl: Bad arguments!\n");
		mp_msg(MSGT_VFILTER, MSGL_ERR, "vf_bmovl: Arguments are 'bool hidden:bool opaque:string fifo'\n");
		return FALSE;
    }

    vf->priv->stream_fd = open(filename, O_RDWR);
    if(vf->priv->stream_fd >= 0) {
		FD_ZERO( &vf->priv->stream_fdset );
		mp_msg(MSGT_VFILTER, MSGL_INFO, "vf_bmovl: Opened fifo %s as FD %d\n", filename, vf->priv->stream_fd);
	} else {
		mp_msg(MSGT_VFILTER, MSGL_WARN, "vf_bmovl: Error! Couldn't open FIFO %s: %s\n", filename, strerror(errno));
		vf->priv->stream_fd = -1;
    }

    return TRUE;
}

vf_info_t vf_info_bmovl = {
    "Read bitmaps from a FIFO and display them in window",
    "bmovl",
    "Per Wigren",
    "",
    vf_open,
    NULL
};
