// Translated by:  Jiri Svoboda, jiri.svoboda@seznam.cz
// Updated by:     Tomas Blaha,  tomas.blaha at kapsa.cz
//                 Jiri Heryan
// Synced with r19979
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Pou¾ití:          mplayer [volby] [url|cesta/]jméno_souboru\n"
"\n"
"Základní volby: (úplný seznam najdete v manuálové stránce)\n"
" -vo <rozhraní>   vybere výstupní video rozhraní (seznam: -vo help)\n"
" -ao <rozhraní>   vybere výstupní audio rozhraní (seznam: -ao help)\n"
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
" dolù èi nahoru   pøevíjení vzad/vpøed o  1 minutu\n"
" pgdown èi pgup   pøevíjení vzad/vpøed o 10 minut\n"
" < nebo >         posun na pøedchozí/dal¹í soubor v playlistu\n"
" p nebo mezerník  pozastaví pøehrávání (pokraèuje po stisku jakékoliv klávesy)\n"
" q nebo ESC       konec pøehrávání a ukonèení programu\n"
" + nebo -         upraví zpo¾dìní zvuku v krocích +/- 0,1 sekundy\n"
" o                cyklická zmìna re¾imu OSD: nic / pozice / pozice a èas\n"
" * nebo /         pøidá nebo ubere PCM hlasitost\n"
" x nebo z         upraví zpo¾dìní titulkù v krocích +/- 0,1 sekundy\n"
" r nebo t         upraví polohu titulkù nahoru/dolù, viz také -vf expand\n"
"\n"
" * * * V MAN STRÁNCE NAJDETE PODROBNOSTI, DAL©Í VOLBY A KLÁVESY * * *\n"
"\n";
#endif

// libmpcodecs/ad_dvdpcm.c:
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
#define MSGTR_CopyCodecsConf "(Zkopírujte/nalinkujte etc/codecs.conf ze zdrojových kódù MPlayeru do ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Pou¾ívám zabudovaný výchozí codecs.conf.\n"
#define MSGTR_CantLoadFont "Nemohu naèíst bitmapový font: %s\n"
#define MSGTR_CantLoadSub "Nemohu naèíst titulky: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: Kritická chyba: Chybí po¾adovaný datový proud!\n"
#define MSGTR_CantOpenDumpfile "Nelze otevøít soubor pro dump.\n"
#define MSGTR_CoreDumped "Jádro odhozeno ;)\n"
#define MSGTR_FPSnotspecified "Údaj o FPS v hlavièce souboru je ¹patný nebo chybí, pou¾ijte volbu -fps!\n"
#define MSGTR_TryForceAudioFmtStr "Pokou¹ím se vynutit rodinu audiokodeku %s...\n"
#define MSGTR_CantFindAudioCodec "Nemohu nalézt kodek pro audio formát 0x%X!\n"
#define MSGTR_RTFMCodecs "Pøeètìte si DOCS/HTML/en/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Pokou¹ím se vynutit rodinu videokodeku %s...\n"
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
"  - Nezkou¹ejte pøehrát velké DVD/DivX na pomalé CPU! Zkuste nìkteré lavdopts,\n"\
"    jako -vfm ffmpeg -lavdopts lowres=1:fast:skiploopfilter=all.\n"\
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
#define MSGTR_Playing "\nPøehrávám %s\n"
#define MSGTR_NoSound "Audio: ¾ádný zvuk\n"
#define MSGTR_FPSforced "FPS vynuceno na hodnotu %5.3f  (vyn. èas: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Pøelo¾eno s detekcí CPU za bìhu."
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
#define MSGTR_AddedSubtitleFile "SUB: Pøidán soubor s titulky (%d): %s\n"
#define MSGTR_RemovedSubtitleFile "SUB: Odebrán soubor s titulky (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Chyba pøi otevírání souboru [%s] pro zápis!\n"
#define MSGTR_CommandLine "Pøíkazový øádek:"
#define MSGTR_RTCDeviceNotOpenable "Selhalo otevøení %s: %s (by mìlo být èitelné u¾ivatelem.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Chyba inicializace Linuxových RTC v ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Zkuste pøidat \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" do startovacích\n skriptù va¹eho systému.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Chyba inicializace Linuxových RTC v ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Pou¾ívám %s èasování.\n"
#define MSGTR_NoIdleAndGui "Volbu -idle nelze pou¾ít pro GMPlayer.\n"
#define MSGTR_MenuInitialized "Menu inicializováno: %s\n"
#define MSGTR_MenuInitFailed "Selhala inicializace menu.\n"
#define MSGTR_Getch2InitializedTwice "VAROVÁNÍ: getch2_init volána dvakrát!\n"
#define MSGTR_DumpstreamFdUnavailable "Nemohu ulo¾it (dump) tento proud - ¾ádný deskriptor souboru není dostupný.\n"
#define MSGTR_FallingBackOnPlaylist "Ustupuji od pokusu o zpracování playlistu %s...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Nemohu otevøít video filtr libmenu s koøenovým menu %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Chyba pøi pøedinicializaci øetìzce audio filtrù!\n"
#define MSGTR_LinuxRTCReadError "Chyba pøi ètení z Linuxových RTC: %s\n"
#define MSGTR_SoftsleepUnderflow "Varování! Podteèení softsleep!\n"
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

#define MSGTR_EdlOutOfMem "Nelze alokovat dostatek pamìti pro vlo¾ení EDL dat.\n"
#define MSGTR_EdlRecordsNo "Naèítám %d EDL akcí.\n"
#define MSGTR_EdlQueueEmpty "Ve¹keré EDL akce ji¾ byly provedeny.\n"
#define MSGTR_EdlCantOpenForWrite "Nelze otevøít EDL soubor [%s] pro zápis.\n"
#define MSGTR_EdlCantOpenForRead "Nelze otevøít EDL soubor [%s] pro ètení.\n"
#define MSGTR_EdlNOsh_video "EDL nelze pou¾ít bez videa, vypínám.\n"
#define MSGTR_EdlNOValidLine "Chybná EDL na øádku: %s\n"
#define MSGTR_EdlBadlyFormattedLine "©patnì formátovaná EDL na øádku [%d], zahazuji.\n"
#define MSGTR_EdlBadLineOverlap "Poslední stop znaèka byla [%f]; dal¹í start je [%f].\n"\
"Vstupy musí být v chronologickém poøadí a nesmí se pøekrývat. Zahazuji.\n"
#define MSGTR_EdlBadLineBadStop "Èasová znaèka stop má být za znaèkou start.\n"
#define MSGTR_EdloutBadStop "EDL: Vynechání zru¹eno, poslední start > stop\n"
#define MSGTR_EdloutStartSkip "EDL: Zaèátek vynechaného bloku, stisknìte znovu 'i' pro ukonèení bloku.\n"
#define MSGTR_EdloutEndSkip "EDL: Konec vynechaného bloku, øádek zapsán.\n"
#define MSGTR_MPEndposNoSizeBased "Volba -endpos v MPlayeru zatím nepodporuje rozmìrové jednotky.\n"

// mplayer.c OSD

#define MSGTR_OSDenabled "zapnuto"
#define MSGTR_OSDdisabled "vypnuto"
#define MSGTR_OSDChannel "Kanál: %s"
#define MSGTR_OSDSubDelay "Zpo¾dìní tit: %d ms"
#define MSGTR_OSDSpeed "Rychlost: x %6.2f"
#define MSGTR_OSDosd "OSD: %s"
#define MSGTR_OSDChapter "Kapitola: (%d) %s"

// property values
#define MSGTR_Enabled "zapnuto"
#define MSGTR_EnabledEdl "zapnuto (EDL)"
#define MSGTR_Disabled "vypnuto"
#define MSGTR_HardFrameDrop "intenzivní"
#define MSGTR_Unknown "neznámé"
#define MSGTR_Bottom "dolù"
#define MSGTR_Center "na støed"
#define MSGTR_Top "nahoru"

// osd bar names
#define MSGTR_Volume "Hlasitost"
#define MSGTR_Panscan "Panscan"
#define MSGTR_Gamma "Gama"
#define MSGTR_Brightness "Jas"
#define MSGTR_Contrast "Kontrast"
#define MSGTR_Saturation "Sytost"
#define MSGTR_Hue "Odstín"

// property state
#define MSGTR_MuteStatus "Zti¹ení: %s"
#define MSGTR_AVDelayStatus "A-V odchylka: %s"
#define MSGTR_OnTopStatus "Zùstat navrchu: %s"
#define MSGTR_RootwinStatus "Koøenové okno: %s"
#define MSGTR_BorderStatus "Rámeèek: %s"
#define MSGTR_FramedroppingStatus "Zahazování snímkù: %s"
#define MSGTR_VSyncStatus "Vertikální synchronizace: %s"
#define MSGTR_SubSelectStatus "Titulky: %s"
#define MSGTR_SubPosStatus "Umístìní titulkù: %s/100"
#define MSGTR_SubAlignStatus "Zarovnání titulkù: %s"
#define MSGTR_SubDelayStatus "Zpo¾dìní titulkù: %s"
#define MSGTR_SubVisibleStatus "Titulky: %s"
#define MSGTR_SubForcedOnlyStatus "Pouze vynucené titulky: %s"
 
// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Øídicí soubor pro tøíprùchodový re¾im: %s\n"
#define MSGTR_MissingFilename "\nChybí jméno souboru.\n\n"
#define MSGTR_CannotOpenFile_Device "Nelze otevøít soubor/zaøízení.\n"
#define MSGTR_CannotOpenDemuxer "Nelze otevøít demuxer.\n"
#define MSGTR_NoAudioEncoderSelected "\nNebyl vybrán audio enkodér (-oac). Nìjaký vyberte (viz -oac help) nebo pou¾ijte -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nNebyl vybrán video enkodér (-ovc). Nìjaký vyberte (viz  -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Nelze otevøít výstupní soubor '%s'\n"
#define MSGTR_EncoderOpenFailed "Selhalo spu¹tìní enkodéru\n"
#define MSGTR_MencoderWrongFormatAVI "\nVAROVÁNÍ: FORMÁT VÝSTUPNÍHO SOUBORU JE _AVI_. Viz -of help.\n"
#define MSGTR_MencoderWrongFormatMPG "\nVAROVÁNÍ: FORMÁT VÝSTUPNÍHO SOUBORU JE _MPEG_. Viz -of help.\n"
#define MSGTR_MissingOutputFilename "Nebyl nastaven výstupní soubor, prostudujte si volbu -o."
#define MSGTR_ForcingOutputFourcc "Vynucuji výstupní FourCC na %x [%.4s].\n"
#define MSGTR_ForcingOutputAudiofmtTag "Vynucuji znaèku výstupního zvukového formátu 0x%x\n"
#define MSGTR_DuplicateFrames "\n%d opakujících se snímkù!\n"
#define MSGTR_SkipFrame "\nPøeskakuji snímek!\n"
#define MSGTR_ResolutionDoesntMatch "\nNový video soubor má jiné rozli¹ení nebo barevný prostor ne¾ jeho pøedchùdce.\n"
#define MSGTR_FrameCopyFileMismatch "\nV¹echny video soubory musí mít shodné fps, rozli¹ení a kodek pro -ovc copy.\n"
#define MSGTR_AudioCopyFileMismatch "\nV¹echny soubory musí pou¾ívat identický audio kodek a formát pro -oac copy.\n"
#define MSGTR_NoAudioFileMismatch "\nNelze kombinovat neozvuèené video soubory s ozvuèenými. Zkuste -nosound.\n"
#define MSGTR_NoSpeedWithFrameCopy "VAROVÁNÍ: volba -speed nemá zaruèenou správnou funkènost spolu s -oac copy!\n"\
"Výsledný film mù¾e být vadný!\n"
#define MSGTR_ErrorWritingFile "%s: chyba pøi zápisu souboru.\n"
#define MSGTR_RecommendedVideoBitrate "Doporuèený datový tok videa pro CD %s: %d\n"
#define MSGTR_VideoStreamResult "\nVideo proud: %8.3f kbit/s  (%d B/s)  velikost: %"PRIu64" bajtù  %5.3f sekund  %d snímkù\n"
#define MSGTR_AudioStreamResult "\nAudio proud: %8.3f kbit/s  (%d B/s)  velikost: %"PRIu64" bajtù  %5.3f sekund\n"
#define MSGTR_OpenedStream "úspìch: formát: %d  data: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videokodek: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "audiokodek: framecopy (formát=%x kanálù=%d frekvence=%d bitù=%d B/s=%d vzorek-%d)\n"
#define MSGTR_CBRPCMAudioSelected "Vybrán CBR PCM zvuk.\n"
#define MSGTR_MP3AudioSelected "Vybrán MP3 zvuk.\n"
#define MSGTR_CannotAllocateBytes "Nelze alokovat %d bajtù.\n"
#define MSGTR_SettingAudioDelay "Nastavuji zpo¾dìní zvuku na %5.3fs.\n"
#define MSGTR_SettingVideoDelay "Nastavuji zpo¾dìní videa na %5.3fs.\n"
#define MSGTR_SettingAudioInputGain "Nastavuji pøedzesílení zvukového vstupu na %f.\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Omezuji pøednaèítání zvuku na 0.4s.\n"
#define MSGTR_IncreasingAudioDensity "Zvy¹uji hustotu audia na 4.\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Vynucuji pøednaèítání zvuku na 0, max korekci pts  na 0.\n"
#define MSGTR_CBRAudioByterate "\n\nCBR zvuk: %d bajtù/s, %d bajtù/blok\n"
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
"            vysílání MP3 proudu po internetu.\n"\
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
#define MSGTR_LameCantInit \
"Nelze nastavit volby pro LAME, ovìøte datový_tok/vzorkovou_rychlost. Nìkteré"\
"velmi nízké datové toky (<32) vy¾adují ni¾¹í vzorkovou rychlost (napø. -srate 8000).\n"\
"Pokud v¹e sel¾e, zkuste preset."
#define MSGTR_ConfigFileError "chyba konfiguraèního souboru"
#define MSGTR_ErrorParsingCommandLine "chyba pøi zpracovávání pøíkazového øádku"
#define MSGTR_VideoStreamRequired "Videoproud je povinný!\n"
#define MSGTR_ForcingInputFPS "Vstupní fps bude interpretováno jako %5.2f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Výstupní formát souboru RAWVIDEO nepodporuje zvuk - vypínám ho.\n"
#define MSGTR_DemuxerDoesntSupportNosound "Tento demuxer zatím nepodporuje -nosound.\n"
#define MSGTR_MemAllocFailed "Alokace pamìti selhala.\n"
#define MSGTR_NoMatchingFilter "Nemohu najít odpovídající filtr/ao formát!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, mo¾ná je vadný pøekladaè C?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, chybí jméno kodeku!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, nemohu najít enkodér pro kodek %s.\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, nemohu alokovat kontext!\n"
#define MSGTR_CouldntOpenCodec "Nelze otevøít kodek %s, br=%d.\n"
#define MSGTR_CantCopyAudioFormat "Audio formát 0x%x je nekompatibilní s '-oac copy', zkuste prosím '-oac pcm',\n nebo pou¾ijte '-fafmttag' pro jeho pøepsání.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metoda promìnného datového toku\n"\
"                0: cbr  (konstantní tok)\n"\
"                1: mt   (VBR algoritmus Mark Taylor)\n"\
"                2: rh   (VBR algoritmus Robert Hegemann - výchozí)\n"\
"                3: abr  (prùmìrný tok)\n"\
"                4: mtrh (VBR alogoritmus Mark Taylor Robert Hegemann)\n"\
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
" preset=<hodnota> Pøednastavené profily poskytující maximální kvalitu.\n"\
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
#define MSGTR_TooManyFourccs "pøíli¹ mnoho FourCC/formátù..."
#define MSGTR_ParseError "chyba interpretace (parse)"
#define MSGTR_ParseErrorFIDNotNumber "chyba interpretace (ID formátu, nikoli èíslo?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "chyba interpretace (alias ID formátu, nikoli èíslo?)"
#define MSGTR_DuplicateFID "zdvojené ID formátu"
#define MSGTR_TooManyOut "pøíli¹ mnoho výstupu..."
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
#define MSGTR_CantReallocCodecsp "Nelze realokovat '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "Jméno kodeku '%s' není jedineèné."
#define MSGTR_CantStrdupName "Nelze provést strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Nelze provést strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Nelze provést strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Nelze provést strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d audio & %d video kodekù\n"
#define MSGTR_CodecDefinitionIncorrect "Kodek není správnì definován."
#define MSGTR_OutdatedCodecsConf "Tento codecs.conf je pøíli¹ starý a nekompatibilní s tímto sestavením  MPlayeru!"

// fifo.c
#define MSGTR_CannotMakePipe "Nelze vytvoøit ROURU!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Nalezený save slot z lvl %d je pøíli¹ starý: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "Volbu %s nelze pou¾ít v konfiguraèním souboru\n"
#define MSGTR_InvalidCmdlineOption "Volbu %s nelze pou¾ít z pøíkazového øádku\n"
#define MSGTR_InvalidSuboption "Chyba: volba '%s' nemá ¾ádnou podvolbu '%s'\n"
#define MSGTR_MissingSuboptionParameter "Chyba: podvolba '%s' volby '%s' musí mít parametr!\n"
#define MSGTR_MissingOptionParameter "Chyba: volba '%s' musí mít parametr!\n"
#define MSGTR_OptionListHeader "\n Název                Typ             Min        Max      Globál  CL    Konfig\n\n"
#define MSGTR_TotalOptions "\nCelkem: %d voleb\n"
#define MSGTR_ProfileInclusionTooDeep "VAROVÁNÍ: Pøíli¹ hluboké vnoøování profilù.\n"
#define MSGTR_NoProfileDefined "®ádný profil nebyl definován.\n"
#define MSGTR_AvailableProfiles "Dostupné profily:\n"
#define MSGTR_UnknownProfile "Neznámý profil '%s'.\n"
#define MSGTR_Profile "Profil %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n Název                Typ             Min        Max\n\n"
#define MSGTR_TotalProperties "\nCelkem: %d nastavení\n"

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

// stream_dvd.c
#define MSGTR_NoDVDSupport "MPlayer byl zkompilován bez podpory DVD, konèím.\n"
#define MSGTR_DVDnumTitles "Na tomto DVD je %d titul(ù).\n"
#define MSGTR_DVDinvalidTitle "Neplatné èíslo DVD titulu: %d\n"
#define MSGTR_DVDnumChapters "V tomto DVD titulu je %d kapitol.\n"
#define MSGTR_DVDinvalidChapter "Neplatné èíslo DVD kapitoly: %d\n"
#define MSGTR_DVDinvalidChapterRange "Nesprávnì nastavený rozsah kapitol %s\n"
#define MSGTR_DVDinvalidLastChapter "Neplatné èíslo poslední DVD kapitoly: %d\n"
#define MSGTR_DVDnumAngles "Tento DVD titul má %d úhlù pohledu.\n"
#define MSGTR_DVDinvalidAngle "Neplatné èíslo DVD úhlu pohledu: %d\n"
#define MSGTR_DVDnoIFO "Nelze otevøít IFO soubor pro DVD titul %d.\n"
#define MSGTR_DVDnoVMG "Nelze otevøít VMG info!\n"
#define MSGTR_DVDnoVOBs "Nelze otevøít VOBy titulu (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDnoMatchingAudio "DVD zvuk v po¾adovaném jazyce nebyl nalezen!\n"
#define MSGTR_DVDaudioChannel "Vybrán DVD zvukový kanál: %d jazyk: %c%c\n"
#define MSGTR_DVDaudioStreamInfo "audio proud: %d formát: %s (%s) jazyk: %s aid: %d.\n"
#define MSGTR_DVDnumAudioChannels "poèet zvukových kanálù na disku: %d.\n"
#define MSGTR_DVDnoMatchingSubtitle "DVD titulky v po¾adovaném jazyce nebyly nalezeny!\n"
#define MSGTR_DVDsubtitleChannel "Vybrán DVD titulkový kanál: %d jazyk: %c%c\n"
#define MSGTR_DVDsubtitleLanguage "titulky ( sid ): %d jazyk: %s\n"
#define MSGTR_DVDnumSubtitles "poèet sad titulkù na disku: %d\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "Pøíli¹ mnoho datových proudù!"
#define MSGTR_RawMuxerOnlyOneStream "Muxer surového zvuku podporuje pouze jeden zvukový proud!\n"
#define MSGTR_IgnoringVideoStream "Ignoruji video proud!\n"
#define MSGTR_UnknownStreamType "Varování, neznámý typ datového proudu: %d\n"
#define MSGTR_WarningLenIsntDivisible "Varování, délka není násobkem velikosti vzorku!\n"
#define MSGTR_MuxbufMallocErr "Nelze alokovat pamì» pro snímkovou vyrovnávací pamì» muxeru!\n"
#define MSGTR_MuxbufReallocErr "Nelze realokovat pamì» pro snímkovou vyrovnávací pamì» muxeru!\n"
#define MSGTR_MuxbufSending "Snímková vyrovnávací pamì» muxeru posílá %d snímkù do muxeru.\n"
#define MSGTR_WritingHeader "Zapisuji hlavièku...\n"
#define MSGTR_WritingTrailer "Zapisuji index...\n"
 
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
#define MSGTR_MissingVideoStream "Nebyl nalezen video proud.\n"
#define MSGTR_MissingAudioStream "Nebyl nalezen audio proud -> bez zvuku.\n"
#define MSGTR_MissingVideoStreamBug "Chybí video proud!? Kontaktujte autora, mù¾e to být chyba :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Soubor neobsahuje zvolený audio nebo video proud.\n"

#define MSGTR_NI_Forced "Vynucen"
#define MSGTR_NI_Detected "Detekován"
#define MSGTR_NI_Message "%s NEPROKLÁDANÝ formát AVI souboru.\n"

#define MSGTR_UsingNINI "Pou¾ívám NEPROKLÁDANÉ vadné formátování AVI souboru.\n"
#define MSGTR_CouldntDetFNo "Nelze urèit poèet snímkù (pro absolutní posun)\n"
#define MSGTR_CantSeekRawAVI "Nelze se posouvat v surových (raw) AVI proudech! (Potøebuji index, zkuste pou¾ít volbu -idx.)\n"
#define MSGTR_CantSeekFile "Nemohu se posouvat v tomto souboru.\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimované hlavièky vy¾adují ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: VAROVÁNÍ: Promìnná FourCC detekována!?\n"
#define MSGTR_MOVtooManyTrk "MOV: VAROVÁNÍ: pøíli¹ mnoho stop"
#define MSGTR_FoundAudioStream "==> Nalezen audio proud: %d\n"
#define MSGTR_FoundVideoStream "==> Nalezen video proud: %d\n"
#define MSGTR_DetectedTV "Detekována TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Nelze otevøít Ogg demuxer.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Hledám audio proud (id: %d).\n"
#define MSGTR_CannotOpenAudioStream "Nemohu otevøít audio proud: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nemohu otevøít proud s titulky: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Nepovedlo se otevøít audio demuxer: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Nepovedlo se otevøít demuxer pro titulky: %s\n"
#define MSGTR_TVInputNotSeekable "TV vstup neumo¾òuje posun! (\"Posun\" bude pravdìpodobnì pou¾it pro zmìnu kanálù ;)\n"
#define MSGTR_DemuxerInfoChanged "Info demuxeru %s zmìnìno na %s\n"
#define MSGTR_ClipInfo "Informace o klipu:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: detekováno 30000/1001 fps NTSC, pøepínám frekvenci snímkù.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: detekováno 24000/1001 fps progresivní NTSC, pøepínám frekvenci snímkù.\n"

#define MSGTR_CacheFill "\rNaplnìní cache: %5.2f%% (%"PRId64" bajtù)   "
#define MSGTR_NoBindFound "Tlaèítko '%s' nemá pøiøazenu ¾ádnou funkci."
#define MSGTR_FailedToOpen "Selhalo otevøení %s.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nelze otevøít kodek.\n"
#define MSGTR_CantCloseCodec "Nelze uzavøít kodek.\n"

