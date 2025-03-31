//------------------------------------------------------------------------------
// File: Transfrm.cpp
//
// Desc: DirectShow base classes - implements class for simple transform
//       filters such as video decompressors.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <measure.h>

// =================================================================
// Implements the CTransformFilter class
// =================================================================

CTransformFilter::CTransformFilter(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, REFCLSID clsid)
    : CBaseFilter(pName, pUnk, &m_csFilter, clsid)
    , m_pInput(NULL)
    , m_pOutput(NULL)
    , m_bEOSDelivered(FALSE)
    , m_bQualityChanged(FALSE)
    , m_bSampleSkipped(FALSE)
{
#ifdef PERF
    RegisterPerfId();
#endif //  PERF
}

#ifdef UNICODE
CTransformFilter::CTransformFilter(__in_opt LPCSTR pName, __inout_opt LPUNKNOWN pUnk, REFCLSID clsid)
    : CBaseFilter(pName, pUnk, &m_csFilter, clsid)
    , m_pInput(NULL)
    , m_pOutput(NULL)
    , m_bEOSDelivered(FALSE)
    , m_bQualityChanged(FALSE)
    , m_bSampleSkipped(FALSE)
{
#ifdef PERF
    RegisterPerfId();
#endif //  PERF
}
#endif

// destructor

CTransformFilter::~CTransformFilter()
{
    // Delete the pins

    delete m_pInput;
    delete m_pOutput;
}

// Transform place holder - should never be called
HRESULT CTransformFilter::Transform(IMediaSample *pIn, IMediaSample *pOut)
{
    UNREFERENCED_PARAMETER(pIn);
    UNREFERENCED_PARAMETER(pOut);
    DbgBreak("CTransformFilter::Transform() should never be called");
    return E_UNEXPECTED;
}

// return the number of pins we provide

int CTransformFilter::GetPinCount()
{
    return 2;
}

// return a non-addrefed CBasePin * for the user to addref if he holds onto it
// for longer than his pointer to us. We create the pins dynamically when they
// are asked for rather than in the constructor. This is because we want to
// give the derived class an oppportunity to return different pin objects

// We return the objects as and when they are needed. If either of these fails
// then we return NULL, the assumption being that the caller will realise the
// whole deal is off and destroy us - which in turn will delete everything.

CBasePin *CTransformFilter::GetPin(int n)
{
    HRESULT hr = S_OK;

    // Create an input pin if necessary

    if (m_pInput == NULL)
    {

        m_pInput = new CTransformInputPin(NAME("Transform input pin"),
                                          this,         // Owner filter
                                          &hr,          // Result code
                                          L"XForm In"); // Pin name

        //  Can't fail
        ASSERT(SUCCEEDED(hr));
        if (m_pInput == NULL)
        {
            return NULL;
        }
        m_pOutput = (CTransformOutputPin *)new CTransformOutputPin(NAME("Transform output pin"),
                                                                   this,          // Owner filter
                                                                   &hr,           // Result code
                                                                   L"XForm Out"); // Pin name

        // Can't fail
        ASSERT(SUCCEEDED(hr));
        if (m_pOutput == NULL)
        {
            delete m_pInput;
            m_pInput = NULL;
        }
    }

    // Return the appropriate pin

    if (n == 0)
    {
        return m_pInput;
    }
    else if (n == 1)
    {
        return m_pOutput;
    }
    else
    {
        return NULL;
    }
}

//
// FindPin
//
// If Id is In or Out then return the IPin* for that pin
// creating the pin if need be.  Otherwise return NULL with an error.

STDMETHODIMP CTransformFilter::FindPin(LPCWSTR Id, __deref_out IPin **ppPin)
{
    CheckPointer(ppPin, E_POINTER);
    ValidateReadWritePtr(ppPin, sizeof(IPin *));

    if (0 == lstrcmpW(Id, L"In"))
    {
        *ppPin = GetPin(0);
    }
    else if (0 == lstrcmpW(Id, L"Out"))
    {
        *ppPin = GetPin(1);
    }
    else
    {
        *ppPin = NULL;
        return VFW_E_NOT_FOUND;
    }

    HRESULT hr = NOERROR;
    //  AddRef() returned pointer - but GetPin could fail if memory is low.
    if (*ppPin)
    {
        (*ppPin)->AddRef();
    }
    else
    {
        hr = E_OUTOFMEMORY; // probably.  There's no pin anyway.
    }
    return hr;
}

