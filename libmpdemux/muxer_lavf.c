#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>
#include "config.h"
#include "../version.h"
#include "../mp_msg.h"

#include "bswap.h"
#include "aviheader.h"
#include "ms_hdr.h"

#include "muxer.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "../m_option.h"
#include "avformat.h"

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

m_option_t lavfopts_conf[] = {
	{"format", &(conf_format), CONF_TYPE_STRING, 0, 0, 0, NULL},
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
	fprintf(stderr, "READ %d\n", size);
	return -1;
}

static int mp_write(URLContext *h, unsigned char *buf, int size)
{
	muxer_t *muxer = (muxer_t*)h->priv_data;
	return fwrite(buf, 1, size, muxer->file);
}

static offset_t mp_seek(URLContext *h, offset_t pos, int whence)
{
	muxer_t *muxer = (muxer_t*)h->priv_data;
	fprintf(stderr, "SEEK %llu\n", pos);
	return fseeko(muxer->file, pos, whence);
}


static URLProtocol mp_protocol = {
	"menc",
	mp_open,
	mp_read,
	mp_write,
	mp_seek,
	mp_close,
};

static muxer_stream_t* lavf_new_stream(muxer_t *muxer, int type)
{
	muxer_priv_t *priv = (muxer_priv_t*) muxer->priv;
	muxer_stream_t *stream;
	muxer_stream_priv_t *spriv;
	AVCodecContext *ctx;

	if(!muxer || (type != MUXER_TYPE_VIDEO && type != MUXER_TYPE_AUDIO)) 
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "UNKNOW TYPE %d\n", type);
		return NULL;
	}
	
	stream = (muxer_stream_t*) calloc(1, sizeof(muxer_stream_t));
	if(!stream)
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "Could not alloc muxer_stream, EXIT\n");
		return NULL;
	}
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
	
	spriv = (muxer_stream_priv_t*) calloc(1, sizeof(muxer_stream_priv_t));
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
	
	ctx = &(spriv->avstream->codec);
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


static void fix_parameters(muxer_stream_t *stream, void *sh)
{
	muxer_stream_priv_t *spriv = (muxer_stream_priv_t *) stream->priv;
	AVCodecContext *ctx;
	
	ctx = &(spriv->avstream->codec);
	
	if(stream->type == MUXER_TYPE_AUDIO)
	{
		ctx->codec_id = codec_get_wav_id(stream->wf->wFormatTag); 
		ctx->codec_tag = codec_get_wav_tag(ctx->codec_id);
		mp_msg(MSGT_MUXER, MSGL_INFO, "AUDIO CODEC ID: %x, TAG: %x\n", ctx->codec_id, (uint32_t) ctx->codec_tag);
		ctx->bit_rate = stream->wf->nAvgBytesPerSec * 8;
		ctx->sample_rate = stream->wf->nSamplesPerSec;
//                mp_msg(MSGT_MUXER, MSGL_INFO, "stream->h.dwSampleSize: %d\n", stream->h.dwSampleSize);
		ctx->channels = stream->wf->nChannels;
                if(stream->h.dwRate && (stream->h.dwScale * (int64_t)ctx->sample_rate) % stream->h.dwRate == 0)
                    ctx->frame_size= (stream->h.dwScale * (int64_t)ctx->sample_rate) / stream->h.dwRate;
//                printf("ctx->block_align = stream->wf->nBlockAlign; %d=%d stream->wf->nAvgBytesPerSec:%d\n", ctx->block_align, stream->wf->nBlockAlign, stream->wf->nAvgBytesPerSec);
		ctx->block_align = stream->wf->nBlockAlign;
	}
	else if(stream->type == MUXER_TYPE_VIDEO)
	{
		ctx->codec_id = codec_get_bmp_id(stream->bih->biCompression);
		mp_msg(MSGT_MUXER, MSGL_INFO, "VIDEO CODEC ID: %d\n", ctx->codec_id);
		ctx->width = stream->bih->biWidth;
		ctx->height = stream->bih->biHeight;
		ctx->bit_rate = 800000;
		ctx->frame_rate = stream->h.dwRate;
		ctx->frame_rate_base = stream->h.dwScale;
	}
}

static void write_chunk(muxer_stream_t *stream, size_t len, unsigned int flags)
{
	muxer_t *muxer = (muxer_t*) stream->muxer;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	muxer_stream_priv_t *spriv = (muxer_stream_priv_t *) stream->priv;
	AVPacket pkt;
	AVCodecContext *ctx;
	
	stream->size += len;
	if(stream->type == MUXER_TYPE_VIDEO && !len)
		return;
	
	av_init_packet(&pkt);
	pkt.size = len;
	pkt.stream_index= spriv->avstream->index;
	pkt.data = stream->buffer;
	
	if((stream->type == MUXER_TYPE_VIDEO) && (flags & AVIIF_KEYFRAME))
		pkt.flags |= PKT_FLAG_KEY;
	else
		pkt.flags = 0;
	
	
	//pkt.pts = AV_NOPTS_VALUE; 
	pkt.pts = AV_TIME_BASE * stream->timer;
	
	if(stream->h.dwSampleSize) 	// CBR
		stream->h.dwLength += len / stream->h.dwSampleSize;
	else				// VBR
		stream->h.dwLength++;
	
	//;
	if(av_interleaved_write_frame(priv->oc, &pkt) != 0) //av_write_frame(priv->oc, &pkt)
	{
		mp_msg(MSGT_MUXER, MSGL_ERR, "Error while writing frame\n");
	}
	
	stream->timer = (double) stream->h.dwLength * stream->h.dwScale / stream->h.dwRate;
	return;
}


static void write_header(muxer_t *muxer)
{
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	
	av_write_header(priv->oc);
	muxer->cont_write_header = NULL;
	mp_msg(MSGT_MUXER, MSGL_INFO, "WRITTEN HEADER\n");
}


static void write_trailer(muxer_t *muxer)
{
	int i;
	muxer_priv_t *priv = (muxer_priv_t *) muxer->priv;
	
	av_write_trailer(priv->oc);
	mp_msg(MSGT_MUXER, MSGL_INFO, "WRITTEN TRAILER\n");
	for(i = 0; i < priv->oc->nb_streams; i++) 
	{
		av_freep(&(priv->oc->streams[i]));
	}

	url_fclose(&(priv->oc->pb));

	av_free(priv->oc);
}

extern char *out_filename;
int muxer_init_muxer_lavf(muxer_t *muxer)
{
	muxer_priv_t *priv;
	AVOutputFormat *fmt = NULL;
	char mp_filename[256] = "menc://stream.dummy";
	
	priv = (muxer_priv_t *) calloc(1, sizeof(muxer_priv_t));
	if(priv == NULL)
		return 0;

	av_register_all();
	
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
