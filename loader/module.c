/*
 * Modules
 *
 * Copyright 1995 Alexandre Julliard
 */
#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>


#include <wine/windef.h>
#include <wine/winerror.h>
#include <wine/heap.h>
#include <wine/module.h>
#include <wine/pe_image.h>
#include <wine/debugtools.h>
#ifdef HAVE_LIBDL
#include <dlfcn.h>
#include <wine/elfdll.h>
#endif
#include "win32.h"
//#include "driver.h"

//#undef TRACE
//#define TRACE printf

struct modref_list_t;

typedef struct modref_list_t
{
    WINE_MODREF* wm;
    struct modref_list_t *next;
    struct modref_list_t *prev;
} modref_list;

//WINE_MODREF *local_wm=NULL;
modref_list* local_wm=NULL;

HANDLE SegptrHeap;

WINE_MODREF* MODULE_FindModule(LPCSTR m)
{
    modref_list* list=local_wm;
    TRACE("Module %s request\n", m);
    if(list==NULL)
	return NULL;
    while(strcmp(m, list->wm->filename))
    {
	TRACE("%s: %x\n", list->wm->filename, list->wm->module);
	list=list->prev;
	if(list==NULL)
	    return NULL;
    }
    TRACE("Resolved to %s\n", list->wm->filename);
    return list->wm;
}

static void MODULE_RemoveFromList(WINE_MODREF *mod)
{
    modref_list* list=local_wm;
    if(list==0)
	return;
    if(mod==0)
	return;
    if((list->prev==NULL)&&(list->next==NULL))
    {
	free(list);
	local_wm=NULL;
//	uninstall_fs();
	return;
    }
    for(;list;list=list->prev)
    {
	if(list->wm==mod)
	{
	    if(list->prev)
		list->prev->next=list->next;
	    if(list->next)
		list->next->prev=list->prev;
	    if(list==local_wm)
		local_wm=list->prev;
	    free(list);
	    return;
	}
    }

}

WINE_MODREF *MODULE32_LookupHMODULE(HMODULE m)
{
    modref_list* list=local_wm;
    TRACE("Module %X request\n", m);
    if(list==NULL)
	return NULL;
    while(m!=list->wm->module)
    {
//      printf("Checking list %X wm %X module %X\n",
//	list, list->wm, list->wm->module);
	list=list->prev;
	if(list==NULL)
	    return NULL;
    }
    TRACE("LookupHMODULE hit %p\n", list->wm);
    return list->wm;
}

/*************************************************************************
 *		MODULE_InitDll
 */
static WIN_BOOL MODULE_InitDll( WINE_MODREF *wm, DWORD type, LPVOID lpReserved )
{
    WIN_BOOL retv = TRUE;

    static LPCSTR typeName[] = { "PROCESS_DETACH", "PROCESS_ATTACH",
                                 "THREAD_ATTACH", "THREAD_DETACH" };
    assert( wm );


    /* Skip calls for modules loaded with special load flags */

    if (    ( wm->flags & WINE_MODREF_DONT_RESOLVE_REFS )
         || ( wm->flags & WINE_MODREF_LOAD_AS_DATAFILE ) )
        return TRUE;


    TRACE("(%s,%s,%p) - CALL\n", wm->modname, typeName[type], lpReserved );

    /* Call the initialization routine */
    switch ( wm->type )
    {
    case MODULE32_PE:
        retv = PE_InitDLL( wm, type, lpReserved );
        break;

    case MODULE32_ELF:
        /* no need to do that, dlopen() already does */
        break;

    default:
        ERR("wine_modref type %d not handled.\n", wm->type );
        retv = FALSE;
        break;
    }

    /* The state of the module list may have changed due to the call
       to PE_InitDLL. We cannot assume that this module has not been
       deleted.  */
    TRACE("(%p,%s,%p) - RETURN %d\n", wm, typeName[type], lpReserved, retv );

    return retv;
}

