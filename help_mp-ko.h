// Translated by: DongCheon Park <pdc@kaist.ac.kr>

// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
// and send a notify message to mplayer-dev-eng maillist.

// ========================= MPlayer 도움말 ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (DOCS 참조!)\n"
"\n";

static char help_text[]=
"사용법:   mplayer [선택사항] [경로/]파일명\n"
"\n"
"선택사항들:\n"
" -vo <drv[:dev]>  비디오 출력 드라이버 및 장치 선택 (목록보기는 '-vo help')\n"
" -ao <drv[:dev]>  오디오 출력 드라이버 및 장치 선택 (목록보기는 '-ao help')\n"
" -vcd <trackno>   파일이 아닌 장치로부터 VCD (비디오 cd) 트랙 재생\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>   인증을 위해 DVD 장치 지정 (암호화된 디스크용)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>   파일이 아닌 장치로부터 DVD 타이틀/트랙 재생\n"
#endif
" -ss <timepos>    특정 위치로 찾아가기 (초 또는 시:분:초)\n"
" -nosound         소리 재생 안함\n"
#ifdef USE_FAKE_MONO
" -stereo <mode>   MPEG1 스테레오 출력 선택 (0:스테레오 1:왼쪽 2:오른쪽)\n"
#endif
" -channels <n>    오디오 출력 채널 개수 지정\n"
" -fs -vm -zoom    화면 크기 지정 (전체화면, 비디오모드, s/w확대)\n"
" -x <x> -y <y>    화면을 <x>*<y>해상도로 변경 [-vo 드라이버가 지원하는 경우만!]\n"
" -sub <file>      사용할 자막파일 지정 (-subfps, -subdelay도 참조)\n"
" -playlist <file> 재생목록파일 지정\n"
" -vid x -aid y    재생할 비디오(x) 와 오디오(y) 스트림 선택\n"
" -fps x -srate y  비디오(x fps)와 오디오(y Hz) 비율 변경\n"
" -pp <quality>    우선처리 필터 사용 (DivX는 0-4, mpegs는 0-63)\n"
" -nobps           AVI 파일을 위해 다른 A-V 동기화 방법 사용\n"
" -framedrop       프레임 빠뜨리기 사용 (느린 machine용)\n"
" -wid <window id> 현재 창에서 비디오 출력 (plugger에 효과적!)\n"
"\n"
"조정키:\n"
" <-  또는  ->     10초 뒤로/앞으로 이동\n"
" up 또는 dn       1분 뒤로/앞으로 이동\n"
" < 또는 >         재생목록에서 뒤로/앞으로 이동\n"
" p 또는 SPACE     잠시 멈춤 (아무키나 누르면 계속)\n"
" q 또는 ESC       재생을 멈추고 프로그램을 끝냄\n"
" + 또는 -         +/- 0.1초 오디오 지연 조절\n"
" o                OSD모드 변경:  없음/탐색줄/탐색줄+타이머\n"
" * 또는 /         볼륨 높임/낮춤 ('m'을 눌러 master/pcm 선택)\n"
" z 또는 x         +/- 0.1초 자막 지연 조절\n"
"\n"
" * * * 자세한 사항(더 많은 선택사항 및 조정키등)은 MANPAGE를 참조하세요 ! * * *\n"
"\n";
#endif

// ========================= MPlayer 메세지 ===========================

// mplayer.c: 

