// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2003 Arpad Gereoffy (看DOCS!)\n"
"\n";

static char help_text[]=
"用法:   mplayer [options] [url|path/]filename\n"
"\n"
"基本选项: (包括所有选项的详细列表参见manpage!)\n"
" -vo <drv[:dev]> 选择视频输出模式和设备(用'-vo help'查看列表)\n"
" -ao <drv[:dev]> 选择音频输出模式和设备(用'-ao help'查看列表)\n"
#ifdef HAVE_VCD
" -vcd <trackno>  从设备而不是普通文件上播放VCD(video cd)track\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  为DVD设备设置授权码(用于加密光盘)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  从设备而不是普通文件上播放DVD title/track\n"
" -alang/-slang   选择DVD音轨/字幕的语言(使用两位的国家代码)\n"
#endif
" -ss <timepos>   寻找指定的(多少秒或hh:mm:ss)位置\n"
" -nosound        不播放声音\n"
" -fs -vm -zoom   全屏播放选项(fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   设置播放的分辨率(用于改变vidmode或软件缩放)\n"
" -sub <file>     指定使用的字幕文件(参见-subfps, -subdelay)\n"
" -playlist <file> 指定使用播放列表文件\n"
" -vid x -aid y   选择用于播放的视频(x)和音频(y)流\n"
" -fps x -srate y 改变视频(x fps)和音频(y Hz)率\n"
" -pp <quality>   使用后期处理滤镜(详细内容参见manpage/docs)\n"
" -framedrop      使用 frame-dropping (用于慢机器)\n"
"\n"
"基本控制键: (完整的列表参见manpage, 同时也要检查一下 input.conf)\n"
" <-  or  ->      向后/向前搜索10秒\n"
" up or down      向后/向前搜索1分钟\n"
" pgup or pgdown  向后/向前搜索10分钟\n"
" < or >          跳到播放列表中的前一首/下一首\n"
" p or SPACE      暂停播放(按任意键继续)\n"
" q or ESC        停止播放并推出\n"
" + or -          调整音频延迟+/-0.1秒\n"
" o               循环OSD模式:  none/seekbar/seekbar+timer\n"
" * or /          增加或减少pcm音量\n"
" z or x          调整字幕延迟+/-0.1秒\n"
" r or t          上/下调整字幕位置, 参见-vop expand !\n"
"\n"
" * * * 详细内容，进一步(高级)的选项和控制键参见MANPAGE！* * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\n正在退出... (%s)\n"
#define MSGTR_Exit_quit "退出"
#define MSGTR_Exit_eof "文件结束"
#define MSGTR_Exit_error "致命错误"
#define MSGTR_IntBySignal "\nMPlayer被 %s 模块中的 %d 信号中断\n"
#define MSGTR_NoHomeDir "找不到HOME目录\n"
#define MSGTR_GetpathProblem "get_path(\"config\")问题\n"
#define MSGTR_CreatingCfgFile "创建config文件: %s\n"
#define MSGTR_InvalidVOdriver "无效的视频输出驱动: %s\n用'-vo help' t查看可用的视频驱动的列表"
#define MSGTR_InvalidAOdriver "无效的视频输出驱动: %s\n用'-vo help' t查看可用的视频驱动的列表"
#define MSGTR_CopyCodecsConf "(把etc/codecs.conf(从MPlayer的源代码中)复制/链接 ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "使用内建默认的codecs.conf\n"
#define MSGTR_CantLoadFont "无法加载字体: %s\n"
#define MSGTR_CantLoadSub "无法加载字幕: %s\n"
#define MSGTR_ErrorDVDkey "DVD KEY处理出错.\n"
#define MSGTR_CmdlineDVDkey "DVD命令行要求的key被储存以用来解扰.\n"
#define MSGTR_DVDauthOk "DVD授权序列似乎OK.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: 致命错误: 指定的流不存在!\n"
#define MSGTR_CantOpenDumpfile "无法打开dump文件.\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS在文件头中没有指定(或者是无效数据)! 用-fps选项!\n"
#define MSGTR_TryForceAudioFmtStr "尝试指定音频解码器驱动族 %s ...\n"
#define MSGTR_CantFindAfmtFallback "找不到指定驱动族的音频解码器, 只能使用其他驱动.\n"
#define MSGTR_CantFindAudioCodec "找不到音频格式 0x%X 的解码器.\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** 尝试用 etc/codecs.conf 升级 %s \n*** 如果这样还不行, 查看DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "无法初始化音频解码器! -> nosound\n"
#define MSGTR_TryForceVideoFmtStr "尝试指定视频解码器驱动族 %s ...\n"
#define MSGTR_CantFindVideoCodec "找不到适合所选的-vo和视频格式 0x%X 的解码器!\n"
#define MSGTR_VOincompCodec "抱歉, 所选的视频输出设备与这个解码器不兼容.\n"
#define MSGTR_CannotInitVO "致命错误: 无法初始化视频驱动!\n"
#define MSGTR_CannotInitAO "无法打开/初始化音频设备 -> NOSOUND\n"
#define MSGTR_StartPlaying "开始播放...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ************************************************\n"\
"         ****       你的系统太慢了，放不了这个！     ****\n"\
"         ************************************************\n"\
" 可能的原因，问题，解决办法：\n"\
"- 最普遍的原因：损坏的或有bug的_音频_驱动\n"\
"  - 试试-ao sdl或使用 ALSA 0.5或ALSA 0.9的oss模拟。\n"\
"  - 试试不同的-autosync的值，不妨从30开始。\n"\
"- 视频输出太慢\n"\
"  - 试试不同的-vo driver(-vo help有列表)或者试试-framedrop！\n"\
"- cpu太慢\n"\
"  - 不要试图在慢速cpu上播放大的dvd/divx! 试试-hardframedrop。\n"\
"- 损坏的文件\n"\
"  - 试试下列选项的不同组合：-nobps  -ni  -mc 0  -forceidx\n"\
"- Slow media (NFS/SMB mounts, DVD, VCD etc)\n"\
"  - 试试 -cache 8192。\n"\
"- 你使用-cache选项播放一个非交错的avi文件？\n"\
"  - 试试-nocache\n"\
"阅读DOCS/video.html和DOCS/sound.html来寻找调整/加速的技巧。\n"\
"如果这些一个都用不上，阅读DOCS/bugreports.html！\n\n"