/*************************************************************************
 *		MODULE_DllProcessAttach
 *
 * Send the process attach notification to all DLLs the given module
 * depends on (recursively). This is somewhat complicated due to the fact that
 *
 * - we have to respect the module dependencies, i.e. modules implicitly
 *   referenced by another module have to be initialized before the module
 *   itself can be initialized
 *
 * - the initialization routine of a DLL can itself call LoadLibrary,
 *   thereby introducing a whole new set of dependencies (even involving
 *   the 'old' modules) at any time during the whole process
 *
 * (Note that this routine can be recursively entered not only directly
 *  from itself, but also via LoadLibrary from one of the called initialization
 *  routines.)
 *
 * Furthermore, we need to rearrange the main WINE_MODREF list to allow
 * the process *detach* notifications to be sent in the correct order.
 * This must not only take into account module dependencies, but also
 * 'hidden' dependencies created by modules calling LoadLibrary in their
 * attach notification routine.
 *
 * The strategy is rather simple: we move a WINE_MODREF to the head of the
 * list after the attach notification has returned.  This implies that the
 * detach notifications are called in the reverse of the sequence the attach
 * notifications *returned*.
 *
 * NOTE: Assumes that the process critical section is held!
 *
 */
static WIN_BOOL MODULE_DllProcessAttach( WINE_MODREF *wm, LPVOID lpReserved )
{
    WIN_BOOL retv = TRUE;
    int i;
    assert( wm );

    /* prevent infinite recursion in case of cyclical dependencies */
    if (    ( wm->flags & WINE_MODREF_MARKER )
         || ( wm->flags & WINE_MODREF_PROCESS_ATTACHED ) )
        return retv;

    TRACE("(%s,%p) - START\n", wm->modname, lpReserved );

    /* Tag current MODREF to prevent recursive loop */
    wm->flags |= WINE_MODREF_MARKER;

    /* Recursively attach all DLLs this one depends on */
/*    for ( i = 0; retv && i < wm->nDeps; i++ )
        if ( wm->deps[i] )
            retv = MODULE_DllProcessAttach( wm->deps[i], lpReserved );
*/
    /* Call DLL entry point */

    //local_wm=wm;
    if(local_wm)
    {
        local_wm->next=malloc(sizeof(modref_list));
        local_wm->next->prev=local_wm;
        local_wm->next->next=NULL;
        local_wm->next->wm=wm;
        local_wm=local_wm->next;
    }
    else
    {
	local_wm=malloc(sizeof(modref_list));
	local_wm->next=local_wm->prev=NULL;
	local_wm->wm=wm;
    }
    /* Remove recursion flag */
    wm->flags &= ~WINE_MODREF_MARKER;

    if ( retv )
    {
        retv = MODULE_InitDll( wm, DLL_PROCESS_ATTACH, lpReserved );
        if ( retv )
            wm->flags |= WINE_MODREF_PROCESS_ATTACHED;
    }


    TRACE("(%s,%p) - END\n", wm->modname, lpReserved );

    return retv;
}

/*************************************************************************
 *		MODULE_DllProcessDetach
 *
 * Send DLL process detach notifications.  See the comment about calling
 * sequence at MODULE_DllProcessAttach.  Unless the bForceDetach flag
 * is set, only DLLs with zero refcount are notified.
 */
static void MODULE_DllProcessDetach( WINE_MODREF* wm, WIN_BOOL bForceDetach, LPVOID lpReserved )
{
    //    WINE_MODREF *wm=local_wm;
    modref_list* l = local_wm;
    wm->flags &= ~WINE_MODREF_PROCESS_ATTACHED;
    MODULE_InitDll( wm, DLL_PROCESS_DETACH, lpReserved );
/*    while (l)
    {
	modref_list* f = l;
	l = l->next;
	free(f);
    }
    local_wm = 0;*/
}

/***********************************************************************
 *	MODULE_LoadLibraryExA	(internal)
 *
 * Load a PE style module according to the load order.
 *
 * The HFILE parameter is not used and marked reserved in the SDK. I can
 * only guess that it should force a file to be mapped, but I rather
 * ignore the parameter because it would be extremely difficult to
 * integrate this with different types of module represenations.
 *
 */
