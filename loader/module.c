/*
 * Modules
 *
 * Copyright 1995 Alexandre Julliard
 */
#include <config.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
/*
#ifdef __linux__
#include <asm/unistd.h>
#include <asm/ldt.h>
#else
#define LDT_ENTRIES     8192
#define LDT_ENTRY_SIZE  8

struct modify_ldt_ldt_s {
        unsigned int  entry_number;
        unsigned long base_addr;
        unsigned int  limit;
        unsigned int  seg_32bit:1;
        unsigned int  contents:2;
        unsigned int  read_exec_only:1;
        unsigned int  limit_in_pages:1;
        unsigned int  seg_not_present:1;
        unsigned int  useable:1;
};

#define MODIFY_LDT_CONTENTS_DATA        0
#define MODIFY_LDT_CONTENTS_STACK       1
#define MODIFY_LDT_CONTENTS_CODE        2
#define __NR_modify_ldt         123
#endif

*/
#include <wine/windef.h>
#include <wine/winerror.h>
#include <wine/heap.h>
#include <wine/module.h>
#include <wine/pe_image.h>
#include <wine/debugtools.h>

struct modref_list_t;

typedef struct modref_list_t
{
    WINE_MODREF* wm;
    struct modref_list_t *next;
    struct modref_list_t *prev;    
}
modref_list;


/***********************************************************************
 *           LDT_EntryToBytes
 *
 * Convert an ldt_entry structure to the raw bytes of the descriptor.
 */
/*static void LDT_EntryToBytes( unsigned long *buffer, const struct modify_ldt_ldt_s *content )
{
    *buffer++ = ((content->base_addr & 0x0000ffff) << 16) |
                 (content->limit & 0x0ffff);
    *buffer = (content->base_addr & 0xff000000) |
              ((content->base_addr & 0x00ff0000)>>16) |
              (content->limit & 0xf0000) |
              (content->contents << 10) |
              ((content->read_exec_only == 0) << 9) |
              ((content->seg_32bit != 0) << 22) |
              ((content->limit_in_pages != 0) << 23) |
              0xf000;
}
*/

//
// funcs:
//
// 0 read LDT
// 1 write old mode
// 0x11 write
//
/*
static int modify_ldt( int func, struct modify_ldt_ldt_s *ptr,
                                  unsigned long count )
{
    int res;
#ifdef __PIC__
    __asm__ __volatile__( "pushl %%ebx\n\t"
                          "movl %2,%%ebx\n\t"
                          "int $0x80\n\t"
                          "popl %%ebx"
                          : "=a" (res)
                          : "0" (__NR_modify_ldt),
                            "r" (func),
                            "c" (ptr),
                            "d" (sizeof(struct modify_ldt_ldt_s)*count) );
#else
    __asm__ __volatile__("int $0x80"
                         : "=a" (res)
                         : "0" (__NR_modify_ldt),
                           "b" (func),
                           "c" (ptr),
                           "d" (sizeof(struct modify_ldt_ldt_s)*count) );
#endif  
    if (res >= 0) return res;
    errno = -res;
    return -1;
}
static int fs_installed=0;
static char* fs_seg=0;
static int install_fs()
{
    struct modify_ldt_ldt_s array;
    int fd;
    int ret;
    void* prev_struct;
    
    if(fs_installed)
	return 0;

    fd=open("/dev/zero", O_RDWR);
    fs_seg=mmap((void*)0xbf000000, 0x30000, PROT_READ | PROT_WRITE, MAP_PRIVATE,
	fd, 0);
    if(fs_seg==0)
    {
	printf("ERROR: Couldn't allocate memory for fs segment\n");
	return -1;
    }	
    array.base_addr=((int)fs_seg+0xffff) & 0xffff0000;
    array.entry_number=0x1;
    array.limit=array.base_addr+getpagesize()-1;
    array.seg_32bit=1;
    array.read_exec_only=0;
    array.seg_not_present=0;
    array.contents=MODIFY_LDT_CONTENTS_DATA;
    array.limit_in_pages=0;
#ifdef linux
    ret=modify_ldt(0x1, &array, 1);
    if(ret<0)
    {
	perror("install_fs");
	MESSAGE("Couldn't install fs segment, expect segfault\n");
    }	
#endif 

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    {
        long d[2];

        LDT_EntryToBytes( d, &array );
        ret = i386_set_ldt(0x1, (union descriptor *)d, 1);
        if (ret < 0)
        {
            perror("install_fs");
            MESSAGE("Did you reconfigure the kernel with \"options USER_LDT\"?\n");
        }
    }
#endif 
    __asm__
    (
    "movl $0xf,%eax\n\t"
//    "pushw %ax\n\t"
    "movw %ax, %fs\n\t"
    );
    prev_struct=malloc(8);
    *(void**)array.base_addr=prev_struct;
    printf("prev_struct: 0x%X\n", prev_struct);
    close(fd);
    
    fs_installed=1;
    return 0;
};    	
static int uninstall_fs()
{
    printf("Uninstalling FS segment\n");
    if(fs_seg==0)
	return -1;
    munmap(fs_seg, 0x30000);
    fs_installed=0;
    return 0;
}

*/
//WINE_MODREF *local_wm=NULL;
modref_list* local_wm=NULL;

