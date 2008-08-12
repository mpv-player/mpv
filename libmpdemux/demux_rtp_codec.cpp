////////// Codec-specific routines used to interface between "MPlayer"
////////// and the "LIVE555 Streaming Media" libraries:

#include "demux_rtp_internal.h"
extern "C" {
#include <limits.h>
#include <math.h>
#include "stheader.h"
#include "libavutil/base64.h"
}

#ifdef CONFIG_LIBAVCODEC
AVCodecParserContext * h264parserctx;
#endif

// Copied from vlc
static unsigned char* parseH264ConfigStr( char const* configStr,
                                          unsigned int& configSize )
{

    char *dup, *psz;
    int i, i_records = 1;

    if( configSize )
    configSize = 0;
    if( configStr == NULL || *configStr == '\0' )
        return NULL;
    psz = dup = strdup( configStr );

 /* Count the number of comma's */
    for( psz = dup; *psz != '\0'; ++psz )
    {
        if( *psz == ',')
        {
            ++i_records;
            *psz = '\0';
        }
    }

    unsigned char *cfg = new unsigned char[5 * strlen(dup)];
    psz = dup;
    for( i = 0; i < i_records; i++ )
    {

        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x00;
        cfg[configSize++] = 0x01;
        configSize += av_base64_decode( (uint8_t*)&cfg[configSize],
                                        psz,
                                        5 * strlen(dup) - 3 );

    psz += strlen(psz)+1;
    }
    if( dup ) free( dup );

    return cfg;
}

static void
needVideoFrameRate(demuxer_t* demuxer, MediaSubsession* subsession); // forward
static Boolean
parseQTState_video(QuickTimeGenericRTPSource::QTState const& qtState,
		   unsigned& fourcc); // forward
static Boolean
parseQTState_audio(QuickTimeGenericRTPSource::QTState const& qtState,
		   unsigned& fourcc, unsigned& numChannels); // forward
		       
