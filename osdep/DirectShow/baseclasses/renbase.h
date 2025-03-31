//------------------------------------------------------------------------------
// File: RenBase.h
//
// Desc: DirectShow base classes - defines a generic ActiveX base renderer
//       class.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __RENBASE__
#define __RENBASE__

// Forward class declarations

class CBaseRenderer;
class CBaseVideoRenderer;
class CRendererInputPin;

// This is our input pin class that channels calls to the renderer

class CRendererInputPin : public CBaseInputPin
{
  protected:
    CBaseRenderer *m_pRenderer;

  public:
    CRendererInputPin(__inout CBaseRenderer *pRenderer, __inout HRESULT *phr, __in_opt LPCWSTR Name);

    // Overriden from the base pin classes

    HRESULT BreakConnect();
    HRESULT CompleteConnect(IPin *pReceivePin);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT CheckMediaType(const CMediaType *pmt);
    HRESULT Active();
    HRESULT Inactive();

    // Add rendering behaviour to interface functions

    STDMETHODIMP QueryId(__deref_out LPWSTR *Id);
    STDMETHODIMP EndOfStream();
    STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();
    STDMETHODIMP Receive(IMediaSample *pMediaSample);

    // Helper
    IMemAllocator inline *Allocator() const { return m_pAllocator; }
};

// Main renderer class that handles synchronisation and state changes

class CBaseRenderer : public CBaseFilter
{
  protected:
    friend class CRendererInputPin;

    friend void CALLBACK EndOfStreamTimer(UINT uID,         // Timer identifier
                                          UINT uMsg,        // Not currently used
                                          DWORD_PTR dwUser, // User information
                                          DWORD_PTR dw1,    // Windows reserved
                                          DWORD_PTR dw2);   // Is also reserved

    CRendererPosPassThru *m_pPosition; // Media seeking pass by object
    CAMEvent m_RenderEvent;            // Used to signal timer events
    CAMEvent m_ThreadSignal;           // Signalled to release worker thread
    CAMEvent m_evComplete;             // Signalled when state complete
    BOOL m_bAbort;                     // Stop us from rendering more data
    BOOL m_bStreaming;                 // Are we currently streaming
    DWORD_PTR m_dwAdvise;              // Timer advise cookie
    IMediaSample *m_pMediaSample;      // Current image media sample
    BOOL m_bEOS;                       // Any more samples in the stream
    BOOL m_bEOSDelivered;              // Have we delivered an EC_COMPLETE
    CRendererInputPin *m_pInputPin;    // Our renderer input pin object
    CCritSec m_InterfaceLock;          // Critical section for interfaces
    CCritSec m_RendererLock;           // Controls access to internals
    IQualityControl *m_pQSink;         // QualityControl sink
    BOOL m_bRepaintStatus;             // Can we signal an EC_REPAINT
    //  Avoid some deadlocks by tracking filter during stop
    volatile BOOL m_bInReceive;    // Inside Receive between PrepareReceive
                                   // And actually processing the sample
    REFERENCE_TIME m_SignalTime;   // Time when we signal EC_COMPLETE
    UINT m_EndOfStreamTimer;       // Used to signal end of stream
    CCritSec m_ObjectCreationLock; // This lock protects the creation and
                                   // of m_pPosition and m_pInputPin.  It
                                   // ensures that two threads cannot create
                                   // either object simultaneously.

  public:
    CBaseRenderer(REFCLSID RenderClass,       // CLSID for this renderer
                  __in_opt LPCTSTR pName,     // Debug ONLY description
                  __inout_opt LPUNKNOWN pUnk, // Aggregated owner object
                  __inout HRESULT *phr);      // General OLE return code

    ~CBaseRenderer();

    // Overriden to say what interfaces we support and where

    virtual HRESULT GetMediaPositionInterface(REFIID riid, __deref_out void **ppv);
    STDMETHODIMP NonDelegatingQueryInterface(REFIID, __deref_out void **);

