
#include "config.h"


#include <stdlib.h>
#include <stdio.h>

#include "../mp_msg.h"
#include "../help_mp.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#ifndef HAVE_OGGVORBIS
/// Some dummy function to use when no Ogg and Vorbis lib are avaible
int demux_ogg_open(demuxer_t* demuxer) {
  return 0;
}

int demux_ogg_fill_buffer(demuxer_t *d) {
  return 0;
}

demuxer_t* init_avi_with_ogg(demuxer_t* demuxer) {
  mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_NoOggVorbis);
  // disable audio
  demuxer->audio->id = -2;
  return demuxer;
}
#else

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#define BLOCK_SIZE 1024

/// Vorbis decoder context : we need the vorbis_info for vorbis timestamping
/// Shall we put this struct def in a common header ?
typedef struct ov_struct_st {
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			  settings */
  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */
} ov_struct_t;

//// OggDS headers
// Header for the new header format
typedef struct stream_header_video
{
	ogg_int32_t	width;
	ogg_int32_t	height;
} stream_header_video;
	
typedef struct stream_header_audio
{
	ogg_int16_t	channels;
	ogg_int16_t	blockalign;
	ogg_int32_t	avgbytespersec;
} stream_header_audio;

typedef struct stream_header
{
	char	streamtype[8];
	char	subtype[4];

	ogg_int32_t	size;				// size of the structure

	ogg_int64_t	time_unit;			// in reference time
	ogg_int64_t	samples_per_unit;
	ogg_int32_t default_len;		// in media time

	ogg_int32_t buffersize;
	ogg_int16_t	bits_per_sample;

	union
	{
		// Video specific
		stream_header_video	video;
		// Audio specific
		stream_header_audio	audio;
	} sh;
} stream_header;

/// Our private datas

/// A logical stream
typedef struct ogg_stream {
  /// Timestamping stuff
  float samplerate; /// granulpos 2 time
  int64_t lastpos;
  int32_t lastsize;

  // Logical stream state
  ogg_stream_state stream;
} ogg_stream_t;

typedef struct ogg_demuxer {
  /// Physical stream state
  ogg_sync_state sync;
  /// Current page
  ogg_page page;
  /// Logical streams
  ogg_stream_t *subs;
  int num_sub;
} ogg_demuxer_t;

/// Some defines from OggDS
#define PACKET_TYPE_HEADER   0x01
#define PACKET_TYPE_BITS         0x07
#define PACKET_LEN_BITS01       0xc0
#define PACKET_LEN_BITS2         0x02
#define PACKET_IS_SYNCPOINT  0x08


// get the logical stream of the current page
// fill os if non NULL and return the stream id
static  int demux_ogg_get_page_stream(ogg_demuxer_t* ogg_d,ogg_stream_state** os) {
  int id,s_no;
  ogg_page* page = &ogg_d->page;

  s_no = ogg_page_serialno(page);

  for(id= 0; id < ogg_d->num_sub ; id++) {
    if(s_no == ogg_d->subs[id].stream.serialno)
      break;
  }

  if(id == ogg_d->num_sub)
    return -1;
  
  if(os)
    *os = &ogg_d->subs[id].stream;

  return id;

}

