// Translated by:  Jiri Svoboda, jiri.svoboda@seznam.cz
// Updated by:     Tomas Blaha,  tomas.blaha at kapsa.club.cz
//                 Jiri Heryan,  technik at domotech.cz
// Synced to 1.149
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Pou¾ití:          mplayer [volby] [url|cesta/]jméno_souboru\n"
"\n"
"Základní volby: (úplný seznam najdete v manuálové stránce)\n"
" -vo <roz[:zaø]>  vybere výst. video rozhraní a zaøízení (seznam: -vo help)\n"
" -ao <roz[:zaø]>  vybere výst. audio rozhraní a zaøízení (seznam: -ao help)\n"
#ifdef HAVE_VCD
" vcd://<è_stopy>  pøehraje (S)VCD (Super Video CD) stopu (z nepøipojeného\n"
"                  zaøízení)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<è_tit>    pøehraje DVD titul ze zaøízení (mechaniky), místo ze souboru\n"
" -alang/-slang    zvolí jazyk zvuku/titulkù na DVD (dvouznakový kód zemì)\n"
#endif
" -ss <pozice>     pøevine na zadanou pozici (sekundy nebo hh:mm:ss)\n"
" -nosound         pøehrávání beze zvuku\n"
" -fs              celoobrazovkové pøehrávání (nebo -vm -zoom, viz manuál)\n"
" -x <x> -y <y>    rozli¹ení obrazu (pro pou¾ití s -vm nebo -zoom)\n"
" -sub <soubor>    zvolí soubor s titulky (viz také -subfps, -subdelay)\n"
" -playlist <soubor> urèí soubor s playlistem\n"
" -vid x -aid y    vybere video (x) a audio (y) proud pro pøehrání\n"
" -fps x -srate y  zmìnit video (x fps) a audio (y Hz) frekvence\n"
" -pp <kvalita>    aktivovat postprocessing (podrobnosti v manuálu)\n"
" -framedrop       povolit zahazování snímkù (pro pomalé stroje)\n"
"\n"
"Základní klávesy: (úplný seznam je v manuálu, viz také input.conf)\n"
" <-  nebo  ->     pøevíjení vzad/vpøed o 10 sekund\n"
" nahoru èi dolù   pøevíjení vzad/vpøed o 1 minutu\n"
" pgup èi pgdown   pøevíjení vzad/vpøed o 10 minut\n"
" < nebo >         posun na pøedchozí/dal¹í soubor v playlistu\n"
" p nebo mezerník  pozastaví pøehrávání (pokraèuje po stisku jakékoliv klávesy)\n"
" q nebo ESC       konec pøehrávání a ukonèení programu\n"
" + nebo -         upraví zpo¾dìní zvuku v krocích +/- 0,1 sekundy\n"
" o                cyklická zmìna re¾imu OSD: nic / pozice / pozice a èas\n"
" * nebo /         pøidá nebo ubere PCM hlasitost\n"
" z nebo x         upraví zpo¾dìní titulkù v krocích +/- 0,1 sekundy\n"
" r nebo t         upraví polohu titulkù nahoru/dolù, viz také -vf expand\n"
"\n"
" * * * V MAN STRÁNCE NAJDETE PODROBNOSTI, DAL©Í VOLBY A KLÁVESY * * *\n"
"\n";
#endif

