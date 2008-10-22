////////// Routines (with C-linkage) that interface between "MPlayer"
////////// and the "LIVE555 Streaming Media" libraries:

extern "C" {
// on MinGW, we must include windows.h before the things it conflicts
#ifdef __MINGW32__    // with.  they are each protected from
#include <windows.h>  // windows.h, but not the other way around.
#endif
#include "demux_rtp.h"
#include "stheader.h"
}
#include "demux_rtp_internal.h"

#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"
#include "GroupsockHelper.hh"
#include <unistd.h>

// A data structure representing input data for each stream:
class ReadBufferQueue {
public:
  ReadBufferQueue(MediaSubsession* subsession, demuxer_t* demuxer,
		  char const* tag);
  virtual ~ReadBufferQueue();

  FramedSource* readSource() const { return fReadSource; }
  RTPSource* rtpSource() const { return fRTPSource; }
  demuxer_t* ourDemuxer() const { return fOurDemuxer; }
  char const* tag() const { return fTag; }

  char blockingFlag; // used to implement synchronous reads

  // For A/V synchronization:
  Boolean prevPacketWasSynchronized;
  float prevPacketPTS;
  ReadBufferQueue** otherQueue;

  // The 'queue' actually consists of just a single "demux_packet_t"
  // (because the underlying OS does the actual queueing/buffering):
  demux_packet_t* dp;

  // However, we sometimes inspect buffers before delivering them.
  // For this, we maintain a queue of pending buffers:
  void savePendingBuffer(demux_packet_t* dp);
  demux_packet_t* getPendingBuffer();

  // For H264 over rtsp using AVParser, the next packet has to be saved
  demux_packet_t* nextpacket;

private:
  demux_packet_t* pendingDPHead;
  demux_packet_t* pendingDPTail;

  FramedSource* fReadSource;
  RTPSource* fRTPSource;
  demuxer_t* fOurDemuxer;
  char const* fTag; // used for debugging
};

// A structure of RTP-specific state, kept so that we can cleanly
// reclaim it:
typedef struct RTPState {
  char const* sdpDescription;
  RTSPClient* rtspClient;
  SIPClient* sipClient;
  MediaSession* mediaSession;
  ReadBufferQueue* audioBufferQueue;
  ReadBufferQueue* videoBufferQueue;
  unsigned flags;
  struct timeval firstSyncTime;
};

extern "C" char* network_username;
extern "C" char* network_password;
static char* openURL_rtsp(RTSPClient* client, char const* url) {
  // If we were given a user name (and optional password), then use them: 
  if (network_username != NULL) {
    char const* password = network_password == NULL ? "" : network_password;
    return client->describeWithPassword(url, network_username, password);
  } else {
    return client->describeURL(url);
  }
}

static char* openURL_sip(SIPClient* client, char const* url) {
  // If we were given a user name (and optional password), then use them: 
  if (network_username != NULL) {
    char const* password = network_password == NULL ? "" : network_password;
    return client->inviteWithPassword(url, network_username, password);
  } else {
    return client->invite(url);
  }
}

int rtspStreamOverTCP = 0; 
extern int rtsp_port;

