//------------------------------------------------------------------------------
// File: WXUtil.h
//
// Desc: DirectShow base classes - defines helper classes and functions for
//       building multimedia filters.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __WXUTIL__
#define __WXUTIL__

// eliminate spurious "statement has no effect" warnings.
#pragma warning(disable : 4705)

// wrapper for whatever critical section we have
class CCritSec
{

    // make copy constructor and assignment operator inaccessible

    CCritSec(const CCritSec &refCritSec);
    CCritSec &operator=(const CCritSec &refCritSec);

    CRITICAL_SECTION m_CritSec;

#ifdef DEBUG
  public:
    DWORD m_currentOwner;
    DWORD m_lockCount;
    BOOL m_fTrace; // Trace this one
  public:
    CCritSec();
    ~CCritSec();
    void Lock();
    void Unlock();
#else

  public:
    CCritSec() { InitializeCriticalSection(&m_CritSec); };

    ~CCritSec() { DeleteCriticalSection(&m_CritSec); };

    void Lock() { EnterCriticalSection(&m_CritSec); };

    void Unlock() { LeaveCriticalSection(&m_CritSec); };
#endif
};

//
// To make deadlocks easier to track it is useful to insert in the
// code an assertion that says whether we own a critical section or
// not.  We make the routines that do the checking globals to avoid
// having different numbers of member functions in the debug and
// retail class implementations of CCritSec.  In addition we provide
// a routine that allows usage of specific critical sections to be
// traced.  This is NOT on by default - there are far too many.
//

#ifdef DEBUG
BOOL WINAPI CritCheckIn(CCritSec *pcCrit);
BOOL WINAPI CritCheckIn(const CCritSec *pcCrit);
BOOL WINAPI CritCheckOut(CCritSec *pcCrit);
BOOL WINAPI CritCheckOut(const CCritSec *pcCrit);
void WINAPI DbgLockTrace(CCritSec *pcCrit, BOOL fTrace);
#else
#define CritCheckIn(x) TRUE
#define CritCheckOut(x) TRUE
#define DbgLockTrace(pc, fT)
#endif

// locks a critical section, and unlocks it automatically
// when the lock goes out of scope
class CAutoLock
{

    // make copy constructor and assignment operator inaccessible

    CAutoLock(const CAutoLock &refAutoLock);
    CAutoLock &operator=(const CAutoLock &refAutoLock);

  protected:
    CCritSec *m_pLock;

  public:
    CAutoLock(CCritSec *plock)
    {
        m_pLock = plock;
        m_pLock->Lock();
    };

    ~CAutoLock() { m_pLock->Unlock(); };
};

// wrapper for event objects
class CAMEvent
{

    // make copy constructor and assignment operator inaccessible

    CAMEvent(const CAMEvent &refEvent);
    CAMEvent &operator=(const CAMEvent &refEvent);

  protected:
    HANDLE m_hEvent;

  public:
    CAMEvent(BOOL fManualReset = FALSE, __inout_opt HRESULT *phr = NULL);
    CAMEvent(__inout_opt HRESULT *phr);
    ~CAMEvent();

    // Cast to HANDLE - we don't support this as an lvalue
    operator HANDLE() const { return m_hEvent; };

    void Set() { EXECUTE_ASSERT(SetEvent(m_hEvent)); };
    BOOL Wait(DWORD dwTimeout = INFINITE) { return (WaitForSingleObject(m_hEvent, dwTimeout) == WAIT_OBJECT_0); };
    void Reset() { ResetEvent(m_hEvent); };
    BOOL Check() { return Wait(0); };
};

// wrapper for event objects that do message processing
// This adds ONE method to the CAMEvent object to allow sent
// messages to be processed while waiting

class CAMMsgEvent : public CAMEvent
{

  public:
    CAMMsgEvent(__inout_opt HRESULT *phr = NULL);

    // Allow SEND messages to be processed while waiting
    BOOL WaitMsg(DWORD dwTimeout = INFINITE);
};

// old name supported for the time being
#define CTimeoutEvent CAMEvent

// support for a worker thread

#ifdef AM_NOVTABLE
// simple thread class supports creation of worker thread, synchronization
// and communication. Can be derived to simplify parameter passing
class AM_NOVTABLE CAMThread
{

    // make copy constructor and assignment operator inaccessible

    CAMThread(const CAMThread &refThread);
    CAMThread &operator=(const CAMThread &refThread);

