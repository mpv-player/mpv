#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"

#include "codec-cfg.h"
#include "libmpdemux/stheader.h"

#include "dec_audio.h"
#include "ad.h"
#include "libaf/af_format.h"

#include "libaf/af.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef CONFIG_DYNAMIC_PLUGINS
#include <dlfcn.h>
#endif

#ifdef CONFIG_FAKE_MONO
int fakemono = 0;
#endif

/* used for ac3surround decoder - set using -channels option */
int audio_output_channels = 2;
af_cfg_t af_cfg = { 1, NULL };	// Configuration for audio filters

void afm_help(void)
{
    int i;
    mp_msg(MSGT_DECAUDIO, MSGL_INFO, MSGTR_AvailableAudioFm);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_DRIVERS\n");
    mp_msg(MSGT_DECAUDIO, MSGL_INFO, "    afm:    info:  (comment)\n");
    for (i = 0; mpcodecs_ad_drivers[i] != NULL; i++)
	if (mpcodecs_ad_drivers[i]->info->comment
	    && mpcodecs_ad_drivers[i]->info->comment[0])
	    mp_msg(MSGT_DECAUDIO, MSGL_INFO, "%9s  %s (%s)\n",
		   mpcodecs_ad_drivers[i]->info->short_name,
		   mpcodecs_ad_drivers[i]->info->name,
		   mpcodecs_ad_drivers[i]->info->comment);
	else
	    mp_msg(MSGT_DECAUDIO, MSGL_INFO, "%9s  %s\n",
		   mpcodecs_ad_drivers[i]->info->short_name,
		   mpcodecs_ad_drivers[i]->info->name);
}

static int init_audio_codec(sh_audio_t *sh_audio)
{
    if ((af_cfg.force & AF_INIT_FORMAT_MASK) == AF_INIT_FLOAT) {
	int fmt = AF_FORMAT_FLOAT_NE;
	if (sh_audio->ad_driver->control(sh_audio, ADCTRL_QUERY_FORMAT,
					 &fmt) == CONTROL_TRUE) {
	    sh_audio->sample_format = fmt;
	    sh_audio->samplesize = 4;
	}
    }
    if (!sh_audio->ad_driver->preinit(sh_audio)) {
	mp_msg(MSGT_DECAUDIO, MSGL_ERR, MSGTR_ADecoderPreinitFailed);
	return 0;
    }

    /* allocate audio in buffer: */
    if (sh_audio->audio_in_minsize > 0) {
	sh_audio->a_in_buffer_size = sh_audio->audio_in_minsize;
	mp_msg(MSGT_DECAUDIO, MSGL_V, MSGTR_AllocatingBytesForInputBuffer,
	       sh_audio->a_in_buffer_size);
	sh_audio->a_in_buffer = av_mallocz(sh_audio->a_in_buffer_size);
	sh_audio->a_in_buffer_len = 0;
    }

    sh_audio->a_buffer_size = sh_audio->audio_out_minsize + MAX_OUTBURST;

    mp_msg(MSGT_DECAUDIO, MSGL_V, MSGTR_AllocatingBytesForOutputBuffer,
	   sh_audio->audio_out_minsize, MAX_OUTBURST, sh_audio->a_buffer_size);

    sh_audio->a_buffer = av_mallocz(sh_audio->a_buffer_size);
    if (!sh_audio->a_buffer) {
	mp_msg(MSGT_DECAUDIO, MSGL_ERR, MSGTR_CantAllocAudioBuf);
	return 0;
    }
    sh_audio->a_buffer_len = 0;

    if (!sh_audio->ad_driver->init(sh_audio)) {
	mp_msg(MSGT_DECAUDIO, MSGL_WARN, MSGTR_ADecoderInitFailed);
	uninit_audio(sh_audio);	// free buffers
	return 0;
    }

    sh_audio->initialized = 1;

    if (!sh_audio->channels || !sh_audio->samplerate) {
	mp_msg(MSGT_DECAUDIO, MSGL_WARN, MSGTR_UnknownAudio);
	uninit_audio(sh_audio);	// free buffers
	return 0;
    }

    if (!sh_audio->o_bps)
	sh_audio->o_bps = sh_audio->channels * sh_audio->samplerate
	                  * sh_audio->samplesize;

    mp_msg(MSGT_DECAUDIO, MSGL_INFO,
	   "AUDIO: %d Hz, %d ch, %s, %3.1f kbit/%3.2f%% (ratio: %d->%d)\n",
	   sh_audio->samplerate, sh_audio->channels,
	   af_fmt2str_short(sh_audio->sample_format),
	   sh_audio->i_bps * 8 * 0.001,
	   ((float) sh_audio->i_bps / sh_audio->o_bps) * 100.0,
	   sh_audio->i_bps, sh_audio->o_bps);
    mp_msg(MSGT_IDENTIFY, MSGL_INFO,
	   "ID_AUDIO_BITRATE=%d\nID_AUDIO_RATE=%d\n" "ID_AUDIO_NCH=%d\n",
	   sh_audio->i_bps * 8, sh_audio->samplerate, sh_audio->channels);

    sh_audio->a_out_buffer_size = 0;
    sh_audio->a_out_buffer = NULL;
    sh_audio->a_out_buffer_len = 0;

    return 1;
}

