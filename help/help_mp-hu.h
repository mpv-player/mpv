// Originally translated by: Gabucino the Almighty! <gabucino@mplayerhq.hu>
// Send me money/hw/babes!
//... Okay enough of the hw, now send the other two!
//
// Updated by: Gabrov <gabrov@freemail.hu>
// Sync'ed with help_mp-en.h r22772 (2007. 03. 31.)

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Indítás:   mplayer [opciók] [url|útvonal/]fájlnév\n"
"\n"
"Alapvető opciók: (az összes opció listájához lásd a man lapot!)\n"
" -vo <drv>        kimeneti videó meghajtó kiválasztása (lista: '-vo help')\n"
" -ao <drv>        kimeneti audió meghajtó kiválasztása (lista: '-ao help')\n"
#ifdef HAVE_VCD
" vcd://<sávszám>  lejátszás (S)VCD (Super Video CD) sávból (közvetlen, nincs mount)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titleno>  a megadott DVD sáv lejátszása, fájl helyett\n"
" -alang/-slang    DVD audio/felirat nyelv kiválasztása (2 betűs országkóddal)\n"
#endif
" -ss <pozíció>    a megadott (másodperc v. óra:perc:mperc) pozícióra tekerés\n"
" -nosound         hanglejátszás kikapcsolása\n"
" -fs              teljesképernyős lejátszás (vagy -vm, -zoom, bővebben lásd man)\n"
" -x <x> -y <y>    felbontás beállítása (-vm vagy -zoom használata esetén)\n"
" -sub <fájl>      felhasználandó felirat-fájl megadása (lásd -subfps, -subdelay)\n"
" -playlist <fájl> lejátszási lista fájl megadása\n"
" -vid x -aid y    lejátszandó video- (x) és audio- (y) streamek kiválasztása\n"
" -fps x -srate y  video (x képkocka/mp) és audio (y Hz) ráta megadása\n"
" -pp <minőség>    képjavítás fokozatainak beállítása (lásd a man lapot)\n"
" -framedrop       képkockák eldobásának engedélyezése (lassú gépekhez)\n"
"\n"
"Fontosabb billentyűk: (a teljes lista a man-ban és nézd meg az input.conf fájlt)\n"
" <-  vagy  ->     10 másodperces hátra/előre ugrás\n"
" le vagy fel      1 percnyi hátra/előre ugrás\n"
" pgdown v. pgup   10 percnyi hátra/előre ugrás\n"
" < vagy >         1 fájllal előre/hátra lépés a lejátszási listában\n"
" p vagy SPACE     pillanatállj (bármely billentyűre továbbmegy)\n"
" q vagy ESC       lejátszás vége és kilépés\n"
" + vagy -         audio késleltetése ą 0.1 másodperccel\n"
" o                OSD-mód váltása:  nincs / keresősáv / keresősáv + idő\n"
" * vagy /         hangerő fel/le\n"
" x vagy z         felirat késleltetése ą 0.1 másodperccel\n"
" r vagy t         felirat pozíciójának megváltoztatása, lásd -vf expand-ot is\n"
"\n"
" * * * A MANPAGE TOVÁBBI RÉSZLETEKET, OPCIÓKAT, BILLENTYŰKET TARTALMAZ! * * *\n"
"\n";
#endif

// libmpcodecs/ad_dvdpcm.c:
#define MSGTR_SamplesWanted "Példa fájlokra van szükségünk ilyen formátummal, hogy jobb legyen a támogatása. Ha neked van ilyened, keresd meg a fejlesztőket.\n"

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
#define MSGTR_BuiltinCodecsConf "Befordított codecs.conf használata.\n"
#define MSGTR_CantLoadFont "Nem tudom betölteni a következő bittérképes betűt: %s\n"
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
"  - Adj különböző értékeket az -autosync opciónak, kezdetnek a 30 megteszi.\n"\
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
#define MSGTR_Playing "\n%s lejátszása.\n"
#define MSGTR_NoSound "Audio: nincs hang!!!\n"
#define MSGTR_FPSforced "FPS kényszerítve %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Futásidejű CPU detektálás használata.\n"
#define MSGTR_CompiledWithCPUExtensions "x86-os CPU - a következő kiterjesztésekkel:"
#define MSGTR_AvailableVideoOutputDrivers "Rendelkezésre álló video meghajtók:\n"
#define MSGTR_AvailableAudioOutputDrivers "Rendelkezésre álló audio meghajtók:\n"
#define MSGTR_AvailableAudioCodecs "Rendelkezésre álló audio codec-ek:\n"
#define MSGTR_AvailableVideoCodecs "Rendelkezésre álló video codec-ek:\n"
#define MSGTR_AvailableAudioFm "Rendelkezésre álló (befordított) audio codec családok/meghajtók:\n"
#define MSGTR_AvailableVideoFm "Rendelkezésre álló (befordított) video codec családok/meghajtók:\n"
#define MSGTR_AvailableFsType "A használható teljesképernyős réteg-módok:\n"
#define MSGTR_UsingRTCTiming "Linux hardveres RTC időzítés használata (%ldHz)\n"
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
"  Lehet hogy a futásidejű CPU detektáló kód hibája...\n"\
"  Olvasd el a DOCS/HTML/hu/bugreports.html fájlt!\n"
#define MSGTR_Exit_SIGILL \
"- Az MPlayer egy 'illegális utasítást' hajtott végre.\n"\
"  Ez akkor történik amikor más CPU-n futtatod az MPlayer-t mint amire a\n"\
"  fordítás/optimalizálás történt.\n"\
"  Ellenőrizd!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- Az MPlayer röpke félrelépése miatt hiba lépett fel a CPU/FPU/RAM-ban.\n"\
"  Fordítsd újra az MPlayer-t az --enable-debug opcióval, és készíts egy\n"\
"  'gdb' backtrace-t. Bővebben: DOCS/HTML/hu/bugreports.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- Az MPlayer összeomlott. Ennek nem lenne szabad megtörténnie. Az ok lehet\n"\
"  egy hiba az MPlayer kódjában _vagy_ a Te meghajtóidban, _vagy_ a gcc-ben.\n"\
"  Ha úgy véled hogy ez egy MPlayer hiba, úgy olvasd el a\n"\
"  DOCS/HTML/hu/bugreports.html fájlt és kövesd az utasításait! Nem tudunk\n"\
"  és nem fogunk segíteni, amíg nem szolgálsz megfelelő információkkal a\n"\
"  hiba bejelentésekor.\n"
#define MSGTR_LoadingConfig "'%s' konfiguráció betöltése\n"
#define MSGTR_AddedSubtitleFile "SUB: Felirat fájl (%d) hozzáadva: %s\n"
#define MSGTR_RemovedSubtitleFile "SUB: Felirat fájl (%d) eltávolítva: %s\n"
#define MSGTR_ErrorOpeningOutputFile "Hiba a(z) [%s] fájl írásakor!\n"
#define MSGTR_CommandLine "Parancs sor:"
#define MSGTR_RTCDeviceNotOpenable "%s megnyitása nem sikerült: %s (a felhasználó által olvashatónak kell lennie.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Linux RTC inicializálási hiba az ioctl-ben (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Próbáld ki ezt: \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" hozzáadni a rendszer indító script-jeidhez!\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Linux RTC inicializálási hiba az ioctl-ben (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "%s időzítés használata.\n"
#define MSGTR_NoIdleAndGui "Az -idle opció nem használható a GMPlayerrel.\n"
#define MSGTR_MenuInitialized "Menü inicializálva: %s\n"
#define MSGTR_MenuInitFailed "Menü inicializálás nem sikerült.\n"
#define MSGTR_Getch2InitializedTwice "FIGYELEM: getch2_init kétszer lett meghívva!\n"
#define MSGTR_DumpstreamFdUnavailable "Ezt a folyamot nem lehet dump-olni - a fájlleíró nem elérhető.\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "A libmenu video szűrőt nem sikerült a(z) %s főmenüvel megnyitni.\n"
#define MSGTR_AudioFilterChainPreinitError "Hiba az audio szűrő lánc elő-inicializálásában!\n"
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

#define MSGTR_EdlOutOfMem "Nem lehet elegendő memóriát foglalni az EDL adatoknak.\n"
#define MSGTR_EdlRecordsNo "%d EDL akciók olvasása.\n"
#define MSGTR_EdlQueueEmpty "Nincs olyan EDL akció, amivel foglalkozni kellene.\n"
#define MSGTR_EdlCantOpenForWrite "Az EDL fájlba [%s] nem lehet írni.\n"
#define MSGTR_EdlCantOpenForRead "Az EDL fájlt [%s] nem lehet olvasni.\n"
#define MSGTR_EdlNOsh_video "Az EDL nem használható video nélkül, letiltva.\n"
#define MSGTR_EdlNOValidLine "Hibás EDL sor: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Hibás formátumú EDL sor [%d], kihagyva.\n"
#define MSGTR_EdlBadLineOverlap "Az utolsó megállítási pozíció [%f] volt; a következő indulási [%f]."\
"A bejegyzéseknek időrendben kell lenniük, nem átlapolhatóak. Kihagyva.\n"
#define MSGTR_EdlBadLineBadStop "A megállítási időnek a kezdési idő után kell lennie.\n"
#define MSGTR_EdloutBadStop "EDL skip visszavonva, az utolsó start > stop\n"
#define MSGTR_EdloutStartSkip "EDL skip eleje, nyomd meg az 'i'-t a blokk befejezéséhez.\n"
#define MSGTR_EdloutEndSkip "EDL skip vége, a sor kiírva.\n"
#define MSGTR_MPEndposNoSizeBased "Az MPlayer -endpos opciója jelenleg még nem támogatja a méretbeli megadást.\n"

// mplayer.c OSD

#define MSGTR_OSDenabled "bekapcsolva"
#define MSGTR_OSDdisabled "kikapcsolva"
#define MSGTR_OSDAudio "Audió: %s"
#define MSGTR_OSDVideo "Videó: %s"
#define MSGTR_OSDChannel "Csatorna: %s"
#define MSGTR_OSDSubDelay "Felirat késés: %d ms"
#define MSGTR_OSDSpeed "Sebesség: x %6.2f"
#define MSGTR_OSDosd "OSD: %s"
#define MSGTR_OSDChapter "Fejezet: (%d) %s"

// property values
#define MSGTR_Enabled "bekapcsolva"
#define MSGTR_EnabledEdl "bekapcsolva (EDL)"
#define MSGTR_Disabled "kikapcsolva"
#define MSGTR_HardFrameDrop "erős"
#define MSGTR_Unknown "ismeretlen"
#define MSGTR_Bottom "alul"
#define MSGTR_Center "középen"
#define MSGTR_Top "fent"

// osd bar names
#define MSGTR_Volume "Hangerő"
#define MSGTR_Panscan "Panscan"
#define MSGTR_Gamma "Gamma"
#define MSGTR_Brightness "Fényerő"
#define MSGTR_Contrast "Kontraszt"
#define MSGTR_Saturation "Telítettség"
#define MSGTR_Hue "Árnyalat"

// property state
#define MSGTR_MuteStatus "Némít: %s"
#define MSGTR_AVDelayStatus "A-V késés: %s ms"
#define MSGTR_OnTopStatus "Mindig felül: %s"
#define MSGTR_RootwinStatus "Főablak: %s"
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

#define MSGTR_UsingPass3ControlFile "Pass3 vezérlő fájl használata: %s\n"
#define MSGTR_MissingFilename "\nHiányzó fájlnév!\n\n"
#define MSGTR_CannotOpenFile_Device "Fájl/eszköz megnyitása sikertelen.\n"
#define MSGTR_CannotOpenDemuxer "Demuxer meghívása sikertelen.\n"
#define MSGTR_NoAudioEncoderSelected "\nNem választottál ki audio enkódert (-oac)! Válassz egyet (lásd -oac help), vagy használd a -nosound opciót!\n"
#define MSGTR_NoVideoEncoderSelected "\nNem választottál ki video enkódert (-ovc)! Válassz egyet (lásd -ovc help)!\n"
#define MSGTR_CannotOpenOutputFile "Nem tudom a kimeneti fájlt (%s) megnyitni.\n"
#define MSGTR_EncoderOpenFailed "Enkóder hívása sikertelen.\n"
#define MSGTR_MencoderWrongFormatAVI "\nFIGYELEM: A KIMENETI FÁJL FORMÁTUM _AVI_. Lásd -of help.\n"
#define MSGTR_MencoderWrongFormatMPG "\nFIGYELEM: A KIMENETI FÁJL FORMÁTUM _MPEG_. Lásd -of help.\n"
#define MSGTR_MissingOutputFilename "Nincs kimeneti fájl megadva, lásd a -o kapcsolót."
#define MSGTR_ForcingOutputFourcc "Kimeneti fourcc kényszerítése: %x [%.4s].\n"
#define MSGTR_ForcingOutputAudiofmtTag "Audió formátum tag kényszerítése: 0x%x.\n"
#define MSGTR_DuplicateFrames "\n%d darab képkocka duplázása!!!\n"
#define MSGTR_SkipFrame "\nképkocka átugrása!!!\n"
#define MSGTR_ResolutionDoesntMatch "\nAz új videó fájl felbontása vagy színtere különbözik az előzőétől.\n"
#define MSGTR_FrameCopyFileMismatch "\nAz összes videó fájlnak azonos fps-sel, felbontással, és codec-kel kell rendelkeznie az -ovc copy-hoz.\n"
#define MSGTR_AudioCopyFileMismatch "\nAz összes fájlnak azonos audió codec-kel és formátummal kell rendelkeznie az -oac copy-hoz.\n"
#define MSGTR_NoAudioFileMismatch "\nNem lehet a csak videót tartalmazó fájlokat összekeverni audió és videó fájlokkal. Próbáld a -nosound kapcsolót.\n"
#define MSGTR_NoSpeedWithFrameCopy "FIGYELEM: A -speed nem biztos, hogy jól működik az -oac copy-val!\n"\
"A kódolásod hibás lehet!\n"
#define MSGTR_ErrorWritingFile "%s: hiba a fájl írásánál.\n"
#define MSGTR_FlushingVideoFrames "\nVideó kockák ürítése.\n"
#define MSGTR_FiltersHaveNotBeenConfiguredEmptyFile "A szűrők nincsenek konfigurálva! Üres fájl?\n"
#define MSGTR_RecommendedVideoBitrate "Ajánlott video bitráta %s CD-hez: %d\n"
#define MSGTR_VideoStreamResult "\nVideo stream: %8.3f kbit/mp  (%d B/s)  méret: %"PRIu64" byte  %5.3f mp  %d képkocka\n"
#define MSGTR_AudioStreamResult "\nAudio stream: %8.3f kbit/mp  (%d B/s)  méret: %"PRIu64" byte  %5.3f mp\n"
#define MSGTR_EdlSkipStartEndCurrent "EDL SKIP: Kezdete: %.2f  Vége: %.2f   Aktuális: V: %.2f  A: %.2f     \r"
#define MSGTR_OpenedStream "sikeres: formátum: %d  adat: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videocodec: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "audiocodec: framecopy (formátum=%x csati=%d ráta=%d bit=%d B/s=%d sample-%d)\n"
#define MSGTR_CBRPCMAudioSelected "CBR PCM audió kiválasztva.\n"
#define MSGTR_MP3AudioSelected "MP3 audió kiválasztva.\n"
#define MSGTR_CannotAllocateBytes "%d byte nem foglalható le.\n"
#define MSGTR_SettingAudioDelay "Audió késleltetés beállítása: %5.3fs.\n"
#define MSGTR_SettingVideoDelay "Videó késleltetés beállítása: %5.3fs.\n"
#define MSGTR_SettingAudioInputGain "Audió input erősítése %f.\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Audió előretöltés korlátozva 0.4 mp-re.\n"
#define MSGTR_IncreasingAudioDensity "Audió tömörítés növelése 4-re.\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Audió előretöltés 0-ra állítva, max pts javítás 0.\n"
#define MSGTR_CBRAudioByterate "\n\nCBR audio: %d byte/mp, %d byte/blokk\n"
#define MSGTR_LameVersion "LAME %s (%s) verzió\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Hiba: A megadott bitráta az ezen beállításhoz tartozó határokon kívül van.\n"\
"\n"\
"Ha ezt a módot használod, \"8\" és \"320\" közötti értéket kell megadnod.\n"\
"\n"\
"További információkért lásd a \"-lameopts preset=help\" kapcsolót!\n"
#define MSGTR_InvalidLamePresetOptions "Hiba: Nem adtál meg érvényes profilt és/vagy opciókat a preset-tel.\n"\
"\n"\
"Az elérhető profilok:\n"\
"\n"\
"   <fast>        alapértelmezett\n"\
"   <fast>        extrém\n"\
"                 őrült\n"\
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
"A preset kapcsolók azért lettek létrehozva, hogy a lehető legjobb minőséget biztosítsák.\n"\
"\n"\
"Legtöbbször elvakult, könyörtelen vájtfülűek tárgyalják ki és állítgatják őket,\n"\
"hogy elérjék a céljaikat.\n"\
"\n"\
"Ezen változtatások folyamatosan frissítésre kerülnek, hogy illeszkedjenek a\n"\
"legújabb fejlesztésekhez és az eredmény majdnem a legjobb minőséget biztosítsa\n"\
"Neked, ami elérhető a LAME-mel.\n"\
"\n"\
"Preset-ek aktiválása:\n"\
"\n"\
"   VBR (változó bitráta) módokhoz (általában a legjobb minőség):\n"\
"\n"\
"     \"preset=standard\" Ez a beállítás ajánlott a legtöbb felhasználónak\n"\
"                             a zenék nagy részénél, és már ez is megfelelően\n"\
"                             jó minőséget biztosít.\n"\
"\n"\
"     \"preset=extreme\" Ha különösen jó hallásod és hasonlóan jó felszerelésed\n"\
"                             van, ez a beállítás meglehetősen jobb minőséget\n"\
"                             fog biztosítani mint a \"standard\" mód.\n"\
"                             \n"\
"\n"\
"   CBR (állandó bitráta) 320kbps (a preset-tel elérhető legjobb minőség):\n"\
"\n"\
"     \"preset=insane\"  Ez a beállítás \"ágyuval verébre\" eset a legtöbb\n"\
"                             embernél és a legtöbb esetben, de ha abszolút a\n"\
"                             legjobb minőségre van szükséged a fájl méretétől\n"\
"                             függetlenül, akkor ez kell neked.\n"\
"\n"\
"   ABR (átlagos bitráta) mód (kiváló minőség az adott bitrátához de nem VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Ezen preset használatával legtöbbször jó minőséget\n"\
"                             kapsz a megadott bitrátával. Az adott bitrátától\n"\
"                             függően ez a preset meghatározza az optimális\n"\
"                             beállításokat.\n"\
"                             Amíg ez a megközelítés működik, messze nem olyan\n"\
"                             rugalmas, mint a VBR, és legtöbbször nem lesz\n"\
"                             olyan minőségű, mint a magas bitrátájú VBR-rel.\n"\
"\n"\
"A következő opciók is elérhetőek a megfelelő profilokhoz:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extrém\n"\
"                 őrült\n"\
"   <cbr> (ABR mód) - Az ABR mód beépített. Használatához egyszerűen\n"\
"                     csak add meg a bitrátát. Például:\n"\
"                     \"preset=185\" ezt a preset-et aktiválja\n"\
"                     és 185-ös átlagos kbps-t használ.\n"\
"\n"\
"   \"fast\" - Engedélyezi az új, gyors VBR-t a megfelelő profilban.\n"\
"            Hátránya, hogy a sebesség miatt a bitráta gyakran \n"\
"            kicsit nagyobb lesz, mint a normál módban, a minőség pedig\n"\
"            kicsit rosszabb.\n"\
"   Figyelem: a jelenlegi állapotban a gyors preset-ek túl nagy bitrátát\n"\
"             produkálnak a normális preset-hez képest.\n"\
"\n"\
"   \"cbr\"  - Ha az ABR módot használod (lásd feljebb) egy olyan bitrátával\n"\
"            mint a 80, 96, 112, 128, 160, 192, 224, 256, 320, használhatod\n"\
"            a \"cbr\" opciót hogy előírd a CBR módot a kódolásnál az\n"\
"            alapértelmezett abr mód helyett. Az ABR jobb minőséget biztosít,\n"\
"            de a CBR hasznosabb lehet olyan esetekben, mint pl. amikor fontos\n"\
"            az MP3 Interneten történő streamelhetősége.\n"\
"\n"\
"    Például:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Pár álnév, ami elérhető az ABR módban:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit \
"A Lame opciók nem állíthatók be, ellenőrizd a bitrátát/mintavételi rátát, néhány\n"\
"alacsony bitrátához (<32) alacsonyabb mintavételi ráta kell (pl. -srate 8000).\n"\
"Ha minden más sikertelen, próbálj ki egy preset-et."
#define MSGTR_ConfigFileError "konfigurációs fájl hibája"
#define MSGTR_ErrorParsingCommandLine "hiba a parancssor értelmezésekor"
#define MSGTR_VideoStreamRequired "Video stream szükséges!\n"
#define MSGTR_ForcingInputFPS "Az input fps inkább %5.2f-ként lesz értelmezve.\n"
#define MSGTR_RawvideoDoesNotSupportAudio "A RAWVIDEO kimeneti fájl formátum nem támogatja a hangot - audió letiltva.\n"
#define MSGTR_DemuxerDoesntSupportNosound "Ez a demuxer még nem támogatja a -nosound kapcsolót.\n"
#define MSGTR_MemAllocFailed "Nem sikerült a memóriafoglalás.\n"
#define MSGTR_NoMatchingFilter "Nem találtam megfelelő szűrőt/ao formátumot!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, talán hibás C fordító?\n"
#define MSGTR_NoLavcAudioCodecName "Audió LAVC, hiányzó codec név!\n"
#define MSGTR_LavcAudioCodecNotFound "Audió LAVC, nem található kódoló a(z) %s codec-hez.\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, nem található a kontextus!\n"
#define MSGTR_CouldntOpenCodec "A(z) %s codec nem nyitható meg, br=%d.\n"
#define MSGTR_CantCopyAudioFormat "A(z) 0x%x audió formátum nem kompatibilis a '-oac copy'-val, kérlek próbáld meg a '-oac pcm' helyette vagy használd a '-fafmttag'-ot a felülbírálásához.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     a változó bitrátájú kódolás módja\n"\
"                0: cbr (konstans bitráta)\n"\
"                1: mt (Mark Taylor VBR algoritmus)\n"\
"                2: rh (Robert Hegemann VBR algoritmus - alapértelmezett)\n"\
"                3: abr (átlagos bitráta)\n"\
"                4: mtrh (Mark Taylor Robert Hegemann VBR algoritmus)\n"\
"\n"\
" abr           átlagos bitráta\n"\
"\n"\
" cbr           konstans bitráta\n"\
"               Előírja a CBR módú kódolást a későbbi ABR módokban is.\n"\
"\n"\
" br=<0-1024>   bitráta kBit-ben (csak CBR és ABR)\n"\
"\n"\
" q=<0-9>       minőség (0-legjobb, 9-legrosszabb) (csak VBR-nél)\n"\
"\n"\
" aq=<0-9>      algoritmikus minőség (0-legjobb, 9-legrosszabb/leggyorsabb)\n"\
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
" fast          valamivel gyorsabb VBR kódolás, kicsit rosszabb minőség és\n"\
"               magasabb bitráta.\n"\
"\n"\
" preset=<érték> A lehető legjobb minőséget biztosítja.\n"\
"                 medium: VBR  kódolás,  kellemes minőség\n"\
"                 (150-180 kbps bitráta tartomány)\n"\
"                 standard:  VBR kódolás, jó minőség\n"\
"                 (170-210 kbps bitráta tartomány)\n"\
"                 extreme: VBR kódolás, nagyon jó minőség\n"\
"                 (200-240 kbps bitráta tartomány)\n"\
"                 insane:  CBR  kódolás, legjobb minőség\n"\
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
#define MSGTR_CodecLacksDriver "\na codec(%s) nem rendelkezik vezélővel!\n"
#define MSGTR_CodecNeedsDLL "\na codec(%s) 'dll'-t igényel!\n"
#define MSGTR_CodecNeedsOutfmt "\ncodec(%s) 'outfmt'-t igényel!\n"
#define MSGTR_CantAllocateComment "Nem tudok memóriát foglalni a megjegyzésnek. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "%s olvasása: "
#define MSGTR_CantOpenFileError "Nem tudom megnyitni '%s'-t: %s\n"
#define MSGTR_CantGetMemoryForLine "Nem tudok memóriát foglalni a 'line'-nak: %s\n"
#define MSGTR_CantReallocCodecsp "A '*codecsp' nem foglalható le újra: %s\n"
#define MSGTR_CodecNameNotUnique "A(z) '%s' codec név nem egyedi."
#define MSGTR_CantStrdupName "Nem végezhető el: strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Nem végezhető el: strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Nem végezhető el: strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Nem végezhető el: strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d audió & %d videó codec\n"
#define MSGTR_CodecDefinitionIncorrect "A codec nincs megfelelően definiálva."
#define MSGTR_OutdatedCodecsConf "Ez a codecs.conf túl régi és nem kompatibilis az MPlayer ezen kiadásával!"

// fifo.c
#define MSGTR_CannotMakePipe "Nem hozható létre PIPE!\n"

// parser-mecmd.c, parser-mpcmd.c
#define MSGTR_NoFileGivenOnCommandLine "'--' azt jelöli, hogy nincs több opció, de nincs fájlnév megadva a parancssorban.\n"
#define MSGTR_TheLoopOptionMustBeAnInteger "A loop opciónak egésznek kell lennie: %s\n"
#define MSGTR_UnknownOptionOnCommandLine "Ismeretlen opció a parancssorban: -%s\n"
#define MSGTR_ErrorParsingOptionOnCommandLine "Hiba a parancssorban megadott opció értelmezésekor: -%s\n"
#define MSGTR_InvalidPlayEntry "Hibás lejátszási bejegyzés: %s\n"
#define MSGTR_NotAnMEncoderOption "-%s nem MEncoder opció\n"
#define MSGTR_NoFileGiven "Nincs megadva fájl\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "A talált mentési slot a(z) %d lvl-ből túl régi: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "A(z) %s kapcsoló nem használható konfigurációs fájlban.\n"
#define MSGTR_InvalidCmdlineOption "A(z) %s kapcsoló nem használható parancssorból.\n"
#define MSGTR_InvalidSuboption "Hiba: '%s' kapcsolónak nincs '%s' alopciója.\n"
#define MSGTR_MissingSuboptionParameter "Hiba: a(z) '%s' '%s' alkapcsolójához paraméter kell!\n"
#define MSGTR_MissingOptionParameter "Hiba: a(z) '%s' kapcsolóhoz kell egy paraméter!\n"
#define MSGTR_OptionListHeader "\n Név                  Típus           Min        Max      Globál  CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nÖsszesen: %d kapcsoló\n"
#define MSGTR_ProfileInclusionTooDeep "FIGYELMEZTETÉS: Túl mély profil beágyazás.\n"
#define MSGTR_NoProfileDefined "Nincs profil megadva.\n"
#define MSGTR_AvailableProfiles "Elérhető profilok:\n"
#define MSGTR_UnknownProfile "Ismeretlen profil: '%s'.\n"
#define MSGTR_Profile "Profil %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n Név                  Típus           Min        Max\n\n"
#define MSGTR_TotalProperties "\nÖsszesen: %d tulajdonság\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "A CD-ROM meghajtó (%s) nem található!\n"
#define MSGTR_ErrTrackSelect "Hiba a VCD-sáv kiválasztásakor!"
#define MSGTR_ReadSTDIN "Olvasás a szabványos bemenetről (stdin)...\n"
#define MSGTR_UnableOpenURL "Nem megnyitható az URL: %s\n"
#define MSGTR_ConnToServer "Csatlakozom a szerverhez: %s\n"
#define MSGTR_FileNotFound "A fájl nem található: '%s'\n"

#define MSGTR_SMBInitError "Samba kliens könyvtár nem inicializálható: %d\n"
#define MSGTR_SMBFileNotFound "Nem nyitható meg a hálózatról: '%s'\n"
#define MSGTR_SMBNotCompiled "Nincs befordítva az MPlayerbe az SMB támogatás\n"

#define MSGTR_CantOpenDVD "Nem tudom megnyitni a DVD eszközt: %s\n"

// stream_dvd.c
#define MSGTR_DVDspeedCantOpen "Nem nyitható meg a DVD eszköz írásra, a DVD sebesség változtatásához írási jog kell.\n"
#define MSGTR_DVDrestoreSpeed "DVD sebesség visszaállítása... "
#define MSGTR_DVDlimitSpeed "DVD sebesség korlátozása %dKB/s-ra... "
#define MSGTR_DVDlimitFail "sikertelen\n"
#define MSGTR_DVDlimitOk "sikeres\n"
#define MSGTR_NoDVDSupport "Az MPlayer DVD támogatás nélkül lett lefordítva, kilépés.\n"
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
#define MSGTR_DVDnoMatchingAudio "Nem található megfelelő nyelvű DVD audió!\n"
#define MSGTR_DVDaudioChannel "Kiválasztott DVD audió csatorna: %d nyelv: %c%c\n"
#define MSGTR_DVDaudioStreamInfo "audió folyam: %d formátum: %s (%s) nyelv: %s aid: %d.\n"
#define MSGTR_DVDnumAudioChannels "audió csatornák száma a lemezen: %d.\n"
#define MSGTR_DVDnoMatchingSubtitle "Nincs megfelelő nyelvű DVD felirat fájl!\n"
#define MSGTR_DVDsubtitleChannel "Kiválasztott DVD felirat csatorna: %d nyelv: %c%c\n"
#define MSGTR_DVDsubtitleLanguage "felirat ( sid ): %d nyelv: %s\n"
#define MSGTR_DVDnumSubtitles "feliratok szám a lemezen: %d\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "Túl sok stream!"
#define MSGTR_RawMuxerOnlyOneStream "A rawaudio muxer csak egy audió folyamot támogat!\n"
#define MSGTR_IgnoringVideoStream "Videó folyam figyelmen kívül hagyva!\n"
#define MSGTR_UnknownStreamType "Figyelem! Ismeretlen folyam típus: %d.\n"
#define MSGTR_WarningLenIsntDivisible "Figyelem! A len nem osztható a samplesize-zal!\n"
#define MSGTR_MuxbufMallocErr "Muxer kocka buffernek nem sikerült memóriát foglalni!\n"
#define MSGTR_MuxbufReallocErr "Muxer kocka buffernek nem sikerült memóriát újrafoglalni!\n"
#define MSGTR_MuxbufSending "Muxer kocka bufferből %d kocka átküldve a muxer-nek.\n"
#define MSGTR_WritingHeader "Fejléc írása...\n"
#define MSGTR_WritingTrailer "Index írása...\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Vigyázat! Többszörösen definiált Audio-folyam: %d (Hibás fájl?)\n"
#define MSGTR_VideoStreamRedefined "Vigyázat! Többszörösen definiált Video-folyam: %d (Hibás fájl?)\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) audio-csomag a pufferben!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) video-csomag a pufferben!\n"
#define MSGTR_MaybeNI "Talán ez egy nem összefésült (interleaved) fájl vagy a codec nem működik jól?\n" \
		      "AVI fájloknál próbáld meg a non-interleaved mód kényszerítését a -ni opcióval.\n"
#define MSGTR_WorkAroundBlockAlignHeaderBug "AVI: CBR-MP3 nBlockAlign fejléc hiba megkerülése!\n"
#define MSGTR_SwitchToNi "\nRosszul összefésült (interleaved) fájl, átváltás -ni módba!\n"
#define MSGTR_InvalidAudioStreamNosound "AVI: hibás audió folyam ID: %d - figyelmen kívül hagyva (nosound)\n"
#define MSGTR_InvalidAudioStreamUsingDefault "AVI: hibás videó folyam ID: %d - figyelmen kívül hagyva (alapértelmezett használata)\n"
#define MSGTR_ON2AviFormat "ON2 AVI formátum"
#define MSGTR_Detected_XXX_FileFormat "Ez egy %s formátumú fájl!\n"
#define MSGTR_DetectedAudiofile "Audio fájl detektálva!\n"
#define MSGTR_NotSystemStream "Nem MPEG System Stream formátum... (talán Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Hibás MPEG-ES-folyam? Lépj kapcsolatba a készítőkkel, lehet, hogy hiba!\n"
#define MSGTR_FormatNotRecognized "========= Sajnos ez a fájlformátum ismeretlen vagy nem támogatott ===========\n"\
				  "= Ha ez egy AVI, ASF vagy MPEG fájl, lépj kapcsolatba a készítőkkel (hiba)! =\n"
#define MSGTR_SettingProcessPriority "Folyamat priorításának beállítása: %s\n"
#define MSGTR_FilefmtFourccSizeFpsFtime "[V] filefmt:%d  fourcc:0x%X  méret:%dx%d  fps:%5.2f  ftime:=%6.4f\n"
#define MSGTR_CannotInitializeMuxer "A muxer nem inicializálható."
#define MSGTR_MissingVideoStream "Nincs képfolyam!\n"
#define MSGTR_MissingAudioStream "Nincs hangfolyam... -> hang nélkül\n"
#define MSGTR_MissingVideoStreamBug "Nincs képfolyam?! Írj a szerzőnek, lehet hogy hiba :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: a fájl nem tartalmazza a kért hang vagy kép folyamot\n"

#define MSGTR_NI_Forced "Kényszerítve"
#define MSGTR_NI_Detected "Detektálva"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI formátum!\n"

#define MSGTR_UsingNINI "NON-INTERLEAVED hibás AVI formátum használata!\n"
#define MSGTR_CouldntDetFNo "Nem tudom meghatározni a képkockák számát (abszolut tekeréshez)   \n"
#define MSGTR_CantSeekRawAVI "Nem tudok nyers .AVI-kban tekerni! (index kell, próbáld az -idx kapcsolóval!)\n"
#define MSGTR_CantSeekFile "Nem tudok ebben a fájlban tekerni!\n"

