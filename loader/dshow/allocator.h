#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "interfaces.h"
#include "cmediasample.h"
#include <list>
#include "iunk.h"
#include "default.h"

class MemAllocator: public IMemAllocator
{
    ALLOCATOR_PROPERTIES props;
    std::list<CMediaSample*> used_list;
    std::list<CMediaSample*> free_list;
    char* new_pointer;
    CMediaSample* modified_sample;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(MemAllocator)
public:
    MemAllocator();
    ~MemAllocator(){delete vt;}
    static long CreateAllocator(GUID* clsid, GUID* iid, void** ppv);
    void SetPointer(char* pointer) { new_pointer=pointer; }
    void ResetPointer() 
    { 
	if(modified_sample) 
	{
	    modified_sample->ResetPointer(); 
	    modified_sample=0;
	}
    }
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
