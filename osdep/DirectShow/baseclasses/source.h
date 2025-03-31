//------------------------------------------------------------------------------
// File: Source.h
//
// Desc: DirectShow base classes - defines classes to simplify creation of
//       ActiveX source filters that support continuous generation of data.
//       No support is provided for IMediaControl or IMediaPosition.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

//
// Derive your source filter from CSource.
// During construction either:
//    Create some CSourceStream objects to manage your pins
//    Provide the user with a means of doing so eg, an IPersistFile interface.
//
// CSource provides:
//    IBaseFilter interface management
//    IMediaFilter interface management, via CBaseFilter
//    Pin counting for CBaseFilter
//
// Derive a class from CSourceStream to manage your output pin types
//  Implement GetMediaType/1 to return the type you support. If you support multiple
//   types then overide GetMediaType/3, CheckMediaType and GetMediaTypeCount.
//  Implement Fillbuffer() to put data into one buffer.
//
// CSourceStream provides:
//    IPin management via CBaseOutputPin
//    Worker thread management

#ifndef __CSOURCE__
#define __CSOURCE__

class CSourceStream; // The class that will handle each pin

//
// CSource
//
// Override construction to provide a means of creating
// CSourceStream derived objects - ie a way of creating pins.
class CSource : public CBaseFilter
{
  public:
    CSource(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN lpunk, CLSID clsid, __inout HRESULT *phr);
    CSource(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN lpunk, CLSID clsid);
#ifdef UNICODE
    CSource(__in_opt LPCSTR pName, __inout_opt LPUNKNOWN lpunk, CLSID clsid, __inout HRESULT *phr);
    CSource(__in_opt LPCSTR pName, __inout_opt LPUNKNOWN lpunk, CLSID clsid);
#endif
    ~CSource();

    int GetPinCount(void);
    CBasePin *GetPin(int n);

    // -- Utilities --

    CCritSec *pStateLock(void) { return &m_cStateLock; } // provide our critical section

    HRESULT AddPin(__in CSourceStream *);
    HRESULT RemovePin(__in CSourceStream *);

    STDMETHODIMP FindPin(LPCWSTR Id, __deref_out IPin **ppPin);

    int FindPinNumber(__in IPin *iPin);

  protected:
    int m_iPins;                 // The number of pins on this filter. Updated by CSourceStream
                                 // constructors & destructors.
    CSourceStream **m_paStreams; // the pins on this filter.

    CCritSec m_cStateLock; // Lock this to serialize function accesses to the filter state
};

//
// CSourceStream
//
// Use this class to manage a stream of data that comes from a
// pin.
// Uses a worker thread to put data on the pin.
class CSourceStream
    : public CAMThread
    , public CBaseOutputPin
{
  public:
    CSourceStream(__in_opt LPCTSTR pObjectName, __inout HRESULT *phr, __inout CSource *pms, __in_opt LPCWSTR pName);
#ifdef UNICODE
    CSourceStream(__in_opt LPCSTR pObjectName, __inout HRESULT *phr, __inout CSource *pms, __in_opt LPCWSTR pName);
#endif
    virtual ~CSourceStream(void); // virtual destructor ensures derived class destructors are called too.

  protected:
    CSource *m_pFilter; // The parent of this stream

    // *
    // * Data Source
    // *
    // * The following three functions: FillBuffer, OnThreadCreate/Destroy, are
    // * called from within the ThreadProc. They are used in the creation of
    // * the media samples this pin will provide
    // *

    // Override this to provide the worker thread a means
    // of processing a buffer
    virtual HRESULT FillBuffer(IMediaSample *pSamp) PURE;

    // Called as the thread is created/destroyed - use to perform
    // jobs such as start/stop streaming mode
    // If OnThreadCreate returns an error the thread will exit.
    virtual HRESULT OnThreadCreate(void) { return NOERROR; };
    virtual HRESULT OnThreadDestroy(void) { return NOERROR; };
    virtual HRESULT OnThreadStartPlay(void) { return NOERROR; };

    // *
    // * Worker Thread
    // *

    HRESULT Active(void);   // Starts up the worker thread
    HRESULT Inactive(void); // Exits the worker thread.

  public:
    // thread commands
    enum Command
    {
        CMD_INIT,
        CMD_PAUSE,
        CMD_RUN,
        CMD_STOP,
        CMD_EXIT
    };
    HRESULT Init(void) { return CallWorker(CMD_INIT); }
    HRESULT Exit(void) { return CallWorker(CMD_EXIT); }
    HRESULT Run(void) { return CallWorker(CMD_RUN); }
    HRESULT Pause(void) { return CallWorker(CMD_PAUSE); }
    HRESULT Stop(void) { return CallWorker(CMD_STOP); }

  protected:
    Command GetRequest(void) { return (Command)CAMThread::GetRequest(); }
    BOOL CheckRequest(Command *pCom) { return CAMThread::CheckRequest((DWORD *)pCom); }

    // override these if you want to add thread commands
    virtual DWORD ThreadProc(void); // the thread function

    virtual HRESULT DoBufferProcessingLoop(void); // the loop executed whilst running

    // *
    // * AM_MEDIA_TYPE support
    // *

    // If you support more than one media type then override these 2 functions
    virtual HRESULT CheckMediaType(const CMediaType *pMediaType);
    virtual HRESULT GetMediaType(int iPosition, __inout CMediaType *pMediaType); // List pos. 0-n

    // If you support only one type then override this fn.
    // This will only be called by the default implementations
    // of CheckMediaType and GetMediaType(int, CMediaType*)
    // You must override this fn. or the above 2!
    virtual HRESULT GetMediaType(__inout CMediaType *pMediaType) { return E_UNEXPECTED; }

    STDMETHODIMP QueryId(__deref_out LPWSTR *Id);
};

#endif // __CSOURCE__
