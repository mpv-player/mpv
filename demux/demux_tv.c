/*
 * Copyright (C) 2001 Alex Beregszaszi
 *
 * Feb 19, 2002: Significant rewrites by Charles R. Henrich (henrich@msu.edu)
 *               to add support for audio, and bktr *BSD support.
 *
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

#include "common/common.h"
#include "common/msg.h"

#include "options/m_option.h"
#include "options/m_config.h"
#include "options/options.h"

#include "demux.h"
#include "codec_tags.h"

#include "audio/format.h"
#include "osdep/endian.h"

#include "stream/stream.h"
#include "stream/tv.h"

static int demux_open_tv(demuxer_t *demuxer, enum demux_check check)
{
    tvi_handle_t *tvh;
    const tvi_functions_t *funcs;

    if (check > DEMUX_CHECK_REQUEST)
        return -1;

    if (!demuxer->stream->info || strcmp(demuxer->stream->info->name, "tv") != 0)
        return -1;

    tv_param_t *params = mp_get_config_group(demuxer, demuxer->global,
                                             &tv_params_conf);
    bstr urlparams = bstr0(demuxer->stream->path);
    bstr channel, input;
    bstr_split_tok(urlparams, "/", &channel, &input);
    if (channel.len) {
        talloc_free(params->channel);
        params->channel = bstrto0(NULL, channel);
    }
    if (input.len) {
        bstr r;
        params->input = bstrtoll(input, &r, 0);
        if (r.len) {
            MP_ERR(demuxer->stream, "invalid input: '%.*s'\n", BSTR_P(input));
            return -1;
        }
    }

    assert(demuxer->priv==NULL);
    if(!(tvh=tv_begin(params, demuxer->log))) return -1;
    if (!tvh->functions->init(tvh->priv)) return -1;

    tvh->demuxer = demuxer;

    if (!open_tv(tvh)){
        tv_uninit(tvh);
        return -1;
    }
    funcs = tvh->functions;
    demuxer->priv=tvh;

    struct sh_stream *sh_v = demux_alloc_sh_stream(STREAM_VIDEO);

    /* get IMAGE FORMAT */
    int fourcc;
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FORMAT, &fourcc);
    if (fourcc == MP_FOURCC_MJPEG || fourcc == MP_FOURCC_JPEG) {
        sh_v->codec->codec = "mjpeg";
    } else {
        sh_v->codec->codec = "rawvideo";
        sh_v->codec->codec_tag = fourcc;
    }

    /* set FPS and FRAMETIME */

    if(!sh_v->codec->fps)
    {
        float tmp;
        if (funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FPS, &tmp) != TVI_CONTROL_TRUE)
             sh_v->codec->fps = 25.0f; /* on PAL */
        else sh_v->codec->fps = tmp;
    }

    if (tvh->tv_param->fps != -1.0f)
        sh_v->codec->fps = tvh->tv_param->fps;

    /* If playback only mode, go to immediate mode, fail silently */
    if(tvh->tv_param->immediate == 1)
        {
        funcs->control(tvh->priv, TVI_CONTROL_IMMEDIATE, 0);
        tvh->tv_param->audio = 0;
        }

    /* set width */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_WIDTH, &sh_v->codec->disp_w);

    /* set height */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &sh_v->codec->disp_h);

    demux_add_sh_stream(demuxer, sh_v);

    demuxer->seekable = 0;

    /* here comes audio init */
    if (tvh->tv_param->audio && funcs->control(tvh->priv, TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
    {
        int audio_format;

        /* yeah, audio is present */

        funcs->control(tvh->priv, TVI_CONTROL_AUD_SET_SAMPLERATE,
                                  &tvh->tv_param->audiorate);

        if (funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_FORMAT, &audio_format) != TVI_CONTROL_TRUE)
            goto no_audio;

        switch(audio_format)
        {
            // This is the only format any of the current inputs generate.
            case AF_FORMAT_S16:
                break;
            default:
                MP_ERR(tvh, "Audio type '%s' unsupported!\n",
                    af_fmt_to_str(audio_format));
                goto no_audio;
        }

        struct sh_stream *sh_a = demux_alloc_sh_stream(STREAM_AUDIO);

        funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_SAMPLERATE,
                   &sh_a->codec->samplerate);
        int nchannels = sh_a->codec->channels.num;
        funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_CHANNELS,
                   &nchannels);
        mp_chmap_from_channels(&sh_a->codec->channels, nchannels);

        // s16ne
        mp_set_pcm_codec(sh_a->codec, true, false, 16, BYTE_ORDER == BIG_ENDIAN);

        demux_add_sh_stream(demuxer, sh_a);

        MP_VERBOSE(tvh, "  TV audio: %d channels, %d bits, %d Hz\n",
                   nchannels, 16, sh_a->codec->samplerate);
    }