/// Calculate the timestamp and add the packet to the demux stream
// return 1 if the packet was added, 0 otherwise
static int demux_ogg_add_packet(demux_stream_t* ds,ogg_stream_t* os,ogg_packet* pack) {
  demuxer_t* d = ds->demuxer;
  demux_packet_t* dp;
  unsigned char* data;
  float pts = 0;
  int flags = 0;

  // If packet is an header we jump it except for vorbis
  if((*pack->packet & PACKET_TYPE_HEADER) && 
     (ds == d->video || (ds == d->audio && ((sh_audio_t*)ds->sh)->format != 0xFFFE )))
    return 0;

  // For vorbis packet the packet is the data, for other codec we must jump the header
  if(ds == d->audio && ((sh_audio_t*)ds->sh)->format == 0xFFFE) {
    data = pack->packet;
    if(*pack->packet & PACKET_TYPE_HEADER)
      pts = 0;
    else {
      vorbis_info* vi = &((ov_struct_t*)((sh_audio_t*)ds->sh)->context)->vi;
      // When we dump the audio, there is no vi, but we dont care of timestamp in this case
      if(vi) {
	int32_t blocksize = vorbis_packet_blocksize(vi,pack) / vi->channels;
	// Calculate the timestamp if the packet don't have any
	if(pack->granulepos == -1) {
	  pack->granulepos = os->lastpos;
	  if(os->lastsize > 0)
	    pack->granulepos += os->lastsize;
	}
	pts = pack->granulepos / (float)vi->rate;
	os->lastsize = blocksize;
	os->lastpos = pack->granulepos;
      }
    }
  } else {
    // Find data start
    int16_t hdrlen = (*pack->packet & PACKET_LEN_BITS01)>>6;
    hdrlen |= (*pack->packet & PACKET_LEN_BITS2) <<1;
    data = pack->packet + 1 + hdrlen;
    // Calculate the timestamp
    if(pack->granulepos == -1)
      pack->granulepos = os->lastpos + os->lastsize;
    // If we alredy have a timestamp it can be a syncpoint
    else if(*pack->packet & PACKET_IS_SYNCPOINT)
      flags = 1;
    pts =  pack->granulepos/os->samplerate;
    // Save the packet length and timestamp
    os->lastsize = 0;
    while(hdrlen) {
      os->lastsize <<= 8;
      os->lastsize |= pack->packet[hdrlen];
      hdrlen--;
    }
    os->lastpos = pack->granulepos;
  }

  /// Send the packet
  dp = new_demux_packet(pack->bytes-(data-pack->packet));
  memcpy(dp->buffer,data,pack->bytes-(data-pack->packet));
  dp->pts = pts;
  dp->flags = flags;
  ds_add_packet(ds,dp);
  if(verbose>1)printf("New dp: %p  ds=%p  pts=%5.3f  len=%d  flag=%d  \n",
      dp, ds, pts, dp->len, flags);
  return 1;
}

