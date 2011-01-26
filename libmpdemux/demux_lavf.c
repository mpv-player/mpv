/*
 * Copyright (C) 2004 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// #include <stdio.h>
#include <stdlib.h>
// #include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"
#include "options.h"
#include "mp_msg.h"
#include "av_opts.h"
#include "bstr.h"

#include "stream/stream.h"
#include "aviprint.h"
#include "demuxer.h"
#include "stheader.h"
#include "m_option.h"
#include "sub/sub.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/avutil.h"
#include "libavutil/avstring.h"
#include "libavcodec/opt.h"

#include "mp_taglists.h"

#define INITIAL_PROBE_SIZE STREAM_BUFFER_SIZE
#define SMALL_MAX_PROBE_SIZE (32 * 1024)
#define PROBE_BUF_SIZE (2*1024*1024)

const m_option_t lavfdopts_conf[] = {
    OPT_INTRANGE("probesize", lavfdopts.probesize, 0, 32, INT_MAX),
    OPT_STRING("format", lavfdopts.format, 0),
    OPT_INTRANGE("analyzeduration", lavfdopts.analyzeduration, 0, 0, INT_MAX),
    OPT_STRING("cryptokey", lavfdopts.cryptokey, 0),
    OPT_STRING("o", lavfdopts.avopt, 0),
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

#define BIO_BUFFER_SIZE 32768

typedef struct lavf_priv {
    AVInputFormat *avif;
    AVFormatContext *avfc;
    ByteIOContext *pb;
    uint8_t buffer[BIO_BUFFER_SIZE];
    int audio_streams;
    int video_streams;
    int sub_streams;
    int64_t last_pts;
    int astreams[MAX_A_STREAMS];
    int vstreams[MAX_V_STREAMS];
    int sstreams[MAX_S_STREAMS];
    int cur_program;
    int nb_streams_last;
    bool internet_radio_hack;
    bool use_dts;
    bool seek_by_bytes;
    int bitrate;
}lavf_priv_t;

static int mp_read(void *opaque, uint8_t *buf, int size) {
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    int ret;

    ret=stream_read(stream, buf, size);

    mp_msg(MSGT_HEADER,MSGL_DBG2,"%d=mp_read(%p, %p, %d), pos: %"PRId64", eof:%d\n",
           ret, stream, buf, size, stream_tell(stream), stream->eof);
    return ret;
}

static int64_t mp_seek(void *opaque, int64_t pos, int whence) {
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    int64_t current_pos;
    mp_msg(MSGT_HEADER,MSGL_DBG2,"mp_seek(%p, %"PRId64", %d)\n", stream, pos, whence);
    if(whence == SEEK_CUR)
        pos +=stream_tell(stream);
    else if(whence == SEEK_END && stream->end_pos > 0)
        pos += stream->end_pos;
    else if(whence == SEEK_SET)
        pos += stream->start_pos;
    else if(whence == AVSEEK_SIZE && stream->end_pos > 0)
        return stream->end_pos - stream->start_pos;
    else
        return -1;

    if(pos<0)
        return -1;
    current_pos = stream_tell(stream);
    if(stream_seek(stream, pos)==0) {
        stream_reset(stream);
        stream_seek(stream, current_pos);
        return -1;
    }

    return pos - stream->start_pos;
}

static int64_t mp_read_seek(void *opaque, int stream_idx, int64_t ts, int flags)
{
    struct demuxer *demuxer = opaque;
    struct stream *stream = demuxer->stream;
    struct lavf_priv *priv = demuxer->priv;

    AVStream *st = priv->avfc->streams[stream_idx];
    double pts = (double)ts * st->time_base.num / st->time_base.den;
    int ret = stream_control(stream, STREAM_CTRL_SEEK_TO_TIME, &pts);
    if (ret < 0)
        ret = AVERROR(ENOSYS);
    return ret;
}

static void list_formats(void) {
    mp_msg(MSGT_DEMUX, MSGL_INFO, "Available lavf input formats:\n");
    AVInputFormat *fmt = NULL;
    while (fmt = av_iformat_next(fmt))
        mp_msg(MSGT_DEMUX, MSGL_INFO, "%15s : %s\n", fmt->name, fmt->long_name);
}

static int lavf_check_file(demuxer_t *demuxer){
    struct MPOpts *opts = demuxer->opts;
    struct lavfdopts *lavfdopts = &opts->lavfdopts;
    AVProbeData avpd;
    lavf_priv_t *priv;
    int probe_data_size = 0;
    int read_size = INITIAL_PROBE_SIZE;
    int score;

    if(!demuxer->priv)
        demuxer->priv=calloc(sizeof(lavf_priv_t),1);
    priv= demuxer->priv;

    av_register_all();

    char *format = lavfdopts->format;
    if (!format)
        format = demuxer->stream->lavf_type;
    if (format) {
        if (strcmp(format, "help") == 0) {
           list_formats();
           return 0;
        }
        priv->avif = av_find_input_format(format);
        if (!priv->avif) {
            mp_msg(MSGT_DEMUX, MSGL_FATAL, "Unknown lavf format %s\n", format);
            return 0;
        }
        mp_msg(MSGT_DEMUX,MSGL_INFO,"Forced lavf %s demuxer\n", priv->avif->long_name);
        return DEMUXER_TYPE_LAVF;
    }

    avpd.buf = av_mallocz(FFMAX(BIO_BUFFER_SIZE, PROBE_BUF_SIZE) +
                          FF_INPUT_BUFFER_PADDING_SIZE);
    do {
        read_size = stream_read(demuxer->stream, avpd.buf + probe_data_size, read_size);
        if(read_size < 0) {
            av_free(avpd.buf);
            return 0;
        }
        probe_data_size += read_size;
        avpd.filename= demuxer->stream->url;
        if (!avpd.filename) {
            mp_msg(MSGT_DEMUX, MSGL_WARN, "Stream url is not set!\n");
            avpd.filename = "";
        }
        if (!strncmp(avpd.filename, "ffmpeg://", 9))
            avpd.filename += 9;
        avpd.buf_size= probe_data_size;

        score = 0;
        priv->avif= av_probe_input_format2(&avpd, probe_data_size > 0, &score);
        read_size = FFMIN(2*read_size, PROBE_BUF_SIZE - probe_data_size);
    } while ((demuxer->desc->type != DEMUXER_TYPE_LAVF_PREFERRED ||
              probe_data_size < SMALL_MAX_PROBE_SIZE) &&
             score <= AVPROBE_SCORE_MAX / 4 &&
             read_size > 0 && probe_data_size < PROBE_BUF_SIZE);
    av_free(avpd.buf);

    if (!priv->avif || score <= AVPROBE_SCORE_MAX / 4) {
        mp_msg(MSGT_HEADER,MSGL_V,"LAVF_check: no clue about this gibberish!\n");
        return 0;
    }else
        mp_msg(MSGT_HEADER,MSGL_V,"LAVF_check: %s\n", priv->avif->long_name);

    demuxer->filetype = priv->avif->long_name;
    if (!demuxer->filetype)
        demuxer->filetype = priv->avif->name;

    return DEMUXER_TYPE_LAVF;
}

static bool matches_avinputformat_name(struct lavf_priv *priv,
                                       const char *name)
{
    const char *avifname = priv->avif->name;
    while (1) {
        const char *next = strchr(avifname, ',');
        if (!next)
            return !strcmp(avifname, name);
        int len = next - avifname;
        if (len == strlen(name) && !memcmp(avifname, name, len))
            return true;
        avifname = next + 1;
    }
}

/* formats for which an internal demuxer is preferred */
static const char * const preferred_internal[] = {
    /* lavf Matroska demuxer doesn't support ordered chapters and fails
     * for more files */
    "matroska",
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(52, 99, 0)
    /* Seeking doesn't work with lavf FLAC demuxer in FFmpeg versions
     * without a FLAC parser. In principle this could use a runtime check to
     * switch if a shared library is updated. */
    "flac",
#endif
    /* lavf gives neither pts nor dts for some video frames in .rm */
    "rm",
    NULL
};

