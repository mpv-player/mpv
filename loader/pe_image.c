/* 
 *  Copyright	1994	Eric Youndale & Erik Bos
 *  Copyright	1995	Martin von Löwis
 *  Copyright   1996-98 Marcus Meissner
 *
 *	based on Eric Youndale's pe-test and:
 *
 *	ftp.microsoft.com:/pub/developer/MSDN/CD8/PEFILE.ZIP
 * make that:
 *	ftp.microsoft.com:/developr/MSDN/OctCD/PEFILE.ZIP
 *
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 *
 */
/* Notes:
 * Before you start changing something in this file be aware of the following:
 *
 * - There are several functions called recursively. In a very subtle and 
 *   obscure way. DLLs can reference each other recursively etc.
 * - If you want to enhance, speed up or clean up something in here, think
 *   twice WHY it is implemented in that strange way. There is usually a reason.
 *   Though sometimes it might just be lazyness ;)
 * - In PE_MapImage, right before fixup_imports() all external and internal 
 *   state MUST be correct since this function can be called with the SAME image
 *   AGAIN. (Thats recursion for you.) That means MODREF.module and
 *   NE_MODULE.module32.
 * - Sometimes, we can't use Linux mmap() to mmap() the images directly.
 *
 *   The problem is, that there is not direct 1:1 mapping from a diskimage and
 *   a memoryimage. The headers at the start are mapped linear, but the sections
 *   are not. Older x86 pe binaries are 512 byte aligned in file and 4096 byte
 *   aligned in memory. Linux likes them 4096 byte aligned in memory (due to
 *   x86 pagesize, this cannot be fixed without a rather large kernel rewrite)
 *   and 'blocksize' file-aligned (offsets). Since we have 512/1024/2048 (CDROM)
 *   and other byte blocksizes, we can't always do this.  We *can* do this for
 *   newer pe binaries produced by MSVC 5 and later, since they are also aligned
 *   to 4096 byte boundaries on disk.
 */
#include "config.h"
#include "debug.h"

#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include "wine/windef.h"
#include "wine/winbase.h"
#include "wine/winerror.h"
#include "wine/heap.h"
#include "wine/pe_image.h"
#include "wine/module.h"
#include "wine/debugtools.h"
#include "ext.h"
#include "win32.h"

#define RVA(x) ((void *)((char *)load_addr+(unsigned int)(x)))

#define AdjustPtr(ptr,delta) ((char *)(ptr) + (delta))

extern void* LookupExternal(const char* library, int ordinal);
extern void* LookupExternalByName(const char* library, const char* name);

static void dump_exports( HMODULE hModule )
{ 
  char		*Module;
  unsigned int i, j;
  u_short	*ordinal;
  u_long	*function,*functions;
  u_char	**name;
  unsigned int load_addr = hModule;

  DWORD rva_start = PE_HEADER(hModule)->OptionalHeader
                   .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
  DWORD rva_end = rva_start + PE_HEADER(hModule)->OptionalHeader
                   .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
  IMAGE_EXPORT_DIRECTORY *pe_exports = (IMAGE_EXPORT_DIRECTORY*)RVA(rva_start);

  Module = (char*)RVA(pe_exports->Name);
  TRACE("*******EXPORT DATA*******\n");
  TRACE("Module name is %s, %ld functions, %ld names\n", 
        Module, pe_exports->NumberOfFunctions, pe_exports->NumberOfNames);

  ordinal=(u_short*) RVA(pe_exports->AddressOfNameOrdinals);
  functions=function=(u_long*) RVA(pe_exports->AddressOfFunctions);
  name=(u_char**) RVA(pe_exports->AddressOfNames);

  TRACE(" Ord    RVA     Addr   Name\n" );
  for (i=0;i<pe_exports->NumberOfFunctions;i++, function++)
  {
      if (!*function) continue;  
      if (TRACE_ON(win32))
      {
	DPRINTF( "%4ld %08lx %p", i + pe_exports->Base, *function, RVA(*function) );
	
	for (j = 0; j < pe_exports->NumberOfNames; j++)
          if (ordinal[j] == i)
          {
              DPRINTF( "  %s", (char*)RVA(name[j]) );
              break;
          }
	if ((*function >= rva_start) && (*function <= rva_end))
	  DPRINTF(" (forwarded -> %s)", (char *)RVA(*function));
	DPRINTF("\n");
      }
  }
}

/* Look up the specified function or ordinal in the exportlist:
 * If it is a string:
 * 	- look up the name in the Name list. 
 *	- look up the ordinal with that index.
 *	- use the ordinal as offset into the functionlist
 * If it is a ordinal:
 *	- use ordinal-pe_export->Base as offset into the functionlist
 */