// override these two functions if you want to inform something
// about entry to or exit from streaming state.

HRESULT
CTransformFilter::StartStreaming()
{
    return NOERROR;
}

HRESULT
CTransformFilter::StopStreaming()
{
    return NOERROR;
}

// override this to grab extra interfaces on connection

HRESULT
CTransformFilter::CheckConnect(PIN_DIRECTION dir, IPin *pPin)
{
    UNREFERENCED_PARAMETER(dir);
    UNREFERENCED_PARAMETER(pPin);
    return NOERROR;
}

// place holder to allow derived classes to release any extra interfaces

HRESULT
CTransformFilter::BreakConnect(PIN_DIRECTION dir)
{
    UNREFERENCED_PARAMETER(dir);
    return NOERROR;
}

// Let derived classes know about connection completion

HRESULT
CTransformFilter::CompleteConnect(PIN_DIRECTION direction, IPin *pReceivePin)
{
    UNREFERENCED_PARAMETER(direction);
    UNREFERENCED_PARAMETER(pReceivePin);
    return NOERROR;
}

// override this to know when the media type is really set

HRESULT
CTransformFilter::SetMediaType(PIN_DIRECTION direction, const CMediaType *pmt)
{
    UNREFERENCED_PARAMETER(direction);
    UNREFERENCED_PARAMETER(pmt);
    return NOERROR;
}

// Set up our output sample
HRESULT
CTransformFilter::InitializeOutputSample(IMediaSample *pSample, __deref_out IMediaSample **ppOutSample)
{
    IMediaSample *pOutSample;

    // default - times are the same

    AM_SAMPLE2_PROPERTIES *const pProps = m_pInput->SampleProps();
    DWORD dwFlags = m_bSampleSkipped ? AM_GBF_PREVFRAMESKIPPED : 0;

    // This will prevent the image renderer from switching us to DirectDraw
    // when we can't do it without skipping frames because we're not on a
    // keyframe.  If it really has to switch us, it still will, but then we
    // will have to wait for the next keyframe
    if (!(pProps->dwSampleFlags & AM_SAMPLE_SPLICEPOINT))
    {
        dwFlags |= AM_GBF_NOTASYNCPOINT;
    }

    ASSERT(m_pOutput->m_pAllocator != NULL);
    HRESULT hr = m_pOutput->m_pAllocator->GetBuffer(
        &pOutSample, pProps->dwSampleFlags & AM_SAMPLE_TIMEVALID ? &pProps->tStart : NULL,
        pProps->dwSampleFlags & AM_SAMPLE_STOPVALID ? &pProps->tStop : NULL, dwFlags);
    *ppOutSample = pOutSample;
    if (FAILED(hr))
    {
        return hr;
    }

    ASSERT(pOutSample);
    IMediaSample2 *pOutSample2;
    if (SUCCEEDED(pOutSample->QueryInterface(IID_IMediaSample2, (void **)&pOutSample2)))
    {
        /*  Modify it */
        AM_SAMPLE2_PROPERTIES OutProps;
        EXECUTE_ASSERT(
            SUCCEEDED(pOutSample2->GetProperties(FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, tStart), (PBYTE)&OutProps)));
        OutProps.dwTypeSpecificFlags = pProps->dwTypeSpecificFlags;
        OutProps.dwSampleFlags =
            (OutProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED) | (pProps->dwSampleFlags & ~AM_SAMPLE_TYPECHANGED);
        OutProps.tStart = pProps->tStart;
        OutProps.tStop = pProps->tStop;
        OutProps.cbData = FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, dwStreamId);
        hr = pOutSample2->SetProperties(FIELD_OFFSET(AM_SAMPLE2_PROPERTIES, dwStreamId), (PBYTE)&OutProps);
        if (pProps->dwSampleFlags & AM_SAMPLE_DATADISCONTINUITY)
        {
            m_bSampleSkipped = FALSE;
        }
        pOutSample2->Release();
    }
    else
    {
        if (pProps->dwSampleFlags & AM_SAMPLE_TIMEVALID)
        {
            pOutSample->SetTime(&pProps->tStart, &pProps->tStop);
        }
        if (pProps->dwSampleFlags & AM_SAMPLE_SPLICEPOINT)
        {
            pOutSample->SetSyncPoint(TRUE);
        }
        if (pProps->dwSampleFlags & AM_SAMPLE_DATADISCONTINUITY)
        {
            pOutSample->SetDiscontinuity(TRUE);
            m_bSampleSkipped = FALSE;
        }
        // Copy the media times

        LONGLONG MediaStart, MediaEnd;
        if (pSample->GetMediaTime(&MediaStart, &MediaEnd) == NOERROR)
        {
            pOutSample->SetMediaTime(&MediaStart, &MediaEnd);
        }
    }
    return S_OK;
}

