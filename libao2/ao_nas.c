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

#define FRAG_SIZE 4096
#define FRAG_COUNT 8
#define BUFFER_SIZE FRAG_SIZE * FRAG_COUNT

#define NAS_DEBUG 0

#if NAS_DEBUG == 1
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

static	AuServer*	aud;
static	AuFlowID	flow;
static	AuDeviceID	dev;

static void *client_buffer;
static int client_buffer_size = BUFFER_SIZE;
static int client_buffer_used;
static int server_buffer_size = BUFFER_SIZE;
static int server_buffer_used;
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t event_thread;
static int stop_thread;

LIBAO_EXTERN(nas)

static void wait_for_event()
{
	AuEvent ev;

	AuNextEvent(aud, AuTrue, &ev);
	AuDispatchEvent(aud, &ev);
}

static int readBuffer(int num)
{
	pthread_mutex_lock(&buffer_mutex);
	DPRINTF("readBuffer(): num=%d client=%d/%d server=%d/%d\n",
			num,
			client_buffer_used, client_buffer_size,
			server_buffer_used, server_buffer_size);

	if (client_buffer_used == 0) {
		DPRINTF("buffer is empty, nothing read.\n");
		pthread_mutex_unlock(&buffer_mutex);
		return 0;
	}
	if (client_buffer_used < num)
		num = client_buffer_used;

	AuWriteElement(aud, flow, 0, num, client_buffer, AuFalse, NULL);
	client_buffer_used -= num;
	server_buffer_used += num;
	memmove(client_buffer, client_buffer + num, client_buffer_used);
	pthread_mutex_unlock(&buffer_mutex);

	return num;
}

static void writeBuffer(void *data, int len)
{
	pthread_mutex_lock(&buffer_mutex);
	DPRINTF("writeBuffer(): len=%d client=%d/%d server=%d/%d\n",
			len, client_buffer_used, client_buffer_size,
			server_buffer_used, server_buffer_size);

	memcpy(client_buffer + client_buffer_used, data, len);
	client_buffer_used += len;

	pthread_mutex_unlock(&buffer_mutex);
	if (server_buffer_used < server_buffer_size)
		readBuffer(server_buffer_size - server_buffer_used);
}


static void *event_thread_start(void *data)
{
	while (!stop_thread) {
		wait_for_event();
	}
}

static AuBool event_handler(AuServer *aud, AuEvent *ev, AuEventHandlerRec *hnd)
{
	switch (ev->type) {
	case AuEventTypeElementNotify: {
		AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;
		DPRINTF("event_handler(): kind %d state %d->%d reason %d numbytes %d\n",
				event->kind,
				event->prev_state,
				event->cur_state,
				event->reason,
				event->num_bytes);

		switch (event->kind) {
		case AuElementNotifyKindLowWater:
			server_buffer_used -= event->num_bytes;
			readBuffer(event->num_bytes);
			break;
		case AuElementNotifyKindState:
			if ((event->cur_state == AuStatePause) &&
				(event->reason != AuReasonUser)) {
				// buffer underrun -> refill buffer
				server_buffer_used = 0;
				readBuffer(server_buffer_size - server_buffer_used);
			}
		}
		}
	}
	return AuTrue;
}

static AuBool error_handler(AuServer* aud, AuErrorEvent* ev)
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

static AuDeviceID find_device(int nch)
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

