// Translated by:  Kuba "Qba" Misiorny <jim85@wp.pl>
// Wszelkie uwagi i poprawki mile widziane :)
//
// Synced with help_mp-en.h 1.140

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"U¿ycie:   mplayer [opcje] [url|¶cie¿ka/]nazwa_pliku\n"
"\n"
"Podstawowe opcje: (Pe³na lista w man)\n"
" -vo <drv[:dev]>  wybierz wyj¶ciowy sterownik video [:urz±dzenie (device)] (lista: '-vo help')\n"
" -ao <drv[:dev]>  wybierz wyj¶ciowy sterownik audio [:urz±dzenie (device)] (lista: '-ao help')\n"
#ifdef HAVE_VCD
" vcd://<numer_¶cie¿ki>  odtwórz ¶cie¿kê (S)VCD (Super Video CD) (bezpo¶rednio z napêdu, bez montowania)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<tytu³>    odtwórz tytu³ bezpo¶rednio z p³yty DVD \n"
" -alang/-slang    wybierz jêzyk ¶cie¿ki d¼wiêkowej/napisów (dwuznakowy kod kraju)\n"
#endif
" -ss <pozycja>    skok do pozycji (sekundy albo hh:mm:ss)\n"
" -nosound         nie odtwarzaj d¼wiêku\n"
" -fs              odtwarzaj na pe³nym ekranie (-vm, -zoom, szczegó³y w man)\n"
" -x <x> -y <y>    ustaw rozmiar obrazu wyj¶ciowego (u¿ywaj z -vm, -zoom)\n"
" -sub <plik>      wybierz plik z napisami (patrz tak¿e -subfps, -subdelay)\n"
" -playlist <plik> wybierz listê odtwarzania \n"
" -vid x -aid y    wybierz strumieñ video (x) lub audio (y)\n"
" -fps x -srate y  zmieñ prêdko¶æ odtwarzania video (x fps) i audio (y Hz) \n"
" -pp <jako¶æ>     w³±cz filtr postprocessingu (szczegó³y w man)\n"
" -framedrop       w³±cz gubienie ramek (dla wolnych maszyn)\n"
"\n"
"Podstawowe klawisze: (Pe³na lista na stronie man, sprawd¼ te¿ input.conf)\n"
" <-   lub  ->      skok w ty³/przód o 10 sekund\n"
" góra lub dó³      skok w ty³/przód o 1 minutê\n"
" pgup lub pgdown   skok w ty³/przód o 10 minut\n"
" < lub >           poprzednia/nastêpna pozycja na li¶cie odtwarzania\n"
" p lub SPACE       pauza (dowolny klawisz aby kontynuowaæ)\n"
" q lub ESC         wyj¶cie\n"
" + lub -           zmieñ opó¼nienie d¼wiêku o +/- 0.1 sekundy\n"
" o                 tryb OSD (On Screen Display): brak / belka / belka + timer\n"
" * lub /           zwiêksz/zmniejsz g³o¶no¶æ (PCM)\n"
" z lub x           zmieñ opó¼nienie napisów o +/- 0.1 sekundy\n"
" r lub t           zmieñ po³o¿enie napisów wy¿ej/ni¿ej, spróbuj te¿ -vf expand\n"
"\n"
" * * * DOK£ADNY SPIS WSZYSTKICH OPCJI ZNAJDUJE SIÊ NA STRONIE MAN * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// MPlayer.c:

#define MSGTR_Exiting "\nWychodzê...\n"
#define MSGTR_ExitingHow "\nWychodzê...(%s)\n"
#define MSGTR_Exit_quit "Wyj¶cie"
#define MSGTR_Exit_eof "Koniec pliku"
#define MSGTR_Exit_error "B³±d Krytyczny"
#define MSGTR_IntBySignal "\nMPlayer przerwany sygna³em %d w module: %s\n"
#define MSGTR_NoHomeDir "Nie mogê znale¼æ katalogu domowego\n"
#define MSGTR_GetpathProblem "Problem z get_path (\"config\")\n"
#define MSGTR_CreatingCfgFile "Tworzê plik konfiguracyjny: %s\n"
#define MSGTR_InvalidAOdriver "Nieprawid³owa nazwa wyj¶ciowego sterownika audio -> %s\n(lista: '-ao help').\n"
#define MSGTR_CopyCodecsConf "(Skopiuj etc/codecs.conf ze ¼róde³ MPlayera do ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "U¿ywam wbudowanego (domy¶lnego) pliku codecs.conf.\n"
#define MSGTR_CantLoadFont "Nie mogê za³adowaæ czcionki: %s\n"
#define MSGTR_CantLoadSub "Nie mogê za³adowaæ napisów: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: B£¡D KRYTYCZNY: Brak wybranego strumienia\n"
#define MSGTR_CantOpenDumpfile "Nie mogê otworzyæ pliku dump.\n"
#define MSGTR_CoreDumped "Core dumped (Zrzut pamiêci)\n"
#define MSGTR_FPSnotspecified "Warto¶æ FPS nie podana (lub b³êdna) w nag³ówku, u¿yj opcji -fps <ilo¶æ_ramek_na_sekundê>.\n"
#define MSGTR_TryForceAudioFmtStr "Wymuszam zastosowanie kodeka audio z rodziny %s...\n"
#define MSGTR_CantFindAudioCodec "Nie mogê znale¼æ kodeka dla formatu audio 0x%X.\n"
#define MSGTR_RTFMCodecs "Przeczytaj DOCS/HTML/pl/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Wymuszam zastosowanie kodeka video z rodziny %s...\n"
#define MSGTR_CantFindVideoCodec "Nie mogê znale¼æ kodeka pasuj±cego do wybranego -vo i formatu video 0x%X.\n"
#define MSGTR_CannotInitVO "B£¡D KRYTYCZNY: Nie mogê zainicjalizowaæ sterownika video.\n"
#define MSGTR_CannotInitAO "Nie mogê otworzyæ/zainicjalizowaæ urz±dzenia audio -> brak d¼wiêku.\n"
#define MSGTR_StartPlaying "Zaczynam odtwarzanie... \n"

