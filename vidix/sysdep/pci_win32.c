/*
   This file is based on:
   $XFree86: xc/programs/Xserver/hw/xfree86/etc/scanpci.c,v 3.34.2.17 1998/11/10 11:55:40 dawes Exp $
   Modified for readability by Nick Kurshev
*/
/*
 * Copyright 1995 by Robin Cutshaw <robin@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of the above listed copyright holder(s)
 * not be used in advertising or publicity pertaining to distribution of
 * the software without specific, written prior permission.  The above listed
 * copyright holder(s) make(s) no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM(S) ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <windows.h>
#include <ddk/ntddk.h>
#include "vidix/dhahelperwin/dhahelper.h"

static HANDLE hDriver;
int IsWinNT();





static __inline__ int enable_os_io(void)
{
    if(IsWinNT()){
      DWORD dwBytesReturned;
      hDriver = CreateFile("\\\\.\\DHAHELPER",GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
      if(!DeviceIoControl(hDriver, IOCTL_DHAHELPER_ENABLEDIRECTIO, NULL,0, NULL, 0, &dwBytesReturned, NULL)){
        fprintf(stderr,"Unable to enable directio please install dhahelper.sys.\n");
        return 1;
      }
    }
    return 0;
}

static __inline__ int disable_os_io(void)
{
    if(IsWinNT()){
      DWORD dwBytesReturned;
      DeviceIoControl(hDriver, IOCTL_DHAHELPER_DISABLEDIRECTIO, NULL,0, NULL, 0, &dwBytesReturned, NULL);
      CloseHandle(hDriver);
    }
    return 0;
}