#define MSGTR_Exiting "\n종료합니다... (%s)\n"
#define MSGTR_Exit_quit "종료"
#define MSGTR_Exit_eof "파일의 끝"
#define MSGTR_Exit_error "치명적 오류"
// FIXME: %d must be before %s !!!
// #define MSGTR_IntBySignal "\nMPlayer가 %s모듈에서 %d신호로 인터럽트되었습니다.\n"
#define MSGTR_NoHomeDir "홈디렉토리를 찾을 수 없습니다.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") 문제 발생\n"
#define MSGTR_CreatingCfgFile "설정파일 %s를 만듭니다.\n"
#define MSGTR_InvalidVOdriver "%s는 잘못된 비디오 출력 드라이버입니다.\n가능한 비디오 출력 드라이버 목록을 보려면 '-vo help' 하세요.\n"
#define MSGTR_InvalidAOdriver "%s는 잘못된 오디오 출력 드라이버입니다.\n가능한 오디오 출력 드라이버 목록을 보려면 '-ao help' 하세요.\n"
#define MSGTR_CopyCodecsConf "((MPlayer 소스 트리의) etc/codecs.conf를 ~/.mplayer/codecs.conf로 복사 또는 링크하세요.)\n"
#define MSGTR_CantLoadFont "%s 폰트를 찾을 수 없습니다.\n"
#define MSGTR_CantLoadSub "%s 자막을 찾을 수 없습니다.\n"
#define MSGTR_ErrorDVDkey "DVD 키를 처리하는 도중 오류가 발생했습니다.\n"
#define MSGTR_CmdlineDVDkey "요청한 DVD 명령줄 키를 해독을 위해 저장했습니다.\n"
#define MSGTR_DVDauthOk "DVD 인증 결과가 정상적인듯 합니다.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: 치명적: 선택된 스트림이 없습니다!\n"
#define MSGTR_CantOpenDumpfile "dump파일을 열 수 없습니다!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "헤더에 FPS가 지정되지않았거나 잘못되었습니다! -fps 옵션을 사용하세요!\n"
#define MSGTR_TryForceAudioFmt "오디오 코덱 드라이버 %d류를 시도하고 있습니다...\n"
#define MSGTR_CantFindAfmtFallback "시도한 드라이버류에서 오디오 코덱을 찾을 수 없습니다. 다른 드라이버로 대체하세요.\n"
#define MSGTR_CantFindAudioCodec "오디오 형식 0x%X를 위한 코덱을 찾을 수 없습니다!\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** etc/codecs.conf로 부터 %s를 업그레이드해보세요.\n*** 여전히 작동하지않으면, DOCS/codecs.html을 읽어보세요!\n"
#define MSGTR_CouldntInitAudioCodec "오디오 코덱을 초기화할 수 없습니다! -> 소리없음\n"
#define MSGTR_TryForceVideoFmt "비디오 코덱 드라이버 %d류를 시도하고 있습니다...\n"
#define MSGTR_CantFindVideoCodec "비디오 형식 0x%X를 위한 코덱을 찾을 수 없습니다!\n"
#define MSGTR_VOincompCodec "죄송합니다, 선택한 비디오 출력 장치는 이 코덱과 호환되지 않습니다.\n"
#define MSGTR_CannotInitVO "치명적: 비디오 드라이버를 초기화할 수 없습니다!\n"
#define MSGTR_CannotInitAO "오디오 장치를 열거나 초기화할 수 없습니다. -> 소리없음\n"
#define MSGTR_StartPlaying "재생을 시작합니다...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ************************************************\n"\
"         **** 재생하기에는 시스템이 너무 느립니다.!  ****\n"\
"         ************************************************\n"\
"!!! 가능한 원인, 문제, 대처방안: \n"\
"- 대부분의 경우: 깨진/버그가 많은 오디오 드라이버. 대처방안: -ao sdl 을 사용해보세요.\n"\
"  ALSA 0.5 나 ALSA 0.9의 oss 에뮬레이션. 더 많은 팁은 DOCS/sound.html 을 참조하세요!\n"\
"- 비디오 출력이 느림. 다른 -vo driver (목록은 -vo help)를 사용해보세요.\n"\
"  -framedrop 사용!  비디오 조절/속도향상 팁은 DOCS/video.html 을 참조하세요!\n"\
"- 느린 cpu. 덩치 큰 dvd나 divx를 재생하지마세요! -hardframedrop 을 사용해보세요.\n"\
"- 깨진 파일. 여러가지 조합을 사용해보세요: -nobps  -ni  -mc 0  -forceidx\n"\
"위의 어떤 사항도 적용되지 않는다면, DOCS/bugreports.html 을 참조하세요!\n\n"

