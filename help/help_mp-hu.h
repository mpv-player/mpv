// Originally translated by: Gabucino the Almighty! <gabucino@mplayerhq.hu>
// Send me money/hw/babes!
//... Okay enough of the hw, now send the other two!
//
// Updated by: Gabrov <gabrov@freemail.hu>
// Sync'ed with help_mp-en.h 1.231 (2006. 03. 26.)

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Indítás:   mplayer [opciók] [url|útvonal/]fájlnév\n"
"\n"
"Alapvetõ opciók: (az összes opció listájához lásd a man lapot!)\n"
" -vo <drv[:dev]> videomeghajtó és -alegység kiválasztása (lista: '-vo help')\n"
" -ao <drv[:dev]> audiomeghajtó és -alegység kiválasztása (lista: '-ao help')\n"
#ifdef HAVE_VCD
" vcd://<sávszám>  lejátszás (S)VCD (super video cd)-sávból, közvetlenül\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titleno>  a megadott DVD sáv lejátszása, fájl helyett\n"
" -alang/-slang    DVD audio/felirat nyelv kiválasztása (2 betûs országkóddal)\n"
#endif
" -ss <idõpoz>     a megadott (másodperc v. óra:perc:mperc) pozícióra tekerés\n"
" -nosound         hanglejátszás kikapcsolása\n"
" -fs              teljesképernyõs lejátszás (vagy -vm, -zoom, bõvebben lásd man)\n"
" -x <x> -y <y>    felbontás beállítása (-vm vagy -zoom használata esetén)\n"
" -sub <fájl>      felhasználandó felirat-fájl megadása (lásd -subfps, -subdelay)\n"
" -playlist <fájl> lejátszási lista fájl megadása\n"
" -vid x -aid y    lejátszandó video- (x) és audio- (y) streamek kiválasztása\n"
" -fps x -srate y  video (x képkocka/mp) és audio (y Hz) ráta megadása\n"
" -pp <minõség>    képjavítás fokozatainak beállítása (lásd a man lapot)\n"
" -framedrop       képkockák eldobásának engedélyezése (lassú gépekhez)\n"
"\n"
"Fontosabb billentyûk: (a teljes lista a man-ban és nézd meg az input.conf fájlt)\n"
" <-  vagy  ->     10 másodperces hátra/elõre ugrás\n"
" le vagy fel      1 percnyi hátra/elõre ugrás\n"
" pgdown v. pgup   10 percnyi hátra/elõre ugrás\n"
" < vagy >         1 fájllal elõre/hátra lépés a lejátszási listában\n"
" p vagy SPACE     pillanatállj (bármely billentyûre továbbmegy)\n"
" q vagy ESC       lejátszás vége és kilépés\n"
" + vagy -         audio késleltetése ± 0.1 másodperccel\n"
" o                OSD-mód váltása:  nincs / keresõsáv / keresõsáv + idõ\n"
" * vagy /         hangerõ fel/le\n"
" x vagy z         felirat késleltetése ± 0.1 másodperccel\n"
" r vagy t         felirat pozíciójának megváltoztatása, lásd -vf expand-ot is\n"
"\n"
" * * * A MANPAGE TOVÁBBI RÉSZLETEKET, OPCIÓKAT, BILLENTYÛKET TARTALMAZ! * * *\n"
"\n";
#endif

#define MSGTR_SamplesWanted "Példa fájlokra van szükségünk ilyen formátummal, hogy jobb legyen a támogatása. Ha neked van ilyened, keresd meg a fejlesztõket.\n"

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nKilépés...\n"
#define MSGTR_ExitingHow "\nKilépés... (%s)\n"
#define MSGTR_Exit_quit "Kilépés"
#define MSGTR_Exit_eof "Vége a fájlnak"
#define MSGTR_Exit_error "Végzetes hiba"
#define MSGTR_IntBySignal "\nAz MPlayer futása %d-es szignál miatt megszakadt a(z) %s modulban\n"
#define MSGTR_NoHomeDir "Nem találom a HOME könyvtárat.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") probléma\n"
#define MSGTR_CreatingCfgFile "Konfigurációs fájl létrehozása: %s\n"
#define MSGTR_InvalidAOdriver "Nem létezõ audio meghajtó: %s\nHasználd az '-ao help' opciót, hogy listát kapj a használható ao meghajtókról.\n"
#define MSGTR_CopyCodecsConf "(másold/linkeld az etc/codecs.conf fájlt ~/.mplayer/codecs.conf-ba)\n"
#define MSGTR_BuiltinCodecsConf "Befordított codecs.conf használata.\n"
#define MSGTR_CantLoadFont "Nem tudom betölteni a következõ fontot: %s\n"
#define MSGTR_CantLoadSub "Nem tudom betölteni a feliratot: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: VÉGZETES HIBA: a kért stream nem található!\n"
#define MSGTR_CantOpenDumpfile "Nem tudom megnyitni a dump fájlt!\n"
#define MSGTR_CoreDumped "Kinyomattam a cuccost, jól.\n"
#define MSGTR_FPSnotspecified "Az FPS (képkocka/mp) érték nincs megadva, vagy hibás! Használd az -fps opciót!\n"
#define MSGTR_TryForceAudioFmtStr "Megpróbálom a(z) %s audio codec-családot használni...\n"
#define MSGTR_CantFindAudioCodec "Nem találok codecet a(z) 0x%X audio-formátumhoz!\n"
#define MSGTR_RTFMCodecs "Olvasd el a DOCS/HTML/hu/codecs.html fájlt!\n"
#define MSGTR_TryForceVideoFmtStr "Megpróbálom a(z) %s video codec-családot használni...\n"
#define MSGTR_CantFindVideoCodec "Nem találok codec-et ami megfelel a kivalasztott vo-hoz és 0x%X video-formátumhoz!\n"
#define MSGTR_CannotInitVO "VÉGZETES HIBA: Nem tudom elindítani a video-meghajtót!\n"
#define MSGTR_CannotInitAO "Nem tudom megnyitni az audio-egységet -> nincs hang.\n"
#define MSGTR_StartPlaying "Lejátszás indítása...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***************************************\n"\
"         **** A rendszered túl LASSÚ ehhez! ****\n"\
"         ***************************************\n"\
"Lehetséges okok, és megoldásaik:\n"\
"- Legyakrabban : hibás _audio_ meghajtó\n"\
"  - Próbáld ki az -ao sdl opciót, vagy használd az ALSA OSS emulációját.\n"\
"  - Adj különbözõ értékeket az -autosync opciónak, kezdetnek a 30 megteszi.\n"\
"- Lassú videokimenet\n"\
"  - Egy másik -vo meghajtó kipróbálása eredményre vezethet (a listához lásd\n"\
"    -vo help), és/vagy használd a -framedrop opciót!\n"\
"- Lassú CPU\n"\
"  - Nagy felbontású DivX/DVD lejátszásával ne próbálkozz gyenge processzoron!\n"\
"    Esetleg próbálj ki lavdopts opciókat, pl.\n"\
"    -vfm ffmpeg -lavdopts lowres=1:fast:skiploopfilter=all.\n"\
"- Hibás fájl\n"\
"  - A -nobps -ni -forceidx -mc 0 opciók kombinációval érdemes szórakozni.\n"\
"- Lassú média (NFS/SMB, DVD, VCD, stb)\n"\
"  - Próbáld ki a -cache 8192 opciót.\n"\
"- Talán egy non-interleaved AVI fájlt próbálsz -cache opcióval lejátszani?\n"\
"  - Használd a -nocache opciót.\n"\
"Tuninghoz tippeket a DOCS/HTML/hu/video.html fájlban találsz.\n"\
"Ha ez sem segít, olvasd el a DOCS/HTML/hu/bugreports.html fájlt.\n\n"

