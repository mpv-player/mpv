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

#include "module.h"

#include <dlfcn.h>
#include <stdio.h>

void *mp_load_module(struct mpv_global *global, const char *type,
                     const char *name, const char *entrypoint)
{
    struct mp_log *module_log = mp_log_new(NULL, global->log, "module");
    struct mp_log *log = mp_log_new(module_log, module_log, type);
    char buf[BUFSIZ] = {0};

    if (!type || !name || !entrypoint)
        mp_err(log, "Invalid module %s:%s:%s\n", type, name, entrypoint);

    snprintf(buf, sizeof(buf) - 1, "libmpv-%s-%s.so", type, name);
    mp_dbg(log, "Trying to load module %s:%s from %s\n", type, name, buf);

    dlerror(); // reset error
    void *handle = dlopen(buf, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        mp_verbose(log, "Could not load module %s:%s from %s: %s\n",
                   type, name, buf, dlerror());
        goto error;
    }

    dlerror(); // reset error
    void *sym = dlsym(handle, entrypoint);
    if (!sym) {
        mp_warn(log, "Could not load entrypoint %s from module %s:%s from %s: %s\n",
                entrypoint, type, name, buf, dlerror());
        goto error;
    }

    mp_dbg(log, "Successfully loaded entrypoint %s from module %s:%s from %s: %s\n",
            entrypoint, type, name, buf, dlerror());

    talloc_free(module_log);
    return sym;

error:
    talloc_free(module_log);
    return NULL;
}