#define MSGTR_NoGui "MPlayer가 GUI 지원없이 컴파일되었습니다!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI는 X11이 필요합니다!\n"
#define MSGTR_Playing "%s 재생중\n"
#define MSGTR_NoSound "오디오: 소리없음!!!\n"
#define MSGTR_FPSforced "FPS가 %5.3f (ftime: %5.3f)이 되도록 하였습니다.\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM 장치 '%s'를 찾을 수 없습니다!\n"
#define MSGTR_ErrTrackSelect "VCD 트랙을 선택하는 도중 에러가 발생했습니다!"
#define MSGTR_ReadSTDIN "표준입력(stdin)으로 부터 읽고 있습니다...\n"
#define MSGTR_UnableOpenURL "%s URL을 열 수 없습니다.\n"
#define MSGTR_ConnToServer "%s 서버에 연결되었습니다.\n"
#define MSGTR_FileNotFound "'%s'파일을 찾을 수 없습니다.\n"

#define MSGTR_CantOpenDVD "DVD 장치 %s를 열 수 없습니다.\n"
#define MSGTR_DVDwait "디스크 구조를 읽고있습니다, 기다려 주세요...\n"
#define MSGTR_DVDnumTitles "이 DVD에는 %d 타이틀이 있습니다.\n"
#define MSGTR_DVDinvalidTitle "잘못된 DVD 타이틀 번호: %d\n"
#define MSGTR_DVDnumChapters "이 DVD 타이틀에는 %d 챕터가 있습니다.\n"
#define MSGTR_DVDinvalidChapter "잘못된 DVD 챕터 번호: %d\n"
#define MSGTR_DVDnumAngles "이 DVD 타이틀에는 %d 앵글이 있습니다.\n"
#define MSGTR_DVDinvalidAngle "잘못된 DVD 앵글 번호: %d\n"
#define MSGTR_DVDnoIFO "DVD 타이틀 %d를 위한 IFO파일을 열 수 없습니다.\n"
#define MSGTR_DVDnoVOBs "타이틀 VOBS (VTS_%02d_1.VOB)를 열 수 없습니다.\n"
#define MSGTR_DVDopenOk "성공적으로 DVD가 열렸습니다!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "경고! 오디오 스트림 헤더 %d가 재정의되었습니다!\n"
#define MSGTR_VideoStreamRedefined "경고! 비디오 스트림 헤더 %d가 재정의되었습니다!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: 버퍼에 너무 많은 (%d in %d bytes) 오디오 패킷이 있습니다!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: 버퍼에 너무 많은 (%d in %d bytes) 비디오 패킷이 있습니다!\n"
#define MSGTR_MaybeNI "(non-interleaved 스트림/파일을 재생하고있거나 코덱이 잘못되었습니다.)\n"
#define MSGTR_DetectedFILMfile "FILM 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedFLIfile "FLI 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedROQfile "RoQ 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedREALfile "REAL 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedAVIfile "AVI 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedASFfile "ASF 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedMPEGPESfile "MPEG-PES 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedMPEGPSfile "MPEG-PS 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedMPEGESfile "MPEG-ES 파일 형식을 발견했습니다!\n"
#define MSGTR_DetectedQTMOVfile "QuickTime/MOV 파일 형식을 발견했습니다!\n"
#define MSGTR_InvalidMPEGES "잘못된 MPEG-ES 스트림??? 저작자에게 문의하세요, 버그일지도 모릅니다. :(\n"
#define MSGTR_FormatNotRecognized "============= 죄송합니다, 이 파일형식은 인식되지못했거나 지원되지않습니다 ===============\n"\
				  "=== 만약 이 파일이 AVI, ASF 또는 MPEG 스트림이라면, 저작자에게 문의하세요! ===\n"
