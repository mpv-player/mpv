//------------------------------------------------------------------------------
// File: RenBase.cpp
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>  // DirectShow base class definitions
#include <mmsystem.h> // Needed for definition of timeGetTime
#include <limits.h>   // Standard data type limit definitions
#include <measure.h>  // Used for time critical log functions

#pragma warning(disable : 4355)

//  Helper function for clamping time differences
int inline TimeDiff(REFERENCE_TIME rt)
{
    if (rt < -(50 * UNITS))
    {
        return -(50 * UNITS);
    }
    else if (rt > 50 * UNITS)
    {
        return 50 * UNITS;
    }
    else
        return (int)rt;
}

// Implements the CBaseRenderer class

CBaseRenderer::CBaseRenderer(REFCLSID RenderClass,       // CLSID for this renderer
                             __in_opt LPCTSTR pName,     // Debug ONLY description
                             __inout_opt LPUNKNOWN pUnk, // Aggregated owner object
                             __inout HRESULT *phr)
    : // General OLE return code

    CBaseFilter(pName, pUnk, &m_InterfaceLock, RenderClass)
    , m_evComplete(TRUE, phr)
    , m_RenderEvent(FALSE, phr)
    , m_bAbort(FALSE)
    , m_pPosition(NULL)
    , m_ThreadSignal(TRUE, phr)
    , m_bStreaming(FALSE)
    , m_bEOS(FALSE)
    , m_bEOSDelivered(FALSE)
    , m_pMediaSample(NULL)
    , m_dwAdvise(0)
    , m_pQSink(NULL)
    , m_pInputPin(NULL)
    , m_bRepaintStatus(TRUE)
    , m_SignalTime(0)
    , m_bInReceive(FALSE)
    , m_EndOfStreamTimer(0)
{
    if (SUCCEEDED(*phr))
    {
        Ready();
#ifdef PERF
        m_idBaseStamp = MSR_REGISTER(TEXT("BaseRenderer: sample time stamp"));
        m_idBaseRenderTime = MSR_REGISTER(TEXT("BaseRenderer: draw time (msec)"));
        m_idBaseAccuracy = MSR_REGISTER(TEXT("BaseRenderer: Accuracy (msec)"));
#endif
    }
}

// Delete the dynamically allocated IMediaPosition and IMediaSeeking helper
// object. The object is created when somebody queries us. These are standard
// control interfaces for seeking and setting start/stop positions and rates.
// We will probably also have made an input pin based on CRendererInputPin
// that has to be deleted, it's created when an enumerator calls our GetPin

CBaseRenderer::~CBaseRenderer()
{
    ASSERT(m_bStreaming == FALSE);
    ASSERT(m_EndOfStreamTimer == 0);
    StopStreaming();
    ClearPendingSample();

    // Delete any IMediaPosition implementation

    if (m_pPosition)
    {
        delete m_pPosition;
        m_pPosition = NULL;
    }

    // Delete any input pin created

    if (m_pInputPin)
    {
        delete m_pInputPin;
        m_pInputPin = NULL;
    }

    // Release any Quality sink

    ASSERT(m_pQSink == NULL);
}

// This returns the IMediaPosition and IMediaSeeking interfaces

HRESULT CBaseRenderer::GetMediaPositionInterface(REFIID riid, __deref_out void **ppv)
{
    CAutoLock cObjectCreationLock(&m_ObjectCreationLock);
    if (m_pPosition)
    {
        return m_pPosition->NonDelegatingQueryInterface(riid, ppv);
    }

    CBasePin *pPin = GetPin(0);
    if (NULL == pPin)
    {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = NOERROR;

    // Create implementation of this dynamically since sometimes we may
    // never try and do a seek. The helper object implements a position
    // control interface (IMediaPosition) which in fact simply takes the
    // calls normally from the filter graph and passes them upstream

    m_pPosition =
        new CRendererPosPassThru(NAME("Renderer CPosPassThru"), CBaseFilter::GetOwner(), (HRESULT *)&hr, pPin);
    if (m_pPosition == NULL)
    {
        return E_OUTOFMEMORY;
    }

    if (FAILED(hr))
    {
        delete m_pPosition;
        m_pPosition = NULL;
        return E_NOINTERFACE;
    }
    return GetMediaPositionInterface(riid, ppv);
}

// Overriden to say what interfaces we support and where

STDMETHODIMP CBaseRenderer::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    // Do we have this interface

    if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking)
    {
        return GetMediaPositionInterface(riid, ppv);
    }
    else
    {
        return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
    }
}

// This is called whenever we change states, we have a manual reset event that
// is signalled whenever we don't won't the source filter thread to wait in us
// (such as in a stopped state) and likewise is not signalled whenever it can
// wait (during paused and running) this function sets or resets the thread
// event. The event is used to stop source filter threads waiting in Receive

HRESULT CBaseRenderer::SourceThreadCanWait(BOOL bCanWait)
{
    if (bCanWait == TRUE)
    {
        m_ThreadSignal.Reset();
    }
    else
    {
        m_ThreadSignal.Set();
    }
    return NOERROR;
}

#ifdef DEBUG
// Dump the current renderer state to the debug terminal. The hardest part of
// the renderer is the window where we unlock everything to wait for a clock
// to signal it is time to draw or for the application to cancel everything
// by stopping the filter. If we get things wrong we can leave the thread in
// WaitForRenderTime with no way for it to ever get out and we will deadlock

void CBaseRenderer::DisplayRendererState()
{
    DbgLog((LOG_TIMING, 1, TEXT("\nTimed out in WaitForRenderTime")));

    // No way should this be signalled at this point

    BOOL bSignalled = m_ThreadSignal.Check();
    DbgLog((LOG_TIMING, 1, TEXT("Signal sanity check %d"), bSignalled));

    // Now output the current renderer state variables

    DbgLog((LOG_TIMING, 1, TEXT("Filter state %d"), m_State));

    DbgLog((LOG_TIMING, 1, TEXT("Abort flag %d"), m_bAbort));

    DbgLog((LOG_TIMING, 1, TEXT("Streaming flag %d"), m_bStreaming));

    DbgLog((LOG_TIMING, 1, TEXT("Clock advise link %d"), m_dwAdvise));

    DbgLog((LOG_TIMING, 1, TEXT("Current media sample %x"), m_pMediaSample));

    DbgLog((LOG_TIMING, 1, TEXT("EOS signalled %d"), m_bEOS));

    DbgLog((LOG_TIMING, 1, TEXT("EOS delivered %d"), m_bEOSDelivered));

    DbgLog((LOG_TIMING, 1, TEXT("Repaint status %d"), m_bRepaintStatus));

    // Output the delayed end of stream timer information

    DbgLog((LOG_TIMING, 1, TEXT("End of stream timer %x"), m_EndOfStreamTimer));

    DbgLog((LOG_TIMING, 1, TEXT("Deliver time %s"), CDisp((LONGLONG)m_SignalTime)));

    // Should never timeout during a flushing state

    BOOL bFlushing = m_pInputPin->IsFlushing();
    DbgLog((LOG_TIMING, 1, TEXT("Flushing sanity check %d"), bFlushing));

    // Display the time we were told to start at
    DbgLog((LOG_TIMING, 1, TEXT("Last run time %s"), CDisp((LONGLONG)m_tStart.m_time)));

    // Have we got a reference clock
    if (m_pClock == NULL)
        return;

    // Get the current time from the wall clock

    CRefTime CurrentTime, StartTime, EndTime;
    m_pClock->GetTime((REFERENCE_TIME *)&CurrentTime);
    CRefTime Offset = CurrentTime - m_tStart;

    // Display the current time from the clock

    DbgLog((LOG_TIMING, 1, TEXT("Clock time %s"), CDisp((LONGLONG)CurrentTime.m_time)));

    DbgLog((LOG_TIMING, 1, TEXT("Time difference %dms"), Offset.Millisecs()));

    // Do we have a sample ready to render
    if (m_pMediaSample == NULL)
        return;

    m_pMediaSample->GetTime((REFERENCE_TIME *)&StartTime, (REFERENCE_TIME *)&EndTime);
    DbgLog((LOG_TIMING, 1, TEXT("Next sample stream times (Start %d End %d ms)"), StartTime.Millisecs(),
            EndTime.Millisecs()));

    // Calculate how long it is until it is due for rendering
    CRefTime Wait = (m_tStart + StartTime) - CurrentTime;
    DbgLog((LOG_TIMING, 1, TEXT("Wait required %d ms"), Wait.Millisecs()));
}
#endif

// Wait until the clock sets the timer event or we're otherwise signalled. We
// set an arbitrary timeout for this wait and if it fires then we display the
// current renderer state on the debugger. It will often fire if the filter's
// left paused in an application however it may also fire during stress tests
// if the synchronisation with application seeks and state changes is faulty

#define RENDER_TIMEOUT 10000

HRESULT CBaseRenderer::WaitForRenderTime()
{
    HANDLE WaitObjects[] = {m_ThreadSignal, m_RenderEvent};
    DWORD Result = WAIT_TIMEOUT;

    // Wait for either the time to arrive or for us to be stopped

    OnWaitStart();
    while (Result == WAIT_TIMEOUT)
    {
        Result = WaitForMultipleObjects(2, WaitObjects, FALSE, RENDER_TIMEOUT);

#ifdef DEBUG
        if (Result == WAIT_TIMEOUT)
            DisplayRendererState();
#endif
    }
    OnWaitEnd();

    // We may have been awoken without the timer firing

    if (Result == WAIT_OBJECT_0)
    {
        return VFW_E_STATE_CHANGED;
    }

    SignalTimerFired();
    return NOERROR;
}

// Poll waiting for Receive to complete.  This really matters when
// Receive may set the palette and cause window messages
// The problem is that if we don't really wait for a renderer to
// stop processing we can deadlock waiting for a transform which
// is calling the renderer's Receive() method because the transform's
// Stop method doesn't know to process window messages to unblock
// the renderer's Receive processing
void CBaseRenderer::WaitForReceiveToComplete()
{
    for (;;)
    {
        if (!m_bInReceive)
        {
            break;
        }

        MSG msg;
        //  Receive all interthread snedmessages
        PeekMessage(&msg, NULL, WM_NULL, WM_NULL, PM_NOREMOVE);

        Sleep(1);
    }

    // If the wakebit for QS_POSTMESSAGE is set, the PeekMessage call
    // above just cleared the changebit which will cause some messaging
    // calls to block (waitMessage, MsgWaitFor...) now.
    // Post a dummy message to set the QS_POSTMESSAGE bit again
    if (HIWORD(GetQueueStatus(QS_POSTMESSAGE)) & QS_POSTMESSAGE)
    {
        //  Send dummy message
        PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
    }
}

// A filter can have four discrete states, namely Stopped, Running, Paused,
// Intermediate. We are in an intermediate state if we are currently trying
// to pause but haven't yet got the first sample (or if we have been flushed
// in paused state and therefore still have to wait for a sample to arrive)

// This class contains an event called m_evComplete which is signalled when
// the current state is completed and is not signalled when we are waiting to
// complete the last state transition. As mentioned above the only time we
// use this at the moment is when we wait for a media sample in paused state
// If while we are waiting we receive an end of stream notification from the
// source filter then we know no data is imminent so we can reset the event
// This means that when we transition to paused the source filter must call
// end of stream on us or send us an image otherwise we'll hang indefinately

// Simple internal way of getting the real state

FILTER_STATE CBaseRenderer::GetRealState()
{
    return m_State;
}

// The renderer doesn't complete the full transition to paused states until
// it has got one media sample to render. If you ask it for its state while
// it's waiting it will return the state along with VFW_S_STATE_INTERMEDIATE

STDMETHODIMP CBaseRenderer::GetState(DWORD dwMSecs, FILTER_STATE *State)
{
    CheckPointer(State, E_POINTER);

    if (WaitDispatchingMessages(m_evComplete, dwMSecs) == WAIT_TIMEOUT)
    {
        *State = m_State;
        return VFW_S_STATE_INTERMEDIATE;
    }
    *State = m_State;
    return NOERROR;
}