no_audio:

    if(!(funcs->start(tvh->priv))){
        // start failed :(
        tv_uninit(tvh);
        return -1;
    }

    /* set color eq */
    tv_set_color_options(tvh, TV_COLOR_BRIGHTNESS, tvh->tv_param->brightness);
    tv_set_color_options(tvh, TV_COLOR_HUE, tvh->tv_param->hue);
    tv_set_color_options(tvh, TV_COLOR_SATURATION, tvh->tv_param->saturation);
    tv_set_color_options(tvh, TV_COLOR_CONTRAST, tvh->tv_param->contrast);

    if(tvh->tv_param->gain!=-1)
        if(funcs->control(tvh->priv,TVI_CONTROL_VID_SET_GAIN,&tvh->tv_param->gain)!=TVI_CONTROL_TRUE)
            MP_WARN(tvh, "Unable to set gain control!\n");

    return 0;
}

static void demux_close_tv(demuxer_t *demuxer)
{
    tvi_handle_t *tvh=(tvi_handle_t*)(demuxer->priv);
    if (!tvh) return;
    tv_uninit(tvh);
    free(tvh);
    demuxer->priv=NULL;
}

static int demux_tv_fill_buffer(demuxer_t *demux)
{
    tvi_handle_t *tvh=(tvi_handle_t*)(demux->priv);
    demux_packet_t* dp;
    unsigned int len=0;
    struct sh_stream *want_audio = NULL, *want_video = NULL;

    int num_streams = demux_get_num_stream(demux);
    for (int n = 0; n < num_streams; n++) {
        struct sh_stream *sh = demux_get_stream(demux, n);
        if (!demux_has_packet(sh) && demux_stream_is_selected(sh)) {
            if (sh->type == STREAM_AUDIO)
                want_audio = sh;
            if (sh->type == STREAM_VIDEO)
                want_video = sh;
        }
    }

    /* ================== ADD AUDIO PACKET =================== */

    if (want_audio && tvh->tv_param->audio &&
        tvh->functions->control(tvh->priv,
                                TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
    {
        len = tvh->functions->get_audio_framesize(tvh->priv);

        dp=new_demux_packet(len);
        if (dp) {
            dp->keyframe = true;
            dp->pts=tvh->functions->grab_audio_frame(tvh->priv, dp->buffer,len);
            demux_add_packet(want_audio, dp);
        }
    }

    /* ================== ADD VIDEO PACKET =================== */

    if (want_video && tvh->functions->control(tvh->priv,
                            TVI_CONTROL_IS_VIDEO, 0) == TVI_CONTROL_TRUE)
    {
        len = tvh->functions->get_video_framesize(tvh->priv);
        dp=new_demux_packet(len);
        if (dp) {
            dp->keyframe = true;
            dp->pts=tvh->functions->grab_video_frame(tvh->priv, dp->buffer, len);
            demux_add_packet(want_video, dp);
        }
    }

    if (tvh->tv_param->scan) tv_scan(tvh);
    return 1;
}

static int demux_tv_control(demuxer_t *demuxer, int cmd, void *arg)
{
    tvi_handle_t *tvh=(tvi_handle_t*)(demuxer->priv);
    if (cmd != DEMUXER_CTRL_STREAM_CTRL)
        return CONTROL_UNKNOWN;
    struct demux_ctrl_stream_ctrl *ctrl = arg;
    ctrl->res = tv_stream_control(tvh, ctrl->ctrl, ctrl->arg);
    return CONTROL_OK;
}

const demuxer_desc_t demuxer_desc_tv = {
    .name = "tv",
    .desc = "TV card demuxer",
    .fill_buffer = demux_tv_fill_buffer,
    .control = demux_tv_control,
    .open = demux_open_tv,
    .close = demux_close_tv,
};
