//  QuickTime MOV file parser by A'rpi
//  based on TOOLS/movinfo.c by me & Al3x
//  compressed header support from moov.c of the openquicktime lib.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#ifdef HAVE_PNG
// should be detected by ./configure...
#define HAVE_ZLIB
#endif

#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

typedef struct {
    unsigned int pts; // duration
    unsigned int size;
    off_t pos;
} mov_sample_t;

typedef struct {
    unsigned int sample; // number of the first sample in teh chunk
    unsigned int size;   // number of samples in the chunk
    int desc;            // for multiple codecs mode - not used
    off_t pos;
} mov_chunk_t;

typedef struct {
    unsigned int first;
    unsigned int spc;
    unsigned int sdid;
} mov_chunkmap_t;

typedef struct {
    unsigned int num;
    unsigned int dur;
} mov_durmap_t;

typedef struct {
    int id;
    int type;
    int pos;
    //
    int timescale;
    unsigned int length;
    int samplesize;  // 0 = variable
    int duration;    // 0 = variable
    int width,height; // for video
    unsigned int fourcc;
    //
    int tkdata_len;  // track data 
    unsigned char* tkdata;
    int stdata_len;  // stream data
    unsigned char* stdata;
    int samples_size;
    mov_sample_t* samples;
    int chunks_size;
    mov_chunk_t* chunks;
    int chunkmap_size;
    mov_chunkmap_t* chunkmap;
    int durmap_size;
    mov_durmap_t* durmap;
} mov_track_t;

void mov_build_index(mov_track_t* trak){
    int i,j,s;
    int last=trak->chunks_size;
    unsigned int pts=0;
    printf("MOV track: %d chunks, %d samples\n",trak->chunks_size,trak->samples_size);
    printf("pts=%d  scale=%d  time=%5.3f\n",trak->length,trak->timescale,(float)trak->length/(float)trak->timescale);
    // process chunkmap:
    i=trak->chunkmap_size;
    while(i>0){
	--i;
	for(j=trak->chunkmap[i].first;j<last;j++){
	    trak->chunks[j].desc=trak->chunkmap[i].sdid;
	    trak->chunks[j].size=trak->chunkmap[i].spc;
	}
	last=trak->chunkmap[i].first;
    }

    // calc pts of chunks:
    s=0;
    for(j=0;j<trak->chunks_size;j++){
        trak->chunks[j].sample=s;
        s+=trak->chunks[j].size;
    }

    if(!trak->samples_size){
	// constant sampesize
	if(trak->durmap_size==1 || (trak->durmap_size==2 && trak->durmap[1].num==1)){
	    trak->duration=trak->durmap[0].dur;
	} else printf("*** constant samplesize & variable duration not yet supported! ***\nContact the author if you have such sample file!\n");
	return;
    }
    
    // calc pts:
    s=0;
    for(j=0;j<trak->durmap_size;j++){
	for(i=0;i<trak->durmap[j].num;i++){
	    trak->samples[s].pts=pts;
	    ++s;
	    pts+=trak->durmap[j].dur;
	}
    }
    
    // calc sample offsets
    s=0;
    for(j=0;j<trak->chunks_size;j++){
	off_t pos=trak->chunks[j].pos;
	for(i=0;i<trak->chunks[j].size;i++){
	    trak->samples[s].pos=pos;
#if 0
	    printf("Sample %5d: pts=%8d  off=0x%08X  size=%d\n",s,
		trak->samples[s].pts,
		(int)trak->samples[s].pos,
		trak->samples[s].size);
#endif
	    pos+=trak->samples[s].size;
	    ++s;
	}
    }

}

#define MOV_MAX_TRACKS 256

#define MOV_TRAK_UNKNOWN 0
#define MOV_TRAK_VIDEO 1
#define MOV_TRAK_AUDIO 2

typedef struct {
    off_t moov_start;
    off_t moov_end;
    off_t mdat_start;
    off_t mdat_end;
    int track_db;
    mov_track_t* tracks[MOV_MAX_TRACKS];
} mov_priv_t;

