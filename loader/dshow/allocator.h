#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "interfaces.h"
#include "cmediasample.h"
#include <list>
#include "iunk.h"
#include "default.h"
using namespace std;
class MemAllocator: public IMemAllocator
{
    ALLOCATOR_PROPERTIES props;
    list<CMediaSample*> used_list;
    list<CMediaSample*> free_list;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(MemAllocator)
public:
    MemAllocator();
    ~MemAllocator(){delete vt;}
    static long CreateAllocator(GUID* clsid, GUID* iid, void** ppv);

    static HRESULT STDCALL SetProperties ( 
        IMemAllocator * This,
        /* [in] */ ALLOCATOR_PROPERTIES *pRequest,
        /* [out] */ ALLOCATOR_PROPERTIES *pActual);
    
    static HRESULT STDCALL GetProperties ( 
        IMemAllocator * This,
        /* [out] */ ALLOCATOR_PROPERTIES *pProps);
    
    static HRESULT STDCALL Commit ( 
        IMemAllocator * This);
    
    static HRESULT STDCALL Decommit ( 
        IMemAllocator * This);
    
    static HRESULT STDCALL GetBuffer ( 
        IMemAllocator * This,
        /* [out] */ IMediaSample **ppBuffer,
        /* [in] */ REFERENCE_TIME *pStartTime,
        /* [in] */ REFERENCE_TIME *pEndTime,
        /* [in] */ DWORD dwFlags);
    
    static HRESULT STDCALL ReleaseBuffer ( 
        IMemAllocator * This,
        /* [in] */ IMediaSample *pBuffer);
};

#endif
