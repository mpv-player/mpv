/*
 * Module definitions
 *
 * Copyright 1995 Alexandre Julliard
 */

#ifndef __WINE_MODULE_H
#define __WINE_MODULE_H

#include "windef.h"
//#include "dosexe.h"
#include "pe_image.h"



typedef struct {
    BYTE type;
    BYTE flags;
    BYTE segnum;
    WORD offs WINE_PACKED;
} ET_ENTRY;

typedef struct {
    WORD first; /* ordinal */
    WORD last; /* ordinal */
    WORD next; /* bundle */
} ET_BUNDLE;


  /* In-memory segment table */
typedef struct
{
    WORD      filepos;   /* Position in file, in sectors */
    WORD      size;      /* Segment size on disk */
    WORD      flags;     /* Segment flags */
    WORD      minsize;   /* Min. size of segment in memory */
    HANDLE16  hSeg;      /* Selector or handle (selector - 1) */
                         /* of segment in memory */
} SEGTABLEENTRY;


  /* Self-loading modules contain this structure in their first segment */

#include "pshpack1.h"

typedef struct
{
    WORD      version;       /* Must be "A0" (0x3041) */
    WORD      reserved;
    FARPROC16 BootApp;       /* startup procedure */
    FARPROC16 LoadAppSeg;    /* procedure to load a segment */
    FARPROC16 reserved2;
    FARPROC16 MyAlloc;       /* memory allocation procedure, 
                              * wine must write this field */
    FARPROC16 EntryAddrProc;
    FARPROC16 ExitProc;      /* exit procedure */
    WORD      reserved3[4];
    FARPROC16 SetOwner;      /* Set Owner procedure, exported by wine */
} SELFLOADHEADER;

  /* Parameters for LoadModule() */
typedef struct
{
    HGLOBAL16 hEnvironment;         /* Environment segment */
    SEGPTR    cmdLine WINE_PACKED;  /* Command-line */
    SEGPTR    showCmd WINE_PACKED;  /* Code for ShowWindow() */
    SEGPTR    reserved WINE_PACKED;
} LOADPARAMS16;

typedef struct 
{
    LPSTR lpEnvAddress;
    LPSTR lpCmdLine;
    UINT16 *lpCmdShow;
    DWORD dwReserved;
} LOADPARAMS;

#include "poppack.h"

/* internal representation of 32bit modules. per process. */
typedef enum {
	MODULE32_PE = 1,
	MODULE32_ELF,
	MODULE32_ELFDLL
} MODULE32_TYPE;

typedef struct _wine_modref
{
	struct _wine_modref	*next;
	struct _wine_modref	*prev;
	MODULE32_TYPE		type;
	union {
		PE_MODREF	pe;
		ELF_MODREF	elf;
	} binfmt;

	HMODULE			module;

	int			nDeps;
	struct _wine_modref	**deps;

	int			flags;
	int			refCount;

	char			*filename;
	char			*modname;
	char			*short_filename;
	char			*short_modname;
} WINE_MODREF;

#define WINE_MODREF_INTERNAL              0x00000001
#define WINE_MODREF_NO_DLL_CALLS          0x00000002
#define WINE_MODREF_PROCESS_ATTACHED      0x00000004
#define WINE_MODREF_LOAD_AS_DATAFILE      0x00000010
#define WINE_MODREF_DONT_RESOLVE_REFS     0x00000020
#define WINE_MODREF_MARKER                0x80000000



/* Resource types */
typedef struct resource_typeinfo_s NE_TYPEINFO;
typedef struct resource_nameinfo_s NE_NAMEINFO;

#define NE_SEG_TABLE(pModule) \
    ((SEGTABLEENTRY *)((char *)(pModule) + (pModule)->seg_table))

#define NE_MODULE_TABLE(pModule) \
    ((WORD *)((char *)(pModule) + (pModule)->modref_table))

#define NE_MODULE_NAME(pModule) \
    (((OFSTRUCT *)((char*)(pModule) + (pModule)->fileinfo))->szPathName)

