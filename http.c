/* 
 * HTTP Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"

HTTP_header_t *
http_new_header() {
	HTTP_header_t *http_hdr;

	http_hdr = (HTTP_header_t*)malloc(sizeof(HTTP_header_t));
	if( http_hdr==NULL ) return NULL;
	memset( http_hdr, 0, sizeof(HTTP_header_t) );

	return http_hdr;
}

void
http_free( HTTP_header_t *http_hdr ) {
	int i;
	if( http_hdr==NULL ) return;
	if( http_hdr->protocol!=NULL ) free( http_hdr->protocol );
	if( http_hdr->uri!=NULL ) free( http_hdr->uri );
	if( http_hdr->reason_phrase!=NULL ) free( http_hdr->reason_phrase );
	if( http_hdr->body!=NULL ) free( http_hdr->body );
	if( http_hdr->field_search!=NULL ) free( http_hdr->field_search );
	if( http_hdr->method!=NULL ) free( http_hdr->method );
	if( http_hdr->buffer!=NULL ) free( http_hdr->buffer );
	for( i=0 ; i<http_hdr->field_nb ; i++ ) 
		if( http_hdr->fields[i]!=NULL ) free( http_hdr->fields[i] );
	free( http_hdr );
}

int
http_response_append( HTTP_header_t *http_hdr, char *response, int length ) {
	char *ptr = NULL;
	if( http_hdr==NULL || response==NULL ) return -1;
	ptr = (char*)malloc( http_hdr->buffer_size+length );
	if( ptr==NULL ) {
		printf("Memory allocation failed\n");
		return -1;
	}
	if( http_hdr->buffer_size==0 ) {
		// Buffer empty, copy response into it.
		memcpy( ptr, response, length );
		http_hdr->buffer_size = length;
	} else {
		// Buffer not empty, grow buffer, copy and append the response.
		memcpy( ptr, http_hdr->buffer, http_hdr->buffer_size );
		free( http_hdr->buffer );
		memcpy( ptr+http_hdr->buffer_size, response, length );
		http_hdr->buffer_size += length;
	}
	http_hdr->buffer = ptr;
	return http_hdr->buffer_size;
}

int
http_is_header_entired( HTTP_header_t *http_hdr ) {
	if( http_hdr==NULL ) return -1;

	if( strstr(http_hdr->buffer, "\r\n\r\n")==NULL ) return 0;
	else return 1;
}

int
http_response_parse( HTTP_header_t *http_hdr ) {
	char *hdr_ptr, *ptr;
	char *field=NULL;
	int pos_hdr_sep, len;
	if( http_hdr==NULL ) return -1;
	if( http_hdr->is_parsed ) return 0;

	// Get the protocol
	hdr_ptr = strstr( http_hdr->buffer, " " );
	if( hdr_ptr==NULL ) {
		printf("Malformed answer. No space separator found.\n");
		return -1;
	}
	len = hdr_ptr-http_hdr->buffer;
	http_hdr->protocol = (char*)malloc(len+1);
	if( http_hdr->protocol==NULL ) {
		printf("Memory allocation failed\n");
		return -1;
	}
	strncpy( http_hdr->protocol, http_hdr->buffer, len );
	http_hdr->protocol[len]='\0';
	if( !strncasecmp( http_hdr->protocol, "HTTP", 4) ) {
		if( sscanf( http_hdr->protocol+5,"1.%d", &(http_hdr->http_minor_version) )!=1 ) {
			printf("Malformed answer. Unable to get HTTP minor version.\n");
			return -1;
		}
	}

	// Get the status code
	if( sscanf( ++hdr_ptr, "%d", &(http_hdr->status_code) )!=1 ) {
		printf("Malformed answer. Unable to get status code.\n");
		return -1;
	}
	hdr_ptr += 4;

	// Get the reason phrase
	ptr = strstr( hdr_ptr, "\r\n" );
	if( hdr_ptr==NULL ) {
		printf("Malformed answer. Unable to get the reason phrase.\n");
		return -1;
	}
	len = ptr-hdr_ptr;
	http_hdr->reason_phrase = (char*)malloc(len+1);
	if( http_hdr->reason_phrase==NULL ) {
		printf("Memory allocation failed\n");
		return -1;
	}
	strncpy( http_hdr->reason_phrase, hdr_ptr, len );
	http_hdr->reason_phrase[len]='\0';

	// Set the position of the header separator: \r\n\r\n
	ptr = strstr( http_hdr->buffer, "\r\n\r\n" );
	if( ptr==NULL ) {
		printf("Header may be incomplete. No CRLF CRLF found.\n");
		return -1;
	}
	pos_hdr_sep = ptr-http_hdr->buffer;

	hdr_ptr = strstr( http_hdr->buffer, "\r\n" )+2;
	do {
		ptr = strstr( hdr_ptr, "\r\n");
		if( ptr==NULL ) {
			printf("No CRLF found\n");
			return -1;
		}
		len = ptr-hdr_ptr;
		field = (char*)realloc(field, len+1);
		if( field==NULL ) {
			printf("Memory allocation failed\n");
			return -1;
		}
		strncpy( field, hdr_ptr, len );
		field[len]='\0';
		http_set_field( http_hdr, field );
		hdr_ptr = ptr+2;
	} while( hdr_ptr<(http_hdr->buffer+pos_hdr_sep) );
	
	if( field!=NULL ) free( field );

	if( pos_hdr_sep+4<http_hdr->buffer_size ) {
		// Response has data!
		int data_length = http_hdr->buffer_size-(pos_hdr_sep+4);
		http_hdr->body = (char*)malloc( data_length );
		if( http_hdr->body==NULL ) {
			printf("Memory allocation failed\n");
			return -1;
		}
		memcpy( http_hdr->body, http_hdr->buffer+pos_hdr_sep+4, data_length );
		http_hdr->body_size = data_length;
	}

	http_hdr->is_parsed = 1;
	return 0;
}

char *
http_build_request( HTTP_header_t *http_hdr ) {
	char *ptr;
	int i;
	int len;
	if( http_hdr==NULL ) return NULL;

	if( http_hdr->method==NULL ) http_set_method( http_hdr, "GET");
	if( http_hdr->uri==NULL ) http_set_uri( http_hdr, "/");

	// Compute the request length
	len = strlen(http_hdr->method)+strlen(http_hdr->uri)+12;	// Method line
	for( i=0 ; i<http_hdr->field_nb ; i++ ) 			// Fields
		len += strlen(http_hdr->fields[i])+2;
	len += 2;							// CRLF
	if( http_hdr->body!=NULL ) {
		len += http_hdr->body_size;
	}
	if( http_hdr->buffer!=NULL ) {
		free( http_hdr->buffer );
		http_hdr->buffer = NULL;
	}
	http_hdr->buffer = (char*)malloc(len);
	if( http_hdr->buffer==NULL ) {
		printf("Memory allocation failed\n");
		return NULL;
	}
	http_hdr->buffer_size = len;

	ptr = http_hdr->buffer;
	ptr += sprintf( ptr, "%s %s HTTP/1.%d\r\n", http_hdr->method, http_hdr->uri, http_hdr->http_minor_version );
	for( i=0 ; i<http_hdr->field_nb ; i++ ) 
		ptr += sprintf( ptr, "%s\r\n", http_hdr->fields[i] );
	ptr += sprintf( ptr, "\r\n" );
	if( http_hdr->body!=NULL ) {
		memcpy( ptr, http_hdr->body, http_hdr->body_size );
	}
	return http_hdr->buffer;	
}

char *
http_get_field( HTTP_header_t *http_hdr, const char *field_name ) {
	if( http_hdr==NULL || field_name==NULL ) return NULL;
	http_hdr->search_pos = 0;
	if( http_hdr->field_search!=NULL ) free( http_hdr->field_search );
	http_hdr->field_search = (char*)malloc(strlen(field_name)+1);
	if( http_hdr->field_search==NULL ) {
		printf("Memory allocation failed\n");
		return NULL;
	}
	strcpy( http_hdr->field_search, field_name );
	return http_get_next_field( http_hdr );
}

char *
http_get_next_field( HTTP_header_t *http_hdr ) {
	char *ptr;
	int i;
	if( http_hdr==NULL ) return NULL;

	for( i=http_hdr->search_pos ; i<http_hdr->field_nb ; i++ ) {
		ptr = strstr( http_hdr->fields[i], ":" );
		if( ptr==NULL ) return NULL;
		if( !strncasecmp( http_hdr->fields[i], http_hdr->field_search, ptr-http_hdr->fields[i] ) ) {
			ptr++;	// Skip the column
			while( ptr[0]==' ' ) ptr++; // Skip the spaces if there is some
			http_hdr->search_pos = i+1;
			return ptr;	// return the value without the field name
		}
	}
	return NULL;
}

void
http_set_field( HTTP_header_t *http_hdr, const char *field ) {
	int pos;
	if( http_hdr==NULL || field==NULL ) return;

	pos = http_hdr->field_nb;

	http_hdr->fields[pos] = (char*)malloc(strlen(field)+1);
	if( http_hdr->fields[pos]==NULL ) {
		printf("Memory allocation failed\n");
		return;
	}
	http_hdr->field_nb++;
	strcpy( http_hdr->fields[pos], field );
}

void
http_set_method( HTTP_header_t *http_hdr, const char *method ) {
	if( http_hdr==NULL || method==NULL ) return;

	http_hdr->method = (char*)malloc(strlen(method)+1);
	if( http_hdr->method==NULL ) {
		printf("Memory allocation failed\n");
		return;
	}
	strcpy( http_hdr->method, method );
}

void
http_set_uri( HTTP_header_t *http_hdr, const char *uri ) {
	if( http_hdr==NULL || uri==NULL ) return;

	http_hdr->uri = (char*)malloc(strlen(uri)+1);
	if( http_hdr->uri==NULL ) {
		printf("Memory allocation failed\n");
		return;
	}
	strcpy( http_hdr->uri, uri );
}

void
http_debug_hdr( HTTP_header_t *http_hdr ) {
	int i;
	if( http_hdr==NULL ) return;

	printf("protocol: %s\n", http_hdr->protocol );
	printf("http minor version: %d\n", http_hdr->http_minor_version );
	printf("uri: %s\n", http_hdr->uri );
	printf("method: %s\n", http_hdr->method );
	printf("status code: %d\n", http_hdr->status_code );
	printf("reason phrase: %s\n", http_hdr->reason_phrase );

	printf("Fields:\n");
	for( i=0 ; i<http_hdr->field_nb ; i++ )
		printf(" %d - %s\n", i, http_hdr->fields[i] );
}