static WINE_MODREF *MODULE_LoadLibraryExA( LPCSTR libname, HFILE hfile, DWORD flags )
{
	DWORD err = GetLastError();
	WINE_MODREF *pwm;
	int i;
//	module_loadorder_t *plo;

        SetLastError( ERROR_FILE_NOT_FOUND );
	TRACE("Trying native dll '%s'\n", libname);
	pwm = PE_LoadLibraryExA(libname, flags);
#ifdef HAVE_LIBDL
	if(!pwm)
	{
    	    TRACE("Trying ELF dll '%s'\n", libname);
	    pwm=(WINE_MODREF*)ELFDLL_LoadLibraryExA(libname, flags);
	}
#endif
//		printf("0x%08x\n", pwm);
//		break;
	if(pwm)
	{
		/* Initialize DLL just loaded */
		TRACE("Loaded module '%s' at 0x%08x, \n", libname, pwm->module);
		/* Set the refCount here so that an attach failure will */
		/* decrement the dependencies through the MODULE_FreeLibrary call. */
		pwm->refCount++;

                SetLastError( err );  /* restore last error */
		return pwm;
	}


	WARN("Failed to load module '%s'; error=0x%08lx, \n", libname, GetLastError());
	return NULL;
}

/***********************************************************************
 *           MODULE_FreeLibrary
 *
 * NOTE: Assumes that the process critical section is held!
 */
static WIN_BOOL MODULE_FreeLibrary( WINE_MODREF *wm )
{
    TRACE("(%s) - START\n", wm->modname );

    /* Recursively decrement reference counts */
    //MODULE_DecRefCount( wm );

    /* Call process detach notifications */
    MODULE_DllProcessDetach( wm, FALSE, NULL );

    PE_UnloadLibrary(wm);

    TRACE("END\n");

    return TRUE;
}

/***********************************************************************
 *           LoadLibraryExA   (KERNEL32)
 */
HMODULE WINAPI LoadLibraryExA(LPCSTR libname, HANDLE hfile, DWORD flags)
{
	WINE_MODREF *wm = 0;
	char* listpath[] = { "", "", "/usr/lib/win32", "/usr/local/lib/win32", 0 };
	extern char* def_path;
	char path[512];
	char checked[2000];
        int i = -1;

        checked[0] = 0;
	if(!libname)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	printf("Loading DLL: '%s'\n", libname);
//	if(fs_installed==0)
//	    install_fs();

	while (wm == 0 && listpath[++i])
	{
	    if (i < 2)
	    {
		if (i == 0)
		    /* check just original file name */
		    strncpy(path, libname, 511);
                else
		    /* check default user path */
		    strncpy(path, def_path, 300);
	    }
	    else if (strcmp(def_path, listpath[i]))
                /* path from the list */
		strncpy(path, listpath[i], 300);
	    else
		continue;

	    if (i > 0)
	    {
		strcat(path, "/");
		strncat(path, libname, 100);
	    }
	    path[511] = 0;
	    wm = MODULE_LoadLibraryExA( path, hfile, flags );

	    if (!wm)
	    {
		if (checked[0])
		    strcat(checked, ", ");
		strcat(checked, path);
                checked[1500] = 0;

	    }
	}
	if ( wm )
	{
		if ( !MODULE_DllProcessAttach( wm, NULL ) )
		{
			WARN_(module)("Attach failed for module '%s', \n", libname);
			MODULE_FreeLibrary(wm);
			SetLastError(ERROR_DLL_INIT_FAILED);
			MODULE_RemoveFromList(wm);
			wm = NULL;
		}
	}

	if (!wm)
	    printf("Win32 LoadLibrary failed to load: %s\n", checked);
	else
	{
	    extern char *win32_codec_name;
	    printf("Loaded %s to address %p\n", libname, wm->module);
	    /* XXX: FIXME, _VERY_ UGLY HACK */
	    if (!strcmp(libname, "m3jpegdec.ax"))
		win32_codec_name = strdup("m3jpeg32.dll");
	}

	return wm ? wm->module : 0;
}


