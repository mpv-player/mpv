////////// Codec-specific routines used to interface between "MPlayer"
////////// and the "LIVE.COM Streaming Media" libraries:

#include "demux_rtp_internal.h"
extern "C" {
#include "stheader.h"
}

static Boolean
parseQTState_video(QuickTimeGenericRTPSource::QTState const& qtState,
		   unsigned& fourcc); // forward
static Boolean
parseQTState_audio(QuickTimeGenericRTPSource::QTState const& qtState,
		   unsigned& fourcc, unsigned& numChannels); // forward
		       
void rtpCodecInitialize_video(demuxer_t* demuxer,
			      MediaSubsession* subsession,
			      unsigned& flags) {
  flags = 0;
  // Create a dummy video stream header
  // to make the main MPlayer code happy:
  sh_video_t* sh_video = new_sh_video(demuxer,0);
  BITMAPINFOHEADER* bih
    = (BITMAPINFOHEADER*)calloc(1,sizeof(BITMAPINFOHEADER));
  bih->biSize = sizeof(BITMAPINFOHEADER);
  sh_video->bih = bih;
  demux_stream_t* d_video = demuxer->video;
  d_video->sh = sh_video; sh_video->ds = d_video;
  
  // If we happen to know the subsession's video frame rate, set it,
  // so that the user doesn't have to give the "-fps" option instead.
  int fps = (int)(subsession->videoFPS());
  if (fps != 0) sh_video->fps = fps;
  
  // Map known video MIME types to the BITMAPINFOHEADER parameters
  // that this program uses.  (Note that not all types need all
  // of the parameters to be set.)
  if (strcmp(subsession->codecName(), "MPV") == 0 ||
      strcmp(subsession->codecName(), "MP1S") == 0 ||
      strcmp(subsession->codecName(), "MP2T") == 0) {
    flags |= RTPSTATE_IS_MPEG;
  } else if (strcmp(subsession->codecName(), "H263") == 0 ||
	     strcmp(subsession->codecName(), "H263-1998") == 0) {
    bih->biCompression = sh_video->format
      = mmioFOURCC('H','2','6','3');
  } else if (strcmp(subsession->codecName(), "H261") == 0) {
    bih->biCompression = sh_video->format
      = mmioFOURCC('H','2','6','1');
  } else if (strcmp(subsession->codecName(), "JPEG") == 0) {
    bih->biCompression = sh_video->format
      = mmioFOURCC('M','J','P','G');
#if (LIVEMEDIA_LIBRARY_VERSION_INT < 1044662400)
    fprintf(stderr, "WARNING: This video stream might not play correctly.  Please upgrade to version \"2003.02.08\" or later of the \"LIVE.COM Streaming Media\" libraries.\n");
#endif
  } else if (strcmp(subsession->codecName(), "MP4V-ES") == 0) {
    bih->biCompression = sh_video->format
      = mmioFOURCC('m','p','4','v');
    //flags |= RTPSTATE_IS_MPEG; // MPEG hdr checking in video.c doesn't work!
  } else if (strcmp(subsession->codecName(), "X-QT") == 0 ||
	     strcmp(subsession->codecName(), "X-QUICKTIME") == 0) {
    // QuickTime generic RTP format, as described in
    // http://developer.apple.com/quicktime/icefloe/dispatch026.html

    // We can't initialize this stream until we've received the first packet
    // that has QuickTime "sdAtom" information in the header.  So, keep
    // reading packets until we get one:
    unsigned char* packetData; unsigned packetDataLen;
    QuickTimeGenericRTPSource* qtRTPSource
      = (QuickTimeGenericRTPSource*)(subsession->rtpSource());
    unsigned fourcc;
    do {
      if (!awaitRTPPacket(demuxer, 0 /*video*/, packetData, packetDataLen)) {
	return;
      }
    } while (!parseQTState_video(qtRTPSource->qtState, fourcc));

    bih->biCompression = sh_video->format = fourcc;
  } else {
    fprintf(stderr,
	    "Unknown MPlayer format code for MIME type \"video/%s\"\n",
	    subsession->codecName());
  }
}

