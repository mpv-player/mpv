#include <stdio.h>
#include "allocator.h"
#include <com.h>
#define E_NOTIMPL 0x80004001
using namespace std;

class AllocatorKeeper
{
public:
    AllocatorKeeper()
    {	
	RegisterComClass(&CLSID_MemoryAllocator, MemAllocator::CreateAllocator);
    }
};
static AllocatorKeeper keeper;
GUID MemAllocator::interfaces[]=
{
    IID_IUnknown,
    IID_IMemAllocator,
};
IMPLEMENT_IUNKNOWN(MemAllocator)

MemAllocator::MemAllocator()
    :refcount(1)
{
    Debug printf("MemAllocator::MemAllocator() called\n");
    vt=new IMemAllocator_vt;
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->SetProperties = SetProperties;
    vt->GetProperties = GetProperties;
    vt->Commit = Commit;
    vt->Decommit = Decommit;
    vt->GetBuffer = GetBuffer;
    vt->ReleaseBuffer = ReleaseBuffer;
    
    props.cBuffers=1;
    props.cbBuffer=65536;/* :/ */
    props.cbAlign=props.cbPrefix=0;
    
    new_pointer=0;
    modified_sample=0;
}

long MemAllocator::CreateAllocator(GUID* clsid, GUID* iid, void** ppv)
{
    if(!ppv)return -1;
    *ppv=0;
    if(memcmp(clsid, &CLSID_MemoryAllocator, sizeof(GUID)))
	return -1;
    
    IMemAllocator* p=new MemAllocator;
    int result=p->vt->QueryInterface((IUnknown*)p, iid, ppv);
    p->vt->Release((IUnknown*)p);
    return result;
}


/*
    long cBuffers;
    long cbBuffer;
    long cbAlign;
    long cbPrefix;
*/
HRESULT STDCALL MemAllocator::SetProperties ( 
    IMemAllocator * This,
    /* [in] */ ALLOCATOR_PROPERTIES *pRequest,
    /* [out] */ ALLOCATOR_PROPERTIES *pActual)
{
    Debug printf("MemAllocator::SetProperties() called\n");
    if(!pRequest)return 0x80004003;
    if(!pActual)return 0x80004003;
    if(pRequest->cBuffers<=0)return -1;
    if(pRequest->cbBuffer<=0)return -1;
    MemAllocator* me=(MemAllocator*)This;
    if(me->used_list.size() || me->free_list.size())return -1;
    me->props=*pRequest;
    *pActual=*pRequest;
    return 0;
}

HRESULT STDCALL MemAllocator::GetProperties ( 
    IMemAllocator * This,
    /* [out] */ ALLOCATOR_PROPERTIES *pProps)
{
    Debug printf("MemAllocator::GetProperties() called\n");
    if(!pProps)return -1;
    if(((MemAllocator*)This)->props.cbBuffer<0)return -1;
    *pProps=((MemAllocator*)This)->props;
    return 0;
}

HRESULT STDCALL MemAllocator::Commit ( 
    IMemAllocator * This)
{
    Debug printf("MemAllocator::Commit() called\n");
    MemAllocator* me=(MemAllocator*)This;
    if(((MemAllocator*)This)->props.cbBuffer<0)return -1;
    if(me->used_list.size() || me->free_list.size())return -1;
    for(int i=0; i<me->props.cBuffers; i++)
	me->free_list.push_back(new CMediaSample(me, me->props.cbBuffer));
    return 0;
}

HRESULT STDCALL MemAllocator::Decommit ( 
    IMemAllocator * This)
{
    Debug printf("MemAllocator::Decommit() called\n");
    MemAllocator* me=(MemAllocator*)This;
    list<CMediaSample*>::iterator it;
    for(it=me->free_list.begin(); it!=me->free_list.end(); it++)
	delete *it;
    for(it=me->used_list.begin(); it!=me->used_list.end(); it++)
	delete *it;
    me->free_list.clear();
    me->used_list.clear();	
    return 0;
}

HRESULT STDCALL MemAllocator::GetBuffer ( 
    IMemAllocator * This,
    /* [out] */ IMediaSample **ppBuffer,
    /* [in] */ REFERENCE_TIME *pStartTime,
    /* [in] */ REFERENCE_TIME *pEndTime,
    /* [in] */ DWORD dwFlags)
{
    Debug printf("%x: MemAllocator::GetBuffer() called\n", This);
    MemAllocator* me=(MemAllocator*)This;
    if(me->free_list.size()==0)
    {
	Debug printf("No samples available\n");
	return -1;//should block here if no samples are available
    }	
    list<CMediaSample*>::iterator it=me->free_list.begin();
    me->used_list.push_back(*it);
    *ppBuffer=*it;
    (*ppBuffer)->vt->AddRef((IUnknown*)*ppBuffer);
    if(me->new_pointer)
    {
	if(me->modified_sample)
	    me->modified_sample->ResetPointer();
        (*it)->SetPointer(me->new_pointer);
	me->modified_sample=*it;
	me->new_pointer=0;
    }
    me->free_list.remove(*it);
    return 0;
}

HRESULT STDCALL MemAllocator::ReleaseBuffer ( 
    IMemAllocator * This,
    /* [in] */ IMediaSample *pBuffer)
{
    Debug printf("%x: MemAllocator::ReleaseBuffer() called\n", This);
    MemAllocator* me=(MemAllocator*)This;
    list<CMediaSample*>::iterator it;
    for(it=me->used_list.begin(); it!=me->used_list.end(); it++)
	if(*it==pBuffer)
	{
	    me->used_list.erase(it);
	    me->free_list.push_back((CMediaSample*)pBuffer);
	    return 0;
	}
    Debug printf("Releasing unknown buffer\n");
    return -1;
}
