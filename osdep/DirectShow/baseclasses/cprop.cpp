//------------------------------------------------------------------------------
// File: CProp.cpp
//
// Desc: DirectShow base classes - implements CBasePropertyPage class.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>

// Constructor for the base property page class. As described in the header
// file we must be initialised with dialog and title resource identifiers.
// The class supports IPropertyPage and overrides AddRef and Release calls
// to keep track of the reference counts. When the last count is released
// we call SetPageSite(NULL) and SetObjects(0,NULL) to release interfaces
// previously obtained by the property page when it had SetObjects called

CBasePropertyPage::CBasePropertyPage(__in_opt LPCTSTR pName,     // Debug only name
                                     __inout_opt LPUNKNOWN pUnk, // COM Delegator
                                     int DialogId,               // Resource ID
                                     int TitleId)
    : // To get tital
    CUnknown(pName, pUnk)
    , m_DialogId(DialogId)
    , m_TitleId(TitleId)
    , m_hwnd(NULL)
    , m_Dlg(NULL)
    , m_pPageSite(NULL)
    , m_bObjectSet(FALSE)
    , m_bDirty(FALSE)
{
}

#ifdef UNICODE
CBasePropertyPage::CBasePropertyPage(__in_opt LPCSTR pName,      // Debug only name
                                     __inout_opt LPUNKNOWN pUnk, // COM Delegator
                                     int DialogId,               // Resource ID
                                     int TitleId)
    : // To get tital
    CUnknown(pName, pUnk)
    , m_DialogId(DialogId)
    , m_TitleId(TitleId)
    , m_hwnd(NULL)
    , m_Dlg(NULL)
    , m_pPageSite(NULL)
    , m_bObjectSet(FALSE)
    , m_bDirty(FALSE)
{
}
#endif

// Increment our reference count

STDMETHODIMP_(ULONG) CBasePropertyPage::NonDelegatingAddRef()
{
    LONG lRef = InterlockedIncrement(&m_cRef);
    ASSERT(lRef > 0);
    return max(ULONG(m_cRef), 1ul);
}

// Release a reference count and protect against reentrancy

STDMETHODIMP_(ULONG) CBasePropertyPage::NonDelegatingRelease()
{
    // If the reference count drops to zero delete ourselves

    LONG lRef = InterlockedDecrement(&m_cRef);
    if (lRef == 0)
    {
        m_cRef++;
        SetPageSite(NULL);
        SetObjects(0, NULL);
        delete this;
        return ULONG(0);
    }
    else
    {
        //  Don't touch m_cRef again here!
        return max(ULONG(lRef), 1ul);
    }
}

// Expose our IPropertyPage interface

