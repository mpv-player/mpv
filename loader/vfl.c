/*
 * Copyright 1998 Marcus Meissner
 *
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 *
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "win32.h"
#include "loader.h"

#include "wine/winbase.h"
#include "wine/windef.h"
#include "wine/winuser.h"
#include "wine/vfw.h"
#include "wine/winestring.h"
#include "wine/driver.h"
#include "wine/avifmt.h"
#include "driver.h"

#define OpenDriverA DrvOpen
#define CloseDriver DrvClose

/***********************************************************************
 *		VideoForWindowsVersion		[MSVFW.2][MSVIDEO.2]
 * Returns the version in major.minor form.
 * In Windows95 this returns 0x040003b6 (4.950)
 */
long VFWAPI VideoForWindowsVersion(void) {
	return 0x040003B6; /* 4.950 */
}

/* system.ini: [drivers] */

/***********************************************************************
 *		ICInfo				[MSVFW.33]
 * Get information about an installable compressor. Return TRUE if there
 * is one.
 */
int VFWAPI
ICInfo(
	long fccType,		/* [in] type of compressor ('vidc') */
	long fccHandler,	/* [in] <n>th compressor */
	ICINFO *lpicinfo	/* [out] information about compressor */
) {
	/* does OpenDriver/CloseDriver */
	lpicinfo->dwSize = sizeof(ICINFO);
	lpicinfo->fccType = fccType;
	lpicinfo->dwFlags = 0;
	return TRUE;
}

/***********************************************************************
 *		ICOpen				[MSVFW.37]
 * Opens an installable compressor. Return special handle.
 */
HIC VFWAPI
//ICOpen(long fccType,long fccHandler,unsigned int wMode) {
ICOpen(long filename,long fccHandler,unsigned int wMode) {
	ICOPEN		icopen;
	HDRVR		hdrv;
	WINE_HIC	*whic;

	/* Well, lParam2 is in fact a LPVIDEO_OPEN_PARMS, but it has the 
	 * same layout as ICOPEN
	 */
	icopen.fccType		= 0x63646976; // "vidc" //fccType;
	icopen.fccHandler	= fccHandler;
	icopen.dwSize		= sizeof(ICOPEN);
	icopen.dwFlags		= wMode;
	icopen.pV1Reserved	= (void*)filename;
	/* FIXME: do we need to fill out the rest too? */
	hdrv=OpenDriverA((long)&icopen);
	if (!hdrv) return 0;
	whic = malloc(sizeof(WINE_HIC));
	whic->hdrv	= hdrv;
	whic->driverproc= ((DRVR*)hdrv)->DriverProc;
//	whic->private	= ICSendMessage((HIC)whic,DRV_OPEN,0,(long)&icopen);
	whic->driverid	= ((DRVR*)hdrv)->dwDriverID;
	return (HIC)whic;
}

/***********************************************************************
 *		ICGetInfo			[MSVFW.30]
 */
LRESULT VFWAPI
ICGetInfo(HIC hic,ICINFO *picinfo,long cb) {
	LRESULT		ret;

	ret = ICSendMessage(hic,ICM_GETINFO,(long)picinfo,cb);
	
	return ret;
}

/***********************************************************************
 *		ICCompress			[MSVFW.23]
 */
long VFWAPIV
ICCompress(
	HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiOutput,void* lpData,
	LPBITMAPINFOHEADER lpbiInput,void* lpBits,long* lpckid,
	long* lpdwFlags,long lFrameNum,long dwFrameSize,long dwQuality,
	LPBITMAPINFOHEADER lpbiPrev,void* lpPrev
) {
	ICCOMPRESS	iccmp;

	iccmp.dwFlags		= dwFlags;

	iccmp.lpbiOutput	= lpbiOutput;
	iccmp.lpOutput		= lpData;
	iccmp.lpbiInput		= lpbiInput;
	iccmp.lpInput		= lpBits;

	iccmp.lpckid		= lpckid;
	iccmp.lpdwFlags		= lpdwFlags;
	iccmp.lFrameNum		= lFrameNum;
	iccmp.dwFrameSize	= dwFrameSize;
	iccmp.dwQuality		= dwQuality;
	iccmp.lpbiPrev		= lpbiPrev;
	iccmp.lpPrev		= lpPrev;
	return ICSendMessage(hic,ICM_COMPRESS,(long)&iccmp,sizeof(iccmp));
}

