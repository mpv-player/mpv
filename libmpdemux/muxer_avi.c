#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>

#include "config.h"
#include "version.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "muxer.h"
#include "aviheader.h"
#include "ms_hdr.h"
#include "mp_msg.h"
#include "help_mp.h"

extern char *info_name;
extern char *info_artist;
extern char *info_genre;
extern char *info_subject;
extern char *info_copyright;
extern char *info_sourceform;
extern char *info_comment;

/* #define ODML_CHUNKLEN    0x02000000 */ /* for testing purposes */
#define ODML_CHUNKLEN    0x40000000
#define ODML_NOTKEYFRAME 0x80000000U
#define MOVIALIGN        0x00001000

float avi_aspect_override = -1.0;
int write_odml = 1;

struct avi_odmlidx_entry {
	uint64_t ofs;
	uint32_t len;
	uint32_t flags;
};

struct avi_odmlsuperidx_entry {
	uint64_t ofs;
	uint32_t len;
	uint32_t duration;
};

struct avi_stream_info {
	int idxsize;
	int idxpos;
	int superidxpos;
	int superidxsize;
	int riffofspos;
	int riffofssize;
	off_t *riffofs;
	struct avi_odmlidx_entry *idx;
	struct avi_odmlsuperidx_entry *superidx;
};

static unsigned int avi_aspect(muxer_stream_t *vstream)
{
    int x,y;
    float aspect = vstream->aspect;

    if (avi_aspect_override > 0.0) {
        aspect = avi_aspect_override;
    }

    if (aspect <= 0.0) return 0;

    if (aspect > 15.99/9.0 && aspect < 16.01/9.0) {
        return MAKE_AVI_ASPECT(16, 9);
    }
    if (aspect > 3.99/3.0 && aspect < 4.01/3.0) {
        return MAKE_AVI_ASPECT(4, 3);
    }

    if (aspect >= 1.0) {
        x = 16384;
        y = (float)x / aspect;
    } else {
        y = 16384;
        x = (float)y * aspect;
    }

    return MAKE_AVI_ASPECT(x, y);
}

static muxer_stream_t* avifile_new_stream(muxer_t *muxer,int type){
    struct avi_stream_info *si;
    muxer_stream_t* s;
    if (!muxer) return NULL;
    if(muxer->avih.dwStreams>=MUXER_MAX_STREAMS){
	mp_msg(MSGT_MUXER, MSGL_ERR, "Too many streams! increase MUXER_MAX_STREAMS !\n");
	return NULL;
    }
    s=malloc(sizeof(muxer_stream_t));
    memset(s,0,sizeof(muxer_stream_t));
    if(!s) return NULL; // no mem!?
    muxer->streams[muxer->avih.dwStreams]=s;
    s->type=type;
    s->id=muxer->avih.dwStreams;
    s->timer=0.0;
    s->size=0;
    s->muxer=muxer;
    s->priv=si=malloc(sizeof(struct avi_stream_info));
    memset(si,0,sizeof(struct avi_stream_info));
    si->idxsize=256;
    si->idx=calloc(si->idxsize, sizeof(struct avi_odmlidx_entry));
    si->riffofssize=16;
    si->riffofs=calloc((si->riffofssize+1), sizeof(off_t));
    memset(si->riffofs, 0, sizeof(off_t)*si->riffofssize);

    switch(type){
    case MUXER_TYPE_VIDEO:
      s->ckid=mmioFOURCC(('0'+s->id/10),('0'+(s->id%10)),'d','c');
      s->h.fccType=streamtypeVIDEO;
      if(!muxer->def_v) muxer->def_v=s;
      break;
    case MUXER_TYPE_AUDIO:
      s->ckid=mmioFOURCC(('0'+s->id/10),('0'+(s->id%10)),'w','b');
      s->h.fccType=streamtypeAUDIO;
      break;
    default:
      mp_msg(MSGT_MUXER, MSGL_WARN, "Warning! unknown stream type: %d\n",type);
      return NULL;
    }
    muxer->avih.dwStreams++;
    return s;
}

