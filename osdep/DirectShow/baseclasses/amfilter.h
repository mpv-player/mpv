//------------------------------------------------------------------------------
// File: AMFilter.h
//
// Desc: DirectShow base classes - efines class hierarchy for streams
//       architecture.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __FILTER__
#define __FILTER__

/* The following classes are declared in this header: */

class CBaseMediaFilter; // IMediaFilter support
class CBaseFilter;      // IBaseFilter,IMediaFilter support
class CBasePin;         // Abstract base class for IPin interface
class CEnumPins;        // Enumerate input and output pins
class CEnumMediaTypes;  // Enumerate the pin's preferred formats
class CBaseOutputPin;   // Adds data provider member functions
class CBaseInputPin;    // Implements IMemInputPin interface
class CMediaSample;     // Basic transport unit for IMemInputPin
class CBaseAllocator;   // General list guff for most allocators
class CMemAllocator;    // Implements memory buffer allocation

//=====================================================================
//=====================================================================
//
// QueryFilterInfo and QueryPinInfo AddRef the interface pointers
// they return.  You can use the macro below to release the interface.
//
//=====================================================================
//=====================================================================

#define QueryFilterInfoReleaseGraph(fi) \
    if ((fi).pGraph)                    \
        (fi).pGraph->Release();

#define QueryPinInfoReleaseFilter(pi) \
    if ((pi).pFilter)                 \
        (pi).pFilter->Release();

//=====================================================================
//=====================================================================
// Defines CBaseMediaFilter
//
// Abstract base class implementing IMediaFilter.
//
// Typically you will derive your filter from CBaseFilter rather than
// this,  unless you are implementing an object such as a plug-in
// distributor that needs to support IMediaFilter but not IBaseFilter.
//
// Note that IMediaFilter is derived from IPersist to allow query of
// class id.
//=====================================================================
//=====================================================================

class AM_NOVTABLE CBaseMediaFilter
    : public CUnknown
    , public IMediaFilter
{

  protected:
    FILTER_STATE m_State;      // current state: running, paused
    IReferenceClock *m_pClock; // this filter's reference clock
    // note: all filters in a filter graph use the same clock

    // offset from stream time to reference time
    CRefTime m_tStart;

    CLSID m_clsid;     // This filters clsid
                       // used for serialization
    CCritSec *m_pLock; // Object we use for locking

  public:
    CBaseMediaFilter(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, __in CCritSec *pLock, REFCLSID clsid);

    virtual ~CBaseMediaFilter();

    DECLARE_IUNKNOWN

    // override this to say what interfaces we support where
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);

    //
    // --- IPersist method ---
    //

    STDMETHODIMP GetClassID(__out CLSID *pClsID);

    // --- IMediaFilter methods ---

    STDMETHODIMP GetState(DWORD dwMSecs, __out FILTER_STATE *State);

    STDMETHODIMP SetSyncSource(__inout_opt IReferenceClock *pClock);

    STDMETHODIMP GetSyncSource(__deref_out_opt IReferenceClock **pClock);

    // default implementation of Stop and Pause just record the
    // state. Override to activate or de-activate your filter.
    // Note that Run when called from Stopped state will call Pause
    // to ensure activation, so if you are a source or transform
    // you will probably not need to override Run.
    STDMETHODIMP Stop();
    STDMETHODIMP Pause();

    // the start parameter is the difference to be added to the
    // sample's stream time to get the reference time for
    // its presentation
    STDMETHODIMP Run(REFERENCE_TIME tStart);

    // --- helper methods ---

    // return the current stream time - ie find out what
    // stream time should be appearing now
    virtual HRESULT StreamTime(CRefTime &rtStream);

    // Is the filter currently active? (running or paused)
    BOOL IsActive()
    {
        CAutoLock cObjectLock(m_pLock);
        return ((m_State == State_Paused) || (m_State == State_Running));
    };
};

//=====================================================================
//=====================================================================
// Defines CBaseFilter
//
// An abstract class providing basic IBaseFilter support for pin
// enumeration and filter information reading.
//
// We cannot derive from CBaseMediaFilter since methods in IMediaFilter
// are also in IBaseFilter and would be ambiguous. Since much of the code
// assumes that they derive from a class that has m_State and other state
// directly available, we duplicate code from CBaseMediaFilter rather than
// having a member variable.
//
// Derive your filter from this, or from a derived object such as
// CTransformFilter.
//=====================================================================
//=====================================================================

class AM_NOVTABLE CBaseFilter
    : public CUnknown
    , // Handles an IUnknown
      public IBaseFilter
    ,                     // The Filter Interface
      public IAMovieSetup // For un/registration
{

    friend class CBasePin;

  protected:
    FILTER_STATE m_State;      // current state: running, paused
    IReferenceClock *m_pClock; // this graph's ref clock
    CRefTime m_tStart;         // offset from stream time to reference time
    CLSID m_clsid;             // This filters clsid
                               // used for serialization
    CCritSec *m_pLock;         // Object we use for locking

    WCHAR *m_pName;           // Full filter name
    IFilterGraph *m_pGraph;   // Graph we belong to
    IMediaEventSink *m_pSink; // Called with notify events
    LONG m_PinVersion;        // Current pin version

  public:
    CBaseFilter(__in_opt LPCTSTR pName,     // Object description
                __inout_opt LPUNKNOWN pUnk, // IUnknown of delegating object
                __in CCritSec *pLock,       // Object who maintains lock
                REFCLSID clsid);            // The clsid to be used to serialize this filter

    CBaseFilter(__in_opt LPCTSTR pName,  // Object description
                __in_opt LPUNKNOWN pUnk, // IUnknown of delegating object
                __in CCritSec *pLock,    // Object who maintains lock
                REFCLSID clsid,          // The clsid to be used to serialize this filter
                __inout HRESULT *phr);   // General OLE return code
#ifdef UNICODE
    CBaseFilter(__in_opt LPCSTR pName,   // Object description
                __in_opt LPUNKNOWN pUnk, // IUnknown of delegating object
                __in CCritSec *pLock,    // Object who maintains lock
                REFCLSID clsid);         // The clsid to be used to serialize this filter

    CBaseFilter(__in_opt LPCSTR pName,   // Object description
                __in_opt LPUNKNOWN pUnk, // IUnknown of delegating object
                __in CCritSec *pLock,    // Object who maintains lock
                REFCLSID clsid,          // The clsid to be used to serialize this filter
                __inout HRESULT *phr);   // General OLE return code
#endif
    ~CBaseFilter();

    DECLARE_IUNKNOWN

    // override this to say what interfaces we support where
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);
#ifdef DEBUG
    STDMETHODIMP_(ULONG) NonDelegatingRelease();
