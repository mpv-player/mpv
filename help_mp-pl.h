// Translated by:  Bohdan Horst <nexus@hoth.amu.edu.pl>

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
#ifdef HAVE_NEW_GUI
"U¿ycie:   mplayer [-gui] [opcje] [url|¶cie¿ka/]nazwa\n"
#else
"U¿ycie:   mplayer [opcje] [url|¶cie¿ka/]nazwa\n"
#endif
"\n"
"Podstawowe opcje: (pe³na lista w manualu)\n"
" -vo <drv[:dev]> wybór sterownika[:urz±dzenia] video (lista po '-vo help')\n"
" -ao <drv[:dev]> wybór sterownika[:urz±dzenia] audio (lista po '-ao help')\n"
#ifdef HAVE_VCD
" -vcd <trackno>  odtwarzanie bezpo¶rednio ¶cie¿ki VCD (video cd)\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  urz±dzenie DVD do autentykacji (dla zaszyfrowanych dysków)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  odtwarzanie bezpo¶rednio ¶cie¿ki DVD\n"
" -alang/-slang   jêzyk dla d¼wiêku/napisów (poprzez 2-znakowy kod kraju)\n"
#endif
" -ss <timepos>   skok do podanej pozycji (sekundy albo hh:mm:ss)\n"
" -nosound        odtwarzanie bez d¼wiêku\n"
" -fs -vm -zoom   opcje pe³noekranowe (pe³en ekran,zmiana trybu,skalowanie)\n"
" -x <x> -y <y>   wybór rozdzielczo¶ci ekranu (dla zmian trybu video lub \n"
"                 skalowania softwarowego)\n"
" -sub <plik>     wybór pliku z napisami (zobacz tak¿e -subfps, -subdelay)\n"
" -playlist <plik>wybór pliku z playlist±\n"
" -vid x -aid y   wybór odtwarzanego strumienia video (x) i audio (y)\n"
" -fps x -srate y wybór prêdko¶ci odtwarzania video (x fps) i audio (y Hz)\n"
" -pp <opcje>     wybór postprocesingu (zobacz manual/dokumentacjê)\n"
" -framedrop      gubienie klatek (dla wolnych maszyn)\n"
"\n"
"Podstawowe klawisze: (pe³na lista w manualu, sprawd¼ tak¿e input.conf\n"
" Right,Up,PgUp   skok naprzód o 10 sekund, 1 minutê, 10 minut\n"
" Left,Down,PgDn  skok do ty³u o 10 sekund, 1 minutê, 10 minut\n"
" < lub >         przeskok o jedn± pozycjê w playli¶cie\n"
" p lub SPACE     zatrzymanie filmu (kontynuacja - dowolny klawisz)\n"
" q lub ESC       zatrzymanie odtwarzania i wyj¶cie z programu\n"
" + lub -         regulacja opó¼nienia d¼wiêku o +/- 0,1 sekundy\n"
" o               prze³±czanie trybów OSD: pusty / belka / belka i zegar\n"
" * lub /         zwiêkszenie lub zmniejszenie natê¿enia d¼wiêku\n"
" z lub x         regulacja opó¼nienia napisów o +/- 0,1 sekundy\n"
" r lub t         regulacja po³o¿enia napisów (zobacz tak¿e -vop expand !)\n"
"\n"
" **** DOK£ADNY SPIS WSZYSTKICH DOSTÊPNYCH OPCJI ZNAJDUJE SIÊ W MANUALU! ****\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nWychodzê... (%s)\n"
#define MSGTR_Exit_frames "Zadana liczba klatek odtworzona"
#define MSGTR_Exit_quit "Wyj¶cie"
#define MSGTR_Exit_eof "Koniec pliku"
#define MSGTR_Exit_error "B³±d krytyczny"
#define MSGTR_IntBySignal "\nMPlayer przerwany sygna³em %d w module: %s \n"
#define MSGTR_NoHomeDir "Nie mogê znale¼æ katalogu HOME\n"
#define MSGTR_GetpathProblem "problem z get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Tworzê plik z konfiguracj±: %s\n"
#define MSGTR_InvalidVOdriver "Nieprawid³owa nazwa sterownika video: %s\nU¿yj '-vo help' aby dostaæ listê dostêpnych sterowników video.\n"
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
#define MSGTR_NoVideoStream "Przykro mi, brak strumienia video... nie dzia³a to na razie\n"
#define MSGTR_TryForceAudioFmt "Wymuszam zastosowanie kodeka audio z rodziny %d ...\n"
#define MSGTR_CantFindAfmtFallback "Nie mogê znale¼æ kodeka audio dla wymuszonej rodziny, wracam do standardowych.\n"
#define MSGTR_CantFindAudioCodec "Nie mogê znale¼æ kodeka dla formatu audio 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Spróbuj uaktualniæ %s etc/codecs.conf\n*** Je¶li to nie pomaga, przeczytaj DOCS/codecs.html !\n"
#define MSGTR_CouldntInitAudioCodec "Nie moge zainicjowaæ sterownika audio! -> nosound\n"
#define MSGTR_TryForceVideoFmt "Wymuszam zastosowanie kodeka video z rodziny %d ...\n"
#define MSGTR_CantFindVfmtFallback "Nie mogê znale¼æ kodeka video dla wymuszonej rodziny, wracam do standardowych..\n"
#define MSGTR_CantFindVideoCodec "Nie mogê znale¼æ kodeka dla wybranego -vo i formatu video 0x%X !\n"
#define MSGTR_VOincompCodec "Przykro mi, wybrany sterownik video_out jest niekompatybilny z tym kodekiem.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Nie mogê zainicjowaæ kodeka video :(\n"
#define MSGTR_EncodeFileExists "Plik ju¿ istnieje: %s (nie nadpisz swojego ulubionego AVI!)\n"
#define MSGTR_CantCreateEncodeFile "Nie mogê stworzyæ pliku do zakodowania\n"
#define MSGTR_CannotInitVO "FATAL: Nie mogê zainicjowaæ sterownika video!\n"
#define MSGTR_CannotInitAO "Nie mogê otworzyæ/zainicjowaæ urz±dzenia audio -> NOSOUND\n"
#define MSGTR_StartPlaying "Pocz±tek odtwarzania...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ************************************\n"\
"         *** Twój system jest zbyt wolny! ***\n"\
"         ************************************\n"\
"!!! Mo¿liwe przyczyny, problemy, rozwi±zania: \n"\
"- Najczêstsza przyczyna: uszkodzony/obarczony b³edami sterownik _audio_.\n"\
"  Rozwi±zanie: spróbuj -ao sdl lub u¿yj ALSA 0.5 lub emulacjê OSS w ALSA 0.9\n"\
"  Przeczytaj DOCS/sound.html!\n"\
"- Wolny sterownik video. Spróbuj z inny sterownikiem -vo (lista: -vo help)\n"\
"  lub odtwarzaj z opcj± -framedrop ! Przeczytaj DOCS/video.html!\n"\
"- Wolny procesor. Nie odtwarzaj du¿ych dvd/divx na wolnych procesorach!\n"\
"  Spróbuj z opcj± -hardframedrop\n"\
"- Uszkodzony plik. Spróbuj ró¿nych kombinacji: -nobps -ni -mc 0 -forceidx\n"\
"- U¿ywasz -cache do odtwarzania plikow non-interleaved? Spróbuj z -nocache\n"\
"Je¶li nic z powy¿szego nie pomaga, przeczytaj DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer zosta³ skompilowany BEZ obs³ugi GUI!\n"
#define MSGTR_GuiNeedsX "GUI MPlayera wymaga X11!\n"
#define MSGTR_Playing "Odtwarzam %s\n"
#define MSGTR_NoSound "Audio: brak d¼wiêku!!!\n"
#define MSGTR_FPSforced "FPS wymuszone na %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c
#define MSGTR_CdDevNotfound "Urz±dzenie CD-ROM '%s' nie znalezione!\n"
#define MSGTR_ErrTrackSelect "B³±d wyboru ¶cie¿ki VCD!"
#define MSGTR_ReadSTDIN "Odczytujê ze stdin...\n"
#define MSGTR_UnableOpenURL "Nie mogê otworzyæ URL: %s\n"
#define MSGTR_ConnToServer "Po³±czony z serwerem: %s\n"
#define MSGTR_FileNotFound "Plik nieznaleziony: '%s'\n"

