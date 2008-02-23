#ifndef MPLAYER_INTERFACES_H
#define MPLAYER_INTERFACES_H

/*
 * Definition of important DirectShow interfaces.
 * Created using freely-available DirectX 8.0 SDK
 * ( http://msdn.microsoft.com )
 */

#include "iunk.h"
#include "com.h"

/*    Sh*t. MSVC++ and g++ use different methods of storing vtables.    */

typedef struct IReferenceClock IReferenceClock;
typedef struct IFilterGraph IFilterGraph;

typedef struct IBaseFilter IBaseFilter;

typedef enum
{
    PINDIR_INPUT = 0,
    PINDIR_OUTPUT
} PIN_DIRECTION;

typedef struct PinInfo
{
    IBaseFilter* pFilter;
    PIN_DIRECTION dir;
    unsigned short achName[128];
} PIN_INFO;

typedef struct AllocatorProperties
{
    long cBuffers;
    long cbBuffer;
    long cbAlign;
    long cbPrefix;
} ALLOCATOR_PROPERTIES;

typedef struct IEnumMediaTypes IEnumMediaTypes;
typedef struct IEnumMediaTypes_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *Next )(IEnumMediaTypes* This,
			      /* [in] */ unsigned long cMediaTypes,
			      /* [size_is][out] */ AM_MEDIA_TYPE** ppMediaTypes,
			      /* [out] */ unsigned long* pcFetched);
    HRESULT STDCALL ( *Skip )(IEnumMediaTypes* This,
			      /* [in] */ unsigned long cMediaTypes);
    HRESULT STDCALL ( *Reset )(IEnumMediaTypes* This);
    HRESULT STDCALL ( *Clone )(IEnumMediaTypes* This,
			       /* [out] */ IEnumMediaTypes** ppEnum);
} IEnumMediaTypes_vt;
struct IEnumMediaTypes { IEnumMediaTypes_vt* vt; };



typedef struct IPin IPin;
typedef struct IPin_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *Connect )(IPin * This,
				 /* [in] */ IPin *pReceivePin,
				 /* [in] */ /*const*/ AM_MEDIA_TYPE *pmt);
    HRESULT STDCALL ( *ReceiveConnection )(IPin * This,
					   /* [in] */ IPin *pConnector,
					   /* [in] */ const AM_MEDIA_TYPE *pmt);
    HRESULT STDCALL ( *Disconnect )(IPin * This);
    HRESULT STDCALL ( *ConnectedTo )(IPin * This, /* [out] */ IPin **pPin);
    HRESULT STDCALL ( *ConnectionMediaType )(IPin * This,
					     /* [out] */ AM_MEDIA_TYPE *pmt);
    HRESULT STDCALL ( *QueryPinInfo )(IPin * This, /* [out] */ PIN_INFO *pInfo);
    HRESULT STDCALL ( *QueryDirection )(IPin * This,
					/* [out] */ PIN_DIRECTION *pPinDir);
    HRESULT STDCALL ( *QueryId )(IPin * This, /* [out] */ unsigned short* *Id);
    HRESULT STDCALL ( *QueryAccept )(IPin * This,
				     /* [in] */ const AM_MEDIA_TYPE *pmt);
    HRESULT STDCALL ( *EnumMediaTypes )(IPin * This,
					/* [out] */ IEnumMediaTypes **ppEnum);
    HRESULT STDCALL ( *QueryInternalConnections )(IPin * This,
						  /* [out] */ IPin **apPin,
						  /* [out][in] */ unsigned long *nPin);
    HRESULT STDCALL ( *EndOfStream )(IPin * This);
    HRESULT STDCALL ( *BeginFlush )(IPin * This);
    HRESULT STDCALL ( *EndFlush )(IPin * This);
    HRESULT STDCALL ( *NewSegment )(IPin * This,
				    /* [in] */ REFERENCE_TIME tStart,
				    /* [in] */ REFERENCE_TIME tStop,
				    /* [in] */ double dRate);
} IPin_vt;
struct IPin { IPin_vt *vt; };


