#ifndef __WINE_WINBASE_H
#define __WINE_WINBASE_H

#include "basetsd.h"
#include "winnt.h"
#include "winestring.h"
#include "pshpack1.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagCOORD {
    INT16 x;
    INT16 y;
} COORD, *LPCOORD;


  /* Windows Exit Procedure flag values */
#define	WEP_FREE_DLL        0
#define	WEP_SYSTEM_EXIT     1

typedef DWORD CALLBACK (*LPTHREAD_START_ROUTINE)(LPVOID);

#define EXCEPTION_DEBUG_EVENT       1
#define CREATE_THREAD_DEBUG_EVENT   2
#define CREATE_PROCESS_DEBUG_EVENT  3
#define EXIT_THREAD_DEBUG_EVENT     4
#define EXIT_PROCESS_DEBUG_EVENT    5
#define LOAD_DLL_DEBUG_EVENT        6
#define UNLOAD_DLL_DEBUG_EVENT      7
#define OUTPUT_DEBUG_STRING_EVENT   8
#define RIP_EVENT                   9

typedef struct _EXCEPTION_DEBUG_INFO {
    EXCEPTION_RECORD ExceptionRecord;
    DWORD dwFirstChance;
} EXCEPTION_DEBUG_INFO;

typedef struct _CREATE_THREAD_DEBUG_INFO {
    HANDLE hThread;
    LPVOID lpThreadLocalBase;
    LPTHREAD_START_ROUTINE lpStartAddress;
} CREATE_THREAD_DEBUG_INFO;

typedef struct _CREATE_PROCESS_DEBUG_INFO {
    HANDLE hFile;
    HANDLE hProcess;
    HANDLE hThread;
    LPVOID lpBaseOfImage;
    DWORD dwDebugInfoFileOffset;
    DWORD nDebugInfoSize;
    LPVOID lpThreadLocalBase;
    LPTHREAD_START_ROUTINE lpStartAddress;
    LPVOID lpImageName;
    WORD fUnicode;
} CREATE_PROCESS_DEBUG_INFO;

typedef struct _EXIT_THREAD_DEBUG_INFO {
    DWORD dwExitCode;
} EXIT_THREAD_DEBUG_INFO;

typedef struct _EXIT_PROCESS_DEBUG_INFO {
    DWORD dwExitCode;
} EXIT_PROCESS_DEBUG_INFO;

typedef struct _LOAD_DLL_DEBUG_INFO {
    HANDLE hFile;
    LPVOID   lpBaseOfDll;
    DWORD    dwDebugInfoFileOffset;
    DWORD    nDebugInfoSize;
    LPVOID   lpImageName;
    WORD     fUnicode;
} LOAD_DLL_DEBUG_INFO;

typedef struct _UNLOAD_DLL_DEBUG_INFO {
    LPVOID lpBaseOfDll;
} UNLOAD_DLL_DEBUG_INFO;

typedef struct _OUTPUT_DEBUG_STRING_INFO {
    LPSTR lpDebugStringData;
    WORD  fUnicode;
    WORD  nDebugStringLength;
} OUTPUT_DEBUG_STRING_INFO;

typedef struct _RIP_INFO {
    DWORD dwError;
    DWORD dwType;
} RIP_INFO;

typedef struct _DEBUG_EVENT {
    DWORD dwDebugEventCode;
    DWORD dwProcessId;
    DWORD dwThreadId;
    union {
        EXCEPTION_DEBUG_INFO      Exception;
        CREATE_THREAD_DEBUG_INFO  CreateThread;
        CREATE_PROCESS_DEBUG_INFO CreateProcessInfo;
        EXIT_THREAD_DEBUG_INFO    ExitThread;
        EXIT_PROCESS_DEBUG_INFO   ExitProcess;
        LOAD_DLL_DEBUG_INFO       LoadDll;
        UNLOAD_DLL_DEBUG_INFO     UnloadDll;
        OUTPUT_DEBUG_STRING_INFO  DebugString;
        RIP_INFO                  RipInfo;
    } u;
} DEBUG_EVENT, *LPDEBUG_EVENT;

#define OFS_MAXPATHNAME 128
typedef struct
{
    BYTE cBytes;
    BYTE fFixedDisk;
    WORD nErrCode;
    BYTE reserved[4];
    BYTE szPathName[OFS_MAXPATHNAME];
} OFSTRUCT, *LPOFSTRUCT;

#define OF_READ               0x0000
#define OF_WRITE              0x0001
#define OF_READWRITE          0x0002
#define OF_SHARE_COMPAT       0x0000
#define OF_SHARE_EXCLUSIVE    0x0010
#define OF_SHARE_DENY_WRITE   0x0020
#define OF_SHARE_DENY_READ    0x0030
#define OF_SHARE_DENY_NONE    0x0040
#define OF_PARSE              0x0100
#define OF_DELETE             0x0200
#define OF_VERIFY             0x0400   /* Used with OF_REOPEN */
#define OF_SEARCH             0x0400   /* Used without OF_REOPEN */
#define OF_CANCEL             0x0800
#define OF_CREATE             0x1000
#define OF_PROMPT             0x2000
#define OF_EXIST              0x4000
#define OF_REOPEN             0x8000

/* SetErrorMode values */
#define SEM_FAILCRITICALERRORS      0x0001
#define SEM_NOGPFAULTERRORBOX       0x0002
#define SEM_NOALIGNMENTFAULTEXCEPT  0x0004
#define SEM_NOOPENFILEERRORBOX      0x8000

/* CopyFileEx flags */
#define COPY_FILE_FAIL_IF_EXISTS        0x00000001
#define COPY_FILE_RESTARTABLE           0x00000002
#define COPY_FILE_OPEN_SOURCE_FOR_WRITE 0x00000004

/* GetTempFileName() Flags */
#define TF_FORCEDRIVE	        0x80

#define DRIVE_CANNOTDETERMINE      0
#define DRIVE_DOESNOTEXIST         1
#define DRIVE_REMOVABLE            2
#define DRIVE_FIXED                3
#define DRIVE_REMOTE               4
/* Win32 additions */
#define DRIVE_CDROM                5
#define DRIVE_RAMDISK              6