#define MSGTR_SamplesWanted "Vzorky tohoto formátu jsou potøeba pro zlep¹ení podpory. Kontaktujte prosím\n vývojový tým.\n"

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nKonèím...\n"
#define MSGTR_ExitingHow "\nKonèím... (%s)\n"
#define MSGTR_Exit_quit "Konec"
#define MSGTR_Exit_eof "Konec souboru"
#define MSGTR_Exit_error "Kritická chyba"
#define MSGTR_IntBySignal "\nMPlayer pøeru¹en signálem %d v modulu %s.\n"
#define MSGTR_NoHomeDir "Nemohu nalézt domácí adresáø.\n"
#define MSGTR_GetpathProblem "Nastal problém s get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Vytváøím konfiguraèní soubor: %s\n"
#define MSGTR_InvalidAOdriver "Neplatné jméno výstupního audio rozhraní: %s\nSeznam dostupných rozhraní zobrazíte pomocí '-ao help'.\n"
#define MSGTR_CopyCodecsConf "(Zkopírujte/nalinkujte etc/codecs.conf ze zdrojových kódù MPlayeru do ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Pou¾ívám zabudovaný výchozí codecs.conf.\n"
#define MSGTR_CantLoadFont "Nemohu naèíst font: %s\n"
#define MSGTR_CantLoadSub "Nemohu naèíst titulky: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: Kritická chyba: Chybí po¾adovaný datový proud!\n"
#define MSGTR_CantOpenDumpfile "Nelze otevøít soubor pro dump.\n"
#define MSGTR_CoreDumped "Jádro vydumpováno ;)\n"
#define MSGTR_FPSnotspecified "Údaj o FPS v hlavièce souboru je ¹patný nebo chybí, pou¾ijte volbu -fps!\n"
#define MSGTR_TryForceAudioFmtStr "Pokou¹ím se vynutit rodinu audiokodeku %s...\n"
#define MSGTR_CantFindAudioCodec "Nemohu nalézt kodek pro audio formát 0x%X!\n"
#define MSGTR_RTFMCodecs "Pøeètìte si DOCS/HTML/en/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Poku¹ím se vynutit rodinu videokodeku %s...\n"
#define MSGTR_CantFindVideoCodec "Nemohu nalézt kodek pro vybraný -vo a video formát 0x%X.\n"
#define MSGTR_CannotInitVO "Kritická chyba: Nemohu inicializovat video rozhraní!\n"
#define MSGTR_CannotInitAO "Nepodaøilo se otevøít/inicializovat audio zaøízení -> nebude zvuk.\n"
#define MSGTR_StartPlaying "Zaèínám pøehrávat...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***********************************************************\n"\
"         ****  Vá¹ systém je pøíli¹ POMALÝ pro toto pøehrávání! ****\n"\
"         ***********************************************************\n\n"\
"Mo¾né pøíèiny, problémy a øe¹ení:\n"\
"- Nejèastìj¹í: ¹patný/chybný _zvukový_ ovladaè!\n"\
"  - Zkuste -ao sdl nebo pou¾ijte OSS emulaci z ALSA.\n"\
"  - Pohrajte si s rùznými hodnotami -autosync, pro zaèátek tøeba 30.\n"\
"- Pomalý obrazový výstup\n"\
"  - Zkuste jiný -vo ovladaè (seznam: -vo help) nebo zkuste -framedrop!\n"\
"- Pomalá CPU\n"\
"  - Nezkou¹ejte pøehrát velké DVD/DivX na pomalé CPU! Zkuste -hardframedrop.\n"\
"- Po¹kozený soubor.\n"\
"  - Zkuste rùzné kombinace voleb -nobps -ni -forceidx -mc 0.\n"\
"- Pøehráváte z pomalého média (NFS/SMB, DVD, VCD, atd.)\n"\
"  - Zkuste -cache 8192.\n"\
"- Pou¾íváte -cache pro neprokládané AVI soubory?\n"\
"  - Zkuste -nocache.\n"\
"Tipy na vyladìní a zrychlení najdete v DOCS/HTML/en/devices.html.\n"\
"Pokud nic z toho nepomù¾e, pøeètìte si DOCS/HTML/en/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer byl pøelo¾en BEZ podpory GUI.\n"
#define MSGTR_GuiNeedsX "GUI MPlayeru vy¾aduje X11.\n"
#define MSGTR_Playing "Pøehrávám %s\n"
#define MSGTR_NoSound "Audio: ¾ádný zvuk\n"
#define MSGTR_FPSforced "FPS vynuceno na hodnotu %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Pøelo¾eno s detekcí CPU za bìhu - VAROVÁNÍ - toto není optimální!\nAbyste získali co nejvìt¹í výkon, pøelo¾te znovu mplayer ze zdrojového kódu\ns volbou --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Pøelo¾eno pro CPU x86 s roz¹íøeními:"
#define MSGTR_AvailableVideoOutputDrivers "Dostupná video rozhraní:\n"
#define MSGTR_AvailableAudioOutputDrivers "Dostupná audio rozhraní:\n"
#define MSGTR_AvailableAudioCodecs "Dostupné audio kodeky:\n"
#define MSGTR_AvailableVideoCodecs "Dostupné video kodeky:\n"
#define MSGTR_AvailableAudioFm "Dostupné (zakompilované) rodiny audio kodekù/ovladaèù:\n"
#define MSGTR_AvailableVideoFm "Dostupné (zakompilované) rodiny video kodekù/ovladaèù:\n"
#define MSGTR_AvailableFsType "Dostupné re¾imy zmìny hladiny pøi celoobrazovkovém zobrazení:\n"
#define MSGTR_UsingRTCTiming "Pro èasování pou¾ity linuxové hardwarové RTC (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Video: Nelze pøeèíst vlastnosti.\n"
#define MSGTR_NoStreamFound "Nenalezen ¾ádný datový proud.\n"
#define MSGTR_ErrorInitializingVODevice "Chyba pøi otevírání/inicializaci vybraného video_out (-vo) zaøízení.\n"
#define MSGTR_ForcedVideoCodec "Vynucen video kodek: %s\n"
#define MSGTR_ForcedAudioCodec "Vynucen audio kodek: %s\n"
#define MSGTR_Video_NoVideo "Video: ®ádné video\n"
#define MSGTR_NotInitializeVOPorVO "\nKritická chyba: Nemohu inicializovat video filtry (-vf) nebo video výstup (-vo)!\n"
#define MSGTR_Paused "\n===== POZASTAVENO =====\r"
#define MSGTR_PlaylistLoadUnable "\nNemohu naèíst playlist %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer havaroval kvùli 'Illegal Instruction'.\n"\
"  To mù¾e být chyba v kódu pro rozpoznání CPU za bìhu...\n"\
"  Prosím, pøeètìte si DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer havaroval kvùli 'Illegal Instruction'.\n"\
"  To se obvykle stává, kdy¾ se ho pokusíte spustit na CPU odli¹ném, ne¾ pro který\n"\
"  byl pøelo¾en/optimalizován.\n  Ovìøte si to!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer havaroval kvùli ¹patnému pou¾ití CPU/FPU/RAM.\n"\
"  Pøelo¾te MPlayer s volbou --enable-debug , proveïte 'gdb' backtrace\n"\
"  a disassembly. Detaily najdete v DOCS/HTML/en/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer havaroval. To by se nemìlo stát.\n"\
"  Mù¾e to být chyba v kódu MPlayeru _nebo_ ve va¹ich ovladaèích _nebo_ ve verzi\n"\
"  va¹eho gcc. Pokud si myslíte, ¾e je to chyba MPlayeru, pøeètìte si, prosím,\n"\
"  DOCS/HTML/en/bugreports.html a pokraèujte podle tam uvedeného návodu. My vám nemù¾eme\n"\
"  pomoci, pokud tyto informace neuvedete pøi ohla¹ování mo¾né chyby.\n"
#define MSGTR_LoadingConfig "Naèítám konfiguraci '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: pøidán soubor s titulky (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Chyba pøi otevírání souboru [%s] pro zápis!\n"
#define MSGTR_CommandLine "PøíkazovýØádek:"
#define MSGTR_RTCDeviceNotOpenable "Selhalo otevøení %s: %s (by mìlo být èitelné u¾ivatelem.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Chyba inicializace Linuxových RTC v ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Zkuste pøidat \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" do startovacích\n skriptù va¹eho systému.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Chyba inicializace Linuxových RTC v ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Pou¾ívám %s èasování.\n"
#define MSGTR_MenuInitialized "Menu inicializováno: %s\n"
#define MSGTR_MenuInitFailed "Selhala inicializace menu.\n"
#define MSGTR_Getch2InitializedTwice "VAROVÁNÍ: getch2_init volána dvakrát!\n"
#define MSGTR_DumpstreamFdUnavailable "Nemohu ulo¾it (dump) tento proud - ¾ádný 'fd' není dostupný.\n"
#define MSGTR_FallingBackOnPlaylist "Ustupuji od pokusu o zpracování playlistu %s...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Nemohu otevøít video filtr libmenu s koøenovým menu %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Chyba pøi pøedinicializaci øetìzce audio filtrù!\n"
#define MSGTR_LinuxRTCReadError "Chyba pøi ètení z Linuxových RTC: %s\n"
#define MSGTR_SoftsleepUnderflow "Varování! Podteèení softsleep!\n"
#define MSGTR_EDLSKIPStartStopLength "\nEDL_SKIP: start [%f], stop [%f], délka [%f]\n"
#define MSGTR_AnsSubVisibility "ANS_SUB_VISIBILITY=%ld\n"
#define MSGTR_AnsLength "ANS_LENGTH=%ld\n"
#define MSGTR_AnsVoFullscreen "ANS_VO_FULLSCREEN=%ld\n"
#define MSGTR_AnsPercentPos "ANS_PERCENT_POSITION=%ld\n"
#define MSGTR_DvdnavNullEvent "Nedefinovaná DVDNAV událost?!\n"
#define MSGTR_DvdnavHighlightEventBroken "DVDNAV událost: Vadné zvýrazòování událostí\n"
#define MSGTR_DvdnavEvent "DVDNAV událost: %s\n"
#define MSGTR_DvdnavHighlightHide "DVDNAV událost: Highlight Hide\n"
#define MSGTR_DvdnavStillFrame "######################################## DVDNAV událost: Stojící snímek: %d sek.\n"
#define MSGTR_DvdnavNavStop "DVDNAV událost: Nav Stop\n"
#define MSGTR_DvdnavNavNOP "DVDNAV událost: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "DVDNAV událost: Nav Zmìna SPU proudu: fyz: %d/%d/%d logický: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "DVDNAV událost: Nav Zmìna SPU proudu: fyz: %d logický: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "DVDNAV událost: Nav Zmìna audio proudu: fyz: %d logický: %d\n"
#define MSGTR_DvdnavNavVTSChange "DVDNAV událost: Nav Zmìna VTS\n"
#define MSGTR_DvdnavNavCellChange "DVDNAV událost: Nav Cell Change\n"
#define MSGTR_DvdnavNavSpuClutChange "DVDNAV událost: Nav Zmìna SPU CLUT\n"
#define MSGTR_DvdnavNavSeekDone "DVDNAV událost: Nav Pøevíjení Dokonèeno\n"
#define MSGTR_MenuCall "Volání menu\n"