extern "C" int audio_id, video_id, dvdsub_id;
extern "C" demuxer_t* demux_open_rtp(demuxer_t* demuxer) {
  Boolean success = False;
  do {
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    if (scheduler == NULL) break;
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
    if (env == NULL) break;

    RTSPClient* rtspClient = NULL;
    SIPClient* sipClient = NULL;

    if (demuxer == NULL || demuxer->stream == NULL) break;  // shouldn't happen
    demuxer->stream->eof = 0; // just in case 

    // Look at the stream's 'priv' field to see if we were initiated
    // via a SDP description:
    char* sdpDescription = (char*)(demuxer->stream->priv);
    if (sdpDescription == NULL) {
      // We weren't given a SDP description directly, so assume that
      // we were given a RTSP or SIP URL:
      char const* protocol = demuxer->stream->streaming_ctrl->url->protocol;
      char const* url = demuxer->stream->streaming_ctrl->url->url;
      extern int verbose;
      if (strcmp(protocol, "rtsp") == 0) {
	rtspClient = RTSPClient::createNew(*env, verbose, "MPlayer");
	if (rtspClient == NULL) {
	  fprintf(stderr, "Failed to create RTSP client: %s\n",
		  env->getResultMsg());
	  break;
	}
	sdpDescription = openURL_rtsp(rtspClient, url);
      } else { // SIP
	unsigned char desiredAudioType = 0; // PCMU (use 3 for GSM)
	sipClient = SIPClient::createNew(*env, desiredAudioType, NULL,
					 verbose, "MPlayer");
	if (sipClient == NULL) {
	  fprintf(stderr, "Failed to create SIP client: %s\n",
		  env->getResultMsg());
	  break;
	}
	sipClient->setClientStartPortNum(8000);
	sdpDescription = openURL_sip(sipClient, url);
      }

      if (sdpDescription == NULL) {
	fprintf(stderr, "Failed to get a SDP description from URL \"%s\": %s\n",
		url, env->getResultMsg());
	break;
      }
    }

    // Now that we have a SDP description, create a MediaSession from it:
    MediaSession* mediaSession = MediaSession::createNew(*env, sdpDescription);
    if (mediaSession == NULL) break;


    // Create a 'RTPState' structure containing the state that we just created,
    // and store it in the demuxer's 'priv' field, for future reference:
    RTPState* rtpState = new RTPState;
    rtpState->sdpDescription = sdpDescription;
    rtpState->rtspClient = rtspClient;
    rtpState->sipClient = sipClient;
    rtpState->mediaSession = mediaSession;
    rtpState->audioBufferQueue = rtpState->videoBufferQueue = NULL;
    rtpState->flags = 0;
    rtpState->firstSyncTime.tv_sec = rtpState->firstSyncTime.tv_usec = 0;
    demuxer->priv = rtpState;

    int audiofound = 0, videofound = 0;
    // Create RTP receivers (sources) for each subsession:
    MediaSubsessionIterator iter(*mediaSession);
    MediaSubsession* subsession;
    unsigned desiredReceiveBufferSize;
    while ((subsession = iter.next()) != NULL) {
      // Ignore any subsession that's not audio or video:
      if (strcmp(subsession->mediumName(), "audio") == 0) {
	if (audiofound) {
	  fprintf(stderr, "Additional subsession \"audio/%s\" skipped\n", subsession->codecName());
	  continue;
	}
	desiredReceiveBufferSize = 100000;
      } else if (strcmp(subsession->mediumName(), "video") == 0) {
	if (videofound) {
	  fprintf(stderr, "Additional subsession \"video/%s\" skipped\n", subsession->codecName());
	  continue;
	}
	desiredReceiveBufferSize = 2000000;
      } else {
	continue;
      }

      if (rtsp_port)
          subsession->setClientPortNum (rtsp_port);
      
      if (!subsession->initiate()) {
	fprintf(stderr, "Failed to initiate \"%s/%s\" RTP subsession: %s\n", subsession->mediumName(), subsession->codecName(), env->getResultMsg());
      } else {
	fprintf(stderr, "Initiated \"%s/%s\" RTP subsession on port %d\n", subsession->mediumName(), subsession->codecName(), subsession->clientPortNum());

	// Set the OS's socket receive buffer sufficiently large to avoid
	// incoming packets getting dropped between successive reads from this
	// subsession's demuxer.  Depending on the bitrate(s) that you expect,
	// you may wish to tweak the "desiredReceiveBufferSize" values above.
	int rtpSocketNum = subsession->rtpSource()->RTPgs()->socketNum();
	int receiveBufferSize
	  = increaseReceiveBufferTo(*env, rtpSocketNum,
				    desiredReceiveBufferSize);
	if (verbose > 0) {
	  fprintf(stderr, "Increased %s socket receive buffer to %d bytes \n",
		  subsession->mediumName(), receiveBufferSize);
	}

	if (rtspClient != NULL) {
	  // Issue a RTSP "SETUP" command on the chosen subsession:
	  if (!rtspClient->setupMediaSubsession(*subsession, False,
						rtspStreamOverTCP)) break;
	  if (!strcmp(subsession->mediumName(), "audio"))
	    audiofound = 1;
	  if (!strcmp(subsession->mediumName(), "video"))
            videofound = 1;
	}
      }
    }

    if (rtspClient != NULL) {
      // Issue a RTSP aggregate "PLAY" command on the whole session:
      if (!rtspClient->playMediaSession(*mediaSession)) break;
    } else if (sipClient != NULL) {
      sipClient->sendACK(); // to start the stream flowing
    }

    // Now that the session is ready to be read, do additional
    // MPlayer codec-specific initialization on each subsession:
    iter.reset();
    while ((subsession = iter.next()) != NULL) {
      if (subsession->readSource() == NULL) continue; // not reading this

      unsigned flags = 0;
      if (strcmp(subsession->mediumName(), "audio") == 0) {
	rtpState->audioBufferQueue
	  = new ReadBufferQueue(subsession, demuxer, "audio");
	rtpState->audioBufferQueue->otherQueue = &(rtpState->videoBufferQueue);
	rtpCodecInitialize_audio(demuxer, subsession, flags);
      } else if (strcmp(subsession->mediumName(), "video") == 0) {
	rtpState->videoBufferQueue
	  = new ReadBufferQueue(subsession, demuxer, "video");
	rtpState->videoBufferQueue->otherQueue = &(rtpState->audioBufferQueue);
	rtpCodecInitialize_video(demuxer, subsession, flags);
      }
      rtpState->flags |= flags;
    }
    success = True;
  } while (0);
  if (!success) return NULL; // an error occurred

  // Hack: If audio and video are demuxed together on a single RTP stream,
  // then create a new "demuxer_t" structure to allow the higher-level
  // code to recognize this:
  if (demux_is_multiplexed_rtp_stream(demuxer)) {
    stream_t* s = new_ds_stream(demuxer->video);
    demuxer_t* od = demux_open(s, DEMUXER_TYPE_UNKNOWN,
			       audio_id, video_id, dvdsub_id, NULL);
    demuxer = new_demuxers_demuxer(od, od, od);
  }

  return demuxer;
}

