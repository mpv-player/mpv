#include <stdio.h>
#include <string.h>
#include "cmediasample.h"
#define E_NOTIMPL 0x80004003
CMediaSample::CMediaSample(IMemAllocator* allocator, long _size):refcount(0)
{
    vt=new IMediaSample_vt;
    
    vt->QueryInterface=QueryInterface;
    vt->AddRef=AddRef;
    vt->Release=Release;
    vt->GetPointer=GetPointer ; 
    vt->GetSize=GetSize ; 
    vt->GetTime=GetTime ; 
    vt->SetTime=SetTime ; 
    vt->IsSyncPoint=IsSyncPoint ; 
    vt->SetSyncPoint=SetSyncPoint; 
    vt->IsPreroll=IsPreroll; 
    vt->SetPreroll=SetPreroll; 
    vt->GetActualDataLength=GetActualDataLength; 
    vt->SetActualDataLength=SetActualDataLength; 
    vt->GetMediaType=GetMediaType; 
    vt->SetMediaType=SetMediaType; 
    vt->IsDiscontinuity=IsDiscontinuity; 
    vt->SetDiscontinuity=SetDiscontinuity; 
    vt->GetMediaTime=GetMediaTime; 
    vt->SetMediaTime=SetMediaTime; 
    
    all=allocator;
    size=_size;
    actual_size=0;
    media_type.pbFormat=0;
    isPreroll=0;
    type_valid=0;
    block=new char[size];    
    Debug printf("%x: Creating media sample with size %d, buffer 0x%x\n", this, _size, block);
}
CMediaSample::~CMediaSample()
{
    Debug printf("%x: CMediaSample::~CMediaSample() called\n", this);
    delete vt;
    delete[] block;
    if(media_type.pbFormat)
	CoTaskMemFree(media_type.pbFormat);
}

long STDCALL CMediaSample::QueryInterface ( 
    IUnknown * This,
    /* [in] */ IID* iid,
    /* [iid_is][out] */ void **ppv)
{
    Debug printf("CMediaSample::QueryInterface() called\n");
    if(!ppv)return 0x80004003;
    if(!memcmp(iid, &IID_IUnknown, 16))
    {
	*ppv=(void*)This;
	This->vt->AddRef(This);
	return 0;
    }
    if(!memcmp(iid, &IID_IMediaSample, 16))
    {
	*ppv=(void*)This;
	This->vt->AddRef(This);
	return 0;
    }
    return 0x80004002;
}

long STDCALL CMediaSample::AddRef ( 
    IUnknown * This)
{
    Debug printf("CMediaSample::AddRef() called\n");
    ((CMediaSample*)This)->refcount++;
    return 0;
}
        
long STDCALL CMediaSample::Release ( 
    IUnknown * This)
{
    Debug printf("%x: CMediaSample::Release() called, new refcount %d\n", This, 
	((CMediaSample*)This)->refcount-1);
    CMediaSample* parent=(CMediaSample*)This;
    if(--((CMediaSample*)This)->refcount==0)
	parent->
	    all->
		vt->
		    ReleaseBuffer(
			(IMemAllocator*)(parent->all), 
			    (IMediaSample*)This);
    return 0;
}
HRESULT STDCALL CMediaSample::GetPointer ( 
    IMediaSample * This,
    /* [out] */ BYTE **ppBuffer)
{
    Debug printf("%x: CMediaSample::GetPointer() called\n", This);
    if(!ppBuffer)return 0x80004003;
    *ppBuffer=(BYTE *)((CMediaSample*)This)->block;
    return 0;
}

long STDCALL CMediaSample::GetSize ( 
    IMediaSample * This)
{
    Debug printf("%x: CMediaSample::GetSize() called -> %d\n", This, ((CMediaSample*)This)->size);
    return ((CMediaSample*)This)->size;
}

