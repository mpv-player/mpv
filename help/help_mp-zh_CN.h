// Synced with help_mp-en.h r21655
// Reminder of hard terms which need better/final solution later:
//   (file links to be updated later if available!);  
//   NAV; section/subsection;  XScreenSaver; keycolor;  
//   AGP move failed on Y plane;  
//   profile? demuxer? drain? flush? 
// 
// Translated by Lu Ran <hephooey@fastmail.fm>, Sheldon Jin <jinsh2 AT yahoo.com>
// (Translator before 2006-04-24)
// Emfox Zhou <EmfoxZhou@gmail.com>
// (Translator before 2005-10-12)
// Lu Ran <hephooey@fastmail.fm>

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"用法:   mplayer [选项] [URL|路径/]文件名\n"
"\n"
"基本选项: (完整列表参见手册页)\n"
" -vo <drv>        选择视频输出驱动 (查看驱动列表用“-vo help”)\n"
" -ao <drv>        选择音频输出驱动 (查看驱动列表用“-ao help”)\n"
#ifdef HAVE_VCD
" vcd://<trackno>  播放 (S)VCD 轨迹号 (原始设备, 无需安挂)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titleno>  从设备而不是普通文件上播放 DVD 标题号\n"
" -alang/-slang    选择 DVD 音轨/字幕的语言(使用两字符的国家代号)\n"
#endif
" -ss <position>   寻找到给定(多少秒或时分秒 hh:mm:ss 的)位置\n"
" -nosound         不播放声音\n"
" -fs              全屏播放 (或者用 -vm, -zoom, 详见于手册页)\n"
" -x <x> -y <y>    设置显示的分辨率(提供给 -vm 或者 -zoom 使用)\n"
" -sub <file>      指定字幕文件 (参见 -subfps, -subdelay)\n"
" -playlist <file> 指定播放列表文件\n"
" -vid x -aid y    选择用于播放的 x 视频流和 y 音频流通道号\n"
" -fps x -srate y  改变视频率为 x 帧秒(fps)和音频率为 y 赫兹(Hz)\n"
" -pp <quality>    使用后期处理过滤器/滤镜 (详见于手册页)\n"
" -framedrop       使用丢帧(用于慢机器)\n"
"\n"
"基本控制键: (完整的列表参见于手册页, 同时也请核查 input.conf)\n"
" <-  or  ->       后退/快进 10 秒\n"
" down or up       后退/快进 1 分钟\n"
" pgdown or pgup   后退/快进 10 分钟\n"
" < or >           跳到播放列表中的前一个/后一个\n"
" p or SPACE       暂停播放(按任意键继续)\n"
" q or ESC         停止播放并退出程序\n"
" + or -           调整音频延迟增加/减少 0.1 秒\n"
" o                循环 OSD 模式:  无/搜索条/搜索条加计时器\n"
" * or /           增加或减少 PCM 音量\n"
" x or z           调整字幕延迟增加/减少 0.1 秒\n"
" r or t           上/下调整字幕位置, 参见“-vf expand”\n"
"\n"
" * * *  详细内容，更多的(高级)选项和控制键，请参见手册页  * * *\n"
"\n";
#endif

// libmpcodecs/ad_dvdpcm.c:
#define MSGTR_SamplesWanted "这个格式的采样需要更好的支持。请联系开发者。\n"

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\n正在退出..\n"
#define MSGTR_ExitingHow "\n正在退出... (%s)\n"
#define MSGTR_Exit_quit "退出"
#define MSGTR_Exit_eof "文件结束"
#define MSGTR_Exit_error "致命错误"
#define MSGTR_IntBySignal "\nMPlayer 被 %d 信号中断(属于 %s 模块)\n"
#define MSGTR_NoHomeDir "找不到主(HOME)目录\n"
#define MSGTR_GetpathProblem "get_path(\"config\") 问题\n"
#define MSGTR_CreatingCfgFile "创建配置文件: %s\n"
#define MSGTR_BuiltinCodecsConf "使用内建默认的 codecs.conf 文件。\n"
#define MSGTR_CantLoadFont "不能加载位图字体: %s\n"
#define MSGTR_CantLoadSub "不能加载字幕: %s\n"
#define MSGTR_DumpSelectedStreamMissing "转储: 致命错误: 指定的流不存在!\n"
#define MSGTR_CantOpenDumpfile "打不开转储文件。\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS 在文件头中没有指定或者无效，用 -fps 选项。\n"
#define MSGTR_TryForceAudioFmtStr "尝试锁定为音频编解码器驱动族 %s...\n"
#define MSGTR_CantFindAudioCodec "找不到音频格式 0x%X 的编解码器。\n"
#define MSGTR_RTFMCodecs "请阅读 DOCS/zh/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "尝试锁定为视频编解码器驱动族 %s...\n"
#define MSGTR_CantFindVideoCodec "找不到匹配 -vo 所选的和视频格式 0x%X 的编解码器。\n"
#define MSGTR_CannotInitVO "致命错误: 无法初始化视频驱动!\n"
#define MSGTR_CannotInitAO "不能打开/初始化音频设备 -> 没声音。\n"
#define MSGTR_StartPlaying "开始播放...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ************************************************\n"\
"         ****      你的系统太“慢”了, 播放不了!     ****\n"\
"         ************************************************\n"\
" 可能的原因, 问题, 和解决办法：\n"\
"- 最常见的原因：损坏的或有错误的 _音频_ 驱动\n"\
"  - 试试 -ao sdl 或使用 ALSA  的 OSS 模拟。\n"\
"  - 试验不同的 -autosync 的值，不妨从 30 开始。\n"\
"- 视频输出太慢\n"\
"  - (参考 -vo help)试试 -vo 用不同的驱动或者试试 -framedrop！\n"\
"- CPU 太慢\n"\
"  - 不要试图在慢速 CPU 上播放大的 DVD/DivX! 试试一些 lavdopts 选项,\n"\
"    例如: -vfm ffmpeg -lavdopts lowres=1:fast:skiploopfilter=all。\n"\
"- 损坏的文件\n"\
"  - 试试下列选项的各种组合: -nobps -ni -forceidx -mc 0。\n"\
"- 太慢的媒体(如 NFS/SMB 安挂点, DVD, VCD 等)\n"\
"  - 试试 -cache 8192。\n"\
"- 你在用 -cache 选项播放一个非交错的 AVI 文件？\n"\
"  - 试试 -nocache。\n"\
"阅读 DOCS/zh/video.html 和 DOCS/zh/sound.html 来寻找调整/加速的技巧。\n"\
"如果这些一个都用不上, 阅读 DOCS/zh/bugreports.html！\n\n"

#define MSGTR_NoGui "MPlayer 的编译没有支持 GUI。\n"
#define MSGTR_GuiNeedsX "MPlayer GUI 需要 X11。\n"
#define MSGTR_Playing "\n正在播放 %s。\n"
#define MSGTR_NoSound "音频: 没声音\n"
#define MSGTR_FPSforced "FPS 锁定为 %5.3f  (ftime: %5.3f)。\n"
#define MSGTR_CompiledWithRuntimeDetection "编译用了实时 CPU 检测。\n"
#define MSGTR_CompiledWithCPUExtensions "编译用了针对 x86 CPU 的扩展指令集:"
#define MSGTR_AvailableVideoOutputDrivers "可用的视频输出驱动:\n"
#define MSGTR_AvailableAudioOutputDrivers "可用的音频输出驱动:\n"
#define MSGTR_AvailableAudioCodecs "可用的音频编解码器:\n"
#define MSGTR_AvailableVideoCodecs "可用的视频编解码器:\n"
#define MSGTR_AvailableAudioFm "\n(已编译进的)可用的音频编解码器族/驱动:\n"
#define MSGTR_AvailableVideoFm "\n(已编译进的)可用的视频编解码器族/驱动:\n"
#define MSGTR_AvailableFsType "可用的全屏层变换模式:\n"
#define MSGTR_UsingRTCTiming "使用 Linux 的硬件 RTC 实计时 (%ldHz)。\n"
#define MSGTR_CannotReadVideoProperties "视频: 无法读取属性\n"
#define MSGTR_NoStreamFound "找不到流媒体。\n"
#define MSGTR_ErrorInitializingVODevice "打开/初始化 (-vo) 所选的视频输出设备出错。\n"
#define MSGTR_ForcedVideoCodec "锁定的视频编解码器: %s\n"
#define MSGTR_ForcedAudioCodec "锁定的音频编解码器: %s\n"
#define MSGTR_Video_NoVideo "视频: 没视频\n"
#define MSGTR_NotInitializeVOPorVO "\n致命错误: 无法初始化 (-vf) 视频过滤器或 (-vo) 视频输出。\n"
#define MSGTR_Paused "\n  =====  暂停  =====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\n无法装载播放列表 %s\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- “非法指令”导致 MPlayer 崩溃。\n"\
"  这可能是我们的新代码中运行时 CPU-检测的一个错误...\n"\
"  请阅读 DOCS/zh/bugreports.html。\n"
#define MSGTR_Exit_SIGILL \
"- “非法指令”导致 MPlayer 崩溃。\n"\
"  这通常发生在现在你所运行之上的 CPU 不同于\n"\
"  编译/优化时的 CPU 所造成的。\n"\
"  证实它！\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- 过度使用 CPU/FPU/RAM 导致 MPlayer 崩溃。\n"\
"  使用 --enable-debug 重新编译 MPlayer 并用调试程序“gdb”反跟踪和\n"\
"  反汇编。具体细节看 DOCS/zh/bugreports.html#crash。\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer 崩溃了。这不应该发生。\n"\
"  这可能是 MPlayer 代码中 _或者_ 你的驱动中 _或者_ 你的 gcc 版本中的一个\n"\
"  错误。如你觉得这是 MPlayer 的错误，请阅读 DOCS/zh/bugreports.html\n"\
"  并遵循上面的步骤报告错误。除非你在报告一个可能的错误时候提供我们\n"\
"  所需要的信息, 否则我们不能也不会帮助你。\n"
#define MSGTR_LoadingConfig "正在装载配置文件 '%s'\n"
#define MSGTR_AddedSubtitleFile "字幕: 添加字幕文件 (%d): %s\n"
#define MSGTR_RemovedSubtitleFile "字幕: 删除字幕文件 (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "打开写入文件 [%s] 失败!\n"
#define MSGTR_CommandLine "命令行: "
#define MSGTR_RTCDeviceNotOpenable "打开 %s 失败: %s (此文件应该能被用户读取。)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Linux RTC 初始化错误在 ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "试图添加 \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" 到你的系统启动脚本。\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Linux RTC 初始化错误在 ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "正在使用 %s 计时。\n"
#define MSGTR_NoIdleAndGui "GMPLayer 不能使用选项 -idle。\n"
#define MSGTR_MenuInitialized "菜单已初始化: %s\n"
#define MSGTR_MenuInitFailed "菜单初始化失败。\n"
#define MSGTR_Getch2InitializedTwice "警告: getch2_init 被调用两次!\n"
#define MSGTR_DumpstreamFdUnavailable "无法转储此流 - 没有可用的文件描述符。\n"
#define MSGTR_FallingBackOnPlaylist "回退到试着解析播放列表 %s...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "不能用根菜单 %s 打开 libmenu 视频过滤器。\n"
#define MSGTR_AudioFilterChainPreinitError "音频过滤器链预启动错误!\n"
#define MSGTR_LinuxRTCReadError "Linux RTC 读取错误: %s\n"
#define MSGTR_SoftsleepUnderflow "警告! Softsleep 向下溢出!\n"
#define MSGTR_DvdnavNullEvent "DVDNAV 事件为空?!\n"
#define MSGTR_DvdnavHighlightEventBroken "DVDNAV 事件: 高亮事件损坏\n"
#define MSGTR_DvdnavEvent "DVDNAV 事件: %s\n"
#define MSGTR_DvdnavHighlightHide "DVDNAV 事件: 高亮隐藏\n"
#define MSGTR_DvdnavStillFrame "######################################## DVDNAV 事件: 静止帧: %d秒\n"
#define MSGTR_DvdnavNavStop "DVDNAV 事件: Nav停止\n"
#define MSGTR_DvdnavNavNOP "DVDNAV 事件: Nav无操作\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "DVDNAV 事件: Nav SPU 流改变: 物理: %d/%d/%d 逻辑: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "DVDNAV 事件: Nav SPU 流改变: 物理: %d 逻辑: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "DVDNAV 事件: Nav 音频流改变: 物理: %d 逻辑: %d\n"
#define MSGTR_DvdnavNavVTSChange "DVDNAV 事件: Nav VTS 改变\n"
#define MSGTR_DvdnavNavCellChange "DVDNAV 事件: Nav Cell 改变\n"
#define MSGTR_DvdnavNavSpuClutChange "DVDNAV 事件: Nav SPU CLUT 改变\n"
#define MSGTR_DvdnavNavSeekDone "DVDNAV 事件: Nav 搜寻完成\n"
#define MSGTR_MenuCall "菜单调用\n"

#define MSGTR_EdlOutOfMem "不能分配足够的内存来保持 EDL 数据。\n"
#define MSGTR_EdlRecordsNo "读取 %d EDL 动作。\n"
#define MSGTR_EdlQueueEmpty "没有 EDL 动作要处理。\n"
#define MSGTR_EdlCantOpenForWrite "打不开 EDL 文件 [%s] 写入。\n"
#define MSGTR_EdlCantOpenForRead "打不开 EDL 文件 [%s] 读取。\n"
#define MSGTR_EdlNOsh_video "没有视频不能使用 EDL, 取消中。\n"
#define MSGTR_EdlNOValidLine "无效 EDL 线: %s\n"
#define MSGTR_EdlBadlyFormattedLine "错误格式的 EDL 线 [%d], 丢弃。\n"
#define MSGTR_EdlBadLineOverlap "上次停止的位置是 [%f]; 下次开始的位置在 [%f]。\n"\
"每一项必须按时间顺序, 不能重叠。 丢弃。\n"
#define MSGTR_EdlBadLineBadStop "停止时间必须是在开始时间之后。\n"
#define MSGTR_EdloutBadStop "EDL 跳跃已取消, 上次开始位置 > 停止位置\n"
#define MSGTR_EdloutStartSkip "EDL 跳跃开始, 再按键 'i' 以停止。\n"
#define MSGTR_EdloutEndSkip "EDL 跳跃结束, 线已写入。\n"
#define MSGTR_MPEndposNoSizeBased "MPlayer 的选项 -endpos 还不支持大小单位。\n"

// mplayer.c OSD

#define MSGTR_OSDenabled "已启用"
#define MSGTR_OSDdisabled "已停用"
#define MSGTR_OSDAudio "音频: %s"
#define MSGTR_OSDVideo "视频: %s"
#define MSGTR_OSDChannel "频道: %s"
#define MSGTR_OSDSubDelay "字幕延迟: %d 毫秒"
#define MSGTR_OSDSpeed "速度: x %6.2f"
#define MSGTR_OSDosd "OSD: %s"
#define MSGTR_OSDChapter "章节: (%d) %s"

// property values
#define MSGTR_Enabled "已启用"
#define MSGTR_EnabledEdl "已启用 EDL"
#define MSGTR_Disabled "已停用"
#define MSGTR_HardFrameDrop "强丢帧"
#define MSGTR_Unknown "未知"
#define MSGTR_Bottom "底部"
#define MSGTR_Center "中部"
#define MSGTR_Top "顶部"

// osd bar names
#define MSGTR_Volume "音量"
#define MSGTR_Panscan "摇移"
#define MSGTR_Gamma "Gamma"
#define MSGTR_Brightness "亮度"
#define MSGTR_Contrast "对比度"
#define MSGTR_Saturation "饱和度"
#define MSGTR_Hue "色调"

// property state
#define MSGTR_MuteStatus "静音: %s"
#define MSGTR_AVDelayStatus "A-V 延迟: %s"
#define MSGTR_OnTopStatus "总在最前: %s"
#define MSGTR_RootwinStatus "根窗口: %s"
#define MSGTR_BorderStatus "边框: %s"
#define MSGTR_FramedroppingStatus "丢帧: %s"
#define MSGTR_VSyncStatus "视频同步: %s"
#define MSGTR_SubSelectStatus "字幕: %s"
#define MSGTR_SubPosStatus "字幕位置: %s/100"
#define MSGTR_SubAlignStatus "字幕对齐: %s"
#define MSGTR_SubDelayStatus "字幕延迟: %s"
#define MSGTR_SubVisibleStatus "显示字幕: %s"
#define MSGTR_SubForcedOnlyStatus "只用锁定的字幕: %s"

// mencoder.c:

