/*
 * This file was ported to MPlayer from xine CVS rmff.c,v 1.3 2002/12/24 01:30:22
 */

/*
 * Copyright (C) 2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * functions for real media file format
 * adopted from joschkas real tools
 */

#include "rmff.h"
#include "xbuffer.h"

/*
#define LOG
*/

#define BE_16(x)  ((((uint8_t*)(x))[0] << 8) | ((uint8_t*)(x))[1])
#define BE_32(x)  ((((uint8_t*)(x))[0] << 24) | \
                   (((uint8_t*)(x))[1] << 16) | \
                   (((uint8_t*)(x))[2] << 8) | \
                    ((uint8_t*)(x))[3])

static void hexdump (const char *buf, int length) {

  int i;

  printf ("rmff: ascii>");
  for (i = 0; i < length; i++) {
    unsigned char c = buf[i];

    if ((c >= 32) && (c <= 128))
      printf ("%c", c);
    else
      printf (".");
  }
  printf ("\n");

  printf ("rmff: hexdump> ");
  for (i = 0; i < length; i++) {
    unsigned char c = buf[i];

    printf ("%02x", c);

    if ((i % 16) == 15)
      printf ("\nrmff:         ");

    if ((i % 2) == 1)
      printf (" ");

  }
  printf ("\n");
}

/*
 * writes header data to a buffer
 */

static void rmff_dump_fileheader(rmff_fileheader_t *fileheader, char *buffer) {

  if (!fileheader) return;
  fileheader->object_id=BE_32(&fileheader->object_id);
  fileheader->size=BE_32(&fileheader->size);
  fileheader->object_version=BE_16(&fileheader->object_version);
  fileheader->file_version=BE_32(&fileheader->file_version);
  fileheader->num_headers=BE_32(&fileheader->num_headers);
  
  memcpy(buffer, fileheader, 8);
  memcpy(&buffer[8], &fileheader->object_version, 2);
  memcpy(&buffer[10], &fileheader->file_version, 8);

  fileheader->size=BE_32(&fileheader->size);
  fileheader->object_version=BE_16(&fileheader->object_version);
  fileheader->file_version=BE_32(&fileheader->file_version);
  fileheader->num_headers=BE_32(&fileheader->num_headers);
  fileheader->object_id=BE_32(&fileheader->object_id);
}

static void rmff_dump_prop(rmff_prop_t *prop, char *buffer) {

  if (!prop) return;
  prop->object_id=BE_32(&prop->object_id);
  prop->size=BE_32(&prop->size);
  prop->object_version=BE_16(&prop->object_version);
  prop->max_bit_rate=BE_32(&prop->max_bit_rate);
  prop->avg_bit_rate=BE_32(&prop->avg_bit_rate);
  prop->max_packet_size=BE_32(&prop->max_packet_size);
  prop->avg_packet_size=BE_32(&prop->avg_packet_size);
  prop->num_packets=BE_32(&prop->num_packets);
  prop->duration=BE_32(&prop->duration);
  prop->preroll=BE_32(&prop->preroll);
  prop->index_offset=BE_32(&prop->index_offset);
  prop->data_offset=BE_32(&prop->data_offset);
  prop->num_streams=BE_16(&prop->num_streams);
  prop->flags=BE_16(&prop->flags);

  memcpy(buffer, prop, 8);
  memcpy(&buffer[8], &prop->object_version, 2);
  memcpy(&buffer[10], &prop->max_bit_rate, 36);
  memcpy(&buffer[46], &prop->num_streams, 2);
  memcpy(&buffer[48], &prop->flags, 2);
  
  prop->size=BE_32(&prop->size);
  prop->object_version=BE_16(&prop->object_version);
  prop->max_bit_rate=BE_32(&prop->max_bit_rate);
  prop->avg_bit_rate=BE_32(&prop->avg_bit_rate);
  prop->max_packet_size=BE_32(&prop->max_packet_size);
  prop->avg_packet_size=BE_32(&prop->avg_packet_size);
  prop->num_packets=BE_32(&prop->num_packets);
  prop->duration=BE_32(&prop->duration);
  prop->preroll=BE_32(&prop->preroll);
  prop->index_offset=BE_32(&prop->index_offset);
  prop->data_offset=BE_32(&prop->data_offset);
  prop->num_streams=BE_16(&prop->num_streams);
  prop->flags=BE_16(&prop->flags);
  prop->object_id=BE_32(&prop->object_id);
}