static int init_audio(sh_audio_t *sh_audio, char *codecname, char *afm,
		      int status, stringset_t *selected)
{
    unsigned int orig_fourcc = sh_audio->wf ? sh_audio->wf->wFormatTag : 0;
    int force = 0;
    if (codecname && codecname[0] == '+') {
	codecname = &codecname[1];
	force = 1;
    }
    sh_audio->codec = NULL;
    while (1) {
	ad_functions_t *mpadec;
	int i;
	sh_audio->ad_driver = 0;
	// restore original fourcc:
	if (sh_audio->wf)
	    sh_audio->wf->wFormatTag = i = orig_fourcc;
	if (!(sh_audio->codec = find_audio_codec(sh_audio->format,
						 sh_audio->wf ? (&i) : NULL,
						 sh_audio->codec, force)))
	    break;
	if (sh_audio->wf)
	    sh_audio->wf->wFormatTag = i;
	// ok we found one codec
	if (stringset_test(selected, sh_audio->codec->name))
	    continue;	// already tried & failed
	if (codecname && strcmp(sh_audio->codec->name, codecname))
	    continue;	// -ac
	if (afm && strcmp(sh_audio->codec->drv, afm))
	    continue;	// afm doesn't match
	if (!force && sh_audio->codec->status < status)
	    continue;	// too unstable
	stringset_add(selected, sh_audio->codec->name);	// tagging it
	// ok, it matches all rules, let's find the driver!
	for (i = 0; mpcodecs_ad_drivers[i] != NULL; i++)
	    if (!strcmp(mpcodecs_ad_drivers[i]->info->short_name,
		 sh_audio->codec->drv))
		break;
	mpadec = mpcodecs_ad_drivers[i];
#ifdef CONFIG_DYNAMIC_PLUGINS
	if (!mpadec) {
	    /* try to open shared decoder plugin */
	    int buf_len;
	    char *buf;
	    ad_functions_t *funcs_sym;
	    ad_info_t *info_sym;

	    buf_len =
		strlen(MPLAYER_LIBDIR) + strlen(sh_audio->codec->drv) + 16;
	    buf = malloc(buf_len);
	    if (!buf)
		break;
	    snprintf(buf, buf_len, "%s/mplayer/ad_%s.so", MPLAYER_LIBDIR,
		     sh_audio->codec->drv);
	    mp_msg(MSGT_DECAUDIO, MSGL_DBG2,
		   "Trying to open external plugin: %s\n", buf);
	    sh_audio->dec_handle = dlopen(buf, RTLD_LAZY);
	    if (!sh_audio->dec_handle)
		break;
	    snprintf(buf, buf_len, "mpcodecs_ad_%s", sh_audio->codec->drv);
	    funcs_sym = dlsym(sh_audio->dec_handle, buf);
	    if (!funcs_sym || !funcs_sym->info || !funcs_sym->preinit
		|| !funcs_sym->init || !funcs_sym->uninit
		|| !funcs_sym->control || !funcs_sym->decode_audio)
		break;
	    info_sym = funcs_sym->info;
	    if (strcmp(info_sym->short_name, sh_audio->codec->drv))
		break;
	    free(buf);
	    mpadec = funcs_sym;
	    mp_msg(MSGT_DECAUDIO, MSGL_V,
		   "Using external decoder plugin (%s/mplayer/ad_%s.so)!\n",
		   MPLAYER_LIBDIR, sh_audio->codec->drv);
	}
#endif
	if (!mpadec) {		// driver not available (==compiled in)
	    mp_msg(MSGT_DECAUDIO, MSGL_ERR,
		   MSGTR_AudioCodecFamilyNotAvailableStr,
		   sh_audio->codec->name, sh_audio->codec->drv);
	    continue;
	}
	// it's available, let's try to init!
	// init()
	mp_msg(MSGT_DECAUDIO, MSGL_INFO, MSGTR_OpeningAudioDecoder,
	       mpadec->info->short_name, mpadec->info->name);
	sh_audio->ad_driver = mpadec;
	if (!init_audio_codec(sh_audio)) {
	    mp_msg(MSGT_DECAUDIO, MSGL_INFO, MSGTR_ADecoderInitFailed);
	    continue;		// try next...
	}
	// Yeah! We got it!
	return 1;
    }
    return 0;
}

