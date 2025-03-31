//------------------------------------------------------------------------------
// File: AMVideo.cpp
//
// Desc: DirectShow base classes - implements helper functions for
//       bitmap formats.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <limits.h>

// These are bit field masks for true colour devices

const DWORD bits555[] = {0x007C00, 0x0003E0, 0x00001F};
const DWORD bits565[] = {0x00F800, 0x0007E0, 0x00001F};
const DWORD bits888[] = {0xFF0000, 0x00FF00, 0x0000FF};

// This maps bitmap subtypes into a bits per pixel value and also a
// name. unicode and ansi versions are stored because we have to
// return a pointer to a static string.
// clang-format off
const struct {
    const GUID *pSubtype;
    WORD BitCount;
    CHAR *pName;
    WCHAR *wszName;
} BitCountMap[] =  { &MEDIASUBTYPE_RGB1,        1,   "RGB Monochrome",     L"RGB Monochrome",   
                     &MEDIASUBTYPE_RGB4,        4,   "RGB VGA",            L"RGB VGA",          
                     &MEDIASUBTYPE_RGB8,        8,   "RGB 8",              L"RGB 8",            
                     &MEDIASUBTYPE_RGB565,      16,  "RGB 565 (16 bit)",   L"RGB 565 (16 bit)", 
                     &MEDIASUBTYPE_RGB555,      16,  "RGB 555 (16 bit)",   L"RGB 555 (16 bit)", 
                     &MEDIASUBTYPE_RGB24,       24,  "RGB 24",             L"RGB 24",           
                     &MEDIASUBTYPE_RGB32,       32,  "RGB 32",             L"RGB 32",
                     &MEDIASUBTYPE_ARGB32,      32,  "ARGB 32",            L"ARGB 32",
                     &MEDIASUBTYPE_Overlay,     0,   "Overlay",            L"Overlay",          
                     &GUID_NULL,                0,   "UNKNOWN",            L"UNKNOWN"           
};
// clang-format on

// Return the size of the bitmap as defined by this header

STDAPI_(DWORD) GetBitmapSize(const BITMAPINFOHEADER *pHeader)
{
    return DIBSIZE(*pHeader);
}

// This is called if the header has a 16 bit colour depth and needs to work
// out the detailed type from the bit fields (either RGB 565 or RGB 555)

STDAPI_(const GUID) GetTrueColorType(const BITMAPINFOHEADER *pbmiHeader)
{
    BITMAPINFO *pbmInfo = (BITMAPINFO *)pbmiHeader;
    ASSERT(pbmiHeader->biBitCount == 16);

    // If its BI_RGB then it's RGB 555 by default

    if (pbmiHeader->biCompression == BI_RGB)
    {
        return MEDIASUBTYPE_RGB555;
    }

    // Compare the bit fields with RGB 555

    DWORD *pMask = (DWORD *)pbmInfo->bmiColors;
    if (pMask[0] == bits555[0])
    {
        if (pMask[1] == bits555[1])
        {
            if (pMask[2] == bits555[2])
            {
                return MEDIASUBTYPE_RGB555;
            }
        }
    }

    // Compare the bit fields with RGB 565

    pMask = (DWORD *)pbmInfo->bmiColors;
    if (pMask[0] == bits565[0])
    {
        if (pMask[1] == bits565[1])
        {
            if (pMask[2] == bits565[2])
            {
                return MEDIASUBTYPE_RGB565;
            }
        }
    }
    return GUID_NULL;
}

// Given a BITMAPINFOHEADER structure this returns the GUID sub type that is
// used to describe it in format negotiations. For example a video codec fills
// in the format block with a VIDEOINFO structure, it also fills in the major
// type with MEDIATYPE_VIDEO and the subtype with a GUID that matches the bit
// count, for example if it is an eight bit image then MEDIASUBTYPE_RGB8

STDAPI_(const GUID) GetBitmapSubtype(const BITMAPINFOHEADER *pbmiHeader)
{
    ASSERT(pbmiHeader);

    // If it's not RGB then create a GUID from the compression type

    if (pbmiHeader->biCompression != BI_RGB)
    {
        if (pbmiHeader->biCompression != BI_BITFIELDS)
        {
            FOURCCMap FourCCMap(pbmiHeader->biCompression);
            return (const GUID)FourCCMap;
        }
    }

    // Map the RGB DIB bit depth to a image GUID

    switch (pbmiHeader->biBitCount)
    {
    case 1: return MEDIASUBTYPE_RGB1;
    case 4: return MEDIASUBTYPE_RGB4;
    case 8: return MEDIASUBTYPE_RGB8;
    case 16: return GetTrueColorType(pbmiHeader);
    case 24: return MEDIASUBTYPE_RGB24;
    case 32: return MEDIASUBTYPE_RGB32;
    }
    return GUID_NULL;
}

// Given a video bitmap subtype we return the number of bits per pixel it uses
// We return a WORD bit count as thats what the BITMAPINFOHEADER uses. If the
// GUID subtype is not found in the table we return an invalid USHRT_MAX

