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

#ifndef PY_MPV_EXTENSION_H
#define PY_MPV_EXTENSION_H
#include <Python.h>


typedef struct {
    PyObject_HEAD

    const char          *filename;
    const char          *path;
    struct MPContext    *mpctx;
    struct mpv_handle   *client;
    struct mp_log       *log;
    PyThreadState       *threadState;
    void                *ta_ctx;
} PyClientCtx;


// PyObject *PyInit_mpv(void);
PyMODINIT_FUNC PyInit_mpv(void);

#endif