// If we're pausing and we have no samples we don't complete the transition
// to State_Paused and we return S_FALSE. However if the m_bAbort flag has
// been set then all samples are rejected so there is no point waiting for
// one. If we do have a sample then return NOERROR. We will only ever return
// VFW_S_STATE_INTERMEDIATE from GetState after being paused with no sample
// (calling GetState after either being stopped or Run will NOT return this)

HRESULT CBaseRenderer::CompleteStateChange(FILTER_STATE OldState)
{
    // Allow us to be paused when disconnected

    if (m_pInputPin->IsConnected() == FALSE)
    {
        Ready();
        return S_OK;
    }

    // Have we run off the end of stream

    if (IsEndOfStream() == TRUE)
    {
        Ready();
        return S_OK;
    }

    // Make sure we get fresh data after being stopped

    if (HaveCurrentSample() == TRUE)
    {
        if (OldState != State_Stopped)
        {
            Ready();
            return S_OK;
        }
    }
    NotReady();
    return S_FALSE;
}

// When we stop the filter the things we do are:-

//      Decommit the allocator being used in the connection
//      Release the source filter if it's waiting in Receive
//      Cancel any advise link we set up with the clock
//      Any end of stream signalled is now obsolete so reset
//      Allow us to be stopped when we are not connected

STDMETHODIMP CBaseRenderer::Stop()
{
    CAutoLock cRendererLock(&m_InterfaceLock);

    // Make sure there really is a state change

    if (m_State == State_Stopped)
    {
        return NOERROR;
    }

    // Is our input pin connected

    if (m_pInputPin->IsConnected() == FALSE)
    {
        NOTE("Input pin is not connected");
        m_State = State_Stopped;
        return NOERROR;
    }

    CBaseFilter::Stop();

    // If we are going into a stopped state then we must decommit whatever
    // allocator we are using it so that any source filter waiting in the
    // GetBuffer can be released and unlock themselves for a state change

    if (m_pInputPin->Allocator())
    {
        m_pInputPin->Allocator()->Decommit();
    }

    // Cancel any scheduled rendering

    SetRepaintStatus(TRUE);
    StopStreaming();
    SourceThreadCanWait(FALSE);
    ResetEndOfStream();
    CancelNotification();

    // There should be no outstanding clock advise
    ASSERT(CancelNotification() == S_FALSE);
    ASSERT(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)m_RenderEvent, 0));
    ASSERT(m_EndOfStreamTimer == 0);

    Ready();
    WaitForReceiveToComplete();
    m_bAbort = FALSE;

    return NOERROR;
}

// When we pause the filter the things we do are:-

//      Commit the allocator being used in the connection
//      Allow a source filter thread to wait in Receive
//      Cancel any clock advise link (we may be running)
//      Possibly complete the state change if we have data
//      Allow us to be paused when we are not connected

STDMETHODIMP CBaseRenderer::Pause()
{
    CAutoLock cRendererLock(&m_InterfaceLock);
    FILTER_STATE OldState = m_State;
    ASSERT(m_pInputPin->IsFlushing() == FALSE);

    // Make sure there really is a state change

    if (m_State == State_Paused)
    {
        return CompleteStateChange(State_Paused);
    }

    // Has our input pin been connected

    if (m_pInputPin->IsConnected() == FALSE)
    {
        NOTE("Input pin is not connected");
        m_State = State_Paused;
        return CompleteStateChange(State_Paused);
    }

    // Pause the base filter class

    HRESULT hr = CBaseFilter::Pause();
    if (FAILED(hr))
    {
        NOTE("Pause failed");
        return hr;
    }

    // Enable EC_REPAINT events again

    SetRepaintStatus(TRUE);
    StopStreaming();
    SourceThreadCanWait(TRUE);
    CancelNotification();
    ResetEndOfStreamTimer();

    // If we are going into a paused state then we must commit whatever
    // allocator we are using it so that any source filter can call the
    // GetBuffer and expect to get a buffer without returning an error

    if (m_pInputPin->Allocator())
    {
        m_pInputPin->Allocator()->Commit();
    }

    // There should be no outstanding advise
    ASSERT(CancelNotification() == S_FALSE);
    ASSERT(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)m_RenderEvent, 0));
    ASSERT(m_EndOfStreamTimer == 0);
    ASSERT(m_pInputPin->IsFlushing() == FALSE);

    // When we come out of a stopped state we must clear any image we were
    // holding onto for frame refreshing. Since renderers see state changes
    // first we can reset ourselves ready to accept the source thread data
    // Paused or running after being stopped causes the current position to
    // be reset so we're not interested in passing end of stream signals

    if (OldState == State_Stopped)
    {
        m_bAbort = FALSE;
        ClearPendingSample();
    }
    return CompleteStateChange(OldState);
}

// When we run the filter the things we do are:-

//      Commit the allocator being used in the connection
//      Allow a source filter thread to wait in Receive
//      Signal the render event just to get us going
//      Start the base class by calling StartStreaming
//      Allow us to be run when we are not connected
//      Signal EC_COMPLETE if we are not connected

STDMETHODIMP CBaseRenderer::Run(REFERENCE_TIME StartTime)
{
    CAutoLock cRendererLock(&m_InterfaceLock);
    FILTER_STATE OldState = m_State;

    // Make sure there really is a state change

    if (m_State == State_Running)
    {
        return NOERROR;
    }

    // Send EC_COMPLETE if we're not connected

    if (m_pInputPin->IsConnected() == FALSE)
    {
        NotifyEvent(EC_COMPLETE, S_OK, (LONG_PTR)(IBaseFilter *)this);
        m_State = State_Running;
        return NOERROR;
    }

    Ready();

    // Pause the base filter class

    HRESULT hr = CBaseFilter::Run(StartTime);
    if (FAILED(hr))
    {
        NOTE("Run failed");
        return hr;
    }

    // Allow the source thread to wait
    ASSERT(m_pInputPin->IsFlushing() == FALSE);
    SourceThreadCanWait(TRUE);
    SetRepaintStatus(FALSE);

    // There should be no outstanding advise
    ASSERT(CancelNotification() == S_FALSE);
    ASSERT(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)m_RenderEvent, 0));
    ASSERT(m_EndOfStreamTimer == 0);
    ASSERT(m_pInputPin->IsFlushing() == FALSE);

    // If we are going into a running state then we must commit whatever
    // allocator we are using it so that any source filter can call the
    // GetBuffer and expect to get a buffer without returning an error

    if (m_pInputPin->Allocator())
    {
        m_pInputPin->Allocator()->Commit();
    }

    // When we come out of a stopped state we must clear any image we were
    // holding onto for frame refreshing. Since renderers see state changes
    // first we can reset ourselves ready to accept the source thread data
    // Paused or running after being stopped causes the current position to
    // be reset so we're not interested in passing end of stream signals

    if (OldState == State_Stopped)
    {
        m_bAbort = FALSE;
        ClearPendingSample();
    }
    return StartStreaming();
}

// Return the number of input pins we support

int CBaseRenderer::GetPinCount()
{
    if (m_pInputPin == NULL)
    {
        //  Try to create it
        (void)GetPin(0);
    }
    return m_pInputPin != NULL ? 1 : 0;
}

// We only support one input pin and it is numbered zero

CBasePin *CBaseRenderer::GetPin(int n)
{
    CAutoLock cObjectCreationLock(&m_ObjectCreationLock);

    // Should only ever be called with zero
    ASSERT(n == 0);

    if (n != 0)
    {
        return NULL;
    }

    // Create the input pin if not already done so

    if (m_pInputPin == NULL)
    {

        // hr must be initialized to NOERROR because
        // CRendererInputPin's constructor only changes
        // hr's value if an error occurs.
        HRESULT hr = NOERROR;

        m_pInputPin = new CRendererInputPin(this, &hr, L"In");
        if (NULL == m_pInputPin)
        {
            return NULL;
        }

        if (FAILED(hr))
        {
            delete m_pInputPin;
            m_pInputPin = NULL;
            return NULL;
        }
    }
    return m_pInputPin;
}

// If "In" then return the IPin for our input pin, otherwise NULL and error

STDMETHODIMP CBaseRenderer::FindPin(LPCWSTR Id, __deref_out IPin **ppPin)
{
    CheckPointer(ppPin, E_POINTER);

    if (0 == lstrcmpW(Id, L"In"))
    {
        *ppPin = GetPin(0);
        if (*ppPin)
        {
            (*ppPin)->AddRef();
        }
        else
        {
            return E_OUTOFMEMORY;
        }
    }
    else
    {
        *ppPin = NULL;
        return VFW_E_NOT_FOUND;
    }
    return NOERROR;
}

// Called when the input pin receives an EndOfStream notification. If we have
// not got a sample, then notify EC_COMPLETE now. If we have samples, then set
// m_bEOS and check for this on completing samples. If we're waiting to pause
// then complete the transition to paused state by setting the state event

HRESULT CBaseRenderer::EndOfStream()
{
    // Ignore these calls if we are stopped

    if (m_State == State_Stopped)
    {
        return NOERROR;
    }

    // If we have a sample then wait for it to be rendered

    m_bEOS = TRUE;
    if (m_pMediaSample)
    {
        return NOERROR;
    }

    // If we are waiting for pause then we are now ready since we cannot now
    // carry on waiting for a sample to arrive since we are being told there
    // won't be any. This sets an event that the GetState function picks up

    Ready();

    // Only signal completion now if we are running otherwise queue it until
    // we do run in StartStreaming. This is used when we seek because a seek
    // causes a pause where early notification of completion is misleading

    if (m_bStreaming)
    {
        SendEndOfStream();
    }
    return NOERROR;
}

// When we are told to flush we should release the source thread

HRESULT CBaseRenderer::BeginFlush()
{
    // If paused then report state intermediate until we get some data

    if (m_State == State_Paused)
    {
        NotReady();
    }

    SourceThreadCanWait(FALSE);
    CancelNotification();
    ClearPendingSample();
    //  Wait for Receive to complete
    WaitForReceiveToComplete();

    return NOERROR;
}

// After flushing the source thread can wait in Receive again

HRESULT CBaseRenderer::EndFlush()
{
    // Reset the current sample media time
    if (m_pPosition)
        m_pPosition->ResetMediaTime();

    // There should be no outstanding advise

    ASSERT(CancelNotification() == S_FALSE);
    SourceThreadCanWait(TRUE);
    return NOERROR;
}

// We can now send EC_REPAINTs if so required

HRESULT CBaseRenderer::CompleteConnect(IPin *pReceivePin)
{
    // The caller should always hold the interface lock because
    // the function uses CBaseFilter::m_State.
    ASSERT(CritCheckIn(&m_InterfaceLock));

    m_bAbort = FALSE;

    if (State_Running == GetRealState())
    {
        HRESULT hr = StartStreaming();
        if (FAILED(hr))
        {
            return hr;
        }

        SetRepaintStatus(FALSE);
    }
    else
    {
        SetRepaintStatus(TRUE);
    }

    return NOERROR;
}

// Called when we go paused or running

HRESULT CBaseRenderer::Active()
{
    return NOERROR;
}

// Called when we go into a stopped state

HRESULT CBaseRenderer::Inactive()
{
    if (m_pPosition)
    {
        m_pPosition->ResetMediaTime();
    }
    //  People who derive from this may want to override this behaviour
    //  to keep hold of the sample in some circumstances
    ClearPendingSample();

    return NOERROR;
}

// Tell derived classes about the media type agreed

HRESULT CBaseRenderer::SetMediaType(const CMediaType *pmt)
{
    return NOERROR;
}

// When we break the input pin connection we should reset the EOS flags. When
// we are asked for either IMediaPosition or IMediaSeeking we will create a
// CPosPassThru object to handles media time pass through. When we're handed
// samples we store (by calling CPosPassThru::RegisterMediaTime) their media
// times so we can then return a real current position of data being rendered

HRESULT CBaseRenderer::BreakConnect()
{
    // Do we have a quality management sink

    if (m_pQSink)
    {
        m_pQSink->Release();
        m_pQSink = NULL;
    }

    // Check we have a valid connection

    if (m_pInputPin->IsConnected() == FALSE)
    {
        return S_FALSE;
    }

    // Check we are stopped before disconnecting
    if (m_State != State_Stopped && !m_pInputPin->CanReconnectWhenActive())
    {
        return VFW_E_NOT_STOPPED;
    }

    SetRepaintStatus(FALSE);
    ResetEndOfStream();
    ClearPendingSample();
    m_bAbort = FALSE;

    if (State_Running == m_State)
    {
        StopStreaming();
    }

    return NOERROR;
}

