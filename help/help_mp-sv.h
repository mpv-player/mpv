// Last sync on 2004-10-20 with help_mp-en.h 1.148
// Translated by:  Carl Fürstenberg <azatoth AT gmail DOT com>
// Helped by: Jan Knutar <jknutar AT nic DOT fi>
// ========================= MPlayer hjälp ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Användning:   mplayer [argument] [url|sökväg/]filnamn\n"
"\n"
"Grundläggande argument: (komplett lista återfinns i `man mplayer`)\n"
" -vo <drv[:enhet]>   välj video-ut drivrutin & enhet ('-vo help' för lista)\n"
" -ao <drv[:enhet]>   välj audio-ut drivrutin & enhet ('-ao help' för lista)\n"
#ifdef HAVE_VCD
" vcd://<spårnr>      spela (S)VCD (Super Video CD) spår (rå enhet, ingen montering)\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titlenr>     spela DVD titel från enhet istället för ifrån en enkel fil\n"
" -alang/-slang       välj DVD audio/textningsspråk (m.h.a. ett 2-teckens landskod)\n"
#endif
" -ss <tidpos>        sök till given position (sekunder eller hh:mm:ss)\n"
" -nosound            spela inte upp ljud\n"
" -fs                 fullskärmsuppspelning (eller -vm, -zoom, detaljer på manualsidan)\n"
" -x <x> -y <y>       sätt skärmupplösning (för användning med -vm eller -zoom)\n"
" -sub <fil>          specifiera textningsfil att använda (se också -subfps, -subdelay)\n"
" -playlist <fil>     specifiera spellistefil\n"
" -vid x -aid y       välj video (x) och audio (y) ström att spela\n"
" -fps x -srate y     ändra video (x fps) och audio (y Hz) frekvens\n"
" -pp <kvalité>       aktivera postredigeringsfilter (detaljer på manualsidan)\n"
" -framedrop          aktivera reducering av antalet bildrutor (för långsamma maskiner)\n" 
"\n"
"Grundläggande navigering: (komplett lista återfinns på manualsidan, läs även input.conf)\n"
" <-  eller  ->       sök bakåt/framåt 10 sekunder\n"
" upp eller ner       sök bakåt/framåt 1 minut\n"
" pgup eller pgdown   sök bakåt/framåt 10 minuter\n"
" < eller >           stega bakåt/framåt i spellistan\n"
" p eller SPACE       pausa filmen (tryck på valfri tagent för att fortsätta)\n"
" q eller ESC         stanna spelningen och avsluta programmet\n"
" + eller -           ställ in audiofördröjning med ± 0.1 sekund\n"
" o                   växla OSD läge:  ingen / lägesindikator / lägesindikator + tidtagare\n"
" * eller /           öka eller sänk PCM-volym\n"
" z eller x           ställ in textningsfördröjning med ± 0.1 sekund\n"
" r or t              ställ in textningsposition upp/ner, se också '-vf expand'\n"
"\n"
" * * * LÄS MANUALEN FÖR FLER DETALJER, MER AVANCERADE ARGUMENT OCH KOMMANDON * * *\n"
"\n";
#endif

// libmpcodecs/ad_dvdpcm.c:
#define MSGTR_SamplesWanted "Fler exempel på detta format behövs för att vidare öka support. Var vänlig kontakta utvecklarna.\n"

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nStänger ner...\n"
#define MSGTR_ExitingHow "\nStänger ner... (%s)\n"
#define MSGTR_Exit_quit "Avsluta"
#define MSGTR_Exit_eof "Slut på fil"
#define MSGTR_Exit_error "Oöverkomligt fel"
#define MSGTR_IntBySignal "\nMPlayer var avbruten av signal %d i modul: %s\n"
#define MSGTR_NoHomeDir "Kan inte lokalisera $HOME-katalog.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problem\n"
#define MSGTR_CreatingCfgFile "Skapar konfigfil: %s\n"
#define MSGTR_BuiltinCodecsConf "Använder standardinbyggd codecs.conf.\n"
#define MSGTR_CantLoadFont "Kan inte ladda font: %s\n"
#define MSGTR_CantLoadSub "Kan inte ladda vald textning: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATALT: Vald ström ej tillgänglig!\n"
#define MSGTR_CantOpenDumpfile "Kan inte öppna dumpfil.\n"
#define MSGTR_CoreDumped "Core dumpad ;)\n"
#define MSGTR_FPSnotspecified "FPS ej specifierad i filhuvudet eller är icke godkänd, använd argument -fps.\n"
#define MSGTR_TryForceAudioFmtStr "Försöker att forcera audiocodecfamilj %s...\n"
#define MSGTR_CantFindAudioCodec "Kan inte finna codec för audioformat 0x%X.\n"
#define MSGTR_RTFMCodecs "Läs DOCS/HTML/en/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Försöker att forcera videocodecfamilj %s...\n"
#define MSGTR_CantFindVideoCodec "Kan inte finna codec för vald -vo och videoformat 0x%X.\n"
#define MSGTR_CannotInitVO "FATALT: Kan inte initiera videodrivrutin.\n"
#define MSGTR_CannotInitAO "Kan inte öppna/initiera audioenhet -> inget ljud.\n"
#define MSGTR_StartPlaying "Påbörjar uppspelning...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"           ***********************************************************\n"\
"           **** Ditt system är för slött för att spela upp detta! ****\n"\
"           ***********************************************************\n\n"\
"Troliga orsaker, problem, samt sätt att fixa det:\n"\
"- Troligast: trasig/buggig _audio_drivrutin\n"\
"  - Försök -ao sdl eller använd OSS-emulatorn i ALSA.\n"\
"  - Experimentera med olika värden för -autosync, 30 är en bra start.\n"\
"- Seg video-ut\n"\
"  - Försök en annan -vo drivrutin (-vo help för en lista) eller försök -framedrop!\n"\
"- Seg CPU\n"\
"  - Försök att inte spela upp allt för stora DVD/DivX på en seg CPU! Testa med -hardframedrop.\n"\
"- Trasig fil\n"\
"  - Försök med olika kombinationer av -nobps -ni -forceidx -mc 0.\n"\
"- Segt media (NFS/SMB mounts, DVD, VCD etc.)\n"\
"  - Försök med -cache 8192.\n"\
"- Använder du -cache till att spela upp en ickeinterleaved AVIfil?\n"\
"  - Försök -nocache.\n"\
"Läs DOCS/HTML/en/video.html för optimeringstips.\n"\
"Om inget av dessa hjälper, läs DOCS/HTML/en/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer var kompilerad UTAN GUI-support.\n"
#define MSGTR_GuiNeedsX "MPlayer GUI kräver X11.\n"
#define MSGTR_Playing "Spelar %s.\n"
#define MSGTR_NoSound "Audio: inget ljud\n"
#define MSGTR_FPSforced "FPS forcerad att vara %5.3f  (ftime: %5.3f).\n"
#define MSGTR_CompiledWithRuntimeDetection "Kompilerad med \"runtime CPU detection\" - VARNING - detta är inte optimalt!\n"\
    "För att få bäst prestanda, omkompilera med '--disable-runtime-cpudetection'.\n"
