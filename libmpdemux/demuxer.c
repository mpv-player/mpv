//=================== DEMUXER v2.5 =========================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "m_config.h"

#include "libvo/fastmemcpy.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "mf.h"

#include "libaf/af_format.h"

#ifdef CONFIG_ASS
#include "libass/ass.h"
#include "libass/ass_mp.h"
#endif

#ifdef CONFIG_LIBAVCODEC
#include "libavcodec/avcodec.h"
#if MP_INPUT_BUFFER_PADDING_SIZE < FF_INPUT_BUFFER_PADDING_SIZE
#error MP_INPUT_BUFFER_PADDING_SIZE is too small!
#endif
#endif

void resync_video_stream(sh_video_t *sh_video);
void resync_audio_stream(sh_audio_t *sh_audio);

// Demuxer list
extern const demuxer_desc_t demuxer_desc_rawaudio;
extern const demuxer_desc_t demuxer_desc_rawvideo;
extern const demuxer_desc_t demuxer_desc_tv;
extern const demuxer_desc_t demuxer_desc_mf;
extern const demuxer_desc_t demuxer_desc_avi;
extern const demuxer_desc_t demuxer_desc_y4m;
extern const demuxer_desc_t demuxer_desc_asf;
extern const demuxer_desc_t demuxer_desc_nuv;
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
    &demuxer_desc_rawaudio,
    &demuxer_desc_rawvideo,
#ifdef CONFIG_TV
    &demuxer_desc_tv,
#endif
    &demuxer_desc_mf,
#ifdef CONFIG_LIBAVFORMAT
    &demuxer_desc_lavf_preferred,
#endif
    &demuxer_desc_avi,
    &demuxer_desc_y4m,
    &demuxer_desc_asf,
    &demuxer_desc_nsv,
    &demuxer_desc_nuv,
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
#ifdef CONFIG_LIBAVFORMAT
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

void free_demuxer_stream(demux_stream_t *ds)
{
    ds_free_packs(ds);
    free(ds);
}

demux_stream_t *new_demuxer_stream(struct demuxer_st *demuxer, int id)
{
    demux_stream_t *ds = malloc(sizeof(demux_stream_t));
    ds->buffer_pos = ds->buffer_size = 0;
    ds->buffer = NULL;
    ds->pts = 0;
    ds->pts_bytes = 0;
    ds->eof = 0;
    ds->pos = 0;
    ds->dpos = 0;
    ds->pack_no = 0;

    ds->packs = 0;
    ds->bytes = 0;
    ds->first = ds->last = ds->current = NULL;
    ds->id = id;
    ds->demuxer = demuxer;

    ds->asf_seq = -1;
    ds->asf_packet = NULL;

    ds->ss_mul = ds->ss_div = 0;

    ds->sh = NULL;
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


demuxer_t *new_demuxer(stream_t *stream, int type, int a_id, int v_id,
                       int s_id, char *filename)
{
    demuxer_t *d = malloc(sizeof(demuxer_t));
    memset(d, 0, sizeof(demuxer_t));
    d->stream = stream;
    d->stream_pts = MP_NOPTS_VALUE;
    d->reference_clock = MP_NOPTS_VALUE;
    d->movi_start = stream->start_pos;
    d->movi_end = stream->end_pos;
    d->seekable = 1;
    d->synced = 0;
    d->filepos = 0;
    d->audio = new_demuxer_stream(d, a_id);
    d->video = new_demuxer_stream(d, v_id);
    d->sub = new_demuxer_stream(d, s_id);
    d->type = type;
    if (type)
        if (!(d->desc = get_demuxer_desc_from_type(type)))
            mp_msg(MSGT_DEMUXER, MSGL_ERR,
                   "BUG! Invalid demuxer type in new_demuxer(), "
                   "big troubles ahead.");
    if (filename) // Filename hack for avs_check_file
        d->filename = strdup(filename);
    stream_reset(stream);
    stream_seek(stream, stream->start_pos);
    return d;
}

extern int dvdsub_id;

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
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_SUBTITLE_ID=%d\n", sid);
    }
    return demuxer->s_streams[id];
}

