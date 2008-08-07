// Fully sync'ed with help_mp-en.h 1.120
// Translated by: DongCheon Park <dcpark@kaist.ac.kr>

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"사용법:   mplayer [선택사항] [url|경로/]파일명\n"
"\n"
"기본 선택사항: (전체 목록은 man 페이지 참조)\n"
" -vo <drv[:dev]>  비디오 출력 드라이버 및 장치 선택 (목록보기는 '-vo help')\n"
" -ao <drv[:dev]>  오디오 출력 드라이버 및 장치 선택 (목록보기는 '-ao help')\n"
#ifdef CONFIG_VCD
" vcd://<trackno>  (S)VCD (Super Video CD) 트랙 재생 (장치로부터, 마운트 없이)\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<titleno>  일반 파일이 아닌 장치로부터 DVD 타이틀 재생\n"
" -alang/-slang    DVD 오디오/자막 언어 선택 (두 글자의 국가 코드)\n"
#endif
" -ss <timepos>    특정 위치로 찾아가기 (초 또는 시:분:초)\n"
" -nosound         소리 재생 안함\n"
" -fs              전체화면 재생 (또는 -vm, -zoom, 자세한 사항은 man 페이지)\n"
" -x <x> -y <y>    화면을 <x>*<y>해상도로 설정 (-vm이나 -zoom과 함께 사용함)\n"
" -sub <file>      사용할 자막 파일 지정 (-subfps, -subdelay도 참고할 것)\n"
" -playlist <file> 재생목록 파일 지정\n"
" -vid x -aid y    재생할 비디오(x) 와 오디오(y) 스트림 선택\n"
" -fps x -srate y  비디오(x fps)와 오디오(y Hz) 비율 변경\n"
" -pp <quality>    후행처리 필터 사용 (자세한 사항은 man 페이지 참조)\n"
" -framedrop       프레임 건너뛰기 사용 (느린 컴퓨터용 선택사항)\n"
"\n"
"기본 조정키: (전체 조정키 목록은 man 페이지 참조, input.conf도 확인할 것)\n"
" <-  또는  ->     10초 뒤로/앞으로 이동\n"
" up 또는 down     1분 뒤로/앞으로 이동\n"
" pgup 또는 pgdown 10분 뒤로/앞으로 이동\n"
" < 또는 >         재생목록에서 뒤로/앞으로 이동\n"
" p 또는 SPACE     잠시 멈춤 (아무키나 누르면 계속)\n"
" q 또는 ESC       재생을 멈추고 프로그램을 끝냄\n"
" + 또는 -         +/- 0.1초씩 오디오 지연 조정\n"
" o                OSD모드 변경:  없음/탐색줄/탐색줄+타이머\n"
" * 또는 /         PCM 음량을 높임/낮춤\n"
" z 또는 x         +/- 0.1초씩 자막 지연 조정\n"
" r 또는 t         자막 위치를 위로/아래로 조정, -vf expand도 참고할 것\n"
"\n"
" * * * 더 자세한 (고급) 선택사항 및 조정키는 MAN 페이지를 참조하세요. * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\n종료합니다.\n"
#define MSGTR_ExitingHow "\n종료합니다... (%s)\n"
#define MSGTR_Exit_quit "종료"
#define MSGTR_Exit_eof "파일의 끝"
#define MSGTR_Exit_error "치명적 오류"
#define MSGTR_IntBySignal "\nMPlayer가 %d시그널에 의해 인터럽트되었습니다. - 모듈: %s\n"
#define MSGTR_NoHomeDir "홈디렉토리를 찾을 수 없습니다.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") 문제 발생\n"
#define MSGTR_CreatingCfgFile "설정파일을 만듭니다.: %s\n"
#define MSGTR_BuiltinCodecsConf "내장된 기본 codecs.conf를 사용합니다.\n"
#define MSGTR_CantLoadFont "폰트를 읽어 들일 수 없습니다.: %s\n"
#define MSGTR_CantLoadSub "자막을 읽어 들일 수 없습니다.: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: 치명적 : 선택된 스트림이 없습니다!\n"
#define MSGTR_CantOpenDumpfile "dump파일을 열 수 없습니다.\n"
#define MSGTR_CoreDumped "Core dumped :)\n"
#define MSGTR_FPSnotspecified "헤더에 FPS가 지정되지 않았거나 유효하지 않습니다. -fps 옵션을 사용하세요.\n"
#define MSGTR_TryForceAudioFmtStr "오디오 코덱 드라이버 집합 %s를 시도하고 있습니다...\n"
#define MSGTR_CantFindAudioCodec "오디오 형식 0x%X를 위한 코덱을 찾을 수 없습니다.\n"
#define MSGTR_RTFMCodecs "DOCS/HTML/en/codecs.html을 참조하세요!\n"
#define MSGTR_TryForceVideoFmtStr "비디오 코덱 드라이버 집합 %s를 시도하고 있습니다...\n"
#define MSGTR_CantFindVideoCodec "선택한 -vo 및 비디오 형식 0x%X과 일치하는 코덱을 찾을 수 없습니다.\n"
#define MSGTR_VOincompCodec "선택한 비디오 출력 장치는 이 코덱과 호환되지 않습니다.\n"
#define MSGTR_CannotInitVO "치명적 오류: 비디오 드라이버를 초기화할 수 없습니다.\n"
#define MSGTR_CannotInitAO "오디오 장치를 열거나 초기화할 수 없습니다. -> 소리없음\n"
#define MSGTR_StartPlaying "재생을 시작합니다...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ************************************************\n"\
"         **** 재생하기에는 시스템이 너무 느립니다.!  ****\n"\
"         ************************************************\n"\
"가능성있는 원인과 문제 및 대처방안: \n"\
"- 대부분의 경우: 깨진/버그가 많은 오디오 드라이버\n"\
"  - -ao sdl을 시도하거나 ALSA 0.5, 혹은 ALSA 0.9의 OSS 에뮬레이션을 사용해보세요.\n"\
"  - -autosync를으로 여러가지 값으로 실험해보세요. 시작 값으로는 30이 적당합니다.\n"\
"- 비디오 출력이 느림\n"\
"  - 다른 -vo driver를 시도하거나 (목록보기는 -vo help), -framedrop을 시도해보세요!\n"\
"- 느린 CPU\n"\
"  - 덩치 큰 DVD나 DivX를 느린 CPU에서 재생하지 마세요! -hardframedrop을 시도해보세요.\n"\
"- 깨진 파일\n"\
"  - -nobps -ni -forceidx -mc 0 등의 여러가지 조합을 시도해보세요.\n"\
"- 느린 미디어 (NFS/SMB 마운트, DVD, VCD 등)\n"\
"  - -cache 8192를 시도해보세요.\n"\
"- non-interleaved AVI 파일을 -cache 옵션으로 재생하고 있나요?\n"\
"  - -nocache를 시도해보세요.\n"\
"미세조정/속도향상 팁은 DOCS/HTML/en/video.html과 DOCS/HTML/en/audio.html을 참조하세요.\n"\
"위의 어떤 사항도 도움이 되지 않는다면, DOCS/HTML/en/bugreports.html을 참조하세요.\n\n"

