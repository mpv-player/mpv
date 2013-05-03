/*
 * DEMUXER v2.5
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "core/options.h"
#include "core/av_common.h"
#include "talloc.h"
#include "core/mp_msg.h"

#include "stream/stream.h"
#include "demux.h"
#include "stheader.h"
#include "mf.h"

#include "audio/format.h"

#include "libavcodec/avcodec.h"
#if MP_INPUT_BUFFER_PADDING_SIZE < FF_INPUT_BUFFER_PADDING_SIZE
#error MP_INPUT_BUFFER_PADDING_SIZE is too small!
#endif

static void clear_parser(sh_audio_t *sh);

// Demuxer list
extern const struct demuxer_desc demuxer_desc_edl;
extern const struct demuxer_desc demuxer_desc_cue;
extern const demuxer_desc_t demuxer_desc_rawaudio;
extern const demuxer_desc_t demuxer_desc_rawvideo;
extern const demuxer_desc_t demuxer_desc_tv;
extern const demuxer_desc_t demuxer_desc_mf;
extern const demuxer_desc_t demuxer_desc_avi;
extern const demuxer_desc_t demuxer_desc_asf;
extern const demuxer_desc_t demuxer_desc_matroska;
extern const demuxer_desc_t demuxer_desc_lavf;
extern const demuxer_desc_t demuxer_desc_mng;
extern const demuxer_desc_t demuxer_desc_mpeg_ps;
extern const demuxer_desc_t demuxer_desc_mpeg_pes;
extern const demuxer_desc_t demuxer_desc_mpeg_gxf;
extern const demuxer_desc_t demuxer_desc_mpeg_es;
extern const demuxer_desc_t demuxer_desc_mpeg4_es;
extern const demuxer_desc_t demuxer_desc_h264_es;
extern const demuxer_desc_t demuxer_desc_mpeg_ts;

/* Please do not add any new demuxers here. If you want to implement a new
 * demuxer, add it to libavformat, except for wrappers around external
 * libraries and demuxers requiring binary support. */

const demuxer_desc_t *const demuxer_list[] = {
    &demuxer_desc_edl,
    &demuxer_desc_cue,
    &demuxer_desc_rawaudio,
    &demuxer_desc_rawvideo,
#ifdef CONFIG_TV
    &demuxer_desc_tv,
#endif
    &demuxer_desc_matroska,
    &demuxer_desc_lavf,
    &demuxer_desc_avi,
    &demuxer_desc_asf,
#ifdef CONFIG_MNG
    &demuxer_desc_mng,
#endif
    &demuxer_desc_mpeg_ps,
    &demuxer_desc_mpeg_pes,
    &demuxer_desc_mpeg_gxf,
    &demuxer_desc_mpeg_es,
    &demuxer_desc_mpeg4_es,
    &demuxer_desc_h264_es,
    &demuxer_desc_mpeg_ts,
    // auto-probe last, because it checks file-extensions only
    &demuxer_desc_mf,
    /* Please do not add any new demuxers here. If you want to implement a new
     * demuxer, add it to libavformat, except for wrappers around external
     * libraries and demuxers requiring binary support. */
    NULL
};

static int packet_destroy(void *ptr)
{
    struct demux_packet *dp = ptr;
    if (dp->avpacket)
        talloc_free(dp->avpacket);
    else
        free(dp->buffer);
    return 0;
}

static struct demux_packet *create_packet(size_t len)
{
    if (len > 1000000000) {
        mp_msg(MSGT_DEMUXER, MSGL_FATAL, "Attempt to allocate demux packet "
               "over 1 GB!\n");
        abort();
    }
    struct demux_packet *dp = talloc(NULL, struct demux_packet);
    talloc_set_destructor(dp, packet_destroy);
    *dp = (struct demux_packet) {
        .len = len,
        .pts = MP_NOPTS_VALUE,
        .duration = -1,
        .stream_pts = MP_NOPTS_VALUE,
    };
    return dp;
}

struct demux_packet *new_demux_packet(size_t len)
{
    struct demux_packet *dp = create_packet(len);
    dp->buffer = malloc(len + MP_INPUT_BUFFER_PADDING_SIZE);
    if (!dp->buffer) {
        mp_msg(MSGT_DEMUXER, MSGL_FATAL, "Memory allocation failure!\n");
        abort();
    }
    memset(dp->buffer + len, 0, MP_INPUT_BUFFER_PADDING_SIZE);
    return dp;
}

// data must already have suitable padding, and does not copy the data
struct demux_packet *new_demux_packet_fromdata(void *data, size_t len)
{
    struct demux_packet *dp = create_packet(len);
    dp->buffer = data;
    return dp;
}

struct demux_packet *new_demux_packet_from(void *data, size_t len)
{
    struct demux_packet *dp = new_demux_packet(len);
    memcpy(dp->buffer, data, len);
    return dp;
}

void resize_demux_packet(struct demux_packet *dp, size_t len)
{
    if (len > 1000000000) {
        mp_msg(MSGT_DEMUXER, MSGL_FATAL, "Attempt to realloc demux packet "
               "over 1 GB!\n");
        abort();
    }
    dp->buffer = realloc(dp->buffer, len + MP_INPUT_BUFFER_PADDING_SIZE);
    if (!dp->buffer) {
        mp_msg(MSGT_DEMUXER, MSGL_FATAL, "Memory allocation failure!\n");
        abort();
    }
    memset(dp->buffer + len, 0, MP_INPUT_BUFFER_PADDING_SIZE);
    dp->len = len;
}

void free_demux_packet(struct demux_packet *dp)
{
    talloc_free(dp);
}

static void free_demuxer_stream(struct demux_stream *ds)
{
    ds_free_packs(ds);
    free(ds);
}

static struct demux_stream *new_demuxer_stream(struct demuxer *demuxer,
                                               enum stream_type type, int id)
{
    demux_stream_t *ds = malloc(sizeof(demux_stream_t));
    *ds = (demux_stream_t) {
        .stream_type = type,
        .id = id,
        .demuxer = demuxer,
        .asf_seq = -1,
    };
    return ds;
}

/**
 * Get demuxer description structure for a given demuxer type
 *
 * @param file_format    type of the demuxer
 * @return               structure for the demuxer, NULL if not found
 */
