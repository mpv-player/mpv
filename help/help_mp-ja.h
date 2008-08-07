// Translated to Japanese.
// Translated by smoker <http://smokerz.net/~smoker/>

// This translation is not complete yet.

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"使い方:   mplayer [オプション] [url|パス/]ファイル名\n"
"\n"
"基本的なオプション: (man page に全て網羅されています)\n"
" -vo <drv[:dev]>  映像出力ドライバ及びデバイスを選択します ('-vo help'で一覧表示されます)\n"
" -ao <drv[:dev]>  音声出力ドライバ及びデバイスを選択します ('-ao help'で一覧表示されます)\n"
#ifdef CONFIG_VCD
" vcd://<trackno>   play VCD (Video CD) track from device instead of plain file\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<titleno>   play DVD title from device instead of plain file\n"
" -alang/-slang    DVDの 音声/サブタイトル 言語設定 (2文字のカントリーコードで指定)\n"
#endif
" -ss <timepos>    timeposに与えられた場所から再生します(seconds or hh:mm:ss)\n"
" -nosound         音声出力を抑止します\n"
" -fs              フルスクリーン表示します(もしくは -vm, -zoom, 詳細はmanにあります)\n"
" -x <x> -y <y>    表示サイズを指定します (一緒に次のオプションを利用下さい -vm or -zoom)\n"
" -sub <file>      利用する subtitle ファイルを選択する(-subfps, -subdelay も御覧下さい)\n"
" -playlist <file> playlistファイルを選択する\n"
" -vid x -aid y    select video (x) and audio (y) stream to play\n"
" -fps x -srate y  change video (x fps) and audio (y Hz) rate\n"
" -pp <quality>    postprocessing filterを有効にする (詳細は man page にあります)\n"
" -framedrop       frame droppingを有効にする (低速なマシン向きです)\n"
"\n"
"基本的なコマンド: (man pageに全て網羅されています。事前にinput.confも確認して下さい)\n"
" <-  or  ->       10秒単位で前後にシークします\n"
" up or down       1分単位で前後にシークします\n"
" pgup or pgdown   10分単位で前後にシークします\n"
" < or >           プレイリストを元に前後のファイルに遷移します\n"
" p or SPACE       再生を静止します(何かボタンを押下すると再生を開始します)\n"
" q or ESC         再生を静止し、プログラムを停止します\n"
" + or -           音声を 0.1 秒単位で早めたり遅れさせたり調整する\n"
" o                cycle OSD mode:  none / seekbar / seekbar + timer\n"
" * or /           PCM 音量を上げたり下げたりする\n"
" z or x           subtitleを 0.1 秒単位で早めたり遅れさせたり調整する\n"
" r or t           subtitleの位置を上げたり下げたり調整する, -vfオプションも確認して下さい\n"
"\n"
" * * * man pageに詳細がありますので、確認して下さい。さらに高度で進んだオプションやキーも記載してます * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\n終了しています\n"
#define MSGTR_ExitingHow "\n終了しています... (%s)\n"
#define MSGTR_Exit_quit "終了"
#define MSGTR_Exit_eof "ファイルの末端です"
#define MSGTR_Exit_error "致命的エラー"
#define MSGTR_IntBySignal "\nMPlayer はシグナル %d によって中断しました．次のモジュールからです: %s\n"
#define MSGTR_NoHomeDir "ホームディレクトリを見付けることが出来ませんでした.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") で問題が起きました\n"
#define MSGTR_CreatingCfgFile "config fileを作成しました: %s\n"
#define MSGTR_BuiltinCodecsConf "組み込まれたデフォルトの codecs.conf を利用してます\n"
#define MSGTR_CantLoadFont "フォントをロード出来ません: %s\n"
#define MSGTR_CantLoadSub "サブタイトルをロード出来ません: %s\n"
#define MSGTR_CantOpenDumpfile "dump fileを開けません\n"
#define MSGTR_CoreDumped "コアダンプ ;)\n"
#define MSGTR_FPSnotspecified "FPS がヘッダに指定されていないか不正です. -fps オプションを利用して下さい.\n"
#define MSGTR_CantFindAudioCodec "audio format 0x%X 向けのコーデックを見付ける事が出来ませんでした.\n"
#define MSGTR_RTFMCodecs "DOCS/HTML/en/codecs.html を御覧下さい\n"
#define MSGTR_VOincompCodec "選択された映像出力デバイスはコーデックと互換性がありません\n"
#define MSGTR_CannotInitVO "FATAL: 映像出力ドライバの初期化に失敗しました.\n"
#define MSGTR_CannotInitAO "音声デバイスの初期化に失敗しました -> 無音声になります.\n"
#define MSGTR_StartPlaying "再生開始...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"           ************************************************\n"\
"           ***  貴方のシステムはこれを再生するには遅い  ***\n"\
"           ************************************************\n\n"\
"予想される問題や環境は:\n"\
"- Most common: 壊れてるか、バギーな 音声ドライバー\n"\
"  - -ao を使い sdl か ALSA 0.5 もしくはALSA 0.9でOSSエミュレーションを試みる.\n"\
"  - Experiment with different values for -autosync, 30 is a good start.\n"\
"- 映像出力が遅い\n"\
"  - 違う -vo ドライバを利用するか(-vo helpでリストが得られます) -framedropを試みる\n"\
"- CPUが遅い\n"\
"  - 巨大なDVDやDivXを遅いCPUで再生しようと試みない ;-) -hardframedropを試みる\n"\
"- ファイルが壊れてる\n"\
"  - 次のオプションの様々な組合せを試みて下さい: -nobps -ni -forceidx -mc 0.\n"\
"- 遅いメディア(NFS/SMB だったり, DVD, VCD などのドライブが遅かったり)\n"\
"  -次のオプションを試みる -cache 8192.\n"\
"- non-interleaved AVI ファイルに -cacheオプションを使ってませんか?\n"\
"  - 次のオプションを試みる -nocache.\n"\
"チューニング、スピードアップの為に DOCS/HTML/en/video.html を御覧下さい.\n"\
"もし、これらを試しても何もこう化が得られない場合は、DOCS/HTML/en/bugreports.html を御覧下さい.\n\n"

