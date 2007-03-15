#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>
#include <limits.h>
#include "config.h"
#include "version.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "aviheader.h"
#include "ms_hdr.h"

#include "stream/stream.h"
#include "muxer.h"
#include "demuxer.h"
#include "stheader.h"
#include "m_option.h"
#ifdef USE_LIBAVFORMAT_SO
#include <ffmpeg/avformat.h>
#else
#include "avformat.h"
#endif

extern const struct AVCodecTag *mp_wav_taglists[];
extern const struct AVCodecTag *mp_bmp_taglists[];

extern char *info_name;
extern char *info_artist;
extern char *info_genre;
extern char *info_subject;
extern char *info_copyright;
extern char *info_sourceform;
extern char *info_comment;

void pstrcpy(char *buf, int buf_size, const char *str);

typedef struct {
	//AVInputFormat *avif;
	AVFormatContext *oc;
	ByteIOContext pb;
	int audio_streams;
	int video_streams;
	int64_t last_pts;
} muxer_priv_t;

typedef struct {
	int64_t last_pts;
	AVStream *avstream;
} muxer_stream_priv_t;

static char *conf_format = NULL;
static int conf_allow_lavf = 0;
static int mux_rate= 0;
static int mux_packet_size= 0;
static float mux_preload= 0.5;
static float mux_max_delay= 0.7;

m_option_t lavfopts_conf[] = {
	{"format", &(conf_format), CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"i_certify_that_my_video_stream_does_not_use_b_frames", &conf_allow_lavf, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"muxrate", &mux_rate, CONF_TYPE_INT, CONF_RANGE, 0, INT_MAX, NULL},
	{"packetsize", &mux_packet_size, CONF_TYPE_INT, CONF_RANGE, 0, INT_MAX, NULL},
	{"preload", &mux_preload, CONF_TYPE_FLOAT, CONF_RANGE, 0, INT_MAX, NULL},
	{"delay", &mux_max_delay, CONF_TYPE_FLOAT, CONF_RANGE, 0, INT_MAX, NULL},

	{NULL, NULL, 0, 0, 0, 0, NULL}
};

static int mp_open(URLContext *h, const char *filename, int flags)
{
	return 0;
}

static int mp_close(URLContext *h)
{
	return 0;
}


static int mp_read(URLContext *h, unsigned char *buf, int size)
{
	mp_msg(MSGT_MUXER, MSGL_WARN, "READ %d\n", size);
	return -1;
}

static int mp_write(URLContext *h, unsigned char *buf, int size)
{
	muxer_t *muxer = (muxer_t*)h->priv_data;
	return stream_write_buffer(muxer->stream, buf, size);
}

static offset_t mp_seek(URLContext *h, offset_t pos, int whence)
{
	muxer_t *muxer = (muxer_t*)h->priv_data;
	if(whence == SEEK_CUR)
	{
		off_t cur = stream_tell(muxer->stream);
		if(cur == -1)
			return -1;
		pos += cur;
	}
	else if(whence == SEEK_END)
	{
		off_t size=0;
		if(stream_control(muxer->stream, STREAM_CTRL_GET_SIZE, &size) == STREAM_UNSUPORTED || size < pos)
			return -1;
		pos = size - pos;
	}
	mp_msg(MSGT_MUXER, MSGL_DBG2, "SEEK %"PRIu64"\n", (int64_t)pos);
	if(!stream_seek(muxer->stream, pos))
		return -1;
	return 0;
}


static URLProtocol mp_protocol = {
	"menc",
	mp_open,
	mp_read,
	mp_write,
	mp_seek,
	mp_close,
	NULL
};