static int lavf_check_preferred_file(demuxer_t *demuxer){
    if (lavf_check_file(demuxer)) {
        const char * const *p;
        lavf_priv_t *priv = demuxer->priv;
        for (p = preferred_internal; *p; p++)
            if (matches_avinputformat_name(priv, *p))
                return 0;
        return DEMUXER_TYPE_LAVF_PREFERRED;
    }
    return 0;
}

static uint8_t char2int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void parse_cryptokey(AVFormatContext *avfc, const char *str) {
    int len = strlen(str) / 2;
    uint8_t *key = av_mallocz(len);
    int i;
    avfc->keylen = len;
    avfc->key = key;
    for (i = 0; i < len; i++, str += 2)
        *key++ = (char2int(str[0]) << 4) | char2int(str[1]);
}

static void handle_stream(demuxer_t *demuxer, AVFormatContext *avfc, int i) {
    lavf_priv_t *priv= demuxer->priv;
    AVStream *st= avfc->streams[i];
    AVCodecContext *codec= st->codec;
    char *stream_type = NULL;
    int stream_id;
    AVMetadataTag *lang = av_metadata_get(st->metadata, "language", NULL, 0);
    AVMetadataTag *title= av_metadata_get(st->metadata, "title",    NULL, 0);
    int g, override_tag = mp_av_codec_get_tag(mp_codecid_override_taglists,
                                              codec->codec_id);
    // For some formats (like PCM) always trust CODEC_ID_* more than codec_tag
    if (override_tag)
        codec->codec_tag = override_tag;

    switch(codec->codec_type){
        case CODEC_TYPE_AUDIO:{
            WAVEFORMATEX *wf;
            sh_audio_t* sh_audio;
            sh_audio = new_sh_audio_aid(demuxer, i, priv->audio_streams);
            if(!sh_audio)
                break;
            stream_type = "audio";
            priv->astreams[priv->audio_streams] = i;
            wf= calloc(sizeof(*wf) + codec->extradata_size, 1);
            // mp4a tag is used for all mp4 files no matter what they actually contain
            if(codec->codec_tag == MKTAG('m', 'p', '4', 'a'))
                codec->codec_tag= 0;
            if(!codec->codec_tag)
                codec->codec_tag= mp_av_codec_get_tag(mp_wav_taglists, codec->codec_id);
            wf->wFormatTag= codec->codec_tag;
            wf->nChannels= codec->channels;
            wf->nSamplesPerSec= codec->sample_rate;
            wf->nAvgBytesPerSec= codec->bit_rate/8;
            wf->nBlockAlign= codec->block_align ? codec->block_align : 1;
            wf->wBitsPerSample= codec->bits_per_coded_sample;
            wf->cbSize= codec->extradata_size;
            if(codec->extradata_size)
                memcpy(wf + 1, codec->extradata, codec->extradata_size);
            sh_audio->wf= wf;
            sh_audio->audio.dwSampleSize= codec->block_align;
            if(codec->frame_size && codec->sample_rate){
                sh_audio->audio.dwScale=codec->frame_size;
                sh_audio->audio.dwRate= codec->sample_rate;
            }else{
                sh_audio->audio.dwScale= codec->block_align ? codec->block_align*8 : 8;
                sh_audio->audio.dwRate = codec->bit_rate;
            }
            g= av_gcd(sh_audio->audio.dwScale, sh_audio->audio.dwRate);
            sh_audio->audio.dwScale /= g;
            sh_audio->audio.dwRate  /= g;
//          printf("sca:%d rat:%d fs:%d sr:%d ba:%d\n", sh_audio->audio.dwScale, sh_audio->audio.dwRate, codec->frame_size, codec->sample_rate, codec->block_align);
            sh_audio->ds= demuxer->audio;
            sh_audio->format= codec->codec_tag;
            sh_audio->channels= codec->channels;
            sh_audio->samplerate= codec->sample_rate;
            sh_audio->i_bps= codec->bit_rate/8;
            switch (codec->codec_id) {
                case CODEC_ID_PCM_S8:
                case CODEC_ID_PCM_U8:
                    sh_audio->samplesize = 1;
                    break;
                case CODEC_ID_PCM_S16LE:
                case CODEC_ID_PCM_S16BE:
                case CODEC_ID_PCM_U16LE:
                case CODEC_ID_PCM_U16BE:
                    sh_audio->samplesize = 2;
                    break;
                case CODEC_ID_PCM_ALAW:
                    sh_audio->format = 0x6;
                    break;
                case CODEC_ID_PCM_MULAW:
                    sh_audio->format = 0x7;
                    break;
            }
            if (title && title->value)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AID_%d_NAME=%s\n", priv->audio_streams, title->value);
            if (lang && lang->value) {
              sh_audio->lang = strdup(lang->value);
              mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AID_%d_LANG=%s\n", priv->audio_streams, sh_audio->lang);
            }
            if (st->disposition & AV_DISPOSITION_DEFAULT)
              sh_audio->default_track = 1;
            if(mp_msg_test(MSGT_HEADER,MSGL_V) ) print_wave_header(sh_audio->wf, MSGL_V);
            // select the first audio stream
            if (!demuxer->audio->sh) {
                demuxer->audio->id = i;
                demuxer->audio->sh= demuxer->a_streams[i];
            } else
                st->discard= AVDISCARD_ALL;
            stream_id = priv->audio_streams++;
            break;
        }
        case CODEC_TYPE_VIDEO:{
            sh_video_t* sh_video;
            BITMAPINFOHEADER *bih;
            sh_video=new_sh_video_vid(demuxer, i, priv->video_streams);
            if(!sh_video) break;
            stream_type = "video";
            priv->vstreams[priv->video_streams] = i;
            bih=calloc(sizeof(*bih) + codec->extradata_size,1);

            if(codec->codec_id == CODEC_ID_RAWVIDEO) {
                switch (codec->pix_fmt) {
                    case PIX_FMT_RGB24:
                        codec->codec_tag= MKTAG(24, 'B', 'G', 'R');
                    case PIX_FMT_BGR24:
                        codec->codec_tag= MKTAG(24, 'R', 'G', 'B');
                }
            }
            if(!codec->codec_tag)
                codec->codec_tag= mp_av_codec_get_tag(mp_bmp_taglists, codec->codec_id);
            bih->biSize= sizeof(*bih) + codec->extradata_size;
            bih->biWidth= codec->width;
            bih->biHeight= codec->height;
            bih->biBitCount= codec->bits_per_coded_sample;
            bih->biSizeImage = bih->biWidth * bih->biHeight * bih->biBitCount/8;
            bih->biCompression= codec->codec_tag;
            sh_video->bih= bih;
            sh_video->disp_w= codec->width;
            sh_video->disp_h= codec->height;
            if (st->time_base.den) { /* if container has time_base, use that */
                sh_video->video.dwRate= st->time_base.den;
                sh_video->video.dwScale= st->time_base.num;
            } else {
                sh_video->video.dwRate= codec->time_base.den;
                sh_video->video.dwScale= codec->time_base.num;
            }
            sh_video->fps=av_q2d(st->r_frame_rate);
            sh_video->frametime=1/av_q2d(st->r_frame_rate);
            sh_video->format=bih->biCompression;
            if(st->sample_aspect_ratio.num)
                sh_video->aspect = codec->width  * st->sample_aspect_ratio.num
                         / (float)(codec->height * st->sample_aspect_ratio.den);
            else
                sh_video->aspect=codec->width  * codec->sample_aspect_ratio.num
                       / (float)(codec->height * codec->sample_aspect_ratio.den);
            sh_video->i_bps=codec->bit_rate/8;
            if (title && title->value)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VID_%d_NAME=%s\n", priv->video_streams, title->value);
            mp_msg(MSGT_DEMUX,MSGL_DBG2,"aspect= %d*%d/(%d*%d)\n",
                codec->width, codec->sample_aspect_ratio.num,
                codec->height, codec->sample_aspect_ratio.den);

            sh_video->ds= demuxer->video;
            if(codec->extradata_size)
                memcpy(sh_video->bih + 1, codec->extradata, codec->extradata_size);
            if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_video_header(sh_video->bih, MSGL_V);
            /*
                short biPlanes;
                int  biXPelsPerMeter;
                int  biYPelsPerMeter;
                int biClrUsed;
                int biClrImportant;
            */
            if(demuxer->video->id != i && demuxer->video->id != -1)
                st->discard= AVDISCARD_ALL;
            else{
                demuxer->video->id = i;
                demuxer->video->sh= demuxer->v_streams[i];
            }
            stream_id = priv->video_streams++;
            break;
        }
        case CODEC_TYPE_SUBTITLE:{
            sh_sub_t* sh_sub;
            char type;
            /* only support text subtitles for now */
            if(codec->codec_id == CODEC_ID_TEXT)
                type = 't';
            else if(codec->codec_id == CODEC_ID_MOV_TEXT)
                type = 'm';
            else if(codec->codec_id == CODEC_ID_SSA)
                type = 'a';
            else if(codec->codec_id == CODEC_ID_DVD_SUBTITLE)
                type = 'v';
            else if(codec->codec_id == CODEC_ID_XSUB)
                type = 'x';
            else if(codec->codec_id == CODEC_ID_DVB_SUBTITLE)
                type = 'b';
            else if(codec->codec_id == CODEC_ID_DVB_TELETEXT)
                type = 'd';
            else if(codec->codec_id == CODEC_ID_HDMV_PGS_SUBTITLE)
                type = 'p';
            else
                break;
            sh_sub = new_sh_sub_sid(demuxer, i, priv->sub_streams);
            if(!sh_sub) break;
            stream_type = "subtitle";
            priv->sstreams[priv->sub_streams] = i;
            sh_sub->type = type;
            if (codec->extradata_size) {
                sh_sub->extradata = malloc(codec->extradata_size);
                memcpy(sh_sub->extradata, codec->extradata, codec->extradata_size);
                sh_sub->extradata_len = codec->extradata_size;
            }
            if (title && title->value)
                mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_NAME=%s\n", priv->sub_streams, title->value);
            if (lang && lang->value) {
              sh_sub->lang = strdup(lang->value);
              mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_LANG=%s\n", priv->sub_streams, sh_sub->lang);
            }
            if (st->disposition & AV_DISPOSITION_DEFAULT)
              sh_sub->default_track = 1;
            stream_id = priv->sub_streams++;
            break;
        }
        case CODEC_TYPE_ATTACHMENT:{
            if (st->codec->codec_id == CODEC_ID_TTF)
                demuxer_add_attachment(demuxer, BSTR(st->filename),
                                       BSTR("application/x-truetype-font"),
                                       (struct bstr){codec->extradata,
                                                     codec->extradata_size});
            break;
        }
        default:
            st->discard= AVDISCARD_ALL;
    }
    if (stream_type) {
        AVCodec *avc = avcodec_find_decoder(codec->codec_id);
        const char *codec_name = avc ? avc->name : "unknown";
        if (!avc && *stream_type == 's' && demuxer->s_streams[stream_id])
            codec_name = sh_sub_type2str(((sh_sub_t *)demuxer->s_streams[stream_id])->type);
        mp_msg(MSGT_DEMUX, MSGL_INFO, "[lavf] stream %d: %s (%s), -%cid %d", i, stream_type, codec_name, *stream_type, stream_id);
        if (lang && lang->value && *stream_type != 'v')
            mp_msg(MSGT_DEMUX, MSGL_INFO, ", -%clang %s", *stream_type, lang->value);
        if (title && title->value)
            mp_msg(MSGT_DEMUX, MSGL_INFO, ", %s", title->value);
        mp_msg(MSGT_DEMUX, MSGL_INFO, "\n");
    }
}

