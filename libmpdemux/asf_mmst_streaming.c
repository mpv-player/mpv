// mmst implementation taken from the xine-mms plugin made by majormms (http://geocities.com/majormms/)
// 
// ported to mplayer by Abhijeet Phatak <abhijeetphatak@yahoo.com>
// date : 16 April 2002
//
// information about the mms protocol can be find at http://get.to/sdp
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"

#include "url.h"
#include "asf.h"

#include "stream.h"

#include "network.h"

int data_length = 0;
int packet_length1;
int media_padding;
int to_skip = 0;

#include <inttypes.h>

#define BUF_SIZE 102400
int checknum =1;

typedef struct 
{
  uint8_t buf[BUF_SIZE];
  int     num_bytes;

} command_t;

int seq_num;
int num_stream_ids;
int stream_ids[20];
int output_fh;

static int get_data (int s, char *buf, size_t count);
int store_data(int s, int size, char *buffer)
{
//	printf("data_length %d, media_padding %d, size %d \n", data_length, media_padding, size );
	if(data_length >= size)
	{   
		if (!get_data (s, buffer, size)) 
		{
			printf ("media data read failed\n");
			return 0;
		}
		data_length -= size;
		return size;
	}
	else 
	{
		int temp_size, media_temp_padding;
		if(data_length)	
		{
			if (!get_data (s, buffer, data_length)) 
			{
				printf ("media data read failed\n");
				return 0;
			}
		}
		if(media_padding)
		{
			if(media_padding > size - data_length)
			{
				memset(buffer+data_length,0,(size - data_length));
				media_padding -= (size - data_length);
				data_length = 0;
				return size;
			}
			else
			{
				memset(buffer+data_length,0,media_padding);
				media_temp_padding = media_padding;
				media_padding = 0;
				temp_size =data_length;
				data_length = 0;
				return (temp_size + media_temp_padding);
			}
		}
		temp_size = data_length;
		data_length = 0;
		return temp_size;
	}
}

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

  len8 = (length + (length%8)) / 8;

  cmd.num_bytes = 0;

  put_32 (&cmd, 0x00000001); /* start sequence */
  put_32 (&cmd, 0xB00BFACE); /* #-)) */
  put_32 (&cmd, length + 32);
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

  if (write (s, cmd.buf, length+48) != (length+48)) {
    printf ("write error\n");
  }
}

static void string_utf16(char *dest, char *src, int len) 
{
  int i;

  memset (dest, 0, 1000);

  for (i=0; i<len; i++) {
    dest[i*2] = src[i];
    dest[i*2+1] = 0;
  }

  dest[i*2] = 0;
  dest[i*2+1] = 0;
}

static void get_answer (int s) 
{
  char  data[BUF_SIZE];
  int   command = 0x1b;

  while (command == 0x1b) {
    int len;

    len = read (s, data, BUF_SIZE) ;
    if (!len) {
      printf ("\nalert! eof\n");
      return;
    }

    command = get_32 (data, 36) & 0xFFFF;

    if (command == 0x1b) 
      send_command (s, 0x1b, 0, 0, 0, data);
  }
}

static int get_data (int s, char *buf, size_t count) 
{
  ssize_t  len, total;
  total = 0;

  while (total < count) {

    len = read (s, &buf[total], count-total);

    if (len<0) {
      perror ("read error:");
      return 0;
    }

    total += len;

    if (len != 0) {
//      printf ("[%d/%d]", total, count);
      fflush (stdout);
    }

  }

  return 1;

}

static int get_header (int s, uint8_t *header, streaming_ctrl_t *streaming_ctrl) 
{
  unsigned char  pre_header[8];
  int            header_len,i ;

  header_len = 0;

  while (1) {
    if (!get_data (s, pre_header, 8)) {
      printf ("pre-header read failed\n");
      return 0;
    }
    if (pre_header[4] == 0x02) {
      
      int packet_len;
      
      packet_len = (pre_header[7] << 8 | pre_header[6]) - 8;

//      printf ("asf header packet detected, len=%d\n", packet_len);

      if (!get_data (s, &header[header_len], packet_len)) {
	printf ("header data read failed\n");
	return 0;
      }

      header_len += packet_len;

      if ( (header[header_len-1] == 1) && (header[header_len-2]==1)) {
	

     if( streaming_bufferize( streaming_ctrl, header, header_len )<0 ) {
				return -1;
 	 }

     //	printf ("get header packet finished\n");

	return (header_len);

      } 

    } else {

      char packet_len;
      int command;
      char data[BUF_SIZE];

      if (!get_data (s, &packet_len, 4)) {
	printf ("packet_len read failed\n");
	return 0;
      }
      
      packet_len = get_32 (&packet_len, 0) + 4;
      
//      printf ("command packet detected, len=%d\n", packet_len);
      
      if (!get_data (s, data, packet_len)) {
	printf ("command data read failed\n");
	return 0;
      }
      
      command = get_32 (data, 24) & 0xFFFF;
      
//      printf ("command: %02x\n", command);
      
      if (command == 0x1b) 
	send_command (s, 0x1b, 0, 0, 0, data);
      
    }

//    printf ("get header packet succ\n");
  }
}

