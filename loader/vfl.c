/*
 * Copyright 1998 Marcus Meissner
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


#define FIXME_(X) printf
#define FIXME printf

#define OpenDriverA DrvOpen
#define CloseDriver DrvClose

long VFWAPI VideoForWindowsVersion(void);


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
#define	STORE_ALL	/**/
#define	REST_ALL	/**/
#else
#define STORE_ALL \
    __asm__ ( \
    "push %%ebx\n\t" \
    "push %%ecx\n\t" \
    "push %%edx\n\t" \
    "push %%esi\n\t" \
    "push %%edi\n\t"::)

#define REST_ALL \
    __asm__ ( \
    "pop %%edi\n\t" \
    "pop %%esi\n\t" \
    "pop %%edx\n\t" \
    "pop %%ecx\n\t" \
    "pop %%ebx\n\t"::)
#endif


/***********************************************************************
 *		VideoForWindowsVersion		[MSVFW.2][MSVIDEO.2]
 * Returns the version in major.minor form.
 * In Windows95 this returns 0x040003b6 (4.950)
 */
long VideoForWindowsVersion(void) {
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
  char	type[5];

	memcpy(type,&fccType,4);type[4]=0;
	
	/* does OpenDriver/CloseDriver */
	lpicinfo->dwSize = sizeof(ICINFO);
	lpicinfo->fccType = fccType;
	lpicinfo->dwFlags = 0;
/*
	if (GetPrivateProfileStringA("drivers32",NULL,NULL,buf,2000,"system.ini")) {
		char *s = buf;
		while (*s) {
			if (!lstrncmpiA(type,s,4)) {
				if(!fccHandler--) {
					lpicinfo->fccHandler = mmioStringToFOURCCA(s+5,0);
					return TRUE;
				}
			}
			s=s+lstrlenA(s)+1; 
    		}
	}
*/
	return TRUE;
}

/***********************************************************************
 *		ICOpen				[MSVFW.37]
 * Opens an installable compressor. Return special handle.
 */
HIC VFWAPI
ICOpen(long fccType,long fccHandler,unsigned int wMode) {
	char		type[5],handler[5],codecname[20];
	ICOPEN		icopen;
	HDRVR		hdrv;
	WINE_HIC	*whic;

	memcpy(type,&fccType,4);type[4]=0;
	memcpy(handler,&fccHandler,4);handler[4]=0;
	
	snprintf(codecname,20,"%s.%s",type,handler);

	/* Well, lParam2 is in fact a LPVIDEO_OPEN_PARMS, but it has the 
	 * same layout as ICOPEN
	 */
	icopen.fccType		= fccType;
	icopen.fccHandler	= fccHandler;
	icopen.dwSize		= sizeof(ICOPEN);
	icopen.dwFlags		= wMode;
	/* FIXME: do we need to fill out the rest too? */
//	hdrv=OpenDriverA(codecname,"drivers32",(long)&icopen);
	hdrv=OpenDriverA((long)&icopen);
/*
	if (!hdrv) {
	    if (!strcasecmp(type,"vids")) {
		snprintf(codecname,20,"vidc.%s",handler);
		fccType = mmioFOURCC('v','i','d','c');
	    }
//	    hdrv=OpenDriverA(codecname,"drivers32",(long)&icopen);
	    hdrv=OpenDriverA((long)&icopen);
*/
	if (!hdrv)
	    return 0;
//	}
	whic = (WINE_HIC*)my_mreq(sizeof(WINE_HIC), 0);
	whic->hdrv	= hdrv;
	whic->driverproc= ((DRVR*)hdrv)->DriverProc;
//	whic->private	= ICSendMessage((HIC)whic,DRV_OPEN,0,(long)&icopen);
	whic->private	= ((DRVR*)hdrv)->dwDriverID;
	return (HIC)whic;
}

/***********************************************************************
 *		ICOpenFunction			[MSVFW.38]
 */
