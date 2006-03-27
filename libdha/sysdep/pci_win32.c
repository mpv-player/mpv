/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/
#include <windows.h>
#include <ddk/ntddk.h>
#include "../dhahelperwin/dhahelper.h"

static HANDLE hDriver;
extern int IsWinNT();





static __inline__ int enable_os_io(void)
{
    if(IsWinNT()){
      DWORD dwBytesReturned;
      hDriver = CreateFile("\\\\.\\DHAHELPER",GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
      if(!DeviceIoControl(hDriver, IOCTL_DHAHELPER_ENABLEDIRECTIO, NULL,0, NULL, 0, &dwBytesReturned, NULL)){
        fprintf(stderr,"Unable to enable directio please install dhahelper.sys.\n");
        return(1);       
      }
    }
    return(0);
}

static __inline__ int disable_os_io(void)
{
    if(IsWinNT()){
      DWORD dwBytesReturned;
      DeviceIoControl(hDriver, IOCTL_DHAHELPER_DISABLEDIRECTIO, NULL,0, NULL, 0, &dwBytesReturned, NULL);
      CloseHandle(hDriver);
    }
    return(0);
}
