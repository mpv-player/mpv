// Sync'ed on 2003-07-26 with help_mp-en.h 1.121
// Translated by Kenneth Chan <chantk@ctk.sytes.net>
// With reference from help_mp-zh.h
// Synced by Lu Ran <hephooey@fastmail.fm>

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"用法:   mplayer [options] [url|path/]filename\n"
"\n"
"基本選項: (完整的選項列表請見 man page)\n"
" -vo <drv[:dev]>  選擇視訊輸出驅動程式及裝置 (用 '-vo help' 查看列表)\n"
" -ao <drv[:dev]>  選擇音效輸出驅動程式及裝置 (用 '-ao help' 查看列表)\n"
#ifdef HAVE_VCD
" vcd://<trackno>   從裝置而並非一般檔案播放 VCD (Video CD) track\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titleno>   從裝置而並非一般檔案播放 DVD title\n"
" -alang/-slang    選擇 DVD 音效/字幕的語言 (使用兩位的國家代號)\n"
#endif
" -ss <timepos>    搜索至指定 (秒或 hh:mm:ss) 的位置\n"
" -nosound         不播放聲音\n"
" -fs              全螢幕播放 (或 -vm, -zoom，詳細內容請見 man page)\n"
" -x <x> -y <y>    設定顯示解析度 (與 -vm 或 -zoom 同時使用)\n"
" -sub <file>      指定使用的字幕檔 (請參見 -subfps, -subdelay)\n"
" -playlist <file> 指定播放列檔\n"
" -vid x -aid y    選擇播放的視訊 (x) 及音效 (y) 串流\n"
" -fps x -srate y  改變視訊 (x fps) 及 音效 (y Hz) 率\n"
" -pp <quality>    使用後期處理濾鏡 (詳細內容請見 man page)\n"
" -framedrop       使用 frame dropping (用於慢機器)\n"
"\n"
"基本控制鍵: (完整的列表請見 man page, 同時請查閱 input.conf)\n"
" <-  or  ->       向前/後搜索 10 秒\n"
" up or down       向前/後搜索 1 分鐘\n"
" pgup or pgdown   向前/後搜索 10 分鐘\n"
" < or >           跳至播放列中的前/後一首\n"
" p or SPACE       暫停播放 (按任意鍵繼續)\n"
" q or ESC         停止播放並離開\n"
" + or -           調整音效延遲 +/- 0.1 秒\n"
" o                循環 OSD 模式:  無顯示 / 搜尋桿 / 搜尋桿+計時器\n"
" * or /           提高或降低 PCM 音量\n"
" z or x           調整字幕延遲 +/- 0.1 秒\n"
" r or t           上/下調整字幕位置, 請見 -vf expand\n"
"\n"
" * * * 詳細內容, 進一步(進階)選項及控制鍵請見 MAN PAGE * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\n正在退出...\n"
#define MSGTR_ExitingHow "\n正在退出... (%s)\n"
#define MSGTR_Exit_quit "離開"
#define MSGTR_Exit_eof "檔案末端"
#define MSGTR_Exit_error "致命錯誤"
//#define MSGTR_IntBySignal "\nMPlayer 被 %s 模組中的 %d 訊號 中斷\n"  // wrong order of format identifiers breaks compilation
#define MSGTR_NoHomeDir "無法找到 HOME 目錄\n"
#define MSGTR_GetpathProblem "get_path(\"config\") 問題\n"
#define MSGTR_CreatingCfgFile "建立 config 檔: %s\n"
#define MSGTR_CopyCodecsConf "(把 etc/codecs.conf 從 MPlayer 原程式碼中複製/建立連接至 ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "使用內建預設的 codecs.conf。\n"
#define MSGTR_CantLoadFont "無法載入字型: %s\n"
#define MSGTR_CantLoadSub "無法載入字幕: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: 致命錯誤: 選擇的串流並不存在!\n"
#define MSGTR_CantOpenDumpfile "無法開啟 dump 檔。\n"
#define MSGTR_CoreDumped "Core dumped ;)\n"
#define MSGTR_FPSnotspecified "FPS 並未在標頭內指定或是無效，請使用 -fps 選項。\n"
#define MSGTR_TryForceAudioFmtStr "正嘗試強行指定音效解碼驅動程式組群 %s...\n"
#define MSGTR_CantFindAudioCodec "無法為音效格式 0x%X 找到解碼器。\n"
#define MSGTR_RTFMCodecs "參閱DOCS/zh/codecs.html﹗\n"
#define MSGTR_TryForceVideoFmtStr "正嘗試強行指定視訊解碼驅動程式組群 %s...\n"
#define MSGTR_CantFindVideoCodec "無法為所選擇的 -vo 與視訊格式 0x%X 找到適合的解碼器。\n"
#define MSGTR_VOincompCodec "所選擇的 video_out 裝置與這個解碼器並不兼容。\n"
#define MSGTR_CannotInitVO "致命錯誤: 無法初始化視訊驅動程式。\n"
#define MSGTR_CannotInitAO "無法開啟/初始化音效裝置 -> 沒有聲音。\n"
#define MSGTR_StartPlaying "開始播放...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"           ************************************************\n"\
"           ****             你的系統太慢了﹗           ****\n"\
"           ************************************************\n\n"\
"可能的原因、問題、解決辦法:\n"\
"- 最普遍的原因: 損壞了/有蟲的_音效_驅動程式\n"\
"  - 可嘗試 -ao sdl 或使用 ALSA 0.5 或 ALSA 0.9 之 OSS 模擬器。\n"\
"  - 試用不同的 -autosync 值, 不妨從 30 開始。\n"\
"- 視訊輸出太慢\n"\
"  - 可試用不同的 -vo driver (-vo help 有列表) 或試試 -framedrop!\n"\
"- CPU 太慢\n"\
"  - 不要試圖在慢的CPU上播大的 DVD/DivX! 試用 -hardframedrop。\n"\
"- 損壞了的檔案\n"\
"  - 可試試不同組合的 -nobps -ni -forceidx -mc 0。\n"\
"- 媒體太慢 (NFS/SMB mounts, DVD, VCD 等等)\n"\
"  - 可試試 -cache 8192。\n"\
"- 你是否正使用 -cache 選項來播放一個非交錯式 AVI 檔案?\n"\
"  - 可試試 -nocache。\n"\
"要取得調整/加速的秘訣請參閱 DOCS/zh/video.html 與 DOCS/zh/sound.html。\n"\
"假如以上沒一個幫得上，請參閱 DOCS/zh/bugreports.html。\n\n"