#define MSGTR_SystemTooSlow "\n\n"\
"           ************************************************\n"\
"           ********* Twój system jest ZA WOLNY!!! ********\n"\
"           ************************************************\n\n"\
"Prawdopodobne przyczyny, rozwi±zania:\n"\
"- Najbardziej powszechne: wadliwe/b³êdne _sterowniki_audio_\n"\
"  - Spróbuj u¿yæ -ao sdl, u¿yj ALSA 0.5 lub emulacji OSS w ALSA 0.9\n"\
"  - Poeksperymentuj z ró¿nymi warto¶ciami -autosync, \"30\" na dobry pocz±tek.\n"\
"- Za wolny sterownik wyj¶ciowy:\n"\
"  - Spróbuj innego sterownika -vo (lista: -vo help) albo -framedrop!\n"\
"- Za wolny procesor\n"\
"  - Nie próbuj odtwarzaæ du¿ych DVD/DivXów na wolnym procesorze! Spróbuj -hardframedrop.\n"\
"- Zepsuty plik\n"\
"  - Spróbuj ró¿nych kombinacji -nobps, -ni, forceidx, -mc 0.\n"\
"- Za wolne ¼ród³o (zamontowane NFS/SMB, DVD, VCD itd.)\n"\
"  - Spróbuj: -cache 8192.\n"\
"- Czy u¿ywasz pamiêci podrêcznej do odtwarzania plików bez przeplotu? Spróbuj -nocache\n"\
"Przeczytaj DOCS/HTML/pl/video.html gdzie znajdziesz wskazówki\n"\
"jak przy¶pieszyæ dzia³anie MPlayera\n"\
"Je¶li nic nie pomaga przeczytaj DOCS/HTML/pl/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer zosta³ skompilowany BEZ obs³ugi GUI.\n"
#define MSGTR_GuiNeedsX "GUI MPlayera potrzebuje X11.\n"
#define MSGTR_Playing "Odtwarzam %s.\n"
#define MSGTR_NoSound "Audio: brak d¼wiêku\n"
#define MSGTR_FPSforced "Warto¶æ FPS wymuszona na %5.3f  (ftime: %5.3f).\n"
#define MSGTR_CompiledWithRuntimeDetection "Skompilowany z wykrywaniem procesora podczas pracy - UWAGA - W ten sposób nie uzyskasz\n najlepszej wydajno¶ci, przekompiluj MPlayera z opcj± --disable-runtime-cpudetection.\n"
#define MSGTR_CompiledWithCPUExtensions "Skompilowany dla procesora z rozszerzeniami:"
#define MSGTR_AvailableVideoOutputDrivers "Dostêpne sterowniki video:\n"
#define MSGTR_AvailableAudioOutputDrivers "Dostêpne sterowniki audio:\n"
#define MSGTR_AvailableAudioCodecs "Dostêpne kodeki audio:\n"
#define MSGTR_AvailableVideoCodecs "Dostêpne kodeki video:\n"
#define MSGTR_AvailableAudioFm "Dostêpne (wkompilowane) rodziny kodeków/sterowników audio:\n"
#define MSGTR_AvailableVideoFm "Dostêpne (wkompilowane) rodziny kodeków/sterowników video:\n"
#define MSGTR_AvailableFsType "Dostêpne tryby pe³noekranowe:\n"
#define MSGTR_UsingRTCTiming "U¿ywam sprzêtowego zegara czasu rzeczywistego (Linux RTC) (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Video: nie mogê odczytaæ w³a¶ciwo¶ci.\n"
#define MSGTR_NoStreamFound "Nie znalaz³em ¿adnego strumienia\n"
#define MSGTR_ErrorInitializingVODevice "B³±d przy otwieraniu/inicjalizacji wybranego urz±dzenia video (-vo).\n"
#define MSGTR_ForcedVideoCodec "Wymuszony kodek video: %s\n"
#define MSGTR_ForcedAudioCodec "Wymuszony kodek audio: %s\n"
#define MSGTR_Video_NoVideo "Video: brak video\n"
#define MSGTR_NotInitializeVOPorVO "\nB£¡D KRYTYCZNY: Nie mogê zainicjalizowaæ filtra video (-vf) lub wyj¶cia video (-vo).\n"
#define MSGTR_Paused "\n  =====  PAUZA  =====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nNie mogê za³adowaæ listy odtwarzania %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer zakoñczy³ pracê z powodu b³êdu 'Nieprawid³owa operacja'\n"\
"  Mo¿e to byæ b³±d w naszym nowym kodzie wykrywaj±cym procesor\n"\
"  Przeczytaj proszê DOCS/HTML/pl/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer zakoñczy³ pracê z powodu b³êdu  'Nieprawid³owa operacja'\n"\
"  Zdarza siê to najczê¶ciej, gdy uruchamiasz MPlayera na innym procesorze ni¿ ten\n"\
"  dla którego by³ on skompilowany/zoptymalizowany.\n"\
"  Sprawd¼ to!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer nieoczekiwanie zakoñczy³ pracê przez z³y u¿ytek procesora/pamiêci/kooprocesora\n"\
"  Przekompiluj MPlayera z opcj± '--enable-debug' i wykonaj 'gdb' backtrace \n"\
"  i zdeassembluj. Szczegó³y w DOCS/HTML/pl/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer nieoczekiwanie zakoñczy³ pracê. To nie powinno siê zdarzyæ! :).\n"\
"  Mo¿e to byæ b³±d w kodzie MPlayera albo w Twoim sterowniku,\n"\
"  lub z³ej wersji gcc. Je¶li uwa¿asz, ¿e to wina MPlayera, przeczytaj\n"\
"  DOCS/HTML/pl/bugreports.html. Nie mo¿emy i nie pomo¿emy, je¶li nie przedstawisz tych informacji\n"\
"  zg³aszaj±c mo¿liwy b³±d.\n"

#define MSGTR_EdlCantUseBothModes "Nie mo¿na uzywaæ -edl i -edlout w tym samym czasie.\n"
#define MSGTR_EdlOutOfMem "Nie mogê zaalokowaæ wystarczaj±co du¿o pamiêci dla danych EDL.\n"
#define MSGTR_EdlRecordsNo "Odczyta³em %d akcji EDL.\n"
#define MSGTR_EdlQueueEmpty "Nie ma ¿adnych akcji EDL do wykonania.\n"
#define MSGTR_EdlCantOpenForWrite "Nie mogê otworzyæ pliku EDL [%s] do zapisu.\n"
#define MSGTR_EdlCantOpenForRead "Nie mogê otworzyæ pliku EDL [%s] do odczytu.\n" 
#define MSGTR_EdlNOsh_video "Nie mo¿na u¿ywaæ EDL bez strumienia video, wy³±czam.\n"
#define MSGTR_EdlNOValidLine "Nieprawid³owa komenda EDL: %s\n"
#define MSGTR_EdlBadlyFormattedLine "¬le sformatowana komenda [%d], odrzucam.\n"
#define MSGTR_EdlBadLineOverlap "Ostatnia pozycja stopu [%f]; nastêpny start to "\
"[%f]. Wpisy musz± byæ w kolejno¶ci chronologicznej, nie mo¿na przeskakiwaæ. Odrzucam.\n"
#define MSGTR_EdlBadLineBadStop "Czas stopu musi siê znale¼æ za ustawionym czasem startu.\n"
 
// mencoder.c:

