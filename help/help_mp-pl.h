// Translated by:  Bohdan Horst <nexus@hoth.amu.edu.pl>
// Wszelkie uwagi i poprawki mile widziane :)
//
// Fixes and updates: Wojtek Kaniewski <wojtekka@bydg.pdi.net>
// Last sync with help_mp-en.h: 2003-03-29

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2003 Arpad Gereoffy (zobacz DOCS)\n"
"\n";

static char help_text[]=
"U¿ycie:   mplayer [opcje] [url|¶cie¿ka/]nazwa\n"
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
" -dvd <titleno>  odtwarzanie bezpo¶rednio tytu³u DVD\n"
" -alang/-slang   jêzyk dla d¼wiêku/napisów (poprzez 2-znakowy kod kraju)\n"
#endif
" -ss <timepos>   skok do podanej pozycji (sekundy albo hh:mm:ss)\n"
" -nosound        odtwarzanie bez d¼wiêku\n"
" -fs -vm -zoom   opcje pe³noekranowe (pe³en ekran,zmiana trybu,skalowanie)\n"
" -x <x> -y <y>   wybór rozdzielczo¶ci ekranu (dla -vm lub -zoom)\n"
" -sub <plik>     wybór pliku z napisami (zobacz tak¿e -subfps, -subdelay)\n"
" -playlist <plik>wybór pliku z playlist±\n"
" -vid x -aid y   wybór odtwarzanego strumienia video (x) i audio (y)\n"
" -fps x -srate y wybór prêdko¶ci odtwarzania video (x fps) i audio (y Hz)\n"
" -pp <opcje>     wybór postprocesingu (zobacz manual)\n"
" -framedrop      gubienie klatek (dla wolnych maszyn)\n"
"\n"
"Podstawowe klawisze: (pe³na lista w manualu, sprawd¼ tak¿e input.conf)\n"
" Right,Up,PgUp   skok naprzód o 10 sekund, 1 minutê, 10 minut\n"
" Left,Down,PgDn  skok do ty³u o 10 sekund, 1 minutê, 10 minut\n"
" < lub >         przeskok o jedn± pozycjê w playli¶cie\n"
" p lub SPACE     zatrzymanie filmu (kontynuacja - dowolny klawisz)\n"
" q lub ESC       zatrzymanie odtwarzania i wyj¶cie z programu\n"
" + lub -         regulacja opó¼nienia d¼wiêku o +/- 0,1 sekundy\n"
" o               prze³±czanie trybów OSD: pusty / belka / belka i zegar\n"
" * lub /         zwiêkszenie lub zmniejszenie natê¿enia d¼wiêku\n"
" z lub x         regulacja opó¼nienia napisów o +/- 0,1 sekundy\n"
" r lub t         regulacja po³o¿enia napisów (zobacz tak¿e -vop expand)\n"
"\n"
" **** DOK£ADNY SPIS WSZYSTKICH DOSTÊPNYCH OPCJI ZNAJDUJE SIÊ W MANUALU! ****\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nWychodzê... (%s)\n"
#define MSGTR_Exit_quit "Wyj¶cie"
#define MSGTR_Exit_eof "Koniec pliku"
#define MSGTR_Exit_error "B³±d krytyczny"
#define MSGTR_IntBySignal "\nMPlayer przerwany sygna³em %d w module: %s \n"
#define MSGTR_NoHomeDir "Nie mogê znale¼æ katalogu domowego\n"
#define MSGTR_GetpathProblem "problem z get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Tworzê plik z konfiguracj±: %s\n"
#define MSGTR_InvalidVOdriver "Nieprawid³owa nazwa sterownika video: %s\nU¿yj '-vo help' aby dostaæ listê dostêpnych sterowników video.\n"
#define MSGTR_InvalidAOdriver "Nieprawid³owa nazwa sterownika audio: %s\nU¿yj '-ao help' aby dostaæ listê dostêpnych sterowników audio.\n"
#define MSGTR_CopyCodecsConf "(skopiuj/zlinkuj etc/codecs.conf do ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "U¿ywam domy¶lnego (wkompilowanego) codecs.conf\n"
#define MSGTR_CantLoadFont "Nie mogê za³adowaæ fontu: %s\n"
#define MSGTR_CantLoadSub "Nie mogê za³adowaæ napisów: %s\n"
#define MSGTR_ErrorDVDkey "B³±d w przetwarzaniu DVD KEY.\n"
#define MSGTR_CmdlineDVDkey "¯±dany klucz DVD u¿ywany jest do dekodowania.\n"
#define MSGTR_DVDauthOk "Sekwencja autoryzacji DVD wygl±da OK.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: nie ma wybranego strumienia!\n"
#define MSGTR_CantOpenDumpfile "Nie mogê otworzyæ pliku dump.\n"
#define MSGTR_CoreDumped "Core dumped ;)\n"
#define MSGTR_FPSnotspecified "FPS nie podane (lub b³êdne) w nag³ówku. U¿yj opcji -fps.\n"
#define MSGTR_TryForceAudioFmtStr "Wymuszam zastosowanie kodeka audio z rodziny %s ...\n"
#define MSGTR_CantFindAfmtFallback "Nie mogê znale¼æ kodeka audio dla wymuszonej rodziny, wracam do standardowych.\n"
#define MSGTR_CantFindAudioCodec "Nie mogê znale¼æ kodeka dla formatu audio 0x%X.\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Spróbuj uaktualniæ %s etc/codecs.conf\n*** Je¶li to nie pomaga, przeczytaj DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Nie moge zainicjowaæ sterownika audio -> brak d¼wiêku.\n"
#define MSGTR_TryForceVideoFmtStr "Wymuszam zastosowanie kodeka video z rodziny %s ...\n"
#define MSGTR_CantFindVideoCodec "Nie mogê znale¼æ kodeka dla wybranego -vo i formatu video 0x%X.\n"
#define MSGTR_VOincompCodec "Przykro mi, wybrany sterownik video_out jest niekompatybilny z tym kodekiem.\n"
#define MSGTR_CannotInitVO "B£¡D: Nie mogê zainicjowaæ sterownika video.\n"
#define MSGTR_CannotInitAO "Nie mogê otworzyæ/zainicjowaæ urz±dzenia audio -> brak d¼wiêku.\n"
#define MSGTR_StartPlaying "Pocz±tek odtwarzania...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ************************************\n"\
"         *** Twój system jest zbyt wolny! ***\n"\
"         ************************************\n"\
"Mo¿liwe przyczyny, problemy, rozwi±zania: \n"\
"- Najczêstsza przyczyna: uszkodzony/obarczony b³êdami sterownik _audio_.\n"\
"  Rozwi±zanie: spróbuj -ao sdl lub u¿yj ALSA 0.5 lub emulacjê OSS w ALSA 0.9\n"\
"  Przeczytaj DOCS/sound.html!\n"\
"  Mo¿esz tak¿e eksperymentowaæ z -autosync 30 (lub innymi warto¶ciami).\n"\
"- Wolny sterownik video. Spróbuj z inny sterownikiem -vo (lista: -vo help)\n"\
"  lub odtwarzaj z opcj± -framedrop!\n"\
"- Wolny procesor. Nie odtwarzaj du¿ych DVD/DivX na wolnych procesorach!\n"\
"  Spróbuj z opcj± -hardframedrop.\n"\
"- Uszkodzony plik. Spróbuj ró¿nych kombinacji: -nobps -ni -mc 0 -forceidx.\n"\
"- Odtwarzaj±c z wolnego ¼ród³a (zamontowane partycje NFS/SMB, DVD, VCD itp)\n"\
"  spróbuj z opcj± -cache 8192\n"\
"- U¿ywasz -cache do odtwarzania plikow non-interleaved? Spróbuj z -nocache\n"\
"Przeczytaj DOCS/video.html i DOCS/sound.html -- znajdziesz tam wskazówki\n"\
"jak przyspieszyæ dzia³anie. Je¶li nic z powy¿szego nie pomaga, przeczytaj\n"\
"DOCS/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer zosta³ skompilowany BEZ obs³ugi GUI.\n"
#define MSGTR_GuiNeedsX "GUI MPlayera wymaga X11.\n"
#define MSGTR_Playing "Odtwarzam %s\n"
#define MSGTR_NoSound "Audio: brak d¼wiêku\n"
#define MSGTR_FPSforced "FPS wymuszone na %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Skompilowane z wykrywaniem procesora - UWAGA - to nie jest optymalne!\nAby uzyskaæ lepsz± wydajno¶æ, przekompiluj MPlayera z opcj±\n--disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Skompilowane dla x86 CPU z rozszerzeniami:"
#define MSGTR_AvailableVideoOutputPlugins "Dostêpne pluginy video:\n"
#define MSGTR_AvailableVideoOutputDrivers "Dostêpne sterowniki video:\n"
#define MSGTR_AvailableAudioOutputDrivers "Dostêpne sterowniki audio:\n"
#define MSGTR_AvailableAudioCodecs "Dostêpne kodeki audio:\n"
#define MSGTR_AvailableVideoCodecs "Dostêpne kodeki video:\n"
#define MSGTR_AvailableAudioFm "\nDostêpne (wkompilowane) rodziny kodeków audio/sterowniki:\n"
#define MSGTR_AvailableVideoFm "\nDostêpne (wkompilowane) rodziny kodeków video/sterowniki:\n"
#define MSGTR_AvailableFsType "Dostêpne tryby pe³noekranowe:\n"
#define MSGTR_UsingRTCTiming "U¿ywam Linux's hardware RTC timing (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Video: nie mogê odczytaæ w³a¶ciwo¶ci\n"
#define MSGTR_NoStreamFound "Nie znaleziono strumienia.\n"
#define MSGTR_InitializingAudioCodec "Inicjalizujê kodek audio...\n"
#define MSGTR_ErrorInitializingVODevice "B³±d otwierania/inicjalizacji wybranego video_out (-vo) urz±dzenia!\n"
#define MSGTR_ForcedVideoCodec "Wymuszony kodek video: %s\n"
#define MSGTR_ForcedAudioCodec "Wymuszony kodek audio: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Opis: %s\nAO: Autor: %s\n"
#define MSGTR_AOComment "AO: Komentarz: %s\n"
#define MSGTR_Video_NoVideo "Video: brak video\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Nie mogê zainicjowaæ filtrów video (-vop) lub wyj¶cia video (-vo).\n"
#define MSGTR_Paused "\n================= PAUZA =================\r"
#define MSGTR_PlaylistLoadUnable "\nNie mo¿na za³adowaæ playlisty %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer wykona³ nieprawid³ow± operacjê.\n"\
"  Byæ mo¿e to b³±d w nowym kodzie wykrywania procesora...\n"\
"  Przeczytaj DOCS/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer wykona³ nieprawid³ow± operacjê.\n"\
"  Zwykle zdarza siê to, gdy uruchamiasz go na innym procesorze ni¿ ten,\n"\
"  dla którego zosta³ skompilowany/zoptymalizowany.\n  Sprawd¼ to!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer wykona³ nieprawid³ow± operacjê zwi±zan± z pamiêci±/koprocesorem.\n"\
"  Przekompiluj MPlayera z --enable-debug i wykonad backtrace 'gdb' i\n"\
"  deasemblacjê. Szczegó³y w pliku DOCS/bugreports.html#crash\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer wykona³ nieprawid³ow± operacjê. To nie powinno siê zdarzyæ.\n"\
"  Byæ mo¿e to b³±d w kodzie MPlayera _lub_ w Twoich sterownikach _lub_\n"\
"  w u¿ywanej przez Ciebie wersji gcc. Je¶li my¶lisz, ¿e to wina MPlayera,\n"\
"  przeczytaj proszê DOCS/bugreports.html i postêpuj zgodnie z instrukacjami.\n"\
"  Nie mo¿emy pomóc i nie pomo¿emy je¶li nie dostarczysz tych informacji przy\n"\
"  zg³aszaniu b³êdu.\n"
	

