// .asf fileformat docs from http://divx.euro.ru


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "asf.h"

#include "asfguid.h"

typedef struct {
  // must be 0 for metadata record, might be non-zero for metadata lib record
  uint16_t lang_list_index;
  uint16_t stream_num;
  uint16_t name_length;
  uint16_t data_type;
  uint32_t data_length;
  uint16_t* name;
  void* data;
} ASF_meta_record_t;

static char* get_ucs2str(const uint16_t* inbuf, uint16_t inlen)
{
  char* outbuf = calloc(inlen, 2);
  char* q;
  int i;

  if (!outbuf) {
    mp_msg(MSGT_HEADER, MSGL_ERR, MSGTR_MemAllocFailed);
    return NULL;
  }
  q = outbuf;
  for (i = 0; i < inlen / 2; i++) {
    uint8_t tmp;
    PUT_UTF8(AV_RL16(&inbuf[i]), tmp, *q++ = tmp;)
  }
  return outbuf;
}

static const char* asf_chunk_type(unsigned char* guid) {
  static char tmp[60];
  char *p;
  int i;

  switch(ASF_LOAD_GUID_PREFIX(guid)){
    case ASF_GUID_PREFIX_audio_stream:
      return "guid_audio_stream";
    case ASF_GUID_PREFIX_ext_audio_stream:
      return "guid_ext_audio_stream";
    case ASF_GUID_PREFIX_ext_stream_embed_stream_header:
      return "guid_ext_stream_embed_stream_header";
    case ASF_GUID_PREFIX_video_stream: 
      return "guid_video_stream";
    case ASF_GUID_PREFIX_audio_conceal_none:
      return "guid_audio_conceal_none";
    case ASF_GUID_PREFIX_audio_conceal_interleave:
      return "guid_audio_conceal_interleave";
    case ASF_GUID_PREFIX_header:
      return "guid_header";
    case ASF_GUID_PREFIX_data_chunk:
      return "guid_data_chunk";
    case ASF_GUID_PREFIX_index_chunk:
      return "guid_index_chunk";
    case ASF_GUID_PREFIX_stream_header:
      return "guid_stream_header";
    case ASF_GUID_PREFIX_header_2_0: 
      return "guid_header_2_0";
    case ASF_GUID_PREFIX_file_header:
      return "guid_file_header";
    case ASF_GUID_PREFIX_content_desc:
      return "guid_content_desc";
    case ASF_GUID_PREFIX_dvr_ms_timing_rep_data:
      return "guid_dvr_ms_timing_rep_data";
    case ASF_GUID_PREFIX_dvr_ms_vid_frame_rep_data:
      return "guid_dvr_ms_vid_frame_rep_data";
    default:
      strcpy(tmp, "unknown guid ");
      p = tmp + strlen(tmp);
      for (i = 0; i < 16; i++) {
	if ((1 << i) & ((1<<4) | (1<<6) | (1<<8))) *p++ = '-';
	sprintf(p, "%02x", guid[i]);
	p += 2;
      }
      return tmp;
  }
}

int asf_check_header(demuxer_t *demuxer){
  unsigned char asfhdrguid[16]={0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11,0xA6,0xD9,0x00,0xAA,0x00,0x62,0xCE,0x6C};
  struct asf_priv* asf = calloc(1,sizeof(*asf));
  asf->scrambling_h=asf->scrambling_w=asf->scrambling_b=1;
  stream_read(demuxer->stream,(char*) &asf->header,sizeof(asf->header)); // header obj
  le2me_ASF_header_t(&asf->header);			// swap to machine endian
//  for(i=0;i<16;i++) printf(" %02X",temp[i]);printf("\n");
//  for(i=0;i<16;i++) printf(" %02X",asfhdrguid[i]);printf("\n");
  if(memcmp(asfhdrguid,asf->header.objh.guid,16)){
    mp_msg(MSGT_HEADER,MSGL_V,"ASF_check: not ASF guid!\n");
    free(asf);
    return 0; // not ASF guid
  }
  if(asf->header.cno>256){
    mp_msg(MSGT_HEADER,MSGL_V,"ASF_check: invalid subchunks_no %d\n",(int) asf->header.cno);
    free(asf);
    return 0; // invalid header???
  }
  demuxer->priv = asf;
  return DEMUXER_TYPE_ASF;
}