#endif

    //
    // --- IPersist method ---
    //

    STDMETHODIMP GetClassID(__out CLSID *pClsID);

    // --- IMediaFilter methods ---

    STDMETHODIMP GetState(DWORD dwMSecs, __out FILTER_STATE *State);

    STDMETHODIMP SetSyncSource(__in_opt IReferenceClock *pClock);

    STDMETHODIMP GetSyncSource(__deref_out_opt IReferenceClock **pClock);

    // override Stop and Pause so we can activate the pins.
    // Note that Run will call Pause first if activation needed.
    // Override these if you want to activate your filter rather than
    // your pins.
    STDMETHODIMP Stop();
    STDMETHODIMP Pause();

    // the start parameter is the difference to be added to the
    // sample's stream time to get the reference time for
    // its presentation
    STDMETHODIMP Run(REFERENCE_TIME tStart);

    // --- helper methods ---

    // return the current stream time - ie find out what
    // stream time should be appearing now
    virtual HRESULT StreamTime(CRefTime &rtStream);

    // Is the filter currently active?
    BOOL IsActive()
    {
        CAutoLock cObjectLock(m_pLock);
        return ((m_State == State_Paused) || (m_State == State_Running));
    };

    // Is this filter stopped (without locking)
    BOOL IsStopped() { return (m_State == State_Stopped); };

    //
    // --- IBaseFilter methods ---
    //

    // pin enumerator
    STDMETHODIMP EnumPins(__deref_out IEnumPins **ppEnum);

    // default behaviour of FindPin assumes pin ids are their names
    STDMETHODIMP FindPin(LPCWSTR Id, __deref_out IPin **ppPin);

    STDMETHODIMP QueryFilterInfo(__out FILTER_INFO *pInfo);

    STDMETHODIMP JoinFilterGraph(__inout_opt IFilterGraph *pGraph, __in_opt LPCWSTR pName);

    // return a Vendor information string. Optional - may return E_NOTIMPL.
    // memory returned should be freed using CoTaskMemFree
    // default implementation returns E_NOTIMPL
    STDMETHODIMP QueryVendorInfo(__deref_out LPWSTR *pVendorInfo);

    // --- helper methods ---

    // send an event notification to the filter graph if we know about it.
    // returns S_OK if delivered, S_FALSE if the filter graph does not sink
    // events, or an error otherwise.
    HRESULT NotifyEvent(long EventCode, LONG_PTR EventParam1, LONG_PTR EventParam2);

    // return the filter graph we belong to
    __out_opt IFilterGraph *GetFilterGraph() { return m_pGraph; }

    // Request reconnect
    // pPin is the pin to reconnect
    // pmt is the type to reconnect with - can be NULL
    // Calls ReconnectEx on the filter graph
    HRESULT ReconnectPin(IPin *pPin, __in_opt AM_MEDIA_TYPE const *pmt);

    // find out the current pin version (used by enumerators)
    virtual LONG GetPinVersion();
    void IncrementPinVersion();

    // you need to supply these to access the pins from the enumerator
    // and for default Stop and Pause/Run activation.
    virtual int GetPinCount() PURE;
    virtual CBasePin *GetPin(int n) PURE;

    // --- IAMovieSetup methods ---

    STDMETHODIMP Register();   // ask filter to register itself
    STDMETHODIMP Unregister(); // and unregister itself

    // --- setup helper methods ---
    // (override to return filters setup data)

    virtual __out_opt LPAMOVIESETUP_FILTER GetSetupData() { return NULL; }
};

//=====================================================================
//=====================================================================
// Defines CBasePin
//
// Abstract class that supports the basics of IPin
//=====================================================================
//=====================================================================

