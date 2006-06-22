/*
 * Basic types definitions
 *
 * Copyright 1996 Alexandre Julliard
 *
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 *
 */

#ifndef __WINE_WINDEF_H
#define __WINE_WINDEF_H

#ifdef __WINE__
# include "config.h"
# undef UNICODE
#endif

#ifdef _EGCS_
#define __stdcall
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Misc. constants. */

#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0

#ifdef TRUE
#undef TRUE
#endif
#define TRUE  1

#ifdef NULL
#undef NULL
#endif
#define NULL  0

/* Macros to map Winelib names to the correct implementation name */
/* depending on __WINE__ and UNICODE macros.                      */
/* Note that Winelib is purely Win32.                             */

#ifdef __WINE__
# define WINELIB_NAME_AW(func) \
    func##_must_be_suffixed_with_W_or_A_in_this_context \
    func##_must_be_suffixed_with_W_or_A_in_this_context
#else  /* __WINE__ */
# ifdef UNICODE
#  define WINELIB_NAME_AW(func) func##W
# else
#  define WINELIB_NAME_AW(func) func##A
# endif  /* UNICODE */
#endif  /* __WINE__ */

#ifdef __WINE__
# define DECL_WINELIB_TYPE_AW(type)  /* nothing */
#else   /* __WINE__ */
# define DECL_WINELIB_TYPE_AW(type)  typedef WINELIB_NAME_AW(type) type;
#endif  /* __WINE__ */

#ifndef NONAMELESSSTRUCT
# if defined(__WINE__) || !defined(_FORCENAMELESSSTRUCT)
#  define NONAMELESSSTRUCT
# endif
#endif /* !defined(NONAMELESSSTRUCT) */

#ifndef NONAMELESSUNION
# if defined(__WINE__) || !defined(_FORCENAMELESSUNION) || !defined(__cplusplus)
#  define NONAMELESSUNION
# endif
#endif /* !defined(NONAMELESSUNION) */

#ifndef NONAMELESSSTRUCT
#define DUMMYSTRUCTNAME
#define DUMMYSTRUCTNAME1
#define DUMMYSTRUCTNAME2
#define DUMMYSTRUCTNAME3
#define DUMMYSTRUCTNAME4
#define DUMMYSTRUCTNAME5
#else /* !defined(NONAMELESSSTRUCT) */
#define DUMMYSTRUCTNAME   s
#define DUMMYSTRUCTNAME1  s1
#define DUMMYSTRUCTNAME2  s2
#define DUMMYSTRUCTNAME3  s3
#define DUMMYSTRUCTNAME4  s4
#define DUMMYSTRUCTNAME5  s5
#endif /* !defined(NONAMELESSSTRUCT) */

#ifndef NONAMELESSUNION
#define DUMMYUNIONNAME
#define DUMMYUNIONNAME1
#define DUMMYUNIONNAME2
#define DUMMYUNIONNAME3
#define DUMMYUNIONNAME4
#define DUMMYUNIONNAME5
#else /* !defined(NONAMELESSUNION) */
#define DUMMYUNIONNAME   u
#define DUMMYUNIONNAME1  u1
#define DUMMYUNIONNAME2  u2
#define DUMMYUNIONNAME3  u3
#define DUMMYUNIONNAME4  u4
#define DUMMYUNIONNAME5  u5
#endif /* !defined(NONAMELESSUNION) */

/* Calling conventions definitions */

#ifdef __i386__
# if defined(__GNUC__) && ((__GNUC__ > 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 7)))
#  ifndef _EGCS_ 
#define __stdcall __attribute__((__stdcall__))
#define __cdecl   __attribute__((__cdecl__))
#  define __RESTORE_ES  __asm__ __volatile__("pushl %ds\n\tpopl %es")
#  endif
# else
// #  error You need gcc >= 2.7 to build Wine on a 386
# endif
#else 
# define __stdcall
# define __cdecl
# define __RESTORE_ES
#endif

#define CALLBACK    __stdcall
#define WINAPI      __stdcall
#define APIPRIVATE  __stdcall
#define PASCAL      __stdcall
#define pascal      __stdcall
#define _pascal     __stdcall
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
#define _stdcall    __stdcall
#define _fastcall   __stdcall
#define __fastcall  __stdcall
#endif
#define __export    __stdcall
#define CDECL       __cdecl
#define _CDECL      __cdecl
#define cdecl       __cdecl
#if !defined(__CYGWIN__) && !defined(__MINGW32__)
#define _cdecl      __cdecl
#endif
#define WINAPIV     __cdecl
#define APIENTRY    WINAPI