// Retrieves the sample times for this samples (note the sample times are
// passed in by reference not value). We return S_FALSE to say schedule this
// sample according to the times on the sample. We also return S_OK in
// which case the object should simply render the sample data immediately

HRESULT CBaseRenderer::GetSampleTimes(IMediaSample *pMediaSample, __out REFERENCE_TIME *pStartTime,
                                      __out REFERENCE_TIME *pEndTime)
{
    ASSERT(m_dwAdvise == 0);
    ASSERT(pMediaSample);

    // If the stop time for this sample is before or the same as start time,
    // then just ignore it (release it) and schedule the next one in line
    // Source filters should always fill in the start and end times properly!

    if (SUCCEEDED(pMediaSample->GetTime(pStartTime, pEndTime)))
    {
        if (*pEndTime < *pStartTime)
        {
            return VFW_E_START_TIME_AFTER_END;
        }
    }
    else
    {
        // no time set in the sample... draw it now?
        return S_OK;
    }

    // Can't synchronise without a clock so we return S_OK which tells the
    // caller that the sample should be rendered immediately without going
    // through the overhead of setting a timer advise link with the clock

    if (m_pClock == NULL)
    {
        return S_OK;
    }
    return ShouldDrawSampleNow(pMediaSample, pStartTime, pEndTime);
}

// By default all samples are drawn according to their time stamps so we
// return S_FALSE. Returning S_OK means draw immediately, this is used
// by the derived video renderer class in its quality management.

HRESULT CBaseRenderer::ShouldDrawSampleNow(IMediaSample *pMediaSample, __out REFERENCE_TIME *ptrStart,
                                           __out REFERENCE_TIME *ptrEnd)
{
    return S_FALSE;
}

// We must always reset the current advise time to zero after a timer fires
// because there are several possible ways which lead us not to do any more
// scheduling such as the pending image being cleared after state changes

void CBaseRenderer::SignalTimerFired()
{
    m_dwAdvise = 0;
}

// Cancel any notification currently scheduled. This is called by the owning
// window object when it is told to stop streaming. If there is no timer link
// outstanding then calling this is benign otherwise we go ahead and cancel
// We must always reset the render event as the quality management code can
// signal immediate rendering by setting the event without setting an advise
// link. If we're subsequently stopped and run the first attempt to setup an
// advise link with the reference clock will find the event still signalled

HRESULT CBaseRenderer::CancelNotification()
{
    ASSERT(m_dwAdvise == 0 || m_pClock);
    DWORD_PTR dwAdvise = m_dwAdvise;

    // Have we a live advise link

    if (m_dwAdvise)
    {
        m_pClock->Unadvise(m_dwAdvise);
        SignalTimerFired();
        ASSERT(m_dwAdvise == 0);
    }

    // Clear the event and return our status

    m_RenderEvent.Reset();
    return (dwAdvise ? S_OK : S_FALSE);
}

// Responsible for setting up one shot advise links with the clock
// Return FALSE if the sample is to be dropped (not drawn at all)
// Return TRUE if the sample is to be drawn and in this case also
// arrange for m_RenderEvent to be set at the appropriate time

BOOL CBaseRenderer::ScheduleSample(IMediaSample *pMediaSample)
{
    REFERENCE_TIME StartSample, EndSample;

    // Is someone pulling our leg

    if (pMediaSample == NULL)
    {
        return FALSE;
    }

    // Get the next sample due up for rendering.  If there aren't any ready
    // then GetNextSampleTimes returns an error.  If there is one to be done
    // then it succeeds and yields the sample times. If it is due now then
    // it returns S_OK other if it's to be done when due it returns S_FALSE

    HRESULT hr = GetSampleTimes(pMediaSample, &StartSample, &EndSample);
    if (FAILED(hr))
    {
        return FALSE;
    }

    // If we don't have a reference clock then we cannot set up the advise
    // time so we simply set the event indicating an image to render. This
    // will cause us to run flat out without any timing or synchronisation

    if (hr == S_OK)
    {
        EXECUTE_ASSERT(SetEvent((HANDLE)m_RenderEvent));
        return TRUE;
    }

    ASSERT(m_dwAdvise == 0);
    ASSERT(m_pClock);
    ASSERT(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)m_RenderEvent, 0));

    // We do have a valid reference clock interface so we can ask it to
    // set an event when the image comes due for rendering. We pass in
    // the reference time we were told to start at and also the current
    // stream time which is the offset from the start reference time

    hr = m_pClock->AdviseTime((REFERENCE_TIME)m_tStart,      // Start run time
                              StartSample,                   // Stream time
                              (HEVENT)(HANDLE)m_RenderEvent, // Render notification
                              &m_dwAdvise);                  // Advise cookie

    if (SUCCEEDED(hr))
    {
        return TRUE;
    }

    // We could not schedule the next sample for rendering despite the fact
    // we have a valid sample here. This is a fair indication that either
    // the system clock is wrong or the time stamp for the sample is duff

    ASSERT(m_dwAdvise == 0);
    return FALSE;
}

// This is called when a sample comes due for rendering. We pass the sample
// on to the derived class. After rendering we will initialise the timer for
// the next sample, NOTE signal that the last one fired first, if we don't
// do this it thinks there is still one outstanding that hasn't completed

HRESULT CBaseRenderer::Render(IMediaSample *pMediaSample)
{
    // If the media sample is NULL then we will have been notified by the
    // clock that another sample is ready but in the mean time someone has
    // stopped us streaming which causes the next sample to be released

    if (pMediaSample == NULL)
    {
        return S_FALSE;
    }

    // If we have stopped streaming then don't render any more samples, the
    // thread that got in and locked us and then reset this flag does not
    // clear the pending sample as we can use it to refresh any output device

    if (m_bStreaming == FALSE)
    {
        return S_FALSE;
    }

    // Time how long the rendering takes

    OnRenderStart(pMediaSample);
    DoRenderSample(pMediaSample);
    OnRenderEnd(pMediaSample);

    return NOERROR;
}

// Checks if there is a sample waiting at the renderer

BOOL CBaseRenderer::HaveCurrentSample()
{
    CAutoLock cRendererLock(&m_RendererLock);
    return (m_pMediaSample == NULL ? FALSE : TRUE);
}

// Returns the current sample waiting at the video renderer. We AddRef the
// sample before returning so that should it come due for rendering the
// person who called this method will hold the remaining reference count
// that will stop the sample being added back onto the allocator free list

IMediaSample *CBaseRenderer::GetCurrentSample()
{
    CAutoLock cRendererLock(&m_RendererLock);
    if (m_pMediaSample)
    {
        m_pMediaSample->AddRef();
    }
    return m_pMediaSample;
}

// Called when the source delivers us a sample. We go through a few checks to
// make sure the sample can be rendered. If we are running (streaming) then we
// have the sample scheduled with the reference clock, if we are not streaming
// then we have received an sample in paused mode so we can complete any state
// transition. On leaving this function everything will be unlocked so an app
// thread may get in and change our state to stopped (for example) in which
// case it will also signal the thread event so that our wait call is stopped

HRESULT CBaseRenderer::PrepareReceive(IMediaSample *pMediaSample)
{
    CAutoLock cInterfaceLock(&m_InterfaceLock);
    m_bInReceive = TRUE;

    // Check our flushing and filter state

    // This function must hold the interface lock because it calls
    // CBaseInputPin::Receive() and CBaseInputPin::Receive() uses
    // CBasePin::m_bRunTimeError.
    HRESULT hr = m_pInputPin->CBaseInputPin::Receive(pMediaSample);

    if (hr != NOERROR)
    {
        m_bInReceive = FALSE;
        return E_FAIL;
    }

    // Has the type changed on a media sample. We do all rendering
    // synchronously on the source thread, which has a side effect
    // that only one buffer is ever outstanding. Therefore when we
    // have Receive called we can go ahead and change the format
    // Since the format change can cause a SendMessage we just don't
    // lock
    if (m_pInputPin->SampleProps()->pMediaType)
    {
        hr = m_pInputPin->SetMediaType((CMediaType *)m_pInputPin->SampleProps()->pMediaType);
        if (FAILED(hr))
        {
            m_bInReceive = FALSE;
            return hr;
        }
    }

    CAutoLock cSampleLock(&m_RendererLock);

    ASSERT(IsActive() == TRUE);
    ASSERT(m_pInputPin->IsFlushing() == FALSE);
    ASSERT(m_pInputPin->IsConnected() == TRUE);
    ASSERT(m_pMediaSample == NULL);

    // Return an error if we already have a sample waiting for rendering
    // source pins must serialise the Receive calls - we also check that
    // no data is being sent after the source signalled an end of stream

    if (m_pMediaSample || m_bEOS || m_bAbort)
    {
        Ready();
        m_bInReceive = FALSE;
        return E_UNEXPECTED;
    }

    // Store the media times from this sample
    if (m_pPosition)
        m_pPosition->RegisterMediaTime(pMediaSample);

    // Schedule the next sample if we are streaming

    if ((m_bStreaming == TRUE) && (ScheduleSample(pMediaSample) == FALSE))
    {
        ASSERT(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)m_RenderEvent, 0));
        ASSERT(CancelNotification() == S_FALSE);
        m_bInReceive = FALSE;
        return VFW_E_SAMPLE_REJECTED;
    }

    // Store the sample end time for EC_COMPLETE handling
    m_SignalTime = m_pInputPin->SampleProps()->tStop;

    // BEWARE we sometimes keep the sample even after returning the thread to
    // the source filter such as when we go into a stopped state (we keep it
    // to refresh the device with) so we must AddRef it to keep it safely. If
    // we start flushing the source thread is released and any sample waiting
    // will be released otherwise GetBuffer may never return (see BeginFlush)

    m_pMediaSample = pMediaSample;
    m_pMediaSample->AddRef();

    if (m_bStreaming == FALSE)
    {
        SetRepaintStatus(TRUE);
    }
    return NOERROR;
}

// Called by the source filter when we have a sample to render. Under normal
// circumstances we set an advise link with the clock, wait for the time to
// arrive and then render the data using the PURE virtual DoRenderSample that
// the derived class will have overriden. After rendering the sample we may
// also signal EOS if it was the last one sent before EndOfStream was called

HRESULT CBaseRenderer::Receive(IMediaSample *pSample)
{
    ASSERT(pSample);

    // It may return VFW_E_SAMPLE_REJECTED code to say don't bother

    HRESULT hr = PrepareReceive(pSample);
    ASSERT(m_bInReceive == SUCCEEDED(hr));
    if (FAILED(hr))
    {
        if (hr == VFW_E_SAMPLE_REJECTED)
        {
            return NOERROR;
        }
        return hr;
    }

    // We realize the palette in "PrepareRender()" so we have to give away the
    // filter lock here.
    if (m_State == State_Paused)
    {
        PrepareRender();
        // no need to use InterlockedExchange
        m_bInReceive = FALSE;
        {
            // We must hold both these locks
            CAutoLock cRendererLock(&m_InterfaceLock);
            if (m_State == State_Stopped)
                return NOERROR;

            m_bInReceive = TRUE;
            CAutoLock cSampleLock(&m_RendererLock);
            OnReceiveFirstSample(pSample);
        }
        Ready();
    }
    // Having set an advise link with the clock we sit and wait. We may be
    // awoken by the clock firing or by a state change. The rendering call
    // will lock the critical section and check we can still render the data

    hr = WaitForRenderTime();
    if (FAILED(hr))
    {
        m_bInReceive = FALSE;
        return NOERROR;
    }

    PrepareRender();

    //  Set this here and poll it until we work out the locking correctly
    //  It can't be right that the streaming stuff grabs the interface
    //  lock - after all we want to be able to wait for this stuff
    //  to complete
    m_bInReceive = FALSE;

    // We must hold both these locks
    CAutoLock cRendererLock(&m_InterfaceLock);

    // since we gave away the filter wide lock, the sate of the filter could
    // have chnaged to Stopped
    if (m_State == State_Stopped)
        return NOERROR;

    CAutoLock cSampleLock(&m_RendererLock);

    // Deal with this sample

    Render(m_pMediaSample);
    ClearPendingSample();
    SendEndOfStream();
    CancelNotification();
    return NOERROR;
}

