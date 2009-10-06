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
// Control info struct.
//
// This struct is the argument in a info call to a filter.
*/

// Argument types
#define AF_CONTROL_TYPE_BOOL	(0x0<<0)
#define AF_CONTROL_TYPE_CHAR	(0x1<<0)
#define AF_CONTROL_TYPE_INT	(0x2<<0)
#define AF_CONTROL_TYPE_FLOAT	(0x3<<0)
#define AF_CONTROL_TYPE_STRUCT	(0x4<<0)
#define AF_CONTROL_TYPE_SPECIAL	(0x5<<0) // a pointer to a function for example
#define AF_CONTROL_TYPE_MASK	(0x7<<0)
// Argument geometry
#define AF_CONTROL_GEOM_SCALAR	(0x0<<3)
#define AF_CONTROL_GEOM_ARRAY	(0x1<<3)
#define AF_CONTROL_GEOM_MATRIX	(0x2<<3)
#define AF_CONTROL_GEOM_MASK	(0x3<<3)
// Argument properties
#define AF_CONTROL_PROP_READ	(0x0<<5) // The argument can be read
#define AF_CONTROL_PROP_WRITE	(0x1<<5) // The argument can be written
#define AF_CONTROL_PROP_SAVE	(0x2<<5) // Can be saved
#define AF_CONTROL_PROP_RUNTIME	(0x4<<5) // Acessable during execution
#define AF_CONTROL_PROP_CHANNEL (0x8<<5) // Argument is set per channel
#define AF_CONTROL_PROP_MASK	(0xF<<5)

typedef struct af_control_info_s{
  int	 def;	// Control enumrification
  char*	 name; 	// Name of argument
  char*	 info;	// Description of what it does
  int 	 flags;	// Flags as defined above
  float	 max;	// Max and min value
  float	 min;	// (only aplicable on float and int)
  int	 xdim;	// 1st dimension
  int	 ydim;	// 2nd dimension (=0 for everything except matrix)
  size_t sz;	// Size of argument in bytes
  int	 ch;	// Channel number (for future use)
  void*  arg;	// Data (for future use)
}af_control_info_t;


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
   configuration in form of a af_data_t struct. If the filter does not
   support the new format the struct should be changed and AF_FALSE
   should be returned. If the incoming and outgoing data streams are
   identical the filter can return AF_DETACH. This will remove the
   filter. */
#define AF_CONTROL_REINIT  		0x00000100 | AF_CONTROL_MANDATORY

// OPTIONAL CALLS

/* Called just after creation with the af_cfg for the stream in which
   the filter resides as input parameter this call can be used by the
   filter to initialize itself */
#define AF_CONTROL_POST_CREATE 		0x00000100 | AF_CONTROL_OPTIONAL

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
// Get info about the control, i.e fill in everything except argument
#define AF_CONTROL_INFO			0x00000002

// Resample

// Set output rate in resample
#define AF_CONTROL_RESAMPLE_RATE	0x00000100 | AF_CONTROL_FILTER_SPECIFIC

// Enable sloppy resampling
#define AF_CONTROL_RESAMPLE_SLOPPY	0x00000200 | AF_CONTROL_FILTER_SPECIFIC

// Set resampling accuracy
#define AF_CONTROL_RESAMPLE_ACCURACY	0x00000300 | AF_CONTROL_FILTER_SPECIFIC

// Format

#define AF_CONTROL_FORMAT_FMT		0x00000400 | AF_CONTROL_FILTER_SPECIFIC

// Channels

// Set number of output channels in channels
#define AF_CONTROL_CHANNELS		0x00000600 | AF_CONTROL_FILTER_SPECIFIC

// Set number of channel routes
#define AF_CONTROL_CHANNELS_ROUTES	0x00000700 | AF_CONTROL_FILTER_SPECIFIC

// Set channel routing pair, arg is int[2] and ch is used
#define AF_CONTROL_CHANNELS_ROUTING	0x00000800 | AF_CONTROL_FILTER_SPECIFIC

// Set nuber of channel routing pairs, arg is int*
#define AF_CONTROL_CHANNELS_NR		0x00000900 | AF_CONTROL_FILTER_SPECIFIC

// Set make af_channels into a router
#define AF_CONTROL_CHANNELS_ROUTER	0x00000A00 | AF_CONTROL_FILTER_SPECIFIC

// Volume

// Turn volume control on and off, arg is int*
#define AF_CONTROL_VOLUME_ON_OFF	0x00000B00 | AF_CONTROL_FILTER_SPECIFIC

// Turn soft clipping of the volume on and off, arg is binary
#define AF_CONTROL_VOLUME_SOFTCLIP	0x00000C00 | AF_CONTROL_FILTER_SPECIFIC

