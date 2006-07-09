/*
 * NAS output plugin for mplayer
 *
 * based on the libaudiooss parts rewritten by me, which were
 * originally based on the NAS output plugin for xmms.
 *
 * xmms plugin by Willem Monsuwe
 * adapted for libaudiooss by Jon Trulson
 * further modified by Erik Inge Bolsø
 * largely rewritten and used for this
 * plugin by Tobias Diedrich
 *
 * Theory of operation:
 *
 * The NAS consists of two parts, a server daemon and a client.
 * We setup the server to use a buffer of size bytes_per_second
 * with a low watermark of buffer_size - NAS_FRAG_SIZE.
 * Upon starting the flow the server will generate a buffer underrun
 * event and the event handler will fill the buffer for the first time.
 * Now the server will generate a lowwater event when the server buffer
 * falls below the low watermark value. The event handler gets called
 * again and refills the buffer by the number of bytes requested by the
 * server (usually a multiple of 4096). To prevent stuttering on
 * startup (start of playing, seeks, unpausing) the client buffer should
 * be bigger than the server buffer. (For debugging we also do some
 * accounting of what we think how much of the server buffer is filled)
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <audio/audiolib.h>

#include "config.h"
#include "mp_msg.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"

#define NAS_FRAG_SIZE 4096

static char *nas_event_types[] = {
	"Undefined",
	"Undefined",
	"ElementNotify",
	"GrabNotify",
	"MonitorNotify",
	"BucketNotify",
	"DeviceNotify"
};

static char *nas_elementnotify_kinds[] = {
	"LowWater",
	"HighWater",
	"State",
	"Unknown"
};

static char *nas_states[] = {
	"Stop",
	"Start",
	"Pause",
	"Any"
};

static char *nas_reasons[] = {
	"User",
	"Underrun",
	"Overrun",
	"EOF",
	"Watermark",
	"Hardware",
	"Any"
};

static char* nas_reason(unsigned int reason)
{
	if (reason > 6) reason = 6;
	return nas_reasons[reason];
}

static char* nas_elementnotify_kind(unsigned int kind)
{
	if (kind > 2) kind = 3;
	return nas_elementnotify_kinds[kind];
}

static char* nas_event_type(unsigned int type) {
	if (type > 6) type = 0;
	return nas_event_types[type];
}

static char* nas_state(unsigned int state) {
	if (state>3) state = 3;
	return nas_states[state];
}

static ao_info_t info = 
{
	"NAS audio output",
	"nas",
	"Tobias Diedrich <ranma+mplayer@tdiedrich.de>",
	""
};

struct ao_nas_data {
	AuServer	*aud;
	AuFlowID	flow;
	AuDeviceID	dev;
	AuFixedPoint	gain;

	unsigned int state;
	int expect_underrun;

	void *client_buffer;
	void *server_buffer;
	unsigned int client_buffer_size;
	unsigned int client_buffer_used;
	unsigned int server_buffer_size;
	unsigned int server_buffer_used;
	pthread_mutex_t buffer_mutex;

	pthread_t event_thread;
	int stop_thread;
};

static struct ao_nas_data *nas_data;

LIBAO_EXTERN(nas)

static void nas_print_error(AuServer *aud, const char *prefix, AuStatus as)
{
	char s[100];
	AuGetErrorText(aud, as, s, 100);
	mp_msg(MSGT_AO, MSGL_ERR, "ao_nas: %s: returned status %d (%s)\n", prefix, as, s);
}

static int nas_readBuffer(struct ao_nas_data *nas_data, int num)
{
	AuStatus as;

	pthread_mutex_lock(&nas_data->buffer_mutex);
	mp_msg(MSGT_AO, MSGL_DBG2, "ao_nas: nas_readBuffer(): num=%d client=%d/%d server=%d/%d\n",
			num,
			nas_data->client_buffer_used, nas_data->client_buffer_size,
			nas_data->server_buffer_used, nas_data->server_buffer_size);

	if (nas_data->client_buffer_used == 0) {
		mp_msg(MSGT_AO, MSGL_DBG2, "ao_nas: buffer is empty, nothing read.\n");
		pthread_mutex_unlock(&nas_data->buffer_mutex);
		return 0;
	}
	if (num > nas_data->client_buffer_used)
		num = nas_data->client_buffer_used;

	/*
	 * It is not appropriate to call AuWriteElement() here because the
	 * buffer is locked and delays writing to the network will cause
	 * other threads to block waiting for buffer_mutex.  Instead the
	 * data is copied to "server_buffer" and written to the network
	 * outside of the locked section of code.
	 *
	 * (Note: Rather than these two buffers, a single circular buffer
	 *  could eliminate the memcpy/memmove steps.)
	 */
	/* make sure we don't overflow the buffer */
	if (num > nas_data->server_buffer_size)
		num = nas_data->server_buffer_size;
	memcpy(nas_data->server_buffer, nas_data->client_buffer, num);

	nas_data->client_buffer_used -= num;
	nas_data->server_buffer_used += num;
	memmove(nas_data->client_buffer, nas_data->client_buffer + num, nas_data->client_buffer_used);
	pthread_mutex_unlock(&nas_data->buffer_mutex);

	/*
	 * Now write the new buffer to the network.
	 */
	AuWriteElement(nas_data->aud, nas_data->flow, 0, num, nas_data->server_buffer, AuFalse, &as);
	if (as != AuSuccess) 
		nas_print_error(nas_data->aud, "nas_readBuffer(): AuWriteElement", as);

	return num;
}