class AM_NOVTABLE CBasePin
    : public CUnknown
    , public IPin
    , public IQualityControl
{

  protected:
    WCHAR *m_pName;                 // This pin's name
    IPin *m_Connected;              // Pin we have connected to
    PIN_DIRECTION m_dir;            // Direction of this pin
    CCritSec *m_pLock;              // Object we use for locking
    bool m_bRunTimeError;           // Run time error generated
    bool m_bCanReconnectWhenActive; // OK to reconnect when active
    bool m_bTryMyTypesFirst;        // When connecting enumerate
                                    // this pin's types first
    CBaseFilter *m_pFilter;         // Filter we were created by
    IQualityControl *m_pQSink;      // Target for Quality messages
    LONG m_TypeVersion;             // Holds current type version
    CMediaType m_mt;                // Media type of connection

    CRefTime m_tStart; // time from NewSegment call
    CRefTime m_tStop;  // time from NewSegment
    double m_dRate;    // rate from NewSegment

#ifdef DEBUG
    LONG m_cRef; // Ref count tracing
#endif

    // displays pin connection information

#ifdef DEBUG
    void DisplayPinInfo(IPin *pReceivePin);
    void DisplayTypeInfo(IPin *pPin, const CMediaType *pmt);
#else
    void DisplayPinInfo(IPin *pReceivePin){};
    void DisplayTypeInfo(IPin *pPin, const CMediaType *pmt){};
#endif

    // used to agree a media type for a pin connection

    // given a specific media type, attempt a connection (includes
    // checking that the type is acceptable to this pin)
    HRESULT
    AttemptConnection(IPin *pReceivePin,    // connect to this pin
                      const CMediaType *pmt // using this type
    );

    // try all the media types in this enumerator - for each that
    // we accept, try to connect using ReceiveConnection.
    HRESULT TryMediaTypes(IPin *pReceivePin,              // connect to this pin
                          __in_opt const CMediaType *pmt, // proposed type from Connect
                          IEnumMediaTypes *pEnum);        // try this enumerator

    // establish a connection with a suitable mediatype. Needs to
    // propose a media type if the pmt pointer is null or partially
    // specified - use TryMediaTypes on both our and then the other pin's
    // enumerator until we find one that works.
    HRESULT AgreeMediaType(IPin *pReceivePin,      // connect to this pin
                           const CMediaType *pmt); // proposed type from Connect

  public:
    CBasePin(__in_opt LPCTSTR pObjectName, // Object description
             __in CBaseFilter *pFilter,    // Owning filter who knows about pins
             __in CCritSec *pLock,         // Object who implements the lock
             __inout HRESULT *phr,         // General OLE return code
             __in_opt LPCWSTR pName,       // Pin name for us
             PIN_DIRECTION dir);           // Either PINDIR_INPUT or PINDIR_OUTPUT
#ifdef UNICODE
    CBasePin(__in_opt LPCSTR pObjectName, // Object description
             __in CBaseFilter *pFilter,   // Owning filter who knows about pins
             __in CCritSec *pLock,        // Object who implements the lock
             __inout HRESULT *phr,        // General OLE return code
             __in_opt LPCWSTR pName,      // Pin name for us
             PIN_DIRECTION dir);          // Either PINDIR_INPUT or PINDIR_OUTPUT
#endif
    virtual ~CBasePin();

    DECLARE_IUNKNOWN

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);
    STDMETHODIMP_(ULONG) NonDelegatingRelease();
    STDMETHODIMP_(ULONG) NonDelegatingAddRef();

    // --- IPin methods ---

    // take lead role in establishing a connection. Media type pointer
    // may be null, or may point to partially-specified mediatype
    // (subtype or format type may be GUID_NULL).
    STDMETHODIMP Connect(IPin *pReceivePin,
                         __in_opt const AM_MEDIA_TYPE *pmt // optional media type
    );

    // (passive) accept a connection from another pin
    STDMETHODIMP ReceiveConnection(IPin *pConnector,        // this is the initiating connecting pin
                                   const AM_MEDIA_TYPE *pmt // this is the media type we will exchange
    );

    STDMETHODIMP Disconnect();

    STDMETHODIMP ConnectedTo(__deref_out IPin **pPin);

    STDMETHODIMP ConnectionMediaType(__out AM_MEDIA_TYPE *pmt);

    STDMETHODIMP QueryPinInfo(__out PIN_INFO *pInfo);

    STDMETHODIMP QueryDirection(__out PIN_DIRECTION *pPinDir);

    STDMETHODIMP QueryId(__deref_out LPWSTR *Id);

    // does the pin support this media type
    STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE *pmt);

    // return an enumerator for this pins preferred media types
    STDMETHODIMP EnumMediaTypes(__deref_out IEnumMediaTypes **ppEnum);

    // return an array of IPin* - the pins that this pin internally connects to
    // All pins put in the array must be AddReffed (but no others)
    // Errors: "Can't say" - FAIL, not enough slots - return S_FALSE
    // Default: return E_NOTIMPL
    // The filter graph will interpret NOT_IMPL as any input pin connects to
    // all visible output pins and vice versa.
    // apPin can be NULL if nPin==0 (not otherwise).
    STDMETHODIMP QueryInternalConnections(__out_ecount_part(*nPin, *nPin) IPin **apPin, // array of IPin*
                                          __inout ULONG *nPin                           // on input, the number of slots
                                                                                        // on output  the number of pins
    )
    {
        return E_NOTIMPL;
    }

    // Called when no more data will be sent
    STDMETHODIMP EndOfStream(void);

    // Begin/EndFlush still PURE

    // NewSegment notifies of the start/stop/rate applying to the data
    // about to be received. Default implementation records data and
    // returns S_OK.
    // Override this to pass downstream.
    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    //================================================================================
    // IQualityControl methods
    //================================================================================

    STDMETHODIMP Notify(IBaseFilter *pSender, Quality q);

    STDMETHODIMP SetSink(IQualityControl *piqc);

    // --- helper methods ---

    // Returns true if the pin is connected. false otherwise.
    BOOL IsConnected(void) { return (m_Connected != NULL); };
    // Return the pin this is connected to (if any)
    IPin *GetConnected() { return m_Connected; };

    // Check if our filter is currently stopped
    BOOL IsStopped() { return (m_pFilter->m_State == State_Stopped); };

    // find out the current type version (used by enumerators)
    virtual LONG GetMediaTypeVersion();
    void IncrementTypeVersion();

    // switch the pin to active (paused or running) mode
    // not an error to call this if already active
    virtual HRESULT Active(void);

    // switch the pin to inactive state - may already be inactive
    virtual HRESULT Inactive(void);

    // Notify of Run() from filter
    virtual HRESULT Run(REFERENCE_TIME tStart);

    // check if the pin can support this specific proposed type and format
    virtual HRESULT CheckMediaType(const CMediaType *) PURE;

    // set the connection to use this format (previously agreed)
    virtual HRESULT SetMediaType(const CMediaType *);

    // check that the connection is ok before verifying it
    // can be overridden eg to check what interfaces will be supported.
    virtual HRESULT CheckConnect(IPin *);

    // Set and release resources required for a connection
    virtual HRESULT BreakConnect();
    virtual HRESULT CompleteConnect(IPin *pReceivePin);

    // returns the preferred formats for a pin
    virtual HRESULT GetMediaType(int iPosition, __inout CMediaType *pMediaType);

    // access to NewSegment values
    REFERENCE_TIME CurrentStopTime() { return m_tStop; }
    REFERENCE_TIME CurrentStartTime() { return m_tStart; }
    double CurrentRate() { return m_dRate; }

    //  Access name
    LPWSTR Name() { return m_pName; };

    //  Can reconnectwhen active?
    void SetReconnectWhenActive(bool bCanReconnect) { m_bCanReconnectWhenActive = bCanReconnect; }

    bool CanReconnectWhenActive() { return m_bCanReconnectWhenActive; }

  protected:
    STDMETHODIMP DisconnectInternal();
};

//=====================================================================
//=====================================================================
// Defines CEnumPins
//
// Pin enumerator class that works by calling CBaseFilter. This interface
// is provided by CBaseFilter::EnumPins and calls GetPinCount() and
// GetPin() to enumerate existing pins. Needs to be a separate object so
// that it can be cloned (creating an existing object at the same
// position in the enumeration)
//
//=====================================================================
//=====================================================================

class CEnumPins : public IEnumPins // The interface we support
{
    int m_Position;         // Current ordinal position
    int m_PinCount;         // Number of pins available
    CBaseFilter *m_pFilter; // The filter who owns us
    LONG m_Version;         // Pin version information
    LONG m_cRef;

    typedef CGenericList<CBasePin> CPinList;

    CPinList m_PinCache; // These pointers have not been AddRef'ed and
                         // so they should not be dereferenced.  They are
                         // merely kept to ID which pins have been enumerated.

#ifdef DEBUG
    DWORD m_dwCookie;
#endif

