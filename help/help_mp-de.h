// Translated by: Johannes Feigl <johannes.feigl@aon.at>
// Reworked by Klaus Umbach <klaus.umbach@gmx.net>
// Moritz Bunkus <moritz@bunkus.org>
// Alexander Strasser <eclipse7@gmx.net>
// Sebastian Krämer <mplayer@skraemer.de>

// In synch with rev 1.153

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Verwendung:   mplayer [Optionen] [URL|Verzeichnis/]Dateiname\n"
"\n"
"Basisoptionen: (siehe Manpage für eine vollständige Liste aller Optionen!)\n"
" -vo <drv[:dev]>  Videoausgabetreiber & -gerät ('-vo help' für eine Liste)\n"
" -ao <drv[:dev]>  Audioausgabetreiber & -gerät ('-ao help' für eine Liste)\n"
#ifdef HAVE_VCD
" vcd://<tracknr>   Spiele einen (S)VCD-Titel (Super Video CD) ab\n"
"                   ( direkter Gerätezugriff, kein mount! )\n"
#endif
#ifdef USE_DVDREAD
" dvd://<titelnr>   Spiele DVD-Titel direkt vom Gerät anstelle einer Datei\n"
" -alang/-slang    Wähle DVD Audio/Untertitel Sprache (2-Zeichen-Ländercode)\n"
#endif
" -ss <zeitpos>    Spiele ab Position (Sekunden oder hh:mm:ss)\n"
" -nosound         Ohne Ton abspielen\n"
" -fs              Im Vollbildmodus abspielen (oder -vm, -zoom, siehe Manpage)\n"
" -x <x> -y <y>    Setze Bildschirmauflösung (für Benutzung mit -vm oder -zoom)\n"
" -sub <datei>     Benutze Untertitel-Datei (siehe auch -subfps, -subdelay)\n"
" -playlist <datei> Benutze Playlist aus Datei\n"
" -vid x -aid y    Wähle Video- (x) und Audiostream (y) zum Abspielen\n"
" -fps x -srate y  Ändere Videoframerate (x fps) und Audiosamplingrate (y Hz)\n"
" -pp <Qualität>   Aktiviere Postprocessing-Filter (siehe Manpage für Details)\n"
" -framedrop       Verwerfe einzelne Frames (bei langsamen Rechnern)\n"
"\n"
"Grundlegende Tasten: (vollständige Liste in der Manpage, siehe auch input.conf)\n"
" <- oder ->       Springe 10 Sekunden vor/zurück\n"
" hoch/runter      Springe  1 Minute vor/zurück\n"
" Bild hoch/runter Springe 10 Minuten vor/zurück\n"
" < oder >         Gehe in der Playlist vor/zurück\n"
" p oder LEER      Pause (drücke eine beliebige Taste zum Fortsetzen)\n"
" q oder ESC       Abspielen stoppen und Programm beenden\n"
" + oder -         Audioverzögerung um +/- 0.1 Sekunde anpassen\n"
" o                OSD-Modus:  Aus / Suchleiste / Suchleiste + Zeitangabe\n"
" * oder /         PCM-Lautstärke erhöhen oder erniedrigen\n"
" z oder x         Untertitelverzögerung um +/- 0.1 Sekunde anpassen\n"
" r oder t         Verschiebe die Untertitel-Position, siehe auch '-vf expand'\n"
"\n"
" * * * SIEHE MANPAGE FÜR DETAILS, WEITERE OPTIONEN UND TASTEN * * *\n"
"\n";
#endif

#define MSGTR_SamplesWanted "Beispiele für dieses Format werden gebraucht, um die Unterstützung zu verbessern. Bitte kontaktiere die Entwickler.\n"

// ========================= MPlayer Ausgaben ===========================

// mplayer.c: 
#define MSGTR_Exiting "\nBeenden...\n"
#define MSGTR_ExitingHow "\nBeenden... (%s)\n"
#define MSGTR_Exit_quit "Ende"
#define MSGTR_Exit_eof "Dateiende erreicht."
#define MSGTR_Exit_error "Fataler Fehler"
#define MSGTR_IntBySignal "\nMPlayer wurde durch Signal %d im Modul %s unterbrochen\n"
#define MSGTR_NoHomeDir "Kann Homeverzeichnis nicht finden.\n"
#define MSGTR_GetpathProblem "get_path(\"config\")-Problem\n"
#define MSGTR_CreatingCfgFile "Erstelle Konfigurationsdatei: %s\n"
#define MSGTR_InvalidAOdriver "Ungültiger Audioausgabetreibername: %s\nBenutze '-vo help' für eine Liste der verfügbaren Audiotreiber.\n"
#define MSGTR_CopyCodecsConf "(Kopiere/verlinke etc/codecs.conf aus dem MPlayer-Quelltext nach ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Benutze eingebaute Standardwerte für codecs.conf.\n"
#define MSGTR_CantLoadFont "Kann Schriftdatei nicht laden: %s\n"
#define MSGTR_CantLoadSub "Kann Untertitel nicht laden: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: Ausgewählter Stream fehlt!\n"
#define MSGTR_CantOpenDumpfile "Kann dump-Datei nicht öffnen!\n"
#define MSGTR_CoreDumped "Core dumped ;)\n"
#define MSGTR_FPSnotspecified "FPS ist im Header nicht angegeben (oder ungültig)! Benutze die -fps Option!\n"
#define MSGTR_TryForceAudioFmtStr "Versuche Audiocodecfamilie %s zu erzwingen...\n"
#define MSGTR_CantFindAudioCodec "Kann Codec für Audioformat 0x%X nicht finden!\n"
#define MSGTR_RTFMCodecs "Lies DOCS/de/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Versuche Videocodecfamilie %s zu erzwingen...\n"
#define MSGTR_CantFindVideoCodec "Kann keinen Codec finden, der  zur gewählten -vo-Option und Videoformat 0x%X passt!\n"
#define MSGTR_CannotInitVO "FATAL: Kann Videoausgabetreiber nicht initialisieren!\n"
#define MSGTR_CannotInitAO "Kann Audiotreiber/Soundkarte nicht öffnen/initialisieren -> kein Ton\n"
#define MSGTR_StartPlaying "Starte Wiedergabe...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***************************************************\n"\
"         **** Dein System ist zu LANGSAM zum Abspielen! ****\n"\
"         ***************************************************\n"\
"Mögliche Gründe, Probleme, Workarounds: \n"\
"- Häufigste Ursache: defekter/fehlerhafter _Audio_treiber.\n"\
"  - Versuche -ao sdl oder die OSS-Emulation von ALSA.\n"\
"  - Experimentiere mit verschiedenen Werten für -autosync, 30 ist ein guter Startwert."\
"- Langsame Videoausgabe\n"\
"  - Versuche einen anderen -vo Treiber (-vo help für eine Liste)\n"\
"    oder probiere -framedrop!\n"\
"- Langsame CPU\n"\
"  - Versuche nicht, DVDs/große DivX-Filme auf langsamen CPUs abzuspielen.\n"\
"    Probiere -hardframedrop.\n"\
"- Defekte Datei\n"\
"  - Versuche verschiedene Kombinationen von: -nobps -ni -forceidx -mc 0.\n"\
"- Für die Wiedergabe von langsamen Medien (NFS/SMB, DVD, VCD usw)\n"\
"  - Versuche -cache 8192.\n"\
"- Benutzt du -cache zusammen mit einer nicht-interleavten AVI-Datei?\n"\
"  - Versuche -nocache.\n"\
"Lies DOCS/de/video.html und DOCS/de/sound.html; dort stehen \n"\
"Tipps und Kniffe für optimale Einstellungen. \n"\
"(Schau evtl. auch bei den entsprechenden englischen Seiten.)\n"\
"Wenn dies nicht hilft, lies DOCS/de/bugreports.html!\n\n"

