#pragma once

#include "video/out/gpu/ra.h"
#include <libplacebo/gpu.h>

struct ra *ra_create_pl(const struct pl_gpu *gpu, struct mp_log *log);

const struct pl_gpu *ra_pl_get(const struct ra *ra);

static inline const struct pl_fmt *ra_pl_fmt_get(const struct ra_format *format)
{
    return format->priv;
}

// Wrap a pl_tex into a ra_tex struct, returns if successful
bool mppl_wrap_tex(struct ra *ra, const struct pl_tex *pltex,
                   struct ra_tex *out_tex);
