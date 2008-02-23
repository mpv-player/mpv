/*
 * Drivers definitions
 */

#ifndef MPLAYER_DRIVER_H
#define MPLAYER_DRIVER_H

#include "windef.h"

#define MMSYSERR_BASE          0

#define MMSYSERR_NOERROR      	0                    /* no error */
#define MMSYSERR_ERROR        	(MMSYSERR_BASE + 1)  /* unspecified error */
#define MMSYSERR_BADDEVICEID  	(MMSYSERR_BASE + 2)  /* device ID out of range */
#define MMSYSERR_NOTENABLED   	(MMSYSERR_BASE + 3)  /* driver failed enable */
#define MMSYSERR_ALLOCATED    	(MMSYSERR_BASE + 4)  /* device already allocated */
#define MMSYSERR_INVALHANDLE  	(MMSYSERR_BASE + 5)  /* device handle is invalid */
#define MMSYSERR_NODRIVER     	(MMSYSERR_BASE + 6)  /* no device driver present */
#define MMSYSERR_NOMEM        	(MMSYSERR_BASE + 7)  /* memory allocation error */
#define MMSYSERR_NOTSUPPORTED 	(MMSYSERR_BASE + 8)  /* function isn't supported */
#define MMSYSERR_BADERRNUM    	(MMSYSERR_BASE + 9)  /* error value out of range */
#define MMSYSERR_INVALFLAG    	(MMSYSERR_BASE + 10) /* invalid flag passed */
#define MMSYSERR_INVALPARAM   	(MMSYSERR_BASE + 11) /* invalid parameter passed */
#define MMSYSERR_LASTERROR    	(MMSYSERR_BASE + 11) /* last error in range */

#define DRV_LOAD                0x0001
#define DRV_ENABLE              0x0002
#define DRV_OPEN                0x0003
#define DRV_CLOSE               0x0004
#define DRV_DISABLE             0x0005
#define DRV_FREE                0x0006
#define DRV_CONFIGURE           0x0007
#define DRV_QUERYCONFIGURE      0x0008
#define DRV_INSTALL             0x0009
#define DRV_REMOVE              0x000A
#define DRV_EXITSESSION         0x000B
#define DRV_EXITAPPLICATION     0x000C
#define DRV_POWER               0x000F

#define DRV_RESERVED            0x0800
#define DRV_USER                0x4000

#define DRVCNF_CANCEL           0x0000
#define DRVCNF_OK               0x0001
#define DRVCNF_RESTART 		0x0002

#define DRVEA_NORMALEXIT  	0x0001
#define DRVEA_ABNORMALEXIT 	0x0002

#define DRV_SUCCESS		0x0001
#define DRV_FAILURE		0x0000

#define GND_FIRSTINSTANCEONLY 	0x00000001

#define GND_FORWARD  		0x00000000
#define GND_REVERSE    		0x00000002

typedef struct {
    DWORD   			dwDCISize;
    LPCSTR  			lpszDCISectionName;
    LPCSTR  			lpszDCIAliasName;
} DRVCONFIGINFO16, *LPDRVCONFIGINFO16;

typedef struct {
    DWORD   			dwDCISize;
    LPCWSTR  			lpszDCISectionName;
    LPCWSTR  			lpszDCIAliasName;
} DRVCONFIGINFO, *LPDRVCONFIGINFO;


/* GetDriverInfo16 references this structure, so this a struct defined
 * in the Win16 API.
 * GetDriverInfo has been deprecated in Win32.
 */
typedef struct
{
    UINT16       		length;
    HDRVR16      		hDriver;
    HINSTANCE16  		hModule;
    CHAR         		szAliasName[128];
} DRIVERINFOSTRUCT16, *LPDRIVERINFOSTRUCT16;

LRESULT WINAPI DefDriverProc16(DWORD dwDevID, HDRVR16 hDriv, UINT16 wMsg, 
                               LPARAM dwParam1, LPARAM dwParam2);
LRESULT WINAPI DefDriverProc(DWORD dwDriverIdentifier, HDRVR hdrvr,
                               UINT Msg, LPARAM lParam1, LPARAM lParam2);
HDRVR16 WINAPI OpenDriver16(LPCSTR szDriverName, LPCSTR szSectionName,
                            LPARAM lParam2);
HDRVR WINAPI OpenDriverA(LPCSTR szDriverName, LPCSTR szSectionName,
                             LPARAM lParam2);
HDRVR WINAPI OpenDriverW(LPCWSTR szDriverName, LPCWSTR szSectionName,
                             LPARAM lParam2);
#define OpenDriver WINELIB_NAME_AW(OpenDriver)
LRESULT WINAPI CloseDriver16(HDRVR16 hDriver, LPARAM lParam1, LPARAM lParam2);
LRESULT WINAPI CloseDriver(HDRVR hDriver, LPARAM lParam1, LPARAM lParam2);
LRESULT WINAPI SendDriverMessage16( HDRVR16 hDriver, UINT16 message,
                                    LPARAM lParam1, LPARAM lParam2 );
LRESULT WINAPI SendDriverMessage( HDRVR hDriver, UINT message,
                                    LPARAM lParam1, LPARAM lParam2 );
HMODULE16 WINAPI GetDriverModuleHandle16(HDRVR16 hDriver);
HMODULE WINAPI GetDriverModuleHandle(HDRVR hDriver);

DWORD WINAPI GetDriverFlags( HDRVR hDriver );
/* this call (GetDriverFlags) is not documented, nor the flags returned.
 * here are Wine only definitions
 */
#define WINE_GDF_EXIST	0x80000000
#define WINE_GDF_16BIT	0x10000000

#endif /* MPLAYER_DRIVER_H */
