#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2001 Arpad Gereoffy (see DOCS/AUTHORS)\n"
"\n";

static char help_text[]=
"U¿ycie:   mplayer [opcje] [¶cie¿ka/]nazwa\n"
"\n"
"Opcje:\n"
" -vo <drv[:dev]> wybór sterownika[:urz±dzenia] video (lista po '-vo help')\n"
" -ao <drv[:dev]> wybór sterownika[:urz±dzenia] audio (lista po '-ao help')\n"
" -vcd <trackno>  odtwarzanie bezpo¶rednio ¶cie¿ki VCD (video cd)\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  urz±dzenie DVD do autentykacji (dla zaszyfrowanych dysków)\n"
#endif
" -ss <timepos>   skok do podanej pozycji (sekundy albo hh:mm:ss)\n"
" -nosound        odtwarzanie bez d¼wiêku\n"
#ifdef USE_FAKE_MONO
" -stereo         wybór trybu stereo dla MPEG1 (0:stereo 1:lewo 2:prawo)\n"
#endif
" -fs -vm -zoom   opcje pe³noekranowe (pe³en ekran,zmiana trybu,skalowanie)\n"
" -x <x> -y <y>   skalowanie do rozdzielczo¶ci <x>*<y> [je¶li -vo pozwala!]\n"
" -sub <file>     wybór pliku z napisami (zobacz tak¿e -subfps, -subdelay)\n"
" -vid x -aid y   wybór odtwarzanego strumienia video (x) i audio (y)\n"
" -fps x -srate y wybór prêdko¶ci odtwarzania video (x fps) i audio (y Hz)\n"
" -pp <quality>   wybór filtra wyg³adzaj±cego (0-4 dla DivX, 0-63 dla mpeg)\n"
" -bps            inna metoda synchronizacji A-V dla plików AVI (mo¿e pomóc!)\n"
" -framedrop      gubienie klatek (dla wolnych maszyn)\n"
"\n"
"Klawisze:\n"
" Right,Up,PgUp   skok naprzód o 10 sekund, 1 minutê, 10 minut\n"
" Left,Down,PgDn  skok do ty³u o 10 sekund, 1 minutê, 10 minut\n"
" p or SPACE      zatrzymanie filmu (naci¶nij dowolny klawisz aby kontynuowaæ)\n"
" q or ESC        zatrzymanie odtwarzania i wyj¶cie z programu\n"
" + or -          regulacja opó¼nienia d¼wiêku o +/- 0.1 sekundy\n"
" o               prze³±czanie trybów OSD: pusty / belka / belka i zegar\n"
" * or /          zwiêkszenie lub zmniejszenie natê¿enia d¼wiêku\n"
"                 (naci¶nij 'm' ¿eby wybraæ master/pcm)\n"
" z or x          regulacja opó¼nienia napisów o +/- 0.1 sekundy\n"
"\n"
" * * * SPRAWD¬ DETALE, POMOCNE OPCJE I KLAWISZE W MANUALU ! * * *\n"
"\n";
#endif

// mplayer.c: 

#define MSGTR_Exiting "\nWychodzê... (%s)\n"
#define MSGTR_Exit_frames "Zadana liczba klatek odtworzona"
#define MSGTR_Exit_quit "Wyj¶cie"
#define MSGTR_Exit_eof "Koniec pliku"
#define MSGTR_Exit_error "B³±d krytyczny"
#define MSGTR_IntBySignal "\nMPlayer przerwany sygna³em %d w module: %s \n"
#define MSGTR_NoHomeDir "Nie mogê znale¼æ katalogu HOME\n"
#define MSGTR_GetpathProblem "problem z get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Stwarzam plik z konfiguracj±: %s\n"
#define MSGTR_InvalidVOdriver "Nieprawid³owa nazwa sterownika video: %s\nU¿yj '-vo help' aby dostaæ listê dostêpnych streowników video.\n"
#define MSGTR_InvalidAOdriver "Nieprawid³owa nazwa sterownika audio: %s\nU¿yj '-ao help' aby dostaæ listê dostêpnych sterowników audio.\n"
#define MSGTR_CopyCodecsConf "(skopiuj/zlinkuj etc/codecs.conf do ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Nie mogê za³adowaæ fontu: %s\n"
#define MSGTR_CantLoadSub "Nie mogê za³adowaæ napisów: %s\n"
#define MSGTR_ErrorDVDkey "B³±d w przetwarzaniu DVD KEY.\n"
#define MSGTR_CmdlineDVDkey "Linia komend DVD wymaga zapisanego klucza do descramblingu.\n"
#define MSGTR_DVDauthOk "Sekwencja autoryzacji DVD wygl±da OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: nie ma wybranego strumienia!\n"
#define MSGTR_CantOpenDumpfile "Nie mogê otworzyæ pliku dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS nie podane (lub b³êdne) w nag³ówku! U¿yj opcji -fps!\n"
#define MSGTR_NoVideoStream "Przepraszam, brak strumienia video... nie dzia³a to na razie\n"
#define MSGTR_TryForceAudioFmt "Wymuszam zastosowanie kodeka audio z rodziny %d ...\n"
#define MSGTR_CantFindAfmtFallback "Nie mogê znale¼æ kodeka audio dla wymuszonej rodziny, wracam do standardowych.\n"
#define MSGTR_CantFindAudioCodec "Nie mogê znale¼æ kodeka dla formatu audio 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Sprobuj uaktualniæ %s etc/codecs.conf\n*** Je¶li to nie pomaga, przeczytaj DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Nie moge zainicjowaæ sterownika audio! -> nosound\n"
#define MSGTR_TryForceVideoFmt "Wymuszam zastosowanie kodeka video z rodziny %d ...\n"
#define MSGTR_CantFindVfmtFallback "Nie mogê znale¼æ kodeka video dla wymuszonej rodziny, wracam do standardowych..\n"
#define MSGTR_CantFindVideoCodec "Nie mogê znale¼æ kodeka dla formatu video 0x%X !\n"
#define MSGTR_VOincompCodec "Przepraszam, wybrany sterownik video_out jest niekompatybilny z tym kodekiem.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Nie mogê zainicjowaæ kodeka video :(\n"
#define MSGTR_EncodeFileExists "Plik ju¿ istnieje: %s (nie nadpisz swojego ulubionego AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Nie mogê stworzyæ pliku do zakodowania\n"
#define MSGTR_CannotInitVO "FATAL: Nie mogê zainicjowaæ sterownika video!\n"
#define MSGTR_CannotInitAO "Nie mogê otworzyæ/zainicjowaæ urz±dzenia audio -> NOSOUND\n"
#define MSGTR_StartPlaying "Pocz±tek odtwarzania...\n"
#define MSGTR_SystemTooSlow "\n*************************************************************************"\
			    "\n*** Twój system jest zbyt wolny! Sprobuj z opcj± -framedrop lub RTFM! ***"\
			    "\n*************************************************************************\n"
