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
#include <dlfcn.h>

#include "dvd_reader.h"
#include "dvd_input.h"

/* For libdvdcss */
typedef struct dvdcss_s *dvdcss_handle;

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
 * initialize and open a DVD device or file.
 */
static dvd_input_t file_open(const char *target)
{
  dvd_input_t dev;
  
  /* Allocate the library structure */
  dev = (dvd_input_t) malloc(sizeof(dvd_input_t));
  if(dev == NULL) {
    fprintf(stderr, "libdvdread: Could not allocate memory.\n");
    return NULL;
  }
  
  /* Open the device */
  dev->fd = open(target, O_RDONLY);
  if(dev->fd < 0) {
    perror("libdvdread: Could not open input");
    free(dev);
    return NULL;
  }
  
  return dev;
}

/**
 * return the last error message
 */
static char *file_error(dvd_input_t dev)
{
  /* use strerror(errno)? */
  return "unknown error";
}

/**
 * seek into the device.
 */
static int file_seek(dvd_input_t dev, int blocks, int flags)
{
  off_t pos;

  pos = lseek(dev->fd, (off_t)blocks * (off_t)DVD_VIDEO_LB_LEN, SEEK_SET);
  if(pos < 0) {
      return pos;
  }
  /* assert pos % DVD_VIDEO_LB_LEN == 0 */
  return (int) (pos / DVD_VIDEO_LB_LEN);
}

/**
 * set the block for the begining of a new title (key).
 */
static int file_title(dvd_input_t dev, int block)
{
  return -1;
}

/**
 * read data from the device.
 */
static int file_read(dvd_input_t dev, void *buffer, int blocks, int flags)
{
  size_t len;
  ssize_t ret;
  
  len = (size_t)blocks * DVD_VIDEO_LB_LEN;
  
  while(len > 0) {
    
    ret = read(dev->fd, buffer, len);
    
    if(ret < 0) {
      /* One of the reads failed, too bad.  We won't even bother
       * returning the reads that went ok, and as in the posix spec
       * the file postition is left unspecified after a failure. */
      return ret;
    }
    
    if(ret == 0) {
      /* Nothing more to read.  Return the whole blocks, if any, that we got.
	 and adjust the file possition back to the previous block boundary. */
      size_t bytes = (size_t)blocks * DVD_VIDEO_LB_LEN - len;
      off_t over_read = -(bytes % DVD_VIDEO_LB_LEN);
      /*off_t pos =*/ lseek(dev->fd, over_read, SEEK_CUR);
      /* should have pos % 2048 == 0 */
      return (int) (bytes / DVD_VIDEO_LB_LEN);
    }
    
    len -= ret;
  }

  return blocks;
}

/**
 * close the DVD device and clean up.
 */
static int file_close(dvd_input_t dev)
{
  int ret;

  ret = close(dev->fd);

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
  void *dvdcss_library = NULL;
  char **dvdcss_version = NULL;
  
  dvdcss_library = dlopen("libdvdcss.so.2", RTLD_LAZY);
  
  if(dvdcss_library != NULL) {
#if defined(__OpenBSD__) && !defined(__ELF__)
#define U_S "_"
#else
#define U_S
#endif
    DVDcss_open = (dvdcss_handle (*)(const char*))
      dlsym(dvdcss_library, U_S "dvdcss_open");
    DVDcss_close = (int (*)(dvdcss_handle))
      dlsym(dvdcss_library, U_S "dvdcss_close");
    DVDcss_title = (int (*)(dvdcss_handle, int))
      dlsym(dvdcss_library, U_S "dvdcss_title");
    DVDcss_seek = (int (*)(dvdcss_handle, int, int))
      dlsym(dvdcss_library, U_S "dvdcss_seek");
    DVDcss_read = (int (*)(dvdcss_handle, void*, int, int))
      dlsym(dvdcss_library, U_S "dvdcss_read");
    DVDcss_error = (char* (*)(dvdcss_handle))
      dlsym(dvdcss_library, U_S "dvdcss_error");
    
    dvdcss_version = (char **)dlsym(dvdcss_library, U_S "dvdcss_interface_2");

    if(dlsym(dvdcss_library, U_S "dvdcss_crack")) {
      fprintf(stderr, 
	      "libdvdread: Old (pre-0.0.2) version of libdvdcss found.\n"
	      "libdvdread: You should get the latest version from "
	      "http://www.videolan.org/\n" );
      dlclose(dvdcss_library);
      dvdcss_library = NULL;
    } else if(!DVDcss_open  || !DVDcss_close || !DVDcss_title || !DVDcss_seek
	      || !DVDcss_read || !DVDcss_error || !dvdcss_version) {
      fprintf(stderr,  "libdvdread: Missing symbols in libdvdcss.so.2, "
	      "this shouldn't happen !\n");
      dlclose(dvdcss_library);
    }
  }
  
  if(dvdcss_library != NULL) {
    /*
    char *psz_method = getenv( "DVDCSS_METHOD" );
    char *psz_verbose = getenv( "DVDCSS_VERBOSE" );
    fprintf(stderr, "DVDCSS_METHOD %s\n", psz_method);
    fprintf(stderr, "DVDCSS_VERBOSE %s\n", psz_verbose);
    */
    fprintf(stderr, "libdvdread: Using libdvdcss version %s for DVD access\n",
	    *dvdcss_version);
    
    /* libdvdcss wraper functions */
    DVDinput_open  = css_open;
    DVDinput_close = css_close;
    DVDinput_seek  = css_seek;
    DVDinput_title = css_title;
    DVDinput_read  = css_read;
    DVDinput_error = css_error;
    return 1;
    
  } else {
    fprintf(stderr, "libdvdread: Encrypted DVD support unavailable.\n");

    /* libdvdcss replacement functions */
    DVDinput_open  = file_open;
    DVDinput_close = file_close;
    DVDinput_seek  = file_seek;
    DVDinput_title = file_title;
    DVDinput_read  = file_read;
    DVDinput_error = file_error;
    return 0;
  }
}
