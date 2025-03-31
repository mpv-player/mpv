//------------------------------------------------------------------------------
// File: WinUtil.cpp
//
// Desc: DirectShow base classes - implements generic window handler class.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <limits.h>
#include <dvdmedia.h>
#include <strsafe.h>
#include <checkbmi.h>

static UINT MsgDestroy;

// Constructor

CBaseWindow::CBaseWindow(BOOL bDoGetDC, bool bDoPostToDestroy)
    : m_hInstance(g_hInst)
    , m_hwnd(NULL)
    , m_hdc(NULL)
    , m_bActivated(FALSE)
    , m_pClassName(NULL)
    , m_ClassStyles(0)
    , m_WindowStyles(0)
    , m_WindowStylesEx(0)
    , m_ShowStageMessage(0)
    , m_ShowStageTop(0)
    , m_MemoryDC(NULL)
    , m_hPalette(NULL)
    , m_bBackground(FALSE)
    ,
#ifdef DEBUG
    m_bRealizing(FALSE)
    ,
#endif
    m_bNoRealize(FALSE)
    , m_bDoPostToDestroy(bDoPostToDestroy)
{
    m_bDoGetDC = bDoGetDC;
}

// Prepare a window by spinning off a worker thread to do the creation and
// also poll the message input queue. We leave this to be called by derived
// classes because they might want to override methods like MessageLoop and
// InitialiseWindow, if we do this during construction they'll ALWAYS call
// this base class methods. We make the worker thread create the window so
// it owns it rather than the filter graph thread which is constructing us

HRESULT CBaseWindow::PrepareWindow()
{
    if (m_hwnd)
        return NOERROR;
    ASSERT(m_hwnd == NULL);
    ASSERT(m_hdc == NULL);

    // Get the derived object's window and class styles

    m_pClassName = GetClassWindowStyles(&m_ClassStyles, &m_WindowStyles, &m_WindowStylesEx);
    if (m_pClassName == NULL)
    {
        return E_FAIL;
    }

    // Register our special private messages
    m_ShowStageMessage = RegisterWindowMessage(SHOWSTAGE);

    // RegisterWindowMessage() returns 0 if an error occurs.
    if (0 == m_ShowStageMessage)
    {
        return AmGetLastErrorToHResult();
    }

    m_ShowStageTop = RegisterWindowMessage(SHOWSTAGETOP);
    if (0 == m_ShowStageTop)
    {
        return AmGetLastErrorToHResult();
    }

    m_RealizePalette = RegisterWindowMessage(REALIZEPALETTE);
    if (0 == m_RealizePalette)
    {
        return AmGetLastErrorToHResult();
    }

    MsgDestroy = RegisterWindowMessage(TEXT("AM_DESTROY"));
    if (0 == MsgDestroy)
    {
        return AmGetLastErrorToHResult();
    }

    return DoCreateWindow();
}

// Destructor just a placeholder so that we know it becomes virtual
// Derived classes MUST call DoneWithWindow in their destructors so
// that no messages arrive after the derived class constructor ends

#ifdef DEBUG
CBaseWindow::~CBaseWindow()
{
    ASSERT(m_hwnd == NULL);
    ASSERT(m_hdc == NULL);
}
#endif

// We use the sync worker event to have the window destroyed. All we do is
// signal the event and wait on the window thread handle. Trying to send it
// messages causes too many problems, furthermore to be on the safe side we
// just wait on the thread handle while it returns WAIT_TIMEOUT or there is
// a sent message to process on this thread. If the constructor failed to
// create the thread in the first place then the loop will get terminated

HRESULT CBaseWindow::DoneWithWindow()
{
    if (!IsWindow(m_hwnd) || (GetWindowThreadProcessId(m_hwnd, NULL) != GetCurrentThreadId()))
    {

        if (IsWindow(m_hwnd))
        {

            // This code should only be executed if the window exists and if the window's
            // messages are processed on a different thread.
            ASSERT(GetWindowThreadProcessId(m_hwnd, NULL) != GetCurrentThreadId());

            if (m_bDoPostToDestroy)
            {

                HRESULT hr = S_OK;
                CAMEvent m_evDone(FALSE, &hr);
                if (FAILED(hr))
                {
                    return hr;
                }

                //  We must post a message to destroy the window
                //  That way we can't be in the middle of processing a
                //  message posted to our window when we do go away
                //  Sending a message gives less synchronization.
                PostMessage(m_hwnd, MsgDestroy, (WPARAM)(HANDLE)m_evDone, 0);
                WaitDispatchingMessages(m_evDone, INFINITE);
            }
            else
            {
                SendMessage(m_hwnd, MsgDestroy, 0, 0);
            }
        }

        //
        // This is not a leak, the window manager automatically free's
        // hdc's that were got via GetDC, which is the case here.
        // We set it to NULL so that we don't get any asserts later.
        //
        m_hdc = NULL;

        //
        // We need to free this DC though because USER32 does not know
        // anything about it.
        //
        if (m_MemoryDC)
        {
            EXECUTE_ASSERT(DeleteDC(m_MemoryDC));
            m_MemoryDC = NULL;
        }

        // Reset the window variables
        m_hwnd = NULL;

        return NOERROR;
    }
    const HWND hwnd = m_hwnd;
    if (hwnd == NULL)
    {
        return NOERROR;
    }

    InactivateWindow();
    NOTE("Inactivated");

    // Reset the window styles before destruction

    SetWindowLong(hwnd, GWL_STYLE, m_WindowStyles);
    ASSERT(GetParent(hwnd) == NULL);
    NOTE1("Reset window styles %d", m_WindowStyles);

    //  UnintialiseWindow sets m_hwnd to NULL so save a copy
    UninitialiseWindow();
    DbgLog((LOG_TRACE, 2, TEXT("Destroying 0x%8.8X"), hwnd));
    if (!DestroyWindow(hwnd))
    {
        DbgLog((LOG_TRACE, 0, TEXT("DestroyWindow %8.8X failed code %d"), hwnd, GetLastError()));
        DbgBreak("");
    }

    // Reset our state so we can be prepared again

    m_pClassName = NULL;
    m_ClassStyles = 0;
    m_WindowStyles = 0;
    m_WindowStylesEx = 0;
    m_ShowStageMessage = 0;
    m_ShowStageTop = 0;

    return NOERROR;
}

// Called at the end to put the window in an inactive state. The pending list
// will always have been cleared by this time so event if the worker thread
// gets has been signaled and gets in to render something it will find both
// the state has been changed and that there are no available sample images
// Since we wait on the window thread to complete we don't lock the object

HRESULT CBaseWindow::InactivateWindow()
{
    // Has the window been activated
    if (m_bActivated == FALSE)
    {
        return S_FALSE;
    }

    m_bActivated = FALSE;
    ShowWindow(m_hwnd, SW_HIDE);
    return NOERROR;
}

HRESULT CBaseWindow::CompleteConnect()
{
    m_bActivated = FALSE;
    return NOERROR;
}

// This displays a normal window. We ask the base window class for default
// sizes which unless overriden will return DEFWIDTH and DEFHEIGHT. We go
// through a couple of extra hoops to get the client area the right size
// as the object specifies which accounts for the AdjustWindowRectEx calls
// We also DWORD align the left and top coordinates of the window here to
// maximise the chance of being able to use DCI/DirectDraw primary surface

HRESULT CBaseWindow::ActivateWindow()
{
    // Has the window been sized and positioned already

    if (m_bActivated == TRUE || GetParent(m_hwnd) != NULL)
    {

        SetWindowPos(m_hwnd,          // Our window handle
                     HWND_TOP,        // Put it at the top
                     0, 0, 0, 0,      // Leave in current position
                     SWP_NOMOVE |     // Don't change it's place
                         SWP_NOSIZE); // Change Z-order only

        m_bActivated = TRUE;
        return S_FALSE;
    }

    // Calculate the desired client rectangle

    RECT WindowRect, ClientRect = GetDefaultRect();
    GetWindowRect(m_hwnd, &WindowRect);
    AdjustWindowRectEx(&ClientRect, GetWindowLong(m_hwnd, GWL_STYLE), FALSE, GetWindowLong(m_hwnd, GWL_EXSTYLE));

    // Align left and top edges on DWORD boundaries

    UINT WindowFlags = (SWP_NOACTIVATE | SWP_FRAMECHANGED);
    WindowRect.left -= (WindowRect.left & 3);
    WindowRect.top -= (WindowRect.top & 3);

    SetWindowPos(m_hwnd,              // Window handle
                 HWND_TOP,            // Put it at the top
                 WindowRect.left,     // Align left edge
                 WindowRect.top,      // And also top place
                 WIDTH(&ClientRect),  // Horizontal size
                 HEIGHT(&ClientRect), // Vertical size
                 WindowFlags);        // Don't show window

    m_bActivated = TRUE;
    return NOERROR;
}

// This can be used to DWORD align the window for maximum performance

HRESULT CBaseWindow::PerformanceAlignWindow()
{
    RECT ClientRect, WindowRect;
    GetWindowRect(m_hwnd, &WindowRect);
    ASSERT(m_bActivated == TRUE);

    // Don't do this if we're owned

    if (GetParent(m_hwnd))
    {
        return NOERROR;
    }

    // Align left and top edges on DWORD boundaries

    GetClientRect(m_hwnd, &ClientRect);
    MapWindowPoints(m_hwnd, HWND_DESKTOP, (LPPOINT)&ClientRect, 2);
    WindowRect.left -= (ClientRect.left & 3);
    WindowRect.top -= (ClientRect.top & 3);
    UINT WindowFlags = (SWP_NOACTIVATE | SWP_NOSIZE);

    SetWindowPos(m_hwnd,          // Window handle
                 HWND_TOP,        // Put it at the top
                 WindowRect.left, // Align left edge
                 WindowRect.top,  // And also top place
                 (int)0, (int)0,  // Ignore these sizes
                 WindowFlags);    // Don't show window

    return NOERROR;
}

// Install a palette into the base window - we may be called by a different
// thread to the one that owns the window. We have to be careful how we do
// the palette realisation as we could be a different thread to the window
// which would cause an inter thread send message. Therefore we realise the
// palette by sending it a special message but without the window locked

HRESULT CBaseWindow::SetPalette(HPALETTE hPalette)
{
    // We must own the window lock during the change
    {
        CAutoLock cWindowLock(&m_WindowLock);
        CAutoLock cPaletteLock(&m_PaletteLock);
        ASSERT(hPalette);
        m_hPalette = hPalette;
    }
    return SetPalette();
}

HRESULT CBaseWindow::SetPalette()
{
    if (!m_bNoRealize)
    {
        SendMessage(m_hwnd, m_RealizePalette, 0, 0);
        return S_OK;
    }
    else
    {
        // Just select the palette
        ASSERT(m_hdc);
        ASSERT(m_MemoryDC);

        CAutoLock cPaletteLock(&m_PaletteLock);
        SelectPalette(m_hdc, m_hPalette, m_bBackground);
        SelectPalette(m_MemoryDC, m_hPalette, m_bBackground);

        return S_OK;
    }
}

void CBaseWindow::UnsetPalette()
{
    CAutoLock cWindowLock(&m_WindowLock);
    CAutoLock cPaletteLock(&m_PaletteLock);

    // Get a standard VGA colour palette

    HPALETTE hPalette = (HPALETTE)GetStockObject(DEFAULT_PALETTE);
    ASSERT(hPalette);

    SelectPalette(GetWindowHDC(), hPalette, TRUE);
    SelectPalette(GetMemoryHDC(), hPalette, TRUE);

    m_hPalette = NULL;
}

void CBaseWindow::LockPaletteLock()
{
    m_PaletteLock.Lock();
}

