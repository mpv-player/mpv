//------------------------------------------------------------------------------
// File: VideoCtl.h
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __VIDEOCTL__
#define __VIDEOCTL__

// These help with property page implementations. The first can be used to
// load any string from a resource file. The buffer to load into is passed
// as an input parameter. The same buffer is the return value if the string
// was found otherwise it returns TEXT(""). The GetDialogSize is passed the
// resource ID of a dialog box and returns the size of it in screen pixels

#define STR_MAX_LENGTH 256
LPTSTR WINAPI StringFromResource(__out_ecount(STR_MAX_LENGTH) LPTSTR pBuffer, int iResourceID);

#ifdef UNICODE
#define WideStringFromResource StringFromResource
LPSTR WINAPI StringFromResource(__out_ecount(STR_MAX_LENGTH) LPSTR pBuffer, int iResourceID);
#else
LPWSTR WINAPI WideStringFromResource(__out_ecount(STR_MAX_LENGTH) LPWSTR pBuffer, int iResourceID);
#endif

BOOL WINAPI GetDialogSize(int iResourceID,      // Dialog box resource identifier
                          DLGPROC pDlgProc,     // Pointer to dialog procedure
                          LPARAM lParam,        // Any user data wanted in pDlgProc
                          __out SIZE *pResult); // Returns the size of dialog box

// Class that aggregates an IDirectDraw interface

class CAggDirectDraw
    : public IDirectDraw
    , public CUnknown
{
  protected:
    LPDIRECTDRAW m_pDirectDraw;

  public:
    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);

    // Constructor and destructor

    CAggDirectDraw(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk)
        : CUnknown(pName, pUnk)
        , m_pDirectDraw(NULL){};

    virtual CAggDirectDraw::~CAggDirectDraw(){};

    // Set the object we should be aggregating
    void SetDirectDraw(__inout LPDIRECTDRAW pDirectDraw) { m_pDirectDraw = pDirectDraw; }

    // IDirectDraw methods

    STDMETHODIMP Compact();
    STDMETHODIMP CreateClipper(DWORD dwFlags, __deref_out LPDIRECTDRAWCLIPPER *lplpDDClipper,
                               __inout_opt IUnknown *pUnkOuter);
    STDMETHODIMP CreatePalette(DWORD dwFlags, __in LPPALETTEENTRY lpColorTable,
                               __deref_out LPDIRECTDRAWPALETTE *lplpDDPalette, __inout_opt IUnknown *pUnkOuter);
    STDMETHODIMP CreateSurface(__in LPDDSURFACEDESC lpDDSurfaceDesc, __deref_out LPDIRECTDRAWSURFACE *lplpDDSurface,
                               __inout_opt IUnknown *pUnkOuter);
    STDMETHODIMP DuplicateSurface(__in LPDIRECTDRAWSURFACE lpDDSurface,
                                  __deref_out LPDIRECTDRAWSURFACE *lplpDupDDSurface);
    STDMETHODIMP EnumDisplayModes(DWORD dwSurfaceDescCount, __in LPDDSURFACEDESC lplpDDSurfaceDescList,
                                  __in LPVOID lpContext, __in LPDDENUMMODESCALLBACK lpEnumCallback);
    STDMETHODIMP EnumSurfaces(DWORD dwFlags, __in LPDDSURFACEDESC lpDDSD, __in LPVOID lpContext,
                              __in LPDDENUMSURFACESCALLBACK lpEnumCallback);
    STDMETHODIMP FlipToGDISurface();
    STDMETHODIMP GetCaps(__out LPDDCAPS lpDDDriverCaps, __out LPDDCAPS lpDDHELCaps);
    STDMETHODIMP GetDisplayMode(__out LPDDSURFACEDESC lpDDSurfaceDesc);
    STDMETHODIMP GetFourCCCodes(__inout LPDWORD lpNumCodes, __out_ecount(*lpNumCodes) LPDWORD lpCodes);
    STDMETHODIMP GetGDISurface(__deref_out LPDIRECTDRAWSURFACE *lplpGDIDDSurface);
    STDMETHODIMP GetMonitorFrequency(__out LPDWORD lpdwFrequency);
    STDMETHODIMP GetScanLine(__out LPDWORD lpdwScanLine);
    STDMETHODIMP GetVerticalBlankStatus(__out LPBOOL lpblsInVB);
    STDMETHODIMP Initialize(__in GUID *lpGUID);
    STDMETHODIMP RestoreDisplayMode();
    STDMETHODIMP SetCooperativeLevel(HWND hWnd, DWORD dwFlags);
    STDMETHODIMP SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBpp);
    STDMETHODIMP WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent);
};

// Class that aggregates an IDirectDrawSurface interface

