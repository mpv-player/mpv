#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asf.h"
#include "url.h"
#include "http.h"


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

static ASF_StreamType_e stream_type = ASF_Unknown_e;



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

void 
asf_stream_type(char *content_type, char *features) {
	stream_type = ASF_Unknown_e;
	if( !strcasecmp(content_type, "application/octet-stream") ) {
		if( strstr(features, "broadcast")) {
			printf("=====> Live stream\n");
			stream_type = ASF_Live_e;
		} else {
			printf("=====> Prerecorded\n");
			stream_type = ASF_Prerecorded_e;
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
			stream_type = ASF_Redirector_e;
		} else {
			printf("=====> unknown content-type: %s\n", content_type );
			stream_type = ASF_Unknown_e;
		}
	}
}

//void asf_http_request(stream_t *stream, URL_t *url) {
void 
asf_http_request(URL_t *url) {
	HTTP_header_t *http_hdr;
	char str[250];
	char *request;
//	int size;

	int offset_hi=0, offset_lo=0, req_nb=1, length=0;
	int asf_nb_stream=1; 

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

	switch( stream_type ) {
		case ASF_Live_e:
		case ASF_Prerecorded_e:
			http_set_field( http_hdr, "Pragma: xPlayStrm=1" );
			sprintf( str, "Pragma: stream-switch-count=%d", asf_nb_stream );
			http_set_field( http_hdr, str );
			http_set_field( http_hdr, "Pragma: stream-switch-entry=ffff:1:0" );
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
	request = http_get_request( http_hdr );

printf("%s\n", request );

}

int
asf_http_parse_response( char *response, int length ) {
	HTTP_header_t *http_hdr;
	char *content_type, *pragma;
	char features[64] = "\0";
	int len;

	http_hdr = http_new_response( response, length );
	
	if( http_hdr->status_code!=200 ) {
		printf("Server return %d:%s\n", http_hdr->status_code, http_hdr->reason_phrase);
		return -1;
	}

	content_type = http_get_field( http_hdr, "Content-Type");

	pragma = http_get_field( http_hdr, "Pragma");
	do {
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
	} while( pragma!=NULL );

	asf_stream_type( content_type, features );

	return 0;
}

#ifdef STREAMING_TEST
int main() {
	URL_t *url = set_url("http://toto.com:12/coucou");
	asf_http_request( url );
	asf_http_parse_response( temp_response, strlen(temp_response) );
	asf_http_request( url );
	return 0;
}
#endif
