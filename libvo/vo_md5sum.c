/* ------------------------------------------------------------------------- */

/* 
 * vo_md5sum.c, md5sum Video Output Driver for MPlayer
 *
 * 
 * Written by Ivo van Poorten. (GPL)2004
 *
 *
 * Changelog
 * 
 * 2004-09-13   First draft.
 * 2004-09-16   Second draft. It now acts on VOCTRL_DRAW_IMAGE and does not
 *              maintain a local copy of the image if the format is YV12.
 *              Speed improvement and uses less memory.
 *
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
#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mplayer.h"			/* for exit_player() */
#include "help_mp.h"
#include "md5sum.h"

/* ------------------------------------------------------------------------- */

/* Defines */

/* Used for temporary buffers to store file- and pathnames */
#define BUFLENGTH 512

/* ------------------------------------------------------------------------- */

/* Info */

static vo_info_t info=
{
	"md5sum of each frame",
	"md5sum",
	"Ivo van Poorten (ivop@euronet.nl)",
	""
};

LIBVO_EXTERN (md5sum)

/* ------------------------------------------------------------------------- */

/* Global Variables */

char *md5sum_outfile = NULL;

FILE *md5sum_fd;
int framenum = 0;

/* ------------------------------------------------------------------------- */

/** \brief Memory allocation failed.
 *
 * The video output driver failed to allocate a block of memory it needed.
 * It displays a message and exits the player.
 *
 * \return nothing It does not return.
 */

void md5sum_malloc_failed(void) {
    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s\n", info.short_name,
            MSGTR_MemAllocFailed);
    exit_player(MSGTR_Exit_error);
}

/* ------------------------------------------------------------------------- */

/** \brief An error occured while writing to a file.
 *
 * The program failed to write data to a file.
 * It displays a message and exits the player.
 *
 * \return nothing It does not return.
 */

void md5sum_write_error(void) {
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

static uint32_t preinit(const char *arg)
{
    char *buf;      /* buf is used to store parsed string values */

    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                                            MSGTR_VO_ParsingSuboptions);
    
    if (arg) {

        while (*arg != '\0') {
            if (!strncmp(arg, ":", 1)) {
                arg++;
                continue;   /* multiple ':' is not really an error */
            } else if (!strncmp(arg, "outfile=", 8)) {
                arg += 8;
                buf = malloc(strlen(arg)+1); /* maximum length possible */
                if (!buf) {
                    md5sum_malloc_failed(); /* message and exit_player! */
                }
                if (sscanf(arg, "%[^:]", buf) == 1) {
                    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s --> %s\n",
                            info.short_name, "outfile", buf);
                    arg += strlen(buf);
                    md5sum_outfile = strdup(buf);
                    if (!md5sum_outfile) md5sum_malloc_failed();
                    free(buf);
                } else {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n",
                            info.short_name, "outfile",
                            MSGTR_VO_NoValueSpecified);
                    exit_player(MSGTR_Exit_error);
                }
            } else {
                mp_msg(MSGT_VO, MSGL_ERR, "%s: %s %-20s...\n", info.short_name,
                        MSGTR_VO_UnknownSuboptions, arg);
                exit_player(MSGTR_Exit_error);
            }
        } /* end while */
    } /* endif */
    
    /* If md5sum_outfile is not set by an option, resort to default of
                                                                "md5sums" */
    if (!md5sum_outfile) {
        md5sum_outfile = strdup("md5sums");
        if (!md5sum_outfile) md5sum_malloc_failed();
    }

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

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
                       uint32_t d_height, uint32_t fullscreen, char *title,
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

static uint32_t draw_frame(uint8_t *src[])
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

    auth_md5Ctx md5_context;
    int i;

    if (mpi->flags & MP_IMGFLAG_PLANAR) { /* Planar */
        if (mpi->flags & MP_IMGFLAG_YUV) { /* Planar YUV */
            auth_md5InitCtx(&md5_context);
            for (i=0; i<h; i++) {
                auth_md5SumCtx(&md5_context, planeY + i * strideY, w);
            }
            w = w / 2;
            h = h / 2;
            for (i=0; i<h; i++) {
                auth_md5SumCtx(&md5_context, planeU + i * strideU, w);
                auth_md5SumCtx(&md5_context, planeV + i * strideV, w);
            }
            auth_md5CloseCtx(&md5_context, md5sum);
            md5sum_output_sum(md5sum);
            return VO_TRUE;
        } else { /* Planar RGB */
            return VO_FALSE;
        }
    } else { /* Packed */
        if (mpi->flags & MP_IMGFLAG_YUV) { /* Packed YUV */
            
            return VO_FALSE;
        } else { /* Packed RGB */
            auth_md5Sum(md5sum, rgbimage, mpi->w * (mpi->bpp >> 3) * mpi->h);
            md5sum_output_sum(md5sum);
            return VO_TRUE;
        }
    }

    return VO_FALSE;
}

/* ------------------------------------------------------------------------- */

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h,
                           int x, int y)
{
    return 0;
}

/* ------------------------------------------------------------------------- */

static uint32_t query_format(uint32_t format)
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

static uint32_t control(uint32_t request, void *data, ...)
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