#define MSGTR_EdlCantUseBothModes "Nelze pou¾ít -edl a -edlout zároveò.\n"
#define MSGTR_EdlOutOfMem "Nelze alokovat dostatek pamìti pro vlo¾ení EDL dat.\n"
#define MSGTR_EdlRecordsNo "Naèítám %d EDL akcí.\n"
#define MSGTR_EdlQueueEmpty "Ve¹keré EDL akce ji¾ byly provedeny.\n"
#define MSGTR_EdlCantOpenForWrite "Nelze otevøít EDL soubor [%s] pro zápis.\n"
#define MSGTR_EdlCantOpenForRead "Nelze otevøít EDL soubor [%s] pro ètení.\n"
#define MSGTR_EdlNOsh_video "EDL nelze pou¾ít bez videa, vypínám.\n"
#define MSGTR_EdlNOValidLine "Chybná EDL na øádku: %s\n"
#define MSGTR_EdlBadlyFormattedLine "©patnì formátovaná EDL na øádku [%d] Zahazuji.\n"
#define MSGTR_EdlBadLineOverlap "Poslední stop znaèka byla [%f]; dal¹í start je "\
"[%f]. Vstupy musí být v chronologickém poøadí a nesmí se pøekrývat. Zahazuji.\n"
#define MSGTR_EdlBadLineBadStop "Èasová znaèka stop má být za znaèkou start.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControllFile "Øídicí soubor pro tøíprùchodový re¾im: %s\n"
#define MSGTR_MissingFilename "\nChybí jméno souboru.\n\n"
#define MSGTR_CannotOpenFile_Device "Nelze otevøít soubor/zaøízení.\n"
#define MSGTR_CannotOpenDemuxer "Nelze otevøít demuxer.\n"
#define MSGTR_NoAudioEncoderSelected "\nNebyl vybrán audio enkodér (-oac). Nìjaký vyberte (viz -oac help) nebo pou¾ijte -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nNebyl vybrán video enkodér (-ovc). Nìjaký vyberte (viz  -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Nelze otevøít výstupní soubor '%s'\n"
#define MSGTR_EncoderOpenFailed "Selhalo spu¹tìní enkodéru\n"
#define MSGTR_ForcingOutputFourcc "Vynucuji výstupní fourcc na %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "Zapisuji AVI hlavièku...\n"
#define MSGTR_DuplicateFrames "\n%d opakujících se snímkù!\n"
#define MSGTR_SkipFrame "\nPøeskakuji snímek!\n"
#define MSGTR_ErrorWritingFile "%s: chyba pøi zápisu souboru.\n"
#define MSGTR_WritingAVIIndex "\nZapisuji AVI index...\n"
#define MSGTR_FixupAVIHeader "Opravuji AVI hlavièku...\n"
#define MSGTR_RecommendedVideoBitrate "Doporuèený datový tok videa pro CD %s: %d\n"
#define MSGTR_VideoStreamResult "\nVideo proud: %8.3f kbit/s  (%d bps)  velikost: %d bajtù  %5.3f sekund  %d snímkù\n"
#define MSGTR_AudioStreamResult "\nAudio proud: %8.3f kbit/s  (%d bps)  velikost: %d bajtù  %5.3f sekund\n"
#define MSGTR_OpenedStream "úspìch: formát: %d  data: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videokodek: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "audiokodek: framecopy (formát=%x kanálù=%d frekvence=%ld bitù=%d bps=%ld vzorek-%ld)\n"
#define MSGTR_CBRPCMAudioSelected "vybrán CBR PCM zvuk\n"
#define MSGTR_MP3AudioSelected "vybrán MP3 zvuk\n"
#define MSGTR_CannotAllocateBytes "Nelze alokovat %d bajtù\n"
#define MSGTR_SettingAudioDelay "Nastavuji ZPO®DÌNÍ ZVUKU na %5.3f\n"
#define MSGTR_SettingAudioInputGain "Nastavuji pøedzesílení zvukového vstupu na %f\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Omezuji pøednaèítání zvuku na 0.4s\n"
#define MSGTR_IncreasingAudioDensity "Zvy¹uji hustotu audia na 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Vynucuji pøednaèítání zvuku na 0, max korekci pts  na 0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR zvuk: %ld bajtù/s, %d bajtù/blok\n"
#define MSGTR_LameVersion "LAME ve verzi %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Chyba: Specifikovaný datový tok je mimo rozsah pro tento preset re¾im.\n"\
"\n"\
"Pokud pou¾íváte tento re¾im, musíte zadat hodnotu od \"8\" do \"320\".\n"\
"\n"\
"Dal¹í informace viz: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Chyba: Nezadali jste platný profil a/nebo volby s preset re¾imem.\n"\
"\n"\
"Dostupné profily jsou:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - Implikuje re¾im ABR. Pro jeho pou¾ití,\n"\
"                      jednodu¹e zadejte datový tok. Napøíklad:\n"\
"                      \"preset=185\" aktivuje tento re¾im\n"\
"                      a pou¾ije prùmìrný datový tok 185 kbps.\n"\
"\n"\
"    Nìkolik pøíkladù:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Dal¹í informace viz: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Preset re¾imy jsou navr¾eny tak, aby poskytovaly co nejvy¹¹í mo¾nou kvalitu.\n"\
"\n"\
"Vìt¹ina z nich byla testována a vyladìna pomocí zevrubných zdvojených slepých\n"\
"poslechových testù, za úèelem dosa¾ení a ovìøení této kvality.\n"\
"\n"\
"Nastavení jsou neustále aktualizována v souladu s nejnovìj¹ím vývojem\n"\
"a mìla by poskytovat prakticky nejvy¹¹í mo¾nou kvalitu, jaká je v souèasnosti \n"\
"s kodekem LAME dosa¾itelná.\n"\
"\n"\
"Aktivace preset re¾imù:\n"\
"\n"\
"   Pro re¾imy VBR (v¹eobecnì nejvy¹¹í kvalita):\n"\
"\n"\
"     \"preset=standard\" Tento re¾im by mìl být jasnou volbou\n"\
"                             pro vìt¹inu lidí a hudebních ¾ánrù a má\n"\
"                             ji¾ vysokou kvalitu.\n"\
"\n"\
"     \"preset=extreme\" Pokud máte výjimeènì dobrý sluch a odpovídající\n"\
"                             vybavení, tento re¾im obecnì poskytuje\n"\
"                             mírnì vy¹¹í kvalitu ne¾ re¾im \"standard\".\n"\
"\n"\
"   Pro CBR 320kbps (nejvy¹¹í mo¾ná kvalita ze v¹ech preset re¾imù):\n"\
"\n"\
"     \"preset=insane\"  Tento re¾im je pro vìt¹inu lidí a situací\n"\
"                             pøedimenzovaný, ale pokud vy¾adujete\n"\
"                             absolutnì nejvy¹¹í kvalitu bez ohledu na\n"\
"                             velikost souboru, je toto va¹e volba.\n"\
"\n"\
"   Pro re¾imy ABR (vysoká kvalita pøi daném datovém toku, ale ne jako VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Pou¾itím tohoto re¾imu obvykle dosáhnete dobré\n"\
"                             kvality pøi daném datovém toku. V závislosti\n"\
"                             na zadaném toku tento preset odvodí optimální\n"\
"                             nastavení pro danou situaci.\n"\
"                             Aèkoli tento pøístup funguje, není ani zdaleka\n"\
"                             tak flexibilní jako VBR, a obvykle nedosahuje\n"\
"                             stejné úrovnì kvality jako VBR na vy¹¹ích dato-\n"\
"                             vých tocích.\n"\
"\n"\
"Pro odpovídající profily jsou také dostupné následující volby:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR re¾im) - Implikuje re¾im ABR. Pro jeho pou¾ití,\n"\
"                      jednodu¹e zadejte datový tok. Napøíklad:\n"\
"                      \"preset=185\" aktivuje tento re¾im\n"\
"                      a pou¾ije prùmìrný datový tok 185 kbps.\n"\
"\n"\
"   \"fast\" - V daném profilu aktivuje novou rychlou VBR kompresi.\n"\
"            Nevýhodou je obvykle mírnì vy¹¹í datový tok ne¾ v normálním\n"\
"            re¾imu a také mù¾e dojít k mírnému poklesu kvality.\n"\
"   Varování:v souèasné verzi mù¾e nastavení \"fast\" vést k pøíli¹\n"\
"            vysokému datovému toku ve srovnání s normálním nastavením.\n"\
"\n"\
"   \"cbr\"  - Pokud pou¾ijete re¾im ABR (viz vý¹e) s významným\n"\
"            datovým tokem, napø. 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            mù¾ete pou¾ít volbu \"cbr\" k vynucení kódování v re¾imu CBR\n"\
"            (konstantní tok) namísto standardního ABR re¾imu. ABR poskytuje\n"\
"            lep¹í kvalitu, ale CBR mù¾e být u¾iteèný v situacích jako je\n"\
"            vysílání mp3 proudu po internetu.\n"\
"\n"\
"    Napøíklad:\n"\
"\n"\
"      \"-lameopts fast:preset=standard  \"\n"\
" nebo \"-lameopts  cbr:preset=192       \"\n"\
" nebo \"-lameopts      preset=172       \"\n"\
" nebo \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Pro ABR re¾im je k dispozici nìkolik zkratek:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_ConfigfileError "chyba konfiguraèního souboru"
#define MSGTR_ErrorParsingCommandLine "chyba pøi zpracovávání pøíkazového øádku"
#define MSGTR_VideoStreamRequired "Videoproud je povinný!\n"
#define MSGTR_ForcingInputFPS "místo toho bude vstupní fps interpretováno jako %5.2f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Výstupní formát souboru RAWVIDEO nepodporuje zvuk - vypínám ho\n"
#define MSGTR_DemuxerDoesntSupportNosound "Tento demuxer zatím nepodporuje -nosound.\n"
#define MSGTR_MemAllocFailed "alokace pamìti selhala"
#define MSGTR_NoMatchingFilter "Nemohu najít odpovídající filtr/ao formát!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, mo¾ná je vadný pøekladaè C?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, chybí jméno kodeku!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, nemohu najít enkodér pro kodek %s\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, nemohu alokovat kontext!\n"
#define MSGTR_CouldntOpenCodec "Nelze otevøít kodek %s, br=%d\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metoda promìnného datového toku\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(výchozí)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           prùmìrný datový tok\n"\
"\n"\
" cbr           konstantní datový tok\n"\
"               Vynutí také metodu CBR pro následné ABR preset re¾imy\n"\
"\n"\
" br=<0-1024>   urèení datového toku v kBit (pouze CBR a ABR)\n"\
"\n"\
" q=<0-9>       kvalita (0-nejvy¹¹í, 9-nejni¾¹í) (pouze pro VBR)\n"\
"\n"\
" aq=<0-9>      kvalita algoritmu (0-nejlep¹í/nejpomalej¹í, 9-nejhor¹í/nejrychlej¹í)\n"\
"\n"\
" ratio=<1-100> kompresní pomìr\n"\
"\n"\
" vol=<0-10>    zesílení zvuku\n"\
"\n"\
" mode=<0-3>    (výchozí: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: ne\n"\
"                1: v¹e\n"\
"                2: upravit\n"\
"\n"\
" fast          Zapíná rychlej¹í enkódování pro následné VBR preset re¾imy,\n"\
"               poskytuje o nìco ni¾¹í kvalitu a vy¹¹í datový tok.\n"\
"\n"\
" preset=<hodnota> Pøednastavené profily poskytující maximání kvalitu.\n"\
"                  medium: enkódování metodou VBR, dobrá kvalita\n"\
"                   (datový tok 150-180 kbps)\n"\
"                  standard: enkódování metodou VBR, vysoká kvalita\n"\
"                   (datový tok 170-210 kbps)\n"\
"                  extreme: enkódování metodou VBR, velmi vysoká kvalita\n"\
"                   (datový tok 200-240 kbps)\n"\
"                  insane: enkódování metodou CBR, nejvy¹¹í preset kvalita\n"\
"                   (datový tok 320 kbps)\n"\
"                  <8-320>: hodnota prùmìrného datového toku pro metodu ABR.\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "zdvojené FourCC"
#define MSGTR_TooManyFourccs "pøíli¾ mnoho FourCC/formátù..."
#define MSGTR_ParseError "chyba interpretace (parse)"
#define MSGTR_ParseErrorFIDNotNumber "chyba interpretace (ID formátu, nikoli èíslo?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "chyba interpretace (alias ID formátu, nikoli èíslo?)"
#define MSGTR_DuplicateFID "zdvojené ID formátu"
#define MSGTR_TooManyOut "pøíli¾ mnoho výstupu..."
#define MSGTR_InvalidCodecName "\njméno kodeku(%s) není platné!\n"
#define MSGTR_CodecLacksFourcc "\nkodek(%s) nemá FourCC/formát!\n"
#define MSGTR_CodecLacksDriver "\nkodek(%s) nemá driver!\n"
#define MSGTR_CodecNeedsDLL "\nkodek(%s) vy¾aduje 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\nkodek(%s) vy¾aduje 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Nelze alokovat pamì» pro komentáø. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "Naèítám %s: "
#define MSGTR_CantOpenFileError "Nelze otevøít '%s': %s\n"
#define MSGTR_CantGetMemoryForLine "Nemám pamì» pro 'line': %s\n"
#define MSGTR_CantReallocCodecsp "Nelze pøealokovat '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "Jméno kodeku '%s' není jedineèné."
#define MSGTR_CantStrdupName "Nelze provést strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Nelze provést strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Nelze provést strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Nelze provést strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d audio & %d video kodekù\n"
#define MSGTR_CodecDefinitionIncorrect "Kodek není správnì definován."
#define MSGTR_OutdatedCodecsConf "Tento codecs.conf je pøíli¾ starý a nekompatibilní s tímto sestavením  MPlayeru!"

