/********************************************************

	DirectShow Video decoder implementation
	Copyright 2000 Eugene Kuznetsov  (divx@euro.ru)
        Converted  C++ --> C  :) by A'rpi/ESP-team

*********************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

int DS_VideoDecoder_Open(char* dllname, GUID* guid, BITMAPINFOHEADER* format, int flip,char** d_ptr);

void DS_VideoDecoder_Start();

void DS_VideoDecoder_Stop();

void DS_VideoDecoder_Restart();

void DS_VideoDecoder_Close();

int DS_VideoDecoder_DecodeFrame(char* src, int size, int is_keyframe, int render);

int DS_VideoDecoder_SetDestFmt(int bits, int csp);

int DS_SetValue_DivX(char* name, int value);

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */
