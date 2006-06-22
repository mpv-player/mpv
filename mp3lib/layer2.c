/*
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

/* 
 * Mpeg Layer-2 audio decoder 
 * --------------------------
 * copyright (c) 1995 by Michael Hipp, All rights reserved. See also 'README'
 *
 */

//#include "mpg123.h"
#include "l2tables.h"

static int grp_3tab[32 * 3] = { 0, };   /* used: 27 */
static int grp_5tab[128 * 3] = { 0, };  /* used: 125 */
static int grp_9tab[1024 * 3] = { 0, }; /* used: 729 */

static real muls[27][64];	/* also used by layer 1 */

static void init_layer2(void)
{
  static double mulmul[27] = {
    0.0 , -2.0/3.0 , 2.0/3.0 ,
    2.0/7.0 , 2.0/15.0 , 2.0/31.0, 2.0/63.0 , 2.0/127.0 , 2.0/255.0 ,
    2.0/511.0 , 2.0/1023.0 , 2.0/2047.0 , 2.0/4095.0 , 2.0/8191.0 ,
    2.0/16383.0 , 2.0/32767.0 , 2.0/65535.0 ,
    -4.0/5.0 , -2.0/5.0 , 2.0/5.0, 4.0/5.0 ,
    -8.0/9.0 , -4.0/9.0 , -2.0/9.0 , 2.0/9.0 , 4.0/9.0 , 8.0/9.0 };
  static int base[3][9] = {
     { 1 , 0, 2 , } ,
     { 17, 18, 0 , 19, 20 , } ,
     { 21, 1, 22, 23, 0, 24, 25, 2, 26 } };
  int i,j,k,l,len;
  real *table;
  static int tablen[3] = { 3 , 5 , 9 };
  static int *itable,*tables[3] = { grp_3tab , grp_5tab , grp_9tab };

  for(i=0;i<3;i++)
  {
    itable = tables[i];
    len = tablen[i];
    for(j=0;j<len;j++)
      for(k=0;k<len;k++)
        for(l=0;l<len;l++)
        {
          *itable++ = base[i][l];
          *itable++ = base[i][k];
          *itable++ = base[i][j];
        }
  }

  for(k=0;k<27;k++)
  {
    double m=mulmul[k];
    table = muls[k];
    if(_has_mmx) 
    {
        for(j=3,i=0;i<63;i++,j--)
	  *table++ = 16384 * m * pow(2.0,(double) j / 3.0);
    }
    else
    for(j=3,i=0;i<63;i++,j--)
    {
      *table++ = m * pow(2.0,(double) j / 3.0);
    }
    *table++ = 0.0;
  }
}


static void II_step_one(unsigned int *bit_alloc,int *scale,struct frame *fr)
{
    int stereo = fr->stereo-1;
    int sblimit = fr->II_sblimit;
    int jsbound = fr->jsbound;
    int sblimit2 = fr->II_sblimit<<stereo;
    struct al_table *alloc1 = fr->alloc;
    int i;
    static unsigned int scfsi_buf[64];
    unsigned int *scfsi,*bita;
    int sc,step;

    bita = bit_alloc;
    if(stereo)
    {
      for (i=jsbound;i>0;i--,alloc1+=(1<<step))
      {
        *bita++ = (char) getbits(step=alloc1->bits);
        *bita++ = (char) getbits(step);
      }
      for (i=sblimit-jsbound;i>0;i--,alloc1+=(1<<step))
      {
        bita[0] = (char) getbits(step=alloc1->bits);
        bita[1] = bita[0];
        bita+=2;
      }
      bita = bit_alloc;
      scfsi=scfsi_buf;
      for (i=sblimit2;i>0;i--)
        if (*bita++)
          *scfsi++ = (char) getbits_fast(2);
    }
    else /* mono */
    {
      for (i=sblimit;i>0;i--,alloc1+=(1<<step))
        *bita++ = (char) getbits(step=alloc1->bits);
      bita = bit_alloc;
      scfsi=scfsi_buf;
      for (i=sblimit;i>0;i--)
        if (*bita++)
          *scfsi++ = (char) getbits_fast(2);
    }

    bita = bit_alloc;
    scfsi=scfsi_buf;
    for (i=sblimit2;i>0;i--) 
      if (*bita++)
        switch (*scfsi++) 
        {
          case 0: 
                *scale++ = getbits_fast(6);
                *scale++ = getbits_fast(6);
                *scale++ = getbits_fast(6);
                break;
          case 1 : 
                *scale++ = sc = getbits_fast(6);
                *scale++ = sc;
                *scale++ = getbits_fast(6);
                break;
          case 2: 
                *scale++ = sc = getbits_fast(6);
                *scale++ = sc;
                *scale++ = sc;
                break;
          default:              /* case 3 */
                *scale++ = getbits_fast(6);
                *scale++ = sc = getbits_fast(6);
                *scale++ = sc;
                break;
        }

}

