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
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "options.h"
#include "talloc.h"
#include "mp_msg.h"
#include "m_config.h"

#include "libvo/fastmemcpy.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "mf.h"

#include "libaf/af_format.h"
#include "libmpcodecs/dec_teletext.h"
#include "libmpcodecs/vd_ffmpeg.h"

#ifdef CONFIG_FFMPEG
#include "libavcodec/avcodec.h"
#if MP_INPUT_BUFFER_PADDING_SIZE < FF_INPUT_BUFFER_PADDING_SIZE
#error MP_INPUT_BUFFER_PADDING_SIZE is too small!
#endif
#endif

static void clear_parser(sh_common_t *sh);

// Demuxer list
extern const struct demuxer_desc demuxer_desc_edl;
extern const demuxer_desc_t demuxer_desc_rawaudio;
extern const demuxer_desc_t demuxer_desc_rawvideo;
extern const demuxer_desc_t demuxer_desc_tv;
extern const demuxer_desc_t demuxer_desc_mf;
extern const demuxer_desc_t demuxer_desc_avi;
extern const demuxer_desc_t demuxer_desc_y4m;
extern const demuxer_desc_t demuxer_desc_asf;
extern const demuxer_desc_t demuxer_desc_real;
extern const demuxer_desc_t demuxer_desc_smjpeg;
extern const demuxer_desc_t demuxer_desc_matroska;
extern const demuxer_desc_t demuxer_desc_realaudio;
extern const demuxer_desc_t demuxer_desc_vqf;
extern const demuxer_desc_t demuxer_desc_mov;
extern const demuxer_desc_t demuxer_desc_vivo;
extern const demuxer_desc_t demuxer_desc_fli;
extern const demuxer_desc_t demuxer_desc_film;
extern const demuxer_desc_t demuxer_desc_roq;
extern const demuxer_desc_t demuxer_desc_gif;
extern const demuxer_desc_t demuxer_desc_ogg;
extern const demuxer_desc_t demuxer_desc_avs;
extern const demuxer_desc_t demuxer_desc_pva;
extern const demuxer_desc_t demuxer_desc_nsv;
extern const demuxer_desc_t demuxer_desc_mpeg_ts;
extern const demuxer_desc_t demuxer_desc_lmlm4;
extern const demuxer_desc_t demuxer_desc_mpeg_ps;
extern const demuxer_desc_t demuxer_desc_mpeg_pes;
extern const demuxer_desc_t demuxer_desc_mpeg_es;
extern const demuxer_desc_t demuxer_desc_mpeg_gxf;
extern const demuxer_desc_t demuxer_desc_mpeg4_es;
extern const demuxer_desc_t demuxer_desc_h264_es;
extern const demuxer_desc_t demuxer_desc_rawdv;
extern const demuxer_desc_t demuxer_desc_mpc;
extern const demuxer_desc_t demuxer_desc_audio;
extern const demuxer_desc_t demuxer_desc_xmms;
extern const demuxer_desc_t demuxer_desc_mpeg_ty;
extern const demuxer_desc_t demuxer_desc_rtp;
extern const demuxer_desc_t demuxer_desc_rtp_nemesi;
extern const demuxer_desc_t demuxer_desc_lavf;
extern const demuxer_desc_t demuxer_desc_lavf_preferred;
extern const demuxer_desc_t demuxer_desc_aac;
extern const demuxer_desc_t demuxer_desc_nut;
extern const demuxer_desc_t demuxer_desc_mng;

/* Please do not add any new demuxers here. If you want to implement a new
 * demuxer, add it to libavformat, except for wrappers around external
 * libraries and demuxers requiring binary support. */