void CBaseWindow::UnlockPaletteLock()
{
    m_PaletteLock.Unlock();
}

// Realise our palettes in the window and device contexts

HRESULT CBaseWindow::DoRealisePalette(BOOL bForceBackground)
{
    {
        CAutoLock cPaletteLock(&m_PaletteLock);

        if (m_hPalette == NULL)
        {
            return NOERROR;
        }

        // Realize the palette on the window thread
        ASSERT(m_hdc);
        ASSERT(m_MemoryDC);

        SelectPalette(m_hdc, m_hPalette, m_bBackground || bForceBackground);
        SelectPalette(m_MemoryDC, m_hPalette, m_bBackground);
    }

    //  If we grab a critical section here we can deadlock
    //  with the window thread because one of the side effects
    //  of RealizePalette is to send a WM_PALETTECHANGED message
    //  to every window in the system.  In our handling
    //  of WM_PALETTECHANGED we used to grab this CS too.
    //  The really bad case is when our renderer calls DoRealisePalette()
    //  while we're in the middle of processing a palette change
    //  for another window.
    //  So don't hold the critical section while actually realising
    //  the palette.  In any case USER is meant to manage palette
    //  handling - we shouldn't have to serialize everything as well
    ASSERT(CritCheckOut(&m_WindowLock));
    ASSERT(CritCheckOut(&m_PaletteLock));

    EXECUTE_ASSERT(RealizePalette(m_hdc) != GDI_ERROR);
    EXECUTE_ASSERT(RealizePalette(m_MemoryDC) != GDI_ERROR);

    return (GdiFlush() == FALSE ? S_FALSE : S_OK);
}

// This is the global window procedure

LRESULT CALLBACK WndProc(HWND hwnd,     // Window handle
                         UINT uMsg,     // Message ID
                         WPARAM wParam, // First parameter
                         LPARAM lParam) // Other parameter
{

    // Get the window long that holds our window object pointer
    // If it is NULL then we are initialising the window in which
    // case the object pointer has been passed in the window creation
    // structure.  IF we get any messages before WM_NCCREATE we will
    // pass them to DefWindowProc.

    CBaseWindow *pBaseWindow = _GetWindowLongPtr<CBaseWindow *>(hwnd, 0);

    if (pBaseWindow == NULL)
    {

        // Get the structure pointer from the create struct.
        // We can only do this for WM_NCCREATE which should be one of
        // the first messages we receive.  Anything before this will
        // have to be passed to DefWindowProc (i.e. WM_GETMINMAXINFO)

        // If the message is WM_NCCREATE we set our pBaseWindow pointer
        // and will then place it in the window structure

        // turn off WS_EX_LAYOUTRTL style for quartz windows
        if (uMsg == WM_NCCREATE)
        {
            SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~0x400000);
        }

        if ((uMsg != WM_NCCREATE) ||
            (NULL == (pBaseWindow = *(CBaseWindow **)((LPCREATESTRUCT)lParam)->lpCreateParams)))
        {
            return (DefWindowProc(hwnd, uMsg, wParam, lParam));
        }

        // Set the window LONG to be the object who created us
#ifdef DEBUG
        SetLastError(0); // because of the way SetWindowLong works
#endif

        LONG_PTR rc = _SetWindowLongPtr(hwnd, (DWORD)0, pBaseWindow);

#ifdef DEBUG
        if (0 == rc)
        {
            // SetWindowLong MIGHT have failed.  (Read the docs which admit
            // that it is awkward to work out if you have had an error.)
            LONG lasterror = GetLastError();
            ASSERT(0 == lasterror);
            // If this is not the case we have not set the pBaseWindow pointer
            // into the window structure and we will blow up.
        }
#endif
    }
    // See if this is the packet of death
    if (uMsg == MsgDestroy && uMsg != 0)
    {
        pBaseWindow->DoneWithWindow();
        if (pBaseWindow->m_bDoPostToDestroy)
        {
            EXECUTE_ASSERT(SetEvent((HANDLE)wParam));
        }
        return 0;
    }
    return pBaseWindow->OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

// When the window size changes we adjust our member variables that
// contain the dimensions of the client rectangle for our window so
// that we come to render an image we will know whether to stretch

BOOL CBaseWindow::OnSize(LONG Width, LONG Height)
{
    m_Width = Width;
    m_Height = Height;
    return TRUE;
}

// This function handles the WM_CLOSE message

BOOL CBaseWindow::OnClose()
{
    ShowWindow(m_hwnd, SW_HIDE);
    return TRUE;
}

// This is called by the worker window thread when it receives a terminate
// message from the window object destructor to delete all the resources we
// allocated during initialisation. By the time the worker thread exits all
// processing will have been completed as the source filter disconnection
// flushes the image pending sample, therefore the GdiFlush should succeed

HRESULT CBaseWindow::UninitialiseWindow()
{
    // Have we already cleaned up

    if (m_hwnd == NULL)
    {
        ASSERT(m_hdc == NULL);
        ASSERT(m_MemoryDC == NULL);
        return NOERROR;
    }

    // Release the window resources

    EXECUTE_ASSERT(GdiFlush());

    if (m_hdc)
    {
        EXECUTE_ASSERT(ReleaseDC(m_hwnd, m_hdc));
        m_hdc = NULL;
    }

    if (m_MemoryDC)
    {
        EXECUTE_ASSERT(DeleteDC(m_MemoryDC));
        m_MemoryDC = NULL;
    }

    // Reset the window variables
    m_hwnd = NULL;

    return NOERROR;
}

// This is called by the worker window thread after it has created the main
// window and it wants to initialise the rest of the owner objects window
// variables such as the device contexts. We execute this function with the
// critical section still locked. Nothing in this function must generate any
// SendMessage calls to the window because this is executing on the window
// thread so the message will never be processed and we will deadlock

HRESULT CBaseWindow::InitialiseWindow(HWND hwnd)
{
    // Initialise the window variables

    ASSERT(IsWindow(hwnd));
    m_hwnd = hwnd;

    if (m_bDoGetDC)
    {
        EXECUTE_ASSERT(m_hdc = GetDC(hwnd));
        EXECUTE_ASSERT(m_MemoryDC = CreateCompatibleDC(m_hdc));

        EXECUTE_ASSERT(SetStretchBltMode(m_hdc, COLORONCOLOR));
        EXECUTE_ASSERT(SetStretchBltMode(m_MemoryDC, COLORONCOLOR));
    }

    return NOERROR;
}

HRESULT CBaseWindow::DoCreateWindow()
{
    WNDCLASS wndclass; // Used to register classes
    BOOL bRegistered;  // Is this class registered
    HWND hwnd;         // Handle to our window

    bRegistered = GetClassInfo(m_hInstance,  // Module instance
                               m_pClassName, // Window class
                               &wndclass);   // Info structure

    // if the window is to be used for drawing puposes and we are getting a DC
    // for the entire lifetime of the window then changes the class style to do
    // say so. If we don't set this flag then the DC comes from the cache and is
    // really bad.
    if (m_bDoGetDC)
    {
        m_ClassStyles |= CS_OWNDC;
    }

    if (bRegistered == FALSE)
    {

        // Register the renderer window class

        wndclass.lpszClassName = m_pClassName;
        wndclass.style = m_ClassStyles;
        wndclass.lpfnWndProc = WndProc;
        wndclass.cbClsExtra = 0;
        wndclass.cbWndExtra = sizeof(CBaseWindow *);
        wndclass.hInstance = m_hInstance;
        wndclass.hIcon = NULL;
        wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
        wndclass.hbrBackground = (HBRUSH)NULL;
        wndclass.lpszMenuName = NULL;

        RegisterClass(&wndclass);
    }

    // Create the frame window.  Pass the pBaseWindow information in the
    // CreateStruct which allows our message handling loop to get hold of
    // the pBaseWindow pointer.

    CBaseWindow *pBaseWindow = this;                  // The owner window object
    hwnd = CreateWindowEx(m_WindowStylesEx,           // Extended styles
                          m_pClassName,               // Registered name
                          TEXT("ActiveMovie Window"), // Window title
                          m_WindowStyles,             // Window styles
                          CW_USEDEFAULT,              // Start x position
                          CW_USEDEFAULT,              // Start y position
                          DEFWIDTH,                   // Window width
                          DEFHEIGHT,                  // Window height
                          NULL,                       // Parent handle
                          NULL,                       // Menu handle
                          m_hInstance,                // Instance handle
                          &pBaseWindow);              // Creation data

    // If we failed signal an error to the object constructor (based on the
    // last Win32 error on this thread) then signal the constructor thread
    // to continue, release the mutex to let others have a go and exit

    if (hwnd == NULL)
    {
        DWORD Error = GetLastError();
        return AmHresultFromWin32(Error);
    }

    // Check the window LONG is the object who created us
    ASSERT(GetWindowLongPtr(hwnd, 0) == (LONG_PTR)this);

    // Initialise the window and then signal the constructor so that it can
    // continue and then finally unlock the object's critical section. The
    // window class is left registered even after we terminate the thread
    // as we don't know when the last window has been closed. So we allow
    // the operating system to free the class resources as appropriate

    InitialiseWindow(hwnd);

    DbgLog((LOG_TRACE, 2, TEXT("Created window class (%s) HWND(%8.8X)"), m_pClassName, hwnd));

    return S_OK;
}

// The base class provides some default handling and calls DefWindowProc