void print_wave_header(WAVEFORMATEX *h, int verbose_level);
void print_video_header(BITMAPINFOHEADER *h, int verbose_level);


static int get_ext_stream_properties(char *buf, int buf_len, int stream_num, struct asf_priv* asf, int is_video)
{
  int pos=0;
  uint8_t *buffer = &buf[0];
  uint64_t avg_ft;
  unsigned bitrate;

  while ((pos = find_asf_guid(buf, asf_ext_stream_header, pos, buf_len)) >= 0) {
    int this_stream_num, stnamect, payct, i;
    int buf_max_index=pos+50;
    if (buf_max_index > buf_len) return 0;
    buffer = &buf[pos];

    // the following info is available
    // some of it may be useful but we're skipping it for now
    // starttime(8 bytes), endtime(8),
    // leak-datarate(4), bucket-datasize(4), init-bucket-fullness(4),
    // alt-leak-datarate(4), alt-bucket-datasize(4), alt-init-bucket-fullness(4),
    // max-object-size(4),
    // flags(4) (reliable,seekable,no_cleanpoints?,resend-live-cleanpoints, rest of bits reserved)

    buffer += 8+8;
    bitrate = AV_RL32(buffer);
    buffer += 8*4;
    this_stream_num=AV_RL16(buffer);buffer+=2;

    if (this_stream_num == stream_num) {
      buf_max_index+=14;
      if (buf_max_index > buf_len) return 0;
      buffer+=2; //skip stream-language-id-index
      avg_ft = AV_RL64(buffer); // provided in 100ns units
      buffer+=8;
      asf->bps = bitrate / 8;

      // after this are values for stream-name-count and
      // payload-extension-system-count
      // followed by associated info for each
      stnamect = AV_RL16(buffer);buffer+=2;
      payct = AV_RL16(buffer);buffer+=2;

      // need to read stream names if present in order 
      // to get lengths - values are ignored for now
      for (i=0; i<stnamect; i++) {
        int stream_name_len;
        buf_max_index+=4;
        if (buf_max_index > buf_len) return 0;
        buffer+=2; //language_id_index
        stream_name_len = AV_RL16(buffer);buffer+=2;
        buffer+=stream_name_len; //stream_name
        buf_max_index+=stream_name_len;
        if (buf_max_index > buf_len) return 0;
      }

      if (is_video) {
        asf->vid_repdata_count = payct;
        asf->vid_repdata_sizes = malloc(payct*sizeof(int));
      } else {
        asf->aud_repdata_count = payct;
        asf->aud_repdata_sizes = malloc(payct*sizeof(int));
      }

      for (i=0; i<payct; i++) {
        int payload_len;
        buf_max_index+=22;
        if (buf_max_index > buf_len) return 0;
        // Each payload extension definition starts with a GUID.
        // In dvr-ms files one of these indicates the presence an
        // extension that contains pts values and this is always present
        // in the video and audio streams.
        // Another GUID indicates the presence of an extension
        // that contains useful video frame demuxing information.
        // Note that the extension data in each packet does not contain
        // these GUIDs and that this header section defines the order the data
        // will appear in.
        if (memcmp(buffer, asf_dvr_ms_timing_rep_data, 16) == 0) {
          if (is_video)
            asf->vid_ext_timing_index = i;
          else
            asf->aud_ext_timing_index = i;
        } else if (is_video && memcmp(buffer, asf_dvr_ms_vid_frame_rep_data, 16) == 0)
          asf->vid_ext_frame_index = i;
        buffer+=16;

        payload_len = AV_RL16(buffer);buffer+=2;

        if (is_video)
          asf->vid_repdata_sizes[i] = payload_len;
        else
          asf->aud_repdata_sizes[i] = payload_len;
        buffer+=4;//sys_len
      }

      return 1;
    }
  }
  return 1;
}

#define CHECKDEC(l, n) if (((l) -= (n)) < 0) return 0
static char* read_meta_record(ASF_meta_record_t* dest, char* buf,
    int* buf_len)
{
  CHECKDEC(*buf_len, 2 + 2 + 2 + 2 + 4);
  dest->lang_list_index = AV_RL16(buf);
  dest->stream_num = AV_RL16(&buf[2]);
  dest->name_length = AV_RL16(&buf[4]);
  dest->data_type = AV_RL16(&buf[6]);
  dest->data_length = AV_RL32(&buf[8]);
  buf += 2 + 2 + 2 + 2 + 4;
  CHECKDEC(*buf_len, dest->name_length);
  dest->name = (uint16_t*)buf;
  buf += dest->name_length;
  CHECKDEC(*buf_len, dest->data_length);
  dest->data = buf;
  buf += dest->data_length;
  return buf;
}

