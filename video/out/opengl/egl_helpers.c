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

#include "egl_helpers.h"
#include "common.h"

void mp_egl_get_depth(struct GL *gl, EGLConfig fbc)
{
    EGLint tokens[] = {EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE};
    int *ptrs[] =     {&gl->fb_r,    &gl->fb_g,      &gl->fb_b};
    for (int n = 0; n < MP_ARRAY_SIZE(tokens); n++) {
        EGLint depth = 0;
        if (eglGetConfigAttrib(eglGetCurrentDisplay(), fbc, tokens[n], &depth))
            *ptrs[n] = depth;
    }
}