LRESULT CBaseWindow::OnReceiveMessage(HWND hwnd,     // Window handle
                                      UINT uMsg,     // Message ID
                                      WPARAM wParam, // First parameter
                                      LPARAM lParam) // Other parameter
{
    ASSERT(IsWindow(hwnd));

    if (PossiblyEatMessage(uMsg, wParam, lParam))
        return 0;

    // This is sent by the IVideoWindow SetWindowForeground method. If the
    // window is invisible we will show it and make it topmost without the
    // foreground focus. If the window is visible it will also be made the
    // topmost window without the foreground focus. If wParam is TRUE then
    // for both cases the window will be forced into the foreground focus

    if (uMsg == m_ShowStageMessage)
    {

        BOOL bVisible = IsWindowVisible(hwnd);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | (bVisible ? SWP_NOACTIVATE : 0));

        // Should we bring the window to the foreground
        if (wParam == TRUE)
        {
            SetForegroundWindow(hwnd);
        }
        return (LRESULT)1;
    }

    // When we go fullscreen we have to add the WS_EX_TOPMOST style to the
    // video window so that it comes out above any task bar (this is more
    // relevant to WindowsNT than Windows95). However the SetWindowPos call
    // must be on the same thread as that which created the window. The
    // wParam parameter can be TRUE or FALSE to set and reset the topmost

    if (uMsg == m_ShowStageTop)
    {
        HWND HwndTop = (wParam == TRUE ? HWND_TOPMOST : HWND_NOTOPMOST);
        BOOL bVisible = IsWindowVisible(hwnd);
        SetWindowPos(hwnd, HwndTop, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | (wParam == TRUE ? SWP_SHOWWINDOW : 0) | (bVisible ? SWP_NOACTIVATE : 0));
        return (LRESULT)1;
    }

    // New palette stuff
    if (uMsg == m_RealizePalette)
    {
        ASSERT(m_hwnd == hwnd);
        return OnPaletteChange(m_hwnd, WM_QUERYNEWPALETTE);
    }

    switch (uMsg)
    {

        // Repaint the window if the system colours change

    case WM_SYSCOLORCHANGE: InvalidateRect(hwnd, NULL, FALSE); return (LRESULT)1;

    // Somebody has changed the palette
    case WM_PALETTECHANGED:

        OnPaletteChange((HWND)wParam, uMsg);
        return (LRESULT)0;

        // We are about to receive the keyboard focus so we ask GDI to realise
        // our logical palette again and hopefully it will be fully installed
        // without any mapping having to be done during any picture rendering

    case WM_QUERYNEWPALETTE: ASSERT(m_hwnd == hwnd); return OnPaletteChange(m_hwnd, uMsg);

    // do NOT fwd WM_MOVE. the parameters are the location of the parent
    // window, NOT what the renderer should be looking at.  But we need
    // to make sure the overlay is moved with the parent window, so we
    // do this.
    case WM_MOVE:
        if (IsWindowVisible(m_hwnd))
        {
            PostMessage(m_hwnd, WM_PAINT, 0, 0);
        }
        break;

        // Store the width and height as useful base class members

    case WM_SIZE:

        OnSize(LOWORD(lParam), HIWORD(lParam));
        return (LRESULT)0;

        // Intercept the WM_CLOSE messages to hide the window

    case WM_CLOSE: OnClose(); return (LRESULT)0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// This handles the Windows palette change messages - if we do realise our
// palette then we return TRUE otherwise we return FALSE. If our window is
// foreground application then we should get first choice of colours in the
// system palette entries. We get best performance when our logical palette
// includes the standard VGA colours (at the beginning and end) otherwise
// GDI may have to map from our palette to the device palette while drawing

LRESULT CBaseWindow::OnPaletteChange(HWND hwnd, UINT Message)
{
    // First check we are not changing the palette during closedown

    if (m_hwnd == NULL || hwnd == NULL)
    {
        return (LRESULT)0;
    }
    ASSERT(!m_bRealizing);

    // Should we realise our palette again

    if ((Message == WM_QUERYNEWPALETTE || hwnd != m_hwnd))
    {
        //  It seems that even if we're invisible that we can get asked
        //  to realize our palette and this can cause really ugly side-effects
        //  Seems like there's another bug but this masks it a least for the
        //  shutting down case.
        if (!IsWindowVisible(m_hwnd))
        {
            DbgLog((LOG_TRACE, 1, TEXT("Realizing when invisible!")));
            return (LRESULT)0;
        }

        // Avoid recursion with multiple graphs in the same app
#ifdef DEBUG
        m_bRealizing = TRUE;
#endif
        DoRealisePalette(Message != WM_QUERYNEWPALETTE);
#ifdef DEBUG
        m_bRealizing = FALSE;
#endif

        // Should we redraw the window with the new palette
        if (Message == WM_PALETTECHANGED)
        {
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
    }

    return (LRESULT)1;
}

// Determine if the window exists.

bool CBaseWindow::WindowExists()
{
    return !!IsWindow(m_hwnd);
}

// Return the default window rectangle

RECT CBaseWindow::GetDefaultRect()
{
    RECT DefaultRect = {0, 0, DEFWIDTH, DEFHEIGHT};
    ASSERT(m_hwnd);
    // ASSERT(m_hdc);
    return DefaultRect;
}

// Return the current window width

LONG CBaseWindow::GetWindowWidth()
{
    ASSERT(m_hwnd);
    // ASSERT(m_hdc);
    return m_Width;
}

// Return the current window height

LONG CBaseWindow::GetWindowHeight()
{
    ASSERT(m_hwnd);
    // ASSERT(m_hdc);
    return m_Height;
}

// Return the window handle

HWND CBaseWindow::GetWindowHWND()
{
    ASSERT(m_hwnd);
    // ASSERT(m_hdc);
    return m_hwnd;
}

// Return the window drawing device context

HDC CBaseWindow::GetWindowHDC()
{
    ASSERT(m_hwnd);
    ASSERT(m_hdc);
    return m_hdc;
}

// Return the offscreen window drawing device context

HDC CBaseWindow::GetMemoryHDC()
{
    ASSERT(m_hwnd);
    ASSERT(m_MemoryDC);
    return m_MemoryDC;
}

#ifdef DEBUG
HPALETTE CBaseWindow::GetPalette()
{
    // The palette lock should always be held when accessing
    // m_hPalette.
    ASSERT(CritCheckIn(&m_PaletteLock));
    return m_hPalette;
}
#endif // DEBUG

// This is available to clients who want to change the window visiblity. It's
// little more than an indirection to the Win32 ShowWindow although these is
// some benefit in going through here as this function may change sometime

HRESULT CBaseWindow::DoShowWindow(LONG ShowCmd)
{
    ShowWindow(m_hwnd, ShowCmd);
    return NOERROR;
}

// Generate a WM_PAINT message for the video window

void CBaseWindow::PaintWindow(BOOL bErase)
{
    InvalidateRect(m_hwnd, NULL, bErase);
}

// Allow an application to have us set the video window in the foreground. We
// have this because it is difficult for one thread to do do this to a window
// owned by another thread. Rather than expose the message we use to execute
// the inter thread send message we provide the interface function. All we do
// is to SendMessage to the video window renderer thread with a WM_SHOWSTAGE

void CBaseWindow::DoSetWindowForeground(BOOL bFocus)
{
    SendMessage(m_hwnd, m_ShowStageMessage, (WPARAM)bFocus, (LPARAM)0);
}

// Constructor initialises the owning object pointer. Since we are a worker
// class for the main window object we have relatively few state variables to
// look after. We are given device context handles to use later on as well as
// the source and destination rectangles (but reset them here just in case)

CDrawImage::CDrawImage(__inout CBaseWindow *pBaseWindow)
    : m_pBaseWindow(pBaseWindow)
    , m_hdc(NULL)
    , m_MemoryDC(NULL)
    , m_bStretch(FALSE)
    , m_pMediaType(NULL)
    , m_bUsingImageAllocator(FALSE)
{
    ASSERT(pBaseWindow);
    ResetPaletteVersion();
    SetRectEmpty(&m_TargetRect);
    SetRectEmpty(&m_SourceRect);

    m_perfidRenderTime = MSR_REGISTER(TEXT("Single Blt time"));
}

// Overlay the image time stamps on the picture. Access to this method is
// serialised by the caller. We display the sample start and end times on
// top of the video using TextOut on the device context we are handed. If
// there isn't enough room in the window for the times we don't show them

void CDrawImage::DisplaySampleTimes(IMediaSample *pSample)
{
#ifdef DEBUG
    //
    // Only allow the "annoying" time messages if the users has turned the
    // logging "way up"
    //
    BOOL bAccept = DbgCheckModuleLevel(LOG_TRACE, 5);
    if (bAccept == FALSE)
    {
        return;
    }
#endif

    TCHAR szTimes[TIMELENGTH]; // Time stamp strings
    ASSERT(pSample);           // Quick sanity check
    RECT ClientRect;           // Client window size
    SIZE Size;                 // Size of text output

    // Get the time stamps and window size

    pSample->GetTime((REFERENCE_TIME *)&m_StartSample, (REFERENCE_TIME *)&m_EndSample);
    HWND hwnd = m_pBaseWindow->GetWindowHWND();
    EXECUTE_ASSERT(GetClientRect(hwnd, &ClientRect));

    // Format the sample time stamps

    (void)StringCchPrintf(szTimes, NUMELMS(szTimes), TEXT("%08d : %08d"), m_StartSample.Millisecs(),
                          m_EndSample.Millisecs());

    ASSERT(lstrlen(szTimes) < TIMELENGTH);

    // Put the times in the middle at the bottom of the window

    GetTextExtentPoint32(m_hdc, szTimes, lstrlen(szTimes), &Size);
    INT XPos = ((ClientRect.right - ClientRect.left) - Size.cx) / 2;
    INT YPos = ((ClientRect.bottom - ClientRect.top) - Size.cy) * 4 / 5;

    // Check the window is big enough to have sample times displayed

    if ((XPos > 0) && (YPos > 0))
    {
        TextOut(m_hdc, XPos, YPos, szTimes, lstrlen(szTimes));
    }
}

// This is called when the drawing code sees that the image has a down level
// palette cookie. We simply call the SetDIBColorTable Windows API with the
// palette that is found after the BITMAPINFOHEADER - we return no errors

void CDrawImage::UpdateColourTable(HDC hdc, __in BITMAPINFOHEADER *pbmi)
{
    ASSERT(pbmi->biClrUsed);
    RGBQUAD *pColourTable = (RGBQUAD *)(pbmi + 1);

    // Set the new palette in the device context

    UINT uiReturn = SetDIBColorTable(hdc, (UINT)0, pbmi->biClrUsed, pColourTable);

    // Should always succeed but check in debug builds
    ASSERT(uiReturn == pbmi->biClrUsed);
}

// No source rectangle scaling is done by the base class

RECT CDrawImage::ScaleSourceRect(const RECT *pSource)
{
    ASSERT(pSource);
    return *pSource;
}

// This is called when the funky output pin uses our allocator. The samples we
// allocate are special because the memory is shared between us and GDI thus
// removing one copy when we ask for the image to be rendered. The source type
// information is in the main renderer m_mtIn field which is initialised when
// the media type is agreed in SetMediaType, the media type may be changed on
// the fly if, for example, the source filter needs to change the palette

void CDrawImage::FastRender(IMediaSample *pMediaSample)
{
    BITMAPINFOHEADER *pbmi; // Image format data
    DIBDATA *pDibData;      // Stores DIB information
    BYTE *pImage;           // Pointer to image data
    HBITMAP hOldBitmap;     // Store the old bitmap
    CImageSample *pSample;  // Pointer to C++ object

    ASSERT(m_pMediaType);

    // From the untyped source format block get the VIDEOINFO and subsequently
    // the BITMAPINFOHEADER structure. We can cast the IMediaSample interface
    // to a CImageSample object so we can retrieve it's DIBSECTION details

    pbmi = HEADER(m_pMediaType->Format());
    pSample = (CImageSample *)pMediaSample;
    pDibData = pSample->GetDIBData();
    hOldBitmap = (HBITMAP)SelectObject(m_MemoryDC, pDibData->hBitmap);

    // Get a pointer to the real image data

    HRESULT hr = pMediaSample->GetPointer(&pImage);
    if (FAILED(hr))
    {
        return;
    }

    // Do we need to update the colour table, we increment our palette cookie
    // each time we get a dynamic format change. The sample palette cookie is
    // stored in the DIBDATA structure so we try to keep the fields in sync
    // By the time we get to draw the images the format change will be done
    // so all we do is ask the renderer for what it's palette version is

    if (pDibData->PaletteVersion < GetPaletteVersion())
    {
        ASSERT(pbmi->biBitCount <= iPALETTE);
        UpdateColourTable(m_MemoryDC, pbmi);
        pDibData->PaletteVersion = GetPaletteVersion();
    }

    // This allows derived classes to change the source rectangle that we do
    // the drawing with. For example a renderer may ask a codec to stretch
    // the video from 320x240 to 640x480, in which case the source we see in
    // here will still be 320x240, although the source we want to draw with
    // should be scaled up to 640x480. The base class implementation of this
    // method does nothing but return the same rectangle as we are passed in

    RECT SourceRect = ScaleSourceRect(&m_SourceRect);

    // Is the window the same size as the video

    if (m_bStretch == FALSE)
    {

        // Put the image straight into the window

        BitBlt((HDC)m_hdc,                             // Target device HDC
               m_TargetRect.left,                      // X sink position
               m_TargetRect.top,                       // Y sink position
               m_TargetRect.right - m_TargetRect.left, // Destination width
               m_TargetRect.bottom - m_TargetRect.top, // Destination height
               m_MemoryDC,                             // Source device context
               SourceRect.left,                        // X source position
               SourceRect.top,                         // Y source position
               SRCCOPY);                               // Simple copy
    }
    else
    {

        // Stretch the image when copying to the window

        StretchBlt((HDC)m_hdc,                             // Target device HDC
                   m_TargetRect.left,                      // X sink position
                   m_TargetRect.top,                       // Y sink position
                   m_TargetRect.right - m_TargetRect.left, // Destination width
                   m_TargetRect.bottom - m_TargetRect.top, // Destination height
                   m_MemoryDC,                             // Source device HDC
                   SourceRect.left,                        // X source position
                   SourceRect.top,                         // Y source position
                   SourceRect.right - SourceRect.left,     // Source width
                   SourceRect.bottom - SourceRect.top,     // Source height
                   SRCCOPY);                               // Simple copy
    }

    // This displays the sample times over the top of the image. This used to
    // draw the times into the offscreen device context however that actually
    // writes the text into the image data buffer which may not be writable

#ifdef DEBUG
    DisplaySampleTimes(pMediaSample);
#endif

    // Put the old bitmap back into the device context so we don't leak
    SelectObject(m_MemoryDC, hOldBitmap);
}

// This is called when there is a sample ready to be drawn, unfortunately the
// output pin was being rotten and didn't choose our super excellent shared
// memory DIB allocator so we have to do this slow render using boring old GDI
// SetDIBitsToDevice and StretchDIBits. The down side of using these GDI
// functions is that the image data has to be copied across from our address
// space into theirs before going to the screen (although in reality the cost
// is small because all they do is to map the buffer into their address space)

void CDrawImage::SlowRender(IMediaSample *pMediaSample)
{
    // Get the BITMAPINFOHEADER for the connection

    ASSERT(m_pMediaType);
    BITMAPINFOHEADER *pbmi = HEADER(m_pMediaType->Format());
    BYTE *pImage;

    // Get the image data buffer

    HRESULT hr = pMediaSample->GetPointer(&pImage);
    if (FAILED(hr))
    {
        return;
    }

    // This allows derived classes to change the source rectangle that we do
    // the drawing with. For example a renderer may ask a codec to stretch
    // the video from 320x240 to 640x480, in which case the source we see in
    // here will still be 320x240, although the source we want to draw with
    // should be scaled up to 640x480. The base class implementation of this
    // method does nothing but return the same rectangle as we are passed in

    RECT SourceRect = ScaleSourceRect(&m_SourceRect);

    LONG lAdjustedSourceTop = SourceRect.top;
    // if the origin of bitmap is bottom-left, adjust soruce_rect_top
    // to be the bottom-left corner instead of the top-left.
    if (pbmi->biHeight > 0)
    {
        lAdjustedSourceTop = pbmi->biHeight - SourceRect.bottom;
    }
    // Is the window the same size as the video

    if (m_bStretch == FALSE)
    {

        // Put the image straight into the window

        SetDIBitsToDevice((HDC)m_hdc,                             // Target device HDC
                          m_TargetRect.left,                      // X sink position
                          m_TargetRect.top,                       // Y sink position
                          m_TargetRect.right - m_TargetRect.left, // Destination width
                          m_TargetRect.bottom - m_TargetRect.top, // Destination height
                          SourceRect.left,                        // X source position
                          lAdjustedSourceTop,                     // Adjusted Y source position
                          (UINT)0,                                // Start scan line
                          pbmi->biHeight,                         // Scan lines present
                          pImage,                                 // Image data
                          (BITMAPINFO *)pbmi,                     // DIB header
                          DIB_RGB_COLORS);                        // Type of palette
    }
    else
    {

        // Stretch the image when copying to the window

        StretchDIBits((HDC)m_hdc,                             // Target device HDC
                      m_TargetRect.left,                      // X sink position
                      m_TargetRect.top,                       // Y sink position
                      m_TargetRect.right - m_TargetRect.left, // Destination width
                      m_TargetRect.bottom - m_TargetRect.top, // Destination height
                      SourceRect.left,                        // X source position
                      lAdjustedSourceTop,                     // Adjusted Y source position
                      SourceRect.right - SourceRect.left,     // Source width
                      SourceRect.bottom - SourceRect.top,     // Source height
                      pImage,                                 // Image data
                      (BITMAPINFO *)pbmi,                     // DIB header
                      DIB_RGB_COLORS,                         // Type of palette
                      SRCCOPY);                               // Simple image copy
    }

    // This shows the sample reference times over the top of the image which
    // looks a little flickery. I tried using GdiSetBatchLimit and GdiFlush to
    // control the screen updates but it doesn't quite work as expected and
    // only partially reduces the flicker. I also tried using a memory context
    // and combining the two in that before doing a final BitBlt operation to
    // the screen, unfortunately this has considerable performance penalties
    // and also means that this code is not executed when compiled retail

#ifdef DEBUG
    DisplaySampleTimes(pMediaSample);
#endif
}

// This is called with an IMediaSample interface on the image to be drawn. We
// decide on the drawing mechanism based on who's allocator we are using. We
// may be called when the window wants an image painted by WM_PAINT messages
// We can't realise the palette here because we have the renderer lock, any
// call to realise may cause an interthread send message to the window thread
// which may in turn be waiting to get the renderer lock before servicing it

BOOL CDrawImage::DrawImage(IMediaSample *pMediaSample)
{
    ASSERT(m_hdc);
    ASSERT(m_MemoryDC);
    NotifyStartDraw();

    // If the output pin used our allocator then the samples passed are in
    // fact CVideoSample objects that contain CreateDIBSection data that we
    // use to do faster image rendering, they may optionally also contain a
    // DirectDraw surface pointer in which case we do not do the drawing

    if (m_bUsingImageAllocator == FALSE)
    {
        SlowRender(pMediaSample);
        EXECUTE_ASSERT(GdiFlush());
        NotifyEndDraw();
        return TRUE;
    }

    // This is a DIBSECTION buffer

    FastRender(pMediaSample);
    EXECUTE_ASSERT(GdiFlush());
    NotifyEndDraw();
    return TRUE;
}

BOOL CDrawImage::DrawVideoImageHere(HDC hdc, IMediaSample *pMediaSample, __in LPRECT lprcSrc, __in LPRECT lprcDst)
{
    ASSERT(m_pMediaType);
    BITMAPINFOHEADER *pbmi = HEADER(m_pMediaType->Format());
    BYTE *pImage;

    // Get the image data buffer

    HRESULT hr = pMediaSample->GetPointer(&pImage);
    if (FAILED(hr))
    {
        return FALSE;
    }

    RECT SourceRect;
    RECT TargetRect;

    if (lprcSrc)
    {
        SourceRect = *lprcSrc;
    }
    else
        SourceRect = ScaleSourceRect(&m_SourceRect);

    if (lprcDst)
    {
        TargetRect = *lprcDst;
    }
    else
        TargetRect = m_TargetRect;

    LONG lAdjustedSourceTop = SourceRect.top;
    // if the origin of bitmap is bottom-left, adjust soruce_rect_top
    // to be the bottom-left corner instead of the top-left.
    if (pbmi->biHeight > 0)
    {
        lAdjustedSourceTop = pbmi->biHeight - SourceRect.bottom;
    }

    // Stretch the image when copying to the DC

    BOOL bRet = (0 != StretchDIBits(hdc, TargetRect.left, TargetRect.top, TargetRect.right - TargetRect.left,
                                    TargetRect.bottom - TargetRect.top, SourceRect.left, lAdjustedSourceTop,
                                    SourceRect.right - SourceRect.left, SourceRect.bottom - SourceRect.top, pImage,
                                    (BITMAPINFO *)pbmi, DIB_RGB_COLORS, SRCCOPY));
    return bRet;
}

// This is called by the owning window object after it has created the window
// and it's drawing contexts. We are constructed with the base window we'll
// be drawing into so when given the notification we retrive the device HDCs
// to draw with. We cannot call these in our constructor as they are virtual

void CDrawImage::SetDrawContext()
{
    m_MemoryDC = m_pBaseWindow->GetMemoryHDC();
    m_hdc = m_pBaseWindow->GetWindowHDC();
}

// This is called to set the target rectangle in the video window, it will be
// called whenever a WM_SIZE message is retrieved from the message queue. We
// simply store the rectangle and use it later when we do the drawing calls

void CDrawImage::SetTargetRect(__in RECT *pTargetRect)
{
    ASSERT(pTargetRect);
    m_TargetRect = *pTargetRect;
    SetStretchMode();
}

// Return the current target rectangle

void CDrawImage::GetTargetRect(__out RECT *pTargetRect)
{
    ASSERT(pTargetRect);
    *pTargetRect = m_TargetRect;
}

// This is called when we want to change the section of the image to draw. We
// use this information in the drawing operation calls later on. We must also
// see if the source and destination rectangles have the same dimensions. If
// not we must stretch during the drawing rather than a direct pixel copy

void CDrawImage::SetSourceRect(__in RECT *pSourceRect)
{
    ASSERT(pSourceRect);
    m_SourceRect = *pSourceRect;
    SetStretchMode();
}

// Return the current source rectangle

void CDrawImage::GetSourceRect(__out RECT *pSourceRect)
{
    ASSERT(pSourceRect);
    *pSourceRect = m_SourceRect;
}

// This is called when either the source or destination rectanges change so we
// can update the stretch flag. If the rectangles don't match we stretch the
// video during the drawing otherwise we call the fast pixel copy functions
// NOTE the source and/or the destination rectangle may be completely empty

void CDrawImage::SetStretchMode()
{
    // Calculate the overall rectangle dimensions

    LONG SourceWidth = m_SourceRect.right - m_SourceRect.left;
    LONG SinkWidth = m_TargetRect.right - m_TargetRect.left;
    LONG SourceHeight = m_SourceRect.bottom - m_SourceRect.top;
    LONG SinkHeight = m_TargetRect.bottom - m_TargetRect.top;

    m_bStretch = TRUE;
    if (SourceWidth == SinkWidth)
    {
        if (SourceHeight == SinkHeight)
        {
            m_bStretch = FALSE;
        }
    }
}

// Tell us whose allocator we are using. This should be called with TRUE if
// the filter agrees to use an allocator based around the CImageAllocator
// SDK base class - whose image buffers are made through CreateDIBSection.
// Otherwise this should be called with FALSE and we will draw the images
// using SetDIBitsToDevice and StretchDIBitsToDevice. None of these calls
// can handle buffers which have non zero strides (like DirectDraw uses)

void CDrawImage::NotifyAllocator(BOOL bUsingImageAllocator)
{
    m_bUsingImageAllocator = bUsingImageAllocator;
}

// Are we using the image DIBSECTION allocator

BOOL CDrawImage::UsingImageAllocator()
{
    return m_bUsingImageAllocator;
}

// We need the media type of the connection so that we can get the BITMAPINFO
// from it. We use that in the calls to draw the image such as StretchDIBits
// and also when updating the colour table held in shared memory DIBSECTIONs

void CDrawImage::NotifyMediaType(__in CMediaType *pMediaType)
{
    m_pMediaType = pMediaType;
}

// We store in this object a cookie maintaining the current palette version.
// Each time a palettised format is changed we increment this value so that
// when we come to draw the images we look at the colour table value they
// have and if less than the current we know to update it. This version is
// only needed and indeed used when working with shared memory DIBSECTIONs

LONG CDrawImage::GetPaletteVersion()
{
    return m_PaletteVersion;
}

// Resets the current palette version number

void CDrawImage::ResetPaletteVersion()
{
    m_PaletteVersion = PALETTE_VERSION;
}

// Increment the current palette version

void CDrawImage::IncrementPaletteVersion()
{
    m_PaletteVersion++;
}

// Constructor must initialise the base allocator. Each sample we create has a
// palette version cookie on board. When the source filter changes the palette
// during streaming the window object increments an internal cookie counter it
// keeps as well. When it comes to render the samples it looks at the cookie
// values and if they don't match then it knows to update the sample's colour
// table. However we always create samples with a cookie of PALETTE_VERSION
// If there have been multiple format changes and we disconnect and reconnect
// thereby causing the samples to be reallocated we will create them with a
// cookie much lower than the current version, this isn't a problem since it
// will be seen by the window object and the versions will then be updated

CImageAllocator::CImageAllocator(__inout CBaseFilter *pFilter, __in_opt LPCTSTR pName, __inout HRESULT *phr)
    : CBaseAllocator(pName, NULL, phr, TRUE, TRUE)
    , m_pFilter(pFilter)
{
    ASSERT(phr);
    ASSERT(pFilter);
}

// Check our DIB buffers have been released

#ifdef DEBUG
CImageAllocator::~CImageAllocator()
{
    ASSERT(m_bCommitted == FALSE);
}
#endif

// Called from destructor and also from base class to free resources. We work
// our way through the list of media samples deleting the DIBSECTION created
// for each. All samples should be back in our list so there is no chance a
// filter is still using one to write on the display or hold on a pending list

void CImageAllocator::Free()
{
    ASSERT(m_lAllocated == m_lFree.GetCount());
    EXECUTE_ASSERT(GdiFlush());
    CImageSample *pSample;
    DIBDATA *pDibData;

    while (m_lFree.GetCount() != 0)
    {
        pSample = (CImageSample *)m_lFree.RemoveHead();
        pDibData = pSample->GetDIBData();
        EXECUTE_ASSERT(DeleteObject(pDibData->hBitmap));
        EXECUTE_ASSERT(CloseHandle(pDibData->hMapping));
        delete pSample;
    }

    m_lAllocated = 0;
}

// Prepare the allocator by checking all the input parameters

STDMETHODIMP CImageAllocator::CheckSizes(__in ALLOCATOR_PROPERTIES *pRequest)
{
    // Check we have a valid connection

    if (m_pMediaType == NULL)
    {
        return VFW_E_NOT_CONNECTED;
    }

    // NOTE We always create a DIB section with the source format type which
    // may contain a source palette. When we do the BitBlt drawing operation
    // the target display device may contain a different palette (we may not
    // have the focus) in which case GDI will do after the palette mapping

    VIDEOINFOHEADER *pVideoInfo = (VIDEOINFOHEADER *)m_pMediaType->Format();

    // When we call CreateDIBSection it implicitly maps only enough memory
    // for the image as defined by thee BITMAPINFOHEADER. If the user asks
    // for an image smaller than this then we reject the call, if they ask
    // for an image larger than this then we return what they can have

    if ((DWORD)pRequest->cbBuffer < pVideoInfo->bmiHeader.biSizeImage)
    {
        return E_INVALIDARG;
    }

    // Reject buffer prefixes

    if (pRequest->cbPrefix > 0)
    {
        return E_INVALIDARG;
    }

    pRequest->cbBuffer = pVideoInfo->bmiHeader.biSizeImage;
    return NOERROR;
}

// Agree the number of media sample buffers and their sizes. The base class
// this allocator is derived from allows samples to be aligned only on byte
// boundaries NOTE the buffers are not allocated until the Commit call

STDMETHODIMP CImageAllocator::SetProperties(__in ALLOCATOR_PROPERTIES *pRequest, __out ALLOCATOR_PROPERTIES *pActual)
{
    ALLOCATOR_PROPERTIES Adjusted = *pRequest;

    // Check the parameters fit with the current connection

    HRESULT hr = CheckSizes(&Adjusted);
    if (FAILED(hr))
    {
        return hr;
    }
    return CBaseAllocator::SetProperties(&Adjusted, pActual);
}

// Commit the memory by allocating the agreed number of media samples. For
// each sample we are committed to creating we have a CImageSample object
// that we use to manage it's resources. This is initialised with a DIBDATA
// structure that contains amongst other things the GDI DIBSECTION handle
// We will access the renderer media type during this so we must have locked
// (to prevent the format changing for example). The class overrides Commit
// and Decommit to do this locking (base class Commit in turn calls Alloc)

HRESULT CImageAllocator::Alloc(void)
{
    ASSERT(m_pMediaType);
    CImageSample *pSample;
    DIBDATA DibData;

    // Check the base allocator says it's ok to continue

    HRESULT hr = CBaseAllocator::Alloc();
    if (FAILED(hr))
    {
        return hr;
    }

    // We create a new memory mapped object although we don't map it into our
    // address space because GDI does that in CreateDIBSection. It is possible
    // that we run out of resources before creating all the samples in which
    // case the available sample list is left with those already created

    ASSERT(m_lAllocated == 0);
    while (m_lAllocated < m_lCount)
    {

        // Create and initialise a shared memory GDI buffer

        hr = CreateDIB(m_lSize, DibData);
        if (FAILED(hr))
        {
            return hr;
        }

        // Create the sample object and pass it the DIBDATA

        pSample = CreateImageSample(DibData.pBase, m_lSize);
        if (pSample == NULL)
        {
            EXECUTE_ASSERT(DeleteObject(DibData.hBitmap));
            EXECUTE_ASSERT(CloseHandle(DibData.hMapping));
            return E_OUTOFMEMORY;
        }

        // Add the completed sample to the available list

        pSample->SetDIBData(&DibData);
        m_lFree.Add(pSample);
        m_lAllocated++;
    }
    return NOERROR;
}

// We have a virtual method that allocates the samples so that a derived class
// may override it and allocate more specialised sample objects. So long as it
// derives its samples from CImageSample then all this code will still work ok

CImageSample *CImageAllocator::CreateImageSample(__in_bcount(Length) LPBYTE pData, LONG Length)
{
    HRESULT hr = NOERROR;
    CImageSample *pSample;

    // Allocate the new sample and check the return codes

    pSample = new CImageSample((CBaseAllocator *)this, // Base class
                               NAME("Video sample"),   // DEBUG name
                               (HRESULT *)&hr,         // Return code
                               (LPBYTE)pData,          // DIB address
                               (LONG)Length);          // Size of DIB

    if (pSample == NULL || FAILED(hr))
    {
        delete pSample;
        return NULL;
    }
    return pSample;
}

// This function allocates a shared memory block for use by the source filter
// generating DIBs for us to render. The memory block is created in shared
// memory so that GDI doesn't have to copy the memory when we do a BitBlt

HRESULT CImageAllocator::CreateDIB(LONG InSize, DIBDATA &DibData)
{
    BITMAPINFO *pbmi; // Format information for pin
    BYTE *pBase;      // Pointer to the actual image
    HANDLE hMapping;  // Handle to mapped object
    HBITMAP hBitmap;  // DIB section bitmap handle

    // Create a file mapping object and map into our address space

    hMapping = CreateFileMapping(hMEMORY,        // Use system page file
                                 NULL,           // No security attributes
                                 PAGE_READWRITE, // Full access to memory
                                 (DWORD)0,       // Less than 4Gb in size
                                 InSize,         // Size of buffer
                                 NULL);          // No name to section
    if (hMapping == NULL)
    {
        DWORD Error = GetLastError();
        return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, Error);
    }

    // NOTE We always create a DIB section with the source format type which
    // may contain a source palette. When we do the BitBlt drawing operation
    // the target display device may contain a different palette (we may not
    // have the focus) in which case GDI will do after the palette mapping

    pbmi = (BITMAPINFO *)HEADER(m_pMediaType->Format());
    if (m_pMediaType == NULL)
    {
        DbgBreak("Invalid media type");
    }

    hBitmap = CreateDIBSection((HDC)NULL,       // NO device context
                               pbmi,            // Format information
                               DIB_RGB_COLORS,  // Use the palette
                               (VOID **)&pBase, // Pointer to image data
                               hMapping,        // Mapped memory handle
                               (DWORD)0);       // Offset into memory

    if (hBitmap == NULL || pBase == NULL)
    {
        EXECUTE_ASSERT(CloseHandle(hMapping));
        DWORD Error = GetLastError();
        return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, Error);
    }

    // Initialise the DIB information structure

    DibData.hBitmap = hBitmap;
    DibData.hMapping = hMapping;
    DibData.pBase = pBase;
    DibData.PaletteVersion = PALETTE_VERSION;
    GetObject(hBitmap, sizeof(DIBSECTION), (VOID *)&DibData.DibSection);

    return NOERROR;
}

