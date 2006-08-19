// Translated by:  Daniel Beòa, benad (at) centrum.cz
// last sync on 2006-04-28 with 1.249
// but not compleated

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
// Preklad do slovenèiny 

static char help_text[]=
"Pou¾itie:   mplayer [prepínaèe] [url|cesta/]menosúboru\n"
"\n"
"Základné prepínaèe: (Kompletný zoznam nájdete v man stránke)\n"
" -vo <drv[:dev]> výber výstup. video ovládaèa&zariadenia (-vo help pre zoznam)\n"
" -ao <drv[:dev]> výber výstup. audio ovládaèa&zariadenia (-ao help pre zoznam)\n"
#ifdef HAVE_VCD
" vcd://<trackno>  prehra» VCD (video cd) stopu zo zariadenia namiesto zo súboru\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titleno>  prehra» DVD titul/stopu zo zariadenia (mechaniky) namiesto súboru\n"
" -alang/-slang   vybra» jazyk DVD zvuku/titulkov(pomocou 2-miest. kódu krajiny)\n"
#endif
" -ss <timepos>   posun na pozíciu (sekundy alebo hh:mm:ss)\n"
" -nosound        prehráva» bez zvuku\n"
" -fs             voµby pre celú obrazovku (alebo -vm -zoom, detaily viï. man stránku)\n"
" -x <x> -y <y>   zväè¹enie obrazu na rozmer <x>*<y> (pokiaµ to vie -vo ovládaè!)\n"
" -sub <file>     voµba súboru s titulkami (viï tie¾ -subfps, -subdelay)\n"
" -playlist <file> urèenie súboru so zoznamom prehrávaných súborov\n"
" -vid x -aid y   výber èísla video (x) a audio (y) prúdu pre prehrávanie\n"
" -fps x -srate y voµba pre zmenu video (x fps) a audio (y Hz) frekvencie\n"
" -pp <quality>   aktivácia postprocesing filtra (0-4 pre DivX, 0-63 pre mpegy)\n"
" -framedrop      povoli» zahadzovanie snímkov (pre pomalé stroje)\n"
"\n"
"Zákl. klávesy:   (pre kompl. pozrite aj man stránku a input.conf)\n"
" <-  alebo  ->   posun vzad/vpred o 10 sekund\n"
" hore / dole     posun vzad/vpred o  1 minútu\n"
" pgup alebo pgdown  posun vzad/vpred o 10 minút\n"
" < alebo >       posun vzad/vpred v zozname prehrávaných súborov\n"
" p al. medzerník pauza (pokraèovanie stlaèením klávesy)\n"
" q alebo ESC     koniec prehrávania a ukonèenie programu\n"
" + alebo -       upravi» spozdenie zvuku v krokoch +/- 0.1 sekundy\n"
" o               cyklická zmena re¾imu OSD:  niè / pozícia / pozícia+èas\n"
" * alebo /       prida» alebo ubra» hlasitos» (stlaèením 'm' výber master/pcm)\n"
" z alebo x       upravi» spozdenie titulkov v krokoch +/- 0.1 sekundy\n"
" r alebo t       upravi» pozíciu titulkov hore/dole, pozrite tie¾ -vf!\n"
"\n"
" * * * * PREÈÍTAJTE SI MAN STRÁNKU PRE DETAILY (ÏAL©IE VO¥BY A KLÁVESY)! * * * *\n"
"\n";
#endif

#define MSGTR_SamplesWanted "Potrebujeme vzorky tohto formátu, aby sme zlep¹ili podporu. Prosím kontaktujte vývojárov.\n"

// ========================= MPlayer messages ===========================
// mplayer.c:

#define MSGTR_Exiting "\nKonèím...\n"
#define MSGTR_ExitingHow "\nKonèím... (%s)\n"
#define MSGTR_Exit_quit "Koniec"
#define MSGTR_Exit_eof "Koniec súboru"
#define MSGTR_Exit_error "Záva¾ná chyba"
#define MSGTR_IntBySignal "\nMPlayer preru¹ený signálom %d v module: %s \n"
#define MSGTR_NoHomeDir "Nemô¾em najs» domáci (HOME) adresár\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problém\n"
#define MSGTR_CreatingCfgFile "Vytváram konfiguraèný súbor: %s\n"
#define MSGTR_CopyCodecsConf "(copy/ln etc/codecs.conf (zo zdrojových kódov MPlayeru) do ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Pou¾ívam vstavané defaultne codecs.conf\n"
#define MSGTR_CantLoadFont "Nemô¾em naèíta» font: %s\n"
#define MSGTR_CantLoadSub "Nemô¾em naèíta» titulky: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: po¾adovaný prúd chýba!\n"
#define MSGTR_CantOpenDumpfile "Nejde otvori» súbor pre dump!!!\n"
#define MSGTR_CoreDumped "jadro vypísané :)\n"
#define MSGTR_FPSnotspecified "V hlavièke súboru nie je udané (alebo je zlé) FPS! Pou¾ite voµbu -fps!\n"
#define MSGTR_TryForceAudioFmtStr "Pokú¹am sa vynúti» rodinu audiokodeku %s...\n"
#define MSGTR_CantFindAudioCodec "Nemô¾em nájs» kodek pre audio formát 0x%X!\n"
#define MSGTR_RTFMCodecs "Preèítajte si DOCS/HTML/en/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Pokú¹am se vnúti» rodinu videokodeku %s...\n"
#define MSGTR_CantFindVideoCodec "Nemô¾em najs» kodek pre video formát 0x%X!\n"
#define MSGTR_CannotInitVO "FATAL: Nemô¾em inicializova» video driver!\n"
#define MSGTR_CannotInitAO "nemô¾em otvori»/inicializova» audio driver -> TICHO\n"
#define MSGTR_StartPlaying "Zaèínam prehráva»...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***********************************************************\n"\
"         ****  Na prehratie tohoto je vá¹ systém príli¹ POMALÝ!  ****\n"\
"         ***********************************************************\n"\
"!!! Mo¾né príèiny, problémy a rie¹enia:\n"\
"- Nejèastej¹ie: nesprávny/chybný _zvukový_ ovládaè.\n"\
"  - Skúste -ao sdl alebo pou¾ite OSS emuláciu ALSA.\n"\
"  - Experimentujte s rôznymi hodnotami -autosync, 30 je dobrý zaèiatok.\n"\
"- Pomalý video výstup\n"\
"  - Skúste iný -vo ovládaè (pre zoznam: -vo help) alebo skúste -framedrop!\n"\
"- Pomalý CPU\n"\
"  - Neskú¹ajte prehráva» veµké dvd/divx na pomalom cpu! Skúste lavdopts,\n"\
"    napr. -vfm ffmpeg -lavdopts lowres=1:fast:skiploopfilter=all.\n"\
"- Po¹kodený súbor\n"\
"  - Skúste rôzne kombinácie týchto volieb -nobps -ni -forceidx -mc 0.\n"\
"- Pomalé médium (NFS/SMB, DVD, VCD...)\n"\
"  - Skúste -cache 8192.\n"\
"- Pou¾ívate -cache na prehrávanie non-interleaved súboru?\n"\
"  - Skúste -nocache.\n"\
"Preèítajte si DOCS/HTML/en/video.html sú tam tipy na vyladenie/zrýchlenie.\n"\
"Ak niè z tohto nepomohlo, preèítajte si DOCS/HTML/en/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer bol zostavený BEZ podpory GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI vy¾aduje X11!\n"
#define MSGTR_Playing "Prehrávam %s\n"
#define MSGTR_NoSound "Audio: bez zvuku!!!\n"
#define MSGTR_FPSforced "FPS vnútené na hodnotu %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Skompilovné s RUNTIME CPU Detection - varovanie, nie je to optimálne! Na získanie max. výkonu, rekompilujte mplayer zo zdrojakov s --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Skompilované pre x86 CPU s roz¹íreniami:"
#define MSGTR_AvailableVideoOutputDrivers "Dostupné video výstupné ovládaèe:\n"
#define MSGTR_AvailableAudioOutputDrivers "Dostupné audio výstupné ovládaèe:\n"
#define MSGTR_AvailableAudioCodecs "Dostupné audio kodeky:\n"
#define MSGTR_AvailableVideoCodecs "Dostupné video kodeky:\n"
#define MSGTR_AvailableAudioFm "Dostupné (vkompilované) audio rodiny kodekov/ovládaèe:\n"
#define MSGTR_AvailableVideoFm "Dostupné (vkompilované) video rodiny kodekov/ovládaèe:\n"
#define MSGTR_AvailableFsType "Dostupné zmeny plnoobrazovkových módov:\n"
#define MSGTR_UsingRTCTiming "Pou¾ívam Linuxové hardvérové RTC èasovanie (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Video: nemô¾em èíta» vlastnosti\n"
#define MSGTR_NoStreamFound "Nenájdený prúd\n"
#define MSGTR_ErrorInitializingVODevice "Chyba pri otváraní/inicializácii vybraných video_out (-vo) zariadení!\n"
#define MSGTR_ForcedVideoCodec "Vnútený video kodek: %s\n"
#define MSGTR_ForcedAudioCodec "Vnútený video kodek: %s\n"
#define MSGTR_Video_NoVideo "Video: ¾iadne video!!!\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Nemô¾em inicializova» video filtre (-vf) alebo video výstup (-vo)!\n"
#define MSGTR_Paused "\n  =====  PAUZA  =====\r"
#define MSGTR_PlaylistLoadUnable "\nNemô¾em naèíta» playlist %s\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer zhavaroval na 'Illegal Instruction'.\n"\
"  Mô¾e to by» chyba v na¹om novom kóde na detekciu procesora...\n"\
"  Prosím preèítajte si DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer zhavaroval na 'Illegal Instruction'.\n"\
"  Obyèajne sa to stáva, keï ho pou¾ívate na inom procesore ako pre ktorý bol\n"\
"  skompilovaný/optimalizovaný.\n"\
"  Skontrolujte si to!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer zhavaroval nesprávnym pou¾itím CPU/FPU/RAM.\n"\
"  Prekompilujte MPlayer s --enable-debug a urobte 'gdb' backtrace a\n"\
"  disassemblujte. Pre detaily, pozrite DOCS/HTML/en/bugreports_what.html#bugreports_crash.b.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer zhavaroval. To sa nemalo sta».\n"\
"  Mô¾e to by» chyba v MPlayer kóde _alebo_ vo Va¹ích ovládaèoch _alebo_ gcc\n"\
"  verzii. Ak si myslíte, ¾e je to chyba MPlayeru, prosím preèítajte si DOCS/HTML/en/bugreports.html\n"\
"  a postupujte podµa in¹trukcii. Nemô¾eme Vám pomôc», pokiaµ neposkytnete\n"\
"  tieto informácie pri ohlasovaní mo¾nej chyby.\n"
#define MSGTR_LoadingConfig "Èítam konfiguráciu '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: pridaný súbor titulkov (%d): %s\n"
#define MSGTR_RemovedSubtitleFile "SUB: odobratý súbor titulkov (%d): %s\n"
#define MSGTR_ErrorOpeningOutputFile "Chyba pri otváraní súboru [%s] pre zápis!\n"
#define MSGTR_CommandLine "Príkazový riadok:"
#define MSGTR_RTCDeviceNotOpenable "Nepodarilo sa otvori» %s: %s (malo by to by» èitateµné pre pou¾ívateµa.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Chyba pri inicializácii Linuxových RTC v ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Skúste prida» \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" do ¹tartovacích skriptov vá¹ho systému.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Chyba pri inicializácii Linuxových RTC v ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Pou¾ívam %s èasovanie.\n"
#define MSGTR_NoIdleAndGui "Voµba -idle sa nedá pou¾i» pre GMPlayer.\n"
#define MSGTR_MenuInitialized "Menu inicializované: %s\n"
#define MSGTR_MenuInitFailed "Zlyhala inicializácia menu.\n"
#define MSGTR_Getch2InitializedTwice "VAROVANIE: getch2_init je volaná dvakrát!\n"
#define MSGTR_DumpstreamFdUnavailable "Nemô¾em ulo¾i» (dump) tento prúd - nie je dostupný ¾iaden deskriptor súboru.\n"
#define MSGTR_FallingBackOnPlaylist "Ustupujem od pokusu o spracovanie playlistu %s...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Nemô¾em otvori» video filter libmenu s koreòovým menu %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Chyba pri predinicializácii re»azca audio filtrov!\n"
#define MSGTR_LinuxRTCReadError "Chyba pri èítaní z Linuxových RTC: %s\n"
#define MSGTR_SoftsleepUnderflow "Pozor! Podteèenie softsleep!\n"
#define MSGTR_DvdnavNullEvent "DVDNAV udalos» NULL?!\n"
#define MSGTR_DvdnavHighlightEventBroken "DVDNAV udalos»: Vadné zvýraznenie udalostí\n"
#define MSGTR_DvdnavEvent "DVDNAV udalos»: %s\n"
#define MSGTR_DvdnavHighlightHide "DVDNAV udalos»: skry» zvýraznenie\n"
#define MSGTR_DvdnavStillFrame "######################################## DVDNAV udalos»: Stojací snímok: %d sec(s)\n"
#define MSGTR_DvdnavNavStop "DVDNAV udalos»: Nav Stop\n"
#define MSGTR_DvdnavNavNOP "DVDNAV udalos»: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "DVDNAV udalos»: Zmena Nav SPU prúdu: fyz: %d/%d/%d logicky: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "DVDNAV udalos»: Zmena Nav SPU prúdu: fyz: %d logicky: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "DVDNAV udalos»: Zmena Nav Audio prúdu: fyz: %d logicky: %d\n"
#define MSGTR_DvdnavNavVTSChange "DVDNAV udalos»: Zmena Nav VTS\n"
#define MSGTR_DvdnavNavCellChange "DVDNAV udalos»: Zmena Nav bunky \n"
#define MSGTR_DvdnavNavSpuClutChange "DVDNAV udalos»: Zmena Nav SPU CLUT\n"
#define MSGTR_DvdnavNavSeekDone "DVDNAV udalos»: Prevíjanie Nav dokonèené\n"
#define MSGTR_MenuCall "Volanie menu\n"

#define MSGTR_EdlOutOfMem "Nedá sa alokova» dostatok pamäte pre EDL dáta.\n"
#define MSGTR_EdlRecordsNo "Èítam %d EDL akcie.\n"
#define MSGTR_EdlQueueEmpty "V¹etky EDL akcie boly u¾ vykonané.\n"
#define MSGTR_EdlCantOpenForWrite "Nedá sa otvori» EDL súbor [%s] pre zápis.\n"
#define MSGTR_EdlCantOpenForRead "Nedá sa otvori» EDL súbor [%s] na èítanie.\n"
#define MSGTR_EdlNOsh_video "EDL sa nedá pou¾i» bez videa, vypínam.\n"
#define MSGTR_EdlNOValidLine "Chyba EDL na riadku: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Zle formátovaný EDL riadok [%d] Zahadzujem.\n"
#define MSGTR_EdlBadLineOverlap "Posledná stop znaèka bola [%f]; ïal¹í ¹tart je "\
"[%f]. Záznamy musia by» chronologicky, a nesmú sa prekrýva». Zahadzujem.\n"
#define MSGTR_EdlBadLineBadStop "Èasová znaèka stop má by» za znaèkou start.\n"