// This is called when we stop or are inactivated to clear the pending sample
// We release the media sample interface so that they can be allocated to the
// source filter again, unless of course we are changing state to inactive in
// which case GetBuffer will return an error. We must also reset the current
// media sample to NULL so that we know we do not currently have an image

HRESULT CBaseRenderer::ClearPendingSample()
{
    CAutoLock cRendererLock(&m_RendererLock);
    if (m_pMediaSample)
    {
        m_pMediaSample->Release();
        m_pMediaSample = NULL;
    }
    return NOERROR;
}

// Used to signal end of stream according to the sample end time

void CALLBACK EndOfStreamTimer(UINT uID,         // Timer identifier
                               UINT uMsg,        // Not currently used
                               DWORD_PTR dwUser, // User information
                               DWORD_PTR dw1,    // Windows reserved
                               DWORD_PTR dw2)    // is also reserved
{
    CBaseRenderer *pRenderer = (CBaseRenderer *)dwUser;
    NOTE1("EndOfStreamTimer called (%d)", uID);
    pRenderer->TimerCallback();
}

//  Do the timer callback work
void CBaseRenderer::TimerCallback()
{
    //  Lock for synchronization (but don't hold this lock when calling
    //  timeKillEvent)
    CAutoLock cRendererLock(&m_RendererLock);

    // See if we should signal end of stream now

    if (m_EndOfStreamTimer)
    {
        m_EndOfStreamTimer = 0;
        SendEndOfStream();
    }
}

// If we are at the end of the stream signal the filter graph but do not set
// the state flag back to FALSE. Once we drop off the end of the stream we
// leave the flag set (until a subsequent ResetEndOfStream). Each sample we
// get delivered will update m_SignalTime to be the last sample's end time.
// We must wait this long before signalling end of stream to the filtergraph

#define TIMEOUT_DELIVERYWAIT 50
#define TIMEOUT_RESOLUTION 10

HRESULT CBaseRenderer::SendEndOfStream()
{
    ASSERT(CritCheckIn(&m_RendererLock));
    if (m_bEOS == FALSE || m_bEOSDelivered || m_EndOfStreamTimer)
    {
        return NOERROR;
    }

    // If there is no clock then signal immediately
    if (m_pClock == NULL)
    {
        return NotifyEndOfStream();
    }

    // How long into the future is the delivery time

    REFERENCE_TIME Signal = m_tStart + m_SignalTime;
    REFERENCE_TIME CurrentTime;
    m_pClock->GetTime(&CurrentTime);
    LONG Delay = LONG((Signal - CurrentTime) / 10000);

    // Dump the timing information to the debugger

    NOTE1("Delay until end of stream delivery %d", Delay);
    NOTE1("Current %s", (LPCTSTR)CDisp((LONGLONG)CurrentTime));
    NOTE1("Signal %s", (LPCTSTR)CDisp((LONGLONG)Signal));

    // Wait for the delivery time to arrive

    if (Delay < TIMEOUT_DELIVERYWAIT)
    {
        return NotifyEndOfStream();
    }

    // Signal a timer callback on another worker thread

    m_EndOfStreamTimer = CompatibleTimeSetEvent((UINT)Delay,        // Period of timer
                                                TIMEOUT_RESOLUTION, // Timer resolution
                                                EndOfStreamTimer,   // Callback function
                                                DWORD_PTR(this),    // Used information
                                                TIME_ONESHOT);      // Type of callback
    if (m_EndOfStreamTimer == 0)
    {
        return NotifyEndOfStream();
    }
    return NOERROR;
}

// Signals EC_COMPLETE to the filtergraph manager

HRESULT CBaseRenderer::NotifyEndOfStream()
{
    CAutoLock cRendererLock(&m_RendererLock);
    ASSERT(m_bEOSDelivered == FALSE);
    ASSERT(m_EndOfStreamTimer == 0);

    // Has the filter changed state

    if (m_bStreaming == FALSE)
    {
        ASSERT(m_EndOfStreamTimer == 0);
        return NOERROR;
    }

    // Reset the end of stream timer
    m_EndOfStreamTimer = 0;

    // If we've been using the IMediaPosition interface, set it's start
    // and end media "times" to the stop position by hand.  This ensures
    // that we actually get to the end, even if the MPEG guestimate has
    // been bad or if the quality management dropped the last few frames

    if (m_pPosition)
        m_pPosition->EOS();
    m_bEOSDelivered = TRUE;
    NOTE("Sending EC_COMPLETE...");
    return NotifyEvent(EC_COMPLETE, S_OK, (LONG_PTR)(IBaseFilter *)this);
}

// Reset the end of stream flag, this is typically called when we transfer to
// stopped states since that resets the current position back to the start so
// we will receive more samples or another EndOfStream if there aren't any. We
// keep two separate flags one to say we have run off the end of the stream
// (this is the m_bEOS flag) and another to say we have delivered EC_COMPLETE
// to the filter graph. We need the latter otherwise we can end up sending an
// EC_COMPLETE every time the source changes state and calls our EndOfStream

HRESULT CBaseRenderer::ResetEndOfStream()
{
    ResetEndOfStreamTimer();
    CAutoLock cRendererLock(&m_RendererLock);

    m_bEOS = FALSE;
    m_bEOSDelivered = FALSE;
    m_SignalTime = 0;

    return NOERROR;
}

// Kills any outstanding end of stream timer

void CBaseRenderer::ResetEndOfStreamTimer()
{
    ASSERT(CritCheckOut(&m_RendererLock));
    if (m_EndOfStreamTimer)
    {
        timeKillEvent(m_EndOfStreamTimer);
        m_EndOfStreamTimer = 0;
    }
}

// This is called when we start running so that we can schedule any pending
// image we have with the clock and display any timing information. If we
// don't have any sample but we have queued an EOS flag then we send it. If
// we do have a sample then we wait until that has been rendered before we
// signal the filter graph otherwise we may change state before it's done

HRESULT CBaseRenderer::StartStreaming()
{
    CAutoLock cRendererLock(&m_RendererLock);
    if (m_bStreaming == TRUE)
    {
        return NOERROR;
    }

    // Reset the streaming times ready for running

    m_bStreaming = TRUE;

    timeBeginPeriod(1);
    OnStartStreaming();

    // There should be no outstanding advise
    ASSERT(WAIT_TIMEOUT == WaitForSingleObject((HANDLE)m_RenderEvent, 0));
    ASSERT(CancelNotification() == S_FALSE);

    // If we have an EOS and no data then deliver it now

    if (m_pMediaSample == NULL)
    {
        return SendEndOfStream();
    }

    // Have the data rendered

    ASSERT(m_pMediaSample);
    if (!ScheduleSample(m_pMediaSample))
        m_RenderEvent.Set();

    return NOERROR;
}

// This is called when we stop streaming so that we can set our internal flag
// indicating we are not now to schedule any more samples arriving. The state
// change methods in the filter implementation take care of cancelling any
// clock advise link we have set up and clearing any pending sample we have

HRESULT CBaseRenderer::StopStreaming()
{
    CAutoLock cRendererLock(&m_RendererLock);
    m_bEOSDelivered = FALSE;

    if (m_bStreaming == TRUE)
    {
        m_bStreaming = FALSE;
        OnStopStreaming();
        timeEndPeriod(1);
    }
    return NOERROR;
}

// We have a boolean flag that is reset when we have signalled EC_REPAINT to
// the filter graph. We set this when we receive an image so that should any
// conditions arise again we can send another one. By having a flag we ensure
// we don't flood the filter graph with redundant calls. We do not set the
// event when we receive an EndOfStream call since there is no point in us
// sending further EC_REPAINTs. In particular the AutoShowWindow method and
// the DirectDraw object use this method to control the window repainting

void CBaseRenderer::SetRepaintStatus(BOOL bRepaint)
{
    CAutoLock cSampleLock(&m_RendererLock);
    m_bRepaintStatus = bRepaint;
}

// Pass the window handle to the upstream filter

void CBaseRenderer::SendNotifyWindow(IPin *pPin, HWND hwnd)
{
    IMediaEventSink *pSink;

    // Does the pin support IMediaEventSink
    HRESULT hr = pPin->QueryInterface(IID_IMediaEventSink, (void **)&pSink);
    if (SUCCEEDED(hr))
    {
        pSink->Notify(EC_NOTIFY_WINDOW, LONG_PTR(hwnd), 0);
        pSink->Release();
    }
    NotifyEvent(EC_NOTIFY_WINDOW, LONG_PTR(hwnd), 0);
}

// Signal an EC_REPAINT to the filter graph. This can be used to have data
// sent to us. For example when a video window is first displayed it may
// not have an image to display, at which point it signals EC_REPAINT. The
// filtergraph will either pause the graph if stopped or if already paused
// it will call put_CurrentPosition of the current position. Setting the
// current position to itself has the stream flushed and the image resent

#define RLOG(_x_) DbgLog((LOG_TRACE, 1, TEXT(_x_)));

void CBaseRenderer::SendRepaint()
{
    CAutoLock cSampleLock(&m_RendererLock);
    ASSERT(m_pInputPin);

    // We should not send repaint notifications when...
    //    - An end of stream has been notified
    //    - Our input pin is being flushed
    //    - The input pin is not connected
    //    - We have aborted a video playback
    //    - There is a repaint already sent

    if (m_bAbort == FALSE)
    {
        if (m_pInputPin->IsConnected() == TRUE)
        {
            if (m_pInputPin->IsFlushing() == FALSE)
            {
                if (IsEndOfStream() == FALSE)
                {
                    if (m_bRepaintStatus == TRUE)
                    {
                        IPin *pPin = (IPin *)m_pInputPin;
                        NotifyEvent(EC_REPAINT, (LONG_PTR)pPin, 0);
                        SetRepaintStatus(FALSE);
                        RLOG("Sending repaint");
                    }
                }
            }
        }
    }
}

// When a video window detects a display change (WM_DISPLAYCHANGE message) it
// can send an EC_DISPLAY_CHANGED event code along with the renderer pin. The
// filtergraph will stop everyone and reconnect our input pin. As we're then
// reconnected we can accept the media type that matches the new display mode
// since we may no longer be able to draw the current image type efficiently

BOOL CBaseRenderer::OnDisplayChange()
{
    // Ignore if we are not connected yet

    CAutoLock cSampleLock(&m_RendererLock);
    if (m_pInputPin->IsConnected() == FALSE)
    {
        return FALSE;
    }

    RLOG("Notification of EC_DISPLAY_CHANGE");

    // Pass our input pin as parameter on the event

    IPin *pPin = (IPin *)m_pInputPin;
    m_pInputPin->AddRef();
    NotifyEvent(EC_DISPLAY_CHANGED, (LONG_PTR)pPin, 0);
    SetAbortSignal(TRUE);
    ClearPendingSample();
    m_pInputPin->Release();

    return TRUE;
}

// Called just before we start drawing.
// Store the current time in m_trRenderStart to allow the rendering time to be
// logged.  Log the time stamp of the sample and how late it is (neg is early)

void CBaseRenderer::OnRenderStart(IMediaSample *pMediaSample)
{
#ifdef PERF
    REFERENCE_TIME trStart, trEnd;
    pMediaSample->GetTime(&trStart, &trEnd);

    MSR_INTEGER(m_idBaseStamp, (int)trStart); // dump low order 32 bits

    m_pClock->GetTime(&m_trRenderStart);
    MSR_INTEGER(0, (int)m_trRenderStart);
    REFERENCE_TIME trStream;
    trStream = m_trRenderStart - m_tStart; // convert reftime to stream time
    MSR_INTEGER(0, (int)trStream);

    const int trLate = (int)(trStream - trStart);
    MSR_INTEGER(m_idBaseAccuracy, trLate / 10000); // dump in mSec
#endif

} // OnRenderStart