static void rmff_dump_mdpr(rmff_mdpr_t *mdpr, char *buffer) {

  int s1, s2, s3;

  if (!mdpr) return;
  mdpr->object_id=BE_32(&mdpr->object_id);
  mdpr->size=BE_32(&mdpr->size);
  mdpr->object_version=BE_16(&mdpr->object_version);
  mdpr->stream_number=BE_16(&mdpr->stream_number);
  mdpr->max_bit_rate=BE_32(&mdpr->max_bit_rate);
  mdpr->avg_bit_rate=BE_32(&mdpr->avg_bit_rate);
  mdpr->max_packet_size=BE_32(&mdpr->max_packet_size);
  mdpr->avg_packet_size=BE_32(&mdpr->avg_packet_size);
  mdpr->start_time=BE_32(&mdpr->start_time);
  mdpr->preroll=BE_32(&mdpr->preroll);
  mdpr->duration=BE_32(&mdpr->duration);

  memcpy(buffer, mdpr, 8);
  memcpy(&buffer[8], &mdpr->object_version, 2);
  memcpy(&buffer[10], &mdpr->stream_number, 2);
  memcpy(&buffer[12], &mdpr->max_bit_rate, 28);
  memcpy(&buffer[40], &mdpr->stream_name_size, 1);
  s1=mdpr->stream_name_size;
  memcpy(&buffer[41], mdpr->stream_name, s1);

  memcpy(&buffer[41+s1], &mdpr->mime_type_size, 1);
  s2=mdpr->mime_type_size;
  memcpy(&buffer[42+s1], mdpr->mime_type, s2);
  
  mdpr->type_specific_len=BE_32(&mdpr->type_specific_len);
  memcpy(&buffer[42+s1+s2], &mdpr->type_specific_len, 4);
  mdpr->type_specific_len=BE_32(&mdpr->type_specific_len);
  s3=mdpr->type_specific_len;
  memcpy(&buffer[46+s1+s2], mdpr->type_specific_data, s3);

  mdpr->size=BE_32(&mdpr->size);
  mdpr->stream_number=BE_16(&mdpr->stream_number);
  mdpr->max_bit_rate=BE_32(&mdpr->max_bit_rate);
  mdpr->avg_bit_rate=BE_32(&mdpr->avg_bit_rate);
  mdpr->max_packet_size=BE_32(&mdpr->max_packet_size);
  mdpr->avg_packet_size=BE_32(&mdpr->avg_packet_size);
  mdpr->start_time=BE_32(&mdpr->start_time);
  mdpr->preroll=BE_32(&mdpr->preroll);
  mdpr->duration=BE_32(&mdpr->duration);
  mdpr->object_id=BE_32(&mdpr->object_id);

}

static void rmff_dump_cont(rmff_cont_t *cont, char *buffer) {

  int p;

  if (!cont) return;
  cont->object_id=BE_32(&cont->object_id);
  cont->size=BE_32(&cont->size);
  cont->object_version=BE_16(&cont->object_version);

  memcpy(buffer, cont, 8);
  memcpy(&buffer[8], &cont->object_version, 2);
  
  cont->title_len=BE_16(&cont->title_len);
  memcpy(&buffer[10], &cont->title_len, 2);
  cont->title_len=BE_16(&cont->title_len);
  memcpy(&buffer[12], cont->title, cont->title_len);
  p=12+cont->title_len;

  cont->author_len=BE_16(&cont->author_len);
  memcpy(&buffer[p], &cont->author_len, 2);
  cont->author_len=BE_16(&cont->author_len);
  memcpy(&buffer[p+2], cont->author, cont->author_len);
  p+=2+cont->author_len;

  cont->copyright_len=BE_16(&cont->copyright_len);
  memcpy(&buffer[p], &cont->copyright_len, 2);
  cont->copyright_len=BE_16(&cont->copyright_len);
  memcpy(&buffer[p+2], cont->copyright, cont->copyright_len);
  p+=2+cont->copyright_len;

  cont->comment_len=BE_16(&cont->comment_len);
  memcpy(&buffer[p], &cont->comment_len, 2);
  cont->comment_len=BE_16(&cont->comment_len);
  memcpy(&buffer[p+2], cont->comment, cont->comment_len);

  cont->size=BE_32(&cont->size);
  cont->object_version=BE_16(&cont->object_version);
  cont->object_id=BE_32(&cont->object_id);
}

