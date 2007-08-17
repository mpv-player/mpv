/* -*- c-basic-offset: 2; indent-tabs-mode: nil -*- */
#ifndef DVD_READER_H_INCLUDED
#define DVD_READER_H_INCLUDED

/*
 * Copyright (C) 2001, 2002 Billy Biggs <vektor@dumbterm.net>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>,
 *                          Björn Englund <d4bjorn@dtek.chalmers.se>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>

/**
 * The DVD access interface.
 *
 * This file contains the functions that form the interface to to
 * reading files located on a DVD.
 */

/**
 * The current version. (0.9.4 => 904, 1.2.3 => 10203)
 */
#define DVDREAD_VERSION 907


/**
 * The length of one Logical Block of a DVD.
 */
#define DVD_VIDEO_LB_LEN 2048

/**
 * Maximum length of filenames allowed in UDF.
 */
#define MAX_UDF_FILE_NAME_LEN 2048

#ifdef __cplusplus
extern "C" {
#endif
  
/**
 * Opaque type that is used as a handle for one instance of an opened DVD.
 */
typedef struct dvd_reader_s dvd_reader_t;
  
/**
 * Opaque type for a file read handle, much like a normal fd or FILE *.
 */
typedef struct dvd_file_s dvd_file_t;

/**
 * Returns the compiled version. (DVDREAD_VERSION as an int)
 */
int DVDVersion(void);


/**
 * Opens a block device of a DVD-ROM file, or an image file, or a directory
 * name for a mounted DVD or HD copy of a DVD.
 *
 * If the given file is a block device, or is the mountpoint for a block
 * device, then that device is used for CSS authentication using libdvdcss.
 * If no device is available, then no CSS authentication is performed, 
 * and we hope that the image is decrypted.
 *
 * If the path given is a directory, then the files in that directory may be
 * in any one of these formats:
 *
 *   path/VIDEO_TS/VTS_01_1.VOB
 *   path/video_ts/vts_01_1.vob
 *   path/VTS_01_1.VOB
 *   path/vts_01_1.vob
 *
 * @param path Specifies the the device, file or directory to be used. 
 * @return If successful a a read handle is returned. Otherwise 0 is returned.
 *
 * dvd = DVDOpen(path);
 *
 * Threads: this function uses chdir() and getcwd().
 * The current working directory is global to all threads,
 * so using chdir/getcwd in another thread could give unexpected results.
 */
dvd_reader_t *DVDOpen( const char * );

/**
 * Closes and cleans up the DVD reader object.
 *
 * You must close all open files before calling this function.
 *
 * @param dvd A read handle that should be closed.
 *
 * DVDClose(dvd);
 */
void DVDClose( dvd_reader_t * );

/**
 * Initializes libdvdread to be used with multithreading apps.
 *
 * You must call this function before using any other functions of libdvdread
 * if you are going to use libdvdread in multiple threads in your program.
 * If you are not using threads, or using libdvdread from just one thread,
 * you do not need to call this, but you are allowed to do so anyway.
 * 
 * There are several restrictions on how you can use libdvdread in
 * multithreading apps, see further documentation.
 *
 * If you have called DVDFinish() you need to call DVDInit again to use
 * libdvdread in multiple threads.
 *
 * DVDInit(void);
 */
void DVDInit(void);

/**
 * frees any dlopened objects.
 *
 * You must DVDClose all handles opened with DVDOpen before calling this.
 * Use this function if you need to close the dlopened libs and any other
 * objects that have been dynamically allocated by libdvdread.
 * 
 * DVDFinish(void);
 */
void DVDFinish(void);

/**
 * 
 */
typedef enum {
  DVD_READ_INFO_FILE,        /**< VIDEO_TS.IFO  or VTS_XX_0.IFO (title) */
  DVD_READ_INFO_BACKUP_FILE, /**< VIDEO_TS.BUP  or VTS_XX_0.BUP (title) */
  DVD_READ_MENU_VOBS,        /**< VIDEO_TS.VOB  or VTS_XX_0.VOB (title) */
  DVD_READ_TITLE_VOBS        /**< VTS_XX_[1-9].VOB (title).  All files in 
                                the title set are opened and read as a
                                single file. */
} dvd_read_domain_t;

/**
 *
 */
typedef struct {
  off_t size;          /**< Total size of file in bytes */
  int nr_parts;           /**< Number of file parts */
  off_t parts_size[9]; /**< Size of each part in bytes */
} dvd_stat_t;

/**
 * Stats a file on the DVD given the title number and domain.
 * The information about the file is stored in a dvd_stat_t
 * which contains information about the size of the file and
 * the number of parts in case of a multipart file and the respective
 * sizes of the parts.
 * A multipart file is for instance VTS_02_1.VOB, VTS_02_2.VOB, VTS_02_3.VOB
 * The size of VTS_02_1.VOB will be stored in stat->parts_size[0],
 * VTS_02_2.VOB in stat->parts_size[1], ...
 * The total size (sum of all parts) is stored in stat->size and
 * stat->nr_parts will hold the number of parts.
 * Only DVD_READ_TITLE_VOBS (VTS_??_[1-9].VOB) can be multipart files.
 * 
 * This function is only of use if you want to get the size of each file
 * in the filesystem. These sizes are not needed to use any other
 * functions in libdvdread. 
 *
 * @param dvd  A dvd read handle.
 * @param titlenum Which Video Title Set should be used, VIDEO_TS is 0.
 * @param domain Which domain. 
 * @param stat Pointer to where the result is stored.
 * @return If successful 0, otherwise -1.
 *
 * int DVDFileStat(dvd, titlenum, domain, stat);
 */
int DVDFileStat(dvd_reader_t *, int, dvd_read_domain_t, dvd_stat_t *);
  
/**
 * Opens a file on the DVD given the title number and domain.
 *
 * If the title number is 0, the video manager information is opened
 * (VIDEO_TS.[IFO,BUP,VOB]).  Returns a file structure which may be
 * used for reads, or 0 if the file was not found.
 *
 * @param dvd  A dvd read handle.
 * @param titlenum Which Video Title Set should be used, VIDEO_TS is 0.
 * @param domain Which domain. 
 * @return If successful a a file read handle is returned, otherwise 0.
 *
 * dvd_file = DVDOpenFile(dvd, titlenum, domain); */
dvd_file_t *DVDOpenFile( dvd_reader_t *, int, dvd_read_domain_t );

/**
 * Closes a file and frees the associated structure.
 *
 * @param dvd_file  The file read handle to be closed.
 *
 * DVDCloseFile(dvd_file);
 */
void DVDCloseFile( dvd_file_t * );

/**
 * Reads block_count number of blocks from the file at the given block offset.
 * Returns number of blocks read on success, -1 on error.  This call is only
 * for reading VOB data, and should not be used when reading the IFO files.  
 * When reading from an encrypted drive, blocks are decrypted using libdvdcss 
 * where required.
 *
 * @param dvd_file  A file read handle.
 * @param offset Block offset from the start of the file to start reading at.
 * @param block_count Number of block to read.
 * @param data Pointer to a buffer to write the data into.
 *             It must be aligned to the logical block size of the device when
 *             reading from a raw/O_DIRECT device (2048 bytes for DVD)
 * @return Returns number of blocks read on success, -1 on error.
 *
 * blocks_read = DVDReadBlocks(dvd_file, offset, block_count, data);
 */
ssize_t DVDReadBlocks( dvd_file_t *, int, size_t, unsigned char * );

/**
 * Seek to the given position in the file.  Returns the resulting position in
 * bytes from the beginning of the file.  The seek position is only used for
 * byte reads from the file, the block read call always reads from the given
 * offset.
 *
 * @param dvd_file  A file read handle.
 * @param seek_offset Byte offset from the start of the file to seek to.
 * @return The resulting position in bytes from the beginning of the file.
 *
 * offset_set = DVDFileSeek(dvd_file, seek_offset);
 */
int DVDFileSeek( dvd_file_t *, int );

/**
 * Reads the given number of bytes from the file.  This call can only be used
 * on the information files, and may not be used for reading from a VOB.  This
 * reads from and increments the currrent seek position for the file.
 *
 * @param dvd_file  A file read handle.
 * @param data Pointer to a buffer to write the data into.
 * @param bytes Number of bytes to read.
 * @return Returns number of bytes read on success, -1 on error.
 *
 * bytes_read = DVDReadBytes(dvd_file, data, bytes);
 */
ssize_t DVDReadBytes( dvd_file_t *, void *, size_t );
  
/**
 * Returns the file size in blocks.
 *
 * @param dvd_file  A file read handle.
 * @return The size of the file in blocks, -1 on error.
 *
 * blocks = DVDFileSize(dvd_file);
 */
ssize_t DVDFileSize( dvd_file_t * );
  
/**
 * Get a unique 128 bit disc ID.
 * This is the MD5 sum of VIDEO_TS.IFO and the VTS_0?_0.IFO files
 * in title order (those that exist).
 * If you need a 'text' representation of the id, print it as a
 * hexadecimal number, using lowercase letters, discid[0] first. 
 * I.e. the same format as the command-line 'md5sum' program uses.
 *
 * @param dvd A read handle to get the disc ID from
 * @param discid The buffer to put the disc ID into. The buffer must
 *               have room for 128 bits (16 chars).
 * @return 0 on success, -1 on error.
 */
int DVDDiscID( dvd_reader_t *, unsigned char * );

/**
 * Get the UDF VolumeIdentifier and VolumeSetIdentifier
 * from the PrimaryVolumeDescriptor.
 *
 * @param dvd A read handle to get the disc ID from
 * @param volid The buffer to put the VolumeIdentifier into.
 *              The VolumeIdentifier is latin-1 encoded (8bit unicode)
 *              null terminated and max 32 bytes (including '\0')
 * @param volid_size No more than volid_size bytes will be copied to volid.
 *                   If the VolumeIdentifier is truncated because of this
 *                   it will still be null terminated.
 * @param volsetid The buffer to put the VolumeSetIdentifier into.
 *                 The VolumeIdentifier is 128 bytes as
 *                 stored in the UDF PrimaryVolumeDescriptor.
 *                 Note that this is not a null terminated string.
 * @param volsetid_size At most volsetid_size bytes will be copied to volsetid.
 * @return 0 on success, -1 on error.
 */
int DVDUDFVolumeInfo( dvd_reader_t *, char *, unsigned int,
                      unsigned char *, unsigned int );

/**
 * Get the ISO9660 VolumeIdentifier and VolumeSetIdentifier
 *
 * * Only use this function as fallback if DVDUDFVolumeInfo returns 0   *
 * * this will happen on a disc mastered only with a iso9660 filesystem *
 * * All video DVD discs have UDF filesystem                            *
 *
 * @param dvd A read handle to get the disc ID from
 * @param volid The buffer to put the VolumeIdentifier into.
 *              The VolumeIdentifier is coded with '0-9','A-Z','_'
 *              null terminated and max 33 bytes (including '\0')
 * @param volid_size No more than volid_size bytes will be copied to volid.
 *                   If the VolumeIdentifier is truncated because of this
 *                   it will still be null terminated.
 * @param volsetid The buffer to put the VolumeSetIdentifier into.
 *                 The VolumeIdentifier is 128 bytes as
 *                 stored in the ISO9660 PrimaryVolumeDescriptor.
 *                 Note that this is not a null terminated string.
 * @param volsetid_size At most volsetid_size bytes will be copied to volsetid.
 * @return 0 on success, -1 on error.
 */
int DVDISOVolumeInfo( dvd_reader_t *, char *, unsigned int,
                      unsigned char *, unsigned int );

/**
 * Sets the level of caching that is done when reading from a device
 *
 * @param dvd A read handle to get the disc ID from
 * @param level The level of caching wanted.
 *             -1 - returns the current setting.
 *              0 - UDF Cache turned off.
 *              1 - (default level) Pointers to IFO files and some data from
 *                  PrimaryVolumeDescriptor are cached. 
 *
 * @return The level of caching.
 */
int DVDUDFCacheLevel( dvd_reader_t *, int );
  
#ifdef __cplusplus
};
#endif
#endif /* DVD_READER_H_INCLUDED */
