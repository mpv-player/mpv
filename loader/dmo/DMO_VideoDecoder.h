#ifndef AVIFILE_DMO_VIDEODECODER_H
#define AVIFILE_DMO_VIDEODECODER_H

typedef struct _DMO_VideoDecoder DMO_VideoDecoder;

int DMO_VideoDecoder_GetCapabilities(DMO_VideoDecoder *this);

DMO_VideoDecoder * DMO_VideoDecoder_Open(char* dllname, GUID* guid, BITMAPINFOHEADER * format, int flip, int maxauto);

void DMO_VideoDecoder_Destroy(DMO_VideoDecoder *this);

void DMO_VideoDecoder_StartInternal(DMO_VideoDecoder *this);

void DMO_VideoDecoder_StopInternal(DMO_VideoDecoder *this);

int DMO_VideoDecoder_DecodeInternal(DMO_VideoDecoder *this, const void* src, int size, int is_keyframe, char* pImage);

/*
 * bits == 0   - leave unchanged
 */
//int SetDestFmt(DMO_VideoDecoder * this, int bits = 24, fourcc_t csp = 0);
int DMO_VideoDecoder_SetDestFmt(DMO_VideoDecoder *this, int bits, unsigned int csp);
int DMO_VideoDecoder_SetDirection(DMO_VideoDecoder *this, int d);


#endif /* AVIFILE_DMO_VIDEODECODER_H */