static int get_meta(char *buf, int buf_len, int this_stream_num,
    float* asp_ratio)
{
  int pos = 0;
  uint16_t records_count;
  uint16_t x = 0, y = 0;

  if ((pos = find_asf_guid(buf, asf_metadata_header, pos, buf_len)) < 0)
    return 0;

  CHECKDEC(buf_len, pos);
  buf += pos;
  CHECKDEC(buf_len, 2);
  records_count = AV_RL16(buf);
  buf += 2;

  while (records_count--) {
    ASF_meta_record_t record_entry;
    char* name;

    if (!(buf = read_meta_record(&record_entry, buf, &buf_len)))
        return 0;
    /* reserved, must be zero */
    if (record_entry.lang_list_index)
      continue;
    /* match stream number: 0 to match all */
    if (record_entry.stream_num && record_entry.stream_num != this_stream_num)
      continue;
    if (!(name = get_ucs2str(record_entry.name, record_entry.name_length))) {
      mp_msg(MSGT_HEADER, MSGL_ERR, MSGTR_MemAllocFailed);
      continue;
    }
    if (strcmp(name, "AspectRatioX") == 0)
      x = AV_RL16(record_entry.data);
    else if (strcmp(name, "AspectRatioY") == 0)
      y = AV_RL16(record_entry.data);
    free(name);
  }
  if (x && y) {
    *asp_ratio = (float)x / (float)y;
    return 1;
  }
  return 0;
}

static int is_drm(char* buf, int buf_len)
{
  uint32_t data_len, type_len, key_len, url_len;
  int pos = find_asf_guid(buf, asf_content_encryption, 0, buf_len);

  if (pos < 0)
    return 0;

  CHECKDEC(buf_len, pos + 4);
  buf += pos;
  data_len = AV_RL32(buf);
  buf += 4;
  CHECKDEC(buf_len, data_len);
  buf += data_len;
  type_len = AV_RL32(buf);
  if (type_len < 4)
    return 0;
  CHECKDEC(buf_len, 4 + type_len + 4);
  buf += 4;

  if (buf[0] != 'D' || buf[1] != 'R' || buf[2] != 'M' || buf[3] != '\0')
    return 0;

  buf += type_len;
  key_len = AV_RL32(buf);
  CHECKDEC(buf_len, key_len + 4);
  buf += 4;

  buf[key_len - 1] = '\0';
  mp_msg(MSGT_HEADER, MSGL_V, "DRM Key ID: %s\n", buf); 

  buf += key_len;
  url_len = AV_RL32(buf);
  CHECKDEC(buf_len, url_len);
  buf += 4;

  buf[url_len - 1] = '\0';
  mp_msg(MSGT_HEADER, MSGL_INFO, MSGTR_MPDEMUX_ASFHDR_DRMLicenseURL, buf);
  return 1;
} 

static int asf_init_audio_stream(demuxer_t *demuxer,struct asf_priv* asf, sh_audio_t* sh_audio, ASF_stream_header_t *streamh, int *ppos, uint8_t** buf, char *hdr, unsigned int hdr_len)
{
  uint8_t *buffer = *buf;
  int pos = *ppos;

  sh_audio->wf=calloc((streamh->type_size<sizeof(WAVEFORMATEX))?sizeof(WAVEFORMATEX):streamh->type_size,1);
  memcpy(sh_audio->wf,buffer,streamh->type_size);
  le2me_WAVEFORMATEX(sh_audio->wf);
  if( mp_msg_test(MSGT_HEADER,MSGL_V) ) print_wave_header(sh_audio->wf,MSGL_V);
  if(ASF_LOAD_GUID_PREFIX(streamh->concealment)==ASF_GUID_PREFIX_audio_conceal_interleave){
    buffer = &hdr[pos];
    pos += streamh->stream_size;
    if (pos > hdr_len) return 0;
    asf->scrambling_h=buffer[0];
    asf->scrambling_w=(buffer[2]<<8)|buffer[1];
    asf->scrambling_b=(buffer[4]<<8)|buffer[3];
    if(asf->scrambling_b>0){
      asf->scrambling_w/=asf->scrambling_b;
    }
  } else {
    asf->scrambling_b=asf->scrambling_h=asf->scrambling_w=1;
  }
  mp_msg(MSGT_HEADER,MSGL_V,"ASF: audio scrambling: %d x %d x %d\n",asf->scrambling_h,asf->scrambling_w,asf->scrambling_b);
  return 1;
}

