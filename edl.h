// EDL version 0.5
// Author: Michael Halcrow <mhalcrow@byu.edu>

#ifndef EDLH
#define EDLH

#define EDL_SKIP 0
#define EDL_MUTE 1

#define MAX_EDL_ENTRIES 1000

struct edl_record {
  float start_sec;
  long start_frame;
  float stop_sec;
  long stop_frame;
  float length_sec;
  long length_frame;
  short action;
  struct edl_record* next;
};

typedef struct edl_record* edl_record_ptr;

#endif