FARPROC PE_FindExportedFunction( 
	WINE_MODREF *wm,	
	LPCSTR funcName,	
        WIN_BOOL snoop )
{
	u_short				* ordinals;
	u_long				* function;
	u_char				** name;
	const char *ename = NULL;
	int				i, ordinal;
	PE_MODREF			*pem = &(wm->binfmt.pe);
	IMAGE_EXPORT_DIRECTORY 		*exports = pem->pe_export;
	unsigned int			load_addr = wm->module;
	u_long				rva_start, rva_end, addr;
	char				* forward;

	if (HIWORD(funcName))
		TRACE("(%s)\n",funcName);
	else
		TRACE("(%d)\n",(int)funcName);
	if (!exports) {
		/* Not a fatal problem, some apps do
		 * GetProcAddress(0,"RegisterPenApp") which triggers this
		 * case.
		 */
		WARN("Module %08x(%s)/MODREF %p doesn't have a exports table.\n",wm->module,wm->modname,pem);
		return NULL;
	}
	ordinals= (u_short*)  RVA(exports->AddressOfNameOrdinals);
	function= (u_long*)   RVA(exports->AddressOfFunctions);
	name	= (u_char **) RVA(exports->AddressOfNames);
	forward = NULL;
	rva_start = PE_HEADER(wm->module)->OptionalHeader
		.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	rva_end = rva_start + PE_HEADER(wm->module)->OptionalHeader
		.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;

	if (HIWORD(funcName))
        {
            
            int min = 0, max = exports->NumberOfNames - 1;
            while (min <= max)
            {
                int res, pos = (min + max) / 2;
		ename = (const char*) RVA(name[pos]);
		if (!(res = strcmp( ename, funcName )))
                {
                    ordinal = ordinals[pos];
                    goto found;
                }
                if (res > 0) max = pos - 1;
                else min = pos + 1;
            }
            
	    for (i = 0; i < exports->NumberOfNames; i++)
            {
		ename = (const char*) RVA(name[i]);
                if (!strcmp( ename, funcName ))
                {
		    ERR( "%s.%s required a linear search\n", wm->modname, funcName );
                    ordinal = ordinals[i];
                    goto found;
                }
            }
            return NULL;
	}
        else  
        {
            ordinal = LOWORD(funcName) - exports->Base;
            if (snoop && name)  
            {
                for (i = 0; i < exports->NumberOfNames; i++)
                    if (ordinals[i] == ordinal)
                    {
                        ename = RVA(name[i]);
                        break;
                    }
            }
	}

 found:
        if (ordinal >= exports->NumberOfFunctions)
        {
            TRACE("	ordinal %ld out of range!\n", ordinal + exports->Base );
            return NULL;
        }
        addr = function[ordinal];
        if (!addr) return NULL;
        if ((addr < rva_start) || (addr >= rva_end))
        {
            FARPROC proc = RVA(addr);
            if (snoop)
            {
                if (!ename) ename = "@";
//                proc = SNOOP_GetProcAddress(wm->module,ename,ordinal,proc);
		TRACE("SNOOP_GetProcAddress n/a\n");
		
            }
            return proc;
        }
        else  
        {
                WINE_MODREF *wm;
                char *forward = RVA(addr);
		char module[256];
		char *end = strchr(forward, '.');

		if (!end) return NULL;
                if (end - forward >= sizeof(module)) return NULL;
                memcpy( module, forward, end - forward );
		module[end-forward] = 0;
                if (!(wm = MODULE_FindModule( module )))
                {
                    ERR("module not found for forward '%s'\n", forward );
                    return NULL;
                }
		return MODULE_GetProcAddress( wm->module, end + 1, snoop );
	}
}