    virtual HRESULT SourceThreadCanWait(BOOL bCanWait);

#ifdef DEBUG
    // Debug only dump of the renderer state
    void DisplayRendererState();
#endif
    virtual HRESULT WaitForRenderTime();
    virtual HRESULT CompleteStateChange(FILTER_STATE OldState);

    // Return internal information about this filter

    BOOL IsEndOfStream() { return m_bEOS; };
    BOOL IsEndOfStreamDelivered() { return m_bEOSDelivered; };
    BOOL IsStreaming() { return m_bStreaming; };
    void SetAbortSignal(BOOL bAbort) { m_bAbort = bAbort; };
    virtual void OnReceiveFirstSample(IMediaSample *pMediaSample){};
    CAMEvent *GetRenderEvent() { return &m_RenderEvent; };

    // Permit access to the transition state

    void Ready() { m_evComplete.Set(); };
    void NotReady() { m_evComplete.Reset(); };
    BOOL CheckReady() { return m_evComplete.Check(); };

    virtual int GetPinCount();
    virtual CBasePin *GetPin(int n);
    FILTER_STATE GetRealState();
    void SendRepaint();
    void SendNotifyWindow(IPin *pPin, HWND hwnd);
    BOOL OnDisplayChange();
    void SetRepaintStatus(BOOL bRepaint);

    // Override the filter and pin interface functions

    STDMETHODIMP Stop();
    STDMETHODIMP Pause();
    STDMETHODIMP Run(REFERENCE_TIME StartTime);
    STDMETHODIMP GetState(DWORD dwMSecs, __out FILTER_STATE *State);
    STDMETHODIMP FindPin(LPCWSTR Id, __deref_out IPin **ppPin);

    // These are available for a quality management implementation

    virtual void OnRenderStart(IMediaSample *pMediaSample);
    virtual void OnRenderEnd(IMediaSample *pMediaSample);
    virtual HRESULT OnStartStreaming() { return NOERROR; };
    virtual HRESULT OnStopStreaming() { return NOERROR; };
    virtual void OnWaitStart(){};
    virtual void OnWaitEnd(){};
    virtual void PrepareRender(){};

#ifdef PERF
    REFERENCE_TIME m_trRenderStart; // Just before we started drawing
                                    // Set in OnRenderStart, Used in OnRenderEnd
    int m_idBaseStamp;              // MSR_id for frame time stamp
    int m_idBaseRenderTime;         // MSR_id for true wait time
    int m_idBaseAccuracy;           // MSR_id for time frame is late (int)
#endif

    // Quality management implementation for scheduling rendering

    virtual BOOL ScheduleSample(IMediaSample *pMediaSample);
    virtual HRESULT GetSampleTimes(IMediaSample *pMediaSample, __out REFERENCE_TIME *pStartTime,
                                   __out REFERENCE_TIME *pEndTime);

    virtual HRESULT ShouldDrawSampleNow(IMediaSample *pMediaSample, __out REFERENCE_TIME *ptrStart,
                                        __out REFERENCE_TIME *ptrEnd);

    // Lots of end of stream complexities

    void TimerCallback();
    void ResetEndOfStreamTimer();
    HRESULT NotifyEndOfStream();
    virtual HRESULT SendEndOfStream();
    virtual HRESULT ResetEndOfStream();
    virtual HRESULT EndOfStream();

    // Rendering is based around the clock

    void SignalTimerFired();
    virtual HRESULT CancelNotification();
    virtual HRESULT ClearPendingSample();

    // Called when the filter changes state

    virtual HRESULT Active();
    virtual HRESULT Inactive();
    virtual HRESULT StartStreaming();
    virtual HRESULT StopStreaming();
    virtual HRESULT BeginFlush();
    virtual HRESULT EndFlush();

    // Deal with connections and type changes