#define MOV_FOURCC(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|(d))

int mov_check_file(demuxer_t* demuxer){
    int flags=0;
    mov_priv_t* priv=malloc(sizeof(mov_priv_t));
    
    mp_msg(MSGT_DEMUX,MSGL_V,"Checking for MOV\n");
    
    memset(priv,0,sizeof(mov_priv_t));
    demuxer->priv=priv;
    
    while(1){
	off_t len=stream_read_dword(demuxer->stream);
	unsigned int id=stream_read_dword(demuxer->stream);
	if(stream_eof(demuxer->stream)) break; // EOF
	if(len<8) break; // invalid chunk
	switch(id){
	case MOV_FOURCC('m','o','o','v'):
	  mp_msg(MSGT_DEMUX,MSGL_V,"MOV: Movie header found!\n");
	  priv->moov_start=stream_tell(demuxer->stream);
	  priv->moov_end=priv->moov_start+len-8;
	  flags|=1;
	  break;
	case MOV_FOURCC('m','d','a','t'):
	  mp_msg(MSGT_DEMUX,MSGL_V,"MOV: Movie DATA found!\n");
	  priv->mdat_start=stream_tell(demuxer->stream);
	  priv->mdat_end=priv->mdat_start+len-8;
	  flags|=2;
	  break;
	default:
	  mp_msg(MSGT_DEMUX,MSGL_V,"MOV: unknown chunk: %.4s %d\n",&id,(int)len);
	}
	if(!stream_skip(demuxer->stream,len-8)) break;
    }
    
    if(flags==1)
	mp_msg(MSGT_DEMUX,MSGL_WARN,"MOV: missing data (mdat) chunk! Maybe broken file...\n");
    else if(flags==2)
	mp_msg(MSGT_DEMUX,MSGL_WARN,"MOV: missing header (moov/cmov) chunk! Maybe broken file...\n");

return (flags==3);
}