#define MSGTR_NoGui "MPlayer 編譯並無 GUI 支援。\n"
#define MSGTR_GuiNeedsX "MPlayer GUI 需要 X11。\n"
#define MSGTR_Playing "正在播放 %s。\n"
#define MSGTR_NoSound "音效: 沒有聲音\n"
#define MSGTR_FPSforced "FPS 被指定為 %5.3f  (ftime: %5.3f)。\n"
#define MSGTR_CompiledWithRuntimeDetection "編譯包括了執行時期CPU偵查 - 警告 - 這並非最佳化!\n要獲得最佳表現，加上 --disable-runtime-cpudetection 選項重新編譯 MPlayer。\n"
#define MSGTR_CompiledWithCPUExtensions "為 x86 CPU 編譯並有 extensions:\n"
#define MSGTR_AvailableVideoOutputDrivers "可用的視訊輸出驅動程式:\n"
#define MSGTR_AvailableAudioOutputDrivers "可用的音效輸出驅動程式:\n"
#define MSGTR_AvailableAudioCodecs "可用的音效 codecs:\n"
#define MSGTR_AvailableVideoCodecs "可用的視訊 codecs:\n"
#define MSGTR_AvailableAudioFm "\n可用的(編譯了的)音效 codec 組/驅動程式:\n"
#define MSGTR_AvailableVideoFm "\n可用的(編譯了的)視訊 codec 組/驅動程式:\n"
#define MSGTR_AvailableFsType "可用的全瑩幕層轉變模式:\n"
#define MSGTR_UsingRTCTiming "正使用 Linux 硬體 RTC 計時(%ldHz)。\n"
#define MSGTR_CannotReadVideoProperties "視訊: 無法讀取內容。\n"
#define MSGTR_NoStreamFound "找不到 stream。\n"
#define MSGTR_ErrorInitializingVODevice "開啟/初始化所選擇的視訊輸出 (-vo) 裝置時發生錯誤。\n"
#define MSGTR_ForcedVideoCodec "強行使用的視訊 codec: %s\n"
#define MSGTR_ForcedAudioCodec "強行使用的音效 codec: %s\n"
#define MSGTR_Video_NoVideo "視訊: 沒有影像\n"
#define MSGTR_NotInitializeVOPorVO "\n致命錯誤: 無法初始化影像過濾器 (-vf) 或 影像輸出 (-vo)。\n"
#define MSGTR_Paused "\n  ====== 暫停 ======\r"
#define MSGTR_PlaylistLoadUnable "\n無法載入播放列 %s。\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- '非法指令'導致 MPlayer 當了。\n"\
"  這可能是我們新的執行時期 CPU 偵查程式碼中的一條臭蟲...\n"\
"  請參閱 DOCS/zh/bugreports.html。\n"
#define MSGTR_Exit_SIGILL \
"- '非法指令'導致 MPlayer 當了。\n"\
"  這通常發生於當你在一個與編譯/最佳化 MPlayer 不同的 CPU 上使用它\n"\
"  所造成的。\n"\
"  檢查一下吧﹗\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- 不良的 CPU/FPU/RAM 應用導致 MPlayer 當了。\n"\
"  可使用 --enable-debug 來重新編譯 MPlayer 並做 'gdb' backtrace 及\n"\
"  disassembly。具體細節請參閱 DOCS/zh/bugreports.html#crash。\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer 當了。 這是不應該發生的。\n"\
"  這可能是在 MPlayer 程式碼 _或_ 你的驅動程式 _或_ 你的 gcc 版本\n"\
"  中有臭蟲。假如你認為是 MPlayer 的毛病，請參閱 \n"\
"  DOCS/zh/bugreports.html 並跟從其步驟。除非你在報告懷疑是臭蟲時\n"\
"  能提供這些資料，否則我們將無法及不會幫忙。\n"


