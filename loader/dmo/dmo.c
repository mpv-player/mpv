/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#include "config.h"
#include "DMO_Filter.h"
#include "driver.h"
#include "com.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "win32.h" // printf macro

void trapbug();
typedef long STDCALL (*GETCLASS) (const GUID*, const GUID*, void**);

void DMO_Filter_Destroy(DMO_Filter* This)
{
    if (This->m_pOptim)
	This->m_pOptim->vt->Release((IUnknown*)This->m_pOptim);
    if (This->m_pInPlace)
	This->m_pInPlace->vt->Release((IUnknown*)This->m_pInPlace);
    if (This->m_pMedia)
	This->m_pMedia->vt->Release((IUnknown*)This->m_pMedia);

    free(This);
#ifdef WIN32_LOADER
    CodecRelease();
#endif
}

DMO_Filter* DMO_FilterCreate(const char* dllname, const GUID* id,
			     DMO_MEDIA_TYPE* in_fmt,
			     DMO_MEDIA_TYPE* out_fmt)
{
    HRESULT hr = 0;
    const char* em = NULL;
    DMO_Filter* This = (DMO_Filter*) malloc(sizeof(DMO_Filter));
    if (!This)
	return NULL;

    memset(This, 0, sizeof(DMO_Filter));
#ifdef WIN32_LOADER
    CodecAlloc();
#endif

    //This->Start = DS_Filter_Start;
    //This->Stop = DS_Filter_Stop;

    for (;;)
    {
	GETCLASS func;
	struct IClassFactory* factory = NULL;
	struct IUnknown* object = NULL;
        unsigned int i;
        unsigned long inputs, outputs;

	This->m_iHandle = LoadLibraryA(dllname);
	if (!This->m_iHandle)
	{
	    em = "could not open DMO DLL";
	    break;
	}
	func = (GETCLASS)GetProcAddress((unsigned)This->m_iHandle, "DllGetClassObject");
	if (!func)
	{
	    em = "illegal or corrupt DMO DLL";
	    break;
	}
//trapbug();
	hr = func(id, &IID_IClassFactory, (void**)&factory);
	if (hr || !factory)
	{
	    em = "no such class object";
	    break;
	}
	hr = factory->vt->CreateInstance(factory, 0, &IID_IUnknown, (void**)&object);
	factory->vt->Release((IUnknown*)factory);
	if (hr || !object)
	{
	    em = "class factory failure";
	    break;
	}
	hr = object->vt->QueryInterface(object, &IID_IMediaObject, (void**)&This->m_pMedia);
	if (hr == 0)
	{
            /* query for some extra available interface */
	    HRESULT r = object->vt->QueryInterface(object, &IID_IMediaObjectInPlace, (void**)&This->m_pInPlace);
            if (r == 0 && This->m_pInPlace)
		printf("DMO dll supports InPlace - PLEASE REPORT to developer\n");
	    r = object->vt->QueryInterface(object, &IID_IDMOVideoOutputOptimizations, (void**)&This->m_pOptim);
	    if (r == 0 && This->m_pOptim)
	    {
                unsigned long flags;
		r = This->m_pOptim->vt->QueryOperationModePreferences(This->m_pOptim, 0, &flags);
		printf("DMO dll supports VO Optimizations %ld %lx\n", r, flags);
		if (flags & DMO_VOSF_NEEDS_PREVIOUS_SAMPLE)
		    printf("DMO dll might use previous sample when requested\n");
	    }
	}
	object->vt->Release((IUnknown*)object);
	if (hr || !This->m_pMedia)
	{
	    em = "object does not provide IMediaObject interface";
	    break;
	}
	hr = This->m_pMedia->vt->SetInputType(This->m_pMedia, 0, in_fmt, 0);
	if (hr)
	{
	    em = "input format not accepted";
	    break;
	}

	if (0) {
	    DMO_MEDIA_TYPE dmo;
            VIDEOINFOHEADER* vi;
	    memset(&dmo, 0, sizeof(dmo));
	    i = This->m_pMedia->vt->GetOutputType(This->m_pMedia, 0, 2, &dmo);
	    printf("GetOutputType %x \n", i);
	    printf("DMO  0x%x (%.4s) 0x%x (%.4s)\n"
	    //printf("DMO  0x%x 0x%x\n"
		   ":: fixszsamp:%d tempcomp:%d  sampsz:%ld\n"
		   ":: formtype: 0x%x\n"
		   ":: unk %p  cbform: %ld  pbform:%p\n",
		   dmo.majortype.f1,
		   (const char*)&dmo.majortype.f1,
		   dmo.subtype.f1,
		   (const char*)&dmo.subtype.f1,
		   dmo.bFixedSizeSamples, dmo.bTemporalCompression,
		   dmo.lSampleSize,
		   dmo.formattype.f1,
                   dmo.pUnk, dmo.cbFormat, dmo.pbFormat
		  );
/*          vi =  (VIDEOINFOHEADER*) dmo.pbFormat;
	    vi =  (VIDEOINFOHEADER*) out_fmt->pbFormat;
	    for (i = 0; i < out_fmt->cbFormat; i++)
                printf("BYTE %d  %02x  %02x\n", i, ((uint8_t*)dmo.pbFormat)[i], ((uint8_t*)out_fmt->pbFormat)[i]);
*/
	}

	hr = This->m_pMedia->vt->SetOutputType(This->m_pMedia, 0, out_fmt, 0);
	if (hr)
	{
	    em = "output format no accepted";
	    break;
	}

	inputs = outputs = 0;
	hr = This->m_pMedia->vt->GetOutputSizeInfo(This->m_pMedia, 0, &inputs, &outputs);
	printf("GetOutput r=0x%lx   size:%ld  align:%ld\n", hr, inputs, outputs);

	// This->m_pMedia->vt->AllocateStreamingResources(This->m_pMedia);
	hr = This->m_pMedia->vt->GetStreamCount(This->m_pMedia, &inputs, &outputs);
	printf("StreamCount r=0x%lx  %ld  %ld\n", hr, inputs, outputs);

        break;
    }
    if (em)
    {
        DMO_Filter_Destroy(This);
	printf("IMediaObject ERROR: %p  %s (0x%lx : %ld)\n", em, em ? em : "", hr, hr);
	This = 0;
    }
    return This;
}