#if !defined(__CYGWIN__) && !defined(__MINGW32__)
#define __declspec(x)
#endif
#define dllimport
#define dllexport

#define CONST       const

/* Standard data types. These are the same for emulator and library. */

typedef void            VOID;
typedef int             INT;
typedef unsigned int    UINT;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;
typedef unsigned long   ULONG;
typedef unsigned char   BYTE;
typedef long            LONG;
typedef short           SHORT;
typedef unsigned short  USHORT;
typedef char            CHAR;
typedef unsigned char   UCHAR;

typedef LONG SCODE;

/* Some systems might have wchar_t, but we really need 16 bit characters */
typedef unsigned short  WCHAR;
typedef int             WIN_BOOL;
typedef double          DATE;
typedef double          DOUBLE;
typedef long long       LONGLONG;
typedef unsigned long long   ULONGLONG;

/* FIXME: Wine does not compile with strict on, therefore strict
 * handles are presently only usable on machines where sizeof(UINT) ==
 * sizeof(void*).  HANDLEs are supposed to be void* but a large amount
 * of WINE code operates on HANDLES as if they are UINTs. So to WINE
 * they exist as UINTs but to the Winelib user who turns on strict,
 * they exist as void*. If there is a size difference between UINT and
 * void* then things get ugly.  */
#ifdef STRICT
typedef VOID*           HANDLE;
#else
typedef UINT            HANDLE;
#endif


typedef HANDLE         *LPHANDLE;

/* Integer types. These are the same for emulator and library. */
typedef UINT            WPARAM;
typedef LONG            LPARAM;
typedef LONG            HRESULT;
typedef LONG            LRESULT;
typedef WORD            ATOM;
typedef WORD            CATCHBUF[9];
typedef WORD           *LPCATCHBUF;
typedef HANDLE          HHOOK;
typedef HANDLE          HMONITOR;
typedef DWORD           LCID;
typedef WORD            LANGID;
typedef DWORD           LCTYPE;
typedef float           FLOAT;

/* Pointers types. These are the same for emulator and library. */
/* winnt types */
typedef VOID           *PVOID;
typedef const void     *PCVOID;
typedef CHAR           *PCHAR;
typedef UCHAR          *PUCHAR;
typedef BYTE           *PBYTE;
typedef WORD           *PWORD;
typedef USHORT         *PUSHORT;
typedef SHORT          *PSHORT;
typedef ULONG          *PULONG;
typedef LONG           *PLONG;
typedef DWORD          *PDWORD;
/* common win32 types */
typedef CHAR           *LPSTR;
typedef CHAR           *PSTR;
typedef const CHAR     *LPCSTR;
typedef const CHAR     *PCSTR;
typedef WCHAR          *LPWSTR;
typedef WCHAR          *PWSTR;
typedef const WCHAR    *LPCWSTR;
typedef const WCHAR    *PCWSTR;
typedef BYTE           *LPBYTE;
typedef WORD           *LPWORD;
typedef DWORD          *LPDWORD;
typedef LONG           *LPLONG;
typedef VOID           *LPVOID;
typedef const VOID     *LPCVOID;
typedef INT            *PINT;
typedef INT            *LPINT;
typedef UINT           *PUINT;
typedef UINT           *LPUINT;
typedef FLOAT          *PFLOAT;
typedef FLOAT          *LPFLOAT;
typedef WIN_BOOL           *PWIN_BOOL;
typedef WIN_BOOL           *LPWIN_BOOL;

/* Special case: a segmented pointer is just a pointer in the user's code. */

#ifdef __WINE__
typedef DWORD SEGPTR;
#else
typedef void* SEGPTR;
#endif /* __WINE__ */

/* Handle types that exist both in Win16 and Win32. */

#ifdef STRICT
#define DECLARE_HANDLE(a) \
	typedef struct a##__ { int unused; } *a; \
	typedef a *P##a; \
	typedef a *LP##a
#else /*STRICT*/
#define DECLARE_HANDLE(a) \
	typedef HANDLE a; \
	typedef a *P##a; \
	typedef a *LP##a
#endif /*STRICT*/