// mencoder.c:

#define MSGTR_MEncoderCopyright "(C) 2000-2003 Arpad Gereoffy (zobacz DOCS)\n"
#define MSGTR_UsingPass3ControllFile "U¿ywam pliku kontrolnego pass3: %s\n"
#define MSGTR_MissingFilename "\nBrak nazwy pliku.\n\n"
#define MSGTR_CannotOpenFile_Device "Nie mo¿na otworzyæ pliku/urz±dzenia.\n"
#define MSGTR_ErrorDVDAuth "B³ad w autoryzacji DVD.\n"
#define MSGTR_CannotOpenDemuxer "Nie mo¿na otworzyæ demuxera\n"
#define MSGTR_NoAudioEncoderSelected "\nNie wybrano enkodera audio (-oac). Wybierz jeden lub u¿yj -nosound. U¿yj -oac help!\n"
#define MSGTR_NoVideoEncoderSelected "\nNie wybrano enkodera video (-ovc). Wybierz jeden, u¿yj -ovc help!\n"
#define MSGTR_InitializingAudioCodec "Inicjalizuje kodek audio...\n"
#define MSGTR_CannotOpenOutputFile "Nie mogê otworzyæ pliku wynikowego: '%s'.\n"
#define MSGTR_EncoderOpenFailed "Nie mogê otworzyæ encodera.\n"
#define MSGTR_ForcingOutputFourcc "Wymuszam fourcc wynikowe na %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "Zapisuje nag³ówek AVI...\n"
#define MSGTR_DuplicateFrames "\npowtórzone %d ramek!\n"
#define MSGTR_SkipFrame "\nOpuszczam ramkê!\n"
#define MSGTR_ErrorWritingFile "%s: B³±d zapisu pliku.\n"
#define MSGTR_WritingAVIIndex "\nZapisujê indeks AVI...\n"
#define MSGTR_FixupAVIHeader "Naprawiam nag³ówek AVI...\n"
#define MSGTR_RecommendedVideoBitrate "Zalecane video bitrate dla %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nStrumieñ video: %8.3f kbit/s (%d bps) rozmiar: %d bajtów %5.3f sekund %d ramek\n"
#define MSGTR_AudioStreamResult "\nStrumieñ audio: %8.3f kbit/s (%d bps) rozmiar: %d bajtów %5.3f sekund\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metoda zmiennego bitrate\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(domy¶lna)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           ¶redni bitrate\n"\
"\n"\
" cbr           sta³y bitrate\n"\
"               Wymusza równie¿ kodowanie CBR w nastêpuj±cych po tej opcji\n"\
"               ustawieniach ABR\n"\
"\n"\
" br=<0-1024>   podaje bitrate w kilobitach (tylko CBR i ABR)\n"\
"\n"\
" q=<0-9>       jako¶æ (0-najwy¿sza, 9-najni¿sza) (tylko VBR)\n"\
"\n"\
" aq=<0-9>      jako¶æ algorytmu (0-najlepsza/najwolniejsza,\n"\
"               9-najgorsza/najszybsza)\n"\
"\n"\
" ratio=<1-100> wspó³czynnik kompresji\n"\
"\n"\
" vol=<0-10>    wzmocnienie sygna³u audio\n"\
"\n"\
" mode=<0-3>    (domy¶lnie: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: no\n"\
"                1: all\n"\
"                2: adjust\n"\
"\n"\
" fast          w³±cza szybsze kodowanie w nastêpuj±cych po tej opcji\n"\
"               ustawieniach VBR, nieznacznie ni¿sza jako¶æ i wy¿szy bitrate.\n"\
"\n"\
" preset=<value>  w³±cza najwy¿sze mo¿liwe ustawienia jako¶ci.\n"\
"                 medium: kodowanie VBR, dobra jako¶æ\n"\
"                 (bitrate w zakresie 150-180 kbps)\n"\
"                 standard: kodowanie VBR, wysoka jako¶æ\n"\
"                 (bitrate w zakresie 170-210 kbps)\n"\
"                 extreme: kodowanie VBR, bardzo wysoka jako¶æ\n"\
"                 (bitrate w zakresie 200-240 kbps)\n"\
"                 insane: kodowanie CBR, najwy¿sza mo¿liwa jako¶æ\n"\
"                 (bitrate 320 kbps)\n"\
"                 <8-320>: kodowanie ABR przy podanym ¶rednim bitrate.\n\n"
	