#define MSGTR_MOVcomprhdr "MOV: A tömörített fejlécek támogatásához ZLIB kell!\n"
#define MSGTR_MOVvariableFourCC "MOV: Vigyázat: változó FourCC detektálva!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Vigyázat: túl sok sáv!"
#define MSGTR_FoundAudioStream "==> Megtalált audio folyam: %d\n"
#define MSGTR_FoundVideoStream "==> Megtalált video folyam: %d\n"
#define MSGTR_DetectedTV "TV detektálva! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Ogg demuxer meghívása nem sikerült.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Audio folyam keresése (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Audio folyam megnyitása sikertelen: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Felirat folyam megnyitása sikertelen: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Audio demuxer meghívása sikertelen: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Felirat demuxer meghívása sikertelen: %s\n"
#define MSGTR_TVInputNotSeekable "TV bemenet nem tekerhető! (Meg kéne csinálni hogy most váltson csatornát ;)\n"
#define MSGTR_DemuxerInfoChanged "%s demuxer infó megváltozott erre: %s\n"
#define MSGTR_ClipInfo "Klipp info: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 30000/1001fps NTSC formátumot találtam, frameráta váltás.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 24000/1001fps progresszív NTSC formátumot találtam, frameráta váltás.\n"

#define MSGTR_CacheFill "\rCache feltöltés: %5.2f%% (%"PRId64" bytes)   "
#define MSGTR_NoBindFound "Nincs semmi sem összerendelve a(z) '%s' gombbal."
#define MSGTR_FailedToOpen "Nem lehet megnyitni: %s.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nem tudom megnyitni a codec-et.\n"
#define MSGTR_CantCloseCodec "Nem tudom lezárni a codec-et.\n"

#define MSGTR_MissingDLLcodec "HIBA: Nem tudom megnyitni a kért DirectShow codec-et: %s\n"
#define MSGTR_ACMiniterror "Nem tudom betölteni/inicializálni a Win32/ACM codec-et (hiányzó DLL fájl?).\n"
#define MSGTR_MissingLAVCcodec "Nem találom a(z) '%s' nevű kodeket a libavcodec-ben...\n"

#define MSGTR_MpegNoSequHdr "MPEG: VÉGZETES: vége lett a fájlnak miközben a szekvencia fejlécet kerestem\n"
#define MSGTR_CannotReadMpegSequHdr "VÉGZETES: Nem tudom olvasni a szekvencia fejlécet!\n"
#define MSGTR_CannotReadMpegSequHdrEx "VÉGZETES: Nem tudom olvasni a szekvencia fejléc kiterjesztését!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Hibás szekvencia fejléc!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Hibás szekvencia fejléc kiterjesztés!\n"

#define MSGTR_ShMemAllocFail "Nem tudok megosztott memóriát lefoglalni\n"
#define MSGTR_CantAllocAudioBuf "Nem tudok kimeneti hangbuffer lefoglalni\n"

#define MSGTR_UnknownAudio "Ismeretlen/hiányzó hangformátum, hang kikapcsolva\n"

#define MSGTR_UsingExternalPP "[PP] Külső minőségjavító szűrő használata, max minőség = %d\n"
#define MSGTR_UsingCodecPP "[PP] Codecbeli minőségjavítás használata, max minőség = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "'%s' video tulajdonság nem támogatott a kiválasztott vo & vd meghajtók által!\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "A kért [%s] video codec család (vfm=%s) nem kiválasztható (fordításnál kapcsold be!)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "A kért [%s] audio codec család (afm=%s) nem kiválasztható (fordításnál kapcsold be!)\n"
#define MSGTR_OpeningVideoDecoder "Video dekóder meghívása: [%s] %s\n"
#define MSGTR_SelectedVideoCodec "Kiválasztott videó codec: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "Audio dekóder meghívása: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "Kiválasztott audió codec: [%s] afm: %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "Audió szűrő lánc felépítése %dHz/%dch/%s -> %dHz/%dch/%s formátumhoz...\n"
#define MSGTR_UninitVideoStr "Videó uninit: %s\n"
#define MSGTR_UninitAudioStr "Audió uninit: %s\n"
#define MSGTR_VDecoderInitFailed "VDecoder init nem sikerült :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder init nem sikerült :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder preinit nem sikerült :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: %d byte allokálása bemeneti buffernek.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: %d + %d = %d byte allokálása bemeneti buffernek.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "LIRC támogatás indítása...\n"
#define MSGTR_LIRCopenfailed "Nem tudtam megnyitni a lirc támogatást. Nem fogod tudni használni a távirányítót.\n"
#define MSGTR_LIRCcfgerr "Nem tudom olvasni a LIRC konfigurációs fájlt: %s \n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nem található a következő video szűrő: '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "A következő video szűrő megnyitása nem sikerült: '%s'.\n"
#define MSGTR_OpeningVideoFilter "Video szűrő megnyitása: "
#define MSGTR_CannotFindColorspace "Nem található közös colorspace, még a 'scale' filterrel sem :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: a codec nem állította be az sh->disp_w és az sh_disp_h izéket, megpróbálom workaroundolni!\n"
#define MSGTR_VoConfigRequest "VDec: vo config kérés - %d x %d (preferált színtér: %s)\n"
#define MSGTR_UsingXAsOutputCspNoY "VDec: %s használata kimeneti színtérként (nincs %d)\n"
#define MSGTR_CouldNotFindColorspace "Nem találok egyező colorspace-t - újra próbálom a -vf scale filterrel...\n"
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

#define MSGTR_InsertingAfVolume "[Mixer] Nincs hardveres keverés, hangerő szűrő használata.\n"
#define MSGTR_NoVolume "[Mixer] Hangerő állítás nem lehetséges.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Az MPlayerről"
#define MSGTR_FileSelect "Fájl kiválasztása..."
#define MSGTR_SubtitleSelect "Felirat kiválasztása..."
#define MSGTR_OtherSelect "Fájl kiválasztása..."
#define MSGTR_AudioFileSelect "Külső audio csatorna választása..."
#define MSGTR_FontSelect "Betűtípus kiválasztása..."
// Megjegyzés: Ha megváltoztatod az MSGTR_PlayList-et, nézd meg, hogy megfelel-e az MSGTR_MENU_PlayList-nek is!
#define MSGTR_PlayList "Lejátszási lista"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_ConfigureEqualizer "Equalizer beállítása"
#define MSGTR_SkinBrowser "Skin böngésző"
#define MSGTR_Network "Hálózati stream-elés..."
// Megjegyzés: Ha megváltoztatod az MSGTR_Preferences-t, nézd meg, hogy megfelel-e az MSGTR_MENU_Preferences-nek is!
#define MSGTR_Preferences "Beállítások"
#define MSGTR_AudioPreferences "Audio vezérlő beállítása"
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
#define MSGTR_ConfigDriver "Vezérlő beállítása"
#define MSGTR_Browse "Tallózás"

// --- error messages ---
#define MSGTR_NEMDB "Nincs elég memória a rajzoló bufferhez."
#define MSGTR_NEMFMR "Nincs elég memória a menü rendereléséhez."
#define MSGTR_IDFGCVD "Nem találtam GUI-kompatibilis videó meghajtót."
#define MSGTR_NEEDLAVC "Nem MPEG fájl lejátszása nem lehetséges a DXR3/H+ hardverrel újrakódolás nélkül.\nKapcsold be a lavc opciót a DXR3/H+ konfigurációs panelen."
#define MSGTR_UNKNOWNWINDOWTYPE "Ismeretlen ablak típust találtam ..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] hiba a skin konfigurációs fájljának %d. sorában: %s"
#define MSGTR_SKIN_WARNING1 "[skin] figyelmeztetés: a skin konfigurációs fájljának %d. sorában: widget (%s) megvan, de nincs előtte \"section\""
#define MSGTR_SKIN_WARNING2 "[skin] figyelmeztetés: a skin konfigurációs fájljának %d. sorában: widget (%s) megvan, de nincs előtte \"subsection\""
#define MSGTR_SKIN_WARNING3 "[skin] figyelmeztetés: a skin konfigurációs fájljának %d. sorában: ez az elem nem használható ebben az alrészben (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[skin] a fájl ( %s ) nem található.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skin] fájl ( %s ) nem olvasható.\n"
#define MSGTR_SKIN_BITMAP_16bit  "16 vagy kevesebb bites bitmap nem támogatott (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "A fájl nem található (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP olvasási hiba (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA olvasási hiba (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG olvasási hiba (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE tömörített TGA-k nincsenek támogatva (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "ismeretlen tipusú fájl (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "hiba a 24-ről 32 bitre konvertálás közben (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "ismeretlen üzenet: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nincs elég memória\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Túl sok betűtipus van deklarálva.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Nem találom a betűtipus fájlt.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Nem találom a betűtipus képfájlt.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "nemlétező betűtipus azonosító (%s)\n"
#define MSGTR_SKIN_UnknownParameter "ismeretlen paraméter (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin nem található (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "A kiválasztott skin ( %s ) nem található, a 'default'-ot próbálom meg...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin konfigurációs fájl olvasási hiba (%s).\n"
#define MSGTR_SKIN_LABEL "Skin-ek:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Az MPlayer-ről"
#define MSGTR_MENU_Open "Megnyitás..."
#define MSGTR_MENU_PlayFile "Fájl lejátszás..."
#define MSGTR_MENU_PlayVCD "VCD lejátszás..."  
#define MSGTR_MENU_PlayDVD "DVD lejátszás..."  
#define MSGTR_MENU_PlayURL "URL lejátszás..."  
#define MSGTR_MENU_LoadSubtitle "Felirat betöltése..."
#define MSGTR_MENU_DropSubtitle "Felirat eldobása..."
#define MSGTR_MENU_LoadExternAudioFile "Külső hang betöltése..."
#define MSGTR_MENU_Playing "Lejátszás"
#define MSGTR_MENU_Play "Lejátszás"
#define MSGTR_MENU_Pause "Pillanatállj"
#define MSGTR_MENU_Stop "Állj"  
#define MSGTR_MENU_NextStream "Következő fájl"
#define MSGTR_MENU_PrevStream "Előző fájl"
#define MSGTR_MENU_Size "Méret"
#define MSGTR_MENU_HalfSize   "Fél méret"
#define MSGTR_MENU_NormalSize "Normál méret"
#define MSGTR_MENU_DoubleSize "Dupla méret"
#define MSGTR_MENU_FullScreen "Teljesképernyő" 
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Lemez megnyitása..."
#define MSGTR_MENU_ShowDVDMenu "DVD menű"
#define MSGTR_MENU_Titles "Sávok"
#define MSGTR_MENU_Title "%2d. sáv"
#define MSGTR_MENU_None "(nincs)"
#define MSGTR_MENU_Chapters "Fejezetek"
#define MSGTR_MENU_Chapter "%2d. fejezet"
#define MSGTR_MENU_AudioLanguages "Szinkron nyelvei"
#define MSGTR_MENU_SubtitleLanguages "Feliratok nyelvei"
#define MSGTR_MENU_PlayList MSGTR_PlayList
#define MSGTR_MENU_SkinBrowser "Skin böngésző"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "Kilépés..."
#define MSGTR_MENU_Mute "Néma"
#define MSGTR_MENU_Original "Eredeti"
#define MSGTR_MENU_AspectRatio "Képarány"
#define MSGTR_MENU_AudioTrack "Audio track"
#define MSGTR_MENU_Track "%d. sáv"
#define MSGTR_MENU_VideoTrack "Video track"
#define MSGTR_MENU_Subtitles "Feliratok"

// --- equalizer
// Megjegyzés: Ha megváltoztatod az MSGTR_EQU_Audio-t, nézd meg, hogy megfelel-e az MSGTR_PREFERENCES_Audio-nak is!
#define MSGTR_EQU_Audio "Audió"
// Megjegyzés: Ha megváltoztatod az MSGTR_EQU_Video-t, nézd meg, hogy megfelel-e az MSGTR_PREFERENCES_Video-nak is!
#define MSGTR_EQU_Video "Videó"
#define MSGTR_EQU_Contrast "Kontraszt: "
#define MSGTR_EQU_Brightness "Fényerő: "
#define MSGTR_EQU_Hue "Szinárnyalat: "
#define MSGTR_EQU_Saturation "Telítettség: "
#define MSGTR_EQU_Front_Left "Bal Első"
#define MSGTR_EQU_Front_Right "Jobb Első"
#define MSGTR_EQU_Back_Left "Bal Hátsó"
#define MSGTR_EQU_Back_Right "Jobb Hátsó"
#define MSGTR_EQU_Center "Középső"
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
#define MSGTR_PREFERENCES_SubtitleOSD "Feliratok & OSD"
#define MSGTR_PREFERENCES_Codecs "Kodekek és demuxerek"
// Megjegyzés: Ha megváltoztatod az MSGTR_PREFERENCES_Misc-et, nézd meg, hogy megfelel-e az MSGTR_PREFERENCES_FRAME_Misc-nek is!
#define MSGTR_PREFERENCES_Misc "Egyéb"

#define MSGTR_PREFERENCES_None "Egyik sem"
#define MSGTR_PREFERENCES_DriverDefault "alapértelmezett vezérlő"
#define MSGTR_PREFERENCES_AvailableDrivers "Driverek:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Hang nélkül"
#define MSGTR_PREFERENCES_NormalizeSound "Hang normalizálása"
#define MSGTR_PREFERENCES_EnableEqualizer "Audio equalizer"
#define MSGTR_PREFERENCES_SoftwareMixer "Szoftveres keverés"
#define MSGTR_PREFERENCES_ExtraStereo "Extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Együttható:"
#define MSGTR_PREFERENCES_AudioDelay "Hang késleltetés"
#define MSGTR_PREFERENCES_DoubleBuffer "Dupla bufferelés"
#define MSGTR_PREFERENCES_DirectRender "Direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Kép eldobás"
#define MSGTR_PREFERENCES_HFrameDrop "Erőszakos kép eldobó"
#define MSGTR_PREFERENCES_Flip "Kép fejjel lefelé"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Óra es indikátorok"
#define MSGTR_PREFERENCES_OSDProgress "Csak a százalék jelzők"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Óra, százalék és a teljes idő"
#define MSGTR_PREFERENCES_Subtitle "Felirat:"
#define MSGTR_PREFERENCES_SUB_Delay "Késleltetés: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozíciója: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Felirat automatikus betöltésének tiltása"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode felirat"
#define MSGTR_PREFERENCES_SUB_MPSUB "A film feliratának konvertálása MPlayer felirat formátumba"
#define MSGTR_PREFERENCES_SUB_SRT "A film feliratának konvertálása SubViewer (SRT) formátumba"
#define MSGTR_PREFERENCES_SUB_Overlap "Felirat átlapolás"
#define MSGTR_PREFERENCES_SUB_USE_ASS "SSA/ASS felirat renderelés"
#define MSGTR_PREFERENCES_SUB_ASS_USE_MARGINS "Margók használata"
#define MSGTR_PREFERENCES_SUB_ASS_TOP_MARGIN "Fent: "
#define MSGTR_PREFERENCES_SUB_ASS_BOTTOM_MARGIN "Lent: "
#define MSGTR_PREFERENCES_Font "Betűk:"
#define MSGTR_PREFERENCES_FontFactor "Betű együttható:"
#define MSGTR_PREFERENCES_PostProcess "Képjavítás"
#define MSGTR_PREFERENCES_AutoQuality "Autómatikus minőség állítás: "
#define MSGTR_PREFERENCES_NI "non-interleaved  AVI  feltételezése (hibás AVI-knál segíthet"
#define MSGTR_PREFERENCES_IDX "Az AVI indexének újraépítése, ha szükséges"
#define MSGTR_PREFERENCES_VideoCodecFamily "Video kodek család:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Audio kodek család:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD szint"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Felirat"
#define MSGTR_PREFERENCES_FRAME_Font "Betű"
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
#define MSGTR_PREFERENCES_FontEncoding16 "Egyszerű kínai karakterkészlet (CP936)"
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
#define MSGTR_PREFERENCES_LoadFullscreen "Indítás teljes képernyőn"
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
#define MSGTR_ABOUT_Contributors "Kód és dokumentáció közreműködői\n"
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
#define MSGTR_AddingVideoFilter "[GUI] Videó szűrő hozzáadása: %s\n"
#define MSGTR_RemovingVideoFilter "[GUI] Videó szűrő eltávolítása: %s\n"

// mw.c

#define MSGTR_NotAFile "Úgy tűnik, hogy ez nem fájl: %s !\n"

// ws.c

#define MSGTR_WS_CouldNotOpenDisplay "[ws] A képernyő nem nyitható meg.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Távoli képernyő, XMITSHM kikapcsolva.\n"
#define MSGTR_WS_NoXshm "[ws] Bocs, a rendszered nem támogatja az X osztott memória kiterjesztést.\n"
#define MSGTR_WS_NoXshape "[ws] Bocs, a rendszered nem támogatja az XShape kiterjesztést.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Bocs, a szín mélység túl kicsi.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] Túl sok nyitott ablak van.\n"
#define MSGTR_WS_ShmError "[ws] osztott memória kiterjesztés hibája\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Bocs, nincs elég memória a rajz buffernek.\n"
#define MSGTR_WS_DpmsUnavailable "A DPMS nem elérhető?\n"
#define MSGTR_WS_DpmsNotEnabled "A DPMS nem engedélyezhető.\n"