#define MSGTR_CompiledWithCPUExtensions "Kompilerad för x86 med tillägg:"
#define MSGTR_AvailableVideoOutputDrivers "Tillgängliga video-ut-drivrutiner:\n"
#define MSGTR_AvailableAudioOutputDrivers "Tillgängliga audio-ut-drivrutiner:\n"
#define MSGTR_AvailableAudioCodecs "Tillgängliga audiocodec:\n"
#define MSGTR_AvailableVideoCodecs "Tillgängliga videocodec:\n"
#define MSGTR_AvailableAudioFm "Tillgängliga (inkompilerade) audiocodec familjer/drivrutiner:\n"
#define MSGTR_AvailableVideoFm "Tillgängliga (inkompilerade) videocodec familjer/drivrutiner:\n"
#define MSGTR_AvailableFsType "Tillgängliga lägen för fullskärmslager:\n"
#define MSGTR_UsingRTCTiming "Använder Linux's hårdvaru-RTC-tidtagning (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Video: Kan inte läsa inställningar.\n"
#define MSGTR_NoStreamFound "Ingen ström funnen.\n"
#define MSGTR_ErrorInitializingVODevice "Fel vid öppning/initiering av vald video_out-enhet (-vo).\n"
#define MSGTR_ForcedVideoCodec "Forcerad videocodec: %s\n"
#define MSGTR_ForcedAudioCodec "Forcerad audiocodec: %s\n"
#define MSGTR_Video_NoVideo "Video: ingen video\n"
#define MSGTR_NotInitializeVOPorVO "\nFATALT: Kunde inte initiera videofilter (-vf) eller video-ut (-vo).\n"
#define MSGTR_Paused "\n  =====  PAUSE  =====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nOförmögen att ladda spellista %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer krachade av en 'Illegal Instruction'.\n"\
"  Det kan vare en bugg i vår nya \"runtime CPU-detection\" kod...\n"\
"  Var god läs DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer krashade av en 'Illegal Instruction'.\n"\
"  Detta händer vanligast om du kör koden på en annan CPU än den var\n"\
"  kompilerad/optimerad för\n"\
"  Verifiera detta!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer krashade på grund utav dålig användning av CPU/FPU/RAM.\n"\
"  Omkompilera MPlayer med '--enable-debug' och kör en \"'gdb' backtrace\" och\n"\
"  deassemblera. Detaljer återfinns i DOCS/HTML/en/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer krashade. Detta borde inte inträffa.\n"\
"  Det kan vara en bugg i MPlayers kod, eller i din drivrutin, eller i din\n"\
"  gcc version. Om du tror det är MPlayers fel, var vänlig läs\n"\
"  DOCS/HTML/en/bugreports.html och följ instruktionerna där, Vi kan inte och\n"\
"  kommer inte att hjälpa dig, om du inte kan befodra denna information när \n"\
"  du rapporterar en trolig bugg.\n"
#define MSGTR_LoadingConfig "Laddar konfiguration '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: lade till textningsfil %d: %s \n"
#define MSGTR_ErrorOpeningOutputFile "Fel vid öppning av fil [%s] för skrivning!\n"
#define MSGTR_CommandLine "Kommandorad:"
#define MSGTR_RTCDeviceNotOpenable "Misslyckades att öppna %s: %s (den borde vara läsbar av användaren.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "'Linux RTC' initieringsfel i 'ioctl' rtc_irqp_set %lu: %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Försök lägg till \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" till ditt systems uppstartningsscript.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "'Linux RTC init' fel i 'ioctl' [rtc_pie_on]: %s\n"
#define MSGTR_UsingTimingType "Använder %s tidtagning.\n"
#define MSGTR_MenuInitialized "Meny initierad: %s\n"
#define MSGTR_MenuInitFailed "Menyinitiering misslyckades.\n"
#define MSGTR_Getch2InitializedTwice "VARNING: getch2_init anropad dubbelt!\n"
#define MSGTR_DumpstreamFdUnavailable "Kan inte dumpa denna ström - ingen 'fd' tillgänglig.\n"
#define MSGTR_FallingBackOnPlaylist "Faller tillbaka med att försöka tolka spellista %s...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Kan inte öppna 'libmenu video filter' med rotmeny %s.\n"
#define MSGTR_AudioFilterChainPreinitError "Fel vid förinitiering av audiofilter!\n"
#define MSGTR_LinuxRTCReadError "'Linux RTC' läsfel: %s\n"
#define MSGTR_SoftsleepUnderflow "Varning! Softsleep underflow!\n"
#define MSGTR_DvdnavNullEvent "DVDNAV-händelse NULL?!\n"
#define MSGTR_DvdnavHighlightEventBroken "DVDNAV-händelse: Highlight-händelse trasig\n" // FIXME highlight
#define MSGTR_DvdnavEvent "DVDNAV-händelse Event: %s\n"
#define MSGTR_DvdnavHighlightHide "DVDNAV-händelse: Highlight gömd\n"
#define MSGTR_DvdnavStillFrame "######################################## DVDNAV-händelse: Fortfarande bildruta: %d sekunder\n"
#define MSGTR_DvdnavNavStop "DVDNAV-händelse: Nav Stop\n" // FIXME Nav Stop?
#define MSGTR_DvdnavNavNOP "DVDNAV-händelse: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "DVDNAV-händelse: 'Nav SPU'-strömningsändring: fysisk: %d/%d/%d logisk: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "DVDNAV-händelse: 'Nav SPU'-strömningsändring: fysisk: %d logisk: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "DVDNAV-händelse: 'Nav Audio'-strömningsändring: fysisk: %d logisk: %d\n"
#define MSGTR_DvdnavNavVTSChange "DVDNAV-händelse: 'Nav VTS' ändrad\n"
#define MSGTR_DvdnavNavCellChange "DVDNAV-händelse: 'Nav Cell' ändrad\n"
#define MSGTR_DvdnavNavSpuClutChange "DVDNAV-händelse: 'Nav SPU CLUT' ändrad\n"
#define MSGTR_DvdnavNavSeekDone "DVDNAV-händelse: 'Nav Seek' ändrad\n"
/*
 * FIXME A lot of shorted words, not translating atm
 */
#define MSGTR_MenuCall "Menyanrop\n"

#define MSGTR_EdlOutOfMem "Kan inte allokera tillräckligt med minne för att hålla EDL-data.\n"
#define MSGTR_EdlRecordsNo "Läst %d EDL-funtioner.\n"
#define MSGTR_EdlQueueEmpty "Det är inga EDL-funktioner att ta hand om.\n"
#define MSGTR_EdlCantOpenForWrite "Kan inte öppna EDL-fil [%s] för skrivning.\n"
#define MSGTR_EdlCantOpenForRead "Kan inte öppna EDL-fil [%s] för läsning.\n"
#define MSGTR_EdlNOsh_video "Kan inte använda EDL utan video, inaktiverar.\n"
#define MSGTR_EdlNOValidLine "Icke godkänd EDL-rad: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Dåligt formaterad EDL-rad [%d]. Kastar bort.\n"
#define MSGTR_EdlBadLineOverlap "Senaste stopposition var [%f] ; nästa start är [%f]. Noteringar måste vara i kronologisk ordning, kan inte lappa över. Kastar bort.\n"
#define MSGTR_EdlBadLineBadStop "Stopptid måste vara efter starttid.\n"


// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Använder pass3-kontrollfil: %s\n"
#define MSGTR_MissingFilename "\nFilnamn saknas.\n\n"
#define MSGTR_CannotOpenFile_Device "Kan inte öppna fil/enhet.\n"
#define MSGTR_CannotOpenDemuxer "Kan inte öppna demuxer.\n"
#define MSGTR_NoAudioEncoderSelected "\nIngen audioencoder (-oac) vald. Välj en (se -oac help) eller använd -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nIngen videoencoder (-ovc) vald. Välj en (se -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Kan inte öppna utfil '%s'.\n"
#define MSGTR_EncoderOpenFailed "Misslyckade att öppna encodern.\n"
#define MSGTR_ForcingOutputFourcc "Forcerar utmatning 'fourcc' till %x [%.4s]\n" // FIXME fourcc?
#define MSGTR_DuplicateFrames "\n%d duplicerad bildruta/or!\n"
#define MSGTR_SkipFrame "\nHoppar över bildruta!\n"
#define MSGTR_ErrorWritingFile "%s: Fel vid skrivning till fil.\n"
#define MSGTR_RecommendedVideoBitrate "Rekommenderad videobitrate för %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nVideostöm: %8.3f kbit/s  (%d B/s)  storlek: %"PRIu64" byte  %5.3f sekunder  %d bildrutor\n"
#define MSGTR_AudioStreamResult "\nAudiostöm: %8.3f kbit/s  (%d B/s)  storlek: %"PRIu64" byte  %5.3f sekunder\n"
#define MSGTR_OpenedStream "klart: format: %d  data: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "videocodec: framecopy (%dx%d %dbpp fourcc=%x)\n" // FIXME translate?
#define MSGTR_ACodecFramecopy "audiocodec: framecopy (format=%x chans=%d rate=%d bits=%d B/s=%d sample-%d)\n" // -''-
#define MSGTR_CBRPCMAudioSelected "CBR PCM audio valt\n"
#define MSGTR_MP3AudioSelected "MP3 audio valt\n"
#define MSGTR_CannotAllocateBytes "Kunde inte allokera %d byte\n"
#define MSGTR_SettingAudioDelay "Sätter AUDIO DELAY till %5.3f\n"
#define MSGTR_SettingAudioInputGain "Sätter 'audio input gain' till %f\n" // FIXME to translate?
#define MSGTR_LamePresetEquals "\npreset=%s\n\n" // FIXME translate?
#define MSGTR_LimitingAudioPreload "Begränsar audioförinladdning till 0.4s\n" // preload?
#define MSGTR_IncreasingAudioDensity "Höjer audiodensitet till 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Forcerar audioförinladdning till 0, 'max pts correction' till 0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR audio: %d byte/sec, %d byte/block\n"
#define MSGTR_LameVersion "LAME version %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Fel: Angiven bitrate är utanför godkänd rymd för detta val\n"\
"\n"\
"Vid användning av detta val så måste du ange ett värde mellan \"8\" och \"320\"\n"\
"\n"\
"För vidare information testa: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Fel: du angav inte en godkänd profil och/eller förinställda val\n"\
"\n"\
"Tillgängliga profiler är:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - ABR-mode är underförstått. För att använda det,,\n"\
"                      helpt enkelt ange en bitrate. För exempel:\n"\
"                      \"preset=185\" aktiverar detta\n"\
"                      förinställda val, och använder 185 som ett genomsnittlig kbps.\n"\
"\n"\
"    Några exempel:\n"\
"\n"\
"       \"-lameopts fast:preset=standard  \"\n"\
" eller \"-lameopts  cbr:preset=192       \"\n"\
" eller \"-lameopts      preset=172       \"\n"\
" eller \"-lameopts      preset=extreme   \"\n"\
"\n"\
"För vidare information, försök: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
    "De förinställda switcharna är designade för att försörja den högsta möjliga kvalité.\n"\
"\n"\
"De har för mestadels blivit utsatta för och instämmnt via rigorösa dubbelblindslystningstester\n"\
"för att verifiera och åstakomma detta mål.\n"\
"\n"\
"Dessa är ideligen uppdaterade för att sammanträffa med de senaste utveckling som\n"\
"förekommer, och som result skulle försörja dig med bortåt den bästa kvalité\n"\
"för stunden möjligt från LAME.\n"\
"\n"\
"För att aktivera dessa förinställda värden:\n"\
"\n"\
"   För VBR-modes (generellt högsta kvalité) \b:\n"\
"\n"\
"     \"preset=standard\" Denna förinställning torde generellt vara transparent\n"\
"                             för de flesta för den mesta musik och har redan\n"\
"                             relativt hög kvalité.\n"\
"\n"\
"     \"preset=extreme\" Om du har extremt god hörsel och liknande utrustning,\n"\
"                             då kommer denna inställning generellt att tillgodose\n"\
"                             något högre kvalité än \"standard\"-inställningen\n"\
"\n"\
"   För 'CBR 320kbps' (högsta möjliga kvalité från förinställningsswitcharna):\n"\
                                                                               "\n"\