#define MSGTR_CantOpenDVD "Nie mogê otworzyæ urz±dzenia DVD: %s\n"
#define MSGTR_DVDwait "Odczytujê strukturê dysku, proszê czekaæ...\n"
#define MSGTR_DVDnumTitles "Na tym DVD znajduje siê %d tytu³ów.\n"
#define MSGTR_DVDinvalidTitle "Nieprawid³owy numer tytu³u DVD: %d\n"
#define MSGTR_DVDnumChapters "W tym tytule DVD znajduje siê %d rozdzia³ów.\n"
#define MSGTR_DVDinvalidChapter "Nieprawid³owy numer rozdzia³u DVD: %d\n"
#define MSGTR_DVDnumAngles "W tym tytule DVD znajduje siê %d ustawieñ kamery.\n"
#define MSGTR_DVDinvalidAngle "Nieprawid³owy numer ustawienia kamery DVD: %d\n"
#define MSGTR_DVDnoIFO "Nie mogê otworzyæ pliku IFO dla tytu³u DVD %d.\n"
#define MSGTR_DVDnoVOBs "Nie mogê otworzyæ tytu³u VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD otwarte poprawnie!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Uwaga! Nag³ówek strumienia audio %d przedefiniowany!\n"
#define MSGTR_VideoStreamRedefined "Uwaga! Nag³ówek strumienia video %d przedefiniowany!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Zbyt wiele (%d w %d bajtach) pakietów audio w buforze!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Zbyt wiele (%d w %d bajtach) pakietów video w buforze!\n"
#define MSGTR_MaybeNI "(mo¿e odtwarzasz strumieñ/plik non-interleaved lub kodek nie zadzia³a³)\n"\
		      "Dla plików .AVI spróbuj wymusiæ tryb non-interleaved poprzez opcje -ni\n"
