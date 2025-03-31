//------------------------------------------------------------------------------
// File: PStream.cpp
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <strsafe.h>

#ifdef PERF
#include <measure.h>
#endif
// #include "pstream.h"  in streams.h

//
// Constructor
//
CPersistStream::CPersistStream(IUnknown *punk, __inout HRESULT *phr)
    : mPS_fDirty(FALSE)
{
    mPS_dwFileVersion = GetSoftwareVersion();
}

//
// Destructor
//
CPersistStream::~CPersistStream()
{
    // Nothing to do
}

#if 0
SAMPLE CODE TO COPY - not active at the moment

//
// NonDelegatingQueryInterface
//
// This object supports IPersist & IPersistStream
STDMETHODIMP CPersistStream::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    if (riid == IID_IPersist) {
        return GetInterface((IPersist *) this, ppv);      // ???
    }
    else if (riid == IID_IPersistStream) {
        return GetInterface((IPersistStream *) this, ppv);
    }
    else {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}
#endif

//
// WriteToStream
//
// Writes to the stream (default action is to write nothing)
HRESULT CPersistStream::WriteToStream(IStream *pStream)
{
    // You can override this to do things like
    // hr = pStream->Write(MyStructure, sizeof(MyStructure), NULL);

    return NOERROR;
}

HRESULT CPersistStream::ReadFromStream(IStream *pStream)
{
    // You can override this to do things like
    // hr = pStream->Read(MyStructure, sizeof(MyStructure), NULL);

    return NOERROR;
}

//
// Load
//
// Load all the data from the given stream
STDMETHODIMP CPersistStream::Load(LPSTREAM pStm)
{
    HRESULT hr;
    // Load the version number then the data
    mPS_dwFileVersion = ReadInt(pStm, hr);
    if (FAILED(hr))
    {
        return hr;
    }

    return ReadFromStream(pStm);
} // Load

//
// Save
//
// Save the contents of this Stream.
STDMETHODIMP CPersistStream::Save(LPSTREAM pStm, BOOL fClearDirty)
{

    HRESULT hr = WriteInt(pStm, GetSoftwareVersion());
    if (FAILED(hr))
    {
        return hr;
    }

    hr = WriteToStream(pStm);
    if (FAILED(hr))
    {
        return hr;
    }

    mPS_fDirty = !fClearDirty;

    return hr;
} // Save

// WriteInt
//
// Writes an integer to an IStream as 11 UNICODE characters followed by one space.
// You could use this for shorts or unsigneds or anything (up to 32 bits)
// where the value isn't actually truncated by squeezing it into 32 bits.
// Values such as (unsigned) 0x80000000 would come out as -2147483648
// but would then load as 0x80000000 through ReadInt.  Cast as you please.

STDAPI WriteInt(IStream *pIStream, int n)
{
    WCHAR Buff[13]; // Allows for trailing null that we don't write
    (void)StringCchPrintfW(Buff, NUMELMS(Buff), L"%011d ", n);
    return pIStream->Write(&(Buff[0]), 12 * sizeof(WCHAR), NULL);
} // WriteInt

// ReadInt
//
// Reads an integer from an IStream.
// Read as 4 bytes.  You could use this for shorts or unsigneds or anything
// where the value isn't actually truncated by squeezing it into 32 bits
// Striped down subset of what sscanf can do (without dragging in the C runtime)

STDAPI_(int) ReadInt(IStream *pIStream, __out HRESULT &hr)
{

    int Sign = 1;
    unsigned int n = 0; // result wil be n*Sign
    WCHAR wch;

    hr = pIStream->Read(&wch, sizeof(wch), NULL);
    if (FAILED(hr))
    {
        return 0;
    }

    if (wch == L'-')
    {
        Sign = -1;
        hr = pIStream->Read(&wch, sizeof(wch), NULL);
        if (FAILED(hr))
        {
            return 0;
        }
    }

    for (;;)
    {
        if (wch >= L'0' && wch <= L'9')
        {
            n = 10 * n + (int)(wch - L'0');
        }
        else if (wch == L' ' || wch == L'\t' || wch == L'\r' || wch == L'\n' || wch == L'\0')
        {
            break;
        }
        else
        {
            hr = VFW_E_INVALID_FILE_FORMAT;
            return 0;
        }

        hr = pIStream->Read(&wch, sizeof(wch), NULL);
        if (FAILED(hr))
        {
            return 0;
        }
    }

    if (n == 0x80000000 && Sign == -1)
    {
        // This is the negative number that has no positive version!
        return (int)n;
    }
    else
        return (int)n * Sign;
} // ReadInt

// The microsoft C/C++ compile generates level 4 warnings to the effect that
// a particular inline function (from some base class) was not needed.
// This line gets rid of hundreds of such unwanted messages and makes
// -W4 compilation feasible:
#pragma warning(disable : 4514)
