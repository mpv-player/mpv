#pragma once

#include "config.h"
#include "common/common.h"
#include "common/msg.h"
#include "video/csputils.h"
#include "video/mp_image.h"

#include <libavutil/buffer.h>

#include <libplacebo/common.h>
#include <libplacebo/log.h>
#include <libplacebo/colorspace.h>
#include <libplacebo/renderer.h>
#include <libplacebo/utils/libav.h>

pl_log mppl_log_create(void *tactx, struct mp_log *log);
pl_log mppl_log_create2(void *tactx, struct mp_log *log, const char *name);
void mppl_log_set_probing(pl_log log, bool probing);

static inline struct pl_rect2d mp_rect2d_to_pl(struct mp_rect rc)
{
    return (struct pl_rect2d) {
        .x0 = rc.x0,
        .y0 = rc.y0,
        .x1 = rc.x1,
        .y1 = rc.y1,
    };
}