#define MSGTR_NoGui "MPlayer가 GUI를 사용할 수 있도록 컴파일되지 않았습니다.\n"
#define MSGTR_GuiNeedsX "MPlayer GUI는 X11을 필요로합니다!\n"
#define MSGTR_Playing "%s 재생 중...\n"
#define MSGTR_NoSound "오디오: 소리없음\n"
#define MSGTR_FPSforced "FPS가 %5.3f (ftime: %5.3f)으로 변경되었습니다.\n"
#define MSGTR_CompiledWithRuntimeDetection "런타임 CPU 감지가 가능하도록 컴파일되었습니다. - 경고 - 이것은 최적 조건이 아닙니다!\n최상의 성능을 얻기위해선, MPlayer를 --disable-runtime-cpudetection 옵션으로 다시 컴파일하세요.\n"
#define MSGTR_CompiledWithCPUExtensions "확장 x86 CPU용으로 컴파일 되었습니다.:"
#define MSGTR_AvailableVideoOutputDrivers "가능한 비디오 출력 드라이버:\n"
#define MSGTR_AvailableAudioOutputDrivers "가능한 오디오 출력 드리아버:\n"
#define MSGTR_AvailableAudioCodecs "가능한 오디오 코덱:\n"
#define MSGTR_AvailableVideoCodecs "가능한 비디오 코덱:\n"
#define MSGTR_AvailableAudioFm "\n가능한 (컴파일된) 오디오 코덱 집합/드라이버:\n"
#define MSGTR_AvailableVideoFm "\n가능한 (컴파일된) 비디오 코덱 집합/드라이버:\n"
#define MSGTR_AvailableFsType "가능한 전체화면 레이어 변경 모드:\n"
#define MSGTR_UsingRTCTiming "리눅스 하드웨어 RTC 타이밍(%ldHz)을 사용합니다.\n"
#define MSGTR_CannotReadVideoProperties "비디오: 속성을 읽을 수 없습니다.\n"
#define MSGTR_NoStreamFound "스티림을 찾을 수 없습니다.\n"
#define MSGTR_ErrorInitializingVODevice "선택한 비디오 출력 (-vo) 장치를 열거나 초기화할 수 없습니다.\n"
#define MSGTR_ForcedVideoCodec "강제로 사용된 비디오 코덱: %s\n"
#define MSGTR_ForcedAudioCodec "강제로 사용된 오디오 코덱: %s\n"
#define MSGTR_Video_NoVideo "비디오: 비디오 없음\n"
#define MSGTR_NotInitializeVOPorVO "\n치명적 오류: 비디오 필터(-vf) 또는 비디오 출력(-vo)을 초기화할 수 없습니다.\n"
#define MSGTR_Paused "\n  =====  잠시멈춤  =====\r"
#define MSGTR_PlaylistLoadUnable "\n재생목록 %s을(를) 열 수 없습니다.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer가 '잘못된 연산'으로 종료되었습니다.\n"\
"  런타임 CPU 감지 코드에 버그가 있을 지도 모릅니다...\n"\
"  DOCS/HTML/en/bugreports.html을 참조하세요.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer가 '잘못된 연산'으로 종료되었습니다.\n"\
"  컴파일/최적화된 CPU와 다른 모델의 CPU에서 실행될 때\n"\
"  종종 일어나는 현상입니다.\n"\
"  확인해 보세요!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer가 잘못된 CPU/FPU/RAM의 사용으로 종료되었습니다.\n"\
"  MPlayer를 --enable-debug 옵션으로 다시 컴파일하고, 'gdb' 백트레이스 및\n"\
"  디스어셈블해보세요. 자세한 사항은 DOCS/HTML/en/bugreports_what.html#bugreports_crash를 참조하세요.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer가 알 수 없는 이유로 종료되었습니다.\n"\
"  MPlayer 코드나 드라이버의 버그, 혹은 gcc버전의 문제일 수도 있습니다.\n"\
"  MPlayer의 문제라고 생각한다면, DOCS/HTML/en/bugreports.html을 읽고 거기있는\n"\
"  설명대로 하시기 바랍니다. 가능한 버그를 보고할 땐, 이 정보를 포함하세요.\n"\
"  그렇지 않으면, 도와줄 방법이 없습니다.\n"


