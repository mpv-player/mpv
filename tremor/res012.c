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

 function: residue backend 0, 1 and 2 implementation

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "ivorbiscodec.h"
#include "codec_internal.h"
#include "registry.h"
#include "codebook.h"
#include "misc.h"
#include "os.h"
#include "block.h"

typedef struct {
  vorbis_info_residue0 *info;
  int         map;

  int         parts;
  int         stages;
  codebook   *fullbooks;
  codebook   *phrasebook;
  codebook ***partbooks;

  int         partvals;
  int       **decodemap;

} vorbis_look_residue0;

void res0_free_info(vorbis_info_residue *i){
  vorbis_info_residue0 *info=(vorbis_info_residue0 *)i;
  if(info){
    memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}

void res0_free_look(vorbis_look_residue *i){
  int j;
  if(i){

    vorbis_look_residue0 *look=(vorbis_look_residue0 *)i;

    for(j=0;j<look->parts;j++)
      if(look->partbooks[j])_ogg_free(look->partbooks[j]);
    _ogg_free(look->partbooks);
    for(j=0;j<look->partvals;j++)
      _ogg_free(look->decodemap[j]);
    _ogg_free(look->decodemap);

    memset(look,0,sizeof(*look));
    _ogg_free(look);
  }
}

static int ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

static int icount(unsigned int v){
  int ret=0;
  while(v){
    ret+=v&1;
    v>>=1;
  }
  return(ret);
}

/* vorbis_info is for range checking */
vorbis_info_residue *res0_unpack(vorbis_info *vi,oggpack_buffer *opb){
  int j,acc=0;
  vorbis_info_residue0 *info=(vorbis_info_residue0 *)_ogg_calloc(1,sizeof(*info));
  codec_setup_info     *ci=(codec_setup_info *)vi->codec_setup;

  info->begin=oggpack_read(opb,24);
  info->end=oggpack_read(opb,24);
  info->grouping=oggpack_read(opb,24)+1;
  info->partitions=oggpack_read(opb,6)+1;
  info->groupbook=oggpack_read(opb,8);

  for(j=0;j<info->partitions;j++){
    int cascade=oggpack_read(opb,3);
    if(oggpack_read(opb,1))
      cascade|=(oggpack_read(opb,5)<<3);
    info->secondstages[j]=cascade;

    acc+=icount(cascade);
  }
  for(j=0;j<acc;j++)
    info->booklist[j]=oggpack_read(opb,8);

  if(info->groupbook>=ci->books)goto errout;
  for(j=0;j<acc;j++)
    if(info->booklist[j]>=ci->books)goto errout;

  return(info);
 errout:
  res0_free_info(info);
  return(NULL);
}

vorbis_look_residue *res0_look(vorbis_dsp_state *vd,vorbis_info_mode *vm,
			  vorbis_info_residue *vr){
  vorbis_info_residue0 *info=(vorbis_info_residue0 *)vr;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)_ogg_calloc(1,sizeof(*look));
  codec_setup_info     *ci=(codec_setup_info *)vd->vi->codec_setup;

  int j,k,acc=0;
  int dim;
  int maxstage=0;
  look->info=info;
  look->map=vm->mapping;

  look->parts=info->partitions;
  look->fullbooks=ci->fullbooks;
  look->phrasebook=ci->fullbooks+info->groupbook;
  dim=look->phrasebook->dim;

  look->partbooks=(codebook ***)_ogg_calloc(look->parts,sizeof(*look->partbooks));

  for(j=0;j<look->parts;j++){
    int stages=ilog(info->secondstages[j]);
    if(stages){
      if(stages>maxstage)maxstage=stages;
      look->partbooks[j]=(codebook **)_ogg_calloc(stages,sizeof(*look->partbooks[j]));
      for(k=0;k<stages;k++)
	if(info->secondstages[j]&(1<<k)){
	  look->partbooks[j][k]=ci->fullbooks+info->booklist[acc++];
#ifdef TRAIN_RES
	  look->training_data[k][j]=calloc(look->partbooks[j][k]->entries,
					   sizeof(***look->training_data));
#endif
	}
    }
  }

  look->partvals=look->parts;
  for(j=1;j<dim;j++)look->partvals*=look->parts;
  look->stages=maxstage;
  look->decodemap=(int **)_ogg_malloc(look->partvals*sizeof(*look->decodemap));
  for(j=0;j<look->partvals;j++){
    long val=j;
    long mult=look->partvals/look->parts;
    look->decodemap[j]=(int *)_ogg_malloc(dim*sizeof(*look->decodemap[j]));
    for(k=0;k<dim;k++){
      long deco=val/mult;
      val-=deco*mult;
      mult/=look->parts;
      look->decodemap[j][k]=deco;
    }
  }

  return(look);
}


