
#include "config.h"

#ifdef HAVE_OGGVORBIS

#include <stdlib.h>
#include <stdio.h>

#include "../mp_msg.h"
#include "../help_mp.h"
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#define BLOCK_SIZE 4096

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

typedef struct ogg_syncpoint {
  int64_t granulepos;
  off_t page_pos;
} ogg_syncpoint_t;

/// A logical stream
typedef struct ogg_stream {
  /// Timestamping stuff
  float samplerate; /// granulpos 2 time
  int64_t lastpos;
  int32_t lastsize;

  // Logical stream state
  ogg_stream_state stream;
  int hdr_packets;
  int vorbis;
} ogg_stream_t;

typedef struct ogg_demuxer {
  /// Physical stream state
  ogg_sync_state sync;
  /// Current page
  ogg_page page;
  /// Logical streams
  ogg_stream_t *subs;
  int num_sub;
  ogg_syncpoint_t* syncpoints;
  int num_syncpoint;
  off_t pos, last_size;
} ogg_demuxer_t;

#define NUM_VORBIS_HDR_PACKETS 3

/// Some defines from OggDS
#define PACKET_TYPE_HEADER   0x01
#define PACKET_TYPE_BITS         0x07
#define PACKET_LEN_BITS01       0xc0
#define PACKET_LEN_BITS2         0x02
#define PACKET_IS_SYNCPOINT  0x08

extern int index_mode;

//-------- subtitle support - should be moved to decoder layer, and queue
//                          - subtitles up in demuxer buffer...

#include "../subreader.h"
#include "../libvo/sub.h"
#define OGG_SUB_MAX_LINE 128

static subtitle ogg_sub;
extern subtitle* vo_sub;
//FILE* subout;

void demux_ogg_init_sub () {
  int lcv;
  if(!ogg_sub.text[0]) // not yet allocated
  for (lcv = 0; lcv < SUB_MAX_TEXT; lcv++) {
    ogg_sub.text[lcv] = (char*)malloc(OGG_SUB_MAX_LINE);
  }
}

void demux_ogg_add_sub (char* packet) {
  int lcv;
  int line_pos = 0;
  int ignoring = 0;

  mp_msg(MSGT_DEMUX,MSGL_DBG2,"\ndemux_ogg_add_sub %02X %02X %02X '%s'\n",
      (unsigned char)packet[0],
      (unsigned char)packet[1],
      (unsigned char)packet[2],
      &packet[3]);

  ogg_sub.lines = 0;
  if (((unsigned char)packet[0]) == 0x88) { // some subtitle text
    lcv = 3;
    while (1) {
      int c = packet[lcv++];
      if(c=='\n' || c==0 || line_pos >= OGG_SUB_MAX_LINE-1){
	  ogg_sub.text[ogg_sub.lines][line_pos] = 0; // close sub
          if(line_pos) ogg_sub.lines++;
	  if(!c || ogg_sub.lines>=SUB_MAX_TEXT) break; // EOL or TooMany
          line_pos = 0;
      }
      switch (c) {
        case '\r':
        case '\n': // just ignore linefeeds for now
                   // their placement seems rather haphazard
          break;
        case '<': // some html markup, ignore for now
          ignoring = 1;
          break;
        case '>':
          ignoring = 0;
          break;
        default:
          if(!ignoring) 
	  ogg_sub.text[ogg_sub.lines][line_pos++] = c;
          break;
      }
    }
  }

  mp_msg(MSGT_DEMUX,MSGL_DBG2,"ogg sub lines: %d  first: '%s'\n",
      ogg_sub.lines, ogg_sub.text[0]);
  vo_sub = &ogg_sub;
  vo_osd_changed(OSDTYPE_SUBTITLE);
}


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

