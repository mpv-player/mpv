
int cue_read_cue (char *in_cue_filename);
int cue_vcd_seek_to_track (int track);
int cue_vcd_get_track_end (int track);
void cue_vcd_read_toc ();
int cue_vcd_read(char *mem);
inline void cue_set_msf(unsigned int sect);