#define MSGTR_NoGui "Az MPlayer grafikus felület NÉLKÜL lett fordítva!\n"
#define MSGTR_GuiNeedsX "Az MPlayer grafikus felületének X11-re van szüksége!\n"
#define MSGTR_Playing "%s lejátszása\n"
#define MSGTR_NoSound "Audio: nincs hang!!!\n"
#define MSGTR_FPSforced "FPS kényszerítve %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Futásidejû CPU detektálás használata.\n"
#define MSGTR_CompiledWithCPUExtensions "x86-os CPU - a következõ kiterjesztésekkel:"
#define MSGTR_AvailableVideoOutputDrivers "Rendelkezésre álló video meghajtók:\n"
#define MSGTR_AvailableAudioOutputDrivers "Rendelkezésre álló audio meghajtók:\n"
#define MSGTR_AvailableAudioCodecs "Rendelkezésre álló audio codec-ek:\n"
#define MSGTR_AvailableVideoCodecs "Rendelkezésre álló video codec-ek:\n"
#define MSGTR_AvailableAudioFm "Rendelkezésre álló (befordított) audio codec családok/meghajtók:\n"
#define MSGTR_AvailableVideoFm "Rendelkezésre álló (befordított) video codec családok/meghajtók:\n"
#define MSGTR_AvailableFsType "A használható teljesképernyõs réteg-módok:\n"
#define MSGTR_UsingRTCTiming "Linux hardveres RTC idõzítés használata (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Video: tulajdonságok beolvasása nem lehetséges.\n"
#define MSGTR_NoStreamFound "Nem található stream\n"
#define MSGTR_ErrorInitializingVODevice "Hiba a kiválasztott video_out (-vo) egység inicializásakor!\n"
#define MSGTR_ForcedVideoCodec "Kényszerített video codec: %s\n"
#define MSGTR_ForcedAudioCodec "Kényszerített audio codec: %s\n"
#define MSGTR_Video_NoVideo "Video: nincs video!!!\n"
#define MSGTR_NotInitializeVOPorVO "\nHIBA: Nem sikerült a video filterek (-vf) vagy a video kimenet (-vo) inicializálása!\n"
#define MSGTR_Paused "\n  =====  SZÜNET  =====\r"
#define MSGTR_PlaylistLoadUnable "\nLejátszási lista (%s) betöltése sikertelen.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- Az MPlayer egy 'illegális utasítást' hajtott végre.\n"\
"  Lehet hogy a futásidejû CPU detektáló kód hibája...\n"\
"  Olvasd el a DOCS/HTML/hu/bugreports.html fájlt!\n"
#define MSGTR_Exit_SIGILL \
"- Az MPlayer egy 'illegális utasítást' hajtott végre.\n"\
"  Ez akkor történik amikor más CPU-n futtatod az MPlayer-t mint amire a\n"\
"  fordítás/optimalizálás történt.\n"\
"  Ellenõrizd!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- Az MPlayer röpke félrelépése miatt hiba lépett fel a CPU/FPU/RAM-ban.\n"\
"  Fordítsd újra az MPlayer-t az --enable-debug opcióval, és készíts egy\n"\
"  'gdb' backtrace-t. Bõvebben: DOCS/HTML/hu/bugreports.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- Az MPlayer összeomlott. Ennek nem lenne szabad megtörténnie. Az ok lehet\n"\
"  egy hiba az MPlayer kódjában _vagy_ a Te meghajtóidban, _vagy_ a gcc-ben.\n"\
"  Ha úgy véled hogy ez egy MPlayer hiba, úgy olvasd el a\n"\
"  DOCS/HTML/hu/bugreports.html fájlt és kövesd az utasításait! Nem tudunk és\n"\
"  nem fogunk segíteni, amíg nem szolgálsz megfelelõ információkkal a hiba\n"\
"  bejelentésekor.\n"
#define MSGTR_LoadingConfig "'%s' konfiguráció betöltése\n"
#define MSGTR_AddedSubtitleFile "SUB: felirat fájl (%d) hozzáadva: %s\n"
#define MSGTR_RemovedSubtitleFile "SUB: felirat fájl (%d) eltávolítva: %s\n"
#define MSGTR_ErrorOpeningOutputFile "Hiba a(z) [%s] fájl írásakor!\n"
#define MSGTR_CommandLine "Parancs sor:"
#define MSGTR_RTCDeviceNotOpenable "%s megnyitása nem sikerült: %s (a felhasználó által olvashatónak kell lennie.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Linux RTC inicializálási hiba az ioctl-ben (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Próbáld ki ezt: \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" hozzáadni a rendszer indító script-jeidhez!\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Linux RTC inicializálási hiba az ioctl-ben (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "%s idõzítés használata.\n"
#define MSGTR_NoIdleAndGui "Az -idle opció nem használható a GMPlayerrel.\n"
#define MSGTR_MenuInitialized "Menü inicializálva: %s\n"
#define MSGTR_MenuInitFailed "Menü inicializálás nem sikerült.\n"
#define MSGTR_Getch2InitializedTwice "FIGYELEM: getch2_init kétszer lett meghívva!\n"
#define MSGTR_DumpstreamFdUnavailable "Ezt a folyamot nem lehet dump-olni - a fájlleíró nem elérhetõ.\n"
#define MSGTR_FallingBackOnPlaylist "Visszalépés a(z) %s lejátszási lista értelmezése közben...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "A libmenu video szûrõt nem sikerült a(z) %s fõmenüvel megnyitni.\n"
#define MSGTR_AudioFilterChainPreinitError "Hiba az audio szûrõ lánc elõ-inicializálásában!\n"
#define MSGTR_LinuxRTCReadError "Linux RTC olvasási hiba: %s\n"
#define MSGTR_SoftsleepUnderflow "Figyelem! Softsleep alulcsordulás!\n"
#define MSGTR_DvdnavNullEvent "DVDNAV esemény NULL (NINCS)?!\n"
#define MSGTR_DvdnavHighlightEventBroken "DVDNAV esemény: Kiemelés esemény hibás\n"
#define MSGTR_DvdnavEvent "DVDNAV esemény: %s\n"
#define MSGTR_DvdnavHighlightHide "DVDNAV esemény: Kiemelés elrejtése\n"
#define MSGTR_DvdnavStillFrame "######################################## DVDNAV esemény: Still Frame: %d mp\n"
#define MSGTR_DvdnavNavStop "DVDNAV esemény: Nav Stop\n"
#define MSGTR_DvdnavNavNOP "DVDNAV esemény: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "DVDNAV esemény: Nav SPU folyam váltás: fizikai: %d/%d/%d logikai: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "DVDNAV esemény: Nav SPU folyam váltás: fizikai: %d logikai: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "DVDNAV esemény: Nav Audio folyam váltás: fizikai: %d logikai: %d\n"
#define MSGTR_DvdnavNavVTSChange "DVDNAV esemény: Nav VTS váltás\n"
#define MSGTR_DvdnavNavCellChange "DVDNAV esemény: Nav cella váltás\n"
#define MSGTR_DvdnavNavSpuClutChange "DVDNAV esemény: Nav SPU CLUT váltás\n"
#define MSGTR_DvdnavNavSeekDone "DVDNAV esemény: Nav keresés kész\n"
#define MSGTR_MenuCall "Menü hívás\n"

#define MSGTR_EdlOutOfMem "Nem lehet elegendõ memóriát foglalni az EDL adatoknak.\n"
#define MSGTR_EdlRecordsNo "%d EDL akciók olvasása.\n"
#define MSGTR_EdlQueueEmpty "Nincs olyan EDL akció, amivel foglalkozni kellene.\n"
#define MSGTR_EdlCantOpenForWrite "Az EDL fájlba [%s] nem lehet írni.\n"
#define MSGTR_EdlCantOpenForRead "Az EDL fájlt [%s] nem lehet olvasni.\n"
#define MSGTR_EdlNOsh_video "Az EDL nem használható video nélkül, letiltva.\n"
#define MSGTR_EdlNOValidLine "Hibás EDL sor: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Hibás formátumú EDL sor [%d], kihagyva.\n"
#define MSGTR_EdlBadLineOverlap "Az utolsó megállítási pozíció [%f] volt; a következõ indulási "\
"[%f]. A bejegyzéseknek idõrendben kell lenniük, nem átlapolhatóak. Kihagyva.\n"
#define MSGTR_EdlBadLineBadStop "A megállítási idõnek a kezdési idõ után kell lennie.\n"

// mplayer.c OSD

#define MSGTR_OSDenabled "bekapcsolva"
#define MSGTR_OSDdisabled "kikapcsolva"
#define MSGTR_OSDChannel "Csatorna: %s"
#define MSGTR_OSDSubDelay "Sub késés: %d ms"
#define MSGTR_OSDSpeed "Sebesség: x %6.2f"
#define MSGTR_OSDosd "OSD: %s"

// property values
#define MSGTR_Enabled "bekapcsolva"
#define MSGTR_EnabledEdl "bekapcsolva (edl)"
#define MSGTR_Disabled "kikapcsolva"
#define MSGTR_HardFrameDrop "erõs"
#define MSGTR_Unknown "ismeretlen"
#define MSGTR_Bottom "alul"
#define MSGTR_Center "középen"
#define MSGTR_Top "fent"

// osd bar names
#define MSGTR_Volume "Hangerõ"
#define MSGTR_Panscan "Panscan"
#define MSGTR_Gamma "Gamma"
#define MSGTR_Brightness "Fényerõ"
#define MSGTR_Contrast "Kontraszt"
#define MSGTR_Saturation "Telítettség"
#define MSGTR_Hue "Árnyalat"

// property state
#define MSGTR_MuteStatus "Némít: %s"
#define MSGTR_AVDelayStatus "A-V késés: %d ms"
#define MSGTR_OnTopStatus "Mindig felül: %s"
#define MSGTR_RootwinStatus "Fõablak: %s"
#define MSGTR_BorderStatus "Keret: %s"
#define MSGTR_FramedroppingStatus "Képkocka dobás: %s"
#define MSGTR_VSyncStatus "VSync: %s"
#define MSGTR_SubSelectStatus "Feliratok: %s"
#define MSGTR_SubPosStatus "Felirat helye: %s/100"
#define MSGTR_SubAlignStatus "Felirat illesztés: %s"
#define MSGTR_SubDelayStatus "Felirat késés: %s"
#define MSGTR_SubVisibleStatus "Feliratok: %s"
#define MSGTR_SubForcedOnlyStatus "Csak kényszerített felirat: %s"

// mencoder.c:

