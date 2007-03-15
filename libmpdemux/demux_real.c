/*
    Real parser & demuxer
    
    (C) Alex Beregszaszi
    (C) 2005, 2006 Roberto Togni
    
    Based on FFmpeg's libav/rm.c.

Audio codecs: (supported by RealPlayer8 for Linux)
    DNET - RealAudio 3.0, really it's AC3 in swapped-byteorder
    SIPR - SiproLab's audio codec, ACELP decoder working with MPlayer,
	   needs fine-tuning too :)
    ATRC - RealAudio 8 (ATRAC3) - www.minidisc.org/atrac3_article.pdf,
           ACM decoder uploaded, needs some fine-tuning to work
	   -> RealAudio 8
    COOK/COKR - Real Cooker -> RealAudio G2

Video codecs: (supported by RealPlayer8 for Linux)
    RV10 - H.263 based, working with libavcodec's decoder
    RV20-RV40 - using RealPlayer's codec plugins
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#ifdef USE_LIBAVCODEC_SO
#include <ffmpeg/avcodec.h>
#elif defined(USE_LIBAVCODEC)
#include "libavcodec/avcodec.h"
#else
#define FF_INPUT_BUFFER_PADDING_SIZE 8
#endif

//#define mp_dbg(mod,lev, args... ) mp_msg_c((mod<<8)|lev, ## args )

#define MKTAG(a, b, c, d) (a | (b << 8) | (c << 16) | (d << 24))

#define MAX_STREAMS 32

static unsigned char sipr_swaps[38][2]={
    {0,63},{1,22},{2,44},{3,90},{5,81},{7,31},{8,86},{9,58},{10,36},{12,68},
    {13,39},{14,73},{15,53},{16,69},{17,57},{19,88},{20,34},{21,71},{24,46},
    {25,94},{26,54},{28,75},{29,50},{32,70},{33,92},{35,74},{38,85},{40,56},
    {42,87},{43,65},{45,59},{48,79},{49,93},{51,89},{55,95},{61,76},{67,83},
    {77,80} };

typedef struct {
    unsigned int	timestamp;
    int		offset;
//    int		packetno;
//    int		len; /* only filled by our index generator */
//    int		flags; /* only filled by our index generator */
} real_index_table_t;

typedef struct {
    /* for seeking */
    int		index_chunk_offset;
    real_index_table_t *index_table[MAX_STREAMS];
	
//    int		*index_table[MAX_STREAMS];
    int		index_table_size[MAX_STREAMS];
    int		index_malloc_size[MAX_STREAMS];
    int		data_chunk_offset;
    int		num_of_packets;
    int		current_packet;
	
// need for seek
    int		audio_need_keyframe;
    int		video_after_seek;

    int		current_apacket;
    int		current_vpacket;
    
    // timestamp correction:
    int64_t		kf_base;// timestamp of the prev. video keyframe
    unsigned int	kf_pts;	// timestamp of next video keyframe
    unsigned int	a_pts;	// previous audio timestamp
    double	v_pts;  // previous video timestamp
    unsigned long	duration;
    
    /* stream id table */
//    int		last_a_stream;
//    int 	a_streams[MAX_STREAMS];
//    int		last_v_stream;
//    int 	v_streams[MAX_STREAMS];

    /**
     * Used to demux multirate files
     */
    int is_multirate; ///< != 0 for multirate files
    int str_data_offset[MAX_STREAMS]; ///< Data chunk offset for every audio/video stream
    int audio_curpos; ///< Current file position for audio demuxing
    int video_curpos; ///< Current file position for video demuxing
    int a_num_of_packets; ///< Number of audio packets
    int v_num_of_packets; ///< Number of video packets
    int a_idx_ptr; ///< Audio index position pointer
    int v_idx_ptr; ///< Video index position pointer
    int a_bitrate; ///< Audio bitrate
    int v_bitrate; ///< Video bitrate
    int stream_switch; ///< Flag used to switch audio/video demuxing

   /**
    * Used to reorder audio data
    */
    unsigned int intl_id[MAX_STREAMS]; ///< interleaver id, per stream
    int sub_packet_size[MAX_STREAMS]; ///< sub packet size, per stream
    int sub_packet_h[MAX_STREAMS]; ///< number of coded frames per block
    int coded_framesize[MAX_STREAMS]; ///< coded frame size, per stream
    int audiopk_size[MAX_STREAMS]; ///< audio packet size
    unsigned char *audio_buf; ///< place to store reordered audio data
    double *audio_timestamp; ///< timestamp for each audio packet
    int sub_packet_cnt; ///< number of subpacket already received
    int audio_filepos; ///< file position of first audio packet in block
} real_priv_t;

//! use at most 200 MB of memory for index, corresponds to around 25 million entries
#define MAX_INDEX_ENTRIES (200*1024*1024 / sizeof(real_index_table_t))

/* originally from FFmpeg */
static void get_str(int isbyte, demuxer_t *demuxer, char *buf, int buf_size)
{
    int len;
    
    if (isbyte)
	len = stream_read_char(demuxer->stream);
    else
	len = stream_read_word(demuxer->stream);

    stream_read(demuxer->stream, buf, (len > buf_size) ? buf_size : len);
    if (len > buf_size)
	stream_skip(demuxer->stream, len-buf_size);

    mp_msg(MSGT_DEMUX, MSGL_V, "read_str: %d bytes read\n", len);
}

static void skip_str(int isbyte, demuxer_t *demuxer)
{
    int len;

    if (isbyte)
	len = stream_read_char(demuxer->stream);
    else
	len = stream_read_word(demuxer->stream);

    stream_skip(demuxer->stream, len);    

    mp_msg(MSGT_DEMUX, MSGL_V, "skip_str: %d bytes skipped\n", len);
}

static void dump_index(demuxer_t *demuxer, int stream_id)
{
    real_priv_t *priv = demuxer->priv;
    real_index_table_t *index;
    int i, entries;

    if ( mp_msg_test(MSGT_DEMUX,MSGL_V) )
	return;
    
    if ((unsigned)stream_id >= MAX_STREAMS)
	return;

    index = priv->index_table[stream_id];
    entries = priv->index_table_size[stream_id];
    
    mp_msg(MSGT_DEMUX, MSGL_V, "Index table for stream %d\n", stream_id);
    for (i = 0; i < entries; i++)
    {
#if 1
	mp_msg(MSGT_DEMUX, MSGL_V,"i: %d, pos: %d, timestamp: %u\n", i, index[i].offset, index[i].timestamp);
#else
	mp_msg(MSGT_DEMUX, MSGL_V,"packetno: %x pos: %x len: %x timestamp: %x flags: %x\n",
	    index[i].packetno, index[i].offset, index[i].len, index[i].timestamp,
	    index[i].flags);
#endif
    }
}

static int parse_index_chunk(demuxer_t *demuxer)
{
    real_priv_t *priv = demuxer->priv;
    int origpos = stream_tell(demuxer->stream);
    int next_header_pos = priv->index_chunk_offset;
    int i, entries, stream_id;

read_index:
    stream_seek(demuxer->stream, next_header_pos);

    i = stream_read_dword_le(demuxer->stream);
    if ((i == -256) || (i != MKTAG('I', 'N', 'D', 'X')))
    {
	mp_msg(MSGT_DEMUX, MSGL_WARN,"Something went wrong, no index chunk found on given address (%d)\n",
	    next_header_pos);
	index_mode = -1;
        if (i == -256)
	    stream_reset(demuxer->stream);
    	stream_seek(demuxer->stream, origpos);
	return 0;
	//goto end;
    }

    mp_msg(MSGT_DEMUX, MSGL_V,"Reading index table from index chunk (%d)\n",
	next_header_pos);

    i = stream_read_dword(demuxer->stream);
    mp_msg(MSGT_DEMUX, MSGL_V,"size: %d bytes\n", i);

    i = stream_read_word(demuxer->stream);
    if (i != 0)
	mp_msg(MSGT_DEMUX, MSGL_WARN,"Hmm, index table with unknown version (%d), please report it to MPlayer developers!\n", i);

    entries = stream_read_dword(demuxer->stream);
    mp_msg(MSGT_DEMUX, MSGL_V,"entries: %d\n", entries);
    
    stream_id = stream_read_word(demuxer->stream);
    mp_msg(MSGT_DEMUX, MSGL_V,"stream_id: %d\n", stream_id);
    
    next_header_pos = stream_read_dword(demuxer->stream);
    mp_msg(MSGT_DEMUX, MSGL_V,"next_header_pos: %d\n", next_header_pos);
    if (entries <= 0 || entries > MAX_INDEX_ENTRIES)
    {
	if (next_header_pos)
	    goto read_index;
	i = entries;
	goto end;
    }

    priv->index_table_size[stream_id] = entries;
    priv->index_table[stream_id] = calloc(priv->index_table_size[stream_id], sizeof(real_index_table_t));
    
    for (i = 0; i < entries; i++)
    {
	stream_skip(demuxer->stream, 2); /* version */
	priv->index_table[stream_id][i].timestamp = stream_read_dword(demuxer->stream);
	priv->index_table[stream_id][i].offset = stream_read_dword(demuxer->stream);
	stream_skip(demuxer->stream, 4); /* packetno */
//	priv->index_table[stream_id][i].packetno = stream_read_dword(demuxer->stream);
//	printf("Index table: Stream#%d: entry: %d: pos: %d\n",
//	    stream_id, i, priv->index_table[stream_id][i].offset);
    }
    
    dump_index(demuxer, stream_id);

    if (next_header_pos > 0)
	goto read_index;

end:
    if (i == -256)
	stream_reset(demuxer->stream);
    stream_seek(demuxer->stream, origpos);
    if (i == -256)
	return 0;
    else
	return 1;
}

#if 1

static void add_index_item(demuxer_t *demuxer, int stream_id, unsigned int timestamp, int offset)
{
  if ((unsigned)stream_id < MAX_STREAMS)
  {
    real_priv_t *priv = demuxer->priv;
    real_index_table_t *index;
    if (priv->index_table_size[stream_id] >= MAX_INDEX_ENTRIES) {
      mp_msg(MSGT_DEMUXER, MSGL_WARN, "Index too large during building\n");
      return;
    }
    if (priv->index_table_size[stream_id] >= priv->index_malloc_size[stream_id])
    {
      if (priv->index_malloc_size[stream_id] == 0)
	priv->index_malloc_size[stream_id] = 2048;
      else
	priv->index_malloc_size[stream_id] += priv->index_malloc_size[stream_id] / 2;
      // in case we have a really large chunk...
      if (priv->index_table_size[stream_id] >=
            priv->index_malloc_size[stream_id])
        priv->index_malloc_size[stream_id] =
          priv->index_table_size[stream_id] + 1;
      priv->index_table[stream_id] = realloc(priv->index_table[stream_id], priv->index_malloc_size[stream_id]*sizeof(priv->index_table[0][0]));
    }
    if (priv->index_table_size[stream_id] > 0)
    {
      index = &priv->index_table[stream_id][priv->index_table_size[stream_id] - 1];
      if (index->timestamp >= timestamp || index->offset >= offset)
	return;
    }
    index = &priv->index_table[stream_id][priv->index_table_size[stream_id]++];
    index->timestamp = timestamp;
    index->offset = offset;
  }
}

