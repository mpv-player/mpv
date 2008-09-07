/*
 * PPM/PGM/PGMYUV video output driver
 *
 * copyright (C) 2004, 2005 Ivo van Poorten
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
#include <sys/stat.h>

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

#define PNM_ASCII_MODE 0
#define PNM_RAW_MODE 1
#define PNM_TYPE_PPM 0
#define PNM_TYPE_PGM 1
#define PNM_TYPE_PGMYUV 2

#define PNM_LINE_OF_ASCII "%03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d %03d\n"
#define PNM_LINE15(a,b) a[b], a[b+1], a[b+2], a[b+3], a[b+4], a[b+5], a[b+6], \
                        a[b+7], a[b+8], a[b+9], a[b+10], a[b+11], a[b+12], \
                        a[b+13], a[b+14]

/* ------------------------------------------------------------------------- */

/* Info */

static const vo_info_t info=
{
	"PPM/PGM/PGMYUV file",
	"pnm",
	"Ivo van Poorten (ivop@euronet.nl)",
	""
};

const LIBVO_EXTERN (pnm)

/* ------------------------------------------------------------------------- */

/* Global Variables */

int pnm_type = PNM_TYPE_PPM;
int pnm_mode = PNM_RAW_MODE;

char *pnm_outdir = NULL;
char *pnm_subdirs = NULL;
int pnm_maxfiles = 1000;
char *pnm_file_extension = NULL;

/* ------------------------------------------------------------------------- */

/** \brief An error occured while writing to a file.
 *
 * The program failed to write data to a file.
 * It displays a message and exits the player.
 *
 * \return nothing It does not return.
 */

static void pnm_write_error(void) {
    mp_msg(MSGT_VO, MSGL_ERR, MSGTR_ErrorWritingFile, info.short_name);
    exit_player(MSGTR_Exit_error);
}

/* ------------------------------------------------------------------------- */

/** \brief Pre-initialisation.
 *
 * This function is called before initialising the video output driver. It
 * parses all suboptions and sets variables accordingly. If an error occurs
 * (like an option being out of range, not having any value or an unknown
 * option is stumbled upon) the player will exit.
 *
 * \param arg   A string containing all the suboptions passed to the video
 *              output driver.
 *
 * \return 0    All went well.
 */

static int preinit(const char *arg)
{
    int ppm_type = 0, pgm_type = 0, pgmyuv_type = 0,
        raw_mode = 0, ascii_mode = 0;
    opt_t subopts[] = {
        {"ppm",         OPT_ARG_BOOL,   &ppm_type,      NULL, 0},
        {"pgm",         OPT_ARG_BOOL,   &pgm_type,      NULL, 0},
        {"pgmyuv",      OPT_ARG_BOOL,   &pgmyuv_type,   NULL, 0},
        {"raw",         OPT_ARG_BOOL,   &raw_mode,      NULL, 0},
        {"ascii",       OPT_ARG_BOOL,   &ascii_mode,    NULL, 0},
        {"outdir",      OPT_ARG_MSTRZ,  &pnm_outdir,    NULL, 0},
        {"subdirs",     OPT_ARG_MSTRZ,  &pnm_subdirs,   NULL, 0},
        {"maxfiles",    OPT_ARG_INT,    &pnm_maxfiles,  (opt_test_f)int_pos, 0},
        {NULL, 0, NULL, NULL, 0}
    };
    const char *info_message = NULL;

    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                                            MSGTR_VO_ParsingSuboptions);

    pnm_maxfiles = 1000;
    pnm_outdir = strdup(".");
    pnm_subdirs = NULL;

    if (subopt_parse(arg, subopts) != 0) {
        return -1;
    }

    pnm_type     = PNM_TYPE_PPM;
    pnm_mode     = PNM_RAW_MODE;

    if (pgmyuv_type) pnm_type     = PNM_TYPE_PGMYUV;
    if (pgm_type)    pnm_type     = PNM_TYPE_PGM;
    if (ppm_type)    pnm_type     = PNM_TYPE_PPM;
    if (ascii_mode)  pnm_mode     = PNM_ASCII_MODE;
    if (raw_mode)    pnm_mode     = PNM_RAW_MODE;

    switch (pnm_mode) {
        case PNM_ASCII_MODE:
            info_message = MSGTR_VO_PNM_ASCIIMode;
            break;
        case PNM_RAW_MODE:
            info_message = MSGTR_VO_PNM_RawMode;
            break;
    }
    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name, info_message);

    switch (pnm_type) {
        case PNM_TYPE_PPM:
            info_message = MSGTR_VO_PNM_PPMType;
            break;
        case PNM_TYPE_PGM:
            info_message = MSGTR_VO_PNM_PGMType;
            break;
        case PNM_TYPE_PGMYUV:
            info_message = MSGTR_VO_PNM_PGMYUVType;
            break;
    }
    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name, info_message);

    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                                            MSGTR_VO_SuboptionsParsedOK);
    return 0;
}

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