static DWORD fixup_imports( WINE_MODREF *wm )
{
    IMAGE_IMPORT_DESCRIPTOR	*pe_imp;
    PE_MODREF			*pem;
    unsigned int load_addr	= wm->module;
    int				i,characteristics_detection=1;
    char			*modname;
    
    assert(wm->type==MODULE32_PE);
    pem = &(wm->binfmt.pe);
    if (pem->pe_export)
    	modname = (char*) RVA(pem->pe_export->Name);
    else
        modname = "<unknown>";

    
    TRACE("Dumping imports list\n");

    
    pe_imp = pem->pe_import;
    if (!pe_imp) return 0;

    /* We assume that we have at least one import with !0 characteristics and
     * detect broken imports with all characteristsics 0 (notably Borland) and
     * switch the detection off for them.
     */
    for (i = 0; pe_imp->Name ; pe_imp++) {
	if (!i && !pe_imp->u.Characteristics)
		characteristics_detection = 0;
	if (characteristics_detection && !pe_imp->u.Characteristics)
		break;
	i++;
    }
    if (!i) return 0;  

    
    wm->nDeps = i;
    wm->deps  = HeapAlloc( GetProcessHeap(), 0, i*sizeof(WINE_MODREF *) );

    /* load the imported modules. They are automatically 
     * added to the modref list of the process.
     */
 
    for (i = 0, pe_imp = pem->pe_import; pe_imp->Name ; pe_imp++) {
    	WINE_MODREF		*wmImp;
	IMAGE_IMPORT_BY_NAME	*pe_name;
	PIMAGE_THUNK_DATA	import_list,thunk_list;
 	char			*name = (char *) RVA(pe_imp->Name);

	if (characteristics_detection && !pe_imp->u.Characteristics)
		break;

//#warning FIXME: here we should fill imports
        TRACE("Loading imports for %s.dll\n", name);
    
	if (pe_imp->u.OriginalFirstThunk != 0) { 
	    TRACE("Microsoft style imports used\n");
	    import_list =(PIMAGE_THUNK_DATA) RVA(pe_imp->u.OriginalFirstThunk);
	    thunk_list = (PIMAGE_THUNK_DATA) RVA(pe_imp->FirstThunk);

	    while (import_list->u1.Ordinal) {
		if (IMAGE_SNAP_BY_ORDINAL(import_list->u1.Ordinal)) {
		    int ordinal = IMAGE_ORDINAL(import_list->u1.Ordinal);

//		    TRACE("--- Ordinal %s,%d\n", name, ordinal);
		    
		    thunk_list->u1.Function=LookupExternal(name, ordinal);
		} else {		
		    pe_name = (PIMAGE_IMPORT_BY_NAME)RVA(import_list->u1.AddressOfData);
//		    TRACE("--- %s %s.%d\n", pe_name->Name, name, pe_name->Hint);
		    thunk_list->u1.Function=LookupExternalByName(name, pe_name->Name);
		}
		import_list++;
		thunk_list++;
	    }
	} else {	
	    TRACE("Borland style imports used\n");
	    thunk_list = (PIMAGE_THUNK_DATA) RVA(pe_imp->FirstThunk);
	    while (thunk_list->u1.Ordinal) {
		if (IMAGE_SNAP_BY_ORDINAL(thunk_list->u1.Ordinal)) {
		    
		    int ordinal = IMAGE_ORDINAL(thunk_list->u1.Ordinal);

		    TRACE("--- Ordinal %s.%d\n",name,ordinal);
		    thunk_list->u1.Function=LookupExternal(
		      name, ordinal);
		} else {
		    pe_name=(PIMAGE_IMPORT_BY_NAME) RVA(thunk_list->u1.AddressOfData);
		    TRACE("--- %s %s.%d\n",
		   		  pe_name->Name,name,pe_name->Hint);
		    thunk_list->u1.Function=LookupExternalByName(
		      name, pe_name->Name);
		}
		thunk_list++;
	    }
	}
    }
    return 0;
}

static int calc_vma_size( HMODULE hModule )
{
    int i,vma_size = 0;
    IMAGE_SECTION_HEADER *pe_seg = PE_SECTIONS(hModule);

    TRACE("Dump of segment table\n");
    TRACE("   Name    VSz  Vaddr     SzRaw   Fileadr  *Reloc *Lineum #Reloc #Linum Char\n");
    for (i = 0; i< PE_HEADER(hModule)->FileHeader.NumberOfSections; i++)
    {
        TRACE("%8s: %4.4lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %4.4x %4.4x %8.8lx\n", 
                      pe_seg->Name, 
                      pe_seg->Misc.VirtualSize,
                      pe_seg->VirtualAddress,
                      pe_seg->SizeOfRawData,
                      pe_seg->PointerToRawData,
                      pe_seg->PointerToRelocations,
                      pe_seg->PointerToLinenumbers,
                      pe_seg->NumberOfRelocations,
                      pe_seg->NumberOfLinenumbers,
                      pe_seg->Characteristics);
        vma_size=max(vma_size, pe_seg->VirtualAddress+pe_seg->SizeOfRawData);
        vma_size=max(vma_size, pe_seg->VirtualAddress+pe_seg->Misc.VirtualSize);
        pe_seg++;
    }
    return vma_size;
}

