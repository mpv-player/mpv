
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "bswap.h"
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
off_t list_end=0;

//---- AVI header:
priv->idx_size=0;
priv->audio_streams=0;
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
    list_end=stream_tell(demuxer->stream)+((len+1)&(~1));
    mp_msg(MSGT_HEADER,MSGL_V,"list_end=0x%X\n",(int)list_end);
    if(id==listtypeAVIMOVIE){
      // found MOVI header
      if(!demuxer->movi_start) demuxer->movi_start=stream_tell(demuxer->stream);
      demuxer->movi_end=demuxer->movi_start+len;
      mp_msg(MSGT_HEADER,MSGL_V,"Found movie at 0x%X - 0x%X\n",(int)demuxer->movi_start,(int)demuxer->movi_end);
      if(demuxer->stream->end_pos) demuxer->movi_end=demuxer->stream->end_pos;
      if(index_mode==-2 || index_mode==2 || index_mode==0)
        break; // reading from non-seekable source (stdin) or forced index or no index forced
      len=(len+1)&(~1);
      stream_skip(demuxer->stream,len);
    }
    continue;
  }
  size2=stream_read_dword_le(demuxer->stream);
  mp_msg(MSGT_HEADER,MSGL_DBG2,"CHUNK %.4s  len=%d\n",(char *) &id,size2);
  chunksize=(size2+1)&(~1);
  switch(id){

    // Indicates where the subject of the file is archived
    case mmioFOURCC('I','A','R','L'): hdr="Archival Location";break;
    // Lists the artist of the original subject of the file;
    // for example, "Michaelangelo."
    case mmioFOURCC('I','A','R','T'): hdr="Artist";break;
    // Lists the name of the person or organization that commissioned
    // the subject of the file; for example "Pope Julian II."
    case mmioFOURCC('I','C','M','S'): hdr="Commissioned";break;
    // Provides general comments about the file or the subject
    // of the file. If the comment is several sentences long, end each
    // sentence with a period. Do not include new-line characters.
    case mmioFOURCC('I','C','M','T'): hdr="Comments";break;
    // Records the copyright information for the file; for example,
    // "Copyright Encyclopedia International 1991." If there are multiple
    // copyrights, separate them by semicolon followed by a space.
    case mmioFOURCC('I','C','O','P'): hdr="Copyright";break;
    // Describes whether an image has been cropped and, if so, how it
    // was cropped; for example, "lower-right corner."
    case mmioFOURCC('I','C','R','D'): hdr="Creation Date";break;
    // Describes whether an image has been cropped and, if so, how it
    // was cropped; for example, "lower-right corner."
    case mmioFOURCC('I','C','R','P'): hdr="Cropped";break;
    // Specifies the size of the original subject of the file; for
    // example, "8.5 in h, 11 in w."
    case mmioFOURCC('I','D','I','M'): hdr="Dimensions";break;
    // Stores dots per inch setting of the digitizer used to
    // produce the file, such as "300."
    case mmioFOURCC('I','D','P','I'): hdr="Dots Per Inch";break;
    // Stores the of the engineer who worked on the file. If there are
    // multiple engineers, separate the names by a semicolon and a blank;
    // for example, "Smith, John; Adams, Joe."
    case mmioFOURCC('I','E','N','G'): hdr="Engineer";break;
    // Describes the original work, such as "landscape,", "portrait,"
    // "still liefe," etc.
    case mmioFOURCC('I','G','N','R'): hdr="Genre";break;
    // Provides a list of keywords that refer to the file or subject of the
    // file. Separate multiple keywords with a semicolon and a blank;
    // for example, "Seattle, aerial view; scenery."
    case mmioFOURCC('I','K','E','Y'): hdr="Keywords";break;
    // ILGT - Describes the changes in the lightness settings on the digitizer
    // required to produce the file. Note that the format of this information
    // depends on the hardware used.
    case mmioFOURCC('I','L','G','T'): hdr="Lightness";break;
    // IMED - Decribes the original subject of the file, such as
    // "computer image," "drawing," "lithograph," and so on.
    case mmioFOURCC('I','M','E','D'): hdr="Medium";break;
    // INAM - Stores the title of the subject of the file, such as
    // "Seattle from Above."
    case mmioFOURCC('I','N','A','M'): hdr="Name";break;
    // IPLT - Specifies the number of colors requested when digitizing
    // an image, such as "256."
    case mmioFOURCC('I','P','L','T'): hdr="Palette Setting";break;
    // IPRD - Specifies the name of title the file was originally intended
    // for, such as "Encyclopedia of Pacific Northwest Geography."
    case mmioFOURCC('I','P','R','D'): hdr="Product";break;
    // ISBJ - Decsribes the contents of the file, such as
    // "Aerial view of Seattle."
    case mmioFOURCC('I','S','B','J'): hdr="Subject";break;
    // ISFT - Identifies the name of the software packages used to create the
    // file, such as "Microsoft WaveEdit"
    case mmioFOURCC('I','S','F','T'): hdr="Software";break;
    // ISHP - Identifies the change in sharpness for the digitizer
    // required to produce the file (the format depends on the hardware used).
    case mmioFOURCC('I','S','H','P'): hdr="Sharpness";break;
    // ISRC - Identifies the name of the person or organization who
    // suplied the original subject of the file; for example, "Try Research."
    case mmioFOURCC('I','S','R','C'): hdr="Source";break;
    // ISRF - Identifies the original form of the material that was digitized,
    // such as "slide," "paper," "map," and so on. This is not necessarily
    // the same as IMED
    case mmioFOURCC('I','S','R','F'): hdr="Source Form";break;
    // ITCH - Identifies the technician who digitized the subject file;
    // for example, "Smith, John."
    case mmioFOURCC('I','T','C','H'): hdr="Technician";break;
    case mmioFOURCC('I','S','M','P'): hdr="Time Code";break;
    case mmioFOURCC('I','D','I','T'): hdr="Digitization Time";break;

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
	// fixup MS-RLE header (seems to be broken for <256 color files)
	if(sh_video->bih->biCompression==1 && sh_video->bih->biSize==40)
	    sh_video->bih->biSize=chunksize;
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
	  mp_msg(MSGT_HEADER,MSGL_V,"Regenerating keyframe table for DIVX 3 video\n");
	  break;
        case mmioFOURCC('D', 'I', 'V', 'X'):
        case mmioFOURCC('d', 'i', 'v', 'x'):
          idxfix_divx=2; // we can fix keyframes only for divx coded files!
	  mp_msg(MSGT_HEADER,MSGL_V,"Regenerating keyframe table for DIVX 4 video\n");
	  break;
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
	++priv->audio_streams;
//        if(demuxer->audio->id==-1) demuxer->audio->id=stream_id;
      }
      break;
    }
    case ckidAVINEWINDEX:
    if(demuxer->movi_end>stream_tell(demuxer->stream))
	demuxer->movi_end=stream_tell(demuxer->stream); // fixup movi-end
    if(index_mode){
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
    mp_msg(MSGT_HEADER,MSGL_V,"hdr=%s  size=%d\n",hdr,size2);
    if(size2==3)
      chunksize=1; // empty
    else {
      char buf[256];
      int len=(size2<250)?size2:250;
      stream_read(demuxer->stream,buf,len);
      chunksize-=len;
      buf[len]=0;
      mp_msg(MSGT_HEADER,MSGL_V,"%-10s: %s\n",hdr,buf);
      demux_info_add(demuxer, hdr, buf);
    }
  }
  mp_msg(MSGT_HEADER,MSGL_DBG2,"list_end=0x%X  pos=0x%X  chunksize=0x%X  next=0x%X\n",
      (int)list_end, (int)stream_tell(demuxer->stream),
      chunksize, (int)chunksize+stream_tell(demuxer->stream));
  if(list_end>0 && chunksize+stream_tell(demuxer->stream)>list_end){
      mp_msg(MSGT_HEADER,MSGL_V,"Broken chunk?  chunksize=%d  (id=%.4s)\n",chunksize,(char *) &id);
      stream_seek(demuxer->stream,list_end);
      list_end=0;
  } else
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
    unsigned int c;
    demuxer->filepos=stream_tell(demuxer->stream);
    if(demuxer->filepos>=demuxer->movi_end && demuxer->movi_start<demuxer->movi_end) break;
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
    
    c=stream_read_dword(demuxer->stream);

    // Fix keyframes for DivX files:
    if(idxfix_divx)
      if(avi_stream_id(id)==idxfix_videostream){
        switch(idxfix_divx){
    	    case 1: if(c&0x40000000) idx->dwFlags=0;break; // divx 3
	    case 2: if(c==0x1B6) idx->dwFlags=0;break; // divx 4
	}
      }

    // update status line:
    { static int lastpos;
      int pos;
      off_t len=demuxer->movi_end-demuxer->movi_start;
      if(len){
          pos=100*(demuxer->filepos-demuxer->movi_start)/len; // %
      } else {
          pos=(demuxer->filepos-demuxer->movi_start)>>20; // MB
      }
      if(pos!=lastpos){
          lastpos=pos;
	  mp_msg(MSGT_HEADER,MSGL_STATUS,"Generating Index: %3d %s     \r",
	      pos, len?"%":"MB");
      }
    }
    mp_dbg(MSGT_HEADER,MSGL_DBG2,"%08X %08X %.4s %08X %X\n",(int)demuxer->filepos,id,(char *) &id,(int)c,(unsigned int) idx->dwFlags);
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