const demuxer_desc_t *const demuxer_list[] = {
    &demuxer_desc_edl,
    &demuxer_desc_rawaudio,
    &demuxer_desc_rawvideo,
#ifdef CONFIG_TV
    &demuxer_desc_tv,
#endif
    &demuxer_desc_mf,
#ifdef CONFIG_FFMPEG
    &demuxer_desc_lavf_preferred,
#endif
    &demuxer_desc_avi,
    &demuxer_desc_y4m,
    &demuxer_desc_asf,
    &demuxer_desc_nsv,
    &demuxer_desc_real,
    &demuxer_desc_smjpeg,
    &demuxer_desc_matroska,
    &demuxer_desc_realaudio,
    &demuxer_desc_vqf,
    &demuxer_desc_mov,
    &demuxer_desc_vivo,
    &demuxer_desc_fli,
    &demuxer_desc_film,
    &demuxer_desc_roq,
#ifdef CONFIG_GIF
    &demuxer_desc_gif,
#endif
#ifdef CONFIG_OGGVORBIS
    &demuxer_desc_ogg,
#endif
#ifdef CONFIG_WIN32DLL
    &demuxer_desc_avs,
#endif
    &demuxer_desc_pva,
    &demuxer_desc_mpeg_ts,
    &demuxer_desc_lmlm4,
    &demuxer_desc_mpeg_ps,
    &demuxer_desc_mpeg_pes,
    &demuxer_desc_mpeg_es,
    &demuxer_desc_mpeg_gxf,
    &demuxer_desc_mpeg4_es,
    &demuxer_desc_h264_es,
    &demuxer_desc_audio,
    &demuxer_desc_mpeg_ty,
#ifdef CONFIG_LIVE555
    &demuxer_desc_rtp,
#endif
#ifdef CONFIG_LIBNEMESI
    &demuxer_desc_rtp_nemesi,
#endif
#ifdef CONFIG_FFMPEG
    &demuxer_desc_lavf,
#endif
#ifdef CONFIG_MUSEPACK
    &demuxer_desc_mpc,
#endif
#ifdef CONFIG_LIBDV095
    &demuxer_desc_rawdv,
#endif
    &demuxer_desc_aac,
#ifdef CONFIG_LIBNUT
    &demuxer_desc_nut,
#endif
#ifdef CONFIG_XMMS
    &demuxer_desc_xmms,
#endif
#ifdef CONFIG_MNG
    &demuxer_desc_mng,
#endif
    /* Please do not add any new demuxers here. If you want to implement a new
     * demuxer, add it to libavformat, except for wrappers around external
     * libraries and demuxers requiring binary support. */
    NULL
};

struct demux_packet *new_demux_packet(size_t len)
{
    if (len > 1000000000) {
        mp_msg(MSGT_DEMUXER, MSGL_FATAL, "Attempt to allocate demux packet "
               "over 1 GB!\n");
        abort();
    }
    struct demux_packet *dp = malloc(sizeof(struct demux_packet));
    dp->len = len;
    dp->next = NULL;
    dp->pts = MP_NOPTS_VALUE;
    dp->duration = -1;
    dp->stream_pts = MP_NOPTS_VALUE;
    dp->pos = 0;
    dp->flags = 0;
    dp->refcount = 1;
    dp->master = NULL;
    dp->buffer = NULL;
    if (len > 0) {
        dp->buffer = malloc(len + MP_INPUT_BUFFER_PADDING_SIZE);
        if (!dp->buffer) {
            mp_msg(MSGT_DEMUXER, MSGL_FATAL, "Memory allocation failure!\n");
            abort();
        }
        memset(dp->buffer + len, 0, 8);
    }
    return dp;
}

void resize_demux_packet(struct demux_packet *dp, size_t len)
{
    if (len > 1000000000) {
        mp_msg(MSGT_DEMUXER, MSGL_FATAL, "Attempt to realloc demux packet "
               "over 1 GB!\n");
        abort();
    }
    if (len > 0) {
        dp->buffer = realloc(dp->buffer, len + 8);
        if (!dp->buffer) {
            mp_msg(MSGT_DEMUXER, MSGL_FATAL, "Memory allocation failure!\n");
            abort();
        }
        memset(dp->buffer + len, 0, 8);
    } else {
        free(dp->buffer);
        dp->buffer = NULL;
    }
    dp->len = len;
}

struct demux_packet *clone_demux_packet(struct demux_packet *pack)
{
    struct demux_packet *dp = malloc(sizeof(struct demux_packet));
    while (pack->master)
        pack = pack->master;  // find the master
    memcpy(dp, pack, sizeof(struct demux_packet));
    dp->next = NULL;
    dp->refcount = 0;
    dp->master = pack;
    pack->refcount++;
    return dp;
}

void free_demux_packet(struct demux_packet *dp)
{
    if (dp->master == NULL) {  //dp is a master packet
        dp->refcount--;
        if (dp->refcount == 0) {
            free(dp->buffer);
            free(dp);
        }
        return;
    }
    // dp is a clone:
    free_demux_packet(dp->master);
    free(dp);
}