// override this to customize the transform process

HRESULT
CTransformFilter::Receive(IMediaSample *pSample)
{
    /*  Check for other streams and pass them on */
    AM_SAMPLE2_PROPERTIES *const pProps = m_pInput->SampleProps();
    if (pProps->dwStreamId != AM_STREAM_MEDIA)
    {
        return m_pOutput->m_pInputPin->Receive(pSample);
    }
    HRESULT hr;
    ASSERT(pSample);
    IMediaSample *pOutSample;

    // If no output to deliver to then no point sending us data

    ASSERT(m_pOutput != NULL);

    // Set up the output sample
    hr = InitializeOutputSample(pSample, &pOutSample);

    if (FAILED(hr))
    {
        return hr;
    }

    // Start timing the transform (if PERF is defined)
    MSR_START(m_idTransform);

    // have the derived class transform the data

    hr = Transform(pSample, pOutSample);

    // Stop the clock and log it (if PERF is defined)
    MSR_STOP(m_idTransform);

    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 1, TEXT("Error from transform")));
    }
    else
    {
        // the Transform() function can return S_FALSE to indicate that the
        // sample should not be delivered; we only deliver the sample if it's
        // really S_OK (same as NOERROR, of course.)
        if (hr == NOERROR)
        {
            hr = m_pOutput->m_pInputPin->Receive(pOutSample);
            m_bSampleSkipped = FALSE; // last thing no longer dropped
        }
        else
        {
            // S_FALSE returned from Transform is a PRIVATE agreement
            // We should return NOERROR from Receive() in this cause because returning S_FALSE
            // from Receive() means that this is the end of the stream and no more data should
            // be sent.
            if (S_FALSE == hr)
            {

                //  Release the sample before calling notify to avoid
                //  deadlocks if the sample holds a lock on the system
                //  such as DirectDraw buffers do
                pOutSample->Release();
                m_bSampleSkipped = TRUE;
                if (!m_bQualityChanged)
                {
                    NotifyEvent(EC_QUALITY_CHANGE, 0, 0);
                    m_bQualityChanged = TRUE;
                }
                return NOERROR;
            }
        }
    }

    // release the output buffer. If the connected pin still needs it,
    // it will have addrefed it itself.
    pOutSample->Release();

    return hr;
}

// Return S_FALSE to mean "pass the note on upstream"
// Return NOERROR (Same as S_OK)
// to mean "I've done something about it, don't pass it on"
HRESULT CTransformFilter::AlterQuality(Quality q)
{
    UNREFERENCED_PARAMETER(q);
    return S_FALSE;
}

// EndOfStream received. Default behaviour is to deliver straight
// downstream, since we have no queued data. If you overrode Receive
// and have queue data, then you need to handle this and deliver EOS after
// all queued data is sent
HRESULT
CTransformFilter::EndOfStream(void)
{
    HRESULT hr = NOERROR;
    if (m_pOutput != NULL)
    {
        hr = m_pOutput->DeliverEndOfStream();
    }

    return hr;
}

// enter flush state. Receives already blocked
// must override this if you have queued data or a worker thread
HRESULT
CTransformFilter::BeginFlush(void)
{
    HRESULT hr = NOERROR;
    if (m_pOutput != NULL)
    {
        // block receives -- done by caller (CBaseInputPin::BeginFlush)

        // discard queued data -- we have no queued data

        // free anyone blocked on receive - not possible in this filter

        // call downstream
        hr = m_pOutput->DeliverBeginFlush();
    }
    return hr;
}

