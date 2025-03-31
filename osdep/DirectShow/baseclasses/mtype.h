//------------------------------------------------------------------------------
// File: MtType.h
//
// Desc: DirectShow base classes - defines a class that holds and manages
//       media type information.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __MTYPE__
#define __MTYPE__

/* Helper class that derived pin objects can use to compare media
   types etc. Has same data members as the struct AM_MEDIA_TYPE defined
   in the streams IDL file, but also has (non-virtual) functions */

class CMediaType : public _AMMediaType
{

  public:
    ~CMediaType();
    CMediaType();
    CMediaType(const GUID *majortype);
    CMediaType(const AM_MEDIA_TYPE &, __out_opt HRESULT *phr = NULL);
    CMediaType(const CMediaType &, __out_opt HRESULT *phr = NULL);

    CMediaType &operator=(const CMediaType &);
    CMediaType &operator=(const AM_MEDIA_TYPE &);

    BOOL operator==(const CMediaType &) const;
    BOOL operator!=(const CMediaType &) const;

    HRESULT Set(const CMediaType &rt);
    HRESULT Set(const AM_MEDIA_TYPE &rt);

    BOOL IsValid() const;

    const GUID *Type() const { return &majortype; };
    void SetType(const GUID *);
    const GUID *Subtype() const { return &subtype; };
    void SetSubtype(const GUID *);

    BOOL IsFixedSize() const { return bFixedSizeSamples; };
    BOOL IsTemporalCompressed() const { return bTemporalCompression; };
    ULONG GetSampleSize() const;

    void SetSampleSize(ULONG sz);
    void SetVariableSize();
    void SetTemporalCompression(BOOL bCompressed);

    // read/write pointer to format - can't change length without
    // calling SetFormat, AllocFormatBuffer or ReallocFormatBuffer

    BYTE *Format() const { return pbFormat; };
    ULONG FormatLength() const { return cbFormat; };

    void SetFormatType(const GUID *);
    const GUID *FormatType() const { return &formattype; };
    BOOL SetFormat(__in_bcount(length) BYTE *pFormat, ULONG length);
    void ResetFormatBuffer();
    BYTE *AllocFormatBuffer(ULONG length);
    BYTE *ReallocFormatBuffer(ULONG length);

    void InitMediaType();

    BOOL MatchesPartial(const CMediaType *ppartial) const;
    BOOL IsPartiallySpecified(void) const;
};

/* General purpose functions to copy and delete a task allocated AM_MEDIA_TYPE
   structure which is useful when using the IEnumMediaFormats interface as
   the implementation allocates the structures which you must later delete */

void WINAPI DeleteMediaType(__inout_opt AM_MEDIA_TYPE *pmt);
AM_MEDIA_TYPE *WINAPI CreateMediaType(AM_MEDIA_TYPE const *pSrc);
HRESULT WINAPI CopyMediaType(__out AM_MEDIA_TYPE *pmtTarget, const AM_MEDIA_TYPE *pmtSource);
void WINAPI FreeMediaType(__inout AM_MEDIA_TYPE &mt);

//  Initialize a media type from a WAVEFORMATEX

STDAPI CreateAudioMediaType(const WAVEFORMATEX *pwfx, __out AM_MEDIA_TYPE *pmt, BOOL bSetFormat);

#endif /* __MTYPE__ */
