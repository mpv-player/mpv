#ifndef MPLAYER_WINEACM_H
#define MPLAYER_WINEACM_H
/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/***********************************************************************
 * Wine specific - Win32
 */


#include "wine/msacmdrv.h"

typedef struct WINE_ACMDRIVERID *PWINE_ACMDRIVERID;
typedef struct WINE_ACMDRIVER   *PWINE_ACMDRIVER;

typedef struct WINE_ACMOBJ
{
  PWINE_ACMDRIVERID	pACMDriverID;
} WINE_ACMOBJ, *PWINE_ACMOBJ;

typedef struct WINE_ACMDRIVER
{
    WINE_ACMOBJ		obj;
    HDRVR      		hDrvr;
    DRIVERPROC		pfnDriverProc;
    PWINE_ACMDRIVER	pNextACMDriver;
    int                 iUsage;
} WINE_ACMDRIVER;

typedef struct WINE_ACMSTREAM
{
    WINE_ACMOBJ		obj;
    PWINE_ACMDRIVER	pDrv;
    ACMDRVSTREAMINSTANCE drvInst;
    HACMDRIVER		hAcmDriver;
} WINE_ACMSTREAM, *PWINE_ACMSTREAM;

typedef struct WINE_ACMDRIVERID
{
    LPSTR               pszFileName;
    WORD		wFormatTag;
    HINSTANCE		hInstModule;          /* NULL if global */
    DWORD		dwProcessID;	      /* ID of process which installed a local driver */
    WIN_BOOL                bEnabled;
    PWINE_ACMDRIVER     pACMDriverList;
    PWINE_ACMDRIVERID   pNextACMDriverID;
    PWINE_ACMDRIVERID	pPrevACMDriverID;
} WINE_ACMDRIVERID;

/* From internal.c */
extern HANDLE MSACM_hHeap;
extern PWINE_ACMDRIVERID MSACM_pFirstACMDriverID;
extern PWINE_ACMDRIVERID MSACM_pLastACMDriverID;

PWINE_ACMDRIVERID MSACM_RegisterDriver(const char* pszFileName,
				       WORD wFormatTag,
				       HINSTANCE hinstModule);
PWINE_ACMDRIVERID MSACM_UnregisterDriver(PWINE_ACMDRIVERID p);
void MSACM_UnregisterAllDrivers(void);
PWINE_ACMDRIVERID MSACM_GetDriverID(HACMDRIVERID hDriverID);
PWINE_ACMDRIVER MSACM_GetDriver(HACMDRIVER hDriver);
PWINE_ACMOBJ MSACM_GetObj(HACMOBJ hObj);

#endif /* MPLAYER_WINEACM_H */