#define MSGTR_NoGui "MPlayer はGUIサポートを無効にしてコンパイルされました.\n"
#define MSGTR_GuiNeedsX "MPlayerのGUIはX11を必要とします.\n"
#define MSGTR_Playing "%s を再生中\n"
#define MSGTR_NoSound "音声: 無し\n"
#define MSGTR_FPSforced "FPS forced to be %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "コンパイル時にRuntime CPU Detectionが試用されています、これは最適ではありません\n最適なパフォーマスを得るには、--disable-runtime-cpudetectionを有効にしてMPLayerを再コンパイルして下さい\n"
#define MSGTR_CompiledWithCPUExtensions "x86 CPU 向けにコンパイルされました:"
#define MSGTR_AvailableVideoOutputDrivers "有効な映像出力ドライバ:\n"
#define MSGTR_AvailableAudioOutputDrivers "有効な音声出力ドライバ:\n"
#define MSGTR_AvailableAudioCodecs "有効な音声コーデック:\n"
#define MSGTR_AvailableVideoCodecs "有効な映像コーデック:\n"
#define MSGTR_AvailableAudioFm "\n有効な(組み込まれた)音声コーデック families/drivers:\n"
#define MSGTR_AvailableVideoFm "\n有効な(組み込まれた)映像コーデック families/drivers:\n"
#define MSGTR_AvailableFsType "全画面表示モードへの切替えは可能です:\n"
#define MSGTR_UsingRTCTiming "Linux hardware RTC timing (%ldHz) を使っています.\n"
#define MSGTR_CannotReadVideoProperties "Video: プロパティーを読み取れません.\n"
#define MSGTR_NoStreamFound "ストリームを見付けることが出来ませんでした.\n"
#define MSGTR_ErrorInitializingVODevice "選択された映像出力(-vo)デバイスを開く事が出来ませんでした.\n"
#define MSGTR_ForcedVideoCodec "指定された映像コーデック: %s\n"
#define MSGTR_ForcedAudioCodec "指定された音声コーデック: %s\n"
#define MSGTR_Video_NoVideo "Video: 映像がありません\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: 画像フィルター(-vf)か画像出力(-vo)の初期化に失敗しました.\n"
#define MSGTR_Paused "\n  =====  停止  =====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nプレイリストの読み込みが出来ません %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayerは不正な命令(Illegal Instruction)によりクラッシュしました\n"\
"  恐らくこれは あたらしいCPU-Detection codeにバグがあります\n"\
"  DOCS/HTML/en/bugreports.html をお読み下さい.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayerは不正な命令(Illegal Instruction)によりクラッシュしました\n"\
"  It usually happens when you run it on a CPU different than the one it was\n"\
"  compiled/optimized for.\n"\
"  Verify this!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayerは不良な CPU/FPU/RAM によってクラッシュしました.\n"\
"  Recompile MPlayer with --enable-debug and make a 'gdb' backtrace and\n"\
"  --enable-debugをつけてMPlyaerをコンパイルしなおし、gdbで調査しましょう\n"\
"  詳細は DOCS/HTML/en/bugreports.html#bugreports_crash にあります\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer は想定されていないクラッシュを起こしました.\n"\
"  It can be a bug in the MPlayer code _or_ in your drivers _or_ in your\n"\
"  gcc version. If you think it's MPlayer's fault, please read\n"\
"  DOCS/HTML/en/bugreports.html and follow the instructions there. We can't and\n"\
"  won't help unless you provide this information when reporting a possible bug.\n"