static void free_demuxer_stream(struct demux_stream *ds)
{
    ds_free_packs(ds);
    free(ds);
}

static struct demux_stream *new_demuxer_stream(struct demuxer *demuxer, int id)
{
    demux_stream_t *ds = malloc(sizeof(demux_stream_t));
    *ds = (demux_stream_t){
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
    d->audio = new_demuxer_stream(d, a_id);
    d->video = new_demuxer_stream(d, v_id);
    d->sub = new_demuxer_stream(d, s_id);
    d->type = type;
    d->opts = opts;
    if (type)
        if (!(d->desc = get_demuxer_desc_from_type(type)))
            mp_msg(MSGT_DEMUXER, MSGL_ERR,
                   "BUG! Invalid demuxer type in new_demuxer(), "
                   "big troubles ahead.");
    if (filename) // Filename hack for avs_check_file
        d->filename = strdup(filename);
    stream_seek(stream, stream->start_pos);
    return d;
}

const char *sh_sub_type2str(int type)
{
    switch (type) {
    case 't': return "text";
    case 'm': return "movtext";
    case 'a': return "ass";
    case 'v': return "vobsub";
    case 'x': return "xsub";
    case 'b': return "dvb";
    case 'd': return "dvb-teletext";
    case 'p': return "hdmv pgs";
    }
    return "unknown";
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
        sh_sub_t *sh = calloc(1, sizeof(sh_sub_t));
        demuxer->s_streams[id] = sh;
        sh->sid = sid;
        sh->opts = demuxer->opts;
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SUBTITLE_ID=%d\n", sid);
    }
    return demuxer->s_streams[id];
}

struct sh_sub *new_sh_sub_sid_lang(struct demuxer *demuxer, int id, int sid,
                                   const char *lang)
{
    struct sh_sub *sh = new_sh_sub_sid(demuxer, id, sid);
    if (lang && lang[0] && strcmp(lang, "und")) {
        sh->lang = strdup(lang);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SID_%d_LANG=%s\n", sid, lang);
    }
    return sh;
}

static void free_sh_sub(sh_sub_t *sh)
{
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_sub at %p\n", sh);
    free(sh->extradata);
    free(sh->lang);
#ifdef CONFIG_FFMPEG
    clear_parser((sh_common_t *)sh);
#endif
    free(sh);
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
        sh_audio_t *sh = calloc(1, sizeof(sh_audio_t));
        demuxer->a_streams[id] = sh;
        sh->aid = aid;
        sh->ds = demuxer->audio;
        // set some defaults
        sh->samplesize = 2;
        sh->sample_format = AF_FORMAT_S16_NE;
        sh->opts = demuxer->opts;
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_ID=%d\n", aid);
    }
    return demuxer->a_streams[id];
}

void free_sh_audio(demuxer_t *demuxer, int id)
{
    sh_audio_t *sh = demuxer->a_streams[id];
    demuxer->a_streams[id] = NULL;
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_audio at %p\n", sh);
    free(sh->wf);
    free(sh->codecdata);
    free(sh->lang);
#ifdef CONFIG_FFMPEG
    clear_parser((sh_common_t *)sh);
#endif
    free(sh);
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
        sh_video_t *sh = calloc(1, sizeof *sh);
        demuxer->v_streams[id] = sh;
        sh->vid = vid;
        sh->ds = demuxer->video;
        sh->opts = demuxer->opts;
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_ID=%d\n", vid);
    }
    return demuxer->v_streams[id];
}

void free_sh_video(sh_video_t *sh)
{
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_video at %p\n", sh);
    free(sh->bih);
#ifdef CONFIG_FFMPEG
    clear_parser((sh_common_t *)sh);
#endif
    free(sh);
}

void free_demuxer(demuxer_t *demuxer)
{
    int i;
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing %s demuxer at %p\n",
           demuxer->desc->shortdesc, demuxer);
    if (demuxer->desc->close)
        demuxer->desc->close(demuxer);
    // Very ugly hack to make it behave like old implementation
    if (demuxer->desc->type == DEMUXER_TYPE_DEMUXERS)
        goto skip_streamfree;
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
 skip_streamfree:
    free(demuxer->filename);
    if (demuxer->teletext)
        teletext_control(demuxer->teletext, TV_VBI_CONTROL_STOP, NULL);
    talloc_free(demuxer);
}