// divx4_vbr.c:
#define MSGTR_OutOfMemory "nedostatek pamìti"
#define MSGTR_OverridingTooLowBitrate "Nastavený datový tok je pøíli¾ malý pro tento klip.\n"\
"Minimální mo¾ný datový tok pro tento klip  je %.0f kbps. Pøepisuji\n"\
"u¾ivatelem nastavenou hodnotu.\n"

// fifo.c
#define MSGTR_CannotMakePipe "Nelze vytvoøit ROURU!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Nalezen pøíli¾ starý save slot z lvl %d: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "Volbu %s nelze pou¾ít v konfiguraèním souboru\n"
#define MSGTR_InvalidCmdlineOption "Volbu %s nelze pou¾ít z pøíkazového øádku\n"
#define MSGTR_InvalidSuboption "Chyba: volba '%s' nemá ¾ádnou podvolbu '%s'\n"
#define MSGTR_MissingSuboptionParameter "Chyba: podvloba '%s' volby '%s' musí mít parametr!\n"
#define MSGTR_MissingOptionParameter "Chyba: volba '%s' musí mít parametr!\n"
#define MSGTR_OptionListHeader "\n Název                Typ             Min        Max      Globál  CL    Konfig\n\n"
#define MSGTR_TotalOptions "\nCelkem: %d voleb\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM zaøízení '%s' nebylo nalezeno.\n"
#define MSGTR_ErrTrackSelect "Chyba pøi výbìru VCD stopy."
#define MSGTR_ReadSTDIN "Ètu ze std. vstupu...\n"
#define MSGTR_UnableOpenURL "Nelze otevøít URL: %s\n"
#define MSGTR_ConnToServer "Pøipojeno k serveru: %s\n"
#define MSGTR_FileNotFound "Soubor nenalezen: '%s'\n"