// We use the media type during the DIBSECTION creation

void CImageAllocator::NotifyMediaType(__in CMediaType *pMediaType)
{
    m_pMediaType = pMediaType;
}

// Overriden to increment the owning object's reference count

STDMETHODIMP_(ULONG) CImageAllocator::NonDelegatingAddRef()
{
    return m_pFilter->AddRef();
}

// Overriden to decrement the owning object's reference count

STDMETHODIMP_(ULONG) CImageAllocator::NonDelegatingRelease()
{
    return m_pFilter->Release();
}

// If you derive a class from CMediaSample that has to transport specialised
// member variables and entry points then there are three alternate solutions
// The first is to create a memory buffer larger than actually required by the
// sample and store your information either at the beginning of it or at the
// end, the former being moderately safer allowing for misbehaving transform
// filters. You then adjust the buffer address when you create the base media
// sample. This has the disadvantage of breaking up the memory allocated to
// the samples into separate blocks. The second solution is to implement a
// class derived from CMediaSample and support additional interface(s) that
// convey your private data. This means defining a custom interface. The final
// alternative is to create a class that inherits from CMediaSample and adds
// the private data structures, when you get an IMediaSample in your Receive()
// call check to see if your allocator is being used, and if it is then cast
// the IMediaSample into one of your objects. Additional checks can be made
// to ensure the sample's this pointer is known to be one of your own objects

