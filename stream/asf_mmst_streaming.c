/*
 * MMST implementation taken from the xine-mms plugin made by
 * Major MMS (http://geocities.com/majormms/).
 * Ported to MPlayer by Abhijeet Phatak <abhijeetphatak@yahoo.com>.
 *
 * Information about the MMS protocol can be found at http://get.to/sdp
 *
 * copyright (C) 2002 Abhijeet Phatak <abhijeetphatak@yahoo.com>
 * copyright (C) 2002 the xine project
 * copyright (C) 2000-2001 major mms
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#if HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifndef CONFIG_SETLOCALE
#undef CONFIG_ICONV
#endif

#ifdef CONFIG_ICONV
#include <iconv.h>
#ifdef HAVE_LANGINFO
#include <langinfo.h>
#endif
#endif

#include "url.h"
#include "libmpdemux/asf.h"

#include "stream.h"

#include "network.h"
#include "tcp.h"

extern int audio_id;
extern int video_id;

#define BUF_SIZE 102400
#define HDR_BUF_SIZE 8192
#define MAX_STREAMS 20

typedef struct 
{
  uint8_t buf[BUF_SIZE];
  int     num_bytes;

} command_t;

static int seq_num;
static int num_stream_ids;
static int stream_ids[MAX_STREAMS];

static int get_data (int s, char *buf, size_t count);

static void put_32 (command_t *cmd, uint32_t value) 
{
  cmd->buf[cmd->num_bytes  ] = value % 256;
  value = value >> 8;
  cmd->buf[cmd->num_bytes+1] = value % 256 ;
  value = value >> 8;
  cmd->buf[cmd->num_bytes+2] = value % 256 ;
  value = value >> 8;
  cmd->buf[cmd->num_bytes+3] = value % 256 ;

  cmd->num_bytes += 4;
}

static uint32_t get_32 (unsigned char *cmd, int offset) 
{
  uint32_t ret;

  ret = cmd[offset] ;
  ret |= cmd[offset+1]<<8 ;
  ret |= cmd[offset+2]<<16 ;
  ret |= cmd[offset+3]<<24 ;

  return ret;
}

static void send_command (int s, int command, uint32_t switches, 
			  uint32_t extra, int length,
			  char *data) 
{
  command_t  cmd;
  int        len8;

  len8 = (length + 7) / 8;

  cmd.num_bytes = 0;

  put_32 (&cmd, 0x00000001); /* start sequence */
  put_32 (&cmd, 0xB00BFACE); /* #-)) */
  put_32 (&cmd, len8*8 + 32);
  put_32 (&cmd, 0x20534d4d); /* protocol type "MMS " */
  put_32 (&cmd, len8 + 4);
  put_32 (&cmd, seq_num);
  seq_num++;
  put_32 (&cmd, 0x0);        /* unknown */
  put_32 (&cmd, 0x0);
  put_32 (&cmd, len8+2);
  put_32 (&cmd, 0x00030000 | command); /* dir | command */
  put_32 (&cmd, switches);
  put_32 (&cmd, extra);

  memcpy (&cmd.buf[48], data, length);
  if (length & 7)
    memset(&cmd.buf[48 + length], 0, 8 - (length & 7));

  if (send (s, cmd.buf, len8*8+48, 0) != (len8*8+48)) {
    mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_WriteError);
  }
}

#ifdef CONFIG_ICONV
static iconv_t url_conv;
#endif

static void string_utf16(char *dest, char *src, int len) {
    int i;
#ifdef CONFIG_ICONV
    size_t len1, len2;
    char *ip, *op;

    if (url_conv != (iconv_t)(-1))
    {
    memset(dest, 0, 1000);
    len1 = len; len2 = 1000;
    ip = src; op = dest;

    iconv(url_conv, &ip, &len1, &op, &len2);
    }
    else
    {
#endif
	if (len > 499) len = 499;
	for (i=0; i<len; i++) {
	    dest[i*2] = src[i];
	    dest[i*2+1] = 0;
        }
	/* trailing zeroes */
	dest[i*2] = 0;
	dest[i*2+1] = 0;
#ifdef CONFIG_ICONV
    }
#endif
}

