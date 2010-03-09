// Most of this was written by Mike Baker <mbm@linux.com>
// and released under the GPL v2+ license.
//
// Modifications and SEVERE cleanup of the code was done by
// Christopher Wingert
// Copyright 2003
//
// Released under GPL2 License.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

//#include "stream/stream.h"
//#include "demuxer.h"
//#include "parse_es.h"
//#include "stheader.h"
//#include "mp3_hdr.h"
//#include "subreader.h"
#include "sub_cc.h"
#include "libvo/sub.h"
#include "demux_ty_osd.h"

//#include "dvdauth.h"

extern int sub_justify;

#define TY_TEXT_MODE        ( 1 << 0 )
#define TY_OSD_MODE         ( 1 << 1 )

static int TY_OSD_flags = TY_TEXT_MODE | TY_OSD_MODE;
static int TY_OSD_debug = 0;

// ===========================================================================
// Closed Caption Decoding and OSD Presentation
// ===========================================================================
#define TY_CCNONE     ( -3 )
#define TY_CCTEXTMODE ( -2 )
#define TY_CCPOPUPNB  ( -1 )
#define TY_CCPOPUP    (  0 )
#define TY_CCPAINTON  (  1 )

#define TY_CC_MAX_X   ( 45 )

static int      TY_CC_CUR_X;
static int      TY_CC_CUR_Y;
static int      TY_CC_stat = TY_CCNONE;
static char     TY_CC_buf[ 255 ];
static char     *TY_CC_ptr = TY_CC_buf;
static unsigned TY_CC_lastcap = 0;
static int      TY_CC_TextItalic;
static int      TY_CC_Y_Offset;

static subtitle ty_OSD1;
static subtitle ty_OSD2;
static subtitle *ty_pOSD1;
static subtitle *ty_pOSD2;
static int             tyOSDInitialized = 0;
static int             tyOSDUpdate = 0;

static void ty_DrawOSD(void)
{
	// printf( "Calling ty_DrawOSD()\n" );
	tyOSDUpdate = 1;
}

void ty_ClearOSD( int start )
{
	int index;
	// printf( "Calling ty_ClearOSD()\n" );
   for ( index = start ; index < SUB_MAX_TEXT ; index++ )
	{
		memset( ty_OSD1.text[ index ], ' ', TY_CC_MAX_X - 1 );
		ty_OSD1.text[ index ][ TY_CC_MAX_X - 1 ] = 0;
		memset( ty_OSD2.text[ index ], ' ', TY_CC_MAX_X - 1 );
		ty_OSD2.text[ index ][ TY_CC_MAX_X - 1 ] = 0;
	}
}

static void ty_DrawChar( int *x, int *y, char disChar, int fgColor, int bgColor )
{
   int cx;
   int cy;

   cx = *x;
   cy = *y;

	if ( *x >= ( TY_CC_MAX_X - 1 ) )
	{
      cx = 0;
	}
	if ( ( *y + TY_CC_Y_Offset ) > SUB_MAX_TEXT )
	{
	   cy = SUB_MAX_TEXT - TY_CC_Y_Offset - 1;
   }

	// printf( "Calling ty_DrawChar() x:%d y:%d %c fg:%d bg:%d\n",
	// 	cx, cy, disChar, fgColor, bgColor );

   ty_OSD1.text[ TY_CC_Y_Offset + cy ][ cx ] = disChar;
   memset( &( ty_OSD1.text[ TY_CC_Y_Offset + cy ][ cx + 1 ] ), ' ',
      TY_CC_MAX_X - cx - 2 );
	( *x )++;
}