// wsxdnd.c

#define MSGTR_WS_NotAFile "Úgy tűnik, hogy ez nem fájl...\n"
#define MSGTR_WS_DDNothing "D&D: Semmi sem jött vissza!\n"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "A kiválasztott video_out eszköz nem kompatibilis ezzel a codec-kel.\n"\
                "Próbáld meg hozzáadni a scale szűrőt a szűrő listádhoz,\n"\
                "pl. -vf spp,scale a -vf spp helyett.\n"
#define MSGTR_VO_GenericError "Hiba történt"
#define MSGTR_VO_UnableToAccess "Nem elérhető"
#define MSGTR_VO_ExistsButNoDirectory "már létezik, de nem könyvtár."
#define MSGTR_VO_DirExistsButNotWritable "A célkönyvtár már létezik, de nem írható."
#define MSGTR_VO_DirExistsAndIsWritable "A célkönyvtár már létezik és írható."
#define MSGTR_VO_CantCreateDirectory "Nem tudtam létrehozni a célkönyvtárat."
#define MSGTR_VO_CantCreateFile "A kimeneti fájl nem hozható létre."
#define MSGTR_VO_DirectoryCreateSuccess "A célkönyvtárat sikeresen létrehoztam."
#define MSGTR_VO_ParsingSuboptions "Alopciók értelmezése."
#define MSGTR_VO_SuboptionsParsedOK "Alopciók értelmezése rendben."
#define MSGTR_VO_ValueOutOfRange "érték határon kívül"
#define MSGTR_VO_NoValueSpecified "Nincs érték megadva."
#define MSGTR_VO_UnknownSuboptions "ismeretlen alopció(k)"

// vo_aa.c

#define MSGTR_VO_AA_HelpHeader "\n\nEzek az aalib vo_aa alopciói:\n"
#define MSGTR_VO_AA_AdditionalOptions "A vo_aa által biztosított opciók:\n" \
"  help        kiírja ezt a súgót\n" \
"  osdcolor    OSD szín beállítása\n  alszín    feliratszín beállítása\n" \
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
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Input nem RGB, nem lehet szétválasztani a színeket mezőnként!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "A kép szélességnek kettővel oszthatónak kell lennie."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Nincs elég memória az RGB framebuffer lefoglalásához."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Nincs elegendő memória vagy fájl handle a(z) \"%s\" írásához!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Hiba a kép kimenetre írása közben!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Ismeretlen aleszköz: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Interlaced kimeneti mód használata, top-field először."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Interlaced kimeneti mód használata, bottom-field először."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Progresszív (alapértelmezett) frame mód használata."

// sub.c
#define MSGTR_VO_SUB_Seekbar "Keresősáv"
#define MSGTR_VO_SUB_Play "Lejátszás"
#define MSGTR_VO_SUB_Pause "Pillanat állj"
#define MSGTR_VO_SUB_Stop "Állj"
#define MSGTR_VO_SUB_Rewind "Vissza"
#define MSGTR_VO_SUB_Forward "Előre"
#define MSGTR_VO_SUB_Clock "Óra"
#define MSGTR_VO_SUB_Contrast "Kontraszt"
#define MSGTR_VO_SUB_Saturation "Telítettség"
#define MSGTR_VO_SUB_Volume "Hangerő"
#define MSGTR_VO_SUB_Brightness "Fényerő"
#define MSGTR_VO_SUB_Hue "Színárnyalat"

// vo_xv.c
#define MSGTR_VO_XV_ImagedimTooHigh "A forrás kép méretei túl nagyok: %ux%u (maximum %ux%u)\n"

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "A pgm video kimeneti vezérlőt lecserélte a -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Az md5 video kimeneti vezérlőt lecserélte a -vo md5sum.\n"

// ======================= AO Audio Output drivers ========================

// libao2 

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: alsa9 és alsa1x modulok törölve lettek, használd a -ao alsa kapcsolót helyettük.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Nem tudom megnyitni a(z) %s keverő eszközt: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: A hangkártya keverőjének nincs '%s' csatornája, az alapértelmezettet használom.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: A(z) %s audio eszközt nem tudom megnyitni: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Nem lehet fájl leíró blokkolást végezni: %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] A(z) %s audio eszköz nem állítható be %s kimenetre, %s-t próbálok...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Nem sikerült az audio eszközt %d csatornára állítani.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: a vezérlő nem támogatja a SNDCTL_DSP_GETOSPACE-t :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Az audio vezérlőd NEM támogatja a select() -et ***\n Fordítsd újra az MPlayer-t az #undef HAVE_AUDIO_SELECT sorral a config.h-ban!\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nVégzetes hiba: *** NEM LEHET ÚJRA MEGNYITNI / BEÁLLÍTANI AZ AUDIO ESZKÖZT *** %s\n"
#define MSGTR_AO_OSS_UnknownUnsupportedFormat "[AO OSS] Ismeretlen/Nem támogatott OSS formátum: %x.\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Csatlakoztam a hang szerverhez.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Nem lehet megnyitni a folyamot.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Folyam megnyitva.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] buffer mérete: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Hangerő beállítása %d-re sikertelen.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] %d Hz nem támogatott, próbáld a resample-t\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound sikertelen: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] latency: [szerver: %0.2fs, net: %0.2fs] (igazítás %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] nem sikerült megnyitni az ESD playback folyamot: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] DVB audio keverő beállítása sikertelen: %s.\n" 
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz nem támogatott, új mintavételt próbálok.\n"

// ao_null.c
// This one desn't even  have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Fájl: %s (%s)\nPCM: Samplerate: %iHz Csatorna: %s Formátum: %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Infó: Gyorsabb dump-olás a -vc null -vo null -ao pcm:fast kapcsolóval érhető el\n[AO PCM] Info: WAVE fájlok írásához használd a -ao pcm:waveheader kapcsolót (alapértelmezett)!\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] %s megnyitása írásra nem sikerült!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Samplerate: %iHz Csatornák: %s Formátum: %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] %s audio vezérlő használata.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Nem támogatott audio formátum: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] SDL Audio inicializálása nem sikerült: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] audio megnyitása nem sikerült: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] vezérlés.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Samplerate: %iHz Csatorna: %s Formátum: %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] lejátszás: hibás eszköz.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparams sikertelen: %s\nNem sikerült beállítani az előírt samplerate-et.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE-et nem fogadta el a kiválasztott erőforrás.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams sikertelen: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: samplerate most már %lf (előírt ráta: %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Nem tudom megnyitni az audio csatornát: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO sikertelen.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: írás sikertelen.\n"
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] %s audio eszköz nem elérhető, %s  -> nincs hang.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: a kártyád nem támogat %d csatornát, %s, %d Hz samplerate-t.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  A hangkártyád NEM támogatja a select()-et ***\nFordítsd újra az MPlayer-t az #undef HAVE_AUDIO_SELECT sorral a config.h-ban !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nVégzetes hiba: *** NEM LEHET ÚJRA MEGNYITNI / BEÁLLÍTANI AZ AUDIO ESZKÖZT (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: kért formátum: %d Hz, %d csatorna, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: nem találtam hangkártyát.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: hibás formátumot (%s) kértél - kimenet letiltva.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: playback megnyitási hiba: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: PCM info hiba: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: %d hangkártyát találtam, ezt használom: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: PCM csatorna info hiba: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: hiba a paraméterek beállításakor: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: hiba a csatorna beállításakor: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: csatorna előkészítési hiba: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: lejátszás drain hiba: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: lejátszás ürítési hiba: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: PCM lezárási hiba: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: lejátszás drain hiba: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: lejátszás ürítési hiba: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: csatorna előkészítési hiba: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: lejátszás drain hiba: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: lejátszás ürítési hiba: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: csatorna előkészítési hiba: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: alsa underrun, folyam beállítása.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: lejátszás előkészítési hiba: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: írási hiba a beállítás után: %s - feladom.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: kimeneti hiba: %s\n"

// ao_alsa.c
#define MSGTR_AO_ALSA_InvalidMixerIndexDefaultingToZero "[AO_ALSA] Hibás mixer index. Alapértelmezés 0-ra.\n"
#define MSGTR_AO_ALSA_MixerOpenError "[AO_ALSA] Mixer megnyitási hiba: %s\n"
#define MSGTR_AO_ALSA_MixerAttachError "[AO_ALSA] Mixer %s csatolás hiba: %s\n"
#define MSGTR_AO_ALSA_MixerRegisterError "[AO_ALSA] Mixer regisztrálási hiba: %s\n"
#define MSGTR_AO_ALSA_MixerLoadError "[AO_ALSA] Mixer betöltés hiba: %s\n"
#define MSGTR_AO_ALSA_UnableToFindSimpleControl "[AO_ALSA] A(z) '%s' egyszerű vezérlés nem található, %i.\n"
#define MSGTR_AO_ALSA_ErrorSettingLeftChannel "[AO_ALSA] Hiba a bal csatorna beállításakor, %s\n"
#define MSGTR_AO_ALSA_ErrorSettingRightChannel "[AO_ALSA] Hiba a jobb csatorna beállításakor, %s\n"
#define MSGTR_AO_ALSA_CommandlineHelp "\n[AO_ALSA] -ao alsa parancssori súgó:\n"\
"[AO_ALSA] Példa: mplayer -ao alsa:device=hw=0.3\n"\
"[AO_ALSA]   Beállítja az első kártya negyedik hardver eszközét.\n\n"\
"[AO_ALSA] Opciók:\n"\
"[AO_ALSA]   noblock\n"\
"[AO_ALSA]     Non-blocking módban nyitja meg az eszközt.\n"\
"[AO_ALSA]   device=<eszköz-név>\n"\
"[AO_ALSA]     Beállítja az eszközt (cseréld ki a ,-t .-ra és a :-t =-re)\n"
#define MSGTR_AO_ALSA_ChannelsNotSupported "[AO_ALSA] %d csatorna nem támogatott.\n"
#define MSGTR_AO_ALSA_OpenInNonblockModeFailed "[AO_ALSA] A nonblock-módú megnyitás sikertelen, megpróbálom block-módban megnyitni.\n"
#define MSGTR_AO_ALSA_PlaybackOpenError "[AO_ALSA] Visszajátszás megnyitásának hibája: %s\n"
#define MSGTR_AO_ALSA_ErrorSetBlockMode "[AL_ALSA] Hiba a(z) %s block-mód beállításakor.\n"
#define MSGTR_AO_ALSA_UnableToGetInitialParameters "[AO_ALSA] Sikertelen a kezdeti paraméterek lekérdezése: %s\n"
#define MSGTR_AO_ALSA_UnableToSetAccessType "[AO_ALSA] Sikertelen a hozzáférési típus beállítása: %s\n"
#define MSGTR_AO_ALSA_FormatNotSupportedByHardware "[AO_ALSA] A(z) %s formátum nem támogatott hardveresen, alapértelmezett próbálása.\n"
#define MSGTR_AO_ALSA_UnableToSetFormat "[AO_ALSA] Sikertelen a formátum beállítás: %s\n"
#define MSGTR_AO_ALSA_UnableToSetChannels "[AO_ALSA] Sikertelen a csatorna beállítás: %s\n"
#define MSGTR_AO_ALSA_UnableToDisableResampling "[AO_ALSA] A resampling letiltása sikertelen: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSamplerate2 "[AO_ALSA] Sikerteln a samplerate-2 beállítása: %s\n"
#define MSGTR_AO_ALSA_UnableToSetBufferTimeNear "[AO_ALSA] Sikertelen a buffer idő beállítása: %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodTime "[AO_ALSA] Sikertelen a periódusidő beállítása: %s\n"
#define MSGTR_AO_ALSA_BufferTimePeriodTime "[AO_ALSA] buffer_time: %d, period_time :%d\n"
#define MSGTR_AO_ALSA_UnableToGetPeriodSize "[AO ALSA] Sikertelen a periódus idő lekérdezése: %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodSize "[AO ALSA] Sikertelen a periódus méret beállítása (%ld): %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriods "[AO_ALSA] Sikertelen a periódusok beállítása: %s\n"
#define MSGTR_AO_ALSA_UnableToSetHwParameters "[AO_ALSA] Sikerteln a hw-paraméter-ek beállítása: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBufferSize "[AO_ALSA] Sikerteln a buffer méret lekérdezése: %s\n"
#define MSGTR_AO_ALSA_UnableToGetSwParameters "[AO_ALSA] Sikertelen az sw-paraméterek lekérdezése: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSwParameters "[AO_ALSA] Sikertelen az sw-paraméterek beállítása: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBoundary "[AO_ALSA] Sikertelen a határ lekérdezése: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStartThreshold "[AO_ALSA] Sikertelen a kezdei küszöb beállítása: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStopThreshold "[AO_ALSA] Sikertelen a befejezési küszöb beállítása: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSilenceSize "[AO_ALSA] Sikertelen a szünet méretének beállítása: %s\n"
#define MSGTR_AO_ALSA_PcmCloseError "[AO_ALSA] pcm lezárási hiba: %s\n"
#define MSGTR_AO_ALSA_NoHandlerDefined "[AO_ALSA] Nincs kezelő definiálva!\n"
#define MSGTR_AO_ALSA_PcmPrepareError "[AO_ALSA] pcm előkészítés hiba: %s\n"
#define MSGTR_AO_ALSA_PcmPauseError "[AO_ALSA] pcm szünet hiba: %s\n"
#define MSGTR_AO_ALSA_PcmDropError "[AO_ALSA] pcm eldobás hiba: %s\n"
#define MSGTR_AO_ALSA_PcmResumeError "[AO_ALSA] pcm folytatás hiba: %s\n"
#define MSGTR_AO_ALSA_DeviceConfigurationError "[AO_ALSA] Eszköz konfigurációs hiba."
#define MSGTR_AO_ALSA_PcmInSuspendModeTryingResume "[AO_ALSA] A pcm pihenő módban van, megpróbálom folytatni.\n"
#define MSGTR_AO_ALSA_WriteError "[AO_ALSA] Írási hiba: %s\n"
#define MSGTR_AO_ALSA_TryingToResetSoundcard "[AO_ALSA] Hangkártya resetelése.\n"
#define MSGTR_AO_ALSA_CannotGetPcmStatus "[AO_ALSA] A pcm állapot nem kérdezhető le: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] hibás plugin: %s\n"

// ======================= AF Audio Filters ================================

// libaf 

// af_ladspa.c

#define MSGTR_AF_LADSPA_AvailableLabels "használható cimkék"
#define MSGTR_AF_LADSPA_WarnNoInputs "FIGYELEM! Ennek a LADSPA pluginnak nincsenek audio bemenetei.\n  A bejövő audió jelek elvesznek."
#define MSGTR_AF_LADSPA_ErrMultiChannel "A több-csatornás (>2) plugin (még) nem támogatott.\n  Csak a mono és sztereo plugin-okat használd."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Ennek a LADSPA pluginnak nincsenek audió bemenetei."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Különbözik a LADSPA plugin audió bemenetek és kimenetek száma."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "nem sikerült betölteni"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "A ladspa_descriptor() függvény nem található a megadott függvénykönyvtár fájlban."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "A címke nem található a plugin könyvtárban."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Nincs alopció megadva."
#define MSGTR_AF_LADSPA_ErrNoLibFile "Nincs könyvtárfájl megadva."
#define MSGTR_AF_LADSPA_ErrNoLabel "Nincs szűrő címke megadva."
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Nincs elég vezérlő megadva a parancssorban."
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: A(z) #%d bemeneti vezérlő a(z) %0.4f alsó határ alatt van.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: A(z) #%d bemeneti vezérlő a(z) %0.4f felső határ felett van.\n"

// format.c

#define MSGTR_AF_FORMAT_UnknownFormat "ismeretlen formátum "

// ========================== INPUT =========================================

// joystick.c

#define MSGTR_INPUT_JOYSTICK_Opening "Botkormány eszköz megnyitása: %s\n"
#define MSGTR_INPUT_JOYSTICK_CantOpen "Nem sikerült a(z) %s botkormány eszközt megnyitni: %s\n"
#define MSGTR_INPUT_JOYSTICK_ErrReading "Hiba a botkormány eszköz olvasása közben: %s\n"
#define MSGTR_INPUT_JOYSTICK_LoosingBytes "Botkormány: elvesztettünk %d bájtnyi adatot\n"
#define MSGTR_INPUT_JOYSTICK_WarnLostSync "Botkormány: figyelmeztető init esemény, elvesztettük a szinkront a vezérlővel.\n"
#define MSGTR_INPUT_JOYSTICK_WarnUnknownEvent "Botkormány ismeretlen figyelmeztető esemény típus: %d\n"

// input.c

