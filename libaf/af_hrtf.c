/* Experimental audio filter that mixes 5.1 and 5.1 with matrix
   encoded rear channels into headphone signal using FIR filtering
   with HRTF.
*/
//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include <math.h>

#include "af.h"
#include "dsp.h"

/* HRTF filter coefficients and adjustable parameters */
#include "af_hrtf.h"

typedef struct af_hrtf_s {
    /* Lengths */
    int dlbuflen, hrflen, basslen;
    /* L, C, R, Ls, Rs channels */
    float *lf, *rf, *lr, *rr, *cf, *cr;
    float *cf_ir, *af_ir, *of_ir, *ar_ir, *or_ir, *cr_ir;
    int cf_o, af_o, of_o, ar_o, or_o, cr_o;
    /* Bass */
    float *ba_l, *ba_r;
    float *ba_ir;
    /* Whether to matrix decode the rear center channel */
    int matrix_mode;
    /* Full wave rectified amplitude used to steer the active matrix
       decoding of center rear channel */
    float lr_fwr, rr_fwr;
    /* Cyclic position on the ring buffer */
    int cyc_pos;
} af_hrtf_t;

/* Convolution on a ring buffer
 *    nx:	length of the ring buffer
 *    nk:	length of the convolution kernel
 *    sx:	ring buffer
 *    sk:	convolution kernel
 *    offset:	offset on the ring buffer, can be 
 */
static float conv(const int nx, const int nk, float *sx, float *sk,
		  const int offset)
{
    /* k = reminder of offset / nx */
    int k = offset >= 0 ? offset % nx : nx + (offset % nx);

    if(nk + k <= nx)
	return fir(nk, sx + k, sk);
    else
	return fir(nk + k - nx, sx, sk + nx - k) +
	    fir(nx - k, sx + k, sk);
}

/* Detect when the impulse response starts (significantly) */
int pulse_detect(float *sx)
{
    /* nmax must be the reference impulse response length (128) minus
       s->hrflen */
    const int nmax = 128 - HRTFFILTLEN;
    const float thresh = IRTHRESH;
    int i;

    for(i = 0; i < nmax; i++)
	if(fabs(sx[i]) > thresh)
	    return i;
    return 0;
}

inline void update_ch(af_hrtf_t *s, short *in, const int k)
{
    /* Update the full wave rectified total amplutude */
    s->lr_fwr += abs(in[2]) - fabs(s->lr[k]);
    s->rr_fwr += abs(in[3]) - fabs(s->rr[k]);

    s->lf[k] = in[0];
    s->cf[k] = in[4];
    s->rf[k] = in[1];
    s->lr[k] = in[2];
    s->rr[k] = in[3];

    s->ba_l[k] = in[0] + in[4] + in[2];
    s->ba_r[k] = in[4] + in[1] + in[3];
}

inline void matrix_decode_cr(af_hrtf_t *s, short *in, const int k)
{
    /* Active matrix decoding of the center rear channel, 1 in the
       denominator is to prevent singularity */
    float lr_agc = in[2] * (s->lr_fwr + s->rr_fwr) /
	(1 + s->lr_fwr + s->lr_fwr);
    float rr_agc = in[3] * (s->lr_fwr + s->rr_fwr) /
	(1 + s->rr_fwr + s->rr_fwr);

    s->cr[k] = (lr_agc + rr_agc) * M_SQRT1_2;
}

