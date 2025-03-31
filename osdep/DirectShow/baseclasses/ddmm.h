//------------------------------------------------------------------------------
// File: DDMM.h
//
// Desc: DirectShow base classes - efines routines for using DirectDraw
//       on a multimonitor system.
//
// Copyright (c) 1995-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C"
{      /* Assume C declarations for C++ */
#endif /* __cplusplus */

// DDRAW.H might not include these
#ifndef DDENUM_ATTACHEDSECONDARYDEVICES
#define DDENUM_ATTACHEDSECONDARYDEVICES 0x00000001L
#endif

    typedef HRESULT (*PDRAWCREATE)(IID *, LPDIRECTDRAW *, LPUNKNOWN);
    typedef HRESULT (*PDRAWENUM)(LPDDENUMCALLBACKA, LPVOID);

    IDirectDraw *DirectDrawCreateFromDevice(__in_opt LPSTR, PDRAWCREATE, PDRAWENUM);
    IDirectDraw *DirectDrawCreateFromDeviceEx(__in_opt LPSTR, PDRAWCREATE, LPDIRECTDRAWENUMERATEEXA);

#ifdef __cplusplus
}
#endif /* __cplusplus */