// Called directly after drawing an image.
// calculate the time spent drawing and log it.

void CBaseRenderer::OnRenderEnd(IMediaSample *pMediaSample)
{
#ifdef PERF
    REFERENCE_TIME trNow;
    m_pClock->GetTime(&trNow);
    MSR_INTEGER(0, (int)trNow);
    int t = (int)((trNow - m_trRenderStart) / 10000); // convert UNITS->msec
    MSR_INTEGER(m_idBaseRenderTime, t);
#endif
} // OnRenderEnd

// Constructor must be passed the base renderer object

CRendererInputPin::CRendererInputPin(__inout CBaseRenderer *pRenderer, __inout HRESULT *phr, __in_opt LPCWSTR pPinName)
    : CBaseInputPin(NAME("Renderer pin"), pRenderer, &pRenderer->m_InterfaceLock, (HRESULT *)phr, pPinName)
{
    m_pRenderer = pRenderer;
    ASSERT(m_pRenderer);
}

// Signals end of data stream on the input pin

STDMETHODIMP CRendererInputPin::EndOfStream()
{
    CAutoLock cRendererLock(&m_pRenderer->m_InterfaceLock);
    CAutoLock cSampleLock(&m_pRenderer->m_RendererLock);

    // Make sure we're streaming ok

    HRESULT hr = CheckStreaming();
    if (hr != NOERROR)
    {
        return hr;
    }

    // Pass it onto the renderer

    hr = m_pRenderer->EndOfStream();
    if (SUCCEEDED(hr))
    {
        hr = CBaseInputPin::EndOfStream();
    }
    return hr;
}

// Signals start of flushing on the input pin - we do the final reset end of
// stream with the renderer lock unlocked but with the interface lock locked
// We must do this because we call timeKillEvent, our timer callback method
// has to take the renderer lock to serialise our state. Therefore holding a
// renderer lock when calling timeKillEvent could cause a deadlock condition

STDMETHODIMP CRendererInputPin::BeginFlush()
{
    CAutoLock cRendererLock(&m_pRenderer->m_InterfaceLock);
    {
        CAutoLock cSampleLock(&m_pRenderer->m_RendererLock);
        CBaseInputPin::BeginFlush();
        m_pRenderer->BeginFlush();
    }
    return m_pRenderer->ResetEndOfStream();
}

// Signals end of flushing on the input pin

STDMETHODIMP CRendererInputPin::EndFlush()
{
    CAutoLock cRendererLock(&m_pRenderer->m_InterfaceLock);
    CAutoLock cSampleLock(&m_pRenderer->m_RendererLock);

    HRESULT hr = m_pRenderer->EndFlush();
    if (SUCCEEDED(hr))
    {
        hr = CBaseInputPin::EndFlush();
    }
    return hr;
}

// Pass the sample straight through to the renderer object

STDMETHODIMP CRendererInputPin::Receive(IMediaSample *pSample)
{
    HRESULT hr = m_pRenderer->Receive(pSample);
    if (FAILED(hr))
    {

        // A deadlock could occur if the caller holds the renderer lock and
        // attempts to acquire the interface lock.
        ASSERT(CritCheckOut(&m_pRenderer->m_RendererLock));

        {
            // The interface lock must be held when the filter is calling
            // IsStopped() or IsFlushing().  The interface lock must also
            // be held because the function uses m_bRunTimeError.
            CAutoLock cRendererLock(&m_pRenderer->m_InterfaceLock);

            // We do not report errors which occur while the filter is stopping,
            // flushing or if the m_bAbort flag is set .  Errors are expected to
            // occur during these operations and the streaming thread correctly
            // handles the errors.
            if (!IsStopped() && !IsFlushing() && !m_pRenderer->m_bAbort && !m_bRunTimeError)
            {

                // EC_ERRORABORT's first parameter is the error which caused
                // the event and its' last parameter is 0.  See the Direct
                // Show SDK documentation for more information.
                m_pRenderer->NotifyEvent(EC_ERRORABORT, hr, 0);

                {
                    CAutoLock alRendererLock(&m_pRenderer->m_RendererLock);
                    if (m_pRenderer->IsStreaming() && !m_pRenderer->IsEndOfStreamDelivered())
                    {
                        m_pRenderer->NotifyEndOfStream();
                    }
                }

                m_bRunTimeError = TRUE;
            }
        }
    }

    return hr;
}

// Called when the input pin is disconnected

HRESULT CRendererInputPin::BreakConnect()
{
    HRESULT hr = m_pRenderer->BreakConnect();
    if (FAILED(hr))
    {
        return hr;
    }
    return CBaseInputPin::BreakConnect();
}

// Called when the input pin is connected

HRESULT CRendererInputPin::CompleteConnect(IPin *pReceivePin)
{
    HRESULT hr = m_pRenderer->CompleteConnect(pReceivePin);
    if (FAILED(hr))
    {
        return hr;
    }
    return CBaseInputPin::CompleteConnect(pReceivePin);
}

// Give the pin id of our one and only pin

STDMETHODIMP CRendererInputPin::QueryId(__deref_out LPWSTR *Id)
{
    CheckPointer(Id, E_POINTER);

    const WCHAR szIn[] = L"In";

    *Id = (LPWSTR)CoTaskMemAlloc(sizeof(szIn));
    if (*Id == NULL)
    {
        return E_OUTOFMEMORY;
    }
    CopyMemory(*Id, szIn, sizeof(szIn));
    return NOERROR;
}

// Will the filter accept this media type

HRESULT CRendererInputPin::CheckMediaType(const CMediaType *pmt)
{
    return m_pRenderer->CheckMediaType(pmt);
}

// Called when we go paused or running

HRESULT CRendererInputPin::Active()
{
    return m_pRenderer->Active();
}

// Called when we go into a stopped state

HRESULT CRendererInputPin::Inactive()
{
    // The caller must hold the interface lock because
    // this function uses m_bRunTimeError.
    ASSERT(CritCheckIn(&m_pRenderer->m_InterfaceLock));

    m_bRunTimeError = FALSE;

    return m_pRenderer->Inactive();
}

// Tell derived classes about the media type agreed

HRESULT CRendererInputPin::SetMediaType(const CMediaType *pmt)
{
    HRESULT hr = CBaseInputPin::SetMediaType(pmt);
    if (FAILED(hr))
    {
        return hr;
    }
    return m_pRenderer->SetMediaType(pmt);
}

// We do not keep an event object to use when setting up a timer link with
// the clock but are given a pointer to one by the owning object through the
// SetNotificationObject method - this must be initialised before starting
// We can override the default quality management process to have it always
// draw late frames, this is currently done by having the following registry
// key (actually an INI key) called DrawLateFrames set to 1 (default is 0)

const TCHAR AMQUALITY[] = TEXT("ActiveMovie");
const TCHAR DRAWLATEFRAMES[] = TEXT("DrawLateFrames");

CBaseVideoRenderer::CBaseVideoRenderer(REFCLSID RenderClass,       // CLSID for this renderer
                                       __in_opt LPCTSTR pName,     // Debug ONLY description
                                       __inout_opt LPUNKNOWN pUnk, // Aggregated owner object
                                       __inout HRESULT *phr)
    : // General OLE return code

    CBaseRenderer(RenderClass, pName, pUnk, phr)
    , m_cFramesDropped(0)
    , m_cFramesDrawn(0)
    , m_bSupplierHandlingQuality(FALSE)
{
    ResetStreamingTimes();

#ifdef PERF
    m_idTimeStamp = MSR_REGISTER(TEXT("Frame time stamp"));
    m_idEarliness = MSR_REGISTER(TEXT("Earliness fudge"));
    m_idTarget = MSR_REGISTER(TEXT("Target (mSec)"));
    m_idSchLateTime = MSR_REGISTER(TEXT("mSec late when scheduled"));
    m_idDecision = MSR_REGISTER(TEXT("Scheduler decision code"));
    m_idQualityRate = MSR_REGISTER(TEXT("Quality rate sent"));
    m_idQualityTime = MSR_REGISTER(TEXT("Quality time sent"));
    m_idWaitReal = MSR_REGISTER(TEXT("Render wait"));
    // m_idWait            = MSR_REGISTER(TEXT("wait time recorded (msec)"));
    m_idFrameAccuracy = MSR_REGISTER(TEXT("Frame accuracy (msecs)"));
    m_bDrawLateFrames = GetProfileInt(AMQUALITY, DRAWLATEFRAMES, FALSE);
    // m_idSendQuality      = MSR_REGISTER(TEXT("Processing Quality message"));

    m_idRenderAvg = MSR_REGISTER(TEXT("Render draw time Avg"));
    m_idFrameAvg = MSR_REGISTER(TEXT("FrameAvg"));
    m_idWaitAvg = MSR_REGISTER(TEXT("WaitAvg"));
    m_idDuration = MSR_REGISTER(TEXT("Duration"));
    m_idThrottle = MSR_REGISTER(TEXT("Audio-video throttle wait"));
    // m_idDebug           = MSR_REGISTER(TEXT("Debug stuff"));
#endif // PERF
} // Constructor

// Destructor is just a placeholder

CBaseVideoRenderer::~CBaseVideoRenderer()
{
    ASSERT(m_dwAdvise == 0);
}

// The timing functions in this class are called by the window object and by
// the renderer's allocator.
// The windows object calls timing functions as it receives media sample
// images for drawing using GDI.
// The allocator calls timing functions when it starts passing DCI/DirectDraw
// surfaces which are not rendered in the same way; The decompressor writes
// directly to the surface with no separate rendering, so those code paths
// call direct into us.  Since we only ever hand out DCI/DirectDraw surfaces
// when we have allocated one and only one image we know there cannot be any
// conflict between the two.
//
// We use timeGetTime to return the timing counts we use (since it's relative
// performance we are interested in rather than absolute compared to a clock)
// The window object sets the accuracy of the system clock (normally 1ms) by
// calling timeBeginPeriod/timeEndPeriod when it changes streaming states

// Reset all times controlling streaming.
// Set them so that
// 1. Frames will not initially be dropped
// 2. The first frame will definitely be drawn (achieved by saying that there
//    has not ben a frame drawn for a long time).

HRESULT CBaseVideoRenderer::ResetStreamingTimes()
{
    m_trLastDraw = -1000; // set up as first frame since ages (1 sec) ago
    m_tStreamingStart = timeGetTime();
    m_trRenderAvg = 0;
    m_trFrameAvg = -1; // -1000 fps == "unset"
    m_trDuration = 0;  // 0 - strange value
    m_trRenderLast = 0;
    m_trWaitAvg = 0;
    m_tRenderStart = 0;
    m_cFramesDrawn = 0;
    m_cFramesDropped = 0;
    m_iTotAcc = 0;
    m_iSumSqAcc = 0;
    m_iSumSqFrameTime = 0;
    m_trFrame = 0; // hygeine - not really needed
    m_trLate = 0;  // hygeine - not really needed
    m_iSumFrameTime = 0;
    m_nNormal = 0;
    m_trEarliness = 0;
    m_trTarget = -300000; // 30mSec early
    m_trThrottle = 0;
    m_trRememberStampForPerf = 0;

#ifdef PERF
    m_trRememberFrameForPerf = 0;
#endif

    return NOERROR;
} // ResetStreamingTimes

// Reset all times controlling streaming. Note that we're now streaming. We
// don't need to set the rendering event to have the source filter released
// as it is done during the Run processing. When we are run we immediately
// release the source filter thread and draw any image waiting (that image
// may already have been drawn once as a poster frame while we were paused)

HRESULT CBaseVideoRenderer::OnStartStreaming()
{
    ResetStreamingTimes();
    return NOERROR;
} // OnStartStreaming

// Called at end of streaming.  Fixes times for property page report

HRESULT CBaseVideoRenderer::OnStopStreaming()
{
    m_tStreamingStart = timeGetTime() - m_tStreamingStart;
    return NOERROR;
} // OnStopStreaming

// Called when we start waiting for a rendering event.
// Used to update times spent waiting and not waiting.

void CBaseVideoRenderer::OnWaitStart()
{
    MSR_START(m_idWaitReal);
} // OnWaitStart