static void add_index_segment(demuxer_t *demuxer, int seek_stream_id, int64_t seek_timestamp)
{
  int tag, len, stream_id, flags;
  unsigned int timestamp;
  if (seek_timestamp != -1 && (unsigned)seek_stream_id >= MAX_STREAMS)
    return;
  while (1)
  {
    demuxer->filepos = stream_tell(demuxer->stream);
    
    tag = stream_read_dword(demuxer->stream);
    if (tag == MKTAG('A', 'T', 'A', 'D'))
    {
      stream_skip(demuxer->stream, 14);
      continue; /* skip to next loop */
    }
    len = tag & 0xffff;
    if (tag == -256 || len < 12)
      break;
    
    stream_id = stream_read_word(demuxer->stream);
    timestamp = stream_read_dword(demuxer->stream);
    
    stream_skip(demuxer->stream, 1); /* reserved */
    flags = stream_read_char(demuxer->stream);
    
    if (flags == -256)
      break;
    
    if (flags & 2)
    {
      add_index_item(demuxer, stream_id, timestamp, demuxer->filepos);
      if (stream_id == seek_stream_id && timestamp >= seek_timestamp)
      {
	stream_seek(demuxer->stream, demuxer->filepos);
	return;
      }
    }
    // printf("Index: stream=%d packet=%d timestamp=%u len=%d flags=0x%x datapos=0x%x\n", stream_id, entries, timestamp, len, flags, index->offset);
    /* skip data */
    stream_skip(demuxer->stream, len-12);
  }
}

static int generate_index(demuxer_t *demuxer)
{
  real_priv_t *priv = demuxer->priv;
  int origpos = stream_tell(demuxer->stream);
  int data_pos = priv->data_chunk_offset-10;
  int i;
  int tag;
  
  stream_seek(demuxer->stream, data_pos);
  tag = stream_read_dword(demuxer->stream);
  if (tag != MKTAG('A', 'T', 'A', 'D'))
  {
    mp_msg(MSGT_DEMUX, MSGL_WARN,"Something went wrong, no data chunk found on given address (%d)\n", data_pos);
  }
  else
  {
    stream_skip(demuxer->stream, 14);
    add_index_segment(demuxer, -1, -1);
  }
  for (i = 0; i < MAX_STREAMS; i++)
  {
    if (priv->index_table_size[i] > 0)
    {
      dump_index(demuxer, i);
    }
  }
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, origpos);
  return 0;
}

#else

static int generate_index(demuxer_t *demuxer)
{
    real_priv_t *priv = demuxer->priv;
    int origpos = stream_tell(demuxer->stream);
    int data_pos = priv->data_chunk_offset-10;
    int num_of_packets = 0;
    int i, entries = 0;
    int len, stream_id = 0, flags;
    unsigned int timestamp;
    int tab_pos = 0;

read_index:
    stream_seek(demuxer->stream, data_pos);

    i = stream_read_dword_le(demuxer->stream);
    if ((i == -256) || (i != MKTAG('D', 'A', 'T', 'A')))
    {
	mp_msg(MSGT_DEMUX, MSGL_WARN,"Something went wrong, no data chunk found on given address (%d)\n",
	    data_pos);
	goto end;
    }
    stream_skip(demuxer->stream, 4); /* chunk size */
    stream_skip(demuxer->stream, 2); /* version */
    
    num_of_packets = stream_read_dword(demuxer->stream);
    mp_msg(MSGT_DEMUX, MSGL_V,"Generating index table from raw data (pos: 0x%x) for %d packets\n",
	data_pos, num_of_packets);

    data_pos = stream_read_dword_le(demuxer->stream)-10; /* next data chunk */

    for (i = 0; i < MAX_STREAMS; i++)
    {
    priv->index_table_size[i] = num_of_packets;
    priv->index_table[i] = calloc(priv->index_table_size[i], sizeof(real_index_table_t));
//    priv->index_table[stream_id] = realloc(priv->index_table[stream_id],
//	priv->index_table_size[stream_id] * sizeof(real_index_table_t));
    }

    tab_pos = 0;
    
//    memset(priv->index_table_size, 0, sizeof(int)*MAX_STREAMS);
//    memset(priv->index_table, 0, sizeof(real_index_table_t)*MAX_STREAMS);
    
    while (tab_pos < num_of_packets)
    {
    i = stream_read_char(demuxer->stream);
    if (i == -256)
	goto end;
    stream_skip(demuxer->stream, 1);
//    stream_skip(demuxer->stream, 2); /* version */

    len = stream_read_word(demuxer->stream);
    stream_id = stream_read_word(demuxer->stream);
    timestamp = stream_read_dword(demuxer->stream);
    
    stream_skip(demuxer->stream, 1); /* reserved */
    flags = stream_read_char(demuxer->stream);

    i = tab_pos;

//    priv->index_table_size[stream_id] = i;
//    if (priv->index_table[stream_id] == NULL)
//	priv->index_table[stream_id] = malloc(priv->index_table_size[stream_id] * sizeof(real_index_table_t));
//    else
//	priv->index_table[stream_id] = realloc(priv->index_table[stream_id],
//	    priv->index_table_size[stream_id] * sizeof(real_index_table_t));
    
    priv->index_table[stream_id][i].timestamp = timestamp;
    priv->index_table[stream_id][i].offset = stream_tell(demuxer->stream)-12;
    priv->index_table[stream_id][i].len = len;
    priv->index_table[stream_id][i].packetno = entries;
    priv->index_table[stream_id][i].flags = flags;

    tab_pos++;

    /* skip data */
    stream_skip(demuxer->stream, len-12);
    }
    dump_index(demuxer, stream_id);
    if (data_pos)
	goto read_index;

end:
    if (i == -256)
	stream_reset(demuxer->stream);
    stream_seek(demuxer->stream, origpos);
    if (i == -256)
	return 0;
    else
	return 1;
}
#endif


static int real_check_file(demuxer_t* demuxer)
{
    real_priv_t *priv;
    int c;

    mp_msg(MSGT_DEMUX,MSGL_V,"Checking for REAL\n");
    
    c = stream_read_dword_le(demuxer->stream);
    if (c == -256)
	return 0; /* EOF */
    if (c != MKTAG('.', 'R', 'M', 'F'))
	return 0; /* bad magic */

    priv = malloc(sizeof(real_priv_t));
    memset(priv, 0, sizeof(real_priv_t));
    demuxer->priv = priv;

    return DEMUXER_TYPE_REAL;
}

void hexdump(char *, unsigned long);

#define SKIP_BITS(n) buffer<<=n
#define SHOW_BITS(n) ((buffer)>>(32-(n)))

static double real_fix_timestamp(real_priv_t* priv, unsigned char* s, unsigned int timestamp, double frametime, unsigned int format){
  double v_pts;
  uint32_t buffer= (s[0]<<24) + (s[1]<<16) + (s[2]<<8) + s[3];
  unsigned int kf=timestamp;
  int pict_type;
  unsigned int orig_kf;

#if 1
  if(format==mmioFOURCC('R','V','3','0') || format==mmioFOURCC('R','V','4','0')){
    if(format==mmioFOURCC('R','V','3','0')){
      SKIP_BITS(3);
      pict_type= SHOW_BITS(2);
      SKIP_BITS(2 + 7);
    }else{
      SKIP_BITS(1);
      pict_type= SHOW_BITS(2);
      SKIP_BITS(2 + 7 + 3);
    }
    orig_kf=
    kf= SHOW_BITS(13);  //    kf= 2*SHOW_BITS(12);
//    if(pict_type==0)
    if(pict_type<=1){
      // I frame, sync timestamps:
      priv->kf_base=(int64_t)timestamp-kf;
      mp_msg(MSGT_DEMUX, MSGL_DBG2,"\nTS: base=%08X\n",priv->kf_base);
      kf=timestamp;
    } else {
      // P/B frame, merge timestamps:
      int64_t tmp=(int64_t)timestamp-priv->kf_base;
      kf|=tmp&(~0x1fff);	// combine with packet timestamp
      if(kf<tmp-4096) kf+=8192; else // workaround wrap-around problems
      if(kf>tmp+4096) kf-=8192;
      kf+=priv->kf_base;
    }
    if(pict_type != 3){ // P || I  frame -> swap timestamps
	unsigned int tmp=kf;
	kf=priv->kf_pts;
	priv->kf_pts=tmp;
//	if(kf<=tmp) kf=0;
    }
    mp_msg(MSGT_DEMUX, MSGL_DBG2,"\nTS: %08X -> %08X (%04X) %d %02X %02X %02X %02X %5u\n",timestamp,kf,orig_kf,pict_type,s[0],s[1],s[2],s[3],kf-(unsigned int)(1000.0*priv->v_pts));
  }
#endif
    v_pts=kf*0.001f;
//    if(v_pts<priv->v_pts || !kf) v_pts=priv->v_pts+frametime;
    priv->v_pts=v_pts;
    return v_pts;
}