void rtpCodecInitialize_audio(demuxer_t* demuxer,
			      MediaSubsession* subsession,
			      unsigned& flags) {
  flags = 0;
  // Create a dummy audio stream header
  // to make the main MPlayer code happy:
  sh_audio_t* sh_audio = new_sh_audio(demuxer,0);
  WAVEFORMATEX* wf = (WAVEFORMATEX*)calloc(1,sizeof(WAVEFORMATEX));
  sh_audio->wf = wf;
  demux_stream_t* d_audio = demuxer->audio;
  d_audio->sh = sh_audio; sh_audio->ds = d_audio;
  
  // Map known audio MIME types to the WAVEFORMATEX parameters
  // that this program uses.  (Note that not all types need all
  // of the parameters to be set.)
  wf->nSamplesPerSec
    = subsession->rtpSource()->timestampFrequency(); // by default
  if (strcmp(subsession->codecName(), "MPA") == 0 ||
      strcmp(subsession->codecName(), "MPA-ROBUST") == 0 ||
      strcmp(subsession->codecName(), "X-MP3-DRAFT-00") == 0) {
    wf->wFormatTag = sh_audio->format = 0x55;
    // Note: 0x55 is for layer III, but should work for I,II also
    wf->nSamplesPerSec = 0; // sample rate is deduced from the data
    flags |= RTPSTATE_IS_MPEG;
  } else if (strcmp(subsession->codecName(), "AC3") == 0) {
    wf->wFormatTag = sh_audio->format = 0x2000;
    wf->nSamplesPerSec = 0; // sample rate is deduced from the data
  } else if (strcmp(subsession->codecName(), "PCMU") == 0) {
    wf->wFormatTag = sh_audio->format = 0x7;
    wf->nChannels = 1;
    wf->nAvgBytesPerSec = 8000;
    wf->nBlockAlign = 1;
    wf->wBitsPerSample = 8;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "PCMA") == 0) {
    wf->wFormatTag = sh_audio->format = 0x6;
    wf->nChannels = 1;
    wf->nAvgBytesPerSec = 8000;
    wf->nBlockAlign = 1;
    wf->wBitsPerSample = 8;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "GSM") == 0) {
    wf->wFormatTag = sh_audio->format = mmioFOURCC('a','g','s','m');
    wf->nChannels = 1;
    wf->nAvgBytesPerSec = 1650;
    wf->nBlockAlign = 33;
    wf->wBitsPerSample = 16;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "QCELP") == 0) {
    wf->wFormatTag = sh_audio->format = mmioFOURCC('Q','c','l','p');
    // The following settings for QCELP don't quite work right #####
    wf->nChannels = 1;
    wf->nAvgBytesPerSec = 1750;
    wf->nBlockAlign = 35;
    wf->wBitsPerSample = 16;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "MP4A-LATM") == 0) {
    wf->wFormatTag = sh_audio->format = mmioFOURCC('m','p','4','a');
#if (LIVEMEDIA_LIBRARY_VERSION_INT < 1042761600)
    fprintf(stderr, "WARNING: This audio stream might not play correctly.  Please upgrade to version \"2003.01.17\" or later of the \"LIVE.COM Streaming Media\" libraries.\n");
#else
    // For the codec to work correctly, it needs "AudioSpecificConfig"
    // data, which is parsed from the "StreamMuxConfig" string that
    // was present (hopefully) in the SDP description:
    unsigned codecdata_len;
    sh_audio->codecdata
      = parseStreamMuxConfigStr(subsession->fmtp_config(),
				codecdata_len);
    sh_audio->codecdata_len = codecdata_len;
#endif
    flags |= RTPSTATE_IS_MPEG;
  } else if (strcmp(subsession->codecName(), "X-QT") == 0 ||
	     strcmp(subsession->codecName(), "X-QUICKTIME") == 0) {
    // QuickTime generic RTP format, as described in
    // http://developer.apple.com/quicktime/icefloe/dispatch026.html

    // We can't initialize this stream until we've received the first packet
    // that has QuickTime "sdAtom" information in the header.  So, keep
    // reading packets until we get one:
    unsigned char* packetData; unsigned packetDataLen;
    QuickTimeGenericRTPSource* qtRTPSource
      = (QuickTimeGenericRTPSource*)(subsession->rtpSource());
    unsigned fourcc, numChannels;
    do {
      if (!awaitRTPPacket(demuxer, 1 /*audio*/, packetData, packetDataLen)) {
	return;
      }
    } while (!parseQTState_audio(qtRTPSource->qtState, fourcc, numChannels));

    wf->wFormatTag = sh_audio->format = fourcc;
    wf->nChannels = numChannels;
  } else {
    fprintf(stderr,
	    "Unknown MPlayer format code for MIME type \"audio/%s\"\n",
	    subsession->codecName());
  }
}

static Boolean
parseQTState_video(QuickTimeGenericRTPSource::QTState const& qtState,
		   unsigned& fourcc) {
  // qtState's "sdAtom" field is supposed to contain a QuickTime video
  // 'sample description' atom.  This atom's name is the 'fourcc' that we want:
  char const* sdAtom = qtState.sdAtom;
  if (sdAtom == NULL || qtState.sdAtomSize < 2*4) return False;

  fourcc = *(unsigned*)(&sdAtom[4]); // put in host order
  return True;
}

static Boolean
parseQTState_audio(QuickTimeGenericRTPSource::QTState const& qtState,
		   unsigned& fourcc, unsigned& numChannels) {
  // qtState's "sdAtom" field is supposed to contain a QuickTime audio
  // 'sample description' atom.  This atom's name is the 'fourcc' that we want.
  // Also, the top half of the 5th word following the atom name should
  // contain the number of channels ("numChannels") that we want:
  char const* sdAtom = qtState.sdAtom;
  if (sdAtom == NULL || qtState.sdAtomSize < 7*4) return False;

  fourcc = *(unsigned*)(&sdAtom[4]); // put in host order

  char const* word7Ptr = &sdAtom[6*4];
  numChannels = (word7Ptr[0]<<8)|(word7Ptr[1]);
  return True;
}
