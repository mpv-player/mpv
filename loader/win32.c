/***********************************************************

Win32 emulation code. Functions that emulate
responses from corresponding Win32 API calls.
Since we are not going to be able to load
virtually any DLL, we can only implement this
much, adding needed functions with each new codec.

Basic principle of implementation: it's not good
for DLL to know too much about its environment.

************************************************************/

#include "config.h"

#include "wine/winbase.h"
#include "wine/winreg.h"
#include "wine/winnt.h"
#include "wine/winerror.h"
#include "wine/debugtools.h"
#include "wine/module.h"

#include <stdio.h>
#include "win32.h"

#include "registry.h"
#include "loader.h"
#include "com.h"

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#ifdef	HAVE_KSTAT
#include <kstat.h>
#endif

#if HAVE_VSSCANF
int vsscanf( const char *str, const char *format, va_list ap);
#else
/* system has no vsscanf.  try to provide one */
static int vsscanf( const char *str, const char *format, va_list ap)
{
    long p1 = va_arg(ap, long);
    long p2 = va_arg(ap, long);
    long p3 = va_arg(ap, long);
    long p4 = va_arg(ap, long);
    long p5 = va_arg(ap, long);
    return sscanf(str, format, p1, p2, p3, p4, p5);
}
#endif

char* def_path = WIN32_PATH;

static void do_cpuid(unsigned int ax, unsigned int *regs)
{
    __asm__ __volatile__
	(
	 "pushl %%ebx; pushl %%ecx; pushl %%edx;"
	 ".byte  0x0f, 0xa2;"
	 "movl   %%eax, (%2);"
	 "movl   %%ebx, 4(%2);"
	 "movl   %%ecx, 8(%2);"
	 "movl   %%edx, 12(%2);"
	 "popl %%edx; popl %%ecx; popl %%ebx;"
	 : "=a" (ax)
	 :  "0" (ax), "S" (regs)
	);
}
static unsigned int c_localcount_tsc()
{
    int a;
    __asm__ __volatile__
	(
	 "rdtsc\n\t"
	 :"=a"(a)
	 :
	 :"edx"
	);
    return a;
}
static void c_longcount_tsc(long long* z)
{
    __asm__ __volatile__
	(
	 "pushl %%ebx\n\t"
	 "movl %%eax, %%ebx\n\t"
	 "rdtsc\n\t"
	 "movl %%eax, 0(%%ebx)\n\t"
	 "movl %%edx, 4(%%ebx)\n\t"
	 "popl %%ebx\n\t"
	 ::"a"(z)
	);
}
static unsigned int c_localcount_notsc()
{
    struct timeval tv;
    unsigned limit=~0;
    limit/=1000000;
    gettimeofday(&tv, 0);
    return limit*tv.tv_usec;
}
static void c_longcount_notsc(long long* z)
{
    struct timeval tv;
    unsigned long long result;
    unsigned limit=~0;
    if(!z)return;
    limit/=1000000;
    gettimeofday(&tv, 0);
    result=tv.tv_sec;
    result<<=32;
    result+=limit*tv.tv_usec;
    *z=result;
}
static unsigned int localcount_stub(void);
static void longcount_stub(long long*);
static unsigned int (*localcount)()=localcount_stub;
static void (*longcount)(long long*)=longcount_stub;

static pthread_mutex_t memmut;

static unsigned int localcount_stub(void)
{
    unsigned int regs[4];
    do_cpuid(1, regs);
    if ((regs[3] & 0x00000010) != 0)
    {
	localcount=c_localcount_tsc;
	longcount=c_longcount_tsc;
    }
    else
    {
	localcount=c_localcount_notsc;
	longcount=c_longcount_notsc;
    }
    return localcount();
}
static void longcount_stub(long long* z)
{
    unsigned int regs[4];
    do_cpuid(1, regs);
    if ((regs[3] & 0x00000010) != 0)
    {
	localcount=c_localcount_tsc;
	longcount=c_longcount_tsc;
    }
    else
    {
	localcount=c_localcount_notsc;
	longcount=c_longcount_notsc;
    }
    longcount(z);
}

int LOADER_DEBUG=1; // active only if compiled with -DDETAILED_OUT
//#define DETAILED_OUT
static inline void dbgprintf(char* fmt, ...)
{
#ifdef DETAILED_OUT
    if(LOADER_DEBUG)
    {
	FILE* f;
	va_list va;
	va_start(va, fmt);
	f=fopen("./log", "a");
	vprintf(fmt, va);
	fflush(stdout);
	if(f)
	{
	    vfprintf(f, fmt, va);
	    fsync(fileno(f));
	    fclose(f);
	}
	va_end(va);
    }
#endif
//#undef MPLAYER
#ifdef MPLAYER
    #include "../mp_msg.h"
    if (verbose > 2)
    {
	va_list va;
	
	va_start(va, fmt);
//	vprintf(fmt, va);
	mp_dbg(MSGT_WIN32, MSGL_DBG3, fmt, va);
	va_end(va);
    }
#endif
}


char export_names[300][32]={
    "name1",
    //"name2",
    //"name3"
};
//#define min(x,y) ((x)<(y)?(x):(y))

void destroy_event(void* event);

struct th_list_t;
typedef struct th_list_t{
    int id;
    void* thread;
    struct th_list_t* next;
    struct th_list_t* prev;
} th_list;


// have to be cleared by GARBAGE COLLECTOR
static unsigned char* heap=NULL;
static int heap_counter=0;
static tls_t* g_tls=NULL;
static th_list* list=NULL;

static void test_heap(void)
{
    int offset=0;
    if(heap==0)
	return;
    while(offset<heap_counter)
    {
	if(*(int*)(heap+offset)!=0x433476)
	{
	    printf("Heap corruption at address %d\n", offset);
	    return;
	}
	offset+=8+*(int*)(heap+offset+4);
    }
    for(;offset<min(offset+1000, 20000000); offset++)
	if(heap[offset]!=0xCC)
	{
	    printf("Free heap corruption at address %d\n", offset);
	}
}
#undef MEMORY_DEBUG

#ifdef MEMORY_DEBUG

static void* my_mreq(int size, int to_zero)
{
    static int test=0;
    test++;
    if(test%10==0)printf("Memory: %d bytes allocated\n", heap_counter);
    //    test_heap();
    if(heap==NULL)
    {
	heap=malloc(20000000);
	memset(heap, 0xCC,20000000);
    }
    if(heap==0)
    {
	printf("No enough memory\n");
	return 0;
    }
    if(heap_counter+size>20000000)
    {
	printf("No enough memory\n");
	return 0;
    }
    *(int*)(heap+heap_counter)=0x433476;
    heap_counter+=4;
    *(int*)(heap+heap_counter)=size;
    heap_counter+=4;
    printf("Allocated %d bytes of memory: sys %d, user %d-%d\n", size, heap_counter-8, heap_counter, heap_counter+size);
    if(to_zero)
	memset(heap+heap_counter, 0, size);
    else
	memset(heap+heap_counter, 0xcc, size);  // make crash reproducable
    heap_counter+=size;
    return heap+heap_counter-size;
}
static int my_release(char* memory)
{
    //    test_heap();
    if(memory==NULL)
    {
	printf("ERROR: free(0)\n");
	return 0;
    }
    if(*(int*)(memory-8)!=0x433476)
    {
	printf("MEMORY CORRUPTION !!!!!!!!!!!!!!!!!!!\n");
	return 0;
    }
    printf("Freed %d bytes of memory\n", *(int*)(memory-4));
    //    memset(memory-8, *(int*)(memory-4), 0xCC);
    return 0;
}

#else
#define GARBAGE
typedef struct alloc_header_t alloc_header;
struct alloc_header_t
{
    // let's keep allocated data 16 byte aligned
    alloc_header* prev;
    alloc_header* next;
    long deadbeef;
    long size;
    long type;
    long reserved1;
    long reserved2;
    long reserved3;
};

#ifdef GARBAGE
static alloc_header* last_alloc = NULL;
static int alccnt = 0;
#endif

#define AREATYPE_CLIENT 0
#define AREATYPE_EVENT 1
#define AREATYPE_MUTEX 2
#define AREATYPE_COND 3
#define AREATYPE_CRITSECT 4

/* -- critical sections -- */
struct CRITSECT
{
    pthread_t id;
    pthread_mutex_t mutex;
    int locked;
    long deadbeef;
};

void* mreq_private(int size, int to_zero, int type);
void* mreq_private(int size, int to_zero, int type)
{
    int nsize = size + sizeof(alloc_header);
    alloc_header* header = (alloc_header* ) malloc(nsize);
    if (!header)
        return 0;
    if (to_zero)
	memset(header, 0, nsize);
#ifdef GARBAGE
    if (!last_alloc)
    {
	pthread_mutex_init(&memmut, NULL);
	pthread_mutex_lock(&memmut);
    }
    else
    {
	pthread_mutex_lock(&memmut);
	last_alloc->next = header;  /* set next */
    }

    header->prev = last_alloc;
    header->next = 0;
    last_alloc = header;
    alccnt++;
    pthread_mutex_unlock(&memmut);
#endif
    header->deadbeef = 0xdeadbeef;
    header->size = size;
    header->type = type;

    //if (alccnt < 40000) printf("MY_REQ: %p\t%d   t:%d  (cnt:%d)\n",  header, size, type, alccnt);
    return header + 1;
}

static int my_release(void* memory)
{
    alloc_header* header = (alloc_header*) memory - 1;
#ifdef GARBAGE
    alloc_header* prevmem;
    alloc_header* nextmem;

    if (memory == 0)
	return 0;

    if (header->deadbeef != (long) 0xdeadbeef)
    {
	printf("FATAL releasing corrupted memory! %p  0x%lx  (%d)\n", header, header->deadbeef, alccnt);
	return 0;
    }

    pthread_mutex_lock(&memmut);

    switch(header->type)
    {
    case AREATYPE_EVENT:
	destroy_event(memory);
	break;
    case AREATYPE_COND:
	pthread_cond_destroy((pthread_cond_t*)memory);
	break;
    case AREATYPE_MUTEX:
	pthread_mutex_destroy((pthread_mutex_t*)memory);
	break;
    case AREATYPE_CRITSECT:
	pthread_mutex_destroy(&((struct CRITSECT*)memory)->mutex);
	break;
    default:
	//memset(memory, 0xcc, header->size);
	;
    }

    header->deadbeef = 0;
    prevmem = header->prev;
    nextmem = header->next;

    if (prevmem)
	prevmem->next = nextmem;
    if (nextmem)
	nextmem->prev = prevmem;

    if (header == last_alloc)
	last_alloc = prevmem;

    alccnt--;

    if (last_alloc)
	pthread_mutex_unlock(&memmut);
    else
	pthread_mutex_destroy(&memmut);

    //if (alccnt < 40000) printf("MY_RELEASE: %p\t%ld    (%d)\n", header, header->size, alccnt);
#else
    if (memory == 0)
	return 0;
#endif
    //memset(header + 1, 0xcc, header->size);
    free(header);
    return 0;
}
#endif

static inline void* my_mreq(int size, int to_zero)
{
    return mreq_private(size, to_zero, AREATYPE_CLIENT);
}

static int my_size(void* memory)
{
    if(!memory) return 0;
    return ((alloc_header*)memory)[-1].size;
}

static void* my_realloc(void* memory, int size)
{
    void *ans = memory;
    int osize;
    if (memory == NULL)
	return my_mreq(size, 0);
    osize = my_size(memory);
    if (osize < size)
    {
	ans = my_mreq(size, 0);
	memcpy(ans, memory, osize);
	my_release(memory);
    }
    return ans;
}

/*
 *
 *  WINE  API  - native implementation for several win32 libraries
 *
 */

static int WINAPI ext_unknown()
{
    printf("Unknown func called\n");
    return 0;
}

static int WINAPI expIsBadWritePtr(void* ptr, unsigned int count)
{
    int result = (count == 0 || ptr != 0) ? 0 : 1;
    dbgprintf("IsBadWritePtr(0x%x, 0x%x) => %d\n", ptr, count, result);
    return result;
}
static int WINAPI expIsBadReadPtr(void* ptr, unsigned int count)
{
    int result = (count == 0 || ptr != 0) ? 0 : 1;
    dbgprintf("IsBadReadPtr(0x%x, 0x%x) => %d\n", ptr, count, result);
    return result;
}
static int WINAPI expDisableThreadLibraryCalls(int module)
{
    dbgprintf("DisableThreadLibraryCalls(0x%x) => 0\n", module);
    return 0;
}

static HMODULE WINAPI expGetDriverModuleHandle(DRVR* pdrv)
{
    HMODULE result;
    if (pdrv==NULL)
	result=0;
    else
	result=pdrv->hDriverModule;
    dbgprintf("GetDriverModuleHandle(%p) => %p\n", pdrv, result);
    return result;
}

#define	MODULE_HANDLE_kernel32	((HMODULE)0x120)
#define	MODULE_HANDLE_user32	((HMODULE)0x121)

static HMODULE WINAPI expGetModuleHandleA(const char* name)
{
    WINE_MODREF* wm;
    HMODULE result;
    if(!name)
	result=0;
    else
    {
	wm=MODULE_FindModule(name);
	if(wm==0)result=0;
	else
	    result=(HMODULE)(wm->module);
    }
    if(!result)
    {
	if(name && strcasecmp(name, "kernel32")==0)
	    result=MODULE_HANDLE_kernel32;
    }
    dbgprintf("GetModuleHandleA('%s') => 0x%x\n", name, result);
    return result;
}


static void* WINAPI expCreateThread(void* pSecAttr, long dwStackSize,
				    void* lpStartAddress, void* lpParameter,
				    long dwFlags, long* dwThreadId)
{
    pthread_t *pth;
    //    printf("CreateThread:");
    pth = (pthread_t*) my_mreq(sizeof(pthread_t), 0);
    pthread_create(pth, NULL, (void*(*)(void*))lpStartAddress, lpParameter);
    if(dwFlags)
	printf( "WARNING: CreateThread flags not supported\n");
    if(dwThreadId)
	*dwThreadId=(long)pth;
    if(list==NULL)
    {
	list=my_mreq(sizeof(th_list), 1);
	list->next=list->prev=NULL;
    }
    else
    {
	list->next=my_mreq(sizeof(th_list), 0);
	list->next->prev=list;
	list->next->next=NULL;
	list=list->next;
    }
    list->thread=pth;
    dbgprintf("CreateThread(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x) => 0x%x\n",
	      pSecAttr, dwStackSize, lpStartAddress, lpParameter, dwFlags, dwThreadId, pth);
    return pth;
}

struct mutex_list_t;

struct mutex_list_t
{
    char type;
    pthread_mutex_t *pm;
    pthread_cond_t  *pc;
    char state;
    char reset;
    char name[128];
    int  semaphore;
    struct mutex_list_t* next;
    struct mutex_list_t* prev;
};
typedef struct mutex_list_t mutex_list;
static mutex_list* mlist=NULL;

void destroy_event(void* event)
{
    mutex_list* pp=mlist;
    //    printf("garbage collector: destroy_event(%x)\n", event);
    while(pp)
    {
	if(pp==(mutex_list*)event)
	{
	    if(pp->next)
		pp->next->prev=pp->prev;
	    if(pp->prev)
		pp->prev->next=pp->next;
	    if(mlist==(mutex_list*)event)
		mlist=mlist->prev;
	    /*
	     pp=mlist;
	     while(pp)
	     {
	     printf("%x => ", pp);
	     pp=pp->prev;
	     }
	     printf("0\n");
	     */
	    return;
	}
	pp=pp->prev;
    }
}

static void* WINAPI expCreateEventA(void* pSecAttr, char bManualReset,
				    char bInitialState, const char* name)
{
    pthread_mutex_t *pm;
    pthread_cond_t  *pc;
    /*
     mutex_list* pp;
     pp=mlist;
     while(pp)
     {
     printf("%x => ", pp);
     pp=pp->prev;
     }
     printf("0\n");
     */
    if(mlist!=NULL)
    {
	mutex_list* pp=mlist;
	if(name!=NULL)
	    do
	{
	    if((strcmp(pp->name, name)==0) && (pp->type==0))
	    {
		dbgprintf("CreateEventA(0x%x, 0x%x, 0x%x, 0x%x='%s') => 0x%x\n",
			  pSecAttr, bManualReset, bInitialState, name, name, pp->pm);
		return pp->pm;
	    }
	}while((pp=pp->prev) != NULL);
    }
    pm=mreq_private(sizeof(pthread_mutex_t), 0, AREATYPE_MUTEX);
    pthread_mutex_init(pm, NULL);
    pc=mreq_private(sizeof(pthread_cond_t), 0, AREATYPE_COND);
    pthread_cond_init(pc, NULL);
    if(mlist==NULL)
    {
	mlist=mreq_private(sizeof(mutex_list), 00, AREATYPE_EVENT);
	mlist->next=mlist->prev=NULL;
    }
    else
    {
	mlist->next=mreq_private(sizeof(mutex_list), 00, AREATYPE_EVENT);
	mlist->next->prev=mlist;
	mlist->next->next=NULL;
	mlist=mlist->next;
    }
    mlist->type=0; /* Type Event */
    mlist->pm=pm;
    mlist->pc=pc;
    mlist->state=bInitialState;
    mlist->reset=bManualReset;
    if(name)
	strncpy(mlist->name, name, 127);
    else
	mlist->name[0]=0;
    if(pm==NULL)
	dbgprintf("ERROR::: CreateEventA failure\n");
    /*
     if(bInitialState)
     pthread_mutex_lock(pm);
     */
    if(name)
	dbgprintf("CreateEventA(0x%x, 0x%x, 0x%x, 0x%x='%s') => 0x%x\n",
		  pSecAttr, bManualReset, bInitialState, name, name, mlist);
    else
	dbgprintf("CreateEventA(0x%x, 0x%x, 0x%x, NULL) => 0x%x\n",
		  pSecAttr, bManualReset, bInitialState, mlist);
    return mlist;
}

static void* WINAPI expSetEvent(void* event)
{
    mutex_list *ml = (mutex_list *)event;
    dbgprintf("SetEvent(%x) => 0x1\n", event);
    pthread_mutex_lock(ml->pm);
    if (ml->state == 0) {
	ml->state = 1;
	pthread_cond_signal(ml->pc);
    }
    pthread_mutex_unlock(ml->pm);

    return (void *)1;
}
static void* WINAPI expResetEvent(void* event)
{
    mutex_list *ml = (mutex_list *)event;
    dbgprintf("ResetEvent(0x%x) => 0x1\n", event);
    pthread_mutex_lock(ml->pm);
    ml->state = 0;
    pthread_mutex_unlock(ml->pm);

    return (void *)1;
}