#define MSGTR_UsingPass3ControllFile "Pass3 vezérlõ fájl használata: %s\n"
#define MSGTR_MissingFilename "\nHiányzó fájlnév!\n\n"
#define MSGTR_CannotOpenFile_Device "Fájl/eszköz megnyitása sikertelen.\n"
#define MSGTR_CannotOpenDemuxer "Demuxer meghívása sikertelen.\n"
#define MSGTR_NoAudioEncoderSelected "\nNem választottál ki audio enkódert (-oac)! Válassz egyet (lásd -oac help), vagy használd a -nosound opciót!\n"
#define MSGTR_NoVideoEncoderSelected "\nNem választottál ki video enkódert (-ovc)! Válassz egyet (lásd -ovc help)!\n"
#define MSGTR_CannotOpenOutputFile "Nem tudom a kimeneti fájlt (%s) megnyitni.\n"
#define MSGTR_EncoderOpenFailed "Enkóder hívása sikertelen.\n"
#define MSGTR_MencoderWrongFormatAVI "\nFIGYELEM: A KIMENETI FÁJL FORMÁTUM _AVI_. Lásd -of help.\n"
#define MSGTR_MencoderWrongFormatMPG "\nFIGYELEM: A KIMENETI FÁJL FORMÁTUM _MPEG_. Lásd -of help.\n"
#define MSGTR_MissingOutputFilename "Nincs kimeneti fájl megadva, lásd a -o kapcsolót"
#define MSGTR_ForcingOutputFourcc "Kimeneti fourcc kényszerítése: %x [%.4s]\n"
#define MSGTR_ForcingOutputAudiofmtTag "Audió formátum tag kényszerítése: 0x%x\n"
#define MSGTR_DuplicateFrames "\n%d darab képkocka duplázása!!!\n"
#define MSGTR_SkipFrame "\nképkocka átugrása!!!\n"
#define MSGTR_ResolutionDoesntMatch "\nAz új videó fájl felbontása vagy színtere különbözik az elõzõétõl.\n"
#define MSGTR_FrameCopyFileMismatch "\nAz összes videó fájlnak azonos fps-sel, felbontással, és codec-kel kell rendelkeznie az -ovc copy-hoz.\n"
#define MSGTR_AudioCopyFileMismatch "\nAz összes fájlnak azonos audió codec-kel és formátummal kell rendelkeznie az -oac copy-hoz.\n"
#define MSGTR_NoAudioFileMismatch "\nNem lehet a csak videót tartalmazó fájlokat összekeverni audió és videó fájlokkal. Próbáld a -nosound kapcsolót.\n"
#define MSGTR_NoSpeedWithFrameCopy "FIGYELEM: A -speed nem biztos, hogy jól mûködik az -oac copy-val!\n"\
"A kódolásod hibás lehet!\n"
#define MSGTR_ErrorWritingFile "%s: hiba a fájl írásánál.\n"
#define MSGTR_RecommendedVideoBitrate "Ajánlott video bitráta %s CD-hez: %d\n"
#define MSGTR_VideoStreamResult "\nVideo stream: %8.3f kbit/mp  (%d B/s)  méret: %d byte  %5.3f mp  %d képkocka\n"
#define MSGTR_AudioStreamResult "\nAudio stream: %8.3f kbit/mp  (%d B/s)  méret: %d byte  %5.3f mp\n"
#define MSGTR_OpenedStream "kész: formátum: %d  adat: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videocodec: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "audiocodec: framecopy (formátum=%x csati=%d ráta=%d bit=%d B/s=%d sample-%ld)\n"
#define MSGTR_CBRPCMAudioSelected "CBR PCM audio kiválasztva\n"
#define MSGTR_MP3AudioSelected "MP3 audio kiválasztva\n"
#define MSGTR_CannotAllocateBytes "%d byte nem foglalható le\n"
#define MSGTR_SettingAudioDelay "Audió késleltetés beállítása: %5.3fs\n"
#define MSGTR_SettingVideoDelay "Videó késleltetés beállítása: %5.3fs\n"
#define MSGTR_SettingAudioInputGain "Audio input erõsítése %f\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Audio elõretöltés korlátozva 0.4 mp-re\n"
#define MSGTR_IncreasingAudioDensity "Audio tömörítés növelése 4-re\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Audio elõretöltés 0-ra állítva, max pts javítás 0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR audio: %d byte/mp, %d byte/blokk\n"
#define MSGTR_LameVersion "LAME %s (%s) verzió\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Hiba: A megadott bitráta az ezen beállításhoz tartozó határokon kívül van\n"\
"\n"\
"Ha ezt a módot használod, \"8\" és \"320\" közötti értéket kell megadnod\n"\
"\n"\
"További információkért lásd a \"-lameopts preset=help\" kapcsolót!\n"
#define MSGTR_InvalidLamePresetOptions "Hiba: Nem adtál meg érvényes profilt és/vagy opciókat a preset-tel\n"\
"\n"\
"Az elérhetõ profilok:\n"\
"\n"\
"   <fast>        alapértelmezett\n"\
"   <fast>        extrém\n"\
"                 õrült\n"\
"   <cbr> (ABR Mód) - Az ABR Mode beépített. Használatához\n"\
"                      csak adj meg egy bitrátát. Például:\n"\
"                      \"preset=185\" aktiválja ezt a\n"\
"                      beállítást és 185 lesz az átlagos kbps.\n"\
"\n"\
"    Néhány példa:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"További információkért lásd a \"-lameopts preset=help\" kapcsolót!\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"A preset kapcsolók azért lettek létrehozva, hogy a lehetõ legjobb minõséget biztosítsák.\n"\
"\n"\
"Legtöbbször elvakult, könyörtelen vájtfülûek tárgyalják ki és állítgatják õket,\n"\
"hogy elérjék a céljaikat.\n"\
"\n"\
"Ezen változtatások folyamatosan frissítésre kerülnek, hogy illeszkedjenek a\n"\
"legújabb fejlesztésekhez és az eredmény majdnem a legjobb minõséget biztosítsa\n"\
"Neked, ami elérhetõ a LAME-mel.\n"\
"\n"\
"Preset-ek aktiválása:\n"\
"\n"\
"   VBR (változó bitráta) módokhoz (általában a legjobb minõség):\n"\
"\n"\
"     \"preset=standard\" Ez a beállítás ajánlott a legtöbb felhasználónak\n"\
"                             a zenék nagy részénél, és már ez is megfelelõen\n"\
"                             jó minõséget biztosít.\n"\
"\n"\
"     \"preset=extreme\" Ha különösen jó hallásod és hasonlóan jó felszerelésed\n"\
"                             van, ez a beállítás meglehetõsen jobb minõséget\n"\
"                             fog biztosítani mint a \"standard\" mód.\n"\
"                             \n"\
"\n"\
"   CBR (állandó bitráta) 320kbps (a preset-tel elérhetõ legjobb minõség):\n"\
"\n"\
"     \"preset=insane\"  Ez a beállítás \"ágyuval verébre\" eset a legtöbb\n"\
"                             embernél és a legtöbb esetben, de ha abszolút a\n"\
"                             legjobb minõségre van szükséged a fájl méretétõl\n"\
"                             függetlenül, akkor ez kell neked.\n"\
"\n"\
"   ABR (átlagos bitráta) mód (kiváló minõség az adott bitrátához de nem VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Ezen preset használatával legtöbbször jó minõséget\n"\
"                             kapsz a megadott bitrátával. Az adott bitrátától\n"\
"                             függõen ez a preset meghatározza az optimális\n"\
"                             beállításokat.\n"\
"                             Amíg ez a megközelítés mûködik, messze nem olyan\n"\
"                             rugalmas, mint a VBR, és legtöbbször nem lesz\n"\
"                             olyan minõségû, mint a magas bitrátájú VBR-rel.\n"\
"\n"\
"A következõ opciók is elérhetõek a megfelelõ profilokhoz:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extrém\n"\
"                 õrült\n"\
"   <cbr> (ABR mód) - Az ABR mód beépített. Használatához egyszerûen\n"\
"                     csak add meg a bitrátát. Például:\n"\
"                     \"preset=185\" ezt a preset-et aktiválja\n"\
"                     és 185-ös átlagos kbps-t használ.\n"\
"\n"\
"   \"fast\" - Engedélyezi az új, gyors VBR-t a megfelelõ profilban.\n"\
"            Hátránya, hogy a sebesség miatt a bitráta gyakran \n"\
"            kicsit nagyobb lesz, mint a normál módban, a minõség pedig\n"\
"            kicsit rosszabb.\n"\
"   Figyelem: a jelenlegi állapotban a gyors preset-ek túl nagy bitrátát\n"\
"             produkálnak a normális preset-hez képest.\n"\
"\n"\
"   \"cbr\"  - Ha az ABR módot használod (lásd feljebb) egy olyan bitrátával\n"\
"            mint a 80, 96, 112, 128, 160, 192, 224, 256, 320, használhatod\n"\
"            a \"cbr\" opciót hogy elõírd a CBR módot a kódolásnál az\n"\
"            alapértelmezett abr mód helyett. Az ABR jobb minõséget biztosít,\n"\
"            de a CBR hasznosabb lehet olyan esetekben, mint pl. amikor fontos\n"\
"            az MP3 Interneten történõ streamelhetõsége.\n"\
"\n"\
"    Például:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Pár álnév, ami elérhetõ az ABR módban:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit "A Lame opciók nem állíthatóak be, ellenõrizd a"\
"bitrátát/mintavételi rátát, néhány nagyon alacsony bitrátához (<32) alacsonyabb"\
"mintavételi ráta kell (pl. -srate 8000)."\
"Ha minden más sikertelen, próbálj ki egy preset-et."
#define MSGTR_ConfigfileError "konfigurációs fájl hibája"
#define MSGTR_ErrorParsingCommandLine "hiba a parancssor értelmezésekor"
#define MSGTR_VideoStreamRequired "Video stream szükséges!\n"
#define MSGTR_ForcingInputFPS "az input fps inkább %5.2f-ként lesz értelmezve\n"
#define MSGTR_RawvideoDoesNotSupportAudio "A RAWVIDEO kimeneti fájl formátum nem támogatja a hangot - audio letiltva\n"
#define MSGTR_DemuxerDoesntSupportNosound "Ez a demuxer még nem támogatja a -nosound kapcsolót.\n"
#define MSGTR_MemAllocFailed "Nem sikerült a memóriafoglalás\n"
#define MSGTR_NoMatchingFilter "Nem találtam megfelelõ szûrõt/ao formátumot!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, talán hibás C fordító?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, hiányzó codec név!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, nem található kódoló a(z) %s codec-hez.\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, nem található a kontextus!\n"
#define MSGTR_CouldntOpenCodec "A(z) %s codec nem nyitható meg, br=%d\n"
#define MSGTR_CantCopyAudioFormat "A(z) 0x%x audió formátum nem kompatibilis a '-oac copy'-val, kérlek próbáld meg a '-oac pcm' helyette vagy használd a '-fafmttag'-ot a felülbírálásához.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     a változó bitrátájú kódolás módja\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(alapértelmezett)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           átlagos bitráta\n"\
"\n"\
" cbr           konstans bitráta\n"\
"               Elõírja a CBR módú kódolást a késõbbi ABR módokban is.\n"\
"\n"\
" br=<0-1024>   bitráta kBit-ben (csak CBR és ABR)\n"\
"\n"\
" q=<0-9>       minõség (0-legjobb, 9-legrosszabb) (csak VBR-nél)\n"\
"\n"\
" aq=<0-9>      algoritmikus minõség (0-legjobb, 9-legrosszabb/leggyorsabb)\n"\
"\n"\
" ratio=<1-100> tömörítés aránya\n"\
"\n"\
" vol=<0-10>    audio bemenet hangereje\n"\
"\n"\
" mode=<0-3>    (alap: automatikus)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: nincs\n"\
"                1: mind\n"\
"                2: állítás\n"\
"\n"\
" fast          valamivel gyorsabb VBR kódolás, kicsit rosszabb minõség és\n"\
"               magasabb bitráta.\n"\
"\n"\
" preset=<érték> A lehetõ legjobb minõséget biztosítja.\n"\
"                 medium: VBR  kódolás,  kellemes minõség\n"\
"                 (150-180 kbps bitráta tartomány)\n"\
"                 standard:  VBR kódolás, jó minõség\n"\
"                 (170-210 kbps bitráta tartomány)\n"\
"                 extreme: VBR kódolás, nagyon jó minõség\n"\
"                 (200-240 kbps bitráta tartomány)\n"\
"                 insane:  CBR  kódolás, legjobb minõség\n"\
"                 (320 kbps bitráta)\n"\
"                 <8-320>: ABR kódolás átlagban a megadott bitrátával.\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "dupla FourCC"
#define MSGTR_TooManyFourccs "túl sok FourCCs/formátum..."
#define MSGTR_ParseError "értelmezési hiba"
#define MSGTR_ParseErrorFIDNotNumber "értelmezési hiba (a formátum ID nem szám?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "értelmezési hiba (a formátum ID álnév nem szám?)"
#define MSGTR_DuplicateFID "duplikált formátum ID"
#define MSGTR_TooManyOut "túl sok kiesett..."
#define MSGTR_InvalidCodecName "\na codec(%s) név hibás!\n"
#define MSGTR_CodecLacksFourcc "\na codec(%s) nem FourCC/formátumú!\n"
#define MSGTR_CodecLacksDriver "\na codec(%s) nem rendelkezik vezélõvel!\n"
#define MSGTR_CodecNeedsDLL "\na codec(%s) 'dll'-t igényel!\n"
#define MSGTR_CodecNeedsOutfmt "\ncodec(%s) 'outfmt'-t igényel!\n"
#define MSGTR_CantAllocateComment "Nem tudok memóriát foglalni a megjegyzésnek. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "%s olvasása: "
#define MSGTR_CantOpenFileError "Nem tudom megnyitni '%s'-t: %s\n"
#define MSGTR_CantGetMemoryForLine "Nem tudok memóriát foglalni a 'line'-nak: %s\n"
#define MSGTR_CantReallocCodecsp "A '*codecsp' nem foglalható le újra: %s\n"
#define MSGTR_CodecNameNotUnique "A(z) '%s' codec név nem egyedi."
#define MSGTR_CantStrdupName "Nem végezhetõ el: strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Nem végezhetõ el: strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Nem végezhetõ el: strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Nem végezhetõ el: strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d audió & %d videó codec\n"
#define MSGTR_CodecDefinitionIncorrect "A codec nincs megfelelõen definiálva."
#define MSGTR_OutdatedCodecsConf "Ez a codecs.conf túl régi és nem kompatibilis az MPlayer ezen kiadásával!"

// divx4_vbr.c:
#define MSGTR_OutOfMemory "elfogyott a memória"
#define MSGTR_OverridingTooLowBitrate "A megadott bitráta túl alacsony ehhez a klipphez.\n"\
"A minimális lehetséges bitráta ehhez a klipphez %.0f kbps. A felhasználói\n"\
"érték felülbírálva.\n"

