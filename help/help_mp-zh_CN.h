// Translated by Lu Ran <hephooey@fastmail.fm>
// Synced with help_mp-en.h 1.248

// (Translator before 2006-04-24)
// Emfox Zhou <EmfoxZhou@gmail.com>
// (Translator before 2005-10-12)
// Lu Ran <hephooey@fastmail.fm>

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"用法:   mplayer [选项] [URL|路径/]文件名\n"
"\n"
"基本选项: (完整列表参见manpage)\n"
" -vo <drv[:dev]> 选择视频输出模式和设备(用'-vo help'查看列表)\n"
" -ao <drv[:dev]> 选择音频输出模式和设备(用'-ao help'查看列表)\n"
#ifdef HAVE_VCD
" vcd://<trackno>  播放(S)VCD(Super Video CD)track(原始设备, 无需mount)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titleno>  从设备而不是普通文件上播放DVD title\n"
" -alang/-slang   选择DVD音轨/字幕的语言(使用两位的国家代码)\n"
#endif
" -ss <timepos>   寻找指定的(多少秒或hh:mm:ss)位置\n"
" -nosound        不播放声音\n"
" -fs             全屏播放(或者-vm, -zoom, 详见man手册页)\n"
" -x <x> -y <y>   设置显示的分辨率(提供给-vm或者-zoom使用)\n"
" -sub <file>     指定使用的字幕文件(参见-subfps, -subdelay)\n"
" -playlist <file> 指定使用播放列表文件\n"
" -vid x -aid y   选择用于播放的视频(x)和音频(y)流\n"
" -fps x -srate y 改变视频(x fps)和音频(y Hz)率\n"
" -pp <quality>   使用后期处理滤镜(详见man手册页)\n"
" -framedrop      使用 去帧(frame dropping) (用于慢机器)\n"
"\n"
"基本控制键: (完整的列表参见manpage, 同时也要检查一下 input.conf)\n"
" <-  or  ->      向后/向前搜索10秒\n"
" down or up      向后/向前搜索1分钟\n"
" pgdown or pgup  向后/向前搜索10分钟\n"
" < or >          跳到播放列表中的前一首/后一首\n"
" p or SPACE      暂停播放(按任意键继续)\n"
" q or ESC        停止播放并退出程序\n"
" + or -          调整音频延迟+/-0.1秒\n"
" o               循环OSD模式:  none/seekbar/seekbar+timer\n"
" * or /          增加或减少pcm音量\n"
" x or z          调整字幕延迟+/-0.1秒\n"
" r or t          上/下调整字幕位置, 参见-vf expand\n"
"\n"
" * * * 详细内容，进一步(高级)的选项和控制键参见MANPAGE！* * *\n"
"\n";
#endif

#define MSGTR_SamplesWanted "这个格式的采样需要被更好地支持. 请联系开发者.\n"

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\n正在退出..\n"
#define MSGTR_ExitingHow "\n正在退出... (%s)\n"
#define MSGTR_Exit_quit "退出"
#define MSGTR_Exit_eof "文件结束"
#define MSGTR_Exit_error "致命错误"
#define MSGTR_IntBySignal "\nMPlayer被 %s 模块中的 %d 信号中断\n"
#define MSGTR_NoHomeDir "找不到HOME目录\n"
#define MSGTR_GetpathProblem "get_path(\"config\")问题\n"
#define MSGTR_CreatingCfgFile "创建config文件: %s\n"
#define MSGTR_CopyCodecsConf "(把etc/codecs.conf(从MPlayer的源代码中)复制/链接 ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "使用内建默认的codecs.conf.\n"
#define MSGTR_CantLoadFont "无法加载字体: %s\n"
#define MSGTR_CantLoadSub "无法加载字幕: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: 致命错误: 指定的流不存在!\n"
#define MSGTR_CantOpenDumpfile "无法打开dump文件.\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS在文件头中没有指定(或者是无效数据)! 用-fps选项!\n"
#define MSGTR_TryForceAudioFmtStr "尝试指定音频解码器驱动族 %s...\n"
#define MSGTR_CantFindAudioCodec "找不到音频格式 0x%X 的解码器.\n"
#define MSGTR_RTFMCodecs "请看DOCS/zh/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "尝试指定视频解码器驱动族 %s...\n"
#define MSGTR_CantFindVideoCodec "找不到适合所选的-vo和视频格式 0x%X 的解码器!\n"
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
"阅读DOCS/zh/video.html和DOCS/zh/sound.html来寻找调整/加速的技巧。\n"\
"如果这些一个都用不上，阅读DOCS/zh/bugreports.html！\n\n"

#define MSGTR_NoGui "MPlayer没有编译GUI的支持!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI需要X11!\n"
#define MSGTR_Playing "\n播放 %s.\n"
#define MSGTR_NoSound "音频: no sound\n"
#define MSGTR_FPSforced "FPS指定为 %5.3f  (ftime: %5.3f).\n"
#define MSGTR_CompiledWithRuntimeDetection "编译了实时CPU检测.\n"
#define MSGTR_CompiledWithCPUExtensions "针对有扩展指令集x86 CPU编译:"
#define MSGTR_AvailableVideoOutputDrivers "可用的视频输出驱动:\n"
#define MSGTR_AvailableAudioOutputDrivers "可用的音频输出驱动:\n"
#define MSGTR_AvailableAudioCodecs "可用的音频解码器:\n"
#define MSGTR_AvailableVideoCodecs "可用的视频解码器:\n"
#define MSGTR_AvailableAudioFm "\n可用的(编译了的)音频解码器族/驱动:\n"
#define MSGTR_AvailableVideoFm "\n可用的(编译了的)视频解码器族/驱动:\n"
#define MSGTR_AvailableFsType "可用的全屏实现模式:\n"
#define MSGTR_UsingRTCTiming "使用Linux的硬件RTC计时(%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "视频: 无法读取属性\n"
#define MSGTR_NoStreamFound "找不到流媒体\n"
#define MSGTR_ErrorInitializingVODevice "打开/初始化所选的视频输出(-vo)设备是出错!\n"
#define MSGTR_ForcedVideoCodec "指定的视频解码器: %s\n"
#define MSGTR_ForcedAudioCodec "指定的音频解码器: %s\n"
#define MSGTR_Video_NoVideo "视频: no video\n"
#define MSGTR_NotInitializeVOPorVO "\n致命错误: 无法初始化视频插件(-vf)或视频输出(-vo)!\n"
#define MSGTR_Paused "\n  =====  暂停  =====\r"
#define MSGTR_PlaylistLoadUnable "\n无法装载播放列表 %s\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- “非法指令”导致MPlayer崩溃。\n"\
"  这可能是我们新的运行时CPU检测代码的一个bug...\n"\
"  请阅读DOCS/zh/bugreports.html\n"
#define MSGTR_Exit_SIGILL \
"- “非法指令”导致MPlayer崩溃。\n"\
"  这通常发生在你在与编译/优化MPlayer不同的CPU上使用\n"\
"  MPlayer造成的\n"\
"  检察一下!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- 过度使用CPU/FPU/RAM导致MPlayer崩溃.\n"\
"  使用--enable-debug重新编译MPlayer用“gdb”backtrace和\n"\
"  反汇编。具体细节看DOCS/zh/bugreports.html#crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer崩溃了。这不应该发生。\n"\
"  这可能是MPlayer代码中的 _或者_ 你的驱动中的 _or_ 你的gcc的\n"\
"  一个bug。如果你觉得这是MPlayer的错，请阅读DOCS/zh/bugreports.html\n"\
"  并遵循上面的步骤。我们不能也不会帮助你除非你在报告一个可能bug的时候\n"\
"  提供所需要的信息。\n"
#define MSGTR_LoadingConfig "正在导入配置文件 '%s'\n"
#define MSGTR_AddedSubtitleFile "字幕: 加入字幕文件(%d): %s\n"
#define MSGTR_RemovedSubtitleFile "字幕: 删除字幕文件(%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "打开文件[%s]写入失败!\n"
#define MSGTR_CommandLine "命令行: "
#define MSGTR_RTCDeviceNotOpenable "打开%s失败: %s (此文件应该可被用户读取.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "通过ioctl启动Linux RTC错误(rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "试图加入\"echo %lu > /proc/sys/dev/rtc/max-user-freq\"到你的系统启动脚本.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "通过ioctl启动Linux RTC错误(rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "正在使用%s计时.\n"
#define MSGTR_NoIdleAndGui "GMPLayer不能使用选项-idle.\n"
#define MSGTR_MenuInitialized "菜单已启动: %s\n"
#define MSGTR_MenuInitFailed "菜单启动失败.\n"
#define MSGTR_Getch2InitializedTwice "警告: getch2_init 被调用两次!\n"
#define MSGTR_DumpstreamFdUnavailable "无法转储这个流 - 没有可用的文件描述符.\n"
#define MSGTR_FallingBackOnPlaylist "回退到试着解析播放列表 %s...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "不能用根菜单%s打开libmenu video filter.\n"
#define MSGTR_AudioFilterChainPreinitError "音频过滤器链预启动错误!\n"
#define MSGTR_LinuxRTCReadError "Linux RTC读取错误: %s\n"
#define MSGTR_SoftsleepUnderflow "警告! Softsleep 向下溢出!\n"
#define MSGTR_DvdnavNullEvent "DVDNAV事件为空?!\n"
#define MSGTR_DvdnavHighlightEventBroken "DVDNAV事件: 高亮事件损坏\n"
#define MSGTR_DvdnavEvent "DVDNAV事件: %s\n"
#define MSGTR_DvdnavHighlightHide "DVDNAV事件: 高亮隐藏\n"
#define MSGTR_DvdnavStillFrame "######################################## DVDNAV事件: 静止帧: %d秒\n"
#define MSGTR_DvdnavNavStop "DVDNAV事件: Nav停止\n"
#define MSGTR_DvdnavNavNOP "DVDNAV事件: Nav无操作\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "DVDNAV事件: Nav SPU流改变: 物理: %d/%d/%d 逻辑: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "DVDNAV事件: Nav SPU流改变: 物理: %d 逻辑: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "DVDNAV事件: Nav音频流改变: 物理: %d 逻辑: %d\n"
#define MSGTR_DvdnavNavVTSChange "DVDNAV事件: Nav VTS改变\n"
#define MSGTR_DvdnavNavCellChange "DVDNAV事件: Nav Cell改变\n"
#define MSGTR_DvdnavNavSpuClutChange "DVDNAV事件: Nav SPU CLUT改变\n"
#define MSGTR_DvdnavNavSeekDone "DVDNAV事件: Nav搜寻完成\n"
#define MSGTR_MenuCall "菜单调用\n"

#define MSGTR_EdlOutOfMem "不能分配足够的内存来保持EDL数据.\n"
#define MSGTR_EdlRecordsNo "读入%d EDL动作.\n"
#define MSGTR_EdlQueueEmpty "没有EDL动作要处理.\n"
#define MSGTR_EdlCantOpenForWrite "不能打开EDL文件[%s]写入.\n"
#define MSGTR_EdlCantOpenForRead "不能打开[%s]读出.\n"
#define MSGTR_EdlNOsh_video "没有视频不能使用EDL, 取消中.\n"
#define MSGTR_EdlNOValidLine "无效EDL线: %s\n"
#define MSGTR_EdlBadlyFormattedLine "错误格式的EDL线[%d]. 丢弃.\n"
#define MSGTR_EdlBadLineOverlap "上一次的停止位置是[%f]; 下一次开始是"\
"[%f]. 每一项必须按时间顺序, 不能重叠. 丢弃.\n"
#define MSGTR_EdlBadLineBadStop "停止时间必须是开始时间之后.\n"

// mplayer.c OSD

#define MSGTR_OSDenabled "启用"
#define MSGTR_OSDdisabled "禁用"
#define MSGTR_OSDChannel "频道: %s"
#define MSGTR_OSDSubDelay "字幕延迟: %d 毫秒"
#define MSGTR_OSDSpeed "速度: x %6.2f"
#define MSGTR_OSDosd "OSD: %s"

// property values
#define MSGTR_Enabled "启用"
#define MSGTR_EnabledEdl "启用 (edl)"
#define MSGTR_Disabled "禁用"
#define MSGTR_HardFrameDrop "强制"
#define MSGTR_Unknown "位置"
#define MSGTR_Bottom "底部"
#define MSGTR_Center "中部"
#define MSGTR_Top "顶部"

