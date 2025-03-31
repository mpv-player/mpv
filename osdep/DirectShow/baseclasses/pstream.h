//------------------------------------------------------------------------------
// File: PStream.h
//
// Desc: DirectShow base classes - defines a class for persistent properties
//       of filters.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __PSTREAM__
#define __PSTREAM__

// Base class for persistent properties of filters
// (i.e. filter properties in saved graphs)

// The simplest way to use this is:
// 1. Arrange for your filter to inherit this class
// 2. Implement in your class WriteToStream and ReadFromStream
//    These will override the "do nothing" functions here.
// 3. Change your NonDelegatingQueryInterface to handle IPersistStream
// 4. Implement SizeMax to return the number of bytes of data you save.
//    If you save UNICODE data, don't forget a char is 2 bytes.
// 5. Whenever your data changes, call SetDirty()
//
// At some point you may decide to alter, or extend the format of your data.
// At that point you will wish that you had a version number in all the old
// saved graphs, so that you can tell, when you read them, whether they
// represent the old or new form.  To assist you in this, this class
// writes and reads a version number.
// When it writes, it calls GetSoftwareVersion()  to enquire what version
// of the software we have at the moment.  (In effect this is a version number
// of the data layout in the file).  It writes this as the first thing in the data.
// If you want to change the version, implement (override) GetSoftwareVersion().
// It reads this from the file into mPS_dwFileVersion before calling ReadFromStream,
// so in ReadFromStream you can check mPS_dwFileVersion to see if you are reading
// an old version file.
// Normally you should accept files whose version is no newer than the software
// version that's reading them.

// CPersistStream
//
// Implements IPersistStream.
// See 'OLE Programmers Reference (Vol 1):Structured Storage Overview' for
// more implementation information.
class CPersistStream : public IPersistStream
{
  private:
    // Internal state:

  protected:
    DWORD mPS_dwFileVersion; // version number of file (being read)
    BOOL mPS_fDirty;

  public:
    // IPersistStream methods

    STDMETHODIMP IsDirty() { return (mPS_fDirty ? S_OK : S_FALSE); } // note FALSE means clean
    STDMETHODIMP Load(LPSTREAM pStm);
    STDMETHODIMP Save(LPSTREAM pStm, BOOL fClearDirty);
    STDMETHODIMP GetSizeMax(__out ULARGE_INTEGER *pcbSize)
    // Allow 24 bytes for version.
    {
        pcbSize->QuadPart = 12 * sizeof(WCHAR) + SizeMax();
        return NOERROR;
    }

    // implementation

    CPersistStream(IUnknown *punk, __inout HRESULT *phr);
    ~CPersistStream();

    HRESULT SetDirty(BOOL fDirty)
    {
        mPS_fDirty = fDirty;
        return NOERROR;
    }

    // override to reveal IPersist & IPersistStream
    // STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

    // --- IPersist ---

    // You must override this to provide your own class id
    STDMETHODIMP GetClassID(__out CLSID *pClsid) PURE;

    // overrideable if you want
    // file version number.  Override it if you ever change format
    virtual DWORD GetSoftwareVersion(void) { return 0; }

    //=========================================================================
    // OVERRIDE THESE to read and write your data
    // OVERRIDE THESE to read and write your data
    // OVERRIDE THESE to read and write your data

    virtual int SizeMax() { return 0; }
    virtual HRESULT WriteToStream(IStream *pStream);
    virtual HRESULT ReadFromStream(IStream *pStream);
    //=========================================================================

  private:
};

// --- Useful helpers ---

// Writes an int to an IStream as UNICODE.
STDAPI WriteInt(IStream *pIStream, int n);

// inverse of WriteInt
STDAPI_(int) ReadInt(IStream *pIStream, __out HRESULT &hr);

#endif // __PSTREAM__
