// Translated by:  Michał "Miśkieusz" Bolek <miskieusz@gmail.com>
// Wszelkie uwagi i poprawki mile widziane :)
//
// Synced with help_mp-en.h r20586

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Użycie:   mplayer [opcje] [url|ścieżka/]plik\n"
"\n"
"Podstawowe opcje: (pełna lista dostępna na stronie man)\n"
" -vo <drv>        podaj wyjściowy sterownik video (lista: '-vo help')\n"
" -ao <drv>        podaj wyjściowy sterownik audio (lista: '-ao help')\n"
#ifdef CONFIG_VCD
" vcd://<numer_ścieżki>  odtwórz ścieżkę (S)VCD (Super Video CD) (bezpośrednio, bez montowania)\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<tytuł>    odtwórz tytuł DVD z urządzenia zamiast pliku\n"
" -alang/-slang    wybierz język napisów/ścieżki dźwiękowej (dwuliterowy kod kraju)\n"
#endif
" -ss <position>   przejdź do pozycji (sekundy lub hh:mm:ss)\n"
" -nosound         nie odtwarzaj dźwięku\n"
" -fs              odtwarzaj pełnoekranowo (lub -vm, -zoom, szczegóły na stronie man)\n"
" -x <x> -y <y>    ustaw rozdzielczość (użyj z -vm lub -zoom)\n"
" -sub <plik>      podaj plik z napisami (zobacz też -subfps, -subdelay)\n"
" -playlist <plik> podaj plik z listą odtwarzania\n"
" -vid x -aid y    wybierz strumień video (x) oraz audio (y) do odtwarzania\n"
" -fps x -srate y  zmień prędkość odtwarzania video (x fps) oraz audio (y Hz)\n"
" -pp <quality>    włącz filtr 'postprocessing' (szczegóły na stronie man)\n"
" -framedrop       włącz pomijanie ramek (dla słabszych komputerów)\n"
"\n"
"Podstawowe klawisze: (pełna lista na stronie man, zobacz też input.conf)\n"
" <-  lub  ->       skocz o 10 sekund do tyłu/przodu\n"
" dół lub góra      skocz o minutę do tyłu/przodu\n"
" pgdown lub pgup   skocz o 10 minut do tyłu/przodu\n"
" < lub >           skocz do tyłu/przodu w liście odtwarzania\n"
" p lub SPACJA      pauza (wciśnij dowolny klawisz by kontynuować)\n"
" q lub ESC         zatrzymaj odtwarzanie i zamknij program\n"
" + lub -           dostosuj opóźnienie audio o +/- 0.1 sekundy\n"
" o                 przełącz tryb OSD:  brak / belka / belka + czas\n"
" * lub /           zwiększ lub zmniejsz głośność - PCM\n"
" x lub z           dostosuj opóźnienie napisów o +/- 0.1 sekundy\n"
" r lub t           dostosuj położenie napisów góra/dół, zobacz też -vf expand\n"
"\n"
" * * * WIĘCEJ OPCJI DOSTĘPNYCH NA STRONIE PODRĘCZNIKA MAN * * *\n"
"\n";
#endif

// libmpcodecs/ad_dvdpcm.c:
#define MSGTR_SamplesWanted "By poprawić obsługę tego formatu potrzebne są próbki. Proszę skontaktować się z twórcami.\n"

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nWychodzę...\n"
#define MSGTR_ExitingHow "\nWychodzę... (%s)\n"
#define MSGTR_Exit_quit "Wyjście"
#define MSGTR_Exit_eof "Koniec pliku"
#define MSGTR_Exit_error "Błąd krytyczny"
#define MSGTR_IntBySignal "\nMPlayer przerwany sygnałem %d w module: %s\n"
#define MSGTR_NoHomeDir "Nie mogę odnaleźć katalogu domowego.\n"
#define MSGTR_GetpathProblem "Problem z get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Tworzę plik konfiguracyjny: %s\n"
#define MSGTR_BuiltinCodecsConf "Używam wbudowanego codecs.conf.\n"
#define MSGTR_CantLoadFont "Nie mogę załadować czcionki: %s\n"
#define MSGTR_CantLoadSub "Nie mogę załadować napisów: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: Błąd krytyczny: Wybrany strumień nie istnieje!\n"
#define MSGTR_CantOpenDumpfile "Nie mogę otworzyć pliku dump.\n"
#define MSGTR_CoreDumped "Zrzut pamięci ;)\n"
#define MSGTR_FPSnotspecified "Brak wartości FPS w nagłówku lub jest ona nieprawidłowa, użyj opcji -fps <ilość_klatek>.\n"
#define MSGTR_TryForceAudioFmtStr "Próbuję wymusić kodek audio z rodziny %s...\n"
#define MSGTR_CantFindAudioCodec "Nie mogę odnaleźć kodeka audio 0x%X.\n"
#define MSGTR_RTFMCodecs "Zobacz DOCS/HTML/en/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Próbuję wymusić kodek video z rodziny %s...\n"
#define MSGTR_CantFindVideoCodec "Nie mogę odnaleźć kodeka video pasującego do wybranego -vo i formatu video 0x%X.\n"
#define MSGTR_CannotInitVO "Błąd krytyczny: Nie mogę uruchomić sterownika video.\n"
#define MSGTR_CannotInitAO "Nie mogę otworzyć/zainicjować urządzenia audio -> brak dźwięku.\n"
#define MSGTR_StartPlaying "Odtwarzam...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"           ************************************************\n"\
"           ********* Twój komputer jest za słaby ! ********\n"\
"           ************************************************\n\n"\
"Prawdopodobne przyczyny, problemy, rozwiązania:\n"\
"- Najczęściej: wadliwe/błędne sterowniki _audio_ \n"\
"  - Spróbuj -ao sdl lub użyj emulacji OSS w ALSA.\n"\
"  - Poeksperymentuj z różnymi wartościami -autosync, '30' to dobry początek.\n"\
"- Wolne odtwarzanie obrazu \n"\
"  - Spróbuj innego sterownika -vo (lista: -vo help) lub spróbuj -framedrop!\n"\
"- Wolny procesor \n"\
"  - Nie próbuj odtwarzać dużych DVD/filmów DivX na wolnym procesorze! spróbuj lavdopts,\n"\
"    np. -vfm ffmpeg -lavdopts lowres=1:fast:skiploopfilter=all.\n"\
"- Zepsuty plik\n"\
"  - Spróbuj różnych kombinacji -nobps -ni -forceidx -mc 0.\n"\
"- Wolne źródło (zasoby NFS/SMB , DVD, VCD itp)\n"\
"  - Spróbuj -cache 8192.\n"\
"- Czy używasz opcji -cache do odtwarzania plików AVI bez przeplotu?\n"\
"  - Spróbuj -nocache.\n"\
"Zobacz DOCS/HTML/en/video.html, znajdziesz tam wskazówki jak przyspieszyć odtwarzanie.\n"\
"Jeśli nie znalazłeś nic pomocnego zobacz DOCS/HTML/en/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer został skompilowany bez interfejsu graficznego.\n"
#define MSGTR_GuiNeedsX "Interfejs graficzny MPlayer wymaga X11.\n"
#define MSGTR_Playing "\nOdtwarzam %s.\n"
#define MSGTR_NoSound "Audio: brak dźwięku\n"
#define MSGTR_FPSforced "Wymuszono FPS na %5.3f  (ftime: %5.3f).\n"
#define MSGTR_CompiledWithRuntimeDetection "Skompilowane z wykrywaniem procesora przy starcie.\n"
#define MSGTR_CompiledWithCPUExtensions "Skompilowano dla procesora x86 z rozszerzeniami:"
#define MSGTR_AvailableVideoOutputDrivers "Dostępne wyjściowe sterowniki video:\n"
#define MSGTR_AvailableAudioOutputDrivers "Dostępne wyjściowe sterowniki audio:\n"
#define MSGTR_AvailableAudioCodecs "Dostępne kodeki audio :\n"
#define MSGTR_AvailableVideoCodecs "Dostępne kodeki video :\n"
#define MSGTR_AvailableAudioFm "Dostępne (wkompilowane) kodeki/sterowniki audio:\n"
#define MSGTR_AvailableVideoFm "Dostępne (wkompilowane) kodeki/sterowniki video:\n"
#define MSGTR_AvailableFsType "Dostępne tryby pełnoekranowe:\n"
#define MSGTR_UsingRTCTiming "Używam sprzętowego zegara RTC (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Wideo: Nie mogę wczytać właściwości.\n"
#define MSGTR_NoStreamFound "Brak strumienia.\n"
#define MSGTR_ErrorInitializingVODevice "Błąd przy otwieraniu/inicjalizacji wybranego urządzenia video (-vo).\n"
#define MSGTR_ForcedVideoCodec "Wymuszam kodek video: %s\n"
#define MSGTR_ForcedAudioCodec "Wymuszam kodek audio: %s\n"
#define MSGTR_Video_NoVideo "Wideo: brak obrazu\n"
#define MSGTR_NotInitializeVOPorVO "\nBłąd krytyczny: Nie mogę zainicjalizować filtrów (-vf) lub wyjścia video (-vo).\n"
#define MSGTR_Paused "\n  =====  PAUZA  =====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nNie mogę wczytać listy odtwarzania %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer zakończył prace z powodu błędu 'Niedozwolona Instrukcja'.\n"\
"  To może być błąd w naszym nowym kodzie detekcji procesora przy starcie...\n"\
"  Proszę zobacz DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer zakończył pracę z powodu błędu 'Niedozwolona Instrukcja'.\n"\
"  To zdarza sie w wypadku gdy MPlayer jest uruchamiany na innym procesorze niż został skompilowany/zoptymalizowany\n"\
"  Sprawdź to!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer zakończył pracę z powodu złego użycia CPU/FPU/RAM.\n"\
"  Przekompiluj MPlayera z opcja --enable-debug i wykonaj śledzenie z 'gdb' oraz \n"\
"  zdisassembluj. Szczegóły w DOCS/HTML/pl/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer zakończył pracę. To nie powinno się zdarzyć.\n"\
"  To może być błąd w kodzie MPlayer LUB twoich sterownikach LUB twojej\n"\
"  wersji gcc. Jeśli uważasz, że to wina MPlayera, przeczytaj proszę\n"\
"  DOCS/HTML/en/bugreports.html i postępuj zgodnie z zawartymi tam instrukcjami. Nie jesteśmy\n"\
"  w stanie pomóc dopóki nie podasz tych danych przy informowaniu o możliwym błędzie.\n"
#define MSGTR_LoadingConfig "Wczytuję konfigurację '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: Dodaje plik z napisami (%d): %s\n"
#define MSGTR_RemovedSubtitleFile "SUB: Usuwam plik z napisami (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Błąd przy otwieraniu pliku [%s] do zapisu!\n"
#define MSGTR_CommandLine "WierszPoleceń:"
#define MSGTR_RTCDeviceNotOpenable "Nie moge otworzyć %s: %s (użytkownik powinien mieć prawo odczytu.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Błąd RTC Linuxa w ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Spróbuj dodać \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" do skryptów startowych swojego systemu.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Błąd RTC Linuxa w ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Używam synchronizacji %s.\n"
#define MSGTR_NoIdleAndGui "Opcja -idle nie może być używana z GMPlayer.\n"
#define MSGTR_MenuInitialized "Menu zainicjowane: %s\n"
#define MSGTR_MenuInitFailed "Nie mogę zainicjować menu.\n"
#define MSGTR_Getch2InitializedTwice "UWAGA: getch2_init wywołany dwukrotnie!\n"
#define MSGTR_DumpstreamFdUnavailable "Nie mogę zrzucić strumienia - brak deskryptora pliku\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Nie mogę otworzyć filtru video libmenu z głownym menu %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Błąd preinicjalizacji łańcucha filtru audio!\n"
#define MSGTR_LinuxRTCReadError "Błąd odczytu RTC Linuxa : %s\n"
#define MSGTR_SoftsleepUnderflow "Uwaga! niedomiar softsleep !\n"
#define MSGTR_DvdnavNullEvent "Zdarzenie DVDNAV NULL?!\n"
#define MSGTR_DvdnavHighlightEventBroken "Zdarzenie DVDNAV: Zepsute podświetlenie\n"
#define MSGTR_DvdnavEvent "Zdarzenie DVDNAV: %s\n"
#define MSGTR_DvdnavHighlightHide "Zdarzenie DVDNAV: Podświetlenie ukryte\n"
#define MSGTR_DvdnavStillFrame "######################################## Zdarzenie DVDNAV: Zatrzymana klatka: %d sek\n"
#define MSGTR_DvdnavNavStop "Zdarzenie DVDNAV: Nav Stop\n"
#define MSGTR_DvdnavNavNOP "Zdarzenie DVDNAV: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "Zdarzenie DVDNAV: Zmiana strumienia Nav SPU: fiz: %d/%d/%d log: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "Zdarzenie DVDNAV: Zmiana strumienia Nav SPU: fiz: %d log: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "Zdarzenie DVDNAV: Zmiana strumienia dźwiękowego Nav: fiz: %d log: %d\n"
#define MSGTR_DvdnavNavVTSChange "Zdarzenie DVDNAV: Zmiana Nav VTS\n"
#define MSGTR_DvdnavNavCellChange "Zdarzenie DVDNAV: Zmiana komórki Nav\n"
#define MSGTR_DvdnavNavSpuClutChange "Zdarzenie DVDNAV: Zmiana Nav SPU CLUT\n"
#define MSGTR_DvdnavNavSeekDone "Zdarzenie DVDNAV: Przeszukiwanie Nav zakończone\n"
#define MSGTR_MenuCall "Wywołanie menu\n"

#define MSGTR_EdlOutOfMem "Nie mogę zaalokowac wystarczająco pamięci na dane EDL.\n"
#define MSGTR_EdlRecordsNo "Wczytuję akcje %d.\n"
#define MSGTR_EdlQueueEmpty "Nie ma akcji EDL.\n"
#define MSGTR_EdlCantOpenForWrite "Nie mogę otworzyć pliku EDL [%s] do zapisu.\n"
#define MSGTR_EdlCantOpenForRead "Nie mogę otworzyć pliku EDL [%s] do odczytu.\n"
#define MSGTR_EdlNOsh_video "Nie moge użyć EDL bez obrazu, wyłączam.\n"
#define MSGTR_EdlNOValidLine "Błędny wiersz EDL: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Źle sformatowany wiersz EDL [%d], pomijam.\n"
#define MSGTR_EdlBadLineOverlap "Ostatnia pozycja stop [%f]; następny start to [%f].\n"\
"Wpisy muszą być w porządku chronologicznym, nie mogą się nakładać. Pomijam.\n"
#define MSGTR_EdlBadLineBadStop "Stop musi się znaleźć za czasem startu.\n"
#define MSGTR_EdloutBadStop "Pominięcie EDL odwołane, ostatni start > stop\n"
#define MSGTR_EdloutStartSkip "Pominięcie EDL rozpoczęte, wciśnij ponownie 'i' by zakończyć blok.\n"
#define MSGTR_EdloutEndSkip "Pominięcie EDL zakończone, wiersz zapisany.\n"
#define MSGTR_MPEndposNoSizeBased "Opcja MPlayer -endpos nie obsługuje jeszcze jednostek rozmiaru.\n"

// mplayer.c OSD

#define MSGTR_OSDenabled "włączony"
#define MSGTR_OSDdisabled "wyłączony"
#define MSGTR_OSDAudio "Audio: %s"
#define MSGTR_OSDChannel "Kanał: %s"
#define MSGTR_OSDSubDelay "Opóźnienie napisów: %d ms"
#define MSGTR_OSDSpeed "Prędkość: x %6.2f"
#define MSGTR_OSDosd "OSD: %s"
#define MSGTR_OSDChapter "Rozdział: (%d) %s"