/* Initialization and runtime control */
static int control(struct af_instance_s *af, int cmd, void* arg)
{
    af_hrtf_t *s = af->setup;
    char mode;

    switch(cmd) {
    case AF_CONTROL_REINIT:
	af->data->rate   = ((af_data_t*)arg)->rate;
	if(af->data->rate != 48000) {
	    // automatic samplerate adjustment in the filter chain
	    // is not yet supported.
	    af_msg(AF_MSG_ERROR,
		   "[hrtf] ERROR: Sampling rate is not 48000 Hz (%d)!\n",
		   af->data->rate);
	    return AF_ERROR;
	}
	af->data->nch    = ((af_data_t*)arg)->nch;
	if(af->data->nch < 5) {
	    af->data->nch = 5;
	}
	af->data->format = AF_FORMAT_SI | AF_FORMAT_NE;
	af->data->bps    = 2;
	return af_test_output(af, (af_data_t*)arg);
    case AF_CONTROL_COMMAND_LINE:
	sscanf((char*)arg, "%c", &mode);
	switch(mode) {
	case 'm':
	    s->matrix_mode = 1;
	    break;
	case '0':
	    s->matrix_mode = 0;
	    break;
	default:
	    af_msg(AF_MSG_ERROR,
		   "[hrtf] Mode is neither 'm', nor '0' (%c).\n",
		   mode);
	    return AF_ERROR;
	}
	return AF_OK;
    }    

    af_msg(AF_MSG_INFO,
	   "[hrtf] Using HRTF to mix %s discrete surround into "
	   "L, R channels\n", s->matrix_mode ? "5" : "5+1");
    if(s->matrix_mode)
	af_msg(AF_MSG_INFO,
	       "[hrtf] Using active matrix to decode rear center "
	       "channel\n");

    return AF_UNKNOWN;
}

/* Deallocate memory */
static void uninit(struct af_instance_s *af)
{
    if(af->setup) {
	af_hrtf_t *s = af->setup;

	if(s->lf)
	    free(s->lf);
	if(s->rf)
	    free(s->rf);
	if(s->lr)
	    free(s->lr);
	if(s->rr)
	    free(s->rr);
	if(s->cf)
	    free(s->cf);
	if(s->cr)
	    free(s->cr);
	if(s->ba_l)
	    free(s->ba_l);
	if(s->ba_r)
	    free(s->ba_r);
	if(s->ba_ir)
	    free(s->ba_ir);
	free(af->setup);
    }
    if(af->data)
	free(af->data);
}

