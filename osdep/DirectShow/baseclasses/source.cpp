//------------------------------------------------------------------------------
// File: Source.cpp
//
// Desc: DirectShow  base classes - implements CSource, which is a Quartz
//       source filter 'template.'
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

// Locking Strategy.
//
// Hold the filter critical section (m_pFilter->pStateLock()) to serialise
// access to functions. Note that, in general, this lock may be held
// by a function when the worker thread may want to hold it. Therefore
// if you wish to access shared state from the worker thread you will
// need to add another critical section object. The execption is during
// the threads processing loop, when it is safe to get the filter critical
// section from within FillBuffer().

#include <streams.h>

//
// CSource::Constructor
//
// Initialise the pin count for the filter. The user will create the pins in
// the derived class.
CSource::CSource(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN lpunk, CLSID clsid)
    : CBaseFilter(pName, lpunk, &m_cStateLock, clsid)
    , m_iPins(0)
    , m_paStreams(NULL)
{
}

CSource::CSource(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN lpunk, CLSID clsid, __inout HRESULT *phr)
    : CBaseFilter(pName, lpunk, &m_cStateLock, clsid)
    , m_iPins(0)
    , m_paStreams(NULL)
{
    UNREFERENCED_PARAMETER(phr);
}

#ifdef UNICODE
CSource::CSource(__in_opt LPCSTR pName, __inout_opt LPUNKNOWN lpunk, CLSID clsid)
    : CBaseFilter(pName, lpunk, &m_cStateLock, clsid)
    , m_iPins(0)
    , m_paStreams(NULL)
{
}

CSource::CSource(__in_opt LPCSTR pName, __inout_opt LPUNKNOWN lpunk, CLSID clsid, __inout HRESULT *phr)
    : CBaseFilter(pName, lpunk, &m_cStateLock, clsid)
    , m_iPins(0)
    , m_paStreams(NULL)
{
    UNREFERENCED_PARAMETER(phr);
}
#endif

//
// CSource::Destructor
//
CSource::~CSource()
{
    /*  Free our pins and pin array */
    while (m_iPins != 0)
    {
        // deleting the pins causes them to be removed from the array...
        delete m_paStreams[m_iPins - 1];
    }

    ASSERT(m_paStreams == NULL);
}

//
//  Add a new pin
//
HRESULT CSource::AddPin(__in CSourceStream *pStream)
{
    CAutoLock lock(&m_cStateLock);

    /*  Allocate space for this pin and the old ones */
    CSourceStream **paStreams = new CSourceStream *[m_iPins + 1];
    if (paStreams == NULL)
    {
        return E_OUTOFMEMORY;
    }
    if (m_paStreams != NULL)
    {
        CopyMemory((PVOID)paStreams, (PVOID)m_paStreams, m_iPins * sizeof(m_paStreams[0]));
        paStreams[m_iPins] = pStream;
        delete[] m_paStreams;
    }
    m_paStreams = paStreams;
    m_paStreams[m_iPins] = pStream;
    m_iPins++;
    return S_OK;
}

//
//  Remove a pin - pStream is NOT deleted
//
HRESULT CSource::RemovePin(__in CSourceStream *pStream)
{
    int i;
    for (i = 0; i < m_iPins; i++)
    {
        if (m_paStreams[i] == pStream)
        {
            if (m_iPins == 1)
            {
                delete[] m_paStreams;
                m_paStreams = NULL;
            }
            else
            {
                /*  no need to reallocate */
                while (++i < m_iPins)
                    m_paStreams[i - 1] = m_paStreams[i];
            }
            m_iPins--;
            return S_OK;
        }
    }
    return S_FALSE;
}

//
// FindPin
//
// Set *ppPin to the IPin* that has the id Id.
// or to NULL if the Id cannot be matched.
STDMETHODIMP CSource::FindPin(LPCWSTR Id, __deref_out IPin **ppPin)
{
    CheckPointer(ppPin, E_POINTER);
    ValidateReadWritePtr(ppPin, sizeof(IPin *));
    // The -1 undoes the +1 in QueryId and ensures that totally invalid
    // strings (for which WstrToInt delivers 0) give a deliver a NULL pin.
    int i = WstrToInt(Id) - 1;
    *ppPin = GetPin(i);
    if (*ppPin != NULL)
    {
        (*ppPin)->AddRef();
        return NOERROR;
    }
    else
    {
        return VFW_E_NOT_FOUND;
    }
}

//
// FindPinNumber
//
// return the number of the pin with this IPin* or -1 if none
int CSource::FindPinNumber(__in IPin *iPin)
{
    int i;
    for (i = 0; i < m_iPins; ++i)
    {
        if ((IPin *)(m_paStreams[i]) == iPin)
        {
            return i;
        }
    }
    return -1;
}