static void do_relocations( unsigned int load_addr, IMAGE_BASE_RELOCATION *r )
{
    int delta = load_addr - PE_HEADER(load_addr)->OptionalHeader.ImageBase;
    int	hdelta = (delta >> 16) & 0xFFFF;
    int	ldelta = delta & 0xFFFF;

	if(delta == 0)
		
		return;
	while(r->VirtualAddress)
	{
		char *page = (char*) RVA(r->VirtualAddress);
		int count = (r->SizeOfBlock - 8)/2;
		int i;
		TRACE_(fixup)("%x relocations for page %lx\n",
			count, r->VirtualAddress);
		
		for(i=0;i<count;i++)
		{
			int offset = r->TypeOffset[i] & 0xFFF;
			int type = r->TypeOffset[i] >> 12;
//			TRACE_(fixup)("patching %x type %x\n", offset, type);
			switch(type)
			{
			case IMAGE_REL_BASED_ABSOLUTE: break;
			case IMAGE_REL_BASED_HIGH:
				*(short*)(page+offset) += hdelta;
				break;
			case IMAGE_REL_BASED_LOW:
				*(short*)(page+offset) += ldelta;
				break;
			case IMAGE_REL_BASED_HIGHLOW:
				*(int*)(page+offset) += delta;
				
				break;
			case IMAGE_REL_BASED_HIGHADJ:
				FIXME("Don't know what to do with IMAGE_REL_BASED_HIGHADJ\n");
				break;
			case IMAGE_REL_BASED_MIPS_JMPADDR:
				FIXME("Is this a MIPS machine ???\n");
				break;
			default:
				FIXME("Unknown fixup type\n");
				break;
			}
		}
		r = (IMAGE_BASE_RELOCATION*)((char*)r + r->SizeOfBlock);
	}
}
		

	
	

/**********************************************************************
 *			PE_LoadImage
 * Load one PE format DLL/EXE into memory
 * 
 * Unluckily we can't just mmap the sections where we want them, for 
 * (at least) Linux does only support offsets which are page-aligned.
 *
 * BUT we have to map the whole image anyway, for Win32 programs sometimes
 * want to access them. (HMODULE32 point to the start of it)
 */
