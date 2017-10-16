#include "malloc.h"
#include "utils.h"
#include "osdep/timer.h"

// Controls the multiplication factor for new slab allocations. The new slab
// will always be allocated such that the size of the slab is this factor times
// the previous slab. Higher values make it grow faster.
#define MPVK_HEAP_SLAB_GROWTH_RATE 4

// Controls the minimum slab size, to reduce the frequency at which very small
// slabs would need to get allocated when allocating the first few buffers.
// (Default: 1 MB)
#define MPVK_HEAP_MINIMUM_SLAB_SIZE (1 << 20)

// Controls the maximum slab size, to reduce the effect of unbounded slab
// growth exhausting memory. If the application needs a single allocation
// that's bigger than this value, it will be allocated directly from the
// device. (Default: 512 MB)
#define MPVK_HEAP_MAXIMUM_SLAB_SIZE (1 << 29)

// Controls the minimum free region size, to reduce thrashing the free space
// map with lots of small buffers during uninit. (Default: 1 KB)
#define MPVK_HEAP_MINIMUM_REGION_SIZE (1 << 10)

// Represents a region of available memory
struct vk_region {
    size_t start; // first offset in region
    size_t end;   // first offset *not* in region
};

static inline size_t region_len(struct vk_region r)
{
    return r.end - r.start;
}

// A single slab represents a contiguous region of allocated memory. Actual
// allocations are served as slices of this. Slabs are organized into linked
// lists, which represent individual heaps.
struct vk_slab {
    VkDeviceMemory mem;   // underlying device allocation
    size_t size;          // total size of `slab`
    size_t used;          // number of bytes actually in use (for GC accounting)
    bool dedicated;       // slab is allocated specifically for one object
    // free space map: a sorted list of memory regions that are available
    struct vk_region *regions;
    int num_regions;
    // optional, depends on the memory type:
    VkBuffer buffer;      // buffer spanning the entire slab
    void *data;           // mapped memory corresponding to `mem`
};

// Represents a single memory heap. We keep track of a vk_heap for each
// combination of buffer type and memory selection parameters. This shouldn't
// actually be that many in practice, because some combinations simply never
// occur, and others will generally be the same for the same objects.
struct vk_heap {
    VkBufferUsageFlags usage;    // the buffer usage type (or 0)
    VkMemoryPropertyFlags flags; // the memory type flags (or 0)
    uint32_t typeBits;           // the memory type index requirements (or 0)
    struct vk_slab **slabs;      // array of slabs sorted by size
    int num_slabs;
};

// The overall state of the allocator, which keeps track of a vk_heap for each
// memory type.
struct vk_malloc {
    VkPhysicalDeviceMemoryProperties props;
    struct vk_heap *heaps;
    int num_heaps;
};

static void slab_free(struct mpvk_ctx *vk, struct vk_slab *slab)
{
    if (!slab)
        return;

    assert(slab->used == 0);

    int64_t start = mp_time_us();
    vkDestroyBuffer(vk->dev, slab->buffer, MPVK_ALLOCATOR);
    // also implicitly unmaps the memory if needed
    vkFreeMemory(vk->dev, slab->mem, MPVK_ALLOCATOR);
    int64_t stop = mp_time_us();

    MP_VERBOSE(vk, "Freeing slab of size %zu took %lld μs.\n",
               slab->size, (long long)(stop - start));

    talloc_free(slab);
}

static bool find_best_memtype(struct mpvk_ctx *vk, uint32_t typeBits,
                              VkMemoryPropertyFlags flags,
                              VkMemoryType *out_type, int *out_index)
{
    struct vk_malloc *ma = vk->alloc;

    // The vulkan spec requires memory types to be sorted in the "optimal"
    // order, so the first matching type we find will be the best/fastest one.
    for (int i = 0; i < ma->props.memoryTypeCount; i++) {
        // The memory type flags must include our properties
        if ((ma->props.memoryTypes[i].propertyFlags & flags) != flags)
            continue;
        // The memory type must be supported by the requirements (bitfield)
        if (typeBits && !(typeBits & (1 << i)))
            continue;
        *out_type = ma->props.memoryTypes[i];
        *out_index = i;
        return true;
    }

    MP_ERR(vk, "Found no memory type matching property flags 0x%x and type "
               "bits 0x%x!\n", (unsigned)flags, (unsigned)typeBits);
    return false;
}