void free_sh_sub(sh_sub_t *sh)
{
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_sub at %p\n", sh);
    free(sh->extradata);
#ifdef CONFIG_ASS
    if (sh->ass_track)
        ass_free_track(sh->ass_track);
#endif
    free(sh->lang);
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
    if (demuxer->a_streams[id])
        mp_msg(MSGT_DEMUXER, MSGL_WARN, MSGTR_AudioStreamRedefined, id);
    else {
        sh_audio_t *sh = calloc(1, sizeof(sh_audio_t));
        mp_msg(MSGT_DEMUXER, MSGL_V, MSGTR_FoundAudioStream, id);
        demuxer->a_streams[id] = sh;
        sh->aid = aid;
        sh->ds = demuxer->audio;
        // set some defaults
        sh->samplesize = 2;
        sh->sample_format = AF_FORMAT_S16_NE;
        sh->audio_out_minsize = 8192;   /* default size, maybe not enough for Win32/ACM */
        sh->pts = MP_NOPTS_VALUE;
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
        mp_msg(MSGT_DEMUXER, MSGL_WARN, MSGTR_VideoStreamRedefined, id);
    else {
        sh_video_t *sh = calloc(1, sizeof(sh_video_t));
        mp_msg(MSGT_DEMUXER, MSGL_V, MSGTR_FoundVideoStream, id);
        demuxer->v_streams[id] = sh;
        sh->vid = vid;
        sh->ds = demuxer->video;
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_ID=%d\n", vid);
    }
    return demuxer->v_streams[id];
}

void free_sh_video(sh_video_t *sh)
{
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing sh_video at %p\n", sh);
    free(sh->bih);
    free(sh);
}

void free_demuxer(demuxer_t *demuxer)
{
    int i;
    mp_msg(MSGT_DEMUXER, MSGL_DBG2, "DEMUXER: freeing demuxer at %p\n",
           demuxer);
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
    if (demuxer->info) {
        for (i = 0; demuxer->info[i] != NULL; i++)
            free(demuxer->info[i]);
        free(demuxer->info);
    }
    free(demuxer->filename);
    if (demuxer->chapters) {
        for (i = 0; i < demuxer->num_chapters; i++)
            free(demuxer->chapters[i].name);
        free(demuxer->chapters);
    }
    if (demuxer->attachments) {
        for (i = 0; i < demuxer->num_attachments; i++) {
            free(demuxer->attachments[i].name);
            free(demuxer->attachments[i].type);
            free(demuxer->attachments[i].data);
        }
        free(demuxer->attachments);
    }
    free(demuxer);
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
#define MAX_ACUMULATED_PACKETS 64
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
#if 0
            if (demux->reference_clock != MP_NOPTS_VALUE) {
                if (   p->pts != MP_NOPTS_VALUE
                    && p->pts >  demux->reference_clock
                    && ds->packs < MAX_ACUMULATED_PACKETS) {
                    if (demux_fill_buffer(demux, ds))
                        continue;
                }
            }
#endif
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
            return 1;
        }
        if (demux->audio->packs >= MAX_PACKS
            || demux->audio->bytes >= MAX_PACK_BYTES) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_TooManyAudioInBuffer,
                   demux->audio->packs, demux->audio->bytes);
            mp_msg(MSGT_DEMUXER, MSGL_HINT, MSGTR_MaybeNI);
            break;
        }
        if (demux->video->packs >= MAX_PACKS
            || demux->video->bytes >= MAX_PACK_BYTES) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_TooManyVideoInBuffer,
                   demux->video->packs, demux->video->bytes);
            mp_msg(MSGT_DEMUXER, MSGL_HINT, MSGTR_MaybeNI);
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
    if (ds->buffer_pos >= ds->buffer_size) {
        if (!ds_fill_buffer(ds)) {
            // EOF
            *start = NULL;
            return -1;
        }
    }
    // Return pts unless this read starts from the middle of a packet
    if (!ds->buffer_pos)
        *pts = ds->current->pts;
    len = ds->buffer_size - ds->buffer_pos;
    *start = &ds->buffer[ds->buffer_pos];
    ds->buffer_pos += len;
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
    while (!ds->first) {
        if (demux->audio->packs >= MAX_PACKS
            || demux->audio->bytes >= MAX_PACK_BYTES) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_TooManyAudioInBuffer,
                   demux->audio->packs, demux->audio->bytes);
            mp_msg(MSGT_DEMUXER, MSGL_HINT, MSGTR_MaybeNI);
            return MP_NOPTS_VALUE;
        }
        if (demux->video->packs >= MAX_PACKS
            || demux->video->bytes >= MAX_PACK_BYTES) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_TooManyVideoInBuffer,
                   demux->video->packs, demux->video->bytes);
            mp_msg(MSGT_DEMUXER, MSGL_HINT, MSGTR_MaybeNI);
            return MP_NOPTS_VALUE;
        }
        if (!demux_fill_buffer(demux, ds))
            return MP_NOPTS_VALUE;
    }
    return ds->first->pts;
}

// ====================================================================

