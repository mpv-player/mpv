#include "cmediasample.h"
#include "wine/winerror.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static long STDCALL CMediaSample_QueryInterface(IUnknown* This,
						/* [in] */ const GUID* iid,
						/* [iid_is][out] */ void **ppv)
{
    Debug printf("CMediaSample_QueryInterface(%p) called\n", This);
    if (!ppv)
	return E_INVALIDARG;
    if (memcmp(iid, &IID_IUnknown, sizeof(*iid)) == 0)
    {
	*ppv = (void*)This;
	((IMediaSample*) This)->vt->AddRef(This);
	return 0;
    }
    if (memcmp(iid, &IID_IMediaSample, sizeof(*iid)) == 0)
    {
	*ppv = (void*)This;
	((IMediaSample*) This)->vt->AddRef(This);
	return 0;
    }
    return E_NOINTERFACE;
}

static long STDCALL CMediaSample_AddRef(IUnknown* This)
{
    Debug printf("CMediaSample_AddRef(%p) called\n", This);
    ((CMediaSample*)This)->refcount++;
    return 0;
}

void CMediaSample_Destroy(CMediaSample* This)
{

    Debug printf("CMediaSample_Destroy(%p) called (ref:%d)\n", This, This->refcount);
    free(This->vt);
    free(This->own_block);
    if (This->media_type.pbFormat)
	CoTaskMemFree(This->media_type.pbFormat);
    free(This);
}

static long STDCALL CMediaSample_Release(IUnknown* This)
{
    CMediaSample* parent = (CMediaSample*)This;
    Debug printf("CMediaSample_Release(%p) called  (new ref:%d)\n",
		 This, ((CMediaSample*)This)->refcount-1);

    if (--((CMediaSample*) This)->refcount == 0)
    {
	parent->all->vt->ReleaseBuffer((IMemAllocator*)(parent->all),
				       (IMediaSample*)This);
    }
    return 0;
}

static HRESULT STDCALL CMediaSample_GetPointer(IMediaSample* This,
					       /* [out] */ BYTE** ppBuffer)
{
    Debug printf("CMediaSample_GetPointer(%p) called -> %p, size: %d  %d\n", This, ((CMediaSample*) This)->block, ((CMediaSample*)This)->actual_size, ((CMediaSample*)This)->size);
    if (!ppBuffer)
	return E_INVALIDARG;
    *ppBuffer = (BYTE*) ((CMediaSample*) This)->block;
    return 0;
}

static long STDCALL CMediaSample_GetSize(IMediaSample * This)
{
    Debug printf("CMediaSample_GetSize(%p) called -> %d\n", This, ((CMediaSample*) This)->size);
    return ((CMediaSample*) This)->size;
}

static HRESULT STDCALL CMediaSample_GetTime(IMediaSample * This,
					    /* [out] */ REFERENCE_TIME *pTimeStart,
					    /* [out] */ REFERENCE_TIME *pTimeEnd)
{
    Debug printf("CMediaSample_GetTime(%p) called (UNIMPLIMENTED)\n", This);
    return E_NOTIMPL;
}

static HRESULT STDCALL CMediaSample_SetTime(IMediaSample * This,
					    /* [in] */ REFERENCE_TIME *pTimeStart,
					    /* [in] */ REFERENCE_TIME *pTimeEnd)
{
    Debug printf("CMediaSample_SetTime(%p) called (UNIMPLIMENTED)\n", This);
    return E_NOTIMPL;
}

static HRESULT STDCALL CMediaSample_IsSyncPoint(IMediaSample * This)
{
    Debug printf("CMediaSample_IsSyncPoint(%p) called\n", This);
    if (((CMediaSample*)This)->isSyncPoint)
	return 0;
    return 1;
}

static HRESULT STDCALL CMediaSample_SetSyncPoint(IMediaSample * This,
						 long bIsSyncPoint)
{
    Debug printf("CMediaSample_SetSyncPoint(%p) called\n", This);
    ((CMediaSample*)This)->isSyncPoint = bIsSyncPoint;
    return 0;
}