static muxer_stream_t* lavf_new_stream(muxer_t *muxer, int type)
{
	muxer_priv_t *priv = muxer->priv;
	muxer_stream_t *stream;
	muxer_stream_priv_t *spriv;
	AVCodecContext *ctx;

	if(!muxer || (type != MUXER_TYPE_VIDEO && type != MUXER_TYPE_AUDIO))
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "UNKNOW TYPE %d\n", type);
		return NULL;
	}
	
	stream = calloc(1, sizeof(muxer_stream_t));
	if(!stream)
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "Could not alloc muxer_stream, EXIT\n");
		return NULL;
	}
	muxer->streams[muxer->avih.dwStreams] = stream;
	stream->b_buffer = malloc(2048);
	if(!stream->b_buffer)
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "Could not alloc b_buffer, EXIT\n");
		free(stream);
		return NULL;
	}
	stream->b_buffer_size = 2048;
	stream->b_buffer_ptr = 0;
	stream->b_buffer_len = 0;
	
	spriv = calloc(1, sizeof(muxer_stream_priv_t));
	if(!spriv) 
	{
		free(stream);
		return NULL;
	}
	stream->priv = spriv;
	
	spriv->avstream = av_new_stream(priv->oc, 1);
	if(!spriv->avstream) 
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "Could not alloc avstream, EXIT\n");
		return NULL;
	}
	spriv->avstream->stream_copy = 1;
	
	ctx = spriv->avstream->codec;
	ctx->codec_id = muxer->avih.dwStreams;
	switch(type) 
	{
		case MUXER_TYPE_VIDEO:
			ctx->codec_type = CODEC_TYPE_VIDEO;
			break;
		case MUXER_TYPE_AUDIO:
			ctx->codec_type = CODEC_TYPE_AUDIO;
			break;
	}

	muxer->avih.dwStreams++;
	stream->muxer = muxer;
	stream->type = type;
	mp_msg(MSGT_MUXER, MSGL_V, "ALLOCATED STREAM N. %d, type=%d\n", muxer->avih.dwStreams, type);
	return stream;
}


static void fix_parameters(muxer_stream_t *stream)
{
	muxer_stream_priv_t *spriv = (muxer_stream_priv_t *) stream->priv;
	AVCodecContext *ctx;
	
	ctx = spriv->avstream->codec;

        ctx->bit_rate= stream->avg_rate;
        if(stream->wf && stream->wf->nAvgBytesPerSec && !ctx->bit_rate)
            ctx->bit_rate = stream->wf->nAvgBytesPerSec * 8;
        ctx->rc_buffer_size= stream->vbv_size;
        ctx->rc_max_rate= stream->max_rate;

	if(stream->type == MUXER_TYPE_AUDIO)
	{
		ctx->codec_id = av_codec_get_id(mp_wav_taglists, stream->wf->wFormatTag); 
#if 0 //breaks aac in mov at least
		ctx->codec_tag = codec_get_wav_tag(ctx->codec_id);
#endif
		mp_msg(MSGT_MUXER, MSGL_INFO, "AUDIO CODEC ID: %x, TAG: %x\n", ctx->codec_id, (uint32_t) ctx->codec_tag);
		ctx->sample_rate = stream->wf->nSamplesPerSec;
//                mp_msg(MSGT_MUXER, MSGL_INFO, "stream->h.dwSampleSize: %d\n", stream->h.dwSampleSize);
		ctx->channels = stream->wf->nChannels;
                if(stream->h.dwRate && (stream->h.dwScale * (int64_t)ctx->sample_rate) % stream->h.dwRate == 0)
                    ctx->frame_size= (stream->h.dwScale * (int64_t)ctx->sample_rate) / stream->h.dwRate;
                mp_msg(MSGT_MUXER, MSGL_V, "MUXER_LAVF(audio stream) frame_size: %d, scale: %u, sps: %u, rate: %u, ctx->block_align = stream->wf->nBlockAlign; %d=%d stream->wf->nAvgBytesPerSec:%d\n", 
			ctx->frame_size, stream->h.dwScale, ctx->sample_rate, stream->h.dwRate,
			ctx->block_align, stream->wf->nBlockAlign, stream->wf->nAvgBytesPerSec);
		ctx->block_align = stream->h.dwSampleSize;
		if(stream->wf+1 && stream->wf->cbSize)
		{
			ctx->extradata = av_malloc(stream->wf->cbSize);
			if(ctx->extradata != NULL)
			{
				ctx->extradata_size = stream->wf->cbSize;
				memcpy(ctx->extradata, stream->wf+1, ctx->extradata_size);
			}
			else
				mp_msg(MSGT_MUXER, MSGL_ERR, "MUXER_LAVF(audio stream) error! couldn't allocate %d bytes for extradata\n",
					stream->wf->cbSize);
		}
	}
	else if(stream->type == MUXER_TYPE_VIDEO)
	{
		ctx->codec_id = av_codec_get_id(mp_bmp_taglists, stream->bih->biCompression);
                if(ctx->codec_id <= 0 || force_fourcc)
                    ctx->codec_tag= stream->bih->biCompression;
		mp_msg(MSGT_MUXER, MSGL_INFO, "VIDEO CODEC ID: %d\n", ctx->codec_id);
		ctx->width = stream->bih->biWidth;
		ctx->height = stream->bih->biHeight;
		ctx->bit_rate = 800000;
		ctx->time_base.den = stream->h.dwRate;
		ctx->time_base.num = stream->h.dwScale;
		if(stream->bih+1 && (stream->bih->biSize > sizeof(BITMAPINFOHEADER)))
		{
			ctx->extradata_size = stream->bih->biSize - sizeof(BITMAPINFOHEADER);
			ctx->extradata = av_malloc(ctx->extradata_size);
			if(ctx->extradata != NULL)
				memcpy(ctx->extradata, stream->bih+1, ctx->extradata_size);
			else
			{
				mp_msg(MSGT_MUXER, MSGL_ERR, "MUXER_LAVF(video stream) error! couldn't allocate %d bytes for extradata\n",
					ctx->extradata_size);
				ctx->extradata_size = 0;
			}
		}
	}
}

