////////// Routines (with C-linkage) that interface between "MPlayer"
////////// and the "LIVE.COM Streaming Media" libraries:

extern "C" {
#include "demux_rtp.h"
#include "stheader.h"
}
#include "demux_rtp_internal.h"

#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"
#include <unistd.h>

extern "C" stream_t* stream_open_sdp(int fd, off_t fileSize,
				     int* file_format) {
  *file_format = DEMUXER_TYPE_RTP;
  stream_t* newStream = NULL;
  do {
    char* sdpDescription = (char*)malloc(fileSize+1);
    if (sdpDescription == NULL) break;

    ssize_t numBytesRead = read(fd, sdpDescription, fileSize);
    if (numBytesRead != fileSize) break;
    sdpDescription[fileSize] = '\0'; // to be safe

    newStream = (stream_t*)calloc(sizeof (stream_t), 1);
    if (newStream == NULL) break;

    // Store the SDP description in the 'priv' field, for later use:
    newStream->priv = sdpDescription; 
  } while (0);
  return newStream;
}

extern "C" int _rtsp_streaming_seek(int /*fd*/, off_t /*pos*/,
				    streaming_ctrl_t* /*streaming_ctrl*/) {
  return -1; // For now, we don't handle RTSP stream seeking
}

extern "C" int rtsp_streaming_start(stream_t* stream) {
  stream->streaming_ctrl->streaming_seek = _rtsp_streaming_seek;

  return 0;
}

// A data structure representing a buffer being read:
class ReadBufferQueue; // forward
class ReadBuffer {
public:
  ReadBuffer(ReadBufferQueue* ourQueue, demux_packet_t* dp);
  virtual ~ReadBuffer();
  Boolean enqueue();

  demux_packet_t* dp() const { return fDP; }
  ReadBufferQueue* ourQueue() { return fOurQueue; }

  ReadBuffer* next;
private:
  demux_packet_t* fDP;
  ReadBufferQueue* fOurQueue;
};

class ReadBufferQueue {
public:
  ReadBufferQueue(MediaSubsession* subsession, demuxer_t* demuxer,
		  char const* tag);
  virtual ~ReadBufferQueue();

  ReadBuffer* dequeue();

  FramedSource* readSource() const { return fReadSource; }
  RTPSource* rtpSource() const { return fRTPSource; }
  demuxer_t* ourDemuxer() const { return fOurDemuxer; }
  char const* tag() const { return fTag; }

  ReadBuffer* head;
  ReadBuffer* tail;
  char blockingFlag; // used to implement synchronous reads
  unsigned counter; // used for debugging
private:
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
  MediaSession* mediaSession;
  ReadBufferQueue* audioBufferQueue;
  ReadBufferQueue* videoBufferQueue;
  unsigned flags;
  struct timeval firstSyncTime;
};

int rtspStreamOverTCP = 0; 

