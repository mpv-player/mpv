/********************************************************
 * 
 *
 *      Stub functions for Wine module
 *
 *
 ********************************************************/
#include <config.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <wine/windef.h>
//#include <wine/winbase.h>
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
int dbg_vprintf( const char *format, ... )
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
    
int GetProcessHeap()
{
    return 1;
}

void* HeapAlloc(int heap, int flags, int size)
{
    if(flags & 0x8)
	return calloc(size, 1);
	else
	return malloc(size);
}

int HeapFree(int heap, int flags, void* mem)
{
    if (mem) free(mem);
    return 1;
}     	

static int last_error;

int GetLastError()
{
    return last_error;
}

int SetLastError(int error)
{ 
    return last_error=error;
}    

int ReadFile(int handle, void* mem, unsigned long size, long* result, long flags)
{
    *result=read(handle, mem, size);
    return *result;
}    
int lstrcmpiA(const char* c1, const char* c2)
{
    return strcasecmp(c1,c2);
}
int lstrcpynA(char* dest, const char* src, int num)
{
    return strncmp(dest,src,num);
}
int lstrlenA(const char* s)
{
    return strlen(s);
}   
int lstrlenW(const short* s)
{
    int l;
    if(!s)
	return 0;
    l=0;
    while(s[l])
	l++;
     return l;
}
int lstrcpynWtoA(char* dest, const char* src, int count)
{
    int moved=0;
    if((dest==0) || (src==0))
	return 0;
    while(moved<count)
    {
        *dest=*src;
	moved++;
	if(*src==0)
	    return moved;
	src++;
	dest++;
    }
}
int wcsnicmp(const unsigned short* s1, const unsigned short* s2, int n)
{
    if(s1==0)
	return;
    if(s2==0)
        return;
    while(n>0)
    {
	if(*s1<*s2)
	    return -1;
	else
    	    if(*s1>*s2)
		return 1;
	    else
		if(*s1==0)
		    return 0;
    s1++;
    s2++;
    n--;
    }
    return 0;
}			
		
		
int IsBadReadPtr(void* data, int size)
{
    if(size==0)
	return 0;
    if(data==NULL)
        return 1;
    return 0;
}   
char* HEAP_strdupA(const char* string)
{
//    return strdup(string);
    char* answ=malloc(strlen(string)+1);
    strcpy(answ, string);
    return answ;
}
short* HEAP_strdupAtoW(void* heap, void* hz, const char* string)
{
    int size, i;
    short* answer;
    if(string==0)
	return 0;
    size=strlen(string);
    answer=malloc(size+size+2);
    for(i=0; i<=size; i++)
	answer[i]=(short)string[i];
    return answer;	
}
char* HEAP_strdupWtoA(void* heap, void* hz, const short* string)
{
    int size, i;
    char* answer;
    if(string==0)
	return 0;
    size=0;
    while(string[size])
       size++;
    answer=malloc(size+2);
    for(i=0; i<=size; i++)
	answer[i]=(char)string[i];
    return answer;	
}

/***********************************************************************
 *           FILE_dommap
 */

//#define MAP_PRIVATE
//#define MAP_SHARED
#undef MAP_ANON
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
#ifdef MAP_ANON
//	printf("Anonymous\n");
        flags |= MAP_ANON;
#else
        static int fdzero = -1;

        if (fdzero == -1)
        {
            if ((fdzero = open( "/dev/zero", O_RDONLY )) == -1)
            {
                perror( "/dev/zero: open" );
                exit(1);
            }
        }
        fd = fdzero;
#endif  /* MAP_ANON */
	/* Linux EINVAL's on us if we don't pass MAP_PRIVATE to an anon mmap */
#ifdef MAP_SHARED
	flags &= ~MAP_SHARED;
#endif
#ifdef MAP_PRIVATE
	flags |= MAP_PRIVATE;
#endif
    }
    else fd = unix_handle;
//    printf("fd %x, start %x, size %x, pos %x, prot %x\n",fd,start,size_low, offset_low, prot);
//    if ((ret = mmap( start, size_low, prot,
//                     flags, fd, offset_low )) != (LPVOID)-1)
    if ((ret = mmap( start, size_low, prot,
                     MAP_PRIVATE | MAP_FIXED, fd, offset_low )) != (LPVOID)-1)
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
static int mapping_size=0;