//
// GetPinCount
//
// Returns the number of pins this filter has
int CSource::GetPinCount(void)
{

    CAutoLock lock(&m_cStateLock);
    return m_iPins;
}

//
// GetPin
//
// Return a non-addref'd pointer to pin n
// needed by CBaseFilter
CBasePin *CSource::GetPin(int n)
{

    CAutoLock lock(&m_cStateLock);

    // n must be in the range 0..m_iPins-1
    // if m_iPins>n  && n>=0 it follows that m_iPins>0
    // which is what used to be checked (i.e. checking that we have a pin)
    if ((n >= 0) && (n < m_iPins))
    {

        ASSERT(m_paStreams[n]);
        return m_paStreams[n];
    }
    return NULL;
}

//

// *
// * --- CSourceStream ----
// *

//
// Set Id to point to a CoTaskMemAlloc'd
STDMETHODIMP CSourceStream::QueryId(__deref_out LPWSTR *Id)
{
    CheckPointer(Id, E_POINTER);
    ValidateReadWritePtr(Id, sizeof(LPWSTR));

    // We give the pins id's which are 1,2,...
    // FindPinNumber returns -1 for an invalid pin
    int i = 1 + m_pFilter->FindPinNumber(this);
    if (i < 1)
        return VFW_E_NOT_FOUND;
    *Id = (LPWSTR)CoTaskMemAlloc(sizeof(WCHAR) * 12);
    if (*Id == NULL)
    {
        return E_OUTOFMEMORY;
    }
    IntToWstr(i, *Id);
    return NOERROR;
}

//
// CSourceStream::Constructor
//
// increments the number of pins present on the filter
CSourceStream::CSourceStream(__in_opt LPCTSTR pObjectName, __inout HRESULT *phr, __inout CSource *ps,
                             __in_opt LPCWSTR pPinName)
    : CBaseOutputPin(pObjectName, ps, ps->pStateLock(), phr, pPinName)
    , m_pFilter(ps)
{

    *phr = m_pFilter->AddPin(this);
}

#ifdef UNICODE
CSourceStream::CSourceStream(__in_opt LPCSTR pObjectName, __inout HRESULT *phr, __inout CSource *ps,
                             __in_opt LPCWSTR pPinName)
    : CBaseOutputPin(pObjectName, ps, ps->pStateLock(), phr, pPinName)
    , m_pFilter(ps)
{

    *phr = m_pFilter->AddPin(this);
}
#endif
//
// CSourceStream::Destructor
//
// Decrements the number of pins on this filter
CSourceStream::~CSourceStream(void)
{

    m_pFilter->RemovePin(this);
}

//
// CheckMediaType
//
// Do we support this type? Provides the default support for 1 type.
HRESULT CSourceStream::CheckMediaType(const CMediaType *pMediaType)
{

    CAutoLock lock(m_pFilter->pStateLock());

    CMediaType mt;
    GetMediaType(&mt);

    if (mt == *pMediaType)
    {
        return NOERROR;
    }

    return E_FAIL;
}

//
// GetMediaType/3
//
// By default we support only one type
// iPosition indexes are 0-n
HRESULT CSourceStream::GetMediaType(int iPosition, __inout CMediaType *pMediaType)
{

    CAutoLock lock(m_pFilter->pStateLock());

    if (iPosition < 0)
    {
        return E_INVALIDARG;
    }
    if (iPosition > 0)
    {
        return VFW_S_NO_MORE_ITEMS;
    }
    return GetMediaType(pMediaType);
}

//
// Active
//
// The pin is active - start up the worker thread
HRESULT CSourceStream::Active(void)
{

    CAutoLock lock(m_pFilter->pStateLock());

    HRESULT hr;

    if (m_pFilter->IsActive())
    {
        return S_FALSE; // succeeded, but did not allocate resources (they already exist...)
    }

    // do nothing if not connected - its ok not to connect to
    // all pins of a source filter
    if (!IsConnected())
    {
        return NOERROR;
    }

    hr = CBaseOutputPin::Active();
    if (FAILED(hr))
    {
        return hr;
    }

    ASSERT(!ThreadExists());

    // start the thread
    if (!Create())
    {
        return E_FAIL;
    }

    // Tell thread to initialize. If OnThreadCreate Fails, so does this.
    hr = Init();
    if (FAILED(hr))
        return hr;

    return Pause();
}

