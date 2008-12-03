/*
 * Elf-dll loader functions
 *
 * Copyright 1999 Bertho A. Stultiens
 *
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 *
 */
#include "config.h"

#ifdef HAVE_LIBDL

#include "wine/windef.h"
#include "wine/module.h"
#include "wine/heap.h"
#include "wine/elfdll.h"
#include "wine/debugtools.h"
#include "wine/winerror.h"
#include "debug.h"

//DEFAULT_DEBUG_CHANNEL(elfdll)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>


//WINE_MODREF *local_wm=NULL;
extern modref_list* local_wm;


/*------------------ HACKS -----------------*/
DWORD fixup_imports(WINE_MODREF *wm);
void dump_exports(HMODULE hModule);
/*---------------- END HACKS ---------------*/

//char *extra_ld_library_path = "/usr/lib/win32";
extern char* def_path;

struct elfdll_image
{
	HMODULE		pe_module_start;
	DWORD		pe_module_size;
};


/****************************************************************************
 *	ELFDLL_dlopen
 *
 * Wrapper for dlopen to search the EXTRA_LD_LIBRARY_PATH from wine.conf
 * manually because libdl.so caches the environment and does not accept our
 * changes.
 */
void *ELFDLL_dlopen(const char *libname, int flags)
{
	char buffer[256];
	int namelen;
	void *handle;
	char *ldpath;

	/* First try the default path search of dlopen() */
	handle = dlopen(libname, flags);
	if(handle)
		return handle;

	/* Now try to construct searches through our extra search-path */
	namelen = strlen(libname);
	ldpath = def_path;
	while(ldpath && *ldpath)
	{
		int len;
		char *cptr;
		char *from;

		from = ldpath;
		cptr = strchr(ldpath, ':');
		if(!cptr)
		{
			len = strlen(ldpath);
			ldpath = NULL;
		}
		else
		{
			len = cptr - ldpath;
			ldpath = cptr + 1;
		}

		if(len + namelen + 1 >= sizeof(buffer))
		{
			ERR("Buffer overflow! Check EXTRA_LD_LIBRARY_PATH or increase buffer size.\n");
			return NULL;
		}

		strncpy(buffer, from, len);
		if(len)
		{
			buffer[len] = '/';
			strcpy(buffer + len + 1, libname);
		}
		else
			strcpy(buffer + len, libname);

		TRACE("Trying dlopen('%s', %d)\n", buffer, flags);

		handle = dlopen(buffer, flags);
		if(handle)
			return handle;
	}
	return NULL;
}


/****************************************************************************
 *	get_sobasename	(internal)
 *
 */
static LPSTR get_sobasename(LPCSTR path, LPSTR name)
{
	char *cptr;

	/* Strip the path from the library name */
	if((cptr = strrchr(path, '/')))
	{
		char *cp = strrchr(cptr+1, '\\');
		if(cp && cp > cptr)
			cptr = cp;
	}
	else
		cptr = strrchr(path, '\\');

	if(!cptr)
		cptr = (char *)path;	/* No '/' nor '\\' in path */
	else
		cptr++;

	strcpy(name, cptr);
	cptr = strrchr(name, '.');
	if(cptr)
		*cptr = '\0';	/* Strip extension */

	/* Convert to lower case.
	 * This must be done manually because it is not sure that
	 * other modules are accessible.
	 */
	for(cptr = name; *cptr; cptr++)
		*cptr = tolower(*cptr);

	return name;
}


/****************************************************************************
 *	ELFDLL_CreateModref	(internal)
 *
 * INPUT
 *	hModule	- the header from the elf-dll's data-segment
 *	path	- requested path from original call
 *
 * OUTPUT
 *	A WINE_MODREF pointer to the new object
 *
 * BUGS
 *	- Does not handle errors due to dependencies correctly
 *	- path can be wrong
 */
#define RVA(base, va)	(((DWORD)base) + ((DWORD)va))

