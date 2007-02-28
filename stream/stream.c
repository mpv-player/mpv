
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

#ifndef HAVE_WINSOCK2
#define closesocket close
#else
#include <winsock2.h>
#endif

#include "mp_msg.h"
#include "help_mp.h"
#include "osdep/shmem.h"

#include "stream.h"
#include "libmpdemux/demuxer.h"

#include "m_option.h"
#include "m_struct.h"

#include "cache2.h"

//#include "vcd_read_bincue.h"

#ifdef HAVE_VCD
extern stream_info_t stream_info_vcd;
#endif
#ifdef HAVE_CDDA
extern stream_info_t stream_info_cdda;
#endif
#ifdef MPLAYER_NETWORK
extern stream_info_t stream_info_netstream;
extern stream_info_t stream_info_pnm;
extern stream_info_t stream_info_asf;
extern stream_info_t stream_info_rtsp;
extern stream_info_t stream_info_rtp;
extern stream_info_t stream_info_udp;
extern stream_info_t stream_info_http1;
extern stream_info_t stream_info_http2;
#endif
#ifdef HAS_DVBIN_SUPPORT
extern stream_info_t stream_info_dvb;
#endif
#ifdef USE_TV
extern stream_info_t stream_info_tv;
#endif
#ifdef USE_RADIO
extern stream_info_t stream_info_radio;
#endif
#ifdef HAVE_PVR
extern stream_info_t stream_info_pvr;
#endif
#ifdef HAVE_FTP
extern stream_info_t stream_info_ftp;
#endif
#ifdef HAVE_VSTREAM
extern stream_info_t stream_info_vstream;
#endif
#ifdef USE_DVDNAV
extern stream_info_t stream_info_dvdnav;
#endif
#ifdef LIBSMBCLIENT
extern stream_info_t stream_info_smb;
#endif
#ifdef STREAMING_LIVE555
extern stream_info_t stream_info_sdp;
extern stream_info_t stream_info_rtsp_sip;
#endif

extern stream_info_t stream_info_cue;
extern stream_info_t stream_info_null;
extern stream_info_t stream_info_mf;
extern stream_info_t stream_info_file;
#ifdef USE_DVDREAD
extern stream_info_t stream_info_dvd;
#endif

stream_info_t* auto_open_streams[] = {
#ifdef HAVE_VCD
  &stream_info_vcd,
#endif
#ifdef HAVE_CDDA
  &stream_info_cdda,
#endif
#ifdef MPLAYER_NETWORK
  &stream_info_netstream,
  &stream_info_http1,
  &stream_info_asf,
  &stream_info_pnm,
  &stream_info_rtsp,
#ifdef STREAMING_LIVE555
  &stream_info_sdp,
  &stream_info_rtsp_sip,
#endif
  &stream_info_rtp,
  &stream_info_udp,
  &stream_info_http2,
#endif
#ifdef HAS_DVBIN_SUPPORT
  &stream_info_dvb,
#endif
#ifdef USE_TV
  &stream_info_tv,
#endif
#ifdef USE_RADIO
  &stream_info_radio,
#endif
#ifdef HAVE_PVR
  &stream_info_pvr,
#endif
#ifdef HAVE_FTP
  &stream_info_ftp,
#endif
#ifdef HAVE_VSTREAM
  &stream_info_vstream,
#endif
#ifdef LIBSMBCLIENT
  &stream_info_smb,
#endif
  &stream_info_cue,
#ifdef USE_DVDREAD
  &stream_info_dvd,
#endif
#ifdef USE_DVDNAV
  &stream_info_dvdnav,
#endif

  &stream_info_null,
  &stream_info_mf,
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
  
  s->mode = mode;

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
      mp_msg(MSGT_OPEN,MSGL_WARN, "Stream type %s has protocols == NULL, it's a bug\n", sinfo->name);
      continue;
    }
    for(j = 0 ; sinfo->protocols[j] ; j++) {
      l = strlen(sinfo->protocols[j]);
      // l == 0 => Don't do protocol matching (ie network and filenames)
      if((l == 0 && !strstr(filename, "://")) ||
         ((strncmp(sinfo->protocols[j],filename,l) == 0) &&
		      (strncmp("://",filename+l,3) == 0))) {
	*file_format = DEMUXER_TYPE_UNKNOWN;
	s = open_stream_plugin(sinfo,filename,mode,options,file_format,&r);
	if(s) return s;
	if(r != STREAM_UNSUPORTED) {
	  mp_msg(MSGT_OPEN,MSGL_ERR, MSGTR_FailedToOpen,filename);
	  return NULL;
	}
	break;
      }
    }
  }

  mp_msg(MSGT_OPEN,MSGL_ERR, "No stream found to handle url %s\n",filename);
  return NULL;
}

