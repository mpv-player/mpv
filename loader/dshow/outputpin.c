#include "outputpin.h"
#include <string.h>
#include <stdio.h>
#include "allocator.h"
#include "iunk.h"
#define E_NOTIMPL 0x80004001
/*
    An object beyond interface IEnumMediaTypes.
    Returned by COutputPin through call IPin::EnumMediaTypes().
*/

class CEnumMediaTypes: public IEnumMediaTypes
{
    AM_MEDIA_TYPE type;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(CEnumMediaTypes)
public:
    CEnumMediaTypes(const AM_MEDIA_TYPE&);
    ~CEnumMediaTypes(){delete vt;}
    static HRESULT STDCALL Next ( 
        IEnumMediaTypes * This,
        /* [in] */ ULONG cMediaTypes,
        /* [size_is][out] */ AM_MEDIA_TYPE **ppMediaTypes,
        /* [out] */ ULONG *pcFetched);
    
    static HRESULT STDCALL Skip ( 
        IEnumMediaTypes * This,
        /* [in] */ ULONG cMediaTypes);
    
    static HRESULT STDCALL Reset ( 
        IEnumMediaTypes * This);
    
    static HRESULT STDCALL Clone ( 
        IEnumMediaTypes * This,
        /* [out] */ IEnumMediaTypes **ppEnum);

};
GUID CEnumMediaTypes::interfaces[]=
{
    IID_IUnknown,
    IID_IEnumMediaTypes,
};
IMPLEMENT_IUNKNOWN(CEnumMediaTypes)
CEnumMediaTypes::CEnumMediaTypes(const AM_MEDIA_TYPE& type)
    :refcount(1)
{
    this->type=type;
    vt=new IEnumMediaTypes_vt;
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->Next = Next; 
    vt->Skip = Skip; 
    vt->Reset = Reset;
    vt->Clone = Clone; 
}

HRESULT STDCALL CEnumMediaTypes::Next ( 
    IEnumMediaTypes * This,
    /* [in] */ ULONG cMediaTypes,
    /* [size_is][out] */ AM_MEDIA_TYPE **ppMediaTypes,
    /* [out] */ ULONG *pcFetched)
{
    AM_MEDIA_TYPE& type=((CEnumMediaTypes*)This)->type;
    Debug printf("CEnumMediaTypes::Next() called\n");
    if(!ppMediaTypes)return 0x80004003;
    if(!pcFetched && (cMediaTypes!=1))return 0x80004003;
    if(cMediaTypes<=0)return 0;
    
    if(pcFetched)*pcFetched=1;
    ppMediaTypes[0]=(AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    memcpy(*ppMediaTypes, &type, sizeof(AM_MEDIA_TYPE));
    if(ppMediaTypes[0]->pbFormat)
    {
	ppMediaTypes[0]->pbFormat=(char *)CoTaskMemAlloc(ppMediaTypes[0]->cbFormat);
	memcpy(ppMediaTypes[0]->pbFormat, type.pbFormat, ppMediaTypes[0]->cbFormat);
    }
    if(cMediaTypes==1)return 0;
    return 1;
}
/*
    I expect that these methods are unused.
*/
HRESULT STDCALL CEnumMediaTypes::Skip ( 
    IEnumMediaTypes * This,
    /* [in] */ ULONG cMediaTypes)
{
    Debug printf("CEnumMediaTypes::Skip() called\n");
    return E_NOTIMPL;
}

HRESULT STDCALL CEnumMediaTypes::Reset ( 
    IEnumMediaTypes * This)
{
    Debug printf("CEnumMediaTypes::Reset() called\n");
    return 0;
}

HRESULT STDCALL CEnumMediaTypes::Clone ( 
    IEnumMediaTypes * This,
    /* [out] */ IEnumMediaTypes **ppEnum)
{
    Debug printf("CEnumMediaTypes::Clone() called\n");
    return E_NOTIMPL;
}

/*
    Implementation of output pin object.
*/

// Constructor

COutputPin::COutputPin(const AM_MEDIA_TYPE& vh) :refcount(1), type(vh), remote(0), frame_pointer(0), frame_size_pointer(0)
{
    IPin::vt=new IPin_vt;
    IPin::vt->QueryInterface = QueryInterface;
    IPin::vt->AddRef = AddRef;
    IPin::vt->Release = Release;
    IPin::vt->Connect = Connect;
    IPin::vt->ReceiveConnection = ReceiveConnection;
    IPin::vt->Disconnect=Disconnect;
    IPin::vt->ConnectedTo = ConnectedTo;
    IPin::vt->ConnectionMediaType = ConnectionMediaType;
    IPin::vt->QueryPinInfo = QueryPinInfo;
    IPin::vt->QueryDirection = QueryDirection;
    IPin::vt->QueryId = QueryId;
    IPin::vt->QueryAccept = QueryAccept;
    IPin::vt->EnumMediaTypes = EnumMediaTypes;
    IPin::vt->QueryInternalConnections = QueryInternalConnections;
    IPin::vt->EndOfStream = EndOfStream;
    IPin::vt->BeginFlush = BeginFlush;
    IPin::vt->EndFlush = EndFlush;
    IPin::vt->NewSegment = NewSegment;

    IMemInputPin::vt=new IMemInputPin_vt;
    IMemInputPin::vt->QueryInterface = M_QueryInterface;
    IMemInputPin::vt->AddRef = M_AddRef;
    IMemInputPin::vt->Release = M_Release;
    IMemInputPin::vt->GetAllocator = GetAllocator;
    IMemInputPin::vt->NotifyAllocator = NotifyAllocator;
    IMemInputPin::vt->GetAllocatorRequirements = GetAllocatorRequirements;
    IMemInputPin::vt->Receive = Receive;
    IMemInputPin::vt->ReceiveMultiple = ReceiveMultiple;
    IMemInputPin::vt->ReceiveCanBlock = ReceiveCanBlock;
}

// IPin->IUnknown methods

HRESULT STDCALL COutputPin::QueryInterface(IUnknown* This, GUID* iid, void** ppv)
{
    Debug printf("COutputPin::QueryInterface() called\n");
    if(!ppv)return 0x80004003;
    if(!memcmp(iid, &IID_IUnknown, 16))
    {
	*ppv=(void*)This;
	This->vt->AddRef(This);
	return 0;
    }
    if(!memcmp(iid, &IID_IMemInputPin, 16))
    {
	*ppv=(void*)(This+1);
	This->vt->AddRef(This);
	return 0;
    }

    Debug printf("Unknown interface : %08x-%04x-%04x-%02x%02x-" \
			"%02x%02x%02x%02x%02x%02x\n",
	 iid->f1,  iid->f2,  iid->f3,
    	 (unsigned char)iid->f4[1], (unsigned char)iid->f4[0],
	(unsigned char)iid->f4[2],(unsigned char)iid->f4[3],(unsigned char)iid->f4[4],	  
	(unsigned char)iid->f4[5],(unsigned char)iid->f4[6],(unsigned char)iid->f4[7]);
    return 0x80004002;
}
HRESULT STDCALL COutputPin::AddRef(IUnknown* This)
{
    Debug printf("COutputPin::AddRef() called\n");
    ((COutputPin*)This)->refcount++;
    return 0;
}
HRESULT STDCALL COutputPin::Release(IUnknown* This)
{
    Debug printf("COutputPin::Release() called\n");
    if(--((COutputPin*)This)->refcount==0)
	delete (COutputPin*)This;
    return 0;
}

// IPin methods

HRESULT STDCALL COutputPin::Connect ( 
    IPin * This,
    /* [in] */ IPin *pReceivePin,
    /* [in] */ /* const */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("COutputPin::Connect() called\n");
/*
    *pmt=((COutputPin*)This)->type;
    if(pmt->cbFormat>0)
    {
	pmt->pbFormat=CoTaskMemAlloc(pmt->cbFormat);
	memcpy(pmt->pbFormat, ((COutputPin*)This)->type.pbFormat, pmt->cbFormat);
    }	
*/
    return E_NOTIMPL;
    // if I put return 0; here, it crashes
}

HRESULT STDCALL COutputPin::ReceiveConnection ( 
    IPin * This,
    /* [in] */ IPin *pConnector,
    /* [in] */ const AM_MEDIA_TYPE *pmt)
{
    Debug printf("COutputPin::ReceiveConnection() called\n");
    ((COutputPin*)This)->remote=pConnector;
    return 0;
}
	    
HRESULT STDCALL COutputPin::Disconnect ( 
    IPin * This)
{
    Debug printf("COutputPin::Disconnect() called\n");
    return 1;
}


HRESULT STDCALL COutputPin::ConnectedTo ( 
    IPin * This,
    /* [out] */ IPin **pPin)
{
    Debug printf("COutputPin::ConnectedTo() called\n");
    if(!pPin)return 0x80004003;
    *pPin=((COutputPin*)This)->remote;
    return 0;
}



HRESULT STDCALL COutputPin::ConnectionMediaType ( 
    IPin * This,
    /* [out] */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("CInputPin::ConnectionMediaType() called\n");
    if(!pmt)return 0x80004003;
    *pmt=((COutputPin*)This)->type;
    if(pmt->cbFormat>0)
    {
	pmt->pbFormat=(char *)CoTaskMemAlloc(pmt->cbFormat);
	memcpy(pmt->pbFormat, ((COutputPin*)This)->type.pbFormat, pmt->cbFormat);
    }	
    return 0;
}

HRESULT STDCALL COutputPin::QueryPinInfo ( 
    IPin * This,
    /* [out] */ PIN_INFO *pInfo)
{
    Debug printf("COutputPin::QueryPinInfo() called\n");
    return E_NOTIMPL;
}


HRESULT STDCALL COutputPin::QueryDirection ( 
    IPin * This,
    /* [out] */ PIN_DIRECTION *pPinDir)
{
    Debug printf("COutputPin::QueryDirection() called\n");
    if(!pPinDir)return -1;
    *pPinDir=PINDIR_INPUT;
    return 0;
}


HRESULT STDCALL COutputPin::QueryId ( 
    IPin * This,
    /* [out] */ LPWSTR *Id)
{
    Debug printf("COutputPin::QueryId() called\n");
    return E_NOTIMPL;
}


HRESULT STDCALL COutputPin::QueryAccept ( 
    IPin * This,
    /* [in] */ const AM_MEDIA_TYPE *pmt)
{
    Debug printf("COutputPin::QueryAccept() called\n");
    return E_NOTIMPL;
}


HRESULT STDCALL COutputPin::EnumMediaTypes ( 
    IPin * This,
    /* [out] */ IEnumMediaTypes **ppEnum)
{
    Debug printf("COutputPin::EnumMediaTypes() called\n");
    if(!ppEnum)return 0x80004003;
    *ppEnum=new CEnumMediaTypes(((COutputPin*)This)->type);
    return 0;
}


HRESULT STDCALL COutputPin::QueryInternalConnections ( 
    IPin * This,
    /* [out] */ IPin **apPin,
    /* [out][in] */ ULONG *nPin)
{
    Debug printf("COutputPin::QueryInternalConnections() called\n");
    return E_NOTIMPL;
}

HRESULT STDCALL COutputPin::EndOfStream ( 
    IPin * This)
{
    Debug printf("COutputPin::EndOfStream() called\n");
    return E_NOTIMPL;
}
    
    
HRESULT STDCALL COutputPin::BeginFlush ( 
IPin * This)
{
    Debug printf("COutputPin::BeginFlush() called\n");
    return E_NOTIMPL;
}


HRESULT STDCALL COutputPin::EndFlush ( 
    IPin * This)
{
    Debug printf("COutputPin::EndFlush() called\n");
    return E_NOTIMPL;
}

HRESULT STDCALL COutputPin::NewSegment ( 
    IPin * This,
    /* [in] */ REFERENCE_TIME tStart,
    /* [in] */ REFERENCE_TIME tStop,
    /* [in] */ double dRate)
{
    Debug printf("COutputPin::NewSegment(%ld,%ld,%f) called\n",tStart,tStop,dRate);
    return 0;
}


















// IMemInputPin->IUnknown methods

HRESULT STDCALL COutputPin::M_QueryInterface(IUnknown* This, GUID* iid, void** ppv)
{
    Debug printf("COutputPin::QueryInterface() called\n");
    if(!ppv)return 0x80004003;
    if(!memcmp(iid, &IID_IUnknown, 16))
    {
	COutputPin* ptr=(COutputPin*)(This-1);
	*ppv=(void*)ptr;
	AddRef((IUnknown*)ptr);
	return 0;
    }
/*    if(!memcmp(iid, &IID_IPin, 16))
    {
	COutputPin* ptr=(COutputPin*)(This-1);
	*ppv=(void*)ptr;
	AddRef((IUnknown*)ptr);
	return 0;
    }*/
    if(!memcmp(iid, &IID_IMemInputPin, 16))
    {
	*ppv=(void*)This;
	This->vt->AddRef(This);
	return 0;
    }
    Debug printf("Unknown interface : %08x-%04x-%04x-%02x%02x-" \
			"%02x%02x%02x%02x%02x%02x\n",
	 iid->f1,  iid->f2,  iid->f3,
    	 (unsigned char)iid->f4[1], (unsigned char)iid->f4[0],
	(unsigned char)iid->f4[2],(unsigned char)iid->f4[3],(unsigned char)iid->f4[4],	  
	(unsigned char)iid->f4[5],(unsigned char)iid->f4[6],(unsigned char)iid->f4[7]);
    return 0x80004002;
}
HRESULT STDCALL COutputPin::M_AddRef(IUnknown* This)
{
    Debug printf("COutputPin::AddRef() called\n");
    ((COutputPin*)(This-1))->refcount++;
    return 0;
}
HRESULT STDCALL COutputPin::M_Release(IUnknown* This)
{
    Debug printf("COutputPin::Release() called\n");
    if(--((COutputPin*)(This-1))->refcount==0)
	delete (COutputPin*)This;
    return 0;
}




// IMemInputPin methods

HRESULT STDCALL COutputPin::GetAllocator( 
        IMemInputPin * This,
    /* [out] */ IMemAllocator **ppAllocator) 
{
    Debug printf("COutputPin::GetAllocator(%x,%x) called\n",This->vt,ppAllocator);
    *ppAllocator=new MemAllocator;
    return 0;
}
    
HRESULT STDCALL COutputPin::NotifyAllocator( 
        IMemInputPin * This,
    /* [in] */ IMemAllocator *pAllocator,
    /* [in] */ int bReadOnly)
{
    Debug printf("COutputPin::NotifyAllocator() called\n");
    return 0;
}

HRESULT STDCALL COutputPin::GetAllocatorRequirements( 
        IMemInputPin * This,
    /* [out] */ ALLOCATOR_PROPERTIES *pProps) 
{
    Debug printf("COutputPin::GetAllocatorRequirements() called\n");
    return E_NOTIMPL;
}

HRESULT STDCALL COutputPin::Receive( 
        IMemInputPin * This,
    /* [in] */ IMediaSample *pSample) 
{
    Debug printf("COutputPin::Receive() called\n");
    COutputPin& me=*(COutputPin*)This;
    if(!pSample)return 0x80004003;
    char* pointer;
    if(pSample->vt->GetPointer(pSample, (BYTE **)&pointer))
	return -1;
    int len=pSample->vt->GetActualDataLength(pSample);
    if(len==0)len=pSample->vt->GetSize(pSample);//for iv50
    //if(me.frame_pointer)memcpy(me.frame_pointer, pointer, len);
    *me.frame_pointer=pointer;
    if(me.frame_size_pointer)*me.frame_size_pointer=len;
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

HRESULT STDCALL COutputPin::ReceiveMultiple( 
        IMemInputPin * This,
    /* [size_is][in] */ IMediaSample **pSamples,
    /* [in] */ long nSamples,
    /* [out] */ long *nSamplesProcessed)
{
    Debug printf("COutputPin::ReceiveMultiple() called\n");
    return E_NOTIMPL;
}

HRESULT STDCALL COutputPin::ReceiveCanBlock(
        IMemInputPin * This) 
{
    Debug printf("COutputPin::ReceiveCanBlock() called\n");
    return E_NOTIMPL;
}
