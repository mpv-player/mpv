/*
 * CDDB HTTP protocol 
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2002, MPlayer team.
 *
 * Implementation follow the freedb.howto1.06.txt specification
 * from http://freedb.freedb.org
 * 
 * discid computation by Jeremy D. Zawodny
 *	 Copyright (c) 1998-2000 Jeremy D. Zawodny <Jeremy@Zawodny.com>
 *	 Code release under GPL
 *
 */

#include "config.h"

#if defined(HAVE_CDDA) && defined(STREAMING)

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__bsdi__)
	#define SYS_BSD 1
#endif

#if defined(__OpenBSD__)
	#define SYS_OBSD 1
#endif

#if defined(__linux__)
	#include <linux/cdrom.h>
#elif defined(SYS_BSD)
	#include <sys/cdio.h>
#elif defined(SYS_OBSD)
	#include <util.h>
	#include <sys/cdio.h>
#endif

#include "cdd.h"
#include "../version.h"
#include "stream.h"
#include "network.h"

#define DEFAULT_FREEDB_SERVER	"freedb.freedb.org"
#define DEFAULT_CACHE_DIR	"/.cddb/"

stream_t* open_cdda(char *dev, char *track);

static cd_toc_t cdtoc[100];

#if defined(__linux__)
int 
read_toc(void) {
	int drive = open("/dev/cdrom", O_RDONLY | O_NONBLOCK);
	struct cdrom_tochdr tochdr;
	struct cdrom_tocentry tocentry;
	int i;

	ioctl(drive, CDROMREADTOCHDR, &tochdr);
	for (i = tochdr.cdth_trk0; i <= tochdr.cdth_trk1; i++) {
		tocentry.cdte_track = i;
		tocentry.cdte_format = CDROM_MSF;
		ioctl(drive, CDROMREADTOCENTRY, &tocentry);
		cdtoc[i-1].min = tocentry.cdte_addr.msf.minute;
		cdtoc[i-1].sec = tocentry.cdte_addr.msf.second;
		cdtoc[i-1].frame = tocentry.cdte_addr.msf.frame;
		cdtoc[i-1].frame += cdtoc[i-1].min*60*75;
		cdtoc[i-1].frame += cdtoc[i-1].sec*75;
	}
	tocentry.cdte_track = 0xAA;
	tocentry.cdte_format = CDROM_MSF;
	ioctl(drive, CDROMREADTOCENTRY, &tocentry);
	cdtoc[tochdr.cdth_trk1].min = tocentry.cdte_addr.msf.minute;
	cdtoc[tochdr.cdth_trk1].sec = tocentry.cdte_addr.msf.second;
	cdtoc[tochdr.cdth_trk1].frame = tocentry.cdte_addr.msf.frame;
	cdtoc[tochdr.cdth_trk1].frame += cdtoc[tochdr.cdth_trk1].min*60*75;
	cdtoc[tochdr.cdth_trk1].frame += cdtoc[tochdr.cdth_trk1].sec*75;
	close(drive);
	return tochdr.cdth_trk1;
}

#elif defined(SYS_BSD)
int 
read_toc(void) {
	int drive = open("/dev/acd0c", O_RDONLY | O_NONBLOCK);
	struct ioc_toc_header tochdr;
	struct ioc_read_toc_single_entry tocentry;
	int i;

	ioctl(drive, CDIOREADTOCHEADER, &tochdr);
	for (i = tochdr.starting_track; i <= tochdr.ending_track; i++) {
		tocentry.track = i;
		tocentry.address_format = CD_MSF_FORMAT;
		ioctl(drive, CDIOREADTOCENTRY, &tocentry);
		cdtoc[i-1].min = tocentry.entry.addr.msf.minute;
		cdtoc[i-1].sec = tocentry.entry.addr.msf.second;
		cdtoc[i-1].frame = tocentry.entry.addr.msf.frame;
		cdtoc[i-1].frame += cdtoc[i-1].min*60*75;
		cdtoc[i-1].frame += cdtoc[i-1].sec*75;
	}
	tocentry.track = 0xAA;
	tocentry.address_format = CD_MSF_FORMAT;
	ioctl(drive, CDIOREADTOCENTRY, &tocentry);
	cdtoc[tochdr.ending_track].min = tocentry.entry.addr.msf.minute;
	cdtoc[tochdr.ending_track].sec = tocentry.entry.addr.msf.second;
	cdtoc[tochdr.ending_track].frame = tocentry.entry.addr.msf.frame;
	cdtoc[tochdr.ending_track].frame += cdtoc[tochdr.ending_track].min*60*75;
	cdtoc[tochdr.ending_track].frame += cdtoc[tochdr.ending_track].sec*75;
	close(drive);
	return tochdr.ending_track;
}

