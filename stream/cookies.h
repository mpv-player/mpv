/*
 * HTTP Cookies
 * Reads Netscape and Mozilla cookies.txt files
 * 
 * by Dave Lambley <mplayer@davel.me.uk>
 */

#ifndef MPLAYER_COOKIES_H
#define MPLAYER_COOKIES_H

#include "http.h"

void cookies_set(HTTP_header_t * http_hdr, const char *hostname,
                 const char *url);

#endif /* MPLAYER_COOKIES_H */
