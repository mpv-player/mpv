// .asf fileformat docs from http://divx.euro.ru


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int verbose; // defined in mplayer.c

#include "config.h"
#include "mp_msg.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#include "asf.h"

#ifdef ARCH_X86
#define	ASF_LOAD_GUID_PREFIX(guid)	(*(uint32_t *)(guid))
#else
#define	ASF_LOAD_GUID_PREFIX(guid)	\
	((guid)[3] << 24 | (guid)[2] << 16 | (guid)[1] << 8 | (guid)[0])
#endif

#define ASF_GUID_PREFIX_audio_stream	0xF8699E40
#define ASF_GUID_PREFIX_video_stream	0xBC19EFC0
#define ASF_GUID_PREFIX_audio_conceal_none 0x49f1a440
#define ASF_GUID_PREFIX_audio_conceal_interleave 0xbfc3cd50
#define ASF_GUID_PREFIX_header		0x75B22630
#define ASF_GUID_PREFIX_data_chunk	0x75b22636
#define ASF_GUID_PREFIX_index_chunk	0x33000890
#define ASF_GUID_PREFIX_stream_header	0xB7DC0791
#define ASF_GUID_PREFIX_header_2_0	0xD6E229D1
#define ASF_GUID_PREFIX_file_header	0x8CABDCA1
#define	ASF_GUID_PREFIX_content_desc	0x75b22633
#define	ASF_GUID_PREFIX_stream_group	0x7bf875ce


static ASF_header_t asfh;
static ASF_obj_header_t objh;
static ASF_file_header_t fileh;
static ASF_stream_header_t streamh;
static ASF_content_description_t contenth;

unsigned char* asf_packet=NULL;
int asf_scrambling_h=1;
int asf_scrambling_w=1;
int asf_scrambling_b=1;
int asf_packetsize=0;

//int i;

// the variable string is modify in this function
void pack_asf_string(char* string, int length) {
  int i,j;
  if( string==NULL ) return;
  for( i=0, j=0; i<length && string[i]!='\0'; i+=2, j++) {
    string[j]=string[i];
  }
  string[j]='\0';
}

// the variable string is modify in this function
void print_asf_string(const char* name, char* string, int length) {
  pack_asf_string(string, length);
  mp_msg(MSGT_HEADER,MSGL_V,"%s%s\n", name, string);
}