    /* If while we are retrieving a pin for example from the filter an error
       occurs we assume that our internal state is stale with respect to the
       filter (someone may have deleted all the pins). We can check before
       starting whether or not the operation is likely to fail by asking the
       filter what it's current version number is. If the filter has not
       overriden the GetPinVersion method then this will always match */

    BOOL AreWeOutOfSync() { return (m_pFilter->GetPinVersion() == m_Version ? FALSE : TRUE); };

    /* This method performs the same operations as Reset, except is does not clear
       the cache of pins already enumerated. */

    STDMETHODIMP Refresh();

  public:
    CEnumPins(__in CBaseFilter *pFilter, __in_opt CEnumPins *pEnumPins);

    virtual ~CEnumPins();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, __deref_out void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IEnumPins
    STDMETHODIMP Next(ULONG cPins,                       // place this many pins...
                      __out_ecount(cPins) IPin **ppPins, // ...in this array of IPin*
                      __out_opt ULONG *pcFetched         // actual count passed returned here
    );

    STDMETHODIMP Skip(ULONG cPins);
    STDMETHODIMP Reset();
    STDMETHODIMP Clone(__deref_out IEnumPins **ppEnum);
};

//=====================================================================
//=====================================================================
// Defines CEnumMediaTypes
//
// Enumerates the preferred formats for input and output pins
//=====================================================================
//=====================================================================

class CEnumMediaTypes : public IEnumMediaTypes // The interface we support
{
    int m_Position;   // Current ordinal position
    CBasePin *m_pPin; // The pin who owns us
    LONG m_Version;   // Media type version value
    LONG m_cRef;
#ifdef DEBUG
    DWORD m_dwCookie;
#endif

    /* The media types a filter supports can be quite dynamic so we add to
       the general IEnumXXXX interface the ability to be signaled when they
       change via an event handle the connected filter supplies. Until the
       Reset method is called after the state changes all further calls to
       the enumerator (except Reset) will return E_UNEXPECTED error code */

    BOOL AreWeOutOfSync() { return (m_pPin->GetMediaTypeVersion() == m_Version ? FALSE : TRUE); };

  public:
    CEnumMediaTypes(__in CBasePin *pPin, __in_opt CEnumMediaTypes *pEnumMediaTypes);

    virtual ~CEnumMediaTypes();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, __deref_out void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IEnumMediaTypes
    STDMETHODIMP Next(ULONG cMediaTypes,                                      // place this many pins...
                      __out_ecount(cMediaTypes) AM_MEDIA_TYPE **ppMediaTypes, // ...in this array
                      __out_opt ULONG *pcFetched                              // actual count passed
    );

    STDMETHODIMP Skip(ULONG cMediaTypes);
    STDMETHODIMP Reset();
    STDMETHODIMP Clone(__deref_out IEnumMediaTypes **ppEnum);
};

//=====================================================================
//=====================================================================
// Defines CBaseOutputPin
//
// class derived from CBasePin that can pass buffers to a connected pin
// that supports IMemInputPin. Supports IPin.
//
// Derive your output pin from this.
//
//=====================================================================
//=====================================================================

class AM_NOVTABLE CBaseOutputPin : public CBasePin
{

  protected:
    IMemAllocator *m_pAllocator;
    IMemInputPin *m_pInputPin; // interface on the downstreaminput pin
                               // set up in CheckConnect when we connect.

  public:
    CBaseOutputPin(__in_opt LPCTSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock, __inout HRESULT *phr,
                   __in_opt LPCWSTR pName);
#ifdef UNICODE
    CBaseOutputPin(__in_opt LPCSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock, __inout HRESULT *phr,
                   __in_opt LPCWSTR pName);
#endif
    // override CompleteConnect() so we can negotiate an allocator
    virtual HRESULT CompleteConnect(IPin *pReceivePin);

    // negotiate the allocator and its buffer size/count and other properties
    // Calls DecideBufferSize to set properties
    virtual HRESULT DecideAllocator(IMemInputPin *pPin, __deref_out IMemAllocator **pAlloc);

    // override this to set the buffer size and count. Return an error
    // if the size/count is not to your liking.
    // The allocator properties passed in are those requested by the
    // input pin - use eg the alignment and prefix members if you have
    // no preference on these.
    virtual HRESULT DecideBufferSize(IMemAllocator *pAlloc, __inout ALLOCATOR_PROPERTIES *ppropInputRequest) PURE;

    // returns an empty sample buffer from the allocator
    virtual HRESULT GetDeliveryBuffer(__deref_out IMediaSample **ppSample, __in_opt REFERENCE_TIME *pStartTime,
                                      __in_opt REFERENCE_TIME *pEndTime, DWORD dwFlags);

    // deliver a filled-in sample to the connected input pin
    // note - you need to release it after calling this. The receiving
    // pin will addref the sample if it needs to hold it beyond the
    // call.
    virtual HRESULT Deliver(IMediaSample *);

    // override this to control the connection
    virtual HRESULT InitAllocator(__deref_out IMemAllocator **ppAlloc);
    HRESULT CheckConnect(IPin *pPin);
    HRESULT BreakConnect();

    // override to call Commit and Decommit
    HRESULT Active(void);
    HRESULT Inactive(void);

    // we have a default handling of EndOfStream which is to return
    // an error, since this should be called on input pins only
    STDMETHODIMP EndOfStream(void);

    // called from elsewhere in our filter to pass EOS downstream to
    // our connected input pin
    virtual HRESULT DeliverEndOfStream(void);

    // same for Begin/EndFlush - we handle Begin/EndFlush since it
    // is an error on an output pin, and we have Deliver methods to
    // call the methods on the connected pin
    STDMETHODIMP BeginFlush(void);
    STDMETHODIMP EndFlush(void);
    virtual HRESULT DeliverBeginFlush(void);
    virtual HRESULT DeliverEndFlush(void);

    // deliver NewSegment to connected pin - you will need to
    // override this if you queue any data in your output pin.
    virtual HRESULT DeliverNewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    //================================================================================
    // IQualityControl methods
    //================================================================================

    // All inherited from CBasePin and not overridden here.
    // STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);
    // STDMETHODIMP SetSink(IQualityControl * piqc);
};

