#ifndef URL_H
#define URL_H

typedef struct {
	char *url;
	char *protocol;
	char *hostname;
	char *path;
	unsigned int port;
} URL_t;

URL_t* set_url(char* url);
void   free_url(URL_t* url);

#endif