// open.c, stream.c:
#define MSGTR_CdDevNotfound "Urz±dzenie CD-ROM '%s' nie znalezione.\n"
#define MSGTR_ErrTrackSelect "B³±d wyboru ¶cie¿ki VCD."
#define MSGTR_ReadSTDIN "Odczytujê ze stdin...\n"
#define MSGTR_UnableOpenURL "Nie mogê otworzyæ URL: %s\n"
#define MSGTR_ConnToServer "Po³±czony z serwerem: %s\n"
#define MSGTR_FileNotFound "Plik nieznaleziony: '%s'\n"

#define MSGTR_SMBInitError "Nie mogê zainicjowaæ biblioteki libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Nie mogê otworzyæ z sieci: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer nie zosta³ skompilowany z obs³ug± SMB\n"

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
#define MSGTR_AudioStreamRedefined "Uwaga! Nag³ówek strumienia audio %d zdefiniowany ponownie.\n"
#define MSGTR_VideoStreamRedefined "Uwaga! Nag³ówek strumienia video %d zdefiniowany ponownie.\n"
#define MSGTR_TooManyAudioInBuffer "\nZbyt wiele (%d w %d bajtach) pakietów audio w buforze.\n"
#define MSGTR_TooManyVideoInBuffer "\nZbyt wiele (%d w %d bajtach) pakietów video w buforze.\n"
#define MSGTR_MaybeNI "Mo¿e odtwarzasz strumieñ/plik non-interleaved lub kodek nie zadzia³a³?\n"\
		      "Dla plików .AVI spróbuj wymusiæ tryb non-interleaved poprzez opcje -ni\n"