static void get_answer (int s) 
{
  char  data[BUF_SIZE];
  int   command = 0x1b;

  while (command == 0x1b) {
    int len;

    len = recv (s, data, BUF_SIZE, 0) ;
    if (!len) {
      mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_EOFAlert);
      return;
    }

    command = get_32 (data, 36) & 0xFFFF;

    if (command == 0x1b) 
      send_command (s, 0x1b, 0, 0, 0, data);
  }
}

static int get_data (int s, char *buf, size_t count) 
{
  ssize_t  len;
  size_t total = 0;

  while (total < count) {

    len = recv (s, &buf[total], count-total, 0);

    if (len<=0) {
      perror ("read error:");
      return 0;
    }

    total += len;

    if (len != 0) {
//      mp_msg(MSGT_NETWORK,MSGL_INFO,"[%d/%d]", total, count);
      fflush (stdout);
    }

  }

  return 1;

}

static int get_header (int s, uint8_t *header, streaming_ctrl_t *streaming_ctrl) 
{
  unsigned char  pre_header[8];
  int            header_len;

  header_len = 0;

  while (1) {
    if (!get_data (s, pre_header, 8)) {
      mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_PreHeaderReadFailed);
      return 0;
    }
    if (pre_header[4] == 0x02) {
      
      int packet_len;
      
      packet_len = (pre_header[7] << 8 | pre_header[6]) - 8;

//      mp_msg(MSGT_NETWORK,MSGL_INFO,"asf header packet detected, len=%d\n", packet_len);

      if (packet_len < 0 || packet_len > HDR_BUF_SIZE - header_len) {
        mp_msg(MSGT_NETWORK, MSGL_FATAL, MSGTR_MPDEMUX_MMST_InvalidHeaderSize);
        return 0;
      }

      if (!get_data (s, &header[header_len], packet_len)) {
	mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_HeaderDataReadFailed);
	return 0;
      }

      header_len += packet_len;

      if ( (header[header_len-1] == 1) && (header[header_len-2]==1)) {
	

     if( streaming_bufferize( streaming_ctrl, header, header_len )<0 ) {
				return -1;
 	 }

     //	mp_msg(MSGT_NETWORK,MSGL_INFO,"get header packet finished\n");

	return header_len;

      } 

    } else {

      int32_t packet_len;
      int command;
      char data[BUF_SIZE];

      if (!get_data (s, (char*)&packet_len, 4)) {
	mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_packet_lenReadFailed);
	return 0;
      }
      
      packet_len = get_32 ((unsigned char*)&packet_len, 0) + 4;
      
//      mp_msg(MSGT_NETWORK,MSGL_INFO,"command packet detected, len=%d\n", packet_len);

      if (packet_len < 0 || packet_len > BUF_SIZE) {
        mp_msg(MSGT_NETWORK, MSGL_FATAL,
                MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize);
        return 0;
      }
      
      if (!get_data (s, data, packet_len)) {
	mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_CmdDataReadFailed);
	return 0;
      }
      
      command = get_32 (data, 24) & 0xFFFF;
      
//      mp_msg(MSGT_NETWORK,MSGL_INFO,"command: %02x\n", command);
      
      if (command == 0x1b) 
	send_command (s, 0x1b, 0, 0, 0, data);
      
    }

//    mp_msg(MSGT_NETWORK,MSGL_INFO,"get header packet succ\n");
  }
}

