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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#ifdef WIN32
#ifdef __MINGW32__
#define mkdir(a,b) mkdir(a)
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2
#include <winsock2.h>
#endif
#else
#include <netdb.h>
#include <sys/ioctl.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include "mp_msg.h"
#include "help_mp.h"

#if defined(__linux__)
	#include <linux/cdrom.h>
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
	#include <sys/cdio.h>
#elif defined(WIN32)
        #include <ddk/ntddcdrm.h>
#elif (__bsdi__)
        #include <dvd.h>
#endif

#include "cdd.h"
#include "version.h"
#include "stream.h"
#include "network.h"

#define DEFAULT_FREEDB_SERVER	"freedb.freedb.org"
#define DEFAULT_CACHE_DIR	"/.cddb/"

stream_t* open_cdda(char *dev, char *track);

static cd_toc_t cdtoc[100];
static int cdtoc_last_track;

int 
read_toc(const char *dev) {
	int first, last;
	int i;
#ifdef WIN32
        HANDLE drive;
        DWORD r;
        CDROM_TOC toc;
        char device[10];

        sprintf(device, "\\\\.\\%s", dev);
        drive = CreateFile(device, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);

        if(!DeviceIoControl(drive, IOCTL_CDROM_READ_TOC, NULL, 0, &toc, sizeof(CDROM_TOC), &r, 0)) {
                mp_msg(MSGT_OPEN, MSGL_ERR, MSGTR_MPDEMUX_CDDB_FailedToReadTOC);
                return 0;
        }

        first = toc.FirstTrack - 1; last = toc.LastTrack;
        for (i = first; i <= last; i++) {
		cdtoc[i].min = toc.TrackData[i].Address[1];
		cdtoc[i].sec = toc.TrackData[i].Address[2];
		cdtoc[i].frame = toc.TrackData[i].Address[3];
        }
        CloseHandle(drive);

#else
	int drive;
	drive = open(dev, O_RDONLY | O_NONBLOCK);
	if( drive<0 ) {
		return drive;
	}
	
#if defined(__linux__) || defined(__bsdi__)
	{
	struct cdrom_tochdr tochdr;
	ioctl(drive, CDROMREADTOCHDR, &tochdr);
	first = tochdr.cdth_trk0 - 1; last = tochdr.cdth_trk1;
	}
	for (i = first; i <= last; i++) {
		struct cdrom_tocentry tocentry;
		tocentry.cdte_track = (i == last) ? 0xAA : i;
		tocentry.cdte_format = CDROM_MSF;
		ioctl(drive, CDROMREADTOCENTRY, &tocentry);
		cdtoc[i].min = tocentry.cdte_addr.msf.minute;
		cdtoc[i].sec = tocentry.cdte_addr.msf.second;
		cdtoc[i].frame = tocentry.cdte_addr.msf.frame;
	}
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__)
	{
	struct ioc_toc_header tochdr;
	ioctl(drive, CDIOREADTOCHEADER, &tochdr);
	first = tochdr.starting_track; last = tochdr.ending_track;
	}
	for (i = first; i <= last; i++) {
		struct ioc_read_toc_single_entry tocentry;
		tocentry.track = (i == last) ? 0xAA : i;
		tocentry.address_format = CD_MSF_FORMAT;
		ioctl(drive, CDIOREADTOCENTRY, &tocentry);
		cdtoc[i].min = tocentry.entry.addr.msf.minute;
		cdtoc[i].sec = tocentry.entry.addr.msf.second;
		cdtoc[i].frame = tocentry.entry.addr.msf.frame;
	}
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	{
	struct ioc_toc_header tochdr;
	ioctl(drive, CDIOREADTOCHEADER, &tochdr);
	first = tochdr.starting_track - 1; last = tochdr.ending_track;
	}
	for (i = first; i <= last; i++) {
		struct ioc_read_toc_entry tocentry;
		struct cd_toc_entry toc_buffer;
		tocentry.starting_track = (i == last) ? 0xAA : i;
		tocentry.address_format = CD_MSF_FORMAT;
		tocentry.data = &toc_buffer;
		tocentry.data_len = sizeof(toc_buffer);
		ioctl(drive, CDIOREADTOCENTRYS, &tocentry);
		cdtoc[i].min = toc_buffer.addr.msf.minute;
		cdtoc[i].sec = toc_buffer.addr.msf.second;
		cdtoc[i].frame = toc_buffer.addr.msf.frame;
	}