// mencoder.c:

#define MSGTR_CannotOpenFile_Device "ファイル及びデバイスが開けません.\n"
#define MSGTR_CannotOpenDemuxer "demuxerを開くことが出来ません.\n"
#define MSGTR_NoAudioEncoderSelected "\n音声エンコーダ(-oac)が指定されていません、 何か指定するか、無指定(-nosound)を与えて下さい。詳細は '-oac help'\n"
#define MSGTR_NoVideoEncoderSelected "\n映像エンコーダ(-ovc)が指定されていません、 何か指定して下さい。 詳細は '-ovc help'\n"
#define MSGTR_CannotOpenOutputFile "出力ファイル'%s'を開く事が出来ません.\n"
#define MSGTR_EncoderOpenFailed "エンコーダを開くことに失敗しました.\n"
#define MSGTR_ForcingOutputFourcc "fourccを %x [%.4s] に指定します\n"
#define MSGTR_DuplicateFrames "\n%d 重複したフレーム\n"
#define MSGTR_SkipFrame "\nフレームをスキップしています\n"
#define MSGTR_ErrorWritingFile "%s: ファイル書き込みエラー.\n"
#define MSGTR_VideoStreamResult "\n映像ストリーム: %8.3f kbit/s  (%d B/s)  サイズ: %"PRIu64" bytes  %5.3f secs  %d フレーム\n"
#define MSGTR_AudioStreamResult "\n音声ストリーム: %8.3f kbit/s  (%d B/s)  サイズ: %"PRIu64" bytes  %5.3f secs\n"

// cfg-mencoder.h:

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM デバイス '%s' が存在しません.\n"
#define MSGTR_ReadSTDIN "標準入力から読み込んでいます...\n"
#define MSGTR_UnableOpenURL "指定されたURLを読み込めません: %s\n"
#define MSGTR_ConnToServer "サーバに接続中: %s\n"
#define MSGTR_FileNotFound "ファイルが存在しません: '%s'\n"

#define MSGTR_SMBInitError "libsmbclient の初期化失敗: %d\n"
#define MSGTR_SMBFileNotFound "ローカルエリアネットワークから開くことが出来ませんでした: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer はSMB reading support を無効にしてコンパイルされています\n"

