/*
 * This is FLAC decoder for MPlayer using stream_decoder from libFLAC
 * (directly or from libmpflac).
 * This file is part of MPlayer, see http://mplayerhq.hu/ for info.  
 * Copyright (C) 2003  Dmitry Baryshkov <mitya at school.ioffe.ru>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * parse_double_, grabbag__replaygain_load_from_vorbiscomment, grabbag__replaygain_compute_scale_factor
 * functions are imported from FLAC project (from grabbag lib sources (replaygain.c)) and are
 * Copyright (C) 2002,2003  Josh Coalson under the terms of GPL.
 */

/*
 * TODO:
 * in demux_audio use data from seektable block for seeking.
 * support FLAC-in-Ogg.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "config.h"
#ifdef HAVE_FLAC
#include "ad_internal.h"
#include "mp_msg.h"

static ad_info_t info =  {
	"FLAC audio decoder",  // name of the driver
	"flac",    // driver name. should be the same as filename without ad_
	"Dmitry Baryshkov",     // writer/maintainer of _this_ file
	"http://flac.sf.net/",          // writer/maintainer/site of the _codec_
	""           // comments
};

LIBAD_EXTERN(flac)

#ifdef USE_MPFLAC_DECODER
#include "FLAC_stream_decoder.h"
#include "FLAC_assert.h"
#include "FLAC_metadata.h"
#else
#include "FLAC/stream_decoder.h"
#include "FLAC/assert.h"
#include "FLAC/metadata.h"
#endif

/* dithering & replaygain always from libmpflac */
#include "dither.h"
#include "replaygain_synthesis.h"

/* Some global constants. Thay have to be configurable, so leaved them as globals. */
static const FLAC__bool album_mode = true;
static const int preamp = 0;
static const FLAC__bool hard_limit = false;
static const int noise_shaping = 1;
static const FLAC__bool dither = true;
typedef struct flac_struct_st
{
	FLAC__StreamDecoder *flac_dec; /*decoder handle*/
	sh_audio_t *sh; /* link back to corresponding sh */
	
	/* set this fields before calling FLAC__stream_decoder_process_single */
	unsigned char *buf; 
	int minlen;
	int maxlen;
	/* Here goes number written at write_callback */
	int written;

	/* replaygain and dithering via plugin_common */
	FLAC__bool has_replaygain;
	double replay_scale;
	DitherContext dither_context;
	int bits_per_sample;
} flac_struct_t;

