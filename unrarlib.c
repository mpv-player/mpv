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

/* ***************************************************************************
 **
 **                           UniquE RAR File Library
 **                     The free file lib for the demoscene
 **                   multi-OS version (Win32, Linux and SunOS)
 **
 *****************************************************************************
 **
 **   ==> Please configure the program in "unrarlib.h". <==
 **
 **   RAR decompression code:
 **    (C) Eugene Roshal
 **   Modifications to a FileLib:
 **    (C) 2000-2002 Christian Scheurer aka. UniquE/Vantage (cs@unrarlib.org)
 **   Linux port:
 **    (C) 2000-2002 Johannes Winkelmann (jw@tks6.net)
 **
 **  The UniquE RAR File Library gives you the ability to access RAR archives
 **  (any compression method supported in RAR v2.0 including Multimedia
 **  Compression and encryption) directly from your program with ease an by
 **  adding only 12kB (6kB UPX-compressed) additional code to your program.
 **  Both solid and normal (recommended for fast random access to the files!)
 **  archives are supported. This FileLib is made for the Demo scene, so it's
 **  designed for easy use within your demos and intros.
 **  Please read "licence.txt" to learn more about how you may use URARFileLib
 **  in your productions.
 **
 *****************************************************************************
 **
 **  ==> see the "CHANGES" file to see what's new
 **
 ************************************************************************** */

/* -- include files ------------------------------------------------------- */
#include "unrarlib.h"                       /* include global configuration */
/* ------------------------------------------------------------------------ */



/* -- global stuff -------------------------------------------------------- */
#ifdef _WIN_32

#include <windows.h>                        /* WIN32 definitions            */
#include <stdio.h>
#include <string.h>


#define ENABLE_ACCESS

#define HOST_OS     WIN_32

#define FM_NORMAL   0x00
#define FM_RDONLY   0x01
#define FM_HIDDEN   0x02
#define FM_SYSTEM   0x04
#define FM_LABEL    0x08
#define FM_DIREC    0x10
#define FM_ARCH     0x20

#define PATHDIVIDER  "\\"
#define CPATHDIVIDER '\\'
#define MASKALL      "*.*"

#define READBINARY   "rb"
#define READTEXT     "rt"
#define UPDATEBINARY "r+b"
#define CREATEBINARY "w+b"
#define CREATETEXT   "w"
#define APPENDTEXT   "at"

#endif

#ifdef _UNIX

#include <stdio.h>                          /* LINUX/UNIX definitions       */
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define ENABLE_ACCESS

#define HOST_OS     UNIX

#define FM_LABEL    0x0000
#define FM_DIREC    0x4000

#define PATHDIVIDER  "/"
#define CPATHDIVIDER '/'
#define MASKALL      "*.*"

#define READBINARY   "r"
#define READTEXT     "r"
#define UPDATEBINARY "r+"
#define CREATEBINARY "w+"
#define CREATETEXT   "w"
#define APPENDTEXT   "a"


/* emulation of the windows API and data types                              */
/* 20-08-2000 Johannes Winkelmann, jw@tks6.net                              */

typedef long    DWORD;
typedef short   BOOL;
#define TRUE    1
#define FALSE   0


#ifdef _DEBUG_LOG                           /* define macros for debugging  */
#include <unistd.h>
#include <sys/time.h>

DWORD GetTickCount()
{
    struct timeval tv;
    gettimeofday( &tv, 0 );
    return (tv.tv_usec / 1000);
}
#endif

#endif





#ifdef _DEBUG_LOG                           /* define macros for debugging  */

BOOL debug_log_first_start = TRUE;

#define debug_log(a); debug_log_proc(a, __FILE__, __LINE__);
#define debug_init(a); debug_init_proc(a);

void debug_log_proc(char *text, char *sourcefile, int sourceline);
void debug_init_proc(char *file_name);

#else
#define debug_log(a);   /* no debug this time */
#define debug_init(a);  /* no debug this time */
#endif





#define MAXWINSIZE      0x100000
#define MAXWINMASK      (MAXWINSIZE-1)
#define UNP_MEMORY      MAXWINSIZE
#define Min(x,y) (((x)<(y)) ? (x):(y))
#define Max(x,y) (((x)>(y)) ? (x):(y))
#define NM  260

#define SIZEOF_MARKHEAD         7
#define SIZEOF_OLDMHD           7
#define SIZEOF_NEWMHD          13
#define SIZEOF_OLDLHD          21
#define SIZEOF_NEWLHD          32
#define SIZEOF_SHORTBLOCKHEAD   7
#define SIZEOF_LONGBLOCKHEAD   11
#define SIZEOF_COMMHEAD        13
#define SIZEOF_PROTECTHEAD     26


#define PACK_VER       20                   /* version of decompression code*/
#define UNP_VER        20
#define PROTECT_VER    20


enum { M_DENYREAD,M_DENYWRITE,M_DENYNONE,M_DENYALL };
enum { FILE_EMPTY,FILE_ADD,FILE_UPDATE,FILE_COPYOLD,FILE_COPYBLOCK };
enum { SUCCESS,WARNING,FATAL_ERROR,CRC_ERROR,LOCK_ERROR,WRITE_ERROR,
       OPEN_ERROR,USER_ERROR,MEMORY_ERROR,USER_BREAK=255,IMM_ABORT=0x8000 };
enum { EN_LOCK=1,EN_VOL=2 };
enum { SD_MEMORY=1,SD_FILES=2 };
enum { NAMES_DONTCHANGE };
enum { LOG_ARC=1,LOG_FILE=2 };
enum { OLD_DECODE=0,OLD_ENCODE=1,NEW_CRYPT=2 };
enum { OLD_UNPACK,NEW_UNPACK };


#define MHD_COMMENT        2
#define MHD_LOCK           4
#define MHD_PACK_COMMENT   16
#define MHD_AV             32
#define MHD_PROTECT        64

#define LHD_SPLIT_BEFORE   1
#define LHD_SPLIT_AFTER    2
#define LHD_PASSWORD       4
#define LHD_COMMENT        8
#define LHD_SOLID          16

#define LHD_WINDOWMASK     0x00e0
#define LHD_WINDOW64       0
#define LHD_WINDOW128      32
#define LHD_WINDOW256      64
#define LHD_WINDOW512      96
#define LHD_WINDOW1024     128
#define LHD_DIRECTORY      0x00e0

#define LONG_BLOCK         0x8000
#define READSUBBLOCK       0x8000

enum { ALL_HEAD=0,MARK_HEAD=0x72,MAIN_HEAD=0x73,FILE_HEAD=0x74,
       COMM_HEAD=0x75,AV_HEAD=0x76,SUB_HEAD=0x77,PROTECT_HEAD=0x78};
enum { EA_HEAD=0x100 };
enum { MS_DOS=0,OS2=1,WIN_32=2,UNIX=3 };


struct MarkHeader
{
  UBYTE Mark[7];
};


struct NewMainArchiveHeader
{
  UWORD HeadCRC;
  UBYTE HeadType;
  UWORD Flags;
  UWORD HeadSize;
  UWORD Reserved;
  UDWORD Reserved1;
};


struct NewFileHeader
{
  UWORD HeadCRC;
  UBYTE HeadType;
  UWORD Flags;
  UWORD HeadSize;
  UDWORD PackSize;
  UDWORD UnpSize;
  UBYTE HostOS;
  UDWORD FileCRC;
  UDWORD FileTime;
  UBYTE UnpVer;
  UBYTE Method;
  UWORD NameSize;
  UDWORD FileAttr;
};


struct BlockHeader
{
  UWORD HeadCRC;
  UBYTE HeadType;
  UWORD Flags;
  UWORD HeadSize;
  UDWORD DataSize;
};


struct Decode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[2];
};


struct MarkHeader MarkHead;
struct NewMainArchiveHeader NewMhd;
struct NewFileHeader NewLhd;
struct BlockHeader BlockHead;

UBYTE *TempMemory;                          /* temporary unpack-buffer      */
char *CommMemory;


UBYTE *UnpMemory;
char ArgName[NM];                           /* current file in rar archive  */
char ArcFileName[NM];                       /* file to decompress           */

#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION  /* mem-to-mem decompression     */
  MemoryFile *MemRARFile;                   /* pointer to RAR file in memory*/
#else
  char ArcName[255];                        /* RAR archive name             */
  FILE *ArcPtr;                             /* input RAR file handler       */
#endif
char Password[255];                         /* password to decrypt files    */

unsigned char *temp_output_buffer;          /* extract files to this pointer*/
unsigned long *temp_output_buffer_offset;   /* size of temp. extract buffer */

BOOL FileFound;                             /* TRUE=use current extracted   */
                                            /* data FALSE=throw data away,  */
                                            /* wrong file                   */
int MainHeadSize;
long CurBlockPos,NextBlockPos;

unsigned long CurUnpRead, CurUnpWrite;
long UnpPackedSize;
long DestUnpSize;

UDWORD HeaderCRC;
int Encryption;

unsigned int UnpWrSize;
unsigned char *UnpWrAddr;
unsigned int UnpPtr,WrPtr;

unsigned char PN1,PN2,PN3;
unsigned short OldKey[4];



/* function header definitions                                              */
int ReadHeader(int BlockType);
BOOL ExtrFile(void);
BOOL ListFile(void);
int tread(void *stream,void *buf,unsigned len);
int tseek(void *stream,long offset,int fromwhere);
BOOL UnstoreFile(void);
int IsArchive(void);
int ReadBlock(int BlockType);
unsigned int UnpRead(unsigned char *Addr,unsigned int Count);
void UnpInitData(void);
void Unpack(unsigned char *UnpAddr);
UBYTE DecodeAudio(int Delta);
static void DecodeNumber(struct Decode *Dec);
void UpdKeys(UBYTE *Buf);
void SetCryptKeys(char *Password);
void SetOldKeys(char *Password);
void DecryptBlock(unsigned char *Buf);
void InitCRC(void);
UDWORD CalcCRC32(UDWORD StartCRC,UBYTE *Addr,UDWORD Size);
void UnpReadBuf(int FirstBuf);
void ReadTables(void);
static void ReadLastTables(void);
static void MakeDecodeTables(unsigned char *LenTab,
                             struct Decode *Dec,
                             int Size);
