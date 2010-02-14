/*
 * Portable Network Graphics renderer
 *
 * Copyright 2001 by Felix Buenemann <atmosfear@users.sourceforge.net>
 *
 * Uses libpng (which uses zlib), so see according licenses.
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "fmt-conversion.h"
#include "mp_core.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "subopt-helper.h"
#include "libavcodec/avcodec.h"

#define BUFLENGTH 512

static const vo_info_t info =
{
	"PNG file",
	"png",
	"Felix Buenemann <atmosfear@users.sourceforge.net>",
	""
};

const LIBVO_EXTERN (png)

static int z_compression;
static char *png_outdir;
static int framenum;
static int use_alpha;
static AVCodecContext *avctx;
static uint8_t *outbuffer;
int outbuffer_size;

static void png_mkdir(char *buf, int verbose) {
    struct stat stat_p;

#ifndef __MINGW32__
    if ( mkdir(buf, 0755) < 0 ) {
#else
    if ( mkdir(buf) < 0 ) {
#endif
        switch (errno) { /* use switch in case other errors need to be caught
                            and handled in the future */
            case EEXIST:
                if ( stat(buf, &stat_p ) < 0 ) {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s: %s\n", info.short_name,
                            MSGTR_VO_GenericError, strerror(errno) );
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s %s\n", info.short_name,
                            MSGTR_VO_UnableToAccess,buf);
                    exit_player(EXIT_ERROR);
                }
                if ( !S_ISDIR(stat_p.st_mode) ) {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s %s\n", info.short_name,
                            buf, MSGTR_VO_ExistsButNoDirectory);
                    exit_player(EXIT_ERROR);
                }
                if ( !(stat_p.st_mode & S_IWUSR) ) {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n", info.short_name,
                            buf, MSGTR_VO_DirExistsButNotWritable);
                    exit_player(EXIT_ERROR);
                }

                mp_msg(MSGT_VO, MSGL_INFO, "%s: %s - %s\n", info.short_name,
                        buf, MSGTR_VO_DirExistsAndIsWritable);
                break;

            default:
                mp_msg(MSGT_VO, MSGL_ERR, "%s: %s: %s\n", info.short_name,
                        MSGTR_VO_GenericError, strerror(errno) );
                mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n", info.short_name,
                        buf, MSGTR_VO_CantCreateDirectory);
                exit_player(EXIT_ERROR);
        } /* end switch */
    } else if ( verbose ) {
        mp_msg(MSGT_VO, MSGL_INFO, "%s: %s - %s\n", info.short_name,
                buf, MSGTR_VO_DirectoryCreateSuccess);
    } /* end if */
}

static int
config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
    char buf[BUFLENGTH];

	    if(z_compression == 0) {
 		    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_PNG_Warning1);
 		    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_PNG_Warning2);
 		    mp_msg(MSGT_VO,MSGL_INFO, MSGTR_LIBVO_PNG_Warning3);
	    }

    snprintf(buf, BUFLENGTH, "%s", png_outdir);
    png_mkdir(buf, 1);
    mp_msg(MSGT_VO,MSGL_DBG2, "PNG Compression level %i\n", z_compression);

    return 0;
}


static uint32_t draw_image(mp_image_t* mpi){
    AVFrame pic;
    int buffersize;
    int res;
    char buf[100];
    FILE *outfile;

    // if -dr or -slices then do nothing:
    if(mpi->flags&(MP_IMGFLAG_DIRECT|MP_IMGFLAG_DRAW_CALLBACK)) return VO_TRUE;

    snprintf (buf, 100, "%s/%08d.png", png_outdir, ++framenum);
    outfile = fopen(buf, "wb");
    if (!outfile) {
        mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_PNG_ErrorOpeningForWriting, strerror(errno));
        return 1;
    }

    avctx->width = mpi->w;
    avctx->height = mpi->h;
    avctx->pix_fmt = imgfmt2pixfmt(mpi->imgfmt);
    pic.data[0] = mpi->planes[0];
    pic.linesize[0] = mpi->stride[0];
    buffersize = mpi->w * mpi->h * 8;
    if (outbuffer_size < buffersize) {
        av_freep(&outbuffer);
        outbuffer = av_malloc(buffersize);
        outbuffer_size = buffersize;
    }
    res = avcodec_encode_video(avctx, outbuffer, outbuffer_size, &pic);

    if(res < 0){
 	    mp_msg(MSGT_VO,MSGL_WARN, MSGTR_LIBVO_PNG_ErrorInCreatePng);
            fclose(outfile);
	    return 1;
    }

    fwrite(outbuffer, res, 1, outfile);
    fclose(outfile);

    return VO_TRUE;
}

static void draw_osd(void){}

static void flip_page (void){}

static int draw_frame(uint8_t * src[])
{
    return -1;
}

static int draw_slice( uint8_t *src[],int stride[],int w,int h,int x,int y )
{
    return -1;
}

static int
query_format(uint32_t format)
{
    const int supported_flags = VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_ACCEPT_STRIDE;
    switch(format){
    case IMGFMT_RGB24:
        return use_alpha ? 0 : supported_flags;
    case IMGFMT_BGR32:
        return use_alpha ? supported_flags : 0;
    }
    return 0;
}

static void uninit(void){
    avcodec_close(avctx);
    av_freep(&avctx);
    av_freep(&outbuffer);
    outbuffer_size = 0;
    if (png_outdir) {
        free(png_outdir);
        png_outdir = NULL;
    }
}

static void check_events(void){}

static int int_zero_to_nine(void *value)
{
    int *sh = value;
    return *sh >= 0 && *sh <= 9;
}

static const opt_t subopts[] = {
    {"alpha", OPT_ARG_BOOL, &use_alpha, NULL},
    {"z",   OPT_ARG_INT, &z_compression, int_zero_to_nine},
    {"outdir",      OPT_ARG_MSTRZ,  &png_outdir,           NULL},
    {NULL}
};

static int preinit(const char *arg)
{
    z_compression = 0;
    png_outdir = strdup(".");
    use_alpha = 0;
    if (subopt_parse(arg, subopts) != 0) {
        return -1;
    }
    avcodec_register_all();
    avctx = avcodec_alloc_context();
    if (avcodec_open(avctx, avcodec_find_encoder(CODEC_ID_PNG)) < 0) {
        uninit();
        return -1;
    }
    avctx->compression_level = z_compression;
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
