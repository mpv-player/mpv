#ifndef DS_ALLOCATOR_H
#define DS_ALLOCATOR_H

#include "interfaces.h"
#include "cmediasample.h"

typedef struct _avm_list_t avm_list_t;
typedef struct _MemAllocator MemAllocator;

struct _MemAllocator
{
    IMemAllocator_vt* vt;
    DECLARE_IUNKNOWN();
    ALLOCATOR_PROPERTIES props;
    avm_list_t* used_list;
    avm_list_t* free_list;
    char* new_pointer;
    CMediaSample* modified_sample;
    GUID interfaces[2];

    void ( *SetPointer )(MemAllocator* This, char* pointer);
    void ( *ResetPointer )(MemAllocator* This);
};

MemAllocator* MemAllocatorCreate(void);

#endif /* DS_ALLOCATOR_H */
