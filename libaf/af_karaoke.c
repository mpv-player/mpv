/*
 * simple voice removal filter
 *
 * copyright (c) 2006 Reynaldo H. Verdejo Pinochet
 * Based on code by Alex Beregszaszi for his 'center' filter.
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "af.h"

// Data for specific instances of this filter

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
	switch(cmd){
		case AF_CONTROL_REINIT:
		af->data->rate	= ((af_data_t*)arg)->rate;
		af->data->nch	= ((af_data_t*)arg)->nch;
		af->data->format= AF_FORMAT_FLOAT_NE;
		af->data->bps	= 4;
		return af_test_output(af,(af_data_t*)arg);
	}
	return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance_s* af)
{
	if(af->data)
		free(af->data);
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
	af_data_t*	c	= data;		 // Current working data
	float*		a	= c->audio;	 // Audio data
	int			len	= c->len/4;	 // Number of samples in current audio block
	int			nch	= c->nch;	 // Number of channels
	register int  i;

	/*
		FIXME1 add a low band pass filter to avoid suppressing
		centered bass/drums
		FIXME2 better calculated* attenuation factor
	*/

	for(i=0;i<len;i+=nch)
	{
		a[i] = (a[i] - a[i+1]) * 0.7;
		a[i+1]=a[i];
	}

	return c;
}

// Allocate memory and set function pointers
static int af_open(af_instance_t* af){
	af->control	= control;
	af->uninit	= uninit;
	af->play	= play;
	af->mul		= 1;
	af->data	= calloc(1,sizeof(af_data_t));

	if(af->data == NULL)
		return AF_ERROR;

	return AF_OK;
}

// Description of this filter
af_info_t af_info_karaoke = {
	"Simple karaoke/voice-removal audio filter",
	"karaoke",
	"Reynaldo H. Verdejo Pinochet",
	"",
	AF_FLAGS_NOT_REENTRANT,
	af_open
};
