/*
 * PE (Portable Execute) File Resources
 *
 * Copyright 1995 Thomas Sandford
 * Copyright 1996 Martin von Loewis
 *
 * Based on the Win16 resource handling code in loader/resource.c
 * Copyright 1993 Robert J. Amstadt
 * Copyright 1995 Alexandre Julliard
 * Copyright 1997 Marcus Meissner
 *
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 *
 */
#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include "wine/winestring.h"
#include "wine/windef.h"
#include "wine/pe_image.h"
#include "wine/module.h"
#include "wine/heap.h"
//#include "task.h"
//#include "process.h"
//#include "stackframe.h"
#include "wine/debugtools.h"
#include "ext.h"

/**********************************************************************
 *  HMODULE32toPE_MODREF 
 *
 * small helper function to get a PE_MODREF from a passed HMODULE32
 */
static PE_MODREF*
HMODULE32toPE_MODREF(HMODULE hmod) {
	WINE_MODREF	*wm;

	wm = MODULE32_LookupHMODULE( hmod );
	if (!wm || wm->type!=MODULE32_PE)
		return NULL;
	return &(wm->binfmt.pe);
}

/**********************************************************************
 *	    GetResDirEntryW
 *
 *	Helper function - goes down one level of PE resource tree
 *
 */
PIMAGE_RESOURCE_DIRECTORY GetResDirEntryW(PIMAGE_RESOURCE_DIRECTORY resdirptr,
					   LPCWSTR name,DWORD root,
					   WIN_BOOL allowdefault)
{
    int entrynum;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY entryTable;
    int namelen;

    if (HIWORD(name)) {
    	if (name[0]=='#') {
		char	buf[10];

		lstrcpynWtoA(buf,name+1,10);
		return GetResDirEntryW(resdirptr,(LPCWSTR)atoi(buf),root,allowdefault);
	}
	entryTable = (PIMAGE_RESOURCE_DIRECTORY_ENTRY) (
			(BYTE *) resdirptr + 
                        sizeof(IMAGE_RESOURCE_DIRECTORY));
	namelen = lstrlenW(name);
	for (entrynum = 0; entrynum < resdirptr->NumberOfNamedEntries; entrynum++)
	{
		PIMAGE_RESOURCE_DIR_STRING_U str =
		(PIMAGE_RESOURCE_DIR_STRING_U) (root + 
			entryTable[entrynum].u1.s.NameOffset);
		if(namelen != str->Length)
			continue;
		if(wcsnicmp(name,str->NameString,str->Length)==0)
			return (PIMAGE_RESOURCE_DIRECTORY) (
				root +
				entryTable[entrynum].u2.s.OffsetToDirectory);
	}
	return NULL;
    } else {
	entryTable = (PIMAGE_RESOURCE_DIRECTORY_ENTRY) (
			(BYTE *) resdirptr + 
                        sizeof(IMAGE_RESOURCE_DIRECTORY) +
			resdirptr->NumberOfNamedEntries * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
	for (entrynum = 0; entrynum < resdirptr->NumberOfIdEntries; entrynum++)
	    if ((DWORD)entryTable[entrynum].u1.Name == (DWORD)name)
		return (PIMAGE_RESOURCE_DIRECTORY) (
			root +
			entryTable[entrynum].u2.s.OffsetToDirectory);
	/* just use first entry if no default can be found */
	if (allowdefault && !name && resdirptr->NumberOfIdEntries)
		return (PIMAGE_RESOURCE_DIRECTORY) (
			root +
			entryTable[0].u2.s.OffsetToDirectory);
	return NULL;
    }
}

/**********************************************************************
 *	    GetResDirEntryA
 */
PIMAGE_RESOURCE_DIRECTORY GetResDirEntryA( PIMAGE_RESOURCE_DIRECTORY resdirptr,
					   LPCSTR name, DWORD root,
					   WIN_BOOL allowdefault )
{
    PIMAGE_RESOURCE_DIRECTORY retv;
    LPWSTR nameW = HIWORD(name)? HEAP_strdupAtoW( GetProcessHeap(), 0, name ) 
                               : (LPWSTR)name;

    retv = GetResDirEntryW( resdirptr, nameW, root, allowdefault );

    if ( HIWORD(name) ) HeapFree( GetProcessHeap(), 0, nameW );

    return retv;
}

/**********************************************************************
 *	    PE_FindResourceEx32W
 */
HANDLE PE_FindResourceExW(
	WINE_MODREF *wm,LPCWSTR name,LPCWSTR type,WORD lang
) {
    PIMAGE_RESOURCE_DIRECTORY resdirptr;
    DWORD root;
    HANDLE result;
    PE_MODREF	*pem = &(wm->binfmt.pe);

    if (!pem || !pem->pe_resource)
    	return 0;

    resdirptr = pem->pe_resource;
    root = (DWORD) resdirptr;
    if ((resdirptr = GetResDirEntryW(resdirptr, type, root, FALSE)) == NULL)
	return 0;
    if ((resdirptr = GetResDirEntryW(resdirptr, name, root, FALSE)) == NULL)
	return 0;
    result = (HANDLE)GetResDirEntryW(resdirptr, (LPCWSTR)(UINT)lang, root, FALSE);
	/* Try LANG_NEUTRAL, too */
    if(!result)
        return (HANDLE)GetResDirEntryW(resdirptr, (LPCWSTR)0, root, TRUE);
    return result;
}


/**********************************************************************
 *	    PE_LoadResource32
 */
HANDLE PE_LoadResource( WINE_MODREF *wm, HANDLE hRsrc )
{
    if (!hRsrc || !wm || wm->type!=MODULE32_PE)
    	return 0;
    return (HANDLE) (wm->module + ((PIMAGE_RESOURCE_DATA_ENTRY)hRsrc)->OffsetToData);
}


/**********************************************************************
 *	    PE_SizeofResource32
 */
DWORD PE_SizeofResource( HINSTANCE hModule, HANDLE hRsrc )
{
    /* we don't need hModule */
    if (!hRsrc)
   	 return 0;
    return ((PIMAGE_RESOURCE_DATA_ENTRY)hRsrc)->Size;
}

/**********************************************************************
 *	    PE_EnumResourceTypes32A
 */
WIN_BOOL
PE_EnumResourceTypesA(HMODULE hmod,ENUMRESTYPEPROCA lpfun,LONG lparam) {
    PE_MODREF	*pem = HMODULE32toPE_MODREF(hmod);
    int		i;
    PIMAGE_RESOURCE_DIRECTORY		resdir;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY	et;
    WIN_BOOL	ret;
    HANDLE	heap = GetProcessHeap();	

    if (!pem || !pem->pe_resource)
    	return FALSE;

    resdir = (PIMAGE_RESOURCE_DIRECTORY)pem->pe_resource;
    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)((LPBYTE)resdir+sizeof(IMAGE_RESOURCE_DIRECTORY));
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
    	LPSTR	name;

	if (et[i].u1.s.NameIsString)
		name = HEAP_strdupWtoA(heap,0,(LPWSTR)((LPBYTE)pem->pe_resource+et[i].u1.s.NameOffset));
	else
		name = (LPSTR)(int)et[i].u1.Id;
	ret = lpfun(hmod,name,lparam);
	if (HIWORD(name))
		HeapFree(heap,0,name);
	if (!ret)
		break;
    }
    return ret;
}

