
extern font_desc_t* vo_font;

extern unsigned char* vo_osd_text;

extern int vo_osd_progbar_type;
extern int vo_osd_progbar_value;   // 0..255

extern int vo_sub_lines;
extern unsigned char* vo_sub_text[8];

#define OSD_PLAY 0x01
#define OSD_PAUSE 0x02
#define OSD_STOP 0x03
#define OSD_REW 0x04
#define OSD_FFW 0x05
#define OSD_CLOCK 0x06
#define OSD_CONTRAST 0x07
#define OSD_SATURATION 0x08
#define OSD_VOLUME 0x09
#define OSD_BRIGHTNESS 0x0A
#define OSD_HUE 0x0B

#define OSD_PB_START 0x10
#define OSD_PB_0 0x11
#define OSD_PB_END 0x12
#define OSD_PB_1 0x13


