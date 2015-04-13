#include <assert.h>

#include "common/common.h"
#include "common/msg.h"

#include "options/m_option.h"
#include "options/m_config.h"
#include "options/options.h"

#include "demux.h"
#include "codec_tags.h"

#include "audio/format.h"
#include "video/img_fourcc.h"
#include "osdep/endian.h"

#include "stream/stream.h"
#include "stream/tv.h"

static int demux_open_tv(demuxer_t *demuxer, enum demux_check check)
{
    tvi_handle_t *tvh;
    sh_video_t *sh_video;
    sh_audio_t *sh_audio = NULL;
    const tvi_functions_t *funcs;

    if (check > DEMUX_CHECK_REQUEST || demuxer->stream->type != STREAMTYPE_TV)
        return -1;

    tv_param_t *params = m_sub_options_copy(demuxer, &tv_params_conf,
                                            demuxer->opts->tv_params);
    struct tv_stream_params *sparams = demuxer->stream->priv;
    if (sparams->channel && sparams->channel[0]) {
        talloc_free(params->channel);
        params->channel = talloc_strdup(NULL, sparams->channel);
    }
    if (sparams->input >= 0)
        params->input = sparams->input;

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

    struct sh_stream *sh_v = new_sh_stream(demuxer, STREAM_VIDEO);
    sh_video = sh_v->video;

    /* get IMAGE FORMAT */
    int fourcc;
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FORMAT, &fourcc);
    if (fourcc == MP_FOURCC_MJPEG) {
        sh_v->codec = "mjpeg";
    } else {
        sh_v->codec = "rawvideo";
        sh_v->format = fourcc;
    }

    /* set FPS and FRAMETIME */

    if(!sh_video->fps)
    {
        float tmp;
        if (funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FPS, &tmp) != TVI_CONTROL_TRUE)
             sh_video->fps = 25.0f; /* on PAL */
        else sh_video->fps = tmp;
    }

    if (tvh->tv_param->fps != -1.0f)
        sh_video->fps = tvh->tv_param->fps;

    /* If playback only mode, go to immediate mode, fail silently */
    if(tvh->tv_param->immediate == 1)
        {
        funcs->control(tvh->priv, TVI_CONTROL_IMMEDIATE, 0);
        tvh->tv_param->audio = 0;
        }

    /* set width */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_WIDTH, &sh_video->disp_w);

    /* set height */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &sh_video->disp_h);

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

        struct sh_stream *sh_a = new_sh_stream(demuxer, STREAM_AUDIO);
        sh_audio = sh_a->audio;

        funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_SAMPLERATE,
                   &sh_audio->samplerate);
        int nchannels = sh_audio->channels.num;
        funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_CHANNELS,
                   &nchannels);
        mp_chmap_from_channels(&sh_audio->channels, nchannels);

        // s16ne
        mp_set_pcm_codec(sh_a, true, false, 16, BYTE_ORDER == BIG_ENDIAN);

        MP_VERBOSE(tvh, "  TV audio: %d channels, %d bits, %d Hz\n",
                   nchannels, 16, sh_audio->samplerate);
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

    for (int n = 0; n < demux->num_streams; n++) {
        struct sh_stream *sh = demux->streams[n];
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
        return DEMUXER_CTRL_NOTIMPL;
    struct demux_ctrl_stream_ctrl *ctrl = arg;
    ctrl->res = tv_stream_control(tvh, ctrl->ctrl, ctrl->arg);
    return DEMUXER_CTRL_OK;
}

const demuxer_desc_t demuxer_desc_tv = {
    .name = "tv",
    .desc = "TV card demuxer",
    .fill_buffer = demux_tv_fill_buffer,
    .control = demux_tv_control,
    .open = demux_open_tv,
    .close = demux_close_tv,
};
