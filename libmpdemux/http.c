/* 
 * HTTP Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "url.h"
#include "mp_msg.h"

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
	HTTP_field_t *field, *field2free;
	if( http_hdr==NULL ) return;
	if( http_hdr->protocol!=NULL ) free( http_hdr->protocol );
	if( http_hdr->uri!=NULL ) free( http_hdr->uri );
	if( http_hdr->reason_phrase!=NULL ) free( http_hdr->reason_phrase );
	if( http_hdr->body!=NULL ) free( http_hdr->body );
	if( http_hdr->field_search!=NULL ) free( http_hdr->field_search );
	if( http_hdr->method!=NULL ) free( http_hdr->method );
	if( http_hdr->buffer!=NULL ) free( http_hdr->buffer );
	field = http_hdr->first_field;
	while( field!=NULL ) {
		field2free = field;
		field = field->next;
		free( field2free );
	}
	free( http_hdr );
	http_hdr = NULL;
}

int
http_response_append( HTTP_header_t *http_hdr, char *response, int length ) {
	char *ptr = NULL;
	if( http_hdr==NULL || response==NULL || length<0 ) return -1;
	ptr = (char*)malloc( http_hdr->buffer_size+length );
	if( ptr==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
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
http_is_header_entire( HTTP_header_t *http_hdr ) {
	if( http_hdr==NULL ) return -1;

	if( strstr(http_hdr->buffer, "\r\n\r\n")==NULL &&
	    strstr(http_hdr->buffer, "\n\n")==NULL ) return 0;
	return 1;
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
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Malformed answer. No space separator found.\n");
		return -1;
	}
	len = hdr_ptr-http_hdr->buffer;
	http_hdr->protocol = (char*)malloc(len+1);
	if( http_hdr->protocol==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return -1;
	}
	strncpy( http_hdr->protocol, http_hdr->buffer, len );
	http_hdr->protocol[len]='\0';
	if( !strncasecmp( http_hdr->protocol, "HTTP", 4) ) {
		if( sscanf( http_hdr->protocol+5,"1.%d", &(http_hdr->http_minor_version) )!=1 ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Malformed answer. Unable to get HTTP minor version.\n");
			return -1;
		}
	}

	// Get the status code
	if( sscanf( ++hdr_ptr, "%d", &(http_hdr->status_code) )!=1 ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Malformed answer. Unable to get status code.\n");
		return -1;
	}
	hdr_ptr += 4;

	// Get the reason phrase
	ptr = strstr( hdr_ptr, "\n" );
	if( hdr_ptr==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Malformed answer. Unable to get the reason phrase.\n");
		return -1;
	}
	len = ptr-hdr_ptr;
	http_hdr->reason_phrase = (char*)malloc(len+1);
	if( http_hdr->reason_phrase==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return -1;
	}
	strncpy( http_hdr->reason_phrase, hdr_ptr, len );
	if( http_hdr->reason_phrase[len-1]=='\r' ) {
		len--;
	}
	http_hdr->reason_phrase[len]='\0';

	// Set the position of the header separator: \r\n\r\n
	ptr = strstr( http_hdr->buffer, "\r\n\r\n" );
	if( ptr==NULL ) {
		ptr = strstr( http_hdr->buffer, "\n\n" );
		if( ptr==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Header may be incomplete. No CRLF CRLF found.\n");
			return -1;
		}
	}
	pos_hdr_sep = ptr-http_hdr->buffer;

	// Point to the first line after the method line.
	hdr_ptr = strstr( http_hdr->buffer, "\n" )+1;
	do {
		ptr = hdr_ptr;
		while( *ptr!='\r' && *ptr!='\n' ) ptr++;
		len = ptr-hdr_ptr;
		if( len==0 ) break;
		field = (char*)realloc(field, len+1);
		if( field==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Memory allocation failed\n");
			return -1;
		}
		strncpy( field, hdr_ptr, len );
		field[len]='\0';
		http_set_field( http_hdr, field );
		hdr_ptr = ptr+((*ptr=='\r')?2:1);
	} while( hdr_ptr<(http_hdr->buffer+pos_hdr_sep) );
	
	if( field!=NULL ) free( field );

	if( pos_hdr_sep+4<http_hdr->buffer_size ) {
		// Response has data!
		int data_length = http_hdr->buffer_size-(pos_hdr_sep+4);
		http_hdr->body = (char*)malloc( data_length );
		if( http_hdr->body==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Memory allocation failed\n");
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
	char *ptr, *uri=NULL;
	int len;
	HTTP_field_t *field;
	if( http_hdr==NULL ) return NULL;

	if( http_hdr->method==NULL ) http_set_method( http_hdr, "GET");
	if( http_hdr->uri==NULL ) http_set_uri( http_hdr, "/");
	else {
		uri = (char*)malloc(strlen(http_hdr->uri)*2);
		if( uri==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_ERR,"Memory allocation failed\n");
			return NULL;
		}
		url_escape_string( uri, http_hdr->uri );
	}

	//**** Compute the request length
	// Add the Method line
	len = strlen(http_hdr->method)+strlen(uri)+12;
	// Add the fields
	field = http_hdr->first_field; 
	while( field!=NULL ) {
		len += strlen(field->field_name)+2;
		field = field->next;
	}
	// Add the CRLF
	len += 2;
	// Add the body
	if( http_hdr->body!=NULL ) {
		len += http_hdr->body_size;
	}
	// Free the buffer if it was previously used
	if( http_hdr->buffer!=NULL ) {
		free( http_hdr->buffer );
		http_hdr->buffer = NULL;
	}
	http_hdr->buffer = (char*)malloc(len+1);
	if( http_hdr->buffer==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_ERR,"Memory allocation failed\n");
		return NULL;
	}
	http_hdr->buffer_size = len;

	//*** Building the request
	ptr = http_hdr->buffer;
	// Add the method line
	ptr += sprintf( ptr, "%s %s HTTP/1.%d\r\n", http_hdr->method, uri, http_hdr->http_minor_version );
	field = http_hdr->first_field;
	// Add the field
	while( field!=NULL ) {
		ptr += sprintf( ptr, "%s\r\n", field->field_name );
		field = field->next;
	}
	ptr += sprintf( ptr, "\r\n" );
	// Add the body
	if( http_hdr->body!=NULL ) {
		memcpy( ptr, http_hdr->body, http_hdr->body_size );
	}

	if( uri ) free( uri );
	return http_hdr->buffer;	
}

char *
http_get_field( HTTP_header_t *http_hdr, const char *field_name ) {
	if( http_hdr==NULL || field_name==NULL ) return NULL;
	http_hdr->field_search_pos = http_hdr->first_field;
	http_hdr->field_search = (char*)realloc( http_hdr->field_search, strlen(field_name)+1 );
	if( http_hdr->field_search==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return NULL;
	}
	strcpy( http_hdr->field_search, field_name );
	return http_get_next_field( http_hdr );
}

char *
http_get_next_field( HTTP_header_t *http_hdr ) {
	char *ptr;
	HTTP_field_t *field;
	if( http_hdr==NULL ) return NULL;

	field = http_hdr->field_search_pos;
	while( field!=NULL ) { 
		ptr = strstr( field->field_name, ":" );
		if( ptr==NULL ) return NULL;
		if( !strncasecmp( field->field_name, http_hdr->field_search, ptr-(field->field_name) ) ) {
			ptr++;	// Skip the column
			while( ptr[0]==' ' ) ptr++; // Skip the spaces if there is some
			http_hdr->field_search_pos = field->next;
			return ptr;	// return the value without the field name
		}
		field = field->next;
	}
	return NULL;
}

void
http_set_field( HTTP_header_t *http_hdr, const char *field_name ) {
	HTTP_field_t *new_field;
	if( http_hdr==NULL || field_name==NULL ) return;

	new_field = (HTTP_field_t*)malloc(sizeof(HTTP_field_t));
	if( new_field==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return;
	}
	new_field->next = NULL;
	new_field->field_name = (char*)malloc(strlen(field_name)+1);
	if( new_field->field_name==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return;
	}
	strcpy( new_field->field_name, field_name );

	if( http_hdr->last_field==NULL ) {
		http_hdr->first_field = new_field;
	} else {
		http_hdr->last_field->next = new_field;
	}
	http_hdr->last_field = new_field;
	http_hdr->field_nb++;
}

void
http_set_method( HTTP_header_t *http_hdr, const char *method ) {
	if( http_hdr==NULL || method==NULL ) return;

	http_hdr->method = (char*)malloc(strlen(method)+1);
	if( http_hdr->method==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return;
	}
	strcpy( http_hdr->method, method );
}

void
http_set_uri( HTTP_header_t *http_hdr, const char *uri ) {
	if( http_hdr==NULL || uri==NULL ) return;

	http_hdr->uri = (char*)malloc(strlen(uri)+1);
	if( http_hdr->uri==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed\n");
		return;
	}
	strcpy( http_hdr->uri, uri );
}

void
http_debug_hdr( HTTP_header_t *http_hdr ) {
	HTTP_field_t *field;
	int i = 0;
	if( http_hdr==NULL ) return;

	mp_msg(MSGT_NETWORK,MSGL_V,"--- HTTP DEBUG HEADER --- START ---\n");
	mp_msg(MSGT_NETWORK,MSGL_V,"protocol:           [%s]\n", http_hdr->protocol );
	mp_msg(MSGT_NETWORK,MSGL_V,"http minor version: [%d]\n", http_hdr->http_minor_version );
	mp_msg(MSGT_NETWORK,MSGL_V,"uri:                [%s]\n", http_hdr->uri );
	mp_msg(MSGT_NETWORK,MSGL_V,"method:             [%s]\n", http_hdr->method );
	mp_msg(MSGT_NETWORK,MSGL_V,"status code:        [%d]\n", http_hdr->status_code );
	mp_msg(MSGT_NETWORK,MSGL_V,"reason phrase:      [%s]\n", http_hdr->reason_phrase );
	mp_msg(MSGT_NETWORK,MSGL_V,"body size:          [%d]\n", http_hdr->body_size );

	mp_msg(MSGT_NETWORK,MSGL_V,"Fields:\n");
	field = http_hdr->first_field;
	while( field!=NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_V," %d - %s\n", i++, field->field_name );
		field = field->next;
	}
	mp_msg(MSGT_NETWORK,MSGL_V,"--- HTTP DEBUG HEADER --- END ---\n");
}
