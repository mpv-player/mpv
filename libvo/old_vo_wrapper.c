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


#include <stdint.h>
#include "old_vo_wrapper.h"
#include "video_out.h"
#include "sub/sub.h"

struct vo *global_vo;
struct osd_state *global_osd;

int old_vo_preinit(struct vo *vo, const char *arg)
{
    global_vo = vo;
    return vo->driver->old_functions->preinit(arg);
}


int old_vo_config(struct vo *vo, uint32_t width, uint32_t height,
                         uint32_t d_width, uint32_t d_height,
                         uint32_t flags, uint32_t format)
{
    return vo->driver->old_functions->config(width, height, d_width,
                                             d_height, flags, "MPlayer",
                                             format);
}


int old_vo_control(struct vo *vo, uint32_t request, void *data)
{
    return vo->driver->old_functions->control(request, data);
}


int old_vo_draw_frame(struct vo *vo, uint8_t *src[])
{
    return vo->driver->old_functions->draw_frame(src);
}


int old_vo_draw_slice(struct vo *vo, uint8_t *src[], int stride[],
                             int w, int h, int x, int y)
{
    return vo->driver->old_functions->draw_slice(src, stride, w, h, x, y);
}


void old_vo_draw_osd(struct vo *vo, struct osd_state *osd)
{
    global_osd = osd;
    vo->driver->old_functions->draw_osd();
}


void old_vo_flip_page(struct vo *vo)
{
    vo->driver->old_functions->flip_page();
}


void old_vo_check_events(struct vo *vo)
{
    vo->driver->old_functions->check_events();
}


void old_vo_uninit(struct vo *vo)
{
    vo->driver->old_functions->uninit();
}


static void draw_alpha_wrapper(void *ctx, int x0, int y0, int w, int h,
                               unsigned char *src, unsigned char *srca,
                               int stride)
{
    void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride) = ctx;
    draw_alpha(x0, y0, w, h, src, srca, stride);
}


void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride))
{
    osd_draw_text(global_osd, dxs, dys, draw_alpha_wrapper, draw_alpha);
}

void vo_draw_text_ext(int dxs, int dys, int left_border, int top_border,
                      int right_border, int bottom_border, int orig_w, int orig_h,
                      void (*draw_alpha)(int x0, int y0, int w,int h,
                                         unsigned char* src,
                                         unsigned char *srca, int stride))
{
    osd_draw_text_ext(global_osd, dxs, dys, left_border, top_border,
                      right_border, bottom_border, orig_w, orig_h,
                      draw_alpha_wrapper, draw_alpha);
}

int vo_update_osd(int dxs, int dys)
{
    return osd_update(global_osd, dxs, dys);
}
