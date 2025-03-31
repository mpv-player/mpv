//------------------------------------------------------------------------------
// File: OutputQ.cpp
//
// Desc: DirectShow base classes - implements COutputQueue class used by an
//       output pin which may sometimes want to queue output samples on a
//       separate thread and sometimes call Receive() directly on the input
//       pin.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>

//
//  COutputQueue Constructor :
//
//  Determines if a thread is to be created and creates resources
//
//     pInputPin  - the downstream input pin we're queueing samples to
//
//     phr        - changed to a failure code if this function fails
//                  (otherwise unchanges)
//
//     bAuto      - Ask pInputPin if it can block in Receive by calling
//                  its ReceiveCanBlock method and create a thread if
//                  it can block, otherwise not.
//
//     bQueue     - if bAuto == FALSE then we create a thread if and only
//                  if bQueue == TRUE
//
//     lBatchSize - work in batches of lBatchSize
//
//     bBatchEact - Use exact batch sizes so don't send until the
//                  batch is full or SendAnyway() is called
//
//     lListSize  - If we create a thread make the list of samples queued
//                  to the thread have this size cache
//
//     dwPriority - If we create a thread set its priority to this
//
COutputQueue::COutputQueue(IPin *pInputPin,      //  Pin to send stuff to
                           __inout HRESULT *phr, //  'Return code'
                           BOOL bAuto,           //  Ask pin if queue or not
                           BOOL bQueue,          //  Send through queue
                           LONG lBatchSize,      //  Batch
                           BOOL bBatchExact,     //  Batch exactly to BatchSize
                           LONG lListSize, DWORD dwPriority,
                           bool bFlushingOpt // flushing optimization
                           )
    : m_lBatchSize(lBatchSize)
    , m_bBatchExact(bBatchExact && (lBatchSize > 1))
    , m_hThread(NULL)
    , m_hSem(NULL)
    , m_List(NULL)
    , m_pPin(pInputPin)
    , m_ppSamples(NULL)
    , m_lWaiting(0)
    , m_evFlushComplete(FALSE, phr)
    , m_pInputPin(NULL)
    , m_bSendAnyway(FALSE)
    , m_nBatched(0)
    , m_bFlushing(FALSE)
    , m_bFlushed(TRUE)
    , m_bFlushingOpt(bFlushingOpt)
    , m_bTerminate(FALSE)
    , m_hEventPop(NULL)
    , m_hr(S_OK)
{
    ASSERT(m_lBatchSize > 0);

    if (FAILED(*phr))
    {
        return;
    }

    //  Check the input pin is OK and cache its IMemInputPin interface

    *phr = pInputPin->QueryInterface(IID_IMemInputPin, (void **)&m_pInputPin);
    if (FAILED(*phr))
    {
        return;
    }

    // See if we should ask the downstream pin

    if (bAuto)
    {
        HRESULT hr = m_pInputPin->ReceiveCanBlock();
        if (SUCCEEDED(hr))
        {
            bQueue = hr == S_OK;
        }
    }

    //  Create our sample batch

    m_ppSamples = new PMEDIASAMPLE[m_lBatchSize];
    if (m_ppSamples == NULL)
    {
        *phr = E_OUTOFMEMORY;
        return;
    }

    //  If we're queueing allocate resources

    if (bQueue)
    {
        DbgLog((LOG_TRACE, 2, TEXT("Creating thread for output pin")));
        m_hSem = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);
        if (m_hSem == NULL)
        {
            DWORD dwError = GetLastError();
            *phr = AmHresultFromWin32(dwError);
            return;
        }
        m_List = new CSampleList(NAME("Sample Queue List"), lListSize,
                                 FALSE // No lock
        );
        if (m_List == NULL)
        {
            *phr = E_OUTOFMEMORY;
            return;
        }

        DWORD dwThreadId;
        m_hThread = CreateThread(NULL, 0, InitialThreadProc, (LPVOID)this, 0, &dwThreadId);
        if (m_hThread == NULL)
        {
            DWORD dwError = GetLastError();
            *phr = AmHresultFromWin32(dwError);
            return;
        }
        SetThreadPriority(m_hThread, dwPriority);
    }
    else
    {
        DbgLog((LOG_TRACE, 2, TEXT("Calling input pin directly - no thread")));
    }
}

