/*

    Definition of important DirectShow interfaces.
    Created using freely-available DirectX 8.0 SDK
    ( http://msdn.microsoft.com )

*/
#ifndef INTERFACES_H
#define INTERFACES_H

#include "guids.h"
#include "default.h"
#include <com.h>
#ifndef STDCALL
#define STDCALL __attribute__((__stdcall__))	
#endif

typedef GUID& REFIID;
typedef GUID CLSID;
typedef GUID IID;

/*    Sh*t. MSVC++ and g++ use different methods of storing vtables.    */

struct IBaseFilter;
struct IReferenceClock;
struct IEnumPins;
struct IEnumMediaTypes;
struct IPin;
struct IFilterGraph;
struct IMemInputPin;
struct IMemAllocator;
struct IMediaSample;

enum PIN_DIRECTION;

class IClassFactory2
{
public:
    virtual long STDCALL QueryInterface(GUID* iid, void** ppv) =0;
    virtual long STDCALL AddRef() =0;
    virtual long STDCALL Release() =0;
    virtual long STDCALL CreateInstance(IUnknown* pUnkOuter, GUID* riid, void** ppvObject) =0;
};

struct IBaseFilter_vt: IUnknown_vt
{
    HRESULT STDCALL ( *GetClassID )( 
        IBaseFilter * This,
        /* [out] */ CLSID *pClassID);
    
    HRESULT STDCALL ( *Stop )( 
        IBaseFilter * This);
    
    HRESULT STDCALL ( *Pause )( 
        IBaseFilter * This);
    
    HRESULT STDCALL ( *Run )( 
        IBaseFilter * This,
        REFERENCE_TIME tStart);
    
    HRESULT STDCALL ( *GetState )( 
        IBaseFilter * This,
        /* [in] */ unsigned long dwMilliSecsTimeout,
//        /* [out] */ FILTER_STATE *State);
    	void* State);
    
    HRESULT STDCALL ( *SetSyncSource )( 
        IBaseFilter * This,
        /* [in] */ IReferenceClock *pClock);
    
    HRESULT STDCALL ( *GetSyncSource )( 
        IBaseFilter * This,
        /* [out] */ IReferenceClock **pClock);
    
    HRESULT STDCALL ( *EnumPins )( 
        IBaseFilter * This,
        /* [out] */ IEnumPins **ppEnum);
    
    HRESULT STDCALL ( *FindPin )( 
        IBaseFilter * This,
        /* [string][in] */ const unsigned short* Id,
        /* [out] */ IPin **ppPin);
    
    HRESULT STDCALL ( *QueryFilterInfo )( 
        IBaseFilter * This,
//        /* [out] */ FILTER_INFO *pInfo);
	void* pInfo);
    
    HRESULT STDCALL ( *JoinFilterGraph )( 
        IBaseFilter * This,
        /* [in] */ IFilterGraph *pGraph,
        /* [string][in] */ const unsigned short* pName);
    
    HRESULT STDCALL ( *QueryVendorInfo )( 
        IBaseFilter * This,
        /* [string][out] */ unsigned short* *pVendorInfo);
    
};

struct IBaseFilter
{
    struct IBaseFilter_vt *vt;
};

struct IEnumPins_vt: IUnknown_vt
{
    HRESULT STDCALL ( *Next )( 
        IEnumPins * This,
        /* [in] */ unsigned long cPins,
        /* [size_is][out] */ IPin **ppPins,
        /* [out] */ unsigned long *pcFetched);
    
    HRESULT STDCALL ( *Skip )( 
        IEnumPins * This,
        /* [in] */ unsigned long cPins);
    
    HRESULT STDCALL ( *Reset )( 
        IEnumPins * This);
    
    HRESULT STDCALL ( *Clone )( 
        IEnumPins * This,
        /* [out] */ IEnumPins **ppEnum);
        
};

struct IEnumPins
{
     struct IEnumPins_vt *vt;
};


struct IPin_vt: IUnknown_vt
{
        HRESULT STDCALL ( *Connect )( 
            IPin * This,
            /* [in] */ IPin *pReceivePin,
            /* [in] */ /*const*/ AM_MEDIA_TYPE *pmt);
        
        HRESULT STDCALL ( *ReceiveConnection )( 
            IPin * This,
            /* [in] */ IPin *pConnector,
            /* [in] */ const AM_MEDIA_TYPE *pmt);
        
        HRESULT STDCALL ( *Disconnect )( 
            IPin * This);
        
        HRESULT STDCALL ( *ConnectedTo )( 
            IPin * This,
            /* [out] */ IPin **pPin);
        