/* module.c */
extern FARPROC MODULE_GetProcAddress( HMODULE hModule, LPCSTR function, WIN_BOOL snoop );
extern WINE_MODREF *MODULE32_LookupHMODULE( HMODULE hModule );
extern WIN_BOOL MODULE_DllProcessAttach( WINE_MODREF *wm, LPVOID lpReserved );
extern void MODULE_DllProcessDetach( WINE_MODREF *wm, WIN_BOOL bForceDetach, LPVOID lpReserved );
extern void MODULE_DllThreadAttach( LPVOID lpReserved );
extern void MODULE_DllThreadDetach( LPVOID lpReserved );
extern WINE_MODREF *MODULE_LoadLibraryExA( LPCSTR libname, HFILE hfile, DWORD flags );
extern WIN_BOOL MODULE_FreeLibrary( WINE_MODREF *wm );
extern WINE_MODREF *MODULE_FindModule( LPCSTR path );
extern HMODULE MODULE_CreateDummyModule( LPCSTR filename, HMODULE module32 );
extern FARPROC16 WINAPI WIN32_GetProcAddress16( HMODULE hmodule, LPCSTR name );
extern SEGPTR WINAPI HasGPHandler16( SEGPTR address );
extern void MODULE_WalkModref( DWORD id );

/* resource.c */
extern INT       WINAPI AccessResource(HMODULE,HRSRC); 
/*
/ loader/ne/module.c 
extern NE_MODULE *NE_GetPtr( HMODULE16 hModule );
extern void NE_DumpModule( HMODULE16 hModule );
extern void NE_WalkModules(void);
extern void NE_RegisterModule( NE_MODULE *pModule );
extern WORD NE_GetOrdinal( HMODULE16 hModule, const char *name );
extern FARPROC16 WINAPI NE_GetEntryPoint( HMODULE16 hModule, WORD ordinal );
extern FARPROC16 NE_GetEntryPointEx( HMODULE16 hModule, WORD ordinal, WIN_BOOL16 snoop );
extern WIN_BOOL16 NE_SetEntryPoint( HMODULE16 hModule, WORD ordinal, WORD offset );
extern int NE_OpenFile( NE_MODULE *pModule );
extern WIN_BOOL NE_CreateProcess( HANDLE hFile, LPCSTR filename, LPCSTR cmd_line, LPCSTR env, 
                              LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa,
                              WIN_BOOL inherit, DWORD flags, LPSTARTUPINFOA startup,
                              LPPROCESS_INFORMATION info );
extern WIN_BOOL NE_InitProcess( NE_MODULE *pModule );


/ loader/ne/resource.c 
extern HGLOBAL16 WINAPI NE_DefResourceHandler(HGLOBAL16,HMODULE16,HRSRC16);
extern WIN_BOOL NE_InitResourceHandler( HMODULE16 hModule );
extern HRSRC16 NE_FindResource( NE_MODULE *pModule, LPCSTR name, LPCSTR type );
extern INT16 NE_AccessResource( NE_MODULE *pModule, HRSRC16 hRsrc );
extern DWORD NE_SizeofResource( NE_MODULE *pModule, HRSRC16 hRsrc );
extern HGLOBAL16 NE_LoadResource( NE_MODULE *pModule, HRSRC16 hRsrc );
extern WIN_BOOL16 NE_FreeResource( NE_MODULE *pModule, HGLOBAL16 handle );
extern NE_TYPEINFO *NE_FindTypeSection( LPBYTE pResTab, NE_TYPEINFO *pTypeInfo, LPCSTR typeId );
extern NE_NAMEINFO *NE_FindResourceFromType( LPBYTE pResTab, NE_TYPEINFO *pTypeInfo, LPCSTR resId );

// loader/ne/segment.c
extern WIN_BOOL NE_LoadSegment( NE_MODULE *pModule, WORD segnum );
extern WIN_BOOL NE_LoadAllSegments( NE_MODULE *pModule );
extern WIN_BOOL NE_CreateSegment( NE_MODULE *pModule, int segnum );
extern WIN_BOOL NE_CreateAllSegments( NE_MODULE *pModule );
extern HINSTANCE16 NE_GetInstance( NE_MODULE *pModule );
extern void NE_InitializeDLLs( HMODULE16 hModule );
extern void NE_DllProcessAttach( HMODULE16 hModule );

// loader/ne/convert.c 
HGLOBAL16 NE_LoadPEResource( NE_MODULE *pModule, WORD type, LPVOID bits, DWORD size );
*/
/* relay32/builtin.c */
extern WINE_MODREF *BUILTIN32_LoadLibraryExA(LPCSTR name, DWORD flags);
extern HMODULE BUILTIN32_LoadExeModule( LPCSTR *filename );
extern void BUILTIN32_UnloadLibrary(WINE_MODREF *wm);
extern void *BUILTIN32_dlopen( const char *name );
extern int BUILTIN32_dlclose( void *handle );

#endif  /* __WINE_MODULE_H */