//=====================================================================
//=====================================================================
// Defines CBaseInputPin
//
// derive your standard input pin from this.
// you need to supply GetMediaType and CheckConnect etc (see CBasePin),
// and you need to supply Receive to do something more useful.
//
//=====================================================================
//=====================================================================

class AM_NOVTABLE CBaseInputPin
    : public CBasePin
    , public IMemInputPin
{

  protected:
    IMemAllocator *m_pAllocator; // Default memory allocator

    // allocator is read-only, so received samples
    // cannot be modified (probably only relevant to in-place
    // transforms
    BYTE m_bReadOnly;

    // in flushing state (between BeginFlush and EndFlush)
    // if TRUE, all Receives are returned with S_FALSE
    BYTE m_bFlushing;

    // Sample properties - initalized in Receive
    AM_SAMPLE2_PROPERTIES m_SampleProps;

  public:
    CBaseInputPin(__in_opt LPCTSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock, __inout HRESULT *phr,
                  __in_opt LPCWSTR pName);
#ifdef UNICODE
    CBaseInputPin(__in_opt LPCSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock, __inout HRESULT *phr,
                  __in_opt LPCWSTR pName);
#endif
    virtual ~CBaseInputPin();

    DECLARE_IUNKNOWN

    // override this to publicise our interfaces
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);

    // return the allocator interface that this input pin
    // would like the output pin to use
    STDMETHODIMP GetAllocator(__deref_out IMemAllocator **ppAllocator);

    // tell the input pin which allocator the output pin is actually
    // going to use.
    STDMETHODIMP NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly);

    // do something with this media sample
    STDMETHODIMP Receive(IMediaSample *pSample);

    // do something with these media samples
    STDMETHODIMP ReceiveMultiple(__in_ecount(nSamples) IMediaSample **pSamples, long nSamples,
                                 __out long *nSamplesProcessed);

    // See if Receive() blocks
    STDMETHODIMP ReceiveCanBlock();

    // Default handling for BeginFlush - call at the beginning
    // of your implementation (makes sure that all Receive calls
    // fail). After calling this, you need to free any queued data
    // and then call downstream.
    STDMETHODIMP BeginFlush(void);

    // default handling for EndFlush - call at end of your implementation
    // - before calling this, ensure that there is no queued data and no thread
    // pushing any more without a further receive, then call downstream,
    // then call this method to clear the m_bFlushing flag and re-enable
    // receives
    STDMETHODIMP EndFlush(void);

    // this method is optional (can return E_NOTIMPL).
    // default implementation returns E_NOTIMPL. Override if you have
    // specific alignment or prefix needs, but could use an upstream
    // allocator
    STDMETHODIMP GetAllocatorRequirements(__out ALLOCATOR_PROPERTIES *pProps);

    // Release the pin's allocator.
    HRESULT BreakConnect();

    // helper method to check the read-only flag
    BOOL IsReadOnly() { return m_bReadOnly; };

    // helper method to see if we are flushing
    BOOL IsFlushing() { return m_bFlushing; };

    //  Override this for checking whether it's OK to process samples
    //  Also call this from EndOfStream.
    virtual HRESULT CheckStreaming();

    // Pass a Quality notification on to the appropriate sink
    HRESULT PassNotify(Quality &q);

    //================================================================================
    // IQualityControl methods (from CBasePin)
    //================================================================================

    STDMETHODIMP Notify(IBaseFilter *pSender, Quality q);

    // no need to override:
    // STDMETHODIMP SetSink(IQualityControl * piqc);

    // switch the pin to inactive state - may already be inactive
    virtual HRESULT Inactive(void);

    // Return sample properties pointer
    AM_SAMPLE2_PROPERTIES *SampleProps()
    {
        ASSERT(m_SampleProps.cbData != 0);
        return &m_SampleProps;
    }
};

///////////////////////////////////////////////////////////////////////////
// CDynamicOutputPin
//

class CDynamicOutputPin
    : public CBaseOutputPin
    , public IPinFlowControl
{
  public:
#ifdef UNICODE
    CDynamicOutputPin(__in_opt LPCSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                      __inout HRESULT *phr, __in_opt LPCWSTR pName);
#endif

    CDynamicOutputPin(__in_opt LPCTSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                      __inout HRESULT *phr, __in_opt LPCWSTR pName);

    ~CDynamicOutputPin();

    // IUnknown Methods
    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);

    // IPin Methods
    STDMETHODIMP Disconnect(void);

    // IPinFlowControl Methods
    STDMETHODIMP Block(DWORD dwBlockFlags, HANDLE hEvent);

    //  Set graph config info
    void SetConfigInfo(IGraphConfig *pGraphConfig, HANDLE hStopEvent);

#ifdef DEBUG
    virtual HRESULT Deliver(IMediaSample *pSample);
    virtual HRESULT DeliverEndOfStream(void);
    virtual HRESULT DeliverNewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);
