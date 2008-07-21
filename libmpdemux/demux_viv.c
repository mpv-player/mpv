//  VIVO file parser by A'rpi
//  VIVO text header parser and audio support by alex

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> /* strtok */

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

/* parameters ! */
int vivo_param_version = -1;
char *vivo_param_acodec = NULL;
int vivo_param_abitrate = -1;
int vivo_param_samplerate = -1;
int vivo_param_bytesperblock = -1;
int vivo_param_width = -1;
int vivo_param_height = -1;
int vivo_param_vformat = -1;

/* VIVO audio standards from vivog723.acm:

    G.723:
	FormatTag = 0x111
	Channels = 1 - mono
	SamplesPerSec = 8000 - 8khz
	AvgBytesPerSec = 800
	BlockAlign (bytes per block) = 24
	BitsPerSample = 8

    Siren:
	FormatTag = 0x112
	Channels = 1 - mono
	SamplesPerSec = 16000 - 16khz
	AvgBytesPerSec = 2000
	BlockAlign (bytes per block) = 40
	BitsPerSample = 8
*/

//enum { VIVO_AUDIO_G723, VIVO_AUDIO_SIREN };

#define VIVO_AUDIO_G723 1
#define VIVO_AUDIO_SIREN 2

typedef struct {
    /* generic */
    char	version;
    int		supported;
    /* info */
    char	*title;
    char	*author;
    char	*copyright;
    char	*producer;
    /* video */
    float	fps;
    int		width;
    int		height;
    int		disp_width;
    int		disp_height;
    /* audio */
    int		audio_codec;
    int		audio_bitrate;
    int		audio_samplerate;
    int		audio_bytesperblock;
} vivo_priv_t;

/* parse all possible extra headers */
/* (audio headers are separate - mostly with recordtype=3 or 4) */
#define TEXTPARSE_ALL 1

static void vivo_parse_text_header(demuxer_t *demux, int header_len)
{
    vivo_priv_t* priv = demux->priv;
    char *buf;
    int i;
    char *token;
    char *opt, *param;
    int parser_in_audio_block = 0;

    if (!demux->priv)
    {
	priv = malloc(sizeof(vivo_priv_t));
	memset(priv, 0, sizeof(vivo_priv_t));
	demux->priv = priv;
	priv->supported = 0;
    }

    buf = malloc(header_len);
    opt = malloc(header_len);
    param = malloc(header_len);
    stream_read(demux->stream, buf, header_len);
    i=0;
    while(i<header_len && buf[i]==0x0D && buf[i+1]==0x0A) i+=2; // skip empty lines
    
    token = strtok(buf, (char *)&("\x0d\x0a"));
    while (token && (header_len>2))
    {
	header_len -= strlen(token)+2;
	if (sscanf(token, "%[^:]:%[^\n]", opt, param) != 2)
	{
	    mp_msg(MSGT_DEMUX, MSGL_V, "viv_text_header_parser: bad line: '%s' at ~%#"PRIx64"\n",
		token, (int64_t)stream_tell(demux->stream));
	    break;
	}
	mp_dbg(MSGT_DEMUX, MSGL_DBG3, "token: '%s' (%d bytes/%d bytes left)\n",
	    token, strlen(token), header_len);
	mp_dbg(MSGT_DEMUX, MSGL_DBG3, "token => o: '%s', p: '%s'\n",
	    opt, param);

	/* checking versions: only v1 or v2 is suitable (or known?:) */
	if (!strcmp(opt, "Version"))
	{
	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "Version: %s\n", param);
	    if (!strncmp(param, "Vivo/1", 6) || !strncmp(param, "Vivo/2", 6))
	    {
		priv->supported = 1;
		/* save major version for fourcc */
		priv->version = param[5];
	    }
	}

	/* video specific */	
	if (!strcmp(opt, "FPS"))
	{
	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "FPS: %f\n", atof(param));
	    priv->fps = atof(param);
	}
	if (!strcmp(opt, "Width"))
	{
	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "Width: %d\n", atoi(param));
	    priv->width = atoi(param);
	}
	if (!strcmp(opt, "Height"))
	{
	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "Height: %d\n", atoi(param));
	    priv->height = atoi(param);
	}
	if (!strcmp(opt, "DisplayWidth"))
	{
	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "Display Width: %d\n", atoi(param));
	    priv->disp_width = atoi(param);
	}
	if (!strcmp(opt, "DisplayHeight"))
	{
	    mp_msg(MSGT_DEMUX, MSGL_DBG2, "Display Height: %d\n", atoi(param));
	    priv->disp_height = atoi(param);
	}

	/* audio specific */
	if (!strcmp(opt, "RecordType"))
	{
	    /* no audio recordblock by Vivo/1.00, 3 and 4 by Vivo/2.00 */
	    if ((atoi(param) == 3) || (atoi(param) == 4))
		parser_in_audio_block = 1;
	    else
		parser_in_audio_block = 0;
	}
	if (!strcmp(opt, "NominalBitrate"))
	{
	    priv->audio_bitrate = atoi(param);
	    if (priv->audio_bitrate == 2000)
		priv->audio_codec = VIVO_AUDIO_SIREN;
	    if (priv->audio_bitrate == 800)
		priv->audio_codec = VIVO_AUDIO_G723;
	}
	if (!strcmp(opt, "SamplingFrequency"))
	{
	    priv->audio_samplerate = atoi(param);
	    if (priv->audio_samplerate == 16000)
		priv->audio_codec = VIVO_AUDIO_SIREN;
	    if (priv->audio_samplerate == 8000)
		priv->audio_codec = VIVO_AUDIO_G723;
	}
	if (!strcmp(opt, "Length") && (parser_in_audio_block == 1))
	{
	    priv->audio_bytesperblock = atoi(param); /* 24 or 40 kbps */
	    if (priv->audio_bytesperblock == 40)
		priv->audio_codec = VIVO_AUDIO_SIREN;
	    if (priv->audio_bytesperblock == 24)
		priv->audio_codec = VIVO_AUDIO_G723;
	}
	
	/* only for displaying some informations about movie*/
	if (!strcmp(opt, "Title"))
	{
	    demux_info_add(demux, "name", param);
	    priv->title = strdup(param);
	}
	if (!strcmp(opt, "Author"))
	{
	    demux_info_add(demux, "author", param);
	    priv->author = strdup(param);
	}
	if (!strcmp(opt, "Copyright"))
	{
	    demux_info_add(demux, "copyright", param);
	    priv->copyright = strdup(param);
	}
	if (!strcmp(opt, "Producer"))
	{
	    demux_info_add(demux, "encoder", param);
	    priv->producer = strdup(param);
	}

	/* get next token */
	token = strtok(NULL, (char *)&("\x0d\x0a"));
    }
    
    if (buf)
	free(buf);
    if (opt)
	free(opt);
    if (param)
	free(param);
}