WINE_MODREF *MODULE_FindModule(LPCSTR m)
{
    modref_list* list=local_wm;
    TRACE("Module %s request\n", m);
    if(list==NULL)
	return NULL;
    while(strcmp(m, list->wm->filename))
    {
//	printf("%s: %x\n", list->wm->filename, list->wm->module);
	list=list->prev;
	if(list==NULL)
	    return NULL;
    }	
    TRACE("Resolved to %s\n", list->wm->filename);
    return list->wm;
}    

void MODULE_RemoveFromList(WINE_MODREF *mod)
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
    TRACE("LookupHMODULE hit %X\n", list->wm);
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
WIN_BOOL MODULE_DllProcessAttach( WINE_MODREF *wm, LPVOID lpReserved )
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
void MODULE_DllProcessDetach( WINE_MODREF* wm, WIN_BOOL bForceDetach, LPVOID lpReserved )
{
//    WINE_MODREF *wm=local_wm;
    wm->flags &= ~WINE_MODREF_PROCESS_ATTACHED;
    MODULE_InitDll( wm, DLL_PROCESS_DETACH, lpReserved );
}


/***********************************************************************
 *           LoadLibraryExA   (KERNEL32)
 */
HMODULE WINAPI LoadLibraryExA(LPCSTR libname, HANDLE hfile, DWORD flags)
{
	WINE_MODREF *wm;

	if(!libname)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
//	if(fs_installed==0)
//	    install_fs();
	    

	wm = MODULE_LoadLibraryExA( libname, hfile, flags );
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

	return wm ? wm->module : 0;
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
WINE_MODREF *MODULE_LoadLibraryExA( LPCSTR libname, HFILE hfile, DWORD flags )
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
//    wm=local_wm;

    if ( !wm || !hLibModule )
    {
        SetLastError( ERROR_INVALID_HANDLE );
	return 0;
    }	
    else
        retv = MODULE_FreeLibrary( wm );
    
    MODULE_RemoveFromList(wm);

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
 *           MODULE_FreeLibrary
 *
 * NOTE: Assumes that the process critical section is held!
 */
WIN_BOOL MODULE_FreeLibrary( WINE_MODREF *wm )
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
	retproc = (FARPROC) dlsym( wm->module, function);
	if (!retproc) SetLastError(ERROR_PROC_NOT_FOUND);
	return retproc;
#endif
    default:
    	ERR("wine_modref type %d not handled.\n",wm->type);
    	SetLastError(ERROR_INVALID_HANDLE);
    	return (FARPROC)0;
    }
}