CImageSample::CImageSample(__inout CBaseAllocator *pAllocator, __in_opt LPCTSTR pName, __inout HRESULT *phr,
                           __in_bcount(length) LPBYTE pBuffer, LONG length)
    : CMediaSample(pName, pAllocator, phr, pBuffer, length)
    , m_bInit(FALSE)
{
    ASSERT(pAllocator);
    ASSERT(pBuffer);
}

// Set the shared memory DIB information

void CImageSample::SetDIBData(__in DIBDATA *pDibData)
{
    ASSERT(pDibData);
    m_DibData = *pDibData;
    m_bInit = TRUE;
}

// Retrieve the shared memory DIB data

__out DIBDATA *CImageSample::GetDIBData()
{
    ASSERT(m_bInit == TRUE);
    return &m_DibData;
}

// This class handles the creation of a palette. It is fairly specialist and
// is intended to simplify palette management for video renderer filters. It
// is for this reason that the constructor requires three other objects with
// which it interacts, namely a base media filter, a base window and a base
// drawing object although the base window or the draw object may be NULL to
// ignore that part of us. We try not to create and install palettes unless
// absolutely necessary as they typically require WM_PALETTECHANGED messages
// to be sent to every window thread in the system which is very expensive

CImagePalette::CImagePalette(__inout CBaseFilter *pBaseFilter, __inout CBaseWindow *pBaseWindow,
                             __inout CDrawImage *pDrawImage)
    : m_pBaseWindow(pBaseWindow)
    , m_pFilter(pBaseFilter)
    , m_pDrawImage(pDrawImage)
    , m_hPalette(NULL)
{
    ASSERT(m_pFilter);
}

// Destructor

#ifdef DEBUG
CImagePalette::~CImagePalette()
{
    ASSERT(m_hPalette == NULL);
}
#endif

// We allow dynamic format changes of the palette but rather than change the
// palette every time we call this to work out whether an update is required.
// If the original type didn't use a palette and the new one does (or vica
// versa) then we return TRUE. If neither formats use a palette we'll return
// FALSE. If both formats use a palette we compare their colours and return
// FALSE if they match. This therefore short circuits palette creation unless
// absolutely necessary since installing palettes is an expensive operation