static int vivo_check_file(demuxer_t* demuxer){
    int i=0;
    int len;
    int c;
    unsigned char buf[2048+256];
    vivo_priv_t* priv;
    int orig_pos = stream_tell(demuxer->stream);
    
    mp_msg(MSGT_DEMUX,MSGL_V,"Checking for VIVO\n");
    
    c=stream_read_char(demuxer->stream);
    if(c==-256) return 0;
    len=0;
    while((c=stream_read_char(demuxer->stream))>=0x80){
	len+=0x80*(c-0x80);
	if(len>1024) return 0;
    }
    len+=c;
    mp_msg(MSGT_DEMUX,MSGL_V,"header block 1 size: %d\n",len);
    //stream_skip(demuxer->stream,len);

    priv=malloc(sizeof(vivo_priv_t));
    memset(priv,0,sizeof(vivo_priv_t));
    demuxer->priv=priv;

#if 0
    vivo_parse_text_header(demuxer, len);
    if (priv->supported == 0)
	return 0;
#else
    /* this is enought for check (for now) */
    stream_read(demuxer->stream,buf,len);
    buf[len]=0;
//    printf("VIVO header: '%s'\n",buf);

    // parse header:
    i=0;
    while(i<len && buf[i]==0x0D && buf[i+1]==0x0A) i+=2; // skip empty lines
    if(strncmp(buf+i,"Version:Vivo/",13)) return 0; // bad version/type!
#endif

#if 0
    c=stream_read_char(demuxer->stream);
    if(c) return 0;
    len2=0;
    while((c=stream_read_char(demuxer->stream))>=0x80){
	len2+=0x80*(c-0x80);
	if(len+len2>2048) return 0;
    }
    len2+=c;
    mp_msg(MSGT_DEMUX,MSGL_V,"header block 2 size: %d\n",len2);
    stream_skip(demuxer->stream,len2);
//    stream_read(demuxer->stream,buf+len,len2);
#endif
    
//    c=stream_read_char(demuxer->stream);
//    printf("first packet: %02X\n",c);

    stream_seek(demuxer->stream, orig_pos);

return DEMUXER_TYPE_VIVO;
}

