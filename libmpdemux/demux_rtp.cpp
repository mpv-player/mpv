extern "C" {
#include "demux_rtp.h"
#include "stheader.h"
}

#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"
#include <unistd.h>

////////// Routines (with C-linkage) that interface between "mplayer"
////////// and the "LIVE.COM Streaming Media" libraries:

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
  demuxer_t* ourDemuxer() const { return fOurDemuxer; }
  char const* tag() const { return fTag; }

  ReadBuffer* head;
  ReadBuffer* tail;
  char blockingFlag; // used to implement synchronous reads
  unsigned counter; // used for debugging
private:
  FramedSource* fReadSource;
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
  int isMPEG; // TRUE for MPEG audio, video, or transport streams
};

extern "C" void demux_open_rtp(demuxer_t* demuxer) {
  do {
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    if (scheduler == NULL) break;
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
    if (env == NULL) break;

    RTSPClient* rtspClient = NULL;
    int isMPEG = 0;

    // Look at the stream's 'priv' field to see if we were initiated
    // via a SDP description:
    char* sdpDescription = (char*)(demuxer->stream->priv);
    if (sdpDescription == NULL) {
      // We weren't given a SDP description directly, so assume that
      // we were give a RTSP URL
      char const* url = demuxer->stream->streaming_ctrl->url->url;

      extern int verbose;
      rtspClient = RTSPClient::createNew(*env, verbose);
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

    // Create RTP receivers (sources) for each subsession:
    MediaSubsessionIterator iter(*mediaSession);
    MediaSubsession* subsession;
    MediaSubsession* audioSubsession = NULL;
    MediaSubsession* videoSubsession = NULL;
    while ((subsession = iter.next()) != NULL) {
      // Ignore any subsession that's not audio or video:
      if (strcmp(subsession->mediumName(), "audio") == 0) {
	audioSubsession = subsession;
      } else if (strcmp(subsession->mediumName(), "video") == 0) {
	videoSubsession = subsession;
      } else {
	continue;
      }

      if (!subsession->initiate()) {
	fprintf(stderr, "Failed to initiate \"%s/%s\" RTP subsession: %s\n", subsession->mediumName(), subsession->codecName(), env->getResultMsg());
      } else {
	fprintf(stderr, "Initiated \"%s/%s\" RTP subsession\n", subsession->mediumName(), subsession->codecName());

	if (rtspClient != NULL) {
	  // Issue RTSP "SETUP" and "PLAY" commands on the chosen subsession:
	  if (!rtspClient->setupMediaSubsession(*subsession)) break;
	  if (!rtspClient->playMediaSubsession(*subsession)) break;
	}

	// Now that the subsession is ready to be read, do additional
	// mplayer-specific initialization on it:
	if (subsession == videoSubsession) {
	  // Create a dummy video stream header
	  // to make the main mplayer code happy:
	  sh_video_t* sh_video = new_sh_video(demuxer,0);
	  demux_stream_t* d_video = demuxer->video;
	  d_video->sh = sh_video; sh_video->ds = d_video;

	  // Map known video MIME types to the format code that this prog uses:
	  if (strcmp(subsession->codecName(), "MPV") == 0 ||
	      strcmp(subsession->codecName(), "MP1S") == 0 ||
	      strcmp(subsession->codecName(), "MP2T") == 0) {
	    isMPEG = 1;
	  } else if (strcmp(subsession->codecName(), "H263") == 0 ||
		     strcmp(subsession->codecName(), "H263-1998") == 0) {
	    sh_video->format = mmioFOURCC('H','2','6','3');
	  } else if (strcmp(subsession->codecName(), "H261") == 0) {
	    sh_video->format = mmioFOURCC('H','2','6','1');
	  } else if (strcmp(subsession->codecName(), "JPEG") == 0) {
	    sh_video->format = mmioFOURCC('M','J','P','G');
	  } else {
	    fprintf(stderr,
		    "Unknown mplayer format code for MIME type \"video/%s\"\n",
		    subsession->codecName());
	  }
	} else if (subsession == audioSubsession) {
	  // Create a dummy audio stream header
	  // to make the main mplayer code happy:
	  sh_audio_t* sh_audio = new_sh_audio(demuxer,0);
	  sh_audio->wf = (WAVEFORMATEX*)calloc(1,sizeof(WAVEFORMATEX));
	  demux_stream_t* d_audio = demuxer->audio;
	  d_audio->sh = sh_audio; sh_audio->ds = d_audio;

	  // Map known audio MIME types to the format code that this prog uses:
	  if (strcmp(subsession->codecName(), "MPA") == 0 ||
	      strcmp(subsession->codecName(), "MPA-ROBUST") == 0 ||
	      strcmp(subsession->codecName(), "X-MP3-DRAFT-00") == 0) {
	    sh_audio->format = 0x50;
	  } else if (strcmp(subsession->codecName(), "AC3") == 0) {
	    sh_audio->format = 0x2000;
	  } else if (strcmp(subsession->codecName(), "PCMU") == 0) {
	    sh_audio->format = 0x7;
	  } else if (strcmp(subsession->codecName(), "PCMA") == 0) {
	    sh_audio->format = 0x6;
	  } else if (strcmp(subsession->codecName(), "GSM") == 0) {
	    sh_audio->format = 0x31;
	  } else {
	    fprintf(stderr,
		    "Unknown mplayer format code for MIME type \"audio/%s\"\n",
		    subsession->codecName());
	  }
	}
      }
    }

    // Hack: Create a 'RTPState' structure containing the state that
    // we just created, and store it in the demuxer's 'priv' field:
    RTPState* rtpState = new RTPState;
    rtpState->sdpDescription = sdpDescription;
    rtpState->rtspClient = rtspClient;
    rtpState->mediaSession = mediaSession;
    rtpState->audioBufferQueue
      = new ReadBufferQueue(audioSubsession, demuxer, "audio");
    rtpState->videoBufferQueue
      = new ReadBufferQueue(videoSubsession, demuxer, "video");
    rtpState->isMPEG = isMPEG;

    demuxer->priv = rtpState;
  } while (0);
}

extern "C" int demux_is_mpeg_rtp_stream(demuxer_t* demuxer) {
  // Get the RTP state that was stored in the demuxer's 'priv' field:
  RTPState* rtpState = (RTPState*)(demuxer->priv);
  return rtpState->isMPEG;
}

static Boolean deliverBufferIfAvailable(ReadBufferQueue* bufferQueue,
					demux_stream_t* ds); // forward

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
  
  // Check whether there's a full buffer to deliver to the client:
  bufferQueue->blockingFlag = 0;
  while (!deliverBufferIfAvailable(bufferQueue, ds)) {
    // Because we weren't able to deliver a buffer to the client immediately,
    // block myself until one comes available:
    TaskScheduler& scheduler
      = bufferQueue->readSource()->envir().taskScheduler();
    scheduler.blockMyself(&bufferQueue->blockingFlag);
  }

  if (demuxer->stream->eof) return 0; // source stream has closed down
  return 1;
}