static HRESULT STDCALL CMediaSample_IsPreroll(IMediaSample * This)
{
    Debug printf("CMediaSample_IsPreroll(%p) called\n", This);

    if (((CMediaSample*)This)->isPreroll)
	return 0;//S_OK

    return 1;//S_FALSE
}

static HRESULT STDCALL CMediaSample_SetPreroll(IMediaSample * This,
					       long bIsPreroll)
{
    Debug printf("CMediaSample_SetPreroll(%p) called\n", This);
    ((CMediaSample*)This)->isPreroll=bIsPreroll;
    return 0;
}

static long STDCALL CMediaSample_GetActualDataLength(IMediaSample* This)
{
    Debug printf("CMediaSample_GetActualDataLength(%p) called -> %d\n", This, ((CMediaSample*)This)->actual_size);
    return ((CMediaSample*)This)->actual_size;
}

static HRESULT STDCALL CMediaSample_SetActualDataLength(IMediaSample* This,
							long __MIDL_0010)
{
    CMediaSample* cms = (CMediaSample*)This;
    Debug printf("CMediaSample_SetActualDataLength(%p, %ld) called\n", This, __MIDL_0010);
    if (__MIDL_0010 > cms->size)
    {
        char* c = cms->own_block;
	Debug printf(" CMediaSample - buffer overflow   %ld %d   %p %p\n",
		     __MIDL_0010, ((CMediaSample*)This)->size, cms->own_block, cms->block);
	cms->own_block = realloc(cms->own_block, __MIDL_0010);
	if (c == cms->block)
	    cms->block = cms->own_block;
        cms->size = __MIDL_0010;
    }
    cms->actual_size = __MIDL_0010;
    return 0;
}

