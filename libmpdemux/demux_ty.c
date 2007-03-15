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
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
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

extern void skip_audio_frame( sh_audio_t *sh_audio );
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


#define TIVO_PES_FILEID   ( 0xf5467abd )
#define TIVO_PART_LENGTH  ( 0x20000000 )

#define CHUNKSIZE        ( 128 * 1024 )
#define MAX_AUDIO_BUFFER ( 16 * 1024 )

#define PTS_MHZ          ( 90 )
#define PTS_KHZ          ( PTS_MHZ * 1000 )

#define TY_V             ( 1 )
#define TY_A             ( 2 )

typedef struct stmf_fileParts
{
   int   fileNo;
   off_t fileSize;
   int   chunks;
   off_t startOffset;
} tmf_fileParts;

#define MAX_TMF_PARTS ( 16 )

typedef struct sTivoInfo
{
   int             whichChunk;

   unsigned char   lastAudio[ MAX_AUDIO_BUFFER ];
   int             lastAudioEnd;

   int             tivoType;           // 1 = SA, 2 = DTiVo

   float           firstAudioPTS;
   float           firstVideoPTS;

   float           lastAudioPTS;
   float           lastVideoPTS;

   int             headerOk;
   unsigned int    pesFileId;          // Should be 0xf5467abd
   int             streamType;         // Should be 0x02
   int             chunkSize;          // Should always be 128k
   off_t           size;
   int             readHeader;
   
   int             tmf;
   tmf_fileParts   tmfparts[ MAX_TMF_PARTS ];
   int             tmf_totalparts;
   off_t           tmf_totalsize;
   off_t           tmf_totalchunks;

} TiVoInfo;

off_t vstream_streamsize( );
void ty_ClearOSD( int start );

// ===========================================================================
#define TMF_SIG "showing.xml"

int ty_octaltodecimal( char *num )
{
   int i;
   int result = 0;
   int len;
   int mult;

   len = strlen( num );
   mult = 1;

   for ( i = ( len - 1 ) ; i >= 0 ; i-- )
   {
      result += ( ( num[ i ] - '0') * mult );
      mult *= 8;
   }
   return( result );
}



// ===========================================================================
int ty_extensionis( char *name, char *ext )
{
   char *ptr;

   if ( strlen( ext ) > strlen( name ) ) return( 0 );
   ptr = name;
   ptr += ( strlen( name ) - strlen( ext ) );
   if ( strcmp( ptr, ext ) == 0 ) return( 1 );
   return( 0 );
}


// ===========================================================================
int ty_tmf_filetoparts( demuxer_t *demux, TiVoInfo *tivo )
{
   char    header[ 512 ];
   char    name[ 100 ];
   char    sizestr[ 80 ];
   int     size;
   int     count;
   int     blocks;
   int     done;
   off_t   offset;
   off_t   totalsize;
   off_t   skip;
   int     error = 0;
   int     parts = 0;
   int     isty;
   int     index;
   int     ok;

   offset = 0;
   totalsize = demux->stream->end_pos;

   done = 0;
   mp_msg( MSGT_DEMUX, MSGL_DBG3, "Dumping tar contents\n" );
   while ( done == 0 )
   {
      ok = stream_seek( demux->stream, offset );
      if ( ( offset + 512 ) == totalsize )
      {
         done = 1;
         break;
      }
      if ( ok == 0 )
      { 
         mp_msg( MSGT_DEMUX, MSGL_DBG3, "Seek bad %"PRId64"\n", (int64_t)offset );
         done = 1;
         error = 1;
         break;
      }
      count = stream_read( demux->stream, header, 512 );
      if ( count < 512 )
      { 
         mp_msg( MSGT_DEMUX, MSGL_DBG3, "Read bad\n" );
         done = 1;
         error = 1;
         break;
      }
      strlcpy( name, &header[ 0 ], 100 );
      strlcpy( sizestr, &header[ 124 ], 12 );
      size = ty_octaltodecimal( sizestr );

      blocks = size / 512;
      if ( ( size % 512 ) > 0 ) blocks++;
      skip = ( blocks + 1 ) * 512;

      if ( ( offset + skip ) > totalsize )
      {
         size = totalsize - offset;
      }

      isty = ty_extensionis( name, ".ty" );

      mp_msg( MSGT_DEMUX, MSGL_DBG3, "name %-20.20s size %-12.12s %d %d\n", 
         name, sizestr, size, isty );

      if ( isty )
      {
         tivo->tmfparts[ parts ].fileNo = parts;
			// HACK - Ignore last chunk of a Part File
			// Why?  I have no idea.
         tivo->tmfparts[ parts ].fileSize = size - 0x20000;
         tivo->tmfparts[ parts ].startOffset = offset + 512;
         tivo->tmfparts[ parts ].chunks = 
            ( tivo->tmfparts[ parts ].fileSize / CHUNKSIZE );
         mp_msg
         ( 
            MSGT_DEMUX, MSGL_DBG3,
           "tmf_filetoparts(): index %d, file %d, chunks %d\n",
            parts,
            tivo->tmfparts[ parts ].fileNo,
            tivo->tmfparts[ parts ].chunks
         );
         mp_msg
         ( 
            MSGT_DEMUX, MSGL_DBG3,
           "tmf_filetoparts(): size %"PRId64"\n",
           tivo->tmfparts[ parts ].fileSize
         );
         mp_msg
         ( 
            MSGT_DEMUX, MSGL_DBG3,
           "tmf_filetoparts(): startOffset %"PRId64"\n",
           tivo->tmfparts[ parts ].startOffset
         );
         parts++;
         if ( parts > MAX_TMF_PARTS )
         {
            mp_msg( MSGT_DEMUX, MSGL_ERR, "ty:tmf too big\n" );
         }
      }

      if ( ( offset + skip ) > totalsize )
      {
         done = 1;
         error = 1;
      }
      else
      {
         offset += skip;
      }
   }
   if ( error )
   {
      mp_msg( MSGT_DEMUX, MSGL_DBG3, 
         "WARNING : tmf parse error, not intact\n" );
   }
   tivo->tmf_totalparts = parts;
   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "tmf_filetoparts(): No More Part Files %d\n", parts );

   tivo->tmf_totalsize = 0;
   tivo->tmf_totalchunks = 0;
   for( index = 0 ; index < tivo->tmf_totalparts ; index++ )
   {
      tivo->tmf_totalsize += tivo->tmfparts[ index ].fileSize;
      tivo->tmf_totalchunks += ( tivo->tmfparts[ index ].fileSize / CHUNKSIZE );
   }
   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "tmf_filetoparts():total size %"PRId64"\n", (int64_t)tivo->tmf_totalsize );
   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "tmf_filetoparts():total chunks %"PRId64"\n", (int64_t)tivo->tmf_totalchunks );

   return( 1 );
}