static int audio_pos=0;
static int audio_rate=0;

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int demux_vivo_fill_buffer(demuxer_t *demux, demux_stream_t *dsds){
  demux_stream_t *ds=NULL;
  int c;
  int len=0;
  int seq;
  int prefix=0;
  demux->filepos=stream_tell(demux->stream);
  
  c=stream_read_char(demux->stream);
  if (c == -256) /* EOF */
    return 0;
//  printf("c=%x,%02X\n",c,c&0xf0);
  if (c == 0x82)
  {
      /* ok, this works, but pts calculating from header is required! */
#warning "Calculate PTS from picture header!"
      prefix = 1;
      c = stream_read_char(demux->stream);
      mp_msg(MSGT_DEMUX, MSGL_V, "packet 0x82(pos=%u) chunk=%x\n",
        (int)stream_tell(demux->stream), c);
  }
  switch(c&0xF0){
  case 0x00: // header - skip it!
  {
      len=stream_read_char(demux->stream);
      if(len>=0x80) len=0x80*(len-0x80)+stream_read_char(demux->stream);
      mp_msg(MSGT_DEMUX, MSGL_V, "vivo extra header: %d bytes\n",len);
#ifdef TEXTPARSE_ALL
{
      int pos;
      /* also try to parse all headers */
      pos = stream_tell(demux->stream);
      vivo_parse_text_header(demux, len);
      stream_seek(demux->stream, pos);
}
#endif
      break;
  }
  case 0x10:  // video packet
      if (prefix == 1)
        len = stream_read_char(demux->stream);
      else
        len=128;
      ds=demux->video;
      break;
  case 0x20:  // video packet
      len=stream_read_char(demux->stream);
      ds=demux->video;
      break;
  case 0x30:  // audio packet
      if (prefix == 1)
        len = stream_read_char(demux->stream);
      else
        len=40;	/* 40kbps */
      ds=demux->audio;
      audio_pos+=len;
      break;
  case 0x40:  // audio packet
      if (prefix == 1)
        len = stream_read_char(demux->stream);
      else
        len=24;	/* 24kbps */
      ds=demux->audio;
      audio_pos+=len;
      break;
  default:
      mp_msg(MSGT_DEMUX,MSGL_WARN,"VIVO - unknown ID found: %02X at pos %"PRIu64" contact author!\n",
        c, (int64_t)stream_tell(demux->stream));
      return 0;
  }

//  printf("chunk=%x, len=%d\n", c, len);
  
  if(!ds || ds->id<-1){
      if(len) stream_skip(demux->stream,len);
      return 1;
  }
  
  seq=c&0x0F;

    if(ds->asf_packet){
      if(ds->asf_seq!=seq){
        // closed segment, finalize packet:
        ds_add_packet(ds,ds->asf_packet);
        ds->asf_packet=NULL;
//	printf("packet!\n");
      } else {
        // append data to it!
        demux_packet_t* dp=ds->asf_packet;
        if(dp->len + len + MP_INPUT_BUFFER_PADDING_SIZE < 0)
	    return 0;
        dp->buffer=realloc(dp->buffer,dp->len+len+MP_INPUT_BUFFER_PADDING_SIZE);
        memset(dp->buffer+dp->len+len, 0, MP_INPUT_BUFFER_PADDING_SIZE);
        //memcpy(dp->buffer+dp->len,data,len);
	stream_read(demux->stream,dp->buffer+dp->len,len);
        mp_dbg(MSGT_DEMUX,MSGL_DBG4,"data appended! %d+%d\n",dp->len,len);
        dp->len+=len;
        // we are ready now.
	if((c&0xF0)==0x20) --ds->asf_seq; // hack!
        return 1;
      }
    }
    // create new packet:
    { demux_packet_t* dp;
      dp=new_demux_packet(len);
      //memcpy(dp->buffer,data,len);
      stream_read(demux->stream,dp->buffer,len);
      dp->pts=audio_rate?((float)audio_pos/(float)audio_rate):0;
//      dp->flags=keyframe;
//      if(ds==demux->video) printf("ASF time: %8d  dur: %5d  \n",time,dur);
      dp->pos=demux->filepos;
      ds->asf_packet=dp;
      ds->asf_seq=seq;
      // we are ready now.
      return 1;
    }

}