"     \"preset=insane\"  Denna förinställning kommer troligen att vara för mycket för de\n"\
"                             flesta och de flesta situationer, men om du måste absolut\n"\
"                             ha den högsta möjliga kvalité med inga invändningar om\n"\
"                             filstorleken så är detta den väg att gå.\n"\
"\n"\
"   För ABR-modes (hög kvalité per given bitrate, men inte så hög som för VBR) \b:\n"\
"\n"\
"     \"preset=<kbps>\"  Användning av denna inställning vill för det mesta ge dig god\n"\
"                             kvalité vid specifik bitrate, Beroende på angiven bitrate,\n"\
"                             denna inställning kommer att anta den mest optimala inställning\n"\
"                             för en optimal situation. Fast detta tillvägagångssätt fungerar,\n"\
"                             så är den inte tillnärmandesvis så flexibelt som VBR, och för det\n"\
"                             mesta så kommer den inte att komma åt samma nivå av kvalité som\n"\
"                             VBR vid högre bitrate.\n"\
"\n"\
"Följande inställningar är även tillgängliga för motsvarande profil:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR Mode) - ABR-mode är underförstått. För att använda det,\n"\
"                      helt enkelt ange en bitrate. För exempel:\n"\
"                      \"preset=185\" aktiverar denna inställning\n"\
"                      och använder 185 som ett genomsnittlig kbps.\n"\
"\n"\
"   \"fast\" - Aktiverar den nya snabba VBR föe en speciell profil.\n"\
"            Nackdel till snabbhetsswitchen är att oftast kommer\n"\
"            bitrate att vara något högre än vid 'normal'-mode\n"\
"            och kvalitén kan även bil något lägre.\n"\
"   Varning: Med aktuell version kan 'fast'-inställningen resultera i\n"\
"            för hör bitrate i jämförelse med ordinarie inställning.\n"\
"\n"\
"   \"cbr\"  - Om du använder ABR-mode (läs ovanstående) med en signifikant\n"\
"            bitrate, såsom 80, 96, 112, 128, 160, 192, 224, 256, 320,\n"\
"            du kan använda \"cbr\"-argument för att forcera CBR-modeskodning\n"\
"            istället för som standard ABR-mode. ABR gör högre kvalité\n"\
"            men CBR kan vara användbar i situationer såsom vid strömmande\n"\
"            av mp3 över internet.\n"\
"\n"\
"    Till exempel:\n"\
"\n"\
"    \"-lameopts fast:preset=standard  \"\n"\
" or \"-lameopts  cbr:preset=192       \"\n"\
" or \"-lameopts      preset=172       \"\n"\
" or \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Ett par alias är tillgängliga för ABR-mode:\n"\
"phone => 16kbps/mono        phon+/lw/mw-eu/sw => 24kbps/mono\n"\
"mw-us => 40kbps/mono        voice => 56kbps/mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_ConfigFileError "konfigurationsfilsfel"
#define MSGTR_ErrorParsingCommandLine "fel vid tolkning av cmdline"
#define MSGTR_VideoStreamRequired "Videoström är obligatoriskt!\n"
#define MSGTR_ForcingInputFPS "'input fps' kommer att bli tolkad som %5.2f istället\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Ut-filformat RAWVIDEO stödjer inte audio - deaktiverar audio\n"
#define MSGTR_DemuxerDoesntSupportNosound "Denna demuxer stödjer inte -nosound ännu.\n"
#define MSGTR_MemAllocFailed "minnesallokering misslyckades"
#define MSGTR_NoMatchingFilter "Kunde inte finna matchande filter/ao-format!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, kanske trasig C-kompilator?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, förkommet codecsnamn!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, kunde inte finna encoder för codec %s\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, kunde inte allokera kontext!\n"
#define MSGTR_CouldntOpenCodec "Kunde inte öppna codec %s, br=%d\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     variabel bitrate metod\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(default)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           medelbitrate\n"\
"\n"\
" cbr           konstant bitrate\n"\
"               Även forcerar CBR-modeskodning på subsequentiellt ABR-inställningsläge.\n"\
"\n"\
" br=<0-1024>   specifierar bitrate i kBit (CBR och ABR endast)\n"\
"\n"\
" q=<0-9>       kvalité (0-högst, 9-lägst) (endast för VBR)\n"\
"\n"\
" aq=<0-9>      algoritmiskt kvalité (0-bäst/segast, 9-sämst/snabbast)\n"\
"\n"\
" ratio=<1-100> kompressionsratio\n"\
"\n"\
" vol=<0-10>    sätt audio-in-ökning\n"\
"\n"\
" mode=<0-3>    (standard: auto)\n"\
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
" fast          Aktivera snabbare kodning på subsequentiellt VBR-inställningsläge,\n"\
"               något lägre kvalité och högre bitrate.\n"\
"\n"\
" preset=<value> Tillhandahåller den högsta tillgängliga kvalitétsinställning.\n"\
"                 medium: VBR  kodning, godkvalité\n"\
"                 (150-180 kbps bitratesrymd)\n"\
"                 standard:  VBR kodning, hög kvalité\n"\
"                 (170-210 kbps bitratesrymd)\n"\
"                 extreme: VBR kodning, mycket hög kvalité\n"\
"                 (200-240 kbps bitratesrymd)\n"\
"                 insane:  CBR  kodning, högsta förinställd kvalité\n"\
"                 (320 kbps bitrate)\n"\
"                 <8-320>: ABR kodning vid i medeltal angiven bitrate (kbps).\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "duplicerad FourCC"
#define MSGTR_TooManyFourccs "för många FourCCs/format..."
#define MSGTR_ParseError "tolkningsfel"
#define MSGTR_ParseErrorFIDNotNumber "tolkningsfel (format-ID är inget nummer?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "tolkningsfel (format-ID-alias är inget nummer?)"
#define MSGTR_DuplicateFID "duplicerade format-ID"
#define MSGTR_TooManyOut "för många ut..." //FIXME 'to many out'?
#define MSGTR_InvalidCodecName "\ncodec(%s) namn är icke godkänt!\n"
#define MSGTR_CodecLacksFourcc "\ncodec(%s) har inte FourCC/format!\n"
#define MSGTR_CodecLacksDriver "\ncodec(%s) har ingen drivrutin!\n"
#define MSGTR_CodecNeedsDLL "\ncodec(%s) behöver en 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\ncodec(%s) behöver en 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Kan inte allokera minne flr kommentar. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token() \b: max >= MAX_MR_TOKEN!" //FIXME translate?
#define MSGTR_ReadingFile "Läser %s: "
#define MSGTR_CantOpenFileError "Kan inte öppna '%s': %s\n"
#define MSGTR_CantGetMemoryForLine "Kan inte få minne för 'line': %s\n"
#define MSGTR_CantReallocCodecsp "Kan inte realloc '*codecsp': %s\n"
#define MSGTR_CodecNameNotUnique "Codec namn '%s' är inte unikt."
#define MSGTR_CantStrdupName "Kan inte strdup -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Kan inte strdup -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Kan inte strdup -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Kan inte strdup -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d audio & %d video codecs\n"
#define MSGTR_CodecDefinitionIncorrect "Codec är inte definerad korrekt."
#define MSGTR_OutdatedCodecsConf "Denna codecs.conf är för gammal och inkompatibel med denna MPlayer version!" // release is more like 'släpp', sounds wrong, using version instead

// fifo.c
#define MSGTR_CannotMakePipe "Kan inte skapa en PIPE!\n" // FIXME make? 

// m_config.c
#define MSGTR_SaveSlotTooOld "Allt för gammal sparningsslottar funna från nivå %d: %d !!!\n" // FIXME slot?
#define MSGTR_InvalidCfgfileOption "Alternativ %s kan inte användas i en konfigurationsfil\n"
#define MSGTR_InvalidCmdlineOption "Alternativ %s kan inte bli används som ett kommandoradsargument\n"
#define MSGTR_InvalidSuboption "Fel: alternativ '%s' har inga underalternativ '%s'\n" // FIXME suboption?
#define MSGTR_MissingSuboptionParameter "Fel: underalternativ '%s' av '%s' måste ha en parameter!\n"
#define MSGTR_MissingOptionParameter "Fel: alternativ '%s' måste ha en parameter!\n"
#define MSGTR_OptionListHeader "\n Namn                 Typ             Min        Max      Global  CL    Cfg\n\n" // TODO why static tabs?
#define MSGTR_TotalOptions "\nTotalt: %d alternativ\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM-enhet '%s' ej funnet.\n"
#define MSGTR_ErrTrackSelect "Fel vid val av VCD-spår."
#define MSGTR_ReadSTDIN "Läser från stdin...\n"
#define MSGTR_UnableOpenURL "Oförmögen att öppna URL: %s\n"
#define MSGTR_ConnToServer "Ansluten till server: %s\n"
#define MSGTR_FileNotFound "Fil ej funnen: '%s'\n"

#define MSGTR_SMBInitError "Kan inte initiera libsmbclient-bilioteket: %d\n"
#define MSGTR_SMBFileNotFound "Kunde inte öppna från LAN: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer var inte kompilerad med SMB-lässtöd.\n"

