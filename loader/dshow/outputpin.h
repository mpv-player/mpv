/*	"output pin" - the one that connects to output of filter.     */

#ifndef OUTPUTPIN_H
#define OUTPUTPIN_H
#include "interfaces.h"
#include "guids.h"
#include "default.h"

class COutputPin: public IPin, public IMemInputPin
{
    int refcount;
    AM_MEDIA_TYPE type;
    IPin* remote;
    char** frame_pointer;
    long* frame_size_pointer;
public:
    COutputPin(const AM_MEDIA_TYPE& vhdr);
    ~COutputPin(){delete IPin::vt; delete IMemInputPin::vt;}
    void SetFramePointer(char** z){frame_pointer=z;}
    void SetFrameSizePointer(long* z){frame_size_pointer=z;}
    void SetNewFormat(const AM_MEDIA_TYPE& a){type=a;}
    static HRESULT STDCALL QueryInterface(IUnknown* This, GUID* iid, void** ppv);
    static HRESULT STDCALL AddRef(IUnknown* This);
    static HRESULT STDCALL Release(IUnknown* This);

    static HRESULT STDCALL M_QueryInterface(IUnknown* This, GUID* iid, void** ppv);
    static HRESULT STDCALL M_AddRef(IUnknown* This);
    static HRESULT STDCALL M_Release(IUnknown* This);

    static HRESULT STDCALL Connect ( 
	IPin * This,
        /* [in] */ IPin *pReceivePin,
        /* [in] */ /*const */AM_MEDIA_TYPE *pmt);
        
    static HRESULT STDCALL ReceiveConnection ( 
        IPin * This,
        /* [in] */ IPin *pConnector,
        /* [in] */ const AM_MEDIA_TYPE *pmt);
    
    static HRESULT STDCALL Disconnect ( 
        IPin * This);
    
    static HRESULT STDCALL ConnectedTo ( 
        IPin * This,
        /* [out] */ IPin **pPin);
    
    static HRESULT STDCALL ConnectionMediaType ( 
        IPin * This,
        /* [out] */ AM_MEDIA_TYPE *pmt);
    
    static HRESULT STDCALL QueryPinInfo ( 
        IPin * This,
        /* [out] */ PIN_INFO *pInfo);
    
    static HRESULT STDCALL QueryDirection ( 
        IPin * This,
        /* [out] */ PIN_DIRECTION *pPinDir);
    
    static HRESULT STDCALL QueryId ( 
        IPin * This,
        /* [out] */ LPWSTR *Id);
    
    static HRESULT STDCALL QueryAccept ( 
        IPin * This,
        /* [in] */ const AM_MEDIA_TYPE *pmt);
    
    static HRESULT STDCALL EnumMediaTypes ( 
        IPin * This,
        /* [out] */ IEnumMediaTypes **ppEnum);
    
    static HRESULT STDCALL QueryInternalConnections ( 
        IPin * This,
        /* [out] */ IPin **apPin,
        /* [out][in] */ ULONG *nPin);
    
    static HRESULT STDCALL EndOfStream ( 
        IPin * This);
    
    static HRESULT STDCALL BeginFlush ( 
        IPin * This);
    
    static HRESULT STDCALL EndFlush ( 
        IPin * This);
    
    static HRESULT STDCALL NewSegment ( 
        IPin * This,
        /* [in] */ REFERENCE_TIME tStart,
        /* [in] */ REFERENCE_TIME tStop,
        /* [in] */ double dRate);
	





    static HRESULT STDCALL GetAllocator( 
        IMemInputPin * This,
        /* [out] */ IMemAllocator **ppAllocator) ;
    
    static HRESULT STDCALL NotifyAllocator( 
        IMemInputPin * This,
        /* [in] */ IMemAllocator *pAllocator,
        /* [in] */ int bReadOnly) ;
    
    static HRESULT STDCALL GetAllocatorRequirements( 
        IMemInputPin * This,
        /* [out] */ ALLOCATOR_PROPERTIES *pProps) ;
    
    static HRESULT STDCALL Receive( 
        IMemInputPin * This,
        /* [in] */ IMediaSample *pSample) ;
    
    static HRESULT STDCALL ReceiveMultiple( 
        IMemInputPin * This,
        /* [size_is][in] */ IMediaSample **pSamples,
        /* [in] */ long nSamples,
        /* [out] */ long *nSamplesProcessed) ;
    
    static HRESULT STDCALL ReceiveCanBlock(
            IMemInputPin * This) ;
};
#endif