//#define MSGTR_

// open.c: 
#define MSGTR_CdDevNotfound "Urz±dzenie CD-ROM '%s' nie znalezione!\n"
#define MSGTR_ErrTrackSelect "B³±d wyboru ¶cie¿ki VCD!"
#define MSGTR_ReadSTDIN "Odczytuje z stdin...\n"
#define MSGTR_UnableOpenURL "Nie mogê otworzyæ URL: %s\n"
#define MSGTR_ConnToServer "Po³±czony z serwerem: %s\n"
#define MSGTR_FileNotFound "Plik nie znaleziony: '%s'\n"

// demuxer.c:
#define MSGTR_AudioStreamRedefined "Uwaga! Nag³ówek strumienia audio %d przedefiniowany!\n"
#define MSGTR_VideoStreamRedefined "Uwaga! Nag³ówek strumienia video %d przedefiniowany!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Zbyt wiele (%d w %d bajtach) pakietów audio w buforze!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Zbyt wiele (%d w %d bajtach) pakietów video w buforze!\n"
#define MSGTR_MaybeNI "(mo¿e odtwarzasz strumieñ/plik non-interleaved lub kodek nie zadzia³a³)\n"
#define MSGTR_DetectedAVIfile "Wykryto format AVI!\n"
#define MSGTR_DetectedASFfile "Wykryto format ASF!\n"
#define MSGTR_DetectedMPEGPESfile "Wykryto format MPEG-PES!\n"
#define MSGTR_DetectedMPEGPSfile "Wykryto format MPEG-PS!\n"
#define MSGTR_DetectedMPEGESfile "Wykryto format MPEG-ES!\n"
#define MSGTR_DetectedQTMOVfile "Wykryto format QuickTime/MOV!\n"
#define MSGTR_MissingMpegVideo "Zagubiony strumien video MPEG !? skontaktuj siê z autorem, mo¿e to b³±d:(\n"
#define MSGTR_InvalidMPEGES "B³êdny strumieñ MPEG-ES ??? skontaktuj siê z autorem, mo¿e to b³±d:(\n"
#define MSGTR_FormatNotRecognized "========== Przepraszam,  format pliku nierozpoznany/nieobs³ugiwany ==========\n"\
				  "=== Je¶li to strumieñ AVI, ASF lub MPEG, proszê skontaktuj siê z autorem! ===\n"
#define MSGTR_MissingASFvideo "ASF: nie znaleziono strumienia video!\n"
#define MSGTR_MissingASFaudio "ASF: nie znaleziono strumienia audio...  ->nosound\n"
#define MSGTR_MissingMPEGaudio "MPEG: nie znaleziono strumienia audio...  ->nosound\n"

//#define MSGTR_

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "About"
#define MSGTR_FileSelect "Select file ..."
#define MSGTR_MessageBox "MessageBox"
#define MSGTR_PlayList "PlayList"
#define MSGTR_SkinBrowser "Skin Browser"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Cancel"
#define MSGTR_Add "Add"
#define MSGTR_Remove "Remove"

// --- error messages ---
#define MSGTR_NEMDB "Sorry, not enough memory for draw buffer."
#define MSGTR_NEMFMR "Sorry, not enough memory for menu rendering."
#define MSGTR_NEMFMM "Sorry, not enough memory for main window shape mask."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] error in skin config file on line %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] warning in skin config file on line %d: widget found but before \"section\" not found ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] warning in skin config file on line %d: widget found but before \"subsection\" not found (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16 bits or less depth bitmap not supported ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "file not found ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "bmp read error ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "tga read error ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "png read error ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE packed tga not supported ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "unknown file type ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "24 bit to 32 bit convert error ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "unknown message: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "not enought memory\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "too many fonts declared\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "font file not found\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "font image file not found\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "non-existent font identifier ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "unknown parameter ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] not enought memory.\n"

#endif