#define MSGTR_UsingPass3ControlFile "使用 pass3 控制文件: %s\n"
#define MSGTR_MissingFilename "\n没有文件名。\n\n"
#define MSGTR_CannotOpenFile_Device "打不开文件/设备。\n"
#define MSGTR_CannotOpenDemuxer "打不开分路器。\n"
#define MSGTR_NoAudioEncoderSelected "\n没有 (-oac) 所选的音频编码器。(参考 -oac help)选择一个或者使用 -nosound。\n"
#define MSGTR_NoVideoEncoderSelected "\n没有 (-ovc) 所选的视频解码器。(参考 -ovc help)选择一个。\n"
#define MSGTR_CannotOpenOutputFile "打不开输出文件 '%s'。\n"
#define MSGTR_EncoderOpenFailed "打开编码器失败。\n"
#define MSGTR_MencoderWrongFormatAVI "\n警告: 输出文件格式是 _AVI_。请查看 -of help。\n"
#define MSGTR_MencoderWrongFormatMPG "\n警告: 输出文件格式是 _MPEG_。请查看 -of help。\n"
#define MSGTR_MissingOutputFilename "没有指定输出文件, 请查看 -o 选项。"
#define MSGTR_ForcingOutputFourcc "锁定输出的 FourCC 为 %x [%.4s]。\n"
#define MSGTR_ForcingOutputAudiofmtTag "锁定输出音频格式标签为 0x%x。\n"
#define MSGTR_DuplicateFrames "\n%d 帧重复!\n"
#define MSGTR_SkipFrame "\n跳帧中!\n"
#define MSGTR_ResolutionDoesntMatch "\n新的视频文件的解析度或色彩空间和前一个不同。\n"
#define MSGTR_FrameCopyFileMismatch "\n所有的视频文件必须要有同样的帧率, 解析度和编解码器才能使用 -ovc copy。\n"
#define MSGTR_AudioCopyFileMismatch "\n所有的音频文件必须要有同样的音频编解码器和格式才能使用 -oac copy。\n"
#define MSGTR_NoAudioFileMismatch "\n不能把只有视频的文件和音频视频文件混合。试试 -nosound。\n"
#define MSGTR_NoSpeedWithFrameCopy "警告: -speed 不保证能和 -oac copy 一起正常工作!\n"\
"你的编码可能失败!\n"
#define MSGTR_ErrorWritingFile "%s: 写文件错误。\n"
#define MSGTR_FlushingVideoFrames "\n清空(flush)视频帧。\n"
#define MSGTR_FiltersHaveNotBeenConfiguredEmptyFile "过滤器尚未配置! 空文件?\n"
#define MSGTR_RecommendedVideoBitrate "%s CD 推荐的视频比特率为: %d\n"
#define MSGTR_VideoStreamResult "\n视频流: %8.3f kbit/s  (%d B/s)  大小: %"PRIu64" 字节  %5.3f 秒  %d 帧\n"
#define MSGTR_AudioStreamResult "\n音频流: %8.3f kbit/s  (%d B/s)  大小: %"PRIu64" 字节  %5.3f 秒\n"
#define MSGTR_EdlSkipStartEndCurrent "EDL跳过: 开始: %.2f  结束: %.2f   当前: V: %.2f  A: %.2f     \r"
#define MSGTR_OpenedStream "成功: 格式: %d数据: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "视频编解码器: 帧复制 (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "音频编解码器: 帧复制 (format=%x chans=%d rate=%d bits=%d B/s=%d sample-%d)\n"
#define MSGTR_CBRPCMAudioSelected "已选 CBR PCM 音频。\n"
#define MSGTR_MP3AudioSelected "已选 MP3音频。\n"
#define MSGTR_CannotAllocateBytes "不能分配 %d 字节。\n"
#define MSGTR_SettingAudioDelay "设置音频延迟为 %5.3fs。\n"
#define MSGTR_SettingVideoDelay "设置视频延迟为 %5.3fs。\n"
#define MSGTR_SettingAudioInputGain "设置音频输出增益为 %f。\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "限制音频预载值为 0.4s。\n"
#define MSGTR_IncreasingAudioDensity "增加音频密度为 4。\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "锁定音频预载值为 0, 最大 PTS 校验为 0。\n"
#define MSGTR_CBRAudioByterate "\n\nCBR 音频: %d 字节/秒, %d 字节/块\n"
#define MSGTR_LameVersion "LAME 版本 %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "错误: 在此预设值上指定的比特率超出有效范围。\n"\
"\n"\
"当使用这种模式时你必须给定一个在\"8\"到\"320\"之间的数值。\n"\
"\n"\
"更多信息，请试试: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "错误: 你没有给定一个有效的配置和/或预设值选项。\n"\
"\n"\
"可用的配置轮廓(profile)包括:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - ABR 模式是暗含的。要使用这个选项,\n"\
"                      简单地指定一个比特率就行了。例如:\n"\
"                      \"preset=185\"就可以激活这个\n"\
"                      预设值并使用 185 作为平均比特率。\n"\
"\n"\
"    一些例子:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"更多信息，请试试: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"预设值开关设计为提供最好的品质。\n"\
"\n"\
"它们大多数已经经过严格的双盲聆听测试来调整和检验性能,\n"\
"以达到我们预期的目标。\n"\
"\n"\
"它们不断地被升级以便和最新的发展保持一致,\n"\
"所以应该能给你提供目前 LAME 所能提供的将近最好的品质。\n"\
"\n"\
"预设值激活:\n"\
"\n"\
"   VBR 模式 (通常情况下的最高品质):\n"\
"\n"\
"     \"preset=standard\" 此项预设值显然应该是大多数人在处理大多数的音乐的时候\n"\
"                             所用到的选项, 它的品质已经相当高。\n" \
"\n"\
"     \"preset=extreme\" 如果你有极好的听力和相当的设备, 这项预设值一般会比\n"\
"                             \"standard\"模式的品质还要提高一点。\n"\
"\n"\
"   CBR 320kbps (预设值开关选项里的最高品质):\n"\
"\n"\
"     \"preset=insane\"  对于大多数人和在大多数情况下, 这个选项都显得有些过度。\n"\
"                             但是如果你一定要有最高品质并且完全不关心文件大小,\n"\
"                             那这正是适合你的。\n"\
"\n"\
"   ABR 模式 (根据给定比特率高品质, 但不及 VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  使用这个预设值总是会在一个指定的比特率下有不错的品质。\n"\
"                             根据的比特率, 预设值将会决定这种情况下所能达到最\n"\
"                             好效果的设置。\n"\
"                             虽然这种方法可行, 但它并没有 VBR 模式那么灵活, \n"\
"                             同样, 一般也达不到 VBR 在高比特率下的同等品质。\n"\
"\n"\
"以下选项在相应的配置文件里也可使用:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - ABR 模式是暗含的。要使用这个选项,\n"\
"                      简单地指定一个比特率就行了。例如:\n"\
"                      \"preset=185\"就可以激活这个\n"\
"                      预设值并使用 185 作为平均比特率。\n"\
"\n"\
"   \"fast\" - 在一个特定的配置文件里启用这新的快速 VBR 模式。\n"\
"            速度切换的坏处是比特率常常要比一般情况下的稍高, \n"\
"            品质也会稍低一点。\n"\
"      警告: 在当前版本下, 快速预设值可能比一般模式偏高得太多。\n"\
"\n"\
"   \"cbr\"  - 如果你使用 ABR 模式(见上)时, 采用特定的比特率, 如\n"\
"            80, 96, 112, 128, 160, 192, 224, 256, 320, 你可以使\n"\
"            用\"cbr\"选项强制以 CBR 模式代替标准 ABR 模式编码。\n"\
"            ABR 固然提供更高的品质, 但是 CBR 在某些情况下可能会\n"\
"            相当重要, 比如从 internet 送一个 MP3 流。\n"\
"\n"\
"    例如:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" 或 \"-lameopts  cbr:preset=192       \"\n"\
" 或 \"-lameopts      preset=172       \"\n"\
" 或 \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"ABR 模式下一些可用的别名:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit \
"不能设定 LAME 选项, 检查比特率/采样率, 一些\n"\
"非常低的比特率(<32)需要低采样率(如 -srate 8000)。\n"\
"如果都不行, 试试使用预设值。"
#define MSGTR_ConfigFileError "配置文件错误"
#define MSGTR_ErrorParsingCommandLine "解析命令行错误"
#define MSGTR_VideoStreamRequired "视频流是必须的!\n"
#define MSGTR_ForcingInputFPS "输入帧率将被替换为 %5.2f。\n"
#define MSGTR_RawvideoDoesNotSupportAudio "RAWVIDEO 输出文件格式不支持音频 - 停用音频。\n"
#define MSGTR_DemuxerDoesntSupportNosound "目前此分路器还不支持 -nosound。\n"
#define MSGTR_MemAllocFailed "内存分配失败。\n"
#define MSGTR_NoMatchingFilter "没找到匹配的 filter/ao 格式!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, C 编译器可能挂了?\n"
#define MSGTR_NoLavcAudioCodecName "音频 LAVC, 没有编解码器名!\n"
#define MSGTR_LavcAudioCodecNotFound "音频 LAVC, 找不到对应的编码器 %s。\n"
#define MSGTR_CouldntAllocateLavcContext "音频 LAVC, 不能分配上下文!\n"
#define MSGTR_CouldntOpenCodec "打不开编解码器 %s, br=%d。\n"
#define MSGTR_CantCopyAudioFormat "音频格式 0x%x 和 '-oac copy' 不兼容, 请试试用 '-oac pcm' 代替或者用 '-fafmttag'。\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     可变比特率方式\n"\
"                0: cbr (常比特率)\n"\
"                1: mt (Mark Taylor VBR 算法)\n"\
"                2: rh (Robert Hegemann VBR 算法 - 默认)\n"\
"                3: abr (平均比特率)\n"\
"                4: mtrh (Mark Taylor Robert Hegemann VBR 算法)\n"\
"\n"\
" abr           平均比特率\n"\
"\n"\
" cbr           常比特率\n"\
"               也会在后继 ABR 预设值模式中强制以 CBR 模式编码。\n"\
"\n"\
" br=<0-1024>   以 kBit 为单位设置比特率 (仅用于 CBR 和 ABR)\n"\
"\n"\
" q=<0-9>       编码质量 (0-最高, 9-最低) (仅用于 VBR)\n"\
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
" fast          在后继 VBR 预设值模式中切换到更快的编码方式，\n"\
"               品质稍低而比特率稍高。\n"\
"\n"\
" preset=<value> 可能提供最高品质的设置。\n"\
"                 medium: VBR 编码，品质：好\n"\
"                 (比特率范围 150-180 kbps)\n"\
"                 standard:  VBR 编码, 品质：高\n"\
"                 (比特率范围 170-210 kbps)\n"\
"                 extreme: VBR 编码，品质：非常高\n"\
"                 (比特率范围 200-240 kbps)\n"\
"                 insane:  CBR 编码，品质：最高\n"\
"                 (比特率 320 kbps)\n"\
"                 <8-320>: 以给定比特率为平均比特率的 ABR 编码。\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "重复的 FourCC"
#define MSGTR_TooManyFourccs "太多的 FourCCs/formats..."
#define MSGTR_ParseError "解析错误"
#define MSGTR_ParseErrorFIDNotNumber "解析错误(格式 ID 不是一个数字?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "解析错误(格式 ID 别名不是一个数字?)"
#define MSGTR_DuplicateFID "重复的格式 ID"
#define MSGTR_TooManyOut "太多输出..."
#define MSGTR_InvalidCodecName "\n编解码器(%s) 的名称无效!\n"
#define MSGTR_CodecLacksFourcc "\n编解码器(%s) 没有 FourCC/format!\n"
#define MSGTR_CodecLacksDriver "\n编解码器(%s) 没有驱动!\n"
#define MSGTR_CodecNeedsDLL "\n编解码器(%s) 需要一个 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\n编解码器(%s) 需要一个 'outfmt'!\n"
#define MSGTR_CantAllocateComment "不能为注释分配内存。"
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "读取 %s: "
#define MSGTR_CantOpenFileError "打不开 '%s': %s\n"
#define MSGTR_CantGetMemoryForLine "不能为 'line' 获取内存: %s\n"
#define MSGTR_CantReallocCodecsp "不能重新分配 '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "编解码器名 '%s' 不唯一。"
#define MSGTR_CantStrdupName "不能 strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "不能 strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "不能 strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "不能 strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d 音频和 %d 视频编解码器\n"
#define MSGTR_CodecDefinitionIncorrect "编解码器没有正确定义。"
#define MSGTR_OutdatedCodecsConf "此 codecs.conf 太旧，与当前的 MPlayer 不兼容!"

// fifo.c
#define MSGTR_CannotMakePipe "不能建立 PIPE!\n"

// parser-mecmd.c, parser-mpcmd.c
#define MSGTR_NoFileGivenOnCommandLine "'--' 表示没有更多选项, 但命令行没有给出文件名。\n"
#define MSGTR_TheLoopOptionMustBeAnInteger "这个loop选项必须是个整数: %s\n"
#define MSGTR_UnknownOptionOnCommandLine "命令行有未知的选项: -%s\n"
#define MSGTR_ErrorParsingOptionOnCommandLine "解析命令行选项出错: -%s\n"
#define MSGTR_InvalidPlayEntry "无效的播放条目 %s\n"
#define MSGTR_NotAnMEncoderOption "-%s 不是一个MEncoder选项\n"
#define MSGTR_NoFileGiven "没有给出文件\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "保存从 lvl %d 里找到的 slot 太旧: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "选项 %s 不能在配置文件里使用。\n"
#define MSGTR_InvalidCmdlineOption "选项 %s 不能在命令行里使用。\n"
#define MSGTR_InvalidSuboption "错误: 选项 '%s' 没有子选项 '%s'。\n"
#define MSGTR_MissingSuboptionParameter "错误: 子选项 '%s' (属于选项 '%s') 必须要有一个参数!\n"
#define MSGTR_MissingOptionParameter "错误: 选项 '%s' 必须要有一个参数!\n"
#define MSGTR_OptionListHeader "\n 名字                 类型            最小       最大     全局  命令行 配置文件\n\n"
#define MSGTR_TotalOptions "\n总共: %d 个选项\n"
#define MSGTR_ProfileInclusionTooDeep "警告: 配置轮廓(Profile)引用太深。\n"
#define MSGTR_NoProfileDefined "没有定义配置轮廓(Profile)。\n"
#define MSGTR_AvailableProfiles "可用的配置轮廓(Profile):\n"
#define MSGTR_UnknownProfile "未知的配置轮廓(Profile) '%s'。\n"
#define MSGTR_Profile "配置轮廓(Profile) %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n 名称                 类型            最小        最大\n\n"
#define MSGTR_TotalProperties "\n总计: %d 条属性\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "找不到 CD-ROM 设备 '%s'。\n"
#define MSGTR_ErrTrackSelect "选择 VCD 轨迹出错。"
#define MSGTR_ReadSTDIN "从标准输入中读取...\n"
#define MSGTR_UnableOpenURL "无法打开 URL: %s\n"
#define MSGTR_ConnToServer "连接到服务器: %s\n"
#define MSGTR_FileNotFound "找不到文件: '%s'\n"

#define MSGTR_SMBInitError "不能初始 libsmbclient 库: %d\n"
#define MSGTR_SMBFileNotFound "打不开局域网内的: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer 没有编译成支持 SMB 的读取。\n"

#define MSGTR_CantOpenDVD "打不开 DVD 设备: %s\n"

// stream_dvd.c
#define MSGTR_DVDspeedCantOpen "不能以写方式打开DVD设备, 改变DVD速度需要写方式。\n"
#define MSGTR_DVDrestoreSpeed "恢复DVD速度... "
#define MSGTR_DVDlimitSpeed "限制DVD速度至 %dKB/s... "
#define MSGTR_DVDlimitFail "限制DVD速度失败。\n"
#define MSGTR_DVDlimitOk "限制DVD速度成功。\n"
#define MSGTR_NoDVDSupport "MPlayer 编译成不支持 DVD，退出中。\n"
#define MSGTR_DVDnumTitles "此 DVD 有 %d 个标题。\n"
#define MSGTR_DVDinvalidTitle "无效的 DVD 标题号: %d\n"
#define MSGTR_DVDnumChapters "此 DVD 标题有 %d 章节。\n"
#define MSGTR_DVDinvalidChapter "无效的 DVD 章节号: %d\n"
#define MSGTR_DVDinvalidChapterRange "无效的章节范围 %s\n"
#define MSGTR_DVDinvalidLastChapter "上次无效的 DVD 章节号: %d\n"
#define MSGTR_DVDnumAngles "此 DVD 标题有 %d 个视角。\n"
#define MSGTR_DVDinvalidAngle "无效的 DVD 视角号: %d\n"
#define MSGTR_DVDnoIFO "打不开 DVD 标题 %d 的 IFO 文件。\n"
#define MSGTR_DVDnoVMG "打不开 VMG 信息!\n"
#define MSGTR_DVDnoVOBs "打不开标题的 VOBS (VTS_%02d_1.VOB)。\n"
#define MSGTR_DVDnoMatchingAudio "未找到匹配的 DVD 音频语言!\n"
#define MSGTR_DVDaudioChannel "已选 DVD 音频通道: %d 语言: %c%c\n"
#define MSGTR_DVDaudioStreamInfo "音频流: %d 格式: %s (%s) 语言: %s aid: %d。\n"
#define MSGTR_DVDnumAudioChannels "盘上的音频通道数: %d。\n"
#define MSGTR_DVDnoMatchingSubtitle "未找到匹配的 DVD 字幕语言!\n"
#define MSGTR_DVDsubtitleChannel "已选 DVD 字幕通道: %d 语言: %c%c\n"
#define MSGTR_DVDsubtitleLanguage "字幕号(sid): %d 语言: %s\n"
#define MSGTR_DVDnumSubtitles "盘上的字幕数: %d\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "流太多!"
#define MSGTR_RawMuxerOnlyOneStream "Rawaudio 合路器只支持一个音频流!\n"
#define MSGTR_IgnoringVideoStream "忽略视频流!\n"
#define MSGTR_UnknownStreamType "警告, 未知的流类型: %d\n"
#define MSGTR_WarningLenIsntDivisible "警告, 长度不能被采样率整除!\n"
#define MSGTR_MuxbufMallocErr "合路器帧缓冲无法分配内存!\n"
#define MSGTR_MuxbufReallocErr "合路器帧缓冲无法重新分配内存!\n"
#define MSGTR_MuxbufSending "合路器帧缓冲正在发送 %d 帧到合路器。\n"
#define MSGTR_WritingHeader "正在写帧头...\n"
#define MSGTR_WritingTrailer "正在写索引...\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "警告: 音频流头部 %d 被重新定义。\n"
#define MSGTR_VideoStreamRedefined "警告: 视频流头部 %d 被重新定义。\n"
#define MSGTR_TooManyAudioInBuffer "\n缓冲中音频包太多(%d in %d 字节)。\n"
#define MSGTR_TooManyVideoInBuffer "\n缓冲中视频包太多(%d in %d 字节)。\n"
#define MSGTR_MaybeNI "(也许你播放了一个非交错的流/文件或者是编解码失败)?\n" \
		      "对于 AVI 文件, 尝试用 -ni 选项锁定非交错模式。\n"
