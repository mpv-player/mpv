#pragma once

#include "filter.h"

// A filter which inserts the required deinterlacing filter based on the
// hardware decode mode and the deinterlace user option.
struct mp_filter *mp_deint_create(struct mp_filter *parent);

// Rotate according to mp_image.rotate and VO capabilities.
struct mp_filter *mp_autorotate_create(struct mp_filter *parent);

// Insert a filter that inserts scaletempo depending on speed settings.
struct mp_filter *mp_autoaspeed_create(struct mp_filter *parent);