#define MSGTR_MissingDLLcodec "CHYBA: Nelze otevøít po¾adovaný DirectShow kodek %s.\n"
#define MSGTR_ACMiniterror "Nemohu naèíst/inicializovat Win32/ACM audio kodek (chybí DLL soubor?).\n"
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
#define MSGTR_SelectedVideoCodec "Vybrán video kodek: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "Otevírám audio dekodér: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "Vybrán audio kodek: [%s] afm: %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "Vytváøím zvukový øetìzec filtrù pro %dHz/%dch/%s -> %dHz/%dch/%s...\n"
#define MSGTR_UninitVideoStr "Uninit video: %s\n"
#define MSGTR_UninitAudioStr "Uninit audio: %s\n"
#define MSGTR_VDecoderInitFailed "Video dekodér - inicializace selhala :(\n"
#define MSGTR_ADecoderInitFailed "Audio dekodér - inicializace selhala :(\n"
#define MSGTR_ADecoderPreinitFailed "Audio dekodér - pøedinicializace selhala :(\n"
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
#define MSGTR_CodecDidNotSet "VDek: Kodek nenastavil sh->disp_w a sh->disp_h, pokou¹ím se to obejít.\n"
#define MSGTR_VoConfigRequest "VDek: Po¾adovaná konfigurace vo - %d x %d (preferovaný barevný prostor: %s)\n"
#define MSGTR_CouldNotFindColorspace "Nemohu nalézt spoleèný barevný prostor - zkou¹ím to znovu s -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Pomìr stran obrazu filmu je %.2f:1 - ¹káluji na správný pomìr.\n"
#define MSGTR_MovieAspectUndefined "Pomìr stran obrazu filmu není definován - nemìním velikost.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Potøebujete aktualizovat nebo nainstalovat binární kodeky.\nJdìte na http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Inicializace Win32/DShow videokodeku OK.\n"
#define MSGTR_DMOInitOK "INFO: Inicializace Win32/DMO videokodeku OK.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Nemohu poslat událost EWMH fullscreen!\n"
#define MSGTR_CouldNotFindXScreenSaver "xscreensaver_disable: Nelze nalézt okno XScreenSaveru.\n"
#define MSGTR_SelectedVideoMode "XF86VM: Vybrán videore¾im %dx%d pro obraz velikosti %dx%d.\n"

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
// Poznámka: Pokud zmìníte MSGTR_PlayList, ujistìte se prosím, ¾e vyhovuje i pro  MSGTR_MENU_PlayList
#define MSGTR_PlayList "Playlist"
#define MSGTR_Equalizer "Ekvalizér"
#define MSGTR_ConfigureEqualizer "Konfigurace ekvalizéru"
#define MSGTR_SkinBrowser "Prohlí¾eè témat"
#define MSGTR_Network "Sí»ové vysílání..."
// Poznámka: Pokud zmìníte MSGTR_Preferences, ujistìte se prosím, ¾e vyhovuje i pro  MSGTR_MENU_Preferences
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
#define MSGTR_UNKNOWNWINDOWTYPE "Nalezen neznámý typ okna ..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[témata] chyba v konfiguraèním souboru témat na øádce %d: %s"
#define MSGTR_SKIN_WARNING1 "[témata] varování v konfiguraèním souboru témat na øádce %d:\nwidget nalezen ale pøed ním nebyla nalezena ¾ádná \"section\" (%s)"
#define MSGTR_SKIN_WARNING2 "[témata] varování v konfiguraèním souboru témat na øádce %d:\nwidget nalezen ale pøed ním nebyla nalezena ¾ádná \"subsection\" (%s)"
#define MSGTR_SKIN_WARNING3 "[témata] varování v konfiguraèním souboru témat na øádce %d:\nwidget (%s) nepodporuje tuto subsekci"
#define MSGTR_SKIN_SkinFileNotFound "[témata] soubor ( %s ) nenalezen.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[témata] soubor ( %s ) nelze pøeèíst.\n"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmapy s hloubkou 16 bitù a ménì nejsou podporovány (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Soubor nenalezen (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "chyba ètení BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "chyba ètení TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "chyba ètení PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "formát TGA zapouzdøený v RLE není podporován (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "neznámý typ souboru (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "chyba konverze z 24 do 32 bitù (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "neznámá zpráva: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nedostatek pamìti\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "deklarováno pøíli¹ mnoho fontù\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "soubor fontu nebyl nalezen\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "soubor obrazu fontu nebyl nalezen\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "neexistující identifikátor fontu (%s)\n"
#define MSGTR_SKIN_UnknownParameter "neznámý parametr (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Téma nenalezeno (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "Vybraný skin ( %s ) nenalezen, zkou¹ím 'výchozí'...\n"
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
#define MSGTR_MENU_HalfSize   "Polovièní velikost"
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
#define MSGTR_MENU_PlayList MSGTR_PlayList
#define MSGTR_MENU_SkinBrowser "Prohlí¾eè témat"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "Konec..."
#define MSGTR_MENU_Mute "Ztlumit"
#define MSGTR_MENU_Original "Pùvodní"
#define MSGTR_MENU_AspectRatio "Pomìr stran"
#define MSGTR_MENU_AudioTrack "Audio stopa"
#define MSGTR_MENU_Track "Stopa %d"
#define MSGTR_MENU_VideoTrack "Video stopa"
#define MSGTR_MENU_Subtitles "Titulky"

// --- equalizer
// Poznámka: Pokud zmìníte MSGTR_EQU_Audio, ujistìte se prosím, ¾e vyhovuje i pro MSGTR_PREFERENCES_Audio
#define MSGTR_EQU_Audio "Zvuk"
// Poznámka: Pokud zmìníte MSGTR_EQU_Video, ujistìte se prosím, ¾e vyhovuje i pro MSGTR_PREFERENCES_Video
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
#define MSGTR_PREFERENCES_Audio MSGTR_EQU_Audio
#define MSGTR_PREFERENCES_Video MSGTR_EQU_Video
#define MSGTR_PREFERENCES_SubtitleOSD "Titulky & OSD"
#define MSGTR_PREFERENCES_Codecs "Kodeky & demuxer"
// Poznámka: Pokud zmìníte MSGTR_PREFERENCES_Misc, ujistìte se prosím, ¾e vyhovuje i pro MSGTR_PREFERENCES_FRAME_Misc
#define MSGTR_PREFERENCES_Misc "Ostatní"

#define MSGTR_PREFERENCES_None "Nic"
#define MSGTR_PREFERENCES_DriverDefault "výchozí nastavení"
#define MSGTR_PREFERENCES_AvailableDrivers "Dostupné ovladaèe:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nepøehrávat zvuk"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizovat zvuk"
#define MSGTR_PREFERENCES_EnableEqualizer "Aktivovat ekvalizér"
#define MSGTR_PREFERENCES_SoftwareMixer "Aktivovat softwarový smì¹ovaè"
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
#define MSGTR_PREFERENCES_FRAME_Misc MSGTR_PREFERENCES_Misc
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
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, gael¹tina, maltéz¹tina, tureètina (ISO-8859-3)"
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
#define MSGTR_PREFERENCES_ArtsBroken "Novìj¹í verze aRts jsou nekompatibilní "\
           "s GTK 1.x a zhavarují GMPlayer!"

#define MSGTR_ABOUT_UHU "Vývoj GUI je sponzorován firmou UHU Linux\n"
#define MSGTR_ABOUT_Contributors "Pøispìvatelé kódu a dokumentace\n"
#define MSGTR_ABOUT_Codecs_libs_contributions "Kodeky a knihovny tøetích stran\n"
#define MSGTR_ABOUT_Translations "Pøeklady\n"
#define MSGTR_ABOUT_Skins "Skiny\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Kritická chyba!"
#define MSGTR_MSGBOX_LABEL_Error "Chyba!"
#define MSGTR_MSGBOX_LABEL_Warning "Varování!"

// bitmap.c

#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] nedostatek pamìti pro obrázek\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] nedostatek pamìti pro obrázek\n"

// cfg.c

#define MSGTR_ConfigFileReadError "[cfg] chyba pøi ètení konfiguraèního souboru...\n"
#define MSGTR_UnableToSaveOption "[cfg] Nelze ulo¾it volbu '%s'.\n"

// interface.c

#define MSGTR_DeletingSubtitles "[GUI] Ma¾u titulky.\n"
#define MSGTR_LoadingSubtitles "[GUI] Naèítám titulky: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] Pøidávám video filtr: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] Odstraòuji video filtr: %s\n"

// mw.c

#define MSGTR_NotAFile "Toto nevypadá jako soubor: %s !\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] Nelze otevøít display.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Vzdálený display, vypínám XMITSHM.\n"
#define MSGTR_WS_NoXshm "[ws] Promiòte, ale vá¹ systém nepodporuje roz¹íøení X shared memory.\n"
#define MSGTR_WS_NoXshape "[ws] Promiòte, ale vá¹ systém nepodporuje roz¹íøení XShape.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Promiòte, ale barevná hloubka je pøíli¹ malá.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] Pøíli¹ mnoho otevøených oken.\n"
#define MSGTR_WS_ShmError "[ws] chyba roz¹íøení shared memory\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Promiòte, nedostatek pamìti pro vykreslení bufferu.\n"
#define MSGTR_WS_DpmsUnavailable "DPMS není k dispozici?\n"
#define MSGTR_WS_DpmsNotEnabled "Nelze zapnout DPMS.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "Toto nevypadá jako soubor...\n"
#define MSGTR_WS_DDNothing "D&D: Nic se nevrátilo!\n"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Vybrané video_out zaøízení je nekompatibilní s tímto kodekem.\n"\
                "Zkuste pøidat filtr scale, èili -vf spp,scale namísto -vf spp.\n"
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
#define MSGTR_VO_ValueOutOfRange "hodnota mimo rozsah"
#define MSGTR_VO_NoValueSpecified "Nebyla zadána hodnota."
#define MSGTR_VO_UnknownSuboptions "neznámá(é) podvolba(y)"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\nZde jsou podvolby aalib vo_aa:\n"
#define MSGTR_VO_AA_AdditionalOptions "Dodateèné volby vo_aa zaji¹»ují:\n" \
"  help        vypí¹e tuto nápovìdu\n" \
"  osdcolor    nastaví barvu OSD\n  subcolor    nastaví barvu titulkù\n" \
"        parametry barev jsou:\n           0 : normal\n" \
"           1 : dim\n           2 : bold\n           3 : boldfont\n" \
"           4 : reverse\n           5 : special\n\n\n"

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
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Nelze získat pamì» nebo ukazatel souboru pro zápis \"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Chyba pøi zápisu obrázku na výstup!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Neznámé podzaøízení: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Pou¾ívám prokládaný výstupní re¾im, horní pole napøed."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Pou¾ívám prokládaný výstupní re¾im, dolní pole napøed."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Pou¾ívám (výchozí) neprokládaný snímkový re¾im."

// sub.c
#define MSGTR_VO_SUB_Seekbar "Postup"
#define MSGTR_VO_SUB_Play "Play"
#define MSGTR_VO_SUB_Pause "Pauza"
#define MSGTR_VO_SUB_Stop "Stop"
#define MSGTR_VO_SUB_Rewind "Zpìt"
#define MSGTR_VO_SUB_Forward "Vpøed"
#define MSGTR_VO_SUB_Clock "Hodiny"
#define MSGTR_VO_SUB_Contrast "Kontrast"
#define MSGTR_VO_SUB_Saturation "Sytost"
#define MSGTR_VO_SUB_Volume "Hlasitost"
#define MSGTR_VO_SUB_Brightness "Jas"
#define MSGTR_VO_SUB_Hue "Barevný tón"

