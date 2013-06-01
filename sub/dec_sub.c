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
#include "demux/stheader.h"
#include "sub/sd.h"
#include "sub/sub.h"
#include "sub/dec_sub.h"
#include "core/options.h"

extern const struct sd_functions sd_ass;
extern const struct sd_functions sd_lavc;
extern const struct sd_functions sd_spu;

static const struct sd_functions *sd_list[] = {
#ifdef CONFIG_ASS
    &sd_ass,
#endif
    &sd_lavc,
    &sd_spu,
    NULL
};

void sub_init(struct sh_sub *sh, struct osd_state *osd)
{
    sh->sd_driver = NULL;
    for (int n = 0; sd_list[n]; n++) {
        if (sd_list[n]->supports_format(sh->gsh->codec)) {
            sh->sd_driver = sd_list[n];
            break;
        }
    }

    if (sh->sd_driver) {
        if (sh->sd_driver->init(sh, osd) < 0)
            return;
        osd->sh_sub = sh;
        osd->switch_sub_id++;
        sh->initialized = true;
        sh->active = true;
    }
}

bool sub_accept_packets_in_advance(struct sh_sub *sh)
{
    return sh->active && sh->sd_driver->accept_packets_in_advance;
}

void sub_decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                int data_len, double pts, double duration)
{
    if (sh->active && sh->sd_driver->decode)
        sh->sd_driver->decode(sh, osd, data, data_len, pts, duration);
}

void sub_get_bitmaps(struct osd_state *osd, struct mp_osd_res dim, double pts,
                     struct sub_bitmaps *res)
{
    struct MPOpts *opts = osd->opts;

    *res = (struct sub_bitmaps) {0};
    if (!opts->sub_visibility || !osd->sh_sub || !osd->sh_sub->active) {
        /* Change ID in case we just switched from visible subtitles
         * to current state. Hopefully, unnecessarily claiming that
         * things may have changed is harmless for empty contents.
         * Increase osd-> values ahead so that _next_ returned id
         * is also guaranteed to differ from this one.
         */
        osd->switch_sub_id++;
    } else {
        if (osd->sh_sub->sd_driver->get_bitmaps)
            osd->sh_sub->sd_driver->get_bitmaps(osd->sh_sub, osd, dim, pts, res);
    }

    res->bitmap_id += osd->switch_sub_id;
    res->bitmap_pos_id += osd->switch_sub_id;
    osd->switch_sub_id = 0;
}

char *sub_get_text(struct osd_state *osd, double pts)
{
    struct MPOpts *opts = osd->opts;
    char *text = NULL;
    if (!opts->sub_visibility || !osd->sh_sub || !osd->sh_sub->active) {
        // -
    } else {
        if (osd->sh_sub->sd_driver->get_text)
            text = osd->sh_sub->sd_driver->get_text(osd->sh_sub, osd, pts);
    }
    return text;
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