static const short h263_format[8][2] = {
    { 0, 0 },
    { 128, 96 },
    { 176, 144 },
    { 352, 288 },
    { 704, 576 },
    { 1408, 1152 },
    { 320, 240 }   // ??????? or 240x180 (found in vivo2) ?
};

static unsigned char* buffer;
static int bufptr=0;
static int bitcnt=0;
static unsigned char buf=0;
static int format, width, height;

static unsigned int x_get_bits(int n){
    unsigned int x=0;
    while(n-->0){
	if(!bitcnt){
	    // fill buff
	    buf=buffer[bufptr++];
	    bitcnt=8;
	}
	//x=(x<<1)|(buf&1);buf>>=1;
	x=(x<<1)|(buf>>7);buf<<=1;
	--bitcnt;
    }
    return x;
}

#define get_bits(xxx,n) x_get_bits(n)
#define get_bits1(xxx) x_get_bits(1)
#define skip_bits(xxx,n) x_get_bits(n)
#define skip_bits1(xxx) x_get_bits(1)

/* most is hardcoded. should extend to handle all h263 streams */
static int h263_decode_picture_header(unsigned char *b_ptr)
{
//    int i;
        
//    for(i=0;i<16;i++) printf(" %02X",b_ptr[i]); printf("\n");
    
    buffer=b_ptr;
    bufptr=bitcnt=buf=0;

    /* picture header */
    if (get_bits(&s->gb, 22) != 0x20){
	mp_msg(MSGT_DEMUX, MSGL_FATAL, "bad picture header\n");
        return -1;
    }
    skip_bits(&s->gb, 8); /* picture timestamp */

    if (get_bits1(&s->gb) != 1){
	mp_msg(MSGT_DEMUX, MSGL_FATAL, "bad marker\n");
        return -1;	/* marker */
    }
    if (get_bits1(&s->gb) != 0){
	mp_msg(MSGT_DEMUX, MSGL_FATAL, "bad h263 id\n");
        return -1;	/* h263 id */
    }
    skip_bits1(&s->gb);	/* split screen off */
    skip_bits1(&s->gb);	/* camera  off */
    skip_bits1(&s->gb);	/* freeze picture release off */

    format = get_bits(&s->gb, 3);

    if (format != 7) {
        mp_msg(MSGT_DEMUX, MSGL_V, "h263_plus = 0  format = %d\n", format);
        /* H.263v1 */
        width = h263_format[format][0];
        height = h263_format[format][1];
	mp_msg(MSGT_DEMUX, MSGL_V, "%d x %d\n", width, height);
//        if (!width) return -1;

	mp_msg(MSGT_DEMUX, MSGL_V, "pict_type=%d\n", get_bits1(&s->gb));
	mp_msg(MSGT_DEMUX, MSGL_V, "unrestricted_mv=%d\n", get_bits1(&s->gb));
#if 1
	mp_msg(MSGT_DEMUX, MSGL_V, "SAC: %d\n", get_bits1(&s->gb));
	mp_msg(MSGT_DEMUX, MSGL_V, "advanced prediction mode: %d\n", get_bits1(&s->gb));
	mp_msg(MSGT_DEMUX, MSGL_V, "PB frame: %d\n", get_bits1(&s->gb));
#else
        if (get_bits1(&s->gb) != 0)
            return -1;	/* SAC: off */
        if (get_bits1(&s->gb) != 0)
            return -1;	/* advanced prediction mode: off */
        if (get_bits1(&s->gb) != 0)
            return -1;	/* not PB frame */
#endif
	mp_msg(MSGT_DEMUX, MSGL_V, "qscale=%d\n", get_bits(&s->gb, 5));
        skip_bits1(&s->gb);	/* Continuous Presence Multipoint mode: off */
    } else {
        mp_msg(MSGT_DEMUX, MSGL_V, "h263_plus = 1\n");
        /* H.263v2 */
        if (get_bits(&s->gb, 3) != 1){
	    mp_msg(MSGT_DEMUX, MSGL_FATAL, "H.263v2 A error\n");
            return -1;
	}
        if (get_bits(&s->gb, 3) != 6){ /* custom source format */
	    mp_msg(MSGT_DEMUX, MSGL_FATAL, "custom source format\n");
            return -1;
	}
        skip_bits(&s->gb, 12);
        skip_bits(&s->gb, 3);
	mp_msg(MSGT_DEMUX, MSGL_V, "pict_type=%d\n", get_bits(&s->gb, 3) + 1);
//        if (s->pict_type != I_TYPE &&
//            s->pict_type != P_TYPE)
//            return -1;
        skip_bits(&s->gb, 7);
        skip_bits(&s->gb, 4); /* aspect ratio */
        width = (get_bits(&s->gb, 9) + 1) * 4;
        skip_bits1(&s->gb);
        height = get_bits(&s->gb, 9) * 4;
	mp_msg(MSGT_DEMUX, MSGL_V, "%d x %d\n", width, height);
        //if (height == 0)
        //    return -1;
	mp_msg(MSGT_DEMUX, MSGL_V, "qscale=%d\n", get_bits(&s->gb, 5));
    }

    /* PEI */
    while (get_bits1(&s->gb) != 0) {
        skip_bits(&s->gb, 8);
    }
//    s->f_code = 1;
//    s->width = width;
//    s->height = height;
    return 0;
}



