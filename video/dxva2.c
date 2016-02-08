/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>

#include "common/av_common.h"
#include "dxva2.h"
#include "mp_image.h"
#include "img_format.h"
#include "mp_image_pool.h"

struct dxva2_surface {
    HMODULE d3dlib;
    HMODULE dxva2lib;

    IDirectXVideoDecoder *decoder;
    LPDIRECT3DSURFACE9   surface;
};

LPDIRECT3DSURFACE9 d3d9_surface_in_mp_image(struct mp_image *mpi)
{
    return mpi && mpi->imgfmt == IMGFMT_DXVA2 ?
        (LPDIRECT3DSURFACE9)mpi->planes[3] : NULL;
}

void dxva2_img_ref_decoder(struct mp_image *mpi, IDirectXVideoDecoder *decoder)
{
    assert(mpi->imgfmt == IMGFMT_DXVA2);
    struct dxva2_surface *surface = (struct dxva2_surface *)mpi->planes[0];
    if (surface->decoder)
        IDirectXVideoDecoder_Release(surface->decoder);
    surface->decoder = decoder;
    IDirectXVideoDecoder_AddRef(surface->decoder);
}

static void dxva2_pool_release_img(void *arg)
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

struct pool_alloc_ctx {
    IDirectXVideoDecoderService *decoder_service;
    D3DFORMAT target_format;
    int surface_alignment;
};

static struct mp_image *dxva2_pool_alloc_img(void *arg, int fmt, int w, int h)
{
    if (fmt != IMGFMT_DXVA2)
        return NULL;
    struct dxva2_surface *surface = talloc_zero(NULL, struct dxva2_surface);

    // Add additional references to the libraries which might otherwise be freed
    // before the surface, which is observed to lead to bad behaviour
    surface->d3dlib   = LoadLibrary(L"d3d9.dll");
    surface->dxva2lib = LoadLibrary(L"dxva2.dll");
    if (!surface->d3dlib || !surface->dxva2lib)
        goto fail;

    struct pool_alloc_ctx *alloc_ctx = arg;
    HRESULT hr = IDirectXVideoDecoderService_CreateSurface(
        alloc_ctx->decoder_service,
        FFALIGN(w, alloc_ctx->surface_alignment),
        FFALIGN(h, alloc_ctx->surface_alignment),
        0, alloc_ctx->target_format, D3DPOOL_DEFAULT, 0,
        DXVA2_VideoDecoderRenderTarget,
        &surface->surface, NULL);
    if (FAILED(hr))
        goto fail;

    struct mp_image mpi = {0};
    mp_image_setfmt(&mpi, IMGFMT_DXVA2);
    mp_image_set_size(&mpi, w, h);
    mpi.planes[0] = (void *)surface;
    mpi.planes[3] = (void *)surface->surface;

    return mp_image_new_custom_ref(&mpi, surface, dxva2_pool_release_img);
fail:
    dxva2_pool_release_img(surface);
    return NULL;
}

void dxva2_pool_set_allocator(struct mp_image_pool *pool,
                              IDirectXVideoDecoderService *decoder_service,
                              D3DFORMAT target_format, int surface_alignment)
{
    struct pool_alloc_ctx *alloc_ctx = talloc_ptrtype(pool, alloc_ctx);
    *alloc_ctx =  (struct pool_alloc_ctx){
        decoder_service   = decoder_service,
        target_format     = target_format,
        surface_alignment = surface_alignment
    };
    mp_image_pool_set_allocator(pool, dxva2_pool_alloc_img, alloc_ctx);
    mp_image_pool_set_lru(pool);
}