// leave flush state. must override this if you have queued data
// or a worker thread
HRESULT
CTransformFilter::EndFlush(void)
{
    // sync with pushing thread -- we have no worker thread

    // ensure no more data to go downstream -- we have no queued data

    // call EndFlush on downstream pins
    ASSERT(m_pOutput != NULL);
    return m_pOutput->DeliverEndFlush();

    // caller (the input pin's method) will unblock Receives
}

// override these so that the derived filter can catch them

STDMETHODIMP
CTransformFilter::Stop()
{
    CAutoLock lck1(&m_csFilter);
    if (m_State == State_Stopped)
    {
        return NOERROR;
    }

    // Succeed the Stop if we are not completely connected

    ASSERT(m_pInput == NULL || m_pOutput != NULL);
    if (m_pInput == NULL || m_pInput->IsConnected() == FALSE || m_pOutput->IsConnected() == FALSE)
    {
        m_State = State_Stopped;
        m_bEOSDelivered = FALSE;
        return NOERROR;
    }

    ASSERT(m_pInput);
    ASSERT(m_pOutput);

    // decommit the input pin before locking or we can deadlock
    m_pInput->Inactive();

    // synchronize with Receive calls

    CAutoLock lck2(&m_csReceive);
    m_pOutput->Inactive();

    // allow a class derived from CTransformFilter
    // to know about starting and stopping streaming

    HRESULT hr = StopStreaming();
    if (SUCCEEDED(hr))
    {
        // complete the state transition
        m_State = State_Stopped;
        m_bEOSDelivered = FALSE;
    }
    return hr;
}

STDMETHODIMP
CTransformFilter::Pause()
{
    CAutoLock lck(&m_csFilter);
    HRESULT hr = NOERROR;

    if (m_State == State_Paused)
    {
        // (This space left deliberately blank)
    }

    // If we have no input pin or it isn't yet connected then when we are
    // asked to pause we deliver an end of stream to the downstream filter.
    // This makes sure that it doesn't sit there forever waiting for
    // samples which we cannot ever deliver without an input connection.

    else if (m_pInput == NULL || m_pInput->IsConnected() == FALSE)
    {
        if (m_pOutput && m_bEOSDelivered == FALSE)
        {
            m_pOutput->DeliverEndOfStream();
            m_bEOSDelivered = TRUE;
        }
        m_State = State_Paused;
    }

    // We may have an input connection but no output connection
    // However, if we have an input pin we do have an output pin

    else if (m_pOutput->IsConnected() == FALSE)
    {
        m_State = State_Paused;
    }

    else
    {
        if (m_State == State_Stopped)
        {
            // allow a class derived from CTransformFilter
            // to know about starting and stopping streaming
            CAutoLock lck2(&m_csReceive);
            hr = StartStreaming();
        }
        if (SUCCEEDED(hr))
        {
            hr = CBaseFilter::Pause();
        }
    }

    m_bSampleSkipped = FALSE;
    m_bQualityChanged = FALSE;
    return hr;
}

HRESULT
CTransformFilter::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    if (m_pOutput != NULL)
    {
        return m_pOutput->DeliverNewSegment(tStart, tStop, dRate);
    }
    return S_OK;
}

// Check streaming status
HRESULT
CTransformInputPin::CheckStreaming()
{
    ASSERT(m_pTransformFilter->m_pOutput != NULL);
    if (!m_pTransformFilter->m_pOutput->IsConnected())
    {
        return VFW_E_NOT_CONNECTED;
    }
    else
    {
        //  Shouldn't be able to get any data if we're not connected!
        ASSERT(IsConnected());

        //  we're flushing
        if (m_bFlushing)
        {
            return S_FALSE;
        }
        //  Don't process stuff in Stopped state
        if (IsStopped())
        {
            return VFW_E_WRONG_STATE;
        }
        if (m_bRunTimeError)
        {
            return VFW_E_RUNTIME_ERROR;
        }
        return S_OK;
    }
}

// =================================================================
// Implements the CTransformInputPin class
// =================================================================

// constructor

CTransformInputPin::CTransformInputPin(__in_opt LPCTSTR pObjectName, __inout CTransformFilter *pTransformFilter,
                                       __inout HRESULT *phr, __in_opt LPCWSTR pName)
    : CBaseInputPin(pObjectName, pTransformFilter, &pTransformFilter->m_csFilter, phr, pName)
{
    DbgLog((LOG_TRACE, 2, TEXT("CTransformInputPin::CTransformInputPin")));
    m_pTransformFilter = pTransformFilter;
}