void ds_add_packet(demux_stream_t *ds, demux_packet_t *dp)
{
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

#ifdef CONFIG_FFMPEG
static void allocate_parser(AVCodecContext **avctx, AVCodecParserContext **parser, unsigned format)
{
    enum CodecID codec_id = CODEC_ID_NONE;

    init_avcodec();

    switch (format) {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 94, 0)
    case MKTAG('M', 'P', '4', 'L'):
        codec_id = CODEC_ID_AAC_LATM;
        break;
#endif
    case 0x2000:
    case 0x332D6361:
    case 0x332D4341:
    case 0x20736D:
    case MKTAG('s', 'a', 'c', '3'):
        codec_id = CODEC_ID_AC3;
        break;
    case MKTAG('d', 'n', 'e', 't'):
        // DNET/byte-swapped AC-3 - there is no parser for that yet
        //codec_id = CODEC_ID_DNET;
        break;
    case MKTAG('E', 'A', 'C', '3'):
        codec_id = CODEC_ID_EAC3;
        break;
    case 0x2001:
    case 0x86:
        codec_id = CODEC_ID_DTS;
        break;
    case MKTAG('f', 'L', 'a', 'C'):
        codec_id = CODEC_ID_FLAC;
        break;
    case MKTAG('M', 'L', 'P', ' '):
        codec_id = CODEC_ID_MLP;
        break;
    case 0x55:
    case 0x5500736d:
    case 0x55005354:
    case MKTAG('.', 'm', 'p', '3'):
    case MKTAG('M', 'P', '3', ' '):
    case MKTAG('L', 'A', 'M', 'E'):
        codec_id = CODEC_ID_MP3;
        break;
    case 0x50:
    case 0x5000736d:
    case MKTAG('.', 'm', 'p', '2'):
    case MKTAG('.', 'm', 'p', '1'):
        codec_id = CODEC_ID_MP2;
        break;
    case MKTAG('T', 'R', 'H', 'D'):
        codec_id = CODEC_ID_TRUEHD;
        break;
    }
    if (codec_id != CODEC_ID_NONE) {
        *avctx = avcodec_alloc_context();
        if (!*avctx)
            return;
        *parser = av_parser_init(codec_id);
        if (!*parser)
            av_freep(avctx);
    }
}

static void get_parser(sh_common_t *sh, AVCodecContext **avctx, AVCodecParserContext **parser)
{
    *avctx  = NULL;
    *parser = NULL;

    if (!sh || !sh->needs_parsing)
        return;

    *avctx  = sh->avctx;
    *parser = sh->parser;
    if (*parser)
        return;

    allocate_parser(avctx, parser, sh->format);
    sh->avctx  = *avctx;
    sh->parser = *parser;
}

int ds_parse(demux_stream_t *ds, uint8_t **buffer, int *len, double pts, off_t pos)
{
    AVCodecContext *avctx;
    AVCodecParserContext *parser;
    get_parser(ds->sh, &avctx, &parser);
    if (!parser)
        return *len;
    return av_parser_parse2(parser, avctx, buffer, len, *buffer, *len, pts, pts, pos);
}

static void clear_parser(sh_common_t *sh)
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
#endif