HIC VFWAPI ICOpenFunction(long fccType, long fccHandler, unsigned int wMode,
void* lpfnHandler) {
	char		type[5],handler[5];
	HIC		hic;
	WINE_HIC	*whic;

	memcpy(type,&fccType,4);type[4]=0;
	memcpy(handler,&fccHandler,4);handler[4]=0;
	FIXME("(%s,%s,%d,%p), stub!\n",type,handler,wMode,lpfnHandler);
	hic = ICOpen(fccType,fccHandler,wMode);
	if (!hic)
		return hic;
	whic = (WINE_HIC*)hic;
	whic->driverproc = (DRIVERPROC)lpfnHandler;
	return hic;
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
 *		ICLocate			[MSVFW.35]
 */
HIC  VFWAPI
ICLocate(
	long fccType, long fccHandler, LPBITMAPINFOHEADER lpbiIn,
	LPBITMAPINFOHEADER lpbiOut, short wMode
) {
	char	type[5],handler[5];
	HIC	hic;
	long	querymsg;

	switch (wMode) {
	case ICMODE_FASTCOMPRESS:
	case ICMODE_COMPRESS: 
		querymsg = ICM_COMPRESS_QUERY;
		break;
	case ICMODE_DECOMPRESS:
	case ICMODE_FASTDECOMPRESS:
		querymsg = ICM_DECOMPRESS_QUERY;
		break;
	case ICMODE_DRAW:
		querymsg = ICM_DRAW_QUERY;
		break;
	default:
		FIXME("Unknown mode (%d)\n",wMode);
		return 0;
	}

	/* Easy case: handler/type match, we just fire a query and return */
	hic = ICOpen(fccType,fccHandler,wMode);
	if (hic) {
		if (!ICSendMessage(hic,querymsg,(long)lpbiIn,(long)lpbiOut))
			return hic;
		ICClose(hic);
	}
	type[4]='\0';memcpy(type,&fccType,4);
	handler[4]='\0';memcpy(handler,&fccHandler,4);
	if (fccType==streamtypeVIDEO) {
		hic = ICLocate(ICTYPE_VIDEO,fccHandler,lpbiIn,lpbiOut,wMode);
		if (hic)
			return hic;
	}
	FIXME("(%s,%s,%p,%p,0x%04x),unhandled!\n",type,handler,lpbiIn,lpbiOut,wMode);
	return 0;
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
	STORE_ALL;
	result=ICSendMessage(hic,ICM_DECOMPRESS,(long)&icd,sizeof(icd));
	REST_ALL;
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
	STORE_ALL;
	result=ICSendMessage(hic,ICM_DECOMPRESSEX,(long)&icd,sizeof(icd));
	REST_ALL;
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
	STORE_ALL;
	result=ICSendMessage(hic,command,(long)&icd,sizeof(icd));
	REST_ALL;
	return result;
}


/***********************************************************************
 *		ICSendMessage			[MSVFW.40]
 */
LRESULT VFWAPI
ICSendMessage(HIC hic,unsigned int msg,long lParam1,long lParam2) {
	LRESULT		ret;
	WINE_HIC	*whic = (WINE_HIC*)hic;
	char qw[200];

#if 0
    __asm__ __volatile__ ("fsave (%0)\n\t": :"r"(&qw));    
#endif
    STORE_ALL;	
        /*__asm__
	(
	    "pushl %eax\n\t"
    	    "movl $0xf,%eax\n\t"
	    "movw %ax, %fs\n\t"
	    "popl %eax\n\t"
        );*/
    	ret = whic->driverproc(whic->private,1,msg,lParam1,lParam2);
    REST_ALL;	
#if 0
    __asm__ __volatile__ ("frstor (%0)\n\t": :"r"(&qw));    
#endif
//	} else

//		ret = SendDriverMessage(whic->hdrv,msg,lParam1,lParam2);
//	TRACE("	-> 0x%08lx\n",ret);
	return ret;
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
	my_release(whic);
	return 0;
}
int VFWAPI ICDoSomething()
{
  return 0;
}