static demuxer_t* demux_open_lavf(demuxer_t *demuxer){
    struct MPOpts *opts = demuxer->opts;
    struct lavfdopts *lavfdopts = &opts->lavfdopts;
    AVFormatContext *avfc;
    AVFormatParameters ap;
    const AVOption *opt;
    AVMetadataTag *t = NULL;
    lavf_priv_t *priv= demuxer->priv;
    int i;
    char mp_filename[256]="mp:";

    memset(&ap, 0, sizeof(AVFormatParameters));

    stream_seek(demuxer->stream, 0);

    avfc = avformat_alloc_context();

    if (lavfdopts->cryptokey)
        parse_cryptokey(avfc, lavfdopts->cryptokey);
    if (matches_avinputformat_name(priv, "avi")) {
        /* for avi libavformat returns the avi timestamps in .dts,
         * some made-up stuff that's not really pts in .pts */
        priv->use_dts = true;
        demuxer->timestamp_type = TIMESTAMP_TYPE_SORT;
    } else {
        if (opts->user_correct_pts != 0)
            avfc->flags |= AVFMT_FLAG_GENPTS;
    }
    if (index_mode == 0)
        avfc->flags |= AVFMT_FLAG_IGNIDX;

    ap.prealloced_context = 1;
    if (lavfdopts->probesize) {
        opt = av_set_int(avfc, "probesize", lavfdopts->probesize);
        if(!opt) mp_msg(MSGT_HEADER,MSGL_ERR, "demux_lavf, couldn't set option probesize to %u\n", lavfdopts->probesize);
    }
    if (lavfdopts->analyzeduration) {
        opt = av_set_int(avfc, "analyzeduration",
                         lavfdopts->analyzeduration * AV_TIME_BASE);
        if (!opt)
            mp_msg(MSGT_HEADER, MSGL_ERR, "demux_lavf, couldn't set option "
                   "analyzeduration to %u\n", lavfdopts->analyzeduration);
    }

    if (lavfdopts->avopt){
        if(parse_avopts(avfc, lavfdopts->avopt) < 0){
            mp_msg(MSGT_HEADER,MSGL_ERR, "Your options /%s/ look like gibberish to me pal\n", lavfdopts->avopt);
            return NULL;
        }
    }

    if(demuxer->stream->url) {
        if (!strncmp(demuxer->stream->url, "ffmpeg://rtsp:", 14))
            av_strlcpy(mp_filename, demuxer->stream->url + 9, sizeof(mp_filename));
        else
            av_strlcat(mp_filename, demuxer->stream->url, sizeof(mp_filename));
    } else
        av_strlcat(mp_filename, "foobar.dummy", sizeof(mp_filename));

    priv->pb = av_alloc_put_byte(priv->buffer, BIO_BUFFER_SIZE, 0,
                                 demuxer, mp_read, NULL, mp_seek);
    priv->pb->read_seek = mp_read_seek;
    priv->pb->is_streamed = !demuxer->stream->end_pos || (demuxer->stream->flags & MP_STREAM_SEEK) != MP_STREAM_SEEK;

    if(av_open_input_stream(&avfc, priv->pb, mp_filename, priv->avif, &ap)<0){
        mp_msg(MSGT_HEADER,MSGL_ERR,"LAVF_header: av_open_input_stream() failed\n");
        return NULL;
    }

    priv->avfc= avfc;

    if(av_find_stream_info(avfc) < 0){
        mp_msg(MSGT_HEADER,MSGL_ERR,"LAVF_header: av_find_stream_info() failed\n");
        return NULL;
    }

    /* Add metadata. */
    av_metadata_conv(avfc, NULL, avfc->iformat->metadata_conv);
    while((t = av_metadata_get(avfc->metadata, "", t, AV_METADATA_IGNORE_SUFFIX)))
        demux_info_add(demuxer, t->key, t->value);

    for(i=0; i < avfc->nb_chapters; i++) {
        AVChapter *c = avfc->chapters[i];
        uint64_t start = av_rescale_q(c->start, c->time_base, (AVRational){1,1000000000});
        uint64_t end   = av_rescale_q(c->end, c->time_base, (AVRational){1,1000000000});
        t = av_metadata_get(c->metadata, "title", NULL, 0);
        demuxer_add_chapter(demuxer, t ? BSTR(t->value) : BSTR(NULL), start, end);
    }

    for(i=0; i<avfc->nb_streams; i++)
        handle_stream(demuxer, avfc, i);
    priv->nb_streams_last = avfc->nb_streams;

    if(avfc->nb_programs) {
        int p;
        for (p = 0; p < avfc->nb_programs; p++) {
            AVProgram *program = avfc->programs[p];
            t = av_metadata_get(program->metadata, "title", NULL, 0);
            mp_msg(MSGT_HEADER,MSGL_INFO,"LAVF: Program %d %s\n", program->id, t ? t->value : "");
            mp_msg(MSGT_IDENTIFY, MSGL_V, "PROGRAM_ID=%d\n", program->id);
        }
    }

    mp_msg(MSGT_HEADER,MSGL_V,"LAVF: %d audio and %d video streams found\n",priv->audio_streams,priv->video_streams);
    mp_msg(MSGT_HEADER,MSGL_V,"LAVF: build %d\n", LIBAVFORMAT_BUILD);
    if(!priv->audio_streams) demuxer->audio->id=-2;  // nosound
//    else if(best_audio > 0 && demuxer->audio->id == -1) demuxer->audio->id=best_audio;
    if(!priv->video_streams){
        if(!priv->audio_streams){
	    mp_msg(MSGT_HEADER,MSGL_ERR,"LAVF: no audio or video headers found - broken file?\n");
            return NULL;
        }
        demuxer->video->id=-2; // audio-only
    } //else if (best_video > 0 && demuxer->video->id == -1) demuxer->video->id = best_video;

    /* libavformat sets bitrate for mpeg based on pts at start and end
     * of file, which fails for files with pts resets. So calculate our
     * own bitrate estimate. */
    if (priv->avif->flags & AVFMT_TS_DISCONT) {
        for (int i = 0; i < avfc->nb_streams; i++)
            priv->bitrate += avfc->streams[i]->codec->bit_rate;
        /* pts-based is more accurate if there are no resets; try to make
         * a somewhat reasonable guess */
        if (!avfc->duration || avfc->duration == AV_NOPTS_VALUE
            || priv->bitrate && (avfc->bit_rate < priv->bitrate / 2
                                 || avfc->bit_rate > priv->bitrate * 2))
            priv->seek_by_bytes = true;
        if (!priv->bitrate)
            priv->bitrate = 1440000;
    }
    demuxer->accurate_seek = !priv->seek_by_bytes;

    return demuxer;
}

