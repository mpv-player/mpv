#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#include "url.h"
#include "http.h"
#include "libmpdemux/asf.h"

#include "stream.h"
#include "libmpdemux/demuxer.h"

#include "network.h"
#include "tcp.h"

#include "libavutil/intreadwrite.h"

#include "libmpdemux/asfguid.h"

extern int network_bandwidth;

int asf_mmst_streaming_start( stream_t *stream );
static int asf_http_streaming_start(stream_t *stream, int *demuxer_type);

static int asf_read_wrapper(int fd, void *buffer, int len, streaming_ctrl_t *stream_ctrl) {
    uint8_t *buf = buffer;
    while (len > 0) {
        int got = nop_streaming_read(fd, buf, len, stream_ctrl);
        if (got <= 0) {
            mp_msg(MSGT_NETWORK, MSGL_ERR, MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream);
            return got;
        }
        buf += got;
        len -= got;
    }
    return 1;
}

// We can try several protocol for asf streaming
// * first the UDP protcol, if there is a firewall, UDP
//   packets will not come back, so the mmsu will fail.
// * Then we can try TCP, but if there is a proxy for
//   internet connection, the TCP connection will not get
//   through
// * Then we can try HTTP.
// 
// Note: Using 	WMP sequence  MMSU then MMST and then HTTP.

static int asf_streaming_start( stream_t *stream, int *demuxer_type) {
    char *proto = stream->streaming_ctrl->url->protocol;
    int fd = -1;
    int port = stream->streaming_ctrl->url->port;

    // Is protocol mms or mmsu?
    if (!strcasecmp(proto, "mmsu") || !strcasecmp(proto, "mms"))
    {
		mp_msg(MSGT_NETWORK,MSGL_V,"Trying ASF/UDP...\n");
		//fd = asf_mmsu_streaming_start( stream );
		if( fd>-1 ) return fd; //mmsu support is not implemented yet - using this code
		mp_msg(MSGT_NETWORK,MSGL_V,"  ===> ASF/UDP failed\n");
		if( fd==-2 ) return -1;
	}

    //Is protocol mms or mmst?
    if (!strcasecmp(proto, "mmst") || !strcasecmp(proto, "mms"))
    {
		mp_msg(MSGT_NETWORK,MSGL_V,"Trying ASF/TCP...\n");
		fd = asf_mmst_streaming_start( stream );
		stream->streaming_ctrl->url->port = port;
		if( fd>-1 ) return fd;
		mp_msg(MSGT_NETWORK,MSGL_V,"  ===> ASF/TCP failed\n");
		if( fd==-2 ) return -1;
	}

    //Is protocol http, http_proxy, or mms? 
    if (!strcasecmp(proto, "http_proxy") || !strcasecmp(proto, "http") ||
	!strcasecmp(proto, "mms") || !strcasecmp(proto, "mmshttp"))
    {
		mp_msg(MSGT_NETWORK,MSGL_V,"Trying ASF/HTTP...\n");
		fd = asf_http_streaming_start( stream, demuxer_type );
		stream->streaming_ctrl->url->port = port;
		if( fd>-1 ) return fd;
		mp_msg(MSGT_NETWORK,MSGL_V,"  ===> ASF/HTTP failed\n");
		if( fd==-2 ) return -1;
	}

    //everything failed
	return -1;
}

static int asf_streaming(ASF_stream_chunck_t *stream_chunck, int *drop_packet ) {
/*	
printf("ASF stream chunck size=%d\n", stream_chunck->size);
printf("length: %d\n", length );
printf("0x%02X\n", stream_chunck->type );
*/
	if( drop_packet!=NULL ) *drop_packet = 0;

	if( stream_chunck->size<8 ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_StreamChunkSize2Small, stream_chunck->size);
		return -1;
	}
	if( stream_chunck->size!=stream_chunck->size_confirm ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_SizeConfirmMismatch, stream_chunck->size, stream_chunck->size_confirm);
		return -1;
	}