STDAPI_(WORD) GetBitCount(const GUID *pSubtype)
{
    ASSERT(pSubtype);
    const GUID *pMediaSubtype;
    INT iPosition = 0;

    // Scan the mapping list seeing if the source GUID matches any known
    // bitmap subtypes, the list is terminated by a GUID_NULL entry

    while (TRUE)
    {
        pMediaSubtype = BitCountMap[iPosition].pSubtype;
        if (IsEqualGUID(*pMediaSubtype, GUID_NULL))
        {
            return USHRT_MAX;
        }
        if (IsEqualGUID(*pMediaSubtype, *pSubtype))
        {
            return BitCountMap[iPosition].BitCount;
        }
        iPosition++;
    }
}

// Given a bitmap subtype we return a description name that can be used for
// debug purposes. In a retail build this function still returns the names
// If the subtype isn't found in the lookup table we return string UNKNOWN

int LocateSubtype(const GUID *pSubtype)
{
    ASSERT(pSubtype);
    const GUID *pMediaSubtype;
    INT iPosition = 0;

    // Scan the mapping list seeing if the source GUID matches any known
    // bitmap subtypes, the list is terminated by a GUID_NULL entry

    while (TRUE)
    {
        pMediaSubtype = BitCountMap[iPosition].pSubtype;
        if (IsEqualGUID(*pMediaSubtype, *pSubtype) || IsEqualGUID(*pMediaSubtype, GUID_NULL))
        {
            break;
        }

        iPosition++;
    }

    return iPosition;
}

STDAPI_(WCHAR *) GetSubtypeNameW(const GUID *pSubtype)
{
    return BitCountMap[LocateSubtype(pSubtype)].wszName;
}

STDAPI_(CHAR *) GetSubtypeNameA(const GUID *pSubtype)
{
    return BitCountMap[LocateSubtype(pSubtype)].pName;
}

#ifndef GetSubtypeName
#error wxutil.h should have defined GetSubtypeName
#endif
#undef GetSubtypeName

// this is here for people that linked to it directly; most people
// would use the header file that picks the A or W version.
STDAPI_(CHAR *) GetSubtypeName(const GUID *pSubtype)
{
    return GetSubtypeNameA(pSubtype);
}

// The mechanism for describing a bitmap format is with the BITMAPINFOHEADER
// This is really messy to deal with because it invariably has fields that
// follow it holding bit fields, palettes and the rest. This function gives
// the number of bytes required to hold a VIDEOINFO that represents it. This
// count includes the prefix information (like the rcSource rectangle) the
// BITMAPINFOHEADER field, and any other colour information on the end.
//
// WARNING If you want to copy a BITMAPINFOHEADER into a VIDEOINFO always make
// sure that you use the HEADER macro because the BITMAPINFOHEADER field isn't
// right at the start of the VIDEOINFO (there are a number of other fields),
//
//     CopyMemory(HEADER(pVideoInfo),pbmi,sizeof(BITMAPINFOHEADER));
//

STDAPI_(LONG) GetBitmapFormatSize(const BITMAPINFOHEADER *pHeader)
{
    // Everyone has this to start with this
    LONG Size = SIZE_PREHEADER + pHeader->biSize;

    ASSERT(pHeader->biSize >= sizeof(BITMAPINFOHEADER));

    // Does this format use a palette, if the number of colours actually used
    // is zero then it is set to the maximum that are allowed for that colour
    // depth (an example is 256 for eight bits). Truecolour formats may also
    // pass a palette with them in which case the used count is non zero

    // This would scare me.
    ASSERT(pHeader->biBitCount <= iPALETTE || pHeader->biClrUsed == 0);

    if (pHeader->biBitCount <= iPALETTE || pHeader->biClrUsed)
    {
        LONG Entries = (DWORD)1 << pHeader->biBitCount;
        if (pHeader->biClrUsed)
        {
            Entries = pHeader->biClrUsed;
        }
        Size += Entries * sizeof(RGBQUAD);
    }

    // Truecolour formats may have a BI_BITFIELDS specifier for compression
    // type which means that room for three DWORDs should be allocated that
    // specify where in each pixel the RGB colour components may be found

    if (pHeader->biCompression == BI_BITFIELDS)
    {
        Size += SIZE_MASKS;
    }

    // A BITMAPINFO for a palettised image may also contain a palette map that
    // provides the information to map from a source palette to a destination
    // palette during a BitBlt for example, because this information is only
    // ever processed during drawing you don't normally store the palette map
    // nor have any way of knowing if it is present in the data structure

    return Size;
}

// Returns TRUE if the VIDEOINFO contains a palette

STDAPI_(BOOL) ContainsPalette(const VIDEOINFOHEADER *pVideoInfo)
{
    if (PALETTISED(pVideoInfo) == FALSE)
    {
        if (pVideoInfo->bmiHeader.biClrUsed == 0)
        {
            return FALSE;
        }
    }
    return TRUE;
}

// Return a pointer to the first entry in a palette

STDAPI_(const RGBQUAD *) GetBitmapPalette(const VIDEOINFOHEADER *pVideoInfo)
{
    if (pVideoInfo->bmiHeader.biCompression == BI_BITFIELDS)
    {
        return TRUECOLOR(pVideoInfo)->bmiColors;
    }
    return COLORS(pVideoInfo);
}