static void check_internet_radio_hack(struct demuxer *demuxer)
{
    struct lavf_priv *priv = demuxer->priv;
    struct AVFormatContext *avfc = priv->avfc;

    if (!matches_avinputformat_name(priv, "ogg"))
        return;
    if (priv->nb_streams_last == avfc->nb_streams)
        return;
    if (avfc->nb_streams - priv->nb_streams_last == 1
        && priv->video_streams == 0 && priv->sub_streams == 0
        && demuxer->a_streams[priv->audio_streams-1]->format == 0x566f // vorbis
        && (priv->audio_streams == 2 || priv->internet_radio_hack)
        && demuxer->a_streams[0]->format == 0x566f) {
        // extradata match could be checked but would require parsing
        // headers, as the comment section will vary
        if (!priv->internet_radio_hack) {
            mp_msg(MSGT_DEMUX, MSGL_V,
                   "[lavf] enabling internet ogg radio hack\n");
#if LIBAVFORMAT_VERSION_MAJOR < 53
            mp_tmsg(MSGT_DEMUX, MSGL_WARN, "[lavf] This looks like an "
                    "internet radio ogg stream with track changes.\n"
                    "Playback will likely fail after %d track changes "
                    "due to libavformat limitations.\n"
                    "You may be able to work around that limitation by "
                    "using -demuxer ogg.\n", MAX_STREAMS);
#endif
        }
#if LIBAVFORMAT_VERSION_MAJOR < 53
        if (avfc->nb_streams == MAX_STREAMS) {
            mp_tmsg(MSGT_DEMUX, MSGL_WARN, "[lavf] This is the %dth "
                    "track.\nPlayback will likely fail at the next change.\n"
                    "You may be able to work around this limitation by "
                    "using -demuxer ogg.\n", MAX_STREAMS);
        }
#endif
        priv->internet_radio_hack = true;
        // use new per-track metadata as global metadata
        AVMetadataTag *t = NULL;
        AVStream *stream = avfc->streams[avfc->nb_streams - 1];
        while ((t = av_metadata_get(stream->metadata, "", t,
                                   AV_METADATA_IGNORE_SUFFIX)))
            demux_info_add(demuxer, t->key, t->value);
    } else {
        if (priv->internet_radio_hack)
            mp_tmsg(MSGT_DEMUX, MSGL_WARN, "[lavf] Internet radio ogg hack "
                    "was enabled, but stream characteristics changed.\n"
                    "This may or may not work.\n");
        priv->internet_radio_hack = false;
    }
}

