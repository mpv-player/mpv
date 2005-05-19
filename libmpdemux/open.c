
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef __FreeBSD__
#include <sys/cdrio.h>
#endif

#include "../m_option.h"
#include "stream.h"
#include "demuxer.h"
#include "mf.h"

#ifdef MPLAYER_NETWORK
#include "url.h"
#include "network.h"
extern int streaming_start( stream_t *stream, int *demuxer_type, URL_t *url);
#ifdef STREAMING_LIVE_DOT_COM
#include "demux_rtp.h"
#endif
static URL_t* url;
#endif

/// We keep these 2 for the gui atm, but they will be removed.
int vcd_track=0;
char* cdrom_device=NULL;


// Define function about auth the libsmbclient library
// FIXME: I really do not not is this function is properly working

#ifdef LIBSMBCLIENT

#include "libsmbclient.h"

static char smb_password[15];
static char smb_username[15];

static void smb_auth_fn(const char *server, const char *share,
             char *workgroup, int wgmaxlen, char *username, int unmaxlen,
	     char *password, int pwmaxlen)
{
  char temp[128];
  
  strcpy(temp, "LAN");				  
  if (temp[strlen(temp) - 1] == 0x0a)
    temp[strlen(temp) - 1] = 0x00;
					
  if (temp[0]) strncpy(workgroup, temp, wgmaxlen - 1);
					   
  strcpy(temp, smb_username); 
  if (temp[strlen(temp) - 1] == 0x0a)
    temp[strlen(temp) - 1] = 0x00;
						    
  if (temp[0]) strncpy(username, temp, unmaxlen - 1);
						      
  strcpy(temp, smb_password); 
  if (temp[strlen(temp) - 1] == 0x0a)
    temp[strlen(temp) - 1] = 0x00;
								
   if (temp[0]) strncpy(password, temp, pwmaxlen - 1);
}
								  

#endif

// Open a new stream  (stdin/file/vcd/url)

stream_t* open_stream(char* filename,char** options, int* file_format){
stream_t* stream=NULL;
int f=-1;
off_t len;

  // Check if playlist or unknown 
  if (*file_format != DEMUXER_TYPE_PLAYLIST){
    *file_format=DEMUXER_TYPE_UNKNOWN;
  }

if(!filename) {
   mp_msg(MSGT_OPEN,MSGL_ERR,"NULL filename, report this bug\n");
   return NULL;
}


#ifdef MPLAYER_NETWORK
#ifdef STREAMING_LIVE_DOT_COM
  // Check for a SDP file:
  if (strncmp("sdp://",filename,6) == 0) {
       filename += 6;
#if defined(__CYGWIN__) || defined(__MINGW32__)
       f=open(filename,O_RDONLY|O_BINARY);
#else
       f=open(filename,O_RDONLY);
#endif
       if(f<0){ mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_FileNotFound,filename);return NULL; }

       len=lseek(f,0,SEEK_END); lseek(f,0,SEEK_SET);
       if (len == -1)
           return NULL;

#ifdef _LARGEFILE_SOURCE
	 mp_msg(MSGT_OPEN,MSGL_V,"File size is %lld bytes\n", (long long)len);
#else
	 mp_msg(MSGT_OPEN,MSGL_V,"File size is %u bytes\n", (unsigned int)len);
#endif
	 return stream_open_sdp(f, len, file_format);
  }
#endif

  // FIXME: to avoid nonsense error messages...
  if (strncmp("tv://", filename, 5) && strncmp("mf://", filename, 5) &&
    strncmp("vcd://", filename, 6) && strncmp("dvb://", filename, 6) &&
    strncmp("cdda://", filename, 7) && strncmp("cddb://", filename, 7) &&
    strncmp("mpst://", filename, 7) && strncmp("tivo://", filename, 7) &&
    strncmp("file://", filename, 7) && strncmp("cue://", filename, 6) &&
    strncmp("ftp://", filename, 6) &&
    strncmp("dvd://", filename, 6) && strncmp("dvdnav://", filename, 9) &&
    strstr(filename, "://")) {
     url = url_new(filename);
    }
  if(url) {
	if (strcmp(url->protocol, "smb")==0){
#ifdef LIBSMBCLIENT
	    
	    // we need init of libsmbclient
            int err;
	    
	    // FIXME: HACK: make the username/password global varaibles
	    // so the auth_fn function should grab it ...
	    // i cannot thing other way...
	    err = smbc_init(smb_auth_fn, 10);  	/* Initialize things */
	                        	        // libsmbclient using				
	    if (err < 0) {
        	mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_SMBInitError,err);
	    	return NULL;
	    }
	    f=smbc_open(filename, O_RDONLY, 0666);    
	    
	    // cannot open the file, outputs that
	    // MSGTR_FileNotFound -> MSGTR_SMBFileNotFound
    	    if(f<0){ 
		mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_SMBFileNotFound,filename);
		return NULL; 
	    }
	    len=smbc_lseek(f,0,SEEK_END); 
	    smbc_lseek(f,0,SEEK_SET);
	    // FIXME: I really wonder is such situation -> but who cares ;)
//	    if (len == -1)	
//    		   return new_stream(f,STREAMTYPE_STREAM); // open as stream
	    url_free(url);
	    url = NULL;
            stream=new_stream(f,STREAMTYPE_SMB);
    	    stream->end_pos=len;
	    return stream;
#else
	    mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_SMBNotCompiled);
	    return NULL;
#endif
	}
        stream=new_stream(f,STREAMTYPE_STREAM);
	if( streaming_start( stream, file_format, url )<0){
          mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_UnableOpenURL, filename);
	  url_free(url);
	  url = NULL;
	  return NULL;
	} else {
        mp_msg(MSGT_OPEN,MSGL_INFO,MSGTR_ConnToServer, url->hostname );
	url_free(url);
	url = NULL;
	return stream;
	}
  }
#endif

//============ Open STDIN or plain FILE ============

  return open_stream_full(filename,STREAM_READ,options,file_format);
}