int interp_header (uint8_t *header, int header_len) 
{
  int i;
  int packet_length;

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
    
//    printf ("guid found: %016llx%016llx\n", guid_1, guid_2);

    length = (uint64_t)header[i] | ((uint64_t)header[i+1]<<8) 
      | ((uint64_t)header[i+2]<<16) | ((uint64_t)header[i+3]<<24)
      | ((uint64_t)header[i+4]<<32) | ((uint64_t)header[i+5]<<40)
      | ((uint64_t)header[i+6]<<48) | ((uint64_t)header[i+7]<<56);

    i += 8;

    if ( (guid_1 == 0x6cce6200aa00d9a6) && (guid_2 == 0x11cf668e75b22630) ) {
      printf ("header object\n");
    } else if ((guid_1 == 0x6cce6200aa00d9a6) && (guid_2 == 0x11cf668e75b22636)) {
      printf ("data object\n");
    } else if ((guid_1 == 0x6553200cc000e48e) && (guid_2 == 0x11cfa9478cabdca1)) {

      packet_length = get_32(header, i+92-24);

      printf ("file object, packet length = %d (%d)\n",
	      packet_length, get_32(header, i+96-24));


    } else if ((guid_1 == 0x6553200cc000e68e) && (guid_2 == 0x11cfa9b7b7dc0791)) {

      int stream_id = header[i+48] | header[i+49] << 8;

      printf ("stream object, stream id: %d\n", stream_id);

      stream_ids[num_stream_ids] = stream_id;
      num_stream_ids++;
      
    } else {
      printf ("unknown object\n");
    }

//    printf ("length    : %lld\n", length);

    i += length-24;

  }

  return packet_length;

}


static int get_media_packet (int s, int padding, char *buffer, int size) 
{
  unsigned char  pre_header[8];
  char           data[BUF_SIZE];
  int 		 CheckInnerData = 1;
  int 		 CheckOuterData = 1;


while(CheckOuterData)
{
	int a;
	if(media_padding ==0 && data_length == 0)
	{
		while(CheckInnerData)
		{
			if (!get_data (s, pre_header, 8)) {
			    printf ("pre-header read failed\n");
			    return 0;
  			}

	 		if (pre_header[4] == 0x04) 
			{
    				data_length = (pre_header[7] << 8 | pre_header[6]) - 8;
				media_padding = packet_length1 - data_length;
				if(to_skip)
				{
					a =  store_data(s, size - to_skip, buffer + to_skip);
					to_skip = 0;
				}
				else
				{
					a = store_data(s, size, buffer);	
				}
//				printf("a inner  %d  \n", size);
				return size;
  			} 
	  		else 
			{
    				int command;
				char packet_len;
    				if (!get_data (s, &packet_len, 4)) 
				{
      					printf ("packet_len read failed\n");
      					return 0;
    				}
	    			packet_len = get_32 (&packet_len, 0) + 4;
	
    				if (!get_data (s, data, packet_len)) 
				{
      					printf ("command data read failed\n");
      					return 0;
    				}
	    			if ( (pre_header[7] != 0xb0) || (pre_header[6] != 0x0b) || (pre_header[5] != 0xfa) || (pre_header[4] != 0xce) ) 
				{
      					printf ("missing signature\n");
      					return -1;
	    			}
    				command = get_32 (data, 24) & 0xFFFF;
	
				if (command == 0x1b)
    				{ 
      					send_command (s, 0x1b, 0, 0, 0, data);
	    			}
    				else if (command == 0x1e) 
				{
      					printf ("everything done. Thank you for downloading a media file containing proprietary and patentend technology.\n");
      					return 0;
	    			} 
				else if (command == 0x21 )
				{
					// Looks like it's new in WMS9
					// Unknown command, but ignoring it seems to work.
					return 0;
				}
				else if (command != 0x05) 
				{
      					printf ("unknown command %02x\n", command);
      					return -1;
	    			}
  			}
		}
	}
	if(to_skip)
	{
		a =  store_data(s, size - to_skip,  buffer+to_skip );
		to_skip = 0;
	}
	else
	{
		a =  store_data(s, size,  buffer);
	}
		
	if(a == size)
	{
//		printf("a outer %d", a );	
		return a;
	}
	else
		to_skip = a;
			
}

}

