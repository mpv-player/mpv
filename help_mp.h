static char* banner_text=
"\n"
"MPlayer " VERSION "       (C) 2000-2001 Arpad Gereoffy <arpi@esp-team.scene.hu>\n"
"\n";

static char* help_text=
"Usage:   mplayer [options] [path/]name\n"
"\n"
"  Options:\n"
"    -vo <driver>    select output driver (see '-vo help' for driver list)\n"
"    -vcd <track>    play video cd track from device instead of plain file\n"
//"    -bg             play in background (X11 only!)\n"
"    -sb <bytepos>   seek to byte position\n"
//"    -ss <timepos>   seek to second position (with timestamp)\n"
"    -nosound        don't play sound\n"
"    -abs <bytes>    audio buffer size (in bytes, default: measuring)\n"
"    -delay <secs>   audio delay in seconds (may be +/- float value)\n"
#ifdef AVI_SYNC_BPS
"    -nobps          do not use avg. byte/sec value for A-V sync (AVI)\n"
#else
"    -bps            use avg. byte/sec value for A-V sync (AVI)\n"
#endif
#ifdef ALSA_TIMER
"    -noalsa         disable timing code\n"
#else
"    -alsa           enable timing code (works better with ALSA)\n"
#endif
"    -aid <id>       select audio channel [MPG: 0-31  AVI: 1-99]\n"
"    -vid <id>       select video channel [MPG: 0-15  AVI:  -- ]\n"
"    -fps <value>    force frame rate (if value is wrong in the header)\n"
"    -mc <s/5f>      maximum sync correction per 5 frames (in seconds)\n"
"    -afm <1-5>      force audio format  1:MPEG 2:PCM 3:AC3 4:Win32 5:aLaw\n"
#ifdef X11_FULLSCREEN
"    -fs             fullscreen playing (only gl, xmga and xv drivers)\n"
#endif
#ifdef HAVE_XF86VM
"    -vm             Use XF86VidMode for psuedo-scaling with x11 driver\n                     (requires -fs)\n"
#endif
"    -x <x> -y <y>   scale image to <x> * <y> resolution [if scalable!]\n"
"\n"
"  Keys:\n"
"    <-  or  ->      seek backward/forward  10 seconds\n"
"    up or down      seek backward/forward   1 minute\n"
"    p or SPACE      pause movie (press any key to continue)\n"
"    q or ESC        stop playing and quit program\n"
"    + or -          adjust audio delay by +/- 0.1 second\n"
"\n";
