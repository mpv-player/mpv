/* -*- c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>,
 *                                Håkan Hjort <d95hjort@dtek.chalmers.se>,
 *                                Björn Englund <d4bjorn@dtek.chalmers.se>
 *
 * Modified for use with MPlayer, changes contained in libdvdread_changes.diff.
 * detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h> /* For the timing of dvdcss_title crack. */
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
 
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__bsdi__) || defined(__DARWIN__) || defined(__DragonFly__)
#define SYS_BSD 1
#endif

#if defined(__sun)
#include <sys/mnttab.h>
#elif defined(hpux)
#include </usr/conf/h/mnttab.h>
#elif defined(SYS_BSD)
#include <fstab.h>
#elif defined(__linux__) || defined(__CYGWIN__)
#include <mntent.h>
#endif

#include "dvd_reader.h"
#include "dvd_input.h"
#include "dvd_udf.h"
#include "md5.h"

#include "dvdread_internal.h"

#define DEFAULT_UDF_CACHE_LEVEL 0

struct dvd_reader_s {
  /* Basic information. */
  int isImageFile;
  
  /* Hack for keeping track of the css status. 
   * 0: no css, 1: perhaps (need init of keys), 2: have done init */
  int css_state;
  int css_title; /* Last title that we have called dvdinpute_title for. */

  /* Information required for an image file. */
  dvd_input_t dev;

  /* Information required for a directory path drive. */
  char *path_root;
  
  /* Filesystem cache */
  int udfcache_level; /* 0 - turned off, 1 - on */
  void *udfcache;

  /* block aligned malloc */
  void *align;
  
  /* error message verbosity level */
  int verbose;
};

struct dvd_file_s {
  /* Basic information. */
  dvd_reader_t *dvd;
  
  /* Hack for selecting the right css title. */
  int css_title;

  /* Information required for an image file. */
  uint32_t lb_start;
  uint32_t seek_pos;

  /* Information required for a directory path drive. */
  size_t title_sizes[ 9 ];
  dvd_input_t title_devs[ 9 ];

  /* Calculated at open-time, size in blocks. */
  ssize_t filesize;
};


#define DVDREAD_VERBOSE_DEFAULT 0

int get_verbose(void)
{
  char *dvdread_verbose;
  int verbose;
  
  dvdread_verbose = getenv("DVDREAD_VERBOSE");
  if(dvdread_verbose) {
    verbose = (int)strtol(dvdread_verbose, NULL, 0);
  } else {
    verbose = DVDREAD_VERBOSE_DEFAULT;
  }
  return verbose;
}

int dvdread_verbose(dvd_reader_t *dvd)
{
  return dvd->verbose;
}

dvd_reader_t *device_of_file(dvd_file_t *file)
{
  return file->dvd;
}

/**
 * Returns the compiled version. (DVDREAD_VERSION as an int)
 */
int DVDVersion(void)
{
  return DVDREAD_VERSION;
}


/**
 * Set the level of caching on udf
 * level = 0 (no caching)
 * level = 1 (caching filesystem info)
 */
int DVDUDFCacheLevel(dvd_reader_t *device, int level)
{
  struct dvd_reader_s *dev = (struct dvd_reader_s *)device;
  
  if(level > 0) {
    level = 1;
  } else if(level < 0) {
    return dev->udfcache_level;
  }

  dev->udfcache_level = level;
  
  return level;
}

void *GetUDFCacheHandle(dvd_reader_t *device)
{
  struct dvd_reader_s *dev = (struct dvd_reader_s *)device;
  
  return dev->udfcache;
}

void SetUDFCacheHandle(dvd_reader_t *device, void *cache)
{
  struct dvd_reader_s *dev = (struct dvd_reader_s *)device;

  dev->udfcache = cache;
}

void *GetAlignHandle(dvd_reader_t *device)
{
  struct dvd_reader_s *dev = (struct dvd_reader_s *)device;
  
  return dev->align;
}

void SetAlignHandle(dvd_reader_t *device, void *align)
{
  struct dvd_reader_s *dev = (struct dvd_reader_s *)device;

  dev->align = align;
}


/* Loop over all titles and call dvdcss_title to crack the keys. */
static int initAllCSSKeys( dvd_reader_t *dvd )
{
  struct timeval all_s, all_e;
  struct timeval t_s, t_e;
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  uint32_t start, len;
  int title;
        
  char *nokeys_str = getenv("DVDREAD_NOKEYS");
  if(nokeys_str != NULL)
    return 0;
    
  if(dvd->verbose >= 1) {
    fprintf( stderr, "\n" );
    fprintf( stderr, "libdvdread: Attempting to retrieve all CSS keys\n" );
    fprintf( stderr, "libdvdread: This can take a _long_ time, "
             "please be patient\n\n" );
  }
  gettimeofday(&all_s, NULL);
        
  for( title = 0; title < 100; title++ ) {
    gettimeofday( &t_s, NULL );
    if( title == 0 ) {
      sprintf( filename, "/VIDEO_TS/VIDEO_TS.VOB" );
    } else {
      sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, 0 );
    }
    start = UDFFindFile( dvd, filename, &len );
    if( start != 0 && len != 0 ) {
      /* Perform CSS key cracking for this title. */
      if(dvd->verbose >= 1) {
        fprintf( stderr, "libdvdread: Get key for %s at 0x%08x\n", 
                 filename, start );
      }
      if( dvdinput_title( dvd->dev, (int)start ) < 0 ) {
        if(dvd->verbose >= 0) {
          fprintf( stderr, "libdvdread: Error cracking CSS key for %s (0x%08x)\n", filename, start);
        }
      }
      gettimeofday( &t_e, NULL );
      if(dvd->verbose >= 1) {
        fprintf( stderr, "libdvdread: Elapsed time %ld\n",  
                 (long int) t_e.tv_sec - t_s.tv_sec );
      }
    }
            
    if( title == 0 ) continue;
            
    gettimeofday( &t_s, NULL );
    sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, 1 );
    start = UDFFindFile( dvd, filename, &len );
    if( start == 0 || len == 0 ) break;
            
    /* Perform CSS key cracking for this title. */
    if(dvd->verbose >= 1) {
      fprintf( stderr, "libdvdread: Get key for %s at 0x%08x\n", 
               filename, start );
    }
    if( dvdinput_title( dvd->dev, (int)start ) < 0 ) {
      if(dvd->verbose >= 0) {
        fprintf( stderr, "libdvdread: Error cracking CSS key for %s (0x%08x)!!\n", filename, start);
      }
    }
    gettimeofday( &t_e, NULL );
    if(dvd->verbose >= 1) {
      fprintf( stderr, "libdvdread: Elapsed time %ld\n",  
               (long int) t_e.tv_sec - t_s.tv_sec );
    }
  }
  title--;
    
  if(dvd->verbose >= 1) {
    fprintf( stderr, "libdvdread: Found %d VTS's\n", title );
  }
  gettimeofday(&all_e, NULL);
  if(dvd->verbose >= 1) {
    fprintf( stderr, "libdvdread: Elapsed time %ld\n",  
             (long int) all_e.tv_sec - all_s.tv_sec );
  }
  return 0;
}