/* The security attributes structure */
typedef struct _SECURITY_ATTRIBUTES
{
    DWORD   nLength;
    LPVOID  lpSecurityDescriptor;
    WIN_BOOL  bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

#ifndef _FILETIME_
#define _FILETIME_
/* 64 bit number of 100 nanoseconds intervals since January 1, 1601 */
typedef struct
{
  DWORD  dwLowDateTime;
  DWORD  dwHighDateTime;
} FILETIME, *LPFILETIME;
#endif /* _FILETIME_ */

/* Find* structures */
typedef struct
{
    DWORD     dwFileAttributes;
    FILETIME  ftCreationTime;
    FILETIME  ftLastAccessTime;
    FILETIME  ftLastWriteTime;
    DWORD     nFileSizeHigh;
    DWORD     nFileSizeLow;
    DWORD     dwReserved0;
    DWORD     dwReserved1;
    CHAR      cFileName[260];
    CHAR      cAlternateFileName[14];
} WIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

typedef struct
{
    DWORD     dwFileAttributes;
    FILETIME  ftCreationTime;
    FILETIME  ftLastAccessTime;
    FILETIME  ftLastWriteTime;
    DWORD     nFileSizeHigh;
    DWORD     nFileSizeLow;
    DWORD     dwReserved0;
    DWORD     dwReserved1;
    WCHAR     cFileName[260];
    WCHAR     cAlternateFileName[14];
} WIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

DECL_WINELIB_TYPE_AW(WIN32_FIND_DATA)
DECL_WINELIB_TYPE_AW(LPWIN32_FIND_DATA)

typedef struct
{
    LPVOID lpData;
    DWORD cbData;
    BYTE cbOverhead;
    BYTE iRegionIndex;
    WORD wFlags;
    union {
        struct {
            HANDLE hMem;
            DWORD dwReserved[3];
        } Block;
        struct {
            DWORD dwCommittedSize;
            DWORD dwUnCommittedSize;
            LPVOID lpFirstBlock;
            LPVOID lpLastBlock;
        } Region;
    } Foo;
} PROCESS_HEAP_ENTRY, *LPPROCESS_HEAP_ENTRY;

#define PROCESS_HEAP_REGION                   0x0001
#define PROCESS_HEAP_UNCOMMITTED_RANGE        0x0002
#define PROCESS_HEAP_ENTRY_BUSY               0x0004
#define PROCESS_HEAP_ENTRY_MOVEABLE           0x0010
#define PROCESS_HEAP_ENTRY_DDESHARE           0x0020

#define INVALID_HANDLE_VALUE16  ((HANDLE16) -1)
#define INVALID_HANDLE_VALUE  ((HANDLE) -1)

#define TLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFF)

/* comm */

#define CBR_110	0xFF10
#define CBR_300	0xFF11
#define CBR_600	0xFF12
#define CBR_1200	0xFF13
#define CBR_2400	0xFF14
#define CBR_4800	0xFF15
#define CBR_9600	0xFF16
#define CBR_14400	0xFF17
#define CBR_19200	0xFF18
#define CBR_38400	0xFF1B
#define CBR_56000	0xFF1F
#define CBR_128000	0xFF23
#define CBR_256000	0xFF27

#define NOPARITY	0
#define ODDPARITY	1
#define EVENPARITY	2
#define MARKPARITY	3
#define SPACEPARITY	4
#define ONESTOPBIT	0
#define ONE5STOPBITS	1
#define TWOSTOPBITS	2

#define IGNORE		0
#define INFINITE16      0xFFFF
#define INFINITE      0xFFFFFFFF

#define CE_RXOVER	0x0001
#define CE_OVERRUN	0x0002
#define CE_RXPARITY	0x0004
#define CE_FRAME	0x0008
#define CE_BREAK	0x0010
#define CE_CTSTO	0x0020
#define CE_DSRTO	0x0040
#define CE_RLSDTO	0x0080
#define CE_TXFULL	0x0100
#define CE_PTO		0x0200
#define CE_IOE		0x0400
#define CE_DNS		0x0800
#define CE_OOP		0x1000
#define CE_MODE	0x8000

#define IE_BADID	-1
#define IE_OPEN	-2
#define IE_NOPEN	-3
#define IE_MEMORY	-4
#define IE_DEFAULT	-5
#define IE_HARDWARE	-10
#define IE_BYTESIZE	-11
#define IE_BAUDRATE	-12

#define EV_RXCHAR	0x0001
#define EV_RXFLAG	0x0002
#define EV_TXEMPTY	0x0004
#define EV_CTS		0x0008
#define EV_DSR		0x0010
#define EV_RLSD	0x0020
#define EV_BREAK	0x0040
#define EV_ERR		0x0080
#define EV_RING	0x0100
#define EV_PERR	0x0200
#define EV_CTSS	0x0400
#define EV_DSRS	0x0800
#define EV_RLSDS	0x1000
#define EV_RINGTE	0x2000
#define EV_RingTe	EV_RINGTE

#define SETXOFF	1
#define SETXON		2
#define SETRTS		3
#define CLRRTS		4
#define SETDTR		5
#define CLRDTR		6
#define RESETDEV	7
#define SETBREAK	8
#define CLRBREAK	9

#define GETBASEIRQ	10

/* Purge functions for Comm Port */
#define PURGE_TXABORT       0x0001  /* Kill the pending/current writes to the 
				       comm port */
#define PURGE_RXABORT       0x0002  /*Kill the pending/current reads to 
				     the comm port */
#define PURGE_TXCLEAR       0x0004  /* Kill the transmit queue if there*/
#define PURGE_RXCLEAR       0x0008  /* Kill the typeahead buffer if there*/


/* Modem Status Flags */
#define MS_CTS_ON           ((DWORD)0x0010)
#define MS_DSR_ON           ((DWORD)0x0020)
#define MS_RING_ON          ((DWORD)0x0040)
#define MS_RLSD_ON          ((DWORD)0x0080)

#define	RTS_CONTROL_DISABLE	0
#define	RTS_CONTROL_ENABLE	1
#define	RTS_CONTROL_HANDSHAKE	2
#define	RTS_CONTROL_TOGGLE	3

#define	DTR_CONTROL_DISABLE	0
#define	DTR_CONTROL_ENABLE	1
#define	DTR_CONTROL_HANDSHAKE	2

#define CSTF_CTSHOLD	0x01
#define CSTF_DSRHOLD	0x02
#define CSTF_RLSDHOLD	0x04
#define CSTF_XOFFHOLD	0x08
#define CSTF_XOFFSENT	0x10
#define CSTF_EOF	0x20
#define CSTF_TXIM	0x40

#define MAKEINTRESOURCEA(i) (LPSTR)((DWORD)((WORD)(i)))
#define MAKEINTRESOURCEW(i) (LPWSTR)((DWORD)((WORD)(i)))
#define MAKEINTRESOURCE WINELIB_NAME_AW(MAKEINTRESOURCE)

/* Predefined resource types */
#define RT_CURSORA         MAKEINTRESOURCEA(1)
#define RT_CURSORW         MAKEINTRESOURCEW(1)
#define RT_CURSOR            WINELIB_NAME_AW(RT_CURSOR)
#define RT_BITMAPA         MAKEINTRESOURCEA(2)
#define RT_BITMAPW         MAKEINTRESOURCEW(2)
#define RT_BITMAP            WINELIB_NAME_AW(RT_BITMAP)
#define RT_ICONA           MAKEINTRESOURCEA(3)
#define RT_ICONW           MAKEINTRESOURCEW(3)
#define RT_ICON              WINELIB_NAME_AW(RT_ICON)
#define RT_MENUA           MAKEINTRESOURCEA(4)
#define RT_MENUW           MAKEINTRESOURCEW(4)
#define RT_MENU              WINELIB_NAME_AW(RT_MENU)
#define RT_DIALOGA         MAKEINTRESOURCEA(5)
#define RT_DIALOGW         MAKEINTRESOURCEW(5)
#define RT_DIALOG            WINELIB_NAME_AW(RT_DIALOG)
#define RT_STRINGA         MAKEINTRESOURCEA(6)
#define RT_STRINGW         MAKEINTRESOURCEW(6)
#define RT_STRING            WINELIB_NAME_AW(RT_STRING)
#define RT_FONTDIRA        MAKEINTRESOURCEA(7)
#define RT_FONTDIRW        MAKEINTRESOURCEW(7)
#define RT_FONTDIR           WINELIB_NAME_AW(RT_FONTDIR)
#define RT_FONTA           MAKEINTRESOURCEA(8)
#define RT_FONTW           MAKEINTRESOURCEW(8)
#define RT_FONT              WINELIB_NAME_AW(RT_FONT)
#define RT_ACCELERATORA    MAKEINTRESOURCEA(9)
#define RT_ACCELERATORW    MAKEINTRESOURCEW(9)
#define RT_ACCELERATOR       WINELIB_NAME_AW(RT_ACCELERATOR)
#define RT_RCDATAA         MAKEINTRESOURCEA(10)
#define RT_RCDATAW         MAKEINTRESOURCEW(10)
#define RT_RCDATA            WINELIB_NAME_AW(RT_RCDATA)
#define RT_MESSAGELISTA    MAKEINTRESOURCEA(11)
#define RT_MESSAGELISTW    MAKEINTRESOURCEW(11)
#define RT_MESSAGELIST       WINELIB_NAME_AW(RT_MESSAGELIST)
#define RT_GROUP_CURSORA   MAKEINTRESOURCEA(12)
#define RT_GROUP_CURSORW   MAKEINTRESOURCEW(12)
#define RT_GROUP_CURSOR      WINELIB_NAME_AW(RT_GROUP_CURSOR)
#define RT_GROUP_ICONA     MAKEINTRESOURCEA(14)
#define RT_GROUP_ICONW     MAKEINTRESOURCEW(14)
#define RT_GROUP_ICON        WINELIB_NAME_AW(RT_GROUP_ICON)


#define LMEM_FIXED          0   
#define LMEM_MOVEABLE       0x0002
#define LMEM_NOCOMPACT      0x0010
#define LMEM_NODISCARD      0x0020
#define LMEM_ZEROINIT       0x0040
#define LMEM_MODIFY         0x0080
#define LMEM_DISCARDABLE    0x0F00
#define LMEM_DISCARDED	    0x4000
#define LMEM_LOCKCOUNT	    0x00FF

#define LPTR (LMEM_FIXED | LMEM_ZEROINIT)

#define GMEM_FIXED          0x0000
#define GMEM_MOVEABLE       0x0002
#define GMEM_NOCOMPACT      0x0010
#define GMEM_NODISCARD      0x0020
#define GMEM_ZEROINIT       0x0040
#define GMEM_MODIFY         0x0080
#define GMEM_DISCARDABLE    0x0100
#define GMEM_NOT_BANKED     0x1000
#define GMEM_SHARE          0x2000
#define GMEM_DDESHARE       0x2000
#define GMEM_NOTIFY         0x4000
#define GMEM_LOWER          GMEM_NOT_BANKED
#define GMEM_DISCARDED      0x4000
#define GMEM_LOCKCOUNT      0x00ff
#define GMEM_INVALID_HANDLE 0x8000

#define GHND                (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GPTR                (GMEM_FIXED | GMEM_ZEROINIT)


typedef struct tagMEMORYSTATUS
{
    DWORD    dwLength;
    DWORD    dwMemoryLoad;
    DWORD    dwTotalPhys;
    DWORD    dwAvailPhys;
    DWORD    dwTotalPageFile;
    DWORD    dwAvailPageFile;
    DWORD    dwTotalVirtual;
    DWORD    dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;


#ifndef NOLOGERROR

/* LogParamError and LogError values */

/* Error modifier bits */
#define ERR_WARNING             0x8000
#define ERR_PARAM               0x4000

#define ERR_SIZE_MASK           0x3000
#define ERR_BYTE                0x1000
#define ERR_WORD                0x2000
#define ERR_DWORD               0x3000

/* LogParamError() values */

/* Generic parameter values */
#define ERR_BAD_VALUE           0x6001
#define ERR_BAD_FLAGS           0x6002
#define ERR_BAD_INDEX           0x6003
#define ERR_BAD_DVALUE          0x7004
#define ERR_BAD_DFLAGS          0x7005
#define ERR_BAD_DINDEX          0x7006
#define ERR_BAD_PTR             0x7007
#define ERR_BAD_FUNC_PTR        0x7008
#define ERR_BAD_SELECTOR        0x6009
#define ERR_BAD_STRING_PTR      0x700a
#define ERR_BAD_HANDLE          0x600b

/* KERNEL parameter errors */
#define ERR_BAD_HINSTANCE       0x6020
#define ERR_BAD_HMODULE         0x6021
#define ERR_BAD_GLOBAL_HANDLE   0x6022
#define ERR_BAD_LOCAL_HANDLE    0x6023
#define ERR_BAD_ATOM            0x6024
#define ERR_BAD_HFILE           0x6025

/* USER parameter errors */
#define ERR_BAD_HWND            0x6040
#define ERR_BAD_HMENU           0x6041
#define ERR_BAD_HCURSOR         0x6042
#define ERR_BAD_HICON           0x6043
#define ERR_BAD_HDWP            0x6044
#define ERR_BAD_CID             0x6045
#define ERR_BAD_HDRVR           0x6046

/* GDI parameter errors */
#define ERR_BAD_COORDS          0x7060
#define ERR_BAD_GDI_OBJECT      0x6061
#define ERR_BAD_HDC             0x6062
#define ERR_BAD_HPEN            0x6063
#define ERR_BAD_HFONT           0x6064
#define ERR_BAD_HBRUSH          0x6065
#define ERR_BAD_HBITMAP         0x6066
#define ERR_BAD_HRGN            0x6067
#define ERR_BAD_HPALETTE        0x6068
#define ERR_BAD_HMETAFILE       0x6069


/* LogError() values */

/* KERNEL errors */
#define ERR_GALLOC              0x0001
#define ERR_GREALLOC            0x0002
#define ERR_GLOCK               0x0003
#define ERR_LALLOC              0x0004
#define ERR_LREALLOC            0x0005
#define ERR_LLOCK               0x0006
#define ERR_ALLOCRES            0x0007
#define ERR_LOCKRES             0x0008
#define ERR_LOADMODULE          0x0009

/* USER errors */
#define ERR_CREATEDLG           0x0040
#define ERR_CREATEDLG2          0x0041
#define ERR_REGISTERCLASS       0x0042
#define ERR_DCBUSY              0x0043
#define ERR_CREATEWND           0x0044
#define ERR_STRUCEXTRA          0x0045
#define ERR_LOADSTR             0x0046
#define ERR_LOADMENU            0x0047
#define ERR_NESTEDBEGINPAINT    0x0048
#define ERR_BADINDEX            0x0049
#define ERR_CREATEMENU          0x004a

/* GDI errors */
#define ERR_CREATEDC            0x0080
#define ERR_CREATEMETA          0x0081
#define ERR_DELOBJSELECTED      0x0082
#define ERR_SELBITMAP           0x0083



/* Debugging support (DEBUG SYSTEM ONLY) */
typedef struct
{
    UINT16  flags;
    DWORD   dwOptions WINE_PACKED;
    DWORD   dwFilter WINE_PACKED;
    CHAR    achAllocModule[8] WINE_PACKED;
    DWORD   dwAllocBreak WINE_PACKED;
    DWORD   dwAllocCount WINE_PACKED;
} WINDEBUGINFO, *LPWINDEBUGINFO;

/* WINDEBUGINFO flags values */
#define WDI_OPTIONS         0x0001
#define WDI_FILTER          0x0002
#define WDI_ALLOCBREAK      0x0004

/* dwOptions values */
#define DBO_CHECKHEAP       0x0001
#define DBO_BUFFERFILL      0x0004
#define DBO_DISABLEGPTRAPPING 0x0010
#define DBO_CHECKFREE       0x0020

#define DBO_SILENT          0x8000

#define DBO_TRACEBREAK      0x2000
#define DBO_WARNINGBREAK    0x1000
#define DBO_NOERRORBREAK    0x0800
#define DBO_NOFATALBREAK    0x0400
#define DBO_INT3BREAK       0x0100

/* DebugOutput flags values */
#define DBF_TRACE           0x0000
#define DBF_WARNING         0x4000
#define DBF_ERROR           0x8000
#define DBF_FATAL           0xc000

/* dwFilter values */
#define DBF_KERNEL          0x1000
#define DBF_KRN_MEMMAN      0x0001
#define DBF_KRN_LOADMODULE  0x0002
#define DBF_KRN_SEGMENTLOAD 0x0004
#define DBF_USER            0x0800
#define DBF_GDI             0x0400
#define DBF_MMSYSTEM        0x0040
#define DBF_PENWIN          0x0020
#define DBF_APPLICATION     0x0008
#define DBF_DRIVER          0x0010

#endif /* NOLOGERROR */

typedef struct {
        WORD wYear;
        WORD wMonth;
        WORD wDayOfWeek;
        WORD wDay;
        WORD wHour;
        WORD wMinute;
        WORD wSecond;
        WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

/* The 'overlapped' data structure used by async I/O functions.
 */
typedef struct {
        DWORD Internal;
        DWORD InternalHigh;
        DWORD Offset;
        DWORD OffsetHigh;
        HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

/* Process startup information.
 */

/* STARTUPINFO.dwFlags */
#define	STARTF_USESHOWWINDOW	0x00000001
#define	STARTF_USESIZE		0x00000002
#define	STARTF_USEPOSITION	0x00000004
#define	STARTF_USECOUNTCHARS	0x00000008
#define	STARTF_USEFILLATTRIBUTE	0x00000010
#define	STARTF_RUNFULLSCREEN	0x00000020
#define	STARTF_FORCEONFEEDBACK	0x00000040
#define	STARTF_FORCEOFFFEEDBACK	0x00000080
#define	STARTF_USESTDHANDLES	0x00000100
#define	STARTF_USEHOTKEY	0x00000200

typedef struct {
        DWORD cb;		/* 00: size of struct */
        LPSTR lpReserved;	/* 04: */
        LPSTR lpDesktop;	/* 08: */
        LPSTR lpTitle;		/* 0c: */
        DWORD dwX;		/* 10: */
        DWORD dwY;		/* 14: */
        DWORD dwXSize;		/* 18: */
        DWORD dwYSize;		/* 1c: */
        DWORD dwXCountChars;	/* 20: */
        DWORD dwYCountChars;	/* 24: */
        DWORD dwFillAttribute;	/* 28: */
        DWORD dwFlags;		/* 2c: */
        WORD wShowWindow;	/* 30: */
        WORD cbReserved2;	/* 32: */
        BYTE *lpReserved2;	/* 34: */
        HANDLE hStdInput;	/* 38: */
        HANDLE hStdOutput;	/* 3c: */
        HANDLE hStdError;	/* 40: */
} STARTUPINFOA, *LPSTARTUPINFOA;

typedef struct {
        DWORD cb;
        LPWSTR lpReserved;
        LPWSTR lpDesktop;
        LPWSTR lpTitle;
        DWORD dwX;
        DWORD dwY;
        DWORD dwXSize;
        DWORD dwYSize;
        DWORD dwXCountChars;
        DWORD dwYCountChars;
        DWORD dwFillAttribute;
        DWORD dwFlags;
        WORD wShowWindow;
        WORD cbReserved2;
        BYTE *lpReserved2;
        HANDLE hStdInput;
        HANDLE hStdOutput;
        HANDLE hStdError;
} STARTUPINFOW, *LPSTARTUPINFOW;

DECL_WINELIB_TYPE_AW(STARTUPINFO)
DECL_WINELIB_TYPE_AW(LPSTARTUPINFO)

typedef struct {
	HANDLE	hProcess;
	HANDLE	hThread;
	DWORD		dwProcessId;
	DWORD		dwThreadId;
} PROCESS_INFORMATION,*LPPROCESS_INFORMATION;

typedef struct {
        LONG Bias;
        WCHAR StandardName[32];
        SYSTEMTIME StandardDate;
        LONG StandardBias;
        WCHAR DaylightName[32];
        SYSTEMTIME DaylightDate;
        LONG DaylightBias;
} TIME_ZONE_INFORMATION, *LPTIME_ZONE_INFORMATION;

#define TIME_ZONE_ID_UNKNOWN    0
#define TIME_ZONE_ID_STANDARD   1
#define TIME_ZONE_ID_DAYLIGHT   2

/* CreateProcess: dwCreationFlag values
 */
#define DEBUG_PROCESS               0x00000001
#define DEBUG_ONLY_THIS_PROCESS     0x00000002
#define CREATE_SUSPENDED            0x00000004
#define DETACHED_PROCESS            0x00000008
#define CREATE_NEW_CONSOLE          0x00000010
#define NORMAL_PRIORITY_CLASS       0x00000020
#define IDLE_PRIORITY_CLASS         0x00000040
#define HIGH_PRIORITY_CLASS         0x00000080
#define REALTIME_PRIORITY_CLASS     0x00000100
#define CREATE_NEW_PROCESS_GROUP    0x00000200
#define CREATE_UNICODE_ENVIRONMENT  0x00000400
#define CREATE_SEPARATE_WOW_VDM     0x00000800
#define CREATE_SHARED_WOW_VDM       0x00001000
#define CREATE_DEFAULT_ERROR_MODE   0x04000000
#define CREATE_NO_WINDOW            0x08000000
#define PROFILE_USER                0x10000000
#define PROFILE_KERNEL              0x20000000
#define PROFILE_SERVER              0x40000000


/* File object type definitions
 */
#define FILE_TYPE_UNKNOWN       0
#define FILE_TYPE_DISK          1
#define FILE_TYPE_CHAR          2
#define FILE_TYPE_PIPE          3
#define FILE_TYPE_REMOTE        32768

/* File creation flags
 */
#define FILE_FLAG_WRITE_THROUGH    0x80000000UL
#define FILE_FLAG_OVERLAPPED 	   0x40000000L
#define FILE_FLAG_NO_BUFFERING     0x20000000L
#define FILE_FLAG_RANDOM_ACCESS    0x10000000L
#define FILE_FLAG_SEQUENTIAL_SCAN  0x08000000L
#define FILE_FLAG_DELETE_ON_CLOSE  0x04000000L
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000L
#define FILE_FLAG_POSIX_SEMANTICS  0x01000000L
#define CREATE_NEW              1
#define CREATE_ALWAYS           2
#define OPEN_EXISTING           3
#define OPEN_ALWAYS             4
#define TRUNCATE_EXISTING       5

/* Standard handle identifiers
 */
#define STD_INPUT_HANDLE        ((DWORD) -10)
#define STD_OUTPUT_HANDLE       ((DWORD) -11)
#define STD_ERROR_HANDLE        ((DWORD) -12)

typedef struct
{
  int dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  int dwVolumeSerialNumber;
  int nFileSizeHigh;
  int nFileSizeLow;
  int nNumberOfLinks;
  int nFileIndexHigh;
  int nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION ;


typedef struct _SYSTEM_POWER_STATUS
{
  WIN_BOOL16  ACLineStatus;
  BYTE    BatteryFlag;
  BYTE    BatteryLifePercent;
  BYTE    reserved;
  DWORD   BatteryLifeTime;
  DWORD   BatteryFullLifeTime;
} SYSTEM_POWER_STATUS, *LPSYSTEM_POWER_STATUS;

typedef struct _MEMORY_BASIC_INFORMATION
{
    LPVOID   BaseAddress;
    LPVOID   AllocationBase;
    DWORD    AllocationProtect;
    DWORD    RegionSize;
    DWORD    State;
    DWORD    Protect;
    DWORD    Type;
} MEMORY_BASIC_INFORMATION,*LPMEMORY_BASIC_INFORMATION;


typedef WIN_BOOL CALLBACK (*CODEPAGE_ENUMPROCA)(LPSTR);
typedef WIN_BOOL CALLBACK (*CODEPAGE_ENUMPROCW)(LPWSTR);
DECL_WINELIB_TYPE_AW(CODEPAGE_ENUMPROC)
typedef WIN_BOOL CALLBACK (*LOCALE_ENUMPROCA)(LPSTR);
typedef WIN_BOOL CALLBACK (*LOCALE_ENUMPROCW)(LPWSTR);
DECL_WINELIB_TYPE_AW(LOCALE_ENUMPROC)

typedef struct tagSYSTEM_INFO
{
    union {
	DWORD	dwOemId; /* Obsolete field - do not use */
	struct {
		WORD wProcessorArchitecture;
		WORD wReserved;
	} DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
    DWORD	dwPageSize;
    LPVOID	lpMinimumApplicationAddress;
    LPVOID	lpMaximumApplicationAddress;
    DWORD	dwActiveProcessorMask;
    DWORD	dwNumberOfProcessors;
    DWORD	dwProcessorType;
    DWORD	dwAllocationGranularity;
    WORD	wProcessorLevel;
    WORD	wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

/* {G,S}etPriorityClass */
#define	NORMAL_PRIORITY_CLASS	0x00000020
#define	IDLE_PRIORITY_CLASS	0x00000040
#define	HIGH_PRIORITY_CLASS	0x00000080
#define	REALTIME_PRIORITY_CLASS	0x00000100

typedef WIN_BOOL CALLBACK (*ENUMRESTYPEPROCA)(HMODULE,LPSTR,LONG);
typedef WIN_BOOL CALLBACK (*ENUMRESTYPEPROCW)(HMODULE,LPWSTR,LONG);
typedef WIN_BOOL CALLBACK (*ENUMRESNAMEPROCA)(HMODULE,LPCSTR,LPSTR,LONG);
typedef WIN_BOOL CALLBACK (*ENUMRESNAMEPROCW)(HMODULE,LPCWSTR,LPWSTR,LONG);
typedef WIN_BOOL CALLBACK (*ENUMRESLANGPROCA)(HMODULE,LPCSTR,LPCSTR,WORD,LONG);
typedef WIN_BOOL CALLBACK (*ENUMRESLANGPROCW)(HMODULE,LPCWSTR,LPCWSTR,WORD,LONG);

DECL_WINELIB_TYPE_AW(ENUMRESTYPEPROC)
DECL_WINELIB_TYPE_AW(ENUMRESNAMEPROC)
DECL_WINELIB_TYPE_AW(ENUMRESLANGPROC)

/* flags that can be passed to LoadLibraryEx */
#define	DONT_RESOLVE_DLL_REFERENCES	0x00000001
#define	LOAD_LIBRARY_AS_DATAFILE	0x00000002
#define	LOAD_WITH_ALTERED_SEARCH_PATH	0x00000008

/* ifdef _x86_ ... */
typedef struct _LDT_ENTRY {
    WORD	LimitLow;
    WORD	BaseLow;
    union {
	struct {
	    BYTE	BaseMid;
	    BYTE	Flags1;/*Declare as bytes to avoid alignment problems */
	    BYTE	Flags2; 
	    BYTE	BaseHi;
	} Bytes;
	struct {	    
	    unsigned	BaseMid		: 8;
	    unsigned	Type		: 5;
	    unsigned	Dpl		: 2;
	    unsigned	Pres		: 1;
	    unsigned	LimitHi		: 4;
	    unsigned	Sys		: 1;
	    unsigned	Reserved_0	: 1;
	    unsigned	Default_Big	: 1;
	    unsigned	Granularity	: 1;
	    unsigned	BaseHi		: 8;
	} Bits;
    } HighWord;
} LDT_ENTRY, *LPLDT_ENTRY;


typedef enum _GET_FILEEX_INFO_LEVELS {
    GetFileExInfoStandard
} GET_FILEEX_INFO_LEVELS;

typedef struct _WIN32_FILE_ATTRIBUTES_DATA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA, *LPWIN32_FILE_ATTRIBUTE_DATA;

typedef struct _DllVersionInfo {
    DWORD cbSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformID;
} DLLVERSIONINFO;

/*
 * This one seems to be a Win32 only definition. It also is defined with
 * WINAPI instead of CALLBACK in the windows headers.
 */
typedef DWORD WINAPI (*LPPROGRESS_ROUTINE)(LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER, 
                                           LARGE_INTEGER, DWORD, DWORD, HANDLE,
                                           HANDLE, LPVOID);


#define WAIT_FAILED		0xffffffff
#define WAIT_OBJECT_0		0
#define WAIT_ABANDONED		STATUS_ABANDONED_WAIT_0
#define WAIT_ABANDONED_0	STATUS_ABANDONED_WAIT_0
#define WAIT_IO_COMPLETION	STATUS_USER_APC
#define WAIT_TIMEOUT		STATUS_TIMEOUT
#define STILL_ACTIVE            STATUS_PENDING

#define	PAGE_NOACCESS		0x01
#define	PAGE_READONLY		0x02
#define	PAGE_READWRITE		0x04
#define	PAGE_WRITECOPY		0x08
#define	PAGE_EXECUTE		0x10
#define	PAGE_EXECUTE_READ	0x20
#define	PAGE_EXECUTE_READWRITE	0x40
#define	PAGE_EXECUTE_WRITECOPY	0x80
#define	PAGE_GUARD		0x100
#define	PAGE_NOCACHE		0x200

#define MEM_COMMIT              0x00001000
#define MEM_RESERVE             0x00002000
#define MEM_DECOMMIT            0x00004000
#define MEM_RELEASE             0x00008000
#define MEM_FREE                0x00010000
#define MEM_PRIVATE             0x00020000
#define MEM_MAPPED              0x00040000
#define MEM_TOP_DOWN            0x00100000
#ifdef __WINE__
#define MEM_SYSTEM              0x80000000
#endif

#define SEC_FILE                0x00800000
#define SEC_IMAGE               0x01000000
#define SEC_RESERVE             0x04000000
#define SEC_COMMIT              0x08000000
#define SEC_NOCACHE             0x10000000

#define FILE_BEGIN              0
#define FILE_CURRENT            1
#define FILE_END                2

#define FILE_CASE_SENSITIVE_SEARCH      0x00000001
#define FILE_CASE_PRESERVED_NAMES       0x00000002
#define FILE_UNICODE_ON_DISK            0x00000004
#define FILE_PERSISTENT_ACLS            0x00000008

#define FILE_MAP_COPY                   0x00000001
#define FILE_MAP_WRITE                  0x00000002
#define FILE_MAP_READ                   0x00000004
#define FILE_MAP_ALL_ACCESS             0x000f001f

#define MOVEFILE_REPLACE_EXISTING       0x00000001
#define MOVEFILE_COPY_ALLOWED           0x00000002
#define MOVEFILE_DELAY_UNTIL_REBOOT     0x00000004

#define FS_CASE_SENSITIVE               FILE_CASE_SENSITIVE_SEARCH
#define FS_CASE_IS_PRESERVED            FILE_CASE_PRESERVED_NAMES
#define FS_UNICODE_STORED_ON_DISK       FILE_UNICODE_ON_DISK

#define EXCEPTION_ACCESS_VIOLATION          STATUS_ACCESS_VIOLATION
#define EXCEPTION_DATATYPE_MISALIGNMENT     STATUS_DATATYPE_MISALIGNMENT
#define EXCEPTION_BREAKPOINT                STATUS_BREAKPOINT
#define EXCEPTION_SINGLE_STEP               STATUS_SINGLE_STEP
#define EXCEPTION_ARRAY_BOUNDS_EXCEEDED     STATUS_ARRAY_BOUNDS_EXCEEDED
#define EXCEPTION_FLT_DENORMAL_OPERAND      STATUS_FLOAT_DENORMAL_OPERAND
#define EXCEPTION_FLT_DIVIDE_BY_ZERO        STATUS_FLOAT_DIVIDE_BY_ZERO
#define EXCEPTION_FLT_INEXACT_RESULT        STATUS_FLOAT_INEXACT_RESULT
#define EXCEPTION_FLT_INVALID_OPERATION     STATUS_FLOAT_INVALID_OPERATION
#define EXCEPTION_FLT_OVERFLOW              STATUS_FLOAT_OVERFLOW
#define EXCEPTION_FLT_STACK_CHECK           STATUS_FLOAT_STACK_CHECK
#define EXCEPTION_FLT_UNDERFLOW             STATUS_FLOAT_UNDERFLOW
#define EXCEPTION_INT_DIVIDE_BY_ZERO        STATUS_INTEGER_DIVIDE_BY_ZERO
#define EXCEPTION_INT_OVERFLOW              STATUS_INTEGER_OVERFLOW
#define EXCEPTION_PRIV_INSTRUCTION          STATUS_PRIVILEGED_INSTRUCTION
#define EXCEPTION_IN_PAGE_ERROR             STATUS_IN_PAGE_ERROR
#define EXCEPTION_ILLEGAL_INSTRUCTION       STATUS_ILLEGAL_INSTRUCTION
#define EXCEPTION_NONCONTINUABLE_EXCEPTION  STATUS_NONCONTINUABLE_EXCEPTION
#define EXCEPTION_STACK_OVERFLOW            STATUS_STACK_OVERFLOW
#define EXCEPTION_INVALID_DISPOSITION       STATUS_INVALID_DISPOSITION
#define EXCEPTION_GUARD_PAGE                STATUS_GUARD_PAGE_VIOLATION
#define EXCEPTION_INVALID_HANDLE            STATUS_INVALID_HANDLE
#define CONTROL_C_EXIT                      STATUS_CONTROL_C_EXIT

/* Wine extension; Windows doesn't have a name for this code */
#define EXCEPTION_CRITICAL_SECTION_WAIT     0xc0000194

#define DUPLICATE_CLOSE_SOURCE		0x00000001
#define DUPLICATE_SAME_ACCESS		0x00000002

#define HANDLE_FLAG_INHERIT             0x00000001
#define HANDLE_FLAG_PROTECT_FROM_CLOSE  0x00000002

#define HINSTANCE_ERROR 32

#define THREAD_PRIORITY_LOWEST          THREAD_BASE_PRIORITY_MIN
#define THREAD_PRIORITY_BELOW_NORMAL    (THREAD_PRIORITY_LOWEST+1)
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_HIGHEST         THREAD_BASE_PRIORITY_MAX
#define THREAD_PRIORITY_ABOVE_NORMAL    (THREAD_PRIORITY_HIGHEST-1)
#define THREAD_PRIORITY_ERROR_RETURN    (0x7fffffff)
#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT
#define THREAD_PRIORITY_IDLE            THREAD_BASE_PRIORITY_IDLE

/* Could this type be considered opaque? */
typedef struct {
	LPVOID	DebugInfo;
	LONG LockCount;
	LONG RecursionCount;
	HANDLE OwningThread;
	HANDLE LockSemaphore;
	DWORD Reserved;
}CRITICAL_SECTION;

#ifdef __WINE__
#define CRITICAL_SECTION_INIT { 0, -1, 0, 0, 0, 0 }
#endif

typedef struct {
        DWORD dwOSVersionInfoSize;
        DWORD dwMajorVersion;
        DWORD dwMinorVersion;
        DWORD dwBuildNumber;
        DWORD dwPlatformId;
        CHAR szCSDVersion[128];
} OSVERSIONINFO16;

typedef struct {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	CHAR szCSDVersion[128];
} OSVERSIONINFOA;

typedef struct {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	WCHAR szCSDVersion[128];
} OSVERSIONINFOW;

DECL_WINELIB_TYPE_AW(OSVERSIONINFO)

#define VER_PLATFORM_WIN32s             0
#define VER_PLATFORM_WIN32_WINDOWS      1
#define VER_PLATFORM_WIN32_NT           2

typedef struct tagCOMSTAT
{
    DWORD status;
    DWORD cbInQue;
    DWORD cbOutQue;
} COMSTAT,*LPCOMSTAT;

typedef struct tagDCB
{
    DWORD DCBlength;
    DWORD BaudRate;
    unsigned fBinary               :1;
    unsigned fParity               :1;
    unsigned fOutxCtsFlow          :1;
    unsigned fOutxDsrFlow          :1;
    unsigned fDtrControl           :2;
    unsigned fDsrSensitivity       :1;
    unsigned fTXContinueOnXoff     :1;
    unsigned fOutX                 :1;
    unsigned fInX                  :1;
    unsigned fErrorChar            :1;
    unsigned fNull                 :1;
    unsigned fRtsControl           :2;
    unsigned fAbortOnError         :1;
    unsigned fDummy2               :17;
    WORD wReserved;
    WORD XonLim;
    WORD XoffLim;
    BYTE ByteSize;
    BYTE Parity;
    BYTE StopBits;
    char XonChar;
    char XoffChar;
    char ErrorChar;
    char EofChar;
    char EvtChar;
} DCB, *LPDCB;



typedef struct tagCOMMTIMEOUTS {
	DWORD	ReadIntervalTimeout;
	DWORD	ReadTotalTimeoutMultiplier;
	DWORD	ReadTotalTimeoutConstant;
	DWORD	WriteTotalTimeoutMultiplier;
	DWORD	WriteTotalTimeoutConstant;
} COMMTIMEOUTS,*LPCOMMTIMEOUTS;
  
#include "poppack.h"

typedef void CALLBACK (*PAPCFUNC)(ULONG_PTR);
typedef void CALLBACK (*PTIMERAPCROUTINE)(LPVOID,DWORD,DWORD);

WIN_BOOL      WINAPI ClearCommError(INT,LPDWORD,LPCOMSTAT);
WIN_BOOL      WINAPI BuildCommDCBA(LPCSTR,LPDCB);
WIN_BOOL      WINAPI BuildCommDCBW(LPCWSTR,LPDCB);
#define     BuildCommDCB WINELIB_NAME_AW(BuildCommDCB)
WIN_BOOL      WINAPI BuildCommDCBAndTimeoutsA(LPCSTR,LPDCB,LPCOMMTIMEOUTS);
WIN_BOOL      WINAPI BuildCommDCBAndTimeoutsW(LPCWSTR,LPDCB,LPCOMMTIMEOUTS);
#define     BuildCommDCBAndTimeouts WINELIB_NAME_AW(BuildCommDCBAndTimeouts)
WIN_BOOL      WINAPI GetCommTimeouts(HANDLE,LPCOMMTIMEOUTS);
WIN_BOOL      WINAPI SetCommTimeouts(HANDLE,LPCOMMTIMEOUTS);
WIN_BOOL      WINAPI GetCommState(INT,LPDCB);
WIN_BOOL      WINAPI SetCommState(INT,LPDCB);
WIN_BOOL      WINAPI TransmitCommChar(INT,CHAR);
WIN_BOOL      WINAPI SetupComm(HANDLE, DWORD, DWORD);
WIN_BOOL      WINAPI GetCommProperties(HANDLE, LPDCB *);
  
/*DWORD WINAPI GetVersion( void );*/
WIN_BOOL16 WINAPI GetVersionEx16(OSVERSIONINFO16*);
WIN_BOOL WINAPI GetVersionExA(OSVERSIONINFOA*);
WIN_BOOL WINAPI GetVersionExW(OSVERSIONINFOW*);
#define GetVersionEx WINELIB_NAME_AW(GetVersionEx)

/*int WinMain(HINSTANCE, HINSTANCE prev, char *cmd, int show);*/

void      WINAPI DeleteCriticalSection(CRITICAL_SECTION *lpCrit);
void      WINAPI EnterCriticalSection(CRITICAL_SECTION *lpCrit);
WIN_BOOL      WINAPI TryEnterCriticalSection(CRITICAL_SECTION *lpCrit);
void      WINAPI InitializeCriticalSection(CRITICAL_SECTION *lpCrit);
void      WINAPI LeaveCriticalSection(CRITICAL_SECTION *lpCrit);
void      WINAPI MakeCriticalSectionGlobal(CRITICAL_SECTION *lpCrit);
WIN_BOOL    WINAPI GetProcessWorkingSetSize(HANDLE,LPDWORD,LPDWORD);
DWORD     WINAPI QueueUserAPC(PAPCFUNC,HANDLE,ULONG_PTR);
void      WINAPI RaiseException(DWORD,DWORD,DWORD,const LPDWORD);
WIN_BOOL    WINAPI SetProcessWorkingSetSize(HANDLE,DWORD,DWORD);
WIN_BOOL    WINAPI TerminateProcess(HANDLE,DWORD);
WIN_BOOL    WINAPI TerminateThread(HANDLE,DWORD);
WIN_BOOL    WINAPI GetExitCodeThread(HANDLE,LPDWORD); 

/* GetBinaryType return values.
 */

#define SCS_32BIT_BINARY    0
#define SCS_DOS_BINARY      1
#define SCS_WOW_BINARY      2
#define SCS_PIF_BINARY      3
#define SCS_POSIX_BINARY    4
#define SCS_OS216_BINARY    5

WIN_BOOL WINAPI GetBinaryTypeA( LPCSTR lpApplicationName, LPDWORD lpBinaryType );
WIN_BOOL WINAPI GetBinaryTypeW( LPCWSTR lpApplicationName, LPDWORD lpBinaryType );
#define GetBinaryType WINELIB_NAME_AW(GetBinaryType)

WIN_BOOL16      WINAPI GetWinDebugInfo16(LPWINDEBUGINFO,UINT16);
WIN_BOOL16      WINAPI SetWinDebugInfo16(LPWINDEBUGINFO);
/* Declarations for functions that exist only in Win32 */


WIN_BOOL        WINAPI AttachThreadInput(DWORD,DWORD,WIN_BOOL);
WIN_BOOL        WINAPI AccessCheck(PSECURITY_DESCRIPTOR,HANDLE,DWORD,PGENERIC_MAPPING,PPRIVILEGE_SET,LPDWORD,LPDWORD,LPWIN_BOOL);
WIN_BOOL        WINAPI AdjustTokenPrivileges(HANDLE,WIN_BOOL,LPVOID,DWORD,LPVOID,LPDWORD);
WIN_BOOL        WINAPI AllocateAndInitializeSid(PSID_IDENTIFIER_AUTHORITY,BYTE,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,PSID *);
WIN_BOOL        WINAPI AllocateLocallyUniqueId(PLUID);
WIN_BOOL      WINAPI AllocConsole(void);
WIN_BOOL      WINAPI AreFileApisANSI(void);
WIN_BOOL        WINAPI BackupEventLogA(HANDLE,LPCSTR);
WIN_BOOL        WINAPI BackupEventLogW(HANDLE,LPCWSTR);
#define     BackupEventLog WINELIB_NAME_AW(BackupEventLog)
WIN_BOOL        WINAPI Beep(DWORD,DWORD);
WIN_BOOL        WINAPI CancelWaitableTimer(HANDLE);
WIN_BOOL        WINAPI ClearEventLogA(HANDLE,LPCSTR);
WIN_BOOL        WINAPI ClearEventLogW(HANDLE,LPCWSTR);
#define     ClearEventLog WINELIB_NAME_AW(ClearEventLog)
WIN_BOOL        WINAPI CloseEventLog(HANDLE);
WIN_BOOL      WINAPI CloseHandle(HANDLE);
WIN_BOOL      WINAPI ContinueDebugEvent(DWORD,DWORD,DWORD);
HANDLE    WINAPI ConvertToGlobalHandle(HANDLE hSrc);
WIN_BOOL      WINAPI CopyFileA(LPCSTR,LPCSTR,WIN_BOOL);
WIN_BOOL      WINAPI CopyFileW(LPCWSTR,LPCWSTR,WIN_BOOL);
#define     CopyFile WINELIB_NAME_AW(CopyFile)
WIN_BOOL      WINAPI CopyFileExA(LPCSTR, LPCSTR, LPPROGRESS_ROUTINE, LPVOID, LPWIN_BOOL, DWORD);
WIN_BOOL      WINAPI CopyFileExW(LPCWSTR, LPCWSTR, LPPROGRESS_ROUTINE, LPVOID, LPWIN_BOOL, DWORD);
#define     CopyFileEx WINELIB_NAME_AW(CopyFileEx)
WIN_BOOL        WINAPI CopySid(DWORD,PSID,PSID);
INT       WINAPI CompareFileTime(LPFILETIME,LPFILETIME);
HANDLE    WINAPI CreateEventA(LPSECURITY_ATTRIBUTES,WIN_BOOL,WIN_BOOL,LPCSTR);
HANDLE    WINAPI CreateEventW(LPSECURITY_ATTRIBUTES,WIN_BOOL,WIN_BOOL,LPCWSTR);
#define     CreateEvent WINELIB_NAME_AW(CreateEvent)
HANDLE     WINAPI CreateFileA(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,
                                 DWORD,DWORD,HANDLE);
HANDLE     WINAPI CreateFileW(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,
                                 DWORD,DWORD,HANDLE);
#define     CreateFile WINELIB_NAME_AW(CreateFile)
HANDLE    WINAPI CreateFileMappingA(HANDLE,LPSECURITY_ATTRIBUTES,DWORD,
                                        DWORD,DWORD,LPCSTR);
HANDLE    WINAPI CreateFileMappingW(HANDLE,LPSECURITY_ATTRIBUTES,DWORD,
                                        DWORD,DWORD,LPCWSTR);
#define     CreateFileMapping WINELIB_NAME_AW(CreateFileMapping)
HANDLE    WINAPI CreateMutexA(LPSECURITY_ATTRIBUTES,WIN_BOOL,LPCSTR);
HANDLE    WINAPI CreateMutexW(LPSECURITY_ATTRIBUTES,WIN_BOOL,LPCWSTR);
#define     CreateMutex WINELIB_NAME_AW(CreateMutex)
WIN_BOOL      WINAPI CreatePipe(PHANDLE,PHANDLE,LPSECURITY_ATTRIBUTES,DWORD);
WIN_BOOL      WINAPI CreateProcessA(LPCSTR,LPSTR,LPSECURITY_ATTRIBUTES,
                                    LPSECURITY_ATTRIBUTES,WIN_BOOL,DWORD,LPVOID,LPCSTR,
                                    LPSTARTUPINFOA,LPPROCESS_INFORMATION);
WIN_BOOL      WINAPI CreateProcessW(LPCWSTR,LPWSTR,LPSECURITY_ATTRIBUTES,
                                    LPSECURITY_ATTRIBUTES,WIN_BOOL,DWORD,LPVOID,LPCWSTR,
                                    LPSTARTUPINFOW,LPPROCESS_INFORMATION);
#define     CreateProcess WINELIB_NAME_AW(CreateProcess)
HANDLE    WINAPI CreateSemaphoreA(LPSECURITY_ATTRIBUTES,LONG,LONG,LPCSTR);
HANDLE    WINAPI CreateSemaphoreW(LPSECURITY_ATTRIBUTES,LONG,LONG,LPCWSTR);
#define     CreateSemaphore WINELIB_NAME_AW(CreateSemaphore)
HANDLE      WINAPI CreateThread(LPSECURITY_ATTRIBUTES,DWORD,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
HANDLE      WINAPI CreateWaitableTimerA(LPSECURITY_ATTRIBUTES,WIN_BOOL,LPCSTR);
HANDLE      WINAPI CreateWaitableTimerW(LPSECURITY_ATTRIBUTES,WIN_BOOL,LPCWSTR);
#define     CreateWaitableTimer WINELIB_NAME_AW(CreateWaitableTimer)
WIN_BOOL        WINAPI DebugActiveProcess(DWORD);
void        WINAPI DebugBreak(void);
WIN_BOOL        WINAPI DeregisterEventSource(HANDLE);
WIN_BOOL        WINAPI DisableThreadLibraryCalls(HMODULE);
WIN_BOOL        WINAPI DosDateTimeToFileTime(WORD,WORD,LPFILETIME);
WIN_BOOL        WINAPI DuplicateHandle(HANDLE,HANDLE,HANDLE,HANDLE*,DWORD,WIN_BOOL,DWORD);
WIN_BOOL        WINAPI EnumDateFormatsA(DATEFMT_ENUMPROCA lpDateFmtEnumProc, LCID Locale, DWORD dwFlags);
WIN_BOOL        WINAPI EnumDateFormatsW(DATEFMT_ENUMPROCW lpDateFmtEnumProc, LCID Locale, DWORD dwFlags);
#define     EnumDateFormats WINELIB_NAME_AW(EnumDateFormats)
WIN_BOOL      WINAPI EnumResourceLanguagesA(HMODULE,LPCSTR,LPCSTR,
                                            ENUMRESLANGPROCA,LONG);
WIN_BOOL      WINAPI EnumResourceLanguagesW(HMODULE,LPCWSTR,LPCWSTR,
                                            ENUMRESLANGPROCW,LONG);
#define     EnumResourceLanguages WINELIB_NAME_AW(EnumResourceLanguages)
WIN_BOOL      WINAPI EnumResourceNamesA(HMODULE,LPCSTR,ENUMRESNAMEPROCA,
                                        LONG);
WIN_BOOL      WINAPI EnumResourceNamesW(HMODULE,LPCWSTR,ENUMRESNAMEPROCW,
                                        LONG);
#define     EnumResourceNames WINELIB_NAME_AW(EnumResourceNames)
WIN_BOOL      WINAPI EnumResourceTypesA(HMODULE,ENUMRESTYPEPROCA,LONG);
WIN_BOOL      WINAPI EnumResourceTypesW(HMODULE,ENUMRESTYPEPROCW,LONG);
#define     EnumResourceTypes WINELIB_NAME_AW(EnumResourceTypes)
WIN_BOOL      WINAPI EnumSystemCodePagesA(CODEPAGE_ENUMPROCA,DWORD);
WIN_BOOL      WINAPI EnumSystemCodePagesW(CODEPAGE_ENUMPROCW,DWORD);
#define     EnumSystemCodePages WINELIB_NAME_AW(EnumSystemCodePages)
WIN_BOOL      WINAPI EnumSystemLocalesA(LOCALE_ENUMPROCA,DWORD);
WIN_BOOL      WINAPI EnumSystemLocalesW(LOCALE_ENUMPROCW,DWORD);
#define     EnumSystemLocales WINELIB_NAME_AW(EnumSystemLocales)
WIN_BOOL      WINAPI EnumTimeFormatsA(TIMEFMT_ENUMPROCA lpTimeFmtEnumProc, LCID Locale, DWORD dwFlags);
WIN_BOOL      WINAPI EnumTimeFormatsW(TIMEFMT_ENUMPROCW lpTimeFmtEnumProc, LCID Locale, DWORD dwFlags);
#define     EnumTimeFormats WINELIB_NAME_AW(EnumTimeFormats)
WIN_BOOL        WINAPI EqualSid(PSID, PSID);
WIN_BOOL        WINAPI EqualPrefixSid(PSID,PSID);
VOID        WINAPI ExitProcess(DWORD) WINE_NORETURN;
VOID        WINAPI ExitThread(DWORD) WINE_NORETURN;
DWORD       WINAPI ExpandEnvironmentStringsA(LPCSTR,LPSTR,DWORD);
DWORD       WINAPI ExpandEnvironmentStringsW(LPCWSTR,LPWSTR,DWORD);
#define     ExpandEnvironmentStrings WINELIB_NAME_AW(ExpandEnvironmentStrings)
WIN_BOOL      WINAPI FileTimeToDosDateTime(const FILETIME*,LPWORD,LPWORD);
WIN_BOOL      WINAPI FileTimeToLocalFileTime(const FILETIME*,LPFILETIME);
WIN_BOOL      WINAPI FileTimeToSystemTime(const FILETIME*,LPSYSTEMTIME);
HANDLE    WINAPI FindFirstChangeNotificationA(LPCSTR,WIN_BOOL,DWORD);
HANDLE    WINAPI FindFirstChangeNotificationW(LPCWSTR,WIN_BOOL,DWORD);
#define     FindFirstChangeNotification WINELIB_NAME_AW(FindFirstChangeNotification)
WIN_BOOL      WINAPI FindNextChangeNotification(HANDLE);
WIN_BOOL      WINAPI FindCloseChangeNotification(HANDLE);
HRSRC     WINAPI FindResourceExA(HMODULE,LPCSTR,LPCSTR,WORD);
HRSRC     WINAPI FindResourceExW(HMODULE,LPCWSTR,LPCWSTR,WORD);
#define     FindResourceEx WINELIB_NAME_AW(FindResourceEx)
WIN_BOOL      WINAPI FlushConsoleInputBuffer(HANDLE);
WIN_BOOL      WINAPI FlushFileBuffers(HANDLE);
WIN_BOOL      WINAPI FlushViewOfFile(LPCVOID, DWORD);
DWORD       WINAPI FormatMessageA(DWORD,LPCVOID,DWORD,DWORD,LPSTR,
				    DWORD,LPDWORD);
DWORD       WINAPI FormatMessageW(DWORD,LPCVOID,DWORD,DWORD,LPWSTR,
				    DWORD,LPDWORD);
#define     FormatMessage WINELIB_NAME_AW(FormatMessage)
WIN_BOOL      WINAPI FreeConsole(void);
WIN_BOOL      WINAPI FreeEnvironmentStringsA(LPSTR);
WIN_BOOL      WINAPI FreeEnvironmentStringsW(LPWSTR);
#define     FreeEnvironmentStrings WINELIB_NAME_AW(FreeEnvironmentStrings)
PVOID       WINAPI FreeSid(PSID);
UINT      WINAPI GetACP(void);
LPCSTR      WINAPI GetCommandLineA(void);
LPCWSTR     WINAPI GetCommandLineW(void);
#define     GetCommandLine WINELIB_NAME_AW(GetCommandLine)
WIN_BOOL      WINAPI GetComputerNameA(LPSTR,LPDWORD);
WIN_BOOL      WINAPI GetComputerNameW(LPWSTR,LPDWORD);
#define     GetComputerName WINELIB_NAME_AW(GetComputerName)
UINT      WINAPI GetConsoleCP(void);
WIN_BOOL      WINAPI GetConsoleMode(HANDLE,LPDWORD);
UINT      WINAPI GetConsoleOutputCP(void);
DWORD       WINAPI GetConsoleTitleA(LPSTR,DWORD);
DWORD       WINAPI GetConsoleTitleW(LPWSTR,DWORD);
#define     GetConsoleTitle WINELIB_NAME_AW(GetConsoleTitle)
WIN_BOOL        WINAPI GetCommMask(HANDLE, LPDWORD);
WIN_BOOL        WINAPI GetCommModemStatus(HANDLE, LPDWORD);
HANDLE      WINAPI GetCurrentProcess(void);
HANDLE      WINAPI GetCurrentThread(void);
INT         WINAPI GetDateFormatA(LCID,DWORD,LPSYSTEMTIME,LPCSTR,LPSTR,INT);
INT         WINAPI GetDateFormatW(LCID,DWORD,LPSYSTEMTIME,LPCWSTR,LPWSTR,INT);
#define     GetDateFormat WINELIB_NAME_AW(GetDateFormat)
LPSTR       WINAPI GetEnvironmentStringsA(void);
LPWSTR      WINAPI GetEnvironmentStringsW(void);
#define     GetEnvironmentStrings WINELIB_NAME_AW(GetEnvironmentStrings)
DWORD       WINAPI GetEnvironmentVariableA(LPCSTR,LPSTR,DWORD);
DWORD       WINAPI GetEnvironmentVariableW(LPCWSTR,LPWSTR,DWORD);
#define     GetEnvironmentVariable WINELIB_NAME_AW(GetEnvironmentVariable)
WIN_BOOL      WINAPI GetFileAttributesExA(LPCSTR,GET_FILEEX_INFO_LEVELS,LPVOID);
WIN_BOOL      WINAPI GetFileAttributesExW(LPCWSTR,GET_FILEEX_INFO_LEVELS,LPVOID);
#define     GetFileattributesEx WINELIB_NAME_AW(GetFileAttributesEx)
DWORD       WINAPI GetFileInformationByHandle(HANDLE,BY_HANDLE_FILE_INFORMATION*);
WIN_BOOL        WINAPI GetFileSecurityA(LPCSTR,SECURITY_INFORMATION,PSECURITY_DESCRIPTOR,DWORD,LPDWORD);
WIN_BOOL        WINAPI GetFileSecurityW(LPCWSTR,SECURITY_INFORMATION,PSECURITY_DESCRIPTOR,DWORD,LPDWORD);
#define     GetFileSecurity WINELIB_NAME_AW(GetFileSecurity)
DWORD       WINAPI GetFileSize(HANDLE,LPDWORD);
WIN_BOOL        WINAPI GetFileTime(HANDLE,LPFILETIME,LPFILETIME,LPFILETIME);
DWORD       WINAPI GetFileType(HANDLE);
DWORD       WINAPI GetFullPathNameA(LPCSTR,DWORD,LPSTR,LPSTR*);
DWORD       WINAPI GetFullPathNameW(LPCWSTR,DWORD,LPWSTR,LPWSTR*);
#define     GetFullPathName WINELIB_NAME_AW(GetFullPathName)
WIN_BOOL      WINAPI GetHandleInformation(HANDLE,LPDWORD);
COORD       WINAPI GetLargestConsoleWindowSize(HANDLE);
DWORD       WINAPI GetLengthSid(PSID);
VOID        WINAPI GetLocalTime(LPSYSTEMTIME);
DWORD       WINAPI GetLogicalDrives(void);
DWORD       WINAPI GetLongPathNameA(LPCSTR,LPSTR,DWORD);
DWORD       WINAPI GetLongPathNameW(LPCWSTR,LPWSTR,DWORD);
#define     GetLongPathName WINELIB_NAME_AW(GetLongPathName)
WIN_BOOL      WINAPI GetNumberOfConsoleInputEvents(HANDLE,LPDWORD);
WIN_BOOL      WINAPI GetNumberOfConsoleMouseButtons(LPDWORD);
WIN_BOOL        WINAPI GetNumberOfEventLogRecords(HANDLE,PDWORD);
UINT      WINAPI GetOEMCP(void);
WIN_BOOL        WINAPI GetOldestEventLogRecord(HANDLE,PDWORD);
DWORD       WINAPI GetPriorityClass(HANDLE);
DWORD       WINAPI GetProcessVersion(DWORD);
WIN_BOOL        WINAPI GetSecurityDescriptorControl(PSECURITY_DESCRIPTOR,PSECURITY_DESCRIPTOR_CONTROL,LPDWORD);
WIN_BOOL        WINAPI GetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR,LPWIN_BOOL,PACL *,LPWIN_BOOL);
WIN_BOOL        WINAPI GetSecurityDescriptorGroup(PSECURITY_DESCRIPTOR,PSID *,LPWIN_BOOL);
DWORD       WINAPI GetSecurityDescriptorLength(PSECURITY_DESCRIPTOR);
WIN_BOOL        WINAPI GetSecurityDescriptorOwner(PSECURITY_DESCRIPTOR,PSID *,LPWIN_BOOL);
WIN_BOOL        WINAPI GetSecurityDescriptorSacl(PSECURITY_DESCRIPTOR,LPWIN_BOOL,PACL *,LPWIN_BOOL);
PSID_IDENTIFIER_AUTHORITY WINAPI GetSidIdentifierAuthority(PSID);
DWORD       WINAPI GetSidLengthRequired(BYTE);
PDWORD      WINAPI GetSidSubAuthority(PSID,DWORD);
PUCHAR      WINAPI GetSidSubAuthorityCount(PSID);
DWORD       WINAPI GetShortPathNameA(LPCSTR,LPSTR,DWORD);
DWORD       WINAPI GetShortPathNameW(LPCWSTR,LPWSTR,DWORD);
#define     GetShortPathName WINELIB_NAME_AW(GetShortPathName)
HFILE     WINAPI GetStdHandle(DWORD);
WIN_BOOL      WINAPI GetStringTypeExA(LCID,DWORD,LPCSTR,INT,LPWORD);
WIN_BOOL      WINAPI GetStringTypeExW(LCID,DWORD,LPCWSTR,INT,LPWORD);
#define     GetStringTypeEx WINELIB_NAME_AW(GetStringTypeEx)
VOID        WINAPI GetSystemInfo(LPSYSTEM_INFO);
WIN_BOOL        WINAPI GetSystemPowerStatus(LPSYSTEM_POWER_STATUS);
VOID        WINAPI GetSystemTime(LPSYSTEMTIME);
VOID        WINAPI GetSystemTimeAsFileTime(LPFILETIME);
INT         WINAPI GetTimeFormatA(LCID,DWORD,LPSYSTEMTIME,LPCSTR,LPSTR,INT);
INT         WINAPI GetTimeFormatW(LCID,DWORD,LPSYSTEMTIME,LPCWSTR,LPWSTR,INT);
#define     GetTimeFormat WINELIB_NAME_AW(GetTimeFormat)
WIN_BOOL        WINAPI GetThreadContext(HANDLE,CONTEXT *);
LCID        WINAPI GetThreadLocale(void);
INT       WINAPI GetThreadPriority(HANDLE);
WIN_BOOL      WINAPI GetThreadSelectorEntry(HANDLE,DWORD,LPLDT_ENTRY);
WIN_BOOL        WINAPI GetThreadTimes(HANDLE,LPFILETIME,LPFILETIME,LPFILETIME,LPFILETIME);
WIN_BOOL        WINAPI GetTokenInformation(HANDLE,TOKEN_INFORMATION_CLASS,LPVOID,DWORD,LPDWORD);
WIN_BOOL        WINAPI GetUserNameA(LPSTR,LPDWORD);
WIN_BOOL        WINAPI GetUserNameW(LPWSTR,LPDWORD);
#define     GetUserName WINELIB_NAME_AW(GetUserName)
VOID        WINAPI GlobalMemoryStatus(LPMEMORYSTATUS);
LPVOID      WINAPI HeapAlloc(HANDLE,DWORD,DWORD);
DWORD       WINAPI HeapCompact(HANDLE,DWORD);
HANDLE    WINAPI HeapCreate(DWORD,DWORD,DWORD);
WIN_BOOL      WINAPI HeapDestroy(HANDLE);
WIN_BOOL      WINAPI HeapFree(HANDLE,DWORD,LPVOID);
WIN_BOOL      WINAPI HeapLock(HANDLE);
LPVOID      WINAPI HeapReAlloc(HANDLE,DWORD,LPVOID,DWORD);
DWORD       WINAPI HeapSize(HANDLE,DWORD,LPVOID);
WIN_BOOL      WINAPI HeapUnlock(HANDLE);
WIN_BOOL      WINAPI HeapValidate(HANDLE,DWORD,LPCVOID);
WIN_BOOL        WINAPI HeapWalk(HANDLE,LPPROCESS_HEAP_ENTRY);
WIN_BOOL        WINAPI InitializeSid(PSID,PSID_IDENTIFIER_AUTHORITY,BYTE);
WIN_BOOL        WINAPI IsValidSecurityDescriptor(PSECURITY_DESCRIPTOR);
WIN_BOOL        WINAPI IsValidSid(PSID);
WIN_BOOL        WINAPI ImpersonateSelf(SECURITY_IMPERSONATION_LEVEL);
WIN_BOOL        WINAPI IsDBCSLeadByteEx(UINT,BYTE);
WIN_BOOL        WINAPI IsProcessorFeaturePresent(DWORD);
WIN_BOOL        WINAPI IsValidLocale(DWORD,DWORD);
WIN_BOOL        WINAPI LookupAccountSidA(LPCSTR,PSID,LPSTR,LPDWORD,LPSTR,LPDWORD,PSID_NAME_USE);
WIN_BOOL        WINAPI LookupAccountSidW(LPCWSTR,PSID,LPWSTR,LPDWORD,LPWSTR,LPDWORD,PSID_NAME_USE);
#define     LookupAccountSid WINELIB_NAME_AW(LookupAccountSidW)
WIN_BOOL        WINAPI LocalFileTimeToFileTime(const FILETIME*,LPFILETIME);
WIN_BOOL        WINAPI LockFile(HANDLE,DWORD,DWORD,DWORD,DWORD);
WIN_BOOL        WINAPI LockFileEx(HANDLE, DWORD, DWORD, DWORD, DWORD, LPOVERLAPPED);    
WIN_BOOL        WINAPI LookupPrivilegeValueA(LPCSTR,LPCSTR,LPVOID);
WIN_BOOL        WINAPI LookupPrivilegeValueW(LPCWSTR,LPCWSTR,LPVOID);
#define     LookupPrivilegeValue WINELIB_NAME_AW(LookupPrivilegeValue)
WIN_BOOL        WINAPI MakeSelfRelativeSD(PSECURITY_DESCRIPTOR,PSECURITY_DESCRIPTOR,LPDWORD);
HMODULE   WINAPI MapHModuleSL(HMODULE16);
HMODULE16   WINAPI MapHModuleLS(HMODULE);
SEGPTR      WINAPI MapLS(LPVOID);
LPVOID      WINAPI MapSL(SEGPTR);
LPVOID      WINAPI MapViewOfFile(HANDLE,DWORD,DWORD,DWORD,DWORD);
LPVOID      WINAPI MapViewOfFileEx(HANDLE,DWORD,DWORD,DWORD,DWORD,LPVOID);
WIN_BOOL      WINAPI MoveFileA(LPCSTR,LPCSTR);
WIN_BOOL      WINAPI MoveFileW(LPCWSTR,LPCWSTR);
#define     MoveFile WINELIB_NAME_AW(MoveFile)
WIN_BOOL      WINAPI MoveFileExA(LPCSTR,LPCSTR,DWORD);
WIN_BOOL      WINAPI MoveFileExW(LPCWSTR,LPCWSTR,DWORD);
#define     MoveFileEx WINELIB_NAME_AW(MoveFileEx)
INT       WINAPI MultiByteToWideChar(UINT,DWORD,LPCSTR,INT,LPWSTR,INT);
WIN_BOOL        WINAPI NotifyChangeEventLog(HANDLE,HANDLE);
INT       WINAPI WideCharToMultiByte(UINT,DWORD,LPCWSTR,INT,LPSTR,INT,LPCSTR,WIN_BOOL*);
HANDLE      WINAPI OpenBackupEventLogA(LPCSTR,LPCSTR);
HANDLE      WINAPI OpenBackupEventLogW(LPCWSTR,LPCWSTR);
#define     OpenBackupEventLog WINELIB_NAME_AW(OpenBackupEventLog)
HANDLE    WINAPI OpenEventA(DWORD,WIN_BOOL,LPCSTR);
HANDLE    WINAPI OpenEventW(DWORD,WIN_BOOL,LPCWSTR);
#define     OpenEvent WINELIB_NAME_AW(OpenEvent)
HANDLE      WINAPI OpenEventLogA(LPCSTR,LPCSTR);
HANDLE      WINAPI OpenEventLogW(LPCWSTR,LPCWSTR);
#define     OpenEventLog WINELIB_NAME_AW(OpenEventLog)
HANDLE    WINAPI OpenFileMappingA(DWORD,WIN_BOOL,LPCSTR);
HANDLE    WINAPI OpenFileMappingW(DWORD,WIN_BOOL,LPCWSTR);
#define     OpenFileMapping WINELIB_NAME_AW(OpenFileMapping)
HANDLE    WINAPI OpenMutexA(DWORD,WIN_BOOL,LPCSTR);
HANDLE    WINAPI OpenMutexW(DWORD,WIN_BOOL,LPCWSTR);
#define     OpenMutex WINELIB_NAME_AW(OpenMutex)
HANDLE    WINAPI OpenProcess(DWORD,WIN_BOOL,DWORD);
WIN_BOOL        WINAPI OpenProcessToken(HANDLE,DWORD,PHANDLE);
HANDLE    WINAPI OpenSemaphoreA(DWORD,WIN_BOOL,LPCSTR);
HANDLE    WINAPI OpenSemaphoreW(DWORD,WIN_BOOL,LPCWSTR);
#define     OpenSemaphore WINELIB_NAME_AW(OpenSemaphore)
WIN_BOOL        WINAPI OpenThreadToken(HANDLE,DWORD,WIN_BOOL,PHANDLE);
HANDLE      WINAPI OpenWaitableTimerA(DWORD,WIN_BOOL,LPCSTR);
HANDLE      WINAPI OpenWaitableTimerW(DWORD,WIN_BOOL,LPCWSTR);
#define     OpenWaitableTimer WINELIB_NAME_AW(OpenWaitableTimer)
WIN_BOOL        WINAPI PulseEvent(HANDLE);
WIN_BOOL        WINAPI PurgeComm(HANDLE,DWORD);
DWORD       WINAPI QueryDosDeviceA(LPCSTR,LPSTR,DWORD);
DWORD       WINAPI QueryDosDeviceW(LPCWSTR,LPWSTR,DWORD);
#define     QueryDosDevice WINELIB_NAME_AW(QueryDosDevice)
WIN_BOOL      WINAPI QueryPerformanceCounter(PLARGE_INTEGER);
WIN_BOOL      WINAPI ReadConsoleA(HANDLE,LPVOID,DWORD,LPDWORD,LPVOID);
WIN_BOOL      WINAPI ReadConsoleW(HANDLE,LPVOID,DWORD,LPDWORD,LPVOID);
#define     ReadConsole WINELIB_NAME_AW(ReadConsole)
WIN_BOOL      WINAPI ReadConsoleOutputCharacterA(HANDLE,LPSTR,DWORD,
						 COORD,LPDWORD);
#define     ReadConsoleOutputCharacter WINELIB_NAME_AW(ReadConsoleOutputCharacter)
WIN_BOOL        WINAPI ReadEventLogA(HANDLE,DWORD,DWORD,LPVOID,DWORD,DWORD *,DWORD *);
WIN_BOOL        WINAPI ReadEventLogW(HANDLE,DWORD,DWORD,LPVOID,DWORD,DWORD *,DWORD *);
#define     ReadEventLog WINELIB_NAME_AW(ReadEventLog)
WIN_BOOL      WINAPI ReadFile(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
HANDLE      WINAPI RegisterEventSourceA(LPCSTR,LPCSTR);
HANDLE      WINAPI RegisterEventSourceW(LPCWSTR,LPCWSTR);
#define     RegisterEventSource WINELIB_NAME_AW(RegisterEventSource)
WIN_BOOL      WINAPI ReleaseMutex(HANDLE);
WIN_BOOL      WINAPI ReleaseSemaphore(HANDLE,LONG,LPLONG);
WIN_BOOL        WINAPI ReportEventA(HANDLE,WORD,WORD,DWORD,PSID,WORD,DWORD,LPCSTR *,LPVOID);
WIN_BOOL        WINAPI ReportEventW(HANDLE,WORD,WORD,DWORD,PSID,WORD,DWORD,LPCWSTR *,LPVOID);
#define     ReportEvent WINELIB_NAME_AW(ReportEvent)
WIN_BOOL      WINAPI ResetEvent(HANDLE);
DWORD       WINAPI ResumeThread(HANDLE);
WIN_BOOL        WINAPI RevertToSelf(void);
DWORD       WINAPI SearchPathA(LPCSTR,LPCSTR,LPCSTR,DWORD,LPSTR,LPSTR*);
DWORD       WINAPI SearchPathW(LPCWSTR,LPCWSTR,LPCWSTR,DWORD,LPWSTR,LPWSTR*);
#define     SearchPath WINELIB_NAME_AW(SearchPath)
WIN_BOOL      WINAPI SetCommMask(INT,DWORD);
WIN_BOOL      WINAPI SetComputerNameA(LPCSTR);
WIN_BOOL      WINAPI SetComputerNameW(LPCWSTR);
#define     SetComputerName WINELIB_NAME_AW(SetComputerName)
WIN_BOOL      WINAPI SetConsoleCursorPosition(HANDLE,COORD);
WIN_BOOL      WINAPI SetConsoleMode(HANDLE,DWORD);
WIN_BOOL      WINAPI SetConsoleTitleA(LPCSTR);
WIN_BOOL      WINAPI SetConsoleTitleW(LPCWSTR);
#define     SetConsoleTitle WINELIB_NAME_AW(SetConsoleTitle)
WIN_BOOL      WINAPI SetEndOfFile(HANDLE);
WIN_BOOL      WINAPI SetEnvironmentVariableA(LPCSTR,LPCSTR);
WIN_BOOL      WINAPI SetEnvironmentVariableW(LPCWSTR,LPCWSTR);
#define     SetEnvironmentVariable WINELIB_NAME_AW(SetEnvironmentVariable)
WIN_BOOL      WINAPI SetEvent(HANDLE);
VOID        WINAPI SetFileApisToANSI(void);
VOID        WINAPI SetFileApisToOEM(void);
DWORD       WINAPI SetFilePointer(HANDLE,LONG,LPLONG,DWORD);
WIN_BOOL        WINAPI SetFileSecurityA(LPCSTR,SECURITY_INFORMATION,PSECURITY_DESCRIPTOR);
WIN_BOOL        WINAPI SetFileSecurityW(LPCWSTR,SECURITY_INFORMATION,PSECURITY_DESCRIPTOR);
#define     SetFileSecurity WINELIB_NAME_AW(SetFileSecurity)
WIN_BOOL        WINAPI SetFileTime(HANDLE,const FILETIME*,const FILETIME*,const FILETIME*);
WIN_BOOL        WINAPI SetHandleInformation(HANDLE,DWORD,DWORD);
WIN_BOOL        WINAPI SetPriorityClass(HANDLE,DWORD);
WIN_BOOL        WINAPI SetLocalTime(const SYSTEMTIME*);
WIN_BOOL        WINAPI SetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR,WIN_BOOL,PACL,WIN_BOOL);
WIN_BOOL        WINAPI SetSecurityDescriptorGroup(PSECURITY_DESCRIPTOR,PSID,WIN_BOOL);
WIN_BOOL        WINAPI SetSecurityDescriptorOwner(PSECURITY_DESCRIPTOR,PSID,WIN_BOOL);
WIN_BOOL        WINAPI SetSecurityDescriptorSacl(PSECURITY_DESCRIPTOR,WIN_BOOL,PACL,WIN_BOOL);
WIN_BOOL      WINAPI SetStdHandle(DWORD,HANDLE);
WIN_BOOL      WINAPI SetSystemPowerState(WIN_BOOL,WIN_BOOL);
WIN_BOOL      WINAPI SetSystemTime(const SYSTEMTIME*);
DWORD       WINAPI SetThreadAffinityMask(HANDLE,DWORD);
WIN_BOOL        WINAPI SetThreadContext(HANDLE,const CONTEXT *);
WIN_BOOL        WINAPI SetThreadLocale(LCID);
WIN_BOOL        WINAPI SetThreadPriority(HANDLE,INT);
WIN_BOOL        WINAPI SetTimeZoneInformation(const LPTIME_ZONE_INFORMATION);
WIN_BOOL        WINAPI SetWaitableTimer(HANDLE,const LARGE_INTEGER*,LONG,PTIMERAPCROUTINE,LPVOID,WIN_BOOL);
VOID        WINAPI Sleep(DWORD);
DWORD       WINAPI SleepEx(DWORD,WIN_BOOL);
DWORD       WINAPI SuspendThread(HANDLE);
WIN_BOOL      WINAPI SystemTimeToFileTime(const SYSTEMTIME*,LPFILETIME);
DWORD       WINAPI TlsAlloc(void);
WIN_BOOL      WINAPI TlsFree(DWORD);
LPVOID      WINAPI TlsGetValue(DWORD);
WIN_BOOL      WINAPI TlsSetValue(DWORD,LPVOID);
VOID        WINAPI UnMapLS(SEGPTR);
WIN_BOOL      WINAPI UnlockFile(HANDLE,DWORD,DWORD,DWORD,DWORD);
WIN_BOOL      WINAPI UnmapViewOfFile(LPVOID);
LPVOID      WINAPI VirtualAlloc(LPVOID,DWORD,DWORD,DWORD);
WIN_BOOL      WINAPI VirtualFree(LPVOID,SIZE_T,DWORD);
WIN_BOOL      WINAPI VirtualLock(LPVOID,DWORD);
WIN_BOOL      WINAPI VirtualProtect(LPVOID,DWORD,DWORD,LPDWORD);
WIN_BOOL      WINAPI VirtualProtectEx(HANDLE,LPVOID,DWORD,DWORD,LPDWORD);
DWORD       WINAPI VirtualQuery(LPCVOID,LPMEMORY_BASIC_INFORMATION,DWORD);
DWORD       WINAPI VirtualQueryEx(HANDLE,LPCVOID,LPMEMORY_BASIC_INFORMATION,DWORD);
WIN_BOOL      WINAPI VirtualUnlock(LPVOID,DWORD);
WIN_BOOL      WINAPI WaitCommEvent(HANDLE,LPDWORD,LPOVERLAPPED);
WIN_BOOL      WINAPI WaitForDebugEvent(LPDEBUG_EVENT,DWORD);
DWORD       WINAPI WaitForMultipleObjects(DWORD,const HANDLE*,WIN_BOOL,DWORD);
DWORD       WINAPI WaitForMultipleObjectsEx(DWORD,const HANDLE*,WIN_BOOL,DWORD,WIN_BOOL);
DWORD       WINAPI WaitForSingleObject(HANDLE,DWORD);
DWORD       WINAPI WaitForSingleObjectEx(HANDLE,DWORD,WIN_BOOL);
WIN_BOOL      WINAPI WriteConsoleA(HANDLE,LPCVOID,DWORD,LPDWORD,LPVOID);
WIN_BOOL      WINAPI WriteConsoleW(HANDLE,LPCVOID,DWORD,LPDWORD,LPVOID);
#define     WriteConsole WINELIB_NAME_AW(WriteConsole)
WIN_BOOL      WINAPI WriteFile(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
LANGID      WINAPI GetSystemDefaultLangID(void);
LCID        WINAPI GetSystemDefaultLCID(void);
LANGID      WINAPI GetUserDefaultLangID(void);
LCID        WINAPI GetUserDefaultLCID(void);
ATOM        WINAPI AddAtomA(LPCSTR);
ATOM        WINAPI AddAtomW(LPCWSTR);
#define     AddAtom WINELIB_NAME_AW(AddAtom)
UINT      WINAPI CompareStringA(DWORD,DWORD,LPCSTR,DWORD,LPCSTR,DWORD);
UINT      WINAPI CompareStringW(DWORD,DWORD,LPCWSTR,DWORD,LPCWSTR,DWORD);
#define     CompareString WINELIB_NAME_AW(CompareString)
WIN_BOOL      WINAPI CreateDirectoryA(LPCSTR,LPSECURITY_ATTRIBUTES);
WIN_BOOL      WINAPI CreateDirectoryW(LPCWSTR,LPSECURITY_ATTRIBUTES);
#define     CreateDirectory WINELIB_NAME_AW(CreateDirectory)
WIN_BOOL      WINAPI CreateDirectoryExA(LPCSTR,LPCSTR,LPSECURITY_ATTRIBUTES);
WIN_BOOL      WINAPI CreateDirectoryExW(LPCWSTR,LPCWSTR,LPSECURITY_ATTRIBUTES);
#define     CreateDirectoryEx WINELIB_NAME_AW(CreateDirectoryEx)
WIN_BOOL        WINAPI DefineDosDeviceA(DWORD,LPCSTR,LPCSTR);
#define     DefineHandleTable(w) ((w),TRUE)
ATOM        WINAPI DeleteAtom(ATOM);
WIN_BOOL      WINAPI DeleteFileA(LPCSTR);
WIN_BOOL      WINAPI DeleteFileW(LPCWSTR);
#define     DeleteFile WINELIB_NAME_AW(DeleteFile)
void        WINAPI FatalAppExitA(UINT,LPCSTR);
void        WINAPI FatalAppExitW(UINT,LPCWSTR);
#define     FatalAppExit WINELIB_NAME_AW(FatalAppExit)
ATOM        WINAPI FindAtomA(LPCSTR);
ATOM        WINAPI FindAtomW(LPCWSTR);
#define     FindAtom WINELIB_NAME_AW(FindAtom)
WIN_BOOL      WINAPI FindClose(HANDLE);
HANDLE16    WINAPI FindFirstFile16(LPCSTR,LPWIN32_FIND_DATAA);
HANDLE    WINAPI FindFirstFileA(LPCSTR,LPWIN32_FIND_DATAA);
HANDLE    WINAPI FindFirstFileW(LPCWSTR,LPWIN32_FIND_DATAW);
#define     FindFirstFile WINELIB_NAME_AW(FindFirstFile)
WIN_BOOL16      WINAPI FindNextFile16(HANDLE16,LPWIN32_FIND_DATAA);
WIN_BOOL      WINAPI FindNextFileA(HANDLE,LPWIN32_FIND_DATAA);
WIN_BOOL      WINAPI FindNextFileW(HANDLE,LPWIN32_FIND_DATAW);
#define     FindNextFile WINELIB_NAME_AW(FindNextFile)
HRSRC     WINAPI FindResourceA(HMODULE,LPCSTR,LPCSTR);
HRSRC     WINAPI FindResourceW(HMODULE,LPCWSTR,LPCWSTR);
#define     FindResource WINELIB_NAME_AW(FindResource)
VOID        WINAPI FreeLibrary16(HINSTANCE16);
WIN_BOOL      WINAPI FreeLibrary(HMODULE);
#define     FreeModule(handle) FreeLibrary(handle)
#define     FreeProcInstance(proc) /*nothing*/
WIN_BOOL      WINAPI FreeResource(HGLOBAL);
UINT      WINAPI GetAtomNameA(ATOM,LPSTR,INT);
UINT      WINAPI GetAtomNameW(ATOM,LPWSTR,INT);
#define     GetAtomName WINELIB_NAME_AW(GetAtomName)
UINT      WINAPI GetCurrentDirectoryA(UINT,LPSTR);
UINT      WINAPI GetCurrentDirectoryW(UINT,LPWSTR);
#define     GetCurrentDirectory WINELIB_NAME_AW(GetCurrentDirectory)
WIN_BOOL      WINAPI GetDiskFreeSpaceA(LPCSTR,LPDWORD,LPDWORD,LPDWORD,LPDWORD);
WIN_BOOL      WINAPI GetDiskFreeSpaceW(LPCWSTR,LPDWORD,LPDWORD,LPDWORD,LPDWORD);
#define     GetDiskFreeSpace WINELIB_NAME_AW(GetDiskFreeSpace)
WIN_BOOL      WINAPI GetDiskFreeSpaceExA(LPCSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER);
WIN_BOOL      WINAPI GetDiskFreeSpaceExW(LPCWSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER);
#define     GetDiskFreeSpaceEx WINELIB_NAME_AW(GetDiskFreeSpaceEx)
UINT      WINAPI GetDriveTypeA(LPCSTR);
UINT      WINAPI GetDriveTypeW(LPCWSTR);
#define     GetDriveType WINELIB_NAME_AW(GetDriveType)
DWORD       WINAPI GetFileAttributesA(LPCSTR);
DWORD       WINAPI GetFileAttributesW(LPCWSTR);
#define     GetFileAttributes WINELIB_NAME_AW(GetFileAttributes)
#define     GetFreeSpace(w) (0x100000L)
UINT      WINAPI GetLogicalDriveStringsA(UINT,LPSTR);
UINT      WINAPI GetLogicalDriveStringsW(UINT,LPWSTR);
#define     GetLogicalDriveStrings WINELIB_NAME_AW(GetLogicalDriveStrings)
INT       WINAPI GetLocaleInfoA(LCID,LCTYPE,LPSTR,INT);
INT       WINAPI GetLocaleInfoW(LCID,LCTYPE,LPWSTR,INT);
#define     GetLocaleInfo WINELIB_NAME_AW(GetLocaleInfo)
DWORD       WINAPI GetModuleFileNameA(HMODULE,LPSTR,DWORD);
DWORD       WINAPI GetModuleFileNameW(HMODULE,LPWSTR,DWORD);
#define     GetModuleFileName WINELIB_NAME_AW(GetModuleFileName)
HMODULE     WINAPI GetModuleHandleA(LPCSTR);
HMODULE     WINAPI GetModuleHandleW(LPCWSTR);
#define     GetModuleHandle WINELIB_NAME_AW(GetModuleHandle)
WIN_BOOL        WINAPI GetOverlappedResult(HANDLE,LPOVERLAPPED,LPDWORD,WIN_BOOL);
UINT        WINAPI GetPrivateProfileIntA(LPCSTR,LPCSTR,INT,LPCSTR);
UINT        WINAPI GetPrivateProfileIntW(LPCWSTR,LPCWSTR,INT,LPCWSTR);
#define     GetPrivateProfileInt WINELIB_NAME_AW(GetPrivateProfileInt)
INT         WINAPI GetPrivateProfileSectionA(LPCSTR,LPSTR,DWORD,LPCSTR);
INT         WINAPI GetPrivateProfileSectionW(LPCWSTR,LPWSTR,DWORD,LPCWSTR);
#define     GetPrivateProfileSection WINELIB_NAME_AW(GetPrivateProfileSection)
DWORD       WINAPI GetPrivateProfileSectionNamesA(LPSTR,DWORD,LPCSTR);
DWORD       WINAPI GetPrivateProfileSectionNamesW(LPWSTR,DWORD,LPCWSTR);
#define     GetPrivateProfileSectionNames WINELIB_NAME_AW(GetPrivateProfileSectionNames)
INT       WINAPI GetPrivateProfileStringA(LPCSTR,LPCSTR,LPCSTR,LPSTR,UINT,LPCSTR);
INT       WINAPI GetPrivateProfileStringW(LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,UINT,LPCWSTR);
#define     GetPrivateProfileString WINELIB_NAME_AW(GetPrivateProfileString)
WIN_BOOL      WINAPI GetPrivateProfileStructA(LPCSTR,LPCSTR,LPVOID,UINT,LPCSTR);
WIN_BOOL      WINAPI GetPrivateProfileStructW(LPCWSTR,LPCWSTR,LPVOID,UINT,LPCWSTR);
#define     GetPrivateProfileStruct WINELIB_NAME_AW(GetPrivateProfileStruct)
FARPROC   WINAPI GetProcAddress(HMODULE,LPCSTR);
UINT      WINAPI GetProfileIntA(LPCSTR,LPCSTR,INT);
UINT      WINAPI GetProfileIntW(LPCWSTR,LPCWSTR,INT);
#define     GetProfileInt WINELIB_NAME_AW(GetProfileInt)
INT       WINAPI GetProfileSectionA(LPCSTR,LPSTR,DWORD);
INT       WINAPI GetProfileSectionW(LPCWSTR,LPWSTR,DWORD);
#define     GetProfileSection WINELIB_NAME_AW(GetProfileSection)
INT       WINAPI GetProfileStringA(LPCSTR,LPCSTR,LPCSTR,LPSTR,UINT);
INT       WINAPI GetProfileStringW(LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,UINT);
#define     GetProfileString WINELIB_NAME_AW(GetProfileString)
VOID        WINAPI GetStartupInfoA(LPSTARTUPINFOA);
VOID        WINAPI GetStartupInfoW(LPSTARTUPINFOW);
#define     GetStartupInfo WINELIB_NAME_AW(GetStartupInfo)
WIN_BOOL      WINAPI GetStringTypeA(LCID,DWORD,LPCSTR,INT,LPWORD);
WIN_BOOL      WINAPI GetStringTypeW(DWORD,LPCWSTR,INT,LPWORD);
#define     GetStringType WINELIB_NAME_AW(GetStringType)
UINT      WINAPI GetSystemDirectoryA(LPSTR,UINT);
UINT      WINAPI GetSystemDirectoryW(LPWSTR,UINT);
#define     GetSystemDirectory WINELIB_NAME_AW(GetSystemDirectory)
UINT      WINAPI GetTempFileNameA(LPCSTR,LPCSTR,UINT,LPSTR);
UINT      WINAPI GetTempFileNameW(LPCWSTR,LPCWSTR,UINT,LPWSTR);
#define     GetTempFileName WINELIB_NAME_AW(GetTempFileName)
UINT      WINAPI GetTempPathA(UINT,LPSTR);
UINT      WINAPI GetTempPathW(UINT,LPWSTR);
#define     GetTempPath WINELIB_NAME_AW(GetTempPath)
LONG        WINAPI GetVersion(void);
WIN_BOOL      WINAPI GetExitCodeProcess(HANDLE,LPDWORD);
WIN_BOOL      WINAPI GetVolumeInformationA(LPCSTR,LPSTR,DWORD,LPDWORD,LPDWORD,LPDWORD,LPSTR,DWORD);
WIN_BOOL      WINAPI GetVolumeInformationW(LPCWSTR,LPWSTR,DWORD,LPDWORD,LPDWORD,LPDWORD,LPWSTR,DWORD);
#define     GetVolumeInformation WINELIB_NAME_AW(GetVolumeInformation)
UINT      WINAPI GetWindowsDirectoryA(LPSTR,UINT);
UINT      WINAPI GetWindowsDirectoryW(LPWSTR,UINT);
#define     GetWindowsDirectory WINELIB_NAME_AW(GetWindowsDirectory)
HGLOBAL16   WINAPI GlobalAlloc16(UINT16,DWORD);
HGLOBAL   WINAPI GlobalAlloc(UINT,DWORD);
DWORD       WINAPI GlobalCompact(DWORD);
UINT      WINAPI GlobalFlags(HGLOBAL);
HGLOBAL16   WINAPI GlobalFree16(HGLOBAL16);
HGLOBAL   WINAPI GlobalFree(HGLOBAL);
HGLOBAL   WINAPI GlobalHandle(LPCVOID);
WORD        WINAPI GlobalFix16(HGLOBAL16);
VOID        WINAPI GlobalFix(HGLOBAL);
LPVOID      WINAPI GlobalLock16(HGLOBAL16);
LPVOID      WINAPI GlobalLock(HGLOBAL);
HGLOBAL   WINAPI GlobalReAlloc(HGLOBAL,DWORD,UINT);
DWORD       WINAPI GlobalSize16(HGLOBAL16);
DWORD       WINAPI GlobalSize(HGLOBAL);
VOID        WINAPI GlobalUnfix16(HGLOBAL16);
VOID        WINAPI GlobalUnfix(HGLOBAL);
WIN_BOOL16      WINAPI GlobalUnlock16(HGLOBAL16);
WIN_BOOL      WINAPI GlobalUnlock(HGLOBAL);
WIN_BOOL16      WINAPI GlobalUnWire16(HGLOBAL16);
WIN_BOOL      WINAPI GlobalUnWire(HGLOBAL);
SEGPTR      WINAPI GlobalWire16(HGLOBAL16);
LPVOID      WINAPI GlobalWire(HGLOBAL);
WIN_BOOL      WINAPI InitAtomTable(DWORD);
WIN_BOOL      WINAPI IsBadCodePtr(FARPROC);
WIN_BOOL      WINAPI IsBadHugeReadPtr(LPCVOID,UINT);
WIN_BOOL      WINAPI IsBadHugeWritePtr(LPVOID,UINT);
WIN_BOOL      WINAPI IsBadReadPtr(LPCVOID,UINT);
WIN_BOOL      WINAPI IsBadStringPtrA(LPCSTR,UINT);
WIN_BOOL      WINAPI IsBadStringPtrW(LPCWSTR,UINT);
#define     IsBadStringPtr WINELIB_NAME_AW(IsBadStringPtr)
WIN_BOOL        WINAPI IsBadWritePtr(LPVOID,UINT);
WIN_BOOL        WINAPI IsDBCSLeadByte(BYTE);
WIN_BOOL        WINAPI IsDebuggerPresent(void);
HINSTANCE16 WINAPI LoadLibrary16(LPCSTR);
HMODULE   WINAPI LoadLibraryA(LPCSTR);
HMODULE   WINAPI LoadLibraryW(LPCWSTR);
#define     LoadLibrary WINELIB_NAME_AW(LoadLibrary)
HMODULE   WINAPI LoadLibraryExA(LPCSTR,HANDLE,DWORD);
HMODULE   WINAPI LoadLibraryExW(LPCWSTR,HANDLE,DWORD);
#define     LoadLibraryEx WINELIB_NAME_AW(LoadLibraryEx)
HINSTANCE16 WINAPI LoadModule16(LPCSTR,LPVOID);
HINSTANCE WINAPI LoadModule(LPCSTR,LPVOID);
HGLOBAL   WINAPI LoadResource(HMODULE,HRSRC);
HLOCAL    WINAPI LocalAlloc(UINT,DWORD);
UINT      WINAPI LocalCompact(UINT);
UINT      WINAPI LocalFlags(HLOCAL);
HLOCAL    WINAPI LocalFree(HLOCAL);
HLOCAL    WINAPI LocalHandle(LPCVOID);
LPVOID      WINAPI LocalLock(HLOCAL);
HLOCAL    WINAPI LocalReAlloc(HLOCAL,DWORD,UINT);
UINT      WINAPI LocalShrink(HGLOBAL,UINT);
UINT      WINAPI LocalSize(HLOCAL);
WIN_BOOL      WINAPI LocalUnlock(HLOCAL);
LPVOID      WINAPI LockResource(HGLOBAL);
#define     LockSegment(handle) GlobalFix((HANDLE)(handle))
#define     MakeProcInstance(proc,inst) (proc)
HFILE16     WINAPI OpenFile16(LPCSTR,OFSTRUCT*,UINT16);
HFILE     WINAPI OpenFile(LPCSTR,OFSTRUCT*,UINT);
VOID        WINAPI OutputDebugStringA(LPCSTR);
VOID        WINAPI OutputDebugStringW(LPCWSTR);
#define     OutputDebugString WINELIB_NAME_AW(OutputDebugString)
WIN_BOOL        WINAPI ReadProcessMemory(HANDLE, LPCVOID, LPVOID, DWORD, LPDWORD);
WIN_BOOL        WINAPI RemoveDirectoryA(LPCSTR);
WIN_BOOL        WINAPI RemoveDirectoryW(LPCWSTR);
#define     RemoveDirectory WINELIB_NAME_AW(RemoveDirectory)
WIN_BOOL      WINAPI SetCurrentDirectoryA(LPCSTR);
WIN_BOOL      WINAPI SetCurrentDirectoryW(LPCWSTR);
#define     SetCurrentDirectory WINELIB_NAME_AW(SetCurrentDirectory)
UINT      WINAPI SetErrorMode(UINT);
WIN_BOOL      WINAPI SetFileAttributesA(LPCSTR,DWORD);
WIN_BOOL      WINAPI SetFileAttributesW(LPCWSTR,DWORD);
#define     SetFileAttributes WINELIB_NAME_AW(SetFileAttributes)
UINT      WINAPI SetHandleCount(UINT);
#define     SetSwapAreaSize(w) (w)
WIN_BOOL        WINAPI SetVolumeLabelA(LPCSTR,LPCSTR);
WIN_BOOL        WINAPI SetVolumeLabelW(LPCWSTR,LPCWSTR);
#define     SetVolumeLabel WINELIB_NAME_AW(SetVolumeLabel)
DWORD       WINAPI SizeofResource(HMODULE,HRSRC);
#define     UnlockSegment(handle) GlobalUnfix((HANDLE)(handle))
WIN_BOOL      WINAPI WritePrivateProfileSectionA(LPCSTR,LPCSTR,LPCSTR);
WIN_BOOL      WINAPI WritePrivateProfileSectionW(LPCWSTR,LPCWSTR,LPCWSTR);
#define     WritePrivateProfileSection WINELIB_NAME_AW(WritePrivateProfileSection)
WIN_BOOL      WINAPI WritePrivateProfileStringA(LPCSTR,LPCSTR,LPCSTR,LPCSTR);
WIN_BOOL      WINAPI WritePrivateProfileStringW(LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR);
#define     WritePrivateProfileString WINELIB_NAME_AW(WritePrivateProfileString)
WIN_BOOL	     WINAPI WriteProfileSectionA(LPCSTR,LPCSTR);
WIN_BOOL	     WINAPI WriteProfileSectionW(LPCWSTR,LPCWSTR);
#define     WritePrivateProfileSection WINELIB_NAME_AW(WritePrivateProfileSection)
WIN_BOOL      WINAPI WritePrivateProfileStructA(LPCSTR,LPCSTR,LPVOID,UINT,LPCSTR);
WIN_BOOL      WINAPI WritePrivateProfileStructW(LPCWSTR,LPCWSTR,LPVOID,UINT,LPCWSTR);
#define     WritePrivateProfileStruct WINELIB_NAME_AW(WritePrivateProfileStruct)
WIN_BOOL        WINAPI WriteProcessMemory(HANDLE, LPVOID, LPVOID, DWORD, LPDWORD);
WIN_BOOL        WINAPI WriteProfileStringA(LPCSTR,LPCSTR,LPCSTR);
WIN_BOOL        WINAPI WriteProfileStringW(LPCWSTR,LPCWSTR,LPCWSTR);
#define     WriteProfileString WINELIB_NAME_AW(WriteProfileString)
#define     Yield32()
LPSTR       WINAPI lstrcatA(LPSTR,LPCSTR);
LPWSTR      WINAPI lstrcatW(LPWSTR,LPCWSTR);
#define     lstrcat WINELIB_NAME_AW(lstrcat)
LPSTR       WINAPI lstrcpyA(LPSTR,LPCSTR);
LPWSTR      WINAPI lstrcpyW(LPWSTR,LPCWSTR);
#define     lstrcpy WINELIB_NAME_AW(lstrcpy)
LPSTR       WINAPI lstrcpynA(LPSTR,LPCSTR,INT);
LPWSTR      WINAPI lstrcpynW(LPWSTR,LPCWSTR,INT);
#define     lstrcpyn WINELIB_NAME_AW(lstrcpyn)
INT       WINAPI lstrlenA(LPCSTR);
INT       WINAPI lstrlenW(LPCWSTR);
#define     lstrlen WINELIB_NAME_AW(lstrlen)
HINSTANCE WINAPI WinExec(LPCSTR,UINT);
LONG        WINAPI _hread(HFILE,LPVOID,LONG);
LONG        WINAPI _hwrite(HFILE,LPCSTR,LONG);
HFILE     WINAPI _lcreat(LPCSTR,INT);
HFILE     WINAPI _lclose(HFILE);
LONG        WINAPI _llseek(HFILE,LONG,INT);
HFILE     WINAPI _lopen(LPCSTR,INT);
UINT      WINAPI _lread(HFILE,LPVOID,UINT);
UINT      WINAPI _lwrite(HFILE,LPCSTR,UINT);
SEGPTR      WINAPI WIN16_GlobalLock16(HGLOBAL16);
INT       WINAPI lstrcmpA(LPCSTR,LPCSTR);
INT       WINAPI lstrcmpW(LPCWSTR,LPCWSTR);
#define     lstrcmp WINELIB_NAME_AW(lstrcmp)
INT       WINAPI lstrcmpiA(LPCSTR,LPCSTR);
INT       WINAPI lstrcmpiW(LPCWSTR,LPCWSTR);
#define     lstrcmpi WINELIB_NAME_AW(lstrcmpi)

/* compatibility macros */
#define     FillMemory RtlFillMemory
#define     MoveMemory RtlMoveMemory
#define     ZeroMemory RtlZeroMemory
#define     CopyMemory RtlCopyMemory

DWORD       WINAPI GetCurrentProcessId(void);
DWORD       WINAPI GetCurrentThreadId(void);
DWORD       WINAPI GetLastError(void);
HANDLE      WINAPI GetProcessHeap(void);
PVOID       WINAPI InterlockedCompareExchange(PVOID*,PVOID,PVOID);
LONG        WINAPI InterlockedDecrement(PLONG);
LONG        WINAPI InterlockedExchange(PLONG,LONG);
LONG        WINAPI InterlockedExchangeAdd(PLONG,LONG);
LONG        WINAPI InterlockedIncrement(PLONG);
VOID        WINAPI SetLastError(DWORD);

#ifdef __WINE__
#define GetCurrentProcess() ((HANDLE)0xffffffff)
#define GetCurrentThread()  ((HANDLE)0xfffffffe)
#endif

#ifdef __cplusplus
}
#endif

#endif  /* __WINE_WINBASE_H */