#define MSGTR_MissingVideoStream "비디오 스트림을 찾지 못했습니다!\n"
#define MSGTR_MissingAudioStream "오디오 스트림을 찾지 못했습니다...  ->소리없음\n"
#define MSGTR_MissingVideoStreamBug "찾을 수 없는 비디오 스트림!? 저작자에게 문의하세요, 버그일지도 모릅니다. :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: 파일에 선택된 오디오 및 비디오 스트림이 없습니다.\n"

#define MSGTR_NI_Forced "강제로"
#define MSGTR_NI_Detected "발견됨"
#define MSGTR_NI_Message "%s는 NON-INTERLEAVED AVI 파일 형식입니다!\n"

#define MSGTR_UsingNINI "NON-INTERLEAVED 깨진 AVI 파일 형식을 사용합니다!\n"
#define MSGTR_CouldntDetFNo "프레임 수를 결정할 수 없습니다.\n"
#define MSGTR_CantSeekRawAVI "raw .AVI 스트림에서는 탐색할 수 없습니다! (인덱스가 필요합니다, -idx 스위치로 시도해보세요!)  \n"
#define MSGTR_CantSeekFile "이 파일에서는 탐색할 수 없습니다!  \n"

#define MSGTR_EncryptedVOB "암호화된 VOB 파일입니다(libcss 지원없이 컴파일되었음)! DOCS/cd-dvd.html을 참조하세요\n"
#define MSGTR_EncryptedVOBauth "암호화된 스트림인데, 인증 신청을 하지않았습니다!!\n"

#define MSGTR_MOVcomprhdr "MOV: 압축된 헤더는 (아직) 지원되지않습니다!\n"
#define MSGTR_MOVvariableFourCC "MOV: 경고! FOURCC 변수 발견!?\n"
#define MSGTR_MOVtooManyTrk "MOV: 경고! 트랙이 너무 많습니다!"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "코덱을 열 수 없습니다.\n"
#define MSGTR_CantCloseCodec "코덱을 닫을 수 없습니다.\n"

#define MSGTR_MissingDLLcodec "에러: 요청한 DirectShow 코덱 %s를 열 수 없습니다.\n"
#define MSGTR_ACMiniterror "Win32/ACM 오디오 코덱을 열거나 초기화할 수 없습니다. (DLL 파일이 없나요?)\n"
#define MSGTR_MissingLAVCcodec "libavcodec에서 '%s' 코덱을 찾을 수 없습니다...\n"

#define MSGTR_MpegNoSequHdr "MPEG: 치명적: 시퀀스 헤더를 찾는 도중 EOF.\n"
#define MSGTR_CannotReadMpegSequHdr "치명적: 시퀀스 헤더를 찾을 수 없습니다!\n"
#define MSGTR_CannotReadMpegSequHdrEx "치명적: 시퀀스 헤더 확장을 읽을 수 없습니다!\n"
#define MSGTR_BadMpegSequHdr "MPEG: 불량한 시퀀스 헤더!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: 불량한 시퀀스 헤더 확장!\n"

#define MSGTR_ShMemAllocFail "공유 메모리를 할당할 수 없습니다.\n"
#define MSGTR_CantAllocAudioBuf "오디오 출력 버퍼를 할당할 수 없습니다.\n"

#define MSGTR_UnknownAudio "알 수 없는 오디오 형식입니다. -> 소리없음\n"

// LIRC:
#define MSGTR_SettingUpLIRC "lirc 지원을 시작합니다...\n"
#define MSGTR_LIRCdisabled "리모콘을 사용할 수 없습니다.\n"
#define MSGTR_LIRCopenfailed "lirc 지원 시작 실패!\n"
#define MSGTR_LIRCcfgerr "LIRC 설정파일 %s를 읽는데 실패했습니다!\n"


// ====================== GUI 메세지/버튼 ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "정보"
#define MSGTR_FileSelect "파일 선택 ..."
#define MSGTR_SubtitleSelect "자막 선택 ..."
#define MSGTR_OtherSelect "선택 ..."
#define MSGTR_PlayList "재생목록"
#define MSGTR_SkinBrowser "스킨 찾기"