// mencoder.c:

#define MSGTR_UsingPass3ControlFile "pass3 컨트롤 파일을 사용합니다.: %s\n"
#define MSGTR_MissingFilename "\n파일이름이 없습니다.\n\n"
#define MSGTR_CannotOpenFile_Device "파일/장치를 열 수 없습니다.\n"
#define MSGTR_CannotOpenDemuxer "해석기를 열 수 없습니다.\n"
#define MSGTR_NoAudioEncoderSelected "\n선택된 오디오 인코더(-oac)가 없습니다. 하나를 선택하거나, -nosound 옵션을 사용하세요. -oac help를 참조하세요!\n"
#define MSGTR_NoVideoEncoderSelected "\n선택된 비디오 인코더(-ovc)가 없습니다. 하나를 선택거나, -ovc help를 참조하세요!\n"
#define MSGTR_CannotOpenOutputFile "출력 파일 '%s'을(를) 열 수 없습니다.\n"
#define MSGTR_EncoderOpenFailed "인코더 열기에 실패했습니다.\n"
#define MSGTR_ForcingOutputFourcc "fourcc를 %x [%.4s](으)로 강제출력합니다.\n"
#define MSGTR_DuplicateFrames "\n%d 프레임(들)이 중복되었습니다!\n"
#define MSGTR_SkipFrame "\n프레임을 건너 뜁니다!\n"
#define MSGTR_ErrorWritingFile "%s: 파일 쓰기 오류가 발생했습니다.\n"
#define MSGTR_RecommendedVideoBitrate "%s CD용으로 추천할 만한 비디오 주사율: %d\n"
#define MSGTR_VideoStreamResult "\n비디오 스트림: %8.3f kbit/s  (%d B/s)  크기: %"PRIu64" 바이트  %5.3f 초  %d 프레임\n"
#define MSGTR_AudioStreamResult "\n오디오 스트림: %8.3f kbit/s  (%d B/s)  크기: %"PRIu64" 바이트  %5.3f 초\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     가변 비트레이트 방식\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(기본값)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           평균 비트레이트\n"\
"\n"\
" cbr           고정 비트레이트\n"\
"               일련의 ABR 프리셋 모드들에서 CBR모드 강제 사용함.\n"\
"\n"\
" br=<0-1024>   비트레이트를 kBit단위로 지정 (CBR 및 ABR에서만)\n"\
"\n"\
" q=<0-9>       음질 (0-최고, 9-최저) (VBR에서만)\n"\
"\n"\
" aq=<0-9>      연산 음질 (0-최고/느림, 9-최저/빠름)\n"\
"\n"\
" ratio=<1-100> 압축률\n"\
"\n"\
" vol=<0-10>    오디오 입력 음량 조절\n"\
"\n"\
" mode=<0-3>    (기본값: 자동)\n"\
"                0: 스테레오\n"\
"                1: 조인트-스테레오\n"\
"                2: 듀얼채널\n"\
"                3: 모노\n"\
"\n"\
" padding=<0-2>\n"\
"                0: 안함\n"\
"                1: 모두\n"\
"                2: 조정\n"\
"\n"\
" fast          일련의 VBR 프리셋 모드들에서 더 빠른 인코딩 사용,\n"\
"               음질이 조금 저하되고 비트레이트가 조금 더 높아짐.\n"\
"\n"\
" preset=<value> 최적의 가능한 음질 세팅들.\n"\
"                 medium: VBR  인코딩, 좋은 음질\n"\
"                 (150-180 kbps 비트레이트 범위)\n"\
"                 standard:  VBR 인코딩, 높은 음질\n"\
"                 (170-210 kbps 비트레이트 범위)\n"\
"                 extreme: VBR 인코딩, 매우 높은 음질\n"\
"                 (200-240 kbps 비트레이트 범위)\n"\
"                 insane:  CBR  인코딩, 가장 높은 음질\n"\
"                 (320 kbps 비트레이트 고정)\n"\
"                 <8-320>: 주어진 kbps 비트레이트의 평균치로 ABR 인코딩.\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM 장치 '%s'를 찾을 수 없습니다!\n"
#define MSGTR_ErrTrackSelect "VCD 트랙을 선택하는 도중 에러가 발생했습니다."
#define MSGTR_ReadSTDIN "표준입력(stdin)으로 부터 읽고 있습니다...\n"
#define MSGTR_UnableOpenURL "URL을 열 수 없습니다.: %s\n"
#define MSGTR_ConnToServer "서버에 연결되었습니다.: %s\n"
#define MSGTR_FileNotFound "파일을 찾을 수 없습니다.: '%s'\n"