#endif
	close(drive);
#endif
	for (i = first; i <= last; i++)
	  cdtoc[i].frame += (cdtoc[i].min * 60 + cdtoc[i].sec) * 75;
	return last;
}

/** 
\brief Reads TOC from CD in the given device and prints the number of tracks
       and the length of each track in minute:second:frame format.
\param *dev the device to analyse
\return if the command line -identify is given, returns the last track of
        the TOC or -1 if the TOC can't be read,
        otherwise just returns 0 and let cddb_resolve the TOC
*/
int cdd_identify(const char *dev)
{
	cdtoc_last_track = 0;
	if (mp_msg_test(MSGT_IDENTIFY, MSGL_INFO))
	{
		int i, min, sec, frame;
		cdtoc_last_track = read_toc(dev);
		if (cdtoc_last_track < 0) {
			mp_msg(MSGT_OPEN, MSGL_ERR, MSGTR_MPDEMUX_CDDB_FailedToOpenDevice, dev);
			return -1;
		}
		mp_msg(MSGT_GLOBAL, MSGL_INFO, "ID_CDDA_TRACKS=%d\n", cdtoc_last_track);
		for (i = 1; i <= cdtoc_last_track; i++)
		{
			frame = cdtoc[i].frame - cdtoc[i-1].frame;
			sec = frame / 75;
			frame -= sec * 75;
			min = sec / 60;
			sec -= min * 60;
			mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_CDDA_TRACK_%d_MSF=%02d:%02d:%02d\n", i, min, sec, frame);
		}
	}
	return cdtoc_last_track;
}

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
	while (i < (unsigned int)tot_trks) {
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
	mp_msg(MSGT_OPEN, MSGL_INFO,"Request[%s]\n", request );

	url = url_new(request);
	if( url==NULL ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_NotAValidURL);
		return -1;
	}
	
	fd = http_send_request(url,0);
	if( fd<0 ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest);
		return -1;
	}

	http_hdr = http_read_response( fd );
	if( http_hdr==NULL ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse);
		return -1;
	}

	http_debug_hdr(http_hdr);
	mp_msg(MSGT_OPEN, MSGL_INFO,"body=[%s]\n", http_hdr->body );

	switch(http_hdr->status_code) {
		case 200:
			ret = reply_parser(http_hdr, cddb_data);
			break;
		case 400:
			mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND);
			break;
		default:
			mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown);
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
	
	file_fd = open(file_name, O_RDONLY
#ifdef WIN32
	| O_BINARY
#endif
	);
	if( file_fd<0 ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_NoCacheFound);
		return -1;
	}

	ret = fstat( file_fd, &stats );
	if( ret<0 ) {
		perror("fstat");
		file_size = 4096;
	} else {
		file_size = stats.st_size;
	}
	
	cddb_data->xmcd_file = malloc(file_size);
	if( cddb_data->xmcd_file==NULL ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MemAllocFailed);
		close(file_fd);
		return -1;
	}
	cddb_data->xmcd_file_size = read(file_fd, cddb_data->xmcd_file, file_size);
	if( cddb_data->xmcd_file_size!=file_size ) {
		mp_msg(MSGT_DEMUX, MSGL_WARN, MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead);
		close(file_fd);
		return -1;
	}
	
	close(file_fd);
	
	return 0;
}