/*	
	printf("  type: 0x%02X\n", stream_chunck->type );
	printf("  size: %d (0x%02X)\n", stream_chunck->size, stream_chunck->size );
	printf("  sequence_number: 0x%04X\n", stream_chunck->sequence_number );
	printf("  unknown: 0x%02X\n", stream_chunck->unknown );
	printf("  size_confirm: 0x%02X\n", stream_chunck->size_confirm );
*/
	switch(stream_chunck->type) {
		case ASF_STREAMING_CLEAR:	// $C	Clear ASF configuration
			mp_msg(MSGT_NETWORK,MSGL_V,"=====> Clearing ASF stream configuration!\n");
			if( drop_packet!=NULL ) *drop_packet = 1;
			return stream_chunck->size;
			break;
		case ASF_STREAMING_DATA:	// $D	Data follows
//			printf("=====> Data follows\n");
			break;
		case ASF_STREAMING_END_TRANS:	// $E	Transfer complete
			mp_msg(MSGT_NETWORK,MSGL_V,"=====> Transfer complete\n");
			if( drop_packet!=NULL ) *drop_packet = 1;
			return stream_chunck->size;
			break;
		case ASF_STREAMING_HEADER:	// $H	ASF header chunk follows
			mp_msg(MSGT_NETWORK,MSGL_V,"=====> ASF header chunk follows\n");
			break;
		default:
			mp_msg(MSGT_NETWORK,MSGL_V,"=====> Unknown stream type 0x%x\n", stream_chunck->type );
	}
	return stream_chunck->size+4;
}

extern int audio_id;
extern int video_id;

static void close_s(stream_t *stream) {
	close(stream->fd);
	stream->fd=-1;
}

static int max_idx(int s_count, int *s_rates, int bound) {
  int i, best = -1, rate = -1;
  for (i = 0; i < s_count; i++) {
    if (s_rates[i] > rate && s_rates[i] <= bound) {
      rate = s_rates[i];
      best = i;
    }
  }
  return best;
}

