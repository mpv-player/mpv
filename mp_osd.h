#ifndef MPLAYER_MP_OSD_H
#define MPLAYER_MP_OSD_H

#define OSD_MSG_TV_CHANNEL              0
#define OSD_MSG_TEXT                    1
#define OSD_MSG_SUB_DELAY               2
#define OSD_MSG_SPEED                   3
#define OSD_MSG_OSD_STATUS              4
#define OSD_MSG_BAR                     5
#define OSD_MSG_PAUSE                   6
#define OSD_MSG_RADIO_CHANNEL           7
/// Base id for messages generated from the commmand to property bridge.
#define OSD_MSG_PROPERTY                0x100

#define MAX_OSD_LEVEL 3
#define MAX_TERM_OSD_LEVEL 1

// These appear in options list
extern int osd_duration;
extern int term_osd;

void set_osd_bar(int type,const char* name,double min,double max,double val);
void set_osd_msg(int id, int level, int time, const char* fmt, ...);
void rm_osd_msg(int id);

#endif /* MPLAYER_MP_OSD_H */
