/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "video/out/wayland_common.h"
#include "video/out/gpu/ra.h"
#include "ra_wldmabuf.h"

static void destroy(struct ra *ra)
{
}

static struct ra_fns ra_fns_wldmabuf = {
    .destroy                = destroy,
};

struct ra *ra_create_wayland(struct mp_log *log, struct wl_display *display)
{
    struct ra *ra =  talloc_zero(NULL, struct ra);

    ra->fns = &ra_fns_wldmabuf;
    ra->log = log;
    ra_add_native_resource(ra, "wl", display);

    return ra;
}

bool ra_is_wldmabuf(struct ra *ra)
{
    return (ra->fns == &ra_fns_wldmabuf);
}