/**
 * Open a DVD image or block device file.
 * Checks if the root directory in the udf image file can be found.
 * If not it assumes this isn't a valid udf image and returns NULL
 */
static dvd_reader_t *DVDOpenImageFile( const char *location, int have_css )
{
  dvd_reader_t *dvd;
  dvd_input_t dev;
  int verbose;

  verbose = get_verbose();

  dev = dvdinput_open( location );
  if( !dev ) {
    if(verbose >= 1) {
      fprintf( stderr, "libdvdread: Can't open '%s' for reading: %s\n",
               location, strerror(errno));
    }
    return NULL;
  }

  dvd = (dvd_reader_t *) malloc( sizeof( dvd_reader_t ) );
  if( !dvd ) {
    int tmp_errno = errno;
    dvdinput_close(dev);
    errno = tmp_errno;
    return NULL;
  }
  dvd->verbose = verbose;
  dvd->isImageFile = 1;
  dvd->dev = dev;
  dvd->path_root = NULL;
    
  dvd->udfcache_level = DEFAULT_UDF_CACHE_LEVEL;
  dvd->udfcache = NULL;

  dvd->align = NULL;

  if( have_css ) {
    /* Only if DVDCSS_METHOD = title, a bit if it's disc or if
     * DVDCSS_METHOD = key but region missmatch. Unfortunaly we
     * don't have that information. */
    
    dvd->css_state = 1; /* Need key init. */
  }
  dvd->css_title = 0;
  
  /* sanity check, is it a valid UDF image, can we find the root dir */
  if(!UDFFindFile(dvd, "/", NULL)) {
    dvdinput_close(dvd->dev);
    if(dvd->udfcache) {
      FreeUDFCache(dvd, dvd->udfcache);
    }
    if(dvd->align) {
      if(dvd->verbose >= 0) {
        fprintf(stderr, "libdvdread: DVDOpenImageFile(): Memory leak in align functions 1\n");
      }
    }
    free(dvd);
    return NULL;
  }
  return dvd;
}

static dvd_reader_t *DVDOpenPath( const char *path_root )
{
  dvd_reader_t *dvd;

  dvd = (dvd_reader_t *) malloc( sizeof( dvd_reader_t ) );
  if( !dvd ) {
    return NULL;
  }
  dvd->verbose = get_verbose();
  dvd->isImageFile = 0;
  dvd->dev = 0;
  dvd->path_root = strdup( path_root );
  if(!dvd->path_root) {
    free(dvd);
    return 0;
  }
  dvd->udfcache_level = DEFAULT_UDF_CACHE_LEVEL;
  dvd->udfcache = NULL;

  dvd->align = NULL;

  dvd->css_state = 0; /* Only used in the UDF path */
  dvd->css_title = 0; /* Only matters in the UDF path */

  return dvd;
}

#if defined(__sun)
/* /dev/rdsk/c0t6d0s0 (link to /devices/...)
   /vol/dev/rdsk/c0t6d0/??
   /vol/rdsk/<name> */
static char *sun_block2char( const char *path )
{
  char *new_path;

  /* Must contain "/dsk/" */ 
  if( !strstr( path, "/dsk/" ) ) return (char *) strdup( path );

  /* Replace "/dsk/" with "/rdsk/" */
  new_path = malloc( strlen(path) + 2 );
  strcpy( new_path, path );
  strcpy( strstr( new_path, "/dsk/" ), "" );
  strcat( new_path, "/rdsk/" );
  strcat( new_path, strstr( path, "/dsk/" ) + strlen( "/dsk/" ) );

  return new_path;
}
#endif

#if defined(SYS_BSD)
/* FreeBSD /dev/(r)(a)cd0c (a is for atapi), recomended to _not_ use r
   update: FreeBSD and DragonFly no longer uses the prefix so don't add it.

   OpenBSD /dev/rcd0c, it needs to be the raw device
   NetBSD  /dev/rcd0[d|c|..] d for x86, c (for non x86), perhaps others
   Darwin  /dev/rdisk0,  it needs to be the raw device
   BSD/OS  /dev/sr0c (if not mounted) or /dev/rsr0c ('c' any letter will do)
   
   returns a string allocated with strdup which should be free()'d when
   no longer used.
*/
static char *bsd_block2char( const char *path )
{
#if defined(__FreeBSD__) || defined(__DragonFly__)
  return (char *) strdup( path );
#else
  char *new_path;

  /* If it doesn't start with "/dev/" or does start with "/dev/r" exit */ 
  if( strncmp( path, "/dev/",  5 ) || !strncmp( path, "/dev/r", 6 ) ) 
    return (char *) strdup( path );

  /* Replace "/dev/" with "/dev/r" */
  new_path = malloc( strlen(path) + 2 );
  strcpy( new_path, "/dev/r" );
  strcat( new_path, path + strlen( "/dev/" ) );

  return new_path;
#endif /* __FreeBSD__ || __DragonFly__ */
}
#endif