#elif defined(SYS_OBSD)
int 
read_toc(void) {
	int drive;
	struct ioc_toc_header tochdr;
	struct ioc_read_toc_entry tocentry;
	int i;
	char *drivename;
	char *realdev;
	struct cd_toc_entry toc_buffer;

	drivename = getenv("CDROM");
	if (!drivename)
		drivename = "cd0";
	drive = opendev(drivename, O_RDONLY, OPENDEV_PART, &realdev);
	if (!drive)
		return -1;

	ioctl(drive, CDIOREADTOCHEADER, &tochdr);
	for (i = tochdr.starting_track; i <= tochdr.ending_track; i++) {
		tocentry.starting_track = i;
		tocentry.address_format = CD_MSF_FORMAT;
		tocentry.data = &toc_buffer;
		tocentry.data_len = sizeof(toc_buffer);
		ioctl(drive, CDIOREADTOCENTRYS, &tocentry);
		cdtoc[i-1].min = toc_buffer.addr.msf.minute;
		cdtoc[i-1].sec = toc_buffer.addr.msf.second;
		cdtoc[i-1].frame = toc_buffer.addr.msf.frame;
		cdtoc[i-1].frame += cdtoc[i-1].min*60*75;
		cdtoc[i-1].frame += cdtoc[i-1].sec*75;
	}
	tocentry.starting_track = 0xAA;
	tocentry.address_format = CD_MSF_FORMAT;
	ioctl(drive, CDIOREADTOCENTRYS, &tocentry);
	cdtoc[tochdr.ending_track].min = toc_buffer.addr.msf.minute;
	cdtoc[tochdr.ending_track].sec = toc_buffer.addr.msf.second;
	cdtoc[tochdr.ending_track].frame = toc_buffer.addr.msf.frame;
	cdtoc[tochdr.ending_track].frame += cdtoc[tochdr.ending_track].min*60*75;
	cdtoc[tochdr.ending_track].frame += cdtoc[tochdr.ending_track].sec*75;
	close(drive);
	return tochdr.ending_track;
}
#endif

unsigned int 
cddb_sum(int n) {
	unsigned int ret;

	ret = 0;
	while (n > 0) {
		ret += (n % 10);
		n /= 10;
	}
	return ret;
}

unsigned long 
cddb_discid(int tot_trks) {
	unsigned int i, t = 0, n = 0;

	i = 0;
	while (i < tot_trks) {
		n = n + cddb_sum((cdtoc[i].min * 60) + cdtoc[i].sec);
		i++;
	}
	t = ((cdtoc[tot_trks].min * 60) + cdtoc[tot_trks].sec) -
		((cdtoc[0].min * 60) + cdtoc[0].sec);
	return ((n % 0xff) << 24 | t << 8 | tot_trks);
}



int
cddb_http_request(char *command, int (*reply_parser)(HTTP_header_t*,cddb_data_t*), cddb_data_t *cddb_data) {
	char request[4096];
	int fd, ret = 0;
	URL_t *url;
	HTTP_header_t *http_hdr;
	
	if( reply_parser==NULL || command==NULL || cddb_data==NULL ) return -1;
	
	sprintf( request, "http://%s/~cddb/cddb.cgi?cmd=%s%s&proto=%d", cddb_data->freedb_server, command, cddb_data->cddb_hello, cddb_data->freedb_proto_level );
	printf("Request[%s]\n", request );

	url = url_new(request);
	if( url==NULL ) {
		printf("Not a valid URL\n");
		return -1;
	}
	
	fd = http_send_request(url);
	if( fd<0 ) {
		printf("failed to send the http request\n");
		return -1;
	}

	http_hdr = http_read_response( fd );
	if( http_hdr==NULL ) {
		printf("Failed to read the http response\n");
		return -1;
	}

	http_debug_hdr(http_hdr);
	printf("body=[%s]\n", http_hdr->body );

	switch(http_hdr->status_code) {
		case 200:
			ret = reply_parser(http_hdr, cddb_data);
			break;
		case 400:
			printf("Not Found\n");
			break;
		default:
			printf("Unknown Error code\n");
	}

	http_free( http_hdr );
	url_free( url );
	
	return ret;
}