typedef struct dp_hdr_s {
    uint32_t chunks;	// number of chunks
    uint32_t timestamp; // timestamp from packet header
    uint32_t len;	// length of actual data
    uint32_t chunktab;	// offset to chunk offset array
} dp_hdr_t;

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int demux_real_fill_buffer(demuxer_t *demuxer, demux_stream_t *dsds)
{
    real_priv_t *priv = demuxer->priv;
    demux_stream_t *ds = NULL;
    int len;
    unsigned int timestamp;
    int stream_id;
#ifdef CRACK_MATRIX
    int i;
#endif
    int flags;
    int version;
    int reserved;
    demux_packet_t *dp;
    int x, sps, cfs, sph, spc, w;
    int audioreorder_getnextpk = 0;

  // Don't demux video if video codec init failed
  if (demuxer->video->id >= 0 && !demuxer->video->sh)
    demuxer->video->id = -2;

  while(!stream_eof(demuxer->stream)){

    /* Handle audio/video demxing switch for multirate files (non-interleaved) */
    if (priv->is_multirate && priv->stream_switch) {
        if (priv->a_idx_ptr >= priv->index_table_size[demuxer->audio->id])
            demuxer->audio->eof = 1;
        if (priv->v_idx_ptr >= priv->index_table_size[demuxer->video->id])
            demuxer->video->eof = 1;
        if (demuxer->audio->eof && demuxer->video->eof)
            return 0;
        else if (!demuxer->audio->eof && demuxer->video->eof)
            stream_seek(demuxer->stream, priv->audio_curpos); // Get audio
        else if (demuxer->audio->eof && !demuxer->video->eof)
            stream_seek(demuxer->stream, priv->video_curpos); // Get video
        else if (priv->index_table[demuxer->audio->id][priv->a_idx_ptr].timestamp <
            priv->index_table[demuxer->video->id][priv->v_idx_ptr].timestamp)
            stream_seek(demuxer->stream, priv->audio_curpos); // Get audio
        else
            stream_seek(demuxer->stream, priv->video_curpos); // Get video
        priv->stream_switch = 0;
    }

    demuxer->filepos = stream_tell(demuxer->stream);
    version = stream_read_word(demuxer->stream); /* version */
    len = stream_read_word(demuxer->stream);
    if ((version==0x4441) && (len==0x5441)) { // new data chunk
	mp_msg(MSGT_DEMUX,MSGL_INFO,"demux_real: New data chunk is coming!!!\n");
    if (priv->is_multirate)
        return 0; // EOF
	stream_skip(demuxer->stream,14); 
	demuxer->filepos = stream_tell(demuxer->stream);
        version = stream_read_word(demuxer->stream); /* version */
	len = stream_read_word(demuxer->stream);	
    } else if ((version == 0x494e) && (len == 0x4458)) {
        mp_msg(MSGT_DEMUX,MSGL_V,"demux_real: Found INDX chunk. EOF.\n");
        demuxer->stream->eof=1;
        return 0;
    }

    
    if (len == -256){ /* EOF */
//	printf("len==-256!\n");
	return 0;
    }
    if (len < 12){
	mp_msg(MSGT_DEMUX, MSGL_V,"%08X: packet v%d len=%d  \n",(int)demuxer->filepos,(int)version,(int)len);
	mp_msg(MSGT_DEMUX, MSGL_WARN,"bad packet len (%d)\n", len);
	stream_skip(demuxer->stream, len);
	continue; //goto loop;
    }

    stream_id = stream_read_word(demuxer->stream);
    timestamp = stream_read_dword(demuxer->stream);
    reserved = stream_read_char(demuxer->stream);
    flags = stream_read_char(demuxer->stream);
    /* flags:		*/
    /*  0x1 - reliable  */
    /* 	0x2 - keyframe	*/

    if (version == 1) {
        int tmp;
        tmp = stream_read_char(demuxer->stream);
        mp_msg(MSGT_DEMUX, MSGL_DBG2,"Version: %d, skipped byte is %d\n", version, tmp);
        len--;
    }

    if (flags & 2)
      add_index_item(demuxer, stream_id, timestamp, demuxer->filepos);

//    printf("%08X: packet v%d len=%4d  id=%d  pts=%6d  rvd=%d  flags=%d  \n",
//	(int)demuxer->filepos,(int)version,(int)len, stream_id,
//	(int) timestamp, reserved, flags);

    mp_dbg(MSGT_DEMUX,MSGL_DBG2,  "\npacket#%d: pos: 0x%0x, len: %d, id: %d, pts: %u, flags: %x rvd:%d\n",
	priv->current_packet, (int)demuxer->filepos, len, stream_id, timestamp, flags, reserved);

    priv->current_packet++;
    len -= 12;    

//    printf("s_id=%d  aid=%d  vid=%d  \n",stream_id,demuxer->audio->id,demuxer->video->id);

    /* check stream_id: */

    if(demuxer->audio->id==stream_id){
    	if (priv->audio_need_keyframe == 1&& flags != 0x2)
		goto discard;
got_audio:
	ds=demuxer->audio;
	mp_dbg(MSGT_DEMUX,MSGL_DBG2, "packet is audio (id: %d)\n", stream_id);

        if (flags & 2) {
    	    priv->sub_packet_cnt = 0;
    	    audioreorder_getnextpk = 0;
        }

	// parse audio chunk:
	{
#ifdef CRACK_MATRIX
	    int spos=stream_tell(demuxer->stream);
	    static int cnt=0;
	    static int cnt2=CRACK_MATRIX;
#endif
	    if (((sh_audio_t *)ds->sh)->format == mmioFOURCC('M', 'P', '4', 'A')) {
		uint16_t *sub_packet_lengths, sub_packets, i;
		/* AAC in Real: several AAC frames in one Real packet. */
		/* Second byte, upper four bits: number of AAC frames */
		/* next n * 2 bytes: length of the AAC frames in bytes, BE */
		sub_packets = (stream_read_word(demuxer->stream) & 0xf0) >> 4;
		sub_packet_lengths = calloc(sub_packets, sizeof(uint16_t));
		for (i = 0; i < sub_packets; i++)
		    sub_packet_lengths[i] = stream_read_word(demuxer->stream);
		for (i = 0; i < sub_packets; i++) {
		    demux_packet_t *dp = new_demux_packet(sub_packet_lengths[i]);
		    stream_read(demuxer->stream, dp->buffer, sub_packet_lengths[i]);
		    if (priv->a_pts != timestamp)
			dp->pts = timestamp / 1000.0;
		    priv->a_pts = timestamp;
		    dp->pos = demuxer->filepos;
		    ds_add_packet(ds, dp);
		}
		free(sub_packet_lengths);
		return 1;
	    }
        if ((priv->intl_id[stream_id] == mmioFOURCC('I', 'n', 't', '4')) ||
            (priv->intl_id[stream_id] == mmioFOURCC('g', 'e', 'n', 'r')) ||
            (priv->intl_id[stream_id] == mmioFOURCC('s', 'i', 'p', 'r'))) {
            sps = priv->sub_packet_size[stream_id];
            sph = priv->sub_packet_h[stream_id];
            cfs = priv->coded_framesize[stream_id];
            w = priv->audiopk_size[stream_id];
            spc = priv->sub_packet_cnt;
            switch (priv->intl_id[stream_id]) {
                case mmioFOURCC('I', 'n', 't', '4'):
                    for (x = 0; x < sph / 2; x++)
                        stream_read(demuxer->stream, priv->audio_buf + x * 2 * w + spc * cfs, cfs);
                    break;
                case mmioFOURCC('g', 'e', 'n', 'r'):
                    for (x = 0; x < w / sps; x++)
                        stream_read(demuxer->stream, priv->audio_buf + sps * (sph * x + ((sph + 1) / 2) * (spc & 1) +
                                    (spc >> 1)), sps);
                    break;
                case mmioFOURCC('s', 'i', 'p', 'r'):
                    stream_read(demuxer->stream, priv->audio_buf + spc * w, w);
                    if (spc == sph - 1) {
                        int n;
                        int bs = sph * w * 2 / 96;  // nibbles per subpacket
                        // Perform reordering
                        for(n=0; n < 38; n++) {
                            int j;
                            int i = bs * sipr_swaps[n][0];
                            int o = bs * sipr_swaps[n][1];
                            // swap nibbles of block 'i' with 'o'      TODO: optimize
                            for(j = 0;j < bs; j++) {
                                int x = (i & 1) ? (priv->audio_buf[i >> 1] >> 4) : (priv->audio_buf[i >> 1] & 0x0F);
                                int y = (o & 1) ? (priv->audio_buf[o >> 1] >> 4) : (priv->audio_buf[o >> 1] & 0x0F);
                                if(o & 1)
                                    priv->audio_buf[o >> 1] = (priv->audio_buf[o >> 1] & 0x0F) | (x << 4);
                                else
                                    priv->audio_buf[o >> 1] = (priv->audio_buf[o >> 1] & 0xF0) | x;
                                if(i & 1)
                                    priv->audio_buf[i >> 1] = (priv->audio_buf[i >> 1] & 0x0F) | (y << 4);
                                else
                                    priv->audio_buf[i >> 1] = (priv->audio_buf[i >> 1] & 0xF0) | y;
                                ++i; ++o;
                            }
                        }
                    }
                    break;
            }
            priv->audio_need_keyframe = 0;
            priv->audio_timestamp[priv->sub_packet_cnt] = (priv->a_pts==timestamp) ? (correct_pts ? MP_NOPTS_VALUE : 0) : (timestamp/1000.0);
            priv->a_pts = timestamp;
            if (priv->sub_packet_cnt == 0)
                priv->audio_filepos = demuxer->filepos;
            if (++(priv->sub_packet_cnt) < sph)
                audioreorder_getnextpk = 1;
            else {
                int apk_usize = ((WAVEFORMATEX*)((sh_audio_t*)ds->sh)->wf)->nBlockAlign;
                audioreorder_getnextpk = 0;
                priv->sub_packet_cnt = 0;
                // Release all the audio packets
                for (x = 0; x < sph*w/apk_usize; x++) {
                    dp = new_demux_packet(apk_usize);
                    memcpy(dp->buffer, priv->audio_buf + x * apk_usize, apk_usize);
                    /* Put timestamp only on packets that correspond to original audio packets in file */
		    if (x * apk_usize % w == 0)
			dp->pts = priv->audio_timestamp[x * apk_usize / w];
                    dp->pos = priv->audio_filepos; // all equal
                    dp->flags = x ? 0 : 0x10; // Mark first packet as keyframe
                    ds_add_packet(ds, dp);
                }
            }
        } else { // No interleaving
            dp = new_demux_packet(len);
            stream_read(demuxer->stream, dp->buffer, len);

#ifdef CRACK_MATRIX
	    mp_msg(MSGT_DEMUX, MSGL_V,"*** audio block len=%d\n",len);
	    { // HACK - used for reverse engineering the descrambling matrix
		FILE* f=fopen("test.rm","r+");
		fseek(f,spos,SEEK_SET);
		++cnt;
//		    for(i=0;i<len;i++) dp->buffer[i]=i/0x12;
//		    for(i=0;i<len;i++) dp->buffer[i]=i;
//		    for(i=0;i<len;i++) dp->buffer[i]=cnt;
//		    for(i=0;i<len;i++) dp->buffer[i]=cnt<<4;
		    for(i=0;i<len;i++) dp->buffer[i]=(i==cnt2) ? (cnt+16*(8+cnt)) : 0;
		if(cnt==6){ cnt=0; ++cnt2; }
		fwrite(dp->buffer, len, 1, f);
		fclose(f);
		if(cnt2>0x150) *((int*)NULL)=1; // sig11 :)
	    }
#endif
#if 0
	    if( ((sh_audio_t *)ds->sh)->format == 0x2000) {
		// if DNET, swap bytes, as DNET is byte-swapped AC3:
		char *ptr = dp->buffer;
		int i;
		for (i = 0; i < len; i += 2)
		{
		    const char tmp = ptr[0];
		    ptr[0] = ptr[1];
		    ptr[1] = tmp;
		    ptr += 2;
		}
	    }
#endif
	    if (priv->audio_need_keyframe == 1) {
		priv->audio_need_keyframe = 0;
	    } else if(priv->a_pts != timestamp)
	        dp->pts = timestamp/1000.0;
	    priv->a_pts=timestamp;
	    dp->pos = demuxer->filepos;
	    dp->flags = (flags & 0x2) ? 0x10 : 0;
	    ds_add_packet(ds, dp);

        } // codec_id check, codec default case
	}
// we will not use audio index if we use -idx and have a video
	if(!demuxer->video->sh && index_mode == 2 && (unsigned)demuxer->audio->id < MAX_STREAMS)
		while (priv->current_apacket + 1 < priv->index_table_size[demuxer->audio->id] &&
		       timestamp > priv->index_table[demuxer->audio->id][priv->current_apacket].timestamp)
			priv->current_apacket += 1;
	
	if(priv->is_multirate)
		while (priv->a_idx_ptr + 1 < priv->index_table_size[demuxer->audio->id] &&
		       timestamp > priv->index_table[demuxer->audio->id][priv->a_idx_ptr + 1].timestamp) {
			priv->a_idx_ptr++;
			priv->audio_curpos = stream_tell(demuxer->stream);
			priv->stream_switch = 1;
		}
	
    // If we're reordering audio packets and we need more data get it
    if (audioreorder_getnextpk)
        continue;

	return 1;
    }
    
    if(demuxer->video->id==stream_id){
got_video:
	ds=demuxer->video;
	mp_dbg(MSGT_DEMUX,MSGL_DBG2, "packet is video (id: %d)\n", stream_id);
	
	// parse video chunk:
	{
	    // we need a more complicated, 2nd level demuxing, as the video
	    // frames are stored fragmented in the video chunks :(
	    sh_video_t *sh_video = ds->sh;
	    demux_packet_t *dp;
	    unsigned vpkg_header, vpkg_length, vpkg_offset;
	    int vpkg_seqnum=-1;
	    int vpkg_subseq=0;

	    while(len>2){
		dp_hdr_t* dp_hdr;
		unsigned char* dp_data;
		uint32_t* extra;

//		printf("xxx len=%d  \n",len);

		// read packet header
		// bit 7: 1=last block in block chain
		// bit 6: 1=short header (only one block?)
		vpkg_header=stream_read_char(demuxer->stream); --len;
		mp_dbg(MSGT_DEMUX,MSGL_DBG2, "hdr: %02X (len=%d) ",vpkg_header,len);

		if (0x40==(vpkg_header&0xc0)) {
		    // seems to be a very short header
	    	    // 2 bytes, purpose of the second byte yet unknown
	    	    int bummer;
		    bummer=stream_read_char(demuxer->stream); --len;
 		    mp_dbg(MSGT_DEMUX,MSGL_DBG2,  "%02X",bummer);
 	    	    vpkg_offset=0;
 		    vpkg_length=len;
		} else {
		
		    if (0==(vpkg_header&0x40)) {
			// sub-seqnum (bits 0-6: number of fragment. bit 7: ???)
		        vpkg_subseq=stream_read_char(demuxer->stream);
	                --len;
		        mp_dbg(MSGT_DEMUX,MSGL_DBG2,  "subseq: %02X ",vpkg_subseq);
			vpkg_subseq&=0x7f;
	            }

	  	    // size of the complete packet
		    // bit 14 is always one (same applies to the offset)
		    vpkg_length=stream_read_word(demuxer->stream);
		    len-=2;
		    mp_dbg(MSGT_DEMUX,MSGL_DBG2, "l: %02X %02X ",vpkg_length>>8,vpkg_length&0xff);
		    if (!(vpkg_length&0xC000)) {
			vpkg_length<<=16;
		        vpkg_length|=(uint16_t)stream_read_word(demuxer->stream);
		        mp_dbg(MSGT_DEMUX,MSGL_DBG2, "l+: %02X %02X ",(vpkg_length>>8)&0xff,vpkg_length&0xff);
	    	        len-=2;
		    } else
		    vpkg_length&=0x3fff;

		    // offset of the following data inside the complete packet
		    // Note: if (hdr&0xC0)==0x80 then offset is relative to the
		    // _end_ of the packet, so it's equal to fragment size!!!
		    vpkg_offset=stream_read_word(demuxer->stream);
	            len-=2;
		    mp_dbg(MSGT_DEMUX,MSGL_DBG2, "o: %02X %02X ",vpkg_offset>>8,vpkg_offset&0xff);
		    if (!(vpkg_offset&0xC000)) {
			vpkg_offset<<=16;
		        vpkg_offset|=(uint16_t)stream_read_word(demuxer->stream);
		        mp_dbg(MSGT_DEMUX,MSGL_DBG2, "o+: %02X %02X ",(vpkg_offset>>8)&0xff,vpkg_offset&0xff);
	    	        len-=2;
		    } else
		    vpkg_offset&=0x3fff;

		    vpkg_seqnum=stream_read_char(demuxer->stream); --len;
		    mp_dbg(MSGT_DEMUX,MSGL_DBG2, "seq: %02X ",vpkg_seqnum);
	        }
 		mp_dbg(MSGT_DEMUX,MSGL_DBG2, "\n");
                mp_dbg(MSGT_DEMUX,MSGL_DBG2, "blklen=%d\n", len);
		mp_msg(MSGT_DEMUX,MSGL_DBG2, "block: hdr=0x%0x, len=%d, offset=%d, seqnum=%d\n",
		    vpkg_header, vpkg_length, vpkg_offset, vpkg_seqnum);

		if(ds->asf_packet){
		    dp=ds->asf_packet;
		    dp_hdr=(dp_hdr_t*)dp->buffer;
		    dp_data=dp->buffer+sizeof(dp_hdr_t);
		    extra=(uint32_t*)(dp->buffer+dp_hdr->chunktab);
		    mp_dbg(MSGT_DEMUX,MSGL_DBG2, "we have an incomplete packet (oldseq=%d new=%d)\n",ds->asf_seq,vpkg_seqnum);
		    // we have an incomplete packet:
		    if(ds->asf_seq!=vpkg_seqnum){
			// this fragment is for new packet, close the old one
			mp_msg(MSGT_DEMUX,MSGL_DBG2, "closing probably incomplete packet, len: %d  \n",dp->len);
			if(priv->video_after_seek){
				priv->kf_base = 0;
				priv->kf_pts = dp_hdr->timestamp;
				dp->pts=
				real_fix_timestamp(priv,dp_data,dp_hdr->timestamp,sh_video->frametime,sh_video->format);
				priv->video_after_seek = 0;
			} else if (dp_hdr->len >= 3)
			    dp->pts =
			    real_fix_timestamp(priv,dp_data,dp_hdr->timestamp,sh_video->frametime,sh_video->format);
			ds_add_packet(ds,dp);
			ds->asf_packet=NULL;
		    } else {
			// append data to it!
			++dp_hdr->chunks;
			mp_msg(MSGT_DEMUX,MSGL_DBG2,"[chunks=%d  subseq=%d]\n",dp_hdr->chunks,vpkg_subseq);
			if(dp_hdr->chunktab+8*(1+dp_hdr->chunks)>dp->len){
			    // increase buffer size, this should not happen!
			    mp_msg(MSGT_DEMUX,MSGL_WARN, "chunktab buffer too small!!!!!\n");
			    dp->len=dp_hdr->chunktab+8*(4+dp_hdr->chunks);
			    dp->buffer=realloc(dp->buffer,dp->len+FF_INPUT_BUFFER_PADDING_SIZE);
			    memset(dp->buffer + dp->len, 0, FF_INPUT_BUFFER_PADDING_SIZE);
			    // re-calc pointers:
			    dp_hdr=(dp_hdr_t*)dp->buffer;
			    dp_data=dp->buffer+sizeof(dp_hdr_t);
			    extra=(uint32_t*)(dp->buffer+dp_hdr->chunktab);
			}
			extra[2*dp_hdr->chunks+0]=1;
			extra[2*dp_hdr->chunks+1]=dp_hdr->len;
			if(0x80==(vpkg_header&0xc0)){
			    // last fragment!
			    if(dp_hdr->len!=vpkg_length-vpkg_offset)
				mp_msg(MSGT_DEMUX,MSGL_V,"warning! assembled.len=%d  frag.len=%d  total.len=%d  \n",dp->len,vpkg_offset,vpkg_length-vpkg_offset);
            		    stream_read(demuxer->stream, dp_data+dp_hdr->len, vpkg_offset);
			    if((dp_data[dp_hdr->len]&0x20) && (sh_video->format==0x30335652)) --dp_hdr->chunks; else
			    dp_hdr->len+=vpkg_offset;
			    len-=vpkg_offset;
 			    mp_dbg(MSGT_DEMUX,MSGL_DBG2, "fragment (%d bytes) appended, %d bytes left\n",vpkg_offset,len);
			    // we know that this is the last fragment -> we can close the packet!
			    if(priv->video_after_seek){
				    priv->kf_base = 0;
				    priv->kf_pts = dp_hdr->timestamp;
				    dp->pts=
				    real_fix_timestamp(priv,dp_data,dp_hdr->timestamp,sh_video->frametime,sh_video->format);
				    priv->video_after_seek = 0;
			    } else if (dp_hdr->len >= 3)
				dp->pts =
				real_fix_timestamp(priv,dp_data,dp_hdr->timestamp,sh_video->frametime,sh_video->format);
			    ds_add_packet(ds,dp);
			    ds->asf_packet=NULL;
			    // continue parsing
			    continue;
			}
			// non-last fragment:
			if(dp_hdr->len!=vpkg_offset)
			    mp_msg(MSGT_DEMUX,MSGL_V,"warning! assembled.len=%d  offset=%d  frag.len=%d  total.len=%d  \n",dp->len,vpkg_offset,len,vpkg_length);
            		stream_read(demuxer->stream, dp_data+dp_hdr->len, len);
			if((dp_data[dp_hdr->len]&0x20) && (sh_video->format==0x30335652)) --dp_hdr->chunks; else
			dp_hdr->len+=len;
			len=0;
			break; // no more fragments in this chunk!
		    }
		}
		// create new packet!
		dp = new_demux_packet(sizeof(dp_hdr_t)+vpkg_length+8*(1+2*(vpkg_header&0x3F)));
	    	// the timestamp seems to be in milliseconds
                dp->pos = demuxer->filepos;
                dp->flags = (flags & 0x2) ? 0x10 : 0;
		ds->asf_seq = vpkg_seqnum;
		dp_hdr=(dp_hdr_t*)dp->buffer;
		dp_hdr->chunks=0;
		dp_hdr->timestamp=timestamp;
		dp_hdr->chunktab=sizeof(dp_hdr_t)+vpkg_length;
		dp_data=dp->buffer+sizeof(dp_hdr_t);
		extra=(uint32_t*)(dp->buffer+dp_hdr->chunktab);
		extra[0]=1; extra[1]=0; // offset of the first chunk
		if(0x00==(vpkg_header&0xc0)){
		    // first fragment:
		    dp_hdr->len=len;
		    stream_read(demuxer->stream, dp_data, len);
		    ds->asf_packet=dp;
		    len=0;
		    if(priv->video_after_seek){
		        priv->kf_base = 0;
		        priv->kf_pts = dp_hdr->timestamp;
		        dp->pts=
		        real_fix_timestamp(priv,dp_data,dp_hdr->timestamp,sh_video->frametime,sh_video->format);
		        priv->video_after_seek = 0;
		    }
		    break;
		}
		// whole packet (not fragmented):
		if (vpkg_length > len) {
		    mp_msg(MSGT_DEMUX, MSGL_WARN,"\n******** WARNING: vpkg_length=%i > len=%i ********\n", vpkg_length, len);
		    /*
		     * To keep the video stream rolling, we need to break 
		     * here. We shouldn't touch len to make sure rest of the
		     * broken packet is skipped.
		     */
		    break;
		}
		dp_hdr->len=vpkg_length; len-=vpkg_length;
		stream_read(demuxer->stream, dp_data, vpkg_length);
		if(priv->video_after_seek){
			priv->kf_base = 0;
			priv->kf_pts = dp_hdr->timestamp;
			dp->pts=
			real_fix_timestamp(priv,dp_data,dp_hdr->timestamp,sh_video->frametime,sh_video->format);
			priv->video_after_seek = 0;
		} else if (dp_hdr->len >= 3)
		    dp->pts =
		    real_fix_timestamp(priv,dp_data,dp_hdr->timestamp,sh_video->frametime,sh_video->format);
		ds_add_packet(ds,dp);

	    } // while(len>0)
	    
	    if(len){
		mp_msg(MSGT_DEMUX, MSGL_WARN,"\n******** !!!!!!!! BUG!! len=%d !!!!!!!!!!! ********\n",len);
		if(len>0) stream_skip(demuxer->stream, len);
	    }
	}
	if ((unsigned)demuxer->video->id < MAX_STREAMS)
		while (priv->current_vpacket + 1 < priv->index_table_size[demuxer->video->id] && 
		       timestamp > priv->index_table[demuxer->video->id][priv->current_vpacket + 1].timestamp)
			priv->current_vpacket += 1;

	if(priv->is_multirate)
		while (priv->v_idx_ptr + 1 < priv->index_table_size[demuxer->video->id] &&
		       timestamp > priv->index_table[demuxer->video->id][priv->v_idx_ptr + 1].timestamp) {
			priv->v_idx_ptr++;
			priv->video_curpos = stream_tell(demuxer->stream);
			priv->stream_switch = 1;
		}
	
	return 1;
    }

if((unsigned)stream_id<MAX_STREAMS){

    if(demuxer->audio->id==-1 && demuxer->a_streams[stream_id]){
	sh_audio_t *sh = demuxer->a_streams[stream_id];
	demuxer->audio->id=stream_id;
	sh->ds=demuxer->audio;
	demuxer->audio->sh=sh;
	priv->audio_buf = calloc(priv->sub_packet_h[demuxer->audio->id], priv->audiopk_size[demuxer->audio->id]);
	priv->audio_timestamp = calloc(priv->sub_packet_h[demuxer->audio->id], sizeof(double));
        mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected RM audio ID = %d\n",stream_id);
	goto got_audio;
    }

    if(demuxer->video->id==-1 && demuxer->v_streams[stream_id]){
	sh_video_t *sh = demuxer->v_streams[stream_id];
	demuxer->video->id=stream_id;
	sh->ds=demuxer->video;
	demuxer->video->sh=sh;
        mp_msg(MSGT_DEMUX,MSGL_V,"Auto-selected RM video ID = %d\n",stream_id);
	goto got_video;
    }

}

    mp_msg(MSGT_DEMUX,MSGL_DBG2, "unknown stream id (%d)\n", stream_id);
discard:
    stream_skip(demuxer->stream, len);
  }//    goto loop;
  return 0;
}