DECLARE_HANDLE(HACMDRIVERID);
DECLARE_HANDLE(HACMDRIVER);
DECLARE_HANDLE(HACMOBJ);
DECLARE_HANDLE(HACMSTREAM);
DECLARE_HANDLE(HMETAFILEPICT);

DECLARE_HANDLE(HACCEL);
DECLARE_HANDLE(HBITMAP);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HCOLORSPACE);
DECLARE_HANDLE(HCURSOR);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HDROP);
DECLARE_HANDLE(HDRVR);
DECLARE_HANDLE(HDWP);
DECLARE_HANDLE(HENHMETAFILE);
DECLARE_HANDLE(HFILE);
DECLARE_HANDLE(HFONT);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HMETAFILE);
DECLARE_HANDLE(HMIDI);
DECLARE_HANDLE(HMIDIIN);
DECLARE_HANDLE(HMIDIOUT);
DECLARE_HANDLE(HMIDISTRM);
DECLARE_HANDLE(HMIXER);
DECLARE_HANDLE(HMIXEROBJ);
DECLARE_HANDLE(HMMIO);
DECLARE_HANDLE(HPALETTE);
DECLARE_HANDLE(HPEN);
DECLARE_HANDLE(HQUEUE);
DECLARE_HANDLE(HRGN);
DECLARE_HANDLE(HRSRC);
DECLARE_HANDLE(HTASK);
DECLARE_HANDLE(HWAVE);
DECLARE_HANDLE(HWAVEIN);
DECLARE_HANDLE(HWAVEOUT);
DECLARE_HANDLE(HWINSTA);
DECLARE_HANDLE(HDESK);
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HKL);
DECLARE_HANDLE(HIC);
DECLARE_HANDLE(HRASCONN);

/* Handle types that must remain interchangeable even with strict on */

typedef HINSTANCE HMODULE;
typedef HANDLE HGDIOBJ;
typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;
typedef HANDLE GLOBALHANDLE;
typedef HANDLE LOCALHANDLE;

/* Callback function pointers types */
//WIN_BOOL CALLBACK DATEFMT_ENUMPROCA(LPSTR);

typedef WIN_BOOL  CALLBACK  (* DATEFMT_ENUMPROCA)(LPSTR);
typedef WIN_BOOL    CALLBACK (* DATEFMT_ENUMPROCW)(LPWSTR);
DECL_WINELIB_TYPE_AW(DATEFMT_ENUMPROC)
typedef WIN_BOOL    CALLBACK (*DLGPROC)(HWND,UINT,WPARAM,LPARAM);
typedef LRESULT CALLBACK (*DRIVERPROC)(DWORD,HDRVR,UINT,LPARAM,LPARAM);
typedef INT     CALLBACK (*EDITWORDBREAKPROCA)(LPSTR,INT,INT,INT);
typedef INT     CALLBACK (*EDITWORDBREAKPROCW)(LPWSTR,INT,INT,INT);
DECL_WINELIB_TYPE_AW(EDITWORDBREAKPROC)
typedef LRESULT CALLBACK (*FARPROC)();
typedef INT     CALLBACK (*PROC)();
typedef WIN_BOOL    CALLBACK (*GRAYSTRINGPROC)(HDC,LPARAM,INT);
typedef LRESULT CALLBACK (*HOOKPROC)(INT,WPARAM,LPARAM);
typedef WIN_BOOL    CALLBACK (*PROPENUMPROCA)(HWND,LPCSTR,HANDLE);
typedef WIN_BOOL    CALLBACK (*PROPENUMPROCW)(HWND,LPCWSTR,HANDLE);
DECL_WINELIB_TYPE_AW(PROPENUMPROC)
typedef WIN_BOOL    CALLBACK (*PROPENUMPROCEXA)(HWND,LPCSTR,HANDLE,LPARAM);
typedef WIN_BOOL    CALLBACK (*PROPENUMPROCEXW)(HWND,LPCWSTR,HANDLE,LPARAM);
DECL_WINELIB_TYPE_AW(PROPENUMPROCEX)
typedef WIN_BOOL    CALLBACK (* TIMEFMT_ENUMPROCA)(LPSTR);
typedef WIN_BOOL    CALLBACK (* TIMEFMT_ENUMPROCW)(LPWSTR);
DECL_WINELIB_TYPE_AW(TIMEFMT_ENUMPROC)
typedef VOID    CALLBACK (*TIMERPROC)(HWND,UINT,UINT,DWORD);
typedef WIN_BOOL CALLBACK (*WNDENUMPROC)(HWND,LPARAM);
typedef LRESULT CALLBACK (*WNDPROC)(HWND,UINT,WPARAM,LPARAM);