// ===========================================================================
void tmf_filetooffset( TiVoInfo *tivo, int chunk, off_t *offset )
{
   int index;

   *offset = 0;

   for( index = 0 ; index < tivo->tmf_totalparts ; index++ )
   {
      if ( chunk >= tivo->tmfparts[ index ].chunks )
      {
         chunk -= tivo->tmfparts[ index ].chunks;
      }
      else
      {
         break;
      }
   }
   if ( chunk < tivo->tmfparts[ index ].chunks )
   {
      *offset = tivo->tmfparts[ index ].startOffset +
         ( chunk * CHUNKSIZE );
   }
   mp_msg
   ( 
      MSGT_DEMUX, MSGL_DBG3, 
      "tmf_filetooffset() offset %"PRIx64"\n", *offset
   );
}


// ===========================================================================
int tmf_load_chunk( demuxer_t *demux, TiVoInfo *tivo, 
   unsigned char *buff, int size, int readChunk )
{
   off_t fileoffset;
   int    count;

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "\ntmf_load_chunk() begin %d\n", 
      readChunk );

   if ( tivo->tmf_totalparts <= 0 )
   {
      return( 0 );
   }

   if ( readChunk >= tivo->tmf_totalchunks )
   {
      mp_msg( MSGT_DEMUX, MSGL_ERR, "Read past EOF()\n" );
      return( 0 );
   }

   tmf_filetooffset( tivo, readChunk, &fileoffset );

   if ( stream_seek( demux->stream, fileoffset ) != 1 )
   {
      mp_msg( MSGT_DEMUX, MSGL_ERR, "Read past EOF()\n" );
      return( 0 );
   }
   count = stream_read( demux->stream, buff, size );
	demux->filepos = stream_tell( demux->stream );

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "tmf_load_chunk() count %x\n", 
		count );

   mp_msg( MSGT_DEMUX, MSGL_DBG3, 
		"tmf_load_chunk() bytes %x %x %x %x %x %x %x %x\n", 
		buff[ 0 ], buff[ 1 ], buff[ 2 ], buff[ 3 ],
		buff[ 4 ], buff[ 5 ], buff[ 6 ], buff[ 7 ] );

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "tmf_load_chunk() end\n" );
    
   return( count );
}

// ===========================================================================

// DTiVo MPEG 336, 480, 576, 768
// SA TiVo 864
// DTiVo AC-3 1550
//
#define SERIES1_PTS_LENGTH         ( 11 )
#define SERIES1_PTS_OFFSET         ( 6 )
#define SERIES2_PTS_LENGTH         ( 16 )
#define SERIES2_PTS_OFFSET         ( 9 )
#define AC3_PTS_LENGTH             ( 16 )
#define AC3_PTS_OFFSET             ( 9 )

