![http://mpv.io/](https://raw.githubusercontent.com/mpv-player/mpv.io/master/source/images/mpv-logo-128.png)

## mpv

--------------


* [External links](#external-links)
* [Overview](#overview)
* [System requirements](#system-requirements)
* [Downloads](#downloads)
* [Changelog](#changelog)
* [Compilation](#compilation)
* [FFmpeg vs. Libav](#ffmpeg-vs-libav)
* [FFmpeg ABI compatibility](#ffmpeg-abi-compatibility)
* [Release cycle](#release-cycle)
* [Bug reports](#bug-reports)
* [Contributing](#contributing)
* [Relation to MPlayer and mplayer2](#relation-to-mplayer-and-mplayer2)
* [License](#license)
* [Contact](#contact)


## 외부링크


* [Wiki](https://github.com/mpv-player/mpv/wiki)
* [FAQ](https://github.com/mpv-player/mpv/wiki/FAQ)
* [Manual](http://mpv.io/manual/master/)


## 오버뷰



**mpv**는 MPlayer와 mPlayer2에 기반을 둔 미디어 플레이어입니다. 이것은 방대한 비디오, 오디오 포맷과 코덱을 지원하며
자막도 다양하게 지원합니다.

출시에 대한 정보는 [release list][releases]에서 볼 수 있습니다.

## 시스템 요구사양


- 너무 구리지만 않으면 됩니다. 리눅스, 윈도우7이나 그 이상, 또는 OSX 10.8이후 버전이면 가능합니다.
- 적당한 CPU나 하드웨어가 필요합니다. 실시간 비디오를 디코딩하여 재생하기 위해서는 말이죠,
  다만 '--hwdec'옵션이 가능해야만 합니다.
- 그래픽카드 역시 어느정도 사양을 갖추어줘야합니다. 윈도우 등에서 항상 그래픽드라이버는 최신의 것인지
확인해줘야합니다. 몇몇 경우에서 오래된 비디오(리눅스의 '--vo=xv' 같은)는 권장되거나 지원되지 않을 수 있습니다.


## 다운로드


다운로드는 여기서 가능합니다.(다만 정식버전이 아닙니다)
[mpv.io/installation](http://mpv.io/installation/).

## 변경기록


 아직 완성된 변경기록은 없습니다. 다만, [interface changelog][interface-changes]에서 인터페이스에
 수정에 대한 정보는 확인하실 수 있습니다.
 C API의 변경은  [client API changelog][api-changes]에 기록되어 있습니다.

 [release list][releases] 에 모든 중요한 수정기록들이 보관되어 있습니다.

기본키 변경에 대한 정보는 아래에서 확인 가능합니다.
[restore-old-bindings.conf][restore-old-bindings].

## 컴파일


완벽한 형태의 컴파일은 개발 파일이나 다수의 외부 라이브러리들이 필요합니다.
mpv 시스템은 [waf](https://waf.io/)를 사용하지만, 우리는 레파지토리에 해당하는 것을 따로
저장해놓지 않았습니다. `./bootstrap.py` 스크립트는 최신버전의 waf를 포함하고 있으며 이것이 빌드에 이용되고 있습니다.

`./waf configure --help`를 통하여 사용가능한 빌드옵션의 리스트를 확인하세요.
만약 몇몇 지원하는 것들이 설치되었으나, 찾는데(detect하지 못했음은 아마 사용하는데 실패한다) 어려움이 있다면
`build/config.log`파일이 그 실패이유에 대한 정보를 가지고 있을겁니다.(아마 buffer 등의 정보가 따로 저장된다는 것 같음)

###참고###
결과물이 너무 지저분해지는 것을 방지하기 위해서는(쓰잘데기 없는 것들), `--help`를 사용하면
각 옵션에 대한 한가지 옵션만을 보여줄 것입니다. 만약 옵션이 기본값으로 자동감지되어진다면, 
`--disable-***`스위치가 출력될 것이고, 옵션이 자동으로 기본값에 의해 비활성화된다면 
`--enable-***`스위치가 출력될 것입니다. 어쨌든 당신은 `--help`를 통해 두 출력문을 보게 되겠지요.
`./waf build`를 통해 당신은 소프트웨어를 빌드하게 될것인데, 그 결과물은 `build/mpv`에 저장될 것입니다.
또한 설치는 `./waf install`에 미리 저장되어지겠죠.
예시:

    ./bootstrap.py
    ./waf configure
    ./waf
    ./waf install

필수  (추가될 수 있음):

- gcc or clang
- X development headers (xlib, xrandr, xext, xscrnsaver, xinerama, libvdpau,
  libGL, GLX, EGL, xv, ...)
- Audio output development headers (libasound/ALSA, pulseaudio)
- FFmpeg libraries (libavutil libavcodec libavformat libswscale libavfilter
  and either libswresample or libavresample)
- zlib
- iconv (기본적으로 system libc에 포함되어 있음)
- libass (OSD, OSC, 문자 자막형태)
- Lua (선택 : OSC pseudo-GUI와 youtube-dl integration)
- libjpeg (선택 : 스크린샷에만 이용됨)
- uchardet (선택 :  자막 charset 감지)
- vdpau and vaapi libraries for hardware decoding on Linux (선택)

Libass 요구사항:

- gcc or clang, yasm on x86 and x86_64
- fribidi, freetype, fontconfig development headers (for libass)
- harfbuzz (선택 : 알맞은 렌더링과 문자들의 조합을 위해 필요함.
  특히, OSX에서 영어가 아닌 이외 문자나 모든 운영체제 상의 아라비아/인도 언어 스크립트를 위해서)

FFmpeg 요구사항:

- gcc or clang, yasm on x86 and x86_64
- OpenSSL or GnuTLS (FFmpeg를 보다 정확하게 컴파일링하기 위해)
- libx264/libmp3lame/libfdk-aac  (인코딩을 하기위해 : 이유는 위와 같음)
- 원본과 같은 DASH playback을 위해 FFmpeg는 --enable-libxml2로 빌드되어야합니다.(보호/보안 관련 이유)
- 리눅스 상의 완벽한 NVIDIA의 지원을 바란다면, nv-codec-headers가 반드시 설치되어있고 설정되어있게 하세요.
- Libav support는 더이상 지원하지 않습니다. (아래에 설명되어 있습니다.)


위 대부분의 라이브러리들은 일반적인 리눅스 배포버전에 잘 맞습니다. 모든 최신버전의 git master에서의 컴파일링을
돕기위해, ([mpv-build][mpv-build])를 이용하여 각각 알맞은 build wrapper를 사용하길 권장합니다.
이는 첫번째로 FFmpeg 라이브러리들과 libass를 컴파일링하며, 그 후에 이에 해당하는 플레이어들에 고정되어 컴파일링될 것입니다.


Windows binary를 빌드하고 싶다면, MSYS2와 MinGW 둘중 아무것이나 사용해도 무관하며,
혹은 Linux에서 MinGW를 사용하여 2중 컴파일하여도 상관 없습니다.
[Windows compilation][windows_compilation]에서 자세한 컴파일링 정보를 참고!


## FFmpeg vs. Libav


일반적으로 mpv는 FFmpeg의 git version과 함께 최신버전이어야 합니다. 현재 최신 FFmpeg API 변경을
진행하지 않아 mpv가 기반인 Libav 지원은 현재 불가능하게 되었습니다.


## FFmpeg ABI 호환성


mpv는 FFmpeg 버전들 사이에 링크를 지원하지 않습니다. 버전이 바뀌면 새로 빌드하셔야합니다.(ABI호환이 되는 버전일지라도)
만약 그대로 버전을 바꾸어 링크 후 진행할 경우 오작동,에러나 보안문제가 발생할 수 있습니다.

이에 해당하는 이유는 이 mpv는 변경점에 비해 너무 복잡하며 불이익한 변경을 진행하였으며 필요없는 FFMpeg API들이 존재합니다.

새로운 mpv 버전은 런타임과 컴파일타임이 FFmpeg라이브러리 버전과 다르면 실행자체를 멈추도록 했습니다.(위의 에러 방지)


## 새 버전 배포 방식


매달 새로운 git 스냅샷이 생성되며 0.X.0의 형태의 버전 넘버로 생성될 것입니다.
더이상의 수정은 없습니다.

목표는 리눅스 개발자들의 만족입니다. 리눅스 개발자들 역시 그들의 패치로서 버그와 보안문제를 해결하길 바랍니다.

최신 버전에 대한 지원과 수정만 진행합니다.

[release policy document][release-policy] 에서 더 정확한 정보를 찾아보세요.

## 버그 리포트

Github에서 지원하는 [issue tracker][issue-tracker]를 통해 우리에게 버그리포트와 현상 request를 주세요.
해당하는 템플릿의 사용법에 따르지 않으면 이슈는 무시되거나 삭제될 수 있습니다.


버그 트래커나 간단한 질문은 받을 수 있습니다. (see [Contact](#Contact) below).

## 컨트리뷰션


 [contribute.md][contribute.md]을 읽으세요.
간단한 수정은 pull request를 통하여 보내주세요. 큰 변경사항의 경우 IRC를 통해 우리에게 말해주세요.
이것이 공동작업에 훨씬 편하며 코드를 분석하는데 도움이 됩니다.


[the wiki](https://github.com/mpv-player/mpv/wiki/Stuff-to-do)
[issue tracker](https://github.com/mpv-player/mpv/issues?q=is%3Aopen+is%3Aissue+label%3A%22feature+request%22)
에서 아이디어의 제출이 가능합니다.

## MPlayer 와 mplayer2


mpv는 MPlayer의 분기점입니다. 대부분은 수정되었고, mpv는 완전 새로운 프로그램으로서 받아들어져야하며
MPlayer의 대체물이라고 더이상 보기 힘듭니다.

더 정확한 정보를 보기 위해서 여기를 확인해주세요.[FAQ entry](https://github.com/mpv-player/mpv/wiki/FAQ#How_is_mpv_related_to_MPlayer).

그래도 무슨 차이인지 궁금하다면, 아직 완성되지 않은 수많은 변경사항들을 이곳에서 확인할 수 있습니다.
[여기][mplayer-changes].

## 라이센스

GPLv2 "or later" by default, LGPLv2.1 "or later" with `--enable-lgpl`.
See [details.](https://github.com/mpv-player/mpv/blob/master/Copyright)


## 연락처


IRC나 Github에서 대부분의 컨택트를 받습니다.

 - **GitHub issue tracker**: [issue tracker][issue-tracker] (report bugs here)
 - **User IRC Channel**: `#mpv` on `irc.freenode.net`
 - **Developer IRC Channel**: `#mpv-devel` on `irc.freenode.net`

`mpv`팀을 개인적으로 문의하고 싶으시다면 `mpv-team@googlegroups.com`으로 메일해주세요. 신중하게 부탁드리겠습니다.

[releases]: https://github.com/mpv-player/mpv/releases
[mpv-build]: https://github.com/mpv-player/mpv-build
[homebrew-mpv]: https://github.com/mpv-player/homebrew-mpv
[issue-tracker]:  https://github.com/mpv-player/mpv/issues
[ffmpeg_vs_libav]: https://github.com/mpv-player/mpv/wiki/FFmpeg-versus-Libav
[release-policy]: https://github.com/mpv-player/mpv/blob/master/DOCS/release-policy.md
[windows_compilation]: https://github.com/mpv-player/mpv/blob/master/DOCS/compile-windows.md
[mplayer-changes]: https://github.com/mpv-player/mpv/blob/master/DOCS/mplayer-changes.rst
[interface-changes]: https://github.com/mpv-player/mpv/blob/master/DOCS/interface-changes.rst
[api-changes]: https://github.com/mpv-player/mpv/blob/master/DOCS/client-api-changes.rst
[restore-old-bindings]: https://github.com/mpv-player/mpv/blob/master/etc/restore-old-bindings.conf
[contribute.md]: https://github.com/mpv-player/mpv/blob/master/DOCS/contribute.md