static int nas_writeBuffer(struct ao_nas_data *nas_data, void *data, int len)
{
	pthread_mutex_lock(&nas_data->buffer_mutex);
	mp_msg(MSGT_AO, MSGL_DBG2, "ao_nas: nas_writeBuffer(): len=%d client=%d/%d server=%d/%d\n",
			len, nas_data->client_buffer_used, nas_data->client_buffer_size,
			nas_data->server_buffer_used, nas_data->server_buffer_size);

	/* make sure we don't overflow the buffer */
	if (len > nas_data->client_buffer_size - nas_data->client_buffer_used)
		len = nas_data->client_buffer_size - nas_data->client_buffer_used;
	memcpy(nas_data->client_buffer + nas_data->client_buffer_used, data, len);
	nas_data->client_buffer_used += len;

	pthread_mutex_unlock(&nas_data->buffer_mutex);

	return len;
}

static int nas_empty_event_queue(struct ao_nas_data *nas_data)
{
	AuEvent ev;
	int result = 0;
	
	while (AuScanForTypedEvent(nas_data->aud, AuEventsQueuedAfterFlush,
				   AuTrue, AuEventTypeElementNotify, &ev)) {
		AuDispatchEvent(nas_data->aud, &ev);
		result = 1;
	}
	return result;
}

static void *nas_event_thread_start(void *data)
{
	struct ao_nas_data *nas_data = data;

	do {
		mp_msg(MSGT_AO, MSGL_DBG2,
		       "ao_nas: event thread heartbeat (state=%s)\n",
		       nas_state(nas_data->state));
		nas_empty_event_queue(nas_data);
		usleep(1000);
	} while (!nas_data->stop_thread);

	return NULL;
}

static AuBool nas_error_handler(AuServer* aud, AuErrorEvent* ev)
{
	char s[100];
	AuGetErrorText(aud, ev->error_code, s, 100);
	mp_msg(MSGT_AO, MSGL_ERR, "ao_nas: error [%s]\n"
		"error_code: %d\n"
		"request_code: %d\n"
		"minor_code: %d\n",
		s,
		ev->error_code,
		ev->request_code,
		ev->minor_code);

	return AuTrue;
}