// fifo.c
#define MSGTR_CannotMakePipe "Nem hozható létre PIPE!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Túl régi mentési slotot találtam az %d lvl-bõl: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "A(z) %s kapcsoló nem használható konfigurációs fájlban.\n"
#define MSGTR_InvalidCmdlineOption "A(z) %s kapcsoló nem használható parancssorból.\n"
#define MSGTR_InvalidSuboption "Hiba: '%s' kapcsolónak nincs '%s' alopciója.\n"
#define MSGTR_MissingSuboptionParameter "Hiba: a(z) '%s' '%s' alkapcsolójához paraméter kell!\n"
#define MSGTR_MissingOptionParameter "Hiba: a(z) '%s' kapcsolóhoz kell egy paraméter!\n"
#define MSGTR_OptionListHeader "\n Név                  Típus           Min        Max      Globál  CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nÖsszesen: %d kapcsoló\n"
#define MSGTR_TooDeepProfileInclusion "FIGYELMEZTETÉS: Túl mély profil beágyazás.\n"
#define MSGTR_NoProfileDefined "Nincs profil megadva.\n"
#define MSGTR_AvailableProfiles "Elérhetõ profilok:\n"
#define MSGTR_UnknownProfile "Ismeretlen profil: '%s'.\n"
#define MSGTR_Profile "Profil %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n Név                  Típus           Min        Max\n\n"
#define MSGTR_TotalProperties "\nÖsszesen: %d tulajdonság\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "A CD-ROM meghajtó (%s) nem található!\n"
#define MSGTR_ErrTrackSelect "Hiba a VCD-sáv kiválasztásakor!"
#define MSGTR_ReadSTDIN "Olvasás a szabványos bemenetrõl (stdin)...\n"
#define MSGTR_UnableOpenURL "Nem megnyitható az URL: %s\n"
#define MSGTR_ConnToServer "Csatlakozom a szerverhez: %s\n"
#define MSGTR_FileNotFound "A fájl nem található: '%s'\n"

#define MSGTR_SMBInitError "Samba kliens könyvtár nem inicializálható: %d\n"
#define MSGTR_SMBFileNotFound "Nem nyitható meg a hálózatról: '%s'\n"
#define MSGTR_SMBNotCompiled "Nincs befordítva az MPlayerbe az SMB támogatás\n"

#define MSGTR_CantOpenDVD "Nem tudom megnyitni a DVD eszközt: %s\n"
#define MSGTR_NoDVDSupport "Az MPlayer DVD támogatás nélkül lett lefordítva, kilépés\n"
#define MSGTR_DVDwait "A lemez struktúrájának olvasása, kérlek várj...\n"
#define MSGTR_DVDnumTitles "%d sáv van a DVD-n.\n"
#define MSGTR_DVDinvalidTitle "Helytelen DVD sáv: %d\n"
#define MSGTR_DVDnumChapters "Az adott DVD sávban %d fejezet van.\n"
#define MSGTR_DVDinvalidChapter "Helytelen DVD fejezet: %d\n"
#define MSGTR_DVDinvalidChapterRange "Helytelen fejezet tartomány specifikáció: %s\n"
#define MSGTR_DVDinvalidLastChapter "Helytelen DVD utolsó fejezet szám: %d\n"
#define MSGTR_DVDnumAngles "%d darab kameraállás van ezen a DVD sávon.\n"
#define MSGTR_DVDinvalidAngle "Helytelen DVD kameraállás: %d\n"
#define MSGTR_DVDnoIFO "Nem tudom a(z) %d. DVD sávhoz megnyitni az IFO fájlt.\n"
#define MSGTR_DVDnoVMG "A VMG infót nem lehet megnyitni!\n"
#define MSGTR_DVDnoVOBs "Nem tudom megnyitni a VOBS sávokat (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDnoMatchingAudio "Nem található megfelelõ nyelvû DVD audió!\n"
#define MSGTR_DVDaudioChannel "Kiválasztott DVD audió csatorna: %d nyelv: %c%c\n"
#define MSGTR_DVDnoMatchingSubtitle "Nincs megfelelõ nyelvû DVD felirat fájl!\n"
#define MSGTR_DVDsubtitleChannel "Kiválasztott DVD felirat csatorna: %d nyelv: %c%c\n"
#define MSGTR_DVDopenOk "DVD sikeresen megnyitva!\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "Túl sok stream!"
#define MSGTR_RawMuxerOnlyOneStream "A rawaudio muxer csak egy audió folyamot támogat!\n"
#define MSGTR_IgnoringVideoStream "Videó folyam figyelmen kívül hagyva!\n"
#define MSGTR_UnknownStreamType "Figyelem! Ismeretlen folyam típus: %d\n"
#define MSGTR_WarningLenIsntDivisible "Figyelem! A len nem osztható a samplesize-zal!\n"
#define MSGTR_MuxbufMallocErr "Muxer keret buffernek nem sikerült memóriát foglalni!\n"
#define MSGTR_MuxbufReallocErr "Muxer keret buffernek nem sikerült memóriát újrafoglalni!\n"
#define MSGTR_MuxbufSending "Muxer keret bufferbõl %d keret átküldve a muxer-nek.\n"
#define MSGTR_WritingHeader "Fejléc írása...\n"
#define MSGTR_WritingTrailer "Index írása...\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Vigyázat! Többszörösen definiált Audio-folyam: %d (Hibás fájl?)\n"
#define MSGTR_VideoStreamRedefined "Vigyázat! Többszörösen definiált Video-folyam: %d (Hibás fájl?)\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) audio-csomag a pufferben!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) video-csomag a pufferben!\n"
#define MSGTR_MaybeNI "Talán ez egy nem összefésült (interleaved) fájl vagy a codec nem mûködik jól?\n" \
		      "AVI fájloknál próbáld meg a non-interleaved mód kényszerítését a -ni opcióval.\n"
#define MSGTR_SwitchToNi "\nRosszul összefésült (interleaved) fájl, átváltás -ni módba!\n"
#define MSGTR_Detected_XXX_FileFormat "Ez egy %s formátumú fájl!\n"
#define MSGTR_DetectedAudiofile "Audio fájl detektálva!\n"
#define MSGTR_NotSystemStream "Nem MPEG System Stream formátum... (talán Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Hibás MPEG-ES-folyam? Lépj kapcsolatba a készítõkkel, lehet, hogy hiba!\n"
#define MSGTR_FormatNotRecognized "========= Sajnos ez a fájlformátum ismeretlen vagy nem támogatott ===========\n"\
				  "= Ha ez egy AVI, ASF vagy MPEG fájl, lépj kapcsolatba a készítõkkel (hiba)! =\n"
#define MSGTR_MissingVideoStream "Nincs képfolyam!\n"
#define MSGTR_MissingAudioStream "Nincs hangfolyam... -> hang nélkül\n"
#define MSGTR_MissingVideoStreamBug "Nincs képfolyam?! Írj a szerzõnek, lehet hogy hiba :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: a fájl nem tartalmazza a kért hang vagy kép folyamot\n"

#define MSGTR_NI_Forced "Kényszerítve"
#define MSGTR_NI_Detected "Detektálva"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI formátum!\n"

#define MSGTR_UsingNINI "NON-INTERLEAVED hibás AVI formátum használata!\n"
#define MSGTR_CouldntDetFNo "Nem tudom meghatározni a képkockák számát (abszolut tekeréshez)   \n"
#define MSGTR_CantSeekRawAVI "Nem tudok nyers .AVI-kban tekerni! (index kell, próbáld az -idx kapcsolóval!)\n"
#define MSGTR_CantSeekFile "Nem tudok ebben a fájlban tekerni!\n"

#define MSGTR_EncryptedVOB "Titkosítótt VOB fájl! Olvasd el a DOCS/HTML/hu/cd-dvd.html fájlt!\n"

#define MSGTR_MOVcomprhdr "MOV: A tömörített fejlécek támogatásához ZLIB kell!\n"
#define MSGTR_MOVvariableFourCC "MOV: Vigyázat: változó FOURCC detektálva!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Vigyázat: túl sok sáv!"
#define MSGTR_FoundAudioStream "==> Megtalált audio folyam: %d\n"
#define MSGTR_FoundVideoStream "==> Megtalált video folyam: %d\n"
#define MSGTR_DetectedTV "TV detektálva! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "OGG demuxer meghívása nem sikerült\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Audio folyam keresése (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Audio folyam megnyitása sikertelen: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Felirat folyam megnyitása sikertelen: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Audio demuxer meghívása sikertelen: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Felirat demuxer meghívása sikertelen: %s\n"
#define MSGTR_TVInputNotSeekable "TV bemenet nem tekerhetõ! (Meg kéne csinálni hogy most váltson csatornát ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "%s demuxer info már jelen van!\n"
#define MSGTR_ClipInfo "Klipp info: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 30000/1001fps NTSC formátumot találtam, frameráta váltás.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 24000/1001fps progresszív NTSC formátumot találtam, frameráta váltás.\n"

#define MSGTR_CacheFill "\rCache feltöltés: %5.2f%% (%"PRId64" bytes)   "
#define MSGTR_NoBindFound "Nincs semmi sem összerendelve a(z) '%s' gombbal"
#define MSGTR_FailedToOpen "Nem lehet megnyitni: %s\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nem tudom megnyitni a kodeket.\n"
#define MSGTR_CantCloseCodec "Nem tudom lezárni a kodeket.\n"

#define MSGTR_MissingDLLcodec "HIBA: Nem tudom megnyitni a kért DirectShow kodeket: %s\n"
#define MSGTR_ACMiniterror "Nem tudom betölteni/inicializálni a Win32/ACM kodeket (hiányzó DLL fájl?)\n"
#define MSGTR_MissingLAVCcodec "Nem találom a(z) '%s' nevû kodeket a libavcodec-ben...\n"

#define MSGTR_MpegNoSequHdr "MPEG: VÉGZETES: vége lett a fájlnak miközben a szekvencia fejlécet kerestem\n"
#define MSGTR_CannotReadMpegSequHdr "VÉGZETES: Nem tudom olvasni a szekvencia fejlécet!\n"
#define MSGTR_CannotReadMpegSequHdrEx "VÉGZETES: Nem tudom olvasni a szekvencia fejléc kiterjesztését!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Hibás szekvencia fejléc!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Hibás szekvencia fejléc kiterjesztés!\n"

#define MSGTR_ShMemAllocFail "Nem tudok megosztott memóriát lefoglalni\n"
#define MSGTR_CantAllocAudioBuf "Nem tudok kimeneti hangbuffer lefoglalni\n"

#define MSGTR_UnknownAudio "Ismeretlen/hiányzó hangformátum, hang kikapcsolva\n"