static int asf_streaming_parse_header(int fd, streaming_ctrl_t* streaming_ctrl) {
  ASF_stream_chunck_t chunk;
  asf_http_streaming_ctrl_t* asf_ctrl = streaming_ctrl->data;
  char* buffer=NULL, *chunk_buffer=NULL;
  int i,r,size,pos = 0;
  int start;
  int buffer_size = 0;
  int chunk_size2read = 0;
  int bw = streaming_ctrl->bandwidth;
  int *v_rates = NULL, *a_rates = NULL;
  int v_rate = 0, a_rate = 0, a_idx = -1, v_idx = -1;
  
  if(asf_ctrl == NULL) return -1;

	// The ASF header can be in several network chunks. For example if the content description
	// is big, the ASF header will be split in 2 network chunk.
	// So we need to retrieve all the chunk before starting to parse the header.
  do {
	  if (asf_read_wrapper(fd, &chunk, sizeof(ASF_stream_chunck_t), streaming_ctrl) <= 0)
	    return -1;
	  // Endian handling of the stream chunk
	  le2me_ASF_stream_chunck_t(&chunk);
	  size = asf_streaming( &chunk, &r) - sizeof(ASF_stream_chunck_t);
	  if(r) mp_msg(MSGT_NETWORK,MSGL_WARN,MSGTR_MPDEMUX_ASF_WarnDropHeader);
	  if(size < 0){
	    mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader);
		return -1;
	  }
	  if (chunk.type != ASF_STREAMING_HEADER) {
	    mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk);
	    return -1;
	  }
	  
	  // audit: do not overflow buffer_size
	  if (size > SIZE_MAX - buffer_size) return -1;
	  buffer = malloc(size+buffer_size);
	  if(buffer == NULL) {
	    mp_msg(MSGT_NETWORK,MSGL_FATAL,MSGTR_MPDEMUX_ASF_BufferMallocFailed,size+buffer_size);
	    return -1;
	  }
	  if( chunk_buffer!=NULL ) {
	  	memcpy( buffer, chunk_buffer, buffer_size );
		free( chunk_buffer );
	  }
	  chunk_buffer = buffer;
	  buffer += buffer_size;
	  buffer_size += size;
	  
	  if (asf_read_wrapper(fd, buffer, size, streaming_ctrl) <= 0)
	    return -1;

	  if( chunk_size2read==0 ) {
		ASF_header_t *asfh = (ASF_header_t *)buffer;
		if(size < (int)sizeof(ASF_header_t)) {
		    mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_ErrChunk2Small);
		    return -1;
		} else mp_msg(MSGT_NETWORK,MSGL_DBG2,"Got chunk\n");
		chunk_size2read = AV_RL64(&asfh->objh.size);
		mp_msg(MSGT_NETWORK,MSGL_DBG2,"Size 2 read=%d\n", chunk_size2read);
	  }
  } while( buffer_size<chunk_size2read);
  buffer = chunk_buffer;
  size = buffer_size;
	  
  start = sizeof(ASF_header_t);
  
  pos = find_asf_guid(buffer, asf_file_header_guid, start, size);
  if (pos >= 0) {
    ASF_file_header_t *fileh = (ASF_file_header_t *) &buffer[pos];
    pos += sizeof(ASF_file_header_t);
    if (pos > size) goto len_err_out;
/*
      if(fileh.packetsize != fileh.packetsize2) {
	printf("Error packetsize check don't match\n");
	return -1;
      }
*/
      asf_ctrl->packet_size = AV_RL32(&fileh->max_packet_size);
      // before playing. 
      // preroll: time in ms to bufferize before playing
      streaming_ctrl->prebuffer_size = (unsigned int)(((double)fileh->preroll/1000.0)*((double)fileh->max_bitrate/8.0));
  }

  pos = start;
  while ((pos = find_asf_guid(buffer, asf_stream_header_guid, pos, size)) >= 0)
  {
    ASF_stream_header_t *streamh = (ASF_stream_header_t *)&buffer[pos];
    pos += sizeof(ASF_stream_header_t);
    if (pos > size) goto len_err_out;
      switch(ASF_LOAD_GUID_PREFIX(streamh->type)) {
      case 0xF8699E40 : // audio stream
	if(asf_ctrl->audio_streams == NULL){
	  asf_ctrl->audio_streams = malloc(sizeof(int));
	  asf_ctrl->n_audio = 1;
	} else {
	  asf_ctrl->n_audio++;
	  asf_ctrl->audio_streams = realloc(asf_ctrl->audio_streams,
						     asf_ctrl->n_audio*sizeof(int));
	}
	asf_ctrl->audio_streams[asf_ctrl->n_audio-1] = AV_RL16(&streamh->stream_no);
	break;
      case 0xBC19EFC0 : // video stream
	if(asf_ctrl->video_streams == NULL){
	  asf_ctrl->video_streams = malloc(sizeof(int));
	  asf_ctrl->n_video = 1;
	} else {
	  asf_ctrl->n_video++;
	  asf_ctrl->video_streams = realloc(asf_ctrl->video_streams,
						     asf_ctrl->n_video*sizeof(int));
	}
	asf_ctrl->video_streams[asf_ctrl->n_video-1] = AV_RL16(&streamh->stream_no);
	break;
      }
  }

  // always allocate to avoid lots of ifs later
  v_rates = calloc(asf_ctrl->n_video, sizeof(int));
  a_rates = calloc(asf_ctrl->n_audio, sizeof(int));

  pos = find_asf_guid(buffer, asf_stream_group_guid, start, size);
  if (pos >= 0) {
    // stream bitrate properties object
	int stream_count;
	char *ptr = &buffer[pos];
	char *end = &buffer[size];
	
	mp_msg(MSGT_NETWORK, MSGL_V, "Stream bitrate properties object\n");
		if (ptr + 2 > end) goto len_err_out;
		stream_count = AV_RL16(ptr);
		ptr += 2;
		mp_msg(MSGT_NETWORK, MSGL_V, " stream count=[0x%x][%u]\n",
		        stream_count, stream_count );
		for( i=0 ; i<stream_count ; i++ ) {
			uint32_t rate;
			int id;
			int j;
			if (ptr + 6 > end) goto len_err_out;
			id = AV_RL16(ptr);
			ptr += 2;
			rate = AV_RL32(ptr);
			ptr += 4;
			mp_msg(MSGT_NETWORK, MSGL_V,
                                "  stream id=[0x%x][%u]\n", id, id);
			mp_msg(MSGT_NETWORK, MSGL_V,
			        "  max bitrate=[0x%x][%u]\n", rate, rate);
			for (j = 0; j < asf_ctrl->n_video; j++) {
			  if (id == asf_ctrl->video_streams[j]) {
			    mp_msg(MSGT_NETWORK, MSGL_V, "  is video stream\n");
			    v_rates[j] = rate;
			    break;
			  }
			}
			for (j = 0; j < asf_ctrl->n_audio; j++) {
			  if (id == asf_ctrl->audio_streams[j]) {
			    mp_msg(MSGT_NETWORK, MSGL_V, "  is audio stream\n");
			    a_rates[j] = rate;
			    break;
			  }
			}
		}
  }
  free(buffer);

  // automatic stream selection based on bandwidth
  if (bw == 0) bw = INT_MAX;
  mp_msg(MSGT_NETWORK, MSGL_V, "Max bandwidth set to %d\n", bw);

  if (asf_ctrl->n_audio) {
    // find lowest-bitrate audio stream
    a_rate = a_rates[0];
    a_idx = 0;
    for (i = 0; i < asf_ctrl->n_audio; i++) {
      if (a_rates[i] < a_rate) {
        a_rate = a_rates[i];
        a_idx = i;
      }
    }
    if (max_idx(asf_ctrl->n_video, v_rates, bw - a_rate) < 0) {
      // both audio and video are not possible, try video only next
      a_idx = -1;
      a_rate = 0;
    }
  }
  // find best video stream
  v_idx = max_idx(asf_ctrl->n_video, v_rates, bw - a_rate);
  if (v_idx >= 0)
    v_rate = v_rates[v_idx];

  // find best audio stream
  a_idx = max_idx(asf_ctrl->n_audio, a_rates, bw - v_rate);
    
  free(v_rates);
  free(a_rates);

  if (a_idx < 0 && v_idx < 0) {
    mp_msg(MSGT_NETWORK, MSGL_FATAL, MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay);
    return -1;
  }

  if (audio_id > 0)
    // a audio stream was forced
    asf_ctrl->audio_id = audio_id;
  else if (a_idx >= 0)
    asf_ctrl->audio_id = asf_ctrl->audio_streams[a_idx];
  else if (asf_ctrl->n_audio) {
    mp_msg(MSGT_NETWORK, MSGL_WARN, MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio);
    audio_id = -2;
  }

  if (video_id > 0)
    // a video stream was forced
    asf_ctrl->video_id = video_id;
  else if (v_idx >= 0)
    asf_ctrl->video_id = asf_ctrl->video_streams[v_idx];
  else if (asf_ctrl->n_video) {
    mp_msg(MSGT_NETWORK, MSGL_WARN, MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo);
    video_id = -2;
  }

  return 1;