void ds_read_packet(demux_stream_t *ds, stream_t *stream, int len,
                    double pts, off_t pos, int flags)
{
    demux_packet_t *dp = new_demux_packet(len);
    len = stream_read(stream, dp->buffer, len);
    resize_demux_packet(dp, len);
    dp->pts = pts;
    dp->pos = pos;
    dp->flags = flags;
    // append packet to DS stream:
    ds_add_packet(ds, dp);
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
    if (mp_msg_test(MSGT_DEMUXER, MSGL_DBG3)) {
        if (ds == demux->audio)
            mp_dbg(MSGT_DEMUXER, MSGL_DBG3,
                   "ds_fill_buffer(d_audio) called\n");
        else if (ds == demux->video)
            mp_dbg(MSGT_DEMUXER, MSGL_DBG3,
                   "ds_fill_buffer(d_video) called\n");
        else if (ds == demux->sub)
            mp_dbg(MSGT_DEMUXER, MSGL_DBG3, "ds_fill_buffer(d_sub) called\n");
        else
            mp_dbg(MSGT_DEMUXER, MSGL_DBG3,
                   "ds_fill_buffer(unknown 0x%X) called\n", (unsigned int) ds);
    }
    while (1) {
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
            ds->flags = p->flags;
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
            return 1;
        }

#define MaybeNI _("Maybe you are playing a non-interleaved stream/file or the codec failed?\n" \
                "For AVI files, try to force non-interleaved mode with the -ni option.\n")

        if (demux->audio->packs >= MAX_PACKS
            || demux->audio->bytes >= MAX_PACK_BYTES) {
            mp_tmsg(MSGT_DEMUXER, MSGL_ERR, "\nToo many audio packets in the buffer: (%d in %d bytes).\n",
                   demux->audio->packs, demux->audio->bytes);
            mp_tmsg(MSGT_DEMUXER, MSGL_HINT, MaybeNI);
            break;
        }
        if (demux->video->packs >= MAX_PACKS
            || demux->video->bytes >= MAX_PACK_BYTES) {
            mp_tmsg(MSGT_DEMUXER, MSGL_ERR, "\nToo many video packets in the buffer: (%d in %d bytes).\n",
                   demux->video->packs, demux->video->bytes);
            mp_tmsg(MSGT_DEMUXER, MSGL_HINT, MaybeNI);
            break;
        }
        if (!demux_fill_buffer(demux, ds)) {
            mp_dbg(MSGT_DEMUXER, MSGL_DBG2,
                   "ds_fill_buffer()->demux_fill_buffer() failed\n");
            break; // EOF
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
                fast_memcpy(mem + bytes, &ds->buffer[ds->buffer_pos], x);
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
    ds->pts = 0;
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

double ds_get_next_pts(demux_stream_t *ds)
{
    demuxer_t *demux = ds->demuxer;
    // if we have not read from the "current" packet, consider it
    // as the next, otherwise we never get the pts for the first packet.
    while (!ds->first && (!ds->current || ds->buffer_pos)) {
        if (demux->audio->packs >= MAX_PACKS
            || demux->audio->bytes >= MAX_PACK_BYTES) {
            mp_tmsg(MSGT_DEMUXER, MSGL_ERR, "\nToo many audio packets in the buffer: (%d in %d bytes).\n",
                   demux->audio->packs, demux->audio->bytes);
            mp_tmsg(MSGT_DEMUXER, MSGL_HINT, MaybeNI);
            return MP_NOPTS_VALUE;
        }
        if (demux->video->packs >= MAX_PACKS
            || demux->video->bytes >= MAX_PACK_BYTES) {
            mp_tmsg(MSGT_DEMUXER, MSGL_ERR, "\nToo many video packets in the buffer: (%d in %d bytes).\n",
                   demux->video->packs, demux->video->bytes);
            mp_tmsg(MSGT_DEMUXER, MSGL_HINT, MaybeNI);
            return MP_NOPTS_VALUE;
        }
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
                                       char *filename)
{
    struct demuxer *demuxer;
    int fformat;
    demuxer = new_demuxer(opts, stream, desc->type, audio_id,
                          video_id, sub_id, filename);
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
                               video_id, sub_id, filename);
    }
 fail:
    free_demuxer(demuxer);
    return NULL;
}

static struct demuxer *demux_open_stream(struct MPOpts *opts,
                                         struct stream *stream,
                                         int file_format, bool force,
                                         int audio_id, int video_id, int sub_id,
                                         char *filename)
{
    struct demuxer *demuxer = NULL;
    const struct demuxer_desc *desc;

    // If somebody requested a demuxer check it
    if (file_format) {
        desc = get_demuxer_desc_from_type(file_format);
        if (!desc)
            // should only happen with obsolete -demuxer 99 numeric format
            return NULL;
        demuxer = open_given_type(opts, desc, stream, force, audio_id,
                                  video_id, sub_id, filename);
        if (demuxer)
            goto dmx_open;
        return NULL;
    }

    // Test demuxers with safe file checks
    for (int i = 0; (desc = demuxer_list[i]); i++) {
        if (desc->safe_check) {
            demuxer = open_given_type(opts, desc, stream, false, audio_id,
                                      video_id, sub_id, filename);
            if (demuxer)
                goto dmx_open;
        }
    }

    // Ok. We're over the stable detectable fileformats, the next ones are
    // a bit fuzzy. So by default (extension_parsing==1) try extension-based
    // detection first:
    if (filename && opts->extension_parsing == 1) {
        desc = get_demuxer_desc_from_type(demuxer_type_by_filename(filename));
        if (desc)
            demuxer = open_given_type(opts, desc, stream, false, audio_id,
                                      video_id, sub_id, filename);
        if (demuxer)
            goto dmx_open;
    }

    // Finally try detection for demuxers with unsafe checks
    for (int i = 0; (desc = demuxer_list[i]); i++) {
        if (!desc->safe_check && desc->check_file) {
            demuxer = open_given_type(opts, desc, stream, false, audio_id,
                                      video_id, sub_id, filename);
            if (demuxer)
                goto dmx_open;
        }
    }

    return NULL;

 dmx_open:

    if (demuxer->type == DEMUXER_TYPE_PLAYLIST)
        return demuxer;

    struct sh_video *sh_video = demuxer->video->sh;
    if (sh_video && sh_video->bih) {
        int biComp = le2me_32(sh_video->bih->biCompression);
        mp_msg(MSGT_DEMUX, MSGL_INFO,
               "VIDEO:  [%.4s]  %dx%d  %dbpp  %5.3f fps  %5.1f kbps (%4.1f kbyte/s)\n",
               (char *) &biComp, sh_video->bih->biWidth,
               sh_video->bih->biHeight, sh_video->bih->biBitCount,
               sh_video->fps, sh_video->i_bps * 0.008f,
               sh_video->i_bps / 1024.0f);
    }
    return demuxer;
}

