/*
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

#ifndef MPLAYER_CONTROL_H
#define MPLAYER_CONTROL_H

#include <sys/types.h>

/*********************************************
// Extended control used with arguments that operates on only one
// channel at the time
*/
typedef struct af_control_ext_s{
  void* arg;	// Argument
  int	ch;	// Chanel number
}af_control_ext_t;

/*********************************************
// Control parameters
*/

/* The control system is divided into 3 levels
   mandatory calls 	 - all filters must answer to all of these
   optional calls  	 - are optional
   filter specific calls - applies only to some filters
*/

#define AF_CONTROL_MANDATORY		0x10000000
#define AF_CONTROL_OPTIONAL		0x20000000
#define AF_CONTROL_FILTER_SPECIFIC	0x40000000

// MANDATORY CALLS

/* Reinitialize filter. The optional argument contains the new
   configuration in form of a struct mp_audio struct. If the filter does not
   support the new format the struct should be changed and AF_FALSE
   should be returned. If the incoming and outgoing data streams are
   identical the filter can return AF_DETACH. This will remove the
   filter. */
#define AF_CONTROL_REINIT  		0x00000100 | AF_CONTROL_MANDATORY

// OPTIONAL CALLS

// Called just before destruction of a filter
#define AF_CONTROL_PRE_DESTROY 		0x00000200 | AF_CONTROL_OPTIONAL

/* Commandline parameters. If there were any commandline parameters
   for this specific filter, they will be given as a char* in the
   argument */
#define AF_CONTROL_COMMAND_LINE		0x00000300 | AF_CONTROL_OPTIONAL


// FILTER SPECIFIC CALLS

// Basic operations: These can be ored with any of the below calls
// Set argument
#define AF_CONTROL_SET			0x00000000
// Get argument
#define AF_CONTROL_GET			0x00000001

// Resample

// Set output rate in resample
#define AF_CONTROL_RESAMPLE_RATE	0x00000100 | AF_CONTROL_FILTER_SPECIFIC

// Format

#define AF_CONTROL_FORMAT_FMT		0x00000400 | AF_CONTROL_FILTER_SPECIFIC

// Channels

// Set number of output channels in channels
#define AF_CONTROL_CHANNELS		0x00000600 | AF_CONTROL_FILTER_SPECIFIC

// Volume

// Set volume level, arg is a float* with the volume for all the channels
#define AF_CONTROL_VOLUME_LEVEL		0x00000D00 | AF_CONTROL_FILTER_SPECIFIC

// Pan

// Pan levels, arg is a control_ext with a float*
#define AF_CONTROL_PAN_LEVEL	 	0x00001A00 | AF_CONTROL_FILTER_SPECIFIC

// Number of outputs from pan, arg is int*
#define AF_CONTROL_PAN_NOUT	 	0x00001B00 | AF_CONTROL_FILTER_SPECIFIC

// Balance, arg is float*; range -1 (left) to 1 (right), 0 center
#define AF_CONTROL_PAN_BALANCE	 	0x00001C00 | AF_CONTROL_FILTER_SPECIFIC


// Delay length in ms, arg is a control_ext with a float*
#define AF_CONTROL_DELAY_LEN		0x00001E00 | AF_CONTROL_FILTER_SPECIFIC


// Subwoofer

// Channel number which to insert the filtered data, arg in int*
#define AF_CONTROL_SUB_CH		0x00001F00 | AF_CONTROL_FILTER_SPECIFIC

// Cutoff frequency [Hz] for lowpass filter, arg is float*
#define AF_CONTROL_SUB_FC		0x00002000 | AF_CONTROL_FILTER_SPECIFIC


// Export
#define AF_CONTROL_EXPORT_SZ            0x00003000 | AF_CONTROL_FILTER_SPECIFIC

// Channel number which to inster the filtered data, arg in int*
#define AF_CONTROL_CENTER_CH		0x00003200 | AF_CONTROL_FILTER_SPECIFIC


#define AF_CONTROL_PLAYBACK_SPEED	0x00003500 | AF_CONTROL_FILTER_SPECIFIC
#define AF_CONTROL_SCALETEMPO_AMOUNT	0x00003600 | AF_CONTROL_FILTER_SPECIFIC

#endif /* MPLAYER_CONTROL_H */