static const demuxer_desc_t *get_demuxer_desc_from_type(int file_format)
{
    int i;

    for (i = 0; demuxer_list[i]; i++)
        if (file_format == demuxer_list[i]->type)
            return demuxer_list[i];

    return NULL;
}


demuxer_t *new_demuxer(struct MPOpts *opts, stream_t *stream, int type,
                       int a_id, int v_id, int s_id, char *filename)
{
    struct demuxer *d = talloc_zero(NULL, struct demuxer);
    d->stream = stream;
    d->stream_pts = MP_NOPTS_VALUE;
    d->reference_clock = MP_NOPTS_VALUE;
    d->movi_start = stream->start_pos;
    d->movi_end = stream->end_pos;
    d->seekable = 1;
    d->synced = 0;
    d->filepos = -1;
    d->audio = new_demuxer_stream(d, STREAM_VIDEO, a_id);
    d->video = new_demuxer_stream(d, STREAM_AUDIO, v_id);
    d->sub = new_demuxer_stream(d, STREAM_SUB, s_id);
    d->ds[STREAM_VIDEO] = d->video;
    d->ds[STREAM_AUDIO] = d->audio;
    d->ds[STREAM_SUB] = d->sub;
    d->type = type;
    d->opts = opts;
    if (type)
        if (!(d->desc = get_demuxer_desc_from_type(type)))
            mp_msg(MSGT_DEMUXER, MSGL_ERR,
                   "BUG! Invalid demuxer type in new_demuxer(), "
                   "big troubles ahead.\n");
    if (filename) // Filename hack for avs_check_file
        d->filename = strdup(filename);
    stream_seek(stream, stream->start_pos);
    return d;
}

static struct sh_stream *new_sh_stream_id(demuxer_t *demuxer,
                                          enum stream_type type,
                                          int stream_index,
                                          int demuxer_id)
{
    if (demuxer->num_streams > MAX_SH_STREAMS || stream_index > MAX_SH_STREAMS) {
        mp_msg(MSGT_DEMUXER, MSGL_WARN, "Too many streams.");
        return NULL;
    }

    struct sh_stream *sh = talloc_struct(demuxer, struct sh_stream, {
        .type = type,
        .demuxer = demuxer,
        .index = demuxer->num_streams,
        .demuxer_id = demuxer_id, // may be overwritten by demuxer
        .stream_index = stream_index,
        .opts = demuxer->opts,
    });
    MP_TARRAY_APPEND(demuxer, demuxer->streams, demuxer->num_streams, sh);
    switch (sh->type) {
        case STREAM_VIDEO: {
            struct sh_video *sht = talloc_zero(demuxer, struct sh_video);
            sht->gsh = sh;
            sht->opts = sh->opts;
            sht->ds = demuxer->video;
            sh->video = sht;
            demuxer->v_streams[sh->stream_index] = sht;
            break;
        }
        case STREAM_AUDIO: {
            struct sh_audio *sht = talloc_zero(demuxer, struct sh_audio);
            sht->gsh = sh;
            sht->opts = sh->opts;
            sht->ds = demuxer->audio;
            sht->samplesize = 2;
            sht->sample_format = AF_FORMAT_S16_NE;
            sh->audio = sht;
            demuxer->a_streams[sh->stream_index] = sht;
            break;
        }
        case STREAM_SUB: {
            struct sh_sub *sht = talloc_zero(demuxer, struct sh_sub);
            sht->gsh = sh;
            sht->opts = sh->opts;
            sht->ds = demuxer->sub;
            sh->sub = sht;
            demuxer->s_streams[sh->stream_index] = sht;
            break;
        }
        default: assert(false);
    }
    return sh;
}

// This is what "modern" demuxers are supposed to use.
struct sh_stream *new_sh_stream(demuxer_t *demuxer, enum stream_type type)
{
    int num = 0;
    for (int n = 0; n < demuxer->num_streams; n++) {
        if (demuxer->streams[n]->type == type)
            num++;
    }
    return new_sh_stream_id(demuxer, type, demuxer->num_streams, num);
}

static void free_sh_stream(struct sh_stream *sh)
{
    if (sh->lav_headers) {
        avcodec_close(sh->lav_headers);
        av_free(sh->lav_headers);
    }
}

sh_sub_t *new_sh_sub_sid(demuxer_t *demuxer, int id, int sid)
{
    if (id > MAX_S_STREAMS - 1 || id < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_WARN,
               "Requested sub stream id overflow (%d > %d)\n", id,
               MAX_S_STREAMS);
        return NULL;
    }
    if (demuxer->s_streams[id])
        mp_msg(MSGT_DEMUXER, MSGL_WARN, "Sub stream %i redefined\n", id);
    else {
        new_sh_stream_id(demuxer, STREAM_SUB, id, sid);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SUBTITLE_ID=%d\n", sid);
    }
    return demuxer->s_streams[id];
}

struct sh_sub *new_sh_sub_sid_lang(struct demuxer *demuxer, int id, int sid,
                                   const char *lang)
{
    struct sh_sub *sh = new_sh_sub_sid(demuxer, id, sid);
    if (lang && lang[0] && strcmp(lang, "und")) {
        sh->gsh->lang = talloc_strdup(sh, lang);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_LANG=%s\n", sid, lang);
    }
    return sh;
}

static void free_sh_sub(sh_sub_t *sh)
{
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_sub at %p\n", sh);
    free(sh->extradata);
    free_sh_stream(sh->gsh);
}

sh_audio_t *new_sh_audio_aid(demuxer_t *demuxer, int id, int aid)
{
    if (id > MAX_A_STREAMS - 1 || id < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_WARN,
               "Requested audio stream id overflow (%d > %d)\n", id,
               MAX_A_STREAMS);
        return NULL;
    }
    if (demuxer->a_streams[id]) {
        mp_tmsg(MSGT_DEMUXER, MSGL_WARN, "WARNING: Audio stream header %d redefined.\n", id);
    } else {
        mp_tmsg(MSGT_DEMUXER, MSGL_V, "==> Found audio stream: %d\n", id);
        new_sh_stream_id(demuxer, STREAM_AUDIO, id, aid);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_ID=%d\n", aid);
    }
    return demuxer->a_streams[id];
}

