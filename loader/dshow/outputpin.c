
#include "wine/winerror.h"
#include "wine/windef.h"
#include "outputpin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
    An object beyond interface IEnumMediaTypes.
    Returned by COutputPin through call IPin::EnumMediaTypes().
*/

static inline int output_unimplemented(const char* s, void* p)
{
    Debug printf("%s(%p) called (UNIMPLEMENTED)", s, p);
    return E_NOTIMPL;
}

typedef struct CEnumMediaTypes
{
    IEnumMediaTypes_vt* vt;
    DECLARE_IUNKNOWN();
    AM_MEDIA_TYPE type;
    GUID interfaces[2];
} CEnumMediaTypes;

struct _COutputMemPin
{
    IMemInputPin_vt* vt;
    DECLARE_IUNKNOWN();
    char** frame_pointer;
    long* frame_size_pointer;
    MemAllocator* pAllocator;
    COutputPin* parent;
};

static HRESULT STDCALL CEnumMediaTypes_Next(IEnumMediaTypes * This,
					    /* [in] */ ULONG cMediaTypes,
					    /* [size_is][out] */ AM_MEDIA_TYPE **ppMediaTypes,
					    /* [out] */ ULONG *pcFetched)
{
    AM_MEDIA_TYPE* type = &((CEnumMediaTypes*)This)->type;
    Debug printf("CEnumMediaTypes::Next(%p) called\n", This);
    if (!ppMediaTypes)
	return E_INVALIDARG;
    if (!pcFetched && (cMediaTypes!=1))
	return E_INVALIDARG;
    if (cMediaTypes <= 0)
	return 0;

    if (pcFetched)
	*pcFetched=1;
    ppMediaTypes[0] = (AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    // copy structures - C can handle this...
    **ppMediaTypes = *type;
    if (ppMediaTypes[0]->pbFormat)
    {
	ppMediaTypes[0]->pbFormat=(char *)CoTaskMemAlloc(ppMediaTypes[0]->cbFormat);
	memcpy(ppMediaTypes[0]->pbFormat, type->pbFormat, ppMediaTypes[0]->cbFormat);
    }
    if (cMediaTypes == 1)
	return 0;
    return 1;
}

/* I expect that these methods are unused. */
static HRESULT STDCALL CEnumMediaTypes_Skip(IEnumMediaTypes * This,
					    /* [in] */ ULONG cMediaTypes)
{
    return output_unimplemented("CEnumMediaTypes::Skip", This);
}

static HRESULT STDCALL CEnumMediaTypes_Reset(IEnumMediaTypes * This)
{
    Debug printf("CEnumMediaTypes::Reset(%p) called\n", This);
    return 0;
}

static HRESULT STDCALL CEnumMediaTypes_Clone(IEnumMediaTypes * This,
				      /* [out] */ IEnumMediaTypes **ppEnum)
{
    Debug printf("CEnumMediaTypes::Clone(%p) called\n", This);
    return E_NOTIMPL;
}

void CEnumMediaTypes_Destroy(CEnumMediaTypes* This)
{
    free(This->vt);
    free(This);
}

// IPin->IUnknown methods
IMPLEMENT_IUNKNOWN(CEnumMediaTypes)

CEnumMediaTypes* CEnumMediaTypesCreate(const AM_MEDIA_TYPE* amt)
{
    CEnumMediaTypes *This = (CEnumMediaTypes*) malloc(sizeof(CEnumMediaTypes)) ;

    if (!This)
        return NULL;

    This->vt = (IEnumMediaTypes_vt*) malloc(sizeof(IEnumMediaTypes_vt));
    if (!This->vt)
    {
	free(This);
	return NULL;
    }

    This->refcount = 1;
    This->type = *amt;

    This->vt->QueryInterface = CEnumMediaTypes_QueryInterface;
    This->vt->AddRef = CEnumMediaTypes_AddRef;
    This->vt->Release = CEnumMediaTypes_Release;
    This->vt->Next = CEnumMediaTypes_Next;
    This->vt->Skip = CEnumMediaTypes_Skip;
    This->vt->Reset = CEnumMediaTypes_Reset;
    This->vt->Clone = CEnumMediaTypes_Clone;

    This->interfaces[0] = IID_IUnknown;
    This->interfaces[1] = IID_IEnumMediaTypes;

    return This;
}


/*************
 * COutputPin
 *************/


static HRESULT STDCALL COutputPin_QueryInterface(IUnknown* This, const GUID* iid, void** ppv)
{
    COutputPin* p = (COutputPin*) This;

    Debug printf("COutputPin_QueryInterface(%p) called\n", This);
    if (!ppv)
	return E_INVALIDARG;

    if (memcmp(iid, &IID_IUnknown, 16) == 0)
    {
	*ppv = p;
	p->vt->AddRef(This);
        return 0;
    }
    if (memcmp(iid, &IID_IMemInputPin, 16) == 0)
    {
	*ppv = p->mempin;
	p->mempin->vt->AddRef((IUnknown*)*ppv);
	return 0;
    }

    Debug printf("Unknown interface : %08x-%04x-%04x-%02x%02x-"
		 "%02x%02x%02x%02x%02x%02x\n",
		 iid->f1,  iid->f2,  iid->f3,
		 (unsigned char)iid->f4[1], (unsigned char)iid->f4[0],
		 (unsigned char)iid->f4[2], (unsigned char)iid->f4[3],
		 (unsigned char)iid->f4[4], (unsigned char)iid->f4[5],
		 (unsigned char)iid->f4[6], (unsigned char)iid->f4[7]);
    return E_NOINTERFACE;
}

// IPin methods
static HRESULT STDCALL COutputPin_Connect(IPin * This,
				    /* [in] */ IPin *pReceivePin,
				    /* [in] */ /* const */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("COutputPin_Connect() called\n");
/*
    *pmt=((COutputPin*)This)->type;
    if(pmt->cbFormat>0)
    {
	pmt->pbFormat=CoTaskMemAlloc(pmt->cbFormat);
	memcpy(pmt->pbFormat, ((COutputPin*)This)->type.pbFormat, pmt->cbFormat);
    }
*/
    //return E_NOTIMPL;
    return 0;// XXXXXXXXXXXXX CHECKME XXXXXXXXXXXXXXX
    // if I put return 0; here, it crashes
}

static HRESULT STDCALL COutputPin_ReceiveConnection(IPin * This,
						    /* [in] */ IPin *pConnector,
						    /* [in] */ const AM_MEDIA_TYPE *pmt)
{
    Debug printf("COutputPin_ReceiveConnection(%p) called\n", This);
    ((COutputPin*)This)->remote = pConnector;
    return 0;
}

static HRESULT STDCALL COutputPin_Disconnect(IPin * This)
{
    Debug printf("COutputPin_Disconnect(%p) called\n", This);
    return 1;
}

static HRESULT STDCALL COutputPin_ConnectedTo(IPin * This,
					/* [out] */ IPin **pPin)
{
    Debug printf("COutputPin_ConnectedTo(%p) called\n", This);
    if (!pPin)
	return E_INVALIDARG;
    *pPin = ((COutputPin*)This)->remote;
    return 0;
}

static HRESULT STDCALL COutputPin_ConnectionMediaType(IPin * This,
						      /* [out] */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("CInputPin::ConnectionMediaType() called\n");
    if (!pmt)
	return E_INVALIDARG;
    *pmt = ((COutputPin*)This)->type;
    if (pmt->cbFormat>0)
    {
	pmt->pbFormat=(char *)CoTaskMemAlloc(pmt->cbFormat);
	memcpy(pmt->pbFormat, ((COutputPin*)This)->type.pbFormat, pmt->cbFormat);
    }
    return 0;
}

static HRESULT STDCALL COutputPin_QueryPinInfo(IPin * This,
					       /* [out] */ PIN_INFO *pInfo)
{
    return output_unimplemented("COutputPin_QueryPinInfo", This);
}

static HRESULT STDCALL COutputPin_QueryDirection(IPin * This,
					   /* [out] */ PIN_DIRECTION *pPinDir)
{
    Debug printf("COutputPin_QueryDirection(%p) called\n", This);
    if (!pPinDir)
	return E_INVALIDARG;
    *pPinDir = PINDIR_INPUT;
    return 0;
}

static HRESULT STDCALL COutputPin_QueryId(IPin * This,
					  /* [out] */ LPWSTR *Id)
{
    return output_unimplemented("COutputPin_QueryId", This);
}

static HRESULT STDCALL COutputPin_QueryAccept(IPin * This,
					      /* [in] */ const AM_MEDIA_TYPE *pmt)
{
    return output_unimplemented("COutputPin_QueryAccept", This);
}

static HRESULT STDCALL COutputPin_EnumMediaTypes(IPin * This,
					   /* [out] */ IEnumMediaTypes **ppEnum)
{
    Debug printf("COutputPin_EnumMediaTypes() called\n");
    if (!ppEnum)
	return E_INVALIDARG;
    *ppEnum = (IEnumMediaTypes*) CEnumMediaTypesCreate(&((COutputPin*)This)->type);
    return 0;
}

static HRESULT STDCALL COutputPin_QueryInternalConnections(IPin * This,
						     /* [out] */ IPin **apPin,
						     /* [out][in] */ ULONG *nPin)
{
    return output_unimplemented("COutputPin_QueryInternalConnections", This);
}

static HRESULT STDCALL COutputPin_EndOfStream(IPin * This)
{
    return output_unimplemented("COutputPin_EndOfStream", This);
}

static HRESULT STDCALL COutputPin_BeginFlush(IPin * This)
{
    return output_unimplemented("COutputPin_BeginFlush", This);
}

static HRESULT STDCALL COutputPin_EndFlush(IPin * This)
{
    return output_unimplemented("COutputPin_EndFlush", This);
}

static HRESULT STDCALL COutputPin_NewSegment(IPin * This,
				       /* [in] */ REFERENCE_TIME tStart,
				       /* [in] */ REFERENCE_TIME tStop,
				       /* [in] */ double dRate)
{
    Debug printf("COutputPin_NewSegment(%Ld,%Ld,%f) called\n",
		 tStart, tStop, dRate);
    return 0;
}



// IMemInputPin->IUnknown methods

static HRESULT STDCALL COutputPin_M_QueryInterface(IUnknown* This, const GUID* iid, void** ppv)
{
    COutputPin* p = (COutputPin*)This;

    Debug printf("COutputPin_M_QueryInterface(%p) called\n", This);
    if (!ppv)
	return E_INVALIDARG;

    if(!memcmp(iid, &IID_IUnknown, 16))
    {
	*ppv = p;
	p->vt->AddRef(This);
	return 0;
    }
    /*if(!memcmp(iid, &IID_IPin, 16))
    {
	COutputPin* ptr=(COutputPin*)(This-1);
	*ppv=(void*)ptr;
	AddRef((IUnknown*)ptr);
	return 0;
    }*/
    if(!memcmp(iid, &IID_IMemInputPin, 16))
    {
	*ppv = p->mempin;
	p->mempin->vt->AddRef(This);
	return 0;
    }
    Debug printf("Unknown interface : %08x-%04x-%04x-%02x%02x-" \
		 "%02x%02x%02x%02x%02x%02x\n",
		 iid->f1,  iid->f2,  iid->f3,
		 (unsigned char)iid->f4[1], (unsigned char)iid->f4[0],
		 (unsigned char)iid->f4[2], (unsigned char)iid->f4[3],
		 (unsigned char)iid->f4[4], (unsigned char)iid->f4[5],
		 (unsigned char)iid->f4[6], (unsigned char)iid->f4[7]);
    return E_NOINTERFACE;
}

// IMemInputPin methods

static HRESULT STDCALL COutputPin_GetAllocator(IMemInputPin* This,
					 /* [out] */ IMemAllocator** ppAllocator)
{
    Debug printf("COutputPin_GetAllocator(%p, %p) called\n", This->vt, ppAllocator);
    *ppAllocator = (IMemAllocator*) MemAllocatorCreate();
    return 0;
}

static HRESULT STDCALL COutputPin_NotifyAllocator(IMemInputPin* This,
						  /* [in] */ IMemAllocator* pAllocator,
						  /* [in] */ int bReadOnly)
{
    Debug printf("COutputPin_NotifyAllocator(%p, %p) called\n", This, pAllocator);
    ((COutputMemPin*)This)->pAllocator = (MemAllocator*) pAllocator;
    return 0;
}

static HRESULT STDCALL COutputPin_GetAllocatorRequirements(IMemInputPin* This,
							   /* [out] */ ALLOCATOR_PROPERTIES* pProps)
{
    return output_unimplemented("COutputPin_GetAllocatorRequirements", This);
}

static HRESULT STDCALL COutputPin_Receive(IMemInputPin* This,
					  /* [in] */ IMediaSample* pSample)
{
    COutputMemPin* mp = (COutputMemPin*)This;
    char* pointer;
    int len;

    Debug printf("COutputPin_Receive(%p) called\n", This);
    if (!pSample)
	return E_INVALIDARG;
    if (pSample->vt->GetPointer(pSample, (BYTE**) &pointer))
	return -1;
    len = pSample->vt->GetActualDataLength(pSample);
    if (len == 0)
	len = pSample->vt->GetSize(pSample);//for iv50
    //if(me.frame_pointer)memcpy(me.frame_pointer, pointer, len);

    if (mp->frame_pointer)
	*(mp->frame_pointer) = pointer;
    if (mp->frame_size_pointer)
	*(mp->frame_size_pointer) = len;
/*
    FILE* file=fopen("./uncompr.bmp", "wb");
    char head[14]={0x42, 0x4D, 0x36, 0x10, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00};
    *(int*)(&head[2])=len+0x36;
    fwrite(head, 14, 1, file);
    fwrite(&((VIDEOINFOHEADER*)me.type.pbFormat)->bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(pointer, len, 1, file);
    fclose(file);
*/
//    pSample->vt->Release((IUnknown*)pSample);

    return 0;
}

static HRESULT STDCALL COutputPin_ReceiveMultiple(IMemInputPin * This,
					    /* [size_is][in] */ IMediaSample **pSamples,
					    /* [in] */ long nSamples,
					    /* [out] */ long *nSamplesProcessed)
{
    return output_unimplemented("COutputPin_ReceiveMultiple", This);
}

static HRESULT STDCALL COutputPin_ReceiveCanBlock(IMemInputPin * This)
{
    return output_unimplemented("COutputPin_ReceiveCanBlock", This);
}

static void COutputPin_SetFramePointer(COutputPin* This, char** z)
{
    This->mempin->frame_pointer = z;
}

static void COutputPin_SetPointer2(COutputPin* This, char* p)
{
    if (This->mempin->pAllocator)
        // fixme
	This->mempin->pAllocator->SetPointer(This->mempin->pAllocator, p);
}

static void COutputPin_SetFrameSizePointer(COutputPin* This, long* z)
{
    This->mempin->frame_size_pointer = z;
}

static void COutputPin_SetNewFormat(COutputPin* This, const AM_MEDIA_TYPE* amt)
{
    This->type = *amt;
}

static void COutputPin_Destroy(COutputPin* This)
{
    if (This->mempin->vt)
	free(This->mempin->vt);
    if (This->mempin)
	free(This->mempin);
    if (This->vt)
	free(This->vt);
    free(This);
}

static HRESULT STDCALL COutputPin_AddRef(IUnknown* This)
{
    Debug printf("COutputPin_AddRef(%p) called (%d)\n", This, ((COutputPin*)This)->refcount);
    ((COutputPin*)This)->refcount++;
    return 0;
}

static HRESULT STDCALL COutputPin_Release(IUnknown* This)
{
    Debug printf("COutputPin_Release(%p) called (%d)\n", This, ((COutputPin*)This)->refcount);
    if (--((COutputPin*)This)->refcount <= 0)
	COutputPin_Destroy((COutputPin*)This);

    return 0;
}

static HRESULT STDCALL COutputPin_M_AddRef(IUnknown* This)
{
    COutputMemPin* p = (COutputMemPin*) This;
    Debug printf("COutputPin_MAddRef(%p) called (%p, %d)\n", p, p->parent, p->parent->refcount);
    p->parent->refcount++;
    return 0;
}

static HRESULT STDCALL COutputPin_M_Release(IUnknown* This)
{
    COutputMemPin* p = (COutputMemPin*) This;
    Debug printf("COutputPin_MRelease(%p) called (%p,   %d)\n",
		 p, p->parent, p->parent->refcount);
    if (--p->parent->refcount <= 0)
	COutputPin_Destroy(p->parent);
    return 0;
}

COutputPin* COutputPinCreate(const AM_MEDIA_TYPE* amt)
{
    COutputPin* This = (COutputPin*) malloc(sizeof(COutputPin));
    IMemInputPin_vt* ivt;

    if (!This)
        return NULL;

    This->vt = (IPin_vt*) malloc(sizeof(IPin_vt));
    This->mempin = (COutputMemPin*) malloc(sizeof(COutputMemPin));
    ivt = (IMemInputPin_vt*) malloc(sizeof(IMemInputPin_vt));

    if (!This->vt || !This->mempin || !ivt)
    {
        COutputPin_Destroy(This);
	return NULL;
    }

    This->mempin->vt = ivt;

    This->refcount = 1;
    This->remote = 0;
    This->type = *amt;

    This->vt->QueryInterface = COutputPin_QueryInterface;
    This->vt->AddRef = COutputPin_AddRef;
    This->vt->Release = COutputPin_Release;
    This->vt->Connect = COutputPin_Connect;
    This->vt->ReceiveConnection = COutputPin_ReceiveConnection;
    This->vt->Disconnect = COutputPin_Disconnect;
    This->vt->ConnectedTo = COutputPin_ConnectedTo;
    This->vt->ConnectionMediaType = COutputPin_ConnectionMediaType;
    This->vt->QueryPinInfo = COutputPin_QueryPinInfo;
    This->vt->QueryDirection = COutputPin_QueryDirection;
    This->vt->QueryId = COutputPin_QueryId;
    This->vt->QueryAccept = COutputPin_QueryAccept;
    This->vt->EnumMediaTypes = COutputPin_EnumMediaTypes;
    This->vt->QueryInternalConnections = COutputPin_QueryInternalConnections;
    This->vt->EndOfStream = COutputPin_EndOfStream;
    This->vt->BeginFlush = COutputPin_BeginFlush;
    This->vt->EndFlush = COutputPin_EndFlush;
    This->vt->NewSegment = COutputPin_NewSegment;

    This->mempin->vt->QueryInterface = COutputPin_M_QueryInterface;
    This->mempin->vt->AddRef = COutputPin_M_AddRef;
    This->mempin->vt->Release = COutputPin_M_Release;
    This->mempin->vt->GetAllocator = COutputPin_GetAllocator;
    This->mempin->vt->NotifyAllocator = COutputPin_NotifyAllocator;
    This->mempin->vt->GetAllocatorRequirements = COutputPin_GetAllocatorRequirements;
    This->mempin->vt->Receive = COutputPin_Receive;
    This->mempin->vt->ReceiveMultiple = COutputPin_ReceiveMultiple;
    This->mempin->vt->ReceiveCanBlock = COutputPin_ReceiveCanBlock;

    This->mempin->frame_size_pointer = 0;
    This->mempin->frame_pointer = 0;
    This->mempin->pAllocator = 0;
    This->mempin->refcount = 1;
    This->mempin->parent = This;

    This->SetPointer2 = COutputPin_SetPointer2;
    This->SetFramePointer = COutputPin_SetFramePointer;
    This->SetFrameSizePointer = COutputPin_SetFrameSizePointer;
    This->SetNewFormat = COutputPin_SetNewFormat;

    return This;
}