static void II_step_two(unsigned int *bit_alloc,real fraction[2][4][SBLIMIT],int *scale,struct frame *fr,int x1)
{
    int i,j,k,ba;
    int stereo = fr->stereo;
    int sblimit = fr->II_sblimit;
    int jsbound = fr->jsbound;
    struct al_table *alloc2,*alloc1 = fr->alloc;
    unsigned int *bita=bit_alloc;
    int d1,step;

    for (i=0;i<jsbound;i++,alloc1+=(1<<step))
    {
      step = alloc1->bits;
      for (j=0;j<stereo;j++)
      {
        if ( (ba=*bita++) ) 
        {
          k=(alloc2 = alloc1+ba)->bits;
          if( (d1=alloc2->d) < 0) 
          {
            real cm=muls[k][scale[x1]];
            fraction[j][0][i] = ((real) ((int)getbits(k) + d1)) * cm;
            fraction[j][1][i] = ((real) ((int)getbits(k) + d1)) * cm;
            fraction[j][2][i] = ((real) ((int)getbits(k) + d1)) * cm;
          }        
          else 
          {
            static int *table[] = { 0,0,0,grp_3tab,0,grp_5tab,0,0,0,grp_9tab };
            unsigned int idx,*tab,m=scale[x1];
            idx = (unsigned int) getbits(k);
            tab = (unsigned int *) (table[d1] + idx + idx + idx);
            fraction[j][0][i] = muls[*tab++][m];
            fraction[j][1][i] = muls[*tab++][m];
            fraction[j][2][i] = muls[*tab][m];  
          }
          scale+=3;
        }
        else
          fraction[j][0][i] = fraction[j][1][i] = fraction[j][2][i] = 0.0;
      }
    }

    for (i=jsbound;i<sblimit;i++,alloc1+=(1<<step))
    {
      step = alloc1->bits;
      bita++;	/* channel 1 and channel 2 bitalloc are the same */
      if ( (ba=*bita++) )
      {
        k=(alloc2 = alloc1+ba)->bits;
        if( (d1=alloc2->d) < 0)
        {
          real cm;
          cm=muls[k][scale[x1+3]];
          fraction[1][0][i] = (fraction[0][0][i] = (real) ((int)getbits(k) + d1) ) * cm;
          fraction[1][1][i] = (fraction[0][1][i] = (real) ((int)getbits(k) + d1) ) * cm;
          fraction[1][2][i] = (fraction[0][2][i] = (real) ((int)getbits(k) + d1) ) * cm;
          cm=muls[k][scale[x1]];
          fraction[0][0][i] *= cm; fraction[0][1][i] *= cm; fraction[0][2][i] *= cm;
        }
        else
        {
          static int *table[] = { 0,0,0,grp_3tab,0,grp_5tab,0,0,0,grp_9tab };
          unsigned int idx,*tab,m1,m2;
          m1 = scale[x1]; m2 = scale[x1+3];
          idx = (unsigned int) getbits(k);
          tab = (unsigned int *) (table[d1] + idx + idx + idx);
          fraction[0][0][i] = muls[*tab][m1]; fraction[1][0][i] = muls[*tab++][m2];
          fraction[0][1][i] = muls[*tab][m1]; fraction[1][1][i] = muls[*tab++][m2];
          fraction[0][2][i] = muls[*tab][m1]; fraction[1][2][i] = muls[*tab][m2];
        }
        scale+=6;
      }
      else {
        fraction[0][0][i] = fraction[0][1][i] = fraction[0][2][i] =
        fraction[1][0][i] = fraction[1][1][i] = fraction[1][2][i] = 0.0;
      }
/* 
   should we use individual scalefac for channel 2 or
   is the current way the right one , where we just copy channel 1 to
   channel 2 ?? 
   The current 'strange' thing is, that we throw away the scalefac
   values for the second channel ...!!
-> changed .. now we use the scalefac values of channel one !! 
*/
    }

    if(sblimit > (fr->down_sample_sblimit) )
      sblimit = fr->down_sample_sblimit;

    for(i=sblimit;i<SBLIMIT;i++)
      for (j=0;j<stereo;j++)
        fraction[j][0][i] = fraction[j][1][i] = fraction[j][2][i] = 0.0;

}