// osd bar names
#define MSGTR_Volume "音量"
#define MSGTR_Panscan "Panscan"
#define MSGTR_Gamma "Gamma"
#define MSGTR_Brightness "亮度"
#define MSGTR_Contrast "对比度"
#define MSGTR_Saturation "饱和度"
#define MSGTR_Hue "色调"

// property state
#define MSGTR_MuteStatus "静音: %s"
#define MSGTR_AVDelayStatus "A-V 延迟: %s"
#define MSGTR_OnTopStatus "常居顶端: %s"
#define MSGTR_RootwinStatus "根窗口: %s"
#define MSGTR_BorderStatus "边框: %s"
#define MSGTR_FramedroppingStatus "掉帧: %s"
#define MSGTR_VSyncStatus "VSync: %s"
#define MSGTR_SubSelectStatus "字幕: %s"
#define MSGTR_SubPosStatus "字幕位置: %s/100"
#define MSGTR_SubAlignStatus "字幕对齐: %s"
#define MSGTR_SubDelayStatus "字幕延迟: %s"
#define MSGTR_SubVisibleStatus "显示字幕: %s"
#define MSGTR_SubForcedOnlyStatus "仅使用指定字幕: %s"

// mencoder.c:

#define MSGTR_UsingPass3ControllFile "使用pass3控制文件: %s\n"
#define MSGTR_MissingFilename "\n没有文件名!\n\n"
#define MSGTR_CannotOpenFile_Device "无法打开文件/设备\n"
#define MSGTR_CannotOpenDemuxer "无法打开demuxer\n"
#define MSGTR_NoAudioEncoderSelected "\n没有选择音频编码器(-oac)! 选择一个(参考-oac help)或者使用-nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\n没有选择视频解码器(-ovc)! 选择一个(参考-ovc help).\n"
#define MSGTR_CannotOpenOutputFile "无法打开输出文件 '%s'\n"
#define MSGTR_EncoderOpenFailed "无法打开编码器\n"
#define MSGTR_MencoderWrongFormatAVI "\n警告: 输出文件格式是 _AVI_. 请查看 -of help.\n"
#define MSGTR_MencoderWrongFormatMPG "\n警告: 输出文件格式是 _MPEG_. 请查看 -of help.\n"
#define MSGTR_MissingOutputFilename "没有指定输出文件, 请查看 -o 选项"
#define MSGTR_ForcingOutputFourcc "指定输出的fourcc为 %x [%.4s]\n"
#define MSGTR_ForcingOutputAudiofmtTag "强制输出音频格式标签(tag) 0x%x\n"
#define MSGTR_DuplicateFrames "\n已复制 %d 帧!\n"
#define MSGTR_SkipFrame "\n跳过这一帧!\n"
#define MSGTR_ResolutionDoesntMatch "\n新的视频文件和前一个的解析度或色彩空间不同.\n"
#define MSGTR_FrameCopyFileMismatch "\n所有的视频文件必须要有同样的帧率, 解析度和编解码器才能使用-ovc copy.\n"
#define MSGTR_AudioCopyFileMismatch "\n所有的音频文件必须要有同样的音频编解码器和格式才能使用-oac copy.\n"
#define MSGTR_NoAudioFileMismatch "\n无法把只有视频的文件和音频视频文件混合. 试试 -nosound.\n"
#define MSGTR_NoSpeedWithFrameCopy "警告: -speed不保证能和-oac copy一起正常工作!\n"\
"你的编码可能失败!\n"
#define MSGTR_ErrorWritingFile "%s: 写入文件错误.\n"
#define MSGTR_RecommendedVideoBitrate "%s CD推荐的视频比特率为: %d\n"
#define MSGTR_VideoStreamResult "\n视频流: %8.3f kbit/s  (%d B/s)  大小: %"PRIu64" bytes  %5.3f secs  %d frames\n"
#define MSGTR_AudioStreamResult "\n音频流: %8.3f kbit/s  (%d B/s)  大小: %"PRIu64" bytes  %5.3f secs\n"
#define MSGTR_OpenedStream "成功: 格式: %d数据: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "视频编解码器: 帧复制 (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "音频编解码器: 帧复制 (format=%x chans=%d rate=%d bits=%d B/s=%d sample-%d)\n"
#define MSGTR_CBRPCMAudioSelected "选定CBR PCM音频\n"
#define MSGTR_MP3AudioSelected "选定MP3音频\n"
#define MSGTR_CannotAllocateBytes "无法分配%d字节\n"
#define MSGTR_SettingAudioDelay "设置音频延迟为%5.3f\n"
#define MSGTR_SettingVideoDelay "设置视频延迟为%5.3fs\n"
#define MSGTR_SettingAudioInputGain "设置音频输出增益(gain)为%f\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "限制音频预设值为0.4s\n"
#define MSGTR_IncreasingAudioDensity "增加音频密度(density)为4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "强制音频预设值为0, 最大pts校验为0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR音频: %d字节/秒, %d字节/块\n"
#define MSGTR_LameVersion "LAME版本 %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "错误: 在这个预设值上指定的比特率超出合法的范围\n"\
"\n"\
"当使用这种模式时你必须给定一个在\"8\"到\"320\"之间的数值\n"\
"\n"\
"更多信息，请试着: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "错误: 你没有给定一个合法的配置或预设值选项\n"\
"\n"\
"可用的配置(profile)包括:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - ABR模式是清楚的. 要使用这个选项,\n"\
"                      简单地指定一个比特率就行了. 例如:\n"\
"                      \"preset=185\"就可以激活这个\n"\
"                      预设值并使用185作为平均比特率.\n"\
"\n"\
"    一些例子:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"更多信息，请试着: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"预设开关被设计为提供最好的品质.\n"\
"\n"\
"它们的大多数的部分已经通过严格的 double blind listening 测试来调整和检验性能\n"\
"以达到我们预期的目标.\n"\
"\n"\
"它们不断地被升级以便和最新的发展保持一致的步调,\n"\
"所以应该能给你提供当然LAME所能提供的将近最好的品质.\n"\
"\n"\
"激活这样预设值:\n"\
"\n"\
"   VBR模式(通常情况下的最高品质):\n"\
"\n"\
"     \"preset=standard\" 此项预设值显然应该是大多数人在处理大多数的音乐的时候\n"\
  "                                   所要用到的选项, 它的品质已经非常高的了.\n" \
"\n"\
"     \"preset=extreme\" 如果你有极好的听力和相当的设备, 这项预设值一般会比\n"\
"                             \"standard\"模式的品质还要提高一点.\n"\
"\n"\
"   CBR 320kbps(预设开关选项里的最高质量):\n"\
"\n"\
"     \"preset=insane\"  对于大多数人和在大多数情况下, 这个选项都显得有些过度了.\n"\
"                             但是如果你一定要有最高品质并且完全不关心文件大小,\n"\
"                             那这正是适合你的.\n"\
"\n"\
"   ABR模式(high quality per given bitrate but not as high as VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  使用这个预设值总是会在一个给定的比特率有不错的品质.\n"\
"                             当指定一个确定的比特率, 预设值将会决定这种情况下所能达\n"\
"                             到的最好效果的设置. \n"\
"                             虽然这种方法是可以的, 但它并没有VBR模式那么灵活, 同样\n"\
"                             一般也不能达到VBR在高比特率下的同等品质. \n"\
"\n"\
"以下选项在一致的配置文件的情况下也可使用:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - ABR模式是清楚的. 要使用这个选项,\n"\
"                      简单地指定一个比特率就行了. 例如:\n"\
"                      \"preset=185\"就可以激活这个\n"\
"                      预设值并使用185作为平均比特率.\n"\
"\n"\
"   \"fast\" - 给一个特定的配置文件启用新的快速VBR模式. 速度切换\n"\
"            的坏处是经常性的比特率要比一般情况下的要高, 品质也会\n"\
"            低一点点.\n"\
"      警告: 在当前版本下, 快速预设值可能有点比一般模式偏高得太多了.\n"\
"\n"\
"   \"cbr\"  - 如果你在特定比特率使用ABR模式(见上), 比如80,\n"\
"            96, 112, 128, 160, 192, 224, 256, 320, 你可\n"\
"            以使用\"cbr\"选项来强制使用CBR模式编码以代替标准\n"\
"            abr模式. ABR不提供更高的品质, 但是cbr可能会用到,\n"\
"            某些情况下比如从internet传送一个mp3的流时就会相\n"\
"            当重要了.\n"\
"\n"\
"    例如:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"ABR模式下的一些可用的别名(alias):\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit "无法设定LAME选项, 检查比特率/采样率,"\
"一些非常低的比特率(<32)需要低采样率(如 -srate 8000)."\
"如果都不行, 试试使用预设值."
#define MSGTR_ConfigfileError "配置文件错误"
#define MSGTR_ErrorParsingCommandLine "解析命令行错误"
#define MSGTR_VideoStreamRequired "视频流是必须的!\n"
#define MSGTR_ForcingInputFPS "输入帧率将被%5.2f代替\n"
#define MSGTR_RawvideoDoesNotSupportAudio "输出文件格式RAWVIDEO不支持音频 - 取消音频\n"
#define MSGTR_DemuxerDoesntSupportNosound "这个demuxer当前还不支持 -nosound.\n"
#define MSGTR_MemAllocFailed "内存分配失败\n"
#define MSGTR_NoMatchingFilter "没找到匹配的filter/ao格式!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, C编译器挂了?\n"
#define MSGTR_NoLavcAudioCodecName "音频LAVC, 没有编解码器名!\n"
#define MSGTR_LavcAudioCodecNotFound "音频LAVC, 无法找到对应的编码器 %s\n"
#define MSGTR_CouldntAllocateLavcContext "音频LAVC, 无法分配上下文!\n"
#define MSGTR_CouldntOpenCodec "无法打开编解码器 %s, br=%d\n"
#define MSGTR_CantCopyAudioFormat "音频格式0x%x和'-oac copy'不兼容, 请试试用'-oac pcm'代替'-fafmttag'来解决这个问题.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     变比特率方式\n"\
"                0: cbr (常比特率)\n"\
"                1: mt (Mark Taylor VBR 算法)\n"\
"                2: rh (Robert Hegemann VBR 算法 - 默认)\n"\
"                3: abr (平均比特率)\n"\
"                4: mtrh (Mark Taylor Robert Hegemann VBR 算法)\n"\
"\n"\
" abr           平均比特率\n"\
"\n"\
" cbr           常比特率\n"\
"               也会在后继ABR预置模式中强制使用CBR模式.\n"\
"\n"\
" br=<0-1024>   以kBit为单位设置比特率 (仅用于CBR和ABR)\n"\
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

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "重复的FourCC"
#define MSGTR_TooManyFourccs "太多的FourCCs/formats..."
#define MSGTR_ParseError "解析错误"
#define MSGTR_ParseErrorFIDNotNumber "解析错误(格式ID不是一个数字?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "解析错误(格式ID昵称(alias)不是一个数字?)"
#define MSGTR_DuplicateFID "重复的格式ID"
#define MSGTR_TooManyOut "太多输出..."
#define MSGTR_InvalidCodecName "\n编解码器(%s)名不合法!\n"
#define MSGTR_CodecLacksFourcc "\n编解码器(%s)没有FourCC/format!\n"
#define MSGTR_CodecLacksDriver "\n编解码器(%s)没有驱动!\n"
#define MSGTR_CodecNeedsDLL "\n编解码器(%s)需要一个'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\n编解码器(%s)需要一个'outfmt'!\n"
#define MSGTR_CantAllocateComment "不能为注释分配内存."
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "读入 %s: "
#define MSGTR_CantOpenFileError "无法打开 '%s': %s\n"
#define MSGTR_CantGetMemoryForLine "无法为 'line' 获取内存: %s\n"
#define MSGTR_CantReallocCodecsp "无法重新分配 '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "编解码器名 '%s' 不唯一."
#define MSGTR_CantStrdupName "不能 strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "不能 strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "不能 strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "不能 strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d 音频和 %d 视频编解码器\n"
#define MSGTR_CodecDefinitionIncorrect "编解码器没有正确定义."
#define MSGTR_OutdatedCodecsConf "这份codecs.conf太老，与当前的MPlayer不兼容!"