static HRESULT STDCALL CMediaSample_GetMediaType(IMediaSample* This,
						 AM_MEDIA_TYPE** ppMediaType)
{
    AM_MEDIA_TYPE* t;
    Debug printf("CMediaSample_GetMediaType(%p) called\n", This);
    if(!ppMediaType)
	return E_INVALIDARG;
    if(!((CMediaSample*)This)->type_valid)
    {
	*ppMediaType=0;
	return 1;
    }

    t = &((CMediaSample*)This)->media_type;
    //    if(t.pbFormat)CoTaskMemFree(t.pbFormat);
    (*ppMediaType) = (AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    **ppMediaType = *t;
    (*ppMediaType)->pbFormat = (char*)CoTaskMemAlloc(t->cbFormat);
    memcpy((*ppMediaType)->pbFormat, t->pbFormat, t->cbFormat);
    //    *ppMediaType=0; //media type was not changed
    return 0;
}

static HRESULT STDCALL CMediaSample_SetMediaType(IMediaSample * This,
						 AM_MEDIA_TYPE *pMediaType)
{
    AM_MEDIA_TYPE* t;
    Debug printf("CMediaSample_SetMediaType(%p) called\n", This);
    if (!pMediaType)
	return E_INVALIDARG;
    t = &((CMediaSample*)This)->media_type;
    if (t->pbFormat)
	CoTaskMemFree(t->pbFormat);
    t = pMediaType;
    if (t->cbFormat)
    {
	t->pbFormat = (char*)CoTaskMemAlloc(t->cbFormat);
	memcpy(t->pbFormat, pMediaType->pbFormat, t->cbFormat);
    }
    else
        t->pbFormat = 0;
    ((CMediaSample*) This)->type_valid=1;

    return 0;
}

static HRESULT STDCALL CMediaSample_IsDiscontinuity(IMediaSample * This)
{
    Debug printf("CMediaSample_IsDiscontinuity(%p) called\n", This);
    return ((CMediaSample*) This)->isDiscontinuity;
}

static HRESULT STDCALL CMediaSample_SetDiscontinuity(IMediaSample * This,
						     long bDiscontinuity)
{
    Debug printf("CMediaSample_SetDiscontinuity(%p) called (%ld)\n", This, bDiscontinuity);
    ((CMediaSample*) This)->isDiscontinuity = bDiscontinuity;
    return 0;
}

static HRESULT STDCALL CMediaSample_GetMediaTime(IMediaSample * This,
						 /* [out] */ LONGLONG *pTimeStart,
						 /* [out] */ LONGLONG *pTimeEnd)
{
    Debug printf("CMediaSample_GetMediaTime(%p) called\n", This);
    if (pTimeStart)
	*pTimeStart = ((CMediaSample*) This)->time_start;
    if (pTimeEnd)
	*pTimeEnd = ((CMediaSample*) This)->time_end;
    return 0;
}

static HRESULT STDCALL CMediaSample_SetMediaTime(IMediaSample * This,
						 /* [in] */ LONGLONG *pTimeStart,
						 /* [in] */ LONGLONG *pTimeEnd)
{
    Debug printf("CMediaSample_SetMediaTime(%p) called\n", This);
    if (pTimeStart)
	((CMediaSample*) This)->time_start = *pTimeStart;
    if (pTimeEnd)
        ((CMediaSample*) This)->time_end = *pTimeEnd;
    return 0;
}

// extension for direct memory write or decompressed data
static void CMediaSample_SetPointer(CMediaSample* This, char* pointer)
{
    Debug printf("CMediaSample_SetPointer(%p) called  -> %p\n", This, pointer);
    if (pointer)
	This->block = pointer;
    else
	This->block = This->own_block;
}

static void CMediaSample_ResetPointer(CMediaSample* This)
{
    Debug printf("CMediaSample_ResetPointer(%p) called\n", This);
    This->block = This->own_block;
}

CMediaSample* CMediaSampleCreate(IMemAllocator* allocator, int _size)
{
    CMediaSample* This = (CMediaSample*) malloc(sizeof(CMediaSample));
    if (!This)
	return NULL;

    // some hack here!
    // it looks like Acelp decoder is actually accessing
    // the allocated memory before it sets the new size for it ???
    // -- maybe it's being initialized with wrong parameters
    // anyway this is fixes the problem somehow with some reserves
    //
    // using different trick for now - in DS_Audio modify sample size
    //if (_size < 0x1000)
    //    _size = (_size + 0xfff) & ~0xfff;

    This->vt = (IMediaSample_vt*) malloc(sizeof(IMediaSample_vt));
    This->own_block = (char*) malloc(_size);
    This->media_type.pbFormat = 0;

    if (!This->vt || !This->own_block)
    {
	CMediaSample_Destroy(This);
	return NULL;
    }

    This->vt->QueryInterface = CMediaSample_QueryInterface;
    This->vt->AddRef = CMediaSample_AddRef;
    This->vt->Release = CMediaSample_Release;
    This->vt->GetPointer = CMediaSample_GetPointer;
    This->vt->GetSize = CMediaSample_GetSize;
    This->vt->GetTime = CMediaSample_GetTime;
    This->vt->SetTime = CMediaSample_SetTime;
    This->vt->IsSyncPoint = CMediaSample_IsSyncPoint;
    This->vt->SetSyncPoint = CMediaSample_SetSyncPoint;
    This->vt->IsPreroll = CMediaSample_IsPreroll;
    This->vt->SetPreroll = CMediaSample_SetPreroll;
    This->vt->GetActualDataLength = CMediaSample_GetActualDataLength;
    This->vt->SetActualDataLength = CMediaSample_SetActualDataLength;
    This->vt->GetMediaType = CMediaSample_GetMediaType;
    This->vt->SetMediaType = CMediaSample_SetMediaType;
    This->vt->IsDiscontinuity = CMediaSample_IsDiscontinuity;
    This->vt->SetDiscontinuity = CMediaSample_SetDiscontinuity;
    This->vt->GetMediaTime = CMediaSample_GetMediaTime;
    This->vt->SetMediaTime = CMediaSample_SetMediaTime;

    This->all = allocator;
    This->size = _size;
    This->refcount = 0; // increased by MemAllocator
    This->actual_size = 0;
    This->isPreroll = 0;
    This->isDiscontinuity = 1;
    This->time_start = 0;
    This->time_end = 0;
    This->type_valid = 0;
    This->block = This->own_block;

    This->SetPointer = CMediaSample_SetPointer;
    This->ResetPointer = CMediaSample_ResetPointer;

    Debug printf("CMediaSample_Create(%p) called - sample size %d, buffer %p\n",
		 This, This->size, This->block);

    return This;
}