FLAC__StreamDecoderReadStatus flac_read_callback (const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], unsigned *bytes, void *client_data)
{
  /* Don't be greedy. Try to read as few packets as possible. *bytes is often
     > 60kb big which is more than one second of data. Reading it all at
     once sucks in all packets available making d_audio->pts jump to the
     pts of the last packet read which is not what we want. We're decoging
     only one FLAC block anyway, so let's just read as few bytes as
     neccessary. */
	int b = demux_read_data(((flac_struct_t*)client_data)->sh->ds, buffer, *bytes > 500 ? 500 : *bytes);
	mp_msg(MSGT_DECAUDIO, MSGL_DBG2, "\nFLAC READ CB read %d bytes\n", b);
	*bytes = b;
	if (b <= 0)
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

/*FIXME: we need to support format conversion:(flac specs allow bits/sample to be from 4 to 32. Not only 8 and 16 !!!)*/
FLAC__StreamDecoderWriteStatus flac_write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	FLAC__byte *buf = ((flac_struct_t*)(client_data))->buf;
	int bps = ((flac_struct_t*)(client_data))->sh->samplesize;
	int lowendian = (((flac_struct_t*)(client_data))->sh->sample_format == AFMT_S16_LE);
	int unsigned_data = (((flac_struct_t*)(client_data))->sh->sample_format == AFMT_U8);
	mp_msg(MSGT_DECAUDIO, MSGL_DBG2, "\nWrite callback (%d bytes)!!!!\n", bps*frame->header.blocksize*frame->header.channels);
	if (buf == NULL)
	{
		/* This is used in control for skipping 1 audio frame */
		return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
	}
	FLAC__replaygain_synthesis__apply_gain(
				buf,
				lowendian,
				unsigned_data,
				buffer,
				frame->header.blocksize,
				frame->header.channels,
				((flac_struct_t*)(client_data))->bits_per_sample,
				((flac_struct_t*)(client_data))->sh->samplesize * 8,
				((flac_struct_t*)(client_data))->replay_scale,
				hard_limit,
				dither,
				&(((flac_struct_t*)(client_data))->dither_context)
		);
	((flac_struct_t*)(client_data))->written += bps*frame->header.blocksize*frame->header.channels;
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

#ifdef local_min
#undef local_min
#endif
#define local_min(a,b) ((a)<(b)?(a):(b))

static FLAC__bool parse_double_(const FLAC__StreamMetadata_VorbisComment_Entry *entry, double *val)
{
	char s[32], *end;
	const char *p, *q;
	double v;

	FLAC__ASSERT(0 != entry);
	FLAC__ASSERT(0 != val);

	p = (const char *)entry->entry;
	q = strchr(p, '=');
	if(0 == q)
		return false;
	q++;
	memset(s, 0, sizeof(s)-1);
	strncpy(s, q, local_min(sizeof(s)-1, entry->length - (q-p)));

	v = strtod(s, &end);
	if(end == s)
		return false;

	*val = v;
	return true;
}

FLAC__bool grabbag__replaygain_load_from_vorbiscomment(const FLAC__StreamMetadata *block, FLAC__bool album_mode, double *gain, double *peak)
{
	int gain_offset, peak_offset;
static const FLAC__byte *tag_title_gain_ = "REPLAYGAIN_TRACK_GAIN";
static const FLAC__byte *tag_title_peak_ = "REPLAYGAIN_TRACK_PEAK";
static const FLAC__byte *tag_album_gain_ = "REPLAYGAIN_ALBUM_GAIN";
static const FLAC__byte *tag_album_peak_ = "REPLAYGAIN_ALBUM_PEAK";

	FLAC__ASSERT(0 != block);
	FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT);

	if(0 > (gain_offset = FLAC__metadata_object_vorbiscomment_find_entry_from(block, /*offset=*/0, (const char *)(album_mode? tag_album_gain_ : tag_title_gain_))))
		return false;
	if(0 > (peak_offset = FLAC__metadata_object_vorbiscomment_find_entry_from(block, /*offset=*/0, (const char *)(album_mode? tag_album_peak_ : tag_title_peak_))))
		return false;

	if(!parse_double_(block->data.vorbis_comment.comments + gain_offset, gain))
		return false;
	if(!parse_double_(block->data.vorbis_comment.comments + peak_offset, peak))
		return false;

	return true;
}

double grabbag__replaygain_compute_scale_factor(double peak, double gain, double preamp, FLAC__bool prevent_clipping)
{
	double scale;
	FLAC__ASSERT(peak >= 0.0);
 	gain += preamp;
	scale = (float) pow(10.0, gain * 0.05);
	if(prevent_clipping && peak > 0.0) {
		const double max_scale = (float)(1.0 / peak);
		if(scale > max_scale)
			scale = max_scale;
	}
	return scale;
}

void flac_metadata_callback (const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	int i, j;
	sh_audio_t *sh = ((flac_struct_t*)client_data)->sh;
	mp_msg(MSGT_DECAUDIO, MSGL_DBG2, "Metadata received\n");
	switch (metadata->type)
	{
		case FLAC__METADATA_TYPE_STREAMINFO:
			mp_msg(MSGT_DECAUDIO, MSGL_V, "STREAMINFO block (%u bytes):\n", metadata->length);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "min_blocksize: %u samples\n", metadata->data.stream_info.min_blocksize);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "max_blocksize: %u samples\n", metadata->data.stream_info.max_blocksize);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "min_framesize: %u bytes\n", metadata->data.stream_info.min_framesize);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "max_framesize: %u bytes\n", metadata->data.stream_info.max_framesize);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "sample_rate: %u Hz\n", metadata->data.stream_info.sample_rate);
			sh->samplerate = metadata->data.stream_info.sample_rate;
			mp_msg(MSGT_DECAUDIO, MSGL_V, "channels: %u\n", metadata->data.stream_info.channels);
			sh->channels = metadata->data.stream_info.channels;
			mp_msg(MSGT_DECAUDIO, MSGL_V, "bits_per_sample: %u\n", metadata->data.stream_info.bits_per_sample);
			((flac_struct_t*)client_data)->bits_per_sample = metadata->data.stream_info.bits_per_sample;
			sh->samplesize = (metadata->data.stream_info.bits_per_sample<=8)?1:2;
			/* FIXME: need to support dithering to samplesize 4 */
			sh->sample_format=(sh->samplesize==1)?AFMT_U8:
