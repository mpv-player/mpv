/* ***************************************************************************
 **
 **  This file is part of the UniquE RAR File Library.
 **
 **  Copyright (C) 2000-2002 by Christian Scheurer (www.ChristianScheurer.ch)
 **  UNIX port copyright (c) 2000-2002 by Johannes Winkelmann (jw@tks6.net)
 **
 **  The contents of this file are subject to the UniquE RAR File Library
 **  License (the "unrarlib-license.txt"). You may not use this file except
 **  in compliance with the License. You may obtain a copy of the License
 **  at http://www.unrarlib.org/license.html.
 **  Software distributed under the License is distributed on an "AS IS"
 **  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied warranty.
 **
 **  Alternatively, the contents of this file may be used under the terms
 **  of the GNU General Public License Version 2 or later (the "GPL"), in
 **  which case the provisions of the GPL are applicable instead of those
 **  above. If you wish to allow use of your version of this file only
 **  under the terms of the GPL and not to allow others to use your version
 **  of this file under the terms of the UniquE RAR File Library License,
 **  indicate your decision by deleting the provisions above and replace
 **  them with the notice and other provisions required by the GPL. If you
 **  do not delete the provisions above, a recipient may use your version
 **  of this file under the terms of the GPL or the UniquE RAR File Library
 **  License.
 **
 ************************************************************************** */

/* include file for the "UniquE RAR File Library"  */
/* (C) 2000-2002 by Christian Scheurer aka. UniquE */
/* multi-OS version (Win32, Linux and SUN)         */

#ifndef __URARLIB_H
#define __URARLIB_H

#ifdef __cplusplus
extern "C"
{
#endif


/* ************************************************************************ */
/* ************************************************************************ */
/* **                                                                    ** */
/* **   CONFIGURATION of the UniquE RAR FileLib                          ** */
/* **   ==> you may change the setting for the lib HERE!                 ** */
/* **                                                                    ** */
/* ************************************************************************ */
/* ************************************************************************ */


#define _DEBUG_LOG                          /* generate debug messages      */

#define _DO_CRC32_CHECK                     /* perform cyclical redundancy  */
                                            /* check (CRC32) - disable this */
                                            /* for a little speed-up        */
/*#define _USE_ASM*/                            /*
                                             * enable assembly extensions
                                             * x86 cpus.
                                             */

/*#define _USE_MEMORY_TO_MEMORY_DECOMPRESSION*/ /* read file from memory or a   */
                                            /* resource instead of reading  */
                                            /* from a file. NOTE: you wont't*/
                                            /*  be able to decompress from  */
                                            /*  file if you enable this     */
                                            /*  option!                     */


#ifdef WIN32                                /* autodetect Win32 and Linux   */
#define _WIN_32                             /* Win32 with VisualC           */
#define _DEBUG_LOG_FILE "C:\\temp\\debug_unrar.txt" /* log file path        */
#else
#define _UNIX                               /* Linux or Unix with GCC       */
#define _DEBUG_LOG_FILE "/tmp/debug_unrar.txt" /* log file path             */
/*#define NON_INTEL_BYTE_ORDER*/               /* GCC on motorola systems    */

#endif

/* ------------------------------------------------------------------------ */



/* -- global type definitions --------------------------------------------- */

#ifdef NON_INTEL_BYTE_ORDER
#ifdef _USE_ASM
#warning Disabling assembly because NON_INTEL_BYTE_ORDER is set
#undef _USE_ASM
#endif
#endif

#ifdef _WIN_32
typedef unsigned char    UBYTE;             /* WIN32 definitions            */
typedef unsigned short   UWORD;
typedef unsigned long    UDWORD;
#endif

#ifdef _UNIX                                /* LINUX/UNIX definitions       */
typedef unsigned char    UBYTE;
typedef unsigned short   UWORD;
typedef unsigned long    UDWORD;
#endif


/* This structure is used for listing archive content                       */
struct RAR20_archive_entry                  /* These infos about files are  */
{                                           /* stored in RAR v2.0 archives  */
  char   *Name;
  UWORD  NameSize;
  UDWORD PackSize;
  UDWORD UnpSize;
  UBYTE  HostOS;                            /* MSDOS=0,OS2=1,WIN32=2,UNIX=3 */
  UDWORD FileCRC;
  UDWORD FileTime;
  UBYTE  UnpVer;
  UBYTE  Method;
  UDWORD FileAttr;
};

typedef struct  archivelist                 /* used to list archives        */
{
  struct RAR20_archive_entry item;
  struct archivelist         *next;
} ArchiveList_struct;


#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
typedef struct  memory_file                 /* used to decompress files in  */
{                                           /* memory                       */
  void                       *data;         /* pointer to the file data     */
  unsigned long              size;          /* total size of the file data  */
  unsigned long              offset;        /* offset within "memory-file"  */
} MemoryFile;
#endif

/* -- global functions ---------------------------------------------------- */

/* urarlib_get:
 * decompresses and decrypt data from a RAR file to a buffer in system memory.
 *
 *   input: *output         pointer to an empty char*. This pointer will show
 *                          to the extracted data
 *          *size           shows where to write the size of the decompressed
 *                          file
 *                          (**NOTE: URARLib _does_ memory allocation etc.!**)
 *          *filename       pointer to string containing the file to decompress
 *          *rarfile        pointer to a string with the full name and path of
 *                          the RAR file or pointer to a RAR file in memory if
 *                          memory-to-memory decompression is active.
 *          *libpassword    pointer to a string with the password used to
 *                          en-/decrypt the RAR
 *   output: int            returns TRUE on success or FALSE on error
 *                          (FALSE=0, TRUE=1)
 */

extern int urarlib_get(void  *output,
                       unsigned long *size,
                       char *filename,
                       void *rarfile,
                       char *libpassword);



/* urarlib_list:
 * list the content of a RAR archive.
 *
 *   input: *rarfile        pointer to a string with the full name and path of
 *                          the RAR file or pointer to a RAR file in memory if
 *                          memory-to-memory decompression is active.
 *          *list           pointer to an ArchiveList_struct that can be
 *                          filled with details about the archive
 *                          to the extracted data
 *   output: int            number of files/directories within archive
 */

extern int urarlib_list(void *rarfile, ArchiveList_struct *list);


/* urarlib_freelist:
 * (after the suggestion and code of Duy Nguyen, Sean O'Blarney
 * and Johannes Winkelmann who independently wrote a patch)
 * free the memory of a ArchiveList_struct created by urarlib_list.
 *
 *    input: *list          pointer to an ArchiveList_struct
 *    output: -
 */

extern void urarlib_freelist(ArchiveList_struct *list);

/* ------------------------------------------------------------------------ */



#ifdef __cplusplus
};
#endif

#endif