//
//  COutputQueuee Destructor :
//
//  Free all resources -
//
//      Thread,
//      Batched samples
//
COutputQueue::~COutputQueue()
{
    DbgLog((LOG_TRACE, 3, TEXT("COutputQueue::~COutputQueue")));
    /*  Free our pointer */
    if (m_pInputPin != NULL)
    {
        m_pInputPin->Release();
    }
    if (m_hThread != NULL)
    {
        {
            CAutoLock lck(this);
            m_bTerminate = TRUE;
            m_hr = S_FALSE;
            NotifyThread();
        }
        DbgWaitForSingleObject(m_hThread);
        EXECUTE_ASSERT(CloseHandle(m_hThread));

        //  The thread frees the samples when asked to terminate

        ASSERT(m_List->GetCount() == 0);
        delete m_List;
    }
    else
    {
        FreeSamples();
    }
    if (m_hSem != NULL)
    {
        EXECUTE_ASSERT(CloseHandle(m_hSem));
    }
    delete[] m_ppSamples;
}

//
//  Call the real thread proc as a member function
//
DWORD WINAPI COutputQueue::InitialThreadProc(__in LPVOID pv)
{
    HRESULT hrCoInit = CAMThread::CoInitializeHelper();

    COutputQueue *pSampleQueue = (COutputQueue *)pv;
    DWORD dwReturn = pSampleQueue->ThreadProc();

    if (hrCoInit == S_OK)
    {
        CoUninitialize();
    }

    return dwReturn;
}

//
//  Thread sending the samples downstream :
//
//  When there is nothing to do the thread sets m_lWaiting (while
//  holding the critical section) and then waits for m_hSem to be
//  set (not holding the critical section)
//
DWORD COutputQueue::ThreadProc()
{
    while (TRUE)
    {
        BOOL bWait = FALSE;
        IMediaSample *pSample;
        LONG lNumberToSend; // Local copy
        NewSegmentPacket *ppacket;

        //
        //  Get a batch of samples and send it if possible
        //  In any case exit the loop if there is a control action
        //  requested
        //
        {
            CAutoLock lck(this);
            while (TRUE)
            {

                if (m_bTerminate)
                {
                    FreeSamples();
                    return 0;
                }
                if (m_bFlushing)
                {
                    FreeSamples();
                    SetEvent(m_evFlushComplete);
                }

                //  Get a sample off the list

                pSample = m_List->RemoveHead();
                // inform derived class we took something off the queue
                if (m_hEventPop)
                {
                    // DbgLog((LOG_TRACE,3,TEXT("Queue: Delivered  SET EVENT")));
                    SetEvent(m_hEventPop);
                }

                if (pSample != NULL && !IsSpecialSample(pSample))
                {

                    //  If its just a regular sample just add it to the batch
                    //  and exit the loop if the batch is full

                    m_ppSamples[m_nBatched++] = pSample;
                    if (m_nBatched == m_lBatchSize)
                    {
                        break;
                    }
                }
                else
                {

                    //  If there was nothing in the queue and there's nothing
                    //  to send (either because there's nothing or the batch
                    //  isn't full) then prepare to wait

                    if (pSample == NULL && (m_bBatchExact || m_nBatched == 0))
                    {

                        //  Tell other thread to set the event when there's
                        //  something do to

                        ASSERT(m_lWaiting == 0);
                        m_lWaiting++;
                        bWait = TRUE;
                    }
                    else
                    {

                        //  We break out of the loop on SEND_PACKET unless
                        //  there's nothing to send

                        if (pSample == SEND_PACKET && m_nBatched == 0)
                        {
                            continue;
                        }

                        if (pSample == NEW_SEGMENT)
                        {
                            // now we need the parameters - we are
                            // guaranteed that the next packet contains them
                            ppacket = (NewSegmentPacket *)m_List->RemoveHead();
                            // we took something off the queue
                            if (m_hEventPop)
                            {
                                // DbgLog((LOG_TRACE,3,TEXT("Queue: Delivered  SET EVENT")));
                                SetEvent(m_hEventPop);
                            }

                            ASSERT(ppacket);
                        }
                        //  EOS_PACKET falls through here and we exit the loop
                        //  In this way it acts like SEND_PACKET
                    }
                    break;
                }
            }
            if (!bWait)
            {
                // We look at m_nBatched from the client side so keep
                // it up to date inside the critical section
                lNumberToSend = m_nBatched; // Local copy
                m_nBatched = 0;
            }
        }

        //  Wait for some more data

        if (bWait)
        {
            DbgWaitForSingleObject(m_hSem);
            continue;
        }

        //  OK - send it if there's anything to send
        //  We DON'T check m_bBatchExact here because either we've got
        //  a full batch or we dropped through because we got
        //  SEND_PACKET or EOS_PACKET - both of which imply we should
        //  flush our batch

        if (lNumberToSend != 0)
        {
            long nProcessed;
            if (m_hr == S_OK)
            {
                ASSERT(!m_bFlushed);
                HRESULT hr = m_pInputPin->ReceiveMultiple(m_ppSamples, lNumberToSend, &nProcessed);
                /*  Don't overwrite a flushing state HRESULT */
                CAutoLock lck(this);
                if (m_hr == S_OK)
                {
                    m_hr = hr;
                }
                ASSERT(!m_bFlushed);
            }
            while (lNumberToSend != 0)
            {
                m_ppSamples[--lNumberToSend]->Release();
            }
            if (m_hr != S_OK)
            {

                //  In any case wait for more data - S_OK just
                //  means there wasn't an error

                DbgLog((LOG_ERROR, 2, TEXT("ReceiveMultiple returned %8.8X"), m_hr));
            }
        }

        //  Check for end of stream

        if (pSample == EOS_PACKET)
        {

            //  We don't send even end of stream on if we've previously
            //  returned something other than S_OK
            //  This is because in that case the pin which returned
            //  something other than S_OK should have either sent
            //  EndOfStream() or notified the filter graph

            if (m_hr == S_OK)
            {
                DbgLog((LOG_TRACE, 2, TEXT("COutputQueue sending EndOfStream()")));
                HRESULT hr = m_pPin->EndOfStream();
                if (FAILED(hr))
                {
                    DbgLog((LOG_ERROR, 2, TEXT("COutputQueue got code 0x%8.8X from EndOfStream()")));
                }
            }
        }

        //  Data from a new source

        if (pSample == RESET_PACKET)
        {
            m_hr = S_OK;
            SetEvent(m_evFlushComplete);
        }

        if (pSample == NEW_SEGMENT)
        {
            m_pPin->NewSegment(ppacket->tStart, ppacket->tStop, ppacket->dRate);
            delete ppacket;
        }
    }
}