#define MSGTR_UsingExternalPP "[PP] Külsõ minõségjavító szûrõ használata, max minõség = %d\n"
#define MSGTR_UsingCodecPP "[PP] Codecbeli minõségjavítás használata, max minõség = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "'%s' video tulajdonság nem támogatott a kiválasztott vo & vd meghajtók által!\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "A kért [%s] video codec család (vfm=%s) nem kiválasztható (fordításnál kapcsold be!)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "A kért [%s] audio codec család (afm=%s) nem kiválasztható (fordításnál kapcsold be!)\n"
#define MSGTR_OpeningVideoDecoder "Video dekóder meghívása: [%s] %s\n"
#define MSGTR_SelectedVideoCodec "Kiválasztott videó codec: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "Audio dekóder meghívása: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "Kiválasztott audió codec: [%s] afm: %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "Audió szûrõ lánc felépítése %dHz/%dch/%s -> %dHz/%dch/%s formátumhoz...\n"
#define MSGTR_UninitVideoStr "uninit video: %s\n"
#define MSGTR_UninitAudioStr "uninit audio: %s\n"
#define MSGTR_VDecoderInitFailed "VDecoder init nem sikerült :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder init nem sikerült :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder preinit nem sikerült :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: %d byte allokálása bemeneti buffernek.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: %d + %d = %d byte allokálása bemeneti buffernek.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "LIRC támogatás indítása...\n"
#define MSGTR_LIRCdisabled "Nem fogod tudni használni a távirányítót.\n"
#define MSGTR_LIRCopenfailed "Nem tudtam megnyitni a lirc támogatást!\n"
#define MSGTR_LIRCcfgerr "Nem tudom olvasni a LIRC konfigurációs fájlt: %s \n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nem található a következõ video szûrõ: '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "A következõ video szûrõ megnyitása nem sikerült: '%s'.\n"
#define MSGTR_OpeningVideoFilter "Video szûrõ megnyitása: "
#define MSGTR_CannotFindColorspace "Nem található közös colorspace, még a 'scale' filterrel sem :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: a codec nem állította be az sh->disp_w és az sh_disp_h izéket, megpróbálom workaroundolni!\n"
#define MSGTR_VoConfigRequest "VDec: vo config kérés - %d x %d (preferált színtér: %s)\n"
#define MSGTR_CouldNotFindColorspace "Nem találok egyezõ colorspace-t - újra próbálom a -vf scale filterrel...\n"
#define MSGTR_MovieAspectIsSet "A film aspect értéke %.2f:1 - aspect arány javítása.\n"
#define MSGTR_MovieAspectUndefined "A film aspect értéke nem definiált - nincs arányjavítás.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Frissítened/installálnod kell a bináris codec csomagot.\nItt megtalálod: http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Win32/DShow video codec inicializálása OK.\n"
#define MSGTR_DMOInitOK "INFO: Win32/DMO video codec init OK.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Nem lehet EWMH fullscreen eseményt küldeni!\n"
#define MSGTR_CouldNotFindXScreenSaver "xscreensaver_disable: Nem található az XScreenSaver ablak.\n"
#define MSGTR_SelectedVideoMode "XF86VM: %dx%d kiválasztott videó mód a(z) %dx%d képmérethez.\n"

#define MSGTR_InsertingAfVolume "[Mixer] Nincs hardveres keverés, hangerõ szûrõ használata.\n"
#define MSGTR_NoVolume "[Mixer] Hangerõ állítás nem lehetséges.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Az MPlayerrõl"
#define MSGTR_FileSelect "Fájl kiválasztása..."
#define MSGTR_SubtitleSelect "Felirat kiválasztása..."
#define MSGTR_OtherSelect "Fájl kiválasztása..."
#define MSGTR_AudioFileSelect "Külsõ audio csatorna választása..."
#define MSGTR_FontSelect "Betûtípus kiválasztása..."
// Megjegyzés: Ha megváltoztatod az MSGTR_PlayList-et, nézd meg, hogy megfelel-e az MSGTR_MENU_PlayList-nek is!
#define MSGTR_PlayList "Lejátszási lista"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Skin böngészõ"
#define MSGTR_Network "Hálózati stream-elés..."
// Megjegyzés: Ha megváltoztatod az MSGTR_Preferences-t, nézd meg, hogy megfelel-e az MSGTR_MENU_Preferences-nek is!
#define MSGTR_Preferences "Beállítások"
#define MSGTR_AudioPreferences "Audio vezérlõ beállítása"
#define MSGTR_NoMediaOpened "nincs megnyitva semmi"
#define MSGTR_VCDTrack "%d. VCD track"
#define MSGTR_NoChapter "nincs megnyitott fejezet"
#define MSGTR_Chapter "%d. fejezet"
#define MSGTR_NoFileLoaded "nincs fájl betöltve"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Mégse"
#define MSGTR_Add "Hozzáad"
#define MSGTR_Remove "Kivesz"
#define MSGTR_Clear "Törlés"
#define MSGTR_Config "Beállítás"
#define MSGTR_ConfigDriver "Vezérlõ beállítása"
#define MSGTR_Browse "Tallózás"

// --- error messages ---
#define MSGTR_NEMDB "Nincs elég memória a buffer kirajzolásához."
#define MSGTR_NEMFMR "Nincs elég memória a menü rendereléséhez."
#define MSGTR_IDFGCVD "Nem talaltam GUI kompatibilis video meghajtót."
#define MSGTR_NEEDLAVCFAME "Nem MPEG fájl lejátszása nem lehetséges a DXR3/H+ hardverrel újrakódolás nélkül.\nKapcsold be a lavc vagy fame opciót a DXR3/H+ konfigurációs panelen."
#define MSGTR_UNKNOWNWINDOWTYPE "Ismeretlen ablak típust találtam ..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] hiba a skin konfigurációs fájljának %d. sorában: %s"
#define MSGTR_SKIN_WARNING1 "[skin] figyelmeztetés a skin konfigurációs fájljának %d. sorában: widget (%S) megvan, de nincs elõtte \"section\""
#define MSGTR_SKIN_WARNING2 "[skin] figyelmeztetés a skin konfigurációs fájljának %d. sorában: widget (%S) megvan, de nincs elõtte \"subsection\""
#define MSGTR_SKIN_WARNING3 "[skin] figyelmeztetés a skin konfigurációs fájljának %d. sorában: ez az elem nem használható ebben az alrészben (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[skin] a fájl ( %s ) nem található.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skin] fájl ( %s ) nem olvasható.\n"
#define MSGTR_SKIN_BITMAP_16bit  "16 vagy kevesebb bites bitmap nem támogatott (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "fájl nem található (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP olvasási hiba (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA olvasási hiba (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG olvasási hiba (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE tömörített TGA-k nincsenek támogatva (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "ismeretlen tipusú fájl (%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "hiba a 24-rõl 32 bitre konvertálás közben (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "ismeretlen üzenet: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nincs elég memória\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Túl sok betûtipus van deklarálva.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Nem találom a betûtipus fájlt.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Nem találom a betûtipus képfájlt.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "nemlétezõ betûtipus azonosító (%s)\n"
#define MSGTR_SKIN_UnknownParameter "ismeretlen paraméter (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin nem található (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "A kiválasztott skin ( %s ) nem található, a 'default'-ot próbálom meg...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin konfigurációs fájl olvasási hiba (%s).\n"
#define MSGTR_SKIN_LABEL "Skin-ek:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Az MPlayer-rõl"
#define MSGTR_MENU_Open "Megnyitás..."
#define MSGTR_MENU_PlayFile "Fájl lejátszás..."
#define MSGTR_MENU_PlayVCD "VCD lejátszás..."  
#define MSGTR_MENU_PlayDVD "DVD lejátszás..."  
#define MSGTR_MENU_PlayURL "URL lejátszás..."  
#define MSGTR_MENU_LoadSubtitle "Felirat betöltése..."
#define MSGTR_MENU_DropSubtitle "Felirat eldobása..."
#define MSGTR_MENU_LoadExternAudioFile "Külsõ hang betöltése..."
#define MSGTR_MENU_Playing "Lejátszás"
#define MSGTR_MENU_Play "Lejátszás"
#define MSGTR_MENU_Pause "Pillanatállj"
#define MSGTR_MENU_Stop "Állj"  
#define MSGTR_MENU_NextStream "Következõ fájl"
#define MSGTR_MENU_PrevStream "Elõzõ fájl"
#define MSGTR_MENU_Size "Méret"
#define MSGTR_MENU_HalfSize   "Fél méret"
#define MSGTR_MENU_NormalSize "Normál méret"
#define MSGTR_MENU_DoubleSize "Dupla méret"
#define MSGTR_MENU_FullScreen "Teljesképernyõ" 
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Lemez megnyitása..."
#define MSGTR_MENU_ShowDVDMenu "DVD menû"
#define MSGTR_MENU_Titles "Sávok"
#define MSGTR_MENU_Title "%2d. sáv"
#define MSGTR_MENU_None "(nincs)"
#define MSGTR_MENU_Chapters "Fejezetek"
#define MSGTR_MENU_Chapter "%2d. fejezet"
#define MSGTR_MENU_AudioLanguages "Szinkron nyelvei"
#define MSGTR_MENU_SubtitleLanguages "Feliratok nyelvei"
#define MSGTR_MENU_PlayList MSGTR_PlayList
#define MSGTR_MENU_SkinBrowser "Skin böngészõ"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "Kilépés..."
#define MSGTR_MENU_Mute "Néma"
#define MSGTR_MENU_Original "Eredeti"
#define MSGTR_MENU_AspectRatio "Képarány"
#define MSGTR_MENU_AudioTrack "Audio track"
#define MSGTR_MENU_Track "%d. sáv"
#define MSGTR_MENU_VideoTrack "Video track"

// --- equalizer
// Megjegyzés: Ha megváltoztatod az MSGTR_EQU_Audio-t, nézd meg, hogy megfelel-e az MSGTR_PREFERENCES_Audio-nak is!
#define MSGTR_EQU_Audio "Audió"
// Megjegyzés: Ha megváltoztatod az MSGTR_EQU_Video-t, nézd meg, hogy megfelel-e az MSGTR_PREFERENCES_Video-nak is!
#define MSGTR_EQU_Video "Videó"
#define MSGTR_EQU_Contrast "Kontraszt: "
#define MSGTR_EQU_Brightness "Fényerõ: "
#define MSGTR_EQU_Hue "Szinárnyalat: "
#define MSGTR_EQU_Saturation "Telítettség: "
#define MSGTR_EQU_Front_Left "Bal Elsõ"
#define MSGTR_EQU_Front_Right "Jobb Elsõ"
#define MSGTR_EQU_Back_Left "Bal Hátsó"
#define MSGTR_EQU_Back_Right "Jobb Hátsó"
#define MSGTR_EQU_Center "Középsõ"
#define MSGTR_EQU_Bass "Basszus"
#define MSGTR_EQU_All "Mindegyik"
#define MSGTR_EQU_Channel1 "1. Csatorna:"
#define MSGTR_EQU_Channel2 "2. Csatorna:"
#define MSGTR_EQU_Channel3 "3. Csatorna:"
#define MSGTR_EQU_Channel4 "4. Csatorna:"
#define MSGTR_EQU_Channel5 "5. Csatorna:"
#define MSGTR_EQU_Channel6 "6. Csatorna:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Útvonal"
#define MSGTR_PLAYLIST_Selected "Kiválasztott fájlok"
#define MSGTR_PLAYLIST_Files "Fájlok"
#define MSGTR_PLAYLIST_DirectoryTree "Könyvtár lista"