static void lschunks(demuxer_t* demuxer,int level,off_t endpos,mov_track_t* trak){
    mov_priv_t* priv=demuxer->priv;
    while(1){
	off_t pos;
	off_t len;
	unsigned int id;
	//
	pos=stream_tell(demuxer->stream);
//	printf("stream_tell==%d\n",pos);
	if(pos>=endpos) return; // END
	len=stream_read_dword(demuxer->stream);
//	printf("len==%d\n",len);
	if(len<8) return; // error
	len-=8;
	id=stream_read_dword(demuxer->stream);
	//
	mp_msg(MSGT_DEMUX,MSGL_DBG2,"lschunks %.4s  %d\n",&id,(int)len);
	//
	if(trak){
	  switch(id){
	    case MOV_FOURCC('t','k','h','d'): {
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sTrack header!\n",level,"");
		// read codec data
		trak->tkdata_len=len;
		trak->tkdata=malloc(trak->tkdata_len);
		stream_read(demuxer->stream,trak->tkdata,trak->tkdata_len);
		break;
	    }
	    case MOV_FOURCC('m','d','h','d'): {
		unsigned int tmp;
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sMedia header!\n",level,"");
#if 0
		tmp=stream_read_dword(demuxer->stream);
		printf("dword1: 0x%08X (%d)\n",tmp,tmp);
		tmp=stream_read_dword(demuxer->stream);
		printf("dword2: 0x%08X (%d)\n",tmp,tmp);
		tmp=stream_read_dword(demuxer->stream);
		printf("dword3: 0x%08X (%d)\n",tmp,tmp);
		tmp=stream_read_dword(demuxer->stream);
		printf("dword4: 0x%08X (%d)\n",tmp,tmp);
		tmp=stream_read_dword(demuxer->stream);
		printf("dword5: 0x%08X (%d)\n",tmp,tmp);
		tmp=stream_read_dword(demuxer->stream);
		printf("dword6: 0x%08X (%d)\n",tmp,tmp);
#endif
		stream_skip(demuxer->stream,12);
		// read timescale
		trak->timescale=stream_read_dword(demuxer->stream);
		// read length
		trak->length=stream_read_dword(demuxer->stream);
		break;
	    }
	    case MOV_FOURCC('v','m','h','d'): {
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sVideo header!\n",level,"");
		trak->type=MOV_TRAK_VIDEO;
		// read video data
		break;
	    }
	    case MOV_FOURCC('s','m','h','d'): {
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sSound header!\n",level,"");
		trak->type=MOV_TRAK_AUDIO;
		// read audio data
		break;
	    }
	    case MOV_FOURCC('s','t','s','d'): {
		int i=stream_read_dword(demuxer->stream); // temp!
		int count=stream_read_dword(demuxer->stream);
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sDescription list! (cnt:%d)\n",level,"",count);
		for(i=0;i<count;i++){
		    off_t pos=stream_tell(demuxer->stream);
		    off_t len=stream_read_dword(demuxer->stream);
		    unsigned int fourcc=stream_read_dword_le(demuxer->stream);
		    if(len<8) break; // error
		    mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*s desc #%d: %.4s",level,"",i,&fourcc);
		    if(!i){
			trak->fourcc=fourcc;
			// read codec data
			trak->stdata_len=len-8;
			trak->stdata=malloc(trak->stdata_len);
			stream_read(demuxer->stream,trak->stdata,trak->stdata_len);
			if(trak->type==MOV_TRAK_VIDEO && trak->stdata_len>43){
			    mp_msg(MSGT_DEMUX,MSGL_V," '%.*s'",trak->stdata_len-43,trak->stdata+43);
			}
		    }
		    mp_msg(MSGT_DEMUX,MSGL_V,"\n");
		    if(fourcc!=trak->fourcc && i)
			mp_msg(MSGT_DEMUX,MSGL_WARN,MSGTR_MOVvariableFourCC);
		    if(!stream_seek(demuxer->stream,pos+len)) break;
		}
		break;
	    }
	    case MOV_FOURCC('s','t','t','s'): {
		int temp=stream_read_dword(demuxer->stream);
		int len=stream_read_dword(demuxer->stream);
		int i;
		int x=0;
		unsigned int pts=0;
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sSample duration table! (%d blocks)\n",level,"",len);
		trak->durmap=malloc(sizeof(mov_durmap_t)*len);
		memset(trak->durmap,0,sizeof(mov_durmap_t)*len);
		trak->durmap_size=len;
		for(i=0;i<len;i++){
		    trak->durmap[i].num=stream_read_dword(demuxer->stream);
		    trak->durmap[i].dur=stream_read_dword(demuxer->stream);
		    pts+=trak->durmap[i].num*trak->durmap[i].dur;
		}
		if(trak->length!=pts) printf("Warning! pts=%d  length=%d\n",pts,trak->length);
		break;
	    }
	    case MOV_FOURCC('s','t','s','c'): {
		int temp=stream_read_dword(demuxer->stream);
		int len=stream_read_dword(demuxer->stream);
		int i;
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sSample->Chunk mapping table!  (%d blocks)\n",level,"",len);
		// read data:
		trak->chunkmap_size=len;
		trak->chunkmap=malloc(sizeof(mov_chunkmap_t)*len);
		for(i=0;i<len;i++){
		    trak->chunkmap[i].first=stream_read_dword(demuxer->stream)-1;
		    trak->chunkmap[i].spc=stream_read_dword(demuxer->stream);
		    trak->chunkmap[i].sdid=stream_read_dword(demuxer->stream);
		}
		break;
	    }
	    case MOV_FOURCC('s','t','s','z'): {
		int temp=stream_read_dword(demuxer->stream);
		int ss=stream_read_dword(demuxer->stream);
		int len=stream_read_dword(demuxer->stream);
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sSample size table!  len=%d  ss=%d\n",level,"",len,ss);
		trak->samplesize=ss;
		if(!ss){
		    // variable samplesize
		    int i;
		    trak->samples=realloc(trak->samples,sizeof(mov_sample_t)*len);
		    trak->samples_size=len;
		    for(i=0;i<len;i++)
			trak->samples[i].size=stream_read_dword(demuxer->stream);
		}
		break;
	    }
	    case MOV_FOURCC('s','t','c','o'): {
		int temp=stream_read_dword(demuxer->stream);
		int len=stream_read_dword(demuxer->stream);
		int i;
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sChunk offset table! (%d chunks)\n",level,"",len);
		// extend array if needed:
		if(len>trak->chunks_size){
		    trak->chunks=realloc(trak->chunks,sizeof(mov_chunk_t)*len);
		    trak->chunks_size=len;
		}
		// read elements:
		for(i=0;i<len;i++) trak->chunks[i].pos=stream_read_dword(demuxer->stream);
		break;
	    }
	    case MOV_FOURCC('m','d','i','a'): {
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sMedia stream!\n",level,"");
		lschunks(demuxer,level+1,pos+len,trak);
		break;
	    }
	    case MOV_FOURCC('m','i','n','f'): {
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sMedia info!\n",level,"");
		lschunks(demuxer,level+1,pos+len,trak);
		break;
	    }
	    case MOV_FOURCC('s','t','b','l'): {
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sSample info!\n",level,"");
		lschunks(demuxer,level+1,pos+len,trak);
		break;
	    }
	  }//switch(id)
	} else
	if(id==MOV_FOURCC('t','r','a','k')){
//	    if(trak) printf("MOV: Warning! trak in trak?\n");
	    if(priv->track_db>=MOV_MAX_TRACKS){
		mp_msg(MSGT_DEMUX,MSGL_WARN,MSGTR_MOVtooManyTrk);
		return;
	    }
	    trak=malloc(sizeof(mov_track_t));
	    memset(trak,0,sizeof(mov_track_t));
	    mp_msg(MSGT_DEMUX,MSGL_V,"MOV: Track #%d:\n",priv->track_db);
	    trak->id=priv->track_db;
	    priv->tracks[priv->track_db]=trak;
	    lschunks(demuxer,level+1,pos+len,trak);
	    mov_build_index(trak);
	    switch(trak->type){
	    case MOV_TRAK_AUDIO: {
		sh_audio_t* sh=new_sh_audio(demuxer,priv->track_db);
		sh->format=trak->fourcc;
		printf("!!! audio bits: %d  chans: %d\n",trak->stdata[19],trak->stdata[17]);
		printf("Fourcc: %.4s\n",&trak->fourcc);
		// Emulate WAVEFORMATEX struct:
		sh->wf=malloc(sizeof(WAVEFORMATEX));
		memset(sh->wf,0,sizeof(WAVEFORMATEX));
		sh->wf->nChannels=trak->stdata[17];
		sh->wf->wBitsPerSample=trak->stdata[19];
		sh->wf->nSamplesPerSec=trak->timescale;
		sh->wf->nAvgBytesPerSec=sh->wf->nChannels*((sh->wf->wBitsPerSample+7)/8)*sh->wf->nSamplesPerSec;
		// Selection:
		if(demuxer->audio->id==-1 || demuxer->audio->id==priv->track_db){
		    // (auto)selected audio track:
		    demuxer->audio->id=priv->track_db;
		    demuxer->audio->sh=sh; sh->ds=demuxer->audio;
		}
		break;
	    }
	    case MOV_TRAK_VIDEO: {
		sh_video_t* sh=new_sh_video(demuxer,priv->track_db);
		sh->format=trak->fourcc;
		sh->fps=trak->timescale;
		sh->frametime=1.0f/sh->fps;
		sh->disp_w=trak->tkdata[77]|(trak->tkdata[76]<<8);
		sh->disp_h=trak->tkdata[81]|(trak->tkdata[80]<<8);

		// emulate BITMAPINFOHEADER:
		sh->bih=malloc(sizeof(BITMAPINFOHEADER));
		memset(sh->bih,0,sizeof(BITMAPINFOHEADER));
		sh->bih->biSize=40;
		sh->bih->biWidth=sh->disp_w;
		sh->bih->biHeight=sh->disp_h;
		sh->bih->biPlanes=0;
		sh->bih->biBitCount=16;
		sh->bih->biCompression=trak->fourcc;
		sh->bih->biSizeImage=sh->bih->biWidth*sh->bih->biHeight;

		printf("Image size: %d x %d\n",sh->disp_w,sh->disp_h);
		printf("Fourcc: %.4s  Codec: '%.*s'\n",&trak->fourcc,trak->stdata_len-43,trak->stdata+43);
		
		if(demuxer->video->id==-1 || demuxer->video->id==priv->track_db){
		    // (auto)selected video track:
		    demuxer->video->id=priv->track_db;
		    demuxer->video->sh=sh; sh->ds=demuxer->video;
		}
		break;
	    }
	    }
	    printf("--------------\n");
	    priv->track_db++;
	    trak=NULL;
	} else
#ifndef HAVE_ZLIB
	if(id==MOV_FOURCC('c','m','o','v')){
	    mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_MOVcomprhdr);
	    return;
	}
#else
	if(id==MOV_FOURCC('c','m','o','v')){
//	    mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_MOVcomprhdr);
	    lschunks(demuxer,level+1,pos+len,NULL);
	} else
	if(id==MOV_FOURCC('d','c','o','m')){
//	    int temp=stream_read_dword(demuxer->stream);
	    unsigned int len=stream_read_dword(demuxer->stream);
	    printf("Compressed header uses %.4s algo!\n",&len);
	} else
	if(id==MOV_FOURCC('c','m','v','d')){
//	    int temp=stream_read_dword(demuxer->stream);
	    unsigned int moov_sz=stream_read_dword(demuxer->stream);
	    unsigned int cmov_sz=len-4;
	    unsigned char* cmov_buf=malloc(cmov_sz);
	    unsigned char* moov_buf=malloc(moov_sz+16);
	    int zret;
	    z_stream zstrm;
	    stream_t* backup;

	    printf("Compressed header size: %d / %d\n",cmov_sz,moov_sz);

	    stream_read(demuxer->stream,cmov_buf,cmov_sz);

	      zstrm.zalloc          = (alloc_func)0;
	      zstrm.zfree           = (free_func)0;
	      zstrm.opaque          = (voidpf)0;
	      zstrm.next_in         = cmov_buf;
	      zstrm.avail_in        = cmov_sz;
	      zstrm.next_out        = moov_buf;
	      zstrm.avail_out       = moov_sz;
	    
	      zret = inflateInit(&zstrm);
	      if (zret != Z_OK)
		{ fprintf(stderr,"QT cmov: inflateInit err %d\n",zret);
		return;
		}
	      zret = inflate(&zstrm, Z_NO_FLUSH);
	      if ((zret != Z_OK) && (zret != Z_STREAM_END))
		{ fprintf(stderr,"QT cmov inflate: ERR %d\n",zret);
		return;
		}
#if 0
	      else {
		FILE *DecOut;
		DecOut = fopen("Out.bin", "w");
		fwrite(moov_buf, 1, moov_sz, DecOut);
		fclose(DecOut);
	      }
#endif
	      if(moov_sz != zstrm.total_out) printf("Warning! moov size differs cmov: %d  zlib: %d\n",moov_sz,zstrm.total_out);
	      zret = inflateEnd(&zstrm);
	      
	      backup=demuxer->stream;
	       demuxer->stream=new_memory_stream(moov_buf,moov_sz);
	       stream_skip(demuxer->stream,8);
	       lschunks(demuxer,level+1,moov_sz,NULL); // parse uncompr. 'moov'
	       //free_stream(demuxer->stream);
	      demuxer->stream=backup;
	    
	}
#endif
	
	pos+=len+8;
	if(pos>=endpos) break;
	if(!stream_seek(demuxer->stream,pos)) break;
    }
}