/*----------------------------------------------------------------------------
** FIXME:  Better isolate Wine's reliance on the xxx16 type definitions.
**         For now, we just isolate them to make the situation clear.
**--------------------------------------------------------------------------*/
/*
 * Basic type definitions for 16 bit variations on Windows types.
 * These types are provided mostly to insure compatibility with
 * 16 bit windows code.
 */

#ifndef __WINE_WINDEF16_H
#define __WINE_WINDEF16_H

#include "windef.h"

/* Standard data types */

typedef short           INT16;
typedef unsigned short  UINT16;
typedef unsigned short  WIN_BOOL16;

typedef UINT16          HANDLE16;
typedef HANDLE16       *LPHANDLE16;

typedef UINT16          WPARAM16;
typedef INT16          *LPINT16;
typedef UINT16         *LPUINT16;

#define DECLARE_HANDLE16(a) \
	typedef HANDLE16 a##16; \
	typedef a##16 *P##a##16; \
	typedef a##16 *NP##a##16; \
	typedef a##16 *LP##a##16 

DECLARE_HANDLE16(HACMDRIVERID);
DECLARE_HANDLE16(HACMDRIVER);
DECLARE_HANDLE16(HACMOBJ);
DECLARE_HANDLE16(HACMSTREAM);
DECLARE_HANDLE16(HMETAFILEPICT);

DECLARE_HANDLE16(HACCEL);
DECLARE_HANDLE16(HBITMAP);
DECLARE_HANDLE16(HBRUSH);
DECLARE_HANDLE16(HCOLORSPACE);
DECLARE_HANDLE16(HCURSOR);
DECLARE_HANDLE16(HDC);
DECLARE_HANDLE16(HDROP);
DECLARE_HANDLE16(HDRVR);
DECLARE_HANDLE16(HDWP);
DECLARE_HANDLE16(HENHMETAFILE);
DECLARE_HANDLE16(HFILE);
DECLARE_HANDLE16(HFONT);
DECLARE_HANDLE16(HICON);
DECLARE_HANDLE16(HINSTANCE);
DECLARE_HANDLE16(HKEY);
DECLARE_HANDLE16(HMENU);
DECLARE_HANDLE16(HMETAFILE);
DECLARE_HANDLE16(HMIDI);
DECLARE_HANDLE16(HMIDIIN);
DECLARE_HANDLE16(HMIDIOUT);
DECLARE_HANDLE16(HMIDISTRM);
DECLARE_HANDLE16(HMIXER);
DECLARE_HANDLE16(HMIXEROBJ);
DECLARE_HANDLE16(HMMIO);
DECLARE_HANDLE16(HPALETTE);
DECLARE_HANDLE16(HPEN);
DECLARE_HANDLE16(HQUEUE);
DECLARE_HANDLE16(HRGN);
DECLARE_HANDLE16(HRSRC);
DECLARE_HANDLE16(HTASK);
DECLARE_HANDLE16(HWAVE);
DECLARE_HANDLE16(HWAVEIN);
DECLARE_HANDLE16(HWAVEOUT);
DECLARE_HANDLE16(HWINSTA);
DECLARE_HANDLE16(HDESK);
DECLARE_HANDLE16(HWND);
DECLARE_HANDLE16(HKL);
DECLARE_HANDLE16(HIC);
DECLARE_HANDLE16(HRASCONN);
#undef DECLARE_HANDLE16

typedef HINSTANCE16 HMODULE16;
typedef HANDLE16 HGDIOBJ16;
typedef HANDLE16 HGLOBAL16;
typedef HANDLE16 HLOCAL16;

/* The SIZE structure */
typedef struct
{
    INT16  cx;
    INT16  cy;
} SIZE16, *PSIZE16, *LPSIZE16;

/* The POINT structure */

typedef struct
{
    INT16  x;
    INT16  y;
} POINT16, *PPOINT16, *LPPOINT16;

/* The RECT structure */

typedef struct
{
    INT16  left;
    INT16  top;
    INT16  right;
    INT16  bottom;
} RECT16, *LPRECT16;

/* Callback function pointers types */

