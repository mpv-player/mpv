//  QuickTime MOV file parser by A'rpi
//  based on TOOLS/movinfo.c by me & Al3x
//  compressed header support from moov.c of the openquicktime lib.
//  References: http://openquicktime.sf.net/, http://www.heroinewarrior.com/
//  http://www.geocities.com/SiliconValley/Lakes/2160/fformats/files/mov.pdf

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "bswap.h"

#include "qtpalette.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include <fcntl.h>

#define BE_16(x) (be2me_16(*(unsigned short *)(x)))
#define BE_32(x) (be2me_32(*(unsigned int *)(x)))

typedef struct {
    unsigned int pts; // duration
    unsigned int size;
    off_t pos;
} mov_sample_t;

typedef struct {
    unsigned int sample; // number of the first sample in the chunk
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

#define MOV_TRAK_UNKNOWN 0
#define MOV_TRAK_VIDEO 1
#define MOV_TRAK_AUDIO 2
#define MOV_TRAK_FLASH 3
#define MOV_TRAK_GENERIC 4
#define MOV_TRAK_CODE 5

typedef struct {
    int id;
    int type;
    off_t pos;
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
    //
    int samples_size;
    mov_sample_t* samples;
    int chunks_size;
    mov_chunk_t* chunks;
    int chunkmap_size;
    mov_chunkmap_t* chunkmap;
    int durmap_size;
    mov_durmap_t* durmap;
    int keyframes_size;
    unsigned int* keyframes;
} mov_track_t;

void mov_build_index(mov_track_t* trak){
    int i,j,s;
    int last=trak->chunks_size;
    unsigned int pts=0;

#if 0
    if (trak->chunks_size <= 0)
    {
	mp_msg(MSGT_DEMUX, MSGL_WARN, "No chunk offset table, trying to build one!\n");
	
	trak->chunks_size = trak->samples_size;
	trak->chunks = realloc(trak->chunks, sizeof(mov_chunk_t)*trak->chunks_size);
	
	for (i=0; i < trak->chunks_size; i++)
	    trak->chunks[i].pos = -1;
    }
#endif

    mp_msg(MSGT_DEMUX, MSGL_HINT, "MOV track: %d chunks, %d samples\n",trak->chunks_size,trak->samples_size);
    mp_msg(MSGT_DEMUX, MSGL_HINT, "pts=%d  scale=%d  time=%5.3f\n",trak->length,trak->timescale,(float)trak->length/(float)trak->timescale);

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

#if 0
    for (i=0; i < trak->chunks_size; i++)
    {
	/* fixup position */
	if (trak->chunks[i].pos == -1)
	    if (i > 0)
		trak->chunks[i].pos = trak->chunks[i-1].pos + trak->chunks[i-1].size;
	    else
		trak->chunks[i].pos = 0; /* FIXME: set initial pos */
    }
#endif

    // calc pts of chunks:
    s=0;
    for(j=0;j<trak->chunks_size;j++){
        trak->chunks[j].sample=s;
        s+=trak->chunks[j].size;
    }

    // workaround for fixed-size video frames (dv and uncompressed)
    if(!trak->samples_size && trak->type==MOV_TRAK_VIDEO){
	trak->samples_size=s;
	trak->samples=malloc(sizeof(mov_sample_t)*s);
	for(i=0;i<s;i++)
	    trak->samples[i].size=trak->samplesize;
	trak->samplesize=0;
    }

    if(!trak->samples_size){
	// constant sampesize
	if(trak->durmap_size==1 || (trak->durmap_size==2 && trak->durmap[1].num==1)){
	    trak->duration=trak->durmap[0].dur;
	} else mp_msg(MSGT_DEMUX, MSGL_ERR, "*** constant samplesize & variable duration not yet supported! ***\nContact the author if you have such sample file!\n");
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
	    mp_msg(MSGT_DEMUX, MSGL_DBG3, "Sample %5d: pts=%8d  off=0x%08X  size=%d\n",s,
		trak->samples[s].pts,
		(int)trak->samples[s].pos,
		trak->samples[s].size);
	    pos+=trak->samples[s].size;
	    ++s;
	}
    }

}