#define MSGTR_NoGui "Mplayer没有编译GUI的支持!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI需要X11!\n"
#define MSGTR_Playing "播放 %s\n"
#define MSGTR_NoSound "音频: no sound\n"
#define MSGTR_FPSforced "FPS指定为 %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "编译实时CPU检测 - 警告, 这不是最佳选择! 如果你想获得最佳的表现, 加上--disable-runtime-cpudetection选项重新编译mplayer\n"
#define MSGTR_CompiledWithCPUExtensions "针对有扩展指令集x86 CPU编译:"
#define MSGTR_AvailableVideoOutputPlugins "可用的视频输出插件:\n"
#define MSGTR_AvailableVideoOutputDrivers "可用的视频输出驱动:\n"
#define MSGTR_AvailableAudioOutputDrivers "可用的音频输出驱动:\n"
#define MSGTR_AvailableAudioCodecs "可用的音频解码器:\n"
#define MSGTR_AvailableVideoCodecs "可用的视频解码器:\n"
#define MSGTR_AvailableAudioFm "\n可用的(编译了的)音频解码器族/驱动:\n"
#define MSGTR_AvailableVideoFm "\n可用的(编译了的)视频解码器族/驱动:\n"
#define MSGTR_UsingRTCTiming "使用Linux的硬件RTC计时(%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "视频: 无法读取属性\n"
#define MSGTR_NoStreamFound "找不到流媒体\n"
#define MSGTR_InitializingAudioCodec "初始化音频解码器...\n"
#define MSGTR_ErrorInitializingVODevice "打开/初始化所选的视频输出(-vo)设备是出错!\n"
#define MSGTR_ForcedVideoCodec "指定的视频解码器: %s\n"
#define MSGTR_ForcedAudioCodec "指定的音频解码器: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: 描述: %s\nAO: 作者: %s\n"
#define MSGTR_AOComment "AO: 备注: %s\n"
#define MSGTR_Video_NoVideo "视频: no video\n"
#define MSGTR_NotInitializeVOPorVO "\n致命错误: 无法初始化视频插件(-vop)或视频输出(-vo) !\n"
#define MSGTR_Paused "\n================= 暂停 =================\r"
#define MSGTR_PlaylistLoadUnable "\n无法装载播放列表 %s\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- “非法指令”导致MPlayer崩溃。\n"\
"  这可能是我们新的运行时CPU检测代码的一个bug...\n"\
"  请阅读DOCS/bugreports.html\n"
#define MSGTR_Exit_SIGILL \
"- “非法指令”导致MPlayer崩溃。\n"\
"  这通常发生在你在与编译/优化MPlayer不同的CPU上使用\n"\
"  MPlayer造成的\n 检察一下!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- 过度使用CPU/FPU/RAM导致MPlayer崩溃.\n"\
"  使用--enable-debug重新编译MPlayer用“gdb”backtrace和\n"\
"  反汇编。具体细节看DOCS/bugreports.html#crash.b.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer崩溃了。这不应该发生。\n"\
"  这可能是MPlayer代码中的 _或者_ 你的驱动中的 _or_ 你的gcc的\n"\
"  一个bug。如果你觉得这是MPlayer的错，请阅读DOCS/bugreports.html\n"\
"  并遵循上面的步骤。我们不能也不会帮助你除非你在报告一个可能bug的时候\n"\
"  提供所需要的信息。\n"