extern "C" int demux_is_mpeg_rtp_stream(demuxer_t* demuxer) {
  // Get the RTP state that was stored in the demuxer's 'priv' field:
  RTPState* rtpState = (RTPState*)(demuxer->priv);

  return (rtpState->flags&RTPSTATE_IS_MPEG12_VIDEO) != 0;
}

extern "C" int demux_is_multiplexed_rtp_stream(demuxer_t* demuxer) {
  // Get the RTP state that was stored in the demuxer's 'priv' field:
  RTPState* rtpState = (RTPState*)(demuxer->priv);

  return (rtpState->flags&RTPSTATE_IS_MULTIPLEXED) != 0;
}

static demux_packet_t* getBuffer(demuxer_t* demuxer, demux_stream_t* ds,
				 Boolean mustGetNewData,
				 float& ptsBehind); // forward

extern "C" int demux_rtp_fill_buffer(demuxer_t* demuxer, demux_stream_t* ds) {
  // Get a filled-in "demux_packet" from the RTP source, and deliver it.
  // Note that this is called as a synchronous read operation, so it needs
  // to block in the (hopefully infrequent) case where no packet is
  // immediately available.

  while (1) {
    float ptsBehind;
    demux_packet_t* dp = getBuffer(demuxer, ds, False, ptsBehind); // blocking
    if (dp == NULL) return 0;

    if (demuxer->stream->eof) return 0; // source stream has closed down
  
    // Before using this packet, check to make sure that its presentation
    // time is not far behind the other stream (if any).  If it is,
    // then we discard this packet, and get another instead.  (The rest of
    // MPlayer doesn't always do a good job of synchronizing when the
    // audio and video streams get this far apart.)
    // (We don't do this when streaming over TCP, because then the audio and
    // video streams are interleaved.)
    // (Also, if the stream is *excessively* far behind, then we allow
    // the packet, because in this case it probably means that there was
    // an error in the source's timestamp synchronization.)
    const float ptsBehindThreshold = 1.0; // seconds
    const float ptsBehindLimit = 60.0; // seconds
    if (ptsBehind < ptsBehindThreshold ||
	ptsBehind > ptsBehindLimit ||
	rtspStreamOverTCP) { // packet's OK
      ds_add_packet(ds, dp);
      break;
    }
    
#ifdef DEBUG_PRINT_DISCARDED_PACKETS
    RTPState* rtpState = (RTPState*)(demuxer->priv);
    ReadBufferQueue* bufferQueue = ds == demuxer->video ? rtpState->videoBufferQueue : rtpState->audioBufferQueue;
    fprintf(stderr, "Discarding %s packet (%fs behind)\n", bufferQueue->tag(), ptsBehind);
#endif
    free_demux_packet(dp); // give back this packet, and get another one
  }

  return 1;
}

