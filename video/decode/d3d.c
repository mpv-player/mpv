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

#include <pthread.h>

#include <libavcodec/avcodec.h>

#include "lavc.h"
#include "common/common.h"
#include "common/av_common.h"
#include "video/fmt-conversion.h"
#include "video/mp_image.h"
#include "video/mp_image_pool.h"
#include "osdep/windows_utils.h"

#include "d3d.h"

// define all the GUIDs used directly here, to avoid problems with inconsistent
// dxva2api.h versions in mingw-w64 and different MSVC version
#include <guiddef.h>
#include <cguid.h>
DEFINE_GUID(DXVA2_ModeMPEG2_VLD,                0xee27417f, 0x5e28, 0x4e65, 0xbe, 0xea, 0x1d, 0x26, 0xb5, 0x08, 0xad, 0xc9);
DEFINE_GUID(DXVA2_ModeMPEG2and1_VLD,            0x86695f12, 0x340e, 0x4f04, 0x9f, 0xd3, 0x92, 0x53, 0xdd, 0x32, 0x74, 0x60);

DEFINE_GUID(DXVA2_ModeH264_E,                   0x1b81be68, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA2_ModeH264_F,                   0x1b81be69, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA_ModeH264_VLD_WithFMOASO_NoFGT, 0xd5f04ff9, 0x3418, 0x45d8, 0x95, 0x61, 0x32, 0xa7, 0x6a, 0xae, 0x2d, 0xdd);
DEFINE_GUID(DXVA_Intel_H264_NoFGT_ClearVideo,   0x604F8E68, 0x4951, 0x4c54, 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6);
DEFINE_GUID(DXVA_ModeH264_VLD_NoFGT_Flash,      0x4245F676, 0x2BBC, 0x4166, 0xa0, 0xBB, 0x54, 0xE7, 0xB8, 0x49, 0xC3, 0x80);

DEFINE_GUID(DXVA2_ModeVC1_D,                    0x1b81beA3, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
DEFINE_GUID(DXVA2_ModeVC1_D2010,                0x1b81beA4, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5); // August 2010 update

DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main,            0x5b11d51b, 0x2f4c, 0x4452, 0xbc, 0xc3, 0x09, 0xf2, 0xa1, 0x16, 0x0c, 0xc0);
DEFINE_GUID(DXVA2_ModeHEVC_VLD_Main10,          0x107af0e0, 0xef1a, 0x4d19, 0xab, 0xa8, 0x67, 0xa1, 0x63, 0x07, 0x3d, 0x13);

DEFINE_GUID(DXVA2_ModeVP9_VLD_Profile0,         0x463707f8, 0xa1d0, 0x4585, 0x87, 0x6d, 0x83, 0xaa, 0x6d, 0x60, 0xb8, 0x9e);

DEFINE_GUID(DXVA2_NoEncrypt,                    0x1b81beD0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);

static const int PROF_MPEG2_MAIN[]   = {FF_PROFILE_MPEG2_SIMPLE,
                                        FF_PROFILE_MPEG2_MAIN, 0};
static const int PROF_H264_HIGH[]    = {FF_PROFILE_H264_CONSTRAINED_BASELINE,
                                        FF_PROFILE_H264_MAIN,
                                        FF_PROFILE_H264_HIGH, 0};
static const int PROF_HEVC_MAIN[]    = {FF_PROFILE_HEVC_MAIN, 0};
static const int PROF_HEVC_MAIN10[]  = {FF_PROFILE_HEVC_MAIN,
                                        FF_PROFILE_HEVC_MAIN_10, 0};

struct d3dva_mode {
    const GUID      *guid;
    const char      *name;
    enum AVCodecID   codec;
    const int       *profiles; // NULL or ends with 0
};