// mencoder.c:

#define MSGTR_MEncoderCopyright "(C) 2000-2003 Arpad Gereoffy (看DOCS!)\n"
#define MSGTR_UsingPass3ControllFile "使用pass3控制文件: %s\n"
#define MSGTR_MissingFilename "\n没有文件名!\n\n"
#define MSGTR_CannotOpenFile_Device "无法打开文件/设备\n"
#define MSGTR_ErrorDVDAuth "DVD授权错误...\n"
#define MSGTR_CannotOpenDemuxer "无法打开demuxer\n"
#define MSGTR_NoAudioEncoderSelected "\n没有选择音频编码器(-oac)! 选择一个或者使用-nosound. 使用-oac help !\n"
#define MSGTR_NoVideoEncoderSelected "\nNo video encoder (-ovc) selected! Select one, use -ovc help !\n"
#define MSGTR_InitializingAudioCodec "初始化音频解码器...\n"
#define MSGTR_CannotOpenOutputFile "无法打开输出文件 '%s'\n"
#define MSGTR_EncoderOpenFailed "无法打开编码器\n"
#define MSGTR_ForcingOutputFourcc "指定输出的fourcc为 %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "正在写AVI文件头...\n"
#define MSGTR_DuplicateFrames "\n已复制 %d 帧!\n"
#define MSGTR_SkipFrame "\n跳过这一帧!\n"
#define MSGTR_ErrorWritingFile "%s: 写入文件错误.\n"
#define MSGTR_WritingAVIIndex "\n正在写AVI索引...\n"
#define MSGTR_FixupAVIHeader "修复AVI文件头...\n"
#define MSGTR_RecommendedVideoBitrate "%s CD推荐的视频比特率为: %d\n"
#define MSGTR_VideoStreamResult "\n视频流: %8.3f kbit/s  (%d bps)  大小: %d bytes  %5.3f secs  %d frames\n"
#define MSGTR_AudioStreamResult "\n音频流: %8.3f kbit/s  (%d bps)  大小: %d bytes  %5.3f secs\n"
// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     变比特率方式\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(默认)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           平均比特率\n"\
"\n"\
" cbr           常比特率\n"\
"               会在后继ABR预置模式中强制使用CBR模式\n"\
"\n"\
" br=<0-1024>   以kBit为单位指定比特率 (仅用于CBR和ABR)\n"\
"\n"\
" q=<0-9>       编码质量 (0-最高, 9-最低) (仅用于VBR)\n"\
"\n"\
" aq=<0-9>      算法质量 (0-最好/最慢, 9-最低/最快)\n"\
"\n"\
" ratio=<1-100> 压缩率\n"\
"\n"\
" vol=<0-10>    设置音频输入增益\n"\
"\n"\
" mode=<0-3>    (默认: 自动)\n"\
"                0: 立体声\n"\
"                1: 联合立体声\n"\
"                2: 双声道\n"\
"                3: 单声道\n"\
"\n"\
" padding=<0-2>\n"\
"                0: 无\n"\
"                1: 所有\n"\
"                2: 调整\n"\
"\n"\
" fast          启动更快的后继VBR预置模式编码，\n"\
"               稍微降低质量并提高比特率。\n"\
"\n"\
" preset=<value> 提供最高的可能的质量设置。\n"\
"                 medium: VBR编码，质量：好\n"\
"                 (150-180 kbps比特率范围)\n"\
"                 standard:  VBR编码, 质量：高\n"\
"                 (170-210 kbps比特率范围)\n"\
"                 extreme: VBR编码，质量：非常高\n"\
"                 (200-240 kbps比特率范围)\n"\
"                 insane:  CBR编码，质量：最高\n"\
"                 (320 kbps bitrate)\n"\
"                 <8-320>: 以所给比特率为平均比特率的ABR编码。\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "找不到CD-ROM设备 '%s' !\n"
#define MSGTR_ErrTrackSelect "选择VCD track出错!"
#define MSGTR_ReadSTDIN "从stdin读取...\n"
#define MSGTR_UnableOpenURL "无法打开URL: %s\n"
#define MSGTR_ConnToServer "连接到服务器: %s\n"
#define MSGTR_FileNotFound "找不到文件: '%s'\n"

