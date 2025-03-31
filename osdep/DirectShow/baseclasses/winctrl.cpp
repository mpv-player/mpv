//------------------------------------------------------------------------------
// File: WinCtrl.cpp
//
// Desc: DirectShow base classes - implements video control interface class.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <intsafe.h>
#include <checkbmi.h>

// The control interface methods require us to be connected

#define CheckConnected(pin, code)             \
    {                                         \
        if (pin == NULL)                      \
        {                                     \
            ASSERT(!TEXT("Pin not set"));     \
        }                                     \
        else if (pin->IsConnected() == FALSE) \
        {                                     \
            return (code);                    \
        }                                     \
    }

// This checks to see whether the window has a drain. An application can in
// most environments set the owner/parent of windows so that they appear in
// a compound document context (for example). In this case, the application
// would probably like to be told of any keyboard/mouse messages. Therefore
// we pass these messages on untranslated, returning TRUE if we're successful

BOOL WINAPI PossiblyEatMessage(HWND hwndDrain, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (hwndDrain != NULL && !InSendMessage())
    {
        switch (uMsg)
        {
        case WM_CHAR:
        case WM_DEADCHAR:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEACTIVATE:
        case WM_MOUSEMOVE:
        // If we pass this on we don't get any mouse clicks
        // case WM_NCHITTEST:
        case WM_NCLBUTTONDBLCLK:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        case WM_NCMBUTTONDBLCLK:
        case WM_NCMBUTTONDOWN:
        case WM_NCMBUTTONUP:
        case WM_NCMOUSEMOVE:
        case WM_NCRBUTTONDBLCLK:
        case WM_NCRBUTTONDOWN:
        case WM_NCRBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_SYSCHAR:
        case WM_SYSDEADCHAR:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:

            DbgLog((LOG_TRACE, 2, TEXT("Forwarding %x to drain")));
            PostMessage(hwndDrain, uMsg, wParam, lParam);

            return TRUE;
        }
    }
    return FALSE;
}

// This class implements the IVideoWindow control functions (dual interface)
// we support a large number of properties and methods designed to allow the
// client (whether it be an automation controller or a C/C++ application) to
// set and get a number of window related properties such as it's position.
// We also support some methods that duplicate the properties but provide a
// more direct and efficient mechanism as many values may be changed in one

CBaseControlWindow::CBaseControlWindow(__inout CBaseFilter *pFilter,  // Owning filter
                                       __in CCritSec *pInterfaceLock, // Locking object
                                       __in_opt LPCTSTR pName,        // Object description
                                       __inout_opt LPUNKNOWN pUnk,    // Normal COM ownership
                                       __inout HRESULT *phr)
    : // OLE return code

    CBaseVideoWindow(pName, pUnk)
    , m_pInterfaceLock(pInterfaceLock)
    , m_hwndOwner(NULL)
    , m_hwndDrain(NULL)
    , m_bAutoShow(TRUE)
    , m_pFilter(pFilter)
    , m_bCursorHidden(FALSE)
    , m_pPin(NULL)
{
    ASSERT(m_pFilter);
    ASSERT(m_pInterfaceLock);
    ASSERT(phr);
    m_BorderColour = VIDEO_COLOUR;
}

// Set the title caption on the base window, we don't do any field checking
// as we really don't care what title they intend to have. We can always get
// it back again later with GetWindowText. The only other complication is to
// do the necessary string conversions between ANSI and OLE Unicode strings

STDMETHODIMP CBaseControlWindow::put_Caption(__in BSTR strCaption)
{
    CheckPointer((PVOID)strCaption, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
#ifdef UNICODE
    SetWindowText(m_hwnd, strCaption);
#else
    CHAR Caption[CAPTION];

    WideCharToMultiByte(CP_ACP, 0, strCaption, -1, Caption, CAPTION, NULL, NULL);
    SetWindowText(m_hwnd, Caption);
#endif
    return NOERROR;
}

// Get the current base window title caption, once again we do no real field
// checking. We allocate a string for the window title to be filled in with
// which ensures the interface doesn't fiddle around with getting memory. A
// BSTR is a normal C string with the length at position (-1), we use the
// WriteBSTR helper function to create the caption to try and avoid OLE32

STDMETHODIMP CBaseControlWindow::get_Caption(__out BSTR *pstrCaption)
{
    CheckPointer(pstrCaption, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    WCHAR WideCaption[CAPTION];

#ifdef UNICODE
    GetWindowText(m_hwnd, WideCaption, CAPTION);
#else
    // Convert the ASCII caption to a UNICODE string

    TCHAR Caption[CAPTION];
    GetWindowText(m_hwnd, Caption, CAPTION);
    MultiByteToWideChar(CP_ACP, 0, Caption, -1, WideCaption, CAPTION);
#endif
    return WriteBSTR(pstrCaption, WideCaption);
}

// Set the window style using GWL_EXSTYLE

STDMETHODIMP CBaseControlWindow::put_WindowStyleEx(long WindowStyleEx)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);

    // Should we be taking off WS_EX_TOPMOST

    if (GetWindowLong(m_hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)
    {
        if ((WindowStyleEx & WS_EX_TOPMOST) == 0)
        {
            SendMessage(m_hwnd, m_ShowStageTop, (WPARAM)FALSE, (LPARAM)0);
        }
    }

    // Likewise should we be adding WS_EX_TOPMOST

    if (WindowStyleEx & WS_EX_TOPMOST)
    {
        SendMessage(m_hwnd, m_ShowStageTop, (WPARAM)TRUE, (LPARAM)0);
        WindowStyleEx &= (~WS_EX_TOPMOST);
        if (WindowStyleEx == 0)
            return NOERROR;
    }
    return DoSetWindowStyle(WindowStyleEx, GWL_EXSTYLE);
}

// Gets the current GWL_EXSTYLE base window style

STDMETHODIMP CBaseControlWindow::get_WindowStyleEx(__out long *pWindowStyleEx)
{
    CheckPointer(pWindowStyleEx, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    return DoGetWindowStyle(pWindowStyleEx, GWL_EXSTYLE);
}

// Set the window style using GWL_STYLE

STDMETHODIMP CBaseControlWindow::put_WindowStyle(long WindowStyle)
{
    // These styles cannot be changed dynamically

    if ((WindowStyle & WS_DISABLED) || (WindowStyle & WS_ICONIC) || (WindowStyle & WS_MAXIMIZE) ||
        (WindowStyle & WS_MINIMIZE) || (WindowStyle & WS_HSCROLL) || (WindowStyle & WS_VSCROLL))
    {

        return E_INVALIDARG;
    }

    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    return DoSetWindowStyle(WindowStyle, GWL_STYLE);
}

// Get the current GWL_STYLE base window style

STDMETHODIMP CBaseControlWindow::get_WindowStyle(__out long *pWindowStyle)
{
    CheckPointer(pWindowStyle, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    return DoGetWindowStyle(pWindowStyle, GWL_STYLE);
}

// Change the base window style or the extended styles depending on whether
// WindowLong is GWL_STYLE or GWL_EXSTYLE. We must call SetWindowPos to have
// the window displayed in it's new style after the change which is a little
// tricky if the window is not currently visible as we realise it offscreen.
// In most cases the client will call get_WindowStyle before they call this
// and then AND and OR in extra bit settings according to the requirements

HRESULT CBaseControlWindow::DoSetWindowStyle(long Style, long WindowLong)
{
    RECT WindowRect;

    // Get the window's visibility before setting the style
    BOOL bVisible = IsWindowVisible(m_hwnd);
    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));

    // Set the new style flags for the window
    SetWindowLong(m_hwnd, WindowLong, Style);
    UINT WindowFlags = SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE;
    WindowFlags |= SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;

    // Show the window again in the current position

    if (bVisible == TRUE)
    {

        SetWindowPos(m_hwnd,       // Base window handle
                     HWND_TOP,     // Just a place holder
                     0, 0, 0, 0,   // Leave size and position
                     WindowFlags); // Just draw it again

        return NOERROR;
    }

    // Move the window offscreen so the user doesn't see the changes

    MoveWindow((HWND)m_hwnd,                  // Base window handle
               GetSystemMetrics(SM_CXSCREEN), // Current desktop width
               GetSystemMetrics(SM_CYSCREEN), // Likewise it's height
               WIDTH(&WindowRect),            // Use the same width
               HEIGHT(&WindowRect),           // Keep height same to
               TRUE);                         // May as well repaint

    // Now show the previously hidden window

    SetWindowPos(m_hwnd,       // Base window handle
                 HWND_TOP,     // Just a place holder
                 0, 0, 0, 0,   // Leave size and position
                 WindowFlags); // Just draw it again

    ShowWindow(m_hwnd, SW_HIDE);

    if (GetParent(m_hwnd))
    {

        MapWindowPoints(HWND_DESKTOP, GetParent(m_hwnd), (LPPOINT)&WindowRect, 2);
    }

    MoveWindow((HWND)m_hwnd,        // Base window handle
               WindowRect.left,     // Existing x coordinate
               WindowRect.top,      // Existing y coordinate
               WIDTH(&WindowRect),  // Use the same width
               HEIGHT(&WindowRect), // Keep height same to
               TRUE);               // May as well repaint

    return NOERROR;
}

// Get the current base window style (either GWL_STYLE or GWL_EXSTYLE)

HRESULT CBaseControlWindow::DoGetWindowStyle(__out long *pStyle, long WindowLong)
{
    *pStyle = GetWindowLong(m_hwnd, WindowLong);
    return NOERROR;
}

// Change the visibility of the base window, this takes the same parameters
// as the ShowWindow Win32 API does, so the client can have the window hidden
// or shown, minimised to an icon, or maximised to play in full screen mode
// We pass the request on to the base window to actually make the change

STDMETHODIMP CBaseControlWindow::put_WindowState(long WindowState)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    DoShowWindow(WindowState);
    return NOERROR;
}