class CAggDrawSurface
    : public IDirectDrawSurface
    , public CUnknown
{
  protected:
    LPDIRECTDRAWSURFACE m_pDirectDrawSurface;

  public:
    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);

    // Constructor and destructor

    CAggDrawSurface(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk)
        : CUnknown(pName, pUnk)
        , m_pDirectDrawSurface(NULL){};

    virtual ~CAggDrawSurface(){};

    // Set the object we should be aggregating
    void SetDirectDrawSurface(__inout LPDIRECTDRAWSURFACE pDirectDrawSurface)
    {
        m_pDirectDrawSurface = pDirectDrawSurface;
    }

    // IDirectDrawSurface methods

    STDMETHODIMP AddAttachedSurface(__in LPDIRECTDRAWSURFACE lpDDSAttachedSurface);
    STDMETHODIMP AddOverlayDirtyRect(__in LPRECT lpRect);
    STDMETHODIMP Blt(__in LPRECT lpDestRect, __in LPDIRECTDRAWSURFACE lpDDSrcSurface, __in LPRECT lpSrcRect,
                     DWORD dwFlags, __in LPDDBLTFX lpDDBltFx);
    STDMETHODIMP BltBatch(__in_ecount(dwCount) LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags);
    STDMETHODIMP BltFast(DWORD dwX, DWORD dwY, __in LPDIRECTDRAWSURFACE lpDDSrcSurface, __in LPRECT lpSrcRect,
                         DWORD dwTrans);
    STDMETHODIMP DeleteAttachedSurface(DWORD dwFlags, __in LPDIRECTDRAWSURFACE lpDDSAttachedSurface);
    STDMETHODIMP EnumAttachedSurfaces(__in LPVOID lpContext, __in LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback);
    STDMETHODIMP EnumOverlayZOrders(DWORD dwFlags, __in LPVOID lpContext, __in LPDDENUMSURFACESCALLBACK lpfnCallback);
    STDMETHODIMP Flip(__in LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags);
    STDMETHODIMP GetAttachedSurface(__in LPDDSCAPS lpDDSCaps, __deref_out LPDIRECTDRAWSURFACE *lplpDDAttachedSurface);
    STDMETHODIMP GetBltStatus(DWORD dwFlags);
    STDMETHODIMP GetCaps(__out LPDDSCAPS lpDDSCaps);
    STDMETHODIMP GetClipper(__deref_out LPDIRECTDRAWCLIPPER *lplpDDClipper);
    STDMETHODIMP GetColorKey(DWORD dwFlags, __out LPDDCOLORKEY lpDDColorKey);
    STDMETHODIMP GetDC(__out HDC *lphDC);
    STDMETHODIMP GetFlipStatus(DWORD dwFlags);
    STDMETHODIMP GetOverlayPosition(__out LPLONG lpdwX, __out LPLONG lpdwY);
    STDMETHODIMP GetPalette(__deref_out LPDIRECTDRAWPALETTE *lplpDDPalette);
    STDMETHODIMP GetPixelFormat(__out LPDDPIXELFORMAT lpDDPixelFormat);
    STDMETHODIMP GetSurfaceDesc(__out LPDDSURFACEDESC lpDDSurfaceDesc);
    STDMETHODIMP Initialize(__in LPDIRECTDRAW lpDD, __in LPDDSURFACEDESC lpDDSurfaceDesc);
    STDMETHODIMP IsLost();
    STDMETHODIMP Lock(__in LPRECT lpDestRect, __inout LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent);
    STDMETHODIMP ReleaseDC(HDC hDC);
    STDMETHODIMP Restore();
    STDMETHODIMP SetClipper(__in LPDIRECTDRAWCLIPPER lpDDClipper);
    STDMETHODIMP SetColorKey(DWORD dwFlags, __in LPDDCOLORKEY lpDDColorKey);
    STDMETHODIMP SetOverlayPosition(LONG dwX, LONG dwY);
    STDMETHODIMP SetPalette(__in LPDIRECTDRAWPALETTE lpDDPalette);
    STDMETHODIMP Unlock(__in LPVOID lpSurfaceData);
    STDMETHODIMP UpdateOverlay(__in LPRECT lpSrcRect, __in LPDIRECTDRAWSURFACE lpDDDestSurface, __in LPRECT lpDestRect,
                               DWORD dwFlags, __in LPDDOVERLAYFX lpDDOverlayFX);
    STDMETHODIMP UpdateOverlayDisplay(DWORD dwFlags);
    STDMETHODIMP UpdateOverlayZOrder(DWORD dwFlags, __in LPDIRECTDRAWSURFACE lpDDSReference);
};

class CLoadDirectDraw
{
    LPDIRECTDRAW m_pDirectDraw; // The DirectDraw driver instance
    HINSTANCE m_hDirectDraw;    // Handle to the loaded library

  public:
    CLoadDirectDraw();
    ~CLoadDirectDraw();

    HRESULT LoadDirectDraw(__in LPSTR szDevice);
    void ReleaseDirectDraw();
    HRESULT IsDirectDrawLoaded();
    LPDIRECTDRAW GetDirectDraw();
    BOOL IsDirectDrawVersion1();
};

#endif // __VIDEOCTL__