#define MSGTR_NoGui "MPlayer wurde OHNE GUI-Unterstützung kompiliert.\n"
#define MSGTR_GuiNeedsX "MPlayer GUI erfordert X11.\n"
#define MSGTR_Playing "Spiele %s\n"
#define MSGTR_NoSound "Audio: kein Ton!\n"
#define MSGTR_FPSforced "FPS von %5.3f erzwungen (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "MPlayer mit CPU-Erkennung zur Laufzeit kompiliert - WARNUNG, das ist nicht optimal!\nKompiliere MPlayer mit --disable-runtime-cpudetection für beste Performance.\n"
#define MSGTR_CompiledWithCPUExtensions "Kompiliert für x86 CPU mit folgenden Erweiterungen:"
#define MSGTR_AvailableVideoOutputDrivers "Verfügbare Videoausgabetreiber:\n"
#define MSGTR_AvailableAudioOutputDrivers "Verfügbare Audioausgabetreiber:\n"
#define MSGTR_AvailableAudioCodecs "Verfügbare Audiocodecs:\n"
#define MSGTR_AvailableVideoCodecs "Verfügbare Videocodecs:\n"
#define MSGTR_AvailableAudioFm "Verfügbare (in das Binary kompilierte) Audio Codec Familien:\n"
#define MSGTR_AvailableVideoFm "Verfügbare (in das Binary kompilierte) Video Codec Familien:\n"
#define MSGTR_AvailableFsType "Verfügbare Vollbildschirm-Modi:\n"
#define MSGTR_UsingRTCTiming "Verwende Linux Hardware RTC-Timing (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Video: Kann Eigenschaften nicht lesen.\n"
#define MSGTR_NoStreamFound "Keine Streams gefunden.\n"
#define MSGTR_ErrorInitializingVODevice "Fehler beim Öffnen/Initialisieren des ausgewählten Videoausgabetreibers (-vo).\n"
#define MSGTR_ForcedVideoCodec "Erzwungener Videocodec: %s\n"
#define MSGTR_ForcedAudioCodec "Erzwungener Audiocodec: %s\n"
#define MSGTR_Video_NoVideo "Video: kein Video\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Konnte Videofilter (-vf) oder -ausgabetreiber (-vo) nicht initialisieren.\n"
#define MSGTR_Paused "\n  =====  PAUSE  =====\r"
#define MSGTR_PlaylistLoadUnable "\nKann Playlist %s nicht laden.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer stürzte aufgrund einer 'ungültigen Anweisung' ab.\n"\
"  Es kann sich um einen Fehler im unserem neuen Code für\n"\
"  die CPU-Erkennung zur Laufzeit handeln...\n"\
"  Bitte lies DOCS/de/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer stürzte aufgrund einer 'ungültigen Anweisung' ab.\n"\
"  Das passiert normalerweise, wenn du MPlayer auf einer anderen CPU\n"\
"  ausführst als auf der, für die er kompiliert/optimiert wurde.\n"\
"  Überprüfe das!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer stürzte wegen falscher Benutzung der CPU/FPU/des RAMs ab.\n"\
"  Kompiliere MPlayer erneut mit --enable-debug und erstelle mit 'gdb'\n"\
"  einen Backtrace und eine Disassemblierung. Details dazu findest du\n"\
"  in DOCS/de/bugreports.html.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer ist abgestürzt. Das sollte nicht passieren.\n"\
"  Es kann sich um einen Fehler im MPlayer-Code _oder_ in deinen Treibern\n"\
"  _oder_ in deinem gcc handeln. Wenn du meinst, es sei MPlayers Fehler, dann\n"\
"  lies DOCS/de/bugreports.html und folge den dortigen Anweisungen.\n"\
"  Wir können und werden dir nicht helfen, wenn du nicht alle dort aufgeführten\n"\
"  Informationen zur Verfügung stellst.\n"
#define MSGTR_LoadingConfig "Lade Konfiguration '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: Untertiteldatei (%d) hinzugefügt: %s\n"
#define MSGTR_ErrorOpeningOutputFile "Fehler beim Öffnen von Datei [%s] zum Schreiben!\n"
#define MSGTR_CommandLine "Kommandozeile:"
#define MSGTR_RTCDeviceNotOpenable "Konnte %s nicht öffnen: %s (sollte für den Benutzer lesbar sein.)\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Linux RTC-Initialisierungsfehler in ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Versuche, \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" zu deinen Systemstartskripten hinzuzufügen.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Linux RTC-Initialisierungsfehler in ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Benutze %s-Zeitgeber.\n"
#define MSGTR_MenuInitialized "Menü initialisiert: %s\n"
#define MSGTR_MenuInitFailed "Initialisierung des Menüs fehlgeschlagen.\n"
#define MSGTR_Getch2InitializedTwice "WARNUNG: getch2_init doppelt aufgerufen!\n"
#define MSGTR_DumpstreamFdUnavailable "Kann Dump dieses Streams nicht anlegen - kein 'fd' verfügbar.\n"
#define MSGTR_FallingBackOnPlaylist "Falle zurück auf den Versuch, Playlist %s einzulesen...\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Kann den libmenu-Videofilter nicht mit dem Ursprungsmenü %s öffnen.\n"
#define MSGTR_AudioFilterChainPreinitError "Fehler bei der Vorinitialisierung der Audiofilterkette!\n"
#define MSGTR_LinuxRTCReadError "Linux RTC-Lesefehler: %s\n"
#define MSGTR_SoftsleepUnderflow "Warnung! Unterlauf des Softsleep!\n"
#define MSGTR_AnsSubVisibility "ANS_SUB_VISIBILITY=%ld\n"
#define MSGTR_AnsLength "ANS_LENGTH=%ld\n"
#define MSGTR_AnsVoFullscreen "ANS_VO_FULLSCREEN=%ld\n"
#define MSGTR_AnsPercentPos "ANS_PERCENT_POSITION=%ld\n"
#define MSGTR_DvdnavNullEvent "DVDNAV-Ereignis NULL?!\n"
#define MSGTR_DvdnavHighlightEventBroken "DVDNAV-Ereignis: Hervorhebungs-Ereignis kaputt\n"
#define MSGTR_DvdnavEvent "DVDNAV-Ereignis: %s\n"
#define MSGTR_DvdnavHighlightHide "DVDNAV-Ereignis: Hervorhebung verbergen\n"
#define MSGTR_DvdnavStillFrame "######################################## DVDNAV-Ereignis: Standbild: %d Sekunde(n)\n"
#define MSGTR_DvdnavNavStop "DVDNAV-Ereignis: Nav Stop\n"
#define MSGTR_DvdnavNavNOP "DVDNAV-Ereignis: Nav NOP\n"
#define MSGTR_DvdnavNavSpuStreamChangeVerbose "DVDNAV-Ereignis: Nav SPU Stream Change: phys: %d/%d/%d logisch: %d\n"
#define MSGTR_DvdnavNavSpuStreamChange "DVDNAV-Ereignis: Nav SPU Stream-Änderung: phys: %d logisch: %d\n"
#define MSGTR_DvdnavNavAudioStreamChange "DVDNAV-Ereignis: Nav Audio Stream-Änderung: phys: %d logisch: %d\n"
#define MSGTR_DvdnavNavVTSChange "DVDNAV-Ereignis: Nav VTS-Änderung\n"
#define MSGTR_DvdnavNavCellChange "DVDNAV-Ereignis: Nav Cell-Änderung\n"
#define MSGTR_DvdnavNavSpuClutChange "DVDNAV-Ereignis: Nav SPU CLUT-Änderung\n"
#define MSGTR_DvdnavNavSeekDone "DVDNAV-Ereignis: Nav Suche beendet.\n"
#define MSGTR_MenuCall "Menü-Aufruf\n"