static int demux_lavf_fill_buffer(demuxer_t *demux, demux_stream_t *dsds){
    lavf_priv_t *priv= demux->priv;
    AVPacket pkt;
    demux_packet_t *dp;
    demux_stream_t *ds;
    int id;
    mp_msg(MSGT_DEMUX,MSGL_DBG2,"demux_lavf_fill_buffer()\n");

    demux->filepos=stream_tell(demux->stream);

    if(av_read_frame(priv->avfc, &pkt) < 0)
        return 0;

    // handle any new streams that might have been added
    for (id = priv->nb_streams_last; id < priv->avfc->nb_streams; id++)
        handle_stream(demux, priv->avfc, id);
    check_internet_radio_hack(demux);

    priv->nb_streams_last = priv->avfc->nb_streams;

    id= pkt.stream_index;

    if (id == demux->audio->id || priv->internet_radio_hack) {
        // audio
        ds=demux->audio;
        if(!ds->sh){
            ds->sh=demux->a_streams[id];
            mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected LAVF audio ID = %d\n",ds->id);
        }
    } else if(id==demux->video->id){
        // video
        ds=demux->video;
        if(!ds->sh){
            ds->sh=demux->v_streams[id];
            mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected LAVF video ID = %d\n",ds->id);
        }
    } else if(id==demux->sub->id){
        // subtitle
        ds=demux->sub;
        sub_utf8=1;
    } else {
        av_free_packet(&pkt);
        return 1;
    }

    if(0/*pkt.destruct == av_destruct_packet*/){
        //ok kids, dont try this at home :)
        dp=malloc(sizeof(demux_packet_t));
        dp->len=pkt.size;
        dp->next=NULL;
        dp->refcount=1;
        dp->master=NULL;
        dp->buffer=pkt.data;
        pkt.destruct= NULL;
    }else{
        dp=new_demux_packet(pkt.size);
        memcpy(dp->buffer, pkt.data, pkt.size);
        av_free_packet(&pkt);
    }

    int64_t ts = priv->use_dts ? pkt.dts : pkt.pts;
    if(ts != AV_NOPTS_VALUE){
        dp->pts = ts * av_q2d(priv->avfc->streams[id]->time_base);
        priv->last_pts= dp->pts * AV_TIME_BASE;
        // always set duration for subtitles, even if PKT_FLAG_KEY is not set,
        // otherwise they will stay on screen to long if e.g. ASS is demuxed from mkv
        if((ds == demux->sub || (pkt.flags & PKT_FLAG_KEY)) &&
           pkt.convergence_duration > 0)
            dp->duration = pkt.convergence_duration * av_q2d(priv->avfc->streams[id]->time_base);
    }
    dp->pos=demux->filepos;
    dp->flags= !!(pkt.flags&PKT_FLAG_KEY);
    // append packet to DS stream:
    ds_add_packet(ds,dp);
    return 1;
}

