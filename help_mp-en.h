// MASTER FILE. Use this file as base for translation!

// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
// and send a notify message to mplayer-dev-eng maillist.

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Usage:   mplayer [-gui] [options] [url|path/]filename\n"
#else
"Usage:   mplayer [options] [url|path/]filename\n"
#endif
"\n"
"Basic options: (see the manpage for the complete list including ALL options!)\n"
" -vo <drv[:dev]> select video output driver & device (see '-vo help' for list)\n"
" -ao <drv[:dev]> select audio output driver & device (see '-ao help' for list)\n"
#ifdef HAVE_VCD
" -vcd <trackno>  play VCD (video cd) track from device instead of plain file\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  specify DVD device for authentication (for encrypted discs)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  play DVD title/track from device instead of plain file\n"
" -alang/-slang   select DVD audio/subtitle language (by 2-char country code)\n"
#endif
" -ss <timepos>   seek to given (seconds or hh:mm:ss) position\n"
" -nosound        don't play sound\n"
" -fs -vm -zoom   fullscreen playing options (fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   set display resolution (for vidmode change or sw scaling)\n"
" -sub <file>     specify subtitle file to use (see also -subfps, -subdelay)\n"
" -playlist <file> specify playlist file\n"
" -vid x -aid y   options to select video (x) and audio (y) stream to play\n"
" -fps x -srate y options to change video (x fps) and audio (y Hz) rate\n"
" -pp <quality>   enable postprocessing filter (see manpage/docs for details)\n"
" -framedrop      enable frame-dropping (for slow machines)\n"
"\n"
"Basic keys: (see the manpage for the complete list, check the input.conf too)\n"
" <-  or  ->      seek backward/forward 10 seconds\n"
" up or down      seek backward/forward  1 minute\n"
" pgup or pgdown  seek backward/forward 10 minutes\n"
" < or >          step backward/forward in playlist\n"
" p or SPACE      pause movie (press any key to continue)\n"
" q or ESC        stop playing and quit program\n"
" + or -          adjust audio delay by +/- 0.1 second\n"
" o               cycle OSD mode:  none / seekbar / seekbar+timer\n"
" * or /          increase or decrease pcm volume\n"
" z or x          adjust subtitle delay by +/- 0.1 second\n"
" r or t          adjust subtitle position up/down, see also -vop expand !\n"
"\n"
" * * * SEE MANPAGE FOR DETAILS, FURTHER (ADVANCED) OPTIONS AND KEYS ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

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
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Try to upgrade %s from etc/codecs.conf\n*** If it's still not OK, then read DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Couldn't initialize audio codec! -> nosound\n"
#define MSGTR_TryForceVideoFmt "Trying to force video codec driver family %d ...\n"
#define MSGTR_CantFindVfmtFallback "Can't find video codec for forced driver family, fallback to other drivers.\n"
#define MSGTR_CantFindVideoCodec "Can't find codec matching selected -vo and video format 0x%X !\n"
#define MSGTR_VOincompCodec "Sorry, selected video_out device is incompatible with this codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Couldn't initialize video codec :(\n"
#define MSGTR_EncodeFileExists "File already exists: %s (don't overwrite your favourite AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Cannot create file for encoding\n"
#define MSGTR_CannotInitVO "FATAL: Cannot initialize video driver!\n"
#define MSGTR_CannotInitAO "couldn't open/init audio device -> NOSOUND\n"
#define MSGTR_StartPlaying "Start playing...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ************************************************\n"\
"         **** Your system is too SLOW to play this!  ****\n"\
"         ************************************************\n"\
"!!! Possible reasons, problems, workaround: \n"\
"- Most common: broken/buggy _audio_ driver. workaround: try -ao sdl or use\n"\
"  ALSA 0.5 or oss emulation of ALSA 0.9. read DOCS/sound.html for more tips!\n"\
"- Slow video output. try different -vo driver (for list: -vo help) or try\n"\
"  with -framedrop !  Read DOCS/video.html for video tuning/speedup tips.\n"\
"- Slow cpu. don't try to playback big dvd/divx on slow cpu! try -hardframedrop\n"\
"- Broken file. try various combinations of these: -nobps  -ni  -mc 0  -forceidx\n"\
"- You're using -cache to play a non-interleaved file? try with -nocache\n"\
"If none of these apply, then read DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer was compiled WITHOUT GUI support!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI requires X11!\n"
#define MSGTR_Playing "Playing %s\n"
#define MSGTR_NoSound "Audio: no sound!!!\n"
#define MSGTR_FPSforced "FPS forced to be %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Compiled with RUNTIME CPU Detection - warning, it's not optimal! To get best performance, recompile mplayer from sources with --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Compiled for x86 CPU with extensions:"
#define MSGTR_AvailableVideoOutputPlugins "Available video output plugins:\n"
#define MSGTR_AvailableVideoOutputDrivers "Available video output drivers:\n"
#define MSGTR_AvailableAudioOutputDrivers "Available audio output drivers:\n"
#define MSGTR_AvailableAudioCodecs "Available audio codecs:\n"
#define MSGTR_AvailableVideoCodecs "Available video codecs:\n"
#define MSGTR_UsingRTCTiming "Using Linux's hardware RTC timing (%ldHz)\n"
#define MSGTR_CannotReadVideoPropertiers "Video: can't read properties\n"
#define MSGTR_NoStreamFound "No stream found\n"
#define MSGTR_InitializingAudioCodec "Initializing audio codec...\n"
#define MSGTR_ErrorInitializingVODevice "Error opening/initializing the selected video_out (-vo) device!\n"
#define MSGTR_ForcedVideoCodec "Forced video codec: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Description: %s\nAO: Author: %s\n"
#define MSGTR_AOComment "AO: Comment: %s\n"
#define MSGTR_Video_NoVideo "Video: no video!!!\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Couldn't initialize video filters (-vop) or video output (-vo) !\n"
#define MSGTR_Paused "\n------ PAUSED -------\r"
#define MSGTR_PlaylistLoadUnable "\nUnable to load playlist %s\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM Device '%s' not found!\n"
#define MSGTR_ErrTrackSelect "Error selecting VCD track!"
#define MSGTR_ReadSTDIN "Reading from stdin...\n"
#define MSGTR_UnableOpenURL "Unable to open URL: %s\n"
#define MSGTR_ConnToServer "Connected to server: %s\n"
#define MSGTR_FileNotFound "File not found: '%s'\n"