#define MSGTR_EdlCantUseBothModes "Kann -edl und -edlout nicht zur selben Zeit benutzen.\n"
#define MSGTR_EdlOutOfMem "Kann nicht genug Speicher für EDL-Daten reservieren.\n"
#define MSGTR_EdlRecordsNo "%d EDL-Aktionen gelesen.\n"
#define MSGTR_EdlQueueEmpty "Es gibt keine auszuführenden EDL-Aktionen.\n"
#define MSGTR_EdlCantOpenForWrite "Kann EDL-Datei [%s] nicht zum Schreiben öffnen.\n"
#define MSGTR_EdlCantOpenForRead "Kann EDL-Datei [%s] nicht zum Lesen öffnen.\n"
#define MSGTR_EdlNOsh_video "Kann EDL nicht ohne Video verwenden, deaktiviere.\n"
#define MSGTR_EdlNOValidLine "Ungültige EDL-Zeile: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Schlecht formatierte EDL-Zeile [%d]. Verwerfe.\n"
#define MSGTR_EdlBadLineOverlap "Letzte Stop-Position war [%f]; nächster Start ist "\
"[%f]. Einträge müssen in chronologischer Reihenfolge sein, ohne Überschneidung. Verwerfe.\n"
#define MSGTR_EdlBadLineBadStop "Zeit des Stops muß nach der Startzeit sein.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControllFile "Verwende Pass 3 Kontrolldatei: %s\n"
#define MSGTR_MissingFilename "\nDateiname nicht angegeben.\n\n"
#define MSGTR_CannotOpenFile_Device "Kann Datei/Gerät nicht öffnen\n"
#define MSGTR_CannotOpenDemuxer "Kann Demuxer nicht öffnen.\n"
#define MSGTR_NoAudioEncoderSelected "\nKein Audioencoder (-oac)  ausgewählt. \nWähle einen aus (siehe -oac help) oder verwende  -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nKein Videoencoder (-ovc) ausgewählt. \nWähle einen aus (siehe -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Kann Ausgabedatei '%s' nicht öffnen.\n"
#define MSGTR_EncoderOpenFailed "Öffnen des Encoders fehlgeschlagen.\n"
#define MSGTR_ForcingOutputFourcc "Erzwinge Output-Fourcc %x  [%.4s].\n"
#define MSGTR_WritingAVIHeader "Schreibe AVI-Header...\n"
#define MSGTR_DuplicateFrames "\n%d doppelte(r) Frame(s)!\n"
#define MSGTR_SkipFrame "\nFrame übersprungen!\n"
#define MSGTR_ErrorWritingFile "%s: Fehler beim Schreiben der Datei.\n"
#define MSGTR_WritingAVIIndex "\nSchreibe AVI-Index...\n"
#define MSGTR_FixupAVIHeader "Korrigiere AVI-Header...\n"
#define MSGTR_RecommendedVideoBitrate "Empfohlene Videobitrate für %s CD(s): %d\n"
#define MSGTR_VideoStreamResult "\nVideostream: %8.3f kbit/s  (%d bps)  Größe: %d Bytes  %5.3f Sek.  %d Frames\n"
#define MSGTR_AudioStreamResult "\nAudiostream: %8.3f kbit/s  (%d bps)  Größe: %d Bytes  %5.3f Sek.\n"
#define MSGTR_OpenedStream "Erfolg: Format: %d  Daten: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "Videocodec: Framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "Audiocodec: Framecopy (Format=%x chans=%d Rate=%ld Bits=%d bps=%ld Sample-%ld)\n"
#define MSGTR_CBRPCMAudioSelected "CBR PCM Audio ausgewählt\n"
#define MSGTR_MP3AudioSelected "MP3 Audio ausgewählt\n"
#define MSGTR_CannotAllocateBytes "Konnte %d Bytes nicht reservieren\n"
#define MSGTR_SettingAudioDelay "Setze AUDIOVERZÖGERUNG auf %5.3f\n"
#define MSGTR_SettingAudioInputGain "Setze Audioeingangsverstärkung auf %f\n"
#define MSGTR_LamePresetEquals "\nPreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Limitiere Audio-Preload auf 0.4s\n"
#define MSGTR_IncreasingAudioDensity "Erhöhe Audiodichte auf 4\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Erzwinge Audio-Preload von 0, maximale pts-Korrektur von 0\n"
#define MSGTR_CBRAudioByterate "\n\nCBR Audio: %ld Bytes/Sek, %d Bytes/Block\n"
#define MSGTR_LameVersion "LAME-Version %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Fehler: Die angegebene Bitrate ist außerhalb des gültigen Bereichs\nfür dieses Preset.\n"\
"\n"\
"Bei Benutzung dieses Modus mußt du einen Wert zwischen \"8\" und \"320\" angeben.\n"\
"\n"\
"Für weitere Informationen hierzu versuche: \"-lameopts preset=help\"\n"
#define MSGTR_InvalidLamePresetOptions "Fehler: Du hast kein gültiges Profil und/oder ungültige Optionen mit\n        dem Preset angegeben.\n"\
"\n"\
"Verfügbare Profile sind folgende:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR-Modus) - Der ABR-Modus ist impliziert. Um ihn zu benutzen,\n"\
"                 gib einfach die Bitrate an. Zum Beispiel:\n"\
"                 \"preset=185\" aktiviert dieses Preset\n"\
"                 und benutzt 185 als durchschnittliche kbps.\n"\
"\n"\
"    Ein paar Beispiele:\n"\
"\n"\
"      \"-lameopts fast:preset=standard  \"\n"\
" oder \"-lameopts  cbr:preset=192       \"\n"\
" oder \"-lameopts      preset=172       \"\n"\
" oder \"-lameopts      preset=extreme   \"\n"\
"\n"\
"Für weitere Informationen hierzu versuche: \"-lameopts preset=help\"\n"
#define MSGTR_LamePresetsLongInfo "\n"\
"Die Preset-Schalter sind angelegt, die höchstmögliche Qualität zur Verfügung\nzu stellen.\n"\
"\n"\
"Sie waren Thema von großangelegten Doppelblind-Hörtests und wurden\n"\
"dementsprechend verfeinert, um diese Objektivität zu erreichen.\n"\
"\n"\
"Diese werden kontinuierlich aktualisiert, um den neuesten Entwicklungen zu\n"\
"entsprechen, die stattfinden. Daher sollte dir das Resultat die fast beste\n"\
"Qualität liefern, die zur Zeit mit LAME möglich ist.\n"\
"\n"\
"Um diese Presets zu aktivieren:\n"\
"\n"\
"   Für VBR-Modi (generell höchste Qualität):\n"\
"\n"\
"     \"preset=standard\" Dieses Preset sollte generell anwendbar sein für\n"\
"                            die meisten Leute und die meiste Musik und hat\n"\
"                            schon eine recht hohe Qualität.\n"\
"\n"\
"     \"preset=extreme\" Wenn du einen extrem guten Hörsinn und ähnlich gute\n"\
"                            Ausstattung hast, wird dir dieses Preset generell\n"\
"                            eine leicht höhere Qualität bieten als der\n"\
"                            \"standard\"-Modus.\n"\
"\n"\
"   Für CBR 320kbps (höchstmögliche Qualität mit diesen Preset-Schaltern):\n"\
"\n"\
"     \"preset=insane\"  Dieses Preset ist vermutlich Overkill für die meisten\n"\
"                            Leute und die meisten Situationen, aber wenn Du\n"\
"                            die absolut höchste Qualität brauchst und die\n"\
"                            Dateigröße egal ist, ist dies der Weg.\n"\
"\n"\
"   Für ABR-Modi (hohe Qualität für gegebenene Bitrate, geringer als bei VBR):\n"\
"\n"\
"     \"preset=<kbps>\"  Benutzung dieses Presets wird dir normalerweise gute\n"\
"                            Qualität zu einer angegebenen Bitrate liefern.\n"\
"                            Je nach Bitrate wird dieses Preset optimale\n"\
"                            Einstellungen für diese bestimmte Situation\n"\
"                            ermitteln. Obwohl dieser Ansatz funktioniert,\n"\
"                            ist er nicht ansatzweise so flexibel wie VBR\n"\
"                            und wird für gewöhnlich nicht das gleiche\n"\
"                            Qualitätslevel wie VBR bei hohen Bitraten\n"\
"                            erreichen.\n"\
"\n"\
"Die folgenden Optionen sind auch bei den zugehörigen Profilen verfügbar:\n"\
"\n"\
"   <fast>        standard\n"\
"   <fast>        extreme\n"\
"                 insane\n"\
"   <cbr> (ABR-Modus) - Der ABR-Modus ist impliziert. Um ihn zu benutzen,\n"\
"                 gib einfach die Bitrate an. Zum Beispiel:\n"\
"                 \"preset=185\" aktiviert dieses Preset\n"\
"                 und benutzt 185 als durchschnittliche kbps.\n"\
"\n"\
"   \"fast\" - Aktiviert die neue schnelle VBR für ein bestimmtes Profil. Der\n"\
"            Nachteil des fast-Schalters ist, daß die Bitrate oft leicht höher\n"\
"            als im normalen Modus ist, außerdem kann die Qualität auch leicht\n"\
"            geringer ausfallen.\n"\
"   Warnung: In der aktuellen Version können fast-Presets im Vergleich zu\n"\
"            regulären Presets in zu hoher Bitrate resultieren.\n"\
"\n"\
"   \"cbr\"  - Bei Benutzung des ABR-Modus (siehe oben) und signifikanter\n"\
"            Bitrate wie 80, 96, 112, 128, 160, 192, 224, 256, 320\n"\
"            kannst du die \"cbr\"-Option benutzen, um Encoding im CBR-Modus"\
"            anstelle des Standard-ABR-Modus zu erzwingen. ABR bietet höhere\n"\
"            Qualität, doch CBR kann nützlich sein in Situationen, bei denen\n"\
"            MP3-Streaming über das Internet wichtig sind.\n"\
"\n"\
"    Zum Beispiel:\n"\
"\n"\
"      \"-lameopts fast:preset=standard  \"\n"\
" oder \"-lameopts  cbr:preset=192       \"\n"\
" oder \"-lameopts      preset=172       \"\n"\
" oder \"-lameopts      preset=extreme   \"\n"\
"\n"\
"\n"\
"Ein paar Pseudonyme sind für den ABR-Modus verfügbar:\n"\
"phone => 16kbps/Mono        phon+/lw/mw-eu/sw => 24kbps/Mono\n"\
"mw-us => 40kbps/Mono        voice => 56kbps/Mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_ConfigfileError "Konfigurationsdatei-Fehler"
#define MSGTR_ErrorParsingCommandLine "Fehler beim Parsen der Kommandozeile."
#define MSGTR_VideoStreamRequired "Videostream zwingend notwendig!\n"
#define MSGTR_ForcingInputFPS "Eingabe-fps werden interpretiert als %5.2f anstelle von\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Ausgabedateiformat RAWVIDEO unterstützt kein Audio - Audio wird deaktiviert.\n"
#define MSGTR_DemuxerDoesntSupportNosound "Dieser Demuxer unterstützt -nosound no nicht.\n"
#define MSGTR_MemAllocFailed "Speicherreservierung fehlgeschlagen"
#define MSGTR_NoMatchingFilter "Konnte passenden Filter/passendes ao-Format nicht finden!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, vielleicht fehlerhafter C-Compiler?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, Fehlender Codec-Name!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, konnte Encoder für Codec %s nicht finden.\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, konnte Kontext nicht zuordnen!\n"
#define MSGTR_CouldntOpenCodec "Konnte Codec %s nicht öffnen, br=%d\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     Modus für variable Bitrate\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh (Standard)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           durchschnittliche Bitrate\n"\
"\n"\
" cbr           konstante Bitrate\n"\
"               Erzwingt auch den CBR-Modus bei nachfolgenden ABR-Voreinstellungen:\n"\
"\n"\
" br=<0-1024>   gibt die Bitrate in kBit an (nur bei CBR und ABR)\n"\
"\n"\
" q=<0-9>       Qualität (0-höchste, 9-niedrigste) (nur bei VBR)\n"\
"\n"\
" aq=<0-9>      Qualität des Algorithmus (0-beste/am langsamsten,\n"\
"               9-schlechteste/am schnellsten)\n"\
"\n"\
" ratio=<1-100> Kompressionsverhältnis\n"\
"\n"\
" vol=<0-10>    Setzt die Audioeingangsverstärkung\n"\
"\n"\
" mode=<0-3>    (Standard: auto)\n"\
"                0: Stereo\n"\
"                1: Joint-stereo\n"\
"                2: Dualchannel\n"\
"                3: Mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: kein Padding\n"\
"                1: alles\n"\
"                2: angepasst\n"\
"\n"\
" fast          Schaltet die schnellere Codierung bei nachfolgenden VBR-Presets\n"\
"               ein, liefert leicht schlechtere Qualität und höhere Bitraten.\n"\
"\n"\
" preset=<wert> Bietet die bestmöglichen Qualitätseinstellungen.\n"\
"                 medium: VBR-Enkodierung, gute Qualität\n"\
"                 (150-180 kbps Bitratenbereich)\n"\
"                 standard:  VBR-Enkodierung, hohe Qualität\n"\
"                 (170-210 kbps Bitratenbereich)\n"\
"                 extreme: VBR-Enkodierung, sehr hohe Qualität\n"\
"                 (200-240 kbps Bitratenbereich)\n"\
"                 insane:  CBR-Enkodierung, höchste Preset-Qualität\n"\
"                 (320 kbps Bitrate)\n"\
"                 <8-320>: ABR-Enkodierung mit der angegebenen durchschnittlichen\n"\
"                          Bitrate\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "doppeltes FourCC"
#define MSGTR_TooManyFourccs "zu viele FourCCs/Formate..."
#define MSGTR_ParseError "Fehler beim Parsen"
#define MSGTR_ParseErrorFIDNotNumber "Fehler beim Parsen (Format-ID keine Nummer?)"
#define MSGTR_ParseErrorFIDAliasNotNumber "Fehler beim Parsen (Alias der Format-ID keine Nummer?)"
#define MSGTR_DuplicateFID "doppelte Format-ID"
#define MSGTR_TooManyOut "zu viele aus..."
#define MSGTR_InvalidCodecName "\nCodecname(%s) ist ungültig!\n"
#define MSGTR_CodecLacksFourcc "\nCodec(%s) hat kein FourCC/Format!\n"
#define MSGTR_CodecLacksDriver "\nCodec(%s) hat keinen Treiber!\n"
#define MSGTR_CodecNeedsDLL "\nCodec(%s) braucht eine 'dll'!\n"
#define MSGTR_CodecNeedsOutfmt "\nCodec(%s) braucht ein 'outfmt'!\n"
#define MSGTR_CantAllocateComment "Kann Speicher für Kommentar nicht allozieren. "
#define MSGTR_GetTokenMaxNotLessThanMAX_NR_TOKEN "get_token(): max >= MAX_MR_TOKEN!"
#define MSGTR_ReadingFile "Lese %s: "
#define MSGTR_CantOpenFileError "Kann '%s' nicht öffnen: %s\n"
#define MSGTR_CantGetMemoryForLine "Bekomme keinen Speicher für 'line': %s\n"
#define MSGTR_CantReallocCodecsp "Kann '*codecsp' nicht erneut allozieren: %s\n"
#define MSGTR_CodecNameNotUnique "Codecname '%s' ist nicht eindeutig."
#define MSGTR_CantStrdupName "Kann strdup nicht ausführen -> 'name': %s\n"
#define MSGTR_CantStrdupInfo "Kann strdup nicht ausführen -> 'info': %s\n"
#define MSGTR_CantStrdupDriver "Kann strdup nicht ausführen -> 'driver': %s\n"
#define MSGTR_CantStrdupDLL "Kann strdup nicht ausführen -> 'dll': %s"
#define MSGTR_AudioVideoCodecTotals "%d Audio- & %d Videocodecs\n"
#define MSGTR_CodecDefinitionIncorrect "Codec ist nicht korrekt definiert."
#define MSGTR_OutdatedCodecsConf "Diese codecs.conf ist zu alt und nicht kompatibel mit dieser Version von MPlayer!"