#define MSGTR_UsingPass3ControllFile "U¿ywam pliku kontrolnego pass3: %s\n" 
#define MSGTR_MissingFilename "\nBrak nazwy pliku.\n\n"
#define MSGTR_CannotOpenFile_Device "Nie mogê otworzyæ pliku/urz±dzenia\n"
#define MSGTR_CannotOpenDemuxer "Nie mogê otworzyæ demuxera.\n"
#define MSGTR_NoAudioEncoderSelected "\nNie wybrano kodera audio (-oac). Wybierz jaki¶ (Lista: -oac help) albo u¿yj opcji '-nosound' \n"
#define MSGTR_NoVideoEncoderSelected "\nNie wybrano kodera video (-ovc). Wybierz jaki¶ (Lista: -ovc help)\n"
#define MSGTR_CannotOpenOutputFile "Nie mogê otworzyæ pliku wyj¶ciowego '%s'.\n"
#define MSGTR_EncoderOpenFailed "Nie mogê otworzyæ kodera.\n"
#define MSGTR_ForcingOutputFourcc "Wymuszam wyj¶ciowe fourcc na %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "Zapisujê nag³ówek AVI...\n"
#define MSGTR_DuplicateFrames "\n%d powtórzona(e) ramka(i)!\n"
#define MSGTR_SkipFrame "\nOpuszczam ramkê!"
#define MSGTR_ErrorWritingFile "%s B³±d przy zapisie pliku.\n"
#define MSGTR_WritingAVIIndex "\nZapisujê indeks AVI...\n"
#define MSGTR_FixupAVIHeader "Naprawiam nag³ówek AVI...\n"
#define MSGTR_RecommendedVideoBitrate "Zalecany video bitrate dla tego %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nStrumieñ video: %8.3f kbit/s (%d bps) rozmiar: %d bajtów %5.3f s %d ramek\n"
#define MSGTR_AudioStreamResult "\nStrumieñ audio: %8.3f kbit/s (%d bps) rozmiar: %d bajtów %5.3f s\n"

#define MSGTR_OpenedStream "sukces: format: %d  dane: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videocodec: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "audiocodec: framecopy (format=%x chans=%d rate=%ld bits=%d bps=%ld sample-%ld)\n"
#define MSGTR_CBRPCMAudioSelected "Wybrano d¼wiêk CBR PCM\n"
#define MSGTR_MP3AudioSelected "Wybrano d¼wiêk MP3\n"
#define MSGTR_CannotAllocateBytes "Nie mo¿na by³o zaalokowaæ %d bajtów\n"
#define MSGTR_SettingAudioDelay "Ustawiam OPÓ¬NIENIE D¬WIÊKU na %5.3f\n"
#define MSGTR_SettingAudioInputGain "Ustawiam podbicie wej¶cia d¼wiêku na %f\n"
#define MSGTR_LamePresetEquals "\nustawienie=%s\n\n"
#define MSGTR_LimitingAudioPreload "Ograniczam buforowanie d¼wiêku do 0.4s\n"
#define MSGTR_IncreasingAudioDensity "Zwiêkszam gêsto¶æ d¼wiêku do 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Wymuszam buforowanie d¼wiêku na 0, maksymaln± korekcjê pts na 0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR audio: %ld bajtów/sek, %d bajtów/blok\n"
#define MSGTR_LameVersion "Wersja LAME %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "B³±d: Wybrany bitrate jest poza prawid³owym zasiegiem tego ustawienia\n"\
"\n"\
"Podczas u¿ywania tego trybu musisz wpisaæ warto¶æ pomiêdzy \"8\" i \"320\"\n"\
"\n"\
"Dalsze informacje: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "B³±d: Nie wprowadzi³e¶ odpowiedniego profilu lub/i opcji dla tego ustawienia\n"\
"\n"\
"Dostêpne profile:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                      insane\n"\
"   <cbr> (ABR Mode) - Tryb ABR jest domy¶lny. Aby go u¿yæ,\n"\
"                      podaj po prostu bitrate. Na przyk³ad:\n"\
"                      \"preset=185\" aktywuje to ustawienie\n"\
"                      i u¿ywa 185 jako ¶rednie kbps.\n"\
"\n"\
"    Kilka przyk³adów:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" lub \"-lameopts  cbr:preset=192       \"\n"\
" lub \"-lameopts      preset=172       \"\n"\
" lub \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Dalsze informacje: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Zestawy ustawieñ zaprojektowane s± w celu uzysakania jak najwy¿szej jako¶ci.\n"\
"\n"\
"Byly poddawane i dopracowywane przez rygorystyczne testy\n"\
"odsluchowe, aby osiagnac ten cel.\n"\
"\n"\
"S± one bez przerwy aktualizowane, aby nad±¿yæ za naj¶wie¿szymi nowinkami\n"\
"co powinno przynosiæ prawie najwy¿sz± osi±galn± w LAME jako¶æ.\n"\
"\n"\
"Aby aktywowaæ te ustawienia:\n"\
"\n"\
"   Dla trybów VBR (zazwyczaj najlepsza jako¶æ):\n"\
"\n"\
"     \"preset=standard\" To ustawienie powinno byæ prze¼roczyste\n"\
"                             dla wiêkszo¶ci ludzi przy odtwarzaniu muzyki i odrazu\n"\
"                             jest w niez³ej jako¶ci.\n"\
"\n"\
"     \"preset=extreme\" Je¶li masz bardzo dobry s³uch i równie dobry sprzêt,\n"\
"                             to ustawienie, daje trochê lepsz± jako¶ ni¿ \n"\
"                             tryb \"standard\".\n"\
"\n"\
"   Dla trybu CBR 320kbps (najwy¿sza mo¿liwa jako¶æ ze wszystkich mo¿liwych ustawieñ):\n"\
"\n"\
"     \"preset=insane\"  To ustawienie bêdzie przesad±\n"\
"                             dla wiêkszo¶ci ludzi w wiêkszo¶ci przypadków,\n"\
"                             ale je¿eli musisz mieæ najwy¿sz± jako¶ niezale¿nie\n"\
"                             od wielko¶ci pliku, to jest w³a¶ciwa droga.\n"\
"\n"\
"   Dla trybów ABR (wysoka jako¶æ z ustalonym bitratem, ale tak wysoka jak VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Okre¶lenie tego parametru da Tobie dobr± jako¶æ\n"\
"                             przy ustalonym bitrate'cie. pieraj±c siê na niej,\n"\
"                             okre¶li ono optymalne ustawienia dla danej sytuacji.\n"\
"			      Niestety nie jest ono tak elastyczne jak VBR i przewa¿nie nie\n"\
"                            zapewni takiego samego poziomu jako¶ci jak VBR\n"\
"                            dla wy¿szych warto¶ci bitrate.\n"\
"\n"\
"Poni¿sze opcje s± równie¿ dostêpne dla odpowiadaj±cych profili:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - Tryb ABR jest domy¶lny. Aby go u¿yæ,\n"\
"                      podaj po prostu bitrate. Na przyk³ad:\n"\
"                      \"preset=185\" aktywuje to ustawienie\n"\
"                      i u¿ywa 185 jako ¶rednie kbps.\n"\
"\n"\
"   \"fast\" - Uruchamia nowe szybkie VBR dla danego profilu. Wad± \n"\
"            w stosunku do ustawienia szybko¶ci jest to, i¿ czêsto bitrate jest\n"\
"            troszkê wy¿szy ni¿ przy normalnym trybie, a jako¶æ \n"\
"            mo¿e byæ troche ni¿sza.\n"\
"   Uwaga: obecna wersja ustawienia \"fast\" mo¿e skutkowaæ wyskomi warto¶ciami\n"\
"            bitrate w stosunku do tego z normalnego ustawienia.\n"\
"\n"\
"   \"cbr\"  - Je¿eli u¿ywasz trybu ABR (przeczytaj powy¿ej) ze znacznym bitratem\n"\
"            jak 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            mo¿esz u¿yæ opcji  \"cbr\", aby wymusiæ enkodowanie w trybie CBR\n"\
"            zamiast standardowego trybu abr. ABR daje wy¿sz± jako¶æ, ale CBR\n"\
"            mo¿e siê przydaæ w sytuacjach, gdy strumieniowanie mp3 przez\n"\
"            Internet jest wa¿ne\n"\
"\n"\
"    Na przyk³ad:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Dostêpnych jest kilka synonimów dla trybu ABR:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_ConfigfileError "b³±ð pliku konfiguracyjnego"
#define MSGTR_ErrorParsingCommandLine "b³±ð przy przetwarzaniu lini komend"
#define MSGTR_VideoStreamRequired "Strumieñ video jest wymagany!\n"
#define MSGTR_ForcingInputFPS "wej¶ciowa warto¶æ fps bêdzie zinterpretowana jako %5.2f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Format wyj¶ciowy RAWVIDEO nie wspiera audio - wy³±czam audio\n"
#define MSGTR_DemuxerDoesntSupportNosound "Ten demuxer jeszcze nie wspiera -nosound.\n"
#define MSGTR_MemAllocFailed "nie uda³o sie zaalokowaæ pamiêci"
#define MSGTR_NoMatchingFilter "Nie mo¿na znale¼æ pasuj±cego formatu filtra/ao!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, zepsuty kompilator C?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC - brakuj±ca nazwa kodeka!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC - nie mogê znale¼æ kodeka dla %s\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC - nie mogê zaalokowaæ tre¶ci!\n"
#define MSGTR_CouldntOpenCodec "Nie mogê otworzyæ kodeka %s, br=%d\n"
// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metoda zmiennego bitrate\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(default)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           ¶redni bitrate\n"\
"\n"\
" cbr           sta³y bitrate\n"\
"               Wymusza tak¿e kodowanie CBR (sta³y bitrate) w nastêpuj±cych po tej opcji ustawieniach ABR\n"\
"\n"\
" br=<0-1024>   podaj bitrate w kBit (tylko CBR i ABR)\n"\
"\n"\
" q=<0-9>       jako¶æ (0-najwy¿sza, 9-najni¿sza) (tylko VBR)\n"\
"\n"\
" aq=<0-9>      jako¶æ algorytmu (0-najlepsza/najwolniejszy, 9-najgorsza/najszybszy)\n"\
"\n"\
" ratio=<1-100> wspó³czynnik kompresji\n"\
"\n"\
" vol=<0-10>    ustaw wzmocnienie sygna³u audio\n"\
"\n"\
" mode=<0-3>    (domy¶lnie: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: nie\n"\
"                1: wszystkie\n"\
"                2: ustaw\n"\
"\n"\
" fast          Prze³±cz na szybsze kodowanie na poni¿szych ustawieniach VBR,\n"\
"		nieznacznie ni¿sza jako¶æ i wy¿szy bitrate.\n"\
"\n"\
" preset=<value>  W³±cza najwy¿sz± mo¿liw± jako¶æ.\n"\
"                 medium: kodowanie VBR, dobra jako¶æ\n"\
"                 (bitrate: 150-180 kb/s)\n"\
"                 standard:  kodowanie VBR, wysoka jako¶æ\n"\
"                 (bitrate: 170-210 kb/s)\n"\
"                 extreme: kodowanie VBR, bardzo wysoka jako¶æ\n"\
"                 (bitrate: 200-240 kb/s)\n"\
"                 insane:  kodowanie CBR, najwy¿sza jako¶æ\n"\
"                 (bitrate: 320 kb/s)\n"\
"                 <8-320>: kodowanie ABR przy podanym ¶rednim bitrate.\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Nie znaleziono urz±dzenia CD-ROM '%s'\n"
#define MSGTR_ErrTrackSelect "B³±d przy wybieraniu ¶cie¿ki VCD."
#define MSGTR_ReadSTDIN "Czytam ze stdin (standardowego wej¶cia)...\n"
#define MSGTR_UnableOpenURL "Nie mogê otworzyæ URL: %s\n"
#define MSGTR_ConnToServer "Po³±czy³em siê z serwerem: %s\n"
#define MSGTR_FileNotFound "Nie znaleziono pliku '%s'\n"