// divx4_vbr.c:
#define MSGTR_OutOfMemory "内存溢出"
#define MSGTR_OverridingTooLowBitrate "指定的比特率对这人剪辑(clip)来说太低了.\n"\
"对这个剪辑来说最小的比特率是 %.0f kbps. 以此替代\n"\
"用户指定的值.\n"

// fifo.c
#define MSGTR_CannotMakePipe "不能建立PIPE!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "等级 %d 里的save slot 太旧: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "选项%s不能在配置文件里使用.\n"
#define MSGTR_InvalidCmdlineOption "选项%s不能在命令选里使用.\n"
#define MSGTR_InvalidSuboption "错误: 选项'%s'没有子选项'%s'.\n"
#define MSGTR_MissingSuboptionParameter "错误: 子选项'%s'(属于选项'%s')必须要有一个参数!\n"
#define MSGTR_MissingOptionParameter "错误: 选项'%s'必须要有一个参数!\n"
#define MSGTR_OptionListHeader "\n 名字                 类型            最小       最大     全局  命令行 配置文件\n\n"
#define MSGTR_TotalOptions "\n总共: %d个选项\n"
#define MSGTR_TooDeepProfileInclusion "警告: Profile 引用太深.\n"
#define MSGTR_NoProfileDefined "没有 profile 的定义.\n"
#define MSGTR_AvailableProfiles "可用的 profile:\n"
#define MSGTR_UnknownProfile "未知的 profile '%s'.\n"
#define MSGTR_Profile "Profile %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n 名称                 类型            最小        最大\n\n"
#define MSGTR_TotalProperties "\n总计: %d 条属性\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "找不到CD-ROM设备 '%s'!\n"
#define MSGTR_ErrTrackSelect "选择VCD track出错!"
#define MSGTR_ReadSTDIN "从stdin读取...\n"
#define MSGTR_UnableOpenURL "无法打开URL: %s\n"
#define MSGTR_ConnToServer "连接到服务器: %s\n"
#define MSGTR_FileNotFound "找不到文件: '%s'\n"

#define MSGTR_SMBInitError "无法初始化libsmbclient库: %d\n"
#define MSGTR_SMBFileNotFound "无法打开局域网内的: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer没有编译SMB读取的支持.\n"

#define MSGTR_CantOpenDVD "无法打开DVD 设备: %s\n"
#define MSGTR_NoDVDSupport "MPlayer 是被编译成不带DVD支持的，退出\n"
#define MSGTR_DVDwait "读取光盘结构, 请等待...\n"
#define MSGTR_DVDnumTitles "这张DVD有 %d 个titles.\n"
#define MSGTR_DVDinvalidTitle "无效的DVD title号: %d\n"
#define MSGTR_DVDnumChapters "这个 DVD title有 %d chapters.\n"
#define MSGTR_DVDinvalidChapter "无效的DVD chapter号: %d\n"
#define MSGTR_DVDinvalidChapterRange "无效的 chapter 范围 %s\n"
#define MSGTR_DVDinvalidLastChapter "无效的 DVD 最后 chapter 数: %d\n"
#define MSGTR_DVDnumAngles "这个 DVD title有 %d 个视角.\n"
#define MSGTR_DVDinvalidAngle "无效的DVD视角号: %d\n"
#define MSGTR_DVDnoIFO "无法打开 DVD title %d 的IFO文件.\n"
#define MSGTR_DVDnoVMG "无法打开 VMG 信息!\n"
#define MSGTR_DVDnoVOBs "无法打开title的VOB(VTS_%02d_1.VOB).\n"
#define MSGTR_DVDnoMatchingAudio "没有找到匹配的 DVD 音频语言!\n"
#define MSGTR_DVDaudioChannel "选定 DVD 音频通道: %d 语言: %c%c\n"
#define MSGTR_DVDnoMatchingSubtitle "没有找到匹配的 DVD 字幕语言!\n"
#define MSGTR_DVDsubtitleChannel "选定 DVD 字幕通道: %d 语言: %c%c\n"
#define MSGTR_DVDopenOk "DVD成功打开!\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "太多的流!"
#define MSGTR_RawMuxerOnlyOneStream "Rawaudio muxer 只支持一个音频流!\n"
#define MSGTR_IgnoringVideoStream "忽略视频流!\n"
#define MSGTR_UnknownStreamType "警告! 未知的流类型: %d\n"
#define MSGTR_WarningLenIsntDivisible "警告! 长度不能被采样率整除!\n"
#define MSGTR_MuxbufMallocErr "Muxer 帧缓冲无法分配内存!\n"
#define MSGTR_MuxbufReallocErr "Muxer 帧缓冲无法重新分配内存!\n"
#define MSGTR_MuxbufSending "Muxer 帧缓冲正在发送 %d 帧到 muxer.\n"
#define MSGTR_WritingHeader "正在写帧头...\n"
#define MSGTR_WritingTrailer "正在写索引...\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "警告! 音频流头部 %d 被重新定义.\n"
#define MSGTR_VideoStreamRedefined "警告! 视频流头部 %d 被重新定义.\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: buffer中音频包太多(%d in %d bytes)!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: buffer中视频包太多(%d in %d bytes)!\n"
#define MSGTR_MaybeNI "(也许你播放了一个非交错的流/文件或者是解码失败)?\n" \
		      "对于AVI文件, 尝试用-ni选项指定非交错模式.\n"
#define MSGTR_SwitchToNi "\n检测到糟糕的交错格式的AVI - 切换到-ni模式...\n"
#define MSGTR_Detected_XXX_FileFormat "检测到%s文件格式。\n"
#define MSGTR_DetectedAudiofile "检测到音频文件!\n"
#define MSGTR_NotSystemStream "非MPEG系统的流格式... (可能是输送流?)\n"
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
#define MSGTR_CouldntDetFNo "无法决定帧数(用于绝对搜索).\n"
#define MSGTR_CantSeekRawAVI "无法在不完整的.AVI流中搜索. (需要索引, 尝试使用-idx 选项!)  \n"
#define MSGTR_CantSeekFile "无法在这个文件中搜索.  \n"

#define MSGTR_EncryptedVOB "加密的VOB文件! 阅读DOCS/zh/cd-dvd.html.\n"

#define MSGTR_MOVcomprhdr "MOV: 压缩的文件头的支持需要ZLIB!\n"
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
#define MSGTR_DemuxerInfoAlreadyPresent "Demuxer info %s 已经显示!\n"
#define MSGTR_ClipInfo "Clip info: \n"
#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 检测到30fps的NTSC内容, 改变帧速率.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 检测到24fps渐进的NTSC内容, 改变帧速率.\n"

#define MSGTR_CacheFill "\r缓冲填充: %5.2f%% (%"PRId64" 字节)   "
#define MSGTR_NoBindFound "没有找到键 '%s' 的键绑定"
#define MSGTR_FailedToOpen "打开 %s 失败\n"

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

#define MSGTR_ShMemAllocFail "无法分配共享内存.\n"
#define MSGTR_CantAllocAudioBuf "无法分配音频输出buffer.\n"

#define MSGTR_UnknownAudio "未知或缺少音频格式, 使用nosound\n"

#define MSGTR_UsingExternalPP "[PP] 使用外部的后处理插件, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] 使用解码器的后处理插件, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "所选的vo & vd不支持视频属性'%s'. \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "要求的视频解码器族 [%s] (vfm=%s) 不可用.\n在编译时开启它.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "要求的音频解码器族 [%s] (afm=%s) 不可用.\n在编译时开启它.\n"
#define MSGTR_OpeningVideoDecoder "打开视频解码器: [%s] %s\n"
#define MSGTR_SelectedVideoCodec "选定视频编解码器: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "打开音频解码器: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "选定音频编解码器: [%s] afm: %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "为 %dHz/%dch/%s -> %dHz/%dch/%s 建造音频过滤链...\n"
#define MSGTR_UninitVideoStr "关闭视频: %s  \n"
#define MSGTR_UninitAudioStr "关闭音频: %s  \n"
#define MSGTR_VDecoderInitFailed "VDecoder初始化失败 :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder初始化失败 :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder预初始化失败 :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: 为输入缓冲分配 %d 字节.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: 为输出缓冲分配 %d + %d = %d 字节.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "起动红外遥控支持...\n"
#define MSGTR_LIRCdisabled "你将无法使用你的遥控器\n"
#define MSGTR_LIRCopenfailed "红外遥控支持起动失败!\n"
#define MSGTR_LIRCcfgerr "读取LIRC配置文件 %s 失败!\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "找不到视频滤镜 '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "无法打开视频滤镜 '%s'.\n"
#define MSGTR_OpeningVideoFilter "打开视频滤镜: "
#define MSGTR_CannotFindColorspace "无法找到合适的色彩空间, 甚至靠插入'scale'也不行 :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: 解码器无法设置sh->disp_w和sh->disp_h, 尝试绕过!\n"
#define MSGTR_VoConfigRequest "VDec: vo配置要求 - %d x %d (选择色彩空间: %s)\n"
#define MSGTR_CouldNotFindColorspace "无法找到匹配的色彩空间 - 重新尝试 -vf scale...\n"
#define MSGTR_MovieAspectIsSet "电影宽高比为 %.2f:1 - 预放大到正确的电影宽高比.\n"
#define MSGTR_MovieAspectUndefined "电影宽高比未定义 - 无法使用预放大.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "你需要升级/安装二进制编解码器包.\n请访问http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Win32/DShow视频解码器初始化OK.\n"
#define MSGTR_DMOInitOK "INFO: Win32/DMO视频解码器初始化OK.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: 不能发送EWMH全屏事件!\n"
#define MSGTR_CouldNotFindXScreenSaver "xscreensaver_disable: 找不到屏幕保护的窗口.\n"
#define MSGTR_SelectedVideoMode "XF86VM: 选定视频模式 %dx%d (图像大小 %dx%d).\n"

#define MSGTR_InsertingAfVolume "[混音器] 没有硬件混音, 插入音量过滤器.\n"
#define MSGTR_NoVolume "[混音器] 没有可用的音量控制.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "关于"
#define MSGTR_FileSelect "选择文件..."
#define MSGTR_SubtitleSelect "选择字幕..."
#define MSGTR_OtherSelect "选择..."
#define MSGTR_AudioFileSelect "选择外部音频轨道..."
#define MSGTR_FontSelect "选择字体..."
// Note: If you change MSGTR_PlayList please see if it still fits MSGTR_MENU_PlayList
#define MSGTR_PlayList "播放列表"
#define MSGTR_Equalizer "均衡器"
#define MSGTR_ConfigureEqualizer "配置均衡器"
#define MSGTR_SkinBrowser "Skin浏览器"
#define MSGTR_Network "网络流媒体..."
// Note: If you change MSGTR_Preferences please see if it still fits MSGTR_MENU_Preferences
#define MSGTR_Preferences "属性设置"
#define MSGTR_AudioPreferences "音频驱动配置"
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
#define MSGTR_UNKNOWNWINDOWTYPE "发现未知窗口类型 ..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] skin配置文件的 %d: %s行出错"
#define MSGTR_SKIN_WARNING1 "[skin] 警告, 在配置文件的 %d行:\n找到widget但在这之前没有找到\"section\" (%s)"
#define MSGTR_SKIN_WARNING2 "[skin] 警告, 在配置文件的 %d行:\n找到widget但在这之前没有找到 \"subsection\" (%s) "
#define MSGTR_SKIN_WARNING3 "[skin] 警告, 在配置文件的 %d行:\n这个widget不支持这个subsection(%s)"
#define MSGTR_SKIN_SkinFileNotFound "[skin] 文件( %s )没找到.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skin] 文件( %s )不可读.\n"
#define MSGTR_SKIN_BITMAP_16bit  "不支持少于16 bits色深的位图(%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "找不到文件(%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP读取错误(%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA读取错误(%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG读取错误(%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "不支持RLE格式压缩的TGA(%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "未知的文件格式(%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "24 bit到32 bit的转换发生错误(%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "未知信息: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "没有足够内存\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "声明了太多字体.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "找不到字体文件.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "找不到字体图像文件.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "不存在的字体标签( %s )\n"
#define MSGTR_SKIN_UnknownParameter "未知参数( %s )\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "找不到skin( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "选定的skin( %s )没找到, 试着使用'default'...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin配置文件( %s )读取错误.\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "关于MPlayer"
#define MSGTR_MENU_Open "打开..."
#define MSGTR_MENU_PlayFile "播放文件..."
#define MSGTR_MENU_PlayVCD "播放VCD..."
#define MSGTR_MENU_PlayDVD "播放DVD..."
#define MSGTR_MENU_PlayURL "播放URL..."
#define MSGTR_MENU_LoadSubtitle "加载字幕..."
#define MSGTR_MENU_DropSubtitle "丢弃字幕..."
#define MSGTR_MENU_LoadExternAudioFile "加载外部音频文件..."
#define MSGTR_MENU_Playing "播放控制"
#define MSGTR_MENU_Play "播放"
#define MSGTR_MENU_Pause "暂停"
#define MSGTR_MENU_Stop "停止"
#define MSGTR_MENU_NextStream "下一个"
#define MSGTR_MENU_PrevStream "上一个"
#define MSGTR_MENU_Size "大小"
#define MSGTR_MENU_HalfSize   "一半大小"
#define MSGTR_MENU_NormalSize "正常大小"
#define MSGTR_MENU_DoubleSize "双倍大小"
#define MSGTR_MENU_FullScreen "全屏"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "打开碟片..."
#define MSGTR_MENU_ShowDVDMenu "显示DVD菜单"
#define MSGTR_MENU_Titles "Titles"
#define MSGTR_MENU_Title "Title %2d"
#define MSGTR_MENU_None "(none)"
#define MSGTR_MENU_Chapters "Chapters"
#define MSGTR_MENU_Chapter "Chapter %2d"
#define MSGTR_MENU_AudioLanguages "音频语言"
#define MSGTR_MENU_SubtitleLanguages "字幕语言"
#define MSGTR_MENU_PlayList MSGTR_PlayList
#define MSGTR_MENU_SkinBrowser "Skin浏览器"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "退出..."
#define MSGTR_MENU_Mute "静音"
#define MSGTR_MENU_Original "原始的"
#define MSGTR_MENU_AspectRatio "长宽比"
#define MSGTR_MENU_AudioTrack "音频轨道"
#define MSGTR_MENU_Track "轨道 %d"
#define MSGTR_MENU_VideoTrack "视频轨道"
#define MSGTR_MENU_Subtitles "字幕"