len_err_out:
  mp_msg(MSGT_NETWORK, MSGL_FATAL, MSGTR_MPDEMUX_ASF_InvalidLenInHeader);
  if (buffer) free(buffer);
  if (v_rates) free(v_rates);
  if (a_rates) free(a_rates);
  return -1;
}

static int asf_http_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *streaming_ctrl ) {
  static ASF_stream_chunck_t chunk;
  int read,chunk_size = 0;
  static int rest = 0, drop_chunk = 0, waiting = 0;
  asf_http_streaming_ctrl_t *asf_http_ctrl = (asf_http_streaming_ctrl_t*)streaming_ctrl->data;

  while(1) {
    if (rest == 0 && waiting == 0) {
      if (asf_read_wrapper(fd, &chunk, sizeof(ASF_stream_chunck_t), streaming_ctrl) <= 0)
        return -1;
      
      // Endian handling of the stream chunk
      le2me_ASF_stream_chunck_t(&chunk);
      chunk_size = asf_streaming( &chunk, &drop_chunk );
      if(chunk_size < 0) {
	mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader);
	return -1;
      }
      chunk_size -= sizeof(ASF_stream_chunck_t);
	
      if(chunk.type != ASF_STREAMING_HEADER && (!drop_chunk)) {
	if (asf_http_ctrl->packet_size < chunk_size) {
	  mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket);
	  return -1;
	}
	waiting = asf_http_ctrl->packet_size;
      } else {
	waiting = chunk_size;
      }

    } else if (rest){
      chunk_size = rest;
      rest = 0;
    }

    read = 0;
    if ( waiting >= chunk_size) {
      if (chunk_size > size){
	rest = chunk_size - size;
	chunk_size = size;
      }
      if (asf_read_wrapper(fd, buffer, chunk_size, streaming_ctrl) <= 0)
        return -1;
      read = chunk_size;
      waiting -= read;
      if (drop_chunk) continue;
    }
    if (rest == 0 && waiting > 0 && size-read > 0) {
      int s = FFMIN(waiting,size-read);
      memset(buffer+read,0,s);
      waiting -= s;
      read += s;
    }
    break;
  }

  return read;
}

