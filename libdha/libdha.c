/*
    libgha.c - Library for direct hardware access
    Copyrights:
    1996/10/27	- Robin Cutshaw (robin@xfree86.org)
		  XFree86 3.3.3 implementation
    1999	- Øyvind Aabling.
    		  Modified for GATOS/win/gfxdump.
		  
    2002	- library implementation by Nick Kurshev
    
    supported O/S's:	SVR4, UnixWare, SCO, Solaris,
			FreeBSD, NetBSD, 386BSD, BSDI BSD/386,
			Linux, Mach/386, ISC
			DOS (WATCOM 9.5 compiler), Win9x (with mapdev.vxd)
    Licence: GPL
    Original location: www.linuxvideo.org/gatos
*/

#include "libdha.h"
#include "AsmMacros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
// MAPDEV.h - include file for VxD MAPDEV
// Copyright (c) 1996 Vireo Software, Inc.

#include <windows.h>

// This is the request structure that applications use
// to request services from the MAPDEV VxD.

typedef struct _MapDevRequest
{
	DWORD	mdr_ServiceID;			// supplied by caller
	LPVOID	mdr_PhysicalAddress;	// supplied by caller
	DWORD	mdr_SizeInBytes;		// supplied by caller
	LPVOID	mdr_LinearAddress;		// returned by VxD
	WORD	mdr_Selector;			// returned if 16-bit caller
	WORD	mdr_Status;				// MDR_xxxx code below
} MAPDEVREQUEST, *PMAPDEVREQUEST;

#define MDR_SERVICE_MAP		CTL_CODE(FILE_DEVICE_UNKNOWN, 1, METHOD_NEITHER, FILE_ANY_ACCESS)
#define MDR_SERVICE_UNMAP	CTL_CODE(FILE_DEVICE_UNKNOWN, 2, METHOD_NEITHER, FILE_ANY_ACCESS)

#define MDR_STATUS_SUCCESS	1
#define MDR_STATUS_ERROR	0
/*#include "winioctl.h"*/
#define FILE_DEVICE_UNKNOWN             0x00000022
#define METHOD_NEITHER                  3
#define FILE_ANY_ACCESS                 0
#define CTL_CODE( DeviceType, Function, Method, Access ) ( \
    ((DeviceType)<<16) | ((Access)<<14) | ((Function)<<2) | (Method) )

/* Memory Map a piece of Real Memory */
void *map_phys_mem(unsigned base, unsigned size) {

  HANDLE hDevice ;
  PVOID inBuf[1] ;		/* buffer for struct pointer to VxD */
  DWORD RetInfo[2] ;		/* buffer to receive data from VxD */
  DWORD cbBytesReturned ;	/* count of bytes returned from VxD */
  MAPDEVREQUEST req ;		/* map device request structure */
  DWORD *pNicstar, Status, Time ; int i ; char *endptr ;
  const PCHAR VxDName = "\\\\.\\MAPDEV.VXD" ;
  const PCHAR VxDNameAlreadyLoaded = "\\\\.\\MAPDEV" ;

  hDevice = CreateFile(VxDName, 0,0,0,
                       CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE, 0) ;
  if (hDevice == INVALID_HANDLE_VALUE)
    hDevice = CreateFile(VxDNameAlreadyLoaded, 0,0,0,
                         CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE, 0) ;
  if (hDevice == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "Cannot open driver, error=%08lx\n", GetLastError()) ;
    exit(1) ; }

  req.mdr_ServiceID = MDR_SERVICE_MAP ;
  req.mdr_PhysicalAddress = (PVOID)base ;
  req.mdr_SizeInBytes = size ;
  inBuf[0] = &req ;

  if ( ! DeviceIoControl(hDevice, MDR_SERVICE_MAP, inBuf, sizeof(PVOID),
         NULL, 0, &cbBytesReturned, NULL) ) {
    fprintf(stderr, "Failed to map device\n") ; exit(1) ; }

  return (void*)req.mdr_LinearAddress ;
}

void unmap_phys_mem(void *ptr, unsigned size) { }

#else
#include <sys/mman.h>

static int mem=-1;
void *map_phys_mem(unsigned base, unsigned size)
{
  void *ptr;
  if ( (mem = open("/dev/mem",O_RDWR)) == -1) {
    perror("libdha: open(/dev/mem) failed") ; exit(1) ;
  }
  ptr=mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,mem,base) ;
  if ((int)ptr == -1) {
    perror("libdha: mmap() failed") ; exit(1) ;
  }
  return ptr;
}

void unmap_phys_mem(void *ptr, unsigned size)
{
  int res=munmap(ptr,size) ;
  if (res == -1) { perror("libdha: munmap() failed") ; exit(1) ; }
  close(mem);
}
#endif

unsigned char  INPORT8(unsigned idx)
{
  return inb(idx);
}

unsigned short INPORT16(unsigned idx)
{
  return inw(idx);
}

unsigned       INPORT32(unsigned idx)
{
  return inl(idx);
}

void          OUTPORT8(unsigned idx,unsigned char val)
{
  outb(idx,val);
}

void          OUTPORT16(unsigned idx,unsigned short val)
{
  outw(idx,val);
}

void          OUTPORT32(unsigned idx,unsigned val)
{
  outl(idx,val);
}