#define MODE2(id) &MP_CONCAT(DXVA2_Mode, id), # id
#define  MODE(id) &MP_CONCAT(DXVA_,      id), # id
// Preferred modes must come first
static const struct d3dva_mode d3dva_modes[] = {
    // MPEG-1/2
    {MODE2(MPEG2_VLD),        AV_CODEC_ID_MPEG2VIDEO, PROF_MPEG2_MAIN},
    {MODE2(MPEG2and1_VLD),    AV_CODEC_ID_MPEG2VIDEO, PROF_MPEG2_MAIN},
    {MODE2(MPEG2and1_VLD),    AV_CODEC_ID_MPEG1VIDEO},

    // H.264
    {MODE2(H264_F),                        AV_CODEC_ID_H264, PROF_H264_HIGH},
    {MODE2(H264_E),                        AV_CODEC_ID_H264, PROF_H264_HIGH},
    {MODE (Intel_H264_NoFGT_ClearVideo),   AV_CODEC_ID_H264, PROF_H264_HIGH},
    {MODE (ModeH264_VLD_WithFMOASO_NoFGT), AV_CODEC_ID_H264, PROF_H264_HIGH},
    {MODE (ModeH264_VLD_NoFGT_Flash),      AV_CODEC_ID_H264, PROF_H264_HIGH},

    // VC-1 / WMV3
    {MODE2(VC1_D),            AV_CODEC_ID_VC1},
    {MODE2(VC1_D),            AV_CODEC_ID_WMV3},
    {MODE2(VC1_D2010),        AV_CODEC_ID_VC1},
    {MODE2(VC1_D2010),        AV_CODEC_ID_WMV3},

    // HEVC
    {MODE2(HEVC_VLD_Main),    AV_CODEC_ID_HEVC, PROF_HEVC_MAIN},
    {MODE2(HEVC_VLD_Main10),  AV_CODEC_ID_HEVC, PROF_HEVC_MAIN10},

    // VP9
    {MODE2(VP9_VLD_Profile0), AV_CODEC_ID_VP9},
};
#undef MODE
#undef MODE2

HMODULE d3d11_dll, d3d9_dll, dxva2_dll;

static pthread_once_t d3d_load_once = PTHREAD_ONCE_INIT;

static void d3d_do_load(void)
{
    d3d11_dll = LoadLibrary(L"d3d11.dll");
    d3d9_dll  = LoadLibrary(L"d3d9.dll");
    dxva2_dll = LoadLibrary(L"dxva2.dll");
}

void d3d_load_dlls(void)
{
    pthread_once(&d3d_load_once, d3d_do_load);
}

int d3d_probe_codec(const char *codec)
{
    enum AVCodecID codecid = mp_codec_to_av_codec_id(codec);
    for (int i = 0; i < MP_ARRAY_SIZE(d3dva_modes); i++) {
        const struct d3dva_mode *mode = &d3dva_modes[i];
        if (mode->codec == codecid)
            return 0;
    }
    return HWDEC_ERR_NO_CODEC;
}

static bool profile_compatible(const struct d3dva_mode *mode, int profile)
{
    if (!mode->profiles)
        return true;

    for (int i = 0; mode->profiles[i]; i++){
        if(mode->profiles[i] == profile)
            return true;
    }
    return false;
}

static bool mode_supported(const struct d3dva_mode *mode,
                           const GUID *device_modes, UINT n_modes)
{
    for (int i = 0; i < n_modes; i++) {
        if (IsEqualGUID(mode->guid, &device_modes[i]))
            return true;
    }
    return false;
}