int stricomp(char *Str1,char *Str2);
/* ------------------------------------------------------------------------ */


/* -- global functions ---------------------------------------------------- */

int urarlib_get(void *output,
                unsigned long *size,
                char *filename,
                void *rarfile,
                char *libpassword)
/* Get a file from a RAR file to the "output" buffer. The UniquE RAR FileLib
 * does everything from allocating memory, decrypting and unpacking the file
 * from the archive. TRUE is returned if the file could be successfully
 * extracted, else a FALSE indicates a failure.
 */
{
  BOOL  retcode = FALSE;

#ifdef _DEBUG_LOG
  int  str_offs;                            /* used for debug-strings       */
  char DebugMsg[500];                       /* used to compose debug msg    */

  if(debug_log_first_start)
  {
    debug_log_first_start=FALSE;            /* only create a new log file   */
    debug_init(_DEBUG_LOG_FILE);            /* on startup                   */
  }

#endif

  InitCRC();                                /* init some vars               */

  strcpy(ArgName, filename);                /* set file(s) to extract       */
#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
  MemRARFile = rarfile;                     /* set pointer to mem-RAR file  */
#else
  strcpy(ArcName, rarfile);                 /* set RAR file name            */
#endif
  if(libpassword != NULL)
    strcpy(Password, libpassword);          /* init password                */

  temp_output_buffer = NULL;
  temp_output_buffer_offset=size;           /* set size of the temp buffer  */

#ifdef _DEBUG_LOG
  sprintf(DebugMsg, "Extracting >%s< from >%s< (password is >%s<)...",
          filename, (char*)rarfile, libpassword);
  debug_log(DebugMsg);
#endif

  retcode = ExtrFile();                     /* unpack file now!             */

  memset(Password,0,sizeof(Password));      /* clear password               */

#ifndef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
  if (ArcPtr!=NULL){
      fclose(ArcPtr);
      ArcPtr = NULL;
  }
#endif

  free(UnpMemory);                          /* free memory                  */
  free(TempMemory);
  free(CommMemory);
  UnpMemory=NULL;
  TempMemory=NULL;
  CommMemory=NULL;


  if(retcode == FALSE)
  {
    free(temp_output_buffer);               /* free memory and return NULL  */
    temp_output_buffer=NULL;
    *(DWORD*)output=0;                      /* pointer on errors            */
    *size=0;
#ifdef _DEBUG_LOG


   /* sorry for this ugly code, but older SunOS gcc compilers don't support */
   /* white spaces within strings                                           */
   str_offs  = sprintf(DebugMsg, "Error - couldn't extract ");
   str_offs += sprintf(DebugMsg + str_offs, ">%s<", filename);
   str_offs += sprintf(DebugMsg + str_offs, " and allocated ");
   str_offs += sprintf(DebugMsg + str_offs, "%u Bytes", (unsigned int)*size);
   str_offs += sprintf(DebugMsg + str_offs, " of unused memory!");

  } else
  {
    sprintf(DebugMsg, "Extracted %u Bytes.", (unsigned int)*size);
  }
  debug_log(DebugMsg);
#else
  }
#endif
  *(DWORD*)output=(DWORD)temp_output_buffer;/* return pointer for unpacked*/
                                            /* data                       */

  return retcode;
}


int urarlib_list(void *rarfile, ArchiveList_struct *list)
{
  ArchiveList_struct *tmp_List = NULL;
  int NoOfFilesInArchive       = 0;         /* number of files in archive   */

#ifdef _DEBUG_LOG
  if(debug_log_first_start)
  {
    debug_log_first_start=FALSE;            /* only create a new log file   */
    debug_init(_DEBUG_LOG_FILE);            /* on startup                   */
  }
#endif

  InitCRC();                                /* init some vars               */

#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
  MemRARFile         = rarfile;             /* assign pointer to RAR file   */
  MemRARFile->offset = 0;
  if (!IsArchive())
  {
    debug_log("Not a RAR file");
    return NoOfFilesInArchive;              /* error => exit!               */
  }
#else
  /* open and identify archive                                              */
  if ((ArcPtr=fopen(rarfile,READBINARY))!=NULL)
  {
    if (!IsArchive())
    {
      debug_log("Not a RAR file");
      fclose(ArcPtr);
      ArcPtr = NULL;
      return NoOfFilesInArchive;            /* error => exit!               */
    }
  }
  else {
    debug_log("Error opening file.");
    return NoOfFilesInArchive;
  }
#endif

  if ((UnpMemory=malloc(UNP_MEMORY))==NULL)
  {
    debug_log("Can't allocate memory for decompression!");
    return NoOfFilesInArchive;
  }

#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
  MemRARFile->offset+=NewMhd.HeadSize-MainHeadSize;
#else
  tseek(ArcPtr,NewMhd.HeadSize-MainHeadSize,SEEK_CUR);
#endif
  (*(DWORD*)list) = (DWORD)NULL;            /* init file list               */
  /* do while file is not extracted and there's no error                    */
  while (TRUE)
  {
    if (ReadBlock(FILE_HEAD | READSUBBLOCK) <= 0) /* read name of the next  */
    {                                       /* file within the RAR archive  */
      debug_log("Couldn't read next filename from archive (I/O error).");
      break;                                /* error, file not found in     */
    }                                       /* archive or I/O error         */
    if (BlockHead.HeadType==SUB_HEAD)
    {
      debug_log("Sorry, sub-headers not supported.");
      break;                                /* error => exit                */
    }

    if((void*)(*(DWORD*)list) == NULL)      /* first entry                  */
    {
      tmp_List = malloc(sizeof(ArchiveList_struct));
      tmp_List->next = NULL;

      (*(DWORD*)list) = (DWORD)tmp_List;

    } else                                  /* add entry                    */
    {
      tmp_List->next = malloc(sizeof(ArchiveList_struct));
      tmp_List = (ArchiveList_struct*) tmp_List->next;
      tmp_List->next = NULL;
    }

    tmp_List->item.Name = malloc(NewLhd.NameSize + 1);
    strcpy(tmp_List->item.Name, ArcFileName);
    tmp_List->item.NameSize = NewLhd.NameSize;
    tmp_List->item.PackSize = NewLhd.PackSize;
    tmp_List->item.UnpSize = NewLhd.UnpSize;
    tmp_List->item.HostOS = NewLhd.HostOS;
    tmp_List->item.FileCRC = NewLhd.FileCRC;
    tmp_List->item.FileTime = NewLhd.FileTime;
    tmp_List->item.UnpVer = NewLhd.UnpVer;
    tmp_List->item.Method = NewLhd.Method;
    tmp_List->item.FileAttr = NewLhd.FileAttr;

    NoOfFilesInArchive++;                   /* count files                  */

#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
    MemRARFile->offset = NextBlockPos;
#else
    if (ArcPtr!=NULL) tseek(ArcPtr,NextBlockPos,SEEK_SET);
#endif

  };

  /* free memory, clear password and close archive                          */
  memset(Password,0,sizeof(Password));      /* clear password               */
#ifndef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
  if (ArcPtr!=NULL){
      fclose(ArcPtr);
      ArcPtr = NULL;
  }
#endif

  free(UnpMemory);                          /* free memory                  */
  free(TempMemory);
  free(CommMemory);
  UnpMemory=NULL;
  TempMemory=NULL;
  CommMemory=NULL;

  return NoOfFilesInArchive;
}



/* urarlib_freelist:
 * (after the suggestion and code of Duy Nguyen, Sean O'Blarney
 * and Johannes Winkelmann who independently wrote a patch)
 * free the memory of a ArchiveList_struct created by urarlib_list.
 *
 *    input: *list          pointer to an ArchiveList_struct
 *    output: -
 */

void urarlib_freelist(ArchiveList_struct *list)
{
    ArchiveList_struct* tmp = list;

    while ( list ) {
        tmp = list->next;
        free( list->item.Name );
        free( list );
        list = tmp;
    }
}


/* ------------------------------------------------------------------------ */

















/****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 *******                    B L O C K   I / O                         *******
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************/



#define GetHeaderByte(N) Header[N]

#define GetHeaderWord(N) (Header[N]+((UWORD)Header[N+1]<<8))

#define GetHeaderDword(N) (Header[N]+((UWORD)Header[N+1]<<8)+\
                          ((UDWORD)Header[N+2]<<16)+\
                          ((UDWORD)Header[N+3]<<24))