static void write_avi_chunk(stream_t *stream,unsigned int id,int len,void* data){
 int le_len = le2me_32(len);
 int le_id = le2me_32(id);
 stream_write_buffer(stream, &le_id, 4);
 stream_write_buffer(stream, &le_len, 4);

if(len>0){
  if(data){
    // DATA
    stream_write_buffer(stream, data, len);
    if(len&1){  // padding
      unsigned char zerobyte=0;
      stream_write_buffer(stream, &zerobyte, 1);
    }
  } else {
    // JUNK
    char *avi_junk_data="[= MPlayer junk data! =]";
    if(len&1) ++len; // padding
    while(len>0){
      int l=strlen(avi_junk_data);
      if(l>len) l=len;
      stream_write_buffer(stream, avi_junk_data, l);
      len-=l;
    }
  }
}
}

static void write_avi_list(stream_t *s,unsigned int id,int len);
static void avifile_write_standard_index(muxer_t *muxer);

static void avifile_odml_new_riff(muxer_t *muxer)
{
    struct avi_stream_info *vsi = muxer->def_v->priv;
    uint32_t riff[3];

    mp_msg(MSGT_MUXER, MSGL_INFO, "ODML: Starting new RIFF chunk at %dMB.\n", (int)(muxer->file_end/1024/1024));

    vsi->riffofspos++;
    if (vsi->riffofspos>=vsi->riffofssize) {
        vsi->riffofssize+=16;
        vsi->riffofs=realloc_struct(vsi->riffofs,(vsi->riffofssize+1),sizeof(off_t));
    }
    vsi->riffofs[vsi->riffofspos] = stream_tell(muxer->stream);

    /* RIFF/AVIX chunk */
    riff[0]=le2me_32(mmioFOURCC('R','I','F','F'));
    riff[1]=0;
    riff[2]=le2me_32(mmioFOURCC('A','V','I','X'));
    stream_write_buffer(muxer->stream, riff, 12);

    write_avi_list(muxer->stream,listtypeAVIMOVIE,0);

    muxer->file_end = stream_tell(muxer->stream);
}

static void avifile_write_header(muxer_t *muxer);

static void avifile_write_chunk(muxer_stream_t *s,size_t len,unsigned int flags, double dts, double pts){
    off_t rifflen;
    muxer_t *muxer=s->muxer;
    struct avi_stream_info *si = s->priv;
    struct avi_stream_info *vsi = muxer->def_v->priv;
    int paddedlen = len + (len&1);

    if (s->type == MUXER_TYPE_VIDEO && !s->h.dwSuggestedBufferSize) {
	off_t pos=stream_tell(muxer->stream);
	stream_seek(muxer->stream, 0);
	avifile_write_header(muxer);
	stream_seek(muxer->stream, pos);
    }
  if(index_mode){
    rifflen = muxer->file_end - vsi->riffofs[vsi->riffofspos] - 8;
    if (vsi->riffofspos == 0) {
	rifflen += 8+muxer->idx_pos*sizeof(AVIINDEXENTRY);
    }
    if (rifflen + paddedlen > ODML_CHUNKLEN && write_odml == 1) {
	if (vsi->riffofspos == 0) {
            avifile_write_standard_index(muxer);
	}
	avifile_odml_new_riff(muxer);
    }

    if (vsi->riffofspos == 0) {
        // add to the traditional index:
        if(muxer->idx_pos>=muxer->idx_size){
            muxer->idx_size+=256; // 4kB
            muxer->idx=realloc_struct(muxer->idx,muxer->idx_size,16);
        }
        muxer->idx[muxer->idx_pos].ckid=s->ckid;
        muxer->idx[muxer->idx_pos].dwFlags=flags; // keyframe?
        muxer->idx[muxer->idx_pos].dwChunkOffset=muxer->file_end-(muxer->movi_start-4);
        muxer->idx[muxer->idx_pos].dwChunkLength=len;
        ++muxer->idx_pos;
    }

    // add to odml index
    if(si->idxpos>=si->idxsize){
	si->idxsize+=256;
	si->idx=realloc_struct(si->idx,si->idxsize,sizeof(*si->idx));
    }
    si->idx[si->idxpos].flags=(flags&AVIIF_KEYFRAME)?0:ODML_NOTKEYFRAME;
    si->idx[si->idxpos].ofs=muxer->file_end;
    si->idx[si->idxpos].len=len;
    ++si->idxpos;
  }
    // write out the chunk:
    write_avi_chunk(muxer->stream,s->ckid,len,s->buffer); /* unsigned char */

    if (len > s->h.dwSuggestedBufferSize){
	s->h.dwSuggestedBufferSize = len;
    }
    if((unsigned int)len>s->h.dwSuggestedBufferSize) s->h.dwSuggestedBufferSize=len;

    muxer->file_end += 8 + paddedlen;
}