static void ty_RollupBuf( int dest, int source, int numLines )
{
	int index;

	// printf( "Calling ty_RollupBuf() dest:%d source %d, numLines %d\n",
	//    dest, source, numLines );
	//
	if ( ( source + TY_CC_Y_Offset + numLines ) > SUB_MAX_TEXT )
	{
      ty_ClearOSD( 1 );
		return;
	}

	if ( ( source + TY_CC_Y_Offset + numLines ) < 0 )
	{
      ty_ClearOSD( 1 );
		return;
	}

	if ( numLines > SUB_MAX_TEXT )
	{
      ty_ClearOSD( 1 );
		return;
	}

	for ( index = 0 ; index < numLines ; index++ )
	{
		strcpy( ty_OSD1.text[ TY_CC_Y_Offset + dest ],
			ty_OSD1.text[ TY_CC_Y_Offset + source ] );
	   dest++;
		source++;
	}
	memset( ty_OSD1.text[ TY_CC_Y_Offset + source - 1 ], ' ', TY_CC_MAX_X - 1 );
	ty_OSD1.text[ TY_CC_Y_Offset + source - 1 ][ TY_CC_MAX_X - 1 ] = 0;
}

static void ty_drawchar( char c )
{
   if ( c < 2 ) return;

   if ( TY_OSD_flags & TY_OSD_MODE && TY_CC_stat != TY_CCNONE &&
      TY_CC_CUR_Y != -1 )
      ty_DrawChar( &TY_CC_CUR_X, &TY_CC_CUR_Y, c, 4, 13 );

   if ( TY_CC_ptr - TY_CC_buf > sizeof( TY_CC_buf ) - 1 )
   {        // buffer overflow
      TY_CC_ptr = TY_CC_buf;
      memset( TY_CC_buf, 0, sizeof( TY_CC_buf ) );
   }
   *( TY_CC_ptr++ ) = ( c == 14 ) ? '/' : c; // swap a '/' for musical note
}

static void ty_draw(void)
{
   if ( TY_CC_ptr != TY_CC_buf && TY_OSD_flags & TY_TEXT_MODE )
   {
      if ( *( TY_CC_ptr - 1 ) == '\n' ) *( TY_CC_ptr - 1 ) = 0;

      mp_msg( MSGT_DEMUX, MSGL_V, "CC: %s\n", TY_CC_buf );
  }
  TY_CC_lastcap = time( NULL );

  TY_CC_ptr = TY_CC_buf;
  memset( TY_CC_buf, 0, sizeof( TY_CC_buf) );

  if ( TY_OSD_flags & TY_OSD_MODE ) ty_DrawOSD();
  if ( TY_CC_TextItalic ) TY_CC_TextItalic = 0;
}


static int CC_last = 0;
static char CC_mode = 0;
static int CC_row[] =
{
   11, -1, 1, 2, 3, 4, 12, 13, 14, 15, 5, 6, 7, 8, 9, 10
};

// char specialchar[] = { '®', '°', '½', '¿', '*', '¢', '£', 14, 'à', ' ', 'è', 'â', 'ê', 'î', 'ô', 'û' };

