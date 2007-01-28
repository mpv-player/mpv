/* 
 * vo_tga.c: targa output
 *
 * this video output module write targa uncompressed file in 15, 24 and 32 bit bgr format.
 *
 * to select the output format use the format filter:
 *  mplayer -vo tga -vf format=bgr15 ...
 *  mplayer -vo tga -vf format=bgr24 ...
 *  mplayer -vo tga -vf format=bgr32 ...
 *
 * The 16 bit file are loaded without problem from Gimp and ImageMagick but give an error
 * with entice (a visualizer from the enlightenment package that use the imlib2 package).
 *
 * In 32 bit mode the alpha channel is set to 255 (0xff). For big endian
 * machines, TGA_ALPHA32 changes from 0xff000000 to 0x000000ff, and TGA_SHIFT32 from 0 to 8.
 *
 * I need to fill the alpha channel because entice consider that alpha channel (and displays
 * nothing, only the background!), but ImageMacick (the program display) or gimp doesn't
 * care.
 *
 * maybe is possible (with a compilation switch) to avoid the fill of the alpha channel
 * and work outside mplayer (if needed)
 *
 *    Daniele Forghieri ( guru@digitalfantasy.it )
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "video_out.h"
#include "video_out_internal.h"

static vo_info_t info =
{
	"Targa output",
	"tga",
	"Daniele Forghieri - guru@digitalfantasy.it",
	""
};


LIBVO_EXTERN (tga)

/* locals vars */
static int      frame_num = 0;
static void     *line_buff;

static void tga_make_header(uint8_t *h, int dx, int dy, int bpp)
{

    int  i;

    for(i = 0; i < 18; i++) {
        switch (i) {
        case 2:
            *h = 0x02;
            break;

        case 12:
            *h = dx & 0xff;
            break;

        case 13:
            *h = (dx >> 8) & 0xff;
            break;

        case 14:
            *h = dy & 0xff;
            break;

        case 15:
            *h = (dy >> 8) & 0xff;
            break;

        case 16:
            *h = bpp;
            break;

        case 17:
            *h = 0x20;
            break;

        default:
            *h = 0;
        }
        ++h;
    }

}

static int write_tga( char *file, int bpp, int dx, int dy, uint8_t *buf, int stride)
{
    int   er;
    FILE  *fo;

    fo = fopen(file, "wb");
    if (fo != NULL) {
        uint8_t hdr[18];

        er = 0;
        tga_make_header(hdr, dx, dy, bpp);
        if (fwrite(hdr, sizeof(hdr), 1, fo) == 1) {
            int    wb;

            wb = ((bpp + 7) / 8) * dx;
            if (bpp == 32) {
                /* Setup the alpha channel for every pixel */
                while (dy-- > 0) {
                    uint8_t    *d;
                    uint8_t    *s;
                    int         x;

                    s = buf;
                    d = line_buff;
                    for(x = 0; x < dx; x++) {
                    #ifdef WORDS_BIGENDIAN
                        d[0] = s[3];
                        d[1] = s[2];
                        d[2] = s[1];
                        d[3] = 0xff;
                    #else
                        d[0] = 0xff;
                        d[1] = s[1];
                        d[2] = s[2];
                        d[3] = s[3];
                    #endif
                        d+=4;
                        s+=4;
                    }
                    if (fwrite(line_buff, wb, 1, fo) != 1) {
                        er = 4;
                        break;
                    }
                    buf += stride;
                }

            }
            else {
                while (dy-- > 0) {
                    if (fwrite(buf, wb, 1, fo) != 1) {
                        er = 4;
                        break;
                    }
                    buf += stride;
                }
            }
        }
        else {
            er = 2;
        }

        fclose(fo);
    }
    else {
        er = 1;
    }

    if (er) {
        fprintf(stderr, "Error writing file [%s]\n", file);
    }
    return(er);
}

static uint32_t draw_image(mp_image_t* mpi)
{
    char    file[20 + 1];

    snprintf (file, 20, "%08d.tga", ++frame_num);

    write_tga( file,
               mpi->bpp,
               mpi->w,
               mpi->h,
               mpi->planes[0],
               mpi->stride[0]);

    return VO_TRUE;
}

static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
    /* buffer for alpha */
    if(line_buff){ free(line_buff); line_buff=NULL; }
    if (format == (IMGFMT_BGR | 32)) {
        line_buff = malloc(width * 4);
    }
    return 0;
}

static void draw_osd(void)
{
}

static void flip_page (void)
{
    return;
}

static int draw_slice(uint8_t *srcimg[], int stride[], int w,int h,int x,int y)
{
    return -1;
}

static int draw_frame(uint8_t * src[])
{
    return -1;
}

static int query_format(uint32_t format)
{
    switch(format){
        case IMGFMT_BGR|15:
        case IMGFMT_BGR|24:
        case IMGFMT_BGR|32:
            return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW;
    }
    return 0;
}

static void uninit(void)
{
    if(line_buff){ free(line_buff); line_buff=NULL; }
}

static void check_events(void)
{
}

static int preinit(const char *arg)
{
    if(arg) {
	mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_TGA_UnknownSubdevice,arg);
	return ENOSYS;
    }
    return 0;
}

static int control(uint32_t request, void *data, ...)
{
  switch (request) {
      case VOCTRL_DRAW_IMAGE:
          return draw_image(data);

      case VOCTRL_QUERY_FORMAT:
          return query_format(*((uint32_t*)data));
  }
  return VO_NOTIMPL;
}
