/*
 * demuxer for PVA files, such as the ones produced by software to manage DVB boards
 * like the Hauppauge WinTV DVBs, for MPlayer.
 *
 * Uses info from the PVA file specifications found at
 * 
 * http://www.technotrend.de/download/av_format_v1.pdf
 * 
 * Known issue:
 * - Does not seem to correctly demux files produced by MultiDec (on some channels).
 *   They seem not to be correctly formatted. In fact, other DVB software which uses
 *   the DVB board for playback plays them fine, but existing software which should
 *   convert PVAs to MPEG-PS streams fails on them as well.
 *   Feedback would be appreciated.
 *
 *
 * written by Matteo Giani
 */




#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "bswap.h"

/*
 * #defines below taken from PVA spec (see URL above)
 */

#define PVA_MAX_VIDEO_PACK_LEN 6*1024

#define VIDEOSTREAM 0x01	
#define MAINAUDIOSTREAM 0x02

typedef struct {
	unsigned long offset;
	long size;
	uint8_t type;
	uint8_t is_packet_start;
	uint64_t pts;
} pva_payload_t;


typedef struct {
	float last_audio_pts;
	float last_video_pts;
	uint8_t just_synced;
	uint8_t synced_stream_id;
} pva_priv_t;


int pva_sync(demuxer_t * demuxer)
{
	uint8_t buffer[5]={0,0,0,0,0};
	int count;
	pva_priv_t * priv = (pva_priv_t *) demuxer->priv;
	
	/* This is a hack. Since this function is called both for actual syncing and by
	 * pva_check_file to check file type, we must check whether the priv structure has
	 * already been allocated, otherwise we will dereference NULL and segfault.
	 * So, if priv is NULL (not yet allocated) use a local variable, otherwise use priv->just_synced.
	 * This field is in the priv structure so that pva_get_payload knows that pva_sync has already
	 * read (part of) the PVA header. This way we can avoid to seek back and (hopefully) be able to read
	 * from pipes and such.
	 */
	
	uint8_t local_synced=0;
	uint8_t * syncedptr;

	syncedptr=(priv==NULL)?&local_synced:&priv->just_synced;

	
	for(count=0 ; count<PVA_MAX_VIDEO_PACK_LEN && !demuxer->stream->eof && !*syncedptr ; count++)
	{
		buffer[0]=buffer[1];
		buffer[1]=buffer[2];
		buffer[2]=buffer[3];
		buffer[3]=buffer[4];
		buffer[4]=stream_read_char(demuxer->stream);
		/*
		 * Check for a PVA packet beginning sequence: we check both the "AV" word at the 
		 * very beginning and the "0x55" reserved byte (which is unused and set to 0x55 by spec)
		 */
		if(buffer[0]=='A' && buffer[1] == 'V' && buffer[4] == 0x55) *syncedptr=1;
		//printf("demux_pva: pva_sync() current offset: %d\n",stream_tell(demuxer->stream));
	}
	if(*syncedptr)
	{
		if(priv!=NULL) priv->synced_stream_id=buffer[2];
		return 1;
	}
	else
	{
		return 0;
	}
}

int pva_check_file(demuxer_t * demuxer)
{
	uint8_t buffer[5]={0,0,0,0,0};
	mp_msg(MSGT_DEMUX, MSGL_V, "Checking for PVA\n");
	stream_read(demuxer->stream,buffer,5);
	if(buffer[0]=='A' && buffer[1] == 'V' && buffer[4] == 0x55)
	{
		mp_msg(MSGT_DEMUX,MSGL_DBG2, "Success: PVA\n");
		return 1;
	}
	else
	{
		mp_msg(MSGT_DEMUX,MSGL_DBG2, "Failed: PVA\n");
		return 0;
	}
}