#define MOV_MAX_TRACKS 256

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
    int no=0;
    mov_priv_t* priv=malloc(sizeof(mov_priv_t));
    
    mp_msg(MSGT_DEMUX,MSGL_V,"Checking for MOV\n");
    
    memset(priv,0,sizeof(mov_priv_t));
    demuxer->priv=priv;
    
    while(1){
	int skipped=8;
	off_t len=stream_read_dword(demuxer->stream);
	unsigned int id=stream_read_dword(demuxer->stream);
	if(stream_eof(demuxer->stream)) break; // EOF
	if (len == 1) /* real size is 64bits - cjb */
	{
#ifndef _LARGEFILE_SOURCE
	    if (stream_read_dword(demuxer->stream) != 0)
		mp_msg(MSGT_DEMUX, MSGL_WARN, "64bit file, but you've MPlayer compiled without LARGEFILE support!\n");
	    len = stream_read_dword(demuxer->stream);
#else
	    len = stream_read_qword(demuxer->stream);
#endif
	    skipped += 8;
	}
	else if(len<8) break; // invalid chunk

	switch(id){
	case MOV_FOURCC('m','o','o','v'):
	  mp_msg(MSGT_DEMUX,MSGL_V,"MOV: Movie header found!\n");
	  priv->moov_start=stream_tell(demuxer->stream);
	  priv->moov_end=priv->moov_start+len-skipped;
	  flags|=1;
	  break;
	case MOV_FOURCC('m','d','a','t'):
	  mp_msg(MSGT_DEMUX,MSGL_V,"MOV: Movie DATA found!\n");
	  priv->mdat_start=stream_tell(demuxer->stream);
	  priv->mdat_end=priv->mdat_start+len-skipped;
	  flags|=2;
	  break;
	case MOV_FOURCC('f','r','e','e'):
	case MOV_FOURCC('s','k','i','p'):
	  /* unused, if you edit a mov, you can use space provided by free atoms (redefining it) */
	  break;
	case MOV_FOURCC('w','i','d','e'):
	  break;
	default:
	  if(no==0) return 0; // first chunk is bad!
	  id = be2me_32(id);
	  mp_msg(MSGT_DEMUX,MSGL_V,"MOV: unknown chunk: %.4s %d\n",&id,(int)len);
	}
	if(!stream_skip(demuxer->stream,len-skipped)) break;
	++no;
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
	    case MOV_FOURCC('f','r','e','e'):
	    case MOV_FOURCC('u','d','t','a'):
		/* here not supported :p */
		break;
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
	    case MOV_FOURCC('g','m','h','d'): {
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sGeneric header!\n",level,"");
		trak->type=MOV_TRAK_GENERIC;
		break;
	    }
	    case MOV_FOURCC('n','m','h','d'): {
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sGeneric header!\n",level,"");
		trak->type=MOV_TRAK_GENERIC;
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
		    mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*s desc #%d: %.4s  (%d bytes)\n",level,"",i,&fourcc,len-16);
		    if(!i){
			trak->fourcc=fourcc;
			// read type specific (audio/video/time/text etc) header
			// NOTE: trak type is not yet known at this point :(((
			trak->stdata_len=len-8;
			trak->stdata=malloc(trak->stdata_len);
			stream_read(demuxer->stream,trak->stdata,trak->stdata_len);
		    }
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
		    
		    if(i==0 && trak->type == MOV_TRAK_VIDEO)
		    {
		    sh_video_t* sh=new_sh_video(demuxer,priv->track_db);
		    if (!sh->fps)
			sh->fps = trak->timescale/trak->durmap[i].dur;
		    /* initial fps */
		    }
		}
		if(trak->length!=pts) mp_msg(MSGT_DEMUX, MSGL_WARN, "Warning! pts=%d  length=%d\n",pts,trak->length);
		break;
	    }
	    case MOV_FOURCC('s','t','s','c'): {
		int temp=stream_read_dword(demuxer->stream);
		int len=stream_read_dword(demuxer->stream);
		int ver = (temp << 24);
		int flags = (temp << 16)|(temp<<8)|temp;
		int i;
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sSample->Chunk mapping table!  (%d blocks) (ver:%d,flags:%ld)\n",
		    level,"",len,ver,flags);
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
		int ver = (temp << 24);
		int flags = (temp << 16)|(temp<<8)|temp;
		int entries=stream_read_dword(demuxer->stream);
		int i;
		
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*sSample size table! (entries=%d ss=%d) (ver:%d,flags:%ld)\n",
		    level,"",entries,ss,ver,flags);
		trak->samplesize=ss;
		if (!ss) {
		  // variable samplesize
		  trak->samples=realloc(trak->samples,sizeof(mov_sample_t)*entries);
		  trak->samples_size=entries;
		  for(i=0;i<entries;i++)
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
	    case MOV_FOURCC('c','o','6','4'): {
		int temp=stream_read_dword(demuxer->stream);
		int len=stream_read_dword(demuxer->stream);
		int i;
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: %*s64bit chunk offset table! (%d chunks)\n",level,"",len);
		// extend array if needed:
		if(len>trak->chunks_size){
		    trak->chunks=realloc(trak->chunks,sizeof(mov_chunk_t)*len);
		    trak->chunks_size=len;
		}
		// read elements:
		for(i=0;i<len;i++)
		{
#ifndef	_LARGEFILE_SOURCE
		    if (stream_read_dword(demuxer->stream) != 0)
			mp_msg(MSGT_DEMUX, MSGL_WARN, "Chunk %d has got 64bit address, but you've MPlayer compiled without LARGEFILE support!\n", i);
		    trak->chunks[i].pos = stream_read_dword(demuxer->stream);
#else
		    trak->chunks[i].pos = stream_read_qword(demuxer->stream);
#endif
		}
		break;
	    }
	    case MOV_FOURCC('s','t','s','s'): {
		int temp=stream_read_dword(demuxer->stream);
		int entries=stream_read_dword(demuxer->stream);
		int ver = (temp << 24);
		int flags = (temp << 16)|(temp<<8)|temp;
		int i;
		mp_msg(MSGT_DEMUX, MSGL_V,"MOV: %*sSyncing samples (keyframes) table! (%d entries) (ver:%d,flags:%ld)\n",
		    level, "",entries, ver, flags);
		trak->keyframes_size=entries;
		trak->keyframes=malloc(sizeof(unsigned int)*entries);
		for (i=0;i<entries;i++)
		    trak->keyframes[i]=stream_read_dword(demuxer->stream)-1;
//		for (i=0;i<entries;i++) printf("%3d: %d\n",i,trak->keyframes[i]);
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
	    case MOV_FOURCC('e','d','t','s'): {
		mp_msg(MSGT_DEMUX, MSGL_V, "MOV: %*sEdit atom!\n", level, "");
		lschunks(demuxer,level+1,pos+len,trak);
		break;
	    }
	    case MOV_FOURCC('e','l','s','t'): {
		int temp=stream_read_dword(demuxer->stream);
		int entries=stream_read_dword(demuxer->stream);
		int ver = (temp << 24);
		int flags = (temp << 16)|(temp<<8)|temp;
		int i;
	
		mp_msg(MSGT_DEMUX, MSGL_V,"MOV: %*sEdit list table (%d entries) (ver:%d,flags:%ld)\n",
		    level, "",entries, ver, flags);
#if 0
		for (i=0;i<entries;i++)
		{
		    printf("entry#%d: dur: %ld mtime: %ld mrate: %ld\n",
			i, stream_read_dword(demuxer->stream),
			stream_read_dword(demuxer->stream),
			stream_read_dword(demuxer->stream));
		}
#endif
		break;
	    }
	    case MOV_FOURCC('c','o','d','e'):
	    {
#warning "Implement atom 'code' for FLASH support"
	    }
	    default:
		id = be2me_32(id);
		mp_msg(MSGT_DEMUX,MSGL_V,"MOV: unknown chunk: %.4s %d\n",&id,(int)len);
		break;
	  }//switch(id)
	} else { /* not in track */
	  switch(id) {
	    case MOV_FOURCC('t','r','a','k'): {
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

//  stdata[]:
//	8   short	version
//	10  short	revision
//	12  int		vendor_id
//	16  short	channels
//	18  short	samplesize
//	20  short	compression_id
//	22  short	packet_size (==0)
//	24  int		sample_rate
//    ---- qt3.0+
//	28  int		samples_per_packet
//	32  int		bytes_per_packet
//	36  int		bytes_per_frame
//	40  int		bytes_per_sample

		mp_msg(MSGT_DEMUX, MSGL_INFO, "Audio bits: %d  chans: %d\n",trak->stdata[19],trak->stdata[17]);
		mp_msg(MSGT_DEMUX, MSGL_INFO, "Fourcc: %.4s\n",&trak->fourcc);
#if 0
		{ FILE* f=fopen("stdata.dat","wb");
		  fwrite(trak->stdata,trak->stdata_len,1,f);
		  fclose(f); }
		{ FILE* f=fopen("tkdata.dat","wb");
		  fwrite(trak->tkdata,trak->tkdata_len,1,f);
		  fclose(f); }
#endif
		// Emulate WAVEFORMATEX struct:
		sh->wf=malloc(sizeof(WAVEFORMATEX));
		memset(sh->wf,0,sizeof(WAVEFORMATEX));
		sh->wf->nChannels=(trak->stdata[16]<<8)+trak->stdata[17];
		sh->wf->wBitsPerSample=(trak->stdata[18]<<8)+trak->stdata[19];
		// sh->wf->nSamplesPerSec=trak->timescale;
		sh->wf->nSamplesPerSec=(trak->stdata[24]<<8)+trak->stdata[25];
		sh->wf->nAvgBytesPerSec=sh->wf->nChannels*sh->wf->wBitsPerSample*sh->wf->nSamplesPerSec/8;
		// Selection:
		if(demuxer->audio->id==-1 || demuxer->audio->id==priv->track_db){
		    // (auto)selected audio track:
		    demuxer->audio->id=priv->track_db;
		    demuxer->audio->sh=sh; sh->ds=demuxer->audio;
		}
		break;
	    }
	    case MOV_TRAK_VIDEO: {
		int i, entry;
		int flag, start, count_flag, end, palette_count;
		int hdr_ptr = 76;  // the byte just after depth
		unsigned char *palette_map;
		sh_video_t* sh=new_sh_video(demuxer,priv->track_db);
		int depth = trak->stdata[75]|(trak->stdata[74]<<8);
		sh->format=trak->fourcc;

//  stdata[]:
//	8   short	version
//	10  short	revision
//	12  int		vendor_id
//	16  int		temporal_quality
//	20  int		spatial_quality
//	24  short	width
//	26  short	height
//	28  int		h_dpi
//	32  int		v_dpi
//	36  int		0
//	40  short	frames_per_sample
//	42  char[32]	compressor_name
//	74  short	depth
//	76  short	color_table_id

		if(!sh->fps) sh->fps=trak->timescale;
		sh->frametime=1.0f/sh->fps;
#if 0
		sh->disp_w=trak->tkdata[77]|(trak->tkdata[76]<<8);
		sh->disp_h=trak->tkdata[81]|(trak->tkdata[80]<<8);
#else
		sh->disp_w=trak->stdata[25]|(trak->stdata[24]<<8);
		sh->disp_h=trak->stdata[27]|(trak->stdata[26]<<8);
#endif		

		if(depth&(~15)) printf("*** depht = 0x%X\n",depth);

		// palettized?
		depth&=31; // flag 32 means grayscale
		if ((depth == 2) || (depth == 4) || (depth == 8))
		  palette_count = (1 << depth);
		else
		  palette_count = 0;

		// emulate BITMAPINFOHEADER:
		if (palette_count)
		{
		  sh->bih=malloc(sizeof(BITMAPINFOHEADER) + palette_count * 4);
		  memset(sh->bih,0,sizeof(BITMAPINFOHEADER) + palette_count * 4);
		  sh->bih->biSize=40 + palette_count * 4;
		  // fetch the relevant fields
		  flag = BE_16(&trak->stdata[hdr_ptr]);
		  hdr_ptr += 2;
		  start = BE_32(&trak->stdata[hdr_ptr]);
		  hdr_ptr += 4;
		  count_flag = BE_16(&trak->stdata[hdr_ptr]);
		  hdr_ptr += 2;
		  end = BE_16(&trak->stdata[hdr_ptr]);
		  hdr_ptr += 2;
		  palette_map = (unsigned char *)sh->bih + 40;

		  // load default palette
		  if (flag & 0x08)
		  {
		    mp_msg(MSGT_DEMUX, MSGL_INFO, "Using default QT palette\n");
		    if (palette_count == 4)
		      memcpy(palette_map, qt_default_palette_4, 4 * 4);
		    else if (palette_count == 16)
		      memcpy(palette_map, qt_default_palette_16, 16 * 4);
		    if (palette_count == 256)
		      memcpy(palette_map, qt_default_palette_256, 256 * 4);
		  }
		  // load palette from file
		  else
		  {
		    mp_msg(MSGT_DEMUX, MSGL_INFO, "Loading palette from file\n");
		    for (i = start; i <= end; i++)
		    {
		      entry = BE_16(&trak->stdata[hdr_ptr]);
		      hdr_ptr += 2;
		      // apparently, if count_flag is set, entry is same as i
		      if (count_flag & 0x8000)
		        entry = i;
		      // only care about top 8 bits of 16-bit R, G, or B value
		      palette_map[entry * 4 + 2] = trak->stdata[hdr_ptr + 0];
		      palette_map[entry * 4 + 1] = trak->stdata[hdr_ptr + 2];
		      palette_map[entry * 4 + 0] = trak->stdata[hdr_ptr + 4];
		      hdr_ptr += 6;
		    }
		  }
		}
		else
		{
		  sh->bih=malloc(sizeof(BITMAPINFOHEADER));
		  memset(sh->bih,0,sizeof(BITMAPINFOHEADER));
		  sh->bih->biSize=40;
		}
		sh->bih->biWidth=sh->disp_w;
		sh->bih->biHeight=sh->disp_h;
		sh->bih->biPlanes=0;
		sh->bih->biBitCount=depth;
		sh->bih->biCompression=trak->fourcc;
		sh->bih->biSizeImage=sh->bih->biWidth*sh->bih->biHeight;

		mp_msg(MSGT_DEMUX, MSGL_INFO, "Image size: %d x %d (%d bpp)\n",sh->disp_w,sh->disp_h,sh->bih->biBitCount);
		if(trak->tkdata_len>81)
		mp_msg(MSGT_DEMUX, MSGL_INFO, "Display size: %d x %d\n",
		    trak->tkdata[77]|(trak->tkdata[76]<<8),
		    trak->tkdata[81]|(trak->tkdata[80]<<8));
		mp_msg(MSGT_DEMUX, MSGL_INFO, "Fourcc: %.4s  Codec: '%.*s'\n",&trak->fourcc,trak->stdata[42]&31,trak->stdata+43);
		
		if(demuxer->video->id==-1 || demuxer->video->id==priv->track_db){
		    // (auto)selected video track:
		    demuxer->video->id=priv->track_db;
		    demuxer->video->sh=sh; sh->ds=demuxer->video;
		}
		break;
	    }
	    case MOV_TRAK_GENERIC:
		mp_msg(MSGT_DEMUX, MSGL_INFO, "Generic track - not completly understood! (id: %d)\n",
		    trak->id);
#warning "Also this contains the FLASH data"
#if 0
		mp_msg(MSGT_DEMUX, MSGL_INFO, "Extracting samples to files (possibly this is an flash anim)\n");
	    {
		int pos = stream_tell(demuxer->stream);
		int i;
		int fd;
		char name[20];
		
		for (i=0; i<trak->samples_size; i++)
		{
		    char buf[trak->samples[i].size];
		    stream_seek(demuxer->stream, trak->samples[i].pos);
		    snprintf((char *)&name[0], 20, "samp%d", i);
		    fd = open((char *)&name[0], O_CREAT|O_WRONLY);
		    stream_read(demuxer->stream, &buf[0], trak->samples[i].size);
		    write(fd, &buf[0], trak->samples[i].size);
		    close(fd);
		 }
		for (i=0; i<trak->chunks_size; i++)
		{
		    char buf[trak->length];
		    stream_seek(demuxer->stream, trak->chunks[i].pos);
		    snprintf((char *)&name[0], 20, "chunk%d", i);
		    fd = open((char *)&name[0], O_CREAT|O_WRONLY);
		    stream_read(demuxer->stream, &buf[0], trak->length);
		    write(fd, &buf[0], trak->length);
		    close(fd);
		 }
		 if (trak->samplesize > 0)
		 {
		    char *buf;
		    
		    buf = malloc(trak->samplesize);
		    stream_seek(demuxer->stream, trak->chunks[0].pos);
		    snprintf((char *)&name[0], 20, "trak%d", trak->id);
		    fd = open((char *)&name[0], O_CREAT|O_WRONLY);
		    stream_read(demuxer->stream, buf, trak->samplesize);
		    write(fd, buf, trak->samplesize);
		    close(fd);
		 }
		 stream_seek(demuxer->stream, pos);
	    }		
#endif
		break;
	    default:
		mp_msg(MSGT_DEMUX, MSGL_INFO, "Unknown track type found (type: %d)\n", trak->type);
		break;
	    }
	    mp_msg(MSGT_DEMUX, MSGL_INFO, "--------------\n");
	    priv->track_db++;
	    trak=NULL;
	    break;
	}
