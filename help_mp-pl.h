#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2001 Arpad Gereoffy (see DOCS/AUTHORS)\n"
"\n";

static char help_text[]=
"Użycie:   mplayer [opcje] [ścieżka/]nazwa\n"
"\n"
"Opcje:\n"
" -vo <drv[:dev]> wybierz sterownik[:urządzenie] video (lista po '-vo help')\n"
" -ao <drv[:dev]> wybierz sterownik[:urządzenie] audio (lista po '-ao help')\n"
" -vcd <trackno>  odtwarzaj bezpośrednio ścieżkę VCD (video cd)\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  urządzenie DVD do autentykacji (dla zaszyfrowanych dysków)\n"
#endif
" -ss <timepos>   skocz do podanej pozycji (sekundy albo hh:mm:ss)\n"
" -nosound        nie odtwarzaj dźwięku\n"
#ifdef USE_FAKE_MONO
" -stereo         wybierz tryb stereo dla MPEG1 (0:stereo 1:lewo 2:prawo)\n"
#endif
" -fs -vm -zoom   opcje pełnoekranowe (pełen ekran,zmiana trybu,skalowanie)\n"
" -x <x> -y <y>   skaluj do rozdzielczości <x>*<y> [jeśli -vo driver pozwala!]\n"
" -sub <file>     podaj plik z napisami (zobacz także -subfps, -subdelay)\n"
" -vid x -aid y   wybór odtwarzanego strumienia video (x) i audio (y)\n"
" -fps x -srate y wybór prędkości odtwarzania video (x fps) i audio (y Hz)\n"
" -pp <quality>   wybór filtra wygładzającego (0-4 dla DivX, 0-63 dla mpeg)\n"
" -bps            inna metoda synchronizacji A-V dla plików AVI (może pomóc!)\n"
" -framedrop      gubienie klatek (dla wolnych maszyn)\n"
"\n"
"Klawisze:\n"
" Right,Up,PgUp   skok naprzód o 10 sekund, 1 minute, 10 minut\n"
" Left,Down,PgDn  skok do tyłu o 10 sekund, 1 minute, 10 minut\n"
" p or SPACE      zatrzymanie filmu (naciśnij dowolny klawisz aby kontynuować)\n"
" q or ESC        zatrzymanie odtwarzania i wyjście z programu\n"
" + or -          regulacja opóźnienia dźwięku o +/- 0.1 sekundy\n"
" o               przełączanie trybów OSD: pusty / belka / belka i zegar\n"
" * or /          zwiększenie lub zmniejszenie natężenia dźwięku\n"
"                 (naciśnij 'm' żeby wybrać master/pcm)\n"
" z or x          regulacja opóźnienia napisów o +/- 0.1 sekundy\n"
"\n"
" * * * SPRAWDŹ DETALE, POMOCNE OPCJE I KLAWISZE W MANUALU ! * * *\n"
"\n";
#endif

// mplayer.c: 

