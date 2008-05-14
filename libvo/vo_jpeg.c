/*
 * JPEG Renderer for MPlayer
 *
 * Copyright (C) 2002 by Pontscho <pontscho@makacs.poliod.hu>
 * Copyright (C) 2003 by Alex
 * Copyright (C) 2004, 2005 by Ivo van Poorten <ivop@euronet.nl>
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
#include <jpeglib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ------------------------------------------------------------------------- */

/* Local Includes */

#include "config.h"
#include "subopt-helper.h"
#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mplayer.h"			/* for exit_player() */
#include "help_mp.h"

/* ------------------------------------------------------------------------- */

/* Defines */

/* Used for temporary buffers to store file- and pathnames */
#define BUFLENGTH 512

/* ------------------------------------------------------------------------- */

/* Info */

static const vo_info_t info=
{
	"JPEG file",
	"jpeg",
	"Zoltan Ponekker (pontscho@makacs.poliod.hu)",
	""
};

const LIBVO_EXTERN (jpeg)

/* ------------------------------------------------------------------------- */

/* Global Variables */

static int image_width;
static int image_height;
static int image_d_width;
static int image_d_height;

int jpeg_baseline = 1;
int jpeg_progressive_mode = 0;
int jpeg_optimize = 100;
int jpeg_smooth = 0;
int jpeg_quality = 75;
int jpeg_dpi = 72; /** Screen resolution = 72 dpi */
char *jpeg_outdir = NULL;
char *jpeg_subdirs = NULL;
int jpeg_maxfiles = 1000;

static int framenum = 0;

/* ------------------------------------------------------------------------- */

/** \brief Create a directory.
 *
 *  This function creates a directory. If it already exists, it tests if
 *  it's a directory and not something else, and if it is, it tests whether
 *  the directory is writable or not.
 *
 * \param buf       Pointer to directory name.
 * \param verbose   Verbose on success. If verbose is non-zero, it will print
 *                  a message if it was successful in creating the directory.
 *
 * \return nothing  In case anything fails, the player will exit. If it
 *                  returns, everything went well.
 */

static void jpeg_mkdir(char *buf, int verbose) { 
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
                    exit_player(MSGTR_Exit_error);
                }
                if ( !S_ISDIR(stat_p.st_mode) ) {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s %s\n", info.short_name,
                            buf, MSGTR_VO_ExistsButNoDirectory);
                    exit_player(MSGTR_Exit_error);
                }
                if ( !(stat_p.st_mode & S_IWUSR) ) {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n", info.short_name,
                            buf, MSGTR_VO_DirExistsButNotWritable);
                    exit_player(MSGTR_Exit_error);
                }
                
                mp_msg(MSGT_VO, MSGL_INFO, "%s: %s - %s\n", info.short_name,
                        buf, MSGTR_VO_DirExistsAndIsWritable);
                break;

            default:
                mp_msg(MSGT_VO, MSGL_ERR, "%s: %s: %s\n", info.short_name,
                        MSGTR_VO_GenericError, strerror(errno) );
                mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n", info.short_name,
                        buf, MSGTR_VO_CantCreateDirectory);
                exit_player(MSGTR_Exit_error);
        } /* end switch */
    } else if ( verbose ) {  
        mp_msg(MSGT_VO, MSGL_INFO, "%s: %s - %s\n", info.short_name,
                buf, MSGTR_VO_DirectoryCreateSuccess);
    } /* end if */
}

/* ------------------------------------------------------------------------- */

static int config(uint32_t width, uint32_t height, uint32_t d_width,
                       uint32_t d_height, uint32_t flags, char *title,
                       uint32_t format)
{
    char buf[BUFLENGTH];

    /* Create outdir. */
    
    snprintf(buf, BUFLENGTH, "%s", jpeg_outdir);
 
    jpeg_mkdir(buf, 1); /* This function only returns if creation was
                           successful. If not, the player will exit. */

    image_height = height;
    image_width = width;
    /* Save for JFIF-Header PAR */
    image_d_width = d_width;
    image_d_height = d_height;
    
    return 0;
}