static void free_sh_audio(demuxer_t *demuxer, int id)
{
    sh_audio_t *sh = demuxer->a_streams[id];
    demuxer->a_streams[id] = NULL;
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_audio at %p\n", sh);
    free(sh->wf);
    free(sh->codecdata);
    clear_parser(sh);
    free_sh_stream(sh->gsh);
}

sh_video_t *new_sh_video_vid(demuxer_t *demuxer, int id, int vid)
{
    if (id > MAX_V_STREAMS - 1 || id < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_WARN,
               "Requested video stream id overflow (%d > %d)\n", id,
               MAX_V_STREAMS);
        return NULL;
    }
    if (demuxer->v_streams[id])
        mp_tmsg(MSGT_DEMUXER, MSGL_WARN, "WARNING: Video stream header %d redefined.\n", id);
    else {
        mp_tmsg(MSGT_DEMUXER, MSGL_V, "==> Found video stream: %d\n", id);
        new_sh_stream_id(demuxer, STREAM_VIDEO, id, vid);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_ID=%d\n", vid);
    }
    return demuxer->v_streams[id];
}

static void free_sh_video(sh_video_t *sh)
{
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_video at %p\n", sh);
    free(sh->bih);
    free_sh_stream(sh->gsh);
}

void free_demuxer(demuxer_t *demuxer)
{
    int i;
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing %s demuxer at %p\n",
           demuxer->desc->shortdesc, demuxer);
    if (demuxer->desc->close)
        demuxer->desc->close(demuxer);
    // free streams:
    for (i = 0; i < MAX_A_STREAMS; i++)
        if (demuxer->a_streams[i])
            free_sh_audio(demuxer, i);
    for (i = 0; i < MAX_V_STREAMS; i++)
        if (demuxer->v_streams[i])
            free_sh_video(demuxer->v_streams[i]);
    for (i = 0; i < MAX_S_STREAMS; i++)
        if (demuxer->s_streams[i])
            free_sh_sub(demuxer->s_streams[i]);
    // free demuxers:
    free_demuxer_stream(demuxer->audio);
    free_demuxer_stream(demuxer->video);
    free_demuxer_stream(demuxer->sub);
    free(demuxer->filename);
    talloc_free(demuxer);
}

void demuxer_add_packet(demuxer_t *demuxer, struct sh_stream *stream,
                        demux_packet_t *dp)
{
    if (!demuxer_stream_is_selected(demuxer, stream)) {
        free_demux_packet(dp);
    } else {
        ds_add_packet(demuxer->ds[stream->type], dp);
    }
}

void ds_add_packet(demux_stream_t *ds, demux_packet_t *dp)
{
    // demux API can't handle 0-sized packets, but at least some vobsubs
    // generate them. Skipping them seems to work fine. Not skipping them will
    // stop demuxing with external vobsubs. See FATE sub/vobsub.{idx,sub} at
    // pts=185.91.
    if (dp->len == 0 && ds->stream_type == STREAM_SUB) {
        mp_dbg(MSGT_DEMUXER, MSGL_INFO, "Discarding empty subtitle packet.\n");
        free_demux_packet(dp);
        return;
    }

    // append packet to DS stream:
    ++ds->packs;
    ds->bytes += dp->len;
    if (ds->last) {
        // next packet in stream
        ds->last->next = dp;
        ds->last = dp;
    } else {
        // first packet in stream
        ds->first = ds->last = dp;
    }
    mp_dbg(MSGT_DEMUXER, MSGL_DBG2,
           "DEMUX: Append packet to %s, len=%d  pts=%5.3f  pos=%u  [packs: A=%d V=%d]\n",
           (ds == ds->demuxer->audio) ? "d_audio" : "d_video", dp->len,
           dp->pts, (unsigned int) dp->pos, ds->demuxer->audio->packs,
           ds->demuxer->video->packs);
}

static void allocate_parser(AVCodecContext **avctx, AVCodecParserContext **parser, const char *format)
{
    enum AVCodecID codec_id = mp_codec_to_av_codec_id(format);

    switch (codec_id) {
    case AV_CODEC_ID_AAC_LATM:
    case AV_CODEC_ID_AC3:
    case AV_CODEC_ID_EAC3:
    case AV_CODEC_ID_DTS:
    case AV_CODEC_ID_FLAC:
    case AV_CODEC_ID_MLP:
    case AV_CODEC_ID_MP3:
    case AV_CODEC_ID_MP2:
    case AV_CODEC_ID_TRUEHD:
        *avctx = avcodec_alloc_context3(NULL);
        if (!*avctx)
            return;
        *parser = av_parser_init(codec_id);
        if (!*parser)
            av_freep(avctx);
        break;
    default: ;
    }
}

static void get_parser(sh_audio_t *sh, AVCodecContext **avctx, AVCodecParserContext **parser)
{
    *avctx  = NULL;
    *parser = NULL;

    if (!sh || !sh->needs_parsing)
        return;

    *avctx  = sh->avctx;
    *parser = sh->parser;
    if (*parser)
        return;

    allocate_parser(avctx, parser, sh->gsh->codec);
    sh->avctx  = *avctx;
    sh->parser = *parser;
}

int ds_parse(demux_stream_t *ds, uint8_t **buffer, int *len, double pts, int64_t pos)
{
    AVCodecContext *avctx;
    AVCodecParserContext *parser;
    get_parser(ds->sh, &avctx, &parser);
    if (!parser)
        return *len;
    return av_parser_parse2(parser, avctx, buffer, len, *buffer, *len, pts, pts, pos);
}

static void clear_parser(sh_audio_t *sh)
{
    av_parser_close(sh->parser);
    sh->parser = NULL;
    av_freep(&sh->avctx);
}

void ds_clear_parser(demux_stream_t *ds)
{
    if (!ds->sh)
        return;
    clear_parser(ds->sh);
}

void ds_read_packet(demux_stream_t *ds, stream_t *stream, int len,
                    double pts, int64_t pos, bool keyframe)
{
    demux_packet_t *dp = new_demux_packet(len);
    len = stream_read(stream, dp->buffer, len);
    resize_demux_packet(dp, len);
    dp->pts = pts;
    dp->pos = pos;
    dp->keyframe = keyframe;
    // append packet to DS stream:
    ds_add_packet(ds, dp);
}

