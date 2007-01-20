
typedef struct {
    // video info:
    int mpeg1; // 0=mpeg2  1=mpeg1
    int display_picture_width;
    int display_picture_height;
    int aspect_ratio_information;
    int frame_rate_code;
    float fps;
    int bitrate; // 0x3FFFF==VBR
    // timing:
    int picture_structure;
    int progressive_sequence;
    int repeat_first_field;
    int progressive_frame;
    int top_field_first;
    int display_time; // secs*100
    //the following are for mpeg4
    unsigned int timeinc_resolution, timeinc_bits, timeinc_unit;
    int picture_type;
} mp_mpeg_header_t;

int mp_header_process_sequence_header (mp_mpeg_header_t * picture, unsigned char * buffer);
int mp_header_process_extension (mp_mpeg_header_t * picture, unsigned char * buffer);
float mpeg12_aspect_info(mp_mpeg_header_t *picture);
int mp4_header_process_vol(mp_mpeg_header_t * picture, unsigned char * buffer);
void mp4_header_process_vop(mp_mpeg_header_t * picture, unsigned char * buffer);
int h264_parse_sps(mp_mpeg_header_t * picture, unsigned char * buf, int len);
int mp_vc1_decode_sequence_header(mp_mpeg_header_t * picture, unsigned char * buf, int len);