#define NUMBER_DIFFERENT_AUDIO_SIZES ( 7 )
static int Series1AudioWithPTS[ NUMBER_DIFFERENT_AUDIO_SIZES ] = 
{ 
   336 + SERIES1_PTS_LENGTH, 
   384 + SERIES1_PTS_LENGTH, 
   480 + SERIES1_PTS_LENGTH, 
   576 + SERIES1_PTS_LENGTH, 
   768 + SERIES1_PTS_LENGTH, 
   864 + SERIES1_PTS_LENGTH 
};
static int Series2AudioWithPTS[ NUMBER_DIFFERENT_AUDIO_SIZES ] = 
{ 
   336 + SERIES2_PTS_LENGTH, 
   384 + SERIES2_PTS_LENGTH, 
   480 + SERIES2_PTS_LENGTH, 
   576 + SERIES2_PTS_LENGTH, 
   768 + SERIES2_PTS_LENGTH, 
   864 + SERIES2_PTS_LENGTH 
};

static int IsValidAudioPacket( int size, int *ptsOffset, int *ptsLen )
{
   int count;

   *ptsOffset = 0;
   *ptsLen = 0;

   // AC-3
   if ( ( size == 1550 ) || ( size == 1552 ) )
   {
      *ptsOffset = AC3_PTS_OFFSET;
      *ptsLen = AC3_PTS_LENGTH;
      return( 1 );
   }

   // MPEG
   for( count = 0 ; count < NUMBER_DIFFERENT_AUDIO_SIZES ; count++ )
   {
      if ( size == Series1AudioWithPTS[ count ] )
      {
         *ptsOffset = SERIES1_PTS_OFFSET;
         *ptsLen = SERIES1_PTS_LENGTH;
         break;
      }
   }
   if ( *ptsOffset == 0 )
   {
      for( count = 0 ; count < NUMBER_DIFFERENT_AUDIO_SIZES ; count++ )
      {
         if ( size == Series2AudioWithPTS[ count ] )
         {
            *ptsOffset = SERIES2_PTS_OFFSET;
            *ptsLen = SERIES2_PTS_LENGTH;
            break;
         }
      }
   }
   if ( *ptsOffset == 0 )
   {
      mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Tossing Audio Packet Size %d\n", 
         size );
      return( 0 );
   }
   else
   {
      return( 1 );
   }
}


static float get_ty_pts( unsigned char *buf )
{
   float result = 0;
   unsigned char temp;

   temp = ( buf[ 0 ] & 0xE ) >> 1;
   result = ( (float) temp ) * ( (float) ( 1L << 30 ) ) / ( (float)PTS_KHZ );
   temp = buf[ 1 ];
   result += ( (float) temp ) * ( (float) ( 1L << 22 ) ) / ( (float)PTS_KHZ );
   temp = ( buf[ 2 ] & 0xFE ) >> 1;
   result += ( (float) temp ) * ( (float) ( 1L << 15 ) ) / ( (float)PTS_KHZ );
   temp = buf[ 3 ];
   result += ( (float) temp ) * ( (float) ( 1L << 7 ) ) / ( (float)PTS_KHZ );
   temp = ( buf[ 4 ] & 0xFE ) >> 1;
   result += ( (float) temp ) / ( (float)PTS_MHZ );

   return result;
}

static void demux_ty_AddToAudioBuffer( TiVoInfo *tivo, unsigned char *buffer, 
   int size )
{
   if ( ( tivo->lastAudioEnd + size ) < MAX_AUDIO_BUFFER )
   {
      memcpy( &( tivo->lastAudio[ tivo->lastAudioEnd ] ), 
         buffer, size );
      tivo->lastAudioEnd += size;
   }
   else
   {
      mp_msg( MSGT_DEMUX, MSGL_ERR,
         "ty:WARNING - Would have blown my audio buffer\n" );
   }
}

static void demux_ty_CopyToDemuxPacket( int type, TiVoInfo *tivo, demux_stream_t *ds, 
	unsigned char *buffer, int size, off_t pos, float pts )
{
   demux_packet_t   *dp;

   // mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Calling ds_add_packet() %7.1f\n", pts );
   // printf( "%x %x %x %x\n", 
   //    buffer[ 0 ], buffer[ 1 ], buffer[ 2 ], buffer[ 3 ] );

   dp = new_demux_packet( size );
   memcpy( dp->buffer, buffer, size );
   dp->pts = pts;
   dp->pos = pos;
   dp->flags = 0;
   ds_add_packet( ds, dp );
	ds->pts = pts;
	if ( type == TY_V )
	{
		if ( tivo->firstVideoPTS == -1 )
		{
			tivo->firstVideoPTS = pts;
		}
	}
	if ( type == TY_A )
	{
		if ( tivo->firstAudioPTS == -1 )
		{
			tivo->firstAudioPTS = pts;
		}
	}
}

static int demux_ty_FindESHeader( unsigned char *header, int headerSize, 
   unsigned char *buffer, int bufferSize, int *esOffset1 )
{
   int count;

   *esOffset1 = -1;
   for( count = 0 ; count < bufferSize ; count++ )
   {
      if ( ( buffer[ count + 0 ] == header[ 0 ] ) &&
         ( buffer[ count + 1 ] == header[ 1 ] ) &&
         ( buffer[ count + 2 ] == header[ 2 ] ) &&
         ( buffer[ count + 3 ] == header[ 3 ] ) )
      {
         *esOffset1 = count;
         return( 1 );
      }
   }
   return( -1 );
}