#ifdef UNICODE
CTransformInputPin::CTransformInputPin(__in_opt LPCSTR pObjectName, __inout CTransformFilter *pTransformFilter,
                                       __inout HRESULT *phr, __in_opt LPCWSTR pName)
    : CBaseInputPin(pObjectName, pTransformFilter, &pTransformFilter->m_csFilter, phr, pName)
{
    DbgLog((LOG_TRACE, 2, TEXT("CTransformInputPin::CTransformInputPin")));
    m_pTransformFilter = pTransformFilter;
}
#endif

// provides derived filter a chance to grab extra interfaces

HRESULT
CTransformInputPin::CheckConnect(IPin *pPin)
{
    HRESULT hr = m_pTransformFilter->CheckConnect(PINDIR_INPUT, pPin);
    if (FAILED(hr))
    {
        return hr;
    }
    return CBaseInputPin::CheckConnect(pPin);
}

// provides derived filter a chance to release it's extra interfaces

HRESULT
CTransformInputPin::BreakConnect()
{
    //  Can't disconnect unless stopped
    ASSERT(IsStopped());
    m_pTransformFilter->BreakConnect(PINDIR_INPUT);
    return CBaseInputPin::BreakConnect();
}

// Let derived class know when the input pin is connected

HRESULT
CTransformInputPin::CompleteConnect(IPin *pReceivePin)
{
    HRESULT hr = m_pTransformFilter->CompleteConnect(PINDIR_INPUT, pReceivePin);
    if (FAILED(hr))
    {
        return hr;
    }
    return CBaseInputPin::CompleteConnect(pReceivePin);
}

// check that we can support a given media type

HRESULT
CTransformInputPin::CheckMediaType(const CMediaType *pmt)
{
    // Check the input type

    HRESULT hr = m_pTransformFilter->CheckInputType(pmt);
    if (S_OK != hr)
    {
        return hr;
    }

    // if the output pin is still connected, then we have
    // to check the transform not just the input format

    if ((m_pTransformFilter->m_pOutput != NULL) && (m_pTransformFilter->m_pOutput->IsConnected()))
    {
        return m_pTransformFilter->CheckTransform(pmt, &m_pTransformFilter->m_pOutput->CurrentMediaType());
    }
    else
    {
        return hr;
    }
}

// set the media type for this connection

HRESULT
CTransformInputPin::SetMediaType(const CMediaType *mtIn)
{
    // Set the base class media type (should always succeed)
    HRESULT hr = CBasePin::SetMediaType(mtIn);
    if (FAILED(hr))
    {
        return hr;
    }

    // check the transform can be done (should always succeed)
    ASSERT(SUCCEEDED(m_pTransformFilter->CheckInputType(mtIn)));

    return m_pTransformFilter->SetMediaType(PINDIR_INPUT, mtIn);
}

// =================================================================
// Implements IMemInputPin interface
// =================================================================

// provide EndOfStream that passes straight downstream
// (there is no queued data)
STDMETHODIMP
CTransformInputPin::EndOfStream(void)
{
    CAutoLock lck(&m_pTransformFilter->m_csReceive);
    HRESULT hr = CheckStreaming();
    if (S_OK == hr)
    {
        hr = m_pTransformFilter->EndOfStream();
    }
    return hr;
}

// enter flushing state. Call default handler to block Receives, then
// pass to overridable method in filter
STDMETHODIMP
CTransformInputPin::BeginFlush(void)
{
    CAutoLock lck(&m_pTransformFilter->m_csFilter);
    //  Are we actually doing anything?
    ASSERT(m_pTransformFilter->m_pOutput != NULL);
    if (!IsConnected() || !m_pTransformFilter->m_pOutput->IsConnected())
    {
        return VFW_E_NOT_CONNECTED;
    }
    HRESULT hr = CBaseInputPin::BeginFlush();
    if (FAILED(hr))
    {
        return hr;
    }

    return m_pTransformFilter->BeginFlush();
}