int ReadBlock(int BlockType)
{
  struct NewFileHeader SaveFileHead;
  int Size=0,ReadSubBlock=0;
  static int LastBlock;
  memcpy(&SaveFileHead,&NewLhd,sizeof(SaveFileHead));
  if (BlockType & READSUBBLOCK)
    ReadSubBlock=1;
  BlockType &= 0xff;
  {
    while (1)
    {
#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
      CurBlockPos=MemRARFile->offset;       /* get offset of mem-file       */
#else
      CurBlockPos=ftell(ArcPtr);
#endif
      Size=ReadHeader(FILE_HEAD);
      if (Size!=0)
      {
        if (NewLhd.HeadSize<SIZEOF_SHORTBLOCKHEAD)
          return(0);
        NextBlockPos=CurBlockPos+NewLhd.HeadSize;
        if (NewLhd.Flags & LONG_BLOCK)
          NextBlockPos+=NewLhd.PackSize;
        if (NextBlockPos<=CurBlockPos)
          return(0);
      }

      if (Size > 0 && BlockType!=SUB_HEAD)
        LastBlock=BlockType;
      if (Size==0 || BlockType==ALL_HEAD || NewLhd.HeadType==BlockType ||
          (NewLhd.HeadType==SUB_HEAD && ReadSubBlock && LastBlock==BlockType))
        break;
#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
      MemRARFile->offset = NextBlockPos;
#else
      tseek(ArcPtr, NextBlockPos, SEEK_SET);
#endif
    }
  }

  BlockHead.HeadCRC=NewLhd.HeadCRC;
  BlockHead.HeadType=NewLhd.HeadType;
  BlockHead.Flags=NewLhd.Flags;
  BlockHead.HeadSize=NewLhd.HeadSize;
  BlockHead.DataSize=NewLhd.PackSize;

  if (BlockType!=NewLhd.HeadType) BlockType=ALL_HEAD;

  if((FILE_HEAD == BlockType) && (Size>0))
  {
    NewLhd.NameSize=Min(NewLhd.NameSize,sizeof(ArcFileName)-1);
#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
    tread(MemRARFile, ArcFileName, NewLhd.NameSize);
#else
    tread(ArcPtr,ArcFileName,NewLhd.NameSize);
#endif
    ArcFileName[NewLhd.NameSize]=0;
#ifdef _DEBUG_LOG
    if (NewLhd.HeadCRC!=(UWORD)~CalcCRC32(HeaderCRC,(UBYTE*)&ArcFileName[0],
                                          NewLhd.NameSize))
    {
      debug_log("file header broken");
    }
#endif
    Size+=NewLhd.NameSize;
  } else
  {
    memcpy(&NewLhd,&SaveFileHead,sizeof(NewLhd));
#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
    MemRARFile->offset = CurBlockPos;
#else
    tseek(ArcPtr,CurBlockPos,SEEK_SET);
#endif
  }


  return(Size);
}


int ReadHeader(int BlockType)
{
  int Size = 0;
  unsigned char Header[64];
  switch(BlockType)
  {
    case MAIN_HEAD:
#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
        Size=tread(MemRARFile, Header, SIZEOF_NEWMHD);
#else
        Size=tread(ArcPtr,Header,SIZEOF_NEWMHD);
#endif
        NewMhd.HeadCRC=(unsigned short)GetHeaderWord(0);
        NewMhd.HeadType=GetHeaderByte(2);
        NewMhd.Flags=(unsigned short)GetHeaderWord(3);
        NewMhd.HeadSize=(unsigned short)GetHeaderWord(5);
        NewMhd.Reserved=(unsigned short)GetHeaderWord(7);
        NewMhd.Reserved1=GetHeaderDword(9);
        HeaderCRC=CalcCRC32(0xFFFFFFFFL,&Header[2],SIZEOF_NEWMHD-2);
      break;
    case FILE_HEAD:
#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
        Size=tread(MemRARFile, Header, SIZEOF_NEWLHD);
#else
        Size=tread(ArcPtr,Header,SIZEOF_NEWLHD);
#endif
        NewLhd.HeadCRC=(unsigned short)GetHeaderWord(0);
        NewLhd.HeadType=GetHeaderByte(2);
        NewLhd.Flags=(unsigned short)GetHeaderWord(3);
        NewLhd.HeadSize=(unsigned short)GetHeaderWord(5);
        NewLhd.PackSize=GetHeaderDword(7);
        NewLhd.UnpSize=GetHeaderDword(11);
        NewLhd.HostOS=GetHeaderByte(15);
        NewLhd.FileCRC=GetHeaderDword(16);
        NewLhd.FileTime=GetHeaderDword(20);
        NewLhd.UnpVer=GetHeaderByte(24);
        NewLhd.Method=GetHeaderByte(25);
        NewLhd.NameSize=(unsigned short)GetHeaderWord(26);
        NewLhd.FileAttr=GetHeaderDword(28);
        HeaderCRC=CalcCRC32(0xFFFFFFFFL,&Header[2],SIZEOF_NEWLHD-2);
      break;

#ifdef _DEBUG_LOG
  case COMM_HEAD:                           /* log errors in case of debug  */
        debug_log("Comment headers not supported! "\
                  "Please create archives without comments.");
      break;
  case PROTECT_HEAD:
        debug_log("Protected headers not supported!");
      break;
  case ALL_HEAD:
        debug_log("ShortBlockHeader not supported!");
      break;
  default:
        debug_log("Unknown//unsupported !");
#else
  default:                                  /* else do nothing              */
        break;
#endif
  }
  return(Size);
}

/* **************************************************************************
 ****************************************************************************
 ****************************************************************************
 ************************************************************************** */

















/* **************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 *******                  E X T R A C T   L O O P                     *******
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ************************************************************************** */


int IsArchive(void)
{
#ifdef _DEBUG_LOG
  int  str_offs;                            /* used for debug-strings       */
  char DebugMsg[500];                       /* used to compose debug msg    */
#endif

#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
  if (tread(MemRARFile, MarkHead.Mark, SIZEOF_MARKHEAD) != SIZEOF_MARKHEAD)
    return(FALSE);
#else
  if (tread(ArcPtr,MarkHead.Mark,SIZEOF_MARKHEAD)!=SIZEOF_MARKHEAD)
    return(FALSE);
#endif
  /* Old archive => error                                                   */
  if (MarkHead.Mark[0]==0x52 && MarkHead.Mark[1]==0x45 &&
      MarkHead.Mark[2]==0x7e && MarkHead.Mark[3]==0x5e)
  {
    debug_log("Attention: format as OLD detected! Can't handel archive!");
  }
  else
      /* original RAR v2.0                                                  */
      if ((MarkHead.Mark[0]==0x52 && MarkHead.Mark[1]==0x61 && /* original  */
           MarkHead.Mark[2]==0x72 && MarkHead.Mark[3]==0x21 && /* RAR header*/
           MarkHead.Mark[4]==0x1a && MarkHead.Mark[5]==0x07 &&
           MarkHead.Mark[6]==0x00) ||
     /* "UniquE!" - header                                                  */
          (MarkHead.Mark[0]=='U' && MarkHead.Mark[1]=='n' &&   /* "UniquE!" */
           MarkHead.Mark[2]=='i' && MarkHead.Mark[3]=='q' &&   /* header    */
           MarkHead.Mark[4]=='u' && MarkHead.Mark[5]=='E' &&
           MarkHead.Mark[6]=='!'))

    {
      if (ReadHeader(MAIN_HEAD)!=SIZEOF_NEWMHD)
        return(FALSE);
    } else
    {

#ifdef _DEBUG_LOG
     /* sorry for this ugly code, but older SunOS gcc compilers don't       */
     /* support white spaces within strings                                 */
     str_offs  = sprintf(DebugMsg, "unknown archive type (only plain RAR ");
     str_offs += sprintf(DebugMsg + str_offs, "supported (normal and solid ");
     str_offs += sprintf(DebugMsg + str_offs, "archives), SFX and Volumes ");
     str_offs += sprintf(DebugMsg + str_offs, "are NOT supported!)");

     debug_log(DebugMsg);
#endif

    }


  MainHeadSize=SIZEOF_NEWMHD;

  return(TRUE);
}


BOOL ExtrFile(void)
{
  BOOL ReturnCode=TRUE;
  FileFound=FALSE;                          /* no file found by default     */

#ifdef  _USE_MEMORY_TO_MEMORY_DECOMPRESSION
  MemRARFile->offset = 0;                   /* start reading from offset 0  */
  if (!IsArchive())
  {
    debug_log("Not a RAR file");
    return FALSE;                           /* error => exit!               */
  }

#else
  /* open and identify archive                                              */
  if ((ArcPtr=fopen(ArcName,READBINARY))!=NULL)
  {
    if (!IsArchive())
    {
      debug_log("Not a RAR file");
      fclose(ArcPtr);
      ArcPtr = NULL;
      return FALSE;                         /* error => exit!               */
    }
  } else
  {
    debug_log("Error opening file.");
    return FALSE;
  }
#endif


  if ((UnpMemory=malloc(UNP_MEMORY))==NULL)
  {
    debug_log("Can't allocate memory for decompression!");
    return FALSE;
  }

#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
  MemRARFile->offset+=NewMhd.HeadSize-MainHeadSize;
#else
  tseek(ArcPtr,NewMhd.HeadSize-MainHeadSize,SEEK_CUR);
#endif

  /* do while file is not extracted and there's no error                    */
  do
  {

    if (ReadBlock(FILE_HEAD | READSUBBLOCK) <= 0) /* read name of the next  */
    {                                       /* file within the RAR archive  */
/*
 *
 * 21.11.2000  UnQ  There's a problem with some linux distros when a file
 *                  can not be found in an archive.
 *
 *    debug_log("Couldn't read next filename from archive (I/O error).");
 *
*/
      ReturnCode=FALSE;
      break;                                /* error, file not found in     */
    }                                       /* archive or I/O error         */
    if (BlockHead.HeadType==SUB_HEAD)
    {
      debug_log("Sorry, sub-headers not supported.");
      ReturnCode=FALSE;
      break;                                /* error => exit                */
    }


    if(TRUE == (FileFound=(stricomp(ArgName, ArcFileName) == 0)))
    /* *** file found! ***                                                  */
    {
      {
        temp_output_buffer=malloc(NewLhd.UnpSize);/* allocate memory for the*/
      }
      *temp_output_buffer_offset=0;         /* file. The default offset     */
                                            /* within the buffer is 0       */

      if(temp_output_buffer == NULL)
      {
        debug_log("can't allocate memory for the file decompression");
        ReturnCode=FALSE;
        break;                              /* error, can't extract file!   */
      }


    }

    /* in case of a solid archive, we need to decompress any single file till
     * we have found the one we are looking for. In case of normal archives
     * (recommended!!), we skip the files until we are sure that it is the
     * one we want.
     */
    if((NewMhd.Flags & 0x08) || FileFound)
    {
      if (NewLhd.UnpVer<13 || NewLhd.UnpVer>UNP_VER)
      {
        debug_log("unknown compression method");
        ReturnCode=FALSE;
        break;                              /* error, can't extract file!   */
      }

      CurUnpRead=CurUnpWrite=0;
      if ((*Password!=0) && (NewLhd.Flags & LHD_PASSWORD))
        Encryption=NewLhd.UnpVer;
      else
        Encryption=0;
      if (Encryption) SetCryptKeys(Password);

      UnpPackedSize=NewLhd.PackSize;
      DestUnpSize=NewLhd.UnpSize;

      if (NewLhd.Method==0x30)
      {
        UnstoreFile();
      } else
      {
        Unpack(UnpMemory);
      }



#ifdef _DO_CRC32_CHECK                      /* calculate CRC32              */
      if((UBYTE*)temp_output_buffer != NULL)
      {
        if(NewLhd.FileCRC!=~CalcCRC32(0xFFFFFFFFL,
                                      (UBYTE*)temp_output_buffer,
                                      NewLhd.UnpSize))
        {
          debug_log("CRC32 error - file couldn't be decompressed correctly!");
          ReturnCode=FALSE;
          break;                              /* error, can't extract file! */
        }
      }
#endif

    }

#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
    MemRARFile->offset = NextBlockPos;
#else
    if (ArcPtr!=NULL) tseek(ArcPtr,NextBlockPos,SEEK_SET);
#endif
  } while(stricomp(ArgName, ArcFileName) != 0);/* exit if file is extracted */

  /* free memory, clear password and close archive                          */
  free(UnpMemory);
  UnpMemory=NULL;
#ifndef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
  if (ArcPtr!=NULL){
      fclose(ArcPtr);
      ArcPtr = NULL;
  }
#endif

  return ReturnCode;                        /* file extracted successful!   */
}