struct file_mapping_s;
typedef struct file_mapping_s
{
    int mapping_size;
    char* name;
    HANDLE handle;
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

HANDLE CreateFileMappingA(int hFile, void* lpAttr,
DWORD flProtect, DWORD dwMaxHigh, DWORD dwMaxLow, const char* name)
{
    unsigned int len;
    HANDLE answer;
    int anon=0;
    int mmap_access=0;
    if(hFile<0)
    {
	anon=1;
	hFile=open("/dev/zero", O_RDWR);
	if(hFile<0)
	    return 0;
    }	    
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
	
    answer=(HANDLE)mmap(NULL, len, mmap_access, MAP_PRIVATE, hFile, 0);    
    if(anon)
        close(hFile);
    if(answer!=(HANDLE)-1)
    {
	if(fm==0)
	{
	    fm=malloc(sizeof(file_mapping));
	    fm->prev=NULL;
	}    
	else
	{
	    fm->next=malloc(sizeof(file_mapping));
	    fm->next->prev=fm;
	    fm=fm->next;
	}
	fm->next=NULL;    
	fm->handle=answer;
	if(name)
	{
	    fm->name=malloc(strlen(name)+1);
	    strcpy(fm->name, name);
	}    
	else
	    fm->name=NULL;
	fm->mapping_size=len;
	
	if(anon)
	    close(hFile);
	return answer;
    }
    return (HANDLE)0;
}        
int UnmapViewOfFile(HANDLE handle)
{
    file_mapping* p;
    int result;
    if(fm==0)
	return (HANDLE)0;
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

void* VirtualAlloc(void* address, DWORD size, DWORD type,  DWORD protection)
{
    void* answer;
    int fd=open("/dev/zero", O_RDWR);
    size=(size+0xffff)&(~0xffff);
//    printf("VirtualAlloc(0x%08X, %d)\n", address
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
	    if((unsigned)address+size<(unsigned)str->address)
	    {
		str=str->prev;
		continue;
	    }
	    if(str->state==0)
	    {
#warning FIXME
		if(((unsigned)address+size<(unsigned)str->address+str->mapping_size) && (type & MEM_COMMIT))
		{
		    close(fd);
		    return address; //returning previously reserved memory
		}
		return NULL;    
	    }
	    close(fd);
	    return NULL;
	}
	answer=mmap(address, size, PROT_READ | PROT_WRITE | PROT_EXEC, 
	    MAP_FIXED | MAP_PRIVATE, fd, 0);
    }	    
    else
    answer=mmap(address, size, PROT_READ | PROT_WRITE | PROT_EXEC, 
	 MAP_PRIVATE, fd, 0);
//    answer=FILE_dommap(-1, address, 0, size, 0, 0,
//	PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE);
     close(fd);
    if(answer==(void*)-1)
    {
	printf("Error no %d\n", errno);
	printf("VirtualAlloc(0x%08X, %d) failed\n", address, size);
	return NULL;
    }
    else
    {
	virt_alloc *new_vm=malloc(sizeof(virt_alloc));
	new_vm->mapping_size=size;
	new_vm->address=answer;
        new_vm->prev=vm;
	if(type == MEM_RESERVE)
	    new_vm->state=0;
	else
	    new_vm->state=1;
	if(vm)
	    vm->next=new_vm;
    	vm=new_vm;
	vm->next=0;
//	if(va_size!=0)
//	    printf("Multiple VirtualAlloc!\n");	    
//	printf("answer=0x%08x\n", answer);
        return answer;
    }	
}    	
int VirtualFree(void*  address, int t1, int t2)//not sure
{
    virt_alloc* str=vm;
    int answer;
    while(str)
    {
    if(address!=str->address)
    {
	str=str->prev;
	continue;
    }	
    answer=munmap(str->address, str->mapping_size);
    if(str->next)str->next->prev=str->prev;
    if(str->prev)str->prev->next=str->next;
    if(vm==str)vm=0;
    free(str);
    return 0;
    }
    return -1;
}

int WideCharToMultiByte(unsigned int codepage, long flags, const short* src,
     int srclen,char* dest, int destlen, const char* defch, int* used_defch)
{
    int i;
    if(src==0)
	return 0;
    if(srclen==-1){srclen=0; while(src[srclen++]);}
//    for(i=0; i<srclen; i++)
//	printf("%c", src[i]);
//    printf("\n");		
    if(dest==0)
    {
    for(i=0; i<srclen; i++)
    {
	src++;
    	if(*src==0)
	    return i+1;
    }
	return srclen+1;	    
    }	
    if(used_defch)
	*used_defch=0;	
    for(i=0; i<min(srclen, destlen); i++)
    {
	*dest=(char)*src;
	dest++;
	src++;
	if(*src==0)
	    return i+1;
    }	    
    return min(srclen, destlen);
}
int MultiByteToWideChar(unsigned int codepage,long flags, const char* src, int srclen,
    short* dest, int destlen)
{
    return 0;
}
HANDLE OpenFileMappingA(long access, long prot, char* name)
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
	    return p->handle;
    }
    return 0;	
}
