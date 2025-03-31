//------------------------------------------------------------------------------
// File: OutputQ.h
//
// Desc: DirectShow base classes -  defines the COutputQueue class, which
//       makes a queue of samples and sends them to an output pin.  The
//       class will optionally send the samples to the pin directly.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

typedef CGenericList<IMediaSample> CSampleList;

class COutputQueue : public CCritSec
{
  public:
    //  Constructor
    COutputQueue(IPin *pInputPin,          //  Pin to send stuff to
                 __inout HRESULT *phr,     //  'Return code'
                 BOOL bAuto = TRUE,        //  Ask pin if blocks
                 BOOL bQueue = TRUE,       //  Send through queue (ignored if
                                           //  bAuto set)
                 LONG lBatchSize = 1,      //  Batch
                 BOOL bBatchExact = FALSE, //  Batch exactly to BatchSize
                 LONG lListSize =          //  Likely number in the list
                 DEFAULTCACHE,
                 DWORD dwPriority = //  Priority of thread to create
                 THREAD_PRIORITY_NORMAL,
                 bool bFlushingOpt = false // flushing optimization
    );
    ~COutputQueue();

    // enter flush state - discard all data
    void BeginFlush(); // Begin flushing samples

    // re-enable receives (pass this downstream)
    void EndFlush(); // Complete flush of samples - downstream
                     // pin guaranteed not to block at this stage

    void EOS(); // Call this on End of stream

    void SendAnyway(); // Send batched samples anyway (if bBatchExact set)

    void NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    HRESULT Receive(IMediaSample *pSample);

    // do something with these media samples
    HRESULT ReceiveMultiple(__in_ecount(nSamples) IMediaSample **pSamples, long nSamples,
                            __out long *nSamplesProcessed);

    void Reset(); // Reset m_hr ready for more data

    //  See if its idle or not
    BOOL IsIdle();

    // give the class an event to fire after everything removed from the queue
    void SetPopEvent(HANDLE hEvent);

  protected:
    static DWORD WINAPI InitialThreadProc(__in LPVOID pv);
    DWORD ThreadProc();
    BOOL IsQueued() { return m_List != NULL; };

    //  The critical section MUST be held when this is called
    void QueueSample(IMediaSample *pSample);

    BOOL IsSpecialSample(IMediaSample *pSample) { return (DWORD_PTR)pSample > (DWORD_PTR)(LONG_PTR)(-16); };

    //  Remove and Release() batched and queued samples
    void FreeSamples();

    //  Notify the thread there is something to do
    void NotifyThread();

  protected:
//  Queue 'messages'
#define SEND_PACKET ((IMediaSample *)(LONG_PTR)(-2))  // Send batch
#define EOS_PACKET ((IMediaSample *)(LONG_PTR)(-3))   // End of stream
#define RESET_PACKET ((IMediaSample *)(LONG_PTR)(-4)) // Reset m_hr
#define NEW_SEGMENT ((IMediaSample *)(LONG_PTR)(-5))  // send NewSegment

    // new segment packet is always followed by one of these
    struct NewSegmentPacket
    {
        REFERENCE_TIME tStart;
        REFERENCE_TIME tStop;
        double dRate;
    };

    // Remember input stuff
    IPin *const m_pPin;
    IMemInputPin *m_pInputPin;
    BOOL const m_bBatchExact;
    LONG const m_lBatchSize;

    CSampleList *m_List;
    HANDLE m_hSem;
    CAMEvent m_evFlushComplete;
    HANDLE m_hThread;
    __field_ecount_opt(m_lBatchSize) IMediaSample **m_ppSamples;
    __range(0, m_lBatchSize) LONG m_nBatched;

    //  Wait optimization
    LONG m_lWaiting;
    //  Flush synchronization
    BOOL m_bFlushing;

    // flushing optimization. some downstream filters have trouble
    // with the queue's flushing optimization. other rely on it
    BOOL m_bFlushed;
    bool m_bFlushingOpt;

    //  Terminate now
    BOOL m_bTerminate;

    //  Send anyway flag for batching
    BOOL m_bSendAnyway;

    //  Deferred 'return code'
    HRESULT volatile m_hr;

    // an event that can be fired after every deliver
    HANDLE m_hEventPop;
};
