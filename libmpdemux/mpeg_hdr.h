
typedef struct {
    // video info:
    int mpeg1; // 0=mpeg2  1=mpeg1
    int display_picture_width;
    int display_picture_height;
    int aspect_ratio_information;
    int frame_rate_code;
    int fps; // fps*10000
    int bitrate; // 0x3FFFF==VBR
    // timing:
    int picture_structure;
    int progressive_sequence;
    int repeat_first_field;
    int progressive_frame;
    int top_field_first;
    int display_time; // secs*100
} mp_mpeg_header_t;

int mp_header_process_sequence_header (mp_mpeg_header_t * picture, unsigned char * buffer);
int mp_header_process_extension (mp_mpeg_header_t * picture, unsigned char * buffer);
