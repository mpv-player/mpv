static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2001 Arpad Gereoffy <arpi@thot.banki.hu>\n"
"\n";

static char help_text[]=
"Usage:   mplayer [options] [path/]name\n"
"\n"
"Options:\n"
" -vo <drv[:dev]> select video output driver & device (see '-vo help' for list)\n"
" -ao <drv[:dev]> select audio output driver & device (see '-ao help' for list)\n"
" -vcd <trackno>  play VCD (video cd) track from device instead of plain file\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  specify DVD device for authentication (for encrypted discs)\n"
#endif
" -ss <timepos>   seek to given (seconds or hh:mm:ss) position\n"
" -nosound        don't play sound\n"
#ifdef USE_FAKE_MONO
" -stereo         select MPEG1 stereo output (0:stereo 1:left 2:right)\n"
#endif
" -fs -vm -zoom   fullscreen playing options (fullscr,vidmode chg,softw.scale)\n"
" -x <x> -y <y>   scale image to <x> * <y> resolution [if -vo driver supports!]\n"
" -sub <file>     specify subtitle file to use (see also -subfps, -subdelay)\n"
" -vid x -aid y   options to select video (x) and audio (y) stream to play\n"
" -fps x -srate y options to change video (x fps) and audio (y Hz) rate\n"
" -pp <quality>   enable postprocessing filter (0-4 for DivX, 0-63 for mpegs)\n"
" -bps            use alternative A-V sync method for AVI files (may help!)\n"
" -framedrop      enable frame-dropping (for slow machines)\n"
"\n"
"Keys:\n"
" <-  or  ->      seek backward/forward 10 seconds\n"
" up or down      seek backward/forward  1 minute\n"
" p or SPACE      pause movie (press any key to continue)\n"
" q or ESC        stop playing and quit program\n"
" + or -          adjust audio delay by +/- 0.1 second\n"
" o               cycle OSD mode:  none / seekbar / seekbar+timer\n"
" * or /          increase or decrease volume (press 'm' to select master/pcm)\n"
" z or x          adjust subtitle delay by +/- 0.1 second\n"
"\n"
" * * * SEE MANPAGE FOR DETAILS, FURTHER OPTIONS AND KEYS ! * * *\n"
"\n";