int mov_read_header(demuxer_t* demuxer){
    mov_priv_t* priv=demuxer->priv;
    
    printf("mov_read_header!\n");

    // Parse header:    
    stream_reset(demuxer->stream);
    if(!stream_seek(demuxer->stream,priv->moov_start)) return 0; // ???
    lschunks(demuxer, 0, priv->moov_end, NULL);

#if 1    
    return 1;
#else
    mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_MOVnotyetsupp);
    return 0;
#endif
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
int demux_mov_fill_buffer(demuxer_t *demuxer,demux_stream_t* ds){
    mov_priv_t* priv=demuxer->priv;
    mov_track_t* trak=NULL;
    float pts;
    
    if(ds->id<0 || ds->id>=priv->track_db) return 0;
    trak=priv->tracks[ds->id];

if(trak->samplesize){
    // read chunk:
    if(trak->pos>=trak->chunks_size) return 0; // EOF
    stream_seek(demuxer->stream,trak->chunks[trak->pos].pos);
    pts=(float)(trak->chunks[trak->pos].sample*trak->duration)/(float)trak->timescale;
    ds_read_packet(ds,demuxer->stream,trak->chunks[trak->pos].size*trak->samplesize,pts,trak->chunks[trak->pos].pos,0);
} else {
    // read sample:
    if(trak->pos>=trak->samples_size) return 0; // EOF
    stream_seek(demuxer->stream,trak->samples[trak->pos].pos);
    pts=(float)trak->samples[trak->pos].pts/(float)trak->timescale;
    ds_read_packet(ds,demuxer->stream,trak->samples[trak->pos].size,pts,trak->samples[trak->pos].pos,0);
}
    ++trak->pos;

    return 1;
    
}