#define MSGTR_SwitchToNi "\nWykryto non-interleaved .AVI - prze³±czam na tryb -ni...\n"
#define MSGTR_Detected_XXX_FileFormat "Wykryto format %s.\n"
#define MSGTR_DetectedAudiofile "Wykryto plik audio.\n"
#define MSGTR_NotSystemStream "Nie jest to MPEG System Stream... (mo¿e Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "B³êdny strumieñ MPEG-ES??? Skontaktuj siê z autorem, mo¿e to b³±d :(\n"
#define MSGTR_FormatNotRecognized "=========== Przykro mi, nierozpoznany/nieobs³ugiwany format pliku ===========\n"\
				  "=== Je¶li to strumieñ AVI, ASF lub MPEG, proszê skontaktuj siê z autorem! ===\n"
#define MSGTR_MissingVideoStream "Nie znaleziono strumienia video.\n"
#define MSGTR_MissingAudioStream "Nie znaleziono strumienia audio -> brak d¼wiêku.\n"
#define MSGTR_MissingVideoStreamBug "Brakuj±cy strumieñ video?! Skontaktuj siê z autorem, mo¿e to b³±d :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Plik nie zawiera wybranego strumienia audio lub video\n"

#define MSGTR_NI_Forced "Wymuszony"
#define MSGTR_NI_Detected "Wykryty"
#define MSGTR_NI_Message "%s format pliku NON-INTERLEAVED AVI.\n"