#define MSGTR_SMBInitError "Nelze inicializovat knihovnu libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Nemohu otevøít soubor ze sítì: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer nebyl pøelo¾en s podporou ètení SMB.\n"

#define MSGTR_CantOpenDVD "Nelze otevøít DVD zaøízení: %s\n"
#define MSGTR_DVDwait "Naèítám strukturu disku, èekejte prosím...\n"
#define MSGTR_DVDnumTitles "Na tomto DVD je %d titul(ù).\n"
#define MSGTR_DVDinvalidTitle "Neplatné èíslo DVD titulu: %d\n"
#define MSGTR_DVDnumChapters "V tomto DVD titulu je %d kapitol.\n"
#define MSGTR_DVDinvalidChapter "Neplatné èíslo DVD kapitoly: %d\n"
#define MSGTR_DVDnumAngles "Tento DVD titul má %d úhlù pohledu.\n"
#define MSGTR_DVDinvalidAngle "Neplatné èíslo DVD úhlu pohledu: %d\n"
#define MSGTR_DVDnoIFO "Nelze otevøít IFO soubor pro DVD titul %d.\n"
#define MSGTR_DVDnoVOBs "Nelze otevøít VOBy titulu (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD úspì¹nì otevøeno.\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "VAROVÁNÍ: Hlavièka audio proudu %d pøedefinována!\n"
#define MSGTR_VideoStreamRedefined "VAROVÁNÍ: Hlavièka video proudu %d pøedefinována!\n"
#define MSGTR_TooManyAudioInBuffer "\nPøíli¹ mnoho audio paketù ve vyrovnávací pamìti: (%d v %d bajtech)\n"
#define MSGTR_TooManyVideoInBuffer "\nPøíli¹ mnoho video paketù ve vyrovnávací pamìti: (%d v %d bajtech)\n"
#define MSGTR_MaybeNI "Mo¾ná pøehráváte neprokládaný proud/soubor nebo kodek selhal?\n"\
		      "V AVI souborech zkuste vynutit neprokládaný re¾im pomocí volby -ni.\n"
#define MSGTR_SwitchToNi "\nDetekován ¹patnì prokládaný AVI soubor - pøepínám do -ni re¾imu...\n"
#define MSGTR_Detected_XXX_FileFormat "Detekován formát souboru %s.\n"
#define MSGTR_DetectedAudiofile "Detekován zvukový soubor.\n"
#define MSGTR_NotSystemStream "Toto není formát MPEG System Stream... (mo¾ná Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "©patný MPEG-ES proud??? Kontaktujte autora, mo¾ná to je chyba :(\n"
#define MSGTR_FormatNotRecognized "======= Bohu¾el, formát tohoto souboru nebyl rozpoznán/není podporován =======\n"\
                                  "==== Pokud je soubor AVI, ASF nebo MPEG proud, kontaktujte prosím autora! ====\n"
#define MSGTR_MissingVideoStream "No video stream found.\n"
#define MSGTR_MissingAudioStream "No audio stream found -> no sound.\n"
#define MSGTR_MissingVideoStreamBug "Missing video stream!? Contact the author, it may be a bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: File doesn't contain the selected audio or video stream.\n"

#define MSGTR_NI_Forced "Vynucen"
#define MSGTR_NI_Detected "Detekován"
#define MSGTR_NI_Message "%s NEPROKLÁDANÝ formát AVI souboru.\n"

#define MSGTR_UsingNINI "Pou¾ívám NEPROKLÁDANÉ vadné formátování AVI souboru.\n"
#define MSGTR_CouldntDetFNo "Nelze urèit poèet snímkù (pro absolutní posun)\n"
#define MSGTR_CantSeekRawAVI "Nelze se posouvat v surových (raw) AVI proudech! (Potøebuji index, zkuste pou¾ít volbu -idx.)\n"
#define MSGTR_CantSeekFile "Nemohu se posouvat v tomto souboru.\n"

#define MSGTR_EncryptedVOB "©ifrovaný soubor VOB! Pøeètìte si DOCS/HTML/en/dvd.html.\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimované hlavièky vy¾adují ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: VAROVÁNÍ: Promìnná FOURCC detekována!?\n"
#define MSGTR_MOVtooManyTrk "MOV: VAROVÁNÍ: pøíli¹ mnoho stop"
#define MSGTR_FoundAudioStream "==> Nalezen audio proud: %d\n"
#define MSGTR_FoundVideoStream "==> Nalezen video proud: %d\n"
#define MSGTR_DetectedTV "Detekována TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Nelze otevøít ogg demuxer.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Hledám audio proud (id: %d).\n"
#define MSGTR_CannotOpenAudioStream "Nemohu otevøít audio proud: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nemohu otevøít proud s titulky: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Nepovedlo se otevøít audio demuxer: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Nepovedlo se otevøít demuxer pro titulky: %s\n"
#define MSGTR_TVInputNotSeekable "TV vstup neumo¾òuje posun! (\"Posun\" bude pravdìpodobnì pou¾it pro zmìnu kanálù ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Informace o demuxeru %s je ji¾ pøítomna!\n"
#define MSGTR_ClipInfo "Informace o klipu:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: detekováno 30fps NTSC, pøepínám frekvenci snímkù.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: detekováno 24fps progresivní NTSC, pøepínám frekvenci snímkù.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nelze otevøít kodek.\n"
#define MSGTR_CantCloseCodec "Nelze uzavøít kodek.\n"

#define MSGTR_MissingDLLcodec "CHYBA: Nelze otevøít po¾adovaný DirectShow kodek %s.\n"
#define MSGTR_ACMiniterror "Nemohu naèíst/inicializovat Win32/ACM AUDIO kodek (chybí DLL soubor?).\n"
#define MSGTR_MissingLAVCcodec "Nemohu najít kodek '%s' v libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: KRITICKÁ CHYBA: Konec souboru v prùbìhu vyhledávání hlavièky sekvence.\n"
#define MSGTR_CannotReadMpegSequHdr "KRITICKÁ CHYBA: Nelze pøeèíst hlavièku sekvence.\n"
#define MSGTR_CannotReadMpegSequHdrEx "KRITICKÁ CHYBA: Nelze pøeèíst roz¹íøení hlavièky sekvence.\n"
#define MSGTR_BadMpegSequHdr "MPEG: ©patná hlavièka sekvence.\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: ©patné roz¹íøení hlavièky sekvence.\n"

#define MSGTR_ShMemAllocFail "Nelze alokovat sdílenou pamì»\n"
#define MSGTR_CantAllocAudioBuf "Nelze alokovat vyrovnávací pamì» pro zvukový výstup\n"

#define MSGTR_UnknownAudio "Neznámý/chybìjící audio formát -> nebude zvuk.\n"