#ifdef WORDS_BIGENDIAN
				AFMT_S16_BE;
#else
				AFMT_S16_LE;
#endif
			sh->o_bps = sh->samplesize * metadata->data.stream_info.channels * metadata->data.stream_info.sample_rate;
			sh->i_bps = metadata->data.stream_info.bits_per_sample * metadata->data.stream_info.channels * metadata->data.stream_info.sample_rate / 8 / 2;
			// input data rate (compressed bytes per second)
			// Compression rate is near 0.5 
			mp_msg(MSGT_DECAUDIO, MSGL_V, "total_samples: %llu\n", metadata->data.stream_info.total_samples);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "md5sum: ");
			for (i = 0; i < 16; i++)
				mp_msg(MSGT_DECAUDIO, MSGL_V, "%02hhx", metadata->data.stream_info.md5sum[i]);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "\n");
			
			break;
		case FLAC__METADATA_TYPE_PADDING:
			mp_msg(MSGT_DECAUDIO, MSGL_V, "PADDING block (%u bytes)\n", metadata->length);
			break;
		case FLAC__METADATA_TYPE_APPLICATION:
			mp_msg(MSGT_DECAUDIO, MSGL_V, "APPLICATION block (%u bytes):\n", metadata->length);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "Application id: 0x");
			for (i = 0; i < 4; i++)
				mp_msg(MSGT_DECAUDIO, MSGL_V, "%02hhx", metadata->data.application.id[i]);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "\nData: \n");
			for (i = 0; i < (metadata->length-4)/8; i++)
			{
				for(j = 0; j < 8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, "%c", (unsigned char)metadata->data.application.data[i*8+j]<0x20?'.':metadata->data.application.data[i*8+j]);
				mp_msg(MSGT_DECAUDIO, MSGL_V, "  |  ");
				for(j = 0; j < 8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, "%#02hhx ", metadata->data.application.data[i*8+j]);
				mp_msg(MSGT_DECAUDIO, MSGL_V, "\n");
			}
			if (metadata->length-4-i*8 != 0)
			{
				for(j = 0; j < metadata->length-4-i*8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, "%c", (unsigned char)metadata->data.application.data[i*8+j]<0x20?'.':metadata->data.application.data[i*8+j]);
				for(; j <8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, " ");
				mp_msg(MSGT_DECAUDIO, MSGL_V, "  |  ");
				for(j = 0; j < metadata->length-4-i*8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, "%#02hhx ", metadata->data.application.data[i*8+j]);
				mp_msg(MSGT_DECAUDIO, MSGL_V, "\n");
			}
			break;
		case FLAC__METADATA_TYPE_SEEKTABLE:
			mp_msg(MSGT_DECAUDIO, MSGL_V, "SEEKTABLE block (%u bytes):\n", metadata->length);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "%d seekpoints:\n", metadata->data.seek_table.num_points);
			for (i = 0; i < metadata->data.seek_table.num_points; i++)
				if (metadata->data.seek_table.points[i].sample_number != FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER)
					mp_msg(MSGT_DECAUDIO, MSGL_V, "  %3d) sample_number=%llu stream_offset=%llu frame_samples=%u\n", i,
						metadata->data.seek_table.points[i].sample_number,
						metadata->data.seek_table.points[i].stream_offset,
						metadata->data.seek_table.points[i].frame_samples);
				else
					mp_msg(MSGT_DECAUDIO, MSGL_V, "  %3d) PLACEHOLDER\n", i);
			break;
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			mp_msg(MSGT_DECAUDIO, MSGL_V, "VORBISCOMMENT block (%u bytes):\n", metadata->length);
			{
				char entry[metadata->data.vorbis_comment.vendor_string.length+1];
				memcpy(&entry, metadata->data.vorbis_comment.vendor_string.entry, metadata->data.vorbis_comment.vendor_string.length);
				entry[metadata->data.vorbis_comment.vendor_string.length] = '\0';
				mp_msg(MSGT_DECAUDIO, MSGL_V, "vendor_string: %s\n", entry);
			}
			mp_msg(MSGT_DECAUDIO, MSGL_V, "%d comment(s):\n",  metadata->data.vorbis_comment.num_comments);
			for (i = 0; i < metadata->data.vorbis_comment.num_comments; i++)
			{
				char entry[metadata->data.vorbis_comment.comments[i].length];
				memcpy(&entry, metadata->data.vorbis_comment.comments[i].entry, metadata->data.vorbis_comment.comments[i].length);
				entry[metadata->data.vorbis_comment.comments[i].length] = '\0';
				mp_msg(MSGT_DECAUDIO, MSGL_V, "%s\n", entry);
			}
			{
				double gain, peak;
				if(grabbag__replaygain_load_from_vorbiscomment(metadata, album_mode, &gain, &peak))
				{
					((flac_struct_t*)client_data)->has_replaygain = true;
					((flac_struct_t*)client_data)->replay_scale = grabbag__replaygain_compute_scale_factor(peak, gain, (double)preamp, /*prevent_clipping=*/!hard_limit);
					mp_msg(MSGT_DECAUDIO, MSGL_V, "calculated replay_scale: %lf\n", ((flac_struct_t*)client_data)->replay_scale);
				}
			}
			break;
		case FLAC__METADATA_TYPE_CUESHEET:
			mp_msg(MSGT_DECAUDIO, MSGL_V, "CUESHEET block (%u bytes):\n", metadata->length);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "mcn: '%s'\n", metadata->data.cue_sheet.media_catalog_number);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "lead_in: %llu\n", metadata->data.cue_sheet.lead_in);
			mp_msg(MSGT_DECAUDIO, MSGL_V, "is_cd: %s\n", metadata->data.cue_sheet.is_cd?"true":"false");
			mp_msg(MSGT_DECAUDIO, MSGL_V, "num_tracks: %u\n", metadata->data.cue_sheet.num_tracks);
			for (i = 0; i < metadata->data.cue_sheet.num_tracks; i++)
			{
				mp_msg(MSGT_DECAUDIO, MSGL_V, "track[%d]:\n", i);
				mp_msg(MSGT_DECAUDIO, MSGL_V, "offset: %llu\n", metadata->data.cue_sheet.tracks[i].offset);
				mp_msg(MSGT_DECAUDIO, MSGL_V, "number: %hhu%s\n", metadata->data.cue_sheet.tracks[i].number, metadata->data.cue_sheet.tracks[i].number==170?"(LEAD-OUT)":"");
				mp_msg(MSGT_DECAUDIO, MSGL_V, "isrc: '%s'\n", metadata->data.cue_sheet.tracks[i].isrc);
				mp_msg(MSGT_DECAUDIO, MSGL_V, "type: %s\n", metadata->data.cue_sheet.tracks[i].type?"non-audio":"audio");
				mp_msg(MSGT_DECAUDIO, MSGL_V, "pre_emphasis: %s\n", metadata->data.cue_sheet.tracks[i].pre_emphasis?"true":"false");
				mp_msg(MSGT_DECAUDIO, MSGL_V, "num_indices: %hhu\n", metadata->data.cue_sheet.tracks[i].num_indices);
				for (j = 0; j < metadata->data.cue_sheet.tracks[i].num_indices; j++)
				{
					mp_msg(MSGT_DECAUDIO, MSGL_V, "index[%d]:\n", j);
					mp_msg(MSGT_DECAUDIO, MSGL_V, "offset:%llu\n", metadata->data.cue_sheet.tracks[i].indices[j].offset);
					mp_msg(MSGT_DECAUDIO, MSGL_V, "number:%hhu\n", metadata->data.cue_sheet.tracks[i].indices[j].number);
				}
			}
			break;
		default: if (metadata->type >= FLAC__METADATA_TYPE_UNDEFINED)
			mp_msg(MSGT_DECAUDIO, MSGL_V, "UNKNOWN block (%u bytes):\n", metadata->length);
			else
			mp_msg(MSGT_DECAUDIO, MSGL_V, "Strange block: UNKNOWN #%d < FLAC__METADATA_TYPE_UNDEFINED (%u bytes):\n", metadata->type, metadata->length);
			for (i = 0; i < (metadata->length)/8; i++)
			{
				for(j = 0; j < 8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, "%c", (unsigned char)metadata->data.unknown.data[i*8+j]<0x20?'.':metadata->data.unknown.data[i*8+j]);
				mp_msg(MSGT_DECAUDIO, MSGL_V, "  |  ");
				for(j = 0; j < 8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, "%#02hhx ", metadata->data.unknown.data[i*8+j]);
				mp_msg(MSGT_DECAUDIO, MSGL_V, "\n");
			}
			if (metadata->length-i*8 != 0)
			{
				for(j = 0; j < metadata->length-i*8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, "%c", (unsigned char)metadata->data.unknown.data[i*8+j]<0x20?'.':metadata->data.unknown.data[i*8+j]);
				for(; j <8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, " ");
				mp_msg(MSGT_DECAUDIO, MSGL_V, "  |  ");
				for(j = 0; j < metadata->length-i*8; j++)
					mp_msg(MSGT_DECAUDIO, MSGL_V, "%#02hhx ", metadata->data.unknown.data[i*8+j]);
				mp_msg(MSGT_DECAUDIO, MSGL_V, "\n");
			}
			break;
	}
}

