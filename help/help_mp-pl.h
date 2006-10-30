// Translated by:  Kuba "Qba" Misiorny <jim85@wp.pl>
// MPlayer-pl translation team, mplayer-pl.emdej.com
// Wszelkie uwagi i poprawki mile widziane :)
//
// Synced with help_mp-en.h 1.173

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Użycie:   mplayer [opcje] [url|ścieżka/]nazwa_pliku\n"
"\n"
"Podstawowe opcje: (Pełna lista w man)\n"
" -vo <drv[:dev]>  wybierz wyjściowy sterownik video [:urządzenie (device)] (lista: '-vo help')\n"
" -ao <drv[:dev]>  wybierz wyjściowy sterownik audio [:urządzenie (device)] (lista: '-ao help')\n"
#ifdef HAVE_VCD
" vcd://<numer_ścieżki>  odtwórz ścieżkę (S)VCD (Super Video CD) (bezpośrednio z napędu, bez montowania)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<tytuł>    odtwórz tytuł bezpośrednio z płyty DVD \n"
" -alang/-slang    wybierz język ścieżki dźwiękowej/napisów (dwuznakowy kod kraju)\n"
#endif
" -ss <pozycja>    skok do pozycji (sekundy albo hh:mm:ss)\n"
" -nosound         nie odtwarzaj dźwięku\n"
" -fs              odtwarzaj na pełnym ekranie (-vm, -zoom, szczegóły w man)\n"
" -x <x> -y <y>    ustaw rozmiar obrazu wyjściowego (używaj z -vm, -zoom)\n"
" -sub <plik>      wybierz plik z napisami (patrz także -subfps, -subdelay)\n"
" -playlist <plik> wybierz listę odtwarzania \n"
" -vid x -aid y    wybierz strumień video (x) lub audio (y)\n"
" -fps x -srate y  zmień prędkość odtwarzania video (x fps) i audio (y Hz) \n"
" -pp <jakość>     włącz filtr postprocessingu (szczegóły w man)\n"
" -framedrop       włącz gubienie ramek (dla wolnych maszyn)\n"
"\n"
"Podstawowe klawisze: (Pełna lista na stronie man, sprawdź też input.conf)\n"
" <-   lub  ->      skok w tył/przód o 10 sekund\n"
" góra lub dół      skok w tył/przód o 1 minutę\n"
" pgup lub pgdown   skok w tył/przód o 10 minut\n"
" < lub >           poprzednia/następna pozycja na liście odtwarzania\n"
" p lub SPACE       pauza (dowolny klawisz aby kontynuować)\n"
" q lub ESC         wyjście\n"
" + lub -           zmień opóźnienie dźwięku o +/- 0.1 sekundy\n"
" o                 tryb OSD (On Screen Display): brak / belka / belka + timer\n"
" * lub /           zwiększ/zmniejsz głośność (PCM)\n"
" z lub x           zmień opóźnienie napisów o +/- 0.1 sekundy\n"
" r lub t           zmień położenie napisów wyżej/niżej, spróbuj też -vf expand\n"
"\n"
" * * * DOKŁADNY SPIS WSZYSTKICH OPCJI ZNAJDUJE SIĘ NA STRONIE MAN * * *\n"
"\n";
#endif
#define MSGTR_SamplesWanted "Aby poprawić obsługę tego formatu, potrzebne są próbki. Proszę się skontaktować z deweloperami.\n"

// ========================= MPlayer messages ===========================

// MPlayer.c:

#define MSGTR_Exiting "\nWychodzę...\n"
#define MSGTR_ExitingHow "\nWychodzę...(%s)\n"
#define MSGTR_Exit_quit "Wyjście"
#define MSGTR_Exit_eof "Koniec pliku"
#define MSGTR_Exit_error "Błąd Krytyczny"
#define MSGTR_IntBySignal "\nMPlayer przerwany sygnałem %d w module: %s\n"
#define MSGTR_NoHomeDir "Nie mogę znaleźć katalogu domowego\n"
#define MSGTR_GetpathProblem "Problem z get_path (\"config\")\n"
#define MSGTR_CreatingCfgFile "Tworzę plik konfiguracyjny: %s\n"
#define MSGTR_CopyCodecsConf "(Skopiuj etc/codecs.conf ze źródeł MPlayera do ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Używam wbudowanego (domyślnego) pliku codecs.conf.\n"
#define MSGTR_CantLoadFont "Nie mogę załadować czcionki: %s\n"
#define MSGTR_CantLoadSub "Nie mogę załadować napisów: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: BŁĄD KRYTYCZNY: Brak wybranego strumienia\n"
#define MSGTR_CantOpenDumpfile "Nie mogę otworzyć pliku dump.\n"
#define MSGTR_CoreDumped "Zrzut pamięci ;)\n"
#define MSGTR_FPSnotspecified "Wartość FPS nie podana (lub błędna) w nagłówku, użyj opcji -fps <ilość_ramek_na_sekundę>.\n"
#define MSGTR_TryForceAudioFmtStr "Wymuszam zastosowanie kodeka audio z rodziny %s...\n"
#define MSGTR_CantFindAudioCodec "Nie mogę znaleźć kodeka dla formatu audio 0x%X.\n"
#define MSGTR_RTFMCodecs "Przeczytaj DOCS/HTML/pl/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Wymuszam zastosowanie kodeka video z rodziny %s...\n"
#define MSGTR_CantFindVideoCodec "Nie mogę znaleźć kodeka pasującego do wybranego -vo i formatu video 0x%X.\n"
#define MSGTR_CannotInitVO "BŁĄD KRYTYCZNY: Nie mogę zainicjalizować sterownika video.\n"
#define MSGTR_CannotInitAO "Nie mogę otworzyć/zainicjalizować urządzenia audio -> brak dźwięku.\n"
#define MSGTR_StartPlaying "Zaczynam odtwarzanie... \n"