HMODULE PE_LoadImage( int handle, LPCSTR filename, WORD *version )
{
    HMODULE	hModule;
    HANDLE	mapping;

    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *pe_sec;
    IMAGE_DATA_DIRECTORY *dir;
    BY_HANDLE_FILE_INFORMATION bhfi;
    int	i, rawsize, lowest_va, vma_size, file_size = 0;
    DWORD load_addr = 0, aoep, reloc = 0;
//    struct get_read_fd_request *req = get_req_buffer();
    int unix_handle = handle;
    int page_size = getpagesize();

    
//    if ( GetFileInformationByHandle( hFile, &bhfi ) ) 
//    	file_size = bhfi.nFileSizeLow; 
    file_size=lseek(handle, 0, SEEK_END);
    lseek(handle, 0, SEEK_SET);

//#warning fix CreateFileMappingA
    mapping = CreateFileMappingA( handle, NULL, PAGE_READONLY | SEC_COMMIT,
                                    0, 0, NULL );
    if (!mapping)
    {
        WARN("CreateFileMapping error %ld\n", GetLastError() );
        return 0;
    }
//    hModule = (HMODULE)MapViewOfFile( mapping, FILE_MAP_READ, 0, 0, 0 );
    hModule=(HMODULE)mapping;
//    CloseHandle( mapping );
    if (!hModule)
    {
        WARN("MapViewOfFile error %ld\n", GetLastError() );
        return 0;
    }
    if ( *(WORD*)hModule !=IMAGE_DOS_SIGNATURE)
    {
        WARN("%s image doesn't have DOS signature, but 0x%04x\n", filename,*(WORD*)hModule);
        goto error;
    }

    nt = PE_HEADER( hModule );

    
    if ( nt->Signature != IMAGE_NT_SIGNATURE )
    {
        WARN("%s image doesn't have PE signature, but 0x%08lx\n", filename, nt->Signature );
        goto error;
    }

    
    if ( nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 )
    {
        MESSAGE("Trying to load PE image for unsupported architecture (");
        switch (nt->FileHeader.Machine)
        {
        case IMAGE_FILE_MACHINE_UNKNOWN: MESSAGE("Unknown"); break;
        case IMAGE_FILE_MACHINE_I860:    MESSAGE("I860"); break;
        case IMAGE_FILE_MACHINE_R3000:   MESSAGE("R3000"); break;
        case IMAGE_FILE_MACHINE_R4000:   MESSAGE("R4000"); break;
        case IMAGE_FILE_MACHINE_R10000:  MESSAGE("R10000"); break;
        case IMAGE_FILE_MACHINE_ALPHA:   MESSAGE("Alpha"); break;
        case IMAGE_FILE_MACHINE_POWERPC: MESSAGE("PowerPC"); break;
        default: MESSAGE("Unknown-%04x", nt->FileHeader.Machine); break;
        }
        MESSAGE(")\n");
        goto error;
    }

    
    pe_sec = PE_SECTIONS( hModule );
    rawsize = 0; lowest_va = 0x10000;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) 
    {
        if (lowest_va > pe_sec[i].VirtualAddress)
           lowest_va = pe_sec[i].VirtualAddress;
    	if (pe_sec[i].Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
	    continue;
	if (pe_sec[i].PointerToRawData+pe_sec[i].SizeOfRawData > rawsize)
	    rawsize = pe_sec[i].PointerToRawData+pe_sec[i].SizeOfRawData;
    }
 
    
    if ( file_size && file_size < rawsize )
    {
        ERR("PE module is too small (header: %d, filesize: %d), "
                    "probably truncated download?\n", 
                    rawsize, file_size );
        goto error;
    }

    
    aoep = nt->OptionalHeader.AddressOfEntryPoint;
    if (aoep && (aoep < lowest_va))
        FIXME("VIRUS WARNING: '%s' has an invalid entrypoint (0x%08lx) "
                      "below the first virtual address (0x%08x) "
                      "(possibly infected by Tchernobyl/SpaceFiller virus)!\n",
                       filename, aoep, lowest_va );


    /* FIXME:  Hack!  While we don't really support shared sections yet,
     *         this checks for those special cases where the whole DLL
     *         consists only of shared sections and is mapped into the
     *         shared address space > 2GB.  In this case, we assume that
     *         the module got mapped at its base address. Thus we simply
     *         check whether the module has actually been mapped there
     *         and use it, if so.  This is needed to get Win95 USER32.DLL
     *         to work (until we support shared sections properly).
     */

    if ( nt->OptionalHeader.ImageBase & 0x80000000 )
    {
        HMODULE sharedMod = (HMODULE)nt->OptionalHeader.ImageBase; 
        IMAGE_NT_HEADERS *sharedNt = (PIMAGE_NT_HEADERS)
               ( (LPBYTE)sharedMod + ((LPBYTE)nt - (LPBYTE)hModule) );

        /* Well, this check is not really comprehensive, 
           but should be good enough for now ... */
        if (    !IsBadReadPtr( (LPBYTE)sharedMod, sizeof(IMAGE_DOS_HEADER) )
             && memcmp( (LPBYTE)sharedMod, (LPBYTE)hModule, sizeof(IMAGE_DOS_HEADER) ) == 0
             && !IsBadReadPtr( sharedNt, sizeof(IMAGE_NT_HEADERS) )
             && memcmp( sharedNt, nt, sizeof(IMAGE_NT_HEADERS) ) == 0 )
        {
            UnmapViewOfFile( (LPVOID)hModule );
            return sharedMod;
        }
    }


    
    load_addr = nt->OptionalHeader.ImageBase;
    vma_size = calc_vma_size( hModule );

    load_addr = (DWORD)VirtualAlloc( (void*)load_addr, vma_size,
                                     MEM_RESERVE | MEM_COMMIT,
                                     PAGE_EXECUTE_READWRITE );
    if (load_addr == 0) 
    {
        
        FIXME("We need to perform base relocations for %s\n", filename);
	dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_BASERELOC;
        if (dir->Size)
            reloc = dir->VirtualAddress;
        else 
        {
            FIXME( "FATAL: Need to relocate %s, but no relocation records present (%s). Try to run that file directly !\n",
                   filename,
                   (nt->FileHeader.Characteristics&IMAGE_FILE_RELOCS_STRIPPED)?
                   "stripped during link" : "unknown reason" );
            goto error;
        }

        /* FIXME: If we need to relocate a system DLL (base > 2GB) we should
         *        really make sure that the *new* base address is also > 2GB.
         *        Some DLLs really check the MSB of the module handle :-/
         */
        if ( nt->OptionalHeader.ImageBase & 0x80000000 )
            ERR( "Forced to relocate system DLL (base > 2GB). This is not good.\n" );

        load_addr = (DWORD)VirtualAlloc( NULL, vma_size,
					 MEM_RESERVE | MEM_COMMIT,
					 PAGE_EXECUTE_READWRITE );
	if (!load_addr) {
            FIXME_(win32)(
                   "FATAL: Couldn't load module %s (out of memory, %d needed)!\n", filename, vma_size);
            goto error;
	}
    }

    TRACE("Load addr is %lx (base %lx), range %x\n",
          load_addr, nt->OptionalHeader.ImageBase, vma_size );
    TRACE_(segment)("Loading %s at %lx, range %x\n",
                    filename, load_addr, vma_size );

#if 0
    
    *(PIMAGE_DOS_HEADER)load_addr = *(PIMAGE_DOS_HEADER)hModule;
    *PE_HEADER( load_addr ) = *nt;
    memcpy( PE_SECTIONS(load_addr), PE_SECTIONS(hModule),
            sizeof(IMAGE_SECTION_HEADER) * nt->FileHeader.NumberOfSections );

    
    memcpy( load_addr, hModule, lowest_fa );
#endif

    if ((void*)FILE_dommap( handle, (void *)load_addr, 0, nt->OptionalHeader.SizeOfHeaders,
                     0, 0, PROT_EXEC | PROT_WRITE | PROT_READ,
                     MAP_PRIVATE | MAP_FIXED ) != (void*)load_addr)
    {
        ERR_(win32)( "Critical Error: failed to map PE header to necessary address.\n");	
        goto error;
    }

    
    pe_sec = PE_SECTIONS( hModule );
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, pe_sec++)
    {
        if (!pe_sec->SizeOfRawData || !pe_sec->PointerToRawData) continue;
        TRACE("%s: mmaping section %s at %p off %lx size %lx/%lx\n",
              filename, pe_sec->Name, (void*)RVA(pe_sec->VirtualAddress),
              pe_sec->PointerToRawData, pe_sec->SizeOfRawData, pe_sec->Misc.VirtualSize );
        if ((void*)FILE_dommap( unix_handle, (void*)RVA(pe_sec->VirtualAddress),
                         0, pe_sec->SizeOfRawData, 0, pe_sec->PointerToRawData,
                         PROT_EXEC | PROT_WRITE | PROT_READ,
                         MAP_PRIVATE | MAP_FIXED ) != (void*)RVA(pe_sec->VirtualAddress))
        {
            
            ERR_(win32)( "Critical Error: failed to map PE section to necessary address.\n");
            goto error;
        }
        if ((pe_sec->SizeOfRawData < pe_sec->Misc.VirtualSize) &&
            (pe_sec->SizeOfRawData & (page_size-1)))
        {
            DWORD end = (pe_sec->SizeOfRawData & ~(page_size-1)) + page_size;
            if (end > pe_sec->Misc.VirtualSize) end = pe_sec->Misc.VirtualSize;
            TRACE("clearing %p - %p\n",
                  RVA(pe_sec->VirtualAddress) + pe_sec->SizeOfRawData,
                  RVA(pe_sec->VirtualAddress) + end );
            memset( (char*)RVA(pe_sec->VirtualAddress) + pe_sec->SizeOfRawData, 0,
                    end - pe_sec->SizeOfRawData );
        }
    }

    
    if ( reloc )
        do_relocations( load_addr, (IMAGE_BASE_RELOCATION *)RVA(reloc) );

    
    *version =   ( (nt->OptionalHeader.MajorSubsystemVersion & 0xff) << 8 )
               |   (nt->OptionalHeader.MinorSubsystemVersion & 0xff);

    
    UnmapViewOfFile( (LPVOID)hModule );
    return (HMODULE)load_addr;