static BITMAPINFOHEADER * insertVideoExtradata(BITMAPINFOHEADER *bih,
                                               unsigned char * extraData,
                                               unsigned size)
{
    BITMAPINFOHEADER * original = bih;
    if (!size || size > INT_MAX - sizeof(BITMAPINFOHEADER))
        return bih;
    bih = (BITMAPINFOHEADER*)realloc(bih, sizeof(BITMAPINFOHEADER) + size);
    if (!bih)
        return original;
    bih->biSize = sizeof(BITMAPINFOHEADER) + size;
    memcpy(bih+1, extraData, size);
    return bih;
}

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
  
  // Map known video MIME types to the BITMAPINFOHEADER parameters
  // that this program uses.  (Note that not all types need all
  // of the parameters to be set.)
  if (strcmp(subsession->codecName(), "MPV") == 0) {
    flags |= RTPSTATE_IS_MPEG12_VIDEO;
  } else if (strcmp(subsession->codecName(), "MP1S") == 0 ||
	     strcmp(subsession->codecName(), "MP2T") == 0) {
    flags |= RTPSTATE_IS_MPEG12_VIDEO|RTPSTATE_IS_MULTIPLEXED;
  } else if (strcmp(subsession->codecName(), "H263") == 0 ||
	     strcmp(subsession->codecName(), "H263-2000") == 0 ||
	     strcmp(subsession->codecName(), "H263-1998") == 0) {
    bih->biCompression = sh_video->format
      = mmioFOURCC('H','2','6','3');
    needVideoFrameRate(demuxer, subsession);
  } else if (strcmp(subsession->codecName(), "H264") == 0) {
    bih->biCompression = sh_video->format
      = mmioFOURCC('H','2','6','4');
    unsigned int configLen = 0;
    unsigned char* configData
      = parseH264ConfigStr(subsession->fmtp_spropparametersets(), configLen);
    sh_video->bih = bih = insertVideoExtradata(bih, configData, configLen);
    delete[] configData;
#ifdef CONFIG_LIBAVCODEC
    avcodec_register_all();
    h264parserctx = av_parser_init(CODEC_ID_H264);
#endif
    needVideoFrameRate(demuxer, subsession);
  } else if (strcmp(subsession->codecName(), "H261") == 0) {
    bih->biCompression = sh_video->format
      = mmioFOURCC('H','2','6','1');
    needVideoFrameRate(demuxer, subsession);
  } else if (strcmp(subsession->codecName(), "JPEG") == 0) {
    bih->biCompression = sh_video->format
      = mmioFOURCC('M','J','P','G');
    needVideoFrameRate(demuxer, subsession);
  } else if (strcmp(subsession->codecName(), "MP4V-ES") == 0) {
    bih->biCompression = sh_video->format
      = mmioFOURCC('m','p','4','v');
    // For the codec to work correctly, it may need a 'VOL Header' to be
    // inserted at the front of the data stream.  Construct this from the
    // "config" MIME parameter, which was present (hopefully) in the
    // session's SDP description:
    unsigned configLen;
    unsigned char* configData
      = parseGeneralConfigStr(subsession->fmtp_config(), configLen);
    sh_video->bih = bih = insertVideoExtradata(bih, configData, configLen);
    needVideoFrameRate(demuxer, subsession);
  } else if (strcmp(subsession->codecName(), "X-QT") == 0 ||
	     strcmp(subsession->codecName(), "X-QUICKTIME") == 0) {
    // QuickTime generic RTP format, as described in
    // http://developer.apple.com/quicktime/icefloe/dispatch026.html

    // We can't initialize this stream until we've received the first packet
    // that has QuickTime "sdAtom" information in the header.  So, keep
    // reading packets until we get one:
    unsigned char* packetData; unsigned packetDataLen; float pts;
    QuickTimeGenericRTPSource* qtRTPSource
      = (QuickTimeGenericRTPSource*)(subsession->rtpSource());
    unsigned fourcc;
    do {
      if (!awaitRTPPacket(demuxer, demuxer->video,
			  packetData, packetDataLen, pts)) {
	return;
      }
    } while (!parseQTState_video(qtRTPSource->qtState, fourcc));

    bih->biCompression = sh_video->format = fourcc;
    bih->biWidth = qtRTPSource->qtState.width;
    bih->biHeight = qtRTPSource->qtState.height;
      uint8_t *pos = (uint8_t*)qtRTPSource->qtState.sdAtom + 86;
      uint8_t *endpos = (uint8_t*)qtRTPSource->qtState.sdAtom
                        + qtRTPSource->qtState.sdAtomSize;
      while (pos+8 < endpos) {
        unsigned atomLength = pos[0]<<24 | pos[1]<<16 | pos[2]<<8 | pos[3];
        if (atomLength == 0 || atomLength > endpos-pos) break;
        if ((!memcmp(pos+4, "avcC", 4) && fourcc==mmioFOURCC('a','v','c','1') || 
             !memcmp(pos+4, "esds", 4) || 
             !memcmp(pos+4, "SMI ", 4) && fourcc==mmioFOURCC('S','V','Q','3')) &&
            atomLength > 8) {
          sh_video->bih = bih = 
              insertVideoExtradata(bih, pos+8, atomLength-8);
          break;
        }
        pos += atomLength;
      }
    needVideoFrameRate(demuxer, subsession);
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
  d_audio->id = sh_audio->aid;
  
  wf->nChannels = subsession->numChannels();

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
  } else if (strcmp(subsession->codecName(), "AC3") == 0) {
    wf->wFormatTag = sh_audio->format = 0x2000;
    wf->nSamplesPerSec = 0; // sample rate is deduced from the data
  } else if (strcmp(subsession->codecName(), "L16") == 0) {
    wf->wFormatTag = sh_audio->format = 0x736f7774; // "twos"
    wf->nBlockAlign = 1;
    wf->wBitsPerSample = 16;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "L8") == 0) {
    wf->wFormatTag = sh_audio->format = 0x20776172; // "raw "
    wf->nBlockAlign = 1;
    wf->wBitsPerSample = 8;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "PCMU") == 0) {
    wf->wFormatTag = sh_audio->format = 0x7;
    wf->nAvgBytesPerSec = 8000;
    wf->nBlockAlign = 1;
    wf->wBitsPerSample = 8;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "PCMA") == 0) {
    wf->wFormatTag = sh_audio->format = 0x6;
    wf->nAvgBytesPerSec = 8000;
    wf->nBlockAlign = 1;
    wf->wBitsPerSample = 8;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "AMR") == 0) {
    wf->wFormatTag = sh_audio->format = mmioFOURCC('s','a','m','r');
  } else if (strcmp(subsession->codecName(), "AMR-WB") == 0) {
    wf->wFormatTag = sh_audio->format = mmioFOURCC('s','a','w','b');
  } else if (strcmp(subsession->codecName(), "GSM") == 0) {
    wf->wFormatTag = sh_audio->format = mmioFOURCC('a','g','s','m');
    wf->nAvgBytesPerSec = 1650;
    wf->nBlockAlign = 33;
    wf->wBitsPerSample = 16;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "QCELP") == 0) {
    wf->wFormatTag = sh_audio->format = mmioFOURCC('Q','c','l','p');
    wf->nAvgBytesPerSec = 1750;
    wf->nBlockAlign = 35;
    wf->wBitsPerSample = 16;
    wf->cbSize = 0;
  } else if (strcmp(subsession->codecName(), "MP4A-LATM") == 0) {
    wf->wFormatTag = sh_audio->format = mmioFOURCC('m','p','4','a');
    // For the codec to work correctly, it needs "AudioSpecificConfig"
    // data, which is parsed from the "StreamMuxConfig" string that
    // was present (hopefully) in the SDP description:
    unsigned codecdata_len;
    sh_audio->codecdata
      = parseStreamMuxConfigStr(subsession->fmtp_config(),
				codecdata_len);
    sh_audio->codecdata_len = codecdata_len;
    //faad doesn't understand LATM's data length field, so omit it
    ((MPEG4LATMAudioRTPSource*)subsession->rtpSource())->omitLATMDataLengthField();
  } else if (strcmp(subsession->codecName(), "MPEG4-GENERIC") == 0) {
    wf->wFormatTag = sh_audio->format = mmioFOURCC('m','p','4','a');
    // For the codec to work correctly, it needs "AudioSpecificConfig"
    // data, which was present (hopefully) in the SDP description:
    unsigned codecdata_len;
    sh_audio->codecdata
      = parseGeneralConfigStr(subsession->fmtp_config(),
			      codecdata_len);
    sh_audio->codecdata_len = codecdata_len;
  } else if (strcmp(subsession->codecName(), "X-QT") == 0 ||
	     strcmp(subsession->codecName(), "X-QUICKTIME") == 0) {
    // QuickTime generic RTP format, as described in
    // http://developer.apple.com/quicktime/icefloe/dispatch026.html

    // We can't initialize this stream until we've received the first packet
    // that has QuickTime "sdAtom" information in the header.  So, keep
    // reading packets until we get one:
    unsigned char* packetData; unsigned packetDataLen; float pts;
    QuickTimeGenericRTPSource* qtRTPSource
      = (QuickTimeGenericRTPSource*)(subsession->rtpSource());
    unsigned fourcc, numChannels;
    do {
      if (!awaitRTPPacket(demuxer, demuxer->audio,
			  packetData, packetDataLen, pts)) {
	return;
      }
    } while (!parseQTState_audio(qtRTPSource->qtState, fourcc, numChannels));

    wf->wFormatTag = sh_audio->format = fourcc;
    wf->nChannels = numChannels;

    uint8_t *pos = (uint8_t*)qtRTPSource->qtState.sdAtom + 52;
    uint8_t *endpos = (uint8_t*)qtRTPSource->qtState.sdAtom
                      + qtRTPSource->qtState.sdAtomSize;
    while (pos+8 < endpos) {
      unsigned atomLength = pos[0]<<24 | pos[1]<<16 | pos[2]<<8 | pos[3];
      if (atomLength == 0 || atomLength > endpos-pos) break;
      if (!memcmp(pos+4, "wave", 4) && fourcc==mmioFOURCC('Q','D','M','2') &&
          atomLength > 8 &&
          atomLength <= INT_MAX) {
        sh_audio->codecdata = (unsigned char*) malloc(atomLength-8);
        if (sh_audio->codecdata) {
          memcpy(sh_audio->codecdata, pos+8, atomLength-8);
          sh_audio->codecdata_len = atomLength-8;
        }
        break;
      }
      pos += atomLength;
    }
  } else {
    fprintf(stderr,
	    "Unknown MPlayer format code for MIME type \"audio/%s\"\n",
	    subsession->codecName());
  }
}

