//  QuickTime MOV file parser
//  based on TOOLS/movinfo.c by A'rpi & Al3x

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "codec-cfg.h"
#include "stheader.h"

typedef struct {
    unsigned int pts; // duration
    unsigned int size;
    off_t pos;
} mov_sample_t;

typedef struct {
    unsigned int size; // samples/chunk
    int desc;          // for multiple codecs mode - not used
    off_t pos;
} mov_chunk_t;

typedef struct {
    unsigned int first;
    unsigned int spc;
    unsigned int sdid;
} mov_chunkmap_t;

typedef struct {
    int id;
    int type;
    int pos;
    //
    int timescale;
    unsigned int length;
    int width,height; // for video
    unsigned int fourcc;
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
} mov_track_t;

void mov_build_index(mov_track_t* trak){
    int i,j,s;
    int last=trak->chunks_size;
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
	if(pos>=endpos) return; // END
	len=stream_read_dword(demuxer->stream);
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
		for(i=0;i<len;i++){
		    unsigned int num=stream_read_dword(demuxer->stream);
		    unsigned int dur=stream_read_dword(demuxer->stream);
		    printf("num=%d  dur=%d  [%d] (%d)\n",num,dur,trak->samples_size,x);
		    num+=x;
		    // extend array if needed:
		    if(num>trak->samples_size){
			trak->samples=realloc(trak->samples,sizeof(mov_sample_t)*num);
			trak->samples_size=num;
		    }
		    // fill array:
		    while(x<num){
			trak->samples[x].pts=pts;
			pts+=dur;
			++x;
		    }
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
		    trak->chunkmap[i].first=stream_read_dword(demuxer->stream);
		    trak->chunkmap[i].spc=stream_read_dword(demuxer->stream);
		    trak->chunkmap[i].sdid=stream_read_dword(demuxer->stream);
		}
		break;
	    }
	    case MOV_FOURCC('s','t','s','z'): {
		int temp=stream_read_dword(demuxer->stream);
		int ss=stream_read_dword(demuxer->stream);
		int len=stream_read_dword(demuxer->stream);
		int i;
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sSample size table!  len=%d  ss=%d\n",level,"",len,ss);
		// extend array if needed:
		if(len>trak->samples_size){
		    trak->samples=realloc(trak->samples,sizeof(mov_sample_t)*len);
		    trak->samples_size=len;
		}
		for(i=0;i<len;i++){
		    trak->samples[i].size=ss?ss:stream_read_dword(demuxer->stream);
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
	if(id==MOV_FOURCC('c','m','o','v')){
	    mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_MOVcomprhdr);
	    return;
	}
	
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

    // Build tables:
    // ...
    
    mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_MOVnotyetsupp);

    return 1;
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

    // read sample:
    if(trak->pos>=trak->samples_size) return 0; // EOF
    
    stream_seek(demuxer->stream,trak->samples[trak->pos].pos);
    pts=(float)trak->samples[trak->pos].pts/(float)trak->timescale;
    ds_read_packet(ds,demuxer->stream,trak->samples[trak->pos].size,pts,trak->samples[trak->pos].pos,0);
    ++trak->pos;
    return 1;
    
}