demuxer_t * demux_open_pva (demuxer_t * demuxer)
{
	sh_video_t *sh_video = new_sh_video(demuxer,0);
	sh_audio_t * sh_audio = new_sh_audio(demuxer,0);
	pva_priv_t * priv;
	unsigned char * buffer;

		
	
	stream_reset(demuxer->stream);
	stream_seek(demuxer->stream,0);
	
	

	priv=malloc(sizeof(pva_priv_t));
	
	if(demuxer->stream->type!=STREAMTYPE_FILE) demuxer->seekable=0;
	else demuxer->seekable=1;
	
	demuxer->priv=priv;
	memset(demuxer->priv,0,sizeof(pva_priv_t));
			
	if(!pva_sync(demuxer))
	{
		mp_msg(MSGT_DEMUX,MSGL_ERR,"Not a PVA file.\n");
		return NULL;
	}

	//printf("priv->just_synced %s after initial sync!\n",priv->just_synced?"set":"UNSET");
	
	demuxer->video->sh=sh_video;
	demuxer->audio->sh=sh_audio;
	mp_msg(MSGT_DEMUXER,MSGL_INFO,"Opened PVA demuxer...\n");
	
	/*
	 * Audio and Video codecs:
	 * the PVA spec only allows MPEG2 video and MPEG layer II audio. No need to check the formats then.
	 * Moreover, there would be no way to do that since the PVA stream format has no fields to describe
	 * the used codecs.
	 */
	
	sh_video->format=0x10000002;
	sh_video->ds=demuxer->video;
		
	sh_audio->format=0x50;
	sh_audio->ds=demuxer->audio;

	priv->last_video_pts=-1;
	priv->last_audio_pts=-1;

	return demuxer;
}

// 0 = EOF or no stream found
// 1 = successfully read a packet
int demux_pva_fill_buffer (demuxer_t * demux)
{
	uint8_t done=0;
	demux_packet_t * dp;
	pva_priv_t * priv=demux->priv;
	pva_payload_t current_payload;

	while(!done)
	{
		if(!pva_get_payload(demux,&current_payload)) return 0;
		switch(current_payload.type)
		{
			case VIDEOSTREAM:
				if(!current_payload.is_packet_start && priv->last_video_pts==-1)
				{
					/* We should only be here at the beginning of a stream, when we have
					 * not yet encountered a valid Video PTS, or after a seek.
					 * So, skip these starting packets in order not to deliver the
					 * player a bogus PTS.
					 */
					done=0;
				}
				else
				{
					/*
					 * In every other condition, we are delivering the payload. Set this
					 * so that the following code knows whether to skip it or read it.
					 */
					done=1;
				}
				if(current_payload.is_packet_start)
				{
					priv->last_video_pts=((float)current_payload.pts)/90000;
					//mp_msg(MSGT_DEMUXER,MSGL_DBG2,"demux_pva: Video PTS=%llu , delivered %f\n",current_payload.pts,priv->last_video_pts);
				}
				if(done)
				{
					dp=new_demux_packet(current_payload.size);
					dp->pts=priv->last_video_pts;
					stream_read(demux->stream,dp->buffer,current_payload.size);
					ds_add_packet(demux->video,dp);
				}
				else
				{
					//printf("Skipping %u video bytes\n",current_payload.size);
					stream_skip(demux->stream,current_payload.size);
				}
				break;
			case MAINAUDIOSTREAM:
				if(!current_payload.is_packet_start && priv->last_audio_pts==-1)
				{
					/* Same as above for invalid video PTS, just for audio. */
					done=0;
				}
				else
				{
					done=1;
				}
				if(current_payload.is_packet_start)
				{
					priv->last_audio_pts=(float)current_payload.pts/90000;
				}
				if(done)
				{
					dp=new_demux_packet(current_payload.size);
					dp->pts=priv->last_audio_pts;
					stream_read(demux->stream,dp->buffer,current_payload.size);
					ds_add_packet(demux->audio,dp);
				}
				else
				{
					stream_skip(demux->stream,current_payload.size);
				}
				break;
		}
	}
	return 1;
}