static bool demux_check_queue_full(demuxer_t *demux)
{
    int apacks = demux->audio ? demux->audio->packs : 0;
    int abytes = demux->audio ? demux->audio->bytes : 0;
    int vpacks = demux->video ? demux->video->packs : 0;
    int vbytes = demux->video ? demux->video->bytes : 0;

    if (apacks < MAX_PACKS && abytes < MAX_PACK_BYTES &&
        vpacks < MAX_PACKS && vbytes < MAX_PACK_BYTES)
        return false;

    if (!demux->warned_queue_overflow) {
        mp_tmsg(MSGT_DEMUXER, MSGL_ERR, "\nToo many packets in the demuxer "
                "packet queue (video: %d packets in %d bytes, audio: %d "
                "packets in %d bytes).\n", vpacks, vbytes, apacks, abytes);
        mp_tmsg(MSGT_DEMUXER, MSGL_HINT, "Maybe you are playing a non-"
                "interleaved stream/file or the codec failed?\nFor AVI files, "
                "try to force non-interleaved mode with the "
                "--demuxer=avi --avi-ni options.\n");
    }

    demux->warned_queue_overflow = true;

    return true;
}

// return value:
//     0 = EOF or no stream found or invalid type
//     1 = successfully read a packet

int demux_fill_buffer(demuxer_t *demux, demux_stream_t *ds)
{
    // Note: parameter 'ds' can be NULL!
    return demux->desc->fill_buffer(demux, ds);
}

// return value:
//     0 = EOF
//     1 = successful
int ds_fill_buffer(demux_stream_t *ds)
{
    demuxer_t *demux = ds->demuxer;
    if (ds->current)
        free_demux_packet(ds->current);
    ds->current = NULL;
    mp_dbg(MSGT_DEMUXER, MSGL_DBG3, "ds_fill_buffer (%s) called\n",
           ds == demux->audio ? "d_audio" : ds == demux->video ? "d_video" :
           ds == demux->sub   ? "d_sub"   : "unknown");
    while (1) {
        int apacks = demux->audio ? demux->audio->packs : 0;
        int vpacks = demux->video ? demux->video->packs : 0;
        if (ds->packs) {
            demux_packet_t *p = ds->first;
            // copy useful data:
            ds->buffer = p->buffer;
            ds->buffer_pos = 0;
            ds->buffer_size = p->len;
            ds->pos = p->pos;
            ds->dpos += p->len; // !!!
            ++ds->pack_no;
            if (p->pts != MP_NOPTS_VALUE) {
                ds->pts = p->pts;
                ds->pts_bytes = 0;
            }
            ds->pts_bytes += p->len;    // !!!
            if (p->stream_pts != MP_NOPTS_VALUE)
                demux->stream_pts = p->stream_pts;
            ds->keyframe = p->keyframe;
            // unlink packet:
            ds->bytes -= p->len;
            ds->current = p;
            ds->first = p->next;
            if (!ds->first)
                ds->last = NULL;
            --ds->packs;
            /* The code below can set ds->eof to 1 when another stream runs
             * out of buffer space. That makes sense because in that situation
             * the calling code should not count on being able to demux more
             * packets from this stream.
             * If however the situation improves and we're called again
             * despite the eof flag then it's better to clear it to avoid
             * weird behavior. */
            ds->eof = 0;
            ds->fill_count = 0;
            return 1;
        }
        // avoid buffering too far ahead in e.g. badly interleaved files
        // or when one stream is shorter, without breaking large audio
        // delay with well interleaved files.
        // This needs to be enough for at least 1 second of packets
        // since libavformat mov demuxer does not try to interleave
        // with more than 1s precision.
        if (ds->fill_count > 80)
            break;

        if (demux_check_queue_full(demux))
            break;

        if (!demux_fill_buffer(demux, ds)) {
            mp_dbg(MSGT_DEMUXER, MSGL_DBG2,
                   "ds_fill_buffer()->demux_fill_buffer() failed\n");
            break; // EOF
        }

        struct sh_video *sh_video = demux->video->sh;

        if (sh_video && sh_video->gsh->attached_picture) {
            if (demux->audio)
                ds->fill_count += demux->audio->packs - apacks;
            if (demux->video && demux->video->packs > vpacks)
                ds->fill_count++;
        }
    }
    ds->buffer_pos = ds->buffer_size = 0;
    ds->buffer = NULL;
    mp_msg(MSGT_DEMUXER, MSGL_V,
           "ds_fill_buffer: EOF reached (stream: %s)  \n",
           ds == demux->audio ? "audio" : "video");
    ds->eof = 1;
    return 0;
}

int demux_read_data(demux_stream_t *ds, unsigned char *mem, int len)
{
    int x;
    int bytes = 0;
    while (len > 0) {
        x = ds->buffer_size - ds->buffer_pos;
        if (x == 0) {
            if (!ds_fill_buffer(ds))
                return bytes;
        } else {
            if (x > len)
                x = len;
            if (mem)
                memcpy(mem + bytes, &ds->buffer[ds->buffer_pos], x);
            bytes += x;
            len -= x;
            ds->buffer_pos += x;
        }
    }
    return bytes;
}

/**
 * \brief read data until the given 3-byte pattern is encountered, up to maxlen
 * \param mem memory to read data into, may be NULL to discard data
 * \param maxlen maximum number of bytes to read
 * \param read number of bytes actually read
 * \param pattern pattern to search for (lowest 8 bits are ignored)
 * \return whether pattern was found
 */
int demux_pattern_3(demux_stream_t *ds, unsigned char *mem, int maxlen,
                    int *read, uint32_t pattern)
{
    register uint32_t head = 0xffffff00;
    register uint32_t pat = pattern & 0xffffff00;
    int total_len = 0;
    do {
        register unsigned char *ds_buf = &ds->buffer[ds->buffer_size];
        int len = ds->buffer_size - ds->buffer_pos;
        register long pos = -len;
        if (unlikely(pos >= 0)) { // buffer is empty
            ds_fill_buffer(ds);
            continue;
        }
        do {
            head |= ds_buf[pos];
            head <<= 8;
        } while (++pos && head != pat);
        len += pos;
        if (total_len + len > maxlen)
            len = maxlen - total_len;
        len = demux_read_data(ds, mem ? &mem[total_len] : NULL, len);
        total_len += len;
    } while ((head != pat || total_len < 3) && total_len < maxlen && !ds->eof);
    if (read)
        *read = total_len;
    return total_len >= 3 && head == pat;
}