/* ------------------------------------------------------------------------- */

static uint32_t jpeg_write(uint8_t * name, uint8_t * buffer)
{
    FILE *outfile;
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;

    if ( !buffer ) return 1; 
    if ( (outfile = fopen(name, "wb") ) == NULL ) {
        mp_msg(MSGT_VO, MSGL_ERR, "\n%s: %s\n", info.short_name,
                MSGTR_VO_CantCreateFile);
        mp_msg(MSGT_VO, MSGL_ERR, "%s: %s: %s\n",
                info.short_name, MSGTR_VO_GenericError,
                strerror(errno) );
        exit_player(MSGTR_Exit_error);
    }
 
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);
    
    cinfo.image_width = image_width;
    cinfo.image_height = image_height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    /* Important: Header info must be set AFTER jpeg_set_defaults() */
    cinfo.write_JFIF_header = TRUE;
    cinfo.JFIF_major_version = 1;
    cinfo.JFIF_minor_version = 2;
    cinfo.density_unit = 1; /* 0=unknown, 1=dpi, 2=dpcm */
    /* Image DPI is determined by Y_density, so we leave that at
       jpeg_dpi if possible and crunch X_density instead (PAR > 1) */
    cinfo.X_density = jpeg_dpi*image_width/image_d_width;
    cinfo.Y_density = jpeg_dpi*image_height/image_d_height;
    cinfo.write_Adobe_marker = TRUE;

    jpeg_set_quality(&cinfo,jpeg_quality, jpeg_baseline);
    cinfo.optimize_coding = jpeg_optimize;
    cinfo.smoothing_factor = jpeg_smooth;

    if ( jpeg_progressive_mode ) {
        jpeg_simple_progression(&cinfo);
    }
    
    jpeg_start_compress(&cinfo, TRUE);
    
    row_stride = image_width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &buffer[cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer,1);
    }

    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    
    return 0;
}

/* ------------------------------------------------------------------------- */

