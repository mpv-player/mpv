static char* banner_text=
"\n\n"
"MPlayer " VERSION "  (C) 2000-2001 Arpad Gereoffy <arpi@thot.banki.hu>\n"
"\n";

static char help_text[]=
"\nUsage:   mplayer [options] [path/]name\n"
"\n"
"  Options:\n"
"    -vo <driver>    select video output driver (see '-vo help' for driver list)\n"
"    -ao <driver>    select audio output driver (see '-ao help' for driver list)\n"
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
#ifdef USE_FAKE_MONO
"    -stereo         select MPEG1 stereo output (0:stereo 1:left 2:right)\n"
#endif
"    -aid <id>       select audio channel [MPG: 0-31  AVI: 1-99]\n"
"    -vid <id>       select video channel [MPG: 0-15  AVI:  -- ]\n"
"    -fps <value>    force frame rate (if value is wrong in the header)\n"
"    -mc <s/5f>      maximum sync correction per 5 frames (in seconds)\n"
//"    -afm <1-5>      force audio format  1:MPEG 2:PCM 3:AC3 4:Win32 5:aLaw\n"
"    -fs -vm -zoom   fullscreen playing options (fullsc,vidmode chg,softscale)\n"
"    -x <x> -y <y>   scale image to <x> * <y> resolution [if scalable!]\n"
"\n"
"  Keys:\n"
"    <-  or  ->      seek backward/forward  10 seconds\n"
"    up or down      seek backward/forward   1 minute\n"
"    p or SPACE      pause movie (press any key to continue)\n"
"    q or ESC        stop playing and quit program\n"
"    + or -          adjust audio delay by +/- 0.1 second\n"
"    o               toggle OSD:  none / seek / seek+timer\n"
"    * or /          increase or decrease volume\n"
"    m               use mixer master or pcm channel\n"
"    z or x          adjust subtitle delay by +/- 0.1 second\n"
"\n"
" * SEE MANPAGE FOR DETAILS/FURTHER OPTIONS AND KEYS !\n"
"\n";
