#ifndef _CMEDIASAMPLE_H
#define _CMEDIASAMPLE_H

#include "interfaces.h"
#include "guids.h"
#include "default.h"
class CMediaSample: public IMediaSample
{
    IMemAllocator* all;
    int size;
    int actual_size;
    char* block;
    char* old_block;
    int refcount;
    int isPreroll;
    int isSyncPoint;
    AM_MEDIA_TYPE media_type;
    int type_valid;
public:
    CMediaSample(IMemAllocator* allocator, long _size);
    ~CMediaSample();
    void SetPointer(char* pointer) { old_block=block; block=pointer; }
    void ResetPointer() { block=old_block; old_block=0; }

    static long STDCALL QueryInterface ( 
        IUnknown * This,
        /* [in] */ IID* riid,
        /* [iid_is][out] */ void **ppvObject);
    
    static long STDCALL AddRef ( 
        IUnknown * This);
        
    static long STDCALL Release ( 
        IUnknown * This);
    
    static HRESULT STDCALL GetPointer ( 
        IMediaSample * This,
        /* [out] */ BYTE **ppBuffer);
    
    static long STDCALL GetSize ( 
        IMediaSample * This);
    
    static HRESULT STDCALL GetTime ( 
        IMediaSample * This,
        /* [out] */ REFERENCE_TIME *pTimeStart,
        /* [out] */ REFERENCE_TIME *pTimeEnd);
    
    static HRESULT STDCALL SetTime ( 
        IMediaSample * This,
        /* [in] */ REFERENCE_TIME *pTimeStart,
        /* [in] */ REFERENCE_TIME *pTimeEnd);
    
    static HRESULT STDCALL IsSyncPoint ( 
        IMediaSample * This);
    
    static HRESULT STDCALL SetSyncPoint ( 
        IMediaSample * This,
        long bIsSyncPoint);
    
    static HRESULT STDCALL IsPreroll ( 
        IMediaSample * This);
    
    static HRESULT STDCALL SetPreroll ( 
        IMediaSample * This,
        long bIsPreroll);
    
    static long STDCALL GetActualDataLength ( 
        IMediaSample * This);
    
    static HRESULT STDCALL SetActualDataLength ( 
        IMediaSample * This,
        long __MIDL_0010);
    
    static HRESULT STDCALL GetMediaType ( 
        IMediaSample * This,
        AM_MEDIA_TYPE **ppMediaType);
    
    static HRESULT STDCALL SetMediaType ( 
        IMediaSample * This,
        AM_MEDIA_TYPE *pMediaType);
    
    static HRESULT STDCALL IsDiscontinuity ( 
        IMediaSample * This);
    
    static HRESULT STDCALL SetDiscontinuity ( 
        IMediaSample * This,
        long bDiscontinuity);
    
    static HRESULT STDCALL GetMediaTime ( 
        IMediaSample * This,
        /* [out] */ LONGLONG *pTimeStart,
        /* [out] */ LONGLONG *pTimeEnd);
    
    static HRESULT STDCALL SetMediaTime ( 
        IMediaSample * This,
        /* [in] */ LONGLONG *pTimeStart,
        /* [in] */ LONGLONG *pTimeEnd);    
};
#endif