// property values
#define MSGTR_Enabled "włączony"
#define MSGTR_EnabledEdl "włączony (EDL)"
#define MSGTR_Disabled "wyłączony"
#define MSGTR_HardFrameDrop "mocne"
#define MSGTR_Unknown "nieznany"
#define MSGTR_Bottom "dół"
#define MSGTR_Center "środek"
#define MSGTR_Top "góra"

// osd bar names
#define MSGTR_Volume "Głośność"
#define MSGTR_Panscan "Panscan"
#define MSGTR_Gamma "Gamma"
#define MSGTR_Brightness "Jasność"
#define MSGTR_Contrast "Kontrast"
#define MSGTR_Saturation "Nasycenie"
#define MSGTR_Hue "Barwa"

// property state
#define MSGTR_MuteStatus "Wycisz: %s"
#define MSGTR_AVDelayStatus "Opóźnienie A-V: %s"
#define MSGTR_OnTopStatus "Zawsze na wierzchu: %s"
#define MSGTR_RootwinStatus "Okno główne: %s"
#define MSGTR_BorderStatus "Ramka: %s"
#define MSGTR_FramedroppingStatus "Pomijanie klatek: %s"
#define MSGTR_VSyncStatus "Synchronizacja pionowa: %s"
#define MSGTR_SubSelectStatus "Napisy: %s"
#define MSGTR_SubPosStatus "Pozycja napisów: %s/100"
#define MSGTR_SubAlignStatus "Wyrównanie napisów: %s"
#define MSGTR_SubDelayStatus "Opóźnienie napisów: %s"
#define MSGTR_SubVisibleStatus "Napisy: %s"
#define MSGTR_SubForcedOnlyStatus "Tylko wymuszone napisy: %s"

// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Używam pliku kontrolnego pass3: %s\n"
#define MSGTR_MissingFilename "\nBrak nazwy pliku.\n\n"
#define MSGTR_CannotOpenFile_Device "Nie mogę otworzyć pliku/urządzenia.\n"
#define MSGTR_CannotOpenDemuxer "Nie mogę otworzyć demuxera.\n"
#define MSGTR_NoAudioEncoderSelected "\nNie wybrano kodera audio (-oac). Wybierz jeden (zobacz -oac help) lub użyj -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nNie wybrano kodera video (-ovc). Wybierz jeden (zobacz -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Nie moge otworzyć pliku wyjściowego '%s'.\n"
#define MSGTR_EncoderOpenFailed "Nie moge otworzyć kodera.\n"
#define MSGTR_MencoderWrongFormatAVI "\nUWAGA: WYJŚCIOWY FORMAT PLIKU TO _AVI_. Zobacz -of help.\n"
#define MSGTR_MencoderWrongFormatMPG "\nUWAGA: WYJŚCIOWY FORMAT PLIKU TO _MPEG_. Zobacz -of help.\n"
#define MSGTR_MissingOutputFilename "Nie podano pliku wyjściowego, proszę sprawdź opcję -o."
#define MSGTR_ForcingOutputFourcc "Wymuszam wyjście FourCC na %x [%.4s].\n"
#define MSGTR_ForcingOutputAudiofmtTag "Wymuszam znacznik wyjściowego formatu audio na 0x%x.\n"
#define MSGTR_DuplicateFrames "\n%d powtórzona(e) klatka(i)!\n"
#define MSGTR_SkipFrame "\nPomijam klatke!\n"
#define MSGTR_ResolutionDoesntMatch "\nNowy film ma inną rozdzielczość lub przestrzeń kolorów od poprzedniego.\n"
#define MSGTR_FrameCopyFileMismatch "\nWszystkie pliki video muszą mieć identyczne fps, rozdzielczość, i kodek przy użyciu -ovc copy.\n"
#define MSGTR_AudioCopyFileMismatch "\nWszystkie pliki muszą mieć identyczny kodek audio oraz format przy użyciu -oac copy.\n"
#define MSGTR_NoAudioFileMismatch "\nNie moge łączyć plików z samym obrazem oraz audio+video. Spróbuj -nosound.\n"
#define MSGTR_NoSpeedWithFrameCopy "UWAGA: opcja -speed może nie działać poprawnie z -oac copy!\n"\
"Kodowanie może nie działać poprawnie!\n"
#define MSGTR_ErrorWritingFile "%s: Błąd zapisu pliku.\n"
#define MSGTR_RecommendedVideoBitrate "Zalecane video bitrate dla %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nStrumień video: %8.3f kbit/s  (%d B/s)  rozmiar: %"PRIu64" bajtów  %5.3f sekund  %d klatek\n"
#define MSGTR_AudioStreamResult "\nStrumień audio: %8.3f kbit/s  (%d B/s)  rozmiar: %"PRIu64" bajtów  %5.3f sekund\n"
#define MSGTR_OpenedStream "sukces: format: %d  dane: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "kodek video: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "kodek audio: framecopy (format=%x chans=%d rate=%d bits=%d B/s=%d sample-%d)\n"
#define MSGTR_CBRPCMAudioSelected "Wybrano dźwięk CBR PCM.\n"
#define MSGTR_MP3AudioSelected "Wybrano dźwięk MP3.\n"
#define MSGTR_CannotAllocateBytes "Nie mogę zaalokować %d bajtów.\n"
#define MSGTR_SettingAudioDelay "Ustawiam opóźnienie audio na %5.3fs.\n"
#define MSGTR_SettingVideoDelay "Ustawiam opóźnienie video na %5.3fs.\n"
#define MSGTR_SettingAudioInputGain "Ustawiam wzmocnienie wejścia dźwięku na %f.\n"
#define MSGTR_LamePresetEquals "\nustawienie=%s\n\n"
#define MSGTR_LimitingAudioPreload "Ograniczam buforowanie audio do 0.4s.\n"
#define MSGTR_IncreasingAudioDensity "Zwiększam gęstość audio do 4.\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Wymuszam buforowanie audio do 0, max korekcja pts do 0.\n"
#define MSGTR_CBRAudioByterate "\n\nCBR audio: %d bajtów/sek, %d bajtów/blok\n"
#define MSGTR_LameVersion "wersja kodeka LAME  %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Błąd: Wybrany bitrate jest poza poprawnym zakresem dla tego ustawienia.\n"\
"\n"\
"Przy używaniu tego trybu musisz podać wartość pomiędzy \"8\" a \"320\".\n"\
"\n"\
"By uzyskać dalsze informacje zobacz: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Błąd: Nie podałeś poprawnych opcji/profilu dla ustawienia.\n"\
"\n"\
"Dostępne profile:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - Tryb ABR jest domyślny. Aby go użyć\n"\
"                      wystarczy sprecyzować bitrate. Na przykład:\n"\
"                      \"preset=185\" aktywuje to ustawienie\n"\
"                      i używa 185 jako średniego kbps.\n"\
"\n"\
"    Przykłady:\n"\
"\n"\
"     \"-lameopts fast:preset=standard  \"\n"\
" lub \"-lameopts  cbr:preset=192       \"\n"\
" lub \"-lameopts      preset=172       \"\n"\
" lub \"-lameopts      preset=extreme   \"\n"\
"\n"\
"By uzyskać dalsze informacje wpisz: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Zestawy ustawień zaprojektowane są w celu uzyskania jak najwyższej jakości.\n"\
"\n"\
"W tym celu były poddawane rygorystycznym testom i dopracowywane w ich trakcie.\n"\
"\n"\
"Są one bez przerwy aktualizowane, aby nadążyć za najświeższymi nowinkami\n"\
"co powinno przynosić prawie najwyższą osiągalną w LAME jakość.\n"\
"\n"\
"Aby aktywować te ustawienia:\n"\
"\n"\
"   Dla trybów VBR (zazwyczaj najlepsza jakość):\n"\
"\n"\
"     \"preset=standard\" To ustawienie powinno być przeźroczyste\n"\
"                             dla większości ludzi przy odtwarzaniu muzyki\n"\
"                             i od razu jest w niezłej jakości.\n"\
"\n"\
"     \"preset=extreme\" Jeśli masz bardzo dobry słuch i równie dobry sprzęt,\n"\
"                             to ustawienie daje trochę lepszą jakość niż \n"\
"                             tryb \"standard\".\n"\
"\n"\
"   Dla trybu CBR 320kbps (najwyższa możliwa jakość ze wszystkich możliwych ustawień):\n"\
"\n"\
"     \"preset=insane\"  To ustawienie będzie przesadą\n"\
"                             dla większości ludzi w większości przypadków,\n"\
"                             ale jeżeli musisz mieć najwyższą jakość niezależnie\n"\
"                             od wielkości pliku, to jest właściwa droga.\n"\
"\n"\
"   Dla trybów ABR (wysoka jakość z ustalonym bitrate, ale nie tak wysoka jak VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Podanie tego parametru da Ci dobrą jakość\n"\
"                             przy ustalonym bitrate. Opierając się na niej,\n"\
"                             określi ono optymalne ustawienia dla danej sytuacji.\n"\
"                             Niestety nie jest ono tak elastyczne jak VBR i przeważnie"\
"                             nie zapewni takiego samego poziomu jakości jak VBR\n"\
"                             przy wyższych wartościach bitrate.\n"\
"\n"\
"Poniższe opcje są również dostępne dla odpowiadających profili:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - Tryb ABR jest domyślny. Aby go użyć\n"\
"                      wystarczy sprecyzować bitrate. Na przykład:\n"\
"                      \"preset=185\" aktywuje to ustawienie\n"\
"                      i używa 185 jako średniego kbps.\n"\
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
"Dla trybu ABR dostępnych jest kilka synonimów:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit \
"Nie mogę ustawić opcji LAME, sprawdź bitrate/częstotliwość próbkowania,\n"\
"niektóre bardzo niskie bitrate (<32) wymagają niższych częstotliwości\n"\
"próbkowania (np. -srate 8000).\n"\
"Spróbuj wbudowanego ustawienia, jesli wszystko inne zawiedzie."
#define MSGTR_ConfigFileError "błąd pliku konfiguracyjnego"
#define MSGTR_ErrorParsingCommandLine "błąd w przetwarzaniu wiersza poleceń"
#define MSGTR_VideoStreamRequired "Wymagany jest strumień video!\n"
#define MSGTR_ForcingInputFPS "Wejściowa wartość FPS zostanie zinterpretowana jako %5.3f.\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Wyjściowy format RAWVIDEO nie obsługuje dźwięku - wyłączam dźwięk.\n"
#define MSGTR_DemuxerDoesntSupportNosound "Ten demuxer jeszcze nie obsługuje opcji -nosound.\n"
#define MSGTR_MemAllocFailed "Alokacja pamięci nie powiodła się.\n"
#define MSGTR_NoMatchingFilter "Nie mogę znaleźć pasującego formatu ao/filtra!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, zepsuty kompilator C?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, brakuje nazwy kodeka!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, nie mogę znaleźć kodera dla kodeka %s.\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, nie mogę zaalokować treści!\n"
#define MSGTR_CouldntOpenCodec "Nie udało się otworzyć kodeka %s, br=%d.\n"
#define MSGTR_CantCopyAudioFormat "Format dźwięku 0x%x nie jest kompatybilny z '-oac copy', spróbuj zamiast niego '-oac pcm' lub użyj '-fafmttag' by go wymusić.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     tryby zmiennego bitrate\n"\
"                0: cbr (stały bitrate)\n"\
"                1: mt (algorytm VBR Marka Taylora)\n"\
"                2: rh (algorytm VBR Roberta Hegemanna VBR - domyślny)\n"\
"                3: abr (średni bitrate)\n"\
"                4: mtrh (algorytm VBR Marka Taylora & Roberta Hegemanna)\n"\
"\n"\
" abr           średni bitrate\n"\
"\n"\
" cbr           stały bitrate\n"\
"               Wymusza także tryb CBR w następujących po tej opcji ustawieniach ABR\n"\
"\n"\
" br=<0-1024>   podaj bitrate w kilobitach (tylko CBR i ABR)\n"\
"\n"\
" q=<0-9>       jakość (0-najwyższa, 9-najniższa) (tylko dla VBR)\n"\
"\n"\
" aq=<0-9>      jakość algorytmu (0-najlepsza/najwolniejsza, 9-najgorsza/najszybsza)\n"\
"\n"\
" ratio=<1-100> wskaźnik kompresji\n"\
"\n"\
" vol=<0-10>    ustaw wzmocnienie dźwięku wejściowego\n"\
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
"                2: dostosuj\n"\
"\n"\
" fast          Ustaw szybsze kodowanie na następnych ustawieniach VBR,\n"\
"		nieznacznie niższa jakość i wyższy bitrate.\n"\
"\n"\
" preset=<value> Ustaw możliwie najwyższe ustawienia jakości.\n"\
"                 medium:  kodowanie VBR,  dobra jakość\n"\
"                 (bitrate: 150-180 kbps)\n"\
"                 standard:kodowanie VBR, wysoka jakość\n"\
"                 (bitrate: 170-210 kbps)\n"\
"                 extreme: kodowanie VBR, bardzo wysoka jakość\n"\
"                 (bitrate: 200-240 kbps)\n"\
"                 insane:  kodowanie CBR, najwyższa jakość\n"\
"                 (bitrate: 320 kbps)\n"\
"                 <8-320>: kodowanie ABR używając podanej wartości bitrate.\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "zduplikowany FourCC"
#define MSGTR_TooManyFourccs "zbyt dużo FourCC/formatów..."
#define MSGTR_ParseError "błąd składni"
#define MSGTR_ParseErrorFIDNotNumber "błąd składni (ID formatu podane nieliczbowo?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "błąd składni (alis formatu ID podany nieliczbowo?)"
#define MSGTR_DuplicateFID "zduplikowane ID formatu"
#define MSGTR_TooManyOut "zbyt dużo wyjśc..."
#define MSGTR_InvalidCodecName "\nnieprawidłowa nazwa kodeka (%s)!\n"
#define MSGTR_CodecLacksFourcc "\nkodek(%s) nie ma FourCC/formatu!\n"
#define MSGTR_CodecLacksDriver "\nkodek(%s) nie ma sterownika!\n"
#define MSGTR_CodecNeedsDLL "\nkodek(%s) wymaga 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\nkodek(%s) wymaga 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Nie mogę zaalokować pamięci na komentarz. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "Wczytuję %s: "
#define MSGTR_CantOpenFileError "Nie mogę otworzyć '%s': %s\n"
#define MSGTR_CantGetMemoryForLine "Brak pamięci na 'line': %s\n"
#define MSGTR_CantReallocCodecsp "Nie mogę zaalokować ponownie '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "Nazwa kodeka '%s' nie jest unikalna."
#define MSGTR_CantStrdupName "Nie mogę strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Nie mogę strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Nie mogę strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Nie mogę strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "kodeki audio %d & video %d\n"
#define MSGTR_CodecDefinitionIncorrect "Kodek jest niepoprawnie zdefiniowany."
#define MSGTR_OutdatedCodecsConf "Plik codecs.conf jest za stary i niekompatybilny z tym wydaniem MPlayer!"

// fifo.c
#define MSGTR_CannotMakePipe "Nie mogę utworzyć PIPE!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Zapis znaleziony w lvl %d jest za stary: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "Opcja %s nie może być użyta w pliku konfiguracyjnym.\n"
#define MSGTR_InvalidCmdlineOption "Opcja %s nie może być używana z wiersza poleceń.\n"
#define MSGTR_InvalidSuboption "Błąd: opcja '%s' nie ma podopcji '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Błąd: podopcja '%s' opcji '%s' musi mieć parametr!\n"
#define MSGTR_MissingOptionParameter "Błąd: opcja '%s' musi mieć parametr!\n"
#define MSGTR_OptionListHeader "\n Nazwa                 Typ            Min        Max      Ogólnie  CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nRazem: %d opcji\n"
#define MSGTR_ProfileInclusionTooDeep "Ostrzeżenie: Profile inclusion too deep.\n"
//nope? 
#define MSGTR_NoProfileDefined "Brak zdefiniowanych profili.\n"
#define MSGTR_AvailableProfiles "Dostępne profile:\n"
#define MSGTR_UnknownProfile "Nieznany profil '%s'.\n"
#define MSGTR_Profile "Profil %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n Nazwa                 Typ            Min        Max\n\n"
#define MSGTR_TotalProperties "\nRazem: %d właściwości\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Nie znaleziono CD-ROMu '%s'.\n"
#define MSGTR_ErrTrackSelect "Błąd wybierania ścieżki VCD."
#define MSGTR_ReadSTDIN "Czytam ze standardowego wejścia...\n"
#define MSGTR_UnableOpenURL "Nie mogę otworzyć URL: %s\n"
#define MSGTR_ConnToServer "Połączony z serwerem: %s\n"
#define MSGTR_FileNotFound "Brak pliku: '%s'\n"

#define MSGTR_SMBInitError "Nie mogę zainicjować biblioteki libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Nie mogę otworzyć z sieci lokalnej (LAN): '%s'\n"
#define MSGTR_SMBNotCompiled "Brak wkompilowanej obsługi zasobów SMB.\n"

#define MSGTR_CantOpenDVD "Nie znaleziono DVD: %s (%s)\n"

// stream_dvd.c
#define MSGTR_NoDVDSupport "MPlayer został skompilowany bez obsługi DVD support, wychodzę.\n"
#define MSGTR_DVDnumTitles "Na DVD znajduje się %d tytył(ów).\n"
#define MSGTR_DVDinvalidTitle "Błędny numer tytułu DVD: %d\n"
#define MSGTR_DVDnumChapters "To DVD ma %d rozdziałów.\n"
#define MSGTR_DVDinvalidChapter "Błędny numer rozdziału DVD: %d\n"
#define MSGTR_DVDinvalidChapterRange "Niepoprawny zakres rozdziałów %s\n"
#define MSGTR_DVDinvalidLastChapter "Niepoprawny numer ostatniego rozdziału DVD: %d\n"
#define MSGTR_DVDnumAngles "To DVD ma %d kątów.\n"
#define MSGTR_DVDinvalidAngle "Niepoprawny numer kąta: %d\n"
#define MSGTR_DVDnoIFO "Nie mogę otworzyć pliku IFO dla DVD %d.\n"
#define MSGTR_DVDnoVMG "Nie mogę otworzyć informacji VMG!\n"
#define MSGTR_DVDnoVOBs "Nie mogę otworzyć tytułu VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDnoMatchingAudio "Brak pasującej ścieżki dźwiękowej!\n"
#define MSGTR_DVDaudioChannel "Wybrano ścieżkę dźwiękową : %d w języku: %c%c\n"
#define MSGTR_DVDaudioStreamInfo "strumień audio: %d format: %s (%s) język: %s aid: %d.\n"
//aid?
#define MSGTR_DVDnumAudioChannels "ilośc ścieżek dźwiękowych na płycie: %d.\n"
#define MSGTR_DVDnoMatchingSubtitle "Brak pasujacych napisów!\n"
#define MSGTR_DVDsubtitleChannel "Wybrano napisy DVD: %d w języku: %c%c\n"
#define MSGTR_DVDsubtitleLanguage "napisy ( sid ): %d język: %s\n"
#define MSGTR_DVDnumSubtitles "Ilość napisów: %d\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "Za dużo strumieni!"
#define MSGTR_RawMuxerOnlyOneStream "Mixer Rawaudio obsługuje tylko jeden strumień!\n"
#define MSGTR_IgnoringVideoStream "Ignoruję strumień video!\n"
#define MSGTR_UnknownStreamType "Uwaga, nieznany typ strumienia: %d\n"
#define MSGTR_WarningLenIsntDivisible "Uwaga, len nie dzieli sie przez wielkość próbki!\n"
#define MSGTR_MuxbufMallocErr "Bufor ramek muxer nie może zaalokować pamięci!\n"
#define MSGTR_MuxbufReallocErr "Bufor ramek muxer nie może realokować pamięci!\n"
#define MSGTR_MuxbufSending "Bufor ramek muxer wysyła %d ramek.\n"
#define MSGTR_WritingHeader "Zapisuję nagłówek...\n"
#define MSGTR_WritingTrailer "Zapisuję index...\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "UWAGA: Nagłówek strumienia audio %d zredefinowany.\n"
#define MSGTR_VideoStreamRedefined "UWAGA: Nagłówek strumienia video %d zredefinowany.\n"
#define MSGTR_TooManyAudioInBuffer "\nZa dużo pakietów audio w buforze: (%d w %d bajtach).\n"
#define MSGTR_TooManyVideoInBuffer "\nZa dużo pakietów video w buforze: (%d w %d bajtach).\n"
#define MSGTR_MaybeNI "Może odtwarzasz strumień/plik bez przeplotu (non-interleaved) albo kodek nie działa?\n" \
		      "Spróbuj wymusić tryb bez przeplotu dla plików AVI opcją -ni.\n"
#define MSGTR_SwitchToNi "\nWykryłem plik AVI z błędnym przeplotem - przełączam na tryb -ni...\n"
#define MSGTR_Detected_XXX_FileFormat "Wykryto format pliku %s.\n"
#define MSGTR_DetectedAudiofile "Wykryto plik audio.\n"
#define MSGTR_NotSystemStream "Strumień nie w formacie MPEG... (może Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Błędny strumień MPEG-ES??? Skontaktuj się z autorem, to może być błąd :(\n"
#define MSGTR_FormatNotRecognized "============ Niestety, ten format pliku jest nieobsługiwany =============\n"\
				  "=== Jeśli plik to AVI lub strumień ASF, MPEG proszę skontaktuj się z autorem! ===\n"
#define MSGTR_MissingVideoStream "Brak strumienia video.\n"
#define MSGTR_MissingAudioStream "Brak strumienia audio -> brak dźwięku.\n"
#define MSGTR_MissingVideoStreamBug "Brak strumienia video!? Skontaktuj się z autorem, to może być błąd :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Plik nie zawiera wybranego strumienia audio lub video.\n"

#define MSGTR_NI_Forced "Wymuszono"
#define MSGTR_NI_Detected "Wykryto"
#define MSGTR_NI_Message "%s Plik AVI BEZ PRZEPLOTU.\n"

#define MSGTR_UsingNINI "Używam uszkodzonego formatu pliku NON-INTERLEAVED AVI.\n"
#define MSGTR_CouldntDetFNo "Nie mogę określić liczby klatek (dla przeszukiwania bezwzględnego).\n"
#define MSGTR_CantSeekRawAVI "Nie moge przeszukiwać strumieni raw AVI. (Wymagany index, spróbuj przełącznika -idx.)\n"
#define MSGTR_CantSeekFile "Nie mogę przeszukiwać tego pliku.\n"

#define MSGTR_MOVcomprhdr "MOV: obsługiwanie skompresowanych nagłówków wymaga ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: UWAGA: Wykryto zmienny FourCC!?\n"
#define MSGTR_MOVtooManyTrk "MOV: UWAGA: za dużo ścieżek"
#define MSGTR_FoundAudioStream "==> Znalazłem strumień audio: %d\n"
#define MSGTR_FoundVideoStream "==> Znalazłem strumień video: %d\n"
#define MSGTR_DetectedTV "Wykryłem TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Błąd otwierania Ogg demuxer.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Poszukuję strumienia audio (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Nie mogę otworzyć strumienia audio: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nie mogę otworzyć strumienia z napisami: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Otwieranie demuxer'a audio nie powiodło się: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Otwieranie demuxer'a napisów nie powiodło się: %s\n"
#define MSGTR_TVInputNotSeekable "Nie można przeszukiwać wejścia TV! (Przeszukiwanie prawdopodobnie służy do zmiany kanałów ;)\n"
#define MSGTR_DemuxerInfoChanged "Informacje o demuxerze %s zmieniono na %s\n"
#define MSGTR_ClipInfo "Informacje o klipie:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 30000/1001fps wykryłem system NTSC, zmieniam liczbę klatek na sekundę.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 24000/1001fps wykryłem progresywną zawartość NTSC, zmieniam liczbę klatek na sekundę.\n"
// Progresywny to jakiśtam rodzaj zawartości, nie ma nic wspólnego z wzrastanem (zresztą czego?)
// Coś na temat było w encoding-guide.xml albo mencoder.xml

#define MSGTR_CacheFill "\rWypełnienie pamięci cache: %5.2f%% (%"PRId64" bajtów)   "
#define MSGTR_NoBindFound "Brak akcji dla klawisza '%s'."
#define MSGTR_FailedToOpen "Otwieranie %s nie powiodło się.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nie mogłem otworzyć kodeka.\n"
#define MSGTR_CantCloseCodec "Nie mogłem zamknąć kodeka.\n"

#define MSGTR_MissingDLLcodec "BŁĄD: Nie mogę otworzyć wymaganego kodeka DirectShow %s.\n"
#define MSGTR_ACMiniterror "Nie mogę wczytać/zainicjować kodeka audio Win32/ACM (brak pliku DLL ?).\n"
#define MSGTR_MissingLAVCcodec "Nie mogę odnaleźć kodeka '%s' w libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: BŁĄD KRYTYCZNY: Wykryłem koniec pliku podczas poszukiwania nagłówka sekwencji.\n"
#define MSGTR_CannotReadMpegSequHdr "BŁĄD KRYTYCZNY: Nie mogę czytać nagłówka sekwencji.\n"
#define MSGTR_CannotReadMpegSequHdrEx "BŁĄD KRYTYCZNY: Nie mogę czytać rozszerzenia nagłówka sekwencji.\n"
#define MSGTR_BadMpegSequHdr "MPEG: zły nagłówek sekwencji\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: złe rozszerzenie nagłówka sekwencji\n"

#define MSGTR_ShMemAllocFail "Nie mogę zaalokować pamięci dzielonej.\n"
#define MSGTR_CantAllocAudioBuf "Nie mogę zaalokować bufora wyjściowego audio.\n"

#define MSGTR_UnknownAudio "Brakuje formatu audio/nieznany -> brak dźwięku\n"

#define MSGTR_UsingExternalPP "[PP] Używam zewnętrznego filtra postprocessing, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Używam filtra postprocessing kodeka, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Atrybut video '%s' nie jest obsługiwany przez wybrany vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Zarządano rodziny kodeków video [%s] (vfm=%s) niedostępna.\nWłącz ją przy kompilacji.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Zarządano rodziny kodeków audio [%s] (afm=%s) niedostępna.\nWłącz ją przy kompilacji.\n"
#define MSGTR_OpeningVideoDecoder "Otwieram dekoder video: [%s] %s\n"
#define MSGTR_SelectedVideoCodec "Wybrany kodek video: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "Otwieram dekoder audio: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "Wybrany kodek audio: [%s] afm: %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "Tworzę łańcuch filtrów audio dla %dHz/%dch/%s -> %dHz/%dch/%s...\n"
#define MSGTR_UninitVideoStr "Deinicjalizacja video: %s\n"
#define MSGTR_UninitAudioStr "Deinicjalizacja audio: %s\n"
#define MSGTR_VDecoderInitFailed "Nie udało się zainicjowac VDecoder'a :(\n"
#define MSGTR_ADecoderInitFailed "Nie udało się zainicjowac ADecoder'a :(\n"
#define MSGTR_ADecoderPreinitFailed "Preinicjalizacja ADecoder'a nie powiodła się :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alokuję %d bajtów na bufor wejściowy.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alokuję %d + %d = %d bajtów na bufor wyjściowy.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Uruchamiam obsługę LIRC...\n"
#define MSGTR_LIRCopenfailed "Nie udało się uruchomić obsługi LIRC. Nie będziesz mogł używać swojego pilota.\n"
#define MSGTR_LIRCcfgerr "Nie udało się wczytać pliku konfiguracyjnego LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nie mogłem odnaleźć filtru video '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "Nie mogłem odnaleźć filtru audio '%s'.\n"
#define MSGTR_OpeningVideoFilter "Otwieram filtr video: "
#define MSGTR_CannotFindColorspace "Nie mogę znaleźć odpowiedniej przestrzenii kolorów, nawet poprzez wstawienie 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Kodek nie ustawił sh->disp_w i sh->disp_h, próbuję obejścia.\n"
#define MSGTR_VoConfigRequest "VDec: wymagana konfiguracja vo - %d x %d (preferowana przestrzeń kolorów: %s)\n"
#define MSGTR_CouldNotFindColorspace "Nie mogłem odnaleźć pasującej przestrzeni kolorów - próbuję ponownie z opcją -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Format filmu to %.2f:1 - zmieniam do poprawnego formatu filmu.\n"
#define MSGTR_MovieAspectUndefined "Format filmu nie zdefiniowany - nie stosuję zmiany formatu.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Musisz uaktualnić/zainstalować pakiet kodeków.\nZnajdziesz go na http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Inicjalizacja kodeka video Win32/DShow OK.\n"
#define MSGTR_DMOInitOK "INFO: Inicjalizacja kodeka video Win32/DMO OK.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Nie mogłem wysłać zdarzenia pełnoekranowego EWMH !\n"
#define MSGTR_CouldNotFindXScreenSaver "xscreensaver_disable: Nie mogłem odnaleźć okna XScreenSaver.\n"
#define MSGTR_SelectedVideoMode "XF86VM: Wybrano tryb video %dx%d dla obrazu %dx%d.\n"

#define MSGTR_InsertingAfVolume "[Mixer] Brak sprzętowego mixowania, włączam filtr głośności.\n"
#define MSGTR_NoVolume "[Mixer] Brak kontroli głośności.\n"

// ====================== GUI messages/buttons ========================

#ifdef CONFIG_GUI

// --- labels ---
#define MSGTR_About "O programie"
#define MSGTR_FileSelect "Wybierz plik..."
#define MSGTR_SubtitleSelect "Wybierz napisy..."
#define MSGTR_OtherSelect "Wybierz..."
#define MSGTR_AudioFileSelect "Wybierz zewnętrzny kanał dźwięku..."
#define MSGTR_FontSelect "Wybierz czcionkę..."
// Note: If you change MSGTR_PlayList please see if it still fits MSGTR_MENU_PlayList
#define MSGTR_PlayList "Lista Odtwarzania"
#define MSGTR_Equalizer "Korektor"
#define MSGTR_ConfigureEqualizer "Konfiguruj Korektor"
#define MSGTR_SkinBrowser "Przeglądarka skórek"
#define MSGTR_Network "Strumieniowanie sieciowe..."
// Note: If you change MSGTR_Preferences please see if it still fits MSGTR_MENU_Preferences
#define MSGTR_Preferences "Opcje"
#define MSGTR_AudioPreferences "Konfiguracja sterownika audio"
#define MSGTR_NoMediaOpened "Nie otwarto żadnego nośnika."
#define MSGTR_VCDTrack "Ścieżka VCD %d"
#define MSGTR_NoChapter "Brak rozdziału"
#define MSGTR_Chapter "Rozdział %d"
#define MSGTR_NoFileLoaded "Nie wczytano pliku."

// --- buttons ---
#define MSGTR_Ok "OK"
#define MSGTR_Cancel "Anuluj"
#define MSGTR_Add "Dodaj"
#define MSGTR_Remove "Usuń"
#define MSGTR_Clear "Wyczyść"
#define MSGTR_Config "Konfiguracja"
#define MSGTR_ConfigDriver "Skonfiguruj sterownik"
#define MSGTR_Browse "Przeglądaj"

// --- error messages ---
#define MSGTR_NEMDB "Za mało pamięci by wyrysować bufor."
#define MSGTR_NEMFMR "Za mało pamięci na renderowanie menu."
#define MSGTR_IDFGCVD "Brak sterownika video zgodnego z GUI."
#define MSGTR_NEEDLAVC "Niestety, nie można odtwarzać plików innych niż MPEG przy użyciu urządzenia DXR3/H+ bez kowersji.\nProszę włączyć lavc w polu sterowania DXR3/H+."
#define MSGTR_UNKNOWNWINDOWTYPE "Okno nieznanego typu ..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skórka] błąd w pliku konfiguracyjnym skórki, linia %d: %s"
#define MSGTR_SKIN_WARNING1 "[skórka] uwaga: w pliku konfiguracyjnym, wiersz %d:\nznacznik widget (%s) znaleziony lecz brak \"section\" przed nim"
#define MSGTR_SKIN_WARNING2 "[skórka] uwaga: w pliku konfiguracyjnym, wiersz %d:\nznacznik widget (%s) znaleziony lecz brak \"subsection\" przed nim"
#define MSGTR_SKIN_WARNING3 "[skórka] uwaga: w pliku konfiguracyjnym, wiersz %d:\nta podsekcja nie jest obsługiwana przez widget (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[skórka] plik ( %s ) nie znaleziony.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skórka] nie mogę odczytać pliku ( %s ).\n"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmapy o głębokości <=16 bitów nie są obsgługiwane (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Pliku nie znaleziono (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "Błąd odczytu BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "Błąd odczytu TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "Błąd odczytu PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "TGA spakowany RLE nie jest obsługiwany (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "nieznany typ pliku (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "Błąd konwersji 24 bitów do 32 bitów (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "nieznana wiadomość: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "za mało pamięci\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Zadeklarowano za dużo czcionek.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Nie znalazłem pliku z czcionką.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Nie znalazłem pliku z obrazem czcionki.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "Nieistniejący identyfikator czcionki (%s)\n"
#define MSGTR_SKIN_UnknownParameter "nieznany parametr (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skórka nie znaleziona (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "Wybrana skórka ( %s ) nie odnaleziona, próbuję 'default'...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "błąd odczytu pliku konfiguracyjnego skórki (%s)\n"
#define MSGTR_SKIN_LABEL "Skórki:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O MPlayerze"
#define MSGTR_MENU_Open "Otwórz..."
#define MSGTR_MENU_PlayFile "Odtwarzaj plik..."
#define MSGTR_MENU_PlayVCD "Odtwarzaj VCD..."
#define MSGTR_MENU_PlayDVD "Odtwarzaj DVD..."
#define MSGTR_MENU_PlayURL "Odtwarzaj URL..."
#define MSGTR_MENU_LoadSubtitle "Wczytaj napisy..."
#define MSGTR_MENU_DropSubtitle "Porzuć napisy..."
#define MSGTR_MENU_LoadExternAudioFile "Wczytaj zewnętrzny plik ścieżki dźwiękowej..."
#define MSGTR_MENU_Playing "Odtwarzanie"
#define MSGTR_MENU_Play "Odtwarzanie"
#define MSGTR_MENU_Pause "Pauza"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Następny strumień"
#define MSGTR_MENU_PrevStream "Poprzedni strumień"
#define MSGTR_MENU_Size "Rozmiar"
#define MSGTR_MENU_HalfSize   "połowa normalnego rozmiaru"
#define MSGTR_MENU_NormalSize "normalny rozmiar"
#define MSGTR_MENU_DoubleSize "podwójny rozmiar"
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
#define MSGTR_MENU_PlayList MSGTR_PlayList
#define MSGTR_MENU_SkinBrowser "Przeglądarka skórek"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "Wyjście..."
#define MSGTR_MENU_Mute "Wycisz"
#define MSGTR_MENU_Original "Oryginalnie"
#define MSGTR_MENU_AspectRatio "Format"
#define MSGTR_MENU_AudioTrack "Ścieżka Audio"
#define MSGTR_MENU_Track "Ścieżka %d"
#define MSGTR_MENU_VideoTrack "Ścieżka Wideo"
#define MSGTR_MENU_Subtitles "Napisy"

// --- equalizer
// Note: If you change MSGTR_EQU_Audio please see if it still fits MSGTR_PREFERENCES_Audio
#define MSGTR_EQU_Audio "Audio"
// Note: If you change MSGTR_EQU_Video please see if it still fits MSGTR_PREFERENCES_Video
#define MSGTR_EQU_Video "Wideo"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Jasność: "
#define MSGTR_EQU_Hue "Barwa: "
#define MSGTR_EQU_Saturation "Nasycenie: "
#define MSGTR_EQU_Front_Left "Lewy przedni"
#define MSGTR_EQU_Front_Right "Prawy przedni"
#define MSGTR_EQU_Back_Left "Lewy tylny"
#define MSGTR_EQU_Back_Right "Prawy tylny"
#define MSGTR_EQU_Center "Centralny"
#define MSGTR_EQU_Bass "Subwoofer(basowy)"
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
#define MSGTR_PLAYLIST_DirectoryTree "Drzewo katalogów"

// --- preferences
#define MSGTR_PREFERENCES_Audio MSGTR_EQU_Audio
#define MSGTR_PREFERENCES_Video MSGTR_EQU_Video
#define MSGTR_PREFERENCES_SubtitleOSD "Napisy & OSD"
#define MSGTR_PREFERENCES_Codecs "Kodeki & demuxer"
// Note: If you change MSGTR_PREFERENCES_Misc see if it still fits MSGTR_PREFERENCES_FRAME_Misc
#define MSGTR_PREFERENCES_Misc "Inne"

#define MSGTR_PREFERENCES_None "Brak"
#define MSGTR_PREFERENCES_DriverDefault "domyślne ustawienia sterownika"
#define MSGTR_PREFERENCES_AvailableDrivers "Dostępne sterowniki:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nie odtwarzaj dźwięku"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizuj dźwięk"
#define MSGTR_PREFERENCES_EnableEqualizer "Włącz korektor"
#define MSGTR_PREFERENCES_SoftwareMixer "Włącz mikser programowy"
#define MSGTR_PREFERENCES_ExtraStereo "Włącz dodatkowe stereo"
#define MSGTR_PREFERENCES_Coefficient "Współczynnik:"
#define MSGTR_PREFERENCES_AudioDelay "Opóźnienie dźwięku"
#define MSGTR_PREFERENCES_DoubleBuffer "Włącz podwójne buforowanie"
#define MSGTR_PREFERENCES_DirectRender "Włącz bezpośrednie renderowanie"
#define MSGTR_PREFERENCES_FrameDrop "Włącz pomijanie klatek"
#define MSGTR_PREFERENCES_HFrameDrop "Włącz pomijanie dużej ilości klatek (niebezpieczne)"
#define MSGTR_PREFERENCES_Flip "Odwróć obraz"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Czas i wskaźniki"
#define MSGTR_PREFERENCES_OSDProgress "Tylko belka"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Czas, czas w procentach i czas całkowity"
#define MSGTR_PREFERENCES_Subtitle "Napisy:"
#define MSGTR_PREFERENCES_SUB_Delay "Opóźnienie: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozycja: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Wyłącz automatycznie wczytywanie napisów"
#define MSGTR_PREFERENCES_SUB_Unicode "Napisy w Unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konwertuj wybrane napisy na format MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Konwertuj wybrane napisy na oparty na czasie format SubViewer (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Włącz nakładanie się napisów"
#define MSGTR_PREFERENCES_SUB_USE_ASS "Wyświetlanie napisów SSA/ASS"
#define MSGTR_PREFERENCES_SUB_ASS_USE_MARGINS "Używaj marginesów"
#define MSGTR_PREFERENCES_SUB_ASS_TOP_MARGIN "Górny: "
#define MSGTR_PREFERENCES_SUB_ASS_BOTTOM_MARGIN "Dolny: "
#define MSGTR_PREFERENCES_Font "Czcionka:"
#define MSGTR_PREFERENCES_FontFactor "Skala czcionki:"
#define MSGTR_PREFERENCES_PostProcess "Włącz postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Jakość automatyczna: "
#define MSGTR_PREFERENCES_NI "Użyj parsera dla pliku AVI bez przeplotu"
#define MSGTR_PREFERENCES_IDX "Odtwórz tablicę indexową, jeśli potrzebne"
#define MSGTR_PREFERENCES_VideoCodecFamily "Rodzina kodeków video:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Rodzina kodeków audio:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Poziom OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Napisy"
#define MSGTR_PREFERENCES_FRAME_Font "Czcionka"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Kodek & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Pamięć podręczna"
#define MSGTR_PREFERENCES_FRAME_Misc MSGTR_PREFERENCES_Misc
#define MSGTR_PREFERENCES_Audio_Device "Urządzenie:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Kanał Mixera:"
#define MSGTR_PREFERENCES_Message "Pamiętaj, że niektóre opcje działają dopiero po zrestartowaniu odtwarzania!"
#define MSGTR_PREFERENCES_DXR3_VENC "Koder video:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Użyj LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Języki zachodnioeuropejskie (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Języki zachodnioeuropejskie z Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Języki Słowiańskie/środkowoeuropejskie (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galicyjski, Maltański, Turecki (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Stary, bałtycki zestaw znaków (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cyrylica (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabski (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Współczesna Greka (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turecki (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Bałtycki (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celtycki (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Znaki hebrajskie (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Rosyjski (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukraiński, Białoruski (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Uproszczony chiński (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Tradycyjny chiński (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Znaki japońskie (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Znaki Koreańskie (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Znaki tajskie charset (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrylica Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Języki Słowiańskie/środkowoeuropejskie Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Bez autoskalowania"
#define MSGTR_PREFERENCES_FontPropWidth "Proporcjonalnie do szerokości filmu"
#define MSGTR_PREFERENCES_FontPropHeight "Proporcjonalnie do wysokości filmu"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporcjonalnie do przekątnej filmu"
#define MSGTR_PREFERENCES_FontEncoding "Kodowanie:"
#define MSGTR_PREFERENCES_FontBlur "Rozmycie:"
#define MSGTR_PREFERENCES_FontOutLine "Obramowanie:"
#define MSGTR_PREFERENCES_FontTextScale "Skala tekstu:"
#define MSGTR_PREFERENCES_FontOSDScale "Skala OSD:"
#define MSGTR_PREFERENCES_Cache "Pamięć podręczna wł/wył"
#define MSGTR_PREFERENCES_CacheSize "Wielkość pamięci podręcznej: "
#define MSGTR_PREFERENCES_LoadFullscreen "Rozpoczynaj w trybie pełnoekranowym"
#define MSGTR_PREFERENCES_SaveWinPos "Zapisz pozycję okna"
#define MSGTR_PREFERENCES_XSCREENSAVER "Wyłącz XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Włącz pasek odtwarzania"
#define MSGTR_PREFERENCES_AutoSync "Autosynchronizacja wł/wył"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosynchronizacja: "
#define MSGTR_PREFERENCES_CDROMDevice "Urządzenie CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Urządzenie DVD:"
#define MSGTR_PREFERENCES_FPS "Ilośc klatek na sekundę:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Pokaż okno video, gdy nieaktywne"
#define MSGTR_PREFERENCES_ArtsBroken "Nowsze wersje aRts nie są kompatybilne z "\
           "GTK 1.x i spowodują błąd GMPlayer!"

#define MSGTR_ABOUT_UHU "Rozwój GUI sponsorowany przez UHU Linux\n"
#define MSGTR_ABOUT_Contributors "Kod i osoby pomagające w tworzeniu dokumentacji\n"
#define MSGTR_ABOUT_Codecs_libs_contributions "Kodeki i inne obce biblioteki\n"
#define MSGTR_ABOUT_Translations "Tłumaczenia\n"
#define MSGTR_ABOUT_Skins "Skórki\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Błąd krytyczny!"
#define MSGTR_MSGBOX_LABEL_Error "Błąd!"
#define MSGTR_MSGBOX_LABEL_Warning "Uwaga!"

// bitmap.c

#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] za mało pamięci na obraz\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] za mało pamięci na obraz\n"

// cfg.c

#define MSGTR_ConfigFileReadError "[cfg] błąd wczytywania pliku konfiguracyjnego ...\n"
#define MSGTR_UnableToSaveOption "[cfg] Nie udało się zapisać opcji '%s'.\n"

// interface.c

#define MSGTR_DeletingSubtitles "[GUI] Usuwam napisy.\n"
#define MSGTR_LoadingSubtitles "[GUI] Wczytuję napisy: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] Dodaję filtr video: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] Usuwam filtr video: %s\n"

// mw.c

#define MSGTR_NotAFile "To nie wygląda na plik: %s !\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] Nie udało sie otworzyć ekranu.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Ekran zdalny, wyłączam XMITSHM.\n"
#define MSGTR_WS_NoXshm "[ws] Niestety Twój system nie obsługuje rozszerzeń dzielonej pamięci X.\n"
#define MSGTR_WS_NoXshape "[ws] Niestety Twój system nie obsługuje rozszerzenia XShape.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Niestety paleta kolorów jest za mała.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] Za dużo otwartych okien.\n"
#define MSGTR_WS_ShmError "[ws] błąd rozszerzenia pamięci dzielonej\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Za mało pamięci do wyrysowania bufora.\n"
#define MSGTR_WS_DpmsUnavailable "DPMS niedostępny?\n"
#define MSGTR_WS_DpmsNotEnabled "Nie mogłem włączyć DPMS.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "To nie wygląda jak plik...\n"
#define MSGTR_WS_DDNothing "D&D: Nic nie zwrócono!\n"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Wybrane urządzenie wyjścia video jest niekompatybilne z tym kodekiem.\n"\
                "Spróbuj dodac filtr scale, np. -vf spp,scale zamiast -vf spp.\n"
#define MSGTR_VO_GenericError "Wystąpił błąd"
#define MSGTR_VO_UnableToAccess "Brak dostępu"
#define MSGTR_VO_ExistsButNoDirectory "istnieje, ale nie jest folderem."
#define MSGTR_VO_DirExistsButNotWritable "Folder wyjściowy istnieje ale nie ma praw do zapisu."
#define MSGTR_VO_DirExistsAndIsWritable "Folder wyjściowy już istnieje i ma prawa zapisu."
#define MSGTR_VO_CantCreateDirectory "Nie mogę stworzyć folderu wyjściowego."
#define MSGTR_VO_CantCreateFile "Nie mogę stworzyć pliku wyjściowego."
#define MSGTR_VO_DirectoryCreateSuccess "Folder wyjściowy utworzony pomyślnie."
#define MSGTR_VO_ParsingSuboptions "Interpretuję podopcje."
#define MSGTR_VO_SuboptionsParsedOK "Pomyślnie zinterpretowano podopcje."
#define MSGTR_VO_ValueOutOfRange "wartość poza zakresem."
#define MSGTR_VO_NoValueSpecified "Nie podano wartości."
#define MSGTR_VO_UnknownSuboptions "nieznana podopcja(e)"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\nPodopcje aalib vo_aa to:\n"
#define MSGTR_VO_AA_AdditionalOptions "Dodatkowe opcje dostępne dla vo_aa :\n" \
"  help        wyświetla tę wiadomość\n" \
"  osdcolor    ustaw kolor OSD\n  subcolor    ustaw kolor napisów\n" \
"        parametry koloru:\n           0 : normalny\n" \
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
#define MSGTR_VO_PNM_PGMType "Zapiszę pliki PGM."
#define MSGTR_VO_PNM_PGMYUVType "Zapiszę pliki PGMYUV."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Tryb przeplotu wymaga aby wysokość obrazu była podzielna przez 4."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Nie mogę zaalokować bufora lini dla trybu przeplotu."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Wejście nie jest w formacie RGB, nie mogę oddzielić jasności przez pola!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "Szerokość obrazu musi być podzielna przez 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Za mało pamięci aby zaalokować bufor klatek RGB."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Nie mogę dostać pamięci lub pliku aby zapisać \"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Błąd zapisu pliku na wyjście!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Nieznane podurządzenie: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Używam wyjścia w trybie przeplotu, najwyższe pola pierwsze."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Używam wyjścia w trybie przeplotu, najniższe pola pierwsze."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Używam (domyślnego) trybu progresywnych klatek."
// sub.c
#define MSGTR_VO_SUB_Seekbar "Belka"
#define MSGTR_VO_SUB_Play "Odtwarzanie"
#define MSGTR_VO_SUB_Pause "Pauza"
#define MSGTR_VO_SUB_Stop "Stop"
#define MSGTR_VO_SUB_Rewind "Przewijanie do tyłu"
#define MSGTR_VO_SUB_Forward "Przewijanie do przodu"
#define MSGTR_VO_SUB_Clock "Zegar"
#define MSGTR_VO_SUB_Contrast "Kontrast"
#define MSGTR_VO_SUB_Saturation "Nasycenie"
#define MSGTR_VO_SUB_Volume "Głośność"
#define MSGTR_VO_SUB_Brightness "Jasność"
#define MSGTR_VO_SUB_Hue "Barwa"

