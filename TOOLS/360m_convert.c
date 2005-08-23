/**
 * convert D-Cinema Video (MPEG2 in GXF, SMPTE 360M) to a
 * MPEG-ES file that MPlayer can play (use -demuxer mpeges).
 * Usage: 360m_convert <infile> <outfile>
 */
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  FILE *in = fopen(argv[1], "r");
  FILE *out = fopen(argv[2], "w");
  int discard = 0;
  unsigned char buf[4];
  if (!in) {
    printf("Could not open %s for reading\n", argv[1]);
    return EXIT_FAILURE;
  }
  if (!out) {
    printf("Could not open %s for writing\n", argv[2]);
    return EXIT_FAILURE;
  }
  fread(buf, 4, 1, in);
  do {
    if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1) {
      // encountered a header
      // skip data between a 0xbf or 0xbc header and the next 0x00 header
      if (buf[3] == 0xbc || buf[3] == 0xbf)
        discard = 1;
      else if (buf[3] == 0)
        discard = 0;
    }
    if (!discard)
      fwrite(&buf[0], 1, 1, out);
    buf[0] = buf[1];
    buf[1] = buf[2];
    buf[2] = buf[3];
    fread(&buf[3], 1, 1, in);
  } while (!feof(in));
  return EXIT_SUCCESS;
}

