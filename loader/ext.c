/********************************************************
 *
 *
 *      Stub functions for Wine module
 *
 *
 ********************************************************/

/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <unistd.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#else
#include "osdep/mmap.h"
#endif
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "osdep/mmap_anon.h"
#include "wine/windef.h"
#include "wine/winbase.h"
#include "wine/debugtools.h"
#include "wine/heap.h"
#include "ext.h"

#if 0
//REMOVE SIMPLIFY
static void* mymalloc(unsigned int size)
{
    printf("malloc %d\n", size);
    return malloc(size);
}

#undef malloc
#define malloc mymalloc
#endif

int dbg_header_err( const char *dbg_channel, const char *func )
{
    return 0;
}
int dbg_header_warn( const char *dbg_channel, const char *func )
{
    return 0;
}
int dbg_header_fixme( const char *dbg_channel, const char *func )
{
    return 0;
}
int dbg_header_trace( const char *dbg_channel, const char *func )
{
    return 0;
}
int dbg_vprintf( const char *format, va_list args )
{
    return 0;
}
int __vprintf( const char *format, ... )
{
#ifdef DETAILED_OUT
    va_list va;
    va_start(va, format);
    vprintf(format, va);
    va_end(va);
#endif
    return 0;
}

HANDLE WINAPI GetProcessHeap(void)
{
    return 1;
}

LPVOID WINAPI HeapAlloc(HANDLE heap, DWORD flags, DWORD size)
{
    //static int i = 5;
    void* m = (flags & 0x8) ? calloc(size, 1) : malloc(size);
    //printf("HeapAlloc %p  %d  (%d)\n", m, size, flags);
    //if (--i == 0)
    //    abort();
    return m;
}

WIN_BOOL WINAPI HeapFree(HANDLE heap, DWORD flags, LPVOID mem)
{
    if (mem) free(mem);
    //printf("HeapFree  %p\n", mem);
    //if (!mem)
    //    abort();
    return 1;
}

static int last_error;

DWORD WINAPI GetLastError(void)
{
    return last_error;
}

VOID WINAPI SetLastError(DWORD error)
{
    last_error=error;
}

WIN_BOOL WINAPI ReadFile(HANDLE handle, LPVOID mem, DWORD size, LPDWORD result, LPOVERLAPPED flags)
{
    *result=read(handle, mem, size);
    return *result;
}
INT WINAPI lstrcmpiA(LPCSTR c1, LPCSTR c2)
{
    return strcasecmp(c1,c2);
}
LPSTR WINAPI lstrcpynA(LPSTR dest, LPCSTR src, INT num)
{
    return strncpy(dest,src,num);
}
INT WINAPI lstrlenA(LPCSTR s)
{
    return strlen(s);
}
INT WINAPI lstrlenW(LPCWSTR s)
{
    int l;
    if(!s)
	return 0;
    l=0;
    while(s[l])
	l++;
     return l;
}
LPSTR WINAPI lstrcpynWtoA(LPSTR dest, LPCWSTR src, INT count)
{
    LPSTR result = dest;
    int moved=0;
    if((dest==0) || (src==0))
	return 0;
    while(moved<count)
    {
        *dest=*src;
	moved++;
	if(*src==0)
	    break;
	src++;
	dest++;
    }
    return result;
}
/* i stands here for ignore case! */
int wcsnicmp(const unsigned short* s1, const unsigned short* s2, int n)
{
    /*
    if(s1==0)
	return;
    if(s2==0)
        return;
    */
    while(n>0)
    {
	if (((*s1 | *s2) & 0xff00) || toupper((char)*s1) != toupper((char)*s2))
	{

	    if(*s1<*s2)
		return -1;
	    else
		if(*s1>*s2)
		    return 1;
		else
		    if(*s1==0)
			return 0;
	}
	s1++;
	s2++;
	n--;
    }
    return 0;
}