// vo_xv.c
#define MSGTR_VO_XV_ImagedimTooHigh "Rozmìry zdrojového obrazu jsou pøíli¹ velké: %ux%u (maximum je %ux%u)\n"

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
#define MSGTR_AO_OSS_CantSet "[AO OSS] Zvukové zaøízení %s nelze nastavit na výstup %s, zkou¹ím %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Selhalo nastavení výstupního zvukového zaøízení na %d kanálù.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: Ovladaè nepodporuje SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Ovladaè Va¹í zvukové karty NEPODPORUJE select()  ***\n Pøekompilujte MPlayer s #undef HAVE_AUDIO_SELECT v config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nKritická chyba: *** NELZE ZNOVUOTEVØÍT / RESTARTOVAT ZVUKOVÉ ZAØÍZENÍ *** %s\n"
#define MSGTR_AO_OSS_UnknownUnsupportedFormat "[AO OSS] Neznámý/nepodporovaný OSS formát: %x.\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Pøipojen ke zvukovému serveru.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Nelze otevøít datový proud.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Datový proud otevøen.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] velikost vyrovnávací pamìti: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Nastavení hlasitosti na %d selhalo.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] %d Hz není podporováno, zkuste pøevzorkovat.\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound selhalo: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] latence: [server: %0.2fs, sí»: %0.2fs] (upravuji %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] selhalo otevøení datového proudu ESD pro pøehrávání: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] selhalo nastavení DVB zvukového mixeru: %s.\n" 
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz není podporováno, zkuste pøevzorkovat.\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Soubor: %s (%s)\nPCM: Vzorkování: %iHz Kanál(y): %s Formát %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info:  Nejrychlej¹í extrakce dosáhnete s -vc null -vo null -ao pcm:fast\n[AO PCM] Info: Pro zápis WAVE souborù pou¾ijte -ao pcm:waveheader (výchozí).\n"
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
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: zápis selhal.\n"
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Nelze otevøít zvukové zaøízení %s, %s  -> nebude zvuk.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: Va¹e karta nepodporuje %d kanálové, %s, %d Hz vzorkování.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Ovladaè Va¹í zvukové karty NEPODPORUJE select()  ***\n Pøekompilujte MPlayer s #undef HAVE_AUDIO_SELECT v config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nKritická chyba: *** NELZE ZNOVUOTEVØÍT / RESTARTOVAT ZVUKOVÉ ZAØÍZENÍ (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: po¾adovaný formát: %d Hz, %d kanál(ù), %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: ¾ádná zvuková karta nebyla nalezena.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: po¾adován neplatný formát (%s) - výstup odpojen.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: chyba otevøení pøehrávání zvuku: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: chyba v PCM info: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: nalezeno %d zvukových karet, pou¾ívám: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: chyba info v PCM kanálu: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: chyba pøi nastavování parametrù: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: chyba pøi nastavování kanálu: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: chyba pøi pøípravì kanálu: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: chyba playback drain: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: chyba playback flush: %s\n" //to jsou názvy ¾e by jeden pad
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: chyba uzavøení PCM: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: chyba playback drain: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: chyba playback flush: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: chyba pøi pøípravì kanálù: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: chyba playback drain: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: chyba playback flush: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: chyba pøi pøípravì kanálù: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: podteèení v alsa, restartuji proud.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: chyba pøípravy pøehrávání zvuku: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: chyba pøi zápisu po restartu: %s - vzdávám to.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: chyba výstupu: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] neplatný zásuvný modul: %s\n"

// ======================= AF Audio Filters ================================

// libaf 

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "dostupné názvy v"
#define MSGTR_AF_LADSPA_WarnNoInputs "VAROVÁNÍ! Tento LADSPA plugin nemá audio vstupy.\n  Vstupní audio signál bude ztracen."
#define MSGTR_AF_LADSPA_ErrMultiChannel "Vícekanálové (>2) pluginy nejsou podporovány (zatím).\n  Pou¾ívejte pouze mono a stereo pluginy."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Tento LADSPA plugin nemá audio výstupy."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Poèet audio vstupù LADSPA pluginu je odli¹ný od poètu audio výstupù."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "selhalo naètení"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Nelze nalézt funkci ladspa_descriptor() v uvedené knihovnì."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Nelze nalézt po¾adovaný název v knihovnì pluginù."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Nebyla zadány ¾ádné podvolby."
#define MSGTR_AF_LADSPA_ErrNoLibFile "Nebyla zadána ¾ádná knihovna."
#define MSGTR_AF_LADSPA_ErrNoLabel "Nebyl zadán název ¾ádného filtru."
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Na pøíkazovém øádku bylo uvedeno málo voleb."
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: Vstupní voliè #%d je ni¾¹í ne¾ minimální hodnota %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: Vstupní voliè #%d je vy¹¹í ne¾ maximální hodnota %0.4f.\n"

// format.c

#define MSGTR_AF_FORMAT_UnknownFormat "neznámý formát "

// ========================== INPUT =========================================

// joystick.c

#define MSGTR_INPUT_JOYSTICK_Opening "Otevírám zaøízení joysticku %s\n"
#define MSGTR_INPUT_JOYSTICK_CantOpen "Nelze otevøít zaøízení joysticku %s: %s\n"
#define MSGTR_INPUT_JOYSTICK_ErrReading "Chyba pøi ètení zaøízení joysticku: %s\n"
#define MSGTR_INPUT_JOYSTICK_LoosingBytes "Joystick: Uvolnili jsme %d bajtù dat\n"
#define MSGTR_INPUT_JOYSTICK_WarnLostSync "Joystick: warning init event, ztratili jsme synchronizaci s ovladaèem.\n"
#define MSGTR_INPUT_JOYSTICK_WarnUnknownEvent "Joystick: varování, neznámý typ události %d\n"

// input.c

#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "Pøíli¹ mnoho souborových deskriptorù pøíkazù, nelze registrovat\n deskriptor souboru %d.\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "Pøíli¹ mnoho souborových deskriptorù klávesnice, nelze registrovat\n deskriptor souboru %d.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "Pøíkaz %s: argument %d není typu integer.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "Pøíkaz %s: argument %d není typu float.\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "Pøíkaz %s: argument %d není ukonèen.\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "Neznámý argument %d\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "Pøíkaz %s vy¾aduje aspoò %d argumentù, nalezli jsme jich v¹ak pouze %d.\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "Chyba pøi ètení pøíkazového deskriptoru souboru %d: %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "Vyrovnávací pamì» deskriptoru souboru pøíkazù %d je plná: zahazuji obsah.\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "©patný pøíkaz pro pøiøazení klávese %s"
#define MSGTR_INPUT_INPUT_ErrSelect "Chyba výbìru: %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "Chyba v deskriptoru souboru klávesového vstupu %d\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "Vstup mrtvé klávesy z deskriptoru souboru %d\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "Pøíli¹ mnoho souèasnì stisknutých kláves\n"
#define MSGTR_INPUT_INPUT_ErrOnCmdFd "Chyba na cmd fd %d\n"
#define MSGTR_INPUT_INPUT_ErrReadingInputConfig "Chyba pøi ètení input konfiguraèního souboru %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrUnknownKey "Neznámá klávesa '%s'\n"
#define MSGTR_INPUT_INPUT_ErrUnfinishedBinding "Nedokonèené pøiøazení %s\n"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForKeyName "Pøíli¹ malá vyrovnávací pamì» pro tento název klávesy: %s\n"
#define MSGTR_INPUT_INPUT_ErrNoCmdForKey "Nenalezen pøíkaz pro tlaèítko %s"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForCmd "Pøíli¹ malá vyrovnávací pamì» pro pøíkaz %s\n"
#define MSGTR_INPUT_INPUT_ErrWhyHere "Co tady dìláme?\n"
#define MSGTR_INPUT_INPUT_ErrCantInitJoystick "Nelze inicializovat vstupní joystick\n"
#define MSGTR_INPUT_INPUT_ErrCantStatFile "Nelze stat %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantOpenFile "Nelze otevøít %s: %s\n"

// ========================== LIBMPDEMUX ===================================

// url.c

#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "Zdá se, ¾e je ji¾ øetìzec eskejpován v url_escape %c%c1%c2\n"

// ai_alsa1x.c

#define MSGTR_MPDEMUX_AIALSA1X_CannotSetSamplerate "Nelze nastavit vzorkovací kmitoèet.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetBufferTime "Nelze nastavit èas vyrovnávací pamìti.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetPeriodTime "Nelze nastavit èas opakování.\n"

// ai_alsa1x.c / ai_alsa.c

#define MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig "Vadná konfigurace pro toto PCM: ¾ádné konfigurace nejsou k dispozici.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableAccessType "Typ pøístupu není k dispozici.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt "Formát vzorku není k dispozici.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableChanCount "Poèet kanálù není k dispozici - vracím výchozí: %d\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams "Nelze nainstalovat hardwarové parametry: %s"
#define MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize "Nelze pou¾ít opakování odpovídající velikosti vyrovnávací pamìti (%u == %lu)\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallSWParams "Nelze nainstalovat softwarové parametry:\n"
#define MSGTR_MPDEMUX_AIALSA_ErrorOpeningAudio "Chyba pøi otevírání zvuku: %s\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatusError "ALSA status error: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUN "ALSA xrun!!! (minimálnì %.3f ms dlouhý)\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatus "ALSA Status:\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUNPrepareError "ALSA xrun: prepare error: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaReadWriteError "ALSA chyba ètení/zápisu"

// ai_oss.c

#define MSGTR_MPDEMUX_AIOSS_Unable2SetChanCount "Nelze nastavit poèet kanálù: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetStereo "Nelze nastavit stereo: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2Open "Nelze otevøít '%s': %s\n"
#define MSGTR_MPDEMUX_AIOSS_UnsupportedFmt "nepodporovaný formát\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt "Nelze nastavit audio formát."
#define MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate "Nelze nastavit vzorkovací kmitoèet: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger "Nelze nastavit spou¹»: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize "Nelze zjistit velikost bloku!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero "Velikost zvukového bloku je nulová, nastavuji ji na %d!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low "Velikost zvukového bloku je pøíli¹ malá, nastavuji ji na %d!\n"

// asfheader.c

#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB "FATAL: velikost hlavièky je vìt¹í ne¾ 1 MB (%d)!\nKontaktujte prosím tvùrce MPlayeru a nahrajte/po¹lete jim tento soubor.\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "Nemohu alokovat %d bajtù pro hlavièku.\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "konec souboru pøi ètení ASF hlavièky, po¹kozený/neúplný soubor?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "DVR bude pravdìpodobnì pracovat pouze s libavformat, v pøípadì problémù zkuste -demuxer 35\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "Po hlavièce nenásleduje ¾ádný datový chunk!\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF: ani audio ani video hlavièky nebyly nalezeny - vadný soubor?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "Nesprávná délka v hlavièce ASF!\n"

// asf_mmst_streaming.c

#define MSGTR_MPDEMUX_MMST_WriteError "chyba zápisu\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\nVýstraha! EOF\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "ètení pre-hlavièky selhalo\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "©patná velikost hlavièky, vzdávám to.\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "Ètení dat hlavièky selhalo.\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "Selhalo ètení packet_len.\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "©patná velikost RTSP paketu, vzdávám to.\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "Selhalo ètení pøíkazových dat.\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "hlavièkový objekt\n"
#define MSGTR_MPDEMUX_MMST_DataObject "datový objekt\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "souborový objekt, délka paketu = %d (%d)\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "proudový objekt, ID datového proudu: %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "Pøíli¹ mnoho ID, proud pøeskoèen."
#define MSGTR_MPDEMUX_MMST_UnknownObject "neznámý objekt\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "Ètení media dat selhalo.\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "chybí signatura\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "V¹e hotovo. Dìkujeme, ¾e jste si stáhli mediální soubor obsahující proprietární a patentovanou technologii.\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "neznámý pøíkaz %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "chyba get_media_packet: %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "Pøipojeno\n"

// asf_streaming.c

#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "Ahhhh, velikost stream_chunck je pøíli¹ malá: %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "size_confirm nesouhlasí!: %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "Varování: zahozena hlavièka ????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "Chyba pøi parsování hlavièky chunku\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "Hlavièka nedo¹la jako první chunk !!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "Chyba: nelze alokovat %d bajtù vyrovnávací pamìti.\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "Chyba pøi ètení proudu ze sítì.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "Chyba: chunk je pøíli¹ malý.\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "Chyba: poèet sub chunkù je nesprávný.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "Pøíli¹ malá pøenosová rychlost, soubor nelze pøehrávat!\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "Pøíli¹ malá pøenosová rychlost, odvolaný audio proud.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "Pøíli¹ malá pøenosová rychlost, odvolaný video proud.\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "Nesprávná délka v ASF hlavièce!\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "Chyba pøi ètení hlavièky chunku.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "Chyba: chunk_size > packet_size\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "Chyba pøi ètení chunku.\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> ASF Redirector\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "neplatná proxy URL\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "neznámý typ ASF proudu\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "Selhalo parsování HTTP odpovìdi.\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "Server vrátil %d:%s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "ASF HTTP PARSE VAROVÁNÍ: Pragma %s zkrácena z %d bajtù na %d\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "Chyba zápisu soketu: %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "Selhalo parsování hlavièky\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "Nenalezen datový proud\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "Neznámý typ ASF proudu\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL: %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "Selhalo, konèím.\n"

// audio_in.c

#define MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio "\nChyba pøi ètení audia: %s\n"
#define MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut "Zotaveno z cross-run, nìkteré snímky mohly být vynechány!\n"
#define MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover "Kritická chyba, nelze zotavit!\n"
#define MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples "\nNedostatek audio vzorkù!\n"