#define MSGTR_CantOpenDVD "DVDデバイスを開くことが出来ませんでした: %s (%s)\n"
#define MSGTR_DVDnumTitles "このDVDには %d タイトル記録されています.\n"
#define MSGTR_DVDinvalidTitle "不正な DVD タイトル番号です: %d\n"
#define MSGTR_DVDnumChapters "このDVDは %d キャプターあります.\n"
#define MSGTR_DVDinvalidChapter "不正なDVDキャプター番号ですr: %d\n"
#define MSGTR_DVDnumAngles "このDVDには %d アングルあります.\n"
#define MSGTR_DVDinvalidAngle "不正なDVDアングル番号です: %d\n"
#define MSGTR_DVDnoIFO "Cannot open the IFO file for DVD title %d.\n"
#define MSGTR_DVDnoVOBs "Cannot open title VOBS (VTS_%02d_1.VOB).\n"

// muxer_*.c:
#define MSGTR_TooManyStreams "ストリームが多過ぎる!"
#define MSGTR_RawMuxerOnlyOneStream "Rawaudio muxerが一つだけの音声ストリームをサポートします!\n"
#define MSGTR_IgnoringVideoStream "映像ストリームを差し置きます!\n"
#define MSGTR_UnknownStreamType "警告: 未知のストリームタイプ: %d\n"
#define MSGTR_WarningLenIsntDivisible "警告: samplesizeでlenが整除できない!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "警告: Audio stream header %d redefined.\n"
#define MSGTR_VideoStreamRedefined "警告: Video stream header %d redefined.\n"
#define MSGTR_TooManyAudioInBuffer "\nバッファに多過ぎる音声パケットが与えられてます: (%d in %d bytes).\n"
#define MSGTR_TooManyVideoInBuffer "\nバッファに多過ぎる映像パケットが与えられてます: (%d in %d bytes).\n"
#define MSGTR_Detected_XXX_FileFormat "%s ファイルフォーマットと判断.\n"
#define MSGTR_DetectedAudiofile "音声ファイルと判断.\n"
#define MSGTR_FormatNotRecognized "============ このファイルフォーマットは サポートしていません =============\n"\
				  "======= もしこのファイルが AVI、ASF、MPEGなら作成者に連絡して下さい ======\n"
#define MSGTR_MissingVideoStream "映像ストリームが存在しません.\n"
#define MSGTR_MissingAudioStream "音声ストリームが存在しません -> 無音声になります\n"
#define MSGTR_MissingVideoStreamBug "Missing video stream!? 作成者に連絡して下さい、恐らくこれはバグです :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: 選択された 映像か音声を格納することが出来ません.\n"

#define MSGTR_NI_Forced "Forced"
#define MSGTR_NI_Detected "Detected"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI ファイル フォーマット.\n"

#define MSGTR_CantSeekRawAVI "Cannot seek in raw AVI streams. (Indexが必要です, -idx を試して下さい.)\n"
#define MSGTR_CantSeekFile "このファイルはシークすることが出来ません.\n"

#define MSGTR_MOVcomprhdr "MOV: 圧縮されたヘッダ(Compressed headers)をサポートするには ZLIB が必要です\n"
#define MSGTR_MOVvariableFourCC "MOV: 警告: Variable FOURCC detected!?\n"
#define MSGTR_MOVtooManyTrk "MOV: 警告: too many tracks"
#define MSGTR_FoundAudioStream "==> 音声ストリームが見付かりました: %d\n"
#define MSGTR_FoundVideoStream "==> 映像ストリームが見付かりました: %d\n"
#define MSGTR_DetectedTV "TV detected! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "ogg demuxer を開くことが出来ません.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: 音声ストリームを探しています (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "音声ストリームを開くことが出来ません: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "サブタイトルストリームを開くことが出来ません: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "audio demuxerを開くこと開くことが出来ません: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "subtitle demuxerを開くことが出来ません: %s\n"
#define MSGTR_TVInputNotSeekable "TV入力はシークすることは出来ません(シークは恐らくチャンネル選択に相当するのでは? ;)\n"
#define MSGTR_ClipInfo "クリップ情報:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 30fps NTSC コンテンツ検出, フレームレート変更中.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 24fps プログレッシブ NTSC コンテンツ検出, フレームレート変更中.\n"


// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "コーデックを開くことが出来ません.\n"
#define MSGTR_CantCloseCodec "コーデックを閉じることが出来ません.\n"

#define MSGTR_MissingDLLcodec "エラー: 要求された DirectShow コーデック %s を開くことが出来ません.\n"
#define MSGTR_ACMiniterror "Win32/ACM音声コーデックの読み込み及び初期化をすることが出来ません (DLLファイルは大丈夫ですか?).\n"
#define MSGTR_MissingLAVCcodec "'%s' を libavcodecから見付けることが出来ません ...\n"

#define MSGTR_CannotReadMpegSequHdr "FATAL: シーケンスヘッダ(sequence header)を読み込めません.\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: 拡張シーケンスヘッダ(sequence header extension)を読み込めません.\n"
#define MSGTR_BadMpegSequHdr "MPEG: 不正なシーケンスヘッダ(sequence header)\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: 不正な拡張シーケンスヘッダ(sequence header)\n"

#define MSGTR_ShMemAllocFail "共有メモリの確保に失敗\n"
#define MSGTR_CantAllocAudioBuf "音声出力バッファーの確保に失敗\n"

#define MSGTR_UnknownAudio "未知の、もしくは壊れた音声フォーマットです -> 無音声になります\n"

#define MSGTR_VideoAttributeNotSupportedByVO_VD "選択された vo と vd では映像属性 '%s' はサポートされてません.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "要求された映像コーデック [%s] (vfm=%s) は無効です (有効にするにはコンパイル時に指定します)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "要求された音声コーデック [%s] (afm=%s) は無効です (有効にするにはコンパイル時に指定します)\n"
#define MSGTR_OpeningVideoDecoder "映像コーデックを開いています: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "音声コーデックを開いています: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit video: %s\n"
#define MSGTR_UninitAudioStr "uninit audio: %s\n"
#define MSGTR_VDecoderInitFailed "映像デコーダの初期化に失敗しました :(\n"
#define MSGTR_ADecoderInitFailed "音声デコーダの初期化に失敗しました :(\n"
#define MSGTR_ADecoderPreinitFailed "音声デコーダの前処理に失敗 :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: 入力バッファを %d bytes 確保しました\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: 出力バッファを %d + %d = %d bytes 確保しました\n"

// LIRC:
#define MSGTR_SettingUpLIRC "LIRC サポートをセッティング中...\n"
#define MSGTR_LIRCopenfailed "LIRC サポートを開く事に失敗.\n"
#define MSGTR_LIRCcfgerr "LIRC 設定ファイル %s を開くことに失敗しました.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "映像フィルタ '%s' が見付かりません\n"
#define MSGTR_CouldNotOpenVideoFilter "音声フィルタ '%s' が見付かりません\n"
#define MSGTR_OpeningVideoFilter "映像フィルタを開いています: "
#define MSGTR_CannotFindColorspace "common colorspaceが見付かりません, even by inserting 'scale' :(\n"

// vd.c
#define MSGTR_VoConfigRequest "VDec: 映像出力設定 - %d x %d (preferred csp: %s)\n"

// ====================== GUI messages/buttons ========================

#ifdef CONFIG_GUI

// --- labels ---
#define MSGTR_About "アバウト"
#define MSGTR_FileSelect "ファイル選択 ..."
#define MSGTR_SubtitleSelect "サブタイトル選択 ..."
#define MSGTR_OtherSelect "選択 ..."
#define MSGTR_FontSelect "フォント選択 ..."
#define MSGTR_PlayList "プレイリスト"
#define MSGTR_Equalizer "エコライザー"
#define MSGTR_SkinBrowser "スキンブラウザ"
#define MSGTR_Preferences "設定"
#define MSGTR_NoMediaOpened "メディアが開かれていません."
#define MSGTR_VCDTrack "VCD トラック %d"
#define MSGTR_NoChapter "キャプターがありません"
#define MSGTR_Chapter "キャプター %d"
#define MSGTR_NoFileLoaded "ファイルが読み込まれていません."