int
cddb_read_cache(cddb_data_t *cddb_data) {
	char file_name[100];
	struct stat stats;
	int file_fd, ret;
	size_t file_size;

	if( cddb_data==NULL || cddb_data->cache_dir==NULL ) return -1;
	
	sprintf( file_name, "%s%08lx", cddb_data->cache_dir, cddb_data->disc_id);
	
	file_fd = open(file_name, O_RDONLY);
	if( file_fd<0 ) {
		printf("No cache found\n");
		return -1;
	}

	ret = fstat( file_fd, &stats );
	if( ret<0 ) {
		perror("fstat");
		file_size = 4096;
	} else {
		file_size = stats.st_size;
	}
	
	cddb_data->xmcd_file = (char*)malloc(file_size);
	if( cddb_data->xmcd_file==NULL ) {
		printf("Memory allocation failed\n");
		close(file_fd);
		return -1;
	}
	cddb_data->xmcd_file_size = read(file_fd, cddb_data->xmcd_file, file_size);
	if( cddb_data->xmcd_file_size!=file_size ) {
		printf("Not all the xmcd file has been read\n");
		close(file_fd);
		return -1;
	}
	
	close(file_fd);
	
	return 0;
}

int
cddb_write_cache(cddb_data_t *cddb_data) {
	// We have the file, save it for cache.
	char file_name[100];
	int file_fd;
	size_t wrote=0;

	if( cddb_data==NULL || cddb_data->cache_dir==NULL ) return -1;

	sprintf( file_name, "%s%08lx", cddb_data->cache_dir, cddb_data->disc_id);
	
	file_fd = creat(file_name, S_IREAD|S_IWRITE);
	if( file_fd<0 ) {
		perror("open");
		return -1;
	}
	
	wrote = write(file_fd, cddb_data->xmcd_file, cddb_data->xmcd_file_size);
	if( wrote<0 ) {
		perror("write");
		close(file_fd);
		return -1;
	}
	if( wrote!=cddb_data->xmcd_file_size ) {
		printf("Not all the xmcd file has been written\n");
		close(file_fd);
		return -1;
	}
	
	close(file_fd);

	return 0;
}

int
cddb_read_parse(HTTP_header_t *http_hdr, cddb_data_t *cddb_data) {
	unsigned long disc_id;
	char category[100];
	char *ptr=NULL, *ptr2=NULL;
	int ret, status;

	if( http_hdr==NULL || cddb_data==NULL ) return -1;
	
	ret = sscanf( http_hdr->body, "%d ", &status);
	if( ret!=1 ) {
		printf("Parse error\n");
		return -1;
	}

	switch(status) {
		case 210:
			ret = sscanf( http_hdr->body, "%d %s %08lx", &status, category, &disc_id);
			if( ret!=3 ) {
				printf("Parse error\n");
				return -1;
			}
			// Check if it's a xmcd database file
			ptr = strstr(http_hdr->body, "# xmcd");
			if( ptr==NULL ) {
				printf("Invalid xmcd database file returned\n");
				return -1;
			}
			// Ok found the beginning of the file
			// look for the end
			ptr2 = strstr(ptr, "\r\n.\r\n");
			if( ptr2==NULL ) {
				ptr2 = strstr(ptr, "\n.\n");
				if( ptr2==NULL ) {
					printf("Unable to find '.'\n");
					return -1;
				}
			}
			// Ok found the end
			// do a sanity check
			if( http_hdr->body_size<(ptr2-ptr) ) {
				printf("Unexpected fix me\n");
				return -1;
			}
			cddb_data->xmcd_file = ptr;
			cddb_data->xmcd_file_size = ptr2-ptr+2;
			cddb_data->xmcd_file[cddb_data->xmcd_file_size] = '\0';
			// Avoid the http_free function to free the xmcd file...save a mempcy...
			http_hdr->body = NULL;
			http_hdr->body_size = 0;
			return cddb_write_cache(cddb_data);
		default:
			printf("Unhandled code\n");
	}
	return 0;
}