// leave flushing state.
// Pass to overridable method in filter, then call base class
// to unblock receives (finally)
STDMETHODIMP
CTransformInputPin::EndFlush(void)
{
    CAutoLock lck(&m_pTransformFilter->m_csFilter);
    //  Are we actually doing anything?
    ASSERT(m_pTransformFilter->m_pOutput != NULL);
    if (!IsConnected() || !m_pTransformFilter->m_pOutput->IsConnected())
    {
        return VFW_E_NOT_CONNECTED;
    }

    HRESULT hr = m_pTransformFilter->EndFlush();
    if (FAILED(hr))
    {
        return hr;
    }

    return CBaseInputPin::EndFlush();
}

// here's the next block of data from the stream.
// AddRef it yourself if you need to hold it beyond the end
// of this call.

HRESULT
CTransformInputPin::Receive(IMediaSample *pSample)
{
    HRESULT hr;
    CAutoLock lck(&m_pTransformFilter->m_csReceive);
    ASSERT(pSample);

    // check all is well with the base class
    hr = CBaseInputPin::Receive(pSample);
    if (S_OK == hr)
    {
        hr = m_pTransformFilter->Receive(pSample);
    }
    return hr;
}

// override to pass downstream
STDMETHODIMP
CTransformInputPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    //  Save the values in the pin
    CBasePin::NewSegment(tStart, tStop, dRate);
    return m_pTransformFilter->NewSegment(tStart, tStop, dRate);
}

// =================================================================
// Implements the CTransformOutputPin class
// =================================================================

// constructor

CTransformOutputPin::CTransformOutputPin(__in_opt LPCTSTR pObjectName, __inout CTransformFilter *pTransformFilter,
                                         __inout HRESULT *phr, __in_opt LPCWSTR pPinName)
    : CBaseOutputPin(pObjectName, pTransformFilter, &pTransformFilter->m_csFilter, phr, pPinName)
    , m_pPosition(NULL)
{
    DbgLog((LOG_TRACE, 2, TEXT("CTransformOutputPin::CTransformOutputPin")));
    m_pTransformFilter = pTransformFilter;
}

#ifdef UNICODE
CTransformOutputPin::CTransformOutputPin(__in_opt LPCSTR pObjectName, __inout CTransformFilter *pTransformFilter,
                                         __inout HRESULT *phr, __in_opt LPCWSTR pPinName)
    : CBaseOutputPin(pObjectName, pTransformFilter, &pTransformFilter->m_csFilter, phr, pPinName)
    , m_pPosition(NULL)
{
    DbgLog((LOG_TRACE, 2, TEXT("CTransformOutputPin::CTransformOutputPin")));
    m_pTransformFilter = pTransformFilter;
}
#endif

// destructor

CTransformOutputPin::~CTransformOutputPin()
{
    DbgLog((LOG_TRACE, 2, TEXT("CTransformOutputPin::~CTransformOutputPin")));

    if (m_pPosition)
        m_pPosition->Release();
}

// overriden to expose IMediaPosition and IMediaSeeking control interfaces

STDMETHODIMP
CTransformOutputPin::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    CheckPointer(ppv, E_POINTER);
    ValidateReadWritePtr(ppv, sizeof(PVOID));
    *ppv = NULL;

    if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking)
    {

        // we should have an input pin by now

        ASSERT(m_pTransformFilter->m_pInput != NULL);

        if (m_pPosition == NULL)
        {

            HRESULT hr = CreatePosPassThru(GetOwner(), FALSE, (IPin *)m_pTransformFilter->m_pInput, &m_pPosition);
            if (FAILED(hr))
            {
                return hr;
            }
        }
        return m_pPosition->QueryInterface(riid, ppv);
    }
    else
    {
        return CBaseOutputPin::NonDelegatingQueryInterface(riid, ppv);
    }
}

// provides derived filter a chance to grab extra interfaces

HRESULT
CTransformOutputPin::CheckConnect(IPin *pPin)
{
    // we should have an input connection first

    ASSERT(m_pTransformFilter->m_pInput != NULL);
    if ((m_pTransformFilter->m_pInput->IsConnected() == FALSE))
    {
        return E_UNEXPECTED;
    }

    HRESULT hr = m_pTransformFilter->CheckConnect(PINDIR_OUTPUT, pPin);
    if (FAILED(hr))
    {
        return hr;
    }
    return CBaseOutputPin::CheckConnect(pPin);
}