/**********************************************************************
 *	    PE_EnumResourceTypes32W
 */
WIN_BOOL
PE_EnumResourceTypesW(HMODULE hmod,ENUMRESTYPEPROCW lpfun,LONG lparam) {
    PE_MODREF	*pem = HMODULE32toPE_MODREF(hmod);
    int		i;
    PIMAGE_RESOURCE_DIRECTORY		resdir;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY	et;
    WIN_BOOL	ret;

    if (!pem || !pem->pe_resource)
    	return FALSE;

    resdir = (PIMAGE_RESOURCE_DIRECTORY)pem->pe_resource;
    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)((LPBYTE)resdir+sizeof(IMAGE_RESOURCE_DIRECTORY));
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
	LPWSTR	type;
    	if (et[i].u1.s.NameIsString)
		type = (LPWSTR)((LPBYTE)pem->pe_resource+et[i].u1.s.NameOffset);
	else
		type = (LPWSTR)(int)et[i].u1.Id;

	ret = lpfun(hmod,type,lparam);
	if (!ret)
		break;
    }
    return ret;
}

/**********************************************************************
 *	    PE_EnumResourceNames32A
 */
WIN_BOOL
PE_EnumResourceNamesA(
	HMODULE hmod,LPCSTR type,ENUMRESNAMEPROCA lpfun,LONG lparam
) {
    PE_MODREF	*pem = HMODULE32toPE_MODREF(hmod);
    int		i;
    PIMAGE_RESOURCE_DIRECTORY		resdir;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY	et;
    WIN_BOOL	ret;
    HANDLE	heap = GetProcessHeap();	
    LPWSTR	typeW;

    if (!pem || !pem->pe_resource)
    	return FALSE;
    resdir = (PIMAGE_RESOURCE_DIRECTORY)pem->pe_resource;
    if (HIWORD(type))
	typeW = HEAP_strdupAtoW(heap,0,type);
    else
	typeW = (LPWSTR)type;
    resdir = GetResDirEntryW(resdir,typeW,(DWORD)pem->pe_resource,FALSE);
    if (HIWORD(typeW))
    	HeapFree(heap,0,typeW);
    if (!resdir)
    	return FALSE;
    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)((LPBYTE)resdir+sizeof(IMAGE_RESOURCE_DIRECTORY));
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
    	LPSTR	name;

	if (et[i].u1.s.NameIsString)
	    name = HEAP_strdupWtoA(heap,0,(LPWSTR)((LPBYTE)pem->pe_resource+et[i].u1.s.NameOffset));
	else
	    name = (LPSTR)(int)et[i].u1.Id;
	ret = lpfun(hmod,type,name,lparam);
	if (HIWORD(name)) HeapFree(heap,0,name);
	if (!ret)
		break;
    }
    return ret;
}