// aviheader.c

#define MSGTR_MPDEMUX_AVIHDR_EmptyList "** prázdný seznam?!\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundMovieAt "Nalezen film na 0x%X - 0x%X\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader "Nalezena 'bih', %u bajtù z %d\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1 "Regeneruji tabulku klíèových snímkù pro MS mpg4v1 video.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3 "Regeneruji tabulku klíèových snímkù pro DIVX3 video.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4 "Regeneruji tabulku klíèových snímkù pro MPEG4 video.\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt "Nalezen 'wf', %d bajtù z %d\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header "AVI: nalezena dmlh (size=%d) (total_frames=%d)\n"
#define MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames "Ètu INDEX blok, %d chunkù pro %d snímkù (fpos=%"PRId64").\n"
#define MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr "Dodateèná RIFF hlavièka...\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** Varování: toto není roz¹íøená AVI hlavièka..\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenChunk "Vadný chunk?  chunksize=%d  (id=%.4s)\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI: ODML: Vytváøím ODML index (%d superindexchunkù).\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI: ODML: Detekován vadný (neúplný?) soubor. Pou¾ije se tradièní index.\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "Nelze èíst indexový soubor %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "%s není planý indexový soubor pro MPlayer.\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "Nemohu alokovat pamì» pro data indexu od %s.\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "pøedèasný konec indexového souboru %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "Nahrán indexový soubor: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "Generuji index: %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI: Vygenerována tabulka indexu pro %d chunkù!\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "Nelze zapsat indexový soubor %s: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "Ulo¾en indexový soubor: %s\n"

// cache2.c

#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\rTento proud nelze ukládat do vyrovnávací pamìti.\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!! read_filepos se li¹í !!! Ohlaste tuto chybu...\n"

// cdda.c

#define MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice "Nelze otevøít CDDA zaøízení.\n"
#define MSGTR_MPDEMUX_CDDA_CantOpenDisc "Nelze otevøít disk.\n"
#define MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks "Nalezeno audio CD s %ld stopami\n"

// cddb.c

#define MSGTR_MPDEMUX_CDDB_FailedToReadTOC "Selhalo ètení TOC.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToOpenDevice "Selhalo otevøení zaøízení %s.\n"
#define MSGTR_MPDEMUX_CDDB_NotAValidURL "neplatná URL\n"
#define MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest "Selhalo odeslání HTTP po¾adavku.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse "Selhalo ètení HTTP odpovìdi.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND "Není k dispozici.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown "neznámý error kód\n"
#define MSGTR_MPDEMUX_CDDB_NoCacheFound "Vyrovnávací pamì» nenalezena.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead "Nebyl pøeèten celý xmcd soubor.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory "Selhalo vytvoøení adresáøe %s.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten "Nebyl zapsán celý xmcd soubor.\n"
#define MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned "Vrácen chybný soubor xmcd databáze.\n"
#define MSGTR_MPDEMUX_CDDB_UnexpectedFIXME "neoèekávané UROB-SI-SÁM\n"
#define MSGTR_MPDEMUX_CDDB_UnhandledCode "neo¹etøený kód\n"
#define MSGTR_MPDEMUX_CDDB_UnableToFindEOL "Nelze nalést konec øádku.\n"
#define MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle "Parsování OK, nalezeno: %s\n"
#define MSGTR_MPDEMUX_CDDB_AlbumNotFound "Album nenalezeno.\n"
#define MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr "Server vrátil: Syntaktická chyba pøíkazu\n"
#define MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable "Nejsou informace o sitech (serverech).\n"
#define MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel "Selhalo získání úrovnì protokolu.\n"
#define MSGTR_MPDEMUX_CDDB_NoCDInDrive "V mechanice není CD.\n"

// cue_read.c

#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] Neoèekávaný øádek v cue souboru: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] otestován bin soubor: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] Nelze nalézt bin soubor - vzdávám to.\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] Pou¾ívám bin soubor %s.\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] neznámý re¾im pro bin soubor. To by se nemìlo stát. Konèím.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] Nelze otevøít %s\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] Chyba ètení z  %s\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] Chyba získání velikosti bin souboru.\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "stopa %02d:  format=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] neoèekávaný konec bin souboru\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] Nelze pøeèíst %d bajtù 'payloadu'.\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "CUE stream_open, soubor=%s, stopa=%d, dostupné stopy: %d -> %d\n"

// network.c

#define MSGTR_MPDEMUX_NW_UnknownAF "Neznámá rodina adres %d\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "Resolvuji %s pro %s...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "Nelze resolvovat jméno pro %s: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "Pøipojuji se k serveru %s[%s]: %d...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "Selhalo pøipojení k serveru pomocí %s\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "Select selhal.\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "spojení vypr¹elo\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "getsockopt selhal: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "chyba spojení: %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "©patné nastavení proxy... Zkou¹ím bez proxy.\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "Nelze resolvovat jméno vzdáleného systému pro AF_INET. Zkou¹ím bez proxy.\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "Chyba pøi odesílání HTTP po¾adavku: Nebyl odeslán celý po¾adavek.\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "Chyba pøi ètení.\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "http_read_response pøeèetlo 0 (to je EOF).\n"
#define MSGTR_MPDEMUX_NW_AuthFailed "Autentifikace selhala. Pou¾ijte volby -user a -passwd pro zadání svého\n"\
"u¾ivatelského_jména/hesla pro seznam URL, nebo URL v následující formì:\n"\
"http://u¾ivatelské_jméno:heslo@jméno_serveru/soubor\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "Pro %s je vy¾adována autentifikace\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "Vy¾adována autentifikace.\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "Nezadáno heslo, zkou¹ím prázdné heslo.\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "Server vrátil %d: %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "Vyrovnávací pamì» nastavena na %d KBajtù\n"

// demux_audio.c

#define MSGTR_MPDEMUX_AUDIO_UnknownFormat "Audio demuxer: neznámý formát %d.\n"

// demux_demuxers.c

#define MSGTR_MPDEMUX_DEMUXERS_FillBufferError "fill_buffer chyba: ¹patný demuxer: ani vd, ad nebo sd.\n"

// demux_nuv.c

#define MSGTR_MPDEMUX_NUV_NoVideoBlocksInFile "V souboru nejsou ¾ádné bloky videa.\n"

// demux_xmms.c

#define MSGTR_MPDEMUX_XMMS_FoundPlugin "Nalezen plugin: %s (%s).\n"
#define MSGTR_MPDEMUX_XMMS_ClosingPlugin "Uzavírám plugin: %s.\n"

// ========================== LIBMPMENU ===================================

// common

#define MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition "[MENU] V definici menu není ¾ádná polo¾ka.\n"

// libmenu/menu.c
#define MSGTR_LIBMENU_SyntaxErrorAtLine "[MENU] syntaktická chyba na øádku: %d\n"
#define MSGTR_LIBMENU_MenuDefinitionsNeedANameAttrib "[MENU] V definici menu je potøeba jmenný atribut (øádek %d)\n"
#define MSGTR_LIBMENU_BadAttrib "[MENU] ¹patný atribut %s=%s v menu '%s' na øádku %d\n"
#define MSGTR_LIBMENU_UnknownMenuType "[MENU] neznámý typ menu '%s' na øádce %d\n"
#define MSGTR_LIBMENU_CantOpenConfigFile "[MENU] Nemohu otevøít konfiguraèní soubor menu: %s\n"
#define MSGTR_LIBMENU_ConfigFileIsTooBig "[MENU] Konfiguraèní soubor je pøíli¹ velký. (> %d KB)\n"
#define MSGTR_LIBMENU_ConfigFileIsEmpty "[MENU] Konfiguraèní soubor je prázdný.\n"
#define MSGTR_LIBMENU_MenuNotFound "[MENU] Menu %s nebylo nalezeno.\n"
#define MSGTR_LIBMENU_MenuInitFailed "[MENU] Menu '%s': Selhala inicializace.\n"
#define MSGTR_LIBMENU_UnsupportedOutformat "[MENU] Nepodporovaný výstupní formát!\n"

// libmenu/menu_cmdlist.c
#define MSGTR_LIBMENU_ListMenuEntryDefinitionsNeedAName "[MENU] Polo¾ky typu seznam vy¾adují název (øádek %d).\n"
#define MSGTR_LIBMENU_ListMenuNeedsAnArgument "[MENU] Polo¾ka typu seznam vy¾aduje argument.\n"

// libmenu/menu_console.c
#define MSGTR_LIBMENU_WaitPidError "[MENU] Chyba pøi èekání na PID: %s.\n"
#define MSGTR_LIBMENU_SelectError "[MENU] Chyba výbìru.\n"
#define MSGTR_LIBMENU_ReadErrorOnChilds "[MENU] Chyba ètení na popisovaèi souboru potomka: %s.\n"
#define MSGTR_LIBMENU_ConsoleRun "[MENU] Spu¹tìní v konsoli: %s ...\n"
#define MSGTR_LIBMENU_AChildIsAlreadyRunning "[MENU] Potomek u¾ bì¾í.\n"
#define MSGTR_LIBMENU_ForkFailed "[MENU] Forkování selhalo!!!\n"
#define MSGTR_LIBMENU_WriteError "[MENU] chyba pøi zápisu.\n"

// libmenu/menu_filesel.c
#define MSGTR_LIBMENU_OpendirError "[MENU] chyba pøi otevírání adresáøe: %s\n"
#define MSGTR_LIBMENU_ReallocError "[MENU] chyba pøi relokaci: %s\n"
#define MSGTR_LIBMENU_MallocError "[MENU] chyba pøi alokaci pamìti: %s\n"
#define MSGTR_LIBMENU_ReaddirError "[MENU] chyba ètení adresáøe: %s\n"
#define MSGTR_LIBMENU_CantOpenDirectory "[MENU] Nelze otevøít adresáø %s.\n"

// libmenu/menu_param.c
#define MSGTR_LIBMENU_SubmenuDefinitionNeedAMenuAttribut "[MENU] Pøi definici podmenu je potøeba uvést atribut 'menu'.\n"
#define MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed "[MENU] Preferenèní polo¾ka menu vy¾aduje korektní atribut 'property' (øádka %d).\n"
#define MSGTR_LIBMENU_PrefMenuNeedsAnArgument "[MENU] Preferenèní menu vy¾aduje argument.\n"

// libmenu/menu_pt.c
#define MSGTR_LIBMENU_CantfindTheTargetItem "[MENU] Nemohu nalézt cílovou polo¾ku??\n"
#define MSGTR_LIBMENU_FailedToBuildCommand "[MENU] Selhalo sestavení pøíkazu: %s.\n"

// libmenu/menu_txt.c
#define MSGTR_LIBMENU_MenuTxtNeedATxtFileName "[MENU] Textové menu vy¾aduje název souboru txt (parametrický soubor).\n"
#define MSGTR_LIBMENU_MenuTxtCantOpen "[MENU] Nelze otevøít: %s.\n"
#define MSGTR_LIBMENU_WarningTooLongLineSplitting "[MENU] Pozor, øádka je pøíli¹ dlouhá. Rozdìluju ji.\n"
#define MSGTR_LIBMENU_ParsedLines "[MENU] Zpracováno %d øádkù.\n"

// libmenu/vf_menu.c
#define MSGTR_LIBMENU_UnknownMenuCommand "[MENU] Neznámý pøíkaz: '%s'.\n"
#define MSGTR_LIBMENU_FailedToOpenMenu "[MENU] Nemohu otevøít menu: '%s'.\n"

// ========================== LIBMPCODECS ===================================

// libmpcodecs/ad_libdv.c
#define MSGTR_MPCODECS_AudioFramesizeDiffers "[AD_LIBDV] Varování! Velikost rámce zvuku se li¹í! pøeèteno=%d  hlavièka=%d.\n"

// libmpcodecs/vd_dmo.c vd_dshow.c vd_vfw.c
#define MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec "[VD_DMO] Nemohu alokovat obraz pro kodek cinepak.\n"