#define MSGTR_CantOpenDVD "Kunde inte öppna DVD-enhet: %s\n"
#define MSGTR_DVDnumTitles "Det är %d titlar på denna DVD.\n"
#define MSGTR_DVDinvalidTitle "Icke godkänt DVD-titelnummer: %d\n"
#define MSGTR_DVDnumChapters "Der är %d kapitel på denna DVD-titel.\n"
#define MSGTR_DVDinvalidChapter "Ej godkänt DVD-kapitelnummer: %d\n"
#define MSGTR_DVDnumAngles "Det är %d vinkar på denna DVD-titel.\n"
#define MSGTR_DVDinvalidAngle "Ej godkänd DVD-vinkelsnummer: %d\n"
#define MSGTR_DVDnoIFO "Kan inte öppna IFO-fil för DVD-titel %d.\n"
#define MSGTR_DVDnoVOBs "Kunde inte öppna titel VOBS (VTS_%02d_1.VOB).\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "VARNING: Audioströmsfilhuvud %d omdefinerad.\n"
#define MSGTR_VideoStreamRedefined "WARNING: Videoströmsfilhuvud %d omdefinerad.\n"
#define MSGTR_TooManyAudioInBuffer "\nAllt för många audiopaket i bufferten: (%d i %d byte).\n"
#define MSGTR_TooManyVideoInBuffer "\nAllt för många videopaket i bufferten: (%d i %d byte).\n"
#define MSGTR_MaybeNI "Kanske försöker du spela upp en icke-interleaved ström/fil, eller så har decodern falierat?\n" \
                      "För AVI-filer, försök med att forcera icke-interleaved-lägen med -ni argumentet.\n" // FIXME non-interleaved
#define MSGTR_SwitchToNi "\nSvårt interleaved AVI-fil detekterad, går över till '-ni'-läge...\n"
#define MSGTR_Detected_XXX_FileFormat "%s filformat detekterat.\n"
#define MSGTR_DetectedAudiofile "Audiofilformat detekterat.\n"
#define MSGTR_NotSystemStream "Icke 'MPEG System Stream'-format... (kanske Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Icke godkänd 'MPEG-ES'-ström??? Kontakta upphovsmannen, det kanske är en bugg :(\n" //FIXME author???
#define MSGTR_FormatNotRecognized "================ Tyvärr, detta filformat är inte rekogniserbart/stött ==================\n"\
                                  "=== Om denna fil är en AVi, ASF eller MPEG-ström, var vänlig kontakta upphovsmannen! ===\n" //FIXME author???
#define MSGTR_MissingVideoStream "Ingen videoström funnen.\n"
#define MSGTR_MissingAudioStream "Ingen audioström funnen -> inget ljud.\n"
#define MSGTR_MissingVideoStreamBug "Saknar videoström!? Kontakta upphovsmannen, det kan vara en bugg :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Fil innehåller ej den valda audio- eller videoströmmen.\n"

#define MSGTR_NI_Forced "Forcerad"
#define MSGTR_NI_Detected "Påvisad" // FIXME right to say?
#define MSGTR_NI_Message "%s 'NON-INTERLEAVED AVI'-filformat.\n"

#define MSGTR_UsingNINI "Använder trasig 'NON-INTERLEAVED AVI'-filformat.\n"
#define MSGTR_CouldntDetFNo "Kunde inte avgöra antalet bildrutor (för absolut sökning).\n"
#define MSGTR_CantSeekRawAVI "Kan inte söka i råa AVI-strömmar. (Index krävs, försök med '-idx'-switchen.)\n"
#define MSGTR_CantSeekFile "Kan inte söka i denna fil.\n"

#define MSGTR_MOVcomprhdr "MOV: filhuvudkomprimeringssupport kräver ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: VARNING: Variabel FOURCC påvisad!?\n"
#define MSGTR_MOVtooManyTrk "MOV: VARNING: allt förmånga spår"
#define MSGTR_FoundAudioStream "==> Fann audioström: %d\n"
#define MSGTR_FoundVideoStream "==> Fann videoström: %d\n"
#define MSGTR_DetectedTV "TV påvisad! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Oförmögen att öppna oggdemuxern.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Söker efter audioström (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Kan inte öppna audioström: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Kan inte öppna textningsström: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Misslyckades att öppna audiodemuxern: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Misslyckades att öppna textningsdemuxern: %s\n"
#define MSGTR_TVInputNotSeekable "TV-in är inte sökbar! (Sökning kommer troligen bli för att ändra kanal ;)\n"
#define MSGTR_ClipInfo "Clip-info:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: '30fps NTSC'-innehåll upptäckt, ändrar framerate.\n" // FIXME framerate?
#define MSGTR_EnterTelecineMode "\ndemux_mpg: '24fps progressive NTSC'-innehåll upptäckt, ändrar framerate.\n" // -''-

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Kunde inte öppna codec.\n"
#define MSGTR_CantCloseCodec "Kunde inte stänga codec\n"

#define MSGTR_MissingDLLcodec "FEL: Kunde inte öppna obligatorisk DirecShow-codec %s.\n"
#define MSGTR_ACMiniterror "Kunde inte ladda/initiera 'Win32/ACM AUDIO'-codec (saknas Dll-fil?).\n"
#define MSGTR_MissingLAVCcodec "Kunde inte finna codec '%s' i libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF under sökning efter sequencefilhuvuden\n" // FIXME sequence?
#define MSGTR_CannotReadMpegSequHdr "FATAL: Kunde inte läsa sequencefilhuvud\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Kunde inte läsa sequencefilhuvudstillägg.\n"
#define MSGTR_BadMpegSequHdr "MPEG: dålig sequencefilhuvud\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: dålig sequencefilhuvudstillägg\n"

#define MSGTR_ShMemAllocFail "Kunde inte allokera delat minne.\n"
#define MSGTR_CantAllocAudioBuf "Kunde inte allokera audio-ut-buffert.\n"

#define MSGTR_UnknownAudio "Okänd/saknad audioformat -> inget ljud\n"