#define MSGTR_UsingNINI "U¿ywa uszkodzonego formatu pliku NON-INTERLEAVED AVI.\n"
#define MSGTR_CouldntDetFNo "Nie mogê okre¶liæ liczby klatek (dla przeszukiwania)\n"
#define MSGTR_CantSeekRawAVI "Nie mogê przeszukiwaæ nieindeksowanych strumieni .AVI! (sprawd¼ opcjê -idx!)\n"
#define MSGTR_CantSeekFile "Nie mogê przeszukiwaæ tego pliku.\n"

#define MSGTR_EncryptedVOB "Zaszyfrowany plik VOB (nie wkompilowano obs³ugi libcss)! Przeczytaj DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Zaszyfrowany strumieñ, nie za¿±da³e¶ autentykacji!\n"

#define MSGTR_MOVcomprhdr "MOV: Skompresowane nag³ówki nie s± obs³ugiwane (na razie).\n"
#define MSGTR_MOVvariableFourCC "MOV: Uwaga! Wykryto zmienn± FOURCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Uwaga! Zbyt du¿o scie¿ek!"
#define MSGTR_FoundAudioStream "==> Znaleziono strumieñ audio: %d\n"
#define MSGTR_FoundVideoStream "==> Znaleziono strumieñ video: %d\n"
#define MSGTR_DetectedTV "Wykryto TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Nie mo¿na otworzyæ demuxera ogg.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Szukanie strumieni audio (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Nie mo¿na otworzyæ strumienia audio: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nie mo¿na otworzyæ strumienia z napisami: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Nieudane otwarcie demuxera audio: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Nieudane otwarcie demuxera napisów: %s\n"
#define MSGTR_TVInputNotSeekable "Wej¶cia TV nie mo¿na przeszukiwaæ! (Prawdopodobnie wyszukiwanie bedzie zmienia³o kana³y ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Demuxer info %s already present!\n"
#define MSGTR_ClipInfo "Informacja o klipie: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: Progressive seq detected, leaving 3:2 TELECINE mode\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 3:2 TELECINE detected, enabling inverse telecine fx. FPS changed to %5.3f!  \n"


// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nie mogê otworzyæ kodeka.\n"
#define MSGTR_CantCloseCodec "Nie mogê zamkn±æ kodeka.\n"

#define MSGTR_MissingDLLcodec "B£¡D: Nie mogê otworzyæ wymaganego kodeka DirectShow: %s.\n"
#define MSGTR_ACMiniterror "Nie mogê za³adowaæ/zainicjalizowaæ kodeka Win32/ACM AUDIO (brakuje pliku DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nie moge znale¼æ w libavcodec kodeka '%s'...\n"
#define MSGTR_MpegNoSequHdr "MPEG: B£¡D: Koniec pliku podczas przeszukiwania nag³ówka sekwencji.\n"
#define MSGTR_CannotReadMpegSequHdr "B£¡D: Nie mogê odczytaæ nag³ówka sekwencji.\n"
#define MSGTR_CannotReadMpegSequHdrEx "B£¡D: Nie mogê odczytaæ rozszerzenia nag³ówka sekwencji.\n"
#define MSGTR_BadMpegSequHdr "MPEG: Nieprawid³owy nag³ówek sekwencji.\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Nieprawid³owe rozszerzenie nag³ówka sekwencji.\n"