// Get the current window state, this function returns a subset of the SW bit
// settings available in ShowWindow, if the window is visible then SW_SHOW is
// set, if it is hidden then the SW_HIDDEN is set, if it is either minimised
// or maximised then the SW_MINIMIZE or SW_MAXIMIZE is set respectively. The
// other SW bit settings are really set commands not readable output values

STDMETHODIMP CBaseControlWindow::get_WindowState(__out long *pWindowState)
{
    CheckPointer(pWindowState, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    ASSERT(pWindowState);
    *pWindowState = FALSE;

    // Is the window visible, a window is termed visible if it is somewhere on
    // the current desktop even if it is completely obscured by other windows
    // so the flag is a style for each window set with the WS_VISIBLE bit

    if (IsWindowVisible(m_hwnd) == TRUE)
    {

        // Is the base window iconic
        if (IsIconic(m_hwnd) == TRUE)
        {
            *pWindowState |= SW_MINIMIZE;
        }

        // Has the window been maximised
        else if (IsZoomed(m_hwnd) == TRUE)
        {
            *pWindowState |= SW_MAXIMIZE;
        }

        // Window is normal
        else
        {
            *pWindowState |= SW_SHOW;
        }
    }
    else
    {
        *pWindowState |= SW_HIDE;
    }
    return NOERROR;
}

// This makes sure that any palette we realise in the base window (through a
// media type or through the overlay interface) is done in the background and
// is therefore mapped to existing device entries rather than taking it over
// as it will do when we this window gets the keyboard focus. An application
// uses this to make sure it doesn't have it's palette removed by the window

STDMETHODIMP CBaseControlWindow::put_BackgroundPalette(long BackgroundPalette)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cWindowLock(&m_WindowLock);

    // Check this is a valid automation boolean type

    if (BackgroundPalette != OATRUE)
    {
        if (BackgroundPalette != OAFALSE)
        {
            return E_INVALIDARG;
        }
    }

    // Make sure the window realises any palette it has again

    m_bBackground = (BackgroundPalette == OATRUE ? TRUE : FALSE);
    PostMessage(m_hwnd, m_RealizePalette, 0, 0);
    PaintWindow(FALSE);

    return NOERROR;
}

// This returns the current background realisation setting

STDMETHODIMP
CBaseControlWindow::get_BackgroundPalette(__out long *pBackgroundPalette)
{
    CheckPointer(pBackgroundPalette, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cWindowLock(&m_WindowLock);

    // Get the current background palette setting

    *pBackgroundPalette = (m_bBackground == TRUE ? OATRUE : OAFALSE);
    return NOERROR;
}

// Change the visibility of the base window

STDMETHODIMP CBaseControlWindow::put_Visible(long Visible)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);

    // Check this is a valid automation boolean type

    if (Visible != OATRUE)
    {
        if (Visible != OAFALSE)
        {
            return E_INVALIDARG;
        }
    }

    // Convert the boolean visibility into SW_SHOW and SW_HIDE

    INT Mode = (Visible == OATRUE ? SW_SHOWNORMAL : SW_HIDE);
    DoShowWindow(Mode);
    return NOERROR;
}

// Return OATRUE if the window is currently visible otherwise OAFALSE

STDMETHODIMP CBaseControlWindow::get_Visible(__out long *pVisible)
{
    CheckPointer(pVisible, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);

    // See if the base window has a WS_VISIBLE style - this will return TRUE
    // even if the window is completely obscured by other desktop windows, we
    // return FALSE if the window is not showing because of earlier calls

    BOOL Mode = IsWindowVisible(m_hwnd);
    *pVisible = (Mode == TRUE ? OATRUE : OAFALSE);
    return NOERROR;
}

// Change the left position of the base window. This keeps the window width
// and height properties the same so it effectively shunts the window left or
// right accordingly - there is the Width property to change that dimension

STDMETHODIMP CBaseControlWindow::put_Left(long Left)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    BOOL bSuccess;
    RECT WindowRect;

    // Get the current window position in a RECT
    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));

    if (GetParent(m_hwnd))
    {

        MapWindowPoints(HWND_DESKTOP, GetParent(m_hwnd), (LPPOINT)&WindowRect, 2);
    }

    // Adjust the coordinates ready for SetWindowPos, the window rectangle we
    // get back from GetWindowRect is in left,top,right and bottom while the
    // coordinates SetWindowPos wants are left,top,width and height values

    WindowRect.bottom = WindowRect.bottom - WindowRect.top;
    WindowRect.right = WindowRect.right - WindowRect.left;
    UINT WindowFlags = SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE;

    bSuccess = SetWindowPos(m_hwnd,            // Window handle
                            HWND_TOP,          // Put it at the top
                            Left,              // New left position
                            WindowRect.top,    // Leave top alone
                            WindowRect.right,  // The WIDTH (not right)
                            WindowRect.bottom, // The HEIGHT (not bottom)
                            WindowFlags);      // Show window options

    if (bSuccess == FALSE)
    {
        return E_INVALIDARG;
    }
    return NOERROR;
}

// Return the current base window left position