// mencoder.c:

#define MSGTR_UsingPass3ControlFile "正在使用 pass3 控制檔: %s\n"
#define MSGTR_MissingFilename "\n沒有檔案名稱。\n\n"
#define MSGTR_CannotOpenFile_Device "無法開啟檔案/裝置。\n"
#define MSGTR_CannotOpenDemuxer "無法開啟 demuxer。\n"
#define MSGTR_NoAudioEncoderSelected "\n沒有選擇音效編碼器 (-oac)。請選擇一個 (可用 -oac help) 或使用 -nosound。\n"
#define MSGTR_NoVideoEncoderSelected "\n沒有選擇視訊編碼器 (-ovc)。請選擇一個 (可用 -oac help)。\n"
#define MSGTR_CannotOpenOutputFile "無法開啟輸出檔 '%s'。\n"
#define MSGTR_EncoderOpenFailed "無法開啟編碼器。\n"
#define MSGTR_ForcingOutputFourcc "強行輸出 fourcc 到 %x [%.4s]\n"
#define MSGTR_DuplicateFrames "\n有 %d 格重覆﹗\n"
#define MSGTR_SkipFrame "\n跳過這一格﹗\n"
#define MSGTR_ErrorWritingFile "%s: 寫入檔案有錯誤。\n"
#define MSGTR_RecommendedVideoBitrate "%s CD 所建議之視訊 bitrate: %d\n"
#define MSGTR_VideoStreamResult "\n視訊串流: %8.3f kbit/s  (%d B/s)  大少: %"PRIu64" bytes  %5.3f 秒 %d 格\n"
#define MSGTR_AudioStreamResult "\n音效串流: %8.3f kbit/s  (%d B/s)  大少: %"PRIu64" bytes  %5.3f 秒\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     變動 bitrate 方式\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(預設)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           平均 bitrate\n"\
"\n"\
" cbr           固定 bitrate\n"\
"               在往後的 ABR 預校模式中亦強行使用 CBR 編碼模式。\n"\
"\n"\
" br=<0-1024>   以 kBit 為單位指定 bitrate (僅適用於 CBR 及 ABR)\n"\
"\n"\
" q=<0-9>       質素 (0-最高, 9-最低) (僅適用於 VBR)\n"\
"\n"\
" aq=<0-9>      計算法質素 (0-最好/最慢, 9-最差/最快)\n"\
"\n"\
" ratio=<1-100> 壓縮比率\n"\
"\n"\
" vol=<0-10>    設定音效輸入增長\n"\
"\n"\
" mode=<0-3>    (預設: 自動)\n"\
"                0: 立體聲\n"\
"                1: 連接立體聲\n"\
"                2: 雙聲道\n"\
"                3: 單聲道\n"\
"\n"\
" padding=<0-2>\n"\
"                0: 無\n"\
"                1: 全部\n"\
"                2: 調校\n"\
"\n"\
" fast          在往後的 VBR 預校模式中均啟用較快的 encoding，\n"\
"               些微較低質素及較高 bitrates。\n"\
"\n"\
" preset=<value> 提供最高可能的質素設定。\n"\
"                 medium: VBR  編碼,  好質素\n"\
"                 (150-180 kbps bitrate 範圍)\n"\
"                 standard:  VBR 編碼, 高質素\n"\
"                 (170-210 kbps bitrate 範圍)\n"\
"                 extreme: VBR 編碼, 非常高質素\n"\
"                 (200-240 kbps bitrate 範圍)\n"\
"                 insane:  CBR  編碼, 最高質素\n"\
"                 (320 kbps bitrate)\n"\
"                 <8-320>: 以所提供的為平均 kbps bitrate ABR 編碼。\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "找不到 CD-ROM 裝置 '%s'。\n"
#define MSGTR_ErrTrackSelect "錯誤選擇 VCD 軌。"
#define MSGTR_ReadSTDIN "從 stdin 讀取...\n"
#define MSGTR_UnableOpenURL "無法開啟 URL: %s\n"
#define MSGTR_ConnToServer "已接駁到伺服器: %s\n"
#define MSGTR_FileNotFound "找不到檔案: '%s'\n"

