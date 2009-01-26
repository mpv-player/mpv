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

#ifndef MPLAYER_AF_H
#define MPLAYER_AF_H

#include <stdio.h>

#include "config.h"
#include "af_mp.h"
#include "control.h"
#include "af_format.h"

struct af_instance_s;

// Number of channels
#ifndef AF_NCH
#define AF_NCH 6
#endif

// Audio data chunk
typedef struct af_data_s
{
  void* audio;  // data buffer
  int len;      // buffer length
  int rate;	// sample rate
  int nch;	// number of channels
  int format;	// format
  int bps; 	// bytes per sample
} af_data_t;


// Flags used for defining the behavior of an audio filter
#define AF_FLAGS_REENTRANT 	0x00000000
#define AF_FLAGS_NOT_REENTRANT 	0x00000001

/* Audio filter information not specific for current instance, but for
   a specific filter */ 
typedef struct af_info_s 
{
  const char *info;
  const char *name;
  const char *author;
  const char *comment;
  const int flags;
  int (*open)(struct af_instance_s* vf);
} af_info_t;

// Linked list of audio filters
typedef struct af_instance_s
{
  af_info_t* info;
  int (*control)(struct af_instance_s* af, int cmd, void* arg);
  void (*uninit)(struct af_instance_s* af);
  af_data_t* (*play)(struct af_instance_s* af, af_data_t* data);
  void* setup;	  // setup data for this specific instance and filter
  af_data_t* data; // configuration for outgoing data stream
  struct af_instance_s* next;
  struct af_instance_s* prev;  
  double delay; /* Delay caused by the filter, in units of bytes read without
		 * corresponding output */
  double mul; /* length multiplier: how much does this instance change
		 the length of the buffer. */
}af_instance_t;

// Initialization flags
extern int* af_cpu_speed;

#define AF_INIT_AUTO		0x00000000
#define AF_INIT_SLOW		0x00000001
#define AF_INIT_FAST		0x00000002
#define AF_INIT_FORCE	  	0x00000003
#define AF_INIT_TYPE_MASK 	0x00000003

#define AF_INIT_INT		0x00000000
#define AF_INIT_FLOAT		0x00000004
#define AF_INIT_FORMAT_MASK	0x00000004

// Default init type 
#ifndef AF_INIT_TYPE
#if HAVE_SSE || HAVE_AMD3DNOW
#define AF_INIT_TYPE (af_cpu_speed?*af_cpu_speed:AF_INIT_FAST)
#else
#define AF_INIT_TYPE (af_cpu_speed?*af_cpu_speed:AF_INIT_SLOW)
#endif
#endif

// Configuration switches
typedef struct af_cfg_s{
  int force;	// Initialization type
  char** list;	/* list of names of filters that are added to filter
		   list during first initialization of stream */
}af_cfg_t;

// Current audio stream
typedef struct af_stream_s
{
  // The first and last filter in the list
  af_instance_t* first;
  af_instance_t* last;
  // Storage for input and output data formats
  af_data_t input;
  af_data_t output;
  // Configuration for this stream
  af_cfg_t cfg;
}af_stream_t;

/*********************************************
// Return values
*/

#define AF_DETACH   2
#define AF_OK       1
#define AF_TRUE     1
#define AF_FALSE    0
#define AF_UNKNOWN -1
#define AF_ERROR   -2
#define AF_FATAL   -3



/*********************************************
// Export functions
*/

/**
 * \defgroup af_chain Audio filter chain functions
 * \{
 * \param s filter chain
 */

/**
 * \brief Initialize the stream "s".
 * \return 0 on success, -1 on failure
 *
 * This function creates a new filter list if necessary, according
 * to the values set in input and output. Input and output should contain
 * the format of the current movie and the format of the preferred output
 * respectively.
 * Filters to convert to the preferred output format are inserted
 * automatically, except when they are set to 0.
 * The function is reentrant i.e. if called with an already initialized
 * stream the stream will be reinitialized.
 */
int af_init(af_stream_t* s);

/**
 * \brief Uninit and remove all filters from audio filter chain
 */
void af_uninit(af_stream_t* s);

/**
 * \brief This function adds the filter "name" to the stream s.
 * \param name name of filter to add
 * \return pointer to the new filter, NULL if insert failed
 *
 * The filter will be inserted somewhere nice in the
 * list of filters (i.e. at the beginning unless the
 * first filter is the format filter (why??).
 */
af_instance_t* af_add(af_stream_t* s, char* name);

/**
 * \brief Uninit and remove the filter "af"
 * \param af filter to remove
 */
void af_remove(af_stream_t* s, af_instance_t* af);

/**
 * \brief find filter in chain by name
 * \param name name of the filter to find
 * \return first filter with right name or NULL if not found
 * 
 * This function is used for finding already initialized filters
 */
af_instance_t* af_get(af_stream_t* s, char* name);

/**
 * \brief filter data chunk through the filters in the list
 * \param data data to play
 * \return resulting data
 * \ingroup af_chain
 */
af_data_t* af_play(af_stream_t* s, af_data_t* data);

/**
 * \brief send control to all filters, starting with the last until
 *        one accepts the command with AF_OK.
 * \param cmd filter control command
 * \param arg argument for filter command
 * \return the accepting filter or NULL if none was found
 */
af_instance_t *af_control_any_rev (af_stream_t* s, int cmd, void* arg);

/**
 * \brief calculate average ratio of filter output lenth to input length
 * \return the ratio
 */