static int interp_header (uint8_t *header, int header_len) 
{
  int i;
  int packet_length=-1;

  /*
   * parse header
   */

  i = 30;
  while (i<header_len) {
    
    uint64_t  guid_1, guid_2, length;

    guid_2 = (uint64_t)header[i] | ((uint64_t)header[i+1]<<8) 
      | ((uint64_t)header[i+2]<<16) | ((uint64_t)header[i+3]<<24)
      | ((uint64_t)header[i+4]<<32) | ((uint64_t)header[i+5]<<40)
      | ((uint64_t)header[i+6]<<48) | ((uint64_t)header[i+7]<<56);
    i += 8;

    guid_1 = (uint64_t)header[i] | ((uint64_t)header[i+1]<<8) 
      | ((uint64_t)header[i+2]<<16) | ((uint64_t)header[i+3]<<24)
      | ((uint64_t)header[i+4]<<32) | ((uint64_t)header[i+5]<<40)
      | ((uint64_t)header[i+6]<<48) | ((uint64_t)header[i+7]<<56);
    i += 8;
    
//    mp_msg(MSGT_NETWORK,MSGL_INFO,"guid found: %016llx%016llx\n", guid_1, guid_2);

    length = (uint64_t)header[i] | ((uint64_t)header[i+1]<<8) 
      | ((uint64_t)header[i+2]<<16) | ((uint64_t)header[i+3]<<24)
      | ((uint64_t)header[i+4]<<32) | ((uint64_t)header[i+5]<<40)
      | ((uint64_t)header[i+6]<<48) | ((uint64_t)header[i+7]<<56);

    i += 8;

    if ( (guid_1 == 0x6cce6200aa00d9a6ULL) && (guid_2 == 0x11cf668e75b22630ULL) ) {
      mp_msg(MSGT_NETWORK,MSGL_INFO,MSGTR_MPDEMUX_MMST_HeaderObject);
    } else if ((guid_1 == 0x6cce6200aa00d9a6ULL) && (guid_2 == 0x11cf668e75b22636ULL)) {
      mp_msg(MSGT_NETWORK,MSGL_INFO,MSGTR_MPDEMUX_MMST_DataObject);
    } else if ((guid_1 == 0x6553200cc000e48eULL) && (guid_2 == 0x11cfa9478cabdca1ULL)) {

      packet_length = get_32(header, i+92-24);

      mp_msg(MSGT_NETWORK,MSGL_INFO,MSGTR_MPDEMUX_MMST_FileObjectPacketLen,
	      packet_length, get_32(header, i+96-24));


    } else if ((guid_1 == 0x6553200cc000e68eULL) && (guid_2 == 0x11cfa9b7b7dc0791ULL)) {

      int stream_id = header[i+48] | header[i+49] << 8;

      mp_msg(MSGT_NETWORK,MSGL_INFO,MSGTR_MPDEMUX_MMST_StreamObjectStreamID, stream_id);

      if (num_stream_ids < MAX_STREAMS) {
      stream_ids[num_stream_ids] = stream_id;
      num_stream_ids++;
      } else {
        mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_2ManyStreamID);
      }
      
    } else {
#if 0
      int b = i;
      printf ("unknown object (guid: %016llx, %016llx, len: %lld)\n", guid_1, guid_2, length);
      for (; b < length; b++)
      {
        if (isascii(header[b]) || isalpha(header[b]))
	    printf("%c ", header[b]);
	else
    	    printf("%x ", header[b]);
      }
      printf("\n");
#else
      mp_msg(MSGT_NETWORK,MSGL_WARN,MSGTR_MPDEMUX_MMST_UnknownObject);
#endif
    }

//    mp_msg(MSGT_NETWORK,MSGL_INFO,"length    : %lld\n", length);

    i += length-24;

  }

  return packet_length;

}


static int get_media_packet (int s, int padding, streaming_ctrl_t *stream_ctrl) {
  unsigned char  pre_header[8];
  char           data[BUF_SIZE];

  if (!get_data (s, pre_header, 8)) {
    mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_PreHeaderReadFailed);
    return 0;
  }

//  for (i=0; i<8; i++)
//    mp_msg(MSGT_NETWORK,MSGL_INFO,"pre_header[%d] = %02x (%d)\n",
//	    i, pre_header[i], pre_header[i]);

  if (pre_header[4] == 0x04) {

    int packet_len;

    packet_len = (pre_header[7] << 8 | pre_header[6]) - 8;

//    mp_msg(MSGT_NETWORK,MSGL_INFO,"asf media packet detected, len=%d\n", packet_len);

    if (packet_len < 0 || packet_len > BUF_SIZE) {
      mp_msg(MSGT_NETWORK, MSGL_FATAL, MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize);
      return 0;
    }
      
    if (!get_data (s, data, packet_len)) {
      mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_MediaDataReadFailed);
      return 0;
    }

    streaming_bufferize(stream_ctrl, data, padding);

  } else {

    int32_t packet_len;
    int command;

    if (!get_data (s, (char*)&packet_len, 4)) {
      mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_packet_lenReadFailed);
      return 0;
    }

    packet_len = get_32 ((unsigned char*)&packet_len, 0) + 4;

    if (packet_len < 0 || packet_len > BUF_SIZE) {
      mp_msg(MSGT_NETWORK,MSGL_FATAL,MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize);
      return 0;
    }

    if (!get_data (s, data, packet_len)) {
      mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_CmdDataReadFailed);
      return 0;
    }

    if ( (pre_header[7] != 0xb0) || (pre_header[6] != 0x0b)
	 || (pre_header[5] != 0xfa) || (pre_header[4] != 0xce) ) {

      mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_MissingSignature);
      return -1;
    }

    command = get_32 (data, 24) & 0xFFFF;