#define MSGTR_CantOpenDVD "Couldn't open DVD device: %s\n"
#define MSGTR_DVDwait "Reading disc structure, please wait...\n"
#define MSGTR_DVDnumTitles "There are %d titles on this DVD.\n"
#define MSGTR_DVDinvalidTitle "Invalid DVD title number: %d\n"
#define MSGTR_DVDnumChapters "There are %d chapters in this DVD title.\n"
#define MSGTR_DVDinvalidChapter "Invalid DVD chapter number: %d\n"
#define MSGTR_DVDnumAngles "There are %d angles in this DVD title.\n"
#define MSGTR_DVDinvalidAngle "Invalid DVD angle number: %d\n"
#define MSGTR_DVDnoIFO "Can't open the IFO file for DVD title %d.\n"
#define MSGTR_DVDnoVOBs "Can't open title VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD successfully opened!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Warning! Audio stream header %d redefined!\n"
#define MSGTR_VideoStreamRedefined "Warning! video stream header %d redefined!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Too many (%d in %d bytes) audio packets in the buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Too many (%d in %d bytes) video packets in the buffer!\n"
#define MSGTR_MaybeNI "(maybe you play a non-interleaved stream/file or the codec failed)?\n" \
		      "For .AVI files, try to force non-interleaved mode with option -ni\n"
#define MSGTR_SwitchToNi "\nDetected badly interleaved .AVI - switching to -ni mode!\n"
#define MSGTR_DetectedFILMfile "Detected FILM file format!\n"
#define MSGTR_DetectedFLIfile "Detected FLI file format!\n"
#define MSGTR_DetectedROQfile "Detected RoQ file format!\n"
#define MSGTR_DetectedREALfile "Detected REAL file format!\n"
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
#define MSGTR_MissingVideoStream "No video stream found!\n"
#define MSGTR_MissingAudioStream "No Audio stream found...  ->nosound\n"
#define MSGTR_MissingVideoStreamBug "Missing video stream!? Contact the author, it may be a bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: file doesn't contain the selected audio or video stream\n"

#define MSGTR_NI_Forced "Forced"
#define MSGTR_NI_Detected "Detected"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI file-format!\n"

#define MSGTR_UsingNINI "Using NON-INTERLEAVED Broken AVI file-format!\n"
#define MSGTR_CouldntDetFNo "Couldn't determine number of frames (for absolute seek)  \n"
#define MSGTR_CantSeekRawAVI "Can't seek in raw .AVI streams! (index required, try with the -idx switch!)  \n"
#define MSGTR_CantSeekFile "Can't seek in this file!  \n"

#define MSGTR_EncryptedVOB "Encrypted VOB file (not compiled with libcss support)! Read file DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Encrypted stream but authentication was not requested by you!!\n"

#define MSGTR_MOVcomprhdr "MOV: Compressed headers not (yet) supported!\n"
#define MSGTR_MOVvariableFourCC "MOV: Warning! variable FOURCC detected!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Warning! too many tracks!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV format not yet supported!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "could not open codec\n"
#define MSGTR_CantCloseCodec "could not close codec\n"

