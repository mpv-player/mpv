#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asf.h"
#include "url.h"
#include "http.h"
#include "network.h"

#define BUFFER_SIZE	2048

const char *temp_response = 
	"HTTP/1.0 200 OK\r\n"
	"Date: Tue, 20 Mar 2001 11:40:35 GMT\r\n"
	"Content-Type: application/octet-stream\r\n"
	"Server: Cougar 4.1.0.3920\r\n"
	"Cache-Control: no-cache\r\n"
	"Pragma: no-cache, client-id=290092, features=\"broadcast\"\r\n"
/*	"Pragma: no-cache\r\n"
	"Pragma: client-id=290092\r\n"
	"Pragma: features=\"broadcast\"\r\n"
*/	"\r\n";

static ASF_StreamType_e streaming_type = ASF_Unknown_e;

void 
asf_streaming(char *data, int length) {
	ASF_stream_chunck_t *stream_chunck=(ASF_stream_chunck_t*)data;
	printf("ASF stream chunck size=%d\n", stream_chunck->size);

	if( stream_chunck->size<8 ) {
		printf("Ahhhh, stream_chunck size is too small: %d\n", stream_chunck->size);
		return;
	}
	if( stream_chunck->size!=stream_chunck->size_confirm ) {
		printf("size_confirm mismatch!: %d %d\n", stream_chunck->size, stream_chunck->size_confirm);
		return;
	}

	switch(stream_chunck->type) {
		case 0x4324:	// Clear ASF configuration
			printf("=====> Clearing ASF stream configuration!\n");
			break;
		case 0x4424:    // Data follows
			printf("=====> Data follows\n");
			break;
		case 0x4524:    // Transfer complete
			printf("=====> Transfer complete\n");
			break;
		case 0x4824:    // ASF header chunk follows
			printf("=====> ASF header chunk follows\n");
			break;
		default:
			printf("=====> Unknown stream type 0x%x\n", stream_chunck->type );
	}
}

int
asf_http_streaming_type(char *content_type, char *features) {
	if( content_type==NULL ) return ASF_Unknown_e;
	if( !strcasecmp(content_type, "application/octet-stream") ) {
		if( features==NULL ) {
			printf("=====> Prerecorded\n");
			return ASF_Prerecorded_e;
		} else if( strstr(features, "broadcast")) {
			printf("=====> Live stream\n");
			return ASF_Live_e;
		} else {
			printf("=====> Prerecorded\n");
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
			printf("=====> Redirector\n");
			return ASF_Redirector_e;
		} else {
			printf("=====> unknown content-type: %s\n", content_type );
			return ASF_Unknown_e;
		}
	}
	return ASF_Unknown_e;
}

//void asf_http_request(stream_t *stream, URL_t *url) {
HTTP_header_t *
asf_http_request(URL_t *url) {
	HTTP_header_t *http_hdr;
	char str[250];
	char *request;

	int offset_hi=0, offset_lo=0, req_nb=1, length=0;
	int asf_nb_stream=1; 	// FIXME

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
			sprintf( str, "Pragma: stream-switch-count=%d", asf_nb_stream );
			http_set_field( http_hdr, str );
			http_set_field( http_hdr, "Pragma: stream-switch-entry=ffff:1:0" );	// FIXME
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
		asf_streaming( http_hdr->body, http_hdr->body_size );
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
asf_http_streaming_start( URL_t **url_ref ) {
	HTTP_header_t *http_hdr=NULL;
	URL_t *url_next=NULL;
	URL_t *url=*url_ref;
	char buffer[BUFFER_SIZE];
	int i;
	int fd=-1;
	int done=1;
	do {
		if( fd>0 ) close( fd );
		fd = connect2Server( url->hostname, url->port );
		if( fd<0 ) return -1;

		http_hdr = asf_http_request( url );
//printf("[%s]\n", http_hdr->buffer );
		write( fd, http_hdr->buffer, http_hdr->buffer_size );
		http_free( http_hdr );

		http_hdr = http_new_header();
		do {
			i = read( fd, buffer, BUFFER_SIZE );
printf("read: %d\n", i );
			http_response_append( http_hdr, buffer, i );
		} while( !http_is_header_entired( http_hdr ) );
//http_hdr->buffer[http_hdr->buffer_len]='\0';
//printf("[%s]\n", http_hdr->buffer );
		if( asf_http_parse_response(http_hdr)<0 ) {
			printf("Failed to parse header\n");
			return -1;
		}

		switch(streaming_type) {
			case ASF_Live_e:
			case ASF_Prerecorded_e:
				if( http_hdr->body_size==0 ) {
					i = read( fd, buffer, BUFFER_SIZE );
printf("read: %d\n", i );
					asf_streaming( buffer, i );
				}
				break;
			case ASF_Redirector_e:
				url_next = asf_http_ASX_redirect( http_hdr );
				if( url_next==NULL ) {
					printf("Failed to parse ASX file\n");
					close(fd);
					return -1;
				}
				if( url_next->port==0 ) url_next->port=80;
				url_free( url );
				url = url_next;
				*url_ref = url_next;
				url_next = NULL;
				break;
			case ASF_Unknown_e:
			default:
				printf("Unknown ASF streaming type\n");
				close(fd);
				return -1;
		}

		// Check if we got a redirect.	
	} while(!done);

	return fd;
}