stream_t* open_output_stream(char* filename,char** options) {
  int file_format; //unused
  if(!filename) {
    mp_msg(MSGT_OPEN,MSGL_ERR,"open_output_stream(), NULL filename, report this bug\n");
    return NULL;
  }

  return open_stream_full(filename,STREAM_WRITE,options,&file_format);
}

//=================== STREAMER =========================

int stream_fill_buffer(stream_t *s){
  int len;
  if (/*s->fd == NULL ||*/ s->eof) { s->buf_pos = s->buf_len = 0; return 0; }
  switch(s->type){
  case STREAMTYPE_STREAM:
#ifdef MPLAYER_NETWORK
    if( s->streaming_ctrl!=NULL ) {
	    len=s->streaming_ctrl->streaming_read(s->fd,s->buffer,STREAM_BUFFER_SIZE, s->streaming_ctrl);break;
    } else {
      len=read(s->fd,s->buffer,STREAM_BUFFER_SIZE);break;
    }
#else
    len=read(s->fd,s->buffer,STREAM_BUFFER_SIZE);break;
#endif
  case STREAMTYPE_DS:
    len = demux_read_data((demux_stream_t*)s->priv,s->buffer,STREAM_BUFFER_SIZE);
    break;
  
    
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

int stream_write_buffer(stream_t *s, unsigned char *buf, int len) {
  int rd;
  if(!s->write_buffer)
    return -1;
  rd = s->write_buffer(s, buf, len);
  if(rd < 0)
    return -1;
  s->pos += rd;
  return rd;
}

int stream_seek_long(stream_t *s,off_t pos){
off_t newpos=0;

//  if( mp_msg_test(MSGT_STREAM,MSGL_DBG3) ) printf("seek_long to 0x%X\n",(unsigned int)pos);

  s->buf_pos=s->buf_len=0;

  if(s->mode == STREAM_WRITE) {
    if(!s->seek || !s->seek(s,pos))
      return 0;
    return 1;
  }

  if(s->sector_size)
      newpos = (pos/s->sector_size)*s->sector_size;
  else
      newpos = pos&(~((off_t)STREAM_BUFFER_SIZE-1));

if( mp_msg_test(MSGT_STREAM,MSGL_DBG3) ){
  mp_msg(MSGT_STREAM,MSGL_DBG3, "s->pos=%"PRIX64"  newpos=%"PRIX64"  new_bufpos=%"PRIX64"  buflen=%X  \n",
    (int64_t)s->pos,(int64_t)newpos,(int64_t)pos,s->buf_len);
}
  pos-=newpos;

if(newpos==0 || newpos!=s->pos){
  switch(s->type){
  case STREAMTYPE_STREAM:
    //s->pos=newpos; // real seek
    // Some streaming protocol allow to seek backward and forward
    // A function call that return -1 can tell that the protocol
    // doesn't support seeking.
#ifdef MPLAYER_NETWORK
    if(s->seek) { // new stream seek is much cleaner than streaming_ctrl one
      if(!s->seek(s,newpos)) {
      	mp_msg(MSGT_STREAM,MSGL_ERR, "Seek failed\n");
      	return 0;
      }
      break;
    }
	
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

while(stream_fill_buffer(s) > 0 && pos >= 0) {
  if(pos<=s->buf_len){
    s->buf_pos=pos; // byte position in sector
    return 1;
  }
  pos -= s->buf_len;
}
  
//  if(pos==s->buf_len) printf("XXX Seek to last byte of file -> EOF\n");
  
  mp_msg(MSGT_STREAM,MSGL_V,"stream_seek: WARNING! Can't seek to 0x%"PRIX64" !\n",(int64_t)(pos+newpos));
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

int stream_control(stream_t *s, int cmd, void *arg){
  if(!s->control) return STREAM_UNSUPORTED;
  return s->control(s, cmd, arg);
}

stream_t* new_memory_stream(unsigned char* data,int len){
  stream_t *s;

  if(len < 0)
    return NULL;
  s=malloc(sizeof(stream_t)+len);
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

#ifdef HAVE_WINSOCK2
  {
    WSADATA wsdata;
    int temp = WSAStartup(0x0202, &wsdata); // there might be a better place for this (-> later)
    mp_msg(MSGT_STREAM,MSGL_V,"WINSOCK2 init: %i\n", temp);
  }
#endif
  
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
  if(s->close) s->close(s);
  if(s->fd>0){
    /* on unix we define closesocket to close
       on windows however we have to distinguish between
       network socket and file */
    if(s->url && strstr(s->url,"://"))
      closesocket(s->fd);
    else close(s->fd);
  }
#ifdef HAVE_WINSOCK2
  mp_msg(MSGT_STREAM,MSGL_V,"WINSOCK2 uninit\n");
  WSACleanup(); // there might be a better place for this (-> later)
#endif
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
