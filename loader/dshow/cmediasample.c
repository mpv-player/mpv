#include "cmediasample.h"
#include "wine/winerror.h"
#include <stdio.h>
#include <string.h>

static long STDCALL CMediaSample_QueryInterface(IUnknown * This,
						/* [in] */ IID* iid,
						/* [iid_is][out] */ void **ppv)
{
    Debug printf("CMediaSample_QueryInterface() called\n");
    if (!ppv)
	return E_INVALIDARG;
    if (!memcmp(iid, &IID_IUnknown, 16))
    {
	*ppv=(void*)This;
	This->vt->AddRef(This);
	return 0;
    }
    if (!memcmp(iid, &IID_IMediaSample, 16))
    {
	*ppv=(void*)This;
	This->vt->AddRef(This);
	return 0;
    }
    return E_NOINTERFACE;
}

static long STDCALL CMediaSample_AddRef(IUnknown* This)
{
    Debug printf("CMediaSample_AddRef() called\n");
    ((CMediaSample*)This)->refcount++;
    return 0;
}

static long STDCALL CMediaSample_Release(IUnknown* This)
{
    Debug printf("%p: CMediaSample_Release() called, new refcount %d\n",
		 This, ((CMediaSample*)This)->refcount-1);
    CMediaSample* parent=(CMediaSample*)This;
    if (--((CMediaSample*)This)->refcount==0)
	parent->all->vt->ReleaseBuffer((IMemAllocator*)(parent->all),
				       (IMediaSample*)This);
    return 0;
}

static HRESULT STDCALL CMediaSample_GetPointer(IMediaSample * This,
					       /* [out] */ BYTE **ppBuffer)
{
    Debug printf("%p: CMediaSample_GetPointer() called\n", This);
    if (!ppBuffer)
	return E_INVALIDARG;
    *ppBuffer=(BYTE *)((CMediaSample*)This)->block;
    return 0;
}

static long STDCALL CMediaSample_GetSize(IMediaSample * This)
{
    Debug printf("%p: CMediaSample_GetSize() called -> %d\n",
		 This, ((CMediaSample*)This)->size);
    return ((CMediaSample*)This)->size;
}

static HRESULT STDCALL CMediaSample_GetTime(IMediaSample * This,
					    /* [out] */ REFERENCE_TIME *pTimeStart,
					    /* [out] */ REFERENCE_TIME *pTimeEnd)
{
    Debug printf("%p: CMediaSample_GetTime() called\n", This);
    return E_NOTIMPL;
}

static HRESULT STDCALL CMediaSample_SetTime(IMediaSample * This,
					    /* [in] */ REFERENCE_TIME *pTimeStart,
					    /* [in] */ REFERENCE_TIME *pTimeEnd)
{
    Debug printf("%p: CMediaSample_SetTime() called\n", This);
    return E_NOTIMPL;
}

static HRESULT STDCALL CMediaSample_IsSyncPoint(IMediaSample * This)
{
    Debug printf("%p: CMediaSample_IsSyncPoint() called\n", This);
    if (((CMediaSample*)This)->isSyncPoint)
	return 0;
    return 1;
}

static HRESULT STDCALL CMediaSample_SetSyncPoint(IMediaSample * This,
						 long bIsSyncPoint)
{
    Debug printf("%p: CMediaSample_SetSyncPoint() called\n", This);
    ((CMediaSample*)This)->isSyncPoint=bIsSyncPoint;
    return 0;
}

static HRESULT STDCALL CMediaSample_IsPreroll(IMediaSample * This)
{
    Debug printf("%p: CMediaSample_IsPreroll() called\n", This);

    if (((CMediaSample*)This)->isPreroll)
	return 0;//S_OK

    return 1;//S_FALSE
}

static HRESULT STDCALL CMediaSample_SetPreroll(IMediaSample * This,
					       long bIsPreroll)
{
    Debug printf("%p: CMediaSample_SetPreroll() called\n", This);
    ((CMediaSample*)This)->isPreroll=bIsPreroll;
    return 0;
}

static long STDCALL CMediaSample_GetActualDataLength(IMediaSample * This)
{
    Debug printf("%p: CMediaSample_GetActualDataLength() called -> %d\n", This, ((CMediaSample*)This)->actual_size);
    return ((CMediaSample*)This)->actual_size;
}

static HRESULT STDCALL CMediaSample_SetActualDataLength(IMediaSample * This,
							long __MIDL_0010)
{
    Debug printf("%p: CMediaSample_SetActualDataLength(%ld) called\n", This, __MIDL_0010);
    if (__MIDL_0010 > ((CMediaSample*)This)->size)
    {
	printf("%p: ERROR: CMediaSample buffer overflow\n", This);
    }
    ((CMediaSample*)This)->actual_size=__MIDL_0010;
    return 0;
}