#define MSGTR_DetectedFILMfile "Wykryto format FILM!\n"
#define MSGTR_DetectedFLIfile "Wykryto format FLI!\n"
#define MSGTR_DetectedROQfile "Wykryto format RoQ!\n"
#define MSGTR_DetectedREALfile "Wykryto format REAL!\n"
#define MSGTR_DetectedAVIfile "Wykryto format AVI!\n"
#define MSGTR_DetectedASFfile "Wykryto format ASF!\n"
#define MSGTR_DetectedMPEGPESfile "Wykryto format MPEG-PES!\n"
#define MSGTR_DetectedMPEGPSfile "Wykryto format MPEG-PS!\n"
#define MSGTR_DetectedMPEGESfile "Wykryto format MPEG-ES!\n"
#define MSGTR_DetectedQTMOVfile "Wykryto format QuickTime/MOV!\n"
#define MSGTR_MissingMpegVideo "Zagubiony strumieñ video MPEG !? skontaktuj siê z autorem, mo¿e to b³±d:(\n"
#define MSGTR_InvalidMPEGES "B³êdny strumieñ MPEG-ES ??? skontaktuj siê z autorem, mo¿e to b³±d:(\n"
#define MSGTR_FormatNotRecognized "=========== Przykro mi, format pliku nierozpoznany/nieobs³ugiwany ===========\n"\
				  "=== Je¶li to strumieñ AVI, ASF lub MPEG, proszê skontaktuj siê z autorem! ===\n"
#define MSGTR_MissingVideoStream "Nie znaleziono strumienia video!\n"
#define MSGTR_MissingAudioStream "Nie znaleziono strumienia audio... -> nosound\n"
#define MSGTR_MissingVideoStreamBug "Zgubiony strumieñ video!? skontaktuj siê z autorem, mo¿e to b³±d:(\n"

#define MSGTR_DoesntContainSelectedStream "demux: plik nie zawiera wybranego strumienia audio lub video\n"

#define MSGTR_NI_Forced "Wymuszony"
#define MSGTR_NI_Detected "Wykryty"
#define MSGTR_NI_Message "%s format pliku NON-INTERLEAVED AVI !\n"

#define MSGTR_UsingNINI "U¿ywa uszkodzonego formatu pliku NON-INTERLEAVED AVI !\n"
#define MSGTR_CouldntDetFNo "Nie mogê okre¶liæ liczby klatek (dla przeszukiwania)\n"
#define MSGTR_CantSeekRawAVI "Nie mogê przeszukiwaæ nieindeksowanych strumieni .AVI! (sprawd¼ opcjê -idx !)\n"
#define MSGTR_CantSeekFile "Nie mogê przeszukiwaæ tego pliku!  \n"

#define MSGTR_EncryptedVOB "Zaszyfrowany plik VOB (nie wkompilowano obs³ugi libcss)! Przeczytaj plik DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Zaszyfrowany strumieñ, nie za¿yczy³e¶ sobie autentykacji!!\n"

