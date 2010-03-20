/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#include "config.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef __FreeBSD__
#include <sys/time.h>
#endif

#include "win32.h"
#include "wine/driver.h"
#include "wine/pe_image.h"
#include "wine/winreg.h"
#include "wine/vfw.h"
#include "registry.h"
#ifdef WIN32_LOADER
#include "ldt_keeper.h"
#endif
#include "drv.h"
#ifndef __MINGW32__
#include "ext.h"
#endif
#include "path.h"

#if 1

/*
 * STORE_ALL/REST_ALL seems like an attempt to workaround problems due to
 * WINAPI/no-WINAPI bustage.
 *
 * There should be no need for the STORE_ALL/REST_ALL hack once all
 * function definitions agree with their prototypes (WINAPI-wise) and
 * we make sure, that we do not call these functions without a proper
 * prototype in scope.
 */

#define STORE_ALL
#define REST_ALL
#else
// this asm code is no longer needed
#define STORE_ALL \
    __asm__ volatile ( \
    "push %%ebx\n\t" \
    "push %%ecx\n\t" \
    "push %%edx\n\t" \
    "push %%esi\n\t" \
    "push %%edi\n\t"::)

#define REST_ALL \
    __asm__ volatile ( \
    "pop %%edi\n\t" \
    "pop %%esi\n\t" \
    "pop %%edx\n\t" \
    "pop %%ecx\n\t" \
    "pop %%ebx\n\t"::)
#endif

static DWORD dwDrvID = 0;

LRESULT WINAPI SendDriverMessage(HDRVR hDriver, UINT message,
				 LPARAM lParam1, LPARAM lParam2)
{
    DRVR* module=(DRVR*)hDriver;
    int result;
#ifndef __svr4__
    char qw[300];
#endif
#ifdef DETAILED_OUT
    printf("SendDriverMessage: driver %X, message %X, arg1 %X, arg2 %X\n", hDriver, message, lParam1, lParam2);
#endif
    if (!module || !module->hDriverModule || !module->DriverProc) return -1;
#ifndef __svr4__
    __asm__ volatile ("fsave (%0)\n\t": :"r"(&qw));
#endif

#ifdef WIN32_LOADER
    Setup_FS_Segment();
#endif

    STORE_ALL;
    result=module->DriverProc(module->dwDriverID, hDriver, message, lParam1, lParam2);
    REST_ALL;

#ifndef __svr4__
    __asm__ volatile ("frstor (%0)\n\t": :"r"(&qw));
#endif

#ifdef DETAILED_OUT
    printf("\t\tResult: %X\n", result);
#endif
    return result;
}

void DrvClose(HDRVR hDriver)
{
    if (hDriver)
    {
	DRVR* d = (DRVR*)hDriver;
	if (d->hDriverModule)
	{
#ifdef WIN32_LOADER
	    Setup_FS_Segment();
#endif
	    if (d->DriverProc)
	    {
		SendDriverMessage(hDriver, DRV_CLOSE, 0, 0);
		d->dwDriverID = 0;
		SendDriverMessage(hDriver, DRV_FREE, 0, 0);
	    }
	    FreeLibrary(d->hDriverModule);
	}
	free(d);
    }
#ifdef WIN32_LOADER
    CodecRelease();
#endif
}

//DrvOpen(LPCSTR lpszDriverName, LPCSTR lpszSectionName, LPARAM lParam2)
HDRVR DrvOpen(LPARAM lParam2)
{
    NPDRVR hDriver;
    char unknown[0x124];
    const char* filename = (const char*) ((ICOPEN*) lParam2)->pV1Reserved;

#ifdef WIN32_LOADER
    Setup_LDT_Keeper();
#endif
    printf("Loading codec DLL: '%s'\n",filename);

    hDriver = malloc(sizeof(DRVR));
    if (!hDriver)
	return (HDRVR) 0;
    memset((void*)hDriver, 0, sizeof(DRVR));

#ifdef WIN32_LOADER
    CodecAlloc();
    Setup_FS_Segment();
#endif

    hDriver->hDriverModule = LoadLibraryA(filename);
    if (!hDriver->hDriverModule)
    {
	printf("Can't open library %s\n", filename);
	DrvClose((HDRVR)hDriver);
	return (HDRVR) 0;
    }

    hDriver->DriverProc = (DRIVERPROC) GetProcAddress(hDriver->hDriverModule,
						      "DriverProc");
    if (!hDriver->DriverProc)
    {
	printf("Library %s is not a valid VfW/ACM codec\n", filename);
	DrvClose((HDRVR)hDriver);
	return (HDRVR) 0;
    }

    TRACE("DriverProc == %X\n", hDriver->DriverProc);
    SendDriverMessage((HDRVR)hDriver, DRV_LOAD, 0, 0);
    TRACE("DRV_LOAD Ok!\n");
    SendDriverMessage((HDRVR)hDriver, DRV_ENABLE, 0, 0);
    TRACE("DRV_ENABLE Ok!\n");
    hDriver->dwDriverID = ++dwDrvID; // generate new id

    // open driver and remmeber proper DriverID
    hDriver->dwDriverID = SendDriverMessage((HDRVR)hDriver, DRV_OPEN, (LPARAM) unknown, lParam2);
    TRACE("DRV_OPEN Ok!(%X)\n", hDriver->dwDriverID);

    printf("Loaded DLL driver %s at %x\n", filename, hDriver->hDriverModule);
    return (HDRVR)hDriver;
}
