/* MP3 Player Library 2.0      (C) 1999 A'rpi/Astral&ESP-team  */

#ifndef MPLAYER_MP3LIB_MP3_H
#define MPLAYER_MP3LIB_MP3_H

/* decoder level: */
#ifdef CONFIG_FAKE_MONO
void MP3_Init(int fakemono);
#else
void MP3_Init(void);
#endif
int MP3_Open(char *filename, int buffsize);
void MP3_SeekFrame(int num, int dir);
void MP3_SeekForward(int num);
int MP3_PrintTAG(void);
int MP3_DecodeFrame(unsigned char *hova, short single);
int MP3_FillBuffers(void);
void MP3_PrintHeader(void);
void MP3_Close(void);
/* public variables: */
extern int MP3_eof;        // set if EOF reached
extern int MP3_pause;      // lock playing
/* informational: */
extern int MP3_filesize;   // filesize
extern int MP3_frames;     // current frame no
extern int MP3_fpos;       // current file pos
extern int MP3_framesize;  // current framesize in bytes (including header)
extern int MP3_bitrate;    // current bitrate (kbits)
extern int MP3_samplerate; // current sampling freq (Hz)
extern int MP3_channels;
extern int MP3_bps;

/* player level: */
int  MP3_OpenDevice(char *devname);  /* devname can be NULL for default) */
void MP3_Play(void);
void MP3_Stop(void);
void MP3_CloseDevice(void);

#endif /* MPLAYER_MP3LIB_MP3_H */
