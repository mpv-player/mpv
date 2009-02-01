/*
 * This file was ported to MPlayer from xine CVS rmff.h,v 1.3 2003/02/10 22:11:10
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
 * some functions for real media file headers
 * adopted from joschkas real tools
 */

#ifndef MPLAYER_RMFF_H
#define MPLAYER_RMFF_H

#include <sys/types.h>
#include "config.h"
#if !HAVE_WINSOCK2_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <winsock2.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


#define RMFF_HEADER_SIZE 0x12

#define RMFF_FILEHEADER_SIZE 18
#define RMFF_PROPHEADER_SIZE 50
#define RMFF_MDPRHEADER_SIZE 46
#define RMFF_CONTHEADER_SIZE 18
#define RMFF_DATAHEADER_SIZE 18

#define FOURCC_TAG( ch0, ch1, ch2, ch3 ) \
        (((long)(unsigned char)(ch3)       ) | \
        ( (long)(unsigned char)(ch2) << 8  ) | \
        ( (long)(unsigned char)(ch1) << 16 ) | \
        ( (long)(unsigned char)(ch0) << 24 ) )


#define RMF_TAG   FOURCC_TAG('.', 'R', 'M', 'F')
#define PROP_TAG  FOURCC_TAG('P', 'R', 'O', 'P')
#define MDPR_TAG  FOURCC_TAG('M', 'D', 'P', 'R')
#define CONT_TAG  FOURCC_TAG('C', 'O', 'N', 'T')
#define DATA_TAG  FOURCC_TAG('D', 'A', 'T', 'A')
#define INDX_TAG  FOURCC_TAG('I', 'N', 'D', 'X')
#define PNA_TAG   FOURCC_TAG('P', 'N', 'A',  0 )

#define MLTI_TAG  FOURCC_TAG('M', 'L', 'T', 'I')

/* prop flags */
#define PN_SAVE_ENABLED         0x01
#define PN_PERFECT_PLAY_ENABLED 0x02
#define PN_LIVE_BROADCAST       0x04

/*
 * rm header data structs
 */

typedef struct {

  uint32_t object_id;
  uint32_t size;
  uint16_t object_version;

  uint32_t file_version;
  uint32_t num_headers;
} rmff_fileheader_t;

typedef struct {

  uint32_t object_id;
  uint32_t size;
  uint16_t object_version;

  uint32_t max_bit_rate;
  uint32_t avg_bit_rate;
  uint32_t max_packet_size;
  uint32_t avg_packet_size;
  uint32_t num_packets;
  uint32_t duration;
  uint32_t preroll;
  uint32_t index_offset;
  uint32_t data_offset;
  uint16_t num_streams;
  uint16_t flags;
    
} rmff_prop_t;

typedef struct {

  uint32_t  object_id;
  uint32_t  size;
  uint16_t  object_version;

  uint16_t  stream_number;
  uint32_t  max_bit_rate;
  uint32_t  avg_bit_rate;
  uint32_t  max_packet_size;
  uint32_t  avg_packet_size;
  uint32_t  start_time;
  uint32_t  preroll;
  uint32_t  duration;
  uint8_t   stream_name_size;
  char      *stream_name;
  uint8_t   mime_type_size;
  char      *mime_type;
  uint32_t  type_specific_len;
  char      *type_specific_data;

  int       mlti_data_size;
  char      *mlti_data;

} rmff_mdpr_t;

typedef struct {

  uint32_t  object_id;
  uint32_t  size;
  uint16_t  object_version;

  uint16_t  title_len;
  char      *title;
  uint16_t  author_len;
  char      *author;
  uint16_t  copyright_len;
  char      *copyright;
  uint16_t  comment_len;
  char      *comment;
  
} rmff_cont_t;

typedef struct {
  
  uint32_t object_id;
  uint32_t size;
  uint16_t object_version;

  uint32_t num_packets;
  uint32_t next_data_header; /* rarely used */
} rmff_data_t;

typedef struct {

  rmff_fileheader_t *fileheader;
  rmff_prop_t *prop;
  rmff_mdpr_t **streams;
  rmff_cont_t *cont;
  rmff_data_t *data;
} rmff_header_t;

typedef struct {

  uint16_t object_version;

  uint16_t length;
  uint16_t stream_number;
  uint32_t timestamp;
  uint8_t reserved;
  uint8_t flags;

} rmff_pheader_t;

/*
 * constructors for header structs
 */
 
rmff_fileheader_t *rmff_new_fileheader(uint32_t num_headers);

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
    uint16_t flags );

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
    const char *type_specific_data );

rmff_cont_t *rmff_new_cont(
    const char *title,
    const char *author,
    const char *copyright,
    const char *comment);

rmff_data_t *rmff_new_dataheader(
    uint32_t num_packets, uint32_t next_data_header);

/*
 * reads header infos from data and returns a newly allocated header struct
 */
rmff_header_t *rmff_scan_header(const char *data);

/*
 * scans a data packet header. Notice, that this function does not allocate
 * the header struct itself.
 */
void rmff_scan_pheader(rmff_pheader_t *h, char *data);

/*
 * reads header infos from stream and returns a newly allocated header struct
 */
rmff_header_t *rmff_scan_header_stream(int fd);

/*
 * prints header information in human readible form to stdout
 */
void rmff_print_header(rmff_header_t *h);

/*
 * does some checks and fixes header if possible
 */
void rmff_fix_header(rmff_header_t *h);

/*
 * returns the size of the header (incl. first data-header)
 */
int rmff_get_header_size(rmff_header_t *h);
 
/*
 * dumps the header <h> to <buffer>. <max> is the size of <buffer>
 */
int rmff_dump_header(rmff_header_t *h, char *buffer, int max);

/*
 * dumps a packet header
 */
void rmff_dump_pheader(rmff_pheader_t *h, char *data);

/*
 * frees a header struct
 */
void rmff_free_header(rmff_header_t *h);

#endif /* MPLAYER_RMFF_H */