// divx4_vbr.c:
#define MSGTR_OutOfMemory "Kein Speicher mehr verfügbar!"
#define MSGTR_OverridingTooLowBitrate "Angegebene Bitrate ist zu niedrig für diesen Clip.\n"\
"Minimal mögliche Bitrate für den Clip ist %.0f kbps. Hebe\n"\
"den vom Benutzer angegebenen Wert auf.\n"

// fifo.c
#define MSGTR_CannotMakePipe "Kann PIPE nicht anlegen!\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Zu alte Speicherstelle gefunden von lvl %d: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "Die Option %s kann in Konfigurationsdateien nicht verwendet werden.\n"
#define MSGTR_InvalidCmdlineOption "Die Option %s kann auf der Kommandozeile nicht verwendet werden.\n"
#define MSGTR_InvalidSuboption "Fehler: Option '%s' hat keine Unteroption '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Fehler: Unteroption '%s' von '%s' benötigt einen Parameter!\n"
#define MSGTR_MissingOptionParameter "Fehler: Option '%s' benötigt einen Parameter!\n"
#define MSGTR_OptionListHeader "\n Name                 Typ             Min        Max      Global  CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nInsgesamt: %d Optionen\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM-Laufwerk '%s' nicht gefunden.\n"
#define MSGTR_ErrTrackSelect "Fehler beim Auswählen des VCD Tracks.\n"
#define MSGTR_ReadSTDIN "Lese von Standardeingabe (stdin)...\n"
#define MSGTR_UnableOpenURL "Kann URL nicht öffnen: %s\n"
#define MSGTR_ConnToServer "Verbunden mit Server: %s\n"
#define MSGTR_FileNotFound "Datei nicht gefunden: '%s'\n"

#define MSGTR_SMBInitError "Kann die Bibliothek libsmbclient nicht öffnen: %d\n"
#define MSGTR_SMBFileNotFound "Konnte '%s' nicht über das Netzwerk öffnen\n"
#define MSGTR_SMBNotCompiled "MPlayer wurde ohne  SMB-Unterstützung  kompiliert.\n"