#define MSGTR_MissingDLLcodec "ERROR: Couldn't open required DirectShow codec: %s\n"
#define MSGTR_ACMiniterror "Could not load/initialize Win32/ACM AUDIO codec (missing DLL file?)\n"
#define MSGTR_MissingLAVCcodec "Can't find codec '%s' in libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayer was compiled WITHOUT directshow support!\n"
#define MSGTR_NoWfvSupport "Support for win32 codecs disabled, or unavailable on non-x86 platforms!\n"
#define MSGTR_NoDivx4Support "MPlayer was compiled WITHOUT DivX4Linux (libdivxdecore.so) support!\n"
#define MSGTR_NoLAVCsupport "MPlayer was compiled WITHOUT ffmpeg/libavcodec support!\n"
#define MSGTR_NoACMSupport "Win32/ACM audio codec disabled, or unavailable on non-x86 CPU -> force nosound :(\n"
#define MSGTR_NoDShowAudio "Compiled without DirectShow support -> force nosound :(\n"
#define MSGTR_NoOggVorbis "OggVorbis audio codec disabled -> force nosound :(\n"
#define MSGTR_NoXAnimSupport "MPlayer was compiled WITHOUT XAnim support!\n"

#define MSGTR_MpegPPhint "WARNING! You requested image postprocessing for an MPEG 1/2 video,\n" \
			 "         but compiled MPlayer without MPEG 1/2 postprocessing support!\n" \
			 "         #define MPEG12_POSTPROC in config.h, and recompile libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF while searching for sequence header\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Cannot read sequence header!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Cannot read sequence header extension!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Bad sequence header!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Bad sequence header extension!\n"

#define MSGTR_ShMemAllocFail "Cannot allocate shared memory\n"
#define MSGTR_CantAllocAudioBuf "Cannot allocate audio out buffer\n"
#define MSGTR_NoMemForDecodedImage "not enough memory for decoded picture buffer (%ld bytes)\n"

#define MSGTR_AC3notvalid "AC3 stream not valid.\n"
#define MSGTR_AC3only48k "Only 48000 Hz streams supported.\n"
#define MSGTR_UnknownAudio "Unknown/missing audio format, using nosound\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Setting up lirc support...\n"
#define MSGTR_LIRCdisabled "You won't be able to use your remote control\n"
#define MSGTR_LIRCopenfailed "Failed opening lirc support!\n"
#define MSGTR_LIRCsocketerr "Something's wrong with the lirc socket: %s\n"
#define MSGTR_LIRCcfgerr "Failed to read LIRC config file %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "About"
#define MSGTR_FileSelect "Select file ..."
#define MSGTR_SubtitleSelect "Select subtitle ..."
#define MSGTR_OtherSelect "Select ..."
#define MSGTR_AudioFileSelect "Select external audio channel ..."
#define MSGTR_FontSelect "Select font ..."
#define MSGTR_MessageBox "MessageBox"
#define MSGTR_PlayList "PlayList"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Skin Browser"
#define MSGTR_Network "Network streaming ..."
#define MSGTR_Preferences "Preferences"
#define MSGTR_OSSPreferences "OSS driver configuration"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Cancel"
#define MSGTR_Add "Add"
#define MSGTR_Remove "Remove"
#define MSGTR_Clear "Clear"
#define MSGTR_Config "Config"
#define MSGTR_ConfigDriver "Configure driver"
#define MSGTR_Browse "Browse"

// --- error messages ---
#define MSGTR_NEMDB "Sorry, not enough memory for draw buffer."
#define MSGTR_NEMFMR "Sorry, not enough memory for menu rendering."
#define MSGTR_NEMFMM "Sorry, not enough memory for main window shape mask."
#define MSGTR_IDFGCVD "Sorry, i don't find gui compatible video output driver."

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
#define MSGTR_SKIN_FONT_NotEnoughtMemory "not enough memory\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "too many fonts declared\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "font file not found\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "font image file not found\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "non-existent font identifier ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "unknown parameter ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] not enough memory.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin not found ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin configfile read error ( %s ).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "About MPlayer"
#define MSGTR_MENU_Open "Open ..."
#define MSGTR_MENU_PlayFile "Play file ..."
#define MSGTR_MENU_PlayVCD "Play VCD ..."
#define MSGTR_MENU_PlayDVD "Play DVD ..."
#define MSGTR_MENU_PlayURL "Play URL ..."
#define MSGTR_MENU_LoadSubtitle "Load subtitle ..."
#define MSGTR_MENU_LoadExternAudioFile "Load external audio file ..."
#define MSGTR_MENU_Playing "Playing"
#define MSGTR_MENU_Play "Play"
#define MSGTR_MENU_Pause "Pause"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Next stream"
#define MSGTR_MENU_PrevStream "Prev stream"
#define MSGTR_MENU_Size "Size"
#define MSGTR_MENU_NormalSize "Normal size"
#define MSGTR_MENU_DoubleSize "Double size"
#define MSGTR_MENU_FullScreen "Fullscreen"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Open disc ..."
#define MSGTR_MENU_ShowDVDMenu "Show DVD menu"
#define MSGTR_MENU_Titles "Titles"
#define MSGTR_MENU_Title "Title %2d"
#define MSGTR_MENU_None "(none)"
#define MSGTR_MENU_Chapters "Chapters"
#define MSGTR_MENU_Chapter "Chapter %2d"
#define MSGTR_MENU_AudioLanguages "Audio languages"
#define MSGTR_MENU_SubtitleLanguages "Subtitle languages"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Skin browser"
#define MSGTR_MENU_Preferences "Preferences"
#define MSGTR_MENU_Exit "Exit ..."

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Contrast: "
#define MSGTR_EQU_Brightness "Brightness: "
#define MSGTR_EQU_Hue "Hue: "
#define MSGTR_EQU_Saturation "Saturation: "
#define MSGTR_EQU_Front_Left "Front Left"
#define MSGTR_EQU_Front_Right "Front Right"
#define MSGTR_EQU_Back_Left "Rear Left"
#define MSGTR_EQU_Back_Right "Rear Right"
#define MSGTR_EQU_Center "Center"
#define MSGTR_EQU_Bass "Bass"
#define MSGTR_EQU_All "All"

