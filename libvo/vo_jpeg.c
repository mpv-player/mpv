/* ------------------------------------------------------------------------- */

/* 
 * vo_jpeg.c, JPEG Renderer for MPlayer
 *
 * 
 * Changelog
 * 
 * Original version: Copyright 2002 by Pontscho (pontscho@makacs.poliod.hu)
 * 2003-04-25   Spring cleanup -- Alex
 * 2004-08-04   Added multiple subdirectory support -- Ivo (ivop@euronet.nl)
 * 2004-09-01   Cosmetics update -- Ivo
 * 2004-09-05   Added suboptions parser -- Ivo
 *
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
#include <math.h>               /* for log10() */

/* ------------------------------------------------------------------------- */

/* Local Includes */

#include "config.h"
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

static vo_info_t info=
{
	"JPEG file",
	"jpeg",
	"Zoltan Ponekker (pontscho@makacs.poliod.hu)",
	""
};

LIBVO_EXTERN (jpeg)

/* ------------------------------------------------------------------------- */

/* Global Variables */

static int image_width;
static int image_height;

int jpeg_baseline = 1;
int jpeg_progressive_mode = 0;
int jpeg_optimize = 100;
int jpeg_smooth = 0;
int jpeg_quality = 75;
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

void jpeg_mkdir(char *buf, int verbose) { 
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

static uint32_t config(uint32_t width, uint32_t height, uint32_t d_width,
                       uint32_t d_height, uint32_t fullscreen, char *title,
                       uint32_t format)
{
    char buf[BUFLENGTH];

    /* Create outdir. */
    
    snprintf(buf, BUFLENGTH, "%s", jpeg_outdir);
 
    jpeg_mkdir(buf, 1); /* This function only returns if creation was
                           successful. If not, the player will exit. */

    image_height = height;
    image_width = width;
    
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

static uint32_t draw_frame(uint8_t *src[])
{
    static uint32_t framecounter = 0, subdircounter = 0;
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

static uint32_t draw_slice(uint8_t *src[], int stride[], int w, int h,
                           int x, int y)
{
    return 0;
}

/* ------------------------------------------------------------------------- */

static uint32_t query_format(uint32_t format)
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

/** \brief Memory allocation failed.
 *
 * This function can be called if memory allocation failed. It prints a
 * message and exits the player.
 *
 * \return none     It never returns.
 */

void jpeg_malloc_failed(void) {
    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s\n", info.short_name,
            MSGTR_MemAllocFailed);
    exit_player(MSGTR_Exit_error);
}

/* ------------------------------------------------------------------------- */

static uint32_t preinit(const char *arg)
{
    char *buf;      /* buf is used to store parsed string values */
    int value;      /* storage for parsed integer values */

    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                                            MSGTR_VO_ParsingSuboptions);
    
    if (arg) {

        while (*arg != '\0') {
            if (!strncmp(arg, ":", 1)) {
                arg++;
                continue;   /* multiple ':' is not really an error */
            } if (!strncmp(arg, "progressive", 11)) {
                arg += 11;
                jpeg_progressive_mode = 1;
                mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                        MSGTR_VO_JPEG_ProgressiveJPEG);
            } else if (!strncmp(arg, "noprogressive", 13)) {
                arg += 13;
                jpeg_progressive_mode = 0;
                mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                        MSGTR_VO_JPEG_NoProgressiveJPEG);
            } else if (!strncmp(arg, "baseline", 8)) {
                arg += 8;
                jpeg_baseline = 1;
                mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                        MSGTR_VO_JPEG_BaselineJPEG);
            } else if (!strncmp(arg, "nobaseline", 10)) {
                arg += 10;
                jpeg_baseline = 0;
                mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                        MSGTR_VO_JPEG_NoBaselineJPEG);
            } else if (!strncmp(arg, "optimize=", 9)) {
                arg += 9;
                if (sscanf(arg, "%d", &value) == 1) {
                    if ( (value < 0 ) || (value > 100) ) {
                        mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s %s.\n",
                                info.short_name, "optimize",
                                MSGTR_VO_ValueOutOfRange, "[0-100]");
                        exit_player(MSGTR_Exit_error);
                    } else {
                        jpeg_optimize = value;
                        mp_msg(MSGT_VO, MSGL_INFO, "%s: %s --> %d\n",
                                info.short_name, "optimize", value);
                    }
                } else {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n",
                            info.short_name, "optimize",
                            MSGTR_VO_NoValueSpecified);
                    exit_player(MSGTR_Exit_error);
                }
                /* only here if value is set and sane */
                if (value) {
                    arg += (int)log10(value) + 1;
                } else {
                    arg++;  /* log10(0) fails */
                }
            } else if (!strncmp(arg, "smooth=", 7)) {
                arg += 7;
                if (sscanf(arg, "%d", &value) == 1 ) {
                    if ( (value < 0) || (value > 100) ) {
                        mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s %s.\n",
                                info.short_name, "smooth",
                                MSGTR_VO_ValueOutOfRange, "[0-100]");
                        exit_player(MSGTR_Exit_error);
                    } else {
                        jpeg_smooth = value;
                        mp_msg(MSGT_VO, MSGL_INFO, "%s: %s --> %d\n",
                                info.short_name, "smooth", value);
                    }
                } else {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n",
                            info.short_name, "smooth",
                            MSGTR_VO_NoValueSpecified);
                    exit_player(MSGTR_Exit_error);
                }
                /* only here if value is set and sane */
                if (value) {
                    arg += (int)log10(value) + 1;
                } else {
                    arg++;  /* log10(0) fails */
                }
            } else if (!strncmp(arg, "quality=", 8)) {
                arg += 8;
                if (sscanf(arg, "%d", &value) == 1) {
                    if ( (value < 0) || (value > 100) ) {
                        mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s %s.\n",
                                info.short_name, "quality",
                                MSGTR_VO_ValueOutOfRange, "[0-100]");
                        exit_player(MSGTR_Exit_error);
                    } else {
                        jpeg_quality = value;
                        mp_msg(MSGT_VO, MSGL_INFO, "%s: %s --> %d\n",
                                info.short_name, "quality", value);
                    }
                } else {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n",
                            info.short_name, "quality",
                            MSGTR_VO_NoValueSpecified);
                    exit_player(MSGTR_Exit_error);
                }
                /* only here if value is set and sane */
                if (value) {
                    arg += (int)log10(value) + 1;
                } else {
                    arg++;  /* log10(0) fails */
                }
            } else if (!strncmp(arg, "outdir=", 7)) {
                arg += 7;
                buf = malloc(strlen(arg)+1); /* maximum length possible */
                if (!buf) jpeg_malloc_failed(); /* print msg and exit */
                if (sscanf(arg, "%[^:]", buf) == 1) {
                    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s --> %s\n",
                            info.short_name, "outdir", buf);
                    arg += strlen(buf);
                    jpeg_outdir = strdup(buf);
                    if (!jpeg_outdir) jpeg_malloc_failed();
                    free(buf);
                } else {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n",
                            info.short_name, "outdir",
                            MSGTR_VO_NoValueSpecified);
                    exit_player(MSGTR_Exit_error);
                }
            } else if (!strncmp(arg, "subdirs=", 8)) {
                arg += 8;
                buf = malloc(strlen(arg)+1); /* maximum length possible */
                if (!buf) jpeg_malloc_failed();
                if (sscanf(arg, "%[^:]", buf) == 1) {
                    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s --> %s\n",
                            info.short_name, "subdirs", buf);
                    arg += strlen(buf);
                    jpeg_subdirs = strdup(buf);
                    if (!jpeg_subdirs) jpeg_malloc_failed();
                    free(buf);
                } else {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n",
                            info.short_name, "subdirs",
                            MSGTR_VO_NoValueSpecified);
                    exit_player(MSGTR_Exit_error);
                }
            } else if (!strncmp(arg, "maxfiles=", 9)) {
                arg += 9;
                if (sscanf(arg, "%d", &value) == 1) {
                    if (value < 1) {
                        mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s %s.\n",
                                info.short_name, "maxfiles",
                                MSGTR_VO_ValueOutOfRange, ">=1");
                        exit_player(MSGTR_Exit_error);
                    } else {
                        jpeg_maxfiles = value;
                        mp_msg(MSGT_VO, MSGL_INFO, "%s: %s --> %d\n",
                                info.short_name, "maxfiles", value);
                    }
                } else {
                    mp_msg(MSGT_VO, MSGL_ERR, "%s: %s - %s\n",
                            info.short_name, "maxfiles",
                            MSGTR_VO_NoValueSpecified);
                    exit_player(MSGTR_Exit_error);
                }
                /* only here if value is set and sane */
                if (value) {
                    arg += (int)log10(value) + 1;
                } else {
                    arg++;  /* log10(0) fails */
                }
            } else {
                mp_msg(MSGT_VO, MSGL_ERR, "%s: %s %-20s...\n", info.short_name,
                        MSGTR_VO_UnknownSuboptions, arg);
                exit_player(MSGTR_Exit_error);
            }
        } /* end while */
    } /* endif */
    
    /* If jpeg_outdir is not set by an option, resort to default of "." */
    if (!jpeg_outdir) {
        jpeg_outdir = strdup(".");
        if (!jpeg_outdir) jpeg_malloc_failed();
    }

    mp_msg(MSGT_VO, MSGL_INFO, "%s: %s\n", info.short_name,
                                            MSGTR_VO_SuboptionsParsedOK);
    return 0;
}

/* ------------------------------------------------------------------------- */

static uint32_t control(uint32_t request, void *data, ...)
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