// --- buttons ---
#define MSGTR_Ok "확인"
#define MSGTR_Cancel "취소"
#define MSGTR_Add "추가"
#define MSGTR_Remove "삭제"

// --- error messages ---
#define MSGTR_NEMDB "죄송합니다, draw 버퍼에 충분한 메모리가 없습니다."
#define MSGTR_NEMFMR "죄송합니다, 메뉴 렌더링을 위한 충분한 메모리가 없습니다."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[스킨] 스킨 설정파일 %s의 %d번째 줄에 에러가 있습니다." 
#define MSGTR_SKIN_WARNING1 "[스킨] 스킨 설정파일의 %d번째 줄에 경고: 위젯을 찾았지만 \"section\"앞에 ( %s )를 찾을 수 없습니다."
#define MSGTR_SKIN_WARNING2 "[스킨] 스킨 설정파일의 %d번째 줄에 경고: 위젯을 찾았지만 \"subsection\"앞에 ( %s )를 찾을 수 없습니다."
#define MSGTR_SKIN_BITMAP_16bit  "16 비트 혹은 더 작은 depth의 비트맵은 지원되지 않습니다 ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "파일을 찾을 수 없습니다 ( %s ).\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "bmp 읽기 에러 ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "tga 읽기 에러 ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "png 읽기 에러 ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE 팩된 tga는 지원되지 않습니다 ( %s ).\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "알 수 없는 파일 형식 ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "24 비트에서 32 비트로 전환 에러 ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "알 수 없는 메세지: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "메모리가 부족합니다.\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "폰트가 너무 많이 선언되어 있습니다.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "폰트파일을 찾을 수 없습니다.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "폰트 이미지파일을 찾을 수 없습니다.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "존재하지 않는 폰트 identifier ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "알 수 없는 매개변수 ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[스킨선택] 메모리가 부족합니다.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "스킨을 찾을 수 없습니다 ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "스킨 설정파일 읽기 에러 ( %s )\n"
#define MSGTR_SKIN_LABEL "스킨:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "MPlayer 정보"
#define MSGTR_MENU_Open "열기 ..."
#define MSGTR_MENU_PlayFile "파일 재생 ..."
#define MSGTR_MENU_PlayVCD "VCD 재생 ..."
#define MSGTR_MENU_PlayDVD "DVD 재생 ..."
#define MSGTR_MENU_PlayURL "URL 재생 ..."
#define MSGTR_MENU_LoadSubtitle "자막 선택 ..."
#define MSGTR_MENU_Playing "재생중"
#define MSGTR_MENU_Play "재생"
#define MSGTR_MENU_Pause "멈춤"
#define MSGTR_MENU_Stop "정지"
#define MSGTR_MENU_NextStream "다음"
#define MSGTR_MENU_PrevStream "이전"
#define MSGTR_MENU_Size "크기"
#define MSGTR_MENU_NormalSize "보통 크기"
#define MSGTR_MENU_DoubleSize "두배 크기"
#define MSGTR_MENU_FullScreen "전체 화면"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "디스크 재생 ..."
#define MSGTR_MENU_ShowDVDMenu "DVD 메뉴보기"
#define MSGTR_MENU_Titles "타이틀"
#define MSGTR_MENU_Title "타이틀 %2d"
#define MSGTR_MENU_None "(없음)"
#define MSGTR_MENU_Chapters "챕터"
#define MSGTR_MENU_Chapter "챕터 %2d"
#define MSGTR_MENU_AudioLanguages "오디오 언어"
#define MSGTR_MENU_SubtitleLanguages "자막 언어"
#define MSGTR_MENU_PlayList "재생목록"
#define MSGTR_MENU_SkinBrowser "스킨선택"
#define MSGTR_MENU_Preferences "선택사항"
#define MSGTR_MENU_Exit "종료 ..."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "치명적인 에러 ..."
#define MSGTR_MSGBOX_LABEL_Error "에러 ..."
#define MSGTR_MSGBOX_LABEL_Warning "경고 ..." 

#endif