#define MSGTR_ShMemAllocFail "Nie mogê zaalokowaæ pamiêci dzielonej\n"
#define MSGTR_CantAllocAudioBuf "Nie mogê zaalokowaæ bufora wyj¶ciowego audio\n"

#define MSGTR_UnknownAudio "Nieznany/brakuj±cy format audio -> brak d¼wiêku\n"

#define MSGTR_UsingExternalPP "[PP] U¿ywam zewnêtrznego filtra postprocessingu, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] U¿ywam postprocessingu w kodeku, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Atrybut video '%s' nie jest obs³ugiwany przez wybrane vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Wybrana rodzina kodeków video [%s] (vfm=%d) niedostêpna (w³±cz j± podczas kompilacji)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Wybrana rodzina kodeków audio [%s] (afm=%d) niedostêpna (w³±cz j± podczas kompilacji)\n"
#define MSGTR_OpeningVideoDecoder "Otwieram dekoder video: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Otwieram dekoder audio: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit video: %s\n"
#define MSGTR_UninitAudioStr "uninit audio: %s\n"
#define MSGTR_VDecoderInitFailed "Nieudana inicjalizacja VDecodera :(\n"
#define MSGTR_ADecoderInitFailed "Nieudana inicjalizacja ADecodera :(\n"
#define MSGTR_ADecoderPreinitFailed "Nieudana preinicjalizacja ADecodera :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Przydzielam %d bajtów dla bufora wej¶ciowego\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Przydzielam %d + %d = %d bajtów dla bufora wyj¶ciowego\n"

// LIRC:
#define MSGTR_SettingUpLIRC "W³±czam obs³ugê LIRC...\n"
#define MSGTR_LIRCdisabled "Nie bêdziesz móg³ u¿ywaæ swojego pilota.\n"
#define MSGTR_LIRCopenfailed "Nieudane otwarcie obs³ugi LIRC.\n"
#define MSGTR_LIRCcfgerr "Nieudane odczytanie pliku konfiguracyjnego LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nie mogê znale¼æ filtra video '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Nie mogê otworzyæ filtra video '%s'\n"
#define MSGTR_OpeningVideoFilter "Otwieram filtr video: "
#define MSGTR_CannotFindColorspace "Nie mogê znale¼æ wspólnej przestrzeni koloru, nawet przez wstawienie 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Kodek nie ustawia sh->disp_w i sh->disp_h, probujê to rozwi±zaæ.\n"
#define MSGTR_VoConfigRequest "VDec: vo config request - %d x %d (preferred csp: %s)\n"	/* XXX */
#define MSGTR_CouldNotFindColorspace "Nie mogê znale¼æ pasuj±cej przestrzeni koloru - ponawiam próbê z -vop scale...\n"
#define MSGTR_MovieAspectIsSet "Proporcje filmu to %.2f:1 - skalujê do prawid³owych propocji.\n"
#define MSGTR_MovieAspectUndefined "Proporcje filmu nie s± zdefiniowane - nie skalujê.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O programie"
#define MSGTR_FileSelect "Wybór pliku ..."
#define MSGTR_SubtitleSelect "Wybór napisów ..."
#define MSGTR_OtherSelect "Wybór ..."
#define MSGTR_AudioFileSelect "Wybór zewnêtrznego kana³u ..."
#define MSGTR_FontSelect "Wybór fontu ..."
#define MSGTR_PlayList "Playlista"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Przegl±darka Skórek"
#define MSGTR_Network "Strumieñ sieciowy ..."
#define MSGTR_Preferences "Preferencje"
#define MSGTR_OSSPreferences "Konfiguracja sterownika OSS"
#define MSGTR_SDLPreferences "Konfiguracja sterownika SDL"
#define MSGTR_NoMediaOpened "Nie otwarto no¶nika."
#define MSGTR_VCDTrack "¦cie¿ka VCD: %d"
#define MSGTR_NoChapter "Brak rozdzia³u"
#define MSGTR_Chapter "Rozdzia³ %d"
#define MSGTR_NoFileLoaded "Nie za³adowano pliku."