static void write_chunk(muxer_stream_t *stream, size_t len, unsigned int flags, double dts, double pts)
{
	muxer_t *muxer = (muxer_t*) stream->muxer;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	muxer_stream_priv_t *spriv = (muxer_stream_priv_t *) stream->priv;
	AVPacket pkt;
	
	if(len)
	{
	av_init_packet(&pkt);
	pkt.size = len;
	pkt.stream_index= spriv->avstream->index;
	pkt.data = stream->buffer;
	
	if(flags & AVIIF_KEYFRAME)
		pkt.flags |= PKT_FLAG_KEY;
	else
		pkt.flags = 0;
	
	
	//pkt.pts = AV_NOPTS_VALUE; 
	pkt.pts = (stream->timer / av_q2d(priv->oc->streams[pkt.stream_index]->time_base) + 0.5);
//fprintf(stderr, "%Ld %Ld id:%d tb:%f %f\n", pkt.dts, pkt.pts, pkt.stream_index, av_q2d(priv->oc->streams[pkt.stream_index]->time_base), stream->timer);
	
	if(av_interleaved_write_frame(priv->oc, &pkt) != 0) //av_write_frame(priv->oc, &pkt)
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "Error while writing frame\n");
	}
	}
	
	return;
}


static void write_header(muxer_t *muxer)
{
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	
	mp_msg(MSGT_MUXER, MSGL_INFO, MSGTR_WritingHeader);
	av_write_header(priv->oc);
	muxer->cont_write_header = NULL;
}


static void write_trailer(muxer_t *muxer)
{
	int i;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	
	mp_msg(MSGT_MUXER, MSGL_INFO, MSGTR_WritingTrailer);
	av_write_trailer(priv->oc);
	for(i = 0; i < priv->oc->nb_streams; i++) 
	{
		av_freep(&(priv->oc->streams[i]));
	}

	url_fclose(&(priv->oc->pb));

	av_free(priv->oc);
}

static void list_formats(void) {
	AVOutputFormat *fmt;
	mp_msg(MSGT_DEMUX, MSGL_INFO, "Available lavf output formats:\n");
	for (fmt = first_oformat; fmt; fmt = fmt->next)
		mp_msg(MSGT_DEMUX, MSGL_INFO, "%15s : %s\n", fmt->name, fmt->long_name);
}

