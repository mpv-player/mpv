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

#include "cuda_dynamic.h"

#include <pthread.h>

#if defined(_WIN32)
# include <windows.h>
# define dlopen(filename, flags) LoadLibrary(TEXT(filename))
# define dlsym(handle, symbol) (void *)GetProcAddress(handle, symbol)
# define dlclose(handle) FreeLibrary(handle)
#else
# include <dlfcn.h>
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
# define CUDA_LIBNAME "nvcuda.dll"
#else
# define CUDA_LIBNAME "libcuda.so.1"
#endif

#define CUDA_DECL(NAME, TYPE) \
    TYPE *mpv_ ## NAME;
CUDA_FNS(CUDA_DECL)

static bool cuda_loaded = false;
static pthread_once_t cuda_load_once = PTHREAD_ONCE_INIT;

static void cuda_do_load(void)
{
    void *lib = dlopen(CUDA_LIBNAME, RTLD_LAZY);
    if (!lib) {
        return;
    }

#define CUDA_LOAD_SYMBOL(NAME, TYPE) \
    mpv_ ## NAME = dlsym(lib, #NAME); if (!mpv_ ## NAME) return;

    CUDA_FNS(CUDA_LOAD_SYMBOL)

    cuda_loaded = true;
}

bool cuda_load(void)
{
    pthread_once(&cuda_load_once, cuda_do_load);
    return cuda_loaded;
}
