// SAMPLE audio decoder - you can use this file as template when creating new codec!

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "ad_internal.h"

static ad_info_t info =  {
	"Sample audio decoder",  // name of the driver
	"sample",    // driver name. should be the same as filename without ad_
	"A'rpi",     // writer/maintainer of _this_ file
	"",          // writer/maintainer/site of the _codec_
	""           // comments
};

LIBAD_EXTERN(sample)

#include "libsample/sample.h" // include your codec's .h files here

static int preinit(sh_audio_t *sh){
  // let's check if the driver is available, return 0 if not.
  // (you should do that if you use external lib(s) which is optional)
  ...
  
  // there are default values set for buffering, but you can override them:
  
  // minimum output buffer size (should be the uncompressed max. frame size)
  sh->audio_out_minsize=4*2*1024; // in this sample, we assume max 4 channels,
                                  // 2 bytes/sample and 1024 samples/frame
				  // Default: 8192
  
  // minimum input buffer size (set only if you need input buffering)
  // (should be the max compressed frame size)
  sh->audio_in_minsize=2048; // Default: 0 (no input buffer)
  
  // if you set audio_in_minsize non-zero, the buffer will be allocated
  // before the init() call by the core, and you can access it via
  // pointer: sh->audio_in_buffer
  // it will free'd after uninit(), so you don't have to use malloc/free here!

  // the next few parameters define the audio format (channels, sample type,
  // in/out bitrate etc.). it's OK to move these to init() if you can set
  // them only after some initialization:
  
  sh->samplesize=2;              // bytes (not bits!) per sample per channel
  sh->channels=2;                // number of channels
  sh->samplerate=44100;          // samplerate
  sh->sample_format=AF_FORMAT_S16_LE; // sample format, see libao2/afmt.h
  
  sh->i_bps=64000/8; // input data rate (compressed bytes per second)
  // Note: if you have VBR or unknown input rate, set it to some common or
  // average value, instead of zero. it's used to predict time delay of
  // buffered compressed bytes, so it must be more-or-less real!
  
//sh->o_bps=...     // output data rate (uncompressed bytes per second)
  // Note: you DON'T need to set o_bps in most cases, as it defaults to:
  //   sh->samplesize*sh->channels*sh->samplerate;

  // for constant rate compressed QuickTime (.mov files) codecs you MUST
  // set the compressed and uncompressed packet size (used by the demuxer):
  sh->ds->ss_mul = 34; // compressed packet size
  sh->ds->ss_div = 64; // samples per packet
  
  return 1; // return values: 1=OK 0=ERROR
}

static int init(sh_audio_t *sh_audio){
  // initialize the decoder, set tables etc...

  // you can store HANDLE or private struct pointer at sh->context
  // you can access WAVEFORMATEX header at sh->wf
  
  // set sample format/rate parameters if you didn't do it in preinit() yet.

  return 1; // return values: 1=OK 0=ERROR
}

static void uninit(sh_audio_t *sh){
  // uninit the decoder etc...
  // again: you don't have to free() a_in_buffer here! it's done by the core.
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen){

  // audio decoding. the most important thing :)
  // parameters you get:
  //  buf = pointer to the output buffer, you have to store uncompressed 
  //        samples there
  //  minlen = requested minimum size (in bytes!) of output. it's just a
  //        _recommendation_, you can decode more or less, it just tell you that
  //        the caller process needs 'minlen' bytes. if it gets less, it will
  //        call decode_audio() again.
  //  maxlen = maximum size (bytes) of output. you MUST NOT write more to the
  //        buffer, it's the upper-most limit!
  //        note: maxlen will be always greater or equal to sh->audio_out_minsize

  // now, let's decode...  
  
  // you can read the compressed stream using the demux stream functions:
  //  demux_read_data(sh->ds, buffer, length) - read 'length' bytes to 'buffer'
  //  ds_get_packet(sh->ds, &buffer) - set ptr buffer to next data packet
  // (both func return number of bytes or 0 for error)

  return len; // return value: number of _bytes_ written to output buffer,
              // or -1 for EOF (or uncorrectable error)
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...){
    // various optional functions you MAY implement:
    switch(cmd){
      case ADCTRL_RESYNC_STREAM:
        // it is called once after seeking, to resync.
	// Note: sh_audio->a_in_buffer_len=0; is done _before_ this call!
	...
	return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
        // it is called to skip (jump over) small amount (1/10 sec or 1 frame)
	// of audio data - used to sync audio to video after seeking
	// if you don't return CONTROL_TRUE, it will defaults to:
	//      ds_fill_buffer(sh_audio->ds);  // skip 1 demux packet
	...
	return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}