#define MSGTR_UsingExternalPP "[PP] Pou¾ívám externí filtr pro postprocessing, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Pou¾ívám integrovaný postprocessing kodeku, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Video atribut '%s' není podporován vybraným vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Po¾adovaná rodina video kodeku [%s] (vfm=%s) není dostupná.\nAktivujte ji pøi kompilaci.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Po¾adovaná rodina audio kodeku [%s] (afm=%s) není dostupná.\nAktivujte ji pøi kompilaci.\n"
#define MSGTR_OpeningVideoDecoder "Otevírám video dekodér: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Otevírám audio dekodér: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit video: %s\n"
#define MSGTR_UninitAudioStr "uninit audio: %s\n"
#define MSGTR_VDecoderInitFailed "VDecoder - inicializace selhala :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder - inicializace selhala :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder - pøedinicializace selhala :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alokuji %d bytù pro vstupní vyrovnávací pamì»\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alokuji %d + %d = %d bytù pro výstupní vyrovnávací pamì»\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Zapínám podporu LIRC...\n"
#define MSGTR_LIRCdisabled "Nebudete moci pou¾ívat dálkový ovladaè.\n"
#define MSGTR_LIRCopenfailed "Nepovedlo se zapnout podporu LIRC.\n"
#define MSGTR_LIRCcfgerr "Nepovedlo se pøeèíst konfiguraèní soubor LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nemohu nalézt video filtr '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Nemohu otevøít video filtr '%s'\n"
#define MSGTR_OpeningVideoFilter "Otevírám video filtr: "
#define MSGTR_CannotFindColorspace "Ani pøi vlo¾ení 'scale' nemohu nalézt spoleèný barevný prostor :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Kodek nenastavil sh->disp_w a sh->disp_h, pokou¹ím se to obejít.\n"
#define MSGTR_VoConfigRequest "VDec: Po¾adovaná konfigurace vo - %d x %d (preferovaný csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Nemohu nalézt spoleèný barevný prostor - zkou¹ím to znovu s -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Pomìr stran obrazu filmu je %.2f:1 - ¹káluji na správný pomìr.\n"
#define MSGTR_MovieAspectUndefined "Pomìr stran obrazu filmu není definován - nemìním velikost.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Potøebujete aktualizovat nebo nainstalovat binární kodeky.\nJdìte na http://mplayerhq.hu/homepage/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Inicializace Win32/DShow videokodeku OK.\n"
#define MSGTR_DMOInitOK "INFO: Inicializace Win32/DMO videokodeku OK.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Nemohu poslat událost EWMH fullscreen!\n"

#define MSGTR_InsertingAfVolume "[Mixer] Hardwarový mixér není k dispozici, vkládám filtr pro hlasitost.\n"
#define MSGTR_NoVolume "[Mixer] Øízení hlasitosti není dostupné.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O aplikaci"
#define MSGTR_FileSelect "Vybrat soubor..."
#define MSGTR_SubtitleSelect "Vybrat titulky..."
#define MSGTR_OtherSelect "Vybrat..."
#define MSGTR_AudioFileSelect "Vybrat externí zvukový kanál..."
#define MSGTR_FontSelect "Vybrat font..."
#define MSGTR_PlayList "Playlist"
#define MSGTR_Equalizer "Ekvalizér"
#define MSGTR_SkinBrowser "Prohlí¾eè témat"
#define MSGTR_Network "Sí»ové vysílání..."
#define MSGTR_Preferences "Nastavení" // Pøedvolby?
#define MSGTR_AudioPreferences "Konfigurace ovladaèe zvuku"
#define MSGTR_NoMediaOpened "Nic není otevøeno."
#define MSGTR_VCDTrack "VCD stopa %d"
#define MSGTR_NoChapter "®ádná kapitola" //bez kapitoly?
#define MSGTR_Chapter "Kapitola %d"
#define MSGTR_NoFileLoaded "Není naèten ¾ádný soubor."

// --- buttons ---
#define MSGTR_Ok "OK"
#define MSGTR_Cancel "Zru¹it"
#define MSGTR_Add "Pøidat"
#define MSGTR_Remove "Odebrat"
#define MSGTR_Clear "Vynulovat"
#define MSGTR_Config "Konfigurace"
#define MSGTR_ConfigDriver "Konfigurovat ovladaè"
#define MSGTR_Browse "Prohlí¾et"

// --- error messages ---
#define MSGTR_NEMDB "Bohu¾el není dostatek pamìti pro vykreslovací mezipamì»."
#define MSGTR_NEMFMR "Bohu¾el není dostatek pamìti pro vykreslení menu."
#define MSGTR_IDFGCVD "Bohu¾el nebyl nalezen video ovladaè kompatibilní s GUI."
#define MSGTR_NEEDLAVCFAME "Bohu¾el nelze pøehrávat ne-MPEG s kartou DXR3/H+ bez pøeenkódování.\nProsím, zapnìte lavc nebo fame v konfiguraci DXR3/H+."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[témata] chyba v konfiguraèním souboru témat na øádce %d: %s"
#define MSGTR_SKIN_WARNING1 "[témata] varování v konfiguraèním souboru témat na øádce %d:\nwidget nalezen ale pøed ním nebyla nalezena ¾ádná \"section\" (%s)"
#define MSGTR_SKIN_WARNING2 "[témata] varování v konfiguraèním souboru témat na øádce %d:\nwidget nalezen ale pøed ním nebyla nalezena ¾ádná \"subsection\" (%s)"
#define MSGTR_SKIN_WARNING3 "[témata] varování v konfiguraèním souboru témat na øádce %d:\nwidget (%s) nepodporuje tuto subsekci"
#define MSGTR_SKIN_BITMAP_16bit  "bitmapa s hloubkou 16 bitù a ménì není podporována (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "soubor nenalezen (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "chyba ètení BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "chyba ètení TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "chyba ètení PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "formát TGA zapouzdøený v RLE není podporován (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "neznámý typ souboru (%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "chyba konverze z 24 do 32 bitù (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "neznámá zpráva: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nedostatek pamìti\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "deklarováno pøíli¹ mnoho fontù\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "soubor fontu nebyl nalezen\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "soubor obrazu fontu nebyl nalezen\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "neexistující identifikátor fontu (%s)\n"
#define MSGTR_SKIN_UnknownParameter "neznámý parametr (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Téma nenalezeno (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "chyba pøi ètení konfiguraèního souboru témat (%s)\n"
#define MSGTR_SKIN_LABEL "Témata:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O aplikaci MPlayer"
#define MSGTR_MENU_Open "Otevøít..."
#define MSGTR_MENU_PlayFile "Pøehrát soubor..."
#define MSGTR_MENU_PlayVCD "Pøehrát VCD..."
#define MSGTR_MENU_PlayDVD "Pøehrát DVD..."
#define MSGTR_MENU_PlayURL "Pøehrát z URL..."
#define MSGTR_MENU_LoadSubtitle "Naèíst titulky..."
#define MSGTR_MENU_DropSubtitle "Zahodit titulky..."
#define MSGTR_MENU_LoadExternAudioFile "Naèíst externí soubor se zvukem..."
#define MSGTR_MENU_Playing "Ovládání pøehrávání"
#define MSGTR_MENU_Play "Pøehrát"
#define MSGTR_MENU_Pause "Pozastavit"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Dal¹í proud"
#define MSGTR_MENU_PrevStream "Pøedchozí proud"
#define MSGTR_MENU_Size "Velikost"
#define MSGTR_MENU_NormalSize "Normální velikost"
#define MSGTR_MENU_DoubleSize "Dvojnásobná velikost"
#define MSGTR_MENU_FullScreen "Celá obrazovka"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Pøehrát disk..."
#define MSGTR_MENU_ShowDVDMenu "Zobrazit DVD menu"
#define MSGTR_MENU_Titles "Tituly"
#define MSGTR_MENU_Title "Titul %2d"
#define MSGTR_MENU_None "(¾ádné)"
#define MSGTR_MENU_Chapters "Kapitoly"
#define MSGTR_MENU_Chapter "Kapitola %2d"
#define MSGTR_MENU_AudioLanguages "Jazyk zvuku"
#define MSGTR_MENU_SubtitleLanguages "Jazyk titulkù"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Prohlí¾eè témat"
#define MSGTR_MENU_Preferences "Pøedvolby"
#define MSGTR_MENU_Exit "Konec..."
#define MSGTR_MENU_Mute "Ztlumit"
#define MSGTR_MENU_Original "Pùvodní"
#define MSGTR_MENU_AspectRatio "Pomìr stran"
#define MSGTR_MENU_AudioTrack "Audio stopa"
#define MSGTR_MENU_Track "Stopa %d"
#define MSGTR_MENU_VideoTrack "Video stopa"

