#ifndef MPLAYER_NTDEF_H
#define MPLAYER_NTDEF_H

#include "basetsd.h"
#include "windef.h"

#include "pshpack1.h"

#define NTAPI   __stdcall

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef OPTIONAL
#define OPTIONAL
#endif

#ifndef VOID
#define VOID void
#endif

typedef LONG NTSTATUS;
typedef NTSTATUS *PNTSTATUS;

typedef short CSHORT;
typedef CSHORT *PCSHORT;

typedef WCHAR * PWCHAR;

/* NT lowlevel Strings (handled by Rtl* functions in NTDLL)
 * If they are zero terminated, Length does not include the terminating 0.
 */

typedef struct STRING {
	USHORT	Length;
	USHORT	MaximumLength;
	PSTR	Buffer;
} STRING,*PSTRING,ANSI_STRING,*PANSI_STRING;

typedef struct CSTRING {
	USHORT	Length;
	USHORT	MaximumLength;
	PCSTR	Buffer;
} CSTRING,*PCSTRING;

typedef struct UNICODE_STRING {
	USHORT	Length;		/* bytes */
	USHORT	MaximumLength;	/* bytes */
	PWSTR	Buffer;
} UNICODE_STRING,*PUNICODE_STRING;

/*
	Objects
*/

#define OBJ_INHERIT             0x00000002L
#define OBJ_PERMANENT           0x00000010L
#define OBJ_EXCLUSIVE           0x00000020L
#define OBJ_CASE_INSENSITIVE    0x00000040L
#define OBJ_OPENIF              0x00000080L
#define OBJ_OPENLINK            0x00000100L
#define OBJ_KERNEL_HANDLE       0x00000200L
#define OBJ_VALID_ATTRIBUTES    0x000003F2L

typedef struct OBJECT_ATTRIBUTES
{   ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;        /* type SECURITY_DESCRIPTOR */
    PVOID SecurityQualityOfService;  /* type SECURITY_QUALITY_OF_SERVICE */
} OBJECT_ATTRIBUTES;

typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

#define InitializeObjectAttributes(p,n,a,r,s) \
{	(p)->Length = sizeof(OBJECT_ATTRIBUTES); \
	(p)->RootDirectory = r; \
	(p)->Attributes = a; \
	(p)->ObjectName = n; \
	(p)->SecurityDescriptor = s; \
	(p)->SecurityQualityOfService = NULL; \
}


#include "poppack.h"

#endif /* MPLAYER_NTDEF_H */