#define MSGTR_SMBInitError "无法初始化libsmbclient库: %d\n"
#define MSGTR_SMBFileNotFound "无法打开局域网内的: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer没有编译SMB读取的支持\n"

#define MSGTR_CantOpenDVD "无法打开DVD 设备: %s\n"
#define MSGTR_DVDwait "读取光盘结构, 请等待...\n"
#define MSGTR_DVDnumTitles "这张DVD有 %d 个titles.\n"
#define MSGTR_DVDinvalidTitle "无效的DVD title号: %d\n"
#define MSGTR_DVDnumChapters "这个 DVD title有 %d chapters.\n"
#define MSGTR_DVDinvalidChapter "I无效的DVD chapter号: %d\n"
#define MSGTR_DVDnumAngles "这个 DVD title有 %d 个视角.\n"
#define MSGTR_DVDinvalidAngle "无效的DVD视角号: %d\n"
#define MSGTR_DVDnoIFO "无法打开 DVD title: %d 的IFO文件.\n"
#define MSGTR_DVDnoVOBs "无法打开title的VOB(VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD成功打开!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "警告! 音频流头部 %d 被重新定义.\n"
#define MSGTR_VideoStreamRedefined "警告! 视频流头部 %d 被重新定义.\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: buffer中音频包太多(%d in %d bytes) !\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: buffer中视频包太多(%d in %d bytes) !\n"
#define MSGTR_MaybeNI "(也许你播放了一个非交错的流/文件或者是解码失败)?\n" \
		      "对于AVI文件, 尝试用-ni选项指定非交错模式.\n"
#define MSGTR_SwitchToNi "\n检测到糟糕的交错格式的AVI - 切换到-ni模式...\n"
#define MSGTR_Detected_XXX_FileFormat "检测到%s文件格式。\n"
#define MSGTR_DetectedAudiofile "检测到音频文件!\n"
#define MSGTR_NotSystemStream "非MPEG系统的流格式... (可能是输送流?)\n"
#define MSGTR_MissingMpegVideo "找不到MPEG视频流!? 联系作者, 这可能是个bug :(\n"
#define MSGTR_InvalidMPEGES "无效的MPEG-ES流??? 联系作者, 这可能是个bug :(\n"
#define MSGTR_FormatNotRecognized "============= 抱歉, 这种文件格式无法辨认或支持 ===============\n"\
				  "=== 如果这个文件是一个AVI, ASF或MPEG流, 请联系作者! ===\n"
#define MSGTR_MissingVideoStream "找不到视频流. \n"
#define MSGTR_MissingAudioStream "找不到音频流...  ->nosound\n"
#define MSGTR_MissingVideoStreamBug "没有视频流!? 联系作者, 这可能是个bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: 文件中没有所选择的音频或视频流 \n"

#define MSGTR_NI_Forced "强行指定"
#define MSGTR_NI_Detected "检测到"
#define MSGTR_NI_Message "%s 非交错AVI文件模式!\n"