static int asf_http_streaming_seek( int fd, off_t pos, streaming_ctrl_t *streaming_ctrl ) {
	return -1;
	// to shut up gcc warning
	fd++;
	pos++;
	streaming_ctrl=NULL;
}

static int asf_header_check( HTTP_header_t *http_hdr ) {
	ASF_obj_header_t *objh;
	if( http_hdr==NULL ) return -1;
	if( http_hdr->body==NULL || http_hdr->body_size<sizeof(ASF_obj_header_t) ) return -1;

	objh = (ASF_obj_header_t*)http_hdr->body;
	if( ASF_LOAD_GUID_PREFIX(objh->guid)==0x75B22630 ) return 0;
	return -1;
}

static int asf_http_streaming_type(char *content_type, char *features, HTTP_header_t *http_hdr ) {
	if( content_type==NULL ) return ASF_Unknown_e;
	if( 	!strcasecmp(content_type, "application/octet-stream") ||
		!strcasecmp(content_type, "application/vnd.ms.wms-hdr.asfv1") ||        // New in Corona, first request
		!strcasecmp(content_type, "application/x-mms-framed") ||                // New in Corana, second request
		!strcasecmp(content_type, "video/x-ms-asf")) {               

		if( strstr(features, "broadcast") ) {
			mp_msg(MSGT_NETWORK,MSGL_V,"=====> ASF Live stream\n");
			return ASF_Live_e;
		} else {
			mp_msg(MSGT_NETWORK,MSGL_V,"=====> ASF Prerecorded\n");
			return ASF_Prerecorded_e;
		}
	} else {
		// Ok in a perfect world, web servers should be well configured
		// so we could used mime type to know the stream type,
		// but guess what? All of them are not well configured.
		// So we have to check for an asf header :(, but it works :p
		if( http_hdr->body_size>sizeof(ASF_obj_header_t) ) {
			if( asf_header_check( http_hdr )==0 ) {
				mp_msg(MSGT_NETWORK,MSGL_V,"=====> ASF Plain text\n");
				return ASF_PlainText_e;
			} else if( (!strcasecmp(content_type, "text/html")) ) {
				mp_msg(MSGT_NETWORK,MSGL_V,"=====> HTML, MPlayer is not a browser...yet!\n");
				return ASF_Unknown_e;
			} else {
				mp_msg(MSGT_NETWORK,MSGL_V,"=====> ASF Redirector\n");
				return ASF_Redirector_e;
			}
		} else {
			if(	(!strcasecmp(content_type, "audio/x-ms-wax")) ||
				(!strcasecmp(content_type, "audio/x-ms-wma")) ||
				(!strcasecmp(content_type, "video/x-ms-asf")) ||
				(!strcasecmp(content_type, "video/x-ms-afs")) ||
				(!strcasecmp(content_type, "video/x-ms-wmv")) ||
				(!strcasecmp(content_type, "video/x-ms-wma")) ) {
				mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_ASFRedirector);
				return ASF_Redirector_e;
			} else if( !strcasecmp(content_type, "text/plain") ) {
				mp_msg(MSGT_NETWORK,MSGL_V,"=====> ASF Plain text\n");
				return ASF_PlainText_e;
			} else {
				mp_msg(MSGT_NETWORK,MSGL_V,"=====> ASF unknown content-type: %s\n", content_type );
				return ASF_Unknown_e;
			}
		}
	}
	return ASF_Unknown_e;
}