#define MSGTR_CantOpenDVD "Kann DVD-Laufwerk nicht öffnen: %s\n"
#define MSGTR_DVDwait "Lese Disk-Struktur, bitte warten...\n"
#define MSGTR_DVDnumTitles "Es sind %d Titel auf dieser DVD.\n"
#define MSGTR_DVDinvalidTitle "Ungültige DVD-Titelnummer: %d\n"
#define MSGTR_DVDnumChapters "Es sind %d Kapitel in diesem DVD-Titel.\n"
#define MSGTR_DVDinvalidChapter "Ungültige DVD-Kapitelnummer: %d\n"
#define MSGTR_DVDnumAngles "Es sind %d Kameraeinstellungen diesem DVD-Titel.\n"
#define MSGTR_DVDinvalidAngle "Ungültige DVD-Kameraeinstellungsnummer %d.\n"
#define MSGTR_DVDnoIFO "Kann die IFO-Datei für den DVD-Titel %d nicht öffnen.\n"
#define MSGTR_DVDnoVOBs "Kann VOB-Dateien des Titels  (VTS_%02d_1.VOB) nicht öffnen.\n"
#define MSGTR_DVDopenOk "DVD erfolgreich geöffnet.\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Warnung! Audiostream-Header %d neu definiert!\n"
#define MSGTR_VideoStreamRedefined "Warnung! Videostream-Header %d neu definiert!\n"
#define MSGTR_TooManyAudioInBuffer "\nZu viele Audiopakete im Puffer: (%d in %d bytes).\n"
#define MSGTR_TooManyVideoInBuffer "\nZu viele Videopakete im Puffer: (%d in %d bytes).\n"
#define MSGTR_MaybeNI "Vielleicht spielst du eine(n) nicht-interleaved Stream/Datei, oder der Codec funktioniert nicht.\n" \
                      "Versuche bei AVI-Dateien, den nicht-interleaved Modus mit der Option -ni zu erzwingen\n"
#define MSGTR_SwitchToNi "\nSchlecht interleavte AVI-Datei erkannt, wechsele in den -ni Modus!\n"
#define MSGTR_Detected_XXX_FileFormat "%s-Dateiformat erkannt!\n"
#define MSGTR_DetectedAudiofile "Audiodatei erkannt!\n"
#define MSGTR_NotSystemStream "Kein MPEG System Stream... (vielleicht ein Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Ungültiger MPEG-ES Stream??? Kontaktiere den Autor, das könnte ein Bug sein :(\n"
#define MSGTR_FormatNotRecognized "========= Sorry, dieses Dateiformat wird nicht erkannt/unterstützt ============\n"\
				  "============== Sollte dies ein AVI, ASF oder MPEG Stream sein, ===============\n"\
				  "================== dann kontaktiere bitte den Autor. ========================\n"
#define MSGTR_MissingVideoStream "Kein Videostream gefunden.\n"
#define MSGTR_MissingAudioStream "Kein Audiostream gefunden. -> kein Ton.\n"
#define MSGTR_MissingVideoStreamBug "Fehlender Videostream!? Kontaktiere den Autor, dies könnte ein Bug sein :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Datei enthält den gewählten Audio- oder Videostream nicht.\n"

#define MSGTR_NI_Forced "Erzwungen"
#define MSGTR_NI_Detected "Erkannt"
#define MSGTR_NI_Message "%s NICHT-INTERLEAVED AVI-Dateiformat.\n"

#define MSGTR_UsingNINI "Verwende defektes NICHT-INTERLEAVED AVI Dateiformat.\n"
#define MSGTR_CouldntDetFNo "Konnte die Anzahl der Frames (für absolute Suche) nicht feststellen.\n"
#define MSGTR_CantSeekRawAVI "Suche in reinen AVI-Streams nicht durchführbar. (Index erforderlich, probiere die '-idx'-Option.)\n"
#define MSGTR_CantSeekFile "Kann diese Datei nicht durchsuchen.\n"

#define MSGTR_EncryptedVOB "Verschlüsselte VOB-Datei! Lies DOCS/de/cd-dvd.html\n"

#define MSGTR_MOVcomprhdr "MOV: komprimierte Header benötigen ZLIB-Unterstützung.\n"
#define MSGTR_MOVvariableFourCC "MOV: Warnung: Variable FOURCC erkannt!?\n"
#define MSGTR_MOVtooManyTrk "MOV: WARNUNG: Zu viele Tracks."
#define MSGTR_FoundAudioStream "==> Audiostream gefunden: %d\n"
#define MSGTR_FoundVideoStream "==> Videostream gefunden: %d\n"
#define MSGTR_DetectedTV "TV erkannt! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Öffnen des OGG-Demuxers fehlgeschlagen.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Suche nach Audiostream (Id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Kann Audiostream nicht öffnen: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Kann Untertitelstream nicht öffnen: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Öffnen des Audio-Demuxers fehlgeschlagen: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Öffnen des Untertitel-Demuxers fehlgeschlagen: %s\n"
#define MSGTR_TVInputNotSeekable "TV-Input ist nicht durchsuchbar. (Suche des Kanals?)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Demuxerinfo %s schon vorhanden.\n"
#define MSGTR_ClipInfo "Clip-Info: \n"

#define MSGTR_LeaveTelecineMode "demux_mpg: 30fps NTSC-Inhalt erkannt, wechsele Framerate.\n"
#define MSGTR_EnterTelecineMode "demux_mpg: 24fps progressiven NTSC-Inhalt erkannt, wechsele Framerate.\n"
				  
// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Konnte Codec nicht öffnen.\n"
#define MSGTR_CantCloseCodec "Konnte Codec nicht schließen.\n"

#define MSGTR_MissingDLLcodec "FEHLER: Kann erforderlichen DirectShow-Codec nicht öffnen: %s\n"
#define MSGTR_ACMiniterror "Kann Win32/ACM-Audiocodec nicht laden/initialisieren (fehlende DLL-Datei?).\n"
#define MSGTR_MissingLAVCcodec "Kann Codec '%s' von libavcodec nicht finden...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF (Ende der Datei) während der Suche nach Sequenzheader.\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Kann Sequenzheader nicht lesen.\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Kann Sequenzheader-Erweiterung nicht lesen.\n"
#define MSGTR_BadMpegSequHdr "MPEG: Schlechter Sequenzheader.\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Schlechte Sequenzheader-Erweiterung.\n"

#define MSGTR_ShMemAllocFail "Kann keinen gemeinsamen Speicher reservieren.\n"
#define MSGTR_CantAllocAudioBuf "Kann keinen Audioausgabe-Puffer reservieren.\n"

#define MSGTR_UnknownAudio "Unbekanntes/fehlendes Audioformat -> kein Ton\n"