static void demux_seek_lavf(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags){
    lavf_priv_t *priv = demuxer->priv;
    int avsflags = 0;
    mp_msg(MSGT_DEMUX,MSGL_DBG2,"demux_seek_lavf(%p, %f, %f, %d)\n", demuxer, rel_seek_secs, audio_delay, flags);

    if (priv->seek_by_bytes) {
        int64_t pos = demuxer->filepos;
        rel_seek_secs *= priv->bitrate / 8;
        pos += rel_seek_secs;
        av_seek_frame(priv->avfc, -1, pos, AVSEEK_FLAG_BYTE);
        return;
    }

    if (flags & SEEK_ABSOLUTE) {
      priv->last_pts = 0;
    } else {
      if (rel_seek_secs < 0) avsflags = AVSEEK_FLAG_BACKWARD;
    }
    if (flags & SEEK_FORWARD)
        avsflags = 0;
    else if (flags & SEEK_BACKWARD)
        avsflags = AVSEEK_FLAG_BACKWARD;
    if (flags & SEEK_FACTOR) {
      if (priv->avfc->duration == 0 || priv->avfc->duration == AV_NOPTS_VALUE)
        return;
      priv->last_pts += rel_seek_secs * priv->avfc->duration;
    } else {
      priv->last_pts += rel_seek_secs * AV_TIME_BASE;
    }
    if (av_seek_frame(priv->avfc, -1, priv->last_pts, avsflags) < 0) {
        avsflags ^= AVSEEK_FLAG_BACKWARD;
        av_seek_frame(priv->avfc, -1, priv->last_pts, avsflags);
    }
}