#define MSGTR_WorkAroundBlockAlignHeaderBug "AVI: 绕过 CBR-MP3 nBlockAlign 头部错误!\n"
#define MSGTR_SwitchToNi "\n检测到糟糕的交错格式的 AVI 文件 - 切换到 -ni 模式...\n"
#define MSGTR_InvalidAudioStreamNosound "AVI: 无效的音频流 ID: %d - 忽略 (nosound)\n"
#define MSGTR_InvalidAudioStreamUsingDefault "AVI: 无效的视频流 ID: %d - 忽略 (使用默认值)\n"
#define MSGTR_ON2AviFormat "ON2 AVI 格式"
#define MSGTR_Detected_XXX_FileFormat "检测到 %s 文件格式。\n"
#define MSGTR_DetectedAudiofile "检测到音频文件。\n"
#define MSGTR_NotSystemStream "非 MPEG 系统的流格式... (可能是输送流?)\n"
#define MSGTR_InvalidMPEGES "MPEG-ES 流无效??? 请联系作者, 这可能是个错误:(\n"
#define MSGTR_FormatNotRecognized "============= 抱歉, 此文件格式无法辨认或支持 ===============\n"\
				  "===     如果此文件是一个 AVI, ASF 或 MPEG 流, 请联系作者!    ===\n"
#define MSGTR_SettingProcessPriority "设置进程优先级: %s\n"
#define MSGTR_FilefmtFourccSizeFpsFtime "[V] 文件格式:%d  fourcc:0x%X  大小:%dx%d  帧速:%5.2f  帧时间:=%6.4f\n"
#define MSGTR_CannotInitializeMuxer "不能初始化muxer。"
#define MSGTR_MissingVideoStream "未找到视频流。\n"
#define MSGTR_MissingAudioStream "未找到音频流...  -> 没声音。\n"
#define MSGTR_MissingVideoStreamBug "没有视频流!? 请联系作者, 这可能是个错误:(\n"

#define MSGTR_DoesntContainSelectedStream "分路: 文件中没有所选择的音频或视频流。\n"

#define MSGTR_NI_Forced "锁定为"
#define MSGTR_NI_Detected "检测到"
#define MSGTR_NI_Message "%s 非交错 AVI 文件模式!\n"

#define MSGTR_UsingNINI "使用非交错的损坏的 AVI 文件格式。\n"
#define MSGTR_CouldntDetFNo "无法决定帧数(用于绝对搜索)。\n"
#define MSGTR_CantSeekRawAVI "无法在原始的 AVI 流中搜索。(需要索引, 尝试使用 -idx 选项。)  \n"
#define MSGTR_CantSeekFile "不能在此文件中搜索。\n"

#define MSGTR_MOVcomprhdr "MOV: 支持压缩的文件头需要 ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: 警告: 检测到可变的 FourCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: 警告: 轨迹太多。"
#define MSGTR_FoundAudioStream "==> 找到音频流: %d\n"
#define MSGTR_FoundVideoStream "==> 找到视频流: %d\n"
#define MSGTR_DetectedTV "检测到 TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "无法打开 Ogg 分路器。\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: 寻找音频流 (id:%d)。\n"
#define MSGTR_CannotOpenAudioStream "打不开音频流: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "打不开字幕流: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "打开音频分路器: %s 失败\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "打开字幕分路器: %s 失败\n"
#define MSGTR_TVInputNotSeekable "TV 输入不能搜索! (可能搜索应该用来更换频道;)\n"
#define MSGTR_DemuxerInfoChanged "分路器信息 %s 已变成 %s\n"
#define MSGTR_ClipInfo "剪辑信息: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 检测到 30fps 的 NTSC 内容, 改变帧率中。\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 检测到 24fps 渐进的 NTSC 内容, 改变帧率中。\n"

#define MSGTR_CacheFill "\r缓存填充: %5.2f%% (%"PRId64" 字节)   "
#define MSGTR_NoBindFound "找不到键 '%s' 的键绑定。"
#define MSGTR_FailedToOpen "打开 %s 失败。\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "打不开解码器。\n"
#define MSGTR_CantCloseCodec "不能关闭解码器。\n"

#define MSGTR_MissingDLLcodec "错误: 打不开所需的 DirectShow 编解码器: %s\n"
#define MSGTR_ACMiniterror "不能加载/初始化 Win32/ACM 音频解码器(缺少 DLL 文件?)。\n"
#define MSGTR_MissingLAVCcodec "在 libavcodec 中找不到解码器 '%s'...\n"

#define MSGTR_MpegNoSequHdr "MPEG: 致命错误: 搜索序列头时遇到 EOF。\n"
#define MSGTR_CannotReadMpegSequHdr "致命错误: 不能读取序列头。\n"
#define MSGTR_CannotReadMpegSequHdrEx "致命错误: 不能读取序列头扩展。\n"
#define MSGTR_BadMpegSequHdr "MPEG: 糟糕的序列头。\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: 糟糕的序列头扩展。\n"

#define MSGTR_ShMemAllocFail "不能分配共享内存。\n"
#define MSGTR_CantAllocAudioBuf "不能分配音频输出缓冲。\n"

#define MSGTR_UnknownAudio "未知或缺少音频格式 -> 没有声音\n"

#define MSGTR_UsingExternalPP "[PP] 使用外部的后处理过滤器, max q = %d。\n"
#define MSGTR_UsingCodecPP "[PP] 使用编解码器的后处理过滤器, max q = %d。\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "所选的 vo & vd 不支持视频属性 '%s'。\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "请求的视频编解码器族 [%s] (vfm=%s) 不可用。\n请在编译时启用它。\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "请求的音频编解码器族 [%s] (afm=%s) 不可用。\n请在编译时启用它。\n"
#define MSGTR_OpeningVideoDecoder "打开视频解码器: [%s] %s\n"
#define MSGTR_SelectedVideoCodec "已选视频编解码器: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "打开音频解码器: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "已选音频编解码器: [%s] afm: %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "为 %dHz/%dch/%s -> %dHz/%dch/%s 建造音频过滤链...\n"
#define MSGTR_UninitVideoStr "反初始视频: %s\n"
#define MSGTR_UninitAudioStr "反初始音频: %s\n"
#define MSGTR_VDecoderInitFailed "VDecoder 初始化失败 :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder 初始化失败 :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder 预初始化失败 :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: 为输入缓冲分配 %d 字节。\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: 为输出缓冲分配 %d + %d = %d 字节。\n"

// LIRC:
#define MSGTR_SettingUpLIRC "起动红外遥控支持...\n"
#define MSGTR_LIRCopenfailed "打开红外遥控支持失败。你将无法使用遥控器。\n"
#define MSGTR_LIRCcfgerr "读取 LIRC 配置文件 %s 失败。\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "找不到视频滤镜 '%s'。\n"
#define MSGTR_CouldNotOpenVideoFilter "打不开视频滤镜 '%s'。\n"
#define MSGTR_OpeningVideoFilter "打开视频滤镜: "
#define MSGTR_CannotFindColorspace "找不到匹配的色彩空间, 甚至靠插入 'scale' 也不行 :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: 编解码器无法设置 sh->disp_w 和 sh->disp_h, 尝试绕过。\n"
#define MSGTR_VoConfigRequest "VDec: vo 配置请求 - %d x %d (色彩空间首选项: %s)\n"
#define MSGTR_UsingXAsOutputCspNoY "VDec: 使用 %s 作为输出 csp (没有 %d)\n"
#define MSGTR_CouldNotFindColorspace "找不到匹配的色彩空间 - 重新尝试 -vf scale...\n"
#define MSGTR_MovieAspectIsSet "电影宽高比为 %.2f:1 - 预放大到正确的电影宽高比。\n"
#define MSGTR_MovieAspectUndefined "电影宽高比未定义 - 没使用预放大。\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "你需要升级/安装二进制编解码器包。\n请访问 http:\/\/www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "信息: Win32/DShow 视频编解码器初始化成功。\n"
#define MSGTR_DMOInitOK "信息: Win32/DMO 视频编解码器初始化成功。\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: 不能发送 EWMH 全屏事件!\n"
#define MSGTR_CouldNotFindXScreenSaver "xscreensaver_disable: 找不到屏保(XScreenSaver)窗口。\n"
#define MSGTR_SelectedVideoMode "XF86VM: 已选视频模式 %dx%d (图像大小 %dx%d)。\n"

#define MSGTR_InsertingAfVolume "[混音器] 没有硬件混音, 插入音量过滤器。\n"
#define MSGTR_NoVolume "[混音器] 没有可用的音量控制。\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "关于"
#define MSGTR_FileSelect "选择文件..."
#define MSGTR_SubtitleSelect "选择字幕..."
#define MSGTR_OtherSelect "选择..."
#define MSGTR_AudioFileSelect "选择外部音频通道..."
#define MSGTR_FontSelect "选择字体..."
// Note: If you change MSGTR_PlayList please see if it still fits MSGTR_MENU_PlayList
#define MSGTR_PlayList "播放列表"
#define MSGTR_Equalizer "均衡器"
#define MSGTR_ConfigureEqualizer "配置均衡器"
#define MSGTR_SkinBrowser "皮肤浏览器"
#define MSGTR_Network "网络流媒体..."
// Note: If you change MSGTR_Preferences please see if it still fits MSGTR_MENU_Preferences
#define MSGTR_Preferences "首选项"
#define MSGTR_AudioPreferences "音频驱动配置"
#define MSGTR_NoMediaOpened "没有打开媒体"
#define MSGTR_VCDTrack "VCD 轨迹 %d"
#define MSGTR_NoChapter "没有章节"
#define MSGTR_Chapter "章节 %d"
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
#define MSGTR_NEMDB "抱歉, 没有足够的内存用于绘制缓冲。"
#define MSGTR_NEMFMR "抱歉, 没有足够的内存用于菜单渲染。"
#define MSGTR_IDFGCVD "抱歉, 未找到 GUI-兼容的视频输出驱动。"
#define MSGTR_NEEDLAVC "抱歉, 不能用没有重编码的 DXR3/H+ 设备播放 non-MPEG 文件\n请启用 DXR3/H+ 配置盒中的 lavc。"
#define MSGTR_UNKNOWNWINDOWTYPE "发现未知窗口类型 ..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] 皮肤配置文件的 %d: %s行出错"
#define MSGTR_SKIN_WARNING1 "[skin] 警告: 在配置文件的 %d行:\n找到组件 (%s) 但在这之前没有找到 \"section\""
#define MSGTR_SKIN_WARNING2 "[skin] 警告: 在配置文件的 %d行:\n找到组件 (%s) 但在这之前没有找到 \"subsection\""
#define MSGTR_SKIN_WARNING3 "[skin] 警告: 在配置文件的 %d行:\n组件 (%s) 不支持此 subsection"
#define MSGTR_SKIN_SkinFileNotFound "[skin] 文件 (%s) 没找到。\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skin] 文件 (%s) 不可读。\n"
#define MSGTR_SKIN_BITMAP_16bit  "不支持少于 16 比特色深的位图 (%s)。\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "找不到文件 (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP 读取错误 (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA 读取错误 (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG 读取错误 (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "不支持 RLE 格式压缩的 TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "未知的文件格式 (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "24 比特到 32 比特的转换发生错误 (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "未知信息: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "内存不够\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "声明字体太多。\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "找不到字体文件。\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "找不到字体图像文件。\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "不存在的字体标签 (%s)\n"
#define MSGTR_SKIN_UnknownParameter "未知参数 (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "找不到皮肤 (%s)。\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "没找到选定的皮肤 (%s), 试着使用默认的...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "皮肤配置文件 (%s) 读取错误。\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "关于 MPlayer"
#define MSGTR_MENU_Open "打开..."
#define MSGTR_MENU_PlayFile "播放文件..."
#define MSGTR_MENU_PlayVCD "播放 VCD..."
#define MSGTR_MENU_PlayDVD "播放 DVD..."
#define MSGTR_MENU_PlayURL "播放 URL..."
#define MSGTR_MENU_LoadSubtitle "加载字幕..."
#define MSGTR_MENU_DropSubtitle "丢弃字幕..."
#define MSGTR_MENU_LoadExternAudioFile "加载外部音频文件..."
#define MSGTR_MENU_Playing "播放控制"
#define MSGTR_MENU_Play "播放"
#define MSGTR_MENU_Pause "暂停"
#define MSGTR_MENU_Stop "停止"
#define MSGTR_MENU_NextStream "下一个"
#define MSGTR_MENU_PrevStream "上一个"
#define MSGTR_MENU_Size "尺寸"
#define MSGTR_MENU_HalfSize   "一半尺寸"
#define MSGTR_MENU_NormalSize "正常尺寸"
#define MSGTR_MENU_DoubleSize "双倍尺寸"
#define MSGTR_MENU_FullScreen "全屏"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "打开盘..."
#define MSGTR_MENU_ShowDVDMenu "显示 DVD 菜单"
#define MSGTR_MENU_Titles "标题"
#define MSGTR_MENU_Title "标题 %2d"
#define MSGTR_MENU_None "(none)"
#define MSGTR_MENU_Chapters "章节"
#define MSGTR_MENU_Chapter "章节 %2d"
#define MSGTR_MENU_AudioLanguages "音频语言"
#define MSGTR_MENU_SubtitleLanguages "字幕语言"
#define MSGTR_MENU_PlayList MSGTR_PlayList
#define MSGTR_MENU_SkinBrowser "皮肤浏览器"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "退出..."
#define MSGTR_MENU_Mute "静音"
#define MSGTR_MENU_Original "原始的"
#define MSGTR_MENU_AspectRatio "宽高比"
#define MSGTR_MENU_AudioTrack "音频轨迹"
#define MSGTR_MENU_Track "轨迹 %d"
#define MSGTR_MENU_VideoTrack "视频轨迹"
#define MSGTR_MENU_Subtitles "字幕"

// --- equalizer
// Note: If you change MSGTR_EQU_Audio please see if it still fits MSGTR_PREFERENCES_Audio
#define MSGTR_EQU_Audio "音频"
// Note: If you change MSGTR_EQU_Video please see if it still fits MSGTR_PREFERENCES_Video
#define MSGTR_EQU_Video "视频"
#define MSGTR_EQU_Contrast "对比度: "
#define MSGTR_EQU_Brightness "亮度: "
#define MSGTR_EQU_Hue "色调: "
#define MSGTR_EQU_Saturation "饱和度: "
#define MSGTR_EQU_Front_Left "前左"
#define MSGTR_EQU_Front_Right "前右"
#define MSGTR_EQU_Back_Left "后左"
#define MSGTR_EQU_Back_Right "后右"
#define MSGTR_EQU_Center "中间"
#define MSGTR_EQU_Bass "低音"
#define MSGTR_EQU_All "所有"
#define MSGTR_EQU_Channel1 "声道 1:"
#define MSGTR_EQU_Channel2 "声道 2:"
#define MSGTR_EQU_Channel3 "声道 3:"
#define MSGTR_EQU_Channel4 "声道 4:"
#define MSGTR_EQU_Channel5 "声道 5:"
#define MSGTR_EQU_Channel6 "声道 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "路径"
#define MSGTR_PLAYLIST_Selected "所选文件"
#define MSGTR_PLAYLIST_Files "所有文件"
#define MSGTR_PLAYLIST_DirectoryTree "目录树"

// --- preferences
#define MSGTR_PREFERENCES_Audio MSGTR_EQU_Audio
#define MSGTR_PREFERENCES_Video MSGTR_EQU_Video
#define MSGTR_PREFERENCES_SubtitleOSD "字幕和 OSD "
#define MSGTR_PREFERENCES_Codecs "编解码器和分路器"
// Note: If you change MSGTR_PREFERENCES_Misc see if it still fits MSGTR_PREFERENCES_FRAME_Misc
#define MSGTR_PREFERENCES_Misc "其他"