static void rmff_dump_dataheader(rmff_data_t *data, char *buffer) {

  if (!data) return;
  data->object_id=BE_32(&data->object_id);
  data->size=BE_32(&data->size);
  data->object_version=BE_16(&data->object_version);
  data->num_packets=BE_32(&data->num_packets);
  data->next_data_header=BE_32(&data->next_data_header);

  memcpy(buffer, data, 8);
  memcpy(&buffer[8], &data->object_version, 2);
  memcpy(&buffer[10], &data->num_packets, 8);
  
  data->num_packets=BE_32(&data->num_packets);
  data->next_data_header=BE_32(&data->next_data_header);
  data->size=BE_32(&data->size);
  data->object_version=BE_16(&data->object_version);
  data->object_id=BE_32(&data->object_id);
}

int rmff_dump_header(rmff_header_t *h, char *buffer, int max) {

  int written=0;
  rmff_mdpr_t **stream=h->streams;

  rmff_dump_fileheader(h->fileheader, &buffer[written]);
  written+=h->fileheader->size;
  rmff_dump_prop(h->prop, &buffer[written]);
  written+=h->prop->size;
  rmff_dump_cont(h->cont, &buffer[written]);
  written+=h->cont->size;
  if (stream)
  {
    while(*stream)
    {
      rmff_dump_mdpr(*stream, &buffer[written]);
      written+=(*stream)->size;
      stream++;
    }
  }
    
  rmff_dump_dataheader(h->data, &buffer[written]);
  written+=18;

  return written;
}

void rmff_dump_pheader(rmff_pheader_t *h, char *data) {

  data[0]=(h->object_version>>8) & 0xff;
  data[1]=h->object_version & 0xff;
  data[2]=(h->length>>8) & 0xff;
  data[3]=h->length & 0xff;
  data[4]=(h->stream_number>>8) & 0xff;
  data[5]=h->stream_number & 0xff;
  data[6]=(h->timestamp>>24) & 0xff;
  data[7]=(h->timestamp>>16) & 0xff;
  data[8]=(h->timestamp>>8) & 0xff;
  data[9]=h->timestamp & 0xff;
  data[10]=h->reserved;
  data[11]=h->flags;
}

static rmff_fileheader_t *rmff_scan_fileheader(const char *data) {

  rmff_fileheader_t *fileheader=malloc(sizeof(rmff_fileheader_t));

  fileheader->object_id=BE_32(data);
  fileheader->size=BE_32(&data[4]);
  fileheader->object_version=BE_16(&data[8]);
  if (fileheader->object_version != 0)
  {
    printf("warning: unknown object version in .RMF: 0x%04x\n",
      fileheader->object_version);
  }
  fileheader->file_version=BE_32(&data[10]);
  fileheader->num_headers=BE_32(&data[14]);

  return fileheader;
}