/**********************************************************************
 *	    PE_EnumResourceNames32W
 */
WIN_BOOL
PE_EnumResourceNamesW(
	HMODULE hmod,LPCWSTR type,ENUMRESNAMEPROCW lpfun,LONG lparam
) {
    PE_MODREF	*pem = HMODULE32toPE_MODREF(hmod);
    int		i;
    PIMAGE_RESOURCE_DIRECTORY		resdir;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY	et;
    WIN_BOOL	ret;

    if (!pem || !pem->pe_resource)
    	return FALSE;

    resdir = (PIMAGE_RESOURCE_DIRECTORY)pem->pe_resource;
    resdir = GetResDirEntryW(resdir,type,(DWORD)pem->pe_resource,FALSE);
    if (!resdir)
    	return FALSE;
    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)((LPBYTE)resdir+sizeof(IMAGE_RESOURCE_DIRECTORY));
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
	LPWSTR	name;
    	if (et[i].u1.s.NameIsString)
		name = (LPWSTR)((LPBYTE)pem->pe_resource+et[i].u1.s.NameOffset);
	else
		name = (LPWSTR)(int)et[i].u1.Id;
	ret = lpfun(hmod,type,name,lparam);
	if (!ret)
		break;
    }
    return ret;
}

/**********************************************************************
 *	    PE_EnumResourceNames32A
 */
WIN_BOOL
PE_EnumResourceLanguagesA(
	HMODULE hmod,LPCSTR name,LPCSTR type,ENUMRESLANGPROCA lpfun,
	LONG lparam
) {
    PE_MODREF	*pem = HMODULE32toPE_MODREF(hmod);
    int		i;
    PIMAGE_RESOURCE_DIRECTORY		resdir;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY	et;
    WIN_BOOL	ret;
    HANDLE	heap = GetProcessHeap();	
    LPWSTR	nameW,typeW;

    if (!pem || !pem->pe_resource)
    	return FALSE;

    resdir = (PIMAGE_RESOURCE_DIRECTORY)pem->pe_resource;
    if (HIWORD(name))
	nameW = HEAP_strdupAtoW(heap,0,name);
    else
    	nameW = (LPWSTR)name;
    resdir = GetResDirEntryW(resdir,nameW,(DWORD)pem->pe_resource,FALSE);
    if (HIWORD(nameW))
    	HeapFree(heap,0,nameW);
    if (!resdir)
    	return FALSE;
    if (HIWORD(type))
	typeW = HEAP_strdupAtoW(heap,0,type);
    else
	typeW = (LPWSTR)type;
    resdir = GetResDirEntryW(resdir,typeW,(DWORD)pem->pe_resource,FALSE);
    if (HIWORD(typeW))
    	HeapFree(heap,0,typeW);
    if (!resdir)
    	return FALSE;
    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)((LPBYTE)resdir+sizeof(IMAGE_RESOURCE_DIRECTORY));
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
    	/* languages are just ids... I hopem */
	ret = lpfun(hmod,name,type,et[i].u1.Id,lparam);
	if (!ret)
		break;
    }
    return ret;
}

/**********************************************************************
 *	    PE_EnumResourceLanguages32W
 */
WIN_BOOL
PE_EnumResourceLanguagesW(
	HMODULE hmod,LPCWSTR name,LPCWSTR type,ENUMRESLANGPROCW lpfun,
	LONG lparam
) {
    PE_MODREF	*pem = HMODULE32toPE_MODREF(hmod);
    int		i;
    PIMAGE_RESOURCE_DIRECTORY		resdir;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY	et;
    WIN_BOOL	ret;

    if (!pem || !pem->pe_resource)
    	return FALSE;

    resdir = (PIMAGE_RESOURCE_DIRECTORY)pem->pe_resource;
    resdir = GetResDirEntryW(resdir,name,(DWORD)pem->pe_resource,FALSE);
    if (!resdir)
    	return FALSE;
    resdir = GetResDirEntryW(resdir,type,(DWORD)pem->pe_resource,FALSE);
    if (!resdir)
    	return FALSE;
    et =(PIMAGE_RESOURCE_DIRECTORY_ENTRY)((LPBYTE)resdir+sizeof(IMAGE_RESOURCE_DIRECTORY));
    ret = FALSE;
    for (i=0;i<resdir->NumberOfNamedEntries+resdir->NumberOfIdEntries;i++) {
	ret = lpfun(hmod,name,type,et[i].u1.Id,lparam);
	if (!ret)
		break;
    }
    return ret;
}