int init_best_audio_codec(sh_audio_t *sh_audio, char **audio_codec_list,
			  char **audio_fm_list)
{
    stringset_t selected;
    char *ac_l_default[2] = { "", (char *) NULL };
    // hack:
    if (!audio_codec_list)
	audio_codec_list = ac_l_default;
    // Go through the codec.conf and find the best codec...
    sh_audio->initialized = 0;
    stringset_init(&selected);
    while (!sh_audio->initialized && *audio_codec_list) {
	char *audio_codec = *(audio_codec_list++);
	if (audio_codec[0]) {
	    if (audio_codec[0] == '-') {
		// disable this codec:
		stringset_add(&selected, audio_codec + 1);
	    } else {
		// forced codec by name:
		mp_msg(MSGT_DECAUDIO, MSGL_INFO, MSGTR_ForcedAudioCodec,
		       audio_codec);
		init_audio(sh_audio, audio_codec, NULL, -1, &selected);
	    }
	} else {
	    int status;
	    // try in stability order: UNTESTED, WORKING, BUGGY.
	    // never try CRASHING.
	    if (audio_fm_list) {
		char **fmlist = audio_fm_list;
		// try first the preferred codec families:
		while (!sh_audio->initialized && *fmlist) {
		    char *audio_fm = *(fmlist++);
		    mp_msg(MSGT_DECAUDIO, MSGL_INFO, MSGTR_TryForceAudioFmtStr,
			   audio_fm);
		    for (status = CODECS_STATUS__MAX;
			 status >= CODECS_STATUS__MIN; --status)
			if (init_audio(sh_audio, NULL, audio_fm, status, &selected))
			    break;
		}
	    }
	    if (!sh_audio->initialized)
		for (status = CODECS_STATUS__MAX; status >= CODECS_STATUS__MIN;
		     --status)
		    if (init_audio(sh_audio, NULL, NULL, status, &selected))
			break;
	}
    }
    stringset_free(&selected);

    if (!sh_audio->initialized) {
	mp_msg(MSGT_DECAUDIO, MSGL_ERR, MSGTR_CantFindAudioCodec,
	       sh_audio->format);
	mp_msg(MSGT_DECAUDIO, MSGL_HINT, MSGTR_RTFMCodecs);
	return 0;   // failed
    }

    mp_msg(MSGT_DECAUDIO, MSGL_INFO, MSGTR_SelectedAudioCodec,
	   sh_audio->codec->name, sh_audio->codec->drv, sh_audio->codec->info);
    return 1;   // success
}