static WINE_MODREF *ELFDLL_CreateModref(HMODULE hModule, LPCSTR path)
{
//	IMAGE_NT_HEADERS *nt = PE_HEADER(hModule);
//	IMAGE_DATA_DIRECTORY *dir;
//	IMAGE_IMPORT_DESCRIPTOR *pe_import = NULL;
	WINE_MODREF *wm;
//	int len;
	HANDLE procheap = GetProcessHeap();

	wm = (WINE_MODREF *)HeapAlloc(procheap, HEAP_ZERO_MEMORY, sizeof(*wm));
	if(!wm)
		return NULL;

	wm->module = hModule;
	wm->type = MODULE32_ELF;		/* FIXME */

//	dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_EXPORT;
//	if(dir->Size)
//		wm->binfmt.pe.pe_export = (PIMAGE_EXPORT_DIRECTORY)RVA(hModule, dir->VirtualAddress);

//	dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_IMPORT;
//	if(dir->Size)
//		pe_import = wm->binfmt.pe.pe_import = (PIMAGE_IMPORT_DESCRIPTOR)RVA(hModule, dir->VirtualAddress);

//	dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_RESOURCE;
//	if(dir->Size)
//		wm->binfmt.pe.pe_resource = (PIMAGE_RESOURCE_DIRECTORY)RVA(hModule, dir->VirtualAddress);


	wm->filename = (char*) malloc(strlen(path)+1);
	strcpy(wm->filename, path);
	wm->modname = strrchr( wm->filename, '\\' );
	if (!wm->modname) wm->modname = wm->filename;
	else wm->modname++;
/*
	len = GetShortPathNameA( wm->filename, NULL, 0 );
	wm->short_filename = (char *)HeapAlloc( procheap, 0, len+1 );
	GetShortPathNameA( wm->filename, wm->short_filename, len+1 );
	wm->short_modname = strrchr( wm->short_filename, '\\' );
	if (!wm->short_modname) wm->short_modname = wm->short_filename;
	else wm->short_modname++;
*/
	/* Link MODREF into process list */

//	EnterCriticalSection( &PROCESS_Current()->crit_section );
	
	if(local_wm)
        {
    	    local_wm->next = (modref_list*) malloc(sizeof(modref_list));
    	    local_wm->next->prev=local_wm;
    	    local_wm->next->next=NULL;
            local_wm->next->wm=wm;
    	    local_wm=local_wm->next;
	}
	else
        {
	    local_wm = (modref_list*) malloc(sizeof(modref_list));
	    local_wm->next=local_wm->prev=NULL;
    	    local_wm->wm=wm;
	}	

//	LeaveCriticalSection( &PROCESS_Current()->crit_section );
	return wm;
}

/****************************************************************************
 *	ELFDLL_LoadLibraryExA	(internal)
 *
 * Implementation of elf-dll loading for PE modules
 */
WINE_MODREF *ELFDLL_LoadLibraryExA(LPCSTR path, DWORD flags)
{
	LPVOID dlhandle;
//	struct elfdll_image *image;
	char name[129];
	char soname[129];
	WINE_MODREF *wm;

	get_sobasename(path, name);
	strcpy(soname, name);
	strcat(soname, ".so");

	/* Try to open the elf-dll */
	dlhandle = ELFDLL_dlopen(soname, RTLD_LAZY);
	if(!dlhandle)
	{
		WARN("Could not load %s (%s)\n", soname, dlerror());
		SetLastError( ERROR_FILE_NOT_FOUND );
		return NULL;
	}

	/* Get the 'dllname_elfdll_image' variable */
/*	strcpy(soname, name);
	strcat(soname, "_elfdll_image");
	image = (struct elfdll_image *)dlsym(dlhandle, soname);
	if(!image) 
	{
		ERR("Could not get elfdll image descriptor %s (%s)\n", soname, dlerror());
		dlclose(dlhandle);
		SetLastError( ERROR_BAD_FORMAT );
		return NULL;
	}

*/
	wm = ELFDLL_CreateModref((int)dlhandle, path);
	if(!wm)
	{
		ERR("Could not create WINE_MODREF for %s\n", path);
		dlclose(dlhandle);
		SetLastError( ERROR_OUTOFMEMORY );
		return NULL;
	}

	return wm;
}


/****************************************************************************
 *	ELFDLL_UnloadLibrary	(internal)
 *
 * Unload an elf-dll completely from memory and deallocate the modref
 */
void ELFDLL_UnloadLibrary(WINE_MODREF *wm)
{
}

#endif /*HAVE_LIBDL*/