#define MSGTR_SystemTooSlow "\n\n"\
"           ************************************************\n"\
"           ********* Twój system jest ZA WOLNY!!! ********\n"\
"           ************************************************\n\n"\
"Prawdopodobne przyczyny, rozwiązania:\n"\
"- Najbardziej powszechne: wadliwe/błędne _sterowniki_audio_\n"\
"  - Spróbuj użyć -ao sdl lub emulacji OSS w ALSA\n"\
"  - Poeksperymentuj z różnymi wartościami -autosync, \"30\" na dobry początek.\n"\
"- Za wolny sterownik wyjściowy:\n"\
"  - Spróbuj innego sterownika -vo (lista: -vo help) albo -framedrop!\n"\
"- Za wolny procesor\n"\
"  - Nie próbuj odtwarzać dużych DVD/DivXów na wolnym procesorze! Spróbuj -hardframedrop.\n"\
"- Zepsuty plik\n"\
"  - Spróbuj różnych kombinacji -nobps, -ni, forceidx, -mc 0.\n"\
"- Za wolne źródło (zamontowane NFS/SMB, DVD, VCD itd.)\n"\
"  - Spróbuj: -cache 8192.\n"\
"- Czy używasz pamięci podręcznej do odtwarzania plików bez przeplotu? Spróbuj -nocache\n"\
"Przeczytaj DOCS/HTML/pl/video.html, gdzie znajdziesz wskazówki\n"\
"jak przyśpieszyć działanie MPlayera\n"\
"Jeśli nic nie pomaga, przeczytaj DOCS/HTML/pl/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer został skompilowany BEZ obsługi GUI.\n"
#define MSGTR_GuiNeedsX "GUI MPlayera potrzebuje X11.\n"
#define MSGTR_Playing "Odtwarzam %s.\n"
#define MSGTR_NoSound "Audio: brak dźwięku\n"
#define MSGTR_FPSforced "Wartość FPS wymuszona na %5.3f  (ftime: %5.3f).\n"
#define MSGTR_CompiledWithRuntimeDetection "Skompilowany z wykrywaniem procesora podczas pracy - UWAGA - W ten sposób nie uzyskasz\n najlepszej wydajności, przekompiluj MPlayera z opcją --disable-runtime-cpudetection.\n"
#define MSGTR_CompiledWithCPUExtensions "Skompilowany dla procesora z rozszerzeniami:"
#define MSGTR_AvailableVideoOutputDrivers "Dostępne sterowniki video:\n"
#define MSGTR_AvailableAudioOutputDrivers "Dostępne sterowniki audio:\n"
#define MSGTR_AvailableAudioCodecs "Dostępne kodeki audio:\n"
#define MSGTR_AvailableVideoCodecs "Dostępne kodeki video:\n"
#define MSGTR_AvailableAudioFm "Dostępne (wkompilowane) rodziny kodeków/sterowników audio:\n"
#define MSGTR_AvailableVideoFm "Dostępne (wkompilowane) rodziny kodeków/sterowników video:\n"
#define MSGTR_AvailableFsType "Dostępne tryby pełnoekranowe:\n"
#define MSGTR_UsingRTCTiming "Używam sprzętowego zegara czasu rzeczywistego (Linux RTC) (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Video: nie mogę odczytać właściwości.\n"
#define MSGTR_NoStreamFound "Nie znalazłem żadnego strumienia\n"
#define MSGTR_ErrorInitializingVODevice "Błąd przy otwieraniu/inicjalizacji wybranego urządzenia video (-vo).\n"
#define MSGTR_ForcedVideoCodec "Wymuszony kodek video: %s\n"
#define MSGTR_ForcedAudioCodec "Wymuszony kodek audio: %s\n"
#define MSGTR_Video_NoVideo "Video: brak video\n"
#define MSGTR_NotInitializeVOPorVO "\nBŁĄD KRYTYCZNY: Nie mogę zainicjalizować filtra video (-vf) lub wyjścia video (-vo).\n"
#define MSGTR_Paused "\n  =====  PAUZA  =====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nNie mogę załadować listy odtwarzania %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer zakończył pracę z powodu błędu 'Nieprawidłowa operacja'\n"\
"  Może to być błąd w naszym nowym kodzie wykrywającym procesor\n"\
"  Przeczytaj proszę DOCS/HTML/pl/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer zakończył pracę z powodu błędu  'Nieprawidłowa operacja'\n"\
"  Zdarza się to najczęściej, gdy uruchamiasz MPlayera na innym procesorze niż ten\n"\
"  dla którego był on skompilowany/zoptymalizowany.\n"\
"  Sprawdź to!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer nieoczekiwanie zakończył pracę przez zły użytek procesora/pamięci/kooprocesora\n"\
"  Przekompiluj MPlayera z opcją '--enable-debug' i wykonaj 'gdb' backtrace \n"\
"  i zdeassembluj. Szczegóły w DOCS/HTML/pl/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer nieoczekiwanie zakończył pracę. To nie powinno się zdarzyć! :).\n"\
"  Może to być błąd w kodzie MPlayera albo w Twoim sterowniku,\n"\
"  lub złej wersji gcc. Jeśli uważasz, że to wina MPlayera, przeczytaj\n"\
"  DOCS/HTML/pl/bugreports.html. Nie możemy i nie pomożemy, jeśli nie przedstawisz tych informacji\n"\
"  zgłaszając możliwy błąd.\n"

#define MSGTR_LoadingConfig "Ładuję konfigurację '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: plik z napisami (%d): %s dodany\n"
#define MSGTR_RemovedSubtitleFile "SUB: plik z napisami (%d): %s usunięty\n"
#define MSGTR_ErrorOpeningOutputFile "Błąd przy otwieraniu pliku [%s] do zapisu!\n"
#define MSGTR_CommandLine "WierszPoleceń:"
#define MSGTR_RTCDeviceNotOpenable "Błąd przy otwieraniu %s: %s (użytkownik pownien móc go odczytać.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Błąd RTC Linuksa w ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Spróbuj dodać \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" do skryptów startowych.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Błąd RTC Linuksa w ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Używam synchronizacji %s.\n"
#define MSGTR_MenuInitialized "Menu zainicjalizowane: %s\n"
#define MSGTR_MenuInitFailed "Błąd inicjalizacji menu.\n"
#define MSGTR_Getch2InitializedTwice "UWAGA: getch2_init został wywołany dwukrotnie!\n"
#define MSGTR_DumpstreamFdUnavailable "Nie mogę zrzucić tego strumienia - brak dostępnych uchwytów plików.\n"
#define MSGTR_FallingBackOnPlaylist "Próbuję zinterpretować jako listę odtwarzania %s...\n" 
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Nie mogę otworzyć filtru video libmenu z głównym menu %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Błąd w preinicjalizacji łańcucha filtrów audio!\n"
#define MSGTR_LinuxRTCReadError "Błąd odczytu RTC Linuksa: %s\n"
#define MSGTR_SoftsleepUnderflow "Uwaga! Niedomiar softsleep!\n"
#define MSGTR_DvdnavNullEvent "zdarzenie DVDNAV NULL?!\n"
#define MSGTR_DvdnavHighlightEventBroken "zdarzenie DVDNAV: Zdarzenie podświetlenia jest zepsute\n"
#define MSGTR_DvdnavEvent "zdarzenie DVDNAV: %s\n"
#define MSGTR_DvdnavHighlightHide "zdarzenie DVDNAV: Ukrycie podświetlenia\n"
#define MSGTR_DvdnavStillFrame "######################################## zdarzenie DVDNAV: Stała Ramka: %d sek\n"
#define MSGTR_DvdnavNavStop "zdarzenie DVDNAV: Nav Stop\n"
#define MSGTR_DvdnavNavNOP "zdarzenie DVDNAV: Nav NOP\n"
//tego nopa -> no operation,  tlumaczyc? 
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "Zdarzenie DVDNAV: Zmiana Strumienia Nav SPU: fiz: %d/%d/%d logiczny: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "Zdarzenie DVDNAV: Zmiana Strumienia Nav SPU: fiz: %d logiczny: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "Zdarzenie DVDNAV: Zmiana Strumienia Audio Nav: fiz: %d logiczny: %d\n"
#define MSGTR_DvdnavNavVTSChange "Zdarzenie DVDNAV: Zmiana Nav VTS\n"
#define MSGTR_DvdnavNavCellChange "Zdarzenie DVDNAV: Zmiana Komórki Nav\n"
#define MSGTR_DvdnavNavSpuClutChange "Zdarzenie DVDNAV: Zmiana Nav SPU CLUT\n"
#define MSGTR_DvdnavNavSeekDone "Zdarzenie DVDNAV: Przeszukiwanie Nav Zrobione\n"
#define MSGTR_MenuCall "Wywołanie menu\n"
#define MSGTR_EdlOutOfMem "Nie mogę zaalokować wystarczająco dużo pamięci dla danych EDL.\n"
#define MSGTR_EdlRecordsNo "Odczytałem %d akcji EDL.\n"
#define MSGTR_EdlQueueEmpty "Nie ma żadnych akcji EDL do wykonania.\n"
#define MSGTR_EdlCantOpenForWrite "Nie mogę otworzyć pliku EDL [%s] do zapisu.\n"
#define MSGTR_EdlCantOpenForRead "Nie mogę otworzyć pliku EDL [%s] do odczytu.\n" 
#define MSGTR_EdlNOsh_video "Nie można używać EDL bez strumienia video, wyłączam.\n"
#define MSGTR_EdlNOValidLine "Nieprawidłowa komenda EDL: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Źle sformatowana komenda [%d], odrzucam.\n"
#define MSGTR_EdlBadLineOverlap "Ostatnia pozycja stopu [%f]; następny start to "\
"[%f]. Wpisy muszą być w kolejności chronologicznej, nie można przeskakiwać. Odrzucam.\n"
#define MSGTR_EdlBadLineBadStop "Czas stopu musi się znaleźć za ustawionym czasem startu.\n"
 
// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Używam pliku kontrolnego pass3: %s\n" 
#define MSGTR_MissingFilename "\nBrak nazwy pliku.\n\n"
#define MSGTR_CannotOpenFile_Device "Nie mogę otworzyć pliku/urządzenia\n"
#define MSGTR_CannotOpenDemuxer "Nie mogę otworzyć demuxera.\n"
#define MSGTR_NoAudioEncoderSelected "\nNie wybrano kodera audio (-oac). Wybierz jakiś (Lista: -oac help) albo użyj opcji '-nosound' \n"
#define MSGTR_NoVideoEncoderSelected "\nNie wybrano kodera video (-ovc). Wybierz jakiś (Lista: -ovc help)\n"
#define MSGTR_CannotOpenOutputFile "Nie mogę otworzyć pliku wyjściowego '%s'.\n"
#define MSGTR_EncoderOpenFailed "Nie mogę otworzyć kodera.\n"
#define MSGTR_ForcingOutputFourcc "Wymuszam wyjściowe fourcc na %x [%.4s]\n"
#define MSGTR_ForcingOutputAudiofmtTag "Wymuszam znacznik wyjściowego formatu audio na 0x%x\n"
#define MSGTR_DuplicateFrames "\n%d powtórzona(e) ramka(i)!\n"
#define MSGTR_SkipFrame "\nOpuszczam ramkę!\n"
#define MSGTR_ResolutionDoesntMatch "\nNowy film ma inną rozdzielczość lub przestrzeń kolorów niż poprzedni.\n"
#define MSGTR_FrameCopyFileMismatch "\nWszystkie filmy muszą mieć identyczne fps, rozdzielczość i przestrzeń kolorów przy -ovc copy.\n"
#define MSGTR_AudioCopyFileMismatch "\nWszystkie filmy muszą mieć identyczne kodeki i formaty audio przy -oac copy.\n"
#define MSGTR_NoSpeedWithFrameCopy "UWAGA: Nie ma gwarancji że -speed działa prawidłowo przy -oac copy"\
"Kodowanie może być popsute!\n"
#define MSGTR_ErrorWritingFile "%s Błąd przy zapisie pliku.\n"
#define MSGTR_RecommendedVideoBitrate "Zalecany video bitrate dla tego %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nStrumień video: %8.3f kbit/s (%d B/s) rozmiar: %"PRIu64" bajtów %5.3f s %d ramek\n"
#define MSGTR_AudioStreamResult "\nStrumień audio: %8.3f kbit/s (%d B/s) rozmiar: %"PRIu64" bajtów %5.3f s\n"

#define MSGTR_OpenedStream "sukces: format: %d  dane: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videocodec: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "audiocodec: framecopy (format=%x chans=%d rate=%d bits=%d B/s=%d sample-%d)\n"
#define MSGTR_CBRPCMAudioSelected "Wybrano dźwięk CBR PCM\n"
#define MSGTR_MP3AudioSelected "Wybrano dźwięk MP3\n"
#define MSGTR_CannotAllocateBytes "Nie można było zaalokować %d bajtów\n"
#define MSGTR_SettingAudioDelay "Ustawiam OPÓŹNIENIE DŹWIĘKU na %5.3f\n"
#define MSGTR_SettingAudioInputGain "Ustawiam podbicie wejścia dźwięku na %f\n"
#define MSGTR_LamePresetEquals "\nustawienie=%s\n\n"
#define MSGTR_LimitingAudioPreload "Ograniczam buforowanie dźwięku do 0.4s\n"
#define MSGTR_IncreasingAudioDensity "Zwiększam gęstość dźwięku do 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Wymuszam buforowanie dźwięku na 0, maksymalną korekcję pts na 0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR audio: %d bajtów/sek, %d bajtów/blok\n"
#define MSGTR_LameVersion "Wersja LAME %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Błąd: Wybrany bitrate jest poza prawidłowym zasiegiem tego ustawienia\n"\
"\n"\
"Podczas używania tego trybu musisz wpisać wartość pomiędzy \"8\" i \"320\"\n"\
"\n"\
"Dalsze informacje: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Błąd: Nie wprowadziłeś odpowiedniego profilu lub/i opcji dla tego ustawienia\n"\
"\n"\
"Dostępne profile:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                      insane\n"\
"   <cbr> (ABR Mode) - Tryb ABR jest domyślny. Aby go użyć,\n"\
"                      podaj po prostu bitrate. Na przykład:\n"\
"                      \"preset=185\" aktywuje to ustawienie\n"\
"                      i używa 185 jako średnie kbps.\n"\
"\n"\
"    Kilka przykładów:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" lub \"-lameopts  cbr:preset=192       \"\n"\
" lub \"-lameopts      preset=172       \"\n"\
" lub \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Dalsze informacje: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Zestawy ustawień zaprojektowane są w celu uzysakania jak najwyższej jakości.\n"\
"\n"\
"Byly poddawane i dopracowywane przez rygorystyczne testy\n"\
"odsluchowe, aby osiagnac ten cel.\n"\
"\n"\
"Są one bez przerwy aktualizowane, aby nadążyć za najświeższymi nowinkami\n"\
"co powinno przynosić prawie najwyższą osiągalną w LAME jakość.\n"\
"\n"\
"Aby aktywować te ustawienia:\n"\
"\n"\
"   Dla trybów VBR (zazwyczaj najlepsza jakość):\n"\
"\n"\
"     \"preset=standard\" To ustawienie powinno być przeźroczyste\n"\
"                             dla większości ludzi przy odtwarzaniu muzyki i odrazu\n"\
"                             jest w niezłej jakości.\n"\
"\n"\
"     \"preset=extreme\" Jeśli masz bardzo dobry słuch i równie dobry sprzęt,\n"\
"                             to ustawienie, daje trochę lepszą jakoś niż \n"\
"                             tryb \"standard\".\n"\
"\n"\
"   Dla trybu CBR 320kbps (najwyższa możliwa jakość ze wszystkich możliwych ustawień):\n"\
"\n"\
"     \"preset=insane\"  To ustawienie będzie przesadą\n"\
"                             dla większości ludzi w większości przypadków,\n"\
"                             ale jeżeli musisz mieć najwyższą jakoś niezależnie\n"\
"                             od wielkości pliku, to jest właściwa droga.\n"\
"\n"\
"   Dla trybów ABR (wysoka jakość z ustalonym bitratem, ale tak wysoka jak VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Określenie tego parametru da Tobie dobrą jakość\n"\
"                             przy ustalonym bitrate'cie. pierając się na niej,\n"\
"                             określi ono optymalne ustawienia dla danej sytuacji.\n"\
"			      Niestety nie jest ono tak elastyczne jak VBR i przeważnie nie\n"\
"                            zapewni takiego samego poziomu jakości jak VBR\n"\
"                            dla wyższych wartości bitrate.\n"\
"\n"\
"Poniższe opcje są również dostępne dla odpowiadających profili:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - Tryb ABR jest domyślny. Aby go użyć,\n"\
"                      podaj po prostu bitrate. Na przykład:\n"\
"                      \"preset=185\" aktywuje to ustawienie\n"\
"                      i używa 185 jako średnie kbps.\n"\
"\n"\
"   \"fast\" - Uruchamia nowe szybkie VBR dla danego profilu. Wadą \n"\
"            w stosunku do ustawienia szybkości jest to, iż często bitrate jest\n"\
"            troszkę wyższy niż przy normalnym trybie, a jakość \n"\
"            może być troche niższa.\n"\
"   Uwaga: obecna wersja ustawienia \"fast\" może skutkować wyskomi wartościami\n"\
"            bitrate w stosunku do tego z normalnego ustawienia.\n"\
"\n"\
"   \"cbr\"  - Jeżeli używasz trybu ABR (przeczytaj powyżej) ze znacznym bitratem\n"\
"            jak 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            możesz użyć opcji  \"cbr\", aby wymusić enkodowanie w trybie CBR\n"\
"            zamiast standardowego trybu abr. ABR daje wyższą jakość, ale CBR\n"\
"            może się przydać w sytuacjach, gdy strumieniowanie mp3 przez\n"\
"            Internet jest ważne\n"\
"\n"\
"    Na przykład:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Dostępnych jest kilka synonimów dla trybu ABR:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit "Nie można ustawić opcji LAME, sprawdź bitrate/częstotliwości "\
"próbkowania. Niektóre bardzo niskie bitrate (<32) wymagają niższych częstotliwości próbkowania "\
"(n.p. -srate 8000). Jeśli wszysto zawiedzie wypróbuj wbudowane ustawienie."
#define MSGTR_ConfigFileError "błąd pliku konfiguracyjnego"
#define MSGTR_ErrorParsingCommandLine "błąd przy przetwarzaniu wiersza poleceń"
#define MSGTR_VideoStreamRequired "Strumień video jest wymagany!\n"
#define MSGTR_ForcingInputFPS "wejściowa wartość fps będzie zinterpretowana jako %5.2f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Format wyjściowy RAWVIDEO nie wspiera audio - wyłączam audio\n"
#define MSGTR_DemuxerDoesntSupportNosound "Ten demuxer jeszcze nie wspiera -nosound.\n"
#define MSGTR_MemAllocFailed "nie udało sie zaalokować pamięci"
#define MSGTR_NoMatchingFilter "Nie można znaleźć pasującego formatu filtra/ao!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, zepsuty kompilator C?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC - brakująca nazwa kodeka!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC - nie mogę znaleźć kodeka dla %s\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC - nie mogę zaalokować treści!\n"
#define MSGTR_CouldntOpenCodec "Nie mogę otworzyć kodeka %s, br=%d\n"
#define MSGTR_CantCopyAudioFormat "Format audio 0x%x jest niekompatybilny z '-oac copy', spróbuj zamiast tego '-oac pcm' albo użyj '-fafmttag' żeby wymusić.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metoda zmiennego bitrate\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(default)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           średni bitrate\n"\
"\n"\
" cbr           stały bitrate\n"\
"               Wymusza także kodowanie CBR (stały bitrate) w następujących po tej opcji ustawieniach ABR\n"\
"\n"\
" br=<0-1024>   podaj bitrate w kBit (tylko CBR i ABR)\n"\
"\n"\
" q=<0-9>       jakość (0-najwyższa, 9-najniższa) (tylko VBR)\n"\
"\n"\
" aq=<0-9>      jakość algorytmu (0-najlepsza/najwolniejszy, 9-najgorsza/najszybszy)\n"\
"\n"\
" ratio=<1-100> współczynnik kompresji\n"\
"\n"\
" vol=<0-10>    ustaw wzmocnienie sygnału audio\n"\
"\n"\
" mode=<0-3>    (domyślnie: auto)\n"\
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
" fast          Przełącz na szybsze kodowanie na poniższych ustawieniach VBR,\n"\
"		nieznacznie niższa jakość i wyższy bitrate.\n"\
"\n"\
" preset=<value>  Włącza najwyższą możliwą jakość.\n"\
"                 medium: kodowanie VBR, dobra jakość\n"\
"                 (bitrate: 150-180 kb/s)\n"\
"                 standard:  kodowanie VBR, wysoka jakość\n"\
"                 (bitrate: 170-210 kb/s)\n"\
"                 extreme: kodowanie VBR, bardzo wysoka jakość\n"\
"                 (bitrate: 200-240 kb/s)\n"\
"                 insane:  kodowanie CBR, najwyższa jakość\n"\
"                 (bitrate: 320 kb/s)\n"\
"                 <8-320>: kodowanie ABR przy podanym średnim bitrate.\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "zduplikowane FourCC"
#define MSGTR_TooManyFourccs "za dużo FourCC/formatów..."
#define MSGTR_ParseError "błąd parsowania"
//again parse error
#define MSGTR_ParseErrorFIDNotNumber "błąd parsowania (ID formatu nie jest liczbą?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "błąd parsowania (alias ID formatu nie jest liczbą?)"
#define MSGTR_DuplicateFID "zduplikowane ID formatu"
#define MSGTR_TooManyOut "za dużo out..."
// Rany, co to jest? Ale po angielsku równie dużo rozumiem.
#define MSGTR_InvalidCodecName "\nkodek(%s) nazwa nieprawidłowa!\n"
#define MSGTR_CodecLacksFourcc "\nkodek(%s) nie ma FourCC/formatu!\n"
#define MSGTR_CodecLacksDriver "\nkodek(%s) nie ma sterownika!\n"
#define MSGTR_CodecNeedsDLL "\nkodek(%s) potrzebuje 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\nkodek(%s) potrzebuje 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Nie mogę zaalokować pamięci na komentarz. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "Czytam %s: "
#define MSGTR_CantOpenFileError "Nie mogę otworzyć '%s': %s\n"
#define MSGTR_CantGetMemoryForLine "Nie mogę dostać pamięci na 'line': %s\n"
#define MSGTR_CantReallocCodecsp "Nie mogę ponownie zaalokować '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "Nazwa kodeka '%s' nie jest unikalna."
#define MSGTR_CantStrdupName "Nie mogę strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Nie mogę strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Nie mogę strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Nie mogę strdup -> 'dll': %s"
// Na moje wyczucie to w '' to nazwy pól => nie tłumaczyć. Ale może się mylę.
#define MSGTR_AudioVideoCodecTotals "Kodeki audio %d i video %d\n"
#define MSGTR_CodecDefinitionIncorrect "Kodek nie jest poprawnie zdefiniowany."
#define MSGTR_OutdatedCodecsConf "Ten codecs.conf jest za stary i nie kompatybilny z tym wydaniem MPlayera!"

