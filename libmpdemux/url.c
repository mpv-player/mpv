/*
 * URL Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "url.h"
#include "mp_msg.h"

URL_t*
url_new(char* url) {
	int pos1, pos2;
	URL_t* Curl;
	char *ptr1, *ptr2, *ptr3;

	if( url==NULL ) return NULL;
	
	// Create the URL container
	Curl = (URL_t*)malloc(sizeof(URL_t));
	if( Curl==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed!\n");
		return NULL;
	}
	// Initialisation of the URL container members
	memset( Curl, 0, sizeof(URL_t) );

	// Copy the url in the URL container
	Curl->url = strdup(url);
	if( Curl->url==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed!\n");
		return NULL;
	}

	// extract the protocol
	ptr1 = strstr(url, "://");
	if( ptr1==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_V,"Not an URL!\n");
		return NULL;
	}
	pos1 = ptr1-url;
	Curl->protocol = (char*)malloc(pos1+1);
	strncpy(Curl->protocol, url, pos1);
	if( Curl->protocol==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed!\n");
		return NULL;
	}
	Curl->protocol[pos1] = '\0';

	// jump the "://"
	ptr1 += 3;
	pos1 += 3;

	// check if a username:password is given
	ptr2 = strstr(ptr1, "@");
	ptr3 = strstr(ptr1, "/");
	if( ptr3!=NULL && ptr3<ptr2 ) {
		// it isn't really a username but rather a part of the path
		ptr2 = NULL;
	}
	if( ptr2!=NULL ) {
		// We got something, at least a username...
		int len = ptr2-ptr1;
		Curl->username = (char*)malloc(len+1);
		if( Curl->username==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed!\n");
			return NULL;
		}
		strncpy(Curl->username, ptr1, len);
		Curl->username[len] = '\0';

		ptr3 = strstr(ptr1, ":");
		if( ptr3!=NULL && ptr3<ptr2 ) {
			// We also have a password
			int len2 = ptr2-ptr3-1;
			Curl->username[ptr3-ptr1]='\0';
			Curl->password = (char*)malloc(len2+1);
			if( Curl->password==NULL ) {
				mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed!\n");
				return NULL;
			}
			strncpy( Curl->password, ptr3+1, len2);
			Curl->password[len2]='\0';
		}
		ptr1 = ptr2+1;
		pos1 = ptr1-url;
	}
	
	// look if the port is given
	ptr2 = strstr(ptr1, ":");
	// If the : is after the first / it isn't the port
	ptr3 = strstr(ptr1, "/");
	if(ptr3 && ptr3 - ptr2 < 0) ptr2 = NULL;
	if( ptr2==NULL ) {
		// No port is given
		// Look if a path is given
		ptr2 = strstr(ptr1, "/");
		if( ptr2==NULL ) {
			// No path/filename
			// So we have an URL like http://www.hostname.com
			pos2 = strlen(url);
		} else {
			// We have an URL like http://www.hostname.com/file.txt
			pos2 = ptr2-url;
		}
	} else {
		// We have an URL beginning like http://www.hostname.com:1212
		// Get the port number
		Curl->port = atoi(ptr2+1);
		pos2 = ptr2-url;
	}
	// copy the hostname in the URL container
	Curl->hostname = (char*)malloc(pos2-pos1+1);
	if( Curl->hostname==NULL ) {
		mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed!\n");
		return NULL;
	}
	strncpy(Curl->hostname, ptr1, pos2-pos1);
	Curl->hostname[pos2-pos1] = '\0';

	// Look if a path is given
	ptr2 = strstr(ptr1, "/");
	if( ptr2!=NULL ) {
		// A path/filename is given
		// check if it's not a trailing '/'
		if( strlen(ptr2)>1 ) {
			// copy the path/filename in the URL container
			Curl->file = strdup(ptr2);
			if( Curl->file==NULL ) {
				mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed!\n");
				return NULL;
			}
		}
	} 
	// Check if a filenme was given or set, else set it with '/'
	if( Curl->file==NULL ) {
		Curl->file = (char*)malloc(2);
		if( Curl->file==NULL ) {
			mp_msg(MSGT_NETWORK,MSGL_FATAL,"Memory allocation failed!\n");
			return NULL;
		}
		strcpy(Curl->file, "/");
	}
	
	return Curl;
}

void
url_free(URL_t* url) {
	if(!url) return;
	if(url->url) free(url->url);
	if(url->protocol) free(url->protocol);
	if(url->hostname) free(url->hostname);
	if(url->file) free(url->file);
	if(url->username) free(url->username);
	if(url->password) free(url->password);
	free(url);
}


/* Replace escape sequences in an URL (or a part of an URL) */
/* works like strcpy(), but without return argument */
/* unescape_url_string comes from ASFRecorder */
void
url_unescape_string(char *outbuf, char *inbuf)
{
	unsigned char c;
	do {
		c = *inbuf++;
		if (c == '%') {
			unsigned char c1 = *inbuf++;
			unsigned char c2 = *inbuf++;
			if (	((c1>='0' && c1<='9') || (c1>='A' && c1<='F')) &&
				((c2>='0' && c2<='9') || (c2>='A' && c2<='F')) ) {
				if (c1>='0' && c1<='9') c1-='0';
				else c1-='A';
				if (c2>='0' && c2<='9') c2-='0';
				else c2-='A';
				c = (c1<<4) + c2;
			}
		}
		*outbuf++ = c;
	} while (c != '\0');
}

/* Replace specific characters in the URL string by an escape sequence */
/* works like strcpy(), but without return argument */
/* escape_url_string comes from ASFRecorder */
void
url_escape_string(char *outbuf, char *inbuf) {
	unsigned char c;
	do {
		c = *inbuf++;
		if(	(c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') ||
			(c >= 0x7f) ||						/* fareast languages(Chinese, Korean, Japanese) */
			c=='-' || c=='_' || c=='.' || c=='!' || c=='~' ||	/* mark characters */
			c=='*' || c=='\'' || c=='(' || c==')' || c=='%' || 	/* do not touch escape character */
			c==';' || c=='/' || c=='?' || c==':' || c=='@' || 	/* reserved characters */
			c=='&' || c=='=' || c=='+' || c=='$' || c==',' || 	/* see RFC 2396 */
			c=='\0' ) {
			*outbuf++ = c;
		} else {
			/* all others will be escaped */
			unsigned char c1 = ((c & 0xf0) >> 4);
			unsigned char c2 = (c & 0x0f);
			if (c1 < 10) c1+='0';
			else c1+='A';
			if (c2 < 10) c2+='0';
			else c2+='A';
			*outbuf++ = '%';
			*outbuf++ = c1;
			*outbuf++ = c2;
		}
	} while (c != '\0');
}

#ifdef __URL_DEBUG
void
url_debug(URL_t *url) {
	if( url==NULL ) {
		printf("URL pointer NULL\n");
		return;
	}
	if( url->url!=NULL ) {
		printf("url=%s\n", url->url );
	}
	if( url->protocol!=NULL ) {
		printf("protocol=%s\n", url->protocol );
	}
	if( url->hostname!=NULL ) {
		printf("hostname=%s\n", url->hostname );
	}
	printf("port=%d\n", url->port );
	if( url->file!=NULL ) {
		printf("file=%s\n", url->file );
	}
	if( url->username!=NULL ) {
		printf("username=%s\n", url->username );
	}
	if( url->password!=NULL ) {
		printf("password=%s\n", url->password );
	}
}
#endif //__URL_DEBUG