error:
    if (unix_handle != -1) close( unix_handle );
    if (load_addr) 
    VirtualFree( (LPVOID)load_addr, 0, MEM_RELEASE );
    UnmapViewOfFile( (LPVOID)hModule );
    return 0;
}

/**********************************************************************
 *                 PE_CreateModule
 *
 * Create WINE_MODREF structure for loaded HMODULE32, link it into
 * process modref_list, and fixup all imports.
 *
 * Note: hModule must point to a correctly allocated PE image,
 *       with base relocations applied; the 16-bit dummy module
 *       associated to hModule must already exist.
 *
 * Note: This routine must always be called in the context of the
 *       process that is to own the module to be created.
 */
WINE_MODREF *PE_CreateModule( HMODULE hModule, 
                              LPCSTR filename, DWORD flags, WIN_BOOL builtin )
{
    DWORD load_addr = (DWORD)hModule;  
    IMAGE_NT_HEADERS *nt = PE_HEADER(hModule);
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_IMPORT_DESCRIPTOR *pe_import = NULL;
    IMAGE_EXPORT_DIRECTORY *pe_export = NULL;
    IMAGE_RESOURCE_DIRECTORY *pe_resource = NULL;
    WINE_MODREF *wm;
    int	result;


    

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_EXPORT;
    if (dir->Size)
        pe_export = (PIMAGE_EXPORT_DIRECTORY)RVA(dir->VirtualAddress);

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_IMPORT;
    if (dir->Size)
        pe_import = (PIMAGE_IMPORT_DESCRIPTOR)RVA(dir->VirtualAddress);

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_RESOURCE;
    if (dir->Size)
        pe_resource = (PIMAGE_RESOURCE_DIRECTORY)RVA(dir->VirtualAddress);

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_EXCEPTION;
    if (dir->Size) FIXME("Exception directory ignored\n" );

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_SECURITY;
    if (dir->Size) FIXME("Security directory ignored\n" );

    
    

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_DEBUG;
    if (dir->Size) TRACE("Debug directory ignored\n" );

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_COPYRIGHT;
    if (dir->Size) FIXME("Copyright string ignored\n" );

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_GLOBALPTR;
    if (dir->Size) FIXME("Global Pointer (MIPS) ignored\n" );

    

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG;
    if (dir->Size) FIXME("Load Configuration directory ignored\n" );

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT;
    if (dir->Size) TRACE("Bound Import directory ignored\n" );

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_IAT;
    if (dir->Size) TRACE("Import Address Table directory ignored\n" );

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT;
    if (dir->Size)
    {
		TRACE("Delayed import, stub calls LoadLibrary\n" );
		/*
		 * Nothing to do here.
		 */

#ifdef ImgDelayDescr
		/*
		 * This code is useful to observe what the heck is going on.
		 */
		{
		ImgDelayDescr *pe_delay = NULL;
        pe_delay = (PImgDelayDescr)RVA(dir->VirtualAddress);
        TRACE_(delayhlp)("pe_delay->grAttrs = %08x\n", pe_delay->grAttrs);
        TRACE_(delayhlp)("pe_delay->szName = %s\n", pe_delay->szName);
        TRACE_(delayhlp)("pe_delay->phmod = %08x\n", pe_delay->phmod);
        TRACE_(delayhlp)("pe_delay->pIAT = %08x\n", pe_delay->pIAT);
        TRACE_(delayhlp)("pe_delay->pINT = %08x\n", pe_delay->pINT);
        TRACE_(delayhlp)("pe_delay->pBoundIAT = %08x\n", pe_delay->pBoundIAT);
        TRACE_(delayhlp)("pe_delay->pUnloadIAT = %08x\n", pe_delay->pUnloadIAT);
        TRACE_(delayhlp)("pe_delay->dwTimeStamp = %08x\n", pe_delay->dwTimeStamp);
        }
#endif 
	}

    dir = nt->OptionalHeader.DataDirectory+IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR;
    if (dir->Size) FIXME("Unknown directory 14 ignored\n" );

    dir = nt->OptionalHeader.DataDirectory+15;
    if (dir->Size) FIXME("Unknown directory 15 ignored\n" );


    

    wm = (WINE_MODREF *)HeapAlloc( GetProcessHeap(), 
                                   HEAP_ZERO_MEMORY, sizeof(*wm) );
    wm->module = hModule;

    if ( builtin ) 
        wm->flags |= WINE_MODREF_INTERNAL;
    if ( flags & DONT_RESOLVE_DLL_REFERENCES )
        wm->flags |= WINE_MODREF_DONT_RESOLVE_REFS;
    if ( flags & LOAD_LIBRARY_AS_DATAFILE )
        wm->flags |= WINE_MODREF_LOAD_AS_DATAFILE;

    wm->type = MODULE32_PE;
    wm->binfmt.pe.pe_export = pe_export;
    wm->binfmt.pe.pe_import = pe_import;
    wm->binfmt.pe.pe_resource = pe_resource;
    wm->binfmt.pe.tlsindex = -1;

    wm->filename = malloc(strlen(filename)+1);
    strcpy(wm->filename, filename );
    wm->modname = strrchr( wm->filename, '\\' );
    if (!wm->modname) wm->modname = wm->filename;
    else wm->modname++;

    if ( pe_export )
        dump_exports( hModule );

    /* Fixup Imports */

    if (    pe_import
         && !( wm->flags & WINE_MODREF_LOAD_AS_DATAFILE )
         && !( wm->flags & WINE_MODREF_DONT_RESOLVE_REFS ) 
         && fixup_imports( wm ) ) 
    {
        /* remove entry from modref chain */
         return NULL;
    }

    return wm;
}