Boolean awaitRTPPacket(demuxer_t* demuxer, demux_stream_t* ds,
		       unsigned char*& packetData, unsigned& packetDataLen,
		       float& pts) {
  // Similar to "demux_rtp_fill_buffer()", except that the "demux_packet"
  // is not delivered to the "demux_stream".
  float ptsBehind;
  demux_packet_t* dp = getBuffer(demuxer, ds, True, ptsBehind); // blocking
  if (dp == NULL) return False;

  packetData = dp->buffer;
  packetDataLen = dp->len;
  pts = dp->pts;

  return True;
}

static void teardownRTSPorSIPSession(RTPState* rtpState); // forward

extern "C" void demux_close_rtp(demuxer_t* demuxer) {
  // Reclaim all RTP-related state:

  // Get the RTP state that was stored in the demuxer's 'priv' field:
  RTPState* rtpState = (RTPState*)(demuxer->priv);
  if (rtpState == NULL) return;

  teardownRTSPorSIPSession(rtpState);

  UsageEnvironment* env = NULL;
  TaskScheduler* scheduler = NULL;
  if (rtpState->mediaSession != NULL) {
    env = &(rtpState->mediaSession->envir());
    scheduler = &(env->taskScheduler());
  }
  Medium::close(rtpState->mediaSession);
  Medium::close(rtpState->rtspClient);
  Medium::close(rtpState->sipClient);
  delete rtpState->audioBufferQueue;
  delete rtpState->videoBufferQueue;
  delete rtpState->sdpDescription;
  delete rtpState;

  env->reclaim(); delete scheduler;
}

////////// Extra routines that help implement the above interface functions:

#define MAX_RTP_FRAME_SIZE 5000000
    // >= the largest conceivable frame composed from one or more RTP packets

static void afterReading(void* clientData, unsigned frameSize,
			 unsigned /*numTruncatedBytes*/,
			 struct timeval presentationTime,
			 unsigned /*durationInMicroseconds*/) {
  int headersize = 0;
  if (frameSize >= MAX_RTP_FRAME_SIZE) {
    fprintf(stderr, "Saw an input frame too large (>=%d).  Increase MAX_RTP_FRAME_SIZE in \"demux_rtp.cpp\".\n",
	    MAX_RTP_FRAME_SIZE);
  }
  ReadBufferQueue* bufferQueue = (ReadBufferQueue*)clientData;
  demuxer_t* demuxer = bufferQueue->ourDemuxer();
  RTPState* rtpState = (RTPState*)(demuxer->priv);

  if (frameSize > 0) demuxer->stream->eof = 0;

  demux_packet_t* dp = bufferQueue->dp;

  if (bufferQueue->readSource()->isAMRAudioSource())
    headersize = 1;
  else if (bufferQueue == rtpState->videoBufferQueue &&
      ((sh_video_t*)demuxer->video->sh)->format == mmioFOURCC('H','2','6','4')) {
    dp->buffer[0]=0x00;
    dp->buffer[1]=0x00;
    dp->buffer[2]=0x01;
    headersize = 3;
  }

  resize_demux_packet(dp, frameSize + headersize);

  // Set the packet's presentation time stamp, depending on whether or
  // not our RTP source's timestamps have been synchronized yet: 
  Boolean hasBeenSynchronized
    = bufferQueue->rtpSource()->hasBeenSynchronizedUsingRTCP();
  if (hasBeenSynchronized) {
    if (verbose > 0 && !bufferQueue->prevPacketWasSynchronized) {
      fprintf(stderr, "%s stream has been synchronized using RTCP \n",
	      bufferQueue->tag());
    }

    struct timeval* fst = &(rtpState->firstSyncTime); // abbrev
    if (fst->tv_sec == 0 && fst->tv_usec == 0) {
      *fst = presentationTime;
    }
    
    // For the "pts" field, use the time differential from the first
    // synchronized time, rather than absolute time, in order to avoid
    // round-off errors when converting to a float:
    dp->pts = presentationTime.tv_sec - fst->tv_sec
      + (presentationTime.tv_usec - fst->tv_usec)/1000000.0;
    bufferQueue->prevPacketPTS = dp->pts;
  } else {
    if (verbose > 0 && bufferQueue->prevPacketWasSynchronized) {
      fprintf(stderr, "%s stream is no longer RTCP-synchronized \n",
	      bufferQueue->tag());
    }

    // use the previous packet's "pts" once again:
    dp->pts = bufferQueue->prevPacketPTS;
  }
  bufferQueue->prevPacketWasSynchronized = hasBeenSynchronized;

  dp->pos = demuxer->filepos;
  demuxer->filepos += frameSize + headersize;

  // Signal any pending 'doEventLoop()' call on this queue:
  bufferQueue->blockingFlag = ~0;
}