extern "C" void demux_open_rtp(demuxer_t* demuxer) {
  if (rtspStreamOverTCP && LIVEMEDIA_LIBRARY_VERSION_INT < 1033689600) {
    fprintf(stderr, "TCP streaming of RTP/RTCP requires \"LIVE.COM Streaming Media\" library version 2002.10.04 or later - ignoring the \"-rtsp-stream-over-tcp\" flag\n");
    rtspStreamOverTCP = 0;
  }
  do {
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    if (scheduler == NULL) break;
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
    if (env == NULL) break;

    RTSPClient* rtspClient = NULL;
    unsigned flags = 0;

    // Look at the stream's 'priv' field to see if we were initiated
    // via a SDP description:
    char* sdpDescription = (char*)(demuxer->stream->priv);
    if (sdpDescription == NULL) {
      // We weren't given a SDP description directly, so assume that
      // we were give a RTSP URL
      char const* url = demuxer->stream->streaming_ctrl->url->url;

      extern int verbose;
      rtspClient = RTSPClient::createNew(*env, verbose, "MPlayer");
      if (rtspClient == NULL) {
	fprintf(stderr, "Failed to create RTSP client: %s\n",
		env->getResultMsg());
	break;
      }

      sdpDescription = rtspClient->describeURL(url);
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
    rtpState->mediaSession = mediaSession;
    rtpState->audioBufferQueue = rtpState->videoBufferQueue = NULL;
    rtpState->firstSyncTime.tv_sec = rtpState->firstSyncTime.tv_usec = 0;
    demuxer->priv = rtpState;

    // Create RTP receivers (sources) for each subsession:
    MediaSubsessionIterator iter(*mediaSession);
    MediaSubsession* subsession;
    unsigned streamType = 0; // 0 => video; 1 => audio
    while ((subsession = iter.next()) != NULL) {
      // Ignore any subsession that's not audio or video:
      if (strcmp(subsession->mediumName(), "audio") == 0) {
	streamType = 1;
      } else if (strcmp(subsession->mediumName(), "video") == 0) {
	streamType = 0;
      } else {
	continue;
      }

      if (!subsession->initiate()) {
	fprintf(stderr, "Failed to initiate \"%s/%s\" RTP subsession: %s\n", subsession->mediumName(), subsession->codecName(), env->getResultMsg());
      } else {
	fprintf(stderr, "Initiated \"%s/%s\" RTP subsession\n", subsession->mediumName(), subsession->codecName());

	if (rtspClient != NULL) {
	  // Issue RTSP "SETUP" and "PLAY" commands on the chosen subsession:
	  if (!rtspClient->setupMediaSubsession(*subsession, False,
						rtspStreamOverTCP)) break;
	  if (!rtspClient->playMediaSubsession(*subsession)) break;
	}

	// Now that the subsession is ready to be read, do additional
	// MPlayer codec-specific initialization on it:
	if (streamType == 0) { // video
	  rtpState->videoBufferQueue
	    = new ReadBufferQueue(subsession, demuxer, "video");
	  rtpCodecInitialize_video(demuxer, subsession, flags);
	} else { // audio
	  rtpState->audioBufferQueue
	    = new ReadBufferQueue(subsession, demuxer, "audio");
	  rtpCodecInitialize_audio(demuxer, subsession, flags);
	}
      }
    }
    rtpState->flags = flags;
  } while (0);
}

extern "C" int demux_is_mpeg_rtp_stream(demuxer_t* demuxer) {
  // Get the RTP state that was stored in the demuxer's 'priv' field:
  RTPState* rtpState = (RTPState*)(demuxer->priv);

  return (rtpState->flags&RTPSTATE_IS_MPEG) != 0;
}

static ReadBuffer* getBuffer(ReadBufferQueue* bufferQueue,
			     demuxer_t* demuxer); // forward

extern "C" int demux_rtp_fill_buffer(demuxer_t* demuxer, demux_stream_t* ds) {
  // Get a filled-in "demux_packet" from the RTP source, and deliver it.
  // Note that this is called as a synchronous read operation, so it needs
  // to block in the (hopefully infrequent) case where no packet is
  // immediately available.

  // Begin by finding the buffer queue that we want to read from:
  // (Get this from the RTP state, which we stored in
  //  the demuxer's 'priv' field)
  RTPState* rtpState = (RTPState*)(demuxer->priv);
  ReadBufferQueue* bufferQueue = NULL;
  if (ds == demuxer->video) {
    bufferQueue = rtpState->videoBufferQueue;
  } else if (ds == demuxer->audio) {
    bufferQueue = rtpState->audioBufferQueue;
  } else {
    fprintf(stderr, "demux_rtp_fill_buffer: internal error: unknown stream\n");
    return 0;
  }

  if (bufferQueue == NULL || bufferQueue->readSource() == NULL) {
    fprintf(stderr, "demux_rtp_fill_buffer failed: no appropriate RTP subsession has been set up\n");
    return 0;
  }
  
  ReadBuffer* readBuffer = getBuffer(bufferQueue, demuxer); // blocking
  if (readBuffer != NULL) ds_add_packet(ds, readBuffer->dp());

  if (demuxer->stream->eof) return 0; // source stream has closed down

  return 1;
}

Boolean awaitRTPPacket(demuxer_t* demuxer, unsigned streamType,
		       unsigned char*& packetData, unsigned& packetDataLen) {
  // Begin by finding the buffer queue that we want to read from:
  // (Get this from the RTP state, which we stored in
  //  the demuxer's 'priv' field)
  RTPState* rtpState = (RTPState*)(demuxer->priv);
  ReadBufferQueue* bufferQueue = NULL;
  if (streamType == 0) {
    bufferQueue = rtpState->videoBufferQueue;
  } else if (streamType == 1) {
    bufferQueue = rtpState->audioBufferQueue;
  } else {
    fprintf(stderr, "awaitRTPPacket: internal error: unknown streamType %d\n",
	    streamType);
    return False;
  }

  if (bufferQueue == NULL || bufferQueue->readSource() == NULL) {
    fprintf(stderr, "awaitRTPPacket failed: no appropriate RTP subsession has been set up\n");
    return False;
  }
  
  ReadBuffer* readBuffer = getBuffer(bufferQueue, demuxer); // blocking
  if (readBuffer == NULL) return False;

  demux_packet_t* dp = readBuffer->dp();
  packetData = dp->buffer;
  packetDataLen = dp->len;

  return True;
}

extern "C" void demux_close_rtp(demuxer_t* demuxer) {
  // Reclaim all RTP-related state:

  // Get the RTP state that was stored in the demuxer's 'priv' field:
  RTPState* rtpState = (RTPState*)(demuxer->priv);
  if (rtpState == NULL) return;
  UsageEnvironment* env = NULL;
  TaskScheduler* scheduler = NULL;
  if (rtpState->mediaSession != NULL) {
    env = &(rtpState->mediaSession->envir());
    scheduler = &(env->taskScheduler());
  }
  Medium::close(rtpState->mediaSession);
  Medium::close(rtpState->rtspClient);
  delete rtpState->audioBufferQueue;
  delete rtpState->videoBufferQueue;
  delete rtpState->sdpDescription;
  delete rtpState;

  delete env; delete scheduler;
}

////////// Extra routines that help implement the above interface functions:

static void afterReading(void* clientData, unsigned frameSize,
			 struct timeval presentationTime); // forward
static void onSourceClosure(void* clientData); // forward

static void scheduleNewBufferRead(ReadBufferQueue* bufferQueue) {
  if (bufferQueue->readSource()->isCurrentlyAwaitingData()) return;
      // a read from this source is already in progress

  // Allocate a new packet buffer, and arrange to read into it:
  unsigned const bufferSize = 30000; // >= the largest conceivable RTP packet
  demux_packet_t* dp = new_demux_packet(bufferSize);
  if (dp == NULL) return;
  ReadBuffer* readBuffer = new ReadBuffer(bufferQueue, dp);

  // Schedule the read operation:
  bufferQueue->readSource()->getNextFrame(dp->buffer, bufferSize,
					  afterReading, readBuffer,
					  onSourceClosure, readBuffer);
}

static void afterReading(void* clientData, unsigned frameSize,
			 struct timeval presentationTime) {
  ReadBuffer* readBuffer = (ReadBuffer*)clientData;
  ReadBufferQueue* bufferQueue = readBuffer->ourQueue();
  demuxer_t* demuxer = bufferQueue->ourDemuxer();
  RTPState* rtpState = (RTPState*)(demuxer->priv);

  if (frameSize > 0) demuxer->stream->eof = 0;

  demux_packet_t* dp = readBuffer->dp();
  dp->len = frameSize;

  // Set the packet's presentation time stamp, depending on whether or
  // not our RTP source's timestamps have been synchronized yet: 
  {
    Boolean hasBeenSynchronized
      = bufferQueue->rtpSource()->hasBeenSynchronizedUsingRTCP();
    if (hasBeenSynchronized) {
      struct timeval* fst = &(rtpState->firstSyncTime); // abbrev
      if (fst->tv_sec == 0 && fst->tv_usec == 0) {
	*fst = presentationTime;
      }

      // For the "pts" field, use the time differential from the first
      // synchronized time, rather than absolute time, in order to avoid
      // round-off errors when converting to a float:
      dp->pts = presentationTime.tv_sec - fst->tv_sec
	+ (presentationTime.tv_usec - fst->tv_usec)/1000000.0;
    } else {
      dp->pts = 0.0;
    }
  }

  dp->pos = demuxer->filepos;
  demuxer->filepos += frameSize;
  if (!readBuffer->enqueue()) {
    // The queue is full, so discard the buffer:
    delete readBuffer;
  }

  // Signal any pending 'doEventLoop()' call on this queue:
  bufferQueue->blockingFlag = ~0;

  // Finally, arrange to do another read, if appropriate
  scheduleNewBufferRead(bufferQueue);
}

static void onSourceClosure(void* clientData) {
  ReadBuffer* readBuffer = (ReadBuffer*)clientData;
  ReadBufferQueue* bufferQueue = readBuffer->ourQueue();
  demuxer_t* demuxer = bufferQueue->ourDemuxer();

  demuxer->stream->eof = 1;

  // Signal any pending 'doEventLoop()' call on this queue:
  bufferQueue->blockingFlag = ~0;
}

static ReadBuffer* getBufferIfAvailable(ReadBufferQueue* bufferQueue) {
  ReadBuffer* readBuffer = bufferQueue->dequeue();

  // Arrange to read a new packet into this queue:
  scheduleNewBufferRead(bufferQueue);

  return readBuffer;
}

static ReadBuffer* getBuffer(ReadBufferQueue* bufferQueue,
			     demuxer_t* demuxer) {
  // Check whether there's a full buffer to deliver to the client:
  bufferQueue->blockingFlag = 0;
  ReadBuffer* readBuffer;
  while ((readBuffer = getBufferIfAvailable(bufferQueue)) == NULL
	 && !demuxer->stream->eof) {
    // Because we weren't able to deliver a buffer to the client immediately,
    // block myself until one comes available:
    TaskScheduler& scheduler
      = bufferQueue->readSource()->envir().taskScheduler();
#if USAGEENVIRONMENT_LIBRARY_VERSION_INT >= 1038614400
    scheduler.doEventLoop(&bufferQueue->blockingFlag);
#else
    scheduler.blockMyself(&bufferQueue->blockingFlag);
#endif
  }

  return readBuffer;
}

////////// "ReadBuffer" and "ReadBufferQueue" implementation:

#define MAX_QUEUE_SIZE 5

ReadBuffer::ReadBuffer(ReadBufferQueue* ourQueue, demux_packet_t* dp)
  : next(NULL), fDP(dp), fOurQueue(ourQueue) {
}

Boolean ReadBuffer::enqueue() {
  if (fOurQueue->counter >= MAX_QUEUE_SIZE) {
    // This queue is full.  Clear out an old entry from it, so that
    // this new one will fit:
    while (fOurQueue->counter >= MAX_QUEUE_SIZE) {
      delete fOurQueue->dequeue();
    }
  }

  // Add ourselves to the tail of our queue:
  if (fOurQueue->tail == NULL) {
    fOurQueue->head = this;
  } else {
    fOurQueue->tail->next = this;
  }
  fOurQueue->tail = this;
  ++fOurQueue->counter;

  return True;
}

ReadBuffer::~ReadBuffer() {
  free_demux_packet(fDP);
  delete next;
}

ReadBufferQueue::ReadBufferQueue(MediaSubsession* subsession,
				 demuxer_t* demuxer, char const* tag)
  : head(NULL), tail(NULL), counter(0),
    fReadSource(subsession == NULL ? NULL : subsession->readSource()),
    fRTPSource(subsession == NULL ? NULL : subsession->rtpSource()),
    fOurDemuxer(demuxer), fTag(strdup(tag)) {
} 

ReadBufferQueue::~ReadBufferQueue() {
  delete head;
  delete fTag;
}

ReadBuffer* ReadBufferQueue::dequeue() {
  ReadBuffer* readBuffer = head;
  if (readBuffer != NULL) {
    head = readBuffer->next;
    if (head == NULL) tail = NULL; 
    --counter;
    readBuffer->next = NULL;
  }
  return readBuffer;
}
