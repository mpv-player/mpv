
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef __MINGW32__
#include <sys/ioctl.h>
#include <sys/wait.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include <strings.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "../osdep/shmem.h"

#include "stream.h"
#include "demuxer.h"

#include "../m_option.h"
#include "../m_struct.h"


extern int verbose; // defined in mplayer.c

#include "cue_read.h"

//#include "vcd_read_bincue.h"

#ifdef USE_DVDREAD
int dvd_read_sector(dvd_priv_t *d,unsigned char* data);
void dvd_seek(dvd_priv_t *d,int pos);
void dvd_close(dvd_priv_t *d);
#endif

#ifdef LIBSMBCLIENT
#include "libsmbclient.h"
#endif

#ifdef HAVE_VCD
extern stream_info_t stream_info_vcd;
#endif
#ifdef HAVE_CDDA
extern stream_info_t stream_info_cdda;
#endif
#ifdef STREAMING
extern stream_info_t stream_info_netstream;
#endif
extern stream_info_t stream_info_null;
extern stream_info_t stream_info_file;

stream_info_t* auto_open_streams[] = {
#ifdef HAVE_VCD
  &stream_info_vcd,
#endif
#ifdef HAVE_CDDA
  &stream_info_cdda,
#endif
#ifdef STREAMING
  &stream_info_netstream,
#endif
  &stream_info_null,
  &stream_info_file,
  NULL
};

stream_t* open_stream_plugin(stream_info_t* sinfo,char* filename,int mode,
			     char** options, int* file_format, int* ret) {
  void* arg = NULL;
  stream_t* s;
  m_struct_t* desc = (m_struct_t*)sinfo->opts;

  // Parse options
  if(desc) {
    arg = m_struct_alloc(desc);
    if(sinfo->opts_url) {
      m_option_t url_opt = 
	{ "stream url", arg , CONF_TYPE_CUSTOM_URL, 0, 0 ,0, sinfo->opts };
      if(m_option_parse(&url_opt,"stream url",filename,arg,M_CONFIG_FILE) < 0) {
	mp_msg(MSGT_OPEN,MSGL_ERR, "URL parsing failed on url %s\n",filename);
	m_struct_free(desc,arg);
	return NULL;
      }	
    }
    if(options) {
      int i;
      for(i = 0 ; options[i] != NULL ; i += 2) {
	mp_msg(MSGT_OPEN,MSGL_DBG2, "Set stream arg %s=%s\n",
	       options[i],options[i+1]);
	if(!m_struct_set(desc,arg,options[i],options[i+1]))
	  mp_msg(MSGT_OPEN,MSGL_WARN, "Failed to set stream option %s=%s\n",
		 options[i],options[i+1]);
      }
    }
  }
  s = new_stream(-2,-2);
  s->url=strdup(filename);
  s->flags |= mode;
  *ret = sinfo->open(s,mode,arg,file_format);
  if((*ret) != STREAM_OK) {
    free(s->url);
    free(s);
    return NULL;
  }
  if(s->type <= -2)
    mp_msg(MSGT_OPEN,MSGL_WARN, "Warning streams need a type !!!!\n");
  if(s->flags & STREAM_SEEK && !s->seek)
    s->flags &= ~STREAM_SEEK;
  if(s->seek && !(s->flags & STREAM_SEEK))
    s->flags |= STREAM_SEEK;
  

  mp_msg(MSGT_OPEN,MSGL_V, "STREAM: [%s] %s\n",sinfo->name,filename);
  mp_msg(MSGT_OPEN,MSGL_V, "STREAM: Description: %s\n",sinfo->info);
  mp_msg(MSGT_OPEN,MSGL_V, "STREAM: Author: %s\n", sinfo->author);
  mp_msg(MSGT_OPEN,MSGL_V, "STREAM: Comment: %s\n", sinfo->comment);
  
  return s;
}


stream_t* open_stream_full(char* filename,int mode, char** options, int* file_format) {
  int i,j,l,r;
  stream_info_t* sinfo;
  stream_t* s;

  for(i = 0 ; auto_open_streams[i] ; i++) {
    sinfo = auto_open_streams[i];
    if(!sinfo->protocols) {
      mp_msg(MSGT_OPEN,MSGL_WARN, "Stream type %s have protocols == NULL, it's a bug\n");
      continue;
    }
    for(j = 0 ; sinfo->protocols[j] ; j++) {
      l = strlen(sinfo->protocols[j]);
      // l == 0 => Don't do protocol matching (ie network and filenames)
      if((l == 0) || ((strncmp(sinfo->protocols[j],filename,l) == 0) &&
		      (strncmp("://",filename+l,3) == 0))) {
	*file_format = DEMUXER_TYPE_UNKNOWN;
	s = open_stream_plugin(sinfo,filename,mode,options,file_format,&r);
	if(s) return s;
	if(r != STREAM_UNSUPORTED) {
	  mp_msg(MSGT_OPEN,MSGL_ERR, "Failed to open %s\n",filename);
	  return NULL;
	}
	break;
      }
    }
  }

  mp_msg(MSGT_OPEN,MSGL_ERR, "No stream found to handle url %s\n",filename);
  return NULL;
}