static void pnm_mkdir(char *buf, int verbose) { 
    struct stat stat_p;

/* Silly MING32 bug workaround */
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
                
                if (strcmp(buf, ".") != 0) {
                mp_msg(MSGT_VO, MSGL_INFO, "%s: %s - %s\n", info.short_name,
                        buf, MSGTR_VO_DirExistsAndIsWritable);
                }
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

/** \brief Configure the video output driver.
 *
 * This functions configures the video output driver. It determines the
 * width and height of the image(s) and creates the output directory.
 *
 *  \return 0             All went well.
 */

static int config(uint32_t width, uint32_t height, uint32_t d_width,
                       uint32_t d_height, uint32_t flags, char *title,
                       uint32_t format)
{
    char buf[BUFLENGTH];

    if (vo_config_count > 0 ) { /* Already configured */
        return 0;
    }

    /* Create outdir. */
    
    snprintf(buf, BUFLENGTH, "%s", pnm_outdir);
    pnm_mkdir(buf, 1); /* This function only returns if creation was
                           successful. If not, the player will exit. */

    if (pnm_type == PNM_TYPE_PPM) {
        pnm_file_extension = strdup("ppm");
    } else if (pnm_type == PNM_TYPE_PGM) {
        pnm_file_extension = strdup("pgm");
    } else if (pnm_type == PNM_TYPE_PGMYUV) {
        pnm_file_extension = strdup("pgmyuv");
    }

    return 0;
}

/* ------------------------------------------------------------------------- */

/** \brief Write PNM file to output file
 *
 * This function writes PPM, PGM or PGMYUV data to an output file, depending
 * on which type was selected on the commandline. pnm_type and pnm_mode are
 * global variables. Depending on which mode was selected, it will write
 * a RAW or an ASCII file.
 *
 * \param outfile       Filedescriptor of output file.
 * \param mpi           The image to write.
 *
 * \return none         The player will exit if anything goes wrong.
 */

static void pnm_write_pnm(FILE *outfile, mp_image_t *mpi)
{
    uint32_t w = mpi->w;
    uint32_t h = mpi->h;
    uint8_t *rgbimage = mpi->planes[0];
    uint8_t *planeY = mpi->planes[0];
    uint8_t *planeU = mpi->planes[1];
    uint8_t *planeV = mpi->planes[2];
    uint8_t *curline;
    uint32_t strideY = mpi->stride[0];
    uint32_t strideU = mpi->stride[1];
    uint32_t strideV = mpi->stride[2];

    unsigned int i, j;

    if (pnm_mode == PNM_RAW_MODE) {

        if (pnm_type == PNM_TYPE_PPM) {
            if ( fprintf(outfile, "P6\n%d %d\n255\n", w, h) < 0 )
                pnm_write_error();
            if ( fwrite(rgbimage, w * 3, h, outfile) < h ) pnm_write_error();
        } else if (pnm_type == PNM_TYPE_PGM) {
            if ( fprintf(outfile, "P5\n%d %d\n255\n", w, h) < 0 )
                pnm_write_error();
            for (i=0; i<h; i++) {
                if ( fwrite(planeY + i * strideY, w, 1, outfile) < 1 )
                    pnm_write_error();
            }
        } else if (pnm_type == PNM_TYPE_PGMYUV) {
            if ( fprintf(outfile, "P5\n%d %d\n255\n", w, h*3/2) < 0 )
                pnm_write_error();
            for (i=0; i<h; i++) {
                if ( fwrite(planeY + i * strideY, w, 1, outfile) < 1 )
                    pnm_write_error();
            }
            w = w / 2;
            h = h / 2;
            for (i=0; i<h; i++) {
                if ( fwrite(planeU + i * strideU, w, 1, outfile) < 1 )
                    pnm_write_error();
                if ( fwrite(planeV + i * strideV, w, 1, outfile) < 1 )
                    pnm_write_error();
            }
        } /* end if pnm_type */

    } else if (pnm_mode == PNM_ASCII_MODE) {

        if (pnm_type == PNM_TYPE_PPM) {
            if ( fprintf(outfile, "P3\n%d %d\n255\n", w, h) < 0 )
                pnm_write_error();
            for (i=0; i <= w * h * 3 - 16 ; i += 15) {
                if ( fprintf(outfile, PNM_LINE_OF_ASCII,
                    PNM_LINE15(rgbimage,i) ) < 0 )  pnm_write_error();
            }
            while (i < (w * h * 3) ) {
                if ( fprintf(outfile, "%03d ", rgbimage[i]) < 0 )
                    pnm_write_error();
                i++;
            }
            if ( fputc('\n', outfile) < 0 ) pnm_write_error();
        } else if ( (pnm_type == PNM_TYPE_PGM) ||
                                            (pnm_type == PNM_TYPE_PGMYUV) ) {

            /* different header for pgm and pgmyuv. pgmyuv is 'higher' */
            if (pnm_type == PNM_TYPE_PGM) {
                if ( fprintf(outfile, "P2\n%d %d\n255\n", w, h) < 0 )
                    pnm_write_error();
            } else { /* PNM_TYPE_PGMYUV */
                if ( fprintf(outfile, "P2\n%d %d\n255\n", w, h*3/2) < 0 )
                    pnm_write_error();
            }

            /* output Y plane for both PGM and PGMYUV */
            for (j=0; j<h; j++) {
                curline = planeY + strideY * j;
                for (i=0; i <= w - 16; i+=15) {
                    if ( fprintf(outfile, PNM_LINE_OF_ASCII,
                        PNM_LINE15(curline,i) ) < 0 ) pnm_write_error();
                }
                while (i < w ) {
                    if ( fprintf(outfile, "%03d ", curline[i]) < 0 )
                        pnm_write_error();
                    i++;
                }
                if ( fputc('\n', outfile) < 0 ) pnm_write_error();
            }

            /* also output U and V planes fpr PGMYUV */
            if (pnm_type == PNM_TYPE_PGMYUV) {
                w = w / 2;
                h = h / 2;
                for (j=0; j<h; j++) {
                    curline = planeU + strideU * j;
                    for (i=0; i<= w-16; i+=15) {
                        if ( fprintf(outfile, PNM_LINE_OF_ASCII,
                            PNM_LINE15(curline,i) ) < 0 ) pnm_write_error();
                    }
                    while (i < w ) {
                        if ( fprintf(outfile, "%03d ", curline[i]) < 0 )
                            pnm_write_error();
                        i++;
                    }
                    if ( fputc('\n', outfile) < 0 ) pnm_write_error();

                    curline = planeV + strideV * j;
                    for (i=0; i<= w-16; i+=15) {
                        if ( fprintf(outfile, PNM_LINE_OF_ASCII,
                            PNM_LINE15(curline,i) ) < 0 ) pnm_write_error();
                    }
                    while (i < w ) {
                        if ( fprintf(outfile, "%03d ", curline[i]) < 0 )
                            pnm_write_error();
                        i++;
                    }
                    if ( fputc('\n', outfile) < 0 ) pnm_write_error();
                }
            }

        } /* end if pnm_type */
    } /* end if pnm_mode */
}

/* ------------------------------------------------------------------------- */

/** \brief Write a PNM image.
 *
 * This function gets called first if a PNM image has to be written to disk.
 * It contains the subdirectory framework and it calls pnm_write_pnm() to
 * actually write the image to disk.
 *
 * \param mpi       The image to write.
 *
 * \return none     The player will exit if anything goes wrong.
 */

static void pnm_write_image(mp_image_t *mpi)
{
    static int framenum = 0, framecounter = 0, subdircounter = 0;
    char buf[BUFLENGTH];
    static char subdirname[BUFLENGTH] = "";
    FILE *outfile;

    if (!mpi) {
        mp_msg(MSGT_VO, MSGL_ERR, "%s: No image data suplied to video output driver\n", info.short_name );
        exit_player(MSGTR_Exit_error);
    }
        
    /* Start writing to new subdirectory after a certain amount of frames */
    if ( framecounter == pnm_maxfiles ) {
        framecounter = 0;
    }

    /* If framecounter is zero (or reset to zero), increment subdirectory
     * number and create the subdirectory.
     * If pnm_subdirs is not set, do nothing. */
    if ( !framecounter && pnm_subdirs ) {
        subdircounter++;
        snprintf(subdirname, BUFLENGTH, "%s%08d", pnm_subdirs, subdircounter);
        snprintf(buf, BUFLENGTH, "%s/%s", pnm_outdir, subdirname);
        pnm_mkdir(buf, 0); /* This function only returns if creation was
                               successful. If not, the player will exit. */
    }
    
    framenum++;
    framecounter++;

    /* snprintf the full pathname of the outputfile */
    snprintf(buf, BUFLENGTH, "%s/%s/%08d.%s", pnm_outdir, subdirname,
                                            framenum, pnm_file_extension);
    
    if ( (outfile = fopen(buf, "wb") ) == NULL ) {
        mp_msg(MSGT_VO, MSGL_ERR, "\n%s: %s\n", info.short_name,
                MSGTR_VO_CantCreateFile);
        mp_msg(MSGT_VO, MSGL_ERR, "%s: %s: %s\n",
                info.short_name, MSGTR_VO_GenericError,
                strerror(errno) );
        exit_player(MSGTR_Exit_error);
    }
    
    pnm_write_pnm(outfile, mpi);

    fclose(outfile);
}

/* ------------------------------------------------------------------------- */

static uint32_t draw_image(mp_image_t *mpi)
{
    if (mpi->flags & MP_IMGFLAG_PLANAR) { /* Planar */
        if (mpi->flags & MP_IMGFLAG_YUV) { /* Planar YUV */
            pnm_write_image(mpi);
            return VO_TRUE;
        } else { /* Planar RGB */
            return VO_FALSE;
        }
    } else { /* Packed */
        if (mpi->flags & MP_IMGFLAG_YUV) { /* Packed YUV */
            return VO_FALSE;
        } else { /* Packed RGB */
            pnm_write_image(mpi);
            return VO_TRUE;
        }
    }

    return VO_FALSE;
}

/* ------------------------------------------------------------------------- */

static int draw_frame(uint8_t *src[])
{
    mp_msg(MSGT_VO, MSGL_V, "%s: draw_frame() is called!\n", info.short_name);
    return -1;
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
    /* Ensure that for PPM we get Packed RGB and for PGM(YUV) we get
     * Planar YUV */
    if (pnm_type == PNM_TYPE_PPM) {
        if (format == IMGFMT_RGB24) {
            return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW;
        }
    } else if ( (pnm_type == PNM_TYPE_PGM) || (pnm_type == PNM_TYPE_PGMYUV) ) {
        if (format == IMGFMT_YV12) {
            return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW;
        }
    }

    return 0;
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
    if (pnm_subdirs) {
        free(pnm_subdirs);
        pnm_subdirs = NULL;
    }
    if (pnm_outdir) {
        free(pnm_outdir);
        pnm_outdir = NULL;
    }
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
#undef PNM_RAW_MODE
#undef PNM_ASCII_MODE
#undef PNM_TYPE_PPM
#undef PNM_TYPE_PGM
#undef PNM_TYPE_PGMYUV

/* ------------------------------------------------------------------------- */