static void write_avi_list(stream_t *stream,unsigned int id,int len){
  unsigned int list_id=FOURCC_LIST;
  int le_len;
  int le_id;
  len+=4; // list fix
  list_id = le2me_32(list_id);
  le_len = le2me_32(len);
  le_id = le2me_32(id);
  stream_write_buffer(stream, &list_id, 4);
  stream_write_buffer(stream, &le_len, 4);
  stream_write_buffer(stream, &le_id, 4);
}

#define WFSIZE(wf) (sizeof(WAVEFORMATEX)+(wf)->cbSize)

static void avifile_write_header(muxer_t *muxer){
  uint32_t riff[3];
  unsigned int dmlh[1];
  unsigned int i;
  unsigned int hdrsize;
  muxer_info_t info[16];
  VideoPropHeader vprp;
  uint32_t aspect = avi_aspect(muxer->def_v);
  struct avi_stream_info *vsi = muxer->def_v->priv;
  int isodml = vsi->riffofspos > 0;

  mp_msg(MSGT_MUXER, MSGL_INFO, MSGTR_WritingHeader);
  if (aspect == 0) {
    mp_msg(MSGT_MUXER, MSGL_INFO, "ODML: Aspect information not (yet?) available or unspecified, not writing vprp header.\n");
  } else {
    mp_msg(MSGT_MUXER, MSGL_INFO, "ODML: vprp aspect is %d:%d.\n", aspect >> 16, aspect & 0xffff);
  }

  /* deal with stream delays */
  for (i = 0; muxer->streams[i] && i < MUXER_MAX_STREAMS; ++i) {
      muxer_stream_t *s = muxer->streams[i];
      if (s->type == MUXER_TYPE_AUDIO && muxer->audio_delay_fix > 0.0) {
          s->h.dwStart = muxer->audio_delay_fix * s->h.dwRate/s->h.dwScale + 0.5;
          mp_msg(MSGT_MUXER, MSGL_INFO, MSGTR_SettingAudioDelay, (float)s->h.dwStart * s->h.dwScale/s->h.dwRate);
      }
      if (s->type == MUXER_TYPE_VIDEO && muxer->audio_delay_fix < 0.0) {
          s->h.dwStart = -muxer->audio_delay_fix * s->h.dwRate/s->h.dwScale + 0.5;
          mp_msg(MSGT_MUXER, MSGL_INFO, MSGTR_SettingVideoDelay, (float)s->h.dwStart * s->h.dwScale/s->h.dwRate);
      }
  }
  
  if (isodml) {
      unsigned int rifflen, movilen;
      int i;

      vsi->riffofs[vsi->riffofspos+1] = muxer->file_end;

      /* fixup RIFF lengths */
      for (i=0; i<=vsi->riffofspos; i++) {
          rifflen = vsi->riffofs[i+1] - vsi->riffofs[i] - 8;
          movilen = le2me_32(rifflen - 12);
          rifflen = le2me_32(rifflen);
          stream_seek(muxer->stream, vsi->riffofs[i]+4);
          stream_write_buffer(muxer->stream,&rifflen,4);

          /* fixup movi length */
          if (i > 0) {
              stream_seek(muxer->stream, vsi->riffofs[i]+16);
              stream_write_buffer(muxer->stream,&movilen,4);
          }
      }

      stream_seek(muxer->stream, 12);
  } else {
    // RIFF header:
    riff[0]=mmioFOURCC('R','I','F','F');
    riff[1]=muxer->file_end-2*sizeof(unsigned int);  // filesize
    riff[2]=formtypeAVI; // 'AVI '
    riff[0]=le2me_32(riff[0]);
    riff[1]=le2me_32(riff[1]);
    riff[2]=le2me_32(riff[2]);
    stream_write_buffer(muxer->stream,&riff,12);
  }

  // update AVI header:
  if(muxer->def_v){
      int i;
      muxer->avih.dwMicroSecPerFrame=1000000.0*muxer->def_v->h.dwScale/muxer->def_v->h.dwRate;
//      muxer->avih.dwMaxBytesPerSec=1000000; // dummy!!!!! FIXME
//      muxer->avih.dwPaddingGranularity=2; // ???
      muxer->avih.dwFlags|=AVIF_ISINTERLEAVED|AVIF_TRUSTCKTYPE;
      muxer->avih.dwTotalFrames=0;
      for (i=0; i<muxer->idx_pos; i++) {
          if (muxer->idx[i].ckid == muxer->def_v->ckid)
              muxer->avih.dwTotalFrames++;
      }
//      muxer->avih.dwSuggestedBufferSize=muxer->def_v->h.dwSuggestedBufferSize;
      muxer->avih.dwWidth=muxer->def_v->bih->biWidth;
      muxer->avih.dwHeight=muxer->def_v->bih->biHeight;
  }

  // AVI header:
  hdrsize=sizeof(muxer->avih)+8;
  if (isodml) hdrsize+=sizeof(dmlh)+20; // dmlh
  // calc total header size:
  for(i=0;i<muxer->avih.dwStreams;i++){
      muxer_stream_t *s = muxer->streams[i];
      struct avi_stream_info *si = s->priv;

      hdrsize+=12; // LIST
      hdrsize+=sizeof(muxer->streams[i]->h)+8; // strh
      switch(muxer->streams[i]->type){
      case MUXER_TYPE_VIDEO:
          hdrsize+=muxer->streams[i]->bih->biSize+8; // strf
	  if (aspect != 0) {
	      hdrsize+=8+4*(9+8*1); // vprp
	  }
	  break;
      case MUXER_TYPE_AUDIO:
          hdrsize+=WFSIZE(muxer->streams[i]->wf)+8; // strf
	  break;
      }
      if (isodml && si && si->superidx && si->superidxsize) {
	  hdrsize += 32 + 16*si->superidxsize; //indx
      }
  }
  write_avi_list(muxer->stream,listtypeAVIHEADER,hdrsize);
  
  le2me_MainAVIHeader(&muxer->avih);
  write_avi_chunk(muxer->stream,ckidAVIMAINHDR,sizeof(muxer->avih),&muxer->avih); /* MainAVIHeader */
  le2me_MainAVIHeader(&muxer->avih);

  // stream headers:
  for(i=0;i<muxer->avih.dwStreams;i++){
      muxer_stream_t *s = muxer->streams[i];
      struct avi_stream_info *si = s->priv;
      unsigned int idxhdr[8];
      int j,n;

      hdrsize=sizeof(s->h)+8; // strh
      if (si && si->superidx && si->superidxsize) {
	  hdrsize += 32 + 16*si->superidxsize; //indx
      }
      switch(s->type){
      case MUXER_TYPE_VIDEO:
          hdrsize+=s->bih->biSize+8; // strf
          s->h.fccHandler = s->bih->biCompression;
          s->h.rcFrame.right = s->bih->biWidth;
          s->h.rcFrame.bottom = s->bih->biHeight;
	  if (aspect != 0) {
	      // fill out vprp info
	      memset(&vprp, 0, sizeof(vprp));
	      vprp.dwVerticalRefreshRate = (s->h.dwRate+s->h.dwScale-1)/s->h.dwScale;
	      vprp.dwHTotalInT = muxer->avih.dwWidth;
	      vprp.dwVTotalInLines = muxer->avih.dwHeight;
	      vprp.dwFrameAspectRatio = aspect;
	      vprp.dwFrameWidthInPixels = muxer->avih.dwWidth;
	      vprp.dwFrameHeightInLines = muxer->avih.dwHeight;
	      vprp.nbFieldPerFrame = 1;
	      vprp.FieldInfo[0].CompressedBMHeight = muxer->avih.dwHeight;
	      vprp.FieldInfo[0].CompressedBMWidth = muxer->avih.dwWidth;
	      vprp.FieldInfo[0].ValidBMHeight = muxer->avih.dwHeight;
	      vprp.FieldInfo[0].ValidBMWidth = muxer->avih.dwWidth;
	      hdrsize+=8+4*(9+8*1); // vprp
	  }
	  break;
      case MUXER_TYPE_AUDIO:
          hdrsize+=WFSIZE(s->wf)+8; // strf
          s->h.fccHandler = s->wf->wFormatTag;
	  break;
      }

      write_avi_list(muxer->stream,listtypeSTREAMHEADER,hdrsize);
      le2me_AVIStreamHeader(&s->h);
      write_avi_chunk(muxer->stream,ckidSTREAMHEADER,sizeof(s->h),&s->h); /* AVISTreamHeader */ // strh
      le2me_AVIStreamHeader(&s->h);

      switch(s->type){
      case MUXER_TYPE_VIDEO:
{
          int biSize=s->bih->biSize;
          le2me_BITMAPINFOHEADER(s->bih);
          write_avi_chunk(muxer->stream,ckidSTREAMFORMAT,biSize,s->bih); /* BITMAPINFOHEADER */
          le2me_BITMAPINFOHEADER(s->bih);

	  if (aspect != 0) {
	      int fields = vprp.nbFieldPerFrame;
	      le2me_VideoPropHeader(&vprp);
	      le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[0]);
	      le2me_VIDEO_FIELD_DESC(&vprp.FieldInfo[1]);
	      write_avi_chunk(muxer->stream,mmioFOURCC('v','p','r','p'),
	                      sizeof(VideoPropHeader) -
	                      sizeof(VIDEO_FIELD_DESC)*(2-fields),
	                      &vprp); /* Video Properties Header */
	  }
}
	  break;
      case MUXER_TYPE_AUDIO:
{
          int wfsize = WFSIZE(s->wf);
          le2me_WAVEFORMATEX(s->wf);
          write_avi_chunk(muxer->stream,ckidSTREAMFORMAT,wfsize,s->wf); /* WAVEFORMATEX */
          le2me_WAVEFORMATEX(s->wf);
}	  
	  break;
      }
      if (isodml && si && si->superidx && si->superidxsize) {
	  n = si->superidxsize;

	  idxhdr[0] = le2me_32(mmioFOURCC('i', 'n', 'd', 'x'));
	  idxhdr[1] = le2me_32(24 + 16*n);
	  idxhdr[2] = le2me_32(0x00000004);
	  idxhdr[3] = le2me_32(si->superidxpos);
	  idxhdr[4] = le2me_32(s->ckid);
	  idxhdr[5] = 0;
	  idxhdr[6] = 0;
	  idxhdr[7] = 0;

	  stream_write_buffer(muxer->stream,idxhdr,sizeof(idxhdr));
	  for (j=0; j<n; j++) {
	      struct avi_odmlsuperidx_entry *entry = &si->superidx[j];
	      unsigned int data[4];
	      data[0] = le2me_32(entry->ofs);
	      data[1] = le2me_32(entry->ofs >> 32);
	      data[2] = le2me_32(entry->len);
	      data[3] = le2me_32(entry->duration);
	      stream_write_buffer(muxer->stream,data,sizeof(data));
	  }
      }
  }

  // ODML
  if (isodml) {
      memset(dmlh, 0, sizeof(dmlh));
      dmlh[0] = le2me_32(muxer->avih.dwTotalFrames);
      write_avi_list(muxer->stream,mmioFOURCC('o','d','m','l'),sizeof(dmlh)+8);
      write_avi_chunk(muxer->stream,mmioFOURCC('d','m','l','h'),sizeof(dmlh),dmlh);
  }