static void* WINAPI expWaitForSingleObject(void* object, int duration)
{
    mutex_list *ml = (mutex_list *)object;
    // FIXME FIXME FIXME - this value is sometime unititialize !!!
    int ret = WAIT_FAILED;
    mutex_list* pp=mlist;
    if(object == (void*)0xcfcf9898)
    {
	/**
	 From GetCurrentThread() documentation:
	 A pseudo handle is a special constant that is interpreted as the current thread handle. The calling thread can use this handle to specify itself whenever a thread handle is required. Pseudo handles are not inherited by child processes.

	 This handle has the maximum possible access to the thread object. For systems that support security descriptors, this is the maximum access allowed by the security descriptor for the calling process. For systems that do not support security descriptors, this is THREAD_ALL_ACCESS.

	 The function cannot be used by one thread to create a handle that can be used by other threads to refer to the first thread. The handle is always interpreted as referring to the thread that is using it. A thread can create a "real" handle to itself that can be used by other threads, or inherited by other processes, by specifying the pseudo handle as the source handle in a call to the DuplicateHandle function.
	 **/
	dbgprintf("WaitForSingleObject(thread_handle) called\n");
	return (void*)WAIT_FAILED;
    }
    dbgprintf("WaitForSingleObject(0x%x, duration %d) =>\n",object, duration);

    // loop below was slightly fixed - its used just for checking if
    // this object really exists in our list
    if (!ml)
	return (void*) ret;
    while (pp && (pp->pm != ml->pm))
	pp = pp->prev;
    if (!pp) {
	dbgprintf("WaitForSingleObject: NotFound\n");
	return (void*)ret;
    }

    pthread_mutex_lock(ml->pm);

    switch(ml->type) {
    case 0: /* Event */
	if (duration == 0) { /* Check Only */
	    if (ml->state == 1) ret = WAIT_FAILED;
	    else                   ret = WAIT_OBJECT_0;
	}
	if (duration == -1) { /* INFINITE */
	    if (ml->state == 0)
		pthread_cond_wait(ml->pc,ml->pm);
	    if (ml->reset)
		ml->state = 0;
	    ret = WAIT_OBJECT_0;
	}
	if (duration > 0) {  /* Timed Wait */
	    struct timespec abstime;
	    struct timeval now;
	    gettimeofday(&now, 0);
	    abstime.tv_sec = now.tv_sec + (now.tv_usec+duration)/1000000;
	    abstime.tv_nsec = ((now.tv_usec+duration)%1000000)*1000;
	    if (ml->state == 0)
		ret=pthread_cond_timedwait(ml->pc,ml->pm,&abstime);
	    if (ret == ETIMEDOUT) ret = WAIT_TIMEOUT;
	    else                  ret = WAIT_OBJECT_0;
	    if (ml->reset)
		ml->state = 0;
	}
	break;
    case 1:  /* Semaphore */
	if (duration == 0) {
	    if(ml->semaphore==0) ret = WAIT_FAILED;
	    else {
		ml->semaphore++;
		ret = WAIT_OBJECT_0;
	    }
	}
	if (duration == -1) {
	    if (ml->semaphore==0)
		pthread_cond_wait(ml->pc,ml->pm);
	    ml->semaphore--;
	}
	break;
    }
    pthread_mutex_unlock(ml->pm);

    dbgprintf("WaitForSingleObject(0x%x, %d): 0x%x => 0x%x \n",object,duration,ml,ret);
    return (void *)ret;
}

static int pf_set = 0;
static BYTE PF[64] = {0,};

static void DumpSystemInfo(const SYSTEM_INFO* si)
{
    dbgprintf("  Processor architecture %d\n", si->u.s.wProcessorArchitecture);
    dbgprintf("  Page size: %d\n", si->dwPageSize);
    dbgprintf("  Minimum app address: %d\n", si->lpMinimumApplicationAddress);
    dbgprintf("  Maximum app address: %d\n", si->lpMaximumApplicationAddress);
    dbgprintf("  Active processor mask: 0x%x\n", si->dwActiveProcessorMask);
    dbgprintf("  Number of processors: %d\n", si->dwNumberOfProcessors);
    dbgprintf("  Processor type: 0x%x\n", si->dwProcessorType);
    dbgprintf("  Allocation granularity: 0x%x\n", si->dwAllocationGranularity);
    dbgprintf("  Processor level: 0x%x\n", si->wProcessorLevel);
    dbgprintf("  Processor revision: 0x%x\n", si->wProcessorRevision);
}

static void WINAPI expGetSystemInfo(SYSTEM_INFO* si)
{
    /* FIXME: better values for the two entries below... */
    static int cache = 0;
    static SYSTEM_INFO cachedsi;
    unsigned int regs[4];
    dbgprintf("GetSystemInfo(%p) =>\n", si);

    if (cache) {
	memcpy(si,&cachedsi,sizeof(*si));
	DumpSystemInfo(si);
	return;
    }
    memset(PF,0,sizeof(PF));
    pf_set = 1;

    cachedsi.u.s.wProcessorArchitecture     = PROCESSOR_ARCHITECTURE_INTEL;
    cachedsi.dwPageSize 			= getpagesize();

    /* FIXME: better values for the two entries below... */
    cachedsi.lpMinimumApplicationAddress	= (void *)0x00000000;
    cachedsi.lpMaximumApplicationAddress	= (void *)0x7FFFFFFF;
    cachedsi.dwActiveProcessorMask		= 1;
    cachedsi.dwNumberOfProcessors		= 1;
    cachedsi.dwProcessorType		= PROCESSOR_INTEL_386;
    cachedsi.dwAllocationGranularity	= 0x10000;
    cachedsi.wProcessorLevel		= 5; /* pentium */
    cachedsi.wProcessorRevision		= 0x0101;

#ifdef MPLAYER
    /* mplayer's way to detect PF's */
    {
#include "../cpudetect.h"
	extern CpuCaps gCpuCaps;

	if (gCpuCaps.hasMMX)
	    PF[PF_MMX_INSTRUCTIONS_AVAILABLE] = TRUE;
	if (gCpuCaps.hasSSE)
	    PF[PF_XMMI_INSTRUCTIONS_AVAILABLE] = TRUE;
	if (gCpuCaps.has3DNow)
	    PF[PF_AMD3D_INSTRUCTIONS_AVAILABLE] = TRUE;

	    switch(gCpuCaps.cpuType)
	    {
		case CPUTYPE_I686:
		case CPUTYPE_I586:
		    cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
		    cachedsi.wProcessorLevel = 5;
		    break;
		case CPUTYPE_I486:
		    cachedsi.dwProcessorType = PROCESSOR_INTEL_486;
		    cachedsi.wProcessorLevel = 4;
		    break;
		case CPUTYPE_I386:
		default:
		    cachedsi.dwProcessorType = PROCESSOR_INTEL_386;
		    cachedsi.wProcessorLevel = 3;
		    break;
	    }
	    cachedsi.wProcessorRevision = gCpuCaps.cpuStepping;
    	    cachedsi.dwNumberOfProcessors = 1;	/* hardcoded */

    }
#endif

/* disable cpuid based detection (mplayer's cpudetect.c does this - see above) */
#ifndef MPLAYER
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__svr4__)
    do_cpuid(1, regs);
    switch ((regs[0] >> 8) & 0xf) {			// cpu family
    case 3: cachedsi.dwProcessorType = PROCESSOR_INTEL_386;
    cachedsi.wProcessorLevel= 3;
    break;
    case 4: cachedsi.dwProcessorType = PROCESSOR_INTEL_486;
    cachedsi.wProcessorLevel= 4;
    break;
    case 5: cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
    cachedsi.wProcessorLevel= 5;
    break;
    case 6: cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
    cachedsi.wProcessorLevel= 5;
    break;
    default:cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
    cachedsi.wProcessorLevel= 5;
    break;
    }
    cachedsi.wProcessorRevision = regs[0] & 0xf;	// stepping
    if (regs[3] & (1 <<  8))
	PF[PF_COMPARE_EXCHANGE_DOUBLE] = TRUE;
    if (regs[3] & (1 << 23))
	PF[PF_MMX_INSTRUCTIONS_AVAILABLE] = TRUE;
    if (regs[3] & (1 << 25))
	PF[PF_XMMI_INSTRUCTIONS_AVAILABLE] = TRUE;
    if (regs[3] & (1 << 31))
	PF[PF_AMD3D_INSTRUCTIONS_AVAILABLE] = TRUE;
    cachedsi.dwNumberOfProcessors=1;
#endif
#endif /* MPLAYER */

/* MPlayer: linux detection enabled (based on proc/cpuinfo) for checking
   fdiv_bug and fpu emulation flags -- alex/MPlayer */
#ifdef __linux__
    {
	char buf[20];
	char line[200];
	FILE *f = fopen ("/proc/cpuinfo", "r");

	if (!f)
	    return;
	while (fgets(line,200,f)!=NULL) {
	    char	*s,*value;

	    /* NOTE: the ':' is the only character we can rely on */
	    if (!(value = strchr(line,':')))
		continue;
	    /* terminate the valuename */
	    *value++ = '\0';
	    /* skip any leading spaces */
	    while (*value==' ') value++;
	    if ((s=strchr(value,'\n')))
		*s='\0';

	    /* 2.1 method */
	    if (!lstrncmpiA(line, "cpu family",strlen("cpu family"))) {
		if (isdigit (value[0])) {
		    switch (value[0] - '0') {
		    case 3: cachedsi.dwProcessorType = PROCESSOR_INTEL_386;
		    cachedsi.wProcessorLevel= 3;
		    break;
		    case 4: cachedsi.dwProcessorType = PROCESSOR_INTEL_486;
		    cachedsi.wProcessorLevel= 4;
		    break;
		    case 5: cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
		    cachedsi.wProcessorLevel= 5;
		    break;
		    case 6: cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
		    cachedsi.wProcessorLevel= 5;
		    break;
		    default:cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
		    cachedsi.wProcessorLevel= 5;
		    break;
		    }
		}
		/* set the CPU type of the current processor */
		sprintf(buf,"CPU %ld",cachedsi.dwProcessorType);
		continue;
	    }
	    /* old 2.0 method */
	    if (!lstrncmpiA(line, "cpu",strlen("cpu"))) {
		if (	isdigit (value[0]) && value[1] == '8' &&
			value[2] == '6' && value[3] == 0
		   ) {
		    switch (value[0] - '0') {
		    case 3: cachedsi.dwProcessorType = PROCESSOR_INTEL_386;
		    cachedsi.wProcessorLevel= 3;
		    break;
		    case 4: cachedsi.dwProcessorType = PROCESSOR_INTEL_486;
		    cachedsi.wProcessorLevel= 4;
		    break;
		    case 5: cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
		    cachedsi.wProcessorLevel= 5;
		    break;
		    case 6: cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
		    cachedsi.wProcessorLevel= 5;
		    break;
		    default:cachedsi.dwProcessorType = PROCESSOR_INTEL_PENTIUM;
		    cachedsi.wProcessorLevel= 5;
		    break;
		    }
		}
		/* set the CPU type of the current processor */
		sprintf(buf,"CPU %ld",cachedsi.dwProcessorType);
		continue;
	    }
	    if (!lstrncmpiA(line,"fdiv_bug",strlen("fdiv_bug"))) {
		if (!lstrncmpiA(value,"yes",3))
		    PF[PF_FLOATING_POINT_PRECISION_ERRATA] = TRUE;

		continue;
	    }
	    if (!lstrncmpiA(line,"fpu",strlen("fpu"))) {
		if (!lstrncmpiA(value,"no",2))
		    PF[PF_FLOATING_POINT_EMULATED] = TRUE;

		continue;
	    }
	    if (!lstrncmpiA(line,"processor",strlen("processor"))) {
		/* processor number counts up...*/
		unsigned int x;

		if (sscanf(value,"%d",&x))
		    if (x+1>cachedsi.dwNumberOfProcessors)
			cachedsi.dwNumberOfProcessors=x+1;

		/* Create a new processor subkey on a multiprocessor
		 * system
		 */
		sprintf(buf,"%d",x);
	    }
	    if (!lstrncmpiA(line,"stepping",strlen("stepping"))) {
		int	x;

		if (sscanf(value,"%d",&x))
		    cachedsi.wProcessorRevision = x;
	    }
	    if
		( (!lstrncmpiA(line,"flags",strlen("flags")))
		  || (!lstrncmpiA(line,"features",strlen("features"))) )
	    {
		if (strstr(value,"cx8"))
		    PF[PF_COMPARE_EXCHANGE_DOUBLE] = TRUE;
		if (strstr(value,"mmx"))
		    PF[PF_MMX_INSTRUCTIONS_AVAILABLE] = TRUE;
		if (strstr(value,"tsc"))
		    PF[PF_RDTSC_INSTRUCTION_AVAILABLE] = TRUE;
		if (strstr(value,"xmm"))
		    PF[PF_XMMI_INSTRUCTIONS_AVAILABLE] = TRUE;
		if (strstr(value,"3dnow"))
		    PF[PF_AMD3D_INSTRUCTIONS_AVAILABLE] = TRUE;
	    }
	}
	fclose (f);
	/*
	 *	ad hoc fix for smp machines.
	 *	some problems on WaitForSingleObject,CreateEvent,SetEvent
	 *			CreateThread ...etc..
	 *
	 */
	cachedsi.dwNumberOfProcessors=1;
    }
#endif /* __linux__ */
    cache = 1;
    memcpy(si,&cachedsi,sizeof(*si));
    DumpSystemInfo(si);
}

// avoid undefined expGetSystemInfo
static WIN_BOOL WINAPI expIsProcessorFeaturePresent(DWORD v)
{
    WIN_BOOL result = 0;
    if (!pf_set)
    {
	SYSTEM_INFO si;
	expGetSystemInfo(&si);
    }
    if(v<64) result=PF[v];
    dbgprintf("IsProcessorFeaturePresent(0x%x) => 0x%x\n", v, result);
    return result;
}


static long WINAPI expGetVersion()
{
    dbgprintf("GetVersion() => 0xC0000004\n");
    return 0xC0000004;//Windows 95
}

static HANDLE WINAPI expHeapCreate(long flags, long init_size, long max_size)
{
    //    printf("HeapCreate:");
    HANDLE result;
    if(init_size==0)
	result=(HANDLE)my_mreq(0x110000, 0);
    else
	result=(HANDLE)my_mreq((init_size + 0xfff) & 0x7ffff000 , 0);
    dbgprintf("HeapCreate(flags 0x%x, initial size %d, maximum size %d) => 0x%x\n", flags, init_size, max_size, result);
    return result;
}

// this is another dirty hack
// VP31 is releasing one allocated Heap chunk twice
// we will silently ignore this second call...
static void* heapfreehack = 0;
static int heapfreehackshown = 0;
//extern void trapbug(void);
static void* WINAPI expHeapAlloc(HANDLE heap, int flags, int size)
{
    void* z;
    /**
     Morgan's m3jpeg32.dll v. 2.0 encoder expects that request for
     HeapAlloc returns area larger than size argument :-/

     actually according to M$ Doc  HeapCreate size should be rounded
     to page boundaries thus we should simulate this
     **/
    //if (size == 22276) trapbug();
    z=my_mreq((size + 0xfff) & 0x7ffff000, (flags & HEAP_ZERO_MEMORY));
    if(z==0)
	printf("HeapAlloc failure\n");
    dbgprintf("HeapAlloc(heap 0x%x, flags 0x%x, size %d) => 0x%x\n", heap, flags, size, z);
    heapfreehack = 0; // reset
    return z;
}
static long WINAPI expHeapDestroy(void* heap)
{
    dbgprintf("HeapDestroy(heap 0x%x) => 1\n", heap);
    my_release(heap);
    return 1;
}

static long WINAPI expHeapFree(HANDLE heap, DWORD dwFlags, LPVOID lpMem)
{
    dbgprintf("HeapFree(0x%x, 0x%x, pointer 0x%x) => 1\n", heap, dwFlags, lpMem);
    if (heapfreehack != lpMem && lpMem != (void*)0xffffffff
	&& lpMem != (void*)0xbdbdbdbd)
	// 0xbdbdbdbd is for i263_drv.drv && libefence
	// it seems to be reading from relased memory
        // EF_PROTECT_FREE doens't show any probleme
	my_release(lpMem);
    else
    {
	if (!heapfreehackshown++)
	    printf("Info: HeapFree deallocating same memory twice! (%p)\n", lpMem);
    }
    heapfreehack = lpMem;
    return 1;
}
static long WINAPI expHeapSize(int heap, int flags, void* pointer)
{
    long result=my_size(pointer);
    dbgprintf("HeapSize(heap 0x%x, flags 0x%x, pointer 0x%x) => %d\n", heap, flags, pointer, result);
    return result;
}
static void* WINAPI expHeapReAlloc(HANDLE heap,int flags,void *lpMem,int size)
{
    long orgsize = my_size(lpMem);
    dbgprintf("HeapReAlloc() Size %ld org %d\n",orgsize,size);
    return my_realloc(lpMem, size);
}
static long WINAPI expGetProcessHeap(void)
{
    dbgprintf("GetProcessHeap() => 1\n");
    return 1;
}
static void* WINAPI expVirtualAlloc(void* v1, long v2, long v3, long v4)
{
    void* z = VirtualAlloc(v1, v2, v3, v4);
    if(z==0)
	printf("VirtualAlloc failure\n");
    dbgprintf("VirtualAlloc(0x%x, %d, %d, %d) => 0x%x \n",v1,v2,v3,v4, z);
    return z;
}
static int WINAPI expVirtualFree(void* v1, int v2, int v3)
{
    int result = VirtualFree(v1,v2,v3);
    dbgprintf("VirtualFree(0x%x, %d, %d) => %d\n",v1,v2,v3, result);
    return result;
}

/* we're building a table of critical sections. cs_win pointer uses the DLL
 cs_unix is the real structure, we're using cs_win only to identifying cs_unix */
struct critsecs_list_t
{
    CRITICAL_SECTION *cs_win;
    struct CRITSECT *cs_unix;
};