void flac_error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	if (status != FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC)
		mp_msg(MSGT_DECAUDIO, MSGL_ERR, "\nError callback called (%s)!!!\n", FLAC__StreamDecoderErrorStatusString[status]);
}

static int preinit(sh_audio_t *sh){
  // there are default values set for buffering, but you can override them:
  
  sh->audio_out_minsize=8*4*65535; // due to specs: we assume max 8 channels,
                                  // 4 bytes/sample and 65535 samples/frame
				  // So allocating 2Mbytes buffer :)
  
  // minimum input buffer size (set only if you need input buffering)
  // (should be the max compressed frame size)
  sh->audio_in_minsize=2048; // Default: 0 (no input buffer)
  
  // if you set audio_in_minsize non-zero, the buffer will be allocated
  // before the init() call by the core, and you can access it via
  // pointer: sh->audio_in_buffer
  // it will free'd after uninit(), so you don't have to use malloc/free here!

  return 1; // return values: 1=OK 0=ERROR
}

static int init(sh_audio_t *sh_audio){
	flac_struct_t *context = (flac_struct_t*)calloc(sizeof(flac_struct_t), 1);
  
	sh_audio->context = context;
	context->sh = sh_audio;
	if (context == NULL)
	{
		mp_msg(MSGT_DECAUDIO, MSGL_FATAL, "flac_init: error allocating context.\n");
		return 0;
	}

	context->flac_dec = FLAC__stream_decoder_new();
	if (context->flac_dec == NULL)
	{
		mp_msg(MSGT_DECAUDIO, MSGL_ERR, "flac_init: error allocaing FLAC decoder.\n");
		return 0;
	}
  
	if (!FLAC__stream_decoder_set_client_data(context->flac_dec, context))
	{
		mp_msg(MSGT_DECAUDIO, MSGL_ERR, "error setting private data for callbacks.\n");
		return 0;
	}

	if (!FLAC__stream_decoder_set_read_callback(context->flac_dec, &flac_read_callback))
	{
		mp_msg(MSGT_DECAUDIO, MSGL_ERR, "error setting read callback.\n");
		return 0;
	}

	if (!FLAC__stream_decoder_set_write_callback(context->flac_dec, &flac_write_callback))
	{
		mp_msg(MSGT_DECAUDIO, MSGL_ERR, "error setting write callback.\n");
		return 0;
	}

	if (!FLAC__stream_decoder_set_metadata_callback(context->flac_dec, &flac_metadata_callback))
	{
		mp_msg(MSGT_DECAUDIO, MSGL_ERR, "error setting metadata callback.\n");
		return 0;
	}

	if (!FLAC__stream_decoder_set_error_callback(context->flac_dec, &flac_error_callback))
	{
		mp_msg(MSGT_DECAUDIO, MSGL_ERR, "error setting error callback.\n");
		return 0;
	}

	if (!FLAC__stream_decoder_set_metadata_respond_all(context->flac_dec))
	{
		mp_msg(MSGT_DECAUDIO, MSGL_ERR, "error during setting metadata_respond_all.\n");
		return 0;
	}

	if (FLAC__stream_decoder_init(context->flac_dec) != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA)
	{
		mp_msg(MSGT_DECAUDIO, MSGL_ERR, "Error initializing decoder!\n");
		return 0;
	}

	context->buf = NULL;
	context->minlen = context->maxlen = 0;
	context->replay_scale = 1.0;

	FLAC__stream_decoder_process_until_end_of_metadata(context->flac_dec);

	FLAC__replaygain_synthesis__init_dither_context(&(context->dither_context), sh_audio->samplesize * 8, noise_shaping);
	
	return 1; // return values: 1=OK 0=ERROR
}

