/* 
 * URL Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 *
 * TODO: 
 * 	Extract the username/password if present
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "url.h"

URL_t*
url_new(char* url) {
	int pos1, pos2;
	URL_t* Curl;
	char *ptr1, *ptr2;

	// Create the URL container
	Curl = (URL_t*)malloc(sizeof(URL_t));
	if( Curl==NULL ) {
		printf("Memory allocation failed!\n");
		exit(1);
	}
	// Initialisation of the URL container members
	memset( Curl, 0, sizeof(URL_t) );

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
		printf("Malformed URL or not an URL!\n");
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
	Curl->hostname[pos2-pos1-3] = '\0';

	// Look if a path is given
	ptr2 = strstr(ptr1+3, "/");
	if( ptr2!=NULL ) {
		// A path/filename is given
		// check if it's not a trailing '/'
		if( strlen(ptr2)>1 ) {
			// copy the path/filename in the URL container
			Curl->file = (char*)malloc(strlen(ptr2)+1);
			if( Curl->file==NULL ) {
				printf("Memory allocation failed!\n");
				exit(1);
			}
			Curl->file[0]='/';
			strcpy(Curl->file+1, ptr2+1);
		}
	} 
	// Check if a filenme was given or set else set it with '/'
	if( Curl->file==NULL ) {
		Curl->file = (char*)malloc(2);
		if( Curl->file==NULL ) {
			printf("Memory allocation failed!\n");
			exit(1);
		}
		strcpy(Curl->file, "/");
	}
	
	return Curl;
}

void
url_free(URL_t* url) {
	if(url) return;
	if(url->url) free(url->url);
	if(url->protocol) free(url->protocol);
	if(url->hostname) free(url->hostname);
	if(url->file) free(url->file);
	if(url->username) free(url->username);
	if(url->password) free(url->password);
	free(url);
}