#define MSGTR_SMBInitError "libsmbclient 라이브러리를 초기화할 수 없습니다.: %d\n"
#define MSGTR_SMBFileNotFound "lan으로 부터 열 수 없습니다.: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer가 SMB읽기를 할 수 있도록 컴파일되지 않았습니다.\n"

#define MSGTR_CantOpenDVD "DVD 장치를 열 수 없습니다.: %s (%s)\n"
#define MSGTR_DVDnumTitles "이 DVD에는 %d개의 타이틀이 있습니다.\n"
#define MSGTR_DVDinvalidTitle "유효하지 않은 DVD 타이틀 번호입니다.: %d\n"
#define MSGTR_DVDnumChapters "이 DVD 타이틀에는 %d개의 챕터가 있습니다.\n"
#define MSGTR_DVDinvalidChapter "유효하지 않은 DVD 챕터 번호입니다.: %d\n"
#define MSGTR_DVDnumAngles "이 DVD 타이틀에는 %d개의 앵글이 있습니다.\n"
#define MSGTR_DVDinvalidAngle "유효하지 않은 DVD 앵글 번호입니다.: %d\n"
#define MSGTR_DVDnoIFO "DVD 타이틀 %d를 위한 IFO파일을 열 수 없습니다.\n"
#define MSGTR_DVDnoVOBs "타이틀 VOBS (VTS_%02d_1.VOB)를 열 수 없습니다.\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "경고: 오디오 스트림 헤더 %d가 재정의되었습니다.\n"
#define MSGTR_VideoStreamRedefined "경고: 비디오 스트림 헤더 %d가 재정의되었습니다.\n"
#define MSGTR_TooManyAudioInBuffer "\n버퍼에 너무 많은 오디오 패킷이 있습니다.: (%d in %d bytes)\n"
#define MSGTR_TooManyVideoInBuffer "\n버퍼에 너무 많은 비디오 패킷이 있습니다.: (%d in %d bytes)\n"
#define MSGTR_MaybeNI "non-interleaved 스트림/파일을 재생하고있거나 코덱에 문제가 있나요?\n" \
		      "AVI 파일의 경우, -ni 옵션으로 non-interleaved 모드로 강제 시도해보세요.\n"
#define MSGTR_SwitchToNi "\n잘못된 interleaved AVI 파일을 발견했습니다. -ni 모드로 변경합니다...\n"
#define MSGTR_Detected_XXX_FileFormat "%s 파일 형식을 발견했습니다.\n"
#define MSGTR_DetectedAudiofile "오디오 파일을 감지하였습니다.\n"
#define MSGTR_NotSystemStream "MPEG 시스템 스트림 포맷이 아닙니다... (혹시 전송 스트림일지도?)\n"
#define MSGTR_InvalidMPEGES "유효하지 않은 MPEG-ES 스트림??? 저작자에게 문의하세요, 버그일지도 모릅니다. :(\n"
#define MSGTR_FormatNotRecognized "============= 죄송합니다. 이 파일형식을 인식하지못했거나 지원하지않습니다 ===============\n"\
				  "=== 만약 이 파일이 AVI, ASF 또는 MPEG 스트림이라면, 저작자에게 문의하세요! ===\n"
#define MSGTR_MissingVideoStream "비디오 스트림을 찾지 못했습니다.\n"
#define MSGTR_MissingAudioStream "오디오 스트림을 찾지 못했습니다. -> 소리없음\n"
#define MSGTR_MissingVideoStreamBug "찾을 수 없는 비디오 스트림!? 저작자에게 문의하세요, 버그일지도 모릅니다. :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: 파일에 선택된 오디오 및 비디오 스트림이 없습니다.\n"