/// Open an ogg physical stream
int demux_ogg_open(demuxer_t* demuxer) {
  ogg_demuxer_t* ogg_d;
  stream_t *s;
  char* buf;
  int np,s_no, n_audio = 0, n_video = 0;
  ogg_sync_state* sync;
  ogg_page* page;
  ogg_packet pack;
  sh_audio_t* sh_a;
  sh_video_t* sh_v;

  s = demuxer->stream;

  ogg_d = (ogg_demuxer_t*)calloc(1,sizeof(ogg_demuxer_t));
  sync = &ogg_d->sync;
  page = &ogg_d->page;

  ogg_sync_init(&ogg_d->sync);

  while(1) {
    /// Try to get a page
    np = ogg_sync_pageout(sync,page);
    /// Error
    if(np < 0) {
      mp_msg(MSGT_DEMUX,MSGL_DBG2,"OGG demuxer : Bad page sync\n");
      return 0;
    }
    /// Need some more data
    if(np <= 0) {
      int len;
      buf = ogg_sync_buffer(sync,BLOCK_SIZE);
      len = stream_read(s,buf,BLOCK_SIZE);      
      if(len == 0 && s->eof) {
	free(ogg_d);
	return 0;
      }
      ogg_sync_wrote(sync,len);
      continue;
    }

    // We got one page now

    if( ! ogg_page_bos(page) ) { // It's not a begining page
      // Header parsing end here, we need to get the page otherwise it will be lost
      int id = demux_ogg_get_page_stream(ogg_d,NULL);
      if(id >= 0)
	ogg_stream_pagein(&ogg_d->subs[id].stream,page);
      else
	mp_msg(MSGT_DEMUX,MSGL_ERR,"OGG : Warning found none bos page from unknow stream %d\n",ogg_page_serialno(page));
      break;
    }

    /// Init  the data structure needed for a logical stream
    ogg_d->subs = (ogg_stream_t*)realloc(ogg_d->subs,(ogg_d->num_sub+1)*sizeof(ogg_stream_t));
    memset(&ogg_d->subs[ogg_d->num_sub],0,sizeof(ogg_stream_t));
    /// Get the stream serial number
    s_no = ogg_page_serialno(page);
    ogg_stream_init(&ogg_d->subs[ogg_d->num_sub].stream,s_no);
    mp_msg(MSGT_DEMUX,MSGL_DBG2,"OGG : Found a stream with serial=%d\n",s_no);
    // Take the first page
    ogg_stream_pagein(&ogg_d->subs[ogg_d->num_sub].stream,page);
    // Get first packet of the page
    ogg_stream_packetout(&ogg_d->subs[ogg_d->num_sub].stream,&pack);

    // Reset our vars
    sh_a = NULL;
    sh_v = NULL;

    // Check for Vorbis
    if(pack.bytes >= 7 && ! strncmp(&pack.packet[1],"vorbis", 6) ) {
      sh_a = new_sh_audio(demuxer,ogg_d->num_sub);
      sh_a->format = 0xFFFE;
      n_audio++;
      mp_msg(MSGT_DEMUX,MSGL_V,"OGG : stream %d is vorbis\n",ogg_d->num_sub);

      /// Check for old header
    } else if(pack.bytes >= 142 && ! strncmp(&pack.packet[1],"Direct Show Samples embedded in Ogg",35) ) {

       // Old video header
      if(*(int32_t*)(pack.packet+96) == 0x05589f80 && pack.bytes >= 184) {
	sh_v = new_sh_video(demuxer,ogg_d->num_sub);
	sh_v->bih = (BITMAPINFOHEADER*)calloc(1,sizeof(BITMAPINFOHEADER));
	sh_v->bih->biSize=sizeof(BITMAPINFOHEADER);
	sh_v->bih->biCompression=
	sh_v->format = mmioFOURCC(pack.packet[68],pack.packet[69],
				pack.packet[70],pack.packet[71]);
	sh_v->frametime =  (*(int64_t*)(pack.packet+164))*0.0000001;
	sh_v->fps = 1/sh_v->frametime;
	sh_v->disp_w = sh_v->bih->biWidth = *(int32_t*)(pack.packet+176);
	sh_v->disp_h = sh_v->bih->biHeight = *(int32_t*)(pack.packet+180);
	sh_v->bih->biBitCount = *(int16_t*)(pack.packet+182);
	if(!sh_v->bih->biBitCount) sh_v->bih->biBitCount=24; // hack, FIXME
	sh_v->bih->biPlanes=1;
	sh_v->bih->biSizeImage=(sh_v->bih->biBitCount>>3)*sh_v->bih->biWidth*sh_v->bih->biHeight;

	ogg_d->subs[ogg_d->num_sub].samplerate = sh_v->fps;
	n_video++;
	mp_msg(MSGT_DEMUX,MSGL_V,"OGG stream %d is video (old hdr)\n",ogg_d->num_sub);
	if(verbose) print_video_header(sh_v->bih);
	// Old audio header
      } else if(*(int32_t*)pack.packet+96 == 0x05589F81) {
	unsigned int extra_size;
	sh_a = new_sh_audio(demuxer,ogg_d->num_sub);
	extra_size = *(int16_t*)(pack.packet+140);
	sh_a->wf = (WAVEFORMATEX*)calloc(1,sizeof(WAVEFORMATEX)+extra_size);
	sh_a->format = sh_a->wf->wFormatTag = *(int16_t*)(pack.packet+124);
	sh_a->channels = sh_a->wf->nChannels = *(int16_t*)(pack.packet+126);
	sh_a->samplerate = sh_a->wf->nSamplesPerSec = *(int32_t*)(pack.packet+128);
	sh_a->wf->nAvgBytesPerSec = *(int32_t*)(pack.packet+132);
	sh_a->wf->nBlockAlign = *(int16_t*)(pack.packet+136);
	sh_a->wf->wBitsPerSample = *(int16_t*)(pack.packet+138);
	sh_a->samplesize = (sh_a->wf->wBitsPerSample+7)/8;
	sh_a->wf->cbSize = extra_size;
	if(extra_size > 0)
	  memcpy(sh_a->wf+sizeof(WAVEFORMATEX),pack.packet+142,extra_size);

	ogg_d->subs[ogg_d->num_sub].samplerate = sh_a->samplerate * sh_a->channels;
	n_audio++;
	mp_msg(MSGT_DEMUX,MSGL_V,"OGG stream %d is audio (old hdr)\n",ogg_d->num_sub);
	if(verbose) print_wave_header(sh_a->wf);
      } else
	mp_msg(MSGT_DEMUX,MSGL_WARN,"OGG stream %d contain an old header but the header type is unknow\n",ogg_d->num_sub);

        // Check new header
    } else if ( (*pack.packet & PACKET_TYPE_BITS ) == PACKET_TYPE_HEADER && 
	      pack.bytes >= (int)sizeof(stream_header)+1) {
      stream_header *st = (stream_header*)(pack.packet+1);
      /// New video header
      if(strncmp(st->streamtype,"video",5) == 0) {
	sh_v = new_sh_video(demuxer,ogg_d->num_sub);
	sh_v->bih = (BITMAPINFOHEADER*)calloc(1,sizeof(BITMAPINFOHEADER));
	sh_v->format = mmioFOURCC(st->subtype[0],st->subtype[1],
				  st->subtype[2],st->subtype[3]);
	sh_v->frametime = st->time_unit*0.0000001;
	sh_v->fps = 1/sh_v->frametime;
	sh_v->bih->biSize = st->buffersize;
	sh_v->bih->biBitCount = st->bits_per_sample;
	sh_v->disp_w = sh_v->bih->biWidth = st->sh.video.width;
	sh_v->disp_h = sh_v->bih->biHeight = st->sh.video.height;

	ogg_d->subs[ogg_d->num_sub].samplerate= sh_v->fps;
	n_video++;
	mp_msg(MSGT_DEMUX,MSGL_V,"OGG stream %d is video (new hdr)\n",ogg_d->num_sub);
	if(verbose) print_video_header(sh_v->bih);
	/// New audio header
      } else if(strncmp(st->streamtype,"audio",5) == 0) {
	char buffer[5];
	unsigned int extra_size = st->size - sizeof(stream_header);
	memcpy(buffer,st->subtype,4);
	buffer[4] = '\0';
	sh_a = new_sh_audio(demuxer,ogg_d->num_sub);
	sh_a->wf = (WAVEFORMATEX*)calloc(1,sizeof(WAVEFORMATEX)+extra_size);
	sh_a->format =  sh_a->wf->wFormatTag = atoi(buffer);
	sh_a->channels = sh_a->wf->nChannels = st->sh.audio.channels;
	sh_a->samplerate = sh_a->wf->nSamplesPerSec = st->samples_per_unit;
	sh_a->wf->nAvgBytesPerSec = st->sh.audio.avgbytespersec;
	sh_a->wf->nBlockAlign = st->sh.audio.blockalign;
	sh_a->wf->wBitsPerSample = st->bits_per_sample;
	sh_a->samplesize = (sh_a->wf->wBitsPerSample+7)/8;
	sh_a->wf->cbSize = extra_size;
	if(extra_size)
	  memcpy(sh_a->wf+sizeof(WAVEFORMATEX),st+1,extra_size);

	ogg_d->subs[ogg_d->num_sub].samplerate = sh_a->samplerate * sh_a->channels;
	n_audio++;
	mp_msg(MSGT_DEMUX,MSGL_V,"OGG stream %d is audio (new hdr)\n",ogg_d->num_sub);
	if(verbose) print_wave_header(sh_a->wf);

	/// Check for text (subtitles) header
      } else if(strncmp(st->streamtype,"text",4) == 0) {
	mp_msg(MSGT_DEMUX,MSGL_WARN,"OGG text stream are not supported\n");
	//// Unknow header type
      } else
	mp_msg(MSGT_DEMUX,MSGL_ERR,"OGG stream %d has a header marker but is of an unknow type\n",ogg_d->num_sub);
      /// Unknow (invalid ?) header
    } else
      mp_msg(MSGT_DEMUX,MSGL_ERR,"OGG stream %d is of an unknow type\n",ogg_d->num_sub);

    if(sh_a || sh_v) {
      demux_stream_t* ds = NULL;
      if(sh_a) {
	// If the audio stream is not defined we took the first one
	if(demuxer->audio->id == -1) {
	  demuxer->audio->id = ogg_d->num_sub;
	  demuxer->audio->sh = sh_a;
	  sh_a->ds = demuxer->audio;
//	  if(sh_a->wf) print_wave_header(sh_a->wf);
	}
	/// Is it the stream we want
	if(demuxer->audio->id == ogg_d->num_sub)
	  ds = demuxer->audio;
      }
      if(sh_v) {
	/// Also for video
	if(demuxer->video->id == -1) {
	  demuxer->video->id = ogg_d->num_sub;
	  demuxer->video->sh = sh_v;
	  sh_v->ds = demuxer->video;
//	  if(sh_v->bih) print_video_header(sh_v->bih);
	}
	if(demuxer->video->id == ogg_d->num_sub)
	  ds = demuxer->video;
      }
      /// Add the packet contained in this page
       if(ds) {
	 /// Finish the page, otherwise packets will be lost
	 do {
	   demux_ogg_add_packet(ds,&ogg_d->subs[ogg_d->num_sub],&pack);
	 } while(ogg_stream_packetout(&ogg_d->subs[ogg_d->num_sub].stream,&pack) == 1);
	 
       }
    }
    ogg_d->num_sub++;      
  }

  /// Finish to setup the demuxer
  demuxer->priv = ogg_d;
  /// We can't seek :(
  demuxer->seekable = 0;

  if(!n_video)
    demuxer->video->id = -2;
  if(!n_audio)
    demuxer->audio->id = -2;

  mp_msg(MSGT_DEMUX,MSGL_V,"OGG demuxer : found %d audio stream and %d video stream\n",n_audio,n_video);
 
  return 1;
}