//  Send batched stuff anyway
void COutputQueue::SendAnyway()
{
    if (!IsQueued())
    {

        //  m_bSendAnyway is a private parameter checked in ReceiveMultiple

        m_bSendAnyway = TRUE;
        LONG nProcessed;
        ReceiveMultiple(NULL, 0, &nProcessed);
        m_bSendAnyway = FALSE;
    }
    else
    {
        CAutoLock lck(this);
        QueueSample(SEND_PACKET);
        NotifyThread();
    }
}

void COutputQueue::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    if (!IsQueued())
    {
        if (S_OK == m_hr)
        {
            if (m_bBatchExact)
            {
                SendAnyway();
            }
            m_pPin->NewSegment(tStart, tStop, dRate);
        }
    }
    else
    {
        if (m_hr == S_OK)
        {
            //
            // we need to queue the new segment to appear in order in the
            // data, but we need to pass parameters to it. Rather than
            // take the hit of wrapping every single sample so we can tell
            // special ones apart, we queue special pointers to indicate
            // special packets, and we guarantee (by holding the
            // critical section) that the packet immediately following a
            // NEW_SEGMENT value is a NewSegmentPacket containing the
            // parameters.
            NewSegmentPacket *ppack = new NewSegmentPacket;
            if (ppack == NULL)
            {
                return;
            }
            ppack->tStart = tStart;
            ppack->tStop = tStop;
            ppack->dRate = dRate;

            CAutoLock lck(this);
            QueueSample(NEW_SEGMENT);
            QueueSample((IMediaSample *)ppack);
            NotifyThread();
        }
    }
}

//
//  End of Stream is queued to output device
//
void COutputQueue::EOS()
{
    CAutoLock lck(this);
    if (!IsQueued())
    {
        if (m_bBatchExact)
        {
            SendAnyway();
        }
        if (m_hr == S_OK)
        {
            DbgLog((LOG_TRACE, 2, TEXT("COutputQueue sending EndOfStream()")));
            m_bFlushed = FALSE;
            HRESULT hr = m_pPin->EndOfStream();
            if (FAILED(hr))
            {
                DbgLog((LOG_ERROR, 2, TEXT("COutputQueue got code 0x%8.8X from EndOfStream()")));
            }
        }
    }
    else
    {
        if (m_hr == S_OK)
        {
            m_bFlushed = FALSE;
            QueueSample(EOS_PACKET);
            NotifyThread();
        }
    }
}