// divx4_vbr.c:
#define MSGTR_OutOfMemory "brak pamięci"
#define MSGTR_OverridingTooLowBitrate "Określony bitrate jest za niski dla tego klipu.\n"\
"Minimalny możliwy bitrate dla tego klipu to %.0f kbps. Nadpisuje\n"\
"wartość podaną przez użytkownika.\n"

// fifo.c
#define MSGTR_CannotMakePipe "Nie mogę utworzyć PIPEa!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Znaleziony slot z lvl %d jest za stary: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "Opcja %s nie może być używana w pliku konfiguracyjnym.\n"
#define MSGTR_InvalidCmdlineOption "Opcja %s nie może być używana w wierszu poleceń.\n"
#define MSGTR_InvalidSuboption "Błąd: opcja '%s' nie ma podopcji '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Błąd: podopcja '%s' opcji '%s' musi mieć paramter!\n"
#define MSGTR_MissingOptionParameter "Błąd: opcja '%s' musi mieć parametr!\n"
#define MSGTR_OptionListHeader "\n Nazwa                Typ             Min        Max      GlobalnaCL    Cfg\n\n"
#define MSGTR_TotalOptions "\nW sumie: %d opcji\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Nie znaleziono urządzenia CD-ROM '%s'\n"
#define MSGTR_ErrTrackSelect "Błąd przy wybieraniu ścieżki VCD."
#define MSGTR_ReadSTDIN "Czytam ze stdin (standardowego wejścia)...\n"
#define MSGTR_UnableOpenURL "Nie mogę otworzyć URL: %s\n"
#define MSGTR_ConnToServer "Połączyłem się z serwerem: %s\n"
#define MSGTR_FileNotFound "Nie znaleziono pliku '%s'\n"

#define MSGTR_SMBInitError "Nie mogę zainicjalizować biblioteki libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Nie mogę odczytać z LANu: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer nie został skompilowany z obsługą SMB.\n"

#define MSGTR_CantOpenDVD "Nie mogę otworzyć DVD: %s\n"
#define MSGTR_DVDnumTitles "Na tym DVD jest %d tytułów.\n"
#define MSGTR_DVDinvalidTitle "Nieprawidłowy numer tytułu: %d\n"
#define MSGTR_DVDnumChapters "W tym tytule DVD jest %d rozdziałów.\n"
#define MSGTR_DVDinvalidChapter "Nieprawidłowy numer rozdziału (DVD): %d\n"
#define MSGTR_DVDnumAngles "W tym tytule DVD znajduje się %d ustawień (kątów) kamery.\n"
#define MSGTR_DVDinvalidAngle "Nieprawidłowy numer ustawienia kamery: %d\n"
#define MSGTR_DVDnoIFO "Nie mogę otworzyć pliku IFO dla tytułu DVD %d.\n"
#define MSGTR_DVDnoVOBs "Nie mogę otworzyć tytułu VOBS (VTS_%02d_1.VOB).\n"