static int ty_CCdecode( char b1, char b2 )
{
   int x;
   int data = ( b2 << 8 ) + b1;

   if ( b1 & 0x60 )                // text
   {
       if ( !TY_OSD_debug && TY_CC_stat == TY_CCNONE ) return 0;
       if ( TY_OSD_debug > 3 )
       {
          mp_msg( MSGT_DEMUX, MSGL_DBG3, "%c %c", b1, b2 );
       }
       ty_drawchar( b1 );
       ty_drawchar( b2 );

       if ( TY_CC_stat > 0 && TY_OSD_flags & TY_OSD_MODE ) ty_DrawOSD();
   }
   else if ( ( b1 & 0x10 ) && ( b2 > 0x1F ) && ( data != CC_last ) )
   {
      #define CURRENT ( ( b1 & 0x08 ) >> 3 )

      if ( CC_mode != CURRENT && TY_CC_stat != TY_CCNONE )
      {
         if ( TY_OSD_debug && TY_CC_ptr != TY_CC_buf ) ty_draw();
         TY_CC_stat = TY_CCNONE;
         return 0;
      }

      if ( TY_CC_stat == TY_CCNONE || TY_CC_CUR_Y == -1 )
      {
         if ( TY_CC_ptr != TY_CC_buf )
         {
            if ( TY_OSD_debug )
               mp_msg( MSGT_DEMUX, MSGL_DBG3, "(TY_OSD_debug) %s\n",
                  TY_CC_buf );
            TY_CC_ptr = TY_CC_buf;
            memset(TY_CC_buf, 0, sizeof(TY_CC_buf));
         }

         if ( CC_mode != CURRENT ) return 0;
      }

      // preamble address code (row & indent)
      if ( b2 & 0x40 )
      {
         TY_CC_CUR_Y = CC_row[ ( ( b1 << 1 ) & 14 ) | ( ( b2 >> 5 ) & 1 ) ];

			// Offset into MPlayer's Buffer
			if ( ( TY_CC_CUR_Y >= 1 ) && ( TY_CC_CUR_Y <= 4 ) )
			{
				TY_CC_Y_Offset = SUB_MAX_TEXT - 5 - 1;
			}
			if ( ( TY_CC_CUR_Y >= 5 ) && ( TY_CC_CUR_Y <= 10 ) )
			{
				TY_CC_Y_Offset = SUB_MAX_TEXT - 5 - 5;
			}
			if ( ( TY_CC_CUR_Y >= 12 ) && ( TY_CC_CUR_Y <= 15 ) )
			{
				TY_CC_Y_Offset = SUB_MAX_TEXT - 5 - 12;
			}

         if ( TY_OSD_debug > 3 )
            mp_msg( MSGT_DEMUX, MSGL_DBG3, "<< preamble %d >>\n", TY_CC_CUR_Y );

         // we still have something in the text buffer
         if (TY_CC_ptr != TY_CC_buf)
         {
            *(TY_CC_ptr++) = '\n';
            if ( TY_CC_TextItalic )
            {
               TY_CC_TextItalic = 0;
            }
         }

         TY_CC_CUR_X = 1;
         // row contains indent flag
         if ( b2 & 0x10 )
         {
            for ( x = 0 ; x < ( ( b2 & 0x0F ) << 1 ) ; x++ )
            {
               TY_CC_CUR_X++;
               *(TY_CC_ptr++) = ' ';
            }
         }
      }
      else
      // !(b2 & 0x40)
      {
         if ( TY_OSD_debug > 3 )
            mp_msg( MSGT_DEMUX, MSGL_DBG3, "<< %02x >>\n", b1 & 0x7 );
         switch (b1 & 0x07)
         {
            case 0x00:                      // attribute
				{
               if ( TY_OSD_debug > 1 )
                  mp_msg( MSGT_DEMUX, MSGL_DBG3, "<<A: %d>>\n", b2 );
               break;
				}
            case 0x01:                      // midrow or char
				{
               switch (b2 & 0x70)
               {
                  case 0x20:                // midrow attribute change
                  {
                     switch (b2 & 0x0e)
                     {
                        case 0x00:          // italics off
                        {
                           TY_CC_TextItalic = 0;
                           *(TY_CC_ptr++) = ' ';
                           break;
                        }
                        case 0x0e:          // italics on
                        {
                           ty_drawchar(' ');
                           TY_CC_TextItalic = 1;
                           break;
                        }
                        default:
                        {
                           if ( TY_OSD_debug > 1 )
                              mp_msg( MSGT_DEMUX, MSGL_DBG3, "<<D: %d>>\n",
                                 b2 & 0x0e );
                        }
                     }
                     if ( b2 & 0x01 )
                     {
                        // TextUnderline = 1;
                     }
                     else
                     {
                        // TextUnderline = 0;
                     }
                     break;
                  }
                  case 0x30:                // special character..
                  {
                     // transparent space
                     if ( ( b2 & 0x0f ) == 9 )
                     {
                        TY_CC_CUR_X++;
                        *(TY_CC_ptr++) = ' ';
                     }
                     else
                     {
                        // ty_drawchar(specialchar[ b2 & 0x0f ] );
                        ty_drawchar( ' ' );
                     }
                     break;
                  }
               }
               break;
				}

            case 0x04:                      // misc
            case 0x05:                      // misc + F
				{
               if ( TY_OSD_debug > 3 )
                  mp_msg( MSGT_DEMUX, MSGL_DBG3, "<< misc %02x >>\n", b2 );
               switch ( b2 )
               {
                  case 0x20:                // resume caption (new caption)
                  {
                     if ( TY_OSD_flags & TY_OSD_MODE &&
                        TY_CC_stat != TY_CCPOPUP )
								ty_ClearOSD( 1 );
                     TY_CC_stat = TY_CCPOPUP;
                     break;
                  }

                  case 0x21:                // backspace
                  {
                     TY_CC_CUR_X--;
                     break;
                  }

                  case 0x25:       // 2-4 row captions
                  case 0x26:
                  case 0x27:
                  {
                     if ( TY_CC_stat == TY_CCPOPUP ) ty_ClearOSD( 1 );
                     TY_CC_stat = b2 - 0x23;
                     if ( TY_CC_CUR_Y < TY_CC_stat ) TY_CC_CUR_Y = TY_CC_stat;
                     break;
                  }

                  case 0x29:                // resume direct caption
                  {
                     TY_CC_stat = TY_CCPAINTON;
                     break;
                  }

                  case 0x2A:                // text restart
                  {
                     ty_draw();
                     /* FALL */
                  }

                  case 0x2B:                // resume text display
                  {
                     TY_CC_stat = TY_CCTEXTMODE;
                     break;
                  }

                  case 0x2C:                // erase displayed memory
                  {
                     TY_CC_lastcap = 0;
                     if ( TY_OSD_flags & TY_OSD_MODE )
                     {
                        if ( TY_CC_stat > TY_CCPOPUP || TY_CC_ptr == TY_CC_buf )
                        {
                           ty_ClearOSD( 1 );
                           ty_draw();
                        }
                        else
                        {
                           ty_ClearOSD( 1 );

                           // CRW -
                           // new buffer
                           // Used to be a buffer swap here, dunno why
                        }
                     }
                     break;
                  }

                  case 0x2D:                // carriage return
                  {
                     ty_draw();
                     TY_CC_CUR_X = 1;
                     if ( TY_OSD_flags & TY_OSD_MODE )
                     {
                        if ( TY_CC_stat > TY_CCPAINTON )
                           ty_RollupBuf
                           (
                              TY_CC_CUR_Y - TY_CC_stat + 1 ,
                              TY_CC_CUR_Y - TY_CC_stat + 2,
                              TY_CC_stat - 1
                            );
                        else
                           TY_CC_CUR_Y++;
                      }
                      break;
                  }

                  case 0x2F:                // end caption + swap memory
                  {
                     ty_draw();
                     /* FALL THROUGH TO 0x2E */
                  }

                  case 0x2E:                // erase non-displayed memory
                  {
                     if ( TY_OSD_debug && TY_CC_ptr != TY_CC_buf )
                        mp_msg( MSGT_DEMUX, MSGL_DBG3, "(TY_OSD_debug) %s\n",
                           TY_CC_buf );
                     if ( TY_OSD_flags & TY_OSD_MODE ) ty_ClearOSD( 1 );

                     TY_CC_CUR_X = 1;
                     TY_CC_CUR_Y = -1;

                     TY_CC_ptr = TY_CC_buf;
                     memset( TY_CC_buf, 0, sizeof( TY_CC_buf ) );
                  }
               }
               break;
				}
            case 0x07:                      // misc (TAB)
            {
               for ( x = 0 ; x < ( b2 - 0x20 ) ; x++ )
                  TY_CC_CUR_X++;
               break;
            }
         }
      }
   }
   CC_last = data;
   return 0;
}