//
//  Flush all the samples in the queue
//
void COutputQueue::BeginFlush()
{
    if (IsQueued())
    {
        {
            CAutoLock lck(this);

            // block receives -- we assume this is done by the
            // filter in which we are a component

            // discard all queued data

            m_bFlushing = TRUE;

            //  Make sure we discard all samples from now on

            if (m_hr == S_OK)
            {
                m_hr = S_FALSE;
            }

            // Optimize so we don't keep calling downstream all the time

            if (m_bFlushed && m_bFlushingOpt)
            {
                return;
            }

            // Make sure we really wait for the flush to complete
            m_evFlushComplete.Reset();

            NotifyThread();
        }

        // pass this downstream

        m_pPin->BeginFlush();
    }
    else
    {
        // pass downstream first to avoid deadlocks
        m_pPin->BeginFlush();
        CAutoLock lck(this);
        // discard all queued data

        m_bFlushing = TRUE;

        //  Make sure we discard all samples from now on

        if (m_hr == S_OK)
        {
            m_hr = S_FALSE;
        }
    }
}

//
// leave flush mode - pass this downstream
void COutputQueue::EndFlush()
{
    {
        CAutoLock lck(this);
        ASSERT(m_bFlushing);
        if (m_bFlushingOpt && m_bFlushed && IsQueued())
        {
            m_bFlushing = FALSE;
            m_hr = S_OK;
            return;
        }
    }

    // sync with pushing thread -- done in BeginFlush
    // ensure no more data to go downstream -- done in BeginFlush
    //
    // Because we are synching here there is no need to hold the critical
    // section (in fact we'd deadlock if we did!)

    if (IsQueued())
    {
        m_evFlushComplete.Wait();
    }
    else
    {
        FreeSamples();
    }

    //  Be daring - the caller has guaranteed no samples will arrive
    //  before EndFlush() returns

    m_bFlushing = FALSE;
    m_bFlushed = TRUE;

    // call EndFlush on downstream pins

    m_pPin->EndFlush();

    m_hr = S_OK;
}

//  COutputQueue::QueueSample
//
//  private method to Send a sample to the output queue
//  The critical section MUST be held when this is called

void COutputQueue::QueueSample(IMediaSample *pSample)
{
    if (NULL == m_List->AddTail(pSample))
    {
        if (!IsSpecialSample(pSample))
        {
            pSample->Release();
        }
    }
}

//
//  COutputQueue::Receive()
//
//  Send a single sample by the multiple sample route
//  (NOTE - this could be optimized if necessary)
//
//  On return the sample will have been Release()'d
//

HRESULT COutputQueue::Receive(IMediaSample *pSample)
{
    LONG nProcessed;
    return ReceiveMultiple(&pSample, 1, &nProcessed);
}

//
//  COutputQueue::ReceiveMultiple()
//
//  Send a set of samples to the downstream pin
//
//      ppSamples           - array of samples
//      nSamples            - how many
//      nSamplesProcessed   - How many were processed
//
//  On return all samples will have been Release()'d
//

HRESULT COutputQueue::ReceiveMultiple(__in_ecount(nSamples) IMediaSample **ppSamples, long nSamples,
                                      __out long *nSamplesProcessed)
{
    if (nSamples < 0)
    {
        return E_INVALIDARG;
    }

    CAutoLock lck(this);
    //  Either call directly or queue up the samples

    if (!IsQueued())
    {

        //  If we already had a bad return code then just return

        if (S_OK != m_hr)
        {

            //  If we've never received anything since the last Flush()
            //  and the sticky return code is not S_OK we must be
            //  flushing
            //  ((!A || B) is equivalent to A implies B)
            ASSERT(!m_bFlushed || m_bFlushing);

            //  We're supposed to Release() them anyway!
            *nSamplesProcessed = 0;
            for (int i = 0; i < nSamples; i++)
            {
                DbgLog(
                    (LOG_TRACE, 3, TEXT("COutputQueue (direct) : Discarding %d samples code 0x%8.8X"), nSamples, m_hr));
                ppSamples[i]->Release();
            }

            return m_hr;
        }
        //
        //  If we're flushing the sticky return code should be S_FALSE
        //
        ASSERT(!m_bFlushing);
        m_bFlushed = FALSE;

        ASSERT(m_nBatched < m_lBatchSize);
        ASSERT(m_nBatched == 0 || m_bBatchExact);

        //  Loop processing the samples in batches

        LONG iLost = 0;
        long iDone = 0;
        for (iDone = 0; iDone < nSamples || (m_nBatched != 0 && m_bSendAnyway);)
        {

            // pragma message (REMIND("Implement threshold scheme"))
            ASSERT(m_nBatched < m_lBatchSize);
            if (iDone < nSamples)
            {
                m_ppSamples[m_nBatched++] = ppSamples[iDone++];
            }
            if (m_nBatched == m_lBatchSize || nSamples == 0 && (m_bSendAnyway || !m_bBatchExact))
            {
                LONG nDone;
                DbgLog((LOG_TRACE, 4, TEXT("Batching %d samples"), m_nBatched));

                if (m_hr == S_OK)
                {
                    m_hr = m_pInputPin->ReceiveMultiple(m_ppSamples, m_nBatched, &nDone);
                }
                else
                {
                    nDone = 0;
                }
                iLost += m_nBatched - nDone;
                for (LONG i = 0; i < m_nBatched; i++)
                {
                    m_ppSamples[i]->Release();
                }
                m_nBatched = 0;
            }
        }
        *nSamplesProcessed = iDone - iLost;
        if (*nSamplesProcessed < 0)
        {
            *nSamplesProcessed = 0;
        }
        return m_hr;
    }
    else
    {
        /*  We're sending to our thread */

        if (m_hr != S_OK)
        {
            *nSamplesProcessed = 0;
            DbgLog((LOG_TRACE, 3, TEXT("COutputQueue (queued) : Discarding %d samples code 0x%8.8X"), nSamples, m_hr));
            for (int i = 0; i < nSamples; i++)
            {
                ppSamples[i]->Release();
            }
            return m_hr;
        }
        m_bFlushed = FALSE;
        for (long i = 0; i < nSamples; i++)
        {
            QueueSample(ppSamples[i]);
        }
        *nSamplesProcessed = nSamples;
        if (!m_bBatchExact || m_nBatched + m_List->GetCount() >= m_lBatchSize)
        {
            NotifyThread();
        }
        return S_OK;
    }
}

