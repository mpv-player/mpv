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
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <audio/audiolib.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"

#define NAS_FRAG_SIZE 4096
#define NAS_FRAG_COUNT 8
#define NAS_BUFFER_SIZE NAS_FRAG_SIZE * NAS_FRAG_COUNT

#define NAS_DEBUG 0

#if NAS_DEBUG == 1

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

#define DPRINTF(format, args...)	fprintf(stderr, format, ## args); \
					fflush(stderr)
#else
#define DPRINTF(format, args...)
#endif

static ao_info_t info = 
{
	"NAS audio output",
	"nas",
	"Tobias Diedrich",
	""
};

struct ao_nas_data {
	AuServer	*aud;
	AuFlowID	flow;
	AuDeviceID	dev;

	int flow_stopped;
	int flow_paused;

	void *client_buffer;
	int client_buffer_size;
	int client_buffer_used;
	int server_buffer_size;
	int server_buffer_used;
	pthread_mutex_t buffer_mutex;

	pthread_t event_thread;
	int stop_thread;
};

static struct ao_nas_data *nas_data;

LIBAO_EXTERN(nas)

static void nas_print_error(AuServer *aud, char *prefix, AuStatus as)
{
	char s[100];
	AuGetErrorText(aud, as, s, 100);
	fprintf(stderr, "ao_nas: %s: returned status %d (%s)\n", prefix, as, s);
	fflush(stderr);
}

static int nas_readBuffer(struct ao_nas_data *nas_data, int num)
{
	AuStatus as;

	pthread_mutex_lock(&nas_data->buffer_mutex);
	DPRINTF("ao_nas: nas_readBuffer(): num=%d client=%d/%d server=%d/%d\n",
			num,
			nas_data->client_buffer_used, nas_data->client_buffer_size,
			nas_data->server_buffer_used, nas_data->server_buffer_size);

	if (nas_data->client_buffer_used == 0) {
		DPRINTF("ao_nas: buffer is empty, nothing read.\n");
		pthread_mutex_unlock(&nas_data->buffer_mutex);
		return 0;
	}
	if (nas_data->client_buffer_used < num)
		num = nas_data->client_buffer_used;

	AuWriteElement(nas_data->aud, nas_data->flow, 0, num, nas_data->client_buffer, AuFalse, &as);
	if (as != AuSuccess) 
		nas_print_error(nas_data->aud, "nas_readBuffer(): AuWriteElement", as);
	else {
		nas_data->client_buffer_used -= num;
		nas_data->server_buffer_used += num;
		memmove(nas_data->client_buffer, nas_data->client_buffer + num, nas_data->client_buffer_used);
	}
	pthread_mutex_unlock(&nas_data->buffer_mutex);
	
	if (nas_data->flow_paused) {
		AuPauseFlow(nas_data->aud, nas_data->flow, &as);
		if (as != AuSuccess)
			nas_print_error(nas_data->aud, "nas_readBuffer(): AuPauseFlow", as);
	}

	return num;
}

static void nas_writeBuffer(struct ao_nas_data *nas_data, void *data, int len)
{
	pthread_mutex_lock(&nas_data->buffer_mutex);
	DPRINTF("ao_nas: nas_writeBuffer(): len=%d client=%d/%d server=%d/%d\n",
			len, nas_data->client_buffer_used, nas_data->client_buffer_size,
			nas_data->server_buffer_used, nas_data->server_buffer_size);

	memcpy(nas_data->client_buffer + nas_data->client_buffer_used, data, len);
	nas_data->client_buffer_used += len;

	pthread_mutex_unlock(&nas_data->buffer_mutex);
	if (nas_data->server_buffer_used < nas_data->server_buffer_size)
		nas_readBuffer(nas_data, nas_data->server_buffer_size - nas_data->server_buffer_used);
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
	AuEvent ev;
	AuBool result;

	do {
		nas_empty_event_queue(nas_data);
		usleep(10000);
	} while (!nas_data->stop_thread);
}

static AuBool nas_error_handler(AuServer* aud, AuErrorEvent* ev)
{
	char s[100];
	AuGetErrorText(aud, ev->error_code, s, 100);
	fprintf(stderr,"ao_nas: error [%s]\n"
		"error_code: %d\n"
		"request_code: %d\n"
		"minor_code: %d\n",
		s,
		ev->error_code,
		ev->request_code,
		ev->minor_code);
	fflush(stderr);

	return AuTrue;
}