int pva_get_payload(demuxer_t * d,pva_payload_t * payload)
{
	uint8_t flags,pes_head_len;
	uint16_t pack_size;
	long long next_offset,pva_payload_start;
	unsigned char buffer[256];
	demux_packet_t * dp; 	//hack to deliver the preBytes (see PVA doc)
	pva_priv_t * priv=(pva_priv_t *) d->priv;

	
	if(d==NULL)
	{
		printf("demux_pva: pva_get_payload got passed a NULL pointer!\n");
		return 0;
	}

	
	if(d->stream->eof)
	{
		mp_msg(MSGT_DEMUX,MSGL_V,"demux_pva: pva_get_payload() detected stream->eof!!!\n");
		return 0;
	}

	//printf("priv->just_synced %s\n",priv->just_synced?"SET":"UNSET");
	if(!priv->just_synced)
	{
		if(stream_read_word(d->stream) != (('A'<<8)|'V'))
		{
			mp_msg(MSGT_DEMUX,MSGL_V,"demux_pva: pva_get_payload() missed a SyncWord at %ld!! Trying to sync...\n",stream_tell(d->stream));
			if(!pva_sync(d)) return 0;
		}
	}
	if(priv->just_synced)
	{
		payload->type=priv->synced_stream_id;
		priv->just_synced=0;
	}
	else
	{
		payload->type=stream_read_char(d->stream);
		stream_skip(d->stream,2); //counter and reserved
	}
	flags=stream_read_char(d->stream);
	payload->is_packet_start=flags & 0x10;
	pack_size=le2me_16(stream_read_word(d->stream));
	mp_msg(MSGT_DEMUX,MSGL_DBG2,"demux_pva::pva_get_payload(): pack_size=%u field read at offset %lu\n",pack_size,stream_tell(d->stream)-2);
	pva_payload_start=stream_tell(d->stream);
	next_offset=pva_payload_start+pack_size;
	if(!payload->is_packet_start)
	{
		payload->offset=stream_tell(d->stream);
		payload->size=pack_size;
	}
	else
	{	//here comes the good part...
		switch(payload->type)
		{
			case VIDEOSTREAM:
				payload->pts=le2me_32(stream_read_dword(d->stream));
				//printf("Video PTS: %llu\n",payload->pts);
				if((flags&0x03) && ((pva_priv_t *)d->priv)->last_video_pts!=-1)
				{
					dp=new_demux_packet(flags&0x03);
					stream_read(d->stream,dp->buffer,flags & 0x03); //read PreBytes
					dp->pts=((pva_priv_t *)d->priv)->last_video_pts;
					ds_add_packet(d->video,dp);
				}
				else
				{
					stream_skip(d->stream,flags&0x03);
				}

				//now we are at real beginning of payload.
				payload->offset=stream_tell(d->stream);
				//size is pack_size minus PTS size minus padding size.
				payload->size=pack_size - 4 - (flags & 0x03);
				break;
			case MAINAUDIOSTREAM:
				stream_skip(d->stream,8); //FIXME properly parse PES header.
				//assuming byte 8 is PES residual header length.
				pes_head_len=stream_read_char(d->stream);
				stream_read(d->stream,buffer,pes_head_len);
				//we should now be on start of real payload.
				//let's now parse the PTS...
				if((buffer[0] & 0xf0)!=0x20)
				{
					mp_msg(MSGT_DEMUX,MSGL_ERR,"demux_pva: expected audio PTS but badly formatted... (read 0x%02X)\n",buffer[0]);
					return;
				}
				payload->pts=0LL;
				payload->pts|=((uint64_t)(buffer[0] & 0x0e) << 29);
				payload->pts|=buffer[1]<<22;
				payload->pts|=(buffer[2] & 0xfe) << 14;
				payload->pts|=buffer[3]<<7;
				payload->pts|=(buffer[4] & 0xfe) >> 1;
				/*
				 * PTS parsing is hopefully finished.
				 * Let's now fill in offset and size.
				 */
				payload->pts=le2me_64(payload->pts);
				payload->offset=stream_tell(d->stream);
				payload->size=pack_size-stream_tell(d->stream)+pva_payload_start;
				break;
		}
	}
}

int demux_seek_pva(demuxer_t * demuxer,float rel_seek_secs,int flags)
{
	int total_bitrate=0;
	long dest_offset;
	pva_priv_t * priv=demuxer->priv;
	
	total_bitrate=((sh_audio_t *)demuxer->audio->sh)->i_bps + ((sh_video_t *)demuxer->video->sh)->i_bps;

	/*
	 * Compute absolute offset inside the stream. Approximate total bitrate with sum of bitrates
	 * reported by the audio and video codecs. The seek is not accurate because, just like
	 * with MPEG streams, the bitrate is not constant. Moreover, we do not take into account
	 * the overhead caused by PVA and PES headers.
	 * If the calculated absolute offset is negative, seek to the beginning of the file.
	 */
	
	dest_offset=stream_tell(demuxer->stream)+rel_seek_secs*total_bitrate;
	if(dest_offset<0) dest_offset=0;
	
	stream_seek(demuxer->stream,dest_offset);

	if(!pva_sync(demuxer))
	{
		mp_msg(MSGT_DEMUX,MSGL_V,"demux_pva: Couldn't seek!\n");
		return 0;
	}
	
	/*
	 * Reset the PTS info inside the pva_priv_t structure. This way we don't deliver
	 * data with the wrong PTSs (the ones we had before seeking).
	 *
	 */
	
	priv->last_video_pts=-1;
	priv->last_audio_pts=-1;
	
	return 1;
}



void demux_close_pva(demuxer_t * demuxer)
{
	if(demuxer->priv)
	{
		free(demuxer->priv);
		demuxer->priv=NULL;
	}
}
			