static void demux_ty_FindESPacket( unsigned char *header, int headerSize, 
   unsigned char *buffer, int bufferSize, int *esOffset1, int *esOffset2 )
{
   int count;

   *esOffset1 = -1;
   *esOffset2 = -1;

   for( count = 0 ; count < bufferSize ; count++ )
   {
      if ( ( buffer[ count + 0 ] == header[ 0 ] ) &&
         ( buffer[ count + 1 ] == header[ 1 ] ) &&
         ( buffer[ count + 2 ] == header[ 2 ] ) &&
         ( buffer[ count + 3 ] == header[ 3 ] ) )
      {
         *esOffset1 = count;
         break;
      }
   }

   if ( *esOffset1 != -1 )
   {
      for( count = *esOffset1 + 1 ; 
         count < bufferSize ; count++ )
      {
         if ( ( buffer[ count + 0 ] == header[ 0 ] ) &&
            ( buffer[ count + 1 ] == header[ 1 ] ) &&
            ( buffer[ count + 2 ] == header[ 2 ] ) &&
            ( buffer[ count + 3 ] == header[ 3 ] ) )
         {
            *esOffset2 = count;
            break;
         }
      }
   }
}

static int tivobuffer2hostlong( unsigned char *buffer )
{
   return
   ( 
      buffer[ 0 ] << 24 | buffer[ 1 ] << 16 | buffer[ 2 ] << 8 | buffer[ 3 ]
   );
}

static unsigned char ty_VideoPacket[] = { 0x00, 0x00, 0x01, 0xe0 };
static unsigned char ty_MPEGAudioPacket[] = { 0x00, 0x00, 0x01, 0xc0 };
static unsigned char ty_AC3AudioPacket[] = { 0x00, 0x00, 0x01, 0xbd };