BOOL CImagePalette::ShouldUpdate(const VIDEOINFOHEADER *pNewInfo, const VIDEOINFOHEADER *pOldInfo)
{
    // We may not have a current format yet

    if (pOldInfo == NULL)
    {
        return TRUE;
    }

    // Do both formats not require a palette

    if (ContainsPalette(pNewInfo) == FALSE)
    {
        if (ContainsPalette(pOldInfo) == FALSE)
        {
            return FALSE;
        }
    }

    // Compare the colours to see if they match

    DWORD VideoEntries = pNewInfo->bmiHeader.biClrUsed;
    if (ContainsPalette(pNewInfo) == TRUE)
        if (ContainsPalette(pOldInfo) == TRUE)
            if (pOldInfo->bmiHeader.biClrUsed == VideoEntries)
                if (pOldInfo->bmiHeader.biClrUsed > 0)
                    if (memcmp((PVOID)GetBitmapPalette(pNewInfo), (PVOID)GetBitmapPalette(pOldInfo),
                               VideoEntries * sizeof(RGBQUAD)) == 0)
                    {

                        return FALSE;
                    }
    return TRUE;
}

// This is normally called when the input pin type is set to install a palette
// We will typically be called from two different places. The first is when we
// have negotiated a palettised media type after connection, the other is when
// we receive a new type during processing with an updated palette in which
// case we must remove and release the resources held by the current palette

// We can be passed an optional device name if we wish to prepare a palette
// for a specific monitor on a multi monitor system

HRESULT CImagePalette::PreparePalette(const CMediaType *pmtNew, const CMediaType *pmtOld, __in LPSTR szDevice)
{
    const VIDEOINFOHEADER *pNewInfo = (VIDEOINFOHEADER *)pmtNew->Format();
    const VIDEOINFOHEADER *pOldInfo = (VIDEOINFOHEADER *)pmtOld->Format();
    ASSERT(pNewInfo);

    // This is an performance optimisation, when we get a media type we check
    // to see if the format requires a palette change. If either we need one
    // when previously we didn't or vica versa then this returns TRUE, if we
    // previously needed a palette and we do now it compares their colours

    if (ShouldUpdate(pNewInfo, pOldInfo) == FALSE)
    {
        NOTE("No update needed");
        return S_FALSE;
    }

    // We must notify the filter graph that the application may have changed
    // the palette although in practice we don't bother checking to see if it
    // is really different. If it tries to get the palette either the window
    // or renderer lock will ensure it doesn't get in until we are finished

    RemovePalette();
    m_pFilter->NotifyEvent(EC_PALETTE_CHANGED, 0, 0);

    // Do we need a palette for the new format

    if (ContainsPalette(pNewInfo) == FALSE)
    {
        NOTE("New has no palette");
        return S_FALSE;
    }

    if (m_pBaseWindow)
    {
        m_pBaseWindow->LockPaletteLock();
    }

    // If we're changing the palette on the fly then we increment our palette
    // cookie which is compared against the cookie also stored in all of our
    // DIBSECTION media samples. If they don't match when we come to draw it
    // then we know the sample is out of date and we'll update it's palette

    NOTE("Making new colour palette");
    m_hPalette = MakePalette(pNewInfo, szDevice);
    ASSERT(m_hPalette != NULL);

    if (m_pBaseWindow)
    {
        m_pBaseWindow->UnlockPaletteLock();
    }

    // The window in which the new palette is to be realised may be a NULL
    // pointer to signal that no window is in use, if so we don't call it
    // Some filters just want to use this object to create/manage palettes

    if (m_pBaseWindow)
        m_pBaseWindow->SetPalette(m_hPalette);

    // This is the only time where we need access to the draw object to say
    // to it that a new palette will be arriving on a sample real soon. The
    // constructor may take a NULL pointer in which case we don't call this

    if (m_pDrawImage)
        m_pDrawImage->IncrementPaletteVersion();
    return NOERROR;
}

// Helper function to copy a palette out of any kind of VIDEOINFO (ie it may
// be YUV or true colour) into a palettised VIDEOINFO. We use this changing
// palettes on DirectDraw samples as a source filter can attach a palette to
// any buffer (eg YUV) and hand it back. We make a new palette out of that
// format and then copy the palette colours into the current connection type

HRESULT CImagePalette::CopyPalette(const CMediaType *pSrc, __out CMediaType *pDest)
{
    // Reset the destination palette before starting

    VIDEOINFOHEADER *pDestInfo = (VIDEOINFOHEADER *)pDest->Format();
    pDestInfo->bmiHeader.biClrUsed = 0;
    pDestInfo->bmiHeader.biClrImportant = 0;

    // Does the destination have a palette

    if (PALETTISED(pDestInfo) == FALSE)
    {
        NOTE("No destination palette");
        return S_FALSE;
    }

    // Does the source contain a palette

    const VIDEOINFOHEADER *pSrcInfo = (VIDEOINFOHEADER *)pSrc->Format();
    if (ContainsPalette(pSrcInfo) == FALSE)
    {
        NOTE("No source palette");
        return S_FALSE;
    }

    // The number of colours may be zero filled

    DWORD PaletteEntries = pSrcInfo->bmiHeader.biClrUsed;
    if (PaletteEntries == 0)
    {
        DWORD Maximum = (1 << pSrcInfo->bmiHeader.biBitCount);
        NOTE1("Setting maximum colours (%d)", Maximum);
        PaletteEntries = Maximum;
    }

    // Make sure the destination has enough room for the palette

    ASSERT(pSrcInfo->bmiHeader.biClrUsed <= iPALETTE_COLORS);
    ASSERT(pSrcInfo->bmiHeader.biClrImportant <= PaletteEntries);
    ASSERT(COLORS(pDestInfo) == GetBitmapPalette(pDestInfo));
    pDestInfo->bmiHeader.biClrUsed = PaletteEntries;
    pDestInfo->bmiHeader.biClrImportant = pSrcInfo->bmiHeader.biClrImportant;
    ULONG BitmapSize = GetBitmapFormatSize(HEADER(pSrcInfo));

    if (pDest->FormatLength() < BitmapSize)
    {
        NOTE("Reallocating destination");
        pDest->ReallocFormatBuffer(BitmapSize);
    }

    // Now copy the palette colours across

    CopyMemory((PVOID)COLORS(pDestInfo), (PVOID)GetBitmapPalette(pSrcInfo), PaletteEntries * sizeof(RGBQUAD));

    return NOERROR;
}

// This is normally called when the palette is changed (typically during a
// dynamic format change) to remove any palette we previously installed. We
// replace it (if necessary) in the video window with a standard VGA palette
// that should always be available even if this is a true colour display

HRESULT CImagePalette::RemovePalette()
{
    if (m_pBaseWindow)
    {
        m_pBaseWindow->LockPaletteLock();
    }

    // Do we have a palette to remove

    if (m_hPalette != NULL)
    {

        if (m_pBaseWindow)
        {
            // Make sure that the window's palette handle matches
            // our palette handle.
            ASSERT(m_hPalette == m_pBaseWindow->GetPalette());

            m_pBaseWindow->UnsetPalette();
        }

        EXECUTE_ASSERT(DeleteObject(m_hPalette));
        m_hPalette = NULL;
    }

    if (m_pBaseWindow)
    {
        m_pBaseWindow->UnlockPaletteLock();
    }

    return NOERROR;
}

// Called to create a palette for the object, the data structure used by GDI
// to describe a palette is a LOGPALETTE, this includes a variable number of
// PALETTEENTRY fields which are the colours, we have to convert the RGBQUAD
// colour fields we are handed in a BITMAPINFO from the media type into these
// This handles extraction of palettes from true colour and YUV media formats

// We can be passed an optional device name if we wish to prepare a palette
// for a specific monitor on a multi monitor system

HPALETTE CImagePalette::MakePalette(const VIDEOINFOHEADER *pVideoInfo, __in LPSTR szDevice)
{
    ASSERT(ContainsPalette(pVideoInfo) == TRUE);
    ASSERT(pVideoInfo->bmiHeader.biClrUsed <= iPALETTE_COLORS);
    BITMAPINFOHEADER *pHeader = HEADER(pVideoInfo);

    const RGBQUAD *pColours; // Pointer to the palette
    LOGPALETTE *lp;          // Used to create a palette
    HPALETTE hPalette;       // Logical palette object

    lp = (LOGPALETTE *)new BYTE[sizeof(LOGPALETTE) + SIZE_PALETTE];
    if (lp == NULL)
    {
        return NULL;
    }

    // Unfortunately for some hare brained reason a GDI palette entry (a
    // PALETTEENTRY structure) is different to a palette entry from a DIB
    // format (a RGBQUAD structure) so we have to do the field conversion
    // The VIDEOINFO containing the palette may be a true colour type so
    // we use GetBitmapPalette to skip over any bit fields if they exist

    lp->palVersion = PALVERSION;
    lp->palNumEntries = (USHORT)pHeader->biClrUsed;
    if (lp->palNumEntries == 0)
        lp->palNumEntries = (1 << pHeader->biBitCount);
    pColours = GetBitmapPalette(pVideoInfo);

    for (DWORD dwCount = 0; dwCount < lp->palNumEntries; dwCount++)
    {
        lp->palPalEntry[dwCount].peRed = pColours[dwCount].rgbRed;
        lp->palPalEntry[dwCount].peGreen = pColours[dwCount].rgbGreen;
        lp->palPalEntry[dwCount].peBlue = pColours[dwCount].rgbBlue;
        lp->palPalEntry[dwCount].peFlags = 0;
    }

    MakeIdentityPalette(lp->palPalEntry, lp->palNumEntries, szDevice);

    // Create a logical palette

    hPalette = CreatePalette(lp);
    ASSERT(hPalette != NULL);
    delete[] lp;
    return hPalette;
}

// GDI does a fair job of compressing the palette entries you give it, so for
// example if you have five entries with an RGB colour (0,0,0) it will remove
// all but one of them. When you subsequently draw an image it will map from
// your logical palette to the compressed device palette. This function looks
// to see if it is trying to be an identity palette and if so sets the flags
// field in the PALETTEENTRYs so they remain expanded to boost performance

// We can be passed an optional device name if we wish to prepare a palette
// for a specific monitor on a multi monitor system

HRESULT CImagePalette::MakeIdentityPalette(__inout_ecount_full(iColours) PALETTEENTRY *pEntry, INT iColours,
                                           __in LPSTR szDevice)
{
    PALETTEENTRY SystemEntries[10];      // System palette entries
    BOOL bIdentityPalette = TRUE;        // Is an identity palette
    ASSERT(iColours <= iPALETTE_COLORS); // Should have a palette
    const int PalLoCount = 10;           // First ten reserved colours
    const int PalHiStart = 246;          // Last VGA palette entries

    // Does this have the full colour range

    if (iColours < 10)
    {
        return S_FALSE;
    }

    // Apparently some displays have odd numbers of system colours

    // Get a DC on the right monitor - it's ugly, but this is the way you have
    // to do it
    HDC hdc;
    if (szDevice == NULL || lstrcmpiLocaleIndependentA(szDevice, "DISPLAY") == 0)
        hdc = CreateDCA("DISPLAY", NULL, NULL, NULL);
    else
        hdc = CreateDCA(NULL, szDevice, NULL, NULL);
    if (NULL == hdc)
    {
        return E_OUTOFMEMORY;
    }
    INT Reserved = GetDeviceCaps(hdc, NUMRESERVED);
    if (Reserved != 20)
    {
        DeleteDC(hdc);
        return S_FALSE;
    }

    // Compare our palette against the first ten system entries. The reason I
    // don't do a memory compare between our two arrays of colours is because
    // I am not sure what will be in the flags fields for the system entries

    UINT Result = GetSystemPaletteEntries(hdc, 0, PalLoCount, SystemEntries);
    for (UINT Count = 0; Count < Result; Count++)
    {
        if (SystemEntries[Count].peRed != pEntry[Count].peRed ||
            SystemEntries[Count].peGreen != pEntry[Count].peGreen ||
            SystemEntries[Count].peBlue != pEntry[Count].peBlue)
        {
            bIdentityPalette = FALSE;
        }
    }

    // And likewise compare against the last ten entries

    Result = GetSystemPaletteEntries(hdc, PalHiStart, PalLoCount, SystemEntries);
    for (UINT Count = 0; Count < Result; Count++)
    {
        if (INT(Count) + PalHiStart < iColours)
        {
            if (SystemEntries[Count].peRed != pEntry[PalHiStart + Count].peRed ||
                SystemEntries[Count].peGreen != pEntry[PalHiStart + Count].peGreen ||
                SystemEntries[Count].peBlue != pEntry[PalHiStart + Count].peBlue)
            {
                bIdentityPalette = FALSE;
            }
        }
    }

    // If not an identity palette then return S_FALSE

    DeleteDC(hdc);
    if (bIdentityPalette == FALSE)
    {
        return S_FALSE;
    }

    // Set the non VGA entries so that GDI doesn't map them

    for (UINT Count = PalLoCount; INT(Count) < min(PalHiStart, iColours); Count++)
    {
        pEntry[Count].peFlags = PC_NOCOLLAPSE;
    }
    return NOERROR;
}