        HRESULT STDCALL ( *ConnectionMediaType )( 
            IPin * This,
            /* [out] */ AM_MEDIA_TYPE *pmt);
        
        HRESULT STDCALL ( *QueryPinInfo )( 
            IPin * This,
            /* [out] */ PIN_INFO *pInfo);
        
        HRESULT STDCALL ( *QueryDirection )( 
            IPin * This,
            /* [out] */ PIN_DIRECTION *pPinDir);
        
        HRESULT STDCALL ( *QueryId )( 
            IPin * This,
            /* [out] */ unsigned short* *Id);
        
        HRESULT STDCALL ( *QueryAccept )( 
            IPin * This,
            /* [in] */ const AM_MEDIA_TYPE *pmt);
        
        HRESULT STDCALL ( *EnumMediaTypes )( 
            IPin * This,
            /* [out] */ IEnumMediaTypes **ppEnum);
        
        HRESULT STDCALL ( *QueryInternalConnections )( 
            IPin * This,
            /* [out] */ IPin **apPin,
            /* [out][in] */ unsigned long *nPin);
        
        HRESULT STDCALL ( *EndOfStream )( 
            IPin * This);
        
        HRESULT STDCALL ( *BeginFlush )( 
            IPin * This);
        
        HRESULT STDCALL ( *EndFlush )( 
            IPin * This);
        
        HRESULT STDCALL ( *NewSegment )( 
            IPin * This,
            /* [in] */ REFERENCE_TIME tStart,
            /* [in] */ REFERENCE_TIME tStop,
            /* [in] */ double dRate);
        
};

struct IPin
{
    IPin_vt *vt;
};

struct IEnumMediaTypes_vt: IUnknown_vt
{
        HRESULT STDCALL ( *Next )( 
            IEnumMediaTypes * This,
            /* [in] */ unsigned long cMediaTypes,
            /* [size_is][out] */ AM_MEDIA_TYPE **ppMediaTypes,
            /* [out] */ unsigned long *pcFetched);
        
        HRESULT STDCALL ( *Skip )( 
            IEnumMediaTypes * This,
            /* [in] */ unsigned long cMediaTypes);
        
        HRESULT STDCALL ( *Reset )( 
            IEnumMediaTypes * This);
        
        HRESULT STDCALL ( *Clone )( 
            IEnumMediaTypes * This,
            /* [out] */ IEnumMediaTypes **ppEnum);
};

struct IEnumMediaTypes
{
    IEnumMediaTypes_vt *vt;
};

    
struct IMemInputPin_vt: IUnknown_vt
{
    HRESULT STDCALL ( *GetAllocator )( 
        IMemInputPin * This,
        /* [out] */ IMemAllocator **ppAllocator);
    
    HRESULT STDCALL ( *NotifyAllocator )( 
        IMemInputPin * This,
        /* [in] */ IMemAllocator *pAllocator,
        /* [in] */ int bReadOnly);
    
    HRESULT STDCALL ( *GetAllocatorRequirements )( 
            IMemInputPin * This,
        /* [out] */ ALLOCATOR_PROPERTIES *pProps);
    
    HRESULT STDCALL ( *Receive )( 
        IMemInputPin * This,
        /* [in] */ IMediaSample *pSample);
    
    HRESULT STDCALL ( *ReceiveMultiple )( 
        IMemInputPin * This,
        /* [size_is][in] */ IMediaSample **pSamples,
        /* [in] */ long nSamples,
        /* [out] */ long *nSamplesProcessed);
    
    HRESULT STDCALL ( *ReceiveCanBlock )( 
        IMemInputPin * This);
};

struct IMemInputPin
{
    IMemInputPin_vt *vt;
};

    

struct IMemAllocator_vt: IUnknown_vt
{
    HRESULT STDCALL ( *SetProperties )( 
        IMemAllocator * This,
        /* [in] */ ALLOCATOR_PROPERTIES *pRequest,
        /* [out] */ ALLOCATOR_PROPERTIES *pActual);
    
    HRESULT STDCALL ( *GetProperties )( 
        IMemAllocator * This,
        /* [out] */ ALLOCATOR_PROPERTIES *pProps);
    
    HRESULT STDCALL ( *Commit )( 
        IMemAllocator * This);
    
    HRESULT STDCALL ( *Decommit )( 
        IMemAllocator * This);
    
    HRESULT STDCALL ( *GetBuffer )( 
        IMemAllocator * This,
        /* [out] */ IMediaSample **ppBuffer,
        /* [in] */ REFERENCE_TIME *pStartTime,
        /* [in] */ REFERENCE_TIME *pEndTime,
        /* [in] */ unsigned long dwFlags);
    