/* Filter data through filter

Two "tricks" are used to compensate the "color" of the KEMAR data:

1. The KEMAR data is refiltered to ensure that the front L, R channels
on the same side of the ear are equalized (especially in the high
frequencies).

2. A bass compensation is introduced to ensure that 0-200 Hz are not
damped (without any real 3D acoustical image, however).
*/
static af_data_t* play(struct af_instance_s *af, af_data_t *data)
{
    af_hrtf_t *s = af->setup;
    short *in = data->audio; // Input audio data
    short *out = NULL; // Output audio data
    short *end = in + data->len / sizeof(short); // Loop end
    float common, left, right, diff, left_b, right_b;
    const int dblen = s->dlbuflen, hlen = s->hrflen, blen = s->basslen;

    if(AF_OK != RESIZE_LOCAL_BUFFER(af, data))
	return NULL;

    out = af->data->audio;

    /* MPlayer's 5 channel layout (notation for the variable):
     * 
     * 0: L (LF), 1: R (RF), 2: Ls (LR), 3: Rs (RR), 4: C (CF), matrix
     * encoded: Cs (CR)
     * 
     * or: L = left, C = center, R = right, F = front, R = rear
     * 
     * Filter notation:
     * 
     *      CF
     * OF        AF
     *      Ear->
     * OR        AR
     *      CR
     * 
     * or: C = center, A = same side, O = opposite, F = front, R = rear
     */

    while(in < end) {
	const int k = s->cyc_pos;

	update_ch(s, in, k);

	/* Simulate a 7.5 ms -20 dB echo of the center channel in the
	   front channels (like reflection from a room wall) - a kind of
	   psycho-acoustically "cheating" to focus the center front
	   channel, which is normally hard to be perceived as front */
	s->lf[k] += CFECHOAMPL * s->cf[(k + CFECHODELAY) % s->dlbuflen];
	s->rf[k] += CFECHOAMPL * s->cf[(k + CFECHODELAY) % s->dlbuflen];

	/* Mixer filter matrix */
	common = conv(dblen, hlen, s->cf, s->cf_ir, k + s->cf_o);
	if(s->matrix_mode) {
	    /* In matrix decoding mode, the rear channel gain must be
	       renormalized, as there is an additional channel. */
	    matrix_decode_cr(s, in, k);
	    common +=
		conv(dblen, hlen, s->cr, s->cr_ir, k + s->cr_o) *
		M1_76DB;
	    left    =
		( conv(dblen, hlen, s->lf, s->af_ir, k + s->af_o) +
		  conv(dblen, hlen, s->rf, s->of_ir, k + s->of_o) +
		  (conv(dblen, hlen, s->lr, s->ar_ir, k + s->ar_o) +
		   conv(dblen, hlen, s->rr, s->or_ir, k + s->or_o)) *
		  M1_76DB + common);
	    right   =
		( conv(dblen, hlen, s->rf, s->af_ir, k + s->af_o) +
		  conv(dblen, hlen, s->lf, s->of_ir, k + s->of_o) +
		  (conv(dblen, hlen, s->rr, s->ar_ir, k + s->ar_o) +
		   conv(dblen, hlen, s->lr, s->or_ir, k + s->or_o)) *
		  M1_76DB + common);
	}
	else {
	    left    =
		( conv(dblen, hlen, s->lf, s->af_ir, k + s->af_o) +
		  conv(dblen, hlen, s->rf, s->of_ir, k + s->of_o) +
		  conv(dblen, hlen, s->lr, s->ar_ir, k + s->ar_o) +
		  conv(dblen, hlen, s->rr, s->or_ir, k + s->or_o) +
		  common);
	    right   =
		( conv(dblen, hlen, s->rf, s->af_ir, k + s->af_o) +
		  conv(dblen, hlen, s->lf, s->of_ir, k + s->of_o) +
		  conv(dblen, hlen, s->rr, s->ar_ir, k + s->ar_o) +
		  conv(dblen, hlen, s->lr, s->or_ir, k + s->or_o) +
		  common);
	}

	/* Bass compensation for the lower frequency cut of the HRTF.  A
	   cross talk of the left and right channel is introduced to
	   match the directional characteristics of higher frequencies.
	   The bass will not have any real 3D perception, but that is
	   OK. */
	left_b  = conv(dblen, blen, s->ba_l, s->ba_ir, k);
	right_b = conv(dblen, blen, s->ba_r, s->ba_ir, k);
	left  += (1 - BASSCROSS) * left_b  + BASSCROSS * right_b;
	right += (1 - BASSCROSS) * right_b + BASSCROSS * left_b;
	/* Also mix the LFE channel (if available) */
	if(af->data->nch >= 6) {
	    left  += out[5] * M3_01DB;
	    right += out[5] * M3_01DB;
	}

	/* Amplitude renormalization. */
	left  *= AMPLNORM;
	right *= AMPLNORM;

	/* "Cheating": linear stereo expansion to amplify the 3D
	   perception.  Note: Too much will destroy the acoustic space
	   and may even result in headaches. */
	diff = STEXPAND2 * (left - right);
	out[0] = (int16_t)(left  + diff);
	out[1] = (int16_t)(right - diff);

	/* The remaining channels are not needed any more */
	out[2] = out[3] = out[4] = 0;
	if(af->data->nch >= 6)
	    out[5] = 0;

	/* Next sample... */
	in = &in[data->nch];
	out = &out[af->data->nch];
	(s->cyc_pos)--;
	if(s->cyc_pos < 0)
	    s->cyc_pos += dblen;
    }

    /* Set output data */
    data->audio = af->data->audio;
    data->len   = (data->len * af->mul.n) / af->mul.d;
    data->nch   = af->data->nch;

    return data;
}