/* a truncated packet here just means 'stop working'; it's not an error */
static int _01inverse(vorbis_block *vb,vorbis_look_residue *vl,
		      ogg_int32_t **in,int ch,
		      long (*decodepart)(codebook *, ogg_int32_t *,
					 oggpack_buffer *,int,int)){

  long i,j,k,l,s;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int partitions_per_word=look->phrasebook->dim;
  int n=info->end-info->begin;

  int partvals=n/samples_per_partition;
  int partwords=(partvals+partitions_per_word-1)/partitions_per_word;
  int ***partword=(int ***)alloca(ch*sizeof(*partword));

  for(j=0;j<ch;j++)
    partword[j]=(int **)_vorbis_block_alloc(vb,partwords*sizeof(*partword[j]));

  for(s=0;s<look->stages;s++){

    /* each loop decodes on partition codeword containing
       partitions_pre_word partitions */
    for(i=0,l=0;i<partvals;l++){
      if(s==0){
	/* fetch the partition word for each channel */
	for(j=0;j<ch;j++){
	  int temp=vorbis_book_decode(look->phrasebook,&vb->opb);
	  if(temp==-1)goto eopbreak;
	  partword[j][l]=look->decodemap[temp];
	  if(partword[j][l]==NULL)goto errout;
	}
      }

      /* now we decode residual values for the partitions */
      for(k=0;k<partitions_per_word && i<partvals;k++,i++)
	for(j=0;j<ch;j++){
	  long offset=info->begin+i*samples_per_partition;
	  if(info->secondstages[partword[j][l][k]]&(1<<s)){
	    codebook *stagebook=look->partbooks[partword[j][l][k]][s];
	    if(stagebook){
	      if(decodepart(stagebook,in[j]+offset,&vb->opb,
			    samples_per_partition,-8)==-1)goto eopbreak;
	    }
	  }
	}
    }
  }

 errout:
 eopbreak:
  return(0);
}

int res0_inverse(vorbis_block *vb,vorbis_look_residue *vl,
		 ogg_int32_t **in,int *nonzero,int ch){
  int i,used=0;
  for(i=0;i<ch;i++)
    if(nonzero[i])
      in[used++]=in[i];
  if(used)
    return(_01inverse(vb,vl,in,used,vorbis_book_decodevs_add));
  else
    return(0);
}

int res1_inverse(vorbis_block *vb,vorbis_look_residue *vl,
		 ogg_int32_t **in,int *nonzero,int ch){
  int i,used=0;
  for(i=0;i<ch;i++)
    if(nonzero[i])
      in[used++]=in[i];
  if(used)
    return(_01inverse(vb,vl,in,used,vorbis_book_decodev_add));
  else
    return(0);
}

/* duplicate code here as speed is somewhat more important */
int res2_inverse(vorbis_block *vb,vorbis_look_residue *vl,
		 ogg_int32_t **in,int *nonzero,int ch){
  long i,k,l,s;
  vorbis_look_residue0 *look=(vorbis_look_residue0 *)vl;
  vorbis_info_residue0 *info=look->info;

  /* move all this setup out later */
  int samples_per_partition=info->grouping;
  int partitions_per_word=look->phrasebook->dim;
  int n=info->end-info->begin;

  int partvals=n/samples_per_partition;
  int partwords=(partvals+partitions_per_word-1)/partitions_per_word;
  int **partword=(int **)_vorbis_block_alloc(vb,partwords*sizeof(*partword));
  int beginoff=info->begin/ch;

  for(i=0;i<ch;i++)if(nonzero[i])break;
  if(i==ch)return(0); /* no nonzero vectors */

  samples_per_partition/=ch;

  for(s=0;s<look->stages;s++){
    for(i=0,l=0;i<partvals;l++){

      if(s==0){
	/* fetch the partition word */
	int temp=vorbis_book_decode(look->phrasebook,&vb->opb);
	if(temp==-1)goto eopbreak;
	partword[l]=look->decodemap[temp];
	if(partword[l]==NULL)goto errout;
      }

      /* now we decode residual values for the partitions */
      for(k=0;k<partitions_per_word && i<partvals;k++,i++)
	if(info->secondstages[partword[l][k]]&(1<<s)){
	  codebook *stagebook=look->partbooks[partword[l][k]][s];

	  if(stagebook){
	    if(vorbis_book_decodevv_add(stagebook,in,
					i*samples_per_partition+beginoff,ch,
					&vb->opb,
					samples_per_partition,-8)==-1)
	      goto eopbreak;
	  }
	}
    }
  }

 errout:
 eopbreak:
  return(0);
}


vorbis_func_residue residue0_exportbundle={
  &res0_unpack,
  &res0_look,
  &res0_free_info,
  &res0_free_look,
  &res0_inverse
};

vorbis_func_residue residue1_exportbundle={
  &res0_unpack,
  &res0_look,
  &res0_free_info,
  &res0_free_look,
  &res1_inverse
};

vorbis_func_residue residue2_exportbundle={
  &res0_unpack,
  &res0_look,
  &res0_free_info,
  &res0_free_look,
  &res2_inverse
};