STDMETHODIMP CBaseControlWindow::get_Left(__out long *pLeft)
{
    CheckPointer(pLeft, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    RECT WindowRect;

    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));
    *pLeft = WindowRect.left;
    return NOERROR;
}

// Change the current width of the base window. This property complements the
// left position property so we must keep the left edge constant and expand or
// contract to the right, the alternative would be to change the left edge so
// keeping the right edge constant but this is maybe a little more intuitive

STDMETHODIMP CBaseControlWindow::put_Width(long Width)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    BOOL bSuccess;
    RECT WindowRect;

    // Adjust the coordinates ready for SetWindowPos, the window rectangle we
    // get back from GetWindowRect is in left,top,right and bottom while the
    // coordinates SetWindowPos wants are left,top,width and height values

    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));

    if (GetParent(m_hwnd))
    {

        MapWindowPoints(HWND_DESKTOP, GetParent(m_hwnd), (LPPOINT)&WindowRect, 2);
    }

    WindowRect.bottom = WindowRect.bottom - WindowRect.top;
    UINT WindowFlags = SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE;

    // This seems to have a bug in that calling SetWindowPos on a window with
    // just the width changing causes it to ignore the width that you pass in
    // and sets it to a mimimum value of 110 pixels wide (Windows NT 3.51)

    bSuccess = SetWindowPos(m_hwnd,            // Window handle
                            HWND_TOP,          // Put it at the top
                            WindowRect.left,   // Leave left alone
                            WindowRect.top,    // Leave top alone
                            Width,             // New WIDTH dimension
                            WindowRect.bottom, // The HEIGHT (not bottom)
                            WindowFlags);      // Show window options

    if (bSuccess == FALSE)
    {
        return E_INVALIDARG;
    }
    return NOERROR;
}

// Return the current base window width

STDMETHODIMP CBaseControlWindow::get_Width(__out long *pWidth)
{
    CheckPointer(pWidth, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    RECT WindowRect;

    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));
    *pWidth = WindowRect.right - WindowRect.left;
    return NOERROR;
}

// This allows the client program to change the top position for the window in
// the same way that changing the left position does not affect the width of
// the image so changing the top position does not affect the window height

STDMETHODIMP CBaseControlWindow::put_Top(long Top)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    BOOL bSuccess;
    RECT WindowRect;

    // Get the current window position in a RECT
    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));

    if (GetParent(m_hwnd))
    {

        MapWindowPoints(HWND_DESKTOP, GetParent(m_hwnd), (LPPOINT)&WindowRect, 2);
    }

    // Adjust the coordinates ready for SetWindowPos, the window rectangle we
    // get back from GetWindowRect is in left,top,right and bottom while the
    // coordinates SetWindowPos wants are left,top,width and height values

    WindowRect.bottom = WindowRect.bottom - WindowRect.top;
    WindowRect.right = WindowRect.right - WindowRect.left;
    UINT WindowFlags = SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE;

    bSuccess = SetWindowPos(m_hwnd,            // Window handle
                            HWND_TOP,          // Put it at the top
                            WindowRect.left,   // Leave left alone
                            Top,               // New top position
                            WindowRect.right,  // The WIDTH (not right)
                            WindowRect.bottom, // The HEIGHT (not bottom)
                            WindowFlags);      // Show window flags

    if (bSuccess == FALSE)
    {
        return E_INVALIDARG;
    }
    return NOERROR;
}

// Return the current base window top position

STDMETHODIMP CBaseControlWindow::get_Top(long *pTop)
{
    CheckPointer(pTop, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    RECT WindowRect;

    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));
    *pTop = WindowRect.top;
    return NOERROR;
}

// Change the height of the window, this complements the top property so when
// we change this we must keep the top position for the base window, as said
// before we could keep the bottom and grow upwards although this is perhaps
// a little more intuitive since we already have a top position property

STDMETHODIMP CBaseControlWindow::put_Height(long Height)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    BOOL bSuccess;
    RECT WindowRect;

    // Adjust the coordinates ready for SetWindowPos, the window rectangle we
    // get back from GetWindowRect is in left,top,right and bottom while the
    // coordinates SetWindowPos wants are left,top,width and height values

    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));

    if (GetParent(m_hwnd))
    {

        MapWindowPoints(HWND_DESKTOP, GetParent(m_hwnd), (LPPOINT)&WindowRect, 2);
    }

    WindowRect.right = WindowRect.right - WindowRect.left;
    UINT WindowFlags = SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE;

    bSuccess = SetWindowPos(m_hwnd,           // Window handle
                            HWND_TOP,         // Put it at the top
                            WindowRect.left,  // Leave left alone
                            WindowRect.top,   // Leave top alone
                            WindowRect.right, // The WIDTH (not right)
                            Height,           // New height dimension
                            WindowFlags);     // Show window flags

    if (bSuccess == FALSE)
    {
        return E_INVALIDARG;
    }
    return NOERROR;
}

// Return the current base window height

STDMETHODIMP CBaseControlWindow::get_Height(__out long *pHeight)
{
    CheckPointer(pHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    RECT WindowRect;

    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));
    *pHeight = WindowRect.bottom - WindowRect.top;
    return NOERROR;
}

// This can be called to change the owning window. Setting the owner is done
// through this function, however to make the window a true child window the
// style must also be set to WS_CHILD. After resetting the owner to NULL an
// application should also set the style to WS_OVERLAPPED | WS_CLIPCHILDREN.

// We cannot lock the object here because the SetParent causes an interthread
// SendMessage to the owner window. If they are in GetState we will sit here
// incomplete with the critical section locked therefore blocking out source
// filter threads from accessing us. Because the source thread can't enter us
// it can't get buffers or call EndOfStream so the GetState will not complete

STDMETHODIMP CBaseControlWindow::put_Owner(OAHWND Owner)
{
    // Check we are connected otherwise reject the call

    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    m_hwndOwner = (HWND)Owner;
    HWND hwndParent = m_hwndOwner;

    // Add or remove WS_CHILD as appropriate

    LONG Style = GetWindowLong(m_hwnd, GWL_STYLE);
    if (Owner == NULL)
    {
        Style &= (~WS_CHILD);
    }
    else
    {
        Style |= (WS_CHILD);
    }
    SetWindowLong(m_hwnd, GWL_STYLE, Style);

    // Don't call this with the filter locked

    SetParent(m_hwnd, hwndParent);

    PaintWindow(TRUE);
    NOTE1("Changed parent %lx", hwndParent);

    return NOERROR;
}

// This complements the put_Owner to get the current owning window property
// we always return NOERROR although the returned window handle may be NULL
// to indicate no owning window (the desktop window doesn't qualify as one)
// If an application sets the owner we call SetParent, however that returns
// NULL until the WS_CHILD bit is set on, so we store the owner internally