void demuxer_help(void)
{
    int i;

    mp_msg(MSGT_DEMUXER, MSGL_INFO, "Available demuxers:\n");
    mp_msg(MSGT_DEMUXER, MSGL_INFO, " demuxer:  type  info:  (comment)\n");
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_DEMUXERS\n");
    for (i = 0; demuxer_list[i]; i++) {
        if (demuxer_list[i]->type > DEMUXER_TYPE_MAX)   // Don't display special demuxers
            continue;
        if (demuxer_list[i]->comment && strlen(demuxer_list[i]->comment))
            mp_msg(MSGT_DEMUXER, MSGL_INFO, "%10s  %2d   %s (%s)\n",
                   demuxer_list[i]->name, demuxer_list[i]->type,
                   demuxer_list[i]->info, demuxer_list[i]->comment);
        else
            mp_msg(MSGT_DEMUXER, MSGL_INFO, "%10s  %2d   %s\n",
                   demuxer_list[i]->name, demuxer_list[i]->type,
                   demuxer_list[i]->info);
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
int get_demuxer_type_from_name(char *demuxer_name, int *force)
{
    int i;
    long type_int;
    char *endptr;

    if (!demuxer_name || !demuxer_name[0])
        return DEMUXER_TYPE_UNKNOWN;
    if (force)
        *force = demuxer_name[0] == '+';
    if (demuxer_name[0] == '+')
        demuxer_name = &demuxer_name[1];
    for (i = 0; demuxer_list[i]; i++) {
        if (demuxer_list[i]->type > DEMUXER_TYPE_MAX)   // Can't select special demuxers from commandline
            continue;
        if (strcmp(demuxer_name, demuxer_list[i]->name) == 0)
            return demuxer_list[i]->type;
    }

    // No match found, try to parse name as an integer (demuxer number)
    type_int = strtol(demuxer_name, &endptr, 0);
    if (*endptr)  // Conversion failed
        return -1;
    if ((type_int > 0) && (type_int <= DEMUXER_TYPE_MAX))
        return (int) type_int;

    return -1;
}

int extension_parsing = 1; // 0=off 1=mixed (used only for unstable formats)

int correct_pts = 0;
int user_correct_pts = -1;

/*
  NOTE : Several demuxers may be opened at the same time so
  demuxers should NEVER rely on an external var to enable them
  self. If a demuxer can't do any auto-detection it should only use
  file_format. The user can explicitly set file_format with the -demuxer
  option so there is really no need for another extra var.
  For convenience an option can be added to set file_format directly
  to the right type (ex: rawaudio,rawvideo).
  Also the stream can override the file_format so a demuxer which rely
  on a special stream type can set file_format at the stream level
  (ex: tv,mf).
*/

static demuxer_t *demux_open_stream(stream_t *stream, int file_format,
                                    int force, int audio_id, int video_id,
                                    int dvdsub_id, char *filename)
{
    demuxer_t *demuxer = NULL;

    sh_video_t *sh_video = NULL;

    const demuxer_desc_t *demuxer_desc;
    int fformat = 0;
    int i;

    // If somebody requested a demuxer check it
    if (file_format) {
        if ((demuxer_desc = get_demuxer_desc_from_type(file_format))) {
            demuxer = new_demuxer(stream, demuxer_desc->type, audio_id,
                                  video_id, dvdsub_id, filename);
            if (demuxer_desc->check_file)
                fformat = demuxer_desc->check_file(demuxer);
            if (force || !demuxer_desc->check_file)
                fformat = demuxer_desc->type;
            if (fformat != 0) {
                if (fformat == demuxer_desc->type) {
                    demuxer_t *demux2 = demuxer;
                    // Move messages to demuxer detection code?
                    mp_msg(MSGT_DEMUXER, MSGL_INFO,
                           MSGTR_Detected_XXX_FileFormat,
                           demuxer_desc->shortdesc);
                    file_format = fformat;
                    if (!demuxer->desc->open
                        || (demux2 = demuxer->desc->open(demuxer))) {
                        demuxer = demux2;
                        goto dmx_open;
                    }
                } else {
                    // Format changed after check, recurse
                    free_demuxer(demuxer);
                    return demux_open_stream(stream, fformat, force, audio_id,
                                             video_id, dvdsub_id, filename);
                }
            }
            // Check failed for forced demuxer, quit
            free_demuxer(demuxer);
            return NULL;
        }
    }
    // Test demuxers with safe file checks
    for (i = 0; (demuxer_desc = demuxer_list[i]); i++) {
        if (demuxer_desc->safe_check) {
            demuxer = new_demuxer(stream, demuxer_desc->type, audio_id,
                                  video_id, dvdsub_id, filename);
            if ((fformat = demuxer_desc->check_file(demuxer)) != 0) {
                if (fformat == demuxer_desc->type) {
                    demuxer_t *demux2 = demuxer;
                    mp_msg(MSGT_DEMUXER, MSGL_INFO,
                           MSGTR_Detected_XXX_FileFormat,
                           demuxer_desc->shortdesc);
                    file_format = fformat;
                    if (!demuxer->desc->open
                        || (demux2 = demuxer->desc->open(demuxer))) {
                        demuxer = demux2;
                        goto dmx_open;
                    }
                } else {
                    if (fformat == DEMUXER_TYPE_PLAYLIST)
                        return demuxer; // handled in mplayer.c
                    // Format changed after check, recurse
                    free_demuxer(demuxer);
                    demuxer = demux_open_stream(stream, fformat, force,
                                                audio_id, video_id,
                                                dvdsub_id, filename);
                    if (demuxer)
                        return demuxer; // done!
                    file_format = DEMUXER_TYPE_UNKNOWN;
                }
            }
            free_demuxer(demuxer);
            demuxer = NULL;
        }
    }

    // If no forced demuxer perform file extension based detection
    // Ok. We're over the stable detectable fileformats, the next ones are
    // a bit fuzzy. So by default (extension_parsing==1) try extension-based
    // detection first:
    if (file_format == DEMUXER_TYPE_UNKNOWN && filename
        && extension_parsing == 1) {
        file_format = demuxer_type_by_filename(filename);
        if (file_format != DEMUXER_TYPE_UNKNOWN) {
            // we like recursion :)
            demuxer = demux_open_stream(stream, file_format, force, audio_id,
                                        video_id, dvdsub_id, filename);
            if (demuxer)
                return demuxer; // done!
            file_format = DEMUXER_TYPE_UNKNOWN; // continue fuzzy guessing...
            mp_msg(MSGT_DEMUXER, MSGL_V,
                   "demuxer: continue fuzzy content-based format guessing...\n");
        }
    }
    // Try detection for all other demuxers
    for (i = 0; (demuxer_desc = demuxer_list[i]); i++) {
        if (!demuxer_desc->safe_check && demuxer_desc->check_file) {
            demuxer = new_demuxer(stream, demuxer_desc->type, audio_id,
                                  video_id, dvdsub_id, filename);
            if ((fformat = demuxer_desc->check_file(demuxer)) != 0) {
                if (fformat == demuxer_desc->type) {
                    demuxer_t *demux2 = demuxer;
                    mp_msg(MSGT_DEMUXER, MSGL_INFO,
                           MSGTR_Detected_XXX_FileFormat,
                           demuxer_desc->shortdesc);
                    file_format = fformat;
                    if (!demuxer->desc->open
                        || (demux2 = demuxer->desc->open(demuxer))) {
                        demuxer = demux2;
                        goto dmx_open;
                    }
                } else {
                    if (fformat == DEMUXER_TYPE_PLAYLIST)
                        return demuxer; // handled in mplayer.c
                    // Format changed after check, recurse
                    free_demuxer(demuxer);
                    demuxer = demux_open_stream(stream, fformat, force,
                                                audio_id, video_id,
                                                dvdsub_id, filename);
                    if (demuxer)
                        return demuxer; // done!
                    file_format = DEMUXER_TYPE_UNKNOWN;
                }
            }
            free_demuxer(demuxer);
            demuxer = NULL;
        }
    }

    return NULL;
    //====== File format recognized, set up these for compatibility: =========
 dmx_open:

    demuxer->file_format = file_format;

    if ((sh_video = demuxer->video->sh) && sh_video->bih) {
        int biComp = le2me_32(sh_video->bih->biCompression);
        mp_msg(MSGT_DEMUX, MSGL_INFO,
               "VIDEO:  [%.4s]  %dx%d  %dbpp  %5.3f fps  %5.1f kbps (%4.1f kbyte/s)\n",
               (char *) &biComp, sh_video->bih->biWidth,
               sh_video->bih->biHeight, sh_video->bih->biBitCount,
               sh_video->fps, sh_video->i_bps * 0.008f,
               sh_video->i_bps / 1024.0f);
    }
#ifdef CONFIG_ASS
    if (ass_enabled && ass_library) {
        for (i = 0; i < MAX_S_STREAMS; ++i) {
            sh_sub_t *sh = demuxer->s_streams[i];
            if (sh && sh->type == 'a') {
                sh->ass_track = ass_new_track(ass_library);
                if (sh->ass_track && sh->extradata)
                    ass_process_codec_private(sh->ass_track, sh->extradata,
                                              sh->extradata_len);
            } else if (sh && sh->type != 'v')
                sh->ass_track = ass_default_track(ass_library);
        }
    }
#endif
    return demuxer;
}

char *audio_stream = NULL;
char *sub_stream = NULL;
int audio_stream_cache = 0;

char *demuxer_name = NULL;       // parameter from -demuxer
char *audio_demuxer_name = NULL; // parameter from -audio-demuxer
char *sub_demuxer_name = NULL;   // parameter from -sub-demuxer

extern int hr_mp3_seek;

extern float stream_cache_min_percent;
extern float stream_cache_seek_min_percent;

demuxer_t *demux_open(stream_t *vs, int file_format, int audio_id,
                      int video_id, int dvdsub_id, char *filename)
{
    stream_t *as = NULL, *ss = NULL;
    demuxer_t *vd, *ad = NULL, *sd = NULL;
    demuxer_t *res;
    int afmt = DEMUXER_TYPE_UNKNOWN, sfmt = DEMUXER_TYPE_UNKNOWN;
    int demuxer_type;
    int audio_demuxer_type = 0, sub_demuxer_type = 0;
    int demuxer_force = 0, audio_demuxer_force = 0, sub_demuxer_force = 0;

    if ((demuxer_type =
         get_demuxer_type_from_name(demuxer_name, &demuxer_force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "-demuxer %s does not exist.\n",
               demuxer_name);
    }
    if ((audio_demuxer_type =
         get_demuxer_type_from_name(audio_demuxer_name,
                                    &audio_demuxer_force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "-audio-demuxer %s does not exist.\n",
               audio_demuxer_name);
    }
    if ((sub_demuxer_type =
         get_demuxer_type_from_name(sub_demuxer_name,
                                    &sub_demuxer_force)) < 0) {
        mp_msg(MSGT_DEMUXER, MSGL_ERR, "-sub-demuxer %s does not exist.\n",
               sub_demuxer_name);
    }

    if (audio_stream) {
        as = open_stream(audio_stream, 0, &afmt);
        if (!as) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_CannotOpenAudioStream,
                   audio_stream);
            return NULL;
        }
        if (audio_stream_cache) {
            if (!stream_enable_cache
                (as, audio_stream_cache * 1024,
                 audio_stream_cache * 1024 * (stream_cache_min_percent /
                                              100.0),
                 audio_stream_cache * 1024 * (stream_cache_seek_min_percent /
                                              100.0))) {
                free_stream(as);
                mp_msg(MSGT_DEMUXER, MSGL_ERR,
                       "Can't enable audio stream cache\n");
                return NULL;
            }
        }
    }
    if (sub_stream) {
        ss = open_stream(sub_stream, 0, &sfmt);
        if (!ss) {
            mp_msg(MSGT_DEMUXER, MSGL_ERR, MSGTR_CannotOpenSubtitlesStream,
                   sub_stream);
            return NULL;
        }
    }

    vd = demux_open_stream(vs, demuxer_type ? demuxer_type : file_format,
                           demuxer_force, audio_stream ? -2 : audio_id,
                           video_id, sub_stream ? -2 : dvdsub_id, filename);
    if (!vd) {
        if (as)
            free_stream(as);
        if (ss)
            free_stream(ss);
        return NULL;
    }
    if (as) {
        ad = demux_open_stream(as,
                               audio_demuxer_type ? audio_demuxer_type : afmt,
                               audio_demuxer_force, audio_id, -2, -2,
                               audio_stream);
        if (!ad) {
            mp_msg(MSGT_DEMUXER, MSGL_WARN, MSGTR_OpeningAudioDemuxerFailed,
                   audio_stream);
            free_stream(as);
        } else if (ad->audio->sh
                   && ((sh_audio_t *) ad->audio->sh)->format == 0x55) // MP3
            hr_mp3_seek = 1;    // Enable high res seeking
    }
    if (ss) {
        sd = demux_open_stream(ss, sub_demuxer_type ? sub_demuxer_type : sfmt,
                               sub_demuxer_force, -2, -2, dvdsub_id,
                               sub_stream);
        if (!sd) {
            mp_msg(MSGT_DEMUXER, MSGL_WARN,
                   MSGTR_OpeningSubtitlesDemuxerFailed, sub_stream);
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

    correct_pts = user_correct_pts;
    if (correct_pts < 0)
        correct_pts = demux_control(res, DEMUXER_CTRL_CORRECT_PTS, NULL)
                      == DEMUXER_CTRL_OK;
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
    demux_stream_t *d_audio = demuxer->audio;
    demux_stream_t *d_video = demuxer->video;
    sh_audio_t *sh_audio = d_audio->sh;
    sh_video_t *sh_video = d_video->sh;
    double tmp = 0;
    double pts;

    if (!demuxer->seekable) {
        if (demuxer->file_format == DEMUXER_TYPE_AVI)
            mp_msg(MSGT_SEEK, MSGL_WARN, MSGTR_CantSeekRawAVI);
#ifdef CONFIG_TV
        else if (demuxer->file_format == DEMUXER_TYPE_TV)
            mp_msg(MSGT_SEEK, MSGL_WARN, MSGTR_TVInputNotSeekable);
#endif
        else
            mp_msg(MSGT_SEEK, MSGL_WARN, MSGTR_CantSeekFile);
        return 0;
    }

    demux_flush(demuxer);
    // clear demux buffers:
    if (sh_audio)
        sh_audio->a_buffer_len = 0;

    demuxer->stream->eof = 0;
    demuxer->video->eof = 0;
    demuxer->audio->eof = 0;

    if (sh_video)
        sh_video->timer = 0;    // !!!!!!

    if (flags & SEEK_ABSOLUTE)
        pts = 0.0f;
    else {
        if (demuxer->stream_pts == MP_NOPTS_VALUE)
            goto dmx_seek;
        pts = demuxer->stream_pts;
    }

    if (flags & SEEK_FACTOR) {
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_TIME_LENGTH, &tmp)
            == STREAM_UNSUPPORTED)
            goto dmx_seek;
        pts += tmp * rel_seek_secs;
    } else
        pts += rel_seek_secs;

    if (stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_TIME, &pts) !=
        STREAM_UNSUPPORTED) {
        demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);
        return 1;
    }

  dmx_seek:
    if (demuxer->desc->seek)
        demuxer->desc->seek(demuxer, rel_seek_secs, audio_delay, flags);

    if (sh_audio)
        resync_audio_stream(sh_audio);

    return 1;
}

