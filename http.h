#ifndef __HTTP_H
#define __HTTP_H

#define HTTP_FIELD_MAX	20

typedef struct {
	char *protocol;
	char *method;
	char *uri;
	int status_code;
	char *reason_phrase;
	int http_minor_version;
	char *fields[HTTP_FIELD_MAX];
	int field_nb;
	char *field_search;
	int search_pos;
	char *body;
	int body_size;
} HTTP_header_t;

HTTP_header_t*	http_new_header();
void		http_free( HTTP_header_t *http_hdr );
HTTP_header_t*	http_new_response( char *data, int length );
char* 		http_get_request( HTTP_header_t *http_hdr );
char* 		http_get_field( HTTP_header_t *http_hdr, const char *field_name );
char*		http_get_next_field( HTTP_header_t *http_hdr );
void		http_set_field( HTTP_header_t *http_hdr, const char *field );
void		http_set_method( HTTP_header_t *http_hdr, const char *method );
void		http_set_uri( HTTP_header_t *http_hdr, const char *uri );

void		http_debug_hdr( HTTP_header_t *http_hdr );

#endif // __HTTP_H