void ds_free_packs(demux_stream_t *ds)
{
    demux_packet_t *dp = ds->first;
    while (dp) {
        demux_packet_t *dn = dp->next;
        free_demux_packet(dp);
        dp = dn;
    }
    if (ds->asf_packet) {
        // free unfinished .asf fragments:
        free(ds->asf_packet->buffer);
        free(ds->asf_packet);
        ds->asf_packet = NULL;
    }
    ds->first = ds->last = NULL;
    ds->packs = 0; // !!!!!
    ds->bytes = 0;
    if (ds->current)
        free_demux_packet(ds->current);
    ds->current = NULL;
    ds->buffer = NULL;
    ds->buffer_pos = ds->buffer_size;
    ds->pts = MP_NOPTS_VALUE;
    ds->pts_bytes = 0;
}

int ds_get_packet(demux_stream_t *ds, unsigned char **start)
{
    int len;
    if (ds->buffer_pos >= ds->buffer_size) {
        if (!ds_fill_buffer(ds)) {
            // EOF
            *start = NULL;
            return -1;
        }
    }
    len = ds->buffer_size - ds->buffer_pos;
    *start = &ds->buffer[ds->buffer_pos];
    ds->buffer_pos += len;
    return len;
}

int ds_get_packet_pts(demux_stream_t *ds, unsigned char **start, double *pts)
{
    int len;
    *pts = MP_NOPTS_VALUE;
    len = ds_get_packet(ds, start);
    if (len < 0)
        return len;
    // Return pts unless this read starts from the middle of a packet
    if (len == ds->buffer_pos)
        *pts = ds->current->pts;
    return len;
}

int ds_get_packet_sub(demux_stream_t *ds, unsigned char **start)
{
    int len;
    if (ds->buffer_pos >= ds->buffer_size) {
        *start = NULL;
        if (!ds->packs)
            return -1;  // no sub
        if (!ds_fill_buffer(ds))
            return -1;  // EOF
    }
    len = ds->buffer_size - ds->buffer_pos;
    *start = &ds->buffer[ds->buffer_pos];
    ds->buffer_pos += len;
    return len;
}

struct demux_packet *ds_get_packet2(struct demux_stream *ds, bool repeat_last)
{
    // This shouldn't get used together with partial reads
    assert(ds->buffer_pos == 0 || ds->buffer_pos >= ds->buffer_size);
    if (!repeat_last)
        ds_fill_buffer(ds);
    ds->buffer_pos = ds->buffer_size;
    return ds->current;
}

double ds_get_next_pts(demux_stream_t *ds)
{
    demuxer_t *demux = ds->demuxer;
    // if we have not read from the "current" packet, consider it
    // as the next, otherwise we never get the pts for the first packet.
    while (!ds->first && (!ds->current || ds->buffer_pos)) {
        if (demux_check_queue_full(demux))
            return MP_NOPTS_VALUE;
        if (!demux_fill_buffer(demux, ds))
            return MP_NOPTS_VALUE;
    }
    // take pts from "current" if we never read from it.
    if (ds->current && !ds->buffer_pos)
        return ds->current->pts;
    return ds->first->pts;
}

// ====================================================================

void demuxer_help(void)
{
    int i;

    mp_msg(MSGT_DEMUXER, MSGL_INFO, "Available demuxers:\n");
    mp_msg(MSGT_DEMUXER, MSGL_INFO, " demuxer:   info:  (comment)\n");
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_DEMUXERS\n");
    for (i = 0; demuxer_list[i]; i++) {
        if (demuxer_list[i]->type >= DEMUXER_TYPE_END)  // internal type
            continue;
        if (demuxer_list[i]->comment && strlen(demuxer_list[i]->comment))
            mp_msg(MSGT_DEMUXER, MSGL_INFO, "%10s  %s (%s)\n",
                   demuxer_list[i]->name, demuxer_list[i]->info,
                   demuxer_list[i]->comment);
        else
            mp_msg(MSGT_DEMUXER, MSGL_INFO, "%10s  %s\n",
                   demuxer_list[i]->name, demuxer_list[i]->info);
    }
}


/**
 * Get demuxer type for a given demuxer name
 *
 * @param demuxer_name    string with demuxer name of demuxer number
 * @param force           will be set if demuxer should be forced.
 *                        May be NULL.
 * @return                DEMUXER_TYPE_xxx, -1 if error or not found
 */
static int get_demuxer_type_from_name(char *demuxer_name, int *force)
{
    if (!demuxer_name || !demuxer_name[0])
        return DEMUXER_TYPE_UNKNOWN;
    if (force)
        *force = demuxer_name[0] == '+';
    if (demuxer_name[0] == '+')
        demuxer_name = &demuxer_name[1];
    for (int i = 0; demuxer_list[i]; i++) {
        if (demuxer_list[i]->type >= DEMUXER_TYPE_END)
            // Can't select special demuxers from commandline
            continue;
        if (strcmp(demuxer_name, demuxer_list[i]->name) == 0)
            return demuxer_list[i]->type;
    }

    return -1;
}