#endif // DEBUG

    HRESULT DeliverBeginFlush(void);
    HRESULT DeliverEndFlush(void);

    HRESULT Inactive(void);
    HRESULT Active(void);
    virtual HRESULT CompleteConnect(IPin *pReceivePin);

    virtual HRESULT StartUsingOutputPin(void);
    virtual void StopUsingOutputPin(void);
    virtual bool StreamingThreadUsingOutputPin(void);

    HRESULT ChangeOutputFormat(const AM_MEDIA_TYPE *pmt, REFERENCE_TIME tSegmentStart, REFERENCE_TIME tSegmentStop,
                               double dSegmentRate);
    HRESULT ChangeMediaType(const CMediaType *pmt);
    HRESULT DynamicReconnect(const CMediaType *pmt);

  protected:
    HRESULT SynchronousBlockOutputPin(void);
    HRESULT AsynchronousBlockOutputPin(HANDLE hNotifyCallerPinBlockedEvent);
    HRESULT UnblockOutputPin(void);

    void BlockOutputPin(void);
    void ResetBlockState(void);

    static HRESULT WaitEvent(HANDLE hEvent);

    enum BLOCK_STATE
    {
        NOT_BLOCKED,
        PENDING,
        BLOCKED
    };

    // This lock should be held when the following class members are
    // being used: m_hNotifyCallerPinBlockedEvent, m_BlockState,
    // m_dwBlockCallerThreadID and m_dwNumOutstandingOutputPinUsers.
    CCritSec m_BlockStateLock;

    // This event should be signaled when the output pin is
    // not blocked.  This is a manual reset event.  For more
    // information on events, see the documentation for
    // CreateEvent() in the Windows SDK.
    HANDLE m_hUnblockOutputPinEvent;

    // This event will be signaled when block operation succeedes or
    // when the user cancels the block operation.  The block operation
    // can be canceled by calling IPinFlowControl2::Block( 0, NULL )
    // while the block operation is pending.
    HANDLE m_hNotifyCallerPinBlockedEvent;

    // The state of the current block operation.
    BLOCK_STATE m_BlockState;

    // The ID of the thread which last called IPinFlowControl::Block().
    // For more information on thread IDs, see the documentation for
    // GetCurrentThreadID() in the Windows SDK.
    DWORD m_dwBlockCallerThreadID;

    // The number of times StartUsingOutputPin() has been sucessfully
    // called and a corresponding call to StopUsingOutputPin() has not
    // been made.  When this variable is greater than 0, the streaming
    // thread is calling IPin::NewSegment(), IPin::EndOfStream(),
    // IMemInputPin::Receive() or IMemInputPin::ReceiveMultiple().  The
    // streaming thread could also be calling: DynamicReconnect(),
    // ChangeMediaType() or ChangeOutputFormat().  The output pin cannot
    // be blocked while the output pin is being used.
    DWORD m_dwNumOutstandingOutputPinUsers;

    // This event should be set when the IMediaFilter::Stop() is called.
    // This is a manual reset event.  It is also set when the output pin
    // delivers a flush to the connected input pin.
    HANDLE m_hStopEvent;
    IGraphConfig *m_pGraphConfig;

    // TRUE if the output pin's allocator's samples are read only.
    // Otherwise FALSE.  For more information, see the documentation
    // for IMemInputPin::NotifyAllocator().
    BOOL m_bPinUsesReadOnlyAllocator;

  private:
    HRESULT Initialize(void);
    HRESULT ChangeMediaTypeHelper(const CMediaType *pmt);

#ifdef DEBUG
    void AssertValid(void);
#endif // DEBUG
};

class CAutoUsingOutputPin
{
  public:
    CAutoUsingOutputPin(__in CDynamicOutputPin *pOutputPin, __inout HRESULT *phr);
    ~CAutoUsingOutputPin();

  private:
    CDynamicOutputPin *m_pOutputPin;
};

inline CAutoUsingOutputPin::CAutoUsingOutputPin(__in CDynamicOutputPin *pOutputPin, __inout HRESULT *phr)
    : m_pOutputPin(NULL)
{
    // The caller should always pass in valid pointers.
    ASSERT(NULL != pOutputPin);
    ASSERT(NULL != phr);

    // Make sure the user initialized phr.
    ASSERT(S_OK == *phr);

    HRESULT hr = pOutputPin->StartUsingOutputPin();
    if (FAILED(hr))
    {
        *phr = hr;
        return;
    }

    m_pOutputPin = pOutputPin;
}

inline CAutoUsingOutputPin::~CAutoUsingOutputPin()
{
    if (NULL != m_pOutputPin)
    {
        m_pOutputPin->StopUsingOutputPin();
    }
}

#ifdef DEBUG

inline HRESULT CDynamicOutputPin::Deliver(IMediaSample *pSample)
{
    // The caller should call StartUsingOutputPin() before calling this
    // method.
    ASSERT(StreamingThreadUsingOutputPin());

    return CBaseOutputPin::Deliver(pSample);
}

inline HRESULT CDynamicOutputPin::DeliverEndOfStream(void)
{
    // The caller should call StartUsingOutputPin() before calling this
    // method.
    ASSERT(StreamingThreadUsingOutputPin());

    return CBaseOutputPin::DeliverEndOfStream();
}

inline HRESULT CDynamicOutputPin::DeliverNewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    // The caller should call StartUsingOutputPin() before calling this
    // method.
    ASSERT(StreamingThreadUsingOutputPin());

    return CBaseOutputPin::DeliverNewSegment(tStart, tStop, dRate);
}

#endif // DEBUG

//=====================================================================
//=====================================================================
// Memory allocators
//
// the shared memory transport between pins requires the input pin
// to provide a memory allocator that can provide sample objects. A
// sample object supports the IMediaSample interface.
//
// CBaseAllocator handles the management of free and busy samples. It
// allocates CMediaSample objects. CBaseAllocator is an abstract class:
// in particular it has no method of initializing the list of free
// samples. CMemAllocator is derived from CBaseAllocator and initializes
// the list of samples using memory from the standard IMalloc interface.
//
// If you want your buffers to live in some special area of memory,
// derive your allocator object from CBaseAllocator. If you derive your
// IMemInputPin interface object from CBaseMemInputPin, you will get
// CMemAllocator-based allocation etc for free and will just need to
// supply the Receive handling, and media type / format negotiation.
//=====================================================================
//=====================================================================

//=====================================================================
//=====================================================================
// Defines CMediaSample
//
// an object of this class supports IMediaSample and represents a buffer
// for media data with some associated properties. Releasing it returns
// it to a freelist managed by a CBaseAllocator derived object.
//=====================================================================
//=====================================================================

class CMediaSample : public IMediaSample2 // The interface we support
{

  protected:
    friend class CBaseAllocator;

    /*  Values for dwFlags - these are used for backward compatiblity
        only now - use AM_SAMPLE_xxx
    */
    enum
    {
        Sample_SyncPoint = 0x01,         /* Is this a sync point */
        Sample_Preroll = 0x02,           /* Is this a preroll sample */
        Sample_Discontinuity = 0x04,     /* Set if start of new segment */
        Sample_TypeChanged = 0x08,       /* Has the type changed */
        Sample_TimeValid = 0x10,         /* Set if time is valid */
        Sample_MediaTimeValid = 0x20,    /* Is the media time valid */
        Sample_TimeDiscontinuity = 0x40, /* Time discontinuity */
        Sample_StopValid = 0x100,        /* Stop time valid */
        Sample_ValidFlags = 0x1FF
    };

    /* Properties, the media sample class can be a container for a format
       change in which case we take a copy of a type through the SetMediaType
       interface function and then return it when GetMediaType is called. As
       we do no internal processing on it we leave it as a pointer */