int
cddb_write_cache(cddb_data_t *cddb_data) {
	// We have the file, save it for cache.
	struct stat file_stat;
	char file_name[100];
	int file_fd, ret;
	int wrote=0;

	if( cddb_data==NULL || cddb_data->cache_dir==NULL ) return -1;

	// Check if the CDDB cache dir exist
	ret = stat( cddb_data->cache_dir, &file_stat );
	if( ret<0 ) {
		// Directory not present, create it.
		ret = mkdir( cddb_data->cache_dir, 0755 );
#ifdef __MINGW32__
		if( ret<0 && errno != EEXIST ) {
#else
		if( ret<0 ) {
#endif
			perror("mkdir");
			mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory, cddb_data->cache_dir);
			return -1;
		}
	}
	
	sprintf( file_name, "%s%08lx", cddb_data->cache_dir, cddb_data->disc_id );
	
	file_fd = creat(file_name, S_IREAD|S_IWRITE);
	if( file_fd<0 ) {
		perror("create");
		return -1;
	}
	
	wrote = write(file_fd, cddb_data->xmcd_file, cddb_data->xmcd_file_size);
	if( wrote<0 ) {
		perror("write");
		close(file_fd);
		return -1;
	}
	if( (unsigned int)wrote!=cddb_data->xmcd_file_size ) {
		mp_msg(MSGT_DEMUX, MSGL_WARN, MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten);
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
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_ParseError);
		return -1;
	}

	switch(status) {
		case 210:
			ret = sscanf( http_hdr->body, "%d %s %08lx", &status, category, &disc_id);
			if( ret!=3 ) {
				mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_ParseError);
				return -1;
			}
			// Check if it's a xmcd database file
			ptr = strstr(http_hdr->body, "# xmcd");
			if( ptr==NULL ) {
				mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned);
				return -1;
			}
			// Ok found the beginning of the file
			// look for the end
			ptr2 = strstr(ptr, "\r\n.\r\n");
			if( ptr2==NULL ) {
				ptr2 = strstr(ptr, "\n.\n");
				if( ptr2==NULL ) {
					mp_msg(MSGT_DEMUX, MSGL_FIXME, "Unable to find '.'\n");
					ptr2=ptr+strlen(ptr); //return -1;
				}
			}
			// Ok found the end
			// do a sanity check
			if( http_hdr->body_size<(unsigned int)(ptr2-ptr) ) {
				mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_UnexpectedFIXME);
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
			mp_msg(MSGT_DEMUX, MSGL_FIXME, MSGTR_MPDEMUX_CDDB_UnhandledCode);
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
cddb_parse_matches_list(HTTP_header_t *http_hdr, cddb_data_t *cddb_data) {
	char album_title[100];
	char *ptr = NULL;
	int ret;
	
	ptr = strstr(http_hdr->body, "\n");
	if( ptr==NULL ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_UnableToFindEOL);
		return -1;
	}
	ptr++;
	// We have a list of exact/inexact matches, so which one do we use?
	// So let's take the first one.
	ret = sscanf(ptr, "%s %08lx %s", cddb_data->category, &(cddb_data->disc_id), album_title);
	if( ret!=3 ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_ParseError);
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
	mp_msg(MSGT_DEMUX, MSGL_STATUS, MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle, album_title);
	return 0;
}