// --- equalizer
// Note: If you change MSGTR_EQU_Audio please see if it still fits MSGTR_PREFERENCES_Audio
#define MSGTR_EQU_Audio "音频"
// Note: If you change MSGTR_EQU_Video please see if it still fits MSGTR_PREFERENCES_Video
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
#define MSGTR_PREFERENCES_SubtitleOSD "字幕和OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs和demuxer"
// Note: If you change MSGTR_PREFERENCES_Misc see if it still fits MSGTR_PREFERENCES_FRAME_Misc
#define MSGTR_PREFERENCES_Misc "其他"

#define MSGTR_PREFERENCES_None "None"
#define MSGTR_PREFERENCES_DriverDefault "默认驱动"
#define MSGTR_PREFERENCES_AvailableDrivers "可用驱动:"
#define MSGTR_PREFERENCES_DoNotPlaySound "不播放声音"
#define MSGTR_PREFERENCES_NormalizeSound "声音标准化"
#define MSGTR_PREFERENCES_EnEqualizer "开启均衡器"
#define MSGTR_PREFERENCES_SoftwareMixer "开启软件混音器"
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
#define MSGTR_PREFERENCES_SUB_MPSUB "将所给字幕转换为MPlayer的字幕文件"
#define MSGTR_PREFERENCES_SUB_SRT "将所给字幕转换为基于时间的SubViewer(SRT) 格式"
#define MSGTR_PREFERENCES_SUB_Overlap "开启字幕重叠"
#define MSGTR_PREFERENCES_Font "字体:"
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
#define MSGTR_PREFERENCES_FRAME_Misc MSGTR_PREFERENCES_Misc
#define MSGTR_PREFERENCES_Audio_Device "设备:"
#define MSGTR_PREFERENCES_Audio_Mixer "混音器:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "混音通道:"
#define MSGTR_PREFERENCES_Message "请记住, 有些功能只有重新播放后才有效果."
#define MSGTR_PREFERENCES_DXR3_VENC "视频编码器:"
#define MSGTR_PREFERENCES_DXR3_LAVC "使用LAVC(FFmpeg)"
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
#define MSGTR_PREFERENCES_FontEncoding22 "Windows的西里尔/中欧语(CP1250)"
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
#define MSGTR_PREFERENCES_SaveWinPos "保存窗口位置"
#define MSGTR_PREFERENCES_CacheSize "缓存大小: "
#define MSGTR_PREFERENCES_XSCREENSAVER "停用XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "使用播放条"
#define MSGTR_PREFERENCES_AutoSync "自同步 打开/关闭"
#define MSGTR_PREFERENCES_AutoSyncValue "自同步: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM设备:"
#define MSGTR_PREFERENCES_DVDDevice "DVD设备:"
#define MSGTR_PREFERENCES_FPS "电影的FPS:"
#define MSGTR_PREFERENCES_ShowVideoWindow "在非激活状态下显示视频窗口"
#define MSGTR_PREFERENCES_ArtsBroken "新的aRts版本和GTK 1.x不兼容,"\
           "会使GMPlayer崩溃!"

#define MSGTR_ABOUT_UHU "GUI开发由UHU Linux赞助\n"
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

#define MSGTR_ConfigFileReadError "[cfg] 读配置文件错误...\n"
#define MSGTR_UnableToSaveOption "[cfg] 无法保存'%s'选项.\n"

// interface.c

#define MSGTR_DeletingSubtitles "[GUI] 删除字幕.\n"
#define MSGTR_LoadingSubtitles "[GUI] 导入字幕: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] 加入视频过滤器: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] 删除视频过滤器: %s\n"

// mw.c

#define MSGTR_NotAFile "这看起来不像是一个文件: %s !\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] 无法打开display.\n"
#define MSGTR_WS_RemoteDisplay "[ws] 远程display, 取消XMITSHM.\n"
#define MSGTR_WS_NoXshm "[ws] 抱歉, 你的系统不支持X共享内存扩展.\n"
#define MSGTR_WS_NoXshape "[ws] 抱歉, 你的系统不支持XShape扩展.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] 抱歉, 色彩深度太低了.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] 打开的窗口太多了.\n"
#define MSGTR_WS_ShmError "[ws] 共享内存扩展错误\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] 抱歉, 内存不够画写缓冲(draw buffer).\n"
#define MSGTR_WS_DpmsUnavailable "DPMS不可用?\n"
#define MSGTR_WS_DpmsNotEnabled "不能启用DPMS.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "这看起来不像是一个文件...\n"
#define MSGTR_WS_DDNothing "D&D: 没有任何东西返回!\n"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "选定的视频输出设备和这个编解码器不兼容.\n"\
                "试着加入缩放过滤器, 例如以 -vf spp,scale 来代替 -vf spp.\n"
#define MSGTR_VO_GenericError "这个错误已经发生"
#define MSGTR_VO_UnableToAccess "无法访问"
#define MSGTR_VO_ExistsButNoDirectory "已经存在, 但不是一个目录."
#define MSGTR_VO_DirExistsButNotWritable "输出目录已经存在, 但是不可写."
#define MSGTR_VO_DirExistsAndIsWritable "输出目录已经存在并且可写."
#define MSGTR_VO_CantCreateDirectory "无法创建输出目录."
#define MSGTR_VO_CantCreateFile "无法创建输出文件."
#define MSGTR_VO_DirectoryCreateSuccess "输出目录成功创建."
#define MSGTR_VO_ParsingSuboptions "解析子选项."
#define MSGTR_VO_SuboptionsParsedOK "子选项解析成功."
#define MSGTR_VO_ValueOutOfRange "值超出范围"
#define MSGTR_VO_NoValueSpecified "没有指定值."
#define MSGTR_VO_UnknownSuboptions "未知子选项"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\n这里是 aalib vo_aa 的子选项:\n"
#define MSGTR_VO_AA_AdditionalOptions "vo_aa 提供的附加选项:\n" \
"  help        显示此帮助信息\n" \
"  osdcolor    设定osd颜色\n  subcolor    设定字幕颜色\n" \
"        颜色参数有:\n           0 : 一般\n" \
"           1 : 模糊\n           2 : 粗\n           3 : 粗字体\n" \
"           4 : 反色\n           5 : 特殊\n\n\n"

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "启用Progressive JPEG."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "取消Progressive JPEG."
#define MSGTR_VO_JPEG_BaselineJPEG "启用Baseline JPEG."
#define MSGTR_VO_JPEG_NoBaselineJPEG "取消Baseline JPEG."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "启用ASCII模式."
#define MSGTR_VO_PNM_RawMode "启用Raw模式."
#define MSGTR_VO_PNM_PPMType "将要写入PPM文件."
#define MSGTR_VO_PNM_PGMType "将要写入PGM文件."
#define MSGTR_VO_PNM_PGMYUVType "将要写入PGMYUV文件."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "交错模式需要图像高度能被4整除."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "无法为交错模式分配线缓冲."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "输入不是RGB, 不能按域分开色讯!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "图像宽度必须能被2整除."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "内存不够, 不能分配RGB缓冲."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "不能得到内存或文件句柄以写入\"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "写图像到输出错误!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "未知的子设备: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "使用交错输出模式(上层域在前)."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "使用交错输出模式(下层域在前)."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "使用(默认)progressive帧模式."

// sub.c
#define MSGTR_VO_SUB_Seekbar "进度条"
#define MSGTR_VO_SUB_Play "播放"
#define MSGTR_VO_SUB_Pause "暂停"
#define MSGTR_VO_SUB_Stop "停止"
#define MSGTR_VO_SUB_Rewind "后退"
#define MSGTR_VO_SUB_Forward "前进"
#define MSGTR_VO_SUB_Clock "计时"
#define MSGTR_VO_SUB_Contrast "对比度"
#define MSGTR_VO_SUB_Saturation "饱和度"
#define MSGTR_VO_SUB_Volume "音量"
#define MSGTR_VO_SUB_Brightness "亮度"
#define MSGTR_VO_SUB_Hue "色相"

// vo_xv.c
#define MSGTR_VO_XV_ImagedimTooHigh "源图像尺寸太大: %ux%u (上限是 %ux%u)\n"

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "Pgm视频输出驱动已经被 -vo pnm:pgmyuv 代替.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Md5视频输出驱动已经被 -vo md5sum 代替.\n"

// ======================= AO Audio Output drivers ========================

// libao2

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: alsa9和alsa1x模块已经被删除,请用 -ao alsa 代替.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: 无法打开混音器设备 %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: 声卡混音器没有'%s', 使用默认通道.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: 无法打开音频设备 %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: 无法建立文件描述块: %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] 无法设定音频设备 %s 到 %s 的输出, 试着使用 %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: 设定音频设备到 %d 通道失败.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: 驱动不支持 SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  你的音频驱动不支持 select()  ***\n 请用 #undef HAVE_AUDIO_SELECT in config.h 重编译MPlayer!\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\n严重错误: *** 无法重新打开或重设音频设备 *** %s\n"
#define MSGTR_AO_OSS_UnknownUnsupportedFormat "[AO OSS] 未知/不支持的 OSS 格式: %x.\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] 已连接到声音设备.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] 无法打开一个流.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] 流已经打开.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] 缓冲大小: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] 设定音量为 %d 失败.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] 不支持 %d Hz, 试试重采样.\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound 失败: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] 延迟: [server: %0.2fs, net: %0.2fs] (adjust %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] 打开 ESD 播放流失败: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] DVB 音频设置混音器错误: %s.\n"
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] 不支持 %d Hz, 试试重采样.\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] 文件: %s (%s)\nPCM: 采样率: %iHz 通道: %s 格式 %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] 信息: 用 -vc null -vo null 可以达到更快速的转储\n[AO PCM] 信息: 如果要写 WAVE 文件, 使用 -ao pcm:waveheader (默认).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] 打开 %s 写失败!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] 采样率: %iHz 通道: %s 格式 %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] 使用 %s 音频驱动.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] 不支持的音频格式: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] SDL 音频启动失败: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] 无法打开音频: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] 控制.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] 启动: 采样率: %iHz 通道: %s 格式 %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] 播放: 非法设备.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] 启动: 设定参数失败: %s\n无法设定需要的采样率.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] 启动: AL_RATE 在给定的源上不可用.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] 启动: 获取参数失败: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] 启动: 当前的采样率为 %lf (需要的速率是 %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] 启动: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] 启动: 无法打开音频通道: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO 失败.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: 写失败.\n"
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] 无法打开音频设备 %s, %s  -> nosound.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: 你的声卡不支持 %d 通道, %s, %d Hz 采样率.\\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  你的音频驱动不支持 select()  ***\n用 #undef HAVE_AUDIO_SELECT in config.h 重新编译MPlayer!\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nFatal error: *** 无法重新打开或重设音频设备 (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: 要求的格式: %d Hz, %d 通道, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: 没有发现声卡.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: 要求的格式 (%s) 非法 - 取消输出.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: 打开回放错误: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: pcm 信息错误: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: 发现 %d 声卡, 使用: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: pcm 通道信息错误: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: 设定参数错误: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: 设定通道错误: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: 通道准备错误: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: 回放 drain 错误: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: 回放 flush 错误: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: pcm 关闭错误: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: 回放 drain 错误: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: 回放 flush 错误: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: 通道准备错误: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: 回放 drain 错误: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: 回放 flush 错误: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: 通道准备错误: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: alsa 未运行, 重新启动流.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: 回放准备错误: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: 重启后写错误: %s - 放弃.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: 输出错误: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] 非法插件: %s\n"