// ===========================================================================
// Extended Data Service Decoding and OSD Presentation
// ===========================================================================
#define XDS_BUFFER_LENGTH     ( 16 )
#define XDS_DISPLAY_FRAMES    ( 120 )
static char *ty_XDS_Display[ XDS_BUFFER_LENGTH ];
static int ty_XDSAddLine = -1;
static int ty_XDSDisplayCount = -1;


static void ty_AddXDSToDisplay( const char *format, ... )
{
   char line[ 80 ];
   int  index;
   va_list ap;

   if ( ty_XDSAddLine == -1 )
   {
      for( index = 0 ; index < XDS_BUFFER_LENGTH ; index++ )
      {
         ty_XDS_Display[ index ] = 0;
      }
      ty_XDSAddLine = 0;
   }

   va_start( ap, format );
   vsnprintf( line, 80, format, ap );
   va_end( ap );
   mp_msg( MSGT_DEMUX, MSGL_V, "XDS: %s\n", line );

   if ( ty_XDSAddLine == XDS_BUFFER_LENGTH )
   {
      mp_msg( MSGT_DEMUX, MSGL_ERR, "XDS Buffer would have been blown\n" );
   }

   if ( ty_XDS_Display[ ty_XDSAddLine ] != 0 )
   {
      free( ty_XDS_Display[ ty_XDSAddLine ] );
      ty_XDS_Display[ ty_XDSAddLine ] = 0;
   }

   ty_XDS_Display[ ty_XDSAddLine ] = malloc( strlen( line ) + 1 );
   strcpy( ty_XDS_Display[ ty_XDSAddLine ], line );
   ty_XDSAddLine++;
}


