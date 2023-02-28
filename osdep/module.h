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

#ifndef MP_MODULE_H
#define MP_MODULE_H

#include "config.h"

#include <stddef.h>

#include "common/msg.h"
#include "common/global.h"
#include "mpv_talloc.h"

struct mp_module {
    const void * driver;
    const char * const module;
    const char * const entrypoint;
};

#define MP_MODULE_BUILTIN(d) { .driver = &d, }

#if HAVE_MODULAR_DRIVERS

#define MP_MODULE_MODULAR(m, symbol) { .module = m, .entrypoint = #symbol }

void *mp_load_module(struct mpv_global *global, const char *type,
                     const char *name, const char *entrypoint);

#else

#define MP_MODULE_MODULAR(m, symbol) MP_MODULE_BUILTIN(symbol)

static inline void *mp_load_module(struct mpv_global *global, const char *type,
                                   const char *name, const char *entrypoint)
{
    struct mp_log *log = mp_log_new(NULL, global->log, "module");
    mp_warn(log,
            "Trying to load module %s:%s while modules are not enabled\n",
            type, name);
    talloc_free(log);
    return NULL;
}

#endif

#endif