static struct vk_slab *slab_alloc(struct mpvk_ctx *vk, struct vk_heap *heap,
                                  size_t size)
{
    struct vk_slab *slab = talloc_ptrtype(NULL, slab);
    *slab = (struct vk_slab) {
        .size = size,
    };

    MP_TARRAY_APPEND(slab, slab->regions, slab->num_regions, (struct vk_region) {
        .start = 0,
        .end   = slab->size,
    });

    VkMemoryAllocateInfo minfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = slab->size,
    };

    uint32_t typeBits = heap->typeBits ? heap->typeBits : UINT32_MAX;
    if (heap->usage) {
        // FIXME: Since we can't keep track of queue family ownership properly,
        // and we don't know in advance what types of queue families this buffer
        // will belong to, we're forced to share all of our buffers between all
        // command pools.
        uint32_t qfs[3] = {0};
        for (int i = 0; i < vk->num_pools; i++)
            qfs[i] = vk->pools[i]->qf;

        VkBufferCreateInfo binfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size  = slab->size,
            .usage = heap->usage,
            .sharingMode = vk->num_pools > 1 ? VK_SHARING_MODE_CONCURRENT
                                             : VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = vk->num_pools,
            .pQueueFamilyIndices = qfs,
        };

        VK(vkCreateBuffer(vk->dev, &binfo, MPVK_ALLOCATOR, &slab->buffer));

        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(vk->dev, slab->buffer, &reqs);
        minfo.allocationSize = reqs.size; // this can be larger than slab->size
        typeBits &= reqs.memoryTypeBits;  // this can restrict the types
    }

    VkMemoryType type;
    int index;
    if (!find_best_memtype(vk, typeBits, heap->flags, &type, &index))
        goto error;

    MP_VERBOSE(vk, "Allocating %zu memory of type 0x%x (id %d) in heap %d.\n",
               slab->size, (unsigned)type.propertyFlags, index, (int)type.heapIndex);

    minfo.memoryTypeIndex = index;
    VK(vkAllocateMemory(vk->dev, &minfo, MPVK_ALLOCATOR, &slab->mem));

    if (heap->flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        VK(vkMapMemory(vk->dev, slab->mem, 0, VK_WHOLE_SIZE, 0, &slab->data));

    if (slab->buffer)
        VK(vkBindBufferMemory(vk->dev, slab->buffer, slab->mem, 0));

    return slab;

error:
    slab_free(vk, slab);
    return NULL;
}

static void insert_region(struct vk_slab *slab, struct vk_region region)
{
    if (region.start == region.end)
        return;

    bool big_enough = region_len(region) >= MPVK_HEAP_MINIMUM_REGION_SIZE;

    // Find the index of the first region that comes after this
    for (int i = 0; i < slab->num_regions; i++) {
        struct vk_region *r = &slab->regions[i];

        // Check for a few special cases which can be coalesced
        if (r->end == region.start) {
            // The new region is at the tail of this region. In addition to
            // modifying this region, we also need to coalesce all the following
            // regions for as long as possible
            r->end = region.end;

            struct vk_region *next = &slab->regions[i+1];
            while (i+1 < slab->num_regions && r->end == next->start) {
                r->end = next->end;
                MP_TARRAY_REMOVE_AT(slab->regions, slab->num_regions, i+1);
            }
            return;
        }

        if (r->start == region.end) {
            // The new region is at the head of this region. We don't need to
            // do anything special here - because if this could be further
            // coalesced backwards, the previous loop iteration would already
            // have caught it.
            r->start = region.start;
            return;
        }

        if (r->start > region.start) {
            // The new region comes somewhere before this region, so insert
            // it into this index in the array.
            if (big_enough) {
                MP_TARRAY_INSERT_AT(slab, slab->regions, slab->num_regions,
                                    i, region);
            }
            return;
        }
    }

    // If we've reached the end of this loop, then all of the regions
    // come before the new region, and are disconnected - so append it
    if (big_enough)
        MP_TARRAY_APPEND(slab, slab->regions, slab->num_regions, region);
}

static void heap_uninit(struct mpvk_ctx *vk, struct vk_heap *heap)
{
    for (int i = 0; i < heap->num_slabs; i++)
        slab_free(vk, heap->slabs[i]);

    talloc_free(heap->slabs);
    *heap = (struct vk_heap){0};
}

void vk_malloc_init(struct mpvk_ctx *vk)
{
    assert(vk->physd);
    vk->alloc = talloc_zero(NULL, struct vk_malloc);
    vkGetPhysicalDeviceMemoryProperties(vk->physd, &vk->alloc->props);
}

void vk_malloc_uninit(struct mpvk_ctx *vk)
{
    struct vk_malloc *ma = vk->alloc;
    if (!ma)
        return;

    for (int i = 0; i < ma->num_heaps; i++)
        heap_uninit(vk, &ma->heaps[i]);

    talloc_free(ma);
    vk->alloc = NULL;
}

void vk_free_memslice(struct mpvk_ctx *vk, struct vk_memslice slice)
{
    struct vk_slab *slab = slice.priv;
    if (!slab)
        return;

    assert(slab->used >= slice.size);
    slab->used -= slice.size;

    MP_DBG(vk, "Freeing slice %zu + %zu from slab with size %zu\n",
           slice.offset, slice.size, slab->size);

    if (slab->dedicated) {
        // If the slab was purpose-allocated for this memslice, we can just
        // free it here
        slab_free(vk, slab);
    } else {
        // Return the allocation to the free space map
        insert_region(slab, (struct vk_region) {
            .start = slice.offset,
            .end   = slice.offset + slice.size,
        });
    }
}

