/* -*- c-basic-offset: 2; indent-tabs-mode: nil -*- */
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU /* to get O_DIRECT in linux */
#include <fcntl.h>
#include <unistd.h>

#include "dvd_reader.h"
#include "dvd_input.h"

#include "dvdread_internal.h"

/* The function pointers that is the exported interface of this file. */
dvd_input_t (*dvdinput_open)  (const char *);
int         (*dvdinput_close) (dvd_input_t);
int         (*dvdinput_seek)  (dvd_input_t, int);
int         (*dvdinput_title) (dvd_input_t, int); 
/**
 *  pointer must be aligned to 2048 bytes
 *  if reading from a raw/O_DIRECT file
 */
int         (*dvdinput_read)  (dvd_input_t, void *, int, int);

char *      (*dvdinput_error) (dvd_input_t);

#ifdef HAVE_DVDCSS_DVDCSS_H
/* linking to libdvdcss */
#include <dvdcss/dvdcss.h>
#define DVDcss_open(a) dvdcss_open((char*)(a))
#define DVDcss_close   dvdcss_close
#define DVDcss_seek    dvdcss_seek
#define DVDcss_title   dvdcss_title
#define DVDcss_read    dvdcss_read
#define DVDcss_error   dvdcss_error
#else
/* dlopening libdvdcss */
#include <dlfcn.h>
typedef struct dvdcss_s *dvdcss_handle;
static dvdcss_handle (*DVDcss_open)  (const char *);
static int           (*DVDcss_close) (dvdcss_handle);
static int           (*DVDcss_seek)  (dvdcss_handle, int, int);
static int           (*DVDcss_title) (dvdcss_handle, int); 
static int           (*DVDcss_read)  (dvdcss_handle, void *, int, int);
static char *        (*DVDcss_error) (dvdcss_handle);
#endif

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
  dev = (dvd_input_t) malloc(sizeof(struct dvd_input_s));
  if(dev == NULL) {
    /* malloc has set errno to ENOMEM */
    return NULL;
  }
  
  /* Really open it with libdvdcss */
  dev->dvdcss = DVDcss_open(target);
  if(dev->dvdcss == 0) {
    free(dev);
    dev = NULL;
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
static int css_seek(dvd_input_t dev, int blocks)
{
  /* DVDINPUT_NOFLAGS should match the DVDCSS_NOFLAGS value. */
  return DVDcss_seek(dev->dvdcss, blocks, DVDINPUT_NOFLAGS);
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

/* Need to use O_BINARY for WIN32 */
#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0
#endif
#endif

/**
 * initialize and open a DVD device or file.
 */
static dvd_input_t file_open(const char *target)
{
  dvd_input_t dev;
  char *use_odirect;
  int oflags;
  
  oflags = O_RDONLY | O_BINARY;
  use_odirect = getenv("DVDREAD_USE_DIRECT");
  if(use_odirect) {
#ifndef O_DIRECT
#define O_DIRECT 0
#endif
    oflags |= O_DIRECT;
  }
  /* Allocate the library structure */
  dev = (dvd_input_t) malloc(sizeof(struct dvd_input_s));
  if(dev == NULL) {
    return NULL;
  }
  
  /* Open the device */
  dev->fd = open(target, oflags);
  if(dev->fd < 0) {
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
  return (char *)"unknown error";
}

/**
 * seek into the device.
 */
static int file_seek(dvd_input_t dev, int blocks)
{
  off_t pos = (off_t)blocks * (off_t)DVD_VIDEO_LB_LEN;

  pos = lseek(dev->fd, pos, SEEK_SET);
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
  unsigned char *buf = buffer;

  len = (size_t)blocks * DVD_VIDEO_LB_LEN;
  
  while(len > 0) {
    
    ret = read(dev->fd, buf, len);
    
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
    
    buf+=ret;
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


static void *dvdcss_library = NULL;
static int dvdcss_library_init = 0;

/**
 * Free any objects allocated by dvdinput_setup.
 * Should only be called when libdvdread is not to be used any more.
 * Closes dlopened libraries.
 */
void dvdinput_free(void)
{
#ifdef HAVE_DVDCSS_DVDCSS_H
  /* linked statically, nothing to free */
  return;
#else
  if(dvdcss_library) {
    dlclose(dvdcss_library);
    dvdcss_library = NULL;
  }
  dvdcss_library_init = 0;
  return;
#endif
}


/**
 * Setup read functions with either libdvdcss or minimal DVD access.
 */
int dvdinput_setup(void)
{
  char **dvdcss_version = NULL;
  int verbose;

  /* dlopening libdvdcss */
  if(dvdcss_library_init) {
    /* libdvdcss is already dlopened, function ptrs set */
    if(dvdcss_library) {
      return 1; /* css available */
    } else {
      return 0; /* css not available */
    }
  }

  verbose = get_verbose();
  
#ifdef HAVE_DVDCSS_DVDCSS_H
  /* linking to libdvdcss */
  dvdcss_library = &dvdcss_library;  /* Give it some value != NULL */
  /* the DVDcss_* functions have been #defined at the top */
  dvdcss_version = &dvdcss_interface_2;

#else

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
      if(verbose >= 0) {
        fprintf(stderr, 
                "libdvdread: Old (pre-0.0.2) version of libdvdcss found.\n"
                "libdvdread: You should get the latest version from "
                "http://www.videolan.org/\n" );
      }
      dlclose(dvdcss_library);
      dvdcss_library = NULL;
    } else if(!DVDcss_open  || !DVDcss_close || !DVDcss_title || !DVDcss_seek
              || !DVDcss_read || !DVDcss_error || !dvdcss_version) {
      if(verbose >= 0) {
        fprintf(stderr,  "libdvdread: Missing symbols in libdvdcss.so.2, "
                "this shouldn't happen !\n");
      }
      dlclose(dvdcss_library);
      dvdcss_library = NULL;
    }
  }
#endif /* HAVE_DVDCSS_DVDCSS_H */

  dvdcss_library_init = 1;
  
  if(dvdcss_library) {
    /*
      char *psz_method = getenv( "DVDCSS_METHOD" );
      char *psz_verbose = getenv( "DVDCSS_VERBOSE" );
      fprintf(stderr, "DVDCSS_METHOD %s\n", psz_method);
      fprintf(stderr, "DVDCSS_VERBOSE %s\n", psz_verbose);
    */
    if(verbose >= 1) {
      fprintf(stderr, "libdvdread: Using libdvdcss version %s for DVD access\n",
              *dvdcss_version);
    }
    /* libdvdcss wrapper functions */
    dvdinput_open  = css_open;
    dvdinput_close = css_close;
    dvdinput_seek  = css_seek;
    dvdinput_title = css_title;
    dvdinput_read  = css_read;
    dvdinput_error = css_error;
    return 1;
    
  } else {
    if(verbose >= 1) {
      fprintf(stderr, "libdvdread: Encrypted DVD support unavailable.\n");
    }
    /* libdvdcss replacement functions */
    dvdinput_open  = file_open;
    dvdinput_close = file_close;
    dvdinput_seek  = file_seek;
    dvdinput_title = file_title;
    dvdinput_read  = file_read;
    dvdinput_error = file_error;
    return 0;
  }
}