#define MSGTR_UsingExternalPP "[PP] Använder externt postprocesseringsfiler, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Använder codecens postprocessing, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Videoattribut '%s' har inget stöd hos vald vo & vd.\n" // FIXME more info? vo & vd
#define MSGTR_VideoCodecFamilyNotAvailableStr "Begärd videocodecfamilj [%s] (vfm=%s) är ej tillgänglig.\nAktivera det vil kompilation.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Begärd audiocodecfamilj [%s] (afm=%s) är ej tillgänglig.\nAktivera det vil kompilation.\n"
#define MSGTR_OpeningVideoDecoder "Öppnar videodecoder: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Öppnar audiodecoder: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit video: %s\n" // FIXME translate?
#define MSGTR_UninitAudioStr "uninit audio: %s\n" // -''-
#define MSGTR_VDecoderInitFailed "VDecoder-initiering misslyckades :(\n" // FIXME VDecoder something special or just a shortcut?
#define MSGTR_ADecoderInitFailed "ADecoder-initiering misslyckades :(\n" // -''-
#define MSGTR_ADecoderPreinitFailed "ADecoder-preinitiering misslyckades :(\n" // -''-
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Allokerar %d byte för inbuffert.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Allokerar %d + %d = %d byte för utbuffert.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Aktiverar LIRC-stöd...\n"
#define MSGTR_LIRCopenfailed "Misslyckades med att aktivera LIRC-stöd.\n"
#define MSGTR_LIRCcfgerr "Misslyckades med att läsa LIRC-konfigurationsfil %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Kunde inte finna videofilter '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "Kunde inte öppna videofilter '%s'.\n"
#define MSGTR_OpeningVideoFilter "Öppnar videofilter: "
#define MSGTR_CannotFindColorspace "Kunde inte hitta matchande färgrymder, t.o.m. vid insättning av 'scale' :(\n" // FIXME colorspace

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Codec satt inte sh->disp_w samt sh->disp_h, försöker gå runt problemet.\n"
#define MSGTR_VoConfigRequest "VDec: vo-konfigurationsbegäran - %d x %d (preferred csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Kunde inte finna matchande färgrymder - försöker åter med -vf scale...\n" // -''-
#define MSGTR_MovieAspectIsSet "Movie-Aspect är %.2f:1 - prescaling till korrekt film-aspect.\n"
#define MSGTR_MovieAspectUndefined "Film-Aspect är ej definerad - ingen prescaling kommer att äga rum.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Du måste uppgradera/installera de binära codecspaketen.\nGå till http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: 'Win32/DShow'-videocodecinitiering: OK.\n"
#define MSGTR_DMOInitOK "INFO: 'Win32/DMO'-videocodecinitiering: OK.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Kunde inte sända EWMH-fullskärmshändelse!\n"

#define MSGTR_InsertingAfVolume "[Mixer] Ingen hårdvarumixning, lägger till volymfilter.\n"
#define MSGTR_NoVolume "[Mixer] Ingen volymkontroll tillgänglig.\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Om"
#define MSGTR_FileSelect "Välj fil..."
#define MSGTR_SubtitleSelect "Välj textning..."
#define MSGTR_OtherSelect "Välj..."
#define MSGTR_AudioFileSelect "Välj extern audiokanal..."
#define MSGTR_FontSelect "Välj font..."
// NOTE: If you change MSGTR_PlayList pleace see if it still fits MSGTR_MENU_PlayList
#define MSGTR_PlayList "Spellista"
#define MSGTR_Equalizer "Equalizer" 
#define MSGTR_SkinBrowser "Skinläsare"
#define MSGTR_Network "Nätverksströmning..."
// NOTE: If you change MSGTR_Preferences pleace see if it still fits MSGTR_MENU_Preferences
#define MSGTR_Preferences "Inställningar"
#define MSGTR_AudioPreferences "Audiodirvrutinskonfiguration"
#define MSGTR_NoMediaOpened "Inget media öppnad"
#define MSGTR_VCDTrack "VCD-spår %d"
#define MSGTR_NoChapter "Inget kapitel"
#define MSGTR_Chapter "Kapitel %d"
#define MSGTR_NoFileLoaded "Ingen fil laddad"

// --- buttons ---
#define MSGTR_Ok "OK"
#define MSGTR_Cancel "Avbryt"
#define MSGTR_Add "Lägg till"
#define MSGTR_Remove "Radera"
#define MSGTR_Clear "Rensa"
#define MSGTR_Config "Konfiguration"
#define MSGTR_ConfigDriver "Konfigurera drivrution"
#define MSGTR_Browse "Bläddra"

// --- error messages ---
#define MSGTR_NEMDB "Tyvärr, inte tillräckligt minne för ritbuffert."
#define MSGTR_NEMFMR "Tyvärr, inte tillräckligt minne för menyrendering."
#define MSGTR_IDFGCVD "Tyvärr, jag hittade inte en GUI-kompatibel video-ut-drivrutin."
#define MSGTR_NEEDLAVC "Tyvärr, du kan inte spela icke-MPEG-filer med ditt DXR3/H+-enhet utan omkodning.\nVar god aktivera lavc i 'DXR3/H+'-konfigurationsboxen."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] fel i skinkonfigureringsfil på rad %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] varning i konfigurationsfil på rad %d:\nwidget (%s) funnen, men ingen \"section\" funnen före"
#define MSGTR_SKIN_WARNING2 "[skin] varning i konfigurationsfil på rad %d:\nwidget (%s) funnen, men ingen \"subsection\" funnen före"
#define MSGTR_SKIN_WARNING3 "[skin] varning i konfigurationsfil på rad %d:\ndenna undersektion stödjs inte av widget (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16-bitar eller lägre bitmappar stödjs inte (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "fil ej funnen (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP läsfel (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA läsfel (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG läsfel (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE-packad TGA stödjs ej (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "okänd filtyp (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "24-bitars till 32-bitars konverteringsfel (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "okänt meddelande: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "ej tillräckligt minne\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Allt för många fonter deklarerade.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Fontfil ej funnen.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Fontbildsfil ej funnen.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "icke-existerande fontidentifkator (%s)\n"
#define MSGTR_SKIN_UnknownParameter "okänd parameter (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin ej funnen (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "skinkonfigurationsfilsläsfel (%s)\n"
#define MSGTR_SKIN_LABEL "Skin:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Om MPlayer"
#define MSGTR_MENU_Open "Öppna..."
#define MSGTR_MENU_PlayFile "Spela fil..."
#define MSGTR_MENU_PlayVCD "Spela VCD..."
#define MSGTR_MENU_PlayDVD "Spela DVD..."
#define MSGTR_MENU_PlayURL "Spela URL..."
#define MSGTR_MENU_LoadSubtitle "Ladda textning..."
#define MSGTR_MENU_DropSubtitle "Droppa textning..."
#define MSGTR_MENU_LoadExternAudioFile "Ladda extern audiofil..."
#define MSGTR_MENU_Playing "Spelar"
#define MSGTR_MENU_Play "Spela"
#define MSGTR_MENU_Pause "Pausa"
#define MSGTR_MENU_Stop "Stopp"
#define MSGTR_MENU_NextStream "Nästa ström"
#define MSGTR_MENU_PrevStream "Föregående ström"
#define MSGTR_MENU_Size "Storlek"
#define MSGTR_MENU_NormalSize "Normal storlek"
#define MSGTR_MENU_DoubleSize "Dubbel storlek"
#define MSGTR_MENU_FullScreen "Fullskärm"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Öppnar disk..." // FIXME to open or is opening?
#define MSGTR_MENU_ShowDVDMenu "Visa DVD-meny"
#define MSGTR_MENU_Titles "Titlar"
#define MSGTR_MENU_Title "Titel %2d"
#define MSGTR_MENU_None "(ingen)"
#define MSGTR_MENU_Chapters "Kapitel"
#define MSGTR_MENU_Chapter "Kapitel %2d"
#define MSGTR_MENU_AudioLanguages "Audiospråk"
#define MSGTR_MENU_SubtitleLanguages "Textningsspråk"
#define MSGTR_MENU_SkinBrowser "Skinläsare"
#define MSGTR_MENU_Exit "Avsluta..."
#define MSGTR_MENU_Mute "Dämpa"
#define MSGTR_MENU_Original "Orginal"
#define MSGTR_MENU_AspectRatio "Aspect ratio" // FIXME translate?
#define MSGTR_MENU_AudioTrack "Audiospår"
#define MSGTR_MENU_Track "Spår %d"
#define MSGTR_MENU_VideoTrack "Videospår"

