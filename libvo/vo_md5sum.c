/* ------------------------------------------------------------------------- */

/*
 * md5sum video output driver
 *
 * Copyright (C) 2004, 2005, 2006 Ivo van Poorten
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

/* ------------------------------------------------------------------------- */

/* Global Includes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ------------------------------------------------------------------------- */

/* Local Includes */

#include "config.h"
#include "subopt-helper.h"
#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mplayer.h"			/* for exit_player() */
#include "help_mp.h"
#include "libavutil/md5.h"

/* ------------------------------------------------------------------------- */

/* Defines */

/* Used for temporary buffers to store file- and pathnames */
#define BUFLENGTH 512

/* ------------------------------------------------------------------------- */

/* Info */

static const vo_info_t info=
{
	"md5sum of each frame",
	"md5sum",
	"Ivo van Poorten (ivop@euronet.nl)",
	""
};

const LIBVO_EXTERN (md5sum)

/* ------------------------------------------------------------------------- */

/* Global Variables */

char *md5sum_outfile = NULL;

FILE *md5sum_fd;
int framenum = 0;

/* ------------------------------------------------------------------------- */

/** \brief An error occured while writing to a file.
 *
 * The program failed to write data to a file.
 * It displays a message and exits the player.
 *
 * \return nothing It does not return.
 */

static void md5sum_write_error(void) {
    mp_msg(MSGT_VO, MSGL_ERR, MSGTR_ErrorWritingFile, info.short_name);
    exit_player(MSGTR_Exit_error);
}

/* ------------------------------------------------------------------------- */

/** \brief Pre-initialisation.
 *
 * This function is called before initialising the video output driver. It
 * parses all suboptions and sets variables accordingly. If an error occurs
 * (like an option being out of range, not having any value or an unknown
 * option is stumbled upon) the player will exit. It also sets default
 * values if necessary.
 *
 * \param arg   A string containing all the suboptions passed to the video
 *              output driver.
 *
 * \return 0    All went well.
 */

static int preinit(const char *arg)
{
    opt_t subopts[] = {
        {"outfile",     OPT_ARG_MSTRZ,    &md5sum_outfile,   NULL, 0},
        {NULL, 0, NULL, NULL, 0}
    };

    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                                            MSGTR_VO_ParsingSuboptions);

    md5sum_outfile = strdup("md5sums");
    if (subopt_parse(arg, subopts) != 0) {
        return -1;
    }

    mp_msg(MSGT_VO, MSGL_V, "%s: outfile --> %s\n", info.short_name,
                                                            md5sum_outfile);

    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                                            MSGTR_VO_SuboptionsParsedOK);
    return 0;
}

/* ------------------------------------------------------------------------- */

/** \brief Configure the video output driver.
 *
 * This functions configures the video output driver. It opens the output
 * file to which this driver will write all the MD5 sums. If something
 * goes wrong, the player will exit.
 *
 *  \return 0             All went well.
 */

static int config(uint32_t width, uint32_t height, uint32_t d_width,
                       uint32_t d_height, uint32_t flags, char *title,
                       uint32_t format)
{
    if (vo_config_count > 0 ) { /* Already configured */
        return 0;
    }

    if ( (md5sum_fd = fopen(md5sum_outfile, "w") ) == NULL ) {
        mp_msg(MSGT_VO, MSGL_ERR, "\n%s: %s\n", info.short_name,
                MSGTR_VO_CantCreateFile);
        mp_msg(MSGT_VO, MSGL_ERR, "%s: %s: %s\n",
                info.short_name, MSGTR_VO_GenericError, strerror(errno) );
        exit_player(MSGTR_Exit_error);
    }

    return 0;
}

/* ------------------------------------------------------------------------- */

/** \brief Write MD5 sum to output file.
 *
 * This function writes an ASCII representation of a 16-byte hexadecimal
 * MD5 sum to our output file. The file descriptor is a global variable.
 *
 * \param md5sum Sixteen bytes that represent an MD5 sum.
 *
 * \return None     The player will exit if a write error occurs.
 */