    CAMEvent m_EventSend;
    CAMEvent m_EventComplete;

    DWORD m_dwParam;
    DWORD m_dwReturnVal;

  protected:
    HANDLE m_hThread;

    // thread will run this function on startup
    // must be supplied by derived class
    virtual DWORD ThreadProc() = 0;

  public:
    CAMThread(__inout_opt HRESULT *phr = NULL);
    virtual ~CAMThread();

    CCritSec m_AccessLock; // locks access by client threads
    CCritSec m_WorkerLock; // locks access to shared objects

    // thread initially runs this. param is actually 'this'. function
    // just gets this and calls ThreadProc
    static unsigned int WINAPI InitialThreadProc(__inout LPVOID pv);

    // start thread running  - error if already running
    BOOL Create();

    // signal the thread, and block for a response
    //
    DWORD CallWorker(DWORD);

    // accessor thread calls this when done with thread (having told thread
    // to exit)
    void Close()
    {

        // Disable warning: Conversion from LONG to PVOID of greater size
#pragma warning(push)
#pragma warning(disable : 4312)
        HANDLE hThread = (HANDLE)InterlockedExchangePointer(&m_hThread, 0);
#pragma warning(pop)

        if (hThread)
        {
            WaitForSingleObject(hThread, INFINITE);
            CloseHandle(hThread);
        }
    };

