/* 
 * URL Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
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