dvd_reader_t *DVDOpen( const char *path )
{
  struct stat fileinfo;
  int ret, have_css;
  char *dev_name = NULL;
  int internal_errno = 0;
  int verbose;

  if( path == NULL ) {
    errno = EINVAL;
    return NULL;
  }
  
  verbose = get_verbose();

#if defined(__CYGWIN__) || defined(__MINGW32__)
  /* Stat doesn't work on devices under mingwin/cygwin. */
  if( path[0] && path[1] == ':' && path[2] == '\0' )
    {
      /* Don't try to stat the file */
      fileinfo.st_mode = S_IFBLK;
    }
  else
#endif
    {
      ret = stat( path, &fileinfo );
      if( ret < 0 ) {
        int tmp_errno = errno;
        /* If we can't stat the file, give up */
        if(verbose >= 1) {
          fprintf( stderr, "libdvdread: Can't stat '%s': %s\n",
                   path, strerror(errno));
        }
        errno = tmp_errno;
        return NULL;
      }
    }

  /* Try to open libdvdcss or fall back to standard functions */
  have_css = dvdinput_setup();

  /* First check if this is a block/char device or a file*/
  if( S_ISBLK( fileinfo.st_mode ) || 
      S_ISCHR( fileinfo.st_mode ) || 
      S_ISREG( fileinfo.st_mode ) ) {
    /**
     * Block devices and regular files are assumed to be DVD-Video images.
     */
    dvd_reader_t *dvd = NULL;
#if defined(__sun)
    dev_name = sun_block2char( path );
#elif defined(SYS_BSD)
    dev_name = bsd_block2char( path );
#else
    dev_name = strdup( path );
#endif
    dvd = DVDOpenImageFile( dev_name, have_css );
    free( dev_name );
    
    return dvd;
  } else if( S_ISDIR( fileinfo.st_mode ) ) {
    dvd_reader_t *auth_drive = 0;
    char *path_copy;
#if defined(SYS_BSD)
    struct fstab* fe;
#elif defined(__sun) || defined(__linux__) || defined(__CYGWIN__)
    FILE *mntfile;
#endif

    /* XXX: We should scream real loud here. */
    if( !(path_copy = strdup( path ) ) ) return 0;

/* don't have fchdir, and getcwd( NULL, ... ) is strange */
#if !(defined(__CYGWIN__) || defined(__MINGW32__))
    /* Resolve any symlinks and get the absolut dir name. */
    {
      char *new_path;
      char *current_path;

      current_path = malloc(PATH_MAX);
      if(current_path) {
        if(!getcwd(current_path, PATH_MAX)) {
          free(current_path);
          current_path = NULL;
        }
      }
      if(current_path) {
        chdir( path_copy );
        new_path = malloc(PATH_MAX);
        if(new_path) {
          if(!getcwd(new_path, PATH_MAX )) {
            free(new_path);
            new_path = NULL;
          }
        }

        chdir(current_path);
        free(current_path);
        if( new_path ) {
          free( path_copy );
          path_copy = new_path;
        }
      }
    }
#endif
        
    /**
     * If we're being asked to open a directory, check if that directory
     * is the mountpoint for a DVD-ROM which we can use instead.
     */

    if( strlen( path_copy ) > 1 ) {
      if( path_copy[ strlen( path_copy ) - 1 ] == '/' ) {
        path_copy[ strlen( path_copy ) - 1 ] = '\0';
      }
    }

    if( strlen( path_copy ) >= 9 ) {
      if( !strcasecmp( &(path_copy[ strlen( path_copy ) - 9 ]), 
                       "/video_ts" ) ) {
        path_copy[ strlen( path_copy ) - 9 ] = '\0';
        if(path_copy[0] == '\0') {
          path_copy[0] = '/';
          path_copy[1] = '\0';
        }
      }
    }

#if defined(SYS_BSD)
    if( ( fe = getfsfile( path_copy ) ) ) {
      dev_name = bsd_block2char( fe->fs_spec );
      if(verbose >= 1) {
        fprintf( stderr,
                 "libdvdread: Attempting to use device %s"
                 " mounted on %s%s\n",
                 dev_name,
                 fe->fs_file,
                 have_css ? " for CSS authentication" : "");
      }
      auth_drive = DVDOpenImageFile( dev_name, have_css );
      if(!auth_drive) {
        internal_errno = errno;
      }
    }
#elif defined(__sun)
    mntfile = fopen( MNTTAB, "r" );
    if( mntfile ) {
      struct mnttab mp;
      int res;
      
      while( ( res = getmntent( mntfile, &mp ) ) != -1 ) {
        if( res == 0 && !strcmp( mp.mnt_mountp, path_copy ) ) {
          dev_name = sun_block2char( mp.mnt_special );
          if(verbose >= 1) {
            fprintf( stderr, 
                     "libdvdread: Attempting to use device %s"
                     " mounted on %s%s\n",
                     dev_name,
                     mp.mnt_mountp,
                     have_css ? " for CSS authentication" : "");
          }
          auth_drive = DVDOpenImageFile( dev_name, have_css );
          if(!auth_drive) {
            internal_errno = errno;
          }
          break;
        }
      }
      fclose( mntfile );
    }
#elif defined(__linux__) || defined(__CYGWIN__)
    mntfile = fopen( MOUNTED, "r" );
    if( mntfile ) {
      struct mntent *me;
 
      while( ( me = getmntent( mntfile ) ) ) {
        if( !strcmp( me->mnt_dir, path_copy ) ) {
          if(verbose >= 1) {
            fprintf( stderr, 
                     "libdvdread: Attempting to use device %s"
                     " mounted on %s%s\n",
                     me->mnt_fsname,
                     me->mnt_dir,
                     have_css ? " for CSS authentication" : "");
          }
          auth_drive = DVDOpenImageFile( me->mnt_fsname, have_css );
          if(!auth_drive) {
            internal_errno = errno;
          }
          dev_name = strdup(me->mnt_fsname);
          break;
        }
      }
      fclose( mntfile );
    }
#elif defined(__MINGW32__)
    dev_name = strdup(path);
    auth_drive = DVDOpenImageFile( path, have_css );
#endif
    if( !dev_name ) {
      if(verbose >= 1) {
        fprintf( stderr, "libdvdread: Couldn't find device name.\n" );
      }
    } else if( !auth_drive ) {
      if(verbose >= 1) {
        fprintf( stderr, "libdvdread: Device %s inaccessible%s: %s\n",
                 dev_name,
                 have_css ? ", CSS authentication not available" : "",
                 strerror(internal_errno));
      }
    }

    free( dev_name );
    free( path_copy );

    /**
     * If we've opened a drive, just use that.
     */
    if( auth_drive ) {
      return auth_drive;
    }
    /**
     * Otherwise, we now try to open the directory tree instead.
     */
    return DVDOpenPath( path );
  }

  /* If it's none of the above, screw it. */
  if(verbose >= 1) {
    fprintf( stderr, "libdvdread: Could not open %s\n", path );
  }
  return 0;
}