static int find_backwards_asf_guid(char *buf, const char *guid, int cur_pos)
{
  int i;
  for (i=cur_pos-16; i>0; i--) {
    if (memcmp(&buf[i], guid, 16) == 0)
      return i + 16 + 8; // point after guid + length
  }
  return -1;
}

int read_asf_header(demuxer_t *demuxer,struct asf_priv* asf){
  int hdr_len = asf->header.objh.size - sizeof(asf->header);
  int hdr_skip = 0;
  char *hdr = NULL;
  char guid_buffer[16];
  int pos, start = stream_tell(demuxer->stream);
  uint32_t* streams = NULL;
  int audio_streams=0;
  int video_streams=0;
  uint16_t stream_count=0;
  int best_video = -1;
  int best_audio = -1;
  uint64_t data_len;
  ASF_stream_header_t *streamh;
  uint8_t *buffer;
  int audio_pos=0;

  if(hdr_len < 0) {
    mp_msg(MSGT_HEADER, MSGL_FATAL, "Header size is too small.\n");
    return 0;
  }
    
  if (hdr_len > 1024 * 1024) {
    mp_msg(MSGT_HEADER, MSGL_ERR, MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB,
			hdr_len);
    hdr_skip = hdr_len - 1024 * 1024;
    hdr_len = 1024 * 1024;
  }
  hdr = malloc(hdr_len);
  if (!hdr) {
    mp_msg(MSGT_HEADER, MSGL_FATAL, MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed,
            hdr_len);
    return 0;
  }
  stream_read(demuxer->stream, hdr, hdr_len);
  if (hdr_skip)
    stream_skip(demuxer->stream, hdr_skip);
  if (stream_eof(demuxer->stream)) {
    mp_msg(MSGT_HEADER, MSGL_FATAL, MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader);
    goto err_out;
  }

  if (is_drm(hdr, hdr_len))
    mp_msg(MSGT_HEADER, MSGL_FATAL, MSGTR_MPDEMUX_ASFHDR_DRMProtected);

  if ((pos = find_asf_guid(hdr, asf_ext_stream_audio, 0, hdr_len)) >= 0)
  {
    // Special case: found GUID for dvr-ms audio.
    // Now skip back to associated stream header.
    int sh_pos=0;

    sh_pos = find_backwards_asf_guid(hdr, asf_stream_header_guid, pos);
 
    if (sh_pos > 0) {
      sh_audio_t *sh_audio;

       mp_msg(MSGT_HEADER, MSGL_V, "read_asf_header found dvr-ms audio stream header pos=%d\n", sh_pos);
      // found audio stream header - following code reads header and
      // initializes audio stream.
      audio_pos = pos - 16 - 8;
      streamh = (ASF_stream_header_t *)&hdr[sh_pos];
      le2me_ASF_stream_header_t(streamh);
      audio_pos += 64; //16+16+4+4+4+16+4;
      buffer = &hdr[audio_pos];
      sh_audio=new_sh_audio(demuxer,streamh->stream_no & 0x7F);
      mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_AudioID, "asfheader", streamh->stream_no & 0x7F);
      ++audio_streams;
      if (!asf_init_audio_stream(demuxer, asf, sh_audio, streamh, &audio_pos, &buffer, hdr, hdr_len))
        goto len_err_out;
      if (!get_ext_stream_properties(hdr, hdr_len, streamh->stream_no, asf, 0))
        goto len_err_out;
    }
  }
  // find stream headers
  // only reset pos if we didnt find dvr_ms audio stream
  // if we did find it then we want to avoid reading its header twice
  if (audio_pos == 0) 
    pos = 0;

  while ((pos = find_asf_guid(hdr, asf_stream_header_guid, pos, hdr_len)) >= 0)
  {
    streamh = (ASF_stream_header_t *)&hdr[pos];
    pos += sizeof(ASF_stream_header_t);
    if (pos > hdr_len) goto len_err_out;
    le2me_ASF_stream_header_t(streamh);
    mp_msg(MSGT_HEADER, MSGL_V, "stream type: %s\n",
            asf_chunk_type(streamh->type));
    mp_msg(MSGT_HEADER, MSGL_V, "stream concealment: %s\n",
            asf_chunk_type(streamh->concealment));
    mp_msg(MSGT_HEADER, MSGL_V, "type: %d bytes,  stream: %d bytes  ID: %d\n",
            (int)streamh->type_size, (int)streamh->stream_size,
            (int)streamh->stream_no);
    mp_msg(MSGT_HEADER, MSGL_V, "unk1: %lX  unk2: %X\n",
            (unsigned long)streamh->unk1, (unsigned int)streamh->unk2);
    mp_msg(MSGT_HEADER, MSGL_V, "FILEPOS=0x%X\n", pos + start);
    // type-specific data:
    buffer = &hdr[pos];
    pos += streamh->type_size;
    if (pos > hdr_len) goto len_err_out;
    switch(ASF_LOAD_GUID_PREFIX(streamh->type)){
      case ASF_GUID_PREFIX_audio_stream: {
        sh_audio_t* sh_audio=new_sh_audio(demuxer,streamh->stream_no & 0x7F);
        mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_AudioID, "asfheader", streamh->stream_no & 0x7F);
        ++audio_streams;
        if (!asf_init_audio_stream(demuxer, asf, sh_audio, streamh, &pos, &buffer, hdr, hdr_len))
          goto len_err_out;
	//if(demuxer->audio->id==-1) demuxer->audio->id=streamh.stream_no & 0x7F;
        break;
        }
      case ASF_GUID_PREFIX_video_stream: {
        unsigned int len;
        float asp_ratio;
        sh_video_t* sh_video=new_sh_video(demuxer,streamh->stream_no & 0x7F);
        mp_msg(MSGT_DEMUX, MSGL_INFO, MSGTR_VideoID, "asfheader", streamh->stream_no & 0x7F);
        len=streamh->type_size-(4+4+1+2);
	++video_streams;
//        sh_video->bih=malloc(chunksize); memset(sh_video->bih,0,chunksize);
        sh_video->bih=calloc((len<sizeof(BITMAPINFOHEADER))?sizeof(BITMAPINFOHEADER):len,1);
        memcpy(sh_video->bih,&buffer[4+4+1+2],len);
	le2me_BITMAPINFOHEADER(sh_video->bih);
	if (sh_video->bih->biSize > len && sh_video->bih->biSize > sizeof(BITMAPINFOHEADER))
		sh_video->bih->biSize = len;
        if (sh_video->bih->biCompression == mmioFOURCC('D', 'V', 'R', ' ')) {
          //mp_msg(MSGT_DEMUXER, MSGL_WARN, MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat);
          //sh_video->fps=(float)sh_video->video.dwRate/(float)sh_video->video.dwScale;
          //sh_video->frametime=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
          asf->asf_frame_state=-1;
          asf->asf_frame_start_found=0;
          asf->asf_is_dvr_ms=1;
          asf->dvr_last_vid_pts=0.0;
        } else asf->asf_is_dvr_ms=0;
        if (!get_ext_stream_properties(hdr, hdr_len, streamh->stream_no, asf, 1))
            goto len_err_out;
        if (get_meta(hdr, hdr_len, streamh->stream_no, &asp_ratio)) {
          sh_video->aspect = asp_ratio * sh_video->bih->biWidth /
            sh_video->bih->biHeight;
        }
        sh_video->i_bps = asf->bps;

        if( mp_msg_test(MSGT_DEMUX,MSGL_V) ) print_video_header(sh_video->bih, MSGL_V);
        //asf_video_id=streamh.stream_no & 0x7F;
	//if(demuxer->video->id==-1) demuxer->video->id=streamh.stream_no & 0x7F;
        break;
        }
      }
      // stream-specific data:
      // stream_read(demuxer->stream,(char*) buffer,streamh.stream_size);
  }

  // find file header
  pos = find_asf_guid(hdr, asf_file_header_guid, 0, hdr_len);
  if (pos >= 0) {
      ASF_file_header_t *fileh = (ASF_file_header_t *)&hdr[pos];
      pos += sizeof(ASF_file_header_t);
      if (pos > hdr_len) goto len_err_out;
      le2me_ASF_file_header_t(fileh);
      mp_msg(MSGT_HEADER, MSGL_V, "ASF: packets: %d  flags: %d  "
              "max_packet_size: %d  min_packet_size: %d  max_bitrate: %d  "
              "preroll: %d\n",
              (int)fileh->num_packets, (int)fileh->flags, 
              (int)fileh->min_packet_size, (int)fileh->max_packet_size,
              (int)fileh->max_bitrate, (int)fileh->preroll);
      asf->packetsize=fileh->max_packet_size;
      asf->packet=malloc(asf->packetsize); // !!!
      asf->packetrate=fileh->max_bitrate/8.0/(double)asf->packetsize;
      asf->movielength=(fileh->play_duration-fileh->preroll)/10000000LL;
  }

  // find content header
  pos = find_asf_guid(hdr, asf_content_desc_guid, 0, hdr_len);
  if (pos >= 0) {
        ASF_content_description_t *contenth = (ASF_content_description_t *)&hdr[pos];
        char *string=NULL;
        uint16_t* wstring = NULL;
        uint16_t len;
        pos += sizeof(ASF_content_description_t);
        if (pos > hdr_len) goto len_err_out;
	le2me_ASF_content_description_t(contenth);
	mp_msg(MSGT_HEADER,MSGL_V,"\n");
        // extract the title
        if((len = contenth->title_size) != 0) {
          wstring = (uint16_t*)&hdr[pos];
          pos += len;
          if (pos > hdr_len) goto len_err_out;
          if ((string = get_ucs2str(wstring, len))) {
            mp_msg(MSGT_HEADER,MSGL_V," Title: %s\n", string);
            demux_info_add(demuxer, "name", string);
            free(string);
          }
        }
        // extract the author 
        if((len = contenth->author_size) != 0) {
          wstring = (uint16_t*)&hdr[pos];
          pos += len;
          if (pos > hdr_len) goto len_err_out;
          if ((string = get_ucs2str(wstring, len))) {
            mp_msg(MSGT_HEADER,MSGL_V," Author: %s\n", string);
            demux_info_add(demuxer, "author", string);
            free(string);
          }
        }
        // extract the copyright
        if((len = contenth->copyright_size) != 0) {
          wstring = (uint16_t*)&hdr[pos];
          pos += len;
          if (pos > hdr_len) goto len_err_out;
          if ((string = get_ucs2str(wstring, len))) {
            mp_msg(MSGT_HEADER,MSGL_V," Copyright: %s\n", string);
            demux_info_add(demuxer, "copyright", string);
            free(string);
          }
        }
        // extract the comment
        if((len = contenth->comment_size) != 0) {
          wstring = (uint16_t*)&hdr[pos];
          pos += len;
          if (pos > hdr_len) goto len_err_out;
          if ((string = get_ucs2str(wstring, len))) {
            mp_msg(MSGT_HEADER,MSGL_V," Comment: %s\n", string);
            demux_info_add(demuxer, "comments", string);
            free(string);
          }
        }
        // extract the rating
        if((len = contenth->rating_size) != 0) {
          wstring = (uint16_t*)&hdr[pos];
          pos += len;
          if (pos > hdr_len) goto len_err_out;
          if ((string = get_ucs2str(wstring, len))) {
            mp_msg(MSGT_HEADER,MSGL_V," Rating: %s\n", string);
            free(string);
          }
        }
	mp_msg(MSGT_HEADER,MSGL_V,"\n");
  }
  
  // find content header
  pos = find_asf_guid(hdr, asf_stream_group_guid, 0, hdr_len);
  if (pos >= 0) {
        int max_streams = (hdr_len - pos - 2) / 6;
        uint16_t stream_id, i;
        uint32_t max_bitrate;
        char *ptr = &hdr[pos];
        mp_msg(MSGT_HEADER,MSGL_V,"============ ASF Stream group == START ===\n");
        if(max_streams <= 0) goto len_err_out;
        stream_count = AV_RL16(ptr);
        ptr += sizeof(uint16_t);
        if(stream_count > max_streams) stream_count = max_streams;
        if(stream_count > 0)
              streams = malloc(2*stream_count*sizeof(uint32_t));
        mp_msg(MSGT_HEADER,MSGL_V," stream count=[0x%x][%u]\n", stream_count, stream_count );
        for( i=0 ; i<stream_count ; i++ ) {
          stream_id = AV_RL16(ptr);
          ptr += sizeof(uint16_t);
          memcpy(&max_bitrate, ptr, sizeof(uint32_t));// workaround unaligment bug on sparc
          max_bitrate = le2me_32(max_bitrate);
          ptr += sizeof(uint32_t);
          mp_msg(MSGT_HEADER,MSGL_V,"   stream id=[0x%x][%u]\n", stream_id, stream_id );
          mp_msg(MSGT_HEADER,MSGL_V,"   max bitrate=[0x%x][%u]\n", max_bitrate, max_bitrate );
          streams[2*i] = stream_id;
          streams[2*i+1] = max_bitrate;
        }
        mp_msg(MSGT_HEADER,MSGL_V,"============ ASF Stream group == END ===\n");
  }
  free(hdr);
  hdr = NULL;
  start = stream_tell(demuxer->stream); // start of first data chunk
  stream_read(demuxer->stream, guid_buffer, 16);
  if (memcmp(guid_buffer, asf_data_chunk_guid, 16) != 0) {
    mp_msg(MSGT_HEADER, MSGL_FATAL, MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader);
    free(streams);
    streams = NULL;
    return 0;
  }
  // read length of chunk
  stream_read(demuxer->stream, (char *)&data_len, sizeof(data_len));
  data_len = le2me_64(data_len);
  demuxer->movi_start = stream_tell(demuxer->stream) + 26;
  demuxer->movi_end = start + data_len;
  mp_msg(MSGT_HEADER, MSGL_V, "Found movie at 0x%X - 0x%X\n",
          (int)demuxer->movi_start, (int)demuxer->movi_end);