// Called when we are awoken from the wait in the window OR by our allocator
// when it is hanging around until the next sample is due for rendering on a
// DCI/DirectDraw surface. We add the wait time into our rolling average.
// We grab the interface lock so that we're serialised with the application
// thread going through the run code - which in due course ends up calling
// ResetStreaming times - possibly as we run through this section of code

void CBaseVideoRenderer::OnWaitEnd()
{
#ifdef PERF
    MSR_STOP(m_idWaitReal);
    // for a perf build we want to know just exactly how late we REALLY are.
    // even if this means that we have to look at the clock again.

    REFERENCE_TIME trRealStream; // the real time now expressed as stream time.
#if 0
    m_pClock->GetTime(&trRealStream); // Calling clock here causes W95 deadlock!
#else
    // We will be discarding overflows like mad here!
    // This is wrong really because timeGetTime() can wrap but it's
    // only for PERF
    REFERENCE_TIME tr = timeGetTime() * 10000;
    trRealStream = tr + m_llTimeOffset;
#endif
    trRealStream -= m_tStart; // convert to stream time (this is a reftime)

    if (m_trRememberStampForPerf == 0)
    {
        // This is probably the poster frame at the start, and it is not scheduled
        // in the usual way at all.  Just count it.  The rememberstamp gets set
        // in ShouldDrawSampleNow, so this does invalid frame recording until we
        // actually start playing.
        PreparePerformanceData(0, 0);
    }
    else
    {
        int trLate = (int)(trRealStream - m_trRememberStampForPerf);
        int trFrame = (int)(tr - m_trRememberFrameForPerf);
        PreparePerformanceData(trLate, trFrame);
    }
    m_trRememberFrameForPerf = tr;
#endif // PERF
} // OnWaitEnd

// Put data on one side that describes the lateness of the current frame.
// We don't yet know whether it will actually be drawn.  In direct draw mode,
// this decision is up to the filter upstream, and it could change its mind.
// The rules say that if it did draw it must call Receive().  One way or
// another we eventually get into either OnRenderStart or OnDirectRender and
// these both call RecordFrameLateness to update the statistics.

void CBaseVideoRenderer::PreparePerformanceData(int trLate, int trFrame)
{
    m_trLate = trLate;
    m_trFrame = trFrame;
} // PreparePerformanceData

// update the statistics:
// m_iTotAcc, m_iSumSqAcc, m_iSumSqFrameTime, m_iSumFrameTime, m_cFramesDrawn
// Note that because the properties page reports using these variables,
// 1. We need to be inside a critical section
// 2. They must all be updated together.  Updating the sums here and the count
// elsewhere can result in imaginary jitter (i.e. attempts to find square roots
// of negative numbers) in the property page code.

void CBaseVideoRenderer::RecordFrameLateness(int trLate, int trFrame)
{
    // Record how timely we are.
    int tLate = trLate / 10000;

    // Best estimate of moment of appearing on the screen is average of
    // start and end draw times.  Here we have only the end time.  This may
    // tend to show us as spuriously late by up to 1/2 frame rate achieved.
    // Decoder probably monitors draw time.  We don't bother.
    MSR_INTEGER(m_idFrameAccuracy, tLate);

    // This is a kludge - we can get frames that are very late
    // especially (at start-up) and they invalidate the statistics.
    // So ignore things that are more than 1 sec off.
    if (tLate > 1000 || tLate < -1000)
    {
        if (m_cFramesDrawn <= 1)
        {
            tLate = 0;
        }
        else if (tLate > 0)
        {
            tLate = 1000;
        }
        else
        {
            tLate = -1000;
        }
    }
    // The very first frame often has a invalid time, so don't
    // count it into the statistics.   (???)
    if (m_cFramesDrawn > 1)
    {
        m_iTotAcc += tLate;
        m_iSumSqAcc += (tLate * tLate);
    }

    // calculate inter-frame time.  Doesn't make sense for first frame
    // second frame suffers from invalid first frame stamp.
    if (m_cFramesDrawn > 2)
    {
        int tFrame = trFrame / 10000; // convert to mSec else it overflows

        // This is a kludge.  It can overflow anyway (a pause can cause
        // a very long inter-frame time) and it overflows at 2**31/10**7
        // or about 215 seconds i.e. 3min 35sec
        if (tFrame > 1000 || tFrame < 0)
            tFrame = 1000;
        m_iSumSqFrameTime += tFrame * tFrame;
        ASSERT(m_iSumSqFrameTime >= 0);
        m_iSumFrameTime += tFrame;
    }
    ++m_cFramesDrawn;

} // RecordFrameLateness

void CBaseVideoRenderer::ThrottleWait()
{
    if (m_trThrottle > 0)
    {
        int iThrottle = m_trThrottle / 10000; // convert to mSec
        MSR_INTEGER(m_idThrottle, iThrottle);
        DbgLog((LOG_TRACE, 0, TEXT("Throttle %d ms"), iThrottle));
        Sleep(iThrottle);
    }
    else
    {
        Sleep(0);
    }
} // ThrottleWait

// Whenever a frame is rendered it goes though either OnRenderStart
// or OnDirectRender.  Data that are generated during ShouldDrawSample
// are added to the statistics by calling RecordFrameLateness from both
// these two places.

// Called in place of OnRenderStart..OnRenderEnd
// When a DirectDraw image is drawn
void CBaseVideoRenderer::OnDirectRender(IMediaSample *pMediaSample)
{
    m_trRenderAvg = 0;
    m_trRenderLast = 5000000; // If we mode switch, we do NOT want this
                              // to inhibit the new average getting going!
                              // so we set it to half a second
    // MSR_INTEGER(m_idRenderAvg, m_trRenderAvg/10000);
    RecordFrameLateness(m_trLate, m_trFrame);
    ThrottleWait();
} // OnDirectRender

// Called just before we start drawing.  All we do is to get the current clock
// time (from the system) and return.  We have to store the start render time
// in a member variable because it isn't used until we complete the drawing
// The rest is just performance logging.

void CBaseVideoRenderer::OnRenderStart(IMediaSample *pMediaSample)
{
    RecordFrameLateness(m_trLate, m_trFrame);
    m_tRenderStart = timeGetTime();
} // OnRenderStart

// Called directly after drawing an image.  We calculate the time spent in the
// drawing code and if this doesn't appear to have any odd looking spikes in
// it then we add it to the current average draw time.  Measurement spikes may
// occur if the drawing thread is interrupted and switched to somewhere else.

void CBaseVideoRenderer::OnRenderEnd(IMediaSample *pMediaSample)
{
    // The renderer time can vary erratically if we are interrupted so we do
    // some smoothing to help get more sensible figures out but even that is
    // not enough as figures can go 9,10,9,9,83,9 and we must disregard 83

    int tr = (timeGetTime() - m_tRenderStart) * 10000; // convert mSec->UNITS
    if (tr < m_trRenderAvg * 2 || tr < 2 * m_trRenderLast)
    {
        // DO_MOVING_AVG(m_trRenderAvg, tr);
        m_trRenderAvg = (tr + (AVGPERIOD - 1) * m_trRenderAvg) / AVGPERIOD;
    }
    m_trRenderLast = tr;
    ThrottleWait();
} // OnRenderEnd

STDMETHODIMP CBaseVideoRenderer::SetSink(IQualityControl *piqc)
{

    m_pQSink = piqc;

    return NOERROR;
} // SetSink

STDMETHODIMP CBaseVideoRenderer::Notify(IBaseFilter *pSelf, Quality q)
{
    // NOTE:  We are NOT getting any locks here.  We could be called
    // asynchronously and possibly even on a time critical thread of
    // someone else's - so we do the minumum.  We only set one state
    // variable (an integer) and if that happens to be in the middle
    // of another thread reading it they will just get either the new
    // or the old value.  Locking would achieve no more than this.

    // It might be nice to check that we are being called from m_pGraph, but
    // it turns out to be a millisecond or so per throw!

    // This is heuristics, these numbers are aimed at being "what works"
    // rather than anything based on some theory.
    // We use a hyperbola because it's easy to calculate and it includes
    // a panic button asymptote (which we push off just to the left)
    // The throttling fits the following table (roughly)
    // Proportion   Throttle (msec)
    //     >=1000         0
    //        900         3
    //        800         7
    //        700        11
    //        600        17
    //        500        25
    //        400        35
    //        300        50
    //        200        72
    //        125       100
    //        100       112
    //         50       146
    //          0       200

    // (some evidence that we could go for a sharper kink - e.g. no throttling
    // until below the 750 mark - might give fractionally more frames on a
    // P60-ish machine).  The easy way to get these coefficients is to use
    // Renbase.xls follow the instructions therein using excel solver.

    if (q.Proportion >= 1000)
    {
        m_trThrottle = 0;
    }
    else
    {
        // The DWORD is to make quite sure I get unsigned arithmetic
        // as the constant is between 2**31 and 2**32
        m_trThrottle = -330000 + (388880000 / (q.Proportion + 167));
    }
    return NOERROR;
} // Notify

// Send a message to indicate what our supplier should do about quality.
// Theory:
// What a supplier wants to know is "is the frame I'm working on NOW
// going to be late?".
// F1 is the frame at the supplier (as above)
// Tf1 is the due time for F1
// T1 is the time at that point (NOW!)
// Tr1 is the time that f1 WILL actually be rendered
// L1 is the latency of the graph for frame F1 = Tr1-T1
// D1 (for delay) is how late F1 will be beyond its due time i.e.
// D1 = (Tr1-Tf1) which is what the supplier really wants to know.
// Unfortunately Tr1 is in the future and is unknown, so is L1
//
// We could estimate L1 by its value for a previous frame,
// L0 = Tr0-T0 and work off
// D1' = ((T1+L0)-Tf1) = (T1 + (Tr0-T0) -Tf1)
// Rearranging terms:
// D1' = (T1-T0) + (Tr0-Tf1)
//       adding (Tf0-Tf0) and rearranging again:
//     = (T1-T0) + (Tr0-Tf0) + (Tf0-Tf1)
//     = (T1-T0) - (Tf1-Tf0) + (Tr0-Tf0)
// But (Tr0-Tf0) is just D0 - how late frame zero was, and this is the
// Late field in the quality message that we send.
// The other two terms just state what correction should be applied before
// using the lateness of F0 to predict the lateness of F1.
// (T1-T0) says how much time has actually passed (we have lost this much)
// (Tf1-Tf0) says how much time should have passed if we were keeping pace
// (we have gained this much).
//
// Suppliers should therefore work off:
//    Quality.Late + (T1-T0)  - (Tf1-Tf0)
// and see if this is "acceptably late" or even early (i.e. negative).
// They get T1 and T0 by polling the clock, they get Tf1 and Tf0 from
// the time stamps in the frames.  They get Quality.Late from us.
//