STDMETHODIMP CBaseControlWindow::get_Owner(__out OAHWND *Owner)
{
    CheckPointer(Owner, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    *Owner = (OAHWND)m_hwndOwner;
    return NOERROR;
}

// And renderer supporting IVideoWindow may have an HWND set who will get any
// keyboard and mouse messages we receive posted on to them. This is separate
// from setting an owning window. By separating the two, applications may get
// messages sent on even when they have set no owner (perhaps it's maximised)

STDMETHODIMP CBaseControlWindow::put_MessageDrain(OAHWND Drain)
{
    // Check we are connected otherwise reject the call

    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    m_hwndDrain = (HWND)Drain;
    return NOERROR;
}

// Return the current message drain

STDMETHODIMP CBaseControlWindow::get_MessageDrain(__out OAHWND *Drain)
{
    CheckPointer(Drain, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    *Drain = (OAHWND)m_hwndDrain;
    return NOERROR;
}

// This is called by the filter graph to inform us of a message we should know
// is being sent to our owning window. We have this because as a child window
// we do not get certain messages that are only sent to top level windows. We
// must see the palette changed/changing/query messages so that we know if we
// have the foreground palette or not. We pass the message on to our window
// using SendMessage - this will cause an interthread send message to occur

STDMETHODIMP
CBaseControlWindow::NotifyOwnerMessage(OAHWND hwnd,     // Window handle
                                       long uMsg,       // Message ID
                                       LONG_PTR wParam, // Parameters
                                       LONG_PTR lParam) // for message
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);

    // Only interested in these Windows messages

    switch (uMsg)
    {

    case WM_SYSCOLORCHANGE:
    case WM_PALETTECHANGED:
    case WM_PALETTEISCHANGING:
    case WM_QUERYNEWPALETTE:
    case WM_DEVMODECHANGE:
    case WM_DISPLAYCHANGE:
    case WM_ACTIVATEAPP:

        // If we do not have an owner then ignore

        if (m_hwndOwner == NULL)
        {
            return NOERROR;
        }
        SendMessage(m_hwnd, uMsg, (WPARAM)wParam, (LPARAM)lParam);
        break;

    // do NOT fwd WM_MOVE. the parameters are the location of the parent
    // window, NOT what the renderer should be looking at.  But we need
    // to make sure the overlay is moved with the parent window, so we
    // do this.
    case WM_MOVE: PostMessage(m_hwnd, WM_PAINT, 0, 0); break;
    }
    return NOERROR;
}

// Allow an application to have us set the base window in the foreground. We
// have this because it is difficult for one thread to do do this to a window
// owned by another thread. We ask the base window class to do the real work

STDMETHODIMP CBaseControlWindow::SetWindowForeground(long Focus)
{
    // Check this is a valid automation boolean type

    if (Focus != OATRUE)
    {
        if (Focus != OAFALSE)
        {
            return E_INVALIDARG;
        }
    }

    // We shouldn't lock as this sends a message

    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    BOOL bFocus = (Focus == OATRUE ? TRUE : FALSE);
    DoSetWindowForeground(bFocus);

    return NOERROR;
}

// This allows a client to set the complete window size and position in one
// atomic operation. The same affect can be had by changing each dimension
// in turn through their individual properties although some flashing will
// occur as each of them gets updated (they are better set at design time)

STDMETHODIMP
CBaseControlWindow::SetWindowPosition(long Left, long Top, long Width, long Height)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    BOOL bSuccess;

    // Set the new size and position
    UINT WindowFlags = SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE;

    ASSERT(IsWindow(m_hwnd));
    bSuccess = SetWindowPos(m_hwnd,       // Window handle
                            HWND_TOP,     // Put it at the top
                            Left,         // Left position
                            Top,          // Top position
                            Width,        // Window width
                            Height,       // Window height
                            WindowFlags); // Show window flags
    ASSERT(bSuccess);
#ifdef DEBUG
    DbgLog((LOG_TRACE, 1, TEXT("SWP failed error %d"), GetLastError()));
#endif
    if (bSuccess == FALSE)
    {
        return E_INVALIDARG;
    }
    return NOERROR;
}

// This complements the SetWindowPosition to return the current window place
// in device coordinates. As before the same information can be retrived by
// calling the property get functions individually but this is atomic and is
// therefore more suitable to a live environment rather than design time

STDMETHODIMP
CBaseControlWindow::GetWindowPosition(__out long *pLeft, __out long *pTop, __out long *pWidth, __out long *pHeight)
{
    // Should check the pointers are not NULL

    CheckPointer(pLeft, E_POINTER);
    CheckPointer(pTop, E_POINTER);
    CheckPointer(pWidth, E_POINTER);
    CheckPointer(pHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    RECT WindowRect;

    // Get the current window coordinates

    EXECUTE_ASSERT(GetWindowRect(m_hwnd, &WindowRect));

    // Convert the RECT into left,top,width and height values

    *pLeft = WindowRect.left;
    *pTop = WindowRect.top;
    *pWidth = WindowRect.right - WindowRect.left;
    *pHeight = WindowRect.bottom - WindowRect.top;

    return NOERROR;
}

// When a window is maximised or iconic calling GetWindowPosition will return
// the current window position (likewise for the properties). However if the
// restored size (ie the size we'll return to when normally shown) is needed
// then this should be used. When in a normal position (neither iconic nor
// maximised) then this returns the same coordinates as GetWindowPosition

STDMETHODIMP
CBaseControlWindow::GetRestorePosition(__out long *pLeft, __out long *pTop, __out long *pWidth, __out long *pHeight)
{
    // Should check the pointers are not NULL

    CheckPointer(pLeft, E_POINTER);
    CheckPointer(pTop, E_POINTER);
    CheckPointer(pWidth, E_POINTER);
    CheckPointer(pHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);

    // Use GetWindowPlacement to find the restore position

    WINDOWPLACEMENT Place;
    Place.length = sizeof(WINDOWPLACEMENT);
    EXECUTE_ASSERT(GetWindowPlacement(m_hwnd, &Place));

    RECT WorkArea;

    // We must take into account any task bar present

    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkArea, FALSE) == TRUE)
    {
        if (GetParent(m_hwnd) == NULL)
        {
            Place.rcNormalPosition.top += WorkArea.top;
            Place.rcNormalPosition.bottom += WorkArea.top;
            Place.rcNormalPosition.left += WorkArea.left;
            Place.rcNormalPosition.right += WorkArea.left;
        }
    }

    // Convert the RECT into left,top,width and height values

    *pLeft = Place.rcNormalPosition.left;
    *pTop = Place.rcNormalPosition.top;
    *pWidth = Place.rcNormalPosition.right - Place.rcNormalPosition.left;
    *pHeight = Place.rcNormalPosition.bottom - Place.rcNormalPosition.top;

    return NOERROR;
}

// Return the current border colour, if we are playing something to a subset
// of the base window display there is an outside area exposed. The default
// action is to paint this colour in the Windows background colour (defined
// as value COLOR_WINDOW) We reset to this default when we're disconnected

STDMETHODIMP CBaseControlWindow::get_BorderColor(__out long *Color)
{
    CheckPointer(Color, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    *Color = (long)m_BorderColour;
    return NOERROR;
}

// This can be called to set the current border colour

STDMETHODIMP CBaseControlWindow::put_BorderColor(long Color)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);

    // Have the window repainted with the new border colour

    m_BorderColour = (COLORREF)Color;
    PaintWindow(TRUE);
    return NOERROR;
}

// Delegate fullscreen handling to plug in distributor

STDMETHODIMP CBaseControlWindow::get_FullScreenMode(__out long *FullScreenMode)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CheckPointer(FullScreenMode, E_POINTER);
    return E_NOTIMPL;
}

// Delegate fullscreen handling to plug in distributor

STDMETHODIMP CBaseControlWindow::put_FullScreenMode(long FullScreenMode)
{
    return E_NOTIMPL;
}

// This sets the auto show property, this property causes the base window to
// be displayed whenever we change state. This allows an application to have
// to do nothing to have the window appear but still allow them to change the
// default behaviour if for example they want to keep it hidden for longer

