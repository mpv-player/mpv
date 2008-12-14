#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#include <libnut.h>

typedef struct {
	int last_pts; // FIXME
	nut_context_tt * nut;
	nut_stream_header_tt * s;
} nut_priv_tt;

static size_t mp_read(void * h, size_t len, uint8_t * buf) {
	stream_t * stream = (stream_t*)h;

	if(stream_eof(stream)) return 0;
	//len = MIN(len, 5);

	return stream_read(stream, buf, len);
}

static int mp_eof(void * h) {
	stream_t * stream = (stream_t*)h;
	if(stream_eof(stream)) return 1;
	return 0;
}

static off_t mp_seek(void * h, long long pos, int whence) {
	stream_t * stream = (stream_t*)h;

	if (stream->end_pos < stream_tell(stream))
		stream->end_pos = stream_tell(stream);

	if (whence == SEEK_CUR) pos += stream_tell(stream);
	else if (whence == SEEK_END) pos += stream->end_pos;
	else if (whence != SEEK_SET) return -1;

	if (pos < stream->end_pos && stream->eof) stream_reset(stream);
	if (stream_seek(stream, pos) == 0) return -1;

	return pos;
}

#define ID_STRING "nut/multimedia container"
#define ID_LENGTH (strlen(ID_STRING) + 1)

static int nut_check_file(demuxer_t * demuxer) {
	uint8_t buf[ID_LENGTH];

	if (stream_read(demuxer->stream, buf, ID_LENGTH) != ID_LENGTH) return 0;

	if (memcmp(buf, ID_STRING, ID_LENGTH)) return 0;

	stream_seek(demuxer->stream, 0);
	return DEMUXER_TYPE_NUT;
}

static demuxer_t * demux_open_nut(demuxer_t * demuxer) {
	nut_demuxer_opts_tt dopts = {
		.input = {
			.priv = demuxer->stream,
			.seek = mp_seek,
			.read = mp_read,
			.eof = mp_eof,
			.file_pos = stream_tell(demuxer->stream),
		},
		.alloc = { .malloc = NULL },
		.read_index = index_mode,
		.cache_syncpoints = 1,
	};
	nut_priv_tt * priv = demuxer->priv = calloc(1, sizeof(nut_priv_tt));
	nut_context_tt * nut = priv->nut = nut_demuxer_init(&dopts);
	nut_stream_header_tt * s;
	int ret;
	int i;

	while ((ret = nut_read_headers(nut, &s, NULL)) == NUT_ERR_EAGAIN);
	if (ret) {
		mp_msg(MSGT_HEADER, MSGL_ERR, "NUT error: %s\n", nut_error(ret));
		return NULL;
	}

	priv->s = s;

	for (i = 0; s[i].type != -1 && i < 2; i++) switch(s[i].type) {
		case NUT_AUDIO_CLASS: {
			WAVEFORMATEX *wf =
				calloc(sizeof(WAVEFORMATEX) +
				              s[i].codec_specific_len, 1);
			sh_audio_t* sh_audio = new_sh_audio(demuxer, i);
			int j;
			mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_AudioID, "nut", i);

			sh_audio->wf= wf; sh_audio->ds = demuxer->audio;
			sh_audio->audio.dwSampleSize = 0; // FIXME
			sh_audio->audio.dwScale = s[i].time_base.num;
			sh_audio->audio.dwRate = s[i].time_base.den;
			sh_audio->format = 0;
			for (j = 0; j < s[i].fourcc_len && j < 4; j++)
				sh_audio->format |= s[i].fourcc[j]<<(j*8);
			sh_audio->channels = s[i].channel_count;
			sh_audio->samplerate =
				s[i].samplerate_num / s[i].samplerate_denom;
			sh_audio->i_bps = 0; // FIXME

			wf->wFormatTag = sh_audio->format;
			wf->nChannels = s[i].channel_count;
			wf->nSamplesPerSec =
				s[i].samplerate_num / s[i].samplerate_denom;
			wf->nAvgBytesPerSec = 0; // FIXME
			wf->nBlockAlign = 0; // FIXME
			wf->wBitsPerSample = 0; // FIXME
			wf->cbSize = s[i].codec_specific_len;
			if (s[i].codec_specific_len)
				memcpy(wf + 1, s[i].codec_specific,
			                       s[i].codec_specific_len);

			demuxer->audio->id = i;
			demuxer->audio->sh= demuxer->a_streams[i];
			break;
		}
		case NUT_VIDEO_CLASS: {
			BITMAPINFOHEADER * bih =
				calloc(sizeof(BITMAPINFOHEADER) +
				              s[i].codec_specific_len, 1);
			sh_video_t * sh_video = new_sh_video(demuxer, i);
			int j;
			mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_VideoID, "nut", i);

			sh_video->bih = bih;
			sh_video->ds = demuxer->video;
			sh_video->disp_w = s[i].width;
			sh_video->disp_h = s[i].height;
			sh_video->video.dwScale = s[i].time_base.num;
			sh_video->video.dwRate  = s[i].time_base.den;

			sh_video->fps = sh_video->video.dwRate/
			                  (float)sh_video->video.dwScale;
			sh_video->frametime = 1./sh_video->fps;
			sh_video->format = 0;
			for (j = 0; j < s[i].fourcc_len && j < 4; j++)
				sh_video->format |= s[i].fourcc[j]<<(j*8);
			if (!s[i].sample_height) sh_video->aspect = 0;
			else sh_video->aspect =
				s[i].sample_width / (float)s[i].sample_height;
			sh_video->i_bps = 0; // FIXME

			bih->biSize = sizeof(BITMAPINFOHEADER) +
			                     s[i].codec_specific_len;
			bih->biWidth = s[i].width;
			bih->biHeight = s[i].height;
			bih->biBitCount = 0; // FIXME
			bih->biSizeImage = 0; // FIXME
			bih->biCompression = sh_video->format;

			if (s[i].codec_specific_len)
				memcpy(bih + 1, s[i].codec_specific,
						s[i].codec_specific_len);

			demuxer->video->id = i;
			demuxer->video->sh = demuxer->v_streams[i];
			break;
		}
	}

	return demuxer;
}