typedef struct IEnumPins IEnumPins;
typedef struct IEnumPins_vt
{
    INHERIT_IUNKNOWN();

    // retrieves a specified number of pins in the enumeration sequence..
    HRESULT STDCALL ( *Next )(IEnumPins* This,
			      /* [in] */ unsigned long cPins,
			      /* [size_is][out] */ IPin** ppPins,
			      /* [out] */ unsigned long* pcFetched);
    // skips over a specified number of pins.
    HRESULT STDCALL ( *Skip )(IEnumPins* This,
			      /* [in] */ unsigned long cPins);
    // resets the enumeration sequence to the beginning.
    HRESULT STDCALL ( *Reset )(IEnumPins* This);
    // makes a copy of the enumerator with the same enumeration state.
    HRESULT STDCALL ( *Clone )(IEnumPins* This,
			       /* [out] */ IEnumPins** ppEnum);
} IEnumPins_vt;
struct IEnumPins { struct IEnumPins_vt* vt; };


typedef struct IMediaSample IMediaSample;
typedef struct IMediaSample_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *GetPointer )(IMediaSample* This,
				    /* [out] */ unsigned char** ppBuffer);
    LONG    STDCALL ( *GetSize )(IMediaSample* This);
    HRESULT STDCALL ( *GetTime )(IMediaSample* This,
				 /* [out] */ REFERENCE_TIME* pTimeStart,
				 /* [out] */ REFERENCE_TIME* pTimeEnd);
    HRESULT STDCALL ( *SetTime )(IMediaSample* This,
				 /* [in] */ REFERENCE_TIME* pTimeStart,
				 /* [in] */ REFERENCE_TIME* pTimeEnd);

    // sync-point property. If true, then the beginning of this
    // sample is a sync-point. (note that if AM_MEDIA_TYPE.bTemporalCompression
    // is false then all samples are sync points). A filter can start
    // a stream at any sync point.  S_FALSE if not sync-point, S_OK if true.
    HRESULT STDCALL ( *IsSyncPoint )(IMediaSample* This);
    HRESULT STDCALL ( *SetSyncPoint )(IMediaSample* This,
				      long bIsSyncPoint);

    // preroll property.  If true, this sample is for preroll only and
    // shouldn't be displayed.
    HRESULT STDCALL ( *IsPreroll )(IMediaSample* This);
    HRESULT STDCALL ( *SetPreroll )(IMediaSample* This,
				    long bIsPreroll);

    LONG    STDCALL ( *GetActualDataLength )(IMediaSample* This);
    HRESULT STDCALL ( *SetActualDataLength )(IMediaSample* This,
					     long __MIDL_0010);

    // these allow for limited format changes in band - if no format change
    // has been made when you receive a sample GetMediaType will return S_FALSE
    HRESULT STDCALL ( *GetMediaType )(IMediaSample* This,
				      AM_MEDIA_TYPE** ppMediaType);
    HRESULT STDCALL ( *SetMediaType )(IMediaSample* This,
				      AM_MEDIA_TYPE* pMediaType);

    // returns S_OK if there is a discontinuity in the data (this frame is
    // not a continuation of the previous stream of data
    // - there has been a seek or some dropped samples).
    HRESULT STDCALL ( *IsDiscontinuity )(IMediaSample* This);
    HRESULT STDCALL ( *SetDiscontinuity )(IMediaSample* This,
					  long bDiscontinuity);

    // get the media times for this sample
    HRESULT STDCALL ( *GetMediaTime )(IMediaSample* This,
				      /* [out] */ long long* pTimeStart,
				      /* [out] */ long long* pTimeEnd);
    // Set the media times for this sample
    // pTimeStart==pTimeEnd==NULL will invalidate the media time stamps in
    // this sample
    HRESULT STDCALL ( *SetMediaTime )(IMediaSample* This,
				      /* [in] */ long long* pTimeStart,
				      /* [in] */ long long* pTimeEnd);
} IMediaSample_vt;
struct IMediaSample { struct IMediaSample_vt* vt; };