static HTTP_header_t *asf_http_request(streaming_ctrl_t *streaming_ctrl) {
	HTTP_header_t *http_hdr;
	URL_t *url = NULL;
	URL_t *server_url = NULL;
	asf_http_streaming_ctrl_t *asf_http_ctrl;
	char str[250];
	char *ptr;
	int i, enable;

	int offset_hi=0, offset_lo=0, length=0;
	int asf_nb_stream=0, stream_id;

	// Sanity check
	if( streaming_ctrl==NULL ) return NULL;
	url = streaming_ctrl->url;
	asf_http_ctrl = (asf_http_streaming_ctrl_t*)streaming_ctrl->data;
	if( url==NULL || asf_http_ctrl==NULL ) return NULL;

	// Common header for all requests.
	http_hdr = http_new_header();
	http_set_field( http_hdr, "Accept: */*" );
	http_set_field( http_hdr, "User-Agent: NSPlayer/4.1.0.3856" );
	http_add_basic_authentication( http_hdr, url->username, url->password );

	// Check if we are using a proxy
	if( !strcasecmp( url->protocol, "http_proxy" ) ) {
		server_url = url_new( (url->file)+1 );
		if( server_url==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_InvalidProxyURL);
			http_free( http_hdr );
			return NULL;
		}
		http_set_uri( http_hdr, server_url->url );
		sprintf( str, "Host: %.220s:%d", server_url->hostname, server_url->port );
		url_free( server_url );
	} else {
		http_set_uri( http_hdr, url->file );
		sprintf( str, "Host: %.220s:%d", url->hostname, url->port );
	}
	
	http_set_field( http_hdr, str );
	http_set_field( http_hdr, "Pragma: xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}" );
	sprintf(str, 
		"Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=%u:%u,request-context=%d,max-duration=%u",
		offset_hi, offset_lo, asf_http_ctrl->request, length );
	http_set_field( http_hdr, str );

	switch( asf_http_ctrl->streaming_type ) {
		case ASF_Live_e:
		case ASF_Prerecorded_e:
			http_set_field( http_hdr, "Pragma: xPlayStrm=1" );
			ptr = str;
			ptr += sprintf( ptr, "Pragma: stream-switch-entry=");
			if(asf_http_ctrl->n_audio > 0) {
				for( i=0; i<asf_http_ctrl->n_audio ; i++ ) {
					stream_id = asf_http_ctrl->audio_streams[i];
					if(stream_id == asf_http_ctrl->audio_id) {
						enable = 0;
					} else {
						enable = 2;
						continue;
					}
					asf_nb_stream++;
					ptr += sprintf(ptr, "ffff:%x:%d ", stream_id, enable);
				}
			}
			if(asf_http_ctrl->n_video > 0) {
				for( i=0; i<asf_http_ctrl->n_video ; i++ ) {
					stream_id = asf_http_ctrl->video_streams[i];
					if(stream_id == asf_http_ctrl->video_id) {
						enable = 0;
					} else {
						enable = 2;
						continue;
					}
					asf_nb_stream++;
					ptr += sprintf(ptr, "ffff:%x:%d ", stream_id, enable);
				}
			}
			http_set_field( http_hdr, str );
			sprintf( str, "Pragma: stream-switch-count=%d", asf_nb_stream );
			http_set_field( http_hdr, str );
			break;
		case ASF_Redirector_e:
			break;
		case ASF_Unknown_e:
			// First request goes here.
			break;
		default:
			mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_UnknownASFStreamType);
	}

	http_set_field( http_hdr, "Connection: Close" );
	http_build_request( http_hdr );

	return http_hdr;
}

static int asf_http_parse_response(asf_http_streaming_ctrl_t *asf_http_ctrl, HTTP_header_t *http_hdr ) {
	char *content_type, *pragma;
	char features[64] = "\0";
	size_t len;
	if( http_response_parse(http_hdr)<0 ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse);
		return -1;
	}
	switch( http_hdr->status_code ) {
		case 200:
			break;
		case 401: // Authentication required
			return ASF_Authenticate_e;
		default:
			mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_ServerReturn, http_hdr->status_code, http_hdr->reason_phrase);
			return -1;
	}

	content_type = http_get_field( http_hdr, "Content-Type");