//    mp_msg(MSGT_NETWORK,MSGL_INFO,"\ncommand packet detected, len=%d  cmd=0x%X\n", packet_len, command);

    if (command == 0x1b) 
      send_command (s, 0x1b, 0, 0, 0, data);
    else if (command == 0x1e) {
      mp_msg(MSGT_NETWORK,MSGL_INFO,MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke);
      return 0;
    }
    else if (command == 0x21 ) {
	// Looks like it's new in WMS9
	// Unknown command, but ignoring it seems to work.
	return 0;
    }
    else if (command != 0x05) {
      mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_UnknownCmd,command);
      return -1;
    }
  }

//  mp_msg(MSGT_NETWORK,MSGL_INFO,"get media packet succ\n");

  return 1;
}


static int packet_length1;

static int asf_mmst_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *stream_ctrl ) 
{
  int len;
  
  while( stream_ctrl->buffer_size==0 ) {
          // buffer is empty - fill it!
	  int ret = get_media_packet( fd, packet_length1, stream_ctrl);
	  if( ret<0 ) {
		  mp_msg(MSGT_NETWORK,MSGL_ERR,MSGTR_MPDEMUX_MMST_GetMediaPacketErr,strerror(errno));
		  return -1;
	  } else if (ret==0) //EOF?
		  return ret;
  }
  
	  len = stream_ctrl->buffer_size-stream_ctrl->buffer_pos;
	  if(len>size) len=size;
	  memcpy( buffer, (stream_ctrl->buffer)+(stream_ctrl->buffer_pos), len );
	  stream_ctrl->buffer_pos += len;
	  if( stream_ctrl->buffer_pos>=stream_ctrl->buffer_size ) {
		  free( stream_ctrl->buffer );
		  stream_ctrl->buffer = NULL;
		  stream_ctrl->buffer_size = 0;
		  stream_ctrl->buffer_pos = 0;
	  }
	  return len;

}

static int asf_mmst_streaming_seek( int fd, off_t pos, streaming_ctrl_t *streaming_ctrl ) 
{
	return -1;
	// Shut up gcc warning
	fd++;
	pos++;
	streaming_ctrl=NULL;
}

