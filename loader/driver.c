#include <config.h>
#include <stdio.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
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
#include "ldt_keeper.h"
#include "driver.h"


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
    __asm__( \
    "push %%ebx\n\t" \
    "push %%ecx\n\t" \
    "push %%edx\n\t" \
    "push %%esi\n\t" \
    "push %%edi\n\t"::)

#define REST_ALL \
    __asm__( \
    "pop %%edi\n\t" \
    "pop %%esi\n\t" \
    "pop %%edx\n\t" \
    "pop %%ecx\n\t" \
    "pop %%ebx\n\t"::)
#endif



    
static DWORD dwDrvID = 0;


LRESULT WINAPI SendDriverMessage( HDRVR hDriver, UINT message,
                                    LPARAM lParam1, LPARAM lParam2 )
{
    DRVR* module=(DRVR*)hDriver;
    int result;
#ifdef DETAILED_OUT    
    printf("SendDriverMessage: driver %X, message %X, arg1 %X, arg2 %X\n", hDriver, message, lParam1, lParam2);
#endif
    if(module==0)return -1;
    if(module->hDriverModule==0)return -1;
    if(module->DriverProc==0)return -1;
    STORE_ALL;
    result=module->DriverProc(module->dwDriverID,1,message,lParam1,lParam2);
    REST_ALL;
#ifdef DETAILED_OUT    
    printf("\t\tResult: %X\n", result);
#endif    
    return result;
}				    

static NPDRVR DrvAlloc(HDRVR*lpDriver, LPUINT lpDrvResult)
{
    NPDRVR npDriver;
    /* allocate and lock handle */
    if (lpDriver)
    {
      if ( (*lpDriver = (HDRVR) malloc(sizeof(DRVR))) )
        {
            if ((npDriver = (NPDRVR) *lpDriver))
            {
                    *lpDrvResult = MMSYSERR_NOERROR;
                    return (npDriver);
            }
            free((NPDRVR)*lpDriver);
        }
        return (*lpDrvResult = MMSYSERR_NOMEM, (NPDRVR) 0);
    }
    return (*lpDrvResult = MMSYSERR_INVALPARAM, (NPDRVR) 0);
}

                                                                                                                    
static void DrvFree(HDRVR hDriver)
{
    int i;
    Setup_FS_Segment();
    if(hDriver)
    	if(((DRVR*)hDriver)->hDriverModule)
    	if(((DRVR*)hDriver)->DriverProc)
	(((DRVR*)hDriver)->DriverProc)(((DRVR*)hDriver)->dwDriverID, hDriver, DRV_CLOSE, 0, 0);
    if(hDriver)	{
            if(((DRVR*)hDriver)->hDriverModule)
    		if(((DRVR*)hDriver)->DriverProc)
			(((DRVR*)hDriver)->DriverProc)(0, hDriver, DRV_FREE, 0, 0);
		FreeLibrary(((DRVR*)hDriver)->hDriverModule);
        	free((NPDRVR)hDriver);
		return;	
    }
}

void DrvClose(HDRVR hdrvr)
{
    DrvFree(hdrvr);
}


char* win32_codec_name=NULL;  // must be set before calling DrvOpen() !!!

HDRVR VFWAPI
DrvOpen(LPARAM lParam2)
{
    ICOPEN *icopen=(ICOPEN *) lParam2;
    UINT uDrvResult;
    HDRVR hDriver;
    NPDRVR npDriver;
    char unknown[0x24];
//    char* codec_name=icopen->fccHandler;

    Setup_LDT_Keeper();

    if (!(npDriver = DrvAlloc(&hDriver, &uDrvResult)))
	return ((HDRVR) 0);

    if (!(npDriver->hDriverModule = LoadLibraryA(win32_codec_name))) {
     	printf("Can't open library %s\n", win32_codec_name);
        DrvFree(hDriver);
        return ((HDRVR) 0);
    }

#if 0
    {
        unsigned char *p=((char*)npDriver->hDriverModule);
        double *dp;
        int i;
        p+=0x14c0;
        for(i=0;i<16;i++)printf(" %02X",p[i]); printf("\n");
        dp=(double*)p;
        printf("divx bitrate = %f\n",(float)(*dp));
//    	*(double*)((char*)npDriver->hDriverModule+0x14c0)=bitrate;
    }
#endif
   
    if (!(npDriver->DriverProc = (DRIVERPROC)
             GetProcAddress(npDriver->hDriverModule, "DriverProc"))) {
#if 1
         printf("Library %s is not a VfW/ACM valid codec\n", win32_codec_name);
#else
        // Try DirectShow...
         GETCLASS func=(GETCLASS)GetProcAddress(npDriver->hDriverModule,"DllGetClassObject");
         if(!func)
           printf("Library %s is not a valid VfW/ACM/DShow codec\n", win32_codec_name);
         else {
            HRESULT result;
            struct IClassFactory* factory=0;
	    struct IUnknown* object=0;
            GUID CLSID_Voxware={0x73f7a062, 0x8829, 0x11d1,
                {0xb5, 0x50, 0x00, 0x60, 0x97, 0x24, 0x2d, 0x8d}};
            GUID* id=&CLSID_Voxware;
            
            result=func(id, &IID_IClassFactory, (void**)&factory);
            if(result || (!factory)) printf("No such class object (wrong/missing GUID?)\n");

            printf("Calling factory->vt->CreateInstance()\n");
            printf("addr = %X\n",(unsigned int)factory->vt->CreateInstance);
            result=factory->vt->CreateInstance(factory, 0, &IID_IUnknown, (void**)&object);
            printf("Calling factory->vt->Release()\n");
            factory->vt->Release((struct IUnknown*)factory);
            if(result || (!object)) printf("Class factory failure\n");

            printf("DirectShow codecs not yet supported...\n");
         }
#endif

         FreeLibrary(npDriver->hDriverModule);
         DrvFree(hDriver);
         return ((HDRVR) 0);

    }

    //TRACE("DriverProc == %X\n", npDriver->DriverProc);
     npDriver->dwDriverID = ++dwDrvID;

     Setup_FS_Segment();

	STORE_ALL;
        (npDriver->DriverProc)(0, hDriver, DRV_LOAD, 0, 0);
	REST_ALL;
	//TRACE("DRV_LOAD Ok!\n");
	STORE_ALL;
	(npDriver->DriverProc)(0, hDriver, DRV_ENABLE, 0, 0);
	REST_ALL;
	//TRACE("DRV_ENABLE Ok!\n");

     // open driver 
    STORE_ALL;
     npDriver->dwDriverID=(npDriver->DriverProc)(npDriver->dwDriverID, hDriver, DRV_OPEN,
         (LPARAM) (LPSTR) unknown, lParam2);
    REST_ALL;

    //TRACE("DRV_OPEN Ok!(%X)\n", npDriver->dwDriverID);

    if (uDrvResult)
    {
         DrvFree(hDriver);
         hDriver = (HDRVR) 0;
     }
     
//     printf("Successfully loaded codec %s\n",win32_codec_name);
     
     return (hDriver);
}
  