/* **************************************************************************
 ****************************************************************************
 ****************************************************************************
 ************************************************************************** */


















/* **************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 *******             G L O B A L   F U N C T I O N S                  *******
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ************************************************************************** */


int tread(void *stream,void *buf,unsigned len)
{
#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION

  if(((MemRARFile->offset + len) > MemRARFile->size) || (len == 0))
     return 0;

  memcpy(buf,
         (BYTE*)(((MemoryFile*)stream)->data)+((MemoryFile*)stream)->offset,
         len % ((((MemoryFile*)stream)->size) - 1));

  MemRARFile->offset+=len;                  /* update read pointer          */
  return len % ((((MemoryFile*)stream)->size) - 1);
#else
  return(fread(buf,1,len,(FILE*)stream));
#endif
}


#ifndef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
int tseek(void *stream,long offset,int fromwhere)
{
  return(fseek((FILE*)stream,offset,fromwhere));
}
#endif


char* strupper(char *Str)
{
  char *ChPtr;
  for (ChPtr=Str;*ChPtr;ChPtr++)
    *ChPtr=(char)toupper(*ChPtr);
  return(Str);
}


int stricomp(char *Str1,char *Str2)
/* compare strings without regard of '\' and '/'                            */
{
  char S1[512],S2[512];
  char *chptr;

  strncpy(S1,Str1,sizeof(S1));
  strncpy(S2,Str2,sizeof(S2));

  while((chptr = strchr(S1, '\\')) != NULL) /* ignore backslash             */
  {
    *chptr = '_';
  }

  while((chptr = strchr(S2, '\\')) != NULL) /* ignore backslash             */
  {
    *chptr = '_';
  }

  while((chptr = strchr(S1, '/')) != NULL)  /* ignore slash                 */
  {
    *chptr = '_';
  }

  while((chptr = strchr(S2, '/')) != NULL)  /* ignore slash                 */
  {
    *chptr = '_';
  }

  return(strcmp(strupper(S1),strupper(S2)));
}


/* **************************************************************************
 ****************************************************************************
 ****************************************************************************
 ************************************************************************** */


















/* **************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 *******                   U N P A C K   C O D E                      *******
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ************************************************************************** */


/* *****************************
 * ** unpack stored RAR files **
 * *****************************/

BOOL UnstoreFile(void)
{
  if ((long)(*temp_output_buffer_offset=UnpRead(temp_output_buffer,
                                                NewLhd.UnpSize))==-1)
  {
    debug_log("Read error of stored file!");
    return FALSE;
  }
  return TRUE;
}




/* ****************************************
 * ** RAR decompression code starts here **
 * ****************************************/

#define NC 298                              /* alphabet = {0,1,2, .,NC - 1} */
#define DC 48
#define RC 28
#define BC 19
#define MC 257

enum {CODE_HUFFMAN=0,CODE_LZ=1,CODE_LZ2=2,CODE_REPEATLZ=3,CODE_CACHELZ=4,
      CODE_STARTFILE=5,CODE_ENDFILE=6,CODE_STARTMM=8,CODE_ENDMM=7,
      CODE_MMDELTA=9};

struct AudioVariables
{
  int K1,K2,K3,K4,K5;
  int D1,D2,D3,D4;
  int LastDelta;
  unsigned int Dif[11];
  unsigned int ByteCount;
  int LastChar;
};


#define NC 298  /* alphabet = {0, 1, 2, ..., NC - 1} */
#define DC 48
#define RC 28
#define BC 19
#define MC 257


struct AudioVariables AudV[4];

#define GetBits()                                                 \
        BitField = ( ( ( (UDWORD)InBuf[InAddr]   << 16 ) |        \
                       ( (UWORD) InBuf[InAddr+1] <<  8 ) |        \
                       (         InBuf[InAddr+2]       ) )        \
                       >> (8-InBit) ) & 0xffff;


#define AddBits(Bits)                          \
        InAddr += ( InBit + (Bits) ) >> 3;     \
        InBit  =  ( InBit + (Bits) ) &  7;

static unsigned char *UnpBuf;
static unsigned int BitField;
static unsigned int Number;

unsigned char InBuf[8192];                  /* input read buffer            */

unsigned char UnpOldTable[MC*4];

unsigned int InAddr,InBit,ReadTop;

unsigned int LastDist,LastLength;
static unsigned int Length,Distance;

unsigned int OldDist[4],OldDistPtr;


struct LitDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[NC];
} LD;

struct DistDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[DC];
} DD;

struct RepDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[RC];
} RD;

struct MultDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[MC];
} MD[4];

struct BitDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[BC];
} BD;

static struct MultDecode *MDPtr[4]={&MD[0],&MD[1],&MD[2],&MD[3]};

int UnpAudioBlock,UnpChannels,CurChannel,ChannelDelta;


