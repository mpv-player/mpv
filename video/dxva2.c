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

#include "common/av_common.h"
#include "dxva2.h"
#include "mp_image.h"
#include "img_format.h"
#include "mp_image_pool.h"

struct dxva2_surface {
    HMODULE d3dlib;
    HMODULE dxva2lib;

    IDirectXVideoDecoder *decoder;
    IDirect3DSurface9    *surface;
};

IDirect3DSurface9 *d3d9_surface_in_mp_image(struct mp_image *mpi)
{
    return mpi && mpi->imgfmt == IMGFMT_DXVA2 ?
        (IDirect3DSurface9 *)mpi->planes[3] : NULL;
}

static void dxva2_release_img(void *arg)
{
    struct dxva2_surface *surface = arg;
    if (surface->surface)
        IDirect3DSurface9_Release(surface->surface);

    if (surface->decoder)
        IDirectXVideoDecoder_Release(surface->decoder);

    if (surface->dxva2lib)
        FreeLibrary(surface->dxva2lib);

    if (surface->d3dlib)
        FreeLibrary(surface->d3dlib);

    talloc_free(surface);
}

struct mp_image *dxva2_new_ref(IDirectXVideoDecoder *decoder,
                               IDirect3DSurface9 *d3d9_surface, int w, int h)
{
    if (!decoder || !d3d9_surface)
        return NULL;
    struct dxva2_surface *surface = talloc_zero(NULL, struct dxva2_surface);

    // Add additional references to the libraries which might otherwise be freed
    // before the surface, which is observed to lead to bad behaviour
    surface->d3dlib   = LoadLibrary(L"d3d9.dll");
    surface->dxva2lib = LoadLibrary(L"dxva2.dll");
    if (!surface->d3dlib || !surface->dxva2lib)
        goto fail;

    surface->surface = d3d9_surface;
    IDirect3DSurface9_AddRef(surface->surface);
    surface->decoder = decoder;
    IDirectXVideoDecoder_AddRef(surface->decoder);

    struct mp_image *mpi = mp_image_new_custom_ref(&(struct mp_image){0},
                                                   surface, dxva2_release_img);
    if (!mpi)
        abort();

    mp_image_setfmt(mpi, IMGFMT_DXVA2);
    mp_image_set_size(mpi, w, h);
    mpi->planes[3] = (void *)surface->surface;
    return mpi;
fail:
    dxva2_release_img(surface);
    return NULL;
}
