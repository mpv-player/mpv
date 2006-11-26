/**************************************************************************


  This file will contain an interface to ACM drivers.
  Its content will be based mainly on wine/dlls/msacm32
  actually, for audio decompression only the following functions
  are needed:
  
  acmStreamOpen ( takes formats of src and dest, returns stream handle )
  acmStreamPrepareHeader ( takes stream handler and info on data )
  acmStreamConvert ( the same as PrepareHeader )
  acmStreamUnprepareHeader
  acmStreamClose
  acmStreamSize
  maybe acmStreamReset
  
  In future I'll also add functions for format enumeration, 
  but not right now.

  Modified for use with MPlayer, detailed changelog at
  http://svn.mplayerhq.hu/mplayer/trunk/
  $Id$
  
***************************************************************************/
#include "config.h"
#include "debug.h"

#include "wine/winbase.h"
#include "wine/windef.h"
#include "wine/winuser.h"
#include "wine/vfw.h"
#include "wine/winestring.h"
#include "wine/driver.h"
#include "wine/winerror.h"
#include "wine/msacm.h"
#include "wine/msacmdrv.h"
#include "wineacm.h"
#ifndef __MINGW32__
#include "ext.h"
#endif
#include "driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma pack(1)
#define OpenDriverA DrvOpen
#define CloseDriver DrvClose

static inline PWINE_ACMSTREAM ACM_GetStream(HACMSTREAM has)
{
    return (PWINE_ACMSTREAM)has;
}

/***********************************************************************
 *           acmDriverAddA (MSACM32.2)
 */
