//------------------------------------------------------------------------------
// File: WXUtil.cpp
//
// Desc: DirectShow base classes - implements helper classes for building
//       multimedia filters.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <process.h>

// --- CAMEvent -----------------------
CAMEvent::CAMEvent(BOOL fManualReset, __inout_opt HRESULT *phr)
{
    m_hEvent = CreateEvent(NULL, fManualReset, FALSE, NULL);
    if (NULL == m_hEvent)
    {
        if (NULL != phr && SUCCEEDED(*phr))
        {
            *phr = E_OUTOFMEMORY;
        }
    }
}

CAMEvent::CAMEvent(__inout_opt HRESULT *phr)
{
    m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == m_hEvent)
    {
        if (NULL != phr && SUCCEEDED(*phr))
        {
            *phr = E_OUTOFMEMORY;
        }
    }
}

CAMEvent::~CAMEvent()
{
    if (m_hEvent)
    {
        EXECUTE_ASSERT(CloseHandle(m_hEvent));
    }
}

// --- CAMMsgEvent -----------------------
// One routine.  The rest is handled in CAMEvent

CAMMsgEvent::CAMMsgEvent(__inout_opt HRESULT *phr)
    : CAMEvent(FALSE, phr)
{
}

BOOL CAMMsgEvent::WaitMsg(DWORD dwTimeout)
{
    // wait for the event to be signalled, or for the
    // timeout (in MS) to expire.  allow SENT messages
    // to be processed while we wait
    DWORD dwWait;
    DWORD dwStartTime;

    // set the waiting period.
    DWORD dwWaitTime = dwTimeout;

    // the timeout will eventually run down as we iterate
    // processing messages.  grab the start time so that
    // we can calculate elapsed times.
    if (dwWaitTime != INFINITE)
    {
        dwStartTime = timeGetTime();
    }

    do
    {
        dwWait = MsgWaitForMultipleObjects(1, &m_hEvent, FALSE, dwWaitTime, QS_SENDMESSAGE);
        if (dwWait == WAIT_OBJECT_0 + 1)
        {
            MSG Message;
            PeekMessage(&Message, NULL, 0, 0, PM_NOREMOVE);

            // If we have an explicit length of time to wait calculate
            // the next wake up point - which might be now.
            // If dwTimeout is INFINITE, it stays INFINITE
            if (dwWaitTime != INFINITE)
            {

                DWORD dwElapsed = timeGetTime() - dwStartTime;

                dwWaitTime = (dwElapsed >= dwTimeout) ? 0 // wake up with WAIT_TIMEOUT
                                                      : dwTimeout - dwElapsed;
            }
        }
    } while (dwWait == WAIT_OBJECT_0 + 1);

    // return TRUE if we woke on the event handle,
    //        FALSE if we timed out.
    return (dwWait == WAIT_OBJECT_0);
}

// --- CAMThread ----------------------

CAMThread::CAMThread(__inout_opt HRESULT *phr)
    : m_EventSend(TRUE, phr)
    , // must be manual-reset for CheckRequest()
    m_EventComplete(FALSE, phr)
{
    m_hThread = NULL;
}

CAMThread::~CAMThread()
{
    Close();
}

// when the thread starts, it calls this function. We unwrap the 'this'
// pointer and call ThreadProc.
unsigned int WINAPI CAMThread::InitialThreadProc(__inout LPVOID pv)
{
    HRESULT hrCoInit = CAMThread::CoInitializeHelper();
    if (FAILED(hrCoInit))
    {
        DbgLog((LOG_ERROR, 1, TEXT("CoInitializeEx failed.")));
    }

    CAMThread *pThread = (CAMThread *)pv;

    HRESULT hr = pThread->ThreadProc();

    if (SUCCEEDED(hrCoInit))
    {
        CoUninitialize();
    }

    return hr;
}

BOOL CAMThread::Create()
{
    CAutoLock lock(&m_AccessLock);

    if (ThreadExists())
    {
        return FALSE;
    }

    m_hThread = (HANDLE)_beginthreadex(NULL,                         /* Security */
                                       0,                            /* Stack Size */
                                       CAMThread::InitialThreadProc, /* Thread process */
                                       (LPVOID)this,                 /* Arguments */
                                       0,                            /* 0 = Start Immediately */
                                       NULL                          /* Thread Address */
    );
    if (!m_hThread)
    {
        return FALSE;
    }

    return TRUE;
}