#define MSGTR_UsingExternalPP "[PP] Verwende externe Postprocessing-Filter, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Verwende Postprocessing-Routinen des Codecs, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Videoeigenschaft '%s' wird von ausgewählten vo & vd nicht unterstützt.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Erforderliche Videocodec Familie [%s] (vfm=%s) nicht verfügbar.\nAktiviere sie beim Kompilieren.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Erforderliche Audiocodec-Familie [%s] (afm=%s) nicht verfügbar.\nAktiviere sie beim Kompilieren.\n"
#define MSGTR_OpeningVideoDecoder "Öffne Videodecoder: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Öffne Audiodecoder: [%s] %s\n"
#define MSGTR_UninitVideoStr "Deinitialisiere Video: %s  \n"
#define MSGTR_UninitAudioStr "Deinitialisiere Audio: %s  \n"
#define MSGTR_VDecoderInitFailed "Initialisierung des Videodecoders fehlgeschlagen :(\n"
#define MSGTR_ADecoderInitFailed "Initialisierung des Audiodecoders fehlgeschlagen :(\n"
#define MSGTR_ADecoderPreinitFailed "Vorinitialisierung des Audiodecoders fehlgeschlagen :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Reserviere %d Bytes für den Eingangspuffer.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Reserviere %d + %d = %d Bytes für den Ausgabepuffer.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Initialisiere LIRC-Unterstützung...\n"
#define MSGTR_LIRCdisabled "Verwendung der Fernbedienung nicht möglich.\n"
#define MSGTR_LIRCopenfailed "Fehler beim Öffnen der LIRC-Unterstützung.\n"
#define MSGTR_LIRCcfgerr "Kann LIRC-Konfigurationsdatei %s nicht lesen.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Konnte Videofilter '%s' nicht finden.\n"
#define MSGTR_CouldNotOpenVideoFilter "Konnte Videofilter '%s' nicht öffnen.\n"
#define MSGTR_OpeningVideoFilter "Öffne Videofilter: "
#define MSGTR_CannotFindColorspace "Konnte keinen passenden Farbraum finden, auch nicht mit '-vf scale' :-(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Codec hat sh->disp_w und sh->disp_h nicht gesetzt!\nVersuche Problem zu umgehen..\n"
#define MSGTR_VoConfigRequest "VDec: VO wird versucht, auf %d x %d (Bevorzugter Farbraum: %s) zu setzen.\n"
#define MSGTR_CouldNotFindColorspace "Konnte keinen passenden Farbraum finden - neuer Versuch mit '-vf scale'..\n"
#define MSGTR_MovieAspectIsSet "Film-Aspekt ist %.2f:1 - Vorskalierung zur Korrektur der Seitenverhältnisse.\n"
#define MSGTR_MovieAspectUndefined "Film-Aspekt ist undefiniert - keine Vorskalierung durchgeführt.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Du mußt das Binärcodec-Paket aktualisieren/installieren.\nGehe dazu auf http://mplayerhq.hu/homepage/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Win32/DShow Videocodec-Initialisierung OK.\n"
#define MSGTR_DMOInitOK "INFO: Win32/DMO Videocodec-Initialisierung OK.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Konnte EWMH-Fullscreen-Event nicht senden!\n"
#define MSGTR_InsertingAfVolume "[Mixer] Kein Hardware-Mixing, füge Lautstärkefilter ein.\n"
#define MSGTR_NoVolume "[Mixer] Keine Lautstärkeregelung verfügbar.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Über..."
#define MSGTR_FileSelect "Wähle Datei..."
#define MSGTR_SubtitleSelect "Wähle Untertitel..."
#define MSGTR_OtherSelect "Wähle..."
#define MSGTR_AudioFileSelect "Wähle externen Audiokanal..."
#define MSGTR_FontSelect "Wähle Schrift..."
#define MSGTR_PlayList "Playlist"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Skin-Browser"
#define MSGTR_Network "Netzwerk-Streaming..."
#define MSGTR_Preferences "Einstellungen"
#define MSGTR_AudioPreferences "Audio-Treiberkonfiguration"
#define MSGTR_NoMediaOpened "Keine Medien geöffnet."
#define MSGTR_VCDTrack "VCD-Titel %d"
#define MSGTR_NoChapter "kein Kapitel"
#define MSGTR_Chapter "Kapitel %d"
#define MSGTR_NoFileLoaded "Keine Datei geladen."

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Abbrechen"
#define MSGTR_Add "Hinzufügen"
#define MSGTR_Remove "Entfernen"
#define MSGTR_Clear "Löschen"
#define MSGTR_Config "Konfiguration"
#define MSGTR_ConfigDriver "Konfiguriere Treiber"
#define MSGTR_Browse "Durchsuchen"

// --- error messages ---
#define MSGTR_NEMDB "Sorry, nicht genug Speicher für den Zeichnungs-Puffer."
#define MSGTR_NEMFMR "Sorry, nicht genug Speicher für Menü-Rendering."
#define MSGTR_IDFGCVD "Sorry, kann keinen GUI-kompatiblen Ausgabetreiber finden."
#define MSGTR_NEEDLAVCFAME "Sorry, du versuchst, Nicht-MPEG Dateien ohne erneute Enkodierung abzuspielen.\nBitte aktiviere lavc oder fame in der DXR3/H+-Configbox."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[Skin] Fehler in Skin-Konfigurationsdatei in Zeile %d: %s" 
#define MSGTR_SKIN_WARNING1 "[Skin] Warnung in Skin-Konfigurationsdatei in Zeile %d:\nWidget (%s) gefunden, aber davor wurde \"section\" nicht gefunden"
#define MSGTR_SKIN_WARNING2 "[Skin] Warnung in Skin-Konfigurationsdatei in Zeile %d:\nWidget (%s) gefunden, aber davor wurde \"subsection\" nicht gefunden (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] Warnung in Skin-Konfigurationsdatei in Zeile %d:\nDiese Untersektion wird vom Widget nicht unterstützt (%s).\n"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmaps mit 16 Bits oder weniger werden nicht unterstützt (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Datei nicht gefunden (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP-Lesefehler (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA-Lesefehler (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG-Lesefehler (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE-gepacktes TGA wird nicht unterstützt (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "unbekannter Dateityp (%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "Konvertierungsfehler von 24 Bit auf 32 Bit (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "unbekannte Nachricht: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nicht genug Speicher\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Zu viele Schriften deklariert.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Schriftdatei nicht gefunden.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Schriftbilddatei nicht gefunden.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "nicht existierende Schriftbezeichnung (%s)\n"
#define MSGTR_SKIN_UnknownParameter "unbekannter Parameter (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin nicht gefunden (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin-Konfigurationsdatei: Lesefehler (%s)\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Über MPlayer"
#define MSGTR_MENU_Open "Öffnen..."
#define MSGTR_MENU_PlayFile "Spiele Datei..."
#define MSGTR_MENU_PlayVCD "Spiele VCD..."
#define MSGTR_MENU_PlayDVD "Spiele DVD..."
#define MSGTR_MENU_PlayURL "Spiele URL..."
#define MSGTR_MENU_LoadSubtitle "Lade Untertitel..."
#define MSGTR_MENU_DropSubtitle "Entferne Untertitel..."
#define MSGTR_MENU_LoadExternAudioFile "Lade externe Audiodatei..."
#define MSGTR_MENU_Playing "Spiele"
#define MSGTR_MENU_Play "Abspielen"
#define MSGTR_MENU_Pause "Pause"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Nächster Stream"
#define MSGTR_MENU_PrevStream "Vorheriger Stream"
#define MSGTR_MENU_Size "Größe"
#define MSGTR_MENU_NormalSize "Normale Größe"
#define MSGTR_MENU_DoubleSize "Doppelte Größe"
#define MSGTR_MENU_FullScreen "Vollbild"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Öffne CD/DVD..."
#define MSGTR_MENU_ShowDVDMenu "Zeige DVD Menü"
#define MSGTR_MENU_Titles "Titel"
#define MSGTR_MENU_Title "Titel %2d"
#define MSGTR_MENU_None "(nichts)"
#define MSGTR_MENU_Chapters "Kapitel"
#define MSGTR_MENU_Chapter "Kapitel %2d"
#define MSGTR_MENU_AudioLanguages "Audio-Sprachen"
#define MSGTR_MENU_SubtitleLanguages "Untertitel-Sprachen"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Skinbrowser"
#define MSGTR_MENU_Preferences "Einstellungen"
#define MSGTR_MENU_Exit "Beenden..."
#define MSGTR_MENU_Mute "Stummschaltung"
#define MSGTR_MENU_Original "Original"
#define MSGTR_MENU_AspectRatio "Seitenverhältnis"
#define MSGTR_MENU_AudioTrack "Audiospur"
#define MSGTR_MENU_Track "Spur %d"
#define MSGTR_MENU_VideoTrack "Videospur"

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Helligkeit: "
#define MSGTR_EQU_Hue "Farbton: "
#define MSGTR_EQU_Saturation "Sättigung: "
#define MSGTR_EQU_Front_Left "Vorne Links"
#define MSGTR_EQU_Front_Right "Vorne Rechts"
#define MSGTR_EQU_Back_Left "Hinten Links"
#define MSGTR_EQU_Back_Right "Hinten Rechts"
#define MSGTR_EQU_Center "Mitte"
#define MSGTR_EQU_Bass "Bass" // LFE
#define MSGTR_EQU_All "Alle"
#define MSGTR_EQU_Channel1 "Kanal 1:"
#define MSGTR_EQU_Channel2 "Kanal 2:"
#define MSGTR_EQU_Channel3 "Kanal 3:"
#define MSGTR_EQU_Channel4 "Kanal 4:"
#define MSGTR_EQU_Channel5 "Kanal 5:"
#define MSGTR_EQU_Channel6 "Kanal 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Pfad"
#define MSGTR_PLAYLIST_Selected "Ausgewählte Dateien"
#define MSGTR_PLAYLIST_Files "Dateien"
#define MSGTR_PLAYLIST_DirectoryTree "Verzeichnisbaum"

