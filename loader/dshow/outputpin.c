#include "outputpin.h"
#include "allocator.h"
#include "iunk.h"
#include <wine/winerror.h>
#include <cstdio>
#include <cstring>

/*
    An object beyond interface IEnumMediaTypes.
    Returned by COutputPin through call IPin::EnumMediaTypes().
*/

using namespace std;

class CEnumMediaTypes: public IEnumMediaTypes
{
public:
    AM_MEDIA_TYPE type;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(CEnumMediaTypes)
    CEnumMediaTypes(const AM_MEDIA_TYPE&);
    ~CEnumMediaTypes() {delete vt;}
};

GUID CEnumMediaTypes::interfaces[]=
{
    IID_IUnknown,
    IID_IEnumMediaTypes,
};

// IPin->IUnknown methods
IMPLEMENT_IUNKNOWN(CEnumMediaTypes)


static HRESULT STDCALL CEnumMediaTypes_Next(IEnumMediaTypes * This,
					    /* [in] */ ULONG cMediaTypes,
					    /* [size_is][out] */ AM_MEDIA_TYPE **ppMediaTypes,
					    /* [out] */ ULONG *pcFetched)
{
    AM_MEDIA_TYPE& type=((CEnumMediaTypes*)This)->type;
    Debug printf("CEnumMediaTypes::Next() called\n");
    if (!ppMediaTypes)
	return E_INVALIDARG;
    if (!pcFetched && (cMediaTypes!=1))
	return E_INVALIDARG;
    if (cMediaTypes <= 0)
	return 0;

    if (pcFetched)
	*pcFetched=1;
    ppMediaTypes[0] = (AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    memcpy(*ppMediaTypes, &type, sizeof(AM_MEDIA_TYPE));
    if (ppMediaTypes[0]->pbFormat)
    {
	ppMediaTypes[0]->pbFormat=(char *)CoTaskMemAlloc(ppMediaTypes[0]->cbFormat);
	memcpy(ppMediaTypes[0]->pbFormat, type.pbFormat, ppMediaTypes[0]->cbFormat);
    }
    if (cMediaTypes == 1)
	return 0;
    return 1;
}

/* I expect that these methods are unused. */
static HRESULT STDCALL CEnumMediaTypes_Skip(IEnumMediaTypes * This,
					    /* [in] */ ULONG cMediaTypes)
{
    Debug printf("CEnumMediaTypes::Skip() called\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL CEnumMediaTypes_Reset(IEnumMediaTypes * This)
{
    Debug printf("CEnumMediaTypes::Reset() called\n");
    return 0;
}

static HRESULT STDCALL CEnumMediaTypes_Clone(IEnumMediaTypes * This,
				      /* [out] */ IEnumMediaTypes **ppEnum)
{
    Debug printf("CEnumMediaTypes::Clone() called\n");
    return E_NOTIMPL;
}


CEnumMediaTypes::CEnumMediaTypes(const AM_MEDIA_TYPE& amtype)
{
    refcount = 1;
    type = amtype;

    vt = new IEnumMediaTypes_vt;
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->Next = CEnumMediaTypes_Next;
    vt->Skip = CEnumMediaTypes_Skip;
    vt->Reset = CEnumMediaTypes_Reset;
    vt->Clone = CEnumMediaTypes_Clone;
}


static HRESULT STDCALL COutputPin_AddRef(IUnknown* This)
{
    Debug printf("COutputPin_AddRef(%p) called (%d)\n",
		 This, ((COutputPin*)This)->refcount);
    ((COutputPin*)This)->refcount++;
    return 0;
}

static HRESULT STDCALL COutputPin_Release(IUnknown* This)
{
    Debug printf("COutputPin_Release(%p) called (%d)\n",
		 This, ((COutputPin*)This)->refcount);
    if (--((COutputPin*)This)->refcount<=0)
	delete (COutputPin*)This;

    return 0;
}

static HRESULT STDCALL COutputPin_M_AddRef(IUnknown* This)
{
    COutputMemPin* p = (COutputMemPin*) This;
    Debug printf("COutputPin_MAddRef(%p) called (%p,   %d)\n",
		 p, p->parent, p->parent->refcount);
    p->parent->refcount++;
    return 0;
}

static HRESULT STDCALL COutputPin_M_Release(IUnknown* This)
{
    COutputMemPin* p = (COutputMemPin*) This;
    Debug printf("COutputPin_MRelease(%p) called (%p,   %d)\n",
		 p, p->parent, p->parent->refcount);
    if (--p->parent->refcount <= 0)
	delete p->parent;
    return 0;
}

/* Implementation of output pin object. */
// Constructor


static HRESULT STDCALL COutputPin_QueryInterface(IUnknown* This, GUID* iid, void** ppv)
{
    Debug printf("COutputPin_QueryInterface(%p) called\n", This);
    if (!ppv)
	return E_INVALIDARG;

    COutputPin* p = (COutputPin*) This;

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

    Debug printf("Unknown interface : %08x-%04x-%04x-%02x%02x-" \
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
    Debug printf("COutputPin_ReceiveConnection() called\n");
    ((COutputPin*)This)->remote=pConnector;
    return 0;
}

static HRESULT STDCALL COutputPin_Disconnect(IPin * This)
{
    Debug printf("COutputPin_Disconnect() called\n");
    return 1;
}

static HRESULT STDCALL COutputPin_ConnectedTo(IPin * This,
					/* [out] */ IPin **pPin)
{
    Debug printf("COutputPin_ConnectedTo() called\n");
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
    Debug printf("COutputPin_QueryPinInfo() called\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL COutputPin_QueryDirection(IPin * This,
					   /* [out] */ PIN_DIRECTION *pPinDir)
{
    Debug printf("COutputPin_QueryDirection() called\n");
    if (!pPinDir)
	return E_INVALIDARG;
    *pPinDir = PINDIR_INPUT;
    return 0;
}

static HRESULT STDCALL COutputPin_QueryId(IPin * This,
					  /* [out] */ LPWSTR *Id)
{
    Debug printf("COutputPin_QueryId() called\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL COutputPin_QueryAccept(IPin * This,
					      /* [in] */ const AM_MEDIA_TYPE *pmt)
{
    Debug printf("COutputPin_QueryAccept() called\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL COutputPin_EnumMediaTypes(IPin * This,
					   /* [out] */ IEnumMediaTypes **ppEnum)
{
    Debug printf("COutputPin_EnumMediaTypes() called\n");
    if (!ppEnum)
	return E_INVALIDARG;
    *ppEnum=new CEnumMediaTypes(((COutputPin*)This)->type);
    return 0;
}

static HRESULT STDCALL COutputPin_QueryInternalConnections(IPin * This,
						     /* [out] */ IPin **apPin,
						     /* [out][in] */ ULONG *nPin)
{
    Debug printf("COutputPin_QueryInternalConnections() called\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL COutputPin_EndOfStream(IPin * This)
{
    Debug printf("COutputPin_EndOfStream() called\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL COutputPin_BeginFlush(IPin * This)
{
    Debug printf("COutputPin_BeginFlush() called\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL COutputPin_EndFlush(IPin * This)
{
    Debug printf("COutputPin_EndFlush() called\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL COutputPin_NewSegment(IPin * This,
				       /* [in] */ REFERENCE_TIME tStart,
				       /* [in] */ REFERENCE_TIME tStop,
				       /* [in] */ double dRate)
{
    Debug printf("COutputPin_NewSegment(%ld,%ld,%f) called\n",
		 tStart, tStop, dRate);
    return 0;
}



// IMemInputPin->IUnknown methods

static HRESULT STDCALL COutputPin_M_QueryInterface(IUnknown* This, GUID* iid, void** ppv)
{
    Debug printf("COutputPin_M_QueryInterface() called\n");
    if (!ppv)
	return E_INVALIDARG;

    COutputPin* p = (COutputPin*)This;
    if(!memcmp(iid, &IID_IUnknown, 16))
    {
	*ppv=p;
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
	*ppv=p->mempin;
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

static HRESULT STDCALL COutputPin_GetAllocator(IMemInputPin * This,
					 /* [out] */ IMemAllocator **ppAllocator)
{
    Debug printf("COutputPin_GetAllocator(%p, %p) called\n", This->vt, ppAllocator);
    *ppAllocator=new MemAllocator;
    return 0;
}
    
static HRESULT STDCALL COutputPin_NotifyAllocator(IMemInputPin * This,
						  /* [in] */ IMemAllocator *pAllocator,
						  /* [in] */ int bReadOnly)
{
    Debug printf("COutputPin_NotifyAllocator(%p, %p) called\n", This, pAllocator);
    ((COutputMemPin*)This)->pAllocator = (MemAllocator*) pAllocator;
    return 0;
}

static HRESULT STDCALL COutputPin_GetAllocatorRequirements(IMemInputPin * This,
							   /* [out] */ ALLOCATOR_PROPERTIES *pProps)
{
    Debug printf("COutputPin_GetAllocatorRequirements() called\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL COutputPin_Receive(IMemInputPin * This,
				    /* [in] */ IMediaSample *pSample)
{
    Debug printf("COutputPin_Receive(%p) called\n", This);
    if (!pSample)
	return E_INVALIDARG;
    char* pointer;
    if (pSample->vt->GetPointer(pSample, (BYTE **)&pointer))
	return -1;
    int len = pSample->vt->GetActualDataLength(pSample);
    if (len == 0)
	len = pSample->vt->GetSize(pSample);//for iv50
    //if(me.frame_pointer)memcpy(me.frame_pointer, pointer, len);

    COutputMemPin* mp= (COutputMemPin*)This;
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
    Debug printf("COutputPin_ReceiveMultiple() called (UNIMPLEMENTED)\n");
    return E_NOTIMPL;
}

static HRESULT STDCALL COutputPin_ReceiveCanBlock(IMemInputPin * This)
{
    Debug printf("COutputPin_ReceiveCanBlock() called (UNIMPLEMENTED)\n");
    return E_NOTIMPL;
}

COutputPin::COutputPin(const AM_MEDIA_TYPE& vh)
    :refcount(1), type(vh), remote(0)
{
    vt = new IPin_vt;
    vt->QueryInterface = COutputPin_QueryInterface;
    vt->AddRef = COutputPin_AddRef;
    vt->Release = COutputPin_Release;
    vt->Connect = COutputPin_Connect;
    vt->ReceiveConnection = COutputPin_ReceiveConnection;
    vt->Disconnect = COutputPin_Disconnect;
    vt->ConnectedTo = COutputPin_ConnectedTo;
    vt->ConnectionMediaType = COutputPin_ConnectionMediaType;
    vt->QueryPinInfo = COutputPin_QueryPinInfo;
    vt->QueryDirection = COutputPin_QueryDirection;
    vt->QueryId = COutputPin_QueryId;
    vt->QueryAccept = COutputPin_QueryAccept;
    vt->EnumMediaTypes = COutputPin_EnumMediaTypes;
    vt->QueryInternalConnections = COutputPin_QueryInternalConnections;
    vt->EndOfStream = COutputPin_EndOfStream;
    vt->BeginFlush = COutputPin_BeginFlush;
    vt->EndFlush = COutputPin_EndFlush;
    vt->NewSegment = COutputPin_NewSegment;

    mempin = new COutputMemPin;
    mempin->vt = new IMemInputPin_vt;
    mempin->vt->QueryInterface = COutputPin_M_QueryInterface;
    mempin->vt->AddRef = COutputPin_M_AddRef;
    mempin->vt->Release = COutputPin_M_Release;
    mempin->vt->GetAllocator = COutputPin_GetAllocator;
    mempin->vt->NotifyAllocator = COutputPin_NotifyAllocator;
    mempin->vt->GetAllocatorRequirements = COutputPin_GetAllocatorRequirements;
    mempin->vt->Receive = COutputPin_Receive;
    mempin->vt->ReceiveMultiple = COutputPin_ReceiveMultiple;
    mempin->vt->ReceiveCanBlock = COutputPin_ReceiveCanBlock;

    mempin->frame_size_pointer = 0;
    mempin->frame_pointer = 0;
    mempin->pAllocator = 0;
    mempin->parent = this;
}

COutputPin::~COutputPin()
{
    delete vt;
    delete mempin->vt;
}
