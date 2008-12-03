/*
 * tivo@wingert.org, February 2003
 *
 * Copyright (C) 2003 Christopher R. Wingert
 *
 * The license covers the portions of this file regarding TiVo additions.
 *
 * Olaf Beck and Tridge (indirectly) were essential at providing
 * information regarding the format of the TiVo streams.
 *
 * However, no code in the following subsection is directly copied from
 * either author.
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
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "parse_es.h"
#include "stheader.h"
#include "sub_cc.h"
#include "libavutil/avstring.h"
#include "libavutil/intreadwrite.h"

void skip_audio_frame( sh_audio_t *sh_audio );
extern int sub_justify;

// 2/c0: audio data
// 3/c0: audio packet header (PES header)
// 4/c0: audio data (S/A only?)
// 9/c0: audio packet header, AC-3 audio
// 2/e0: video data
// 6/e0: video packet header (PES header)
// 7/e0: video sequence header start
// 8/e0: video I-frame header start
// a/e0: video P-frame header start
// b/e0: video B-frame header start
// c/e0: video GOP header start
// e/01: closed-caption data
// e/02: Extended data services data


#define TIVO_PES_FILEID   0xf5467abd
#define TIVO_PART_LENGTH  0x20000000

#define CHUNKSIZE        ( 128 * 1024 )
#define MAX_AUDIO_BUFFER ( 16 * 1024 )

#define TY_V             1
#define TY_A             2

typedef struct
{
   off_t startOffset;
   off_t fileSize;
   int   chunks;
} tmf_fileParts;

#define MAX_TMF_PARTS 16

typedef struct
{
   int             whichChunk;

   unsigned char   lastAudio[ MAX_AUDIO_BUFFER ];
   int             lastAudioEnd;

   int             tivoType;           // 1 = SA, 2 = DTiVo

   int64_t        lastAudioPTS;
   int64_t        lastVideoPTS;

   off_t           size;
   int             readHeader;

   int             tmf;
   tmf_fileParts   tmfparts[ MAX_TMF_PARTS ];
   int             tmf_totalparts;
} TiVoInfo;

off_t vstream_streamsize( );
void ty_ClearOSD( int start );

// ===========================================================================
#define TMF_SIG "showing.xml"

// ===========================================================================
static int ty_tmf_filetoparts( demuxer_t *demux, TiVoInfo *tivo )
{
   int     parts = 0;

   stream_seek(demux->stream, 0);

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "Dumping tar contents\n" );
   while (!demux->stream->eof)
   {
      char    header[ 512 ];
      char    *name;
      char    *extension;
      char    *sizestr;
      int     size;
      off_t   skip;
      if (stream_read(demux->stream, header, 512) < 512)
      {
         mp_msg( MSGT_DEMUX, MSGL_DBG3, "Read bad\n" );
         break;
      }
      name = header;
      name[99] = 0;
      sizestr = &header[124];
      sizestr[11] = 0;
      size = strtol(sizestr, NULL, 8);

      mp_msg( MSGT_DEMUX, MSGL_DBG3, "name %-20.20s size %-12.12s %d\n",
         name, sizestr, size );

      extension = strrchr(name, '.');
      if (extension && strcmp(extension, ".ty") == 0)
      {
         if ( parts >= MAX_TMF_PARTS ) {
            mp_msg( MSGT_DEMUX, MSGL_ERR, "ty:tmf too big\n" );
            break;
         }
         tivo->tmfparts[ parts ].fileSize = size;
         tivo->tmfparts[ parts ].startOffset = stream_tell(demux->stream);
         tivo->tmfparts[ parts ].chunks = size / CHUNKSIZE;
         mp_msg(MSGT_DEMUX, MSGL_DBG3,
           "tmf_filetoparts(): index %d, chunks %d\n"
           "tmf_filetoparts(): size %"PRId64"\n"
           "tmf_filetoparts(): startOffset %"PRId64"\n",
           parts, tivo->tmfparts[ parts ].chunks,
           tivo->tmfparts[ parts ].fileSize, tivo->tmfparts[ parts ].startOffset
         );
         parts++;
      }

      // size rounded up to blocks
      skip = (size + 511) & ~511;
      stream_skip(demux->stream, skip);
   }
   stream_reset(demux->stream);
   tivo->tmf_totalparts = parts;
   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "tmf_filetoparts(): No More Part Files %d\n", parts );

   return 1;
}


// ===========================================================================
static off_t tmf_filetooffset(TiVoInfo *tivo, int chunk)
{
  int i;
  for (i = 0; i < tivo->tmf_totalparts; i++) {
    if (chunk < tivo->tmfparts[i].chunks)
      return tivo->tmfparts[i].startOffset + chunk * CHUNKSIZE;
    chunk -= tivo->tmfparts[i].chunks;
  }
  return -1;
}


// ===========================================================================
static int tmf_load_chunk( demuxer_t *demux, TiVoInfo *tivo,
   unsigned char *buff, int readChunk )
{
   off_t fileoffset;
   int    count;

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "\ntmf_load_chunk() begin %d\n",
      readChunk );

   fileoffset = tmf_filetooffset(tivo, readChunk);

   if (fileoffset == -1 || !stream_seek(demux->stream, fileoffset)) {
      mp_msg( MSGT_DEMUX, MSGL_ERR, "Read past EOF()\n" );
      return 0;
   }
   count = stream_read( demux->stream, buff, CHUNKSIZE );
   demux->filepos = stream_tell( demux->stream );

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "tmf_load_chunk() count %x\n",
           count );

   mp_msg( MSGT_DEMUX, MSGL_DBG3,
           "tmf_load_chunk() bytes %x %x %x %x %x %x %x %x\n",
           buff[ 0 ], buff[ 1 ], buff[ 2 ], buff[ 3 ],
           buff[ 4 ], buff[ 5 ], buff[ 6 ], buff[ 7 ] );

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "tmf_load_chunk() end\n" );

   return count;
}

// ===========================================================================

// DTiVo MPEG 336, 480, 576, 768
// SA TiVo 864
// DTiVo AC-3 1550
//
#define SERIES1_PTS_LENGTH           11
#define SERIES1_PTS_OFFSET            6
#define SERIES2_PTS_LENGTH           16
#define SERIES2_PTS_OFFSET            9
#define AC3_PTS_LENGTH               16
#define AC3_PTS_OFFSET                9

static int IsValidAudioPacket( int size, int *ptsOffset, int *ptsLen )
{
   // AC-3
   if ( size == 1550 || size == 1552 )
   {
      *ptsOffset = AC3_PTS_OFFSET;
      *ptsLen = AC3_PTS_LENGTH;
      return 1;
   }

   // MPEG
      if ( (size & 15) == (SERIES1_PTS_LENGTH & 15) )
      {
         *ptsOffset = SERIES1_PTS_OFFSET;
         *ptsLen = SERIES1_PTS_LENGTH;
         return 1;
      }
         if ( (size & 15) == (SERIES2_PTS_LENGTH & 15) )
         {
            *ptsOffset = SERIES2_PTS_OFFSET;
            *ptsLen = SERIES2_PTS_LENGTH;
            return 1;
         }
      mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Tossing Audio Packet Size %d\n",
         size );
      return 0;
}


static int64_t get_ty_pts( unsigned char *buf )
{
  int a = buf[0] & 0xe;
  int b = AV_RB16(buf + 1);
  int c = AV_RB16(buf + 3);

  if (!(1 & a & b & c)) // invalid MPEG timestamp
    return MP_NOPTS_VALUE;
  a >>= 1; b >>= 1; c >>= 1;
  return (((uint64_t)a) << 30) | (b << 15) | c;
}

static void demux_ty_AddToAudioBuffer( TiVoInfo *tivo, unsigned char *buffer,
   int size )
{
   if ( tivo->lastAudioEnd + size < MAX_AUDIO_BUFFER )
   {
      memcpy( &tivo->lastAudio[ tivo->lastAudioEnd ],
         buffer, size );
      tivo->lastAudioEnd += size;
   }
   else
      mp_msg( MSGT_DEMUX, MSGL_ERR,
         "ty:WARNING - Would have blown my audio buffer\n" );
}

static void demux_ty_CopyToDemuxPacket( demux_stream_t *ds,
       unsigned char *buffer, int size, off_t pos, int64_t pts )
{
   demux_packet_t *dp = new_demux_packet( size );
   memcpy( dp->buffer, buffer, size );
   if (pts != MP_NOPTS_VALUE)
   dp->pts = pts / 90000.0;
   dp->pos = pos;
   dp->flags = 0;
   ds_add_packet( ds, dp );
}

static int demux_ty_FindESHeader( uint8_t nal,
   unsigned char *buffer, int bufferSize )
{
   uint32_t search = 0x00000100 | nal;
   uint32_t found = -1;
   uint8_t *p = buffer;
   uint8_t *end = p + bufferSize;
   while (p < end) {
      found <<= 8;
      found |= *p++;
      if (found == search)
         return p - buffer - 4;
   }
   return -1;
}

static void demux_ty_FindESPacket( uint8_t nal,
   unsigned char *buffer, int bufferSize, int *esOffset1, int *esOffset2 )
{
  *esOffset1 = demux_ty_FindESHeader(nal, buffer, bufferSize);
  if (*esOffset1 == -1) {
    *esOffset2 = -1;
    return;
  }
  buffer += *esOffset1 + 1;
  bufferSize -= *esOffset1 + 1;
  *esOffset2 = demux_ty_FindESHeader(nal, buffer, bufferSize);
  if (*esOffset2 != -1)
    *esOffset2 += *esOffset1 + 1;
}

#define VIDEO_NAL 0xe0
#define AUDIO_NAL 0xc0
#define AC3_NAL 0xbd

static int demux_ty_fill_buffer( demuxer_t *demux, demux_stream_t *dsds )
{
   int              invalidType = 0;
   int              errorHeader = 0;
   int              recordsDecoded = 0;

   unsigned char    chunk[ CHUNKSIZE ];
   int              readSize;

   int              numberRecs;
   unsigned char    *recPtr;
   int              offset;

   int              counter;

   int              aid;

   TiVoInfo         *tivo = demux->priv;

   if ( demux->stream->type == STREAMTYPE_DVD )
      return 0;

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:ty processing\n" );

   if( demux->stream->eof ) return 0;

   // ======================================================================
   // If we haven't figured out the size of the stream, let's do so
   // ======================================================================
#ifdef STREAMTYPE_STREAM_TY
   if ( demux->stream->type == STREAMTYPE_STREAM_TY )
   {
      // The vstream code figures out the exact size of the stream
      demux->movi_start = 0;
      demux->movi_end = vstream_streamsize();
      tivo->size = vstream_streamsize();
   }
   else
#endif
   {
      // If its a local file, try to find the Part Headers, so we can
      // calculate the ACTUAL stream size
      // If we can't find it, go off with the file size and hope the
      // extract program did the "right thing"
      if ( tivo->readHeader == 0 )
      {
         off_t filePos;
         tivo->readHeader = 1;

         filePos = demux->filepos;
         stream_seek( demux->stream, 0 );

         readSize = stream_read( demux->stream, chunk, CHUNKSIZE );

         if ( memcmp( chunk, TMF_SIG, sizeof( TMF_SIG ) ) == 0 )
         {
            mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Detected a tmf\n" );
            tivo->tmf = 1;
            ty_tmf_filetoparts( demux, tivo );
            readSize = tmf_load_chunk( demux, tivo, chunk, 0 );
         }

         if ( readSize == CHUNKSIZE && AV_RB32(chunk) == TIVO_PES_FILEID )
         {
               off_t numberParts;

               readSize = 0;

               if ( tivo->tmf != 1 )
               {
                  off_t offset;

                  numberParts = demux->stream->end_pos / TIVO_PART_LENGTH;
                  offset = numberParts * TIVO_PART_LENGTH;

                  mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:ty/ty+Number Parts %"PRId64"\n",
                    (int64_t)numberParts );

                  if ( offset + CHUNKSIZE < demux->stream->end_pos )
                  {
                     stream_seek( demux->stream, offset );
                     readSize = stream_read( demux->stream, chunk, CHUNKSIZE );
                  }
               }
               else
               {
                  numberParts = tivo->tmf_totalparts;
                  offset = numberParts * TIVO_PART_LENGTH;
                  readSize = tmf_load_chunk( demux, tivo, chunk,
                     numberParts * ( TIVO_PART_LENGTH - CHUNKSIZE ) /
                     CHUNKSIZE );
               }

               if ( readSize == CHUNKSIZE && AV_RB32(chunk) == TIVO_PES_FILEID )
               {
                     int size = AV_RB24(chunk + 12);
                     size -= 4;
                     size *= CHUNKSIZE;
                     tivo->size = numberParts * TIVO_PART_LENGTH;
                     tivo->size += size;
                     mp_msg( MSGT_DEMUX, MSGL_DBG3,
                        "ty:Header Calc Stream Size %"PRId64"\n", tivo->size );
               }
         }

         if ( demux->stream->start_pos > 0 )
            filePos = demux->stream->start_pos;
         stream_seek( demux->stream, filePos );
         demux->filepos = stream_tell( demux->stream );
         tivo->whichChunk = filePos / CHUNKSIZE;
      }
      demux->movi_start = 0;
      demux->movi_end = tivo->size;
   }

   // ======================================================================
   // Give a clue as to where we are in the stream
   // ======================================================================
   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "ty:ty header size %"PRIx64"\n", (int64_t)tivo->size );
   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "ty:ty which Chunk %d\n", tivo->whichChunk );
   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "ty:file end_pos   %"PRIx64"\n", (int64_t)demux->stream->end_pos );
   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "\nty:wanted current offset %"PRIx64"\n", (int64_t)stream_tell( demux->stream ) );

   if ( tivo->size > 0 && stream_tell( demux->stream ) > tivo->size )
   {
         demux->stream->eof = 1;
         return 0;
   }

   do {
   if ( tivo->tmf != 1 )
   {
      // Make sure we are on a 128k boundary
      if ( demux->filepos % CHUNKSIZE != 0 )
      {
         int whichChunk = demux->filepos / CHUNKSIZE;
         if ( demux->filepos % CHUNKSIZE > CHUNKSIZE / 2 )
            whichChunk++;
         stream_seek( demux->stream, whichChunk * CHUNKSIZE );
      }

      demux->filepos = stream_tell( demux->stream );
      tivo->whichChunk = demux->filepos / CHUNKSIZE;
      readSize = stream_read( demux->stream, chunk, CHUNKSIZE );
      if ( readSize != CHUNKSIZE )
         return 0;
   }
   else
   {
      readSize = tmf_load_chunk( demux, tivo, chunk, tivo->whichChunk );
      if ( readSize != CHUNKSIZE )
         return 0;
      tivo->whichChunk++;
   }
   if (AV_RB32(chunk) == TIVO_PES_FILEID)
      mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Skipping PART Header\n" );
   } while (AV_RB32(chunk) == TIVO_PES_FILEID);

   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "\nty:actual current offset %"PRIx64"\n", stream_tell( demux->stream ) -
      CHUNKSIZE );


   // Let's make a Video Demux Stream for MPlayer
   aid = 0x0;
   if( !demux->v_streams[ aid ] ) new_sh_video( demux, aid );
   if( demux->video->id == -1 ) demux->video->id = aid;
   if( demux->video->id == aid )
   {
      demux_stream_t *ds = demux->video;
      if( !ds->sh ) ds->sh = demux->v_streams[ aid ];
   }

   // ======================================================================
   // Finally, we get to actually parse the chunk
   // ======================================================================
   mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:ty parsing a chunk\n" );
   numberRecs = chunk[ 0 ];
   recPtr = &chunk[ 4 ];
   offset = numberRecs * 16 + 4;
   for ( counter = 0 ; counter < numberRecs ; counter++ )
   {
      int size = AV_RB24(recPtr) >> 4;
      int type = recPtr[ 3 ];
      int nybbleType = recPtr[ 2 ] & 0x0f;
      recordsDecoded++;

      mp_msg( MSGT_DEMUX, MSGL_DBG3,
         "ty:Record Type %x/%x %d\n", nybbleType, type, size );

      // ================================================================
      // Video Parsing
      // ================================================================
      if ( type == 0xe0 )
      {
         if ( size > 0 && size + offset <= CHUNKSIZE )
         {
            int esOffset1 = demux_ty_FindESHeader( VIDEO_NAL, &chunk[ offset ],
               size);
            if ( esOffset1 != -1 )
               tivo->lastVideoPTS = get_ty_pts(
                  &chunk[ offset + esOffset1 + 9 ] );

            // Do NOT Pass the PES Header onto the MPEG2 Decode
            if( nybbleType != 0x06 )
               demux_ty_CopyToDemuxPacket( demux->video,
                  &chunk[ offset ], size, demux->filepos + offset,
                  tivo->lastVideoPTS );
            offset += size;
         }
         else
            errorHeader++;
      }
      // ================================================================
      // Audio Parsing
      // ================================================================
      else if ( type == 0xc0 )
      {
         if ( size > 0 && size + offset <= CHUNKSIZE )
         {
            if( demux->audio->id == -1 )
            {
               if ( nybbleType == 0x02 )
                  continue;    // DTiVo inconclusive, wait for more
               else if ( nybbleType == 0x09 )
               {
                  mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Setting AC-3 Audio\n" );
                  aid = 0x80;  // AC-3
               }
               else
               {
                  mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Setting MPEG Audio\n" );
                  aid = 0x0;   // MPEG Audio
               }

               demux->audio->id = aid;
               if( !demux->a_streams[ aid ] ) new_sh_audio( demux, aid );
               if( demux->audio->id == aid )
               {
                  demux_stream_t *ds = demux->audio;
                  if( !ds->sh ) {
                    sh_audio_t* sh_a;
                    ds->sh = demux->a_streams[ aid ];
                    sh_a = (sh_audio_t*)ds->sh;
                    switch(aid & 0xE0){  // 1110 0000 b  (high 3 bit: type  low 5: id)
                      case 0x00: sh_a->format=0x50;break; // mpeg
                      case 0xA0: sh_a->format=0x10001;break;  // dvd pcm
                      case 0x80: if((aid & 0xF8) == 0x88) sh_a->format=0x2001;//dts
                                  else sh_a->format=0x2000;break; // ac3
                    }
                 }
               }
            }

            aid = demux->audio->id;


            // SA DTiVo Audio Data, no PES
            // ================================================
            if ( nybbleType == 0x02 || nybbleType == 0x04 )
            {
               if ( nybbleType == 0x02 && tivo->tivoType == 2 )
                  demux_ty_AddToAudioBuffer( tivo, &chunk[ offset ], size );
               else
               {

                  mp_msg( MSGT_DEMUX, MSGL_DBG3,
                     "ty:Adding Audio Packet Size %d\n", size );
                  demux_ty_CopyToDemuxPacket( demux->audio,
                     &chunk[ offset ], size, ( demux->filepos + offset ),
                     tivo->lastAudioPTS );
               }
            }

            // 3 - MPEG Audio with PES Header, either SA or DTiVo
            // 9 - DTiVo AC3 Audio Data with PES Header
            // ================================================
            if ( nybbleType == 0x03 || nybbleType == 0x09 )
            {
               int esOffset1, esOffset2;
               if ( nybbleType == 0x03 )
               esOffset1 = demux_ty_FindESHeader( AUDIO_NAL, &chunk[ offset ],
                  size);

               // SA PES Header, No Audio Data
               // ================================================
               if ( nybbleType == 0x03 && esOffset1 == 0 && size == 16 )
               {
                  tivo->tivoType = 1;
                  tivo->lastAudioPTS = get_ty_pts( &chunk[ offset +
                     SERIES2_PTS_OFFSET ] );
               }
               else
               // DTiVo Audio with PES Header
               // ================================================
               {
                  tivo->tivoType = 2;

                  demux_ty_AddToAudioBuffer( tivo, &chunk[ offset ], size );
                  demux_ty_FindESPacket( nybbleType == 9 ? AC3_NAL : AUDIO_NAL,
                     tivo->lastAudio, tivo->lastAudioEnd, &esOffset1,
                     &esOffset2 );

                  if ( esOffset1 != -1 && esOffset2 != -1 )
                  {
                     int packetSize = esOffset2 - esOffset1;
                     int headerSize;
                     int ptsOffset;

                     if ( IsValidAudioPacket( packetSize, &ptsOffset,
                           &headerSize ) )
                     {
                        mp_msg( MSGT_DEMUX, MSGL_DBG3,
                           "ty:Adding DTiVo Audio Packet Size %d\n",
                           packetSize );

                        tivo->lastAudioPTS = get_ty_pts(
                           &tivo->lastAudio[ esOffset1 + ptsOffset ] );

                        if (nybbleType == 9) headerSize = 0;
                        demux_ty_CopyToDemuxPacket
                        (
                           demux->audio,
                           &tivo->lastAudio[ esOffset1 + headerSize ],
                           packetSize - headerSize,
                           demux->filepos + offset,
                           tivo->lastAudioPTS
                        );

                     }

                     // Collapse the Audio Buffer
                     tivo->lastAudioEnd -= esOffset2;
                     memmove( &tivo->lastAudio[ 0 ],
                        &tivo->lastAudio[ esOffset2 ],
                        tivo->lastAudioEnd );
                  }
               }
            }

            offset += size;
         }
         else
            errorHeader++;
      }
      // ================================================================
      // 1 = Closed Caption
      // 2 = Extended Data Services
      // ================================================================
      else if ( type == 0x01 || type == 0x02 )
      {
                        unsigned char lastXDS[ 16 ];
         int b = AV_RB24(recPtr) >> 4;
         b &= 0x7f7f;

         mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:%s %04x\n", type == 1 ? "CC" : "XDS", b);

         lastXDS[ 0x00 ] = 0x00;
         lastXDS[ 0x01 ] = 0x00;
         lastXDS[ 0x02 ] = 0x01;
         lastXDS[ 0x03 ] = 0xb2;
         lastXDS[ 0x04 ] = 'T';
         lastXDS[ 0x05 ] = 'Y';
         lastXDS[ 0x06 ] = type;
         lastXDS[ 0x07 ] = b >> 8;
         lastXDS[ 0x08 ] = b;
         if ( subcc_enabled )
            demux_ty_CopyToDemuxPacket( demux->video, lastXDS, 0x09,
               demux->filepos + offset, tivo->lastVideoPTS );
      }
      // ================================================================
      // Unknown
      // ================================================================
      else
      {
         if ( size > 0 && size + offset <= CHUNKSIZE )
            offset += size;
         if (type != 3 && type != 5 && (type != 0 || size > 0)) {
         mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Invalid Type %x\n", type );
         invalidType++;
         }
      }
     recPtr += 16;
   }

   if ( errorHeader > 0 || invalidType > 0 )
   {
      mp_msg( MSGT_DEMUX, MSGL_DBG3,
         "ty:Error Check - Records %d, Parsed %d, Errors %d + %d\n",
         numberRecs, recordsDecoded, errorHeader, invalidType );

      // Invalid MPEG ES Size Check
      if ( errorHeader > numberRecs / 2 )
         return 0;

      // Invalid MPEG Stream Type Check
      if ( invalidType > numberRecs / 2 )
         return 0;
   }

   demux->filepos = stream_tell( demux->stream );

   return 1;
}

static void demux_seek_ty( demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags )
{
   demux_stream_t *d_audio = demuxer->audio;
   demux_stream_t *d_video = demuxer->video;
   sh_audio_t     *sh_audio = d_audio->sh;
   sh_video_t     *sh_video = d_video->sh;
   off_t          newpos;
   off_t          res;
   TiVoInfo       *tivo = demuxer->priv;

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Seeking to %7.1f\n", rel_seek_secs );

      tivo->lastAudioEnd = 0;
      tivo->lastAudioPTS = MP_NOPTS_VALUE;
      tivo->lastVideoPTS = MP_NOPTS_VALUE;
   //
   //================= seek in MPEG ==========================
   demuxer->filepos = stream_tell( demuxer->stream );

   newpos = ( flags & SEEK_ABSOLUTE ) ? demuxer->movi_start : demuxer->filepos;

   if( flags & SEEK_FACTOR )
      // float seek 0..1
      newpos += ( demuxer->movi_end - demuxer->movi_start ) * rel_seek_secs;
   else
   {
      // time seek (secs)
      if( ! sh_video->i_bps ) // unspecified or VBR
         newpos += 2324 * 75 * rel_seek_secs; // 174.3 kbyte/sec
      else
         newpos += sh_video->i_bps * rel_seek_secs;
   }

   if ( newpos < demuxer->movi_start )
   {
      if( demuxer->stream->type != STREAMTYPE_VCD ) demuxer->movi_start = 0;
      if( newpos < demuxer->movi_start ) newpos = demuxer->movi_start;
   }

   res = newpos / CHUNKSIZE;
   if ( rel_seek_secs >= 0 )
      newpos = ( res + 1 ) * CHUNKSIZE;
   else
      newpos = res * CHUNKSIZE;

   if ( newpos < 0 )
      newpos = 0;

   tivo->whichChunk = newpos / CHUNKSIZE;

   stream_seek( demuxer->stream, newpos );

   // re-sync video:
   videobuf_code_len = 0; // reset ES stream buffer

   ds_fill_buffer( d_video );
   if( sh_audio )
     ds_fill_buffer( d_audio );

   while( 1 )
   {
     int i;
     if( sh_audio && !d_audio->eof && d_video->pts && d_audio->pts )
     {
        float a_pts = d_audio->pts;
        a_pts += ( ds_tell_pts( d_audio ) - sh_audio->a_in_buffer_len ) /
           (float)sh_audio->i_bps;
        if( d_video->pts > a_pts )
        {
           skip_audio_frame( sh_audio );  // sync audio
           continue;
        }
     }
     i = sync_video_packet( d_video );
     if( i == 0x1B3 || i == 0x1B8 ) break; // found it!
     if( !i || !skip_video_packet( d_video ) ) break; // EOF?
   }
   if ( subcc_enabled )
      ty_ClearOSD( 0 );
}

static int demux_ty_control( demuxer_t *demuxer,int cmd, void *arg )
{
   demux_stream_t *d_video = demuxer->video;
   sh_video_t     *sh_video = d_video->sh;

   switch(cmd)
   {
      case DEMUXER_CTRL_GET_TIME_LENGTH:
         if(!sh_video->i_bps)  // unspecified or VBR
             return DEMUXER_CTRL_DONTKNOW;
         *(double *)arg=
            (double)demuxer->movi_end-demuxer->movi_start/sh_video->i_bps;
         return DEMUXER_CTRL_GUESS;

      case DEMUXER_CTRL_GET_PERCENT_POS:
          return DEMUXER_CTRL_DONTKNOW;
       default:
          return DEMUXER_CTRL_NOTIMPL;
    }
}


static void demux_close_ty( demuxer_t *demux )
{
   TiVoInfo         *tivo = demux->priv;

      free( tivo );
      sub_justify = 0;
}


static int ty_check_file(demuxer_t* demuxer)
{
  TiVoInfo *tivo = calloc(1, sizeof(TiVoInfo));
  demuxer->priv = tivo;
  return ds_fill_buffer(demuxer->video) ? DEMUXER_TYPE_MPEG_TY : 0;
}


static demuxer_t* demux_open_ty(demuxer_t* demuxer)
{
    sh_audio_t *sh_audio=NULL;
    sh_video_t *sh_video=NULL;

    sh_video=demuxer->video->sh;sh_video->ds=demuxer->video;

    if(demuxer->audio->id!=-2) {
        if(!ds_fill_buffer(demuxer->audio)){
            mp_msg(MSGT_DEMUXER,MSGL_INFO,"MPEG: " MSGTR_MissingAudioStream);
            demuxer->audio->sh=NULL;
        } else {
            sh_audio=demuxer->audio->sh;sh_audio->ds=demuxer->audio;
        }
    }

    return demuxer;
}


const demuxer_desc_t demuxer_desc_mpeg_ty = {
  "TiVo demuxer",
  "tivo",
  "TiVo",
  "Christopher R. Wingert",
  "Demux streams from TiVo",
  DEMUXER_TYPE_MPEG_TY,
  0, // unsafe autodetect
  ty_check_file,
  demux_ty_fill_buffer,
  demux_open_ty,
  demux_close_ty,
  demux_seek_ty,
  demux_ty_control
};