int demux_ogg_fill_buffer(demuxer_t *d) {
  ogg_demuxer_t* ogg_d;
  stream_t *s;
  demux_stream_t *ds;
  ogg_sync_state* sync;
  ogg_stream_state* os;
  ogg_page* page;
  ogg_packet pack;
  int np = 0, id=0;

  s = d->stream;
  ogg_d = d->priv;
  sync = &ogg_d->sync;
  page = &ogg_d->page;

  /// Find the stream we are working on
  if ( (id = demux_ogg_get_page_stream(ogg_d,&os)) < 0) {
      mp_msg(MSGT_DEMUX,MSGL_ERR,"OGG demuxer : can't get current stream\n");
      return 0;
  }

  while(1) {
    np = 0;
    ds = NULL;
    /// Try to get some packet from the current page
    while( (np = ogg_stream_packetout(os,&pack)) != 1) {
      /// No packet we go the next page
      if(np == 0) {
	while(1) {
	  int pa,len;
	  char *buf;
	  /// Get the next page from the physical stream
	  while( (pa = ogg_sync_pageout(sync,page)) != 1) {
	    /// Error : we skip some bytes
	    if(pa < 0) {
	      mp_msg(MSGT_DEMUX,MSGL_WARN,"OGG : Page out not synced, we skip some bytes\n");
	      continue;
	    }
	    /// We need more data
	    buf = ogg_sync_buffer(sync,BLOCK_SIZE);	    
	    len = stream_read(s,buf,BLOCK_SIZE);
	    if(len == 0 && s->eof) {
	      mp_msg(MSGT_DEMUX,MSGL_DBG2,"OGG : Stream EOF !!!!\n");
	      return 0;
	    }
	    ogg_sync_wrote(sync,len);
	  } /// Page loop

	  /// Find the page's logical stream
	  if( (id = demux_ogg_get_page_stream(ogg_d,&os)) < 0) {
	    mp_msg(MSGT_DEMUX,MSGL_ERR,"OGG demuxer error : we met an unknow stream\n");
	    return 0;
	  }
	  /// Take the page
	  if(ogg_stream_pagein(os,page) == 0)
	    break;
	  /// Page was invalid => retry
	  mp_msg(MSGT_DEMUX,MSGL_WARN,"OGG demuxer : got invalid page !!!!!\n");
	}
      } else /// Packet was corrupted
	mp_msg(MSGT_DEMUX,MSGL_WARN,"OGG : bad packet in stream %d\n",id);
    } /// Packet loop
    
    /// Is the actual logical stream in use ?
    if(id == d->audio->id)
      ds = d->audio;
    else if(id == d->video->id)
      ds = d->video;

    if(ds) {
      if(!demux_ogg_add_packet(ds,&ogg_d->subs[id],&pack))
	continue; /// Unuseful packet, get another
      return 1;
    }

  } /// while(1)

}