DWORD
CAMThread::CallWorker(DWORD dwParam)
{
    // lock access to the worker thread for scope of this object
    CAutoLock lock(&m_AccessLock);

    if (!ThreadExists())
    {
        return (DWORD)E_FAIL;
    }

    // set the parameter
    m_dwParam = dwParam;

    // signal the worker thread
    m_EventSend.Set();

    // wait for the completion to be signalled
    m_EventComplete.Wait();

    // done - this is the thread's return value
    return m_dwReturnVal;
}

// Wait for a request from the client
DWORD
CAMThread::GetRequest()
{
    m_EventSend.Wait();
    return m_dwParam;
}

// is there a request?
BOOL CAMThread::CheckRequest(__out_opt DWORD *pParam)
{
    if (!m_EventSend.Check())
    {
        return FALSE;
    }
    else
    {
        if (pParam)
        {
            *pParam = m_dwParam;
        }
        return TRUE;
    }
}

// reply to the request
void CAMThread::Reply(DWORD dw)
{
    m_dwReturnVal = dw;

    // The request is now complete so CheckRequest should fail from
    // now on
    //
    // This event should be reset BEFORE we signal the client or
    // the client may Set it before we reset it and we'll then
    // reset it (!)

    m_EventSend.Reset();

    // Tell the client we're finished

    m_EventComplete.Set();
}

HRESULT CAMThread::CoInitializeHelper()
{
    // call CoInitializeEx and tell OLE not to create a window (this
    // thread probably won't dispatch messages and will hang on
    // broadcast msgs o/w).
    //
    // If CoInitEx is not available, threads that don't call CoCreate
    // aren't affected. Threads that do will have to handle the
    // failure. Perhaps we should fall back to CoInitialize and risk
    // hanging?
    //

    // older versions of ole32.dll don't have CoInitializeEx

    HRESULT hr = E_FAIL;
    HINSTANCE hOle = GetModuleHandle(TEXT("ole32.dll"));
    if (hOle)
    {
        typedef HRESULT(STDAPICALLTYPE * PCoInitializeEx)(LPVOID pvReserved, DWORD dwCoInit);
        PCoInitializeEx pCoInitializeEx = (PCoInitializeEx)(GetProcAddress(hOle, "CoInitializeEx"));
        if (pCoInitializeEx)
        {
            hr = (*pCoInitializeEx)(0, COINIT_DISABLE_OLE1DDE);
        }
    }
    else
    {
        // caller must load ole32.dll
        DbgBreak("couldn't locate ole32.dll");
    }

    return hr;
}

// destructor for CMsgThread  - cleans up any messages left in the
// queue when the thread exited
CMsgThread::~CMsgThread()
{
    if (m_hThread != NULL)
    {
        WaitForSingleObject(m_hThread, INFINITE);
        EXECUTE_ASSERT(CloseHandle(m_hThread));
    }

    POSITION pos = m_ThreadQueue.GetHeadPosition();
    while (pos)
    {
        CMsg *pMsg = m_ThreadQueue.GetNext(pos);
        delete pMsg;
    }
    m_ThreadQueue.RemoveAll();

    if (m_hSem != NULL)
    {
        EXECUTE_ASSERT(CloseHandle(m_hSem));
    }
}

BOOL CMsgThread::CreateThread()
{
    m_hSem = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);
    if (m_hSem == NULL)
    {
        return FALSE;
    }

    m_hThread = ::CreateThread(NULL, 0, DefaultThreadProc, (LPVOID)this, 0, &m_ThreadId);
    return m_hThread != NULL;
}

// This is the threads message pump.  Here we get and dispatch messages to
// clients thread proc until the client refuses to process a message.
// The client returns a non-zero value to stop the message pump, this
// value becomes the threads exit code.

