/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#ifndef MPLAYER_VFW_H
#define MPLAYER_VFW_H
//#include "pshpack1.h"
#include "windef.h"

typedef struct __attribute__((__packed__))
{
    short    bfType;
    long   bfSize;
    short    bfReserved1;
    short    bfReserved2;
    long   bfOffBits;
} BITMAPFILEHEADER;

#ifndef _BITMAPINFOHEADER_
#define _BITMAPINFOHEADER_
typedef struct __attribute__((__packed__))
{
    long 	biSize;
    long  	biWidth;
    long  	biHeight;
    short 	biPlanes;
    short 	biBitCount;
    long 	biCompression;
    long 	biSizeImage;
    long  	biXPelsPerMeter;
    long  	biYPelsPerMeter;
    long 	biClrUsed;
    long 	biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct {
	BITMAPINFOHEADER bmiHeader;
	int	bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;
#endif

#define VFWAPI
#define VFWAPIV
#ifndef MPLAYER_WINDEF_H
typedef long (__stdcall__ *DRIVERPROC)(long,HDRVR,unsigned int,long,long);
#endif



#ifndef mmioFOURCC
#define mmioFOURCC( ch0, ch1, ch2, ch3 )				\
	( (long)(unsigned char)(ch0) | ( (long)(unsigned char)(ch1) << 8 ) |		\
	( (long)(unsigned char)(ch2) << 16 ) | ( (long)(unsigned char)(ch3) << 24 ) )
#endif

#ifndef aviTWOCC
#define aviTWOCC(ch0, ch1) ((short)(unsigned char)(ch0) | ((short)(unsigned char)(ch1) << 8))
#endif

#define ICTYPE_VIDEO	mmioFOURCC('v', 'i', 'd', 'c')
#define ICTYPE_AUDIO	mmioFOURCC('a', 'u', 'd', 'c')


/* Installable Compressor M? */

/* HIC struct (same layout as Win95 one) */
typedef struct tagWINE_HIC {
	long		magic;		/* 00: 'Smag' */
	HANDLE	curthread;	/* 04: */
	long		type;		/* 08: */
	long		handler;	/* 0C: */
	HDRVR		hdrv;		/* 10: */
	long		driverid;	/* 14:(handled by SendDriverMessage)*/
	DRIVERPROC	driverproc;	/* 18:(handled by SendDriverMessage)*/
	long		x1;		/* 1c: name? */
	short		x2;		/* 20: */
	long		x3;		/* 22: */
					/* 26: */
} WINE_HIC;

/* error return codes */
#define	ICERR_OK		0
#define	ICERR_DONTDRAW		1
#define	ICERR_NEWPALETTE	2
#define	ICERR_GOTOKEYFRAME	3
#define	ICERR_STOPDRAWING	4

#define	ICERR_UNSUPPORTED	-1
#define	ICERR_BADFORMAT		-2
#define	ICERR_MEMORY		-3
#define	ICERR_INTERNAL		-4
#define	ICERR_BADFLAGS		-5
#define	ICERR_BADPARAM		-6
#define	ICERR_BADSIZE		-7
#define	ICERR_BADHANDLE		-8
#define	ICERR_CANTUPDATE	-9
#define	ICERR_ABORT		-10
#define	ICERR_ERROR		-100
#define	ICERR_BADBITDEPTH	-200
#define	ICERR_BADIMAGESIZE	-201

#define	ICERR_CUSTOM		-400

/* ICM Messages */
#define	ICM_USER		(DRV_USER+0x0000)

/* ICM driver message range */
#define	ICM_RESERVED_LOW	(DRV_USER+0x1000)
#define	ICM_RESERVED_HIGH	(DRV_USER+0x2000)
#define	ICM_RESERVED		ICM_RESERVED_LOW

#define	ICM_GETSTATE		(ICM_RESERVED+0)
#define	ICM_SETSTATE		(ICM_RESERVED+1)
#define	ICM_GETINFO		(ICM_RESERVED+2)

#define	ICM_CONFIGURE		(ICM_RESERVED+10)
#define	ICM_ABOUT		(ICM_RESERVED+11)
/* */

#define	ICM_GETDEFAULTQUALITY	(ICM_RESERVED+30)
#define	ICM_GETQUALITY		(ICM_RESERVED+31)
#define	ICM_SETQUALITY		(ICM_RESERVED+32)

#define	ICM_SET			(ICM_RESERVED+40)
#define	ICM_GET			(ICM_RESERVED+41)

/* 2 constant FOURCC codes */
#define ICM_FRAMERATE		mmioFOURCC('F','r','m','R')
#define ICM_KEYFRAMERATE	mmioFOURCC('K','e','y','R')

#define	ICM_COMPRESS_GET_FORMAT		(ICM_USER+4)
#define	ICM_COMPRESS_GET_SIZE		(ICM_USER+5)
#define	ICM_COMPRESS_QUERY		(ICM_USER+6)
#define	ICM_COMPRESS_BEGIN		(ICM_USER+7)
#define	ICM_COMPRESS			(ICM_USER+8)
#define	ICM_COMPRESS_END		(ICM_USER+9)

#define	ICM_DECOMPRESS_GET_FORMAT	(ICM_USER+10)
#define	ICM_DECOMPRESS_QUERY		(ICM_USER+11)
#define	ICM_DECOMPRESS_BEGIN		(ICM_USER+12)
#define	ICM_DECOMPRESS			(ICM_USER+13)
#define	ICM_DECOMPRESS_END		(ICM_USER+14)
#define	ICM_DECOMPRESS_SET_PALETTE	(ICM_USER+29)
#define	ICM_DECOMPRESS_GET_PALETTE	(ICM_USER+30)

#define	ICM_DRAW_QUERY			(ICM_USER+31)
#define	ICM_DRAW_BEGIN			(ICM_USER+15)
#define	ICM_DRAW_GET_PALETTE		(ICM_USER+16)
#define	ICM_DRAW_START			(ICM_USER+18)
#define	ICM_DRAW_STOP			(ICM_USER+19)
#define	ICM_DRAW_END			(ICM_USER+21)
#define	ICM_DRAW_GETTIME		(ICM_USER+32)
#define	ICM_DRAW			(ICM_USER+33)
#define	ICM_DRAW_WINDOW			(ICM_USER+34)
#define	ICM_DRAW_SETTIME		(ICM_USER+35)
#define	ICM_DRAW_REALIZE		(ICM_USER+36)
#define	ICM_DRAW_FLUSH			(ICM_USER+37)
#define	ICM_DRAW_RENDERBUFFER		(ICM_USER+38)

#define	ICM_DRAW_START_PLAY		(ICM_USER+39)
#define	ICM_DRAW_STOP_PLAY		(ICM_USER+40)

#define	ICM_DRAW_SUGGESTFORMAT		(ICM_USER+50)
#define	ICM_DRAW_CHANGEPALETTE		(ICM_USER+51)

#define	ICM_GETBUFFERSWANTED		(ICM_USER+41)

#define	ICM_GETDEFAULTKEYFRAMERATE	(ICM_USER+42)

#define	ICM_DECOMPRESSEX_BEGIN		(ICM_USER+60)
#define	ICM_DECOMPRESSEX_QUERY		(ICM_USER+61)
#define	ICM_DECOMPRESSEX		(ICM_USER+62)
#define	ICM_DECOMPRESSEX_END		(ICM_USER+63)

#define	ICM_COMPRESS_FRAMES_INFO	(ICM_USER+70)
#define	ICM_SET_STATUS_PROC		(ICM_USER+72)

/* structs */

typedef struct {
	long	dwSize;		/* 00: size */
	long	fccType;	/* 04: type 'vidc' usually */
	long	fccHandler;	/* 08: */
	long	dwVersion;	/* 0c: version of compman opening you */
	long	dwFlags;	/* 10: LOshort is type specific */
	LRESULT	dwError;	/* 14: */
	void*	pV1Reserved;	/* 18: */
	void*	pV2Reserved;	/* 1c: */
	long	dnDevNode;	/* 20: */
				/* 24: */
} ICOPEN,*LPICOPEN;

#define ICCOMPRESS_KEYFRAME     0x00000001L

typedef struct {
    long		dwFlags;
    LPBITMAPINFOHEADER	lpbiOutput;
    void*		lpOutput;
    LPBITMAPINFOHEADER	lpbiInput;
    const void*		lpInput;
    long*		lpckid;
    long*		lpdwFlags;
    long		lFrameNum;
    long		dwFrameSize;
    long		dwQuality;
    LPBITMAPINFOHEADER	lpbiPrev;
    void*		lpPrev;
} ICCOMPRESS;


long VFWAPI VideoForWindowsVersion(void);

long VFWAPIV ICCompress(
	HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiOutput,void* lpData,
	LPBITMAPINFOHEADER lpbiInput,void* lpBits,long* lpckid,
	long* lpdwFlags,long lFrameNum,long dwFrameSize,long dwQuality,
	LPBITMAPINFOHEADER lpbiPrev,void* lpPrev
);


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

/* ICCOMPRESSFRAMES.dwFlags */
#define ICCOMPRESSFRAMES_PADDING        0x00000001
typedef struct {
    long               dwFlags;
    LPBITMAPINFOHEADER  lpbiOutput;
    LPARAM              lOutput;
    LPBITMAPINFOHEADER  lpbiInput;
    LPARAM              lInput;
    long                lStartFrame;
    long                lFrameCount;
    long                lQuality;
    long                lDataRate;
    long                lKeyRate;
    long               dwRate;
    long               dwScale;
    long               dwOverheadPerFrame;
    long               dwReserved2;
    long CALLBACK (*GetData)(LPARAM lInput,long lFrame,void* lpBits,long len);
    long CALLBACK (*PutData)(LPARAM lOutput,long lFrame,void* lpBits,long len);
} ICCOMPRESSFRAMES;

/* Values for wMode of ICOpen() */
#define	ICMODE_COMPRESS		1
#define	ICMODE_DECOMPRESS	2
#define	ICMODE_FASTDECOMPRESS	3
#define	ICMODE_QUERY		4
#define	ICMODE_FASTCOMPRESS	5
#define	ICMODE_DRAW		8

/* quality flags */
#define ICQUALITY_LOW       0
#define ICQUALITY_HIGH      10000
#define ICQUALITY_DEFAULT   -1

typedef struct {
	long	dwSize;		/* 00: */
	long	fccType;	/* 04:compressor type     'vidc' 'audc' */
	long	fccHandler;	/* 08:compressor sub-type 'rle ' 'jpeg' 'pcm '*/
	long	dwFlags;	/* 0c:flags LOshort is type specific */
	long	dwVersion;	/* 10:version of the driver */
	long	dwVersionICM;	/* 14:version of the ICM used */
	/*
	 * under Win32, the driver always returns UNICODE strings.
	 */
	WCHAR	szName[16];		/* 18:short name */
	WCHAR	szDescription[128];	/* 38:long name */
	WCHAR	szDriver[128];		/* 138:driver that contains compressor*/
					/* 238: */
} ICINFO;

/* ICINFO.dwFlags */
#define	VIDCF_QUALITY		0x0001  /* supports quality */
#define	VIDCF_CRUNCH		0x0002  /* supports crunching to a frame size */
#define	VIDCF_TEMPORAL		0x0004  /* supports inter-frame compress */
#define	VIDCF_COMPRESSFRAMES	0x0008  /* wants the compress all frames message */
#define	VIDCF_DRAW		0x0010  /* supports drawing */
#define	VIDCF_FASTTEMPORALC	0x0020  /* does not need prev frame on compress */
#define	VIDCF_FASTTEMPORALD	0x0080  /* does not need prev frame on decompress */
#define	VIDCF_QUALITYTIME	0x0040  /* supports temporal quality */

#define	VIDCF_FASTTEMPORAL	(VIDCF_FASTTEMPORALC|VIDCF_FASTTEMPORALD)


/* function shortcuts */
/* ICM_ABOUT */
#define ICMF_ABOUT_QUERY         0x00000001

#define ICQueryAbout(hic) \
	(ICSendMessage(hic,ICM_ABOUT,(long)-1,ICMF_ABOUT_QUERY)==ICERR_OK)

#define ICAbout(hic, hwnd) ICSendMessage(hic,ICM_ABOUT,(long)(unsigned int)(hwnd),0)

/* ICM_CONFIGURE */
#define ICMF_CONFIGURE_QUERY	0x00000001
#define ICQueryConfigure(hic) \
	(ICSendMessage(hic,ICM_CONFIGURE,(long)-1,ICMF_CONFIGURE_QUERY)==ICERR_OK)

#define ICConfigure(hic,hwnd) \
	ICSendMessage(hic,ICM_CONFIGURE,(long)(unsigned int)(hwnd),0)

/* Decompression stuff */
#define ICDECOMPRESS_HURRYUP		0x80000000	/* don't draw just buffer (hurry up!) */
#define ICDECOMPRESS_UPDATE		0x40000000	/* don't draw just update screen */
#define ICDECOMPRESS_PREROL		0x20000000	/* this frame is before real start */
#define ICDECOMPRESS_NULLFRAME		0x10000000	/* repeat last frame */
#define ICDECOMPRESS_NOTKEYFRAME	0x08000000	/* this frame is not a key frame */

typedef struct {
    long		dwFlags;	/* flags (from AVI index...) */
    LPBITMAPINFOHEADER	lpbiInput;	/* BITMAPINFO of compressed data */
    const void*		lpInput;	/* compressed data */
    LPBITMAPINFOHEADER	lpbiOutput;	/* DIB to decompress to */
    void*		lpOutput;
    long		ckid;		/* ckid from AVI file */
} ICDECOMPRESS;

typedef struct {
    long		dwFlags;
    LPBITMAPINFOHEADER lpbiSrc;
    const void*		lpSrc;
    LPBITMAPINFOHEADER	lpbiDst;
    void*		lpDst;

    /* changed for ICM_DECOMPRESSEX */
    INT		xDst;       /* destination rectangle */
    INT		yDst;
    INT		dxDst;
    INT		dyDst;

    INT		xSrc;       /* source rectangle */
    INT		ySrc;
    INT		dxSrc;
    INT		dySrc;
} ICDECOMPRESSEX;


long VFWAPIV ICDecompress(HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiFormat,void* lpData,LPBITMAPINFOHEADER lpbi,void* lpBits);
long VFWAPIV ICDecompressEx(HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiFormat,void* lpData,LPBITMAPINFOHEADER lpbi,void* lpBits);
long VFWAPIV ICUniversalEx(HIC hic,int command,LPBITMAPINFOHEADER lpbiFormat,LPBITMAPINFOHEADER lpbi);


#define ICDecompressBegin(hic, lpbiInput, lpbiOutput) 	\
    ICSendMessage(						\
    	hic, ICM_DECOMPRESS_BEGIN, (long)(void*)(lpbiInput),	\
	(long)(void*)(lpbiOutput)				\
    )

#define ICDecompressBeginEx(hic, lpbiInput, lpbiOutput) 	\
    ICUniversalEx(						\
    	hic, ICM_DECOMPRESSEX_BEGIN, (lpbiInput),		\
	(lpbiOutput)						\
    )

#define ICDecompressQuery(hic, lpbiInput, lpbiOutput)	 	\
    ICSendMessage(						\
    	hic,ICM_DECOMPRESS_QUERY, (long)(void*)(lpbiInput),	\
	(long) (void*)(lpbiOutput)				\
    )

#define ICDecompressQueryEx(hic, lpbiInput, lpbiOutput) 	\
    ICUniversalEx(						\
    	hic,ICM_DECOMPRESSEX_QUERY, (lpbiInput),		\
	(lpbiOutput)						\
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
#define ICDecompressEndEx(hic) ICSendMessage(hic,ICM_DECOMPRESSEX_END, 0, 0)

#define ICDRAW_QUERY        0x00000001L   /* test for support */
#define ICDRAW_FULLSCREEN   0x00000002L   /* draw to full screen */
#define ICDRAW_HDC          0x00000004L   /* draw to a HDC/HWND */


WIN_BOOL	VFWAPI	ICInfo(long fccType, long fccHandler, ICINFO * lpicinfo);
LRESULT	VFWAPI	ICGetInfo(HIC hic,ICINFO *picinfo, long cb);
HIC	VFWAPI	ICOpen(long fccType, long fccHandler, UINT wMode);
//HIC	VFWAPI	ICOpenFunction(long fccType, long fccHandler, unsigned int wMode, void* lpfnHandler);

LRESULT VFWAPI ICClose(HIC hic);
LRESULT	VFWAPI ICSendMessage(HIC hic, unsigned int msg, long dw1, long dw2);
//HIC	VFWAPI ICLocate(long fccType, long fccHandler, LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut, short wFlags);

int VFWAPI ICDoSomething(void);

long	VFWAPIV	ICDrawBegin(
        HIC			hic,
        long			dwFlags,/* flags */
        HPALETTE		hpal,	/* palette to draw with */
        HWND			hwnd,	/* window to draw to */
        HDC			hdc,	/* HDC to draw to */
        INT			xDst,	/* destination rectangle */
        INT			yDst,
        INT			dxDst,
        INT			dyDst,
        LPBITMAPINFOHEADER	lpbi,	/* format of frame to draw */
        INT			xSrc,	/* source rectangle */
        INT			ySrc,
        INT			dxSrc,
        INT			dySrc,
        long			dwRate,	/* frames/second = (dwRate/dwScale) */
        long			dwScale
);

/* as passed to ICM_DRAW_BEGIN (FIXME: correct only for Win32?)  */
typedef struct {
	long		dwFlags;
	HPALETTE	hpal;
	HWND		hwnd;
	HDC		hdc;
	INT		xDst;
	INT		yDst;
	INT		dxDst;
	INT		dyDst;
	LPBITMAPINFOHEADER	lpbi;
	INT		xSrc;
	INT		ySrc;
	INT		dxSrc;
	INT		dySrc;
	long		dwRate;
	long		dwScale;
} ICDRAWBEGIN;

#define ICDRAW_HURRYUP      0x80000000L   /* don't draw just buffer (hurry up!) */
#define ICDRAW_UPDATE       0x40000000L   /* don't draw just update screen */
#define ICDRAW_PREROLL      0x20000000L   /* this frame is before real start */
#define ICDRAW_NULLFRAME    0x10000000L   /* repeat last frame */
#define ICDRAW_NOTKEYFRAME  0x08000000L   /* this frame is not a key frame */

typedef struct {
	long	dwFlags;
	void*	lpFormat;
	void*	lpData;
	long	cbData;
	long	lTime;
} ICDRAW;

long VFWAPIV ICDraw(HIC hic,long dwFlags,void* lpFormat,void* lpData,long cbData,long lTime);


#define	AVIGETFRAMEF_BESTDISPLAYFMT	1

typedef struct AVISTREAMINFOA {
    long	fccType;
    long	fccHandler;
    long	dwFlags;        /* AVIIF_* */
    long	dwCaps;
    short	wPriority;
    short	wLanguage;
    long	dwScale;
    long	dwRate;		/* dwRate / dwScale == samples/second */
    long	dwStart;
    long	dwLength;	/* In units above... */
    long	dwInitialFrames;
    long	dwSuggestedBufferSize;
    long	dwQuality;
    long	dwSampleSize;
    RECT	rcFrame;
    long	dwEditCount;
    long	dwFormatChangeCount;
    char	szName[64];
} AVISTREAMINFOA, * LPAVISTREAMINFOA, *PAVISTREAMINFOA;

typedef struct AVISTREAMINFOW {
    long	fccType;
    long	fccHandler;
    long	dwFlags;
    long	dwCaps;
    short	wPriority;
    short	wLanguage;
    long	dwScale;
    long	dwRate;		/* dwRate / dwScale == samples/second */
    long	dwStart;
    long	dwLength;	/* In units above... */
    long	dwInitialFrames;
    long	dwSuggestedBufferSize;
    long	dwQuality;
    long	dwSampleSize;
    RECT	rcFrame;
    long	dwEditCount;
    long	dwFormatChangeCount;
    short	szName[64];
} AVISTREAMINFOW, * LPAVISTREAMINFOW, *PAVISTREAMINFOW;
DECL_WINELIB_TYPE_AW(AVISTREAMINFO)
DECL_WINELIB_TYPE_AW(LPAVISTREAMINFO)
DECL_WINELIB_TYPE_AW(PAVISTREAMINFO)

#define AVISTREAMINFO_DISABLED		0x00000001
#define AVISTREAMINFO_FORMATCHANGES	0x00010000

/* AVIFILEINFO.dwFlags */
#define AVIFILEINFO_HASINDEX		0x00000010
#define AVIFILEINFO_MUSTUSEINDEX	0x00000020
#define AVIFILEINFO_ISINTERLEAVED	0x00000100
#define AVIFILEINFO_WASCAPTUREFILE	0x00010000
#define AVIFILEINFO_COPYRIGHTED		0x00020000

/* AVIFILEINFO.dwCaps */
#define AVIFILECAPS_CANREAD		0x00000001
#define AVIFILECAPS_CANWRITE		0x00000002
#define AVIFILECAPS_ALLKEYFRAMES	0x00000010
#define AVIFILECAPS_NOCOMPRESSION	0x00000020

typedef struct AVIFILEINFOW {
    long               dwMaxBytesPerSec;
    long               dwFlags;
    long               dwCaps;
    long               dwStreams;
    long               dwSuggestedBufferSize;
    long               dwWidth;
    long               dwHeight;
    long               dwScale;
    long               dwRate;
    long               dwLength;
    long               dwEditCount;
    short               szFileType[64];
} AVIFILEINFOW, * LPAVIFILEINFOW, *PAVIFILEINFOW;

typedef struct AVIFILEINFOA {
    long               dwMaxBytesPerSec;
    long               dwFlags;
    long               dwCaps;
    long               dwStreams;
    long               dwSuggestedBufferSize;
    long               dwWidth;
    long               dwHeight;
    long               dwScale;
    long               dwRate;
    long               dwLength;
    long               dwEditCount;
    char		szFileType[64];
} AVIFILEINFOA, * LPAVIFILEINFOA, *PAVIFILEINFOA;

DECL_WINELIB_TYPE_AW(AVIFILEINFO)
DECL_WINELIB_TYPE_AW(PAVIFILEINFO)
DECL_WINELIB_TYPE_AW(LPAVIFILEINFO)

/* AVICOMPRESSOPTIONS.dwFlags. determines presence of fields in below struct */
#define AVICOMPRESSF_INTERLEAVE	0x00000001
#define AVICOMPRESSF_DATARATE	0x00000002
#define AVICOMPRESSF_KEYFRAMES	0x00000004
#define AVICOMPRESSF_VALID	0x00000008

typedef struct {
    long	fccType;		/* stream type, for consistency */
    long	fccHandler;		/* compressor */
    long	dwKeyFrameEvery;	/* keyframe rate */
    long	dwQuality;		/* compress quality 0-10,000 */
    long	dwBytesPerSecond;	/* unsigned chars per second */
    long	dwFlags;		/* flags... see below */
    void*	lpFormat;		/* save format */
    long	cbFormat;
    void*	lpParms;		/* compressor options */
    long	cbParms;
    long	dwInterleaveEvery;	/* for non-video streams only */
} AVICOMPRESSOPTIONS, *LPAVICOMPRESSOPTIONS,*PAVICOMPRESSOPTIONS;



typedef struct {
    long		cbSize;		// set to sizeof(COMPVARS) before
					// calling ICCompressorChoose
    long		dwFlags;	// see below...
    HIC			hic;		// HIC of chosen compressor
    long               fccType;	// basically ICTYPE_VIDEO
    long               fccHandler;	// handler of chosen compressor or
					// "" or "DIB "
    LPBITMAPINFO	lpbiIn;		// input format
    LPBITMAPINFO	lpbiOut;	// output format - will compress to this
    void*		lpBitsOut;
    void*		lpBitsPrev;
    long		lFrame;
    long		lKey;		// key frames how often?
    long		lDataRate;	// desired data rate KB/Sec
    long		lQ;		// desired quality
    long		lKeyCount;
    void*		lpState;	// state of compressor
    long		cbState;	// size of the state
} COMPVARS, *PCOMPVARS;

// FLAGS for dwFlags element of COMPVARS structure:


#define AVIERR_OK		0
#define MAKE_AVIERR(error)	MAKE_SCODE(SEVERITY_ERROR,FACILITY_ITF,0x4000+error)

#define AVIERR_UNSUPPORTED	MAKE_AVIERR(101)
#define AVIERR_BADFORMAT	MAKE_AVIERR(102)
#define AVIERR_MEMORY		MAKE_AVIERR(103)
#define AVIERR_INTERNAL		MAKE_AVIERR(104)
#define AVIERR_BADFLAGS		MAKE_AVIERR(105)
#define AVIERR_BADPARAM		MAKE_AVIERR(106)
#define AVIERR_BADSIZE		MAKE_AVIERR(107)
#define AVIERR_BADHANDLE	MAKE_AVIERR(108)
#define AVIERR_FILEREAD		MAKE_AVIERR(109)
#define AVIERR_FILEWRITE	MAKE_AVIERR(110)
#define AVIERR_FILEOPEN		MAKE_AVIERR(111)
#define AVIERR_COMPRESSOR	MAKE_AVIERR(112)
#define AVIERR_NOCOMPRESSOR	MAKE_AVIERR(113)
#define AVIERR_READONLY		MAKE_AVIERR(114)
#define AVIERR_NODATA		MAKE_AVIERR(115)
#define AVIERR_BUFFERTOOSMALL	MAKE_AVIERR(116)
#define AVIERR_CANTCOMPRESS	MAKE_AVIERR(117)
#define AVIERR_USERABORT	MAKE_AVIERR(198)
#define AVIERR_ERROR		MAKE_AVIERR(199)

#endif /* MPLAYER_VFW_H */
