/* WARNING! All changes made to this file will be lost! */

#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_DVD_DEVICE "/dev/sr0"
#define DEFAULT_CDROM_DEVICE "/dev/sr0"
#define HAVE_LGPL 0
#define HAVE_GPL 1
#define HAVE_LIBAF 1
#define HAVE_CPLAYER 0
#if defined(_USRDLL) || defined(_WINDLL)
#   define HAVE_LIBMPV_SHARED 1
#   define HAVE_LIBMPV_STATIC 0
#else
#   define HAVE_LIBMPV_SHARED 0
#   define HAVE_LIBMPV_STATIC 1
#endif
#define HAVE_STATIC_BUILD 0
#define HAVE_BUILD_DATE 1
#define HAVE_OPTIMIZE 1
#define HAVE_DEBUG_BUILD 0
#define HAVE_MANPAGE_BUILD 0
#define HAVE_HTML_BUILD 0
#define HAVE_PDF_BUILD 0
#define HAVE_LIBDL 1
#define HAVE_CPLUGINS 0
#define HAVE_ZSH_COMP 0
#if defined(__INTEL_COMPILER)
#   define HAVE_ASM 1
#else
#   define HAVE_ASM 0
#endif
#define HAVE_TEST 0
#define HAVE_CLANG_DATABASE 0
#define HAVE_NOEXECSTACK 0
#define HAVE_LIBM 0
#define HAVE_MINGW 1
#define HAVE_POSIX 0
#define HAVE_ANDROID 0
#define HAVE_POSIX_OR_MINGW 0
#define HAVE_UWP 0
#define HAVE_WIN32_DESKTOP 1
#define HAVE_WIN32_INTERNAL_PTHREADS 1
#define HAVE_PTHREADS 0
#define HAVE_GNUC 0
#define HAVE_STDATOMIC 1
#define HAVE_ATOMICS 1
#define HAVE_LIBRT 0
#define HAVE_ICONV 0
#define HAVE_DOS_PATHS 1
#define HAVE_POSIX_SPAWN_NATIVE 0
#define HAVE_POSIX_SPAWN_ANDROID 0
#define HAVE_POSIX_SPAWN 0
#define HAVE_WIN32_PIPES HAVE_WIN32_DESKTOP
#define HAVE_GLOB_POSIX 0
#define HAVE_GLOB_WIN32 1
#define HAVE_GLOB 1
#define HAVE_FCHMOD 1
#define HAVE_VT_H 1
#define HAVE_GBM_H 0
#define HAVE_GLIBC_THREAD_NAME 0
#define HAVE_OSX_THREAD_NAME 0
#define HAVE_BSD_THREAD_NAME 0
#define HAVE_BSD_FSTATFS 0
#define HAVE_LINUX_FSTATFS 0
#define HAVE_LIBSMBCLIENT 0
#define HAVE_LUA 0
#define HAVE_JAVASCRIPT 0
#define HAVE_LIBASS 0
#define HAVE_LIBASS_OSD 0
#define HAVE_DUMMY_OSD 1
#define HAVE_ZLIB 0
#define HAVE_ENCODING 1
#define HAVE_LIBBLURAY 0
#define HAVE_DVDREAD 0
#define HAVE_DVDNAV 0
#define HAVE_DVDREAD_COMMON 0
#define HAVE_CDDA 0
#define HAVE_UCHARDET 0
#define HAVE_RUBBERBAND 0
#define HAVE_LCMS2 0
#define HAVE_VAPOURSYNTH 0
#define HAVE_VAPOURSYNTH_LAZY 0
#define HAVE_VAPOURSYNTH_CORE 0
#define HAVE_LIBARCHIVE 0
#define HAVE_SDL2 0
#define HAVE_SDL1 0
#define HAVE_OSS_AUDIO 0
#define HAVE_RSOUND 0
#define HAVE_SNDIO 0
#define HAVE_PULSE 0
#define HAVE_JACK 0
#define HAVE_OPENAL 0
#define HAVE_OPENSLES 0
#define HAVE_ALSA 0
#define HAVE_COREAUDIO 0
#define HAVE_AUDIOUNIT 0
#define HAVE_WASAPI 1
#define HAVE_COCOA 0
#define HAVE_DRM 0
#define HAVE_DRMPRIME 0
#define HAVE_GBM 0
#define HAVE_WAYLAND_PROTOCOLS 0
#define HAVE_WAYLAND 0
#define HAVE_X11 0
#define HAVE_XV 0
#define HAVE_GL_COCOA 0
#define HAVE_GL_X11 0
#define HAVE_EGL_X11 0
#define HAVE_EGL_DRM 0
#define HAVE_GL_WAYLAND 0
#define HAVE_GL_WIN32 1
#define HAVE_GL_DXINTEROP 1
#define HAVE_EGL_ANGLE 0
#define HAVE_EGL_ANGLE_LIB 0
#define HAVE_EGL_ANGLE_WIN32 0
#define HAVE_VDPAU 0
#define HAVE_VDPAU_GL_X11 0
#define HAVE_VAAPI 0
#define HAVE_VAAPI_X11 0
#define HAVE_VAAPI_WAYLAND 0
#define HAVE_VAAPI_DRM 0
#define HAVE_VAAPI_GLX 0
#define HAVE_VAAPI_X_EGL 0
#define HAVE_VAAPI_EGL 0
#define HAVE_CACA 0
#define HAVE_JPEG 0
#define HAVE_DIRECT3D 1
#define HAVE_SHADERC_SHARED 0
#define HAVE_SHADERC_STATIC 0
#define HAVE_SHADERC 0
#define HAVE_CROSSC 0
#define HAVE_D3D11 0
#define HAVE_RPI 0
#define HAVE_IOS_GL 0
#define HAVE_PLAIN_GL 1
#define HAVE_MALI_FBDEV 0
#define HAVE_GL 1
#define HAVE_VULKAN 0
#define HAVE_EGL_HELPERS 0
#define HAVE_LIBAVCODEC 1
#define HAVE_FFMPEG 1
#define HAVE_LIBAV 0
#define HAVE_LIBAV_ANY 1
#define HAVE_LIBAVDEVICE 1
#define HAVE_VIDEOTOOLBOX_HWACCEL 0
#define HAVE_VIDEOTOOLBOX_GL 0
#define HAVE_D3D_HWACCEL 0
#define HAVE_D3D9_HWACCEL 0
#define HAVE_GL_DXINTEROP_D3D9 0
#define HAVE_CUDA_HWACCEL 1
#define HAVE_TV 0
#define HAVE_SYS_VIDEOIO_H 0
#define HAVE_VIDEODEV 0
#define HAVE_TV_V4L2 0
#define HAVE_LIBV4L2 0
#define HAVE_AUDIO_INPUT 0
#define HAVE_DVBIN 0
#define HAVE_WIN32_EXECUTABLE 1
#define HAVE_APPLE_REMOTE 0
#define HAVE_MACOS_TOUCHBAR 0
#define CONFIGURATION "configure"
#define FULLCONFIG "alsa asm atomics build-date cplugins cuda-hwaccel drm drmprime egl-helpers egl-x11 encoding fchmod ffmpeg gl gl-x11 glibc-thread-name glob glob-posix gnuc gpl iconv libaf libarchive libass libass-osd libav-any libavcodec libavdevice libdl libm libmpv-static librt linux-fstatfs optimize oss-audio plain-gl posix posix-or-mingw posix-spawn posix-spawn-native pthreads pulse stdatomic vaapi vaapi-egl vaapi-glx vaapi-x-egl vaapi-x11 vdpau vdpau-gl-x11 vt.h x11 xv zlib"

#endif /* W_CONFIG_H_WAF */