// mplayer.c OSD

#define MSGTR_OSDenabled "zapnuté"
#define MSGTR_OSDdisabled "vypnuté"
#define MSGTR_OSDChannel "Kanál: %s"
#define MSGTR_OSDSubDelay "Zpozdenie tit: %d ms"
#define MSGTR_OSDSpeed "Rýchlos»: x %6.2f"
#define MSGTR_OSDosd "OSD: %s"

// property values
#define MSGTR_Enabled "zapnuté"
#define MSGTR_EnabledEdl "zapnuté (edl)"
#define MSGTR_Disabled "vypnuté"
#define MSGTR_HardFrameDrop "hard"
#define MSGTR_Unknown "neznáme"
#define MSGTR_Bottom "dole"
#define MSGTR_Center "stred"
#define MSGTR_Top "hore"

// osd bar names
#define MSGTR_Volume "Hlasitos»"
#define MSGTR_Panscan "Panscan"
#define MSGTR_Gamma "Gama"
#define MSGTR_Brightness "Jas"
#define MSGTR_Contrast "Kontrast"
#define MSGTR_Saturation "Sýtos»"
#define MSGTR_Hue "Tón"

// property state
#define MSGTR_MuteStatus "Utlmenie zvuku: %s"
#define MSGTR_AVDelayStatus "A-V odchylka: %s"
#define MSGTR_OnTopStatus "V¾dy navrchu: %s"
#define MSGTR_RootwinStatus "Hlavné okno: %s"
#define MSGTR_BorderStatus "Ohranièenie: %s"
#define MSGTR_FramedroppingStatus "Zahadzovanie snímkov: %s"
#define MSGTR_VSyncStatus "VSync: %s"
#define MSGTR_SubSelectStatus "Titulky: %s"
#define MSGTR_SubPosStatus "Pozícia tit.: %s/100"
#define MSGTR_SubAlignStatus "Zarovnanie tit.: %s"
#define MSGTR_SubDelayStatus "Spozdenie tit.: %s"
#define MSGTR_SubVisibleStatus "Zobr. titulkov: %s"
#define MSGTR_SubForcedOnlyStatus "Iba vynútené tit.: %s"

// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Pou¾ívam pass3 ovládací súbor: %s\n"
#define MSGTR_MissingFilename "\nChýbajúce meno súboru.\n\n"
#define MSGTR_CannotOpenFile_Device "Nemô¾em otvori» súbor/zariadenie\n"
#define MSGTR_CannotOpenDemuxer "Nemô¾em otvori» demuxer\n"
#define MSGTR_NoAudioEncoderSelected "\nNevybraný encoder (-oac)! Vyberte jeden alebo -nosound. Pou¾itie -oac help!\n"
#define MSGTR_NoVideoEncoderSelected "\nNevybraný encoder (-ovc)! Vyberte jeden, pou¾itie -ovc help!\n"
#define MSGTR_CannotOpenOutputFile "Nemô¾em otvori» súbor '%s'\n"
#define MSGTR_EncoderOpenFailed "Zlyhalo spustenie enkóderu\n"
#define MSGTR_MencoderWrongFormatAVI "\nVAROVANIE: FORMÁT VÝSTUPNÉHO SÚBORU JE _AVI_. viz -of help.\n"
#define MSGTR_MencoderWrongFormatMPG "\nVAROVANIE: FORMÁT VÝSTUPNÉHO SÚBORU JE _MPEG_. viz -of help.\n"
#define MSGTR_MissingOutputFilename "Nebol nastavený výstupný súbor, pre¹tudujte si volbu -o"
#define MSGTR_ForcingOutputFourcc "Vnucujem výstupný formát (fourcc) na %x [%.4s]\n"
#define MSGTR_ForcingOutputAudiofmtTag "Vynucujem znaèku výstupného zvukového formátu 0x%x\n"
#define MSGTR_DuplicateFrames "\nduplikujem %d snímkov!!!    \n"
#define MSGTR_SkipFrame "\npreskoèi» snímok!!!    \n"
#define MSGTR_ResolutionDoesntMatch "\nNový video súbor má iné rozli¹ení alebo farebný priestor ako jeho predchodca.\n"
#define MSGTR_FrameCopyFileMismatch "\nV¹etky video soubory musí mít shodné fps, rozli¹ení a kodek pro -ovc copy.\n"
#define MSGTR_AudioCopyFileMismatch "\nV¹etky súbory musí pou¾íva» identický audio kódek a formát pro -oac copy.\n"
#define MSGTR_NoAudioFileMismatch "\nNemô¾ete mixova» iba video s audio a video súbormi. Skúste -nosound.\n"
#define MSGTR_NoSpeedWithFrameCopy "VAROVANIE: -speed nemá zaruèenú funkènos» s -oac copy!\n"\
"Výsledny súbor mô¾e by» vadný!\n"
#define MSGTR_ErrorWritingFile "%s: chyba pri zápise súboru.\n"
#define MSGTR_RecommendedVideoBitrate "Odporúèaný dátový tok videa pre CD %s: %d\n"
#define MSGTR_VideoStreamResult "\nVideo prúd: %8.3f kbit/s  (%d B/s)  velkos»: %"PRIu64" bytov  %5.3f sekund  %d snímkov\n"
#define MSGTR_AudioStreamResult "\nAudio prúd: %8.3f kbit/s  (%d B/s)  velkos»: %"PRIu64" bytov  %5.3f sekund\n"
#define MSGTR_OpenedStream "úspech: formát: %d  dáta: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videokódek: framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "audiokódek: framecopy (formát=%x kanálov=%d frekvencia=%d bitov=%d B/s=%d vzorka-%d)\n"
#define MSGTR_CBRPCMAudioSelected "zvolený CBR PCM zvuk\n"
#define MSGTR_MP3AudioSelected "zvolený MP3 zvuk\n"
#define MSGTR_CannotAllocateBytes "Nedá sa alokova» %d bajtov\n"
#define MSGTR_SettingAudioDelay "Nastavujem spozdenie zvuku na %5.3f\n"
#define MSGTR_SettingVideoDelay "Nastavujem spozdìnie videa na %5.3fs\n"
#define MSGTR_SettingAudioInputGain "Nastavujem predzosilnenie zvukového vstupu na %f\n"
#define MSGTR_LamePresetEquals "\npreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Obmedzujem prednaèítanie zvuku na 0.4s\n"
#define MSGTR_IncreasingAudioDensity "Zvy¹ujem hustotu audia na 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Vnucujem prednaèítanie zvuku na 0, max korekciu pts na 0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR zvuk: %d bajtov/s, %d bajtov/blok\n"
#define MSGTR_LameVersion "LAME verzia %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Chyba: ©pecifikovaný dátový tok je mimo rozsah pre tento preset.\n"\
"\n"\
"Pokiaµ pou¾ívate tento re¾im, musíte zadat hodnotu od \"8\" do \"320\".\n"\
"\n"\
"Dal¹ie informácie viz: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Chyba: Nezadali ste platný profil a/alebo voµby s presetom.\n"\
"\n"\
"Dostupné profily sú:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - Implikuje re¾im ABR. Pre jeho pou¾itie,\n"\
"                      jednoduche zadejte dátový tok. Napríklad:\n"\
"                      \"preset=185\" aktivuje tento preset\n"\
"                      a pou¾ije priemerný dátový tok 185 kbps.\n"\
"\n"\
"    Niekolko príkladov:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Dal¹ie informácie viz: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Preset prepínaèe sú navrhnuté tak, aby poskytovaly èo najvy¹¹iu mo¾nú kvalitu.\n"\
"\n"\
"Väè¹ina z nich bola testovaná a vyladená pomocou dôkladných zdvojených slepých\n"\
"posluchových testov, za úèelom dosiahnutia a overenia tohto ciela.\n"\
"\n"\
"Nastavenia sú neustále aktualizované v súlade s najnov¹ím vývojom\n"\
"a mali by poskytova» prakticky najvy¹¹iu mo¾nú kvalitu, aká je v súèasnosti \n"\
"s kódekom LAME dosa¾iteµná.\n"\
"\n"\
"Aktivácia presetov:\n"\
"\n"\
"   Pre re¾imy VBR (vo v¹eobecnosti najvy¹¹ia kvalita):\n"\
"\n"\
"     \"preset=standard\" Tento preset by mal bý» jasnou voµbou\n"\
"                             pre väè¹inu ludí a hudobných ¾ánrov a má\n"\
"                             u¾ vysokú kvalitu.\n"\
"\n"\
"     \"preset=extreme\" Pokiaµ máte výnimoène dobrý sluch a zodpovedajúce\n"\
"                             vybavenie, tento preset vo v¹eob. poskytuje\n"\
"                             miernì vy¹¹í kvalitu ako re¾im \"standard\".\n"\
"\n"\
"   Pre CBR 320kbps (najvy¹¹ia mo¾ná kvalita ze v¹etkých presetov):\n"\
"\n"\
"     \"preset=insane\"  Tento preset je pre väè¹inu ludí a situácii\n"\
"                             predimenzovaný, ale pokiaµ vy¾adujete\n"\
"                             absolutne najvy¹¹iu kvalitu bez ohµadu na\n"\
"                             velkos» súboru, je toto va¹a voµba.\n"\
"\n"\
"   Pre re¾imy ABR (vysoká kvalita pri danom dátovém toku, ale nie tak ako VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Pou¾itím tohoto presetu obvykle dosiahnete dobrú\n"\
"                             kvalitu pri danom dátovém toku. V závislosti\n"\
"                             na zadanom toku tento preset odvodí optimálne\n"\
"                             nastavenie pre danú situáciu.\n"\
"                             Hoci tento prístup funguje, nie je ani zïaleka\n"\
"                             tak flexibilný ako VBR, a obvykle nedosahuje\n"\
"                             úrovne kvality ako VBR na vy¹¹ích dátových tokoch.\n"\
"\n"\
"Pre zodpovedajúce profily sú k dispozícii tie¾ nasledujúce voµby:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR re¾im) - Implikuje re¾im ABR. Pre jeho pou¾itie\n"\
"                      jednoducho zadajte dátový tok. Napríklad:\n"\
"                      \"preset=185\" aktivuje tento preset\n"\
"                      a pou¾ije priemerný dátový tok 185 kbps.\n"\
"\n"\
"   \"fast\" - V danom profile aktivuje novú rýchlu VBR kompresiu.\n"\
"            Nevýhodou je obvykle mierne vy¹¹í dátový tok ako v normálnom\n"\
"            re¾ime a tie¾ mô¾e dôjs» k miernemu poklesu kvality.\n"\
"   Varovanie:v aktuálnej verzi mô¾e nastavenie \"fast\" vies» k príli¹\n"\
"            vysokému dátovému toku v porovnaní s normálnym nastavením.\n"\
"\n"\
"   \"cbr\"  - Pokiaµ pou¾ijete re¾im ABR (viz vy¹¹ie) s významným\n"\
"            dátovým tokom, napr. 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            mô¾ete pou¾í» voµbu \"cbr\" k vnúteniu kódovánia v re¾ime CBR\n"\
"            (kon¹tantný tok) namiesto ¹tandardního ABR re¾imu. ABR poskytuje\n"\
"            lep¹iu kvalitu, ale CBR mô¾e by» u¾itoèný v situáciach ako je\n"\
"            vysielanie mp3 prúdu po internete.\n"\
"\n"\
"    Napríklad:\n"\
"\n"\
"      \"-lameopts fast:preset=standard  \"\n"\
" alebo \"-lameopts  cbr:preset=192       \"\n"\
" alebo \"-lameopts      preset=172       \"\n"\
" alebo \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Pre ABR re¾im je k dispozícii niekolko skratiek:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit "Nedá sa nastavi» voµba pre LAME, overte dátový_tok/vzorkovaciu_frekv.,"\
"niektoré veµmi nízke dátové toky (<32) vy¾adujú ni¾¹iu vzorkovaciu frekv. (napr. -srate 8000)."\
"Pokud v¹etko ostané zlyhá, zkúste prednastavenia (presets)."
#define MSGTR_ConfigFileError "chyba konfiguraèného súboru"
#define MSGTR_ErrorParsingCommandLine "chyba pri spracovávaní príkazového riadku"
#define MSGTR_VideoStreamRequired "Video prúd je povinný!\n"
#define MSGTR_ForcingInputFPS "vstupné fps bude interpretované ako %5.2f\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Výstupný formát súboru RAWVIDEO nepodporuje zvuk - vypínam ho\n"
#define MSGTR_DemuxerDoesntSupportNosound "Tento demuxer zatiaµ nepodporuje -nosound.\n"
#define MSGTR_MemAllocFailed "Alokácia pamäte zlyhala\n"
#define MSGTR_NoMatchingFilter "Nemo¾em nájs» zodpovedajúci filter/ao formát!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, mo¾no je vadný prekladaè C?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, chýba meno kódeku!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, nemô¾em nájs» enkóder pre kódek %s\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, nemô¾em alokova» kontext!\n"
#define MSGTR_CouldntOpenCodec "Nedá sa otvori» kódek %s, br=%d\n"
#define MSGTR_CantCopyAudioFormat "Audio formát 0x%x je nekompatibilný s '-oac copy', skúste prosím '-oac pcm',\n alebo pou¾ite '-fafmttag' pre jeho prepísanie.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metóda variabilnej bit. rýchlosti \n"\
"                0: cbr (kon¹tantná bit.rýchlos»)\n"\
"                1: mt (Mark Taylor VBR alg.)\n"\
"                2: rh(Robert Hegemann VBR alg. - default)\n"\
"                3: abr (priemerná bit.rýchlos»)\n"\
"                4: mtrh (Mark Taylor Robert Hegemann VBR alg.)\n"\
"\n"\
" abr           priemerná bit. rýchlos»\n"\
"\n"\
" cbr           kon¹tantná bit. rýchlos»\n"\
"               Vnúti tie¾ CBR mód na podsekvenciách ABR módov\n"\
"\n"\
" br=<0-1024>   ¹pecifikova» bit. rýchlos» v kBit (platí iba pre CBR a ABR)\n"\
"\n"\
" q=<0-9>       kvalita (0-najvy¹¹ia, 9-najni¾¹ia) (iba pre VBR)\n"\
"\n"\
" aq=<0-9>      algoritmická kvalita (0-najlep./najpomal¹ia, 9-najhor¹ia/najrýchl.)\n"\
"\n"\
" ratio=<1-100> kompresný pomer\n"\
"\n"\
" vol=<0-10>    nastavenie audio zosilnenia\n"\
"\n"\
" mode=<0-3>    (default: auto)\n"\
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
" fast          prepnú» na rýchlej¹ie kódovanie na podsekvenciách VBR módov,\n"\
"               mierne ni¾¹ia kvalita and vy¹¹ia bit. rýchlos».\n"\
"\n"\
" preset=<value> umo¾òuje najvy¹¹ie mo¾né nastavenie kvality.\n"\
"                 medium: VBR  kódovanie,  dobrá kvalita\n"\
"                 (150-180 kbps rozpätie bit. rýchlosti)\n"\
"                 standard:  VBR kódovanie, vysoká kvalita\n"\
"                 (170-210 kbps rozpätie bit. rýchlosti)\n"\
"                 extreme: VBR kódovanie, velmi vysoká kvalita\n"\
"                 (200-240 kbps rozpätie bit. rýchlosti)\n"\
"                 insane:  CBR  kódovanie, najvy¹¹ie nastavenie kvality\n"\
"                 (320 kbps bit. rýchlos»)\n"\
"                 <8-320>: ABR kódovanie na zadanej kbps bit. rýchlosti.\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "zdvojené FourCC"
#define MSGTR_TooManyFourccs "príli¹ vela FourCCs/formátov..."
#define MSGTR_ParseError "chyba spracovania (parse)"
#define MSGTR_ParseErrorFIDNotNumber "chyba spracovania (parse) (ID formátu nie je èíslo?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "chyba spracovania (parse) (alias ID formátu nie je èíslo?)"
#define MSGTR_DuplicateFID "duplikátne format ID"
#define MSGTR_TooManyOut "príli¹ mnoho výstupu..."
#define MSGTR_InvalidCodecName "\nmeno kódeku(%s) nie je platné!\n"
#define MSGTR_CodecLacksFourcc "\nmeno kódeku(%s) nemá FourCC/formát!\n"
#define MSGTR_CodecLacksDriver "\nmeno kódeku(%s) nemá ovládaè!\n"
#define MSGTR_CodecNeedsDLL "\nkódek(%s) vy¾aduje 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\nkódek(%s) vy¾aduje 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Nedá sa alokova» pamä» pre poznámku. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "Èítam %s: "
#define MSGTR_CantOpenFileError "Nedá sa otvori» '%s': %s\n"
#define MSGTR_CantGetMemoryForLine "Nejde získa» pamä» pre 'line': %s\n"
#define MSGTR_CantReallocCodecsp "Nedá sa realokova» '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique " Meno kódeku '%s' nie je jedineèné."
#define MSGTR_CantStrdupName "Nedá sa spravi» strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Nedá sa spravi» strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Nedá sa spravi» strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Nedá sa spravi» strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d audio & %d video codecs\n"
#define MSGTR_CodecDefinitionIncorrect "Kódek nie je definovaný korektne."
#define MSGTR_OutdatedCodecsConf "Súbor codecs.conf je príli¹ starý a nekompatibilný s touto verziou MPlayer-u!"