extern void print_wave_header(WAVEFORMATEX *h, int verbose_level);

static demuxer_t* demux_open_real(demuxer_t* demuxer)
{
    real_priv_t* priv = demuxer->priv;
    int num_of_headers;
    int a_streams=0;
    int v_streams=0;
    int i;
    int header_size;

    header_size = stream_read_dword(demuxer->stream); /* header size */
    mp_msg(MSGT_DEMUX,MSGL_V, "real: Header size: %d\n", header_size);
    i = stream_read_word(demuxer->stream); /* version */
    mp_msg(MSGT_DEMUX,MSGL_V, "real: Header object version: %d\n", i);
    if (header_size == 0x10)
    	i = stream_read_word(demuxer->stream);
    else /* we should test header_size here too. */
    	i = stream_read_dword(demuxer->stream);
    mp_msg(MSGT_DEMUX,MSGL_V, "real: File version: %d\n", i);
    num_of_headers = stream_read_dword(demuxer->stream);

    /* parse chunks */
    for (i = 1; i <= num_of_headers; i++)
//    for (i = 1; ; i++)
    {
	int chunk_id, chunk_pos, chunk_size;
	
	chunk_pos = stream_tell(demuxer->stream);
	chunk_id = stream_read_dword_le(demuxer->stream);
	chunk_size = stream_read_dword(demuxer->stream);

	stream_skip(demuxer->stream, 2); /* version */

	mp_msg(MSGT_DEMUX,MSGL_V, "Chunk: %.4s (%x) (size: 0x%x, offset: 0x%x)\n",
	    (char *)&chunk_id, chunk_id, chunk_size, chunk_pos);
	
	if (chunk_size < 10){
	    mp_msg(MSGT_DEMUX,MSGL_ERR,"demux_real: invalid chunksize! (%d)\n",chunk_size);
	    break; //return;
	}
	
	switch(chunk_id)
	{
	    case MKTAG('P', 'R', 'O', 'P'):
		/* Properties header */

		stream_skip(demuxer->stream, 4); /* max bitrate */
		stream_skip(demuxer->stream, 4); /* avg bitrate */
		stream_skip(demuxer->stream, 4); /* max packet size */
		stream_skip(demuxer->stream, 4); /* avg packet size */
		stream_skip(demuxer->stream, 4); /* nb packets */
		priv->duration = stream_read_dword(demuxer->stream)/1000; /* duration */
		stream_skip(demuxer->stream, 4); /* preroll */
		priv->index_chunk_offset = stream_read_dword(demuxer->stream);
		mp_msg(MSGT_DEMUX,MSGL_V,"First index chunk offset: 0x%x\n", priv->index_chunk_offset);
		priv->data_chunk_offset = stream_read_dword(demuxer->stream)+10;
		mp_msg(MSGT_DEMUX,MSGL_V,"First data chunk offset: 0x%x\n", priv->data_chunk_offset);
		stream_skip(demuxer->stream, 2); /* nb streams */
#if 0
		stream_skip(demuxer->stream, 2); /* flags */
#else
		{
		    int flags = stream_read_word(demuxer->stream);
		    
		    if (flags)
		    {
		    mp_msg(MSGT_DEMUX,MSGL_V,"Flags (%x): ", flags);
		    if (flags & 0x1)
			mp_msg(MSGT_DEMUX,MSGL_V,"[save allowed] ");
		    if (flags & 0x2)
			mp_msg(MSGT_DEMUX,MSGL_V,"[perfect play (more buffers)] ");
		    if (flags & 0x4)
			mp_msg(MSGT_DEMUX,MSGL_V,"[live broadcast] ");
		    mp_msg(MSGT_DEMUX,MSGL_V,"\n");
		    }
		}
#endif
		break;
	    case MKTAG('C', 'O', 'N', 'T'):
	    {
		/* Content description header */
		char *buf;
		int len;

		len = stream_read_word(demuxer->stream);
		if (len > 0)
		{
		    buf = malloc(len+1);
		    stream_read(demuxer->stream, buf, len);
		    buf[len] = 0;
		    demux_info_add(demuxer, "name", buf);
		    free(buf);
		}

		len = stream_read_word(demuxer->stream);
		if (len > 0)
		{
		    buf = malloc(len+1);
		    stream_read(demuxer->stream, buf, len);
		    buf[len] = 0;
		    demux_info_add(demuxer, "author", buf);
		    free(buf);
		}

		len = stream_read_word(demuxer->stream);
		if (len > 0)
		{
		    buf = malloc(len+1);
		    stream_read(demuxer->stream, buf, len);
		    buf[len] = 0;
		    demux_info_add(demuxer, "copyright", buf);
		    free(buf);
		}

		len = stream_read_word(demuxer->stream);
		if (len > 0)
		{
		    buf = malloc(len+1);
	    	    stream_read(demuxer->stream, buf, len);
		    buf[len] = 0;
		    demux_info_add(demuxer, "comment", buf);
		    free(buf);
		}
		break;
	    }
	    case MKTAG('M', 'D', 'P', 'R'):
	    {
		/* Media properties header */
		int stream_id;
		int bitrate;
		int codec_data_size;
		int codec_pos;
		int tmp;
		int len;
		char *descr, *mimet = NULL;

		stream_id = stream_read_word(demuxer->stream);
		mp_msg(MSGT_DEMUX,MSGL_V,"Found new stream (id: %d)\n", stream_id);
		
		stream_skip(demuxer->stream, 4); /* max bitrate */
		bitrate = stream_read_dword(demuxer->stream); /* avg bitrate */
		stream_skip(demuxer->stream, 4); /* max packet size */
		stream_skip(demuxer->stream, 4); /* avg packet size */
		stream_skip(demuxer->stream, 4); /* start time */
		stream_skip(demuxer->stream, 4); /* preroll */
		stream_skip(demuxer->stream, 4); /* duration */
		
		if ((len = stream_read_char(demuxer->stream)) > 0) {
		    descr = malloc(len+1);
	    	stream_read(demuxer->stream, descr, len);
		    descr[len] = 0;
		    mp_msg(MSGT_DEMUX, MSGL_INFO,"Stream description: %s\n", descr);
		    free(descr);
		}
		if ((len = stream_read_char(demuxer->stream)) > 0) {
		    mimet = malloc(len+1);
	    	stream_read(demuxer->stream, mimet, len);
		    mimet[len] = 0;
		    mp_msg(MSGT_DEMUX, MSGL_INFO,"Stream mimetype: %s\n", mimet);
		}
		
		/* Type specific header */
		codec_data_size = stream_read_dword(demuxer->stream);
		codec_pos = stream_tell(demuxer->stream);

#ifdef MP_DEBUG
#define stream_skip(st,siz) { int i; for(i=0;i<siz;i++) mp_msg(MSGT_DEMUX,MSGL_V," %02X",stream_read_char(st)); mp_msg(MSGT_DEMUX,MSGL_V,"\n");}
#endif

	if (!strncmp(mimet,"audio/",6)) {
	  if (strstr(mimet,"x-pn-realaudio") || strstr(mimet,"x-pn-multirate-realaudio")) {
		tmp = stream_read_dword(demuxer->stream);
		if (tmp != MKTAG(0xfd, 'a', 'r', '.'))
		{
		    mp_msg(MSGT_DEMUX,MSGL_V,"Audio: can't find .ra in codec data\n");
		} else {
		    /* audio header */
		    sh_audio_t *sh = new_sh_audio(demuxer, stream_id);
		    char buf[128]; /* for codec name */
		    int frame_size;
		    int sub_packet_size;
		    int sub_packet_h;
		    int version;
		    int flavor;
		    int coded_frame_size;
		    int codecdata_length;
		    int i;
		    char *buft;
		    int hdr_size;
		    
		    mp_msg(MSGT_DEMUX,MSGL_V,"Found audio stream!\n");
		    version = stream_read_word(demuxer->stream);
		    mp_msg(MSGT_DEMUX,MSGL_V,"version: %d\n", version);
                   if (version == 3) {
                    stream_skip(demuxer->stream, 2);
                    stream_skip(demuxer->stream, 10);
                    stream_skip(demuxer->stream, 4);
                    // Name, author, (c) are also in CONT tag
                    if ((i = stream_read_char(demuxer->stream)) != 0) {
                      buft = malloc(i+1);
                      stream_read(demuxer->stream, buft, i);
                      buft[i] = 0;
                      demux_info_add(demuxer, "Name", buft);
                      free(buft);
                    }
                    if ((i = stream_read_char(demuxer->stream)) != 0) {
                      buft = malloc(i+1);
                      stream_read(demuxer->stream, buft, i);
                      buft[i] = 0;
                      demux_info_add(demuxer, "Author", buft);
                      free(buft);
                    }
                    if ((i = stream_read_char(demuxer->stream)) != 0) {
                      buft = malloc(i+1);
                      stream_read(demuxer->stream, buft, i);
                      buft[i] = 0;
                      demux_info_add(demuxer, "Copyright", buft);
                      free(buft);
                    }
                    if ((i = stream_read_char(demuxer->stream)) != 0)
                      mp_msg(MSGT_DEMUX,MSGL_WARN,"Last header byte is not zero!\n");
                    stream_skip(demuxer->stream, 1);
                    i = stream_read_char(demuxer->stream);
                    sh->format = stream_read_dword_le(demuxer->stream);
                    if (i != 4) {
                      mp_msg(MSGT_DEMUX,MSGL_WARN,"Audio FourCC size is not 4 (%d), please report to "
                             "MPlayer developers\n", i);
                      stream_skip(demuxer->stream, i - 4);
                    }
                    if (sh->format != mmioFOURCC('l','p','c','J')) {
                      mp_msg(MSGT_DEMUX,MSGL_WARN,"Version 3 audio with FourCC %8x, please report to "
                             "MPlayer developers\n", sh->format);
                    }
                    sh->channels = 1;
                    sh->samplesize = 16;
                    sh->samplerate = 8000;
                    frame_size = 240;
                    strcpy(buf, "14_4");
                   } else {
		    stream_skip(demuxer->stream, 2); // 00 00
		    stream_skip(demuxer->stream, 4); /* .ra4 or .ra5 */
		    stream_skip(demuxer->stream, 4); // ???
		    stream_skip(demuxer->stream, 2); /* version (4 or 5) */
		    hdr_size = stream_read_dword(demuxer->stream); // header size
		    mp_msg(MSGT_DEMUX,MSGL_V,"header size: %d\n", hdr_size);
		    flavor = stream_read_word(demuxer->stream);/* codec flavor id */
		    coded_frame_size = stream_read_dword(demuxer->stream);/* needed by codec */
		    mp_msg(MSGT_DEMUX,MSGL_V,"coded_frame_size: %d\n", coded_frame_size);
		    stream_skip(demuxer->stream, 4); // big number
		    stream_skip(demuxer->stream, 4); // bigger number
		    stream_skip(demuxer->stream, 4); // 2 || -''-
		    sub_packet_h = stream_read_word(demuxer->stream);
		    mp_msg(MSGT_DEMUX,MSGL_V,"sub_packet_h: %d\n", sub_packet_h);
		    frame_size = stream_read_word(demuxer->stream);
		    mp_msg(MSGT_DEMUX,MSGL_V,"frame_size: %d\n", frame_size);
		    sub_packet_size = stream_read_word(demuxer->stream);
		    mp_msg(MSGT_DEMUX,MSGL_V,"sub_packet_size: %d\n", sub_packet_size);
		    stream_skip(demuxer->stream, 2); // 0
		    
		    if (version == 5)
			stream_skip(demuxer->stream, 6); //0,srate,0

		    sh->samplerate = stream_read_word(demuxer->stream);
		    stream_skip(demuxer->stream, 2);  // 0
		    sh->samplesize = stream_read_word(demuxer->stream)/8;
		    sh->channels = stream_read_word(demuxer->stream);
		    mp_msg(MSGT_DEMUX,MSGL_V,"samplerate: %d, channels: %d\n",
			sh->samplerate, sh->channels);

		    if (version == 5)
		    {
			stream_read(demuxer->stream, buf, 4);  // interleaver id
			priv->intl_id[stream_id] = MKTAG(buf[0], buf[1], buf[2], buf[3]);
			stream_read(demuxer->stream, buf, 4); // fourcc
			buf[4] = 0;
		    }
		    else
		    {		
			/* Interleaver id */
			get_str(1, demuxer, buf, sizeof(buf));
			priv->intl_id[stream_id] = MKTAG(buf[0], buf[1], buf[2], buf[3]);
			/* Codec FourCC */
			get_str(1, demuxer, buf, sizeof(buf));
		    }
                   }

		    /* Emulate WAVEFORMATEX struct: */
		    sh->wf = malloc(sizeof(WAVEFORMATEX));
		    memset(sh->wf, 0, sizeof(WAVEFORMATEX));
		    sh->wf->nChannels = sh->channels;
		    sh->wf->wBitsPerSample = sh->samplesize*8;
		    sh->wf->nSamplesPerSec = sh->samplerate;
		    sh->wf->nAvgBytesPerSec = bitrate/8;
		    sh->wf->nBlockAlign = frame_size;
		    sh->wf->cbSize = 0;
		    sh->format = MKTAG(buf[0], buf[1], buf[2], buf[3]);

		    switch (sh->format)
		    {
			case MKTAG('d', 'n', 'e', 't'):
			    mp_msg(MSGT_DEMUX,MSGL_V,"Audio: DNET -> AC3\n");
//			    sh->format = 0x2000;
			    break;
			case MKTAG('1', '4', '_', '4'):
                sh->wf->nBlockAlign = 0x14;
                            break;

			case MKTAG('2', '8', '_', '8'):
			    sh->wf->nBlockAlign = coded_frame_size;
			    break;

			case MKTAG('s', 'i', 'p', 'r'):
			case MKTAG('a', 't', 'r', 'c'):
			case MKTAG('c', 'o', 'o', 'k'):
			    // realaudio codec plugins - common:
			    stream_skip(demuxer->stream,3);  // Skip 3 unknown bytes 
			    if (version==5)
			      stream_skip(demuxer->stream,1);  // Skip 1 additional unknown byte 
			    codecdata_length=stream_read_dword(demuxer->stream);
			    // Check extradata len, we can't store bigger values in cbSize anyway
			    if ((unsigned)codecdata_length > 0xffff) {
			        mp_msg(MSGT_DEMUX,MSGL_ERR,"Extradata too big (%d)\n", codecdata_length);
				goto skip_this_chunk;
			    }
			    sh->wf->cbSize = codecdata_length;
			    sh->wf = realloc(sh->wf, sizeof(WAVEFORMATEX)+sh->wf->cbSize);
			    stream_read(demuxer->stream, ((char*)(sh->wf+1)), codecdata_length); // extras
                if (priv->intl_id[stream_id] == MKTAG('g', 'e', 'n', 'r'))
    			    sh->wf->nBlockAlign = sub_packet_size;
    			else
    			    sh->wf->nBlockAlign = coded_frame_size;

			    break;

			case MKTAG('r', 'a', 'a', 'c'):
			case MKTAG('r', 'a', 'c', 'p'):
			    /* This is just AAC. The two or five bytes of */
			    /* config data needed for libfaad are stored */
			    /* after the audio headers. */
			    stream_skip(demuxer->stream,3);  // Skip 3 unknown bytes 
			    if (version==5)
				stream_skip(demuxer->stream,1);  // Skip 1 additional unknown byte 
			    codecdata_length=stream_read_dword(demuxer->stream);
			    if (codecdata_length>=1) {
				sh->codecdata_len = codecdata_length - 1;
				sh->codecdata = calloc(sh->codecdata_len, 1);
				stream_skip(demuxer->stream, 1);
				stream_read(demuxer->stream, sh->codecdata, sh->codecdata_len);
			    }
			    sh->format = mmioFOURCC('M', 'P', '4', 'A');
			    break;
			default:
			    mp_msg(MSGT_DEMUX,MSGL_V,"Audio: Unknown (%s)\n", buf);
		    }

		    // Interleaver setup
		    priv->sub_packet_size[stream_id] = sub_packet_size;
		    priv->sub_packet_h[stream_id] = sub_packet_h;
		    priv->coded_framesize[stream_id] = coded_frame_size;
		    priv->audiopk_size[stream_id] = frame_size;

		    sh->wf->wFormatTag = sh->format;
		    
		    mp_msg(MSGT_DEMUX,MSGL_V,"audio fourcc: %.4s (%x)\n", (char *)&sh->format, sh->format);
		    if ( mp_msg_test(MSGT_DEMUX,MSGL_V) )
		    print_wave_header(sh->wf, MSGL_V);

		    /* Select audio stream with highest bitrate if multirate file*/
		    if (priv->is_multirate && ((demuxer->audio->id == -1) ||
		                               ((demuxer->audio->id >= 0) && priv->a_bitrate && (bitrate > priv->a_bitrate)))) {
			    demuxer->audio->id = stream_id;
			    priv->a_bitrate = bitrate;
			    mp_msg(MSGT_DEMUX,MSGL_DBG2,"Multirate autoselected audio id %d with bitrate %d\n", stream_id, bitrate);
		    }

		    if(demuxer->audio->id==stream_id){
			sh->ds=demuxer->audio;
			demuxer->audio->sh=sh;
        	priv->audio_buf = calloc(priv->sub_packet_h[demuxer->audio->id], priv->audiopk_size[demuxer->audio->id]);
        	priv->audio_timestamp = calloc(priv->sub_packet_h[demuxer->audio->id], sizeof(double));
		    }
		    
		    ++a_streams;

#ifdef stream_skip
#undef stream_skip
#endif
		}
	  } else if (strstr(mimet,"X-MP3-draft-00")) {
		    sh_audio_t *sh = new_sh_audio(demuxer, stream_id);

		    /* Emulate WAVEFORMATEX struct: */
		    sh->wf = malloc(sizeof(WAVEFORMATEX));
		    memset(sh->wf, 0, sizeof(WAVEFORMATEX));
		    sh->wf->nChannels = 0;//sh->channels;
		    sh->wf->wBitsPerSample = 16;
		    sh->wf->nSamplesPerSec = 0;//sh->samplerate;
		    sh->wf->nAvgBytesPerSec = 0;//bitrate;
		    sh->wf->nBlockAlign = 0;//frame_size;
		    sh->wf->cbSize = 0;
		    sh->wf->wFormatTag = sh->format = mmioFOURCC('a','d','u',0x55);
		    
		    if(demuxer->audio->id==stream_id){
			    sh->ds=demuxer->audio;
			    demuxer->audio->sh=sh;
		    }
		    
		    ++a_streams;
	  } else if (strstr(mimet,"x-ralf-mpeg4")) {
		 mp_msg(MSGT_DEMUX,MSGL_ERR,"Real lossless audio not supported yet\n");
	  } else {
		 mp_msg(MSGT_DEMUX,MSGL_V,"Unknown audio stream format\n");
		}
	} else if (!strncmp(mimet,"video/",6)) {
	  if (strstr(mimet,"x-pn-realvideo") || strstr(mimet,"x-pn-multirate-realvideo")) {
		stream_skip(demuxer->stream, 4);  // VIDO length, same as codec_data_size
		tmp = stream_read_dword(demuxer->stream);
		if(tmp != MKTAG('O', 'D', 'I', 'V'))
		{
		    mp_msg(MSGT_DEMUX,MSGL_V,"Video: can't find VIDO in codec data\n");
		} else {
		    /* video header */
		    sh_video_t *sh = new_sh_video(demuxer, stream_id);

		    sh->format = stream_read_dword_le(demuxer->stream); /* fourcc */
		    mp_msg(MSGT_DEMUX,MSGL_V,"video fourcc: %.4s (%x)\n", (char *)&sh->format, sh->format);

		    /* emulate BITMAPINFOHEADER */
		    sh->bih = malloc(sizeof(BITMAPINFOHEADER));
		    memset(sh->bih, 0, sizeof(BITMAPINFOHEADER));
	    	    sh->bih->biSize = sizeof(BITMAPINFOHEADER);
		    sh->disp_w = sh->bih->biWidth = stream_read_word(demuxer->stream);
		    sh->disp_h = sh->bih->biHeight = stream_read_word(demuxer->stream);
		    sh->bih->biPlanes = 1;
		    sh->bih->biBitCount = 24;
		    sh->bih->biCompression = sh->format;
		    sh->bih->biSizeImage= sh->bih->biWidth*sh->bih->biHeight*3;

		    sh->fps = (float) stream_read_word(demuxer->stream);
		    if (sh->fps<=0) sh->fps=24; // we probably won't even care about fps
		    sh->frametime = 1.0f/sh->fps;
		    
#if 1
		    stream_skip(demuxer->stream, 4);
#else
		    mp_msg(MSGT_DEMUX, MSGL_V,"unknown1: 0x%X  \n",stream_read_dword(demuxer->stream));
		    mp_msg(MSGT_DEMUX, MSGL_V,"unknown2: 0x%X  \n",stream_read_word(demuxer->stream));
		    mp_msg(MSGT_DEMUX, MSGL_V,"unknown3: 0x%X  \n",stream_read_word(demuxer->stream));
#endif
//		    if(sh->format==0x30335652 || sh->format==0x30325652 )
		    if(1)
		    {
			int tmp=stream_read_word(demuxer->stream);
			if(tmp>0){
			    sh->fps=tmp; sh->frametime = 1.0f/sh->fps;
			}
		    } else {
	    		int fps=stream_read_word(demuxer->stream);
			mp_msg(MSGT_DEMUX, MSGL_WARN,"realvid: ignoring FPS = %d\n",fps);
		    }
		    stream_skip(demuxer->stream, 2);
		    
		    {
			    // read and store codec extradata
			    unsigned int cnt = codec_data_size - (stream_tell(demuxer->stream) - codec_pos);
			    if (cnt > 0x7fffffff - sizeof(BITMAPINFOHEADER)) {
			        mp_msg(MSGT_DEMUX, MSGL_ERR,"Extradata too big (%u)\n", cnt);
			    } else  {
				sh->bih = realloc(sh->bih, sizeof(BITMAPINFOHEADER) + cnt);
			        sh->bih->biSize += cnt;
				stream_read(demuxer->stream, ((unsigned char*)(sh->bih+1)), cnt);
			    }
		    } 
		    if(sh->format == 0x30315652 && ((unsigned char*)(sh->bih+1))[6] == 0x30)
			    sh->bih->biCompression = sh->format = mmioFOURCC('R', 'V', '1', '3');
		    
		    /* Select video stream with highest bitrate if multirate file*/
		    if (priv->is_multirate && ((demuxer->video->id == -1) ||
		                               ((demuxer->video->id >= 0) && priv->v_bitrate && (bitrate > priv->v_bitrate)))) {
			    demuxer->video->id = stream_id;
			    priv->v_bitrate = bitrate;
			    mp_msg(MSGT_DEMUX,MSGL_DBG2,"Multirate autoselected video id %d with bitrate %d\n", stream_id, bitrate);
		    }

		    if(demuxer->video->id==stream_id){
			sh->ds=demuxer->video;
			demuxer->video->sh=sh;
		    }
		    
		    ++v_streams;

		}
	  } else {
		 mp_msg(MSGT_DEMUX,MSGL_V,"Unknown video stream format\n");
	  }
	} else if (strstr(mimet,"logical-")) {
		 if (strstr(mimet,"fileinfo")) {
		     mp_msg(MSGT_DEMUX,MSGL_V,"Got a logical-fileinfo chunk\n");
		 } else if (strstr(mimet,"-audio") || strstr(mimet,"-video")) {
		    int i, stream_cnt;
		    int stream_list[MAX_STREAMS];
		    
		    priv->is_multirate = 1;
		    stream_skip(demuxer->stream, 4); // Length of codec data (repeated)
		    stream_cnt = stream_read_dword(demuxer->stream); // Get number of audio or video streams
		    if ((unsigned)stream_cnt >= MAX_STREAMS) {
		        mp_msg(MSGT_DEMUX,MSGL_ERR,"Too many streams in %s. Big troubles ahead.\n", mimet);
		        goto skip_this_chunk;
		    }
		    for (i = 0; i < stream_cnt; i++)
		        stream_list[i] = stream_read_word(demuxer->stream);
		    for (i = 0; i < stream_cnt; i++)
		        if ((unsigned)stream_list[i] >= MAX_STREAMS) {
		            mp_msg(MSGT_DEMUX,MSGL_ERR,"Stream id out of range: %d. Ignored.\n", stream_list[i]);
		            stream_skip(demuxer->stream, 4); // Skip DATA offset for broken stream
		        } else {
		            priv->str_data_offset[stream_list[i]] = stream_read_dword(demuxer->stream);
		            mp_msg(MSGT_DEMUX,MSGL_V,"Stream %d with DATA offset 0x%08x\n", stream_list[i], priv->str_data_offset[stream_list[i]]);
		        }
		    // Skip the rest of this chunk
		 } else 
		     mp_msg(MSGT_DEMUX,MSGL_V,"Unknown logical stream\n");
		}
		else {
		    mp_msg(MSGT_DEMUX, MSGL_ERR, "Not audio/video stream or unsupported!\n");
		}
//		break;
//	    default:
skip_this_chunk:
		/* skip codec info */
		tmp = stream_tell(demuxer->stream) - codec_pos;
		mp_msg(MSGT_DEMUX,MSGL_V,"### skipping %d bytes of codec info\n", codec_data_size - tmp);
#if 0
		{ int i;
		  for(i=0;i<codec_data_size - tmp;i++)
		      mp_msg(MSGT_DEMUX, MSGL_V," %02X",stream_read_char(demuxer->stream));
		  mp_msg(MSGT_DEMUX, MSGL_V,"\n");
		}
#else
		stream_skip(demuxer->stream, codec_data_size - tmp);
#endif
		if (mimet)
		    free (mimet);
		break;
//	    }
	    }
	    case MKTAG('D', 'A', 'T', 'A'):
		goto header_end;
	    case MKTAG('I', 'N', 'D', 'X'):
	    default:
		mp_msg(MSGT_DEMUX,MSGL_V,"Unknown chunk: %x\n", chunk_id);
		stream_skip(demuxer->stream, chunk_size - 10);
		break;
	}
    }

header_end:
    if(priv->is_multirate) {
        mp_msg(MSGT_DEMUX,MSGL_V,"Selected video id %d audio id %d\n", demuxer->video->id, demuxer->audio->id);
        /* Perform some sanity checks to avoid checking streams id all over the code*/
        if (demuxer->audio->id >= MAX_STREAMS) {
            mp_msg(MSGT_DEMUX,MSGL_ERR,"Invalid audio stream %d. No sound will be played.\n", demuxer->audio->id);
            demuxer->audio->id = -2;
        } else if ((demuxer->audio->id >= 0) && (priv->str_data_offset[demuxer->audio->id] == 0)) {
            mp_msg(MSGT_DEMUX,MSGL_ERR,"Audio stream %d not found. No sound will be played.\n", demuxer->audio->id);
            demuxer->audio->id = -2;
        }
        if (demuxer->video->id >= MAX_STREAMS) {
            mp_msg(MSGT_DEMUX,MSGL_ERR,"Invalid video stream %d. No video will be played.\n", demuxer->video->id);
            demuxer->video->id = -2;
        } else if ((demuxer->video->id >= 0) && (priv->str_data_offset[demuxer->video->id] == 0)) {
            mp_msg(MSGT_DEMUX,MSGL_ERR,"Video stream %d not found. No video will be played.\n", demuxer->video->id);
            demuxer->video->id = -2;
        }
    }

    if(priv->is_multirate && ((demuxer->video->id >= 0) || (demuxer->audio->id  >=0))) {
        /* If audio or video only, seek to right place and behave like standard file */
        if (demuxer->video->id < 0) {
            // Stream is audio only, or -novideo
            stream_seek(demuxer->stream, priv->data_chunk_offset = priv->str_data_offset[demuxer->audio->id]+10);
            priv->is_multirate = 0;
        }
        if (demuxer->audio->id < 0) {
            // Stream is video only, or -nosound
            stream_seek(demuxer->stream, priv->data_chunk_offset = priv->str_data_offset[demuxer->video->id]+10);
            priv->is_multirate = 0;
        }
    }

  if(!priv->is_multirate) {
//    printf("i=%d num_of_headers=%d   \n",i,num_of_headers);
    priv->num_of_packets = stream_read_dword(demuxer->stream);
    stream_skip(demuxer->stream, 4); /* next data header */

    mp_msg(MSGT_DEMUX,MSGL_V,"Packets in file: %d\n", priv->num_of_packets);

    if (priv->num_of_packets == 0)
	priv->num_of_packets = -10;
  } else {
        priv->audio_curpos = priv->str_data_offset[demuxer->audio->id] + 18;
        stream_seek(demuxer->stream, priv->str_data_offset[demuxer->audio->id]+10);
        priv->a_num_of_packets=priv->a_num_of_packets = stream_read_dword(demuxer->stream);
        priv->video_curpos = priv->str_data_offset[demuxer->video->id] + 18;
        stream_seek(demuxer->stream, priv->str_data_offset[demuxer->video->id]+10);
        priv->v_num_of_packets = stream_read_dword(demuxer->stream);
        priv->stream_switch = 1;
        /* Index required for multirate playback, force building if it's not there */
        /* but respect user request to force index regeneration */
        if (index_mode == -1)
            index_mode = 1;
    }


    priv->audio_need_keyframe = 0;
    priv->video_after_seek = 0;

    switch (index_mode){
	case -1: // untouched
	    if (priv->index_chunk_offset && parse_index_chunk(demuxer))
	    {
		demuxer->seekable = 1;
	    }
	    break;
	case 1: // use (generate index)
	    if (priv->index_chunk_offset && parse_index_chunk(demuxer))
	    {
		demuxer->seekable = 1;
	    } else {
		generate_index(demuxer);
		demuxer->seekable = 1;
	    }
	    break;
	case 2: // force generating index
	    generate_index(demuxer);
	    demuxer->seekable = 1;
	    break;
	default: // do nothing
    	    break;
    }

    if(priv->is_multirate)
        demuxer->seekable = 0; // No seeking yet for multirate streams

    // detect streams:
    if(demuxer->video->id==-1 && v_streams>0){
	// find the valid video stream:
	if(!ds_fill_buffer(demuxer->video)){
          mp_msg(MSGT_DEMUXER,MSGL_INFO,"RM: " MSGTR_MissingVideoStream);
	}
    }
    if(demuxer->audio->id==-1 && a_streams>0){
	// find the valid audio stream:
	if(!ds_fill_buffer(demuxer->audio)){
          mp_msg(MSGT_DEMUXER,MSGL_INFO,"RM: " MSGTR_MissingAudioStream);
	}
    }

    if(demuxer->video->sh){
	sh_video_t *sh=demuxer->video->sh;
	mp_msg(MSGT_DEMUX,MSGL_V,"VIDEO:  %.4s [%08X,%08X]  %dx%d  (aspect %4.2f)  %4.2f fps\n",
	    (char *)&sh->format,((unsigned int*)(sh->bih+1))[1],((unsigned int*)(sh->bih+1))[0],
	    sh->disp_w,sh->disp_h,sh->aspect,sh->fps);
    }

    if(demuxer->audio->sh){
	sh_audio_t *sh=demuxer->audio->sh;
	mp_msg(MSGT_DEMUX,MSGL_V,"AUDIO:  %.4s [%08X]\n",
	    (char *)&sh->format,sh->format);
    }

    return demuxer;
}