/* 'NEWTYPE' is working with VIVO and 3ivX dll (no more segfaults) -- alex */
#undef CRITSECS_NEWTYPE
//#define CRITSECS_NEWTYPE 1

#ifdef CRITSECS_NEWTYPE
#define CRITSECS_LIST_MAX 20
static struct critsecs_list_t critsecs_list[CRITSECS_LIST_MAX];

static int critsecs_get_pos(CRITICAL_SECTION *cs_win)
{
    int i;

    for (i=0; i < CRITSECS_LIST_MAX; i++)
	if (critsecs_list[i].cs_win == cs_win)
	    return(i);
    return(-1);
}

static int critsecs_get_unused(void)
{
    int i;

    for (i=0; i < CRITSECS_LIST_MAX; i++)
	if (critsecs_list[i].cs_win == NULL)
	    return(i);
    return(-1);
}

#if 0
#define critsecs_get_unix(cs_win) (critsecs_list[critsecs_get_pos(cs_win)].cs_win)
#else
struct CRITSECT *critsecs_get_unix(CRITICAL_SECTION *cs_win)
{
    int i;

    for (i=0; i < CRITSECS_LIST_MAX; i++)
	if (critsecs_list[i].cs_win == cs_win && critsecs_list[i].cs_unix)
	    return(critsecs_list[i].cs_unix);
    return(NULL);
}
#endif
#endif

static void WINAPI expInitializeCriticalSection(CRITICAL_SECTION* c)
{
    dbgprintf("InitializeCriticalSection(0x%x)\n", c);
    /*    if(sizeof(pthread_mutex_t)>sizeof(CRITICAL_SECTION))
     {
     printf(" ERROR:::: sizeof(pthread_mutex_t) is %d, expected <=%d!\n",
     sizeof(pthread_mutex_t), sizeof(CRITICAL_SECTION));
     return;
     }*/
    /*    pthread_mutex_init((pthread_mutex_t*)c, NULL);   */
#ifdef CRITSECS_NEWTYPE
    {
	struct CRITSECT *cs;
	int i = critsecs_get_unused();

	if (i < 0)
	{
	    printf("InitializeCriticalSection(%p) - no more space in list\n", c);
	    return;
	}
	dbgprintf("got unused space at %d\n", i);
	cs = malloc(sizeof(struct CRITSECT));
	if (!cs)
	{
	    printf("InitializeCriticalSection(%p) - out of memory\n", c);
	    return;
	}
	pthread_mutex_init(&cs->mutex, NULL);
	cs->locked = 0;
	critsecs_list[i].cs_win = c;
	critsecs_list[i].cs_unix = cs;
	dbgprintf("InitializeCriticalSection -> itemno=%d, cs_win=%p, cs_unix=%p\n",
		  i, c, cs);
    }
#else
    {
	struct CRITSECT* cs = mreq_private(sizeof(struct CRITSECT) + sizeof(CRITICAL_SECTION),
					   0, AREATYPE_CRITSECT);
	pthread_mutex_init(&cs->mutex, NULL);
	cs->locked=0;
        cs->deadbeef = 0xdeadbeef;
	*(void**)c = cs + 1;
    }
#endif
    return;
}

static void WINAPI expEnterCriticalSection(CRITICAL_SECTION* c)
{
#ifdef CRITSECS_NEWTYPE
    struct CRITSECT* cs = critsecs_get_unix(c);
#else
    struct CRITSECT* cs = (*(struct CRITSECT**)c) - 1;

#endif
    dbgprintf("EnterCriticalSection(0x%x) %p maso:0x%x\n",c, cs, cs->deadbeef);
    if (!cs)
    {
	dbgprintf("entered uninitialized critisec!\n");
	expInitializeCriticalSection(c);
#ifdef CRITSECS_NEWTYPE
	cs=critsecs_get_unix(c);
#else
	cs = (*(struct CRITSECT**)c) - 1;
#endif
	printf("Win32 Warning: Accessed uninitialized Critical Section (%p)!\n", c);
    }
    if(cs->locked)
	if(cs->id==pthread_self())
	    return;
    pthread_mutex_lock(&(cs->mutex));
    cs->locked=1;
    cs->id=pthread_self();
    return;
}
static void WINAPI expLeaveCriticalSection(CRITICAL_SECTION* c)
{
#ifdef CRITSECS_NEWTYPE
    struct CRITSECT* cs = critsecs_get_unix(c);
#else
    struct CRITSECT* cs = (*(struct CRITSECT**)c) - 1;
#endif
    //    struct CRITSECT* cs=(struct CRITSECT*)c;
    dbgprintf("LeaveCriticalSection(0x%x) 0x%x\n",c, cs->deadbeef);
    if (!cs)
    {
	printf("Win32 Warning: Leaving uninitialized Critical Section %p!!\n", c);
	return;
    }
    cs->locked=0;
    pthread_mutex_unlock(&(cs->mutex));
    return;
}
static void WINAPI expDeleteCriticalSection(CRITICAL_SECTION *c)
{
#ifdef CRITSECS_NEWTYPE
    struct CRITSECT* cs = critsecs_get_unix(c);
#else
    struct CRITSECT* cs= (*(struct CRITSECT**)c) - 1;
#endif
    //    struct CRITSECT* cs=(struct CRITSECT*)c;
    dbgprintf("DeleteCriticalSection(0x%x)\n",c);

#ifndef GARBAGE
    pthread_mutex_destroy(&(cs->mutex));
    // released by GarbageCollector in my_relase otherwise
#endif
    my_release(cs);
#ifdef CRITSECS_NEWTYPE
    {
	int i = critsecs_get_pos(c);

	if (i < 0)
	{
	    printf("DeleteCriticalSection(%p) error (critsec not found)\n", c);
	    return;
	}

	critsecs_list[i].cs_win = NULL;
	expfree(critsecs_list[i].cs_unix);
	critsecs_list[i].cs_unix = NULL;
	dbgprintf("DeleteCriticalSection -> itemno=%d\n", i);
    }
#endif
    return;
}
static int WINAPI expGetCurrentThreadId()
{
    dbgprintf("GetCurrentThreadId() => %d\n", getpid());
    return getpid();
}
static int WINAPI expGetCurrentProcess()
{
    dbgprintf("GetCurrentProcess() => %d\n", getpid());
    return getpid();
}

#if 0
// this version is required for Quicktime codecs (.qtx/.qts) to work.
// (they assume some pointers at FS: segment)

extern void* fs_seg;

//static int tls_count;
static int tls_use_map[64];
static int WINAPI expTlsAlloc()
{
    int i;
    for(i=0; i<64; i++)
	if(tls_use_map[i]==0)
	{
	    tls_use_map[i]=1;
	    dbgprintf("TlsAlloc() => %d\n",i);
	    return i;
	}
    dbgprintf("TlsAlloc() => -1 (ERROR)\n");
    return -1;
}

static int WINAPI expTlsSetValue(DWORD index, void* value)
{
    dbgprintf("TlsSetValue(%d,%p)\n",index,value);
//    if((index<0) || (index>64))
    if((index>=64))
	return 0;
    *(void**)((char*)fs_seg+0x88+4*index) = value;
    return 1;
}

static void* WINAPI expTlsGetValue(DWORD index)
{
    dbgprintf("TlsGetValue(%d)\n",index);
//    if((index<0) || (index>64))
    if((index>=64)) return NULL;
    return *(void**)((char*)fs_seg+0x88+4*index);
}

static int WINAPI expTlsFree(int idx)
{
    int index = (int) idx;
    dbgprintf("TlsFree(%d)\n",index);
    if((index<0) || (index>64))
	return 0;
    tls_use_map[index]=0;
    return 1;
}

#else
struct tls_s {
    void* value;
    int used;
    struct tls_s* prev;
    struct tls_s* next;
};

static void* WINAPI expTlsAlloc()
{
    if (g_tls == NULL)
    {
	g_tls=my_mreq(sizeof(tls_t), 0);
	g_tls->next=g_tls->prev=NULL;
    }
    else
    {
	g_tls->next=my_mreq(sizeof(tls_t), 0);
	g_tls->next->prev=g_tls;
	g_tls->next->next=NULL;
	g_tls=g_tls->next;
    }
    dbgprintf("TlsAlloc() => 0x%x\n", g_tls);
    if (g_tls)
	g_tls->value=0; /* XXX For Divx.dll */
    return g_tls;
}

static int WINAPI expTlsSetValue(void* idx, void* value)
{
    tls_t* index = (tls_t*) idx;
    int result;
    if(index==0)
	result=0;
    else
    {
	index->value=value;
	result=1;
    }
    dbgprintf("TlsSetValue(index 0x%x, value 0x%x) => %d \n", index, value, result );
    return result;
}
static void* WINAPI expTlsGetValue(void* idx)
{
    tls_t* index = (tls_t*) idx;
    void* result;
    if(index==0)
	result=0;
    else
	result=index->value;
    dbgprintf("TlsGetValue(index 0x%x) => 0x%x\n", index, result);
    return result;
}
static int WINAPI expTlsFree(void* idx)
{
    tls_t* index = (tls_t*) idx;
    int result;
    if(index==0)
	result=0;
    else
    {
	if(index->next)
	    index->next->prev=index->prev;
	if(index->prev)
	    index->prev->next=index->next;
	if (g_tls == index)
            g_tls = index->prev;
	my_release((void*)index);
	result=1;
    }
    dbgprintf("TlsFree(index 0x%x) => %d\n", index, result);
    return result;
}
#endif

static void* WINAPI expLocalAlloc(int flags, int size)
{
    void* z = my_mreq(size, (flags & GMEM_ZEROINIT));
    if (z == 0)
	printf("LocalAlloc() failed\n");
    dbgprintf("LocalAlloc(%d, flags 0x%x) => 0x%x\n", size, flags, z);
    return z;
}

static void* WINAPI expLocalReAlloc(int handle,int size, int flags)
{
    void *newpointer;
    int oldsize;

    newpointer=NULL;
    if (flags & LMEM_MODIFY) {
	dbgprintf("LocalReAlloc MODIFY\n");
	return (void *)handle;
    }
    oldsize = my_size((void *)handle);
    newpointer = my_realloc((void *)handle,size);
    dbgprintf("LocalReAlloc(%x %d(old %d), flags 0x%x) => 0x%x\n", handle,size,oldsize, flags,newpointer);

    return newpointer;
}

static void* WINAPI expLocalLock(void* z)
{
    dbgprintf("LocalLock(0x%x) => 0x%x\n", z, z);
    return z;
}

static void* WINAPI expGlobalAlloc(int flags, int size)
{
    void* z;
    dbgprintf("GlobalAlloc(%d, flags 0x%X)\n", size, flags);

    z=my_mreq(size, (flags & GMEM_ZEROINIT));
    //z=calloc(size, 1);
    //z=malloc(size);
    if(z==0)
	printf("GlobalAlloc() failed\n");
    dbgprintf("GlobalAlloc(%d, flags 0x%x) => 0x%x\n", size, flags, z);
    return z;
}
static void* WINAPI expGlobalLock(void* z)
{
    dbgprintf("GlobalLock(0x%x) => 0x%x\n", z, z);
    return z;
}
// pvmjpg20 - but doesn't work anyway
static int WINAPI expGlobalSize(void* amem)
{
    int size = 100000;
#ifdef GARBAGE
    alloc_header* header = last_alloc;
    alloc_header* mem = (alloc_header*) amem - 1;
    if (amem == 0)
	return 0;
    pthread_mutex_lock(&memmut);
    while (header)
    {
	if (header->deadbeef != 0xdeadbeef)
	{
	    printf("FATAL found corrupted memory! %p  0x%lx  (%d)\n", header, header->deadbeef, alccnt);
	    break;
	}

	if (header == mem)
	{
	    size = header->size;
	    break;
	}

	header = header->prev;
    }
    pthread_mutex_unlock(&memmut);
#endif

    dbgprintf("GlobalSize(0x%x)\n", amem);
    return size;
}
static int WINAPI expLoadStringA(long instance, long  id, void* buf, long size)
{
    int result=LoadStringA(instance, id, buf, size);
    //    if(buf)
    dbgprintf("LoadStringA(instance 0x%x, id 0x%x, buffer 0x%x, size %d) => %d ( %s )\n",
	      instance, id, buf, size, result, buf);
    //    else
    //    dbgprintf("LoadStringA(instance 0x%x, id 0x%x, buffer 0x%x, size %d) => %d\n",
    //	instance, id, buf, size, result);
    return result;
}

static long WINAPI expMultiByteToWideChar(long v1, long v2, char* s1, long siz1, short* s2, int siz2)
{
#warning FIXME
    int i;
    int result;
    if(s2==0)
	result=1;
    else
    {
	if(siz1>siz2/2)siz1=siz2/2;
	for(i=1; i<=siz1; i++)
	{
	    *s2=*s1;
	    if(!*s1)break;
	    s2++;
	    s1++;
	}
	result=i;
    }
    if(s1)
	dbgprintf("MultiByteToWideChar(codepage %d, flags 0x%x, string 0x%x='%s',"
		  "size %d, dest buffer 0x%x, dest size %d) => %d\n",
		  v1, v2, s1, s1, siz1, s2, siz2, result);
    else
	dbgprintf("MultiByteToWideChar(codepage %d, flags 0x%x, string NULL,"
		  "size %d, dest buffer 0x%x, dest size %d) =>\n",
		  v1, v2, siz1, s2, siz2, result);
    return result;
}
static void wch_print(const short* str)
{
    dbgprintf("  src: ");
    while(*str)dbgprintf("%c", *str++);
    dbgprintf("\n");
}
static long WINAPI expWideCharToMultiByte(long v1, long v2, short* s1, long siz1,
					  char* s2, int siz2, char* c3, int* siz3)
{
    int result;
    dbgprintf("WideCharToMultiByte(codepage %d, flags 0x%x, src 0x%x, src size %d, "
	      "dest 0x%x, dest size %d, defch 0x%x, used_defch 0x%x)", v1, v2, s1, siz1, s2, siz2, c3, siz3);
    result=WideCharToMultiByte(v1, v2, s1, siz1, s2, siz2, c3, siz3);
    dbgprintf("=> %d\n", result);
    //if(s1)wch_print(s1);
    if(s2)dbgprintf("  dest: %s\n", s2);
    return result;
}
static long WINAPI expGetVersionExA(OSVERSIONINFOA* c)
{
    dbgprintf("GetVersionExA(0x%x) => 1\n");
    c->dwOSVersionInfoSize=sizeof(*c);
    c->dwMajorVersion=4;
    c->dwMinorVersion=0;
    c->dwBuildNumber=0x4000457;
#if 1
    // leave it here for testing win9x-only codecs
    c->dwPlatformId=VER_PLATFORM_WIN32_WINDOWS;
    strcpy(c->szCSDVersion, " B");
#else
    c->dwPlatformId=VER_PLATFORM_WIN32_NT; // let's not make DLL assume that it can read CR* registers
    strcpy(c->szCSDVersion, "Service Pack 3");
#endif
    dbgprintf("  Major version: 4\n  Minor version: 0\n  Build number: 0x4000457\n"
	      "  Platform Id: VER_PLATFORM_WIN32_NT\n Version string: 'Service Pack 3'\n");
    return 1;
}
static HANDLE WINAPI expCreateSemaphoreA(char* v1, long init_count,
					 long max_count, char* name)
{
    pthread_mutex_t *pm;
    pthread_cond_t  *pc;
    mutex_list* pp;
    /*
     printf("CreateSemaphoreA(%p = %s)\n", name, (name ? name : "<null>"));
     pp=mlist;
     while(pp)
     {
     printf("%p => ", pp);
     pp=pp->prev;
     }
     printf("0\n");
     */
    if(mlist!=NULL)
    {
	mutex_list* pp=mlist;
	if(name!=NULL)
	    do
	{
	    if((strcmp(pp->name, name)==0) && (pp->type==1))
	    {
		dbgprintf("CreateSemaphoreA(0x%x, init_count %d, max_count %d, name 0x%x='%s') => 0x%x\n",
			  v1, init_count, max_count, name, name, mlist);
		return (HANDLE)mlist;
	    }
	}while((pp=pp->prev) != NULL);
    }
    pm=mreq_private(sizeof(pthread_mutex_t), 0, AREATYPE_MUTEX);
    pthread_mutex_init(pm, NULL);
    pc=mreq_private(sizeof(pthread_cond_t), 0, AREATYPE_COND);
    pthread_cond_init(pc, NULL);
    if(mlist==NULL)
    {
	mlist=mreq_private(sizeof(mutex_list), 00, AREATYPE_EVENT);
	mlist->next=mlist->prev=NULL;
    }
    else
    {
	mlist->next=mreq_private(sizeof(mutex_list), 00, AREATYPE_EVENT);
	mlist->next->prev=mlist;
	mlist->next->next=NULL;
	mlist=mlist->next;
	//	printf("new semaphore %p\n", mlist);
    }
    mlist->type=1; /* Type Semaphore */
    mlist->pm=pm;
    mlist->pc=pc;
    mlist->state=0;
    mlist->reset=0;
    mlist->semaphore=init_count;
    if(name!=NULL)
	strncpy(mlist->name, name, 64);
    else
	mlist->name[0]=0;
    if(pm==NULL)
	dbgprintf("ERROR::: CreateSemaphoreA failure\n");
    if(name)
	dbgprintf("CreateSemaphoreA(0x%x, init_count %d, max_count %d, name 0x%x='%s') => 0x%x\n",
		  v1, init_count, max_count, name, name, mlist);
    else
	dbgprintf("CreateSemaphoreA(0x%x, init_count %d, max_count %d, name 0) => 0x%x\n",
		  v1, init_count, max_count, mlist);
    return (HANDLE)mlist;
}

static long WINAPI expReleaseSemaphore(long hsem, long increment, long* prev_count)
{
    // The state of a semaphore object is signaled when its count
    // is greater than zero and nonsignaled when its count is equal to zero
    // Each time a waiting thread is released because of the semaphore's signaled
    // state, the count of the semaphore is decreased by one.
    mutex_list *ml = (mutex_list *)hsem;

    pthread_mutex_lock(ml->pm);
    if (prev_count != 0) *prev_count = ml->semaphore;
    if (ml->semaphore == 0) pthread_cond_signal(ml->pc);
    ml->semaphore += increment;
    pthread_mutex_unlock(ml->pm);
    dbgprintf("ReleaseSemaphore(semaphore 0x%x, increment %d, prev_count 0x%x) => 1\n",
	      hsem, increment, prev_count);
    return 1;
}