// muxer_*.c:
#define MSGTR_TooManyStreams "Za dużo strumieni!"
#define MSGTR_RawMuxerOnlyOneStream "Mukser rawaudio obsługuje tylko jeden strumień audio!\n"
#define MSGTR_IgnoringVideoStream "Ignoruję strumień video!\n"
#define MSGTR_UnknownStreamType "Ostrzeżenie! Nieznany typ strumienia: %d\n"
#define MSGTR_WarningLenIsntDivisible "Ostrzeżenie! len nie jest podzielne przez samplesize!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "UWAGA: Redefiniowano nagłówek strumienia audio %d!\n"
#define MSGTR_VideoStreamRedefined "UWAGA: Redefiniowano nagłówek strumienia video %d!\n"
#define MSGTR_TooManyAudioInBuffer "\nZa dużo pakietów audio w buforze (%d w %d bajtach)\n"
#define MSGTR_TooManyVideoInBuffer "\nZa dużo pakietów video w buforze (%d w %d bajtach)\n"

#define MSGTR_MaybeNI "Może odtwarzasz plik/strumień bez przeplotu (non-interleaved) albo kodek nie zadziałał?\n"\
		      "Dla plików AVI spróbuj wymusić tryb bez przeplotu z opcją '-ni'\n"
#define MSGTR_SwitchToNi "\nWykryto zbiór AVI z błędnym przeplotem - przełączam w tryb -ni\n"
#define MSGTR_Detected_XXX_FileFormat "Wykryto format %s.\n"
#define MSGTR_DetectedAudiofile "Wykryto plik audio.\n"
#define MSGTR_NotSystemStream "Nie jest to format MPEG System Stream... (może Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Nieprawidłowy strumień MPEG-ES??? Skontaktuj się z autorem to może być błąd :(\n"
#define MSGTR_FormatNotRecognized "============ Przykro mi, nierozpoznany/nieobsługiwany format pliku =============\n"\
				  "=== Jeśli jest to strumień AVI, ASF albo MPEG skontaktuj się z autorem! ===\n"
#define MSGTR_MissingVideoStream "Nie znaleziono strumienia video.\n"
#define MSGTR_MissingAudioStream "Nie znaleziono strumienia audio -> brak dźwięku.\n"
#define MSGTR_MissingVideoStreamBug "Brakuje strumienia video!? Skontaktuj się z autorem, to może być błąd:(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Plik nie zawiera wybranego strumienia audio i video.\n"

#define MSGTR_NI_Forced "Wymuszony"
#define MSGTR_NI_Detected "Wykryty"
#define MSGTR_NI_Message "%s plik formatu NON-INTERLEAVED AVI (bez przeplotu).\n"

#define MSGTR_UsingNINI "Używam uszkodzonego formatu pliku NON-INTERLAVED AVI.\n"
#define MSGTR_CouldntDetFNo "Nie mogę określić liczby ramek (dla przeszukiwania bezwzględnego).\n"
#define MSGTR_CantSeekRawAVI "Nie mogę przeszukiwać nieindeksowanych strumieni AVI (Index jest wymagany, spróbuj z opcja '-idx')\n"
#define MSGTR_CantSeekFile "Nie mogę przeszukiwać tego pliku\n"

#define MSGTR_EncryptedVOB "Zaszyfrowany plik VOB! Przeczytaj DOCS/HTML/pl/dvd.html.\n"

#define MSGTR_MOVcomprhdr "MOV: Obsługa skompresowanych nagłówków wymaga ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: UWAGA: Zmienna FOURCC wykryta!?\n"
#define MSGTR_MOVtooManyTrk "MOV: UWAGA: za dużo ścieżek"
#define MSGTR_FoundAudioStream "==> Wykryto strumień audio: %d\n"
#define MSGTR_FoundVideoStream "==> Wykryto strumień video: %d\n"
#define MSGTR_DetectedTV "Wykryto TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Nie mogę otworzyć demuxera ogg.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Szukam strumienia audio (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Nie mogę otworzyć strumienia audio %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nie mogę otworzyć strumienia z napisami: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Nie mogę otworzyć demuxera audio: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Nie mogę otworzyć demuxera napisów: %s\n"
#define MSGTR_TVInputNotSeekable "Wejścia TV nie można przeszukiwać (Przeszukiwanie będzie służyło do zmiany kanałów ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Informacje %s o demuxerze są już obecne!\n"
#define MSGTR_ClipInfo "Informacje o klipie:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: Wykryto zawartość 30000/1001fps NTSC, zmieniam liczbę ramek na sekundę.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: Wykryto progresywną zawartość 240000/1001fps NTSC, zmieniam liczbę ramek na sekundę.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nie mogę otworzyć kodeka.\n"
#define MSGTR_CantCloseCodec "Nie mogę zamknąć kodeka.\n"

#define MSGTR_MissingDLLcodec "Błąd: Nie mogę otworzyć wymaganego kodeka DirectShow %s.\n"
#define MSGTR_ACMiniterror "Nie mogę załadować/zainicjalizować kodeka Win32/ACM AUDIO (Brakuje pliku DLL?).\n"
#define MSGTR_MissingLAVCcodec "Nie mogę znaleźć kodeka '%s' w libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: BŁĄD KRYTYCZNY: EOF (koniec pliku) podczas szukania nagłówka sekwencji.\n"
#define MSGTR_CannotReadMpegSequHdr "BŁĄD KRYTYCZNY: Nie mogę odczytać nagłówka sekwencji.\n"
#define MSGTR_CannotReadMpegSequHdrEx "BŁĄD KRYTYCZNY: Nie mogę odczytać rozszerzenia nagłówka sekwencji.\n"
#define MSGTR_BadMpegSequHdr "MPEG: Nieprawidłowy nagłówek sekwencji\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Nieprawidłowe rozszerzenie nagłówka sekwencji\n"

#define MSGTR_ShMemAllocFail "Nie mogę zaalokować pamięci współdzielonej.\n"
#define MSGTR_CantAllocAudioBuf "Nie mogę zaalokować bufora wyjściowego audio.\n"

#define MSGTR_UnknownAudio "Nieznany/brakujący format audio -> brak dźwięku\n"

#define MSGTR_UsingExternalPP "[PP] Używam zewnętrznego filtra postprocessingu, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Używam postprocessingu kodeka, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Atrybut video '%s' nie jest obsługiwany przez wybrane vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Wybrana rodzina kodeków video [%s](vfm=%s) jest niedostępna.\nWłącz ją przy kompilacji.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Wybrana rodzina kodeków audio [%s](vfm=%s) jest niedostępna.\nWłącz ją przy kompilacji.\n"
#define MSGTR_OpeningVideoDecoder "Otwieram dekoder video: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Otwieram dekoder audio: [%s] %s\n"
#define MSGTR_UninitVideoStr "deinicjalizacja video: %s\n"
#define MSGTR_UninitAudioStr "deinicjalizacja audio: %s\n"
#define MSGTR_VDecoderInitFailed "Inicjalizacja VDecodera nie powiodła się :(\n"
#define MSGTR_ADecoderInitFailed "Inicjalizacja ADecodera nie powiodła się :(\n"
#define MSGTR_ADecoderPreinitFailed "Nieudana preinicjalizacja ADecodera :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alokuję %d bajtów dla bufora wejściowego.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alokuję %d + %d = %d bajtów dla bufora wyjściowego.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Włączam obsługę LIRC...\n"
#define MSGTR_LIRCdisabled "Nie będziesz mógł używać swojego pilota.\n"
#define MSGTR_LIRCopenfailed "Nie mogę uruchomić obsługi LIRC.\n"
#define MSGTR_LIRCcfgerr "Nie mogę odczytać pliku konfiguracyjnego LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nie mogę znaleźć filtra video '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "Nie mogę otworzyć filtra video '%s'.\n"
#define MSGTR_OpeningVideoFilter "Otwieram filtr video: "
#define MSGTR_CannotFindColorspace "Nie mogę znaleźć odpowiedniej przestrzenii kolorów, nawet poprzez wstawienie 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Kodek nie ustawił sh->disp_w i sh->disp_h, próbuję to rozwiązać.\n"
#define MSGTR_VoConfigRequest "VDec: wymagana konfiguracja vo - %d x %d (preferowana csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Nie mogę znaleźć pasującej przestrzeni koloru - próbuję ponownie z -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Proporcje filmu (obrazu) to %.2f:1 - skaluję do prawidłowych proporcji.\n"
#define MSGTR_MovieAspectUndefined "Proporcje filmu (obrazu) nie są zdefiniowane - nie skaluję.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Musisz zainstalować/zaktualizować pakiet binarnych kodeków.\nIdź do http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFORMACJA: Inicjalizacja kodeka video Win32/DShow przebiegła pomyślnie.\n"
#define MSGTR_DMOInitOK "INFORMACJA: Inicjalizacja kodeka video Win32/DMO przebiegła pomyślnie.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Nie mogłem wysłać zdarzenia pełnego ekranu EWMH!\n"