static void onSourceClosure(void* clientData) {
  ReadBufferQueue* bufferQueue = (ReadBufferQueue*)clientData;
  demuxer_t* demuxer = bufferQueue->ourDemuxer();

  demuxer->stream->eof = 1;

  // Signal any pending 'doEventLoop()' call on this queue:
  bufferQueue->blockingFlag = ~0;
}

static demux_packet_t* getBuffer(demuxer_t* demuxer, demux_stream_t* ds,
				 Boolean mustGetNewData,
				 float& ptsBehind) {
  // Begin by finding the buffer queue that we want to read from:
  // (Get this from the RTP state, which we stored in
  //  the demuxer's 'priv' field)
  RTPState* rtpState = (RTPState*)(demuxer->priv);
  ReadBufferQueue* bufferQueue = NULL;
  int headersize = 0;
  TaskToken task;

  if (demuxer->stream->eof) return NULL;

  if (ds == demuxer->video) {
    bufferQueue = rtpState->videoBufferQueue;
    if (((sh_video_t*)ds->sh)->format == mmioFOURCC('H','2','6','4'))
      headersize = 3;
  } else if (ds == demuxer->audio) {
    bufferQueue = rtpState->audioBufferQueue;
    if (bufferQueue->readSource()->isAMRAudioSource())
      headersize = 1;
  } else {
    fprintf(stderr, "(demux_rtp)getBuffer: internal error: unknown stream\n");
    return NULL;
  }

  if (bufferQueue == NULL || bufferQueue->readSource() == NULL) {
    fprintf(stderr, "(demux_rtp)getBuffer failed: no appropriate RTP subsession has been set up\n");
    return NULL;
  }
  
  demux_packet_t* dp = NULL;
  if (!mustGetNewData) {
    // Check whether we have a previously-saved buffer that we can use:
    dp = bufferQueue->getPendingBuffer();
    if (dp != NULL) {
      ptsBehind = 0.0; // so that we always accept this data
      return dp;
    }
  }

  // Allocate a new packet buffer, and arrange to read into it:
    if (!bufferQueue->nextpacket) {
  dp = new_demux_packet(MAX_RTP_FRAME_SIZE);
  bufferQueue->dp = dp;
  if (dp == NULL) return NULL;
    }

#ifdef CONFIG_LIBAVCODEC
  extern AVCodecParserContext * h264parserctx;
  int consumed, poutbuf_size = 1;
  const uint8_t *poutbuf = NULL;
  float lastpts = 0.0;

  do {
    if (!bufferQueue->nextpacket) {
#endif
  // Schedule the read operation:
  bufferQueue->blockingFlag = 0;
  bufferQueue->readSource()->getNextFrame(&dp->buffer[headersize], MAX_RTP_FRAME_SIZE - headersize,
					  afterReading, bufferQueue,
					  onSourceClosure, bufferQueue);
  // Block ourselves until data becomes available:
  TaskScheduler& scheduler
    = bufferQueue->readSource()->envir().taskScheduler();
  int delay = 10000000;
  if (bufferQueue->prevPacketPTS * 1.05 > rtpState->mediaSession->playEndTime())
    delay /= 10;
  task = scheduler.scheduleDelayedTask(delay, onSourceClosure, bufferQueue);
  scheduler.doEventLoop(&bufferQueue->blockingFlag);
  scheduler.unscheduleDelayedTask(task);
  if (demuxer->stream->eof) {
    free_demux_packet(dp);
    return NULL;
  }

  if (headersize == 1) // amr
    dp->buffer[0] =
        ((AMRAudioSource*)bufferQueue->readSource())->lastFrameHeader();
#ifdef CONFIG_LIBAVCODEC
    } else {
      bufferQueue->dp = dp = bufferQueue->nextpacket;
      bufferQueue->nextpacket = NULL;
    }
    if (headersize == 3 && h264parserctx) { // h264
      consumed = h264parserctx->parser->parser_parse(h264parserctx,
                               NULL,
                               &poutbuf, &poutbuf_size,
                               dp->buffer, dp->len);

      if (!consumed && !poutbuf_size)
        return NULL;

      if (!poutbuf_size) {
        lastpts=dp->pts;
        free_demux_packet(dp);
        bufferQueue->dp = dp = new_demux_packet(MAX_RTP_FRAME_SIZE);
      } else {
        bufferQueue->nextpacket = dp;
        bufferQueue->dp = dp = new_demux_packet(poutbuf_size);
        memcpy(dp->buffer, poutbuf, poutbuf_size);
        dp->pts=lastpts;
      }
    }
  } while (!poutbuf_size);
#endif

  // Set the "ptsBehind" result parameter:
  if (bufferQueue->prevPacketPTS != 0.0
      && bufferQueue->prevPacketWasSynchronized
      && *(bufferQueue->otherQueue) != NULL
      && (*(bufferQueue->otherQueue))->prevPacketPTS != 0.0
      && (*(bufferQueue->otherQueue))->prevPacketWasSynchronized) {
    ptsBehind = (*(bufferQueue->otherQueue))->prevPacketPTS
		 - bufferQueue->prevPacketPTS;
  } else {
    ptsBehind = 0.0;
  }

  if (mustGetNewData) {
    // Save this buffer for future reads:
    bufferQueue->savePendingBuffer(dp);
  }

  return dp;
}