static AuBool nas_event_handler(AuServer *aud, AuEvent *ev, AuEventHandlerRec *hnd)
{
	AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;
	AuStatus as;
	struct ao_nas_data *nas_data = hnd->data;

	switch (ev->type) {
	case AuEventTypeElementNotify:
		DPRINTF("ao_nas: event_handler(): kind %s state %s->%s reason %s numbytes %d\n",
			nas_elementnotify_kind(event->kind),
			nas_state(event->prev_state),
			nas_state(event->cur_state),
			nas_reason(event->reason),
			event->num_bytes);

		nas_data->server_buffer_used -= event->num_bytes;
		if (nas_data->server_buffer_used < 0)
			nas_data->server_buffer_used = 0;

		switch (event->kind) {
		case AuElementNotifyKindLowWater:
			nas_readBuffer(nas_data, event->num_bytes);
			break;
		case AuElementNotifyKindState:
			if (event->cur_state == AuStatePause) {
				switch (event->reason) {
				case AuReasonUnderrun:
					// buffer underrun -> refill buffer
					nas_data->server_buffer_used = 0;
					nas_readBuffer(nas_data, nas_data->server_buffer_size - nas_data->server_buffer_used);
					break;
				default:
					break;
				}
			}
			break;
		default: // silently ignored
			break;
		}
		break;
	default: 
		printf("ao_nas: nas_event_handler(): unhandled event type %d\n", ev->type);
		break;
	}
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

static unsigned char nas_aformat_to_auformat(unsigned int format)
{
	switch (format) {
	case	AFMT_U8:	return AuFormatLinearUnsigned8;
	case	AFMT_S8:	return AuFormatLinearSigned8;
	case	AFMT_U16_LE:	return AuFormatLinearUnsigned16LSB;
	case	AFMT_U16_BE:	return AuFormatLinearUnsigned16MSB;
#ifndef WORDS_BIGENDIAN
	case AFMT_AC3:
#endif
	case	AFMT_S16_LE:	return AuFormatLinearSigned16LSB;
#ifdef WORDS_BIGENDIAN
	case AFMT_AC3:
#endif
	case	AFMT_S16_BE:	return AuFormatLinearSigned16MSB;
	case	AFMT_MU_LAW:	return AuFormatULAW8;
	default: return 0;
	}
}

// to set/get/query special features/parameters
static int control(int cmd,int arg){
	return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags)
{
	AuElement elms[3];
	AuStatus as;
	unsigned char auformat = nas_aformat_to_auformat(format);
	int bytes_per_sample = channels * AuSizeofFormat(auformat);
	char *server;

	nas_data=malloc(sizeof(struct ao_nas_data));

	printf("ao2: %d Hz  %d chans  %s\n",rate,channels,
		audio_out_format_name(format));

	if (!auformat) {
		printf("ao_nas: init(): Unsupported format -> nosound\n");
		return 0;
	}

	nas_data->client_buffer_size = NAS_BUFFER_SIZE;
	nas_data->client_buffer = malloc(nas_data->client_buffer_size);
	nas_data->server_buffer_size = NAS_BUFFER_SIZE;

	ao_data.samplerate = rate;
	ao_data.channels = channels;
	ao_data.buffersize = NAS_BUFFER_SIZE * 2;
	ao_data.outburst = NAS_FRAG_SIZE;
	ao_data.bps = rate * bytes_per_sample;

	if (!bytes_per_sample) {
		printf("ao_nas: init(): Zero bytes per sample -> nosound\n");
		return 0;
	}

	if (!(server = getenv("AUDIOSERVER")))
		server = getenv("DISPLAY");

	if (!server) // default to tcp/localhost:8000
		server = "tcp/localhost:8000";

	printf("ao_nas: init(): Using audioserver %s\n", server);

	nas_data->aud = AuOpenServer(server, 0, NULL, 0, NULL, NULL);
	if (!nas_data->aud){ 
		printf("ao_nas: init(): Can't open nas audio server -> nosound\n");
		return 0;
	}

	nas_data->dev = nas_find_device(nas_data->aud, channels);
	if ((nas_data->dev == AuNone) || (!(nas_data->flow = AuCreateFlow(nas_data->aud, NULL)))) {
		printf("ao_nas: init(): Can't find a device serving that many channels -> nosound\n");
		AuCloseServer(nas_data->aud);
		nas_data->aud = 0;
		return 0;
	}

	AuMakeElementImportClient(elms, rate, auformat, channels, AuTrue,
				NAS_BUFFER_SIZE / bytes_per_sample,
				(NAS_BUFFER_SIZE - NAS_FRAG_SIZE) / bytes_per_sample,
				0, NULL);
	AuMakeElementExportDevice(elms+1, 0, nas_data->dev, rate,
				AuUnlimitedSamples, 0, NULL);
	AuSetElements(nas_data->aud, nas_data->flow, AuTrue, 2, elms, &as);
	if (as != AuSuccess)
		nas_print_error(nas_data->aud, "init(): AuSetElements", as);
	AuRegisterEventHandler(nas_data->aud, AuEventHandlerIDMask |
				AuEventHandlerTypeMask,
				AuEventTypeElementNotify, nas_data->flow,
				nas_event_handler, (AuPointer) nas_data);
	AuSetErrorHandler(nas_data->aud, nas_error_handler);
	nas_data->flow_stopped=1;

	pthread_mutex_init(&nas_data->buffer_mutex, NULL);
	pthread_create(&nas_data->event_thread, NULL, &nas_event_thread_start, nas_data);

	return 1;
}

// close audio device
static void uninit(){
	AuStatus as;

	nas_data->stop_thread = 1;
	pthread_join(nas_data->event_thread, NULL);
	if (!nas_data->flow_stopped) {
		AuStopFlow(nas_data->aud, nas_data->flow, &as);
		if (as != AuSuccess)
			nas_print_error(nas_data->aud, "uninit(): AuStopFlow", as);
	}
	AuCloseServer(nas_data->aud);
	nas_data->aud = 0;
	free(nas_data->client_buffer);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(){
	AuStatus as;

	pthread_mutex_lock(&nas_data->buffer_mutex);
	nas_data->client_buffer_used = 0;
	if (!nas_data->flow_stopped) {
		AuStopFlow(nas_data->aud, nas_data->flow, &as);
		if (as != AuSuccess)
			nas_print_error(nas_data->aud, "reset(): AuStopFlow", as);
		nas_data->flow_stopped = 1;
	}
	nas_data->server_buffer_used = 0;
	pthread_mutex_unlock(&nas_data->buffer_mutex);
}

// stop playing, keep buffers (for pause)
static void audio_pause()
{
	AuStatus as;

	DPRINTF("ao_nas: audio_pause()\n");

	nas_data->flow_paused = 1;
}

// resume playing, after audio_pause()
static void audio_resume()
{
	AuStatus as;
	AuEvent ev;

	DPRINTF("ao_nas: audio_resume()\n");

	nas_data->flow_stopped = 0;
	nas_data->flow_paused = 0;
	AuStartFlow(nas_data->aud, nas_data->flow, &as);
	if (as != AuSuccess)
		nas_print_error(nas_data->aud, "play(): AuStartFlow", as);
}


// return: how many bytes can be played without blocking
static int get_space()
{
	int result;
	
	DPRINTF("ao_nas: get_space()\n");

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

	DPRINTF("ao_nas: play()\n");

	if (nas_data->flow_stopped) {
		AuEvent ev;

		AuStartFlow(nas_data->aud, nas_data->flow, &as);
		if (as != AuSuccess)
			nas_print_error(nas_data->aud, "play(): AuStartFlow", as);
		nas_data->flow_stopped = 0;
		while (!nas_empty_event_queue(nas_data)); // wait for first buffer underrun event
	}

	pthread_mutex_lock(&nas_data->buffer_mutex);
	maxbursts = (nas_data->client_buffer_size -
		     nas_data->client_buffer_used) / ao_data.outburst;
	playbursts = len / ao_data.outburst;
	writelen = (playbursts > maxbursts ? maxbursts : playbursts) *
		   ao_data.outburst;
	pthread_mutex_unlock(&nas_data->buffer_mutex);

	nas_writeBuffer(nas_data, data, writelen);
	return writelen;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay()
{
	float result;
	
	DPRINTF("ao_nas: get_delay()\n");

	pthread_mutex_lock(&nas_data->buffer_mutex);
	result = ((float)(nas_data->client_buffer_used +
			  nas_data->server_buffer_used)) /
		 (float)ao_data.bps;
	pthread_mutex_unlock(&nas_data->buffer_mutex);

	return result;
}