extern "C" void demux_close_rtp(demuxer_t* demuxer) {
  // Reclaim all RTP-related state:

  // Get the RTP state that was stored in the demuxer's 'priv' field:
  RTPState* rtpState = (RTPState*)(demuxer->priv);
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

static void scheduleNewBufferRead(ReadBufferQueue* bufferQueue); // forward

static Boolean deliverBufferIfAvailable(ReadBufferQueue* bufferQueue,
					demux_stream_t* ds) {
  Boolean deliveredBuffer = False;
  ReadBuffer* readBuffer = bufferQueue->dequeue();
  if (readBuffer != NULL) {
    // Append the packet to the reader's DS stream:
    ds_add_packet(ds, readBuffer->dp());
    deliveredBuffer = True;
  }

  // Arrange to read a new packet into this queue:
  scheduleNewBufferRead(bufferQueue);

  return deliveredBuffer;
}

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
			 struct timeval /*presentationTime*/) {
  ReadBuffer* readBuffer = (ReadBuffer*)clientData;
  ReadBufferQueue* bufferQueue = readBuffer->ourQueue();
  demuxer_t* demuxer = bufferQueue->ourDemuxer();

  if (frameSize > 0) demuxer->stream->eof = 0;

  demux_packet_t* dp = readBuffer->dp();
  dp->len = frameSize;
  dp->pts = 0;
  dp->pos = demuxer->filepos;
  demuxer->filepos += frameSize;
  if (!readBuffer->enqueue()) {
    // The queue is full, so discard the buffer:
    delete readBuffer;
  }

  // Signal any pending 'blockMyself()' call on this queue:
  bufferQueue->blockingFlag = ~0;

  // Finally, arrange to do another read, if appropriate
  scheduleNewBufferRead(bufferQueue);
}

static void onSourceClosure(void* clientData) {
  ReadBuffer* readBuffer = (ReadBuffer*)clientData;
  ReadBufferQueue* bufferQueue = readBuffer->ourQueue();
  demuxer_t* demuxer = bufferQueue->ourDemuxer();

  demuxer->stream->eof = 1;

  // Signal any pending 'blockMyself()' call on this queue:
  bufferQueue->blockingFlag = ~0;
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