#define MSGTR_InsertingAfVolume "[Mixer] Nie ma sprzętowego miksowania, włączam filtr głośności.\n"
#define MSGTR_NoVolume "[Mixer] Regulacja głośności niedostępna.\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O programie"
#define MSGTR_FileSelect "Wybierz plik..."
#define MSGTR_SubtitleSelect "Wybierz napisy..."
#define MSGTR_OtherSelect "Wybierz..."
#define MSGTR_AudioFileSelect "Wybierz zewnętrzny kanał audio..."
#define MSGTR_FontSelect "Wybierz czcionkę..."
#define MSGTR_PlayList "Lista odtwarzania"
#define MSGTR_Equalizer "Equalizer (korektor)"
#define MSGTR_SkinBrowser "Przeglądarka skórek"
#define MSGTR_Network "Strumieniowanie sieciowe..."
#define MSGTR_Preferences "Preferencje"
#define MSGTR_AudioPreferences "Konfiguracja sterownika audio"
#define MSGTR_NoMediaOpened "Nie otwarto żadnego nośnika."
#define MSGTR_VCDTrack "ścieżka VCD: %d"
#define MSGTR_NoChapter "Brak rozdziału"
#define MSGTR_Chapter "Rozdział %d"
#define MSGTR_NoFileLoaded "Nie załadowano żadnego pliku."

// --- buttons ---
#define MSGTR_Ok "OK"
#define MSGTR_Cancel "Anuluj"
#define MSGTR_Add "Dodaj"
#define MSGTR_Remove "Usuń"
#define MSGTR_Clear "Wyczyść"
#define MSGTR_Config "Konfiguracja"
#define MSGTR_ConfigDriver "Konfiguracja sterownika"
#define MSGTR_Browse "Przeglądaj"

// --- error messages ---
#define MSGTR_NEMDB "Przykro mi, za mało pamięci na bufor rysowania."
#define MSGTR_NEMFMR "Przykro mi, za mało pamięci do wyrenderowania menu."
#define MSGTR_IDFGCVD "Przykro mi, nie znalazłem kompatybilnego z GUI sterownika video."
#define MSGTR_NEEDLAVCFAME "Przykro mi, nie możesz odtwarzać plików innych niż MPEG za pomocą urządzenia DXR3/H+ bez przekodowania.\nWłącz lavc albo fame w konfiguracji DXR3/H+"
#define MSGTR_UNKNOWNWINDOWTYPE "Znaleziono okno nieznanego typu..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] błąd w pliku konfiguracyjnym skórki, w wierszu %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] ostrzeżenie w pliku konfiguracyjnym w wierszu %d:\nznaleziono znacznik widget (%s) ale nie ma przed nim \"section\""
#define MSGTR_SKIN_WARNING2 "[skin] ostrzeżenie w pliku konfiguracyjnym w wierszu %d:\nznaleziono znacznik widget (%s) ale nie ma przed nim \"subsection\""
#define MSGTR_SKIN_WARNING3 "[skin] ostrzeżenie w pliku konfiguracyjnym w wierszu %d::\nta podsekcja nie jest obsługiwana przez widget (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[skin] nie znaleziono pliku ( %s ).\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skin] pliku nie można czytać ( %s ).\n"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmapy 16 bitowe lub mniejsze nie są obsługiwane (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "nie znaleziono pliku (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "błąd odczytu bmp (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "błąd odczytu tga (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "błąd odczytu png (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "tga kompresowane przez RLE nie obsługiwane (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "nieznany typ pliku (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "błąd przy konwersji 24 bitów na 32 bity (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "nieznany komunikat: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "za mało pamięci\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "za dużo zadeklarowanych czcionek\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "nie znaleziono pliku z czcionką\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "nie znaleziono pliku z obrazem czcionki\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "nieistniejący identyfikator czcionki (%s)\n"
#define MSGTR_SKIN_UnknownParameter "nieznany parametr (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "nie znalazłem skórki (%s)\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "Nie znalazłem wybranej skórki ( %s ) używam 'default'...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Błąd odczytu pliku konfiguracyjnego skórki (%s).\n"
#define MSGTR_SKIN_LABEL "Skórki:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O MPlayerze"
#define MSGTR_MENU_Open "Otwórz..."
#define MSGTR_MENU_PlayFile "Odtwórz plik..."
#define MSGTR_MENU_PlayVCD "Odtwórz VCD..."
#define MSGTR_MENU_PlayDVD "Odtwórz DVD..."
#define MSGTR_MENU_PlayURL "Odtwórz URL..."
#define MSGTR_MENU_LoadSubtitle "Załaduj napisy..."
#define MSGTR_MENU_DropSubtitle "Wyłącz napisy..."
#define MSGTR_MENU_LoadExternAudioFile "Załaduj zewnętrzny plik audio..."
#define MSGTR_MENU_Playing "Odtwarzanie"
#define MSGTR_MENU_Play "Odtwarzaj"
#define MSGTR_MENU_Pause "Pauza"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Następny strumień"
#define MSGTR_MENU_PrevStream "Poprzedni strumień"
#define MSGTR_MENU_Size "Rozmiar"
#define MSGTR_MENU_HalfSize   "Połowa normalnego rozmiaru"
#define MSGTR_MENU_NormalSize "Normalny rozmiar"
#define MSGTR_MENU_DoubleSize "Podwójny rozmiar"
#define MSGTR_MENU_FullScreen "Pełny ekran"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Otwórz dysk..."
#define MSGTR_MENU_ShowDVDMenu "Pokaż menu DVD"
#define MSGTR_MENU_Titles "Tytuły"
#define MSGTR_MENU_Title "Tytuł %2d"
#define MSGTR_MENU_None "(brak)"
#define MSGTR_MENU_Chapters "Rozdziały"
#define MSGTR_MENU_Chapter "Rozdział %2d"
#define MSGTR_MENU_AudioLanguages "Języki ścieżki dźwiękowej"
#define MSGTR_MENU_SubtitleLanguages "Języki napisów"
#define MSGTR_MENU_SkinBrowser "Przeglądarka skórek"
#define MSGTR_MENU_Exit "Wyjście..."
#define MSGTR_MENU_Mute "Wyciszenie"
#define MSGTR_MENU_Original "Oryginalny"
#define MSGTR_MENU_AspectRatio "Proporcje obrazu"
#define MSGTR_MENU_AudioTrack "Ścieżka audio"
#define MSGTR_MENU_Track "Ścieżka %d"
#define MSGTR_MENU_VideoTrack "Ścieżka video"

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Jasność: "
#define MSGTR_EQU_Hue "Barwa: "
#define MSGTR_EQU_Saturation "Nasycenie: "
#define MSGTR_EQU_Front_Left "Lewy przedni"
#define MSGTR_EQU_Front_Right "Prawy przedni"
#define MSGTR_EQU_Back_Left "Lewy tylny"
#define MSGTR_EQU_Back_Right "Prawy tylny"
#define MSGTR_EQU_Center "Centralny"
#define MSGTR_EQU_Bass "Basowy"
#define MSGTR_EQU_All "Wszystkie"
#define MSGTR_EQU_Channel1 "Kanał 1:"
#define MSGTR_EQU_Channel2 "Kanał 2:"
#define MSGTR_EQU_Channel3 "Kanał 3:"
#define MSGTR_EQU_Channel4 "Kanał 4:"
#define MSGTR_EQU_Channel5 "Kanał 5:"
#define MSGTR_EQU_Channel6 "Kanał 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Ścieżka"
#define MSGTR_PLAYLIST_Selected "Wybrane pliki"
#define MSGTR_PLAYLIST_Files "Pliki"
#define MSGTR_PLAYLIST_DirectoryTree "Drzewo katalogu"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "Napisy i OSD"
#define MSGTR_PREFERENCES_Codecs "kodeki i demuxer"
#define MSGTR_PREFERENCES_Misc "Inne"

