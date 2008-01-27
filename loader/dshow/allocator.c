/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#include "config.h"
#include "allocator.h"
#include "com.h"
#include "wine/winerror.h"
#include <stdio.h>
#include <stdlib.h>

static int AllocatorKeeper = 0;

struct avm_list_t
{
    struct avm_list_t* next;
    struct avm_list_t* prev;
    void* member;
};

static inline int avm_list_size(avm_list_t* head)
{
    avm_list_t* it = head;
    int i = 0;
    if (it)
    {
	for (;;)
	{
            i++;
	    it = it->next;
	    if (it == head)
                break;
	}
    }
    return i;
}

static inline int avm_list_print(avm_list_t* head)
{
    avm_list_t* it = head;
    int i = 0;
    printf("Head: %p\n", head);
    if (it)
    {
	for (;;)
	{
	    i++;
	    printf("%d:  member: %p    next: %p  prev: %p\n",
		   i, it->member, it->next, it->prev);
	    it = it->next;
	    if (it == head)
                break;
	}
    }
    return i;
}

static inline avm_list_t* avm_list_add_head(avm_list_t* head, void* member)
{
    avm_list_t* n = (avm_list_t*) malloc(sizeof(avm_list_t));
    n->member = member;

    if (!head)
    {
	head = n;
        head->prev = head;
    }

    n->prev = head->prev;
    head->prev = n;
    n->next = head;

    return n;
}

static inline avm_list_t* avm_list_add_tail(avm_list_t* head, void* member)
{
    avm_list_t* n = avm_list_add_head(head, member);
    return (!head) ? n : head;
}

static inline avm_list_t* avm_list_del_head(avm_list_t* head)
{
    avm_list_t* n = 0;

    if (head)
    {
	if (head->next != head)
	{
	    n = head->next;
	    head->prev->next = head->next;
	    head->next->prev = head->prev;
	}
	free(head);
    }
    return n;
}

static inline avm_list_t* avm_list_find(avm_list_t* head, void* member)
{
    avm_list_t* it = head;
    if (it)
    {
	for (;;)
	{
	    if (it->member == member)
		return it;
	    it = it->next;
	    if (it == head)
                break;
	}
    }
    return NULL;
}

static long MemAllocator_CreateAllocator(GUID* clsid, const GUID* iid, void** ppv)
{
    IMemAllocator* p;
    int result;
    if (!ppv)
	return -1;
    *ppv = 0;
    if (memcmp(clsid, &CLSID_MemoryAllocator, sizeof(GUID)))
	return -1;

    p = (IMemAllocator*) MemAllocatorCreate();
    result = p->vt->QueryInterface((IUnknown*)p, iid, ppv);
    p->vt->Release((IUnknown*)p);

    return result;
}

static HRESULT STDCALL MemAllocator_SetProperties(IMemAllocator * This,
						  /* [in] */ ALLOCATOR_PROPERTIES *pRequest,
						  /* [out] */ ALLOCATOR_PROPERTIES *pActual)
{
    MemAllocator* me = (MemAllocator*)This;
    Debug printf("MemAllocator_SetProperties(%p) called\n", This);
    if (!pRequest || !pActual)
	return E_INVALIDARG;
    if (pRequest->cBuffers<=0 || pRequest->cbBuffer<=0)
	return E_FAIL;
    if (me->used_list != 0 || me->free_list != 0)
	return E_FAIL;

    *pActual = *pRequest;
    /*
       DirectShow DOCS ("Negotiating Allocators" chapter) says that allocator might not
       honor the requested properties. Thus, since WMSP audio codecs requests bufer with two 
       bytes length for unknown reason, we should correct requested value. Otherwise above
       codec don't want to load.
    */
    if (pActual->cbBuffer == 2)
        pActual->cbBuffer = 10240; //Enough for WMSP codec

    me->props = *pActual;

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
    MemAllocator* me = (MemAllocator*)This;
    int i;
    Debug printf("MemAllocator_Commit(%p) called\n", This);
    if (((MemAllocator*)This)->props.cbBuffer < 0)
	return E_FAIL;
    if (me->used_list || me->free_list)
	return E_INVALIDARG;
    for (i = 0; i < me->props.cBuffers; i++)
    {
	CMediaSample* sample = CMediaSampleCreate((IMemAllocator*)me,
						  me->props.cbBuffer);
	if (!sample)
            return E_OUTOFMEMORY;
	//printf("FREEEEEEEEEEEE ADDED %p\n", sample);
	me->free_list = avm_list_add_tail(me->free_list, sample);
	//avm_list_print(me->free_list);
    }

    //printf("Added mem %p: lsz: %d  %d  size: %d\n", me, avm_list_size(me->free_list), me->props.cBuffers, me->props.cbBuffer);
    return 0;
}

static HRESULT STDCALL MemAllocator_Decommit(IMemAllocator * This)
{
    MemAllocator* me=(MemAllocator*)This;
    Debug printf("MemAllocator_Decommit(%p) called\n", This);
    //printf("Deleted mem %p: %d  %d\n", me, me->free_list.size(), me->used_list.size());
    while (me->used_list)
    {
	me->free_list = avm_list_add_tail(me->free_list,
					  (CMediaSample*) me->used_list->member);
	me->used_list = avm_list_del_head(me->used_list);
    }

    while (me->free_list)
    {
        CMediaSample* sample = (CMediaSample*) me->free_list->member;
	//printf("****************** Decommiting FREE %p\n", sample);
	//sample->vt->Release((IUnknown*)sample);
	CMediaSample_Destroy((CMediaSample*)sample);
	me->free_list = avm_list_del_head(me->free_list);
    }

    return 0;
}