#define MSGTR_SMBInitError "無法初始 libsmbclient library: %d\n"
#define MSGTR_SMBFileNotFound "無法從 LAN: '%s' 開啟\n"
#define MSGTR_SMBNotCompiled "MPlayer 編譯並無讀取 SMB 支援。\n"

#define MSGTR_CantOpenDVD "無法開啟 DVD 裝置: %s\n"
#define MSGTR_DVDnumTitles "這片 DVD 內有 %d 個 titles。\n"
#define MSGTR_DVDinvalidTitle "無效的 DVD title 號數: %d\n"
#define MSGTR_DVDnumChapters "這個 DVD title 內有 %d 個 chapters。\n"
#define MSGTR_DVDinvalidChapter "無效的 DVD chapter 號數: %d\n"
#define MSGTR_DVDnumAngles "這個 DVD title 內有 %d 個角度。\n"
#define MSGTR_DVDinvalidAngle "無效的 DVD 角度號碼: %d\n"
#define MSGTR_DVDnoIFO "無法為 DVD title %d 開啟 IFO 檔。\n"
#define MSGTR_DVDnoVOBs "無法開啟 title VOBS (VTS_%02d_1.VOB)。\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "警告: 音效串流標頭 %d 從新定義。\n"
#define MSGTR_VideoStreamRedefined "警告: 視訊串流標頭 %d 從新定義。\n"
#define MSGTR_TooManyAudioInBuffer "\n緩衝區有太多音效封包: (%d bytes 中有 %d 個)。\n"
#define MSGTR_TooManyVideoInBuffer "\n緩衝區有太多視訊封包: (%d bytes 中有 %d 個)。\n"
#define MSGTR_MaybeNI "可能你在播放一個非交錯式的串流/檔，或者是 codec 失敗了﹖\n" \
		      "如果是 AVI 檔，可以試用 -ni 選項來執行非交錯模式。\n"
#define MSGTR_SwitchToNi "\n偵測到交錯得很厲害的 AVI 檔 - 轉換到 -ni 模式...\n"
#define MSGTR_Detected_XXX_FileFormat "偵測到 %s 檔格式。\n"
#define MSGTR_DetectedAudiofile "偵測到音效檔。\n"
#define MSGTR_NotSystemStream "並非 MPEG 系統串流格式... (可能是輸送串流﹖)\n"
#define MSGTR_InvalidMPEGES "無效的 MPEG-ES 串流??? 這可能是一隻臭蟲，請聯絡作者 :(\n"
#define MSGTR_FormatNotRecognized "============ 很抱歉，這個檔案格式不能被辨認/不支援 =============\n"\
				  "=== 如果這是個 AVI 檔、ASF 或 MPEG 串流，請聯絡作者！ ===\n"
#define MSGTR_MissingVideoStream "找不到視訊串流。\n"
#define MSGTR_MissingAudioStream "找不到音效串流 -> 無聲音。\n"
#define MSGTR_MissingVideoStreamBug "缺少了視訊串流!? 這可能是個臭蟲，請聯絡作者 :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: 檔案內並無所選擇的音效或視訊串流。\n"