typedef LRESULT CALLBACK (*DRIVERPROC16)(DWORD,HDRVR16,UINT16,LPARAM,LPARAM);
typedef WIN_BOOL16  CALLBACK (*DLGPROC16)(HWND16,UINT16,WPARAM16,LPARAM);
typedef INT16   CALLBACK (*EDITWORDBREAKPROC16)(LPSTR,INT16,INT16,INT16);
typedef LRESULT CALLBACK (*FARPROC16)();
typedef INT16   CALLBACK (*PROC16)();
typedef WIN_BOOL16  CALLBACK (*GRAYSTRINGPROC16)(HDC16,LPARAM,INT16);
typedef LRESULT CALLBACK (*HOOKPROC16)(INT16,WPARAM16,LPARAM);
typedef WIN_BOOL16  CALLBACK (*PROPENUMPROC16)(HWND16,SEGPTR,HANDLE16);
typedef VOID    CALLBACK (*TIMERPROC16)(HWND16,UINT16,UINT16,DWORD);
typedef LRESULT CALLBACK (*WNDENUMPROC16)(HWND16,LPARAM);
typedef LRESULT CALLBACK (*WNDPROC16)(HWND16,UINT16,WPARAM16,LPARAM);

#endif /* __WINE_WINDEF16_H */

/* Define some empty macros for compatibility with Windows code. */

#ifndef __WINE__
#define NEAR
#define FAR
#define near
#define far
#define _near
#define _far
#define IN
#define OUT
#define OPTIONAL
#endif  /* __WINE__ */

/* Macro for structure packing. */

#ifdef __GNUC__
#ifndef _EGCS_
#define WINE_PACKED   __attribute__((packed))
#define WINE_UNUSED   __attribute__((unused))
#define WINE_NORETURN __attribute__((noreturn))
#endif
#else
#define WINE_PACKED    /* nothing */
#define WINE_UNUSED    /* nothing */
#define WINE_NORETURN  /* nothing */
#endif

/* Macros to split words and longs. */

#define LOBYTE(w)              ((BYTE)(WORD)(w))
#define HIBYTE(w)              ((BYTE)((WORD)(w) >> 8))

#define LOWORD(l)              ((WORD)(DWORD)(l))
#define HIWORD(l)              ((WORD)((DWORD)(l) >> 16))

#define SLOWORD(l)             ((INT16)(LONG)(l))
#define SHIWORD(l)             ((INT16)((LONG)(l) >> 16))

#define MAKEWORD(low,high)     ((WORD)(((BYTE)(low)) | ((WORD)((BYTE)(high))) << 8))
#define MAKELONG(low,high)     ((LONG)(((WORD)(low)) | (((DWORD)((WORD)(high))) << 16)))
#define MAKELPARAM(low,high)   ((LPARAM)MAKELONG(low,high))
#define MAKEWPARAM(low,high)   ((WPARAM)MAKELONG(low,high))
#define MAKELRESULT(low,high)  ((LRESULT)MAKELONG(low,high))
#define MAKEINTATOM(atom)      ((LPCSTR)MAKELONG((atom),0))

#define SELECTOROF(ptr)     (HIWORD(ptr))
#define OFFSETOF(ptr)       (LOWORD(ptr))

#ifdef __WINE__
/* macros to set parts of a DWORD (not in the Windows API) */
#define SET_LOWORD(dw,val)  ((dw) = ((dw) & 0xffff0000) | LOWORD(val))
#define SET_LOBYTE(dw,val)  ((dw) = ((dw) & 0xffffff00) | LOBYTE(val))
#define SET_HIBYTE(dw,val)  ((dw) = ((dw) & 0xffff00ff) | (LOWORD(val) & 0xff00))
#define ADD_LOWORD(dw,val)  ((dw) = ((dw) & 0xffff0000) | LOWORD((DWORD)(dw)+(val)))
#endif

/* Macros to access unaligned or wrong-endian WORDs and DWORDs. */
/* Note: These macros are semantically broken, at least for wrc.  wrc
   spits out data in the platform's current binary format, *not* in 
   little-endian format.  These macros are used throughout the resource
   code to load and store data to the resources.  Since it is unlikely 
   that we'll ever be dealing with little-endian resource data, the 
   byte-swapping nature of these macros has been disabled.  Rather than 
   remove the use of these macros from the resource loading code, the
   macros have simply been disabled.  In the future, someone may want 
   to reactivate these macros for other purposes.  In that case, the
   resource code will have to be modified to use different macros. */ 

#if 1
#define PUT_WORD(ptr,w)   (*(WORD *)(ptr) = (w))
#define GET_WORD(ptr)     (*(WORD *)(ptr))
#define PUT_DWORD(ptr,dw) (*(DWORD *)(ptr) = (dw))
#define GET_DWORD(ptr)    (*(DWORD *)(ptr))
#else
#define PUT_WORD(ptr,w)   (*(BYTE *)(ptr) = LOBYTE(w), \
                           *((BYTE *)(ptr) + 1) = HIBYTE(w))
#define GET_WORD(ptr)     ((WORD)(*(BYTE *)(ptr) | \
                                  (WORD)(*((BYTE *)(ptr)+1) << 8)))
#define PUT_DWORD(ptr,dw) (PUT_WORD((ptr),LOWORD(dw)), \
                           PUT_WORD((WORD *)(ptr)+1,HIWORD(dw)))
#define GET_DWORD(ptr)    ((DWORD)(GET_WORD(ptr) | \
                                   ((DWORD)GET_WORD((WORD *)(ptr)+1) << 16)))
#endif  /* 1 */

/* min and max macros */
#define __max(a,b) (((a) > (b)) ? (a) : (b))
#define __min(a,b) (((a) < (b)) ? (a) : (b))
#ifndef max
#define max(a,b)   (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)   (((a) < (b)) ? (a) : (b))
#endif

#ifndef _MAX_PATH
#define _MAX_PATH  260
#endif
#ifndef MAX_PATH
#define MAX_PATH   260
#endif
#ifndef _MAX_DRIVE
#define _MAX_DRIVE 3
#endif
#ifndef _MAX_DIR
#define _MAX_DIR   256
#endif
#ifndef _MAX_FNAME
#define _MAX_FNAME 255
#endif
#ifndef _MAX_EXT
#define _MAX_EXT   256
#endif

#define HFILE_ERROR16   ((HFILE16)-1)
#define HFILE_ERROR     ((HFILE)-1)

/* The SIZE structure */
typedef struct tagSIZE
{
    INT  cx;
    INT  cy;
} SIZE, *PSIZE, *LPSIZE;


typedef SIZE SIZEL, *PSIZEL, *LPSIZEL;

#define CONV_SIZE16TO32(s16,s32) \
            ((s32)->cx = (INT)(s16)->cx, (s32)->cy = (INT)(s16)->cy)
#define CONV_SIZE32TO16(s32,s16) \
            ((s16)->cx = (INT16)(s32)->cx, (s16)->cy = (INT16)(s32)->cy)

/* The POINT structure */
typedef struct tagPOINT
{
    LONG  x;
    LONG  y;
} POINT, *PPOINT, *LPPOINT;

typedef struct _POINTL
{
    LONG x;
    LONG y;
} POINTL;

#define CONV_POINT16TO32(p16,p32) \
            ((p32)->x = (INT)(p16)->x, (p32)->y = (INT)(p16)->y)
#define CONV_POINT32TO16(p32,p16) \
            ((p16)->x = (INT16)(p32)->x, (p16)->y = (INT16)(p32)->y)

#define MAKEPOINT16(l) (*((POINT16 *)&(l)))

/* The POINTS structure */

typedef struct tagPOINTS
{
	SHORT x;
	SHORT y;
} POINTS, *PPOINTS, *LPPOINTS;


#define MAKEPOINTS(l)  (*((POINTS *)&(l)))


/* The RECT structure */
typedef struct tagRECT
{
    short  left;
    short  top;
    short  right;
    short  bottom;
} RECT, *PRECT, *LPRECT;
typedef const RECT *LPCRECT;


typedef struct tagRECTL
{
    LONG left;
    LONG top;  
    LONG right;
    LONG bottom;
} RECTL, *PRECTL, *LPRECTL;

typedef const RECTL *LPCRECTL;

#define CONV_RECT16TO32(r16,r32) \
    ((r32)->left  = (INT)(r16)->left,  (r32)->top    = (INT)(r16)->top, \
     (r32)->right = (INT)(r16)->right, (r32)->bottom = (INT)(r16)->bottom)
#define CONV_RECT32TO16(r32,r16) \
    ((r16)->left  = (INT16)(r32)->left,  (r16)->top    = (INT16)(r32)->top, \
     (r16)->right = (INT16)(r32)->right, (r16)->bottom = (INT16)(r32)->bottom)

#ifdef __cplusplus
}
#endif

#endif /* __WINE_WINDEF_H */