static long WINAPI expRegOpenKeyExA(long key, const char* subkey, long reserved, long access, int* newkey)
{
    long result=RegOpenKeyExA(key, subkey, reserved, access, newkey);
    dbgprintf("RegOpenKeyExA(key 0x%x, subkey %s, reserved %d, access 0x%x, pnewkey 0x%x) => %d\n",
	      key, subkey, reserved, access, newkey, result);
    if(newkey)dbgprintf("  New key: 0x%x\n", *newkey);
    return result;
}
static long WINAPI expRegCloseKey(long key)
{
    long result=RegCloseKey(key);
    dbgprintf("RegCloseKey(0x%x) => %d\n", key, result);
    return result;
}
static long WINAPI expRegQueryValueExA(long key, const char* value, int* reserved, int* type, int* data, int* count)
{
    long result=RegQueryValueExA(key, value, reserved, type, data, count);
    dbgprintf("RegQueryValueExA(key 0x%x, value %s, reserved 0x%x, data 0x%x, count 0x%x)"
	      " => 0x%x\n", key, value, reserved, data, count, result);
    if(data && count)dbgprintf("  read %d bytes: '%s'\n", *count, data);
    return result;
}
static long WINAPI expRegCreateKeyExA(long key, const char* name, long reserved,
				      void* classs, long options, long security,
				      void* sec_attr, int* newkey, int* status)
{
    long result=RegCreateKeyExA(key, name, reserved, classs, options, security, sec_attr, newkey, status);
    dbgprintf("RegCreateKeyExA(key 0x%x, name 0x%x='%s', reserved=0x%x,"
	      " 0x%x, 0x%x, 0x%x, newkey=0x%x, status=0x%x) => %d\n",
	      key, name, name, reserved, classs, options, security, sec_attr, newkey, status, result);
    if(!result && newkey) dbgprintf("  New key: 0x%x\n", *newkey);
    if(!result && status) dbgprintf("  New key status: 0x%x\n", *status);
    return result;
}
static long WINAPI expRegSetValueExA(long key, const char* name, long v1, long v2, void* data, long size)
{
    long result=RegSetValueExA(key, name, v1, v2, data, size);
    dbgprintf("RegSetValueExA(key 0x%x, name '%s', 0x%x, 0x%x, data 0x%x -> 0x%x '%s', size=%d) => %d",
	      key, name, v1, v2, data, *(int*)data, data, size, result);
    return result;
}

static long WINAPI expRegOpenKeyA (long hKey, LPCSTR lpSubKey, int* phkResult)
{
    long result=RegOpenKeyExA(hKey, lpSubKey, 0, 0, phkResult);
    dbgprintf("RegOpenKeyExA(key 0x%x, subkey '%s', 0x%x) => %d\n",
	      hKey, lpSubKey, phkResult, result);
    if(!result && phkResult) dbgprintf("  New key: 0x%x\n", *phkResult);
    return result;
}

static DWORD WINAPI expRegEnumValueA(HKEY hkey, DWORD index, LPSTR value, LPDWORD val_count,
				     LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD count)
{
    return RegEnumValueA(hkey, index, value, val_count,
			 reserved, type, data, count);
}

static DWORD WINAPI expRegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcbName,
				     LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcbClass,
				     LPFILETIME lpftLastWriteTime)
{
    return RegEnumKeyExA(hKey, dwIndex, lpName, lpcbName, lpReserved, lpClass,
			 lpcbClass, lpftLastWriteTime);
}

static long WINAPI expQueryPerformanceCounter(long long* z)
{
    longcount(z);
    dbgprintf("QueryPerformanceCounter(0x%x) => 1 ( %Ld )\n", z, *z);
    return 1;
}

/*
 * return CPU clock (in kHz), using linux's /proc filesystem (/proc/cpuinfo)
 */
static double linux_cpuinfo_freq()
{
    double freq=-1;
    FILE *f;
    char line[200];
    char *s,*value;

    f = fopen ("/proc/cpuinfo", "r");
    if (f != NULL) {
	while (fgets(line,sizeof(line),f)!=NULL) {
	    /* NOTE: the ':' is the only character we can rely on */
	    if (!(value = strchr(line,':')))
		continue;
	    /* terminate the valuename */
	    *value++ = '\0';
	    /* skip any leading spaces */
	    while (*value==' ') value++;
	    if ((s=strchr(value,'\n')))
		*s='\0';

	    if (!strncasecmp(line, "cpu MHz",strlen("cpu MHz"))
		&& sscanf(value, "%lf", &freq) == 1) {
		freq*=1000;
		break;
	    }
	}
	fclose(f);
    }
    return freq;
}


static double solaris_kstat_freq()
{
#if	defined(HAVE_LIBKSTAT) && defined(KSTAT_DATA_INT32)
    /*
     * try to extract the CPU speed from the solaris kernel's kstat data
     */
    kstat_ctl_t   *kc;
    kstat_t       *ksp;
    kstat_named_t *kdata;
    int            mhz = 0;

    kc = kstat_open();
    if (kc != NULL)
    {
	ksp = kstat_lookup(kc, "cpu_info", 0, "cpu_info0");

	/* kstat found and name/value pairs? */
	if (ksp != NULL && ksp->ks_type == KSTAT_TYPE_NAMED)
	{
	    /* read the kstat data from the kernel */
	    if (kstat_read(kc, ksp, NULL) != -1)
	    {
		/*
		 * lookup desired "clock_MHz" entry, check the expected
		 * data type
		 */
		kdata = (kstat_named_t *)kstat_data_lookup(ksp, "clock_MHz");
		if (kdata != NULL && kdata->data_type == KSTAT_DATA_INT32)
		    mhz = kdata->value.i32;
	    }
	}
	kstat_close(kc);
    }

    if (mhz > 0)
	return mhz * 1000.;
#endif	/* HAVE_LIBKSTAT */
    return -1;		// kstat stuff is not available, CPU freq is unknown
}

/*
 * Measure CPU freq using the pentium's time stamp counter register (TSC)
 */
static double tsc_freq()
{
    static double ofreq=0.0;
    int i;
    int x,y;
    i=time(NULL);
    if (ofreq != 0.0) return ofreq;
    while(i==time(NULL));
    x=localcount();
    i++;
    while(i==time(NULL));
    y=localcount();
    ofreq = (double)(y-x)/1000.;
    return ofreq;
}

static double CPU_Freq()
{
    double freq;

    if ((freq = linux_cpuinfo_freq()) > 0)
	return freq;

    if ((freq = solaris_kstat_freq()) > 0)
	return freq;

    return tsc_freq();
}

static long WINAPI expQueryPerformanceFrequency(long long* z)
{
    *z=(long long)CPU_Freq();
    dbgprintf("QueryPerformanceFrequency(0x%x) => 1 ( %Ld )\n", z, *z);
    return 1;
}
static long WINAPI exptimeGetTime()
{
    struct timeval t;
    long result;
    gettimeofday(&t, 0);
    result=1000*t.tv_sec+t.tv_usec/1000;
    dbgprintf("timeGetTime() => %d\n", result);
    return result;
}
static void* WINAPI expLocalHandle(void* v)
{
    dbgprintf("LocalHandle(0x%x) => 0x%x\n", v, v);
    return v;
}

static void* WINAPI expGlobalHandle(void* v)
{
    dbgprintf("GlobalHandle(0x%x) => 0x%x\n", v, v);
    return v;
}
static int WINAPI expGlobalUnlock(void* v)
{
    dbgprintf("GlobalUnlock(0x%x) => 1\n", v);
    return 1;
}
static void* WINAPI expGlobalFree(void* v)
{
    dbgprintf("GlobalFree(0x%x) => 0\n", v);
    my_release(v);
    //free(v);
    return 0;
}

static void* WINAPI expGlobalReAlloc(void* v, int size, int flags)
{
    void* result=my_realloc(v, size);
    //void* result=realloc(v, size);
    dbgprintf("GlobalReAlloc(0x%x, size %d, flags 0x%x) => 0x%x\n", v,size,flags,result);
    return result;
}

static int WINAPI expLocalUnlock(void* v)
{
    dbgprintf("LocalUnlock(0x%x) => 1\n", v);
    return 1;
}
//
static void* WINAPI expLocalFree(void* v)
{
    dbgprintf("LocalFree(0x%x) => 0\n", v);
    my_release(v);
    return 0;
}
static HRSRC WINAPI expFindResourceA(HMODULE module, char* name, char* type)
{
    HRSRC result;

    result=FindResourceA(module, name, type);
    dbgprintf("FindResourceA(module 0x%x, name 0x%x(%s), type 0x%x(%s)) => 0x%x\n",
	module, name, HIWORD(name) ? name : "UNICODE", type, HIWORD(type) ? type : "UNICODE", result);
    return result;
}

extern HRSRC WINAPI LoadResource(HMODULE, HRSRC);
static HGLOBAL WINAPI expLoadResource(HMODULE module, HRSRC res)
{
    HGLOBAL result=LoadResource(module, res);
    dbgprintf("LoadResource(module 0x%x, resource 0x%x) => 0x%x\n", module, res, result);
    return result;
}
static void* WINAPI expLockResource(long res)
{
    void* result=LockResource(res);
    dbgprintf("LockResource(0x%x) => 0x%x\n", res, result);
    return result;
}
static int WINAPI expFreeResource(long res)
{
    int result=FreeResource(res);
    dbgprintf("FreeResource(0x%x) => %d\n", res, result);
    return result;
}
//bool fun(HANDLE)
//!0 on success
static int WINAPI expCloseHandle(long v1)
{
    dbgprintf("CloseHandle(0x%x) => 1\n", v1);
    /* do not close stdin,stdout and stderr */
    if (v1 > 2)
	if (!close(v1))
	    return 0;
    return 1;
}

static const char* WINAPI expGetCommandLineA()
{
    dbgprintf("GetCommandLineA() => \"c:\\aviplay.exe\"\n");
    return "c:\\aviplay.exe";
}
static short envs[]={'p', 'a', 't', 'h', ' ', 'c', ':', '\\', 0, 0};
static LPWSTR WINAPI expGetEnvironmentStringsW()
{
    dbgprintf("GetEnvironmentStringsW() => 0\n", envs);
    return 0;
}
static void * WINAPI expRtlZeroMemory(void *p, size_t len)
{
    void* result=memset(p,0,len);
    dbgprintf("RtlZeroMemory(0x%x, len %d) => 0x%x\n",p,len,result);
    return result;
}
static void * WINAPI expRtlMoveMemory(void *dst, void *src, size_t len)
{
    void* result=memmove(dst,src,len);
    dbgprintf("RtlMoveMemory (dest 0x%x, src 0x%x, len %d) => 0x%x\n",dst,src,len,result);
    return result;
}

static void * WINAPI expRtlFillMemory(void *p, int ch, size_t len)
{
    void* result=memset(p,ch,len);
    dbgprintf("RtlFillMemory(0x%x, char 0x%x, len %d) => 0x%x\n",p,ch,len,result);
    return result;
}
static int WINAPI expFreeEnvironmentStringsW(short* strings)
{
    dbgprintf("FreeEnvironmentStringsW(0x%x) => 1\n", strings);
    return 1;
}
static int WINAPI expFreeEnvironmentStringsA(char* strings)
{
    dbgprintf("FreeEnvironmentStringsA(0x%x) => 1\n", strings);
    return 1;
}

static const char ch_envs[]=
"__MSVCRT_HEAP_SELECT=__GLOBAL_HEAP_SELECTED,1\r\n"
"PATH=C:\\;C:\\windows\\;C:\\windows\\system\r\n";
static LPCSTR WINAPI expGetEnvironmentStrings()
{
    dbgprintf("GetEnvironmentStrings() => 0x%x\n", ch_envs);
    return (LPCSTR)ch_envs;
    // dbgprintf("GetEnvironmentStrings() => 0\n");
    // return 0;
}

static int WINAPI expGetStartupInfoA(STARTUPINFOA *s)
{
    int i;
    dbgprintf("GetStartupInfoA(0x%x) => 1\n");
    memset(s, 0, sizeof(*s));
    s->cb=sizeof(*s);
    // s->lpReserved="Reserved";
    // s->lpDesktop="Desktop";
    // s->lpTitle="Title";
    // s->dwX=s->dwY=0;
    // s->dwXSize=s->dwYSize=200;
    s->dwFlags=s->wShowWindow=1;
    // s->hStdInput=s->hStdOutput=s->hStdError=0x1234;
    dbgprintf("  cb=%d\n", s->cb);
    dbgprintf("  lpReserved='%s'\n", s->lpReserved);
    dbgprintf("  lpDesktop='%s'\n", s->lpDesktop);
    dbgprintf("  lpTitle='%s'\n", s->lpTitle);
    dbgprintf("  dwX=%d dwY=%d dwXSize=%d dwYSize=%d\n",
	      s->dwX, s->dwY, s->dwXSize, s->dwYSize);
    dbgprintf("  dwXCountChars=%d dwYCountChars=%d dwFillAttribute=%d\n",
	      s->dwXCountChars, s->dwYCountChars, s->dwFillAttribute);
    dbgprintf("  dwFlags=0x%x wShowWindow=0x%x cbReserved2=0x%x\n",
	      s->dwFlags, s->wShowWindow, s->cbReserved2);
    dbgprintf("  lpReserved2=0x%x hStdInput=0x%x hStdOutput=0x%x hStdError=0x%x\n",
	      s->lpReserved2, s->hStdInput, s->hStdOutput, s->hStdError);
    return 1;
}

static int WINAPI expGetStdHandle(int z)
{
    dbgprintf("GetStdHandle(0x%x) => 0x%x\n", z+0x1234);
    return z+0x1234;
}
static int WINAPI expGetFileType(int handle)
{
    dbgprintf("GetFileType(0x%x) => 0x3 = pipe\n", handle);
    return 0x3;
}
static int WINAPI expSetHandleCount(int count)
{
    dbgprintf("SetHandleCount(0x%x) => 1\n", count);
    return 1;
}
static int WINAPI expGetACP()
{
    dbgprintf("GetACP() => 0\n");
    return 0;
}
extern WINE_MODREF *MODULE32_LookupHMODULE(HMODULE m);
static int WINAPI expGetModuleFileNameA(int module, char* s, int len)
{
    WINE_MODREF *mr;
    int result;
    //    printf("File name of module %X requested\n", module);
    if(s==0)
	result=0;
    else
	if(len<35)
	    result=0;
	else
	{
	    result=1;
	    strcpy(s, "c:\\windows\\system\\");
	    mr=MODULE32_LookupHMODULE(module);
	    if(mr==0)//oops
		strcat(s, "aviplay.dll");
	    else
		if(strrchr(mr->filename, '/')==NULL)
		    strcat(s, mr->filename);
		else
		    strcat(s, strrchr(mr->filename, '/')+1);
	}
    if(!s)
	dbgprintf("GetModuleFileNameA(0x%x, 0x%x, %d) => %d\n",
		  module, s, len, result);
    else
	dbgprintf("GetModuleFileNameA(0x%x, 0x%x, %d) => %d ( '%s' )\n",
		  module, s, len, result, s);
    return result;
}

static int WINAPI expSetUnhandledExceptionFilter(void* filter)
{
    dbgprintf("SetUnhandledExceptionFilter(0x%x) => 1\n", filter);
    return 1;//unsupported and probably won't ever be supported
}

static int WINAPI expLoadLibraryA(char* name)
{
    int result = 0;
    char* lastbc;
    int i;
    if (!name)
	return -1;
    // we skip to the last backslash
    // this is effectively eliminating weird characters in
    // the text output windows

    lastbc = strrchr(name, '\\');
    if (lastbc)
    {
	int i;
	lastbc++;
	for (i = 0; 1 ;i++)
	{
	    name[i] = *lastbc++;
	    if (!name[i])
		break;
	}
    }
    if(strncmp(name, "c:\\windows\\", 11)==0) name += 11;
    if(strncmp(name, ".\\", 2)==0) name += 2;

    dbgprintf("Entering LoadLibraryA(%s)\n", name);

    // PIMJ and VIVO audio are loading  kernel32.dll
    if (strcasecmp(name, "kernel32.dll") == 0 || strcasecmp(name, "kernel32") == 0)
	return MODULE_HANDLE_kernel32;
//	return ERROR_SUCCESS; /* yeah, we have also the kernel32 calls */
			      /* exported -> do not return failed! */

    if (strcasecmp(name, "user32.dll") == 0 || strcasecmp(name, "user32") == 0)
//	return MODULE_HANDLE_kernel32;
	return MODULE_HANDLE_user32;

    result=LoadLibraryA(name);
    dbgprintf("Returned LoadLibraryA(0x%x='%s'), def_path=%s => 0x%x\n", name, name, def_path, result);

    return result;
}
static int WINAPI expFreeLibrary(int module)
{
    int result=FreeLibrary(module);
    dbgprintf("FreeLibrary(0x%x) => %d\n", module, result);
    return result;
}
static void* WINAPI expGetProcAddress(HMODULE mod, char* name)
{
    void* result;
    switch(mod){
    case MODULE_HANDLE_kernel32:
	result=LookupExternalByName("kernel32.dll", name); break;
    case MODULE_HANDLE_user32:
	result=LookupExternalByName("user32.dll", name); break;
    default:
	result=GetProcAddress(mod, name);
    }
    dbgprintf("GetProcAddress(0x%x, '%s') => 0x%x\n", mod, name, result);
    return result;
}

static long WINAPI expCreateFileMappingA(int hFile, void* lpAttr,
					 long flProtect, long dwMaxHigh,
					 long dwMaxLow, const char* name)
{
    long result=CreateFileMappingA(hFile, lpAttr, flProtect, dwMaxHigh, dwMaxLow, name);
    if(!name)
	dbgprintf("CreateFileMappingA(file 0x%x, lpAttr 0x%x,"
		  "flProtect 0x%x, dwMaxHigh 0x%x, dwMaxLow 0x%x, name 0) => %d\n",
		  hFile, lpAttr, flProtect, dwMaxHigh, dwMaxLow, result);
    else
	dbgprintf("CreateFileMappingA(file 0x%x, lpAttr 0x%x,"
		  "flProtect 0x%x, dwMaxHigh 0x%x, dwMaxLow 0x%x, name 0x%x='%s') => %d\n",
		  hFile, lpAttr, flProtect, dwMaxHigh, dwMaxLow, name, name, result);
    return result;
}