static struct demuxer *open_given_type(struct MPOpts *opts,
                                       const struct demuxer_desc *desc,
                                       struct stream *stream, bool force,
                                       int audio_id, int video_id, int sub_id,
                                       char *filename,
                                       struct demuxer_params *params)
{
    struct demuxer *demuxer;
    int fformat;
    mp_msg(MSGT_DEMUXER, MSGL_V, "Trying demuxer: %s\n", desc->name);
    demuxer = new_demuxer(opts, stream, desc->type, audio_id,
                          video_id, sub_id, filename);
    demuxer->params = params;
    if (desc->check_file)
        fformat = desc->check_file(demuxer);
    else
        fformat = desc->type;
    if (force)
        fformat = desc->type;
    if (fformat == 0)
        goto fail;
    if (fformat == desc->type) {
        if (demuxer->filetype)
            mp_tmsg(MSGT_DEMUXER, MSGL_INFO, "Detected file format: %s (%s)\n",
                    demuxer->filetype, desc->shortdesc);
        else
            mp_tmsg(MSGT_DEMUXER, MSGL_INFO, "Detected file format: %s\n",
                    desc->shortdesc);
        if (demuxer->desc->open) {
            struct demuxer *demux2 = demuxer->desc->open(demuxer);
            if (!demux2) {
                mp_tmsg(MSGT_DEMUXER, MSGL_ERR, "Opening as detected format "
                        "\"%s\" failed.\n", desc->shortdesc);
                goto fail;
            }
            /* At least demux_mov can return a demux_demuxers instance
             * from open() instead of the original fed in. */
            demuxer = demux2;
        }
        demuxer->file_format = fformat;
        opts->correct_pts = opts->user_correct_pts;
        if (opts->correct_pts < 0)
            opts->correct_pts =
                demux_control(demuxer, DEMUXER_CTRL_CORRECT_PTS,
                            NULL) == DEMUXER_CTRL_OK;
        if (stream_manages_timeline(demuxer->stream)) {
            // Incorrect, but fixes some behavior with DVD/BD
            demuxer->ts_resets_possible = false;
            // Doesn't work, because stream_pts is a "guess".
            demuxer->accurate_seek = false;
        }
        demuxer_sort_chapters(demuxer);
        return demuxer;
    } else {
        // demux_mov can return playlist instead of mov
        if (fformat == DEMUXER_TYPE_PLAYLIST)
            return demuxer; // handled in mplayer.c
        /* Internal MPEG PS demuxer check can return other MPEG subtypes
         * which don't have their own checks; recurse to try opening as
         * the returned type instead. */
        free_demuxer(demuxer);
        desc = get_demuxer_desc_from_type(fformat);
        if (!desc) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR,
                   "BUG: recursion to nonexistent file format\n");
            return NULL;
        }
        return open_given_type(opts, desc, stream, false, audio_id,
                               video_id, sub_id, filename, params);
    }
 fail:
    free_demuxer(demuxer);
    return NULL;
}

struct demuxer *demux_open_withparams(struct MPOpts *opts,
                                      struct stream *stream, int file_format,
                                      char *force_format, int audio_id,
                                      int video_id, int sub_id, char *filename,
                                      struct demuxer_params *params)
{
    struct demuxer *demuxer = NULL;
    const struct demuxer_desc *desc;

    int force = 0;
    int demuxer_type;
    if ((demuxer_type = get_demuxer_type_from_name(force_format, &force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "Demuxer %s does not exist.\n",
               force_format);
        return NULL;
    }
    if (demuxer_type)
        file_format = demuxer_type;

    // Some code (e.g. dvd stuff, network code, or extension.c) explicitly
    // request certain file formats. The list of formats are always handled by
    // libavformat.
    // Maybe attempts should be made to convert the mplayer format to the libav
    // format, instead of reyling on libav to auto-detect the stream's format
    // correctly.
    switch (file_format) {
    case DEMUXER_TYPE_MPEG_PS:
    //case DEMUXER_TYPE_MPEG_TS:
    case DEMUXER_TYPE_Y4M:
    case DEMUXER_TYPE_NSV:
    case DEMUXER_TYPE_AAC:
    case DEMUXER_TYPE_MPC:
        file_format = DEMUXER_TYPE_LAVF;
    }

    // If somebody requested a demuxer check it
    if (file_format) {
        desc = get_demuxer_desc_from_type(file_format);
        if (!desc)
            // should only happen with obsolete -demuxer 99 numeric format
            return NULL;
        return open_given_type(opts, desc, stream, force, audio_id,
                               video_id, sub_id, filename, params);
    }

    // Test demuxers with safe file checks
    for (int i = 0; (desc = demuxer_list[i]); i++) {
        if (desc->safe_check) {
            demuxer = open_given_type(opts, desc, stream, false, audio_id,
                                      video_id, sub_id, filename, params);
            if (demuxer)
                return demuxer;
        }
    }

    // Ok. We're over the stable detectable fileformats, the next ones are
    // a bit fuzzy. So by default (extension_parsing==1) try extension-based
    // detection first:
    if (filename && opts->extension_parsing == 1) {
        desc = get_demuxer_desc_from_type(demuxer_type_by_filename(filename));
        if (desc)
            demuxer = open_given_type(opts, desc, stream, false, audio_id,
                                      video_id, sub_id, filename, params);
        if (demuxer)
            return demuxer;
    }

    // Finally try detection for demuxers with unsafe checks
    for (int i = 0; (desc = demuxer_list[i]); i++) {
        if (!desc->safe_check && desc->check_file) {
            demuxer = open_given_type(opts, desc, stream, false, audio_id,
                                      video_id, sub_id, filename, params);
            if (demuxer)
                return demuxer;
        }
    }

    return NULL;
}

struct demuxer *demux_open(struct MPOpts *opts, stream_t *vs, int file_format,
                           int audio_id, int video_id, int sub_id,
                           char *filename)
{
    return demux_open_withparams(opts, vs, file_format, opts->demuxer_name,
                                 audio_id, video_id, sub_id, filename, NULL);
}

void demux_flush(demuxer_t *demuxer)
{
    ds_free_packs(demuxer->video);
    ds_free_packs(demuxer->audio);
    ds_free_packs(demuxer->sub);
}

int demux_seek(demuxer_t *demuxer, float rel_seek_secs, float audio_delay,
               int flags)
{
    if (!demuxer->seekable) {
        if (demuxer->file_format == DEMUXER_TYPE_AVI)
            mp_tmsg(MSGT_SEEK, MSGL_WARN, "Cannot seek in raw AVI streams. (Index required, try with the -idx switch.)\n");
#ifdef CONFIG_TV
        else if (demuxer->file_format == DEMUXER_TYPE_TV)
            mp_tmsg(MSGT_SEEK, MSGL_WARN, "TV input is not seekable! (Seeking will probably be for changing channels ;)\n");
#endif
        else
            mp_tmsg(MSGT_SEEK, MSGL_WARN, "Cannot seek in this file.\n");
        return 0;
    }

    if (rel_seek_secs == MP_NOPTS_VALUE && (flags & SEEK_ABSOLUTE))
        return 0;

    // clear demux buffers:
    demux_flush(demuxer);
    demuxer->video->eof = 0;
    demuxer->audio->eof = 0;
    demuxer->sub->eof = 0;
    demuxer->warned_queue_overflow = false;

    /* HACK: assume any demuxer used with these streams can cope with
     * the stream layer suddenly seeking to a different position under it
     * (nothing actually implements DEMUXER_CTRL_RESYNC now).
     */
    struct stream *stream = demuxer->stream;
    if (stream_manages_timeline(stream)) {
        double pts;

        if (flags & SEEK_ABSOLUTE)
            pts = 0.0f;
        else {
            if (demuxer->stream_pts == MP_NOPTS_VALUE)
                goto dmx_seek;
            pts = demuxer->stream_pts;
        }

        if (flags & SEEK_FACTOR) {
            double tmp = 0;
            if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH,
                               &tmp) == STREAM_UNSUPPORTED)
                goto dmx_seek;
            pts += tmp * rel_seek_secs;
        } else
            pts += rel_seek_secs;