int
asf_mmst_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *stream_ctrl ) 
{
  int len = 0;
  if( stream_ctrl->buffer_size!=0 ) {
	  int buffer_len = stream_ctrl->buffer_size-stream_ctrl->buffer_pos;
	  len = (size<buffer_len)?size:buffer_len;
	  memcpy( buffer, (stream_ctrl->buffer)+(stream_ctrl->buffer_pos), len );
	  stream_ctrl->buffer_pos += len;
	  if( stream_ctrl->buffer_pos>=stream_ctrl->buffer_size ) {
		  free( stream_ctrl->buffer );
		  stream_ctrl->buffer = NULL;
		  stream_ctrl->buffer_size = 0;
		  stream_ctrl->buffer_pos = 0;
	  }

  }

  if( len<size ) {

	  int ret;

	  ret = get_media_packet( fd, size - len, buffer+len, size-len );

	  if( ret<0 ) {

		  printf("get_media_packet error : %s\n",strerror(errno));
		  return -1;

	  }

	  len += ret;

	  //printf("read %d bytes from network\n", len );

	 }			      

  return len;
}

int
asf_mmst_streaming_seek( int fd, off_t pos, streaming_ctrl_t *streaming_ctrl ) 
{
	return -1;
}

int asf_mmst_streaming_start(stream_t *stream)
{
  char                 str[1024];
  char                 data[1024];
  uint8_t              asf_header[8192];
  int                  asf_header_len;
  int                  len, i, packet_length;
  char                 host[256];
  char                *path;
  URL_t *url1 = stream->streaming_ctrl->url;
  int s = stream->fd;

  if( s>0 ) {
	  close( stream->fd );
	  stream->fd = -1;
  }
  
  /* parse url */
  path = strchr(url1->file,'/') + 1;

  url1->port=1755;
  s = connect2Server( url1->hostname, url1->port );
  if( s<0 ) {
	  return s;
  }
  printf ("connected\n");

  /*
  * Send the initial connect info including player version no. Client GUID (random) and the host address being connected to. 
  * This command is sent at the very start of protocol initiation. It sends local information to the serve 
  * cmd 1 0x01 
  * */

  sprintf (str, "\034\003NSPlayer/7.0.0.1956; {33715801-BAB3-9D85-24E9-03B90328270A}; Host: %s", host);
  string_utf16 (data, str, strlen(str)+2);
// send_command(s, commandno ....)
  send_command (s, 1, 0, 0x0004000b, strlen(str) * 2+8, data);

  len = read (s, data, BUF_SIZE) ;

  /*This sends details of the local machine IP address to a Funnel system at the server. 
  * Also, the TCP or UDP transport selection is sent.
  *
  * here 192.168.0.129 is local ip address TCP/UDP states the tronsport we r using
  * and 1037 is the  local TCP or UDP socket number
  * cmd 2 0x02
  *  */

  string_utf16 (&data[8], "\002\000\\\\192.168.0.1\\TCP\\1037\0000", 
		28);
  memset (data, 0, 8);
  send_command (s, 2, 0, 0, 28*2+8, data);

  len = read (s, data, BUF_SIZE) ;

  /* This command sends file path (at server) and file name request to the server.
  * 0x5 */

  string_utf16 (&data[8], path, strlen(path));
  memset (data, 0, 8);
  send_command (s, 5, 0, 0, strlen(path)*2+12, data);

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
//  printf("---------------------------------- asf_header %d\n",asf_header);
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

  for (i=1; i<num_stream_ids; i++) {
    data [ (i-1) * 6 + 2 ] = 0xFF;
    data [ (i-1) * 6 + 3 ] = 0xFF;
    data [ (i-1) * 6 + 4 ] = stream_ids[i];
    data [ (i-1) * 6 + 5 ] = 0x00;
  }

  send_command (s, 0x33, num_stream_ids, 0xFFFF | stream_ids[0] << 16, (num_stream_ids-1)*6+2 , data);

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
	stream->streaming_ctrl->buffering = 1;

      	packet_length1 = packet_length;

  return 0;
}