static long WINAPI expOpenFileMappingA(long hFile, long hz, const char* name)
{
    long result=OpenFileMappingA(hFile, hz, name);
    if(!name)
	dbgprintf("OpenFileMappingA(0x%x, 0x%x, 0) => %d\n",
		  hFile, hz, result);
    else
	dbgprintf("OpenFileMappingA(0x%x, 0x%x, 0x%x='%s') => %d\n",
		  hFile, hz, name, name, result);
    return result;
}

static void* WINAPI expMapViewOfFile(HANDLE file, DWORD mode, DWORD offHigh,
				     DWORD offLow, DWORD size)
{
    dbgprintf("MapViewOfFile(0x%x, 0x%x, 0x%x, 0x%x, size %d) => 0x%x\n",
	      file,mode,offHigh,offLow,size,(char*)file+offLow);
    return (char*)file+offLow;
}

static void* WINAPI expUnmapViewOfFile(void* view)
{
    dbgprintf("UnmapViewOfFile(0x%x) => 0\n", view);
    return 0;
}

static void* WINAPI expSleep(int time)
{
#if HAVE_NANOSLEEP
    /* solaris doesn't have thread safe usleep */
    struct timespec tsp;
    tsp.tv_sec  =  time / 1000000;
    tsp.tv_nsec = (time % 1000000) * 1000;
    nanosleep(&tsp, NULL);
#else
    usleep(time);
#endif
    dbgprintf("Sleep(%d) => 0\n", time);
    return 0;
}

// why does IV32 codec want to call this? I don't know ...
static int WINAPI expCreateCompatibleDC(int hdc)
{
    int dc = 0;//0x81;
    //dbgprintf("CreateCompatibleDC(%d) => 0x81\n", hdc);
    dbgprintf("CreateCompatibleDC(%d) => %d\n", hdc, dc);
    return dc;
}

static int WINAPI expGetDeviceCaps(int hdc, int unk)
{
    dbgprintf("GetDeviceCaps(0x%x, %d) => 0\n", hdc, unk);
    return 0;
}

static WIN_BOOL WINAPI expDeleteDC(int hdc)
{
    dbgprintf("DeleteDC(0x%x) => 0\n", hdc);
    if (hdc == 0x81)
	return 1;
    return 0;
}

static WIN_BOOL WINAPI expDeleteObject(int hdc)
{
    dbgprintf("DeleteObject(0x%x) => 1\n", hdc);
    /* FIXME - implement code here */
    return 1;
}

/* btvvc32.drv wants this one */
static void* WINAPI expGetWindowDC(int hdc)
{
    dbgprintf("GetWindowDC(%d) => 0x0\n", hdc);
    return 0;
}

/*
 * Returns the number of milliseconds, modulo 2^32, since the start
 * of the wineserver.
 */
static int WINAPI expGetTickCount(void)
{
    static int tcstart = 0;
    struct timeval t;
    int tc;
    gettimeofday( &t, NULL );
    tc = ((t.tv_sec * 1000) + (t.tv_usec / 1000)) - tcstart;
    if (tcstart == 0)
    {
	tcstart = 0;
        tc = 0;
    }
    dbgprintf("GetTickCount() => %d\n", tc);
    return tc;
}

static int WINAPI expCreateFontA(void)
{
    dbgprintf("CreateFontA() => 0x0\n");
    return 1;
}

/* tried to get pvmjpg work in a different way - no success */
static int WINAPI expDrawTextA(int hDC, char* lpString, int nCount,
			       LPRECT lpRect, unsigned int uFormat)
{
    dbgprintf("expDrawTextA(%p,...) => 8\n", hDC);
    return 8;
}

static int WINAPI expGetPrivateProfileIntA(const char* appname,
					   const char* keyname,
					   int default_value,
					   const char* filename)
{
    int size=255;
    char buffer[256];
    char* fullname;
    int result;

    buffer[255]=0;
    if(!(appname && keyname && filename) )
    {
	dbgprintf("GetPrivateProfileIntA('%s', '%s', %d, '%s') => %d\n", appname, keyname, default_value, filename, default_value );
	return default_value;
    }
    fullname=(char*)malloc(50+strlen(appname)+strlen(keyname)+strlen(filename));
    strcpy(fullname, "Software\\IniFileMapping\\");
    strcat(fullname, appname);
    strcat(fullname, "\\");
    strcat(fullname, keyname);
    strcat(fullname, "\\");
    strcat(fullname, filename);
    result=RegQueryValueExA(HKEY_LOCAL_MACHINE, fullname, NULL, NULL, (int*)buffer, &size);
    if((size>=0)&&(size<256))
	buffer[size]=0;
    //    printf("GetPrivateProfileIntA(%s, %s, %s) -> %s\n", appname, keyname, filename, buffer);
    free(fullname);
    if(result)
	result=default_value;
    else
	result=atoi(buffer);
    dbgprintf("GetPrivateProfileIntA('%s', '%s', %d, '%s') => %d\n", appname, keyname, default_value, filename, result);
    return result;
}
static int WINAPI expGetProfileIntA(const char* appname,
				    const char* keyname,
				    int default_value)
{
    dbgprintf("GetProfileIntA -> ");
    return expGetPrivateProfileIntA(appname, keyname, default_value, "default");
}

static int WINAPI expGetPrivateProfileStringA(const char* appname,
					      const char* keyname,
					      const char* def_val,
					      char* dest, unsigned int len,
					      const char* filename)
{
    int result;
    int size;
    char* fullname;
    dbgprintf("GetPrivateProfileStringA('%s', '%s', def_val '%s', 0x%x, 0x%x, '%s')", appname, keyname, def_val, dest, len, filename );
    if(!(appname && keyname && filename) ) return 0;
    fullname=(char*)malloc(50+strlen(appname)+strlen(keyname)+strlen(filename));
    strcpy(fullname, "Software\\IniFileMapping\\");
    strcat(fullname, appname);
    strcat(fullname, "\\");
    strcat(fullname, keyname);
    strcat(fullname, "\\");
    strcat(fullname, filename);
    size=len;
    result=RegQueryValueExA(HKEY_LOCAL_MACHINE, fullname, NULL, NULL, (int*)dest, &size);
    free(fullname);
    if(result)
    {
	strncpy(dest, def_val, size);
	if (strlen(def_val)< size) size = strlen(def_val);
    }
    dbgprintf(" => %d ( '%s' )\n", size, dest);
    return size;
}
static int WINAPI expWritePrivateProfileStringA(const char* appname,
						const char* keyname,
						const char* string,
						const char* filename)
{
    int size=256;
    char* fullname;
    dbgprintf("WritePrivateProfileStringA('%s', '%s', '%s', '%s')", appname, keyname, string, filename );
    if(!(appname && keyname && filename) )
    {
	dbgprintf(" => -1\n");
	return -1;
    }
    fullname=(char*)malloc(50+strlen(appname)+strlen(keyname)+strlen(filename));
    strcpy(fullname, "Software\\IniFileMapping\\");
    strcat(fullname, appname);
    strcat(fullname, "\\");
    strcat(fullname, keyname);
    strcat(fullname, "\\");
    strcat(fullname, filename);
    RegSetValueExA(HKEY_LOCAL_MACHINE, fullname, 0, REG_SZ, (int*)string, strlen(string));
    //    printf("RegSetValueExA(%s,%d)\n", string, strlen(string));
    //    printf("WritePrivateProfileStringA(%s, %s, %s, %s)\n", appname, keyname, string, filename );
    free(fullname);
    dbgprintf(" => 0\n");
    return 0;
}

unsigned int _GetPrivateProfileIntA(const char* appname, const char* keyname, INT default_value, const char* filename)
{
    return expGetPrivateProfileIntA(appname, keyname, default_value, filename);
}
int _GetPrivateProfileStringA(const char* appname, const char* keyname,
			      const char* def_val, char* dest, unsigned int len, const char* filename)
{
    return expGetPrivateProfileStringA(appname, keyname, def_val, dest, len, filename);
}
int _WritePrivateProfileStringA(const char* appname, const char* keyname,
				const char* string, const char* filename)
{
    return expWritePrivateProfileStringA(appname, keyname, string, filename);
}



static int WINAPI expDefDriverProc(int _private, int id, int msg, int arg1, int arg2)
{
    dbgprintf("DefDriverProc(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) => 0\n", _private, id, msg, arg1, arg2);
    return 0;
}

static int WINAPI expSizeofResource(int v1, int v2)
{
    int result=SizeofResource(v1, v2);
    dbgprintf("SizeofResource(0x%x, 0x%x) => %d\n", v1, v2, result);
    return result;
}

static int WINAPI expGetLastError()
{
    int result=GetLastError();
    dbgprintf("GetLastError() => 0x%x\n", result);
    return result;
}

static void WINAPI expSetLastError(int error)
{
    dbgprintf("SetLastError(0x%x)\n", error);
    SetLastError(error);
}

static int WINAPI expStringFromGUID2(GUID* guid, char* str, int cbMax)
{
    int result=snprintf(str, cbMax, "%.8x-%.4x-%.4x-%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
			guid->f1, guid->f2, guid->f3,
			(unsigned char)guid->f4[0], (unsigned char)guid->f4[1],
			(unsigned char)guid->f4[2], (unsigned char)guid->f4[3],
			(unsigned char)guid->f4[4], (unsigned char)guid->f4[5],
			(unsigned char)guid->f4[6], (unsigned char)guid->f4[7]);
    dbgprintf("StringFromGUID2(0x%x, 0x%x='%s', %d) => %d\n", guid, str, str, cbMax, result);
    return result;
}


static int WINAPI expGetFileVersionInfoSizeA(const char* name, int* lpHandle)
{
    dbgprintf("GetFileVersionInfoSizeA(0x%x='%s', 0x%X) => 0\n", name, name, lpHandle);
    return 0;
}

static int WINAPI expIsBadStringPtrW(const short* string, int nchars)
{
    int result;
    if(string==0)result=1; else result=0;
    dbgprintf("IsBadStringPtrW(0x%x, %d) => %d", string, nchars, result);
    if(string)wch_print(string);
    return result;
}
static int WINAPI expIsBadStringPtrA(const char* string, int nchars)
{
    return expIsBadStringPtrW((const short*)string, nchars);
}
static long WINAPI expInterlockedExchangeAdd( long* dest, long incr )
{
    long ret;
    __asm__ __volatile__
	(
	 "lock; xaddl %0,(%1)"
	 : "=r" (ret)
	 : "r" (dest), "0" (incr)
	 : "memory"
	);
    return ret;
}

static long WINAPI expInterlockedIncrement( long* dest )
{
    long result=expInterlockedExchangeAdd( dest, 1 ) + 1;
    dbgprintf("InterlockedIncrement(0x%x => %d) => %d\n", dest, *dest, result);
    return result;
}
static long WINAPI expInterlockedDecrement( long* dest )
{
    long result=expInterlockedExchangeAdd( dest, -1 ) - 1;
    dbgprintf("InterlockedDecrement(0x%x => %d) => %d\n", dest, *dest, result);
    return result;
}

static void WINAPI expOutputDebugStringA( const char* string )
{
    dbgprintf("OutputDebugStringA(0x%x='%s')\n", string);
    fprintf(stderr, "DEBUG: %s\n", string);
}

static int WINAPI expGetDC(int hwnd)
{
    dbgprintf("GetDC(0x%x) => 0\n", hwnd);
    return 0;
}

static int WINAPI expReleaseDC(int hwnd, int hdc)
{
    dbgprintf("ReleaseDC(0x%x, 0x%x) => 0\n", hwnd, hdc);
    return 0;
}

static int WINAPI expGetDesktopWindow()
{
    dbgprintf("GetDesktopWindow() => 0\n");
    return 0;
}

static int cursor[100];

static int WINAPI expLoadCursorA(int handle,LPCSTR name)
{
    dbgprintf("LoadCursorA(%d, 0x%x='%s') => 0x%x\n", handle, name, (int)&cursor[0]);
    return (int)&cursor[0];
}
static int WINAPI expSetCursor(void *cursor)
{
    dbgprintf("SetCursor(0x%x) => 0x%x\n", cursor, cursor);
    return (int)cursor;
}
static int WINAPI expGetCursorPos(void *cursor)
{
    dbgprintf("GetCursorPos(0x%x) => 0x%x\n", cursor, cursor);
    return 1;
}
static int WINAPI expRegisterWindowMessageA(char *message)
{
    dbgprintf("RegisterWindowMessageA(%s)\n", message);
    return 1;
}
static int WINAPI expGetProcessVersion(int pid)
{
    dbgprintf("GetProcessVersion(%d)\n", pid);
    return 1;
}
static int WINAPI expGetCurrentThread(void)
{
#warning FIXME!
    dbgprintf("GetCurrentThread() => %x\n", 0xcfcf9898);
    return 0xcfcf9898;
}
static int WINAPI expGetOEMCP(void)
{
    dbgprintf("GetOEMCP()\n");
    return 1;
}
static int WINAPI expGetCPInfo(int cp,void *info)
{
    dbgprintf("GetCPInfo()\n");
    return 0;
}
static int WINAPI expGetSystemMetrics(int index)
{
    dbgprintf("GetSystemMetrics(%d)\n", index);
    return 1;
}
static int WINAPI expGetSysColor(int index)
{
    dbgprintf("GetSysColor(%d)\n", index);
    return 1;
}
static int WINAPI expGetSysColorBrush(int index)
{
    dbgprintf("GetSysColorBrush(%d)\n", index);
    return 1;
}



static int WINAPI expGetSystemPaletteEntries(int hdc, int iStartIndex, int nEntries, void* lppe)
{
    dbgprintf("GetSystemPaletteEntries(0x%x, 0x%x, 0x%x, 0x%x) => 0\n",
	      hdc, iStartIndex, nEntries, lppe);
    return 0;
}

/*
 typedef struct _TIME_ZONE_INFORMATION {
 long Bias;
 char StandardName[32];
 SYSTEMTIME StandardDate;
 long StandardBias;
 char DaylightName[32];
 SYSTEMTIME DaylightDate;
 long DaylightBias;
 } TIME_ZONE_INFORMATION, *LPTIME_ZONE_INFORMATION;
 */

static int WINAPI expGetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation)
{
    const short name[]={'C', 'e', 'n', 't', 'r', 'a', 'l', ' ', 'S', 't', 'a',
    'n', 'd', 'a', 'r', 'd', ' ', 'T', 'i', 'm', 'e', 0};
    const short pname[]={'C', 'e', 'n', 't', 'r', 'a', 'l', ' ', 'D', 'a', 'y',
    'l', 'i', 'g', 'h', 't', ' ', 'T', 'i', 'm', 'e', 0};
    dbgprintf("GetTimeZoneInformation(0x%x) => TIME_ZONE_ID_STANDARD\n");
    memset(lpTimeZoneInformation, 0, sizeof(TIME_ZONE_INFORMATION));
    lpTimeZoneInformation->Bias=360;//GMT-6
    memcpy(lpTimeZoneInformation->StandardName, name, sizeof(name));
    lpTimeZoneInformation->StandardDate.wMonth=10;
    lpTimeZoneInformation->StandardDate.wDay=5;
    lpTimeZoneInformation->StandardDate.wHour=2;
    lpTimeZoneInformation->StandardBias=0;
    memcpy(lpTimeZoneInformation->DaylightName, pname, sizeof(pname));
    lpTimeZoneInformation->DaylightDate.wMonth=4;
    lpTimeZoneInformation->DaylightDate.wDay=1;
    lpTimeZoneInformation->DaylightDate.wHour=2;
    lpTimeZoneInformation->DaylightBias=-60;
    return TIME_ZONE_ID_STANDARD;
}

static void WINAPI expGetLocalTime(SYSTEMTIME* systime)
{
    time_t local_time;
    struct tm *local_tm;
    struct timeval tv;

    dbgprintf("GetLocalTime(0x%x)\n");
    gettimeofday(&tv, NULL);
    local_time=tv.tv_sec;
    local_tm=localtime(&local_time);

    systime->wYear = local_tm->tm_year + 1900;
    systime->wMonth = local_tm->tm_mon + 1;
    systime->wDayOfWeek = local_tm->tm_wday;
    systime->wDay = local_tm->tm_mday;
    systime->wHour = local_tm->tm_hour;
    systime->wMinute = local_tm->tm_min;
    systime->wSecond = local_tm->tm_sec;
    systime->wMilliseconds = (tv.tv_usec / 1000) % 1000;
    dbgprintf("  Year: %d\n  Month: %d\n  Day of week: %d\n"
	      "  Day: %d\n  Hour: %d\n  Minute: %d\n  Second:  %d\n"
	      "  Milliseconds: %d\n",
	      systime->wYear, systime->wMonth, systime->wDayOfWeek, systime->wDay,
	      systime->wHour, systime->wMinute, systime->wSecond, systime->wMilliseconds);
}

static int WINAPI expGetSystemTime(SYSTEMTIME* systime)
{
    time_t local_time;
    struct tm *local_tm;
    struct timeval tv;

    dbgprintf("GetSystemTime(0x%x)\n", systime);
    gettimeofday(&tv, NULL);
    local_time=tv.tv_sec;
    local_tm=gmtime(&local_time);

    systime->wYear = local_tm->tm_year + 1900;
    systime->wMonth = local_tm->tm_mon + 1;
    systime->wDayOfWeek = local_tm->tm_wday;
    systime->wDay = local_tm->tm_mday;
    systime->wHour = local_tm->tm_hour;
    systime->wMinute = local_tm->tm_min;
    systime->wSecond = local_tm->tm_sec;
    systime->wMilliseconds = (tv.tv_usec / 1000) % 1000;
    dbgprintf("  Year: %d\n  Month: %d\n  Day of week: %d\n"
	      "  Day: %d\n  Hour: %d\n  Minute: %d\n  Second:  %d\n"
	      "  Milliseconds: %d\n",
	      systime->wYear, systime->wMonth, systime->wDayOfWeek, systime->wDay,
	      systime->wHour, systime->wMinute, systime->wSecond, systime->wMilliseconds);
    return 0;
}

