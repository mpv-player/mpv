#ifndef DS_ALLOCATOR_H
#define DS_ALLOCATOR_H

#include "interfaces.h"
#include "cmediasample.h"
#include "iunk.h"

#include <list>

struct MemAllocator: public IMemAllocator
{
    ALLOCATOR_PROPERTIES props;
    std::list<CMediaSample*> used_list;
    std::list<CMediaSample*> free_list;
    char* new_pointer;
    CMediaSample* modified_sample;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(MemAllocator)

    MemAllocator();
    ~MemAllocator();
    void SetPointer(char* pointer) { new_pointer=pointer; }
    void ResetPointer() 
    { 
	if (modified_sample)
	{
	    modified_sample->ResetPointer(); 
	    modified_sample=0;
	}
    }

    static long CreateAllocator(GUID* clsid, GUID* iid, void** ppv);
};

#endif /* DS_ALLOCATOR_H */