// --- preferences
#define MSGTR_PREFERENCES_Audio MSGTR_EQU_Audio
#define MSGTR_PREFERENCES_Video MSGTR_EQU_Video
#define MSGTR_PREFERENCES_SubtitleOSD "Felirat & OSD"
#define MSGTR_PREFERENCES_Codecs "Kodekek és demuxerek"
// Megjegyzés: Ha megváltoztatod az MSGTR_PREFERENCES_Misc-et, nézd meg, hogy megfelel-e az MSGTR_PREFERENCES_FRAME_Misc-nek is!
#define MSGTR_PREFERENCES_Misc "Egyéb"

#define MSGTR_PREFERENCES_None "Egyik sem"
#define MSGTR_PREFERENCES_DriverDefault "alapértelmezett vezérlõ"
#define MSGTR_PREFERENCES_AvailableDrivers "Driverek:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Hang nélkül"
#define MSGTR_PREFERENCES_NormalizeSound "Hang normalizálása"
#define MSGTR_PREFERENCES_EnEqualizer "Audio equalizer"
#define MSGTR_PREFERENCES_SoftwareMixer "Szoftveres keverés"
#define MSGTR_PREFERENCES_ExtraStereo "Extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Együttható:"
#define MSGTR_PREFERENCES_AudioDelay "Hang késleltetés"
#define MSGTR_PREFERENCES_DoubleBuffer "Dupla bufferelés"
#define MSGTR_PREFERENCES_DirectRender "Direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Kép eldobás"
#define MSGTR_PREFERENCES_HFrameDrop "Erõszakos kép eldobó"
#define MSGTR_PREFERENCES_Flip "Kép fejjel lefelé"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Óra es indikátorok"
#define MSGTR_PREFERENCES_OSDProgress "Csak a százalék jelzõk"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Óra, százalék és a teljes idõ"
#define MSGTR_PREFERENCES_Subtitle "Felirat:"
#define MSGTR_PREFERENCES_SUB_Delay "Késleltetés: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozíciója: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Felirat automatikus betöltésének tiltása"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode felirat"
#define MSGTR_PREFERENCES_SUB_MPSUB "A film feliratának konvertálása MPlayer felirat formátumba"
#define MSGTR_PREFERENCES_SUB_SRT "A film feliratának konvertálása SubViewer (SRT) formátumba"
#define MSGTR_PREFERENCES_SUB_Overlap "Felirat átlapolás"
#define MSGTR_PREFERENCES_Font "Betûk:"
#define MSGTR_PREFERENCES_FontFactor "Betû együttható:"
#define MSGTR_PREFERENCES_PostProcess "Képjavítás"
#define MSGTR_PREFERENCES_AutoQuality "Autómatikus minõség állítás: "
#define MSGTR_PREFERENCES_NI "non-interleaved  AVI  feltételezése (hibás AVI-knál segíthet"
#define MSGTR_PREFERENCES_IDX "Az AVI indexének újraépítése, ha szükséges"
#define MSGTR_PREFERENCES_VideoCodecFamily "Video kodek család:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Audio kodek család:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD szint"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Felirat"
#define MSGTR_PREFERENCES_FRAME_Font "Betû"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Képjavítás"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Gyorsítótár"
#define MSGTR_PREFERENCES_FRAME_Misc MSGTR_PREFERENCES_Misc
#define MSGTR_PREFERENCES_Audio_Device "Eszköz:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Mixer csatorna:"
#define MSGTR_PREFERENCES_Message "Kérlek emlékezz, néhány opció igényli a lejátszás újraindítását."
#define MSGTR_PREFERENCES_DXR3_VENC "Video kódoló:"
#define MSGTR_PREFERENCES_DXR3_LAVC "LAVC használata (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "FAME használata"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Nyugat-Európai karakterkészlet (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Nyugat-Európai karakterkészlet euróval (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Szláv és közép-európai karakterkészlet (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Eszperantó, gall, máltai és török karakterkészlet (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Régi balti karakterkészlet (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cirill karakterkészlet (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arab karakterkészlet (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Modern görög karakterkészlet (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Török karakterkészlet (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltik karakterkészlet (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Kelta karakterkészlet (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Héber karakterkészlet (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Orosz karakterkészlet (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrán, Belorusz karakterkészlet (KOI8-U/UR)"
#define MSGTR_PREFERENCES_FontEncoding16 "Egyszerû kínai karakterkészlet (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Tradicionális kínai karakterkészlet (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japán karakterkészlet (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Koreai karakterkészlet (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thai karakterkészlet (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cirill karakterkészlet (Windows) (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Szláv és közép-európai karakterkészlet (Windows) (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Nincs automata karakterméret választás"
#define MSGTR_PREFERENCES_FontPropWidth "Karakterméret film szélességéhez való állítása"
#define MSGTR_PREFERENCES_FontPropHeight "Karakterméret film magasságához való állítása"
#define MSGTR_PREFERENCES_FontPropDiagonal "Karakterméret film átlójához való állítása"
#define MSGTR_PREFERENCES_FontEncoding "Kódolás:"
#define MSGTR_PREFERENCES_FontBlur "Blur:"
#define MSGTR_PREFERENCES_FontOutLine "Körvonal:"
#define MSGTR_PREFERENCES_FontTextScale "Szöveg skála:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD skála:"
#define MSGTR_PREFERENCES_Cache "Gyorsítótár be/ki"
#define MSGTR_PREFERENCES_CacheSize "Gyorsítótár merete:"
#define MSGTR_PREFERENCES_LoadFullscreen "Indítás teljes képernyõn"
#define MSGTR_PREFERENCES_SaveWinPos "Ablakok pozíciójának mentése"
#define MSGTR_PREFERENCES_XSCREENSAVER "XScreenSaver leállítása film lejátszásakor"
#define MSGTR_PREFERENCES_PlayBar "PlayBar engedélyezése"
#define MSGTR_PREFERENCES_AutoSync "AutoSync ki/be"
#define MSGTR_PREFERENCES_AutoSyncValue "Értéke:"
#define MSGTR_PREFERENCES_CDROMDevice "CD meghajtó:"
#define MSGTR_PREFERENCES_DVDDevice "DVD meghajtó:"
#define MSGTR_PREFERENCES_FPS "Film FPS:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Lejátszó ablak megjelenítése ha inaktív"
#define MSGTR_PREFERENCES_ArtsBroken "Az újabb aRts verziók inkompatibilisek "\
           "a GTK 1.x-szel és összeomlasztják a GMPlayert!"

#define MSGTR_ABOUT_UHU "GUI fejlesztést az UHU Linux támogatta\n"
#define MSGTR_ABOUT_Contributors "Kód és dokumentáció közremûködõi\n"
#define MSGTR_ABOUT_Codecs_libs_contributions "Codec-ek és third party könyvtárak\n"
#define MSGTR_ABOUT_Translations "Fordítások\n"
#define MSGTR_ABOUT_Skins "Skin-ek\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Végzetes hiba!"
#define MSGTR_MSGBOX_LABEL_Error "Hiba!"
#define MSGTR_MSGBOX_LABEL_Warning "Figyelmeztetés!"

// bitmap.c

#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] nincs elég memória a képhez\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] nincs elég memória a képhez\n"

// cfg.c

#define MSGTR_ConfigFileReadError "[cfg] hiba a konfigurációs fájl olvasásakor ...\n"
#define MSGTR_UnableToSaveOption "[cfg] A(z) '%s' opciót nem sikerült elmenteni.\n"

// interface.c

#define MSGTR_DeletingSubtitles "[GUI] Feliratok törlése.\n"
#define MSGTR_LoadingSubtitles "[GUI] Feliratok betöltése: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] Videó szûrõ hozzáadása: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] Videó szûrõ eltávolítása: %s\n"

// mw.c

#define MSGTR_NotAFile "Úgy tûnik, hogy ez nem fájl: %s !\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] A képernyõ nem nyitható meg.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Távoli képernyõ, XMITSHM kikapcsolva.\n"
#define MSGTR_WS_NoXshm "[ws] Bocs, a rendszered nem támogatja az X osztott memória kiterjesztést.\n"
#define MSGTR_WS_NoXshape "[ws] Bocs, a rendszered nem támogatja az XShape kiterjesztést.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Bocs, a szín mélység túl kicsi.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] Túl sok nyitott ablak van.\n"
#define MSGTR_WS_ShmError "[ws] osztott memória kiterjesztés hibája\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Bocs, nincs elég memória a rajz buffernek.\n"
#define MSGTR_WS_DpmsUnavailable "A DPMS nem elérhetõ?\n"
#define MSGTR_WS_DpmsNotEnabled "A DPMS nem engedélyezhetõ.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "Úgy tûnik, hogy ez nem fájl...\n"
#define MSGTR_WS_DDNothing "D&D: Semmi sem jött vissza!\n"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "A kiválasztott video_out eszköz nem kompatibilis ezzel a codec-kel.\n"\
                "Próbáld meg hozzáadni a scale szûrõt, pl. -vf spp,scale a -vf spp helyett.\n"
#define MSGTR_VO_GenericError "Hiba történt"
#define MSGTR_VO_UnableToAccess "Nem elérhetõ"
#define MSGTR_VO_ExistsButNoDirectory "már létezik, de nem könyvtár."
#define MSGTR_VO_DirExistsButNotWritable "A célkönyvtár már létezik, de nem írható."
#define MSGTR_VO_DirExistsAndIsWritable "A célkönyvtár már létezik és írható."
#define MSGTR_VO_CantCreateDirectory "Nem tudtam létrehozni a célkönyvtárat."
#define MSGTR_VO_CantCreateFile "A kimeneti fájl nem hozható létre."
#define MSGTR_VO_DirectoryCreateSuccess "A célkönyvtárat sikeresen létrehoztam."
#define MSGTR_VO_ParsingSuboptions "Alopciók értelmezése."
#define MSGTR_VO_SuboptionsParsedOK "Alopciók értelmezése rendben."
#define MSGTR_VO_ValueOutOfRange "Érték határon kívül"
#define MSGTR_VO_NoValueSpecified "Nincs érték megadva."
#define MSGTR_VO_UnknownSuboptions "Ismeretlen alopció(k)"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\nEzek az aalib vo_aa alopciói:\n"
#define MSGTR_VO_AA_AdditionalOptions "A vo_aa által biztosított opciók:\n" \
"  help        kiírja ezt a súgót\n" \
"  osdcolor    osd szín beállítása\n  subcolor    feliratszín beállítása\n" \
"        a szín paraméterek:\n           0 : normál\n" \
"           1 : dim\n           2 : félkövér\n           3 : boldfont\n" \
"           4 : fordított\n           5 : speciális\n\n\n"

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "Progresszív JPEG engedélyezve."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "Progresszív JPEG letiltva."
#define MSGTR_VO_JPEG_BaselineJPEG "Baseline JPEG engedélyezve."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Baseline JPEG letiltva."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "ASCII mód engedélyezve."
#define MSGTR_VO_PNM_RawMode "Raw mód engedélyezve."
#define MSGTR_VO_PNM_PPMType "PPM fájlok írása."
#define MSGTR_VO_PNM_PGMType "PGM fájlok írása."
#define MSGTR_VO_PNM_PGMYUVType "PGMYUV fájlok írása."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Az interlaced módhoz néggyel osztható kép magasság szükséges."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Nem sikerült sor buffert foglalni az interlaced módhoz."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Input nem RGB, nem lehet szétválasztani a színeket mezõnként!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "A kép szélességnek kettõvel oszthatónak kell lennie."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Nincs elég memória az RGB framebuffer lefoglalásához."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Nincs elegendõ memória vagy fájl handle a(z) \"%s\" írásához!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Hiba a kép kimenetre írása közben!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Ismeretlen aleszköz: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Interlaced kimeneti mód használata, top-field elõször."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Interlaced kimeneti mód használata, bottom-field elõször."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Progresszív (alapértelmezett) frame mód használata."

