// EDL version 0.6

#ifndef MPLAYER_EDL_H
#define MPLAYER_EDL_H

#define EDL_SKIP 0
#define EDL_MUTE 1

#define EDL_MUTE_START 1
#define EDL_MUTE_END 0

struct edl_record {
  float start_sec;
  float stop_sec;
  float length_sec;
  short action;
  struct edl_record* next;
  struct edl_record* prev;
};

typedef struct edl_record* edl_record_ptr;

extern char *edl_filename; // file to extract EDL entries from (-edl)
extern char *edl_output_filename; // file to put EDL entries in (-edlout)

void free_edl(edl_record_ptr next_edl_record); // free's entire EDL list.
edl_record_ptr edl_parse_file(void); // fills EDL stack

#endif /* MPLAYER_EDL_H */
