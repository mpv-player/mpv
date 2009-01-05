/*
  MAPDEV.h - include file for VxD MAPDEV
  Copyright (c) 1996 Vireo Software, Inc.
  Modified for libdha by Nick Kurshev.
*/

#include <windows.h>
#include <ddk/ntddk.h>
#include "vidix/dhahelperwin/dhahelper.h"

/*
  This is the request structure that applications use
  to request services from the MAPDEV VxD.
*/

typedef struct MapDevRequest
{
	DWORD	mdr_ServiceID;		/* supplied by caller */
	LPVOID	mdr_PhysicalAddress;	/* supplied by caller */
	DWORD	mdr_SizeInBytes;	/* supplied by caller */
	LPVOID	mdr_LinearAddress;	/* returned by VxD */
	WORD	mdr_Selector;		/* returned if 16-bit caller */
	WORD	mdr_Status;		/* MDR_xxxx code below */
} MAPDEVREQUEST, *PMAPDEVREQUEST;

#define MDR_SERVICE_MAP		CTL_CODE(FILE_DEVICE_UNKNOWN, 1, METHOD_NEITHER, FILE_ANY_ACCESS)
#define MDR_SERVICE_UNMAP	CTL_CODE(FILE_DEVICE_UNKNOWN, 2, METHOD_NEITHER, FILE_ANY_ACCESS)

#define MDR_STATUS_SUCCESS	1
#define MDR_STATUS_ERROR	0
/*#include "winioctl.h"*/
#define FILE_DEVICE_UNKNOWN             0x00000022
#define METHOD_NEITHER                  3

    
int IsWinNT(void) {
  OSVERSIONINFO OSVersionInfo;
  OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&OSVersionInfo);
  return OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT;
}  

static HANDLE hDriver = INVALID_HANDLE_VALUE;  
    
    
/* Memory Map a piece of Real Memory */
void *map_phys_mem(unsigned long base, unsigned long size) {
  if(!IsWinNT()){
  HANDLE hDevice ;
  PVOID inBuf[1] ;		/* buffer for struct pointer to VxD */
  DWORD cbBytesReturned ;	/* count of bytes returned from VxD */
  MAPDEVREQUEST req ;		/* map device request structure */
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
  else{
    dhahelper_t dhahelper_priv;
    DWORD dwBytesReturned;
    dhahelper_priv.size = size;
    dhahelper_priv.base = base;
    if(hDriver==INVALID_HANDLE_VALUE)hDriver = CreateFile("\\\\.\\DHAHELPER",GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (!DeviceIoControl(hDriver, IOCTL_DHAHELPER_MAPPHYSTOLIN, &dhahelper_priv,sizeof(dhahelper_t), &dhahelper_priv, sizeof(dhahelper_t),&dwBytesReturned, NULL)){
      fprintf(stderr,"Unable to map the requested memory region.\n");
      return NULL;
    }
    else return dhahelper_priv.ptr;
  }
}

void unmap_phys_mem(void *ptr, unsigned long size) {
  if(IsWinNT()){
    dhahelper_t dhahelper_priv;
    DWORD dwBytesReturned;
    dhahelper_priv.ptr = ptr;
    DeviceIoControl(hDriver, IOCTL_DHAHELPER_UNMAPPHYSADDR, &dhahelper_priv,sizeof(dhahelper_t), NULL, 0, &dwBytesReturned, NULL);
  }
}