#define MSGTR_PREFERENCES_None "None"
#define MSGTR_PREFERENCES_DriverDefault "默认驱动"
#define MSGTR_PREFERENCES_AvailableDrivers "可用驱动:"
#define MSGTR_PREFERENCES_DoNotPlaySound "不播放声音"
#define MSGTR_PREFERENCES_NormalizeSound "声音标准化"
#define MSGTR_PREFERENCES_EnableEqualizer "启用均衡器"
#define MSGTR_PREFERENCES_SoftwareMixer "启用软件混音器"
#define MSGTR_PREFERENCES_ExtraStereo "启用立体声加强"
#define MSGTR_PREFERENCES_Coefficient "参数:"
#define MSGTR_PREFERENCES_AudioDelay "音频延迟"
#define MSGTR_PREFERENCES_DoubleBuffer "启用双重缓冲"
#define MSGTR_PREFERENCES_DirectRender "启用直接渲染"
#define MSGTR_PREFERENCES_FrameDrop "启用丢帧"
#define MSGTR_PREFERENCES_HFrameDrop "启用强制丢帧(危险)"
#define MSGTR_PREFERENCES_Flip "上下翻转图像"
#define MSGTR_PREFERENCES_Panscan "摇移: "
#define MSGTR_PREFERENCES_OSDTimer "显示计时器和指示器"
#define MSGTR_PREFERENCES_OSDProgress "只显示进度条"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "计时器, 百分比和总时间"
#define MSGTR_PREFERENCES_Subtitle "字幕:"
#define MSGTR_PREFERENCES_SUB_Delay "延迟: "
#define MSGTR_PREFERENCES_SUB_FPS "帧率:"
#define MSGTR_PREFERENCES_SUB_POS "位置: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "停用字幕自动装载"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode 字幕"
#define MSGTR_PREFERENCES_SUB_MPSUB "转换给定的字幕成为 MPlayer 的字幕文件"
#define MSGTR_PREFERENCES_SUB_SRT "转换给定的字幕成为基于时间的 SubViewer (SRT) 格式"
#define MSGTR_PREFERENCES_SUB_Overlap "启/停用字幕重叠"
#define MSGTR_PREFERENCES_SUB_USE_ASS "SSA/ASS 字幕提供中"
#define MSGTR_PREFERENCES_SUB_ASS_USE_MARGINS "使用边空白"
#define MSGTR_PREFERENCES_SUB_ASS_TOP_MARGIN "上: "
#define MSGTR_PREFERENCES_SUB_ASS_BOTTOM_MARGIN "下: "
#define MSGTR_PREFERENCES_Font "字体:"
#define MSGTR_PREFERENCES_FontFactor "字体效果:"
#define MSGTR_PREFERENCES_PostProcess "启用后期处理"
#define MSGTR_PREFERENCES_AutoQuality "自动品质控制: "
#define MSGTR_PREFERENCES_NI "使用非交错的 AVI 解析器"
#define MSGTR_PREFERENCES_IDX "如果需要的话, 重建索引表"
#define MSGTR_PREFERENCES_VideoCodecFamily "视频解码器族:"
#define MSGTR_PREFERENCES_AudioCodecFamily "音频解码器族:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD 级别"
#define MSGTR_PREFERENCES_FRAME_Subtitle "字幕"
#define MSGTR_PREFERENCES_FRAME_Font "字体"
#define MSGTR_PREFERENCES_FRAME_PostProcess "后期处理"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "编解码器和分路器"
#define MSGTR_PREFERENCES_FRAME_Cache "缓存"
#define MSGTR_PREFERENCES_FRAME_Misc MSGTR_PREFERENCES_Misc
#define MSGTR_PREFERENCES_Audio_Device "设备:"
#define MSGTR_PREFERENCES_Audio_Mixer "混音器:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "混音通道:"
#define MSGTR_PREFERENCES_Message "请注意, 有些功能只有重新播放后能生效。"
#define MSGTR_PREFERENCES_DXR3_VENC "视频编码器:"
#define MSGTR_PREFERENCES_DXR3_LAVC "使用 LAVC (FFmpeg)"
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
#define MSGTR_PREFERENCES_FontEncoding21 "Windows 的西里尔语(CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Windows 的西里尔/中欧语(CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "不自动缩放"
#define MSGTR_PREFERENCES_FontPropWidth "宽度成比例"
#define MSGTR_PREFERENCES_FontPropHeight "高度成比例"
#define MSGTR_PREFERENCES_FontPropDiagonal "对角线成比例"
#define MSGTR_PREFERENCES_FontEncoding "编码:"
#define MSGTR_PREFERENCES_FontBlur "模糊:"
#define MSGTR_PREFERENCES_FontOutLine "轮廓:"
#define MSGTR_PREFERENCES_FontTextScale "文字缩放:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD 缩放:"
#define MSGTR_PREFERENCES_Cache "打开/关闭缓存"
#define MSGTR_PREFERENCES_CacheSize "缓存大小: "
#define MSGTR_PREFERENCES_LoadFullscreen "以全屏方式启动"
#define MSGTR_PREFERENCES_SaveWinPos "保存窗口位置"
#define MSGTR_PREFERENCES_XSCREENSAVER "停用屏保(XScreenSaver)"
#define MSGTR_PREFERENCES_PlayBar "使用播放条"
#define MSGTR_PREFERENCES_AutoSync "自同步 打开/关闭"
#define MSGTR_PREFERENCES_AutoSyncValue "自同步: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM 设备:"
#define MSGTR_PREFERENCES_DVDDevice "DVD 设备:"
#define MSGTR_PREFERENCES_FPS "电影的帧率:"
#define MSGTR_PREFERENCES_ShowVideoWindow "在非激活状态下显示视频窗口"
#define MSGTR_PREFERENCES_ArtsBroken "新版 aRts 与 GTK 1.x 不兼容, "\
           "会使 GMPlayer 崩溃!"

#define MSGTR_ABOUT_UHU "GUI 开发由 UHU Linux 赞助\n"
#define MSGTR_ABOUT_Contributors "代码和文档贡献者\n"
#define MSGTR_ABOUT_Codecs_libs_contributions "编解码器和第三方库\n"
#define MSGTR_ABOUT_Translations "翻译\n"
#define MSGTR_ABOUT_Skins "皮肤\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "致命错误!"
#define MSGTR_MSGBOX_LABEL_Error "错误!"
#define MSGTR_MSGBOX_LABEL_Warning "警告!"

// bitmap.c

#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] 内存不够, 容不下图片\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] 内存不够, 容不下图片\n"

// cfg.c

#define MSGTR_ConfigFileReadError "[cfg] 配置文件读取错误...\n"
#define MSGTR_UnableToSaveOption "[cfg] 无法保存 '%s' 选项。\n"

// interface.c

#define MSGTR_DeletingSubtitles "[GUI] 删除字幕。\n"
#define MSGTR_LoadingSubtitles "[GUI] 导入字幕: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] 添加视频过滤器: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] 删除视频过滤器: %s\n"

// mw.c

#define MSGTR_NotAFile "这好像不是文件: %s !\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] 打不开显示。\n"
#define MSGTR_WS_RemoteDisplay "[ws] 远程显示, 停用 XMITSHM。\n"
#define MSGTR_WS_NoXshm "[ws] 抱歉, 你的系统不支持 X 共享内存扩展。\n"
#define MSGTR_WS_NoXshape "[ws] 抱歉, 你的系统不支持 XShape 扩展。\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] 抱歉, 色彩深度太低。\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] 打开窗口太多。\n"
#define MSGTR_WS_ShmError "[ws] 共享内存扩展错误\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] 抱歉, 内存不够绘制缓冲。\n"
#define MSGTR_WS_DpmsUnavailable "DPMS 不可用?\n"
#define MSGTR_WS_DpmsNotEnabled "不能启用 DPMS。\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "这好像不是一个文件...\n"
#define MSGTR_WS_DDNothing "D&D: 没有任何东西返回!\n"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "选定的视频输出设备和这个编解码器不兼容。\n"\
                "试着添加缩放过滤器, 例如以 -vf spp,scale 来代替 -vf spp。\n"
#define MSGTR_VO_GenericError "这个错误已经发生"
#define MSGTR_VO_UnableToAccess "无法访问"
#define MSGTR_VO_ExistsButNoDirectory "已经存在, 但不是一个目录。"
#define MSGTR_VO_DirExistsButNotWritable "输出目录已经存在, 但是不可写。"
#define MSGTR_VO_DirExistsAndIsWritable "输出目录已经存在并且可写。"
#define MSGTR_VO_CantCreateDirectory "无法创建输出目录。"
#define MSGTR_VO_CantCreateFile "无法创建输出文件。"
#define MSGTR_VO_DirectoryCreateSuccess "输出目录创建成功。"
#define MSGTR_VO_ParsingSuboptions "解析子选项。"
#define MSGTR_VO_SuboptionsParsedOK "子选项解析成功。"
#define MSGTR_VO_ValueOutOfRange "值超出范围"
#define MSGTR_VO_NoValueSpecified "没有指定值。"
#define MSGTR_VO_UnknownSuboptions "未知子选项"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\n这是 aalib vo_aa 子选项:\n"
#define MSGTR_VO_AA_AdditionalOptions "vo_aa 提供的附加选项:\n" \
"  help        显示此帮助信息\n" \
"  osdcolor    设定 OSD 颜色\n  subcolor    设定字幕颜色\n" \
"        颜色参数有:\n           0 : 一般\n" \
"           1 : 模糊\n           2 : 粗\n           3 : 粗字体\n" \
"           4 : 反色\n           5 : 特殊\n\n\n"

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "启用渐显 JPEG。"
#define MSGTR_VO_JPEG_NoProgressiveJPEG "停用渐显 JPEG。"
#define MSGTR_VO_JPEG_BaselineJPEG "启用基线 JPEG。"
#define MSGTR_VO_JPEG_NoBaselineJPEG "停用基线 JPEG。"

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "启用 ASCII 模式。"
#define MSGTR_VO_PNM_RawMode "启用 Raw 模式。"
#define MSGTR_VO_PNM_PPMType "将要写入 PPM 文件。"
#define MSGTR_VO_PNM_PGMType "将要写入 PGM 文件。"
#define MSGTR_VO_PNM_PGMYUVType "将要写入 PGMYUV 文件。"

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "交错模式要求图像高度能被 4 整除。"
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "无法为交错模式分配线缓冲。"
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "输入不是 RGB, 不能按域分开色差!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "图像宽度必须能被 2 整除。"
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "内存不够, 不能分配 RGB 缓冲。"
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "不能取得内存或文件句柄以写入 \"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "图像写到输出错误!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "未知的子设备: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "使用交错输出模式, 前场(奇数图场)优先。"
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "使用交错输出模式, 后场(偶数图场)优先。"
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "使用(默认的) 渐显帧模式。"

// sub.c
#define MSGTR_VO_SUB_Seekbar "搜索条"
#define MSGTR_VO_SUB_Play "播放"
#define MSGTR_VO_SUB_Pause "暂停"
#define MSGTR_VO_SUB_Stop "停止"
#define MSGTR_VO_SUB_Rewind "后退"
#define MSGTR_VO_SUB_Forward "快进"
#define MSGTR_VO_SUB_Clock "计时"
#define MSGTR_VO_SUB_Contrast "对比度"
#define MSGTR_VO_SUB_Saturation "饱和度"
#define MSGTR_VO_SUB_Volume "音量"
#define MSGTR_VO_SUB_Brightness "亮度"
#define MSGTR_VO_SUB_Hue "色调"

// vo_xv.c
#define MSGTR_VO_XV_ImagedimTooHigh "源图像尺寸太大: %ux%u (上限是 %ux%u)\n"

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "PGM 视频输出驱动已经被 -vo pnm:pgmyuv 代替。\n"
#define MSGTR_VO_MD5_HasBeenReplaced "MD5 视频输出驱动已经被 -vo md5sum 代替。\n"

// ======================= AO Audio Output drivers ========================

// libao2

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "音频输出: alsa9 和 alsa1x 模块已被删除, 请用 -ao alsa 代替。\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] 音频设置: 无法打开混音器设备 %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] 音频设置: 声卡混音器没有'%s', 使用默认通道。\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] 音频设置: 无法打开音频设备 %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] 音频设置: 无法建立文件描述块: %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] 无法设定音频设备 %s 到 %s 的输出, 试着使用 %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] 音频设置: 设定音频设备到 %d 通道失败。\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] 音频设置: 驱动不支持 SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  你的音频驱动不支持 select()  ***\n 请用 config.h 中的 #undef HAVE_AUDIO_SELECT 重新编译 MPlayer!\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\n致命错误: *** 无法重新打开或重设音频设备 *** %s\n"
#define MSGTR_AO_OSS_UnknownUnsupportedFormat "[AO OSS] 未知/不支持的 OSS 格式: %x。\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] 已连接到声音设备。\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] 无法打开一个流。\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] 流已经打开。\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] 缓冲大小: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] 设定音量为 %d 失败。\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] 不支持 %d Hz, 试试重采样。\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound 失败: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] 延迟: [服务器: %0.2fs, 网络: %0.2fs] (调整 %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] 打开 ESD 播放流失败: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] DVB 音频设置混音器错误: %s。\n"
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] 不支持 %d Hz, 试试重采样。\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] 文件: %s (%s)\nPCM: 采样率: %iHz 通道: %s 格式 %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] 信息: 用 -vc null -vo null 可以更快速的转储\n[AO PCM] 信息: 如果要写 WAVE 文件, 使用 -ao pcm:waveheader (默认)。\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] 打开写 %s 失败!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] 采样率: %iHz 通道: %s 格式 %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] 使用 %s 音频驱动。\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] 不支持的音频格式: 0x%x。\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] SDL 音频初始化失败: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] 无法打开音频: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] 控制。\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] 初始: 采样率: %iHz 通道: %s 格式 %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] 播放: 无效的设备。\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] 初始: 设定参数失败: %s\n不能设定需要的采样率。\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] 初始: AL_RATE 在给定的源上不能用。\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] 初始: 获取参数失败: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] 初始: 当前的采样率为 %lf (需要的速率是 %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] 初始: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] 初始: 无法打开音频通道: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] 反初始: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] 重置: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] 音频暂停: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] 音频恢复: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO 失败。\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: 写失败。\n"
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] 无法打开音频设备 %s, %s  -> 没声音。\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] 音频设置: 你的声卡不支持 %d 通道, %s, %d Hz 采样率。\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  你的音频驱动不支持 select()  ***\n用 config.h 中的 #undef HAVE_AUDIO_SELECT 重新编译 MPlayer!\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\n致命错误: *** 无法重新打开或重设音频设备 (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-初始: 请求的格式: %d Hz, %d 通道, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-初始: 找不到声卡。\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-初始: 请求无效的格式 (%s) - 停用输出。\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-初始: 回放打开错误: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-初始: PCM 信息错误: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-初始: 找到 %d 声卡, 使用: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-初始: PCM 通道信息错误: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-初始: 设定参数错误: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-初始: 设定通道错误: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-初始: 通道准备错误: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-反初始: 回放排出(drain)错误: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-反初始: 回放清空(flush)错误: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-反初始: PCM 关闭错误: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-重置: 回放排出(drain)错误: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-重置: 回放清空(flush)错误: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-重置: 通道准备错误: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-暂停: 回放排出(drain)错误: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-暂停: 回放清空(flush)错误: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-恢复: 通道准备错误: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: alsa 未运行, 重新启动流。\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-播放: 回放准备错误: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-播放: 重启后写错误: %s - 放弃。\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-播放: 输出错误: %s\n"