/***********************************************************************
 *		ICDecompress			[MSVFW.26]
 */
long VFWAPIV 
ICDecompress(HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiFormat,void* lpData,LPBITMAPINFOHEADER  lpbi,void* lpBits) {
	ICDECOMPRESS	icd;
	int result;
	icd.dwFlags	= dwFlags;
	icd.lpbiInput	= lpbiFormat;
	icd.lpInput	= lpData;

	icd.lpbiOutput	= lpbi;
	icd.lpOutput	= lpBits;
	icd.ckid	= 0;
	result=ICSendMessage(hic,ICM_DECOMPRESS,(long)&icd,sizeof(icd));
	return result;
}

/***********************************************************************
 *		ICDecompressEx			[MSVFW.26]
 */
long VFWAPIV 
ICDecompressEx(HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiFormat,void* lpData,LPBITMAPINFOHEADER  lpbi,void* lpBits) {
	ICDECOMPRESSEX	icd;
	int result;
	
	icd.dwFlags	= dwFlags;

	icd.lpbiSrc	= lpbiFormat;
	icd.lpSrc	= lpData;

	icd.lpbiDst	= lpbi;
	icd.lpDst	= lpBits;
	
	icd.xSrc=icd.ySrc=0;
	icd.dxSrc=lpbiFormat->biWidth;
	icd.dySrc=abs(lpbiFormat->biHeight);

	icd.xDst=icd.yDst=0;
	icd.dxDst=lpbi->biWidth;
	icd.dyDst=abs(lpbi->biHeight);
	
	//icd.ckid	= 0;
	result=ICSendMessage(hic,ICM_DECOMPRESSEX,(long)&icd,sizeof(icd));
	return result;
}

long VFWAPIV 
ICUniversalEx(HIC hic,int command,LPBITMAPINFOHEADER lpbiFormat,LPBITMAPINFOHEADER lpbi) {
	ICDECOMPRESSEX	icd;
	int result;
	
	icd.dwFlags	= 0;

	icd.lpbiSrc	= lpbiFormat;
	icd.lpSrc	= 0;

	icd.lpbiDst	= lpbi;
	icd.lpDst	= 0;
	
	icd.xSrc=icd.ySrc=0;
	icd.dxSrc=lpbiFormat->biWidth;
	icd.dySrc=abs(lpbiFormat->biHeight);

	icd.xDst=icd.yDst=0;
	icd.dxDst=lpbi->biWidth;
	icd.dyDst=abs(lpbi->biHeight);
	
	//icd.ckid	= 0;
	result=ICSendMessage(hic,command,(long)&icd,sizeof(icd));
	return result;
}


/***********************************************************************
 *		ICSendMessage			[MSVFW.40]
 */
LRESULT VFWAPI
ICSendMessage(HIC hic,unsigned int msg,long lParam1,long lParam2) {
    WINE_HIC	*whic = (WINE_HIC*)hic;
    return SendDriverMessage(whic->hdrv, msg, lParam1,lParam2);
}


/***********************************************************************
 *		ICClose			[MSVFW.22]
 */
LRESULT VFWAPI ICClose(HIC hic) {
	WINE_HIC	*whic = (WINE_HIC*)hic;
	/* FIXME: correct? */
//	CloseDriver(whic->hdrv,0,0);
        DrvClose(whic->hdrv);
//#warning FIXME: DrvClose
	free(whic);
	return 0;
}

int VFWAPI ICDoSomething()
{
  return 0;
}

