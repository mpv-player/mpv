//------------------------------------------------------------------------------
// File: StrmCtl.h
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1996-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __strmctl_h__
#define __strmctl_h__

class CBaseStreamControl : public IAMStreamControl
{
  public:
    // Used by the implementation
    enum StreamControlState
    {
        STREAM_FLOWING = 0x1000,
        STREAM_DISCARDING
    };

  private:
    enum StreamControlState m_StreamState;       // Current stream state
    enum StreamControlState m_StreamStateOnStop; // State after next stop
                                                 // (i.e.Blocking or Discarding)

    REFERENCE_TIME m_tStartTime;    // MAX_TIME implies none
    REFERENCE_TIME m_tStopTime;     // MAX_TIME implies none
    DWORD m_dwStartCookie;          // Cookie for notification to app
    DWORD m_dwStopCookie;           // Cookie for notification to app
    volatile BOOL m_bIsFlushing;    // No optimization pls!
    volatile BOOL m_bStopSendExtra; // bSendExtra was set
    volatile BOOL m_bStopExtraSent; // the extra one was sent

    CCritSec m_CritSec; // CritSec to guard above attributes

    // Event to fire when we can come
    // out of blocking, or to come out of waiting
    // to discard if we change our minds.
    //
    CAMEvent m_StreamEvent;

    // All of these methods execute immediately.  Helpers for others.
    //
    void ExecuteStop();
    void ExecuteStart();
    void CancelStop();
    void CancelStart();

    // Some things we need to be told by our owning filter
    // Your pin must also expose IAMStreamControl when QI'd for it!
    //
    IReferenceClock *m_pRefClock; // Need it to set advises
                                  // Filter must tell us via
                                  // SetSyncSource
    IMediaEventSink *m_pSink;     // Event sink
                                  // Filter must tell us after it
                                  // creates it in JoinFilterGraph()
    FILTER_STATE m_FilterState;   // Just need it!
                                  // Filter must tell us via
                                  // NotifyFilterState
    REFERENCE_TIME m_tRunStart;   // Per the Run call to the filter

    // This guy will return one of the three StreamControlState's.  Here's what
    // the caller should do for each one:
    //
    // STREAM_FLOWING:		Proceed as usual (render or pass the sample on)
    // STREAM_DISCARDING:	Calculate the time 'til *pSampleStop and wait
    //				that long for the event handle
    //				(GetStreamEventHandle()).  If the wait
    //				expires, throw the sample away.  If the event
    //				fires, call me back - I've changed my mind.
    //
    enum StreamControlState CheckSampleTimes(__in const REFERENCE_TIME *pSampleStart,
                                             __in const REFERENCE_TIME *pSampleStop);

  public:
    // You don't have to tell us much when we're created, but there are other
    // obligations that must be met.  See SetSyncSource & NotifyFilterState
    // below.
    //
    CBaseStreamControl(__inout_opt HRESULT *phr = NULL);
    ~CBaseStreamControl();

    // If you want this class to work properly, there are thing you need to
    // (keep) telling it.  Filters with pins that use this class
    // should ensure that they pass through to this method any calls they
    // receive on their SetSyncSource.

    // We need a clock to see what time it is.  This is for the
    // "discard in a timely fashion" logic.  If we discard everything as
    // quick as possible, a whole 60 minute file could get discarded in the
    // first 10 seconds, and if somebody wants to turn streaming on at 30
    // minutes into the file, and they make the call more than a few seconds
    // after the graph is run, it may be too late!
    // So we hold every sample until it's time has gone, then we discard it.
    // The filter should call this when it gets a SetSyncSource
    //
    void SetSyncSource(IReferenceClock *pRefClock)
    {
        CAutoLock lck(&m_CritSec);
        if (m_pRefClock)
            m_pRefClock->Release();
        m_pRefClock = pRefClock;
        if (m_pRefClock)
            m_pRefClock->AddRef();
    }

    // Set event sink for notifications
    // The filter should call this in its JoinFilterGraph after it creates the
    // IMediaEventSink
    //
    void SetFilterGraph(IMediaEventSink *pSink) { m_pSink = pSink; }

    // Since we schedule in stream time, we need the tStart and must track the
    // state of our owning filter.
    // The app should call this ever state change
    //
    void NotifyFilterState(FILTER_STATE new_state, REFERENCE_TIME tStart = 0);

    // Filter should call Flushing(TRUE) in BeginFlush,
    // and Flushing(FALSE) in EndFlush.
    //
    void Flushing(BOOL bInProgress);

    // The two main methods of IAMStreamControl

    // Class adds default values suitable for immediate
    // muting and unmuting of the stream.

    STDMETHODIMP StopAt(const REFERENCE_TIME *ptStop = NULL, BOOL bSendExtra = FALSE, DWORD dwCookie = 0);
    STDMETHODIMP StartAt(const REFERENCE_TIME *ptStart = NULL, DWORD dwCookie = 0);
    STDMETHODIMP GetInfo(__out AM_STREAM_INFO *pInfo);

    // Helper function for pin's receive method.  Call this with
    // the sample and we'll tell you what to do with it.  We'll do a
    // WaitForSingleObject within this call if one is required.  This is
    // a "What should I do with this sample?" kind of call. We'll tell the
    // caller to either flow it or discard it.
    // If pSample is NULL we evaluate based on the current state
    // settings
    enum StreamControlState CheckStreamState(IMediaSample *pSample);

  private:
    // These don't require locking, but we are relying on the fact that
    // m_StreamState can be retrieved with integrity, and is a snap shot that
    // may have just been, or may be just about to be, changed.
    HANDLE GetStreamEventHandle() const { return m_StreamEvent; }
    enum StreamControlState GetStreamState() const { return m_StreamState; }
    BOOL IsStreaming() const { return m_StreamState == STREAM_FLOWING; }
};

#endif
