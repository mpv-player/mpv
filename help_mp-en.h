#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2001 Arpad Gereoffy (see DOCS/AUTHORS)\n"
"\n";

static char help_text[]=
"Usage:   mplayer [options] [path/]name\n"
"\n"
"Options:\n"
" -vo <drv[:dev]> select video output driver & device (see '-vo help' for list)\n"
" -ao <drv[:dev]> select audio output driver & device (see '-ao help' for list)\n"
" -vcd <trackno>  play VCD (video cd) track from device instead of plain file\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  specify DVD device for authentication (for encrypted discs)\n"
#endif
" -ss <timepos>   seek to given (seconds or hh:mm:ss) position\n"
" -nosound        don't play sound\n"
#ifdef USE_FAKE_MONO
" -stereo         select MPEG1 stereo output (0:stereo 1:left 2:right)\n"
#endif
" -fs -vm -zoom   fullscreen playing options (fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   scale image to <x> * <y> resolution [if -vo driver supports!]\n"
" -sub <file>     specify subtitle file to use (see also -subfps, -subdelay)\n"
" -vid x -aid y   options to select video (x) and audio (y) stream to play\n"
" -fps x -srate y options to change video (x fps) and audio (y Hz) rate\n"
" -pp <quality>   enable postprocessing filter (0-4 for DivX, 0-63 for mpegs)\n"
" -bps            use alternative A-V sync method for AVI files (may help!)\n"
" -framedrop      enable frame-dropping (for slow machines)\n"
"\n"
"Keys:\n"
" <-  or  ->      seek backward/forward 10 seconds\n"
" up or down      seek backward/forward  1 minute\n"
" p or SPACE      pause movie (press any key to continue)\n"
" q or ESC        stop playing and quit program\n"
" + or -          adjust audio delay by +/- 0.1 second\n"
" o               cycle OSD mode:  none / seekbar / seekbar+timer\n"
" * or /          increase or decrease volume (press 'm' to select master/pcm)\n"
" z or x          adjust subtitle delay by +/- 0.1 second\n"
"\n"
" * * * SEE MANPAGE FOR DETAILS, FURTHER OPTIONS AND KEYS ! * * *\n"
"\n";
#endif

// mplayer.c: 

#define MSGTR_Exiting "\nExiting... (%s)\n"
#define MSGTR_Exit_frames "Requested number of frames played"
#define MSGTR_Exit_quit "Quit"
#define MSGTR_Exit_eof "End of file"
#define MSGTR_Exit_error "Fatal error"
#define MSGTR_IntBySignal "\nMPlayer interrupted by signal %d in module: %s \n"
#define MSGTR_NoHomeDir "Can't find HOME dir\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problem\n"
#define MSGTR_CreatingCfgFile "Creating config file: %s\n"
#define MSGTR_InvalidVOdriver "Invalid video output driver name: %s\nUse '-vo help' to get a list of available video drivers.\n"
#define MSGTR_InvalidAOdriver "Invalid audio output driver name: %s\nUse '-ao help' to get a list of available audio drivers.\n"
#define MSGTR_CopyCodecsConf "(copy/ln etc/codecs.conf (from MPlayer source tree) to ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Can't load font: %s\n"
#define MSGTR_CantLoadSub "Can't load subtitles: %s\n"
#define MSGTR_ErrorDVDkey "Error processing DVD KEY.\n"
#define MSGTR_CmdlineDVDkey "DVD command line requested key is stored for descrambling.\n"
#define MSGTR_DVDauthOk "DVD auth sequence seems to be OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: selected stream missing!\n"
#define MSGTR_CantOpenDumpfile "Can't open dump file!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS not specified (or invalid) in the header! Use the -fps option!\n"
#define MSGTR_NoVideoStream "Sorry, no video stream... it's unplayable yet\n"
#define MSGTR_TryForceAudioFmt "Trying to force audio codec driver family %d ...\n"
#define MSGTR_CantFindAfmtFallback "Can't find audio codec for forced driver family, fallback to other drivers.\n"
#define MSGTR_CantFindAudioCodec "Can't find codec for audio format 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Try to upgrade %s from etc/codecs.conf\n*** If it's still not OK, then read DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Couldn't initialize audio codec! -> nosound\n"
#define MSGTR_TryForceVideoFmt "Trying to force video codec driver family %d ...\n"
#define MSGTR_CantFindVfmtFallback "Can't find video codec for forced driver family, fallback to other drivers.\n"
#define MSGTR_CantFindVideoCodec "Can't find codec for video format 0x%X !\n"
#define MSGTR_VOincompCodec "Sorry, selected video_out device is incompatible with this codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Couldn't initialize video codec :(\n"
#define MSGTR_EncodeFileExists "File already exists: %s (don't overwrite your favourite AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Cannot create file for encoding\n"
#define MSGTR_CannotInitVO "FATAL: Cannot initialize video driver!\n"
#define MSGTR_CannotInitAO "couldn't open/init audio device -> NOSOUND\n"
#define MSGTR_StartPlaying "Start playing...\n"
#define MSGTR_SystemTooSlow "\n************************************************************************"\
			    "\n** Your system is too SLOW to play this! try with -framedrop or RTFM! **"\
			    "\n************************************************************************\n"