    virtual HRESULT BreakConnect();
    virtual HRESULT SetMediaType(const CMediaType *pmt);
    virtual HRESULT CompleteConnect(IPin *pReceivePin);

    // These look after the handling of data samples

    virtual HRESULT PrepareReceive(IMediaSample *pMediaSample);
    virtual HRESULT Receive(IMediaSample *pMediaSample);
    virtual BOOL HaveCurrentSample();
    virtual IMediaSample *GetCurrentSample();
    virtual HRESULT Render(IMediaSample *pMediaSample);

    // Derived classes MUST override these
    virtual HRESULT DoRenderSample(IMediaSample *pMediaSample) PURE;
    virtual HRESULT CheckMediaType(const CMediaType *) PURE;

    // Helper
    void WaitForReceiveToComplete();
};

// CBaseVideoRenderer is a renderer class (see its ancestor class) and
// it handles scheduling of media samples so that they are drawn at the
// correct time by the reference clock.  It implements a degradation
// strategy.  Possible degradation modes are:
//    Drop frames here (only useful if the drawing takes significant time)
//    Signal supplier (upstream) to drop some frame(s) - i.e. one-off skip.
//    Signal supplier to change the frame rate - i.e. ongoing skipping.
//    Or any combination of the above.
// In order to determine what's useful to try we need to know what's going
// on.  This is done by timing various operations (including the supplier).
// This timing is done by using timeGetTime as it is accurate enough and
// usually cheaper than calling the reference clock.  It also tells the
// truth if there is an audio break and the reference clock stops.
// We provide a number of public entry points (named OnXxxStart, OnXxxEnd)
// which the rest of the renderer calls at significant moments.  These do
// the timing.

// the number of frames that the sliding averages are averaged over.
// the rule is (1024*NewObservation + (AVGPERIOD-1) * PreviousAverage)/AVGPERIOD
#define AVGPERIOD 4
#define DO_MOVING_AVG(avg, obs) (avg = (1024 * obs + (AVGPERIOD - 1) * avg) / AVGPERIOD)
// Spot the bug in this macro - I can't. but it doesn't work!

