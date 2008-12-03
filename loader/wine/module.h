/*
 * Module definitions
 *
 * Copyright 1995 Alexandre Julliard
 */

#ifndef MPLAYER_MODULE_H
#define MPLAYER_MODULE_H

#include "windef.h"
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

typedef struct wine_modref
{
	struct wine_modref	*next;
	struct wine_modref	*prev;
	MODULE32_TYPE		type;
	union {
		PE_MODREF	pe;
		ELF_MODREF	elf;
	} binfmt;

	HMODULE			module;

	int			nDeps;
	struct wine_modref	**deps;

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

struct modref_list_t;

typedef struct modref_list_t
{
    WINE_MODREF* wm;
    struct modref_list_t *next;
    struct modref_list_t *prev;
} modref_list;


/* module.c */
FARPROC MODULE_GetProcAddress( HMODULE hModule, LPCSTR function, WIN_BOOL snoop );
WINE_MODREF *MODULE32_LookupHMODULE( HMODULE hModule );
WINE_MODREF *MODULE_FindModule( LPCSTR path );

/* resource.c */
INT WINAPI AccessResource( HMODULE, HRSRC );

#endif /* MPLAYER_MODULE_H */
