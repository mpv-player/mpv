#include <stdio.h>
#include <unistd.h>
#include <iconv.h>
#define INBUFSIZE 1024
#define OUTBUFSIZE 4096

char inbuffer[INBUFSIZE];
char outbuffer[OUTBUFSIZE];

int main(void) {
  size_t numread;
  iconv_t icdsc;
  char *tocode="UTF-8";
  char *fromcode="cp1250";
  if ((icdsc = iconv_open(tocode, fromcode)) != (iconv_t)(-1)) {
    while ((numread = read(0, inbuffer, INBUFSIZE))) {
      char *iptr=inbuffer;
      char *optr=outbuffer;
      size_t inleft=numread;
      size_t outleft=OUTBUFSIZE;
      if (iconv(icdsc, &iptr, &inleft, &optr, &outleft)
          != (size_t)(-1)) {
        write(1, outbuffer, OUTBUFSIZE - outleft);
      }
    }
    if (iconv_close(icdsc) == -1)
      ;
  }
  return 0;
}