static int demux_ty_fill_buffer( demuxer_t *demux, demux_stream_t *dsds )
{
   int              invalidType = 0;
   int              errorHeader = 0;
   int              recordsDecoded = 0;
   off_t            filePos = 0;

   unsigned char    chunk[ CHUNKSIZE ];
   int              whichChunk;
   int              readSize;
   unsigned int     pesFileId = 0;

   int              numberRecs;
   unsigned char    *recPtr;
   int              offset;
   int              size;

   int              type;
   int              nybbleType;

   int              counter;

   int              aid;
   demux_stream_t   *ds = NULL;
   
   int              esOffset1;
   int              esOffset2;

   unsigned char    lastCC[ 16 ];
   unsigned char    lastXDS[ 16 ];

   TiVoInfo         *tivo = 0;

   if ( demux->stream->type == STREAMTYPE_DVD )
	{
		return( 0 );
	}

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:ty processing\n" );
   if ( ( demux->a_streams[ MAX_A_STREAMS - 1 ] ) == 0 )
   {
      demux->a_streams[ MAX_A_STREAMS - 1 ] = malloc( sizeof( TiVoInfo ) );
      tivo = demux->a_streams[ MAX_A_STREAMS - 1 ];
      memset( tivo, 0, sizeof( TiVoInfo ) );
      tivo->firstAudioPTS = -1;
      tivo->firstVideoPTS = -1;
   }
   else
   {
      tivo = demux->a_streams[ MAX_A_STREAMS - 1 ];
   }

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
         tivo->readHeader = 1;
         tivo->size = demux->stream->end_pos;

         filePos = demux->filepos;
         stream_seek( demux->stream, 0 );

         // mp_msg( MSGT_DEMUX, MSGL_DBG3, 
			// 	"ty:Reading a chunk %d\n", __LINE__ );

         readSize = stream_read( demux->stream, chunk, CHUNKSIZE );

         if ( memcmp( chunk, TMF_SIG, sizeof( TMF_SIG ) ) == 0 )
         {
            mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Detected a tmf\n" );
            tivo->tmf = 1;
            ty_tmf_filetoparts( demux, tivo );
            readSize = tmf_load_chunk( demux, tivo, chunk, CHUNKSIZE, 0 );
         }

         if ( readSize == CHUNKSIZE )
         {
            tivo->pesFileId = tivobuffer2hostlong( &chunk[ 0x00 ] );
            tivo->streamType = tivobuffer2hostlong( &chunk[ 0x04 ] );
            tivo->chunkSize = tivobuffer2hostlong( &chunk[ 0x08 ] );

            if ( tivo->pesFileId == TIVO_PES_FILEID )
            {
               off_t numberParts;

               readSize = 0;
               
               if ( tivo->tmf != 1 )
               {
                  off_t size;
                  off_t offset;

                  numberParts = demux->stream->end_pos / TIVO_PART_LENGTH;
                  offset = numberParts * TIVO_PART_LENGTH;

                  mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:ty/ty+Number Parts %"PRId64"\n",
                    (int64_t)numberParts );

                  if ( ( offset + CHUNKSIZE ) < demux->stream->end_pos )
                  {
                     stream_seek( demux->stream, offset );
                     readSize = stream_read( demux->stream, chunk, CHUNKSIZE );
                  }
               }
               else
               {
                  numberParts = tivo->tmf_totalparts;
                  offset = numberParts * TIVO_PART_LENGTH;
                  readSize = tmf_load_chunk( demux, tivo, chunk, CHUNKSIZE, 
                     ( numberParts * ( TIVO_PART_LENGTH - 0x20000 ) / 
                     CHUNKSIZE ) );
               }

               if ( readSize == CHUNKSIZE )
               {
                  pesFileId = tivobuffer2hostlong( &chunk[ 0x00 ] );
                  if ( pesFileId == TIVO_PES_FILEID )
                  {
                     size = tivobuffer2hostlong( &chunk[ 0x0c ] );
                     size /= 256;
                     size -= 4;
                     size *= CHUNKSIZE;
                     tivo->size = numberParts * TIVO_PART_LENGTH;
                     tivo->size += size;
                     mp_msg( MSGT_DEMUX, MSGL_DBG3, 
                        "ty:Header Calc Stream Size %"PRId64"\n", tivo->size );
                  }
               }
            }
         }
         if ( tivo->size > demux->stream->end_pos )
         {
            tivo->size = demux->stream->end_pos;
         }

			if ( demux->stream->start_pos > 0 )
			{
				filePos = demux->stream->start_pos;
			}
         stream_seek( demux->stream, filePos );
			demux->filepos = stream_tell( demux->stream );
         tivo->whichChunk = ( filePos / CHUNKSIZE );
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

   if ( tivo->size > 0 )
   {
      if ( stream_tell( demux->stream ) > tivo->size )
      {
         demux->stream->eof = 1;
         return( 0 );
      }
   }

   if ( tivo->tmf != 1 )
   {
      // Make sure we are on a 128k boundary
      if ( ( demux->filepos % CHUNKSIZE ) != 0 )
      {
         whichChunk = demux->filepos / CHUNKSIZE;
         if ( ( demux->filepos % CHUNKSIZE ) > ( CHUNKSIZE / 2 ) )
         {
            whichChunk++;
         }
         stream_seek( demux->stream, ( whichChunk * CHUNKSIZE ) );
      }

      demux->filepos = stream_tell( demux->stream );
      tivo->whichChunk = demux->filepos / CHUNKSIZE;
      readSize = stream_read( demux->stream, chunk, CHUNKSIZE );
      if ( readSize != CHUNKSIZE )
      {
         return( 0 );
      }
   }
   else
   {
      readSize = tmf_load_chunk( demux, tivo, chunk, CHUNKSIZE, 
         tivo->whichChunk );
      if ( readSize != CHUNKSIZE )
      {
         return( 0 );
      }
      tivo->whichChunk++;
   }

   // We found a part header, skip it
   pesFileId = tivobuffer2hostlong( &chunk[ 0x00 ] );
   if( pesFileId == TIVO_PES_FILEID )
   {
      mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Skipping PART Header\n" );
      if ( tivo->tmf != 1 )
      {
         demux->filepos = stream_tell( demux->stream );
         readSize = stream_read( demux->stream, chunk, CHUNKSIZE );
      }
      else
      {
         readSize = tmf_load_chunk( demux, tivo, chunk, CHUNKSIZE, 
            tivo->whichChunk );
         tivo->whichChunk++;
      }

      if ( readSize != CHUNKSIZE )
      {
         return( 0 );
      }
   }   
   mp_msg( MSGT_DEMUX, MSGL_DBG3,
      "\nty:actual current offset %"PRIx64"\n", ( stream_tell( demux->stream ) - 
		0x20000 ) );


   // Let's make a Video Demux Stream for MPlayer
   aid = 0x0;
   if( !demux->v_streams[ aid ] ) new_sh_video( demux, aid );
   if( demux->video->id == -1 ) demux->video->id = aid;
   if( demux->video->id == aid )
   {
      ds = demux->video;
      if( !ds->sh ) ds->sh = demux->v_streams[ aid ];
   }

   // ======================================================================
   // Finally, we get to actually parse the chunk
   // ======================================================================
   mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:ty parsing a chunk\n" );
   numberRecs = chunk[ 0 ];
   recPtr = &chunk[ 4 ];
   offset = ( numberRecs * 16 ) + 4;
   for ( counter = 0 ; counter < numberRecs ; counter++ )
   {
      size = ( recPtr[ 0 ] << 8 | recPtr[ 1 ] ) << 4 | ( recPtr[ 2 ] >> 4 );
      type = recPtr[ 3 ];
      nybbleType = recPtr[ 2 ] & 0x0f;
      recordsDecoded++;

      mp_msg( MSGT_DEMUX, MSGL_DBG3,
         "ty:Record Type %x/%x %d\n", nybbleType, type, size );

      // ================================================================
      // Video Parsing
      // ================================================================
      if ( type == 0xe0 )
      {
         if ( ( size > 0 ) && ( ( size + offset ) <= CHUNKSIZE ) )
         {
#if 0
            printf( "Video Chunk Header " );
            for( count = 0 ; count < 24 ; count++ )
            {
               printf( "%2.2x ", chunk[ offset + count ] );
            }
            printf( "\n" );
#endif
            demux_ty_FindESHeader( ty_VideoPacket, 4, &chunk[ offset ], 
               size, &esOffset1 );
            if ( esOffset1 != -1 )
            {
               tivo->lastVideoPTS = get_ty_pts( 
                  &chunk[ offset + esOffset1 + 9 ] );
               mp_msg( MSGT_DEMUX, MSGL_DBG3, "Video PTS %7.1f\n", 
                  tivo->lastVideoPTS );
            }

            // Do NOT Pass the PES Header onto the MPEG2 Decode
            if( nybbleType != 0x06 )
            { 
               demux_ty_CopyToDemuxPacket( TY_V, tivo, demux->video, 
						&chunk[ offset ], size, ( demux->filepos + offset ), 
						tivo->lastVideoPTS );
            }
            offset += size;
         }
			else
			{
				errorHeader++;
			}
      }
      // ================================================================
      // Audio Parsing
      // ================================================================
		else if ( type == 0xc0 )
      {
         if ( ( size > 0 ) && ( ( size + offset ) <= CHUNKSIZE ) )
         {
#if 0
            printf( "Audio Chunk Header " );
            for( count = 0 ; count < 24 ; count++ )
            {
               printf( "%2.2x ", chunk[ offset + count ] );
            }
            printf( "\n" );
#endif

            if( demux->audio->id == -1 )
            {
               if ( nybbleType == 0x02 )
               {
                  continue;    // DTiVo inconclusive, wait for more
               }
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
                  ds = demux->audio;
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
            if ( nybbleType == 0x02 )
            {
               if ( tivo->tivoType == 2 )
               {
                  demux_ty_AddToAudioBuffer( tivo, &chunk[ offset ], size );
               }
               else
               {

                  mp_msg( MSGT_DEMUX, MSGL_DBG3,
                     "ty:Adding Audio Packet Size %d\n", size );
                  demux_ty_CopyToDemuxPacket( TY_A, tivo, demux->audio, 
							&chunk[ offset ], size, ( demux->filepos + offset ), 
							tivo->lastAudioPTS );
               }
            }

            // MPEG Audio with PES Header, either SA or DTiVo
            // ================================================
            if ( nybbleType == 0x03 )
            {
               demux_ty_FindESHeader( ty_MPEGAudioPacket, 4, &chunk[ offset ], 
                  size, &esOffset1 );

               // SA PES Header, No Audio Data
               // ================================================
               if ( ( esOffset1 == 0 ) && ( size == 16 ) )
               {
                  tivo->tivoType = 1;
                  tivo->lastAudioPTS = get_ty_pts( &chunk[ offset + 
                     SERIES2_PTS_OFFSET ] );
                  mp_msg( MSGT_DEMUX, MSGL_DBG3, "SA Audio PTS %7.1f\n", 
                     tivo->lastAudioPTS );
               }
               else
               // DTiVo Audio with PES Header
               // ================================================
               {
                  tivo->tivoType = 2;

                  demux_ty_AddToAudioBuffer( tivo, &chunk[ offset ], size );
                  demux_ty_FindESPacket( ty_MPEGAudioPacket, 4,
                     tivo->lastAudio, tivo->lastAudioEnd, &esOffset1,
                     &esOffset2 );

                  if ( ( esOffset1 != -1 ) && ( esOffset2 != -1 ) )
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
                        mp_msg( MSGT_DEMUX, MSGL_DBG3, 
                           "MPEG Audio PTS %7.1f\n", tivo->lastAudioPTS );

                        demux_ty_CopyToDemuxPacket
                        ( 
								   TY_A,
								   tivo,
                           demux->audio, 
                           &( tivo->lastAudio[ esOffset1 + headerSize ] ), 
                           ( packetSize - headerSize ),
                           ( demux->filepos + offset ), 
                           tivo->lastAudioPTS 
                        );

                     }

                     // Collapse the Audio Buffer
                     memmove( &(tivo->lastAudio[ 0 ] ), 
                        &( tivo->lastAudio[ esOffset2 ] ), 
                        ( tivo->lastAudioEnd - esOffset2 ) );
                     tivo->lastAudioEnd -= esOffset2;
                  }
               }
            }

            // SA Audio with no PES Header
            // ================================================
            if ( nybbleType == 0x04 )
            {
               mp_msg( MSGT_DEMUX, MSGL_DBG3,
                  "ty:Adding Audio Packet Size %d\n", size );
               demux_ty_CopyToDemuxPacket( TY_A, tivo, demux->audio, 
						&chunk[ offset ], size, ( demux->filepos + offset ), 
						tivo->lastAudioPTS );
            }

            // DTiVo AC3 Audio Data with PES Header
            // ================================================
            if ( nybbleType == 0x09 )
            {
               tivo->tivoType = 2;

               demux_ty_AddToAudioBuffer( tivo, &chunk[ offset ], size );
               demux_ty_FindESPacket( ty_AC3AudioPacket, 4,
                  tivo->lastAudio, tivo->lastAudioEnd, &esOffset1,
                  &esOffset2 );

               if ( ( esOffset1 != -1 ) && ( esOffset2 != -1 ) )
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
                     mp_msg( MSGT_DEMUX, MSGL_DBG3, 
                        "AC3 Audio PTS %7.1f\n", tivo->lastAudioPTS );

                     // AC3 Decoder WANTS the PTS
                     demux_ty_CopyToDemuxPacket
                     ( 
							   TY_A,
							   tivo,
                        demux->audio, 
                        &( tivo->lastAudio[ esOffset1 ] ), 
                        ( packetSize ),
                        ( demux->filepos + offset ), 
                        tivo->lastAudioPTS 
                     );

                  }

                  // Collapse the Audio Buffer
                  memmove( &(tivo->lastAudio[ 0 ] ), 
                     &( tivo->lastAudio[ esOffset2 ] ), 
                     ( tivo->lastAudioEnd - esOffset2 ) );
                  tivo->lastAudioEnd -= esOffset2;
               }
            }
            offset += size;
         }
			else
			{
				errorHeader++;
			}
		}
      // ================================================================
      // Closed Caption
      // ================================================================
      else if ( type == 0x01 )
		{
			unsigned char b1;
			unsigned char b2;

			b1 = ( ( ( recPtr[ 0 ] & 0x0f ) << 4 ) | 
				( ( recPtr[ 1 ] & 0xf0 ) >> 4 ) );
			b1 &= 0x7f;
			b2 = ( ( ( recPtr[ 1 ] & 0x0f ) << 4 ) | 
				( ( recPtr[ 2 ] & 0xf0 ) >> 4 ) );
			b2 &= 0x7f;

			mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:CC %x %x\n", b1, b2 );

			lastCC[ 0x00 ] = 0x00;
			lastCC[ 0x01 ] = 0x00;
			lastCC[ 0x02 ] = 0x01;
			lastCC[ 0x03 ] = 0xb2;
			lastCC[ 0x04 ] = 'T';
			lastCC[ 0x05 ] = 'Y';
			lastCC[ 0x06 ] = 0x01;
			lastCC[ 0x07 ] = b1;
			lastCC[ 0x08 ] = b2;
         if ( subcc_enabled )
         {
			   demux_ty_CopyToDemuxPacket( TY_V, tivo, demux->video, lastCC, 0x09,
  				   ( demux->filepos + offset ), tivo->lastVideoPTS );
         }
		}
      // ================================================================
      // Extended Data Services
      // ================================================================
		else if ( type == 0x02 )
		{
			unsigned char b1;
			unsigned char b2;

			b1 = ( ( ( recPtr[ 0 ] & 0x0f ) << 4 ) | 
				( ( recPtr[ 1 ] & 0xf0 ) >> 4 ) );
			b1 &= 0x7f;
			b2 = ( ( ( recPtr[ 1 ] & 0x0f ) << 4 ) | 
				( ( recPtr[ 2 ] & 0xf0 ) >> 4 ) );
			b2 &= 0x7f;

         mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:XDS %x %x\n", b1, b2 );

			lastXDS[ 0x00 ] = 0x00;
			lastXDS[ 0x01 ] = 0x00;
			lastXDS[ 0x02 ] = 0x01;
			lastXDS[ 0x03 ] = 0xb2;
			lastXDS[ 0x04 ] = 'T';
			lastXDS[ 0x05 ] = 'Y';
			lastXDS[ 0x06 ] = 0x02;
			lastXDS[ 0x07 ] = b1;
			lastXDS[ 0x08 ] = b2;
         if ( subcc_enabled )
         {
			   demux_ty_CopyToDemuxPacket( TY_V, tivo, demux->video, lastXDS, 0x09,
				   ( demux->filepos + offset ), tivo->lastVideoPTS );
         }
		}
      // ================================================================
      // Found a 0x03 on Droid's TiVo, I have no idea what it is
      // ================================================================
		else if ( type == 0x03 )
		{
         if ( ( size > 0 ) && ( ( size + offset ) <= CHUNKSIZE ) )
         {
            offset += size;
			}
		}
      // ================================================================
      // Found a 0x03 on Hermit's TiVo, I have no idea what it is
      // ================================================================
		else if ( type == 0x03 )
		{
         if ( ( size > 0 ) && ( ( size + offset ) <= CHUNKSIZE ) )
         {
            offset += size;
			}
		}
      // ================================================================
      // Unknown
      // ================================================================
		else if ( type == 0x05 )
		{
         if ( ( size > 0 ) && ( ( size + offset ) <= CHUNKSIZE ) )
         {
            offset += size;
			}
		}
		else
		{
         if ( ( size > 0 ) && ( ( size + offset ) <= CHUNKSIZE ) )
         {
            offset += size;
			}
         mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Invalid Type %x\n", type );
			invalidType++;
		}
     recPtr += 16;
   }

   if ( errorHeader > 0 || invalidType > 0 )
   {
      mp_msg( MSGT_DEMUX, MSGL_DBG3, 
         "ty:Error Check - Records %d, Parsed %d, Errors %d + %d\n",
         numberRecs, recordsDecoded, errorHeader, invalidType );

      // Invalid MPEG ES Size Check
      if ( errorHeader > ( numberRecs / 2 ) )
      {
         return( 0 );
      }

      // Invalid MPEG Stream Type Check
      if ( invalidType > ( numberRecs / 2 ) )
      {
         return( 0 );
		}
   }

   demux->filepos = stream_tell( demux->stream );

   return( 1 );
}