// --- preferences
#define MSGTR_PREFERENCES_Audio "Audio"
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_SubtitleOSD "Untertitel & OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs & Demuxer"
#define MSGTR_PREFERENCES_Misc "Sonstiges"

#define MSGTR_PREFERENCES_None "Nichts"
#define MSGTR_PREFERENCES_DriverDefault "Treiber-Standardeinstellung"
#define MSGTR_PREFERENCES_AvailableDrivers "Verfügbare Treiber:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Spiele keinen Ton"
#define MSGTR_PREFERENCES_NormalizeSound "Normalisiere Ton"
#define MSGTR_PREFERENCES_EnEqualizer "Equalizer verwenden"
#define MSGTR_PREFERENCES_ExtraStereo "Extra Stereo verwenden"
#define MSGTR_PREFERENCES_Coefficient "Koeffizient:"
#define MSGTR_PREFERENCES_AudioDelay "Audio-Verzögerung"
#define MSGTR_PREFERENCES_DoubleBuffer "Doublebuffering verwenden"
#define MSGTR_PREFERENCES_DirectRender "Direct-Rendering verwenden"
#define MSGTR_PREFERENCES_FrameDrop "Frame-Dropping aktivieren"
#define MSGTR_PREFERENCES_HFrameDrop "HARTES Frame-Dropping aktivieren (gefährlich)"
#define MSGTR_PREFERENCES_Flip "Bild horizontal spiegeln"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Zeit und Indikatoren"
#define MSGTR_PREFERENCES_OSDProgress "Nur Fortschrittsbalken"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Timer, prozentuale und absolute Zeit"
#define MSGTR_PREFERENCES_Subtitle "Untertitel:"
#define MSGTR_PREFERENCES_SUB_Delay "Verzögerung: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Position: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Automatisches Laden der Untertitel ausschalten"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode-Untertitel"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konvertiere Untertitel in das MPlayer-Untertitelformat"
#define MSGTR_PREFERENCES_SUB_SRT "Konvertiere Untertitel in das zeitbasierte SubViewer-Untertitelformat (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Schalte Untertitelüberlappung ein/aus"
#define MSGTR_PREFERENCES_Font "Schrift:"
#define MSGTR_PREFERENCES_FontFactor "Schriftfaktor:"
#define MSGTR_PREFERENCES_PostProcess "Postprocessing aktivieren:"
#define MSGTR_PREFERENCES_AutoQuality "Auto-Qualität: "
#define MSGTR_PREFERENCES_NI "Nicht-Interleaved AVI Parser verwenden"
#define MSGTR_PREFERENCES_IDX "Indextabelle neu erstellen, falls benötigt"
#define MSGTR_PREFERENCES_VideoCodecFamily "Videocodec-Familie:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Audiocodec-Familie:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD-Modus"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Untertitel"
#define MSGTR_PREFERENCES_FRAME_Font "Schrift"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & Demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache"
#define MSGTR_PREFERENCES_FRAME_Misc "Sonstiges"
#define MSGTR_PREFERENCES_Audio_Device "Gerät:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Mixer-Kanal:"
#define MSGTR_PREFERENCES_Message "Bitte bedenke, dass manche Optionen einen Neustart der Wiedergabe erfordern."
#define MSGTR_PREFERENCES_DXR3_VENC "Videoencoder:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Verwende LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Verwende FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Westeuropäische Sprachen (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Westeuropäische Sprachen mit Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Slavische / Westeuropäische Sprache (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galizisch, Maltesisch, Türkisch (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Alte Baltische Schriftzeichen (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Kyrillisch (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabisch (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Modernes Griechisch (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Türkisch (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltisch (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Keltisch (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Hebräische Schriftzeichen (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Russisch (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrainisch, Belarussisch (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Vereinfachte chinesische Schriftzeichen (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Traditionelle chinesische Schriftzeichen (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japanische Schriftzeichen (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Koreanische Schriftzeichen (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thailändische Schriftzeichen (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Kyrillisch Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Slavisch / Zentraleuropäisch Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Keine automatische Skalierung"
#define MSGTR_PREFERENCES_FontPropWidth "Proportional zur Breite des Films"
#define MSGTR_PREFERENCES_FontPropHeight "Proportional zur Höhe des Films"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proportional zur Diagonale des Films"
#define MSGTR_PREFERENCES_FontEncoding "Kodierung:"
#define MSGTR_PREFERENCES_FontBlur "Unschärfe:"
#define MSGTR_PREFERENCES_FontOutLine "Zeichenumriss (Outline):"
#define MSGTR_PREFERENCES_FontTextScale "Textskalierung:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD-Skalierung:"
#define MSGTR_PREFERENCES_Cache "Cache ein/aus"
#define MSGTR_PREFERENCES_CacheSize "Cachegröße: "
#define MSGTR_PREFERENCES_LoadFullscreen "Im Vollbildmodus starten"
#define MSGTR_PREFERENCES_SaveWinPos "Speichere Fensterposition"
#define MSGTR_PREFERENCES_XSCREENSAVER "Deaktiviere XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Aktiviere die Playbar"
#define MSGTR_PREFERENCES_AutoSync "AutoSync ein/aus"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosyncwert: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM-Gerät:"
#define MSGTR_PREFERENCES_DVDDevice "DVD-Gerät:"
#define MSGTR_PREFERENCES_FPS "FPS des Films:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Zeige Videofenster, wenn inaktiv"

#define MSGTR_ABOUT_UHU "GUI-Entwicklung wurde von UHU Linux gesponsert.\n"
#define MSGTR_ABOUT_CoreTeam "   MPlayers Kernentwickler-Team:\n"
#define MSGTR_ABOUT_AdditionalCoders " Weitere Programmierer:\n"
#define MSGTR_ABOUT_MainTesters "     Haupttester:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Fataler Fehler!"
#define MSGTR_MSGBOX_LABEL_Error "Fehler!"
#define MSGTR_MSGBOX_LABEL_Warning "Warnung!"

#endif

 // ======================= VO Video Output drivers ========================

#define MSGTR_VOincompCodec "Der ausgewählte Videoausgabetreiber ist nicht kompatibel mit diesem Codec.\n"
#define MSGTR_VO_GenericError "Dieser Fehler ist aufgetreten"
#define MSGTR_VO_UnableToAccess "Zugriff nicht möglich."
#define MSGTR_VO_ExistsButNoDirectory "existiert schon, ist aber kein Verzeichnis."
#define MSGTR_VO_DirExistsButNotWritable "Ausgabeverzeichnis existiert schon, ist aber nicht beschreibbar."
#define MSGTR_VO_DirExistsAndIsWritable "Ausgabeverzeichnis existiert schon und ist beschreibbar."
#define MSGTR_VO_CantCreateDirectory "Kann Ausgabeverzeichnis nicht erstellen."
#define MSGTR_VO_CantCreateFile "Kann Ausgabedatei nicht erstellen."
#define MSGTR_VO_DirectoryCreateSuccess "Ausgabeverzeichnis erfolgreich erstellt."
#define MSGTR_VO_ParsingSuboptions "Unteroptionen werden geparst."
#define MSGTR_VO_SuboptionsParsedOK "Parsen der Unteroptionen OK."
#define MSGTR_VO_ValueOutOfRange "Wert außerhalb des gültigen Bereichs"
#define MSGTR_VO_NoValueSpecified "Kein Wert angegeben."
#define MSGTR_VO_UnknownSuboptions "Unbekannte Unteroption(en)"

 // vo_jpeg.c
#define MSGTR_VO_JPEG_ProgressiveJPEG "Progressives JPEG aktiviert."
#define MSGTR_VO_JPEG_NoProgressiveJPEG "Progressives JPEG deaktiviert."
#define MSGTR_VO_JPEG_BaselineJPEG "Baseline-JPEG aktiviert."
#define MSGTR_VO_JPEG_NoBaselineJPEG "Baseline-JPEG deaktiviert."

// vo_pnm.c
#define MSGTR_VO_PNM_ASCIIMode "ASCII-Modus aktiviert."
#define MSGTR_VO_PNM_RawMode "Raw-Modus aktiviert."
#define MSGTR_VO_PNM_PPMType "Werde PPM-Dateien schreiben."
#define MSGTR_VO_PNM_PGMType "Werde PGM-Dateien schreiben."
#define MSGTR_VO_PNM_PGMYUVType "Werde PGMYUV-Dateien schreiben."