// ======================= AF Audio Filters ================================

// libaf 

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "可用的标签"
#define MSGTR_AF_LADSPA_WarnNoInputs "警告! 这个 LADSPA 插件没有音频输入.\n 以后的音频信号将会丢失."
#define MSGTR_AF_LADSPA_ErrMultiChannel "现在还不支持多通道(>2)插件.\n 只能使用单声道或立体声道插件."
#define MSGTR_AF_LADSPA_ErrNoOutputs "这个 LADSPA 插件没有音频输出."
#define MSGTR_AF_LADSPA_ErrInOutDiff "LADSPA 插件的音频输入和音频输出的数目不相等."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "导入失败"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "在指定的库文件里找不到 ladspa_descriptor() 函数."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "在插件库里找不到标签."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "没有子选项标签"
#define MSGTR_AF_LADSPA_ErrNoLibFile "没有指定库文件"
#define MSGTR_AF_LADSPA_ErrNoLabel "没有指定过滤器标签"
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "命令行给定的控制项不够"
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: 输入控制 #%d 在下限 %0.4f 之下.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: 输入控制 #%d 在上限 %0.4f 之上.\n"

// format.c

#define MSGTR_AF_FORMAT_UnknownFormat "未知格式"

// ========================== INPUT =========================================

// joystick.c

#define MSGTR_INPUT_JOYSTICK_Opening "打开操纵杆设备 %s\n"
#define MSGTR_INPUT_JOYSTICK_CantOpen "无法打开操纵杆设备 %s: %s\n"
#define MSGTR_INPUT_JOYSTICK_ErrReading "读操纵杆设备时发生错误: %s\n"
#define MSGTR_INPUT_JOYSTICK_LoosingBytes "操纵杆: 丢失了 %d 字节的数据\n"
#define MSGTR_INPUT_JOYSTICK_WarnLostSync "操纵杆: 警告启动事件, 失去了和驱动的同步\n"
#define MSGTR_INPUT_JOYSTICK_WarnUnknownEvent "操作杆警告未知事件类型%d\n"

// input.c

#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "太多命令文件描述符了, 无法注册文件描述符 %d.\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "太多键文件描述符了, 无法注册文件描述符 %d.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "命令 %s: 参数 %d 不是一个整数.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "命令 %s: 参数 %d 不是一个浮点数.\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "命令 %s: 参数 %d 未结束.\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "未知参数 %d\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "命令 %s 需要至少 %d 个参数, 然而只发现了 %d 个.\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "当读取命令文件描述符 %d 时发生错误: %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "文件描述符 %d 的命令缓存已满: 正在丢失内容\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "绑定键 %s 的命令非法"
#define MSGTR_INPUT_INPUT_ErrSelect "选定错误: %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "键输入文件描述符 %d 发生错误\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "文件描述符 %d 得到死的键输入\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "同时有太多键按下事件发生\n"
#define MSGTR_INPUT_INPUT_ErrOnCmdFd "命令文件描述符 %d 发生错误\n"
#define MSGTR_INPUT_INPUT_ErrReadingInputConfig "当读取输入配置文件 %s 时发生错误: %s\n"
#define MSGTR_INPUT_INPUT_ErrUnknownKey "未知键 '%s'\n"
#define MSGTR_INPUT_INPUT_ErrUnfinishedBinding "未完成的绑定 %s\n"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForKeyName "这个键名的缓存太小: %s\n"
#define MSGTR_INPUT_INPUT_ErrNoCmdForKey "没有找到键 %s 的命令"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForCmd "这个命令的缓存太小: %s\n"
#define MSGTR_INPUT_INPUT_ErrWhyHere "怎么会运行到这里了?\n"
#define MSGTR_INPUT_INPUT_ErrCantInitJoystick "无法启动输入法操纵杆\n"
#define MSGTR_INPUT_INPUT_ErrCantStatFile "无法 stat %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantOpenFile "无法打开 %s: %s\n"

// ========================== LIBMPDEMUX ===================================

// url.c

#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "字符串好像已经被解开在 url_escape %c%c1%c2\n"

// ai_alsa1x.c

#define MSGTR_MPDEMUX_AIALSA1X_CannotSetSamplerate "无法设置采样率\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetBufferTime "无法设置缓冲时间\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetPeriodTime "无法设置间断时间\n"

// ai_alsa1x.c / ai_alsa.c

#define MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig "此 PCM 的配置文件损坏: 配置不可用\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableAccessType "访问类型不可用\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt "采样文件不可用\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableChanCount "通道数不可用 - 使用默认: %d\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams "无法安装硬件参数: ％s"
#define MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize "无法使用等于缓冲大小的间隔 (%u == %lu)\n"
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
#define MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt "无法设置音频格式."
#define MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate "无法设置采样率: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger "无法设置触发器: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize "无法得到块大小!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero "音频块大小是零, 设成 %d!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low "音频块大小不够, 设成 %d!\n"

// asfheader.c

#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB "FATAL: header 的大小大于 1 MB (%d)!\n请联系 MPlayer 的作者, 并且发送或上传这个文件.\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "不能为 header 分配 %d 字节的空间\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "读 asf header 的时候结束, 坏掉或不完整的文件?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "DVR 可能只能和 libavformat 一起工作, 如果有问题请试试 -demuxer 35\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "没有数据块紧随 header 之后!\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF: 没发现音频或视频 header - 损坏的文件?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "ASF header 非常长度!\n"

// asf_mmst_streaming.c

#define MSGTR_MPDEMUX_MMST_WriteError "写错误\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\n警告: 文件结束\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "Header 预读取失败\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "非法 header 大小，正在放弃\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "Header 数据读失败\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "packet_len 读失败\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "非法 rtsp 包大小，正在放弃\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "命令数据读失败\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "Header 对象\n"
#define MSGTR_MPDEMUX_MMST_DataObject "Data 对象\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "文件对象, 包长 = %d (%d)\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "流对象, 流 id: %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "Id 太多, 跳过流"
#define MSGTR_MPDEMUX_MMST_UnknownObject "未知的对象\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "媒体对象读错误\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "丢失签名\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "一切都结束了. 感谢下载一个包含专利保护的媒体文件.\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "未知命令 %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "get_media_packet 错误 : %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "已连接\n"

// asf_streaming.c

#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "啊…… stream_chunck 大小太小了: %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "size_confirm 不匹配!: %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "警告 : 掉了 header ????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "解析区块 header 的时候发生错误\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "不要把 header 当成第一个区块 !!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "不能分配 %d 字节的缓存\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "读网络流的时候发生错误\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "错误 区块太小\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "错误 子区块号非法\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "带宽太小, 文件不能播放!\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "带宽太小, 取消选定音频流\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "带宽太水, 取消选定视频流\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "非法 ASF header 的长度!\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "当读区块 header 的时候发生错误\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "Error chunk_size > packet_size\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "读区块的时候发生错误\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> ASF Redirector\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "非法的代理 URL\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "未知的 asf 流类型\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "解析 HTTP 回应失败\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "服务器返回 %d:%s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "ASF HTTP 解析警告 : Pragma %s 被从 %d 字节切到 %d\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "Socket 写错误: %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "解析 header 失败\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "没有找到流\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "未知 ASF 流类型\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL: %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "失败, 正在退出\n"

// audio_in.c

#define MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio "\n读音频错误: %s\n"
#define MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut "从交叉运行中恢复, 可能丢失了某些帧!\n"
#define MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover "致命错误, 无法恢复!\n"
#define MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples "\n音频采样不够!\n"

// aviheader.c

#define MSGTR_MPDEMUX_AVIHDR_EmptyList "**空列表?!\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundMovieAt "在 0x%X - 0x%X 找到电影\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader "找到 'bih', %u 字节的 %d\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1 "为 M$ mpg4v1 视频重新生成关键帧表\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3 "为 DIVX3 视频重新生成关键帧表\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4 "为 MPEG4 视频重新生成关键帧表\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt "找到 'wf', %d 字节的 %d\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header "AVI: 发现 dmlh (size=%d) (total_frames=%d)\n"
#define MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames  "读 INDEX 块, %d 区块的 %d 帧 (fpos=%"PRId64")\n"
#define MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr "附加的 RIFF 头...\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** 警告: 这不是扩展的 AVI 头..\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenChunk "损坏的区块?  chunksize=%d  (id=%.4s)\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI: ODML: 建造 odml 索引 (%d superindexchunks)\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI: ODML: 检测到损坏的(不完整的?)文件. 将使用传统的索引\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "无法读索引文件 %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "%s 不是有效的 MPlayer 索引文件\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "无法为来自 %s 的索引数据分配内存\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "过早结束的索引文件 %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "导入索引文件: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "正在生成索引: %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI: 为 %d 区块生成索引表!\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "无法写索引文件 %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "已保存索引文件: %s\n"

// cache2.c

#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\r这个流是不可缓冲的.\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!! read_filepos 不同!!! 请报告这个 bug...\n"

// cdda.c

#define MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice "无法打开 cdda 设备.\n"
#define MSGTR_MPDEMUX_CDDA_CantOpenDisc "无法打开盘.\n"
#define MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks "发现音频 CD，共 %ld 音轨.\n"

// cddb.c

#define MSGTR_MPDEMUX_CDDB_FailedToReadTOC "无法读 TOC.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToOpenDevice "打开 %s 设备失败.\n"
#define MSGTR_MPDEMUX_CDDB_NotAValidURL "不是合法的 URL\n"
#define MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest "发送 http 请求失败.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse "读 http 回复失败.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND "没有发现.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown "未知错误代码\n"
#define MSGTR_MPDEMUX_CDDB_NoCacheFound "没有发现缓存.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead "没有读出所有的 xmcd 文件.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory "创建目录 %s 失败.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten "没有写入所有的 xmcd 文件.\n"
#define MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned "返回了非法的 xmcd 数据库文件.\n"
#define MSGTR_MPDEMUX_CDDB_UnexpectedFIXME "意外。请修复\n"
#define MSGTR_MPDEMUX_CDDB_UnhandledCode "未处理的代码\n"
#define MSGTR_MPDEMUX_CDDB_UnableToFindEOL "无法找到行结束\n"
#define MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle "解析完成，找到: %s\n"
#define MSGTR_MPDEMUX_CDDB_AlbumNotFound "没发现专辑\n"
#define MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr "服务器返回: 命令语法错误\n"
#define MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable "没有可用的站点信息\n"
#define MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel "获得协议级别失败\n"
#define MSGTR_MPDEMUX_CDDB_NoCDInDrive "没有 CD 在驱动器里\n"

// cue_read.c

#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] 意外的 cue 文件行: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] bin 文件名测试: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] 找不到 bin 文件 - 正在放弃\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] 使用 bin 文件 %s\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] 未知的 bin 文件模式. 不应该发生. 正在停止\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] 无法打开 %s\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] 从 %s 中读取错误\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] 得到 bin 文件大小时发生错误\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "音轨 %02d:  format=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] 意外的 bin 文件结束\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] 无法读取 %d 字节的 payload\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "CUE stream_open, filename=%s, track=%d, 可用音轨: %d -> %d\n"

