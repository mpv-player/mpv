
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"

#include "stream.h"

#ifdef STREAMING
#include "url.h"
#include "network.h"
static URL_t* url;
#endif

extern int verbose;
extern int vcd_get_track_end(int fd,int track);

// Open a new stream  (stdin/file/vcd/url)

stream_t* open_stream(char* filename,int vcd_track,int* file_format){
stream_t* stream=NULL;
int f=-1;
off_t len;
#ifdef VCD_CACHE
int vcd_cache_size=128;
#endif
#ifdef __FreeBSD__
int bsize = VCD_SECTOR_SIZE;
#endif

//============ Open VideoCD track ==============
if(vcd_track){
  int ret,ret2;
  f=open(filename,O_RDONLY);
  if(f<0){ fprintf(stderr,"CD-ROM Device '%s' not found!\n",filename);return NULL; }
  vcd_read_toc(f);
  ret2=vcd_get_track_end(f,vcd_track);
  if(ret2<0){ fprintf(stderr,"Error selecting VCD track! (get)\n");return NULL;}
  ret=vcd_seek_to_track(f,vcd_track);
  if(ret<0){ fprintf(stderr,"Error selecting VCD track! (seek)\n");return NULL;}
//  seek_to_byte+=ret;
  if(verbose) printf("VCD start byte position: 0x%X  end: 0x%X\n",ret,ret2);
#ifdef VCD_CACHE
  vcd_cache_init(vcd_cache_size);
#endif
#ifdef __FreeBSD__
  if (ioctl (f, CDRIOCSETBLOCKSIZE, &bsize) == -1) {
        perror ( "Error in CDRIOCSETBLOCKSIZE");
  }
#endif
  stream=new_stream(f,STREAMTYPE_VCD);
  stream->start_pos=ret;
  stream->end_pos=ret2;
  return stream;
}

//============ Open STDIN ============
  if(!strcmp(filename,"-")){
      // read from stdin
      printf("Reading from stdin...\n");
      f=0; // 0=stdin
      stream=new_stream(f,STREAMTYPE_STREAM);
      return stream;
  }
  
#ifdef STREAMING
  url = url_new(filename);
  if(url) {
        (*file_format)=autodetectProtocol( url, &f );
        if( (*file_format)==DEMUXER_TYPE_UNKNOWN ) { 
          fprintf(stderr,"Unable to open URL: %s\n", filename);
          url_free(url);
          return NULL;
        }
        f=streaming_start( &url, f, file_format );
        if(f<0){ fprintf(stderr,"Unable to open URL: %s\n", url->url); return NULL; }
        printf("Connected to server: %s\n", url->hostname );
        stream=new_stream(f,STREAMTYPE_STREAM);
	return NULL;
  }
#endif

//============ Open plain FILE ============
       f=open(filename,O_RDONLY);
       if(f<0){ fprintf(stderr,"File not found: '%s'\n",filename);return NULL; }
       len=lseek(f,0,SEEK_END); lseek(f,0,SEEK_SET);
       if (len == -1)
	 perror("Error: lseek failed to obtain video file size");
       else
        if(verbose)
#ifdef _LARGEFILE_SOURCE
	 printf("File size is %lld bytes\n", (long long)len);
#else
	 printf("File size is %u bytes\n", (unsigned int)len);
#endif
       stream=new_stream(f,STREAMTYPE_FILE);
       stream->end_pos=len;
       return stream;

}