static void ty_DisplayXDSInfo(void)
{
   int index;
   int size;

   if ( ty_XDSDisplayCount == -1 )
   {
      for( index = 0 ; index < XDS_BUFFER_LENGTH ; index++ )
      {
         if ( ty_XDS_Display[ index ] != 0 )
         {
            break;
         }
      }
      if ( index != XDS_BUFFER_LENGTH )
      {
         size =  strlen( ty_XDS_Display[ index ] );

         // Right Justify the XDS Stuff
         memcpy( &( ty_OSD1.text[ 0 ][ TY_CC_MAX_X - size - 1 ] ),
            ty_XDS_Display[ index ], size );
         free( ty_XDS_Display[ index ] );
         ty_XDS_Display[ index ] = 0;
         ty_XDSDisplayCount = 0;
         tyOSDUpdate = 1;

      }
      else
      {
         // We cleaned out all the XDS stuff to be displayed
         ty_XDSAddLine = 0;
      }
   }
   else
   {
      // We displayed that piece of XDS information long enough
      // Let's move on
      ty_XDSDisplayCount++;
      if ( ty_XDSDisplayCount >= XDS_DISPLAY_FRAMES )
      {
		   memset( ty_OSD1.text[ 0 ], ' ', TY_CC_MAX_X - 1 );
		      ty_OSD1.text[ 0 ][ TY_CC_MAX_X - 1 ] = 0;
         ty_XDSDisplayCount = -1;
         tyOSDUpdate = 1;
      }
   }
}


static int  TY_XDS_mode = 0;
static int  TY_XDS_type = 0;
static int  TY_XDS_length = 0;
static char TY_XDS_checksum = 0;

// Array of [ Mode ][ Type ][ Length ]
static char TY_XDS    [ 8 ][ 25 ][ 34 ];
static char TY_XDS_new[ 8 ][ 25 ][ 34 ];

// Array of [ MPAARating|TVRating ][ NumberRatings ]
static const char * const TY_XDS_CHIP[ 2 ][ 8 ] =
{
   { "(NOT APPLICABLE)", "G", "PG", "PG-13", "R", "NC-17", "X", "(NOT RATED)" },
   { "(NOT RATED)", "TV-Y", "TV-Y7", "TV-G", "TV-PG", "TV-14", "TV-MA",
      "(NOT RATED)" }
};

static const char * const TY_XDS_modes[] =
{
  "CURRENT",                        // 01h-02h current program
  "FUTURE ",                        // 03h-04h future program
  "CHANNEL",                        // 05h-06h channel
  "MISC.  ",                        // 07h-08h miscellaneous
  "PUBLIC ",                        // 09h-0Ah public service
  "RESERV.",                        // 0Bh-0Ch reserved
  "UNDEF. ",
  "INVALID",
  "INVALID",
  "INVALID"
};

