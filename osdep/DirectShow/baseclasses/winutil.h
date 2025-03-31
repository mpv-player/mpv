//------------------------------------------------------------------------------
// File: WinUtil.h
//
// Desc: DirectShow base classes - defines generic handler classes.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

// Make sure that you call PrepareWindow to initialise the window after
// the object has been constructed. It is a separate method so that
// derived classes can override useful methods like MessageLoop. Also
// any derived class must call DoneWithWindow in its destructor. If it
// doesn't a message may be retrieved and call a derived class member
// function while a thread is executing the base class destructor code

#ifndef __WINUTIL__
#define __WINUTIL__

const int DEFWIDTH = 320;             // Initial window width
const int DEFHEIGHT = 240;            // Initial window height
const int CAPTION = 256;              // Maximum length of caption
const int TIMELENGTH = 50;            // Maximum length of times
const int PROFILESTR = 128;           // Normal profile string
const WORD PALVERSION = 0x300;        // GDI palette version
const LONG PALETTE_VERSION = (LONG)1; // Initial palette version
const COLORREF VIDEO_COLOUR = 0;      // Defaults to black background
const HANDLE hMEMORY = (HANDLE)(-1);  // Says to open as memory file

#define WIDTH(x) ((*(x)).right - (*(x)).left)
#define HEIGHT(x) ((*(x)).bottom - (*(x)).top)
#define SHOWSTAGE TEXT("WM_SHOWSTAGE")
#define SHOWSTAGETOP TEXT("WM_SHOWSTAGETOP")
#define REALIZEPALETTE TEXT("WM_REALIZEPALETTE")

class AM_NOVTABLE CBaseWindow
{
  protected:
    HINSTANCE m_hInstance;   // Global module instance handle
    HWND m_hwnd;             // Handle for our window
    HDC m_hdc;               // Device context for the window
    LONG m_Width;            // Client window width
    LONG m_Height;           // Client window height
    BOOL m_bActivated;       // Has the window been activated
    LPTSTR m_pClassName;     // Static string holding class name
    DWORD m_ClassStyles;     // Passed in to our constructor
    DWORD m_WindowStyles;    // Likewise the initial window styles
    DWORD m_WindowStylesEx;  // And the extended window styles
    UINT m_ShowStageMessage; // Have the window shown with focus
    UINT m_ShowStageTop;     // Makes the window WS_EX_TOPMOST
    UINT m_RealizePalette;   // Makes us realize our new palette
    HDC m_MemoryDC;          // Used for fast BitBlt operations
    HPALETTE m_hPalette;     // Handle to any palette we may have
    BYTE m_bNoRealize;       // Don't realize palette now
    BYTE m_bBackground;      // Should we realise in background
    BYTE m_bRealizing;       // already realizing the palette
    CCritSec m_WindowLock;   // Serialise window object access
    BOOL m_bDoGetDC;         // Should this window get a DC
    bool m_bDoPostToDestroy; // Use PostMessage to destroy
    CCritSec m_PaletteLock;  // This lock protects m_hPalette.
                             // It should be held anytime the
                             // program use the value of m_hPalette.

    // Maps windows message procedure into C++ methods
    friend LRESULT CALLBACK WndProc(HWND hwnd,      // Window handle
                                    UINT uMsg,      // Message ID
                                    WPARAM wParam,  // First parameter
                                    LPARAM lParam); // Other parameter

    virtual LRESULT OnPaletteChange(HWND hwnd, UINT Message);

  public:
    CBaseWindow(BOOL bDoGetDC = TRUE, bool bPostToDestroy = false);

#ifdef DEBUG
    virtual ~CBaseWindow();
#endif

    virtual HRESULT DoneWithWindow();
    virtual HRESULT PrepareWindow();
    virtual HRESULT InactivateWindow();
    virtual HRESULT ActivateWindow();
    virtual BOOL OnSize(LONG Width, LONG Height);
    virtual BOOL OnClose();
    virtual RECT GetDefaultRect();
    virtual HRESULT UninitialiseWindow();
    virtual HRESULT InitialiseWindow(HWND hwnd);

    HRESULT CompleteConnect();
    HRESULT DoCreateWindow();

