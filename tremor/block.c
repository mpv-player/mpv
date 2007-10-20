/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: PCM data vector blocking, windowing and dis/reassembly

 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ogg.h"
#include "ivorbiscodec.h"
#include "codec_internal.h"

#include "window.h"
#include "registry.h"
#include "misc.h"
#include "block.h"

static int ilog(unsigned int v){
  int ret=0;
  if(v)--v;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

/* pcm accumulator examples (not exhaustive):

 <-------------- lW ---------------->
                   <--------------- W ---------------->
:            .....|.....       _______________         |
:        .'''     |     '''_---      |       |\        |
:.....'''         |_____--- '''......|       | \_______|
:.................|__________________|_______|__|______|
                  |<------ Sl ------>|      > Sr <     |endW
                  |beginSl           |endSl  |  |endSr   
                  |beginW            |endlW  |beginSr


                      |< lW >|       
                   <--------------- W ---------------->
                  |   |  ..  ______________            |
                  |   | '  `/        |     ---_        |
                  |___.'___/`.       |         ---_____| 
                  |_______|__|_______|_________________|
                  |      >|Sl|<      |<------ Sr ----->|endW
                  |       |  |endSl  |beginSr          |endSr
                  |beginW |  |endlW                     
                  mult[0] |beginSl                     mult[n]

 <-------------- lW ----------------->
                          |<--W-->|                               
:            ..............  ___  |   |                    
:        .'''             |`/   \ |   |                       
:.....'''                 |/`....\|...|                    
:.........................|___|___|___|                  
                          |Sl |Sr |endW    
                          |   |   |endSr
                          |   |beginSr
                          |   |endSl
			  |beginSl
			  |beginW
*/

/* block abstraction setup *********************************************/

#ifndef WORD_ALIGN
#define WORD_ALIGN 8
#endif

int vorbis_block_init(vorbis_dsp_state *v, vorbis_block *vb){
  memset(vb,0,sizeof(*vb));
  vb->vd=v;
  vb->localalloc=0;
  vb->localstore=NULL;
  
  return(0);
}

void *_vorbis_block_alloc(vorbis_block *vb,long bytes){
  bytes=(bytes+(WORD_ALIGN-1)) & ~(WORD_ALIGN-1);
  if(bytes+vb->localtop>vb->localalloc){
    /* can't just _ogg_realloc... there are outstanding pointers */
    if(vb->localstore){
      struct alloc_chain *link=(struct alloc_chain *)_ogg_malloc(sizeof(*link));
      vb->totaluse+=vb->localtop;
      link->next=vb->reap;
      link->ptr=vb->localstore;
      vb->reap=link;
    }
    /* highly conservative */
    vb->localalloc=bytes;
    vb->localstore=_ogg_malloc(vb->localalloc);
    vb->localtop=0;
  }
  {
    void *ret=(void *)(((char *)vb->localstore)+vb->localtop);
    vb->localtop+=bytes;
    return ret;
  }
}

/* reap the chain, pull the ripcord */
void _vorbis_block_ripcord(vorbis_block *vb){
  /* reap the chain */
  struct alloc_chain *reap=vb->reap;
  while(reap){
    struct alloc_chain *next=reap->next;
    _ogg_free(reap->ptr);
    memset(reap,0,sizeof(*reap));
    _ogg_free(reap);
    reap=next;
  }
  /* consolidate storage */
  if(vb->totaluse){
    vb->localstore=_ogg_realloc(vb->localstore,vb->totaluse+vb->localalloc);
    vb->localalloc+=vb->totaluse;
    vb->totaluse=0;
  }

  /* pull the ripcord */
  vb->localtop=0;
  vb->reap=NULL;
}

int vorbis_block_clear(vorbis_block *vb){
  _vorbis_block_ripcord(vb);
  if(vb->localstore)_ogg_free(vb->localstore);

  memset(vb,0,sizeof(*vb));
  return(0);
}

int vorbis_synthesis_init(vorbis_dsp_state *v,vorbis_info *vi){
  int i;
  codec_setup_info *ci=(codec_setup_info *)vi->codec_setup;
  backend_lookup_state *b=NULL;

  memset(v,0,sizeof(*v));
  b=(backend_lookup_state *)(v->backend_state=_ogg_calloc(1,sizeof(*b)));

  v->vi=vi;
  b->modebits=ilog(ci->modes);

  /* Vorbis I uses only window type 0 */
  b->window[0]=_vorbis_window(0,ci->blocksizes[0]/2);
  b->window[1]=_vorbis_window(0,ci->blocksizes[1]/2);

  /* finish the codebooks */
  if(!ci->fullbooks){
    ci->fullbooks=(codebook *)_ogg_calloc(ci->books,sizeof(*ci->fullbooks));
    for(i=0;i<ci->books;i++){
      vorbis_book_init_decode(ci->fullbooks+i,ci->book_param[i]);
      /* decode codebooks are now standalone after init */
      vorbis_staticbook_destroy(ci->book_param[i]);
      ci->book_param[i]=NULL;
    }
  }

  v->pcm_storage=ci->blocksizes[1];
  v->pcm=(ogg_int32_t **)_ogg_malloc(vi->channels*sizeof(*v->pcm));
  v->pcmret=(ogg_int32_t **)_ogg_malloc(vi->channels*sizeof(*v->pcmret));
  for(i=0;i<vi->channels;i++)
    v->pcm[i]=(ogg_int32_t *)_ogg_calloc(v->pcm_storage,sizeof(*v->pcm[i]));

  /* all 1 (large block) or 0 (small block) */
  /* explicitly set for the sake of clarity */
  v->lW=0; /* previous window size */
  v->W=0;  /* current window size */

  /* all vector indexes */
  v->centerW=ci->blocksizes[1]/2;

  v->pcm_current=v->centerW;

  /* initialize all the mapping/backend lookups */
  b->mode=(vorbis_look_mapping **)_ogg_calloc(ci->modes,sizeof(*b->mode));
  for(i=0;i<ci->modes;i++){
    int mapnum=ci->mode_param[i]->mapping;
    int maptype=ci->map_type[mapnum];
    b->mode[i]=_mapping_P[maptype]->look(v,ci->mode_param[i],
					 ci->map_param[mapnum]);
  }

  v->pcm_returned=-1;
  v->granulepos=-1;
  v->sequence=-1;

  return(0);
}

void vorbis_dsp_clear(vorbis_dsp_state *v){
  int i;
  if(v){
    vorbis_info *vi=v->vi;
    codec_setup_info *ci=(codec_setup_info *)(vi?vi->codec_setup:NULL);
    backend_lookup_state *b=(backend_lookup_state *)v->backend_state;

    if(v->pcm){
      for(i=0;i<vi->channels;i++)
	if(v->pcm[i])_ogg_free(v->pcm[i]);
      _ogg_free(v->pcm);
      if(v->pcmret)_ogg_free(v->pcmret);
    }

    /* free mode lookups; these are actually vorbis_look_mapping structs */
    if(ci){
      for(i=0;i<ci->modes;i++){
	int mapnum=ci->mode_param[i]->mapping;
	int maptype=ci->map_type[mapnum];
	if(b && b->mode)_mapping_P[maptype]->free_look(b->mode[i]);
      }
    }

    if(b){
      if(b->mode)_ogg_free(b->mode);    
      _ogg_free(b);
    }
    
    memset(v,0,sizeof(*v));
  }
}

/* Unlike in analysis, the window is only partially applied for each
   block.  The time domain envelope is not yet handled at the point of
   calling (as it relies on the previous block). */

int vorbis_synthesis_blockin(vorbis_dsp_state *v,vorbis_block *vb){
  vorbis_info *vi=v->vi;
  codec_setup_info *ci=(codec_setup_info *)vi->codec_setup;
  int i,j;

  if(v->pcm_current>v->pcm_returned  && v->pcm_returned!=-1)return(OV_EINVAL);

  v->lW=v->W;
  v->W=vb->W;
  v->nW=-1;

  if(v->sequence+1 != vb->sequence)v->granulepos=-1; /* out of sequence;
                                                     lose count */

  v->sequence=vb->sequence;
  
  {
    int n=ci->blocksizes[v->W]/2;
    int n0=ci->blocksizes[0]/2;
    int n1=ci->blocksizes[1]/2;
    
    int thisCenter;
    int prevCenter;
    
    if(v->centerW){
      thisCenter=n1;
      prevCenter=0;
    }else{
      thisCenter=0;
      prevCenter=n1;
    }
    
    /* v->pcm is now used like a two-stage double buffer.  We don't want
       to have to constantly shift *or* adjust memory usage.  Don't
       accept a new block until the old is shifted out */
    
    /* overlap/add PCM */
    
    for(j=0;j<vi->channels;j++){
      /* the overlap/add section */
      if(v->lW){
	if(v->W){
	  /* large/large */
	  ogg_int32_t *pcm=v->pcm[j]+prevCenter;
	  ogg_int32_t *p=vb->pcm[j];
	  for(i=0;i<n1;i++)
	    pcm[i]+=p[i];
	}else{
	  /* large/small */
	  ogg_int32_t *pcm=v->pcm[j]+prevCenter+n1/2-n0/2;
	  ogg_int32_t *p=vb->pcm[j];
	  for(i=0;i<n0;i++)
	    pcm[i]+=p[i];
	}
      }else{
	if(v->W){
	  /* small/large */
	  ogg_int32_t *pcm=v->pcm[j]+prevCenter;
	  ogg_int32_t *p=vb->pcm[j]+n1/2-n0/2;
	  for(i=0;i<n0;i++)
	    pcm[i]+=p[i];
	  for(;i<n1/2+n0/2;i++)
	    pcm[i]=p[i];
	}else{
	  /* small/small */
	  ogg_int32_t *pcm=v->pcm[j]+prevCenter;
	  ogg_int32_t *p=vb->pcm[j];
	  for(i=0;i<n0;i++)
	    pcm[i]+=p[i];
	}
      }
      
      /* the copy section */
      {
	ogg_int32_t *pcm=v->pcm[j]+thisCenter;
	ogg_int32_t *p=vb->pcm[j]+n;
	for(i=0;i<n;i++)
	  pcm[i]=p[i];
      }
    }
    
    if(v->centerW)
      v->centerW=0;
    else
      v->centerW=n1;
    
    /* deal with initial packet state; we do this using the explicit
       pcm_returned==-1 flag otherwise we're sensitive to first block
       being short or long */

    if(v->pcm_returned==-1){
      v->pcm_returned=thisCenter;
      v->pcm_current=thisCenter;
    }else{
      v->pcm_returned=prevCenter;
      v->pcm_current=prevCenter+
	ci->blocksizes[v->lW]/4+
	ci->blocksizes[v->W]/4;
    }

    /* track the frame number... This is for convenience, but also
       making sure our last packet doesn't end with added padding.  If
       the last packet is partial, the number of samples we'll have to
       return will be past the vb->granulepos.
       
       This is not foolproof!  It will be confused if we begin
       decoding at the last page after a seek or hole.  In that case,
       we don't have a starting point to judge where the last frame
       is.  For this reason, vorbisfile will always try to make sure
       it reads the last two marked pages in proper sequence */

    if(v->granulepos==-1)
      if(vb->granulepos==-1){
	v->granulepos=0;
      }else{
	v->granulepos=vb->granulepos;
      }
    else{
      v->granulepos+=ci->blocksizes[v->lW]/4+ci->blocksizes[v->W]/4;
      if(vb->granulepos!=-1 && v->granulepos!=vb->granulepos){

	if(v->granulepos>vb->granulepos){
	  long extra=v->granulepos-vb->granulepos;

	  if(vb->eofflag){
	    /* partial last frame.  Strip the extra samples off */
	    v->pcm_current-=extra;
	  }else if(vb->sequence == 1){
	    /* ^^^ argh, this can be 1 from seeking! */


	    /* partial first frame.  Discard extra leading samples */
	    v->pcm_returned+=extra;
	    if(v->pcm_returned>v->pcm_current)
	      v->pcm_returned=v->pcm_current;
	    
	  }
	  
	}/* else{ Shouldn't happen *unless* the bitstream is out of
	    spec.  Either way, believe the bitstream } */
	v->granulepos=vb->granulepos;
      }
    }
    
    /* Update, cleanup */
    
    if(vb->eofflag)v->eofflag=1;
  }
  
  return(0);
}

/* pcm==NULL indicates we just want the pending samples, no more */
int vorbis_synthesis_pcmout(vorbis_dsp_state *v,ogg_int32_t ***pcm){
  vorbis_info *vi=v->vi;
  if(v->pcm_returned>-1 && v->pcm_returned<v->pcm_current){
    if(pcm){
      int i;
      for(i=0;i<vi->channels;i++)
	v->pcmret[i]=v->pcm[i]+v->pcm_returned;
      *pcm=v->pcmret;
    }
    return(v->pcm_current-v->pcm_returned);
  }
  return(0);
}

int vorbis_synthesis_read(vorbis_dsp_state *v,int bytes){
  if(bytes && v->pcm_returned+bytes>v->pcm_current)return(OV_EINVAL);
  v->pcm_returned+=bytes;
  return(0);
}