static int ty_XDSdecode( char b1, char b2 )
{
   char line[ 80 ];

   if ( b1 < 0x0F )
   {                                        // start packet
      TY_XDS_length = 0;
      TY_XDS_mode = b1 >> 1;                // every other mode is a resume
      TY_XDS_type = b2;
      TY_XDS_checksum = b1 + b2;
      return 0;
   }

   TY_XDS_checksum += b1 + b2;

   // eof (next byte is checksum)
   if ( b1 == 0x0F )
   {
      // validity check
      if ( !TY_XDS_length || TY_XDS_checksum & 0x7F )
      {
         if ( TY_OSD_debug > 3 && !TY_XDS_length )
         {
            mp_msg( MSGT_DEMUX, MSGL_DBG3,
               "%% TY_XDS CHECKSUM ERROR (ignoring)\n" );
         }
         else
         {
            TY_XDS_mode = 0;
            TY_XDS_type = 0;
            return 1;
         }
      }

      // check to see if the data has changed.
      if ( strncmp( TY_XDS[ TY_XDS_mode ][ TY_XDS_type ],
         TY_XDS_new[ TY_XDS_mode ][ TY_XDS_type ], TY_XDS_length - 1 ) )
      {
         char *TY_XDS_ptr = TY_XDS[ TY_XDS_mode ][ TY_XDS_type ];

         TY_XDS_ptr[ TY_XDS_length ] = 0;
         memcpy( TY_XDS[ TY_XDS_mode ][ TY_XDS_type ],
            TY_XDS_new[ TY_XDS_mode ][ TY_XDS_type ], TY_XDS_length );

         // nasty hack: only print time codes if seconds are 0
         if ( TY_XDS_mode == 3 && TY_XDS_type == 1 &&
            !( TY_XDS_new[ 3 ][ 1 ][ 3 ] & 0x20 ) )
			{
            return 0;
			}
         if ( TY_XDS_mode == 0 && TY_XDS_type == 2 &&
            ( TY_XDS_new[ 0 ][ 2 ][ 4 ] & 0x3f ) > 1 )
			{
            return 0;
			}

         mp_msg( MSGT_DEMUX, MSGL_DBG3, "%% %s ", TY_XDS_modes[ TY_XDS_mode ] );

         line[ 0 ] = 0;
         // printf( "XDS Code %x\n",
			//    ( TY_XDS_mode << 9 ) + TY_XDS_type + 0x100 );
         switch ( ( TY_XDS_mode << 9 ) + TY_XDS_type + 0x100 )
         {
            // cases are specified in 2 bytes hex representing mode, type.
            // TY_XDS_ptr will point to the current class buffer
            case 0x0101:                    // current
            case 0x0301:                    // future
            {
               char *mon[] =
               {
                  "0", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                  "Aug", "Sep", "Oct", "Nov", "Dec", "13", "14", "15"
               };
               ty_AddXDSToDisplay( "AIR DATE: %s %2d %d:%02d:00",
                 mon[ TY_XDS_ptr[ 3 ] & 0x0f ],
                 TY_XDS_ptr[ 2 ] & 0x1f,
                 TY_XDS_ptr[ 1 ] & 0x1f,
                 TY_XDS_ptr[ 0 ] & 0x3f
                 );

               // Program is tape delayed
               if ( TY_XDS_ptr[ 3 ] & 0x10 ) ty_AddXDSToDisplay( " TAPE" );
            }
            break;

            case 0x0102:                    // current program length
            case 0x0302:                    // future
            {
               ty_AddXDSToDisplay(
                  "DURATION: %d:%02d:%02d of %d:%02d:%02d",
                  TY_XDS_ptr[ 3 ] & 0x3f,
                  TY_XDS_ptr[ 2 ] & 0x3f,
                  TY_XDS_ptr[ 4 ] & 0x3f,
                  TY_XDS_ptr[ 1 ] & 0x3f,
                  TY_XDS_ptr[ 0 ] & 0x3f, 0);
               break;
            }

            case 0x0103:                    // current program name
            case 0x0303:                    // future
            {
               ty_AddXDSToDisplay( "TITLE: %s", TY_XDS_ptr );
               break;
            }

            case 0x0104:                    // current program type
            case 0x0304:                    // future
            {
               // for now just print out the raw data
               // requires a 127 string array to parse
               // properly and isn't worth it.
               sprintf ( line, "%sGENRE:", line );
               {
                  int x;
                  for ( x = 0 ; x < TY_XDS_length ; x++ )
                     sprintf( line, "%s %02x", line, TY_XDS_ptr[ x ] );
               }
               ty_AddXDSToDisplay( line );
               break;
            }

            case 0x0105:                    // current program rating
            case 0x0305:                    // future
            {
               sprintf( line, "%sRATING: %s", line,
                  TY_XDS_CHIP[ ( TY_XDS_ptr[ 0 ] & 0x08 ) >> 3 ]
                  [ TY_XDS_ptr[ 1 ] & 0x07 ] );
               if ( TY_XDS_ptr[ 0 ] & 0x20 )
                  sprintf( line, "%s DIALOGUE", line );
               if ( TY_XDS_ptr[ 1 ] & 0x08 )
                  sprintf( line, "%s LANGUAGE", line );
               if ( TY_XDS_ptr[ 1 ] & 0x10 )
                  sprintf( line, "%s SEXUAL", line );
               if ( TY_XDS_ptr[ 1 ] & 0x20 )
                  sprintf( line, "%s VIOLENCE", line );
               ty_AddXDSToDisplay( line );

               // raw output for verification.
               if ( TY_OSD_debug > 1 )
                  mp_msg( MSGT_DEMUX, MSGL_DBG3, " (%02x %02x)",
                     TY_XDS_ptr[ 0 ], TY_XDS_ptr[ 1 ] );
               break;
            }

            case 0x0106:                    // current program audio services
            case 0x0306:                    // future
            {
               // requires table, never actually seen it used either
               ty_AddXDSToDisplay( "AUDIO: %02x %02x", TY_XDS_ptr[ 0 ],
                  TY_XDS_ptr[ 1 ] );
               break;
            }

            case 0x0109:                    // current program aspect ratio
            case 0x0309:                    // future
            {
               // requires table, rare
               ty_AddXDSToDisplay( "ASPECT: %02x %02x",
                  TY_XDS_ptr[ 0 ], TY_XDS_ptr[ 1 ] );
               break;
            }

            case 0x0110:         // program description
            case 0x0111:
            case 0x0112:
            case 0x0113:
            case 0x0114:
            case 0x0115:
            case 0x0116:
            case 0x0117:
            {
               ty_AddXDSToDisplay( "DESCRIP: %s", TY_XDS_ptr );
               break;
            }

            case 0x0501:                    // channel network name
            {
               ty_AddXDSToDisplay( "NETWORK: %s", TY_XDS_ptr );
               break;
            }

            case 0x0502:                    // channel network call letters
            {
               ty_AddXDSToDisplay( "CALLSIGN: %s", TY_XDS_ptr );
               break;
            }

            case 0x0701:                    // misc. time of day
            {
#define TIMEZONE          ( TY_XDS[ 3 ][ 4 ][ 0 ] & 0x1f )
#define DST               ( ( TY_XDS[ 3 ][ 4 ][ 0 ] & 0x20 ) >> 5 )
               struct tm tm =
               {
                  .tm_sec = 0,                                // sec
                  .tm_min = ( TY_XDS_ptr[ 0 ] & 0x3F ),       // min
                  .tm_hour = ( TY_XDS_ptr[ 1 ] & 0x1F ),      // hour
                  .tm_mday = ( TY_XDS_ptr[ 2 ] & 0x1F ),      // day
                  .tm_mon = ( TY_XDS_ptr[ 3 ] & 0x1f ) - 1,   // month
                  .tm_year = ( TY_XDS_ptr[ 5 ] & 0x3f ) + 90, // year
                  .tm_wday = 0,                               // day of week
                  .tm_yday = 0,                               // day of year
                  .tm_isdst = 0,                              // DST
               };

               time_t time_t = mktime( &tm );
               char *timestr;

               time_t -= ( ( TIMEZONE - DST ) * 60 * 60 );
               timestr = ctime( &time_t );
               timestr[ strlen( timestr ) - 1 ] = 0;

               sprintf( line, "%sCUR.TIME: %s ", line, timestr );
               if ( TY_XDS[ 3 ][ 4 ][ 0 ] )
               {
                  sprintf( line, "%sUTC-%d", line, TIMEZONE );
                  if (DST) sprintf( line, "%s DST", line );
               }
               else
                  sprintf( line, "%sUTC", line );

               ty_AddXDSToDisplay( line );

               break;
            }

            case 0x0704:                    //misc. local time zone
            {
               sprintf( line, "%sTIMEZONE: UTC-%d",
                  line, TY_XDS_ptr[ 0 ] & 0x1f );
               if ( TY_XDS_ptr[ 0 ] & 0x20 ) sprintf( line, "%s DST", line );
               ty_AddXDSToDisplay( line );
               break;
            }

            default:
            {
               mp_msg( MSGT_DEMUX, MSGL_DBG3, "UNKNOWN CLASS %d TYPE %d",
                  ( TY_XDS_mode << 1 ) + 1, TY_XDS_type );
              if ( TY_OSD_debug > 1 )
              {
                  int x;
                  mp_msg( MSGT_DEMUX, MSGL_DBG3, "\nDUMP:\n" );
                  for ( x = 0 ; x < TY_XDS_length ; x++ )
                    mp_msg( MSGT_DEMUX, MSGL_DBG3, " %02x %c",
                       TY_XDS_ptr[ x ], TY_XDS_ptr[ x ] );
                  mp_msg( MSGT_DEMUX, MSGL_DBG3, "\n" );
               }
            }
         }
         if ( TY_OSD_debug > 1 )
            mp_msg( MSGT_DEMUX, MSGL_DBG3, " (%d)", TY_XDS_length );
      }
      TY_XDS_mode = 0;
      TY_XDS_type = 0;
   }
   else if ( TY_XDS_length < 34 )
   {
      TY_XDS_new[ TY_XDS_mode ][ TY_XDS_type ][ TY_XDS_length++ ] = b1;
      TY_XDS_new[ TY_XDS_mode ][ TY_XDS_type ][ TY_XDS_length++ ] = b2;
   }
   return 0;
}