/// For avi with Ogg audio stream we have to create an ogg demuxer for this
// stream, then we join the avi and ogg demuxer with a demuxers demuxer
demuxer_t* init_avi_with_ogg(demuxer_t* demuxer) {
  demuxer_t  *od;
  ogg_demuxer_t *ogg_d;
  stream_t* s;
  uint32_t hdrsizes[3];
  demux_packet_t *dp;
  sh_audio_t *sh_audio = demuxer->audio->sh;
  int np;
  unsigned char *p = NULL,*buf;
  int plen;

  /// Check that the cbSize is enouth big for the following reads
  if(sh_audio->wf->cbSize < 3*sizeof(uint32_t)) {
    mp_msg(MSGT_DEMUX,MSGL_ERR,"AVI OGG : Initial audio header is too small !!!!!\n");
    goto fallback;
  }
  /// Get the size of the 3 header packet
  memcpy(hdrsizes, ((unsigned char*)sh_audio->wf)+2*sizeof(WAVEFORMATEX), 3*sizeof(uint32_t));

  /// Check the size
  if(sh_audio->wf->cbSize < 3*sizeof(uint32_t) + sizeof(uint32_t)+hdrsizes[0]+hdrsizes[1] + hdrsizes[2]) {
    mp_msg(MSGT_DEMUX,MSGL_ERR,"AVI OGG : Audio header is too small !!!!!\n");
    goto fallback;
  }

  // Build the ogg demuxer private datas
  ogg_d = (ogg_demuxer_t*)calloc(1,sizeof(ogg_demuxer_t));
  ogg_d->num_sub = 1;
  ogg_d->subs = (ogg_stream_t*)malloc(sizeof(ogg_stream_t));

   // Init the ogg physical stream
  ogg_sync_init(&ogg_d->sync);

  // Get the first page of the stream : we assume there only 1 logical stream
  while((np = ogg_sync_pageout(&ogg_d->sync,&ogg_d->page)) <= 0 ) {
    if(np < 0) {
      mp_msg(MSGT_DEMUX,MSGL_ERR,"AVI OGG error : Can't init using first stream packets\n");
      free(ogg_d);
      goto fallback;
    }
    // Add some data
    plen = ds_get_packet(demuxer->audio,&p);    
    buf = ogg_sync_buffer(&ogg_d->sync,plen);
    memcpy(buf,p,plen);
    ogg_sync_wrote(&ogg_d->sync,plen);
  }
  // Init the logical stream
  mp_msg(MSGT_DEMUX,MSGL_DBG2,"AVI OGG found page with serial %d\n",ogg_page_serialno(&ogg_d->page));
  ogg_stream_init(&ogg_d->subs[0].stream,ogg_page_serialno(&ogg_d->page));
  // Write the page
  ogg_stream_pagein(&ogg_d->subs[0].stream,&ogg_d->page);

  // Create the ds_stream and the ogg demuxer
  s = new_ds_stream(demuxer->audio);
  od = new_demuxer(s,DEMUXER_TYPE_OGG,0,-2,-2);

  /// Add the header packets in the ogg demuxer audio stream
  // Initial header
  dp = new_demux_packet(hdrsizes[0]);
  memcpy(dp->buffer,((unsigned char*)sh_audio->wf)+2*sizeof(WAVEFORMATEX)+3*sizeof(uint32_t),hdrsizes[0]);
  ds_add_packet(od->audio,dp);
  /// Comments
  dp = new_demux_packet(hdrsizes[1]);
  memcpy(dp->buffer,((unsigned char*)sh_audio->wf)+2*sizeof(WAVEFORMATEX)+3*sizeof(uint32_t)+hdrsizes[0],hdrsizes[1]);
  ds_add_packet(od->audio,dp);
  /// Code book
  dp = new_demux_packet(hdrsizes[2]);
  memcpy(dp->buffer,((unsigned char*)sh_audio->wf)+2*sizeof(WAVEFORMATEX)+3*sizeof(uint32_t)+hdrsizes[0]+hdrsizes[1],hdrsizes[2]);
  ds_add_packet(od->audio,dp);

  // Finish setting up the ogg demuxer
  od->priv = ogg_d;
  sh_audio = new_sh_audio(od,0);
  od->audio->id = 0;
  od->video->id = -2;
  od->audio->sh = sh_audio;
  sh_audio->ds = od->audio;
  sh_audio->format = 0xFFFE;

  /// Return the joined demuxers
  return new_demuxers_demuxer(demuxer,od,demuxer);

 fallback:
  demuxer->audio->id = -2;
  return demuxer;

}

/// TODO : Seeking 8-)
  
#endif