// --- buttons ---
#define MSGTR_Ok "OK"
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
#define MSGTR_IDFGCVD "Przykro mi, nie znalaz³em kompatybilnego z GUI sterownika video."
#define MSGTR_NEEDLAVCFAME "Przykro mi, nie mo¿esz odtwarzaæ plików innych ni¿ MPEG za pomoc± twojego urz±dzenia DXR3/H+ bez przekodowania.\nProszê w³±cz lavc lub fame w konfiguracji DXR3/H+."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] b³±d w pliku konfiguracyjnym skórki w linii %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] ostrze¿enie w pliku konfiguracyjnym skórki w linii %d: kontrolka znaleziona, ale poprzednia \"sekcja\" nie znaleziona ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] ostrze¿enie w pliku konfiguracyjnym skórki w linii %d: kontrolka znaleziona, ale poprzednia \"podsekcja\" nie znaleziona (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] ostrze¿enie w pliku konfiguracyjnym skórki w linii %d: podsekcja nie jest obs³ugiwana przez t± kontrolkê (%s)"
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
#define MSGTR_MENU_DropSubtitle "Wy³aduj napisy ..."
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
#define MSGTR_MENU_Mute "Mute"
#define MSGTR_MENU_Original "Oryginalne"
#define MSGTR_MENU_AspectRatio "Proporcje obrazu"
#define MSGTR_MENU_AudioTrack "¦cie¿ka Audio"
#define MSGTR_MENU_Track "¦cie¿ka %d"
#define MSGTR_MENU_VideoTrack "¦cie¿ka Video"

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Jasno¶æ: "
#define MSGTR_EQU_Hue "Hue: "
#define MSGTR_EQU_Saturation "Nasycenie: "
#define MSGTR_EQU_Front_Left "Lewy Przedni"
#define MSGTR_EQU_Front_Right "Prawy Przedni"
#define MSGTR_EQU_Back_Left "Lewy Tylny"
#define MSGTR_EQU_Back_Right "Prawy Tylny"
#define MSGTR_EQU_Center "Centralny"
#define MSGTR_EQU_Bass "Basowy"
#define MSGTR_EQU_All "Wszystkie"
#define MSGTR_EQU_Channel1 "Kana³ 1:"
#define MSGTR_EQU_Channel2 "Kana³ 2:"
#define MSGTR_EQU_Channel3 "Kana³ 3:"
#define MSGTR_EQU_Channel4 "Kana³ 4:"
#define MSGTR_EQU_Channel5 "Kana³ 5:"
#define MSGTR_EQU_Channel6 "Kana³ 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "¦cie¿ka"
#define MSGTR_PLAYLIST_Selected "Wybrane pliki"
#define MSGTR_PLAYLIST_Files "Pliki"
#define MSGTR_PLAYLIST_DirectoryTree "Drzewo katalogów"

// --- preferences
#define MSGTR_PREFERENCES_Audio "D¼wiêk"
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_SubtitleOSD "Napisy i OSD"
#define MSGTR_PREFERENCES_Codecs "Kodeki i demuxery"
#define MSGTR_PREFERENCES_Misc "Ró¿ne"