static void teardownRTSPorSIPSession(RTPState* rtpState) {
  MediaSession* mediaSession = rtpState->mediaSession;
  if (mediaSession == NULL) return;
  if (rtpState->rtspClient != NULL) {
    rtpState->rtspClient->teardownMediaSession(*mediaSession);
  } else if (rtpState->sipClient != NULL) {
    rtpState->sipClient->sendBYE();
  }
}

////////// "ReadBuffer" and "ReadBufferQueue" implementation:

ReadBufferQueue::ReadBufferQueue(MediaSubsession* subsession,
				 demuxer_t* demuxer, char const* tag)
  : prevPacketWasSynchronized(False), prevPacketPTS(0.0), otherQueue(NULL),
    dp(NULL), nextpacket(NULL),
    pendingDPHead(NULL), pendingDPTail(NULL),
    fReadSource(subsession == NULL ? NULL : subsession->readSource()),
    fRTPSource(subsession == NULL ? NULL : subsession->rtpSource()),
    fOurDemuxer(demuxer), fTag(strdup(tag)) {
} 

ReadBufferQueue::~ReadBufferQueue() {
  delete fTag;

  // Free any pending buffers (that never got delivered):
  demux_packet_t* dp = pendingDPHead;
  while (dp != NULL) {
    demux_packet_t* dpNext = dp->next;
    dp->next = NULL;
    free_demux_packet(dp);
    dp = dpNext;
  }
}

void ReadBufferQueue::savePendingBuffer(demux_packet_t* dp) {
  // Keep this buffer around, until MPlayer asks for it later:
  if (pendingDPTail == NULL) {
    pendingDPHead = pendingDPTail = dp;
  } else {
    pendingDPTail->next = dp;
    pendingDPTail = dp;
  }
  dp->next = NULL;
}

demux_packet_t* ReadBufferQueue::getPendingBuffer() {
  demux_packet_t* dp = pendingDPHead;
  if (dp != NULL) {
    pendingDPHead = dp->next;
    if (pendingDPHead == NULL) pendingDPTail = NULL; 

    dp->next = NULL;
  }

  return dp;
}

static int demux_rtp_control(struct demuxer_st *demuxer, int cmd, void *arg) {
  double endpts = ((RTPState*)demuxer->priv)->mediaSession->playEndTime();

  switch(cmd) {
    case DEMUXER_CTRL_GET_TIME_LENGTH:
      if (endpts <= 0)
        return DEMUXER_CTRL_DONTKNOW;
      *((double *)arg) = endpts;
      return DEMUXER_CTRL_OK;

    case DEMUXER_CTRL_GET_PERCENT_POS:
      if (endpts <= 0)
        return DEMUXER_CTRL_DONTKNOW;
      *((int *)arg) = (int)(((RTPState*)demuxer->priv)->videoBufferQueue->prevPacketPTS*100/endpts);
      return DEMUXER_CTRL_OK;

    default:
      return DEMUXER_CTRL_NOTIMPL;
    }
}

demuxer_desc_t demuxer_desc_rtp = {
  "LIVE555 RTP demuxer",
  "live555",
  "",
  "Ross Finlayson",
  "requires LIVE555 Streaming Media library",
  DEMUXER_TYPE_RTP,
  0, // no autodetect
  NULL,
  demux_rtp_fill_buffer,
  demux_open_rtp,
  demux_close_rtp,
  NULL,
  demux_rtp_control
};