STDMETHODIMP CBaseControlWindow::put_AutoShow(long AutoShow)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);

    // Check this is a valid automation boolean type

    if (AutoShow != OATRUE)
    {
        if (AutoShow != OAFALSE)
        {
            return E_INVALIDARG;
        }
    }

    m_bAutoShow = (AutoShow == OATRUE ? TRUE : FALSE);
    return NOERROR;
}

// This can be called to get the current auto show flag. The flag is updated
// when we connect and disconnect and through this interface all of which are
// controlled and serialised by means of the main renderer critical section

STDMETHODIMP CBaseControlWindow::get_AutoShow(__out long *AutoShow)
{
    CheckPointer(AutoShow, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    *AutoShow = (m_bAutoShow == TRUE ? OATRUE : OAFALSE);
    return NOERROR;
}

// Return the minimum ideal image size for the current video. This may differ
// to the actual video dimensions because we may be using DirectDraw hardware
// that has specific stretching requirements. For example the Cirrus Logic
// cards have a minimum stretch factor depending on the overlay surface size

STDMETHODIMP
CBaseControlWindow::GetMinIdealImageSize(__out long *pWidth, __out long *pHeight)
{
    CheckPointer(pWidth, E_POINTER);
    CheckPointer(pHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    FILTER_STATE State;

    // Must not be stopped for this to work correctly

    m_pFilter->GetState(0, &State);
    if (State == State_Stopped)
    {
        return VFW_E_WRONG_STATE;
    }

    RECT DefaultRect = GetDefaultRect();
    *pWidth = WIDTH(&DefaultRect);
    *pHeight = HEIGHT(&DefaultRect);
    return NOERROR;
}

// Return the maximum ideal image size for the current video. This may differ
// to the actual video dimensions because we may be using DirectDraw hardware
// that has specific stretching requirements. For example the Cirrus Logic
// cards have a maximum stretch factor depending on the overlay surface size

STDMETHODIMP
CBaseControlWindow::GetMaxIdealImageSize(__out long *pWidth, __out long *pHeight)
{
    CheckPointer(pWidth, E_POINTER);
    CheckPointer(pHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    FILTER_STATE State;

    // Must not be stopped for this to work correctly

    m_pFilter->GetState(0, &State);
    if (State == State_Stopped)
    {
        return VFW_E_WRONG_STATE;
    }

    RECT DefaultRect = GetDefaultRect();
    *pWidth = WIDTH(&DefaultRect);
    *pHeight = HEIGHT(&DefaultRect);
    return NOERROR;
}

// Allow an application to hide the cursor on our window

STDMETHODIMP
CBaseControlWindow::HideCursor(long HideCursor)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);

    // Check this is a valid automation boolean type

    if (HideCursor != OATRUE)
    {
        if (HideCursor != OAFALSE)
        {
            return E_INVALIDARG;
        }
    }

    m_bCursorHidden = (HideCursor == OATRUE ? TRUE : FALSE);
    return NOERROR;
}

// Returns whether we have the cursor hidden or not

STDMETHODIMP CBaseControlWindow::IsCursorHidden(__out long *CursorHidden)
{
    CheckPointer(CursorHidden, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    *CursorHidden = (m_bCursorHidden == TRUE ? OATRUE : OAFALSE);
    return NOERROR;
}

// This class implements the IBasicVideo control functions (dual interface)
// we support a large number of properties and methods designed to allow the
// client (whether it be an automation controller or a C/C++ application) to
// set and get a number of video related properties such as the native video
// size. We support some methods that duplicate the properties but provide a
// more direct and efficient mechanism as many values may be changed in one

CBaseControlVideo::CBaseControlVideo(__inout CBaseFilter *pFilter,  // Owning filter
                                     __in CCritSec *pInterfaceLock, // Locking object
                                     __in_opt LPCTSTR pName,        // Object description
                                     __inout_opt LPUNKNOWN pUnk,    // Normal COM ownership
                                     __inout HRESULT *phr)
    : // OLE return code

    CBaseBasicVideo(pName, pUnk)
    , m_pFilter(pFilter)
    , m_pInterfaceLock(pInterfaceLock)
    , m_pPin(NULL)
{
    ASSERT(m_pFilter);
    ASSERT(m_pInterfaceLock);
    ASSERT(phr);
}

// Return an approximate average time per frame

STDMETHODIMP CBaseControlVideo::get_AvgTimePerFrame(__out REFTIME *pAvgTimePerFrame)
{
    CheckPointer(pAvgTimePerFrame, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);

    VIDEOINFOHEADER *pVideoInfo = GetVideoFormat();
    if (pVideoInfo == NULL)
        return E_OUTOFMEMORY;
    COARefTime AvgTime(pVideoInfo->AvgTimePerFrame);
    *pAvgTimePerFrame = (REFTIME)AvgTime;

    return NOERROR;
}

// Return an approximate bit rate for the video

STDMETHODIMP CBaseControlVideo::get_BitRate(__out long *pBitRate)
{
    CheckPointer(pBitRate, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);

    VIDEOINFOHEADER *pVideoInfo = GetVideoFormat();
    if (pVideoInfo == NULL)
        return E_OUTOFMEMORY;
    *pBitRate = pVideoInfo->dwBitRate;
    return NOERROR;
}

// Return an approximate bit error rate

STDMETHODIMP CBaseControlVideo::get_BitErrorRate(__out long *pBitErrorRate)
{
    CheckPointer(pBitErrorRate, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);

    VIDEOINFOHEADER *pVideoInfo = GetVideoFormat();
    if (pVideoInfo == NULL)
        return E_OUTOFMEMORY;
    *pBitErrorRate = pVideoInfo->dwBitErrorRate;
    return NOERROR;
}

// This returns the current video width

STDMETHODIMP CBaseControlVideo::get_VideoWidth(__out long *pVideoWidth)
{
    CheckPointer(pVideoWidth, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);

    VIDEOINFOHEADER *pVideoInfo = GetVideoFormat();
    if (pVideoInfo == NULL)
        return E_OUTOFMEMORY;
    *pVideoWidth = pVideoInfo->bmiHeader.biWidth;
    return NOERROR;
}

// This returns the current video height

STDMETHODIMP CBaseControlVideo::get_VideoHeight(__out long *pVideoHeight)
{
    CheckPointer(pVideoHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);

    VIDEOINFOHEADER *pVideoInfo = GetVideoFormat();
    if (pVideoInfo == NULL)
        return E_OUTOFMEMORY;
    *pVideoHeight = pVideoInfo->bmiHeader.biHeight;
    return NOERROR;
}

// This returns the current palette the video is using as an array allocated
// by the user. To remain consistent we use PALETTEENTRY fields to return the
// colours in rather than RGBQUADs that multimedia decided to use. The memory
// is allocated by the user so we simple copy each in turn. We check that the
// number of entries requested and the start position offset are both valid
// If the number of entries evaluates to zero then we return an S_FALSE code

STDMETHODIMP CBaseControlVideo::GetVideoPaletteEntries(long StartIndex, long Entries, __out long *pRetrieved,
                                                       __out_ecount_part(Entries, *pRetrieved) long *pPalette)
{
    CheckPointer(pRetrieved, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    CMediaType MediaType;

    // Get the video format from the derived class

    VIDEOINFOHEADER *pVideoInfo = GetVideoFormat();
    if (pVideoInfo == NULL)
        return E_OUTOFMEMORY;
    BITMAPINFOHEADER *pHeader = HEADER(pVideoInfo);

    // Is the current format palettised

    if (PALETTISED(pVideoInfo) == FALSE)
    {
        *pRetrieved = 0;
        return VFW_E_NO_PALETTE_AVAILABLE;
    }

    // Do they just want to know how many are available

    if (pPalette == NULL)
    {
        *pRetrieved = pHeader->biClrUsed;
        return NOERROR;
    }

    // Make sure the start position is a valid offset

    if (StartIndex >= (LONG)pHeader->biClrUsed || StartIndex < 0)
    {
        *pRetrieved = 0;
        return E_INVALIDARG;
    }

    // Correct the number we can retrieve

    LONG Available = (LONG)pHeader->biClrUsed - StartIndex;
    *pRetrieved = max(0, min(Available, Entries));
    if (*pRetrieved == 0)
    {
        return S_FALSE;
    }

    // Copy the palette entries to the output buffer

    PALETTEENTRY *pEntries = (PALETTEENTRY *)pPalette;
    RGBQUAD *pColours = COLORS(pVideoInfo) + StartIndex;

    for (LONG Count = 0; Count < *pRetrieved; Count++)
    {
        pEntries[Count].peRed = pColours[Count].rgbRed;
        pEntries[Count].peGreen = pColours[Count].rgbGreen;
        pEntries[Count].peBlue = pColours[Count].rgbBlue;
        pEntries[Count].peFlags = 0;
    }
    return NOERROR;
}

// This returns the current video dimensions as a method rather than a number
// of individual property get calls. For the same reasons as said before we
// cannot access the renderer media type directly as the window object thread
// may be updating it since dynamic format changes may change these values

STDMETHODIMP CBaseControlVideo::GetVideoSize(__out long *pWidth, __out long *pHeight)
{
    CheckPointer(pWidth, E_POINTER);
    CheckPointer(pHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);

    // Get the video format from the derived class
    VIDEOINFOHEADER *pVideoInfo = GetVideoFormat();
    if (pVideoInfo == NULL)
        return E_OUTOFMEMORY;
    *pWidth = pVideoInfo->bmiHeader.biWidth;
    *pHeight = pVideoInfo->bmiHeader.biHeight;
    return NOERROR;
}

// Set the source video rectangle as left,top,right and bottom coordinates
// rather than left,top,width and height as per OLE automation interfaces
// Then pass the rectangle on to the window object to set the source

STDMETHODIMP
CBaseControlVideo::SetSourcePosition(long Left, long Top, long Width, long Height)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT SourceRect;
    SourceRect.left = Left;
    SourceRect.top = Top;
    SourceRect.right = Left + Width;
    SourceRect.bottom = Top + Height;

    // Check the source rectangle is valid

    HRESULT hr = CheckSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the source rectangle

    hr = SetSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the source rectangle in left,top,width and height rather than the
// left,top,right and bottom values that RECT uses (and which the window
// object returns through GetSourceRect) which requires a little work

STDMETHODIMP
CBaseControlVideo::GetSourcePosition(__out long *pLeft, __out long *pTop, __out long *pWidth, __out long *pHeight)
{
    // Should check the pointers are non NULL

    CheckPointer(pLeft, E_POINTER);
    CheckPointer(pTop, E_POINTER);
    CheckPointer(pWidth, E_POINTER);
    CheckPointer(pHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    RECT SourceRect;

    CAutoLock cInterfaceLock(m_pInterfaceLock);
    GetSourceRect(&SourceRect);

    *pLeft = SourceRect.left;
    *pTop = SourceRect.top;
    *pWidth = WIDTH(&SourceRect);
    *pHeight = HEIGHT(&SourceRect);

    return NOERROR;
}

// Set the video destination as left,top,right and bottom coordinates rather
// than the left,top,width and height uses as per OLE automation interfaces
// Then pass the rectangle on to the window object to set the destination

STDMETHODIMP
CBaseControlVideo::SetDestinationPosition(long Left, long Top, long Width, long Height)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT DestinationRect;

    DestinationRect.left = Left;
    DestinationRect.top = Top;
    DestinationRect.right = Left + Width;
    DestinationRect.bottom = Top + Height;

    // Check the target rectangle is valid

    HRESULT hr = CheckTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the new target rectangle

    hr = SetTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the destination rectangle in left,top,width and height rather than
// the left,top,right and bottom values that RECT uses (and which the window
// object returns through GetDestinationRect) which requires a little work

STDMETHODIMP
CBaseControlVideo::GetDestinationPosition(__out long *pLeft, __out long *pTop, __out long *pWidth, __out long *pHeight)
{
    // Should check the pointers are not NULL

    CheckPointer(pLeft, E_POINTER);
    CheckPointer(pTop, E_POINTER);
    CheckPointer(pWidth, E_POINTER);
    CheckPointer(pHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    RECT DestinationRect;

    CAutoLock cInterfaceLock(m_pInterfaceLock);
    GetTargetRect(&DestinationRect);

    *pLeft = DestinationRect.left;
    *pTop = DestinationRect.top;
    *pWidth = WIDTH(&DestinationRect);
    *pHeight = HEIGHT(&DestinationRect);

    return NOERROR;
}

// Set the source left position, the source rectangle we get back from the
// window object is a true rectangle in left,top,right and bottom positions
// so all we have to do is to update the left position and pass it back. We
// must keep the current width constant when we're updating this property

STDMETHODIMP CBaseControlVideo::put_SourceLeft(long SourceLeft)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT SourceRect;
    GetSourceRect(&SourceRect);
    SourceRect.right = SourceLeft + WIDTH(&SourceRect);
    SourceRect.left = SourceLeft;

    // Check the source rectangle is valid

    HRESULT hr = CheckSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the source rectangle

    hr = SetSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the current left source video position

STDMETHODIMP CBaseControlVideo::get_SourceLeft(__out long *pSourceLeft)
{
    CheckPointer(pSourceLeft, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT SourceRect;

    GetSourceRect(&SourceRect);
    *pSourceLeft = SourceRect.left;
    return NOERROR;
}

// Set the source width, we get the current source rectangle and then update
// the right position to be the left position (thereby keeping it constant)
// plus the new source width we are passed in (it expands to the right)

STDMETHODIMP CBaseControlVideo::put_SourceWidth(long SourceWidth)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT SourceRect;
    GetSourceRect(&SourceRect);
    SourceRect.right = SourceRect.left + SourceWidth;

    // Check the source rectangle is valid

    HRESULT hr = CheckSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the source rectangle

    hr = SetSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the current source width

STDMETHODIMP CBaseControlVideo::get_SourceWidth(__out long *pSourceWidth)
{
    CheckPointer(pSourceWidth, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT SourceRect;

    GetSourceRect(&SourceRect);
    *pSourceWidth = WIDTH(&SourceRect);
    return NOERROR;
}

// Set the source top position - changing this property does not affect the
// current source height. So changing this shunts the source rectangle up and
// down appropriately. Changing the height complements this functionality by
// keeping the top position constant and simply changing the source height

STDMETHODIMP CBaseControlVideo::put_SourceTop(long SourceTop)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT SourceRect;
    GetSourceRect(&SourceRect);
    SourceRect.bottom = SourceTop + HEIGHT(&SourceRect);
    SourceRect.top = SourceTop;

    // Check the source rectangle is valid

    HRESULT hr = CheckSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the source rectangle

    hr = SetSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the current top position

STDMETHODIMP CBaseControlVideo::get_SourceTop(__out long *pSourceTop)
{
    CheckPointer(pSourceTop, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT SourceRect;

    GetSourceRect(&SourceRect);
    *pSourceTop = SourceRect.top;
    return NOERROR;
}

// Set the source height

STDMETHODIMP CBaseControlVideo::put_SourceHeight(long SourceHeight)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT SourceRect;
    GetSourceRect(&SourceRect);
    SourceRect.bottom = SourceRect.top + SourceHeight;

    // Check the source rectangle is valid

    HRESULT hr = CheckSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the source rectangle

    hr = SetSourceRect(&SourceRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the current source height

STDMETHODIMP CBaseControlVideo::get_SourceHeight(__out long *pSourceHeight)
{
    CheckPointer(pSourceHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT SourceRect;

    GetSourceRect(&SourceRect);
    *pSourceHeight = HEIGHT(&SourceRect);
    return NOERROR;
}

// Set the target left position, the target rectangle we get back from the
// window object is a true rectangle in left,top,right and bottom positions
// so all we have to do is to update the left position and pass it back. We
// must keep the current width constant when we're updating this property

STDMETHODIMP CBaseControlVideo::put_DestinationLeft(long DestinationLeft)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT DestinationRect;
    GetTargetRect(&DestinationRect);
    DestinationRect.right = DestinationLeft + WIDTH(&DestinationRect);
    DestinationRect.left = DestinationLeft;

    // Check the target rectangle is valid

    HRESULT hr = CheckTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the new target rectangle

    hr = SetTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the left position for the destination rectangle

STDMETHODIMP CBaseControlVideo::get_DestinationLeft(__out long *pDestinationLeft)
{
    CheckPointer(pDestinationLeft, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT DestinationRect;

    GetTargetRect(&DestinationRect);
    *pDestinationLeft = DestinationRect.left;
    return NOERROR;
}

// Set the destination width

STDMETHODIMP CBaseControlVideo::put_DestinationWidth(long DestinationWidth)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT DestinationRect;
    GetTargetRect(&DestinationRect);
    DestinationRect.right = DestinationRect.left + DestinationWidth;

    // Check the target rectangle is valid

    HRESULT hr = CheckTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the new target rectangle

    hr = SetTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the width for the destination rectangle

STDMETHODIMP CBaseControlVideo::get_DestinationWidth(__out long *pDestinationWidth)
{
    CheckPointer(pDestinationWidth, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT DestinationRect;

    GetTargetRect(&DestinationRect);
    *pDestinationWidth = WIDTH(&DestinationRect);
    return NOERROR;
}

// Set the target top position - changing this property does not affect the
// current target height. So changing this shunts the target rectangle up and
// down appropriately. Changing the height complements this functionality by
// keeping the top position constant and simply changing the target height

STDMETHODIMP CBaseControlVideo::put_DestinationTop(long DestinationTop)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT DestinationRect;
    GetTargetRect(&DestinationRect);
    DestinationRect.bottom = DestinationTop + HEIGHT(&DestinationRect);
    DestinationRect.top = DestinationTop;

    // Check the target rectangle is valid

    HRESULT hr = CheckTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the new target rectangle

    hr = SetTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the top position for the destination rectangle

STDMETHODIMP CBaseControlVideo::get_DestinationTop(__out long *pDestinationTop)
{
    CheckPointer(pDestinationTop, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT DestinationRect;

    GetTargetRect(&DestinationRect);
    *pDestinationTop = DestinationRect.top;
    return NOERROR;
}

// Set the destination height

STDMETHODIMP CBaseControlVideo::put_DestinationHeight(long DestinationHeight)
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT DestinationRect;
    GetTargetRect(&DestinationRect);
    DestinationRect.bottom = DestinationRect.top + DestinationHeight;

    // Check the target rectangle is valid

    HRESULT hr = CheckTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now set the new target rectangle

    hr = SetTargetRect(&DestinationRect);
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return the height for the destination rectangle

STDMETHODIMP CBaseControlVideo::get_DestinationHeight(__out long *pDestinationHeight)
{
    CheckPointer(pDestinationHeight, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    RECT DestinationRect;

    GetTargetRect(&DestinationRect);
    *pDestinationHeight = HEIGHT(&DestinationRect);
    return NOERROR;
}

// Reset the source rectangle to the full video dimensions

STDMETHODIMP CBaseControlVideo::SetDefaultSourcePosition()
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    HRESULT hr = SetDefaultSourceRect();
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return S_OK if we're using the default source otherwise S_FALSE

STDMETHODIMP CBaseControlVideo::IsUsingDefaultSource()
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    return IsDefaultSourceRect();
}

// Reset the video renderer to use the entire playback area

STDMETHODIMP CBaseControlVideo::SetDefaultDestinationPosition()
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    HRESULT hr = SetDefaultTargetRect();
    if (FAILED(hr))
    {
        return hr;
    }
    return OnUpdateRectangles();
}

// Return S_OK if we're using the default target otherwise S_FALSE

STDMETHODIMP CBaseControlVideo::IsUsingDefaultDestination()
{
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    return IsDefaultTargetRect();
}

// Return a copy of the current image in the video renderer

STDMETHODIMP
CBaseControlVideo::GetCurrentImage(__inout long *pBufferSize,
                                   __out_bcount_part(*pBufferSize, *pBufferSize) long *pVideoImage)
{
    CheckPointer(pBufferSize, E_POINTER);
    CheckConnected(m_pPin, VFW_E_NOT_CONNECTED);
    CAutoLock cInterfaceLock(m_pInterfaceLock);
    FILTER_STATE State;

    // Make sure we are in a paused state

    if (pVideoImage != NULL)
    {
        m_pFilter->GetState(0, &State);
        if (State != State_Paused)
        {
            return VFW_E_NOT_PAUSED;
        }
        return GetStaticImage(pBufferSize, pVideoImage);
    }

    // Just return the memory required

    VIDEOINFOHEADER *pVideoInfo = GetVideoFormat();
    if (pVideoInfo == NULL)
        return E_OUTOFMEMORY;
    RECT SourceRect;
    GetSourceRect(&SourceRect);
    return GetImageSize(pVideoInfo, pBufferSize, &SourceRect);
}

// An application has two ways of using GetCurrentImage, one is to pass a real
// buffer which should be filled with the current image. The other is to pass
// a NULL buffer pointer which is interpreted as asking us to return how much
// memory is required for the image. The constraints for when the latter can
// be called are much looser. To calculate the memory required we synthesize
// a VIDEOINFO that takes into account the source rectangle that's being used

HRESULT CBaseControlVideo::GetImageSize(__in VIDEOINFOHEADER *pVideoInfo, __out long *pBufferSize,
                                        __in RECT *pSourceRect)
{
    NOTE("Entering GetImageSize");
    ASSERT(pSourceRect);

    // Check we have the correct input parameters

    if (pSourceRect == NULL || pVideoInfo == NULL || pBufferSize == NULL)
    {

        return E_UNEXPECTED;
    }

    // Is the data format compatible

    if (pVideoInfo->bmiHeader.biCompression != BI_RGB)
    {
        if (pVideoInfo->bmiHeader.biCompression != BI_BITFIELDS)
        {
            return E_INVALIDARG;
        }
    }

    ASSERT(IsRectEmpty(pSourceRect) == FALSE);

    BITMAPINFOHEADER bih;
    bih.biWidth = WIDTH(pSourceRect);
    bih.biHeight = HEIGHT(pSourceRect);
    bih.biBitCount = pVideoInfo->bmiHeader.biBitCount;
    LONG Size = DIBSIZE(bih);
    Size += GetBitmapFormatSize(HEADER(pVideoInfo)) - SIZE_PREHEADER;
    *pBufferSize = Size;

    return NOERROR;
}

// Given an IMediaSample containing a linear buffer with an image and a type
// describing the bitmap make a rendering of the image into the output buffer
// This may be called by derived classes who render typical video images to
// handle the IBasicVideo GetCurrentImage method. The pVideoImage pointer may
// be NULL when passed to GetCurrentImage in which case GetImageSize will be
// called instead, which will just do the calculation of the memory required

HRESULT CBaseControlVideo::CopyImage(IMediaSample *pMediaSample, __in VIDEOINFOHEADER *pVideoInfo,
                                     __inout long *pBufferSize,
                                     __out_bcount_part(*pBufferSize, *pBufferSize) BYTE *pVideoImage,
                                     __in RECT *pSourceRect)
{
    NOTE("Entering CopyImage");
    ASSERT(pSourceRect);
    BYTE *pCurrentImage;

    // Check we have an image to copy

    if (pMediaSample == NULL || pSourceRect == NULL || pVideoInfo == NULL || pVideoImage == NULL || pBufferSize == NULL)
    {

        return E_UNEXPECTED;
    }

    // Is the data format compatible

    if (pVideoInfo->bmiHeader.biCompression != BI_RGB)
    {
        if (pVideoInfo->bmiHeader.biCompression != BI_BITFIELDS)
        {
            return E_INVALIDARG;
        }
    }

    if (*pBufferSize < 0)
    {
        return E_INVALIDARG;
    }

    // Arbitrarily large size to prevent integer overflow problems
    if (pVideoInfo->bmiHeader.biSize > 4096)
    {
        return E_INVALIDARG;
    }

    ASSERT(IsRectEmpty(pSourceRect) == FALSE);

    BITMAPINFOHEADER bih;
    bih.biWidth = WIDTH(pSourceRect);
    bih.biHeight = HEIGHT(pSourceRect);
    bih.biBitCount = pVideoInfo->bmiHeader.biBitCount;
    DWORD Size = GetBitmapFormatSize(HEADER(pVideoInfo)) - SIZE_PREHEADER;
    DWORD Total;
    DWORD dwDibSize;

    if (!ValidateBitmapInfoHeader(HEADER(pVideoInfo), Size))
    {
        return E_INVALIDARG;
    }

    //  ValidateBitmapInfoHeader checks this but for some reason code scanning
    //  tools aren't picking up the annotation
    __analysis_assume(Size >= sizeof(BITMAPINFOHEADER));

    if (FAILED(SAFE_DIBSIZE(&bih, &dwDibSize)))
    {
        return E_INVALIDARG;
    }

    if (FAILED(DWordAdd(Size, dwDibSize, &Total)))
    {
        return E_INVALIDARG;
    }

    // Make sure we have a large enough buffer

    if ((DWORD)*pBufferSize < Total)
    {
        return E_OUTOFMEMORY;
    }

    // Copy the BITMAPINFO

    CopyMemory((PVOID)pVideoImage, (PVOID)&pVideoInfo->bmiHeader, Size);
    ((BITMAPINFOHEADER *)pVideoImage)->biWidth = WIDTH(pSourceRect);
    ((BITMAPINFOHEADER *)pVideoImage)->biHeight = HEIGHT(pSourceRect);
    ((BITMAPINFOHEADER *)pVideoImage)->biSizeImage = DIBSIZE(bih);
    BYTE *pImageData = pVideoImage + Size;

    // Get the pointer to it's image data

    HRESULT hr = pMediaSample->GetPointer(&pCurrentImage);
    if (FAILED(hr))
    {
        return hr;
    }

    // Now we are ready to start copying the source scan lines

    LONG ScanLine = (pVideoInfo->bmiHeader.biBitCount / 8) * WIDTH(pSourceRect);
    LONG LinesToSkip = pVideoInfo->bmiHeader.biHeight;
    LinesToSkip -= pSourceRect->top + HEIGHT(pSourceRect);
    pCurrentImage += LinesToSkip * DIBWIDTHBYTES(pVideoInfo->bmiHeader);
    pCurrentImage += pSourceRect->left * (pVideoInfo->bmiHeader.biBitCount / 8);

    // Even money on this GP faulting sometime...

    for (LONG Line = 0; Line < HEIGHT(pSourceRect); Line++)
    {
        CopyMemory((PVOID)pImageData, (PVOID)pCurrentImage, ScanLine);
        pImageData += DIBWIDTHBYTES(*(BITMAPINFOHEADER *)pVideoImage);
        pCurrentImage += DIBWIDTHBYTES(pVideoInfo->bmiHeader);
    }
    return NOERROR;
}

// Called when we change media types either during connection or dynamically
// We inform the filter graph and therefore the application that the video
// size may have changed, we don't bother looking to see if it really has as
// we leave that to the application - the dimensions are the event parameters

HRESULT CBaseControlVideo::OnVideoSizeChange()
{
    // Get the video format from the derived class

    VIDEOINFOHEADER *pVideoInfo = GetVideoFormat();
    if (pVideoInfo == NULL)
        return E_OUTOFMEMORY;
    WORD Width = (WORD)pVideoInfo->bmiHeader.biWidth;
    WORD Height = (WORD)pVideoInfo->bmiHeader.biHeight;

    return m_pFilter->NotifyEvent(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(Width, Height), MAKEWPARAM(0, 0));
}

// Set the video source rectangle. We must check the source rectangle against
// the actual video dimensions otherwise when we come to draw the pictures we
// get access violations as GDI tries to touch data outside of the image data
// Although we store the rectangle in left, top, right and bottom coordinates
// instead of left, top, width and height as OLE uses we do take into account
// that the rectangle is used up to, but not including, the right column and
// bottom row of pixels, see the Win32 documentation on RECT for more details

HRESULT CBaseControlVideo::CheckSourceRect(__in RECT *pSourceRect)
{
    CheckPointer(pSourceRect, E_POINTER);
    LONG Width, Height;
    GetVideoSize(&Width, &Height);

    // Check the coordinates are greater than zero
    // and that the rectangle is valid (left<right, top<bottom)

    if ((pSourceRect->left >= pSourceRect->right) || (pSourceRect->left < 0) ||
        (pSourceRect->top >= pSourceRect->bottom) || (pSourceRect->top < 0))
    {

        return E_INVALIDARG;
    }

    // Check the coordinates are less than the extents

    if ((pSourceRect->right > Width) || (pSourceRect->bottom > Height))
    {

        return E_INVALIDARG;
    }
    return NOERROR;
}

// Check the target rectangle has some valid coordinates, which amounts to
// little more than checking the destination rectangle isn't empty. Derived
// classes may call this when they have their SetTargetRect method called to
// check the rectangle validity, we do not update the rectangles passed in
// Although we store the rectangle in left, top, right and bottom coordinates
// instead of left, top, width and height as OLE uses we do take into account
// that the rectangle is used up to, but not including, the right column and
// bottom row of pixels, see the Win32 documentation on RECT for more details

HRESULT CBaseControlVideo::CheckTargetRect(__in RECT *pTargetRect)
{
    // Check the pointer is valid

    if (pTargetRect == NULL)
    {
        return E_POINTER;
    }

    // These overflow the WIDTH and HEIGHT checks

    if (pTargetRect->left > pTargetRect->right || pTargetRect->top > pTargetRect->bottom)
    {
        return E_INVALIDARG;
    }

    // Check the rectangle has valid coordinates

    if (WIDTH(pTargetRect) <= 0 || HEIGHT(pTargetRect) <= 0)
    {
        return E_INVALIDARG;
    }

    ASSERT(IsRectEmpty(pTargetRect) == FALSE);
    return NOERROR;
}