//=================== STREAMER =========================

int stream_fill_buffer(stream_t *s){
  int len;
  if (/*s->fd == NULL ||*/ s->eof) { s->buf_pos = s->buf_len = 0; return 0; }
  switch(s->type){
#ifdef LIBSMBCLIENT
  case STREAMTYPE_SMB:
    len=smbc_read(s->fd,s->buffer,STREAM_BUFFER_SIZE);
    break;
#endif    
  case STREAMTYPE_STREAM:
#ifdef STREAMING
    if( s->streaming_ctrl!=NULL ) {
	    len=s->streaming_ctrl->streaming_read(s->fd,s->buffer,STREAM_BUFFER_SIZE, s->streaming_ctrl);break;
    } else {
      len=read(s->fd,s->buffer,STREAM_BUFFER_SIZE);break;
    }
#else
    len=read(s->fd,s->buffer,STREAM_BUFFER_SIZE);break;
#endif
  case STREAMTYPE_VCDBINCUE:
    len=cue_vcd_read(s->buffer);break;
#ifdef USE_DVDNAV
  case STREAMTYPE_DVDNAV: {
    dvdnav_stream_read((dvdnav_priv_t*)s->priv,s->buffer,&len);
    if (len==0) return 0; // this was an event, so repeat the read
    break;
  }
#endif
#ifdef USE_DVDREAD
  case STREAMTYPE_DVD: {
    off_t pos=dvd_read_sector(s->priv,s->buffer);
    if(pos>=0){
	len=2048; // full sector
	s->pos=2048*pos-len;
    } else len=-1; // error
    break;
  }
#endif
  case STREAMTYPE_DS:
    len = demux_read_data((demux_stream_t*)s->priv,s->buffer,STREAM_BUFFER_SIZE);
    break;
  
#ifdef HAS_DVBIN_SUPPORT
  case STREAMTYPE_DVB:
   len = dvb_streaming_read(s->fd, s->buffer, STREAM_BUFFER_SIZE, s->priv);
  break;
#endif


    
  default: 
    len= s->fill_buffer ? s->fill_buffer(s,s->buffer,STREAM_BUFFER_SIZE) : 0;
  }
  if(len<=0){ s->eof=1; s->buf_pos=s->buf_len=0; return 0; }
  s->buf_pos=0;
  s->buf_len=len;
  s->pos+=len;
//  printf("[%d]",len);fflush(stdout);
  return len;
}