static int draw_frame(uint8_t *src[])
{
    static int framecounter = 0, subdircounter = 0;
    char buf[BUFLENGTH];
    static char subdirname[BUFLENGTH] = "";

    /* Start writing to new subdirectory after a certain amount of frames */
    if ( framecounter == jpeg_maxfiles ) {
        framecounter = 0;
    }

    /* If framecounter is zero (or reset to zero), increment subdirectory
     * number and create the subdirectory.
     * If jpeg_subdirs is not set, do nothing and resort to old behaviour. */
    if ( !framecounter && jpeg_subdirs ) {
        subdircounter++;
        snprintf(subdirname, BUFLENGTH, "%s%08d", jpeg_subdirs, subdircounter);
        snprintf(buf, BUFLENGTH, "%s/%s", jpeg_outdir, subdirname);
        jpeg_mkdir(buf, 0); /* This function only returns if creation was
                               successful. If not, the player will exit. */
    }
    
    framenum++;

    /* snprintf the full pathname of the outputfile */
    snprintf(buf, BUFLENGTH, "%s/%s/%08d.jpg", jpeg_outdir, subdirname,
                                                                    framenum);
    
    framecounter++;
    
    return jpeg_write(buf, src[0]);
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

static int draw_slice(uint8_t *src[], int stride[], int w, int h,
                           int x, int y)
{
    return 0;
}

/* ------------------------------------------------------------------------- */

static int query_format(uint32_t format)
{
    if (format == IMGFMT_RGB24) {
        return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW;
    }
    
    return 0;
}

/* ------------------------------------------------------------------------- */

static void uninit(void)
{
    if (jpeg_subdirs) {
        free(jpeg_subdirs);
        jpeg_subdirs = NULL;
    }
    if (jpeg_outdir) {
        free(jpeg_outdir);
        jpeg_outdir = NULL;
    }
}

/* ------------------------------------------------------------------------- */

static void check_events(void)
{
}

/* ------------------------------------------------------------------------- */

/** \brief Validation function for values [0-100]
 */

static int int_zero_hundred(int *val)
{
    if ( (*val >=0) && (*val<=100) )
        return 1;
    return 0;
}

static int preinit(const char *arg)
{
    opt_t subopts[] = {
        {"progressive", OPT_ARG_BOOL,   &jpeg_progressive_mode, NULL, 0},
        {"baseline",    OPT_ARG_BOOL,   &jpeg_baseline,         NULL, 0},
        {"optimize",    OPT_ARG_INT,    &jpeg_optimize,
                                            (opt_test_f)int_zero_hundred, 0},
        {"smooth",      OPT_ARG_INT,    &jpeg_smooth,
                                            (opt_test_f)int_zero_hundred, 0},
        {"quality",     OPT_ARG_INT,    &jpeg_quality,
                                            (opt_test_f)int_zero_hundred, 0},
        {"dpi",         OPT_ARG_INT,    &jpeg_dpi,              NULL, 0},
        {"outdir",      OPT_ARG_MSTRZ,  &jpeg_outdir,           NULL, 0},
        {"subdirs",     OPT_ARG_MSTRZ,  &jpeg_subdirs,          NULL, 0},
        {"maxfiles",    OPT_ARG_INT,    &jpeg_maxfiles, (opt_test_f)int_pos, 0},
        {NULL, 0, NULL, NULL, 0}
    };
    const char *info_message = NULL;

    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                                            MSGTR_VO_ParsingSuboptions);

    jpeg_progressive_mode = 0;
    jpeg_baseline = 1;
    jpeg_optimize = 100;
    jpeg_smooth = 0;
    jpeg_quality = 75;
    jpeg_maxfiles = 1000;
    jpeg_outdir = strdup(".");
    jpeg_subdirs = NULL;

    if (subopt_parse(arg, subopts) != 0) {
        return -1;
    }

    if (jpeg_progressive_mode) info_message = MSGTR_VO_JPEG_ProgressiveJPEG;
    else info_message = MSGTR_VO_JPEG_NoProgressiveJPEG;
    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name, info_message);

    if (jpeg_baseline) info_message = MSGTR_VO_JPEG_BaselineJPEG;
    else info_message = MSGTR_VO_JPEG_NoBaselineJPEG;
    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name, info_message);

    mp_msg(MSGT_VO, MSGL_V, "%s: optimize --> %d\n", info.short_name,
                                                                jpeg_optimize);
    mp_msg(MSGT_VO, MSGL_V, "%s: smooth --> %d\n", info.short_name,
                                                                jpeg_smooth);
    mp_msg(MSGT_VO, MSGL_V, "%s: quality --> %d\n", info.short_name,
                                                                jpeg_quality);
    mp_msg(MSGT_VO, MSGL_V, "%s: dpi --> %d\n", info.short_name,
                                                                jpeg_dpi);
    mp_msg(MSGT_VO, MSGL_V, "%s: outdir --> %s\n", info.short_name,
                                                                jpeg_outdir);
    if (jpeg_subdirs) {
        mp_msg(MSGT_VO, MSGL_V, "%s: subdirs --> %s\n", info.short_name,
                                                                jpeg_subdirs);
        mp_msg(MSGT_VO, MSGL_V, "%s: maxfiles --> %d\n", info.short_name,
                                                                jpeg_maxfiles);
    }

    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                                            MSGTR_VO_SuboptionsParsedOK);
    return 0;
}

/* ------------------------------------------------------------------------- */

static int control(uint32_t request, void *data, ...)
{
    switch (request) {
        case VOCTRL_QUERY_FORMAT:
            return query_format(*((uint32_t*)data));
    }
    return VO_NOTIMPL;
}

/* ------------------------------------------------------------------------- */

#undef BUFLENGTH

/* ------------------------------------------------------------------------- */