// network.c

#define MSGTR_MPDEMUX_NW_UnknownAF "未知地址族 %d\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "正在解析 %s (为 %s)...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "无法为 %s 解析名字: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "正在连接到服务器 %s[%s]: %d...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "连接服务器失败: %s\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "选择失败.\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "连接超时.\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "getsockopt 失败: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "连接错误: %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "无效的代理设置... 试着不用代理.\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "无法为 AF_INET 解析过程主机名. 试着不用代理.\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "发送 HTTP 请求时发生错误: 没有发出所有请求.\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "读失败.\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "http_read_response 读进 0 (比如已经结束)\n"
#define MSGTR_MPDEMUX_NW_AuthFailed "认证失败. 请使用 -user 和 -passwd 选项来指定你的\n"\
"用户名/密码, 以便提供给一组 URLs, 或者使用这样的 URL 格式:\n"\
"http://username:password@hostname/file\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "%s 需要认证\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "需要认证.\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "没有给定密码, 试着使用空密码.\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "服务器返回 %d: %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "缓存大小设为 %d K字节\n"

// demux_audio.c

#define MSGTR_MPDEMUX_AUDIO_UnknownFormat "音频分路器: 未知格式 %d.\n"

// demux_demuxers.c

#define MSGTR_MPDEMUX_DEMUXERS_FillBufferError "fill_buffer 错误: 分路器错误: 不是 vd, ad 或 sd.\n"

// demux_nuv.c

#define MSGTR_MPDEMUX_NUV_NoVideoBlocksInFile "文件中没有视频块.\n"

// demux_xmms.c

#define MSGTR_MPDEMUX_XMMS_FoundPlugin "找到插件: %s (%s).\n"
#define MSGTR_MPDEMUX_XMMS_ClosingPlugin "关闭插件: %s.\n"

// ========================== LIBMPMENU ===================================

// libmenu/menu.c
#define MSGTR_LIBMENU_SyntaxErrorAtLine "[MENU] 语法错误: %d 行\n"
#define MSGTR_LIBMENU_MenuDefinitionsNeedANameAttrib "[MENU] 菜单定义需要名称属性 (%d 行)\n"
#define MSGTR_LIBMENU_BadAttrib "[MENU] 错误属性 %s=%s，菜单 '%s' 中 %d 行\n"
#define MSGTR_LIBMENU_UnknownMenuType "[MENU] 未知菜单类型 '%s': %d行\n"
#define MSGTR_LIBMENU_CantOpenConfigFile "[MENU] 无法打开菜单配置文件: %s\n"
#define MSGTR_LIBMENU_ConfigFileIsTooBig "[MENU] 配置文件过长 (> %d KB)\n"
#define MSGTR_LIBMENU_ConfigFileIsEmpty "[MENU] 配置文件是空文件\n"
#define MSGTR_LIBMENU_MenuNotFound "[MENU] 找不到菜单 %s.\n"
#define MSGTR_LIBMENU_MenuInitFailed "[MENU] 菜单 '%s': 初始化失败.\n"
#define MSGTR_LIBMENU_UnsupportedOutformat "[MENU] 输出格式不支持!!!!\n"

// libmenu/menu_cmdlist.c
#define MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition "[MENU] 菜单定义中没有内容.\n"
#define MSGTR_LIBMENU_ListMenuEntryDefinitionsNeedAName "[MENU] 列表菜单的定义需要名称 (%d 行).\n"
#define MSGTR_LIBMENU_ListMenuNeedsAnArgument "[MENU] 列表菜单需要参数.\n"

// libmenu/menu_console.c
#define MSGTR_LIBMENU_WaitPidError "[MENU] Waitpid 错误: %s.\n"
#define MSGTR_LIBMENU_SelectError "[MENU] Select 错误.\n"
#define MSGTR_LIBMENU_ReadErrorOnChilds "[MENU] 子进程的文件描述符读取错误: %s.\n"
#define MSGTR_LIBMENU_ConsoleRun "[MENU] 终端运行: %s ...\n"
#define MSGTR_LIBMENU_AChildIsAlreadyRunning "[MENU] 子进程已经运行.\n"
#define MSGTR_LIBMENU_ForkFailed "[MENU] Fork 失败!!!\n"
#define MSGTR_LIBMENU_WriteError "[MENU] Write 错误.\n"

// libmenu/menu_filesel.c
#define MSGTR_LIBMENU_OpendirError "[MENU] Opendir 错误: %s.\n"
#define MSGTR_LIBMENU_ReallocError "[MENU] Realloc 错误: %s.\n"
#define MSGTR_LIBMENU_MallocError "[MENU] 内存分配错误: %s.\n"
#define MSGTR_LIBMENU_ReaddirError "[MENU] Readdir 错误: %s.\n"
#define MSGTR_LIBMENU_CantOpenDirectory "[MENU] 无法打开目录 %s\n"

// libmenu/menu_param.c
#define MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition "[MENU] 菜单定义中没有内容.\n"
#define MSGTR_LIBMENU_SubmenuDefinitionNeedAMenuAttribut "[MENU] 子菜单定义需要需要 'menu' 属性.\n"
#define MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed "[MENU] Pref 菜单选项的定义需要有效的 'property' 属性 (%d 行).\n"
#define MSGTR_LIBMENU_PrefMenuNeedsAnArgument "[MENU] Pref 菜单需要参数.\n"

// libmenu/menu_pt.c
#define MSGTR_LIBMENU_CantfindTheTargetItem "[MENU] 找不到目标项 ????\n"
#define MSGTR_LIBMENU_FailedToBuildCommand "[MENU] 生成命令失败: %s.\n"

// libmenu/menu_txt.c
#define MSGTR_LIBMENU_MenuTxtNeedATxtFileName "[MENU] 文本菜单需要 txt 文件名(参数文件).\n"
#define MSGTR_LIBMENU_MenuTxtCantOpen "[MENU] 无法打开: %s.\n"
#define MSGTR_LIBMENU_WarningTooLongLineSplitting "[MENU] 警告, 行过长. 分割之.\n"
#define MSGTR_LIBMENU_ParsedLines "[MENU] 分析了 %d 行.\n"

// libmenu/vf_menu.c
#define MSGTR_LIBMENU_UnknownMenuCommand "[MENU] 未知命令: '%s'.\n"
#define MSGTR_LIBMENU_FailedToOpenMenu "[MENU] 打开菜单失败: '%s'.\n"

// ========================== LIBMPCODECS ===================================

// libmpcodecs/ad_libdv.c
#define MSGTR_MPCODECS_AudioFramesizeDiffers "[AD_LIBDV] 警告! 音频帧大小不一致! read=%d  hdr=%d.\n"

// libmpcodecs/vd_dmo.c vd_dshow.c vd_vfw.c
#define MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec "[VD_DMO] 无法为 cinepak 编解码器分配图像.\n"

// libmpcodecs/vd_ffmpeg.c
#define MSGTR_MPCODECS_XVMCAcceleratedCodec "[VD_FFMPEG] XVMC 加速的编解码器.\n"
#define MSGTR_MPCODECS_ArithmeticMeanOfQP "[VD_FFMPEG] QP 的算术平均值: %2.4f, QP 的调和平均值: %2.4f\n"
#define MSGTR_MPCODECS_DRIFailure "[VD_FFMPEG] DRI失败.\n"
#define MSGTR_MPCODECS_CouldntAllocateImageForCodec "[VD_FFMPEG] 无法为编解码器分配图像.\n"
#define MSGTR_MPCODECS_XVMCAcceleratedMPEG2 "[VD_FFMPEG] XVMC 加速的 MPEG-2.\n"
#define MSGTR_MPCODECS_TryingPixfmt "[VD_FFMPEG] 尝试 pixfmt=%d.\n"
#define MSGTR_MPCODECS_McGetBufferShouldWorkOnlyWithXVMC "[VD_FFMPEG] Mc_get_buffer 只能用于 XVMC 加速!!"
#define MSGTR_MPCODECS_UnexpectedInitVoError "[VD_FFMPEG] Init_vo 意外错误.\n"
#define MSGTR_MPCODECS_UnrecoverableErrorRenderBuffersNotTaken "[VD_FFMPEG] 无法恢复的错误, 渲染缓冲无法获得.\n"
#define MSGTR_MPCODECS_OnlyBuffersAllocatedByVoXvmcAllowed "[VD_FFMPEG] 只允许 vo_xvmc 分配的缓冲.\n"

// libmpcodecs/ve_lavc.c
#define MSGTR_MPCODECS_HighQualityEncodingSelected "[VE_LAVC] 选用高质量编码 (非实时)!\n"
#define MSGTR_MPCODECS_UsingConstantQscale "[VE_LAVC] 使用常数的 qscale = %f (VBR).\n"

// libmpcodecs/ve_raw.c
#define MSGTR_MPCODECS_OutputWithFourccNotSupported "[VE_RAW] 不支持 fourcc [%x] 的 raw 输出!\n"
#define MSGTR_MPCODECS_NoVfwCodecSpecified "[VE_RAW] 未指定需要的 VfW 编解码器!!\n"

// libmpcodecs/vf_crop.c
#define MSGTR_MPCODECS_CropBadPositionWidthHeight "[CROP] 错误的位置/宽度/高度 - 切割区域在原始图像外!\n"

// libmpcodecs/vf_cropdetect.c
#define MSGTR_MPCODECS_CropArea "[CROP] 切割区域: X: %d..%d  Y: %d..%d  (-vf crop=%d:%d:%d:%d).\n"

// libmpcodecs/vf_format.c, vf_palette.c, vf_noformat.c
#define MSGTR_MPCODECS_UnknownFormatName "[VF_FORMAT] 未知格式名: '%s'.\n"

// libmpcodecs/vf_framestep.c vf_noformat.c vf_palette.c vf_tile.c
#define MSGTR_MPCODECS_ErrorParsingArgument "[VF_FRAMESTEP] 分析参数错误.\n"

// libmpcodecs/ve_vfw.c
#define MSGTR_MPCODECS_CompressorType "压缩类型: %.4lx\n"
#define MSGTR_MPCODECS_CompressorSubtype "副压缩类型: %.4lx\n"
#define MSGTR_MPCODECS_CompressorFlags "压缩标记: %lu, 版本 %lu, ICM 版本: %lu\n"
#define MSGTR_MPCODECS_Flags "标记:"
#define MSGTR_MPCODECS_Quality "质量"

// libmpcodecs/vf_expand.c
#define MSGTR_MPCODECS_FullDRNotPossible "无法完全使用 DR, 尝试使用 SLICES!\n"
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupportSlices  "警告! 下一个滤镜不支持 SLICES, 等着 sig11...\n"
#define MSGTR_MPCODECS_FunWhydowegetNULL "为什么我们得到了 NULL??\n"

// libmpcodecs/vf_fame.c
#define MSGTR_MPCODECS_FatalCantOpenlibFAME "致命错误: 无法打开 libFAME!\n"

// libmpcodecs/vf_test.c, vf_yuy2.c, vf_yvu9.c
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupport "下一个滤镜/视频输出不支持 %s :(\n"

// ================================== LIBMPVO ====================================

// mga_common.c

#define MSGTR_LIBVO_MGA_ErrorInConfigIoctl "mga_vid_config ioctl 错误 (mga_vid.o 版本错误?)"
#define MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule "无法在内核模块中获得 luma 值!\n"
#define MSGTR_LIBVO_MGA_CouldNotSetLumaValuesFromTheKernelModule "无法在内核模块中设置 luma 值!\n"
#define MSGTR_LIBVO_MGA_ScreenWidthHeightUnknown "屏幕宽度/高度未知!\n"
#define MSGTR_LIBVO_MGA_InvalidOutputFormat "mga: 非法的输出格式 %0X\n"
#define MSGTR_LIBVO_MGA_MgaInvalidOutputFormat "非法输出格式 %0X.\n"
#define MSGTR_LIBVO_MGA_IncompatibleDriverVersion "你的 mga_vid 驱动的版本与 MPlayer 的版本不兼容!\n"
#define MSGTR_LIBVO_MGA_UsingBuffers "使用 %d 缓冲.\n"
#define MSGTR_LIBVO_MGA_CouldntOpen "无法打开: %s\n"