int demux_info_add(demuxer_t *demuxer, const char *opt, const char *param)
{
    char **info = demuxer->info;
    int n = 0;


    for (n = 0; info && info[2 * n] != NULL; n++) {
        if (!strcasecmp(opt, info[2 * n])) {
            mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_DemuxerInfoChanged, opt,
                   param);
            free(info[2 * n + 1]);
            info[2 * n + 1] = strdup(param);
            return 0;
        }
    }

    info = demuxer->info = (char **) realloc(info,
                                             (2 * (n + 2)) * sizeof(char *));
    info[2 * n] = strdup(opt);
    info[2 * n + 1] = strdup(param);
    memset(&info[2 * (n + 1)], 0, 2 * sizeof(char *));

    return 1;
}

int demux_info_print(demuxer_t *demuxer)
{
    char **info = demuxer->info;
    int n;

    if (!info)
        return 0;

    mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_ClipInfo);
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



double demuxer_get_time_length(demuxer_t *demuxer)
{
    double get_time_ans;
    sh_video_t *sh_video = demuxer->video->sh;
    sh_audio_t *sh_audio = demuxer->audio->sh;
    // <= 0 means DEMUXER_CTRL_NOTIMPL or DEMUXER_CTRL_DONTKNOW
    if (demux_control
        (demuxer, DEMUXER_CTRL_GET_TIME_LENGTH, (void *) &get_time_ans) <= 0) {
        if (sh_video && sh_video->i_bps && sh_audio && sh_audio->i_bps)
            get_time_ans = (double) (demuxer->movi_end -
                                     demuxer->movi_start) / (sh_video->i_bps +
                                                             sh_audio->i_bps);
        else if (sh_video && sh_video->i_bps)
            get_time_ans = (double) (demuxer->movi_end -
                                     demuxer->movi_start) / sh_video->i_bps;
        else if (sh_audio && sh_audio->i_bps)
            get_time_ans = (double) (demuxer->movi_end -
                                     demuxer->movi_start) / sh_audio->i_bps;
        else
            get_time_ans = 0;
    }
    return get_time_ans;
}