WIN_BOOL WINAPI IsBadReadPtr(LPCVOID data, UINT size)
{
    if(size==0)
	return 0;
    if(data==NULL)
        return 1;
    return 0;
}
LPSTR HEAP_strdupA(HANDLE heap, DWORD flags, LPCSTR string)
{
//    return strdup(string);
    char* answ = (char*) malloc(strlen(string) + 1);
    strcpy(answ, string);
    return answ;
}
LPWSTR HEAP_strdupAtoW(HANDLE heap, DWORD flags, LPCSTR string)
{
    int size, i;
    WCHAR* answer;
    if(string==0)
	return 0;
    size=strlen(string);
    answer = (WCHAR*) malloc(sizeof(WCHAR) * (size + 1));
    for(i=0; i<=size; i++)
	answer[i]=(short)string[i];
    return answer;
}
LPSTR HEAP_strdupWtoA(HANDLE heap, DWORD flags, LPCWSTR string)
{
    int size, i;
    char* answer;
    if(string==0)
	return 0;
    size=0;
    while(string[size])
       size++;
    answer = (char*) malloc(size + 2);
    for(i=0; i<=size; i++)
	answer[i]=(char)string[i];
    return answer;
}

/***********************************************************************
 *           FILE_dommap
 */

//#define MAP_PRIVATE
//#define MAP_SHARED
LPVOID FILE_dommap( int unix_handle, LPVOID start,
                    DWORD size_high, DWORD size_low,
                    DWORD offset_high, DWORD offset_low,
                    int prot, int flags )
{
    int fd = -1;
    int pos;
    LPVOID ret;

    if (size_high || offset_high)
        printf("offsets larger than 4Gb not supported\n");

    if (unix_handle == -1)
    {
        ret = mmap_anon( start, size_low, prot, flags, offset_low );
    }
    else 
    {
        fd = unix_handle;
        ret = mmap( start, size_low, prot, flags, fd, offset_low );
    }

    if (ret != (LPVOID)-1)
    {
//	    printf("address %08x\n", *(int*)ret);
//	printf("%x\n", ret);
	    return ret;
    }

//    printf("mmap %d\n", errno);

    /* mmap() failed; if this is because the file offset is not    */
    /* page-aligned (EINVAL), or because the underlying filesystem */
    /* does not support mmap() (ENOEXEC), we do it by hand.        */

    if (unix_handle == -1) return ret;
    if ((errno != ENOEXEC) && (errno != EINVAL)) return ret;
    if (prot & PROT_WRITE)
    {
        /* We cannot fake shared write mappings */
#ifdef MAP_SHARED
	if (flags & MAP_SHARED) return ret;
#endif
#ifdef MAP_PRIVATE
	if (!(flags & MAP_PRIVATE)) return ret;
#endif
    }
/*    printf( "FILE_mmap: mmap failed (%d), faking it\n", errno );*/
    /* Reserve the memory with an anonymous mmap */
    ret = FILE_dommap( -1, start, size_high, size_low, 0, 0,
                       PROT_READ | PROT_WRITE, flags );
    if (ret == (LPVOID)-1)
//    {
//	perror(
	 return ret;
    /* Now read in the file */
    if ((pos = lseek( fd, offset_low, SEEK_SET )) == -1)
    {
        FILE_munmap( ret, size_high, size_low );
//	printf("lseek\n");
        return (LPVOID)-1;
    }
    read( fd, ret, size_low );
    lseek( fd, pos, SEEK_SET );  /* Restore the file pointer */
    mprotect( ret, size_low, prot );  /* Set the right protection */
//    printf("address %08x\n", *(int*)ret);
    return ret;
}


/***********************************************************************
 *           FILE_munmap
 */
int FILE_munmap( LPVOID start, DWORD size_high, DWORD size_low )
{
    if (size_high)
      printf("offsets larger than 4Gb not supported\n");
    return munmap( start, size_low );
}

struct file_mapping_s;
typedef struct file_mapping_s
{
    int mapping_size;
    char* name;
    LPVOID handle;
    struct file_mapping_s* next;
    struct file_mapping_s* prev;
}file_mapping;
static file_mapping* fm=0;



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