static void demux_close_real(demuxer_t *demuxer)
{
    int i;
    real_priv_t* priv = demuxer->priv;
 
    if (priv){
    	for(i=0; i<MAX_STREAMS; i++)
	    if(priv->index_table[i])
	        free(priv->index_table[i]);
    if (priv->audio_buf)
        free(priv->audio_buf);
    if (priv->audio_timestamp)
        free(priv->audio_timestamp);
	free(priv);
    }

    return;
}

/* please upload RV10 samples WITH INDEX CHUNK */
static void demux_seek_real(demuxer_t *demuxer, float rel_seek_secs, float audio_delay, int flags)
{
    real_priv_t *priv = demuxer->priv;
    demux_stream_t *d_audio = demuxer->audio;
    demux_stream_t *d_video = demuxer->video;
    sh_audio_t *sh_audio = d_audio->sh;
    sh_video_t *sh_video = d_video->sh;
    int vid = d_video->id, aid = d_audio->id;
    int next_offset = 0;
    int64_t cur_timestamp = 0;
    int streams = 0;
    int retried = 0;


    if (sh_video && (unsigned)vid < MAX_STREAMS && priv->index_table_size[vid])
	streams |= 1;
    if (sh_audio && (unsigned)aid < MAX_STREAMS && priv->index_table_size[aid])
	streams |= 2;

//    printf("streams: %d\n", streams);

    if (!streams)
	return;

    if (flags & 1)
	/* seek absolute */
	priv->current_apacket = priv->current_vpacket = 0;
    if (flags & 2) // percent seek
        rel_seek_secs *= priv->duration;

    if ((streams & 1) && priv->current_vpacket >= priv->index_table_size[vid])
	priv->current_vpacket = priv->index_table_size[vid] - 1;
    if ((streams & 2) && priv->current_apacket >= priv->index_table_size[aid])
	priv->current_apacket = priv->index_table_size[aid] - 1;

//    if (index_mode == 1 || index_mode == 2) {
    	if (streams & 1) {// use the video index if we have one
            cur_timestamp = priv->index_table[vid][priv->current_vpacket].timestamp;
	    if (rel_seek_secs > 0)
	    	while ((priv->index_table[vid][priv->current_vpacket].timestamp - cur_timestamp) < rel_seek_secs * 1000){
	    		priv->current_vpacket += 1;
	    		if (priv->current_vpacket >= priv->index_table_size[vid]) {
	    			priv->current_vpacket = priv->index_table_size[vid] - 1;
				if (!retried) {
					stream_seek(demuxer->stream, priv->index_table[vid][priv->current_vpacket].offset);
					add_index_segment(demuxer, vid, cur_timestamp + rel_seek_secs * 1000);
					retried = 1;
				}
				else
	    				break;
	    		}
	    	} 
	    else if (rel_seek_secs < 0)
	    	while ((cur_timestamp - priv->index_table[vid][priv->current_vpacket].timestamp) < - rel_seek_secs * 1000){
	    		priv->current_vpacket -= 1;
	    		if (priv->current_vpacket < 0) {
	    			priv->current_vpacket = 0;
	    			break;
	    		}
	    	}
	    next_offset = priv->index_table[vid][priv->current_vpacket].offset;
	    priv->audio_need_keyframe = 1;
	    priv->video_after_seek = 1;
        }
    	else if (streams & 2) {
            cur_timestamp = priv->index_table[aid][priv->current_apacket].timestamp;
	    if (rel_seek_secs > 0)
	    	while ((priv->index_table[aid][priv->current_apacket].timestamp - cur_timestamp) < rel_seek_secs * 1000){
	    		priv->current_apacket += 1;
	    		if (priv->current_apacket >= priv->index_table_size[aid]) {
	    			priv->current_apacket = priv->index_table_size[aid] - 1;
	    			break;
	    		}
	    	}
	    else if (rel_seek_secs < 0)
	    	while ((cur_timestamp - priv->index_table[aid][priv->current_apacket].timestamp) < - rel_seek_secs * 1000){
	    		priv->current_apacket -= 1;
	    		if (priv->current_apacket < 0) {
	    			priv->current_apacket = 0;
	    			break;
	    		}
	    	}
	    next_offset = priv->index_table[aid][priv->current_apacket].offset;
        }
//    }
//    printf("seek: pos: %d, current packets: a: %d, v: %d\n",
//	next_offset, priv->current_apacket, priv->current_vpacket);
    if (next_offset)
        stream_seek(demuxer->stream, next_offset);

    demux_real_fill_buffer(demuxer, NULL);
}