        if (stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_TIME, &pts)
            != STREAM_UNSUPPORTED) {
            demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);
            return 1;
        }
    }

  dmx_seek:
    if (demuxer->desc->seek)
        demuxer->desc->seek(demuxer, rel_seek_secs, audio_delay, flags);

    return 1;
}

int demux_info_add(demuxer_t *demuxer, const char *opt, const char *param)
{
    return demux_info_add_bstr(demuxer, bstr0(opt), bstr0(param));
}

int demux_info_add_bstr(demuxer_t *demuxer, struct bstr opt, struct bstr param)
{
    char **info = demuxer->info;
    int n = 0;


    for (n = 0; info && info[2 * n] != NULL; n++) {
        if (!bstrcasecmp(opt, bstr0(info[2*n]))) {
            if (!bstrcmp(param, bstr0(info[2*n + 1]))) {
                mp_msg(MSGT_DEMUX, MSGL_V, "Demuxer info %.*s set to unchanged value %.*s\n",
                       BSTR_P(opt), BSTR_P(param));
                return 0;
            }
            mp_tmsg(MSGT_DEMUX, MSGL_INFO, "Demuxer info %.*s changed to %.*s\n",
                    BSTR_P(opt), BSTR_P(param));
            talloc_free(info[2*n + 1]);
            info[2*n + 1] = talloc_strndup(demuxer->info, param.start, param.len);
            return 0;
        }
    }

    info = demuxer->info = talloc_realloc(demuxer, info, char *, 2 * (n + 2));
    info[2*n]     = talloc_strndup(demuxer->info, opt.start,   opt.len);
    info[2*n + 1] = talloc_strndup(demuxer->info, param.start, param.len);
    memset(&info[2 * (n + 1)], 0, 2 * sizeof(char *));

    return 1;
}

int demux_info_print(demuxer_t *demuxer)
{
    char **info = demuxer->info;
    int n;

    if (!info)
        return 0;

    mp_tmsg(MSGT_DEMUX, MSGL_INFO, "Clip info:\n");
    for (n = 0; info[2 * n] != NULL; n++) {
        mp_msg(MSGT_DEMUX, MSGL_INFO, " %s: %s\n", info[2 * n],
               info[2 * n + 1]);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CLIP_INFO_NAME%d=%s\n", n,
               info[2 * n]);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CLIP_INFO_VALUE%d=%s\n", n,
               info[2 * n + 1]);
    }
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CLIP_INFO_N=%d\n", n);

    return 0;
}

char *demux_info_get(demuxer_t *demuxer, const char *opt)
{
    int i;
    char **info = demuxer->info;

    for (i = 0; info && info[2 * i] != NULL; i++) {
        if (!strcasecmp(opt, info[2 * i]))
            return info[2 * i + 1];
    }

    return NULL;
}

int demux_control(demuxer_t *demuxer, int cmd, void *arg)
{

    if (demuxer->desc->control)
        return demuxer->desc->control(demuxer, cmd, arg);

    return DEMUXER_CTRL_NOTIMPL;
}

struct sh_stream *demuxer_stream_by_demuxer_id(struct demuxer *d,
                                               enum stream_type t, int id)
{
    for (int n = 0; n < d->num_streams; n++) {
        struct sh_stream *s = d->streams[n];
        if (s->type == t && s->demuxer_id == id)
            return d->streams[n];
    }
    return NULL;
}

void demuxer_switch_track(struct demuxer *demuxer, enum stream_type type,
                          struct sh_stream *stream)
{
    assert(!stream || stream->type == type);

    int old_id = demuxer->ds[type]->id;

    // legacy
    int index = stream ? stream->stream_index : -2;
    if (type == STREAM_AUDIO) {
        if (demux_control(demuxer, DEMUXER_CTRL_SWITCH_AUDIO, &index)
                == DEMUXER_CTRL_NOTIMPL)
            demuxer->audio->id = index;
    } else if (type == STREAM_VIDEO) {
        if (demux_control(demuxer, DEMUXER_CTRL_SWITCH_VIDEO, &index)
                == DEMUXER_CTRL_NOTIMPL)
            demuxer->video->id = index;
    } else if (type == STREAM_SUB) {
        demuxer->ds[type]->id = index;
    } else {
        abort();
    }

    int new_id = demuxer->ds[type]->id;
    void *new = NULL;
    if (new_id >= 0) {
        switch (type) {
        case STREAM_VIDEO: new = demuxer->v_streams[new_id]; break;
        case STREAM_AUDIO: new = demuxer->a_streams[new_id]; break;
        case STREAM_SUB: new = demuxer->s_streams[new_id]; break;
        }
    }
    demuxer->ds[type]->sh = new;

    if (old_id != new_id) {
        ds_free_packs(demuxer->ds[type]);
        demux_control(demuxer, DEMUXER_CTRL_SWITCHED_TRACKS, NULL);
    }
}

bool demuxer_stream_is_selected(struct demuxer *d, struct sh_stream *stream)
{
    return stream && d->ds[stream->type]->id == stream->stream_index;
}