DWORD WINAPI CMsgThread::DefaultThreadProc(__inout LPVOID lpParam)
{
    CMsgThread *lpThis = (CMsgThread *)lpParam;
    CMsg msg;
    LRESULT lResult;

    // !!!
    CoInitialize(NULL);

    // allow a derived class to handle thread startup
    lpThis->OnThreadInit();

    do
    {
        lpThis->GetThreadMsg(&msg);
        lResult = lpThis->ThreadMessageProc(msg.uMsg, msg.dwFlags, msg.lpParam, msg.pEvent);
    } while (lResult == 0L);

    // !!!
    CoUninitialize();

    return (DWORD)lResult;
}

// Block until the next message is placed on the list m_ThreadQueue.
// copies the message to the message pointed to by *pmsg
void CMsgThread::GetThreadMsg(__out CMsg *msg)
{
    CMsg *pmsg = NULL;

    // keep trying until a message appears
    while (TRUE)
    {
        {
            CAutoLock lck(&m_Lock);
            pmsg = m_ThreadQueue.RemoveHead();
            if (pmsg == NULL)
            {
                m_lWaiting++;
            }
            else
            {
                break;
            }
        }
        // the semaphore will be signalled when it is non-empty
        WaitForSingleObject(m_hSem, INFINITE);
    }
    // copy fields to caller's CMsg
    *msg = *pmsg;

    // this CMsg was allocated by the 'new' in PutThreadMsg
    delete pmsg;
}

// Helper function - convert int to WSTR
void WINAPI IntToWstr(int i, __out_ecount(12) LPWSTR wstr)
{
#ifdef UNICODE
    if (FAILED(StringCchPrintf(wstr, 12, L"%d", i)))
    {
        wstr[0] = 0;
    }
#else
    TCHAR temp[12];
    if (FAILED(StringCchPrintf(temp, NUMELMS(temp), "%d", i)))
    {
        wstr[0] = 0;
    }
    else
    {
        MultiByteToWideChar(CP_ACP, 0, temp, -1, wstr, 12);
    }
#endif
} // IntToWstr

#define MEMORY_ALIGNMENT 4
#define MEMORY_ALIGNMENT_LOG2 2
#define MEMORY_ALIGNMENT_MASK MEMORY_ALIGNMENT - 1

void *__stdcall memmoveInternal(void *dst, const void *src, size_t count)
{
    void *ret = dst;

#ifdef _X86_
    if (dst <= src || (char *)dst >= ((char *)src + count))
    {

        /*
         * Non-Overlapping Buffers
         * copy from lower addresses to higher addresses
         */
        _asm {
            mov     esi,src
            mov     edi,dst
            mov     ecx,count
            cld
            mov     edx,ecx
            and     edx,MEMORY_ALIGNMENT_MASK
            shr     ecx,MEMORY_ALIGNMENT_LOG2
            rep     movsd
            or      ecx,edx
            jz      memmove_done
            rep     movsb
memmove_done:
        }
    }
    else
    {

        /*
         * Overlapping Buffers
         * copy from higher addresses to lower addresses
         */
        _asm {
            mov     esi,src
            mov     edi,dst
            mov     ecx,count
            std
            add     esi,ecx
            add     edi,ecx
            dec     esi
            dec     edi
            rep     movsb
            cld
        }
    }
#else
    MoveMemory(dst, src, count);
#endif

    return ret;
}

HRESULT AMSafeMemMoveOffset(__in_bcount(dst_size) void *dst, __in size_t dst_size, __in DWORD cb_dst_offset,
                            __in_bcount(src_size) const void *src, __in size_t src_size, __in DWORD cb_src_offset,
                            __in size_t count)
{
    // prevent read overruns
    if (count + cb_src_offset < count ||  // prevent integer overflow
        count + cb_src_offset > src_size) // prevent read overrun
    {
        return E_INVALIDARG;
    }

    // prevent write overruns
    if (count + cb_dst_offset < count ||  // prevent integer overflow
        count + cb_dst_offset > dst_size) // prevent write overrun
    {
        return E_INVALIDARG;
    }

    memmoveInternal((BYTE *)dst + cb_dst_offset, (BYTE *)src + cb_src_offset, count);
    return S_OK;
}

