/*
 * LDT copy
 *
 * Copyright 1995 Alexandre Julliard
 */

#ifndef __WINE_LDT_H
#define __WINE_LDT_H

#include "windef.h"
enum seg_type
{
    SEGMENT_DATA  = 0,
    SEGMENT_STACK = 1,
    SEGMENT_CODE  = 2
};

  /* This structure represents a real LDT entry.        */
  /* It is used by get_ldt_entry() and set_ldt_entry(). */
typedef struct
{
    unsigned long base;            /* base address */
    unsigned long limit;           /* segment limit (in pages or bytes) */
    int           seg_32bit;       /* is segment 32-bit? */
    int           read_only;       /* is segment read-only? */
    int           limit_in_pages;  /* is the limit in pages or bytes? */
    enum seg_type type;            /* segment type */
} ldt_entry;
#ifdef __cplusplus
extern "C" 
{
#endif
extern void LDT_BytesToEntry( const unsigned long *buffer, ldt_entry *content);
extern void LDT_EntryToBytes( unsigned long *buffer, const ldt_entry *content);
extern int LDT_GetEntry( int entry, ldt_entry *content );
extern int LDT_SetEntry( int entry, const ldt_entry *content );
extern void LDT_Print( int start, int length );


  /* This structure is used to build the local copy of the LDT. */
typedef struct
{
    unsigned long base;    /* base address or 0 if entry is free   */
    unsigned long limit;   /* limit in bytes or 0 if entry is free */
} ldt_copy_entry;

#define LDT_SIZE  8192

extern ldt_copy_entry ldt_copy[LDT_SIZE];

#define __AHSHIFT  3  /* don't change! */
#define __AHINCR   (1 << __AHSHIFT)

#define SELECTOR_TO_ENTRY(sel)  (((int)(sel) & 0xffff) >> __AHSHIFT)
#define ENTRY_TO_SELECTOR(i)    ((i) ? (((int)(i) << __AHSHIFT) | 7) : 0)
#define IS_LDT_ENTRY_FREE(i)    (!(ldt_flags_copy[(i)] & LDT_FLAGS_ALLOCATED))
#define IS_SELECTOR_FREE(sel)   (IS_LDT_ENTRY_FREE(SELECTOR_TO_ENTRY(sel)))
#define GET_SEL_BASE(sel)       (ldt_copy[SELECTOR_TO_ENTRY(sel)].base)
#define GET_SEL_LIMIT(sel)      (ldt_copy[SELECTOR_TO_ENTRY(sel)].limit)

/* Convert a segmented ptr (16:16) to a linear (32) pointer */

#define PTR_SEG_OFF_TO_LIN(seg,off) \
   ((void*)(GET_SEL_BASE(seg) + (unsigned int)(off)))
#define PTR_SEG_TO_LIN(ptr) \
   PTR_SEG_OFF_TO_LIN(SELECTOROF(ptr),OFFSETOF(ptr))
#define PTR_SEG_OFF_TO_SEGPTR(seg,off) \
   ((SEGPTR)MAKELONG(off,seg))
#define PTR_SEG_OFF_TO_HUGEPTR(seg,off) \
   PTR_SEG_OFF_TO_SEGPTR( (seg) + (HIWORD(off) << __AHSHIFT), LOWORD(off) )

#define W32S_APPLICATION() (PROCESS_Current()->flags & PDB32_WIN32S_PROC)
#define W32S_OFFSET 0x10000
#define W32S_APP2WINE(addr, offset) ((addr)? (DWORD)(addr) + (DWORD)(offset) : 0)
#define W32S_WINE2APP(addr, offset) ((addr)? (DWORD)(addr) - (DWORD)(offset) : 0)

extern unsigned char ldt_flags_copy[LDT_SIZE];

#define LDT_FLAGS_TYPE      0x03  /* Mask for segment type */
#define LDT_FLAGS_READONLY  0x04  /* Segment is read-only (data) */
#define LDT_FLAGS_EXECONLY  0x04  /* Segment is execute-only (code) */
#define LDT_FLAGS_32BIT     0x08  /* Segment is 32-bit (code or stack) */
#define LDT_FLAGS_BIG       0x10  /* Segment is big (limit is in pages) */
#define LDT_FLAGS_ALLOCATED 0x80  /* Segment is allocated (no longer free) */

#define GET_SEL_FLAGS(sel)   (ldt_flags_copy[SELECTOR_TO_ENTRY(sel)])

#define FIRST_LDT_ENTRY_TO_ALLOC  17

/* Determine if sel is a system selector (i.e. not managed by Wine) */
#define IS_SELECTOR_SYSTEM(sel) \
   (!((sel) & 4) || (SELECTOR_TO_ENTRY(sel) < FIRST_LDT_ENTRY_TO_ALLOC))
#define IS_SELECTOR_32BIT(sel) \
   (IS_SELECTOR_SYSTEM(sel) || (GET_SEL_FLAGS(sel) & LDT_FLAGS_32BIT))
#ifdef __cplusplus
}
#endif
#endif  /* __WINE_LDT_H */
