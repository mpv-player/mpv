/*
 * imdct_mlib.c
 * Copyright (C) 2000-2001 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * a52dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * a52dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifdef LIBA52_MLIB

#include <mlib_types.h>
#include <mlib_status.h>
#include <mlib_signal.h>
#include <string.h>
#include <inttypes.h>

#include "a52.h"
#include "a52_internal.h"
#include "attributes.h"

extern sample_t imdct_window[];

void
imdct_do_512_mlib(sample_t data[], sample_t delay[], sample_t bias)
{
	sample_t *buf_real;
	sample_t *buf_imag;
	sample_t *data_ptr;
	sample_t *delay_ptr;
	sample_t *window_ptr;
	sample_t tmp[256] ATTR_ALIGN (16);
	int i;
	
	memcpy(tmp, data, 256 * sizeof(sample_t));
	mlib_SignalIMDCT_F32(tmp);
  
	buf_real = tmp;
	buf_imag = tmp + 128;
	data_ptr = data;
	delay_ptr = delay;
	window_ptr = imdct_window;

	/* Window and convert to real valued signal */
	for(i=0; i< 64; i++) 
	{ 
		*data_ptr++ = -buf_imag[64+i]   * *window_ptr++ + *delay_ptr++ + bias; 
		*data_ptr++ =  buf_real[64-i-1] * *window_ptr++ + *delay_ptr++ + bias; 
	}

	for(i=0; i< 64; i++) 
	{ 
		*data_ptr++ = -buf_real[i]       * *window_ptr++ + *delay_ptr++ + bias; 
		*data_ptr++ =  buf_imag[128-i-1] * *window_ptr++ + *delay_ptr++ + bias; 
	}
	
	/* The trailing edge of the window goes into the delay line */
	delay_ptr = delay;

	for(i=0; i< 64; i++) 
	{ 
		*delay_ptr++ = -buf_real[64+i]   * *--window_ptr; 
		*delay_ptr++ =  buf_imag[64-i-1] * *--window_ptr; 
	}

	for(i=0; i<64; i++) 
	{
		*delay_ptr++ =  buf_imag[i]       * *--window_ptr; 
		*delay_ptr++ = -buf_real[128-i-1] * *--window_ptr; 
	}
}

void
imdct_do_256_mlib(sample_t data[], sample_t delay[], sample_t bias)
{
	sample_t *buf1_real, *buf1_imag;
	sample_t *buf2_real, *buf2_imag;
	sample_t *data_ptr;
	sample_t *delay_ptr;
	sample_t *window_ptr;
	sample_t tmp[256] ATTR_ALIGN (16);
	int i;
	
	memcpy(tmp, data, 256 * sizeof(sample_t));
	mlib_SignalIMDCTSplit_F32(tmp);
  
	buf1_real = tmp;
	buf1_imag = tmp + 128 + 64;
	buf2_real = tmp + 64;
	buf2_imag = tmp + 128;
	data_ptr = data;
	delay_ptr = delay;
	window_ptr = imdct_window;

	/* Window and convert to real valued signal */
	for(i=0; i< 64; i++) 
	{
		*data_ptr++ = -buf1_imag[i]      * *window_ptr++ + *delay_ptr++ + bias;
		*data_ptr++ =  buf1_real[64-i-1] * *window_ptr++ + *delay_ptr++ + bias;
	}

	for(i=0; i< 64; i++) 
	{
		*data_ptr++ = -buf1_real[i]      * *window_ptr++ + *delay_ptr++ + bias;
		*data_ptr++ =  buf1_imag[64-i-1] * *window_ptr++ + *delay_ptr++ + bias;
	}
	
	delay_ptr = delay;

	for(i=0; i< 64; i++) 
	{
		*delay_ptr++ = -buf2_real[i]      * *--window_ptr;
		*delay_ptr++ =  buf2_imag[64-i-1] * *--window_ptr;
	}

	for(i=0; i< 64; i++) 
	{
		*delay_ptr++ =  buf2_imag[i]      * *--window_ptr;
		*delay_ptr++ = -buf2_real[64-i-1] * *--window_ptr;
	}
}

#endif