/***********************************************************************
 *           LoadLibraryA         (KERNEL32)
 */
HMODULE WINAPI LoadLibraryA(LPCSTR libname) {
	return LoadLibraryExA(libname,0,0);
}

/***********************************************************************
 *           FreeLibrary
 */
WIN_BOOL WINAPI FreeLibrary(HINSTANCE hLibModule)
{
    WIN_BOOL retv = FALSE;
    WINE_MODREF *wm;

    wm=MODULE32_LookupHMODULE(hLibModule);

    if ( !wm || !hLibModule )
    {
        SetLastError( ERROR_INVALID_HANDLE );
	return 0;
    }
    else
        retv = MODULE_FreeLibrary( wm );

    MODULE_RemoveFromList(wm);

    /* garbage... */
    if (local_wm == NULL) my_garbagecollection();

    return retv;
}

/***********************************************************************
 *           MODULE_DecRefCount
 *
 * NOTE: Assumes that the process critical section is held!
 */
static void MODULE_DecRefCount( WINE_MODREF *wm )
{
    int i;

    if ( wm->flags & WINE_MODREF_MARKER )
        return;

    if ( wm->refCount <= 0 )
        return;

    --wm->refCount;
    TRACE("(%s) refCount: %d\n", wm->modname, wm->refCount );

    if ( wm->refCount == 0 )
    {
        wm->flags |= WINE_MODREF_MARKER;

        for ( i = 0; i < wm->nDeps; i++ )
            if ( wm->deps[i] )
                MODULE_DecRefCount( wm->deps[i] );

        wm->flags &= ~WINE_MODREF_MARKER;
    }
}

/***********************************************************************
 *           GetProcAddress   		(KERNEL32.257)
 */
FARPROC WINAPI GetProcAddress( HMODULE hModule, LPCSTR function )
{
    return MODULE_GetProcAddress( hModule, function, TRUE );
}

/***********************************************************************
 *           MODULE_GetProcAddress   		(internal)
 */
FARPROC MODULE_GetProcAddress(
	HMODULE hModule, 	/* [in] current module handle */
	LPCSTR function,	/* [in] function to be looked up */
	WIN_BOOL snoop )
{
    WINE_MODREF	*wm = MODULE32_LookupHMODULE( hModule );
//    WINE_MODREF *wm=local_wm;
    FARPROC	retproc;

    if (HIWORD(function))
	TRACE_(win32)("(%08lx,%s)\n",(DWORD)hModule,function);
    else
	TRACE_(win32)("(%08lx,%p)\n",(DWORD)hModule,function);
    if (!wm) {
    	SetLastError(ERROR_INVALID_HANDLE);
        return (FARPROC)0;
    }
    switch (wm->type)
    {
    case MODULE32_PE:
     	retproc = PE_FindExportedFunction( wm, function, snoop );
	if (!retproc) SetLastError(ERROR_PROC_NOT_FOUND);
	return retproc;
#ifdef HAVE_LIBDL
    case MODULE32_ELF:
	retproc = (FARPROC) dlsym( (void*) wm->module, function);
	if (!retproc) SetLastError(ERROR_PROC_NOT_FOUND);
	return retproc;
#endif
    default:
    	ERR("wine_modref type %d not handled.\n",wm->type);
    	SetLastError(ERROR_INVALID_HANDLE);
    	return (FARPROC)0;
    }
}

static int acounter = 0;
void CodecAlloc(void)
{
    acounter++;
    //printf("**************CODEC ALLOC %d\n", acounter);
}

void CodecRelease(void)
{
    acounter--;
    //printf("**************CODEC RELEASE %d\n", acounter);
    if (acounter == 0)
    {
	for (;;)
	{
	    modref_list* list = local_wm;
	    if (!local_wm)
		break;
	    //printf("CODECRELEASE %p\n", list);
            MODULE_FreeLibrary(list->wm);
	    MODULE_RemoveFromList(list->wm);
            if (local_wm == NULL)
		my_garbagecollection();
	}
    }
}
