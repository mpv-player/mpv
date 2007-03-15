
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "aviheader.h"
#include "libavutil/common.h"

static MainAVIHeader avih;

extern void print_avih(MainAVIHeader *h, int verbose_level);
extern void print_avih_flags(MainAVIHeader *h, int verbose_level);
extern void print_strh(AVIStreamHeader *h, int verbose_level);
extern void print_wave_header(WAVEFORMATEX *h, int verbose_level);
extern void print_video_header(BITMAPINFOHEADER *h, int verbose_level);
extern void print_index(AVIINDEXENTRY *idx,int idx_size, int verbose_level);
extern void print_avistdindex_chunk(avistdindex_chunk *h, int verbose_level);
extern void print_avisuperindex_chunk(avisuperindex_chunk *h, int verbose_level);
extern void print_vprp(VideoPropHeader *vprp, int verbose_level);

static int odml_get_vstream_id(int id, unsigned char res[])
{
    unsigned char *p = (unsigned char *)&id;
    id = le2me_32(id);

    if (p[2] == 'd') {
	if (res) {
	    res[0] = p[0];
	    res[1] = p[1];
	}
	return 1;
    }
    return 0;
}

int avi_idx_cmp(const void *elem1,const void *elem2) {
  register off_t a = AVI_IDX_OFFSET((AVIINDEXENTRY *)elem1);
  register off_t b = AVI_IDX_OFFSET((AVIINDEXENTRY *)elem2);
  return (a > b) - (b > a);
}

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
  unsigned chunksize,size2;
  static int last_fccType=0;
  char* hdr=NULL;
  //
  if(stream_eof(demuxer->stream)) break;
  // Imply -forceidx if -saveidx is specified
  if (index_file_save)
    index_mode = 2;
  //
  if(id==mmioFOURCC('L','I','S','T')){
    unsigned len=stream_read_dword_le(demuxer->stream);   // list size
    id=stream_read_dword_le(demuxer->stream);             // list type
    mp_msg(MSGT_HEADER,MSGL_DBG2,"LIST %.4s  len=%u\n",(char *) &id,len);
    if(len >= 4) {
	len -= 4;
	list_end=stream_tell(demuxer->stream)+((len+1)&(~1));
    } else {
	mp_msg(MSGT_HEADER,MSGL_WARN,MSGTR_MPDEMUX_AVIHDR_EmptyList);
	list_end = 0;
    }
    mp_msg(MSGT_HEADER,MSGL_V,"list_end=0x%X\n",(int)list_end);
    if(id==listtypeAVIMOVIE){
      // found MOVI header
      if(!demuxer->movi_start) demuxer->movi_start=stream_tell(demuxer->stream);
      demuxer->movi_end=stream_tell(demuxer->stream)+len;
      mp_msg(MSGT_HEADER,MSGL_V,MSGTR_MPDEMUX_AVIHDR_FoundMovieAt,(int)demuxer->movi_start,(int)demuxer->movi_end);
      if(demuxer->stream->end_pos>demuxer->movi_end) demuxer->movi_end=demuxer->stream->end_pos;
      if(index_mode==-2 || index_mode==2 || index_mode==0)
        break; // reading from non-seekable source (stdin) or forced index or no index forced
      if(list_end>0) stream_seek(demuxer->stream,list_end); // skip movi
      list_end=0;
    }
    continue;
  }
  size2=stream_read_dword_le(demuxer->stream);
  mp_msg(MSGT_HEADER,MSGL_DBG2,"CHUNK %.4s  len=%u\n",(char *) &id,size2);
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
      stream_read(demuxer->stream,(char*) &avih,FFMIN(size2,sizeof(avih)));
      le2me_MainAVIHeader(&avih); // swap to machine endian
      chunksize-=FFMIN(size2,sizeof(avih));
      if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_avih(&avih,MSGL_V); // else print_avih_flags(&avih,MSGL_V);
      break;
    case ckidSTREAMHEADER: {      // read 'strh'
      AVIStreamHeader h;
      stream_read(demuxer->stream,(char*) &h,FFMIN(size2,sizeof(h)));
      le2me_AVIStreamHeader(&h);  // swap to machine endian
      chunksize-=FFMIN(size2,sizeof(h));
      ++stream_id;
      if(h.fccType==streamtypeVIDEO){
        sh_video=new_sh_video(demuxer,stream_id);
        memcpy(&sh_video->video,&h,sizeof(h));
        sh_video->stream_delay = (float)sh_video->video.dwStart * sh_video->video.dwScale/sh_video->video.dwRate;
      } else
      if(h.fccType==streamtypeAUDIO){
        sh_audio=new_sh_audio(demuxer,stream_id);
        memcpy(&sh_audio->audio,&h,sizeof(h));
        sh_audio->stream_delay = (float)sh_audio->audio.dwStart * sh_audio->audio.dwScale/sh_audio->audio.dwRate;
      }
      last_fccType=h.fccType;
      if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_strh(&h,MSGL_V);
      break; }
    case mmioFOURCC('i', 'n', 'd', 'x'): {
      uint32_t i;
      avisuperindex_chunk *s;

      if(!index_mode) break;

      if(chunksize<=24){
        break;
      }
      priv->suidx_size++;
      priv->suidx = realloc_struct(priv->suidx, priv->suidx_size, sizeof (avisuperindex_chunk));
      s = &priv->suidx[priv->suidx_size-1];

      chunksize-=24;
      memcpy(s->fcc, "indx", 4);
      s->dwSize = size2;
      s->wLongsPerEntry = stream_read_word_le(demuxer->stream);
      s->bIndexSubType = stream_read_char(demuxer->stream);
      s->bIndexType = stream_read_char(demuxer->stream);
      s->nEntriesInUse = stream_read_dword_le(demuxer->stream);
      *(uint32_t *)s->dwChunkId = stream_read_dword_le(demuxer->stream);
      stream_read(demuxer->stream, (char *)s->dwReserved, 3*4);
      memset(s->dwReserved, 0, 3*4);
	  
      print_avisuperindex_chunk(s,MSGL_V);
      
      if( ((chunksize/4)/s->wLongsPerEntry) < s->nEntriesInUse){
        mp_msg (MSGT_HEADER, MSGL_WARN, "Broken super index chunk\n");
        s->nEntriesInUse = (chunksize/4)/s->wLongsPerEntry;
      }

      // Check and fix this useless crap
      if(s->wLongsPerEntry != sizeof (avisuperindex_entry)/4) {
          mp_msg (MSGT_HEADER, MSGL_WARN, "Broken super index chunk size: %u\n",s->wLongsPerEntry);
          s->wLongsPerEntry = sizeof(avisuperindex_entry)/4;
      }
      s->aIndex = calloc(s->nEntriesInUse, sizeof (avisuperindex_entry));
      s->stdidx = calloc(s->nEntriesInUse, sizeof (avistdindex_chunk));

      // now the real index of indices
      for (i=0; i<s->nEntriesInUse; i++) {
	  chunksize-=16;
	  s->aIndex[i].qwOffset = stream_read_dword_le(demuxer->stream) & 0xffffffff;
	  s->aIndex[i].qwOffset |= ((uint64_t)stream_read_dword_le(demuxer->stream) & 0xffffffff)<<32;
	  s->aIndex[i].dwSize = stream_read_dword_le(demuxer->stream);
	  s->aIndex[i].dwDuration = stream_read_dword_le(demuxer->stream);
	  mp_msg (MSGT_HEADER, MSGL_V, "ODML (%.4s): [%d] 0x%016"PRIx64" 0x%04x %u\n", 
		  (s->dwChunkId), i,
		  (uint64_t)s->aIndex[i].qwOffset, s->aIndex[i].dwSize, s->aIndex[i].dwDuration);
      }

      break; }
    case ckidSTREAMFORMAT: {      // read 'strf'
      if(last_fccType==streamtypeVIDEO){
        sh_video->bih=calloc((chunksize<sizeof(BITMAPINFOHEADER))?sizeof(BITMAPINFOHEADER):chunksize,1);
//        sh_video->bih=malloc(chunksize); memset(sh_video->bih,0,chunksize);
        mp_msg(MSGT_HEADER,MSGL_V,MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader,chunksize,sizeof(BITMAPINFOHEADER));
        stream_read(demuxer->stream,(char*) sh_video->bih,chunksize);
	le2me_BITMAPINFOHEADER(sh_video->bih);  // swap to machine endian
	if (sh_video->bih->biSize > chunksize && sh_video->bih->biSize > sizeof(BITMAPINFOHEADER))
		sh_video->bih->biSize = chunksize;
	// fixup MS-RLE header (seems to be broken for <256 color files)
	if(sh_video->bih->biCompression<=1 && sh_video->bih->biSize==40)
	    sh_video->bih->biSize=chunksize;
        if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_video_header(sh_video->bih,MSGL_V);
        chunksize=0;
        sh_video->fps=(float)sh_video->video.dwRate/(float)sh_video->video.dwScale;
        sh_video->frametime=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
//        if(demuxer->video->id==-1) demuxer->video->id=stream_id;
        // IdxFix:
        idxfix_videostream=stream_id;
        switch(sh_video->bih->biCompression){
	case mmioFOURCC('M', 'P', 'G', '4'):
	case mmioFOURCC('m', 'p', 'g', '4'):
	case mmioFOURCC('D', 'I', 'V', '1'):
          idxfix_divx=3; // set index recovery mpeg4 flavour: msmpeg4v1
	  mp_msg(MSGT_HEADER,MSGL_V,MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1);
	  break;
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
          idxfix_divx=1; // set index recovery mpeg4 flavour: msmpeg4v3
	  mp_msg(MSGT_HEADER,MSGL_V,MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3);
	  break;
        case mmioFOURCC('D', 'I', 'V', 'X'):
        case mmioFOURCC('d', 'i', 'v', 'x'):
        case mmioFOURCC('D', 'X', '5', '0'):
        case mmioFOURCC('X', 'V', 'I', 'D'):
        case mmioFOURCC('x', 'v', 'i', 'd'):
        case mmioFOURCC('F', 'M', 'P', '4'):
        case mmioFOURCC('f', 'm', 'p', '4'):
          idxfix_divx=2; // set index recovery mpeg4 flavour: generic mpeg4
	  mp_msg(MSGT_HEADER,MSGL_V,MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4);
	  break;
        }
      } else
      if(last_fccType==streamtypeAUDIO){
	unsigned wf_size = chunksize<sizeof(WAVEFORMATEX)?sizeof(WAVEFORMATEX):chunksize;
        sh_audio->wf=calloc(wf_size,1);
//        sh_audio->wf=malloc(chunksize); memset(sh_audio->wf,0,chunksize);
        mp_msg(MSGT_HEADER,MSGL_V,MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt,chunksize,sizeof(WAVEFORMATEX));
        stream_read(demuxer->stream,(char*) sh_audio->wf,chunksize);
	le2me_WAVEFORMATEX(sh_audio->wf);
	if (sh_audio->wf->cbSize != 0 &&
	    wf_size < sizeof(WAVEFORMATEX)+sh_audio->wf->cbSize) {
	    sh_audio->wf=realloc(sh_audio->wf, sizeof(WAVEFORMATEX)+sh_audio->wf->cbSize);
	}
	sh_audio->format=sh_audio->wf->wFormatTag;
	sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
        chunksize=0;
        if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_wave_header(sh_audio->wf,MSGL_V);
	++priv->audio_streams;
//        if(demuxer->audio->id==-1) demuxer->audio->id=stream_id;
      }
      break;
    }
    case mmioFOURCC('v', 'p', 'r', 'p'): {
	VideoPropHeader *vprp = malloc(chunksize);
	unsigned int i;
	stream_read(demuxer->stream, (void*)vprp, chunksize);
	le2me_VideoPropHeader(vprp);
	chunksize -= sizeof(*vprp)-sizeof(vprp->FieldInfo);
	chunksize /= sizeof(VIDEO_FIELD_DESC);
	if (vprp->nbFieldPerFrame > chunksize) {
	    vprp->nbFieldPerFrame = chunksize;
	}
	chunksize = 0;
	for (i=0; i<vprp->nbFieldPerFrame; i++) {
		le2me_VIDEO_FIELD_DESC(&vprp->FieldInfo[i]);
	}
	if (sh_video) {
		sh_video->aspect = GET_AVI_ASPECT(vprp->dwFrameAspectRatio);
	}
	if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_vprp(vprp,MSGL_V);
	free(vprp);
	break;
    }
    case mmioFOURCC('d', 'm', 'l', 'h'): {
	// dmlh 00 00 00 04 frms
	unsigned int total_frames = stream_read_dword_le(demuxer->stream);
	mp_msg(MSGT_HEADER,MSGL_V,MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header, chunksize, total_frames);
	stream_skip(demuxer->stream, chunksize-4);
	chunksize = 0;
    }
    break;
    case ckidAVINEWINDEX:
    if(demuxer->movi_end>stream_tell(demuxer->stream))
	demuxer->movi_end=stream_tell(demuxer->stream); // fixup movi-end
    if(index_mode && !priv->isodml){
      int i;
      priv->idx_size=size2>>4;
      mp_msg(MSGT_HEADER,MSGL_V,MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames,
        priv->idx_size,avih.dwTotalFrames, (int64_t)stream_tell(demuxer->stream));
      priv->idx=malloc(priv->idx_size<<4);
//      printf("\nindex to %p !!!!! (priv=%p)\n",priv->idx,priv);
      stream_read(demuxer->stream,(char*)priv->idx,priv->idx_size<<4);
      for (i = 0; i < priv->idx_size; i++) {	// swap index to machine endian
	AVIINDEXENTRY *entry=(AVIINDEXENTRY*)priv->idx + i;
	le2me_AVIINDEXENTRY(entry);
	/*
	 * We (ab)use the upper word for bits 32-47 of the offset, so
	 * we'll clear them here.
	 * FIXME: AFAIK no codec uses them, but if one does it will break
	 */
	entry->dwFlags&=0xffff;
      }
      chunksize-=priv->idx_size<<4;
      if( mp_msg_test(MSGT_HEADER,MSGL_DBG2) ) print_index(priv->idx,priv->idx_size,MSGL_DBG2);
    }
    break;
    /* added May 2002 */
    case mmioFOURCC('R','I','F','F'): {
	char riff_type[4];

	mp_msg(MSGT_HEADER, MSGL_V, MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr);
	stream_read(demuxer->stream, riff_type, sizeof riff_type);
	if (strncmp(riff_type, "AVIX", sizeof riff_type))
	    mp_msg(MSGT_HEADER, MSGL_WARN, MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr);
	else {
		/*
		 * We got an extended AVI header, so we need to switch to
		 * ODML to get seeking to work, provided we got indx chunks
		 * in the header (suidx_size > 0).
		 */
		if (priv->suidx_size > 0)
			priv->isodml = 1;
	}
	chunksize = 0;
	list_end = 0; /* a new list will follow */
	break; }
    case ckidAVIPADDING:
	stream_skip(demuxer->stream, chunksize);
	chunksize = 0;
	break;
  }
  if(hdr){
    mp_msg(MSGT_HEADER,MSGL_V,"hdr=%s  size=%u\n",hdr,size2);
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
  mp_msg(MSGT_HEADER,MSGL_DBG2,"list_end=0x%"PRIX64"  pos=0x%"PRIX64"  chunksize=0x%"PRIX64"  next=0x%"PRIX64"\n",
      (int64_t)list_end, (int64_t)stream_tell(demuxer->stream),
      (int64_t)chunksize, (int64_t)chunksize+(int64_t)stream_tell(demuxer->stream));
  if(list_end>0 &&
     chunksize+stream_tell(demuxer->stream) == list_end) list_end=0;
  if(list_end>0 && chunksize+stream_tell(demuxer->stream)>list_end){
      mp_msg(MSGT_HEADER,MSGL_V,MSGTR_MPDEMUX_AVIHDR_BrokenChunk,chunksize,(char *) &id);
      stream_seek(demuxer->stream,list_end);
      list_end=0;
  } else
  if(chunksize>0) stream_skip(demuxer->stream,chunksize); else
  if((int)chunksize<0) mp_msg(MSGT_HEADER,MSGL_WARN,"chunksize=%u  (id=%.4s)\n",chunksize,(char *) &id);
  
}

if (priv->suidx_size > 0 && priv->idx_size == 0) {
    /*
     * No NEWAVIINDEX, but we got an OpenDML index.
     */
    priv->isodml = 1;
}

if (priv->isodml && (index_mode==-1 || index_mode==0 || index_mode==1)) {
    int i, j, k;

    avisuperindex_chunk *cx;
    AVIINDEXENTRY *idx;


    if (priv->idx_size) free(priv->idx);
    priv->idx_size = 0;
    priv->idx_offset = 0;
    priv->idx = NULL;

    mp_msg(MSGT_HEADER, MSGL_INFO, MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx, priv->suidx_size);

    // read the standard indices
    for (cx = &priv->suidx[0], i=0; i<priv->suidx_size; cx++, i++) {
	stream_reset(demuxer->stream);
	for (j=0; j<cx->nEntriesInUse; j++) {
	    int ret1, ret2;
	    memset(&cx->stdidx[j], 0, 32);
	    ret1 = stream_seek(demuxer->stream, (off_t)cx->aIndex[j].qwOffset);
	    ret2 = stream_read(demuxer->stream, (char *)&cx->stdidx[j], 32);
	    if (ret1 != 1 || ret2 != 32 || cx->stdidx[j].nEntriesInUse==0) {
		// this is a broken file (probably incomplete) let the standard
		// gen_index routine handle this
		priv->isodml = 0;
		priv->idx_size = 0;
		mp_msg(MSGT_HEADER, MSGL_WARN, MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile);
		goto freeout;
	    }

	    le2me_AVISTDIDXCHUNK(&cx->stdidx[j]);
	    print_avistdindex_chunk(&cx->stdidx[j],MSGL_V);
	    priv->idx_size += cx->stdidx[j].nEntriesInUse;
	    cx->stdidx[j].aIndex = malloc(cx->stdidx[j].nEntriesInUse*sizeof(avistdindex_entry));
	    stream_read(demuxer->stream, (char *)cx->stdidx[j].aIndex, 
		    cx->stdidx[j].nEntriesInUse*sizeof(avistdindex_entry));
	    for (k=0;k<cx->stdidx[j].nEntriesInUse; k++)
		le2me_AVISTDIDXENTRY(&cx->stdidx[j].aIndex[k]);

	    cx->stdidx[j].dwReserved3 = 0;

	}
    }

    /*
     * We convert the index by translating all entries into AVIINDEXENTRYs
     * and sorting them by offset.  The result should be the same index
     * we would get with -forceidx.
     */

    idx = priv->idx = malloc(priv->idx_size * sizeof (AVIINDEXENTRY));

    for (cx = priv->suidx; cx != &priv->suidx[priv->suidx_size]; cx++) {
	avistdindex_chunk *sic;
	for (sic = cx->stdidx; sic != &cx->stdidx[cx->nEntriesInUse]; sic++) {
	    avistdindex_entry *sie;
	    for (sie = sic->aIndex; sie != &sic->aIndex[sic->nEntriesInUse]; sie++) {
		uint64_t off = sic->qwBaseOffset + sie->dwOffset - 8;
		memcpy(&idx->ckid, sic->dwChunkId, 4);
		idx->dwChunkOffset = off;
		idx->dwFlags = (off >> 32) << 16;
		idx->dwChunkLength = sie->dwSize & 0x7fffffff;
		idx->dwFlags |= (sie->dwSize&0x80000000)?0x0:AVIIF_KEYFRAME; // bit 31 denotes !keyframe
		idx++;
	    }
	}
    }
    qsort(priv->idx, priv->idx_size, sizeof(AVIINDEXENTRY), avi_idx_cmp);

    /*
       Hack to work around a "wrong" index in some divx odml files
       (processor_burning.avi as an example)
       They have ##dc on non keyframes but the ix00 tells us they are ##db.
       Read the fcc of a non-keyframe vid frame and check it.
     */

    {
	uint32_t id;
	uint32_t db = 0;
	stream_reset (demuxer->stream);

	// find out the video stream id. I have seen files with 01db.
	for (idx = &((AVIINDEXENTRY *)priv->idx)[0], i=0; i<priv->idx_size; i++, idx++){
	    unsigned char res[2];
	    if (odml_get_vstream_id(idx->ckid, res)) {
		db = mmioFOURCC(res[0], res[1], 'd', 'b');
		break;
	    }
	}

	// find first non keyframe
	for (idx = &((AVIINDEXENTRY *)priv->idx)[0], i=0; i<priv->idx_size; i++, idx++){
	    if (!(idx->dwFlags & AVIIF_KEYFRAME) && idx->ckid == db) break;
	}
	if (i<priv->idx_size && db) {
	    stream_seek(demuxer->stream, AVI_IDX_OFFSET(idx));
	    id = stream_read_dword_le(demuxer->stream);
	    if (id && id != db) // index fcc and real fcc differ? fix it.
		for (idx = &((AVIINDEXENTRY *)priv->idx)[0], i=0; i<priv->idx_size; i++, idx++){
		    if (!(idx->dwFlags & AVIIF_KEYFRAME) && idx->ckid == db)
			idx->ckid = id;
	    }
	}
    }

    if ( mp_msg_test(MSGT_HEADER,MSGL_DBG2) ) print_index(priv->idx, priv->idx_size,MSGL_DBG2);

    demuxer->movi_end=demuxer->stream->end_pos;

freeout:

    // free unneeded stuff
    cx = &priv->suidx[0];
    do {
	for (j=0;j<cx->nEntriesInUse;j++)
	    if (cx->stdidx[j].nEntriesInUse) free(cx->stdidx[j].aIndex);
	free(cx->stdidx);

    } while (cx++ != &priv->suidx[priv->suidx_size-1]);
    free(priv->suidx);

}