#ifndef HAVE_ZLIB
	case MOV_FOURCC('c','m','o','v'): {
	    mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_MOVcomprhdr);
	    return;
	}
#else
	case MOV_FOURCC('c','m','o','v'): {
//	    mp_msg(MSGT_DEMUX,MSGL_ERR,MSGTR_MOVcomprhdr);
	    lschunks(demuxer,level+1,pos+len,NULL);
	    break;
	}
	case MOV_FOURCC('d','c','o','m'): {
//	    int temp=stream_read_dword(demuxer->stream);
	    unsigned int len=be2me_32(stream_read_dword(demuxer->stream));
	    mp_msg(MSGT_DEMUX, MSGL_INFO, "Compressed header uses %.4s algo!\n",&len);
	    break;
	}
	case MOV_FOURCC('c','m','v','d'): {
//	    int temp=stream_read_dword(demuxer->stream);
	    unsigned int moov_sz=stream_read_dword(demuxer->stream);
	    unsigned int cmov_sz=len-4;
	    unsigned char* cmov_buf=malloc(cmov_sz);
	    unsigned char* moov_buf=malloc(moov_sz+16);
	    int zret;
	    z_stream zstrm;
	    stream_t* backup;

	    mp_msg(MSGT_DEMUX, MSGL_INFO, "Compressed header size: %d / %d\n",cmov_sz,moov_sz);

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
		{ mp_msg(MSGT_DEMUX, MSGL_ERR, "QT cmov: inflateInit err %d\n",zret);
		return;
		}
	      zret = inflate(&zstrm, Z_NO_FLUSH);
	      if ((zret != Z_OK) && (zret != Z_STREAM_END))
		{ mp_msg(MSGT_DEMUX, MSGL_ERR, "QT cmov inflate: ERR %d\n",zret);
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
	      if(moov_sz != zstrm.total_out)
	        mp_msg(MSGT_DEMUX, MSGL_WARN, "Warning! moov size differs cmov: %d  zlib: %d\n",moov_sz,zstrm.total_out);
	      zret = inflateEnd(&zstrm);
	      
	      backup=demuxer->stream;
	       demuxer->stream=new_memory_stream(moov_buf,moov_sz);
	       stream_skip(demuxer->stream,8);
	       lschunks(demuxer,level+1,moov_sz,NULL); // parse uncompr. 'moov'
	       //free_stream(demuxer->stream);
	      demuxer->stream=backup;
	      free(cmov_buf);
	      free(moov_buf);	    
	      break;
	}
#endif
	case MOV_FOURCC('u','d','t','a'):
	{
	    unsigned int udta_id;
	    off_t udta_len;
	    off_t udta_size = len;
	
	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "mov: user data record found\n");
	    mp_msg(MSGT_DEMUX, MSGL_INFO, "Quicktime Clip Info:\n");

	    while((len > 8) && (udta_size > 8))
	    {
		udta_len = stream_read_dword(demuxer->stream);
		udta_id = stream_read_dword(demuxer->stream);
		udta_size -= 8;
		mp_msg(MSGT_DEMUX, MSGL_DBG2, "udta_id: %.4s (len: %d)\n", &udta_id, udta_len);
		switch (udta_id)
		{
		    case MOV_FOURCC(0xa9,'c','p','y'):
		    case MOV_FOURCC(0xa9,'d','a','y'):
		    case MOV_FOURCC(0xa9,'d','i','r'):
		    /* 0xa9,'e','d','1' - '9' : edit timestamps */
		    case MOV_FOURCC(0xa9,'f','m','t'):
		    case MOV_FOURCC(0xa9,'i','n','f'):
		    case MOV_FOURCC(0xa9,'p','r','d'):
		    case MOV_FOURCC(0xa9,'p','r','f'):
		    case MOV_FOURCC(0xa9,'r','e','q'):
		    case MOV_FOURCC(0xa9,'s','r','c'):
		    case MOV_FOURCC('n','a','m','e'):
		    case MOV_FOURCC(0xa9,'n','a','m'):
		    case MOV_FOURCC(0xa9,'A','R','T'):
		    case MOV_FOURCC(0xa9,'c','m','t'):
		    case MOV_FOURCC(0xa9,'a','u','t'):
		    case MOV_FOURCC(0xa9,'s','w','r'):
		    {
			off_t text_len = stream_read_word(demuxer->stream);
			char text[text_len+2+1];
			stream_read(demuxer->stream, (char *)&text, text_len+2);
			text[text_len+2] = 0x0;
			switch(udta_id)
			{
			    case MOV_FOURCC(0xa9,'a','u','t'):
				demux_info_add(demuxer, "author", &text[2]);
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Author: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'c','p','y'):
				demux_info_add(demuxer, "copyright", &text[2]);
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Copyright: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'i','n','f'):
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Info: %s\n", &text[2]);
				break;
			    case MOV_FOURCC('n','a','m','e'):
			    case MOV_FOURCC(0xa9,'n','a','m'):
				demux_info_add(demuxer, "name", &text[2]);
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Name: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'A','R','T'):
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Artist: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'d','i','r'):
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Director: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'c','m','t'):
				demux_info_add(demuxer, "comments", &text[2]);
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Comment: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'r','e','q'):
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Requirements: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'s','w','r'):
				demux_info_add(demuxer, "encoder", &text[2]);
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Software: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'d','a','y'):
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Creation timestamp: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'f','m','t'):
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Format: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'p','r','d'):
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Producer: %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'p','r','f'):
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Performer(s): %s\n", &text[2]);
				break;
			    case MOV_FOURCC(0xa9,'s','r','c'):
				mp_msg(MSGT_DEMUX, MSGL_INFO, " Source providers: %s\n", &text[2]);
				break;
			}
			udta_size -= 4+text_len;
			break;
		    }
		    /* some other shits:    WLOC - window location,
					    LOOP - looping style,
					    SelO - play only selected frames
					    AllF - play all frames
		    */
		    case MOV_FOURCC('W','L','O','C'):
		    case MOV_FOURCC('L','O','O','P'):
		    case MOV_FOURCC('S','e','l','O'):
		    case MOV_FOURCC('A','l','l','F'):
		    default:
		    {
			char dump[udta_len-4];
			stream_read(demuxer->stream, (char *)&dump, udta_len-4-4);
			udta_size -= udta_len;
		    }
		}
	    }
	    break;
	} /* eof udta */
	default:
	  id = be2me_32(id);
	  mp_msg(MSGT_DEMUX,MSGL_V,"MOV: unknown chunk: %.4s %d\n",&id,(int)len);
	} /* endof switch */
	} /* endof else */

	pos+=len+8;
	if(pos>=endpos) break;
	if(!stream_seek(demuxer->stream,pos)) break;
    }
}