// Constructor initialises the VIDEOINFO we keep storing the current display
// format. The format can be changed at any time, to reset the format held
// by us call the RefreshDisplayType directly (it's a public method). Since
// more than one thread will typically call us (ie window threads resetting
// the type and source threads in the type checking methods) we have a lock

CImageDisplay::CImageDisplay()
{
    RefreshDisplayType(NULL);
}

// This initialises the format we hold which contains the display device type
// We do a conversion on the display device type in here so that when we start
// type checking input formats we can assume that certain fields have been set
// correctly, an example is when we make the 16 bit mask fields explicit. This
// is normally called when we receive WM_DEVMODECHANGED device change messages

// The optional szDeviceName parameter tells us which monitor we are interested
// in for a multi monitor system

HRESULT CImageDisplay::RefreshDisplayType(__in_opt LPSTR szDeviceName)
{
    CAutoLock cDisplayLock(this);

    // Set the preferred format type

    ZeroMemory((PVOID)&m_Display, sizeof(VIDEOINFOHEADER) + sizeof(TRUECOLORINFO));
    m_Display.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_Display.bmiHeader.biBitCount = FALSE;

    // Get the bit depth of a device compatible bitmap

    // get caps of whichever monitor they are interested in (multi monitor)
    HDC hdcDisplay;
    // it's ugly, but this is the way you have to do it
    if (szDeviceName == NULL || lstrcmpiLocaleIndependentA(szDeviceName, "DISPLAY") == 0)
        hdcDisplay = CreateDCA("DISPLAY", NULL, NULL, NULL);
    else
        hdcDisplay = CreateDCA(NULL, szDeviceName, NULL, NULL);
    if (hdcDisplay == NULL)
    {
        ASSERT(FALSE);
        DbgLog((LOG_ERROR, 1, TEXT("ACK! Can't get a DC for %hs"), szDeviceName ? szDeviceName : "<NULL>"));
        return E_FAIL;
    }
    else
    {
        DbgLog((LOG_TRACE, 3, TEXT("Created a DC for %s"), szDeviceName ? szDeviceName : "<NULL>"));
    }
    HBITMAP hbm = CreateCompatibleBitmap(hdcDisplay, 1, 1);
    if (hbm)
    {
        GetDIBits(hdcDisplay, hbm, 0, 1, NULL, (BITMAPINFO *)&m_Display.bmiHeader, DIB_RGB_COLORS);

        // This call will get the colour table or the proper bitfields
        GetDIBits(hdcDisplay, hbm, 0, 1, NULL, (BITMAPINFO *)&m_Display.bmiHeader, DIB_RGB_COLORS);
        DeleteObject(hbm);
    }
    DeleteDC(hdcDisplay);

    // Complete the display type initialisation

    ASSERT(CheckHeaderValidity(&m_Display));
    UpdateFormat(&m_Display);
    DbgLog((LOG_TRACE, 3, TEXT("New DISPLAY bit depth =%d"), m_Display.bmiHeader.biBitCount));
    return NOERROR;
}

// We assume throughout this code that any bitfields masks are allowed no
// more than eight bits to store a colour component. This checks that the
// bit count assumption is enforced and also makes sure that all the bits
// set are contiguous. We return a boolean TRUE if the field checks out ok

BOOL CImageDisplay::CheckBitFields(const VIDEOINFO *pInput)
{
    DWORD *pBitFields = (DWORD *)BITMASKS(pInput);

    for (INT iColour = iRED; iColour <= iBLUE; iColour++)
    {

        // First of all work out how many bits are set

        DWORD SetBits = CountSetBits(pBitFields[iColour]);
        if (SetBits > iMAXBITS || SetBits == 0)
        {
            NOTE1("Bit fields for component %d invalid", iColour);
            return FALSE;
        }

        // Next work out the number of zero bits prefix
        DWORD PrefixBits = CountPrefixBits(pBitFields[iColour]);

        // This is going to see if all the bits set are contiguous (as they
        // should be). We know how much to shift them right by from the
        // count of prefix bits. The number of bits set defines a mask, we
        // invert this (ones complement) and AND it with the shifted bit
        // fields. If the result is NON zero then there are bit(s) sticking
        // out the left hand end which means they are not contiguous

        DWORD TestField = pBitFields[iColour] >> PrefixBits;
        DWORD Mask = ULONG_MAX << SetBits;
        if (TestField & Mask)
        {
            NOTE1("Bit fields for component %d not contiguous", iColour);
            return FALSE;
        }
    }
    return TRUE;
}

// This counts the number of bits set in the input field

DWORD CImageDisplay::CountSetBits(DWORD Field)
{
    // This is a relatively well known bit counting algorithm

    DWORD Count = 0;
    DWORD init = Field;

    // Until the input is exhausted, count the number of bits

    while (init)
    {
        init = init & (init - 1); // Turn off the bottommost bit
        Count++;
    }
    return Count;
}

// This counts the number of zero bits upto the first one set NOTE the input
// field should have been previously checked to ensure there is at least one
// set although if we don't find one set we return the impossible value 32

DWORD CImageDisplay::CountPrefixBits(DWORD Field)
{
    DWORD Mask = 1;
    DWORD Count = 0;

    while (TRUE)
    {
        if (Field & Mask)
        {
            return Count;
        }
        Count++;

        ASSERT(Mask != 0x80000000);
        if (Mask == 0x80000000)
        {
            return Count;
        }
        Mask <<= 1;
    }
}

// This is called to check the BITMAPINFOHEADER for the input type. There are
// many implicit dependancies between the fields in a header structure which
// if we validate now make for easier manipulation in subsequent handling. We
// also check that the BITMAPINFOHEADER matches it's specification such that
// fields likes the number of planes is one, that it's structure size is set
// correctly and that the bitmap dimensions have not been set as negative

BOOL CImageDisplay::CheckHeaderValidity(const VIDEOINFO *pInput)
{
    // Check the bitmap width and height are not negative.

    if (pInput->bmiHeader.biWidth <= 0 || pInput->bmiHeader.biHeight <= 0)
    {
        NOTE("Invalid bitmap dimensions");
        return FALSE;
    }

    // Check the compression is either BI_RGB or BI_BITFIELDS

    if (pInput->bmiHeader.biCompression != BI_RGB)
    {
        if (pInput->bmiHeader.biCompression != BI_BITFIELDS)
        {
            NOTE("Invalid compression format");
            return FALSE;
        }
    }

    // If BI_BITFIELDS compression format check the colour depth

    if (pInput->bmiHeader.biCompression == BI_BITFIELDS)
    {
        if (pInput->bmiHeader.biBitCount != 16)
        {
            if (pInput->bmiHeader.biBitCount != 32)
            {
                NOTE("BI_BITFIELDS not 16/32 bit depth");
                return FALSE;
            }
        }
    }

    // Check the assumptions about the layout of the bit fields

    if (pInput->bmiHeader.biCompression == BI_BITFIELDS)
    {
        if (CheckBitFields(pInput) == FALSE)
        {
            NOTE("Bit fields are not valid");
            return FALSE;
        }
    }

    // Are the number of planes equal to one

    if (pInput->bmiHeader.biPlanes != 1)
    {
        NOTE("Number of planes not one");
        return FALSE;
    }

    // Check the image size is consistent (it can be zero)

    if (pInput->bmiHeader.biSizeImage != GetBitmapSize(&pInput->bmiHeader))
    {
        if (pInput->bmiHeader.biSizeImage)
        {
            NOTE("Image size incorrectly set");
            return FALSE;
        }
    }

    // Check the size of the structure

    if (pInput->bmiHeader.biSize != sizeof(BITMAPINFOHEADER))
    {
        NOTE("Size of BITMAPINFOHEADER wrong");
        return FALSE;
    }
    return CheckPaletteHeader(pInput);
}

// This runs a few simple tests against the palette fields in the input to
// see if it looks vaguely correct. The tests look at the number of palette
// colours present, the number considered important and the biCompression
// field which should always be BI_RGB as no other formats are meaningful

BOOL CImageDisplay::CheckPaletteHeader(const VIDEOINFO *pInput)
{
    // The checks here are for palettised videos only

    if (PALETTISED(pInput) == FALSE)
    {
        if (pInput->bmiHeader.biClrUsed)
        {
            NOTE("Invalid palette entries");
            return FALSE;
        }
        return TRUE;
    }

    // Compression type of BI_BITFIELDS is meaningless for palette video

    if (pInput->bmiHeader.biCompression != BI_RGB)
    {
        NOTE("Palettised video must be BI_RGB");
        return FALSE;
    }

    // Check the number of palette colours is correct

    if (pInput->bmiHeader.biClrUsed > PALETTE_ENTRIES(pInput))
    {
        NOTE("Too many colours in palette");
        return FALSE;
    }

    // The number of important colours shouldn't exceed the number used

    if (pInput->bmiHeader.biClrImportant > pInput->bmiHeader.biClrUsed)
    {
        NOTE("Too many important colours");
        return FALSE;
    }
    return TRUE;
}

// Return the format of the video display

const VIDEOINFO *CImageDisplay::GetDisplayFormat()
{
    return &m_Display;
}

// Return TRUE if the display uses a palette

BOOL CImageDisplay::IsPalettised()
{
    return PALETTISED(&m_Display);
}

// Return the bit depth of the current display setting

WORD CImageDisplay::GetDisplayDepth()
{
    return m_Display.bmiHeader.biBitCount;
}

// Initialise the optional fields in a VIDEOINFO. These are mainly to do with
// the source and destination rectangles and palette information such as the
// number of colours present. It simplifies our code just a little if we don't
// have to keep checking for all the different valid permutations in a header
// every time we want to do anything with it (an example would be creating a
// palette). We set the base class media type before calling this function so
// that the media types between the pins match after a connection is made