int asf_mmst_streaming_start(stream_t *stream)
{
  char                 str[1024];
  char                 data[BUF_SIZE];
  uint8_t              asf_header[HDR_BUF_SIZE];
  int                  asf_header_len;
  int                  len, i, packet_length;
  char                *path, *unescpath;
  URL_t *url1 = stream->streaming_ctrl->url;
  int s = stream->fd;

  if( s>0 ) {
	  closesocket( stream->fd );
	  stream->fd = -1;
  }
  
  /* parse url */
  path = strchr(url1->file,'/') + 1;

  /* mmst filename are not url_escaped by MS MediaPlayer and are expected as
   * "plain text" by the server, so need to decode it here
   */
  unescpath=malloc(strlen(path)+1);
  if (!unescpath) {
	mp_msg(MSGT_NETWORK,MSGL_FATAL,MSGTR_MemAllocFailed);
	return -1; 
  }
  url_unescape_string(unescpath,path);
  path=unescpath;
  

  if( url1->port==0 ) {
	url1->port=1755;
  }
  s = connect2Server( url1->hostname, url1->port, 1);
  if( s<0 ) {
	  free(path);
	  return s;
  }
  mp_msg(MSGT_NETWORK,MSGL_INFO,MSGTR_MPDEMUX_MMST_Connected);
  
  seq_num=0;

  /*
  * Send the initial connect info including player version no. Client GUID (random) and the host address being connected to. 
  * This command is sent at the very start of protocol initiation. It sends local information to the serve 
  * cmd 1 0x01 
  * */

  /* prepare for the url encoding conversion */
#ifdef CONFIG_ICONV
#ifdef HAVE_LANGINFO
  url_conv = iconv_open("UTF-16LE",nl_langinfo(CODESET));
#else
  url_conv = iconv_open("UTF-16LE", NULL);
#endif
#endif

  snprintf (str, 1023, "\034\003NSPlayer/7.0.0.1956; {33715801-BAB3-9D85-24E9-03B90328270A}; Host: %s", url1->hostname);
  string_utf16 (data, str, strlen(str));
// send_command(s, commandno ....)
  send_command (s, 1, 0, 0x0004000b, strlen(str)*2+2, data);

  len = recv (s, data, BUF_SIZE, 0) ;

  /*This sends details of the local machine IP address to a Funnel system at the server. 
  * Also, the TCP or UDP transport selection is sent.
  *
  * here 192.168.0.1 is local ip address TCP/UDP states the tronsport we r using
  * and 1037 is the  local TCP or UDP socket number
  * cmd 2 0x02
  *  */

  string_utf16 (&data[8], "\002\000\\\\192.168.0.1\\TCP\\1037", 24);
  memset (data, 0, 8);
  send_command (s, 2, 0, 0, 24*2+10, data);

  len = recv (s, data, BUF_SIZE, 0) ;

  /* This command sends file path (at server) and file name request to the server.
  * 0x5 */

  string_utf16 (&data[8], path, strlen(path));
  memset (data, 0, 8);
  send_command (s, 5, 0, 0, strlen(path)*2+10, data);
  free(path);

  get_answer (s);

  /* The ASF header chunk request. Includes ?session' variable for pre header value. 
  * After this command is sent, 
  * the server replies with 0x11 command and then the header chunk with header data follows.
  * 0x15 */

  memset (data, 0, 40);
  data[32] = 2;

  send_command (s, 0x15, 1, 0, 40, data);

  num_stream_ids = 0;
  /* get_headers(s, asf_header);  */

  asf_header_len = get_header (s, asf_header, stream->streaming_ctrl);
//  mp_msg(MSGT_NETWORK,MSGL_INFO,"---------------------------------- asf_header %d\n",asf_header);
  if (asf_header_len==0) { //error reading header
    closesocket(s);
    return -1;
  }
  packet_length = interp_header (asf_header, asf_header_len);


  /* 
  * This command is the media stream MBR selector. Switches are always 6 bytes in length.  
  * After all switch elements, data ends with bytes [00 00] 00 20 ac 40 [02]. 
  * Where:  
  * [00 00] shows 0x61 0x00 (on the first 33 sent) or 0xff 0xff for ASF files, and with no ending data for WMV files. 
  * It is not yet understood what all this means. 
  * And the last [02] byte is probably the header ?session' value. 
  *  
  *  0x33 */

  memset (data, 0, 40);

  if (audio_id > 0) {
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = audio_id;
    send_command(s, 0x33, num_stream_ids, 0xFFFF | audio_id << 16, 8, data);
  } else {
  for (i=1; i<num_stream_ids; i++) {
    data [ (i-1) * 6 + 2 ] = 0xFF;
    data [ (i-1) * 6 + 3 ] = 0xFF;
    data [ (i-1) * 6 + 4 ] = stream_ids[i];
    data [ (i-1) * 6 + 5 ] = 0x00;
  }

  send_command (s, 0x33, num_stream_ids, 0xFFFF | stream_ids[0] << 16, (num_stream_ids-1)*6+2 , data);
  }

  get_answer (s);

  /* Start sending file from packet xx. 
  * This command is also used for resume downloads or requesting a lost packet. 
  * Also used for seeking by sending a play point value which seeks to the media time point. 
  * Includes ?session' value in pre header and the maximum media stream time. 
  * 0x07 */

  memset (data, 0, 40);

  for (i=8; i<16; i++)
    data[i] = 0xFF;

  data[20] = 0x04;

  send_command (s, 0x07, 1, 0xFFFF | stream_ids[0] << 16, 24, data);

  stream->fd = s;
  stream->streaming_ctrl->streaming_read = asf_mmst_streaming_read;
  stream->streaming_ctrl->streaming_seek = asf_mmst_streaming_seek;
  stream->streaming_ctrl->buffering = 1;
  stream->streaming_ctrl->status = streaming_playing_e;

  packet_length1 = packet_length;
  mp_msg(MSGT_NETWORK,MSGL_INFO,"mmst packet_length = %d\n", packet_length);

#ifdef CONFIG_ICONV
  if (url_conv != (iconv_t)(-1))
    iconv_close(url_conv);
#endif

  return 0;
}