#define MSGTR_MOVcomprhdr "MOV: Skompresowane nag³ówki nie s± obs³ugiwane (na razie)!\n"
#define MSGTR_MOVvariableFourCC "MOV: Uwaga! wykryto zmienn± FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Uwaga! zbyt du¿o scie¿ek!"
#define MSGTR_MOVnotyetsupp "\n**** Format Quicktime MOV nie jest na razie obs³ugiwany !!!!!!! ****\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nie mogê otworzyæ kodeka\n"
#define MSGTR_CantCloseCodec "nie mogê zamkn±æ kodeka\n"

#define MSGTR_MissingDLLcodec "ERROR: Nie mogê otworzyæ wymaganego kodeka DirectShow: %s\n"
#define MSGTR_ACMiniterror "Nie mogê za³adowaæ/zainicjalizowaæ kodeka Win32/ACM AUDIO (brakuje pliku DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nie moge znale¼æ w libavcodec kodeka '%s' ...\n"

#define MSGTR_NoDShowSupport "MPlayer skompilowany BEZ obs³ugi directshow!\n"
#define MSGTR_NoWfvSupport "Obs³uga kodeków win32 wy³±czona lub niedostêpna na platformach nie-x86!\n"
#define MSGTR_NoDivx4Support "MPlayer skompilowany BEZ obs³ugi DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayer skompilowany BEZ obs³ugi ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Kodek audio Win32/ACM wy³±czony lub niedostêpny dla nie-x86 CPU -> wymuszam brak d¼wiêku :(\n"
#define MSGTR_NoDShowAudio "Skompilowane bez obs³ugi DirectShow -> wymuszam brak d¼wiêku :(\n"
#define MSGTR_NoOggVorbis "Kodek audio OggVorbis wy³±czony -> wymuszam brak d¼wiêku :(\n"
#define MSGTR_NoXAnimSupport "MPlayer skompilowany BEZ obs³ugi XAnim!\n"

#define MSGTR_MpegPPhint "UWAGA! Za¿±da³e¶ u¿ycia filtra wyg³adzaj±cego dla video MPEG 1/2,\n" \
			 "       ale skompilowa³e¶ MPlayera bez obs³ugi wyg³adzania dla MPEG 1/2!\n" \
			 "       #define MPEG12_POSTPROC w config.h, i przekompiluj libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF podczas przeszukiwania nag³ówka sekwencji\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Nie mogê odczytaæ nag³ówka sekwencji!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Nie mogê odczytaæ rozszerzenia nag³ówka sekwencji!!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Nieprawid³owy nag³ówek sekwencji!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Nieprawid³owe rozszerzenie nag³ówka sekwencji!\n"

#define MSGTR_ShMemAllocFail "Nie mogê zaalokowaæ pamiêci dzielonej\n"
#define MSGTR_CantAllocAudioBuf "Nie mogê zaalokowaæ buforu wyj¶ciowego audio\n"
#define MSGTR_NoMemForDecodedImage "Za ma³o pamiêci dla zdekodowanego bufora obrazu (%ld bajtów)\n"

#define MSGTR_AC3notvalid "Nieprawid³owy strumieñ AC3.\n"
#define MSGTR_AC3only48k "Obs³ugiwane s± tylko strumienie 48000 Hz.\n"
#define MSGTR_UnknownAudio "Nieznany/zgubiony format audio, nie u¿ywam d¼wiêku\n"

// LIRC:
#define MSGTR_SettingUpLIRC "W³±czam obs³ugê lirc...\n"
#define MSGTR_LIRCdisabled "Nie bêdziesz móg³ u¿ywaæ twojego pilota\n"
#define MSGTR_LIRCopenfailed "Nieudane otwarcie obs³ugi lirc!\n"
#define MSGTR_LIRCsocketerr "Co¶ jest nie tak z socketem lirc: %s\n"
#define MSGTR_LIRCcfgerr "Nieudane odczytanie pliku konfiguracyjnego LIRC %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O programie"
#define MSGTR_FileSelect "Wybór pliku ..."
#define MSGTR_SubtitleSelect "Wybór napisów ..."
#define MSGTR_OtherSelect "Wybór ..."
#define MSGTR_AudioFileSelect "Wybór zewnêtrznego kana³u ..."
#define MSGTR_FontSelect "Wybór fontu ..."
#define MSGTR_MessageBox "Komunikat"
#define MSGTR_PlayList "Playlista"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Przegl±darka Skórek"
#define MSGTR_Network "Strumieñ sieciowy ..."
#define MSGTR_Preferences "Preferencje"
#define MSGTR_OSSPreferences "Konfiguracja sterownika OSS"

