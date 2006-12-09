#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "libaf/af_format.h"
#include "ad_internal.h"

#include "libmpdemux/mp3_hdr.h"

//based on ad_hwac3.c and ad_libmad.c

static ad_info_t info = 
{
	"MPEG audio pass-through (fake decoder)",
	"hwmpa",
	"NicoDVB",
	"NicoDVB",
	"For hardware decoders"
};

LIBAD_EXTERN(hwmpa)

static int mpa_sync(sh_audio_t *sh, int no_frames, int *n, int *chans, int *srate, int *spf, int *mpa_layer, int *br)
{
	int cnt = 0, x = 0, len, frames_count;

	frames_count = 0;
	do
	{
		while(cnt + 4 < sh->a_in_buffer_len)
		{
			x = mp_get_mp3_header(&(sh->a_in_buffer[cnt]), chans, srate, spf, mpa_layer, br);
			if(x > 0)
			{
				frames_count++;
				if(frames_count == no_frames)
				{
					*n = x;
					return cnt;
				}
			}
			cnt++;
		}
		len = demux_read_data(sh->ds,&sh->a_in_buffer[sh->a_in_buffer_len],sh->a_in_buffer_size-sh->a_in_buffer_len);
		if(len > 0)
			sh->a_in_buffer_len += len;
	} while(len > 0);
	mp_msg(MSGT_DECAUDIO,MSGL_INFO,"Cannot sync MPA frame: %d\r\n", len);
	return -1;
}

static int preinit(sh_audio_t *sh)
{
	sh->audio_out_minsize = 4608;//check
	sh->audio_in_minsize = 4608;//check
	sh->sample_format = AF_FORMAT_MPEG2;
	return 1;
}

static int init(sh_audio_t *sh)
{
	int cnt, chans, srate, spf, mpa_layer, br, len;

	if((cnt = mpa_sync(sh, 1, &len, &chans, &srate, &spf, &mpa_layer, &br)) < 0)
		return 0;

	sh->channels = chans;
	sh->samplerate = srate;
	sh->i_bps = br * 125;
	sh->samplesize = 2;
	
	mp_msg(MSGT_DECAUDIO,MSGL_V,"AC_HWMPA initialized, bitrate: %d kb/s\r\n", len);
	return 1;
}

static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen)
{
	int len, start, tot;
	int chans, srate, spf, mpa_layer, br;
	int tot2;

	tot = tot2 = 0;

	while(tot2 < maxlen)
	{
		start = mpa_sync(sh, 1, &len, &chans, &srate, &spf, &mpa_layer, &br);
		if(start < 0 || tot2 + spf * 2 * chans > maxlen)
			break;

		if(start + len > sh->a_in_buffer_len)
		{
			int l;
			l = FFMIN(sh->a_in_buffer_size - sh->a_in_buffer_len, start + len);
			l = demux_read_data(sh->ds,&sh->a_in_buffer[sh->a_in_buffer_len], l);
			if(! l)
				break;
			sh->a_in_buffer_len += l;
			continue;
		}

		memcpy(&buf[tot], &(sh->a_in_buffer[start]), len);
		tot += len;

		sh->a_in_buffer_len -= start + len;
		memmove(sh->a_in_buffer, &(sh->a_in_buffer[start + len]), sh->a_in_buffer_len);
		tot2 += spf * 2 * chans;

                /* HACK: seems to fix most A/V sync issues */
                break;
	}

	memset(&buf[tot], 0, tot2-tot);
	return tot2;
}


static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
	int start, len;

	switch(cmd)
	{
		case ADCTRL_RESYNC_STREAM:
			if(mpa_sync(sh, 1, &len, NULL, NULL, NULL, NULL, NULL) >= 0)
				return CONTROL_TRUE;
			else
				return CONTROL_FALSE;
		case ADCTRL_SKIP_FRAME:
			start = mpa_sync(sh, 2, &len, NULL, NULL, NULL, NULL, NULL);
			if(len < 0)
				return CONTROL_FALSE;

			sh->a_in_buffer_len -= start;
			memmove(sh->a_in_buffer, &(sh->a_in_buffer[start]), sh->a_in_buffer_len);
			return CONTROL_TRUE;
	}
	return CONTROL_UNKNOWN;
}


static void uninit(sh_audio_t *sh)
{
}