#define MSGTR_NI_Forced "강제로"
#define MSGTR_NI_Detected "발견됨"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI 파일 형식입니다.\n"

#define MSGTR_UsingNINI "NON-INTERLEAVED 깨진 AVI 파일 형식을 사용중입니다.\n"
#define MSGTR_CouldntDetFNo "(절대 탐색을 위한) 프레임 수를 결정할 수 없습니다.\n"
#define MSGTR_CantSeekRawAVI "raw AVI 스트림에서는 탐색할 수 없습니다. (인덱스가 필요합니다. -idx 스위치로 시도해보세요.)  \n"
#define MSGTR_CantSeekFile "이 파일에서는 탐색할 수 없습니다.\n"

#define MSGTR_MOVcomprhdr "MOV: 압축된 헤더는 (아직) 지원되지않습니다.\n"
#define MSGTR_MOVvariableFourCC "MOV: 경고: 가변적인 FOURCC 발견!?\n"
#define MSGTR_MOVtooManyTrk "MOV: 경고: 트랙이 너무 많습니다."
#define MSGTR_FoundAudioStream "==> 오디오 스트림을 찾았습니다.: %d\n"
#define MSGTR_FoundVideoStream "==> 비디오 스트림을 찾았습니다.: %d\n"
#define MSGTR_DetectedTV "TV를 발견하였습니다! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "ogg 해석기를 열 수 없습니다.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: 오디오 스트림(id:%d)을 찾고 있습니다.\n"
#define MSGTR_CannotOpenAudioStream "오디오 스트림을 열 수 없습니다.: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "자막 스트림을 열 수 없습니다.: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "오디오 해석기를 여는데 실패했습니다.: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "자막 해석기를 여는데 실패했습니다.: %s\n"
#define MSGTR_TVInputNotSeekable "TV 입력을 찾을 수 없습니다! (채널을 바꾸고 하면 될수도 있습니다. ;)\n"
#define MSGTR_ClipInfo "클립 정보: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 30fps NTSC 항목을 감지하여, 프레임속도를 바꿉니다.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 24fps progressive NTSC 항목을 감지하여, 프레임속도를 바꿉니다.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "코덱을 열 수 없습니다.\n"
#define MSGTR_CantCloseCodec "코덱을 닫을 수 없습니다.\n"

#define MSGTR_MissingDLLcodec "오류: 요청한 DirectShow 코덱 %s를 열 수 없습니다.\n"
#define MSGTR_ACMiniterror "Win32/ACM 오디오 코덱을 열거나 초기화할 수 없습니다. (DLL 파일이 없나요?)\n"
#define MSGTR_MissingLAVCcodec "libavcodec에서 '%s' 코덱을 찾을 수 없습니다...\n"

#define MSGTR_MpegNoSequHdr "MPEG: 치명적 오류: 시퀀스 헤더를 찾는 도중 EOF.\n"
#define MSGTR_CannotReadMpegSequHdr "치명적 오류: 시퀀스 헤더를 읽을 수 없습니다.\n"
#define MSGTR_CannotReadMpegSequHdrEx "치명적 오류: 시퀀스 헤더 확장을 읽을 수 없습니다.\n"
#define MSGTR_BadMpegSequHdr "MPEG: 잘못된 시퀀스 헤더입니다.\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: 잘못된 시퀀스 헤더 확장입니다.\n"

#define MSGTR_ShMemAllocFail "공유 메모리를 할당할 수 없습니다.\n"
#define MSGTR_CantAllocAudioBuf "오디오 출력 버퍼를 할당할 수 없습니다.\n"

#define MSGTR_UnknownAudio "알 수 없는 오디오 형식입니다. -> 소리없음\n"

#define MSGTR_UsingExternalPP "[PP] 외부 후행처리 필터를 사용합니다. max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] 코덱의 후행처리를 사용합니다. max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "선택된 vo & vd가 비디오 속성 '%s'을(를) 지원하지 않습니다. \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "요청한 비디오 코덱 집합 [%s] (vfm=%s)을(를) 사용할 수 없습니다. (컴파일시에 가능하도록 설정하세요.)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "요청한 오디오 코텍 집합 [%s] (afm=%s)을(를) 사용할 수 없습니다. (컴파일시에 가능하도록 설정하세요.)\n"
#define MSGTR_OpeningVideoDecoder "비디오 디코더를 열고 있습니다.: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "오디오 디코더를 열고 있습니다.: [%s] %s\n"
#define MSGTR_UninitVideoStr "비디오 초기화를 취소합니다.: %s\n"
#define MSGTR_UninitAudioStr "오디오 초기화를 취소합니다.: %s\n"
#define MSGTR_VDecoderInitFailed "비디오 디코더 초기화를 실패했습니다. :(\n"
#define MSGTR_ADecoderInitFailed "오디오 디코더 초기화를 실패했습니다. :(\n"
#define MSGTR_ADecoderPreinitFailed "오디오 디코더 사전 초기화를 실패했습니다. :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: 입력 버퍼로 %d 바이트를 할당합니다.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: 출력 버퍼로 %d + %d = %d 바이트를 할당합니다.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "LIRC 지원을 시작합니다...\n"
#define MSGTR_LIRCopenfailed "LIRC 지원 시작을 실패했습니다.\n"
#define MSGTR_LIRCcfgerr "LIRC 설정파일 %s를 읽는데 실패했습니다.\n"