// libmpcodecs/vd_ffmpeg.c
#define MSGTR_MPCODECS_XVMCAcceleratedCodec "[VD_FFMPEG] XVMC akcelerovaný kodek.\n"
#define MSGTR_MPCODECS_ArithmeticMeanOfQP "[VD_FFMPEG] Aritmetický prùmìr QP: %2.4f, harmonický prùmìr QP: %2.4f\n"
#define MSGTR_MPCODECS_DRIFailure "[VD_FFMPEG] DRI selhalo.\n"
#define MSGTR_MPCODECS_CouldntAllocateImageForCodec "[VD_FFMPEG] Nemohu alokovat obraz pro kodek.\n"
#define MSGTR_MPCODECS_XVMCAcceleratedMPEG2 "[VD_FFMPEG] XVMC-akcelerovaný MPEG-2.\n"
#define MSGTR_MPCODECS_TryingPixfmt "[VD_FFMPEG] Zkou¹ím pixfmt=%d.\n"
#define MSGTR_MPCODECS_McGetBufferShouldWorkOnlyWithXVMC "[VD_FFMPEG] mc_get_buffer by mìlo fungovat jen s XVMC akcelerací!"
#define MSGTR_MPCODECS_UnexpectedInitVoError "[VD_FFMPEG] Neoèekávaná chyba init_vo.\n"
#define MSGTR_MPCODECS_UnrecoverableErrorRenderBuffersNotTaken "[VD_FFMPEG] Neodstranitelná chyba, vykreslovací buffery nepou¾ity.\n"
#define MSGTR_MPCODECS_OnlyBuffersAllocatedByVoXvmcAllowed "[VD_FFMPEG] Povoleny jsou jen buffery alokované pomocí vo_xvmc.\n"

// libmpcodecs/ve_lavc.c
#define MSGTR_MPCODECS_HighQualityEncodingSelected "[VE_LAVC] Vybráno vysoce kvalitní kódování (nebude probíhat v reálném èase)!\n"
#define MSGTR_MPCODECS_UsingConstantQscale "[VE_LAVC] Pou¾ívám konstantní qscale = %f (VBR).\n"

// libmpcodecs/ve_raw.c
#define MSGTR_MPCODECS_OutputWithFourccNotSupported "[VE_RAW] Surový výstup s fourcc [%x] není podporován!\n"
#define MSGTR_MPCODECS_NoVfwCodecSpecified "[VE_RAW] Po¾adovaný VfW kodek nebyl specifikován!\n"

// libmpcodecs/vf_crop.c
#define MSGTR_MPCODECS_CropBadPositionWidthHeight "[CROP] ©patná pozice/¹íøka/vý¹ka - oøezová oblast zasahuje mimo originál!\n"

// libmpcodecs/vf_cropdetect.c
#define MSGTR_MPCODECS_CropArea "[CROP] Oøezová oblast: X: %d..%d  Y: %d..%d  (-vf crop=%d:%d:%d:%d).\n"

// libmpcodecs/vf_format.c, vf_palette.c, vf_noformat.c
#define MSGTR_MPCODECS_UnknownFormatName "[VF_FORMAT] Neznámý název formátu: '%s'.\n"

// libmpcodecs/vf_framestep.c vf_noformat.c vf_palette.c vf_tile.c
#define MSGTR_MPCODECS_ErrorParsingArgument "[VF_FRAMESTEP] Chyba pøi zpracování argumentu.\n"

// libmpcodecs/ve_vfw.c
#define MSGTR_MPCODECS_CompressorType "Typ komprese: %.4lx\n"
#define MSGTR_MPCODECS_CompressorSubtype "Podtyp komprese: %.4lx\n"
#define MSGTR_MPCODECS_CompressorFlags "Pøíznaky kompresoru: %lu, verze %lu, verze ICM: %lu\n"
#define MSGTR_MPCODECS_Flags "Pøíznaky:"
#define MSGTR_MPCODECS_Quality " kvalita"

// libmpcodecs/vf_expand.c
#define MSGTR_MPCODECS_FullDRNotPossible "Plný DR není mo¾ný, zkou¹ím místo nìj SLICES!\n"
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupportSlices  "Varování! Dal¹í filtr nepodporuje SLICES, oèekávejte sig11...\n"
#define MSGTR_MPCODECS_FunWhydowegetNULL "Proè jsme dostali NULL??\n"

// libmpcodecs/vf_fame.c
#define MSGTR_MPCODECS_FatalCantOpenlibFAME "Fatální chyba: Nelze otevøít libFAME!\n"

// libmpcodecs/vf_test.c, vf_yuy2.c, vf_yvu9.c
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupport "%s není dal¹ím filtrem/vo podporován :(\n"

// ================================== LIBMPVO ====================================

// mga_common.c

#define MSGTR_LIBVO_MGA_ErrorInConfigIoctl "[MGA] Chyba v mga_vid_config ioctl (¹patná verze mga_vid.o?)."
#define MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule "[MGA] Nemohu získat hodnoty luma z jaderného modulu!\n"
#define MSGTR_LIBVO_MGA_CouldNotSetLumaValuesFromTheKernelModule "[MGA] Nemohu nastavit hodnoty luma v jaderném modulu!\n"
#define MSGTR_LIBVO_MGA_ScreenWidthHeightUnknown "[MGA] Pomìr stran obrazovky není znám!\n"
#define MSGTR_LIBVO_MGA_InvalidOutputFormat "[MGA] Neplatný formát výstupu %0X\n"
#define MSGTR_LIBVO_MGA_IncompatibleDriverVersion "[MGA] Verze va¹eho mga_vid ovladaèe není kompatibilní s touto verzí MPlayeru!\n"
#define MSGTR_LIBVO_MGA_UsingBuffers "[MGA] Pou¾ívám %d buferù.\n"
#define MSGTR_LIBVO_MGA_CouldntOpen "[MGA] Nemohu otevøít: %s\n"
#define MGSTR_LIBVO_MGA_ResolutionTooHigh "[MGA] Vstupní rozli¹ení minimálnì v jednom rozmìru vìt¹í ne¾ 1023x1023. Pøe¹kálujte prosím softwarovì, nebo pou¾ijte -lavdopts lowres=1\n"
 
// libvo/vesa_lvo.c
 
#define MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported "[VESA_LVO] Tato vìtev není nadále podporována.\n[VESA_LVO] Pou¾ijte prosím -vo vesa:vidix.\n"
#define MSGTR_LIBVO_VESA_CouldntOpen "[VESA_LVO] Nemohu otevøít: '%s'\n"
#define MSGTR_LIBVO_VESA_InvalidOutputFormat "[VESA_LVI] Neplatný výstupní formát: %s(%0X)\n"
#define MSGTR_LIBVO_VESA_IncompatibleDriverVersion "[VESA_LVO] Verze va¹eho fb_vid ovladaèe není koémpatibilní s touto verzí MPlayeru!\n"

// libvo/vo_3dfx.c

#define MSGTR_LIBVO_3DFX_Only16BppSupported "[VO_3DFX] Podporováno je jen 16bpp!"
#define MSGTR_LIBVO_3DFX_VisualIdIs "[VO_3DFX] Visual ID je  %lx.\n"
#define MSGTR_LIBVO_3DFX_UnableToOpenDevice "[VO_3DFX] Nemohu otevøít /dev/3dfx.\n"
#define MSGTR_LIBVO_3DFX_Error "[VO_3DFX] Chyba: %d.\n"
#define MSGTR_LIBVO_3DFX_CouldntMapMemoryArea "[VO_3DFX] Nemohu namapovat oblasti pamìti 3dfx: %p,%p,%d.\n"
#define MSGTR_LIBVO_3DFX_DisplayInitialized "[VO_3DFX] Inicialozováno: %p.\n"
#define MSGTR_LIBVO_3DFX_UnknownSubdevice "[VO_3DFX] Neznámé podzaøízení: %s.\n"

// libvo/vo_dxr3.c

#define MSGTR_LIBVO_DXR3_UnableToLoadNewSPUPalette "[VO_DXR3] Nemohu nahrát novou SPU paletu!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetPlaymode "[VO_DXR3] Nemohu nastavit re¾im pøehrávání!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetSubpictureMode "[VO_DXR3] Nemohu nastavit re¾im titulkù!\n"
#define MSGTR_LIBVO_DXR3_UnableToGetTVNorm "[VO_DXR3] Nemohu zjistit televizní normu!\n"
#define MSGTR_LIBVO_DXR3_AutoSelectedTVNormByFrameRate "[VO_DXR3] Automaticky nastavená televizní norma podle snímkové rychlosti: "
#define MSGTR_LIBVO_DXR3_UnableToSetTVNorm "[VO_DXR3] Nemohu nastavit televizní normu!\n"
#define MSGTR_LIBVO_DXR3_SettingUpForNTSC "[VO_DXR3] Nastavuji pro NTSC.\n"
#define MSGTR_LIBVO_DXR3_SettingUpForPALSECAM "[VO_DXR3] Nastavuji pro PAL/SECAM.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo43 "[VO_DXR3] Nastavuji pomìr stran 4:3.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo169 "[VO_DXR3] Nastavuji pomìr stran 16:9.\n"
#define MSGTR_LIBVO_DXR3_OutOfMemory "[VO_DXR3] do¹la pamì»\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateKeycolor "[VO_DXR3] Nemohu alokovat klíèovací barvu!\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateExactKeycolor "[VO_DXR3] Nemohu alokovat klíèovací barvu pøesnì, pou¾ívám nejbli¾¹í (0x%lx).\n"
#define MSGTR_LIBVO_DXR3_Uninitializing "[VO_DXR3] Deinicializuji.\n"
#define MSGTR_LIBVO_DXR3_FailedRestoringTVNorm "[VO_DXR3] Nepovedlo se nastavit pùvodní televizní normu!\n"
#define MSGTR_LIBVO_DXR3_EnablingPrebuffering "[VO_DXR3] Zapínám prebuffering.\n"
#define MSGTR_LIBVO_DXR3_UsingNewSyncEngine "[VO_DXR3] Pou¾ívám nový synchronizaèní kód.\n"
#define MSGTR_LIBVO_DXR3_UsingOverlay "[VO_DXR3] Pou¾ívám overlay.\n"
#define MSGTR_LIBVO_DXR3_ErrorYouNeedToCompileMplayerWithX11 "[VO_DXR3] Chyba: Overlay vy¾aduje kompilaci s nainstalovanými x11 knihovnami a hlavièkami.\n"
#define MSGTR_LIBVO_DXR3_WillSetTVNormTo "[VO_DXR3] Nastavím televizní normu na: "
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALPAL60 "pøepínám na rychlost snímkù podle filmu (PAL/PAL-60)"
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALNTSC "pøepínám na rychlost snímkù podle filmu (PAL/NTSC)"
#define MSGTR_LIBVO_DXR3_UseCurrentNorm "Pou¾ít souèasnou normu"
#define MSGTR_LIBVO_DXR3_UseUnknownNormSuppliedCurrentNorm "Pøedána neznámá norma. Pou¾iji souèasnou."
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTrying "[VO_DXR3] Chyba pøi otevírání %s pro zápis, zkusím /dev/em8300.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingMV "[VO_DXR3] Chyba pøi otevírání %s pro zápis, zkusím /dev/em8300_mv.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWell "[VO_DXR3] Chyba pøi otevírání /dev/em8300 pro zápis!\nVzdávám to.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellMV "[VO_DXR3] Chyba pøi otevírání /dev/em8300_mv pro zápis!\nVzdávám to.\n"
#define MSGTR_LIBVO_DXR3_Opened "[VO_DXR3] Otevøeno: %s.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingSP "[VO_DXR3] Chyba pøi otevírání %s pro zápis, zkou¹ím /dev/em8300_sp.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellSP "[VO_DXR3] Chyba pøi otevírání /dev/em8300_sp pro zápis!\nVzdávám to.\n"
#define MSGTR_LIBVO_DXR3_UnableToOpenDisplayDuringHackSetup "[VO_DXR3] Bìhem hacku na nastavení overlaye se nepodaøilo otevøít display!\n"
#define MSGTR_LIBVO_DXR3_UnableToInitX11 "[VO_DXR3] Nemohu inicializovat X11!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayAttribute "[VO_DXR3] Nepodaøilo se nastavit atribut overlaye.\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayScreen "[VO_DXR3] Nepodaøilo se nastavit obrazovku pro overlay!\nKonèím.\n"
#define MSGTR_LIBVO_DXR3_FailedEnablingOverlay "[VO_DXR3] Nepodaøilo se zapnout overlay!\nKonèím.\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayBcs "[VO_DXR3] Nemohu nastavit bcs overlaye!\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayYOffsetValues "[VO_DXR3] Nemohu získat posunutí Y overlaye!\nKonèím.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXOffsetValues "[VO_DXR3] Nemohu získat posunutí X overlaye!\nKonèím.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXScaleCorrection "[VO_DXR3] Nemohu získat korekci zvìt¹ení X!\nKonèím.\n"
#define MSGTR_LIBVO_DXR3_YOffset "[VO_DXR3] Posunutí Y: %d.\n"
#define MSGTR_LIBVO_DXR3_XOffset "[VO_DXR3] Posunutí X: %d.\n"
#define MSGTR_LIBVO_DXR3_XCorrection "[VO_DXR3] Korekce X: %d.\n"
#define MSGTR_LIBVO_DXR3_FailedSetSignalMix "[VO_DXR3] Nepodaøilo se nastavit signál mix!\n"