#ifdef DEBUG
/******************************Public*Routine******************************\
* Debug CCritSec helpers
*
* We provide debug versions of the Constructor, destructor, Lock and Unlock
* routines.  The debug code tracks who owns each critical section by
* maintaining a depth count.
*
* History:
*
\**************************************************************************/

CCritSec::CCritSec()
{
    InitializeCriticalSection(&m_CritSec);
    m_currentOwner = m_lockCount = 0;
    m_fTrace = FALSE;
}

CCritSec::~CCritSec()
{
    DeleteCriticalSection(&m_CritSec);
}

void CCritSec::Lock()
{
    UINT tracelevel = 3;
    DWORD us = GetCurrentThreadId();
    DWORD currentOwner = m_currentOwner;
    if (currentOwner && (currentOwner != us))
    {
        // already owned, but not by us
        if (m_fTrace)
        {
            DbgLog((LOG_LOCKING, 2, TEXT("Thread %d about to wait for lock %x owned by %d"), GetCurrentThreadId(),
                    &m_CritSec, currentOwner));
            tracelevel = 2;
            // if we saw the message about waiting for the critical
            // section we ensure we see the message when we get the
            // critical section
        }
    }
    EnterCriticalSection(&m_CritSec);
    if (0 == m_lockCount++)
    {
        // we now own it for the first time.  Set owner information
        m_currentOwner = us;

        if (m_fTrace)
        {
            DbgLog((LOG_LOCKING, tracelevel, TEXT("Thread %d now owns lock %x"), m_currentOwner, &m_CritSec));
        }
    }
}

void CCritSec::Unlock()
{
    if (0 == --m_lockCount)
    {
        // about to be unowned
        if (m_fTrace)
        {
            DbgLog((LOG_LOCKING, 3, TEXT("Thread %d releasing lock %x"), m_currentOwner, &m_CritSec));
        }

        m_currentOwner = 0;
    }
    LeaveCriticalSection(&m_CritSec);
}

void WINAPI DbgLockTrace(CCritSec *pcCrit, BOOL fTrace)
{
    pcCrit->m_fTrace = fTrace;
}

BOOL WINAPI CritCheckIn(CCritSec *pcCrit)
{
    return (GetCurrentThreadId() == pcCrit->m_currentOwner);
}

BOOL WINAPI CritCheckIn(const CCritSec *pcCrit)
{
    return (GetCurrentThreadId() == pcCrit->m_currentOwner);
}

BOOL WINAPI CritCheckOut(CCritSec *pcCrit)
{
    return (GetCurrentThreadId() != pcCrit->m_currentOwner);
}

BOOL WINAPI CritCheckOut(const CCritSec *pcCrit)
{
    return (GetCurrentThreadId() != pcCrit->m_currentOwner);
}
#endif

STDAPI WriteBSTR(__deref_out BSTR *pstrDest, LPCWSTR szSrc)
{
    *pstrDest = SysAllocString(szSrc);
    if (!(*pstrDest))
        return E_OUTOFMEMORY;
    return NOERROR;
}

STDAPI FreeBSTR(__deref_in BSTR *pstr)
{
    if ((PVOID)*pstr == NULL)
        return S_FALSE;
    SysFreeString(*pstr);
    return NOERROR;
}

// Return a wide string - allocating memory for it
// Returns:
//    S_OK          - no error
//    E_POINTER     - ppszReturn == NULL
//    E_OUTOFMEMORY - can't allocate memory for returned string
STDAPI AMGetWideString(LPCWSTR psz, __deref_out LPWSTR *ppszReturn)
{
    CheckPointer(ppszReturn, E_POINTER);
    ValidateReadWritePtr(ppszReturn, sizeof(LPWSTR));
    *ppszReturn = NULL;
    size_t nameLen;
    HRESULT hr = StringCbLengthW(psz, 100000, &nameLen);
    if (FAILED(hr))
    {
        return hr;
    }
    *ppszReturn = (LPWSTR)CoTaskMemAlloc(nameLen + sizeof(WCHAR));
    if (*ppszReturn == NULL)
    {
        return E_OUTOFMEMORY;
    }
    CopyMemory(*ppszReturn, psz, nameLen + sizeof(WCHAR));
    return NOERROR;
}