//typedef struct IBaseFilter IBaseFilter;
typedef struct IBaseFilter_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *GetClassID )(IBaseFilter * This,
				    /* [out] */ CLSID *pClassID);
    HRESULT STDCALL ( *Stop )(IBaseFilter * This);
    HRESULT STDCALL ( *Pause )(IBaseFilter * This);
    HRESULT STDCALL ( *Run )(IBaseFilter * This,
			     REFERENCE_TIME tStart);
    HRESULT STDCALL ( *GetState )(IBaseFilter * This,
				  /* [in] */ unsigned long dwMilliSecsTimeout,
				  ///* [out] */ FILTER_STATE *State);
				  void* State);
    HRESULT STDCALL ( *SetSyncSource )(IBaseFilter* This,
				       /* [in] */ IReferenceClock *pClock);
    HRESULT STDCALL ( *GetSyncSource )(IBaseFilter* This,
				       /* [out] */ IReferenceClock **pClock);
    HRESULT STDCALL ( *EnumPins )(IBaseFilter* This,
				  /* [out] */ IEnumPins **ppEnum);
    HRESULT STDCALL ( *FindPin )(IBaseFilter* This,
				 /* [string][in] */ const unsigned short* Id,
				 /* [out] */ IPin** ppPin);
    HRESULT STDCALL ( *QueryFilterInfo )(IBaseFilter* This,
					 // /* [out] */ FILTER_INFO *pInfo);
					 void* pInfo);
    HRESULT STDCALL ( *JoinFilterGraph )(IBaseFilter* This,
					 /* [in] */ IFilterGraph* pGraph,
					 /* [string][in] */ const unsigned short* pName);
    HRESULT STDCALL ( *QueryVendorInfo )(IBaseFilter* This,
					 /* [string][out] */ unsigned short** pVendorInfo);
} IBaseFilter_vt;
struct IBaseFilter { struct IBaseFilter_vt* vt; };



typedef struct IMemAllocator IMemAllocator;
typedef struct IMemAllocator_vt
{
    INHERIT_IUNKNOWN();

    // specifies the number of buffers to allocate and the size of each buffer.
    HRESULT STDCALL ( *SetProperties )(IMemAllocator* This,
				       /* [in] */ ALLOCATOR_PROPERTIES *pRequest,
				       /* [out] */ ALLOCATOR_PROPERTIES *pActual);
    // retrieves the number of buffers that the allocator will create, and the buffer properties.
    HRESULT STDCALL ( *GetProperties )(IMemAllocator* This,
				       /* [out] */ ALLOCATOR_PROPERTIES *pProps);
    // allocates the buffer memory.
    HRESULT STDCALL ( *Commit )(IMemAllocator* This);
    // releases the memory for the buffers.
    HRESULT STDCALL ( *Decommit )(IMemAllocator* This);
    // retrieves a media sample that contains an empty buffer.
    HRESULT STDCALL ( *GetBuffer )(IMemAllocator* This,
				   /* [out] */ IMediaSample** ppBuffer,
				   /* [in] */ REFERENCE_TIME* pStartTime,
				   /* [in] */ REFERENCE_TIME* pEndTime,
				   /* [in] */ unsigned long dwFlags);
    // releases a media sample.
    HRESULT STDCALL ( *ReleaseBuffer )(IMemAllocator* This,
				       /* [in] */ IMediaSample* pBuffer);
} IMemAllocator_vt;
struct IMemAllocator { IMemAllocator_vt* vt; };



typedef struct IMemInputPin IMemInputPin;
typedef struct IMemInputPin_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *GetAllocator )(IMemInputPin * This,
				      /* [out] */ IMemAllocator **ppAllocator);
    HRESULT STDCALL ( *NotifyAllocator )(IMemInputPin * This,
					 /* [in] */ IMemAllocator *pAllocator,
					 /* [in] */ int bReadOnly);
    HRESULT STDCALL ( *GetAllocatorRequirements )(IMemInputPin * This,
						  /* [out] */ ALLOCATOR_PROPERTIES *pProps);
    HRESULT STDCALL ( *Receive )(IMemInputPin * This,
				 /* [in] */ IMediaSample *pSample);
    HRESULT STDCALL ( *ReceiveMultiple )(IMemInputPin * This,
					 /* [size_is][in] */ IMediaSample **pSamples,
					 /* [in] */ long nSamples,
					 /* [out] */ long *nSamplesProcessed);
    HRESULT STDCALL ( *ReceiveCanBlock )(IMemInputPin * This);
} IMemInputPin_vt;
struct IMemInputPin { IMemInputPin_vt* vt; };