static AuBool nas_event_handler(AuServer *aud, AuEvent *ev, AuEventHandlerRec *hnd)
{
	AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;
	struct ao_nas_data *nas_data = hnd->data;

	mp_msg(MSGT_AO, MSGL_DBG2, "ao_nas: event_handler(): type %s kind %s state %s->%s reason %s numbytes %d expect_underrun %d\n",
		nas_event_type(event->type),
		nas_elementnotify_kind(event->kind),
		nas_state(event->prev_state),
		nas_state(event->cur_state),
		nas_reason(event->reason),
		event->num_bytes,
		nas_data->expect_underrun);

	if (event->num_bytes > INT_MAX) {
		mp_msg(MSGT_AO, MSGL_ERR, "ao_nas: num_bytes > 2GB, server buggy?\n");
	}

	if (event->num_bytes > nas_data->server_buffer_used)
		event->num_bytes = nas_data->server_buffer_used;
	nas_data->server_buffer_used -= event->num_bytes;

	switch (event->reason) {
	case AuReasonWatermark:
		nas_readBuffer(nas_data, event->num_bytes);
		break;
	case AuReasonUnderrun:
		// buffer underrun -> refill buffer
		nas_data->server_buffer_used = 0;
		if (nas_data->expect_underrun) {
			nas_data->expect_underrun = 0;
		} else {
			static int hint = 1;
			mp_msg(MSGT_AO, MSGL_WARN,
			       "ao_nas: Buffer underrun.\n");
			if (hint) {
				hint = 0;
				mp_msg(MSGT_AO, MSGL_HINT,
				       "Possible reasons are:\n"
				       "1) Network congestion.\n"
				       "2) Your NAS server is too slow.\n"
				       "Try renicing your nasd to e.g. -15.\n");
			}
		}
		if (nas_readBuffer(nas_data,
		                   nas_data->server_buffer_size -
		                   nas_data->server_buffer_used) != 0) {
			event->cur_state = AuStateStart;
			break;
		}
		mp_msg(MSGT_AO, MSGL_DBG2,
			"ao_nas: Can't refill buffer, stopping flow.\n");
		AuStopFlow(nas_data->aud, nas_data->flow, NULL);
		break;
	default:
		break;
	}
	nas_data->state=event->cur_state;
	return AuTrue;
}

static AuDeviceID nas_find_device(AuServer *aud, int nch)
{
	int i;
	for (i = 0; i < AuServerNumDevices(aud); i++) {
		AuDeviceAttributes *dev = AuServerDevice(aud, i);
		if ((AuDeviceKind(dev) == AuComponentKindPhysicalOutput) &&
		     AuDeviceNumTracks(dev) == nch) {
			return AuDeviceIdentifier(dev);
		}
	}
	return AuNone;
}

static unsigned int nas_aformat_to_auformat(unsigned int *format)
{
	switch (*format) {
	case	AF_FORMAT_U8:
		return AuFormatLinearUnsigned8;
	case	AF_FORMAT_S8:
		return AuFormatLinearSigned8;
	case	AF_FORMAT_U16_LE:
		return AuFormatLinearUnsigned16LSB;
	case	AF_FORMAT_U16_BE:
		return AuFormatLinearUnsigned16MSB;
	case	AF_FORMAT_S16_LE:
		return AuFormatLinearSigned16LSB;
	case	AF_FORMAT_S16_BE:
		return AuFormatLinearSigned16MSB;
	case	AF_FORMAT_MU_LAW:
		return AuFormatULAW8;
	default:
		*format=AF_FORMAT_S16_NE;
		return nas_aformat_to_auformat(format);
	}
}