struct d3d_decoder_fmt d3d_select_decoder_mode(
    struct lavc_ctx *s, const GUID *device_guids, UINT n_guids,
    const struct d3d_decoded_format *formats, int n_formats,
    bool (*test_fmt_cb)(struct lavc_ctx *s, const GUID *guid,
                        const struct d3d_decoded_format *fmt))
{
    struct d3d_decoder_fmt fmt = {
        .guid   = &GUID_NULL,
        .format = NULL,
    };

    // this has the right bit-depth, but is unfortunately not the native format
    int sw_img_fmt = pixfmt2imgfmt(s->avctx->sw_pix_fmt);
    if (sw_img_fmt == IMGFMT_NONE)
        return fmt;

    int depth = IMGFMT_RGB_DEPTH(sw_img_fmt);

    for (int i = 0; i < MP_ARRAY_SIZE(d3dva_modes); i++) {
        const struct d3dva_mode *mode = &d3dva_modes[i];
        if (mode->codec == s->avctx->codec_id &&
            profile_compatible(mode, s->avctx->profile) &&
            mode_supported(mode, device_guids, n_guids)) {

            for (int n = 0; n < n_formats; n++) {
                const struct d3d_decoded_format *format = &formats[n];

                if (depth <= format->depth && test_fmt_cb(s, mode->guid, format))
                {
                    MP_VERBOSE(s, "Selecting %s ",
                               d3d_decoder_guid_to_desc(mode->guid));
                    if (format->dxfmt >= (1 << 16)) {
                        MP_VERBOSE(s, "%s\n", mp_tag_str(format->dxfmt));
                    } else {
                        MP_VERBOSE(s, "%d\n", (int)format->dxfmt);
                    }

                    fmt.guid   = mode->guid;
                    fmt.format = format;
                    return fmt;
                }
            }
        }
    }
    return fmt;
}

char *d3d_decoder_guid_to_desc_buf(char *buf, size_t buf_size,
                                   const GUID *mode_guid)
{
    const char *name = "<unknown>";
    for (int i = 0; i < MP_ARRAY_SIZE(d3dva_modes); i++) {
        const struct d3dva_mode *mode = &d3dva_modes[i];
        if (IsEqualGUID(mode->guid, mode_guid)) {
            name = mode->name;
            break;
        }
    }
    snprintf(buf, buf_size, "%s %s", mp_GUID_to_str(mode_guid), name);
    return buf;
}

void d3d_surface_align(struct lavc_ctx *s, int *w, int *h)
{
    int alignment = 16;
    switch (s->avctx->codec_id) {
        // decoding MPEG-2 requires additional alignment on some Intel GPUs, but it
        // causes issues for H.264 on certain AMD GPUs.....
    case AV_CODEC_ID_MPEG2VIDEO:
        alignment = 32;
        break;
        // the HEVC DXVA2 spec asks for 128 pixel aligned surfaces to ensure
        // all coding features have enough room to work with
    case AV_CODEC_ID_HEVC:
        alignment = 128;
        break;
    }
    *w = FFALIGN(*w, alignment);
    *h = FFALIGN(*h, alignment);
}

unsigned d3d_decoder_config_score(struct lavc_ctx *s,
                                  GUID *guidConfigBitstreamEncryption,
                                  UINT ConfigBitstreamRaw)
{
    unsigned score = 0;
    if (ConfigBitstreamRaw == 1) {
        score = 1;
    } else if (s->avctx->codec_id == AV_CODEC_ID_H264
               && ConfigBitstreamRaw == 2) {
        score = 2;
    } else {
        return 0;
    }

    if (IsEqualGUID(guidConfigBitstreamEncryption, &DXVA2_NoEncrypt))
        score += 16;

    return score;
}

BOOL is_clearvideo(const GUID *mode_guid)
{
    return IsEqualGUID(mode_guid, &DXVA_Intel_H264_NoFGT_ClearVideo);
}

void copy_nv12(struct mp_image *dest, uint8_t *src_bits,
               unsigned src_pitch, unsigned surf_height)
{
    struct mp_image buf = {0};
    mp_image_setfmt(&buf, dest->imgfmt);
    mp_image_set_size(&buf, dest->w, dest->h);

    buf.planes[0] = src_bits;
    buf.stride[0] = src_pitch;
    buf.planes[1] = src_bits + src_pitch * surf_height;
    buf.stride[1] = src_pitch;
    mp_image_copy_gpu(dest, &buf);
}