// Waits for the HANDLE hObject.  While waiting messages sent
// to windows on our thread by SendMessage will be processed.
// Using this function to do waits and mutual exclusion
// avoids some deadlocks in objects with windows.
// Return codes are the same as for WaitForSingleObject
DWORD WINAPI WaitDispatchingMessages(HANDLE hObject, DWORD dwWait, HWND hwnd, UINT uMsg, HANDLE hEvent)
{
    BOOL bPeeked = FALSE;
    DWORD dwResult;
    DWORD dwStart;
    DWORD dwThreadPriority;

    static UINT uMsgId = 0;

    HANDLE hObjects[2] = {hObject, hEvent};
    if (dwWait != INFINITE && dwWait != 0)
    {
        dwStart = GetTickCount();
    }
    for (;;)
    {
        DWORD nCount = NULL != hEvent ? 2 : 1;

        //  Minimize the chance of actually dispatching any messages
        //  by seeing if we can lock immediately.
        dwResult = WaitForMultipleObjects(nCount, hObjects, FALSE, 0);
        if (dwResult < WAIT_OBJECT_0 + nCount)
        {
            break;
        }

        DWORD dwTimeOut = dwWait;
        if (dwTimeOut > 10)
        {
            dwTimeOut = 10;
        }
        dwResult = MsgWaitForMultipleObjects(nCount, hObjects, FALSE, dwTimeOut,
                                             hwnd == NULL ? QS_SENDMESSAGE : QS_SENDMESSAGE + QS_POSTMESSAGE);
        if (dwResult == WAIT_OBJECT_0 + nCount || dwResult == WAIT_TIMEOUT && dwTimeOut != dwWait)
        {
            MSG msg;
            if (hwnd != NULL)
            {
                while (PeekMessage(&msg, hwnd, uMsg, uMsg, PM_REMOVE))
                {
                    DispatchMessage(&msg);
                }
            }
            // Do this anyway - the previous peek doesn't flush out the
            // messages
            PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

            if (dwWait != INFINITE && dwWait != 0)
            {
                DWORD dwNow = GetTickCount();

                // Working with differences handles wrap-around
                DWORD dwDiff = dwNow - dwStart;
                if (dwDiff > dwWait)
                {
                    dwWait = 0;
                }
                else
                {
                    dwWait -= dwDiff;
                }
                dwStart = dwNow;
            }
            if (!bPeeked)
            {
                //  Raise our priority to prevent our message queue
                //  building up
                dwThreadPriority = GetThreadPriority(GetCurrentThread());
                if (dwThreadPriority < THREAD_PRIORITY_HIGHEST)
                {
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
                }
                bPeeked = TRUE;
            }
        }
        else
        {
            break;
        }
    }
    if (bPeeked)
    {
        SetThreadPriority(GetCurrentThread(), dwThreadPriority);
        if (HIWORD(GetQueueStatus(QS_POSTMESSAGE)) & QS_POSTMESSAGE)
        {
            if (uMsgId == 0)
            {
                uMsgId = RegisterWindowMessage(TEXT("AMUnblock"));
            }
            if (uMsgId != 0)
            {
                MSG msg;
                //  Remove old ones
                while (PeekMessage(&msg, (HWND)-1, uMsgId, uMsgId, PM_REMOVE))
                {
                }
            }
            PostThreadMessage(GetCurrentThreadId(), uMsgId, 0, 0);
        }
    }
    return dwResult;
}

HRESULT AmGetLastErrorToHResult()
{
    DWORD dwLastError = GetLastError();
    if (dwLastError != 0)
    {
        return HRESULT_FROM_WIN32(dwLastError);
    }
    else
    {
        return E_FAIL;
    }
}

IUnknown *QzAtlComPtrAssign(__deref_inout_opt IUnknown **pp, __in_opt IUnknown *lp)
{
    if (lp != NULL)
        lp->AddRef();
    if (*pp)
        (*pp)->Release();
    *pp = lp;
    return lp;
}