int
cddb_query_parse(HTTP_header_t *http_hdr, cddb_data_t *cddb_data) {
	char album_title[100];
	char *ptr = NULL;
	int ret, status;
	
	ret = sscanf( http_hdr->body, "%d ", &status);
	if( ret!=1 ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_ParseError);
		return -1;
	}

	switch(status) {
		case 200:
			// Found exact match
			ret = sscanf(http_hdr->body, "%d %s %08lx %s", &status, cddb_data->category, &(cddb_data->disc_id), album_title);
			if( ret!=4 ) {
				mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_ParseError);
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
			mp_msg(MSGT_DEMUX, MSGL_STATUS, MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle, album_title);
			return cddb_request_titles(cddb_data);
		case 202:
			// No match found
			mp_msg(MSGT_DEMUX, MSGL_WARN, MSGTR_MPDEMUX_CDDB_AlbumNotFound);
			break;
		case 210:
			// Found exact matches, list follows
			cddb_parse_matches_list(http_hdr, cddb_data);
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
			cddb_parse_matches_list(http_hdr, cddb_data);
			return cddb_request_titles(cddb_data);
		case 500:
			mp_msg(MSGT_DEMUX, MSGL_FIXME, MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr);
			break;
		default:
			mp_msg(MSGT_DEMUX, MSGL_FIXME, MSGTR_MPDEMUX_CDDB_UnhandledCode);	
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
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_ParseError);	
		return -1;
	}

	switch(status) {
		case 210:
			ptr = strstr(http_hdr->body, "max proto:");
			if( ptr==NULL ) {
				mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_ParseError);
				return -1;
			}
			ret = sscanf(ptr, "max proto: %d", &max);
			if( ret!=1 ) {
				mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_ParseError);
				return -1;
			}
			cddb_data->freedb_proto_level = max;
			return 0;
		default:
			mp_msg(MSGT_DEMUX, MSGL_FIXME, MSGTR_MPDEMUX_CDDB_UnhandledCode);	
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
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_ParseError);
		return -1;
	}

	switch(status) {
		case 210:
			// TODO: Parse the sites
			ret = cddb_data->anonymous;	// For gcc complaining about unused parameter.
			return 0;
		case 401:
			mp_msg(MSGT_DEMUX, MSGL_FIXME, MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable);
			break;
		default:
			mp_msg(MSGT_DEMUX, MSGL_FIXME, MSGTR_MPDEMUX_CDDB_UnhandledCode);
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
	unsigned int i, time_len;
	int ret;

	ptr = offsets;
	for( i=0; i<cddb_data->tracks ; i++ ) {
		ptr += sprintf(ptr, "%d+", cdtoc[i].frame );
		if (ptr-offsets > sizeof offsets - 40) break;
	}
	ptr[0]=0;
	time_len = (cdtoc[cddb_data->tracks].frame)/75;
	
	cddb_data->freedb_server = DEFAULT_FREEDB_SERVER;
	cddb_data->freedb_proto_level = 1;
	cddb_data->xmcd_file = NULL;

	cddb_create_hello(cddb_data);
	if( cddb_get_proto_level(cddb_data)<0 ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel);
		return -1;
	}

	//cddb_get_freedb_sites(&cddb_data);

	sprintf(command, "cddb+query+%08lx+%d+%s%d", cddb_data->disc_id, cddb_data->tracks, offsets, time_len );
	ret = cddb_http_request(command, cddb_query_parse, cddb_data);
	if( ret<0 ) return -1;

	if( cddb_data->cache_dir!=NULL ) {
		free(cddb_data->cache_dir);
	}
	return 0;
}

int
cddb_resolve(const char *dev, char **xmcd_file) {
	char cddb_cache_dir[] = DEFAULT_CACHE_DIR;
	char *home_dir = NULL;
	cddb_data_t cddb_data;

	if (cdtoc_last_track <= 0)
	{
	    cdtoc_last_track = read_toc(dev);
	    if (cdtoc_last_track < 0) {
		mp_msg(MSGT_OPEN, MSGL_ERR, MSGTR_MPDEMUX_CDDB_FailedToOpenDevice, dev);
		return -1;
	    }
	}
	cddb_data.tracks = cdtoc_last_track;
	cddb_data.disc_id = cddb_discid(cddb_data.tracks);
	cddb_data.anonymous = 1;	// Don't send user info by default

	// Check if there is a CD in the drive
	// FIXME: That's not really a good way to check
	if( cddb_data.disc_id==0 ) {
		mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MPDEMUX_CDDB_NoCDInDrive);
		return -1;
	}
	
	home_dir = getenv("HOME");
#ifdef __MINGW32__
	if( home_dir==NULL ) home_dir = getenv("USERPROFILE");
	if( home_dir==NULL ) home_dir = getenv("HOMEPATH");
	// Last resort, store the cddb cache in the mplayer directory
	if( home_dir==NULL ) home_dir = (char *)get_path("");
#endif
	if( home_dir==NULL ) {
		cddb_data.cache_dir = NULL;
	} else {
		cddb_data.cache_dir = malloc(strlen(home_dir)+strlen(cddb_cache_dir)+1);
		if( cddb_data.cache_dir==NULL ) {
			mp_msg(MSGT_DEMUX, MSGL_ERR, MSGTR_MemAllocFailed);
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
		cd_info->album = malloc(strlen(album+2)+1);
		if( cd_info->album==NULL ) {
			return NULL;
		}
		strcpy( cd_info->album, album+2 );
		album--;
		album[0] = '\0';
		cd_info->artist = malloc(strlen(ptr)+1);
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
		cd_info->genre = malloc(strlen(ptr)+1);
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
			else if( xmcd_parse_ttitle(cd_info, ptr) ) audiolen++;	// <-- audiolen++ to shut up gcc warning
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
