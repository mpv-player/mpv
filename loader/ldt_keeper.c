/**
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * This file MUST be in main library because LDT must
 * be modified before program creates first thread
 * - avifile includes this file from C++ code
 * and initializes it at the start of player!
 * it might sound like a hack and it really is - but
 * as aviplay is deconding video with more than just one
 * thread currently it's necessary to do it this way
 * this might change in the future
 */

/* applied some modification to make make our xine friend more happy */
#include "ldt_keeper.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#ifdef __linux__
#include <asm/unistd.h>
#include <asm/ldt.h>
/* prototype it here, so we won't depend on kernel headers */
#ifdef  __cplusplus
extern "C" {
#endif
int modify_ldt(int func, void *ptr, unsigned long bytecount);
#ifdef  __cplusplus
}
#endif
#else
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <machine/segments.h>
#include <machine/sysarch.h>
#endif

#ifdef __svr4__
#include <sys/segment.h>
#include <sys/sysi86.h>

/* solaris x86: add missing prototype for sysi86() */
#ifdef  __cplusplus
extern "C" {
#endif
int sysi86(int, void*);
#ifdef  __cplusplus
}
#endif

#ifndef NUMSYSLDTS             /* SunOS 2.5.1 does not define NUMSYSLDTS */
#define NUMSYSLDTS     6       /* Let's hope the SunOS 5.8 value is OK */
#endif

#define       TEB_SEL_IDX     NUMSYSLDTS
#endif

#define LDT_ENTRIES     8192
#define LDT_ENTRY_SIZE  8
#pragma pack(4)
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
#endif


/* user level (privilege level: 3) ldt (1<<2) segment selector */
#define       LDT_SEL(idx) ((idx) << 3 | 1 << 2 | 3)

/* i got this value from wine sources, it's the first free LDT entry */
#ifndef       TEB_SEL_IDX
#define       TEB_SEL_IDX     17
#endif

#define       TEB_SEL LDT_SEL(TEB_SEL_IDX)

/**
 * here is a small logical problem with Restore for multithreaded programs -
 * in C++ we use static class for this...
 */

#ifdef __cplusplus
extern "C"
#endif
void Setup_FS_Segment(void)
{
    __asm__ __volatile__(
	"movl %0,%%eax; movw %%ax, %%fs" : : "i" (TEB_SEL)
    );
}

/* we don't need this - use modify_ldt instead */
#if 0
#ifdef __linux__
/* XXX: why is this routine from libc redefined here? */
/* NOTE: the redefined version ignores the count param, count is hardcoded as 16 */
static int LDT_Modify( int func, struct modify_ldt_ldt_s *ptr,
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
			  "d"(16)//sizeof(*ptr) from kernel point of view
			  :"esi"     );
#else
    __asm__ __volatile__("int $0x80"
			 : "=a" (res)
			 : "0" (__NR_modify_ldt),
			 "b" (func),
			 "c" (ptr),
			 "d"(16)
			 :"esi");
#endif  /* __PIC__ */
    if (res >= 0) return res;
    errno = -res;
    return -1;
}
#endif
#endif

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
static void LDT_EntryToBytes( unsigned long *buffer, const struct modify_ldt_ldt_s *content )
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
#endif

//void* fs_seg=0;

ldt_fs_t* Setup_LDT_Keeper(void)
{
    struct modify_ldt_ldt_s array;
    int ret;
    ldt_fs_t* ldt_fs = (ldt_fs_t*) malloc(sizeof(ldt_fs_t));

    if (!ldt_fs)
	return NULL;

    ldt_fs->fd = open("/dev/zero", O_RDWR);
    if(ldt_fs->fd<0){
        perror( "Cannot open /dev/zero for READ+WRITE. Check permissions! error: ");
	return NULL;
    }
//    fs_seg=
    ldt_fs->fs_seg = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE, MAP_PRIVATE,
			  ldt_fs->fd, 0);
    if (ldt_fs->fs_seg == (void*)-1)
    {
	perror("ERROR: Couldn't allocate memory for fs segment");
        close(ldt_fs->fd);
        free(ldt_fs);
	return NULL;
    }
    *(void**)((char*)ldt_fs->fs_seg+0x18) = ldt_fs->fs_seg;
    array.base_addr=(int)ldt_fs->fs_seg;
    array.entry_number=TEB_SEL_IDX;
    array.limit=array.base_addr+getpagesize()-1;
    array.seg_32bit=1;
    array.read_exec_only=0;
    array.seg_not_present=0;
    array.contents=MODIFY_LDT_CONTENTS_DATA;
    array.limit_in_pages=0;
#ifdef __linux__
    //ret=LDT_Modify(0x1, &array, sizeof(struct modify_ldt_ldt_s));
    ret=modify_ldt(0x1, &array, sizeof(struct modify_ldt_ldt_s));
    if(ret<0)
    {
	perror("install_fs");
	printf("Couldn't install fs segment, expect segfault\n");
    }
#endif /*linux*/

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    {
        unsigned long d[2];

        LDT_EntryToBytes( d, &array );
        ret = i386_set_ldt(array.entry_number, (union descriptor *)d, 1);
        if (ret < 0)
        {
            perror("install_fs");
	    printf("Couldn't install fs segment, expect segfault\n");
            printf("Did you reconfigure the kernel with \"options USER_LDT\"?\n");
        }
    }
#endif  /* __NetBSD__ || __FreeBSD__ || __OpenBSD__ */

#if defined(__svr4__)
    {
	struct ssd ssd;
	ssd.sel = TEB_SEL;
	ssd.bo = array.base_addr;
	ssd.ls = array.limit - array.base_addr;
	ssd.acc1 = ((array.read_exec_only == 0) << 1) |
	    (array.contents << 2) |
	    0xf0;   /* P(resent) | DPL3 | S */
	ssd.acc2 = 0x4;   /* byte limit, 32-bit segment */
	if (sysi86(SI86DSCR, &ssd) < 0) {
	    perror("sysi86(SI86DSCR)");
	    printf("Couldn't install fs segment, expect segfault\n");
	}
    }
#endif

    Setup_FS_Segment();

    ldt_fs->prev_struct = (char*)malloc(sizeof(char) * 8);
    *(void**)array.base_addr = ldt_fs->prev_struct;

    return ldt_fs;
}

void Restore_LDT_Keeper(ldt_fs_t* ldt_fs)
{
    if (ldt_fs == NULL || ldt_fs->fs_seg == 0)
	return;
    if (ldt_fs->prev_struct)
	free(ldt_fs->prev_struct);
    munmap((char*)ldt_fs->fs_seg, getpagesize());
    ldt_fs->fs_seg = 0;
    close(ldt_fs->fd);
    free(ldt_fs);
}