#define SECS_1601_TO_1970  ((369 * 365 + 89) * 86400ULL)
static void WINAPI expGetSystemTimeAsFileTime(FILETIME* systime)
{
    struct tm *local_tm;
    struct timeval tv;
    unsigned long long secs;

    dbgprintf("GetSystemTime(0x%x)\n", systime);
    gettimeofday(&tv, NULL);
    secs = (tv.tv_sec + SECS_1601_TO_1970) * 10000000;
    secs += tv.tv_usec * 10;
    systime->dwLowDateTime = secs & 0xffffffff;
    systime->dwHighDateTime = (secs >> 32);
}

static int WINAPI expGetEnvironmentVariableA(const char* name, char* field, int size)
{
    char *p;
    //    printf("%s %x %x\n", name, field, size);
    if(field)field[0]=0;
    /*
     p = getenv(name);
     if (p) strncpy(field,p,size);
     */
    if (strcmp(name,"__MSVCRT_HEAP_SELECT")==0)
	strcpy(field,"__GLOBAL_HEAP_SELECTED,1");
    dbgprintf("GetEnvironmentVariableA(0x%x='%s', 0x%x, %d) => %d\n", name, name, field, size, strlen(field));
    return strlen(field);
}

static int WINAPI expSetEnvironmentVariableA(const char *name, const char *value)
{
    dbgprintf("SetEnvironmentVariableA(%s, %s)\n", name, value);
    return 0;
}

static void* WINAPI expCoTaskMemAlloc(ULONG cb)
{
    return my_mreq(cb, 0);
}
static void WINAPI expCoTaskMemFree(void* cb)
{
    my_release(cb);
}




void* CoTaskMemAlloc(unsigned long cb)
{
    return expCoTaskMemAlloc(cb);
}
void CoTaskMemFree(void* cb)
{
    expCoTaskMemFree(cb);
}

struct COM_OBJECT_INFO
{
    GUID clsid;
    long (*GetClassObject) (GUID* clsid, const GUID* iid, void** ppv);
};

static struct COM_OBJECT_INFO* com_object_table=0;
static int com_object_size=0;
int RegisterComClass(const GUID* clsid, GETCLASSOBJECT gcs)
{
    if(!clsid || !gcs)
	return -1;
    com_object_table=realloc(com_object_table, sizeof(struct COM_OBJECT_INFO)*(++com_object_size));
    com_object_table[com_object_size-1].clsid=*clsid;
    com_object_table[com_object_size-1].GetClassObject=gcs;
    return 0;
}

int UnregisterComClass(const GUID* clsid, GETCLASSOBJECT gcs)
{
    int found = 0;
    int i = 0;
    if(!clsid || !gcs)
	return -1;

    if (com_object_table == 0)
	printf("Warning: UnregisterComClass() called without any registered class\n");
    while (i < com_object_size)
    {
	if (found && i > 0)
	{
	    memcpy(&com_object_table[i - 1].clsid,
		   &com_object_table[i].clsid, sizeof(GUID));
	    com_object_table[i - 1].GetClassObject =
		com_object_table[i].GetClassObject;
	}
	else if (memcmp(&com_object_table[i].clsid, clsid, sizeof(GUID)) == 0
		 && com_object_table[i].GetClassObject == gcs)
	{
	    found++;
	}
	i++;
    }
    if (found)
    {
	if (--com_object_size == 0)
	{
	    free(com_object_table);
	    com_object_table = 0;
	}
    }
    return 0;
}