/******************************************************************************
 * The PE Library Loader frontend. 
 * FIXME: handle the flags.
 */
WINE_MODREF *PE_LoadLibraryExA (LPCSTR name, DWORD flags)
{
	HMODULE		hModule32;
	WINE_MODREF	*wm;
	char        	filename[256];
	int hFile;
	WORD		version = 0;

	
	strncpy(filename, name, sizeof(filename));      
	hFile=open(filename, O_RDONLY);
	if(hFile==-1)
	    return NULL;
	
	
	hModule32 = PE_LoadImage( hFile, filename, &version );
	if (!hModule32)
	{
		SetLastError( ERROR_OUTOFMEMORY );	
		return NULL;
	}

	if ( !(wm = PE_CreateModule( hModule32, filename, flags, FALSE )) )
	{
		ERR( "can't load %s\n", filename );
		SetLastError( ERROR_OUTOFMEMORY );
		return NULL;
	}
	close(hFile);
	//printf("^^^^^^^^^^^^^^^^Alloc VM1  %p\n", wm);
	return wm;
}


/*****************************************************************************
 *	PE_UnloadLibrary
 *
 * Unload the library unmapping the image and freeing the modref structure.
 */
void PE_UnloadLibrary(WINE_MODREF *wm)
{
    TRACE(" unloading %s\n", wm->filename);

    if (wm->filename)
	free(wm->filename);
    if (wm->short_filename)
	free(wm->short_filename);
    HeapFree( GetProcessHeap(), 0, wm->deps );
    VirtualFree( (LPVOID)wm->module, 0, MEM_RELEASE );
    HeapFree( GetProcessHeap(), 0, wm );
    //printf("^^^^^^^^^^^^^^^^Free VM1  %p\n", wm);
}