// vf.c:
#define MSGTR_CouldNotFindVideoFilter "비디오 필터 '%s'을(를) 찾을 수 없습니다.\n"
#define MSGTR_CouldNotOpenVideoFilter "비디오 필터 '%s'을(를) 열 수 없습니다.\n"
#define MSGTR_OpeningVideoFilter "비디오 필터를 열고 있습니다.: "
#define MSGTR_CannotFindColorspace "'scale'을 시도했지만, 맞는 컬러공간을 찾을 수 없습니다. :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: 코덱이 sh->disp_w와 sh->disp_h로 설정되지 않아서, 다시 시도합니다.\n"
#define MSGTR_VoConfigRequest "VDec: vo 설정 요청 - %d x %d (선호하는 csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "어울리는 컬러공간을 찾을 수 없습니다. -vf 크기조절로 다시 시도합니다...\n"
#define MSGTR_MovieAspectIsSet "화면비율이 %.2f:1 입니다. - 화면비율을 조정하기위해 사전 크기조절을 합니다.\n"
#define MSGTR_MovieAspectUndefined "화면비율이 정의되지 않았습니다. - 사전 크기조절이 적용되지 않았습니다.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: EWMH 전체화면 이벤트를 보낼 수 없습니다!\n"


// ====================== GUI messages/buttons ========================

#ifdef CONFIG_GUI

// --- labels ---
#define MSGTR_About "정보"
#define MSGTR_FileSelect "파일 선택..."
#define MSGTR_SubtitleSelect "자막 선택..."
#define MSGTR_OtherSelect "선택..."
#define MSGTR_AudioFileSelect "음악 파일 선택..."
#define MSGTR_FontSelect "글꼴 선택..."
#define MSGTR_PlayList "재생목록"
#define MSGTR_Equalizer "이퀄라이저"
#define MSGTR_SkinBrowser "스킨 찾기"
#define MSGTR_Network "네트워크 스트리밍..."
#define MSGTR_Preferences "선택사항"
#define MSGTR_NoMediaOpened "미디어 없음"
#define MSGTR_VCDTrack "VCD 트랙 %d"
#define MSGTR_NoChapter "챕터 없음"
#define MSGTR_Chapter "챕터 %d"
#define MSGTR_NoFileLoaded "파일 없음"

// --- buttons ---
#define MSGTR_Ok "확인"
#define MSGTR_Cancel "취소"
#define MSGTR_Add "추가"
#define MSGTR_Remove "삭제"
#define MSGTR_Clear "지움"
#define MSGTR_Config "설정"
#define MSGTR_ConfigDriver "드라이버 설정"
#define MSGTR_Browse "열기"