static HRESULT STDCALL CMediaSample_GetMediaType(IMediaSample * This,
						 AM_MEDIA_TYPE **ppMediaType)
{
    Debug printf("%p: CMediaSample_GetMediaType() called\n", This);
    if(!ppMediaType)
	return E_INVALIDARG;
    if(!((CMediaSample*)This)->type_valid)
    {
	*ppMediaType=0;
	return 1;
    }
    AM_MEDIA_TYPE& t=((CMediaSample*)This)->media_type;
//    if(t.pbFormat)CoTaskMemFree(t.pbFormat);
    (*ppMediaType)=(AM_MEDIA_TYPE*)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    memcpy(*ppMediaType, &t, sizeof(AM_MEDIA_TYPE));
    (*ppMediaType)->pbFormat=(char*)CoTaskMemAlloc(t.cbFormat);
    memcpy((*ppMediaType)->pbFormat, t.pbFormat, t.cbFormat);
//    *ppMediaType=0; //media type was not changed
    return 0;
}

static HRESULT STDCALL CMediaSample_SetMediaType(IMediaSample * This,
						 AM_MEDIA_TYPE *pMediaType)
{
    Debug printf("%p: CMediaSample_SetMediaType() called\n", This);
    if (!pMediaType)
	return E_INVALIDARG;
    AM_MEDIA_TYPE& t = ((CMediaSample*)This)->media_type;
    if (t.pbFormat)
	CoTaskMemFree(t.pbFormat);
    t = *pMediaType;
    t.pbFormat = (char*)CoTaskMemAlloc(t.cbFormat);
    memcpy(t.pbFormat, pMediaType->pbFormat, t.cbFormat);
    ((CMediaSample*)This)->type_valid=1;

    return 0;
}

static HRESULT STDCALL CMediaSample_IsDiscontinuity(IMediaSample * This)
{
    Debug printf("%p: CMediaSample_IsDiscontinuity() called\n", This);
    return 1;
}

static HRESULT STDCALL CMediaSample_SetDiscontinuity(IMediaSample * This,
						     long bDiscontinuity)
{
    Debug printf("%p: CMediaSample_SetDiscontinuity() called\n", This);
    return E_NOTIMPL;
}

static HRESULT STDCALL CMediaSample_GetMediaTime(IMediaSample * This,
						 /* [out] */ LONGLONG *pTimeStart,
						 /* [out] */ LONGLONG *pTimeEnd)
{
    Debug printf("%p: CMediaSample_GetMediaTime() called\n", This);
    return E_NOTIMPL;
}

static HRESULT STDCALL CMediaSample_SetMediaTime(IMediaSample * This,
						 /* [in] */ LONGLONG *pTimeStart,
						 /* [in] */ LONGLONG *pTimeEnd)
{
    Debug printf("%p: CMediaSample_SetMediaTime() called\n", This);
    return E_NOTIMPL;
}

CMediaSample::CMediaSample(IMemAllocator* allocator, long _size)
{
    vt = new IMediaSample_vt;

    vt->QueryInterface = CMediaSample_QueryInterface;
    vt->AddRef = CMediaSample_AddRef;
    vt->Release = CMediaSample_Release;
    vt->GetPointer = CMediaSample_GetPointer;
    vt->GetSize = CMediaSample_GetSize;
    vt->GetTime = CMediaSample_GetTime;
    vt->SetTime = CMediaSample_SetTime;
    vt->IsSyncPoint = CMediaSample_IsSyncPoint;
    vt->SetSyncPoint = CMediaSample_SetSyncPoint;
    vt->IsPreroll = CMediaSample_IsPreroll;
    vt->SetPreroll = CMediaSample_SetPreroll;
    vt->GetActualDataLength = CMediaSample_GetActualDataLength;
    vt->SetActualDataLength = CMediaSample_SetActualDataLength;
    vt->GetMediaType = CMediaSample_GetMediaType;
    vt->SetMediaType = CMediaSample_SetMediaType;
    vt->IsDiscontinuity = CMediaSample_IsDiscontinuity;
    vt->SetDiscontinuity = CMediaSample_SetDiscontinuity;
    vt->GetMediaTime = CMediaSample_GetMediaTime;
    vt->SetMediaTime = CMediaSample_SetMediaTime;

    all = allocator;
    size = _size;
    refcount = 0;
    actual_size = 0;
    media_type.pbFormat = 0;
    isPreroll = 0;
    type_valid = 0;
    own_block = new char[size];
    block = own_block;
    Debug printf("%p: Creating media sample with size %ld, buffer %p\n",
		 this, _size, block);
}

CMediaSample::~CMediaSample()
{
    Debug printf("%p: CMediaSample::~CMediaSample() called\n", this);
    if (!vt)
        printf("Second delete of CMediaSample()!!|\n");
    delete vt;
    delete own_block;
    if (media_type.pbFormat)
	CoTaskMemFree(media_type.pbFormat);
}