int
cddb_request_titles(cddb_data_t *cddb_data) {
	char command[1024];
	sprintf( command, "cddb+read+%s+%08lx", cddb_data->category, cddb_data->disc_id);
	return cddb_http_request(command, cddb_read_parse, cddb_data); 
}

int
cddb_query_parse(HTTP_header_t *http_hdr, cddb_data_t *cddb_data) {
	char album_title[100];
	char *ptr = NULL;
	int ret, status;
	
	ret = sscanf( http_hdr->body, "%d ", &status);
	if( ret!=1 ) {
		printf("Parse error\n");
		return -1;
	}

	switch(status) {
		case 200:
			// Found exact match
			ret = sscanf(http_hdr->body, "%d %s %08lx %s", &status, cddb_data->category, &(cddb_data->disc_id), album_title);
			if( ret!=4 ) {
				printf("Parse error\n");
				return -1;
			}
			ptr = strstr(http_hdr->body, album_title);
			if( ptr!=NULL ) {
				char *ptr2;
				int len;
				ptr2 = strstr(ptr, "\n");
				if( ptr2==NULL ) {
					len = (http_hdr->body_size)-(ptr-(http_hdr->body));
				} else {
					len = ptr2-ptr+1;
				}
				strncpy(album_title, ptr, len);
				album_title[len-2]='\0';
			}
			printf("Parse OK, found: %s\n", album_title);
			return cddb_request_titles(cddb_data);
		case 202:
			// No match found
			printf("Album not found\n");
			break;
		case 210:
			// Found exact matches, list follows
			ptr = strstr(http_hdr->body, "\n");
			if( ptr==NULL ) {
				printf("Unable to find end of line\n");
				return -1;
			}
			ptr++;
			// We have a list of exact matches, so which one do
			// we use? So let's take the first one.
			ret = sscanf(ptr, "%s %08lx %s", cddb_data->category, &(cddb_data->disc_id), album_title);
			if( ret!=3 ) {
				printf("Parse error\n");
				return -1;
			}
			ptr = strstr(http_hdr->body, album_title);
			if( ptr!=NULL ) {
				char *ptr2;
				int len;
				ptr2 = strstr(ptr, "\n");
				if( ptr2==NULL ) {
					len = (http_hdr->body_size)-(ptr-(http_hdr->body));
				} else {
					len = ptr2-ptr+1;
				}
				strncpy(album_title, ptr, len);
				album_title[len-2]='\0';
			}
			printf("Parse OK, found: %s\n", album_title);
			return cddb_request_titles(cddb_data);
/*
body=[210 Found exact matches, list follows (until terminating `.')
misc c711930d Santana / Supernatural
rock c711930d Santana / Supernatural
blues c711930d Santana / Supernatural
.]
*/	
		case 211:
			// Found inexact matches, list follows
			printf("No exact matches found, list follows\n");
			break;
		default:
			printf("Unhandled code\n");
	}
	return -1;
}

int
cddb_proto_level_parse(HTTP_header_t *http_hdr, cddb_data_t *cddb_data) {
	int max;
	int ret, status;
	char *ptr;
	
	ret = sscanf( http_hdr->body, "%d ", &status);
	if( ret!=1 ) {
		printf("Parse error\n");
		return -1;
	}

	switch(status) {
		case 210:
			ptr = strstr(http_hdr->body, "max proto:");
			if( ptr==NULL ) {
				printf("Parse error\n");
				return -1;
			}
			ret = sscanf(ptr, "max proto: %d", &max);
			if( ret!=1 ) {
				printf("Parse error\n");
				return -1;
			}
			cddb_data->freedb_proto_level = max;
			return 0;
		default:
			printf("Unhandled code\n");
	}
	return -1;
}

int
cddb_get_proto_level(cddb_data_t *cddb_data) {
	return cddb_http_request("stat", cddb_proto_level_parse, cddb_data);
}