// divx4_vbr.c:
#define MSGTR_OutOfMemory "nedostatok pamäte"
#define MSGTR_OverridingTooLowBitrate "Zadaný dátový tok je príli¹ nízky pre tento klip.\n"\
"Minimálny mo¾ný dátový tok pre tento klip je %.0f kbps. Prepisujem\n"\
"pou¾ívateµom nastavenú hodnotu.\n"

// fifo.c
#define MSGTR_CannotMakePipe "Nedá sa vytvori» PIPE!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Príli¹ starý save slot nájdený z lvl %d: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "Voµba %s sa nedá pou¾i» v konfiguraènom súbore.\n"
#define MSGTR_InvalidCmdlineOption "Voµba %s sa nedá pou¾i» z príkazového riadku.\n"
#define MSGTR_InvalidSuboption "Chyba: voµba '%s' nemá ¾iadnu podvoµbu '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Chyba: podvoµba '%s' voµby '%s' musí ma» parameter!\n"
#define MSGTR_MissingOptionParameter "Chyba: voµba '%s' musí ma» parameter!\n"
#define MSGTR_OptionListHeader "\n Názov                Typ             Min        Max      Globál  CL    Konfig\n\n"
#define MSGTR_TotalOptions "\nCelkovo: %d volieb\n"
#define MSGTR_ProfileInclusionTooDeep "VAROVANIE: Príli¹ hlboké vnorovanie profilov.\n"
#define MSGTR_NoProfileDefined "®iadny profil nebol definovaný.\n"
#define MSGTR_AvailableProfiles "Dostupné profily:\n"
#define MSGTR_UnknownProfile "Neznámy profil '%s'.\n"
#define MSGTR_Profile "Profil %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n Meno                 Typ             Min        Max\n\n"
#define MSGTR_TotalProperties "\nCelkovo: %d vlastností\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM zariadenie '%s' nenájdené!\n"
#define MSGTR_ErrTrackSelect "Chyba pri výbere VCD stopy!"
#define MSGTR_ReadSTDIN "Èítam z stdin...\n"
#define MSGTR_UnableOpenURL "Nejde otvori» URL: %s\n"
#define MSGTR_ConnToServer "Pripojený k servru: %s\n"
#define MSGTR_FileNotFound "Súbor nenájdený: '%s'\n"

#define MSGTR_SMBInitError "Nemô¾em inicializova» kni¾nicu libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Nemô¾em otvori» z LAN: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer mebol skompilovaný s podporou èítania z SMB\n"

#define MSGTR_CantOpenDVD "Nejde otvori» DVD zariadenie: %s\n"
#define MSGTR_NoDVDSupport "MPlayer bol skompilovaný bez podpory DVD, koniec\n"
#define MSGTR_DVDnumTitles "Na tomto DVD je %d titulov.\n"
#define MSGTR_DVDinvalidTitle "Neplatné èíslo DVD titulu: %d\n"
#define MSGTR_DVDnumChapters "Na tomto DVD je %d kapitol.\n"
#define MSGTR_DVDinvalidChapter "Neplatné èíslo kapitoly DVD: %d\n"
#define MSGTR_DVDinvalidChapterRange "Nesprávnì nastavený rozsah kapitol %s\n"
#define MSGTR_DVDinvalidLastChapter "Neplatné èíslo poslednej DVD kapitoly: %d\n"
#define MSGTR_DVDnumAngles "Na tomto DVD je %d uhlov pohµadov.\n"
#define MSGTR_DVDinvalidAngle "Neplatné èíslo uhlu pohµadu DVD: %d\n"
#define MSGTR_DVDnoIFO "Nemô¾em otvori» súbor IFO pre DVD titul %d.\n"
#define MSGTR_DVDnoVMG "Nedá sa otvori» VMG info!\n"
#define MSGTR_DVDnoVOBs "Nemô¾em otvori» VOB súbor (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDnoMatchingAudio "DVD zvuk v po¾adovanom jazyku nebyl nájdený!\n"
#define MSGTR_DVDaudioChannel "Zvolený DVD zvukový kanál: %d jazyk: %c%c\n"
#define MSGTR_DVDnoMatchingSubtitle "DVD titulky v po¾adovanom jazyku neboli nájdené!\n"
#define MSGTR_DVDsubtitleChannel "Zvolený DVD titulkový kanál: %d jazyk: %c%c\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "Príli¹ veµa prúdov!"
#define MSGTR_RawMuxerOnlyOneStream "Rawaudio muxer podporuje iba jeden audio prúd!\n"
#define MSGTR_IgnoringVideoStream "Ignorujem video prúd!\n"
#define MSGTR_UnknownStreamType "Varovanie! neznámy typ prúdu: %d\n"
#define MSGTR_WarningLenIsntDivisible "Varovanie! då¾ka nie je deliteµná velkos»ou vzorky!\n"
#define MSGTR_MuxbufMallocErr "Nedá sa alokova» pamä» pre frame buffer muxeru!\n"
#define MSGTR_MuxbufReallocErr "Nedá sa realokova» pamä» pre frame buffer muxeru!\n"
#define MSGTR_MuxbufSending "Frame buffer muxeru posiela %d snímkov do muxeru.\n"
#define MSGTR_WritingHeader "Zapisujem header...\n"
#define MSGTR_WritingTrailer "Zapisujem index...\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Upozornenie! Hlavièka audio prúdu %d predefinovaná!\n"
#define MSGTR_VideoStreamRedefined "Upozornenie! Hlavièka video prúdu %d predefinovaná!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Príli¹ mnoho (%d v %d bajtoch) audio paketov v bufferi!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Príli¹ mnoho (%d v %d bajtoch) video paketov v bufferi!\n"
#define MSGTR_MaybeNI "(mo¾no prehrávate neprekladaný prúd/súbor alebo kodek zlyhal)\n" \
		      "Pre .AVI súbory skúste vynúti» neprekladaný mód voµbou -ni\n"