// libvo/vesa_lvo.c

#define MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported "[VESA_LVO] 这个分支已经不再维护.\n[VESA_LVO] 请使用 -vo vesa:vidix.\n"
#define MSGTR_LIBVO_VESA_CouldntOpen "[VESA_LVO] 无法打开: '%s'\n"
#define MSGTR_LIBVO_VESA_InvalidOutputFormat "[VESA_LVI] 非法的输出格式: %s(%0X)\n"
#define MSGTR_LIBVO_VESA_IncompatibleDriverVersion "[VESA_LVO] 你的 fb_vid 驱动的版本与 MPlayer 的版本不兼容!\n"

// libvo/vo_3dfx.c

#define MSGTR_LIBVO_3DFX_Only16BppSupported "[VO_3DFX] 只支持 16bpp!"
#define MSGTR_LIBVO_3DFX_VisualIdIs "[VO_3DFX] Visual id 是  %lx.\n"
#define MSGTR_LIBVO_3DFX_UnableToOpenDevice "[VO_3DFX] 无法打开 /dev/3dfx.\n"
#define MSGTR_LIBVO_3DFX_Error "[VO_3DFX] 错误: %d.\n"
#define MSGTR_LIBVO_3DFX_CouldntMapMemoryArea "[VO_3DFX] 无法映射 3dfx 内存区域: %p,%p,%d.\n"
#define MSGTR_LIBVO_3DFX_DisplayInitialized "[VO_3DFX] 初始化: %p.\n"
#define MSGTR_LIBVO_3DFX_UnknownSubdevice "[VO_3DFX] 未知子设备: %s.\n"

// libvo/vo_dxr3.c

#define MSGTR_LIBVO_DXR3_UnableToLoadNewSPUPalette "[VO_DXR3] 无法载入新的 SPU 调色板!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetPlaymode "[VO_DXR3] 无法设置播放模式!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetSubpictureMode "[VO_DXR3] 无法设置 subpicture 模式!\n"
#define MSGTR_LIBVO_DXR3_UnableToGetTVNorm "[VO_DXR3] 无法获得电视制式!\n"
#define MSGTR_LIBVO_DXR3_AutoSelectedTVNormByFrameRate "[VO_DXR3] 利用帧速率自动选择电视制式: "
#define MSGTR_LIBVO_DXR3_UnableToSetTVNorm "[VO_DXR3] 无法设置电视制式!\n"
#define MSGTR_LIBVO_DXR3_SettingUpForNTSC "[VO_DXR3] 设置 NTSC.\n"
#define MSGTR_LIBVO_DXR3_SettingUpForPALSECAM "[VO_DXR3] 设置 PAL/SECAM.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo43 "[VO_DXR3] 长宽比设为 to 4:3.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo169 "[VO_DXR3] 长宽比设为 16:9.\n"
#define MSGTR_LIBVO_DXR3_OutOfMemory "[VO_DXR3] 内存耗尽.\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateKeycolor "[VO_DXR3] 无法分配 keycolor!\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateExactKeycolor "[VO_DXR3] 无法精确分配 keycolor, 使用最接近的匹配 (0x%lx).\n"
#define MSGTR_LIBVO_DXR3_Uninitializing "[VO_DXR3] 释放资源.\n"
#define MSGTR_LIBVO_DXR3_FailedRestoringTVNorm "[VO_DXR3] 恢复电视制式失败!\n"
#define MSGTR_LIBVO_DXR3_EnablingPrebuffering "[VO_DXR3] 启用 prebuffering.\n"
#define MSGTR_LIBVO_DXR3_UsingNewSyncEngine "[VO_DXR3] 使用新的同步引擎.\n"
#define MSGTR_LIBVO_DXR3_UsingOverlay "[VO_DXR3] 使用 overlay.\n"
#define MSGTR_LIBVO_DXR3_ErrorYouNeedToCompileMplayerWithX11 "[VO_DXR3] 错误: 你需要安装 x11 的库和头文件后编译 mplayer 来使用 overlay.\n"
#define MSGTR_LIBVO_DXR3_WillSetTVNormTo "[VO_DXR3] 将电视制式设置为: "
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALPAL60 "自动调节电影的帧速率 (PAL/PAL-60)"
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALNTSC "自动调节电影的帧速率 (PAL/NTSC)"
#define MSGTR_LIBVO_DXR3_UseCurrentNorm "使用当前制式"
#define MSGTR_LIBVO_DXR3_UseUnknownNormSuppliedCurrentNorm "未知制式，使用当前制式."
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTrying "[VO_DXR3] 打开 %s 写入错误, 尝试 /dev/em8300.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingMV "[VO_DXR3] 打开 %s 写入错误, 尝试 /dev/em8300_mv.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWell "[VO_DXR3] 打开 /dev/em8300 写入错误!\nBailing.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellMV "[VO_DXR3] 打开 /dev/em8300_mv 写入错误!\nBailing.\n"
#define MSGTR_LIBVO_DXR3_Opened "[VO_DXR3] 打开: %s.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingSP "[VO_DXR3] 打开 %s 写入错误, 尝试 /dev/em8300_sp.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellSP "[VO_DXR3] 打开 /dev/em8300_sp 写入错误!\nBailing.\n"
#define MSGTR_LIBVO_DXR3_UnableToOpenDisplayDuringHackSetup "[VO_DXR3] 在 overlay hack 设置中无法打开显示设备!\n"
#define MSGTR_LIBVO_DXR3_UnableToInitX11 "[VO_DXR3] 无法初始化 x11!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayAttribute "[VO_DXR3] 设置 overlay 属性失败.\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayScreen "[VO_DXR3] 设置 overlay screen 失败!\n退出.\n"
#define MSGTR_LIBVO_DXR3_FailedEnablingOverlay "[VO_DXR3] 启用 overlay 失败!\n退出.\n"
#define MSGTR_LIBVO_DXR3_FailedResizingOverlayWindow "[VO_DXR3] 设置 overlay 窗口大小失败!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayBcs "[VO_DXR3] 设置 overlay bcs 失败!\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayYOffsetValues "[VO_DXR3] 无法获得 overlay Y-offset 的值!\n退出.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXOffsetValues "[VO_DXR3] 无法获得 overlay X-offset 的值!\n退出.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXScaleCorrection "[VO_DXR3] 无法获得 overlay X scale correction!\n退出.\n"
#define MSGTR_LIBVO_DXR3_YOffset "[VO_DXR3] Yoffset: %d.\n"
#define MSGTR_LIBVO_DXR3_XOffset "[VO_DXR3] Xoffset: %d.\n"
#define MSGTR_LIBVO_DXR3_XCorrection "[VO_DXR3] Xcorrection: %d.\n"
#define MSGTR_LIBVO_DXR3_FailedResizingOverlayWindow "[VO_DXR3] 设置 overlay 窗口大小失败!\n"
#define MSGTR_LIBVO_DXR3_FailedSetSignalMix "[VO_DXR3] 设置 signal mix 失败!\n"

// libvo/vo_mga.c

#define MSGTR_LIBVO_MGA_AspectResized "[VO_MGA] aspect(): 改变大小为 %dx%d.\n"
#define MSGTR_LIBVO_MGA_Uninit "[VO] 释放资源!\n"

// libvo/vo_null.c

#define MSGTR_LIBVO_NULL_UnknownSubdevice "[VO_NULL] 未知子设备: %s.\n"
															
// libvo/vo_png.c

#define MSGTR_LIBVO_PNG_Warning1 "[VO_PNG] 警告: 压缩级别设置为 0, 禁用压缩!\n"
#define MSGTR_LIBVO_PNG_Warning2 "[VO_PNG] 信息: 使用 -vo png:z=<n> 设置 0 到 9 的压缩级别.\n"
#define MSGTR_LIBVO_PNG_Warning3 "[VO_PNG] 信息: (0 = 不压缩, 1 = 最快，压缩率最低 - 9 最好，最慢的压缩)\n"
#define MSGTR_LIBVO_PNG_ErrorOpeningForWriting "\n[VO_PNG] 打开 '%s' 写入错误!\n"
#define MSGTR_LIBVO_PNG_ErrorInCreatePng "[VO_PNG] create_png 错误.\n"

// libvo/vo_sdl.c

#define MSGTR_LIBVO_SDL_CouldntGetAnyAcceptableSDLModeForOutput "[VO_SDL] 无法获得可用的 SDL 输出模式.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailed "[VO_SDL] set_video_mode: SDL_SetVideoMode 失败: %s.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailedFull "[VO_SDL] Set_fullmode: SDL_SetVideoMode 失败: %s.\n"
#define MSGTR_LIBVO_SDL_MappingI420ToIYUV "[VO_SDL] I420 映射到 IYUV.\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormat "[VO_SDL] 不支持的图像格式 (0x%X).\n"
#define MSGTR_LIBVO_SDL_InfoPleaseUseVmOrZoom "[VO_SDL] 信息 - 请使用 -vm 或 -zoom 切换到最佳分辨率.\n"
#define MSGTR_LIBVO_SDL_FailedToSetVideoMode "[VO_SDL] 设置视频模式失败: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateAYUVOverlay "[VO_SDL] 无法创建 YUV overlay: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateARGBSurface "[VO_SDL] 无法创建 RGB surface: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDepthColorspaceConversion "[VO_SDL] 使用深度/颜色空间转换, 这会减慢速度 (%ibpp -> %ibpp).\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormatInDrawslice "[VO_SDL] draw_slice 不支持的图像格式, 联系 MPlayer 的开发者!\n"
#define MSGTR_LIBVO_SDL_BlitFailed "[VO_SDL] Blit 失败: %s.\n"
#define MSGTR_LIBVO_SDL_InitializingOfSDLFailed "[VO_SDL] 初始化 SDL 失败: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDriver "[VO_SDL] 使用驱动: %s.\n"

// libvo/vobsub_vidix.c

#define MSGTR_LIBVO_SUB_VIDIX_CantStartPlayback "[VO_SUB_VIDIX] 无法开始播放: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantStopPlayback "[VO_SUB_VIDIX] 无法停止播放: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_InterleavedUvForYuv410pNotSupported "[VO_SUB_VIDIX] 对 yuv410p 不支持交错的 uv.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawsliceWasCalled "[VO_SUB_VIDIX] 调用 dummy vidix_draw_slice().\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawframeWasCalled "[VO_SUB_VIDIX] 调用 dummy vidix_draw_frame().\n"
#define MSGTR_LIBVO_SUB_VIDIX_UnsupportedFourccForThisVidixDriver "[VO_SUB_VIDIX] 这个 vidix 驱动不支持 fourcc: %x (%s).\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedResolution "[VO_SUB_VIDIX] 视频服务器不支持分辨率 (%dx%d), 支持的分辨率: %dx%d-%dx%d.\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedColorDepth "[VO_SUB_VIDIX] Vidix 不支持视频服务器的色深 (%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantUpscaleImage "[VO_SUB_VIDIX] Vidix 驱动无法放大图像 (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantDownscaleImage "[VO_SUB_VIDIX] Vidix 驱动无法缩小图像 (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantConfigurePlayback "[VO_SUB_VIDIX] 无法配置播放: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_YouHaveWrongVersionOfVidixLibrary "[VO_SUB_VIDIX] VIDIX 库版本错误.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntFindWorkingVidixDriver "[VO_SUB_VIDIX] 无法找到可用的 VIDIX 驱动.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntGetCapability "[VO_SUB_VIDIX] 无法获得兼容性: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_Description "[VO_SUB_VIDIX] 描述: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_Author "[VO_SUB_VIDIX] 作者: %s.\n"

// libvo/vo_svga.c

