#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "url.h"
#include "http.h"
#include "asf.h"
#include "network.h"

#include "stream.h"
#include "demuxer.h"

extern demuxer_t *demuxer;

static ASF_StreamType_e streaming_type = ASF_Unknown_e;

int
asf_http_streaming_read( streaming_ctrl_t *streaming_ctrl ) {
	char *buffer;
	int drop_packet;
	int ret;
printf("asf_streaming_read\n");
	ret = asf_streaming( streaming_ctrl->buffer->buffer, streaming_ctrl->buffer->length, &drop_packet );
printf("ret: %d\n", ret);
	if( ret<0 ) return -1;
	if( ret>streaming_ctrl->buffer->length ) return 0;
	buffer = (char*)malloc(ret);
	if( buffer==NULL ) {
		printf("Memory allocation failed\n");
		return -1;
	}
printf("buffer length: %d\n", streaming_ctrl->buffer->length );
	net_fifo_pop( streaming_ctrl->buffer, buffer, ret );
printf("  pop: 0x%02X\n", *((unsigned int*)buffer) );
printf("buffer length: %d\n", streaming_ctrl->buffer->length );
printf("0x%02X\n", *((unsigned int*)(buffer+sizeof(ASF_stream_chunck_t))) );
	if( !drop_packet ) {
		write( streaming_ctrl->fd_pipe_in, buffer+sizeof(ASF_stream_chunck_t), ret-sizeof(ASF_stream_chunck_t) );
	}
	free( buffer );
	return ret;
}

int 
asf_streaming(char *data, int length, int *drop_packet ) {
	ASF_stream_chunck_t *stream_chunck=(ASF_stream_chunck_t*)data;
	printf("ASF stream chunck size=%d\n", stream_chunck->size);
printf("length: %d\n", length );
printf("0x%02X\n", stream_chunck->type );

	if( drop_packet!=NULL ) *drop_packet = 0;
	if( data==NULL || length<=0 ) return -1;

	if( stream_chunck->size<8 ) {
		printf("Ahhhh, stream_chunck size is too small: %d\n", stream_chunck->size);
		return -1;
	}
	if( stream_chunck->size!=stream_chunck->size_confirm ) {
		printf("size_confirm mismatch!: %d %d\n", stream_chunck->size, stream_chunck->size_confirm);
		return -1;
	}

	printf("  type: 0x%02X\n", stream_chunck->type );
	printf("  size: %d (0x%02X)\n", stream_chunck->size, stream_chunck->size );
	printf("  sequence_number: 0x%04X\n", stream_chunck->sequence_number );
	printf("  unknown: 0x%02X\n", stream_chunck->unknown );
	printf("  size_confirm: 0x%02X\n", stream_chunck->size_confirm );


	switch(stream_chunck->type) {
		case 0x4324:	// Clear ASF configuration
			printf("=====> Clearing ASF stream configuration!\n");
			if( drop_packet!=NULL ) *drop_packet = 1;
			return stream_chunck->size;
			break;
		case 0x4424:    // Data follows
			printf("=====> Data follows\n");
			break;
		case 0x4524:    // Transfer complete
			printf("=====> Transfer complete\n");
			if( drop_packet!=NULL ) *drop_packet = 1;
			return stream_chunck->size;
			break;
		case 0x4824:    // ASF header chunk follows
			printf("=====> ASF header chunk follows\n");
			break;
		default:
			printf("=====> Unknown stream type 0x%x\n", stream_chunck->type );
	}
	return stream_chunck->size+4;
}

int
asf_http_streaming_type(char *content_type, char *features) {
	if( content_type==NULL ) return ASF_Unknown_e;
	if( !strcasecmp(content_type, "application/octet-stream") ) {
		if( features==NULL ) {
			printf("=====> ASF Prerecorded\n");
			return ASF_Prerecorded_e;
		} else if( strstr(features, "broadcast")) {
			printf("=====> ASF Live stream\n");
			return ASF_Live_e;
		} else {
			printf("=====> ASF Prerecorded\n");
			return ASF_Prerecorded_e;
		}
	} else {
		if(	(!strcasecmp(content_type, "audio/x-ms-wax")) ||
			(!strcasecmp(content_type, "audio/x-ms-wma")) ||
			(!strcasecmp(content_type, "video/x-ms-asf")) ||
			(!strcasecmp(content_type, "video/x-ms-afs")) ||
			(!strcasecmp(content_type, "video/x-ms-wvx")) ||
			(!strcasecmp(content_type, "video/x-ms-wmv")) ||
			(!strcasecmp(content_type, "video/x-ms-wma")) ) {
			printf("=====> ASF Redirector\n");
			return ASF_Redirector_e;
		} else {
			printf("=====> ASF unknown content-type: %s\n", content_type );
			return ASF_Unknown_e;
		}
	}
	return ASF_Unknown_e;
}

HTTP_header_t *
asf_http_request(URL_t *url) {
	HTTP_header_t *http_hdr;
	char str[250];
	char *ptr;
	char *request;
	int i;

	int offset_hi=0, offset_lo=0, req_nb=1, length=0;
	int asf_nb_stream;

	// Common header for all requests.
	http_hdr = http_new_header();
	http_set_uri( http_hdr, url->file );
	http_set_field( http_hdr, "Accept: */*" );
	http_set_field( http_hdr, "User-Agent: NSPlayer/4.1.0.3856" );
        sprintf( str, "Host: %s:%d", url->hostname, url->port );
	http_set_field( http_hdr, str );
	http_set_field( http_hdr, "Pragma: xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}" );
	sprintf(str, 
		"Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=%u:%u,request-context=%d,max-duration=%u",
		offset_hi, offset_lo, req_nb, length );
	http_set_field( http_hdr, str );

	switch( streaming_type ) {
		case ASF_Live_e:
		case ASF_Prerecorded_e:
			http_set_field( http_hdr, "Pragma: xPlayStrm=1" );
			ptr = str;
			ptr += sprintf( ptr, "Pragma: stream-switch-entry=");
			for( i=0, asf_nb_stream=0 ; i<256 ; i++ ) {
				// FIXME START
				if( demuxer==NULL ) {
					ptr += sprintf( ptr, " ffff:1:0" );
					asf_nb_stream = 1;
					break;
				}
				// FIXME END
				if( demuxer->a_streams[i] ) {
					ptr += sprintf( ptr, " ffff:%d:0", i );
					asf_nb_stream++;
				}
				if( demuxer->v_streams[i] ) {
					ptr += sprintf( ptr, " ffff:%d:0", i );
					asf_nb_stream++;
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
			printf("Unknown asf stream type\n");
	}

	http_set_field( http_hdr, "Connection: Close" );
	http_build_request( http_hdr );

	return http_hdr;
}

int
asf_http_parse_response( HTTP_header_t *http_hdr ) {
	char *content_type, *pragma;
	char features[64] = "\0";
	int len;
	if( http_response_parse(http_hdr)<0 ) {
		printf("Failed to parse HTTP response\n");
		return -1;
	}
	if( http_hdr->status_code!=200 ) {
		printf("Server return %d:%s\n", http_hdr->status_code, http_hdr->reason_phrase);
		return -1;
	}

	content_type = http_get_field( http_hdr, "Content-Type");

	pragma = http_get_field( http_hdr, "Pragma");
	while( pragma!=NULL ) {
		char *comma_ptr=NULL;
		char *end;
		// The pragma line can get severals attributes 
		// separeted with a comma ','.
		do {
			if( !strncasecmp( pragma, "features=", 9) ) {
				pragma += 9;
				end = strstr( pragma, "," );
				if( end==NULL ) {
					len = strlen(pragma);
				}
				len = MIN(end-pragma,sizeof(features));
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

	streaming_type = asf_http_streaming_type( content_type, features );

	if( http_hdr->body_size>0 ) {
		asf_streaming( http_hdr->body, http_hdr->body_size, NULL);
	}

	return 0;
}

URL_t *
asf_http_ASX_redirect( HTTP_header_t *http_hdr ) {
	URL_t *url_redirect=NULL;
	printf("=========>> ASX parser not yet implemented <<==========\n");

	printf("ASX=[%s]\n", http_hdr->body );

	return url_redirect;
}

int
asf_http_streaming_start( streaming_ctrl_t *streaming_ctrl ) {
	HTTP_header_t *http_hdr=NULL;
	URL_t *url_next=NULL;
	URL_t *url = *(streaming_ctrl->url);
	char buffer[BUFFER_SIZE];
	int i;
	int fd = streaming_ctrl->fd_net;
	int done=1;

streaming_type = ASF_Live_e;
	do {
		if( fd>0 ) close( fd );

		fd = connect2Server( url->hostname, url->port );
		if( fd<0 ) return -1;

		http_hdr = asf_http_request( url );
printf("[%s]\n", http_hdr->buffer );
		write( fd, http_hdr->buffer, http_hdr->buffer_size );
//		http_free( http_hdr );

		http_hdr = http_new_header();
		do {
			i = readFromServer( fd, buffer, BUFFER_SIZE );
printf("read: %d\n", i );
			if( i<0 ) {
				perror("read");
				http_free( http_hdr );
				return -1;
			}
			http_response_append( http_hdr, buffer, i );
		} while( !http_is_header_entired( http_hdr ) );
//http_hdr->buffer[http_hdr->buffer_len]='\0';
//printf("[%s]\n", http_hdr->buffer );
		if( asf_http_parse_response(http_hdr)<0 ) {
			printf("Failed to parse header\n");
			http_free( http_hdr );
			return -1;
		}
		switch( streaming_type ) {
			case ASF_Live_e:
			case ASF_Prerecorded_e:
				if( http_hdr->body_size>0 ) {
printf("--- 0x%02X\n", streaming_ctrl->buffer );
					net_fifo_push( streaming_ctrl->buffer, http_hdr->body, http_hdr->body_size );
				} else {
					ASF_stream_chunck_t *ptr;
					int ret;
					i = readFromServer( fd, buffer, sizeof(ASF_stream_chunck_t) );
printf("read: %d\n", i );
					ret = asf_streaming( buffer, i, NULL );
					net_fifo_push( streaming_ctrl->buffer, buffer, i );
					ptr = (ASF_stream_chunck_t*)buffer;
					if( ret==ptr->size ) {
					}
				}
//				done = 0;
				break;
			case ASF_Redirector_e:
				url_next = asf_http_ASX_redirect( http_hdr );
				if( url_next==NULL ) {
					printf("Failed to parse ASX file\n");
					close(fd);
					http_free( http_hdr );
					return -1;
				}
				if( url_next->port==0 ) url_next->port=80;
				url_free( url );
				url = url_next;
				*(streaming_ctrl->url) = url_next;
				url_next = NULL;
				break;
			case ASF_Unknown_e:
			default:
				printf("Unknown ASF streaming type\n");
				close(fd);
				http_free( http_hdr );
				return -1;
		}

		// Check if we got a redirect.	
	} while(!done);

	streaming_ctrl->fd_net = fd;
	streaming_ctrl->streaming_read = asf_http_streaming_read;
        streaming_ctrl->prebuffer_size = 10000;
	streaming_ctrl->buffering = 1;
	streaming_ctrl->status = streaming_playing_e;

	http_free( http_hdr );
	return fd;
}