static unsigned char aformat_to_auformat(unsigned int format)
{
	switch (format) {
	case	AFMT_U8:	return AuFormatLinearUnsigned8;
	case	AFMT_S8:	return AuFormatLinearSigned8;
	case	AFMT_U16_LE:	return AuFormatLinearUnsigned16LSB;
	case	AFMT_U16_BE:	return AuFormatLinearUnsigned16MSB;
	case	AFMT_S16_LE:	return AuFormatLinearSigned16LSB;
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
	unsigned char auformat = aformat_to_auformat(format);
	int bytes_per_sample;
	char *server;

	printf("ao2: %d Hz  %d chans  %s\n",rate,channels,
	audio_out_format_name(format));

	if (!auformat) {
		printf("Unsupported format -> nosound\n");
		return 0;
	}

	client_buffer = malloc(BUFFER_SIZE);
	bytes_per_sample = channels * AuSizeofFormat(auformat);

	ao_data.samplerate = rate;
	ao_data.channels = channels;
	ao_data.buffersize = BUFFER_SIZE * 2;
	ao_data.outburst = FRAG_SIZE;
	ao_data.bps = rate * bytes_per_sample;

	if (!bytes_per_sample) {
		printf("Zero bytes per sample -> nosound\n");
		return 0;
	}

	if (!(server = getenv("AUDIOSERVER")))
		server = getenv("DISPLAY");

	if (!server) // default to tcp/localhost:8000
		server = "tcp/localhost:8000";

	printf("Using audioserver %s\n", server);

	aud = AuOpenServer(server, 0, NULL, 0, NULL, NULL);
	if (!aud){ 
		printf("Can't open nas audio server -> nosound\n");
		return 0;
	}

	dev = find_device(channels);
	if ((dev == AuNone) || (!(flow = AuCreateFlow(aud, NULL)))) {
		printf("Can't find a device serving that many channels -> nosound\n");
		AuCloseServer(aud);
		aud = 0;
		return 0;
	}

	AuMakeElementImportClient(elms, rate, auformat, channels, AuTrue,
				BUFFER_SIZE / bytes_per_sample,
				(BUFFER_SIZE - FRAG_SIZE) / bytes_per_sample,
				0, NULL);
	AuMakeElementExportDevice(elms+1, 0, dev, rate,
				AuUnlimitedSamples, 0, NULL);
	AuSetElements(aud, flow, AuTrue, 2, elms, &as);
	if (as != AuSuccess)
		printf("AuSetElements returned status %d!\n", as);
	AuRegisterEventHandler(aud, AuEventHandlerIDMask |
				AuEventHandlerTypeMask,
				AuEventTypeElementNotify, flow,
				event_handler, (AuPointer) NULL);
	AuSetErrorHandler(aud, error_handler);
	AuStartFlow(aud, flow, &as);
	if (as != AuSuccess)
		printf("AuSetElements returned status %d!\n", as);

	/*
	 * Wait for first buffer underrun event
	 *
	 * For some weird reason we get a buffer underrun event if we
	 * don't fill the server buffer fast enough after staring the
	 * flow. So we just wait for it to happen to be in a sane state.
	 */
	wait_for_event();

	pthread_create(&event_thread, NULL, &event_thread_start, NULL);

	return 1;
}

// close audio device
static void uninit(){
	stop_thread = 1;
	pthread_join(event_thread, NULL);
	AuStopFlow(aud, flow, NULL);
	AuCloseServer(aud);
	aud = 0;
	free(client_buffer);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(){
	pthread_mutex_lock(&buffer_mutex);
	client_buffer_used = 0;
	pthread_mutex_unlock(&buffer_mutex);
	while (server_buffer_used > 0) {
		usleep(1000);
//		DPRINTF("used=%d\n", server_buffer_used);
	}
}

// stop playing, keep buffers (for pause)
static void audio_pause()
{
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
static void audio_resume()
{
}

// return: how many bytes can be played without blocking
static int get_space()
{
	int result;

	pthread_mutex_lock(&buffer_mutex);
	result = client_buffer_size - client_buffer_used;
	pthread_mutex_unlock(&buffer_mutex);

	return result;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
	writeBuffer(data, len);
//    printf("ao_nas: wrote %d bytes of audio data\n", len);
	return len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay()
{
	float result;

	pthread_mutex_lock(&buffer_mutex);
	result = ((float)(client_buffer_used + server_buffer_used)) /
		(float)ao_data.bps;
	pthread_mutex_unlock(&buffer_mutex);

	return result;
}