static rmff_prop_t *rmff_scan_prop(const char *data) {

  rmff_prop_t *prop=malloc(sizeof(rmff_prop_t));

  prop->object_id=BE_32(data);
  prop->size=BE_32(&data[4]);
  prop->object_version=BE_16(&data[8]);
  if (prop->object_version != 0)
  {
    printf("warning: unknown object version in PROP: 0x%04x\n",
      prop->object_version);
  }
  prop->max_bit_rate=BE_32(&data[10]);
  prop->avg_bit_rate=BE_32(&data[14]);
  prop->max_packet_size=BE_32(&data[18]);
  prop->avg_packet_size=BE_32(&data[22]);
  prop->num_packets=BE_32(&data[26]);
  prop->duration=BE_32(&data[30]);
  prop->preroll=BE_32(&data[34]);
  prop->index_offset=BE_32(&data[38]);
  prop->data_offset=BE_32(&data[42]);
  prop->num_streams=BE_16(&data[46]);
  prop->flags=BE_16(&data[48]);

  return prop;
}

static rmff_mdpr_t *rmff_scan_mdpr(const char *data) {

  rmff_mdpr_t *mdpr=malloc(sizeof(rmff_mdpr_t));

  mdpr->object_id=BE_32(data);
  mdpr->size=BE_32(&data[4]);
  mdpr->object_version=BE_16(&data[8]);
  if (mdpr->object_version != 0)
  {
    printf("warning: unknown object version in MDPR: 0x%04x\n",
      mdpr->object_version);
  }
  mdpr->stream_number=BE_16(&data[10]);
  mdpr->max_bit_rate=BE_32(&data[12]);
  mdpr->avg_bit_rate=BE_32(&data[16]);
  mdpr->max_packet_size=BE_32(&data[20]);
  mdpr->avg_packet_size=BE_32(&data[24]);
  mdpr->start_time=BE_32(&data[28]);
  mdpr->preroll=BE_32(&data[32]);
  mdpr->duration=BE_32(&data[36]);
  
  mdpr->stream_name_size=data[40];
  mdpr->stream_name=malloc(sizeof(char)*(mdpr->stream_name_size+1));
  memcpy(mdpr->stream_name, &data[41], mdpr->stream_name_size);
  mdpr->stream_name[mdpr->stream_name_size]=0;
  
  mdpr->mime_type_size=data[41+mdpr->stream_name_size];
  mdpr->mime_type=malloc(sizeof(char)*(mdpr->mime_type_size+1));
  memcpy(mdpr->mime_type, &data[42+mdpr->stream_name_size], mdpr->mime_type_size);
  mdpr->mime_type[mdpr->mime_type_size]=0;
  
  mdpr->type_specific_len=BE_32(&data[42+mdpr->stream_name_size+mdpr->mime_type_size]);
  mdpr->type_specific_data=malloc(sizeof(char)*(mdpr->type_specific_len));
  memcpy(mdpr->type_specific_data, 
      &data[46+mdpr->stream_name_size+mdpr->mime_type_size], mdpr->type_specific_len);
  
  return mdpr;
}

static rmff_cont_t *rmff_scan_cont(const char *data) {

  rmff_cont_t *cont=malloc(sizeof(rmff_cont_t));
  int pos;

  cont->object_id=BE_32(data);
  cont->size=BE_32(&data[4]);
  cont->object_version=BE_16(&data[8]);
  if (cont->object_version != 0)
  {
    printf("warning: unknown object version in CONT: 0x%04x\n",
      cont->object_version);
  }
  cont->title_len=BE_16(&data[10]);
  cont->title=malloc(sizeof(char)*(cont->title_len+1));
  memcpy(cont->title, &data[12], cont->title_len);
  cont->title[cont->title_len]=0;
  pos=cont->title_len+12;
  cont->author_len=BE_16(&data[pos]);
  cont->author=malloc(sizeof(char)*(cont->author_len+1));
  memcpy(cont->author, &data[pos+2], cont->author_len);
  cont->author[cont->author_len]=0;
  pos=pos+2+cont->author_len;
  cont->copyright_len=BE_16(&data[pos]);
  cont->copyright=malloc(sizeof(char)*(cont->copyright_len+1));
  memcpy(cont->copyright, &data[pos+2], cont->copyright_len);
  cont->copyright[cont->copyright_len]=0;
  pos=pos+2+cont->copyright_len;
  cont->comment_len=BE_16(&data[pos]);
  cont->comment=malloc(sizeof(char)*(cont->comment_len+1));
  memcpy(cont->comment, &data[pos+2], cont->comment_len);
  cont->comment[cont->comment_len]=0;

  return cont;
}