static demuxer_t* demux_open_vivo(demuxer_t* demuxer){
    vivo_priv_t* priv=demuxer->priv;

  if(!ds_fill_buffer(demuxer->video)){
    mp_msg(MSGT_DEMUX,MSGL_ERR,"VIVO: " MSGTR_MissingVideoStreamBug);
    return NULL;
  }

    audio_pos=0;
  
  h263_decode_picture_header(demuxer->video->buffer);
  
  if (vivo_param_version != -1)
    priv->version = '0' + vivo_param_version;

{		sh_video_t* sh=new_sh_video(demuxer,0);

		/* viv1, viv2 (for better codecs.conf) */    
		sh->format = mmioFOURCC('v', 'i', 'v', priv->version);
		if(!sh->fps)
		{
		    if (priv->fps)
			sh->fps=priv->fps;
		    else
			sh->fps=15.0f;
		}
		sh->frametime=1.0f/sh->fps;

		/* XXX: FIXME: can't scale image. */
		/* hotfix to disable: */
		priv->disp_width = priv->width;
		priv->disp_height = priv->height;

		if (vivo_param_width != -1)
		    priv->disp_width = priv->width = vivo_param_width;

		if (vivo_param_height != -1)
		    priv->disp_height = priv->height = vivo_param_height;
		
		if (vivo_param_vformat != -1)
		{
		    priv->disp_width = priv->width = h263_format[vivo_param_vformat][0];
		    priv->disp_height = priv->height = h263_format[vivo_param_vformat][1];
		}

		if (priv->disp_width)
		    sh->disp_w = priv->disp_width;
		else
		    sh->disp_w = width;
		if (priv->disp_height)
		    sh->disp_h = priv->disp_height;
		else
		    sh->disp_h = height;

		// emulate BITMAPINFOHEADER:
		sh->bih=malloc(sizeof(BITMAPINFOHEADER));
		memset(sh->bih,0,sizeof(BITMAPINFOHEADER));
		sh->bih->biSize=40;
		if (priv->width)
		    sh->bih->biWidth = priv->width;
		else
		    sh->bih->biWidth = width;
		if (priv->height)
		    sh->bih->biHeight = priv->height;
		else
		    sh->bih->biHeight = height;
		sh->bih->biPlanes=1;
		sh->bih->biBitCount=24;
		sh->bih->biCompression=sh->format;
		sh->bih->biSizeImage=sh->bih->biWidth*sh->bih->biHeight*3;

		/* insert as stream */
		demuxer->video->sh=sh;
		sh->ds=demuxer->video;
		demuxer->video->id=0;
		
		/* disable seeking */
		demuxer->seekable = 0;

		mp_msg(MSGT_DEMUX,MSGL_V,"VIVO Video stream %d size: display: %dx%d, codec: %ux%u\n",
		    demuxer->video->id, sh->disp_w, sh->disp_h, sh->bih->biWidth,
		    sh->bih->biHeight);
}

/* AUDIO init */
if (demuxer->audio->id >= -1){
  if(!ds_fill_buffer(demuxer->audio)){
    mp_msg(MSGT_DEMUX,MSGL_ERR,"VIVO: " MSGTR_MissingAudioStream);
  } else
{		sh_audio_t* sh=new_sh_audio(demuxer,1);

		/* Select audio codec */
		if (priv->audio_codec == 0)
		{
		    if (priv->version == '2')
			priv->audio_codec = VIVO_AUDIO_SIREN;
		    else
			priv->audio_codec = VIVO_AUDIO_G723;
		}
		if (vivo_param_acodec != NULL)
		{
		    if (!strcasecmp(vivo_param_acodec, "g723"))
			priv->audio_codec = VIVO_AUDIO_G723;
		    if (!strcasecmp(vivo_param_acodec, "siren"))
			priv->audio_codec = VIVO_AUDIO_SIREN;
		}

		if (priv->audio_codec == VIVO_AUDIO_G723)
		    sh->format = 0x111;
		else if (priv->audio_codec == VIVO_AUDIO_SIREN)
		    sh->format = 0x112;
		else
		{
		    mp_msg(MSGT_DEMUX, MSGL_ERR, "VIVO: Not support audio codec (%d)\n",
			priv->audio_codec);
		    free_sh_audio(demuxer, 1);
		    goto nosound;
		}

		// Emulate WAVEFORMATEX struct:
		sh->wf=malloc(sizeof(WAVEFORMATEX));
		memset(sh->wf,0,sizeof(WAVEFORMATEX));
		sh->wf->wFormatTag=sh->format;
		sh->wf->nChannels=1; /* 1 channels for both Siren and G.723 */

		/* Set bits per sample */
		if (priv->audio_codec == VIVO_AUDIO_SIREN)
		    sh->wf->wBitsPerSample = 16;
		else
		if (priv->audio_codec == VIVO_AUDIO_G723)
		    sh->wf->wBitsPerSample = 8;

		/* Set sampling rate */
		if (priv->audio_samplerate) /* got from header */
		    sh->wf->nSamplesPerSec = priv->audio_samplerate;
		else
		{
		    if (priv->audio_codec == VIVO_AUDIO_SIREN)
			sh->wf->nSamplesPerSec = 16000;
		    if (priv->audio_codec == VIVO_AUDIO_G723)
			sh->wf->nSamplesPerSec = 8000;
		}
		if (vivo_param_samplerate != -1)
		    sh->wf->nSamplesPerSec = vivo_param_samplerate;

		/* Set audio bitrate */
		if (priv->audio_bitrate) /* got from header */
		    sh->wf->nAvgBytesPerSec = priv->audio_bitrate;
		else
		{
		    if (priv->audio_codec == VIVO_AUDIO_SIREN)
			sh->wf->nAvgBytesPerSec = 2000;
		    if (priv->audio_codec == VIVO_AUDIO_G723)
			sh->wf->nAvgBytesPerSec = 800;
		}
		if (vivo_param_abitrate != -1)
		    sh->wf->nAvgBytesPerSec = vivo_param_abitrate;
		audio_rate=sh->wf->nAvgBytesPerSec;

		if (!priv->audio_bytesperblock)
		{
		    if (priv->audio_codec == VIVO_AUDIO_SIREN)
			sh->wf->nBlockAlign = 40;
		    if (priv->audio_codec == VIVO_AUDIO_G723)
			sh->wf->nBlockAlign = 24;
		}
		else
		    sh->wf->nBlockAlign = priv->audio_bytesperblock;
		if (vivo_param_bytesperblock != -1)
		    sh->wf->nBlockAlign = vivo_param_bytesperblock;
		
/*sound_ok:*/
		/* insert as stream */
		demuxer->audio->sh=sh;
		sh->ds=demuxer->audio;
		demuxer->audio->id=1;
nosound:
		return demuxer;
}
}
    return demuxer;
}

static void demux_close_vivo(demuxer_t *demuxer)
{
    vivo_priv_t* priv=demuxer->priv;
 
    if (priv) {
	if (priv->title)
	    free(priv->title);
        if (priv->author)
	    free(priv->author);
	if (priv->copyright)
	    free(priv->copyright);
	if (priv->producer)
	   free(priv->producer);
	free(priv);
    }
    return;
}


const demuxer_desc_t demuxer_desc_vivo = {
  "Vivo demuxer",
  "vivo",
  "VIVO",
  "A'rpi, Alex Beregszasi",
  "",
  DEMUXER_TYPE_VIVO,
  0, // unsafe autodetect
  vivo_check_file,
  demux_vivo_fill_buffer,
  demux_open_vivo,
  demux_close_vivo,
  NULL,
  NULL
};