#define MSGTR_NI_Forced "強行使用"
#define MSGTR_NI_Detected "偵測到"
#define MSGTR_NI_Message "%s 非交錯式 AVI 檔案格式。\n"

#define MSGTR_UsingNINI "正使用非交錯式損壞了的 AVI 檔案格式。\n"
#define MSGTR_CouldntDetFNo "無法 (為準確搜尋) 確定格數。\n"
#define MSGTR_CantSeekRawAVI "無法在不完整的 AVI 串流中作搜尋。(需要索引，試用 -idx 選項。)\n"
#define MSGTR_CantSeekFile "無法在這檔案中作搜尋。\n"

#define MSGTR_MOVcomprhdr "MOV: 壓縮的標頭的支援需要ZLIB﹗\n"
#define MSGTR_MOVvariableFourCC "MOV: 警告: 偵測到變動的 FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: 警告: 有太多的音軌"
#define MSGTR_FoundAudioStream "==> 找到音效串流: %d\n"
#define MSGTR_FoundVideoStream "==> 找到視訊串流: %d\n"
#define MSGTR_DetectedTV "偵測到有 TV ﹗ ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "無法開啟 ogg demuxer。\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: 正在搜尋音效串流 (id:%d)。\n"
#define MSGTR_CannotOpenAudioStream "無法開啟音效串流: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "無法開啟字幕串流: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "無法成功開啟音效 demuxer: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "無法成功開啟字幕 demuxer: %s\n"
#define MSGTR_TVInputNotSeekable "TV 輸入不能搜索﹗(搜索可能是用來轉換頻道 ;)\n"
#define MSGTR_ClipInfo "片段資料:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 偵測到30fps的NTSC內容，改變幀速率。\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 偵測到24fps的漸近NTSC內容，改變幀速率。\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "無法開啟 codec。\n"
#define MSGTR_CantCloseCodec "無法關閉 codec。\n"

#define MSGTR_MissingDLLcodec "錯誤: 無法開啟所需的 DirectShow codec %s。\n"
#define MSGTR_ACMiniterror "無法載入/初始 Win32/ACM AUDIO codec (DLL 檔失蹤了﹖)。\n"
#define MSGTR_MissingLAVCcodec "無法在 libavcodec 內找到 codec '%s'...\n"

#define MSGTR_MpegNoSequHdr "MPEG: 致命錯誤: 當搜找 sequence header 時遇到檔案末 (EOF)。\n"
#define MSGTR_CannotReadMpegSequHdr "致命錯誤: 無法讀取 sequence header。\n"
#define MSGTR_CannotReadMpegSequHdrEx "致命錯誤: 無法讀取 sequence header extension。\n"
#define MSGTR_BadMpegSequHdr "MPEG: 很差的 sequence header\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: 很差的 sequence header extension\n"

#define MSGTR_ShMemAllocFail "無法分配共用記憶。\n"
#define MSGTR_CantAllocAudioBuf "無法分配 audio out 緩衝區。\n"

#define MSGTR_UnknownAudio "不知明/找不到的音效格式 -> 沒有聲音\n"

#define MSGTR_UsingExternalPP "[PP] 正使用外置的後期處理過濾器，最大的 q = %d。\n"
#define MSGTR_UsingCodecPP "[PP] 正使用 codec 之後期處理，最大的 q = %d。\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "影像屬性 '%s' 不被所選擇的 vo 及 vd 支援。\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "沒有所要求的 codec 族群 [%s] (vfm=%s)。\n請在編譯時選定。\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "沒有所要求的 codec 族群 [%s] (afm=%s)。\n請在編譯時選定。\n"
#define MSGTR_OpeningVideoDecoder "正在開啟影像解碼器: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "正在開啟音效解碼器: [%s] %s\n"
#define MSGTR_UninitVideoStr "未初始視訊: %s\n"
#define MSGTR_UninitAudioStr "未初始音效: %s\n"
#define MSGTR_VDecoderInitFailed "VDecoder 初始失敗 :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder 初始失敗 :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder 預先初始失敗 :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: 正在分配 %d bytes 給輸入緩衝區\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: 正在分配 %d + %d = %d bytes 給輸出緩衝區\n"