// ===========================================================================
// Callback from Video Display Processing to put up the OSD
// ===========================================================================
void ty_processuserdata( unsigned char* buf, int len )
{
	int index;

	sub_justify = 1;

	if ( subcc_enabled )
	{
		if ( tyOSDInitialized == 0 )
		{
			for ( index = 0; index < SUB_MAX_TEXT ; index++ )
			{
				ty_OSD1.text[ index ] = malloc( TY_CC_MAX_X );
				ty_OSD2.text[ index ] = malloc( TY_CC_MAX_X );
			}
			ty_ClearOSD( 0 );
			ty_OSD1.lines = SUB_MAX_TEXT;
			ty_OSD2.lines = SUB_MAX_TEXT;
			ty_pOSD1 = &ty_OSD1;
			ty_pOSD2 = &ty_OSD2;
			tyOSDUpdate = 0;
			tyOSDInitialized = 1;
		}

		if ( buf[ 0 ] == 0x01 )
		{
			ty_CCdecode( buf[ 1 ], buf[ 2 ] );
		}
		if ( buf[ 0 ] == 0x02 )
		{
			ty_XDSdecode( buf[ 1 ], buf[ 2 ] );
		}

      ty_DisplayXDSInfo();

		if ( tyOSDUpdate )
		{
			// for ( index = 0; index < SUB_MAX_TEXT ; index++ )
			// {
         //    printf( "OSD:%d:%s\n", index, ty_OSD1.text[ index ] );
         // }
		   vo_sub = &ty_OSD1;
   		vo_osd_changed( OSDTYPE_SUBTITLE );
			tyOSDUpdate = 0;
		}
	}
}