static void uninit(sh_audio_t *sh){
  // uninit the decoder etc...
  FLAC__stream_decoder_finish(((flac_struct_t*)(sh->context))->flac_dec);
  FLAC__stream_decoder_delete(((flac_struct_t*)(sh->context))->flac_dec);
  // again: you don't have to free() a_in_buffer here! it's done by the core.
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen){
	FLAC__StreamDecoderState decstate;
	FLAC__bool status;

  // audio decoding. the most important thing :)
  // parameters you get:
  //  buf = pointer to the output buffer, you have to store uncompressed 
  //        samples there
  //  minlen = requested minimum size (in bytes!) of output. it's just a
  //        _recommendation_, you can decode more or less, it just tell you that
  //        the caller process needs 'minlen' bytes. if it gets less, it will
  //        call decode_audio() again.
  //  maxlen = maximum size (bytes) of output. you MUST NOT write more to the
  //        buffer, it's the upper-most limit!
  //        note: maxlen will be always greater or equal to sh->audio_out_minsize

// Store params in private context for callback:
	((flac_struct_t*)(sh_audio->context))->buf = buf;
	((flac_struct_t*)(sh_audio->context))->minlen = minlen;
	((flac_struct_t*)(sh_audio->context))->maxlen = maxlen;
	((flac_struct_t*)(sh_audio->context))->written = 0;

	status = FLAC__stream_decoder_process_single(((flac_struct_t*)(sh_audio->context))->flac_dec);
	decstate = FLAC__stream_decoder_get_state(((flac_struct_t*)(sh_audio->context))->flac_dec);
	if (!status || (
		decstate != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA &&
		decstate != FLAC__STREAM_DECODER_READ_METADATA &&
		decstate != FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC &&
		decstate != FLAC__STREAM_DECODER_READ_FRAME
		))
	{
		if (decstate == FLAC__STREAM_DECODER_END_OF_STREAM)
		{
			/* return what we have decoded */
			if (((flac_struct_t*)(sh_audio->context))->written != 0)
				return ((flac_struct_t*)(sh_audio->context))->written;
			mp_msg(MSGT_DECAUDIO, MSGL_V, "End of stream.\n");
			return -1;
		}
		mp_msg(MSGT_DECAUDIO, MSGL_WARN, "process_single problem: returned %s, state is %s!\n", status?"true":"false", FLAC__StreamDecoderStateString[decstate]);
		FLAC__stream_decoder_flush(((flac_struct_t*)(sh_audio->context))->flac_dec);
		return -1;
	}


  return ((flac_struct_t*)(sh_audio->context))->written; // return value: number of _bytes_ written to output buffer,
              // or -1 for EOF (or uncorrectable error)
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...){
    switch(cmd){
      case ADCTRL_RESYNC_STREAM:
        // it is called once after seeking, to resync.
	// Note: sh_audio->a_in_buffer_len=0; is done _before_ this call!
	FLAC__stream_decoder_flush (((flac_struct_t*)(sh->context))->flac_dec);
	return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
        // it is called to skip (jump over) small amount (1/10 sec or 1 frame)
	// of audio data - used to sync audio to video after seeking
	// if you don't return CONTROL_TRUE, it will defaults to:
	//      ds_fill_buffer(sh_audio->ds);  // skip 1 demux packet
	((flac_struct_t*)(sh->context))->buf = NULL;
	((flac_struct_t*)(sh->context))->minlen =
	((flac_struct_t*)(sh->context))->maxlen = 0;
	FLAC__stream_decoder_process_single(((flac_struct_t*)(sh->context))->flac_dec);
	return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}
#endif