HRESULT CBaseVideoRenderer::SendQuality(REFERENCE_TIME trLate, REFERENCE_TIME trRealStream)
{
    Quality q;
    HRESULT hr;

    // If we are the main user of time, then report this as Flood/Dry.
    // If our suppliers are, then report it as Famine/Glut.
    //
    // We need to take action, but avoid hunting.  Hunting is caused by
    // 1. Taking too much action too soon and overshooting
    // 2. Taking too long to react (so averaging can CAUSE hunting).
    //
    // The reason why we use trLate as well as Wait is to reduce hunting;
    // if the wait time is coming down and about to go into the red, we do
    // NOT want to rely on some average which is only telling is that it used
    // to be OK once.

    q.TimeStamp = (REFERENCE_TIME)trRealStream;

    if (m_trFrameAvg < 0)
    {
        q.Type = Famine; // guess
    }
    // Is the greater part of the time taken bltting or something else
    else if (m_trFrameAvg > 2 * m_trRenderAvg)
    {
        q.Type = Famine; // mainly other
    }
    else
    {
        q.Type = Flood; // mainly bltting
    }

    q.Proportion = 1000; // default

    if (m_trFrameAvg < 0)
    {
        // leave it alone - we don't know enough
    }
    else if (trLate > 0)
    {
        // try to catch up over the next second
        // We could be Really, REALLY late, but rendering all the frames
        // anyway, just because it's so cheap.

        q.Proportion = 1000 - (int)((trLate) / (UNITS / 1000));
        if (q.Proportion < 500)
        {
            q.Proportion = 500; // don't go daft. (could've been negative!)
        }
        else
        {
        }
    }
    else if (m_trWaitAvg > 20000 && trLate < -20000)
    {
        // Go cautiously faster - aim at 2mSec wait.
        if (m_trWaitAvg >= m_trFrameAvg)
        {
            // This can happen because of some fudges.
            // The waitAvg is how long we originally planned to wait
            // The frameAvg is more honest.
            // It means that we are spending a LOT of time waiting
            q.Proportion = 2000; // double.
        }
        else
        {
            if (m_trFrameAvg + 20000 > m_trWaitAvg)
            {
                q.Proportion = 1000 * (m_trFrameAvg / (m_trFrameAvg + 20000 - m_trWaitAvg));
            }
            else
            {
                // We're apparently spending more than the whole frame time waiting.
                // Assume that the averages are slightly out of kilter, but that we
                // are indeed doing a lot of waiting.  (This leg probably never
                // happens, but the code avoids any potential divide by zero).
                q.Proportion = 2000;
            }
        }

        if (q.Proportion > 2000)
        {
            q.Proportion = 2000; // don't go crazy.
        }
    }

    // Tell the supplier how late frames are when they get rendered
    // That's how late we are now.
    // If we are in directdraw mode then the guy upstream can see the drawing
    // times and we'll just report on the start time.  He can figure out any
    // offset to apply.  If we are in DIB Section mode then we will apply an
    // extra offset which is half of our drawing time.  This is usually small
    // but can sometimes be the dominant effect.  For this we will use the
    // average drawing time rather than the last frame.  If the last frame took
    // a long time to draw and made us late, that's already in the lateness
    // figure.  We should not add it in again unless we expect the next frame
    // to be the same.  We don't, we expect the average to be a better shot.
    // In direct draw mode the RenderAvg will be zero.

    q.Late = trLate + m_trRenderAvg / 2;

    // log what we're doing
    MSR_INTEGER(m_idQualityRate, q.Proportion);
    MSR_INTEGER(m_idQualityTime, (int)q.Late / 10000);

    // A specific sink interface may be set through IPin

    if (m_pQSink == NULL)
    {
        // Get our input pin's peer.  We send quality management messages
        // to any nominated receiver of these things (set in the IPin
        // interface), or else to our source filter.

        IQualityControl *pQC = NULL;
        IPin *pOutputPin = m_pInputPin->GetConnected();
        ASSERT(pOutputPin != NULL);

        // And get an AddRef'd quality control interface

        hr = pOutputPin->QueryInterface(IID_IQualityControl, (void **)&pQC);
        if (SUCCEEDED(hr))
        {
            m_pQSink = pQC;
        }
    }
    if (m_pQSink)
    {
        return m_pQSink->Notify(this, q);
    }

    return S_FALSE;

} // SendQuality

// We are called with a valid IMediaSample image to decide whether this is to
// be drawn or not.  There must be a reference clock in operation.
// Return S_OK if it is to be drawn Now (as soon as possible)
// Return S_FALSE if it is to be drawn when it's due
// Return an error if we want to drop it
// m_nNormal=-1 indicates that we dropped the previous frame and so this
// one should be drawn early.  Respect it and update it.
// Use current stream time plus a number of heuristics (detailed below)
// to make the decision

HRESULT CBaseVideoRenderer::ShouldDrawSampleNow(IMediaSample *pMediaSample, __inout REFERENCE_TIME *ptrStart,
                                                __inout REFERENCE_TIME *ptrEnd)
{

    // Don't call us unless there's a clock interface to synchronise with
    ASSERT(m_pClock);

    MSR_INTEGER(m_idTimeStamp, (int)((*ptrStart) >> 32)); // high order 32 bits
    MSR_INTEGER(m_idTimeStamp, (int)(*ptrStart));         // low order 32 bits

    // We lose a bit of time depending on the monitor type waiting for the next
    // screen refresh.  On average this might be about 8mSec - so it will be
    // later than we think when the picture appears.  To compensate a bit
    // we bias the media samples by -8mSec i.e. 80000 UNITs.
    // We don't ever make a stream time negative (call it paranoia)
    if (*ptrStart >= 80000)
    {
        *ptrStart -= 80000;
        *ptrEnd -= 80000; // bias stop to to retain valid frame duration
    }

    // Cache the time stamp now.  We will want to compare what we did with what
    // we started with (after making the monitor allowance).
    m_trRememberStampForPerf = *ptrStart;

    // Get reference times (current and late)
    REFERENCE_TIME trRealStream; // the real time now expressed as stream time.
    m_pClock->GetTime(&trRealStream);
#ifdef PERF
    // While the reference clock is expensive:
    // Remember the offset from timeGetTime and use that.
    // This overflows all over the place, but when we subtract to get
    // differences the overflows all cancel out.
    m_llTimeOffset = trRealStream - timeGetTime() * 10000;
#endif
    trRealStream -= m_tStart; // convert to stream time (this is a reftime)

    // We have to wory about two versions of "lateness".  The truth, which we
    // try to work out here and the one measured against m_trTarget which
    // includes long term feedback.  We report statistics against the truth
    // but for operational decisions we work to the target.
    // We use TimeDiff to make sure we get an integer because we
    // may actually be late (or more likely early if there is a big time
    // gap) by a very long time.
    const int trTrueLate = TimeDiff(trRealStream - *ptrStart);
    const int trLate = trTrueLate;

    MSR_INTEGER(m_idSchLateTime, trTrueLate / 10000);

    // Send quality control messages upstream, measured against target
    HRESULT hr = SendQuality(trLate, trRealStream);
    // Note: the filter upstream is allowed to this FAIL meaning "you do it".
    m_bSupplierHandlingQuality = (hr == S_OK);

    // Decision time!  Do we drop, draw when ready or draw immediately?

    const int trDuration = (int)(*ptrEnd - *ptrStart);
    {
        // We need to see if the frame rate of the file has just changed.
        // This would make comparing our previous frame rate with the current
        // frame rate inefficent.  Hang on a moment though.  I've seen files
        // where the frames vary between 33 and 34 mSec so as to average
        // 30fps.  A minor variation like that won't hurt us.
        int t = m_trDuration / 32;
        if (trDuration > m_trDuration + t || trDuration < m_trDuration - t)
        {
            // There's a major variation.  Reset the average frame rate to
            // exactly the current rate to disable decision 9002 for this frame,
            // and remember the new rate.
            m_trFrameAvg = trDuration;
            m_trDuration = trDuration;
        }
    }

    MSR_INTEGER(m_idEarliness, m_trEarliness / 10000);
    MSR_INTEGER(m_idRenderAvg, m_trRenderAvg / 10000);
    MSR_INTEGER(m_idFrameAvg, m_trFrameAvg / 10000);
    MSR_INTEGER(m_idWaitAvg, m_trWaitAvg / 10000);
    MSR_INTEGER(m_idDuration, trDuration / 10000);

#ifdef PERF
    if (S_OK == pMediaSample->IsDiscontinuity())
    {
        MSR_INTEGER(m_idDecision, 9000);
    }
#endif

    // Control the graceful slide back from slow to fast machine mode.
    // After a frame drop accept an early frame and set the earliness to here
    // If this frame is already later than the earliness then slide it to here
    // otherwise do the standard slide (reduce by about 12% per frame).
    // Note: earliness is normally NEGATIVE
    BOOL bJustDroppedFrame = (m_bSupplierHandlingQuality
                              //  Can't use the pin sample properties because we might
                              //  not be in Receive when we call this
                              && (S_OK == pMediaSample->IsDiscontinuity()) // he just dropped one
                              ) ||
                             (m_nNormal == -1); // we just dropped one

    // Set m_trEarliness (slide back from slow to fast machine mode)
    if (trLate > 0)
    {
        m_trEarliness = 0; // we are no longer in fast machine mode at all!
    }
    else if ((trLate >= m_trEarliness) || bJustDroppedFrame)
    {
        m_trEarliness = trLate; // Things have slipped of their own accord
    }
    else
    {
        m_trEarliness = m_trEarliness - m_trEarliness / 8; // graceful slide
    }

    // prepare the new wait average - but don't pollute the old one until
    // we have finished with it.
    int trWaitAvg;
    {
        // We never mix in a negative wait.  This causes us to believe in fast machines
        // slightly more.
        int trL = trLate < 0 ? -trLate : 0;
        trWaitAvg = (trL + m_trWaitAvg * (AVGPERIOD - 1)) / AVGPERIOD;
    }

    int trFrame;
    {
        REFERENCE_TIME tr = trRealStream - m_trLastDraw; // Cd be large - 4 min pause!
        if (tr > 10000000)
        {
            tr = 10000000; // 1 second - arbitrarily.
        }
        trFrame = int(tr);
    }

    // We will DRAW this frame IF...
    if (
        // ...the time we are spending drawing is a small fraction of the total
        // observed inter-frame time so that dropping it won't help much.
        (3 * m_trRenderAvg <= m_trFrameAvg)

        // ...or our supplier is NOT handling things and the next frame would
        // be less timely than this one or our supplier CLAIMS to be handling
        // things, and is now less than a full FOUR frames late.
        || (m_bSupplierHandlingQuality ? (trLate <= trDuration * 4) : (trLate + trLate < trDuration))

        // ...or we are on average waiting for over eight milliseconds then
        // this may be just a glitch.  Draw it and we'll hope to catch up.
        || (m_trWaitAvg > 80000)

        // ...or we haven't drawn an image for over a second.  We will update
        // the display, which stops the video looking hung.
        // Do this regardless of how late this media sample is.
        || ((trRealStream - m_trLastDraw) > UNITS)

    )
    {
        HRESULT Result;

        // We are going to play this frame.  We may want to play it early.
        // We will play it early if we think we are in slow machine mode.
        // If we think we are NOT in slow machine mode, we will still play
        // it early by m_trEarliness as this controls the graceful slide back.
        // and in addition we aim at being m_trTarget late rather than "on time".

        BOOL bPlayASAP = FALSE;

        // we will play it AT ONCE (slow machine mode) if...

        // ...we are playing catch-up
        if (bJustDroppedFrame)
        {
            bPlayASAP = TRUE;
            MSR_INTEGER(m_idDecision, 9001);
        }

        // ...or if we are running below the true frame rate
        // exact comparisons are glitchy, for these measurements,
        // so add an extra 5% or so
        else if ((m_trFrameAvg > trDuration + trDuration / 16)

                 // It's possible to get into a state where we are losing ground, but
                 // are a very long way ahead.  To avoid this or recover from it
                 // we refuse to play early by more than 10 frames.
                 && (trLate > -trDuration * 10))
        {
            bPlayASAP = TRUE;
            MSR_INTEGER(m_idDecision, 9002);
        }
#if 0
            // ...or if we have been late and are less than one frame early
        else if (  (trLate + trDuration > 0)
                && (m_trWaitAvg<=20000)
                ) {
            bPlayASAP = TRUE;
            MSR_INTEGER(m_idDecision, 9003);
        }
#endif
        // We will NOT play it at once if we are grossly early.  On very slow frame
        // rate movies - e.g. clock.avi - it is not a good idea to leap ahead just
        // because we got starved (for instance by the net) and dropped one frame
        // some time or other.  If we are more than 900mSec early, then wait.
        if (trLate < -9000000)
        {
            bPlayASAP = FALSE;
        }

        if (bPlayASAP)
        {

            m_nNormal = 0;
            MSR_INTEGER(m_idDecision, 0);
            // When we are here, we are in slow-machine mode.  trLate may well
            // oscillate between negative and positive when the supplier is
            // dropping frames to keep sync.  We should not let that mislead
            // us into thinking that we have as much as zero spare time!
            // We just update with a zero wait.
            m_trWaitAvg = (m_trWaitAvg * (AVGPERIOD - 1)) / AVGPERIOD;

            // Assume that we draw it immediately.  Update inter-frame stats
            m_trFrameAvg = (trFrame + m_trFrameAvg * (AVGPERIOD - 1)) / AVGPERIOD;
#ifndef PERF
            // If this is NOT a perf build, then report what we know so far
            // without looking at the clock any more.  This assumes that we
            // actually wait for exactly the time we hope to.  It also reports
            // how close we get to the manipulated time stamps that we now have
            // rather than the ones we originally started with.  It will
            // therefore be a little optimistic.  However it's fast.
            PreparePerformanceData(trTrueLate, trFrame);
#endif
            m_trLastDraw = trRealStream;
            if (m_trEarliness > trLate)
            {
                m_trEarliness = trLate; // if we are actually early, this is neg
            }
            Result = S_OK; // Draw it now
        }
        else
        {
            ++m_nNormal;
            // Set the average frame rate to EXACTLY the ideal rate.
            // If we are exiting slow-machine mode then we will have caught up
            // and be running ahead, so as we slide back to exact timing we will
            // have a longer than usual gap at this point.  If we record this
            // real gap then we'll think that we're running slow and go back
            // into slow-machine mode and vever get it straight.
            m_trFrameAvg = trDuration;
            MSR_INTEGER(m_idDecision, 1);

            // Play it early by m_trEarliness and by m_trTarget

            {
                int trE = m_trEarliness;
                if (trE < -m_trFrameAvg)
                {
                    trE = -m_trFrameAvg;
                }
                *ptrStart += trE; // N.B. earliness is negative
            }

            int Delay = -trTrueLate;
            Result = Delay <= 0 ? S_OK : S_FALSE; // OK = draw now, FALSE = wait

            m_trWaitAvg = trWaitAvg;

            // Predict when it will actually be drawn and update frame stats

            if (Result == S_FALSE)
            { // We are going to wait
                trFrame = TimeDiff(*ptrStart - m_trLastDraw);
                m_trLastDraw = *ptrStart;
            }
            else
            {
                // trFrame is already = trRealStream-m_trLastDraw;
                m_trLastDraw = trRealStream;
            }
#ifndef PERF
            int iAccuracy;
            if (Delay > 0)
            {
                // Report lateness based on when we intend to play it
                iAccuracy = TimeDiff(*ptrStart - m_trRememberStampForPerf);
            }
            else
            {
                // Report lateness based on playing it *now*.
                iAccuracy = trTrueLate; // trRealStream-RememberStampForPerf;
            }
            PreparePerformanceData(iAccuracy, trFrame);
#endif
        }
        return Result;
    }

    // We are going to drop this frame!
    // Of course in DirectDraw mode the guy upstream may draw it anyway.

    // This will probably give a large negative wack to the wait avg.
    m_trWaitAvg = trWaitAvg;

#ifdef PERF
    // Respect registry setting - debug only!
    if (m_bDrawLateFrames)
    {
        return S_OK; // draw it when it's ready
    }                // even though it's late.
#endif

    // We are going to drop this frame so draw the next one early
    // n.b. if the supplier is doing direct draw then he may draw it anyway
    // but he's doing something funny to arrive here in that case.

    MSR_INTEGER(m_idDecision, 2);
    m_nNormal = -1;
    return E_FAIL; // drop it

} // ShouldDrawSampleNow