#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "Túl sok parancs fájl leíró, nem sikerült a(z) %d fájl leíró regisztálása.\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "Túl sok gomb fájl leíró, nem sikerült a(z) %d fájl leíró regisztálása.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "%s parancs: %d argumentum nem egész.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "%s parancs: %d argumentum nem lebegőpontos.\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "%s parancs: %d argumentum lezáratlan.\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "Ismeretlen argumentum: %d\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "A(z) %s parancsnak legalább %d argumentum kell, de csak %d-t találtunk eddig.\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "Hiba a(z) %d parancs fájl leíró olvasása közben: %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "A(z) %d fájlleíró parancs buffere tele van: tartalom eldobása.\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "Hibás parancs a(z) %s gombnál"
#define MSGTR_INPUT_INPUT_ErrSelect "Kiválasztási hiba: %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "Hiba a(z) %d gomb input fájl leírójában\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "Halott gomb input a(z) %d fájl leírónál\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "Túl sok gomblenyomási esemény egy időben\n"
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

#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "A karakterlánc már escape-ltnek tűnik az url_escape-ben %c%c1%c2\n"

// ai_alsa1x.c

#define MSGTR_MPDEMUX_AIALSA1X_CannotSetSamplerate "Nem állítható be a mintavételi ráta.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetBufferTime "Nem állítható be a buffer idő.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetPeriodTime "Nem állítható be a periódus idő.\n"

// ai_alsa1x.c / ai_alsa.c

#define MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig "Hibás konfiguráció ehhez a PCM-hez: nincs elérhető konfiguráció.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableAccessType "Elérési típus nem használható.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt "Minta formátum nem elérhető.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableChanCount "Csatorna számláló nem elérhető - visszatérés az alapértelmezetthez: %d\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams "Sikertelen a hardver paraméterek beállítása: %s"
#define MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize "Nem használható a buffer mérettel egyező periódus (%u == %lu)\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallSWParams "Sikertelen a szoftver paraméterek beállítása:\n"
#define MSGTR_MPDEMUX_AIALSA_ErrorOpeningAudio "Hiba az audió megnyitásakor: %s\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatusError "ALSA státusz hiba: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUN "ALSA xrun!!! (legalább %.3f ms hosszan)\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatus "ALSA Státusz:\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUNPrepareError "ALSA xrun: előkészítési hiba: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaReadWriteError "ALSA olvasás/írás hiba"

// ai_oss.c

#define MSGTR_MPDEMUX_AIOSS_Unable2SetChanCount "Sikertelen a csatorna számláló beállítása: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetStereo "Sikertelen a sztereó beállítása: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2Open "'%s' nem nyitható meg: %s\n"
#define MSGTR_MPDEMUX_AIOSS_UnsupportedFmt "nem támogatott formátum\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt "Az audió formátum nem állítható be."
#define MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate "A mintavételi ráta nem állítható be: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger "A trigger nem állítható be: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize "Nem sikerült lekérdezni a blokkméretet!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero "Az audió blokk méret nulla, beállítva: %d!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low "Az audió blokk méret túl kicsi, beállítva: %d!\n"

// asfheader.c

#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB "VÉGZETES HIBA: fejléc méret nagyobb, mint 1 MB (%d)!\nKeresd meg az MPlayer készítőit és töltsd fel/küldd el ezt a fájlt.\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "Nem sikerült %d bájt lefoglalása a fejléchez.\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "EOF az ASF fejléc olvasása közben, hibás/nem teljes fájl?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "A DVR valószínűleg csak libavformat-tal működik, próbáld ki a -demuxer 35 -öt probléma esetén\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "Nincs adat rész a fejléc után!\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF: nem található audió vagy videó fejléc - hibás fájl?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "Hibás hossz az ASF fejlécben!\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMLicenseURL "DRM Licensz URL: %s\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMProtected "Ez a fájl DRM titkosítással van ellátva, nem lehet lejátszani az MPlayerrel!\n"

// asf_mmst_streaming.c

#define MSGTR_MPDEMUX_MMST_WriteError "írási hiba\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\nRiadó! eof\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "elő-fejléc olvasás sikertelen\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "Hibás fejléc méret, feladom.\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "Fejléc adat olvasási hiba.\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "packet_len olvasási hiba.\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "Hibás RTSP csomag méret, feladom.\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "Parancs adat olvasási hiba.\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "fejléc objektum\n"
#define MSGTR_MPDEMUX_MMST_DataObject "adat objektum\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "fájl objektum, csomag méret = %d (%d)\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "folyam objektum, folyam id: %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "Túl sok id, a folyam figyelmen kívül hagyva."
#define MSGTR_MPDEMUX_MMST_UnknownObject "ismeretlen objektum\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "Média adat olvasási hiba.\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "hiányzó aláírás\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "Minden kész. Köszönjük, hogy szabadalmazott technológiát alkalmazó médiát töltöttél le.\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "ismeretlen parancs %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "get_media_packet hiba : %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "Csatlakozva\n"

// asf_streaming.c

#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "Ahhhh, stream_chunck méret túl kicsi: %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "size_confirm hibás!: %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "Figyelmeztetés: fejléc eldobva ????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "Hiba a fejléc chunk értelmezésekor\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "Nem fejléc az első chunk !!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "Hiba, nem lehet allokálni %d bájtos buffert.\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "Hiba a hálózati folyam olvasása közben.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "Hiba, a chunk túl kicsi.\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "Hiba, az al-chunk-ok száma helytelen.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "Kicsi a sávszélesség, a fájl nem lejátszható!\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "A sávszélesség túl kicsi, audió folyam kikapcsolva.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "A sávszélesség túl kicsi, videó folyam kikapcsolva.\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "Hibás hossz az ASF fejlécben!\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "Hiba a chunk fejlécének olvasásakor.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "Hiba: chunk_size > packet_size\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "Hiba a chunk olvasása közben.\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> ASF Redirector\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "hibás proxy URL\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "Ismeretlen ASF folyam típus\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "Sikertelen a HTTP válasz értelmezése.\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "Szerver válasz %d:%s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "ASF HTTP ÉRTELMEZÉSI HIBA : %s pragma levágva %d bájtról %d bájtra\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "socket írási hiba : %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "Sikertelen a fájléc értelmezése.\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "Nem található folyam.\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "ismeretlen ASF folyam típus\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL: %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "Sikertelen, kilépés.\n"

// audio_in.c

#define MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio "\nHiba az audió olvasásakor: %s\n"
#define MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut "Visszatérés a cross-run-ból, néhány képkocka kimaradhatott!\n"
#define MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover "Végzetes hiba, nem lehet visszatérni!\n"
#define MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples "\nNincs elég audió minta!\n"

// aviheader.c

#define MSGTR_MPDEMUX_AVIHDR_EmptyList "** üres lista?!\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundMovieAt "Film megtalálva: 0x%X - 0x%X\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader "'bih' megtalálva, %u bájt %d bájtból\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1 "A kulcs képkocka tábla újragenerálva az M$ mpg4v1 videóhoz.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3 "Kulcs képkocka tábla újragenerálása a DIVX3 videóhoz.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4 "Kulcs képkocka tábla újragenerálása az MPEG4 videóhoz.\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt "'wf' megtalálva, %d bájt %d bájtból\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header "AVI: dmlh megtalálva (size=%d) (total_frames=%d)\n"
#define MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames "INDEX blokk olvasása, %d chunk %d képkockához (fpos=%"PRId64").\n"
#define MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr "Kiegészítő RIFF fejléc...\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** Figyelmeztetés: ez nem kiterjesztett AVI fejléc..\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenChunk "Hibás chunk?  chunksize=%d  (id=%.4s)\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI: ODML: ODML index felépítése (%d superindexchunks)\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI: ODML: Hibás (nem teljes?) fájlt találtam. Tradícionális index használata.\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "A(z) %s index fájl nem olvasható: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "%s nem érvényes MPlayer index fájl.\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "Nem lehet memóriát foglalni az index adatoknak %s-ből.\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "korai index fájlvég %s fájlban\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "Betöltött index fájl: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "Index generálása: %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI: Index tábla legenerálva %d chunk-hoz!\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "Nem sikerült a(z) %s index fájl írása: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "Elmentett index fájl: %s\n"

// cache2.c

#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\rEz a folyam nem cache-elhető.\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!! read_filepos különbözik!!! Jelezd ezt a hibát...\n"

// cdda.c

#define MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice "Nem nyitható meg a CDDA eszköz.\n"
#define MSGTR_MPDEMUX_CDDA_CantOpenDisc "Nem nyitható meg a lemez.\n"
#define MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks "Audió CD-t találtam %ld sávval.\n"

// cddb.c

#define MSGTR_MPDEMUX_CDDB_FailedToReadTOC "Hiba a TOC olvasása közben.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToOpenDevice "Hiba a(z) %s eszköz megnyitásakor.\n"
#define MSGTR_MPDEMUX_CDDB_NotAValidURL "hibás URL\n"
#define MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest "HTTP kérés elküldése nem sikerült.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse "HTTP válasz olvasása nem sikerült.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND "Nem található.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown "ismeretlen hibakód\n"
#define MSGTR_MPDEMUX_CDDB_NoCacheFound "Nem találtam cache-t.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead "Nem minden xmcd fájl lett elolvasva.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory "Sikertelen a(z) %s könyvtár létrehozása.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten "Nem minden xmcd fájl lett kiírva.\n"
#define MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned "Hibás xmcd adatbázis fájl érkezett vissza.\n"
#define MSGTR_MPDEMUX_CDDB_UnexpectedFIXME "váratlan FIXME\n"
#define MSGTR_MPDEMUX_CDDB_UnhandledCode "kezeletlen kód\n"
#define MSGTR_MPDEMUX_CDDB_UnableToFindEOL "Nem található a sor vége.\n"
#define MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle "Értelmezés OK, találtam: %s\n"
#define MSGTR_MPDEMUX_CDDB_AlbumNotFound "Album nem található.\n"
#define MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr "Szerver válasza: Parancs szintaxis hibás\n"
#define MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable "Nincs elérhető oldal információ.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel "Sikertelen a protokol szint lekérdezése.\n"
#define MSGTR_MPDEMUX_CDDB_NoCDInDrive "Nincs CD a meghajtóban.\n"

// cue_read.c

#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] Nem várt cuefájl sor: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] tesztelt bin fájlnév: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] Nem található a bin fájl - feladom.\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] %s bin fájl használata.\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] Ismeretlen mód a binfájlhoz. Nem szabadna megtörténnie. Megszakítás.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] %s nem nyitható meg.\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] Hiba %s fájlból történő olvasáskor\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] Hiba a bin fájl méretének lekérdezésekor.\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "sáv %02d:  formátum=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] nem várt vége a bin fájlnak\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] Nem olvasható %d bájtnyi payload.\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "CUE stream_open, fájlnév=%s, sáv=%d, elérhető sávok: %d -> %d\n"

// network.c

#define MSGTR_MPDEMUX_NW_UnknownAF "Ismeretlen címosztály: %d\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "%s feloldása erre: %s...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "Nem oldható fel név %s -hez: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "Csatlakozás a(z) %s[%s] szerverhez: %d...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "Sikertelen csatlakozás a szerverhez %s -sel\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "Kiválasztás sikertelen.\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "időtúllépés a csatlakozáskor\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "getsockopt sikertelen: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "csatlakozási hiba: %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "Hibás proxy beállítás... Megpróbálom proxy nélkül.\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "Nem oldható fel a távoli hosztnév az AF_INET-hez. Megpróbálom proxy nélkül.\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "Hiba a HTTP kérés küldésekor: nem küldte el az összes kérést.\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "Olvasás sikertelen.\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "http_read_response 0-át olvasott (pl. EOF).\n"
#define MSGTR_MPDEMUX_NW_AuthFailed "Azonosítás sikertelen. Kérlek használd a -user és -passwd kapcsolókat az\n"\
"azonosító/jelszó megadásához URL listáknál, vagy írd az alábbi formában az URL-t:\n"\
"http://usernev:jelszo@hostnev/fajl\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "Azonosítás szükséges ehhez: %s\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "Azonosítás szükséges.\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "Nincs jelszó megadva, üres jelszót próbálok.\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "Szerver válasz %d: %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "Cache méret beállítva %d KByte-ra\n"

// demux_audio.c

#define MSGTR_MPDEMUX_AUDIO_UnknownFormat "Audio demuxer: %d ismeretlen formátum.\n"

// demux_demuxers.c

#define MSGTR_MPDEMUX_DEMUXERS_FillBufferError "fill_buffer hiba: hibás demuxer: nem vd, ad vagy sd.\n"

// demux_mkv.c
#define MSGTR_MPDEMUX_MKV_ZlibInitializationFailed "[mkv] zlib inicializálás sikertelen.\n"
#define MSGTR_MPDEMUX_MKV_ZlibDecompressionFailed "[mkv] zlib kicsomagolás sikertelen.\n"
#define MSGTR_MPDEMUX_MKV_LzoInitializationFailed "[mkv] lzo inicializálás sikertelen.\n"
#define MSGTR_MPDEMUX_MKV_LzoDecompressionFailed "[mkv] lzo kicsomagolás sikertelen.\n"
#define MSGTR_MPDEMUX_MKV_TrackEncrypted "[mkv] A(z) %u. sorszámú sáv titkosított, a visszakódolás pedig még\n[mkv] nem támogatott. Sáv kihagyása.\n"
#define MSGTR_MPDEMUX_MKV_UnknownContentEncoding "[mkv] Ismeretlen tartalom kódolási típus a(z) %u. sávban. Sáv kihagyása.\n"
#define MSGTR_MPDEMUX_MKV_UnknownCompression "[mkv] A(z) %u. sáv ismeretlen/nem támogatott tömörítő algoritmussal lett\n[mkv] tömörítve (%u). Sáv kihagyása.\n"
#define MSGTR_MPDEMUX_MKV_ZlibCompressionUnsupported "[mkv] A(z) %u. sáv zlib-bel lett tömörítve, de az MPlayer\n[mkv] zlib tömörítés támogatása nélkül lett lefordítva. Sáv kihagyása.\n"
#define MSGTR_MPDEMUX_MKV_TrackIDName "[mkv] Track ID %u: %s (%s) \"%s\", %s\n"
#define MSGTR_MPDEMUX_MKV_TrackID "[mkv] Track ID %u: %s (%s), %s\n"
#define MSGTR_MPDEMUX_MKV_UnknownCodecID "[mkv] Ismeretlen/nem támogatott CodecID (%s) vagy hiányzó/hibás CodecPrivate\n[mkv] adat (%u. sáv).\n"
#define MSGTR_MPDEMUX_MKV_FlacTrackDoesNotContainValidHeaders "[mkv] A FLAC sáv nem tartalmaz érvényes fejlécet.\n"
#define MSGTR_MPDEMUX_MKV_UnknownAudioCodec "[mkv] Ismeretlen/nem támogatott audió codec ID '%s' a(z) %u. sávban vagy hiányzó/hibás\n[mkv] privát codec adat.\n"
#define MSGTR_MPDEMUX_MKV_SubtitleTypeNotSupported "[mkv] A(z) '%s' felirat típus nem támogatott.\n"
#define MSGTR_MPDEMUX_MKV_WillPlayVideoTrack "[mkv] %u. videó sáv lejátszása.\n"
#define MSGTR_MPDEMUX_MKV_NoVideoTrackFound "[mkv] Nem található/nincs kiválasztott videó sáv.\n"
#define MSGTR_MPDEMUX_MKV_NoAudioTrackFound "[mkv] Nem található/nincs kiválasztott audió sáv.\n"
#define MSGTR_MPDEMUX_MKV_WillDisplaySubtitleTrack "[mkv] %u. felirat sáv megjelenítése.\n"
#define MSGTR_MPDEMUX_MKV_NoBlockDurationForSubtitleTrackFound "[mkv] Figyelmeztetés: Nem található BlockDuration a felirat sávban.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublines "[mkv] Figyelmeztetés: túl sok renderelendő subline, kihagyás.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublinesSkippingAfterFirst "\n[mkv] Figyelmeztetés: túl sok renderelendő subline, kihagyva az első %i után.\n"

// demux_nuv.c

#define MSGTR_MPDEMUX_NUV_NoVideoBlocksInFile "Nincs videó blokk a fájlban.\n"

// demux_xmms.c

#define MSGTR_MPDEMUX_XMMS_FoundPlugin "Megtalált plugin: %s (%s).\n"
#define MSGTR_MPDEMUX_XMMS_ClosingPlugin "Plugin lezárása: %s.\n"
#define MSGTR_MPDEMUX_XMMS_WaitForStart "Várakozás a(z) '%s' XMMS plugin általi lejátszására...\n"

// ========================== LIBMPMENU ===================================

// common

#define MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition "[MENU] Nem található bejegyzés a menü definícióban.\n"