/**
 * \brief demuxer_get_current_time() returns the time of the current play in three possible ways:
 *        either when the stream reader satisfies STREAM_CTRL_GET_CURRENT_TIME (e.g. dvd)
 *        or using sh_video->pts when the former method fails
 *        0 otherwise
 * \return the current play time
 */
int demuxer_get_current_time(demuxer_t *demuxer)
{
    double get_time_ans = 0;
    sh_video_t *sh_video = demuxer->video->sh;
    if (demuxer->stream_pts != MP_NOPTS_VALUE)
        get_time_ans = demuxer->stream_pts;
    else if (sh_video)
        get_time_ans = sh_video->pts;
    return (int) get_time_ans;
}

int demuxer_get_percent_pos(demuxer_t *demuxer)
{
    int ans = 0;
    int res = demux_control(demuxer, DEMUXER_CTRL_GET_PERCENT_POS, &ans);
    int len = (demuxer->movi_end - demuxer->movi_start) / 100;
    if (res <= 0) {
        if (len > 0)
            ans = (demuxer->filepos - demuxer->movi_start) / len;
        else
            ans = 0;
    }
    if (ans < 0)
        ans = 0;
    if (ans > 100)
        ans = 100;
    return ans;
}

int demuxer_switch_audio(demuxer_t *demuxer, int index)
{
    int res = demux_control(demuxer, DEMUXER_CTRL_SWITCH_AUDIO, &index);
    if (res == DEMUXER_CTRL_NOTIMPL)
        index = demuxer->audio->id;
    if (demuxer->audio->id >= 0)
        demuxer->audio->sh = demuxer->a_streams[demuxer->audio->id];
    else
        demuxer->audio->sh = NULL;
    return index;
}

