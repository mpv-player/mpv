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
"    -ss <timepos>   seek to second position (with timestamp)\n"
"    -nosound        don't play sound\n"
#ifdef USE_FAKE_MONO
"    -stereo         select MPEG1 stereo output (0:stereo 1:left 2:right)\n"
#endif
"    -fs -vm -zoom   fullscreen playing options (fullsc,vidmode chg,softscale)\n"
"    -sub <file>     specify subtitle file to use\n"
"    -x <x> -y <y>   scale image to <x> * <y> resolution [if scalable!]\n"
"    -aid -vid       change audio/video stream to play\n"
"    -fps -srate     change video/audio rate\n"
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