void uninit_audio(sh_audio_t *sh_audio)
{
    if (sh_audio->afilter) {
	mp_msg(MSGT_DECAUDIO, MSGL_V, "Uninit audio filters...\n");
	af_uninit(sh_audio->afilter);
	free(sh_audio->afilter);
	sh_audio->afilter = NULL;
    }
    if (sh_audio->initialized) {
	mp_msg(MSGT_DECAUDIO, MSGL_V, MSGTR_UninitAudioStr,
	       sh_audio->codec->drv);
	sh_audio->ad_driver->uninit(sh_audio);
#ifdef CONFIG_DYNAMIC_PLUGINS
	if (sh_audio->dec_handle)
	    dlclose(sh_audio->dec_handle);
#endif
	sh_audio->initialized = 0;
    }
    free(sh_audio->a_out_buffer);
    sh_audio->a_out_buffer = NULL;
    sh_audio->a_out_buffer_size = 0;
    av_freep(&sh_audio->a_buffer);
    av_freep(&sh_audio->a_in_buffer);
}


int init_audio_filters(sh_audio_t *sh_audio, int in_samplerate,
		       int *out_samplerate, int *out_channels, int *out_format)
{
    af_stream_t *afs = sh_audio->afilter;
    if (!afs) {
	afs = malloc(sizeof(af_stream_t));
	memset(afs, 0, sizeof(af_stream_t));
    }
    // input format: same as codec's output format:
    afs->input.rate   = in_samplerate;
    afs->input.nch    = sh_audio->channels;
    afs->input.format = sh_audio->sample_format;
    af_fix_parameters(&(afs->input));

    // output format: same as ao driver's input format (if missing, fallback to input)
    afs->output.rate   = *out_samplerate;
    afs->output.nch    = *out_channels;
    afs->output.format = *out_format;
    af_fix_parameters(&(afs->output));

    // filter config:
    memcpy(&afs->cfg, &af_cfg, sizeof(af_cfg_t));

    mp_msg(MSGT_DECAUDIO, MSGL_V, MSGTR_BuildingAudioFilterChain,
	   afs->input.rate, afs->input.nch,
	   af_fmt2str_short(afs->input.format), afs->output.rate,
	   afs->output.nch, af_fmt2str_short(afs->output.format));

    // let's autoprobe it!
    if (0 != af_init(afs)) {
	sh_audio->afilter = NULL;
	free(afs);
	return 0;   // failed :(
    }

    *out_samplerate = afs->output.rate;
    *out_channels = afs->output.nch;
    *out_format = afs->output.format;

    sh_audio->a_out_buffer_len = 0;

    // ok!
    sh_audio->afilter = (void *) afs;
    return 1;
}

static int filter_n_bytes(sh_audio_t *sh, int len)
{
    int error = 0;
    // Filter
    af_data_t filter_input = {
	.audio = sh->a_buffer,
	.rate = sh->samplerate,
	.nch = sh->channels,
	.format = sh->sample_format
    };
    af_data_t *filter_output;

    assert(len-1 + sh->audio_out_minsize <= sh->a_buffer_size);

    // Decode more bytes if needed
    while (sh->a_buffer_len < len) {
	unsigned char *buf = sh->a_buffer + sh->a_buffer_len;
	int minlen = len - sh->a_buffer_len;
	int maxlen = sh->a_buffer_size - sh->a_buffer_len;
	int ret = sh->ad_driver->decode_audio(sh, buf, minlen, maxlen);
	if (ret <= 0) {
	    error = -1;
	    len = sh->a_buffer_len;
	    break;
	}
	sh->a_buffer_len += ret;
    }

    filter_input.len = len;
    af_fix_parameters(&filter_input);
    filter_output = af_play(sh->afilter, &filter_input);
    if (!filter_output)
	return -1;
    if (sh->a_out_buffer_size < sh->a_out_buffer_len + filter_output->len) {
	int newlen = sh->a_out_buffer_len + filter_output->len;
	mp_msg(MSGT_DECAUDIO, MSGL_V, "Increasing filtered audio buffer size "
	       "from %d to %d\n", sh->a_out_buffer_size, newlen);
	sh->a_out_buffer = realloc(sh->a_out_buffer, newlen);
	sh->a_out_buffer_size = newlen;
    }
    memcpy(sh->a_out_buffer + sh->a_out_buffer_len, filter_output->audio,
	   filter_output->len);
    sh->a_out_buffer_len += filter_output->len;

    // remove processed data from decoder buffer:
    sh->a_buffer_len -= len;
    memmove(sh->a_buffer, sh->a_buffer + len, sh->a_buffer_len);

    return error;
}