static unsigned char* demux_ogg_read_packet(ogg_stream_t* os,ogg_packet* pack,vorbis_info* vi,float* pts,int* flags) {
  unsigned char* data;

  *pts = 0;
  *flags = 0;

  if(os->vorbis) {
    data = pack->packet;
    if(*pack->packet & PACKET_TYPE_HEADER)
      os->hdr_packets++;
    else if(vi) {
      // When we dump the audio, there is no vi, but we dont care of timestamp in this case
      int32_t blocksize = vorbis_packet_blocksize(vi,pack) / vi->channels;
      // Calculate the timestamp if the packet don't have any
      if(pack->granulepos == -1) {
	pack->granulepos = os->lastpos;
	if(os->lastsize > 0)
	  pack->granulepos += os->lastsize;
      }
      *pts = pack->granulepos / (float)vi->rate;
      os->lastsize = blocksize;
      os->lastpos = pack->granulepos;
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
    if(*pack->packet & PACKET_IS_SYNCPOINT)
      *flags = 1;
    *pts =  pack->granulepos/os->samplerate;
    // Save the packet length and timestamp
    os->lastsize = 0;
    while(hdrlen) {
      os->lastsize <<= 8;
      os->lastsize |= pack->packet[hdrlen];
      hdrlen--;
    }
    os->lastpos = pack->granulepos;
  }
  return data;
}

/// Calculate the timestamp and add the packet to the demux stream
// return 1 if the packet was added, 0 otherwise
static int demux_ogg_add_packet(demux_stream_t* ds,ogg_stream_t* os,ogg_packet* pack) {
  demuxer_t* d = ds->demuxer;
  demux_packet_t* dp;
  unsigned char* data;
  float pts = 0;
  int flags = 0;

  if (ds == d->sub) { // don't want to add subtitles to the demuxer for now
    demux_ogg_add_sub(pack->packet);
    return 0;
  }
  // If packet is an header we jump it except for vorbis
  if((*pack->packet & PACKET_TYPE_HEADER) && 
     (ds == d->video || (ds == d->audio && ( ((sh_audio_t*)ds->sh)->format != 0xFFFE || os->hdr_packets >= NUM_VORBIS_HDR_PACKETS ) ) ))
    return 0;

  // For vorbis packet the packet is the data, for other codec we must jump the header
  if(ds == d->audio && ((sh_audio_t*)ds->sh)->format == 0xFFFE)
    data = demux_ogg_read_packet(os,pack,&((ov_struct_t*)((sh_audio_t*)ds->sh)->context)->vi,
				 &pts,&flags);
  else
    data = demux_ogg_read_packet(os,pack,NULL,&pts,&flags);


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

/// Build a table of all syncpoints to make seeking easier
void demux_ogg_build_syncpoints_table(demuxer_t* demuxer) {
  ogg_demuxer_t* ogg_d = demuxer->priv;
  stream_t *s = demuxer->stream;
  ogg_sync_state* sync = &ogg_d->sync;
  ogg_page* page= &ogg_d->page;
  ogg_stream_state* oss;
  ogg_stream_t* os;
  ogg_packet op;
  int np,sid,p;
  vorbis_info* vi = NULL;
  off_t pos, last_pos;
  pos = last_pos = demuxer->movi_start;

  // Reset the stream
  stream_seek(s,demuxer->movi_start);
  ogg_sync_reset(sync);

  // Get the serial number of the stream we use
  if(demuxer->video->id >= 0)
    sid = demuxer->video->id;
  else {
    sid = demuxer->audio->id;
    if(((sh_audio_t*)demuxer->audio->sh)->format == 0xFFFE)
      vi = &((ov_struct_t*)((sh_audio_t*)demuxer->audio->sh)->context)->vi;
  }
  os = &ogg_d->subs[sid];
  oss = &os->stream;

  while(1) {
    np = ogg_sync_pageseek(sync,page);
    if(np < 0) { // We had to skip some bytes
      mp_msg(MSGT_DEMUX,MSGL_ERR,"Bad page sync while building syncpoints table (%ld)\n",-np);
      pos += -np;
      continue;
    }
    if(np <= 0) { // We need more data
      char* buf = ogg_sync_buffer(sync,BLOCK_SIZE);
      int len = stream_read(s,buf,BLOCK_SIZE);
      if(len == 0 && s->eof)
	break;
      ogg_sync_wrote(sync,len);
      continue;
    }
    // The page is ready
    //ogg_sync_pageout(sync,page);
    if(ogg_page_serialno(page) != os->stream.serialno) { // It isn't a page from the stream we want
      pos += np;
      continue;
    }
    if(ogg_stream_pagein(oss,page) != 0) {
      mp_msg(MSGT_DEMUX,MSGL_ERR,"Pagein error ????\n");
      pos += np;
      continue;
    }
    p = 0;
    while(ogg_stream_packetout(oss,&op) == 1) {
      float pts;
      int flags;
      demux_ogg_read_packet(os,&op,vi,&pts,&flags);
      if(flags || (os->vorbis && op.granulepos >= 0)) {
	ogg_d->syncpoints = (ogg_syncpoint_t*)realloc(ogg_d->syncpoints,(ogg_d->num_syncpoint+1)*sizeof(ogg_syncpoint_t));
	ogg_d->syncpoints[ogg_d->num_syncpoint].granulepos = op.granulepos;
	ogg_d->syncpoints[ogg_d->num_syncpoint].page_pos = (ogg_page_continued(page) && p == 0) ? last_pos : pos;
	ogg_d->num_syncpoint++;
      }
      p++;
    }
    if(p > 1 || (p == 1 && ! ogg_page_continued(page)))
      last_pos = pos;
    pos += np;
    mp_msg(MSGT_DEMUX,MSGL_INFO,"Building syncpoint table %d%\r",pos*100/s->end_pos);
  }
  mp_msg(MSGT_DEMUX,MSGL_INFO,"\n");

  mp_msg(MSGT_DEMUX,MSGL_V,"Ogg syncpoints table builed: %d syncpoints\n",ogg_d->num_syncpoint);

  stream_reset(s);
  stream_seek(s,demuxer->movi_start);
  ogg_sync_reset(sync);
  for(np = 0 ; np < ogg_d->num_sub ; np++) {
    ogg_stream_reset(&ogg_d->subs[np].stream);
    ogg_d->subs[np].lastpos = ogg_d->subs[np].lastsize = ogg_d->subs[np].hdr_packets = 0;
  }

  
  // Get the first page
  while(1) {
    np = ogg_sync_pageout(sync,page);
    if(np <= 0) { // We need more data
      char* buf = ogg_sync_buffer(sync,BLOCK_SIZE);
      int len = stream_read(s,buf,BLOCK_SIZE);
      if(len == 0 && s->eof) {
	mp_msg(MSGT_DEMUX,MSGL_ERR,"EOF while trying to get the first page !!!!\n");
	break;
      }
      
      ogg_sync_wrote(sync,len);
      continue;
    }
    demux_ogg_get_page_stream(ogg_d,&oss);
    ogg_stream_pagein(oss,page);
    break;
  }
  
}

/// Open an ogg physical stream
int demux_ogg_open(demuxer_t* demuxer) {
  ogg_demuxer_t* ogg_d;
  stream_t *s;
  char* buf;
  int np,s_no, n_audio = 0, n_video = 0, n_text = 0;
  ogg_sync_state* sync;
  ogg_page* page;
  ogg_packet pack;
  sh_audio_t* sh_a;
  sh_video_t* sh_v;

  s = demuxer->stream;

  ogg_d = (ogg_demuxer_t*)calloc(1,sizeof(ogg_demuxer_t));
  sync = &ogg_d->sync;
  page = &ogg_d->page;

  ogg_sync_init(sync);

  while(1) {
    /// Try to get a page
    ogg_d->pos += ogg_d->last_size;
    np = ogg_sync_pageseek(sync,page);
    /// Error
    if(np < 0) {
      mp_msg(MSGT_DEMUX,MSGL_DBG2,"OGG demuxer : Bad page sync\n");
      return 0;
    }
    /// Need some more data
    if(np == 0) {
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
    ogg_d->last_size = np;
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
      ogg_d->subs[ogg_d->num_sub].vorbis = 1;
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

	ogg_d->subs[ogg_d->num_sub].samplerate = sh_a->samplerate; // * sh_a->channels;
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
	sh_v->bih->biSize=sizeof(BITMAPINFOHEADER);
	sh_v->bih->biCompression=
	sh_v->format = mmioFOURCC(st->subtype[0],st->subtype[1],
				  st->subtype[2],st->subtype[3]);
	sh_v->frametime = st->time_unit*0.0000001;
	sh_v->fps = 1/sh_v->frametime;
	sh_v->bih->biBitCount = st->bits_per_sample;
	sh_v->disp_w = sh_v->bih->biWidth = st->sh.video.width;
	sh_v->disp_h = sh_v->bih->biHeight = st->sh.video.height;
	if(!sh_v->bih->biBitCount) sh_v->bih->biBitCount=24; // hack, FIXME
	sh_v->bih->biPlanes=1;
	sh_v->bih->biSizeImage=(sh_v->bih->biBitCount>>3)*sh_v->bih->biWidth*sh_v->bih->biHeight;

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
	sh_a->format =  sh_a->wf->wFormatTag = strtol(buffer, NULL, 16);
	sh_a->channels = sh_a->wf->nChannels = st->sh.audio.channels;
	sh_a->samplerate = sh_a->wf->nSamplesPerSec = st->samples_per_unit;
	sh_a->wf->nAvgBytesPerSec = st->sh.audio.avgbytespersec;
	sh_a->wf->nBlockAlign = st->sh.audio.blockalign;
	sh_a->wf->wBitsPerSample = st->bits_per_sample;
	sh_a->samplesize = (sh_a->wf->wBitsPerSample+7)/8;
	sh_a->wf->cbSize = extra_size;
	if(extra_size)
	  memcpy(sh_a->wf+sizeof(WAVEFORMATEX),st+1,extra_size);

	ogg_d->subs[ogg_d->num_sub].samplerate = sh_a->samplerate; // * sh_a->channels;
	n_audio++;
	mp_msg(MSGT_DEMUX,MSGL_V,"OGG stream %d is audio (new hdr)\n",ogg_d->num_sub);
	if(verbose) print_wave_header(sh_a->wf);

	/// Check for text (subtitles) header
      } else if (strncmp(st->streamtype, "text", 4) == 0) {
          mp_msg(MSGT_DEMUX, MSGL_V, "OGG stream %d is text\n", ogg_d->num_sub);
          if(demuxer->sub->id==-1) demuxer->sub->id = ogg_d->num_sub;
          n_text++;
          demux_ogg_init_sub();
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
//	  if(sh_a->wf) print_wave_header(sh_a->wf);
	}
	/// Is it the stream we want
	if(demuxer->audio->id == ogg_d->num_sub) {
	  demuxer->audio->sh = sh_a;
	  sh_a->ds = demuxer->audio;
	  ds = demuxer->audio;
	}
      }
      if(sh_v) {
	/// Also for video
	if(demuxer->video->id == -1) {
	  demuxer->video->id = ogg_d->num_sub;
//	  if(sh_v->bih) print_video_header(sh_v->bih);
	}
	if(demuxer->video->id == ogg_d->num_sub) {
	  demuxer->video->sh = sh_v;
	  sh_v->ds = demuxer->video;
	  ds = demuxer->video;
	}
      }
      /// Add the header packets if the stream isn't seekable
      if(ds && (!s->end_pos || index_mode != 2)) {
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

  if(!n_video)
    demuxer->video->id = -2;
  if(!n_audio)
    demuxer->audio->id = -2;
  if(!n_text)
    demuxer->sub->id = -2;

  if(!s->end_pos)
    demuxer->seekable = 0;
  else {
    demuxer->movi_start = 0;
    demuxer->movi_end = s->end_pos;
    demuxer->seekable = 1;
    if(index_mode == 2)
      demux_ogg_build_syncpoints_table(demuxer);
  }

  mp_msg(MSGT_DEMUX,MSGL_V,"OGG demuxer : found %d audio stream%s, %d video stream%s and %d text stream%s\n",n_audio,n_audio>1?"s":"",n_video,n_video>1?"s":"",n_text,n_text>1?"s":"");
 
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
	  ogg_d->pos += ogg_d->last_size;
	  /// Get the next page from the physical stream
	  while( (pa = ogg_sync_pageseek(sync,page)) <= 0) {
	    /// Error : we skip some bytes
	    if(pa < 0) {
	      mp_msg(MSGT_DEMUX,MSGL_WARN,"OGG : Page out not synced, we skip some bytes\n");
	      ogg_d->pos -= pa;
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
	  ogg_d->last_size = pa;
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
	  ogg_d->pos += ogg_d->last_size;
	}
      } else /// Packet was corrupted
	mp_msg(MSGT_DEMUX,MSGL_WARN,"OGG : bad packet in stream %d\n",id);
    } /// Packet loop
    
    /// Is the actual logical stream in use ?
    if(id == d->audio->id)
      ds = d->audio;
    else if(id == d->video->id)
      ds = d->video;
    else if (id == d->sub->id)
      ds = d->sub;

    if(ds) {
      if(!demux_ogg_add_packet(ds,&ogg_d->subs[id],&pack))
	continue; /// Unuseful packet, get another
      d->filepos = ogg_d->pos;
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
  ogg_d->subs[0].vorbis = 1;

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

void demux_ogg_seek(demuxer_t *demuxer,float rel_seek_secs,int flags) {
  ogg_demuxer_t* ogg_d = demuxer->priv;
  ogg_sync_state* sync = &ogg_d->sync;
  ogg_page* page= &ogg_d->page;
  ogg_stream_state* oss;
  ogg_stream_t* os;
  demux_stream_t* ds;
  sh_audio_t* sh_audio = demuxer->audio->sh;
  ogg_packet op;
  float rate;
  int i,sp;
  vorbis_info* vi = NULL;
  int64_t gp = 0;
  off_t pos;

  if(demuxer->video->id >= 0) {
    ds = demuxer->video;
    rate = ogg_d->subs[ds->id].samplerate;
  } else {
    ds = demuxer->audio;
    vi = &((ov_struct_t*)((sh_audio_t*)ds->sh)->context)->vi;
    rate = (float)vi->rate;
  }

  os = &ogg_d->subs[ds->id];
  oss = &os->stream;

  if(ogg_d->syncpoints) {
    float time_pos = flags & 1 ? 0 : os->lastpos/ rate;
    if(flags & 2) {
      if(ogg_d->syncpoints)
	time_pos += ogg_d->syncpoints[ogg_d->num_syncpoint].granulepos / rate * rel_seek_secs;
      else
	time_pos += (demuxer->movi_end - demuxer->movi_start) * rel_seek_secs;
    } else
      time_pos += rel_seek_secs;
    
    gp = time_pos * rate;

    for(sp = 0; sp < ogg_d->num_syncpoint ; sp++) {
      if(ogg_d->syncpoints[sp].granulepos >= gp)
	break;
    }

    if(sp >= ogg_d->num_syncpoint)
      return;
    pos = ogg_d->syncpoints[sp].page_pos;

  } else {
    pos = flags & 1 ? demuxer->movi_start : ogg_d->pos;
    if(flags & 2)
      pos += (demuxer->movi_end - demuxer->movi_start) * rel_seek_secs;
    else
      pos += rel_seek_secs * ogg_d->pos / (os->lastpos / rate);

    if(pos < demuxer->movi_start)
      pos = demuxer->movi_start;
    else if(pos > demuxer->movi_end)
      return;
  }

  stream_seek(demuxer->stream,pos);
  ogg_sync_reset(sync);
  for(i = 0 ; i < ogg_d->num_sub ; i++) {
    ogg_stream_reset(&ogg_d->subs[i].stream);
    ogg_d->subs[i].lastpos = ogg_d->subs[i].lastsize = 0;
  }
  ogg_d->pos = pos;
  ogg_d->last_size = 0;

  while(1) {
    int np;
    ogg_d->pos += ogg_d->last_size;
    ogg_d->last_size = 0;
    np = ogg_sync_pageseek(sync,page);

    if(np < 0)
      ogg_d->pos -= np;
    if(np <= 0) { // We need more data
      char* buf = ogg_sync_buffer(sync,BLOCK_SIZE);
      int len = stream_read(demuxer->stream,buf,BLOCK_SIZE);
       if(len == 0 && demuxer->stream->eof) {
	mp_msg(MSGT_DEMUX,MSGL_ERR,"EOF while trying to seek !!!!\n");
	break;
      }
      ogg_sync_wrote(sync,len);
      continue;
    }
    ogg_d->last_size = np;
    if(ogg_page_serialno(page) != oss->serialno)
      continue;

    if(ogg_stream_pagein(oss,page) != 0)
      continue;

     while(1) {
      np = ogg_stream_packetout(oss,&op);
      if(np < 0)
	continue;
      else if(np == 0)
	break;

      if( ((*op.packet & PACKET_IS_SYNCPOINT)  || os->vorbis )  &&
	  (!ogg_d->syncpoints || op.granulepos >= gp) ) {
	demux_ogg_add_packet(ds,os,&op);
	if(sh_audio)
	  resync_audio_stream(sh_audio); 
	return;
      }
     }
  }

  mp_msg(MSGT_DEMUX,MSGL_ERR,"Can't find the good packet :(\n");  

}

void demux_close_ogg(demuxer_t* demuxer) {
  ogg_demuxer_t* ogg_d = demuxer->priv;

  if(!ogg_d)
    return;

  if(ogg_d->subs)
    free(ogg_d->subs);
  if(ogg_d->syncpoints)
    free(ogg_d->syncpoints);
  free(ogg_d);
}

#endif
