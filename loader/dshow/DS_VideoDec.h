/********************************************************

	DirectShow Video decoder implementation
	Copyright 2000 Eugene Kuznetsov  (divx@euro.ru)
        Converted  C++ --> C  :) by A'rpi/ESP-team

*********************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

void* DS_VideoDecoder_Open(char* dllname, GUID* guid, BITMAPINFOHEADER* format, int flip, int maxauto);

void DS_VideoDecoder_StartInternal(void* _handle);

void DS_VideoDecoder_Stop(void* _handle);

void DS_VideoDecoder_Destroy(void* _handle);

int DS_VideoDecoder_DecodeInternal(void* _handle, char* src, int size, int is_keyframe, char* dest);

int DS_VideoDecoder_SetDestFmt(void* _handle, int bits, int csp);

int DS_VideoDecoder_SetValue(void* _handle, char* name, int value);
int DS_SetAttr_DivX(char* attribute, int value);

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */
