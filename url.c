#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "url.h"

URL_t*
set_url(char* url) {
	int pos1, pos2;
	URL_t* Curl;
	char *ptr1, *ptr2;

	// Create the URL container
	Curl = (URL_t*)malloc(sizeof(URL_t));
	if( Curl==NULL ) {
		printf("Memory allocation failed!\n");
		exit(1);
	}
	// Copy the url in the URL container
	Curl->url = (char*)malloc(strlen(url)+1);
	if( Curl->url==NULL ) {
		printf("Memory allocation failed!\n");
		exit(1);
	}
	strcpy(Curl->url, url);

	// extract the protocol
	ptr1 = strstr(url, "://");
	if( ptr1==NULL ) {
		printf("Malformed URL!\n");
		return NULL;
	}
	pos1 = ptr1-url;
	Curl->protocol = (char*)malloc(pos1+1);
	strncpy(Curl->protocol, url, pos1);
	Curl->protocol[pos1] = '\0';

	// look if the port is given
	ptr2 = strstr(ptr1+3, ":");
	if( ptr2==NULL ) {
		// No port is given
		// Look if a path is given
		ptr2 = strstr(ptr1+3, "/");
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
	Curl->hostname = (char*)malloc(strlen(url)+1);
	if( Curl->hostname==NULL ) {
		printf("Memory allocation failed!\n");
		exit(1);
	}
	strncpy(Curl->hostname, ptr1+3, pos2-pos1-3);

	// Look if a path is given
	ptr2 = strstr(ptr1+3, "/");
	if( ptr2!=NULL ) {
		// A path/filename is given
		// check if it's not a trailing '/'
		if( strlen(ptr2)>1 ) {
			// copy the path/filename in the URL container
			Curl->path = (char*)malloc(strlen(ptr2));
			if( Curl->path==NULL ) {
				printf("Memory allocation failed!\n");
				exit(1);
			}
			strcpy(Curl->path, ptr2+1);
		}
	}
	
	return Curl;
}

void
free_url(URL_t* url) {
	if(url) return;
	if(url->url) free(url->url);
	if(url->protocol) free(url->protocol);
	if(url->hostname) free(url->hostname);
	if(url->path) free(url->path);
	free(url);
}
