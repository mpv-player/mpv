
/*
 * Common stuff for netstream
 * Packets and so on are defined here along with a few helpers
 * wich are used by both the client and the server
 *
 * Data is always low endian
 */

typedef struct mp_net_stream_packet_st {
  uint16_t len;
  uint8_t cmd;
  char data[0];
} __attribute__ ((packed))  mp_net_stream_packet_t;

#define PACKET_MAX_SIZE 4096

// Commands sent by the client
#define NET_STREAM_OPEN 0
// data is the url
#define NET_STREAM_FILL_BUFFER 1
// data is an uint16 wich is the max len of the data to return
#define NET_STREAM_SEEK 3
// data is an uint64 wich the pos where to seek
#define NET_STREAM_CLOSE 4
// no data
#define NET_STREAM_RESET 5
// no data

// Server response
#define NET_STREAM_OK 128
// Data returned if open is successful
typedef struct mp_net_stream_opened_st {
  uint32_t file_format;
  uint32_t flags;
  uint32_t sector_size;
  uint64_t start_pos;
  uint64_t end_pos;
}  __attribute__ ((packed))  mp_net_stream_opened_t;
// FILL_BUFFER return the data
// CLOSE return nothing
#define NET_STREAM_ERROR 129
// Data is the error message (if any ;)

static int net_read(int fd, char* buf, int len) {
  int r = 0;
  while(len) {
    r = recv(fd,buf,len,0);
    if(r <= 0) {
      if(errno == EINTR) continue;
      if(r < 0)
	mp_msg(MSGT_NETST,MSGL_ERR,"Read failed: %s\n",strerror(errno));
      return 0;
    }
    len -= r;
    buf += r;
  }
  return 1;
}

static mp_net_stream_packet_t* read_packet(int fd) {
  uint16_t len;
  mp_net_stream_packet_t* pack = 
    (mp_net_stream_packet_t*)malloc(sizeof(mp_net_stream_packet_t));
  
  if(!net_read(fd,(char*)pack,sizeof(mp_net_stream_packet_t))) {
    free(pack);
    return NULL;
  }
  pack->len = le2me_16(pack->len);

  if(pack->len < sizeof(mp_net_stream_packet_t)) {
    mp_msg(MSGT_NETST,MSGL_WARN,"Got invalid packet (too small: %d)\n",pack->len);
    free(pack);
    return NULL;
  }
  if(pack->len > PACKET_MAX_SIZE) {
    mp_msg(MSGT_NETST,MSGL_WARN,"Got invalid packet (too big: %d)\n",pack->len);
    free(pack);
    return NULL;
  }
  len = pack->len;
  if(len > sizeof(mp_net_stream_packet_t)) {
    pack = realloc(pack,len);
    if(!pack) {
      mp_msg(MSGT_NETST,MSGL_ERR,"Failed to get memory for the packet (%d bytes)\n",len);
      return NULL;
    }
    if(!net_read(fd,pack->data,len - sizeof(mp_net_stream_packet_t)))
      return NULL;
  }
  //  printf ("Read packet %d %d %d\n",fd,pack->cmd,pack->len);
  return pack;
}

static int net_write(int fd, char* buf, int len) {
  int w;
  while(len) {
    w = send(fd,buf,len,0);
    if(w <= 0) {
      if(errno == EINTR) continue;
      if(w < 0)
	mp_msg(MSGT_NETST,MSGL_ERR,"Write failed: %s\n",strerror(errno));
      return 0;
    }
    len -= w;
    buf += w;
  }
  return 1;
}

static int write_packet(int fd, uint8_t cmd,char* data,int len) {
  mp_net_stream_packet_t* pack = malloc(len + sizeof(mp_net_stream_packet_t));
  
  if(len > 0 && data)
    memcpy(pack->data,data,len);
  pack->len = len + sizeof(mp_net_stream_packet_t);
  pack->cmd = cmd;
  
  //  printf("Write packet %d %d (%p) %d\n",fd,cmd,data,len);
  pack->len = le2me_16(pack->len);
  if(net_write(fd,(char*)pack,pack->len)) {
    free(pack);
    return 1;
  }
  free(pack);
  return 0;
}

static void net_stream_opened_2_me(mp_net_stream_opened_t* o) {
  o->file_format = le2me_32(o->file_format);
  o->flags = le2me_32(o->flags);
  o->sector_size = le2me_32(o->sector_size);
  o->start_pos = le2me_64(o->start_pos);
  o->end_pos = le2me_64(o->end_pos);
}