// --- equalizer
#define MSGTR_EQU_Audio "Zvuk"
#define MSGTR_EQU_Video "Obraz"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Jas: "
#define MSGTR_EQU_Hue "Odstín: "
#define MSGTR_EQU_Saturation "Sytost: "
#define MSGTR_EQU_Front_Left "Levý pøední"
#define MSGTR_EQU_Front_Right "Pravý pøední"
#define MSGTR_EQU_Back_Left "Levý zadní"
#define MSGTR_EQU_Back_Right "Pravý zadní"
#define MSGTR_EQU_Center "Støedový"
#define MSGTR_EQU_Bass "Basový"
#define MSGTR_EQU_All "V¹e"
#define MSGTR_EQU_Channel1 "Kanál 1:"
#define MSGTR_EQU_Channel2 "Kanál 2:"
#define MSGTR_EQU_Channel3 "Kanál 3:"
#define MSGTR_EQU_Channel4 "Kanál 4:"
#define MSGTR_EQU_Channel5 "Kanál 5:"
#define MSGTR_EQU_Channel6 "Kanál 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Cesta"
#define MSGTR_PLAYLIST_Selected "Vybrané soubory"
#define MSGTR_PLAYLIST_Files "Soubory"
#define MSGTR_PLAYLIST_DirectoryTree "Adresáøe"

// --- preferences
#define MSGTR_PREFERENCES_Audio "Zvuk"
#define MSGTR_PREFERENCES_Video "Obraz"
#define MSGTR_PREFERENCES_SubtitleOSD "Titulky & OSD"
#define MSGTR_PREFERENCES_Codecs "Kodeky & demuxer"
#define MSGTR_PREFERENCES_Misc "Ostatní"

#define MSGTR_PREFERENCES_None "Nic"
#define MSGTR_PREFERENCES_DriverDefault "výchozí nastavení"
#define MSGTR_PREFERENCES_AvailableDrivers "Dostupné ovladaèe:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nepøehrávat zvuk"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizovat zvuk"
#define MSGTR_PREFERENCES_EnEqualizer "Aktivovat ekvalizér"
#define MSGTR_PREFERENCES_ExtraStereo "Aktivovat extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Koeficient:"
#define MSGTR_PREFERENCES_AudioDelay "Zpo¾dìní zvuku"
#define MSGTR_PREFERENCES_DoubleBuffer "Aktivovat dvojitou vyrovnávací pamì»"
#define MSGTR_PREFERENCES_DirectRender "Aktivovat direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Aktivovat zahazování snímkù"
#define MSGTR_PREFERENCES_HFrameDrop "Aktivovat TVRDÉ zahazování snímkù (nebezpeèné)"
#define MSGTR_PREFERENCES_Flip "Pøevrátit obraz vzhùru nohama"
#define MSGTR_PREFERENCES_Panscan "Panscan:"
#define MSGTR_PREFERENCES_OSDTimer "Èas a ostatní ukazatele"
#define MSGTR_PREFERENCES_OSDProgress "Pouze ukazatele pozice a nastavení"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Èas, procenta a celkový èas"
#define MSGTR_PREFERENCES_Subtitle "Titulky:"
#define MSGTR_PREFERENCES_SUB_Delay "Zpo¾dìní: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozice: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Vypnout automatické naètení titulkù"
#define MSGTR_PREFERENCES_SUB_Unicode "Titulky v UNICODE"
#define MSGTR_PREFERENCES_SUB_MPSUB "Pøevést dané titulky do vlastního formátu MPlayeru"
#define MSGTR_PREFERENCES_SUB_SRT "Pøevést dané titulky do èasovì orientovaného formátu SubViewer (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Zapnout pøekrývání titulkù"
#define MSGTR_PREFERENCES_Font "Font:"
#define MSGTR_PREFERENCES_FontFactor "Zvìt¹ení Fontu:"
#define MSGTR_PREFERENCES_PostProcess "Aktivovat postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Automatické øízení kvality:"
#define MSGTR_PREFERENCES_NI "Pou¾ít parser pro neprokládaný AVI formát"
#define MSGTR_PREFERENCES_IDX "Znovu sestavit tabulku indexù, pokud je to tøeba"
#define MSGTR_PREFERENCES_VideoCodecFamily "Rodina video kodeku:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Rodina audio kodeku:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Typ OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Titulky"
#define MSGTR_PREFERENCES_FRAME_Font "Font"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Kodek & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Vyrovnávací pamì»"
#define MSGTR_PREFERENCES_FRAME_Misc "Ostatní"
#define MSGTR_PREFERENCES_Audio_Device "Zaøízení:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixér:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Kanál mixéru:"
#define MSGTR_PREFERENCES_Message "Pozor, nìkterá nastavení potøebují pro svou funkci restartovat pøehrávání!"
#define MSGTR_PREFERENCES_DXR3_VENC "Video enkodér:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Pou¾ít LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Pou¾ít FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Západoevropské jazyky (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Západoevropské jazyky s Eurem (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Slovanské/støedoevropské jazyky (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, gal¹tina, maltéz¹tina, tureètina (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Staré Baltské kódování (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cyrilice (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arab¹tina (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Moderní øeètina (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Tureètina (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltické (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Kelt¹tina (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Hebrej¹tina (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Ru¹tina (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrajin¹tina, bìloru¹tina (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Jednoduchá èín¹tina (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Tradièní èín¹tina (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japon¹tina (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Korej¹tina (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thaj¹tina (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrilické Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Slovanské/støedoevropské Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Bez automatické velikosti"
#define MSGTR_PREFERENCES_FontPropWidth "Proporènì dle ¹íøky obrazu"
#define MSGTR_PREFERENCES_FontPropHeight "Proporènì dle vý¹ky obrazu"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporènì dle úhlopøíèky"
#define MSGTR_PREFERENCES_FontEncoding "Kódování:"
#define MSGTR_PREFERENCES_FontBlur "Rozmazání:"
#define MSGTR_PREFERENCES_FontOutLine "Obrys:"
#define MSGTR_PREFERENCES_FontTextScale "Velikost textu:"
#define MSGTR_PREFERENCES_FontOSDScale "Velikost OSD:"
#define MSGTR_PREFERENCES_Cache "Zapnout vyrovnávací pamì»"
#define MSGTR_PREFERENCES_CacheSize "Velikost vyrovnávací pamìti: "
#define MSGTR_PREFERENCES_LoadFullscreen "Spustit pøes celou obrazovku"
#define MSGTR_PREFERENCES_SaveWinPos "Ulo¾it pozici okna"
#define MSGTR_PREFERENCES_XSCREENSAVER "Zastavit XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Aktivovat playbar"
#define MSGTR_PREFERENCES_AutoSync "Zapnout automatickou synchronizaci"
#define MSGTR_PREFERENCES_AutoSyncValue "Automatická synchronizace: "
#define MSGTR_PREFERENCES_CDROMDevice "Zaøízení CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Zaøízení DVD:"
#define MSGTR_PREFERENCES_FPS "Snímková rychlost (FPS):"
#define MSGTR_PREFERENCES_ShowVideoWindow "Zobrazovat video okno pøi neèinnosti"