int stream_seek_long(stream_t *s,off_t pos){
off_t newpos=0;

//  if(verbose>=3) printf("seek_long to 0x%X\n",(unsigned int)pos);

  s->buf_pos=s->buf_len=0;

  switch(s->type){
  case STREAMTYPE_SMB:
  case STREAMTYPE_STREAM:
#ifdef _LARGEFILE_SOURCE
    newpos=pos&(~((long long)STREAM_BUFFER_SIZE-1));break;
#else
    newpos=pos&(~(STREAM_BUFFER_SIZE-1));break;
#endif
  case STREAMTYPE_VCDBINCUE:
    newpos=(pos/VCD_SECTOR_DATA)*VCD_SECTOR_DATA;break;
  case STREAMTYPE_DVD:
    newpos=pos/2048; newpos*=2048; break;
  default:
    // Round on sector size
    if(s->sector_size)
      newpos=(pos/s->sector_size)*s->sector_size;
    else { // Otherwise on the buffer size
#ifdef _LARGEFILE_SOURCE
      newpos=pos&(~((long long)STREAM_BUFFER_SIZE-1));break;
#else
      newpos=pos&(~(STREAM_BUFFER_SIZE-1));break;
#endif
    }
    break;
  }

if(verbose>=3){
#ifdef _LARGEFILE_SOURCE
  printf("s->pos=%llX  newpos=%llX  new_bufpos=%llX  buflen=%X  \n",
    (long long)s->pos,(long long)newpos,(long long)pos,s->buf_len);
#else
  printf("s->pos=%X  newpos=%X  new_bufpos=%X  buflen=%X  \n",
    (unsigned int)s->pos,newpos,pos,s->buf_len);
#endif
}

  pos-=newpos;

if(newpos==0 || newpos!=s->pos){
  switch(s->type){
#ifdef LIBSMBCLIENT
  case STREAMTYPE_SMB:
    s->pos=newpos; // real seek
    if(smbc_lseek(s->fd,s->pos,SEEK_SET)<0) s->eof=1;
    break;
#endif
  case STREAMTYPE_VCDBINCUE:
    s->pos=newpos; // real seek
    cue_set_msf(s->pos/VCD_SECTOR_DATA);
    break;
#ifdef USE_DVDNAV
  case STREAMTYPE_DVDNAV: {
    if (newpos==0) {
      if (dvdnav_stream_reset((dvdnav_priv_t*)s->priv))
        s->pos=0;
    }
    if(newpos!=s->pos){
      mp_msg(MSGT_STREAM,MSGL_INFO,"Cannot seek in DVDNAV streams yet!\n");
      return 1;
    }
    break;
  }
#endif
#ifdef USE_DVDREAD
  case STREAMTYPE_DVD:
    s->pos=newpos; // real seek
    dvd_seek(s->priv,s->pos/2048);
    break;
#endif
  case STREAMTYPE_STREAM:
    //s->pos=newpos; // real seek
    // Some streaming protocol allow to seek backward and forward
    // A function call that return -1 can tell that the protocol
    // doesn't support seeking.
#ifdef STREAMING
    if( s->streaming_ctrl!=NULL && s->streaming_ctrl->streaming_seek ) {
      if( s->streaming_ctrl->streaming_seek( s->fd, pos, s->streaming_ctrl )<0 ) {
        mp_msg(MSGT_STREAM,MSGL_INFO,"Stream not seekable!\n");
        return 1;
      }
    } 
#else
    if(newpos<s->pos){
      mp_msg(MSGT_STREAM,MSGL_INFO,"Cannot seek backward in linear streams!\n");
      return 1;
    }
    while(s->pos<newpos){
      if(stream_fill_buffer(s)<=0) break; // EOF
    }
#endif
    break;
  default:
    // This should at the beginning as soon as all streams are converted
    if(!s->seek)
      return 0;
    // Now seek
    if(!s->seek(s,newpos)) {
      mp_msg(MSGT_STREAM,MSGL_ERR, "Seek failed\n");
      return 0;
    }
  }
//   putchar('.');fflush(stdout);
//} else {
//   putchar('%');fflush(stdout);
}

  stream_fill_buffer(s);
  if(pos>=0 && pos<=s->buf_len){
    s->buf_pos=pos; // byte position in sector
    return 1;
  }
  
//  if(pos==s->buf_len) printf("XXX Seek to last byte of file -> EOF\n");
  
#ifdef _LARGEFILE_SOURCE
  mp_msg(MSGT_STREAM,MSGL_V,"stream_seek: WARNING! Can't seek to 0x%llX !\n",(long long)(pos+newpos));
#else
  mp_msg(MSGT_STREAM,MSGL_V,"stream_seek: WARNING! Can't seek to 0x%X !\n",(pos+newpos));
#endif
  return 0;
}


void stream_reset(stream_t *s){
  if(s->eof){
    s->pos=0; //ftell(f);
//    s->buf_pos=s->buf_len=0;
    s->eof=0;
  }
  if(s->control) s->control(s,STREAM_CTRL_RESET,NULL);
  //stream_seek(s,0);
}

stream_t* new_memory_stream(unsigned char* data,int len){
  stream_t *s=malloc(sizeof(stream_t)+len);
  memset(s,0,sizeof(stream_t));
  s->fd=-1;
  s->type=STREAMTYPE_MEMORY;
  s->buf_pos=0; s->buf_len=len;
  s->start_pos=0; s->end_pos=len;
  stream_reset(s);
  s->pos=len;
  memcpy(s->buffer,data,len);
  return s;
}

stream_t* new_stream(int fd,int type){
  stream_t *s=malloc(sizeof(stream_t));
  if(s==NULL) return NULL;
  memset(s,0,sizeof(stream_t));
  
  s->fd=fd;
  s->type=type;
  s->buf_pos=s->buf_len=0;
  s->start_pos=s->end_pos=0;
  s->priv=NULL;
  s->url=NULL;
  s->cache_pid=0;
  stream_reset(s);
  return s;
}

void free_stream(stream_t *s){
//  printf("\n*** free_stream() called ***\n");
#ifdef USE_STREAM_CACHE
  if(s->cache_pid) {
    cache_uninit(s);
  }
#endif
  switch(s->type) {
#ifdef LIBSMBCLIENT
  case STREAMTYPE_SMB:
    smbc_close(s->fd);
    break;    
#endif
#ifdef HAS_DVBIN_SUPPORT
  case STREAMTYPE_DVB:
    dvbin_close(s->priv);
  break;
#endif

#ifdef USE_DVDREAD
  case STREAMTYPE_DVD:
    dvd_close(s->priv);
#endif
  default:
    if(s->close) s->close(s);
  }
  if(s->fd>0) close(s->fd);
  // Disabled atm, i don't like that. s->priv can be anything after all
  // streams should destroy their priv on close
  //if(s->priv) free(s->priv);
  if(s->url) free(s->url);
  free(s);
}

stream_t* new_ds_stream(demux_stream_t *ds) {
  stream_t* s = new_stream(-1,STREAMTYPE_DS);
  s->priv = ds;
  return s;
}