// NOTE we're called by both the window thread and the source filter thread
// so we have to be protected by a critical section (locked before called)
// Also, when the window thread gets signalled to render an image, it always
// does so regardless of how late it is. All the degradation is done when we
// are scheduling the next sample to be drawn. Hence when we start an advise
// link to draw a sample, that sample's time will always become the last one
// drawn - unless of course we stop streaming in which case we cancel links

BOOL CBaseVideoRenderer::ScheduleSample(IMediaSample *pMediaSample)
{
    // We override ShouldDrawSampleNow to add quality management

    BOOL bDrawImage = CBaseRenderer::ScheduleSample(pMediaSample);
    if (bDrawImage == FALSE)
    {
        ++m_cFramesDropped;
        return FALSE;
    }

    // m_cFramesDrawn must NOT be updated here.  It has to be updated
    // in RecordFrameLateness at the same time as the other statistics.
    return TRUE;
}

// Implementation of IQualProp interface needed to support the property page
// This is how the property page gets the data out of the scheduler. We are
// passed into the constructor the owning object in the COM sense, this will
// either be the video renderer or an external IUnknown if we're aggregated.
// We initialise our CUnknown base class with this interface pointer. Then
// all we have to do is to override NonDelegatingQueryInterface to expose
// our IQualProp interface. The AddRef and Release are handled automatically
// by the base class and will be passed on to the appropriate outer object

STDMETHODIMP CBaseVideoRenderer::get_FramesDroppedInRenderer(__out int *pcFramesDropped)
{
    CheckPointer(pcFramesDropped, E_POINTER);
    CAutoLock cVideoLock(&m_InterfaceLock);
    *pcFramesDropped = m_cFramesDropped;
    return NOERROR;
} // get_FramesDroppedInRenderer

// Set *pcFramesDrawn to the number of frames drawn since
// streaming started.

STDMETHODIMP CBaseVideoRenderer::get_FramesDrawn(int *pcFramesDrawn)
{
    CheckPointer(pcFramesDrawn, E_POINTER);
    CAutoLock cVideoLock(&m_InterfaceLock);
    *pcFramesDrawn = m_cFramesDrawn;
    return NOERROR;
} // get_FramesDrawn

// Set iAvgFrameRate to the frames per hundred secs since
// streaming started.  0 otherwise.

STDMETHODIMP CBaseVideoRenderer::get_AvgFrameRate(int *piAvgFrameRate)
{
    CheckPointer(piAvgFrameRate, E_POINTER);
    CAutoLock cVideoLock(&m_InterfaceLock);

    int t;
    if (m_bStreaming)
    {
        t = timeGetTime() - m_tStreamingStart;
    }
    else
    {
        t = m_tStreamingStart;
    }

    if (t <= 0)
    {
        *piAvgFrameRate = 0;
        ASSERT(m_cFramesDrawn == 0);
    }
    else
    {
        // i is frames per hundred seconds
        *piAvgFrameRate = MulDiv(100000, m_cFramesDrawn, t);
    }
    return NOERROR;
} // get_AvgFrameRate

// Set *piAvg to the average sync offset since streaming started
// in mSec.  The sync offset is the time in mSec between when the frame
// should have been drawn and when the frame was actually drawn.

STDMETHODIMP CBaseVideoRenderer::get_AvgSyncOffset(__out int *piAvg)
{
    CheckPointer(piAvg, E_POINTER);
    CAutoLock cVideoLock(&m_InterfaceLock);

    if (NULL == m_pClock)
    {
        *piAvg = 0;
        return NOERROR;
    }

    // Note that we didn't gather the stats on the first frame
    // so we use m_cFramesDrawn-1 here
    if (m_cFramesDrawn <= 1)
    {
        *piAvg = 0;
    }
    else
    {
        *piAvg = (int)(m_iTotAcc / (m_cFramesDrawn - 1));
    }
    return NOERROR;
} // get_AvgSyncOffset

// To avoid dragging in the maths library - a cheap
// approximate integer square root.
// We do this by getting a starting guess which is between 1
// and 2 times too large, followed by THREE iterations of
// Newton Raphson.  (That will give accuracy to the nearest mSec
// for the range in question - roughly 0..1000)
//
// It would be faster to use a linear interpolation and ONE NR, but
// who cares.  If anyone does - the best linear interpolation is
// to approximates sqrt(x) by
// y = x * (sqrt(2)-1) + 1 - 1/sqrt(2) + 1/(8*(sqrt(2)-1))
// 0r y = x*0.41421 + 0.59467
// This minimises the maximal error in the range in question.
// (error is about +0.008883 and then one NR will give error .0000something
// (Of course these are integers, so you can't just multiply by 0.41421
// you'd have to do some sort of MulDiv).
// Anyone wanna check my maths?  (This is only for a property display!)

int isqrt(int x)
{
    int s = 1;
    // Make s an initial guess for sqrt(x)
    if (x > 0x40000000)
    {
        s = 0x8000; // prevent any conceivable closed loop
    }
    else
    {
        while (s * s < x)
        {              // loop cannot possible go more than 31 times
            s = 2 * s; // normally it goes about 6 times
        }
        // Three NR iterations.
        if (x == 0)
        {
            s = 0; // Wouldn't it be tragic to divide by zero whenever our
                   // accuracy was perfect!
        }
        else
        {
            s = (s * s + x) / (2 * s);
            if (s >= 0)
                s = (s * s + x) / (2 * s);
            if (s >= 0)
                s = (s * s + x) / (2 * s);
        }
    }
    return s;
}

//
//  Do estimates for standard deviations for per-frame
//  statistics
//
HRESULT CBaseVideoRenderer::GetStdDev(int nSamples, __out int *piResult, LONGLONG llSumSq, LONGLONG iTot)
{
    CheckPointer(piResult, E_POINTER);
    CAutoLock cVideoLock(&m_InterfaceLock);

    if (NULL == m_pClock)
    {
        *piResult = 0;
        return NOERROR;
    }

    // If S is the Sum of the Squares of observations and
    //    T the Total (i.e. sum) of the observations and there were
    //    N observations, then an estimate of the standard deviation is
    //      sqrt( (S - T**2/N) / (N-1) )

    if (nSamples <= 1)
    {
        *piResult = 0;
    }
    else
    {
        LONGLONG x;
        // First frames have invalid stamps, so we get no stats for them
        // So we need 2 frames to get 1 datum, so N is cFramesDrawn-1

        // so we use m_cFramesDrawn-1 here
        x = llSumSq - llMulDiv(iTot, iTot, nSamples, 0);
        x = x / (nSamples - 1);
        ASSERT(x >= 0);
        *piResult = isqrt((LONG)x);
    }
    return NOERROR;
}

// Set *piDev to the standard deviation in mSec of the sync offset
// of each frame since streaming started.

STDMETHODIMP CBaseVideoRenderer::get_DevSyncOffset(__out int *piDev)
{
    // First frames have invalid stamps, so we get no stats for them
    // So we need 2 frames to get 1 datum, so N is cFramesDrawn-1
    return GetStdDev(m_cFramesDrawn - 1, piDev, m_iSumSqAcc, m_iTotAcc);
} // get_DevSyncOffset

// Set *piJitter to the standard deviation in mSec of the inter-frame time
// of frames since streaming started.

STDMETHODIMP CBaseVideoRenderer::get_Jitter(__out int *piJitter)
{
    // First frames have invalid stamps, so we get no stats for them
    // So second frame gives invalid inter-frame time
    // So we need 3 frames to get 1 datum, so N is cFramesDrawn-2
    return GetStdDev(m_cFramesDrawn - 2, piJitter, m_iSumSqFrameTime, m_iSumFrameTime);
} // get_Jitter

// Overidden to return our IQualProp interface

STDMETHODIMP
CBaseVideoRenderer::NonDelegatingQueryInterface(REFIID riid, __deref_out VOID **ppv)
{
    // We return IQualProp and delegate everything else

    if (riid == IID_IQualProp)
    {
        return GetInterface((IQualProp *)this, ppv);
    }
    else if (riid == IID_IQualityControl)
    {
        return GetInterface((IQualityControl *)this, ppv);
    }
    return CBaseRenderer::NonDelegatingQueryInterface(riid, ppv);
}

// Override JoinFilterGraph so that, just before leaving
// the graph we can send an EC_WINDOW_DESTROYED event

STDMETHODIMP
CBaseVideoRenderer::JoinFilterGraph(__inout_opt IFilterGraph *pGraph, __in_opt LPCWSTR pName)
{
    // Since we send EC_ACTIVATE, we also need to ensure
    // we send EC_WINDOW_DESTROYED or the resource manager may be
    // holding us as a focus object
    if (!pGraph && m_pGraph)
    {

        // We were in a graph and now we're not
        // Do this properly in case we are aggregated
        IBaseFilter *pFilter = this;
        NotifyEvent(EC_WINDOW_DESTROYED, (LPARAM)pFilter, 0);
    }
    return CBaseFilter::JoinFilterGraph(pGraph, pName);
}

// This removes a large number of level 4 warnings from the
// Microsoft compiler which in this case are not very useful
#pragma warning(disable : 4514)