// --- error messages ---
#define MSGTR_NEMDB "죄송합니다. 그리기 버퍼를 위한 충분한 메모리가 없습니다."
#define MSGTR_NEMFMR "죄송합니다. 메뉴 렌더링을 위한 충분한 메모리가 없습니다."
#define MSGTR_IDFGCVD "죄송합니다. GUI 호환 비디오 출력 드라이버를 찾지 못했습니다."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[스킨] 스킨 설정파일의 %d번째 줄에 오류가 있습니다.: %s" 
#define MSGTR_SKIN_WARNING1 "[스킨] 설정파일의 %d번째 줄 경고:\n위젯(%s)을 찾았지만 그 앞에 \"section\"을 찾을 수 없습니다."
#define MSGTR_SKIN_WARNING2 "[스킨] 설정파일의 %d번째 줄 경고:\n위젯(%s)을 찾았지만 그 앞에 \"subsection\"을 찾을 수 없습니다."
#define MSGTR_SKIN_WARNING3 "[스킨] 설정파일의 %d번째 줄 경고:\n이 subsection은 현재 위젯에서 지원되지 않습니다. (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16 비트 혹은 더 낮은 품질의 비트맵은 지원되지 않습니다. (%s)\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "파일을 찾을 수 없습니다. (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP 읽기 오류입니다. (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA 읽기 오류입니다. (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG 읽기 오류입니다. (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE로 압축된 TGA는 지원되지 않습니다. (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "알 수 없는 파일 형식입니다. (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "24 비트에서 32 비트로 전환 오류 (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "알 수 없는 메세지입니다.: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "메모리가 부족합니다.\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "폰트가 너무 많이 선언되어 있습니다.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "폰트파일을 찾을 수 없습니다.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "폰트 이미지파일을 찾을 수 없습니다.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "폰트 지정자가 존재하지 않습니다. (%s)\n"
#define MSGTR_SKIN_UnknownParameter "알 수 없는 매개변수입니다. (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "스킨을 찾을 수 없습니다. (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "스킨 설정파일 읽기 오류입니다. (%s)\n"
#define MSGTR_SKIN_LABEL "스킨:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "MPlayer 정보"
#define MSGTR_MENU_Open "열기..."
#define MSGTR_MENU_PlayFile "파일 재생..."
#define MSGTR_MENU_PlayVCD "VCD 재생..."
#define MSGTR_MENU_PlayDVD "DVD 재생..."
#define MSGTR_MENU_PlayURL "URL 재생..."
#define MSGTR_MENU_LoadSubtitle "자막 선택..."
#define MSGTR_MENU_DropSubtitle "자막 없앰..."
#define MSGTR_MENU_LoadExternAudioFile "음악 파일..."
#define MSGTR_MENU_Playing "작동"
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
#define MSGTR_MENU_PlayDisc "디스크 열기..."
#define MSGTR_MENU_ShowDVDMenu "DVD 메뉴보기"
#define MSGTR_MENU_Titles "타이틀"
#define MSGTR_MENU_Title "타이틀 %2d"
#define MSGTR_MENU_None "(없음)"
#define MSGTR_MENU_Chapters "챕터"
#define MSGTR_MENU_Chapter "챕터 %2d"
#define MSGTR_MENU_AudioLanguages "오디오 언어"
#define MSGTR_MENU_SubtitleLanguages "자막 언어"
#define MSGTR_MENU_SkinBrowser "스킨선택"
#define MSGTR_MENU_Exit "종료..."
#define MSGTR_MENU_Mute "음소거"
#define MSGTR_MENU_Original "원래대로"
#define MSGTR_MENU_AspectRatio "화면비율"
#define MSGTR_MENU_AudioTrack "오디오 트랙"
#define MSGTR_MENU_Track "트랙 %d"
#define MSGTR_MENU_VideoTrack "비디오 트랙"

// --- equalizer
#define MSGTR_EQU_Audio "오디오"
#define MSGTR_EQU_Video "비디오"
#define MSGTR_EQU_Contrast "대비: "
#define MSGTR_EQU_Brightness "밝기: "
#define MSGTR_EQU_Hue "색상: "
#define MSGTR_EQU_Saturation "채도: "
#define MSGTR_EQU_Front_Left "왼쪽 앞"
#define MSGTR_EQU_Front_Right "오른쪽 앞"
#define MSGTR_EQU_Back_Left "왼쪽 뒤"
#define MSGTR_EQU_Back_Right "오른쪽 뒤"
#define MSGTR_EQU_Center "가운데"
#define MSGTR_EQU_Bass "베이스"
#define MSGTR_EQU_All "모두"
#define MSGTR_EQU_Channel1 "채널 1:"
#define MSGTR_EQU_Channel2 "채널 2:"
#define MSGTR_EQU_Channel3 "채널 3:"
#define MSGTR_EQU_Channel4 "채널 4:"
#define MSGTR_EQU_Channel5 "채널 5:"
#define MSGTR_EQU_Channel6 "채널 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "경로"
#define MSGTR_PLAYLIST_Selected "선택된 파일"
#define MSGTR_PLAYLIST_Files "파일"
#define MSGTR_PLAYLIST_DirectoryTree "디렉토리"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "자막 & OSD"
#define MSGTR_PREFERENCES_Codecs "코덱 & 해석기"
#define MSGTR_PREFERENCES_Misc "기타"