        HRESULT STDCALL ( *ReleaseBuffer )( 
        IMemAllocator * This,
        /* [in] */ IMediaSample *pBuffer);
};

struct IMemAllocator
{
    IMemAllocator_vt *vt;
};

struct IMediaSample_vt: IUnknown_vt
{
    HRESULT STDCALL ( *GetPointer )( 
        IMediaSample * This,
        /* [out] */ unsigned char **ppBuffer);
    
    long STDCALL ( *GetSize )( 
        IMediaSample * This);
    
    HRESULT STDCALL ( *GetTime )( 
        IMediaSample * This,
        /* [out] */ REFERENCE_TIME *pTimeStart,
        /* [out] */ REFERENCE_TIME *pTimeEnd);
    
    HRESULT STDCALL ( *SetTime )( 
        IMediaSample * This,
        /* [in] */ REFERENCE_TIME *pTimeStart,
        /* [in] */ REFERENCE_TIME *pTimeEnd);
    
    HRESULT STDCALL ( *IsSyncPoint )( 
        IMediaSample * This);
    
    HRESULT STDCALL ( *SetSyncPoint )( 
        IMediaSample * This,
        long bIsSyncPoint);
    
    HRESULT STDCALL ( *IsPreroll )( 
        IMediaSample * This);
    
    HRESULT STDCALL ( *SetPreroll )( 
        IMediaSample * This,
        long bIsPreroll);
    
    long STDCALL ( *GetActualDataLength )( 
        IMediaSample * This);
    
    HRESULT STDCALL ( *SetActualDataLength )( 
        IMediaSample * This,
        long __MIDL_0010);
    
    HRESULT STDCALL ( *GetMediaType )( 
        IMediaSample * This,
        AM_MEDIA_TYPE **ppMediaType);
    
    HRESULT STDCALL ( *SetMediaType )( 
        IMediaSample * This,
        AM_MEDIA_TYPE *pMediaType);
    
    HRESULT STDCALL ( *IsDiscontinuity )( 
        IMediaSample * This);
    
    HRESULT STDCALL ( *SetDiscontinuity )( 
        IMediaSample * This,
        long bDiscontinuity);
    
    HRESULT STDCALL ( *GetMediaTime )( 
        IMediaSample * This,
        /* [out] */ long long *pTimeStart,
        /* [out] */ long long *pTimeEnd);
    
    HRESULT STDCALL ( *SetMediaTime )( 
        IMediaSample * This,
        /* [in] */ long long *pTimeStart,
        /* [in] */ long long *pTimeEnd);
};

struct IMediaSample
{
    struct IMediaSample_vt *vt;
};

struct IHidden;    
struct IHidden_vt: IUnknown_vt
{
    HRESULT STDCALL ( *GetSmth )(IHidden * This,
 int* pv);
    HRESULT STDCALL ( *SetSmth )(IHidden * This,
 int v1, int v2);
    HRESULT STDCALL ( *GetSmth2 )(IHidden * This,
 int* pv);
    HRESULT STDCALL ( *SetSmth2 )(IHidden * This,
 int v1, int v2);
    HRESULT STDCALL ( *GetSmth3 )(IHidden * This,
 int* pv);
    HRESULT STDCALL ( *SetSmth3 )(IHidden * This,
 int v1, int v2);
    HRESULT STDCALL ( *GetSmth4 )(IHidden * This,
 int* pv);
    HRESULT STDCALL ( *SetSmth4 )(IHidden * This,
 int v1, int v2);
    HRESULT STDCALL ( *GetSmth5 )(IHidden * This,
 int* pv);
    HRESULT STDCALL ( *SetSmth5 )(IHidden * This,
 int v1, int v2);
    HRESULT STDCALL ( *GetSmth6 )(IHidden * This,
 int* pv);
};
    
struct IHidden
{
    struct IHidden_vt *vt;
};

struct IHidden2;
struct IHidden2_vt: IUnknown_vt
{
    HRESULT STDCALL (*unk1) ();
    HRESULT STDCALL (*unk2) ();
    HRESULT STDCALL (*unk3) ();
    HRESULT STDCALL (*DecodeGet) (IHidden2* This, int* region);
    HRESULT STDCALL (*unk5) ();
    HRESULT STDCALL (*DecodeSet) (IHidden2* This, int* region);
    HRESULT STDCALL (*unk7) ();
    HRESULT STDCALL (*unk8) ();
};
struct IHidden2
{
    struct IHidden2_vt *vt;
};
#endif