HANDLE WINAPI CreateFileMappingA(HANDLE handle, LPSECURITY_ATTRIBUTES lpAttr,
				 DWORD flProtect,
				 DWORD dwMaxHigh, DWORD dwMaxLow,
				 LPCSTR name)
{
    int hFile = (int)handle;
    unsigned int len;
    LPVOID answer;
    int anon=0;
    int mmap_access=0;
    if(hFile<0)
        anon=1;

    if(!anon)
    {
        len=lseek(hFile, 0, SEEK_END);
	lseek(hFile, 0, SEEK_SET);
    }
    else len=dwMaxLow;

    if(flProtect & PAGE_READONLY)
	mmap_access |=PROT_READ;
    else
	mmap_access |=PROT_READ|PROT_WRITE;

    if(anon)
        answer=mmap_anon(NULL, len, mmap_access, MAP_PRIVATE, 0);
    else
        answer=mmap(NULL, len, mmap_access, MAP_PRIVATE, hFile, 0);

    if(answer!=(LPVOID)-1)
    {
	if(fm==0)
	{
	    fm = (file_mapping*) malloc(sizeof(file_mapping));
	    fm->prev=NULL;
	}
	else
	{
	    fm->next = (file_mapping*) malloc(sizeof(file_mapping));
	    fm->next->prev=fm;
	    fm=fm->next;
	}
	fm->next=NULL;
	fm->handle=answer;
	if(name)
	{
	    fm->name = (char*) malloc(strlen(name)+1);
	    strcpy(fm->name, name);
	}
	else
	    fm->name=NULL;
	fm->mapping_size=len;

	return (HANDLE)answer;
    }
    return (HANDLE)0;
}
WIN_BOOL WINAPI UnmapViewOfFile(LPVOID handle)
{
    file_mapping* p;
    int result;
    if(fm==0)
	return 0;
    for(p=fm; p; p=p->next)
    {
	if(p->handle==handle)
	{
	    result=munmap((void*)handle, p->mapping_size);
	    if(p->next)p->next->prev=p->prev;
	    if(p->prev)p->prev->next=p->next;
	    if(p->name)
		free(p->name);
	    if(p==fm)
		fm=p->prev;
	    free(p);
	    return result;
	}
    }
    return 0;
}
//static int va_size=0;
struct virt_alloc_s;
typedef struct virt_alloc_s
{
    int mapping_size;
    char* address;
    struct virt_alloc_s* next;
    struct virt_alloc_s* prev;
    int state;
}virt_alloc;
static virt_alloc* vm=0;
#define MEM_COMMIT              0x00001000
#define MEM_RESERVE             0x00002000