#define MSGTR_SMBInitError "Nie mogê zainicjalizowaæ biblioteki libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Nie mogê odczytaæ z LANu: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer nie zosta³ skompilowany z obs³ug± SMB.\n"

#define MSGTR_CantOpenDVD "Nie mogê otworzyæ DVD: %s\n"
#define MSGTR_DVDwait "Odczytujê strukturê dysku, proszê czekaæ...\n"
#define MSGTR_DVDnumTitles "Na tym DVD jest %d tytu³ów.\n"
#define MSGTR_DVDinvalidTitle "Nieprawid³owy numer tytu³u: %d\n"
#define MSGTR_DVDnumChapters "W tym tytule DVD jest %d rozdzia³ów.\n"
#define MSGTR_DVDinvalidChapter "Nieprawid³owy numer rozdzia³u (DVD): %d\n"
#define MSGTR_DVDnumAngles "W tym tytule DVD znajduje siê %d ustawieñ (k±tów) kamery.\n"
#define MSGTR_DVDinvalidAngle "Nieprawid³owy numer ustawienia kamery: %d\n"
#define MSGTR_DVDnoIFO "Nie mogê otworzyæ pliku IFO dla tytu³u DVD %d.\n"
#define MSGTR_DVDnoVOBs "Nie mogê otworzyæ tytu³u VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD otwarte prawid³owo.\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "UWAGA: Redefiniowano nag³ówek strumienia audio %d!\n"
#define MSGTR_VideoStreamRedefined "UWAGA: Redefiniowano nag³ówek strumienia video %d!\n"
#define MSGTR_TooManyAudioInBuffer "\nZa du¿o pakietów audio w buforze (%d w %d bajtach)\n"
#define MSGTR_TooManyVideoInBuffer "\nZa du¿o pakietów video w buforze (%d w %d bajtach)\n"

#define MSGTR_MaybeNI "Mo¿e odtwarzasz plik/strumieñ bez przeplotu (non-interleaved) albo kodek nie zadzia³a³?\n"\
		      "Dla plików AVI spróbuj wymusiæ tryb bez przeplotu z opcj± '-ni'\n"
#define MSGTR_SwitchToNi "\nWykryto zbiór AVI z b³êdnym przeplotem - prze³±czam w tryb -ni\n"
#define MSGTR_Detected_XXX_FileFormat "Wykryto format %s.\n"
#define MSGTR_DetectedAudiofile "Wykryto plik audio.\n"
#define MSGTR_NotSystemStream "Nie jest to format MPEG System Stream... (mo¿e Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Nieprawid³owy strumieñ MPEG-ES??? Skontaktuj siê z autorem to mo¿e byæ b³±d :(\n"
#define MSGTR_FormatNotRecognized "============ Przykro mi, nierozpoznany/nieobs³ugiwany format pliku =============\n"\
				  "=== Je¶li jest to strumieñ AVI, ASF albo MPEG skontaktuj siê z autorem! ===\n"