// ============= INFO ===============
// always include software info
info[0].id=mmioFOURCC('I','S','F','T'); // Software:
info[0].text="MEncoder " VERSION;
// include any optional strings
i=1;
if(info_name!=NULL){
  info[i].id=mmioFOURCC('I','N','A','M'); // Name:
  info[i++].text=info_name;
}
if(info_artist!=NULL){
  info[i].id=mmioFOURCC('I','A','R','T'); // Artist:
  info[i++].text=info_artist;
}
if(info_genre!=NULL){
  info[i].id=mmioFOURCC('I','G','N','R'); // Genre:
  info[i++].text=info_genre;
}
if(info_subject!=NULL){
  info[i].id=mmioFOURCC('I','S','B','J'); // Subject:
  info[i++].text=info_subject;
}
if(info_copyright!=NULL){
  info[i].id=mmioFOURCC('I','C','O','P'); // Copyright:
  info[i++].text=info_copyright;
}
if(info_sourceform!=NULL){
  info[i].id=mmioFOURCC('I','S','R','F'); // Source Form:
  info[i++].text=info_sourceform;
}
if(info_comment!=NULL){
  info[i].id=mmioFOURCC('I','C','M','T'); // Comment:
  info[i++].text=info_comment;
}
info[i].id=0;

  hdrsize=0;
  // calc info size:
  for(i=0;info[i].id!=0;i++) if(info[i].text){
      size_t sz=strlen(info[i].text)+1;
      hdrsize+=sz+8+sz%2;
  }
  // write infos:
  if (hdrsize!=0){
      write_avi_list(muxer->stream,mmioFOURCC('I','N','F','O'),hdrsize);
      for(i=0;info[i].id!=0;i++) if(info[i].text){
          write_avi_chunk(muxer->stream,info[i].id,strlen(info[i].text)+1,info[i].text);
      }
  }

  // JUNK:
  write_avi_chunk(muxer->stream,ckidAVIPADDING,MOVIALIGN-(stream_tell(muxer->stream)%MOVIALIGN)-8,NULL); /* junk */
  if (!isodml) {
    // 'movi' header:
    write_avi_list(muxer->stream,listtypeAVIMOVIE,muxer->movi_end-stream_tell(muxer->stream)-12);
  } else {
    if (stream_tell(muxer->stream) != MOVIALIGN) {
	mp_msg(MSGT_MUXER, MSGL_ERR, "Opendml superindex is too big for reserved space!\n");
	mp_msg(MSGT_MUXER, MSGL_ERR, "Expected filepos %d, real filepos %ld, missing space %ld\n", MOVIALIGN, stream_tell(muxer->stream), stream_tell(muxer->stream)-MOVIALIGN);
	mp_msg(MSGT_MUXER, MSGL_ERR, "Try increasing MOVIALIGN in libmpdemux/muxer_avi.c\n");
    }
    write_avi_list(muxer->stream,listtypeAVIMOVIE,muxer->movi_end-stream_tell(muxer->stream)-12);
  }
  muxer->movi_start=stream_tell(muxer->stream);
  if (muxer->file_end == 0) muxer->file_end = stream_tell(muxer->stream);
}