LPVOID WINAPI VirtualAlloc(LPVOID address, DWORD size, DWORD type,  DWORD protection)
{
    void* answer;
    long pgsz;

    //printf("VirtualAlloc(0x%08X, %u, 0x%08X, 0x%08X)\n", (unsigned)address, size, type, protection);

    if ((type&(MEM_RESERVE|MEM_COMMIT)) == 0) return NULL;

    if (type&MEM_RESERVE && (unsigned)address&0xffff) {
	size += (unsigned)address&0xffff;
	address = (unsigned)address&~0xffff;
    }
    pgsz = sysconf(_SC_PAGESIZE);
    if (type&MEM_COMMIT && (unsigned)address%pgsz) {
	size += (unsigned)address%pgsz;
	address -= (unsigned)address%pgsz;
    }

    if (type&MEM_RESERVE && size<0x10000) size = 0x10000;
    if (size%pgsz) size += pgsz - size%pgsz;

    if(address!=0)
    {
    //check whether we can allow to allocate this
        virt_alloc* str=vm;
        while(str)
        {
	    if((unsigned)address>=(unsigned)str->address+str->mapping_size)
	    {
		str=str->prev;
		continue;
	    }
	    if((unsigned)address+size<=(unsigned)str->address)
	    {
		str=str->prev;
		continue;
	    }
	    if(str->state==0)
	    {
#warning FIXME
		if(   ((unsigned)address >= (unsigned)str->address)
		   && ((unsigned)address+size<=(unsigned)str->address+str->mapping_size)
		   && (type & MEM_COMMIT))
		{
		    return address; //returning previously reserved memory
		}
		//printf(" VirtualAlloc(...) does not commit or not entirely within reserved, and\n");
	    }
	    /*printf(" VirtualAlloc(...) (0x%08X, %u) overlaps with (0x%08X, %u, state=%d)\n",
	           (unsigned)address, size, (unsigned)str->address, str->mapping_size, str->state);*/
	    return NULL;
	}
    }

    answer=mmap_anon(address, size, PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_PRIVATE, 0);
//    answer=FILE_dommap(-1, address, 0, size, 0, 0,
//	PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE);

    if (answer != (void *)-1 && address && answer != address) {
	/* It is dangerous to try mmap() with MAP_FIXED since it does not
	   always detect conflicts or non-allocation and chaos ensues after
	   a successful call but an overlapping or non-allocated region.  */
	munmap(answer, size);
	answer = (void *) -1;
	errno = EINVAL;
	//printf(" VirtualAlloc(...) cannot satisfy requested address but address=NULL would work.\n");
    }
    if(answer==(void*)-1)
    {
	/*printf(" VirtualAlloc(...) mmap(0x%08X, %u, ...) failed with errno=%d (\"%s\")\n",
	       (unsigned)address, size, errno, strerror(errno));*/
	return NULL;
    }
    else
    {
	virt_alloc *new_vm = (virt_alloc*) malloc(sizeof(virt_alloc));
	new_vm->mapping_size=size;
	new_vm->address=(char*)answer;
        new_vm->prev=vm;
	if(type == MEM_RESERVE)
	    new_vm->state=0;
	else
	    new_vm->state=1;
	if(vm)
	    vm->next=new_vm;
    	vm=new_vm;
	vm->next=0;
	//if(va_size!=0)
	//    printf("Multiple VirtualAlloc!\n");
	//printf(" VirtualAlloc(...) provides (0x%08X, %u)\n", (unsigned)answer, size);
        return answer;
    }
}

WIN_BOOL WINAPI VirtualFree(LPVOID  address, SIZE_T dwSize, DWORD dwFreeType)//not sure
{
    virt_alloc* str=vm;
    int answer;

    //printf("VirtualFree(0x%08X, %d, 0x%08X)\n", (unsigned)address, dwSize, dwFreeType);
    while(str)
    {
	if(address!=str->address)
	{
	    str=str->prev;
	    continue;
	}
	//printf(" VirtualFree(...) munmap(0x%08X, %d)\n", (unsigned)str->address, str->mapping_size);
	answer=munmap(str->address, str->mapping_size);
	if(str->next)str->next->prev=str->prev;
	if(str->prev)str->prev->next=str->next;
	if(vm==str)vm=str->prev;
	free(str);
	return 0;
    }
    return -1;
}

INT WINAPI WideCharToMultiByte(UINT codepage, DWORD flags, LPCWSTR src,
     INT srclen,LPSTR dest, INT destlen, LPCSTR defch, WIN_BOOL* used_defch)
{
    int i;
    if(srclen==-1){srclen=0; while(src[srclen++]);}
    if(destlen==0)
	return srclen;
    if(used_defch)
	*used_defch=0;
    for(i=0; i<min(srclen, destlen); i++)
	*dest++=(char)*src++;
    return min(srclen, destlen);
}
INT WINAPI MultiByteToWideChar(UINT codepage,DWORD flags, LPCSTR src, INT srclen,
    LPWSTR dest, INT destlen)
{
    int i;
    if(srclen==-1){srclen=0; while(src[srclen++]);}
    if(destlen==0)
	return srclen;
    for(i=0; i<min(srclen, destlen); i++)
	*dest++=(WCHAR)*src++;
    return min(srclen, destlen);
}
HANDLE WINAPI OpenFileMappingA(DWORD access, WIN_BOOL prot, LPCSTR name)
{
    file_mapping* p;
    if(fm==0)
	return (HANDLE)0;
    if(name==0)
	return (HANDLE)0;
    for(p=fm; p; p=p->prev)
    {
	if(p->name==0)
	    continue;
	if(strcmp(p->name, name)==0)
	    return (HANDLE)p->handle;
    }
    return 0;
}