/* Try to get at least minlen decoded+filtered bytes in sh_audio->a_out_buffer
 * (total length including possible existing data).
 * Return 0 on success, -1 on error/EOF (not distinguished).
 * In the former case sh_audio->a_out_buffer_len is always >= minlen
 * on return. In case of EOF/error it might or might not be.
 * Can reallocate sh_audio->a_out_buffer if needed to fit all filter output. */
int decode_audio(sh_audio_t *sh_audio, int minlen)
{
    // Indicates that a filter seems to be buffering large amounts of data
    int huge_filter_buffer = 0;
    // Decoded audio must be cut at boundaries of this many bytes
    int unitsize = sh_audio->channels * sh_audio->samplesize * 16;

    /* Filter output size will be about filter_multiplier times input size.
     * If some filter buffers audio in big blocks this might only hold
     * as average over time. */
    double filter_multiplier = af_calc_filter_multiplier(sh_audio->afilter);

    /* If the decoder set audio_out_minsize then it can do the equivalent of
     * "while (output_len < target_len) output_len += audio_out_minsize;",
     * so we must guarantee there is at least audio_out_minsize-1 bytes
     * more space in the output buffer than the minimum length we try to
     * decode. */
    int max_decode_len = sh_audio->a_buffer_size - sh_audio->audio_out_minsize;
    max_decode_len -= max_decode_len % unitsize;

    while (sh_audio->a_out_buffer_len < minlen) {
	int declen = (minlen - sh_audio->a_out_buffer_len) / filter_multiplier
	    + (unitsize << 5); // some extra for possible filter buffering
	if (huge_filter_buffer)
	/* Some filter must be doing significant buffering if the estimated
	 * input length didn't produce enough output from filters.
	 * Feed the filters 2k bytes at a time until we have enough output.
	 * Very small amounts could make filtering inefficient while large
	 * amounts can make MPlayer demux the file unnecessarily far ahead
	 * to get audio data and buffer video frames in memory while doing
	 * so. However the performance impact of either is probably not too
	 * significant as long as the value is not completely insane. */
	    declen = 2000;
	declen -= declen % unitsize;
	if (declen > max_decode_len)
	    declen = max_decode_len;
	else
	    /* if this iteration does not fill buffer, we must have lots
	     * of buffering in filters */
	    huge_filter_buffer = 1;
	if (filter_n_bytes(sh_audio, declen) < 0)
	    return -1;
    }
    return 0;
}

void resync_audio_stream(sh_audio_t *sh_audio)
{
    sh_audio->a_in_buffer_len = 0;	// clear audio input buffer
    if (!sh_audio->initialized)
	return;
    sh_audio->ad_driver->control(sh_audio, ADCTRL_RESYNC_STREAM, NULL);
}

void skip_audio_frame(sh_audio_t *sh_audio)
{
    if (!sh_audio->initialized)
	return;
    if (sh_audio->ad_driver->control(sh_audio, ADCTRL_SKIP_FRAME, NULL) ==
	CONTROL_TRUE)
	return;
    // default skip code:
    ds_fill_buffer(sh_audio->ds);	// skip block
}