    HRESULT PerformanceAlignWindow();
    HRESULT DoShowWindow(LONG ShowCmd);
    void PaintWindow(BOOL bErase);
    void DoSetWindowForeground(BOOL bFocus);
    virtual HRESULT SetPalette(HPALETTE hPalette);
    void SetRealize(BOOL bRealize) { m_bNoRealize = !bRealize; }

    //  Jump over to the window thread to set the current palette
    HRESULT SetPalette();
    void UnsetPalette(void);
    virtual HRESULT DoRealisePalette(BOOL bForceBackground = FALSE);

    void LockPaletteLock();
    void UnlockPaletteLock();

    virtual BOOL PossiblyEatMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) { return FALSE; };

    // Access our window information

    bool WindowExists();
    LONG GetWindowWidth();
    LONG GetWindowHeight();
    HWND GetWindowHWND();
    HDC GetMemoryHDC();
    HDC GetWindowHDC();

#ifdef DEBUG
    HPALETTE GetPalette();
#endif // DEBUG

    // This is the window procedure the derived object should override

    virtual LRESULT OnReceiveMessage(HWND hwnd,      // Window handle
                                     UINT uMsg,      // Message ID
                                     WPARAM wParam,  // First parameter
                                     LPARAM lParam); // Other parameter

    // Must be overriden to return class and window styles

    virtual LPTSTR GetClassWindowStyles(__out DWORD *pClassStyles,          // Class styles
                                        __out DWORD *pWindowStyles,         // Window styles
                                        __out DWORD *pWindowStylesEx) PURE; // Extended styles
};

// This helper class is entirely subservient to the owning CBaseWindow object
// All this object does is to split out the actual drawing operation from the
// main object (because it was becoming too large). We have a number of entry
// points to set things like the draw device contexts, to implement the actual
// drawing and to set the destination rectangle in the client window. We have
// no critical section locking in this class because we are used exclusively
// by the owning window object which looks after serialising calls into us

// If you want to use this class make sure you call NotifyAllocator once the
// allocate has been agreed, also call NotifyMediaType with a pointer to a
// NON stack based CMediaType once that has been set (we keep a pointer to
// the original rather than taking a copy). When the palette changes call
// IncrementPaletteVersion (easiest thing to do is to also call this method
// in the SetMediaType method most filters implement). Finally before you
// start rendering anything call SetDrawContext so that we can get the HDCs
// for drawing from the CBaseWindow object we are given during construction

class CDrawImage
{
  protected:
    CBaseWindow *m_pBaseWindow;  // Owning video window object
    CRefTime m_StartSample;      // Start time for the current sample
    CRefTime m_EndSample;        // And likewise it's end sample time
    HDC m_hdc;                   // Main window device context
    HDC m_MemoryDC;              // Offscreen draw device context
    RECT m_TargetRect;           // Target destination rectangle
    RECT m_SourceRect;           // Source image rectangle
    BOOL m_bStretch;             // Do we have to stretch the images
    BOOL m_bUsingImageAllocator; // Are the samples shared DIBSECTIONs
    CMediaType *m_pMediaType;    // Pointer to the current format
    int m_perfidRenderTime;      // Time taken to render an image
    LONG m_PaletteVersion;       // Current palette version cookie

    // Draw the video images in the window

    void SlowRender(IMediaSample *pMediaSample);
    void FastRender(IMediaSample *pMediaSample);
    void DisplaySampleTimes(IMediaSample *pSample);
    void UpdateColourTable(HDC hdc, __in BITMAPINFOHEADER *pbmi);
    void SetStretchMode();

  public:
    // Used to control the image drawing

    CDrawImage(__inout CBaseWindow *pBaseWindow);
    BOOL DrawImage(IMediaSample *pMediaSample);
    BOOL DrawVideoImageHere(HDC hdc, IMediaSample *pMediaSample, __in LPRECT lprcSrc, __in LPRECT lprcDst);
    void SetDrawContext();
    void SetTargetRect(__in RECT *pTargetRect);
    void SetSourceRect(__in RECT *pSourceRect);
    void GetTargetRect(__out RECT *pTargetRect);
    void GetSourceRect(__out RECT *pSourceRect);
    virtual RECT ScaleSourceRect(const RECT *pSource);