extern char *out_filename;
int muxer_init_muxer_lavf(muxer_t *muxer)
{
	muxer_priv_t *priv;
	AVOutputFormat *fmt = NULL;
	char mp_filename[256] = "menc://stream.dummy";

	av_register_all();

	if (conf_format && strcmp(conf_format, "help") == 0) {
		list_formats();
		return 0;
	}

	mp_msg(MSGT_MUXER, MSGL_WARN, "** MUXER_LAVF *****************************************************************\n");
	if (!conf_allow_lavf) {
		mp_msg(MSGT_MUXER, MSGL_FATAL,
"If you wish to use libavformat muxing, you must ensure that your video stream\n"
"does not contain B frames (out of order decoding) and specify:\n"
"    -lavfopts i_certify_that_my_video_stream_does_not_use_b_frames\n"
"on the command line.\n");
	} else {
		mp_msg(MSGT_MUXER, MSGL_WARN,
"You have certified that your video stream does not contain B frames.\n");
	}
	mp_msg(MSGT_MUXER, MSGL_WARN,
"REMEMBER: MEncoder's libavformat muxing is presently broken and will generate\n"
"INCORRECT files in the presence of B frames. Moreover, due to bugs MPlayer\n"
"will play these INCORRECT files as if nothing were wrong!\n"
"*******************************************************************************\n");
	if (!conf_allow_lavf) return 0;
	
	priv = (muxer_priv_t *) calloc(1, sizeof(muxer_priv_t));
	if(priv == NULL)
		return 0;

	priv->oc = av_alloc_format_context();
	if(!priv->oc) 
	{
		mp_msg(MSGT_MUXER, MSGL_FATAL, "Couldn't get format context\n");
		goto fail;
	}

	if(conf_format)		
		fmt = guess_format(conf_format, NULL, NULL);
	if(! fmt)
		fmt = guess_format(NULL, out_filename, NULL);
	if(! fmt)
	{
		mp_msg(MSGT_MUXER, MSGL_FATAL, "CAN'T GET SPECIFIED FORMAT\n");
		goto fail;
	}
	priv->oc->oformat = fmt;

	
	if(av_set_parameters(priv->oc, NULL) < 0) 
	{
		mp_msg(MSGT_MUXER, MSGL_FATAL, "Invalid output format parameters\n");
		goto fail;
	}
	priv->oc->packet_size= mux_packet_size;
        priv->oc->mux_rate= mux_rate;
        priv->oc->preload= (int)(mux_preload*AV_TIME_BASE);
        priv->oc->max_delay= (int)(mux_max_delay*AV_TIME_BASE);
        if (info_name)
            pstrcpy(priv->oc->title    , sizeof(priv->oc->title    ), info_name     );
        if (info_artist)
            pstrcpy(priv->oc->author   , sizeof(priv->oc->author   ), info_artist   );
        if (info_genre)
            pstrcpy(priv->oc->genre    , sizeof(priv->oc->genre    ), info_genre    );
        if (info_copyright)
            pstrcpy(priv->oc->copyright, sizeof(priv->oc->copyright), info_copyright);
        if (info_comment)
            pstrcpy(priv->oc->comment  , sizeof(priv->oc->comment  ), info_comment  );
	register_protocol(&mp_protocol);

	if(url_fopen(&priv->oc->pb, mp_filename, URL_WRONLY))
	{
		mp_msg(MSGT_MUXER, MSGL_FATAL, "Coulnd't open outfile\n");
		goto fail;
        }
	
	((URLContext*)(priv->oc->pb.opaque))->priv_data= muxer;
	
	muxer->priv = (void *) priv;
	muxer->cont_new_stream = &lavf_new_stream;
	muxer->cont_write_chunk = &write_chunk;
	muxer->cont_write_header = &write_header;
	muxer->cont_write_index = &write_trailer;
	muxer->fix_stream_parameters = &fix_parameters;
	mp_msg(MSGT_MUXER, MSGL_INFO, "OK, exit\n");
	return 1;
	
fail:
	free(priv);
	return 0;
}