    DWORD m_dwFlags;                                 /* Flags for this sample */
                                                     /* Type specific flags are packed
                                                        into the top word
                                                     */
    DWORD m_dwTypeSpecificFlags;                     /* Media type specific flags */
    __field_ecount_opt(m_cbBuffer) LPBYTE m_pBuffer; /* Pointer to the complete buffer */
    LONG m_lActual;                                  /* Length of data in this sample */
    LONG m_cbBuffer;                                 /* Size of the buffer */
    CBaseAllocator *m_pAllocator;                    /* The allocator who owns us */
    CMediaSample *m_pNext;                           /* Chaining in free list */
    REFERENCE_TIME m_Start;                          /* Start sample time */
    REFERENCE_TIME m_End;                            /* End sample time */
    LONGLONG m_MediaStart;                           /* Real media start position */
    LONG m_MediaEnd;                                 /* A difference to get the end */
    AM_MEDIA_TYPE *m_pMediaType;                     /* Media type change data */
    DWORD m_dwStreamId;                              /* Stream id */
  public:
    LONG m_cRef; /* Reference count */

  public:
    CMediaSample(__in_opt LPCTSTR pName, __in_opt CBaseAllocator *pAllocator, __inout_opt HRESULT *phr,
                 __in_bcount_opt(length) LPBYTE pBuffer = NULL, LONG length = 0);
#ifdef UNICODE
    CMediaSample(__in_opt LPCSTR pName, __in_opt CBaseAllocator *pAllocator, __inout_opt HRESULT *phr,
                 __in_bcount_opt(length) LPBYTE pBuffer = NULL, LONG length = 0);
#endif

    virtual ~CMediaSample();

    /* Note the media sample does not delegate to its owner */

    STDMETHODIMP QueryInterface(REFIID riid, __deref_out void **ppv);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // set the buffer pointer and length. Used by allocators that
    // want variable sized pointers or pointers into already-read data.
    // This is only available through a CMediaSample* not an IMediaSample*
    // and so cannot be changed by clients.
    HRESULT SetPointer(__in_bcount(cBytes) BYTE *ptr, LONG cBytes);

    // Get me a read/write pointer to this buffer's memory.
    STDMETHODIMP GetPointer(__deref_out BYTE **ppBuffer);

    STDMETHODIMP_(LONG) GetSize(void);

    // get the stream time at which this sample should start and finish.
    STDMETHODIMP GetTime(__out REFERENCE_TIME *pTimeStart, // put time here
                         __out REFERENCE_TIME *pTimeEnd);

    // Set the stream time at which this sample should start and finish.
    STDMETHODIMP SetTime(__in_opt REFERENCE_TIME *pTimeStart, // put time here
                         __in_opt REFERENCE_TIME *pTimeEnd);
    STDMETHODIMP IsSyncPoint(void);
    STDMETHODIMP SetSyncPoint(BOOL bIsSyncPoint);
    STDMETHODIMP IsPreroll(void);
    STDMETHODIMP SetPreroll(BOOL bIsPreroll);

    STDMETHODIMP_(LONG) GetActualDataLength(void);
    STDMETHODIMP SetActualDataLength(LONG lActual);

    // these allow for limited format changes in band

    STDMETHODIMP GetMediaType(__deref_out AM_MEDIA_TYPE **ppMediaType);
    STDMETHODIMP SetMediaType(__in_opt AM_MEDIA_TYPE *pMediaType);

    // returns S_OK if there is a discontinuity in the data (this same is
    // not a continuation of the previous stream of data
    // - there has been a seek).
    STDMETHODIMP IsDiscontinuity(void);
    // set the discontinuity property - TRUE if this sample is not a
    // continuation, but a new sample after a seek.
    STDMETHODIMP SetDiscontinuity(BOOL bDiscontinuity);

    // get the media times for this sample
    STDMETHODIMP GetMediaTime(__out LONGLONG *pTimeStart, __out LONGLONG *pTimeEnd);

    // Set the media times for this sample
    STDMETHODIMP SetMediaTime(__in_opt LONGLONG *pTimeStart, __in_opt LONGLONG *pTimeEnd);

    // Set and get properties (IMediaSample2)
    STDMETHODIMP GetProperties(DWORD cbProperties, __out_bcount(cbProperties) BYTE *pbProperties);

    STDMETHODIMP SetProperties(DWORD cbProperties, __in_bcount(cbProperties) const BYTE *pbProperties);
};

//=====================================================================
//=====================================================================
// Defines CBaseAllocator
//
// Abstract base class that manages a list of media samples
//
// This class provides support for getting buffers from the free list,
// including handling of commit and (asynchronous) decommit.
//
// Derive from this class and override the Alloc and Free functions to
// allocate your CMediaSample (or derived) objects and add them to the
// free list, preparing them as necessary.
//=====================================================================
//=====================================================================