static rmff_data_t *rmff_scan_dataheader(const char *data) {

  rmff_data_t *dh=malloc(sizeof(rmff_data_t));

  dh->object_id=BE_32(data);
  dh->size=BE_32(&data[4]);
  dh->object_version=BE_16(&data[8]);
  if (dh->object_version != 0)
  {
    printf("warning: unknown object version in DATA: 0x%04x\n",
      dh->object_version);
  }
  dh->num_packets=BE_32(&data[10]);
  dh->next_data_header=BE_32(&data[14]);

  return dh;
}
 
rmff_header_t *rmff_scan_header(const char *data) {

	rmff_header_t *header=malloc(sizeof(rmff_header_t));
	rmff_mdpr_t   *mdpr=NULL;
	int           chunk_size;
	uint32_t      chunk_type;
  const char    *ptr=data;
  int           i;

  header->fileheader=NULL;
	header->prop=NULL;
	header->cont=NULL;
	header->data=NULL;

  chunk_type = BE_32(ptr);
  if (chunk_type != RMF_TAG)
  {
    printf("rmff: not an real media file header (.RMF tag not found).\n");
    free(header);
    return NULL;
  }
  header->fileheader=rmff_scan_fileheader(ptr);
  ptr += header->fileheader->size;
	
	header->streams=malloc(sizeof(rmff_mdpr_t*)*(header->fileheader->num_headers));
  for (i=0; i<header->fileheader->num_headers; i++) {
    header->streams[i]=NULL;
  }
  
  for (i=1; i<header->fileheader->num_headers; i++) {
    chunk_type = BE_32(ptr);
  
    if (ptr[0] == 0)
    {
      printf("rmff: warning: only %d of %d header found.\n", i, header->fileheader->num_headers);
      break;
    }
    
    chunk_size=1;
    switch (chunk_type) {
    case PROP_TAG:
      header->prop=rmff_scan_prop(ptr);
      chunk_size=header->prop->size;
      break;
    case MDPR_TAG:
      mdpr=rmff_scan_mdpr(ptr);
      chunk_size=mdpr->size;
      header->streams[mdpr->stream_number]=mdpr;
      break;
    case CONT_TAG:
      header->cont=rmff_scan_cont(ptr);
      chunk_size=header->cont->size;
      break;
    case DATA_TAG:
      header->data=rmff_scan_dataheader(ptr);
      chunk_size=34;     /* hard coded header size */
      break;
    default:
      printf("unknown chunk\n");
      hexdump(ptr,10);
      chunk_size=1;
      break;
    }
    ptr+=chunk_size;
  }

	return header;
}

rmff_header_t *rmff_scan_header_stream(int fd) {

  rmff_header_t *header;
  char *buf=xbuffer_init(1024);
  int index=0;
  uint32_t chunk_type;
  uint32_t chunk_size;

  do {
    buf = xbuffer_ensure_size(buf, index+8);
    recv(fd, buf+index, 8, 0);
    chunk_type=BE_32(buf+index); index+=4;
    chunk_size=BE_32(buf+index); index+=4;

    switch (chunk_type) {
      case DATA_TAG:
        chunk_size=18;
      case MDPR_TAG:
      case CONT_TAG:
      case RMF_TAG:
      case PROP_TAG:
        buf = xbuffer_ensure_size(buf, index+chunk_size-8);
        recv(fd, buf+index, (chunk_size-8), 0);
	index+=(chunk_size-8);
        break;
      default:
        printf("rmff_scan_header_stream: unknown chunk");
        hexdump(buf+index-8, 8);
        chunk_type=DATA_TAG;
    }
  } while (chunk_type != DATA_TAG);

  header = rmff_scan_header(buf);

  xbuffer_free(buf);

  return header;
}

void rmff_scan_pheader(rmff_pheader_t *h, char *data) {

  h->object_version=BE_16(data);
  h->length=BE_16(data+2);
  h->stream_number=BE_16(data+4);
  h->timestamp=BE_32(data+6);
  h->reserved=(uint8_t)data[10];
  h->flags=(uint8_t)data[11];
}