STDMETHODIMP
CBasePropertyPage::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    if (riid == IID_IPropertyPage)
    {
        return GetInterface((IPropertyPage *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

// Get the page info so that the page site can size itself

STDMETHODIMP CBasePropertyPage::GetPageInfo(__out LPPROPPAGEINFO pPageInfo)
{
    CheckPointer(pPageInfo, E_POINTER);
    WCHAR wszTitle[STR_MAX_LENGTH];
    WideStringFromResource(wszTitle, m_TitleId);

    // Allocate dynamic memory for the property page title

    LPOLESTR pszTitle;
    HRESULT hr = AMGetWideString(wszTitle, &pszTitle);
    if (FAILED(hr))
    {
        NOTE("No caption memory");
        return hr;
    }

    pPageInfo->cb = sizeof(PROPPAGEINFO);
    pPageInfo->pszTitle = pszTitle;
    pPageInfo->pszDocString = NULL;
    pPageInfo->pszHelpFile = NULL;
    pPageInfo->dwHelpContext = 0;

    // Set defaults in case GetDialogSize fails
    pPageInfo->size.cx = 340;
    pPageInfo->size.cy = 150;

    GetDialogSize(m_DialogId, DialogProc, 0L, &pPageInfo->size);
    return NOERROR;
}

// Handles the messages for our property window

INT_PTR CALLBACK CBasePropertyPage::DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CBasePropertyPage *pPropertyPage;

    switch (uMsg)
    {

    case WM_INITDIALOG:

        _SetWindowLongPtr(hwnd, DWLP_USER, lParam);

        // This pointer may be NULL when calculating size

        pPropertyPage = (CBasePropertyPage *)lParam;
        if (pPropertyPage == NULL)
        {
            return (LRESULT)1;
        }
        pPropertyPage->m_Dlg = hwnd;
    }

    // This pointer may be NULL when calculating size

    pPropertyPage = _GetWindowLongPtr<CBasePropertyPage *>(hwnd, DWLP_USER);
    if (pPropertyPage == NULL)
    {
        return (LRESULT)1;
    }
    return pPropertyPage->OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

// Tells us the object that should be informed of the property changes

STDMETHODIMP CBasePropertyPage::SetObjects(ULONG cObjects, __in_ecount_opt(cObjects) LPUNKNOWN *ppUnk)
{
    if (cObjects == 1)
    {

        if ((ppUnk == NULL) || (*ppUnk == NULL))
        {
            return E_POINTER;
        }

        // Set a flag to say that we have set the Object
        m_bObjectSet = TRUE;
        return OnConnect(*ppUnk);
    }
    else if (cObjects == 0)
    {

        // Set a flag to say that we have not set the Object for the page
        m_bObjectSet = FALSE;
        return OnDisconnect();
    }

    DbgBreak("No support for more than one object");
    return E_UNEXPECTED;
}

// Create the window we will use to edit properties

STDMETHODIMP CBasePropertyPage::Activate(HWND hwndParent, LPCRECT pRect, BOOL fModal)
{
    CheckPointer(pRect, E_POINTER);

    // Return failure if SetObject has not been called.
    if (m_bObjectSet == FALSE)
    {
        return E_UNEXPECTED;
    }

    if (m_hwnd)
    {
        return E_UNEXPECTED;
    }

    m_hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(m_DialogId), hwndParent, DialogProc, (LPARAM)this);
    if (m_hwnd == NULL)
    {
        return E_OUTOFMEMORY;
    }

    OnActivate();
    Move(pRect);
    return Show(SW_SHOWNORMAL);
}

// Set the position of the property page

STDMETHODIMP CBasePropertyPage::Move(LPCRECT pRect)
{
    CheckPointer(pRect, E_POINTER);

    if (m_hwnd == NULL)
    {
        return E_UNEXPECTED;
    }

    MoveWindow(m_hwnd,        // Property page handle
               pRect->left,   // x coordinate
               pRect->top,    // y coordinate
               WIDTH(pRect),  // Overall window width
               HEIGHT(pRect), // And likewise height
               TRUE);         // Should we repaint it

    return NOERROR;
}

// Display the property dialog

STDMETHODIMP CBasePropertyPage::Show(UINT nCmdShow)
{
    // Have we been activated yet

    if (m_hwnd == NULL)
    {
        return E_UNEXPECTED;
    }

    // Ignore wrong show flags

    if ((nCmdShow != SW_SHOW) && (nCmdShow != SW_SHOWNORMAL) && (nCmdShow != SW_HIDE))
    {
        return E_INVALIDARG;
    }

    ShowWindow(m_hwnd, nCmdShow);
    InvalidateRect(m_hwnd, NULL, TRUE);
    return NOERROR;
}

// Destroy the property page dialog

STDMETHODIMP CBasePropertyPage::Deactivate(void)
{
    if (m_hwnd == NULL)
    {
        return E_UNEXPECTED;
    }

    // Remove WS_EX_CONTROLPARENT before DestroyWindow call

    DWORD dwStyle = GetWindowLong(m_hwnd, GWL_EXSTYLE);
    dwStyle = dwStyle & (~WS_EX_CONTROLPARENT);

    //  Set m_hwnd to be NULL temporarily so the message handler
    //  for WM_STYLECHANGING doesn't add the WS_EX_CONTROLPARENT
    //  style back in
    HWND hwnd = m_hwnd;
    m_hwnd = NULL;
    SetWindowLong(hwnd, GWL_EXSTYLE, dwStyle);
    m_hwnd = hwnd;

    OnDeactivate();

    // Destroy the dialog window

    DestroyWindow(m_hwnd);
    m_hwnd = NULL;
    return NOERROR;
}

// Tells the application property page site

STDMETHODIMP CBasePropertyPage::SetPageSite(__in_opt LPPROPERTYPAGESITE pPageSite)
{
    if (pPageSite)
    {

        if (m_pPageSite)
        {
            return E_UNEXPECTED;
        }

        m_pPageSite = pPageSite;
        m_pPageSite->AddRef();
    }
    else
    {

        if (m_pPageSite == NULL)
        {
            return E_UNEXPECTED;
        }

        m_pPageSite->Release();
        m_pPageSite = NULL;
    }
    return NOERROR;
}

// Apply any changes so far made

STDMETHODIMP CBasePropertyPage::Apply()
{
    // In ActiveMovie 1.0 we used to check whether we had been activated or
    // not. This is too constrictive. Apply should be allowed as long as
    // SetObject was called to set an object. So we will no longer check to
    // see if we have been activated (ie., m_hWnd != NULL), but instead
    // make sure that m_bObjectSet is TRUE (ie., SetObject has been called).

    if (m_bObjectSet == FALSE)
    {
        return E_UNEXPECTED;
    }

    // Must have had a site set

    if (m_pPageSite == NULL)
    {
        return E_UNEXPECTED;
    }

    // Has anything changed

    if (m_bDirty == FALSE)
    {
        return NOERROR;
    }

    // Commit derived class changes

    HRESULT hr = OnApplyChanges();
    if (SUCCEEDED(hr))
    {
        m_bDirty = FALSE;
    }
    return hr;
}

// Base class definition for message handling

INT_PTR CBasePropertyPage::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // we would like the TAB key to move around the tab stops in our property
    // page, but for some reason OleCreatePropertyFrame clears the CONTROLPARENT
    // style behind our back, so we need to switch it back on now behind its
    // back.  Otherwise the tab key will be useless in every page.
    //

    CBasePropertyPage *pPropertyPage;
    {
        pPropertyPage = _GetWindowLongPtr<CBasePropertyPage *>(hwnd, DWLP_USER);

        if (pPropertyPage->m_hwnd == NULL)
        {
            return 0;
        }
        switch (uMsg)
        {
        case WM_STYLECHANGING:
            if (wParam == GWL_EXSTYLE)
            {
                LPSTYLESTRUCT lpss = (LPSTYLESTRUCT)lParam;
                lpss->styleNew |= WS_EX_CONTROLPARENT;
                return 0;
            }
        }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