// ao_alsa.c
#define MSGTR_AO_ALSA_InvalidMixerIndexDefaultingToZero "[AO_ALSA] 无效的混音索引。取默认值 0。\n"
#define MSGTR_AO_ALSA_MixerOpenError "[AO_ALSA] 混音打开错误: %s\n"
#define MSGTR_AO_ALSA_MixerAttachError "[AO_ALSA] 混音接上 %s 错误: %s\n"
#define MSGTR_AO_ALSA_MixerRegisterError "[AO_ALSA] 混音注册错误: %s\n"
#define MSGTR_AO_ALSA_MixerLoadError "[AO_ALSA] 混音装载错误: %s\n"
#define MSGTR_AO_ALSA_UnableToFindSimpleControl "[AO_ALSA] 无法找到控制 '%s',%i。\n"
#define MSGTR_AO_ALSA_ErrorSettingLeftChannel "[AO_ALSA] 错误设置左声道, %s\n"
#define MSGTR_AO_ALSA_ErrorSettingRightChannel "[AO_ALSA] 错误设置右声道, %s\n"
#define MSGTR_AO_ALSA_CommandlineHelp "\n[AO_ALSA] -ao alsa 命令行帮助:\n"\
"[AO_ALSA] 示例: mplayer -ao alsa:device=hw=0.3\n"\
"[AO_ALSA]   设置第一卡第四硬件设备。\n\n"\
"[AO_ALSA] 选项:\n"\
"[AO_ALSA]   noblock\n"\
"[AO_ALSA]     以 non-blocking 模式打开设备。\n"\
"[AO_ALSA]   device=<device-name>\n"\
"[AO_ALSA]     设置设备 (change , to . and : to =)\n"
#define MSGTR_AO_ALSA_ChannelsNotSupported "[AO_ALSA] %d 声道不被支持。\n"
#define MSGTR_AO_ALSA_CannotReadAlsaConfiguration "[AO_ALSA] 不能读取 ALSA 配置: %s\n"
#define MSGTR_AO_ALSA_CannotCopyConfiguration "[AO_ALSA] 不能拷贝配置: %s\n"
#define MSGTR_AO_ALSA_OpenInNonblockModeFailed "[AO_ALSA] 打开 nonblock-模式 失败, 试着打开 block-模式。\n"
#define MSGTR_AO_ALSA_PlaybackOpenError "[AO_ALSA] 回放打开错误: %s\n"
#define MSGTR_AO_ALSA_ErrorSetBlockMode "[AL_ALSA] 错误设置 block-模式 %s。\n"
#define MSGTR_AO_ALSA_UnableToGetInitialParameters "[AO_ALSA] 无法得到初始参数: %s\n"
#define MSGTR_AO_ALSA_UnableToSetAccessType "[AO_ALSA] 无法设置访问类型: %s\n"
#define MSGTR_AO_ALSA_FormatNotSupportedByHardware "[AO_ALSA] 格式 %s 不被硬件支持, 试试默认的。\n"
#define MSGTR_AO_ALSA_UnableToSetFormat "[AO_ALSA] 无法设置格式: %s\n"
#define MSGTR_AO_ALSA_UnableToSetChannels "[AO_ALSA] 无法设置声道: %s\n"
#define MSGTR_AO_ALSA_UnableToDisableResampling "[AO_ALSA] 无法停用再抽样: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSamplerate2 "[AO_ALSA] 无法设置 采样率-2: %s\n"
#define MSGTR_AO_ALSA_UnableToSetBufferTimeNear "[AO_ALSA] 无法设置缓冲时间约: %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodTime "[AO_ALSA] 无法设置区段时间: %s\n"
#define MSGTR_AO_ALSA_BufferTimePeriodTime "[AO_ALSA] buffer_time: %d, period_time :%d\n"
#define MSGTR_AO_ALSA_UnableToGetPeriodSize "[AO ALSA] 无法取得区段大小: %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodSize "[AO ALSA] 无法设置区段大小(%ld): %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriods "[AO_ALSA] 无法设置区段: %s\n"
#define MSGTR_AO_ALSA_UnableToSetHwParameters "[AO_ALSA] 无法设置 hw-parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBufferSize "[AO_ALSA] 无法取得缓冲大小: %s\n"
#define MSGTR_AO_ALSA_UnableToGetSwParameters "[AO_ALSA] 无法取得 sw-parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSwParameters "[AO_ALSA] 无法设置 sw-parameters: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBoundary "[AO_ALSA] 无法取得边界: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStartThreshold "[AO_ALSA] 无法设置开始点: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStopThreshold "[AO_ALSA] 无法设置停止点: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSilenceSize "[AO_ALSA] 无法设置安静大小: %s\n"
#define MSGTR_AO_ALSA_PcmCloseError "[AO_ALSA] pcm 关闭错误: %s\n"
#define MSGTR_AO_ALSA_NoHandlerDefined "[AO_ALSA] 没定义处理器!\n"
#define MSGTR_AO_ALSA_PcmPrepareError "[AO_ALSA] pcm 准备错误: %s\n"
#define MSGTR_AO_ALSA_PcmPauseError "[AO_ALSA] pcm 暂停错误: %s\n"
#define MSGTR_AO_ALSA_PcmDropError "[AO_ALSA] pcm 丢弃错误: %s\n"
#define MSGTR_AO_ALSA_PcmResumeError "[AO_ALSA] pcm 恢复错误: %s\n"
#define MSGTR_AO_ALSA_DeviceConfigurationError "[AO_ALSA] 设备配置错误。"
#define MSGTR_AO_ALSA_PcmInSuspendModeTryingResume "[AO_ALSA] Pcm 在挂机模式, 试着恢复。\n"
#define MSGTR_AO_ALSA_WriteError "[AO_ALSA] 写错误: %s\n"
#define MSGTR_AO_ALSA_TryingToResetSoundcard "[AO_ALSA] 试着重置声卡。\n"
#define MSGTR_AO_ALSA_CannotGetPcmStatus "[AO_ALSA] 不能取得 pcm 状态: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] 无效插件: %s\n"

// ======================= AF Audio Filters ================================

// libaf 

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "可用的标签"
#define MSGTR_AF_LADSPA_WarnNoInputs "警告! 此 LADSPA 插件没有音频输入。\n 以后的音频信号将会丢失。"
#define MSGTR_AF_LADSPA_ErrMultiChannel "现在还不支持多通道(>2)插件。\n 只能使用单声道或立体声道插件。"
#define MSGTR_AF_LADSPA_ErrNoOutputs "此 LADSPA 插件没有音频输出。"
#define MSGTR_AF_LADSPA_ErrInOutDiff "LADSPA 插件的音频输入和音频输出的数目不相等。"
#define MSGTR_AF_LADSPA_ErrFailedToLoad "装载失败"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "在指定的库文件里找不到 ladspa_descriptor() 函数。"
#define MSGTR_AF_LADSPA_ErrLabelNotFound "在插件库里找不到标签。"
#define MSGTR_AF_LADSPA_ErrNoSuboptions "没有指定子选项标签。"
#define MSGTR_AF_LADSPA_ErrNoLibFile "没有指定库文件。"
#define MSGTR_AF_LADSPA_ErrNoLabel "没有指定过滤器标签。"
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "命令行指定的控制项不够。"
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: 输入控制 #%d 低于下限 %0.4f。\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: 输入控制 #%d 高于上限 %0.4f。\n"

// format.c

#define MSGTR_AF_FORMAT_UnknownFormat "未知格式"

// ========================== INPUT =========================================

// joystick.c

#define MSGTR_INPUT_JOYSTICK_Opening "打开操纵杆设备 %s\n"
#define MSGTR_INPUT_JOYSTICK_CantOpen "打不开操纵杆设备 %s: %s\n"
#define MSGTR_INPUT_JOYSTICK_ErrReading "读操纵杆设备时发生错误: %s\n"
#define MSGTR_INPUT_JOYSTICK_LoosingBytes "操纵杆: 丢失了 %d 字节的数据\n"
#define MSGTR_INPUT_JOYSTICK_WarnLostSync "操纵杆: 警告初始事件, 失去了和驱动的同步。\n"
#define MSGTR_INPUT_JOYSTICK_WarnUnknownEvent "操作杆警告未知事件类型%d\n"

// input.c

#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "命令文件描述符太多, 不能注册文件描述符 %d。\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "键文件描述符太多, 无法注册文件描述符 %d。\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "命令 %s: 参数 %d 不是整数。\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "命令 %s: 参数 %d 不是浮点数。\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "命令 %s: 参数 %d 无结束符。\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "未知参数 %d\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "命令 %s 需要至少 %d 个参数, 然而只发现了 %d 个。\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "读取命令文件描述符 %d 时发生错误: %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "文件描述符 %d 的命令缓存已满: 正在丢失内容。\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "绑定键 %s 的命令无效"
#define MSGTR_INPUT_INPUT_ErrSelect "选定错误: %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "键输入文件描述符 %d 发生错误\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "键输入文件描述符 %d 得到死键\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "同时有太多的按键事件发生\n"
#define MSGTR_INPUT_INPUT_ErrOnCmdFd "命令文件描述符 %d 发生错误\n"
#define MSGTR_INPUT_INPUT_ErrReadingInputConfig "当读取输入配置文件 %s 时发生错误: %s\n"
#define MSGTR_INPUT_INPUT_ErrUnknownKey "未知键 '%s'\n"
#define MSGTR_INPUT_INPUT_ErrUnfinishedBinding "未完成的绑定 %s\n"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForKeyName "此键名的缓存太小: %s\n"
#define MSGTR_INPUT_INPUT_ErrNoCmdForKey "找不到键 %s 的命令"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForCmd "此命令的缓存太小: %s\n"
#define MSGTR_INPUT_INPUT_ErrWhyHere "怎么会运行到这里了?\n"
#define MSGTR_INPUT_INPUT_ErrCantInitJoystick "不能初始华输入法操纵杆\n"
#define MSGTR_INPUT_INPUT_ErrCantStatFile "不能统计(stat) %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantOpenFile "打不开 %s: %s\n"

// ========================== LIBMPDEMUX ===================================

// url.c

#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "字符转义好像已发生在 url_escape %c%c1%c2\n"

// ai_alsa1x.c

#define MSGTR_MPDEMUX_AIALSA1X_CannotSetSamplerate "无法设置采样率。\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetBufferTime "无法设置缓冲时间。\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetPeriodTime "无法设置间隔时间。\n"

// ai_alsa1x.c / ai_alsa.c

#define MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig "此 PCM 的配置文件损坏: 配置不可用。\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableAccessType "访问类型不可用。\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt "采样文件不可用。\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableChanCount "通道记数不可用 - 使用默认: %d\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams "无法安装硬件参数: %s"
#define MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize "不能使用等于缓冲大小的间隔 (%u == %lu)\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallSWParams "无法安装软件参数:\n"
#define MSGTR_MPDEMUX_AIALSA_ErrorOpeningAudio "打开音频错误: %s\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatusError "ALSA 状态错误: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUN "ALSA xrun!!! (至少 %.3f ms)\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatus "ALSA 状态:\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUNPrepareError "ALSA xrun: 准备错误: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaReadWriteError "ALSA 读/写错误"

// ai_oss.c

#define MSGTR_MPDEMUX_AIOSS_Unable2SetChanCount "无法设置通道数: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetStereo "无法设置立体声: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2Open "无法打开 '%s': %s\n"
#define MSGTR_MPDEMUX_AIOSS_UnsupportedFmt "不支持的格式\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt "无法设置音频格式。"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate "无法设置采样率: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger "无法设置触发器: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize "无法取得块大小!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero "音频块大小是零, 设成 %d!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low "音频块大小太小, 设成 %d!\n"

// asfheader.c

#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB "致命: 头部的大小超过 1 MB (%d)!\n请联系 MPlayer 的作者, 并且发送或上传此文件。\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "不能为头部分配 %d 字节的空间。\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "读 ASF 头部时遇到 EOF, 文件损坏或不完整?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "DVR 可能只能和 libavformat 一起工作, 如果有问题请试试 -demuxer 35\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "没有数据块紧随头部之后!\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF: 找不到音频或视频头部 - 文件损坏?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "无效的 ASF 头部长度!\n"

// asf_mmst_streaming.c

#define MSGTR_MPDEMUX_MMST_WriteError "写错误\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\n提醒! EOF 文件结束\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "头部预读取失败\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "无效的头部大小, 正在放弃。\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "读头部数据失败。\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "读 packet_len 失败。\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "无效的 RTSP 包大小, 正在放弃。\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "读命令数据失败。\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "头部对象\n"
#define MSGTR_MPDEMUX_MMST_DataObject "数据对象\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "文件对象, 包长 = %d (%d)\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "流对象, 流 ID: %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "ID 太多, 跳过流。"
#define MSGTR_MPDEMUX_MMST_UnknownObject "未知的对象\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "读媒体数据失败\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "签名缺失\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "一切结束。感谢你下载一个包含专利保护的媒体文件。\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "未知命令 %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "get_media_packet 错误 : %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "已连接\n"

// asf_streaming.c

#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "啊…… stream_chunck 大小太小了: %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "size_confirm 不匹配!: %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "警告: 丢掉头部????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "解析区块头部时发生错误\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "没取到作为第一个区块的头部!!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "不能分配 %d 字节的缓冲。\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "读网络流时发生错误。\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "错误: 区块太小。\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "错误: 子区块号无效。\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "带宽太小, 文件不能播放!\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "带宽太小, 取消选定音频流。\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "带宽太小, 取消选定视频流。\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "无效的 ASF 头部长度!\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "读区块头部时发生错误。\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "错误: chunk_size > packet_size\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "读区块时发生错误。\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> ASF 转向器\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "无效的代理 URL\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "未知的 ASF 流类型\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "解析 HTTP 响应失败。\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "服务器返回 %d:%s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "ASF HTTP 解析警告 : Pragma %s 被从 %d 字节切到 %d\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "Socket 写错误: %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "解析头部失败。\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "找不到流。\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "未知 ASF 流类型\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL: %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "失败, 正在退出。\n"

// audio_in.c

#define MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio "\n读音频错误: %s\n"
#define MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut "从交叉运行中恢复, 某些帧可能丢失了!\n"
#define MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover "致命错误, 无法恢复!\n"
#define MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples "\n音频采样不够!\n"

// aviheader.c

#define MSGTR_MPDEMUX_AVIHDR_EmptyList "**空列表?!\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundMovieAt "在 0x%X - 0x%X 找到电影\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader "找到 'bih', %u 字节的 %d\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1 "为 M$ mpg4v1 视频重新生成关键帧表。\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3 "为 DIVX3 视频重新生成关键帧表。\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4 "为 MPEG4 视频重新生成关键帧表。\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt "找到 'wf', %d 字节的 %d\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header "AVI: 发现 dmlh (size=%d) (total_frames=%d)\n"
#define MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames  "正在读 INDEX 块, %d 区块的 %d 帧 (fpos=%"PRId64")。\n"
#define MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr "附加的 RIFF 头...\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** 警告: 这不是扩展的 AVI 头部..\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenChunk "区块损坏?  chunksize=%d  (id=%.4s)\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI: ODML: 建造 ODML 索引 (%d superindexchunks)。\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI: ODML: 检测到损坏的(不完整的?)文件。将使用传统的索引。\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "不能读索引文件 %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "%s 不是有效的 MPlayer 索引文件。\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "无法为来自 %s 的索引数据分配内存。\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "过早结束的索引文件 %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "装载索引文件: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "正在生成索引: %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI: 为 %d 区块生成索引表!\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "无法写索引文件 %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "已保存索引文件: %s\n"

// cache2.c

#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\r此流不可缓存。\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!! read_filepos 不同!!! 请报告此错误...\n"

// cdda.c

#define MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice "打不开 CDDA 设备。\n"
#define MSGTR_MPDEMUX_CDDA_CantOpenDisc "打不开盘。\n"
#define MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks "发现音频 CD，共 %ld 音轨。\n"

// cddb.c

#define MSGTR_MPDEMUX_CDDB_FailedToReadTOC "读取 TOC 失败。\n"
#define MSGTR_MPDEMUX_CDDB_FailedToOpenDevice "打开 %s 设备失败。\n"
#define MSGTR_MPDEMUX_CDDB_NotAValidURL "不是有效的 URL\n"
#define MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest "发送 HTTP 请求失败。\n"
#define MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse "读取 HTTP 响应失败。\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND "没有发现。\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown "未知错误代码\n"
#define MSGTR_MPDEMUX_CDDB_NoCacheFound "找不到缓存。\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead "没有读出所有的 xmcd 文件。\n"
#define MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory "创建目录 %s 失败。\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten "没有写入所有的 xmcd 文件。\n"
#define MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned "返回了无效的 xmcd 数据库文件。\n"
#define MSGTR_MPDEMUX_CDDB_UnexpectedFIXME "意外。请修复\n"
#define MSGTR_MPDEMUX_CDDB_UnhandledCode "未处理的代码\n"
#define MSGTR_MPDEMUX_CDDB_UnableToFindEOL "无法找到行结束。\n"
#define MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle "解析完成，找到: %s\n"
#define MSGTR_MPDEMUX_CDDB_AlbumNotFound "没发现专辑。\n"
#define MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr "服务器返回: 命令语法错误\n"
#define MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable "没有可用的站点信息。\n"
#define MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel "获得协议级别失败。\n"
#define MSGTR_MPDEMUX_CDDB_NoCDInDrive "驱动器里没有 CD。\n"

// cue_read.c

#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] 意外的 cue 文件行: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] bin 文件名测试: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] 找不到 bin 文件 - 正在放弃。\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] 正在使用 bin 文件 %s。\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] 未知的 bin 文件模式。不应该发生。正在停止。\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] 打不开 %s。\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] 读取 %s 出错\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] 得到 bin 文件大小时出错。\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "音轨 %02d:  format=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] 意外的 bin 文件结束\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] 无法读取预载的 %d 字节。\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "CUE stream_open, filename=%s, track=%d, 可用音轨: %d -> %d\n"

// network.c

#define MSGTR_MPDEMUX_NW_UnknownAF "未知地址族 %d\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "正在解析 %s (为 %s)...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "不能为 %s 解析名字: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "正在连接到服务器 %s[%s]: %d...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "连接服务器失败: %s\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "选择失败。\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "连接超时\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "getsockopt 失败: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "连接错误: %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "无效的代理设置... 试着不用代理。\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "不能为 AF_INET 解析远程主机名。试着不用代理。\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "发送 HTTP 请求时发生错误: 没有发出所有请求。\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "读失败。\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "http_read_response 读进 0 (如: EOF)。\n"
#define MSGTR_MPDEMUX_NW_AuthFailed "认证失败。请使用 -user 和 -passwd 选项来指定你的\n"\
"用户名/密码, 以便提供给一组 URL, 或者使用 URL 格式:\n"\
"http://username:password@hostname/file\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "%s 需要认证\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "需要认证。\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "没有给定密码, 试着使用空密码。\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "服务器返回 %d: %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "缓存大小设为 %d K字节\n"

// demux_audio.c

#define MSGTR_MPDEMUX_AUDIO_UnknownFormat "音频分路器: 未知格式 %d。\n"

// demux_demuxers.c

#define MSGTR_MPDEMUX_DEMUXERS_FillBufferError "fill_buffer 错误: 分路器错误: 不是 vd, ad 或 sd。\n"