void DVDClose( dvd_reader_t *dvd )
{
  if( dvd ) {
    if( dvd->dev ) dvdinput_close( dvd->dev );
    if( dvd->path_root ) free( dvd->path_root );
    if( dvd->udfcache ) FreeUDFCache( dvd, dvd->udfcache );
    if(dvd->align) {
      if(dvd->verbose >= 0) {
        fprintf(stderr, "libdvdread: DVDClose(): Memory leak in align functions\n");
      }
    }

    free( dvd );
  }
}

void DVDInit(void)
{
  dvdinput_setup();
}

void DVDFinish(void)
{
  dvdinput_free();
}

/**
 * Open an unencrypted file on a DVD image file.
 */
static dvd_file_t *DVDOpenFileUDF( dvd_reader_t *dvd, char *filename )
{
  uint32_t start, len;
  dvd_file_t *dvd_file;

  start = UDFFindFile( dvd, filename, &len );
  if( !start ) return 0;

  dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
  if( !dvd_file ) return 0;
  dvd_file->dvd = dvd;
  dvd_file->lb_start = start;
  dvd_file->seek_pos = 0;
  memset( dvd_file->title_sizes, 0, sizeof( dvd_file->title_sizes ) );
  memset( dvd_file->title_devs, 0, sizeof( dvd_file->title_devs ) );
  dvd_file->filesize = len / DVD_VIDEO_LB_LEN;

  return dvd_file;
}

/**
 * Searches for <file> in directory <path>, ignoring case.
 * Returns 0 and full filename in <filename>.
 *     or -1 on file not found.
 *     or -2 on path not found.
 */
static int findDirFile( const char *path, const char *file, char *filename ) 
{
  DIR *dir;
  struct dirent *ent;

  dir = opendir( path );
  if( !dir ) return -2;

  while( ( ent = readdir( dir ) ) != NULL ) {
    if( !strcasecmp( ent->d_name, file ) ) {
      sprintf( filename, "%s%s%s", path,
               ( ( path[ strlen( path ) - 1 ] == '/' ) ? "" : "/" ),
               ent->d_name );
      closedir(dir);
      return 0;
    }
  }
  closedir(dir);
  return -1;
}

static int findDVDFile( dvd_reader_t *dvd, const char *file, char *filename )
{
  char video_path[ PATH_MAX + 1 ];
  const char *nodirfile;
  int ret;

  /* Strip off the directory for our search */
  if( !strncasecmp( "/VIDEO_TS/", file, 10 ) ) {
    nodirfile = &(file[ 10 ]);
  } else {
    nodirfile = file;
  }

  ret = findDirFile( dvd->path_root, nodirfile, filename );
  if( ret < 0 ) {
    /* Try also with adding the path, just in case. */
    sprintf( video_path, "%s/VIDEO_TS/", dvd->path_root );
    ret = findDirFile( video_path, nodirfile, filename );
    if( ret < 0 ) {
      /* Try with the path, but in lower case. */
      sprintf( video_path, "%s/video_ts/", dvd->path_root );
      ret = findDirFile( video_path, nodirfile, filename );
      if( ret < 0 ) {
        return 0;
      }
    }
  }

  return 1;
}

/**
 * Open an unencrypted file from a DVD directory tree.
 */
static dvd_file_t *DVDOpenFilePath( dvd_reader_t *dvd, char *filename )
{
  char full_path[ PATH_MAX + 1 ];
  dvd_file_t *dvd_file;
  struct stat fileinfo;
  dvd_input_t dev;

  /* Get the full path of the file. */
  if( !findDVDFile( dvd, filename, full_path ) ) return 0;

  dev = dvdinput_open( full_path );
  if( !dev ) return 0;

  dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
  if( !dvd_file ) return 0;
  dvd_file->dvd = dvd;
  dvd_file->lb_start = 0;
  dvd_file->seek_pos = 0;
  memset( dvd_file->title_sizes, 0, sizeof( dvd_file->title_sizes ) );
  memset( dvd_file->title_devs, 0, sizeof( dvd_file->title_devs ) );
  dvd_file->filesize = 0;

  if( stat( full_path, &fileinfo ) < 0 ) {
    if(dvd->verbose >= 1) {
      fprintf( stderr, "libdvdread: Can't stat() %s.\n", filename );
    }
    free( dvd_file );
    return 0;
  }
  dvd_file->title_sizes[ 0 ] = fileinfo.st_size / DVD_VIDEO_LB_LEN;
  dvd_file->title_devs[ 0 ] = dev;
  dvd_file->filesize = dvd_file->title_sizes[ 0 ];

  return dvd_file;
}

static dvd_file_t *DVDOpenVOBUDF( dvd_reader_t *dvd, int title, int menu )
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  uint32_t start, len;
  dvd_file_t *dvd_file;

  if( title == 0 ) {
    sprintf( filename, "/VIDEO_TS/VIDEO_TS.VOB" );
  } else {
    sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, menu ? 0 : 1 );
  }
  start = UDFFindFile( dvd, filename, &len );
  if( start == 0 ) return 0;

  dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
  if( !dvd_file ) return 0;
  dvd_file->dvd = dvd;
  /*Hack*/ dvd_file->css_title = title << 1 | menu;
  dvd_file->lb_start = start;
  dvd_file->seek_pos = 0;
  memset( dvd_file->title_sizes, 0, sizeof( dvd_file->title_sizes ) );
  memset( dvd_file->title_devs, 0, sizeof( dvd_file->title_devs ) );
  dvd_file->filesize = len / DVD_VIDEO_LB_LEN;

  /* Calculate the complete file size for every file in the VOBS */
  if( !menu ) {
    int cur;

    for( cur = 2; cur < 10; cur++ ) {
      sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, cur );
      if( !UDFFindFile( dvd, filename, &len ) ) break;
      dvd_file->filesize += len / DVD_VIDEO_LB_LEN;
    }
  }
    
  if( dvd->css_state == 1 /* Need key init */ ) {
//    initAllCSSKeys( dvd );
//    dvd->css_state = 2;
  }
  /*    
        if( dvdinput_title( dvd_file->dvd->dev, (int)start ) < 0 ) {
        fprintf( stderr, "libdvdread: Error cracking CSS key for %s\n",
        filename );
        }
  */
    
  return dvd_file;
}

