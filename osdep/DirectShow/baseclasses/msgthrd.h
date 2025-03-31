//------------------------------------------------------------------------------
// File: MsgThrd.h
//
// Desc: DirectShow base classes - provides support for a worker thread
//       class to which one can asynchronously post messages.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

// Message class - really just a structure.
//
class CMsg
{
  public:
    UINT uMsg;
    DWORD dwFlags;
    LPVOID lpParam;
    CAMEvent *pEvent;

    CMsg(UINT u, DWORD dw, __inout_opt LPVOID lp, __in_opt CAMEvent *pEvnt)
        : uMsg(u)
        , dwFlags(dw)
        , lpParam(lp)
        , pEvent(pEvnt)
    {
    }

    CMsg()
        : uMsg(0)
        , dwFlags(0L)
        , lpParam(NULL)
        , pEvent(NULL)
    {
    }
};

// This is the actual thread class.  It exports all the usual thread control
// functions.  The created thread is different from a normal WIN32 thread in
// that it is prompted to perform particaular tasks by responding to messages
// posted to its message queue.
//
class AM_NOVTABLE CMsgThread
{
  private:
    static DWORD WINAPI DefaultThreadProc(__inout LPVOID lpParam);
    DWORD m_ThreadId;
    HANDLE m_hThread;

  protected:
    // if you want to override GetThreadMsg to block on other things
    // as well as this queue, you need access to this
    CGenericList<CMsg> m_ThreadQueue;
    CCritSec m_Lock;
    HANDLE m_hSem;
    LONG m_lWaiting;

  public:
    CMsgThread()
        : m_ThreadId(0)
        , m_hThread(NULL)
        , m_lWaiting(0)
        , m_hSem(NULL)
        ,
        // make a list with a cache of 5 items
        m_ThreadQueue(NAME("MsgThread list"), 5)
    {
    }

    ~CMsgThread();
    // override this if you want to block on other things as well
    // as the message loop
    void virtual GetThreadMsg(__out CMsg *msg);

    // override this if you want to do something on thread startup
    virtual void OnThreadInit(){};

    BOOL CreateThread();

    BOOL WaitForThreadExit(__out LPDWORD lpdwExitCode)
    {
        if (m_hThread != NULL)
        {
            WaitForSingleObject(m_hThread, INFINITE);
            return GetExitCodeThread(m_hThread, lpdwExitCode);
        }
        return FALSE;
    }

    DWORD ResumeThread() { return ::ResumeThread(m_hThread); }

    DWORD SuspendThread() { return ::SuspendThread(m_hThread); }

    int GetThreadPriority() { return ::GetThreadPriority(m_hThread); }

    BOOL SetThreadPriority(int nPriority) { return ::SetThreadPriority(m_hThread, nPriority); }

    HANDLE GetThreadHandle() { return m_hThread; }

    DWORD GetThreadId() { return m_ThreadId; }

    void PutThreadMsg(UINT uMsg, DWORD dwMsgFlags, __in_opt LPVOID lpMsgParam, __in_opt CAMEvent *pEvent = NULL)
    {
        CAutoLock lck(&m_Lock);
        CMsg *pMsg = new CMsg(uMsg, dwMsgFlags, lpMsgParam, pEvent);
        m_ThreadQueue.AddTail(pMsg);
        if (m_lWaiting != 0)
        {
            ReleaseSemaphore(m_hSem, m_lWaiting, 0);
            m_lWaiting = 0;
        }
    }

    // This is the function prototype of the function that the client
    // supplies.  It is always called on the created thread, never on
    // the creator thread.
    //
    virtual LRESULT ThreadMessageProc(UINT uMsg, DWORD dwFlags, __inout_opt LPVOID lpParam,
                                      __in_opt CAMEvent *pEvent) = 0;
};
