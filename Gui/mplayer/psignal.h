
#ifndef __GUI_SIGNAL
#define __GUI_SIGNAL

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define mplNoneEvent                             0
#define mplResizeEvent                           1
#define mplQuit                                  2
#define mplPauseEvent                            3
#define mplEndOfFile                             4
#define mplExposeEvent                           5
#define mplSetVideoData                          6
#define mplAudioError                            7
#define mplUnknowError                           8
#define mplSeekEvent                             9
#define mplUnknowFileType                        10
#define mplCodecConfNotFound                     11
#define mplErrorDVDKeyProcess                    12
#define mplErrorDVDAuth                          13
#define mplErrorAVINI                            14
#define mplAVIErrorMissingVideoStream            15
#define mplASFErrorMissingVideoStream            16
#define mplMPEGErrorSeqHeaderSearch              17
#define mplErrorShMemAlloc                       18
#define mplMPEGErrorCannotReadSeqHeader          19
#define mplMPEGErrorBadSeqHeader                 20
#define mplMPEGErrorCannotReadSeqHeaderExt       21
#define mplMPEGErrorBadSeqHeaderExt              22
#define mplCantFindCodecForVideoFormat           23
#define mplIncompatibleVideoOutDevice            24
#define mplCompileWithoutDSSupport               25
#define mplDSCodecNotFound                       26
#define mplCantInitVideoDriver                   27
#define mplIncAudioBufferDelay                   28
#define mplDecAudioBufferDelay                   29

#ifndef SIGTYPE
#ifdef SIGUSR2
#define	SIGTYPE SIGUSR2
#warning should we use SIGUSR1 or SIGUSR2 on linux, bsd, ... too?
#else
#ifdef	__bsdi__
#define	_NSIG NSIG
#endif
#define SIGTYPE _NSIG - 1
#endif
#endif

extern int gtkIsOk;

extern pid_t mplMPlayerPID;
extern pid_t mplParentPID;

extern pid_t gtkPID;

extern void gtkSigHandler( int s );
extern void mplPlayerSigHandler( int s );
extern void mplMainSigHandler( int s );

extern void gtkSendMessage( int msg );

extern void mplErrorHandler( int critical,const char * format, ... );

#endif