static void II_select_table(struct frame *fr)
{
  static int translate[9][2][16] =
   { { { 0,2,2,2,2,2,2,0,0,0,1,1,1,1,1,0 } ,   /*44.1 stereo*/
       { 0,2,2,0,0,0,1,1,1,1,1,1,1,1,1,0 } } , /*44.1 mono*/
     { { 0,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0 } ,   /*48 stereo*/
       { 0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0 } } , /*48 mono*/
     { { 0,3,3,3,3,3,3,0,0,0,1,1,1,1,1,0 } ,   /*32 stereo*/
       { 0,3,3,0,0,0,1,1,1,1,1,1,1,1,1,0 } } , /*32 mono*/
     { { 2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,0 } ,   /*22.05 stereo*/
       { 2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,0 } } , /*22.05 mono*/
     { { 2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,0 } ,   /*24 stereo*/
       { 2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,0 } } , /*24 mono*/
     { { 2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,0 } ,   /*16 stereo*/
       { 2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,0 } } , /*16 mono*/
     { { 2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,0 } ,   /*11.025 stereo*/
       { 2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,0 } } , /*11.025 mono*/
     { { 2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,0 } ,   /*12 stereo*/
       { 2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,0 } } , /*12 mono*/
     { { 2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,0 } ,   /*8 stereo*/
       { 2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,0 } }   /*8 mono*/
/*       0  48  64  96 128 192 256 384 */
/*        32  56  80 112 160 224 320  XX*/
       };

  int table,sblim;
  static struct al_table *tables[5] =
       { alloc_0, alloc_1, alloc_2, alloc_3 , alloc_4 };
  static int sblims[5] = { 27 , 30 , 8, 12 , 30 };

  if(fr->lsf)
    table = 4;
  else
    table = translate[fr->sampling_frequency][2-fr->stereo][fr->bitrate_index];
  sblim = sblims[table];

  fr->alloc      = tables[table];
  fr->II_sblimit = sblim;
}


static int do_layer2(struct frame *fr,int outmode)
{
  int clip=0;
  int i,j;
  int stereo = fr->stereo;
  real fraction[2][4][SBLIMIT]; /* pick_table clears unused subbands */
  unsigned int bit_alloc[64];
  int scale[192];
  int single = fr->single;

  II_select_table(fr);
  fr->jsbound = (fr->mode == MPG_MD_JOINT_STEREO) ?
     (fr->mode_ext<<2)+4 : fr->II_sblimit;

  if(stereo == 1 || single == 3)
    single = 0;

  II_step_one(bit_alloc, scale, fr);

  for (i=0;i<SCALE_BLOCK;i++) 
  {
    II_step_two(bit_alloc,fraction,scale,fr,i>>2);
    for (j=0;j<3;j++) 
    {
      if(single >= 0)
      {
        clip += (fr->synth_mono) (fraction[single][j],pcm_sample,&pcm_point);
      }
      else {
          int p1 = pcm_point;
          clip += (fr->synth) (fraction[0][j],0,pcm_sample,&p1);
          clip += (fr->synth) (fraction[1][j],1,pcm_sample,&pcm_point);
      }

//      if(pcm_point >= audiobufsize) audio_flush(outmode,ai);
    }
  }

  return clip;
}