#define MSGTR_MissingVideoStream "Nie znaleziono strumienia video.\n"
#define MSGTR_MissingAudioStream "Nie znaleziono strumienia audio -> brak d¼wiêku.\n"
#define MSGTR_MissingVideoStreamBug "Brakuje strumienia video!? Skontaktuj siê z autorem, to mo¿e byæ b³±d:(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Plik nie zawiera wybranego strumienia audio i video.\n"

#define MSGTR_NI_Forced "Wymuszony"
#define MSGTR_NI_Detected "Wykryty"
#define MSGTR_NI_Message "%s plik formatu NON-INTERLEAVED AVI (bez przeplotu).\n"

#define MSGTR_UsingNINI "U¿ywam uszkodzonego formatu pliku NON-INTERLAVED AVI.\n"
#define MSGTR_CouldntDetFNo "Nie mogê okre¶liæ liczby ramek (dla przeszukiwania bezwzglêdnego).\n"
#define MSGTR_CantSeekRawAVI "Nie mogê przeszukiwaæ nieindeksowanych strumieni AVI (Index jest wymagany, spróbuj z opcja '-idx')\n"
#define MSGTR_CantSeekFile "Nie mogê przeszukiwaæ tego pliku\n"

#define MSGTR_EncryptedVOB "Zaszyfrowany plik VOB! Przeczytaj DOCS/HTML/pl/dvd.html.\n"

#define MSGTR_MOVcomprhdr "MOV: Obs³uga skompresowanych nag³ówków wymaga ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: UWAGA: Zmienna FOURCC wykryta!?\n"
#define MSGTR_MOVtooManyTrk "MOV: UWAGA: za du¿o ¶cie¿ek"
#define MSGTR_FoundAudioStream "==> Wykryto strumieñ audio: %d\n"
#define MSGTR_FoundVideoStream "==> Wykryto strumieñ video: %d\n"
#define MSGTR_DetectedTV "Wykryto TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Nie mogê otworzyæ demuxera ogg.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Szukam strumienia audio (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Nie mogê otworzyæ strumienia audio %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nie mogê otworzyæ strumienia z napisami: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Nie mogê otworzyæ demuxera audio: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Nie mogê otworzyæ demuxera napisów: %s\n"
#define MSGTR_TVInputNotSeekable "Wej¶cia TV nie mo¿na przeszukiwaæ (Przeszukiwanie bêdzie s³u¿y³o do zmiany kana³ów ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Informacje %s o demuxerze s± ju¿ obecne!\n"
#define MSGTR_ClipInfo "Informacje o klipie:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: Wykryto zawarto¶æ 30fps NTSC, zmieniam liczbê ramek na sekundê.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: Wykryto progresywn± zawarto¶æ 24fps NTSC, zmieniam liczbê ramek na sekundê"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nie mogê otworzyæ kodeka.\n"
#define MSGTR_CantCloseCodec "Nie mogê zamkn±æ kodeka.\n"

#define MSGTR_MissingDLLcodec "B³±d: Nie mogê otworzyæ wymaganego kodeka DirectShow %s.\n"
#define MSGTR_ACMiniterror "Nie mogê za³adowaæ/zainicjalizowaæ kodeka Win32/ACM AUDIO (Brakuje pliku DLL?).\n"
#define MSGTR_MissingLAVCcodec "Nie mogê znale¼æ kodeka '%s' w libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: B£¡D KRYTYCZNY: EOF (koniec pliku) podczas szukania nag³ówka sekwencji.\n"
#define MSGTR_CannotReadMpegSequHdr "B£¡D KRYTYCZNY: Nie mogê odczytaæ nag³ówka sekwencji.\n"
#define MSGTR_CannotReadMpegSequHdrEx "B£¡D KRYTYCZNY: Nie mogê odczytaæ rozszerzenia nag³ówka sekwencji.\n"
#define MSGTR_BadMpegSequHdr "MPEG: Nieprawid³owy nag³ówek sekwencji\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Nieprawid³owe rozszerzenie nag³ówka sekwencji\n"

#define MSGTR_ShMemAllocFail "Nie mogê zaalokowaæ pamiêci wspó³dzielonej.\n"
#define MSGTR_CantAllocAudioBuf "Nie mogê zaalokowaæ bufora wyj¶ciowego audio.\n"

#define MSGTR_UnknownAudio "Nieznany/brakuj±cy format audio -> brak d¼wiêku\n"

#define MSGTR_UsingExternalPP "[PP] U¿ywam zewnêtrznego filtra postprocessingu, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] U¿ywam postprocessingu kodeka, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Atrybut video '%s' nie jest obs³ugiwany przez wybrane vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Wybrana rodzina kodeków video [%s](vfm=%s) jest niedostêpna.\nW³±cz j± przy kompilacji.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Wybrana rodzina kodeków audio [%s](vfm=%s) jest niedostêpna.\nW³±cz j± przy kompilacji.\n"
#define MSGTR_OpeningVideoDecoder "Otwieram dekoder video: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Otwieram dekoder audio: [%s] %s\n"
#define MSGTR_UninitVideoStr "deinicjalizacja video: %s\n"
#define MSGTR_UninitAudioStr "deinicjalizacja audio: %s\n"
#define MSGTR_VDecoderInitFailed "Inicjalizacja VDecodera nie powiod³a siê :(\n"
#define MSGTR_ADecoderInitFailed "Inicjalizacja ADecodera nie powiod³a siê :(\n"
#define MSGTR_ADecoderPreinitFailed "Nieudana preinicjalizacja ADecodera :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alokujê %d bajtów dla bufora wej¶ciowego.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alokujê %d + %d = %d bajtów dla bufora wyj¶ciowego.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "W³±czam obs³ugê LIRC...\n"
#define MSGTR_LIRCdisabled "Nie bêdziesz móg³ u¿ywaæ swojego pilota.\n"
#define MSGTR_LIRCopenfailed "Nie mogê uruchomiæ obs³ugi LIRC.\n"
#define MSGTR_LIRCcfgerr "Nie mogê odczytaæ pliku konfiguracyjnego LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nie mogê znale¼æ filtra video '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "Nie mogê otworzyæ filtra video '%s'.\n"
#define MSGTR_OpeningVideoFilter "Otwieram filtr video: "
#define MSGTR_CannotFindColorspace "Nie mogê znale¼æ odpowiedniej przestrzenii kolorów, nawet poprzez wstawienie 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Kodek nie ustawi³ sh->disp_w i sh->disp_h, próbujê to rozwi±zaæ.\n"
#define MSGTR_VoConfigRequest "VDec: wymagana konfiguracja vo - %d x %d (preferowana csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Nie mogê znale¼æ pasuj±cej przestrzeni koloru - próbujê ponownie z -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Proporcje filmu (obrazu) to %.2f:1 - skalujê do prawid³owych proporcji.\n"
#define MSGTR_MovieAspectUndefined "Proporcje filmu (obrazu) nie s± zdefiniowane - nie skalujê.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Musisz zainstalowaæ/zaktualizowaæ pakiet binarnych kodeków.\nId¼ do http://mplayerhq.hu/homepage/dload.html\n"
#define MSGTR_DShowInitOK "INFORMACJA: Inicjalizacja kodeka video Win32/DShow przebieg³a pomy¶lnie.\n"
#define MSGTR_DMOInitOK "INFORMACJA: Inicjalizacja kodeka video Win32/DMO przebieg³a pomy¶lnie.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Nie mog³em wys³aæ zdarzenia pe³nego ekranu EWMH!\n"