#define MSGTR_PREFERENCES_None "없음"
#define MSGTR_PREFERENCES_AvailableDrivers "가능한 드라이버:"
#define MSGTR_PREFERENCES_DoNotPlaySound "사운드 재생 안함"
#define MSGTR_PREFERENCES_NormalizeSound "사운드 표준화"
#define MSGTR_PREFERENCES_EnableEqualizer "이퀄라이저 사용"
#define MSGTR_PREFERENCES_ExtraStereo "외부 스테레오 사용"
#define MSGTR_PREFERENCES_Coefficient "계수:"
#define MSGTR_PREFERENCES_AudioDelay "오디오 지연:"
#define MSGTR_PREFERENCES_DoubleBuffer "이중 버퍼링 사용"
#define MSGTR_PREFERENCES_DirectRender "다이렉트 렌더링 사용"
#define MSGTR_PREFERENCES_FrameDrop "프레임 건너뛰기 사용"
#define MSGTR_PREFERENCES_HFrameDrop "강제 프레임 건너뛰기 사용(위험함)"
#define MSGTR_PREFERENCES_Flip "이미지 상하 반전"
#define MSGTR_PREFERENCES_Panscan "팬스캔: "
#define MSGTR_PREFERENCES_OSDTimer "타이머 및 표시기"
#define MSGTR_PREFERENCES_OSDProgress "진행 막대만 표시"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "타이머, 퍼센트 및 전체시간"
#define MSGTR_PREFERENCES_Subtitle "자막:"
#define MSGTR_PREFERENCES_SUB_Delay "지연: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "위치: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "자동으로 자막 열지 않기"
#define MSGTR_PREFERENCES_SUB_Unicode "유니코드 자막"
#define MSGTR_PREFERENCES_SUB_MPSUB "주어진 자막을 MPlayer용 자막 형식으로 바꿈"
#define MSGTR_PREFERENCES_SUB_SRT "주어진 자막을 SRT 형식으로 바꿈"
#define MSGTR_PREFERENCES_SUB_Overlap "자막 겹침 켜기"
#define MSGTR_PREFERENCES_Font "글꼴:"
#define MSGTR_PREFERENCES_FontFactor "글꼴 팩터:"
#define MSGTR_PREFERENCES_PostProcess "후행처리 사용"
#define MSGTR_PREFERENCES_AutoQuality "자동 품질조정: "
#define MSGTR_PREFERENCES_NI "non-interleaved AVI 파서 사용"
#define MSGTR_PREFERENCES_IDX "필요한 경우, 인덱스 테이블 다시 만들기"
#define MSGTR_PREFERENCES_VideoCodecFamily "비디오 코덱 집합:"
#define MSGTR_PREFERENCES_AudioCodecFamily "오디오 코덱 집합:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD 레벨"
#define MSGTR_PREFERENCES_FRAME_Subtitle "자막"
#define MSGTR_PREFERENCES_FRAME_Font "글꼴"
#define MSGTR_PREFERENCES_FRAME_PostProcess "후행처리"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "코덱 & 해석기"
#define MSGTR_PREFERENCES_FRAME_Cache "캐시"
#define MSGTR_PREFERENCES_Message "선택사항들을 적용하려면 재생기를 다시 시작해야 합니다!"
#define MSGTR_PREFERENCES_DXR3_VENC "비디오 인코더:"
#define MSGTR_PREFERENCES_DXR3_LAVC "LAVC 사용 (FFmpeg)"
#define MSGTR_PREFERENCES_FontEncoding1 "유니코드"
#define MSGTR_PREFERENCES_FontEncoding2 "서유럽어 (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Euro 포함 서유럽어 (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "슬라브/중앙 유럽어 (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "에스페란토, 갈리시아, 몰타, 터키어 (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "고대 발트 문자셋 (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "키릴 자모 (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "아랍어 (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "현대 그리스어 (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "터키어 (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "발트어 (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "켈트어 (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "히브리어 (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "러시아어 (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "우크라이나, 벨로루시어 (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "중국어 간체 (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "중국어 번체 (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "일본어 (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "한국어 (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "태국어 (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "발트어 Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "슬라브/중앙 유럽어 Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "자동 크기조절 끔"
#define MSGTR_PREFERENCES_FontPropWidth "스크린의 너비에 비례"
#define MSGTR_PREFERENCES_FontPropHeight "스크린의 높이에 비례"
#define MSGTR_PREFERENCES_FontPropDiagonal "스크린의 대각선에 비례"
#define MSGTR_PREFERENCES_FontEncoding "인코딩:"
#define MSGTR_PREFERENCES_FontBlur "흐림:"
#define MSGTR_PREFERENCES_FontOutLine "외곽선:"
#define MSGTR_PREFERENCES_FontTextScale "텍스트 크기조절:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD 크기조절:"
#define MSGTR_PREFERENCES_Cache "캐쉬 켜기/끄기"
#define MSGTR_PREFERENCES_CacheSize "캐시 크기: "
#define MSGTR_PREFERENCES_LoadFullscreen "전체화면으로 시작"
#define MSGTR_PREFERENCES_SaveWinPos "창의 위치 저장"
#define MSGTR_PREFERENCES_XSCREENSAVER "X스크린세이버 정지"
#define MSGTR_PREFERENCES_PlayBar "재생표시줄 사용"
#define MSGTR_PREFERENCES_AutoSync "자동 동기화 켜기/끄기"
#define MSGTR_PREFERENCES_AutoSyncValue "자동 동기화: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM 장치:"
#define MSGTR_PREFERENCES_DVDDevice "DVD 장치:"
#define MSGTR_PREFERENCES_FPS "동영상 FPS:"
#define MSGTR_PREFERENCES_ShowVideoWindow "정지 중일 때 비디오 창 보이기"

#define MSGTR_ABOUT_UHU "GUI 개발 지원: UHU Linux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "치명적 오류!"
#define MSGTR_MSGBOX_LABEL_Error "오류!"
#define MSGTR_MSGBOX_LABEL_Warning "경고!" 

#endif