const GUID IID_IUnknown =
{
    0x00000000, 0x0000, 0x0000,
    {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
};
const GUID IID_IClassFactory =
{
    0x00000001, 0x0000, 0x0000,
    {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
};

static long WINAPI expCoCreateInstance(GUID* rclsid, struct IUnknown* pUnkOuter,
				       long dwClsContext, const GUID* riid, void** ppv)
{
    int i;
    struct COM_OBJECT_INFO* ci=0;
    for(i=0; i<com_object_size; i++)
	if(!memcmp(rclsid, &com_object_table[i].clsid, sizeof(GUID)))
	    ci=&com_object_table[i];
    if(!ci)return REGDB_E_CLASSNOTREG;
    // in 'real' world we should mess with IClassFactory here
    i=ci->GetClassObject(rclsid, riid, ppv);
    return i;
}

long CoCreateInstance(GUID* rclsid, struct IUnknown* pUnkOuter,
		      long dwClsContext, const GUID* riid, void** ppv)
{
    return expCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

static int WINAPI expIsRectEmpty(CONST RECT *lprc)
{
    int r = 0;
//    int r = (!lprc || (lprc->right == lprc->left) || (lprc->top == lprc->bottom));
    int w,h;

    if (lprc)
    {
	w = lprc->right - lprc->left;
	h = lprc->bottom - lprc->top;
	if (w <= 0 || h <= 0)
	    r = 1;
    }
    else
	r = 1;

    dbgprintf("IsRectEmpty(%p) => %s\n", lprc, (r) ? "TRUE" : "FALSE");
//    printf("Rect: left: %d, top: %d, right: %d, bottom: %d\n",
//	lprc->left, lprc->top, lprc->right, lprc->bottom);
    return r;
}

static int _adjust_fdiv=0; //what's this? - used to adjust division




static unsigned int WINAPI expGetTempPathA(unsigned int len, char* path)
{
    dbgprintf("GetTempPathA(%d, 0x%x)", len, path);
    if(len<5)
    {
	dbgprintf(" => 0\n");
	return 0;
    }
    strcpy(path, "/tmp");
    dbgprintf(" => 5 ( '/tmp' )\n");
    return 5;
}
/*
 FYI:
 typedef struct
 {
 DWORD     dwFileAttributes;
 FILETIME  ftCreationTime;
 FILETIME  ftLastAccessTime;
 FILETIME  ftLastWriteTime;
 DWORD     nFileSizeHigh;
 DWORD     nFileSizeLow;
 DWORD     dwReserved0;
 DWORD     dwReserved1;
 CHAR      cFileName[260];
 CHAR      cAlternateFileName[14];
 } WIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;
 */

static HANDLE WINAPI expFindFirstFileA(LPCSTR s, LPWIN32_FIND_DATAA lpfd)
{
    dbgprintf("FindFirstFileA(0x%x='%s', 0x%x) => 0\n", s, s, lpfd);
    strcpy(lpfd->cFileName, "msms001.vwp");
    strcpy(lpfd->cAlternateFileName, "msms001.vwp");
    return (HANDLE)0;
}
static WIN_BOOL WINAPI expFindNextFileA(HANDLE h,LPWIN32_FIND_DATAA p)
{
    dbgprintf("FindNextFileA(0x%x, 0x%x) => 0\n", h, p);
    return 0;
}
static WIN_BOOL WINAPI expFindClose(HANDLE h)
{
    dbgprintf("FindClose(0x%x) => 0\n", h);
    return 0;
}
static UINT WINAPI expSetErrorMode(UINT i)
{
    dbgprintf("SetErrorMode(%d) => 0\n", i);
    return 0;
}
static UINT WINAPI expGetWindowsDirectoryA(LPSTR s,UINT c)
{
    char windir[]="c:\\windows";
    int result;
    strncpy(s, windir, c);
    result=1+((c<strlen(windir))?c:strlen(windir));
    dbgprintf("GetWindowsDirectoryA(0x%x, %d) => %d\n", s, c, result);
    return result;
}

static WIN_BOOL  WINAPI expDeleteFileA(LPCSTR s)
{
    dbgprintf("DeleteFileA(0x%x='%s') => 0\n", s, s);
    return 0;
}
static WIN_BOOL  WINAPI expFileTimeToLocalFileTime(const FILETIME* cpf, LPFILETIME pf)
{
    dbgprintf("FileTimeToLocalFileTime(0x%x, 0x%x) => 0\n", cpf, pf);
    return 0;
}

static UINT WINAPI expGetTempFileNameA(LPCSTR cs1,LPCSTR cs2,UINT i,LPSTR ps)
{
    char mask[16]="/tmp/AP_XXXXXX";
    int result;
    dbgprintf("GetTempFileNameA(0x%x='%s', 0x%x='%s', %d, 0x%x)", cs1, cs1, cs2, cs2, i, ps);
    if(i && i<10)
    {
	dbgprintf(" => -1\n");
	return -1;
    }
    result=mkstemp(mask);
    sprintf(ps, "AP%d", result);
    dbgprintf(" => %d\n", strlen(ps));
    return strlen(ps);
}
//
// This func might need proper implementation if we want AngelPotion codec.
// They try to open APmpeg4v1.apl with it.
// DLL will close opened file with CloseHandle().
//
static HANDLE WINAPI expCreateFileA(LPCSTR cs1,DWORD i1,DWORD i2,
				    LPSECURITY_ATTRIBUTES p1, DWORD i3,DWORD i4,HANDLE i5)
{
    dbgprintf("CreateFileA(0x%x='%s', %d, %d, 0x%x, %d, %d, 0x%x)\n", cs1, cs1, i1,
	      i2, p1, i3, i4, i5);
    if((!cs1) || (strlen(cs1)<2))return -1;

    if(strncmp(cs1, "AP", 2) == 0)
    {
	int result;
	char* tmp=(char*)malloc(strlen(def_path)+50);
	strcpy(tmp, def_path);
	strcat(tmp, "/");
	strcat(tmp, "APmpg4v1.apl");
	result=open(tmp, O_RDONLY);
	free(tmp);
	return result;
    }
    if (strstr(cs1, "vp3"))
    {
	int r;
	int flg = 0;
	char* tmp=(char*)malloc(20 + strlen(cs1));
	strcpy(tmp, "/tmp/");
	strcat(tmp, cs1);
	r = 4;
	while (tmp[r])
	{
	    if (tmp[r] == ':' || tmp[r] == '\\')
		tmp[r] = '_';
	    r++;
	}
	if (GENERIC_READ & i1)
	    flg |= O_RDONLY;
	else if (GENERIC_WRITE & i1)
	{
	    flg |= O_WRONLY;
	    printf("Warning: openning filename %s  %d (flags; 0x%x) for write\n", tmp, r, flg);
	}
	r=open(tmp, flg);
	free(tmp);
	return r;
    }

#if 0
    /* we need this for some virtualdub filters */
    {
	int r;
	int flg = 0;
	if (GENERIC_READ & i1)
	    flg |= O_RDONLY;
	else if (GENERIC_WRITE & i1)
	{
	    flg |= O_WRONLY;
	    printf("Warning: openning filename %s  %d (flags; 0x%x) for write\n", cs1, r, flg);
	}
	r=open(cs1, flg);
	return r;
    }
#endif

    return atoi(cs1+2);
}
static UINT WINAPI expGetSystemDirectoryA(
  char* lpBuffer,  // address of buffer for system directory
  UINT uSize        // size of directory buffer
){
    dbgprintf("GetSystemDirectoryA(%p,%d)\n", lpBuffer,uSize);
    if(!lpBuffer) strcpy(lpBuffer,".");
    return 1;
}
/*
static char sysdir[]=".";
static LPCSTR WINAPI expGetSystemDirectoryA()
{
    dbgprintf("GetSystemDirectoryA() => 0x%x='%s'\n", sysdir, sysdir);
    return sysdir;
}
*/
static DWORD WINAPI expGetFullPathNameA
(
	LPCTSTR lpFileName,
	DWORD nBufferLength,
	LPTSTR lpBuffer,
	LPTSTR lpFilePart
){
    if(!lpFileName) return 0;
    dbgprintf("GetFullPathNameA('%s',%d,%p,%p)\n",lpFileName,nBufferLength,
	lpBuffer, lpFilePart);
    strcpy(lpFilePart, lpFileName);
    strcpy(lpBuffer, lpFileName);
//    strncpy(lpBuffer, lpFileName, rindex(lpFileName, '\\')-lpFileName);
    return strlen(lpBuffer);
}

static DWORD WINAPI expGetShortPathNameA
(
        LPCSTR longpath,
        LPSTR shortpath,
        DWORD shortlen
){
    if(!longpath) return 0;
    dbgprintf("GetShortPathNameA('%s',%p,%d)\n",longpath,shortpath,shortlen);
    strcpy(shortpath,longpath);
    return strlen(shortpath);
}

static WIN_BOOL WINAPI expReadFile(HANDLE h,LPVOID pv,DWORD size,LPDWORD rd,LPOVERLAPPED unused)
{
    int result;
    dbgprintf("ReadFile(%d, 0x%x, %d -> 0x%x)\n", h, pv, size, rd);
    result=read(h, pv, size);
    if(rd)*rd=result;
    if(!result)return 0;
    return 1;
}

static WIN_BOOL WINAPI expWriteFile(HANDLE h,LPCVOID pv,DWORD size,LPDWORD wr,LPOVERLAPPED unused)
{
    int result;
    dbgprintf("WriteFile(%d, 0x%x, %d -> 0x%x)\n", h, pv, size, wr);
    if(h==1234)h=1;
    result=write(h, pv, size);
    if(wr)*wr=result;
    if(!result)return 0;
    return 1;
}
static DWORD  WINAPI expSetFilePointer(HANDLE h, LONG val, LPLONG ext, DWORD whence)
{
    int wh;
    dbgprintf("SetFilePointer(%d, %d, 0x%x, %d)\n", h, val, ext, whence);
    //why would DLL want temporary file with >2Gb size?
    switch(whence)
    {
    case FILE_BEGIN:
	wh=SEEK_SET;break;
    case FILE_END:
	wh=SEEK_END;break;
    case FILE_CURRENT:
	wh=SEEK_CUR;break;
    default:
	return -1;
    }
    return lseek(h, val, wh);
}

static HDRVR WINAPI expOpenDriverA(LPCSTR szDriverName, LPCSTR szSectionName,
				   LPARAM lParam2)
{
    dbgprintf("OpenDriverA(0x%x='%s', 0x%x='%s', 0x%x) => -1\n", szDriverName,  szDriverName, szSectionName, szSectionName, lParam2);
    return -1;
}
static HDRVR WINAPI expOpenDriver(LPCSTR szDriverName, LPCSTR szSectionName,
				  LPARAM lParam2)
{
    dbgprintf("OpenDriver(0x%x='%s', 0x%x='%s', 0x%x) => -1\n", szDriverName, szDriverName, szSectionName, szSectionName, lParam2);
    return -1;
}


static WIN_BOOL WINAPI expGetProcessAffinityMask(HANDLE hProcess,
						 LPDWORD lpProcessAffinityMask,
						 LPDWORD lpSystemAffinityMask)
{
    dbgprintf("GetProcessAffinityMask(0x%x, 0x%x, 0x%x) => 1\n",
	      hProcess, lpProcessAffinityMask, lpSystemAffinityMask);
    if(lpProcessAffinityMask)*lpProcessAffinityMask=1;
    if(lpSystemAffinityMask)*lpSystemAffinityMask=1;
    return 1;
}

static int WINAPI expMulDiv(int nNumber, int nNumerator, int nDenominator)
{
    static const long long max_int=0x7FFFFFFFLL;
    static const long long min_int=-0x80000000LL;
    long long tmp=(long long)nNumber*(long long)nNumerator;
    dbgprintf("expMulDiv %d * %d / %d\n", nNumber, nNumerator, nDenominator);
    if(!nDenominator)return 1;
    tmp/=nDenominator;
    if(tmp<min_int) return 1;
    if(tmp>max_int) return 1;
    return (int)tmp;
}

static LONG WINAPI explstrcmpiA(const char* str1, const char* str2)
{
    LONG result=strcasecmp(str1, str2);
    dbgprintf("strcmpi(0x%x='%s', 0x%x='%s') => %d\n", str1, str1, str2, str2, result);
    return result;
}

static LONG WINAPI explstrlenA(const char* str1)
{
    LONG result=strlen(str1);
    dbgprintf("strlen(0x%x='%.50s') => %d\n", str1, str1, result);
    return result;
}

static LONG WINAPI explstrcpyA(char* str1, const char* str2)
{
    int result= (int) strcpy(str1, str2);
    dbgprintf("strcpy(0x%.50x, 0x%.50x='%.50s') => %d\n", str1, str2, str2, result);
    return result;
}
static LONG WINAPI explstrcpynA(char* str1, const char* str2,int len)
{
    int result;
    if (strlen(str2)>len)
	result = (int) strncpy(str1, str2,len);
    else
	result = (int) strcpy(str1,str2);
    dbgprintf("strncpy(0x%x, 0x%x='%s' len %d strlen %d) => %x\n", str1, str2, str2,len, strlen(str2),result);
    return result;
}
static LONG WINAPI explstrcatA(char* str1, const char* str2)
{
    int result= (int) strcat(str1, str2);
    dbgprintf("strcat(0x%x, 0x%x='%s') => %d\n", str1, str2, str2, result);
    return result;
}


static LONG WINAPI expInterlockedExchange(long *dest, long l)
{
    long retval = *dest;
    *dest = l;
    return retval;
}

static void WINAPI expInitCommonControls(void)
{
    printf("InitCommonControls called!\n");
    return;
}

/* alex: implement this call! needed for 3ivx */
static HRESULT WINAPI expCoCreateFreeThreadedMarshaler(void *pUnkOuter, void **ppUnkInner)
{
    printf("CoCreateFreeThreadedMarshaler(%p, %p) called!\n",
	   pUnkOuter, ppUnkInner);
//    return 0;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


static int WINAPI expDuplicateHandle(HANDLE hSourceProcessHandle,  // handle to source process
				     HANDLE hSourceHandle,         // handle to duplicate
				     HANDLE hTargetProcessHandle,  // handle to target process
				     HANDLE* lpTargetHandle,      // duplicate handle
				     DWORD dwDesiredAccess,        // requested access
				     int bInheritHandle,          // handle inheritance option
				     DWORD dwOptions               // optional actions
				    )
{
    dbgprintf("DuplicateHandle(%p, %p, %p, %p, 0x%x, %d, %d) called\n",
	      hSourceProcessHandle, hSourceHandle, hTargetProcessHandle,
	      lpTargetHandle, dwDesiredAccess, bInheritHandle, dwOptions);
    *lpTargetHandle = hSourceHandle;
    return 1;
}

// required by PIM1 codec (used by win98 PCTV Studio capture sw)
static HRESULT WINAPI expCoInitialize(
				      LPVOID lpReserved	/* [in] pointer to win32 malloc interface
				      (obsolete, should be NULL) */
				     )
{
    /*
     * Just delegate to the newer method.
     */
    return 0; //CoInitializeEx(lpReserved, COINIT_APARTMENTTHREADED);
}

static DWORD WINAPI expSetThreadAffinityMask
(
        HANDLE hThread,
        DWORD dwThreadAffinityMask
){
    return 0;
};

/*
 * no WINAPI functions - CDECL
 */
static void* expmalloc(int size)
{
    //printf("malloc");
    //    return malloc(size);
    void* result=my_mreq(size,0);
    dbgprintf("malloc(0x%x) => 0x%x\n", size,result);
    if(result==0)
	printf("WARNING: malloc() failed\n");
    return result;
}
static void expfree(void* mem)
{
    //    return free(mem);
    dbgprintf("free(%p)\n", mem);
    my_release(mem);
}
/* needed by atrac3.acm */
static void *expcalloc(int num, int size)
{
    void* result=my_mreq(num*size,1);
    dbgprintf("calloc(%d,%d) => %p\n", num,size,result);
    if(result==0)
	printf("WARNING: calloc() failed\n");
    return result;
}
static void* expnew(int size)
{
    //    printf("NEW:: Call from address %08x\n STACK DUMP:\n", *(-1+(int*)&size));
    //    printf("%08x %08x %08x %08x\n",
    //    size, *(1+(int*)&size),
    //    *(2+(int*)&size),*(3+(int*)&size));
    void* result;
    assert(size >= 0);

    result=my_mreq(size,0);
    dbgprintf("new(%d) => %p\n", size, result);
    if (result==0)
	printf("WARNING: new() failed\n");
    return result;

}
static int expdelete(void* memory)
{
    dbgprintf("delete(%p)\n", memory);
    my_release(memory);
    return 0;
}
#if 0
static int exp_initterm(int v1, int v2)
{
    dbgprintf("_initterm(0x%x, 0x%x) => 0\n", v1, v2);
    return 0;
}
#else
/* merged from wine - 2002.04.21 */
typedef void (*_INITTERMFUNC)();
static int exp_initterm(_INITTERMFUNC *start, _INITTERMFUNC *end)
{
    dbgprintf("_initterm(0x%x, 0x%x) %p\n", start, end, *start);
    while (start < end)
    {
	if (*start)
	{
	    //printf("call _initfunc: from: %p %d\n", *start);
	    // ok this trick with push/pop is necessary as otherwice
	    // edi/esi registers are being trashed
	    void* p = *start;
	    __asm__ __volatile__
		(
		 "pushl %%ebx		\n\t"
		 "pushl %%ecx		\n\t"
		 "pushl %%edx		\n\t"
		 "pushl %%edi		\n\t"
		 "pushl %%esi		\n\t"
		 "call  *%%eax		\n\t"
		 "popl  %%esi		\n\t"
		 "popl  %%edi		\n\t"
		 "popl  %%edx		\n\t"
		 "popl  %%ecx		\n\t"
		 "popl  %%ebx		\n\t"
		 :
		 : "a"(p)
		 : "memory"
		);
            //printf("done  %p  %d:%d\n", end);
	}
	start++;
    }
    return 0;
}
#endif

static void* exp__dllonexit()
{
    // FIXME extract from WINE
    return NULL;
}

static int expwsprintfA(char* string, char* format, ...)
{
    va_list va;
    int result;
    va_start(va, format);
    result = vsprintf(string, format, va);
    dbgprintf("wsprintfA(0x%x, '%s', ...) => %d\n", string, format, result);
    va_end(va);
    return result;
}

static int expsprintf(char* str, const char* format, ...)
{
    va_list args;
    int r;
    dbgprintf("sprintf(%s, %s)\n", str, format);
    va_start(args, format);
    r = vsprintf(str, format, args);
    va_end(args);
    return r;
}
static int expsscanf(const char* str, const char* format, ...)
{
    va_list args;
    int r;
    dbgprintf("sscanf(%s, %s)\n", str, format);
    va_start(args, format);
    r = vsscanf(str, format, args);
    va_end(args);
    return r;
}
static void* expfopen(const char* path, const char* mode)
{
    printf("fopen: \"%s\"  mode:%s\n", path, mode);
    //return fopen(path, mode);
    return fdopen(0, mode); // everything on screen
}
static int expfprintf(void* stream, const char* format, ...)
{
    va_list args;
    int r = 0;
    dbgprintf("fprintf(%p, %s, ...)\n", stream, format);
#if 1
    va_start(args, format);
    r = vfprintf((FILE*) stream, format, args);
    va_end(args);
#endif
    return r;
}

static int expprintf(const char* format, ...)
{
    va_list args;
    int r;
    dbgprintf("printf(%s, ...)\n", format);
    va_start(args, format);
    r = vprintf(format, args);
    va_end(args);
    return r;
}

static char* expgetenv(const char* varname)
{
    char* v = getenv(varname);
    dbgprintf("getenv(%s) => %s\n", varname, v);
    return v;
}

static void* expwcscpy(WCHAR* dst, const WCHAR* src)
{
    WCHAR* p = dst;
    while ((*p++ = *src++))
	;
    return dst;
}

static char* expstrrchr(char* string, int value)
{
    char* result=strrchr(string, value);
    if(result)
	dbgprintf("strrchr(0x%x='%s', %d) => 0x%x='%s'", string, string, value, result, result);
    else
	dbgprintf("strrchr(0x%x='%s', %d) => 0", string, string, value);
    return result;
}

static char* expstrchr(char* string, int value)
{
    char* result=strchr(string, value);
    if(result)
	dbgprintf("strchr(0x%x='%s', %d) => 0x%x='%s'", string, string, value, result, result);
    else
	dbgprintf("strchr(0x%x='%s', %d) => 0", string, string, value);
    return result;
}
static int expstrlen(char* str)
{
    int result=strlen(str);
    dbgprintf("strlen(0x%x='%s') => %d\n", str, str, result);
    return result;
}
static char* expstrcpy(char* str1, const char* str2)
{
    char* result= strcpy(str1, str2);
    dbgprintf("strcpy(0x%x, 0x%x='%s') => %p\n", str1, str2, str2, result);
    return result;
}
static int expstrcmp(const char* str1, const char* str2)
{
    int result=strcmp(str1, str2);
    dbgprintf("strcmp(0x%x='%s', 0x%x='%s') => %d\n", str1, str1, str2, str2, result);
    return result;
}
static int expstrncmp(const char* str1, const char* str2,int x)
{
    int result=strncmp(str1, str2,x);
    dbgprintf("strcmp(0x%x='%s', 0x%x='%s') => %d\n", str1, str1, str2, str2, result);
    return result;
}
static char* expstrcat(char* str1, const char* str2)
{
    char* result = strcat(str1, str2);
    dbgprintf("strcat(0x%x='%s', 0x%x='%s') => %p\n", str1, str1, str2, str2, result);
    return result;
}
static char* exp_strdup(const char* str1)
{
    int l = strlen(str1);
    char* result = (char*) my_mreq(l + 1,0);
    if (result)
	strcpy(result, str1);
    dbgprintf("_strdup(0x%x='%s') => %p\n", str1, str1, result);
    return result;
}
static int expisalnum(int c)
{
    int result= (int) isalnum(c);
    dbgprintf("isalnum(0x%x='%c' => %d\n", c, c, result);
    return result;
}
static int expisspace(int c)
{
    int result= (int) isspace(c);
    dbgprintf("isspace(0x%x='%c' => %d\n", c, c, result);
    return result;
}
static int expisalpha(int c)
{
    int result= (int) isalpha(c);
    dbgprintf("isalpha(0x%x='%c' => %d\n", c, c, result);
    return result;
}
static int expisdigit(int c)
{
    int result= (int) isdigit(c);
    dbgprintf("isdigit(0x%x='%c' => %d\n", c, c, result);
    return result;
}
static void* expmemmove(void* dest, void* src, int n)
{
    void* result = memmove(dest, src, n);
    dbgprintf("memmove(0x%x, 0x%x, %d) => %p\n", dest, src, n, result);
    return result;
}
static int expmemcmp(void* dest, void* src, int n)
{
    int result = memcmp(dest, src, n);
    dbgprintf("memcmp(0x%x, 0x%x, %d) => %d\n", dest, src, n, result);
    return result;
}
static void* expmemcpy(void* dest, void* src, int n)
{
    void *result = memcpy(dest, src, n);
    dbgprintf("memcpy(0x%x, 0x%x, %d) => %p\n", dest, src, n, result);
    return result;
}
static void* expmemset(void* dest, int c, size_t n)
{
    void *result = memset(dest, c, n);
    dbgprintf("memset(0x%x, %d, %d) => %p\n", dest, c, n, result);
    return result;
}
static time_t exptime(time_t* t)
{
    time_t result = time(t);
    dbgprintf("time(0x%x) => %d\n", t, result);
    return result;
}

static int exprand(void)
{
    return rand();
}

static void expsrand(int seed)
{
    srand(seed);
}

#if 1

// prefered compilation with  -O2 -ffast-math !

static double explog10(double x)
{
    /*printf("Log10 %f => %f    0x%Lx\n", x, log10(x), *((int64_t*)&x));*/
    return log10(x);
}

static double expcos(double x)
{
    /*printf("Cos %f => %f  0x%Lx\n", x, cos(x), *((int64_t*)&x));*/
    return cos(x);
}

/* doens't work */
static long exp_ftol_wrong(double x)
{
    return (long) x;
}

#else

static void explog10(void)
{
    __asm__ __volatile__
	(
	 "fldl 8(%esp)	\n\t"
	 "fldln2	\n\t"
	 "fxch %st(1)	\n\t"
	 "fyl2x		\n\t"
	);
}

static void expcos(void)
{
    __asm__ __volatile__
	(
	 "fldl 8(%esp)	\n\t"
	 "fcos		\n\t"
	);
}

#endif

// this seem to be the only how to make this function working properly
// ok - I've spent tremendous amount of time (many many many hours
// of debuging fixing & testing - it's almost unimaginable - kabi

// _ftol - operated on the float value which is already on the FPU stack

static void exp_ftol(void)
{
    __asm__ __volatile__
	(
	 "sub $12, %esp		\n\t"
	 "fstcw   -2(%ebp)	\n\t"
	 "wait			\n\t"
	 "movw	  -2(%ebp), %ax	\n\t"
	 "orb	 $0x0C, %ah	\n\t"
	 "movw    %ax, -4(%ebp)	\n\t"
	 "fldcw   -4(%ebp)	\n\t"
	 "fistpl -12(%ebp)	\n\t"
	 "fldcw   -2(%ebp)	\n\t"
	 "movl	 -12(%ebp), %eax \n\t"
	 //Note: gcc 3.03 does not do the following op if it
	 //      knows that ebp=esp
	 "movl %ebp, %esp       \n\t"
	);
}

static double exppow(double x, double y)
{
    /*printf("Pow %f  %f    0x%Lx  0x%Lx  => %f\n", x, y, *((int64_t*)&x), *((int64_t*)&y), pow(x, y));*/
    return pow(x, y);
}



static int exp_stricmp(const char* s1, const char* s2)
{
    return strcasecmp(s1, s2);
}

/* from declaration taken from Wine sources - this fountion seems to be
 * undocumented in any M$ doc */
static int exp_setjmp3(void* jmpbuf, int x)
{
    //dbgprintf("!!!!UNIMPLEMENTED: setjmp3(%p, %d) => 0\n", jmpbuf, x);
    //return 0;
    __asm__ __volatile__
	(
	 //"mov 4(%%esp), %%edx	\n\t"
	 "mov (%%esp), %%eax   \n\t"
	 "mov %%eax, (%%edx)	\n\t" // store ebp

	 //"mov %%ebp, (%%edx)	\n\t"
	 "mov %%ebx, 4(%%edx)	\n\t"
	 "mov %%edi, 8(%%edx)	\n\t"
	 "mov %%esi, 12(%%edx)	\n\t"
	 "mov %%esp, 16(%%edx)	\n\t"

	 "mov 4(%%esp), %%eax	\n\t"
	 "mov %%eax, 20(%%edx)	\n\t"

	 "movl $0x56433230, 32(%%edx)	\n\t" // VC20 ??
	 "movl $0, 36(%%edx)	\n\t"
	 : // output
	 : "d"(jmpbuf) // input
	 : "eax"
	);
#if 1
    __asm__ __volatile__
	(
	 "mov %%fs:0, %%eax	\n\t" // unsure
	 "mov %%eax, 24(%%edx)	\n\t"
	 "cmp $0xffffffff, %%eax \n\t"
	 "jnz l1                \n\t"
	 "mov %%eax, 28(%%edx)	\n\t"
	 "l1:                   \n\t"
	 :
	 :
	 : "eax"
	);
#endif

	return 0;
}

static DWORD WINAPI expGetCurrentProcessId(void)
{
    return getpid(); //(DWORD)NtCurrentTeb()->pid;
}

static HANDLE WINAPI
expCreateMutexA( SECURITY_ATTRIBUTES *sa, WIN_BOOL owner, LPCSTR name ){
    static int x=0xcfcf9898;
    //++x;
    dbgprintf("CreateMutexA(%p,%d,'%s') => %d\n",sa,owner,name,x);
    return x;
}

typedef struct {
    UINT	wPeriodMin;
    UINT	wPeriodMax;
} TIMECAPS, *LPTIMECAPS;

static MMRESULT WINAPI exptimeGetDevCaps(LPTIMECAPS lpCaps, UINT wSize)
{
    dbgprintf("timeGetDevCaps(%p, %u) !\n", lpCaps, wSize);

    lpCaps->wPeriodMin = 1;
    lpCaps->wPeriodMax = 65535;
    return 0;
}

static MMRESULT WINAPI exptimeBeginPeriod(UINT wPeriod)
{
    dbgprintf("timeBeginPeriod(%u) !\n", wPeriod);

    if (wPeriod < 1 || wPeriod > 65535) return 96+1; //TIMERR_NOCANDO;
    return 0;
}

static void WINAPI expGlobalMemoryStatus(
            LPMEMORYSTATUS lpmem
) {
    static MEMORYSTATUS	cached_memstatus;
    static int cache_lastchecked = 0;
    SYSTEM_INFO si;
    FILE *f;

    if (time(NULL)==cache_lastchecked) {
	memcpy(lpmem,&cached_memstatus,sizeof(MEMORYSTATUS));
	return;
    }

#if 1
    f = fopen( "/proc/meminfo", "r" );
    if (f)
    {
        char buffer[256];
        int total, used, free, shared, buffers, cached;

        lpmem->dwLength = sizeof(MEMORYSTATUS);
        lpmem->dwTotalPhys = lpmem->dwAvailPhys = 0;
        lpmem->dwTotalPageFile = lpmem->dwAvailPageFile = 0;
        while (fgets( buffer, sizeof(buffer), f ))
        {
	    /* old style /proc/meminfo ... */
            if (sscanf( buffer, "Mem: %d %d %d %d %d %d", &total, &used, &free, &shared, &buffers, &cached ))
            {
                lpmem->dwTotalPhys += total;
                lpmem->dwAvailPhys += free + buffers + cached;
            }
            if (sscanf( buffer, "Swap: %d %d %d", &total, &used, &free ))
            {
                lpmem->dwTotalPageFile += total;
                lpmem->dwAvailPageFile += free;
            }

	    /* new style /proc/meminfo ... */
	    if (sscanf(buffer, "MemTotal: %d", &total))
	    	lpmem->dwTotalPhys = total*1024;
	    if (sscanf(buffer, "MemFree: %d", &free))
	    	lpmem->dwAvailPhys = free*1024;
	    if (sscanf(buffer, "SwapTotal: %d", &total))
	        lpmem->dwTotalPageFile = total*1024;
	    if (sscanf(buffer, "SwapFree: %d", &free))
	        lpmem->dwAvailPageFile = free*1024;
	    if (sscanf(buffer, "Buffers: %d", &buffers))
	        lpmem->dwAvailPhys += buffers*1024;
	    if (sscanf(buffer, "Cached: %d", &cached))
	        lpmem->dwAvailPhys += cached*1024;
        }
        fclose( f );

        if (lpmem->dwTotalPhys)
        {
            DWORD TotalPhysical = lpmem->dwTotalPhys+lpmem->dwTotalPageFile;
            DWORD AvailPhysical = lpmem->dwAvailPhys+lpmem->dwAvailPageFile;
            lpmem->dwMemoryLoad = (TotalPhysical-AvailPhysical)
                                      / (TotalPhysical / 100);
        }
    } else
#endif
    {
	/* FIXME: should do something for other systems */
	lpmem->dwMemoryLoad    = 0;
	lpmem->dwTotalPhys     = 16*1024*1024;
	lpmem->dwAvailPhys     = 16*1024*1024;
	lpmem->dwTotalPageFile = 16*1024*1024;
	lpmem->dwAvailPageFile = 16*1024*1024;
    }
    expGetSystemInfo(&si);
    lpmem->dwTotalVirtual  = si.lpMaximumApplicationAddress-si.lpMinimumApplicationAddress;
    /* FIXME: we should track down all the already allocated VM pages and substract them, for now arbitrarily remove 64KB so that it matches NT */
    lpmem->dwAvailVirtual  = lpmem->dwTotalVirtual-64*1024;
    memcpy(&cached_memstatus,lpmem,sizeof(MEMORYSTATUS));
    cache_lastchecked = time(NULL);

    /* it appears some memory display programs want to divide by these values */
    if(lpmem->dwTotalPageFile==0)
        lpmem->dwTotalPageFile++;

    if(lpmem->dwAvailPageFile==0)
        lpmem->dwAvailPageFile++;
}

/**********************************************************************
 * SetThreadPriority [KERNEL32.@]  Sets priority for thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
static WIN_BOOL WINAPI expSetThreadPriority(
    HANDLE hthread, /* [in] Handle to thread */
    INT priority)   /* [in] Thread priority level */
{
    dbgprintf("SetThreadPriority(%p,%d)\n",hthread,priority);
    return TRUE;
}

static void WINAPI expExitProcess( DWORD status )
{
    printf("EXIT - code %d\n",status);
    exit(status);
}

static INT WINAPI expMessageBoxA(HWND hWnd, LPCSTR text, LPCSTR title, UINT type){
    printf("MSGBOX '%s' '%s' (%d)\n",text,title,type);
    return 1;
}

/* these are needed for mss1 */

/* defined in stubs.s */
void exp_EH_prolog(void);

#include <netinet/in.h>
static WINAPI inline unsigned long int exphtonl(unsigned long int hostlong)
{
//    dbgprintf("htonl(%x) => %x\n", hostlong, htonl(hostlong));
    return htonl(hostlong);
}

static WINAPI inline unsigned long int expntohl(unsigned long int netlong)
{
//    dbgprintf("ntohl(%x) => %x\n", netlong, ntohl(netlong));
    return ntohl(netlong);
}
static void WINAPI expVariantInit(void* p)
{
    printf("InitCommonControls called!\n");
    return;
}

int expRegisterClassA(const void/*WNDCLASSA*/ *wc)
{
    dbgprintf("RegisterClassA(%p) => random id\n", wc);
    return time(NULL); /* be precise ! */
}

int expUnregisterClassA(const char *className, HINSTANCE hInstance)
{
    dbgprintf("UnregisterClassA(%s, %p) => 0\n", className, hInstance);
    return 0;
}

struct exports
{
    char name[64];
    int id;
    void* func;
};
struct libs
{
    char name[64];
    int length;
    struct exports* exps;
};

#define FF(X,Y) \
    {#X, Y, (void*)exp##X},

struct exports exp_kernel32[]=
{
    FF(IsBadWritePtr, 357)
    FF(IsBadReadPtr, 354)
    FF(IsBadStringPtrW, -1)
    FF(IsBadStringPtrA, -1)
    FF(DisableThreadLibraryCalls, -1)
    FF(CreateThread, -1)
    FF(CreateEventA, -1)
    FF(SetEvent, -1)
    FF(ResetEvent, -1)
    FF(WaitForSingleObject, -1)
    FF(GetSystemInfo, -1)
    FF(GetVersion, 332)
    FF(HeapCreate, 461)
    FF(HeapAlloc, -1)
    FF(HeapDestroy, -1)
    FF(HeapFree, -1)
    FF(HeapSize, -1)
    FF(HeapReAlloc,-1)
    FF(GetProcessHeap, -1)
    FF(VirtualAlloc, -1)
    FF(VirtualFree, -1)
    FF(InitializeCriticalSection, -1)
    FF(EnterCriticalSection, -1)
    FF(LeaveCriticalSection, -1)
    FF(DeleteCriticalSection, -1)
    FF(TlsAlloc, -1)
    FF(TlsFree, -1)
    FF(TlsGetValue, -1)
    FF(TlsSetValue, -1)
    FF(GetCurrentThreadId, -1)
    FF(GetCurrentProcess, -1)
    FF(LocalAlloc, -1)
    FF(LocalReAlloc,-1)
    FF(LocalLock, -1)
    FF(GlobalAlloc, -1)
    FF(GlobalReAlloc, -1)
    FF(GlobalLock, -1)
    FF(GlobalSize, -1)
    FF(MultiByteToWideChar, 427)
    FF(WideCharToMultiByte, -1)
    FF(GetVersionExA, -1)
    FF(CreateSemaphoreA, -1)
    FF(QueryPerformanceCounter, -1)
    FF(QueryPerformanceFrequency, -1)
    FF(LocalHandle, -1)
    FF(LocalUnlock, -1)
    FF(LocalFree, -1)
    FF(GlobalHandle, -1)
    FF(GlobalUnlock, -1)
    FF(GlobalFree, -1)
    FF(LoadResource, -1)
    FF(ReleaseSemaphore, -1)
    FF(FindResourceA, -1)
    FF(LockResource, -1)
    FF(FreeResource, -1)
    FF(SizeofResource, -1)
    FF(CloseHandle, -1)
    FF(GetCommandLineA, -1)
    FF(GetEnvironmentStringsW, -1)
    FF(FreeEnvironmentStringsW, -1)
    FF(FreeEnvironmentStringsA, -1)
    FF(GetEnvironmentStrings, -1)
    FF(GetStartupInfoA, -1)
    FF(GetStdHandle, -1)
    FF(GetFileType, -1)
    FF(SetHandleCount, -1)
    FF(GetACP, -1)
    FF(GetModuleFileNameA, -1)
    FF(SetUnhandledExceptionFilter, -1)
    FF(LoadLibraryA, -1)
    FF(GetProcAddress, -1)
    FF(FreeLibrary, -1)
    FF(CreateFileMappingA, -1)
    FF(OpenFileMappingA, -1)
    FF(MapViewOfFile, -1)
    FF(UnmapViewOfFile, -1)
    FF(Sleep, -1)
    FF(GetModuleHandleA, -1)
    FF(GetProfileIntA, -1)
    FF(GetPrivateProfileIntA, -1)
    FF(GetPrivateProfileStringA, -1)
    FF(WritePrivateProfileStringA, -1)
    FF(GetLastError, -1)
    FF(SetLastError, -1)
    FF(InterlockedIncrement, -1)
    FF(InterlockedDecrement, -1)
    FF(GetTimeZoneInformation, -1)
    FF(OutputDebugStringA, -1)
    FF(GetLocalTime, -1)
    FF(GetSystemTime, -1)
    FF(GetSystemTimeAsFileTime, -1)
    FF(GetEnvironmentVariableA, -1)
    FF(SetEnvironmentVariableA, -1)
    FF(RtlZeroMemory,-1)
    FF(RtlMoveMemory,-1)
    FF(RtlFillMemory,-1)
    FF(GetTempPathA,-1)
    FF(FindFirstFileA,-1)
    FF(FindNextFileA,-1)
    FF(FindClose,-1)
    FF(FileTimeToLocalFileTime,-1)
    FF(DeleteFileA,-1)
    FF(ReadFile,-1)
    FF(WriteFile,-1)
    FF(SetFilePointer,-1)
    FF(GetTempFileNameA,-1)
    FF(CreateFileA,-1)
    FF(GetSystemDirectoryA,-1)
    FF(GetWindowsDirectoryA,-1)
    FF(GetShortPathNameA,-1)
    FF(GetFullPathNameA,-1)
    FF(SetErrorMode, -1)
    FF(IsProcessorFeaturePresent, -1)
    FF(GetProcessAffinityMask, -1)
    FF(InterlockedExchange, -1)
    FF(MulDiv, -1)
    FF(lstrcmpiA, -1)
    FF(lstrlenA, -1)
    FF(lstrcpyA, -1)
    FF(lstrcatA, -1)
    FF(lstrcpynA,-1)
    FF(GetProcessVersion,-1)
    FF(GetCurrentThread,-1)
    FF(GetOEMCP,-1)
    FF(GetCPInfo,-1)
    FF(DuplicateHandle,-1)
    FF(GetTickCount, -1)
    FF(SetThreadAffinityMask,-1)
    FF(GetCurrentProcessId,-1)
    FF(CreateMutexA,-1)
    FF(GlobalMemoryStatus,-1)
    FF(SetThreadPriority,-1)
    FF(ExitProcess,-1)
    {"LoadLibraryExA", -1, (void*)&LoadLibraryExA},
};

struct exports exp_msvcrt[]={
    FF(malloc, -1)
    FF(_initterm, -1)
    FF(__dllonexit, -1)
    FF(free, -1)
    {"??3@YAXPAX@Z", -1, expdelete},
    {"??2@YAPAXI@Z", -1, expnew},
    {"_adjust_fdiv", -1, (void*)&_adjust_fdiv},
    FF(strrchr, -1)
    FF(strchr, -1)
    FF(strlen, -1)
    FF(strcpy, -1)
    FF(wcscpy, -1)
    FF(strcmp, -1)
    FF(strncmp, -1)
    FF(strcat, -1)
    FF(_stricmp,-1)
    FF(_strdup,-1)
    FF(_setjmp3,-1)
    FF(isalnum, -1)
    FF(isspace, -1)
    FF(isalpha, -1)
    FF(isdigit, -1)
    FF(memmove, -1)
    FF(memcmp, -1)
    FF(memset, -1)
    FF(memcpy, -1)
    FF(time, -1)
    FF(rand, -1)
    FF(srand, -1)
    FF(log10, -1)
    FF(pow, -1)
    FF(cos, -1)
    FF(_ftol,-1)
    FF(sprintf,-1)
    FF(sscanf,-1)
    FF(fopen,-1)
    FF(fprintf,-1)
    FF(printf,-1)
    FF(getenv,-1)
    FF(_EH_prolog,-1)
    FF(calloc,-1)
    {"ceil",-1,(void*)&ceil}
};
struct exports exp_winmm[]={
    FF(GetDriverModuleHandle, -1)
    FF(timeGetTime, -1)
    FF(DefDriverProc, -1)
    FF(OpenDriverA, -1)
    FF(OpenDriver, -1)
    FF(timeGetDevCaps, -1)
    FF(timeBeginPeriod, -1)
};
struct exports exp_user32[]={
    FF(LoadStringA, -1)
    FF(wsprintfA, -1)
    FF(GetDC, -1)
    FF(GetDesktopWindow, -1)
    FF(ReleaseDC, -1)
    FF(IsRectEmpty, -1)
    FF(LoadCursorA,-1)
    FF(SetCursor,-1)
    FF(GetCursorPos,-1)
    FF(GetCursorPos,-1)
    FF(RegisterWindowMessageA,-1)
    FF(GetSystemMetrics,-1)
    FF(GetSysColor,-1)
    FF(GetSysColorBrush,-1)
    FF(GetWindowDC, -1)
    FF(DrawTextA, -1)
    FF(MessageBoxA, -1)
    FF(RegisterClassA, -1)
    FF(UnregisterClassA, -1)
};
struct exports exp_advapi32[]={
    FF(RegCloseKey, -1)
    FF(RegCreateKeyExA, -1)
    FF(RegEnumKeyExA, -1)
    FF(RegEnumValueA, -1)
    FF(RegOpenKeyA, -1)
    FF(RegOpenKeyExA, -1)
    FF(RegQueryValueExA, -1)
    FF(RegSetValueExA, -1)
};
struct exports exp_gdi32[]={
    FF(CreateCompatibleDC, -1)
    FF(CreateFontA, -1)
    FF(DeleteDC, -1)
    FF(DeleteObject, -1)
    FF(GetDeviceCaps, -1)
    FF(GetSystemPaletteEntries, -1)
};
struct exports exp_version[]={
    FF(GetFileVersionInfoSizeA, -1)
};
struct exports exp_ole32[]={
    FF(CoCreateFreeThreadedMarshaler,-1)
    FF(CoCreateInstance, -1)
    FF(CoInitialize, -1)
    FF(CoTaskMemAlloc, -1)
    FF(CoTaskMemFree, -1)
    FF(StringFromGUID2, -1)
};
// do we really need crtdll ???
// msvcrt is the correct place probably...
struct exports exp_crtdll[]={
    FF(memcpy, -1)
    FF(wcscpy, -1)
};
struct exports exp_comctl32[]={
    FF(StringFromGUID2, -1)
    FF(InitCommonControls, 17)
};
struct exports exp_wsock32[]={
    FF(htonl,8)
    FF(ntohl,14)
};
struct exports exp_msdmo[]={
    FF(memcpy, -1) // just test
/*
    FF(MoCopyMediaType)
    FF(MoCreateMediaType)
    FF(MoDeleteMediaType)
    FF(MoDuplicateMediaType)
    FF(MoFreeMediaType)
    FF(MoInitMediaType)
*/
};
struct exports exp_oleaut32[]={
    FF(VariantInit, 8)
};

/*  realplayer8:
	DLL Name: PNCRT.dll
	vma:  Hint/Ord Member-Name
	22ff4	  615  free
	2302e	  250  _ftol
	22fea	  666  malloc
	2303e	  609  fprintf
	2305e	  167  _adjust_fdiv
	23052	  280  _initterm

	22ffc	  176  _beginthreadex
	23036	  284  _iob
	2300e	   85  __CxxFrameHandler
	23022	  411  _purecall
*/
#ifdef MPLAYER
struct exports exp_pncrt[]={
    FF(malloc, -1) // just test
    FF(free, -1) // just test
    FF(fprintf, -1) // just test
    {"_adjust_fdiv", -1, (void*)&_adjust_fdiv},
    FF(_ftol,-1)
    FF(_initterm, -1)
};
#endif

#define LL(X) \
    {#X".dll", sizeof(exp_##X)/sizeof(struct exports), exp_##X},

struct libs libraries[]={
    LL(kernel32)
    LL(msvcrt)
    LL(winmm)
    LL(user32)
    LL(advapi32)
    LL(gdi32)
    LL(version)
    LL(ole32)
    LL(oleaut32)
    LL(crtdll)
    LL(comctl32)
    LL(wsock32)
    LL(msdmo)
#ifdef MPLAYER
    LL(pncrt)
#endif
};

static void ext_stubs(void)
{
    // expects:
    //  ax  position index
    //  cx  address of printf function
#if 1
    __asm__ __volatile__
	(
         "push %%edx		\n\t"
	 "movl $0xdeadbeef, %%eax \n\t"
	 "movl $0xdeadbeef, %%edx \n\t"
	 "shl $5, %%eax		\n\t"			// ax * 32
	 "addl $0xdeadbeef, %%eax \n\t"			// overwrite export_names
	 "pushl %%eax		\n\t"
	 "pushl $0xdeadbeef   	\n\t"                   // overwrite called_unk
	 "call *%%edx		\n\t"                   // printf (via dx)
	 "addl $8, %%esp	\n\t"
	 "xorl %%eax, %%eax	\n\t"
	 "pop %%edx             \n\t"
	 :
	 :
	 : "eax"
	);
#else
    __asm__ __volatile__
	(
         "push %%edx		\n\t"
	 "movl $0, %%eax	\n\t"
	 "movl $0, %%edx	\n\t"
	 "shl $5, %%eax		\n\t"			// ax * 32
	 "addl %0, %%eax	\n\t"
	 "pushl %%eax		\n\t"
	 "pushl %1		\n\t"
	 "call *%%edx		\n\t"                   // printf (via dx)
	 "addl $8, %%esp	\n\t"
	 "xorl %%eax, %%eax	\n\t"
	 "pop %%edx		\n\t"
	 ::"m"(*export_names), "m"(*called_unk)
	: "memory", "edx", "eax"
	);
#endif

}

//static void add_stub(int pos)

extern int unk_exp1;
static int pos=0;
static char extcode[20000];// place for 200 unresolved exports
static const char* called_unk = "Called unk_%s\n";

static void* add_stub()
{
    // generated code in runtime!
    char* answ = (char*)extcode+pos*0x30;
#if 0
    memcpy(answ, &unk_exp1, 0x64);
    *(int*)(answ+9)=pos;
    *(int*)(answ+47)-=((int)answ-(int)&unk_exp1);
#endif
    memcpy(answ, ext_stubs, 0x2f); // 0x2c is current size
    //answ[4] = 0xb8; // movl $0, eax  (0xb8 0x00000000)
    *((int*) (answ + 5)) = pos;
    //answ[9] = 0xba; // movl $0, edx  (0xba 0x00000000)
    *((long*) (answ + 10)) = (long)printf;
    //answ[17] = 0x05; // addl $0, eax  (0x05 0x00000000)
    *((long*) (answ + 18)) = (long)export_names;
    //answ[23] = 0x68; // pushl $0  (0x68 0x00000000)
    *((long*) (answ + 24)) = (long)called_unk;
    pos++;
    return (void*)answ;
}

void* LookupExternal(const char* library, int ordinal)
{
    int i,j;
    if(library==0)
    {
	printf("ERROR: library=0\n");
	return (void*)ext_unknown;
    }
    //    printf("%x %x\n", &unk_exp1, &unk_exp2);

    printf("External func %s:%d\n", library, ordinal);

    for(i=0; i<sizeof(libraries)/sizeof(struct libs); i++)
    {
	if(strcasecmp(library, libraries[i].name))
	    continue;
	for(j=0; j<libraries[i].length; j++)
	{
	    if(ordinal!=libraries[i].exps[j].id)
		continue;
	    //printf("Hit: 0x%p\n", libraries[i].exps[j].func);
	    return libraries[i].exps[j].func;
	}
    }

    /* ok, this is a hack, and a big memory leak. should be fixed. - alex */
    {
	int hand;
	WINE_MODREF *wm;
	void *func;

	hand = LoadLibraryA(library);
	if (!hand)
	    goto no_dll;
	wm = MODULE32_LookupHMODULE(hand);
	if (!wm)
	{
	    FreeLibrary(hand);
	    goto no_dll;
	}
	func = PE_FindExportedFunction(wm, (LPCSTR) ordinal, 0);
	if (!func)
	{
	    printf("No such ordinal in external dll\n");
	    FreeLibrary((int)hand);
	    goto no_dll;
	}

	printf("External dll loaded (offset: 0x%x, func: %p)\n",
	       hand, func);
	return func;
    }

no_dll:
    if(pos>150)return 0;
    sprintf(export_names[pos], "%s:%d", library, ordinal);
    return add_stub();
}

void* LookupExternalByName(const char* library, const char* name)
{
    char* answ;
    int i,j;
    //   return (void*)ext_unknown;
    if(library==0)
    {
	printf("ERROR: library=0\n");
	return (void*)ext_unknown;
    }
    if(name==0)
    {
	printf("ERROR: name=0\n");
	return (void*)ext_unknown;
    }
    //printf("External func %s:%s\n", library, name);
    for(i=0; i<sizeof(libraries)/sizeof(struct libs); i++)
    {
	if(strcasecmp(library, libraries[i].name))
	    continue;
	for(j=0; j<libraries[i].length; j++)
	{
	    if(strcmp(name, libraries[i].exps[j].name))
		continue;
	    //	    printf("Hit: 0x%08X\n", libraries[i].exps[j].func);
	    return libraries[i].exps[j].func;
	}
    }
    if(pos>150)return 0;// to many symbols
    strcpy(export_names[pos], name);
    return add_stub();
}

void my_garbagecollection(void)
{
#ifdef GARBAGE
    int unfree = 0, unfreecnt = 0;

    int max_fatal = 8;
    free_registry();
    while (last_alloc)
    {
	alloc_header* mem = last_alloc + 1;
	unfree += my_size(mem);
	unfreecnt++;
	if (my_release(mem) != 0)
	    // avoid endless loop when memory is trashed
	    if (--max_fatal < 0)
		break;
    }
    printf("Total Unfree %d bytes cnt %d [%p,%d]\n",unfree, unfreecnt, last_alloc, alccnt);
#endif
    g_tls = NULL;
    list = NULL;
}