// sub.c
#define MSGTR_VO_SUB_Seekbar "Keresõsáv"
#define MSGTR_VO_SUB_Play "Lejátszás"
#define MSGTR_VO_SUB_Pause "Pillanat állj"
#define MSGTR_VO_SUB_Stop "Állj"
#define MSGTR_VO_SUB_Rewind "Vissza"
#define MSGTR_VO_SUB_Forward "Elõre"
#define MSGTR_VO_SUB_Clock "Óra"
#define MSGTR_VO_SUB_Contrast "Kontraszt"
#define MSGTR_VO_SUB_Saturation "Telítettség"
#define MSGTR_VO_SUB_Volume "Hangerõ"
#define MSGTR_VO_SUB_Brightness "Fényerõ"
#define MSGTR_VO_SUB_Hue "Színárnyalat"

// vo_xv.c
#define MSGTR_VO_XV_ImagedimTooHigh "A forrás kép méretei túl nagyok: %ux%u (maximum %ux%u)\n"

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "A pgm video kimeneti vezérlõt lecserélte a -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Az md5 video kimeneti vezérlõt lecserélte a -vo md5sum.\n"

// ======================= AO Audio Output drivers ========================

// libao2 

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: alsa9 és alsa1x modulok törölve lettek, használd a -ao alsa kapcsolót helyettük.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Nem tudom megnyitni a(z) %s keverõ eszközt: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: A hangkártya keverõjének nincs '%s' csatornája, az alapértelmezettet használom.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: A(z) %s audio eszközt nem tudom megnyitni: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Nem lehet fájl leíró blokkolást végezni: %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] A(z) %s audio eszköz nem állítható be %s kimenetre, %s-t próbálok...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Nem sikerült az audio eszközt %d csatornára állítani.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: a vezérlõ nem támogatja a SNDCTL_DSP_GETOSPACE-t :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Az audio vezérlõd NEM támogatja a select() -et ***\n Fordítsd újra az MPlayer-t az #undef HAVE_AUDIO_SELECT sorral a config.h-ban!\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nVégzetes hiba: *** NEM LEHET ÚJRA MEGNYITNI / BEÁLLÍTANI AZ AUDIO ESZKÖZT *** %s\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Csatlakoztam a hang szerverhez.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Nem lehet megnyitni a folyamot.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Folyam megnyitva.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] buffer mérete: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Hangerõ beállítása %d-re sikertelen.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] %d Hz nem támogatott, próbáld a resample-t\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound sikertelen: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] latency: [szerver: %0.2fs, net: %0.2fs] (igazítás %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] nem sikerült megnyitni az ESD playback folyamot: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] DVB audio keverõ beállítása sikertelen: %s.\n" 
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz nem támogatott, új mintavételt próbálok.\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Fájl: %s (%s)\nPCM: Samplerate: %iHz Csatorna: %s Formátum: %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: Gyorsabb dump-olás a -vc null -vo null kapcsolóval érhetõ el\n[AO PCM] Info: WAVE fájlok írásához használd a -ao pcm:waveheader kapcsolót (alapértelmezett)!\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] %s megnyitása írásra nem sikerült!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Samplerate: %iHz Csatornák: %s Formátum: %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] %s audio vezérlõ használata.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Nem támogatott audio formátum: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] SDL Audio inicializálása nem sikerült: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] audio megnyitása nem sikerült: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] vezérlés.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Samplerate: %iHz Csatorna: %s Formátum: %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] lejátszás: hibás eszköz.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparams sikertelen: %s\nNem sikerült beállítani az elõírt samplerate-et.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE-et nem fogadta el a kiválasztott erõforrás.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams sikertelen: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: samplerate most már %lf (elõírt ráta: %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Nem tudom megnyitni az audio csatornát: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO sikertelen.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: írás sikertelen.\n"
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] %s audio eszköz nem elérhetõ, %s  -> nincs hang.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: a kártyád nem támogat %d csatornát, %s, %d Hz samplerate-t.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  A hangkártyád NEM támogatja a select()-et ***\nFordítsd újra az MPlayer-t az #undef HAVE_AUDIO_SELECT sorral a config.h-ban !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nVégzetes hiba: *** NEM LEHET ÚJRA MEGNYITNI / BEÁLLÍTANI AZ AUDIO ESZKÖZT (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: kért formátum: %d Hz, %d csatorna, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: nem találtam hangkártyát.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: hibás formátumot (%s) kértél - kimenet letiltva.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: playback megnyitási hiba: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: pcm info hiba: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: %d hangkártyát találtam, ezt használom: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: pcm csatorna info hiba: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: hiba a paraméterek beállításakor: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: hiba a csatorna beállításakor: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: csatorna elõkészítési hiba: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: lejátszás drain hiba: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: lejátszás ürítési hiba: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: pcm lezárási hiba: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: lejátszás drain hiba: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: lejátszás ürítési hiba: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: csatorna elõkészítési hiba: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: lejátszás drain hiba: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: lejátszás ürítési hiba: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: csatorna elõkészítési hiba: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: alsa underrun, folyam beállítása.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: lejátszás elõkészítési hiba: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: írási hiba a beállítás után: %s - feladom.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: kimeneti hiba: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] hibás plugin: %s\n"

// ======================= AF Audio Filters ================================

// libaf 

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "használható cimkék"
#define MSGTR_AF_LADSPA_WarnNoInputs "FIGYELEM! Ennek a LADSPA pluginnak nincsenek audio bemenetei.\n  A bejövõ audió jelek elvesznek."
#define MSGTR_AF_LADSPA_ErrMultiChannel "A több-csatornás (>2) plugin (még) nem támogatott.\n  Csak a mono és sztereo plugin-okat használd."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Ennek a LADSPA pluginnak nincsenek audió bemenetei."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Különbözik a LADSPA plugin audió bemenetek és kimenetek száma."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "nem sikerült betölteni"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "A ladspa_descriptor() függvény nem található a megadott függvénykönyvtár fájlban."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "A címke nem található a plugin könyvtárban."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Nincs alopció megadva"
#define MSGTR_AF_LADSPA_ErrNoLibFile "Nincs könyvtárfájl megadva"
#define MSGTR_AF_LADSPA_ErrNoLabel "Nincs szûrõ címke megadva"
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Nincs elég vezérlõ megadva a parancssorban"
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: A(z) #%d bemeneti vezérlõ a(z) %0.4f alsó határ alatt van.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: A(z) #%d bemeneti vezérlõ a(z) %0.4f felsõ határ felett van.\n"

// format.c

#define MSGTR_AF_FORMAT_UnknownFormat "ismeretlen formátum "

// ========================== INPUT =========================================

// joystick.c

#define MSGTR_INPUT_JOYSTICK_Opening "Botkormány eszköz megnyitása: %s\n"
#define MSGTR_INPUT_JOYSTICK_CantOpen "Nem sikerült a(z) %s botkormány eszközt megnyitni: %s\n"
#define MSGTR_INPUT_JOYSTICK_ErrReading "Hiba a botkormány eszköz olvasása közben: %s\n"
#define MSGTR_INPUT_JOYSTICK_LoosingBytes "Botkormány: elvesztettünk %d bájtnyi adatot\n"
#define MSGTR_INPUT_JOYSTICK_WarnLostSync "Botkormány: figyelmeztetõ init esemény, elvesztettük a szinkront a vezérlõvel\n"
#define MSGTR_INPUT_JOYSTICK_WarnUnknownEvent "Botkormány ismeretlen figyelmeztetõ esemény típus: %d\n"

// input.c

#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "Túl sok parancs fájl leíró, nem sikerült a(z) %d fájl leíró regisztálása.\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "Túl sok gomb fájl leíró, nem sikerült a(z) %d fájl leíró regisztálása.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "%s parancs: %d argumentum nem egész.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "%s parancs: %d argumentum nem lebegõpontos.\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "%s parancs: %d argumentum lezáratlan.\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "Ismeretlen argumentum: %d\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "A(z) %s parancsnak legalább %d argumentum kell, de csak %d-t találtunk eddig.\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "Hiba a(z) %d parancs fájl leíró olvasása közben: %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "%d parancs fájl leíró buffere tele van: tartalom eldobása\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "Hibás parancs a(z) %s gombnál"
#define MSGTR_INPUT_INPUT_ErrSelect "Kiválasztási hiba: %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "Hiba a(z) %d gomb input fájl leírójában\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "Halott gomb input a(z) %d fájl leírónál\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "Túl sok gomblenyomási esemény egy idõben\n"
#define MSGTR_INPUT_INPUT_ErrOnCmdFd "Hiba a(z) %d parancs fájlleíróban\n"
#define MSGTR_INPUT_INPUT_ErrReadingInputConfig "Hiba a(z) %s input konfigurációs fájl olvasása közben: %s\n"
#define MSGTR_INPUT_INPUT_ErrUnknownKey "Ismeretlen gomb '%s'\n"
#define MSGTR_INPUT_INPUT_ErrUnfinishedBinding "Nem befejezett hozzárendelés: %s\n"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForKeyName "A buffer túl kicsi ehhez a gomb névhez: %s\n"
#define MSGTR_INPUT_INPUT_ErrNoCmdForKey "A(z) %s gombhoz nem található parancs"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForCmd "A buffer túl kicsi a(z) %s parancshoz\n"
#define MSGTR_INPUT_INPUT_ErrWhyHere "Mit keresünk mi itt?\n"
#define MSGTR_INPUT_INPUT_ErrCantInitJoystick "A bemeneti borkormány inicializálása nem sikerült\n"
#define MSGTR_INPUT_INPUT_ErrCantStatFile "Nem stat-olható %s: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantOpenFile "Nem nyitható meg %s: %s\n"

// ========================== LIBMPDEMUX ===================================

// url.c

#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "A karakterlánc már escape-ltnek tûnik az url_escape-ben %c%c1%c2\n"

// ai_alsa1x.c

