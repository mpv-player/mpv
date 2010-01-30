/*
 * HTTP Helper
 *
 * Copyright (C) 2001 Bertrand Baudet <bertrand_baudet@yahoo.com>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_HTTP_H
#define MPLAYER_HTTP_H

#include <sys/types.h>

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
#endif /* MPLAYER_HTTP_H */
