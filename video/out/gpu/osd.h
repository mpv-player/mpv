#ifndef MPLAYER_GL_OSD_H
#define MPLAYER_GL_OSD_H

#include <stdbool.h>
#include <inttypes.h>

#include "utils.h"
#include "shader_cache.h"
#include "sub/osd.h"

struct mpgl_osd *mpgl_osd_init(struct ra *ra, struct mp_log *log,
                               struct osd_state *osd);
void mpgl_osd_destroy(struct mpgl_osd *ctx);

void mpgl_osd_generate(struct mpgl_osd *ctx, struct mp_osd_res res, double pts,
                       int stereo_mode, int draw_flags);
void mpgl_osd_resize(struct mpgl_osd *ctx, struct mp_osd_res res, int stereo_mode);
bool mpgl_osd_draw_prepare(struct mpgl_osd *ctx, int index,
                           struct gl_shader_cache *sc);
void mpgl_osd_draw_finish(struct mpgl_osd *ctx, int index,
                          struct gl_shader_cache *sc, struct ra_fbo fbo);
bool mpgl_osd_check_change(struct mpgl_osd *ctx, struct mp_osd_res *res,
                           double pts);

#endif