//printf("Content-Type: [%s]\n", content_type);

	pragma = http_get_field( http_hdr, "Pragma");
	while( pragma!=NULL ) {
		char *comma_ptr=NULL;
		char *end;
//printf("Pragma: [%s]\n", pragma );
		// The pragma line can get severals attributes 
		// separeted with a comma ','.
		do {
			if( !strncasecmp( pragma, "features=", 9) ) {
				pragma += 9;
				end = strstr( pragma, "," );
				if( end==NULL ) {
				  len = strlen(pragma);
				} else { 
				  len = (unsigned int)(end-pragma);
				}
				if(len > sizeof(features) - 1) {
				  mp_msg(MSGT_NETWORK,MSGL_WARN,MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma,pragma,len,sizeof(features) - 1);
				  len = sizeof(features) - 1;
				}
				strncpy( features, pragma, len );
				features[len]='\0';
				break;
			}
			comma_ptr = strstr( pragma, "," );
			if( comma_ptr!=NULL ) {
				pragma = comma_ptr+1;
				if( pragma[0]==' ' ) pragma++;
			}
		} while( comma_ptr!=NULL );
		pragma = http_get_next_field( http_hdr );
	}
	asf_http_ctrl->streaming_type = asf_http_streaming_type( content_type, features, http_hdr );
	return 0;
}

static int asf_http_streaming_start( stream_t *stream, int *demuxer_type ) {
	HTTP_header_t *http_hdr=NULL;
	URL_t *url = stream->streaming_ctrl->url;
	asf_http_streaming_ctrl_t *asf_http_ctrl;
	char buffer[BUFFER_SIZE];
	int i, ret;
	int fd = stream->fd;
	int done;
	int auth_retry = 0;

	asf_http_ctrl = malloc(sizeof(asf_http_streaming_ctrl_t));
	if( asf_http_ctrl==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,MSGTR_MemAllocFailed);
		return -1;
	}
	asf_http_ctrl->streaming_type = ASF_Unknown_e;
	asf_http_ctrl->request = 1;
	asf_http_ctrl->audio_streams = asf_http_ctrl->video_streams = NULL;
	asf_http_ctrl->n_audio = asf_http_ctrl->n_video = 0;
	stream->streaming_ctrl->data = (void*)asf_http_ctrl;

	do {
		done = 1;
		if( fd>0 ) closesocket( fd );

		if( !strcasecmp( url->protocol, "http_proxy" ) ) {
			if( url->port==0 ) url->port = 8080;
		} else {
			if( url->port==0 ) url->port = 80;
		}
		fd = connect2Server( url->hostname, url->port, 1);
		if( fd<0 ) return fd;

		http_hdr = asf_http_request( stream->streaming_ctrl );
		mp_msg(MSGT_NETWORK,MSGL_DBG2,"Request [%s]\n", http_hdr->buffer );
		for(i=0; i < (int)http_hdr->buffer_size ; ) {
			int r = send( fd, http_hdr->buffer+i, http_hdr->buffer_size-i, 0 );
			if(r <0) {
				mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_SocketWriteError,strerror(errno));
				goto err_out;
			}
			i += r;
		}       
		http_free( http_hdr );
		http_hdr = http_new_header();
		do {
			i = recv( fd, buffer, BUFFER_SIZE, 0 );
//printf("read: %d\n", i );
			if( i<=0 ) {
				perror("read");
				goto err_out;
			}
			http_response_append( http_hdr, buffer, i );
		} while( !http_is_header_entire( http_hdr ) );
		if( mp_msg_test(MSGT_NETWORK,MSGL_V) ) {
			http_hdr->buffer[http_hdr->buffer_size]='\0';
			mp_msg(MSGT_NETWORK,MSGL_DBG2,"Response [%s]\n", http_hdr->buffer );
		}
		ret = asf_http_parse_response(asf_http_ctrl, http_hdr);
		if( ret<0 ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_HeaderParseFailed);
			goto err_out;
		}
		switch( asf_http_ctrl->streaming_type ) {
			case ASF_Live_e:
			case ASF_Prerecorded_e:
			case ASF_PlainText_e:
				if( http_hdr->body_size>0 ) {
					if( streaming_bufferize( stream->streaming_ctrl, http_hdr->body, http_hdr->body_size )<0 ) {
						goto err_out;
					}
				}
				if( asf_http_ctrl->request==1 ) {
					if( asf_http_ctrl->streaming_type!=ASF_PlainText_e ) {
						// First request, we only got the ASF header.
						ret = asf_streaming_parse_header(fd,stream->streaming_ctrl);
						if(ret < 0) goto err_out;
						if(asf_http_ctrl->n_audio == 0 && asf_http_ctrl->n_video == 0) {
							mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_NoStreamFound);
							goto err_out;
						}
						asf_http_ctrl->request++;
						done = 0;
					} else {
						done = 1;
					}
				}
				break;
			case ASF_Redirector_e:
				if( http_hdr->body_size>0 ) {
					if( streaming_bufferize( stream->streaming_ctrl, http_hdr->body, http_hdr->body_size )<0 ) {
						goto err_out;
					}
				}
				*demuxer_type = DEMUXER_TYPE_PLAYLIST;
				done = 1;
				break;
			case ASF_Authenticate_e:
				if( http_authenticate( http_hdr, url, &auth_retry)<0 ) return -1;
				asf_http_ctrl->streaming_type = ASF_Unknown_e;
				done = 0;
				break;
			case ASF_Unknown_e:
			default:
				mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_ASF_UnknownASFStreamingType);
				goto err_out;
		}
	// Check if we got a redirect.	
	} while(!done);

	stream->fd = fd;
	if( asf_http_ctrl->streaming_type==ASF_PlainText_e || asf_http_ctrl->streaming_type==ASF_Redirector_e ) {
		stream->streaming_ctrl->streaming_read = nop_streaming_read;
		stream->streaming_ctrl->streaming_seek = nop_streaming_seek;
	} else {
		stream->streaming_ctrl->streaming_read = asf_http_streaming_read;
		stream->streaming_ctrl->streaming_seek = asf_http_streaming_seek;
		stream->streaming_ctrl->buffering = 1;
	}
	stream->streaming_ctrl->status = streaming_playing_e;
	stream->close = close_s;

	http_free( http_hdr );
	return 0;