// Test if Direct3D11 can be used by us. Basically, this prevents trying to use
// D3D11 on Win7, and then failing somewhere in the process.
bool d3d11_check_decoding(ID3D11Device *dev)
{
    HRESULT hr;
    // We assume that NV12 is always supported, if hw decoding is supported at
    // all.
    UINT supported = 0;
    hr = ID3D11Device_CheckFormatSupport(dev, DXGI_FORMAT_NV12, &supported);
    return !FAILED(hr) && (supported & D3D11_BIND_DECODER);
}

static int get_dxgi_mpfmt(DWORD dxgi_fmt)
{
    switch (dxgi_fmt) {
    case DXGI_FORMAT_NV12: return IMGFMT_NV12;
    case DXGI_FORMAT_P010: return IMGFMT_P010;
    case DXGI_FORMAT_P016: return IMGFMT_P010;
    }
    return 0;
}

struct mp_image *d3d11_download_image(struct mp_hwdec_ctx *ctx,
                                      struct mp_image *mpi,
                                      struct mp_image_pool *swpool)
{
    HRESULT hr;
    ID3D11Device *device = ctx->ctx;

    if (mpi->imgfmt != IMGFMT_D3D11VA && mpi->imgfmt != IMGFMT_D3D11NV12)
        return NULL;

    ID3D11Texture2D *texture = (void *)mpi->planes[1];
    int subindex = (intptr_t)mpi->planes[2];
    if (!texture)
        return NULL;

    D3D11_TEXTURE2D_DESC tex_desc;
    ID3D11Texture2D_GetDesc(texture, &tex_desc);
    int mpfmt = get_dxgi_mpfmt(tex_desc.Format);
    if (!mpfmt)
        return NULL;

    // create staging texture shared with the CPU with mostly the same
    // parameters as the source texture
    tex_desc.MipLevels      = 1;
    tex_desc.MiscFlags      = 0;
    tex_desc.ArraySize      = 1;
    tex_desc.Usage          = D3D11_USAGE_STAGING;
    tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    tex_desc.BindFlags      = 0;
    ID3D11Texture2D *staging = NULL;
    hr = ID3D11Device_CreateTexture2D(device, &tex_desc, NULL, &staging);
    if (FAILED(hr))
        return NULL;

    bool ok = false;
    struct mp_image *sw_img = NULL;
    ID3D11DeviceContext *device_ctx = NULL;
    ID3D11Device_GetImmediateContext(device, &device_ctx);

    // copy to the staging texture
    ID3D11DeviceContext_CopySubresourceRegion(
        device_ctx,
        (ID3D11Resource *)staging, 0, 0, 0, 0,
        (ID3D11Resource *)texture, subindex, NULL);

    sw_img = mp_image_pool_get(swpool, mpfmt, tex_desc.Width, tex_desc.Height);
    if (!sw_img)
        goto done;

    // copy staging texture to the cpu mp_image
    D3D11_MAPPED_SUBRESOURCE lock;
    hr = ID3D11DeviceContext_Map(device_ctx, (ID3D11Resource *)staging,
                                 0, D3D11_MAP_READ, 0, &lock);
    if (FAILED(hr))
        goto done;
    copy_nv12(sw_img, lock.pData, lock.RowPitch, tex_desc.Height);
    ID3D11DeviceContext_Unmap(device_ctx, (ID3D11Resource *)staging, 0);

    mp_image_set_size(sw_img, mpi->w, mpi->h);
    mp_image_copy_attributes(sw_img, mpi);
    ok = true;

done:
    ID3D11Texture2D_Release(staging);
    ID3D11DeviceContext_Release(device_ctx);
    if (!ok)
        mp_image_unrefp(&sw_img);
    return sw_img;
}