    // Handle updating palettes as they change

    LONG GetPaletteVersion();
    void ResetPaletteVersion();
    void IncrementPaletteVersion();

    // Tell us media types and allocator assignments

    void NotifyAllocator(BOOL bUsingImageAllocator);
    void NotifyMediaType(__in CMediaType *pMediaType);
    BOOL UsingImageAllocator();

    // Called when we are about to draw an image

    void NotifyStartDraw() { MSR_START(m_perfidRenderTime); };

    // Called when we complete an image rendering

    void NotifyEndDraw() { MSR_STOP(m_perfidRenderTime); };
};

// This is the structure used to keep information about each GDI DIB. All the
// samples we create from our allocator will have a DIBSECTION allocated to
// them. When we receive the sample we know we can BitBlt straight to an HDC

typedef struct tagDIBDATA
{

    LONG PaletteVersion;   // Current palette version in use
    DIBSECTION DibSection; // Details of DIB section allocated
    HBITMAP hBitmap;       // Handle to bitmap for drawing
    HANDLE hMapping;       // Handle to shared memory block
    BYTE *pBase;           // Pointer to base memory address

} DIBDATA;

// This class inherits from CMediaSample and uses all of it's methods but it
// overrides the constructor to initialise itself with the DIBDATA structure
// When we come to render an IMediaSample we will know if we are using our own
// allocator, and if we are, we can cast the IMediaSample to a pointer to one
// of these are retrieve the DIB section information and hence the HBITMAP

class CImageSample : public CMediaSample
{
  protected:
    DIBDATA m_DibData; // Information about the DIBSECTION
    BOOL m_bInit;      // Is the DIB information setup

  public:
    // Constructor

    CImageSample(__inout CBaseAllocator *pAllocator, __in_opt LPCTSTR pName, __inout HRESULT *phr,
                 __in_bcount(length) LPBYTE pBuffer, LONG length);

    // Maintain the DIB/DirectDraw state

    void SetDIBData(__in DIBDATA *pDibData);
    __out DIBDATA *GetDIBData();
};

// This is an allocator based on the abstract CBaseAllocator base class that
// allocates sample buffers in shared memory. The number and size of these
// are determined when the output pin calls Prepare on us. The shared memory
// blocks are used in subsequent calls to GDI CreateDIBSection, once that
// has been done the output pin can fill the buffers with data which will
// then be handed to GDI through BitBlt calls and thereby remove one copy

class CImageAllocator : public CBaseAllocator
{
  protected:
    CBaseFilter *m_pFilter;   // Delegate reference counts to
    CMediaType *m_pMediaType; // Pointer to the current format

    // Used to create and delete samples

    HRESULT Alloc();
    void Free();

    // Manage the shared DIBSECTION and DCI/DirectDraw buffers

    HRESULT CreateDIB(LONG InSize, DIBDATA &DibData);
    STDMETHODIMP CheckSizes(__in ALLOCATOR_PROPERTIES *pRequest);
    virtual CImageSample *CreateImageSample(__in_bcount(Length) LPBYTE pData, LONG Length);

  public:
    // Constructor and destructor

    CImageAllocator(__inout CBaseFilter *pFilter, __in_opt LPCTSTR pName, __inout HRESULT *phr);
#ifdef DEBUG
    ~CImageAllocator();
#endif

    STDMETHODIMP_(ULONG) NonDelegatingAddRef();
    STDMETHODIMP_(ULONG) NonDelegatingRelease();
    void NotifyMediaType(__in CMediaType *pMediaType);

    // Agree the number of buffers to be used and their size

    STDMETHODIMP SetProperties(__in ALLOCATOR_PROPERTIES *pRequest, __out ALLOCATOR_PROPERTIES *pActual);
};

// This class is a fairly specialised helper class for image renderers that
// have to create and manage palettes. The CBaseWindow class looks after
// realising palettes once they have been installed. This class can be used
// to create the palette handles from a media format (which must contain a
// VIDEOINFO structure in the format block). We try to make the palette an
// identity palette to maximise performance and also only change palettes
// if actually required to (we compare palette colours before updating).
// All the methods are virtual so that they can be overriden if so required