#define MSGTR_ABOUT_UHU "Vývoj GUI je sponzorován firmou UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   Hlavní vývojáøi programu MPlayer:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Dal¹í vývojáøi:\n"
#define MSGTR_ABOUT_MainTesters "   Hlavní testeøi:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Kritická chyba!"
#define MSGTR_MSGBOX_LABEL_Error "Chyba!"
#define MSGTR_MSGBOX_LABEL_Warning "Varování!"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Vybrané video_out zaøízení je nekompatibilní s tímto kodekem.\n"
#define MSGTR_VO_GenericError "Tato chyba nastala"
#define MSGTR_VO_UnableToAccess "Nemám pøístup k"
#define MSGTR_VO_ExistsButNoDirectory "ji¾ existuje, ale není to adresáø."
#define MSGTR_VO_DirExistsButNotWritable "Výstupní adresáø ji¾ existuje, ale nelze do nìj zapisovat."
#define MSGTR_VO_DirExistsAndIsWritable "Výstupní adresáø ji¾ existuje a lze do nìj zapisovat."
#define MSGTR_VO_CantCreateDirectory "Nelze vytvoøit výstupní adresáø."
#define MSGTR_VO_CantCreateFile "Nelze vytvoøit výstupní soubor."
#define MSGTR_VO_DirectoryCreateSuccess "Úspì¹nì vytvoøen výstupní adresáø."
#define MSGTR_VO_ParsingSuboptions "Interpretuji podvolby."
#define MSGTR_VO_SuboptionsParsedOK "Podvolby interpretovány OK."
#define MSGTR_VO_ValueOutOfRange "Hodnota je mimo rozsah"
#define MSGTR_VO_NoValueSpecified "Nebyla zadána hodnota."
#define MSGTR_VO_UnknownSuboptions "Neznámá(é) podvolba(y)"

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "Zapnut progresivní JPEG."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "Vypnut progresivní JPEG."
#define MSGTR_VO_JPEG_BaselineJPEG "Zapnut základní JPEG."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Vypnut základní JPEG."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "Zapnut ASCII re¾im."
#define MSGTR_VO_PNM_RawMode "Zapnut surový (Raw) re¾im."
#define MSGTR_VO_PNM_PPMType "Budou zapisovány PPM soubory."
#define MSGTR_VO_PNM_PGMType "Budou zapisovány PGM soubory."
#define MSGTR_VO_PNM_PGMYUVType "Budou zapisovány PGMYUV soubory."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Prokládaný re¾im obrazu vy¾aduje vý¹ku obrazu dìlitelnou 4."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Nelze alokovat øádkovou vyrovnávací pamì» pro re¾im prokládaného obrazu."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Vstup není RGB, nelze oddìlit jasovou slo¾ku podle polí!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "©íøka obrazu musí být dìlitelná 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Není dostatek pamìti pro alokaci RGB framebufferu."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Nelze získat pamì» nebo ukazatel souboru pro zápis \"stream.yuv\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Chyba pøi zápisu obrázku na výstup!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Neznámé podzaøízení: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Pou¾ívám prokládaný výstupní re¾im, horní pole napøed."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Pou¾ívám prokládaný výstupní re¾im, dolní pole napøed."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Pou¾ívám (výchozí) neprokládaný snímkový re¾im."

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "Výstupní videorozhraní pgm bylo nahrazeno -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Výstupní videorozhraní md5 bylo nahrazeno -vo md5sum.\n"

// ======================= AO Audio Output drivers ========================

// libao2 

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: moduly alsa9 a alsa1x byly odstranìny, místo nich pou¾ijte -ao alsa.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Nelze otevøít mixá¾ní zaøízení %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: Mixer zvukové karty nemá kanál '%s', pou¾ívám výchozí.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Nelze otevøít zvukové zaøízení %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Nelze provést blokování souborového deskriptoru: %s\n"
#define MSGTR_AO_OSS_CantSetAC3 "[AO OSS] Zvukové zaøízení %s nelze nastavit na výstup AC3, zkou¹ím S16...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Selhalo nastavení výstupního zvukového zaøízení na %d kanálù.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: Ovladaè nepodporuje SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Ovladaè Va¹í zvukové karty NEPODPORUJE select()  ***\n Pøekompilujte MPlayer s #undef HAVE_AUDIO_SELECT v config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nKritická chyba: *** NELZE ZNOVUOTEVØÍT / RESTARTOVAT ZVUKOVÉ ZAØÍZENÍ *** %s\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Pøipojen ke zvukovému serveru.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Nelze otevøít datový proud.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Datový proud otevøen.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] velikost vyrovnávací pamìti: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Nastavení hlasitosti na %d selhalo.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] dxr2: %d Hz není podporováno, zkuste \"-aop list=resample\"\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound selhalo: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] latence: [server: %0.2fs, sí»: %0.2fs] (upravuji %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] selhalo otevøení datového proudu esd pro pøehrávání: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] selhalo nastavení DVB zvukového mixeru: %s\n" 
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz není podporováno, zkuste resamplovat...\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Soubor: %s (%s)\nPCM: Vzorkování: %iHz Kanál(y): %s Formát %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info:  nejrychlej¹ího dumpingu dosáhnete s -vc dummy -vo null\nPCM: Info: pro zápis WAVE souborù pou¾ijte -waveheader (výchozí).\n" // ví nìkdo co je dumping?
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Selhalo otevøení %s pro zápis!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Vzorkování: %iHz Kanál(y): %s Formát %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] pou¾ívám zvukový ovladaè %s.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Nepodporovaný formát zvuku: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Inicializace SDL Audio selhala: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Nelze otevøít zvuk: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] ovládání.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Vzorkování: %iHz Kanál(y): %s Formát %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] pøehrávání: neplatné zaøízení.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: selhalo setparams: %s\nNelze nastavit po¾adované vzorkování.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE nebyl pøijat daným zdrojem.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: selhalo getparams: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: vzorkování je nyní %lf (po¾adovaný kmitoèet je %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Nelze otevøít zvukový kanál: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: selhalo SETINFO.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: zápis selhal."
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Nelze otevøít zvukové zaøízení %s, %s  -> nebude zvuk.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: Va¹e karta nepodporuje %d kanálové, %s, %d Hz vzorkování.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Ovladaè Va¹í zvukové karty NEPODPORUJE select()  ***\n Pøekompilujte MPlayer s #undef HAVE_AUDIO_SELECT v config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nKritická chyba: *** NELZE ZNOVUOTEVØÍT / RESTARTOVAT ZVUKOVÉ ZAØÍZENÍ (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: po¾adovaný formát: %d Hz, %d kanál(ù), %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: ¾ádná zvuková karta nebyla nalezena.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: po¾adován neplatný formát (%s) - výstup odpojen.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: chyba otevøení pøehrávání zvuku: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: chyba v pcm info: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: nalezeno %d zvukových karet, pou¾ívám: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: chyba info v pcm kanálu: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: chyba pøi nastavování parametrù: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: chyba pøi nastavování kanálu: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: chyba pøi pøípravì kanálu: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: chyba playback drain: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: chyba playback flush: %s\n" //to jsou názvy ¾e by jeden pad
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: chyba uzavøení pcm: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: chyba playback drain: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: chyba playback flush: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: chyba pøi pøípravì kanálù: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: chyba playback drain: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: chyba playback flush: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: chyba pøi pøípravì kanálù: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: podteèení v alsa, restartuji proud.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: chyba pøípravy pøehrávání zvuku: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: chyba pøi zápisu po restartu: %s - giving up.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: vyba výstupu: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] neplatný zásuvný modul: %s\n"
