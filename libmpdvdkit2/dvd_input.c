/*
 * Copyright (C) 2002 Samuel Hocevar <sam@zoy.org>,
 *                    Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "dvd_reader.h"
#include "dvd_input.h"

#include "dvdcss.h"

dvdcss_handle (*DVDcss_open)  (const char *);
int           (*DVDcss_close) (dvdcss_handle);
int           (*DVDcss_seek)  (dvdcss_handle, int, int);
int           (*DVDcss_title) (dvdcss_handle, int); 
int           (*DVDcss_read)  (dvdcss_handle, void *, int, int);
char *        (*DVDcss_error) (dvdcss_handle);


/* The DVDinput handle, add stuff here for new input methods. */
struct dvd_input_s {
  /* libdvdcss handle */
  dvdcss_handle dvdcss;
  
  /* dummy file input */
  int fd;
};


/**
 * initialize and open a DVD device or file.
 */
static dvd_input_t css_open(const char *target)
{
  dvd_input_t dev;
  
  /* Allocate the handle structure */
  dev = (dvd_input_t) malloc(sizeof(dvd_input_t));
  if(dev == NULL) {
    fprintf(stderr, "libdvdread: Could not allocate memory.\n");
    return NULL;
  }
  
  /* Really open it with libdvdcss */
  dev->dvdcss = DVDcss_open(target);
  if(dev->dvdcss == 0) {
    fprintf(stderr, "libdvdread: Could not open device with libdvdcss.\n");
    free(dev);
    return NULL;
  }
  
  return dev;
}

/**
 * return the last error message
 */
static char *css_error(dvd_input_t dev)
{
  return DVDcss_error(dev->dvdcss);
}

/**
 * seek into the device.
 */
static int css_seek(dvd_input_t dev, int blocks, int flags)
{
  return DVDcss_seek(dev->dvdcss, blocks, flags);
}

/**
 * set the block for the begining of a new title (key).
 */
static int css_title(dvd_input_t dev, int block)
{
  return DVDcss_title(dev->dvdcss, block);
}

/**
 * read data from the device.
 */
static int css_read(dvd_input_t dev, void *buffer, int blocks, int flags)
{
  return DVDcss_read(dev->dvdcss, buffer, blocks, flags);
}

/**
 * close the DVD device and clean up the library.
 */
static int css_close(dvd_input_t dev)
{
  int ret;

  ret = DVDcss_close(dev->dvdcss);

  if(ret < 0)
    return ret;

  free(dev);

  return 0;
}



/**
 * Setup read functions with either libdvdcss or minimal DVD access.
 */
int DVDInputSetup(void)
{
    DVDcss_open = dvdcss_open;
    DVDcss_close = dvdcss_close;
    DVDcss_title = dvdcss_title;
    DVDcss_seek = dvdcss_seek;
    DVDcss_read = dvdcss_read;
    DVDcss_error = dvdcss_error;
    
    /*
    char *psz_method = getenv( "DVDCSS_METHOD" );
    char *psz_verbose = getenv( "DVDCSS_VERBOSE" );
    fprintf(stderr, "DVDCSS_METHOD %s\n", psz_method);
    fprintf(stderr, "DVDCSS_VERBOSE %s\n", psz_verbose);
    */
//    fprintf(stderr, "libdvdread: Using libdvdcss version %s for DVD access\n",
//	    *dvdcss_version);
    
    /* libdvdcss wraper functions */
    DVDinput_open  = css_open;
    DVDinput_close = css_close;
    DVDinput_seek  = css_seek;
    DVDinput_title = css_title;
    DVDinput_read  = css_read;
    DVDinput_error = css_error;
    return 1;
    
}