double af_calc_filter_multiplier(af_stream_t* s);

/**
 * \brief Calculate the total delay caused by the filters
 * \return delay in bytes of "missing" output
 */
double af_calc_delay(af_stream_t* s);

/** \} */ // end of af_chain group

// Helper functions and macros used inside the audio filters

/**
 * \defgroup af_filter Audio filter helper functions
 * \{
 */

/* Helper function called by the macro with the same name only to be
   called from inside filters */
int af_resize_local_buffer(af_instance_t* af, af_data_t* data);

/* Helper function used to calculate the exact buffer length needed
   when buffers are resized. The returned length is >= than what is
   needed */
int af_lencalc(double mul, af_data_t* data);

/**
 * \brief convert dB to gain value
 * \param n number of values to convert
 * \param in [in] values in dB, <= -200 will become 0 gain
 * \param out [out] gain values
 * \param k input values are divided by this
 * \param mi minimum dB value, input will be clamped to this
 * \param ma maximum dB value, input will be clamped to this
 * \return AF_ERROR on error, AF_OK otherwise
 */
int af_from_dB(int n, float* in, float* out, float k, float mi, float ma);

/**
 * \brief convert gain value to dB
 * \param n number of values to convert
 * \param in [in] gain values, 0 wil become -200 dB
 * \param out [out] values in dB
 * \param k output values will be multiplied by this
 * \return AF_ERROR on error, AF_OK otherwise
 */
int af_to_dB(int n, float* in, float* out, float k);

/**
 * \brief convert milliseconds to sample time
 * \param n number of values to convert
 * \param in [in] values in milliseconds
 * \param out [out] sample time values
 * \param rate sample rate
 * \param mi minimum ms value, input will be clamped to this
 * \param ma maximum ms value, input will be clamped to this
 * \return AF_ERROR on error, AF_OK otherwise
 */
int af_from_ms(int n, float* in, int* out, int rate, float mi, float ma);

/**
 * \brief convert sample time to milliseconds
 * \param n number of values to convert
 * \param in [in] sample time values
 * \param out [out] values in milliseconds
 * \param rate sample rate
 * \return AF_ERROR on error, AF_OK otherwise
 */
int af_to_ms(int n, int* in, float* out, int rate); 

/**
 * \brief test if output format matches
 * \param af audio filter
 * \param out needed format, will be overwritten by available
 *            format if they do not match
 * \return AF_FALSE if formats do not match, AF_OK if they match
 *
 * compares the format, bps, rate and nch values of af->data with out
 */
int af_test_output(struct af_instance_s* af, af_data_t* out);

/**
 * \brief soft clipping function using sin()
 * \param a input value
 * \return clipped value
 */
float af_softclip(float a);

/** \} */ // end of af_filter group, but more functions of this group below

/** Print a list of all available audio filters */
void af_help(void);

/**
 * \brief fill the missing parameters in the af_data_t structure
 * \param data structure to fill
 * \ingroup af_filter
 * 
 * Currently only sets bps based on format
 */
void af_fix_parameters(af_data_t *data);

/** Memory reallocation macro: if a local buffer is used (i.e. if the
   filter doesn't operate on the incoming buffer this macro must be
   called to ensure the buffer is big enough.
 * \ingroup af_filter
 */
#define RESIZE_LOCAL_BUFFER(a,d)\
((a->data->len < af_lencalc(a->mul,d))?af_resize_local_buffer(a,d):AF_OK)

/* Some other useful macro definitions*/
#ifndef min
#define min(a,b)(((a)>(b))?(b):(a))
#endif

#ifndef max
#define max(a,b)(((a)>(b))?(a):(b))
#endif

#ifndef clamp
#define clamp(a,min,max) (((a)>(max))?(max):(((a)<(min))?(min):(a)))
#endif

#ifndef sign
#define sign(a) (((a)>0)?(1):(-1)) 
#endif

#ifndef lrnd
#define lrnd(a,b) ((b)((a)>=0.0?(a)+0.5:(a)-0.5))
#endif

/* Error messages */

typedef struct af_msg_cfg_s
{
  int level;   	/* Message level for debug and error messages max = 2
		   min = -2 default = 0 */
  FILE* err;	// Stream to print error messages to
  FILE* msg;	// Stream to print information messages to
}af_msg_cfg_t;

extern af_msg_cfg_t af_msg_cfg; // Message 

//! \addtogroup af_filter
//! \{
#define AF_MSG_FATAL	-3 ///< Fatal error exit immediately
#define AF_MSG_ERROR    -2 ///< Error return gracefully
#define AF_MSG_WARN     -1 ///< Print warning but do not exit (can be suppressed)
#define AF_MSG_INFO	 0 ///< Important information
#define AF_MSG_VERBOSE	 1 ///< Print this if verbose is enabled 
#define AF_MSG_DEBUG0	 2 ///< Print if very verbose
#define AF_MSG_DEBUG1	 3 ///< Print if very very verbose 

//! Macro for printing error messages
#ifndef af_msg
#define af_msg(lev, args... ) \
(((lev)<AF_MSG_WARN)?(fprintf(af_msg_cfg.err?af_msg_cfg.err:stderr, ## args )): \
(((lev)<=af_msg_cfg.level)?(fprintf(af_msg_cfg.msg?af_msg_cfg.msg:stdout, ## args )):0))
#endif
//! \}

#endif /* MPLAYER_AF_H */