// LIRC:
#define MSGTR_SettingUpLIRC "正在設定 LIRC 支援...\n"
#define MSGTR_LIRCopenfailed "無法開啟 LIRC 支援。\n"
#define MSGTR_LIRCcfgerr "讀取 LIRC 設定檔 %s 失敗。\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "無法找到影像過濾器 '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "無法開啟影像過濾器 '%s'\n"
#define MSGTR_OpeningVideoFilter "開啟影像過濾器: "
#define MSGTR_CannotFindColorspace "無法找到共用的 colorspace，即使加入 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Codec 沒有設定 sh->disp_w 及 sh->disp_h，正嘗試解決辦法。\n"
#define MSGTR_VoConfigRequest "VDec: vo 設定要求 — %d x %d (喜好的 csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "無法找到配合的 colorspace — 用 -vf scale 再嘗試...\n"
#define MSGTR_MovieAspectIsSet "電影比例是 %.2f:1 — 使用 prescaling 調校至正確比例。\n"
#define MSGTR_MovieAspectUndefined "電影比例未有說明 — 並無使用 prescaling。\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "你需要升級/安裝二進制codecs包。\n請訪問http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Win32/DShow影像codec初始OK。\n"
#define MSGTR_DMOInitOK "INFO: Win32/DMO影像codec初始。\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: 無法發送EWMH全螢幕事件！\n"


// ====================== GUI messages/buttons ============================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "關於"
#define MSGTR_FileSelect "選擇檔案..."
#define MSGTR_SubtitleSelect "選擇字幕..."
#define MSGTR_OtherSelect "選擇..."
#define MSGTR_AudioFileSelect "選擇外置音效頻道..."
#define MSGTR_FontSelect "選擇字型..."
#define MSGTR_PlayList "播放列"
#define MSGTR_Equalizer "平衡器"
#define MSGTR_SkinBrowser "佈景瀏覽器"
#define MSGTR_Network "網路串流..."
#define MSGTR_Preferences "喜好設定"
#define MSGTR_NoMediaOpened "沒有媒體開啟。"
#define MSGTR_VCDTrack "VCD 第 %d 軌"
#define MSGTR_NoChapter "沒有 chapter"
#define MSGTR_Chapter "Chapter %d"
#define MSGTR_NoFileLoaded "沒有載入檔案。"

// --- buttons ---
#define MSGTR_Ok "確定"
#define MSGTR_Cancel "取消"
#define MSGTR_Add "加入"
#define MSGTR_Remove "移除"
#define MSGTR_Clear "清除"
#define MSGTR_Config "設定"
#define MSGTR_ConfigDriver "設定驅動程式"
#define MSGTR_Browse "瀏覽"