// libvo/vo_mga.c

#define MSGTR_LIBVO_MGA_AspectResized "[VO_MGA] aspect(): velikost zmìnìna na %dx%d.\n"
#define MSGTR_LIBVO_MGA_Uninit "[VO] deinicializace!\n"

// libvo/vo_null.c

#define MSGTR_LIBVO_NULL_UnknownSubdevice "[VO_NULL] Neznámé podzaøízení: %s.\n"

// libvo/vo_png.c

#define MSGTR_LIBVO_PNG_Warning1 "[VO_PNG] Upozornìní: úroveò komprimace nastavena na 0, komprimace vypnuta!\n"
#define MSGTR_LIBVO_PNG_Warning2 "[VO_PNG] Info: Pou¾ijte -vo png:z=<n> k nastavení úrovnì komprese v rozsahu 0 a¾ 9.\n"
#define MSGTR_LIBVO_PNG_Warning3 "[VO_PNG] Info: (0 = ¾ádná komprese, 1 = nejrychlej¹í, nejni¾¹í - 9 nejvy¹¹í, ale nejpomalej¹í komprese)\n"
#define MSGTR_LIBVO_PNG_ErrorOpeningForWriting "\n[VO_PNG] Nemohu otevøít '%s' pro zápis!\n"
#define MSGTR_LIBVO_PNG_ErrorInCreatePng "[VO_PNG] Chyba pøi create_png.\n"

// libvo/vo_sdl.c

#define MSGTR_LIBVO_SDL_CouldntGetAnyAcceptableSDLModeForOutput "[VO_SDL] Nemohu získat ¾ádný akceptovatelný re¾im SDL pro výstup.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailed "[VO_SDL] set_video_mode: SDL_SetVideoMode selhalo: %s.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailedFull "[VO_SDL] Set_fullmode: SDL_SetVideoMode selhalo: %s.\n"
#define MSGTR_LIBVO_SDL_MappingI420ToIYUV "[VO_SDL] Mapuji I420 na IYUV.\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormat "[VO_SDL] Nepodporovaný obrazový formát (0x%X).\n"
#define MSGTR_LIBVO_SDL_InfoPleaseUseVmOrZoom "[VO_SDL] Info: Pou¾ijte -vm nebo -zoom k pøepnutí do nejvhodnìj¹ího rozli¹ení.\n"
#define MSGTR_LIBVO_SDL_FailedToSetVideoMode "[VO_SDL] Nepodaøilo se nastavit graický re¾im: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateAYUVOverlay "[VO_SDL] Nemohu vytvoøit YUV overlay: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateARGBSurface "[VO_SDL] Nemohu vytvoøit RGB povrch: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDepthColorspaceConversion "[VO_SDL] Pou¾ívám konverzi hloubky/barevného prostoru, co¾ zpomaluje (%ibpp -> %ibpp).\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormatInDrawslice "[VO_SDL] Ve draw_slice se vyskytl nepodporovaný obrazový formát, kontaktujte vývojáøe MPlayeru!\n"
#define MSGTR_LIBVO_SDL_BlitFailed "[VO_SDL] Blit selhal: %s.\n"
#define MSGTR_LIBVO_SDL_InitializationFailed "[VO_SDL] Inicializace SDL selhala: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDriver "[VO_SDL] Pou¾ívám ovladaè: %s.\n"

// libvo/vobsub_vidix.c

#define MSGTR_LIBVO_SUB_VIDIX_CantStartPlayback "[VO_SUB_VIDIX] Nemohu spustit pøehrávání: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantStopPlayback "[VO_SUB_VIDIX] Nemohu zastavit pøehrávání: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_InterleavedUvForYuv410pNotSupported "[VO_SUB_VIDIX] Prokládané UV pro YUV410P není podporováno.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawsliceWasCalled "[VO_SUB_VIDIX] Bylo zavoláno prázdné vidix_draw_slice().\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawframeWasCalled "[VO_SUB_VIDIX] Bylo zavoláno prázdné vidix_draw_frame().\n"
#define MSGTR_LIBVO_SUB_VIDIX_UnsupportedFourccForThisVidixDriver "[VO_SUB_VIDIX] Nepodporovaný FourCC pro tento VIDIX ovladaè: %x (%s).\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedResolution "[VO_SUB_VIDIX] Video server má nepodporované rozli¹ení (%dx%d), podporováno je: %dx%d-%dx%d.\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedColorDepth "[VO_SUB_VIDIX] Video server má vidixem nepodporovanou barevnou hloubku (%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantUpscaleImage "[VO_SUB_VIDIX] Ovladaè VIDIX nemù¾e zvìt¹it obraz (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantDownscaleImage "[VO_SUB_VIDIX] Ovladaè VIDIX nemù¾e zmen¹it obraz (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantConfigurePlayback "[VO_SUB_VIDIX] Nemohu nakonfigurovat pøehrávání: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_YouHaveWrongVersionOfVidixLibrary "[VO_SUB_VIDIX] Máte ¹patnou verzi knihovny VIDIX.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntFindWorkingVidixDriver "[VO_SUB_VIDIX] Nemohu nalézt funkèní ovladaè VIDIX.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntGetCapability "[VO_SUB_VIDIX] Nemohu zjistit schopnosti: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_Description "[VO_SUB_VIDIX] Popis: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_Author "[VO_SUB_VIDIX] Autor: %s.\n"

// libvo/vo_svga.c

#define MSGTR_LIBVO_SVGA_ForcedVidmodeNotAvailable "[VO_SVGA] Vynucený vid_mode %d (%s) není k dispozici.\n"
#define MSGTR_LIBVO_SVGA_ForcedVidmodeTooSmall "[VO_SVGA] Vynucený vid_mode %d (%s) je pøíli¹ malý.\n"
#define MSGTR_LIBVO_SVGA_Vidmode "[VO_SVGA] Vid_mode: %d, %dx%d %dbpp.\n"
#define MSGTR_LIBVO_SVGA_VgasetmodeFailed "[VO_SVGA] Vga_setmode(%d) selhal.\n"
#define MSGTR_LIBVO_SVGA_VideoModeIsLinearAndMemcpyCouldBeUsed "[VO_SVGA] Grafický re¾im je lineární a mù¾eme pou¾ít k pøenosu obrazu memcpy.\n"
#define MSGTR_LIBVO_SVGA_VideoModeHasHardwareAcceleration "[VO_SVGA] Grafický re¾im má hardwarovou akceleraci a mù¾eme pou¾ít put_image.\n"
#define MSGTR_LIBVO_SVGA_IfItWorksForYouIWouldLikeToKnow "[VO_SVGA] Pokud vám to funguje, dejte mi vìdìt. \n[VO_SVGA] (po¹lete záznam z `mplayer test.avi -v -v -v -v &> svga.log`). Díky.\n"
#define MSGTR_LIBVO_SVGA_VideoModeHas "[VO_SVGA] Grafický re¾im má %d stránek.\n"
#define MSGTR_LIBVO_SVGA_CenteringImageStartAt "[VO_SVGA] Vystøeïuji obraz. Zaèínám na (%d,%d)\n"
#define MSGTR_LIBVO_SVGA_UsingVidix "[VO_SVGA] Pou¾ívám VIDIX. w=%i h=%i  mw=%i mh=%i\n"

// libvo/vo_syncfb.c

#define MSGTR_LIBVO_SYNCFB_CouldntOpen "[VO_SYNCFB] Nemohu otevøít /dev/syncfb nebo /dev/mga_vid.\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p3 "[VO_SYNCFB] Pou¾ívám paletu YUV420P3.\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p2 "[VO_SYNCFB] Pou¾ívám paletu YUV420P2.\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420 "[VO_SYNCFB] Pou¾ívám paletu YUV420.\n"
#define MSGTR_LIBVO_SYNCFB_NoSupportedPaletteFound "[VO_SYNCFB] Nenalezl jsem ¾ádnou podporovanou paletu.\n"
#define MSGTR_LIBVO_SYNCFB_BesSourcerSize "[VO_SYNCFB] BES sourcer velikost: %d x %d.\n"
#define MSGTR_LIBVO_SYNCFB_FramebufferMemory "[VO_SYNCFB] pamì» framebufferu: %ld v %ld bufferech.\n"
#define MSGTR_LIBVO_SYNCFB_RequestingFirstBuffer "[VO_SYNCFB] Po¾aduji první buffer #%d.\n"
#define MSGTR_LIBVO_SYNCFB_GotFirstBuffer "[VO_SYNCFB] Získal jsem první buffer #%d.\n"
#define MSGTR_LIBVO_SYNCFB_UnknownSubdevice "[VO_SYNCFB] neznámé podzaøízení: %s.\n"

// libvo/vo_tdfxfb.c

#define MSGTR_LIBVO_TDFXFB_CantOpen "[VO_TDFXFB] Nemohu otevøít %s: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetFscreenInfo "[VO_TDFXFB] Problém s ioctl FBITGET_FSCREENINFO: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetVscreenInfo "[VO_TDFXFB] Problém s ioctl FBITGET_VSCREENINFO: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ThisDriverOnlySupports "[VO_TDFXFB] Ovladaè podporuje jen 3Dfx Banshee, Voodoo3 a Voodoo 5.\n"
#define MSGTR_LIBVO_TDFXFB_OutputIsNotSupported "[VO_TDFXFB] %d bpp výstup není podporován.\n"
#define MSGTR_LIBVO_TDFXFB_CouldntMapMemoryAreas "[VO_TDFXFB] Nemohu namapovat pamì»ové bloky: %s.\n"
#define MSGTR_LIBVO_TDFXFB_BppOutputIsNotSupported "[VO_TDFXFB] %d bpp výstup není podporován. (To by se nemìlo nikdy stát.)\n"
#define MSGTR_LIBVO_TDFXFB_SomethingIsWrongWithControl "[VO_TDFXFB] Echt! Nìco není v poøádku s control().\n"
#define MSGTR_LIBVO_TDFXFB_NotEnoughVideoMemoryToPlay "[VO_TDFXFB] Pro pøehrávání filmu není dostatek video pamìti. Zkuste ni¾¹í rozli¹ení.\n"
#define MSGTR_LIBVO_TDFXFB_ScreenIs "[VO_TDFXFB] Obrazovka je %dx%d pøi %d bpp, vstup je %dx%d pøi %d bpp, norma je %dx%d.\n"

// libvo/vo_tdfx_vid.c

#define MSGTR_LIBVO_TDFXVID_Move "[VO_TDXVID] Pøesun %d(%d) x %d => %d.\n"
#define MSGTR_LIBVO_TDFXVID_AGPMoveFailedToClearTheScreen "[VO_TDFXVID] AGP pøesunu se nepodaøilo vyèistit obrazovku.\n"
#define MSGTR_LIBVO_TDFXVID_BlitFailed "[VO_TDFXVID] Blit selhal.\n"
#define MSGTR_LIBVO_TDFXVID_NonNativeOverlayFormatNeedConversion "[VO_TDFXVID] Ne-nativní formát overlaye potøebuje konverzi.\n"
#define MSGTR_LIBVO_TDFXVID_UnsupportedInputFormat "[VO_TDFXVID] Nepodporovaný vstupní formát 0x%x.\n"
#define MSGTR_LIBVO_TDFXVID_OverlaySetupFailed "[VO_TDFXVID] Npodaøilo se nastavit overlay.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOnFailed "[VO_TDFXVID] Zapnutí overlaye selhalo.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayReady "[VO_TDFXVID] Overlay pøipraven: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_TextureBlitReady "[VO_TDFXVID] Pøipraven blit textury: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOffFailed "[VO_TDFXVID] Vypnutí overlaye selhalo.\n"
#define MSGTR_LIBVO_TDFXVID_CantOpen "[VO_TDFXVID] Nemohu otevøít %s: %s.\n"
#define MSGTR_LIBVO_TDFXVID_CantGetCurrentCfg "[VO_TDFXVID] Nemohu získat souèasnou konfiguraci: %s.\n"
#define MSGTR_LIBVO_TDFXVID_MemmapFailed "[VO_TDFXVID] Memmap selhako!!!\n"
#define MSGTR_LIBVO_TDFXVID_GetImageTodo "Get image bude dodìlán.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailed "[VO_TDFXVID] AGP pøesun selhal.\n"
#define MSGTR_LIBVO_TDFXVID_SetYuvFailed "[VO_TDFXVID] Nastavení YUV selhalo.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnYPlane "[VO_TDFXVID] AGP pøesun selhal na slo¾ce Y.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnUPlane "[VO_TDFXVID] AGP pøesun selhal na slo¾ce U.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnVPlane "[VO_TDFXVID] AGP pøesun selhal na slo¾ce V.\n"
#define MSGTR_LIBVO_TDFXVID_UnknownFormat "[VO_TDFXVID] Neznámý formát: 0x%x.\n"

