//------------------------------------------------------------------------------
// File: WinCtrl.h
//
// Desc: DirectShow base classes - defines classes for video control
//       interfaces.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __WINCTRL__
#define __WINCTRL__

#define ABSOL(x) (x < 0 ? -x : x)
#define NEGAT(x) (x > 0 ? -x : x)

//  Helper
BOOL WINAPI PossiblyEatMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

class CBaseControlWindow
    : public CBaseVideoWindow
    , public CBaseWindow
{
  protected:
    CBaseFilter *m_pFilter;     // Pointer to owning media filter
    CBasePin *m_pPin;           // Controls media types for connection
    CCritSec *m_pInterfaceLock; // Externally defined critical section
    COLORREF m_BorderColour;    // Current window border colour
    BOOL m_bAutoShow;           // What happens when the state changes
    HWND m_hwndOwner;           // Owner window that we optionally have
    HWND m_hwndDrain;           // HWND to post any messages received
    BOOL m_bCursorHidden;       // Should we hide the window cursor

  public:
    // Internal methods for other objects to get information out

    HRESULT DoSetWindowStyle(long Style, long WindowLong);
    HRESULT DoGetWindowStyle(__out long *pStyle, long WindowLong);
    BOOL IsAutoShowEnabled() { return m_bAutoShow; };
    COLORREF GetBorderColour() { return m_BorderColour; };
    HWND GetOwnerWindow() { return m_hwndOwner; };
    BOOL IsCursorHidden() { return m_bCursorHidden; };

    inline BOOL PossiblyEatMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        return ::PossiblyEatMessage(m_hwndDrain, uMsg, wParam, lParam);
    }

    // Derived classes must call this to set the pin the filter is using
    // We don't have the pin passed in to the constructor (as we do with
    // the CBaseFilter object) because filters typically create the
    // pins dynamically when requested in CBaseFilter::GetPin. This can
    // not be called from our constructor because is is a virtual method

    void SetControlWindowPin(CBasePin *pPin) { m_pPin = pPin; }

  public:
    CBaseControlWindow(__inout CBaseFilter *pFilter,  // Owning media filter
                       __in CCritSec *pInterfaceLock, // Locking object
                       __in_opt LPCTSTR pName,        // Object description
                       __inout_opt LPUNKNOWN pUnk,    // Normal COM ownership
                       __inout HRESULT *phr);         // OLE return code

    // These are the properties we support

    STDMETHODIMP put_Caption(__in BSTR strCaption);
    STDMETHODIMP get_Caption(__out BSTR *pstrCaption);
    STDMETHODIMP put_AutoShow(long AutoShow);
    STDMETHODIMP get_AutoShow(__out long *AutoShow);
    STDMETHODIMP put_WindowStyle(long WindowStyle);
    STDMETHODIMP get_WindowStyle(__out long *pWindowStyle);
    STDMETHODIMP put_WindowStyleEx(long WindowStyleEx);
    STDMETHODIMP get_WindowStyleEx(__out long *pWindowStyleEx);
    STDMETHODIMP put_WindowState(long WindowState);
    STDMETHODIMP get_WindowState(__out long *pWindowState);
    STDMETHODIMP put_BackgroundPalette(long BackgroundPalette);
    STDMETHODIMP get_BackgroundPalette(__out long *pBackgroundPalette);
    STDMETHODIMP put_Visible(long Visible);
    STDMETHODIMP get_Visible(__out long *pVisible);
    STDMETHODIMP put_Left(long Left);
    STDMETHODIMP get_Left(__out long *pLeft);
    STDMETHODIMP put_Width(long Width);
    STDMETHODIMP get_Width(__out long *pWidth);
    STDMETHODIMP put_Top(long Top);
    STDMETHODIMP get_Top(__out long *pTop);
    STDMETHODIMP put_Height(long Height);
    STDMETHODIMP get_Height(__out long *pHeight);
    STDMETHODIMP put_Owner(OAHWND Owner);
    STDMETHODIMP get_Owner(__out OAHWND *Owner);
    STDMETHODIMP put_MessageDrain(OAHWND Drain);
    STDMETHODIMP get_MessageDrain(__out OAHWND *Drain);
    STDMETHODIMP get_BorderColor(__out long *Color);
    STDMETHODIMP put_BorderColor(long Color);
    STDMETHODIMP get_FullScreenMode(__out long *FullScreenMode);
    STDMETHODIMP put_FullScreenMode(long FullScreenMode);

    // And these are the methods

    STDMETHODIMP SetWindowForeground(long Focus);
    STDMETHODIMP NotifyOwnerMessage(OAHWND hwnd, long uMsg, LONG_PTR wParam, LONG_PTR lParam);
    STDMETHODIMP GetMinIdealImageSize(__out long *pWidth, __out long *pHeight);
    STDMETHODIMP GetMaxIdealImageSize(__out long *pWidth, __out long *pHeight);
    STDMETHODIMP SetWindowPosition(long Left, long Top, long Width, long Height);
    STDMETHODIMP GetWindowPosition(__out long *pLeft, __out long *pTop, __out long *pWidth, __out long *pHeight);
    STDMETHODIMP GetRestorePosition(__out long *pLeft, __out long *pTop, __out long *pWidth, __out long *pHeight);
    STDMETHODIMP HideCursor(long HideCursor);
    STDMETHODIMP IsCursorHidden(__out long *CursorHidden);
};

// This class implements the IBasicVideo interface

class CBaseControlVideo : public CBaseBasicVideo
{
  protected:
    CBaseFilter *m_pFilter;     // Pointer to owning media filter
    CBasePin *m_pPin;           // Controls media types for connection
    CCritSec *m_pInterfaceLock; // Externally defined critical section

  public:
    // Derived classes must provide these for the implementation