int demuxer_switch_video(demuxer_t *demuxer, int index)
{
    int res = demux_control(demuxer, DEMUXER_CTRL_SWITCH_VIDEO, &index);
    if (res == DEMUXER_CTRL_NOTIMPL)
        index = demuxer->video->id;
    if (demuxer->video->id >= 0)
        demuxer->video->sh = demuxer->v_streams[demuxer->video->id];
    else
        demuxer->video->sh = NULL;
    return index;
}

int demuxer_add_attachment(demuxer_t *demuxer, const char *name,
                           const char *type, const void *data, size_t size)
{
    if (!(demuxer->num_attachments & 31))
        demuxer->attachments = realloc(demuxer->attachments,
                (demuxer->num_attachments + 32) * sizeof(demux_attachment_t));

    demuxer->attachments[demuxer->num_attachments].name = strdup(name);
    demuxer->attachments[demuxer->num_attachments].type = strdup(type);
    demuxer->attachments[demuxer->num_attachments].data = malloc(size);
    memcpy(demuxer->attachments[demuxer->num_attachments].data, data, size);
    demuxer->attachments[demuxer->num_attachments].data_size = size;

    return demuxer->num_attachments++;
}

int demuxer_add_chapter(demuxer_t *demuxer, const char *name, uint64_t start,
                        uint64_t end)
{
    if (demuxer->chapters == NULL)
        demuxer->chapters = malloc(32 * sizeof(*demuxer->chapters));
    else if (!(demuxer->num_chapters % 32))
        demuxer->chapters = realloc(demuxer->chapters,
                                    (demuxer->num_chapters + 32) *
                                        sizeof(*demuxer->chapters));

    demuxer->chapters[demuxer->num_chapters].start = start;
    demuxer->chapters[demuxer->num_chapters].end = end;
    demuxer->chapters[demuxer->num_chapters].name = strdup(name ? name : MSGTR_Unknown);

    return demuxer->num_chapters++;
}