static void avifile_odml_write_index(muxer_t *muxer){
  muxer_stream_t* s;
  struct avi_stream_info *si;
  int i;

  for (i=0; i<muxer->avih.dwStreams; i++) {
    int j,k,n,idxpos,len,last,entries_per_subidx;
    unsigned int idxhdr[8];
    s = muxer->streams[i];
    si = s->priv;

    /*
     * According to Avery Lee MSMP wants the subidx chunks to have the same size.
     *
     * So this code figures out how many entries we can put into
     * an ix?? chunk, so that each ix?? chunk has the same size and the offsets
     * don't overflow (Using ODML_CHUNKLEN for that is a bit more restrictive
     * than it has to be though).
     */

    len = 0;
    n = 0;
    entries_per_subidx = INT_MAX;
    do {
	off_t start = si->idx[0].ofs;
	last = entries_per_subidx;
	for (j=0; j<si->idxpos; j++) {
	    len = si->idx[j].ofs - start;
	    if(len >= ODML_CHUNKLEN || n >= entries_per_subidx) {
		if (entries_per_subidx > n) {
		    entries_per_subidx = n;
		}
		start = si->idx[j].ofs;
		len = 0;
		n = 0;
	    }
	    n++;
	}
    } while (last != entries_per_subidx);

    si->superidxpos = (si->idxpos+entries_per_subidx-1) / entries_per_subidx;

    mp_msg(MSGT_MUXER, MSGL_V, "ODML: Stream %d: Using %d entries per subidx, %d entries in superidx\n",
		i, entries_per_subidx, si->superidxpos);

    si->superidxsize = si->superidxpos;
    si->superidx = calloc(si->superidxsize, sizeof(*si->superidx));
    memset(si->superidx, 0, sizeof(*si->superidx) * si->superidxsize);

    idxpos = 0;
    for (j=0; j<si->superidxpos; j++) {
	off_t start = si->idx[idxpos].ofs;
	int duration;

	duration = 0;
	for (k=0; k<entries_per_subidx && idxpos+k<si->idxpos; k++) {
		duration += s->h.dwSampleSize ? si->idx[idxpos+k].len/s->h.dwSampleSize : 1;
	}

	idxhdr[0] = le2me_32((s->ckid << 16) | mmioFOURCC('i', 'x', 0, 0));
	idxhdr[1] = le2me_32(24 + 8*k);
	idxhdr[2] = le2me_32(0x01000002);
	idxhdr[3] = le2me_32(k);
	idxhdr[4] = le2me_32(s->ckid);
	idxhdr[5] = le2me_32(start + 8);
	idxhdr[6] = le2me_32((start + 8)>> 32);
	idxhdr[7] = 0; /* unused */

	si->superidx[j].len = 32 + 8*k;
	si->superidx[j].ofs = stream_tell(muxer->stream);
	si->superidx[j].duration = duration;

	stream_write_buffer(muxer->stream, idxhdr,sizeof(idxhdr));
	for (k=0; k<entries_per_subidx && idxpos<si->idxpos; k++) {
	    unsigned int entry[2];
	    entry[0] = le2me_32(si->idx[idxpos].ofs - start);
	    entry[1] = le2me_32(si->idx[idxpos].len | si->idx[idxpos].flags);
	    idxpos++;
	    stream_write_buffer(muxer->stream, entry, sizeof(entry));
	}
     }
  }
  muxer->file_end=stream_tell(muxer->stream);
}