static int demux_lavf_control(demuxer_t *demuxer, int cmd, void *arg)
{
    lavf_priv_t *priv = demuxer->priv;

    switch (cmd) {
        case DEMUXER_CTRL_CORRECT_PTS:
	    return DEMUXER_CTRL_OK;
    case DEMUXER_CTRL_GET_TIME_LENGTH:
        if (priv->seek_by_bytes) {
            /* Our bitrate estimate may be better than would be used in
             * otherwise similar fallback code at higher level */
            if (demuxer->movi_end <= 0)
                return DEMUXER_CTRL_DONTKNOW;
            *(double *)arg = (demuxer->movi_end - demuxer->movi_start) * 8 /
                priv->bitrate;
            return DEMUXER_CTRL_GUESS;
        }
	    if (priv->avfc->duration == 0 || priv->avfc->duration == AV_NOPTS_VALUE)
	        return DEMUXER_CTRL_DONTKNOW;

	    *((double *)arg) = (double)priv->avfc->duration / AV_TIME_BASE;
	    return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_GET_PERCENT_POS:
        if (priv->seek_by_bytes)
            return DEMUXER_CTRL_DONTKNOW; // let it use the fallback code
	    if (priv->avfc->duration == 0 || priv->avfc->duration == AV_NOPTS_VALUE)
	        return DEMUXER_CTRL_DONTKNOW;

	    *((int *)arg) = (int)((priv->last_pts - priv->avfc->start_time)*100 / priv->avfc->duration);
	    return DEMUXER_CTRL_OK;
	case DEMUXER_CTRL_SWITCH_AUDIO:
	case DEMUXER_CTRL_SWITCH_VIDEO:
	{
	    int id = *((int*)arg);
	    int newid = -2;
	    int i, curridx = -1;
	    int nstreams, *pstreams;
	    demux_stream_t *ds;

	    if(cmd == DEMUXER_CTRL_SWITCH_VIDEO)
	    {
	        ds = demuxer->video;
	        nstreams = priv->video_streams;
	        pstreams = priv->vstreams;
	    }
	    else
	    {
	        ds = demuxer->audio;
	        nstreams = priv->audio_streams;
	        pstreams = priv->astreams;
	    }
	    for(i = 0; i < nstreams; i++)
	    {
	        if(pstreams[i] == ds->id) //current stream id
	        {
	            curridx = i;
	            break;
	        }
	    }

            if(id == -2) { // no sound
                i = -1;
            } else if(id == -1) { // next track
                i = (curridx + 2) % (nstreams + 1) - 1;
                if (i >= 0)
                    newid = pstreams[i];
	    }
	    else // select track by id
	    {
	        if (id >= 0 && id < nstreams) {
	            i = id;
	            newid = pstreams[i];
	        }
	    }
	    if (i == curridx) {
                *(int *) arg = curridx;
                return DEMUXER_CTRL_OK;
            } else {
	        ds_free_packs(ds);
	        if(ds->id >= 0)
	            priv->avfc->streams[ds->id]->discard = AVDISCARD_ALL;
                ds->id = newid;
                *(int *) arg = i < 0 ? -2 : i;
	        if(newid >= 0)
	            priv->avfc->streams[newid]->discard = AVDISCARD_NONE;
	        return DEMUXER_CTRL_OK;
	    }
        }
        case DEMUXER_CTRL_IDENTIFY_PROGRAM:
        {
            demux_program_t *prog = arg;
            AVProgram *program;
            int p, i;
            int start;

            prog->vid = prog->aid = prog->sid = -2;	//no audio and no video by default
            if(priv->avfc->nb_programs < 1)
                return DEMUXER_CTRL_DONTKNOW;

            if(prog->progid == -1)
            {
                p = 0;
                while(p<priv->avfc->nb_programs && priv->avfc->programs[p]->id != priv->cur_program)
                    p++;
                p = (p + 1) % priv->avfc->nb_programs;
            }
            else
            {
                for(i=0; i<priv->avfc->nb_programs; i++)
                    if(priv->avfc->programs[i]->id == prog->progid)
                        break;
                if(i==priv->avfc->nb_programs)
                    return DEMUXER_CTRL_DONTKNOW;
                p = i;
            }
            start = p;
redo:
            program = priv->avfc->programs[p];
            for(i=0; i<program->nb_stream_indexes; i++)
            {
                switch(priv->avfc->streams[program->stream_index[i]]->codec->codec_type)
                {
                    case CODEC_TYPE_VIDEO:
                        if(prog->vid == -2)
                            prog->vid = program->stream_index[i];
                        break;
                    case CODEC_TYPE_AUDIO:
                        if(prog->aid == -2)
                            prog->aid = program->stream_index[i];
                        break;
                    case CODEC_TYPE_SUBTITLE:
                        if(prog->sid == -2 && priv->avfc->streams[program->stream_index[i]]->codec->codec_id == CODEC_ID_TEXT)
                            prog->sid = program->stream_index[i];
                        break;
                }
            }
            if (prog->aid >= 0 && prog->aid < MAX_A_STREAMS &&
                demuxer->a_streams[prog->aid]) {
                sh_audio_t *sh = demuxer->a_streams[prog->aid];
                prog->aid = sh->aid;
            } else
                prog->aid = -2;
            if (prog->vid >= 0 && prog->vid < MAX_V_STREAMS &&
                demuxer->v_streams[prog->vid]) {
                sh_video_t *sh = demuxer->v_streams[prog->vid];
                prog->vid = sh->vid;
            } else
                prog->vid = -2;
            if(prog->progid == -1 && prog->vid == -2 && prog->aid == -2)
            {
                p = (p + 1) % priv->avfc->nb_programs;
                if (p == start)
                    return DEMUXER_CTRL_DONTKNOW;
                goto redo;
            }
            priv->cur_program = prog->progid = program->id;
            return DEMUXER_CTRL_OK;
        }
	default:
	    return DEMUXER_CTRL_NOTIMPL;
    }
}