#define MSGTR_MPDEMUX_AIALSA1X_CannotSetSamplerate "Nem állítható be a mintavételi ráta\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetBufferTime "Nem állítható be a buffer idõ\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetPeriodTime "Nem állítható be a periódus idõ\n"

// ai_alsa1x.c / ai_alsa.c

#define MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig "Hibás konfiguráció ehhez a PCM-hez: nincs elérhetõ konfiguráció\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableAccessType "Elérési típus nem használható\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt "Minta formátum nem elérhetõ\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableChanCount "Csatorna számláló nem elérhetõ - visszatérés az alapértelmezetthez: %d\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams "Sikertelen a hw paraméterek beállítása:"
#define MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize "Nem használható a buffer mérettel egyezõ periódus (%u == %lu)\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallSWParams "Sikertelen az sw paraméterek beállítása:\n"
#define MSGTR_MPDEMUX_AIALSA_ErrorOpeningAudio "Hiba az audió megnyitásakor: %s\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatusError "ALSA státusz hiba: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUN "ALSA xrun!!! (legalább %.3f ms hosszan)\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatus "ALSA Státusz:\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUNPrepareError "ALSA xrun: elõkészítési hiba: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaReadWriteError "ALSA olvasás/írás hiba"

// ai_oss.c

#define MSGTR_MPDEMUX_AIOSS_Unable2SetChanCount "Sikertelen a csatorna számláló beállítása: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetStereo "Sikertelen a sztereó beállítása: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2Open "'%s' nem nyitható meg: %s\n"
#define MSGTR_MPDEMUX_AIOSS_UnsupportedFmt "Nem támogatott formátum\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt "Az audió formátum nem állítható be."
#define MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate "A mintavételi ráta nem állítható be: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger "A trigger nem állítható be: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize "Nem sikerült lekérdezni a blokkméretet!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero "Az audió blokk méret nulla, beállítva: %d!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low "Az audió blokk méret túl kicsi, beállítva: %d!\n"

// asfheader.c

#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB "VÉGZETES HIBA: fejléc méret nagyobb, mint 1 MB (%d)!\nKeresd meg az MPlayer készítõit és töltsd fel/küldd el ezt a fájlt.\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "Nem sikerült %d bájt lefoglalása a fejléchez\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "EOF az asf fejléc olvasása közben, hibás/nem teljes fájl?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "A DVR valószínûleg csak libavformat-tal mûködik, próbáld ki a -demuxer 35 -öt probléma esetén\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "Nincs adat rész a fejléc után!\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF: nem található audió vagy videó fejléc - hibás fájl?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "Hibás hossz az ASF fejlécben!\n"

// asf_mmst_streaming.c

#define MSGTR_MPDEMUX_MMST_WriteError "Írási hiba\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\nRiadó! eof\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "elõ-fejléc olvasás sikertelen\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "Hibás fejléc méret, feladom\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "Fejléc adat olvasási hiba\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "packet_len olvasási hiba\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "Hibás rtsp csomag méret, feladom\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "Parancs adat olvasási hiba\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "Fejléc objektum\n"
#define MSGTR_MPDEMUX_MMST_DataObject "Adat objektum\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "Fájl objektum, csomag méret = %d (%d)\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "Folyam objektum, folyam id: %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "Túl sok id, a folyam figyelmen kívül hagyva"
#define MSGTR_MPDEMUX_MMST_UnknownObject "Ismeretlen objektum\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "Média adat olvasási hiba\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "Hiányzó aláírás\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "Minden kész. Köszönjük, hogy szabadalmazott technológiát alkalmazó médiát töltöttél le.\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "ismeretlen parancs %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "get_media_packet hiba : %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "Csatlakozva\n"

// asf_streaming.c

#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "Ahhhh, stream_chunck méret túl kicsi: %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "size_confirm hibás!: %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "Figyelmeztetés : fejléc eldobva ????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "Hiba a fejléc chunk értelmezésekor\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "Ne a fejléc legyen az elsõ chunk !!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "Hiba, nem lehet allokálni a(z) %d bájtos buffert\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "Hiba a hálózati folyam olvasása közben\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "Hiba, a chunk túl kicsi\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "Hiba, az al-chunk-ok száma hibás\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "Kicsi a sávszélesség, a fájl nem lejátszható!\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "A sávszélesség túl kicsi, audió folyam kikapcsolva\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "A sávszélesség túl kicsi, videó folyam kikapcsolva\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "Hibás hossz az ASF fejlécben!\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "Hiba a fejléc chunk olvasásakor\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "Hiba: chunk_size > packet_size\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "Hiba a chunk olvasása közben\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> ASF Redirector\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "Hibás proxy URL\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "Ismeretlen asf folyam típus\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "Sikertelen a HTTP válasz értelmezése\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "Szerver válasz %d:%s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "ASF HTTP ÉRTELMEZÉSI HIBA : %s pragma elvágva %d bájttól %d bájtig\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "Socket írási hiba : %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "Sikertelen a fájléc értelmezése\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "Nem található folyam\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "Ismeretlen ASF folyam típus\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL: %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "Sikertelen, kilépés\n"

// audio_in.c

#define MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio "\nHiba az audió olvasásakor: %s\n"
#define MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut "Visszatérés a cross-run-ból, néhány képkocka kimaradhatott!\n"
#define MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover "Végzetes hiba, nem lehet visszatérni!\n"
#define MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples "\nNincs elég audió minta!\n"

// aviheader.c

#define MSGTR_MPDEMUX_AVIHDR_EmptyList "** üres lista?!\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundMovieAt "Film megtalálva: 0x%X - 0x%X\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader "'bih' megtalálva, %u bájt %d bájtból\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1 "A kulcs képkocka tábla újragenerálva az M$ mpg4v1 videóhoz\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3 "Kulcs képkocka tábla újragenerálása a DIVX3 videóhoz\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4 "Kulcs képkocka tábla újragenerálása az MPEG4 videóhoz\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt "'wf' megtalálva, %d bájt %d bájtból\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header "AVI: dmlh megtalálva (size=%d) (total_frames=%d)\n"
#define MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames "INDEX blokk olvasása, %d chunk %d képkockához (fpos=%"PRId64")\n"
#define MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr "Kiegészítõ RIFF fejléc...\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** figyelmeztetés: ez nem kiterjesztett AVI fejléc..\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenChunk "Hibás chunk?  chunksize=%d  (id=%.4s)\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI: ODML: odml index felépítése (%d superindexchunks)\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI: ODML: Hibás (nem teljes?) fájlt találtam. Tradícionális index használata\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "A(z) %s index fájl nem olvasható: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "%s nem érvényes MPlayer index fájl\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "Nem lehet memóriát foglalni az index adatoknak %s-bõl\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "Korai index fájlvég %s fájlban\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "Betöltött index fájl: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "Index generálása: %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI: Index tábla legenerálva %d chunk-hoz!\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "Nem sikerült a(z) %s index fájl írása: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "Elmentett index fájl: %s\n"

// cache2.c

#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\rEz a folyam nem cache-elhetõ.\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!! read_filepos különbözik!!! jelentsd ezt a hibát...\n"

// cdda.c

#define MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice "Nem nyitható meg a CDDA eszköz.\n"
#define MSGTR_MPDEMUX_CDDA_CantOpenDisc "Nem nyitható meg a lemez.\n"
#define MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks "Audió CD-t találtam %ld sávval.\n"

// cddb.c

#define MSGTR_MPDEMUX_CDDB_FailedToReadTOC "Hiba a TOC olvasása közben.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToOpenDevice "Hiba a(z) %s eszköz megnyitásakor.\n"
#define MSGTR_MPDEMUX_CDDB_NotAValidURL "Hibás URL\n"
#define MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest "HTTP kérés elküldése nem sikerült.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse "HTTP válasz olvasása nem sikerült.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND "Nem található.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown "Ismeretlen hibakód\n"
#define MSGTR_MPDEMUX_CDDB_NoCacheFound "Nem találtam cache-t.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead "Nem minden xmcd fájl lett elolvasva.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory "Sikertelen a(z) %s könyvtár létrehozása.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten "Nem minden xmcd fájl lett kiírva.\n"
#define MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned "Hibás xmcd adatbázis fájl érkezett vissza.\n"
#define MSGTR_MPDEMUX_CDDB_UnexpectedFIXME "Nem várt FIXME\n"
#define MSGTR_MPDEMUX_CDDB_UnhandledCode "Kezeletlen kód\n"
#define MSGTR_MPDEMUX_CDDB_UnableToFindEOL "Nem található a sor vége\n"
#define MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle "Értelmezés OK, találtam: %s\n"
#define MSGTR_MPDEMUX_CDDB_AlbumNotFound "Album nem található\n"
#define MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr "Szerver válasza: Parancs szintaxis hibás\n"
#define MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable "Nincs elérhetõ oldal információ\n"
#define MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel "Sikertelen a protokol szint lekérdezése\n"
#define MSGTR_MPDEMUX_CDDB_NoCDInDrive "Nincs CD a meghajtóban\n"

// cue_read.c

#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] Nem várt cuefájl sor: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] tesztelt bin fájlnév: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] Nem található a bin fájl - feladom\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] %s bin fájl használata\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] Ismeretlen mód a binfájlhoz. Nem szabadna megtörténnie. Megszakítás.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] %s nem nyitható meg\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] Hiba %s fájlból történõ olvasáskor\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] Hiba a bin fájl méretének lekérdezésekor\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "sáv %02d:  formátum=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] nem várt vége a bin fájlnak\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] Nem olvasható %d bájtnyi payload\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "CUE stream_open, fájlnév=%s, sáv=%d, elérhetõ sávok: %d -> %d\n"

// network.c

#define MSGTR_MPDEMUX_NW_UnknownAF "Ismeretlen címosztály: %d\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "%s feloldása erre: %s...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "Nem oldható fel név %s -hez: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "Csatlakozás a(z) %s[%s] szerverhez: %d...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "Sikertelen csatlakozás a szerverhez %s -sel\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "Kiválasztás sikertelen.\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "Idõtúllépés a csatlakozáskor.\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "getsockopt sikertelen: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "Csatlakozási hiba: %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "Hibás proxy beállítás... Megpróbálom proxy nélkül.\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "Nem oldható fel a távoli hosztnév az AF_INET-hez. Megpróbálom proxy nélkül.\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "Hiba a HTTP kérés küldésekor: nem küldte el az összes kérést.\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "Olvasás sikertelen.\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "http_read_response 0-át olvasott, (pl. EOF)\n"
#define MSGTR_MPDEMUX_NW_AuthFailed "Azonosítás sikertelen. Kérlek használd a -user és -passwd kapcsolókat az\n"\
"azonosító/jelszó megadásához URL listáknál, vagy írd az alábbi formában az URL-t:\n"\
"http://usernev:jelszo@hostnev/fajl\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "Azonosítás szükséges ehhez: %s\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "Azonosítás szükséges.\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "Nincs jelszó megadva, üres jelszót próbálok.\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "Szerver válasz %d: %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "Cache méret beállítva %d KByte-ra\n"
