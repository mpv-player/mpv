#ifndef _AC3_IEC958_H
#define _AC3_IEC958_H

#define IEC61937_DATA_TYPE_AC3 1

struct hwac3info {
  int bitrate, framesize, samplerate, bsmod;
};

int ac3_iec958_build_burst(int length, int data_type, int big_endian, unsigned char * data, unsigned char * out);
int ac3_iec958_parse_syncinfo(unsigned char *buf, int size, struct hwac3info *ai, int *skipped);

#endif