int demuxer_add_attachment(demuxer_t *demuxer, struct bstr name,
                           struct bstr type, struct bstr data)
{
    if (!(demuxer->num_attachments % 32))
        demuxer->attachments = talloc_realloc(demuxer, demuxer->attachments,
                                              struct demux_attachment,
                                              demuxer->num_attachments + 32);

    struct demux_attachment *att =
        demuxer->attachments + demuxer->num_attachments;
    att->name = talloc_strndup(demuxer->attachments, name.start, name.len);
    att->type = talloc_strndup(demuxer->attachments, type.start, type.len);
    att->data = talloc_size(demuxer->attachments, data.len);
    memcpy(att->data, data.start, data.len);
    att->data_size = data.len;

    return demuxer->num_attachments++;
}

static int chapter_compare(const void *p1, const void *p2)
{
    struct demux_chapter *c1 = (void *)p1;
    struct demux_chapter *c2 = (void *)p2;

    if (c1->start > c2->start)
        return 1;
    else if (c1->start < c2->start)
        return -1;
    return c1->original_index > c2->original_index ? 1 :-1; // never equal
}

void demuxer_sort_chapters(demuxer_t *demuxer)
{
    qsort(demuxer->chapters, demuxer->num_chapters,
          sizeof(struct demux_chapter), chapter_compare);
}

int demuxer_add_chapter(demuxer_t *demuxer, struct bstr name,
                        uint64_t start, uint64_t end)
{
    struct demux_chapter new = {
        .original_index = demuxer->num_chapters,
        .start = start,
        .end = end,
        .name = name.len ? bstrdup0(demuxer, name)
                         : talloc_strdup(demuxer, mp_gtext("unknown")),
    };
    MP_TARRAY_APPEND(demuxer, demuxer->chapters, demuxer->num_chapters, new);
    return 0;
}

/**
 * \brief demuxer_seek_chapter() seeks to a chapter in two possible ways:
 *        either using the demuxer->chapters structure set by the demuxer
 *        or asking help to the stream layer (e.g. dvd)
 * \param chapter - chapter number wished - 0-based
 * \param seek_pts set by the function to the pts to seek to (if demuxer->chapters is set)
 * \return -1 on error, current chapter if successful
 */

int demuxer_seek_chapter(demuxer_t *demuxer, int chapter, double *seek_pts)
{
    int ris;

    if (!demuxer->num_chapters || !demuxer->chapters) {
        demux_flush(demuxer);

        ris = stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_CHAPTER,
                             &chapter);
        if (ris != STREAM_UNSUPPORTED)
            demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);

        // exit status may be ok, but main() doesn't have to seek itself
        // (because e.g. dvds depend on sectors, not on pts)
        *seek_pts = -1.0;

        return ris != STREAM_UNSUPPORTED ? chapter : -1;
    } else {  // chapters structure is set in the demuxer
        if (chapter >= demuxer->num_chapters)
            return -1;
        if (chapter < 0)
            chapter = 0;

        *seek_pts = demuxer->chapters[chapter].start / 1e9;

        return chapter;
    }
}

int demuxer_get_current_chapter(demuxer_t *demuxer, double time_now)
{
    int chapter = -2;
    if (!demuxer->num_chapters || !demuxer->chapters) {
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_CHAPTER,
                           &chapter) == STREAM_UNSUPPORTED)
            chapter = -2;
    } else {
        uint64_t now = time_now * 1e9 + 0.5;
        for (chapter = demuxer->num_chapters - 1; chapter >= 0; --chapter) {
            if (demuxer->chapters[chapter].start <= now)
                break;
        }
    }
    return chapter;
}

char *demuxer_chapter_name(demuxer_t *demuxer, int chapter)
{
    if (demuxer->num_chapters && demuxer->chapters) {
        if (chapter >= 0 && chapter < demuxer->num_chapters
            && demuxer->chapters[chapter].name)
            return talloc_strdup(NULL, demuxer->chapters[chapter].name);
    }
    return NULL;
}

double demuxer_chapter_time(demuxer_t *demuxer, int chapter)
{
    if (demuxer->num_chapters && demuxer->chapters && chapter >= 0
        && chapter < demuxer->num_chapters) {
        return demuxer->chapters[chapter].start / 1e9;
    }
    return -1.0;
}

int demuxer_chapter_count(demuxer_t *demuxer)
{
    if (!demuxer->num_chapters || !demuxer->chapters) {
        int num_chapters = 0;
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_CHAPTERS,
                           &num_chapters) == STREAM_UNSUPPORTED)
            num_chapters = 0;
        return num_chapters;
    } else
        return demuxer->num_chapters;
}

double demuxer_get_time_length(struct demuxer *demuxer)
{
    double len;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &len) > 0)
        return len;
    // <= 0 means DEMUXER_CTRL_NOTIMPL or DEMUXER_CTRL_DONTKNOW
    if (demux_control(demuxer, DEMUXER_CTRL_GET_TIME_LENGTH, &len) > 0)
        return len;
    return -1;
}

double demuxer_get_start_time(struct demuxer *demuxer)
{
    double time;
    if (stream_control(demuxer->stream, STREAM_CTRL_GET_START_TIME, &time) > 0)
        return time;
    if (demux_control(demuxer, DEMUXER_CTRL_GET_START_TIME, &time) > 0)
        return time;
    return 0;
}

int demuxer_angles_count(demuxer_t *demuxer)
{
    int ris, angles = -1;

    ris = stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_ANGLES, &angles);
    if (ris == STREAM_UNSUPPORTED)
        return -1;
    return angles;
}

int demuxer_get_current_angle(demuxer_t *demuxer)
{
    int ris, curr_angle = -1;
    ris = stream_control(demuxer->stream, STREAM_CTRL_GET_ANGLE, &curr_angle);
    if (ris == STREAM_UNSUPPORTED)
        return -1;
    return curr_angle;
}


int demuxer_set_angle(demuxer_t *demuxer, int angle)
{
    int ris, angles = -1;

    angles = demuxer_angles_count(demuxer);
    if ((angles < 1) || (angle > angles))
        return -1;

    demux_flush(demuxer);

    ris = stream_control(demuxer->stream, STREAM_CTRL_SET_ANGLE, &angle);
    if (ris == STREAM_UNSUPPORTED)
        return -1;

    demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);

    return angle;
}