#define MSGTR_SwitchToNi "\nDetekovaný zle prekladaný .AVI - prepnite -ni mód!\n"
#define MSGTR_Detected_XXX_FileFormat "Detekovaný %s formát súboru!\n"
#define MSGTR_DetectedAudiofile "Detekovaný audio súbor!\n"
#define MSGTR_NotSystemStream "Nie je to MPEG System Stream formát... (mo¾no Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Neplatný MPEG-ES prúd??? kontaktujte autora, mo¾no je to chyba (bug) :(\n"
#define MSGTR_FormatNotRecognized "========== ®iaµ, tento formát súboru nie je rozpoznaný/podporovaný =======\n"\
				  "==== Pokiaµ je tento súbor AVI, ASF alebo MPEG prúd, kontaktujte autora! ====\n"
#define MSGTR_MissingVideoStream "®iadny video prúd nenájdený!\n"
#define MSGTR_MissingAudioStream "®iadny audio prúd nenájdený...  -> bez zvuku\n"
#define MSGTR_MissingVideoStreamBug "Chýbajúci video prúd!? Kontaktujte autora, mo¾no to je chyba (bug) :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: súbor neobsahuje vybraný audio alebo video prúd\n"

#define MSGTR_NI_Forced "Vnútený"
#define MSGTR_NI_Detected "Detekovaný"
#define MSGTR_NI_Message "%s NEPREKLADANÝ formát súboru AVI!\n"

#define MSGTR_UsingNINI "Pou¾ívam NEPREKLADANÝ po¹kodený formát súboru AVI!\n" 
#define MSGTR_CouldntDetFNo "Nemô¾em urèi» poèet snímkov (pre absolútny posun)  \n"
#define MSGTR_CantSeekRawAVI "Nemô¾em sa posúva» v surových (raw) .AVI prúdoch! (Potrebujem index, zkuste pou¾í» voµbu -idx!)  \n"
#define MSGTR_CantSeekFile "Nemô¾em sa posúva» v tomto súbore!  \n"

#define MSGTR_EncryptedVOB "©ifrovaný súbor VOB! Preèítajte si DOCS/HTML/en/cd-dvd.html.\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimované hlavièky nie sú (e¹te) podporované!\n"
#define MSGTR_MOVvariableFourCC "MOV: Upozornenie! premenná FOURCC detekovaná!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Upozornenie! Príli¹ veµa stôp!"
#define MSGTR_FoundAudioStream "==> Nájdený audio prúd: %d\n"
#define MSGTR_FoundVideoStream "==> Nájdený video prúd: %d\n"
#define MSGTR_DetectedTV "TV detekovaný! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Nemô¾em otvori» ogg demuxer\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Hµadám audio prúd (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Nemô¾em otvori» audio prúd: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nemô¾em otvori» prúd titulkov: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Nemô¾em otvori» audio demuxer: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Nemô¾em otvori» demuxer titulkov: %s\n"
#define MSGTR_TVInputNotSeekable "v TV vstupe nie je mo¾né sa pohybova»! (mo¾no posun bude na zmenu kanálov ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Demuxer info %s u¾ prítomné!\n"
#define MSGTR_ClipInfo "Informácie o klipe: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: detekovaný 30000/1001 fps NTSC, prepínam frekvenciu snímkov.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: detekovaný 24000/1001 fps progresívny NTSC, prepínam frekvenciu snímkov.\n"

#define MSGTR_CacheFill "\rNaplnenie cache: %5.2f%% (%"PRId64" bajtov)   "
#define MSGTR_NoBindFound "Tlaèidlo '%s' nemá priradenú ¾iadnu funkciu."
#define MSGTR_FailedToOpen "Zlyhalo otvorenie %s\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nemô¾em otvori» kodek\n"
#define MSGTR_CantCloseCodec "nemô¾em uzavie» kodek\n"

#define MSGTR_MissingDLLcodec "CHYBA: Nemô¾em otvori» potrebný DirectShow kodek: %s\n"
#define MSGTR_ACMiniterror "Nemô¾em naèíta»/inicializova» Win32/ACM AUDIO kodek (chýbajúci súbor DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nemô¾em najs» kodek '%s' v libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF - koniec súboru v priebehu vyhµadávania hlavièky sekvencie\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Nemô¾em preèíta» hlavièku sekvencie!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Nemô¾em preèíta» roz¹írenie hlavièky sekvencie!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Zlá hlavièka sekvencie!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Zlé roz¹írenie hlavièky sekvencie!\n"

#define MSGTR_ShMemAllocFail "Nemô¾em alokova» zdieµanú pamä»\n"
#define MSGTR_CantAllocAudioBuf "Nemô¾em alokova» pamä» pre výstupný audio buffer\n"

#define MSGTR_UnknownAudio "Neznámy/chýbajúci audio formát -> bez zvuku\n"

#define MSGTR_UsingExternalPP "[PP] Pou¾ívam externý postprocessing filter, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Po¾ívam postprocessing z kodeku, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Video atribút '%s' nie je podporovaný výberom vo & vd! \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Po¾adovaná rodina video kodekov [%s] (vfm=%s) nie je dostupná (zapnite ju pri kompilácii!)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Po¾adovaná rodina audio kodekov [%s] (afm=%s) nie je dostupná (zapnite ju pri kompilácii!)\n"
#define MSGTR_OpeningVideoDecoder "Otváram video dekóder: [%s] %s\n"
#define MSGTR_SelectedVideoCodec "Zvolený video kódek: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "Otváram audio dekóder: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "Zvolený audio kódek: [%s] afm: %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "Vytváram re»azec audio filterov pre %dHz/%dch/%s -> %dHz/%dch/%s...\n"
#define MSGTR_UninitVideoStr "odinicializova» video: %s  \n"
#define MSGTR_UninitAudioStr "odinicializova» audio: %s  \n"
#define MSGTR_VDecoderInitFailed "VDecoder init zlyhal :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder init zlyhal :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder preinit zlyhal :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alokujem %d bytov pre vstupný buffer\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alokujem %d + %d = %d bytov pre výstupný buffer\n"
			 
// LIRC:
#define MSGTR_SettingUpLIRC "Zapínam podporu LIRC...\n"
#define MSGTR_LIRCdisabled "Nebudete môc» pou¾íva» diaµkový ovládaè.\n"
#define MSGTR_LIRCopenfailed "Zlyhal pokus o otvorenie podpory LIRC!\n"
#define MSGTR_LIRCcfgerr "Zlyhalo èítanie konfiguraèného súboru LIRC %s!\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nemô¾em nájs» video filter '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Nemô¾em otvori» video filter '%s'\n"
#define MSGTR_OpeningVideoFilter "Otváram video filter: "
#define MSGTR_CannotFindColorspace "Nemô¾em nájs» be¾ný priestor farieb, ani vlo¾ením 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: kodek nenastavil sh->disp_w a sh->disp_h, skú¹am to obís»!\n"
#define MSGTR_VoConfigRequest "VDec: vo konfiguraèná po¾iadavka - %d x %d (preferovaný csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Nemô¾em nájs» zhodný priestor farieb - skú¹am znova s -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Movie-Aspect je %.2f:1 - mením rozmery na správne.\n"
#define MSGTR_MovieAspectUndefined "Movie-Aspect je nedefinovný - nemenia sa rozmery.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Potrebujete aktualizova» alebo nain¹talova» binárne kódeky.\nChodte na http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Inicializácia Win32/DShow videokódeku OK.\n"
#define MSGTR_DMOInitOK "INFO: Inicializácia Win32/DMO videokódeku OK.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Nemô¾em posla» udalos» EWMH fullscreen!\n"
#define MSGTR_CouldNotFindXScreenSaver "xscreensaver_disable: Nedá sa nájs» XScreenSaveru.\n"
#define MSGTR_SelectedVideoMode "XF86VM: Zvolený videore¾im %dx%d pre obraz velkosti %dx%d.\n"