#define MSGTR_UsingNINI "使用非交错的损坏的AVI文件格式!\n"
#define MSGTR_CouldntDetFNo "无法决定帧数(用于绝对搜索)  \n"
#define MSGTR_CantSeekRawAVI "无法在不完整的.AVI流中搜索. (需要索引, 尝试使用-idx 选项!)  \n"
#define MSGTR_CantSeekFile "无法在这个文件中搜索.  \n"

#define MSGTR_EncryptedVOB "加密的VOB文件(没有编译libcss支持)! 阅读DOCS/cd-dvd.html.\n"
#define MSGTR_EncryptedVOBauth "加密流但你没有要求使用授权!\n"

#define MSGTR_MOVcomprhdr "MOV: 压缩的文件头(目前)不支持.\n"
#define MSGTR_MOVvariableFourCC "MOV: 警告! 检测到可变的FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: 警告! 太多轨道."
#define MSGTR_FoundAudioStream "==> 找到音频流: %d\n"
#define MSGTR_FoundVideoStream "==> 找到视频流: %d\n"
#define MSGTR_DetectedTV "检测到TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "无法打开ogg demuxer\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: 寻找音频流(id:%d)\n"
#define MSGTR_CannotOpenAudioStream "无法打开音频流: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "无法打开字幕流: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "打开音频demuxer: %s失败\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "打开字幕demuxer: %s失败\n"
#define MSGTR_TVInputNotSeekable "TV输入不能搜索! (可能搜索应该用来更换频道;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Demuxer info %s 已经显示\n!"
#define MSGTR_ClipInfo "Clip info: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 检测到渐进序列, 离开3:2 TELECINE模式\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 检测到3:2 TELECINE, 启用反转telecine特效. FPS变为%5.3f!  \n"
// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "无法打开解码器\n"
#define MSGTR_CantCloseCodec "无法关闭解码器\n"

#define MSGTR_MissingDLLcodec "错误: 无法打开要求的DirectShow解码器: %s\n"
#define MSGTR_ACMiniterror "无法加载/初始化Win32/ACM音频解码器(缺少DLL文件?)\n"
#define MSGTR_MissingLAVCcodec "在libavcodec中找不到解码器 '%s'...\n"

#define MSGTR_MpegNoSequHdr "MPEG: 致命错误: 搜索序列头时遇到EOF\n"
#define MSGTR_CannotReadMpegSequHdr "致命错误: 无法读取序列头.\n"
#define MSGTR_CannotReadMpegSequHdrEx "致命错误: 无法读取序列头扩展.\n"
#define MSGTR_BadMpegSequHdr "MPEG: 糟糕的序列头.\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: 糟糕的序列头扩展.\n"

#define MSGTR_ShMemAllocFail "无法分配共享内存\n"
#define MSGTR_CantAllocAudioBuf "无法分配音频输出buffer\n"

#define MSGTR_UnknownAudio "未知或缺少音频格式, 使用nosound\n"

#define MSGTR_UsingExternalPP "[PP] 使用外部的后处理插件, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] 使用解码器的后处理插件, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "所选的vo & vd不支持视频属性'%s'. \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "要求的视频解码器族 [%s] (vfm=%s) 不可用 (在编译时开启它)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "要求的音频解码器族 [%s] (afm=%s) 不可用 (在编译时开启它)\n"
#define MSGTR_OpeningVideoDecoder "打开视频解码器: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "打开音频解码器: [%s] %s\n"
#define MSGTR_UninitVideoStr "关闭视频: %s  \n"
#define MSGTR_UninitAudioStr "关闭音频: %s  \n"
#define MSGTR_VDecoderInitFailed "VDecoder初始化失败 :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder初始化失败 :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder预初始化失败 :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: 为输入缓冲分配 %d 字节\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: 为输出缓冲分配 %d + %d = %d 字节\n"

