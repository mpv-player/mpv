#ifndef MPLAYER_PE_IMAGE_H
#define MPLAYER_PE_IMAGE_H

#include "winnt.h"
#include "winbase.h"

#define PE_HEADER(module) \
    ((IMAGE_NT_HEADERS*)((LPBYTE)(module) + \
                         (((IMAGE_DOS_HEADER*)(module))->e_lfanew)))

#define PE_SECTIONS(module) \
    ((IMAGE_SECTION_HEADER*)((LPBYTE)&PE_HEADER(module)->OptionalHeader + \
                           PE_HEADER(module)->FileHeader.SizeOfOptionalHeader))

#define RVA_PTR(module,field) ((LPBYTE)(module) + PE_HEADER(module)->field)

/* modreference used for attached processes
 * all section are calculated here, relocations etc.
 */
typedef struct {
	PIMAGE_IMPORT_DESCRIPTOR	pe_import;
	PIMAGE_EXPORT_DIRECTORY	pe_export;
	PIMAGE_RESOURCE_DIRECTORY	pe_resource;
	int				tlsindex;
} PE_MODREF;

struct wine_modref;
int PE_unloadImage(HMODULE hModule);
FARPROC PE_FindExportedFunction(struct wine_modref *wm, LPCSTR funcName, WIN_BOOL snoop);
WIN_BOOL PE_EnumResourceTypesA(HMODULE, ENUMRESTYPEPROCA, LONG);
WIN_BOOL PE_EnumResourceTypesW(HMODULE, ENUMRESTYPEPROCW, LONG);
WIN_BOOL PE_EnumResourceNamesA(HMODULE, LPCSTR, ENUMRESNAMEPROCA, LONG);
WIN_BOOL PE_EnumResourceNamesW(HMODULE, LPCWSTR, ENUMRESNAMEPROCW, LONG);
WIN_BOOL PE_EnumResourceLanguagesA(HMODULE, LPCSTR, LPCSTR, ENUMRESLANGPROCA, LONG);
WIN_BOOL PE_EnumResourceLanguagesW(HMODULE, LPCWSTR, LPCWSTR, ENUMRESLANGPROCW, LONG);
HRSRC PE_FindResourceExW(struct wine_modref*, LPCWSTR, LPCWSTR, WORD);
DWORD PE_SizeofResource(HMODULE, HRSRC);
struct wine_modref *PE_LoadLibraryExA(LPCSTR, DWORD);
void PE_UnloadLibrary(struct wine_modref *);
HGLOBAL PE_LoadResource(struct wine_modref *wm, HRSRC);
HMODULE PE_LoadImage(int hFile, LPCSTR filename, WORD *version);
struct wine_modref *PE_CreateModule(HMODULE hModule, LPCSTR filename,
                                    DWORD flags, WIN_BOOL builtin);
WIN_BOOL PE_CreateProcess(HANDLE hFile, LPCSTR filename, LPCSTR cmd_line, LPCSTR env,
                          LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa,
                          WIN_BOOL inherit, DWORD flags, LPSTARTUPINFOA startup,
                          LPPROCESS_INFORMATION info);

void PE_InitTls(void);
WIN_BOOL PE_InitDLL(struct wine_modref *wm, DWORD type, LPVOID lpReserved);

PIMAGE_RESOURCE_DIRECTORY GetResDirEntryA(PIMAGE_RESOURCE_DIRECTORY, LPCSTR, DWORD, WIN_BOOL);
PIMAGE_RESOURCE_DIRECTORY GetResDirEntryW(PIMAGE_RESOURCE_DIRECTORY, LPCWSTR, DWORD, WIN_BOOL);

typedef DWORD CALLBACK (*DLLENTRYPROC)(HMODULE,DWORD,LPVOID);

typedef struct {
	WORD	popl	WINE_PACKED;	/* 0x8f 0x05 */
	DWORD	addr_popped WINE_PACKED;/* ...  */
	BYTE	pushl1	WINE_PACKED;	/* 0x68 */
	DWORD	newret WINE_PACKED;	/* ...  */
	BYTE	pushl2 	WINE_PACKED;	/* 0x68 */
	DWORD	origfun WINE_PACKED;	/* original function */
	BYTE	ret1	WINE_PACKED;	/* 0xc3 */
	WORD	addesp 	WINE_PACKED;	/* 0x83 0xc4 */
	BYTE	nrofargs WINE_PACKED;	/* nr of arguments to add esp, */
	BYTE	pushl3	WINE_PACKED;	/* 0x68 */
	DWORD	oldret	WINE_PACKED;	/* Filled out from popl above  */
	BYTE	ret2	WINE_PACKED;	/* 0xc3 */
} ELF_STDCALL_STUB;

typedef struct {
	void*			dlhandle;
	ELF_STDCALL_STUB	*stubs;
} ELF_MODREF;

struct wine_modref *ELF_LoadLibraryExA( LPCSTR libname, DWORD flags);
void ELF_UnloadLibrary(struct wine_modref *);
FARPROC ELF_FindExportedFunction(struct wine_modref *wm, LPCSTR funcName);

#endif /* MPLAYER_PE_IMAGE_H */