#define MSGTR_PREFERENCES_None "Puste"
#define MSGTR_PREFERENCES_AvailableDrivers "Dostêpne sterowniki:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nie odtwarzaj d¼wiêku"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizuj d¼wiêk"
#define MSGTR_PREFERENCES_EnEqualizer "W³±cz equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "W³±cz extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Coefficient:"	/* XXX */
#define MSGTR_PREFERENCES_AudioDelay "Opó¼nienie d¼wiêku"
#define MSGTR_PREFERENCES_DoubleBuffer "W³±cz podwójne buforowanie"
#define MSGTR_PREFERENCES_DirectRender "W³±cz rysowanie bezpo¶rednie"
#define MSGTR_PREFERENCES_FrameDrop "W³±cz zrzucanie ramek"
#define MSGTR_PREFERENCES_HFrameDrop "W³±cz gwa³towne zrzucanie ramek (niebezpieczne)"
#define MSGTR_PREFERENCES_Flip "Odwróæ obraz do góry nogami"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Timer i wska¼niki"
#define MSGTR_PREFERENCES_OSDProgress "Tylko belki"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Timer, czas procentowy i ca³kowity"
#define MSGTR_PREFERENCES_Subtitle "Napisy:"
#define MSGTR_PREFERENCES_SUB_Delay "Opó¼nienie napisów: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozycja: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Wy³±cz automatyczne ³adowanie napisów"
#define MSGTR_PREFERENCES_SUB_Unicode "Napisy w Unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konwertuj podane napisy do formatu napisów Mplayera"
#define MSGTR_PREFERENCES_SUB_SRT "Konwertuj podane napisy do formatu SRT (time based SubViewer)"
#define MSGTR_PREFERENCES_SUB_Overlap "Prze³±cz nak³adanie siê napisów"
#define MSGTR_PREFERENCES_Font "Czcionka:"
#define MSGTR_PREFERENCES_FontFactor "Skala czcionki:"
#define MSGTR_PREFERENCES_PostProcess "W³±cz postprocesing"
#define MSGTR_PREFERENCES_AutoQuality "Automatyczna jako¶æ: "
#define MSGTR_PREFERENCES_NI "U¿yj parsera dla non-interleaved AVI"
#define MSGTR_PREFERENCES_IDX "Przebuduj tablice indeksów jesli to potrzebne"
#define MSGTR_PREFERENCES_VideoCodecFamily "Rodzina kodeków video:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Rodzina kodeków audio:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Poziom OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Napisy"
#define MSGTR_PREFERENCES_FRAME_Font "Font"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocesing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Kodek i demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Pamiêæ podrêczna"
#define MSGTR_PREFERENCES_FRAME_Misc "Ró¿ne"
#define MSGTR_PREFERENCES_OSS_Device "Urz±dzenie:"
#define MSGTR_PREFERENCES_OSS_Mixer "Mikser:"
#define MSGTR_PREFERENCES_SDL_Driver "Sterownik:"
#define MSGTR_PREFERENCES_Message "Proszê pamiêtaæ, ¿e niektóre funkcje wymagaja restartowania odtwarzania!"
#define MSGTR_PREFERENCES_DXR3_VENC "Enkoder video:"
#define MSGTR_PREFERENCES_DXR3_LAVC "U¿yj LAVC (ffmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "U¿yj FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unikod"
#define MSGTR_PREFERENCES_FontEncoding2 "Jêzyki zachodnioeuropejskie (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Jêzyki zachodnioeuropejskiez Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Jêzyki s³owiañskie i ¶rodkowoeuropejskie (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galician, maltañski, turecki (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Stare znaki ba³tyckie (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cyrylica (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabski (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Wspó³czesna greka (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turecki (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Ba³tycki (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celtycki (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Znaki hebrajskie (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Rosyjski (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukraiñski, bia³oruski (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Uproszczone znaki chiñskie (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Tradycyjne znaki chiñskie (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Znaki japoñskie (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Znaki koreañskie (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Znaki tajskie (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrylica Windows (CP1251)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Bez autoskalowania"
#define MSGTR_PREFERENCES_FontPropWidth "Proporcjonalnie do szeroko¶ci filmu"
#define MSGTR_PREFERENCES_FontPropHeight "Proporcjonalnie do wysoko¶ci filmu"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporcjonalnie do przek±tnej filmu"
#define MSGTR_PREFERENCES_FontEncoding "Kodowanie:"
#define MSGTR_PREFERENCES_FontBlur "Rozmazanie:"
#define MSGTR_PREFERENCES_FontOutLine "Obramowanie:"
#define MSGTR_PREFERENCES_FontTextScale "Skalowanie tekstu:"
#define MSGTR_PREFERENCES_FontOSDScale "Skalowanie OSD:"
#define MSGTR_PREFERENCES_Cache "Cache w³±cz/wy³±cz"
#define MSGTR_PREFERENCES_CacheSize "Wielko¶æ cache: "
#define MSGTR_PREFERENCES_LoadFullscreen "Rozpocznij na pe³nym ekranie"
#define MSGTR_PREFERENCES_SaveWinPos "Zapisz po³o¿enei okna"
#define MSGTR_PREFERENCES_XSCREENSAVER "Zatrzymaj wygaszacz ekranu"
#define MSGTR_PREFERENCES_PlayBar "Enable playbar"	/* XXX */
#define MSGTR_PREFERENCES_AutoSync "Autosynchronizacja w³±cz/wy³±cz"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosynchronizacja: "
#define MSGTR_PREFERENCES_CDROMDevice "Urz±dzenie CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Urz±dzenie DVD:"
#define MSGTR_PREFERENCES_FPS "FPS:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Poka¿ okno video gdy nieaktywne"

#define MSGTR_ABOUT_UHU "Rozwijanie GUI sponsorowane przez UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   G³ówni cz³onkowie zespo³u MPlayera:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Dodatkowki koderzy:\n"
#define MSGTR_ABOUT_MainTesters "   G³ówni testerzy:\n"
				  
// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "B³±d krytyczny!"
#define MSGTR_MSGBOX_LABEL_Error "B³±d!"
#define MSGTR_MSGBOX_LABEL_Warning "Ostrze¿enie!" 

#endif