// LIRC:
#define MSGTR_SettingUpLIRC "起动红外遥控支持...\n"
#define MSGTR_LIRCdisabled "你将无法使用你的遥控器\n"
#define MSGTR_LIRCopenfailed "红外遥控支持起动失败!\n"
#define MSGTR_LIRCcfgerr "读取LIRC配置文件 %s 失败!\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "找不到视频滤镜 '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "无法打开视频滤镜 '%s'\n"
#define MSGTR_OpeningVideoFilter "打开视频滤镜: "
#define MSGTR_CannotFindColorspace "无法找到公用的色彩空间, 甚至靠插入'scale'也不行 :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: 解码器无法设置sh->disp_w和sh->disp_h, 尝试绕过!\n"
#define MSGTR_VoConfigRequest "VDec: vo配置要求 - %d x %d (选择 csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "无法找到匹配的色彩空间 - 重新尝试 -vop scale...\n"
#define MSGTR_MovieAspectIsSet "电影宽高比为 %.2f:1 - 预放大到正确的电影宽高比.\n"
#define MSGTR_MovieAspectUndefined "电影宽高比未定义 - 无法使用预放大.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "关于"
#define MSGTR_FileSelect "选择文件..."
#define MSGTR_SubtitleSelect "选择字幕..."
#define MSGTR_OtherSelect "选择..."
#define MSGTR_AudioFileSelect "选择外部音频轨道..."
#define MSGTR_FontSelect "选择字体..."
#define MSGTR_PlayList "播放列表"
#define MSGTR_Equalizer "均衡器"
#define MSGTR_SkinBrowser "Skin浏览器"
#define MSGTR_Network "网络流媒体 ..."
#define MSGTR_Preferences "属性设置"
#define MSGTR_OSSPreferences "OSS驱动配置"
#define MSGTR_SDLPreferences "SDL驱动配置"
#define MSGTR_NoMediaOpened "没有打开媒体"
#define MSGTR_VCDTrack "VCD %d 轨道"
#define MSGTR_NoChapter "没有chapter"
#define MSGTR_Chapter "chapter %d"
#define MSGTR_NoFileLoaded "没有载入文件"

// --- buttons ---
#define MSGTR_Ok "确定"
#define MSGTR_Cancel "取消"
#define MSGTR_Add "添加"
#define MSGTR_Remove "删除"
#define MSGTR_Clear "清空"
#define MSGTR_Config "配置"
#define MSGTR_ConfigDriver "配置驱动"
#define MSGTR_Browse "浏览"

// --- error messages ---
#define MSGTR_NEMDB "抱歉, 没有足够的内存用于绘制缓冲."
#define MSGTR_NEMFMR "抱歉, 没有足够的内存用于菜单渲染."
#define MSGTR_IDFGCVD "抱歉, 无法找到gui兼容的视频输出驱动."
#define MSGTR_NEEDLAVCFAME "抱歉, 你不能用你的DXR3/H+设备不经过重新编码而播放非mpeg的文件.\n请在DXR3/H+配置中开启lavc或者fame."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] skin配置文件的 %d: %s行出错" 
#define MSGTR_SKIN_WARNING1 "[skin] 警告, 在skin配置文件的 %d行: 找到widget但在这之前没有找到\"section\" ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] 警告, 在skin配置文件的 %d行: 找到widget但在这之前没有找到 \"subsection\" ( %s) "
#define MSGTR_SKIN_WARNING3 "[skin] 警告, 在skin配置文件的 %d行: 这个widget不支持这个subsection(%s)"
#define MSGTR_SKIN_BITMAP_16bit  "不支持少于16 bits色深的位图( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "找不到文件( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "bmp读取错误( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "tga读取错误( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "png读取错误( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "不支持RLE格式压缩的tga( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "未知的文件格式( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "24 bit到32 bit的转换发生错误( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "未知信息: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "没有足够内存\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "声明了太多字体\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "找不到字体文件\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "找不到字体图像文件\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "不存在的字体标签( %s )\n"
#define MSGTR_SKIN_UnknownParameter "未知参数( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser]没有足够内存.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "找不到Skin( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin配置文件( %s )读取错误.\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "关于MPlayer"
#define MSGTR_MENU_Open "打开 ..."
#define MSGTR_MENU_PlayFile "播放文件 ..."
#define MSGTR_MENU_PlayVCD "播放VCD ..."
#define MSGTR_MENU_PlayDVD "播放DVD ..."
#define MSGTR_MENU_PlayURL "播放URL ..."
#define MSGTR_MENU_LoadSubtitle "加载字幕 ..."
#define MSGTR_MENU_DropSubtitle "丢弃字幕 ..."
#define MSGTR_MENU_LoadExternAudioFile "加载外部音频文件 ..."
#define MSGTR_MENU_Playing "播放控制"
#define MSGTR_MENU_Play "播放"
#define MSGTR_MENU_Pause "暂停"
#define MSGTR_MENU_Stop "停止"
#define MSGTR_MENU_NextStream "下一个"
#define MSGTR_MENU_PrevStream "上一个"
#define MSGTR_MENU_Size "大小"
#define MSGTR_MENU_NormalSize "正常大小"
#define MSGTR_MENU_DoubleSize "双倍大小"
#define MSGTR_MENU_FullScreen "全屏"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "打开碟片 ..."
#define MSGTR_MENU_ShowDVDMenu "显示DVD菜单"
#define MSGTR_MENU_Titles "Titles"
#define MSGTR_MENU_Title "Title %2d"
#define MSGTR_MENU_None "(none)"
#define MSGTR_MENU_Chapters "Chapters"
#define MSGTR_MENU_Chapter "Chapter %2d"
#define MSGTR_MENU_AudioLanguages "音频语言"
#define MSGTR_MENU_SubtitleLanguages "字幕语言"
#define MSGTR_MENU_PlayList "播放列表"
#define MSGTR_MENU_SkinBrowser "Skin浏览器"
#define MSGTR_MENU_Preferences "属性设置"
#define MSGTR_MENU_Exit "退出 ..."
#define MSGTR_MENU_Mute "静音"
#define MSGTR_MENU_Original "原始的"
#define MSGTR_MENU_AspectRatio "长宽比"
#define MSGTR_MENU_AudioTrack "音频轨道"
#define MSGTR_MENU_Track "轨道 %d"
#define MSGTR_MENU_VideoTrack "视频轨道"