int
cddb_freedb_sites_parse(HTTP_header_t *http_hdr, cddb_data_t *cddb_data) {
	int ret, status;
	
	ret = sscanf( http_hdr->body, "%d ", &status);
	if( ret!=1 ) {
		printf("Parse error\n");
		return -1;
	}

	switch(status) {
		case 210:
			// Parse the sites
			return 0;
		case 401:
			printf("No sites information available\n");
			break;
		default:
			printf("Unhandled code\n");
	}
	return -1;
}

int
cddb_get_freedb_sites(cddb_data_t *cddb_data) {
	return cddb_http_request("sites", cddb_freedb_sites_parse, cddb_data);
}

void
cddb_create_hello(cddb_data_t *cddb_data) {
	char host_name[51];
	char *user_name;
	
	if( cddb_data->anonymous ) {	// Default is anonymous
		/* Note from Eduardo Pérez Ureta <eperez@it.uc3m.es> : 
		 * We don't send current user/host name in hello to prevent spam.
		 * Software that sends this is considered spyware
		 * that most people don't like.
		 */
		user_name = "anonymous";
		strcpy(host_name, "localhost");
	} else {
		if( gethostname(host_name, 50)<0 ) {
			strcpy(host_name, "localhost");
		}
		user_name = getenv("LOGNAME");
	}
	sprintf( cddb_data->cddb_hello, "&hello=%s+%s+%s+%s", user_name, host_name, "MPlayer", VERSION );
}

int 
cddb_retrieve(cddb_data_t *cddb_data) {
	char offsets[1024], command[1024];
	char *ptr;
	int i, time_len;

	ptr = offsets;
	for( i=0; i<cddb_data->tracks ; i++ ) {
		ptr += sprintf(ptr, "%d+", cdtoc[i].frame );
	}
	time_len = (cdtoc[cddb_data->tracks].frame)/75;

	cddb_data->freedb_server = DEFAULT_FREEDB_SERVER;
	cddb_data->freedb_proto_level = 1;
	cddb_data->xmcd_file = NULL;

	cddb_create_hello(cddb_data);
	if( cddb_get_proto_level(cddb_data)<0 ) {
		printf("Failed to get the protocol level\n");
		return -1;
	}

	//cddb_get_freedb_sites(&cddb_data);

	sprintf(command, "cddb+query+%08lx+%d+%s%d", cddb_data->disc_id, cddb_data->tracks, offsets, time_len );
	i = cddb_http_request(command, cddb_query_parse, cddb_data);
	if( i<0 ) return -1;

	if( cddb_data->cache_dir!=NULL ) {
		free(cddb_data->cache_dir);
	}
	return 0;
}

int
cddb_resolve(char **xmcd_file) {
	char cddb_cache_dir[] = DEFAULT_CACHE_DIR;
	char *home_dir = NULL;
	cddb_data_t cddb_data;

	cddb_data.tracks = read_toc();
	cddb_data.disc_id = cddb_discid(cddb_data.tracks);
	cddb_data.anonymous = 1;	// Don't send user info by default
	
	home_dir = getenv("HOME");
	if( home_dir==NULL ) {
		cddb_data.cache_dir = NULL;
	} else {
		cddb_data.cache_dir = (char*)malloc(strlen(home_dir)+strlen(cddb_cache_dir)+1);
		if( cddb_data.cache_dir==NULL ) {
			printf("Memory allocation failed\n");
			return -1;
		}
		sprintf(cddb_data.cache_dir, "%s%s", home_dir, cddb_cache_dir );
	}

	// Check for a cached file
	if( cddb_read_cache(&cddb_data)<0 ) {
		// No Cache found
		if( cddb_retrieve(&cddb_data)<0 ) {
			return -1;
		}
	}

	if( cddb_data.xmcd_file!=NULL ) {
//		printf("%s\n", cddb_data.xmcd_file );
		*xmcd_file = cddb_data.xmcd_file;
		return 0;
	}
	
	return -1;
}

/*******************************************************************************************************************
 *
 * xmcd parser
 *
 *******************************************************************************************************************/