#define MSGTR_InsertingAfVolume "[Mixer] Nie ma sprzêtowego miksowania, w³±czam filtr g³o¶no¶ci.\n"
#define MSGTR_NoVolume "[Mixer] Regulacja g³o¶no¶ci niedostêpna.\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O programie"
#define MSGTR_FileSelect "Wybierz plik..."
#define MSGTR_SubtitleSelect "Wybierz napisy..."
#define MSGTR_OtherSelect "Wybierz..."
#define MSGTR_AudioFileSelect "Wybierz zewnêtrzny kana³ audio..."
#define MSGTR_FontSelect "Wybierz czcionkê..."
#define MSGTR_PlayList "Lista Odtwarzania"
#define MSGTR_Equalizer "Equalizer (korektor)"
#define MSGTR_SkinBrowser "Przegl±darka skórek"
#define MSGTR_Network "Strumieniowanie sieciowe..."
#define MSGTR_Preferences "Preferencje"
#define MSGTR_AudioPreferences "Konfiguracja sterownika audio"
#define MSGTR_NoMediaOpened "Nie otwarto ¿adnego no¶nika."
#define MSGTR_VCDTrack "¶cie¿ka VCD: %d"
#define MSGTR_NoChapter "Brak rozdzia³u"
#define MSGTR_Chapter "Rozdzia³ %d"
#define MSGTR_NoFileLoaded "Nie za³adowano ¿adnego pliku."

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
#define MSGTR_NEMFMR "Przykro mi, za ma³o pamiêci do wyrenderowania menu."
#define MSGTR_IDFGCVD "Przykro mi, nie znalaz³em kompatybilnego z GUI sterownika video."
#define MSGTR_NEEDLAVCFAME "Przykro mi, nie mo¿esz odtwarzaæ plików innych ni¿ MPEG za pomoc± urz±dzenia DXR3/H+ bez przekodowania.\nW³±cz lavc albo fame w konfiguracji DXR3/H+"

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] b³±d w pliku konfiguracyjnym skórki, w linii %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] ostrze¿enie w pliku konfiguracyjnym w linii %d:\nznaleziono znacznik widget (%s) ale nie ma przed nim \"section\""
#define MSGTR_SKIN_WARNING2 "[skin] ostrze¿enie w pliku konfiguracyjnym w linii %d:\nznaleziono znacznik widget (%s) ale nie ma przednim \"subsection\""
#define MSGTR_SKIN_WARNING3 "[skin] ostrze¿enie w pliku konfiguracyjnym w linii %d::\nta podsekcja nie jest obs³ugiwana przez widget (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmapy 16 bitowe lub mniejsze nie s± obs³ugiwane (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "nie znaleziono pliku (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "b³±d odczytu bmp (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "b³±d odczytu tga (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "b³±d odczytu png (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "tga kompresowane przez RLE nie obs³ugiwane (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "nieznany typ pliku (%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "b³±d przy konwersji 24 bitów na 32 bity (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "nieznany komunikat: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "za ma³o pamiêci\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "za du¿o zadeklarowanych czcionek\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "nie znaleziono pliku z czcionk±\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "nie znaleziono pliku z obrazem czcionki\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "nieistniej±cy identyfikator czcionki (%s)\n"
#define MSGTR_SKIN_UnknownParameter "nieznany parametr (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "nie znalaz³em skórki (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "B³±d odczytu pliku konfiguracyjnego skórki (%s).\n"
#define MSGTR_SKIN_LABEL "Skórki:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O MPlayerze"
#define MSGTR_MENU_Open "Otwórz..."
#define MSGTR_MENU_PlayFile "Odtwórz plik..."
#define MSGTR_MENU_PlayVCD "Odtwórz VCD..."
#define MSGTR_MENU_PlayDVD "Odtwórz DVD..."
#define MSGTR_MENU_PlayURL "Odtwórz URL..."
#define MSGTR_MENU_LoadSubtitle "Za³aduj napisy..."
#define MSGTR_MENU_DropSubtitle "Wy³±cz napisy..."
#define MSGTR_MENU_LoadExternAudioFile "Za³aduj zewnêtrzny plik audio..."
#define MSGTR_MENU_Playing "Odtwarzanie"
#define MSGTR_MENU_Play "Odtwarzaj"
#define MSGTR_MENU_Pause "Pauza"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Nastêpny strumieñ"
#define MSGTR_MENU_PrevStream "Poprzedni strumieñ"
#define MSGTR_MENU_Size "Rozmiar"
#define MSGTR_MENU_NormalSize "Normalny rozmiar"
#define MSGTR_MENU_DoubleSize "Podwójny rozmiar"
#define MSGTR_MENU_FullScreen "Pe³ny ekran"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Otwórz dysk..."
#define MSGTR_MENU_ShowDVDMenu "Poka¿ menu DVD"
#define MSGTR_MENU_Titles "Tytu³y"
#define MSGTR_MENU_Title "Tytu³ %2d"
#define MSGTR_MENU_None "(brak)"
#define MSGTR_MENU_Chapters "Rozdzia³y"
#define MSGTR_MENU_Chapter "Rozdzia³ %2d"
#define MSGTR_MENU_AudioLanguages "Jêzyki ¶cie¿ki d¼wiêkowej"
#define MSGTR_MENU_SubtitleLanguages "Jêzyki napisów"
#define MSGTR_MENU_PlayList "Lista odtwarzania"
#define MSGTR_MENU_SkinBrowser "Przegl±darka skórek"
#define MSGTR_MENU_Preferences "Preferencje"
#define MSGTR_MENU_Exit "Wyj¶cie..."
#define MSGTR_MENU_Mute "Wyciszenie"
#define MSGTR_MENU_Original "Oryginalny"
#define MSGTR_MENU_AspectRatio "Proporcje obrazu"
#define MSGTR_MENU_AudioTrack "¦cie¿ka audio"
#define MSGTR_MENU_Track "¦cie¿ka %d"
#define MSGTR_MENU_VideoTrack "¦cie¿ka video"

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Jasno¶æ: "
#define MSGTR_EQU_Hue "Barwa: "
#define MSGTR_EQU_Saturation "Nasycenie: "
#define MSGTR_EQU_Front_Left "Lewy przedni"
#define MSGTR_EQU_Front_Right "Prawy przedni"
#define MSGTR_EQU_Back_Left "Lewy tylny"
#define MSGTR_EQU_Back_Right "Prawy tylny"
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
#define MSGTR_PLAYLIST_DirectoryTree "Drzewo katalogu"

