#include <inttypes.h>
#include <math.h>

static double desired_7kHz_lowpass[] = {1.0, 0.0};
static double weights_7kHz_lowpass[] = {0.2, 2.0};

double *calc_coefficients_7kHz_lowpass(int rate)
{
  double *result = (double *)malloc(32*sizeof(double));
  double bands[4];

  bands[0] = 0.0;  bands[1] = 6800.0/rate;
  bands[2] = 8500.0/rate;  bands[3] = 0.5;

  remez(result, 32, 2, bands,
	desired_7kHz_lowpass, weights_7kHz_lowpass, BANDPASS);

  return result;
}

#if 0

static double desired_125Hz_lowpass[] = {1.0, 0.0};
static double weights_125Hz_lowpass[] = {0.2, 2.0};

double *calc_coefficients_125Hz_lowpass(int rate)
{
  double *result = (double *)malloc(256*sizeof(double));
  double bands[4];

  bands[0] = 0.0;  bands[1] = 125.0/rate;
  bands[2] = 175.0/rate;  bands[3] = 0.5;

  remez(result, 256, 2, bands,
	desired_125Hz_lowpass, weights_125Hz_lowpass, BANDPASS);

  return result;
}

#endif

int16_t firfilter(int16_t *buf, int pos, int len, int count, double *coefficients)
{
  double result = 0.0;
  int count1, count2;
  int16_t *ptr;

  if (pos >= count) {
    pos -= count;
    count1 = count; count2 = 0;
  }
  else {
    count2 = pos;
    count1 = count - pos;
    pos = len - count1;
  }
  //fprintf(stderr, "pos=%d, count1=%d, count2=%d\n", pos, count1, count2);

  // high part of window
  ptr = &buf[pos];
  while (count1--)  result += *ptr++ * *coefficients++;
  // wrapped part of window
  while (count2--)  result += *buf++ * *coefficients++;
  return result;
}

void dump_filter_coefficients(double *coefficients)
{
  int i;
  fprintf(stderr, "pl_surround: Filter coefficients are: \n");
  for (i=0; (i<32); i++) {
    fprintf(stderr, "  [%2d]: %23.20f\n", i, coefficients[i]);
  }
}

#ifdef TESTING

#define PI 3.1415926536

// For testing purposes, fill a buffer with some sine-wave tone
void sinewave(int16_t *output, int samples, int incr, int freq, double phase, int samplerate)
{
  double radians_per_sample = 2*PI / ((0.0+samplerate) / freq), r;

  //fprintf(stderr, "samples=%d tone freq=%d, samplerate=%d, radians/sample=%f\n",
  //	  samples, freq, samplerate, radians_per_sample);
  r = phase;
  while (samples--) {
    *output = sin(r)*10000;  output = &output[incr];
    r += radians_per_sample;
  }
}

// Pass various frequencies through a FIR filter and report amplitudes
void testfilter(double *coefficients, int count, int samplerate)
{
  int16_t wavein[8192]; //, waveout[2048];
  int sample, samples, maxsample, minsample, totsample;
  int nyquist=samplerate/2;
  int freq, i;

  for (freq=25; freq<nyquist; freq+=25) {
    // Make input tone
    sinewave(wavein, 8192, 1, freq, 0.0, samplerate);
    //for (i=0; i<32; i++)
    //  fprintf(stderr, "%5d\n", wavein[i]);
    // Filter through the filter, measure results
    maxsample=0;  minsample=1000000;  totsample=0;  samples=0;
    for (i=2048; i<8192; i++) {
      //waveout[i] = wavein[i];
      sample = abs(firfilter(wavein, i, 8192, count, coefficients));
      if (sample > maxsample) maxsample=sample;
      if (sample < minsample) minsample=sample;
      totsample += sample;  samples++;
    }
    // Report results
    fprintf(stderr, "%5d  %5d  %5d  %5d  %f\n", freq, totsample/samples, maxsample, minsample, 10*log((totsample/samples)/6500.0));
  }
}

#endif