static int demux_nut_fill_buffer(demuxer_t * demuxer, demux_stream_t * dsds) {
	nut_priv_tt * priv = demuxer->priv;
	nut_context_tt * nut = priv->nut;
	demux_packet_t *dp;
	demux_stream_t *ds;
	nut_packet_tt pd;
	int ret;
	double pts;

	demuxer->filepos = stream_tell(demuxer->stream);
	if (stream_eof(demuxer->stream)) return 0;

	while ((ret = nut_read_next_packet(nut, &pd)) == NUT_ERR_EAGAIN);
	if (ret) {
		if (ret != NUT_ERR_EOF)
			mp_msg(MSGT_HEADER, MSGL_ERR, "NUT error: %s\n",
			                               nut_error(ret));
		return 0; // fatal error
	}

	pts = (double)pd.pts * priv->s[pd.stream].time_base.num /
	                       priv->s[pd.stream].time_base.den;

	if (pd.stream == demuxer->audio->id)  {
		ds = demuxer->audio;
	}
	else if (pd.stream == demuxer->video->id) {
		ds = demuxer->video;
	}
	else {
		uint8_t buf[pd.len];
		while ((ret = nut_read_frame(nut, &pd.len, buf)) == NUT_ERR_EAGAIN);
		if (ret) {
			mp_msg(MSGT_HEADER, MSGL_ERR, "NUT error: %s\n",
			                               nut_error(ret));
			return 0; // fatal error
		}
		return 1;
	}

	if (pd.stream == 0) priv->last_pts = pd.pts;

	dp = new_demux_packet(pd.len);

	dp->pts = pts;

	dp->pos = demuxer->filepos;
	dp->flags= (pd.flags & NUT_FLAG_KEY) ? 0x10 : 0;

	{int len = pd.len;
	while ((ret = nut_read_frame(nut, &len, dp->buffer + pd.len-len)) == NUT_ERR_EAGAIN);
	}
	if (ret) {
		mp_msg(MSGT_HEADER, MSGL_ERR, "NUT error: %s\n",
		                               nut_error(ret));
		return 0; // fatal error
	}

	ds_add_packet(ds, dp); // append packet to DS stream
	return 1;
}

static void demux_seek_nut(demuxer_t * demuxer, float time_pos, float audio_delay, int flags) {
	nut_context_tt * nut = ((nut_priv_tt*)demuxer->priv)->nut;
	nut_priv_tt * priv = demuxer->priv;
	int nutflags = 0;
	int ret;
	const int tmp[] = { 0, -1 };

	if (!(flags & SEEK_ABSOLUTE)) {
		nutflags |= 1; // relative
		if (time_pos > 0) nutflags |= 2; // forwards
	}

	if (flags & SEEK_FACTOR)
		time_pos *= priv->s[0].max_pts *
		               (double)priv->s[0].time_base.num /
		                       priv->s[0].time_base.den;

	while ((ret = nut_seek(nut, time_pos, nutflags, tmp)) == NUT_ERR_EAGAIN);
	priv->last_pts = -1;
	if (ret) mp_msg(MSGT_HEADER, MSGL_ERR, "NUT error: %s\n", nut_error(ret));
	demuxer->filepos = stream_tell(demuxer->stream);
}

static int demux_control_nut(demuxer_t * demuxer, int cmd, void * arg) {
	nut_priv_tt * priv = demuxer->priv;
	switch (cmd) {
		case DEMUXER_CTRL_GET_TIME_LENGTH:
			*((double *)arg) = priv->s[0].max_pts *
				(double)priv->s[0].time_base.num /
				        priv->s[0].time_base.den;
			return DEMUXER_CTRL_OK;
		case DEMUXER_CTRL_GET_PERCENT_POS:
			if (priv->s[0].max_pts == 0 || priv->last_pts == -1)
				return DEMUXER_CTRL_DONTKNOW;
			*((int *)arg) = priv->last_pts * 100 /
			                (double)priv->s[0].max_pts;
			return DEMUXER_CTRL_OK;
		default:
			return DEMUXER_CTRL_NOTIMPL;
	}
}

static void demux_close_nut(demuxer_t *demuxer) {
	nut_priv_tt * priv = demuxer->priv;
	if (!priv) return;
	nut_demuxer_uninit(priv->nut);
	free(demuxer->priv);
	demuxer->priv = NULL;
}


const demuxer_desc_t demuxer_desc_nut = {
	"NUT demuxer",
	"nut",
	"libnut",
	"Oded Shimon (ods15)",
	"NUT demuxer, requires libnut",
	DEMUXER_TYPE_NUT,
	1, // safe check demuxer
	nut_check_file,
	demux_nut_fill_buffer,
	demux_open_nut,
	demux_close_nut,
	demux_seek_nut,
	demux_control_nut
};