// --- preferences
#define MSGTR_PREFERENCES_Audio "Audio"
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_SubtitleOSD "Napisy i OSD"
#define MSGTR_PREFERENCES_Codecs "kodeki i demuxer"
#define MSGTR_PREFERENCES_Misc "Inne"

#define MSGTR_PREFERENCES_None "Brak"
#define MSGTR_PREFERENCES_DriverDefault "domy¶lne ustawienia sterownika"
#define MSGTR_PREFERENCES_AvailableDrivers "Dostêpne sterowniki:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nie odtwarzaj d¼wiêku"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizuj d¼wiêk"
#define MSGTR_PREFERENCES_EnEqualizer "W³±cz equalizer (korektor)"
#define MSGTR_PREFERENCES_ExtraStereo "W³±cz extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Wspó³czynnik:"
#define MSGTR_PREFERENCES_AudioDelay "Opó¼nienie d¼wiêku"
#define MSGTR_PREFERENCES_DoubleBuffer "W³±cz podwójne buforowanie"
#define MSGTR_PREFERENCES_DirectRender "W³±cz bezpo¶rednie renderowanie (direct rendering)"
#define MSGTR_PREFERENCES_FrameDrop "W³±cz gubienie ramek"
#define MSGTR_PREFERENCES_HFrameDrop "W³±cz gubienie du¿ej ilo¶ci ramek (niebezpieczne)"
#define MSGTR_PREFERENCES_Flip "Odwróæ obraz do góry nogami"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Timer i wska¼niki"
#define MSGTR_PREFERENCES_OSDProgress "Tylko belka"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Timer, czas w procentach i ca³kowity"
#define MSGTR_PREFERENCES_Subtitle "Napisy:"
#define MSGTR_PREFERENCES_SUB_Delay "Opó¼nienie: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozycja: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Wy³±cz automatyczne ³adowanie napisów"
#define MSGTR_PREFERENCES_SUB_Unicode "Napisy w Unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konwertuj dane napisy na format napisów MPlayera"
#define MSGTR_PREFERENCES_SUB_SRT "Konwertuj dane napisy na format SRT (bazowany na czasie SubViewer)"
#define MSGTR_PREFERENCES_SUB_Overlap "Prze³±cz nak³adanie (overlapping) napisów"
#define MSGTR_PREFERENCES_Font "Czcionka:"
#define MSGTR_PREFERENCES_FontFactor "Skala czcionki:"
#define MSGTR_PREFERENCES_PostProcess "W³±cz postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Automatyczna jako¶æ:"
#define MSGTR_PREFERENCES_NI "U¿yj parsera odpowiedniego dla AVI bez przeplotu (non-interleaved)"
#define MSGTR_PREFERENCES_IDX "Przebuduj tablicê indeksów, je¶li to potrzebne"
#define MSGTR_PREFERENCES_VideoCodecFamily "Rodzina kodeków video:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Rodzina kodeków audio:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Poziom OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Napisy"
#define MSGTR_PREFERENCES_FRAME_Font "Czcionka"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Kodeki i demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache (pamiêæ podrêczna)"
#define MSGTR_PREFERENCES_FRAME_Misc "Inne"
#define MSGTR_PREFERENCES_Audio_Device "Urz±dzenie:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mikser:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Kana³ miksera:"
#define MSGTR_PREFERENCES_Message "Pamiêtaj, ¿e niektóre opcje wymagaj± zrestartowania odtwarzania!"
#define MSGTR_PREFERENCES_DXR3_VENC "Enkoder video:"
#define MSGTR_PREFERENCES_DXR3_LAVC "U¿yj LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "U¿yj FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Zachodnioeuropejskie jêzyki (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Zachodnioeuropejskie jêzyki z Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Jêzyki s³owiañskie i ¶rodkowoeuropejskie (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galijski, Maltañski, Turecki (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Stary, ba³tycki zestaw znaków (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cyrlica (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabski (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Wspó³czesna Greka (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turecki (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Jêzyki ba³tyckie (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celtycki (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Znaki hebrajskie (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Rosyjski (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukraiñski, Bia³oruski (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Uproszczone znaki chiñskie (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Tradycyjne znaki chiñskie (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Znaki japoñskie (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Znaki koreañskie (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Znaki tajskie (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrlica Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Jêzyki s³owiañskie i ¶rodkowoeuropejskie Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Bez autoskalowania"
#define MSGTR_PREFERENCES_FontPropWidth "Proporcjonalnie do szeroko¶ci obrazu"
#define MSGTR_PREFERENCES_FontPropHeight "Proporcjonalnie do wysoko¶ci obrazu"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporcjonalnie do przek±tnej obrazu"
#define MSGTR_PREFERENCES_FontEncoding "Kodowanie:"
#define MSGTR_PREFERENCES_FontBlur "Rozmycie:"
#define MSGTR_PREFERENCES_FontOutLine "Obramowanie:"
#define MSGTR_PREFERENCES_FontTextScale "Skala tekstu:"
#define MSGTR_PREFERENCES_FontOSDScale "Skala OSD:"
#define MSGTR_PREFERENCES_Cache "Pamiêæ podrêczna"
#define MSGTR_PREFERENCES_CacheSize "Rozmiar pamiêci podrêcznej: "
#define MSGTR_PREFERENCES_LoadFullscreen "Rozpocznij w trybie pe³noekranowym"
#define MSGTR_PREFERENCES_SaveWinPos "Zapisz pozycjê okna"
#define MSGTR_PREFERENCES_XSCREENSAVER "Wy³±cz XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "W³±cz podrêczny pasek odtwarzania"
#define MSGTR_PREFERENCES_AutoSync "W³±cz/Wy³±cz autosynchronizacjê"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosynchronizacja: "
#define MSGTR_PREFERENCES_CDROMDevice "Urz±dzenie CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Urz±dzenie DVD:"
#define MSGTR_PREFERENCES_FPS "FPS:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Pokazuj okno video gdy nieaktywne"

#define MSGTR_ABOUT_UHU "Rozwój GUI sponsorowany przez UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   G³ówni cz³onkowie zespo³u MPlayera:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Dodatkowi koderzy (programi¶ci):\n"
#define MSGTR_ABOUT_MainTesters "   G³owni testerzy:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "B³±d krytyczny!"
#define MSGTR_MSGBOX_LABEL_Error "B³±d!" 
#define MSGTR_MSGBOX_LABEL_Warning "Ostrze¿enie!"

#endif


// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Wybrane urz±dzenie wyj¶cia video jest niekompatybilne z tym kodekiem.\n"
#define MSGTR_VO_GenericError "Wyst±pi³ b³±d"
#define MSGTR_VO_UnableToAccess "Brak dostêpu"
#define MSGTR_VO_ExistsButNoDirectory "ju¿ istnieje, ale nie jest katalogiem."
#define MSGTR_VO_DirExistsButNotWritable "Wyj¶ciowy katalog ju¿ istnieje, ale nie jest zapisywalny."
#define MSGTR_VO_DirExistsAndIsWritable "Wyj¶ciowy katalog ju¿ istnieje i nie jest zapisywalny."
#define MSGTR_VO_CantCreateDirectory "Nie mo¿na utworzyæ wyj¶ciowego katalogu."
#define MSGTR_VO_CantCreateFile "Nie mo¿na utworzyæ pliku wyj¶ciowego."
#define MSGTR_VO_DirectoryCreateSuccess "Katalog wyj¶ciowy stworzony."
#define MSGTR_VO_ParsingSuboptions "Interpretujê podopcje."
#define MSGTR_VO_SuboptionsParsedOK "Podopcje zinterpretowane poprawnie."
#define MSGTR_VO_ValueOutOfRange "Warto¶æ poza zakresem"
#define MSGTR_VO_NoValueSpecified "Nie podano ¿adnej warto¶ci."
#define MSGTR_VO_UnknownSuboptions "Nieznana podopcja(e)"

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "Progresywny JPEG w³±czony."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "Progresywny JPEG wy³±czony."
#define MSGTR_VO_JPEG_BaselineJPEG "Baseline JPEG w³±czony."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Baseline JPEG wy³±czony."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "Tryb ASCII w³±czony."
#define MSGTR_VO_PNM_RawMode "Surowy tryb w³±czony."
#define MSGTR_VO_PNM_PPMType "Zapiszê pliki PPM."
#define MSGTR_VO_PNM_PGMType "Zapiszê pliki PGM" 
//Will write PGM files.
#define MSGTR_VO_PNM_PGMYUVType "Zapiszê pliki PGMYUV."


// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Tryb przeplotu wymaga aby wysoko¶æ obrazka by³a podzielna przez 4."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Nie mogê zaalokowaæ lini bufora dla trybu przeplotu."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Wej¶cie nie jest w formacie RGB, nie mogê oddzieliæ jasno¶ci przez pola."
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "Szeroko¶æ obrazka musi byæ podzielna przez 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Za ma³o pamiêci aby zaalokowaæ bufor ramek RGB."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Nie mogê dostaæ pamiêci lub pliku aby zapisaæ \"stream.yuv\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "B³±d przy zapisie pliku do wyj¶cia!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Nieznane podurz±dzenie: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "U¿ywam wyj¶cia w trybie przeplotu, najwy¿sze pola najpierw."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "U¿ywam wyj¶cia w trybie przeplotu, najni¿sze pola najpierw."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "U¿ywam (domy¶lnego) trybu progresywnych ramek."

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "Sterownki wyj¶cia video pgm zosta³ zast±piony przez -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Sterownik wyj¶cia video md5 zosta³ zast±piony przez -vo md5sum.\n"


// ======================= AO Audio Output drivers ========================

// libao2 

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Nie mogê otworzyæ miksera %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: Mikser karty d¼wiêkowej nie ma kana³u '%s', u¿ywam domy¶lnego.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Nie mogê otworzyæ urz±dzenia audio %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Nie mogê utworzyæ deskryptora blokuj±cego: %s\n"
#define MSGTR_AO_OSS_CantSetAC3 "[AO OSS] Nie mogê ustawiæ urz±dzenia audio %s na wyj¶cie AC3, próbujê S16...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Nie mogê ustawiæ urz±dzenia audio na %d kana³ów.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: sterownik nie obs³uguje SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Twój sterownik d¼wiêku NIE obs³uguje select()  ***\n Przekompiluj mplayera z #undef HAVE_AUDIO_SELECT w config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nB³±d krytyczny: *** NIE MO¯NA PONOWNIE OTWORZYÆ / ZRESETOWAÆ URZ¡DZENIA AUDIO *** %s\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Po³±czono z serwerem d¼wiêku.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Nie mo¿na otworzyæ strumienia.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Strumieñ otwarty.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] rozmiar bufora: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Ustawienie g³o¶no¶ci na %d nie powiod³o siê.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] dxr2: %d Hz nie obs³ugiwana, spróbuj \"-aop list=resample\"\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound zawiód³: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "opó¼nienie [AO ESD]: [server: %0.2fs, net: %0.2fs] (adjust %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] Nie uda³o siê otworzyæ strumienia esd: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] Ustawienie miksera DVB nie powiod³o siê: %s\n" 
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz nie obs³ugiwana, spróbuj resamplowaæ...\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Plik: %s (%s)\nPCM: Czêstotliwo¶æ próbkowamia: %iHz Kana³y: %s Format %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: najszybsze zrzucanie jest osi±gane poprzez -vc dummy -vo null\nPCM: Info: aby zapisaæ plik WAVE u¿yj -waveheader (default)."
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Nie powiod³o siê otwarcie %s do zapisu!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Czêstotliwo¶æ próbkowania: %iHz Kana³y: %s Format %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] u¿ywam sterownika audio %s.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Nieobs³ugiwany format d¼wiêku: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Inicjalizacja SDL Audio nie powiod³a siê: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Nie mo¿na otworzyæ audio: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] - kontrola.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Czêstotliwo¶æ próbkowania: %iHz Kana³y: %s Format %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] play: niepoprawne urz±dzenie.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparams zawiód³: %s\nNie mo¿na ustawiæ po¿±danej czêstotliwo¶ci próbkowania.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE nie zosta³ zakceptowany przy podanym ¼ródle.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams zawiód³: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: czêstotliwo¶æ próbkowania ustawiona na %lf (po¿±dana skala to %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Nie mo¿na otworzyæ kana³u audio: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO zawiód³.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: zapis nie powiód³ siê."
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Nie mo¿na otworzyæ urz±dzenia audio %s, %s  -> brak d¼wiêku.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: Twoja karta nie obs³uguje %d kana³ul, %s, %d Hz samplerate.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Twój sterownik d¼wiêku NIE obs³uguje select()  ***\n Przekompiluj mplayera z #undef HAVE_AUDIO_SELECT w config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO OSS]\nB³±d krytyczny: *** NIE MO¯NA PONOWNIE OTWORZYÆ / ZRESETOWAÆ URZ¡DZENIA AUDIO *** %s\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: ¿±dany format: %d Hz, %d kana³y, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: nie znaleziono ¿adnych kart d¼wiêkowych.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: nieprawid³owy format (%s) ¿±dany - wyj¶cie wy³±czone.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: B³±d przy odtwarzaniu: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: b³±d informacji pcm: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: znaleziono %d kart d¼wiêkowych, u¿ywam: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: b³±d informacji kana³u pcm: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: b³ad przy ustawianiu parametru: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: b³±d przy ustawianiu kana³u: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: b³±d przy przygotowywaniu kana³u: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: b³±d przy ods±czaniu odtwarzania: %s\n"
//[FIXME] heheh jakies propoycje
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: b³±d przy wy³±czaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: b³±d przy zamykaniu pcm: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: b³±d przy ods±czaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: b³±d przy wy³±czaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: b³±d przy przygotowywaniu kana³u: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: b³±d przy ods±czaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: b³±d przy wy³±czaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: b³±d przy przygotowywaniu kana³u: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: b³±d alsa, resetujê strumieñ.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: b³±d przy przytowywaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: b³±ð zapisu po resecie: %s - poddaje siê.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: b³±d wyj¶cia: %s\n"


// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] nieprawid³owa wtyczka: %s\n"