#define MSGTR_Exiting "\nWychodzę... (%s)\n"
#define MSGTR_Exit_frames "Zadana liczba klatek odtworzona"
#define MSGTR_Exit_quit "Wyjście"
#define MSGTR_Exit_eof "Koniec pliku"
#define MSGTR_IntBySignal "\nMPlayer przerwany sygnałem %d w module: %s \n"
#define MSGTR_NoHomeDir "Nie mogę znaleźć katalogu HOME\n"
#define MSGTR_GetpathProblem "problem z get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Stwarzam plik z konfiguracją: %s\n"
#define MSGTR_InvalidVOdriver "Nieprawidłowa nazwa sterownika video: %s\nUżyj '-vo help' aby dostać listę dostępnych streowników video.\n"
#define MSGTR_InvalidAOdriver "Nieprawidłowa nazwa sterownika audio: %s\nUżyj '-ao help' aby dostać listę dostępnych sterowników audio.\n"
#define MSGTR_CopyCodecsConf "(skopiuj/zlinkuj DOCS/codecs.conf do ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Nie mogę załadować fontu: %s\n"
#define MSGTR_CantLoadSub "Nie mogę załadować napisów: %s\n"
#define MSGTR_ErrorDVDkey "Błąd w przetwarzaniu DVD KEY.\n"
#define MSGTR_CmdlineDVDkey "Linia komend DVD wymaga zapisanego klucza do descramblingu.\n"
#define MSGTR_DVDauthOk "Sekwencja autoryzacji DVD wygląda OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: nie ma wybranego strumienia!\n"
#define MSGTR_CantOpenDumpfile "Nie mogę otworzyć pliku dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS nie podane (lub błędne) w nagłówku! Użyj opcji -fps!\n"
#define MSGTR_NoVideoStream "Przepraszam, brak strumienia video... nie działa to na razie\n"
#define MSGTR_TryForceAudioFmt "Wymuszam zastosowanie kodeka audio z rodziny %d ...\n"
#define MSGTR_CantFindAfmtFallback "Nie mogę znaleźć kodeka audio dla wymuszonej rodziny, wracam do standardowych.\n"
#define MSGTR_CantFindAudioCodec "Nie mogę znaleźć kodeka dla formatu audio 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Sprobuj uaktualnić %s DOCS/codecs.conf\n*** Jeśli to nie pomaga, przeczytaj DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Nie moge zainicjować sterownika audio! -> nosound\n"
#define MSGTR_TryForceVideoFmt "Wymuszam zastosowanie kodeka video z rodziny  %d ...\n"
#define MSGTR_CantFindVfmtFallback "Nie mogę znaleźć kodeka video dla wymuszonej rodziny, wracam do standardowych..\n"
#define MSGTR_CantFindVideoCodec "Nie mogę znaleźć kodeka dla formatu video  0x%X !\n"
#define MSGTR_VOincompCodec "Przepraszam, wybrany sterownik video_out jest niekompatybilny z tym kodekiem.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Nie mogę zainicjować kodeka video :(\n"
#define MSGTR_EncodeFileExists "Plik już istnieje: %s (nie nadpisz swojego ulubionego AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Nie mogę stworzyć pliku do zakodowania\n"
#define MSGTR_CannotInitVO "FATAL: Nie mogę zainicjować sterownika video!\n"
#define MSGTR_CannotInitAO "Nie mogę otworzyć/zainicjować urządzenia audio -> NOSOUND\n"
#define MSGTR_StartPlaying "Początek odtwarzania...\n"
#define MSGTR_SystemTooSlow "\n*************************************************************************"\
			    "\n*** Twój system jest zbyt wolny! Sprobuj z opcją -framedrop lub RTFM! ***"\
			    "\n*************************************************************************\n"
//#define MSGTR_

// open.c: 
#define MSGTR_CdDevNotfound "Urządzenie CD-ROM '%s' nie znalezione!\n"
#define MSGTR_ErrTrackSelect "Błąd wyboru ścieżki VCD!"
#define MSGTR_ReadSTDIN "Odczytuje z stdin...\n"
#define MSGTR_UnableOpenURL "Nie mogę otworzyć URL: %s\n"
#define MSGTR_ConnToServer "Połączony do serwera: %s\n"
#define MSGTR_FileNotFound "Plik nie znaleziony: '%s'\n"

// demuxer.c:
#define MSGTR_AudioStreamRedefined "Uwaga! Nagłówek strumienia audio %d przedefiniowany!\n"
#define MSGTR_VideoStreamRedefined "Uwaga! Nagłówek strumienia video %d przedefiniowany!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Zbyt wiele (%d w %d bajtach) pakietów audio w buforze!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Zbyt wiele (%d w %d bajtach) pakietów video w buforze!\n"
#define MSGTR_MaybeNI "(może odtwarzacz non-interleaved strumień/plik lub kodek nie zadziałał)\n"
#define MSGTR_DetectedAVIfile "Wykryty format AVI!\n"
#define MSGTR_DetectedASFfile "Wykryty format ASF!\n"
#define MSGTR_DetectedMPEGPESfile "Wykryty format MPEG-PES!\n"
#define MSGTR_DetectedMPEGPSfile "Wykryty format MPEG-PS!\n"
#define MSGTR_DetectedMPEGESfile "Wykryty format MPEG-ES!\n"
#define MSGTR_DetectedQTMOVfile "Wykryty format QuickTime/MOV!\n"
#define MSGTR_MissingMpegVideo "Zagubiony strumien video MPEG !? skontaktuj się z autorem, może to błąd:(\n"
#define MSGTR_InvalidMPEGES "Błędny strumień MPEG-ES ??? skontaktuj się z autorem, może to błąd:(\n"
#define MSGTR_FormatNotRecognized "========== Przepraszam,  format pliku nierozpoznany/nieobsługiwany ==========\n"\
				  "=== Jeśli to strumień AVI, ASF lub MPEG, proszę skontaktuj się z autorem! ===\n"
#define MSGTR_MissingASFvideo "ASF: nie znaleziono strumienia video!\n"
#define MSGTR_MissingASFaudio "ASF: nie znaleziono strumienia audio...  ->nosound\n"
#define MSGTR_MissingMPEGaudio "MPEG: nie znaleziono strumienia audio...  ->nosound\n"

//#define MSGTR_