/*****************************************************************************
 * Load the PE main .EXE. All other loading is done by PE_LoadLibraryExA
 * FIXME: this function should use PE_LoadLibraryExA, but currently can't
 * due to the PROCESS_Create stuff.
 */


/*
 * This is a dirty hack.
 * The win32 DLLs contain an alloca routine, that first probes the soon
 * to be allocated new memory *below* the current stack pointer in 4KByte
 * increments.  After the mem probing below the current %esp,  the stack
 * pointer is finally decremented to make room for the "alloca"ed memory.
 * Maybe the probing code is intended to extend the stack on a windows box.
 * Anyway, the linux kernel does *not* extend the stack by simply accessing
 * memory below %esp;  it segfaults.
 * The extend_stack_for_dll_alloca() routine just preallocates a big chunk
 * of memory on the stack, for use by the DLLs alloca routine.
 * Added the noinline attribute as e.g. gcc 3.2.2 inlines this function
 * in a way that breaks it.
 */
static void __attribute__((noinline)) extend_stack_for_dll_alloca(void)
{
#if !defined(__FreeBSD__) && !defined(__DragonFly__)
    volatile int* mem=alloca(0x20000);
    *mem=0x1234;
#endif
}

/* Called if the library is loaded or freed.
 * NOTE: if a thread attaches a DLL, the current thread will only do
 * DLL_PROCESS_ATTACH. Only new created threads do DLL_THREAD_ATTACH
 * (SDK)
 */
WIN_BOOL PE_InitDLL( WINE_MODREF *wm, DWORD type, LPVOID lpReserved )
{
    WIN_BOOL retv = TRUE;
    assert( wm->type == MODULE32_PE );

    
    if ((PE_HEADER(wm->module)->FileHeader.Characteristics & IMAGE_FILE_DLL) &&
        (PE_HEADER(wm->module)->OptionalHeader.AddressOfEntryPoint)
    ) {
	DLLENTRYPROC entry ;
	entry = (void*)PE_FindExportedFunction(wm, "DllMain", 0);
	if(entry==NULL)
	    entry = (void*)RVA_PTR( wm->module,OptionalHeader.AddressOfEntryPoint );
        
	TRACE_(relay)("CallTo32(entryproc=%p,module=%08x,type=%ld,res=%p)\n",
                       entry, wm->module, type, lpReserved );
	
	
	TRACE("Entering DllMain(");
	switch(type)
	{
	    case DLL_PROCESS_DETACH:
	        TRACE("DLL_PROCESS_DETACH) ");
		break;
	    case DLL_PROCESS_ATTACH:
	        TRACE("DLL_PROCESS_ATTACH) ");
		break;
	    case DLL_THREAD_DETACH:
	        TRACE("DLL_THREAD_DETACH) ");
		break;
	    case DLL_THREAD_ATTACH:
	        TRACE("DLL_THREAD_ATTACH) ");
		break;
	}	
	TRACE("for %s\n", wm->filename);
	extend_stack_for_dll_alloca();
        retv = entry( wm->module, type, lpReserved );
    }

    return retv;
}

static LPVOID
_fixup_address(PIMAGE_OPTIONAL_HEADER opt,int delta,LPVOID addr) {
	if (	((DWORD)addr>opt->ImageBase) &&
		((DWORD)addr<opt->ImageBase+opt->SizeOfImage)
	)
		
		return (LPVOID)(((DWORD)addr)+delta);
	else
		
		return addr;
}