void Unpack(unsigned char *UnpAddr)
/* *** 38.3% of all CPU time is spent within this function!!!               */
{
  static unsigned char LDecode[]={0,1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,
                                  40,48,56,64,80,96,112,128,160,192,224};
  static unsigned char LBits[]=  {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,
                                  3,3,3,4,4,4,4,5,5,5,5};
  static int DDecode[]={0,1,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256,384,
                        512,768,1024,1536,2048,3072,4096,6144,8192,12288,
                        16384,24576,32768U,49152U,65536,98304,131072,196608,
                        262144,327680,393216,458752,524288,589824,655360,
                        720896,786432,851968,917504,983040};
  static unsigned char DBits[]=  {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,
                                  9,10,10,11,11,12,12,13,13,14,14,15,15,16,
                                  16,16,16,16,16,16,16,16,16,16,16,16,16};
  static unsigned char SDDecode[]={0,4,8,16,32,64,128,192};
  static unsigned char SDBits[]={2,2,3, 4, 5, 6,  6,  6};
  unsigned int Bits;


  UnpBuf=UnpAddr;                           /* UnpAddr is a pointer to the  */
  UnpInitData();                            /* unpack buffer                */
  UnpReadBuf(1);
  if (!(NewLhd.Flags & LHD_SOLID))
     ReadTables();
   DestUnpSize--;

  while (DestUnpSize>=0)
  {
    UnpPtr&=MAXWINMASK;

    if (InAddr>sizeof(InBuf)-30)
      UnpReadBuf(0);
    if (((WrPtr-UnpPtr) & MAXWINMASK)<270 && WrPtr!=UnpPtr)
    {


      if (FileFound)
      {

        if (UnpPtr<WrPtr)
        {
                        if((*temp_output_buffer_offset + UnpPtr) > NewLhd.UnpSize)
                        {
                           debug_log("Fatal! Buffer overrun during decompression!");
                          DestUnpSize=-1;

                    } else
                        {
              /* copy extracted data to output buffer                         */
              memcpy(temp_output_buffer + *temp_output_buffer_offset,
                     &UnpBuf[WrPtr], (0-WrPtr) & MAXWINMASK);
              /* update offset within buffer                                  */
              *temp_output_buffer_offset+= (0-WrPtr) & MAXWINMASK;
              /* copy extracted data to output buffer                         */
              memcpy(temp_output_buffer + *temp_output_buffer_offset, UnpBuf,
                     UnpPtr);
              /* update offset within buffer                                  */
              *temp_output_buffer_offset+=UnpPtr;
                        }
        } else
        {
                        if((*temp_output_buffer_offset + (UnpPtr-WrPtr)) > NewLhd.UnpSize)
                        {
                           debug_log("Fatal! Buffer overrun during decompression!");
                          DestUnpSize=-1;
                    } else
                        {
                  /* copy extracted data to output buffer                       */
              memcpy(temp_output_buffer + *temp_output_buffer_offset,
                     &UnpBuf[WrPtr], UnpPtr-WrPtr);
              *temp_output_buffer_offset+=UnpPtr-WrPtr;                                                /* update offset within buffer */
                    }

            }
      }

      WrPtr=UnpPtr;
    }

    if (UnpAudioBlock)
    {
      DecodeNumber((struct Decode *)MDPtr[CurChannel]);
      if (Number==256)
      {
        ReadTables();
        continue;
      }
      UnpBuf[UnpPtr++]=DecodeAudio(Number);
      if (++CurChannel==UnpChannels)
        CurChannel=0;
      DestUnpSize--;
      continue;
    }

    DecodeNumber((struct Decode *)&LD);
    if (Number<256)
    {
      UnpBuf[UnpPtr++]=(UBYTE)Number;
      DestUnpSize--;
      continue;
    }
    if (Number>269)
    {
      Length=LDecode[Number-=270]+3;
      if ((Bits=LBits[Number])>0)
      {
        GetBits();
        Length+=BitField>>(16-Bits);
        AddBits(Bits);
      }

      DecodeNumber((struct Decode *)&DD);
      Distance=DDecode[Number]+1;
      if ((Bits=DBits[Number])>0)
      {
        GetBits();
        Distance+=BitField>>(16-Bits);
        AddBits(Bits);
      }

      if (Distance>=0x40000L)
        Length++;

      if (Distance>=0x2000)
        Length++;

       LastDist=OldDist[OldDistPtr++ & 3]=Distance;
       DestUnpSize-=(LastLength=Length);
       while (Length--)
       {
         UnpBuf[UnpPtr]=UnpBuf[(UnpPtr-Distance) & MAXWINMASK];
         UnpPtr=(UnpPtr+1) & MAXWINMASK;
       }

      continue;
    }
    if (Number==269)
    {
      ReadTables();
      continue;
    }
    if (Number==256)
    {
      Length=LastLength;
      Distance=LastDist;
       LastDist=OldDist[OldDistPtr++ & 3]=Distance;
       DestUnpSize-=(LastLength=Length);
       while (Length--)
       {
         UnpBuf[UnpPtr]=UnpBuf[(UnpPtr-Distance) & MAXWINMASK];
         UnpPtr=(UnpPtr+1) & MAXWINMASK;
       }
      continue;
    }
    if (Number<261)
    {
      Distance=OldDist[(OldDistPtr-(Number-256)) & 3];
      DecodeNumber((struct Decode *)&RD);
      Length=LDecode[Number]+2;
      if ((Bits=LBits[Number])>0)
      {
        GetBits();
        Length+=BitField>>(16-Bits);
        AddBits(Bits);
      }
      if (Distance>=0x40000)
        Length++;
      if (Distance>=0x2000)
        Length++;
      if (Distance>=0x101)
        Length++;
       LastDist=OldDist[OldDistPtr++ & 3]=Distance;
       DestUnpSize-=(LastLength=Length);
       while (Length--)
       {
         UnpBuf[UnpPtr]=UnpBuf[(UnpPtr-Distance) & MAXWINMASK];
         UnpPtr=(UnpPtr+1) & MAXWINMASK;
       }
      continue;
    }
    if (Number<270)
    {
      Distance=SDDecode[Number-=261]+1;
      if ((Bits=SDBits[Number])>0)
      {
        GetBits();
        Distance+=BitField>>(16-Bits);
        AddBits(Bits);
      }
      Length=2;
       LastDist=OldDist[OldDistPtr++ & 3]=Distance;
       DestUnpSize-=(LastLength=Length);
       while (Length--)
       {
         UnpBuf[UnpPtr]=UnpBuf[(UnpPtr-Distance) & MAXWINMASK];
         UnpPtr=(UnpPtr+1) & MAXWINMASK;
       }
      continue;
   }
  }
  ReadLastTables();

  if (FileFound)                            /* flush buffer                 */
  {

    if (UnpPtr<WrPtr)
    {
          if((*temp_output_buffer_offset + UnpPtr) > NewLhd.UnpSize)
          {
            debug_log("Fatal! Buffer overrun during decompression!");
                DestUnpSize=-1;
          } else
          {
        /* copy extracted data to output buffer                             */
        memcpy(temp_output_buffer + *temp_output_buffer_offset, &UnpBuf[WrPtr],
               (0-WrPtr) & MAXWINMASK);
        /* update offset within buffer                                      */
        *temp_output_buffer_offset+= (0-WrPtr) & MAXWINMASK;
        /* copy extracted data to output buffer                             */
        memcpy(temp_output_buffer + *temp_output_buffer_offset, UnpBuf, UnpPtr);
        /* update offset within buffer                                      */
        *temp_output_buffer_offset+=UnpPtr;
          }
    } else
    {
          if((*temp_output_buffer_offset + (UnpPtr-WrPtr)) > NewLhd.UnpSize)
          {
                 debug_log("Fatal! Buffer overrun during decompression!");
                DestUnpSize=-1;
          } else
          {
        /* copy extracted data to output buffer                             */
        memcpy(temp_output_buffer + *temp_output_buffer_offset, &UnpBuf[WrPtr],
               UnpPtr-WrPtr);
        /* update offset within buffer                                      */
        *temp_output_buffer_offset+=UnpPtr-WrPtr;
          }
    }
  }

  WrPtr=UnpPtr;
}


unsigned int UnpRead(unsigned char *Addr,unsigned int Count)
{
  int RetCode=0;
  unsigned int I,ReadSize,TotalRead=0;
  unsigned char *ReadAddr;
  ReadAddr=Addr;
  while (Count > 0)
  {
    ReadSize=(unsigned int)((Count>(unsigned long)UnpPackedSize) ?
                                                  UnpPackedSize : Count);
#ifdef _USE_MEMORY_TO_MEMORY_DECOMPRESSION
    if(MemRARFile->data == NULL)
      return(0);
    RetCode=tread(MemRARFile, ReadAddr, ReadSize);
#else
    if (ArcPtr==NULL)
      return(0);
    RetCode=tread(ArcPtr,ReadAddr,ReadSize);
#endif
    CurUnpRead+=RetCode;
    ReadAddr+=RetCode;
    TotalRead+=RetCode;
    Count-=RetCode;
    UnpPackedSize-=RetCode;
      break;
  }
  if (RetCode!= -1)
  {
    RetCode=TotalRead;
    if (Encryption)
    {
      if (Encryption<20)
          {
            debug_log("Old Crypt() not supported!");
          }
      else
      {
        for (I=0;I<(unsigned int)RetCode;I+=16)
          DecryptBlock(&Addr[I]);
      }
    }
  }
  return(RetCode);
}


void UnpReadBuf(int FirstBuf)
{
  int RetCode;
  if (FirstBuf)
  {
    ReadTop=UnpRead(InBuf,sizeof(InBuf));
    InAddr=0;
  }
  else
  {
    memcpy(InBuf,&InBuf[sizeof(InBuf)-32],32);
    InAddr&=0x1f;
    RetCode=UnpRead(&InBuf[32],sizeof(InBuf)-32);
    if (RetCode>0)
      ReadTop=RetCode+32;
    else
      ReadTop=InAddr;
  }
}


void ReadTables(void)
{
  UBYTE BitLength[BC];
  unsigned char Table[MC*4];
  int TableSize,N,I;
  if (InAddr>sizeof(InBuf)-25)
    UnpReadBuf(0);
  GetBits();
  UnpAudioBlock=(BitField & 0x8000);

  if (!(BitField & 0x4000))
    memset(UnpOldTable,0,sizeof(UnpOldTable));
  AddBits(2);


  if (UnpAudioBlock)
  {
    UnpChannels=((BitField>>12) & 3)+1;
    if (CurChannel>=UnpChannels)
      CurChannel=0;
    AddBits(2);
    TableSize=MC*UnpChannels;
  }
  else
    TableSize=NC+DC+RC;


  for (I=0;I<BC;I++)
  {
    GetBits();
    BitLength[I]=(UBYTE)(BitField >> 12);
    AddBits(4);
  }
  MakeDecodeTables(BitLength,(struct Decode *)&BD,BC);
  I=0;
  while (I<TableSize)
  {
    if (InAddr>sizeof(InBuf)-5)
      UnpReadBuf(0);
    DecodeNumber((struct Decode *)&BD);
    if (Number<16)
      Table[I++]=(Number+UnpOldTable[I]) & 0xf;
    else
      if (Number==16)
      {
        GetBits();
        N=(BitField >> 14)+3;
        AddBits(2);
        while (N-- > 0 && I<TableSize)
        {
          Table[I]=Table[I-1];
          I++;
        }
      }
      else
      {
        if (Number==17)
        {
          GetBits();
          N=(BitField >> 13)+3;
          AddBits(3);
        }
        else
        {
          GetBits();
          N=(BitField >> 9)+11;
          AddBits(7);
        }
        while (N-- > 0 && I<TableSize)
          Table[I++]=0;
      }
  }
  if (UnpAudioBlock)
    for (I=0;I<UnpChannels;I++)
      MakeDecodeTables(&Table[I*MC],(struct Decode *)MDPtr[I],MC);
  else
  {
    MakeDecodeTables(&Table[0],(struct Decode *)&LD,NC);
    MakeDecodeTables(&Table[NC],(struct Decode *)&DD,DC);
    MakeDecodeTables(&Table[NC+DC],(struct Decode *)&RD,RC);
  }
  memcpy(UnpOldTable,Table,sizeof(UnpOldTable));
}


static void ReadLastTables(void)
{
  if (ReadTop>=InAddr+5)
  {
    if (UnpAudioBlock)
    {
      DecodeNumber((struct Decode *)MDPtr[CurChannel]);
      if (Number==256)
        ReadTables();
    }
    else
    {
      DecodeNumber((struct Decode *)&LD);
      if (Number==269)
        ReadTables();
    }
  }
}


static void MakeDecodeTables(unsigned char *LenTab,
                             struct Decode *Dec,
                             int Size)
{
  int LenCount[16],TmpPos[16],I;
  long M,N;
  memset(LenCount,0,sizeof(LenCount));
  for (I=0;I<Size;I++)
    LenCount[LenTab[I] & 0xF]++;

  LenCount[0]=0;
  for (TmpPos[0]=Dec->DecodePos[0]=Dec->DecodeLen[0]=0,N=0,I=1;I<16;I++)
  {
    N=2*(N+LenCount[I]);
    M=N<<(15-I);
    if (M>0xFFFF)
      M=0xFFFF;
    Dec->DecodeLen[I]=(unsigned int)M;
    TmpPos[I]=Dec->DecodePos[I]=Dec->DecodePos[I-1]+LenCount[I-1];
  }

  for (I=0;I<Size;I++)
    if (LenTab[I]!=0)
      Dec->DecodeNum[TmpPos[LenTab[I] & 0xF]++]=I;
  Dec->MaxNum=Size;
}