// Set volume level, arg is a float* with the volume for all the channels
#define AF_CONTROL_VOLUME_LEVEL		0x00000D00 | AF_CONTROL_FILTER_SPECIFIC

// Probed power level for all channels, arg is a float*
#define AF_CONTROL_VOLUME_PROBE		0x00000E00 | AF_CONTROL_FILTER_SPECIFIC

// Maximum probed power level for all channels, arg is a float*
#define AF_CONTROL_VOLUME_PROBE_MAX	0x00000F00 | AF_CONTROL_FILTER_SPECIFIC

// Compressor/expander

// Turn compressor/expander on and off
#define AF_CONTROL_COMP_ON_OFF	 	0x00001000 | AF_CONTROL_FILTER_SPECIFIC

// Compression/expansion threshold [dB]
#define AF_CONTROL_COMP_THRESH	 	0x00001100 | AF_CONTROL_FILTER_SPECIFIC

// Compression/expansion attack time [ms]
#define AF_CONTROL_COMP_ATTACK	 	0x00001200 | AF_CONTROL_FILTER_SPECIFIC

// Compression/expansion release time [ms]
#define AF_CONTROL_COMP_RELEASE 	0x00001300 | AF_CONTROL_FILTER_SPECIFIC

// Compression/expansion gain level [dB]
#define AF_CONTROL_COMP_RATIO	 	0x00001400 | AF_CONTROL_FILTER_SPECIFIC

// Noise gate

// Turn noise gate on an off
#define AF_CONTROL_GATE_ON_OFF	 	0x00001500 | AF_CONTROL_FILTER_SPECIFIC

// Noise gate threshold [dB]
#define AF_CONTROL_GATE_THRESH	 	0x00001600 | AF_CONTROL_FILTER_SPECIFIC

// Noise gate attack time [ms]
#define AF_CONTROL_GATE_ATTACK	 	0x00001700 | AF_CONTROL_FILTER_SPECIFIC

// Noise gate release time [ms]
#define AF_CONTROL_GATE_RELEASE 	0x00001800 | AF_CONTROL_FILTER_SPECIFIC

// Noise gate release range level [dB]
#define AF_CONTROL_GATE_RANGE	 	0x00001900 | AF_CONTROL_FILTER_SPECIFIC

// Pan

// Pan levels, arg is a control_ext with a float*
#define AF_CONTROL_PAN_LEVEL	 	0x00001A00 | AF_CONTROL_FILTER_SPECIFIC

// Number of outputs from pan, arg is int*
#define AF_CONTROL_PAN_NOUT	 	0x00001B00 | AF_CONTROL_FILTER_SPECIFIC

// Balance, arg is float*; range -1 (left) to 1 (right), 0 center
#define AF_CONTROL_PAN_BALANCE	 	0x00001C00 | AF_CONTROL_FILTER_SPECIFIC

// Set equalizer gain, arg is a control_ext with a float*
#define AF_CONTROL_EQUALIZER_GAIN 	0x00001D00 | AF_CONTROL_FILTER_SPECIFIC


// Delay length in ms, arg is a control_ext with a float*
#define AF_CONTROL_DELAY_LEN		0x00001E00 | AF_CONTROL_FILTER_SPECIFIC


// Subwoofer

// Channel number which to insert the filtered data, arg in int*
#define AF_CONTROL_SUB_CH		0x00001F00 | AF_CONTROL_FILTER_SPECIFIC

// Cutoff frequency [Hz] for lowpass filter, arg is float*
#define AF_CONTROL_SUB_FC		0x00002000 | AF_CONTROL_FILTER_SPECIFIC


// Export
#define AF_CONTROL_EXPORT_SZ            0x00003000 | AF_CONTROL_FILTER_SPECIFIC


// ExtraStereo Multiplier
#define AF_CONTROL_ES_MUL		0x00003100 | AF_CONTROL_FILTER_SPECIFIC


// Center

// Channel number which to inster the filtered data, arg in int*
#define AF_CONTROL_CENTER_CH		0x00003200 | AF_CONTROL_FILTER_SPECIFIC


// SineSuppress
#define AF_CONTROL_SS_FREQ		0x00003300 | AF_CONTROL_FILTER_SPECIFIC
#define AF_CONTROL_SS_DECAY		0x00003400 | AF_CONTROL_FILTER_SPECIFIC

#define AF_CONTROL_PLAYBACK_SPEED	0x00003500 | AF_CONTROL_FILTER_SPECIFIC
#define AF_CONTROL_SCALETEMPO_AMOUNT	0x00003600 | AF_CONTROL_FILTER_SPECIFIC

#endif /* MPLAYER_CONTROL_H */
