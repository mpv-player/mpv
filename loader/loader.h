/********************************************************

	Win32 binary loader interface
	Copyright 2000 Eugene Kuznetsov (divx@euro.ru)
	Shamelessly stolen from Wine project

*********************************************************/

#ifndef _LOADER_H
#define _LOADER_H
#include <wine/windef.h>
#include <wine/driver.h>
#include <wine/mmreg.h>
#include <wine/vfw.h>
#include <wine/msacm.h>
#ifdef __cplusplus
extern "C" {
#endif

extern char* win32_codec_name;  // must be set before calling DrvOpen() !!!

unsigned int _GetPrivateProfileIntA(const char* appname, const char* keyname, int default_value, const char* filename);
int _GetPrivateProfileStringA(const char* appname, const char* keyname,
	const char* def_val, char* dest, unsigned int len, const char* filename);
int _WritePrivateProfileStringA(const char* appname, const char* keyname,
	const char* string, const char* filename);


/**********************************************

    MS VFW ( Video For Windows ) interface
	    
**********************************************/	    

long VFWAPIV ICCompress(
	HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiOutput,void* lpData,
	LPBITMAPINFOHEADER lpbiInput,void* lpBits,long* lpckid,
	long* lpdwFlags,long lFrameNum,long dwFrameSize,long dwQuality,
	LPBITMAPINFOHEADER lpbiPrev,void* lpPrev
);

long VFWAPIV ICDecompress(HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiFormat,void* lpData,LPBITMAPINFOHEADER lpbi,void* lpBits);

WIN_BOOL	VFWAPI	ICInfo(long fccType, long fccHandler, ICINFO * lpicinfo);
LRESULT	VFWAPI	ICGetInfo(HIC hic,ICINFO *picinfo, long cb);
HIC	VFWAPI	ICOpen(long fccType, long fccHandler, UINT wMode);
HIC	VFWAPI	ICOpenFunction(long fccType, long fccHandler, unsigned int wMode, void* lpfnHandler);

LRESULT VFWAPI ICClose(HIC hic);
LRESULT	VFWAPI ICSendMessage(HIC hic, unsigned int msg, long dw1, long dw2);
HIC	VFWAPI ICLocate(long fccType, long fccHandler, LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut, short wFlags);

int VFWAPI ICDoSomething();

#define ICCompressGetFormat(hic, lpbiInput, lpbiOutput) 		\
	ICSendMessage(							\
	    hic,ICM_COMPRESS_GET_FORMAT,(long)(void*)(lpbiInput),	\
	    (long)(void*)(lpbiOutput)					\
	)

#define ICCompressGetFormatSize(hic,lpbi) ICCompressGetFormat(hic,lpbi,NULL)

#define ICGetDefaultKeyFrameRate(hic,lpint) 		\
	ICSendMessage(					\
	    hic, ICM_GETDEFAULTKEYFRAMERATE,		\
	    (long)(void*)(lpint), 			\
	    0	)		

#define ICGetDefaultQuality(hic,lpint) 			\
	ICSendMessage(					\
	    hic, ICM_GETDEFAULTQUALITY,			\
	    (long)(void*)(lpint), 			\
	    0	)		
	    	

#define ICCompressBegin(hic, lpbiInput, lpbiOutput) 			\
    ICSendMessage(							\
    	hic, ICM_COMPRESS_BEGIN, (long)(void*)(lpbiInput),		\
	(long)(void*)(lpbiOutput)					\
    )

#define ICCompressGetSize(hic, lpbiInput, lpbiOutput) 		\
    ICSendMessage(							\
    	hic, ICM_COMPRESS_GET_SIZE, (long)(void*)(lpbiInput), 	\
	(long)(void*)(lpbiOutput)					\
    )

#define ICCompressQuery(hic, lpbiInput, lpbiOutput)		\
    ICSendMessage(						\
    	hic, ICM_COMPRESS_QUERY, (long)(void*)(lpbiInput),	\
	(long)(void*)(lpbiOutput)				\
    )


#define ICCompressEnd(hic) ICSendMessage(hic, ICM_COMPRESS_END, 0, 0)



#define ICDecompressBegin(hic, lpbiInput, lpbiOutput) 	\
    ICSendMessage(						\
    	hic, ICM_DECOMPRESS_BEGIN, (long)(void*)(lpbiInput),	\
	(long)(void*)(lpbiOutput)				\
    )

#define ICDecompressQuery(hic, lpbiInput, lpbiOutput) 	\
    ICSendMessage(						\
    	hic,ICM_DECOMPRESS_QUERY, (long)(void*)(lpbiInput),	\
	(long) (void*)(lpbiOutput)				\
    )

#define ICDecompressGetFormat(hic, lpbiInput, lpbiOutput)		\
    ((long)ICSendMessage(						\
    	hic,ICM_DECOMPRESS_GET_FORMAT, (long)(void*)(lpbiInput),	\
	(long)(void*)(lpbiOutput)					\
    ))

#define ICDecompressGetFormatSize(hic, lpbi) 				\
	ICDecompressGetFormat(hic, lpbi, NULL)

#define ICDecompressGetPalette(hic, lpbiInput, lpbiOutput)		\
    ICSendMessage(							\
    	hic, ICM_DECOMPRESS_GET_PALETTE, (long)(void*)(lpbiInput), 	\
	(long)(void*)(lpbiOutput)					\
    )

#define ICDecompressSetPalette(hic,lpbiPalette)	\
        ICSendMessage(				\
		hic,ICM_DECOMPRESS_SET_PALETTE,		\
		(long)(void*)(lpbiPalette),0		\
	)

#define ICDecompressEnd(hic) ICSendMessage(hic, ICM_DECOMPRESS_END, 0, 0)


/*****************************************************

    MS ACM ( Audio Compression Manager ) interface
	    
******************************************************/	    


MMRESULT WINAPI acmDriverAddA(
  PHACMDRIVERID phadid, HINSTANCE hinstModule,
  LPARAM lParam, DWORD dwPriority, DWORD fdwAdd
);
MMRESULT WINAPI acmDriverAddW(
  PHACMDRIVERID phadid, HINSTANCE hinstModule,
  LPARAM lParam, DWORD dwPriority, DWORD fdwAdd
);
MMRESULT WINAPI acmDriverClose(
  HACMDRIVER had, DWORD fdwClose
);
MMRESULT WINAPI acmDriverDetailsA(
  HACMDRIVERID hadid, PACMDRIVERDETAILSA padd, DWORD fdwDetails
);
MMRESULT WINAPI acmDriverDetailsW(
  HACMDRIVERID hadid, PACMDRIVERDETAILSW padd, DWORD fdwDetails
);
MMRESULT WINAPI acmDriverEnum(
  ACMDRIVERENUMCB fnCallback, DWORD dwInstance, DWORD fdwEnum
);
MMRESULT WINAPI acmDriverID(
  HACMOBJ hao, PHACMDRIVERID phadid, DWORD fdwDriverID
);
LRESULT WINAPI acmDriverMessage(
  HACMDRIVER had, UINT uMsg, LPARAM lParam1, LPARAM lParam2
);
MMRESULT WINAPI acmDriverOpen(
  PHACMDRIVER phad, HACMDRIVERID hadid, DWORD fdwOpen
);
MMRESULT WINAPI acmDriverPriority(
  HACMDRIVERID hadid, DWORD dwPriority, DWORD fdwPriority
);
MMRESULT WINAPI acmDriverRemove(
  HACMDRIVERID hadid, DWORD fdwRemove
);
MMRESULT WINAPI acmFilterChooseA(
  PACMFILTERCHOOSEA pafltrc
);
MMRESULT WINAPI acmFilterChooseW(
  PACMFILTERCHOOSEW pafltrc
);
MMRESULT WINAPI acmFilterDetailsA(
  HACMDRIVER had, PACMFILTERDETAILSA pafd, DWORD fdwDetails
);
MMRESULT WINAPI acmFilterDetailsW(
  HACMDRIVER had, PACMFILTERDETAILSW pafd, DWORD fdwDetails
);
MMRESULT WINAPI acmFilterEnumA(
  HACMDRIVER had, PACMFILTERDETAILSA pafd, 
  ACMFILTERENUMCBA fnCallback, DWORD dwInstance, DWORD fdwEnum
);
MMRESULT WINAPI acmFilterEnumW(
  HACMDRIVER had, PACMFILTERDETAILSW pafd, 
  ACMFILTERENUMCBW fnCallback, DWORD dwInstance, DWORD fdwEnum
);
MMRESULT WINAPI acmFilterTagDetailsA(
  HACMDRIVER had, PACMFILTERTAGDETAILSA paftd, DWORD fdwDetails
);
MMRESULT WINAPI acmFilterTagDetailsW(
  HACMDRIVER had, PACMFILTERTAGDETAILSW paftd, DWORD fdwDetails
);
MMRESULT WINAPI acmFilterTagEnumA(
  HACMDRIVER had, PACMFILTERTAGDETAILSA paftd,
  ACMFILTERTAGENUMCBA fnCallback, DWORD dwInstance, DWORD fdwEnum
);
MMRESULT WINAPI acmFilterTagEnumW(
  HACMDRIVER had, PACMFILTERTAGDETAILSW paftd,
  ACMFILTERTAGENUMCBW fnCallback, DWORD dwInstance, DWORD fdwEnum
);
MMRESULT WINAPI acmFormatChooseA(
  PACMFORMATCHOOSEA pafmtc
);
MMRESULT WINAPI acmFormatChooseW(
  PACMFORMATCHOOSEW pafmtc
);
MMRESULT WINAPI acmFormatDetailsA(
  HACMDRIVER had, PACMFORMATDETAILSA pafd, DWORD fdwDetails
);
MMRESULT WINAPI acmFormatDetailsW(
  HACMDRIVER had, PACMFORMATDETAILSW pafd, DWORD fdwDetails
);
MMRESULT WINAPI acmFormatEnumA(
  HACMDRIVER had, PACMFORMATDETAILSA pafd,
  ACMFORMATENUMCBA fnCallback, DWORD dwInstance, DWORD fdwEnum
);
MMRESULT WINAPI acmFormatEnumW(
  HACMDRIVER had, PACMFORMATDETAILSW pafd,
  ACMFORMATENUMCBW fnCallback, DWORD dwInstance,  DWORD fdwEnum
);
MMRESULT WINAPI acmFormatSuggest(
  HACMDRIVER had, PWAVEFORMATEX pwfxSrc, PWAVEFORMATEX pwfxDst,
  DWORD cbwfxDst, DWORD fdwSuggest
);
MMRESULT WINAPI acmFormatTagDetailsA(
  HACMDRIVER had, PACMFORMATTAGDETAILSA paftd, DWORD fdwDetails
);
MMRESULT WINAPI acmFormatTagDetailsW(
  HACMDRIVER had, PACMFORMATTAGDETAILSW paftd, DWORD fdwDetails
);
MMRESULT WINAPI acmFormatTagEnumA(
  HACMDRIVER had, PACMFORMATTAGDETAILSA paftd,
  ACMFORMATTAGENUMCBA fnCallback, DWORD dwInstance, DWORD fdwEnum
);
MMRESULT WINAPI acmFormatTagEnumW(
  HACMDRIVER had, PACMFORMATTAGDETAILSW paftd,
  ACMFORMATTAGENUMCBW fnCallback, DWORD dwInstance, DWORD fdwEnum
);
DWORD WINAPI acmGetVersion(
);
MMRESULT WINAPI acmMetrics(
  HACMOBJ hao, UINT  uMetric, LPVOID  pMetric
);
MMRESULT WINAPI acmStreamClose(
  HACMSTREAM has, DWORD fdwClose
);
MMRESULT WINAPI acmStreamConvert(
  HACMSTREAM has, PACMSTREAMHEADER pash, DWORD fdwConvert
);
MMRESULT WINAPI acmStreamMessage(
  HACMSTREAM has, UINT uMsg, LPARAM lParam1, LPARAM lParam2
);
MMRESULT WINAPI acmStreamOpen(
  PHACMSTREAM phas, HACMDRIVER had, PWAVEFORMATEX pwfxSrc,
  PWAVEFORMATEX pwfxDst, PWAVEFILTER pwfltr, DWORD dwCallback,
  DWORD dwInstance, DWORD fdwOpen
);
MMRESULT WINAPI acmStreamPrepareHeader(
  HACMSTREAM has, PACMSTREAMHEADER pash, DWORD fdwPrepare
);
MMRESULT WINAPI acmStreamReset(
  HACMSTREAM has, DWORD fdwReset
);
MMRESULT WINAPI acmStreamSize(
  HACMSTREAM has, DWORD cbInput, 
  LPDWORD pdwOutputBytes, DWORD fdwSize
);
MMRESULT WINAPI acmStreamUnprepareHeader(
  HACMSTREAM has, PACMSTREAMHEADER pash, DWORD fdwUnprepare
);
void MSACM_RegisterAllDrivers(void);

INT WINAPI LoadStringA( HINSTANCE instance, UINT resource_id,
                            LPSTR buffer, INT buflen );

#ifdef __cplusplus
}
#endif
#endif /* __LOADER_H */