#define MSGTR_InsertingAfVolume "[Mixer] Hardvérový mixér nie je k dispozicí, vkladám filter pre hlasitos».\n"
#define MSGTR_NoVolume "[Mixer] Ovládanie hlasitosti nie je dostupné.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O aplikácii"
#define MSGTR_FileSelect "Vybra» súbor..."
#define MSGTR_SubtitleSelect "Vybra» titulky..."
#define MSGTR_OtherSelect "Vybra»..."
#define MSGTR_AudioFileSelect "Vybra» externý audio kanál..."
#define MSGTR_FontSelect "Vybra» font..."
// Note: If you change MSGTR_PlayList please see if it still fits MSGTR_MENU_PlayList
#define MSGTR_PlayList "PlayList"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_ConfigureEqualizer "Konfigurova» Equalizer"
#define MSGTR_SkinBrowser "Prehliadaè tém"
#define MSGTR_Network "Sie»ové prehrávanie (streaming)..."
// Note: If you change MSGTR_Preferences please see if it still fits MSGTR_MENU_Preferences
#define MSGTR_Preferences "Preferencie"
#define MSGTR_AudioPreferences "Konfiguracia ovladaèa zvuku"
#define MSGTR_NoMediaOpened "Niè nie je otvorené"
#define MSGTR_VCDTrack "VCD stopa %d"
#define MSGTR_NoChapter "®iadna kapitola"
#define MSGTR_Chapter "Kapitola %d"
#define MSGTR_NoFileLoaded "Nenahraný ¾iaden súbor"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Zru¹i»"
#define MSGTR_Add "Prida»"
#define MSGTR_Remove "Odobra»"
#define MSGTR_Clear "Vyèisti»"
#define MSGTR_Config "Konfigurácia"
#define MSGTR_ConfigDriver "Konfigurova» ovládaè"
#define MSGTR_Browse "Prehliada»"

// --- error messages ---
#define MSGTR_NEMDB "®iaµ, nedostatok pamäte pre buffer na kreslenie."
#define MSGTR_NEMFMR "®iaµ, nedostatok pamäte pre vytváranie menu."
#define MSGTR_IDFGCVD "®iaµ, nemô¾em nájs» gui kompatibilný ovládaè video výstupu."
#define MSGTR_NEEDLAVCFAME "®iaµ, nemô¾ete prehráva» nie mpeg súbory s DXR3/H+ zariadením bez prekódovania.\nProsím zapnite lavc alebo fame v DXR3/H+ konfig. okne."
#define MSGTR_UNKNOWNWINDOWTYPE "Neznámy typ okna nájdený ..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[témy] chyba v konfig. súbore tém %d: %s"
#define MSGTR_SKIN_WARNING1 "[témy] varovanie v konfig. súbore tém na riadku %d: widget najdený ale pred  \"section\" nenájdený (%s)"
#define MSGTR_SKIN_WARNING2 "[témy] varovanie v konfig. súbore tém na riadku %d: widget najdený ale pred \"subsection\" nenájdený (%s)"
#define MSGTR_SKIN_WARNING3 "[témy] varovanie v konfig. súbore tém na riadku %d: táto subsekcia nie je podporovaná týmto widget (%s)"
#define MSGTR_SKIN_SkinFileNotFound "[skin] súbor ( %s ) nenájdený.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skin] súbor ( %s ) sa nedá preèíta».\n"
#define MSGTR_SKIN_BITMAP_16bit  "bitmapa s håbkou 16 bit a menej je nepodporovaná (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "súbor nenájdený (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "chyba èítania BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "chyba èítania TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "chyba èítania PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "formát RLE packed TGA nepodporovaný (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "neznámy typ súboru (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "chyba konverzie z 24 bit do 32 bit (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "neznáma správa: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nedostatok pamäte\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "príli¹ mnoho fontov deklarovaných\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "súbor fontov nenájdený\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "súbor obrazov fontu nenájdený\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "neexistujúci identifikátor fontu (%s)\n"
#define MSGTR_SKIN_UnknownParameter "neznámy parameter (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Téma nenájdená (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "Vybraná téma ( %s ) nenájdená, skú¹am 'prednastavenú'...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Chyba pri èítaní konfiguraèného súboru tém (%s).\n"
#define MSGTR_SKIN_LABEL "Témy:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O aplikácii MPlayer"
#define MSGTR_MENU_Open "Otvori»..."
#define MSGTR_MENU_PlayFile "Prehra» súbor..."
#define MSGTR_MENU_PlayVCD "Prehra» VCD..."
#define MSGTR_MENU_PlayDVD "Prehra» DVD..."
#define MSGTR_MENU_PlayURL "Prehra» URL..."
#define MSGTR_MENU_LoadSubtitle "Naèíta» titulky..."
#define MSGTR_MENU_DropSubtitle "Zahodi» titulky..."
#define MSGTR_MENU_LoadExternAudioFile "Naèíta» externý audio súbor..."
#define MSGTR_MENU_Playing "Prehrávam"
#define MSGTR_MENU_Play "Prehra»"
#define MSGTR_MENU_Pause "Pauza"
#define MSGTR_MENU_Stop "Zastavi»"
#define MSGTR_MENU_NextStream "Ïal¹í prúd"
#define MSGTR_MENU_PrevStream "Predchádzajúci prúd"
#define MSGTR_MENU_Size "Veµkos»"
#define MSGTR_MENU_HalfSize   "Polovièná velikos»"
#define MSGTR_MENU_NormalSize "Normálna veµkos»"
#define MSGTR_MENU_DoubleSize "Dvojnásobná veµkos»"
#define MSGTR_MENU_FullScreen "Celá obrazovka"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Prehra» disk..."
#define MSGTR_MENU_ShowDVDMenu "Zobrazi» DVD menu"
#define MSGTR_MENU_Titles "Tituly"
#define MSGTR_MENU_Title "Titul %2d"
#define MSGTR_MENU_None "(niè)"
#define MSGTR_MENU_Chapters "Kapitoly"
#define MSGTR_MENU_Chapter "Kapitola %2d"
#define MSGTR_MENU_AudioLanguages "Jazyk zvuku"
#define MSGTR_MENU_SubtitleLanguages "Jazyk titulkov"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Prehliadaè tém"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "Koniec..."
#define MSGTR_MENU_Mute "Stlmi» zvuk"
#define MSGTR_MENU_Original "Originál"
#define MSGTR_MENU_AspectRatio "Pomer strán obrazu"
#define MSGTR_MENU_AudioTrack "Audio stopa"
#define MSGTR_MENU_Track "Stopa %d"
#define MSGTR_MENU_VideoTrack "Video stopa"
#define MSGTR_MENU_Subtitles "Titulky"