/**
 * \brief demuxer_seek_chapter() seeks to a chapter in two possible ways:
 *        either using the demuxer->chapters structure set by the demuxer
 *        or asking help to the stream layer (e.g. dvd)
 * \param chapter - chapter number wished - 0-based
 * \param mode 0: relative to current main pts, 1: absolute
 * \param seek_pts set by the function to the pts to seek to (if demuxer->chapters is set)
 * \param num_chapters number of chapters present (set by this function is param is not null)
 * \param chapter_name name of chapter found (set by this function is param is not null)
 * \return -1 on error, current chapter if successful
 */

int demuxer_seek_chapter(demuxer_t *demuxer, int chapter, int mode,
                         float *seek_pts, int *num_chapters,
                         char **chapter_name)
{
    int ris;
    int current, total;
    sh_video_t *sh_video = demuxer->video->sh;
    sh_audio_t *sh_audio = demuxer->audio->sh;

    if (!demuxer->num_chapters || !demuxer->chapters) {
        if (!mode) {
            ris = stream_control(demuxer->stream,
                                 STREAM_CTRL_GET_CURRENT_CHAPTER, &current);
            if (ris == STREAM_UNSUPPORTED)
                return -1;
            chapter += current;
        }

        demux_flush(demuxer);

        ris = stream_control(demuxer->stream, STREAM_CTRL_SEEK_TO_CHAPTER,
                             &chapter);
        if (ris != STREAM_UNSUPPORTED)
            demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);
        if (sh_video) {
            ds_fill_buffer(demuxer->video);
            resync_video_stream(sh_video);
        }

        if (sh_audio) {
            ds_fill_buffer(demuxer->audio);
            resync_audio_stream(sh_audio);
        }
        // exit status may be ok, but main() doesn't have to seek itself
        // (because e.g. dvds depend on sectors, not on pts)
        *seek_pts = -1.0;

        if (num_chapters) {
            if (stream_control(demuxer->stream, STREAM_CTRL_GET_NUM_CHAPTERS,
                               num_chapters) == STREAM_UNSUPPORTED)
                *num_chapters = 0;
        }

        if (chapter_name) {
            *chapter_name = NULL;
            if (num_chapters && *num_chapters) {
                char *tmp = malloc(16);
                if (tmp) {
                    sprintf(tmp, " of %3d", *num_chapters);
                    *chapter_name = tmp;
                }
            }
        }

        return ris != STREAM_UNSUPPORTED ? chapter : -1;
    } else {  // chapters structure is set in the demuxer
        total = demuxer->num_chapters;

        if (mode == 1)  //absolute seeking
            current = chapter;
        else {          //relative seeking
            uint64_t now;
            now = (sh_video ? sh_video->pts : (sh_audio ? sh_audio->pts : 0.))
                  * 1000 + .5;

            for (current = total - 1; current >= 0; --current) {
                demux_chapter_t *chapter = demuxer->chapters + current;
                if (chapter->start <= now)
                    break;
            }
            current += chapter;
        }

        if (current >= total)
            return -1;
        if (current < 0)
            current = 0;

        *seek_pts = demuxer->chapters[current].start / 1000.0;

        if (num_chapters)
            *num_chapters = demuxer->num_chapters;

        if (chapter_name) {
            if (demuxer->chapters[current].name)
                *chapter_name = strdup(demuxer->chapters[current].name);
            else
                *chapter_name = NULL;
        }

        return current;
    }
}