HRESULT STDCALL CMediaSample::GetTime ( 
    IMediaSample * This,
    /* [out] */ REFERENCE_TIME *pTimeStart,
    /* [out] */ REFERENCE_TIME *pTimeEnd)
{
    Debug printf("%x: CMediaSample::GetTime() called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL CMediaSample::SetTime ( 
    IMediaSample * This,
    /* [in] */ REFERENCE_TIME *pTimeStart,
    /* [in] */ REFERENCE_TIME *pTimeEnd)
{
    Debug printf("%x: CMediaSample::SetTime() called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL CMediaSample::IsSyncPoint ( 
    IMediaSample * This)
{
    Debug printf("%x: CMediaSample::IsSyncPoint() called\n", This);
    if(((CMediaSample*)This)->isSyncPoint)return 0;
    return 1;
}

HRESULT STDCALL CMediaSample::SetSyncPoint ( 
    IMediaSample * This,
    long bIsSyncPoint)
{
    Debug printf("%x: CMediaSample::SetSyncPoint() called\n", This);
    ((CMediaSample*)This)->isSyncPoint=bIsSyncPoint;
    return 0;
}

HRESULT STDCALL CMediaSample::IsPreroll ( 
    IMediaSample * This)
{
    Debug printf("%x: CMediaSample::IsPreroll() called\n", This);
    if(((CMediaSample*)This)->isPreroll==0)
	return 1;//S_FALSE
    else
	return 0;//S_OK
}

HRESULT STDCALL CMediaSample::SetPreroll ( 
    IMediaSample * This,
    long bIsPreroll)
{
    Debug printf("%x: CMediaSample::SetPreroll() called\n", This);
    ((CMediaSample*)This)->isPreroll=bIsPreroll;
    return 0;
}

long STDCALL CMediaSample::GetActualDataLength ( 
    IMediaSample * This)
{
    Debug printf("%x: CMediaSample::GetActualDataLength() called -> %d\n", This, ((CMediaSample*)This)->actual_size);
    return ((CMediaSample*)This)->actual_size;
}

HRESULT STDCALL CMediaSample::SetActualDataLength ( 
    IMediaSample * This,
    long __MIDL_0010)
{
    Debug printf("%x: CMediaSample::SetActualDataLength(%d) called\n", This, __MIDL_0010);
    if(__MIDL_0010>((CMediaSample*)This)->size)
    {
	printf("%x: ERROR: CMediaSample buffer overflow\n", This);
    }
    ((CMediaSample*)This)->actual_size=__MIDL_0010;
    return 0;
}

HRESULT STDCALL CMediaSample::GetMediaType ( 
    IMediaSample * This,
    AM_MEDIA_TYPE **ppMediaType)
{
    Debug printf("%x: CMediaSample::GetMediaType() called\n", This);
    if(!ppMediaType)
	return 0x80004003;
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

HRESULT STDCALL CMediaSample::SetMediaType ( 
    IMediaSample * This,
    AM_MEDIA_TYPE *pMediaType)
{
    Debug printf("%x: CMediaSample::SetMediaType() called\n", This);
    if(!pMediaType)return 0x80004003;
    AM_MEDIA_TYPE& t=((CMediaSample*)This)->media_type;
    if(t.pbFormat)CoTaskMemFree(t.pbFormat);
    t=*pMediaType;
    t.pbFormat=(char*)CoTaskMemAlloc(t.cbFormat);
    memcpy(t.pbFormat, pMediaType->pbFormat, t.cbFormat);     
    ((CMediaSample*)This)->type_valid=1;
    return 0;
}

HRESULT STDCALL CMediaSample::IsDiscontinuity ( 
    IMediaSample * This)
{
    Debug printf("%x: CMediaSample::IsDiscontinuity() called\n", This);
    return 1;
}

HRESULT STDCALL CMediaSample::SetDiscontinuity ( 
    IMediaSample * This,
    long bDiscontinuity)
{
    Debug printf("%x: CMediaSample::SetDiscontinuity() called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL CMediaSample::GetMediaTime ( 
    IMediaSample * This,
    /* [out] */ LONGLONG *pTimeStart,
    /* [out] */ LONGLONG *pTimeEnd)
{
    Debug printf("%x: CMediaSample::GetMediaTime() called\n", This);
    return E_NOTIMPL;
}

HRESULT STDCALL CMediaSample::SetMediaTime ( 
    IMediaSample * This,
    /* [in] */ LONGLONG *pTimeStart,
    /* [in] */ LONGLONG *pTimeEnd)    
{
    Debug printf("%x: CMediaSample::SetMediaTime() called\n", This);
    return E_NOTIMPL;
}
