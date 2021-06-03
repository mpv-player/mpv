![mpv logo](https://raw.githubusercontent.com/mpv-player/mpv.io/master/source/images/mpv-logo-128.png)

# mpv


* [外部链接](#外部链接)
* [概述](#概述)
* [系统要求](#系统要求)
* [下载](#下载)
* [变更日志](#变更日志)
* [汇编](#汇编)
* [发布周期](#发布周期)
* [错误报告](#错误报告)
* [贡献](#贡献)
* [License](#license)
* [交流](#交流)


## 外部链接


* [Wiki](https://github.com/mpv-player/mpv/wiki)
* [FAQ][FAQ]
* [Manual](http://mpv.io/manual/master/)


## 概述


**mpv** 是一款使用命令行操作的免费（自由）媒体播放器。它支持多种媒体文件格式，如音频和视频编解码器以及字幕文件。

常见问题解答 [FAQ][FAQ]。

发布列表 [release list][releases]。

## 系统要求

- 不太古老的Linux系统，Windows 7或更高版本，OSX 10.8 或更高版本。
- 性能稍强的CPU。如果CPU速度太慢而无法实时解码视频，也许硬件解码有所帮助，但是必须使用 `--hwdec`以明确硬件解码被启用。
- 不太糟糕的GPU。mpv的重点不在于在嵌入式或集成 GPU上进行节能播放（比如，在默认情况下不启用硬件解码）。低功耗 GPU 可能会导致画面撕裂和卡顿等问题。主视频输出使用的是着色器进行视频渲染和缩放，而不是 GPU硬件模块。在 Windows平台，您可能需要确保图形驱动程序是最新版本。在某些情况下，古老的备用视频输出方法会有所帮助（比如，在Linux启用`--vo=xv`），但不推荐或不支持这种用法。

## 下载

对于非官方构建和第三方包，请参阅[mpv.io/installation](http://mpv.io/installation/).

## 变更日志

没有完整的变更日志；但是，对播放器核心接口的变更在[interface changelog][interface-changes]。

C API 的更改记录在 [client API changelog][api-changes]。

[release list][releases] 版本列表中总结了每个版本的大部分重要更改。

默认键绑定的更改在[restore-old-bindings.conf][restore-old-bindings]。

## 编译

编译完整功能需要多个外部库的开发文件。以下是要求列表。

mpv构建系统使用[waf](https://waf.io/)，但我们没有将其存储在仓库中。`./bootstrap.py`脚本会下载构建系统测试过的最新版waf。

查询有关生成选项的列表，请使用 `./waf configure --help`。

注意：为避免重复的信息输出，`--help`只显示两个选项中的一个。如果默认开启自动检测选项，则显示`--disable-***`；如果默认禁用自动检测，则显示 `--enable-***`。无论那种方式，您可以使用`--enable-***` 或 `--disable-**`，而不管`--help`显示什么。

如需构建，您可以使用./waf build：编译结果将位于build/mpv。编译后可以使用./waf installmpv将mpv安装到指定路径。

例如:

    ./bootstrap.py
    ./waf configure
    ./waf
    ./waf install

基本依赖项（不完整列表）:

- gcc 或 clang
- X development headers (xlib, xrandr, xext, xscrnsaver, xinerama, libvdpau,
  libGL, GLX, EGL, xv, ...)
- 音频输出 (libasound/ALSA, pulseaudio)
- FFmpeg 库 (libavutil libavcodec libavformat libswscale libavfilter
  and either libswresample or libavresample)
- zlib
- iconv (通常由系统 libc 提供)
- libass (OSD, OSC, text subtitles)
- Lua (optional, required for the OSC pseudo-GUI and youtube-dl integration)
- libjpeg (可选，仅用于截图)
- uchardet (可选，用于字幕字符集检测)
- 用于 Linux 上硬件解码的 nvdec 和 vaapi 库（可选）

Libass 依赖项 (当构建 libass 时)：

- gcc 或 clang, x86 和 x86_64 yasm 
- fribidi, freetype, fontconfig development headers (用于 libass)
- harfbuzz（需要正确呈现组合字符，特别是在 OSX 上正确呈现非英语文本，以及在任何平台上正确呈现阿拉伯语/印度语脚本）

FFmpeg 依赖项 (当构建 FFmpeg 时)：

- gcc 或 clang, x86 和 x86_64 yasm 
- OpenSSL 或 GnuTLS（编译 FFmpeg 时必须显式启用）
- libx264/libmp3lame/libfdk-aac 如果你想使用编码（编译FFmpeg时必须显式启用）
- 对于本机DASH 播放，FFmpeg 需要使用 --enable-libxml2 构建（尽管存在安全隐患，DASH支持也存在许多缺陷）。
- AV1 解码支持需要 dav1d。
- 为了在 Linux 上获得良好的nvidia支持，请确保安装了nv-codec-headers并且可以通过 configure 找到。

上述大多数库在正常 Linux 发行版上都有合适的版本。为了便于编译最新的 git master，您可能希望使用单独可用的构建包装器([mpv-build][mpv-build])，它首先编译 FFmpeg 库和 libass，然后编译与它们静态链接的播放器。

如果要构建 Windows 二进制文件，则必须使用 MSYS2 和 MinGW，或者从 Linux 与 MinGW 交叉编译。请参阅 [Windows compilation][windows_compilation] 。

## 发布周期

每隔一个月，就会生成一个任意的git快照，并进行分配0.X.0版本号。无需进一步维护。

发布的目地是为了适配Linux发行版，Linux发行版可能会使用自己的补丁，以防止错误和安全问题。

最新版本以外的版本不受支持和维护。

有关更多信息，请参阅发布政策文档[release policy document][release-policy] 。

## 错误报告

请使用GitHub 提供的问题跟踪器 [issue tracker][issue-tracker] 向我们发送错误报告或功能请求。请按照模板的说明进行提交，否则该问题可能会被忽略或因无效而关闭。

使用bug跟踪器处理简单问题很好，但我们建议使用 IRC（请参阅[Contact](#Contact) ）。

## 贡献

请阅读[contribute.md][contribute.md]。

对于小的更改，您可以通过 GitHub 向我们发送拉取请求。对于比较大的变化，在发送拉取请求之前，请在 IRC 频道与我们交谈。这将使双方以后更容易进行代码审查。

您可以查看[the wiki](https://github.com/mpv-player/mpv/wiki/Stuff-to-do)或问题跟踪器 [issue tracker](https://github.com/mpv-player/mpv/issues?q=is%3Aopen+is%3Aissue+label%3Ameta%3Afeature-request)以获取有关您可以贡献的内容的想法。

## License

默认情况下 GPLv2或更高版本，LGPLv2.1或更高版本带有`--enable-lgpl`。
查看 [details.](https://github.com/mpv-player/mpv/blob/master/Copyright)。

## 历史

该软件基于 MPlayer 项目。在 mpv 作为项目存在之前，代码库是在 mplayer2 项目下简要开发的。有关详细信息，请参阅 [FAQ][FAQ]。

## 接触

多数交流发生在 IRC 频道和 github 问题跟踪器上。

- **GitHub 问题跟踪器**: [issue tracker][issue-tracker] (report bugs here)
- **用户 IRC 频道**: `#mpv` on `irc.freenode.net`
- **开发者 IRC 频道**: `#mpv-devel` on `irc.freenode.net`

[FAQ]: https://github.com/mpv-player/mpv/wiki/FAQ
[releases]: https://github.com/mpv-player/mpv/releases
[mpv-build]: https://github.com/mpv-player/mpv-build
[issue-tracker]:  https://github.com/mpv-player/mpv/issues
[release-policy]: https://github.com/mpv-player/mpv/blob/master/DOCS/release-policy.md
[windows_compilation]: https://github.com/mpv-player/mpv/blob/master/DOCS/compile-windows.md
[interface-changes]: https://github.com/mpv-player/mpv/blob/master/DOCS/interface-changes.rst
[api-changes]: https://github.com/mpv-player/mpv/blob/master/DOCS/client-api-changes.rst
[restore-old-bindings]: https://github.com/mpv-player/mpv/blob/master/etc/restore-old-bindings.conf
[contribute.md]: https://github.com/mpv-player/mpv/blob/master/DOCS/contribute.md