static void md5sum_output_sum(unsigned char *md5sum) {
    int i;

    for(i=0; i<16; i++) {
        if ( fprintf(md5sum_fd, "%02x", md5sum[i]) < 0 ) md5sum_write_error();
    }
    if ( fprintf(md5sum_fd, " frame%08d\n", framenum) < 0 )
        md5sum_write_error();

    framenum++;
}

/* ------------------------------------------------------------------------- */

static int draw_frame(uint8_t *src[])
{
    mp_msg(MSGT_VO, MSGL_V, "%s: draw_frame() is called!\n", info.short_name);
    return -1;
}

/* ------------------------------------------------------------------------- */

static uint32_t draw_image(mp_image_t *mpi)
{
    unsigned char md5sum[16];
    uint32_t w = mpi->w;
    uint32_t h = mpi->h;
    uint8_t *rgbimage = mpi->planes[0];
    uint8_t *planeY = mpi->planes[0];
    uint8_t *planeU = mpi->planes[1];
    uint8_t *planeV = mpi->planes[2];
    uint32_t strideY = mpi->stride[0];
    uint32_t strideU = mpi->stride[1];
    uint32_t strideV = mpi->stride[2];

    uint8_t md5_context_memory[av_md5_size];
    struct AVMD5 *md5_context = (struct AVMD5*) md5_context_memory;
    unsigned int i;

    if (mpi->flags & MP_IMGFLAG_PLANAR) { /* Planar */
        if (mpi->flags & MP_IMGFLAG_YUV) { /* Planar YUV */
            av_md5_init(md5_context);
            for (i=0; i<h; i++) {
                av_md5_update(md5_context, planeY + i * strideY, w);
            }
            w = w / 2;
            h = h / 2;
            for (i=0; i<h; i++) {
                av_md5_update(md5_context, planeU + i * strideU, w);
                av_md5_update(md5_context, planeV + i * strideV, w);
            }
            av_md5_final(md5_context, md5sum);
            md5sum_output_sum(md5sum);
            return VO_TRUE;
        } else { /* Planar RGB */
            return VO_FALSE;
        }
    } else { /* Packed */
        if (mpi->flags & MP_IMGFLAG_YUV) { /* Packed YUV */
            
            return VO_FALSE;
        } else { /* Packed RGB */
            av_md5_sum(md5sum, rgbimage, mpi->w * (mpi->bpp >> 3) * mpi->h);
            md5sum_output_sum(md5sum);
            return VO_TRUE;
        }
    }

    return VO_FALSE;
}

/* ------------------------------------------------------------------------- */

static int draw_slice(uint8_t *src[], int stride[], int w, int h,
                           int x, int y)
{
    return 0;
}

/* ------------------------------------------------------------------------- */

static int query_format(uint32_t format)
{
    switch (format) {
        case IMGFMT_RGB24:
        case IMGFMT_YV12:
            return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW;
        default:
            return 0;
    }
}

/* ------------------------------------------------------------------------- */

static int control(uint32_t request, void *data, ...)
{
    switch (request) {
        case VOCTRL_QUERY_FORMAT:
            return query_format(*((uint32_t*)data));
        case VOCTRL_DRAW_IMAGE:
            return draw_image(data);
    }
    return VO_NOTIMPL;
}

/* ------------------------------------------------------------------------- */

static void uninit(void)
{
    if (md5sum_outfile) {
        free(md5sum_outfile);
        md5sum_outfile = NULL;
    }
    if (md5sum_fd) fclose(md5sum_fd);
}

/* ------------------------------------------------------------------------- */

static void check_events(void)
{
}

/* ------------------------------------------------------------------------- */

static void draw_osd(void)
{
}

/* ------------------------------------------------------------------------- */

static void flip_page (void)
{
}

/* ------------------------------------------------------------------------- */

#undef BUFLENGTH
#undef MD5SUM_RGB_MODE
#undef MD5SUM_YUV_MODE

/* ------------------------------------------------------------------------- */