static void demux_close_lavf(demuxer_t *demuxer)
{
    lavf_priv_t* priv = demuxer->priv;
    if (priv){
        if(priv->avfc)
       {
         av_freep(&priv->avfc->key);
         av_close_input_stream(priv->avfc);
        }
        av_freep(&priv->pb);
        free(priv); demuxer->priv= NULL;
    }
}


const demuxer_desc_t demuxer_desc_lavf = {
  "libavformat demuxer",
  "lavf",
  "libavformat",
  "Michael Niedermayer",
  "supports many formats, requires libavformat",
  DEMUXER_TYPE_LAVF,
  0, // Check after other demuxer
  lavf_check_file,
  demux_lavf_fill_buffer,
  demux_open_lavf,
  demux_close_lavf,
  demux_seek_lavf,
  demux_lavf_control
};

const demuxer_desc_t demuxer_desc_lavf_preferred = {
  "libavformat preferred demuxer",
  "lavfpref",
  "libavformat",
  "Michael Niedermayer",
  "supports many formats, requires libavformat",
  DEMUXER_TYPE_LAVF_PREFERRED,
  1,
  lavf_check_preferred_file,
  demux_lavf_fill_buffer,
  demux_open_lavf,
  demux_close_lavf,
  demux_seek_lavf,
  demux_lavf_control
};