// libmenu/menu.c
#define MSGTR_LIBMENU_SyntaxErrorAtLine "[MENU] szintaktikai hiba ebben a sorban: %d\n"
#define MSGTR_LIBMENU_MenuDefinitionsNeedANameAttrib "[MENU] A menü definíciókhoz nevesített attribútum kell (%d. sor).\n"
#define MSGTR_LIBMENU_BadAttrib "[MENU] hibás attribútum %s=%s a(z) '%s' menüben a(z) %d. sorban\n"
#define MSGTR_LIBMENU_UnknownMenuType "[MENU] ismeretlen menü típus: '%s' a(z) %d. sorban\n"
#define MSGTR_LIBMENU_CantOpenConfigFile "[MENU] A menü konfigurációs fájl nem nyitható meg: %s\n"
#define MSGTR_LIBMENU_ConfigFileIsTooBig "[MENU] A konfigurációs fájl túl nagy (> %d KB)\n"
#define MSGTR_LIBMENU_ConfigFileIsEmpty "[MENU] A konfigurációs fájl üres.\n"
#define MSGTR_LIBMENU_MenuNotFound "[MENU] A(z) %s menü nem található.\n"
#define MSGTR_LIBMENU_MenuInitFailed "[MENU] '%s' menü: init sikertelen.\n"
#define MSGTR_LIBMENU_UnsupportedOutformat "[MENU] Nem támogatott kimeneti formátum!!!!\n"

// libmenu/menu_cmdlist.c
#define MSGTR_LIBMENU_ListMenuEntryDefinitionsNeedAName "[MENU] A lista menüelemek definícióihoz kell egy név (%d. sor).\n"
#define MSGTR_LIBMENU_ListMenuNeedsAnArgument "[MENU] A lista menühöz egy argumentum kell.\n"

// libmenu/menu_console.c
#define MSGTR_LIBMENU_WaitPidError "[MENU] Waitpid hiba: %s.\n"
#define MSGTR_LIBMENU_SelectError "[MENU] Kiválasztási hiba.\n"
#define MSGTR_LIBMENU_ReadErrorOnChilds "[MENU] Olvasási hiba a gyerek fájlleírójában: %s.\n"
#define MSGTR_LIBMENU_ConsoleRun "[MENU] Konzol futtatás: %s ...\n"
#define MSGTR_LIBMENU_AChildIsAlreadyRunning "[MENU] Egy gyermek már fut.\n"
#define MSGTR_LIBMENU_ForkFailed "[MENU] Fork sikertelen !!!\n"
#define MSGTR_LIBMENU_WriteError "[MENU] írási hiba\n"

// libmenu/menu_filesel.c
#define MSGTR_LIBMENU_OpendirError "[MENU] opendir hiba: %s.\n"
#define MSGTR_LIBMENU_ReallocError "[MENU] realloc hiba: %s.\n"
#define MSGTR_LIBMENU_MallocError "[MENU] memória foglalási hiba: %s.\n"
#define MSGTR_LIBMENU_ReaddirError "[MENU] readdir hiba: %s.\n"
#define MSGTR_LIBMENU_CantOpenDirectory "[MENU] A(z) %s könyvtár nem nyitható meg.\n"

// libmenu/menu_param.c
#define MSGTR_LIBMENU_SubmenuDefinitionNeedAMenuAttribut "[MENU] Az almenü definíciókba kell egy 'menu' attribútum.\n"
#define MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed "[MENU] Pref menü bejegyzés definícióihoz egy jó 'property' attribútum kell (%d. sor).\n"
#define MSGTR_LIBMENU_PrefMenuNeedsAnArgument "[MENU] Pref menühöz egy argumentum kell.\n"

// libmenu/menu_pt.c
#define MSGTR_LIBMENU_CantfindTheTargetItem "[MENU] Nem található a cél elem ????\n"
#define MSGTR_LIBMENU_FailedToBuildCommand "[MENU] Nem sikerült a parancs felépítése: %s.\n"

// libmenu/menu_txt.c
#define MSGTR_LIBMENU_MenuTxtNeedATxtFileName "[MENU] A szöveges menühöz egy szöveges fájl név kell (fájl paraméter).\n"
#define MSGTR_LIBMENU_MenuTxtCantOpen "[MENU] Nem nyitható meg %s.\n"
#define MSGTR_LIBMENU_WarningTooLongLineSplitting "[MENU] Figyelem, túl hozzú sor. Elvágom.\n"
#define MSGTR_LIBMENU_ParsedLines "[MENU] %d sor értelmezve.\n"

// libmenu/vf_menu.c
#define MSGTR_LIBMENU_UnknownMenuCommand "[MENU] Ismeretlen parancs: '%s'.\n"
#define MSGTR_LIBMENU_FailedToOpenMenu "[MENU] Sikertelen a menü megnyitása: '%s'.\n"

// ========================== LIBMPCODECS ===================================

// libmpcodecs/ad_libdv.c
#define MSGTR_MPCODECS_AudioFramesizeDiffers "[AD_LIBDV] Figyelem! Az audió keretméret különböző! read=%d  hdr=%d.\n"

// libmpcodecs/vd_dmo.c vd_dshow.c vd_vfw.c
#define MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec "[VD_DMO] Nem foglalható le a kép a cinepak codec-hez.\n"

// libmpcodecs/vd_ffmpeg.c
#define MSGTR_MPCODECS_XVMCAcceleratedCodec "[VD_FFMPEG] XVMC-vel gyorsított codec.\n"
#define MSGTR_MPCODECS_ArithmeticMeanOfQP "[VD_FFMPEG] QP aritmetikus közepe: %2.4f, QP harmonikus közepe: %2.4f\n"
#define MSGTR_MPCODECS_DRIFailure "[VD_FFMPEG] DRI hiba.\n"
#define MSGTR_MPCODECS_CouldntAllocateImageForCodec "[VD_FFMPEG] Nem sikerült a kép lefoglalása a codec-hez.\n"
#define MSGTR_MPCODECS_XVMCAcceleratedMPEG2 "[VD_FFMPEG] XVMC-vel gyorsított MPEG-2.\n"
#define MSGTR_MPCODECS_TryingPixfmt "[VD_FFMPEG] pixfmt=%d kipróbálása.\n"
#define MSGTR_MPCODECS_McGetBufferShouldWorkOnlyWithXVMC "[VD_FFMPEG] Az mc_get_buffer csak XVMC gyorsítással működik!!"
#define MSGTR_MPCODECS_UnexpectedInitVoError "[VD_FFMPEG] Váratlan init_vo hiba.\n"
#define MSGTR_MPCODECS_UnrecoverableErrorRenderBuffersNotTaken "[VD_FFMPEG] Helyrehozhatatlan hiba, a render bufferek nincsenek meg.\n"
#define MSGTR_MPCODECS_OnlyBuffersAllocatedByVoXvmcAllowed "[VD_FFMPEG] Csak a vo_xvmc által lefoglalt bufferek használhatóak.\n"

// libmpcodecs/ve_lavc.c
#define MSGTR_MPCODECS_HighQualityEncodingSelected "[VE_LAVC] Nagyon jó minőségű kódolás kiválasztva (nem valós idejű)!\n"
#define MSGTR_MPCODECS_UsingConstantQscale "[VE_LAVC] Konstans qscale = %f (VBR) használata.\n"

// libmpcodecs/ve_raw.c
#define MSGTR_MPCODECS_OutputWithFourccNotSupported "[VE_RAW] Nyers kimenet FourCC-vel [%x] nem támogatott!\n"
#define MSGTR_MPCODECS_NoVfwCodecSpecified "[VE_RAW] A kért VfW codec nincs megadva!!\n"

// libmpcodecs/vf_crop.c
#define MSGTR_MPCODECS_CropBadPositionWidthHeight "[CROP] Hibás pozíció/szélesség/magasság - a levágott terület az eredetin kívül van!\n"

// libmpcodecs/vf_cropdetect.c
#define MSGTR_MPCODECS_CropArea "[CROP] Vágási terület: X: %d..%d  Y: %d..%d  (-vf crop=%d:%d:%d:%d).\n"

// libmpcodecs/vf_format.c, vf_palette.c, vf_noformat.c
#define MSGTR_MPCODECS_UnknownFormatName "[VF_FORMAT] Ismeretlen formátumnév: '%s'.\n"

// libmpcodecs/vf_framestep.c vf_noformat.c vf_palette.c vf_tile.c
#define MSGTR_MPCODECS_ErrorParsingArgument "[VF_FRAMESTEP] Hiba az argumentum értelmezésekor.\n"

// libmpcodecs/ve_vfw.c
#define MSGTR_MPCODECS_CompressorType "Tömörítő típusa: %.4lx\n"
#define MSGTR_MPCODECS_CompressorSubtype "Tömörítő altípusa: %.4lx\n"
#define MSGTR_MPCODECS_CompressorFlags "Tömörítő flag-jei: %lu, %lu verzió, ICM verzió: %lu\n"
#define MSGTR_MPCODECS_Flags "Flag-ek:"
#define MSGTR_MPCODECS_Quality " minőség"

// libmpcodecs/vf_expand.c
#define MSGTR_MPCODECS_FullDRNotPossible "A teljes DR nem lehetséges, inkább SLICES-t próbálok helyette!\n"
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupportSlices  "FIGYELEM! A következő szűrő nem támogatja a SLICES-t, készülj a sig11-re...\n"
#define MSGTR_MPCODECS_FunWhydowegetNULL "Miért kaptunk itt NULL-t??\n"

// libmpcodecs/vf_test.c, vf_yuy2.c, vf_yvu9.c
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupport "%s nem támogatott a következő szűrőben/vo-ban :(\n"

// ================================== LIBMPVO ====================================

// mga_common.c

#define MSGTR_LIBVO_MGA_ErrorInConfigIoctl "[MGA] hiba az mga_vid_config ioctl-ben (hibás verziójú mga_vid.o?)"
#define MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule "[MGA] Nem kérdezhetőek le a luma értékek a kernel modulból!\n"
#define MSGTR_LIBVO_MGA_CouldNotSetLumaValuesFromTheKernelModule "[MGA] Nem állíthatóak be a luma értékek a kernel modulból!\n"
#define MSGTR_LIBVO_MGA_ScreenWidthHeightUnknown "[MGA] Képernyő szélesség/magasság ismeretlen!\n"
#define MSGTR_LIBVO_MGA_InvalidOutputFormat "[MGA] Hibás kimeneti formátum %0X\n"
#define MSGTR_LIBVO_MGA_IncompatibleDriverVersion "[MGA] Az mga_vid vezérlőd verziója nem kompatibilis ezzel az MPlayer verzióval!\n"
#define MSGTR_LIBVO_MGA_UsingBuffers "[MGA] %d buffer használata.\n"
#define MSGTR_LIBVO_MGA_CouldntOpen "[MGA] Nem nyitható meg: %s\n"
#define MGSTR_LIBVO_MGA_ResolutionTooHigh "[MGA] A forrás felbontás legalább egy dimenzióban nagyobb, mint 1023x1023. Kérlek méretezd át szoftveresen vagy használd a -lavdopts lowres=1-t\n"

// libvo/vesa_lvo.c

#define MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported "[VESA_LVO] Ez a ág már nem támogatott.\n[VESA_LVO] Kérjük használd a -vo vesa:vidix kapcsolót helyette.\n"
#define MSGTR_LIBVO_VESA_CouldntOpen "[VESA_LVO] Nem nyitható meg: '%s'\n"
#define MSGTR_LIBVO_VESA_InvalidOutputFormat "[VESA_LVI] Hibás kimeneti formátum: %s(%0X)\n"
#define MSGTR_LIBVO_VESA_IncompatibleDriverVersion "[VESA_LVO] Az fb_vid vezérlőd verziója nem kompatibilis ezzel az MPlayer verzióval!\n"

// libvo/vo_3dfx.c

#define MSGTR_LIBVO_3DFX_Only16BppSupported "[VO_3DFX] Csak 16bpp támogatott!"
#define MSGTR_LIBVO_3DFX_VisualIdIs "[VO_3DFX] A vizuális ID  %lx.\n"
#define MSGTR_LIBVO_3DFX_UnableToOpenDevice "[VO_3DFX] A /dev/3dfx nem nyitható meg.\n"
#define MSGTR_LIBVO_3DFX_Error "[VO_3DFX] Hiba: %d.\n"
#define MSGTR_LIBVO_3DFX_CouldntMapMemoryArea "[VO_3DFX] Nem mappolhatóak a 3dfx memória területek: %p,%p,%d.\n"
#define MSGTR_LIBVO_3DFX_DisplayInitialized "[VO_3DFX] Inicializálva: %p.\n"
#define MSGTR_LIBVO_3DFX_UnknownSubdevice "[VO_3DFX] Ismeretlen aleszköz: %s.\n"

// libvo/aspect.c
#define MSGTR_LIBVO_ASPECT_NoSuitableNewResFound "[ASPECT] Figyelem: Nem található megfelelő új felbontás!\n"
#define MSGTR_LIBVO_ASPECT_NoNewSizeFoundThatFitsIntoRes "[ASPECT] Hiba: Nem található új méret, mely illeszkedne a felbontásba!\n"

// libvo/vo_dxr3.c

#define MSGTR_LIBVO_DXR3_UnableToLoadNewSPUPalette "[VO_DXR3] Sikertelen az új SPU paletta betöltése!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetPlaymode "[VO_DXR3] Sikertelen a lejátszási mód beállítása!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetSubpictureMode "[VO_DXR3] Sikertelen a subpicture mód beállítása!\n"
#define MSGTR_LIBVO_DXR3_UnableToGetTVNorm "[VO_DXR3] Sikertelen a TV norma lekérdezése!\n"
#define MSGTR_LIBVO_DXR3_AutoSelectedTVNormByFrameRate "[VO_DXR3] Auto-kiválasztásos TV norma a framerátából: "
#define MSGTR_LIBVO_DXR3_UnableToSetTVNorm "[VO_DXR3] Sikertelen a TV normba beállítása!\n"
#define MSGTR_LIBVO_DXR3_SettingUpForNTSC "[VO_DXR3] Beállítás NTSC-re.\n"
#define MSGTR_LIBVO_DXR3_SettingUpForPALSECAM "[VO_DXR3] Beállítás PAL/SECAM-ra.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo43 "[VO_DXR3] Képarány beállítása 4:3-ra.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo169 "[VO_DXR3] Képarány beállítása 16:9-re.\n"
#define MSGTR_LIBVO_DXR3_OutOfMemory "[VO_DXR3] elfogyott a memória\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateKeycolor "[VO_DXR3] Sikertelen a színkulcs lefoglalása!\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateExactKeycolor "[VO_DXR3] Sikertelen a pontos színkulcs lefoglalása, legközelebbi találat használata (0x%lx).\n"
#define MSGTR_LIBVO_DXR3_Uninitializing "[VO_DXR3] Nem inicializált.\n"
#define MSGTR_LIBVO_DXR3_FailedRestoringTVNorm "[VO_DXR3] Sikertelen a TV norma visszaállítása!\n"
#define MSGTR_LIBVO_DXR3_EnablingPrebuffering "[VO_DXR3] Előbufferelés engedélyezése.\n"
#define MSGTR_LIBVO_DXR3_UsingNewSyncEngine "[VO_DXR3] Új sync motor használata.\n"
#define MSGTR_LIBVO_DXR3_UsingOverlay "[VO_DXR3] Átlapolás használata.\n"
#define MSGTR_LIBVO_DXR3_ErrorYouNeedToCompileMplayerWithX11 "[VO_DXR3] Hiba: Az átlapoláshoz telepített lib-ek/fejléc fájlok mellett kell fordítani az MPlayert.\n"
#define MSGTR_LIBVO_DXR3_WillSetTVNormTo "[VO_DXR3] TV norma beállítása erre: "
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALPAL60 "automatikus beállítás a film frame rátájára (PAL/PAL-60)"
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALNTSC "automatikus beállítás a film frame rátájára (PAL/NTSC)"
#define MSGTR_LIBVO_DXR3_UseCurrentNorm "Jelenlegi norma használata."
#define MSGTR_LIBVO_DXR3_UseUnknownNormSuppliedCurrentNorm "Ismeretlen norma lett megadva. Aktuális norma használata."
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTrying "[VO_DXR3] Hiba a(z) %s írásra történő megnyitásakor, /dev/em8300-at próbálom helyette.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingMV "[VO_DXR3] Hiba a(z) %s írásra történő megnyitásakor, /dev/em8300_mv-t próbálom helyette.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWell "[VO_DXR3] Hiba a /dev/em8300 írásra történő megnyitásakor is!\nFeladom.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellMV "[VO_DXR3] hiba a /dev/em8300_mv írásra történő megnyitásakor is!\nFeladom.\n"
#define MSGTR_LIBVO_DXR3_Opened "[VO_DXR3] Megnyitva: %s.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingSP "[VO_DXR3] Hiba %s írásra történő megnyitásakor, /dev/em8300_sp-t próbálom helyette.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellSP "[VO_DXR3] Hiba a /dev/em8300_sp írásra történő megnyitásakor is!\nFeladom.\n"
#define MSGTR_LIBVO_DXR3_UnableToOpenDisplayDuringHackSetup "[VO_DXR3] Nem nyitható meg a képernyő az overlay hack beállítása alatt!\n"
#define MSGTR_LIBVO_DXR3_UnableToInitX11 "[VO_DXR3] Nem sikerült az X11 inicializálása!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayAttribute "[VO_DXR3] Sikertelen az átlapolási attribútumok beállítása.\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayScreen "[VO_DXR3] Sikertelen az átlapolt képernyő beállítása!\nKilépés.\n"
#define MSGTR_LIBVO_DXR3_FailedEnablingOverlay "[VO_DXR3] Sikertelen az átlapolás bekapcsolása!\nKilépés.\n"
#define MSGTR_LIBVO_DXR3_FailedResizingOverlayWindow "[VO_DXR3] Sikertelen az átlapolt ablak átméretezése!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayBcs "[VO_DXR3] Sikertelen az átlapolási bcs beállítása!\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayYOffsetValues "[VO_DXR3] Sikertelen az átlapolás Y-offset értékének beállítása!\nKilépés.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXOffsetValues "[VO_DXR3] Sikertelen az átlapolás X-offset értékének beállítása!\nKilépés.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXScaleCorrection "[VO_DXR3] Sikertelen az átlapolás X arány korrekciójának lekérdezése!\nKilépés.\n"
#define MSGTR_LIBVO_DXR3_YOffset "[VO_DXR3] Yoffset: %d.\n"
#define MSGTR_LIBVO_DXR3_XOffset "[VO_DXR3] Xoffset: %d.\n"
#define MSGTR_LIBVO_DXR3_XCorrection "[VO_DXR3] Xcorrection: %d.\n"
#define MSGTR_LIBVO_DXR3_FailedSetSignalMix "[VO_DXR3] Sikertelen a kevert jel beállítása!\n"