//#define MSGTR_

// open.c: 
#define MSGTR_CdDevNotfound "CD-ROM Device '%s' not found!\n"
#define MSGTR_ErrTrackSelect "Error selecting VCD track!"
#define MSGTR_ReadSTDIN "Reading from stdin...\n"
#define MSGTR_UnableOpenURL "Unable to open URL: %s\n"
#define MSGTR_ConnToServer "Connected to server: %s\n"
#define MSGTR_FileNotFound "File not found: '%s'\n"

// demuxer.c:
#define MSGTR_AudioStreamRedefined "Warning! Audio stream header %d redefined!\n"
#define MSGTR_VideoStreamRedefined "Warning! video stream header %d redefined!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Too many (%d in %d bytes) audio packets in the buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Too many (%d in %d bytes) video packets in the buffer!\n"
#define MSGTR_MaybeNI "(maybe you play a non-interleaved stream/file or the codec failed)\n"
#define MSGTR_DetectedAVIfile "Detected AVI file format!\n"
#define MSGTR_DetectedASFfile "Detected ASF file format!\n"
#define MSGTR_DetectedMPEGPESfile "Detected MPEG-PES file format!\n"
#define MSGTR_DetectedMPEGPSfile "Detected MPEG-PS file format!\n"
#define MSGTR_DetectedMPEGESfile "Detected MPEG-ES file format!\n"
#define MSGTR_DetectedQTMOVfile "Detected QuickTime/MOV file format!\n"
#define MSGTR_MissingMpegVideo "Missing MPEG video stream!? contact the author, it may be a bug :(\n"
#define MSGTR_InvalidMPEGES "Invalid MPEG-ES stream??? contact the author, it may be a bug :(\n"
#define MSGTR_FormatNotRecognized "============= Sorry, this file format not recognized/supported ===============\n"\
				  "=== If this file is an AVI, ASF or MPEG stream, please contact the author! ===\n"
#define MSGTR_MissingASFvideo "ASF: no video stream found!\n"
#define MSGTR_MissingASFaudio "ASF: No Audio stream found...  ->nosound\n"
#define MSGTR_MissingMPEGaudio "MPEG: No Audio stream found...  ->nosound\n"

//#define MSGTR_

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "About"
#define MSGTR_FileSelect "Select file ..."
#define MSGTR_MessageBox "MessageBox"
#define MSGTR_PlayList "PlayList"
#define MSGTR_SkinBrowser "Skin Browser"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Cancel"
#define MSGTR_Add "Add"
#define MSGTR_Remove "Remove"

// --- error messages ---
#define MSGTR_NEMDB "Sorry, not enough memory for draw buffer."
#define MSGTR_NEMFMR "Sorry, not enough memory for menu rendering."
#define MSGTR_NEMFMM "Sorry, not enough memory for main window shape mask."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] error in skin config file on line %d: %s" 
#define MSGTR_SKIN_WARNING1 "[skin] warning in skin config file on line %d: widget found but before \"section\" not found ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] warning in skin config file on line %d: widget found but before \"subsection\" not found (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16 bits or less depth bitmap not supported ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "file not found ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "bmp read error ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "tga read error ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "png read error ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE packed tga not supported ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "unknown file type ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "24 bit to 32 bit convert error ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "unknown message: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "not enought memory\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "too many fonts declared\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "font file not found\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "font image file not found\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "non-existent font identifier ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "unknown parameter ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] not enought memory.\n"

#endif
