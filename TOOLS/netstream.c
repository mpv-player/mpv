/*
 * Copyright (C) Alban Bedel - 04/2003
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include "config.h"

#if !HAVE_WINSOCK2_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "mp_msg.h"
#include "libavutil/common.h"
#include "mpbswap.h"

/// Netstream packets def and some helpers
#include "stream/stream_netstream.h"

// linking hacks
char *info_name;
char *info_artist;
char *info_genre;
char *info_subject;
char *info_copyright;
char *info_sourceform;
char *info_comment;

char* out_filename = NULL;
char* force_fourcc=NULL;
char* passtmpfile="divx2pass.log";

static unsigned short int port = 10000;

typedef struct client_st client_t;
struct client_st {
  int fd;
  stream_t* stream;
  client_t* next;
  client_t* prev;
};

static int write_error(int fd,char* msg) {
  int len = strlen(msg) + 1;
  return write_packet(fd,NET_STREAM_ERROR,msg,len);
}

static int net_stream_open(client_t* cl,char* url) {
  int file_format=DEMUXER_TYPE_UNKNOWN;
  mp_net_stream_opened_t ret;

  if(cl->stream) {
    if(!write_error(cl->fd,"A stream is currently opened\n"))
      return 0;
    return 1;
  }

  mp_msg(MSGT_NETST,MSGL_V,"Open stream %s\n",url);
  cl->stream = open_stream(url,NULL,&file_format);
  if(!cl->stream) {
    if(!write_error(cl->fd,"Open failed\n"))
      return 0;
    return 1;
  }
  stream_reset(cl->stream);
  stream_seek(cl->stream,cl->stream->start_pos);
  ret.file_format = file_format;
  ret.flags = cl->stream->flags;
  ret.sector_size = cl->stream->sector_size;
  ret.start_pos = cl->stream->start_pos;
  ret.end_pos = cl->stream->end_pos;
  net_stream_opened_2_me(&ret);

  if(!write_packet(cl->fd,NET_STREAM_OK,(char*)&ret,sizeof(mp_net_stream_opened_t)))
    return 0;
  return 1;
}

static int net_stream_fill_buffer(client_t* cl,uint16_t max_len) {
  int r;
  mp_net_stream_packet_t *pack;
  
  if(!cl->stream) {
    if(!write_error(cl->fd,"No stream is currently opened\n"))
      return 0;
    return 1;
  }
  if(max_len == 0) {
    if(!write_error(cl->fd,"Fill buffer called with 0 length\n"))
      return 0;
    return 1;
  }
  pack = malloc(max_len + sizeof(mp_net_stream_packet_t));
  pack->cmd = NET_STREAM_OK;
  r = stream_read(cl->stream,pack->data,max_len);
  pack->len = le2me_16(r + sizeof(mp_net_stream_packet_t));
  if(!net_write(cl->fd,(char*)pack,le2me_16(pack->len))) {
    free(pack);
    return 0;
  }
  free(pack);
  return 1;
}

static int net_stream_seek(client_t* cl, uint64_t pos) {
  
  if(!cl->stream) {
    if(!write_error(cl->fd,"No stream is currently opened\n"))
      return 0;
    return 1;
  }

  if(!stream_seek(cl->stream,(off_t)pos)) {
    if(!write_error(cl->fd,"Seek failed\n"))
      return 0;
    return 1;
  }
  if(!write_packet(cl->fd,NET_STREAM_OK,NULL,0))
    return 0;
  return 1;
}

static int net_stream_reset(client_t* cl) {
  if(!cl->stream) {
    if(!write_error(cl->fd,"No stream is currently opened\n"))
      return 0;
    return 1;
  }
  stream_reset(cl->stream);
  if(!write_packet(cl->fd,NET_STREAM_OK,NULL,0))
    return 0;
  return 1;
}

static int net_stream_close(client_t* cl) {
  if(!cl->stream) {
    if(!write_error(cl->fd,"No stream is currently opened\n"))
      return 0;
    return 1;
  }

  free_stream(cl->stream);
  cl->stream = NULL;

  if(!write_packet(cl->fd,NET_STREAM_OK,NULL,0))
    return 0;
  return 1;
}

static int handle_client(client_t* cl,mp_net_stream_packet_t* pack) {

  if(!pack)
    return 0;
 
  switch(pack->cmd) {
  case NET_STREAM_OPEN:
    if(((char*)pack)[pack->len-1] != '\0') {
      mp_msg(MSGT_NETST,MSGL_WARN,"Got invalid open packet\n");
      return 0;
    }
    return net_stream_open(cl,pack->data);
  case NET_STREAM_FILL_BUFFER:
    if(pack->len != sizeof(mp_net_stream_packet_t) + 2) {
      mp_msg(MSGT_NETST,MSGL_WARN,"Got invalid fill buffer packet\n");
      return 0;
    }
    return net_stream_fill_buffer(cl,le2me_16(*((uint16_t*)pack->data)));
  case NET_STREAM_SEEK:
    if(pack->len != sizeof(mp_net_stream_packet_t) + 8) {
      mp_msg(MSGT_NETST,MSGL_WARN,"Got invalid fill buffer packet\n");
      return 0;
    }
    return net_stream_seek(cl,le2me_64(*((uint64_t*)pack->data)));
  case NET_STREAM_RESET:
    return net_stream_reset(cl);
  case NET_STREAM_CLOSE:
    if(pack->len != sizeof(mp_net_stream_packet_t)){
      mp_msg(MSGT_NETST,MSGL_WARN,"Got invalid fill buffer packet\n");
      return 0;
    }
    return net_stream_close(cl);
  default:
    mp_msg(MSGT_NETST,MSGL_WARN,"Got unknown command %d\n",pack->cmd);
    if(!write_error(cl->fd,"Unknown command\n"))
      return 0;
  }
  return 0;
}

static client_t* add_client(client_t *head,int fd) {
  client_t *new = calloc(1,sizeof(client_t));
  new->fd = fd;
  if(!head) return new;
  new->next = head;
  head->prev = new;
  return new;
}

static int make_fd_set(fd_set* fds, client_t** _cl, int listen) {
  int max_fd = listen;
  client_t *cl = *_cl;
  FD_ZERO(fds);
  FD_SET(listen,fds);
  while(cl) {
    // Remove this client
    if(cl->fd < 0) {
      client_t* f = cl;
      if(cl->prev) cl->prev->next = cl->next;
      if(cl->next) cl->next->prev = cl->prev;
      if(cl->stream) free_stream(cl->stream);
      if(!cl->prev) // Remove the head
	*_cl = cl->next;
      cl = cl->next;  
      free(f);
      continue;
    }
    FD_SET(cl->fd,fds);
    if(cl->fd > max_fd) max_fd = cl->fd;
    cl = cl->next;
  }
  return max_fd+1;
}

/// Hack to 'cleanly' exit
static int run_server = 1;

void exit_sig(int sig) {
  static int count = 0;
  sig++; // gcc warning
  count++;
  if(count==3) exit(1);
  if(count > 3)
#ifdef __MINGW32__
    WSACleanup();
#else
    kill(getpid(),SIGKILL);
#endif
  run_server = 0;
}

static int main_loop(int listen_fd) {
  client_t *clients = NULL,*iter;
  fd_set fds;

  signal(SIGTERM,exit_sig); // kill
#ifndef __MINGW32__
  signal(SIGHUP,exit_sig);  // kill -HUP  /  xterm closed
  signal(SIGINT,exit_sig);  // Interrupt from keyboard
  signal(SIGQUIT,exit_sig); // Quit from keyboard
#endif 

  while(run_server) {
    int sel_n = make_fd_set(&fds,&clients,listen_fd);
    int n = select(sel_n,&fds,NULL,NULL,NULL);
    if(n < 0) {
      if(errno == EINTR)
	continue;
      mp_msg(MSGT_NETST,MSGL_FATAL,"Select error: %s\n",strerror(errno));
      return 1;
    }
    // New connection
    if(FD_ISSET(listen_fd,&fds)) {
      struct sockaddr_in addr;
      socklen_t slen = sizeof(struct sockaddr_in);
      int client_fd = accept(listen_fd,(struct sockaddr*)&addr,&slen);
      if(client_fd < 0) {
	mp_msg(MSGT_NETST,MSGL_ERR,"accept failed: %s\n",strerror(errno));
	continue;
      }
      mp_msg(MSGT_NETST,MSGL_V,"New client from %s\n",inet_ntoa(addr.sin_addr));
      clients = add_client(clients,client_fd);
      if(n == 1) continue;
    }
    // Look for the clients
    for(iter = clients ; iter ; iter = iter->next) {
      mp_net_stream_packet_t* pack;
      if(!FD_ISSET(iter->fd,&fds)) continue;
      pack = read_packet(iter->fd);
      if(!pack) {
	close(iter->fd);
	iter->fd = -1;
	continue;
      }
      if(!handle_client(iter,pack)) {
	close(iter->fd);
	iter->fd = -1;
      }
      free(pack);
    }
  }
  mp_msg(MSGT_NETST,MSGL_INFO,"Exit ....\n");
  close(listen_fd);
#ifdef __MINGW32__
  WSACleanup();
#endif
  while(clients) {
    client_t* f = clients;
    if(f->stream) free_stream(f->stream);
    if(f->fd > 0) close(f->fd);
    free(f);
    clients = clients->next;
  }
  return 0;
}

int main(void) {
  int listen_fd;
  struct sockaddr_in addr;

  mp_msg_init();
  //  mp_msg_set_level(verbose+MSGL_STATUS);
  
#ifdef __MINGW32__
  WSADATA wsaData;
  WSAStartup(MAKEWORD(1,1), &wsaData);
#endif
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(listen_fd < 0) {
    mp_msg(MSGT_NETST,MSGL_FATAL,"Failed to create listen_fd: %s\n",strerror(errno));
    return -1;
  }
  memset(&addr,0,sizeof(struct sockaddr));
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  addr.sin_family = AF_INET;
  if(bind(listen_fd,(struct sockaddr*)&addr,sizeof(struct sockaddr))) {
    mp_msg(MSGT_NETST,MSGL_FATAL,"Failed to bind listen socket: %s\n",strerror(errno));
    return -1;
  }
  

  if(listen(listen_fd,1)) {
    mp_msg(MSGT_NETST,MSGL_FATAL,"Failed to turn the socket in listen state: %s\n",strerror(errno));
    return -1;
  }
  return main_loop(listen_fd);
}