/* Read a saved index file */
if (index_file_load) {
  FILE *fp;
  char magic[7];
  unsigned int i;

  if ((fp = fopen(index_file_load, "r")) == NULL) {
    mp_msg(MSGT_HEADER,MSGL_ERR, MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile, index_file_load, strerror(errno));
    goto gen_index;
  }
  fread(&magic, 6, 1, fp);
  if (strncmp(magic, "MPIDX1", 6)) {
    mp_msg(MSGT_HEADER,MSGL_ERR, MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile, index_file_load);
    goto gen_index;
  }
  fread(&priv->idx_size, sizeof(priv->idx_size), 1, fp);
  priv->idx=malloc(priv->idx_size*sizeof(AVIINDEXENTRY));
  if (!priv->idx) {
    mp_msg(MSGT_HEADER,MSGL_ERR, MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile, index_file_load);
    priv->idx_size = 0;
    goto gen_index;
  }

  for (i=0; i<priv->idx_size;i++) {
    AVIINDEXENTRY *idx;
    idx=&((AVIINDEXENTRY *)priv->idx)[i];
    fread(idx, sizeof(AVIINDEXENTRY), 1, fp);
    if (feof(fp)) {
      mp_msg(MSGT_HEADER,MSGL_ERR, MSGTR_MPDEMUX_AVIHDR_PrematureEOF, index_file_load);
      free(priv->idx);
      priv->idx_size = 0;
      goto gen_index;
    }
  }
  fclose(fp);
  mp_msg(MSGT_HEADER,MSGL_INFO, MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded, index_file_load);
}
gen_index:
if(index_mode>=2 || (priv->idx_size==0 && index_mode==1)){
  // build index for file:
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream,demuxer->movi_start);
  
  priv->idx_pos=0;
  priv->idx_size=0;
  priv->idx=NULL;

  while(1){
    int id;
    unsigned len;
    off_t skip;
    AVIINDEXENTRY* idx;
    unsigned int c;
    demuxer->filepos=stream_tell(demuxer->stream);
    if(demuxer->filepos>=demuxer->movi_end && demuxer->movi_start<demuxer->movi_end) break;
    id=stream_read_dword_le(demuxer->stream);
    len=stream_read_dword_le(demuxer->stream);
    if(id==mmioFOURCC('L','I','S','T') || id==mmioFOURCC('R', 'I', 'F', 'F')){
      id=stream_read_dword_le(demuxer->stream); // list or RIFF type
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
    idx->dwFlags|=(demuxer->filepos>>16)&0xffff0000U;
    idx->dwChunkOffset=(unsigned long)demuxer->filepos;
    idx->dwChunkLength=len;
    
    c=stream_read_dword(demuxer->stream);

    if(!len) idx->dwFlags&=~AVIIF_KEYFRAME;

    // Fix keyframes for DivX files:
    if(idxfix_divx)
      if(avi_stream_id(id)==idxfix_videostream){
        switch(idxfix_divx){
    	    case 3: c=stream_read_dword(demuxer->stream)<<5; //skip 32+5 bits for m$mpeg4v1
    	    case 1: if(c&0x40000000) idx->dwFlags&=~AVIIF_KEYFRAME;break; // divx 3
	    case 2: if(c==0x1B6) idx->dwFlags&=~AVIIF_KEYFRAME;break; // divx 4
	}
      }

    // update status line:
    { static off_t lastpos;
      off_t pos;
      off_t len=demuxer->movi_end-demuxer->movi_start;
      if(len){
          pos=100*(demuxer->filepos-demuxer->movi_start)/len; // %
      } else {
          pos=(demuxer->filepos-demuxer->movi_start)>>20; // MB
      }
      if(pos!=lastpos){
          lastpos=pos;
	  mp_msg(MSGT_HEADER,MSGL_STATUS,MSGTR_MPDEMUX_AVIHDR_GeneratingIdx,
		 (unsigned long)pos, len?"%":"MB");
      }
    }
    mp_dbg(MSGT_HEADER,MSGL_DBG2,"%08X %08X %.4s %08X %X\n",(unsigned int)demuxer->filepos,id,(char *) &id,(int)c,(unsigned int) idx->dwFlags);
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
    skip=(len+1)&(~1UL); // total bytes in this chunk
    stream_seek(demuxer->stream,8+demuxer->filepos+skip);
  }
  priv->idx_size=priv->idx_pos;
  mp_msg(MSGT_HEADER,MSGL_INFO,MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks,priv->idx_size);
  if( mp_msg_test(MSGT_HEADER,MSGL_DBG2) ) print_index(priv->idx,priv->idx_size,MSGL_DBG2);

  /* Write generated index to a file */
  if (index_file_save) {
    FILE *fp;
    unsigned int i;

    if ((fp=fopen(index_file_save, "w")) == NULL) {
      mp_msg(MSGT_HEADER,MSGL_ERR, MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile, index_file_save, strerror(errno));
      return;
    }
    fwrite("MPIDX1", 6, 1, fp);
    fwrite(&priv->idx_size, sizeof(priv->idx_size), 1, fp);
    for (i=0; i<priv->idx_size; i++) {
      AVIINDEXENTRY *idx = &((AVIINDEXENTRY *)priv->idx)[i];
      fwrite(idx, sizeof(AVIINDEXENTRY), 1, fp);
    }
    fclose(fp);
    mp_msg(MSGT_HEADER,MSGL_INFO, MSGTR_MPDEMUX_AVIHDR_IdxFileSaved, index_file_save);
  }
}
}