// --- buttons ---
#define MSGTR_Ok "Tak"
#define MSGTR_Cancel "Anuluj"
#define MSGTR_Add "Dodaj"
#define MSGTR_Remove "Usuñ"
#define MSGTR_Clear "Wyczy¶æ"
#define MSGTR_Config "Konfiguracja"
#define MSGTR_ConfigDriver "Konfiguracja sterownika"
#define MSGTR_Browse "Przegl±daj"

// --- error messages ---
#define MSGTR_NEMDB "Przykro mi, za ma³o pamiêci na bufor rysowania."
#define MSGTR_NEMFMR "Przykro mi, za ma³o pamiêci na renderowanie menu."
#define MSGTR_NEMFMM "Przykro mi, za ma³o pamiêci na maskê kszta³tu g³ównego okna."
#define MSGTR_IDFGCVD "Przykro mi, nie znalaz³em kompatybilnego sterownika video."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] b³±d w pliku konfiguracyjnym skórki w linii %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] ostrze¿enie w pliku konfiguracyjnym skórki w linii %d: widget znaleziony, ale poprzednia \"sekcja\" nie znaleziona ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] ostrze¿enie w pliku konfiguracyjnym skórki w linii %d: widget znaleziony, ale poprzednia \"podsekcja\" nie znaleziona (%s)"
#define MSGTR_SKIN_BITMAP_16bit "Bitmapy 16 bitowe lub mniejsze nie obs³ugiwane ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound "plik nie znaleziony ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "b³±d odczytu bmp ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "b³±d odczytu tga ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "b³±d odczytu png ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "tga kompresowane RLE nie obs³ugiwane ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "nieznany typ pliku ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "b³±d konwersji 24 bitów na 32 bity ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "nieznany komunikat: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "za ma³o pamiêci\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "za du¿o zadeklarowanych fontów\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "nie znaleziono pliku z fontami\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "nie znaleziono pliku z obrazem fontu\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "nie istniej±cy identyfikator fontu ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "nieznany parametr ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] za ma³o pamiêci.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skórka nie znaleziona ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "B³ad odczytu pliku konfiguracyjnego skórki ( %s ).\n"
#define MSGTR_SKIN_LABEL "Skórki:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O MPlayerze"
#define MSGTR_MENU_Open "Otwórz ..."
#define MSGTR_MENU_PlayFile "Odtwarzaj plik ..."
#define MSGTR_MENU_PlayVCD "Odtwarzaj VCD ..."
#define MSGTR_MENU_PlayDVD "Odtwarzaj DVD ..."
#define MSGTR_MENU_PlayURL "Odtwarzaj URL ..."
#define MSGTR_MENU_LoadSubtitle "Za³aduj napisy ..."
#define MSGTR_MENU_LoadExternAudioFile "Za³aduj zewnêtrzny plik audio ..."
#define MSGTR_MENU_Playing "Odtwarzanie"
#define MSGTR_MENU_Play "Odtwarzaj"
#define MSGTR_MENU_Pause "Pauza"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Nastêpny strumieñ"
#define MSGTR_MENU_PrevStream "Poprzedni strumieñ"
#define MSGTR_MENU_Size "Wielko¶æ"
#define MSGTR_MENU_NormalSize "Normalna wielko¶æ"
#define MSGTR_MENU_DoubleSize "Podwójna wielko¶æ"
#define MSGTR_MENU_FullScreen "Pe³en ekran"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Odtwarzaj dysk ..."
#define MSGTR_MENU_ShowDVDMenu "Poka¿ menu DVD"
#define MSGTR_MENU_Titles "Tytu³y"
#define MSGTR_MENU_Title "Tytu³ %2d"
#define MSGTR_MENU_None "(puste)"
#define MSGTR_MENU_Chapters "Rozdzia³y"
#define MSGTR_MENU_Chapter "Rozdzia³ %2d"
#define MSGTR_MENU_AudioLanguages "Jêzyki audio"
#define MSGTR_MENU_SubtitleLanguages "Jêzyki napisów"
#define MSGTR_MENU_PlayList "Playlista"
#define MSGTR_MENU_SkinBrowser "Przegl±darka skórek"
#define MSGTR_MENU_Preferences "Preferencje"
#define MSGTR_MENU_Exit "Wyj¶cie ..."

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Jasno¶æ: "
#define MSGTR_EQU_Hue "Hue: "
#define MSGTR_EQU_Saturation "Saturation: "
#define MSGTR_EQU_Front_Left "Lewy Przedni"
#define MSGTR_EQU_Front_Right "Prawy Przedni"
#define MSGTR_EQU_Back_Left "Lewy Tylni"
#define MSGTR_EQU_Back_Right "Prawy Tylni"
#define MSGTR_EQU_Center "Centralny"
#define MSGTR_EQU_Bass "Basowy"
#define MSGTR_EQU_All "Wszystkie"