static void DecodeNumber(struct Decode *Deco)
/* *** 52.6% of all CPU time is spent within this function!!!               */
{
  unsigned int I;
  register unsigned int N;
  GetBits();

#ifdef _USE_ASM

#ifdef _WIN_32
 __asm {

    xor eax, eax
    mov eax, BitField                       // N=BitField & 0xFFFE;
    and eax, 0xFFFFFFFE
    mov [N], eax
    mov edx, [Deco]                         // EAX=N, EDX=Deco

          cmp  eax, dword ptr[edx + 8*4 + 4]// if (N<Dec->DecodeLen[8])
          jae  else_G

             cmp  eax, dword ptr[edx + 4*4 + 4]// if (N<Dec->DecodeLen[4])
             jae  else_F


                cmp  eax, dword ptr[edx + 2*4 + 4]// if (N<Dec->DecodeLen[2])
                jae  else_C

                   cmp  eax, dword ptr[edx + 1*4 + 4]// if (N<Dec->DecodeLen[1])
                   jae  else_1
                   mov  I, 1                         //  I=1;
                   jmp  next_1
                 else_1:                             // else
                   mov  I, 2                         //  I=2;
                 next_1:

                jmp  next_C
              else_C:                             // else

                   cmp  eax, dword ptr[edx + 3*4 + 4]// if (N<Dec->DecodeLen[3])
                   jae  else_2
                   mov  I, 3                         //  I=3;
                   jmp  next_2
                 else_2:                             // else
                   mov  I, 4                         //  I=4;
                 next_2:

              next_C:                             // else

             jmp  next_F
           else_F:


             cmp  eax, dword ptr[edx + 6*4 + 4]// if (N<Dec->DecodeLen[6])
             jae  else_E

                cmp  eax, dword ptr[edx + 5*4 + 4]// if (N<Dec->DecodeLen[5])
                jae  else_3
                mov  I, 5                         //  I=5;
                jmp  next_3
              else_3:                             // else
                mov  I, 6                         //  I=6;
              next_3:

             jmp  next_E
           else_E:                             // else

                cmp  eax, dword ptr[edx + 7*4 + 4]// if (N<Dec->DecodeLen[7])
                jae  else_4
                mov  I, 7                         //  I=7;
                jmp  next_4
              else_4:                             // else
                mov  I, 8                         //  I=8;
              next_4:

           next_E:

           next_F:

          jmp  next_G
        else_G:

          cmp  eax, dword ptr[edx + 12*4 + 4] // if (N<Dec->DecodeLen[12])
          jae  else_D

             cmp  eax, dword ptr[edx + 10*4 + 4]// if (N<Dec->DecodeLen[10])
             jae  else_B

                cmp  eax, dword ptr[edx + 9*4 + 4]// if (N<Dec->DecodeLen[9])
                jae  else_5
                mov  I, 9                         //  I=9;
                jmp  next_5
              else_5:                             // else
                mov  I, 10                         //  I=10;
              next_5:

             jmp  next_B
           else_B:                             // else

                cmp  eax, dword ptr[edx + 11*4 + 4]// if (N<Dec->DecodeLen[11])
                jae  else_6
                mov  I, 11                         //  I=11;
                jmp  next_6
              else_6:                             // else
                mov  I, 12                         //  I=12;
              next_6:

           next_B:


          jmp  next_D
        else_D:                             // else

               cmp  eax, dword ptr[edx + 14*4 + 4]// if (N<Dec->DecodeLen[14])
               jae  else_A

                  cmp  eax, dword ptr[edx + 13*4 + 4]// if (N<Dec->DecodeLen[13])
                  jae  else_7
                  mov  I, 13                         //  I=13;
                  jmp  next_7
                 else_7:                             // else
                  mov  I, 14                         //  I=14;
                 next_7:

               jmp  next_A
              else_A:                             // else
               mov  I, 15                         //  I=15;
              next_A:

        next_D:
    next_G:
}
#else
 __asm__ __volatile__ (
     "andl $0xFFFFFFFE, %%eax"
"      movl %%eax, %1"
"          cmpl 8*4(%%edx), %%eax /* 5379 */"
"          jae  else_G"
""
"             cmpl 4*4(%%edx), %%eax"
"             jae  else_F"
""
"                cmpl 2*4(%%edx), %%eax"
"                jae  else_C"
""
"                   cmpl 1*4(%%edx), %%eax"
""
"                   jae  else_1"
"                   movl $1, %0"
"                   jmp  next_1"
"                 else_1:       "
"                   movl  $2, %0"
"                 next_1:"
"                "
"                jmp  next_C"
"              else_C:          "
""
"                   cmpl 3*4(%%edx), %%eax "
"                   jae  else_2"
"                   movl  $3, %0"
"                   jmp  next_2"
"                 else_2:       "
"                   movl  $4, %0"
"                 next_2:"
""
"              next_C:          "
""
"             jmp  next_F"
"           else_F:"
""
"             cmpl 6*4(%%edx), %%eax"
"             jae  else_E"
""
"                cmpl 5*4(%%edx), %%eax"
"                jae  else_3"
"                movl  $5, %0   "
"                jmp  next_3"
"              else_3:          "
"                movl  $6, %0   "
"              next_3:"
""
"             jmp  next_E"
"           else_E:             "
""
"                cmpl 7*4(%%edx), %%eax"
"                jae  else_4"
"                movl  $7, %0   "
"                jmp  next_4"
"              else_4:          "
"                movl  $8, %0   "
"              next_4:"
""
"           next_E:"
""
"           next_F:"
""
"          jmp  next_G"
"        else_G:"
""
"          cmpl 12*4(%%edx), %%eax"
"          jae  else_D"
""
"             cmpl 10*4(%%edx), %%eax"
"             jae  else_B"
""
"                cmpl 9*4(%%edx), %%eax"
"                jae  else_5"
"                movl  $9, %0   "
"                jmp  next_5"
"              else_5:          "
"                movl  $10, %0  "
"              next_5:"
""
"             jmp  next_B"
"           else_B:             "
""
"                cmpl 11*4(%%edx), %%eax"
" "
"                jae  else_6"
"                movl  $11, %0  "
"                jmp  next_6"
"              else_6:          "
"                movl  $12, %0  "
"              next_6:"
""
"           next_B:"
"      "
"        "
"          jmp  next_D"
"        else_D:                "
""
"               cmpl 14*4(%%edx), %%eax"
"               jae  else_A"
""
"                  cmpl 13*4(%%edx), %%eax"
"                  jae  else_7"
"                  movl  $13, %0"
"                  jmp  next_7"
"                 else_7:       "
"                  movl  $14, %0"
"                 next_7:"
""
"               jmp  next_A"
"              else_A:          "
"               movl  $15, %0   "
"              next_A:"
"          "
"        next_D:                             "
"    next_G:"
     : "=g" (I), "=r"(N)
     : "eax" ((long)BitField), "edx"((long)Deco->DecodeLen)
      : "memory"
     );
#endif /* #ifdef _WIN_32 ... #elif defined _X86_ASM_ */

#else
  N=BitField & 0xFFFE;
  if (N<Deco->DecodeLen[8])  {
    if (N<Deco->DecodeLen[4]) {
      if (N<Deco->DecodeLen[2]) {
        if (N<Deco->DecodeLen[1])
          I=1;
        else
          I=2;
      } else {
        if (N<Deco->DecodeLen[3])
          I=3;
        else
          I=4;
      }
    } else {
      if (N<Deco->DecodeLen[6])  {
        if (N<Deco->DecodeLen[5])
          I=5;
        else
          I=6;
      } else {
        if (N<Deco->DecodeLen[7])
          I=7;
        else
          I=8;
      }
   }
  } else {
    if (N<Deco->DecodeLen[12]) {
      if (N<Deco->DecodeLen[10]) {
        if (N<Deco->DecodeLen[9])
          I=9;
        else
          I=10;
      } else {
        if (N<Deco->DecodeLen[11])
          I=11;
        else
          I=12;
      }
    } else {
      if (N<Deco->DecodeLen[14]) {
        if (N<Deco->DecodeLen[13])
          I=13;
        else
          I=14;

      } else {
          I=15;
      }
    }

  }
#endif

  AddBits(I);
  if ((N=Deco->DecodePos[I]+((N-Deco->DecodeLen[I-1])>>(16-I)))>=Deco->MaxNum)
      N=0;
  Number=Deco->DecodeNum[N];
}