#define MSGTR_PREFERENCES_None "Brak"
#define MSGTR_PREFERENCES_DriverDefault "domyślne ustawienia sterownika"
#define MSGTR_PREFERENCES_AvailableDrivers "Dostępne sterowniki:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nie odtwarzaj dźwięku"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizuj dźwięk"
#define MSGTR_PREFERENCES_EnableEqualizer "Włącz equalizer (korektor)"
#define MSGTR_PREFERENCES_SoftwareMixer "Włącz Mikser Programowy"
#define MSGTR_PREFERENCES_ExtraStereo "Włącz extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Współczynnik:"
#define MSGTR_PREFERENCES_AudioDelay "Opóźnienie dźwięku"
#define MSGTR_PREFERENCES_DoubleBuffer "Włącz podwójne buforowanie"
#define MSGTR_PREFERENCES_DirectRender "Włącz bezpośrednie renderowanie (direct rendering)"
#define MSGTR_PREFERENCES_FrameDrop "Włącz gubienie ramek"
#define MSGTR_PREFERENCES_HFrameDrop "Włącz gubienie dużej ilości ramek (niebezpieczne)"
#define MSGTR_PREFERENCES_Flip "Odwróć obraz do góry nogami"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Timer i wskaźniki"
#define MSGTR_PREFERENCES_OSDProgress "Tylko belka"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Timer, czas w procentach i całkowity"
#define MSGTR_PREFERENCES_Subtitle "Napisy:"
#define MSGTR_PREFERENCES_SUB_Delay "Opóźnienie: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozycja: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Wyłącz automatyczne ładowanie napisów"
#define MSGTR_PREFERENCES_SUB_Unicode "Napisy w Unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konwertuj dane napisy na format napisów MPlayera"
#define MSGTR_PREFERENCES_SUB_SRT "Konwertuj dane napisy na format SRT (bazowany na czasie SubViewer)"
#define MSGTR_PREFERENCES_SUB_Overlap "Przełącz nakładanie (overlapping) napisów"
#define MSGTR_PREFERENCES_Font "Czcionka:"
#define MSGTR_PREFERENCES_FontFactor "Skala czcionki:"
#define MSGTR_PREFERENCES_PostProcess "Włącz postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Automatyczna jakość:"
#define MSGTR_PREFERENCES_NI "Użyj parsera odpowiedniego dla AVI bez przeplotu (non-interleaved)"
#define MSGTR_PREFERENCES_IDX "Przebuduj tablicę indeksów, jeśli to potrzebne"
#define MSGTR_PREFERENCES_VideoCodecFamily "Rodzina kodeków video:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Rodzina kodeków audio:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Poziom OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Napisy"
#define MSGTR_PREFERENCES_FRAME_Font "Czcionka"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Kodeki i demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache (pamięć podręczna)"
#define MSGTR_PREFERENCES_Audio_Device "Urządzenie:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mikser:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Kanał miksera:"
#define MSGTR_PREFERENCES_Message "Pamiętaj, że niektóre opcje wymagają zrestartowania odtwarzania!"
#define MSGTR_PREFERENCES_DXR3_VENC "Enkoder video:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Użyj LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Użyj FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Zachodnioeuropejskie języki (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Zachodnioeuropejskie języki z Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Języki słowiańskie i środkowoeuropejskie (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galijski, Maltański, Turecki (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Stary, bałtycki zestaw znaków (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cyrlica (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabski (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Współczesna Greka (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turecki (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Języki bałtyckie (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celtycki (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Znaki hebrajskie (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Rosyjski (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukraiński, Białoruski (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Uproszczone znaki chińskie (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Tradycyjne znaki chińskie (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Znaki japońskie (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Znaki koreańskie (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Znaki tajskie (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrlica Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Języki słowiańskie i środkowoeuropejskie Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Bez autoskalowania"
#define MSGTR_PREFERENCES_FontPropWidth "Proporcjonalnie do szerokości obrazu"
#define MSGTR_PREFERENCES_FontPropHeight "Proporcjonalnie do wysokości obrazu"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporcjonalnie do przekątnej obrazu"
#define MSGTR_PREFERENCES_FontEncoding "Kodowanie:"
#define MSGTR_PREFERENCES_FontBlur "Rozmycie:"
#define MSGTR_PREFERENCES_FontOutLine "Obramowanie:"
#define MSGTR_PREFERENCES_FontTextScale "Skala tekstu:"
#define MSGTR_PREFERENCES_FontOSDScale "Skala OSD:"
#define MSGTR_PREFERENCES_Cache "Pamięć podręczna"
#define MSGTR_PREFERENCES_CacheSize "Rozmiar pamięci podręcznej: "
#define MSGTR_PREFERENCES_LoadFullscreen "Rozpocznij w trybie pełnoekranowym"
#define MSGTR_PREFERENCES_SaveWinPos "Zapisz pozycję okna"
#define MSGTR_PREFERENCES_XSCREENSAVER "Wyłącz XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Włącz podręczny pasek odtwarzania"
#define MSGTR_PREFERENCES_AutoSync "Włącz/Wyłącz autosynchronizację"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosynchronizacja: "
#define MSGTR_PREFERENCES_CDROMDevice "Urządzenie CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Urządzenie DVD:"
#define MSGTR_PREFERENCES_FPS "FPS:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Pokazuj okno video gdy nieaktywne"

#define MSGTR_ABOUT_UHU "Rozwój GUI sponsorowany przez UHU Linux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Błąd krytyczny!"
#define MSGTR_MSGBOX_LABEL_Error "Błąd!" 
#define MSGTR_MSGBOX_LABEL_Warning "Ostrzeżenie!"

// bitmap.c

#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] za mało pamięci na obraz\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] za mało pamięci na obraz\n"

// cfg.c

#define MSGTR_ConfigFileReadError "[cfg] błąd odczytu pliku konfiguracyjnego...\n"
#define MSGTR_UnableToSaveOption "[cfg] Nie mogę zapisać opcji '%s'.\n"

// interface.c

#define MSGTR_DeletingSubtitles "[GUI] Usuwam napisy.\n"
#define MSGTR_LoadingSubtitles "[GUI] Ładuje napisy: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] Dodaje filtr obrazu: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] Usuwam filtr obrazu: %s\n"
// mw.c

#define MSGTR_NotAFile "%s nie wygląda na plik!\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] Nie mogłem otworzyć ekranu.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Zdalny ekran, wyłączam XMITSHM.\n"
#define MSGTR_WS_NoXshm "[ws] Przykro mi, Twój system nie obsługuje rozszerzenia dzielonej pamięci X.\n"
#define MSGTR_WS_NoXshape "[ws] Przykro mi, Twój system nie obsługuje rozszerzenia XShape.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Przykro mi, za niskie ustwienie głębii kolorów.\n"

// czy MPlayerowi moze byc przykro ?

#define MSGTR_WS_TooManyOpenWindows "[ws] Za dużo otwartych okien.\n"
#define MSGTR_WS_ShmError "[ws] błąd rozszerzenia dzielonej pamięci\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Przykro mi, za mało pamięci na bufor.\n"
#define MSGTR_WS_DpmsUnavailable "Czy DPMS jest dostępny?\n"
#define MSGTR_WS_DpmsNotEnabled "Nie mogę uruchomić DPMS.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "To nie wygląda na plik...\n"
#define MSGTR_WS_DDNothing "D&D: Nothing returned!\n"

#endif


// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Wybrane urządzenie wyjścia video jest niekompatybilne z tym kodekiem.\n"
#define MSGTR_VO_GenericError "Wystąpił błąd"
#define MSGTR_VO_UnableToAccess "Brak dostępu"
#define MSGTR_VO_ExistsButNoDirectory "już istnieje, ale nie jest katalogiem."
#define MSGTR_VO_DirExistsButNotWritable "Wyjściowy katalog już istnieje, ale nie jest zapisywalny."
#define MSGTR_VO_DirExistsAndIsWritable "Wyjściowy katalog już istnieje i nie jest zapisywalny."
#define MSGTR_VO_CantCreateDirectory "Nie można utworzyć wyjściowego katalogu."
#define MSGTR_VO_CantCreateFile "Nie można utworzyć pliku wyjściowego."
#define MSGTR_VO_DirectoryCreateSuccess "Katalog wyjściowy stworzony."
#define MSGTR_VO_ParsingSuboptions "Interpretuję podopcje."
#define MSGTR_VO_SuboptionsParsedOK "Podopcje zinterpretowane poprawnie."
#define MSGTR_VO_ValueOutOfRange "Wartość poza zakresem"
#define MSGTR_VO_NoValueSpecified "Nie podano żadnej wartości."
#define MSGTR_VO_UnknownSuboptions "Nieznana podopcja(e)"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\nAAlib ma następujące podopcje:\n"
#define MSGTR_VO_AA_AdditionalOptions "Dodatkowe opcje obsługiwane przez vo_aa:\n" \
"  help        wyświetla tę wiadomość\n" \
"  osdcolor    ustawia kolor osd\n  subcolor    ustawia kolor napisów\n" \
"        parametry kolorów to:\n           0 : normalny\n" \
"           1 : ciemny\n           2 : jasny\n           3 : pogrubiony\n" \
"           4 : odwrócony\n           5 : specjalny\n\n\n"

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "Progresywny JPEG włączony."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "Progresywny JPEG wyłączony."
#define MSGTR_VO_JPEG_BaselineJPEG "Baseline JPEG włączony."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Baseline JPEG wyłączony."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "Tryb ASCII włączony."
#define MSGTR_VO_PNM_RawMode "Surowy tryb włączony."
#define MSGTR_VO_PNM_PPMType "Zapiszę pliki PPM."
#define MSGTR_VO_PNM_PGMType "Zapiszę pliki PGM" 
//Will write PGM files.
#define MSGTR_VO_PNM_PGMYUVType "Zapiszę pliki PGMYUV."


// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Tryb przeplotu wymaga aby wysokość obrazka była podzielna przez 4."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Nie mogę zaalokować lini bufora dla trybu przeplotu."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Wejście nie jest w formacie RGB, nie mogę oddzielić jasności przez pola."
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "Szerokość obrazka musi być podzielna przez 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Za mało pamięci aby zaalokować bufor ramek RGB."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Nie mogę dostać pamięci lub pliku aby zapisać \"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Błąd przy zapisie pliku do wyjścia!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Nieznane podurządzenie: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Używam wyjścia w trybie przeplotu, najwyższe pola najpierw."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Używam wyjścia w trybie przeplotu, najniższe pola najpierw."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Używam (domyślnego) trybu progresywnych ramek."

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "Sterownki wyjścia video pgm został zastąpiony przez -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Sterownik wyjścia video md5 został zastąpiony przez -vo md5sum.\n"


// ======================= AO Audio Output drivers ========================

// libao2 

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: moduły alsa9 i alsa1x zostały usunięte, zamiast nich uzywaj -ao alsa.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Nie mogę otworzyć miksera %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: Mikser karty dźwiękowej nie ma kanału '%s', używam domyślnego.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Nie mogę otworzyć urządzenia audio %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Nie mogę utworzyć deskryptora blokującego: %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] Nie mogę ustawić urządzenia audio %s na wyjście %s, próbuję %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Nie mogę ustawić urządzenia audio na %d kanałów.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: sterownik nie obsługuje SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Twój sterownik dźwięku NIE obsługuje select()  ***\n Przekompiluj MPlayera z #undef HAVE_AUDIO_SELECT w config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nBłąd krytyczny: *** NIE MOŻNA PONOWNIE OTWORZYĆ / ZRESETOWAĆ URZĄDZENIA AUDIO *** %s\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Połączono z serwerem dźwięku.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Nie można otworzyć strumienia.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Strumień otwarty.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] rozmiar bufora: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Ustawienie głośności na %d nie powiodło się.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] dxr2: %d Hz nie obsługiwana, spróbuj \"-aop list=resample\"\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound zawiódł: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "opóźnienie [AO ESD]: [server: %0.2fs, net: %0.2fs] (adjust %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] Nie udało się otworzyć strumienia esd: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] Ustawienie miksera DVB nie powiodło się: %s\n" 
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz nie obsługiwana, spróbuj resamplować...\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Plik: %s (%s)\nPCM: Częstotliwość próbkowamia: %iHz Kanały: %s Format %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: najszybsze zrzucanie jest osiągane poprzez -vc dummy -vo null\nPCM: Info: aby zapisać plik WAVE użyj -ao pcm:waveheader (domyślny).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Nie powiodło się otwarcie %s do zapisu!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Częstotliwość próbkowania: %iHz Kanały: %s Format %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] używam sterownika audio %s.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Nieobsługiwany format dźwięku: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Inicjalizacja SDL Audio nie powiodła się: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Nie można otworzyć audio: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] - kontrola.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Częstotliwość próbkowania: %iHz Kanały: %s Format %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] play: niepoprawne urządzenie.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparams zawiódł: %s\nNie można ustawić pożądanej częstotliwości próbkowania.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE nie został zakceptowany przy podanym źródle.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams zawiódł: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: częstotliwość próbkowania ustawiona na %lf (pożądana skala to %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Nie można otworzyć kanału audio: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO zawiódł.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: zapis nie powiódł się."
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Nie można otworzyć urządzenia audio %s, %s  -> brak dźwięku.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: Twoja karta nie obsługuje %d kanałul, %s, %d Hz samplerate.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Twój sterownik dźwięku NIE obsługuje select()  ***\n Przekompiluj MPlayera z #undef HAVE_AUDIO_SELECT w config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO OSS]\nBłąd krytyczny: *** NIE MOŻNA PONOWNIE OTWORZYĆ / ZRESETOWAĆ URZĄDZENIA AUDIO *** %s\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: żądany format: %d Hz, %d kanały, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: nie znaleziono żadnych kart dźwiękowych.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: nieprawidłowy format (%s) żądany - wyjście wyłączone.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: Błąd przy odtwarzaniu: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: błąd informacji pcm: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: znaleziono %d kart dźwiękowych, używam: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: błąd informacji kanału pcm: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: bład przy ustawianiu parametru: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: błąd przy ustawianiu kanału: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: błąd przy przygotowywaniu kanału: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: błąd przy odsączaniu odtwarzania: %s\n"
//[FIXME] heheh jakies propoycje
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: błąd przy wyłączaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: błąd przy zamykaniu pcm: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: błąd przy odsączaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: błąd przy wyłączaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: błąd przy przygotowywaniu kanału: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: błąd przy odsączaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: błąd przy wyłączaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: błąd przy przygotowywaniu kanału: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: błąd alsa, resetuję strumień.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: błąd przy przytowywaniu odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: błąd zapisu po resecie: %s - poddaję się.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: błąd wyjścia: %s\n"


// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] nieprawidłowa wtyczka: %s\n"

// ======================= AF Audio Filters ================================

// libaf 

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "dostępne etykiety w"
#define MSGTR_AF_LADSPA_WarnNoInputs "UWAGA! Ta wtyczka LADSPA nie ma wejścia audio.\n Przychodzący sygnał audio będzie stracony."
#define MSGTR_AF_LADSPA_ErrMultiChannel "Wielo-kanałowe (>2) wtyczki nie są obsługiwane (jeszcze).\n  Używaj tylko mono i stereofonicznych wtyczek."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Ta wtyczka LADSPA nie ma żadnego wyjścia audio."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Liczba wejść i wyjść audio wtyczki LADSPA róźni się."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "nie udało się załadować"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Nie można znaleźć funkcji ladspa_descriptor() w podanej bibliotece."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Nie można znaleźć etytkiety w bibliotece wtyczek."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Żadne podopcje nie zostały podane"
#define MSGTR_AF_LADSPA_ErrNoLibFile "Żadna biblioteka nie została podana"
#define MSGTR_AF_LADSPA_ErrNoLabel "Żadna etykieta filtru nie została podana"
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Nie wystarczająca ilość opcji została podana w linii poleceń"
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: Kontrola wejścia #%d jest poniżej dolnej granicy wynoszącej %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: Kontrola wejścia #%d jest powyżej górnej granicy wynoszącej %0.4f.\n"