static void demux_seek_ty( demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags )
{
   demux_stream_t *d_audio = demuxer->audio;
   demux_stream_t *d_video = demuxer->video;
   sh_audio_t     *sh_audio = d_audio->sh;
   sh_video_t     *sh_video = d_video->sh;
   off_t          newpos;
   off_t          res;
   TiVoInfo       *tivo = 0;

   mp_msg( MSGT_DEMUX, MSGL_DBG3, "ty:Seeking to %7.1f\n", rel_seek_secs );

   if ( ( demuxer->a_streams[ MAX_A_STREAMS - 1 ] ) != 0 )
   {
      tivo = demuxer->a_streams[ MAX_A_STREAMS - 1 ];
      tivo->lastAudioEnd = 0;
      tivo->lastAudioPTS = 0;
      tivo->lastVideoPTS = 0;
   }
   //
   //================= seek in MPEG ==========================
   demuxer->filepos = stream_tell( demuxer->stream );

   newpos = ( flags & 1 ) ? demuxer->movi_start : demuxer->filepos;
	
   if( flags & 2 )
   {
	   // float seek 0..1
	   newpos += ( demuxer->movi_end - demuxer->movi_start ) * rel_seek_secs;
   } 
   else 
   {
	   // time seek (secs)
      if( ! sh_video->i_bps ) // unspecified or VBR
      {
         newpos += 2324 * 75 * rel_seek_secs; // 174.3 kbyte/sec
      }
      else
      {
         newpos += sh_video->i_bps * rel_seek_secs;
      }
   }

   if ( newpos < demuxer->movi_start )
   {
	   if( demuxer->stream->type != STREAMTYPE_VCD ) demuxer->movi_start = 0;
	   if( newpos < demuxer->movi_start ) newpos = demuxer->movi_start;
	}

   res = newpos / CHUNKSIZE;
   if ( rel_seek_secs >= 0 )
   {
      newpos = ( res + 1 ) * CHUNKSIZE;
   }
   else
   {
      newpos = res * CHUNKSIZE;
   }

   if ( newpos < 0 )
   {
      newpos = 0;
   }

   tivo->whichChunk = newpos / CHUNKSIZE;

   stream_seek( demuxer->stream, newpos );

   // re-sync video:
   videobuf_code_len = 0; // reset ES stream buffer

	ds_fill_buffer( d_video );
	if( sh_audio )
   {
	  ds_fill_buffer( d_audio );
	}

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
	{
	   ty_ClearOSD( 0 );
	}
}