static dvd_file_t *DVDOpenVOBPath( dvd_reader_t *dvd, int title, int menu )
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  char full_path[ PATH_MAX + 1 ];
  struct stat fileinfo;
  dvd_file_t *dvd_file;
  int i;

  dvd_file = (dvd_file_t *) malloc( sizeof( dvd_file_t ) );
  if( !dvd_file ) return 0;
  dvd_file->dvd = dvd;
  /*Hack*/ dvd_file->css_title = title << 1 | menu;
  dvd_file->lb_start = 0;
  dvd_file->seek_pos = 0;
  memset( dvd_file->title_sizes, 0, sizeof( dvd_file->title_sizes ) );
  memset( dvd_file->title_devs, 0, sizeof( dvd_file->title_devs ) );
  dvd_file->filesize = 0;
    
  if( menu ) {
    dvd_input_t dev;

    if( title == 0 ) {
      sprintf( filename, "VIDEO_TS.VOB" );
    } else {
      sprintf( filename, "VTS_%02i_0.VOB", title );
    }
    if( !findDVDFile( dvd, filename, full_path ) ) {
      free( dvd_file );
      return 0;
    }

    dev = dvdinput_open( full_path );
    if( dev == NULL ) {
      free( dvd_file );
      return 0;
    }

    if( stat( full_path, &fileinfo ) < 0 ) {
      if(dvd->verbose >= 1) {
        fprintf( stderr, "libdvdread: Can't stat() %s.\n", filename );
      }
      free( dvd_file );
      return 0;
    }
    dvd_file->title_sizes[ 0 ] = fileinfo.st_size / DVD_VIDEO_LB_LEN;
    dvd_file->title_devs[ 0 ] = dev;
    dvdinput_title( dvd_file->title_devs[0], 0);
    dvd_file->filesize = dvd_file->title_sizes[ 0 ];

  } else {
    for( i = 0; i < 9; ++i ) {

      sprintf( filename, "VTS_%02i_%i.VOB", title, i + 1 );
      if( !findDVDFile( dvd, filename, full_path ) ) {
        break;
      }

      if( stat( full_path, &fileinfo ) < 0 ) {
        if(dvd->verbose >= 1) {
          fprintf( stderr, "libdvdread: Can't stat() %s.\n", filename );
        }
        break;
      }

      dvd_file->title_sizes[ i ] = fileinfo.st_size / DVD_VIDEO_LB_LEN;
      dvd_file->title_devs[ i ] = dvdinput_open( full_path );
      dvdinput_title( dvd_file->title_devs[ i ], 0 );
      dvd_file->filesize += dvd_file->title_sizes[ i ];
    }
    if( !dvd_file->title_devs[ 0 ] ) {
      free( dvd_file );
      return 0;
    }
  }

  return dvd_file;
}

dvd_file_t *DVDOpenFile( dvd_reader_t *dvd, int titlenum, 
                         dvd_read_domain_t domain )
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
    
  /* Check arguments. */
  if( dvd == NULL || titlenum < 0 ) {
    errno = EINVAL;
    return NULL;
  }

  switch( domain ) {
  case DVD_READ_INFO_FILE:
    if( titlenum == 0 ) {
      sprintf( filename, "/VIDEO_TS/VIDEO_TS.IFO" );
    } else {
      sprintf( filename, "/VIDEO_TS/VTS_%02i_0.IFO", titlenum );
    }
    break;
  case DVD_READ_INFO_BACKUP_FILE:
    if( titlenum == 0 ) {
      sprintf( filename, "/VIDEO_TS/VIDEO_TS.BUP" );
    } else {
      sprintf( filename, "/VIDEO_TS/VTS_%02i_0.BUP", titlenum );
    }
    break;
  case DVD_READ_MENU_VOBS:
    if( dvd->isImageFile ) {
      return DVDOpenVOBUDF( dvd, titlenum, 1 );
    } else {
      return DVDOpenVOBPath( dvd, titlenum, 1 );
    }
    break;
  case DVD_READ_TITLE_VOBS:
    if( titlenum == 0 ) return 0;
    if( dvd->isImageFile ) {
      return DVDOpenVOBUDF( dvd, titlenum, 0 );
    } else {
      return DVDOpenVOBPath( dvd, titlenum, 0 );
    }
    break;
  default:
    if(dvd->verbose >= 1) {
      fprintf( stderr, "libdvdread: Invalid domain for file open.\n" );
    }
    errno = EINVAL;
    return NULL;
  }
    
  if( dvd->isImageFile ) {
    return DVDOpenFileUDF( dvd, filename );
  } else {
    return DVDOpenFilePath( dvd, filename );
  }
}

void DVDCloseFile( dvd_file_t *dvd_file )
{
  int i;

  if( dvd_file ) {
    if( dvd_file->dvd->isImageFile ) {
      ;
    } else {
      for( i = 0; i < 9; ++i ) {
        if( dvd_file->title_devs[ i ] ) {
          dvdinput_close( dvd_file->title_devs[i] );
        }
      }
    }

    free( dvd_file );
    dvd_file = 0;
  }
}

static int DVDFileStatVOBUDF(dvd_reader_t *dvd, int title, 
                             int menu, dvd_stat_t *statbuf)
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  uint32_t size;
  off_t tot_size;
  off_t parts_size[9];
  int nr_parts = 0;
  int n;
 
  if( title == 0 ) {
    sprintf( filename, "/VIDEO_TS/VIDEO_TS.VOB" );
  } else {
    sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, menu ? 0 : 1 );
  }
  if(!UDFFindFile( dvd, filename, &size )) {
    return -1;
  }
  tot_size = size;
  nr_parts = 1;
  parts_size[0] = size;

  if( !menu ) {
    int cur;

    for( cur = 2; cur < 10; cur++ ) {
      sprintf( filename, "/VIDEO_TS/VTS_%02d_%d.VOB", title, cur );
      if( !UDFFindFile( dvd, filename, &size ) ) {
        break;
      }
      parts_size[nr_parts] = size;
      tot_size += size;
      nr_parts++;
    }
  }
  
  statbuf->size = tot_size;
  statbuf->nr_parts = nr_parts;
  for(n = 0; n < nr_parts; n++) {
    statbuf->parts_size[n] = parts_size[n];
  }
  return 0;
}