// --- error messages ---
#define MSGTR_NEMDB "很抱歉，繪圖援衝區沒有足夠記憶。"
#define MSGTR_NEMFMR "很抱歉，目錄顯示沒有足夠記憶。"
#define MSGTR_IDFGCVD "很抱歉，找不到一個 GUI 兼容的視訊輸出驅動程式。"

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] 於 skin 設定檔 %d: %s 行出錯"
#define MSGTR_SKIN_WARNING1 "[skin] 警告，於 skin 設定檔 %d 行: 找到 widget 但在這之前找不到 \"section\" (%s)"
#define MSGTR_SKIN_WARNING2 "[skin] 警告，於 skin 設定檔 %d 行: 找到 widget 但在這之前找不到 \"subsection\" (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] 警告，於 skin 設定檔 %d 行: 這個 widget 並不支援這個 subsection (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "不支援 16 位元或以下之色彩點陣 (%s)。\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "找不到檔案 (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP 讀取錯誤 (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA 讀取錯誤 (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG 讀取錯誤 (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE 壓縮的 TGA 並不支援 (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "不明的檔案類別 (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "24 位元至 32 位元轉換錯誤 (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "不明的訊息: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "記憶體不足\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "宣告了太多字型。\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "找不到字型檔。\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "找不到字型形像檔。\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "並不存在的字型識別器 (%s)\n"
#define MSGTR_SKIN_UnknownParameter "不明的參數 (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "找不到 skin (%s)。\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin 設定檔讀取錯誤 (%s)。\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "關於 MPlayer"
#define MSGTR_MENU_Open "開啟..."
#define MSGTR_MENU_PlayFile "播放檔案..."
#define MSGTR_MENU_PlayVCD "播放 VCD..."
#define MSGTR_MENU_PlayDVD "播放 DVD..."
#define MSGTR_MENU_PlayURL "播放 URL..."
#define MSGTR_MENU_LoadSubtitle "載入字幕..."
#define MSGTR_MENU_DropSubtitle "撤消字幕..."
#define MSGTR_MENU_LoadExternAudioFile "載入外置音效檔..."
#define MSGTR_MENU_Playing "正在播放"
#define MSGTR_MENU_Play "播放"
#define MSGTR_MENU_Pause "暫停"
#define MSGTR_MENU_Stop "停止"
#define MSGTR_MENU_NextStream "下一個串流"
#define MSGTR_MENU_PrevStream "上一個串流"
#define MSGTR_MENU_Size "大小"
#define MSGTR_MENU_NormalSize "普通大小"
#define MSGTR_MENU_DoubleSize "雙倍大小"
#define MSGTR_MENU_FullScreen "全螢幕"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "開啟光碟..."
#define MSGTR_MENU_ShowDVDMenu "顯示 DVD 目錄"
#define MSGTR_MENU_Titles "標題"
#define MSGTR_MENU_Title "標題 %2d"
#define MSGTR_MENU_None "(無)"
#define MSGTR_MENU_Chapters "Chapters"
#define MSGTR_MENU_Chapter "Chapter %2d"
#define MSGTR_MENU_AudioLanguages "音效語言"
#define MSGTR_MENU_SubtitleLanguages "字幕語言"
// TODO: Why is this different from MSGTR_PlayList?
#define MSGTR_MENU_PlayList "播放列表"
#define MSGTR_MENU_SkinBrowser "Skin 瀏覽器"
#define MSGTR_MENU_Exit "退出..."
#define MSGTR_MENU_Mute "靜音"
#define MSGTR_MENU_Original "原來的"
#define MSGTR_MENU_AspectRatio "影像比率"
#define MSGTR_MENU_AudioTrack "音軌"
#define MSGTR_MENU_Track "第 %d 首"
#define MSGTR_MENU_VideoTrack "影像軌"

// --- equalizer
#define MSGTR_EQU_Audio "音效"
#define MSGTR_EQU_Video "視訊"
#define MSGTR_EQU_Contrast "對比度: "
#define MSGTR_EQU_Brightness "光暗度: "
#define MSGTR_EQU_Hue "色彩度: "
#define MSGTR_EQU_Saturation "飽和度: "
#define MSGTR_EQU_Front_Left "左前"
#define MSGTR_EQU_Front_Right "右前"
#define MSGTR_EQU_Back_Left "左後"
#define MSGTR_EQU_Back_Right "右後"
#define MSGTR_EQU_Center "中置"
#define MSGTR_EQU_Bass "低音"
#define MSGTR_EQU_All "全部"
#define MSGTR_EQU_Channel1 "聲道 1:"
#define MSGTR_EQU_Channel2 "聲道 2:"
#define MSGTR_EQU_Channel3 "聲道 3:"
#define MSGTR_EQU_Channel4 "聲道 4:"
#define MSGTR_EQU_Channel5 "聲道 5:"
#define MSGTR_EQU_Channel6 "聲道 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "路徑"
#define MSGTR_PLAYLIST_Selected "選擇的檔案"
#define MSGTR_PLAYLIST_Files "檔案"
#define MSGTR_PLAYLIST_DirectoryTree "目錄樹"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "字幕及 OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs & demuxer"
#define MSGTR_PREFERENCES_Misc "雜項"