if(streams) {
  // stream selection is done in the network code, it shouldn't be done here
  // as the servers often do not care about what we requested.
#if 0
  uint32_t vr = 0, ar = 0,i;
#ifdef CONFIG_NETWORK
  if( demuxer->stream->streaming_ctrl!=NULL ) {
	  if( demuxer->stream->streaming_ctrl->bandwidth!=0 && demuxer->stream->streaming_ctrl->data!=NULL ) {
		  best_audio = ((asf_http_streaming_ctrl_t*)demuxer->stream->streaming_ctrl->data)->audio_id;
		  best_video = ((asf_http_streaming_ctrl_t*)demuxer->stream->streaming_ctrl->data)->video_id;
	  }
  } else
#endif
  for(i = 0; i < stream_count; i++) {
    uint32_t id = streams[2*i];
    uint32_t rate = streams[2*i+1];
    if(demuxer->v_streams[id] && rate > vr) {
      vr = rate;
      best_video = id;
    } else if(demuxer->a_streams[id] && rate > ar) {
      ar = rate;
      best_audio = id;
    }
  }
#endif
  free(streams);
  streams = NULL;
}

mp_msg(MSGT_HEADER,MSGL_V,"ASF: %d audio and %d video streams found\n",audio_streams,video_streams);
if(!audio_streams) demuxer->audio->id=-2;  // nosound
else if(best_audio > 0 && demuxer->audio->id == -1) demuxer->audio->id=best_audio;
if(!video_streams){
    if(!audio_streams){
	mp_msg(MSGT_HEADER,MSGL_ERR,MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound);
	return 0; 
    }
    demuxer->video->id=-2; // audio-only
} else if (best_video > 0 && demuxer->video->id == -1) demuxer->video->id = best_video;

#if 0
if( mp_msg_test(MSGT_HEADER,MSGL_V) ){
    printf("ASF duration: %d\n",(int)fileh.duration);
    printf("ASF start pts: %d\n",(int)fileh.start_timestamp);
    printf("ASF end pts: %d\n",(int)fileh.end_timestamp);
}
#endif

return 1;

len_err_out:
  mp_msg(MSGT_HEADER, MSGL_FATAL, MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader);
err_out:
  if (hdr) free(hdr);
  if (streams) free(streams);
  return 0;
}