class CImagePalette
{
  protected:
    CBaseWindow *m_pBaseWindow; // Window to realise palette in
    CBaseFilter *m_pFilter;     // Media filter to send events
    CDrawImage *m_pDrawImage;   // Object who will be drawing
    HPALETTE m_hPalette;        // The palette handle we own

  public:
    CImagePalette(__inout CBaseFilter *pBaseFilter, __inout CBaseWindow *pBaseWindow, __inout CDrawImage *pDrawImage);

#ifdef DEBUG
    virtual ~CImagePalette();
#endif

    static HPALETTE MakePalette(const VIDEOINFOHEADER *pVideoInfo, __in LPSTR szDevice);
    HRESULT RemovePalette();
    static HRESULT MakeIdentityPalette(__inout_ecount_full(iColours) PALETTEENTRY *pEntry, INT iColours,
                                       __in LPSTR szDevice);
    HRESULT CopyPalette(const CMediaType *pSrc, __out CMediaType *pDest);
    BOOL ShouldUpdate(const VIDEOINFOHEADER *pNewInfo, const VIDEOINFOHEADER *pOldInfo);
    HRESULT PreparePalette(const CMediaType *pmtNew, const CMediaType *pmtOld, __in LPSTR szDevice);

    BOOL DrawVideoImageHere(HDC hdc, IMediaSample *pMediaSample, __in LPRECT lprcSrc, __in LPRECT lprcDst)
    {
        return m_pDrawImage->DrawVideoImageHere(hdc, pMediaSample, lprcSrc, lprcDst);
    }
};

// Another helper class really for video based renderers. Most such renderers
// need to know what the display format is to some degree or another. This
// class initialises itself with the display format. The format can be asked
// for through GetDisplayFormat and various other accessor functions. If a
// filter detects a display format change (perhaps it gets a WM_DEVMODECHANGE
// message then it can call RefreshDisplayType to reset that format). Also
// many video renderers will want to check formats as they are proposed by
// source filters. This class provides methods to check formats and only
// accept those video formats that can be efficiently drawn using GDI calls

class CImageDisplay : public CCritSec
{
  protected:
    // This holds the display format; biSize should not be too big, so we can
    // safely use the VIDEOINFO structure
    VIDEOINFO m_Display;

    static DWORD CountSetBits(const DWORD Field);
    static DWORD CountPrefixBits(const DWORD Field);
    static BOOL CheckBitFields(const VIDEOINFO *pInput);

  public:
    // Constructor and destructor

    CImageDisplay();

    // Used to manage BITMAPINFOHEADERs and the display format

    const VIDEOINFO *GetDisplayFormat();
    HRESULT RefreshDisplayType(__in_opt LPSTR szDeviceName);
    static BOOL CheckHeaderValidity(const VIDEOINFO *pInput);
    static BOOL CheckPaletteHeader(const VIDEOINFO *pInput);
    BOOL IsPalettised();
    WORD GetDisplayDepth();

    // Provide simple video format type checking

    HRESULT CheckMediaType(const CMediaType *pmtIn);
    HRESULT CheckVideoType(const VIDEOINFO *pInput);
    HRESULT UpdateFormat(__inout VIDEOINFO *pVideoInfo);
    const DWORD *GetBitMasks(const VIDEOINFO *pVideoInfo);

    BOOL GetColourMask(__out DWORD *pMaskRed, __out DWORD *pMaskGreen, __out DWORD *pMaskBlue);
};

//  Convert a FORMAT_VideoInfo to FORMAT_VideoInfo2
STDAPI ConvertVideoInfoToVideoInfo2(__inout AM_MEDIA_TYPE *pmt);

//  Check a media type containing VIDEOINFOHEADER
STDAPI CheckVideoInfoType(const AM_MEDIA_TYPE *pmt);

//  Check a media type containing VIDEOINFOHEADER
STDAPI CheckVideoInfo2Type(const AM_MEDIA_TYPE *pmt);

#endif // __WINUTIL__