// --- equalizer
// Note: If you change MSGTR_EQU_Audio please see if it still fits MSGTR_PREFERENCES_Audio
#define MSGTR_EQU_Audio "Audio"
// Note: If you change MSGTR_EQU_Video please see if it still fits MSGTR_PREFERENCES_Video
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Jas: "
#define MSGTR_EQU_Hue "Odtieò: "
#define MSGTR_EQU_Saturation "Nasýtenie: "
#define MSGTR_EQU_Front_Left "Predný ¥avý"
#define MSGTR_EQU_Front_Right "Predný Pravý"
#define MSGTR_EQU_Back_Left "Zadný ¥avý"
#define MSGTR_EQU_Back_Right "Zadný Pravý"
#define MSGTR_EQU_Center "Stredný"
#define MSGTR_EQU_Bass "Basový"
#define MSGTR_EQU_All "V¹etko"
#define MSGTR_EQU_Channel1 "Kanál 1:"
#define MSGTR_EQU_Channel2 "Kanál 2:"
#define MSGTR_EQU_Channel3 "Kanál 3:"
#define MSGTR_EQU_Channel4 "Kanál 4:"
#define MSGTR_EQU_Channel5 "Kanál 5:"
#define MSGTR_EQU_Channel6 "Kanál 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Cesta"
#define MSGTR_PLAYLIST_Selected "Vybrané súbory"
#define MSGTR_PLAYLIST_Files "Súbory"
#define MSGTR_PLAYLIST_DirectoryTree "Adresárový strom"

// --- preferences
#define MSGTR_PREFERENCES_Audio MSGTR_EQU_Audio
#define MSGTR_PREFERENCES_Video MSGTR_EQU_Video
#define MSGTR_PREFERENCES_SubtitleOSD "Titulky a OSD"
#define MSGTR_PREFERENCES_Codecs "Kódeky a demuxer"
// Poznámka: Pokiaµ zmeníte MSGTR_PREFERENCES_Misc, uistite sa prosím, ¾e vyhovuje aj pre MSGTR_PREFERENCES_FRAME_Misc
#define MSGTR_PREFERENCES_Misc "Rôzne"

#define MSGTR_PREFERENCES_None "Niè"
#define MSGTR_PREFERENCES_DriverDefault "východzie nastavenie"
#define MSGTR_PREFERENCES_AvailableDrivers "Dostupné ovládaèe:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nehra» zvuk"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizova» zvuk"
#define MSGTR_PREFERENCES_EnableEqualizer "Zapnú» equalizer"
#define MSGTR_PREFERENCES_SoftwareMixer "Aktivova» softvérový mixér"
#define MSGTR_PREFERENCES_ExtraStereo "Zapnú» extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Koeficient:"
#define MSGTR_PREFERENCES_AudioDelay "Audio oneskorenie"
#define MSGTR_PREFERENCES_DoubleBuffer "Zapnú» dvojtý buffering"
#define MSGTR_PREFERENCES_DirectRender "Zapnú» direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Povoli» zahadzovanie rámcov"
#define MSGTR_PREFERENCES_HFrameDrop "Povoli» TVRDÉ zahadzovanie rámcov (nebezpeèné)"
#define MSGTR_PREFERENCES_Flip "prehodi» obraz horná strana-dole"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Èas a indikátor"
#define MSGTR_PREFERENCES_OSDProgress "Iba ukazovateµ priebehu a nastavenie"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Èas, percentá and celkový èas"
#define MSGTR_PREFERENCES_Subtitle "Titulky:"
#define MSGTR_PREFERENCES_SUB_Delay "Oneskorenie: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozícia: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Zakáza» automatické nahrávanie titulkov"
#define MSGTR_PREFERENCES_SUB_Unicode "Titulky v Unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konvertova» dané titulky do MPlayer formátu"
#define MSGTR_PREFERENCES_SUB_SRT "Konvertova» dané titulky do èasovo-urèeného SubViewer (SRT) formátu"
#define MSGTR_PREFERENCES_SUB_Overlap "Zapnú» prekrývanie titulkov"
#define MSGTR_PREFERENCES_Font "Font:"
#define MSGTR_PREFERENCES_FontFactor "Font faktor:"
#define MSGTR_PREFERENCES_PostProcess "Zapnú» postprocess"
#define MSGTR_PREFERENCES_AutoQuality "Automatická qualita: "
#define MSGTR_PREFERENCES_NI "Pou¾i» neprekladaný AVI parser"
#define MSGTR_PREFERENCES_IDX "Obnovi» index tabulku, ak je potrebné"
#define MSGTR_PREFERENCES_VideoCodecFamily "Rodina video kodekov:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Rodina audeo kodekov:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD úroveò"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Titulky"
#define MSGTR_PREFERENCES_FRAME_Font "Font"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocess"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Kódek & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Vyrovnávacia pamä»"
#define MSGTR_PREFERENCES_FRAME_Misc MSGTR_PREFERENCES_Misc
#define MSGTR_PREFERENCES_Audio_Device "Zariadenie:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixér:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Kanál mixéru:"
#define MSGTR_PREFERENCES_Message "Prosím pamätajte, nietoré voµby potrebujú re¹tart prehrávania!"
#define MSGTR_PREFERENCES_DXR3_VENC "Video kóder:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Pou¾i» LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Pou¾i» FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Western European Languages (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Western European Languages with Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Slavic/Central European Languages (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galician, Maltese, Turkish (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Old Baltic charset (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cyrillic (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabic (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Modern Greek (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turkish (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltic (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celtic (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Hebrew charsets (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Russian (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrainian, Belarusian (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Simplified Chinese charset (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Traditional Chinese charset (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japanese charsets (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Korean charset (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thai charset (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrillic Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Slavic/Central European Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Nemeni» rozmery"
#define MSGTR_PREFERENCES_FontPropWidth "Proporcionálne k ¹írke obrazu"
#define MSGTR_PREFERENCES_FontPropHeight "Proporcionálne k vý¹ke obrazu"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporcionálne k diagonále obrazu"
#define MSGTR_PREFERENCES_FontEncoding "Kódovanie:"
#define MSGTR_PREFERENCES_FontBlur "Rozmazanie:"
#define MSGTR_PREFERENCES_FontOutLine "Obrys:"
#define MSGTR_PREFERENCES_FontTextScale "Mierka textu:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD mierka:"
#define MSGTR_PREFERENCES_Cache "Vyrovnávacia pamä» zap./vyp."
#define MSGTR_PREFERENCES_CacheSize "Veµkos» vyr. pamäte: "
#define MSGTR_PREFERENCES_LoadFullscreen "Na¹tartova» v re¾ime celej obrazovky"
#define MSGTR_PREFERENCES_SaveWinPos "Ulo¾i» pozíciu okna"
#define MSGTR_PREFERENCES_XSCREENSAVER "Zastavi» XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Zapnú» playbar"
#define MSGTR_PREFERENCES_AutoSync "Automatická synchronizácia zap./vyp."
#define MSGTR_PREFERENCES_AutoSyncValue "Automatická synchronizácia: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM zariadenie:"
#define MSGTR_PREFERENCES_DVDDevice "DVD zariadenie:"
#define MSGTR_PREFERENCES_FPS "Snímková rýchlos» (FPS):"
#define MSGTR_PREFERENCES_ShowVideoWindow "Ukáza» video okno pri neaktivite"
#define MSGTR_PREFERENCES_ArtsBroken "Nov¹ie verze aRts sú nekompatibilné "\
           "s GTK 1.x a zhavarujú GMPlayer!"

#define MSGTR_ABOUT_UHU "vývoj GUI sponzoroval UHU Linux\n"
#define MSGTR_ABOUT_Contributors "Pøispievatelia kódu a dokumentacie\n"
#define MSGTR_ABOUT_Codecs_libs_contributions "Kódeky a kni¾nice tretích strán\n"
#define MSGTR_ABOUT_Translations "Preklady\n"
#define MSGTR_ABOUT_Skins "Témy\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Fatálna chyba!"
#define MSGTR_MSGBOX_LABEL_Error "Chyba!"
#define MSGTR_MSGBOX_LABEL_Warning "Upozornenie!"

#endif