// to set/get/query special features/parameters
static int control(int cmd, void *arg)
{
	AuElementParameters aep;
	AuStatus as;
	int retval = CONTROL_UNKNOWN;

	ao_control_vol_t *vol = (ao_control_vol_t *)arg;

	switch (cmd) {
	case AOCONTROL_GET_VOLUME:

		vol->right = (float)nas_data->gain/AU_FIXED_POINT_SCALE*50;
		vol->left = vol->right;

		mp_msg(MSGT_AO, MSGL_DBG2, "ao_nas: AOCONTROL_GET_VOLUME: %08x\n", nas_data->gain);
		retval = CONTROL_OK;
		break;

	case AOCONTROL_SET_VOLUME:
		/*
		 * kn: we should have vol->left == vol->right but i don't
		 * know if something can change it outside of ao_nas
		 * so i take the mean of both values.
		 */
		nas_data->gain = AU_FIXED_POINT_SCALE*((vol->left+vol->right)/2)/50;
		mp_msg(MSGT_AO, MSGL_DBG2, "ao_nas: AOCONTROL_SET_VOLUME: %08x\n", nas_data->gain);

		aep.parameters[AuParmsMultiplyConstantConstant]=nas_data->gain;
		aep.flow = nas_data->flow;
		aep.element_num = 1;
		aep.num_parameters = AuParmsMultiplyConstant;

		AuSetElementParameters(nas_data->aud, 1, &aep, &as);
		if (as != AuSuccess) {
			nas_print_error(nas_data->aud,
			                "control(): AuSetElementParameters", as);
			retval = CONTROL_ERROR;
		} else retval = CONTROL_OK;
		break;
	};

	return retval;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags)
{
	AuElement elms[3];
	AuStatus as;
	unsigned char auformat = nas_aformat_to_auformat(&format);
	int bytes_per_sample = channels * AuSizeofFormat(auformat);
	int buffer_size;
	char *server;

	nas_data=malloc(sizeof(struct ao_nas_data));
	memset(nas_data, 0, sizeof(struct ao_nas_data));

	mp_msg(MSGT_AO, MSGL_V, "ao2: %d Hz  %d chans  %s\n",rate,channels,
		af_fmt2str_short(format));

	ao_data.format = format;
	ao_data.samplerate = rate;
	ao_data.channels = channels;
	ao_data.outburst = NAS_FRAG_SIZE;
	ao_data.bps = rate * bytes_per_sample;
	buffer_size = ao_data.bps; /* buffer 1 second */
	/*
	 * round up to multiple of NAS_FRAG_SIZE
	 * divide by 3 first because of 2:1 split
	 */
	buffer_size = (buffer_size/3 + NAS_FRAG_SIZE-1) & ~(NAS_FRAG_SIZE-1);
	ao_data.buffersize = buffer_size*3;

	nas_data->client_buffer_size = buffer_size*2;
	nas_data->client_buffer = malloc(nas_data->client_buffer_size);
	nas_data->server_buffer_size = buffer_size;
	nas_data->server_buffer = malloc(nas_data->server_buffer_size);

	if (!bytes_per_sample) {
		mp_msg(MSGT_AO, MSGL_ERR, "ao_nas: init(): Zero bytes per sample -> nosound\n");
		return 0;
	}

	if (!(server = getenv("AUDIOSERVER")) &&
	    !(server = getenv("DISPLAY"))) {
		mp_msg(MSGT_AO, MSGL_ERR, "ao_nas: init(): AUDIOSERVER environment variable not set -> nosound\n");
		return 0;
	}

	mp_msg(MSGT_AO, MSGL_V, "ao_nas: init(): Using audioserver %s\n", server);

	nas_data->aud = AuOpenServer(server, 0, NULL, 0, NULL, NULL);
	if (!nas_data->aud) { 
		mp_msg(MSGT_AO, MSGL_ERR, "ao_nas: init(): Can't open nas audio server -> nosound\n");
		return 0;
	}

	while (channels>1) {
		nas_data->dev = nas_find_device(nas_data->aud, channels);
		if (nas_data->dev != AuNone &&
		    ((nas_data->flow = AuCreateFlow(nas_data->aud, NULL)) != 0))
			break;
		channels--;
	}

	if (nas_data->flow == 0) {
		mp_msg(MSGT_AO, MSGL_ERR, "ao_nas: init(): Can't find a suitable output device -> nosound\n");
		AuCloseServer(nas_data->aud);
		nas_data->aud = 0;
		return 0;
	}

	AuMakeElementImportClient(elms, rate, auformat, channels, AuTrue,
				buffer_size / bytes_per_sample,
				(buffer_size - NAS_FRAG_SIZE) /
				bytes_per_sample, 0, NULL);
	nas_data->gain = AuFixedPointFromFraction(1, 1);
	AuMakeElementMultiplyConstant(elms+1, 0, nas_data->gain);
	AuMakeElementExportDevice(elms+2, 1, nas_data->dev, rate,
				AuUnlimitedSamples, 0, NULL);
	AuSetElements(nas_data->aud, nas_data->flow, AuTrue, sizeof(elms)/sizeof(*elms), elms, &as);
	if (as != AuSuccess) {
		nas_print_error(nas_data->aud, "init(): AuSetElements", as);
		AuCloseServer(nas_data->aud);
		nas_data->aud = 0;
		return 0;
	}
	AuRegisterEventHandler(nas_data->aud, AuEventHandlerIDMask |
				AuEventHandlerTypeMask,
				AuEventTypeElementNotify, nas_data->flow,
				nas_event_handler, (AuPointer) nas_data);
	AuSetErrorHandler(nas_data->aud, nas_error_handler);
	nas_data->state=AuStateStop;
	nas_data->expect_underrun=0;

	pthread_mutex_init(&nas_data->buffer_mutex, NULL);
	pthread_create(&nas_data->event_thread, NULL, &nas_event_thread_start, nas_data);

	return 1;
}