//
// Inactive
//
// Pin is inactive - shut down the worker thread
// Waits for the worker to exit before returning.
HRESULT CSourceStream::Inactive(void)
{

    CAutoLock lock(m_pFilter->pStateLock());

    HRESULT hr;

    // do nothing if not connected - its ok not to connect to
    // all pins of a source filter
    if (!IsConnected())
    {
        return NOERROR;
    }

    // !!! need to do this before trying to stop the thread, because
    // we may be stuck waiting for our own allocator!!!

    hr = CBaseOutputPin::Inactive(); // call this first to Decommit the allocator
    if (FAILED(hr))
    {
        return hr;
    }

    if (ThreadExists())
    {
        hr = Stop();

        if (FAILED(hr))
        {
            return hr;
        }

        hr = Exit();
        if (FAILED(hr))
        {
            return hr;
        }

        Close(); // Wait for the thread to exit, then tidy up.
    }

    // hr = CBaseOutputPin::Inactive();  // call this first to Decommit the allocator
    // if (FAILED(hr)) {
    //	return hr;
    //}

    return NOERROR;
}

//
// ThreadProc
//
// When this returns the thread exits
// Return codes > 0 indicate an error occured
DWORD CSourceStream::ThreadProc(void)
{

    HRESULT hr; // the return code from calls
    Command com;

    do
    {
        com = GetRequest();
        if (com != CMD_INIT)
        {
            DbgLog((LOG_ERROR, 1, TEXT("Thread expected init command")));
            Reply((DWORD)E_UNEXPECTED);
        }
    } while (com != CMD_INIT);

    DbgLog((LOG_TRACE, 1, TEXT("CSourceStream worker thread initializing")));

    hr = OnThreadCreate(); // perform set up tasks
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 1, TEXT("CSourceStream::OnThreadCreate failed. Aborting thread.")));
        OnThreadDestroy();
        Reply(hr); // send failed return code from OnThreadCreate
        return 1;
    }

    // Initialisation suceeded
    Reply(NOERROR);

    Command cmd;
    do
    {
        cmd = GetRequest();

        switch (cmd)
        {

        case CMD_EXIT: Reply(NOERROR); break;

        case CMD_RUN:
            DbgLog((LOG_ERROR, 1, TEXT("CMD_RUN received before a CMD_PAUSE???")));
            // !!! fall through???

        case CMD_PAUSE:
            Reply(NOERROR);
            DoBufferProcessingLoop();
            break;

        case CMD_STOP: Reply(NOERROR); break;

        default:
            DbgLog((LOG_ERROR, 1, TEXT("Unknown command %d received!"), cmd));
            Reply((DWORD)E_NOTIMPL);
            break;
        }
    } while (cmd != CMD_EXIT);

    hr = OnThreadDestroy(); // tidy up.
    if (FAILED(hr))
    {
        DbgLog((LOG_ERROR, 1, TEXT("CSourceStream::OnThreadDestroy failed. Exiting thread.")));
        return 1;
    }

    DbgLog((LOG_TRACE, 1, TEXT("CSourceStream worker thread exiting")));
    return 0;
}

//
// DoBufferProcessingLoop
//
// Grabs a buffer and calls the users processing function.
// Overridable, so that different delivery styles can be catered for.
HRESULT CSourceStream::DoBufferProcessingLoop(void)
{

    Command com;

    OnThreadStartPlay();

    do
    {
        while (!CheckRequest(&com))
        {

            IMediaSample *pSample;

            HRESULT hr = GetDeliveryBuffer(&pSample, NULL, NULL, 0);
            if (FAILED(hr))
            {
                Sleep(1);
                continue; // go round again. Perhaps the error will go away
                          // or the allocator is decommited & we will be asked to
                          // exit soon.
            }

            // Virtual function user will override.
            hr = FillBuffer(pSample);

            if (hr == S_OK)
            {
                hr = Deliver(pSample);
                pSample->Release();

                // downstream filter returns S_FALSE if it wants us to
                // stop or an error if it's reporting an error.
                if (hr != S_OK)
                {
                    DbgLog((LOG_TRACE, 2, TEXT("Deliver() returned %08x; stopping"), hr));
                    return S_OK;
                }
            }
            else if (hr == S_FALSE)
            {
                // derived class wants us to stop pushing data
                pSample->Release();
                DeliverEndOfStream();
                return S_OK;
            }
            else
            {
                // derived class encountered an error
                pSample->Release();
                DbgLog((LOG_ERROR, 1, TEXT("Error %08lX from FillBuffer!!!"), hr));
                DeliverEndOfStream();
                m_pFilter->NotifyEvent(EC_ERRORABORT, hr, 0);
                return hr;
            }

            // all paths release the sample
        }

        // For all commands sent to us there must be a Reply call!

        if (com == CMD_RUN || com == CMD_PAUSE)
        {
            Reply(NOERROR);
        }
        else if (com != CMD_STOP)
        {
            Reply((DWORD)E_UNEXPECTED);
            DbgLog((LOG_ERROR, 1, TEXT("Unexpected command!!!")));
        }
    } while (com != CMD_STOP);

    return S_FALSE;
}