class CBaseVideoRenderer
    : public CBaseRenderer
    , // Base renderer class
      public IQualProp
    ,                        // Property page guff
      public IQualityControl // Allow throttling
{
  protected:
    // Hungarian:
    //     tFoo is the time Foo in mSec (beware m_tStart from filter.h)
    //     trBar is the time Bar by the reference clock

    //******************************************************************
    // State variables to control synchronisation
    //******************************************************************

    // Control of sending Quality messages.  We need to know whether
    // we are in trouble (e.g. frames being dropped) and where the time
    // is being spent.

    // When we drop a frame we play the next one early.
    // The frame after that is likely to wait before drawing and counting this
    // wait as spare time is unfair, so we count it as a zero wait.
    // We therefore need to know whether we are playing frames early or not.

    int m_nNormal; // The number of consecutive frames
                   // drawn at their normal time (not early)
                   // -1 means we just dropped a frame.

#ifdef PERF
    BOOL m_bDrawLateFrames; // Don't drop any frames (debug and I'm
                            // not keen on people using it!)
#endif

    BOOL m_bSupplierHandlingQuality; // The response to Quality messages says
                                     // our supplier is handling things.
                                     // We will allow things to go extra late
                                     // before dropping frames.  We will play
                                     // very early after he has dropped one.

    // Control of scheduling, frame dropping etc.
    // We need to know where the time is being spent so as to tell whether
    // we should be taking action here, signalling supplier or what.
    // The variables are initialised to a mode of NOT dropping frames.
    // They will tell the truth after a few frames.
    // We typically record a start time for an event, later we get the time
    // again and subtract to get the elapsed time, and we average this over
    // a few frames.  The average is used to tell what mode we are in.

    // Although these are reference times (64 bit) they are all DIFFERENCES
    // between times which are small.  An int will go up to 214 secs before
    // overflow.  Avoiding 64 bit multiplications and divisions seems
    // worth while.

    // Audio-video throttling.  If the user has turned up audio quality
    // very high (in principle it could be any other stream, not just audio)
    // then we can receive cries for help via the graph manager.  In this case
    // we put in a wait for some time after rendering each frame.
    int m_trThrottle;

    // The time taken to render (i.e. BitBlt) frames controls which component
    // needs to degrade.  If the blt is expensive, the renderer degrades.
    // If the blt is cheap it's done anyway and the supplier degrades.
    int m_trRenderAvg;  // Time frames are taking to blt
    int m_trRenderLast; // Time for last frame blt
    int m_tRenderStart; // Just before we started drawing (mSec)
                        // derived from timeGetTime.

    // When frames are dropped we will play the next frame as early as we can.
    // If it was a false alarm and the machine is fast we slide gently back to
    // normal timing.  To do this, we record the offset showing just how early
    // we really are.  This will normally be negative meaning early or zero.
    int m_trEarliness;

    // Target provides slow long-term feedback to try to reduce the
    // average sync offset to zero.  Whenever a frame is actually rendered
    // early we add a msec or two, whenever late we take off a few.
    // We add or take off 1/32 of the error time.
    // Eventually we should be hovering around zero.  For a really bad case
    // where we were (say) 300mSec off, it might take 100 odd frames to
    // settle down.  The rate of change of this is intended to be slower
    // than any other mechanism in Quartz, thereby avoiding hunting.
    int m_trTarget;

    // The proportion of time spent waiting for the right moment to blt
    // controls whether we bother to drop a frame or whether we reckon that
    // we're doing well enough that we can stand a one-frame glitch.
    int m_trWaitAvg; // Average of last few wait times
                     // (actually we just average how early
                     // we were).  Negative here means LATE.

    // The average inter-frame time.
    // This is used to calculate the proportion of the time used by the
    // three operations (supplying us, waiting, rendering)
    int m_trFrameAvg; // Average inter-frame time
    int m_trDuration; // duration of last frame.

#ifdef PERF
    // Performance logging identifiers
    int m_idTimeStamp;     // MSR_id for frame time stamp
    int m_idEarliness;     // MSR_id for earliness fudge
    int m_idTarget;        // MSR_id for Target fudge
    int m_idWaitReal;      // MSR_id for true wait time
    int m_idWait;          // MSR_id for wait time recorded
    int m_idFrameAccuracy; // MSR_id for time frame is late (int)
    int m_idRenderAvg;     // MSR_id for Render time recorded (int)
    int m_idSchLateTime;   // MSR_id for lateness at scheduler
    int m_idQualityRate;   // MSR_id for Quality rate requested
    int m_idQualityTime;   // MSR_id for Quality time requested
    int m_idDecision;      // MSR_id for decision code
    int m_idDuration;      // MSR_id for duration of a frame
    int m_idThrottle;      // MSR_id for audio-video throttling
    // int m_idDebug;                  // MSR_id for trace style debugging
    // int m_idSendQuality;          // MSR_id for timing the notifications per se
#endif                                       // PERF
    REFERENCE_TIME m_trRememberStampForPerf; // original time stamp of frame
                                             // with no earliness fudges etc.
#ifdef PERF
    REFERENCE_TIME m_trRememberFrameForPerf; // time when previous frame rendered

    // debug...
    int m_idFrameAvg;
    int m_idWaitAvg;
#endif

    // PROPERTY PAGE
    // This has edit fields that show the user what's happening
    // These member variables hold these counts.

    int m_cFramesDropped; // cumulative frames dropped IN THE RENDERER
    int m_cFramesDrawn;   // Frames since streaming started seen BY THE
                          // RENDERER (some may be dropped upstream)

    // Next two support average sync offset and standard deviation of sync offset.
    LONGLONG m_iTotAcc;   // Sum of accuracies in mSec
    LONGLONG m_iSumSqAcc; // Sum of squares of (accuracies in mSec)

    // Next two allow jitter calculation.  Jitter is std deviation of frame time.
    REFERENCE_TIME m_trLastDraw; // Time of prev frame (for inter-frame times)
    LONGLONG m_iSumSqFrameTime;  // Sum of squares of (inter-frame time in mSec)
    LONGLONG m_iSumFrameTime;    // Sum of inter-frame times in mSec

    // To get performance statistics on frame rate, jitter etc, we need
    // to record the lateness and inter-frame time.  What we actually need are the
    // data above (sum, sum of squares and number of entries for each) but the data
    // is generated just ahead of time and only later do we discover whether the
    // frame was actually drawn or not.  So we have to hang on to the data
    int m_trLate;  // hold onto frame lateness
    int m_trFrame; // hold onto inter-frame time

    int m_tStreamingStart; // if streaming then time streaming started
                           // else time of last streaming session
                           // used for property page statistics
#ifdef PERF
    LONGLONG m_llTimeOffset; // timeGetTime()*10000+m_llTimeOffset==ref time
#endif

  public:
    CBaseVideoRenderer(REFCLSID RenderClass,       // CLSID for this renderer
                       __in_opt LPCTSTR pName,     // Debug ONLY description
                       __inout_opt LPUNKNOWN pUnk, // Aggregated owner object
                       __inout HRESULT *phr);      // General OLE return code

    ~CBaseVideoRenderer();

    // IQualityControl methods - Notify allows audio-video throttling

    STDMETHODIMP SetSink(IQualityControl *piqc);
    STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q);

    // These provide a full video quality management implementation

    void OnRenderStart(IMediaSample *pMediaSample);
    void OnRenderEnd(IMediaSample *pMediaSample);
    void OnWaitStart();
    void OnWaitEnd();
    HRESULT OnStartStreaming();
    HRESULT OnStopStreaming();
    void ThrottleWait();

    // Handle the statistics gathering for our quality management

    void PreparePerformanceData(int trLate, int trFrame);
    virtual void RecordFrameLateness(int trLate, int trFrame);
    virtual void OnDirectRender(IMediaSample *pMediaSample);
    virtual HRESULT ResetStreamingTimes();
    BOOL ScheduleSample(IMediaSample *pMediaSample);
    HRESULT ShouldDrawSampleNow(IMediaSample *pMediaSample, __inout REFERENCE_TIME *ptrStart,
                                __inout REFERENCE_TIME *ptrEnd);

    virtual HRESULT SendQuality(REFERENCE_TIME trLate, REFERENCE_TIME trRealStream);
    STDMETHODIMP JoinFilterGraph(__inout_opt IFilterGraph *pGraph, __in_opt LPCWSTR pName);

    //
    //  Do estimates for standard deviations for per-frame
    //  statistics
    //
    //  *piResult = (llSumSq - iTot * iTot / m_cFramesDrawn - 1) /
    //                            (m_cFramesDrawn - 2)
    //  or 0 if m_cFramesDrawn <= 3
    //
    HRESULT GetStdDev(int nSamples, __out int *piResult, LONGLONG llSumSq, LONGLONG iTot);

  public:
    // IQualProp property page support

    STDMETHODIMP get_FramesDroppedInRenderer(__out int *cFramesDropped);
    STDMETHODIMP get_FramesDrawn(__out int *pcFramesDrawn);
    STDMETHODIMP get_AvgFrameRate(__out int *piAvgFrameRate);
    STDMETHODIMP get_Jitter(__out int *piJitter);
    STDMETHODIMP get_AvgSyncOffset(__out int *piAvg);
    STDMETHODIMP get_DevSyncOffset(__out int *piDev);

    // Implement an IUnknown interface and expose IQualProp

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out VOID **ppv);
};

#endif // __RENBASE__
