#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp_msg.h"
#include "help_mp.h"
#include "bitmap.h"
#ifdef USE_LIBAVCODEC_SO
#include <ffmpeg/avcodec.h>
#else
#include "libavcodec/avcodec.h"
#endif
#include "libvo/fastmemcpy.h"

static int pngRead( unsigned char * fname,txSample * bf )
{
 int             decode_ok;
 void           *data;
 int             len;
 AVCodecContext *avctx;
 AVFrame        *frame;
 
 FILE *fp=fopen( fname,"rb" );
 if ( !fp ) 
  {
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png] file read error ( %s )\n",fname );
   return 1;
  }

 fseek(fp, 0, SEEK_END);
 len = ftell(fp);
 if (len > 50 * 1024 * 1024) return 2;
 data = malloc(len + FF_INPUT_BUFFER_PADDING_SIZE);
 fseek(fp, 0, SEEK_SET);
 fread(data, len, 1, fp);
 fclose(fp);
 avctx = avcodec_alloc_context();
 frame = avcodec_alloc_frame();
 avcodec_open(avctx, &png_decoder);
 avcodec_decode_video(avctx, frame, &decode_ok, data, len);
 memset(bf, 0, sizeof(*bf));
 switch (avctx->pix_fmt) {
   case PIX_FMT_GRAY8:    bf->BPP =  8; break;
   case PIX_FMT_GRAY16BE: bf->BPP = 16; break;
   case PIX_FMT_RGB24:    bf->BPP = 24; break;
   case PIX_FMT_RGB32:    bf->BPP = 32; break;
   default:               bf->BPP =  0; break;
 }
 if (decode_ok && bf->BPP) {
   int bpl;
   bf->Width = avctx->width; bf->Height = avctx->height;
   bpl = bf->Width * (bf->BPP / 8);
   bf->ImageSize = bpl * bf->Height;
   bf->Image = malloc(bf->ImageSize);
   memcpy_pic(bf->Image, frame->data[0], bpl, bf->Height, bpl, frame->linesize[0]);
 }
 avcodec_close(avctx);
 av_freep(&frame);
 av_freep(&avctx);

 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png] filename: %s.\n",fname );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png]  size: %dx%d bits: %d\n",bf->Width,bf->Height,bf->BPP );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[png]  imagesize: %lu\n",bf->ImageSize );
 return !(decode_ok && bf->BPP);
}

static int conv24to32( txSample * bf )
{
 unsigned char * tmpImage;
 int             i,c;

 if ( bf->BPP == 24 )
  {
   tmpImage=bf->Image;
   bf->ImageSize=bf->Width * bf->Height * 4;
   bf->BPP=32;
   if ( ( bf->Image=malloc( bf->ImageSize ) ) == NULL )
    {
     mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[bitmap] not enough memory for image\n" );
     return 1;
    }
   memset( bf->Image,0,bf->ImageSize );
   for ( c=0,i=0; c < bf->ImageSize; )
    {
     bf->Image[c++]=tmpImage[i++];	//red
     bf->Image[c++]=tmpImage[i++];	//green
     bf->Image[c++]=tmpImage[i++]; c++;	//blue
    }
   free( tmpImage );
  }
 return 0;
}

static void bgr2rgb( txSample * bf )
{
 unsigned char c;
 int           i;

 for ( i=0;i < (int)bf->ImageSize;i+=4 )
  {
   c=bf->Image[i];
   bf->Image[i]=bf->Image[i+2];
   bf->Image[i+2]=c;
  }
}

static void Normalize( txSample * bf )
{
 int           i;
#ifndef WORDS_BIGENDIAN 
 for ( i=0;i < (int)bf->ImageSize;i+=4 ) bf->Image[i+3]=0;
#else
 for ( i=0;i < (int)bf->ImageSize;i+=4 ) bf->Image[i]=0; 
#endif
}

static unsigned char tmp[512];

static unsigned char * fExist( unsigned char * fname )
{
 FILE          * fl;
 unsigned char   ext[][6] = { ".png\0",".PNG\0" };
 int             i;

 fl=fopen( fname,"rb" );
 if ( fl != NULL )
  {
   fclose( fl );
   return fname;
  }
 for ( i=0;i<2;i++ )
  {
   snprintf( tmp,511,"%s%s",fname,ext[i] );
   fl=fopen( tmp,"rb" );
   if ( fl != NULL )
    {
     fclose( fl );
     return tmp;
    }
  }
 return NULL;
}

int bpRead( char * fname, txSample * bf )
{
 fname=fExist( fname );
 if ( fname == NULL ) return -2;
 if ( pngRead( fname,bf ) ) 
  {
   mp_dbg( MSGT_GPLAYER,MSGL_FATAL,"[bitmap] unknown file type ( %s )\n",fname );
   return -5;
  }
 if ( bf->BPP < 24 )
  {
   mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[bitmap] Sorry, only 24 and 32 bpp bitmaps are supported.\n" );
   return -1;
  }
 if ( conv24to32( bf ) ) return -8;
#ifdef WORDS_BIGENDIAN
 swab(bf->Image, bf->Image, bf->ImageSize);
#endif
 bgr2rgb( bf );
 Normalize( bf );
 return 0;
}

void Convert32to1( txSample * in,txSample * out,int adaptivlimit )
{
 out->Width=in->Width;
 out->Height=in->Height;
 out->BPP=1;
 out->ImageSize=(out->Width * out->Height + 7) / 8;
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[c32to1] imagesize: %d\n",out->ImageSize );
 out->Image=calloc( 1,out->ImageSize );
 if ( out->Image == NULL ) mp_msg( MSGT_GPLAYER,MSGL_WARN,MSGTR_NotEnoughMemoryC32To1 );
 {
  int i,b,c=0; unsigned int * buf = NULL; unsigned char tmp = 0; int nothaveshape = 1;
  buf=(unsigned int *)in->Image;
  for ( b=0,i=0;i < (int)(out->Width * out->Height);i++ )
   {
    if ( (int)buf[i] != adaptivlimit ) tmp=( tmp >> 1 )|128;
     else { tmp=tmp >> 1; buf[i]=nothaveshape=0; }
    if ( b++ == 7 ) { out->Image[c++]=tmp; tmp=b=0; }
   }
  if ( b ) out->Image[c]=tmp;
  if ( nothaveshape ) { free( out->Image ); out->Image=NULL; }
 }
}

static void Convert1to32( txSample * in,txSample * out )
{
 if ( in->Image == NULL ) return;
 out->Width=in->Width;
 out->Height=in->Height;
 out->BPP=32;
 out->ImageSize=out->Width * out->Height * 4;
 out->Image=calloc( 1,out->ImageSize );
 mp_dbg( MSGT_GPLAYER,MSGL_DBG2,"[c1to32] imagesize: %d\n",out->ImageSize );
 if ( out->Image == NULL ) mp_msg( MSGT_GPLAYER,MSGL_WARN,MSGTR_NotEnoughMemoryC1To32 );
 {
  int i,b,c=0; unsigned int * buf = NULL; unsigned char tmp = 0;
  buf=(unsigned int *)out->Image;
  for ( c=0,i=0;i < (int)(in->Width * in->Height / 8);i++ )
   {
    tmp=in->Image[i];
    for ( b=0;b<8;b++ )
     {
      buf[c]=0;
      if ( tmp&0x1 ) buf[c]=0xffffffff;
      c++; tmp=tmp>>1;
     }
   }
 }
}
