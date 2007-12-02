
#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stream.h"
#include "network.h"
#include "libmpdemux/demuxer.h"
#include "help_mp.h"

extern int network_bandwidth;

static int _rtsp_streaming_seek(int fd, off_t pos, streaming_ctrl_t* streaming_ctrl) {
  return -1; // For now, we don't handle RTSP stream seeking
}

static int rtsp_streaming_start(stream_t* stream) {
  stream->streaming_ctrl->streaming_seek = _rtsp_streaming_seek;
  return 0;
}


static int open_live_rtsp_sip(stream_t *stream,int mode, void* opts, int* file_format) {
  URL_t *url;

  stream->streaming_ctrl = streaming_ctrl_new();
  if( stream->streaming_ctrl==NULL ) {
    return STREAM_ERROR;
  }
  stream->streaming_ctrl->bandwidth = network_bandwidth;
  url = url_new(stream->url);
  stream->streaming_ctrl->url = check4proxies(url);
  //url_free(url);

  mp_msg(MSGT_OPEN, MSGL_INFO, "STREAM_LIVE555, URL: %s\n", stream->url);

  if(rtsp_streaming_start(stream) < 0) {
    mp_msg(MSGT_NETWORK,MSGL_ERR,"rtsp_streaming_start failed\n");
    goto fail;
  }

  *file_format = DEMUXER_TYPE_RTP;
  stream->type = STREAMTYPE_STREAM;
  return STREAM_OK;

fail:
  streaming_ctrl_free( stream->streaming_ctrl );
  stream->streaming_ctrl = NULL;
  return STREAM_ERROR;
}

static int open_live_sdp(stream_t *stream,int mode, void* opts, int* file_format) {
  int f;
  char *filename = stream->url;
  off_t len;
  char* sdpDescription;
  ssize_t numBytesRead;

  if(strncmp("sdp://",filename,6) == 0) {
    filename += 6;
#if defined(__CYGWIN__) || defined(__MINGW32__)
    f = open(filename,O_RDONLY|O_BINARY);
#else
    f = open(filename,O_RDONLY);
#endif
    if(f < 0) {
      mp_msg(MSGT_OPEN,MSGL_ERR,MSGTR_FileNotFound,filename);
      return STREAM_ERROR;
    }

    len=lseek(f,0,SEEK_END); 
    lseek(f,0,SEEK_SET);
    if(len == -1)
      return STREAM_ERROR;
    if(len > SIZE_MAX - 1)
      return STREAM_ERROR;

    sdpDescription = malloc(len+1);
    if(sdpDescription == NULL) return STREAM_ERROR;
    numBytesRead = read(f, sdpDescription, len);
    if(numBytesRead != len) {
      free(sdpDescription);
      return STREAM_ERROR;
    }
    sdpDescription[len] = '\0'; // to be safe
    stream->priv = sdpDescription;

    stream->type = STREAMTYPE_SDP;
    *file_format = DEMUXER_TYPE_RTP;
    return STREAM_OK;
  }
  return STREAM_UNSUPPORTED;
}


const stream_info_t stream_info_rtsp_sip = {
  "standard RTSP and SIP",
  "RTSP and SIP",
  "Ross Finlayson",
  "Uses LIVE555 Streaming Media library.",
  open_live_rtsp_sip,
  {"rtsp", "sip", NULL },
  NULL,
  0 // Urls are an option string
};

const stream_info_t stream_info_sdp = {
  "SDP stream descriptor",
  "SDP",
  "Ross Finlayson",
  "Uses LIVE555 Streaming Media library.",
  open_live_sdp,
  {"sdp", NULL },
  NULL,
  0 // Urls are an option string
};
