//------------------------------------------------------------------------------
// File: FourCC.h
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

// FOURCCMap
//
// provides a mapping between old-style multimedia format DWORDs
// and new-style GUIDs.
//
// A range of 4 billion GUIDs has been allocated to ensure that this
// mapping can be done straightforwardly one-to-one in both directions.
//
// January 95

#ifndef __FOURCC__
#define __FOURCC__

// Multimedia format types are marked with DWORDs built from four 8-bit
// chars and known as FOURCCs. New multimedia AM_MEDIA_TYPE definitions include
// a subtype GUID. In order to simplify the mapping, GUIDs in the range:
//    XXXXXXXX-0000-0010-8000-00AA00389B71
// are reserved for FOURCCs.

class FOURCCMap : public GUID
{

  public:
    FOURCCMap();
    FOURCCMap(DWORD Fourcc);
    FOURCCMap(const GUID *);

    DWORD GetFOURCC(void);
    void SetFOURCC(DWORD fourcc);
    void SetFOURCC(const GUID *);

  private:
    void InitGUID();
};

#define GUID_Data2 0
#define GUID_Data3 0x10
#define GUID_Data4_1 0xaa000080
#define GUID_Data4_2 0x719b3800

inline void FOURCCMap::InitGUID()
{
    Data2 = GUID_Data2;
    Data3 = GUID_Data3;
    ((DWORD *)Data4)[0] = GUID_Data4_1;
    ((DWORD *)Data4)[1] = GUID_Data4_2;
}

inline FOURCCMap::FOURCCMap()
{
    InitGUID();
    SetFOURCC(DWORD(0));
}

inline FOURCCMap::FOURCCMap(DWORD fourcc)
{
    InitGUID();
    SetFOURCC(fourcc);
}

inline FOURCCMap::FOURCCMap(const GUID *pGuid)
{
    InitGUID();
    SetFOURCC(pGuid);
}

inline void FOURCCMap::SetFOURCC(const GUID *pGuid)
{
    FOURCCMap *p = (FOURCCMap *)pGuid;
    SetFOURCC(p->GetFOURCC());
}

inline void FOURCCMap::SetFOURCC(DWORD fourcc)
{
    Data1 = fourcc;
}

inline DWORD FOURCCMap::GetFOURCC(void)
{
    return Data1;
}

#endif /* __FOURCC__ */