// demux_mkv.c
#define MSGTR_MPDEMUX_MKV_ZlibInitializationFailed "[mkv] zlib 初始化失败。\n"
#define MSGTR_MPDEMUX_MKV_ZlibDecompressionFailed "[mkv] zlib 解压失败。\n"
#define MSGTR_MPDEMUX_MKV_LzoInitializationFailed "[mkv] lzo 初始化失败。\n"
#define MSGTR_MPDEMUX_MKV_LzoDecompressionFailed "[mkv] lzo 解压失败。\n"
#define MSGTR_MPDEMUX_MKV_TrackEncrypted "[mkv] 轨迹号 %u 已加密但解密还没\n[mkv] 实现。跳过轨迹。\n"
#define MSGTR_MPDEMUX_MKV_UnknownContentEncoding "[mkv] 轨迹 %u 的内容编码类型未知。跳过轨迹。\n"
#define MSGTR_MPDEMUX_MKV_UnknownCompression "[mkv] 轨迹 %u 已压缩, 用了未知的/不支持的压缩\n[mkv] 算法(%u)。跳过轨迹。\n"
#define MSGTR_MPDEMUX_MKV_ZlibCompressionUnsupported "[mkv] 轨迹 %u 已用 zlib 压缩但 mplayer 还没编译成\n[mkv] 支持 zlib 压缩。跳过轨迹。\n"
#define MSGTR_MPDEMUX_MKV_TrackIDName "[mkv] 轨迹 ID %u: %s (%s) \"%s\", %s\n"
#define MSGTR_MPDEMUX_MKV_TrackID "[mkv] 轨迹 ID %u: %s (%s), %s\n"
#define MSGTR_MPDEMUX_MKV_UnknownCodecID "[mkv] 未知的/不支持的 CodecID (%s) 或者缺少的/坏的 CodecPrivate\n[mkv] 数据(轨迹 %u)。\n"
#define MSGTR_MPDEMUX_MKV_FlacTrackDoesNotContainValidHeaders "[mkv] FLAC 轨迹没含有效的头部。\n"
#define MSGTR_MPDEMUX_MKV_UnknownAudioCodec "[mkv] 未知的/不支持的音频编解码器 ID '%s' 对于轨迹 %u 或者缺少的/有缺点的\n[mkv] 编解码器私有数据。\n"
#define MSGTR_MPDEMUX_MKV_SubtitleTypeNotSupported "[mkv] 不支持字幕类型 '%s'。\n"
#define MSGTR_MPDEMUX_MKV_WillPlayVideoTrack "[mkv] 将播放视频轨迹 %u。\n"
#define MSGTR_MPDEMUX_MKV_NoVideoTrackFound "[mkv] 没有找到/所要的视频轨迹。\n"
#define MSGTR_MPDEMUX_MKV_NoAudioTrackFound "[mkv] 没有找到/所要的音频轨迹。\n"
#define MSGTR_MPDEMUX_MKV_WillDisplaySubtitleTrack "[mkv] 将播放字幕轨迹 %u。\n"
#define MSGTR_MPDEMUX_MKV_NoBlockDurationForSubtitleTrackFound "[mkv] 警告: 对于所找到的字幕轨迹没有 BlockDuration。\n"
#define MSGTR_MPDEMUX_MKV_TooManySublines "[mkv] Warning: 太多的字幕要渲染, 跳过。\n"
#define MSGTR_MPDEMUX_MKV_TooManySublinesSkippingAfterFirst "\n[mkv] 警告: 太多的字幕要渲染, %i 以后跳过。n"

// demux_nuv.c

#define MSGTR_MPDEMUX_NUV_NoVideoBlocksInFile "文件中没有视频块。\n"

// demux_xmms.c

#define MSGTR_MPDEMUX_XMMS_FoundPlugin "找到插件: %s (%s)。\n"
#define MSGTR_MPDEMUX_XMMS_ClosingPlugin "关闭插件: %s。\n"

// ========================== LIBMPMENU ===================================

// common

#define MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition "[MENU] 菜单定义中没有找到条目。\n"

// libmenu/menu.c
#define MSGTR_LIBMENU_SyntaxErrorAtLine "[MENU] 语法错误: 行 %d\n"
#define MSGTR_LIBMENU_MenuDefinitionsNeedANameAttrib "[MENU] 菜单定义需要名称属性 (行 %d)。\n"
#define MSGTR_LIBMENU_BadAttrib "[MENU] 错误属性 %s=%s，在菜单 '%s' 的 %d 行\n"
#define MSGTR_LIBMENU_UnknownMenuType "[MENU] 未知菜单类型 '%s' (行 %d)\n"
#define MSGTR_LIBMENU_CantOpenConfigFile "[MENU] 打不开菜单配置文件: %s\n"
#define MSGTR_LIBMENU_ConfigFileIsTooBig "[MENU] 配置文件过长 (> %d KB)\n"
#define MSGTR_LIBMENU_ConfigFileIsEmpty "[MENU] 配置文件为空。\n"
#define MSGTR_LIBMENU_MenuNotFound "[MENU] 没找到菜单 %s。\n"
#define MSGTR_LIBMENU_MenuInitFailed "[MENU] 菜单 '%s': 初始化失败。\n"
#define MSGTR_LIBMENU_UnsupportedOutformat "[MENU] 输出格式不支持!!!!\n"

// libmenu/menu_cmdlist.c
#define MSGTR_LIBMENU_ListMenuEntryDefinitionsNeedAName "[MENU] 列表菜单条目的定义需要名称 (行 %d)。\n"
#define MSGTR_LIBMENU_ListMenuNeedsAnArgument "[MENU] 列表菜单需要参数。\n"

// libmenu/menu_console.c
#define MSGTR_LIBMENU_WaitPidError "[MENU] Waitpid 错误: %s。\n"
#define MSGTR_LIBMENU_SelectError "[MENU] Select 错误。\n"
#define MSGTR_LIBMENU_ReadErrorOnChilds "[MENU] 子进程的文件描述符读取错误: %s。\n"
#define MSGTR_LIBMENU_ConsoleRun "[MENU] 终端运行: %s ...\n"
#define MSGTR_LIBMENU_AChildIsAlreadyRunning "[MENU] 子进程已经运行。\n"
#define MSGTR_LIBMENU_ForkFailed "[MENU] Fork 失败!!!\n"
#define MSGTR_LIBMENU_WriteError "[MENU] write 错误\n"

// libmenu/menu_filesel.c
#define MSGTR_LIBMENU_OpendirError "[MENU] opendir 错误: %s\n"
#define MSGTR_LIBMENU_ReallocError "[MENU] realloc 错误: %s\n"
#define MSGTR_LIBMENU_MallocError "[MENU] 内存分配错误: %s\n"
#define MSGTR_LIBMENU_ReaddirError "[MENU] readdir 错误: %s\n"
#define MSGTR_LIBMENU_CantOpenDirectory "[MENU] 打不开目录 %s。\n"

// libmenu/menu_param.c
#define MSGTR_LIBMENU_SubmenuDefinitionNeedAMenuAttribut "[MENU] 子菜单定义需要 'menu' 属性。\n"
#define MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed "[MENU] 首选项菜单条目的定义需要有效的 'property' 属性 (行 %d)。\n"
#define MSGTR_LIBMENU_PrefMenuNeedsAnArgument "[MENU] 首选项菜单需要参数。\n"

// libmenu/menu_pt.c
#define MSGTR_LIBMENU_CantfindTheTargetItem "[MENU] 找不到目标项 ????\n"
#define MSGTR_LIBMENU_FailedToBuildCommand "[MENU] 生成命令失败: %s。\n"

// libmenu/menu_txt.c
#define MSGTR_LIBMENU_MenuTxtNeedATxtFileName "[MENU] 文本菜单需要文本文件名(参数文件)。\n"
#define MSGTR_LIBMENU_MenuTxtCantOpen "[MENU] 打不开 %s。\n"
#define MSGTR_LIBMENU_WarningTooLongLineSplitting "[MENU] 警告, 行过长. 分割之。\n"
#define MSGTR_LIBMENU_ParsedLines "[MENU] 解析了行 %d。\n"

// libmenu/vf_menu.c
#define MSGTR_LIBMENU_UnknownMenuCommand "[MENU] 未知命令: '%s'。\n"
#define MSGTR_LIBMENU_FailedToOpenMenu "[MENU] 打开菜单失败: '%s'。\n"

// ========================== LIBMPCODECS ===================================

// libmpcodecs/ad_libdv.c
#define MSGTR_MPCODECS_AudioFramesizeDiffers "[AD_LIBDV] 警告! 音频帧大小不一致! read=%d  hdr=%d。\n"

// libmpcodecs/vd_dmo.c vd_dshow.c vd_vfw.c
#define MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec "[VD_DMO] 无法为 cinepak 编解码器分配图像。\n"

// libmpcodecs/vd_ffmpeg.c
#define MSGTR_MPCODECS_XVMCAcceleratedCodec "[VD_FFMPEG] XVMC 加速的编解码器。\n"
#define MSGTR_MPCODECS_ArithmeticMeanOfQP "[VD_FFMPEG] QP 的算术平均值: %2.4f, QP 的调和平均值: %2.4f\n"
#define MSGTR_MPCODECS_DRIFailure "[VD_FFMPEG] DRI 失败。\n"
#define MSGTR_MPCODECS_CouldntAllocateImageForCodec "[VD_FFMPEG] 无法为编解码器分配图像。\n"
#define MSGTR_MPCODECS_XVMCAcceleratedMPEG2 "[VD_FFMPEG] XVMC-加速的 MPEG-2。\n"
#define MSGTR_MPCODECS_TryingPixfmt "[VD_FFMPEG] 尝试 pixfmt=%d。\n"
#define MSGTR_MPCODECS_McGetBufferShouldWorkOnlyWithXVMC "[VD_FFMPEG] Mc_get_buffer 只能用于 XVMC 加速!!"
#define MSGTR_MPCODECS_UnexpectedInitVoError "[VD_FFMPEG] Init_vo 意外错误。\n"
#define MSGTR_MPCODECS_UnrecoverableErrorRenderBuffersNotTaken "[VD_FFMPEG] 无法恢复的错误, 渲染缓冲无法获得。\n"
#define MSGTR_MPCODECS_OnlyBuffersAllocatedByVoXvmcAllowed "[VD_FFMPEG] 只允许 vo_xvmc 分配的缓冲。\n"

// libmpcodecs/ve_lavc.c
#define MSGTR_MPCODECS_HighQualityEncodingSelected "[VE_LAVC] 已选高品质编码 (非实时)!\n"
#define MSGTR_MPCODECS_UsingConstantQscale "[VE_LAVC] 使用常数的 qscale = %f (VBR)。\n"

// libmpcodecs/ve_raw.c
#define MSGTR_MPCODECS_OutputWithFourccNotSupported "[VE_RAW] 不支持 FourCC [%x] 的 raw 输出!\n"
#define MSGTR_MPCODECS_NoVfwCodecSpecified "[VE_RAW] 未指定需要的 VfW 编解码器!!\n"

// libmpcodecs/vf_crop.c
#define MSGTR_MPCODECS_CropBadPositionWidthHeight "[CROP] 错误的位置/宽度/高度 - 切割区域在原始图像外!\n"

// libmpcodecs/vf_cropdetect.c
#define MSGTR_MPCODECS_CropArea "[CROP] 切割区域: X: %d..%d  Y: %d..%d  (-vf crop=%d:%d:%d:%d)。\n"

// libmpcodecs/vf_format.c, vf_palette.c, vf_noformat.c
#define MSGTR_MPCODECS_UnknownFormatName "[VF_FORMAT] 未知格式名: '%s'。\n"

// libmpcodecs/vf_framestep.c vf_noformat.c vf_palette.c vf_tile.c
#define MSGTR_MPCODECS_ErrorParsingArgument "[VF_FRAMESTEP] 解析参数错误。\n"

// libmpcodecs/ve_vfw.c
#define MSGTR_MPCODECS_CompressorType "压缩类型: %.4lx\n"
#define MSGTR_MPCODECS_CompressorSubtype "副压缩类型: %.4lx\n"
#define MSGTR_MPCODECS_CompressorFlags "压缩标记: %lu, 版本 %lu, ICM 版本: %lu\n"
#define MSGTR_MPCODECS_Flags "标记:"
#define MSGTR_MPCODECS_Quality "品质"

// libmpcodecs/vf_expand.c
#define MSGTR_MPCODECS_FullDRNotPossible "无法完全使用 DR, 尝试使用 SLICES!\n"
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupportSlices  "警告! 下一个滤镜不支持 SLICES, 等着 sig11...\n"
#define MSGTR_MPCODECS_FunWhydowegetNULL "为什么我们得到了 NULL??\n"

// libmpcodecs/vf_test.c, vf_yuy2.c, vf_yvu9.c
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupport "下一个滤镜/视频输出不支持 %s :(\n"

// ================================== LIBMPVO ====================================

// mga_common.c

#define MSGTR_LIBVO_MGA_ErrorInConfigIoctl "[MGA] mga_vid_config ioctl 错误 (mga_vid.o 版本错误?)"
#define MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule "[MGA] 无法在内核模块中获得 luma 值!\n"
#define MSGTR_LIBVO_MGA_CouldNotSetLumaValuesFromTheKernelModule "[MGA] 无法在内核模块中设置 luma 值!\n"
#define MSGTR_LIBVO_MGA_ScreenWidthHeightUnknown "[MGA] 屏幕宽度/高度未知!\n"
#define MSGTR_LIBVO_MGA_InvalidOutputFormat "[MGA] 无效的输出格式 %0X\n"
#define MSGTR_LIBVO_MGA_IncompatibleDriverVersion "[MGA] 你的 mga_vid 驱动版本与 MPlayer 的版本不兼容!\n"
#define MSGTR_LIBVO_MGA_UsingBuffers "[MGA] 使用 %d 缓冲。\n"
#define MSGTR_LIBVO_MGA_CouldntOpen "[MGA] 打不开: %s\n"
#define MGSTR_LIBVO_MGA_ResolutionTooHigh "[MGA] 原分辨率至少有一维大于 1023x1023。请用软件或用 -lavdopts lowres=1 重新缩放\n"

// libvo/vesa_lvo.c

#define MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported "[VESA_LVO] 这个分支已经不再维护。\n[VESA_LVO] 请使用 -vo vesa:vidix。\n"
#define MSGTR_LIBVO_VESA_CouldntOpen "[VESA_LVO] 打不开: '%s'\n"
#define MSGTR_LIBVO_VESA_InvalidOutputFormat "[VESA_LVI] 无效的输出格式: %s(%0X)\n"
#define MSGTR_LIBVO_VESA_IncompatibleDriverVersion "[VESA_LVO] 你的 fb_vid 驱动版本与 MPlayer 的版本不兼容!\n"

// libvo/vo_3dfx.c

#define MSGTR_LIBVO_3DFX_Only16BppSupported "[VO_3DFX] 只支持 16bpp!"
#define MSGTR_LIBVO_3DFX_VisualIdIs "[VO_3DFX] 可视 ID 是  %lx。\n"
#define MSGTR_LIBVO_3DFX_UnableToOpenDevice "[VO_3DFX] 无法打开 /dev/3dfx。\n"
#define MSGTR_LIBVO_3DFX_Error "[VO_3DFX] 错误: %d。\n"
#define MSGTR_LIBVO_3DFX_CouldntMapMemoryArea "[VO_3DFX] 没能映射 3dfx 内存区域: %p,%p,%d。\n"
#define MSGTR_LIBVO_3DFX_DisplayInitialized "[VO_3DFX] 初始化: %p。\n"
#define MSGTR_LIBVO_3DFX_UnknownSubdevice "[VO_3DFX] 未知子设备: %s。\n"

// libvo/aspect.c
#define MSGTR_LIBVO_ASPECT_NoSuitableNewResFound "[ASPECT] 警告: 无法找到新的合适的分辨率!\n"
#define MSGTR_LIBVO_ASPECT_NoNewSizeFoundThatFitsIntoRes "[ASPECT] 错误: 无法找到适合分辨率的新尺寸!\n"

// libvo/vo_dxr3.c

