/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "config.h"
#include "libmpdemux/stheader.h"
#include "sub/sd.h"
#include "sub/sub.h"
#include "sub/dec_sub.h"
#include "options.h"

extern const struct sd_functions sd_ass;
extern const struct sd_functions sd_lavc;

void sub_init(struct sh_sub *sh, struct osd_state *osd)
{
    struct MPOpts *opts = sh->opts;

    assert(!osd->sh_sub);
#ifdef CONFIG_ASS
    if (opts->ass_enabled && is_text_sub(sh->type))
        sh->sd_driver = &sd_ass;
#endif
    if (strchr("bpx", sh->type))
        sh->sd_driver = &sd_lavc;
    if (sh->sd_driver) {
        if (sh->sd_driver->init(sh, osd) < 0)
            return;
        osd->sh_sub = sh;
        osd->changed_outside_sd = true;
        sh->initialized = true;
        sh->active = true;
    }
}

void sub_decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                int data_len, double pts, double duration)
{
    if (sh->active && sh->sd_driver->decode)
        sh->sd_driver->decode(sh, osd, data, data_len, pts, duration);
}

void sub_get_bitmaps(struct osd_state *osd, struct sub_bitmaps *res)
{
    struct MPOpts *opts = osd->opts;

    *res = (struct sub_bitmaps){.imgs = NULL, .changed = 2};
    if (!opts->sub_visibility || !osd->sh_sub || !osd->sh_sub->active) {
        osd->changed_outside_sd = true;
        return;
    }
    if (osd->sh_sub->sd_driver->get_bitmaps)
        osd->sh_sub->sd_driver->get_bitmaps(osd->sh_sub, osd, res);
    if (osd->changed_outside_sd)
        res->changed = 2;
    osd->changed_outside_sd = false;
}

void sub_reset(struct sh_sub *sh, struct osd_state *osd)
{
    if (sh->active && sh->sd_driver->reset)
        sh->sd_driver->reset(sh, osd);
}

void sub_switchoff(struct sh_sub *sh, struct osd_state *osd)
{
    if (sh->active && sh->sd_driver->switch_off) {
        assert(osd->sh_sub == sh);
        sh->sd_driver->switch_off(sh, osd);
        osd->sh_sub = NULL;
    }
    sh->active = false;
}

void sub_uninit(struct sh_sub *sh)
{
    assert (!sh->active);
    if (sh->initialized && sh->sd_driver->uninit)
        sh->sd_driver->uninit(sh);
    sh->initialized = false;
}