// --- buttons ---
#define MSGTR_Ok "OK"
#define MSGTR_Cancel "キャンセル"
#define MSGTR_Add "追加"
#define MSGTR_Remove "削除"
#define MSGTR_Clear "クリア"
#define MSGTR_Config "設定"
#define MSGTR_ConfigDriver "ドライバ設定"
#define MSGTR_Browse "ブラウズ"

// --- error messages ---
#define MSGTR_NEMDB "描画に必要なバッファを確保するためのメモリが足りません."
#define MSGTR_NEMFMR "メニューを描画に必要なメモリが足りません."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] エラー: スキン設定ファイル %d 行: %s"
#define MSGTR_SKIN_WARNING1 "[skin] 警告: スキン設定ファイル %d 行: widget found but before \"section\" not found ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] 警告: スキン設定ファイル %d 行: widget found but before \"subsection\" not found (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] 警告: スキン設定ファイル %d 行: this subsection not supported by this widget (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16 ビット以下の解消度はサポートされていません (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "ファイルが存在しません (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "bmp 読み込みエラー (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "tga 読み込みエラー (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "png 読み込みエラー (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE packed tga はサポートされていません (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "未知のファイルタイプです (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "24bitから32bitへの変換エラー (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "未知のメッセージ: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "メモリが不足しています\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "フォントファイルが存在しません\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "フォントイメージファイルが存在しません\n"
#define MSGTR_SKIN_UnknownParameter "未知のパラメータ(%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "スキンが存在しません( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "スキン設定ファイルの読み込みエラー(%s).\n"
#define MSGTR_SKIN_LABEL "スキン:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "MPlayerについて"
#define MSGTR_MENU_Open "開く ..."
#define MSGTR_MENU_PlayFile "ファイル再生 ..."
#define MSGTR_MENU_PlayVCD "VCD 再生 ..."
#define MSGTR_MENU_PlayDVD "DVD 再生 ..."
#define MSGTR_MENU_PlayURL "URL 再生 ..."
#define MSGTR_MENU_LoadSubtitle "サブタイトル読み込み ..."
#define MSGTR_MENU_DropSubtitle "サブタイトル破棄 ..."
#define MSGTR_MENU_Playing "現在のファイル"
#define MSGTR_MENU_Play "再生"
#define MSGTR_MENU_Pause "一時停止"
#define MSGTR_MENU_Stop "停止"
#define MSGTR_MENU_NextStream "次のストリーム"
#define MSGTR_MENU_PrevStream "前のストリーム"
#define MSGTR_MENU_Size "サイズ"
#define MSGTR_MENU_NormalSize "通常サイズ"
#define MSGTR_MENU_DoubleSize "2倍サイズ"
#define MSGTR_MENU_FullScreen "フルスクリーン"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "ディスク再生 ..."
#define MSGTR_MENU_ShowDVDMenu "DVD メニューの表示"
#define MSGTR_MENU_Titles "タイトル"
#define MSGTR_MENU_Title "タイトル %2d"
#define MSGTR_MENU_None "(無し)"
#define MSGTR_MENU_AudioLanguages "音声言語"
#define MSGTR_MENU_SubtitleLanguages "サブタイトル言語"
#define MSGTR_MENU_SkinBrowser "スキンブラウザ"
#define MSGTR_MENU_Exit "終了 ..."
#define MSGTR_MENU_Mute "消音"
#define MSGTR_MENU_Original "オリジナル"
#define MSGTR_MENU_Track "トラック %d"
#define MSGTR_MENU_VideoTrack "映像トラック"