rmff_fileheader_t *rmff_new_fileheader(uint32_t num_headers) {

  rmff_fileheader_t *fileheader=malloc(sizeof(rmff_fileheader_t));

  fileheader->object_id=RMF_TAG;
  fileheader->size=18;
  fileheader->object_version=0;
  fileheader->file_version=0;
  fileheader->num_headers=num_headers;

  return fileheader;
}

rmff_prop_t *rmff_new_prop (
    uint32_t max_bit_rate,
    uint32_t avg_bit_rate,
    uint32_t max_packet_size,
    uint32_t avg_packet_size,
    uint32_t num_packets,
    uint32_t duration,
    uint32_t preroll,
    uint32_t index_offset,
    uint32_t data_offset,
    uint16_t num_streams,
    uint16_t flags ) {

  rmff_prop_t *prop=malloc(sizeof(rmff_prop_t));

  prop->object_id=PROP_TAG;
  prop->size=50;
  prop->object_version=0;

  prop->max_bit_rate=max_bit_rate;
  prop->avg_bit_rate=avg_bit_rate;
  prop->max_packet_size=max_packet_size;
  prop->avg_packet_size=avg_packet_size;
  prop->num_packets=num_packets;
  prop->duration=duration;
  prop->preroll=preroll;
  prop->index_offset=index_offset;
  prop->data_offset=data_offset;
  prop->num_streams=num_streams;
  prop->flags=flags;
   
  return prop;
}

rmff_mdpr_t *rmff_new_mdpr(
      uint16_t   stream_number,
      uint32_t   max_bit_rate,
      uint32_t   avg_bit_rate,
      uint32_t   max_packet_size,
      uint32_t   avg_packet_size,
      uint32_t   start_time,
      uint32_t   preroll,
      uint32_t   duration,
      const char *stream_name,
      const char *mime_type,
      uint32_t   type_specific_len,
      const char *type_specific_data ) {

  rmff_mdpr_t *mdpr=malloc(sizeof(rmff_mdpr_t));
  
  mdpr->object_id=MDPR_TAG;
  mdpr->object_version=0;

  mdpr->stream_number=stream_number;
  mdpr->max_bit_rate=max_bit_rate;
  mdpr->avg_bit_rate=avg_bit_rate;
  mdpr->max_packet_size=max_packet_size;
  mdpr->avg_packet_size=avg_packet_size;
  mdpr->start_time=start_time;
  mdpr->preroll=preroll;
  mdpr->duration=duration;
  mdpr->stream_name_size=0;
  if (stream_name) {
    mdpr->stream_name=strdup(stream_name);
    mdpr->stream_name_size=strlen(stream_name);
  }
  mdpr->mime_type_size=0;
  if (mime_type) {
    mdpr->mime_type=strdup(mime_type);
    mdpr->mime_type_size=strlen(mime_type);
  }
  mdpr->type_specific_len=type_specific_len;
  mdpr->type_specific_data=malloc(sizeof(char)*type_specific_len);
  memcpy(mdpr->type_specific_data,type_specific_data,type_specific_len);
  mdpr->mlti_data=NULL;
  
  mdpr->size=mdpr->stream_name_size+mdpr->mime_type_size+mdpr->type_specific_len+46;

  return mdpr;
}

rmff_cont_t *rmff_new_cont(const char *title, const char *author, const char *copyright, const char *comment) {

  rmff_cont_t *cont=malloc(sizeof(rmff_cont_t));

  cont->object_id=CONT_TAG;
  cont->object_version=0;

  cont->title=NULL;
  cont->author=NULL;
  cont->copyright=NULL;
  cont->comment=NULL;
  
  cont->title_len=0;
  cont->author_len=0;
  cont->copyright_len=0;
  cont->comment_len=0;

  if (title) {
    cont->title_len=strlen(title);
    cont->title=strdup(title);
  }
  if (author) {
    cont->author_len=strlen(author);
    cont->author=strdup(author);
  }
  if (copyright) {
    cont->copyright_len=strlen(copyright);
    cont->copyright=strdup(copyright);
  }
  if (comment) {
    cont->comment_len=strlen(comment);
    cont->comment=strdup(comment);
  }
  cont->size=cont->title_len+cont->author_len+cont->copyright_len+cont->comment_len+18;

  return cont;
}