// libvo/vo_tga.c

#define MSGTR_LIBVO_TGA_UnknownSubdevice "[VO_TGA] Neznámé podzaøízení: %s.\n"

// libvo/vo_vesa.c

#define MSGTR_LIBVO_VESA_FatalErrorOccurred "[VO_VESA] Nastala záva¾ná chyba! Nemohu pokraèovat.\n"
#define MSGTR_LIBVO_VESA_UnkownSubdevice "[VO_VESA] Neznámé podzaøízení: '%s'.\n"
#define MSGTR_LIBVO_VESA_YouHaveTooLittleVideoMemory "[VO_VESA] Pro tento re¾im máte málo videopamìti:\n[VO_VESA] Po¾adováno: %08lX dostupné: %08lX.\n"
#define MSGTR_LIBVO_VESA_YouHaveToSpecifyTheCapabilitiesOfTheMonitor "[VO_VESA] Mìl(a) byste specifikovat mo¾nosti monitoru. Nebudu mìnit obnovovací frekvenci.\n"
#define MSGTR_LIBVO_VESA_UnableToFitTheMode "[VO_VESA] Po¾adavky re¾imu pøesahují schopnosti monitoru. Nebudu mìnit obnovovací frekvenci.\n"
#define MSGTR_LIBVO_VESA_DetectedInternalFatalError "[VO_VESA] Byla zji¹tìna záva¾ná chyba: init byl zavolán pøed preinit.\n"
#define MSGTR_LIBVO_VESA_SwitchFlipIsNotSupported "[VO_VESA] Volba -flip není podporována.\n"
#define MSGTR_LIBVO_VESA_PossibleReasonNoVbe2BiosFound "[VO_VESA] Mo¾né pøíèiny: Nenalezen VBE2 BIOS.\n"
#define MSGTR_LIBVO_VESA_FoundVesaVbeBiosVersion "[VO_VESA] Nalezen VESA VBE BIOS verze %x.%x revize: %x.\n"
#define MSGTR_LIBVO_VESA_VideoMemory "[VO_VESA] Video pamì»: %u Kb.\n"
#define MSGTR_LIBVO_VESA_Capabilites "[VO_VESA] Schpnosti VESA: %s %s %s %s %s.\n"
#define MSGTR_LIBVO_VESA_BelowWillBePrintedOemInfo "[VO_VESA] !!! Ní¾e najdete OEM informace !!!\n"
#define MSGTR_LIBVO_VESA_YouShouldSee5OemRelatedLines "[VO_VESA] Ní¾e by mìlo být vypsáno pìt øádkù OEM, jinak máte rozbitý vm86.\n"
#define MSGTR_LIBVO_VESA_OemInfo "[VO_VESA] OEM informace: %s.\n"
#define MSGTR_LIBVO_VESA_OemRevision "[VO_VESA] OEM revize: %x.\n"
#define MSGTR_LIBVO_VESA_OemVendor "[VO_VESA] OEM výrobce: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductName "[VO_VESA] OEM název produktu: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductRev "[VO_VESA] OEM revize produktu: %s.\n"
#define MSGTR_LIBVO_VESA_Hint "[VO_VESA] Tip: Aby fungoval TV-Out, mìl(a) byste zasunout televizní konektor\n"\
"[VO_VESA] pøed nabootováním PC, proto¾e VESA BIOS se inicializuje jen bìhem POST.\n"
#define MSGTR_LIBVO_VESA_UsingVesaMode "[VO_VESA] Pou¾ívám VESA re¾im (%u) = %x [%ux%u@%u]\n"
#define MSGTR_LIBVO_VESA_CantInitializeSwscaler "[VO_VESA] Nemohu inicializovat softwarové ¹kálování.\n"
#define MSGTR_LIBVO_VESA_CantUseDga "[VO_VESA] Nemohu pou¾ít DGA. Vynucuji re¾im pøepínání bank. :(\n"
#define MSGTR_LIBVO_VESA_UsingDga "[VO_VESA] Pou¾ívám DGA (fyzické zdroje: %08lXh, %08lXh)"
#define MSGTR_LIBVO_VESA_CantUseDoubleBuffering "[VO_VESA] Nemohu pou¾ít double buffering: není dostatek videopamìti.\n"
#define MSGTR_LIBVO_VESA_CantFindNeitherDga "[VO_VESA] Nemohu najít ani DGA, ani relokovatelný rámec okna.\n"
#define MSGTR_LIBVO_VESA_YouveForcedDga "[VO_VESA] Vynutil jste DGA. Konèím.\n"
#define MSGTR_LIBVO_VESA_CantFindValidWindowAddress "[VO_VESA] Nemohu najít platnou adresu okna.\n"
#define MSGTR_LIBVO_VESA_UsingBankSwitchingMode "[VO_VESA] Pou¾ívám øe¾im pøepínání bank (fyzické zdroje: %08lXh, %08lXh).\n"
#define MSGTR_LIBVO_VESA_CantAllocateTemporaryBuffer "[VO_VESA] Nemohu alokovat doèasný buffer.\n"
#define MSGTR_LIBVO_VESA_SorryUnsupportedMode "[VO_VESA] Promiòte, tento re¾im není podporován, zkuste -x 640 -zoom.\n"
#define MSGTR_LIBVO_VESA_OhYouReallyHavePictureOnTv "[VO_VESA] No, skuteènì máte obraz na televizi!\n"
#define MSGTR_LIBVO_VESA_CantInitialozeLinuxVideoOverlay "[VO_VESA] Nemohu inicializovat Linux Video Overlay.\n"
#define MSGTR_LIBVO_VESA_UsingVideoOverlay "[VO_VESA] Pou¾ívám video overlay: %s.\n"
#define MSGTR_LIBVO_VESA_CantInitializeVidixDriver "[VO_VESA] Nemohu inicializovat ovladaè VIDIX.\n"
#define MSGTR_LIBVO_VESA_UsingVidix "[VO_VESA] Pou¾ívám VIDIX.\n"
#define MSGTR_LIBVO_VESA_CantFindModeFor "[VO_VESA] Nemohu najít re¾im pro: %ux%u@%u.\n"
#define MSGTR_LIBVO_VESA_InitializationComplete "[VO_VESA] Inicializace VESA je dokonèena.\n"

// libvo/vo_x11.c

#define MSGTR_LIBVO_X11_DrawFrameCalled "[VO_X11] Zavoláno draw_frame()!!!\n"

// libvo/vo_xv.c

#define MSGTR_LIBVO_XV_DrawFrameCalled "[VO_XV] Zavoláno draw_frame()!!!\n"

// stream/stream_radio.c

#define MSGTR_RADIO_ChannelNamesDetected "[radio] Detekovány názvy stanic.\n"
#define MSGTR_RADIO_WrongFreqForChannel "[radio] Nesprávná frekvence pro stanici %s\n"
#define MSGTR_RADIO_WrongChannelNumberFloat "[radio] Nesprávné èíslo kanálu: %.2f\n"
#define MSGTR_RADIO_WrongChannelNumberInt "[radio] Nesprávné èíslo kanálu: %d\n"
#define MSGTR_RADIO_WrongChannelName "[radio] Nesprávné jméno kanálu: %s\n"
#define MSGTR_RADIO_FreqParameterDetected "[radio] Radio parametr detekován jako frekvence.\n"
#define MSGTR_RADIO_DoneParsingChannels "[radio] Parsování stanic dokonèeno.\n"
#define MSGTR_RADIO_GetTunerFailed "[radio] Varování: ioctl get tuner selhala: %s. Nastavuji frac na %d.\n"
#define MSGTR_RADIO_NotRadioDevice "[radio] %s není rádiovým zaøízením!\n"
#define MSGTR_RADIO_TunerCapLowYes "[radio] tuner je low:yes frac=%d\n"
#define MSGTR_RADIO_TunerCapLowNo "[radio] tuner je low:no frac=%d\n"
#define MSGTR_RADIO_SetFreqFailed "[radio] ioctl set frequency 0x%x (%.2f) selhala: %s\n"
#define MSGTR_RADIO_GetFreqFailed "[radio] ioctl get frequency selhala: %s\n"
#define MSGTR_RADIO_SetMuteFailed "[radio] ioctl set mute selhala: %s\n"
#define MSGTR_RADIO_QueryControlFailed "[radio] ioctl query control selhala: %s\n"
#define MSGTR_RADIO_GetVolumeFailed "[radio] ioctl get volume selhala: %s\n"
#define MSGTR_RADIO_SetVolumeFailed "[radio] ioctl set volume selhala: %s\n"
#define MSGTR_RADIO_DroppingFrame "\n[radio] pøíli¹ ¹patné - zahazuji audio rámec (%d bajtù)!\n"
#define MSGTR_RADIO_BufferEmpty "[radio] grab_audio_frame: prázdná vyrovnávací pamì», èekám na %d bajtù dat.\n"
#define MSGTR_RADIO_AudioInitFailed "[radio] audio_in_init selhala: %s\n"
#define MSGTR_RADIO_AudioBuffer "[radio] Zachytávání zvuku - vyrovnávací pamì»=%d bajtù (blok=%d bajtù).\n"
#define MSGTR_RADIO_AllocateBufferFailed "[radio] nemohu alokovat vyrovnávací pamì» zvuku (blok=%d,buf=%d): %s\n"
#define MSGTR_RADIO_CurrentFreq "[radio] Souèasná frekvence: %.2f\n"
#define MSGTR_RADIO_SelectedChannel "[radio] Zvolený kanál: %d - %s (frekv: %.2f)\n"
#define MSGTR_RADIO_ChangeChannelNoChannelList "[radio] Nelze zmìnit kanál: nezadán seznam kanálù.\n"
#define MSGTR_RADIO_UnableOpenDevice "[radio] Nelze otevøít '%s': %s\n"
#define MSGTR_RADIO_RadioDevice "[radio] Radio fd: %d, %s\n"
#define MSGTR_RADIO_InitFracFailed "[radio] init_frac selhala.\n"
#define MSGTR_RADIO_WrongFreq "[radio] ©patná frekvence: %.2f\n"
#define MSGTR_RADIO_UsingFreq "[radio] Pou¾ívám frekvuenci: %.2f.\n"
#define MSGTR_RADIO_AudioInInitFailed "[radio] audio_in_init selhala.\n"
#define MSGTR_RADIO_BufferString "[radio] %s: ve vyrovnávací pamìti=%d zahozeno=%d\n"
#define MSGTR_RADIO_AudioInSetupFailed "[radio] volání audio_in_setup selhalo: %s\n"
#define MSGTR_RADIO_CaptureStarting "[radio] Zahajuji zachytávání obsahu.\n"
#define MSGTR_RADIO_ClearBufferFailed "[radio] Vypráznìní vyrovnávací pamìti selhalo: %s\n"
#define MSGTR_RADIO_StreamEnableCacheFailed "[radio] Volání do stream_enable_cache selhalo: %s\n"
#define MSGTR_RADIO_DriverUnknownId "[radio] Neznámé ID ovladaèe: %d\n"
#define MSGTR_RADIO_DriverUnknownStr "[radio] Neznámé jméno ovladaèe: %s\n"
#define MSGTR_RADIO_DriverV4L2 "[radio] Pou¾ívám V4Lv2 rádio rozhraní.\n"
#define MSGTR_RADIO_DriverV4L "[radio] Pou¾ívám V4Lv1 rádio rozhraní.\n"
