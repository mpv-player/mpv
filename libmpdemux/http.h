/* 
 * HTTP Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __HTTP_H
#define __HTTP_H

typedef struct HTTP_field_type {
	char *field_name;
	struct HTTP_field_type *next;
} HTTP_field_t;

typedef struct {
	char *protocol;
	char *method;
	char *uri;
	unsigned int status_code;
	char *reason_phrase;
	unsigned int http_minor_version;
	// Field variables
	HTTP_field_t *first_field;
	HTTP_field_t *last_field;
	unsigned int field_nb;
	char *field_search;
	HTTP_field_t *field_search_pos;
	// Body variables
	char *body;
	size_t body_size;
	char *buffer;
	size_t buffer_size;
	unsigned int is_parsed;
} HTTP_header_t;

HTTP_header_t*	http_new_header(void);
void		http_free( HTTP_header_t *http_hdr );
int		http_response_append( HTTP_header_t *http_hdr, char *data, int length );
int		http_response_parse( HTTP_header_t *http_hdr );
int		http_is_header_entire( HTTP_header_t *http_hdr );
char* 		http_build_request( HTTP_header_t *http_hdr );
char* 		http_get_field( HTTP_header_t *http_hdr, const char *field_name );
char*		http_get_next_field( HTTP_header_t *http_hdr );
void		http_set_field( HTTP_header_t *http_hdr, const char *field_name );
void		http_set_method( HTTP_header_t *http_hdr, const char *method );
void		http_set_uri( HTTP_header_t *http_hdr, const char *uri );
int		http_add_basic_authentication( HTTP_header_t *http_hdr, const char *username, const char *password );

void		http_debug_hdr( HTTP_header_t *http_hdr );

int 		base64_encode(const void *enc, int encLen, char *out, int outMax);
#endif // __HTTP_H
