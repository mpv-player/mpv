#ifndef AVIFILE_DS_VIDEODECODER_H
#define AVIFILE_DS_VIDEODECODER_H

typedef struct _DS_VideoDecoder DS_VideoDecoder;

int DS_VideoDecoder_GetCapabilities(DS_VideoDecoder *this);

DS_VideoDecoder * DS_VideoDecoder_Open(char* dllname, GUID* guid, BITMAPINFOHEADER * format, int flip, int maxauto);

void DS_VideoDecoder_Destroy(DS_VideoDecoder *this);

void DS_VideoDecoder_StartInternal(DS_VideoDecoder *this);

void DS_VideoDecoder_StopInternal(DS_VideoDecoder *this);

int DS_VideoDecoder_DecodeInternal(DS_VideoDecoder *this, const void* src, int size, int is_keyframe, char* pImage);

/*
 * bits == 0   - leave unchanged
 */
//int SetDestFmt(DS_VideoDecoder * this, int bits = 24, fourcc_t csp = 0);
int DS_VideoDecoder_SetDestFmt(DS_VideoDecoder *this, int bits, unsigned int csp);
int DS_VideoDecoder_SetDirection(DS_VideoDecoder *this, int d);
int DS_VideoDecoder_GetValue(DS_VideoDecoder *this, const char* name, int* value);
int DS_VideoDecoder_SetValue(DS_VideoDecoder *this, const char* name, int value);


#endif /* AVIFILE_DS_VIDEODECODER_H */
