#include <assert.h>
#include <string.h>

#include "filterutils.h"

void copy_plane(
    unsigned char *dest, unsigned dest_stride,
    const unsigned char *src, unsigned src_stride,
    unsigned length,
    unsigned rows
    )
{
    unsigned i;
    assert(dest_stride >= length);
    assert(src_stride >= length);
    for (i = 0; i < rows; ++i)
        memcpy(&dest[dest_stride * i], &src[src_stride * i], length);
}