class AM_NOVTABLE CBaseAllocator
    : public CUnknown
    , // A non delegating IUnknown
      public IMemAllocatorCallbackTemp
    ,                 // The interface we support
      public CCritSec // Provides object locking
{
    class CSampleList;
    friend class CSampleList;

    /*  Trick to get at protected member in CMediaSample */
    static CMediaSample *&NextSample(__in CMediaSample *pSample) { return pSample->m_pNext; };

    /*  Mini list class for the free list */
    class CSampleList
    {
      public:
        CSampleList()
            : m_List(NULL)
            , m_nOnList(0){};
#ifdef DEBUG
        ~CSampleList() { ASSERT(m_nOnList == 0); };
#endif
        CMediaSample *Head() const { return m_List; };
        CMediaSample *Next(__in CMediaSample *pSample) const { return CBaseAllocator::NextSample(pSample); };
        int GetCount() const { return m_nOnList; };
        void Add(__inout CMediaSample *pSample)
        {
            ASSERT(pSample != NULL);
            CBaseAllocator::NextSample(pSample) = m_List;
            m_List = pSample;
            m_nOnList++;
        };
        CMediaSample *RemoveHead()
        {
            CMediaSample *pSample = m_List;
            if (pSample != NULL)
            {
                m_List = CBaseAllocator::NextSample(m_List);
                m_nOnList--;
            }
            return pSample;
        };
        void Remove(__inout CMediaSample *pSample);

      public:
        CMediaSample *m_List;
        int m_nOnList;
    };

  protected:
    CSampleList m_lFree; // Free list

    /*  Note to overriders of CBaseAllocator.

        We use a lazy signalling mechanism for waiting for samples.
        This means we don't call the OS if no waits occur.

        In order to implement this:

        1. When a new sample is added to m_lFree call NotifySample() which
           calls ReleaseSemaphore on m_hSem with a count of m_lWaiting and
           sets m_lWaiting to 0.
           This must all be done holding the allocator's critical section.

        2. When waiting for a sample call SetWaiting() which increments
           m_lWaiting BEFORE leaving the allocator's critical section.

        3. Actually wait by calling WaitForSingleObject(m_hSem, INFINITE)
           having left the allocator's critical section.  The effect of
           this is to remove 1 from the semaphore's count.  You MUST call
           this once having incremented m_lWaiting.

        The following are then true when the critical section is not held :
            (let nWaiting = number about to wait or waiting)

            (1) if (m_lFree.GetCount() != 0) then (m_lWaiting == 0)
            (2) m_lWaiting + Semaphore count == nWaiting

        We would deadlock if
           nWaiting != 0 &&
           m_lFree.GetCount() != 0 &&
           Semaphore count == 0

           But from (1) if m_lFree.GetCount() != 0 then m_lWaiting == 0 so
           from (2) Semaphore count == nWaiting (which is non-0) so the
           deadlock can't happen.
    */

    HANDLE m_hSem;     // For signalling
    long m_lWaiting;   // Waiting for a free element
    long m_lCount;     // how many buffers we have agreed to provide
    long m_lAllocated; // how many buffers are currently allocated
    long m_lSize;      // agreed size of each buffer
    long m_lAlignment; // agreed alignment
    long m_lPrefix;    // agreed prefix (preceeds GetPointer() value)
    BOOL m_bChanged;   // Have the buffer requirements changed

    // if true, we are decommitted and can't allocate memory
    BOOL m_bCommitted;
    // if true, the decommit has happened, but we haven't called Free yet
    // as there are still outstanding buffers
    BOOL m_bDecommitInProgress;

    //  Notification interface
    IMemAllocatorNotifyCallbackTemp *m_pNotify;

    BOOL m_fEnableReleaseCallback;

    // called to decommit the memory when the last buffer is freed
    // pure virtual - need to override this
    virtual void Free(void) PURE;

    // override to allocate the memory when commit called
    virtual HRESULT Alloc(void);

  public:
    CBaseAllocator(__in_opt LPCTSTR, __inout_opt LPUNKNOWN, __inout HRESULT *, BOOL bEvent = TRUE,
                   BOOL fEnableReleaseCallback = FALSE);
#ifdef UNICODE
    CBaseAllocator(__in_opt LPCSTR, __inout_opt LPUNKNOWN, __inout HRESULT *, BOOL bEvent = TRUE,
                   BOOL fEnableReleaseCallback = FALSE);
#endif
    virtual ~CBaseAllocator();

    DECLARE_IUNKNOWN

    // override this to publicise our interfaces
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);

    STDMETHODIMP SetProperties(__in ALLOCATOR_PROPERTIES *pRequest, __out ALLOCATOR_PROPERTIES *pActual);

    // return the properties actually being used on this allocator
    STDMETHODIMP GetProperties(__out ALLOCATOR_PROPERTIES *pProps);

    // override Commit to allocate memory. We handle the GetBuffer
    // state changes
    STDMETHODIMP Commit();

    // override this to handle the memory freeing. We handle any outstanding
    // GetBuffer calls
    STDMETHODIMP Decommit();

    // get container for a sample. Blocking, synchronous call to get the
    // next free buffer (as represented by an IMediaSample interface).
    // on return, the time etc properties will be invalid, but the buffer
    // pointer and size will be correct. The two time parameters are
    // optional and either may be NULL, they may alternatively be set to
    // the start and end times the sample will have attached to it
    // bPrevFramesSkipped is not used (used only by the video renderer's
    // allocator where it affects quality management in direct draw).

    STDMETHODIMP GetBuffer(__deref_out IMediaSample **ppBuffer, __in_opt REFERENCE_TIME *pStartTime,
                           __in_opt REFERENCE_TIME *pEndTime, DWORD dwFlags);

    // final release of a CMediaSample will call this
    STDMETHODIMP ReleaseBuffer(IMediaSample *pBuffer);
    // obsolete:: virtual void PutOnFreeList(CMediaSample * pSample);

    STDMETHODIMP SetNotify(IMemAllocatorNotifyCallbackTemp *pNotify);

    STDMETHODIMP GetFreeCount(__out LONG *plBuffersFree);

    // Notify that a sample is available
    void NotifySample();

    // Notify that we're waiting for a sample
    void SetWaiting() { m_lWaiting++; };
};

//=====================================================================
//=====================================================================
// Defines CMemAllocator
//
// this is an allocator based on CBaseAllocator that allocates sample
// buffers in main memory (from 'new'). You must call SetProperties
// before calling Commit.
//
// we don't free the memory when going into Decommit state. The simplest
// way to implement this without complicating CBaseAllocator is to
// have a Free() function, called to go into decommit state, that does
// nothing and a ReallyFree function called from our destructor that
// actually frees the memory.
//=====================================================================
//=====================================================================

//  Make me one from quartz.dll
STDAPI CreateMemoryAllocator(__deref_out IMemAllocator **ppAllocator);

class CMemAllocator : public CBaseAllocator
{

  protected:
    LPBYTE m_pBuffer; // combined memory for all buffers

    // override to free the memory when decommit completes
    // - we actually do nothing, and save the memory until deletion.
    void Free(void);

    // called from the destructor (and from Alloc if changing size/count) to
    // actually free up the memory
    void ReallyFree(void);

    // overriden to allocate the memory when commit called
    HRESULT Alloc(void);

  public:
    /* This goes in the factory template table to create new instances */
    static CUnknown *CreateInstance(__inout_opt LPUNKNOWN, __inout HRESULT *);

    STDMETHODIMP SetProperties(__in ALLOCATOR_PROPERTIES *pRequest, __out ALLOCATOR_PROPERTIES *pActual);

    CMemAllocator(__in_opt LPCTSTR, __inout_opt LPUNKNOWN, __inout HRESULT *);
#ifdef UNICODE
    CMemAllocator(__in_opt LPCSTR, __inout_opt LPUNKNOWN, __inout HRESULT *);
#endif
    ~CMemAllocator();
};

// helper used by IAMovieSetup implementation
STDAPI
AMovieSetupRegisterFilter(const AMOVIESETUP_FILTER *const psetupdata, IFilterMapper *pIFM, BOOL bRegister);

///////////////////////////////////////////////////////////////////////////
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////

#endif /* __FILTER__ */
