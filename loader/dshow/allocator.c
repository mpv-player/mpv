#include "allocator.h"
#include <com.h>
#include <wine/winerror.h>
#include <stdio.h>

//#undef Debug
//#define Debug

using namespace std;

class AllocatorKeeper
{
public:
    AllocatorKeeper()
    {
	RegisterComClass(&CLSID_MemoryAllocator, MemAllocator::CreateAllocator);
    }
    ~AllocatorKeeper()
    {
	UnregisterComClass(&CLSID_MemoryAllocator, MemAllocator::CreateAllocator);
    }
};
static AllocatorKeeper keeper;


GUID MemAllocator::interfaces[]=
{
    IID_IUnknown,
    IID_IMemAllocator,
};

IMPLEMENT_IUNKNOWN(MemAllocator)

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


static HRESULT STDCALL MemAllocator_SetProperties(IMemAllocator * This,
						  /* [in] */ ALLOCATOR_PROPERTIES *pRequest,
						  /* [out] */ ALLOCATOR_PROPERTIES *pActual)
{
    Debug printf("MemAllocator_SetProperties() called\n");
    if (!pRequest || !pActual)
	return E_INVALIDARG;
    if (pRequest->cBuffers<=0 || pRequest->cbBuffer<=0)
	return E_FAIL;
    MemAllocator* me = (MemAllocator*)This;
    if (me->used_list.size() || me->free_list.size())
	return E_FAIL;
    me->props = *pRequest;
    *pActual = *pRequest;
    return 0;
}

static HRESULT STDCALL MemAllocator_GetProperties(IMemAllocator * This,
						  /* [out] */ ALLOCATOR_PROPERTIES *pProps)
{
    Debug printf("MemAllocator_GetProperties(%p) called\n", This);
    if (!pProps)
	return E_INVALIDARG;
    if (((MemAllocator*)This)->props.cbBuffer<0)
	return E_FAIL;
    *pProps=((MemAllocator*)This)->props;
    return 0;
}

static HRESULT STDCALL MemAllocator_Commit(IMemAllocator * This)
{
    Debug printf("MemAllocator_Commit(%p) called\n", This);
    MemAllocator* me=(MemAllocator*)This;
    if (((MemAllocator*)This)->props.cbBuffer < 0)
	return E_FAIL;
    if (me->used_list.size() || me->free_list.size())
	return E_INVALIDARG;
    for(int i = 0; i<me->props.cBuffers; i++)
	me->free_list.push_back(new CMediaSample(me, me->props.cbBuffer));

    return 0;
}

static HRESULT STDCALL MemAllocator_Decommit(IMemAllocator * This)
{
    Debug printf("MemAllocator_Decommit(%p) called\n", This);
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

static HRESULT STDCALL MemAllocator_GetBuffer(IMemAllocator * This,
					      /* [out] */ IMediaSample **ppBuffer,
					      /* [in] */ REFERENCE_TIME *pStartTime,
					      /* [in] */ REFERENCE_TIME *pEndTime,
					      /* [in] */ DWORD dwFlags)
{
    Debug printf("MemAllocator_GetBuffer(%p) called\n", This);
    MemAllocator* me = (MemAllocator*)This;
    if (me->free_list.size() == 0)
    {
	Debug printf("No samples available\n");
	return E_FAIL;//should block here if no samples are available
    }
    list<CMediaSample*>::iterator it = me->free_list.begin();
    me->used_list.push_back(*it);
    *ppBuffer = *it;
    (*ppBuffer)->vt->AddRef((IUnknown*)*ppBuffer);
    if (me->new_pointer)
    {
	if(me->modified_sample)
	    me->modified_sample->ResetPointer();
	(*it)->SetPointer(me->new_pointer);
	me->modified_sample=*it;
	me->new_pointer = 0;
    }
    me->free_list.remove(*it);
    return 0;
}

static HRESULT STDCALL MemAllocator_ReleaseBuffer(IMemAllocator * This,
						  /* [in] */ IMediaSample *pBuffer)
{
    Debug printf("MemAllocator_ReleaseBuffer(%p) called\n", This);
    MemAllocator* me = (MemAllocator*)This;
    list<CMediaSample*>::iterator it;
    for (it = me->used_list.begin(); it != me->used_list.end(); it++)
	if (*it == pBuffer)
	{
	    me->used_list.erase(it);
	    me->free_list.push_back((CMediaSample*)pBuffer);
	    return 0;
	}
    Debug printf("Releasing unknown buffer\n");
    return E_FAIL;
}

MemAllocator::MemAllocator()
{
    Debug printf("MemAllocator::MemAllocator() called\n");
    vt = new IMemAllocator_vt;
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->SetProperties = MemAllocator_SetProperties;
    vt->GetProperties = MemAllocator_GetProperties;
    vt->Commit = MemAllocator_Commit;
    vt->Decommit = MemAllocator_Decommit;
    vt->GetBuffer = MemAllocator_GetBuffer;
    vt->ReleaseBuffer = MemAllocator_ReleaseBuffer;

    refcount = 1;
    props.cBuffers = 1;
    props.cbBuffer = 65536; /* :/ */
    props.cbAlign = props.cbPrefix = 0;

    new_pointer=0;
    modified_sample=0;
}

MemAllocator::~MemAllocator()
{
    Debug printf("MemAllocator::~MemAllocator() called\n");
    delete vt;
}