// reqs: can be NULL
static struct vk_heap *find_heap(struct mpvk_ctx *vk, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags flags,
                                 VkMemoryRequirements *reqs)
{
    struct vk_malloc *ma = vk->alloc;
    int typeBits = reqs ? reqs->memoryTypeBits : 0;

    for (int i = 0; i < ma->num_heaps; i++) {
        if (ma->heaps[i].usage != usage)
            continue;
        if (ma->heaps[i].flags != flags)
            continue;
        if (ma->heaps[i].typeBits != typeBits)
            continue;
        return &ma->heaps[i];
    }

    // Not found => add it
    MP_TARRAY_GROW(ma, ma->heaps, ma->num_heaps + 1);
    struct vk_heap *heap = &ma->heaps[ma->num_heaps++];
    *heap = (struct vk_heap) {
        .usage    = usage,
        .flags    = flags,
        .typeBits = typeBits,
    };
    return heap;
}

static inline bool region_fits(struct vk_region r, size_t size, size_t align)
{
    return MP_ALIGN_UP(r.start, align) + size <= r.end;
}

// Finds the best-fitting region in a heap. If the heap is too small or too
// fragmented, a new slab will be allocated under the hood.
static bool heap_get_region(struct mpvk_ctx *vk, struct vk_heap *heap,
                            size_t size, size_t align,
                            struct vk_slab **out_slab, int *out_index)
{
    struct vk_slab *slab = NULL;

    // If the allocation is very big, serve it directly instead of bothering
    // with the heap
    if (size > MPVK_HEAP_MAXIMUM_SLAB_SIZE) {
        slab = slab_alloc(vk, heap, size);
        *out_slab = slab;
        *out_index = 0;
        return !!slab;
    }

    for (int i = 0; i < heap->num_slabs; i++) {
        slab = heap->slabs[i];
        if (slab->size < size)
            continue;

        // Attempt a best fit search
        int best = -1;
        for (int n = 0; n < slab->num_regions; n++) {
            struct vk_region r = slab->regions[n];
            if (!region_fits(r, size, align))
                continue;
            if (best >= 0 && region_len(r) > region_len(slab->regions[best]))
                continue;
            best = n;
        }

        if (best >= 0) {
            *out_slab = slab;
            *out_index = best;
            return true;
        }
    }

    // Otherwise, allocate a new vk_slab and append it to the list.
    size_t cur_size = MPMAX(size, slab ? slab->size : 0);
    size_t slab_size = MPVK_HEAP_SLAB_GROWTH_RATE * cur_size;
    slab_size = MPMAX(MPVK_HEAP_MINIMUM_SLAB_SIZE, slab_size);
    slab_size = MPMIN(MPVK_HEAP_MAXIMUM_SLAB_SIZE, slab_size);
    assert(slab_size >= size);
    slab = slab_alloc(vk, heap, slab_size);
    if (!slab)
        return false;
    MP_TARRAY_APPEND(NULL, heap->slabs, heap->num_slabs, slab);

    // Return the only region there is in a newly allocated slab
    assert(slab->num_regions == 1);
    *out_slab = slab;
    *out_index = 0;
    return true;
}

static bool slice_heap(struct mpvk_ctx *vk, struct vk_heap *heap, size_t size,
                       size_t alignment, struct vk_memslice *out)
{
    struct vk_slab *slab;
    int index;
    alignment = MP_ALIGN_UP(alignment, vk->limits.bufferImageGranularity);
    if (!heap_get_region(vk, heap, size, alignment, &slab, &index))
        return false;

    struct vk_region reg = slab->regions[index];
    MP_TARRAY_REMOVE_AT(slab->regions, slab->num_regions, index);
    *out = (struct vk_memslice) {
        .vkmem = slab->mem,
        .offset = MP_ALIGN_UP(reg.start, alignment),
        .size = size,
        .priv = slab,
    };

    MP_DBG(vk, "Sub-allocating slice %zu + %zu from slab with size %zu\n",
           out->offset, out->size, slab->size);

    size_t out_end = out->offset + out->size;
    insert_region(slab, (struct vk_region) { reg.start, out->offset });
    insert_region(slab, (struct vk_region) { out_end, reg.end });

    slab->used += size;
    return true;
}

bool vk_malloc_generic(struct mpvk_ctx *vk, VkMemoryRequirements reqs,
                       VkMemoryPropertyFlags flags, struct vk_memslice *out)
{
    struct vk_heap *heap = find_heap(vk, 0, flags, &reqs);
    return slice_heap(vk, heap, reqs.size, reqs.alignment, out);
}

bool vk_malloc_buffer(struct mpvk_ctx *vk, VkBufferUsageFlags bufFlags,
                      VkMemoryPropertyFlags memFlags, VkDeviceSize size,
                      VkDeviceSize alignment, struct vk_bufslice *out)
{
    struct vk_heap *heap = find_heap(vk, bufFlags, memFlags, NULL);
    if (!slice_heap(vk, heap, size, alignment, &out->mem))
        return false;

    struct vk_slab *slab = out->mem.priv;
    out->buf = slab->buffer;
    if (slab->data)
        out->data = (void *)((uintptr_t)slab->data + (ptrdiff_t)out->mem.offset);

    return true;
}