char*
xmcd_parse_dtitle(cd_info_t *cd_info, char *line) {
	char *ptr, *album;
	ptr = strstr(line, "DTITLE=");
	if( ptr!=NULL ) {
		ptr += 7;
		album = strstr(ptr, "/");
		if( album==NULL ) return NULL;
		cd_info->album = (char*)malloc(strlen(album+2)+1);
		if( cd_info->album==NULL ) {
			return NULL;
		}
		strcpy( cd_info->album, album+2 );
		album--;
		album[0] = '\0';
		cd_info->artist = (char*)malloc(strlen(ptr)+1);
		if( cd_info->artist==NULL ) {
			return NULL;
		}
		strcpy( cd_info->artist, ptr );
	}
	return ptr;
}

char*
xmcd_parse_dgenre(cd_info_t *cd_info, char *line) {
	char *ptr;
	ptr = strstr(line, "DGENRE=");
	if( ptr!=NULL ) {
		ptr += 7;
		cd_info->genre = (char*)malloc(strlen(ptr)+1);
		if( cd_info->genre==NULL ) {
			return NULL;
		}
		strcpy( cd_info->genre, ptr );
	}
	return ptr;
}

char*
xmcd_parse_ttitle(cd_info_t *cd_info, char *line) {
	unsigned int track_nb;
	unsigned long sec, off;
	char *ptr;
	ptr = strstr(line, "TTITLE");
	if( ptr!=NULL ) {
		ptr += 6;
		// Here we point to the track number
		track_nb = atoi(ptr);
		ptr = strstr(ptr, "=");
		if( ptr==NULL ) return NULL;
		ptr++;
		
		sec = cdtoc[track_nb].frame;
		off = cdtoc[track_nb+1].frame-sec+1;

		cd_info_add_track( cd_info, ptr, track_nb+1, (unsigned int)(off/(60*75)), (unsigned int)((off/75)%60), (unsigned int)(off%75), sec, off );
	}
	return ptr;
}

cd_info_t*
cddb_parse_xmcd(char *xmcd_file) {
	cd_info_t *cd_info = NULL;
	int length, pos = 0;
	char *ptr, *ptr2;
	unsigned int audiolen;
	if( xmcd_file==NULL ) return NULL;
	
	cd_info = cd_info_new();
	if( cd_info==NULL ) {
		return NULL;
	}
	
	length = strlen(xmcd_file);
	ptr = xmcd_file;
	while( ptr!=NULL && pos<length ) {
		// Read a line
		ptr2 = ptr;
		while( ptr2[0]!='\0' && ptr2[0]!='\r' && ptr2[0]!='\n' ) ptr2++;
		if( ptr2[0]=='\0' ) {
			break;
		}
		ptr2[0] = '\0';
		// Ignore comments
		if( ptr[0]!='#' ) {
			// Search for the album title
			if( xmcd_parse_dtitle(cd_info, ptr) );
			// Search for the genre
			else if( xmcd_parse_dgenre(cd_info, ptr) );
			// Search for a track title
			else if( xmcd_parse_ttitle(cd_info, ptr) );
		}
		if( ptr2[1]=='\n' ) ptr2++;
		pos = (ptr2+1)-ptr;
		ptr = ptr2+1;
	}

	audiolen = cdtoc[cd_info->nb_tracks].frame-cdtoc[0].frame;	
	cd_info->min  = (unsigned int)(audiolen/(60*75));
	cd_info->sec  = (unsigned int)((audiolen/75)%60);
	cd_info->msec = (unsigned int)(audiolen%75);
	
	return cd_info;
}

stream_t* 
cddb_open(char *dev, char *track) {
	stream_t *stream;
	cd_info_t *cd_info = NULL;
	cdda_priv *priv;
	char *xmcd_file = NULL;
	int ret;
	
	ret = cddb_resolve(&xmcd_file);
	if( ret==0 ) {
		cd_info = cddb_parse_xmcd(xmcd_file);
		free(xmcd_file);
cd_info_debug( cd_info );	
	}
	stream = open_cdda(dev, track);

	priv = ((cdda_priv*)(stream->priv));
cd_info_debug(priv->cd_info);
	if( cd_info!=NULL ) { 
		cd_info_free(priv->cd_info);
		priv->cd_info = cd_info;
	}	
	return stream;
}
#endif