static int DVDFileStatVOBPath( dvd_reader_t *dvd, int title,
                                       int menu, dvd_stat_t *statbuf )
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  char full_path[ PATH_MAX + 1 ];
  struct stat fileinfo;
  off_t tot_size;
  off_t parts_size[9];
  int nr_parts = 0;
  int n;

 
    
  if( title == 0 ) {
    sprintf( filename, "VIDEO_TS.VOB" );
  } else {
    sprintf( filename, "VTS_%02d_%d.VOB", title, menu ? 0 : 1 );
  }
  if( !findDVDFile( dvd, filename, full_path ) ) {
    return -1;
  }
  
  if( stat( full_path, &fileinfo ) < 0 ) {
    if(dvd->verbose >= 1) {
      fprintf( stderr, "libdvdread: Can't stat() %s.\n", filename );
    }
    return -1;
  }
  

  tot_size = fileinfo.st_size;
  nr_parts = 1;
  parts_size[0] = fileinfo.st_size;

  if( !menu ) {
    int cur;
    
    for( cur = 2; cur < 10; cur++ ) {

      sprintf( filename, "VTS_%02d_%d.VOB", title, cur );
      if( !findDVDFile( dvd, filename, full_path ) ) {
        break;
      }

      if( stat( full_path, &fileinfo ) < 0 ) {
        if(dvd->verbose >= 1) {
          fprintf( stderr, "libdvdread: Can't stat() %s.\n", filename );
        }
        break;
      }
      
      parts_size[nr_parts] = fileinfo.st_size;
      tot_size += parts_size[nr_parts];
      nr_parts++;
    }
  }

  statbuf->size = tot_size;
  statbuf->nr_parts = nr_parts;
  for(n = 0; n < nr_parts; n++) {
    statbuf->parts_size[n] = parts_size[n];
  }
  return 0;
}


int DVDFileStat(dvd_reader_t *dvd, int titlenum, 
                dvd_read_domain_t domain, dvd_stat_t *statbuf)
{
  char filename[ MAX_UDF_FILE_NAME_LEN ];
  char full_path[ PATH_MAX + 1 ];
  struct stat fileinfo;
  uint32_t size;

  /* Check arguments. */
  if( dvd == NULL || titlenum < 0 ) {
    errno = EINVAL;
    return -1;
  }

  switch( domain ) {
  case DVD_READ_INFO_FILE:
    if( titlenum == 0 ) {
      sprintf( filename, "/VIDEO_TS/VIDEO_TS.IFO" );
    } else {
      sprintf( filename, "/VIDEO_TS/VTS_%02i_0.IFO", titlenum );
    }
    break;
  case DVD_READ_INFO_BACKUP_FILE:
    if( titlenum == 0 ) {
      sprintf( filename, "/VIDEO_TS/VIDEO_TS.BUP" );
    } else {
      sprintf( filename, "/VIDEO_TS/VTS_%02i_0.BUP", titlenum );
    }
    break;
  case DVD_READ_MENU_VOBS:
    if( dvd->isImageFile ) {
      return DVDFileStatVOBUDF( dvd, titlenum, 1, statbuf );
    } else {
      return DVDFileStatVOBPath( dvd, titlenum, 1, statbuf );
    }
    break;
  case DVD_READ_TITLE_VOBS:
    if( titlenum == 0 ) {
      return -1;
    }
    if( dvd->isImageFile ) {
      return DVDFileStatVOBUDF( dvd, titlenum, 0, statbuf );
    } else {
      return DVDFileStatVOBPath( dvd, titlenum, 0, statbuf );
    }
    break;
  default:
    if(dvd->verbose >= 1) {
      fprintf( stderr, "libdvdread: Invalid domain for file stat.\n" );
    }
    errno = EINVAL;
    return -1;
  }
  
  if( dvd->isImageFile ) {
    if( UDFFindFile( dvd, filename, &size ) ) {
      statbuf->size = size;
      statbuf->nr_parts = 1;
      statbuf->parts_size[0] = size;
      return 0;
    }
  } else {
    if( findDVDFile( dvd, filename, full_path ) )  {
      if( stat( full_path, &fileinfo ) < 0 ) {
        if(dvd->verbose >= 1) {
          fprintf( stderr, "libdvdread: Can't stat() %s.\n", filename );
        }
      } else {
        statbuf->size = fileinfo.st_size;
        statbuf->nr_parts = 1;
        statbuf->parts_size[0] = statbuf->size;
        return 0;
      }
    }
  }
  return -1;
}

/**
 * Internal, but used from dvd_udf.c 
 *
 * @param device A read handle.
 * @param lb_number Logical block number to start read from.
 * @param block_count Number of logical blocks to read.
 * @param data Pointer to buffer where read data should be stored.
 *             This buffer must be large enough to hold lb_number*2048 bytes.
 *             The pointer must be aligned to the logical block size when
 *             reading from a raw/O_DIRECT device.
 * @param encrypted 0 if no decryption shall be performed,
 *                  1 if decryption shall be performed
 * @param return Returns number of blocks read on success, negative on error
 */
int UDFReadBlocksRaw( dvd_reader_t *device, uint32_t lb_number,
                      size_t block_count, unsigned char *data, 
                      int encrypted )
{
  int ret;

  if( !device->dev ) {
    if(device->verbose >= 1) {
      fprintf( stderr, "libdvdread: Fatal error in block read.\n" );
    }
    return 0;
  }

  ret = dvdinput_seek( device->dev, (int) lb_number );
  if( ret != (int) lb_number ) {
    if(device->verbose >= 1) {
      fprintf( stderr,
               "libdvdread: UDFReadBlocksRaw: Can't seek to block %u\n",
               lb_number );
    }
    return 0;
  }

  return dvdinput_read( device->dev, (char *) data, 
                        (int) block_count, encrypted );
}

/**
 * This is using a single input and starting from 'dvd_file->lb_start' offset.
 *
 * Reads 'block_count' blocks from 'dvd_file' at block offset 'offset'
 * into the buffer located at 'data' and if 'encrypted' is set
 * descramble the data if it's encrypted.  Returning either an
 * negative error or the number of blocks read.
 *
 * @param data Pointer to buffer where read data should be placed.
 *             This buffer must be large enough to hold block_count*2048 bytes.
 *             The pointer must be aligned to 2048 bytes when reading from
 *             a raw/O_DIRECT device.
 * @return Returns the number of blocks read on success or a negative error.
 */