int mov_read_header(demuxer_t* demuxer){
    mov_priv_t* priv=demuxer->priv;
    
    mp_msg(MSGT_DEMUX, MSGL_DBG3, "mov_read_header!\n");

    // Parse header:    
    stream_reset(demuxer->stream);
    if(!stream_seek(demuxer->stream,priv->moov_start)) return 0; // ???
    lschunks(demuxer, 0, priv->moov_end, NULL);

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

if(trak->samplesize){
    // read chunk:
    int x;
    if(trak->pos>=trak->chunks_size) return 0; // EOF
    stream_seek(demuxer->stream,trak->chunks[trak->pos].pos);
    pts=(float)(trak->chunks[trak->pos].sample*trak->duration)/(float)trak->timescale;
    if(trak->samplesize!=1)
    {
	mp_msg(MSGT_DEMUX, MSGL_DBG2, "WARNING! Samplesize(%d) != 1\n",
	    trak->samplesize);
	x=trak->chunks[trak->pos].size*trak->samplesize;
    }
    else
	x=trak->chunks[trak->pos].size;
//    printf("X = %d\n", x);
    /* the following stuff is audio related */
    if (trak->type == MOV_TRAK_AUDIO)
    {
    if(trak->stdata_len>=36 && trak->stdata[30] && trak->stdata[31]){
	// extended stsd header - works for CBR MP3:
	x/=(trak->stdata[30]<<8)+trak->stdata[31];  // samples/packet
	// x*=(trak->stdata[34]<<8)+trak->stdata[35];  // bytes/packet
	x*=(trak->stdata[38]<<8)+trak->stdata[39];  // bytes/frame
    } else {
	// works for ima4:   -- we should find this info in mov headers!
	if(ds->ss_div!=1 || ds->ss_mul!=1){
	    x/=ds->ss_div; x*=ds->ss_mul; // compression ratio fix  ! HACK !
	} else {
	    x*=(trak->stdata[18]<<8)+trak->stdata[19];x/=8;  // bits/sample
	}
    }
    } /* MOV_TRAK_AUDIO */
    ds_read_packet(ds,demuxer->stream,x,pts,trak->chunks[trak->pos].pos,0);
    if(ds==demuxer->audio) mp_msg(MSGT_DEMUX, MSGL_DBG2, "sample %d bytes pts %5.3f\n",trak->chunks[trak->pos].size*trak->samplesize,pts);
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
    unsigned int ipts;
    if(!(flags&1)) pts+=trak->samples[trak->pos].pts;
    if(pts<0) pts=0;
    ipts=pts;
    //printf("MOV track seek - sample: %d  \n",ipts);
    for(trak->pos=0;trak->pos<trak->samples_size;++trak->pos){
	if(trak->samples[trak->pos].pts>=ipts) break; // found it!
    }
    if(trak->keyframes_size){
	// find nearest keyframe
	int i;
	for(i=0;i<trak->keyframes_size;i++){
	    if(trak->keyframes[i]>=trak->pos) break;
	}
	if(i>0 && (trak->keyframes[i]-trak->pos) > (trak->pos-trak->keyframes[i-1]))
	  --i;
	trak->pos=trak->keyframes[i];
//	printf("nearest keyframe: %d  \n",trak->pos);
    }
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