err_out:
	if (fd > 0)
		closesocket(fd);
	stream->fd = -1;
	http_free(http_hdr);
	return -1;
}

static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
	URL_t *url;

	stream->streaming_ctrl = streaming_ctrl_new();
	if( stream->streaming_ctrl==NULL ) {
		return STREAM_ERROR;
	}
	stream->streaming_ctrl->bandwidth = network_bandwidth;
	url = url_new(stream->url);
	stream->streaming_ctrl->url = check4proxies(url);
	url_free(url);
	
	mp_msg(MSGT_OPEN, MSGL_INFO, MSGTR_MPDEMUX_ASF_InfoStreamASFURL, stream->url);
	if((!strncmp(stream->url, "http", 4)) && (*file_format!=DEMUXER_TYPE_ASF && *file_format!=DEMUXER_TYPE_UNKNOWN)) {
		streaming_ctrl_free(stream->streaming_ctrl);
		stream->streaming_ctrl = NULL;
		return STREAM_UNSUPPORTED;
	}

	if(asf_streaming_start(stream, file_format) < 0) {
		mp_msg(MSGT_OPEN, MSGL_ERR, MSGTR_MPDEMUX_ASF_StreamingFailed);
		streaming_ctrl_free(stream->streaming_ctrl);
		stream->streaming_ctrl = NULL;
		return STREAM_UNSUPPORTED;
	}
	
	if (*file_format != DEMUXER_TYPE_PLAYLIST)
		*file_format = DEMUXER_TYPE_ASF;
	stream->type = STREAMTYPE_STREAM;
	fixup_network_stream_cache(stream);
	return STREAM_OK;
}

const stream_info_t stream_info_asf = {
  "mms and mms over http streaming",
  "null",
  "Bertrand, Reimar Doeffinger, Albeu",
  "originally based on work by Majormms (is that code still there?)",
  open_s,
  {"mms", "mmsu", "mmst", "http", "http_proxy", "mmshttp", NULL},
  NULL,
  0 // Urls are an option string
};