    // ThreadExists
    // Return TRUE if the thread exists. FALSE otherwise
    BOOL ThreadExists(void) const
    {
        if (m_hThread == 0)
        {
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }

    // wait for the next request
    DWORD GetRequest();

    // is there a request?
    BOOL CheckRequest(__out_opt DWORD *pParam);

    // reply to the request
    void Reply(DWORD);

    // If you want to do WaitForMultipleObjects you'll need to include
    // this handle in your wait list or you won't be responsive
    HANDLE GetRequestHandle() const { return m_EventSend; };

    // Find out what the request was
    DWORD GetRequestParam() const { return m_dwParam; };

    // call CoInitializeEx (COINIT_DISABLE_OLE1DDE) if
    // available. S_FALSE means it's not available.
    static HRESULT CoInitializeHelper();
};
#endif // AM_NOVTABLE

// CQueue
//
// Implements a simple Queue ADT.  The queue contains a finite number of
// objects, access to which is controlled by a semaphore.  The semaphore
// is created with an initial count (N).  Each time an object is added
// a call to WaitForSingleObject is made on the semaphore's handle.  When
// this function returns a slot has been reserved in the queue for the new
// object.  If no slots are available the function blocks until one becomes
// available.  Each time an object is removed from the queue ReleaseSemaphore
// is called on the semaphore's handle, thus freeing a slot in the queue.
// If no objects are present in the queue the function blocks until an
// object has been added.

#define DEFAULT_QUEUESIZE 2

template <class T> class CQueue
{
  private:
    HANDLE hSemPut;            // Semaphore controlling queue "putting"
    HANDLE hSemGet;            // Semaphore controlling queue "getting"
    CRITICAL_SECTION CritSect; // Thread seriallization
    int nMax;                  // Max objects allowed in queue
    int iNextPut;              // Array index of next "PutMsg"
    int iNextGet;              // Array index of next "GetMsg"
    T *QueueObjects;           // Array of objects (ptr's to void)

    void Initialize(int n)
    {
        iNextPut = iNextGet = 0;
        nMax = n;
        InitializeCriticalSection(&CritSect);
        hSemPut = CreateSemaphore(NULL, n, n, NULL);
        hSemGet = CreateSemaphore(NULL, 0, n, NULL);
        QueueObjects = new T[n];
    }

  public:
    CQueue(int n) { Initialize(n); }

    CQueue() { Initialize(DEFAULT_QUEUESIZE); }

    ~CQueue()
    {
        delete[] QueueObjects;
        DeleteCriticalSection(&CritSect);
        CloseHandle(hSemPut);
        CloseHandle(hSemGet);
    }

    T GetQueueObject()
    {
        int iSlot;
        T Object;
        LONG lPrevious;

        // Wait for someone to put something on our queue, returns straight
        // away is there is already an object on the queue.
        //
        WaitForSingleObject(hSemGet, INFINITE);

        EnterCriticalSection(&CritSect);
        iSlot = iNextGet++ % nMax;
        Object = QueueObjects[iSlot];
        LeaveCriticalSection(&CritSect);

        // Release anyone waiting to put an object onto our queue as there
        // is now space available in the queue.
        //
        ReleaseSemaphore(hSemPut, 1L, &lPrevious);
        return Object;
    }

    void PutQueueObject(T Object)
    {
        int iSlot;
        LONG lPrevious;

        // Wait for someone to get something from our queue, returns straight
        // away is there is already an empty slot on the queue.
        //
        WaitForSingleObject(hSemPut, INFINITE);

        EnterCriticalSection(&CritSect);
        iSlot = iNextPut++ % nMax;
        QueueObjects[iSlot] = Object;
        LeaveCriticalSection(&CritSect);

        // Release anyone waiting to remove an object from our queue as there
        // is now an object available to be removed.
        //
        ReleaseSemaphore(hSemGet, 1L, &lPrevious);
    }
};

// Ensures that memory is not read past the length source buffer
// and that memory is not written past the length of the dst buffer
//   dst - buffer to copy to
//   dst_size - total size of destination buffer
//   cb_dst_offset - offset, first byte copied to dst+cb_dst_offset
//   src - buffer to copy from
//   src_size - total size of source buffer
//   cb_src_offset - offset, first byte copied from src+cb_src_offset
//   count - number of bytes to copy
//
// Returns:
//    S_OK          - no error
//    E_INVALIDARG  - values passed would lead to overrun
HRESULT AMSafeMemMoveOffset(__in_bcount(dst_size) void *dst, __in size_t dst_size, __in DWORD cb_dst_offset,
                            __in_bcount(src_size) const void *src, __in size_t src_size, __in DWORD cb_src_offset,
                            __in size_t count);

extern "C" void *__stdcall memmoveInternal(void *, const void *, size_t);

inline void *__cdecl memchrInternal(const void *buf, int chr, size_t cnt)
{
#ifdef _X86_
    void *pRet = NULL;

    _asm {
        cld // make sure we get the direction right
        mov     ecx, cnt // num of bytes to scan
        mov     edi, buf // pointer byte stream
        mov     eax, chr // byte to scan for
        repne   scasb // look for the byte in the byte stream
        jnz     exit_memchr // Z flag set if byte found
        dec     edi // scasb always increments edi even when it
                            // finds the required byte
        mov     pRet, edi
exit_memchr:
    }
    return pRet;

#else
    while (cnt && (*(unsigned char *)buf != (unsigned char)chr))
    {
        buf = (unsigned char *)buf + 1;
        cnt--;
    }

    return (cnt ? (void *)buf : NULL);
#endif
}

void WINAPI IntToWstr(int i, __out_ecount(12) LPWSTR wstr);

#define WstrToInt(sz) _wtoi(sz)
#define atoiW(sz) _wtoi(sz)
#define atoiA(sz) atoi(sz)

// These are available to help managing bitmap VIDEOINFOHEADER media structures

extern const DWORD bits555[3];
extern const DWORD bits565[3];
extern const DWORD bits888[3];

// These help convert between VIDEOINFOHEADER and BITMAPINFO structures

STDAPI_(const GUID) GetTrueColorType(const BITMAPINFOHEADER *pbmiHeader);
STDAPI_(const GUID) GetBitmapSubtype(const BITMAPINFOHEADER *pbmiHeader);
STDAPI_(WORD) GetBitCount(const GUID *pSubtype);

// strmbase.lib implements this for compatibility with people who
// managed to link to this directly.  we don't want to advertise it.
//
// STDAPI_(/* T */ CHAR *) GetSubtypeName(const GUID *pSubtype);

STDAPI_(CHAR *) GetSubtypeNameA(const GUID *pSubtype);
STDAPI_(WCHAR *) GetSubtypeNameW(const GUID *pSubtype);

#ifdef UNICODE
#define GetSubtypeName GetSubtypeNameW
#else
#define GetSubtypeName GetSubtypeNameA
#endif

STDAPI_(LONG) GetBitmapFormatSize(const BITMAPINFOHEADER *pHeader);
STDAPI_(DWORD) GetBitmapSize(const BITMAPINFOHEADER *pHeader);

#ifdef __AMVIDEO__
STDAPI_(BOOL) ContainsPalette(const VIDEOINFOHEADER *pVideoInfo);
STDAPI_(const RGBQUAD *) GetBitmapPalette(const VIDEOINFOHEADER *pVideoInfo);
#endif // __AMVIDEO__

// Compares two interfaces and returns TRUE if they are on the same object
BOOL WINAPI IsEqualObject(IUnknown *pFirst, IUnknown *pSecond);

// This is for comparing pins
#define EqualPins(pPin1, pPin2) IsEqualObject(pPin1, pPin2)

// Arithmetic helper functions

// Compute (a * b + rnd) / c
LONGLONG WINAPI llMulDiv(LONGLONG a, LONGLONG b, LONGLONG c, LONGLONG rnd);
LONGLONG WINAPI Int64x32Div32(LONGLONG a, LONG b, LONG c, LONG rnd);

// Avoids us dyna-linking to SysAllocString to copy BSTR strings
STDAPI WriteBSTR(__deref_out BSTR *pstrDest, LPCWSTR szSrc);
STDAPI FreeBSTR(__deref_in BSTR *pstr);

// Return a wide string - allocating memory for it
// Returns:
//    S_OK          - no error
//    E_POINTER     - ppszReturn == NULL
//    E_OUTOFMEMORY - can't allocate memory for returned string
STDAPI AMGetWideString(LPCWSTR pszString, __deref_out LPWSTR *ppszReturn);

// Special wait for objects owning windows
DWORD WINAPI WaitDispatchingMessages(HANDLE hObject, DWORD dwWait, HWND hwnd = NULL, UINT uMsg = 0,
                                     HANDLE hEvent = NULL);

// HRESULT_FROM_WIN32 converts ERROR_SUCCESS to a success code, but in
// our use of HRESULT_FROM_WIN32, it typically means a function failed
// to call SetLastError(), and we still want a failure code.
//
#define AmHresultFromWin32(x) (MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, x))

// call GetLastError and return an HRESULT value that will fail the
// SUCCEEDED() macro.
HRESULT AmGetLastErrorToHResult(void);

// duplicate of ATL's CComPtr to avoid linker conflicts.

IUnknown *QzAtlComPtrAssign(__deref_inout_opt IUnknown **pp, __in_opt IUnknown *lp);

template <class T> class QzCComPtr
{
  public:
    typedef T _PtrClass;
    QzCComPtr() { p = NULL; }
    QzCComPtr(T *lp)
    {
        if ((p = lp) != NULL)
            p->AddRef();
    }
    QzCComPtr(const QzCComPtr<T> &lp)
    {
        if ((p = lp.p) != NULL)
            p->AddRef();
    }
    ~QzCComPtr()
    {
        if (p)
            p->Release();
    }
    void Release()
    {
        if (p)
            p->Release();
        p = NULL;
    }
    operator T *() { return (T *)p; }
    T &operator*()
    {
        ASSERT(p != NULL);
        return *p;
    }
    // The assert on operator& usually indicates a bug.  If this is really
    // what is needed, however, take the address of the p member explicitly.
    T **operator&()
    {
        ASSERT(p == NULL);
        return &p;
    }
    T *operator->()
    {
        ASSERT(p != NULL);
        return p;
    }
    T *operator=(T *lp) { return (T *)QzAtlComPtrAssign((IUnknown **)&p, lp); }
    T *operator=(const QzCComPtr<T> &lp) { return (T *)QzAtlComPtrAssign((IUnknown **)&p, lp.p); }
#if _MSC_VER > 1020
    bool operator!() { return (p == NULL); }
#else
    BOOL operator!() { return (p == NULL) ? TRUE : FALSE; }
#endif
    T *p;
};

#define CompatibleTimeSetEvent(uDelay, uResolution, lpTimeProc, dwUser, fuEvent) \
    timeSetEvent((uDelay), (uResolution), (lpTimeProc), (dwUser), (fuEvent) | TIME_KILL_SYNCHRONOUS)

//  Helper to replace lstrcpmi
__inline int lstrcmpiLocaleIndependentW(LPCWSTR lpsz1, LPCWSTR lpsz2)
{
    return CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, lpsz1, -1, lpsz2, -1) - CSTR_EQUAL;
}
__inline int lstrcmpiLocaleIndependentA(LPCSTR lpsz1, LPCSTR lpsz2)
{
    return CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, lpsz1, -1, lpsz2, -1) - CSTR_EQUAL;
}

#endif /* __WXUTIL__ */
