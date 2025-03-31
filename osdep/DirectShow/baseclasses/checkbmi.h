//  Copyright (c) 1992 - 1997  Microsoft Corporation.  All Rights Reserved.

#ifndef _CHECKBMI_H_
#define _CHECKBMI_H_

#ifdef __cplusplus
extern "C" {
#endif

//  Helper
__inline BOOL MultiplyCheckOverflow(DWORD a, DWORD b, __deref_out_range(==, a * b) DWORD *pab) {
    *pab = a * b;
    if ((a == 0) || (((*pab) / a) == b)) {
        return TRUE;
    }
    return FALSE;
}


//  Checks if the fields in a BITMAPINFOHEADER won't generate
//  overlows and buffer overruns
//  This is not a complete check and does not guarantee code using this structure will be secure
//  from attack
//  Bugs this is guarding against:
//        1.  Total structure size calculation overflowing
//        2.  biClrUsed > 256 for 8-bit palettized content
//        3.  Total bitmap size in bytes overflowing
//        4.  biSize < size of the base structure leading to accessessing random memory
//        5.  Total structure size exceeding know size of data
//

__success(return != 0) __inline BOOL ValidateBitmapInfoHeader(
    const BITMAPINFOHEADER *pbmi,   // pointer to structure to check
    __out_range(>=, sizeof(BITMAPINFOHEADER)) DWORD cbSize     // size of memory block containing structure
)
{
    DWORD dwWidthInBytes;
    DWORD dwBpp;
    DWORD dwWidthInBits;
    DWORD dwHeight;
    DWORD dwSizeImage;
    DWORD dwClrUsed;

    // Reject bad parameters - do the size check first to avoid reading bad memory
    if (cbSize < sizeof(BITMAPINFOHEADER) ||
        pbmi->biSize < sizeof(BITMAPINFOHEADER) ||
        pbmi->biSize > 4096) {
        return FALSE;
    }

    //  Reject 0 size
    if (pbmi->biWidth == 0 || pbmi->biHeight == 0) {
        return FALSE;
    }

    // Use bpp of 200 for validating against further overflows if not set for compressed format
    dwBpp = 200;

    if (pbmi->biBitCount > dwBpp) {
        return FALSE;
    }

    // Strictly speaking abs can overflow so cast explicitly to DWORD
    dwHeight = (DWORD)abs(pbmi->biHeight);

    if (!MultiplyCheckOverflow(dwBpp, (DWORD)pbmi->biWidth, &dwWidthInBits)) {
        return FALSE;
    }

    //  Compute correct width in bytes - rounding up to 4 bytes
    dwWidthInBytes = (dwWidthInBits / 8 + 3) & ~3;

    if (!MultiplyCheckOverflow(dwWidthInBytes, dwHeight, &dwSizeImage)) {
        return FALSE;
    }

    // Fail if total size is 0 - this catches indivual quantities being 0
    // Also don't allow huge values > 1GB which might cause arithmetic
    // errors for users
    if (dwSizeImage > 0x40000000 ||
        pbmi->biSizeImage > 0x40000000) {
        return FALSE;
    }

    //  Fail if biClrUsed looks bad
    if (pbmi->biClrUsed > 256) {
        return FALSE;
    }

    if (pbmi->biClrUsed == 0 && pbmi->biBitCount <= 8 && pbmi->biBitCount > 0) {
        dwClrUsed = (1 << pbmi->biBitCount);
    } else {
        dwClrUsed = pbmi->biClrUsed;
    }

    //  Check total size
    if (cbSize < pbmi->biSize + dwClrUsed * sizeof(RGBQUAD) +
                 (pbmi->biCompression == BI_BITFIELDS ? 3 * sizeof(DWORD) : 0)) {
        return FALSE;
    }

    //  If it is RGB validate biSizeImage - lots of code assumes the size is correct
    if (pbmi->biCompression == BI_RGB || pbmi->biCompression == BI_BITFIELDS) {
        if (pbmi->biSizeImage != 0) {
            DWORD dwBits = (DWORD)pbmi->biWidth * (DWORD)pbmi->biBitCount;
            DWORD dwWidthInBytes = ((DWORD)((dwBits+31) & (~31)) / 8);
            DWORD dwTotalSize = (DWORD)abs(pbmi->biHeight) * dwWidthInBytes;
            if (dwTotalSize > pbmi->biSizeImage) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

#ifdef __cplusplus
}
#endif

#endif // _CHECKBMI_H_