// close audio device
static void uninit(int immed){

	mp_msg(MSGT_AO, MSGL_DBG3, "ao_nas: uninit()\n");

	nas_data->expect_underrun = 1;
	if (!immed)
	while (nas_data->state != AuStateStop) usleep(1000);
	nas_data->stop_thread = 1;
	pthread_join(nas_data->event_thread, NULL);
	AuCloseServer(nas_data->aud);
	nas_data->aud = 0;
	free(nas_data->client_buffer);
	free(nas_data->server_buffer);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){
	AuStatus as;

	mp_msg(MSGT_AO, MSGL_DBG3, "ao_nas: reset()\n");

	pthread_mutex_lock(&nas_data->buffer_mutex);
	nas_data->client_buffer_used = 0;
	pthread_mutex_unlock(&nas_data->buffer_mutex);
	while (nas_data->state != AuStateStop) {
		AuStopFlow(nas_data->aud, nas_data->flow, &as);
		if (as != AuSuccess)
			nas_print_error(nas_data->aud, "reset(): AuStopFlow", as);
		usleep(1000);
	}
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
	AuStatus as;
	mp_msg(MSGT_AO, MSGL_DBG3, "ao_nas: audio_pause()\n");

	AuStopFlow(nas_data->aud, nas_data->flow, &as);
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
	AuStatus as;

	mp_msg(MSGT_AO, MSGL_DBG3, "ao_nas: audio_resume()\n");

	AuStartFlow(nas_data->aud, nas_data->flow, &as);
	if (as != AuSuccess)
		nas_print_error(nas_data->aud,
		                "play(): AuStartFlow", as);
}


// return: how many bytes can be played without blocking
static int get_space(void)
{
	int result;
	
	mp_msg(MSGT_AO, MSGL_DBG3, "ao_nas: get_space()\n");

	pthread_mutex_lock(&nas_data->buffer_mutex);
	result = nas_data->client_buffer_size - nas_data->client_buffer_used;
	pthread_mutex_unlock(&nas_data->buffer_mutex);

	return result;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags)
{
	int maxbursts, playbursts, writelen;
	AuStatus as;

	mp_msg(MSGT_AO, MSGL_DBG3,
	       "ao_nas: play(%p, %d, %d)\n",
	       data, len, flags);

	if (len == 0)
		return 0;

	pthread_mutex_lock(&nas_data->buffer_mutex);
	maxbursts = (nas_data->client_buffer_size -
		     nas_data->client_buffer_used) / ao_data.outburst;
	playbursts = len / ao_data.outburst;
	writelen = (playbursts > maxbursts ? maxbursts : playbursts) *
		   ao_data.outburst;
	pthread_mutex_unlock(&nas_data->buffer_mutex);

	writelen = nas_writeBuffer(nas_data, data, writelen);

	if (nas_data->state != AuStateStart &&
	    maxbursts == playbursts) {
		mp_msg(MSGT_AO, MSGL_DBG2, "ao_nas: play(): Starting flow.\n");
		nas_data->expect_underrun = 1;
		AuStartFlow(nas_data->aud, nas_data->flow, &as);
		if (as != AuSuccess)
			nas_print_error(nas_data->aud, "play(): AuStartFlow", as);
	}

	return writelen;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void)
{
	float result;
	
	mp_msg(MSGT_AO, MSGL_DBG3, "ao_nas: get_delay()\n");

	pthread_mutex_lock(&nas_data->buffer_mutex);
	result = ((float)(nas_data->client_buffer_used +
			  nas_data->server_buffer_used)) /
		 (float)ao_data.bps;
	pthread_mutex_unlock(&nas_data->buffer_mutex);

	return result;
}