MMRESULT WINAPI acmDriverAddA(PHACMDRIVERID phadid, HINSTANCE hinstModule,
			      LPARAM lParam, DWORD dwPriority, DWORD fdwAdd)
{
    if (!phadid)
	return MMSYSERR_INVALPARAM;
    
    /* Check if any unknown flags */
    if (fdwAdd & 
	~(ACM_DRIVERADDF_FUNCTION|ACM_DRIVERADDF_NOTIFYHWND|
	  ACM_DRIVERADDF_GLOBAL))
	return MMSYSERR_INVALFLAG;
    
    /* Check if any incompatible flags */
    if ((fdwAdd & ACM_DRIVERADDF_FUNCTION) && 
	(fdwAdd & ACM_DRIVERADDF_NOTIFYHWND))
	return MMSYSERR_INVALFLAG;
    
    /* FIXME: in fact, should GetModuleFileName(hinstModule) and do a 
     * LoadDriver on it, to be sure we can call SendDriverMessage on the
     * hDrvr handle.
     */
    *phadid = (HACMDRIVERID) MSACM_RegisterDriver(NULL, 0, hinstModule);
    
    /* FIXME: lParam, dwPriority and fdwAdd ignored */
    
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           acmDriverClose (MSACM32.4)
 */
MMRESULT WINAPI acmDriverClose(HACMDRIVER had, DWORD fdwClose)
{
    PWINE_ACMDRIVER  p;
    PWINE_ACMDRIVER* tp;
    
    if (fdwClose)
	return MMSYSERR_INVALFLAG;
    
    p = MSACM_GetDriver(had);
    if (!p)
	return MMSYSERR_INVALHANDLE;

    for (tp = &(p->obj.pACMDriverID->pACMDriverList); *tp; *tp = (*tp)->pNextACMDriver) {
	if (*tp == p) {
	    *tp = (*tp)->pNextACMDriver;
	    break;
	}
    }
    
    if (p->hDrvr && !p->obj.pACMDriverID->pACMDriverList)
	CloseDriver(p->hDrvr);
    
    HeapFree(MSACM_hHeap, 0, p);
    
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           acmDriverEnum (MSACM32.7)
 */
MMRESULT WINAPI acmDriverEnum(ACMDRIVERENUMCB fnCallback, DWORD dwInstance, DWORD fdwEnum)
{
    PWINE_ACMDRIVERID	p;
    DWORD		fdwSupport;

    if (!fnCallback) {
	return MMSYSERR_INVALPARAM;
    }
    
    if (fdwEnum && ~(ACM_DRIVERENUMF_NOLOCAL|ACM_DRIVERENUMF_DISABLED)) {
	return MMSYSERR_INVALFLAG;
    }
    
    for (p = MSACM_pFirstACMDriverID; p; p = p->pNextACMDriverID) {
	fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CODEC;
	if (!p->bEnabled) {
	    if (fdwEnum & ACM_DRIVERENUMF_DISABLED)
		fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_DISABLED;
	    else
		continue;
	}
	(*fnCallback)((HACMDRIVERID) p, dwInstance, fdwSupport);
    }
    
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           acmDriverID (MSACM32.8)
 */
MMRESULT WINAPI acmDriverID(HACMOBJ hao, PHACMDRIVERID phadid, DWORD fdwDriverID)
{
    PWINE_ACMOBJ pao;
    
    pao = MSACM_GetObj(hao);
    if (!pao)
	return MMSYSERR_INVALHANDLE;
    
    if (!phadid)
	return MMSYSERR_INVALPARAM;
    
    if (fdwDriverID)
	return MMSYSERR_INVALFLAG;
    
    *phadid = (HACMDRIVERID) pao->pACMDriverID;
    
    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           acmDriverMessage (MSACM32.9)
 * FIXME
 *   Not implemented
 */
LRESULT WINAPI acmDriverMessage(HACMDRIVER had, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
    PWINE_ACMDRIVER pad = MSACM_GetDriver(had);
    if (!pad)
	return MMSYSERR_INVALPARAM;
    
    /* FIXME: Check if uMsg legal */
    
    if (!SendDriverMessage(pad->hDrvr, uMsg, lParam1, lParam2))
	return MMSYSERR_NOTSUPPORTED;
    
    return MMSYSERR_NOERROR;
}


/***********************************************************************
 *           acmDriverOpen (MSACM32.10)
 */
MMRESULT WINAPI acmDriverOpen(PHACMDRIVER phad, HACMDRIVERID hadid, DWORD fdwOpen)
{
    PWINE_ACMDRIVERID	padid;
    PWINE_ACMDRIVER	pad;
    ICOPEN		icopen;
    HDRVR		hdrv;


    TRACE("(%p, %x, %08lu)\n", phad, hadid, fdwOpen);

    if (!phad)
	return MMSYSERR_INVALPARAM;
    
    padid = MSACM_GetDriverID(hadid); 
    if (!padid)
	return MMSYSERR_INVALHANDLE;
    
    if (fdwOpen)
	return MMSYSERR_INVALFLAG;
    
    pad = (PWINE_ACMDRIVER) HeapAlloc(MSACM_hHeap, 0, sizeof(WINE_ACMDRIVER));
    if (!pad)
	return MMSYSERR_NOMEM;

    pad->obj.pACMDriverID = padid;
    icopen.fccType		= mmioFOURCC('a', 'u', 'd', 'c');
    icopen.fccHandler		= (long)padid->pszFileName;
    icopen.dwSize		= sizeof(ICOPEN);
    icopen.dwFlags		= 0;

    icopen.pV1Reserved = padid->pszFileName;
    if (!padid->hInstModule)
	pad->hDrvr = OpenDriverA((long)&icopen);
    else
	pad->hDrvr = padid->hInstModule;
    
    if (!pad->hDrvr) {
	HeapFree(MSACM_hHeap, 0, pad);
	return MMSYSERR_ERROR;
    }

    pad->pfnDriverProc = GetProcAddress(pad->hDrvr, "DriverProc");

    /* insert new pad at beg of list */
    pad->pNextACMDriver = padid->pACMDriverList;
    padid->pACMDriverList = pad;

    /* FIXME: Create a WINE_ACMDRIVER32 */
    *phad = (HACMDRIVER)pad;

    return MMSYSERR_NOERROR;
}

/***********************************************************************
 *           acmDriverRemove (MSACM32.12)
 */
MMRESULT WINAPI acmDriverRemove(HACMDRIVERID hadid, DWORD fdwRemove)
{
    PWINE_ACMDRIVERID padid;
    
    padid = MSACM_GetDriverID(hadid);
    if (!padid)
	return MMSYSERR_INVALHANDLE;
    
    if (fdwRemove)
	return MMSYSERR_INVALFLAG;
    
    MSACM_UnregisterDriver(padid);
    
    return MMSYSERR_NOERROR;
}



/**********************************************************************/

HANDLE MSACM_hHeap = (HANDLE) NULL;
PWINE_ACMDRIVERID MSACM_pFirstACMDriverID = NULL;
PWINE_ACMDRIVERID MSACM_pLastACMDriverID = NULL;

/***********************************************************************
 *           MSACM_RegisterDriver32() 
 */
PWINE_ACMDRIVERID MSACM_RegisterDriver(const char* pszFileName,
				       WORD wFormatTag,
				       HINSTANCE hinstModule)
{
    PWINE_ACMDRIVERID padid;

    TRACE("('%s', '%x', 0x%08x)\n", pszFileName, wFormatTag, hinstModule);

#ifndef WIN32_LOADER
	MSACM_hHeap = GetProcessHeap();
#endif
    padid = (PWINE_ACMDRIVERID) HeapAlloc(MSACM_hHeap, 0, sizeof(WINE_ACMDRIVERID));
    padid->pszFileName = malloc(strlen(pszFileName)+1);
    strcpy(padid->pszFileName, pszFileName);
//    1~strdup(pszDriverAlias);
    padid->wFormatTag = wFormatTag;
    padid->hInstModule = hinstModule;
    padid->bEnabled = TRUE;
    padid->pACMDriverList = NULL;
    padid->pNextACMDriverID = NULL;
    padid->pPrevACMDriverID = MSACM_pLastACMDriverID;
    if (MSACM_pLastACMDriverID)
	MSACM_pLastACMDriverID->pNextACMDriverID = padid;
    MSACM_pLastACMDriverID = padid;
    if (!MSACM_pFirstACMDriverID)
	MSACM_pFirstACMDriverID = padid;
    
    return padid;
}


/***********************************************************************
 *           MSACM_UnregisterDriver32()
 */
PWINE_ACMDRIVERID MSACM_UnregisterDriver(PWINE_ACMDRIVERID p)
{
    PWINE_ACMDRIVERID pNextACMDriverID;
    
    while (p->pACMDriverList)
	acmDriverClose((HACMDRIVER) p->pACMDriverList, 0);
    
    if (p->pszFileName)
	free(p->pszFileName);
    
    if (p == MSACM_pFirstACMDriverID)
	MSACM_pFirstACMDriverID = p->pNextACMDriverID;
    if (p == MSACM_pLastACMDriverID)
	MSACM_pLastACMDriverID = p->pPrevACMDriverID;

    if (p->pPrevACMDriverID)
	p->pPrevACMDriverID->pNextACMDriverID = p->pNextACMDriverID;
    if (p->pNextACMDriverID)
	p->pNextACMDriverID->pPrevACMDriverID = p->pPrevACMDriverID;
    
    pNextACMDriverID = p->pNextACMDriverID;
    
    HeapFree(MSACM_hHeap, 0, p);
    
    return pNextACMDriverID;
}

/***********************************************************************
 *           MSACM_UnregisterAllDrivers32()
 * FIXME
 *   Where should this function be called?
 */
void MSACM_UnregisterAllDrivers(void)
{
    PWINE_ACMDRIVERID p;

    for (p = MSACM_pFirstACMDriverID; p; p = MSACM_UnregisterDriver(p));
}

/***********************************************************************
 *           MSACM_GetDriverID32() 
 */
PWINE_ACMDRIVERID MSACM_GetDriverID(HACMDRIVERID hDriverID)
{
    return (PWINE_ACMDRIVERID)hDriverID;
}

/***********************************************************************
 *           MSACM_GetDriver32()
 */
PWINE_ACMDRIVER MSACM_GetDriver(HACMDRIVER hDriver)
{
    return (PWINE_ACMDRIVER)hDriver;
}

/***********************************************************************
 *           MSACM_GetObj32()
 */
PWINE_ACMOBJ MSACM_GetObj(HACMOBJ hObj)
{
    return (PWINE_ACMOBJ)hObj;
}



/***********************************************************************
 *           acmStreamOpen (MSACM32.40)
 */
MMRESULT WINAPI acmStreamOpen(PHACMSTREAM phas, HACMDRIVER had, PWAVEFORMATEX pwfxSrc,
			      PWAVEFORMATEX pwfxDst, PWAVEFILTER pwfltr, DWORD dwCallback,
			      DWORD dwInstance, DWORD fdwOpen)
{
    PWINE_ACMSTREAM	was;
    PWINE_ACMDRIVER	wad;
    MMRESULT		ret;
    int			wfxSrcSize;
    int			wfxDstSize;
    
    TRACE("(%p, 0x%08x, %p, %p, %p, %ld, %ld, %ld)\n",
	  phas, had, pwfxSrc, pwfxDst, pwfltr, dwCallback, dwInstance, fdwOpen);

    TRACE("src [wFormatTag=%u, nChannels=%u, nSamplesPerSec=%lu, nAvgBytesPerSec=%lu, nBlockAlign=%u, wBitsPerSample=%u, cbSize=%u]\n", 
	  pwfxSrc->wFormatTag, pwfxSrc->nChannels, pwfxSrc->nSamplesPerSec, pwfxSrc->nAvgBytesPerSec, 
	  pwfxSrc->nBlockAlign, pwfxSrc->wBitsPerSample, pwfxSrc->cbSize);

    TRACE("dst [wFormatTag=%u, nChannels=%u, nSamplesPerSec=%lu, nAvgBytesPerSec=%lu, nBlockAlign=%u, wBitsPerSample=%u, cbSize=%u]\n", 
	  pwfxDst->wFormatTag, pwfxDst->nChannels, pwfxDst->nSamplesPerSec, pwfxDst->nAvgBytesPerSec, 
	  pwfxDst->nBlockAlign, pwfxDst->wBitsPerSample, pwfxDst->cbSize);

#define SIZEOF_WFX(wfx) (sizeof(WAVEFORMATEX) + ((wfx->wFormatTag == WAVE_FORMAT_PCM) ? 0 : wfx->cbSize))
    wfxSrcSize = SIZEOF_WFX(pwfxSrc);
    wfxDstSize = SIZEOF_WFX(pwfxDst);
#undef SIZEOF_WFX

    was = (PWINE_ACMSTREAM) HeapAlloc(MSACM_hHeap, 0, sizeof(*was) + wfxSrcSize + wfxDstSize + ((pwfltr) ? sizeof(WAVEFILTER) : 0));
    if (was == NULL)
	return MMSYSERR_NOMEM;
    was->drvInst.cbStruct = sizeof(was->drvInst);
    was->drvInst.pwfxSrc = (PWAVEFORMATEX)((LPSTR)was + sizeof(*was));
    memcpy(was->drvInst.pwfxSrc, pwfxSrc, wfxSrcSize);
    // LHACM is checking for 0x1
    // but if this will not help
    // was->drvInst.pwfxSrc->wFormatTag = 1;
    was->drvInst.pwfxDst = (PWAVEFORMATEX)((LPSTR)was + sizeof(*was) + wfxSrcSize);
    memcpy(was->drvInst.pwfxDst, pwfxDst, wfxDstSize);
    if (pwfltr) {
	was->drvInst.pwfltr = (PWAVEFILTER)((LPSTR)was + sizeof(*was) + wfxSrcSize + wfxDstSize);
	memcpy(was->drvInst.pwfltr, pwfltr, sizeof(WAVEFILTER));
    } else {
	was->drvInst.pwfltr = NULL;
    }
    was->drvInst.dwCallback = dwCallback;    
    was->drvInst.dwInstance = dwInstance;
    was->drvInst.fdwOpen = fdwOpen;
    was->drvInst.fdwDriver = 0L;  
    was->drvInst.dwDriver = 0L;     
    was->drvInst.has = (HACMSTREAM)was;
    
    if (had) {
	if (!(wad = MSACM_GetDriver(had))) {
	    ret = MMSYSERR_INVALPARAM;
	    goto errCleanUp;
	}
	
	was->obj.pACMDriverID = wad->obj.pACMDriverID;
	was->pDrv = wad;
	was->hAcmDriver = 0; /* not to close it in acmStreamClose */

	ret = SendDriverMessage(wad->hDrvr, ACMDM_STREAM_OPEN, (DWORD)&was->drvInst, 0L);
	if (ret != MMSYSERR_NOERROR)
	    goto errCleanUp;
    } else {
	PWINE_ACMDRIVERID wadi;
	short drv_tag;
	ret = ACMERR_NOTPOSSIBLE;
/*	if(pwfxSrc->wFormatTag==1)//compression
	    drv_tag=pwfxDst->wFormatTag;
	    else
	    if(pwfxDst->wFormatTag==1)//decompression
		drv_tag=pwfxSrc->wFormatTag;
		else
		goto errCleanUp;

	    ret=acmDriverOpen2(drv_tag); 
	    if (ret == MMSYSERR_NOERROR) {
		if ((wad = MSACM_GetDriver(had)) != 0) {
		    was->obj.pACMDriverID = wad->obj.pACMDriverID;
		    was->pDrv = wad;
		    was->hAcmDriver = had;
		    
		    ret = SendDriverMessage(wad->hDrvr, ACMDM_STREAM_OPEN, (DWORD)&was->drvInst, 0L);
		    if (ret == MMSYSERR_NOERROR) {
			if (fdwOpen & ACM_STREAMOPENF_QUERY) {
			    acmDriverClose(had, 0L);
			}
			break;
		    }
		}
		acmDriverClose(had, 0L);*/
	//if(MSACM_pFirstACMDriverID==NULL)
	//    MSACM_RegisterAllDrivers();
	
	for (wadi = MSACM_pFirstACMDriverID; wadi; wadi = wadi->pNextACMDriverID)
	{
	    /* Check Format */
	    if ((int)wadi->wFormatTag != (int)pwfxSrc->wFormatTag) continue;

	    ret = acmDriverOpen(&had, (HACMDRIVERID)wadi, 0L);
	    if (ret == MMSYSERR_NOERROR) {
		if ((wad = MSACM_GetDriver(had)) != 0) {
		    was->obj.pACMDriverID = wad->obj.pACMDriverID;
		    was->pDrv = wad;
		    was->hAcmDriver = had;
		    
		    ret = SendDriverMessage(wad->hDrvr, ACMDM_STREAM_OPEN, (DWORD)&was->drvInst, 0L);
		    //lhacm - crash printf("RETOPEN %d\n", ret);
                    //ret = 0;
		    if (ret == MMSYSERR_NOERROR) {
			if (fdwOpen & ACM_STREAMOPENF_QUERY) {
			    acmDriverClose(had, 0L);
			}
			break;
		    }
		}
		// no match, close this acm driver and try next one 
		acmDriverClose(had, 0L);
	    }
	}
	if (ret != MMSYSERR_NOERROR) {
	    ret = ACMERR_NOTPOSSIBLE;
	    goto errCleanUp;
	}
    }
    ret = MMSYSERR_NOERROR;
    if (!(fdwOpen & ACM_STREAMOPENF_QUERY)) {
	if (phas)
	    *phas = (HACMSTREAM)was;
	TRACE("=> (%d)\n", ret);
#ifdef WIN32_LOADER
        CodecAlloc();
#endif
	return ret;
    }
errCleanUp:		
    if (phas)
	*phas = (HACMSTREAM)0;
    HeapFree(MSACM_hHeap, 0, was);
    TRACE("=> (%d)\n", ret);
    return ret;
}


MMRESULT WINAPI acmStreamClose(HACMSTREAM has, DWORD fdwClose)
{
    PWINE_ACMSTREAM	was;
    MMRESULT		ret;
		
    TRACE("(0x%08x, %ld)\n", has, fdwClose);
    
    if ((was = ACM_GetStream(has)) == NULL) {
	return MMSYSERR_INVALHANDLE;
    }
    ret = SendDriverMessage(was->pDrv->hDrvr, ACMDM_STREAM_CLOSE, (DWORD)&was->drvInst, 0);
    if (ret == MMSYSERR_NOERROR) {
	if (was->hAcmDriver)
	    acmDriverClose(was->hAcmDriver, 0L);	
	HeapFree(MSACM_hHeap, 0, was);
#ifdef WIN32_LOADER
        CodecRelease();
#endif
    }
    TRACE("=> (%d)\n", ret);
    return ret;
}

/***********************************************************************
 *           acmStreamConvert (MSACM32.38)
 */
MMRESULT WINAPI acmStreamConvert(HACMSTREAM has, PACMSTREAMHEADER pash, 
				 DWORD fdwConvert)
{
    PWINE_ACMSTREAM	was;
    MMRESULT		ret = MMSYSERR_NOERROR;
    PACMDRVSTREAMHEADER	padsh;

    TRACE("(0x%08x, %p, %ld)\n", has, pash, fdwConvert);

    if ((was = ACM_GetStream(has)) == NULL)
	return MMSYSERR_INVALHANDLE;
    if (!pash || pash->cbStruct < sizeof(ACMSTREAMHEADER))
	return MMSYSERR_INVALPARAM;

    if (!(pash->fdwStatus & ACMSTREAMHEADER_STATUSF_PREPARED))
	return ACMERR_UNPREPARED;

    /* Note: the ACMSTREAMHEADER and ACMDRVSTREAMHEADER structs are of same
     * size. some fields are private to msacm internals, and are exposed
     * in ACMSTREAMHEADER in the dwReservedDriver array
     */
    padsh = (PACMDRVSTREAMHEADER)pash;

    /* check that pointers have not been modified */
    if (padsh->pbPreparedSrc != padsh->pbSrc ||
	padsh->cbPreparedSrcLength < padsh->cbSrcLength ||
	padsh->pbPreparedDst != padsh->pbDst ||
	padsh->cbPreparedDstLength < padsh->cbDstLength) {
	return MMSYSERR_INVALPARAM;
    }	

    padsh->fdwConvert = fdwConvert;

    ret = SendDriverMessage(was->pDrv->hDrvr, ACMDM_STREAM_CONVERT, (DWORD)&was->drvInst, (DWORD)padsh);
    if (ret == MMSYSERR_NOERROR) {
	padsh->fdwStatus |= ACMSTREAMHEADER_STATUSF_DONE;
    }
    TRACE("=> (%d)\n", ret);
    return ret;
}


/***********************************************************************
 *           acmStreamPrepareHeader (MSACM32.41)
 */
MMRESULT WINAPI acmStreamPrepareHeader(HACMSTREAM has, PACMSTREAMHEADER pash, 
				       DWORD fdwPrepare)
{
    PWINE_ACMSTREAM	was;
    MMRESULT		ret = MMSYSERR_NOERROR;
    PACMDRVSTREAMHEADER	padsh;

    TRACE("(0x%08x, %p, %ld)\n", has, pash, fdwPrepare);
    
    if ((was = ACM_GetStream(has)) == NULL)
	return MMSYSERR_INVALHANDLE;
    if (!pash || pash->cbStruct < sizeof(ACMSTREAMHEADER))
	return MMSYSERR_INVALPARAM;
    if (fdwPrepare)
	ret = MMSYSERR_INVALFLAG;

    if (pash->fdwStatus & ACMSTREAMHEADER_STATUSF_DONE)
	return MMSYSERR_NOERROR;

    /* Note: the ACMSTREAMHEADER and ACMDRVSTREAMHEADER structs are of same
     * size. some fields are private to msacm internals, and are exposed
     * in ACMSTREAMHEADER in the dwReservedDriver array
     */
    padsh = (PACMDRVSTREAMHEADER)pash;

    padsh->fdwConvert = fdwPrepare;
    padsh->padshNext = NULL;
    padsh->fdwDriver = padsh->dwDriver = 0L;

    padsh->fdwPrepared = 0;
    padsh->dwPrepared = 0;
    padsh->pbPreparedSrc = 0;
    padsh->cbPreparedSrcLength = 0;
    padsh->pbPreparedDst = 0;
    padsh->cbPreparedDstLength = 0;

    ret = SendDriverMessage(was->pDrv->hDrvr, ACMDM_STREAM_PREPARE, (DWORD)&was->drvInst, (DWORD)padsh);
    if (ret == MMSYSERR_NOERROR || ret == MMSYSERR_NOTSUPPORTED) {
	ret = MMSYSERR_NOERROR;
	padsh->fdwStatus &= ~(ACMSTREAMHEADER_STATUSF_DONE|ACMSTREAMHEADER_STATUSF_INQUEUE);
	padsh->fdwStatus |= ACMSTREAMHEADER_STATUSF_PREPARED;
	padsh->fdwPrepared = padsh->fdwStatus;
	padsh->dwPrepared = 0;
	padsh->pbPreparedSrc = padsh->pbSrc;
	padsh->cbPreparedSrcLength = padsh->cbSrcLength;
	padsh->pbPreparedDst = padsh->pbDst;
	padsh->cbPreparedDstLength = padsh->cbDstLength;
    } else {
	padsh->fdwPrepared = 0;
	padsh->dwPrepared = 0;
	padsh->pbPreparedSrc = 0;
	padsh->cbPreparedSrcLength = 0;
	padsh->pbPreparedDst = 0;
	padsh->cbPreparedDstLength = 0;
    }
    TRACE("=> (%d)\n", ret);
    return ret;
}

/***********************************************************************
 *           acmStreamReset (MSACM32.42)
 */
MMRESULT WINAPI acmStreamReset(HACMSTREAM has, DWORD fdwReset)
{
    PWINE_ACMSTREAM	was;
    MMRESULT		ret = MMSYSERR_NOERROR;

    TRACE("(0x%08x, %ld)\n", has, fdwReset);

    if (fdwReset) {
	ret = MMSYSERR_INVALFLAG;
    } else if ((was = ACM_GetStream(has)) == NULL) {
	return MMSYSERR_INVALHANDLE;
    } else if (was->drvInst.fdwOpen & ACM_STREAMOPENF_ASYNC) {
	ret = SendDriverMessage(was->pDrv->hDrvr, ACMDM_STREAM_RESET, (DWORD)&was->drvInst, 0);
    }
    TRACE("=> (%d)\n", ret);
    return ret;
}

/***********************************************************************
 *           acmStreamSize (MSACM32.43)
 */
MMRESULT WINAPI acmStreamSize(HACMSTREAM has, DWORD cbInput, 
			      LPDWORD pdwOutputBytes, DWORD fdwSize)
{
    PWINE_ACMSTREAM	was;
    ACMDRVSTREAMSIZE	adss;
    MMRESULT		ret;
    
    TRACE("(0x%08x, %ld, %p, %ld)\n", has, cbInput, pdwOutputBytes, fdwSize);
    
    if ((was = ACM_GetStream(has)) == NULL) {
	return MMSYSERR_INVALHANDLE;
    }
    if ((fdwSize & ~ACM_STREAMSIZEF_QUERYMASK) != 0) {
	return MMSYSERR_INVALFLAG;
    }

    *pdwOutputBytes = 0L;
    
    switch (fdwSize & ACM_STREAMSIZEF_QUERYMASK) {
    case ACM_STREAMSIZEF_DESTINATION:
	adss.cbDstLength = cbInput;
	adss.cbSrcLength = 0;
	break;
    case ACM_STREAMSIZEF_SOURCE:
	adss.cbSrcLength = cbInput;
	adss.cbDstLength = 0;
	break;
    default:	
	return MMSYSERR_INVALFLAG;
    }
    
    adss.cbStruct = sizeof(adss);
    adss.fdwSize = fdwSize;
    ret = SendDriverMessage(was->pDrv->hDrvr, ACMDM_STREAM_SIZE, 
			    (DWORD)&was->drvInst, (DWORD)&adss);
    if (ret == MMSYSERR_NOERROR) {
	switch (fdwSize & ACM_STREAMSIZEF_QUERYMASK) {
	case ACM_STREAMSIZEF_DESTINATION:
	    *pdwOutputBytes = adss.cbSrcLength;
	    break;
	case ACM_STREAMSIZEF_SOURCE:
	    *pdwOutputBytes = adss.cbDstLength;
	    break;
	}
    }
    TRACE("=> (%d) [%lu]\n", ret, *pdwOutputBytes);
    return ret;
}

/***********************************************************************
 *           acmStreamUnprepareHeader (MSACM32.44)
 */
MMRESULT WINAPI acmStreamUnprepareHeader(HACMSTREAM has, PACMSTREAMHEADER pash, 
					 DWORD fdwUnprepare)
{
    PWINE_ACMSTREAM	was;
    MMRESULT		ret = MMSYSERR_NOERROR;
    PACMDRVSTREAMHEADER	padsh;

    TRACE("(0x%08x, %p, %ld)\n", has, pash, fdwUnprepare);
    
    if ((was = ACM_GetStream(has)) == NULL)
	return MMSYSERR_INVALHANDLE;
    if (!pash || pash->cbStruct < sizeof(ACMSTREAMHEADER))
	return MMSYSERR_INVALPARAM;

    if (!(pash->fdwStatus & ACMSTREAMHEADER_STATUSF_PREPARED))
	return ACMERR_UNPREPARED;

    /* Note: the ACMSTREAMHEADER and ACMDRVSTREAMHEADER structs are of same
     * size. some fields are private to msacm internals, and are exposed
     * in ACMSTREAMHEADER in the dwReservedDriver array
     */
    padsh = (PACMDRVSTREAMHEADER)pash;

    /* check that pointers have not been modified */
    if (padsh->pbPreparedSrc != padsh->pbSrc ||
	padsh->cbPreparedSrcLength < padsh->cbSrcLength ||
	padsh->pbPreparedDst != padsh->pbDst ||
	padsh->cbPreparedDstLength < padsh->cbDstLength) {
	return MMSYSERR_INVALPARAM;
    }	

    padsh->fdwConvert = fdwUnprepare;

    ret = SendDriverMessage(was->pDrv->hDrvr, ACMDM_STREAM_UNPREPARE, (DWORD)&was->drvInst, (DWORD)padsh);
    if (ret == MMSYSERR_NOERROR || ret == MMSYSERR_NOTSUPPORTED) {
	ret = MMSYSERR_NOERROR;
	padsh->fdwStatus &= ~(ACMSTREAMHEADER_STATUSF_DONE|ACMSTREAMHEADER_STATUSF_INQUEUE|ACMSTREAMHEADER_STATUSF_PREPARED);
    }
    TRACE("=> (%d)\n", ret);
    return ret;
}