// --- equalizer
#define MSGTR_EQU_Audio "音频"
#define MSGTR_EQU_Video "视频"
#define MSGTR_EQU_Contrast "对比度: "
#define MSGTR_EQU_Brightness "亮度: "
#define MSGTR_EQU_Hue "色相: "
#define MSGTR_EQU_Saturation "饱和度: "
#define MSGTR_EQU_Front_Left "前左"
#define MSGTR_EQU_Front_Right "前右"
#define MSGTR_EQU_Back_Left "后左"
#define MSGTR_EQU_Back_Right "后右"
#define MSGTR_EQU_Center "中间"
#define MSGTR_EQU_Bass "低音"
#define MSGTR_EQU_All "所有"

// --- playlist
#define MSGTR_PLAYLIST_Path "路径"
#define MSGTR_PLAYLIST_Selected "所选文件"
#define MSGTR_PLAYLIST_Files "所有文件"
#define MSGTR_PLAYLIST_DirectoryTree "目录树"

// --- preferences
#define MSGTR_PREFERENCES_Audio "音频"
#define MSGTR_PREFERENCES_Video "视频"
#define MSGTR_PREFERENCES_SubtitleOSD "字幕和OSD"
#define MSGTR_PREFERENCES_Misc "其他"

#define MSGTR_PREFERENCES_None "None"
#define MSGTR_PREFERENCES_AvailableDrivers "可用驱动:"
#define MSGTR_PREFERENCES_DoNotPlaySound "不播放声音"
#define MSGTR_PREFERENCES_NormalizeSound "声音标准化"
#define MSGTR_PREFERENCES_EnEqualizer "开启均衡器"
#define MSGTR_PREFERENCES_ExtraStereo "开启立体声加强"
#define MSGTR_PREFERENCES_Coefficient "参数:"
#define MSGTR_PREFERENCES_AudioDelay "音频延迟"
#define MSGTR_PREFERENCES_DoubleBuffer "开启双重缓冲"
#define MSGTR_PREFERENCES_DirectRender "开启直接渲染"
#define MSGTR_PREFERENCES_FrameDrop "开启掉帧选项"
#define MSGTR_PREFERENCES_HFrameDrop "开启HARD掉帧选项(危险)"
#define MSGTR_PREFERENCES_Flip "上下翻转图像"
#define MSGTR_PREFERENCES_Panscan "图像切割: "
#define MSGTR_PREFERENCES_OSDTimer "显示计时器和指示器"
#define MSGTR_PREFERENCES_OSDProgress "只显示进度条"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "计时器, 百分比和总时间"
#define MSGTR_PREFERENCES_Subtitle "字幕:"
#define MSGTR_PREFERENCES_SUB_Delay "延迟: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "位置: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "禁用字幕自动装载"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode字幕"
#define MSGTR_PREFERENCES_SUB_MPSUB "将所给字幕转换为Mplayer的字幕文件"
#define MSGTR_PREFERENCES_SUB_SRT "将所给字幕转换为基于时间的SubViewer(SRT) 格式"
#define MSGTR_PREFERENCES_SUB_Overlap "开启字幕重叠"
#define MSGTR_PREFERENCES_Font "字体:"
#define MSGTR_PREFERENCES_Codecs "Codecs和demuxer"
#define MSGTR_PREFERENCES_FontFactor "字体效果:"
#define MSGTR_PREFERENCES_PostProcess "开启后期处理"
#define MSGTR_PREFERENCES_AutoQuality "自动控制质量: "
#define MSGTR_PREFERENCES_NI "使用非交错的AVI分析器"
#define MSGTR_PREFERENCES_IDX "如果需要的话, 重建索引表"
#define MSGTR_PREFERENCES_VideoCodecFamily "视频解码器族:"
#define MSGTR_PREFERENCES_AudioCodecFamily "音频解码器族:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD级别"
#define MSGTR_PREFERENCES_FRAME_Subtitle "字幕"
#define MSGTR_PREFERENCES_FRAME_Font "字体"
#define MSGTR_PREFERENCES_FRAME_PostProcess "后期处理"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec和demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "缓存"
#define MSGTR_PREFERENCES_FRAME_Misc "其他"
#define MSGTR_PREFERENCES_OSS_Device "设备:"
#define MSGTR_PREFERENCES_OSS_Mixer "混音器:"
#define MSGTR_PREFERENCES_SDL_Driver "驱动:"
#define MSGTR_PREFERENCES_Message "请记住, 有些功能只有重新播放后才有效果."
#define MSGTR_PREFERENCES_DXR3_VENC "视频编码器:"
#define MSGTR_PREFERENCES_DXR3_LAVC "使用LAVC(ffmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "使用FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "西欧(ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "西欧(ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "中欧(ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "中欧(ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "波罗的语(ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "斯拉夫语(ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "阿拉伯语(ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "现代希腊语(ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "土耳其语(ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "波罗的语(ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "凯尔特语(ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "希伯来语(ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "俄语(KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "俄语(KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "简体中文(CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "繁体中文(BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "日语(SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "韩语(CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "泰语(CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Windows的西里尔语(CP1251)"
#define MSGTR_PREFERENCES_FontNoAutoScale "不自动缩放"
#define MSGTR_PREFERENCES_FontPropWidth "宽度成比例"
#define MSGTR_PREFERENCES_FontPropHeight "高度成比例"
#define MSGTR_PREFERENCES_FontPropDiagonal "对角线成比例"
#define MSGTR_PREFERENCES_FontEncoding "编码:"
#define MSGTR_PREFERENCES_FontBlur "模糊:"
#define MSGTR_PREFERENCES_FontOutLine "轮廓:"
#define MSGTR_PREFERENCES_FontTextScale "文字缩放:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD缩放:"
#define MSGTR_PREFERENCES_Cache "打开/关闭缓存"
#define MSGTR_PREFERENCES_LoadFullscreen "以全屏方式开始"
#define MSGTR_PREFERENCES_CacheSize "缓存大小: "
#define MSGTR_PREFERENCES_XSCREENSAVER "停用XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "使用播放条"
#define MSGTR_PREFERENCES_AutoSync "自同步 打开/关闭"
#define MSGTR_PREFERENCES_AutoSyncValue "自同步: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM设备:"
#define MSGTR_PREFERENCES_DVDDevice "DVD设备:"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "致命错误 ..."
#define MSGTR_MSGBOX_LABEL_Error "错误 ..."
#define MSGTR_MSGBOX_LABEL_Warning "警告 ..." 

#endif