static char* asf_chunk_type(unsigned char* guid) {
  static char tmp[60];
  char *p;
  int i;

  switch(ASF_LOAD_GUID_PREFIX(guid)){
    case ASF_GUID_PREFIX_audio_stream:
      return "guid_audio_stream";
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
  stream_read(demuxer->stream,(char*) &asfh,sizeof(asfh)); // header obj
  le2me_ASF_header_t(&asfh);			// swap to machine endian
//  for(i=0;i<16;i++) printf(" %02X",temp[i]);printf("\n");
//  for(i=0;i<16;i++) printf(" %02X",asfhdrguid[i]);printf("\n");
  if(memcmp(asfhdrguid,asfh.objh.guid,16)){
    mp_msg(MSGT_HEADER,MSGL_V,"ASF_check: not ASF guid!\n");
    return 0; // not ASF guid
  }
  if(asfh.cno>256){
    mp_msg(MSGT_HEADER,MSGL_V,"ASF_check: invalid subchunks_no %d\n",(int) asfh.cno);
    return 0; // invalid header???
  }
  return 1;
}

extern void print_wave_header(WAVEFORMATEX *h);
extern void print_video_header(BITMAPINFOHEADER *h);

int read_asf_header(demuxer_t *demuxer){
  static unsigned char buffer[1024];
  uint32_t* streams = NULL;
  int audio_streams=0;
  int video_streams=0;
  uint16_t stream_count=0;
  int best_video = -1;
  int best_audio = -1;

#if 1
  //printf("ASF file! (subchunks: %d)\n",asfh.cno);
while(!stream_eof(demuxer->stream)){
  int pos,endpos;
  pos=stream_tell(demuxer->stream);
  stream_read(demuxer->stream,(char*) &objh,sizeof(objh));
  le2me_ASF_obj_header_t(&objh);
  if(stream_eof(demuxer->stream)) break; // EOF
  endpos=pos+objh.size;
//  for(i=0;i<16;i++) printf("%02X ",objh.guid[i]);
  //printf("0x%08X  [%s] %d\n",pos, asf_chunk_type(objh.guid),(int) objh.size);
  switch(ASF_LOAD_GUID_PREFIX(objh.guid)){
    case ASF_GUID_PREFIX_stream_header:
      stream_read(demuxer->stream,(char*) &streamh,sizeof(streamh));
      le2me_ASF_stream_header_t(&streamh);
      if(verbose){
        mp_msg(MSGT_HEADER,MSGL_V,"stream type: %s\n",asf_chunk_type(streamh.type));
	mp_msg(MSGT_HEADER,MSGL_V,"stream concealment: %s\n",asf_chunk_type(streamh.concealment));
	mp_msg(MSGT_HEADER,MSGL_V,"type: %d bytes,  stream: %d bytes  ID: %d\n",(int)streamh.type_size,(int)streamh.stream_size,(int)streamh.stream_no);
	mp_msg(MSGT_HEADER,MSGL_V,"unk1: %lX  unk2: %X\n",(unsigned long)streamh.unk1,(unsigned int)streamh.unk2);
	mp_msg(MSGT_HEADER,MSGL_V,"FILEPOS=0x%X\n",stream_tell(demuxer->stream));
      }
      if(streamh.type_size>1024 || streamh.stream_size>1024){
          mp_msg(MSGT_HEADER,MSGL_FATAL,"FATAL: header size bigger than 1024 bytes!\n"
              "Please contact mplayer authors, and upload/send this file.\n");
          return 0;
      }
      // type-specific data:
      stream_read(demuxer->stream,(char*) buffer,streamh.type_size);
      switch(ASF_LOAD_GUID_PREFIX(streamh.type)){
      case ASF_GUID_PREFIX_audio_stream: {
        sh_audio_t* sh_audio=new_sh_audio(demuxer,streamh.stream_no & 0x7F);
	++audio_streams;
        sh_audio->wf=calloc((streamh.type_size<sizeof(WAVEFORMATEX))?sizeof(WAVEFORMATEX):streamh.type_size,1);
        memcpy(sh_audio->wf,buffer,streamh.type_size);
	le2me_WAVEFORMATEX(sh_audio->wf);
        if(verbose>=1) print_wave_header(sh_audio->wf);
	if(ASF_LOAD_GUID_PREFIX(streamh.concealment)==ASF_GUID_PREFIX_audio_conceal_interleave){
          stream_read(demuxer->stream,(char*) buffer,streamh.stream_size);
          asf_scrambling_h=buffer[0];
          asf_scrambling_w=(buffer[2]<<8)|buffer[1];
          asf_scrambling_b=(buffer[4]<<8)|buffer[3];
  	  asf_scrambling_w/=asf_scrambling_b;
	} else {
	  asf_scrambling_b=asf_scrambling_h=asf_scrambling_w=1;
	}
	mp_msg(MSGT_HEADER,MSGL_V,"ASF: audio scrambling: %d x %d x %d\n",asf_scrambling_h,asf_scrambling_w,asf_scrambling_b);
	//if(demuxer->audio->id==-1) demuxer->audio->id=streamh.stream_no & 0x7F;
        break;
        }
      case ASF_GUID_PREFIX_video_stream: {
        sh_video_t* sh_video=new_sh_video(demuxer,streamh.stream_no & 0x7F);
        unsigned int len=streamh.type_size-(4+4+1+2);
	++video_streams;
//        sh_video->bih=malloc(chunksize); memset(sh_video->bih,0,chunksize);
        sh_video->bih=calloc((len<sizeof(BITMAPINFOHEADER))?sizeof(BITMAPINFOHEADER):len,1);
        memcpy(sh_video->bih,&buffer[4+4+1+2],len);
	le2me_BITMAPINFOHEADER(sh_video->bih);
        //sh_video->fps=(float)sh_video->video.dwRate/(float)sh_video->video.dwScale;
        //sh_video->frametime=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
        if(verbose>=1) print_video_header(sh_video->bih);
        //asf_video_id=streamh.stream_no & 0x7F;
	//if(demuxer->video->id==-1) demuxer->video->id=streamh.stream_no & 0x7F;
        break;
        }
      }
      // stream-specific data:
      // stream_read(demuxer->stream,(char*) buffer,streamh.stream_size);
      break;
//  case ASF_GUID_PREFIX_header_2_0: return "guid_header_2_0";
    case ASF_GUID_PREFIX_file_header: // guid_file_header
      stream_read(demuxer->stream,(char*) &fileh,sizeof(fileh));
      le2me_ASF_file_header_t(&fileh);
      //mp_msg(MSGT_HEADER,MSGL_V,"ASF: packets: %d  flags: %d  pack_size: %d  frame_size: %d\n",(int)fileh.packets,(int)fileh.flags,(int)fileh.packetsize,(int)fileh.frame_size);
      mp_msg(MSGT_HEADER,MSGL_V,"ASF: packets: %d  flags: %d  max_packet_size: %d  min_packet_size: %d  max_bitrate: %d  preroll: %d\n",(int)fileh.num_packets,(int)fileh.flags,(int)fileh.min_packet_size,(int)fileh.max_packet_size,(int)fileh.max_bitrate,(int)fileh.preroll);
      asf_packetsize=fileh.max_packet_size;
      asf_packet=malloc(asf_packetsize); // !!!
      break;
    case ASF_GUID_PREFIX_data_chunk: // guid_data_chunk
      demuxer->movi_start=stream_tell(demuxer->stream)+26;
      demuxer->movi_end=endpos;
      mp_msg(MSGT_HEADER,MSGL_V,"Found movie at 0x%X - 0x%X\n",(int)demuxer->movi_start,(int)demuxer->movi_end);
      break;

//  case ASF_GUID_PREFIX_index_chunk: return "guid_index_chunk";

    case ASF_GUID_PREFIX_content_desc: // Content description
    {
        char *string=NULL;
        stream_read(demuxer->stream,(char*) &contenth,sizeof(contenth));
	le2me_ASF_content_description_t(&contenth);
	mp_msg(MSGT_HEADER,MSGL_V,"\n");
        // extract the title
        if( contenth.title_size!=0 ) {
          string=(char*)malloc(contenth.title_size);
          stream_read(demuxer->stream, string, contenth.title_size);
          if(verbose)
            print_asf_string(" Title: ", string, contenth.title_size);
	  else
	    pack_asf_string(string, contenth.title_size);
	  demux_info_add(demuxer, "name", string);
        }
        // extract the author 
        if( contenth.author_size!=0 ) {
          string=(char*)realloc((void*)string, contenth.author_size);
          stream_read(demuxer->stream, string, contenth.author_size);
          if(verbose)
            print_asf_string(" Author: ", string, contenth.author_size);
	  else
	    pack_asf_string(string, contenth.author_size);
	  demux_info_add(demuxer, "author", string);
        }
        // extract the copyright
        if( contenth.copyright_size!=0 ) {
          string=(char*)realloc((void*)string, contenth.copyright_size);
          stream_read(demuxer->stream, string, contenth.copyright_size);
          if(verbose)
            print_asf_string(" Copyright: ", string, contenth.copyright_size);
	  else
	    pack_asf_string(string, contenth.copyright_size);
	  demux_info_add(demuxer, "copyright", string);
        }
        // extract the comment
        if( contenth.comment_size!=0 ) {
          string=(char*)realloc((void*)string, contenth.comment_size);
          stream_read(demuxer->stream, string, contenth.comment_size);
          if(verbose)
            print_asf_string(" Comment: ", string, contenth.comment_size);
	  else
	    pack_asf_string(string, contenth.comment_size);
	  demux_info_add(demuxer, "comments", string);
        }
        // extract the rating
        if( contenth.rating_size!=0 ) {
          string=(char*)realloc((void*)string, contenth.rating_size);
          stream_read(demuxer->stream, string, contenth.rating_size);
          if(verbose)
            print_asf_string(" Rating: ", string, contenth.rating_size);
        }
	mp_msg(MSGT_HEADER,MSGL_V,"\n");
        free(string);
      break;
    }
    case ASF_GUID_PREFIX_stream_group: {
        uint16_t stream_id, i;
        uint32_t max_bitrate;
        char *object=NULL, *ptr=NULL;
        printf("============ ASF Stream group == START ===\n");
        printf(" object size = %d\n", (int)objh.size);
        object = (char*)malloc(objh.size);
	if( object==NULL ) {
          printf("Memory allocation failed\n");
	  return 0;
	}
        stream_read( demuxer->stream, object, objh.size );
	// FIXME: We need some endian handling below...
	ptr = object;
        stream_count = le2me_16(*(uint16_t*)ptr);
        ptr += sizeof(uint16_t);
        if(stream_count > 0)
              streams = (uint32_t*)malloc(2*stream_count*sizeof(uint32_t));
        printf(" stream count=[0x%x][%u]\n", stream_count, stream_count );
        for( i=0 ; i<stream_count && ptr<((char*)object+objh.size) ; i++ ) {
          stream_id = le2me_16(*(uint16_t*)ptr);
          ptr += sizeof(uint16_t);
          memcpy(&max_bitrate, ptr, sizeof(uint32_t));// workaround unaligment bug on sparc
          max_bitrate = le2me_32(max_bitrate);
          ptr += sizeof(uint32_t);
          printf("   stream id=[0x%x][%u]\n", stream_id, stream_id );
          printf("   max bitrate=[0x%x][%u]\n", max_bitrate, max_bitrate );
          streams[2*i] = stream_id;
          streams[2*i+1] = max_bitrate;
        }
        printf("============ ASF Stream group == END ===\n");
        free( object );
      break;
    }
  } // switch GUID

  if(ASF_LOAD_GUID_PREFIX(objh.guid)==ASF_GUID_PREFIX_data_chunk) break; // movi chunk

  if(!stream_seek(demuxer->stream,endpos)) break;
} // while EOF

if(streams) {
  uint32_t vr = 0, ar = 0,i;
#ifdef STREAMING
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
  free(streams);
}

mp_msg(MSGT_HEADER,MSGL_V,"ASF: %d audio and %d video streams found\n",audio_streams,video_streams);
if(!audio_streams) demuxer->audio->id=-2;  // nosound
else if(best_audio > 0 && demuxer->audio->id == -1) demuxer->audio->id=best_audio;
if(!video_streams){
    if(!audio_streams){
	mp_msg(MSGT_HEADER,MSGL_ERR,"ASF: no audio or video headers found - broken file?\n");
	return 0; 
    }
    demuxer->video->id=-2; // audio-only
} else if (best_video > 0 && demuxer->video->id == -1) demuxer->video->id = best_video;

#if 0
if(verbose){
    printf("ASF duration: %d\n",(int)fileh.duration);
    printf("ASF start pts: %d\n",(int)fileh.start_timestamp);
    printf("ASF end pts: %d\n",(int)fileh.end_timestamp);
}
#endif

#endif
return 1;
}