rmff_data_t *rmff_new_dataheader(uint32_t num_packets, uint32_t next_data_header) {

  rmff_data_t *data=malloc(sizeof(rmff_data_t));

  data->object_id=DATA_TAG;
  data->size=18;
  data->object_version=0;
  data->num_packets=num_packets;
  data->next_data_header=next_data_header;

  return data;
}
  
void rmff_print_header(rmff_header_t *h) {

  rmff_mdpr_t **stream;
  
  if(!h) {
    printf("rmff_print_header: NULL given\n");
    return;
  }
  if(h->fileheader)
  {
    printf("\nFILE:\n");
    printf("file version      : %d\n", h->fileheader->file_version);
    printf("number of headers : %d\n", h->fileheader->num_headers);
  }
  if(h->cont)
  {
    printf("\nCONTENT:\n");
    printf("title     : %s\n", h->cont->title);
    printf("author    : %s\n", h->cont->author);
    printf("copyright : %s\n", h->cont->copyright);
    printf("comment   : %s\n", h->cont->comment);
  }
  if(h->prop)
  {
    printf("\nSTREAM PROPERTIES:\n");
    printf("bit rate (max/avg)    : %i/%i\n", h->prop->max_bit_rate, h->prop->avg_bit_rate);
    printf("packet size (max/avg) : %i/%i bytes\n", h->prop->max_packet_size, h->prop->avg_packet_size);
    printf("packets       : %i\n", h->prop->num_packets);
    printf("duration      : %i ms\n", h->prop->duration);
    printf("pre-buffer    : %i ms\n", h->prop->preroll);
    printf("index offset  : %i bytes\n", h->prop->index_offset);
    printf("data offset   : %i bytes\n", h->prop->data_offset);
    printf("media streams : %i\n", h->prop->num_streams);
    printf("flags         : ");
    if (h->prop->flags & PN_SAVE_ENABLED) printf("save_enabled ");
    if (h->prop->flags & PN_PERFECT_PLAY_ENABLED) printf("perfect_play_enabled ");
    if (h->prop->flags & PN_LIVE_BROADCAST) printf("live_broadcast ");
    printf("\n");
  }
  stream=h->streams;
  if(stream)
  {
    while (*stream)
    {
      printf("\nSTREAM %i:\n", (*stream)->stream_number);
      printf("stream name [mime type] : %s [%s]\n", (*stream)->stream_name, (*stream)->mime_type);
      printf("bit rate (max/avg)      : %i/%i\n", (*stream)->max_bit_rate, (*stream)->avg_bit_rate);
      printf("packet size (max/avg)   : %i/%i bytes\n", (*stream)->max_packet_size, (*stream)->avg_packet_size);
      printf("start time : %i\n", (*stream)->start_time);
      printf("pre-buffer : %i ms\n", (*stream)->preroll);
      printf("duration   : %i ms\n", (*stream)->duration);
      printf("type specific data:\n");
      hexdump((*stream)->type_specific_data, (*stream)->type_specific_len);
      stream++;
    }
  }
  if(h->data)
  {
    printf("\nDATA:\n");
    printf("size      : %i\n", h->data->size);
    printf("packets   : %i\n", h->data->num_packets);
    printf("next DATA : 0x%08x\n", h->data->next_data_header);
  } 
}