// libvo/vo_mga.c

#define MSGTR_LIBVO_MGA_AspectResized "[VO_MGA] aspect(): átméretezve erre: %dx%d.\n"
#define MSGTR_LIBVO_MGA_Uninit "[VO] uninit!\n"

// libvo/vo_null.c

#define MSGTR_LIBVO_NULL_UnknownSubdevice "[VO_NULL] Ismeretlen aleszköz: %s.\n"

// libvo/vo_png.c

#define MSGTR_LIBVO_PNG_Warning1 "[VO_PNG] Figyelmeztetés: a tömörítési szint 0-ra állítva, tömörítés kikapcsolva!\n"
#define MSGTR_LIBVO_PNG_Warning2 "[VO_PNG] Infó: Használd a -vo png:z=<n> opciót a tömörítési szint beállításához 0-tól 9-ig.\n"
#define MSGTR_LIBVO_PNG_Warning3 "[VO_PNG] Infó: (0 = nincs tömörítés, 1 = leggyorsabb, legrosszabb - 9 legjobb, leglassabb tömörítés)\n"
#define MSGTR_LIBVO_PNG_ErrorOpeningForWriting "\n[VO_PNG] Hiba a(z) '%s' írásra történő megnyitásakor!\n"
#define MSGTR_LIBVO_PNG_ErrorInCreatePng "[VO_PNG] Hiba a create_png-ben.\n"

// libvo/vo_sdl.c

#define MSGTR_LIBVO_SDL_CouldntGetAnyAcceptableSDLModeForOutput "[VO_SDL] Sikertelen bármilyen elfogadható SDL mód lekérdezése a kimenethez.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailed "[VO_SDL] set_video_mode: SDL_SetVideoMode sikertelen: %s.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailedFull "[VO_SDL] Set_fullmode: SDL_SetVideoMode sikertelen: %s.\n"
#define MSGTR_LIBVO_SDL_MappingI420ToIYUV "[VO_SDL] I420 mappolása IYUV.\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormat "[VO_SDL] Nem támogatott kép formátum (0x%X).\n"
#define MSGTR_LIBVO_SDL_InfoPleaseUseVmOrZoom "[VO_SDL] Infó - Kérlek használd a -vm vagy -zoom opciót a legjobb felbontásra váltáshoz.\n"
#define MSGTR_LIBVO_SDL_FailedToSetVideoMode "[VO_SDL] Sikertelen a videó mód beállítása: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateAYUVOverlay "[VO_SDL] Nem hozható létre a YUV átlapolás: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateARGBSurface "[VO_SDL] Nem hozható létre az RGB felület: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDepthColorspaceConversion "[VO_SDL] Mélység/színtér konverzió használata, ez lelassítja a dolgokat (%ibpp -> %ibpp).\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormatInDrawslice "[VO_SDL] Nem támogatott kép formátum a draw_slice-ban, lépj kapcsolatba az MPlayer fejlesztőkkel!\n"
#define MSGTR_LIBVO_SDL_BlitFailed "[VO_SDL] Blit sikertelen: %s.\n"
#define MSGTR_LIBVO_SDL_InitializationFailed "[VO_SDL] SDL inicializálása sikertelen: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDriver "[VO_SDL] Használt vezérlő: %s.\n"

// libvo/vobsub_vidix.c

#define MSGTR_LIBVO_SUB_VIDIX_CantStartPlayback "[VO_SUB_VIDIX] Nem indítható el a lejátszás: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantStopPlayback "[VO_SUB_VIDIX] Nem állítható meg a lejátszás: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_InterleavedUvForYuv410pNotSupported "[VO_SUB_VIDIX] Az átlapolt UV a YUV410P-hez nem támogatott.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawsliceWasCalled "[VO_SUB_VIDIX] Üres vidix_draw_slice() meghívva.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawframeWasCalled "[VO_SUB_VIDIX] Üres vidix_draw_frame() meghívva.\n"
#define MSGTR_LIBVO_SUB_VIDIX_UnsupportedFourccForThisVidixDriver "[VO_SUB_VIDIX] Nem támogatott FourCC ehhez a VIDIX vezérlőhöz: %x (%s).\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedResolution "[VO_SUB_VIDIX] A videó szerver felbontása (%dx%d) nem támogatott, a támogatott: %dx%d-%dx%d.\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedColorDepth "[VO_SUB_VIDIX] A videó szerver vidix által nem támogatott színmélységet használ (%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantUpscaleImage "[VO_SUB_VIDIX] A VIDIX vezérlő nem tudja felméretezni a képet (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantDownscaleImage "[VO_SUB_VIDIX] A VIDIX vezérlő nem tudja leméretezni a képet (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantConfigurePlayback "[VO_SUB_VIDIX] Nem állítható be a lejátszás: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_YouHaveWrongVersionOfVidixLibrary "[VO_SUB_VIDIX] Rossz verziójú VIDIX függvénykönyvtárad van.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntFindWorkingVidixDriver "[VO_SUB_VIDIX] Nem található működő VIDIX vezérlő.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntGetCapability "[VO_SUB_VIDIX] Nem elérhető képesség: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_Description "[VO_SUB_VIDIX] Leírás: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_Author "[VO_SUB_VIDIX] Szerző: %s.\n"

// libvo/vo_svga.c

#define MSGTR_LIBVO_SVGA_ForcedVidmodeNotAvailable "[VO_SVGA] Kényszerített vid_mode %d (%s) nem elérhető.\n"
#define MSGTR_LIBVO_SVGA_ForcedVidmodeTooSmall "[VO_SVGA] Kényszerített vid_mode %d (%s) túl kicsi.\n"
#define MSGTR_LIBVO_SVGA_Vidmode "[VO_SVGA] Vid_mode: %d, %dx%d %dbpp.\n"
#define MSGTR_LIBVO_SVGA_VgasetmodeFailed "[VO_SVGA] Vga_setmode(%d) sikertelen.\n"
#define MSGTR_LIBVO_SVGA_VideoModeIsLinearAndMemcpyCouldBeUsed "[VO_SVGA] A videó mód lineáris, a memcpy használható a kép átvitelre.\n"
#define MSGTR_LIBVO_SVGA_VideoModeHasHardwareAcceleration "[VO_SVGA] A videó módnak hardveres gyorsítása van, a put_image használható.\n"
#define MSGTR_LIBVO_SVGA_IfItWorksForYouIWouldLikeToKnow "[VO_SVGA] Ha működik nálad, szeretnénk tudni róla.\n[VO_SVGA] (küldj logot: `mplayer test.avi -v -v -v -v &> svga.log`). Thx!\n"
#define MSGTR_LIBVO_SVGA_VideoModeHas "[VO_SVGA] A videó módnak %d lapja van.\n"
#define MSGTR_LIBVO_SVGA_CenteringImageStartAt "[VO_SVGA] Kép középre igazítása. Kezdőpont (%d,%d)\n"
#define MSGTR_LIBVO_SVGA_UsingVidix "[VO_SVGA] VIDIX használata. w=%i h=%i  mw=%i mh=%i\n"

// libvo/vo_syncfb.c

#define MSGTR_LIBVO_SYNCFB_CouldntOpen "[VO_SYNCFB] A /dev/syncfb vagy a /dev/mga_vid nem nyitható meg.\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p3 "[VO_SYNCFB] YUV420P3 paletta használata.\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420p2 "[VO_SYNCFB] YUV420P2 paletta használata.\n"
#define MSGTR_LIBVO_SYNCFB_UsingPaletteYuv420 "[VO_SYNCFB] YUV420 paletta használata.\n"
#define MSGTR_LIBVO_SYNCFB_NoSupportedPaletteFound "[VO_SYNCFB] Nem találtam támogatott palettát.\n"
#define MSGTR_LIBVO_SYNCFB_BesSourcerSize "[VO_SYNCFB] BES sourcer méret: %d x %d.\n"
#define MSGTR_LIBVO_SYNCFB_FramebufferMemory "[VO_SYNCFB] framebuffer memória: %ld, %ld bufferben.\n"
#define MSGTR_LIBVO_SYNCFB_RequestingFirstBuffer "[VO_SYNCFB] #%d. számú első buffer igénylése.\n"
#define MSGTR_LIBVO_SYNCFB_GotFirstBuffer "[VO_SYNCFB] #%d. számú első buffer megvan.\n"
#define MSGTR_LIBVO_SYNCFB_UnknownSubdevice "[VO_SYNCFB] ismeretlen aleszköz: %s.\n"

// libvo/vo_tdfxfb.c

#define MSGTR_LIBVO_TDFXFB_CantOpen "[VO_TDFXFB] Nem nyitható meg %s: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetFscreenInfo "[VO_TDFXFB] Probléma az FBITGET_FSCREENINFO ioctl-lel: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetVscreenInfo "[VO_TDFXFB] Probléma az FBITGET_VSCREENINFO ioctl-lel: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ThisDriverOnlySupports "[VO_TDFXFB] Ez a vezérlő csak a 3Dfx Banshee-t, a Voodoo3-at és a Voodoo 5-öt támogatja.\n"
#define MSGTR_LIBVO_TDFXFB_OutputIsNotSupported "[VO_TDFXFB] %d bpp-s kimenet nem támogatott.\n"
#define MSGTR_LIBVO_TDFXFB_CouldntMapMemoryAreas "[VO_TDFXFB] Nem mappolhatóak a memóriaterületek: %s.\n"
#define MSGTR_LIBVO_TDFXFB_BppOutputIsNotSupported "[VO_TDFXFB] %d bpp-s kimenet nem támogatott (Ennek soha nem szabad megtörténnie).\n"
#define MSGTR_LIBVO_TDFXFB_SomethingIsWrongWithControl "[VO_TDFXFB] Eik! Valami baj van a control()-lal.\n"
#define MSGTR_LIBVO_TDFXFB_NotEnoughVideoMemoryToPlay "[VO_TDFXFB] Nincs elég videó memória ezen film lejátszásához. Próbáld meg csökkenteni a felbontást.\n"
#define MSGTR_LIBVO_TDFXFB_ScreenIs "[VO_TDFXFB] A képernyő %dx%d %d bpp-vel, ezen van %dx%d %d bpp-vel, a norma %dx%d.\n"

// libvo/vo_tdfx_vid.c

#define MSGTR_LIBVO_TDFXVID_Move "[VO_TDXVID] Mozgatás: %d(%d) x %d => %d.\n"
#define MSGTR_LIBVO_TDFXVID_AGPMoveFailedToClearTheScreen "[VO_TDFXVID] AGP mozgatás sikertelen a képernyő törléséhez.\n"
#define MSGTR_LIBVO_TDFXVID_BlitFailed "[VO_TDFXVID] Blit sikertelen.\n"
#define MSGTR_LIBVO_TDFXVID_NonNativeOverlayFormatNeedConversion "[VO_TDFXVID] Nem-natív átlapolási formátumhoz konverzió kell.\n"
#define MSGTR_LIBVO_TDFXVID_UnsupportedInputFormat "[VO_TDFXVID] Nem támogatott bemeneti formátum 0x%x.\n"
#define MSGTR_LIBVO_TDFXVID_OverlaySetupFailed "[VO_TDFXVID] Átlapolás beállítása sikertelen.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOnFailed "[VO_TDFXVID] Átlapolás bekapcsolása sikertelen.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayReady "[VO_TDFXVID] Átlapolás kész: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_TextureBlitReady "[VO_TDFXVID] Textúra blit kész: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOffFailed "[VO_TDFXVID] Átlapolás kikapcsolása sikertelen\n"
#define MSGTR_LIBVO_TDFXVID_CantOpen "[VO_TDFXVID] Nem nyitható meg %s: %s.\n"
#define MSGTR_LIBVO_TDFXVID_CantGetCurrentCfg "[VO_TDFXVID] Nem található az aktuális konfiguráció: %s.\n"
#define MSGTR_LIBVO_TDFXVID_MemmapFailed "[VO_TDFXVID] Memmap sikertelen !!!!!\n"
#define MSGTR_LIBVO_TDFXVID_GetImageTodo "Get image todo.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailed "[VO_TDFXVID] AGP mozgatás sikertelen.\n"
#define MSGTR_LIBVO_TDFXVID_SetYuvFailed "[VO_TDFXVID] YUV beállítása sikertelen.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnYPlane "[VO_TDFXVID] AGP mozgatás sikertelen az Y síkon.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnUPlane "[VO_TDFXVID] AGP mozgatás sikertelen az U síkon.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnVPlane "[VO_TDFXVID] AGP mozgatás sikertelen a V síkon.\n"
#define MSGTR_LIBVO_TDFXVID_UnknownFormat "[VO_TDFXVID] ismeretlen formátum: 0x%x.\n"

// libvo/vo_tga.c

#define MSGTR_LIBVO_TGA_UnknownSubdevice "[VO_TGA] Ismeretlen aleszköz: %s.\n"

// libvo/vo_vesa.c

#define MSGTR_LIBVO_VESA_FatalErrorOccurred "[VO_VESA] Végzetes hiba történt! Nem lehet folytatni.\n"
#define MSGTR_LIBVO_VESA_UnknownSubdevice "[VO_VESA] ismeretlen aleszköz: '%s'.\n"
#define MSGTR_LIBVO_VESA_YouHaveTooLittleVideoMemory "[VO_VESA] Túl kevés videó memóriád van ehhez a módhoz:\n[VO_VESA] Szükséges: %08lX rendelkezésre áll: %08lX.\n"
#define MSGTR_LIBVO_VESA_YouHaveToSpecifyTheCapabilitiesOfTheMonitor "[VO_VESA] Meg kell adnod a monitor adatait. Nem változott a frissítési ráta.\n"
#define MSGTR_LIBVO_VESA_UnableToFitTheMode "[VO_VESA] A mód nem felel meg a monitor korlátainak. Nem változott a frissítési ráta.\n"
#define MSGTR_LIBVO_VESA_DetectedInternalFatalError "[VO_VESA] Végzetes belső hibát találtam: az init a preinit előtt lett meghívva.\n"
#define MSGTR_LIBVO_VESA_SwitchFlipIsNotSupported "[VO_VESA] A -flip kapcsoló nem támogatott.\n"
#define MSGTR_LIBVO_VESA_PossibleReasonNoVbe2BiosFound "[VO_VESA] Lehetséges ok: Nem található VBE2 BIOS.\n"
#define MSGTR_LIBVO_VESA_FoundVesaVbeBiosVersion "[VO_VESA] VESA VBE BIOS Version %x.%x Revision: %x található.\n"
#define MSGTR_LIBVO_VESA_VideoMemory "[VO_VESA] Videó memória: %u Kb.\n"
#define MSGTR_LIBVO_VESA_Capabilites "[VO_VESA] VESA Képességek: %s %s %s %s %s.\n"
#define MSGTR_LIBVO_VESA_BelowWillBePrintedOemInfo "[VO_VESA] !!! Az OEM infó kiírása következik !!!\n"
#define MSGTR_LIBVO_VESA_YouShouldSee5OemRelatedLines "[VO_VESA] 5 OEM-mel kapcsolatos sort kell látnod ez alatt; ha nem, akkor hibás a vm86-od.\n"
#define MSGTR_LIBVO_VESA_OemInfo "[VO_VESA] OEM infó: %s.\n"
#define MSGTR_LIBVO_VESA_OemRevision "[VO_VESA] OEM Revision: %x.\n"
#define MSGTR_LIBVO_VESA_OemVendor "[VO_VESA] OEM szállító: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductName "[VO_VESA] OEM Termék Neve: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductRev "[VO_VESA] OEM Termék Rev: %s.\n"
#define MSGTR_LIBVO_VESA_Hint "[VO_VESA] Tanács: Működő TV-Kimenethez a PC boot-olásakor már bedugott\n"\
"[VO_VESA] TV-csatlakozó kell, mivel a VESA BIOS inicializálja azt saját maga a POST során.\n"
#define MSGTR_LIBVO_VESA_UsingVesaMode "[VO_VESA] VESA mód (%u) használata = %x [%ux%u@%u]\n"
#define MSGTR_LIBVO_VESA_CantInitializeSwscaler "[VO_VESA] A szoftveres méretező nem inicializálható.\n"
#define MSGTR_LIBVO_VESA_CantUseDga "[VO_VESA] A DGA nem használható. Bank váltásos mód kényszerítése. :(\n"
#define MSGTR_LIBVO_VESA_UsingDga "[VO_VESA] DGA használata (fizikai erőforrások: %08lXh, %08lXh)"
#define MSGTR_LIBVO_VESA_CantUseDoubleBuffering "[VO_VESA] Nem használható a dupla bufferelés: nincs elég videó memória.\n"
#define MSGTR_LIBVO_VESA_CantFindNeitherDga "[VO_VESA] Nem található sem DGA, sem áthelyezhető ablak keret.\n"
#define MSGTR_LIBVO_VESA_YouveForcedDga "[VO_VESA] DGA-t kényszerítettél. Kilépés\n"
#define MSGTR_LIBVO_VESA_CantFindValidWindowAddress "[VO_VESA] Nem található helyes ablak cím.\n"
#define MSGTR_LIBVO_VESA_UsingBankSwitchingMode "[VO_VESA] Bank váltás mód használata (fizikai erőforrások: %08lXh, %08lXh).\n"
#define MSGTR_LIBVO_VESA_CantAllocateTemporaryBuffer "[VO_VESA] Nem foglalható le az ideiglenes buffer.\n"
#define MSGTR_LIBVO_VESA_SorryUnsupportedMode "[VO_VESA] Bocs, nem támogatott mód -- próbáld a -x 640 -zoom opciókat.\n"
#define MSGTR_LIBVO_VESA_OhYouReallyHavePictureOnTv "[VO_VESA] Óó tényleg van képed a TV-n!\n"
#define MSGTR_LIBVO_VESA_CantInitialozeLinuxVideoOverlay "[VO_VESA] A Linux Video Overlay nem inicializálható.\n"
#define MSGTR_LIBVO_VESA_UsingVideoOverlay "[VO_VESA] Videó átlapolás használata: %s.\n"
#define MSGTR_LIBVO_VESA_CantInitializeVidixDriver "[VO_VESA] Nem inicializálható a VIDIX vezérlő.\n"
#define MSGTR_LIBVO_VESA_UsingVidix "[VO_VESA] VIDIX használata.\n"
#define MSGTR_LIBVO_VESA_CantFindModeFor "[VO_VESA] Nem található mód ehhez: %ux%u@%u.\n"
#define MSGTR_LIBVO_VESA_InitializationComplete "[VO_VESA] VESA inicializálás kész.\n"