// --- playlist
#define MSGTR_PLAYLIST_Path "Path"
#define MSGTR_PLAYLIST_Selected "Selected files"
#define MSGTR_PLAYLIST_Files "Files"
#define MSGTR_PLAYLIST_DirectoryTree "Directory tree"

// --- preferences
#define MSGTR_PREFERENCES_None "None"
#define MSGTR_PREFERENCES_Codec1 "Use VFW (Win32) codecs"
#define MSGTR_PREFERENCES_Codec2 "Use OpenDivX/DivX4 codec (YV12)"
#define MSGTR_PREFERENCES_Codec3 "Use DirectShow (Win32) codecs"
#define MSGTR_PREFERENCES_Codec4 "Use ffmpeg (libavcodec) codecs"
#define MSGTR_PREFERENCES_Codec5 "Use DivX4 codec (YUY2)"
#define MSGTR_PREFERENCES_Codec6 "Use XAnim codecs"
#define MSGTR_PREFERENCES_AvailableDrivers "Available drivers:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Do not play sound"
#define MSGTR_PREFERENCES_NormalizeSound "Normalize sound"
#define MSGTR_PREFERENCES_EnEqualizer "Enable equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "Enable extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Coefficient:"
#define MSGTR_PREFERENCES_AudioDelay "Audio delay"
#define MSGTR_PREFERENCES_Audio "Audio"
#define MSGTR_PREFERENCES_VideoEqu "Enable video equalizer"
#define MSGTR_PREFERENCES_DoubleBuffer "Enable double buffering"
#define MSGTR_PREFERENCES_DirectRender "Enable direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Enable frame dropping"
#define MSGTR_PREFERENCES_HFrameDrop "Enable HARD frame drop( dangerous )"
#define MSGTR_PREFERENCES_Flip "Flip image upside-down"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_OSDTimer "Timer and indicators"
#define MSGTR_PREFERENCES_OSDProgress "Progressbars only"
#define MSGTR_PREFERENCES_Subtitle "Subtitle:"
#define MSGTR_PREFERENCES_SUB_Delay "Delay: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Position: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Disable subtitle autoloading"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode subtitle"
#define MSGTR_PREFERENCES_SUB_MPSUB "Convert the given subtitle to MPlayer's subtitle format"
#define MSGTR_PREFERENCES_SUB_SRT "Convert the given subtitle to the time-based SubViewer( SRT ) format"
#define MSGTR_PREFERENCES_Font "Font:"
#define MSGTR_PREFERENCES_FontFactor "Font factor:"
#define MSGTR_PREFERENCES_PostProcess "Enable postprocess"
#define MSGTR_PREFERENCES_AutoQuality "Auto quality: "
#define MSGTR_PREFERENCES_NI "Use non-interleaved AVI parser"
#define MSGTR_PREFERENCES_IDX "Rebuilt index table, if needed"
#define MSGTR_PREFERENCES_VideoCodecFamily "Video codec family:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD level"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Subtitle"
#define MSGTR_PREFERENCES_FRAME_Font "Font"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocess"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & demuxer"
#define MSGTR_PREFERENCES_OSS_Device "Device:"
#define MSGTR_PREFERENCES_OSS_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Message "Please remember, some function need restart the playing."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "fatal error ..."
#define MSGTR_MSGBOX_LABEL_Error "error ..."
#define MSGTR_MSGBOX_LABEL_Warning "warning ..." 

#endif