// vo_yuv4mpeg.c
#define MSGTR_VO_YUV4MPEG_InterlacedHeightDivisibleBy4 "Interlaced-Modus benötigt eine durch 4 teilbare Bildhöhe."
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Kann Linien-Buffer für den Interlaced-Modus nicht allozieren."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Eingabe ist nicht RGB, kann Chrominanz nicht in Felder separieren!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "Bildhöhe muss durch 2 teilbar sein."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Nicht genug Speicher, um RGB-Framebuffer zu allozieren."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Bekomme keinen Speicher oder Datei-Handle, um \"%s\" zu schreiben!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Fehler beim Schreiben des Bildes auf die Ausgabe!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Unbekanntes Subdevice: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Benutze Interlaced-Ausgabemodus, oberes Feld (top-field) zuerst."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Benutze Interlaced-Ausgabemodus, unteres Feld (bottom-field) zuerst."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Benutze (Standard-) Progressive-Frame-Modus."

// Old vo drivers that have been replaced

#define MSGTR_VO_PGM_HasBeenReplaced "Der pgm-Videoausgabetreiber wurde ersetzt durch -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Der md5-Videoausgabetreiber wurde ersetzt durch -vo md5sum.\n"

// ======================= AO Audio Output drivers ========================

// libao2

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed "audio_out: Die Module alsa9 und alsa1x wurden entfernt, benutze stattdessen -ao alsa.\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Kann Mixer %s: %s nicht öffnen.\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: Soundkartenmixer hat Kanal '%s' nicht, benutze Standard.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Kann Audiogerät %s nicht öffnen: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Kann Dateideskriptor nicht anlegen, blockiert: %s\n"
#define MSGTR_AO_OSS_CantSetAC3 "[AO OSS] Kann Audiogerät %s nicht auf AC3-Ausgabe setzen, versuche S16...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Audiogerät auf %d Kanäle zu setzen ist fehlgeschlagen.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: Treiber unterstützt SNDCTL_DSP_GETOSPACE nicht :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   *** Dein Audiotreiber unterstützt select() NICHT ***\nKompiliere MPlayer mit #undef HAVE_AUDIO_SELECT in der Datei config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nKritischer Fehler: *** KANN AUDIO-GERÄT NICHT ERNEUT ÖFFNEN / ZURÜCKSETZEN *** %s\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Verbindung zum Soundserver hergestellt.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Kann keinen Stream öffnen.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Stream geöffnet.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] Größe des Buffers: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Die Lautstärke auf %d zu setzen ist fehlgeschlagen.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] dxr2: %d Hz nicht unterstützt, versuche \"-aop list=resample\"\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound fehlgeschlagen: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] Latenz: [Server: %0.2fs, Netz: %0.2fs] (Anpassung %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] Öffnen des esd-Wiedergabestreams fehlgeschlagen: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] Setzen des DVB-Audiomixers fehlgeschlagen: %s\n"
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz nicht unterstützt, versuche Resampling...\n"

// ao_null.c
// Der hier hat weder mp_msg noch printf's?? [CHECK]

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Datei: %s (%s)\nPCM: Samplerate: %iHz Kanäle: %s Format %s\n"
#define MSGTR_AO_PCM_HintInfo "[AO PCM] Info: Das Anlegen von Dump-Dateien wird am Schnellsten mit -vc dummy -vo null erreicht.\nPCM: Info: Um WAVE-Dateien zu schreiben, benutze -waveheader (Standard).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Öffnen von %s zum Schreiben fehlgeschlagen!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Samplerate: %iHz Kanäle: %s Format %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] benutze Audiotreiber %s.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Nichtunterstütztes Audioformat: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] Initialisierung von SDL-Audio fehlgeschlagen: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Kann Audio nicht öffnen: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] Kontrolle.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Samplerate: %iHz Kanäle: %s Format %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] Wiedergabe: Ungültiges Gerät.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate "[AO SGI] init: setparams fehlgeschlagen: %s\nKonnte gewünschte Samplerate nicht setzen.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE wurde von der angegebenen Ressource nicht akzeptiert.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams fehlgeschlagen: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: Samplerate ist jetzt %lf (gewünschte Rate ist %lf)\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Konnte Audiokanal nicht öffnen: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO fehlgeschlagen.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: Schreiben fehlgeschlagen."
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Kann Audiogerät %s nicht öffnen, %s  -> nosound.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: Deine Karte unterstützt %d Kanäle nicht, %s, %d Hz Samplerate.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n   *** Dein Audiotreiber unterstützt select() NICHT ***\nKompiliere MPlayer mit #undef HAVE_AUDIO_SELECT in der Datei config.h !\n\n"
#define MSGTR_AO_SUN_CantReopenReset "[AO SUN]\nKritischer Fehler: *** KANN AUDIO-GERÄT NICHT ERNEUT ÖFFNEN / ZURÜCKSETZEN *** %s\n"

// ao_alsa5.c
#define MSGTR_AO_ALSA5_InitInfo "[AO ALSA5] alsa-init: angefordertes Format: %d Hz, %d Kanäle, %s\n"
#define MSGTR_AO_ALSA5_SoundCardNotFound "[AO ALSA5] alsa-init: Keine Soundkarten gefunden.\n"
#define MSGTR_AO_ALSA5_InvalidFormatReq "[AO ALSA5] alsa-init: ungültiges Format (%s) angefordert - Ausgabe deaktiviert.\n"
#define MSGTR_AO_ALSA5_PlayBackError "[AO ALSA5] alsa-init: Fehler beim Öffnen der Wiedergabe: %s\n"
#define MSGTR_AO_ALSA5_PcmInfoError "[AO ALSA5] alsa-init: PCM-Informatationsfehler: %s\n"
#define MSGTR_AO_ALSA5_SoundcardsFound "[AO ALSA5] alsa-init: %d Soundkarte(n) gefunden, benutze: %s\n"
#define MSGTR_AO_ALSA5_PcmChanInfoError "[AO ALSA5] alsa-init: PCM-Kanal-Informationsfehler: %s\n"
#define MSGTR_AO_ALSA5_CantSetParms "[AO ALSA5] alsa-init: Fehler beim Setzen der Parameter: %s\n"
#define MSGTR_AO_ALSA5_CantSetChan "[AO ALSA5] alsa-init: Fehler beim Setzen des Kanals: %s\n"
#define MSGTR_AO_ALSA5_ChanPrepareError "[AO ALSA5] alsa-init: Fehler beim Vorbereiten des Kanals: %s\n"
#define MSGTR_AO_ALSA5_DrainError "[AO ALSA5] alsa-uninit: Fehler beim Ablauf der Wiedergabe: %s\n"
#define MSGTR_AO_ALSA5_FlushError "[AO ALSA5] alsa-uninit: Wiedergabe-Flush-Fehler: %s\n"
#define MSGTR_AO_ALSA5_PcmCloseError "[AO ALSA5] alsa-uninit: Fehler beim Schließen von PCM: %s\n"
#define MSGTR_AO_ALSA5_ResetDrainError "[AO ALSA5] alsa-reset: Fehler beim Ablauf der Wiedergabe: %s\n"
#define MSGTR_AO_ALSA5_ResetFlushError "[AO ALSA5] alsa-reset: Wiedergabe-Flush-Fehler: %s\n"
#define MSGTR_AO_ALSA5_ResetChanPrepareError "[AO ALSA5] alsa-reset: Fehler beim Vorbereiten des Kanals: %s\n"
#define MSGTR_AO_ALSA5_PauseDrainError "[AO ALSA5] alsa-pause: Fehler beim Ablauf der Wiedergabe: %s\n"
#define MSGTR_AO_ALSA5_PauseFlushError "[AO ALSA5] alsa-pause: Wiedergabe-Flush-Fehler: %s\n"
#define MSGTR_AO_ALSA5_ResumePrepareError "[AO ALSA5] alsa-resume: Fehler beim Vorbereiten des Kanals: %s\n"
#define MSGTR_AO_ALSA5_Underrun "[AO ALSA5] alsa-play: Alsa-Underrun, setze Stream zurück.\n"
#define MSGTR_AO_ALSA5_PlaybackPrepareError "[AO ALSA5] alsa-play: Fehler beim Vorbereiten der Wiedergabe: %s\n"
#define MSGTR_AO_ALSA5_WriteErrorAfterReset "[AO ALSA5] alsa-play: Schreibfehler nach Rücksetzen: %s - gebe auf.\n"
#define MSGTR_AO_ALSA5_OutPutError "[AO ALSA5] alsa-play: Ausgabefehler: %s\n"

// ao_plugin.c

#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] ungültiges Plugin: %s\n"