static void avifile_write_standard_index(muxer_t *muxer){

  muxer->movi_end=stream_tell(muxer->stream);
  if(muxer->idx && muxer->idx_pos>0){
      int i;
      // fixup index entries:
//      for(i=0;i<muxer->idx_pos;i++) muxer->idx[i].dwChunkOffset-=muxer->movi_start-4;
      // write index chunk:
      for (i=0; i<muxer->idx_pos; i++) le2me_AVIINDEXENTRY((&muxer->idx[i]));
      write_avi_chunk(muxer->stream,ckidAVINEWINDEX,16*muxer->idx_pos,muxer->idx); /* AVIINDEXENTRY */
      for (i=0; i<muxer->idx_pos; i++) le2me_AVIINDEXENTRY((&muxer->idx[i]));
      muxer->avih.dwFlags|=AVIF_HASINDEX;
  }
  muxer->file_end=stream_tell(muxer->stream);
}

static void avifile_write_index(muxer_t *muxer){
  struct avi_stream_info *vsi = muxer->def_v->priv;

  mp_msg(MSGT_MUXER, MSGL_INFO, MSGTR_WritingTrailer);
  if (vsi->riffofspos > 0){
    avifile_odml_write_index(muxer);
  } else {
    avifile_write_standard_index(muxer);
  }
}

static void avifile_fix_parameters(muxer_stream_t *s){
  /* adjust audio_delay_fix according to individual stream delay */
  if (s->type == MUXER_TYPE_AUDIO)
    s->muxer->audio_delay_fix -= (float)s->decoder_delay * s->h.dwScale/s->h.dwRate;
  if (s->type == MUXER_TYPE_VIDEO)
    s->muxer->audio_delay_fix += (float)s->decoder_delay * s->h.dwScale/s->h.dwRate;
}

int muxer_init_muxer_avi(muxer_t *muxer){
  muxer->cont_new_stream = &avifile_new_stream;
  muxer->cont_write_chunk = &avifile_write_chunk;
  muxer->cont_write_header = &avifile_write_header;
  muxer->cont_write_index = &avifile_write_index;
  muxer->fix_stream_parameters = &avifile_fix_parameters;
  return 1;
}
