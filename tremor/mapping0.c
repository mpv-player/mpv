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

 function: channel mapping 0 implementation

 ********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "os.h"
#include "ivorbiscodec.h"
#include "mdct.h"
#include "codec_internal.h"
#include "codebook.h"
#include "window.h"
#include "registry.h"
#include "misc.h"

/* simplistic, wasteful way of doing this (unique lookup for each
   mode/submapping); there should be a central repository for
   identical lookups.  That will require minor work, so I'm putting it
   off as low priority.

   Why a lookup for each backend in a given mode?  Because the
   blocksize is set by the mode, and low backend lookups may require
   parameters from other areas of the mode/mapping */

typedef struct {
  vorbis_info_mode *mode;
  vorbis_info_mapping0 *map;

  vorbis_look_floor **floor_look;

  vorbis_look_residue **residue_look;

  vorbis_func_floor **floor_func;
  vorbis_func_residue **residue_func;

  int ch;
  long lastframe; /* if a different mode is called, we need to
		     invalidate decay */
} vorbis_look_mapping0;

static void mapping0_free_info(vorbis_info_mapping *i){
  vorbis_info_mapping0 *info=(vorbis_info_mapping0 *)i;
  if(info){
    memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}

static void mapping0_free_look(vorbis_look_mapping *look){
  int i;
  vorbis_look_mapping0 *l=(vorbis_look_mapping0 *)look;
  if(l){

    for(i=0;i<l->map->submaps;i++){
      l->floor_func[i]->free_look(l->floor_look[i]);
      l->residue_func[i]->free_look(l->residue_look[i]);
    }

    _ogg_free(l->floor_func);
    _ogg_free(l->residue_func);
    _ogg_free(l->floor_look);
    _ogg_free(l->residue_look);
    memset(l,0,sizeof(*l));
    _ogg_free(l);
  }
}

static vorbis_look_mapping *mapping0_look(vorbis_dsp_state *vd,vorbis_info_mode *vm,
			  vorbis_info_mapping *m){
  int i;
  vorbis_info          *vi=vd->vi;
  codec_setup_info     *ci=(codec_setup_info *)vi->codec_setup;
  vorbis_look_mapping0 *look=(vorbis_look_mapping0 *)_ogg_calloc(1,sizeof(*look));
  vorbis_info_mapping0 *info=look->map=(vorbis_info_mapping0 *)m;
  look->mode=vm;

  look->floor_look=(vorbis_look_floor **)_ogg_calloc(info->submaps,sizeof(*look->floor_look));

  look->residue_look=(vorbis_look_residue **)_ogg_calloc(info->submaps,sizeof(*look->residue_look));

  look->floor_func=(vorbis_func_floor **)_ogg_calloc(info->submaps,sizeof(*look->floor_func));
  look->residue_func=(vorbis_func_residue **)_ogg_calloc(info->submaps,sizeof(*look->residue_func));

  for(i=0;i<info->submaps;i++){
    int floornum=info->floorsubmap[i];
    int resnum=info->residuesubmap[i];

    look->floor_func[i]=_floor_P[ci->floor_type[floornum]];
    look->floor_look[i]=look->floor_func[i]->
      look(vd,vm,ci->floor_param[floornum]);
    look->residue_func[i]=_residue_P[ci->residue_type[resnum]];
    look->residue_look[i]=look->residue_func[i]->
      look(vd,vm,ci->residue_param[resnum]);

  }

  look->ch=vi->channels;

  return(look);
}

static int ilog(unsigned int v){
  int ret=0;
  if(v)--v;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

/* also responsible for range checking */
static vorbis_info_mapping *mapping0_unpack(vorbis_info *vi,oggpack_buffer *opb){
  int i;
  vorbis_info_mapping0 *info=(vorbis_info_mapping0 *)_ogg_calloc(1,sizeof(*info));
  codec_setup_info     *ci=(codec_setup_info *)vi->codec_setup;
  memset(info,0,sizeof(*info));

  if(oggpack_read(opb,1))
    info->submaps=oggpack_read(opb,4)+1;
  else
    info->submaps=1;

  if(oggpack_read(opb,1)){
    info->coupling_steps=oggpack_read(opb,8)+1;

    for(i=0;i<info->coupling_steps;i++){
      int testM=info->coupling_mag[i]=oggpack_read(opb,ilog(vi->channels));
      int testA=info->coupling_ang[i]=oggpack_read(opb,ilog(vi->channels));

      if(testM<0 ||
	 testA<0 ||
	 testM==testA ||
	 testM>=vi->channels ||
	 testA>=vi->channels) goto err_out;
    }

  }

  if(oggpack_read(opb,2)>0)goto err_out; /* 2,3:reserved */

  if(info->submaps>1){
    for(i=0;i<vi->channels;i++){
      info->chmuxlist[i]=oggpack_read(opb,4);
      if(info->chmuxlist[i]>=info->submaps)goto err_out;
    }
  }
  for(i=0;i<info->submaps;i++){
    int temp=oggpack_read(opb,8);
    if(temp>=ci->times)goto err_out;
    info->floorsubmap[i]=oggpack_read(opb,8);
    if(info->floorsubmap[i]>=ci->floors)goto err_out;
    info->residuesubmap[i]=oggpack_read(opb,8);
    if(info->residuesubmap[i]>=ci->residues)goto err_out;
  }

  return info;

 err_out:
  mapping0_free_info(info);
  return(NULL);
}

static int seq=0;
static int mapping0_inverse(vorbis_block *vb,vorbis_look_mapping *l){
  vorbis_dsp_state     *vd=vb->vd;
  vorbis_info          *vi=vd->vi;
  codec_setup_info     *ci=(codec_setup_info *)vi->codec_setup;
  backend_lookup_state *b=(backend_lookup_state *)vd->backend_state;
  vorbis_look_mapping0 *look=(vorbis_look_mapping0 *)l;
  vorbis_info_mapping0 *info=look->map;

  int                   i,j;
  long                  n=vb->pcmend=ci->blocksizes[vb->W];

  ogg_int32_t **pcmbundle=(ogg_int32_t **)alloca(sizeof(*pcmbundle)*vi->channels);
  int    *zerobundle=(int *)alloca(sizeof(*zerobundle)*vi->channels);

  int   *nonzero  =(int *)alloca(sizeof(*nonzero)*vi->channels);
  void **floormemo=(void **)alloca(sizeof(*floormemo)*vi->channels);

  /* time domain information decode (note that applying the
     information would have to happen later; we'll probably add a
     function entry to the harness for that later */
  /* NOT IMPLEMENTED */

  /* recover the spectral envelope; store it in the PCM vector for now */
  for(i=0;i<vi->channels;i++){
    int submap=info->chmuxlist[i];
    floormemo[i]=look->floor_func[submap]->
      inverse1(vb,look->floor_look[submap]);
    if(floormemo[i])
      nonzero[i]=1;
    else
      nonzero[i]=0;
    memset(vb->pcm[i],0,sizeof(*vb->pcm[i])*n/2);
  }

  /* channel coupling can 'dirty' the nonzero listing */
  for(i=0;i<info->coupling_steps;i++){
    if(nonzero[info->coupling_mag[i]] ||
       nonzero[info->coupling_ang[i]]){
      nonzero[info->coupling_mag[i]]=1;
      nonzero[info->coupling_ang[i]]=1;
    }
  }

  /* recover the residue into our working vectors */
  for(i=0;i<info->submaps;i++){
    int ch_in_bundle=0;
    for(j=0;j<vi->channels;j++){
      if(info->chmuxlist[j]==i){
	if(nonzero[j])
	  zerobundle[ch_in_bundle]=1;
	else
	  zerobundle[ch_in_bundle]=0;
	pcmbundle[ch_in_bundle++]=vb->pcm[j];
      }
    }

    look->residue_func[i]->inverse(vb,look->residue_look[i],
				   pcmbundle,zerobundle,ch_in_bundle);
  }

  //for(j=0;j<vi->channels;j++)
  //_analysis_output("coupled",seq+j,vb->pcm[j],-8,n/2,0,0);


  /* channel coupling */
  for(i=info->coupling_steps-1;i>=0;i--){
    ogg_int32_t *pcmM=vb->pcm[info->coupling_mag[i]];
    ogg_int32_t *pcmA=vb->pcm[info->coupling_ang[i]];

    for(j=0;j<n/2;j++){
      ogg_int32_t mag=pcmM[j];
      ogg_int32_t ang=pcmA[j];

      if(mag>0)
	if(ang>0){
	  pcmM[j]=mag;
	  pcmA[j]=mag-ang;
	}else{
	  pcmA[j]=mag;
	  pcmM[j]=mag+ang;
	}
      else
	if(ang>0){
	  pcmM[j]=mag;
	  pcmA[j]=mag+ang;
	}else{
	  pcmA[j]=mag;
	  pcmM[j]=mag-ang;
	}
    }
  }

  //for(j=0;j<vi->channels;j++)
  //_analysis_output("residue",seq+j,vb->pcm[j],-8,n/2,0,0);

  /* compute and apply spectral envelope */
  for(i=0;i<vi->channels;i++){
    ogg_int32_t *pcm=vb->pcm[i];
    int submap=info->chmuxlist[i];
    look->floor_func[submap]->
      inverse2(vb,look->floor_look[submap],floormemo[i],pcm);
  }

  //for(j=0;j<vi->channels;j++)
  //_analysis_output("mdct",seq+j,vb->pcm[j],-24,n/2,0,1);

  /* transform the PCM data; takes PCM vector, vb; modifies PCM vector */
  /* only MDCT right now.... */
  for(i=0;i<vi->channels;i++){
    ogg_int32_t *pcm=vb->pcm[i];
    mdct_backward(n,pcm,pcm);
  }

  //for(j=0;j<vi->channels;j++)
  //_analysis_output("imdct",seq+j,vb->pcm[j],-24,n,0,0);

  /* window the data */
  for(i=0;i<vi->channels;i++){
    ogg_int32_t *pcm=vb->pcm[i];
    if(nonzero[i])
      _vorbis_apply_window(pcm,b->window,ci->blocksizes,vb->lW,vb->W,vb->nW);
    else
      for(j=0;j<n;j++)
	pcm[j]=0;

  }

  //for(j=0;j<vi->channels;j++)
  //_analysis_output("window",seq+j,vb->pcm[j],-24,n,0,0);

  seq+=vi->channels;
  /* all done! */
  return(0);
}

/* export hooks */
vorbis_func_mapping mapping0_exportbundle={
  &mapping0_unpack,
  &mapping0_look,
  &mapping0_free_info,
  &mapping0_free_look,
  &mapping0_inverse
};