int demuxer_get_current_chapter(demuxer_t *demuxer)
{
    int chapter = -1;
    if (!demuxer->num_chapters || !demuxer->chapters) {
        if (stream_control(demuxer->stream, STREAM_CTRL_GET_CURRENT_CHAPTER,
                           &chapter) == STREAM_UNSUPPORTED)
            chapter = -1;
    } else {
        sh_video_t *sh_video = demuxer->video->sh;
        sh_audio_t *sh_audio = demuxer->audio->sh;
        uint64_t now;
        now = (sh_video ? sh_video->pts : (sh_audio ? sh_audio->pts : 0))
              * 1000 + 0.5;
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
            *end = demuxer->chapters[chapter].end / 1000.0;
        return demuxer->chapters[chapter].start / 1000.0;
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
    sh_video_t *sh_video = demuxer->video->sh;
    sh_audio_t *sh_audio = demuxer->audio->sh;

    angles = demuxer_angles_count(demuxer);
    if ((angles < 1) || (angle > angles))
        return -1;

    demux_flush(demuxer);

    ris = stream_control(demuxer->stream, STREAM_CTRL_SET_ANGLE, &angle);
    if (ris == STREAM_UNSUPPORTED)
        return -1;

    demux_control(demuxer, DEMUXER_CTRL_RESYNC, NULL);
    if (sh_video) {
        ds_fill_buffer(demuxer->video);
        resync_video_stream(sh_video);
    }

    if (sh_audio) {
        ds_fill_buffer(demuxer->audio);
        resync_audio_stream(sh_audio);
    }

    return angle;
}

int demuxer_audio_track_by_lang(demuxer_t *d, char *lang)
{
    int i, len;
    lang += strspn(lang, ",");
    while ((len = strcspn(lang, ",")) > 0) {
        for (i = 0; i < MAX_A_STREAMS; ++i) {
            sh_audio_t *sh = d->a_streams[i];
            if (sh && sh->lang && strncmp(sh->lang, lang, len) == 0)
                return sh->aid;
        }
        lang += len;
        lang += strspn(lang, ",");
    }
    return -1;
}

int demuxer_sub_track_by_lang(demuxer_t *d, char *lang)
{
    int i, len;
    lang += strspn(lang, ",");
    while ((len = strcspn(lang, ",")) > 0) {
        for (i = 0; i < MAX_S_STREAMS; ++i) {
            sh_sub_t *sh = d->s_streams[i];
            if (sh && sh->lang && strncmp(sh->lang, lang, len) == 0)
                return sh->sid;
        }
        lang += len;
        lang += strspn(lang, ",");
    }
    return -1;
}

int demuxer_default_audio_track(demuxer_t *d)
{
    int i;
    for (i = 0; i < MAX_A_STREAMS; ++i) {
        sh_audio_t *sh = d->a_streams[i];
        if (sh && sh->default_track)
            return sh->aid;
    }
    for (i = 0; i < MAX_A_STREAMS; ++i) {
        sh_audio_t *sh = d->a_streams[i];
        if (sh)
            return sh->aid;
    }
    return -1;
}

int demuxer_default_sub_track(demuxer_t *d)
{
    int i;
    for (i = 0; i < MAX_S_STREAMS; ++i) {
        sh_sub_t *sh = d->s_streams[i];
        if (sh && sh->default_track)
            return sh->sid;
    }
    return -1;
}
