// EDL version 0.6

#ifndef EDLH
#define EDLH

#define EDL_SKIP 0
#define EDL_MUTE 1
#define EDL_ERROR -1
#define EDL_MUTE_START 1
#define EDL_MUTE_END 0

struct edl_record {
  float start_sec;
  long start_frame;
  float stop_sec;
  long stop_frame;
  float length_sec;
  long length_frame;
  short action;
  short mute_state;
  struct edl_record* next;
};

typedef struct edl_record* edl_record_ptr;

char *edl_filename; // file to extract EDL entries from (-edl)
char *edl_output_filename; // file to put EDL entries in (-edlout)

int edl_check_mode(void); // we cannot do -edl and -edlout at the same time
int edl_count_entries(void); // returns total number of entries needed
int edl_parse_file(edl_record_ptr edl_records); // fills EDL stack

#endif
