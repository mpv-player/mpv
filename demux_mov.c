//  QuickTime MOV file parser
//  based on TOOLS/movinfo.c by A'rpi & Al3x

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"

#include "stream.h"
#include "demuxer.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "codec-cfg.h"
#include "stheader.h"

typedef struct {
    int id;
    int type;
    int timescale;
    int width,height; // for video
    unsigned int fourcc;
    int data_len;
    unsigned char* data;
} mov_track_t;

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
		// read width x height
		break;
	    }
	    case MOV_FOURCC('m','d','h','d'): {
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sMedia header!\n",level,"");
		// read timescale
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
			trak->data_len=len-8;
			trak->data=malloc(trak->data_len);
			stream_read(demuxer->stream,trak->data,trak->data_len);
			if(trak->type==MOV_TRAK_VIDEO && trak->data_len>43){
			    mp_msg(MSGT_DEMUX,MSGL_V," '%.*s'",trak->data_len-43,trak->data+43);
			}
		    }
		    mp_msg(MSGT_DEMUX,MSGL_V,"\n");
		    if(fourcc!=trak->fourcc && i)
			mp_msg(MSGT_DEMUX,MSGL_WARN,"MOV: Warning! variable FOURCC detected!?\n");
		    if(!stream_seek(demuxer->stream,pos+len)) break;
		}
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
		mp_msg(MSGT_DEMUX,MSGL_WARN,"MOV: Warning! too many tracks!");
		return;
	    }
	    trak=malloc(sizeof(mov_track_t));
	    memset(trak,0,sizeof(mov_track_t));
	    mp_msg(MSGT_DEMUX,MSGL_V,"MOV: Track #%d:\n",priv->track_db);
	    trak->id=priv->track_db;
	    priv->tracks[priv->track_db++]=trak;
	    lschunks(demuxer,level+1,pos+len,trak);
	    trak=NULL;
	} else
	if(id==MOV_FOURCC('c','m','o','v')){
	    mp_msg(MSGT_DEMUX,MSGL_ERR,"MOV: Compressed headers not (yet) supported!\n");
	    return;
	}
	
	pos+=len+8;
	if(pos>=endpos) break;
	if(!stream_seek(demuxer->stream,pos)) break;
    }
}

int mov_read_header(demuxer_t* demuxer){
    mov_priv_t* priv=demuxer->priv;
    
//    printf("mov_read_header!\n");

    // Parse header:    
    if(!stream_seek(demuxer->stream,priv->moov_start)) return 0; // ???
    lschunks(demuxer, 0, priv->moov_end, NULL);

    // Build tables:
    // ...
    
    mp_msg(MSGT_DEMUX,MSGL_ERR,"\n****** Quicktime MOV format not yet supported!!!!!!! *******\n");

    return 1;
}