void rmff_fix_header(rmff_header_t *h) {

  int num_headers=0;
  int header_size=0;
  rmff_mdpr_t **streams;
  int num_streams=0;

  if (!h) {
    printf("rmff_fix_header: fatal: no header given.\n");
    return;
  }

  if (!h->streams) {
    printf("rmff_fix_header: warning: no MDPR chunks\n");
  } else
  {
    streams=h->streams;
    while (*streams)
    {
      num_streams++;
      num_headers++;
      header_size+=(*streams)->size;
      streams++;
    }
  }
  
  if (h->prop) {
    if (h->prop->size != 50)
    {
#ifdef LOG
      printf("rmff_fix_header: correcting prop.size from %i to %i\n", h->prop->size, 50);
#endif
      h->prop->size=50;
    }
    if (h->prop->num_streams != num_streams)
    {
#ifdef LOG
      printf("rmff_fix_header: correcting prop.num_streams from %i to %i\n", h->prop->num_streams, num_streams);
#endif
      h->prop->num_streams=num_streams;
    }
    num_headers++;
    header_size+=50;
  } else
    printf("rmff_fix_header: warning: no PROP chunk.\n");

  if (h->cont) {
    num_headers++;
    header_size+=h->cont->size;
  } else
    printf("rmff_fix_header: warning: no CONT chunk.\n");

  if (!h->data) {
#ifdef LOG
    printf("rmff_fix_header: no DATA chunk, creating one\n");
#endif
    h->data=malloc(sizeof(rmff_data_t));
    h->data->object_id=DATA_TAG;
    h->data->object_version=0;
    h->data->size=34;
    h->data->num_packets=0;
    h->data->next_data_header=0;
  }
  num_headers++;

  
  if (!h->fileheader) {
#ifdef LOG
    printf("rmff_fix_header: no fileheader, creating one");
#endif
    h->fileheader=malloc(sizeof(rmff_fileheader_t));
    h->fileheader->object_id=RMF_TAG;
    h->fileheader->size=34;
    h->fileheader->object_version=0;
    h->fileheader->file_version=0;
    h->fileheader->num_headers=num_headers+1;
  }
  header_size+=h->fileheader->size;
  num_headers++;

  if(h->fileheader->num_headers != num_headers) {
#ifdef LOG
    printf("rmff_fix_header: setting num_headers from %i to %i\n", h->fileheader->num_headers, num_headers); 
#endif
    h->fileheader->num_headers=num_headers;
  }

  if(h->prop) {
    if (h->prop->data_offset != header_size) {
#ifdef LOG
      printf("rmff_fix_header: setting prop.data_offset from %i to %i\n", h->prop->data_offset, header_size); 
#endif
      h->prop->data_offset=header_size;
    }
    if (h->prop->num_packets == 0) {
      int p=(int)(h->prop->avg_bit_rate/8.0*(h->prop->duration/1000.0)/h->prop->avg_packet_size);
#ifdef LOG
      printf("rmff_fix_header: assuming prop.num_packets=%i\n", p); 
#endif
      h->prop->num_packets=p;
    }
    if (h->data->num_packets == 0) {
#ifdef LOG
      printf("rmff_fix_header: assuming data.num_packets=%i\n", h->prop->num_packets); 
#endif
      h->data->num_packets=h->prop->num_packets;
    }
    
#ifdef LOG
    printf("rmff_fix_header: assuming data.size=%i\n", h->prop->num_packets*h->prop->avg_packet_size); 
#endif
    h->data->size=h->prop->num_packets*h->prop->avg_packet_size;
  }
}

int rmff_get_header_size(rmff_header_t *h) {

  if (!h) return 0;
  if (!h->prop) return -1;

  return h->prop->data_offset+18;
  
}

void rmff_free_header(rmff_header_t *h) {

  if (!h) return;

  if (h->fileheader) free(h->fileheader);
  if (h->prop) free(h->prop);
  if (h->data) free(h->data);
  if (h->cont)
  {
    free(h->cont->title);
    free(h->cont->author);
    free(h->cont->copyright);
    free(h->cont->comment);
    free(h->cont);
  }
  if (h->streams)
  {
    rmff_mdpr_t **s=h->streams;

    while(*s) {
      free((*s)->stream_name);
      free((*s)->mime_type);
      free((*s)->type_specific_data);
      free(*s);
      s++;
    }
    free(h->streams);
  }
  free(h);
}