// --- playlist
#define MSGTR_PLAYLIST_Path "¦cie¿ka"
#define MSGTR_PLAYLIST_Selected "Wybrane pliki"
#define MSGTR_PLAYLIST_Files "Pliki"
#define MSGTR_PLAYLIST_DirectoryTree "Drzewo katalogów"

// --- preferences
#define MSGTR_PREFERENCES_None "Puste"
#define MSGTR_PREFERENCES_Codec1 "U¿yj kodeków VFW (Win32)"
#define MSGTR_PREFERENCES_Codec2 "U¿yj kodeków OpenDivX/DivX4 (YV12)"
#define MSGTR_PREFERENCES_Codec3 "U¿yj kodeków DirectShow (Win32)"
#define MSGTR_PREFERENCES_Codec4 "U¿yj kodeków ffmpeg (libavcodec)"
#define MSGTR_PREFERENCES_Codec5 "U¿yj kodeków DivX4 (YUY2)"
#define MSGTR_PREFERENCES_Codec6 "U¿yj kodeków XAnim"
#define MSGTR_PREFERENCES_AvailableDrivers "Dostêpne sterowniki:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nie odtwarzaj d¼wiêku"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizuj d¼wiêk"
#define MSGTR_PREFERENCES_EnEqualizer "W³±cz equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "W³±cz extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Coefficient:"
#define MSGTR_PREFERENCES_AudioDelay "Opó¼nienie d¼wiêku"
#define MSGTR_PREFERENCES_Audio "D¼wiêk"
#define MSGTR_PREFERENCES_VideoEqu "W³±cz equalizer video"
#define MSGTR_PREFERENCES_DoubleBuffer "W³±cz podwójne buforowanie"
#define MSGTR_PREFERENCES_DirectRender "W³±cz bezpo¶rednie rysowanie"
#define MSGTR_PREFERENCES_FrameDrop "W³±cz zrzucanie ramek"
#define MSGTR_PREFERENCES_HFrameDrop "W³±cz gwa³towne zrzucanie ramek (niebezpieczne)"
#define MSGTR_PREFERENCES_Flip "Odwróæ obraz góra-dó³"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_OSDTimer "Timer i wska¼niki"
#define MSGTR_PREFERENCES_OSDProgress "Tylko belki"
#define MSGTR_PREFERENCES_Subtitle "Napisy:"
#define MSGTR_PREFERENCES_SUB_Delay "Opó¼nienie napisów: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozycja: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Wy³±cz automatyczne ³adowanie napisów"
#define MSGTR_PREFERENCES_SUB_Unicode "Napisy w Unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konertuj podane napisy do formatu napisów Mplayera"
#define MSGTR_PREFERENCES_SUB_SRT "Konwertuj podane napisy do formatu SRT (time-based SubViewer)"
#define MSGTR_PREFERENCES_Font "Font:"
#define MSGTR_PREFERENCES_FontFactor "Font factor:"
#define MSGTR_PREFERENCES_PostProcess "W³±cz postprocesing"
#define MSGTR_PREFERENCES_AutoQuality "Automatyczna jako¶æ: "
#define MSGTR_PREFERENCES_NI "U¿yj parsera dla non-interleaved AVI"
#define MSGTR_PREFERENCES_IDX "Przebuduj tablice indeksów jesli to potrzebne"
#define MSGTR_PREFERENCES_VideoCodecFamily "Rodzina kodeków video:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Poziom OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Napisy"
#define MSGTR_PREFERENCES_FRAME_Font "Font"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocesing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Kodek & demuxer"
#define MSGTR_PREFERENCES_OSS_Device "Urz±dzenie:"
#define MSGTR_PREFERENCES_OSS_Mixer "Mikser:"
#define MSGTR_PREFERENCES_Message "Proszê pamiêtaæ, ¿e niektóre funkcje wymagaja restartowania odtwarzania."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "b³±d krytyczny ..."
#define MSGTR_MSGBOX_LABEL_Error "b³±d ..."
#define MSGTR_MSGBOX_LABEL_Warning "ostrze¿enie ..." 

#endif
