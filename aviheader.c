
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
#include "bswap.h"
#include "stheader.h"
#include "aviheader.h"

#define MIN(a,b) (((a)<(b))?(a):(b))


static MainAVIHeader avih;

extern void print_avih(MainAVIHeader *h);
extern void print_avih_flags(MainAVIHeader *h);
extern void print_strh(AVIStreamHeader *h);
extern void print_wave_header(WAVEFORMATEX *h);
extern void print_video_header(BITMAPINFOHEADER *h);
extern void print_index(AVIINDEXENTRY *idx,int idx_size);

void read_avi_header(demuxer_t *demuxer,int index_mode){
sh_audio_t *sh_audio=NULL;
sh_video_t *sh_video=NULL;
int stream_id=-1;
int idxfix_videostream=0;
int idxfix_divx=0;
avi_priv_t* priv=demuxer->priv;

//---- AVI header:
priv->idx_size=0;
while(1){
  int id=stream_read_dword_le(demuxer->stream);
  int chunksize,size2;
  static int last_fccType=0;
  char* hdr=NULL;
  //
  if(stream_eof(demuxer->stream)) break;
  //
  if(id==mmioFOURCC('L','I','S','T')){
    int len=stream_read_dword_le(demuxer->stream)-4; // list size
    id=stream_read_dword_le(demuxer->stream);        // list type
    mp_msg(MSGT_HEADER,MSGL_DBG2,"LIST %.4s  len=%d\n",(char *) &id,len);
    if(id==listtypeAVIMOVIE){
      // found MOVI header
      demuxer->movi_start=stream_tell(demuxer->stream);
      demuxer->movi_end=demuxer->movi_start+len;
      mp_msg(MSGT_HEADER,MSGL_V,"Found movie at 0x%X - 0x%X\n",(int)demuxer->movi_start,(int)demuxer->movi_end);
      if(index_mode==-2) break; // reading from non-seekable source (stdin)
      len=(len+1)&(~1);
      stream_skip(demuxer->stream,len);
    }
    continue;
  }
  size2=stream_read_dword_le(demuxer->stream);
  mp_msg(MSGT_HEADER,MSGL_DBG2,"CHUNK %.4s  len=%d\n",(char *) &id,size2);
  chunksize=(size2+1)&(~1);
  switch(id){
    case mmioFOURCC('I','S','F','T'): hdr="Software";break;
    case mmioFOURCC('I','N','A','M'): hdr="Name";break;
    case mmioFOURCC('I','S','B','J'): hdr="Title";break;
    case mmioFOURCC('I','A','R','T'): hdr="Author";break;
    case mmioFOURCC('I','C','O','P'): hdr="Copyright";break;
    case mmioFOURCC('I','C','M','T'): hdr="Comment";break;
    case ckidAVIMAINHDR:          // read 'avih'
      stream_read(demuxer->stream,(char*) &avih,MIN(size2,sizeof(avih)));
      le2me_MainAVIHeader(&avih); // swap to machine endian
      chunksize-=MIN(size2,sizeof(avih));
      if(verbose) print_avih(&avih); else print_avih_flags(&avih);
      break;
    case ckidSTREAMHEADER: {      // read 'strh'
      AVIStreamHeader h;
      stream_read(demuxer->stream,(char*) &h,MIN(size2,sizeof(h)));
      le2me_AVIStreamHeader(&h);  // swap to machine endian
      chunksize-=MIN(size2,sizeof(h));
      ++stream_id;
      if(h.fccType==streamtypeVIDEO){
        sh_video=new_sh_video(demuxer,stream_id);
        memcpy(&sh_video->video,&h,sizeof(h));
      } else
      if(h.fccType==streamtypeAUDIO){
        sh_audio=new_sh_audio(demuxer,stream_id);
        memcpy(&sh_audio->audio,&h,sizeof(h));
      }
      last_fccType=h.fccType;
      if(verbose>=1) print_strh(&h);
      break; }
    case ckidSTREAMFORMAT: {      // read 'strf'
      if(last_fccType==streamtypeVIDEO){
        sh_video->bih=calloc((chunksize<sizeof(BITMAPINFOHEADER))?sizeof(BITMAPINFOHEADER):chunksize,1);
//        sh_video->bih=malloc(chunksize); memset(sh_video->bih,0,chunksize);
        mp_msg(MSGT_HEADER,MSGL_V,"found 'bih', %d bytes of %d\n",chunksize,sizeof(BITMAPINFOHEADER));
        stream_read(demuxer->stream,(char*) sh_video->bih,chunksize);
	le2me_BITMAPINFOHEADER(sh_video->bih);  // swap to machine endian
        if(verbose>=1) print_video_header(sh_video->bih);
        chunksize=0;
//        sh_video->fps=(float)sh_video->video.dwRate/(float)sh_video->video.dwScale;
//        sh_video->frametime=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
//        if(demuxer->video->id==-1) demuxer->video->id=stream_id;
        // IdxFix:
        idxfix_videostream=stream_id;
        switch(sh_video->bih->biCompression){
	case mmioFOURCC('D', 'I', 'V', '3'):
	case mmioFOURCC('d', 'i', 'v', '3'):
	case mmioFOURCC('D', 'I', 'V', '4'):
        case mmioFOURCC('d', 'i', 'v', '4'):
	case mmioFOURCC('D', 'I', 'V', '5'):
	case mmioFOURCC('d', 'i', 'v', '5'):
	case mmioFOURCC('D', 'I', 'V', '6'):
        case mmioFOURCC('d', 'i', 'v', '6'):
	case mmioFOURCC('M', 'P', '4', '3'):
	case mmioFOURCC('m', 'p', '4', '3'):
	case mmioFOURCC('M', 'P', '4', '2'):
	case mmioFOURCC('m', 'p', '4', '2'):
	case mmioFOURCC('D', 'I', 'V', '2'):
        case mmioFOURCC('A', 'P', '4', '1'):
          idxfix_divx=1; // we can fix keyframes only for divx coded files!
        }
      } else
      if(last_fccType==streamtypeAUDIO){
	int wf_size = chunksize<sizeof(WAVEFORMATEX)?sizeof(WAVEFORMATEX):chunksize;
        sh_audio->wf=calloc(wf_size,1);
//        sh_audio->wf=malloc(chunksize); memset(sh_audio->wf,0,chunksize);
        mp_msg(MSGT_HEADER,MSGL_V,"found 'wf', %d bytes of %d\n",chunksize,sizeof(WAVEFORMATEX));
        stream_read(demuxer->stream,(char*) sh_audio->wf,chunksize);
	le2me_WAVEFORMATEX(sh_audio->wf);
	if (sh_audio->wf->cbSize != 0 &&
	    wf_size < sizeof(WAVEFORMATEX)+sh_audio->wf->cbSize) {
	    sh_audio->wf=realloc(sh_audio->wf, sizeof(WAVEFORMATEX)+sh_audio->wf->cbSize);
	}
        chunksize=0;
        if(verbose>=1) print_wave_header(sh_audio->wf);
//        if(demuxer->audio->id==-1) demuxer->audio->id=stream_id;
      }
      break;
    }
    case ckidAVINEWINDEX: if(index_mode){
      int i;
      priv->idx_size=size2>>4;
      mp_msg(MSGT_HEADER,MSGL_V,"Reading INDEX block, %d chunks for %ld frames\n",
        priv->idx_size,avih.dwTotalFrames);
      priv->idx=malloc(priv->idx_size<<4);
      stream_read(demuxer->stream,(char*)priv->idx,priv->idx_size<<4);
      for (i = 0; i < priv->idx_size; i++)	// swap index to machine endian
	le2me_AVIINDEXENTRY((AVIINDEXENTRY*)priv->idx + i);
      chunksize-=priv->idx_size<<4;
      if(verbose>=2) print_index(priv->idx,priv->idx_size);
      break;
    }
  }
  if(hdr){
      char buf[256];
      int len=(size2<250)?size2:250;
      stream_read(demuxer->stream,buf,len);
      chunksize-=len;
      buf[len]=0;
      mp_msg(MSGT_HEADER,MSGL_V,"%-10s: %s\n",hdr,buf);
  }
  if(chunksize>0) stream_skip(demuxer->stream,chunksize); else
  if(chunksize<0) mp_msg(MSGT_HEADER,MSGL_WARN,"chunksize=%d  (id=%.4s)\n",chunksize,(char *) &id);
  
}

if(index_mode>=2 || (priv->idx_size==0 && index_mode==1)){
  // build index for file:
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream,demuxer->movi_start);
  
  priv->idx_pos=0;
  priv->idx_size=0;
  priv->idx=NULL;

  while(1){
    int id,len,skip;
    AVIINDEXENTRY* idx;
    unsigned char c;
    demuxer->filepos=stream_tell(demuxer->stream);
    if(demuxer->filepos>=demuxer->movi_end) break;
    id=stream_read_dword_le(demuxer->stream);
    len=stream_read_dword_le(demuxer->stream);
    if(id==mmioFOURCC('L','I','S','T')){
      id=stream_read_dword_le(demuxer->stream);      // list type
      continue;
    }
    if(stream_eof(demuxer->stream)) break;
    if(!id || avi_stream_id(id)==100) goto skip_chunk; // bad ID (or padding?)

    if(priv->idx_pos>=priv->idx_size){
//      priv->idx_size+=32;
      priv->idx_size+=1024; // +16kB
      priv->idx=realloc(priv->idx,priv->idx_size*sizeof(AVIINDEXENTRY));
      if(!priv->idx){priv->idx_pos=0; break;} // error!
    }
    idx=&((AVIINDEXENTRY *)priv->idx)[priv->idx_pos++];
    idx->ckid=id;
    idx->dwFlags=AVIIF_KEYFRAME; // FIXME
    idx->dwChunkOffset=demuxer->filepos;
    idx->dwChunkLength=len;
    
    c=stream_read_char(demuxer->stream);

    // Fix keyframes for DivX files:
    if(idxfix_divx)
      if(avi_stream_id(id)==idxfix_videostream){
        if(c&0x40) idx->dwFlags=0;
      }
    
    mp_dbg(MSGT_HEADER,MSGL_DBG2,"%08X %08X %.4s %02X %X\n",demuxer->filepos,id,(char *) &id,c,(unsigned int) idx->dwFlags);
#if 0
    { unsigned char tmp[64];
      int i;
      stream_read(demuxer->stream,tmp,64);
      printf("%.4s",&id);
      for(i=0;i<64;i++) printf(" %02X",tmp[i]);
      printf("\n");
    }
#endif
skip_chunk:
    skip=(len+1)&(~1); // total bytes in this chunk
    stream_seek(demuxer->stream,8+demuxer->filepos+skip);
  }
  priv->idx_size=priv->idx_pos;
  mp_msg(MSGT_HEADER,MSGL_INFO,"AVI: Generated index table for %d chunks!\n",priv->idx_size);
}

}

#undef MIN