#define MSGTR_PREFERENCES_None "無"
#define MSGTR_PREFERENCES_AvailableDrivers "可用的驅動程式:"
#define MSGTR_PREFERENCES_DoNotPlaySound "不播放聲音"
#define MSGTR_PREFERENCES_NormalizeSound "正常化聲音"
#define MSGTR_PREFERENCES_EnableEqualizer "採用調音器"
#define MSGTR_PREFERENCES_ExtraStereo "採用額外立體聲"
#define MSGTR_PREFERENCES_Coefficient "係數:"
#define MSGTR_PREFERENCES_AudioDelay "音效延遲"
#define MSGTR_PREFERENCES_DoubleBuffer "採用 double buffering"
#define MSGTR_PREFERENCES_DirectRender "採用 direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "採用 frame dropping"
#define MSGTR_PREFERENCES_HFrameDrop "採用 HARD frame dropping (具危險性)"
#define MSGTR_PREFERENCES_Flip "上下倒轉影像"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "計時器及顯示器"
#define MSGTR_PREFERENCES_OSDProgress "僅進度棒"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "計時器、百份比及總共時間"
#define MSGTR_PREFERENCES_Subtitle "字幕:"
#define MSGTR_PREFERENCES_SUB_Delay "延遲: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "位置: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "不會自動載入字幕"
#define MSGTR_PREFERENCES_SUB_Unicode "統一碼字幕"
#define MSGTR_PREFERENCES_SUB_MPSUB "轉換提供的字幕至 MPlayer 的字幕格式"
#define MSGTR_PREFERENCES_SUB_SRT "轉換提供的字幕至時間性的 SubViewer (SRT) 格式"
#define MSGTR_PREFERENCES_SUB_Overlap "開關字幕重疊"
#define MSGTR_PREFERENCES_Font "字型:"
#define MSGTR_PREFERENCES_FontFactor "字型因素:"
#define MSGTR_PREFERENCES_PostProcess "採用後置處理"
#define MSGTR_PREFERENCES_AutoQuality "自動質素: "
#define MSGTR_PREFERENCES_NI "使用非交錯式 AVI 語法分析器"
#define MSGTR_PREFERENCES_IDX "如有需要，重新建立索引表"
#define MSGTR_PREFERENCES_VideoCodecFamily "視訊 codec 家族:"
#define MSGTR_PREFERENCES_AudioCodecFamily "音效 codec 家族:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD 度"
#define MSGTR_PREFERENCES_FRAME_Subtitle "字幕"
#define MSGTR_PREFERENCES_FRAME_Font "字型"
#define MSGTR_PREFERENCES_FRAME_PostProcess "後置處理"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "快取記憶"
#define MSGTR_PREFERENCES_Message "請記得某些選項要重新播放才會生效﹗"
#define MSGTR_PREFERENCES_DXR3_VENC "視訊 encoder:"
#define MSGTR_PREFERENCES_DXR3_LAVC "使用 LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_FontEncoding1 "統一碼"
#define MSGTR_PREFERENCES_FontEncoding2 "西歐語系 (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "西歐語系包含歐羅符號 (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "斯拉夫/中歐語系 (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "世界語、加里西亞語、馬爾他語、土耳其語 (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "舊波羅的海字集 (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "斯拉夫語 (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "阿拉伯語 (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "現代希臘 (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "土耳其語 (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "波羅的海語 (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "克爾特語 (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "希伯來文字集 (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "俄羅斯語 (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "烏克蘭、白俄羅斯語 (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "簡體中文字集 (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "繁體中文字集 (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "日文字集 (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "韓文字集 (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "泰文字集 (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "斯拉夫視窗 (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "斯拉夫/中歐視窗 (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "沒有自動比例"
#define MSGTR_PREFERENCES_FontPropWidth "根據電影闊度比例"
#define MSGTR_PREFERENCES_FontPropHeight "根據電影高度比例"
#define MSGTR_PREFERENCES_FontPropDiagonal "根據電影對角比例"
#define MSGTR_PREFERENCES_FontEncoding "編碼:"
#define MSGTR_PREFERENCES_FontBlur "模糊度:"
#define MSGTR_PREFERENCES_FontOutLine "輪廓:"
#define MSGTR_PREFERENCES_FontTextScale "文字比例:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD 比例:"
#define MSGTR_PREFERENCES_Cache "快取記憶開/關"
#define MSGTR_PREFERENCES_CacheSize "快取記憶大小: "
#define MSGTR_PREFERENCES_LoadFullscreen "全螢幕開始"
#define MSGTR_PREFERENCES_SaveWinPos "儲存視窗位置"
#define MSGTR_PREFERENCES_XSCREENSAVER "停用 XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "使用播放棒"
#define MSGTR_PREFERENCES_AutoSync "自動同步開/關"
#define MSGTR_PREFERENCES_AutoSyncValue "自動同步: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM 裝置:"
#define MSGTR_PREFERENCES_DVDDevice "DVD 裝置:"
#define MSGTR_PREFERENCES_FPS "電影的 FPS:"
#define MSGTR_PREFERENCES_ShowVideoWindow "不活躍時顯示影像視窗"

#define MSGTR_ABOUT_UHU "GUI 開發由 UHU Linux 贊助\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "致命錯誤﹗"
#define MSGTR_MSGBOX_LABEL_Error "錯誤﹗"
#define MSGTR_MSGBOX_LABEL_Warning "警告﹗"

#endif