static int DVDReadBlocksUDF( dvd_file_t *dvd_file, uint32_t offset,
                             size_t block_count, unsigned char *data,
                             int encrypted )
{
  return UDFReadBlocksRaw( dvd_file->dvd, dvd_file->lb_start + offset,
                           block_count, data, encrypted );
}

/**
 * This is using possibly several inputs and starting from an offset of '0'.
 * data must be aligned to logical block size (2048 bytes) of the device
 * for raw/O_DIRECT devices to work
 * Reads 'block_count' blocks from 'dvd_file' at block offset 'offset'
 * into the buffer located at 'data' and if 'encrypted' is set
 * descramble the data if it's encrypted.  Returning either an
 * negative error or the number of blocks read.
 *
 * @param dvd_file A file read handle.
 * @param offset Block offset from start of file.
 * @return Returns number of blocks read on success, negative on error.
 */
static int DVDReadBlocksPath( dvd_file_t *dvd_file, unsigned int offset,
                              size_t block_count, unsigned char *data,
                              int encrypted )
{
  int i;
  int ret, ret2, off;

  ret = 0;
  ret2 = 0;
  for( i = 0; i < 9; ++i ) {
    if( !dvd_file->title_sizes[ i ] ) return 0; /* Past end of file */

    if( offset < dvd_file->title_sizes[ i ] ) {
      if( ( offset + block_count ) <= dvd_file->title_sizes[ i ] ) {
        off = dvdinput_seek( dvd_file->title_devs[ i ], (int)offset );
        if( off < 0 || off != (int)offset ) {
          if(dvd_file->dvd->verbose >= 1) {
            fprintf( stderr, "libdvdread: DVDReadBlocksPath1: Can't seek to block %d\n", 
                     offset );
          }
          return off < 0 ? off : 0;
        }
        ret = dvdinput_read( dvd_file->title_devs[ i ], data,
                             (int)block_count, encrypted );
        break;
      } else {
        size_t part1_size = dvd_file->title_sizes[ i ] - offset;
        /* FIXME: Really needs to be a while loop.
         * (This is only true if you try and read >1GB at a time) */
                
        /* Read part 1 */
        off = dvdinput_seek( dvd_file->title_devs[ i ], (int)offset );
        if( off < 0 || off != (int)offset ) {
          if(dvd_file->dvd->verbose >= 1) {
            fprintf( stderr, "libdvdread: DVDReadBlocksPath2: Can't seek to block %d\n", 
                     offset );
          }
          return off < 0 ? off : 0;
        }
        ret = dvdinput_read( dvd_file->title_devs[ i ], data,
                             (int)part1_size, encrypted );
        if( ret < 0 ) return ret;
        /* FIXME: This is wrong if i is the last file in the set. 
         * also error from this read will not show in ret. */
                
        /* Does the next part exist? If not then return now. */
        if( !dvd_file->title_devs[ i + 1 ] ) return ret;

        /* Read part 2 */
        off = dvdinput_seek( dvd_file->title_devs[ i + 1 ], 0 );
        if( off < 0 || off != 0 ) {
          if(dvd_file->dvd->verbose >= 1) {
            fprintf( stderr, "libdvdread: DVDReadBlocksPath3: Can't seek to block %d\n", 0 );
          }
          return off < 0 ? off : 0;
        }
        ret2 = dvdinput_read( dvd_file->title_devs[ i + 1 ], 
                              data + ( part1_size
                                       * (int64_t)DVD_VIDEO_LB_LEN ),
                              (int)(block_count - part1_size),
                              encrypted );
        if( ret2 < 0 ) return ret2;
        break;
      }
    } else {
      offset -= dvd_file->title_sizes[ i ];
    }
  }

  return ret + ret2;
}

/**
 * This is broken reading more than 2Gb at a time if ssize_t is 32-bit.
 */
ssize_t DVDReadBlocks( dvd_file_t *dvd_file, int offset, 
                       size_t block_count, unsigned char *data )
{
  int ret;
    
  /* Check arguments. */
  if( dvd_file == NULL || offset < 0 || data == NULL )
    return -1;
    
  /* Hack, and it will still fail for multiple opens in a threaded app ! */
  if( dvd_file->dvd->css_title != dvd_file->css_title ) {
    dvd_file->dvd->css_title = dvd_file->css_title;
    if( dvd_file->dvd->isImageFile ) {
      dvdinput_title( dvd_file->dvd->dev, (int)dvd_file->lb_start );
    } 
    /* Here each vobu has it's own dvdcss handle, so no need to update 
       else {
       dvdinput_title( dvd_file->title_devs[ 0 ], (int)dvd_file->lb_start );
       }*/
  }
    
  if( dvd_file->dvd->isImageFile ) {
    ret = DVDReadBlocksUDF( dvd_file, (uint32_t)offset, 
                            block_count, data, DVDINPUT_READ_DECRYPT );
  } else {
    ret = DVDReadBlocksPath( dvd_file, (unsigned int)offset, 
                             block_count, data, DVDINPUT_READ_DECRYPT );
  }
    
  return (ssize_t)ret;
}

int DVDFileSeek( dvd_file_t *dvd_file, int offset )
{
  /* Check arguments. */
  if( dvd_file == NULL || offset < 0 )
    return -1;
    
  if( offset > dvd_file->filesize * DVD_VIDEO_LB_LEN ) {
    return -1;
  }
  dvd_file->seek_pos = (uint32_t) offset;
  return offset;
}

#ifndef HAVE_UINTPTR_T
#warning "Assuming that (unsigned long) can hold (void *)"
typedef unsigned long uintptr_t;
#endif

#define DVD_ALIGN(ptr) (void *)((((uintptr_t)(ptr)) + (DVD_VIDEO_LB_LEN-1)) \
                                / DVD_VIDEO_LB_LEN * DVD_VIDEO_LB_LEN)