static HRESULT STDCALL MemAllocator_GetBuffer(IMemAllocator * This,
					      /* [out] */ IMediaSample **ppBuffer,
					      /* [in] */ REFERENCE_TIME *pStartTime,
					      /* [in] */ REFERENCE_TIME *pEndTime,
					      /* [in] */ DWORD dwFlags)
{
    MemAllocator* me = (MemAllocator*)This;
    CMediaSample* sample;
    Debug printf("MemAllocator_ReleaseBuffer(%p) called   %d  %d\n", This,
		 avm_list_size(me->used_list), avm_list_size(me->free_list));

    if (!me->free_list)
    {
	Debug printf("No samples available\n");
	return E_FAIL;//should block here if no samples are available
    }

    sample = (CMediaSample*) me->free_list->member;
    me->free_list = avm_list_del_head(me->free_list);
    me->used_list = avm_list_add_tail(me->used_list, sample);

    *ppBuffer = (IMediaSample*) sample;
    sample->vt->AddRef((IUnknown*) sample);
    if (me->new_pointer)
    {
	if (me->modified_sample)
	    me->modified_sample->ResetPointer(me->modified_sample);
	sample->SetPointer(sample, me->new_pointer);
	me->modified_sample = sample;
	me->new_pointer = 0;
    }
    return 0;
}

static HRESULT STDCALL MemAllocator_ReleaseBuffer(IMemAllocator* This,
						  /* [in] */ IMediaSample* pBuffer)
{
    avm_list_t* l;
    MemAllocator* me = (MemAllocator*)This;
    Debug printf("MemAllocator_ReleaseBuffer(%p) called   %d  %d\n", This,
		 avm_list_size(me->used_list), avm_list_size(me->free_list));

    l = avm_list_find(me->used_list, pBuffer);
    if (l)
    {
	CMediaSample* sample = (CMediaSample*) l->member;
	if (me->modified_sample == sample)
	{
	    me->modified_sample->ResetPointer(me->modified_sample);
	    me->modified_sample = 0;
	}
	me->used_list = avm_list_del_head(me->used_list);
	me->free_list = avm_list_add_head(me->free_list, sample);
	//printf("****************** RELEASED OK %p  %p\n", me->used_list, me->free_list);
	return 0;
    }
    Debug printf("MemAllocator_ReleaseBuffer(%p) releasing unknown buffer!!!! %p\n", This, pBuffer);
    return E_FAIL;
}


static void MemAllocator_SetPointer(MemAllocator* This, char* pointer)
{
    This->new_pointer = pointer;
}

static void MemAllocator_ResetPointer(MemAllocator* This)
{
    if (This->modified_sample)
    {
	This->modified_sample->ResetPointer(This->modified_sample);
	This->modified_sample = 0;
    }
}

static void MemAllocator_Destroy(MemAllocator* This)
{
    Debug printf("MemAllocator_Destroy(%p) called  (%d, %d)\n", This, This->refcount, AllocatorKeeper);
#ifdef WIN32_LOADER
    if (--AllocatorKeeper == 0)
	UnregisterComClass(&CLSID_MemoryAllocator, MemAllocator_CreateAllocator);
#endif
    free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(MemAllocator)

MemAllocator* MemAllocatorCreate()
{
    MemAllocator* This = (MemAllocator*) malloc(sizeof(MemAllocator));

    if (!This)
        return NULL;

    Debug printf("MemAllocatorCreate() called -> %p\n", This);

    This->refcount = 1;
    This->props.cBuffers = 1;
    This->props.cbBuffer = 65536; /* :/ */
    This->props.cbAlign = 1;
    This->props.cbPrefix = 0;

    This->vt = (IMemAllocator_vt*) malloc(sizeof(IMemAllocator_vt));

    if (!This->vt)
    {
        free(This);
	return NULL;
    }

    This->vt->QueryInterface = MemAllocator_QueryInterface;
    This->vt->AddRef = MemAllocator_AddRef;
    This->vt->Release = MemAllocator_Release;
    This->vt->SetProperties = MemAllocator_SetProperties;
    This->vt->GetProperties = MemAllocator_GetProperties;
    This->vt->Commit = MemAllocator_Commit;
    This->vt->Decommit = MemAllocator_Decommit;
    This->vt->GetBuffer = MemAllocator_GetBuffer;
    This->vt->ReleaseBuffer = MemAllocator_ReleaseBuffer;

    This->SetPointer = MemAllocator_SetPointer;
    This->ResetPointer = MemAllocator_ResetPointer;

    This->modified_sample = 0;
    This->new_pointer = 0;
    This->used_list = 0;
    This->free_list = 0;

    This->interfaces[0]=IID_IUnknown;
    This->interfaces[1]=IID_IMemAllocator;

#ifdef WIN32_LOADER
    if (AllocatorKeeper++ == 0)
	RegisterComClass(&CLSID_MemoryAllocator, MemAllocator_CreateAllocator);
#endif

    return This;
}