int demux_ty_control( demuxer_t *demuxer,int cmd, void *arg )
{
   demux_stream_t *d_video = demuxer->video;
   sh_video_t     *sh_video = d_video->sh;

   switch(cmd) 
   {
	   case DEMUXER_CTRL_GET_TIME_LENGTH:
	      if(!sh_video->i_bps)  // unspecified or VBR 
    		   return DEMUXER_CTRL_DONTKNOW;
	      *((double *)arg)=
            ((double)demuxer->movi_end-demuxer->movi_start)/sh_video->i_bps;
	      return DEMUXER_CTRL_GUESS;

	   case DEMUXER_CTRL_GET_PERCENT_POS:
    		   return DEMUXER_CTRL_DONTKNOW;
	    default:
	       return DEMUXER_CTRL_NOTIMPL;
    }
}


static void demux_close_ty( demuxer_t *demux )
{
   TiVoInfo         *tivo = 0;

   if ( ( demux->a_streams[ MAX_A_STREAMS - 1 ] ) != 0 )
   {
      tivo = demux->a_streams[ MAX_A_STREAMS - 1 ];
      free( tivo );
      demux->a_streams[ MAX_A_STREAMS - 1 ] = 0;
	   sub_justify = 0;
   }
}


static int ty_check_file(demuxer_t* demuxer)
{
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


demuxer_desc_t demuxer_desc_mpeg_ty = {
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
