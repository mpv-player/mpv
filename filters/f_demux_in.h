#pragma once

#include "filter.h"

struct sh_stream;

// Create a filter with a single output for the given stream. The stream must
// be selected, and remain so until the filter is destroyed. The filter will
// set/unset the stream's wakeup callback.
struct mp_filter *mp_demux_in_create(struct mp_filter *parent,
                                     struct sh_stream *src);