HRESULT CImageDisplay::UpdateFormat(__inout VIDEOINFO *pVideoInfo)
{
    ASSERT(pVideoInfo);

    BITMAPINFOHEADER *pbmi = HEADER(pVideoInfo);
    SetRectEmpty(&pVideoInfo->rcSource);
    SetRectEmpty(&pVideoInfo->rcTarget);

    // Set the number of colours explicitly

    if (PALETTISED(pVideoInfo))
    {
        if (pVideoInfo->bmiHeader.biClrUsed == 0)
        {
            pVideoInfo->bmiHeader.biClrUsed = PALETTE_ENTRIES(pVideoInfo);
        }
    }

    // The number of important colours shouldn't exceed the number used, on
    // some displays the number of important colours is not initialised when
    // retrieving the display type so we set the colours used correctly

    if (pVideoInfo->bmiHeader.biClrImportant > pVideoInfo->bmiHeader.biClrUsed)
    {
        pVideoInfo->bmiHeader.biClrImportant = PALETTE_ENTRIES(pVideoInfo);
    }

    // Change the image size field to be explicit

    if (pVideoInfo->bmiHeader.biSizeImage == 0)
    {
        pVideoInfo->bmiHeader.biSizeImage = GetBitmapSize(&pVideoInfo->bmiHeader);
    }
    return NOERROR;
}

// Lots of video rendering filters want code to check proposed formats are ok
// This checks the VIDEOINFO we are passed as a media type. If the media type
// is a valid media type then we return NOERROR otherwise E_INVALIDARG. Note
// however we only accept formats that can be easily displayed in the display
// so if we are on a 16 bit device we will not accept 24 bit images. The one
// complexity is that most displays draw 8 bit palettised images efficiently
// Also if the input format is less colour bits per pixel then we also accept

HRESULT CImageDisplay::CheckVideoType(const VIDEOINFO *pInput)
{
    // First of all check the VIDEOINFOHEADER looks correct

    if (CheckHeaderValidity(pInput) == FALSE)
    {
        return E_INVALIDARG;
    }

    // Virtually all devices support palettised images efficiently

    if (m_Display.bmiHeader.biBitCount == pInput->bmiHeader.biBitCount)
    {
        if (PALETTISED(pInput) == TRUE)
        {
            ASSERT(PALETTISED(&m_Display) == TRUE);
            NOTE("(Video) Type connection ACCEPTED");
            return NOERROR;
        }
    }

    // Is the display depth greater than the input format

    if (m_Display.bmiHeader.biBitCount > pInput->bmiHeader.biBitCount)
    {
        NOTE("(Video) Mismatch agreed");
        return NOERROR;
    }

    // Is the display depth less than the input format

    if (m_Display.bmiHeader.biBitCount < pInput->bmiHeader.biBitCount)
    {
        NOTE("(Video) Format mismatch");
        return E_INVALIDARG;
    }

    // Both input and display formats are either BI_RGB or BI_BITFIELDS

    ASSERT(m_Display.bmiHeader.biBitCount == pInput->bmiHeader.biBitCount);
    ASSERT(PALETTISED(pInput) == FALSE);
    ASSERT(PALETTISED(&m_Display) == FALSE);

    // BI_RGB 16 bit representation is implicitly RGB555, and likewise BI_RGB
    // 24 bit representation is RGB888. So we initialise a pointer to the bit
    // fields they really mean and check against the display device format
    // This is only going to be called when both formats are equal bits pixel

    const DWORD *pInputMask = GetBitMasks(pInput);
    const DWORD *pDisplayMask = GetBitMasks((VIDEOINFO *)&m_Display);

    if (pInputMask[iRED] != pDisplayMask[iRED] || pInputMask[iGREEN] != pDisplayMask[iGREEN] ||
        pInputMask[iBLUE] != pDisplayMask[iBLUE])
    {

        NOTE("(Video) Bit field mismatch");
        return E_INVALIDARG;
    }

    NOTE("(Video) Type connection ACCEPTED");
    return NOERROR;
}

// Return the bit masks for the true colour VIDEOINFO provided

const DWORD *CImageDisplay::GetBitMasks(const VIDEOINFO *pVideoInfo)
{
    static const DWORD FailMasks[] = {0, 0, 0};

    if (pVideoInfo->bmiHeader.biCompression == BI_BITFIELDS)
    {
        return BITMASKS(pVideoInfo);
    }

    ASSERT(pVideoInfo->bmiHeader.biCompression == BI_RGB);

    switch (pVideoInfo->bmiHeader.biBitCount)
    {
    case 16: return bits555;
    case 24: return bits888;
    case 32: return bits888;
    default: return FailMasks;
    }
}

// Check to see if we can support media type pmtIn as proposed by the output
// pin - We first check that the major media type is video and also identify
// the media sub type. Then we thoroughly check the VIDEOINFO type provided
// As well as the contained VIDEOINFO being correct the major type must be
// video, the subtype a recognised video format and the type GUID correct

HRESULT CImageDisplay::CheckMediaType(const CMediaType *pmtIn)
{
    // Does this have a VIDEOINFOHEADER format block

    const GUID *pFormatType = pmtIn->FormatType();
    if (*pFormatType != FORMAT_VideoInfo)
    {
        NOTE("Format GUID not a VIDEOINFOHEADER");
        return E_INVALIDARG;
    }
    ASSERT(pmtIn->Format());

    // Check the format looks reasonably ok

    ULONG Length = pmtIn->FormatLength();
    if (Length < SIZE_VIDEOHEADER)
    {
        NOTE("Format smaller than a VIDEOHEADER");
        return E_FAIL;
    }

    VIDEOINFO *pInput = (VIDEOINFO *)pmtIn->Format();

    // Check the major type is MEDIATYPE_Video

    const GUID *pMajorType = pmtIn->Type();
    if (*pMajorType != MEDIATYPE_Video)
    {
        NOTE("Major type not MEDIATYPE_Video");
        return E_INVALIDARG;
    }

    // Check we can identify the media subtype

    const GUID *pSubType = pmtIn->Subtype();
    if (GetBitCount(pSubType) == USHRT_MAX)
    {
        NOTE("Invalid video media subtype");
        return E_INVALIDARG;
    }
    return CheckVideoType(pInput);
}

// Given a video format described by a VIDEOINFO structure we return the mask
// that is used to obtain the range of acceptable colours for this type, for
// example, the mask for a 24 bit true colour format is 0xFF in all cases. A
// 16 bit 5:6:5 display format uses 0xF8, 0xFC and 0xF8, therefore given any
// RGB triplets we can AND them with these fields to find one that is valid

BOOL CImageDisplay::GetColourMask(__out DWORD *pMaskRed, __out DWORD *pMaskGreen, __out DWORD *pMaskBlue)
{
    CAutoLock cDisplayLock(this);
    *pMaskRed = 0xFF;
    *pMaskGreen = 0xFF;
    *pMaskBlue = 0xFF;

    // If this format is palettised then it doesn't have bit fields

    if (m_Display.bmiHeader.biBitCount < 16)
    {
        return FALSE;
    }

    // If this is a 24 bit true colour display then it can handle all the
    // possible colour component ranges described by a byte. It is never
    // allowed for a 24 bit colour depth image to have BI_BITFIELDS set

    if (m_Display.bmiHeader.biBitCount == 24)
    {
        ASSERT(m_Display.bmiHeader.biCompression == BI_RGB);
        return TRUE;
    }

    // Calculate the mask based on the format's bit fields

    const DWORD *pBitFields = (DWORD *)GetBitMasks((VIDEOINFO *)&m_Display);
    DWORD *pOutputMask[] = {pMaskRed, pMaskGreen, pMaskBlue};

    // We know from earlier testing that there are no more than iMAXBITS
    // bits set in the mask and that they are all contiguous. All that
    // therefore remains is to shift them into the correct position

    for (INT iColour = iRED; iColour <= iBLUE; iColour++)
    {

        // This works out how many bits there are and where they live

        DWORD PrefixBits = CountPrefixBits(pBitFields[iColour]);
        DWORD SetBits = CountSetBits(pBitFields[iColour]);

        // The first shift moves the bit field so that it is right justified
        // in the DWORD, after which we then shift it back left which then
        // puts the leading bit in the bytes most significant bit position

        *(pOutputMask[iColour]) = pBitFields[iColour] >> PrefixBits;
        *(pOutputMask[iColour]) <<= (iMAXBITS - SetBits);
    }
    return TRUE;
}

/*  Helper to convert to VIDEOINFOHEADER2
 */
STDAPI ConvertVideoInfoToVideoInfo2(__inout AM_MEDIA_TYPE *pmt)
{
    if (pmt->formattype != FORMAT_VideoInfo)
    {
        return E_INVALIDARG;
    }
    if (NULL == pmt->pbFormat || pmt->cbFormat < sizeof(VIDEOINFOHEADER))
    {
        return E_INVALIDARG;
    }
    VIDEOINFO *pVideoInfo = (VIDEOINFO *)pmt->pbFormat;
    DWORD dwNewSize;
    HRESULT hr = DWordAdd(pmt->cbFormat, sizeof(VIDEOINFOHEADER2) - sizeof(VIDEOINFOHEADER), &dwNewSize);
    if (FAILED(hr))
    {
        return hr;
    }
    PVOID pvNew = CoTaskMemAlloc(dwNewSize);
    if (pvNew == NULL)
    {
        return E_OUTOFMEMORY;
    }
    CopyMemory(pvNew, pmt->pbFormat, FIELD_OFFSET(VIDEOINFOHEADER, bmiHeader));
    ZeroMemory((PBYTE)pvNew + FIELD_OFFSET(VIDEOINFOHEADER, bmiHeader),
               sizeof(VIDEOINFOHEADER2) - sizeof(VIDEOINFOHEADER));
    CopyMemory((PBYTE)pvNew + FIELD_OFFSET(VIDEOINFOHEADER2, bmiHeader),
               pmt->pbFormat + FIELD_OFFSET(VIDEOINFOHEADER, bmiHeader),
               pmt->cbFormat - FIELD_OFFSET(VIDEOINFOHEADER, bmiHeader));
    VIDEOINFOHEADER2 *pVideoInfo2 = (VIDEOINFOHEADER2 *)pvNew;
    pVideoInfo2->dwPictAspectRatioX = (DWORD)pVideoInfo2->bmiHeader.biWidth;
    pVideoInfo2->dwPictAspectRatioY = (DWORD)abs(pVideoInfo2->bmiHeader.biHeight);
    pmt->formattype = FORMAT_VideoInfo2;
    CoTaskMemFree(pmt->pbFormat);
    pmt->pbFormat = (PBYTE)pvNew;
    pmt->cbFormat += sizeof(VIDEOINFOHEADER2) - sizeof(VIDEOINFOHEADER);
    return S_OK;
}

//  Check a media type containing VIDEOINFOHEADER
STDAPI CheckVideoInfoType(const AM_MEDIA_TYPE *pmt)
{
    if (NULL == pmt || NULL == pmt->pbFormat)
    {
        return E_POINTER;
    }
    if (pmt->majortype != MEDIATYPE_Video || pmt->formattype != FORMAT_VideoInfo ||
        pmt->cbFormat < sizeof(VIDEOINFOHEADER))
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    const VIDEOINFOHEADER *pHeader = (const VIDEOINFOHEADER *)pmt->pbFormat;
    if (!ValidateBitmapInfoHeader(&pHeader->bmiHeader, pmt->cbFormat - FIELD_OFFSET(VIDEOINFOHEADER, bmiHeader)))
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    return S_OK;
}

//  Check a media type containing VIDEOINFOHEADER2
STDAPI CheckVideoInfo2Type(const AM_MEDIA_TYPE *pmt)
{
    if (NULL == pmt || NULL == pmt->pbFormat)
    {
        return E_POINTER;
    }
    if (pmt->majortype != MEDIATYPE_Video || pmt->formattype != FORMAT_VideoInfo2 ||
        pmt->cbFormat < sizeof(VIDEOINFOHEADER2))
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    const VIDEOINFOHEADER2 *pHeader = (const VIDEOINFOHEADER2 *)pmt->pbFormat;
    if (!ValidateBitmapInfoHeader(&pHeader->bmiHeader, pmt->cbFormat - FIELD_OFFSET(VIDEOINFOHEADER2, bmiHeader)))
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    return S_OK;
}