// --- equalizer
// Note: If you change MSGTR_EQU_Audio please see if it still fits MSGTR_PREFERENCES_Audio
#define MSGTR_EQU_Audio "Audio"
// Note: If you change MSGTR_EQU_Video please see if it still fits MSGTR_PREFERENCES_Video
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Ljusstyrka: "
#define MSGTR_EQU_Hue "Hue: "
#define MSGTR_EQU_Saturation "Saturation: "
#define MSGTR_EQU_Front_Left "Vänster fram"
#define MSGTR_EQU_Front_Right "Höger fram"
#define MSGTR_EQU_Back_Left "Vänster bak"
#define MSGTR_EQU_Back_Right "Höger bak"
#define MSGTR_EQU_Center "Center"
#define MSGTR_EQU_Bass "Bass"
#define MSGTR_EQU_All "Allt"
#define MSGTR_EQU_Channel1 "Kanal 1:"
#define MSGTR_EQU_Channel2 "Kanal 2:"
#define MSGTR_EQU_Channel3 "Kanal 3:"
#define MSGTR_EQU_Channel4 "Kanal 4:"
#define MSGTR_EQU_Channel5 "Kanal 5:"
#define MSGTR_EQU_Channel6 "Kanal 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Sökväg"
#define MSGTR_PLAYLIST_Selected "Valda filer"
#define MSGTR_PLAYLIST_Files "Filer"
#define MSGTR_PLAYLIST_DirectoryTree "Katalogträd"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "Textning & OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs & demuxer"
// NOTE: If you change MSGTR_PREFERENCES_Misc see if it still fits MSGTR_PREFERENCES_FRAME_Misc
#define MSGTR_PREFERENCES_Misc "Diverse"

#define MSGTR_PREFERENCES_None "Inget"
#define MSGTR_PREFERENCES_DriverDefault "standarddrivrutin"
#define MSGTR_PREFERENCES_AvailableDrivers "Tillgängliga drivrutioner:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Spela inte upp ljud"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizera ljud"
#define MSGTR_PREFERENCES_EnableEqualizer "AKtivera equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "Aktivera extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Koefficient:"
#define MSGTR_PREFERENCES_AudioDelay "Audiofördröjning"
#define MSGTR_PREFERENCES_DoubleBuffer "Aktivera double buffering"
#define MSGTR_PREFERENCES_DirectRender "Aktivera direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Aktivera frame dropping"
#define MSGTR_PREFERENCES_HFrameDrop "Aktivera HÅRD frame dropping (dangerous)"
#define MSGTR_PREFERENCES_Flip "Flippa bilden uppochner"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Timers och indikatorer"
#define MSGTR_PREFERENCES_OSDProgress "Tillståndsrad endast"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Timer, procent och total tid"
#define MSGTR_PREFERENCES_Subtitle "Textning:"
#define MSGTR_PREFERENCES_SUB_Delay "Fördröjning: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Position: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Deaktivera automatisk laddning av textning"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicodetextning"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konvertera given text till MPlayers egna textningsformat"
#define MSGTR_PREFERENCES_SUB_SRT "Konvertera given text till det tidbaserade SubViewer (SRT) formatet"
#define MSGTR_PREFERENCES_SUB_Overlap "Aktivera textningsöverlappning"
#define MSGTR_PREFERENCES_Font "Font:"
#define MSGTR_PREFERENCES_FontFactor "Fontfaktor:"
#define MSGTR_PREFERENCES_PostProcess "Aktivera postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Autokvalité: "
#define MSGTR_PREFERENCES_NI "Använd non-interleaved AVI tolk"
#define MSGTR_PREFERENCES_IDX "Återbygg indextabell, om så behövs"
#define MSGTR_PREFERENCES_VideoCodecFamily "Videocodecfamilj:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Audiocodecfamilj:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD-nivå"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Textning"
#define MSGTR_PREFERENCES_FRAME_Font "Font"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache"
#define MSGTR_PREFERENCES_Audio_Device "Enhet:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Mixerkanal:"
#define MSGTR_PREFERENCES_Message "Var god komihåg att du måste starta om uppspelning för att vissa ändringar ska ta effekt!"
#define MSGTR_PREFERENCES_DXR3_VENC "Videoencoder:"
#define MSGTR_PREFERENCES_DXR3_LAVC "ANvänd LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Västeuropeiska språk (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Västeuropeiska språk med Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Slaviska/Centraleuropeiska språk (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galician, Maltese, Turkiska (ISO-8859-3)" // FIXME Galician, Maltese
#define MSGTR_PREFERENCES_FontEncoding6 "Äldre baltisk teckenuppsättning (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Kyrilliska (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabiska (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Modern grekiska (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turkiska (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltiska (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celtiska (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Hebrew teckenuppsättningar (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Ryska (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrainska, Vitrysska (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Enkel Kinesisk teckenuppsättning (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Traditionell Kinesisk teckenuppsättning (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japansk teckenuppsättning (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Koreansk teckenuppsättning (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thailänsk teckenuppsättning (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Kyrilliska Windown (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Slaviska/Centraleuropeiska Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Ingen autoskalning"
#define MSGTR_PREFERENCES_FontPropWidth "Propotionellt mot filmbredd"
#define MSGTR_PREFERENCES_FontPropHeight "Propotionellt mot filmhöjd"
#define MSGTR_PREFERENCES_FontPropDiagonal "Propotionellt mot filmdiagonalen"
#define MSGTR_PREFERENCES_FontEncoding "Kodning:"
#define MSGTR_PREFERENCES_FontBlur "Blur:"
#define MSGTR_PREFERENCES_FontOutLine "Outline:"
#define MSGTR_PREFERENCES_FontTextScale "Textskalning:"
#define MSGTR_PREFERENCES_FontOSDScale "OSDskalning:"
#define MSGTR_PREFERENCES_Cache "Cache på/av"
#define MSGTR_PREFERENCES_CacheSize "Cachestorlek: "
#define MSGTR_PREFERENCES_LoadFullscreen "Starta i fullskärm"
#define MSGTR_PREFERENCES_SaveWinPos "Spara fönsterposition"
#define MSGTR_PREFERENCES_XSCREENSAVER "Stoppa XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Aktivera spelindikator"
#define MSGTR_PREFERENCES_AutoSync "AutoSync på/av"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM-enhet:"
#define MSGTR_PREFERENCES_DVDDevice "DVD-enhet:"
#define MSGTR_PREFERENCES_FPS "Film-FPS:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Visa videofönster när den är inaktiv"

#define MSGTR_ABOUT_UHU "GUI-utveckling sponstrat av UHU Linux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Oöverkomligt fel!"
#define MSGTR_MSGBOX_LABEL_Error "Fel!"
#define MSGTR_MSGBOX_LABEL_Warning "Varning!"

#endif

// ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Vald video-ut-enhet är inte kompatibel med denna codec.\n"
#define MSGTR_VO_GenericError "Detta fel har inträffat"
#define MSGTR_VO_UnableToAccess "Kan inte accessa"
#define MSGTR_VO_ExistsButNoDirectory "finns redan, men är inte en katalog."
#define MSGTR_VO_DirExistsButNotWritable "Ut-katalog finns redan, men är inte skrivbar."
#define MSGTR_VO_DirExistsAndIsWritable "Utkatalog finns redan och är skrivbar."
#define MSGTR_VO_CantCreateDirectory "Oförmögen att skapa ut-katalog."
#define MSGTR_VO_CantCreateFile "Oförmögen att skapa utfil."
#define MSGTR_VO_DirectoryCreateSuccess "Ut-katalog skapad."
#define MSGTR_VO_ParsingSuboptions "Tolkar suboptions." // FIXME suboptions?
#define MSGTR_VO_SuboptionsParsedOK "Suboptions tolkad OK." // -''-
#define MSGTR_VO_ValueOutOfRange "Värden utanför godkänd rymd"
#define MSGTR_VO_NoValueSpecified "Inget värde angett."
#define MSGTR_VO_UnknownSuboptions "Okänd suboption" // -''-

// vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "'Progressive JPEG' aktiverat."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "'Progressive JPEG' deaktiverat."
#define MSGTR_VO_JPEG_BaselineJPEG "'Baseline JPEG' aktiverat."
#define MSGTR_VO_JPEG_NoBaselineJPEG "'Baseline JPEG' deaktiverat."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "ASCII-mode aktiverat."
#define MSGTR_VO_PNM_RawMode "Rått-mode aktiverat." // FIXME Rått sounds strange
#define MSGTR_VO_PNM_PPMType "Kommer att skriva PPM-filer."
#define MSGTR_VO_PNM_PGMType "Kommer att skriva PGM-filer."
#define MSGTR_VO_PNM_PGMYUVType "Kommer att skriva PGMYUV-filer."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "'Interlaced'-mode kräver bildhöjd som är delbar med 4." // FIXME interlaced?
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Oförmögen att allokera linjebufferrt för interlaced-mode."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "indata är ej i RGB-format, kan inte separera 'chrominance' via fält!" // FIXME chrominance
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "Bildbredd måste vara delbart med 2."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Ej tillräckligt med minne för att allokera RGB-bildramsbuffert."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Kan inte få minnes- eller filhanterare att skriva till \"%s\"!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Fel vid skrivning av bild till ut!" // FIXME output here?
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Okänd subdevice: %s" // FIXME subdevice
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Använder 'interlaced output mode', övre fältet först." // FIXME top-field first? && 'interlaced output mode'
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Använder 'interlaced output mode',nedre fältet först." // FIXME bottom-field first? && 'interlaced output mode'

#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Använder (som standard) progressiv bildramsinställning."

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "pgm-video-ut-drivrutinen har blivit utbytt av '-vo pnm:pgmyuv'.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "md5-video-ut-drivrutinen har blivit utbytt av '-vo md5sum'.\n"


// ======================= AO Audio Output drivers ========================

// libao2 

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: alsa9- samt alsa1xmodulerna har blivit borttagna, använd -ao istället.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Kan inte öppna mixernehet %s: %s\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: Audiokortsmixer har inte kanal '%s' använder standard.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Kan inte öppna audioenhet %s: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Kan inte få till 'filedescriptor'sblockning: %s\n" // FIXME filedescriptor
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Misslyckades att sätta audioenhet till %d kanaler.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: dirvrutin hanerar ej SNDCTL_DSP_GETOSPACE :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   ***  Din audiodrivrutin hanterar inte select()  ***\n Komplilera om med '#undef HAVE_AUDIO_SELECT' i config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nFatalt fel: *** CAN INTE BLI ÅTERÖPPNAD / ÅTERSTÄLLER AUDIOENHET *** %s\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n" // FIXME nothing?
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Anslutet till ljudserver.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Oförmögen att öppna en ström.\n" // FIXME 'ström' or 'ljudström'?
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Ström öppnad.\n" // -''-
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] buffertstorlek: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Sättning av volym till %d misslyckades.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] dxr2: %d Hz är ej tillgänglig, försök med \"-aop list=resample\"\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound misslyckades: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] latency: [server: %0.2fs, net: %0.2fs] (adjust %0.2fs)\n" // FIXME translate?
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] misslyckades att öppna uppspelningsström: %s\n" 

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] DVB-audio-sättningsmixer misslyckades: %s\n" // set ~= sättning?
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz ej tillgänglig, försöker resampla...\n"

// ao_null.c
// This one desn't even have any mp_msg nor printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Fil: %s (%s)\nPCM: Samplerate: %iHz Kanaler: %s Format %s\n" // FIXME Samplerate?
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: snabbaste dumplning är tillgänglig via -vc dummy -vo null\nPCM: Info: för att skriva WAVE-filer använd -ao pcm:waveheader (standard).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Misslyckades att öppna %s för skrivning!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Samplerate: %iHz Kanaler: %s Format %s\n" // -''-
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] använder %s som audioenhet.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Icke tillgängligt audioformat: 0x%x.\n" // support?
#define MSGTR_AO_SDL_CantInit "[AO SDL] Initialisering av 'SDL Audio' misslyckades: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Oförmögen att öppna audio: %s\n" // audio what?

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] kontroll.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Samplerate: %iHz Kanaler: %s Format %s\n" // FIXME Samplerate
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] play: icke godkänd enhet.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparams misslyckades: %s\nKunde inte sätta önskad samplerate.\n" // -''-
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE var inte accepterad på given resurs.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams misslyckades: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: samplerate är nu %lf (önskad rate var %lf)\n" // -''- also rate?
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Oförmögen att öppna audiokanal: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n" // FIXME translate?
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n" // -''-
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n" // -''-
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n" // -''-

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO misslyckades.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: skrivning misslyckades."
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Kan inte öppna audioenhet %s, %s  -> inget ljud.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: ditt kort hanterar inte %d kanaler, %s, %d Hz samplerate.\n" // FIXME samplerate
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   ***  Din ljudkortsenhet hanterar inte select()  ***\nKompilera om med '#undef HAVE_AUDIO_SELECT' i config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nFatalt fel: *** KAN INTE ÅTERÖPPNA / ÅTERSTÄLLA AUDIOENHET (%s) ***\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: önskat format: %d Hz, %d kanaler, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: inga ljudkort funna.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: icke godkänt format (%s) önskat - ut deaktiverat.\n" // FIXME output -> ut here?
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: uppspelningsöppningsfel: %s\n" 
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: pcm-infofel: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: %d ljurtkort funna, använder: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: pcm-kanalinfofel: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: fel vid sättning av parametrarna: %s\n" // FIXME setting?
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: fel vid initiering av kanal: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: kanalprepareringsfel: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: uppspelningslänsningsfel: %s\n" // FIXME drain -> länsning?
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: uppspelningsspolningsfel: %s\n" // FIXME flush -> spolning?
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: pcm-stängningsfel: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: uppspelningslänsningsfel: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: uppspelningsspolningsfel: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: kanalprepareringsfel: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: uppspelningslänsningsfel: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: uppspelningsspolningsfel: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: kanalprepareringsfel: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: alsa underrun, återställer ström.\n" // FIXME underun - translate?
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: uppspelningsprepareringsfel: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: skrivfel efter återställning: %s - ger upp.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: utfel: %s\n" // FIXME output -> ut her?

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] icke godkänd plugin: %s\n" // FIXME plugin - translate?