// --- equalizer
#define MSGTR_EQU_Audio "音声"
#define MSGTR_EQU_Video "映像"
#define MSGTR_EQU_Contrast "明暗: "
#define MSGTR_EQU_Brightness "光度: "
#define MSGTR_EQU_Front_Left "前方 左"
#define MSGTR_EQU_Front_Right "前方 右"
#define MSGTR_EQU_Back_Left "後方 左"
#define MSGTR_EQU_Back_Right "後方 右"
#define MSGTR_EQU_Center "中央"
#define MSGTR_EQU_Bass "バス"
#define MSGTR_EQU_All "All"
#define MSGTR_EQU_Channel1 "チャンネル 1:"
#define MSGTR_EQU_Channel2 "チャンネル 2:"
#define MSGTR_EQU_Channel3 "チャンネル 3:"
#define MSGTR_EQU_Channel4 "チャンネル 4:"
#define MSGTR_EQU_Channel5 "チャンネル 5:"
#define MSGTR_EQU_Channel6 "チャンネル 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "パス"
#define MSGTR_PLAYLIST_Selected "選択されたファイル"
#define MSGTR_PLAYLIST_Files "ファイル"
#define MSGTR_PLAYLIST_DirectoryTree "ディレクトリツリー"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "サブタイトル & OSD"
#define MSGTR_PREFERENCES_Codecs "コーデック & demuxer"
#define MSGTR_PREFERENCES_Misc "Misc"

#define MSGTR_PREFERENCES_None "無し"
#define MSGTR_PREFERENCES_AvailableDrivers "有効なドライバ:"
#define MSGTR_PREFERENCES_EnableEqualizer "イコライザーの有効"
#define MSGTR_PREFERENCES_DoubleBuffer "double buffering 有効"
#define MSGTR_PREFERENCES_DirectRender "direct rendering 有効"
#define MSGTR_PREFERENCES_FrameDrop "frame dropping 有効"
#define MSGTR_PREFERENCES_HFrameDrop "HARD frame dropping (危険です) 有効"
#define MSGTR_PREFERENCES_OSDTimer "タイマーとインディケイター"
#define MSGTR_PREFERENCES_OSDProgress "プログレスバーだけ"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "タイマー パーセンテージとトータル時間"
#define MSGTR_PREFERENCES_Subtitle "サブタイトル:"
#define MSGTR_PREFERENCES_SUB_Delay "Delay: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "位置: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "subtitle 自動読み込み無効"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode サブタイトル"
#define MSGTR_PREFERENCES_SUB_MPSUB "与えられたサブタイトルをMPLayerのサブタイトルフォーマットに変換"
#define MSGTR_PREFERENCES_Font "フォント:"
#define MSGTR_PREFERENCES_PostProcess "postprocessing 有効"
#define MSGTR_PREFERENCES_NI "non-interleaved AVI パーサ使用"
#define MSGTR_PREFERENCES_IDX "必要ならindex tableの再構築"
#define MSGTR_PREFERENCES_VideoCodecFamily "映像コーデック:"
#define MSGTR_PREFERENCES_AudioCodecFamily "音声コーデック:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD レベル"
#define MSGTR_PREFERENCES_FRAME_Subtitle "サブタイトル"
#define MSGTR_PREFERENCES_FRAME_Font "フォント"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "コーデック& demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "キャッシュ"
#define MSGTR_PREFERENCES_DXR3_VENC "Video エンコーダ:"
#define MSGTR_PREFERENCES_DXR3_LAVC "LAVC (FFmpeg)使用"
#define MSGTR_PREFERENCES_FontEncoding1 "ユニコード"
#define MSGTR_PREFERENCES_FontEncoding "エンコーディング:"
#define MSGTR_PREFERENCES_Cache "キャッシュ on/off"
#define MSGTR_PREFERENCES_CacheSize "キャッシュサイズ: "
#define MSGTR_PREFERENCES_LoadFullscreen "開始時にフルスクリーン"
#define MSGTR_PREFERENCES_SaveWinPos "ウィンドウ位置を保持"
#define MSGTR_PREFERENCES_XSCREENSAVER "XScreenSaverをストップ"
#define MSGTR_PREFERENCES_PlayBar "プレイバー有効"
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM デバイス:"
#define MSGTR_PREFERENCES_DVDDevice "DVD デバイス:"
#define MSGTR_PREFERENCES_FPS "Movie FPS:"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "致命的エラー!"
#define MSGTR_MSGBOX_LABEL_Error "エラー"
#define MSGTR_MSGBOX_LABEL_Warning "警告"

#endif