typedef struct IHidden IHidden;
typedef struct IHidden_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *GetSmth )(IHidden* This, int* pv);
    HRESULT STDCALL ( *SetSmth )(IHidden* This, int v1, int v2);
    HRESULT STDCALL ( *GetSmth2 )(IHidden* This, int* pv);
    HRESULT STDCALL ( *SetSmth2 )(IHidden* This, int v1, int v2);
    HRESULT STDCALL ( *GetSmth3 )(IHidden* This, int* pv);
    HRESULT STDCALL ( *SetSmth3 )(IHidden* This, int v1, int v2);
    HRESULT STDCALL ( *GetSmth4 )(IHidden* This, int* pv);
    HRESULT STDCALL ( *SetSmth4 )(IHidden* This, int v1, int v2);
    HRESULT STDCALL ( *GetSmth5 )(IHidden* This, int* pv);
    HRESULT STDCALL ( *SetSmth5 )(IHidden* This, int v1, int v2);
    HRESULT STDCALL ( *GetSmth6 )(IHidden* This, int* pv);
} IHidden_vt;
struct IHidden { struct IHidden_vt* vt; };


typedef struct IHidden2 IHidden2;
typedef struct IHidden2_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *unk1 )(void);
    HRESULT STDCALL ( *unk2 )(void);
    HRESULT STDCALL ( *unk3 )(void);
    HRESULT STDCALL ( *DecodeGet )(IHidden2* This, int* region);
    HRESULT STDCALL ( *unk5 )(void);
    HRESULT STDCALL ( *DecodeSet )(IHidden2* This, int* region);
    HRESULT STDCALL ( *unk7 )(void);
    HRESULT STDCALL ( *unk8 )(void);
} IHidden2_vt;
struct IHidden2 { struct IHidden2_vt* vt; };


// fixme
typedef struct IDivxFilterInterface {
    struct IDivxFilterInterface_vt* vt;
} IDivxFilterInterface;

struct IDivxFilterInterface_vt
{
    INHERIT_IUNKNOWN();

    HRESULT STDCALL ( *get_PPLevel )(IDivxFilterInterface* This, int* PPLevel); // current postprocessing level
    HRESULT STDCALL ( *put_PPLevel )(IDivxFilterInterface* This, int PPLevel); // new postprocessing level
    HRESULT STDCALL ( *put_DefaultPPLevel )(IDivxFilterInterface* This);
    HRESULT STDCALL ( *put_MaxDelayAllowed )(IDivxFilterInterface* This, int maxdelayallowed);
    HRESULT STDCALL ( *put_Brightness )(IDivxFilterInterface* This,  int brightness);
    HRESULT STDCALL ( *put_Contrast )(IDivxFilterInterface* This,  int contrast);
    HRESULT STDCALL ( *put_Saturation )(IDivxFilterInterface* This, int saturation);
    HRESULT STDCALL ( *get_MaxDelayAllowed )(IDivxFilterInterface* This,  int* maxdelayallowed);
    HRESULT STDCALL ( *get_Brightness)(IDivxFilterInterface* This, int* brightness);
    HRESULT STDCALL ( *get_Contrast)(IDivxFilterInterface* This, int* contrast);
    HRESULT STDCALL ( *get_Saturation )(IDivxFilterInterface* This, int* saturation);
    HRESULT STDCALL ( *put_AspectRatio )(IDivxFilterInterface* This, int x, IDivxFilterInterface* Thisit, int y);
    HRESULT STDCALL ( *get_AspectRatio )(IDivxFilterInterface* This, int* x, IDivxFilterInterface* Thisit, int* y);
};

#endif  /*MPLAYER_INTERFACES_H */