static int demux_real_control(demuxer_t *demuxer, int cmd, void *arg)
{
    real_priv_t *priv = demuxer->priv;
    unsigned int lastpts = priv->v_pts ? priv->v_pts : priv->a_pts;
    
    switch (cmd) {
        case DEMUXER_CTRL_GET_TIME_LENGTH:
	    if (priv->duration == 0)
	        return DEMUXER_CTRL_DONTKNOW;
	    
	    *((double *)arg) = (double)priv->duration;
	    return DEMUXER_CTRL_OK;

	case DEMUXER_CTRL_GET_PERCENT_POS:
	    if (priv->duration == 0)
	        return DEMUXER_CTRL_DONTKNOW;
	    
	    *((int *)arg) = (int)(100 * lastpts / priv->duration);
	    return DEMUXER_CTRL_OK;
	
	default:
	    return DEMUXER_CTRL_NOTIMPL;
    }
}


demuxer_desc_t demuxer_desc_real = {
  "Realmedia demuxer",
  "real",
  "REAL",
  "Alex Beregszasi, Florian Schneider, A'rpi, Roberto Togni",
  "handles new .RMF files",
  DEMUXER_TYPE_REAL,
  1, // safe autodetect
  real_check_file,
  demux_real_fill_buffer,
  demux_open_real,
  demux_close_real,
  demux_seek_real,
  demux_real_control
};