#define MSGTR_LIBVO_SVGA_ForcedVidmodeNotAvailable "[VO_SVGA] 指定的 vid_mode %d (%s) 不可用.\n"
#define MSGTR_LIBVO_SVGA_ForcedVidmodeTooSmall "[VO_SVGA] 指定的 vid_mode %d (%s) 太小.\n"
#define MSGTR_LIBVO_SVGA_Vidmode "[VO_SVGA] Vid_mode: %d, %dx%d %dbpp.\n"
#define MSGTR_LIBVO_SVGA_VgasetmodeFailed "[VO_SVGA] Vga_setmode(%d) 失败.\n"
#define MSGTR_LIBVO_SVGA_VideoModeIsLinearAndMemcpyCouldBeUsed "[VO_SVGA] 视频模式是线性的可以使用 memcpy 操作图像.\n"
#define MSGTR_LIBVO_SVGA_VideoModeHasHardwareAcceleration "[VO_SVGA] 视频模式是硬件加速的可以使用 put_image.\n"
#define MSGTR_LIBVO_SVGA_IfItWorksForYouIWouldLineToKnow "[VO_SVGA] 如果工作正常请告诉我. \n[VO_SVGA] (发送 `mplayer test.avi -v -v -v -v &> svga.log` 生成的 log). 谢\n"
#define MSGTR_LIBVO_SVGA_VideoModeHas "[VO_SVGA] 视频模式有 %d 页.\n"
#define MSGTR_LIBVO_SVGA_CenteringImageStartAt "[VO_SVGA] 图像居中. 开始于 (%d,%d)\n"
#define MSGTR_LIBVO_SVGA_UsingVidix "[VO_SVGA] 使用 VIDIX. w=%i h=%i  mw=%i mh=%i\n"

// libvo/vo_syncfb.c

#define MSGTR_LIBVO_SYNCFB_CouldntOpen "[VO_SYNCFB] 无法打开 /dev/syncfb 或 /dev/mga_vid.\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p3 "[VO_SYNCFB] 使用 yuv420p3 调色板.\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p2 "[VO_SYNCFB] 使用 yuv420p2 调色板.\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420 "[VO_SYNCFB] 使用 yuv420 调色板.\n"
#define MSGTR_LIBVO_SYNCFB_NoSupportedPaletteFound "[VO_SYNCFB] 没有找到支持的调色板.\n"
#define MSGTR_LIBVO_SYNCFB_BesSourcerSize "[VO_SYNCFB] BES Sourcer 尺寸: %d x %d.\n"
#define MSGTR_LIBVO_SYNCFB_FramebufferMemory "[VO_SYNCFB] Framebuffer 内存: %ld in %ld buffers.\n"
#define MSGTR_LIBVO_SYNCFB_RequestingFirstBuffer "[VO_SYNCFB] 申请第一个缓冲 #%d.\n"
#define MSGTR_LIBVO_SYNCFB_GotFirstBuffer "[VO_SYNCFB] 获得第一个缓冲 #%d.\n"
#define MSGTR_LIBVO_SYNCFB_UnknownSubdevice "[VO_SYNCFB] 未知子设备: %s.\n"

// libvo/vo_tdfxfb.c

#define MSGTR_LIBVO_TDFXFB_CantOpen "[VO_TDFXFB] 无法打开 %s: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetFscreenInfo "[VO_TDFXFB] FBITGET_FSCREENINFO ioctl 出错: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetVscreenInfo "[VO_TDFXFB] FBITGET_VSCREENINFO ioctl 出错: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ThisDriverIsOnlySupports "[VO_TDFXFB] 这个驱动仅支持 3Dfx Banshee, Voodoo3 和 Voodoo 5.\n"
#define MSGTR_LIBVO_TDFXFB_OutputIsNotSupported "[VO_TDFXFB] %d bpp 输出不支持.\n"
#define MSGTR_LIBVO_TDFXFB_CouldntMapMemoryAreas "[VO_TDFXFB] 无法映射内存区域: %s.\n"
#define MSGTR_LIBVO_TDFXFB_BppOutputIsNotSupported "[VO_TDFXFB] %d bpp 输出不支持 (应该永远不会发生).\n"
#define MSGTR_LIBVO_TDFXFB_SomethingIsWrongWithControl "[VO_TDFXFB] Eik! control() 出错.\n"
#define MSGTR_LIBVO_TDFXFB_NotEnoughVideoMemoryToPlay "[VO_TDFXFB] 没有足够的显存播放这个电影. 尝试较低的分辨率.\n"
#define MSGTR_LIBVO_TDFXFB_ScreenIs "[VO_TDFXFB] 屏幕 %dx%d 色深 %d bpp, 输入 %dx%d 色深 %d bpp, 输出 %dx%d.\n"

// libvo/vo_tdfx_vid.c

#define MSGTR_LIBVO_TDFXVID_Move "[VO_TDXVID] Move %d(%d) x %d => %d.\n"
#define MSGTR_LIBVO_TDFXVID_AGPMoveFailedToClearTheScreen "[VO_TDFXVID] AGP move 清除屏幕失败.\n"
#define MSGTR_LIBVO_TDFXVID_BlitFailed "[VO_TDFXVID] Blit 失败.\n"
#define MSGTR_LIBVO_TDFXVID_NonNativeOverlayFormatNeedConversion "[VO_TDFXVID] 非本地支持的 overlay 格式需要转换.\n"
#define MSGTR_LIBVO_TDFXVID_UnsupportedInputFormat "[VO_TDFXVID] 不支持的输入格式 0x%x.\n"
#define MSGTR_LIBVO_TDFXVID_OverlaySetupFailed "[VO_TDFXVID] Overlay 设置失败.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOnFailed "[VO_TDFXVID] Overlay 打开失败.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayReady "[VO_TDFXVID] Overlay 准备完成: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_TextureBlitReady "[VO_TDFXVID] Texture blit 准备完成: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOffFailed "[VO_TDFXVID] Overlay 关闭失败\n"
#define MSGTR_LIBVO_TDFXVID_CantOpen "[VO_TDFXVID] 无法打开 %s: %s.\n"
#define MSGTR_LIBVO_TDFXVID_CantGetCurrentCfg "[VO_TDFXVID] 无法获得当前配置: %s.\n"
#define MSGTR_LIBVO_TDFXVID_MemmapFailed "[VO_TDFXVID] Memmap 失败 !!!!!\n"
#define MSGTR_LIBVO_TDFXVID_GetImageTodo "获得图像格式 todo.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailed "[VO_TDFXVID] AGP move 失败.\n"
#define MSGTR_LIBVO_TDFXVID_SetYuvFailed "[VO_TDFXVID] 设置 yuv 失败.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnYPlane "[VO_TDFXVID] AGP move 操作 Y plane 失败.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnUPlane "[VO_TDFXVID] AGP move 操作 U plane 失败.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnVPlane "[VO_TDFXVID] AGP move 操作 V plane 失败.\n"
#define MSGTR_LIBVO_TDFXVID_WhatsThatForAFormat "[VO_TDFXVID] 这是什么格式 0x%x.\n"

// libvo/vo_tga.c

#define MSGTR_LIBVO_TGA_UnknownSubdevice "[VO_TGA] 未知子设备: %s.\n"

// libvo/vo_vesa.c

#define MSGTR_LIBVO_VESA_FatalErrorOccurred "[VO_VESA] 发生致命错误! 无法恢复.\n"
#define MSGTR_LIBVO_VESA_UnkownSubdevice "[VO_VESA] 未知子设备: '%s'.\n"
#define MSGTR_LIBVO_VESA_YourHaveTooSmallSizeOfVideoMemory "[VO_VESA] 显存太小不能支持这个模式:\n[VO_VESA] 需要: %08lX 可用: %08lX.\n"
#define MSGTR_LIBVO_VESA_YouHaveToSpecifyTheCapabilitiesOfTheMonitor "[VO_VESA] 你需要设置显示器的兼容性. 不改变刷新率.\n"
#define MSGTR_LIBVO_VESA_UnableToFitTheMode "[VO_VESA] 模式超出显示器的限制. 不改变刷新率.\n"
#define MSGTR_LIBVO_VESA_DetectedInternalFatalError "[VO_VESA] 检测到内部致命错误: init 在 preinit 前被调用.\n"
#define MSGTR_LIBVO_VESA_SwitchFlipIsNotSupported "[VO_VESA] -flip 命令不支持.\n"
#define MSGTR_LIBVO_VESA_PossibleReasonNoVbe2BiosFound "[VO_VESA] 可能的原因: 找不到 VBE2 BIOS.\n"
#define MSGTR_LIBVO_VESA_FoundVesaVbeBiosVersion "[VO_VESA] 找到 VESA VBE BIOS 版本 %x.%x 修订版本: %x.\n"
#define MSGTR_LIBVO_VESA_VideoMemory "[VO_VESA] 显存: %u Kb.\n"
#define MSGTR_LIBVO_VESA_Capabilites "[VO_VESA] VESA 兼容性: %s %s %s %s %s.\n"
#define MSGTR_LIBVO_VESA_BelowWillBePrintedOemInfo "[VO_VESA] !!! 下面显示 OEM 信息. !!!\n"
#define MSGTR_LIBVO_VESA_YouShouldSee5OemRelatedLines "[VO_VESA] 你应该看到 5 行 OEM 相关内容，否则, 你的 vm86 有问题.\n"
#define MSGTR_LIBVO_VESA_OemInfo "[VO_VESA] OEM 信息: %s.\n"
#define MSGTR_LIBVO_VESA_OemRevision "[VO_VESA] OEM 版本: %x.\n"
#define MSGTR_LIBVO_VESA_OemVendor "[VO_VESA] OEM 发行商: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductName "[VO_VESA] OEM 产品名: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductRev "[VO_VESA] OEM 产品版本: %s.\n"
#define MSGTR_LIBVO_VESA_Hint "[VO_VESA] 提示: 为使用电视输出你需要在启动 PC 之前插入电视接口.\n"\
"[VO_VESA] 因为 VESA BIOS 只在自检的时候初始化自己.\n"
#define MSGTR_LIBVO_VESA_UsingVesaMode "[VO_VESA] 使用 VESA 模式 (%u) = %x [%ux%u@%u]\n"
#define MSGTR_LIBVO_VESA_CantInitializeSwscaler "[VO_VESA] 无法初始化 SwScaler.\n"
#define MSGTR_LIBVO_VESA_CantUseDga "[VO_VESA] 无法使用 DGA. 指定 bank 切换模式. :(\n"
#define MSGTR_LIBVO_VESA_UsingDga "[VO_VESA] 使用 DGA (物理资源: %08lXh, %08lXh)"
#define MSGTR_LIBVO_VESA_CantUseDoubleBuffering "[VO_VESA] 无法使用双缓冲: 显存不足.\n"
#define MSGTR_LIBVO_VESA_CantFindNeitherDga "[VO_VESA] 找不到 DGA 也不能重新分配窗口的大小.\n"
#define MSGTR_LIBVO_VESA_YouveForcedDga "[VO_VESA] 你指定了 DGA. 退出\n"
#define MSGTR_LIBVO_VESA_CantFindValidWindowAddress "[VO_VESA] 找不到可用的窗口地址.\n"
#define MSGTR_LIBVO_VESA_UsingBankSwitchingMode "[VO_VESA] 使用 bank 切换模式 (物理资源: %08lXh, %08lXh).\n"
#define MSGTR_LIBVO_VESA_CantAllocateTemporaryBuffer "[VO_VESA] 无法分配临时缓冲.\n"
#define MSGTR_LIBVO_VESA_SorryUnsupportedMode "[VO_VESA] 对不起, 模式不支持 -- 尝试 -x 640 -zoom.\n"
#define MSGTR_LIBVO_VESA_OhYouReallyHavePictureOnTv "[VO_VESA] 啊你的电视上有图像了!\n"
#define MSGTR_LIBVO_VESA_CantInitialozeLinuxVideoOverlay "[VO_VESA] 无法初始化 Linux Video Overlay.\n"
#define MSGTR_LIBVO_VESA_UsingVideoOverlay "[VO_VESA] 使用视频 overlay: %s.\n"
#define MSGTR_LIBVO_VESA_CantInitializeVidixDriver "[VO_VESA] 无法初始化 VIDIX driver.\n"
#define MSGTR_LIBVO_VESA_UsingVidix "[VO_VESA] 使用 VIDIX.\n"
#define MSGTR_LIBVO_VESA_CantFindModeFor "[VO_VESA] 找不到适合 %ux%u@%u 的模式.\n"
#define MSGTR_LIBVO_VESA_InitializationComplete "[VO_VESA] VESA 初始化完成.\n"

// libvo/vo_x11.c

#define MSGTR_LIBVO_X11_DrawFrameCalled "[VO_X11] 调用 draw_frame()!!!!!!\n"

// libvo/vo_xv.c

#define MSGTR_LIBVO_XV_DrawFrameCalled "[VO_XV] 调用 draw_frame()!!!!!!\n"
