/*
 * URL Helper
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

#ifndef MPLAYER_URL_H
#define MPLAYER_URL_H

//#define URL_DEBUG

typedef struct {
	char *url;
	char *protocol;
	char *hostname;
	char *file;
	unsigned int port;
	char *username;
	char *password;
} URL_t;

URL_t *url_redirect(URL_t **url, const char *redir);
URL_t* url_new(const char* url);
void   url_free(URL_t* url);

void url_unescape_string(char *outbuf, const char *inbuf);
void url_escape_string(char *outbuf, const char *inbuf);

#ifdef URL_DEBUG
void url_debug(const URL_t* url);
#endif /* URL_DEBUG */

#endif /* MPLAYER_URL_H */