static void needVideoFrameRate(demuxer_t* demuxer,
			       MediaSubsession* subsession) {
  // For some codecs, MPlayer's decoding software can't (or refuses to :-)
  // figure out the frame rate by itself, so (unless the user specifies
  // it manually, using "-fps") we figure it out ourselves here, using the
  // presentation timestamps in successive packets,
  extern double force_fps; if (force_fps != 0.0) return; // user used "-fps"

  demux_stream_t* d_video = demuxer->video;
  sh_video_t* sh_video = (sh_video_t*)(d_video->sh);

  // If we already know the subsession's video frame rate, use it:
  int fps = (int)(subsession->videoFPS());
  if (fps != 0) {
    sh_video->fps = fps;
    sh_video->frametime = 1.0f/fps;
    return;
  }
  
  // Keep looking at incoming frames until we see two with different,
  // non-zero "pts" timestamps:
  unsigned char* packetData; unsigned packetDataLen;
  float lastPTS = 0.0, curPTS;
  unsigned const maxNumFramesToWaitFor = 300;
  int lastfps = 0;
  for (unsigned i = 0; i < maxNumFramesToWaitFor; ++i) {
    if (!awaitRTPPacket(demuxer, d_video, packetData, packetDataLen, curPTS)) {
      break;
    }

    if (curPTS != lastPTS && lastPTS != 0.0) {
      // Use the difference between these two "pts"s to guess the frame rate.
      // (should really check that there were no missing frames inbetween)#####
      // Guess the frame rate as an integer.  If it's not, use "-fps" instead.
      fps = (int)(1/fabs(curPTS-lastPTS) + 0.5); // rounding
        if (fps == lastfps) {
      fprintf(stderr, "demux_rtp: Guessed the video frame rate as %d frames-per-second.\n\t(If this is wrong, use the \"-fps <frame-rate>\" option instead.)\n", fps);
      sh_video->fps = fps;
      sh_video->frametime=1.0f/fps;
      return;
        }
      if (fps>lastfps) lastfps = fps;
    }
    lastPTS = curPTS;
  }
  fprintf(stderr, "demux_rtp: Failed to guess the video frame rate\n");
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