void UnpInitData()
{
  InAddr=InBit=0;
  if (!(NewLhd.Flags & LHD_SOLID))
  {
    ChannelDelta=CurChannel=0;

#ifdef _USE_ASM

#ifdef _WIN_32                              /* Win32 with VisualC           */

    __asm {
        push edi
        push eax
        push ecx

        cld                                 /* increment EDI and ESI        */
        mov  al, 0x00
        mov  ecx, SIZE AudV
        mov  edi, Offset AudV
        rep  stosb                          /* clear memory                 */

        mov  ecx, SIZE OldDist
        mov  edi, Offset OldDist
        rep  stosb                          /* clear memory                 */

        mov  ecx, SIZE UnpOldTable
        mov  edi, Offset UnpOldTable
        rep  stosb                          /* clear memory                 */

        pop  ecx
        pop  eax
        pop  edi


        mov  [OldDistPtr], 0
        mov  [LastDist], 0
        mov  [LastLength], 0
        mov  [UnpPtr], 0
        mov  [WrPtr], 0
        mov  [OldDistPtr], 0
        mov  [LastLength], 0
        mov  [LastDist], 0
        mov  [UnpPtr], 0
        mov  [WrPtr], 0

    }
    memset(UnpBuf,0,MAXWINSIZE);


#else                    /* unix/linux on i386 cpus */
    __asm__ __volatile (
"        cld                                 /* increment EDI and ESI        */"
"        movb $0x00, %%al"
"        movl %0, %%ecx"
"        movl %1, %%edi"
"        rep  "
"        stosb                              /* clear memory                 */"
""
"        movl %2, %%ecx"
"        mov  %3, %%edi"
"        rep  "
"        stosb                              /* clear memory                 */"
""
"        movl %4, %%ecx"
"        movl %5, %%edi"
"        rep  "
"        stosb                              /* clear memory                 */"
""
"        movl $0, (OldDistPtr)"
"        movl $0, (LastDist)"
"        movl $0, (LastLength)"
"        movl $0, (UnpPtr)"
"        movl $0, (WrPtr)"
"        movl $0, (OldDistPtr)"
"        movl $0, (LastLength)"
"        movl $0, (LastDist)"
"        movl $0, (UnpPtr)"
"        movl $0, (WrPtr)"
        :
        : "m" ((long)sizeof(AudV)),
          "m" ((long)AudV),
          "m" ((long)sizeof(OldDist)),
          "m" ((long)OldDist),
          "m" ((long)sizeof(UnpOldTable)),
          "m" ((long)UnpOldTable)
        : "memory", "edi", "eax", "ecx"
    );
    memset(UnpBuf,0,MAXWINSIZE);
#endif

#else                                       /* unix/linux on non-i386 cpu  */
    memset(AudV,0,sizeof(AudV));
    memset(OldDist,0,sizeof(OldDist));
    OldDistPtr=0;
    LastDist=LastLength=0;
    memset(UnpBuf,0,MAXWINSIZE);
    memset(UnpOldTable,0,sizeof(UnpOldTable));
    UnpPtr=WrPtr=0;
#endif

  }
}


UBYTE DecodeAudio(int Delta)
{
  struct AudioVariables *V;
  unsigned int Ch;
  unsigned int NumMinDif,MinDif;
  int PCh,I;

  V=&AudV[CurChannel];
  V->ByteCount++;
  V->D4=V->D3;
  V->D3=V->D2;
  V->D2=V->LastDelta-V->D1;
  V->D1=V->LastDelta;
  PCh=8*V->LastChar+V->K1*V->D1+V->K2*V->D2+
           V->K3*V->D3+V->K4*V->D4+V->K5*ChannelDelta;
  PCh=(PCh>>3) & 0xFF;

  Ch=PCh-Delta;

  I=((signed char)Delta)<<3;

  V->Dif[0]+=abs(I);
  V->Dif[1]+=abs(I-V->D1);
  V->Dif[2]+=abs(I+V->D1);
  V->Dif[3]+=abs(I-V->D2);
  V->Dif[4]+=abs(I+V->D2);
  V->Dif[5]+=abs(I-V->D3);
  V->Dif[6]+=abs(I+V->D3);
  V->Dif[7]+=abs(I-V->D4);
  V->Dif[8]+=abs(I+V->D4);
  V->Dif[9]+=abs(I-ChannelDelta);
  V->Dif[10]+=abs(I+ChannelDelta);

  ChannelDelta=V->LastDelta=(signed char)(Ch-V->LastChar);
  V->LastChar=Ch;

  if ((V->ByteCount & 0x1F)==0)
  {
    MinDif=V->Dif[0];
    NumMinDif=0;
    V->Dif[0]=0;
    for (I=1;(unsigned int)I<sizeof(V->Dif)/sizeof(V->Dif[0]);I++)
    {
      if (V->Dif[I]<MinDif)
      {
        MinDif=V->Dif[I];
        NumMinDif=I;
      }
      V->Dif[I]=0;
    }
    switch(NumMinDif)
    {
      case 1:
        if (V->K1>=-16)
          V->K1--;
        break;
      case 2:
        if (V->K1<16)
          V->K1++;
        break;
      case 3:
        if (V->K2>=-16)
          V->K2--;
        break;
      case 4:
        if (V->K2<16)
          V->K2++;
        break;
      case 5:
        if (V->K3>=-16)
          V->K3--;
        break;
      case 6:
        if (V->K3<16)
          V->K3++;
        break;
      case 7:
        if (V->K4>=-16)
          V->K4--;
        break;
      case 8:
        if (V->K4<16)
          V->K4++;
        break;
      case 9:
        if (V->K5>=-16)
          V->K5--;
        break;
      case 10:
        if (V->K5<16)
          V->K5++;
        break;
    }
  }
  return((UBYTE)Ch);
}







/* ***************************************************
 * ** CRCCrypt Code - decryption engine starts here **
 * ***************************************************/


#define NROUNDS 32

#define rol(x,n)  (((x)<<(n)) | ((x)>>(8*sizeof(x)-(n))))
#define ror(x,n)  (((x)>>(n)) | ((x)<<(8*sizeof(x)-(n))))

#define substLong(t) ( (UDWORD)SubstTable[(int)t&255] | \
           ((UDWORD)SubstTable[(int)(t>> 8)&255]<< 8) | \
           ((UDWORD)SubstTable[(int)(t>>16)&255]<<16) | \
           ((UDWORD)SubstTable[(int)(t>>24)&255]<<24) )


UDWORD CRCTab[256];

UBYTE SubstTable[256];
UBYTE InitSubstTable[256]={
  215, 19,149, 35, 73,197,192,205,249, 28, 16,119, 48,221,  2, 42,
  232,  1,177,233, 14, 88,219, 25,223,195,244, 90, 87,239,153,137,
  255,199,147, 70, 92, 66,246, 13,216, 40, 62, 29,217,230, 86,  6,
   71, 24,171,196,101,113,218,123, 93, 91,163,178,202, 67, 44,235,
  107,250, 75,234, 49,167,125,211, 83,114,157,144, 32,193,143, 36,
  158,124,247,187, 89,214,141, 47,121,228, 61,130,213,194,174,251,
   97,110, 54,229,115, 57,152, 94,105,243,212, 55,209,245, 63, 11,
  164,200, 31,156, 81,176,227, 21, 76, 99,139,188,127, 17,248, 51,
  207,120,189,210,  8,226, 41, 72,183,203,135,165,166, 60, 98,  7,
  122, 38,155,170, 69,172,252,238, 39,134, 59,128,236, 27,240, 80,
  131,  3, 85,206,145, 79,154,142,159,220,201,133, 74, 64, 20,129,
  224,185,138,103,173,182, 43, 34,254, 82,198,151,231,180, 58, 10,
  118, 26,102, 12, 50,132, 22,191,136,111,162,179, 45,  4,148,108,
  161, 56, 78,126,242,222, 15,175,146, 23, 33,241,181,190, 77,225,
    0, 46,169,186, 68, 95,237, 65, 53,208,253,168,  9, 18,100, 52,
  116,184,160, 96,109, 37, 30,106,140,104,150,  5,204,117,112, 84
};

UDWORD Key[4];


void EncryptBlock(UBYTE *Buf)
{
  int I;

  UDWORD A,B,C,D,T,TA,TB;
#ifdef NON_INTEL_BYTE_ORDER
  A=((UDWORD)Buf[0]|((UDWORD)Buf[1]<<8)|((UDWORD)Buf[2]<<16)|
      ((UDWORD)Buf[3]<<24))^Key[0];
  B=((UDWORD)Buf[4]|((UDWORD)Buf[5]<<8)|((UDWORD)Buf[6]<<16)|
      ((UDWORD)Buf[7]<<24))^Key[1];
  C=((UDWORD)Buf[8]|((UDWORD)Buf[9]<<8)|((UDWORD)Buf[10]<<16)|
      ((UDWORD)Buf[11]<<24))^Key[2];
  D=((UDWORD)Buf[12]|((UDWORD)Buf[13]<<8)|((UDWORD)Buf[14]<<16)|
      ((UDWORD)Buf[15]<<24))^Key[3];
#else
  UDWORD *BufPtr;
  BufPtr=(UDWORD *)Buf;
  A=BufPtr[0]^Key[0];
  B=BufPtr[1]^Key[1];
  C=BufPtr[2]^Key[2];
  D=BufPtr[3]^Key[3];
#endif
  for(I=0;I<NROUNDS;I++)
  {
    T=((C+rol(D,11))^Key[I&3]);
    TA=A^substLong(T);
    T=((D^rol(C,17))+Key[I&3]);
    TB=B^substLong(T);
    A=C;
    B=D;
    C=TA;
    D=TB;
  }
#ifdef NON_INTEL_BYTE_ORDER
  C^=Key[0];
  Buf[0]=(UBYTE)C;
  Buf[1]=(UBYTE)(C>>8);
  Buf[2]=(UBYTE)(C>>16);
  Buf[3]=(UBYTE)(C>>24);
  D^=Key[1];
  Buf[4]=(UBYTE)D;
  Buf[5]=(UBYTE)(D>>8);
  Buf[6]=(UBYTE)(D>>16);
  Buf[7]=(UBYTE)(D>>24);
  A^=Key[2];
  Buf[8]=(UBYTE)A;
  Buf[9]=(UBYTE)(A>>8);
  Buf[10]=(UBYTE)(A>>16);
  Buf[11]=(UBYTE)(A>>24);
  B^=Key[3];
  Buf[12]=(UBYTE)B;
  Buf[13]=(UBYTE)(B>>8);
  Buf[14]=(UBYTE)(B>>16);
  Buf[15]=(UBYTE)(B>>24);
#else
  BufPtr[0]=C^Key[0];
  BufPtr[1]=D^Key[1];
  BufPtr[2]=A^Key[2];
  BufPtr[3]=B^Key[3];
#endif
  UpdKeys(Buf);
}


