/*
 * ao_jack - JACK audio output driver for MPlayer
 *
 * Kamil Strzelecki < esack at browarek.net >
 *
 * This driver is distribuited under terms of GPL
 *
 * It uses bio2jack (http://bio2jack.sf.net/).
 *
 */

#include <stdio.h>
#include <jack/jack.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"
#include "config.h"
#include "mp_msg.h"

//#include "bio2jack.h"

static int driver = 0;
long latency = 0;
long approx_bytes_in_jackd = 0;

//bio2jack stuff:
#define ERR_SUCCESS						0
#define ERR_OPENING_JACK				1
#define ERR_RATE_MISMATCH				2
#define ERR_BYTES_PER_FRAME_INVALID		3
enum status_enum { PLAYING, PAUSED, STOPPED, CLOSED, RESET };
void JACK_Init(void);
int  JACK_Open(int* deviceID, unsigned int bits_per_sample, unsigned long *rate, int channels);
int  JACK_Close(int deviceID); /* return 0 for success */
void JACK_Reset(int deviceID); /* free all buffered data and reset several values in the device */
long JACK_Write(int deviceID, char *data, unsigned long bytes); /* returns the number of bytes written */
long JACK_GetJackLatency(int deviceID); /* return the latency in milliseconds of jack */
int  JACK_SetState(int deviceID, enum status_enum state); /* playing, paused, stopped */
int  JACK_SetAllVolume(int deviceID, unsigned int volume);
int  JACK_SetVolumeForChannel(int deviceID, unsigned int channel, unsigned int volume);
void JACK_GetVolumeForChannel(int deviceID, unsigned int channel, unsigned int *volume);
//


static ao_info_t info =
{
	"JACK audio output",
	"jack",
	"Kamil Strzelecki <esack@browarek.net>",
	""
};


LIBAO_EXTERN(jack)


static int control(int cmd, void *arg)
{
	switch(cmd) {
		case AOCONTROL_GET_VOLUME:	
			{
				ao_control_vol_t *vol = (ao_control_vol_t *)arg;
				unsigned int l, r;
				
				JACK_GetVolumeForChannel(driver, 0, &l);
				JACK_GetVolumeForChannel(driver, 1, &r);
				vol->left = (float )l;
				vol->right = (float )r;
				
				return CONTROL_OK;
			}
		case AOCONTROL_SET_VOLUME:
			{
				ao_control_vol_t *vol = (ao_control_vol_t *)arg;
				unsigned int l = (int )vol->left,
					r = (int )vol->right,
					avg_vol = (l + r) / 2,
					err = 0;

				switch (ao_data.channels) {
				case 6:
					if((err = JACK_SetVolumeForChannel(driver, 5, avg_vol))) {
					mp_msg(MSGT_AO, MSGL_ERR, 
						"AO: [Jack] Setting lfe volume failed, error %d\n",err);
						return CONTROL_ERROR;
					}
				case 5:
					if((err = JACK_SetVolumeForChannel(driver, 4, avg_vol))) {
						mp_msg(MSGT_AO, MSGL_ERR, 
						"AO: [Jack] Setting center volume failed, error %d\n",err);
						return CONTROL_ERROR;
					}
				case 4:
					if((err = JACK_SetVolumeForChannel(driver, 3, r))) {
						mp_msg(MSGT_AO, MSGL_ERR,
						"AO: [Jack] Setting rear right volume failed, error %d\n",err);
					return CONTROL_ERROR;
				}
				case 3:
					if((err = JACK_SetVolumeForChannel(driver, 2, l))) {
						mp_msg(MSGT_AO, MSGL_ERR,
						"AO: [Jack] Setting rear left volume failed, error %d\n",err);
						return CONTROL_ERROR;
					}
				case 2:
				if((err = JACK_SetVolumeForChannel(driver, 1, r))) {
					mp_msg(MSGT_AO, MSGL_ERR, 
						"AO: [Jack] Setting right volume failed, error %d\n",err);
					return CONTROL_ERROR;
				}
				case 1:
					if((err = JACK_SetVolumeForChannel(driver, 0, l))) {
						mp_msg(MSGT_AO, MSGL_ERR,
						"AO: [Jack] Setting left volume failed, error %d\n",err);
						return CONTROL_ERROR;
					}
				}

				return CONTROL_OK;
			}
	}

	return(CONTROL_UNKNOWN);
}