// vo_xv.c
#define MSGTR_VO_XV_ImagedimTooHigh "Rozmiary obrazu źródłowego są za duże: %ux%u (maximum to %ux%u)\n"

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "Sterownik wyjścia video pgm został zastąpiony przez -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Sterownik wyjścia video md5 został zastąpiony przez -vo md5sum.\n"

// ======================= AO Audio Output drivers ========================

// libao2 

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: moduły alsa9 i alsa1x zostały usunięte, użyj w zamian -ao alsa.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Nie mogę otworzyć mixera %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: Mixer karty dźwiękowej nie ma kanału '%s', używam domyślnego.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Nie moge otworzyć urządzenia audio %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Nie mogę utworzyć deskryptora blokującego: %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] Nie mogę ustawić urządzenia audio %s na wyjście %s, próbuję %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Nie udało się ustawić urządzenia audio na %d kanałów.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: Sterownik nie obsługuje SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Twój sterownik dźwięku NIE OBSŁUGUJE select()  ***\n Przekompiluj MPlayera z opcją #undef HAVE_AUDIO_SELECT w config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nBłąd krytyczny: *** Nie mogę otworzyć ponownie/zresetowac urządzenia audio *** %s\n"
#define MSGTR_AO_OSS_UnknownUnsupportedFormat "[AO OSS] Nieznany/nieobsługiwany format OSS: %x.\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Połączony z serwerem dźwięku.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Nie mogę otworzyć strumienia.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Strumień otwarty.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] rozmiar bufora: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Ustawienie głośności na %d nie powiodło się.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] %d Hz nie obsługiwane, spróbuj zmienić jakość.\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound nie powiodło się: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] opóźnienie: [server: %0.2fs, net: %0.2fs] (dostosowanie %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] nie udało się otworzyć strumienia ESD: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] ustawienie mixer DVB nie powiodło się: %s.\n"
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz nie obsługiwane, spróbuj zmienić jakość.\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Plik: %s (%s)\nPCM: Częstotliwość próbkowania: %iHz Kanałów: %s Format %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: Szybsze zrzucanie można uzyskać używając -vc null -vo null -ao pcm:fast\n[AO PCM] Info: Żeby zapisać pliki WAVE użyj -ao pcm:waveheader (domyślne).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Nie udało się otworzyć %s do zapisu!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Częstotliwość próbkowania: %iHz Kanałów: %s Format %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] używam sterownika dźwięku %s.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Nieobsługiwany format dźwięku: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Inicjalizacja dźwięku SDL nie powiodła się: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Nie udało się otworzyć dźwięku: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] kontrola.\n" // Mam poważne podejrzenia że sterowanie. Ale bez pewności
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Częstotliwość próbkowania: %iHz Kanałów: %s Format %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] play: nieprawidłowe urządzenie.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparms nie powiodło się %s\nNie udało sie ustawić wybranej częstotliwości próbkowania.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE nie został zakceptowany przy podanym źródle.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams nie powiodło się: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: częstotliwość próbkowania: %lf (wymagana skala %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Nie udało się otworzyć kanału dźwięku: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO nie powiodło się.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: zapis nie powiódł się.\n"
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Nie mogę otworzyć urządzenia audio %s, %s  -> brak dźwięku.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: Twoja karta nie obsługuje %d kanałów, %s, częstotliwości próbkowania %d Hz.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Twój sterownik dźwięku NIE OBSŁUGUJE select()  ***\nPrzekompiluj MPlayer z opcją #undef HAVE_AUDIO_SELECT w config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nFatal error: *** Nie mogę otworzyć ponownie/zresetować urządzenia audio (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: żądany format: %d Hz, %d kanały, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: nie znaleziono żadnych kart dźwiękowych.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: żądanie niewłaściwego formatu (%s)a - wyjście wyłączone.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: błąd odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: bład PCM info: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: znalazłem %d kart dźwiękowych, używam: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: błąd PCM channel info: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: błąd ustawiania paremetrów: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: błąd ustawiania kanału: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: błąd przygotowywania kanału: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: błąd odsączania odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: błąd czyszczenia odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: błąd zamykania PCM: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: błąd odsączania odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: błąd czyszczenia odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: błąd przygotowywania kanału: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: błąd odsączania odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: błąd czyszczenia odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: błąd przygotowywania kanału: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: błąd alsa, resetuję strumień.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: błąd przygotowywania do odtwarzania: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: błąd zapisu po resecie: %s - przestaję.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: błąd wyjścia: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] nieprawidłowa wtyczka: %s\n"

// ======================= AF Audio Filters ================================

// libaf 

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "dostępne etykiety w"
#define MSGTR_AF_LADSPA_WarnNoInputs "UWAGA! Ta wtyczka LADSPA nie ma wejść dźwięku.\n  Wchodzący sygnał dźwiękowy zostanie utracony."
#define MSGTR_AF_LADSPA_ErrMultiChannel "Wtyczki wielokanałowe (>2) nie są (jeszcze) obsługiwane).\n  Używaj tylko wtyczek mono lub stereo."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Ta wtyczka LADSPA nie posiada wyjścia dźwieku."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Liczba wejść i wyjść dźwięku dla wtyczki LADSPA różni się."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "nie udało się wczytać"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Nie udało sie odnaleźć funkcji ladspa_descriptor() w podanej bibliotece."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Nie udało się odnaleźć etykiety w bibliotece wtyczek."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Nie podano podopcji."
#define MSGTR_AF_LADSPA_ErrNoLibFile "Nie podano pliku biblitek."
#define MSGTR_AF_LADSPA_ErrNoLabel "Nie podano etykiety filtru."
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "W wierszu poleceń podano niewystarczającą ilośc opcji."
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: Kontrola wejścia #%d jest poniżej dolnej granicy wynoszącej %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: Input control #%d jest powyżej górnej granicy wynoszącej %0.4f.\n"

// format.c

#define MSGTR_AF_FORMAT_UnknownFormat "nieznany format "

// ========================== INPUT =========================================

// joystick.c

#define MSGTR_INPUT_JOYSTICK_Opening "Otwieram joystick %s\n"
#define MSGTR_INPUT_JOYSTICK_CantOpen "Nie mogę otworzyć dźojstika %s: %s\n"
#define MSGTR_INPUT_JOYSTICK_ErrReading "Błąd odczytu dźojstika: %s\n"
#define MSGTR_INPUT_JOYSTICK_LoosingBytes "Joystick: Tracę %d bajtów danych\n"
#define MSGTR_INPUT_JOYSTICK_WarnLostSync "Joystick: ostrzeżenie inicjalizacji, straciłem synchronizację ze sterownikiem.\n"
#define MSGTR_INPUT_JOYSTICK_WarnUnknownEvent "Ostrzeżenie. Nieznane zdarzenie joysticka %d\n"

// input.c

#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "Za dużo deskryptorów pliku, nie mogę zarejestrować dekryptora pliku %d.\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "Za dużo kluczowych dekryptorów pliku, nie mogę zarejestrować deskryptora pliku %d.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "Polecenie %s: argument %d nie jest liczbą całkowitą.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "Polecenie %s: argument %d nie jest liczbą zmiennoprzecinkową.\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "Polecenie %s: argument %d nie został zakończony.\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "Nieznany argument %d\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "Polecenie %s wymaga co najmniej %d argumentów, podano tylko %d.\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "Błąd podczas odczytu deskryptora pliku %d: %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "Bufor poleceń lub deskryptorów pliku %d jest pełny: porzucam zawartość.\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "Nieprawidłowe polecenie dla klawisza %s"
#define MSGTR_INPUT_INPUT_ErrSelect "Błąd wyboru: %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "Błąd na wejściu deskryptora pliku %d\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "Błąd klawisza wejściowego na deskryptorze pliku %d\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "Za dużo klawiszy wciśniętych jednocześnie.\n"
#define MSGTR_INPUT_INPUT_ErrOnCmdFd "Błąd polecenia deskryptora pliku %d\n"
#define MSGTR_INPUT_INPUT_ErrReadingInputConfig "Błąd odczytu wejściowego pliku konfiguracyjnego %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrUnknownKey "Nieznany klawisz '%s'\n"
#define MSGTR_INPUT_INPUT_ErrUnfinishedBinding "Niedokończone wiązanie %s\n"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForKeyName "Bufor jest za mały na nazwe tego klawisza: %s\n"
#define MSGTR_INPUT_INPUT_ErrNoCmdForKey "Brak polecenia przypisanego do klawisza %s"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForCmd "Bufor jest za mały na polecenie %s\n"
#define MSGTR_INPUT_INPUT_ErrWhyHere "Co my robimy??\n"
#define MSGTR_INPUT_INPUT_ErrCantInitJoystick "Nie mogę zainicjować dżojstika\n"
#define MSGTR_INPUT_INPUT_ErrCantStatFile "Nie moge podać statystyk %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantOpenFile "Nie moge otworzyć %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantInitAppleRemote "Nie mogę zainicjować Apple Remote.\n"

// ========================== LIBMPDEMUX ===================================

// url.c

#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "Łancuch wydaje się być już zakodowany w url_escape %c%c1%c2\n"

// ai_alsa1x.c

#define MSGTR_MPDEMUX_AIALSA1X_CannotSetSamplerate "Nie mogę ustawić częstotliwości próbkowania.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetBufferTime "Nie moge ustawić czasu bufora.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetPeriodTime "Nie mogę ustawić czasu okresu.\n"

// ai_alsa1x.c / ai_alsa.c

#define MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig "Błędna konfiguracja tego PCM: brak dostępnych konfiguracji.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableAccessType "Niedostępny tryb dostępu.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt "Niedostępny format próbki.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableChanCount "Niedostępa liczba kanałów - wracam do domyślnej: %d\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams "Nie mogę zainstalować parametrów sprzętu: %s"
#define MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize "Nie można używać okresu równego rozmiarowi bufora (%u == %lu)\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallSWParams "Nie mogę zainstalować parametrów oprogramowania:\n"
#define MSGTR_MPDEMUX_AIALSA_ErrorOpeningAudio "Błąd otwierania dźwięku: %s\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatusError "Błąd statusu ALSA : %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUN "ALSA xrun!!! (długości co najmniej %.3f ms)\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatus "Status ALSA :\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUNPrepareError "ALSA xrun: błąd przygotowywania: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaReadWriteError "ALSA błąd odczytu/zapisu"

// ai_oss.c

#define MSGTR_MPDEMUX_AIOSS_Unable2SetChanCount "Nie mogę ustawić liczby kanałów: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetStereo "Nie mogę ustawić stereo: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2Open "Nie mogę otworzyć '%s': %s\n"
#define MSGTR_MPDEMUX_AIOSS_UnsupportedFmt "nieobsługiwany format\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt "Nie moge ustawić formatu dźwięku."
#define MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate "Nie moge ustawić częstotliwości próbkowania: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger "Nie mogę ustawić zapadki: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize "Nie moge pobrać rozmiaru bloku!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero "Rozmiar bloku dźwiękowego wynosi zero, ustawiam na %d!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low "Za mały rozmiar bloku dźwiękowego, ustawiam na %d!\n"

// asfheader.c

#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB "BŁĄD KRYTYCZNY: rozmiar nagłowka jest większy niż 1 MB (%d)!\nProszę skontaktuj się z autorami MPlayera i wyślij ten plik.\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "Nie moge zaalokować %d bajtów na nagłówek.\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "Koniec pliku podczas odczytywania nagłówka ASF, zepsuty/niepełny plik?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "DVR prawdopodobnie zadziała tylko z libavformat, spróbuj -demuxer 35 jeśli napotykasz problemy\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "Brak danych po nagłówku!\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF: nie odnaleziono nagłówków audio/video - zepsuty plik?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "Nieprawidłowa długośc nagłówka ASF!\n"

// asf_mmst_streaming.c

#define MSGTR_MPDEMUX_MMST_WriteError "błąd zapisu\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\nAlarm! Koniec pliku(EOF)\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "błąd odczytu pre-nagłówka\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "Niewłaściwy rozmiar nagłowka, pomijam.\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "Błąd odczytu danych nagłówka.\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "błąd odczytu packet_len.\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "Nieprawidłowy rozmiar pakietu RTSP, pomijam.\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "Polecenie odczytu danych nie powiodło się.\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "obiekt nagłówka\n"
#define MSGTR_MPDEMUX_MMST_DataObject "obiekt danych\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "obiekt pliku, długość pakietu = %d (%d)\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "strumień, ID strumienia: %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "Za dużo ID, strumień pominięty."
#define MSGTR_MPDEMUX_MMST_UnknownObject "nieznany obiekt\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "Błąd odczytu danych z medium.\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "brak sygnatury\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "Wszystko gotowe. Dziękuje za pobranie pliku zawierającego zastrzeżoną i opatentowaną techonologię:).\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "nieznane polecenie %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "błąd get_media_packet : %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "Połączony\n"

// asf_streaming.c

#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "Aaaaa, rozmiar stream_chunk jest za mały: %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "niezgodność size_confirm!: %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "UWAGA: porzuć nagłówek ????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "Błąd podczas interpretowania nagłówka kawałka danych\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "Nie otrzymałem nagłówka jako pierwszego kawałka danych!!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "Błąd: Nie mogę zaalokowac %d bajtowego bufora.\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "Błąd podczas odczytu strumienia sieciowego.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "Błąd: Zbyt mały kawałek danych.\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "Błąd: nieprawidłowy numer podkawałka danych.\n"
//brzmi tragicznie:|
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "Za mała przepustowość, plik nie może zostać odtworzony!\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "Za mała przepustowość, wyłaczam strumień audio.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "Za mała przepustowość, wyłaczam strumień video.\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "Nieprawidłowa długość w nagłówku ASF!\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "Błąd odczytu kawałka nagłówka.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "Błąd: chunk_size > packet_size\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "Błąd odczytu kawałka danych.\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> Przekierowanie ASF\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "Nieprawidłowy URL serwera proxy\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "nieznany typ strumienia ASF\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "Błąd interpretacji odpowiedzi HTTP.\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "Serwer zwrócił %d:%s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "Ostrzeżenie intepretacji ASF HTTP : Pragma %s obcięte z %d bajtów do %d\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "błąd zapisu gniazda: %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "Błąd interpretacji nagłówka.\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "Nie odnaleziono strumienia.\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "nieznany typ strumieniowania ASF\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL: %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "Błąd, wychodzę.\n"

// audio_in.c

#define MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio "\nBłąd odczytu dźwięku: %s\n"
#define MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut "Odzyskano z przebiegu-krzyżowego, niektóre klatki mogły zostać utracone!\n"
#define MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover "Błąd krytyczny, nie mogę odzyskać!\n"
#define MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples "\nZa mało próbek dźwięku!\n"

// aviheader.c

#define MSGTR_MPDEMUX_AVIHDR_EmptyList "** pusta lista?!\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundMovieAt "Znalazłem film na pozycji 0x%X - 0x%X\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader "Znalazłem 'bih', %u bajtów z %d\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1 "Regeneruję tabelę klatek kluczowych dla M$ mpg4v1 video.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3 "Regeneruję tabelę klatek kluczowych dla DIVX3 video.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4 "Regeneruję tabelę klatek kluczowych dla MPEG-4 video.\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt "Znalazłem 'wf', %d bajtów z %d\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header "AVI: znalazłem dmlh (rozmiar=%d) (razem_klatek=%d)\n"
#define MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames "Czytam blok INDEX, %d kawałków na %d klatek (fpos=%"PRId64").\n"
#define MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr "Dodatkowy nagłówek RIFF ...\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** OSTRZEŻENIE: to nie jest rozszerzony nagłówek AVI..\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenChunk "Zepsuty kawałek danych? rozmiar=%d  (id=%.4s)\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI: ODML: Tworzę index ODML (%d kawałków superindex).\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI: ODML: Zepsuty (niepełny?) plik. Użyję tradycyjnego indeksu.\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "Nie mogę stworzyć pliku z indeksem %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "%s nie jest poprawnym plikiem indeksowym MPlayer.\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "Nie mogłem zaalokować pamięci na dane indeksowe z %s.\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "zbyt wczesny koniec pliku indeksowego %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "Wczytałem plik indeksowy: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "Tworzę index: %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI: Stworzony tablicę indeksową dla %d kawałków danych!\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "Nie mogłem zapisać pliku indeksu %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "Zapisałem plik indeksu: %s\n"

// cache2.c

#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\rTego strumienia nie da się zapisywać w pamięci podręcznej (cache).\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!! read_filepos różni się!!! Powiadom o tym błędzie...\n"

// cdda.c

#define MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice "Nie omgę otworzyć urządzenia CDDA.\n"
#define MSGTR_MPDEMUX_CDDA_CantOpenDisc "Nie mogę otworzyć płyty.\n"
#define MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks "Znalazłem płytę audio z %ld ścieżkami.\n"

// cddb.c

#define MSGTR_MPDEMUX_CDDB_FailedToReadTOC "Błąd odczytu TOC.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToOpenDevice "Błąd otwierania urządzenia %s.\n"
#define MSGTR_MPDEMUX_CDDB_NotAValidURL "niepoprawny URL\n"
#define MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest "Błąd wysyłania żądania HTTP.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse "Błąd odczytu odpowiedzi HTTP.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND "Nie znaleziono.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown "nieznany kod błędu\n"
#define MSGTR_MPDEMUX_CDDB_NoCacheFound "Nie znalazłem pamięci podręcznej (cache).\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead "Nie cały plik xmcd został odczytany.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory "Tworzenie katalogu nie powiodło się %s.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten "Nie cały plik xmcd został zapisany.\n"
#define MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned "Nieprawidłowa baza danych pliku xmcd została zwrócona.\n"
#define MSGTR_MPDEMUX_CDDB_UnexpectedFIXME "nieoczekiwane FIXME\n"
#define MSGTR_MPDEMUX_CDDB_UnhandledCode "nieoczekiwany kod\n"
#define MSGTR_MPDEMUX_CDDB_UnableToFindEOL "Nie mogę odnaleźć końca wiersza.\n"
#define MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle "Interpretowanie OK, znalazłem: %s\n"
#define MSGTR_MPDEMUX_CDDB_AlbumNotFound "Album nie odnaleziony.\n"
#define MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr "Serwer zwrócił: Błąd składni polecenia\n"
#define MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable "Brak dostępnych informacji o stronach.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel "Błąd pobierania poziomu protokołu.\n"
#define MSGTR_MPDEMUX_CDDB_NoCDInDrive "Brak płyty CD w napędzie.\n"

// cue_read.c

#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] nieoczekiwany wiersz cuefile: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] przetestowałem nazwę pliku bin: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] Nie mogłem odnaleźć pliku bin - pomijam.\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] Używam pliku bin %s.\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] nieznany tryb dla pliku bin. Nie powinno się zdarzyć. Anuluję.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] Nie mogę otworzyć %s.\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] Błąd odczytu z %s\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] Błąd pobierania rozmiaru pliku bin.\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "ścieżka %02d:  format=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] nieoczekiwany koniec pliku bin\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] Nie mogłem odczytać %d bajtów z ładunku.\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "CUE stream_open, nazwa_pliku=%s, ścieżka=%d, dostępne ścieżki: %d -> %d\n"

// network.c

#define MSGTR_MPDEMUX_NW_UnknownAF "Nieznana rodzina adresów %d\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "Zamieniam %s na %s...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "Nie mogłem zamienić nazwy dla  %s: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "Łączę z serwerem %s[%s]: %d...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "Błąd połączenia do serwera z %s\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "Wybieranie nie powiodło się.\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "połączenie zerwane\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "getsockopt nie powiodło się: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "błąd połączenia: %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "Nieprawidłowe ustawienia serwera proxy... Próbuję bez.\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "Nie mogłem zamienić nazwy hosta dla AF_INET. Próbuje bez serwera proxy.\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "Błąd podczas wysyłania żądania HTTP: Nie wysłano całego żadania.\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "Odczyt nie powiódł się.\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "odczyt http_read_response dał 0 (np. koniec pliku).\n"
#define MSGTR_MPDEMUX_NW_AuthFailed "Błąd logowania. Proszę ustaw opcje -user i -passwd by podać swoją \n"\
"nazwę_użytkowinka/hasło dla listy URLi, lub stworzyć URL typu:\n"\
"http://nazwa_użytkownika:hasło@nazwa_serwera/plik\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "Serwer %s wymaga logowania\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "Logowanie wymagane.\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "Nie podano hasła, próbuje pustego.\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "Serwer zwraca %d: %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "Rozmiar pamięci podręcznej (cache) ustawiono na %d Kbajtów\n"

// demux_audio.c

#define MSGTR_MPDEMUX_AUDIO_UnknownFormat "demuxer dźwięku: nieznany format %d.\n"

// demux_demuxers.c

#define MSGTR_MPDEMUX_DEMUXERS_FillBufferError "błąd fill_buffer: zły demuxer: nie vd, ad lub sd.\n"

// demux_nuv.c

#define MSGTR_MPDEMUX_NUV_NoVideoBlocksInFile "Brak danych video w pliku.\n"

// demux_xmms.c

#define MSGTR_MPDEMUX_XMMS_FoundPlugin "Znalazłem wtyczkę: %s (%s).\n"
#define MSGTR_MPDEMUX_XMMS_ClosingPlugin "Zamykam wtyczkę: %s.\n"

// ========================== LIBMPMENU ===================================

// common

#define MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition "[MENU] Brak wpisów w definicji menu.\n"

// libmenu/menu.c
#define MSGTR_LIBMENU_SyntaxErrorAtLine "[MENU] błąd składni w wierszu: %d\n"
#define MSGTR_LIBMENU_MenuDefinitionsNeedANameAttrib "[MENU] Definicje menu potrzebują atrybutu name (wiersz %d).\n"
#define MSGTR_LIBMENU_BadAttrib "[MENU] zły atrybut %s=%s w menu '%s' w wierszu %d\n"
#define MSGTR_LIBMENU_UnknownMenuType "[MENU] nieznany typ menu '%s' w wierszu %d\n"
// Też podejrzewam że to tekstowe -> wiersz. A jeśli nie to linii
#define MSGTR_LIBMENU_CantOpenConfigFile "[MENU] Nie mogę otworzyć pliku konfiguracyjnego menu: %s\n"
#define MSGTR_LIBMENU_ConfigFileIsTooBig "[MENU] Plik konfiguracyjny jest za duży (> %d KB)\n"
#define MSGTR_LIBMENU_ConfigFileIsEmpty "[MENU] Plik konfiguracyjny jest pusty.\n"
#define MSGTR_LIBMENU_MenuNotFound "[MENU] Menu %s nie zostało odnalezione.\n"
#define MSGTR_LIBMENU_MenuInitFailed "[MENU] Menu '%s': Inicjalizacja nie powiodła się.\n"
#define MSGTR_LIBMENU_UnsupportedOutformat "[MENU] Nieobsługiwany format wyjściowy!!!!\n"

// libmenu/menu_cmdlist.c
#define MSGTR_LIBMENU_ListMenuEntryDefinitionsNeedAName "[MENU] Wpisy w menu 'lista' potrzebują nazwy (wiersz %d).\n"
#define MSGTR_LIBMENU_ListMenuNeedsAnArgument "[MENU] Menu 'lista' wymaga argumentu.\n"

// libmenu/menu_console.c
#define MSGTR_LIBMENU_WaitPidError "[MENU] błąd waitpid: %s.\n"
#define MSGTR_LIBMENU_SelectError "[MENU] Błąd wyboru.\n"
#define MSGTR_LIBMENU_ReadErrorOnChildFD "[MENU] Błąd odczytu deskryptora pliku potomnego: %s.\n"
#define MSGTR_LIBMENU_ConsoleRun "[MENU] Uruchom: %s ...\n"
#define MSGTR_LIBMENU_AChildIsAlreadyRunning "[MENU] Proces potomny już działa.\n"
#define MSGTR_LIBMENU_ForkFailed "[MENU] Fork nie powiodło się !!!\n"
#define MSGTR_LIBMENU_WriteError "[MENU] błąd zapisu\n"

// libmenu/menu_filesel.c
#define MSGTR_LIBMENU_OpendirError "[MENU] błąd opendir: %s\n"
#define MSGTR_LIBMENU_ReallocError "[MENU] błąd realloc: %s\n"
#define MSGTR_LIBMENU_MallocError "[MENU] błąd alokacji pamięci: %s\n"
#define MSGTR_LIBMENU_ReaddirError "[MENU] błąd readdir: %s\n"
#define MSGTR_LIBMENU_CantOpenDirectory "[MENU] Nie mogę otworzyć katalogu %s.\n"

// libmenu/menu_param.c
#define MSGTR_LIBMENU_SubmenuDefinitionNeedAMenuAttribut "[MENU] Definicje podmenu potrzebują atrybutu 'menu'.\n"
#define MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed "[MENU] Definicje menu pref potrzebują poprawnego atrybutu 'property' (wiersz %d).\n"
#define MSGTR_LIBMENU_PrefMenuNeedsAnArgument "[MENU] Menu pref potrzebuje argumentu.\n"

// libmenu/menu_pt.c
#define MSGTR_LIBMENU_CantfindTheTargetItem "[MENU] Nie mogę znaleźć przedmiotu docelowego ????\n"
#define MSGTR_LIBMENU_FailedToBuildCommand "[MENU] Nie udało się stworzyć polecenia: %s.\n"

// libmenu/menu_txt.c
#define MSGTR_LIBMENU_MenuTxtNeedATxtFileName "[MENU] Menu tekstowe wymaga nazwy pliku tekstowego (parametr file).\n"
#define MSGTR_LIBMENU_MenuTxtCantOpen "[MENU] Nie mogę otworzyć %s.\n"
#define MSGTR_LIBMENU_WarningTooLongLineSplitting "[MENU] Uwaga, wiersz za długi. Dzielę go.\n"
#define MSGTR_LIBMENU_ParsedLines "[MENU] Zinterpretowano %d wierszy.\n"


// libmenu/vf_menu.c
#define MSGTR_LIBMENU_UnknownMenuCommand "[MENU] Nieznane polecenie: '%s'.\n"
#define MSGTR_LIBMENU_FailedToOpenMenu "[MENU] Nie udało się otworzyć menu: '%s'.\n"

// ========================== LIBMPCODECS ===================================

// libmpcodecs/ad_libdv.c
#define MSGTR_MPCODECS_AudioFramesizeDiffers "[AD_LIBDV] Uwaga! Rozmiar ramek dźwieku różni się! wczytano=%d  hdr=%d.\n"

// libmpcodecs/vd_dmo.c vd_dshow.c vd_vfw.c
#define MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec "[VD_DMO] Nie mogłem zaalokować obrazu dla kodeka cinepak.\n"

// libmpcodecs/vd_ffmpeg.c
#define MSGTR_MPCODECS_XVMCAcceleratedCodec "[VD_FFMPEG] Kodek przyspieszony przez XVMC.\n"
#define MSGTR_MPCODECS_ArithmeticMeanOfQP "[VD_FFMPEG] Średnia arytmetyczna QP: %2.4f, Średnia harmoniczna QP: %2.4f\n"
#define MSGTR_MPCODECS_DRIFailure "[VD_FFMPEG] błąd DRI.\n"
#define MSGTR_MPCODECS_CouldntAllocateImageForCodec "[VD_FFMPEG] Nie mogłem zaalokować obrazu dla kodeka.\n"
#define MSGTR_MPCODECS_XVMCAcceleratedMPEG2 "[VD_FFMPEG] MPEG-2 przyspieszony przez XVMC.\n"
#define MSGTR_MPCODECS_TryingPixfmt "[VD_FFMPEG] Próbuję pixfmt=%d.\n"
#define MSGTR_MPCODECS_McGetBufferShouldWorkOnlyWithXVMC "[VD_FFMPEG] mc_get_buffer powinien działać tylko z przyspieszeniem XVMC!!"
#define MSGTR_MPCODECS_UnexpectedInitVoError "[VD_FFMPEG] Nieoczekiwany błąd init_vo.\n"
#define MSGTR_MPCODECS_UnrecoverableErrorRenderBuffersNotTaken "[VD_FFMPEG] Nieodzyskiwalny błąd, bufory renderowania nie użyte.\n"
#define MSGTR_MPCODECS_OnlyBuffersAllocatedByVoXvmcAllowed "[VD_FFMPEG] Dozwolone tylko bufory zaalokowane przez vo_xvmc.\n"

// libmpcodecs/ve_lavc.c
#define MSGTR_MPCODECS_HighQualityEncodingSelected "[VE_LAVC] Wybrano kodowanie wysokiej jakości (nie w czasie rzeczywistym)!\n"
#define MSGTR_MPCODECS_UsingConstantQscale "[VE_LAVC] Używam stałego qscale = %f (VBR).\n"

// libmpcodecs/ve_raw.c
#define MSGTR_MPCODECS_OutputWithFourccNotSupported "[VE_RAW] Surowe wyjście przez FourCC [%x] jest nieobsługiwane!\n"
#define MSGTR_MPCODECS_NoVfwCodecSpecified "[VE_RAW] Nie podano wymaganego kodeka VfW!!\n"

// libmpcodecs/vf_crop.c
#define MSGTR_MPCODECS_CropBadPositionWidthHeight "[CROP] Zła pozycja/szerokość/wysokość - wycinany obszar poza oryginałem!\n"

// libmpcodecs/vf_cropdetect.c
#define MSGTR_MPCODECS_CropArea "[CROP] Wycinany obszar: X: %d..%d  Y: %d..%d  (-vf crop=%d:%d:%d:%d).\n"

// libmpcodecs/vf_format.c, vf_palette.c, vf_noformat.c
#define MSGTR_MPCODECS_UnknownFormatName "[VF_FORMAT] Nieznana nazwa formatu: '%s'.\n"

// libmpcodecs/vf_framestep.c vf_noformat.c vf_palette.c vf_tile.c
#define MSGTR_MPCODECS_ErrorParsingArgument "[VF_FRAMESTEP] Błąd interpretacji argumentu.\n"

// libmpcodecs/ve_vfw.c
#define MSGTR_MPCODECS_CompressorType "Typ kompresora: %.4lx\n"
#define MSGTR_MPCODECS_CompressorSubtype "Podtyp kompresora: %.4lx\n"
#define MSGTR_MPCODECS_CompressorFlags "Flagi kompresora: %lu, wersja %lu, wersja ICM: %lu\n"
#define MSGTR_MPCODECS_Flags "Flagi:"
#define MSGTR_MPCODECS_Quality " jakość"

// libmpcodecs/vf_expand.c
#define MSGTR_MPCODECS_FullDRNotPossible "Pełny DR niemożliwy, próbuję SLICES(części) zamiast!\n"
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupportSlices  "UWAGA! Następny filtr nie obsługuje SLICES, przygotuj się na sig11...\n"
#define MSGTR_MPCODECS_FunWhydowegetNULL "Dlaczego dostaliśmy NULL??\n"

// libmpcodecs/vf_test.c, vf_yuy2.c, vf_yvu9.c
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupport "%s nie jest obsługiwane przez następny filtr/vo:(\n"

// ================================== LIBMPVO ====================================

// mga_common.c

#define MSGTR_LIBVO_MGA_ErrorInConfigIoctl "[MGA] błąd w mga_vid_config ioctl (zła wersja mga_vid.o ?)"
#define MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule "[MGA] Nie mogłem pobrać wartości luma z modułu jądra!\n"
#define MSGTR_LIBVO_MGA_CouldNotSetLumaValuesFromTheKernelModule "[MGA] Nie mogłem ustawić wartości luma z modułu jądra!\n"
#define MSGTR_LIBVO_MGA_ScreenWidthHeightUnknown "[MGA] Nieznana szerokość/wysokośc ekranu!\n"
#define MSGTR_LIBVO_MGA_InvalidOutputFormat "[MGA] niepoprawny format wyjściowy %0X\n"
#define MSGTR_LIBVO_MGA_IncompatibleDriverVersion "[MGA] Wersja Twojego sterownika mga_vid jest niekompatybilna z tą wersją MPlayera!\n"
#define MSGTR_LIBVO_MGA_CouldntOpen "[MGA] Nie mogłem otworzyć: %s\n"
#define MSGTR_LIBVO_MGA_ResolutionTooHigh "[MGA] Żródłowa rozdzielczość jest co najmniej o wymiar większa niż 1023x1023. Proszę przeskaluj programowo lub użyj -lavdopts lowres=1\n"

// libvo/vesa_lvo.c

#define MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported "[VESA_LVO] Ta gałąź nie jest już kontynuowana.\n[VESA_LVO] Proszę użyj -vo vesa:vidix.\n"
#define MSGTR_LIBVO_VESA_CouldntOpen "[VESA_LVO] Nie mogłem otworzyć: '%s'\n"
#define MSGTR_LIBVO_VESA_InvalidOutputFormat "[VESA_LVI] niepoprawny format wyjściowy: %s(%0X)\n"
#define MSGTR_LIBVO_VESA_IncompatibleDriverVersion "[VESA_LVO] Wersja Twojego sterownika fb_vid jest niekompatybilna z tą wersją MPlayera!\n"

// libvo/vo_3dfx.c

#define MSGTR_LIBVO_3DFX_Only16BppSupported "[VO_3DFX] Obsługiwane tylko 16bpp!"
#define MSGTR_LIBVO_3DFX_VisualIdIs "[VO_3DFX] Visual ID to  %lx.\n"
#define MSGTR_LIBVO_3DFX_UnableToOpenDevice "[VO_3DFX] Nie mogę otworzyć /dev/3dfx.\n"
#define MSGTR_LIBVO_3DFX_Error "[VO_3DFX] Błąd: %d.\n"
#define MSGTR_LIBVO_3DFX_CouldntMapMemoryArea "[VO_3DFX] Nie mogłem zmapować obszarów pamięci 3dfx: %p,%p,%d.\n"
#define MSGTR_LIBVO_3DFX_DisplayInitialized "[VO_3DFX] Zainicjalizowane: %p.\n"
#define MSGTR_LIBVO_3DFX_UnknownSubdevice "[VO_3DFX] Nieznane podurządzenie: %s.\n"

// libvo/vo_dxr3.c

#define MSGTR_LIBVO_DXR3_UnableToLoadNewSPUPalette "[VO_DXR3] Nie mogę wczytac nowej palety SPU!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetPlaymode "[VO_DXR3] Nie mogę ustawić trybu odtwarzania!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetSubpictureMode "[VO_DXR3] Nie moge ustawić trybu podobrazu!\n"
#define MSGTR_LIBVO_DXR3_UnableToGetTVNorm "[VO_DXR3] Nie mogę pobrać trybu TV!\n"
#define MSGTR_LIBVO_DXR3_AutoSelectedTVNormByFrameRate "[VO_DXR3] Auto-wybrany tryb TV dzięki częstotliwości odświeżania: "
#define MSGTR_LIBVO_DXR3_UnableToSetTVNorm "[VO_DXR3] Nie mogę ustawić trybu TV!\n"
#define MSGTR_LIBVO_DXR3_SettingUpForNTSC "[VO_DXR3] Ustawiam NTSC.\n"
#define MSGTR_LIBVO_DXR3_SettingUpForPALSECAM "[VO_DXR3] Ustawiam PAL/SECAM.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo43 "[VO_DXR3] Usawiam format filmu na 4:3.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo169 "[VO_DXR3] Usawiam format filmu na 16:9.\n"
#define MSGTR_LIBVO_DXR3_OutOfMemory "[VO_DXR3] brak pamięci\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateKeycolor "[VO_DXR3] Nie mogę zaalokować koloru kluczowego!\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateExactKeycolor "[VO_DXR3] Nie mogę zaalokować dokładnego koloru, używam najlepiej dopasowanego (0x%lx).\n"
#define MSGTR_LIBVO_DXR3_Uninitializing "[VO_DXR3] Deinicjalizuję.\n"
#define MSGTR_LIBVO_DXR3_FailedRestoringTVNorm "[VO_DXR3] Błąd przywracania trybu TV!\n"
#define MSGTR_LIBVO_DXR3_EnablingPrebuffering "[VO_DXR3] Włączam prebuforowanie.\n"
#define MSGTR_LIBVO_DXR3_UsingNewSyncEngine "[VO_DXR3] Używam nowego silnika synchronizującego.\n"
#define MSGTR_LIBVO_DXR3_UsingOverlay "[VO_DXR3] używam nakładki.\n"
#define MSGTR_LIBVO_DXR3_ErrorYouNeedToCompileMplayerWithX11 "[VO_DXR3] Błąd: Nakładka wymaga skompilowania z zainstalowanymi bibliotekami/nagłówkami X11.\n"
#define MSGTR_LIBVO_DXR3_WillSetTVNormTo "[VO_DXR3] ustawię tryb TV na: "
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALPAL60 "auto-dostosowanie do częstotliwości filmu (PAL/PAL-60)"
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALNTSC "auto-dostosowanie do częstotliwości filmu (PAL/NTSC)"
#define MSGTR_LIBVO_DXR3_UseCurrentNorm "Użyję obecnego trybu."
#define MSGTR_LIBVO_DXR3_UseUnknownNormSuppliedCurrentNorm "Podano nieznany tryb. Używam obecnego trybu."
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTrying "[VO_DXR3] Błąd otwierania %s do zapisu, próbuje /dev/em8300.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingMV "[VO_DXR3] Błąd otwierania %s do zapisu, próbuje /dev/em8300_mv.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWell "[VO_DXR3] Również błąd przy otwieraniu /dev/em8300 do zapisu!\nOpuszczam.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellMV "[VO_DXR3] Również błąd przy otwieraniu /dev/em8300_mv do zapisu!\nOpuszczam.\n"
#define MSGTR_LIBVO_DXR3_Opened "[VO_DXR3] Otworzyłem: %s.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingSP "[VO_DXR3] Błąd otwierania %s do zapisu, próbuję /dev/em8300_sp.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellSP "[VO_DXR3] Również błąd przy otwieraniu /dev/em8300_sp do zapisu!\nOpuszczam.\n"
#define MSGTR_LIBVO_DXR3_UnableToOpenDisplayDuringHackSetup "[VO_DXR3] Nie mogę otworzyć ekranu podczas konfiguracji hakowanej nakładki!\n"
#define MSGTR_LIBVO_DXR3_UnableToInitX11 "[VO_DXR3] Nie moge zainicjować X11!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayAttribute "[VO_DXR3] Ustawianie atrybutu nakładki nie powiodło się.\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayScreen "[VO_DXR3] Błąd ustawiania nakładki ekranu!\nWychodzę.\n"
#define MSGTR_LIBVO_DXR3_FailedEnablingOverlay "[VO_DXR3] Błąd uruchamiania nakładki!\nWychodzę.\n"
#define MSGTR_LIBVO_DXR3_FailedResizingOverlayWindow "[VO_DXR3] Błąd zmiany rozmiaru okna nakładki!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayBcs "[VO_DXR3] Błąd ustawiania bcs nakładki!\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayYOffsetValues "[VO_DXR3] Błąd pobierania wartości Y-offset nakładki!\nWychodzę.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXOffsetValues "[VO_DXR3] Błąd pobierania wartości X-offset nakładki!\nWychodzę.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXScaleCorrection "[VO_DXR3] Błąd pobierania korekcji X scale!\nWychodzę.\n"
#define MSGTR_LIBVO_DXR3_YOffset "[VO_DXR3] Yoffset: %d.\n"
#define MSGTR_LIBVO_DXR3_XOffset "[VO_DXR3] Xoffset: %d.\n"
#define MSGTR_LIBVO_DXR3_XCorrection "[VO_DXR3] Xcorrection: %d.\n"
#define MSGTR_LIBVO_DXR3_FailedSetSignalMix "[VO_DXR3] Ustawianie sygnału mix nie powiodło się!\n"

// libvo/vo_mga.c

#define MSGTR_LIBVO_MGA_AspectResized "[VO_MGA] aspect(): zmieniono rozmiar do %dx%d.\n"
#define MSGTR_LIBVO_MGA_Uninit "[VO] deinicjalizacja!\n"

// libvo/vo_null.c

#define MSGTR_LIBVO_NULL_UnknownSubdevice "[VO_NULL] Nieznane podurządzenie: %s.\n"

// libvo/vo_png.c

#define MSGTR_LIBVO_PNG_Warning1 "[VO_PNG] Uwaga: poziom kompresji ustawiony na 0, kompresja wyłączona!\n"
#define MSGTR_LIBVO_PNG_Warning2 "[VO_PNG] Info: użyj -vo png:z=<n> by ustawić poziom kompresji od 0 do 9.\n"
#define MSGTR_LIBVO_PNG_Warning3 "[VO_PNG] Info: (0 = brak kompresji, 1 = najszybsza, najniższa - 9 najlepsza, najwolniejsza kompresja)\n"
#define MSGTR_LIBVO_PNG_ErrorOpeningForWriting "\n[VO_PNG] Błąd otwierania '%s' do zapisu!\n"
#define MSGTR_LIBVO_PNG_ErrorInCreatePng "[VO_PNG] Błąd w create_png.\n"

// libvo/vo_sdl.c

#define MSGTR_LIBVO_SDL_CouldntGetAnyAcceptableSDLModeForOutput "[VO_SDL] Nie mogłem pobrać żadnego akceptowalnego trybu SDL dla wyjścia.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailed "[VO_SDL] set_video_mode: SDL_SetVideoMode nie powiodło się: %s.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailedFull "[VO_SDL] Set_fullmode: SDL_SetVideoMode nie powiodło się: %s.\n"
#define MSGTR_LIBVO_SDL_MappingI420ToIYUV "[VO_SDL] Mapuję I420 do IYUV.\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormat "[VO_SDL] Nieobsługiwany format obrazka (0x%X).\n"
#define MSGTR_LIBVO_SDL_InfoPleaseUseVmOrZoom "[VO_SDL] Info - prosze użyj -vm lub -zoom by przełączyć na najlepszą rozdzielczość.\n"
#define MSGTR_LIBVO_SDL_FailedToSetVideoMode "[VO_SDL] Nie udało się ustawić trybu video: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateAYUVOverlay "[VO_SDL] Nie mogłem ustawić nakładki YUV: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateARGBSurface "[VO_SDL] Nie mogłem stworzyć powierzchni RGB: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDepthColorspaceConversion "[VO_SDL] Używam konwersji głębokości/przestrzeni kolorów, to trochę wszystko spowolni (%ibpp -> %ibpp).\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormatInDrawslice "[VO_SDL] Nieobsługiwany format obrazka w draw_slice, skontaktuj się z twórcami MPlayera!\n"
#define MSGTR_LIBVO_SDL_BlitFailed "[VO_SDL] Blit nie powiodło się: %s.\n"
//dunno what is blit?
#define MSGTR_LIBVO_SDL_InitializationFailed "[VO_SDL] inicjalizacja SDL nie powiodła się: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDriver "[VO_SDL] Używam sterownika: %s.\n"

// libvo/vobsub_vidix.c

#define MSGTR_LIBVO_SUB_VIDIX_CantStartPlayback "[VO_SUB_VIDIX] Nie mogę rozpocząć odtwarzania: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantStopPlayback "[VO_SUB_VIDIX] Nie mogę zatrzymać odtwarzania: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_InterleavedUvForYuv410pNotSupported "[VO_SUB_VIDIX] UV z przeplotem dla YUV410P jest nieobsługiwany.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawsliceWasCalled "[VO_SUB_VIDIX] Odwołanie do sztucznego vidix_draw_slice().\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawframeWasCalled "[VO_SUB_VIDIX] Odwołanie do sztucznego vidix_draw_frame().\n"
#define MSGTR_LIBVO_SUB_VIDIX_UnsupportedFourccForThisVidixDriver "[VO_SUB_VIDIX] Nieobsługiwany FourCC dla tego sterownika VIDIX: %x (%s).\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedResolution "[VO_SUB_VIDIX] Serwer video używa nieobsługiwanej rozdzielczości (%dx%d), obsługiwana: %dx%d-%dx%d.\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedColorDepth "[VO_SUB_VIDIX] Serwer video używa nieobsługiwanej głębi kolorów w vidix (%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantUpscaleImage "[VO_SUB_VIDIX] Sterownik VIDIX nie może powiększyć obrazu (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantDownscaleImage "[VO_SUB_VIDIX] Sterownik VIDIX nie może pomniejszyć obrazu (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantConfigurePlayback "[VO_SUB_VIDIX] Nie mogę skonfigurować odtwarzania: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_YouHaveWrongVersionOfVidixLibrary "[VO_SUB_VIDIX] Masz nieprawidłową wersję biblioteki VIDIX.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntFindWorkingVidixDriver "[VO_SUB_VIDIX] Nie odnalazłem działającego sterownika VIDIX.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntGetCapability "[VO_SUB_VIDIX] Nie mogłem pobrać możliwości: %s.\n"

// libvo/vo_svga.c

#define MSGTR_LIBVO_SVGA_ForcedVidmodeNotAvailable "[VO_SVGA] Wymuszony vid_mode %d (%s) nie jest obsługiwany.\n"
#define MSGTR_LIBVO_SVGA_ForcedVidmodeTooSmall "[VO_SVGA] Wymuszony vid_mode %d (%s) za mały.\n"
#define MSGTR_LIBVO_SVGA_Vidmode "[VO_SVGA] Vid_mode: %d, %dx%d %dbpp.\n"
#define MSGTR_LIBVO_SVGA_VgasetmodeFailed "[VO_SVGA] Vga_setmode(%d) nie powiodło się.\n"
#define MSGTR_LIBVO_SVGA_VideoModeIsLinearAndMemcpyCouldBeUsed "[VO_SVGA] Liniowy tryb video i memcpy może zostać użyte do transferu obrazu.\n"
#define MSGTR_LIBVO_SVGA_VideoModeHasHardwareAcceleration "[VO_SVGA] Tryb video posiada przyspieszenie sprzętowe i put_image może zostać użyte.\n"
#define MSGTR_LIBVO_SVGA_IfItWorksForYouIWouldLikeToKnow "[VO_SVGA] Jeśli to zadziała, prosze daj mi znać.\n[VO_SVGA] (wyślij dziennik z `mplayer test.avi -v -v -v -v &> svga.log`). DZIEKI!\n"
#define MSGTR_LIBVO_SVGA_VideoModeHas "[VO_SVGA] Tryb video ma %d stron.\n"
#define MSGTR_LIBVO_SVGA_CenteringImageStartAt "[VO_SVGA] Wyśrodkowuję obraz. Zaczynam w (%d,%d)\n"
#define MSGTR_LIBVO_SVGA_UsingVidix "[VO_SVGA] Używam VIDIX. w=%i h=%i  mw=%i mh=%i\n"

// libvo/vo_tdfxfb.c

#define MSGTR_LIBVO_TDFXFB_CantOpen "[VO_TDFXFB] Nie mogę otworzyć %s: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetFscreenInfo "[VO_TDFXFB] Problem z FBITGET_FSCREENINFO ioctl: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetVscreenInfo "[VO_TDFXFB] Problem z FBITGET_VSCREENINFO ioctl: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ThisDriverOnlySupports "[VO_TDFXFB] Ten sterownik obsługuje tylko 3Dfx Banshee, Voodoo3 i Voodoo 5.\n"
#define MSGTR_LIBVO_TDFXFB_OutputIsNotSupported "[VO_TDFXFB] wyjście %d bpp jest nieobsługiwane.\n"
#define MSGTR_LIBVO_TDFXFB_CouldntMapMemoryAreas "[VO_TDFXFB] Nie mogłem zmapować obszarów pamięci: %s.\n"
#define MSGTR_LIBVO_TDFXFB_BppOutputIsNotSupported "[VO_TDFXFB] wyjście %d bpp jest nieobsługiwane (To nie powinno się nigdy zdarzyć).\n"
#define MSGTR_LIBVO_TDFXFB_SomethingIsWrongWithControl "[VO_TDFXFB] Eik! Coś jest nie tak z control().\n"
#define MSGTR_LIBVO_TDFXFB_NotEnoughVideoMemoryToPlay "[VO_TDFXFB] Za mało pamięci video by odtworzyć ten film. Spróbuj w niższej rozdzielczości.\n"
#define MSGTR_LIBVO_TDFXFB_ScreenIs "[VO_TDFXFB] Ekran ma %dx%d w %d bpp, in ma %dx%d w %d bpp, norm ma %dx%d.\n"

// libvo/vo_tdfx_vid.c

#define MSGTR_LIBVO_TDFXVID_Move "[VO_TDXVID] Przesuwam %d(%d) x %d => %d.\n"
#define MSGTR_LIBVO_TDFXVID_AGPMoveFailedToClearTheScreen "[VO_TDFXVID] AGP move by wyczyścić ekran nie powiodło się.\n"
#define MSGTR_LIBVO_TDFXVID_BlitFailed "[VO_TDFXVID] Blit nie powiodło się.\n"
#define MSGTR_LIBVO_TDFXVID_NonNativeOverlayFormatNeedConversion "[VO_TDFXVID] Nie-rdzenny format nakładki, wymaga konwersji.\n"
#define MSGTR_LIBVO_TDFXVID_UnsupportedInputFormat "[VO_TDFXVID] Nieobsługiwany format wejściowy 0x%x.\n"
#define MSGTR_LIBVO_TDFXVID_OverlaySetupFailed "[VO_TDFXVID] Ustawienie nakładki nie powiodło się.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOnFailed "[VO_TDFXVID] Włączanie nakładki nie powiodło się.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayReady "[VO_TDFXVID] Nakładka gotowa: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_TextureBlitReady "[VO_TDFXVID] blit dla tekstur gotwy: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOffFailed "[VO_TDFXVID] Wyłączanie nakładki nie powiodło się\n"
#define MSGTR_LIBVO_TDFXVID_CantOpen "[VO_TDFXVID] Nie mogę otworzyć %s: %s.\n"
#define MSGTR_LIBVO_TDFXVID_CantGetCurrentCfg "[VO_TDFXVID] Nie mogę pobrać obecnej konfiguracji: %s.\n"
#define MSGTR_LIBVO_TDFXVID_MemmapFailed "[VO_TDFXVID] Memmap nie powiodło się!!!!!\n"
#define MSGTR_LIBVO_TDFXVID_GetImageTodo "Sprawdź listę do-zrobienia obrazu.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailed "[VO_TDFXVID] AGP move nie powiodło się.\n"
#define MSGTR_LIBVO_TDFXVID_SetYuvFailed "[VO_TDFXVID] Ustawianie YUV nie powiodło się.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnYPlane "[VO_TDFXVID] AGP move nie powiodło się na płaszczyźnie Y.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnUPlane "[VO_TDFXVID] AGP move nie powiodło się na płaszczyźnie U.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnVPlane "[VO_TDFXVID] AGP move nie powiodło się na płaszczyźnie V.\n"
#define MSGTR_LIBVO_TDFXVID_UnknownFormat "[VO_TDFXVID] nieznany format: 0x%x.\n"

// libvo/vo_tga.c

#define MSGTR_LIBVO_TGA_UnknownSubdevice "[VO_TGA] Nieznane podurządzenie: %s.\n"

// libvo/vo_vesa.c

#define MSGTR_LIBVO_VESA_FatalErrorOccurred "[VO_VESA] Wystąpił błąd krytyczny! Nie moge kontynuuować.\n"
#define MSGTR_LIBVO_VESA_UnknownSubdevice "[VO_VESA] nieznane podurządzenie: '%s'.\n"
#define MSGTR_LIBVO_VESA_YouHaveTooLittleVideoMemory "[VO_VESA] Masz za mało pamięci video dla tego trybu:\n[VO_VESA] Wymagane: %08lX obecne: %08lX.\n"
#define MSGTR_LIBVO_VESA_YouHaveToSpecifyTheCapabilitiesOfTheMonitor "[VO_VESA] Musisz podać możliwości monitora. Nie zmieniam częstotliwości odświeżania.\n"
#define MSGTR_LIBVO_VESA_UnableToFitTheMode "[VO_VESA] Tryb nie jest zgodny z limitami monitora. Nie zmieniam częstotliwości odświeżania.\n"
#define MSGTR_LIBVO_VESA_DetectedInternalFatalError "[VO_VESA] Wykryto krytyczny błąd wewnętrzny: init zostało wywołane przed preinit.\n"
#define MSGTR_LIBVO_VESA_SwitchFlipIsNotSupported "[VO_VESA] Opcja -flip nie jest obsługiwana.\n"
#define MSGTR_LIBVO_VESA_PossibleReasonNoVbe2BiosFound "[VO_VESA] Prawdopodobny powód: Nie znaleziono BIOSu VBE2.\n"
#define MSGTR_LIBVO_VESA_FoundVesaVbeBiosVersion "[VO_VESA] Znalazłem BIOS VESA VBE Wersja %x.%x Rewizja: %x.\n"
#define MSGTR_LIBVO_VESA_VideoMemory "[VO_VESA] Pamięć video: %u Kb.\n"
#define MSGTR_LIBVO_VESA_Capabilites "[VO_VESA] Możliwości VESA: %s %s %s %s %s.\n"
#define MSGTR_LIBVO_VESA_BelowWillBePrintedOemInfo "[VO_VESA] !!! Wiadomość OEM zostanie wydrukowana poniżej!!!\n"
#define MSGTR_LIBVO_VESA_YouShouldSee5OemRelatedLines "[VO_VESA] Powinieneś widzieć 5 wierszy związanych z OEM; Jeśli nie, masz zepsute vm86.\n"
#define MSGTR_LIBVO_VESA_OemInfo "[VO_VESA] OEM info: %s.\n"
#define MSGTR_LIBVO_VESA_OemRevision "[VO_VESA] OEM Rewizja: %x.\n"
#define MSGTR_LIBVO_VESA_OemVendor "[VO_VESA] OEM producent: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductName "[VO_VESA] OEM nazwa produktu: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductRev "[VO_VESA] OEM rewizja produktu: %s.\n"
#define MSGTR_LIBVO_VESA_Hint "[VO_VESA] Wskazówka: By TV-Out działało powinieneś podpiąć kabel TV\n"\
"[VO_VESA] przed bootowaniem, ponieważ VESA BIOS inicjalizuje się tylko podczas POST.\n"
#define MSGTR_LIBVO_VESA_UsingVesaMode "[VO_VESA] Używam trybu VESA (%u) = %x [%ux%u@%u]\n"
#define MSGTR_LIBVO_VESA_CantInitializeSwscaler "[VO_VESA] Nie mogę zainicjalizować programowego skalowania.\n"
#define MSGTR_LIBVO_VESA_CantUseDga "[VO_VESA] Nie moge użyć DGA. Wymuszam torowy tryb zmieniania. :(\n"
//bank?
#define MSGTR_LIBVO_VESA_UsingDga "[VO_VESA] Używam DGA (zasoby fizyczne: %08lXh, %08lXh)"
#define MSGTR_LIBVO_VESA_CantUseDoubleBuffering "[VO_VESA] Nie mogę użyć podwójnego buforowania: za mało pamięci video.\n"
#define MSGTR_LIBVO_VESA_CantFindNeitherDga "[VO_VESA] Nie mogę odnaleźć DGA ani ramki okna do przeniesienia.\n"
#define MSGTR_LIBVO_VESA_YouveForcedDga "[VO_VESA] Wymusiłeś DGA. Wychodzę\n"
#define MSGTR_LIBVO_VESA_CantFindValidWindowAddress "[VO_VESA] Nie mogę pobrać poprawnego adresu okna.\n"
#define MSGTR_LIBVO_VESA_UsingBankSwitchingMode "[VO_VESA] Używam torowego trybu przełączania (zasoby fizyczne: %08lXh, %08lXh).\n"
#define MSGTR_LIBVO_VESA_CantAllocateTemporaryBuffer "[VO_VESA] Nie mogę zaalokować tymczasowego bufora.\n"
#define MSGTR_LIBVO_VESA_SorryUnsupportedMode "[VO_VESA] Nieobsługiwany tryb -- spróbuj -x 640 -zoom.\n"
#define MSGTR_LIBVO_VESA_OhYouReallyHavePictureOnTv "[VO_VESA] Wow naprawdę masz obraz na TV!\n"
#define MSGTR_LIBVO_VESA_CantInitialozeLinuxVideoOverlay "[VO_VESA] Nie mogę zainicjalizować Nakładki Linux Video.\n"
// Czy LVO to nie jest nazwa? Jak jest to nie powinniśmy tłumaczyć.
#define MSGTR_LIBVO_VESA_UsingVideoOverlay "[VO_VESA] Używam nakładki videop: %s.\n"
#define MSGTR_LIBVO_VESA_CantInitializeVidixDriver "[VO_VESA] Nie mogę zainicjalizować sterownika VIDIX.\n"
#define MSGTR_LIBVO_VESA_UsingVidix "[VO_VESA] Używam VIDIX.\n"
#define MSGTR_LIBVO_VESA_CantFindModeFor "[VO_VESA] Nie mogę znaleźć trybu dla: %ux%u@%u.\n"
#define MSGTR_LIBVO_VESA_InitializationComplete "[VO_VESA] Inicjalizacja VESA gotowa.\n"

// libvo/vo_x11.c

#define MSGTR_LIBVO_X11_DrawFrameCalled "[VO_X11] wywołano draw_frame()!!!!!!\n"

// libvo/vo_xv.c

#define MSGTR_LIBVO_XV_DrawFrameCalled "[VO_XV] wywołano draw_frame()!!!!!!\n"

// stream/stream_radio.c

#define MSGTR_RADIO_ChannelNamesDetected "[radio] Wykryłem nazwy kanałów radiowych.\n"
#define MSGTR_RADIO_WrongFreqForChannel "[radio] Nieprawidłowa częstotliwość dla kanału %s\n"
#define MSGTR_RADIO_WrongChannelNumberFloat "[radio] Nieprawidłowy numer kanału: %.2f\n"
#define MSGTR_RADIO_WrongChannelNumberInt "[radio] Nieprawidłowy numer kanału: %d\n"
#define MSGTR_RADIO_WrongChannelName "[radio] Nieprawidłowa nazwa kanału: %s\n"
#define MSGTR_RADIO_FreqParameterDetected "[radio] Wykryłem częstotliwośc radia.\n"
#define MSGTR_RADIO_DoneParsingChannels "[radio] Skończyłem interpretować kanały.\n"
#define MSGTR_RADIO_GetTunerFailed "[radio] Uwaga: ioctl get tuner nie powiodło się: %s. Ustawiam frac na %d.\n"
#define MSGTR_RADIO_NotRadioDevice "[radio] %s nie jest radiem!\n"
#define MSGTR_RADIO_TunerCapLowYes "[radio] tuner to low:yes frac=%d\n"
#define MSGTR_RADIO_TunerCapLowNo "[radio] tuner to low:no frac=%d\n"
#define MSGTR_RADIO_SetFreqFailed "[radio] ioctl set frequency 0x%x (%.2f) nie powiodło się: %s\n"
#define MSGTR_RADIO_GetFreqFailed "[radio] ioctl get frequency nie powiodło się: %s\n"
#define MSGTR_RADIO_SetMuteFailed "[radio] ioctl set mute nie powiodło się: %s\n"
#define MSGTR_RADIO_QueryControlFailed "[radio] ioctl query control nie powiodło się: %s\n"
#define MSGTR_RADIO_GetVolumeFailed "[radio] ioctl get volume nie powiodło się: %s\n"
#define MSGTR_RADIO_SetVolumeFailed "[radio] ioctl set volume nie powiodło się: %s\n"
#define MSGTR_RADIO_DroppingFrame "\n[radio] szkoda - opuszczam klatkę dźwiękową (%d bajtów)!\n"
#define MSGTR_RADIO_BufferEmpty "[radio] grab_audio_frame: bufor pusty, czekam na %d bajtów danych.\n"
#define MSGTR_RADIO_AudioInitFailed "[radio] audio_in_init nie powiodło się: %s\n"
#define MSGTR_RADIO_AudioBuffer "[radio] Nagrywanie dźwięku - bufor=%d bajtów (blok=%d bajtów).\n"
#define MSGTR_RADIO_AllocateBufferFailed "[radio] nie mogę zaalokować bufora audio (blok=%d,buf=%d): %s\n"
#define MSGTR_RADIO_CurrentFreq "[radio] Obecna częstotliwość: %.2f\n"
#define MSGTR_RADIO_SelectedChannel "[radio] Wybrano kanał: %d - %s (częstotliwość: %.2f)\n"
#define MSGTR_RADIO_ChangeChannelNoChannelList "[radio] Nie mogę zmienić kanału: nie podano listy kanałów .\n"
#define MSGTR_RADIO_UnableOpenDevice "[radio] Nie mogę otworzyć'%s': %s\n"
#define MSGTR_RADIO_RadioDevice "[radio] Radio fd: %d, %s\n"
#define MSGTR_RADIO_InitFracFailed "[radio] init_frac nie powiodło się.\n"
#define MSGTR_RADIO_WrongFreq "[radio] Nieprawidłowa częstotliwość: %.2f\n"
#define MSGTR_RADIO_UsingFreq "[radio] Używam częstotliwości: %.2f.\n"
#define MSGTR_RADIO_AudioInInitFailed "[radio] audio_in_init nie powiodło się.\n"
#define MSGTR_RADIO_BufferString "[radio] %s: w buforze=%d opuszczono=%d\n"
#define MSGTR_RADIO_AudioInSetupFailed "[radio] wywołanie audio_in_setup nie powiodło się: %s\n"
#define MSGTR_RADIO_CaptureStarting "[radio] Zaczynam nagrywanie.\n"
#define MSGTR_RADIO_ClearBufferFailed "[radio] Czyszczenie bufora nie powiodło się: %s\n"
#define MSGTR_RADIO_StreamEnableCacheFailed "[radio] Wywołanie stream_enable_cache nie powiodło się: %s\n"
#define MSGTR_RADIO_DriverUnknownStr "[radio] Nieznana nazwa sterownika: %s\n"
#define MSGTR_RADIO_DriverV4L2 "[radio] Używam interfejsu radiowego V4Lv2.\n"
#define MSGTR_RADIO_DriverV4L "[radio] Używam interfejsu radiowego V4Lv1.\n"