void DecryptBlock(UBYTE *Buf)
{
  int I;
  UBYTE InBuf[16];
  UDWORD A,B,C,D,T,TA,TB;
#ifdef NON_INTEL_BYTE_ORDER
  A=((UDWORD)Buf[0]|((UDWORD)Buf[1]<<8)|((UDWORD)Buf[2]<<16)|
      ((UDWORD)Buf[3]<<24))^Key[0];
  B=((UDWORD)Buf[4]|((UDWORD)Buf[5]<<8)|((UDWORD)Buf[6]<<16)|
      ((UDWORD)Buf[7]<<24))^Key[1];
  C=((UDWORD)Buf[8]|((UDWORD)Buf[9]<<8)|((UDWORD)Buf[10]<<16)|
      ((UDWORD)Buf[11]<<24))^Key[2];
  D=((UDWORD)Buf[12]|((UDWORD)Buf[13]<<8)|((UDWORD)Buf[14]<<16)|
      ((UDWORD)Buf[15]<<24))^Key[3];
#else
  UDWORD *BufPtr;
  BufPtr=(UDWORD *)Buf;
  A=BufPtr[0]^Key[0];                       /* xxx may be this can be       */
  B=BufPtr[1]^Key[1];                       /* optimized in assembler       */
  C=BufPtr[2]^Key[2];
  D=BufPtr[3]^Key[3];
#endif
  memcpy(InBuf,Buf,sizeof(InBuf));
  for(I=NROUNDS-1;I>=0;I--)
  {
    T=((C+rol(D,11))^Key[I&3]);
    TA=A^substLong(T);
    T=((D^rol(C,17))+Key[I&3]);
    TB=B^substLong(T);
    A=C;
    B=D;
    C=TA;
    D=TB;
  }
#ifdef NON_INTEL_BYTE_ORDER
  C^=Key[0];
  Buf[0]=(UBYTE)C;
  Buf[1]=(UBYTE)(C>>8);
  Buf[2]=(UBYTE)(C>>16);
  Buf[3]=(UBYTE)(C>>24);
  D^=Key[1];
  Buf[4]=(UBYTE)D;
  Buf[5]=(UBYTE)(D>>8);
  Buf[6]=(UBYTE)(D>>16);
  Buf[7]=(UBYTE)(D>>24);
  A^=Key[2];
  Buf[8]=(UBYTE)A;
  Buf[9]=(UBYTE)(A>>8);
  Buf[10]=(UBYTE)(A>>16);
  Buf[11]=(UBYTE)(A>>24);
  B^=Key[3];
  Buf[12]=(UBYTE)B;
  Buf[13]=(UBYTE)(B>>8);
  Buf[14]=(UBYTE)(B>>16);
  Buf[15]=(UBYTE)(B>>24);
#else
  BufPtr[0]=C^Key[0];
  BufPtr[1]=D^Key[1];
  BufPtr[2]=A^Key[2];
  BufPtr[3]=B^Key[3];
#endif
  UpdKeys(InBuf);
}


void UpdKeys(UBYTE *Buf)
{
  int I;
  for (I=0;I<16;I+=4)
  {
    Key[0]^=CRCTab[Buf[I]];                 /* xxx may be I'll rewrite this */
    Key[1]^=CRCTab[Buf[I+1]];               /* in asm for speedup           */
    Key[2]^=CRCTab[Buf[I+2]];
    Key[3]^=CRCTab[Buf[I+3]];
  }
}

void SetCryptKeys(char *Password)
{
  unsigned int I,J,K,PswLength;
  unsigned char N1,N2;
  unsigned char Psw[256];

#if !defined _USE_ASM
  UBYTE Ch;
#endif

  SetOldKeys(Password);

  Key[0]=0xD3A3B879L;
  Key[1]=0x3F6D12F7L;
  Key[2]=0x7515A235L;
  Key[3]=0xA4E7F123L;
  memset(Psw,0,sizeof(Psw));
  strcpy((char *)Psw,Password);
  PswLength=strlen(Password);
  memcpy(SubstTable,InitSubstTable,sizeof(SubstTable));

  for (J=0;J<256;J++)
    for (I=0;I<PswLength;I+=2)
    {
      N2=(unsigned char)CRCTab[(Psw[I+1]+J)&0xFF];
      for (K=1, N1=(unsigned char)CRCTab[(Psw[I]-J)&0xFF];
           (N1!=N2) && (N1 < 256);      /* I had to add "&& (N1 < 256)",    */
           N1++, K++)                   /* because the system crashed with  */
          {                             /* encrypted RARs                   */
#ifdef _USE_ASM

#ifdef _WIN_32
          __asm {

                    mov ebx, Offset SubstTable
                    mov edx, ebx

                    xor ecx, ecx            // read SubstTable[N1]...
                    mov cl, N1
                    add ebx, ecx
                    mov al, byte ptr[ebx]

                    mov cl, N1              // read SubstTable[(N1+I+K)&0xFF]...
                    add ecx, I
                    add ecx, K
                    and ecx, 0xFF
                    add edx, ecx
                    mov ah, byte ptr[edx]

                    mov  byte ptr[ebx], ah  // and write back
                    mov  byte ptr[edx], al

               }
#else
                     __asm__ __volatile__ (
"                    xorl %%ecx, %%ecx"
"                    movl %2, %%ecx                     /* ecx = N1 */"
"                    mov %%ebx, %%edx"
"                    addl %%ecx, %%ebx"
""
"                    addl %0, %%ecx"
"                    addl %1, %%ecx"
"                    andl $0x000000FF, %%ecx"
"                    addl %%ecx, %%edx"
"                    "
"                    movb (%%ebx), %%al"
"                    movb (%%edx), %%ah"
""
"                    movb  %%ah, (%%ebx)     /* and write back */"
"                    movb  %%al, (%%edx)"
                    : : "g" ((long)I),
                        "g" ((long)K),
                        "g" ((long)N1),
                        "ebx"((long)SubstTable)
                    : "ecx", "edx"

              );
#endif

#else
            /* Swap(&SubstTable[N1],&SubstTable[(N1+I+K)&0xFF]);            */
             Ch=SubstTable[N1];
             SubstTable[N1]=SubstTable[(N1+I+K)&0xFF];
             SubstTable[(N1+I+K)&0xFF]=Ch;
#endif
          }
    }
  for (I=0;I<PswLength;I+=16)
    EncryptBlock(&Psw[I]);
}


void SetOldKeys(char *Password)
{
  UDWORD PswCRC;
  UBYTE Ch;
  PswCRC=CalcCRC32(0xFFFFFFFFL,(UBYTE*)Password,strlen(Password));
  OldKey[0]=(UWORD)PswCRC;
  OldKey[1]=(UWORD)(PswCRC>>16);
  OldKey[2]=OldKey[3]=0;
  PN1=PN2=PN3=0;
  while ((Ch=*Password)!=0)
  {
    PN1+=Ch;
    PN2^=Ch;
    PN3+=Ch;
    PN3=(UBYTE)rol(PN3,1);
    OldKey[2]^=((UWORD)(Ch^CRCTab[Ch]));
    OldKey[3]+=((UWORD)(Ch+(CRCTab[Ch]>>16)));
    Password++;
  }
}

void InitCRC(void)
{
  int I, J;
  UDWORD C;
  for (I=0;I<256;I++)
  {
    for (C=I,J=0;J<8;J++)
      C=(C & 1) ? (C>>1)^0xEDB88320L : (C>>1);
    CRCTab[I]=C;
  }
}


UDWORD CalcCRC32(UDWORD StartCRC,UBYTE *Addr,UDWORD Size)
{
  unsigned int I;
  for (I=0; I<Size; I++)
    StartCRC = CRCTab[(UBYTE)StartCRC ^ Addr[I]] ^ (StartCRC >> 8);
  return(StartCRC);
}


/* **************************************************************************
 ****************************************************************************
 ****************************************************************************
 ************************************************************************** */

























/* **************************************************************************
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 *******              D E B U G    F U N C T I O N S                  *******
 *******                                                              *******
 *******                                                              *******
 *******                                                              *******
 ****************************************************************************
 ****************************************************************************
 ****************************************************************************
 ************************************************************************** */
#ifdef _DEBUG_LOG


/* -- global stuff -------------------------------------------------------- */
char  log_file_name[256];                   /* file name for the log file   */
DWORD debug_start_time;                     /* starttime of debug           */
BOOL  debug_started = FALSE;                /* debug_log writes only if     */
                                            /* this is TRUE                 */
/* ------------------------------------------------------------------------ */


/* -- global functions ---------------------------------------------------- */
void debug_init_proc(char *file_name)
/* Create/Rewrite a log file                                                */
{
  FILE *fp;
  char date[] = __DATE__;
  char time[] = __TIME__;

  debug_start_time = GetTickCount();        /* get start time               */
  strcpy(log_file_name, file_name);         /* save file name               */

  if((fp = fopen(log_file_name, CREATETEXT)) != NULL)
  {
    debug_started = TRUE;                   /* enable debug                 */
    fprintf(fp, "Debug log of UniquE's RARFileLib\n"\
                "~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~^~\n");
    fprintf(fp, "(executable compiled on %s at %s)\n\n", date, time);
    fclose(fp);
  }
}


void debug_log_proc(char *text, char *sourcefile, int sourceline)
/* add a line to the log file                                               */
{
  FILE *fp;

  if(debug_started == FALSE) return;        /* exit if not initialized      */

  if((fp = fopen(log_file_name, APPENDTEXT)) != NULL) /* append to logfile  */

  {
    fprintf(fp, " %8u ms (line %u in %s):\n              - %s\n",
            (unsigned int)(GetTickCount() - debug_start_time),
            sourceline, sourcefile, text);
    fclose(fp);
  }
}

/* ------------------------------------------------------------------------ */
#endif
/* **************************************************************************
 ****************************************************************************
 ****************************************************************************
 ************************************************************************** */


/* end of file urarlib.c */
