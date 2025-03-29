#pragma once

#include "filter.h"

// A filter which inserts the required deinterlacing filter based on the
// hardware decode mode and the deinterlace user option.
struct mp_filter *mp_deint_create(struct mp_filter *parent);

// Flip according to mp_image.vflip
struct mp_filter *mp_autovflip_create(struct mp_filter *parent);

// Rotate according to mp_image.rotate and VO capabilities.
struct mp_filter *mp_autorotate_create(struct mp_filter *parent);

// Insert a filter that inserts scaletempo2 depending on speed settings.
struct mp_filter *mp_autoaspeed_create(struct mp_filter *parent);

bool mp_deint_active(struct mp_filter *parent);