ssize_t DVDReadBytes( dvd_file_t *dvd_file, void *data, size_t byte_size )
{
  unsigned char *secbuf_start;
  unsigned char *secbuf; //must be aligned to 2048-bytes for raw/O_DIRECT
  unsigned int numsec, seek_sector, seek_byte;
  int ret;
    
  /* Check arguments. */
  if( dvd_file == NULL || data == NULL ) {
    errno = EINVAL;
    return -1;
  }
  seek_sector = dvd_file->seek_pos / DVD_VIDEO_LB_LEN;
  seek_byte   = dvd_file->seek_pos % DVD_VIDEO_LB_LEN;

  numsec = ( ( seek_byte + byte_size ) / DVD_VIDEO_LB_LEN ) +
    ( ( ( seek_byte + byte_size ) % DVD_VIDEO_LB_LEN ) ? 1 : 0 );

  /* must align to 2048 bytes if we are reading from raw/O_DIRECT */
  secbuf_start = (unsigned char *) malloc( (numsec+1) * DVD_VIDEO_LB_LEN );
  if( !secbuf_start ) {
    /* errno will be set to ENOMEM by malloc */
    return -1;
  }

  secbuf = DVD_ALIGN(secbuf_start);

  if( dvd_file->dvd->isImageFile ) {
    ret = DVDReadBlocksUDF( dvd_file, (uint32_t) seek_sector, 
                            (size_t) numsec, secbuf, DVDINPUT_NOFLAGS );
  } else {
    ret = DVDReadBlocksPath( dvd_file, seek_sector, 
                             (size_t) numsec, secbuf, DVDINPUT_NOFLAGS );
  }

  if( ret != (int) numsec ) {
    free( secbuf_start );
    return ret < 0 ? ret : 0;
  }

  memcpy( data, &(secbuf[ seek_byte ]), byte_size );
  free( secbuf_start );

  dvd_file->seek_pos += byte_size;
  return byte_size;
}

ssize_t DVDFileSize( dvd_file_t *dvd_file )
{
  /* Check arguments. */
  if( dvd_file == NULL )
    return -1;
    
  return dvd_file->filesize;
}

int DVDDiscID( dvd_reader_t *dvd, unsigned char *discid )
{
  struct md5_ctx ctx;
  int title;
  int nr_of_files = 0;
  int tmp_errno;
  int nofiles_errno = ENOENT;
  /* Check arguments. */
  if( dvd == NULL || discid == NULL ) {
    errno = EINVAL;
    return -1;
  }
  /* Go through the first 10 IFO:s, in order, 
   * and md5sum them, i.e  VIDEO_TS.IFO and VTS_0?_0.IFO */
  md5_init_ctx( &ctx );
  for( title = 0; title < 10; title++ ) {
    dvd_file_t *dvd_file = DVDOpenFile( dvd, title, DVD_READ_INFO_FILE );
    if( dvd_file != NULL ) {
      ssize_t bytes_read;
      size_t file_size = dvd_file->filesize * DVD_VIDEO_LB_LEN;
      char *buffer = malloc( file_size );

      nr_of_files++;

      if( buffer == NULL ) {
        /* errno will be set to ENOMEM by malloc */
        return -1;
      }

      bytes_read = DVDReadBytes( dvd_file, buffer, file_size );
      if( bytes_read != file_size ) {
        tmp_errno = errno;
        if(dvd->verbose >= 1) {
          fprintf( stderr, "libdvdread: DVDDiscId read returned %d bytes"
                   ", wanted %d\n", (int)bytes_read, (int)file_size );
        }
        free(buffer);
        DVDCloseFile( dvd_file );
        errno = tmp_errno;
        return -1;
      }
            
      md5_process_bytes( buffer, file_size,  &ctx );
            
      DVDCloseFile( dvd_file );
      free( buffer );
    } else {
      if(errno != ENOENT) {
        nofiles_errno = errno;
      }
    }
  }
  md5_finish_ctx( &ctx, discid );
  if(nr_of_files == 0) {
    errno = nofiles_errno;
    return -1;
  }
  return 0;
}


int DVDISOVolumeInfo( dvd_reader_t *dvd,
                      char *volid, unsigned int volid_size,
                      unsigned char *volsetid, unsigned int volsetid_size )
{
  unsigned char *buffer; /* must be aligned to 2048 for raw/O_DIRECT */
  unsigned char *buffer_start; 
  int ret;

  /* Check arguments. */
  if( dvd == NULL ) {
    errno = EINVAL;
    return -1;
  }
  
  if( dvd->dev == NULL ) {
    /* No block access, so no ISO... */
    errno = EINVAL;
    return -1;
  }
  
  buffer_start = malloc( 2 * DVD_VIDEO_LB_LEN );
  if( buffer_start == NULL ) {
    return -1;
  }

  buffer = DVD_ALIGN(buffer_start);
  
  ret = UDFReadBlocksRaw( dvd, 16, 1, buffer, 0 );
  if( ret != 1 ) {
    if(dvd->verbose >= 1) {
      fprintf( stderr, "libdvdread: DVDISOVolumeInfo, failed to "
               "read ISO9660 Primary Volume Descriptor!\n" );
    }
    free(buffer_start);
    return -1;
  }
  
  if( (volid != NULL) && (volid_size > 0) ) {
    unsigned int n;
    for(n = 0; n < 32; n++) {
      if(buffer[40+n] == 0x20) {
        break;
      }
    }
    
    if(volid_size > n+1) {
      volid_size = n+1;
    }

    memcpy(volid, &buffer[40], volid_size-1);
    volid[volid_size-1] = '\0';
  }
  
  if( (volsetid != NULL) && (volsetid_size > 0) ) {
    if(volsetid_size > 128) {
      volsetid_size = 128;
    }
    memcpy(volsetid, &buffer[190], volsetid_size);
  }
  free(buffer_start);

  return 0;
}


int DVDUDFVolumeInfo( dvd_reader_t *dvd,
                      char *volid, unsigned int volid_size,
                      unsigned char *volsetid, unsigned int volsetid_size )
{
  int ret;
  /* Check arguments. */
  if( dvd == NULL )
    return -1;
  
  if( dvd->dev == NULL ) {
    /* No block access, so no UDF VolumeSet Identifier */
    return -1;
  }
  
  if( (volid != NULL) && (volid_size > 0) ) {
    ret = UDFGetVolumeIdentifier(dvd, volid, volid_size);
    if(!ret) {
      return -1;
    }
  }
  if( (volsetid != NULL) && (volsetid_size > 0) ) {
    ret =  UDFGetVolumeSetIdentifier(dvd, volsetid, volsetid_size);
    if(!ret) {
      return -1;
    }
  }
    
  return 0;  
}
