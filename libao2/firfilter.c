
static double desired_7kHz_lowpass[] = {1.0, 0.0};
static double weights_7kHz_lowpass[] = {0.1, 0.1};

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

int16_t firfilter(int16_t *buf, int pos, int len, int count, double *coefficients)
{
  double result = 0.0;

  // Back 32 samples, maybe wrapping in buffer.
  pos = (pos+len-count)%len;
  // And do the multiply-accumulate
  while (count--) {
    result += buf[pos++] * *coefficients++;  pos %= len;
  }
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
