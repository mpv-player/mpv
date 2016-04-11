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

#include "mp_image.h"
#include "d3d11va.h"

struct d3d11va_surface {
    HMODULE d3d11_dll;
    ID3D11Texture2D              *texture;
    ID3D11VideoDecoderOutputView *surface;
};

ID3D11VideoDecoderOutputView *d3d11_surface_in_mp_image(struct mp_image *mpi)
{
    return mpi && mpi->imgfmt == IMGFMT_D3D11VA ?
        (ID3D11VideoDecoderOutputView *)mpi->planes[3] : NULL;
}

ID3D11Texture2D *d3d11_texture_in_mp_image(struct mp_image *mpi)
{
    if (!mpi || mpi->imgfmt != IMGFMT_D3D11VA)
        return NULL;
    struct d3d11va_surface *surface = (void *)mpi->planes[0];
    return surface->texture;
}

static void d3d11va_release_img(void *arg)
{
    struct d3d11va_surface *surface = arg;
    if (surface->surface)
        ID3D11VideoDecoderOutputView_Release(surface->surface);

    if (surface->texture)
        ID3D11Texture2D_Release(surface->texture);

    if (surface->d3d11_dll)
        FreeLibrary(surface->d3d11_dll);

    talloc_free(surface);
}

struct mp_image *d3d11va_new_ref(ID3D11VideoDecoderOutputView *view,
                                 int w, int h)
{
    if (!view)
        return NULL;
    struct d3d11va_surface *surface = talloc_zero(NULL, struct d3d11va_surface);

    surface->d3d11_dll = LoadLibrary(L"d3d11.dll");
    if (!surface->d3d11_dll)
        goto fail;

    surface->surface = view;
    ID3D11VideoDecoderOutputView_AddRef(surface->surface);
    ID3D11VideoDecoderOutputView_GetResource(
        surface->surface, (ID3D11Resource **)&surface->texture);

    struct mp_image *mpi = mp_image_new_custom_ref(
        &(struct mp_image){0}, surface, d3d11va_release_img);
    if (!mpi)
        abort();

    mp_image_setfmt(mpi, IMGFMT_D3D11VA);
    mp_image_set_size(mpi, w, h);
    mpi->planes[0] = (void *)surface;
    mpi->planes[3] = (void *)surface->surface;

    return mpi;
fail:
    d3d11va_release_img(surface);
    return NULL;
}