#define MSGTR_LIBVO_DXR3_UnableToLoadNewSPUPalette "[VO_DXR3] 无法载入新的 SPU 调色板!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetPlaymode "[VO_DXR3] 无法设置播放模式!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetSubpictureMode "[VO_DXR3] 无法设置 subpicture 模式!\n"
#define MSGTR_LIBVO_DXR3_UnableToGetTVNorm "[VO_DXR3] 无法获取电视制式!\n"
#define MSGTR_LIBVO_DXR3_AutoSelectedTVNormByFrameRate "[VO_DXR3] 利用帧速率自动选择电视制式: "
#define MSGTR_LIBVO_DXR3_UnableToSetTVNorm "[VO_DXR3] 无法设置电视制式!\n"
#define MSGTR_LIBVO_DXR3_SettingUpForNTSC "[VO_DXR3] 设置 NTSC。\n"
#define MSGTR_LIBVO_DXR3_SettingUpForPALSECAM "[VO_DXR3] 设置 PAL/SECAM。\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo43 "[VO_DXR3] 宽高比设为 4:3。\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo169 "[VO_DXR3] 宽高比设为 16:9。\n"
#define MSGTR_LIBVO_DXR3_OutOfMemory "[VO_DXR3] 内存耗尽\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateKeycolor "[VO_DXR3] 无法分配 keycolor!\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateExactKeycolor "[VO_DXR3] 无法精确分配 keycolor, 使用最接近的匹配 (0x%lx)。\n"
#define MSGTR_LIBVO_DXR3_Uninitializing "[VO_DXR3] 反初始化(释放资源)。\n"
#define MSGTR_LIBVO_DXR3_FailedRestoringTVNorm "[VO_DXR3] 恢复电视制式失败!\n"
#define MSGTR_LIBVO_DXR3_EnablingPrebuffering "[VO_DXR3] 启用预缓冲。\n"
#define MSGTR_LIBVO_DXR3_UsingNewSyncEngine "[VO_DXR3] 使用新的同步引擎。\n"
#define MSGTR_LIBVO_DXR3_UsingOverlay "[VO_DXR3] 使用覆盖。\n"
#define MSGTR_LIBVO_DXR3_ErrorYouNeedToCompileMplayerWithX11 "[VO_DXR3] 错误: 覆盖需要安装 X11 的库和头文件后编译。\n"
#define MSGTR_LIBVO_DXR3_WillSetTVNormTo "[VO_DXR3] 将电视制式设置为: "
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALPAL60 "自动调节电影的帧速率 (PAL/PAL-60)"
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALNTSC "自动调节电影的帧速率 (PAL/NTSC)"
#define MSGTR_LIBVO_DXR3_UseCurrentNorm "使用当前制式。"
#define MSGTR_LIBVO_DXR3_UseUnknownNormSuppliedCurrentNorm "未知制式，使用当前制式。"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTrying "[VO_DXR3] 打开 %s 写入错误, 尝试 /dev/em8300。\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingMV "[VO_DXR3] 打开 %s 写入错误, 尝试 /dev/em8300_mv。\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWell "[VO_DXR3] 打开 /dev/em8300 写入错误!\n跳出。\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellMV "[VO_DXR3] 打开 /dev/em8300_mv 写入错误!\n跳出。\n"
#define MSGTR_LIBVO_DXR3_Opened "[VO_DXR3] 打开: %s。\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingSP "[VO_DXR3] 打开 %s 写入错误, 尝试 /dev/em8300_sp。\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellSP "[VO_DXR3] 打开 /dev/em8300_sp 写入错误!\n跳出。\n"
#define MSGTR_LIBVO_DXR3_UnableToOpenDisplayDuringHackSetup "[VO_DXR3] 在 overlay hack 设置中无法打开显示设备!\n"
#define MSGTR_LIBVO_DXR3_UnableToInitX11 "[VO_DXR3] 无法初始化 X11!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayAttribute "[VO_DXR3] 设置覆盖属性失败。\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayScreen "[VO_DXR3] 设置覆盖屏幕失败!\n退出。\n"
#define MSGTR_LIBVO_DXR3_FailedEnablingOverlay "[VO_DXR3] 启用覆盖失败!\n退出。\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayBcs "[VO_DXR3] 设置 overlay bcs 失败!\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayYOffsetValues "[VO_DXR3] 获取覆盖的 Y-偏移量失败!\n退出。\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXOffsetValues "[VO_DXR3] 获取覆盖的 X-偏移量失败!\n退出。\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXScaleCorrection "[VO_DXR3] 获取覆盖的 X-比例校正失败!\n退出。\n"
#define MSGTR_LIBVO_DXR3_YOffset "[VO_DXR3] Y-偏移量: %d。\n"
#define MSGTR_LIBVO_DXR3_XOffset "[VO_DXR3] X-偏移量: %d。\n"
#define MSGTR_LIBVO_DXR3_XCorrection "[VO_DXR3] X-比例校正: %d。\n"
#define MSGTR_LIBVO_DXR3_FailedResizingOverlayWindow "[VO_DXR3] 设置覆盖窗口大小失败!\n"
#define MSGTR_LIBVO_DXR3_FailedSetSignalMix "[VO_DXR3] 设置信号混合失败!\n"

// libvo/vo_mga.c

#define MSGTR_LIBVO_MGA_AspectResized "[VO_MGA] aspect(): 改变大小为 %dx%d。\n"
#define MSGTR_LIBVO_MGA_Uninit "[VO] 反初始化(释放资源)!\n"

// libvo/vo_null.c

#define MSGTR_LIBVO_NULL_UnknownSubdevice "[VO_NULL] 未知子设备: %s。\n"
															
// libvo/vo_png.c

#define MSGTR_LIBVO_PNG_Warning1 "[VO_PNG] 警告: 压缩级别设置为 0, 停用压缩!\n"
#define MSGTR_LIBVO_PNG_Warning2 "[VO_PNG] 信息: 使用 -vo png:z=<n> 设置 0 到 9 的压缩级别。\n"
#define MSGTR_LIBVO_PNG_Warning3 "[VO_PNG] 信息: (0 = 不压缩, 1 = 最快，压缩率最低 - 9 最好，最慢的压缩)\n"
#define MSGTR_LIBVO_PNG_ErrorOpeningForWriting "\n[VO_PNG] 打开 '%s' 写入错误!\n"
#define MSGTR_LIBVO_PNG_ErrorInCreatePng "[VO_PNG] create_png 错误。\n"

// libvo/vo_sdl.c

#define MSGTR_LIBVO_SDL_CouldntGetAnyAcceptableSDLModeForOutput "[VO_SDL] 无法获得可用的 SDL 输出模式。\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailed "[VO_SDL] set_video_mode: SDL_SetVideoMode 失败: %s。\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailedFull "[VO_SDL] Set_fullmode: SDL_SetVideoMode 失败: %s。\n"
#define MSGTR_LIBVO_SDL_MappingI420ToIYUV "[VO_SDL] I420 映射到 IYUV。\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormat "[VO_SDL] 不支持的图像格式 (0x%X)。\n"
#define MSGTR_LIBVO_SDL_InfoPleaseUseVmOrZoom "[VO_SDL] 信息 - 请使用 -vm 或 -zoom 切换到最佳分辨率。\n"
#define MSGTR_LIBVO_SDL_FailedToSetVideoMode "[VO_SDL] 设置视频模式失败: %s。\n"
#define MSGTR_LIBVO_SDL_CouldntCreateAYUVOverlay "[VO_SDL] 没能创建 YUV 覆盖: %s。\n"
#define MSGTR_LIBVO_SDL_CouldntCreateARGBSurface "[VO_SDL] 没能创建 RGB 表面: %s。\n"
#define MSGTR_LIBVO_SDL_UsingDepthColorspaceConversion "[VO_SDL] 使用深度/颜色空间转换, 这会减慢速度 (%ibpp -> %ibpp)。\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormatInDrawslice "[VO_SDL] draw_slice 不支持的图像格式, 联系 MPlayer 的开发者!\n"
#define MSGTR_LIBVO_SDL_BlitFailed "[VO_SDL] Blit 失败: %s。\n"
#define MSGTR_LIBVO_SDL_InitializationFailed "[VO_SDL] 初始化 SDL 失败: %s。\n"
#define MSGTR_LIBVO_SDL_UsingDriver "[VO_SDL] 使用驱动: %s。\n"

// libvo/vobsub_vidix.c

#define MSGTR_LIBVO_SUB_VIDIX_CantStartPlayback "[VO_SUB_VIDIX] 不能开始播放: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantStopPlayback "[VO_SUB_VIDIX] 不能停止播放: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_InterleavedUvForYuv410pNotSupported "[VO_SUB_VIDIX] YUV410P 不支持交错的 UV。\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawsliceWasCalled "[VO_SUB_VIDIX] 调用 dummy vidix_draw_slice()。\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawframeWasCalled "[VO_SUB_VIDIX] 调用 dummy vidix_draw_frame()。\n"
#define MSGTR_LIBVO_SUB_VIDIX_UnsupportedFourccForThisVidixDriver "[VO_SUB_VIDIX] 此 VIDIX 驱动不支持 FourCC: %x (%s)。\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedResolution "[VO_SUB_VIDIX] 视频服务器不支持分辨率 (%dx%d), 支持的分辨率: %dx%d-%dx%d。\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedColorDepth "[VO_SUB_VIDIX] VIDIX 不支持视频服务器的色深 (%d)。\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantUpscaleImage "[VO_SUB_VIDIX] VIDIX 驱动不能放大图像 (%d%d -> %d%d)。\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantDownscaleImage "[VO_SUB_VIDIX] VIDIX 驱动不能缩小图像 (%d%d -> %d%d)。\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantConfigurePlayback "[VO_SUB_VIDIX] 不能配置回放: %s。\n"
#define MSGTR_LIBVO_SUB_VIDIX_YouHaveWrongVersionOfVidixLibrary "[VO_SUB_VIDIX] VIDIX 库版本错误。\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntFindWorkingVidixDriver "[VO_SUB_VIDIX] 无法找到能工作的 VIDIX 驱动。\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntGetCapability "[VO_SUB_VIDIX] 无法获得兼容性: %s。\n"
#define MSGTR_LIBVO_SUB_VIDIX_Description "[VO_SUB_VIDIX] 描述: %s。\n"
#define MSGTR_LIBVO_SUB_VIDIX_Author "[VO_SUB_VIDIX] 作者: %s。\n"

// libvo/vo_svga.c

#define MSGTR_LIBVO_SVGA_ForcedVidmodeNotAvailable "[VO_SVGA] 锁定的 vid_mode %d (%s) 不可用。\n"
#define MSGTR_LIBVO_SVGA_ForcedVidmodeTooSmall "[VO_SVGA] 锁定的 vid_mode %d (%s) 太小。\n"
#define MSGTR_LIBVO_SVGA_Vidmode "[VO_SVGA] Vid_mode: %d, %dx%d %dbpp。\n"
#define MSGTR_LIBVO_SVGA_VgasetmodeFailed "[VO_SVGA] Vga_setmode(%d) 失败。\n"
#define MSGTR_LIBVO_SVGA_VideoModeIsLinearAndMemcpyCouldBeUsed "[VO_SVGA] 线性的视频模式可以使用 memcpy 操作图像。\n"
#define MSGTR_LIBVO_SVGA_VideoModeHasHardwareAcceleration "[VO_SVGA] 硬件加速的视频模式可以使用 put_image。\n"
#define MSGTR_LIBVO_SVGA_IfItWorksForYouIWouldLikeToKnow "[VO_SVGA] 如果工作正常请告诉我。\n[VO_SVGA] (发送 `mplayer test.avi -v -v -v -v &> svga.log` 生成的日志文件)。谢!\n"
#define MSGTR_LIBVO_SVGA_VideoModeHas "[VO_SVGA] 视频模式有 %d 页。\n"
#define MSGTR_LIBVO_SVGA_CenteringImageStartAt "[VO_SVGA] 图像居中。始于 (%d,%d)\n"
#define MSGTR_LIBVO_SVGA_UsingVidix "[VO_SVGA] 使用 VIDIX. w=%i h=%i  mw=%i mh=%i\n"

// libvo/vo_syncfb.c

#define MSGTR_LIBVO_SYNCFB_CouldntOpen "[VO_SYNCFB] 打不开 /dev/syncfb 或 /dev/mga_vid。\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p3 "[VO_SYNCFB] 使用 YUV420P3 调色板。\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p2 "[VO_SYNCFB] 使用 YUV420P2 调色板。\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420 "[VO_SYNCFB] 使用 YUV420 调色板。\n"
#define MSGTR_LIBVO_SYNCFB_NoSupportedPaletteFound "[VO_SYNCFB] 找不到支持的调色板。\n"
#define MSGTR_LIBVO_SYNCFB_BesSourcerSize "[VO_SYNCFB] BES sourcer 尺寸: %d x %d。\n"
#define MSGTR_LIBVO_SYNCFB_FramebufferMemory "[VO_SYNCFB] 帧缓冲内存: %ld 在 %ld 缓冲。\n"
#define MSGTR_LIBVO_SYNCFB_RequestingFirstBuffer "[VO_SYNCFB] 申请第一个缓冲 #%d。\n"
#define MSGTR_LIBVO_SYNCFB_GotFirstBuffer "[VO_SYNCFB] 获得第一个缓冲 #%d。\n"
#define MSGTR_LIBVO_SYNCFB_UnknownSubdevice "[VO_SYNCFB] 未知子设备: %s。\n"

// libvo/vo_tdfxfb.c

#define MSGTR_LIBVO_TDFXFB_CantOpen "[VO_TDFXFB] 打不开 %s: %s。\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetFscreenInfo "[VO_TDFXFB] FBITGET_FSCREENINFO ioctl 出错: %s。\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetVscreenInfo "[VO_TDFXFB] FBITGET_VSCREENINFO ioctl 出错: %s。\n"
#define MSGTR_LIBVO_TDFXFB_ThisDriverOnlySupports "[VO_TDFXFB] 此驱动仅支持 3Dfx Banshee, Voodoo3 和 Voodoo 5。\n"
#define MSGTR_LIBVO_TDFXFB_OutputIsNotSupported "[VO_TDFXFB] %d bpp 输出不支持。\n"
#define MSGTR_LIBVO_TDFXFB_CouldntMapMemoryAreas "[VO_TDFXFB] 无法映射内存区域: %s。\n"
#define MSGTR_LIBVO_TDFXFB_BppOutputIsNotSupported "[VO_TDFXFB] %d bpp 输出不支持 (应该永远不会发生)。\n"
#define MSGTR_LIBVO_TDFXFB_SomethingIsWrongWithControl "[VO_TDFXFB] Eik! control() 出错。\n"
#define MSGTR_LIBVO_TDFXFB_NotEnoughVideoMemoryToPlay "[VO_TDFXFB] 没有足够的显存播放此片，尝试较低的分辨率。\n"
#define MSGTR_LIBVO_TDFXFB_ScreenIs "[VO_TDFXFB] 屏幕 %dx%d 色深 %d bpp, 输入 %dx%d 色深 %d bpp, 输出 %dx%d。\n"

// libvo/vo_tdfx_vid.c

#define MSGTR_LIBVO_TDFXVID_Move "[VO_TDXVID] Move %d(%d) x %d => %d。\n"
#define MSGTR_LIBVO_TDFXVID_AGPMoveFailedToClearTheScreen "[VO_TDFXVID] AGP move 清除屏幕失败。\n"
#define MSGTR_LIBVO_TDFXVID_BlitFailed "[VO_TDFXVID] Blit 失败。\n"
#define MSGTR_LIBVO_TDFXVID_NonNativeOverlayFormatNeedConversion "[VO_TDFXVID] 非本地支持的覆盖格式需要转换。\n"
#define MSGTR_LIBVO_TDFXVID_UnsupportedInputFormat "[VO_TDFXVID] 不支持的输入格式 0x%x。\n"
#define MSGTR_LIBVO_TDFXVID_OverlaySetupFailed "[VO_TDFXVID] 覆盖设置失败。\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOnFailed "[VO_TDFXVID] 覆盖打开失败。\n"
#define MSGTR_LIBVO_TDFXVID_OverlayReady "[VO_TDFXVID] 覆盖准备完成: %d(%d) x %d @ %d => %d(%d) x %d @ %d。\n"
#define MSGTR_LIBVO_TDFXVID_TextureBlitReady "[VO_TDFXVID] 纹理 blit 准备完成: %d(%d) x %d @ %d => %d(%d) x %d @ %d。\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOffFailed "[VO_TDFXVID] 覆盖关闭失败\n"
#define MSGTR_LIBVO_TDFXVID_CantOpen "[VO_TDFXVID] 打不开 %s: %s。\n"
#define MSGTR_LIBVO_TDFXVID_CantGetCurrentCfg "[VO_TDFXVID] 没能获得当前配置: %s。\n"
#define MSGTR_LIBVO_TDFXVID_MemmapFailed "[VO_TDFXVID] Memmap 失败 !!!!!\n"
#define MSGTR_LIBVO_TDFXVID_GetImageTodo "获得图像格式 todo。\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailed "[VO_TDFXVID] AGP move 失败。\n"
#define MSGTR_LIBVO_TDFXVID_SetYuvFailed "[VO_TDFXVID] 设置 YUV 失败。\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnYPlane "[VO_TDFXVID] AGP move 操作 Y plane 失败。\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnUPlane "[VO_TDFXVID] AGP move 操作 U plane 失败。\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnVPlane "[VO_TDFXVID] AGP move 操作 V plane 失败。\n"
#define MSGTR_LIBVO_TDFXVID_UnknownFormat "[VO_TDFXVID] 未知格式 0x%x。\n"

// libvo/vo_tga.c

#define MSGTR_LIBVO_TGA_UnknownSubdevice "[VO_TGA] 未知子设备: %s。\n"

// libvo/vo_vesa.c