//  Get ready for new data - cancels sticky m_hr
void COutputQueue::Reset()
{
    if (!IsQueued())
    {
        m_hr = S_OK;
    }
    else
    {
        {
            CAutoLock lck(this);
            QueueSample(RESET_PACKET);
            NotifyThread();
        }
        m_evFlushComplete.Wait();
    }
}

//  Remove and Release() all queued and Batched samples
void COutputQueue::FreeSamples()
{
    CAutoLock lck(this);
    if (IsQueued())
    {
        while (TRUE)
        {
            IMediaSample *pSample = m_List->RemoveHead();
            // inform derived class we took something off the queue
            if (m_hEventPop)
            {
                // DbgLog((LOG_TRACE,3,TEXT("Queue: Delivered  SET EVENT")));
                SetEvent(m_hEventPop);
            }

            if (pSample == NULL)
            {
                break;
            }
            if (!IsSpecialSample(pSample))
            {
                pSample->Release();
            }
            else
            {
                if (pSample == NEW_SEGMENT)
                {
                    //  Free NEW_SEGMENT packet
                    NewSegmentPacket *ppacket = (NewSegmentPacket *)m_List->RemoveHead();
                    // inform derived class we took something off the queue
                    if (m_hEventPop)
                    {
                        // DbgLog((LOG_TRACE,3,TEXT("Queue: Delivered  SET EVENT")));
                        SetEvent(m_hEventPop);
                    }

                    ASSERT(ppacket != NULL);
                    delete ppacket;
                }
            }
        }
    }
    for (int i = 0; i < m_nBatched; i++)
    {
        m_ppSamples[i]->Release();
    }
    m_nBatched = 0;
}

//  Notify the thread if there is something to do
//
//  The critical section MUST be held when this is called
void COutputQueue::NotifyThread()
{
    //  Optimize - no need to signal if it's not waiting
    ASSERT(IsQueued());
    if (m_lWaiting)
    {
        ReleaseSemaphore(m_hSem, m_lWaiting, NULL);
        m_lWaiting = 0;
    }
}

//  See if there's any work to do
//  Returns
//      TRUE  if there is nothing on the queue and nothing in the batch
//            and all data has been sent
//      FALSE otherwise
//
BOOL COutputQueue::IsIdle()
{
    CAutoLock lck(this);

    //  We're idle if
    //      there is no thread (!IsQueued()) OR
    //      the thread is waiting for more work  (m_lWaiting != 0)
    //  AND
    //      there's nothing in the current batch (m_nBatched == 0)

    if (IsQueued() && m_lWaiting == 0 || m_nBatched != 0)
    {
        return FALSE;
    }
    else
    {

        //  If we're idle it shouldn't be possible for there
        //  to be anything on the work queue

        ASSERT(!IsQueued() || m_List->GetCount() == 0);
        return TRUE;
    }
}

void COutputQueue::SetPopEvent(HANDLE hEvent)
{
    m_hEventPop = hEvent;
}
