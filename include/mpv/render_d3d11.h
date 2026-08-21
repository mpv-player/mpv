/* Copyright (C) 2024 the mpv developers
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef MPV_CLIENT_API_RENDER_D3D11_H_
#define MPV_CLIENT_API_RENDER_D3D11_H_

#include "render.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Direct3D 11 backend
 * -------------------
 *
 * This header contains definitions for using Direct3D 11 with the render.h
 * API.
 *
 * API use
 * -------
 *
 * The mpv_render_* API is used. That API supports multiple backends, and this
 * section documents specifics for the Direct3D 11 backend.
 *
 * Use mpv_render_context_create() with MPV_RENDER_PARAM_API_TYPE set to
 * MPV_RENDER_API_TYPE_D3D11, and MPV_RENDER_PARAM_D3D11_INIT_PARAMS provided.
 *
 * Call mpv_render_context_render() with MPV_RENDER_PARAM_D3D11_FBO to render
 * the video frame to a D3D11 texture. The API user is responsible for creating
 * and presenting the texture (for example, via a DXGI swap chain).
 *
 * Threading
 * ---------
 *
 * Unlike OpenGL, Direct3D 11 does not use a per-thread implicit context. Any
 * thread may call the mpv_render_* functions with this backend, subject to the
 * general restrictions described in render.h.
 *
 * The ID3D11Device passed in mpv_d3d11_init_params must be usable from the
 * thread that calls the mpv_render_* functions. An ID3D11Device created with
 * D3D11_CREATE_DEVICE_SINGLETHREADED is therefore not supported.
 */

/**
 * For initializing the mpv D3D11 state via MPV_RENDER_PARAM_D3D11_INIT_PARAMS.
 */
typedef struct mpv_d3d11_init_params {
    /**
     * The ID3D11Device to use for rendering. libmpv will add a reference to
     * the device, so it is safe to release the caller's reference after
     * mpv_render_context_create() returns.
     *
     * The device must remain valid until mpv_render_context_free() is called.
     *
     * Type: ID3D11Device*
     */
    void *device;
} mpv_d3d11_init_params;

/**
 * For MPV_RENDER_PARAM_D3D11_FBO.
 */
typedef struct mpv_d3d11_fbo {
    /**
     * The D3D11 texture to render into. This must be a 2D texture
     * (ID3D11Texture2D) created on the same ID3D11Device that was passed to
     * mpv_render_context_create() via mpv_d3d11_init_params.
     *
     * The texture must have been created with at least the
     * D3D11_BIND_RENDER_TARGET bind flag and D3D11_USAGE_DEFAULT usage. It
     * must not be multisampled, a texture array, or mipmapped.
     *
     * The texture is owned by the caller. libmpv does not take a reference,
     * so the caller must ensure the texture remains valid for the duration of
     * the mpv_render_context_render() call.
     *
     * Type: ID3D11Texture2D*
     */
    void *tex;
    /**
     * Dimensions of the render target in pixels. Must always be set, and must
     * match the actual size of the texture.
     */
    int w, h;
} mpv_d3d11_fbo;

#ifdef __cplusplus
}
#endif

#endif