// libvo/vo_x11.c

#define MSGTR_LIBVO_X11_DrawFrameCalled "[VO_X11] draw_frame() meghívva!!!!!!\n"

// libvo/vo_xv.c

#define MSGTR_LIBVO_XV_DrawFrameCalled "[VO_XV] draw_frame() meghívva!!!!!!\n"
#define MSGTR_LIBVO_XV_SharedMemoryNotSupported "[VO_XV] Az osztott memória nem támogatott\nVisszatérés a normál Xv-hez.\n"
#define MSGTR_LIBVO_XV_XvNotSupportedByX11 "[VO_XV] Bocs, az Xv nem támogatott ezen X11 verzióval/vezérlővel\n[VO_XV] ******** Próbáld ki:  -vo x11  vagy  -vo sdl  *********\n"
#define MSGTR_LIBVO_XV_XvQueryAdaptorsFailed  "[VO_XV] XvQueryAdaptors sikertelen.\n"
#define MSGTR_LIBVO_XV_InvalidPortParameter "[VO_XV] Hibás port paraméter, felülbírálva port 0-val.\n"
#define MSGTR_LIBVO_XV_CouldNotGrabPort "[VO_XV] A(z) %i port nem foglalható le.\n"
#define MSGTR_LIBVO_XV_CouldNotFindFreePort "[VO_XV] Nem található szabad Xvideo port - talán egy másik processz már\n"\
"[VO_XV] használja. Zárj be minden videó alkalmazást és próbáld újra. Ha ez nem segít,\n"\
"[VO_XV] nézd meg az 'mplayer -vo help'-et más (nem-xv) videó kimeneti vezérlőkért.\n"
#define MSGTR_LIBVO_XV_NoXvideoSupport "[VO_XV] Úgy tűnik, hogy nincs Xvideo támogatás a videó kártyádhoz.\n"\
"[VO_XV] Futtasd le az 'xvinfo'-t és ellenőrizd az Xv támogatását,\n"\
"[VO_XV] majd olvasd el a DOCS/HTML/hu/video.html#xv fájlt!\n"\
"[VO_XV] Lásd az 'mplayer -vo help'-et más (nem-xv) videó kimeneti vezérlőkért.\n"\
"[VO_XV] Próbáld ki a -vo x11 -et.\n"


// loader/ldt_keeper.c

#define MSGTR_LOADER_DYLD_Warning "FIGYELMEZTETÉS: DLL codec-ek használatának kísérlete, de a\n         DYLD_BIND_AT_LAUNCH környezeti változó nincs beállítva. Így ez összeomlást okoz.\n"

// stream/stream_radio.c

#define MSGTR_RADIO_ChannelNamesDetected "[radio] Rádió csatornák neve megtalálva.\n"
#define MSGTR_RADIO_FreqRange "[radio] Az engedélyezett frekvencia tartomány %.2f-%.2f MHz.\n"
#define MSGTR_RADIO_WrongFreqForChannel "[radio] Hibás frekvencia a(z) %s csatornának\n"
#define MSGTR_RADIO_WrongChannelNumberFloat "[radio] Hibás csatorna szám: %.2f\n"
#define MSGTR_RADIO_WrongChannelNumberInt "[radio] Hibás csatorna szám: %d\n"
#define MSGTR_RADIO_WrongChannelName "[radio] Hibás csatorna név: %s\n"
#define MSGTR_RADIO_FreqParameterDetected "[radio] Rádió frekvencia paramétere megtalálva.\n"
#define MSGTR_RADIO_DoneParsingChannels "[radio] Csatornák értelmezése kész.\n"
#define MSGTR_RADIO_GetTunerFailed "[radio] Figyelmeztetés:ioctl get tuner sikertelen: %s. Frac beállítása: %d.\n"
#define MSGTR_RADIO_NotRadioDevice "[radio] %s nem rádiós eszköz!\n"
#define MSGTR_RADIO_TunerCapLowYes "[radio] a tuner low:yes frac=%d\n"
#define MSGTR_RADIO_TunerCapLowNo "[radio] a tuner low:no frac=%d\n"
#define MSGTR_RADIO_SetFreqFailed "[radio] ioctl set frequency 0x%x (%.2f) sikertelen: %s\n"
#define MSGTR_RADIO_GetFreqFailed "[radio] ioctl get frequency sikertelen: %s\n"
#define MSGTR_RADIO_SetMuteFailed "[radio] ioctl set mute sikertelen: %s\n"
#define MSGTR_RADIO_QueryControlFailed "[radio] ioctl query control sikertelen: %s\n"
#define MSGTR_RADIO_GetVolumeFailed "[radio] ioctl get volume sikertelen: %s\n"
#define MSGTR_RADIO_SetVolumeFailed "[radio] ioctl set volume sikertelen: %s\n"
#define MSGTR_RADIO_DroppingFrame "\n[radio] túl rossz - audió keret eldobása (%d bájt)!\n"
#define MSGTR_RADIO_BufferEmpty "[radio] grab_audio_frame: üres a buffer, várakozás %d adat bájtra.\n"
#define MSGTR_RADIO_AudioInitFailed "[radio] audio_in_init sikertelen: %s\n"
#define MSGTR_RADIO_AudioBuffer "[radio] Audió rögzítés - buffer=%d bájt (blokk=%d bájt).\n"
#define MSGTR_RADIO_AllocateBufferFailed "[radio] az audió buffer nem foglalható le (block=%d,buf=%d): %s\n"
#define MSGTR_RADIO_CurrentFreq "[radio] Jelenlegi frekvencia: %.2f\n"
#define MSGTR_RADIO_SelectedChannel "[radio] Kiválasztott csatorna: %d - %s (freq: %.2f)\n"
#define MSGTR_RADIO_ChangeChannelNoChannelList "[radio] Nem lehet csatornát választani: nincs csatornalista megadva.\n"
#define MSGTR_RADIO_UnableOpenDevice "[radio] '%s' nem nyitható meg: %s\n"
#define MSGTR_RADIO_RadioDevice "[radio] Radio fd: %d, %s\n"
#define MSGTR_RADIO_InitFracFailed "[radio] init_frac sikertelen.\n"
#define MSGTR_RADIO_WrongFreq "[radio] Hibás frekvencia: %.2f\n"
#define MSGTR_RADIO_UsingFreq "[radio] Használt frekvencia: %.2f.\n"
#define MSGTR_RADIO_AudioInInitFailed "[radio] audio_in_init sikertelen.\n"
#define MSGTR_RADIO_BufferString "[radio] %s: in buffer=%d dropped=%d\n"
#define MSGTR_RADIO_AudioInSetupFailed "[radio] audio_in_setup hívás sikertelen: %s\n"
#define MSGTR_RADIO_CaptureStarting "[radio] Mentés kezdése.\n"
#define MSGTR_RADIO_ClearBufferFailed "[radio] Buffer kiürítése sikertelen: %s\n"
#define MSGTR_RADIO_StreamEnableCacheFailed "[radio] stream_enable_cache hívás sikertelen: %s\n"
#define MSGTR_RADIO_DriverUnknownStr "[radio] Ismeretlen vezérlő név: %s\n"
#define MSGTR_RADIO_DriverV4L2 "[radio] V4Lv2 rádió interfész használata.\n"
#define MSGTR_RADIO_DriverV4L "[radio] V4Lv1 rádió interfész használata.\n"
#define MSGTR_RADIO_DriverBSDBT848 "[radio] *BSD BT848 rádió interfész használata.\n"
#define MSGTR_RADIO_AvailableDrivers "[radio] Használható vezérlők: "

// ================================== LIBASS ====================================

// ass_bitmap.c
#define MSGTR_LIBASS_FT_Glyph_To_BitmapError "[ass] FT_Glyph_To_Bitmap hiba %d \n"
#define MSGTR_LIBASS_UnsupportedPixelMode "[ass] Nem támogatott pixel mód: %d\n"

// ass.c
#define MSGTR_LIBASS_NoStyleNamedXFoundUsingY "[ass] [%p] Figyelmeztetés: nincs '%s' nevű stílus, '%s' használata\n"
#define MSGTR_LIBASS_BadTimestamp "[ass] hibás időbélyeg\n"
#define MSGTR_LIBASS_BadEncodedDataSize "[ass] rossz kódolt adatméret\n"
#define MSGTR_LIBASS_FontLineTooLong "[ass] Betűtípus sor túl hosszú: %d, %s\n"
#define MSGTR_LIBASS_EventFormatHeaderMissing "[ass] Esemény formátum fejléc hiányzik\n"
#define MSGTR_LIBASS_ErrorOpeningIconvDescriptor "[ass] hiba az iconv leíró megnyitásakor.\n"
#define MSGTR_LIBASS_ErrorRecodingFile "[ass] hiba a fájl rögzítésekor.\n"
#define MSGTR_LIBASS_FopenFailed "[ass] ass_read_file(%s): fopen sikertelen\n"
#define MSGTR_LIBASS_FseekFailed "[ass] ass_read_file(%s): fseek sikertelen\n"
#define MSGTR_LIBASS_RefusingToLoadSubtitlesLargerThan10M "[ass] ass_read_file(%s): 10M-nél nagyobb felirat fájl betöltése visszautasítva\n"
#define MSGTR_LIBASS_ReadFailed "Olvasás sikertelen, %d: %s\n"
#define MSGTR_LIBASS_AddedSubtitleFileMemory "[ass] Felirat fájl hozzáadása: <memória> (%d stílus, %d esemény)\n"
#define MSGTR_LIBASS_AddedSubtitleFileFname "[ass] Felirat fájl hozzáadása: %s (%d stílus, %d esemény)\n"
#define MSGTR_LIBASS_FailedToCreateDirectory "[ass] Sikertelen a(z) %s könyvtár létrehozása\n"
#define MSGTR_LIBASS_NotADirectory "[ass] Nem könyvtár: %s\n"

// ass_cache.c
#define MSGTR_LIBASS_TooManyFonts "[ass] Túl sok betűtípus\n"
#define MSGTR_LIBASS_ErrorOpeningFont "[ass] Hiba a betűtípus megnyitásakor: %s, %d\n"

// ass_fontconfig.c
#define MSGTR_LIBASS_SelectedFontFamilyIsNotTheRequestedOne "[ass] fontconfig: A kiválasztott betűtípus család nem a kért: '%s' != '%s'\n"
#define MSGTR_LIBASS_UsingDefaultFontFamily "[ass] fontconfig_select: Alapértelmezett betűtípus család használata: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingDefaultFont "[ass] fontconfig_select: Alapértelmezett betűtípus használata: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingArialFontFamily "[ass] fontconfig_select: 'Arial' betűtípus család használata: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_FcInitLoadConfigAndFontsFailed "[ass] FcInitLoadConfigAndFonts sikertelen.\n"
#define MSGTR_LIBASS_UpdatingFontCache "[ass] Betűtípus cache frissítése.\n"
#define MSGTR_LIBASS_BetaVersionsOfFontconfigAreNotSupported "[ass] A fontconfig béta verziói nem támogatottak.\n[ass] Frissíts, mielőtt hibát jelentesz.\n"
#define MSGTR_LIBASS_FcStrSetAddFailed "[ass] FcStrSetAdd sikertelen.\n"
#define MSGTR_LIBASS_FcDirScanFailed "[ass] FcDirScan sikertelen.\n"
#define MSGTR_LIBASS_FcDirSave "[ass] FcDirSave sikertelen.\n"
#define MSGTR_LIBASS_FcConfigAppFontAddDirFailed "[ass] FcConfigAppFontAddDir sikertelen\n"
#define MSGTR_LIBASS_FontconfigDisabledDefaultFontWillBeUsed "[ass] Fontconfig letiltva, csak az alapértelmezett betűtípus használható.\n"
#define MSGTR_LIBASS_FunctionCallFailed "[ass] %s sikertelen\n"

// ass_render.c
#define MSGTR_LIBASS_NeitherPlayResXNorPlayResYDefined "[ass] Sem a PlayResX sem a PlayResY nincs definiálva. 384x288 a feltételezett.\n"
#define MSGTR_LIBASS_PlayResYUndefinedSettingY "[ass] PlayResY nem definiált, beállított érték: %d.\n"
#define MSGTR_LIBASS_PlayResXUndefinedSettingX "[ass] PlayResX nem definiált, beállított érték: %d.\n"
#define MSGTR_LIBASS_FT_Init_FreeTypeFailed "[ass] FT_Init_FreeType sikertelen.\n"
#define MSGTR_LIBASS_Init "[ass] Inicializálás\n"
#define MSGTR_LIBASS_InitFailed "[ass] Inicializálás sikertelen.\n"
#define MSGTR_LIBASS_BadCommand "[ass] Hibás parancs: %c%c\n"
#define MSGTR_LIBASS_ErrorLoadingGlyph  "[ass] Hiba a jel betöltésekor.\n"
#define MSGTR_LIBASS_FT_Glyph_Stroke_Error "[ass] FT_Glyph_Stroke %d hiba \n"
#define MSGTR_LIBASS_UnknownEffectType_InternalError "[ass] Ismeretlen effekt típus (belső hiba)\n"
#define MSGTR_LIBASS_NoStyleFound "[ass] Nem található stílus!\n"
#define MSGTR_LIBASS_EmptyEvent "[ass] Üres esemény!\n"
#define MSGTR_LIBASS_MAX_GLYPHS_Reached "[ass] MAX_GLYPHS elérve: %d esemény, start = %llu, tartam = %llu\n Szöveg = %s\n"
#define MSGTR_LIBASS_EventHeightHasChanged "[ass] Figyelem! Esemény magassága megváltozott!  \n"

// ass_font.c
#define MSGTR_LIBASS_GlyphNotFoundReselectingFont "[ass] 0x%X jel nem található, betűtípus újraválasztása ehhez: (%s, %d, %d)\n"
#define MSGTR_LIBASS_GlyphNotFound "[ass] 0x%X jel nem található a betűtípusban ehhez: (%s, %d, %d)\n"
#define MSGTR_LIBASS_ErrorOpeningMemoryFont "[ass] Hiba a betűtípus memóriában történő megnyitásakor: %s\n"
#define MSGTR_LIBASS_NoCharmaps "[ass] betűtípus leírás karaktertábla nélkül\n"
#define MSGTR_LIBASS_NoCharmapAutodetected "[ass] nincs alapértelmezetten megtalált karaktertábla, az elsőt próbálom\n"