#define MSGTR_LIBVO_VESA_FatalErrorOccurred "[VO_VESA] 发生致命错误! 不能继续。\n"
#define MSGTR_LIBVO_VESA_UnknownSubdevice "[VO_VESA] 未知子设备: '%s'。\n"
#define MSGTR_LIBVO_VESA_YouHaveTooLittleVideoMemory "[VO_VESA] 显存太小不能支持这个模式:\n[VO_VESA] 需要: %08lX 可用: %08lX。\n"
#define MSGTR_LIBVO_VESA_YouHaveToSpecifyTheCapabilitiesOfTheMonitor "[VO_VESA] 你需要设置显示器的兼容性。不要改变刷新率。\n"
#define MSGTR_LIBVO_VESA_UnableToFitTheMode "[VO_VESA] 模式超出显示器的限制。不要改变刷新率。\n"
#define MSGTR_LIBVO_VESA_DetectedInternalFatalError "[VO_VESA] 检测到内部致命错误: 初始化在预初始化前被调用。\n"
#define MSGTR_LIBVO_VESA_SwitchFlipIsNotSupported "[VO_VESA] -flip 命令不支持。\n"
#define MSGTR_LIBVO_VESA_PossibleReasonNoVbe2BiosFound "[VO_VESA] 可能的原因: 找不到 VBE2 BIOS。\n"
#define MSGTR_LIBVO_VESA_FoundVesaVbeBiosVersion "[VO_VESA] 找到 VESA VBE BIOS 版本 %x.%x 修订版本: %x。\n"
#define MSGTR_LIBVO_VESA_VideoMemory "[VO_VESA] 显存: %u Kb。\n"
#define MSGTR_LIBVO_VESA_Capabilites "[VO_VESA] VESA 兼容性: %s %s %s %s %s。\n"
#define MSGTR_LIBVO_VESA_BelowWillBePrintedOemInfo "[VO_VESA] !!! 下面显示 OEM 信息 !!!\n"
#define MSGTR_LIBVO_VESA_YouShouldSee5OemRelatedLines "[VO_VESA] 你应该看到 5 行 OEM 相关内容; 否则, 你的 vm86 有问题。\n"
#define MSGTR_LIBVO_VESA_OemInfo "[VO_VESA] OEM 信息: %s。\n"
#define MSGTR_LIBVO_VESA_OemRevision "[VO_VESA] OEM 版本: %x。\n"
#define MSGTR_LIBVO_VESA_OemVendor "[VO_VESA] OEM 发行商: %s。\n"
#define MSGTR_LIBVO_VESA_OemProductName "[VO_VESA] OEM 产品名: %s。\n"
#define MSGTR_LIBVO_VESA_OemProductRev "[VO_VESA] OEM 产品版本: %s。\n"
#define MSGTR_LIBVO_VESA_Hint "[VO_VESA] 提示: 为使用电视输出你需要在启动之前插入 TV 接口。\n"\
"[VO_VESA] 因为 VESA BIOS 只在自举的时候初始化自己。\n"
#define MSGTR_LIBVO_VESA_UsingVesaMode "[VO_VESA] 使用 VESA 模式 (%u) = %x [%ux%u@%u]\n"
#define MSGTR_LIBVO_VESA_CantInitializeSwscaler "[VO_VESA] 不能初始化软件缩放。\n"
#define MSGTR_LIBVO_VESA_CantUseDga "[VO_VESA] 不能使用 DGA。锁定区域切换模式。 :(\n"
#define MSGTR_LIBVO_VESA_UsingDga "[VO_VESA] 使用 DGA (物理资源: %08lXh, %08lXh)"
#define MSGTR_LIBVO_VESA_CantUseDoubleBuffering "[VO_VESA] 不能使用双缓冲: 显存不足。\n"
#define MSGTR_LIBVO_VESA_CantFindNeitherDga "[VO_VESA] 未找到 DGA 也不能重新分配窗口的大小。\n"
#define MSGTR_LIBVO_VESA_YouveForcedDga "[VO_VESA] 你锁定了 DGA。退出中\n"
#define MSGTR_LIBVO_VESA_CantFindValidWindowAddress "[VO_VESA] 未找到可用的窗口地址。\n"
#define MSGTR_LIBVO_VESA_UsingBankSwitchingMode "[VO_VESA] 使用区域切换模式 (物理资源: %08lXh, %08lXh)。\n"
#define MSGTR_LIBVO_VESA_CantAllocateTemporaryBuffer "[VO_VESA] 不能分配临时缓冲。\n"
#define MSGTR_LIBVO_VESA_SorryUnsupportedMode "[VO_VESA] 抱歉, 模式不支持 -- 试试 -x 640 -zoom。\n"
#define MSGTR_LIBVO_VESA_OhYouReallyHavePictureOnTv "[VO_VESA] 啊你的电视机上有图像了!\n"
#define MSGTR_LIBVO_VESA_CantInitialozeLinuxVideoOverlay "[VO_VESA] 不能初始化 Linux Video Overlay。\n"
#define MSGTR_LIBVO_VESA_UsingVideoOverlay "[VO_VESA] 使用视频覆盖: %s。\n"
#define MSGTR_LIBVO_VESA_CantInitializeVidixDriver "[VO_VESA] 不能初始化 VIDIX 驱动。\n"
#define MSGTR_LIBVO_VESA_UsingVidix "[VO_VESA] 使用 VIDIX 中。\n"
#define MSGTR_LIBVO_VESA_CantFindModeFor "[VO_VESA] 未找到适合 %ux%u@%u 的模式。\n"
#define MSGTR_LIBVO_VESA_InitializationComplete "[VO_VESA] VESA 初始化完成。\n"

// libvo/vo_x11.c

#define MSGTR_LIBVO_X11_DrawFrameCalled "[VO_X11] 调用 draw_frame()!!!!!!\n"

// libvo/vo_xv.c

#define MSGTR_LIBVO_XV_DrawFrameCalled "[VO_XV] 调用 draw_frame()!!!!!!\n"
#define MSGTR_LIBVO_XV_SharedMemoryNotSupported "[VO_XV] 不支持共享内存\n回复到正常 Xv。\n"
#define MSGTR_LIBVO_XV_XvNotSupportedByX11 "[VO_XV] 对不起, 此 X11 版本/驱动不支持 Xv\n[VO_XV] ******** 试试使用  -vo x11  或  -vo sdl  *********\n"
#define MSGTR_LIBVO_XV_XvQueryAdaptorsFailed  "[VO_XV] XvQueryAdaptors 失败.\n"
#define MSGTR_LIBVO_XV_InvalidPortParameter "[VO_XV] 无效端口参数, 端口 0 重载。\n"
#define MSGTR_LIBVO_XV_CouldNotGrabPort "[VO_XV] 不能抓取端口 %i.\n"
#define MSGTR_LIBVO_XV_CouldNotFindFreePort "[VO_XV] 未找到空闲 Xvideo 端口 - 或许另一过程已\n"\
"[VO_XV] 在使用。请关闭所有的应用程序再试。如果那样做\n"\
"[VO_XV] 没用, 请参见 'mplayer -vo help' 找其它 (非-xv) 视频输出驱动。\n"
#define MSGTR_LIBVO_XV_NoXvideoSupport "[VO_XV] 好像不存在 Xvideo 支持你可用的显卡。\n"\
"[VO_XV] 运行 'xvinfo' 证实有 Xv 的支持并阅读\n"\
"[VO_XV] DOCS/HTML/en/video.html#xv!\n"\
"[VO_XV] 请参见 'mplayer -vo help' 找其它 (非-xv) 视频输出驱动。\n"\
"[VO_XV] 试试 -vo x11.\n"


// loader/ldt_keeper.c

#define MSGTR_LOADER_DYLD_Warning "警告: 尝试使用 DLL 编解码器, 但是环境变量\n         DYLD_BIND_AT_LAUNCH 未设定。 这很可能造成程序崩溃。\n"

// stream/stream_radio.c

#define MSGTR_RADIO_ChannelNamesDetected "[radio] 检测到广播通道名。\n"
#define MSGTR_RADIO_FreqRange "[radio] 允许的频率范围是 %.2f-%.2f MHz。\n"
#define MSGTR_RADIO_WrongFreqForChannel "[radio] 错误的通道频率 %s\n"
#define MSGTR_RADIO_WrongChannelNumberFloat "[radio] 错误的通道号: %.2f\n"
#define MSGTR_RADIO_WrongChannelNumberInt "[radio] 错误的通道号: %d\n"
#define MSGTR_RADIO_WrongChannelName "[radio] 错误的通道名: %s\n"
#define MSGTR_RADIO_FreqParameterDetected "[radio] 检测到广播频率参数。\n"
#define MSGTR_RADIO_DoneParsingChannels "[radio] 解析通道完成。\n"
#define MSGTR_RADIO_GetTunerFailed "[radio] Warning: ioctl 获取调谐器失败: %s。设置 frac 为 %d。\n"
#define MSGTR_RADIO_NotRadioDevice "[radio] %s 决不是广播设备!\n"
#define MSGTR_RADIO_TunerCapLowYes "[radio] 调谐器调低了:是 frac=%d\n"
#define MSGTR_RADIO_TunerCapLowNo "[radio] 调谐器调低了:否 frac=%d\n"
#define MSGTR_RADIO_SetFreqFailed "[radio] ioctl 设定频率为 0x%x (%.2f) failed: %s\n"
#define MSGTR_RADIO_GetFreqFailed "[radio] ioctl 获取频率失败: %s\n"
#define MSGTR_RADIO_SetMuteFailed "[radio] ioctl 设定静音失败: %s\n"
#define MSGTR_RADIO_QueryControlFailed "[radio] ioctl 查询控制失败: %s\n"
#define MSGTR_RADIO_GetVolumeFailed "[radio] ioctl 获取音量失败: %s\n"
#define MSGTR_RADIO_SetVolumeFailed "[radio] ioctl 设定音量失败: %s\n"
#define MSGTR_RADIO_DroppingFrame "\n[radio] 太糟糕 - 丢失音频帧 (%d 字节)!\n"
#define MSGTR_RADIO_BufferEmpty "[radio] grab_audio_frame: 缓冲为空, 等待 %d 字节数据。\n"
#define MSGTR_RADIO_AudioInitFailed "[radio] audio_in_init 失败: %s\n"
#define MSGTR_RADIO_AudioBuffer "[radio] 音频捕获 - buffer=%d 字节 (block=%d 字节)。\n"
#define MSGTR_RADIO_AllocateBufferFailed "[radio] 不能分配音频缓冲 (block=%d,buf=%d): %s\n"
#define MSGTR_RADIO_CurrentFreq "[radio] 当前频率: %.2f\n"
#define MSGTR_RADIO_SelectedChannel "[radio] 已选通道: %d - %s (freq: %.2f)\n"
#define MSGTR_RADIO_ChangeChannelNoChannelList "[radio] 不能改变通道: 无给定的通道列表。\n"
#define MSGTR_RADIO_UnableOpenDevice "[radio] 无法打开 '%s': %s\n"
#define MSGTR_RADIO_RadioDevice "[radio] 广播设备 fd: %d, %s\n"
#define MSGTR_RADIO_InitFracFailed "[radio] init_frac 失败。\n"
#define MSGTR_RADIO_WrongFreq "[radio] 错误频率: %.2f\n"
#define MSGTR_RADIO_UsingFreq "[radio] 使用频率: %.2f。\n"
#define MSGTR_RADIO_AudioInInitFailed "[radio] audio_in_init 失败。\n"
#define MSGTR_RADIO_BufferString "[radio] %s: 在 buffer=%d dropped=%d\n"
#define MSGTR_RADIO_AudioInSetupFailed "[radio] audio_in_setup 调用失败: %s\n"
#define MSGTR_RADIO_CaptureStarting "[radio] 开始捕获。\n"
#define MSGTR_RADIO_ClearBufferFailed "[radio] 清空缓冲失败: %s\n"
#define MSGTR_RADIO_StreamEnableCacheFailed "[radio] 调用 stream_enable_cache 失败: %s\n"
#define MSGTR_RADIO_DriverUnknownId "[radio] 未知驱动号: %d\n"
#define MSGTR_RADIO_DriverUnknownStr "[radio] 未知驱动名: %s\n"
#define MSGTR_RADIO_DriverV4L2 "[radio] 使用 V4Lv2 广播接口。\n"
#define MSGTR_RADIO_DriverV4L "[radio] 使用 V4Lv1 广播接口。\n"
#define MSGTR_RADIO_DriverBSDBT848 "[radio] 使用 *BSD BT848 广播接口。\n"

// ================================== LIBASS ====================================

// ass_bitmap.c
#define MSGTR_LIBASS_FT_Glyph_To_BitmapError "[ass] FT_Glyph_To_Bitmap 出错 %d \n"
#define MSGTR_LIBASS_UnsupportedPixelMode "[ass] 不支持的象素模式: %d\n"

// ass.c
#define MSGTR_LIBASS_NoStyleNamedXFoundUsingY "[ass] [%p] 警告: 没有找到风格(style) '%s', 将使用 '%s'\n"
#define MSGTR_LIBASS_BadTimestamp "[ass] 错误的时间戳\n"
#define MSGTR_LIBASS_BadEncodedDataSize "[ass] 错误的编码数据大小\n"
#define MSGTR_LIBASS_FontLineTooLong "[ass] 字体行太长: %d, %s\n"
#define MSGTR_LIBASS_EventFormatHeaderMissing "[ass] 未找到事件格式头\n"
#define MSGTR_LIBASS_ErrorOpeningIconvDescriptor "[ass] 打开iconv描述符出错。\n"
#define MSGTR_LIBASS_ErrorRecodingFile "[ass] 记录到文件出错。\n"
#define MSGTR_LIBASS_FopenFailed "[ass] ass_read_file(%s): 文件打开(fopen)失败\n"
#define MSGTR_LIBASS_FseekFailed "[ass] ass_read_file(%s): 文件定位(fseek)失败\n"
#define MSGTR_LIBASS_RefusingToLoadSubtitlesLargerThan10M "[ass] ass_read_file(%s): 拒绝装入大于10M的字幕\n"
#define MSGTR_LIBASS_ReadFailed "读失败, %d: %s\n"
#define MSGTR_LIBASS_AddedSubtitleFileMemory "[ass] 已加入字幕文件: <内存> (%d styles, %d events)\n"
#define MSGTR_LIBASS_AddedSubtitleFileFname "[ass] 已加入字幕文件: %s (%d styles, %d events)\n"
#define MSGTR_LIBASS_FailedToCreateDirectory "[ass] 创建目录失败 %s\n"
#define MSGTR_LIBASS_NotADirectory "[ass] 不是一个目录: %s\n"

// ass_cache.c
#define MSGTR_LIBASS_TooManyFonts "[ass] 太多字体\n"
#define MSGTR_LIBASS_ErrorOpeningFont "[ass] 打开字体出错: %s, %d\n"

// ass_fontconfig.c
#define MSGTR_LIBASS_SelectedFontFamilyIsNotTheRequestedOne "[ass] fontconfig: 选中的字体家族不是要求的: '%s' != '%s'\n"
#define MSGTR_LIBASS_UsingDefaultFontFamily "[ass] fontconfig_select: 使用缺省字体家族: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingDefaultFont "[ass] fontconfig_select: 使用缺省字体: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingArialFontFamily "[ass] fontconfig_select: 使用 'Arial' 字体家族: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_FcInitLoadConfigAndFontsFailed "[ass] FcInitLoadConfigAndFonts 失败。\n"
#define MSGTR_LIBASS_UpdatingFontCache "[ass] 更新字体缓存区。\n"
#define MSGTR_LIBASS_BetaVersionsOfFontconfigAreNotSupported "[ass] 不支持测试版的fontconfig。\n[ass] 在报告bug前请先更新。\n"
#define MSGTR_LIBASS_FcStrSetAddFailed "[ass] FcStrSetAdd 失败。\n"
#define MSGTR_LIBASS_FcDirScanFailed "[ass] FcDirScan 失败。\n"
#define MSGTR_LIBASS_FcDirSave "[ass] FcDirSave 失败。\n"
#define MSGTR_LIBASS_FcConfigAppFontAddDirFailed "[ass] FcConfigAppFontAddDir 失败\n"
#define MSGTR_LIBASS_FontconfigDisabledDefaultFontWillBeUsed "[ass] Fontconfig 已禁用, 将只使用缺省字体。\n"
#define MSGTR_LIBASS_FunctionCallFailed "[ass] %s 失败\n"

// ass_render.c
#define MSGTR_LIBASS_NeitherPlayResXNorPlayResYDefined "[ass] PlayResX 和 PlayResY 都没有定义. 假定为 384x288。\n"
#define MSGTR_LIBASS_PlayResYUndefinedSettingY "[ass] PlayResY 未定义, 设为 %d。\n"
#define MSGTR_LIBASS_PlayResXUndefinedSettingX "[ass] PlayResX 未定义, 设为 %d。\n"
#define MSGTR_LIBASS_FT_Init_FreeTypeFailed "[ass] FT_Init_FreeType 失败。\n"
#define MSGTR_LIBASS_Init "[ass] 初始化\n"
#define MSGTR_LIBASS_InitFailed "[ass] 初始化失败。\n"
#define MSGTR_LIBASS_BadCommand "[ass] 错误的命令: %c%c\n"
#define MSGTR_LIBASS_ErrorLoadingGlyph  "[ass] 装入字形出错。\n"
#define MSGTR_LIBASS_FT_Glyph_Stroke_Error "[ass] FT_Glyph_Stroke 错误 %d \n"
#define MSGTR_LIBASS_UnknownEffectType_InternalError "[ass] 未知的效果类型 (内部错误)\n"
#define MSGTR_LIBASS_NoStyleFound "[ass] 找不到风格(style)!\n"
#define MSGTR_LIBASS_EmptyEvent "[ass] 空事件!\n"
#define MSGTR_LIBASS_MAX_GLYPHS_Reached "[ass] 达到了字形最大值: 事件 %d, 开始 = %llu, 时长 = %llu\n 文本 = %s\n"
#define MSGTR_LIBASS_EventHeightHasChanged "[ass] 警告! 事件高度(height) 已改变!  \n"
#define MSGTR_LIBASS_TooManySimultaneousEvents "[ass] 过多同时的事件!\n"

// ass_font.c
#define MSGTR_LIBASS_GlyphNotFoundReselectingFont "[ass] 字形 0x%X 未找到, 重新选择字体 (%s, %d, %d)\n"
#define MSGTR_LIBASS_GlyphNotFound "[ass] 字形 0x%X 未在字体中找到 (%s, %d, %d)\n"
#define MSGTR_LIBASS_ErrorOpeningMemoryFont "[ass] 打开内存字体出错: %s\n"