static int allocate(af_hrtf_t *s)
{
    if ((s->lf = malloc(s->dlbuflen * sizeof(float))) == NULL) return -1;
    if ((s->rf = malloc(s->dlbuflen * sizeof(float))) == NULL) return -1;
    if ((s->lr = malloc(s->dlbuflen * sizeof(float))) == NULL) return -1;
    if ((s->rr = malloc(s->dlbuflen * sizeof(float))) == NULL) return -1;
    if ((s->cf = malloc(s->dlbuflen * sizeof(float))) == NULL) return -1;
    if ((s->cr = malloc(s->dlbuflen * sizeof(float))) == NULL) return -1;
    if ((s->ba_l = malloc(s->dlbuflen * sizeof(float))) == NULL) return -1;
    if ((s->ba_r = malloc(s->dlbuflen * sizeof(float))) == NULL) return -1;
    return 0;
}

/* Allocate memory and set function pointers */
static int open(af_instance_t* af)
{
    int i;
    af_hrtf_t *s;
    float fc;

    af_msg(AF_MSG_INFO,
	   "[hrtf] Head related impulse response (HRIR) derived from KEMAR measurement\n"
	   "[hrtf] data by Bill Gardner <billg@media.mit.edu>\n"
	   "[hrtf] and Keith Martin <kdm@media.mit.edu>.\n"
	   "[hrtf] This data is Copyright 1994 by the MIT Media Laboratory.  It is\n"
	   "[hrtf] provided free with no restrictions on use, provided the authors are\n"
	   "[hrtf] cited when the data is used in any research or commercial application.\n"
	   "[hrtf] URL: http://sound.media.mit.edu/KEMAR.html\n");

    af->control = control;
    af->uninit = uninit;
    af->play = play;
    af->mul.n = 1;
    af->mul.d = 1;
    af->data = calloc(1, sizeof(af_data_t));
    af->setup = calloc(1, sizeof(af_hrtf_t));
    if((af->data == NULL) || (af->setup == NULL))
	return AF_ERROR;

    s = af->setup;

    s->dlbuflen = DELAYBUFLEN;
    s->hrflen = HRTFFILTLEN;
    s->basslen = BASSFILTLEN;

    s->cyc_pos = s->dlbuflen - 1;
    s->matrix_mode = 1;

    if (allocate(s) != 0) {
 	af_msg(AF_MSG_ERROR, "[hrtf] Memory allocation error.\n");
	return AF_ERROR;
    }

    for(i = 0; i < s->dlbuflen; i++)
	s->lf[i] = s->rf[i] = s->lr[i] = s->rr[i] = s->cf[i] =
	    s->cr[i] = 0;

    s->lr_fwr =
	s->rr_fwr = 0;

    s->cf_ir = cf_filt + (s->cf_o = pulse_detect(cf_filt));
    s->af_ir = af_filt + (s->af_o = pulse_detect(af_filt));
    s->of_ir = of_filt + (s->of_o = pulse_detect(of_filt));
    s->ar_ir = ar_filt + (s->ar_o = pulse_detect(ar_filt));
    s->or_ir = or_filt + (s->or_o = pulse_detect(or_filt));
    s->cr_ir = cr_filt + (s->cr_o = pulse_detect(cr_filt));

    if((s->ba_ir = malloc(s->basslen * sizeof(float))) == NULL) {
 	af_msg(AF_MSG_ERROR, "[hrtf] Memory allocation error.\n");
	return AF_ERROR;
    }
    fc = 2.0 * BASSFILTFREQ / (float)af->data->rate;
    if(design_fir(s->basslen, s->ba_ir, &fc, LP | KAISER, 4 * M_PI) ==
       -1) {
	af_msg(AF_MSG_ERROR, "[hrtf] Unable to design low-pass "
	       "filter.\n");
	return AF_ERROR;
    }
    for(i = 0; i < s->basslen; i++)
	s->ba_ir[i] *= BASSGAIN;
    
    return AF_OK;
}

/* Description of this filter */
af_info_t af_info_hrtf = {
    "HRTF Headphone",
    "hrtf",
    "ylai",
    "",
    AF_FLAGS_REENTRANT,
    open
};
