#pragma once

#include "common/common.h"
#include "common/msg.h"
#include "video/csputils.h"

#include <libplacebo/common.h>
#include <libplacebo/context.h>
#include <libplacebo/colorspace.h>

pl_log mppl_log_create(struct mp_log *log);
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

enum pl_color_primaries mp_prim_to_pl(enum mp_csp_prim prim);
enum pl_color_transfer mp_trc_to_pl(enum mp_csp_trc trc);
enum pl_color_system mp_csp_to_pl(enum mp_csp csp);
enum pl_color_levels mp_levels_to_pl(enum mp_csp_levels levels);
enum pl_alpha_mode mp_alpha_to_pl(enum mp_alpha_type alpha);
enum pl_chroma_location mp_chroma_to_pl(enum mp_chroma_location chroma);