static float mov_seek_track(mov_track_t* trak,float pts,int flags){

//    printf("MOV track seek called  %5.3f  \n",pts);
    if(flags&2) pts*=trak->length; else pts*=(float)trak->timescale;

if(trak->samplesize){
    int sample=pts/trak->duration;
//    printf("MOV track seek - chunk: %d  (pts: %5.3f  dur=%d)  \n",sample,pts,trak->duration);
    if(!(flags&1)) sample+=trak->chunks[trak->pos].sample; // relative
    trak->pos=0;
    while(trak->pos<trak->chunks_size && trak->chunks[trak->pos].sample<sample) ++trak->pos;
    pts=(float)(trak->chunks[trak->pos].sample*trak->duration)/(float)trak->timescale;
} else {
    unsigned int ipts=pts;
//    printf("MOV track seek - sample: %d  \n",ipts);
    if(!(flags&1)) ipts+=trak->samples[trak->pos].pts;
    trak->pos=0;
    while(trak->pos<trak->samples_size && trak->samples[trak->pos].pts<ipts) ++trak->pos;
    pts=(float)trak->samples[trak->pos].pts/(float)trak->timescale;
}

//    printf("MOV track seek done:  %5.3f  \n",pts);

return pts;
}

void demux_seek_mov(demuxer_t *demuxer,float pts,int flags){
    mov_priv_t* priv=demuxer->priv;
    demux_stream_t* ds;

//    printf("MOV seek called  %5.3f  flag=%d  \n",pts,flags);
    
    ds=demuxer->video;
    if(ds && ds->id>=0 && ds->id<priv->track_db){
	mov_track_t* trak=priv->tracks[ds->id];
	//if(flags&2) pts*=(float)trak->length/(float)trak->timescale;
	//if(!(flags&1)) pts+=ds->pts;
	pts=ds->pts=mov_seek_track(trak,pts,flags);
	flags=1; // absolute seconds
    }

    ds=demuxer->audio;
    if(ds && ds->id>=0 && ds->id<priv->track_db){
	mov_track_t* trak=priv->tracks[ds->id];
	//if(flags&2) pts*=(float)trak->length/(float)trak->timescale;
	//if(!(flags&1)) pts+=ds->pts;
	ds->pts=mov_seek_track(trak,pts,flags);
    }

}