static int init(int rate_hz, int channels, int format, int flags)
{
	int err, m, frag_spec;
	unsigned long rate;
	unsigned int bits_per_sample;
	
	unsigned long jack_port_flags=JackPortIsPhysical;
	unsigned int jack_port_name_count=0;
	const char *jack_port_name=NULL;

	mp_msg(MSGT_AO, MSGL_INFO, "AO: [Jack] Initialising library.\n");
	JACK_Init();

	if (ao_subdevice) {
		jack_port_flags = 0;
		jack_port_name_count = 1;
		jack_port_name = ao_subdevice;
		mp_msg(MSGT_AO, MSGL_INFO, "AO: [Jack] Trying to use jack device:%s.\n", ao_subdevice);
	}

	switch (format) {
		case AFMT_U8:
		case AFMT_S8:
			format = AFMT_U8;
			bits_per_sample = 8;
			m = 1;
			break;
		default:
			format = AFMT_S16_LE;
			bits_per_sample = 16;
			m = 2;
			break;
	}

	rate = rate_hz;
	
	err = JACK_OpenEx(&driver, bits_per_sample, &rate, channels, channels,
			&jack_port_name, jack_port_name_count, jack_port_flags);
	
	/* if sample rates doesn't match try to open device with jack's rate and
	 * let mplayer convert it (rate now contains that which jackd use) */
	if(err == ERR_RATE_MISMATCH) {
		mp_msg(MSGT_AO, MSGL_INFO, 
				"AO: [Jack] Sample rate mismatch, trying to resample.\n");
		
		err = JACK_OpenEx(&driver, bits_per_sample, &rate, channels, channels,
				&jack_port_name, jack_port_name_count, jack_port_flags);
	}
	
	/* any other error */
	if(err != ERR_SUCCESS) {
		mp_msg(MSGT_AO, MSGL_ERR, 
				"AO: [Jack] JACK_Open() failed, error %d\n", err);
		return 0;
	}
	
	err = JACK_SetAllVolume(driver, 100);
	if(err != ERR_SUCCESS) {
		// This is not fatal, but would be peculiar...
		mp_msg(MSGT_AO, MSGL_ERR,
			"AO: [Jack] JACK_SetAllVolume() failed, error %d\n", err);
	}

	latency = JACK_GetJackLatency(driver);

	ao_data.format = format;
	ao_data.channels = channels;
	ao_data.samplerate = rate;
	ao_data.bps = ( rate * channels * m );

	// Rather rough way to find out the rough number of bytes buffered
	approx_bytes_in_jackd = JACK_GetJackBufferedBytes(driver) * 2;
	
	mp_msg(MSGT_AO, MSGL_INFO, 
			"AO: [Jack] OK. I'm ready to go (%d Hz/%d channels/%d bit)\n",
			ao_data.samplerate, ao_data.channels, bits_per_sample);

	return 1;
}


static void uninit(int immed)
{
	int errval = 0;
	
	if((errval = JACK_Close(driver)))
		mp_msg(MSGT_AO, MSGL_ERR, 
				"AO: [Jack] error closing device, error %d\n", errval);
}


static int play(void* data,int len,int flags)
{
	return JACK_Write(driver, data, len);
}


static void audio_pause()
{
	JACK_SetState(driver, PAUSED);
}


static void audio_resume()
{
	JACK_SetState(driver, PLAYING);
}


static void reset()
{
	JACK_Reset(driver);
	latency = JACK_GetJackLatency(driver);
	// Rather rough way to find out the rough number of bytes buffered
	approx_bytes_in_jackd = JACK_GetJackBufferedBytes(driver) * 2;
}


static int get_space()
{
	return JACK_GetBytesFreeSpace(driver);
}


static float get_delay()
{
	float ret = 0;
	ret = (JACK_GetBytesStored(driver) + approx_bytes_in_jackd + latency) / (float)ao_data.bps;
	return ret;
}