demuxer_t *demux_open(struct MPOpts *opts, stream_t *vs, int file_format,
                      int audio_id, int video_id, int dvdsub_id,
                      char *filename)
{
    stream_t *as = NULL, *ss = NULL;
    demuxer_t *vd, *ad = NULL, *sd = NULL;
    demuxer_t *res;
    int afmt = DEMUXER_TYPE_UNKNOWN, sfmt = DEMUXER_TYPE_UNKNOWN;
    int demuxer_type;
    int audio_demuxer_type = 0, sub_demuxer_type = 0;
    int demuxer_force = 0, audio_demuxer_force = 0, sub_demuxer_force = 0;

    if ((demuxer_type =
         get_demuxer_type_from_name(opts->demuxer_name, &demuxer_force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "-demuxer %s does not exist.\n",
               opts->demuxer_name);
        return NULL;
    }
    if ((audio_demuxer_type =
         get_demuxer_type_from_name(opts->audio_demuxer_name,
                                    &audio_demuxer_force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "-audio-demuxer %s does not exist.\n",
               opts->audio_demuxer_name);
        if (opts->audio_stream)
            return NULL;
    }
    if ((sub_demuxer_type =
         get_demuxer_type_from_name(opts->sub_demuxer_name,
                                    &sub_demuxer_force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "-sub-demuxer %s does not exist.\n",
               opts->sub_demuxer_name);
        if (opts->sub_stream)
            return NULL;
    }

    if (opts->audio_stream) {
        as = open_stream(opts->audio_stream, 0, &afmt);
        if (!as) {
            mp_tmsg(MSGT_DEMUXER, MSGL_ERR, "Cannot open audio stream: %s\n",
                   opts->audio_stream);
            return NULL;
        }
        if (opts->audio_stream_cache) {
            if (!stream_enable_cache
                (as, opts->audio_stream_cache * 1024,
                 opts->audio_stream_cache * 1024 *
                            (opts->stream_cache_min_percent / 100.0),
                 opts->audio_stream_cache * 1024 *
                            (opts->stream_cache_seek_min_percent / 100.0))) {
                free_stream(as);
                mp_msg(MSGT_DEMUXER, MSGL_ERR,
                       "Can't enable audio stream cache\n");
                return NULL;
            }
        }
    }
    if (opts->sub_stream) {
        ss = open_stream(opts->sub_stream, 0, &sfmt);
        if (!ss) {
            mp_tmsg(MSGT_DEMUXER, MSGL_ERR, "Cannot open subtitle stream: %s\n",
                   opts->sub_stream);
            return NULL;
        }
    }

    vd = demux_open_stream(opts, vs, demuxer_type ? demuxer_type : file_format,
                           demuxer_force, opts->audio_stream ? -2 : audio_id,
                           video_id, opts->sub_stream ? -2 : dvdsub_id, filename);
    if (!vd) {
        if (as)
            free_stream(as);
        if (ss)
            free_stream(ss);
        return NULL;
    }
    if (as) {
        ad = demux_open_stream(opts, as,
                               audio_demuxer_type ? audio_demuxer_type : afmt,
                               audio_demuxer_force, audio_id, -2, -2,
                               opts->audio_stream);
        if (!ad) {
            mp_tmsg(MSGT_DEMUXER, MSGL_WARN, "Failed to open audio demuxer: %s\n",
                   opts->audio_stream);
            free_stream(as);
        } else if (ad->audio->sh
                   && ((sh_audio_t *) ad->audio->sh)->format == 0x55) // MP3
            opts->hr_mp3_seek = 1;    // Enable high res seeking
    }
    if (ss) {
        sd = demux_open_stream(opts, ss,
                               sub_demuxer_type ? sub_demuxer_type : sfmt,
                               sub_demuxer_force, -2, -2, dvdsub_id,
                               opts->sub_stream);
        if (!sd) {
            mp_tmsg(MSGT_DEMUXER, MSGL_WARN,
                   "Failed to open subtitle demuxer: %s\n", opts->sub_stream);
            free_stream(ss);
        }
    }

    if (ad && sd)
        res = new_demuxers_demuxer(vd, ad, sd);
    else if (ad)
        res = new_demuxers_demuxer(vd, ad, vd);
    else if (sd)
        res = new_demuxers_demuxer(vd, vd, sd);
    else
        res = vd;

    opts->correct_pts = opts->user_correct_pts;
    if (opts->correct_pts < 0)
        opts->correct_pts =
            demux_control(vd ? vd : res, DEMUXER_CTRL_CORRECT_PTS,
                          NULL) == DEMUXER_CTRL_OK;
    return res;
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
    // clear demux buffers:
    demux_flush(demuxer);
    demuxer->video->eof = 0;
    demuxer->audio->eof = 0;
    demuxer->sub->eof = 0;

    /* HACK: assume any demuxer used with these streams can cope with
     * the stream layer suddenly seeking to a different position under it
     * (nothing actually implements DEMUXER_CTRL_RESYNC now).
     */
    struct stream *stream = demuxer->stream;
    if (stream->type == STREAMTYPE_DVD || stream->type == STREAMTYPE_DVDNAV) {
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
    return demux_info_add_bstr(demuxer, BSTR(opt), BSTR(param));
}

int demux_info_add_bstr(demuxer_t *demuxer, struct bstr opt, struct bstr param)
{
    char **info = demuxer->info;
    int n = 0;


    for (n = 0; info && info[2 * n] != NULL; n++) {
        if (!bstrcasecmp(opt, BSTR(info[2*n]))) {
            if (!bstrcmp(param, BSTR(info[2*n + 1]))) {
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

int demuxer_switch_audio(demuxer_t *demuxer, int index)
{
    int res = demux_control(demuxer, DEMUXER_CTRL_SWITCH_AUDIO, &index);
    if (res == DEMUXER_CTRL_NOTIMPL) {
        struct sh_audio *sh_audio = demuxer->audio->sh;
        return sh_audio ? sh_audio->aid : -2;
    }
    if (demuxer->audio->id >= 0) {
        struct sh_audio *sh_audio = demuxer->a_streams[demuxer->audio->id];
        demuxer->audio->sh = sh_audio;
        index = sh_audio->aid; // internal MPEG demuxers don't set it right
    }
    else
        demuxer->audio->sh = NULL;
    return index;
}

int demuxer_switch_video(demuxer_t *demuxer, int index)
{
    int res = demux_control(demuxer, DEMUXER_CTRL_SWITCH_VIDEO, &index);
    if (res == DEMUXER_CTRL_NOTIMPL) {
        struct sh_video *sh_video = demuxer->video->sh;
        return sh_video ? sh_video->vid : -2;
    }
    if (demuxer->video->id >= 0) {
        struct sh_video *sh_video = demuxer->v_streams[demuxer->video->id];
        demuxer->video->sh = sh_video;
        index = sh_video->vid; // internal MPEG demuxers don't set it right
    } else
        demuxer->video->sh = NULL;
    return index;
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

int demuxer_add_chapter(demuxer_t *demuxer, struct bstr name,
                        uint64_t start, uint64_t end)
{
    if (!(demuxer->num_chapters % 32))
        demuxer->chapters = talloc_realloc(demuxer, demuxer->chapters,
                                           struct demux_chapter,
                                           demuxer->num_chapters + 32);

    demuxer->chapters[demuxer->num_chapters].start = start;
    demuxer->chapters[demuxer->num_chapters].end = end;
    demuxer->chapters[demuxer->num_chapters].name = name.len ?
        talloc_strndup(demuxer->chapters, name.start, name.len) :
        talloc_strdup(demuxer->chapters, mp_gtext("unknown"));

    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_ID=%d\n", demuxer->num_chapters);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_%d_START=%"PRIu64"\n", demuxer->num_chapters, start / 1000000);
    if (end)
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_%d_END=%"PRIu64"\n", demuxer->num_chapters, end / 1000000);
    if (name.start)
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CHAPTER_%d_NAME=%.*s\n", demuxer->num_chapters, BSTR_P(name));

    return demuxer->num_chapters++;
}

/**
 * \brief demuxer_seek_chapter() seeks to a chapter in two possible ways:
 *        either using the demuxer->chapters structure set by the demuxer
 *        or asking help to the stream layer (e.g. dvd)
 * \param chapter - chapter number wished - 0-based
 * \param seek_pts set by the function to the pts to seek to (if demuxer->chapters is set)
 * \param chapter_name name of chapter found (set by this function is param is not null)
 * \return -1 on error, current chapter if successful
 */

int demuxer_seek_chapter(demuxer_t *demuxer, int chapter, double *seek_pts,
                         char **chapter_name)
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

        if (chapter_name) {
            *chapter_name = NULL;
            int num_chapters;
            if (stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_CHAPTERS,
                               &num_chapters) == STREAM_UNSUPPORTED)
                num_chapters = 0;
            if (num_chapters) {
                *chapter_name = talloc_size(NULL, 16);
                sprintf(*chapter_name, " of %3d", num_chapters);
            }
        }

        return ris != STREAM_UNSUPPORTED ? chapter : -1;
    } else {  // chapters structure is set in the demuxer
        if (chapter >= demuxer->num_chapters)
            return -1;
        if (chapter < 0)
            chapter = 0;

        *seek_pts = demuxer->chapters[chapter].start / 1e9;

        if (chapter_name)
            *chapter_name = talloc_strdup(NULL, demuxer->chapters[chapter].name);

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
            return strdup(demuxer->chapters[chapter].name);
    }
    return NULL;
}

char *demuxer_chapter_display_name(demuxer_t *demuxer, int chapter)
{
    char *chapter_name = demuxer_chapter_name(demuxer, chapter);
    if (chapter_name) {
        char *tmp = malloc(strlen(chapter_name) + 14);
        snprintf(tmp, 63, "(%d) %s", chapter + 1, chapter_name);
        free(chapter_name);
        return tmp;
    } else {
        int chapter_num = demuxer_chapter_count(demuxer);
        char tmp[30];
        if (chapter_num <= 0)
            sprintf(tmp, "(%d)", chapter + 1);
        else
            sprintf(tmp, "(%d) of %d", chapter + 1, chapter_num);
        return strdup(tmp);
    }
}

float demuxer_chapter_time(demuxer_t *demuxer, int chapter, float *end)
{
    if (demuxer->num_chapters && demuxer->chapters && chapter >= 0
        && chapter < demuxer->num_chapters) {
        if (end)
            *end = demuxer->chapters[chapter].end / 1e9;
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

int demuxer_audio_track_by_lang_and_default(struct demuxer *d, char **langt)
{
    int n = 0;
    while (1) {
        char *lang = langt ? langt[n++] : NULL;
        int id = -1;
        for (int i = 0; i < MAX_A_STREAMS; i++) {
            struct sh_audio *sh = d->a_streams[i];
            if (sh && (!lang || sh->lang && !strcmp(lang, sh->lang))) {
                if (sh->default_track)
                    return sh->aid;
                if (id < 0)
                    id = sh->aid;
            }
        }
        if (id >= 0)
            return id;
        if (!lang)
            return -1;
    }
}

int demuxer_sub_track_by_lang_and_default(struct demuxer *d, char **langt)
{
    int n = 0;
    while (1) {
        char *lang = langt ? langt[n++] : NULL;
        int id = -1;
        for (int i = 0; i < MAX_S_STREAMS; i++) {
            struct sh_sub *sh = d->s_streams[i];
            if (sh && (!lang || sh->lang && !strcmp(lang, sh->lang))) {
                if (sh->default_track)
                    return sh->sid;
                if (id < 0)
                    id = sh->sid;
            }
        }
        if (!lang)
            return -1;
        if (id >= 0)
            return id;
    }
}