    virtual HRESULT IsDefaultTargetRect() PURE;
    virtual HRESULT SetDefaultTargetRect() PURE;
    virtual HRESULT SetTargetRect(RECT *pTargetRect) PURE;
    virtual HRESULT GetTargetRect(RECT *pTargetRect) PURE;
    virtual HRESULT IsDefaultSourceRect() PURE;
    virtual HRESULT SetDefaultSourceRect() PURE;
    virtual HRESULT SetSourceRect(RECT *pSourceRect) PURE;
    virtual HRESULT GetSourceRect(RECT *pSourceRect) PURE;
    virtual HRESULT GetStaticImage(__inout long *pBufferSize,
                                   __out_bcount_part(*pBufferSize, *pBufferSize) long *pDIBImage) PURE;

    // Derived classes must override this to return a VIDEOINFO representing
    // the video format. We cannot call IPin ConnectionMediaType to get this
    // format because various filters dynamically change the type when using
    // DirectDraw such that the format shows the position of the logical
    // bitmap in a frame buffer surface, so the size might be returned as
    // 1024x768 pixels instead of 320x240 which is the real video dimensions

    __out virtual VIDEOINFOHEADER *GetVideoFormat() PURE;

    // Helper functions for creating memory renderings of a DIB image

    HRESULT GetImageSize(__in VIDEOINFOHEADER *pVideoInfo, __out LONG *pBufferSize, __in RECT *pSourceRect);

    HRESULT CopyImage(IMediaSample *pMediaSample, __in VIDEOINFOHEADER *pVideoInfo, __inout LONG *pBufferSize,
                      __out_bcount_part(*pBufferSize, *pBufferSize) BYTE *pVideoImage, __in RECT *pSourceRect);

    // Override this if you want notifying when the rectangles change
    virtual HRESULT OnUpdateRectangles() { return NOERROR; };
    virtual HRESULT OnVideoSizeChange();

    // Derived classes must call this to set the pin the filter is using
    // We don't have the pin passed in to the constructor (as we do with
    // the CBaseFilter object) because filters typically create the
    // pins dynamically when requested in CBaseFilter::GetPin. This can
    // not be called from our constructor because is is a virtual method

    void SetControlVideoPin(__inout CBasePin *pPin) { m_pPin = pPin; }

    // Helper methods for checking rectangles
    virtual HRESULT CheckSourceRect(__in RECT *pSourceRect);
    virtual HRESULT CheckTargetRect(__in RECT *pTargetRect);

  public:
    CBaseControlVideo(__inout CBaseFilter *pFilter,  // Owning media filter
                      __in CCritSec *pInterfaceLock, // Serialise interface
                      __in_opt LPCTSTR pName,        // Object description
                      __inout_opt LPUNKNOWN pUnk,    // Normal COM ownership
                      __inout HRESULT *phr);         // OLE return code

    // These are the properties we support

    STDMETHODIMP get_AvgTimePerFrame(__out REFTIME *pAvgTimePerFrame);
    STDMETHODIMP get_BitRate(__out long *pBitRate);
    STDMETHODIMP get_BitErrorRate(__out long *pBitErrorRate);
    STDMETHODIMP get_VideoWidth(__out long *pVideoWidth);
    STDMETHODIMP get_VideoHeight(__out long *pVideoHeight);
    STDMETHODIMP put_SourceLeft(long SourceLeft);
    STDMETHODIMP get_SourceLeft(__out long *pSourceLeft);
    STDMETHODIMP put_SourceWidth(long SourceWidth);
    STDMETHODIMP get_SourceWidth(__out long *pSourceWidth);
    STDMETHODIMP put_SourceTop(long SourceTop);
    STDMETHODIMP get_SourceTop(__out long *pSourceTop);
    STDMETHODIMP put_SourceHeight(long SourceHeight);
    STDMETHODIMP get_SourceHeight(__out long *pSourceHeight);
    STDMETHODIMP put_DestinationLeft(long DestinationLeft);
    STDMETHODIMP get_DestinationLeft(__out long *pDestinationLeft);
    STDMETHODIMP put_DestinationWidth(long DestinationWidth);
    STDMETHODIMP get_DestinationWidth(__out long *pDestinationWidth);
    STDMETHODIMP put_DestinationTop(long DestinationTop);
    STDMETHODIMP get_DestinationTop(__out long *pDestinationTop);
    STDMETHODIMP put_DestinationHeight(long DestinationHeight);
    STDMETHODIMP get_DestinationHeight(__out long *pDestinationHeight);

    // And these are the methods

    STDMETHODIMP GetVideoSize(__out long *pWidth, __out long *pHeight);
    STDMETHODIMP SetSourcePosition(long Left, long Top, long Width, long Height);
    STDMETHODIMP GetSourcePosition(__out long *pLeft, __out long *pTop, __out long *pWidth, __out long *pHeight);
    STDMETHODIMP GetVideoPaletteEntries(long StartIndex, long Entries, __out long *pRetrieved,
                                        __out_ecount_part(Entries, *pRetrieved) long *pPalette);
    STDMETHODIMP SetDefaultSourcePosition();
    STDMETHODIMP IsUsingDefaultSource();
    STDMETHODIMP SetDestinationPosition(long Left, long Top, long Width, long Height);
    STDMETHODIMP GetDestinationPosition(__out long *pLeft, __out long *pTop, __out long *pWidth, __out long *pHeight);
    STDMETHODIMP SetDefaultDestinationPosition();
    STDMETHODIMP IsUsingDefaultDestination();
    STDMETHODIMP GetCurrentImage(__inout long *pBufferSize,
                                 __out_bcount_part(*pBufferSize, *pBufferSize) long *pVideoImage);
};

#endif // __WINCTRL__