// provides derived filter a chance to release it's extra interfaces

HRESULT
CTransformOutputPin::BreakConnect()
{
    //  Can't disconnect unless stopped
    ASSERT(IsStopped());
    m_pTransformFilter->BreakConnect(PINDIR_OUTPUT);
    return CBaseOutputPin::BreakConnect();
}

// Let derived class know when the output pin is connected

HRESULT
CTransformOutputPin::CompleteConnect(IPin *pReceivePin)
{
    HRESULT hr = m_pTransformFilter->CompleteConnect(PINDIR_OUTPUT, pReceivePin);
    if (FAILED(hr))
    {
        return hr;
    }
    return CBaseOutputPin::CompleteConnect(pReceivePin);
}

// check a given transform - must have selected input type first

HRESULT
CTransformOutputPin::CheckMediaType(const CMediaType *pmtOut)
{
    // must have selected input first
    ASSERT(m_pTransformFilter->m_pInput != NULL);
    if ((m_pTransformFilter->m_pInput->IsConnected() == FALSE))
    {
        return E_INVALIDARG;
    }

    return m_pTransformFilter->CheckTransform(&m_pTransformFilter->m_pInput->CurrentMediaType(), pmtOut);
}

// called after we have agreed a media type to actually set it in which case
// we run the CheckTransform function to get the output format type again

HRESULT
CTransformOutputPin::SetMediaType(const CMediaType *pmtOut)
{
    HRESULT hr = NOERROR;
    ASSERT(m_pTransformFilter->m_pInput != NULL);

    ASSERT(m_pTransformFilter->m_pInput->CurrentMediaType().IsValid());

    // Set the base class media type (should always succeed)
    hr = CBasePin::SetMediaType(pmtOut);
    if (FAILED(hr))
    {
        return hr;
    }

#ifdef DEBUG
    if (FAILED(m_pTransformFilter->CheckTransform(&m_pTransformFilter->m_pInput->CurrentMediaType(), pmtOut)))
    {
        DbgLog((LOG_ERROR, 0, TEXT("*** This filter is accepting an output media type")));
        DbgLog((LOG_ERROR, 0, TEXT("    that it can't currently transform to.  I hope")));
        DbgLog((LOG_ERROR, 0, TEXT("    it's smart enough to reconnect its input.")));
    }
#endif

    return m_pTransformFilter->SetMediaType(PINDIR_OUTPUT, pmtOut);
}

// pass the buffer size decision through to the main transform class

HRESULT
CTransformOutputPin::DecideBufferSize(IMemAllocator *pAllocator, __inout ALLOCATOR_PROPERTIES *pProp)
{
    return m_pTransformFilter->DecideBufferSize(pAllocator, pProp);
}

// return a specific media type indexed by iPosition

HRESULT
CTransformOutputPin::GetMediaType(int iPosition, __inout CMediaType *pMediaType)
{
    ASSERT(m_pTransformFilter->m_pInput != NULL);

    //  We don't have any media types if our input is not connected

    if (m_pTransformFilter->m_pInput->IsConnected())
    {
        return m_pTransformFilter->GetMediaType(iPosition, pMediaType);
    }
    else
    {
        return VFW_S_NO_MORE_ITEMS;
    }
}

// Override this if you can do something constructive to act on the
// quality message.  Consider passing it upstream as well

// Pass the quality mesage on upstream.

STDMETHODIMP
CTransformOutputPin::Notify(IBaseFilter *pSender, Quality q)
{
    UNREFERENCED_PARAMETER(pSender);
    ValidateReadPtr(pSender, sizeof(IBaseFilter));

    // First see if we want to handle this ourselves
    HRESULT hr = m_pTransformFilter->AlterQuality(q);
    if (hr != S_FALSE)
    {
        return hr; // either S_OK or a failure
    }

    // S_FALSE means we pass the message on.
    // Find the quality sink for our input pin and send it there

    ASSERT(m_pTransformFilter->m_pInput != NULL);

    return m_pTransformFilter->m_pInput->PassNotify(q);

} // Notify

// the following removes a very large number of level 4 warnings from the microsoft
// compiler output, which are not useful at all in this case.
#pragma warning(disable : 4514)
