// Translated by: Johannes Feigl <johannes.feigl@aon.at>
// Reworked by Klaus Umbach <klaus.umbach@gmx.net>
// Moritz Bunkus <moritz@bunkus.org>
// Alexander Strasser <eclipse7@gmx.net>
// Sebastian Krämer <mail@kraymer.de>

// In sync with r27123
// TODO: cosmetic commit 27072
// FIXME: improve wording/meaning of periodsize|timer.. for ao_alsa.c strings


// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Verwendung:   mplayer [Optionen] [URL|Verzeichnis/]Dateiname\n"
"\n"
"Basisoptionen: (siehe Manpage für eine vollständige Liste aller Optionen!)\n"
" -vo <drv>        Wähle Videoausgabetreiber ('-vo help' für eine Liste)\n"
" -ao <drv>        Wähle Audioausgabetreiber ('-ao help' für eine Liste)\n"
#ifdef CONFIG_VCD
" vcd://<tracknr>  Spiele einen (S)VCD-Titel (Super Video CD) ab\n"
"                  ( direkter Gerätezugriff, kein mount! )\n"
#endif
#ifdef CONFIG_DVDREAD
" dvd://<titelnr>  Spiele DVD-Titel direkt vom Gerät anstelle einer Datei\n"
#endif
" -alang/-slang    Wähle DVD Audio/Untertitel Sprache (2-Zeichen-Ländercode)\n"
" -ss <Position>   Spiele ab Position (Sekunden oder hh:mm:ss)\n"
" -nosound         Ohne Ton abspielen\n"
" -fs              Im Vollbildmodus abspielen (oder -vm, -zoom, siehe Manpage)\n"
" -x <x> -y <y>    Setze Bildschirmauflösung (für Benutzung mit -vm oder -zoom)\n"
" -sub <Datei>     Benutze Untertitel-Datei (siehe auch -subfps, -subdelay)\n"
" -playlist <Datei> Benutze Playlist aus Datei\n"
" -vid x -aid y    Wähle Video- (x) und Audiostream (y) zum Abspielen\n"
" -fps x -srate y  Ändere Videoframerate (x fps) und Audiosamplingrate (y Hz)\n"
" -pp <Qualität>   Aktiviere Postprocessing-Filter (siehe Manpage für Details)\n"
" -framedrop       Verwerfe einzelne Frames (bei langsamen Rechnern)\n"
"\n"
"Grundlegende Tasten: (vollständige Liste in der Manpage, siehe auch input.conf)\n"
" <- oder ->       Springe 10 Sekunden zurück/vor\n"
" runter/hoch      Springe  1 Minute zurück/vor\n"
" Bild runter/hoch Springe 10 Minuten zurück/vor\n"
" < oder >         Gehe in der Playlist zurück/vor\n"
" p oder LEER      Pause (drücke eine beliebige Taste zum Fortsetzen)\n"
" q oder ESC       Abspielen stoppen und Programm beenden\n"
" + oder -         Audioverzögerung um +/- 0.1 Sekunde anpassen\n"
" o                OSD-Modus:  Aus / Suchleiste / Suchleiste + Zeitangabe\n"
" * oder /         PCM-Lautstärke erhöhen oder erniedrigen\n"
" x oder z         Untertitelverzögerung um +/- 0.1 Sekunde anpassen\n"
" r oder t         Verschiebe die Untertitel-Position, siehe auch '-vf expand'\n"
"\n"
" * * * SIEHE MANPAGE FÜR DETAILS, WEITERE OPTIONEN UND TASTEN * * *\n"
"\n";
#endif

// libmpcodecs/ad_dvdpcm.c:
#define MSGTR_SamplesWanted "Beispiele für dieses Format werden gebraucht, um die Unterstützung zu verbessern. Bitte kontaktiere die Entwickler.\n"


// ========================= MPlayer Ausgaben ===========================

// mplayer.c:
#define MSGTR_Exiting "\nBeenden...\n"
#define MSGTR_ExitingHow "\nBeenden... (%s)\n"
#define MSGTR_Exit_quit "Ende"
#define MSGTR_Exit_eof "Dateiende erreicht"
#define MSGTR_Exit_error "Fataler Fehler"
#define MSGTR_IntBySignal "\nMPlayer wurde durch Signal %d im Modul %s unterbrochen.\n"
#define MSGTR_NoHomeDir "Kann Homeverzeichnis nicht finden.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") fehlgeschlagen.\n"
#define MSGTR_CreatingCfgFile "Erstelle Konfigurationsdatei: %s\n"
#define MSGTR_BuiltinCodecsConf "Benutze eingebaute Standardwerte für codecs.conf.\n"
#define MSGTR_CantLoadFont "Kann Bitmap-Schriftdatei nicht laden: %s\n"
#define MSGTR_CantLoadSub "Kann Untertitel nicht laden: %s\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: Ausgewählter Stream fehlt!\n"
#define MSGTR_CantOpenDumpfile "Kann dump-Datei nicht öffnen!\n"
#define MSGTR_CoreDumped "Core dumped ;)\n"
#define MSGTR_FPSnotspecified "FPS ist im Header nicht angegeben (oder ungültig)! Benutze die Option -fps!\n"
#define MSGTR_TryForceAudioFmtStr "Versuche Audiocodecfamilie %s zu erzwingen...\n"
#define MSGTR_CantFindAudioCodec "Kann Codec für Audioformat 0x%X nicht finden!\n"
#define MSGTR_RTFMCodecs "Lies DOCS/HTML/de/codecs.html!\n"
#define MSGTR_TryForceVideoFmtStr "Versuche Videocodecfamilie %s zu erzwingen...\n"
#define MSGTR_CantFindVideoCodec "Kann keinen Codec finden, der zu gewählter Option -vo und Videoformat 0x%X passt!\n"
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
"  - Experimentiere mit verschiedenen Werten für -autosync, 30 ist ein guter\n"\
"    Startwert.\n"\
"- Langsame Videoausgabe\n"\
"  - Versuche einen anderen -vo Treiber (-vo help für eine Liste)\n"\
"    oder probiere -framedrop!\n"\
"- Langsame CPU\n"\
"  - Versuche nicht, DVDs/große DivX-Filme auf langsamen CPUs abzuspielen.\n"\
"    Probiere Optionen von lavdopts, z.B.\n"\
"    -vfm ffmpeg -lavdopts lowres=1:fast:skiploopfilter=all.\n"\
"- Defekte Datei\n"\
"  - Versuche verschiedene Kombinationen von: -nobps -ni -forceidx -mc 0.\n"\
"- Für die Wiedergabe von langsamen Medien (NFS/SMB, DVD, VCD usw)\n"\
"  - Versuche -cache 8192.\n"\
"- Benutzt du -cache zusammen mit einer nicht-interleavten AVI-Datei?\n"\
"  - Versuche -nocache.\n"\
"Lies DOCS/HTML/de/video.html; dort stehen Tipps für optimale Einstellungen.\n"\
"(Schau evtl. auch bei den entsprechenden englischen Seiten.)\n"\
"Wenn dies nicht hilft, lies DOCS/HTML/de/bugreports.html!\n\n"

#define MSGTR_NoGui "MPlayer wurde OHNE GUI-Unterstützung kompiliert.\n"
#define MSGTR_GuiNeedsX "MPlayer GUI erfordert X11.\n"
#define MSGTR_Playing "\nSpiele %s.\n"
#define MSGTR_NoSound "Audio: kein Ton!\n"
#define MSGTR_FPSforced "FPS von %5.3f erzwungen (ftime: %5.3f).\n"
#define MSGTR_CompiledWithRuntimeDetection "MPlayer mit CPU-Erkennung zur Laufzeit kompiliert.\n"
#define MSGTR_CompiledWithCPUExtensions "Kompiliert für x86 CPU mit folgenden Erweiterungen:"
#define MSGTR_AvailableVideoOutputDrivers "Verfügbare Videoausgabetreiber:\n"
#define MSGTR_AvailableAudioOutputDrivers "Verfügbare Audioausgabetreiber:\n"
#define MSGTR_AvailableAudioCodecs "Verfügbare Audiocodecs:\n"
#define MSGTR_AvailableVideoCodecs "Verfügbare Videocodecs:\n"
#define MSGTR_AvailableAudioFm "Verfügbare (in das Binary kompilierte) Audiocodecfamilien:\n"
#define MSGTR_AvailableVideoFm "Verfügbare (in das Binary kompilierte) Videocodecfamilien:\n"
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
"  Bitte lies DOCS/HTML/de/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer stürzte aufgrund einer 'ungültigen Anweisung' ab.\n"\
"  Das passiert normalerweise, wenn du MPlayer auf einer anderen CPU\n"\
"  ausführst als auf der, für die er kompiliert/optimiert wurde.\n"\
"  Überprüfe das!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer stürzte wegen falscher Benutzung der CPU/FPU/des RAMs ab.\n"\
"  Kompiliere MPlayer erneut mit --enable-debug und erstelle mit 'gdb'\n"\
"  einen Backtrace und eine Disassemblierung. Details dazu findest du\n"\
"  in DOCS/HTML/de/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer ist abgestürzt. Das sollte nicht passieren.\n"\
"  Es kann sich um einen Fehler im MPlayer-Code _oder_ in deinen Treibern\n"\
"  _oder_ in deinem gcc handeln. Wenn du meinst, es sei MPlayers Fehler, dann\n"\
"  lies DOCS/HTML/de/bugreports.html und folge den dortigen Anweisungen.\n"\
"  Wir können und werden dir nicht helfen, wenn du nicht alle dort aufgeführten\n"\
"  Informationen zur Verfügung stellst.\n"
#define MSGTR_LoadingConfig "Lade Konfiguration '%s'\n"
#define MSGTR_LoadingProtocolProfile "Lade protokolleigenes Profil '%s'\n"
#define MSGTR_LoadingExtensionProfile "Lade erweiterungseigenes Profil '%s'\n"
#define MSGTR_AddedSubtitleFile "SUB: Untertiteldatei (%d) hinzugefügt: %s\n"
#define MSGTR_RemovedSubtitleFile "SUB: Untertiteldatei (%d) entfernt: %s\n"
#define MSGTR_ErrorOpeningOutputFile "Fehler beim Öffnen von Datei [%s] zum Schreiben!\n"
#define MSGTR_CommandLine "Kommandozeile:"
#define MSGTR_RTCDeviceNotOpenable "Konnte %s nicht öffnen: %s (sollte für den Benutzer lesbar sein).\n"
#define MSGTR_LinuxRTCInitErrorIrqpSet "Linux-RTC-Initialisierungsfehler in ioctl (rtc_irqp_set %lu): %s\n"
#define MSGTR_IncreaseRTCMaxUserFreq "Versuche, \"echo %lu > /proc/sys/dev/rtc/max-user-freq\" zu deinen Systemstartskripten hinzuzufügen.\n"
#define MSGTR_LinuxRTCInitErrorPieOn "Linux-RTC-Initialisierungsfehler in ioctl (rtc_pie_on): %s\n"
#define MSGTR_UsingTimingType "Benutze %s-Zeitgeber.\n"
#define MSGTR_NoIdleAndGui "Die Option -idle kann mit GMPlayer nicht verwendet werden.\n"
#define MSGTR_MenuInitialized "Menü initialisiert: %s\n"
#define MSGTR_MenuInitFailed "Initialisierung des Menüs fehlgeschlagen.\n"
#define MSGTR_Getch2InitializedTwice "WARNUNG: getch2_init doppelt aufgerufen!\n"
#define MSGTR_DumpstreamFdUnavailable "Kann Dump dieses Streams nicht anlegen - kein 'fd' verfügbar.\n"
#define MSGTR_CantOpenLibmenuFilterWithThisRootMenu "Kann den libmenu-Videofilter nicht mit dem Ursprungsmenü %s öffnen.\n"
#define MSGTR_AudioFilterChainPreinitError "Fehler bei der Vorinitialisierung der Audiofilterkette!\n"
#define MSGTR_LinuxRTCReadError "Linux-RTC-Lesefehler: %s\n"
#define MSGTR_SoftsleepUnderflow "Warnung! Unterlauf des Softsleep!\n"
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

#define MSGTR_EdlOutOfMem "Kann nicht genug Speicher für EDL-Daten reservieren.\n"
#define MSGTR_EdlRecordsNo "%d EDL-Aktionen gelesen.\n"
#define MSGTR_EdlQueueEmpty "Es gibt keine auszuführenden EDL-Aktionen.\n"
#define MSGTR_EdlCantOpenForWrite "Kann EDL-Datei [%s] nicht zum Schreiben öffnen.\n"
#define MSGTR_EdlCantOpenForRead "Kann EDL-Datei [%s] nicht zum Lesen öffnen.\n"
#define MSGTR_EdlNOsh_video "Kann EDL nicht ohne Video verwenden, deaktiviere.\n"
#define MSGTR_EdlNOValidLine "Ungültige EDL-Zeile: %s\n"
#define MSGTR_EdlBadlyFormattedLine "Schlecht formatierte EDL-Zeile [%d], verwerfe.\n"
#define MSGTR_EdlBadLineOverlap "Letzte Stop-Position war [%f]; nächster Start ist [%f].\n"\
"Einträge müssen in chronologischer Reihenfolge sein, ohne Überschneidung. Verwerfe.\n"
#define MSGTR_EdlBadLineBadStop "Zeit des Stopps muss nach der Startzeit sein.\n"
#define MSGTR_EdloutBadStop "EDL-Sprung abgebrochen, letzter Start > Stop\n"
#define MSGTR_EdloutStartSkip "EDL-Sprung begonnen, drücke 'i' erneut, um den Block zu beenden.\n"
#define MSGTR_EdloutEndSkip "EDL-Sprung beendet, Zeile geschrieben.\n"
#define MSGTR_MPEndposNoSizeBased "Die Option -endpos unterstützt für MPlayer noch keine Größenangaben.\n"

// mplayer.c OSD
#define MSGTR_OSDenabled "aktiviert"
#define MSGTR_OSDdisabled "deaktiviert"
#define MSGTR_OSDAudio "Ton: %s"
#define MSGTR_OSDVideo "Video: %s"
#define MSGTR_OSDChannel "Kanal: %s"
#define MSGTR_OSDSubDelay "Untertitelverzögerung: %dms"
#define MSGTR_OSDSpeed "Geschwindigkeit: x %6.2f"
#define MSGTR_OSDosd "OSD: %s"
#define MSGTR_OSDChapter "Kapitel: (%d) %s"
#define MSGTR_OSDAngle "Winkel: %d/%d"

// Werte für Eigenschaften
#define MSGTR_Enabled "aktiviert"
#define MSGTR_EnabledEdl "aktiviert (EDL)"
#define MSGTR_Disabled "deaktiviert"
#define MSGTR_HardFrameDrop "hart"
#define MSGTR_Unknown "unbekannt"
#define MSGTR_Bottom "unten"
#define MSGTR_Center "mittig"
#define MSGTR_Top "oben"
#define MSGTR_SubSourceFile "Datei"
#define MSGTR_SubSourceVobsub "vobsub"
#define MSGTR_SubSourceDemux "eingebettet"

// Balkennamen für OSD
#define MSGTR_Volume "Lautstärke"
#define MSGTR_Panscan "Panscan"
#define MSGTR_Gamma "Gamma"
#define MSGTR_Brightness "Helligkeit"
#define MSGTR_Contrast "Kontrast"
#define MSGTR_Saturation "Sättigung"
#define MSGTR_Hue "Farbton"
#define MSGTR_Balance "Balance"

// Eigenschaftenzustände
#define MSGTR_LoopStatus "Endloswiederholung: %s"
#define MSGTR_MuteStatus "Stumm: %s"
#define MSGTR_AVDelayStatus "A/V-Verzögerung: %s"
#define MSGTR_OnTopStatus "Immer im Vordergrund: %s"
#define MSGTR_RootwinStatus "Anzeige auf dem Desktop: %s"
#define MSGTR_BorderStatus "Rahmen: %s"
#define MSGTR_FramedroppingStatus "Framedropping: %s"
#define MSGTR_VSyncStatus "VSync: %s"
#define MSGTR_SubSelectStatus "Untertitel: %s"
#define MSGTR_SubSourceStatus "Untertitelquelle: %s"
#define MSGTR_SubPosStatus "Untertitelposition: %s/100"
#define MSGTR_SubAlignStatus "Untertitelausrichtung: %s"
#define MSGTR_SubDelayStatus "Untertitelverzögerung: %s"
#define MSGTR_SubScale "Untertitelskalierung: %s"
#define MSGTR_SubVisibleStatus "Untertitel: %s"
#define MSGTR_SubForcedOnlyStatus "Erzwungene Untertitel: %s"

// mencoder.c:
#define MSGTR_UsingPass3ControlFile "Verwende Pass 3 Kontrolldatei: %s\n"
#define MSGTR_MissingFilename "\nDateiname nicht angegeben.\n\n"
#define MSGTR_CannotOpenFile_Device "Kann Datei/Gerät nicht öffnen.\n"
#define MSGTR_CannotOpenDemuxer "Kann Demuxer nicht öffnen.\n"
#define MSGTR_NoAudioEncoderSelected "\nKein Audioencoder (-oac)  ausgewählt. \nWähle einen aus (siehe -oac help) oder verwende -nosound.\n"
#define MSGTR_NoVideoEncoderSelected "\nKein Videoencoder (-ovc) ausgewählt. \nWähle einen aus (siehe -ovc help).\n"
#define MSGTR_CannotOpenOutputFile "Kann Ausgabedatei '%s' nicht öffnen.\n"
#define MSGTR_EncoderOpenFailed "Öffnen des Encoders fehlgeschlagen.\n"
#define MSGTR_MencoderWrongFormatAVI "\nWARNING: Format der Ausgabedatei ist _AVI_. Siehe '-of help'.\n"
#define MSGTR_MencoderWrongFormatMPG "\nWARNUNG: Format der Ausgabedatei ist _MPEG_. Siehe '-of help'.\n"
#define MSGTR_MissingOutputFilename "Keine Ausgabedatei angegeben, schaue dir bitte die Option '-o' an."
#define MSGTR_ForcingOutputFourcc "Erzwinge Ausgabe-FourCC %x [%.4s].\n"
#define MSGTR_ForcingOutputAudiofmtTag "Erzwinge Audioformatkennzeichnung 0x%x in der Ausgabe.\n"
#define MSGTR_DuplicateFrames "\n%d doppelte(r) Frame(s)!\n"
#define MSGTR_SkipFrame "\nFrame übersprungen!\n"
#define MSGTR_ResolutionDoesntMatch "\nNeue Videodatei hat eine andere Auflösung oder anderen Farbraum als die vorige.\n"
#define MSGTR_FrameCopyFileMismatch "\nAlle Videodateien müssen für -ovc copy identische fps, Auflösung und Codec haben.\n"
#define MSGTR_AudioCopyFileMismatch "\nAlle Videodateien müssen für -oac copy identischen Audiocodec und Format haben.\n"
#define MSGTR_NoAudioFileMismatch "\nVideodateien ohne Ton können nicht mit Audio/Video-Dateien gemischt werden.  Versuche -nosound.\n"
#define MSGTR_NoSpeedWithFrameCopy "WARNUNG: Korrektes Funktionieren von -speed kann zusammen mit -oac copy nicht garantiert werden!\n"\
"Das Ergebnis der Encodierung könnte defekt sein!\n"
#define MSGTR_ErrorWritingFile "%s: Fehler beim Schreiben der Datei.\n"
#define MSGTR_FlushingVideoFrames "\nVideoframes werden geleert.\n"
#define MSGTR_FiltersHaveNotBeenConfiguredEmptyFile "Filter wurden nicht konfiguriert! Leere Datei?\n"
#define MSGTR_RecommendedVideoBitrate "Empfohlene Videobitrate für %s CD(s): %d\n"
#define MSGTR_VideoStreamResult "\nVideostream: %8.3f kbit/s  (%d B/s)  Größe: %"PRIu64" Bytes  %5.3f Sek.  %d Frames\n"
#define MSGTR_AudioStreamResult "\nAudiostream: %8.3f kbit/s  (%d B/s)  Größe: %"PRIu64" Bytes  %5.3f Sek.\n"
#define MSGTR_EdlSkipStartEndCurrent "EDL-SPRUNG: Beginn: %.2f  Ende: %.2f   Aktuell: V: %.2f  A: %.2f     \r"
#define MSGTR_OpenedStream "Erfolg: Format: %d  Daten: 0x%X - 0x%x\n"
#define MSGTR_VCodecFramecopy "Videocodec: Framecopy (%dx%d %dbpp fourcc=%x)\n"
#define MSGTR_ACodecFramecopy "Audiocodec: Framecopy (Format=%x chans=%d Rate=%d Bits=%d B/s=%d Sample-%d)\n"
#define MSGTR_CBRPCMAudioSelected "CBR PCM Audio ausgewählt.\n"
#define MSGTR_MP3AudioSelected "MP3 Audio ausgewählt.\n"
#define MSGTR_CannotAllocateBytes "Konnte %d Bytes nicht reservieren.\n"
#define MSGTR_SettingAudioDelay "Setze Audioverzögerung auf %5.3fs.\n"
#define MSGTR_SettingVideoDelay "Setze Videoverzögerung auf %5.3fs.\n"
#define MSGTR_SettingAudioInputGain "Setze Audioeingangsverstärkung auf %f.\n"
#define MSGTR_LamePresetEquals "\nPreset=%s\n\n"
#define MSGTR_LimitingAudioPreload "Limitiere Audio-Preload auf 0.4s.\n"
#define MSGTR_IncreasingAudioDensity "Erhöhe Audiodichte auf 4.\n"
#define MSGTR_ZeroingAudioPreloadAndMaxPtsCorrection "Erzwinge Audio-Preload von 0, maximale pts-Korrektur von 0.\n"
#define MSGTR_CBRAudioByterate "\n\nCBR Audio: %d Bytes/Sek, %d Bytes/Block\n"
#define MSGTR_LameVersion "LAME-Version %s (%s)\n\n"
#define MSGTR_InvalidBitrateForLamePreset "Fehler: Die angegebene Bitrate ist außerhalb des gültigen Bereichs\nfür dieses Preset.\n"\
"\n"\
"Bei Benutzung dieses Modus musst du einen Wert zwischen \"8\" und \"320\" angeben.\n"\
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
"Diese werden kontinuierlich aktualisiert, um den neuesten stattfindenden\n"\
"Entwicklungen zu entsprechen. Daher sollte dir das Resultat die fast beste\n"\
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
"            Nachteil des fast-Schalters ist, dass die Bitrate oft leicht höher\n"\
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
"Für den ABR-Modus sind ein paar Pseudonyme verfügbar:\n"\
"phone => 16kbps/Mono        phon+/lw/mw-eu/sw => 24kbps/Mono\n"\
"mw-us => 40kbps/Mono        voice => 56kbps/Mono\n"\
"fm/radio/tape => 112kbps    hifi => 160kbps\n"\
"cd => 192kbps               studio => 256kbps"
#define MSGTR_LameCantInit \
"Kann Optionen für Lame nicht setzen, überprüfe Bitrate/Samplerate.\n"\
"Manche sehr niedrige Bitraten (<32) benötigen niedrigere Sampleraten \n"\
"(z.B. -srate 8000). Wenn alles andere nicht funktioniert, versuche es \n"\
"mit einem Preset."
#define MSGTR_ConfigFileError "Konfigurationsdatei-Fehler"
#define MSGTR_ErrorParsingCommandLine "Fehler beim Parsen der Kommandozeile"
#define MSGTR_VideoStreamRequired "Videostream zwingend notwendig!\n"
#define MSGTR_ForcingInputFPS "Input-Framerate wird als statt dessen als %5.3f interpretiert.\n"
#define MSGTR_RawvideoDoesNotSupportAudio "Ausgabedateiformat RAWVIDEO unterstützt kein Audio - Audio wird deaktiviert.\n"
#define MSGTR_DemuxerDoesntSupportNosound "Dieser Demuxer unterstützt -nosound noch nicht.\n"
#define MSGTR_MemAllocFailed "Speicherreservierung fehlgeschlagen."
#define MSGTR_NoMatchingFilter "Konnte passenden Filter/passendes ao-Format nicht finden!\n"
#define MSGTR_MP3WaveFormatSizeNot30 "sizeof(MPEGLAYER3WAVEFORMAT)==%d!=30, vielleicht fehlerhafter C-Compiler?\n"
#define MSGTR_NoLavcAudioCodecName "Audio LAVC, Fehlender Codecname!\n"
#define MSGTR_LavcAudioCodecNotFound "Audio LAVC, konnte Encoder für Codec %s nicht finden.\n"
#define MSGTR_CouldntAllocateLavcContext "Audio LAVC, konnte Kontext nicht zuordnen!\n"
#define MSGTR_CouldntOpenCodec "Konnte Codec %s nicht öffnen, br=%d.\n"
#define MSGTR_CantCopyAudioFormat "Audioformat 0x%x ist nicht mit '-oac copy' kompatibel. Versuche bitte stattdessen '-oac pcm' oder benutze '-fafmttag', um ein anderes Format zu erzwingen.\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     Modus für variable Bitrate\n"\
"                0: cbr (konstante Bitrate)\n"\
"                1: mt (Mark Taylor VBR Algorithmus)\n"\
"                2: rh (Robert Hegemann VBR Algorithmus - Standard)\n"\
"                3: abr (durchschnittliche Bitrate)\n"\
"                4: mtrh (Mark Taylor Robert Hegemann VBR Algorithmus)\n"\
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
"                 medium: VBR-Encodierung, gute Qualität\n"\
"                 (150-180 kbps Bitratenbereich)\n"\
"                 standard:  VBR-Encodierung, hohe Qualität\n"\
"                 (170-210 kbps Bitratenbereich)\n"\
"                 extreme: VBR-Encodierung, sehr hohe Qualität\n"\
"                 (200-240 kbps Bitratenbereich)\n"\
"                 insane:  CBR-Encodierung, höchste Preset-Qualität\n"\
"                 (320 kbps Bitrate)\n"\
"                 <8-320>: ABR-Encodierung mit der angegebenen durchschnittlichen\n"\
"                          Bitrate\n\n"

//codec-cfg.c:
#define MSGTR_DuplicateFourcc "Doppelter FourCC."
#define MSGTR_TooManyFourccs "Zu viele FourCCs/Formate..."
#define MSGTR_ParseError "Fehler beim Parsen."
#define MSGTR_ParseErrorFIDNotNumber "Fehler beim Parsen (Format-ID keine Nummer?)."
#define MSGTR_ParseErrorFIDAliasNotNumber "Fehler beim Parsen (Alias der Format-ID keine Nummer?)."
#define MSGTR_DuplicateFID "Doppelte Format-ID."
#define MSGTR_TooManyOut "Zu viele Ausgabeformate..."
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
#define MSGTR_OutdatedCodecsConf "Diese codecs.conf ist zu alt und inkompatibel mit dieser Version von MPlayer!"

// fifo.c
#define MSGTR_CannotMakePipe "Kann PIPE nicht anlegen!\n"

// parser-mecmd.c, parser-mpcmd.c
#define MSGTR_NoFileGivenOnCommandLine "'--' weist auf weitere Optionen hin, auf der Kommandozeile wurde aber kein\nDateiname angegeben.\n"
#define MSGTR_TheLoopOptionMustBeAnInteger "Die loop-Option muss ein Integer sein: %s\n"
#define MSGTR_UnknownOptionOnCommandLine "Unbekannte Option in der Kommandozeile: -%s\n"
#define MSGTR_ErrorParsingOptionOnCommandLine "Fehler beim Einlesen der Kommandozeilenoption: -%s\n"
#define MSGTR_InvalidPlayEntry "Ungültiger Eintrag %s\n"
#define MSGTR_NotAnMEncoderOption "-%s ist keine MEncoder-Option\n"
#define MSGTR_NoFileGiven "Keine Datei angegeben\n"

// m_config.c
#define MSGTR_SaveSlotTooOld "Von lvl gefundene Speicherstelle %d ist zu alt: %d !!!\n"
#define MSGTR_InvalidCfgfileOption "Die Option %s kann in Konfigurationsdateien nicht verwendet werden.\n"
#define MSGTR_InvalidCmdlineOption "Die Option %s kann auf der Kommandozeile nicht verwendet werden.\n"
#define MSGTR_InvalidSuboption "Fehler: Option '%s' hat keine Unteroption '%s'.\n"
#define MSGTR_MissingSuboptionParameter "Fehler: Unteroption '%s' von '%s' benötigt einen Parameter!\n"
#define MSGTR_MissingOptionParameter "Fehler: Option '%s' benötigt einen Parameter!\n"
#define MSGTR_OptionListHeader "\n Name                 Typ             Min        Max      Global  CL    Cfg\n\n"
#define MSGTR_TotalOptions "\nInsgesamt: %d Optionen\n"
#define MSGTR_ProfileInclusionTooDeep "WARNUNG: Zu tiefe Profileinfügung.\n"
#define MSGTR_NoProfileDefined "Es wurden keine Profile definiert.\n"
#define MSGTR_AvailableProfiles "Verfügbare Profile:\n"
#define MSGTR_UnknownProfile "Unbekanntes Profil '%s'.\n"
#define MSGTR_Profile "Profil %s: %s\n"

// m_property.c
#define MSGTR_PropertyListHeader "\n Name                 Typ             Min        Max\n\n"
#define MSGTR_TotalProperties "\nInsgesamt: %d Eigenschaften\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CDROM-Laufwerk '%s' nicht gefunden.\n"
#define MSGTR_ErrTrackSelect "Fehler beim Auswählen des VCD Tracks."
#define MSGTR_ReadSTDIN "Lese von Standardeingabe (stdin)...\n"
#define MSGTR_UnableOpenURL "Kann URL nicht öffnen: %s\n"
#define MSGTR_ConnToServer "Verbunden mit Server: %s\n"
#define MSGTR_FileNotFound "Datei nicht gefunden: '%s'\n"

#define MSGTR_SMBInitError "Kann die Bibliothek libsmbclient nicht öffnen: %d\n"
#define MSGTR_SMBFileNotFound "Konnte '%s' nicht über das Netzwerk öffnen.\n"
#define MSGTR_SMBNotCompiled "MPlayer wurde ohne SMB-Unterstützung kompiliert.\n"

#define MSGTR_CantOpenDVD "Kann DVD-Laufwerk nicht öffnen: %s (%s)\n"

// stream_dvd.c
#define MSGTR_DVDspeedCantOpen "Kann DVD-Laufwerk nicht zum Schreiben öffnen, setzen der DVD-Geschwindigkeit\nbenötigt Schreibzugriff.\n"
#define MSGTR_DVDrestoreSpeed "Setze DVD-Geschwindigkeit auf Laufwerksstandard zurück... "
#define MSGTR_DVDlimitSpeed "Setzen der DVD-Geschwindigkeit auf %dKB/s... "
#define MSGTR_DVDlimitFail "fehlgeschlagen\n"
#define MSGTR_DVDlimitOk "erfolgreich\n"
#define MSGTR_NoDVDSupport "MPlayer wurde ohne DVD-Unterstützung übersetzt, beende.\n"
#define MSGTR_DVDnumTitles "Es sind %d Titel auf dieser DVD.\n"
#define MSGTR_DVDinvalidTitle "Ungültige DVD-Titelnummer: %d\n"
#define MSGTR_DVDnumChapters "Es sind %d Kapitel in diesem DVD-Titel.\n"
#define MSGTR_DVDinvalidChapter "Ungültige DVD-Kapitelnummer: %d\n"
#define MSGTR_DVDinvalidChapterRange "Ungültige Kapitelbereichsangabe: %s\n"
#define MSGTR_DVDinvalidLastChapter "Ungültiger Wert für das letzte DVD-Kapitel: %d\n"
#define MSGTR_DVDnumAngles "Es sind %d Kameraeinstellungen diesem DVD-Titel.\n"
#define MSGTR_DVDinvalidAngle "Ungültige DVD-Kameraeinstellungsnummer %d.\n"
#define MSGTR_DVDnoIFO "Kann die IFO-Datei für den DVD-Titel %d nicht öffnen.\n"
#define MSGTR_DVDnoVMG "Kann VMG-Informationen nicht öffnen!\n"
#define MSGTR_DVDnoVOBs "Kann VOB-Dateien des Titels  (VTS_%02d_1.VOB) nicht öffnen.\n"
#define MSGTR_DVDnoMatchingAudio "Keine passende DVD-Tonspur gefunden!\n"
#define MSGTR_DVDaudioStreamInfo "Audio-Stream: %d Format: %s (%s) Sprache: %s aid: %d.\n"
#define MSGTR_DVDnumAudioChannels "Anzahl der Audiokanäle auf der Disc: %d.\n"
#define MSGTR_DVDaudioChannel "Ausgewählte DVD-Audiospur: %d Sprache: %c%c\n"
#define MSGTR_DVDnoMatchingSubtitle "Keine passende Untertitelspur gefunden!\n"
#define MSGTR_DVDsubtitleChannel "Ausgewählte DVD-Untertitelspur: %d Sprache: %c%c\n"
#define MSGTR_DVDsubtitleLanguage "Untertitel ( sid ): %d Sprache: %s\n"
#define MSGTR_DVDnumSubtitles "Anzahl der Untertitel auf der Disc: %d\n"

// muxer.c, muxer_*.c:
#define MSGTR_TooManyStreams "Zu viele Streams!"
#define MSGTR_RawMuxerOnlyOneStream "Der rawaudio-Muxer unterstützt nur einen Audiostream!\n"
#define MSGTR_IgnoringVideoStream "Ignoriere Videostream!\n"
#define MSGTR_UnknownStreamType "Warnung, unbekannter Streamtyp: %d\n"
#define MSGTR_WarningLenIsntDivisible "Warnung, 'len' ist nicht durch Samplegröße teilbar!\n"

#define MSGTR_MuxbufMallocErr "Speicher für Muxer-Framepuffer konnte nicht alloziert werden!\n"
#define MSGTR_MuxbufReallocErr "Speicher für Muxer-Framepuffer konnte nicht vergrößert werden!\n"
#define MSGTR_MuxbufSending "Muxer-Framepuffer: Sende %d Frame(s) zum Muxer.\n"
#define MSGTR_WritingHeader "Schreibe Dateikopf...\n"
#define MSGTR_WritingTrailer "Schreibe Dateiindex...\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Warnung! Audiostream-Header %d neu definiert!\n"
#define MSGTR_VideoStreamRedefined "Warnung! Videostream-Header %d neu definiert!\n"
#define MSGTR_TooManyAudioInBuffer "\nZu viele Audiopakete im Puffer: (%d in %d Bytes).\n"
#define MSGTR_TooManyVideoInBuffer "\nZu viele Videopakete im Puffer: (%d in %d Bytes).\n"
#define MSGTR_MaybeNI \
"Vielleicht spielst du eine(n) nicht-interleaved Stream/Datei, oder der \n"\
"Codec funktioniert nicht. Versuche bei AVI-Dateien, den nicht-interleaved \n"\
"Modus mit der Option -ni zu erzwingen.\n"
#define MSGTR_WorkAroundBlockAlignHeaderBug "AVI: Umgehe CBR-MP3 nBlockAlign-Header Bug!\n"
#define MSGTR_SwitchToNi "\nSchlecht interleavte AVI-Datei erkannt, wechsele in den -ni Modus!\n"
#define MSGTR_InvalidAudioStreamNosound "AVI: Ungültige Audiostream-ID: %d - ignoriert (nosound)\n"
#define MSGTR_InvalidAudioStreamUsingDefault "AVI: Ungültige Videostream-ID: %d - ignoriert (verwende Standard)\n"
#define MSGTR_ON2AviFormat "ON2 AVI-Format"
#define MSGTR_Detected_XXX_FileFormat "%s-Dateiformat erkannt!\n"
#define MSGTR_DetectedAudiofile "Audiodatei erkannt!\n"
#define MSGTR_NotSystemStream "Kein MPEG System Stream... (vielleicht ein Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Ungültiger MPEG-ES Stream??? Kontaktiere den Autor, das könnte ein Bug sein :(\n"
#define MSGTR_FormatNotRecognized \
"========== Sorry, dieses Dateiformat wird nicht erkannt/unterstützt ==========\n"\
"============== Sollte dies ein AVI, ASF oder MPEG Stream sein, ===============\n"\
"====================== dann kontaktiere bitte den Autor. =====================\n"
#define MSGTR_SettingProcessPriority "Setze Prozesspriorität: %s\n"
#define MSGTR_FilefmtFourccSizeFpsFtime "[V] Dateiformat:%d  fourcc:0x%X  Größe:%dx%d  fps:%5.3f  ftime:=%6.4f\n"
#define MSGTR_CannotInitializeMuxer "Cannot initialize muxer."
#define MSGTR_MissingVideoStream "Kein Videostream gefunden.\n"
#define MSGTR_MissingAudioStream "Kein Audiostream gefunden. -> kein Ton.\n"
#define MSGTR_MissingVideoStreamBug "Fehlender Videostream!? Kontaktiere den Autor, dies könnte ein Bug sein :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Datei enthält den gewählten Audio- oder Videostream nicht.\n"

#define MSGTR_NI_Forced "erzwungen"
#define MSGTR_NI_Detected "erkannt"
#define MSGTR_NI_Message "NICHT-INTERLEAVETES AVI-Dateiformat %s.\n"

#define MSGTR_UsingNINI "Verwende defektes NICHT-INTERLEAVED AVI-Dateiformat.\n"
#define MSGTR_CouldntDetFNo "Konnte die Anzahl der Frames (für absolute Suche) nicht feststellen.\n"
#define MSGTR_CantSeekRawAVI "Suche in reinen AVI-Streams nicht durchführbar (Index erforderlich, probiere die '-idx'-Option.).\n"
#define MSGTR_CantSeekFile "Kann diese Datei nicht durchsuchen.\n"

#define MSGTR_MOVcomprhdr "MOV: komprimierte Header benötigen ZLIB-Unterstützung.\n"
#define MSGTR_MOVvariableFourCC "MOV: Warnung: Variabler FourCC erkannt!?\n"
#define MSGTR_MOVtooManyTrk "MOV: WARNUNG: Zu viele Tracks."
#define MSGTR_FoundAudioStream "==> Audiostream gefunden: %d\n"
#define MSGTR_FoundVideoStream "==> Videostream gefunden: %d\n"
#define MSGTR_DetectedTV "TV erkannt! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Kann Ogg-Demuxer nicht öffnen.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Suche nach Audiostream (Id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Kann Audiostream nicht öffnen: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Kann Untertitelstream nicht öffnen: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Öffnen des Audio-Demuxers fehlgeschlagen: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Öffnen des Untertitel-Demuxers fehlgeschlagen: %s\n"
#define MSGTR_TVInputNotSeekable "TV-Input ist nicht durchsuchbar (Suche des Kanals?).\n"
#define MSGTR_DemuxerInfoChanged "Demuxerinfo %s geändert zu %s.\n"
#define MSGTR_ClipInfo "Clip-Info:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: 30000/1001fps NTSC-Inhalt erkannt, wechsele Framerate.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 24000/1001fps progressiver NTSC-Inhalt erkannt, wechsele Framerate.\n"

#define MSGTR_CacheFill "\rFülle Zwischenpuffer: %5.2f%% (%"PRId64" Bytes)   "
#define MSGTR_NoBindFound "Bindung für Taste '%s' nicht gefunden."
#define MSGTR_FailedToOpen "Konnte '%s' nicht öffnen.\n"

#define MSGTR_VideoID "[%s] Videostream gefunden, -vid %d\n"
#define MSGTR_AudioID "[%s] Audiostream gefunden, -aid %d\n"
#define MSGTR_SubtitleID "[%s] Untertitelstream gefunden, -sid %d\n"

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

#define MSGTR_UsingExternalPP "[PP] Verwende externe Postprocessing-Filter, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Verwende Postprocessing-Routinen des Codecs, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Videoeigenschaft '%s' wird von ausgewählten vo & vd nicht unterstützt.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Erforderliche Videocodec Familie [%s] (vfm=%s) nicht verfügbar.\nAktiviere sie beim Kompilieren.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Erforderliche Audiocodec-Familie [%s] (afm=%s) nicht verfügbar.\nAktiviere sie beim Kompilieren.\n"
#define MSGTR_OpeningVideoDecoder "Öffne Videodecoder: [%s] %s\n"
#define MSGTR_SelectedVideoCodec "Ausgewählter Videocodec: [%s] vfm: %s (%s)\n"
#define MSGTR_OpeningAudioDecoder "Öffne Audiodecoder: [%s] %s\n"
#define MSGTR_SelectedAudioCodec "Ausgewählter Audiocodec: [%s] afm: %s (%s)\n"
#define MSGTR_BuildingAudioFilterChain "Baue Tonfilterkette von %dHz/%dch/%s nach %dHz/%dch/%s auf...\n"
#define MSGTR_UninitVideoStr "Deinitialisiere Video: %s\n"
#define MSGTR_UninitAudioStr "Deinitialisiere Audio: %s\n"
#define MSGTR_VDecoderInitFailed "Initialisierung des Videodecoders fehlgeschlagen :(\n"
#define MSGTR_ADecoderInitFailed "Initialisierung des Audiodecoders fehlgeschlagen :(\n"
#define MSGTR_ADecoderPreinitFailed "Vorinitialisierung des Audiodecoders fehlgeschlagen :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Reserviere %d Bytes für den Eingangspuffer.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Reserviere %d + %d = %d Bytes für den Ausgabepuffer.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Initialisiere LIRC-Unterstützung...\n"
#define MSGTR_LIRCopenfailed "Fehler beim Öffnen der LIRC-Unterstützung.\nVerwendung der Fernbedienung nicht möglich.\n"
#define MSGTR_LIRCcfgerr "Kann LIRC-Konfigurationsdatei %s nicht lesen.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Konnte Videofilter '%s' nicht finden.\n"
#define MSGTR_CouldNotOpenVideoFilter "Konnte Videofilter '%s' nicht öffnen.\n"
#define MSGTR_OpeningVideoFilter "Öffne Videofilter: "
#define MSGTR_CannotFindColorspace "Konnte keinen passenden Farbraum finden, auch nicht mit '-vf scale'. :-(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Codec hat sh->disp_w und sh->disp_h nicht gesetzt!\nVersuche Problem zu umgehen..\n"
#define MSGTR_VoConfigRequest "VDec: VO wird versucht, auf %d x %d (Bevorzugter Farbraum: %s) zu setzen.\n"
#define MSGTR_UsingXAsOutputCspNoY "VDec: Verwende %s als Ausgabefarbraum (Nummer %d).\n"
#define MSGTR_CouldNotFindColorspace "Konnte keinen passenden Farbraum finden - neuer Versuch mit '-vf scale'...\n"
#define MSGTR_MovieAspectIsSet "Film-Aspekt ist %.2f:1 - Vorskalierung zur Korrektur der Seitenverhältnisse.\n"
#define MSGTR_MovieAspectUndefined "Film-Aspekt ist undefiniert - keine Vorskalierung durchgeführt.\n"

// vd_dshow.c, vd_dmo.c
#define MSGTR_DownloadCodecPackage "Du musst das Binärcodec-Paket aktualisieren/installieren.\nGehe dazu auf http://www.mplayerhq.hu/dload.html\n"
#define MSGTR_DShowInitOK "INFO: Win32/DShow Videocodec-Initialisierung OK.\n"
#define MSGTR_DMOInitOK "INFO: Win32/DMO Videocodec-Initialisierung OK.\n"

// x11_common.c
#define MSGTR_EwmhFullscreenStateFailed "\nX11: Konnte EWMH-Fullscreen-Event nicht senden!\n"
#define MSGTR_CouldNotFindXScreenSaver "xscreensaver_disable: Konnte das XScreenSaver-Fenster nicht finden.\n"
#define MSGTR_SelectedVideoMode "XF86VM: Ausgewählter Videomodus %dx%d für Bildgröße %dx%d.\n"

#define MSGTR_InsertingAfVolume "[Mixer] Kein Hardware-Mixing, füge Lautstärkefilter ein.\n"
#define MSGTR_NoVolume "[Mixer] Keine Lautstärkeregelung verfügbar.\n"
#define MSGTR_NoBalance "[Mixer] Keine Regelung für Balance verfügbar.\n"


// ====================== GUI-Nachrichten/-Buttons ========================

// --- labels ---
#define MSGTR_About "Über..."
#define MSGTR_FileSelect "Wähle Datei..."
#define MSGTR_SubtitleSelect "Wähle Untertitel..."
#define MSGTR_OtherSelect "Wähle..."
#define MSGTR_AudioFileSelect "Wähle externen Audiokanal..."
#define MSGTR_FontSelect "Wähle Schrift..."
// Beachte: Wenn du MSGTR_PlayList änderst, überprüfe bitte, ob der Eintrag noch zu MSGTR_MENU_PlayList passt.
#define MSGTR_PlayList "Playlist"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_ConfigureEqualizer "Equalizer-Konfiguration"
#define MSGTR_SkinBrowser "Skin-Browser"
#define MSGTR_Network "Netzwerk-Streaming..."
// Beachte: Wenn du MSGTR_Preferences änderst, überprüfe bitte, ob der Eintrag noch zu MSGTR_MENU_Preferences passt.
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
#define MSGTR_NEMDB "Sorry, nicht genug Speicher zum Zeichnen des Puffers."
#define MSGTR_NEMFMR "Sorry, nicht genug Speicher für Menü-Rendering."
#define MSGTR_IDFGCVD "Sorry, habe keinen GUI-kompatiblen Ausgabetreiber gefunden."
#define MSGTR_NEEDLAVC "Sorry, du versuchst, Nicht-MPEG Dateien ohne erneute Encodierung abzuspielen.\nBitte aktiviere lavc in der DXR3/H+-Konfigurationsbox."
#define MSGTR_UNKNOWNWINDOWTYPE "Unbekannten Fenstertyp gefunden ..."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[Skin] Fehler in Skin-Konfigurationsdatei in Zeile %d: %s"
#define MSGTR_SKIN_WARNING1 "[Skin] Warnung: in Skin-Konfigurationsdatei in Zeile %d:\nWidget (%s) gefunden, aber davor wurde \"section\" nicht gefunden"
#define MSGTR_SKIN_WARNING2 "[Skin] Warnung: in Skin-Konfigurationsdatei in Zeile %d:\nWidget (%s) gefunden, aber davor wurde \"subsection\" nicht gefunden."
#define MSGTR_SKIN_WARNING3 "[skin] Warnung: in Skin-Konfigurationsdatei in Zeile %d:\nDiese Untersektion wird vom Widget nicht unterstützt (%s)."
#define MSGTR_SKIN_SkinFileNotFound "[skin] Datei ( %s ) nicht gefunden.\n"
#define MSGTR_SKIN_SkinFileNotReadable "[skin] Datei ( %s ) nicht lesbar.\n"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmaps mit 16 Bits oder weniger werden nicht unterstützt (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Datei nicht gefunden (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP-Lesefehler (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA-Lesefehler (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG-Lesefehler (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE-gepacktes TGA wird nicht unterstützt (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "unbekannter Dateityp (%s)\n"
#define MSGTR_SKIN_BITMAP_ConversionError "Konvertierungsfehler von 24 Bit auf 32 Bit (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "unbekannte Nachricht: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nicht genug Speicher\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Zu viele Schriften deklariert.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Schriftdatei nicht gefunden.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Schriftbilddatei nicht gefunden.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "nicht existierende Schriftbezeichnung (%s)\n"
#define MSGTR_SKIN_UnknownParameter "unbekannter Parameter (%s)\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin nicht gefunden (%s).\n"
#define MSGTR_SKIN_SKINCFG_SelectedSkinNotFound "Ausgewähltes Skin ( %s ) wurde nicht gefunden, versuche 'Standard'...\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin-Konfigurationsdatei: Lesefehler (%s)\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- GTK-Menüs
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
#define MSGTR_MENU_HalfSize "Halbe Größe"
#define MSGTR_MENU_NormalSize "Normale Größe"
#define MSGTR_MENU_DoubleSize "Doppelte Größe"
#define MSGTR_MENU_FullScreen "Vollbild"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Öffne CD/DVD..."
#define MSGTR_MENU_ShowDVDMenu "Zeige DVD Menü"
#define MSGTR_MENU_Titles "Titel"
#define MSGTR_MENU_Title "Titel %2d"
#define MSGTR_MENU_None "(keine)"
#define MSGTR_MENU_Chapters "Kapitel"
#define MSGTR_MENU_Chapter "Kapitel %2d"
#define MSGTR_MENU_AudioLanguages "Audio-Sprachen"
#define MSGTR_MENU_SubtitleLanguages "Untertitel-Sprachen"
#define MSGTR_MENU_PlayList MSGTR_PlayList
#define MSGTR_MENU_SkinBrowser "Skinbrowser"
#define MSGTR_MENU_Preferences MSGTR_Preferences
#define MSGTR_MENU_Exit "Beenden..."
#define MSGTR_MENU_Mute "Stummschaltung"
#define MSGTR_MENU_Original "Original"
#define MSGTR_MENU_AspectRatio "Seitenverhältnis"
#define MSGTR_MENU_AudioTrack "Audiospur"
#define MSGTR_MENU_Track "Spur %d"
#define MSGTR_MENU_VideoTrack "Videospur"
#define MSGTR_MENU_Subtitles "Untertitel"

// --- equalizer
// Beachte: Wenn du MSGTR_EQU_Audio änderst, überprüfe bitte, ob der Eintrag noch zu MSGTR_PREFERENCES_Audio passt.
#define MSGTR_EQU_Audio "Audio"
// Beachte: Wenn du MSGTR_EQU_Video änderst, überprüfe bitte, ob der Eintrag noch zu MSGTR_PREFERENCES_Video passt.
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
#define MSGTR_PREFERENCES_Audio MSGTR_EQU_Audio
#define MSGTR_PREFERENCES_Video MSGTR_EQU_Video
#define MSGTR_PREFERENCES_SubtitleOSD "Untertitel & OSD"
#define MSGTR_PREFERENCES_Codecs "Codecs & Demuxer"
// Beachte: Wenn du MSGTR_PREFERENCES_Misc änderst, überprüfe bitte, ob der Eintrag noch zu MSGTR_PREFERENCES_FRAME_Misc passt.
#define MSGTR_PREFERENCES_Misc "Sonstiges"

#define MSGTR_PREFERENCES_None "Nichts"
#define MSGTR_PREFERENCES_DriverDefault "Treiber-Standardeinstellung"
#define MSGTR_PREFERENCES_AvailableDrivers "Verfügbare Treiber:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Spiele keinen Ton"
#define MSGTR_PREFERENCES_NormalizeSound "Normalisiere Ton"
#define MSGTR_PREFERENCES_EnableEqualizer "Equalizer verwenden"
#define MSGTR_PREFERENCES_SoftwareMixer "Aktiviere Software-Mixer"
#define MSGTR_PREFERENCES_ExtraStereo "Extra Stereo verwenden"
#define MSGTR_PREFERENCES_Coefficient "Koeffizient:"
#define MSGTR_PREFERENCES_AudioDelay "Audioverzögerung"
#define MSGTR_PREFERENCES_DoubleBuffer "Doublebuffering verwenden"
#define MSGTR_PREFERENCES_DirectRender "Direct-Rendering verwenden"
#define MSGTR_PREFERENCES_FrameDrop "Frame-Dropping aktivieren"
#define MSGTR_PREFERENCES_HFrameDrop "HARTES Framedropping aktivieren (gefährlich)"
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
#define MSGTR_PREFERENCES_SUB_USE_ASS "Rendering von SSA/ASS-Untertiteln"
#define MSGTR_PREFERENCES_SUB_ASS_USE_MARGINS "Benutze Ränder"
#define MSGTR_PREFERENCES_SUB_ASS_TOP_MARGIN "Oben: "
#define MSGTR_PREFERENCES_SUB_ASS_BOTTOM_MARGIN "Unten: "
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
#define MSGTR_PREFERENCES_FRAME_Misc MSGTR_PREFERENCES_Misc
#define MSGTR_PREFERENCES_Audio_Device "Gerät:"
#define MSGTR_PREFERENCES_Audio_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Audio_MixerChannel "Mixer-Kanal:"
#define MSGTR_PREFERENCES_Message "Bitte bedenke, dass manche Optionen einen Neustart der Wiedergabe erfordern."
#define MSGTR_PREFERENCES_DXR3_VENC "Videoencoder:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Verwende LAVC (FFmpeg)"
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
#define MSGTR_PREFERENCES_FontEncoding23 "Arabisch Windows (CP1256)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Keine automatische Skalierung"
#define MSGTR_PREFERENCES_FontPropWidth "Proportional zur Breite des Films"
#define MSGTR_PREFERENCES_FontPropHeight "Proportional zur Höhe des Films"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proportional zur Diagonale des Films"
#define MSGTR_PREFERENCES_FontEncoding "Codierung:"
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
#define MSGTR_PREFERENCES_ArtsBroken \
"Neuere Versionen von aRts sind mit GTK 1.x nicht kompatibel und bringen \n"\
"GMPlayer zum Absturz!"

#define MSGTR_ABOUT_UHU "GUI-Entwicklung wurde von UHU Linux gesponsert.\n"
#define MSGTR_ABOUT_Contributors "Mitwirkende am Programm und der Dokumentation\n"
#define MSGTR_ABOUT_Codecs_libs_contributions "Codecs und externe Bibliotheken\n"
#define MSGTR_ABOUT_Translations "Übersetzungen\n"
#define MSGTR_ABOUT_Skins "Skins\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Fataler Fehler!"
#define MSGTR_MSGBOX_LABEL_Error "Fehler!"
#define MSGTR_MSGBOX_LABEL_Warning "Warnung!"

// bitmap.c
#define MSGTR_NotEnoughMemoryC32To1 "[c32to1] Für das Bild ist nicht genügend Speicher vorhanden.\n"
#define MSGTR_NotEnoughMemoryC1To32 "[c1to32] Für das Bild ist nicht genügend Speicher vorhanden.\n"

// cfg.c
#define MSGTR_ConfigFileReadError "[cfg] Fehler beim Lesen der Konfigurationsdatei ...\n"
#define MSGTR_UnableToSaveOption "[cfg] Kann die Option '%s' nicht speichern.\n"

// interface.c
#define MSGTR_DeletingSubtitles "[GUI] Lösche Untertitel.\n"
#define MSGTR_LoadingSubtitles "[GUI] Lade Untertitel: %s\n"
#define MSGTR_AddingVideoFilter "[GUI] Füge Videofilter %s hinzu.\n"
#define MSGTR_RemovingVideoFilter "[GUI] Entferne Videofilter: %s\n"

// mw.c
#define MSGTR_NotAFile "Dies scheint keine Datei zu sein: %s !\n"

// ws.c
#define MSGTR_WS_CouldNotOpenDisplay "[ws] Konnte das Display nicht öffnen.\n"
#define MSGTR_WS_RemoteDisplay "[ws] Ferngesteuertes Display, deaktiviere XMITSHM.\n"
#define MSGTR_WS_NoXshm "[ws] Sorry, dein System unterstützt die Shared-Memory-Erweiterung von X nicht.\n"
#define MSGTR_WS_NoXshape "[ws] Sorry, dein System unterstützt die XShape-Erweiterung nicht.\n"
#define MSGTR_WS_ColorDepthTooLow "[ws] Sorry, die Farbtiefe ist zu niedrig.\n"
#define MSGTR_WS_TooManyOpenWindows "[ws] Es sind zu viele Fenster geöffnet.\n"
#define MSGTR_WS_ShmError "[ws] Fehler der Shared-Memory-Erweiterung\n"
#define MSGTR_WS_NotEnoughMemoryDrawBuffer "[ws] Sorry, nicht genügend Speicher zum Schreiben des Buffers.\n"
#define MSGTR_WS_DpmsUnavailable "DPMS nicht verfügbar?\n"
#define MSGTR_WS_DpmsNotEnabled "Konnte DPMS nicht aktivieren.\n"

// wsxdnd.c
#define MSGTR_WS_NotAFile "Dies scheint keine Datei zu sein...\n"
#define MSGTR_WS_DDNothing "D&D: Nichts zurückgegeben!\n"



// ======================= Videoausgabetreiber ========================

#define MSGTR_VOincompCodec \
"Der ausgewählte Videoausgabetreiber ist nicht kompatibel mit diesem Codec.\n"\
"Versuche den scale-Filter zu deiner Filterliste hinzuzufügen,\n"\
"z.B. mit -vf spp,scale an Stelle von -vf spp.\n"
#define MSGTR_VO_GenericError "Dieser Fehler ist aufgetreten"
#define MSGTR_VO_UnableToAccess "Zugriff nicht möglich."
#define MSGTR_VO_ExistsButNoDirectory "Existiert schon, ist aber kein Verzeichnis."
#define MSGTR_VO_DirExistsButNotWritable "Ausgabeverzeichnis existiert schon, ist aber nicht beschreibbar."
#define MSGTR_VO_DirExistsAndIsWritable "Ausgabeverzeichnis existiert schon und ist beschreibbar."
#define MSGTR_VO_CantCreateDirectory "Kann Ausgabeverzeichnis nicht erstellen."
#define MSGTR_VO_CantCreateFile "Kann Ausgabedatei nicht erstellen."
#define MSGTR_VO_DirectoryCreateSuccess "Ausgabeverzeichnis erfolgreich erstellt."
#define MSGTR_VO_ParsingSuboptions "Unteroptionen werden geparst."
#define MSGTR_VO_SuboptionsParsedOK "Parsen der Unteroptionen OK."
#define MSGTR_VO_ValueOutOfRange "Wert außerhalb des gültigen Bereichs"
#define MSGTR_VO_NoValueSpecified "Kein Wert angegeben."
#define MSGTR_VO_UnknownSuboptions "unbekannte Unteroption(en)"

// vo_aa.c
#define MSGTR_VO_AA_HelpHeader "\n\nDies sind die Unteroptionen von aalib vo_aa:\n"
#define MSGTR_VO_AA_AdditionalOptions \
"Zusätzlich von vo_aa bereitgestellte Optionen:\n" \
"  help        gib diese Hilfsnachricht aus\n" \
"  osdcolor    setze OSD-Farbe\n" \
"  subcolor    setze Untertitelfarbe\n" \
"        die Farbparameter sind folgende:\n"\
"           0 : normal\n" \
"           1 : dim\n" \
"           2 : bold\n" \
"           3 : boldfont\n" \
"           4 : reverse\n" \
"           5 : special\n\n\n"

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
#define MSGTR_VO_YUV4MPEG_InterlacedLineBufAllocFail "Kann Linien-Puffer für den Interlaced-Modus nicht allozieren."
#define MSGTR_VO_YUV4MPEG_InterlacedInputNotRGB "Eingabe ist nicht RGB, kann Chrominanz nicht in Felder separieren!"
#define MSGTR_VO_YUV4MPEG_WidthDivisibleBy2 "Bildhöhe muss durch 2 teilbar sein."
#define MSGTR_VO_YUV4MPEG_NoMemRGBFrameBuf "Nicht genug Speicher, um RGB-Framebuffer zu allozieren."
#define MSGTR_VO_YUV4MPEG_OutFileOpenError "Bekomme keinen Speicher oder Datei-Handle, um \"%s\" zu schreiben!"
#define MSGTR_VO_YUV4MPEG_OutFileWriteError "Fehler beim Schreiben des Bildes auf die Ausgabe!"
#define MSGTR_VO_YUV4MPEG_UnknownSubDev "Unbekanntes Subdevice: %s"
#define MSGTR_VO_YUV4MPEG_InterlacedTFFMode "Benutze Interlaced-Ausgabemodus, oberes Feld (top-field) zuerst."
#define MSGTR_VO_YUV4MPEG_InterlacedBFFMode "Benutze Interlaced-Ausgabemodus, unteres Feld (bottom-field) zuerst."
#define MSGTR_VO_YUV4MPEG_ProgressiveMode "Benutze (Standard-) Progressive-Frame-Modus."

// sub.c

#define MSGTR_VO_SUB_Seekbar "Suchlaufleiste"
#define MSGTR_VO_SUB_Play "Play"
#define MSGTR_VO_SUB_Pause "Pause"
#define MSGTR_VO_SUB_Stop "Stop"
#define MSGTR_VO_SUB_Rewind "Rücklauf"
#define MSGTR_VO_SUB_Forward "Vorlauf"
#define MSGTR_VO_SUB_Clock "Spielzeit"
#define MSGTR_VO_SUB_Contrast "Kontrast"
#define MSGTR_VO_SUB_Saturation "Sättigung"
#define MSGTR_VO_SUB_Volume "Lautstärke"
#define MSGTR_VO_SUB_Brightness "Helligkeit"
#define MSGTR_VO_SUB_Hue "Farbwertkorrektur"
#define MSGTR_VO_SUB_Balance "Balance"

// vo_xv.c
#define MSGTR_VO_XV_ImagedimTooHigh "Dimensionen des Quellbildes sind zu groß: %ux%u (Maximalgröße ist %ux%u)\n"

// alte Videoausgabetreiber, die ersetzt wurden
#define MSGTR_VO_PGM_HasBeenReplaced "Der pgm-Videoausgabetreiber wurde ersetzt durch -vo pnm:pgmyuv.\n"
#define MSGTR_VO_MD5_HasBeenReplaced "Der md5-Videoausgabetreiber wurde ersetzt durch -vo md5sum.\n"


// ======================= Audioausgabetreiber ========================

// audio_out.c
#define MSGTR_AO_ALSA9_1x_Removed \
"audio_out: Die Module alsa9 und alsa1x wurden entfernt, benutze stattdessen \n" \
"-ao alsa.\n"
#define MSGTR_AO_TryingPreferredAudioDriver "Probiere bevorzugten Audiotreiber '%.*s', Optionen '%s'\n"
#define MSGTR_AO_NoSuchDriver "Kein Audiotreiber '%.*s'\n"
#define MSGTR_AO_FailedInit "Konnte Audiotreiber '%s' nicht initialisieren\n"
#define MSGTR_AO_TryingEveryKnown "Probiere jeden bekannten Audiotreiber...\n"

// ao_oss.c
#define MSGTR_AO_OSS_CantOpenMixer "[AO OSS] audio_setup: Kann Mixer %s: %s nicht öffnen.\n"
#define MSGTR_AO_OSS_ChanNotFound "[AO OSS] audio_setup: Soundkartenmixer hat Kanal '%s' nicht, benutze Standard.\n"
#define MSGTR_AO_OSS_CantOpenDev "[AO OSS] audio_setup: Kann Audiogerät %s nicht öffnen: %s\n"
#define MSGTR_AO_OSS_CantMakeFd "[AO OSS] audio_setup: Kann Dateideskriptor nicht anlegen, blockiert: %s\n"
#define MSGTR_AO_OSS_CantSet "[AO OSS] Kann Audiogerät %s nicht auf %s-Ausgabe setzen, versuche %s...\n"
#define MSGTR_AO_OSS_CantSetChans "[AO OSS] audio_setup: Audiogerät auf %d Kanäle zu setzen ist fehlgeschlagen.\n"
#define MSGTR_AO_OSS_CantUseGetospace "[AO OSS] audio_setup: Treiber unterstützt SNDCTL_DSP_GETOSPACE nicht :-(\n"
#define MSGTR_AO_OSS_CantUseSelect "[AO OSS]\n   *** Dein Audiotreiber unterstützt select() NICHT ***\nKompiliere MPlayer mit #undef HAVE_AUDIO_SELECT in der Datei config.h !\n\n"
#define MSGTR_AO_OSS_CantReopen "[AO OSS]\nKritischer Fehler: *** KANN AUDIO-GERÄT NICHT ERNEUT ÖFFNEN / ZURÜCKSETZEN *** %s\n"
#define MSGTR_AO_OSS_UnknownUnsupportedFormat "[AO OSS] Unbekanntes/Nicht unterstütztes OSS-Format: %x.\n"

// ao_arts.c
#define MSGTR_AO_ARTS_CantInit "[AO ARTS] %s\n"
#define MSGTR_AO_ARTS_ServerConnect "[AO ARTS] Verbindung zum Soundserver hergestellt.\n"
#define MSGTR_AO_ARTS_CantOpenStream "[AO ARTS] Kann keinen Stream öffnen.\n"
#define MSGTR_AO_ARTS_StreamOpen "[AO ARTS] Stream geöffnet.\n"
#define MSGTR_AO_ARTS_BufferSize "[AO ARTS] Größe des Buffers: %d\n"

// ao_dxr2.c
#define MSGTR_AO_DXR2_SetVolFailed "[AO DXR2] Die Lautstärke auf %d zu setzen ist fehlgeschlagen.\n"
#define MSGTR_AO_DXR2_UnsupSamplerate "[AO DXR2] %d Hz nicht unterstützt, versuche Resampling.\n"

// ao_esd.c
#define MSGTR_AO_ESD_CantOpenSound "[AO ESD] esd_open_sound fehlgeschlagen: %s\n"
#define MSGTR_AO_ESD_LatencyInfo "[AO ESD] Latenz: [Server: %0.2fs, Netz: %0.2fs] (Anpassung %0.2fs)\n"
#define MSGTR_AO_ESD_CantOpenPBStream "[AO ESD] Öffnen des ESD-Wiedergabestreams fehlgeschlagen: %s\n"

// ao_mpegpes.c
#define MSGTR_AO_MPEGPES_CantSetMixer "[AO MPEGPES] Setzen des DVB-Audiomixers fehlgeschlagen: %s\n"
#define MSGTR_AO_MPEGPES_UnsupSamplerate "[AO MPEGPES] %d Hz nicht unterstützt, versuche Resampling.\n"

// ao_pcm.c
#define MSGTR_AO_PCM_FileInfo "[AO PCM] Datei: %s (%s)\nPCM: Samplerate: %iHz Kanäle: %s Format %s\n"
#define MSGTR_AO_PCM_HintInfo \
"[AO PCM] Info: Das Anlegen von Dump-Dateien wird am Schnellsten mit\n" \
"         -vc null -vo null -ao pcm:fast erreicht.\n" \
"[AO PCM] Info: Um WAVE-Dateien zu schreiben, benutze\n" \
"         -ao pcm:waveheader (Standard).\n"
#define MSGTR_AO_PCM_CantOpenOutputFile "[AO PCM] Öffnen von %s zum Schreiben fehlgeschlagen!\n"

// ao_sdl.c
#define MSGTR_AO_SDL_INFO "[AO SDL] Samplerate: %iHz Kanäle: %s Format %s\n"
#define MSGTR_AO_SDL_DriverInfo "[AO SDL] Benutze Audiotreiber %s.\n"
#define MSGTR_AO_SDL_UnsupportedAudioFmt "[AO SDL] Nichtunterstütztes Audioformat: 0x%x.\n"
#define MSGTR_AO_SDL_CantInit "[AO SDL] SDL-Audio-Initialisierung fehlgeschlagen: %s\n"
#define MSGTR_AO_SDL_CantOpenAudio "[AO SDL] Kann Audio nicht öffnen: %s\n"

// ao_sgi.c
#define MSGTR_AO_SGI_INFO "[AO SGI] Kontrolle.\n"
#define MSGTR_AO_SGI_InitInfo "[AO SGI] init: Samplerate: %iHz Kanäle: %s Format %s\n"
#define MSGTR_AO_SGI_InvalidDevice "[AO SGI] Wiedergabe: Ungültiges Gerät.\n"
#define MSGTR_AO_SGI_CantSetParms_Samplerate \
"[AO SGI] init: setparams fehlgeschlagen: %s\n" \
"         Konnte gewünschte Samplerate nicht setzen.\n"
#define MSGTR_AO_SGI_CantSetAlRate "[AO SGI] init: AL_RATE wurde von der angegebenen Ressource nicht akzeptiert.\n"
#define MSGTR_AO_SGI_CantGetParms "[AO SGI] init: getparams fehlgeschlagen: %s\n"
#define MSGTR_AO_SGI_SampleRateInfo "[AO SGI] init: Samplerate ist jetzt %lf (gewünschte Rate ist %lf).\n"
#define MSGTR_AO_SGI_InitConfigError "[AO SGI] init: %s\n"
#define MSGTR_AO_SGI_InitOpenAudioFailed "[AO SGI] init: Konnte Audiokanal nicht öffnen: %s\n"
#define MSGTR_AO_SGI_Uninit "[AO SGI] uninit: ...\n"
#define MSGTR_AO_SGI_Reset "[AO SGI] reset: ...\n"
#define MSGTR_AO_SGI_PauseInfo "[AO SGI] audio_pause: ...\n"
#define MSGTR_AO_SGI_ResumeInfo "[AO SGI] audio_resume: ...\n"

// ao_sun.c
#define MSGTR_AO_SUN_RtscSetinfoFailed "[AO SUN] rtsc: SETINFO fehlgeschlagen.\n"
#define MSGTR_AO_SUN_RtscWriteFailed "[AO SUN] rtsc: Schreiben fehlgeschlagen.\n"
#define MSGTR_AO_SUN_CantOpenAudioDev "[AO SUN] Kann Audiogerät %s nicht öffnen, %s  -> nosound.\n"
#define MSGTR_AO_SUN_UnsupSampleRate "[AO SUN] audio_setup: Deine Karte unterstützt %d Kanäle nicht, %s, %d Hz Samplerate.\n"
#define MSGTR_AO_SUN_CantUseSelect "[AO SUN]\n" \
"   *** Dein Audiotreiber unterstützt select() NICHT ***\n" \
"Kompiliere MPlayer mit #undef HAVE_AUDIO_SELECT in der Datei config.h !\n\n"
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

// ao_alsa.c
#define MSGTR_AO_ALSA_InvalidMixerIndexDefaultingToZero "[AO_ALSA] Ungültiger Mixerindex. Verwende Standardwert 0.\n"
#define MSGTR_AO_ALSA_MixerOpenError "[AO_ALSA] Fehler beim Öffnen des Mixers: %s\n"
#define MSGTR_AO_ALSA_MixerAttachError "[AO_ALSA] Fehler beim Einfügen von %s: %s\n"
#define MSGTR_AO_ALSA_MixerRegisterError "[AO_ALSA] Fehler bei der Registrierung des Mixers: %s\n"
#define MSGTR_AO_ALSA_MixerLoadError "[AO_ALSA] Fehler beim Laden des Mixers: %s\n"
#define MSGTR_AO_ALSA_UnableToFindSimpleControl "[AO_ALSA] Konnte einfache Steuerung '%s',%i nicht finden.\n"
#define MSGTR_AO_ALSA_ErrorSettingLeftChannel "[AO_ALSA] Fehler beim Setzen des linken Kanals, %s\n"
#define MSGTR_AO_ALSA_ErrorSettingRightChannel "[AO_ALSA] Fehler beim Setzen des rechten Kanals, %s\n"
#define MSGTR_AO_ALSA_CommandlineHelp "\n[AO_ALSA] Kommandozeilenhilfe für -ao alsa:\n"\
"[AO_ALSA] Beispiel: mplayer -ao alsa:device=hw=0.3\n"\
"[AO_ALSA]   Setzt das vierte Hardwaregerät der ersten Karte.\n\n"\
"[AO_ALSA] Optionen:\n"\
"[AO_ALSA]   noblock\n"\
"[AO_ALSA]     Öffnet Gerät im non-blocking-Modus.\n"\
"[AO_ALSA]   device=<Gerätname>\n"\
"[AO_ALSA]     Setzt Gerät (ändere , zu . und : zu =)\n"
#define MSGTR_AO_ALSA_ChannelsNotSupported "[AO_ALSA] Keine Unterstützung für %d Kanäle.\n"
#define MSGTR_AO_ALSA_OpenInNonblockModeFailed "[AO_ALSA] Öffnen im nonblock-Modus fehlgeschlagen, versuche im block-Modus zu öffnen.\n"
#define MSGTR_AO_ALSA_PlaybackOpenError "[AO_ALSA] Fehler beim Öffnen der Wiedergabe: %s\n"
#define MSGTR_AO_ALSA_ErrorSetBlockMode "[AL_ALSA] Fehler beim Setzen des block-Modus %s.\n"
#define MSGTR_AO_ALSA_UnableToGetInitialParameters "[AO_ALSA] Kann Startparameter nicht ermitteln: %s\n"
#define MSGTR_AO_ALSA_UnableToSetAccessType "[AO_ALSA] Kann Zugriffstyp nicht setzen: %s\n"
#define MSGTR_AO_ALSA_FormatNotSupportedByHardware "[AO_ALSA] Format %s wird von der Hardware nicht unterstütz, versuche Standard.\n"
#define MSGTR_AO_ALSA_UnableToSetFormat "[AO_ALSA] Kann Format nicht setzen: %s\n"
#define MSGTR_AO_ALSA_UnableToSetChannels "[AO_ALSA] Kann Kanäle nicht ermitteln: %s\n"
#define MSGTR_AO_ALSA_UnableToDisableResampling "[AO_ALSA] Kann Resampling nicht deaktivieren: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSamplerate2 "[AO_ALSA] Kann samplerate-2 nicht setzen: %s\n"
#define MSGTR_AO_ALSA_UnableToSetBufferTimeNear "[AO_ALSA] Kann Puffer-Zeit in Nähe von %s nicht setzen.\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodTime "[AO_ALSA] Kann Periodenzeit nicht setzen: %s\n"
#define MSGTR_AO_ALSA_BufferTimePeriodTime "[AO_ALSA] Pufferzeit: %d, Periodenzeit: %d\n"
#define MSGTR_AO_ALSA_UnableToGetPeriodSize "[AO ALSA] Kann Periodenzeit nicht ermitteln: %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriodSize "[AO ALSA] Kann Periodenzeit (%ld) nicht setzen: %s\n"
#define MSGTR_AO_ALSA_UnableToSetPeriods "[AO_ALSA] Kann Perioden nicht setzen: %s\n"
#define MSGTR_AO_ALSA_UnableToSetHwParameters "[AO_ALSA] Kann Hardwareparameter nicht setzen: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBufferSize "[AO_ALSA] Kann Puffergröße nicht ermitteln: %s\n"
#define MSGTR_AO_ALSA_UnableToGetSwParameters "[AO_ALSA] Kann Softwareparameter nicht ermitteln: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSwParameters "[AO_ALSA] Kann Softwareparameter nicht setzen: %s\n"
#define MSGTR_AO_ALSA_UnableToGetBoundary "[AO_ALSA] Kann Begrenzungen nicht ermitteln: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStartThreshold "[AO_ALSA] Kann Startschwellenwert nicht setzen: %s\n"
#define MSGTR_AO_ALSA_UnableToSetStopThreshold "[AO_ALSA] Kann Stoppschwellenwert nicht setzen: %s\n"
#define MSGTR_AO_ALSA_UnableToSetSilenceSize "[AO_ALSA] Kann Größe für silence nicht setzen: %s\n"
#define MSGTR_AO_ALSA_PcmCloseError "[AO_ALSA] Fehler beim Schließen von pcm: %s\n"
#define MSGTR_AO_ALSA_NoHandlerDefined "[AO_ALSA] Kein Handler definiert!\n"
#define MSGTR_AO_ALSA_PcmPrepareError "[AO_ALSA] Fehler beim Vorbereiten von pcm: %s\n"
#define MSGTR_AO_ALSA_PcmPauseError "[AO_ALSA] Fehler beim Pausieren von pcm: %s\n"
#define MSGTR_AO_ALSA_PcmDropError "[AO_ALSA] Fehler beim Dropping von pcm: %s\n"
#define MSGTR_AO_ALSA_PcmResumeError "[AO_ALSA] Fehler beim Wiederherstellen von pcm: %s\n"
#define MSGTR_AO_ALSA_DeviceConfigurationError "[AO_ALSA] Fehler bei der Konfiguration des Geräts."
#define MSGTR_AO_ALSA_PcmInSuspendModeTryingResume "[AO_ALSA] Pcm in Suspend-Modus, versuche Wiederherstellung.\n"
#define MSGTR_AO_ALSA_WriteError "[AO_ALSA] Schreibfehler: %s\n"
#define MSGTR_AO_ALSA_TryingToResetSoundcard "[AO_ALSA] Versuche Reset der Soundkarte.\n"
#define MSGTR_AO_ALSA_CannotGetPcmStatus "[AO_ALSA] Kann pcm-Status nicht ermitteln: %s\n"

// ao_plugin.c
#define MSGTR_AO_PLUGIN_InvalidPlugin "[AO PLUGIN] ungültiges Plugin: %s\n"


// ======================= Audiofilter ================================

// af_scaletempo.c
#define MSGTR_AF_ValueOutOfRange MSGTR_VO_ValueOutOfRange

// af_ladspa.c
#define MSGTR_AF_LADSPA_AvailableLabels "verfügbare Label in"
#define MSGTR_AF_LADSPA_WarnNoInputs "WARNUNG! Dieses LADSPA-Plugin hat keine Audio-Inputs.\n  Das eingehende Audiosignal wird verlorengehen."
#define MSGTR_AF_LADSPA_ErrMultiChannel "Multichannel-Plugins (>2) werden (noch) nicht unterstützt.\n  Benutze nur Mono- und Stereo-Plugins."
#define MSGTR_AF_LADSPA_ErrNoOutputs "Dieses LADSPA-Plugin hat keine Audio-Outputs."
#define MSGTR_AF_LADSPA_ErrInOutDiff "Die Anzahl der Audio-Inputs und Audio-Outputs des LADSPA-Plugins sind verschieden."
#define MSGTR_AF_LADSPA_ErrFailedToLoad "Datei konnte nicht geladen werden:"
#define MSGTR_AF_LADSPA_ErrNoDescriptor "Konnte Funktion ladspa_descriptor() in der angegebenen Bibliotheksdatei \nnicht finden."
#define MSGTR_AF_LADSPA_ErrLabelNotFound "Konnte Label in der Plugin-Bibliothek nicht finden."
#define MSGTR_AF_LADSPA_ErrNoSuboptions "Keine Suboptionen angegeben."
#define MSGTR_AF_LADSPA_ErrNoLibFile "Keine Bibliotheksdatei angegeben."
#define MSGTR_AF_LADSPA_ErrNoLabel "Keine Filterlabel angegeben."
#define MSGTR_AF_LADSPA_ErrNotEnoughControls "Nicht genügend Bedienelemente in der Kommandozeile angegeben."
#define MSGTR_AF_LADSPA_ErrControlBelow "%s: Input-Kontrolle #%d ist unterhalb der Grenze von %0.4f.\n"
#define MSGTR_AF_LADSPA_ErrControlAbove "%s: Input-Kontrolle #%d ist überhalb der Grenze von %0.4f.\n"

// format.c

#define MSGTR_AF_FORMAT_UnknownFormat "unbekanntes Format "


// ========================== INPUT =========================================

// joystick.c
#define MSGTR_INPUT_JOYSTICK_Opening "Öffne Joystick-Gerätedatei '%s'.\n"
#define MSGTR_INPUT_JOYSTICK_CantOpen "Konnte Joystick-Gerätedatei '%s' nicht öffnen: %s\n"
#define MSGTR_INPUT_JOYSTICK_ErrReading "Fehler beim Lesen von Joystick-Gerätedatei: %s\n"
#define MSGTR_INPUT_JOYSTICK_LoosingBytes "Joystick: %d Byte Daten verloren.\n"
#define MSGTR_INPUT_JOYSTICK_WarnLostSync "Joystick: Warnung: Initialisierungsereignis, Synchronisierung mit Treiber verloren.\n"
#define MSGTR_INPUT_JOYSTICK_WarnUnknownEvent "Joystick: Warnung: Unbekannter Ereignistyp %d.\n"

// appleir.c
#define MSGTR_INPUT_APPLE_IR_Init "Initialisiere Apple-Fernbedienung auf %s\n"
#define MSGTR_INPUT_APPLE_IR_Detect "Apple-Fernbedienung auf %s erkannt\n"
#define MSGTR_INPUT_APPLE_IR_CantOpen "Kann Gerät für Apple-Fernbedienung nicht öffnen: %s\n"

// input.c
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyCmdFds "Zu viele Kommandodateideskriptoren, konnte Deskriptor %d nicht registrieren.\n"
#define MSGTR_INPUT_INPUT_ErrCantRegister2ManyKeyFds "Zu viele Tastendateidesktriptoren, konnte Deskriptor %d nicht registrieren.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeInt "Kommando %s: Argument %d ist keine Integerzahl.\n"
#define MSGTR_INPUT_INPUT_ErrArgMustBeFloat "Kommando %s: Argument %d ist keine Fließkommazahl.\n"
#define MSGTR_INPUT_INPUT_ErrUnterminatedArg "Kommando %s: Argument %d ist nicht abgeschlossen.\n"
#define MSGTR_INPUT_INPUT_ErrUnknownArg "Unbekanntes Argument %d.\n"
#define MSGTR_INPUT_INPUT_Err2FewArgs "Kommando %s benötigt mindestens %d Argumente, es wurden aber nur %d gefunden.\n"
#define MSGTR_INPUT_INPUT_ErrReadingCmdFd "Fehler beim Lesen von Kommandodateideskriptor %d: %s\n"
#define MSGTR_INPUT_INPUT_ErrCmdBufferFullDroppingContent "Kommandopuffer von Dateideskriptor %d ist voll: Verwerfe Inhalt.\n"
#define MSGTR_INPUT_INPUT_ErrInvalidCommandForKey "Ungültiges Kommando für gebundene Taste '%s'."
#define MSGTR_INPUT_INPUT_ErrSelect "Select gab Fehler zurück: %s\n"
#define MSGTR_INPUT_INPUT_ErrOnKeyInFd "Fehler beim Lesen von Tasteneingabedateideskriptor %d.\n"
#define MSGTR_INPUT_INPUT_ErrDeadKeyOnFd "Fataler Fehler beim Lesen von Tasteneingabedateideskriptor %d.\n"
#define MSGTR_INPUT_INPUT_Err2ManyKeyDowns "Zu viele gleichzeitige Tastendruckereignisse.\n"
#define MSGTR_INPUT_INPUT_ErrOnCmdFd "Fehler beim Lesen von Kommandodateideskriptor %d.\n"
#define MSGTR_INPUT_INPUT_ErrReadingInputConfig "Fehler beim Lesen von Eingabekonfigurationsdatei '%s': %s\n"
#define MSGTR_INPUT_INPUT_ErrUnknownKey "Unbekannte Taste '%s'.\n"
#define MSGTR_INPUT_INPUT_ErrUnfinishedBinding "Konnte Bindung '%s' nicht fertigstellen.\n"
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForKeyName "Der Puffer ist zu klein für diesen Tastennamen: %s\n"
#define MSGTR_INPUT_INPUT_ErrNoCmdForKey "Kein Kommando für Taste '%s' gefunden."
#define MSGTR_INPUT_INPUT_ErrBuffer2SmallForCmd "Der Puffer ist zu klein für Kommando '%s'.\n"
#define MSGTR_INPUT_INPUT_ErrWhyHere "Interner Fehler: Was machen wir hier?\n"
#define MSGTR_INPUT_INPUT_ErrCantInitJoystick "Konnte Joystick nicht initialisieren.\n"
#define MSGTR_INPUT_INPUT_ErrCantStatFile "Stat auf Datei '%s' fehlgeschlagen: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantOpenFile "Konnte Datei '%s' nicht öffnen: %s\n"
#define MSGTR_INPUT_INPUT_ErrCantInitAppleRemote "Konnte Apple Remote Fernbedienung nicht initialisieren.\n"


// ========================== LIBMPDEMUX ===================================

// url.c
#define MSGTR_MPDEMUX_URL_StringAlreadyEscaped "Zeichenkette scheint bereits im URL-Format %c%c1%c2 'escaped' zu sein.\n"

// ai_alsa1x.c
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetSamplerate "Konnte Samplingrate nicht setzen.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetBufferTime "Konnte Pufferzeit nicht setzen.\n"
#define MSGTR_MPDEMUX_AIALSA1X_CannotSetPeriodTime "Konnte Periode nicht setzen.\n"

// ai_alsa.c
#define MSGTR_MPDEMUX_AIALSA_PcmBrokenConfig "Kaputte Konfiguration für diesen PCM-Kanal: Keine Konfiguration verfügbar.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableAccessType "Zugriffstyp nicht verfügbar.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableSampleFmt "Sampleformat nicht verfügbar.\n"
#define MSGTR_MPDEMUX_AIALSA_UnavailableChanCount "Kanalzahl nicht verfügbar - falle auf Standard zurück: %d\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallHWParams "Konnte Hardwareparameter nicht einrichten: %s"
#define MSGTR_MPDEMUX_AIALSA_PeriodEqualsBufferSize "Periode darf nicht gleich der Puffergröße sein (%u == %lu).\n"
#define MSGTR_MPDEMUX_AIALSA_CannotInstallSWParams "Konnte Softwareparameter nicht einrichten:\n"
#define MSGTR_MPDEMUX_AIALSA_ErrorOpeningAudio "Konnte Audio nicht öffnen: %s\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatusError "ALSA-Statusfehler: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUN "ALSA xrun!!! (mindestens %.3f ms lang)\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaStatus "ALSA-Status:\n"
#define MSGTR_MPDEMUX_AIALSA_AlsaXRUNPrepareError "ALSA xrun: Fehler bei Vorbereitung: %s"
#define MSGTR_MPDEMUX_AIALSA_AlsaReadWriteError "ALSA-Ein/Ausgabefehler."

// ai_oss.c
#define MSGTR_MPDEMUX_AIOSS_Unable2SetChanCount "Konnte Kanalzahl nicht setzen: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetStereo "Konnte Stereo nicht setzen: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2Open "Konnte '%s' nicht öffnen: %s\n"
#define MSGTR_MPDEMUX_AIOSS_UnsupportedFmt "Nichtunterstütztes Format\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetAudioFmt "Konnte Tonformat nicht setzen."
#define MSGTR_MPDEMUX_AIOSS_Unable2SetSamplerate "Konnte Samplerate nicht setzen: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2SetTrigger "Konnte Trigger nicht setzen: %d\n"
#define MSGTR_MPDEMUX_AIOSS_Unable2GetBlockSize "Konnte Blockgröße nicht ermitteln!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSizeZero "Blockgröße des Tons ist null, setze auf %d!\n"
#define MSGTR_MPDEMUX_AIOSS_AudioBlockSize2Low "Blockgröße des Tons ist zu klein, setze auf %d!\n"

// asfheader.c
#define MSGTR_MPDEMUX_ASFHDR_HeaderSizeOver1MB \
"FATAL: Dateikopf größer als 1 MB (%d)!\n" \
"Setze dich mit den MPlayer-Autoren in Verbindung und sende oder \n" \
"lade diese Datei hoch.\n"
#define MSGTR_MPDEMUX_ASFHDR_HeaderMallocFailed "%d Bytes konnten nicht für den Kopf reserviert werden.\n"
#define MSGTR_MPDEMUX_ASFHDR_EOFWhileReadingHeader "Dateiende beim lesen des ASF-Headers, kaputte/unvollständige Datei?\n"
#define MSGTR_MPDEMUX_ASFHDR_DVRWantsLibavformat "DVR funktioniert vermutlich nur mit libavformat, versuche -demuxer 35, falls du Probleme hast.\n"
#define MSGTR_MPDEMUX_ASFHDR_NoDataChunkAfterHeader "Auf den Header folgt kein Datenblock!\n"
#define MSGTR_MPDEMUX_ASFHDR_AudioVideoHeaderNotFound "ASF: Kein Ton/Bild-Header gefunden - kaputte Datei?\n"
#define MSGTR_MPDEMUX_ASFHDR_InvalidLengthInASFHeader "Ungültige Längenangabe im ASF-Header!\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMLicenseURL "URL der DRM-Lizenz: %s\n"
#define MSGTR_MPDEMUX_ASFHDR_DRMProtected "Diese Datei wurde mit DRM-Verschlüsselung versehen, MPlayer wird sie nicht abspielen!\n"

// asf_mmst_streaming.c
#define MSGTR_MPDEMUX_MMST_WriteError "Schreibfehler\n"
#define MSGTR_MPDEMUX_MMST_EOFAlert "\nAchtung! Dateiende.\n"
#define MSGTR_MPDEMUX_MMST_PreHeaderReadFailed "Konnte 'pre-header' nicht lesen.\n"
#define MSGTR_MPDEMUX_MMST_InvalidHeaderSize "Ungültige Kopfgröße, gebe auf.\n"
#define MSGTR_MPDEMUX_MMST_HeaderDataReadFailed "Konnte Headerdaten nicht lesen.\n"
#define MSGTR_MPDEMUX_MMST_packet_lenReadFailed "Konnte 'packet_len' nicht lesen.\n"
#define MSGTR_MPDEMUX_MMST_InvalidRTSPPacketSize "Ungültige RTSP-Paketgröße, gebe auf.\n"
#define MSGTR_MPDEMUX_MMST_CmdDataReadFailed "Fehler beim Lesen der Kommandodaten.\n"
#define MSGTR_MPDEMUX_MMST_HeaderObject "Headerobjekt.\n"
#define MSGTR_MPDEMUX_MMST_DataObject "Datenobjekt.\n"
#define MSGTR_MPDEMUX_MMST_FileObjectPacketLen "Dateiobjekt, Paketgröße = %d (%d).\n"
#define MSGTR_MPDEMUX_MMST_StreamObjectStreamID "Datenstromobjekt, ID: %d\n"
#define MSGTR_MPDEMUX_MMST_2ManyStreamID "Zu viele IDs, Datenstrom übersprungen."
#define MSGTR_MPDEMUX_MMST_UnknownObject "unbekanntes Objekt\n"
#define MSGTR_MPDEMUX_MMST_MediaDataReadFailed "Konnte Mediendaten nicht lesen.\n"
#define MSGTR_MPDEMUX_MMST_MissingSignature "fehlende Signatur\n"
#define MSGTR_MPDEMUX_MMST_PatentedTechnologyJoke "Alles fertig. Vielen dank, dass Du eine prorietäre und patentierte Technologie beinhaltende Mediendatei heruntergeladen hast.\n"
#define MSGTR_MPDEMUX_MMST_UnknownCmd "Unbekanntes Kommando %02x\n"
#define MSGTR_MPDEMUX_MMST_GetMediaPacketErr "get_media_packet lieferte Fehler zurück: %s\n"
#define MSGTR_MPDEMUX_MMST_Connected "Verbunden.\n"

// asf_streaming.c
#define MSGTR_MPDEMUX_ASF_StreamChunkSize2Small "Ahhhh, stream_chunk-Größe ist zu klein: %d\n"
#define MSGTR_MPDEMUX_ASF_SizeConfirmMismatch "size_confirm passt nicht!: %d %d\n"
#define MSGTR_MPDEMUX_ASF_WarnDropHeader "Warnung: 'header' verloren ????\n"
#define MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader "Fehler beim Parsen des Blockheaders.\n"
#define MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk "Habe keinen Header als ersten Block bekommen!!!!\n"
#define MSGTR_MPDEMUX_ASF_BufferMallocFailed "Konnte Puffer über %d Bytes nicht reservieren.\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingNetworkStream "Fehler beim Lesen des Datenstroms über das Netzwerk.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunk2Small "Fehler: Block ist zu klein.\n"
#define MSGTR_MPDEMUX_ASF_ErrSubChunkNumberInvalid "Fehler: Unterblocknummer ist ungültig.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallCannotPlay "Bandbreite reicht nicht aus, kann Datei nicht abspielen!\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedAudio "Bandbreite reicht nicht aus, Tonspur deaktiviert.\n"
#define MSGTR_MPDEMUX_ASF_Bandwidth2SmallDeselectedVideo "Bandbreite reicht nicht aus, Videospur deaktiviert.\n"
#define MSGTR_MPDEMUX_ASF_InvalidLenInHeader "Ungültige Längenangabe im ASF-Header!\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunkHeader "Fehler beim Lesen des Blockheaders.\n"
#define MSGTR_MPDEMUX_ASF_ErrChunkBiggerThanPacket "Fehler: Blockgröße > Paketgröße.\n"
#define MSGTR_MPDEMUX_ASF_ErrReadingChunk "Fehler beim Lesen des Blocks.\n"
#define MSGTR_MPDEMUX_ASF_ASFRedirector "=====> ASF Redirector\n"
#define MSGTR_MPDEMUX_ASF_InvalidProxyURL "ungültige Proxy-URL\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamType "unbekannter ASF-Datenstromtyp\n"
#define MSGTR_MPDEMUX_ASF_Failed2ParseHTTPResponse "Konnte HTTP-Antworte nicht parsen.\n"
#define MSGTR_MPDEMUX_ASF_ServerReturn "Server hat %d zurückgegeben: %s\n"
#define MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma "ASF-HTTP-Parser Warnung: Pragma '%s' von %d auf %d Byte abgeschnitten.\n"
#define MSGTR_MPDEMUX_ASF_SocketWriteError "Socketschreibfehler: %s\n"
#define MSGTR_MPDEMUX_ASF_HeaderParseFailed "Konnte Header nicht parsen.\n"
#define MSGTR_MPDEMUX_ASF_NoStreamFound "Kein Stream gefunden.\n"
#define MSGTR_MPDEMUX_ASF_UnknownASFStreamingType "Unbekannter ASF-Streamingtyp\n"
#define MSGTR_MPDEMUX_ASF_InfoStreamASFURL "STREAM_ASF, URL: %s\n"
#define MSGTR_MPDEMUX_ASF_StreamingFailed "Fehlgeschlagen, beende.\n"

// audio_in.c
#define MSGTR_MPDEMUX_AUDIOIN_ErrReadingAudio "\nFehler beim Lesen des Tons: %s\n"
#define MSGTR_MPDEMUX_AUDIOIN_XRUNSomeFramesMayBeLeftOut "Es gab einen 'cross-run', einige Frames könnten fehlen!\n"
#define MSGTR_MPDEMUX_AUDIOIN_ErrFatalCannotRecover "Fataler Fehler, Rettung nicht möglich!\n"
#define MSGTR_MPDEMUX_AUDIOIN_NotEnoughSamples "\nNicht genug Tonsamples!\n"

// aviheader.c
#define MSGTR_MPDEMUX_AVIHDR_EmptyList "** leere Liste?!\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundMovieAt "Film von 0x%X - 0x%X gefunden.\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundBitmapInfoHeader "'bih' gefunden, %u Byte von %d.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPG4V1 "Erstelle Keyframe-Tabelle für MS-mpg4v1-Video neu.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForDIVX3 "Erstelle Keyframe-Tabelle für DIVX3-Video neu.\n"
#define MSGTR_MPDEMUX_AVIHDR_RegeneratingKeyfTableForMPEG4 "Erstelle Keyframe-Tabelle für MPEG4-Video neu.\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundWaveFmt "'wf' gefunden, %d Bytes von %d.\n"
#define MSGTR_MPDEMUX_AVIHDR_FoundAVIV2Header "AVI: dmlh gefunden (size=%d) (total_frames=%d).\n"
#define MSGTR_MPDEMUX_AVIHDR_ReadingIndexBlockChunksForFrames "Lese INDEX-Block, %d Blöcke für %d Frames (fpos=%"PRId64").\n"
#define MSGTR_MPDEMUX_AVIHDR_AdditionalRIFFHdr "zusätzlicher RIFF-Kopf...\n"
#define MSGTR_MPDEMUX_AVIHDR_WarnNotExtendedAVIHdr "** Warnung: Dies ist kein erweiterter AVI-Header...\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenChunk "Kaputter Block?  Blockgröße=%d  (id=%.4s)\n"
#define MSGTR_MPDEMUX_AVIHDR_BuildingODMLidx "AVI: ODML: Erstelle ODML-Index (%d Superindexblöcke).\n"
#define MSGTR_MPDEMUX_AVIHDR_BrokenODMLfile "AVI: ODML: Kaputte (unfertige?) Datei erkannt. Benutze den herkömmlichen Index.\n"
#define MSGTR_MPDEMUX_AVIHDR_CantReadIdxFile "Konnte Index-Datei '%s' nicht lesen: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_NotValidMPidxFile "'%s' ist keine gültige MPlayer-Indexdatei.\n"
#define MSGTR_MPDEMUX_AVIHDR_FailedMallocForIdxFile "Konnte Speicher für Indexdaten von '%s' nicht reservieren.\n"
#define MSGTR_MPDEMUX_AVIHDR_PrematureEOF "vorzeitiges Ende der Indexdatei %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileLoaded "Indexdatei geladen: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_GeneratingIdx "Erzeuge Index: %3lu %s     \r"
#define MSGTR_MPDEMUX_AVIHDR_IdxGeneratedForHowManyChunks "AVI: Erzeuge Indextabelle für %d Blöcke!\n"
#define MSGTR_MPDEMUX_AVIHDR_Failed2WriteIdxFile "Konnte Indexdatei '%s' nicht schreiben: %s\n"
#define MSGTR_MPDEMUX_AVIHDR_IdxFileSaved "Indexdatei '%s' gespeichert.\n"

// cache2.c
#define MSGTR_MPDEMUX_CACHE2_NonCacheableStream "\rDieser Datenstrom lässt sich nicht zwischenspeichern.\n"
#define MSGTR_MPDEMUX_CACHE2_ReadFileposDiffers "!!!Unterschied in read_filepos!!! Melde diesen Fehler...\n"

// cdda.c
#define MSGTR_MPDEMUX_CDDA_CantOpenCDDADevice "Kann CDDA-Gerät nicht öffnen.\n"
#define MSGTR_MPDEMUX_CDDA_CantOpenDisc "Kann CD nicht öffnen.\n"
#define MSGTR_MPDEMUX_CDDA_AudioCDFoundWithNTracks "Musik-CD mit %ld Titeln gefunden.\n"

// cddb.c
#define MSGTR_MPDEMUX_CDDB_FailedToReadTOC "Konnte Inhaltsverzeichnis der CD nicht lesen.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToOpenDevice "Konnte Gerät '%s' nicht öffnen.\n"
#define MSGTR_MPDEMUX_CDDB_NotAValidURL "ungültige URL\n"
#define MSGTR_MPDEMUX_CDDB_FailedToSendHTTPRequest "Konnte die HTTP-Anfrage nicht senden.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToReadHTTPResponse "Konnte die HTTP-Antwort nicht lesen.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorNOTFOUND "Nicht gefunden.\n"
#define MSGTR_MPDEMUX_CDDB_HTTPErrorUnknown "unbekannter Fehlercode\n"
#define MSGTR_MPDEMUX_CDDB_NoCacheFound "Kein Cache gefunden.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenRead "Konnte die XMCD-Datei nicht komplett lesen.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToCreateDirectory "Konnte Verzeichnis '%s' nicht erstellen.\n"
#define MSGTR_MPDEMUX_CDDB_NotAllXMCDFileHasBeenWritten "Konnte die XMCD-Datei nicht komplett schreiben.\n"
#define MSGTR_MPDEMUX_CDDB_InvalidXMCDDatabaseReturned "XMCD-Datenbankdatei ist ungültig.\n"
#define MSGTR_MPDEMUX_CDDB_UnexpectedFIXME "unerwartetes FIXME\n"
#define MSGTR_MPDEMUX_CDDB_UnhandledCode "unbehandelter Code\n"
#define MSGTR_MPDEMUX_CDDB_UnableToFindEOL "Konnte Zeilenendmarkierung nicht finden.\n"
#define MSGTR_MPDEMUX_CDDB_ParseOKFoundAlbumTitle "Album '%s' gefunden.\n"
#define MSGTR_MPDEMUX_CDDB_AlbumNotFound "Album nicht gefunden.\n"
#define MSGTR_MPDEMUX_CDDB_ServerReturnsCommandSyntaxErr "Fehlercode des Servers: Kommandosyntaxfehler.\n"
#define MSGTR_MPDEMUX_CDDB_NoSitesInfoAvailable "Keine Sites-Informationen verfügbar.\n"
#define MSGTR_MPDEMUX_CDDB_FailedToGetProtocolLevel "Konnte aktuelle Protokollebene nicht ermitteln.\n"
#define MSGTR_MPDEMUX_CDDB_NoCDInDrive "Keine CD im Laufwerk.\n"

// cue_read.c
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedCuefileLine "[bincue] Unerwartete Zeile in der Cue-Datei: %s\n"
#define MSGTR_MPDEMUX_CUEREAD_BinFilenameTested "[bincue] Zugehörige Bin-Datei unter dem Namen '%s' nicht gefunden.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotFindBinFile "[bincue] Konnte Bin-Datei nicht finden. Gebe auf.\n"
#define MSGTR_MPDEMUX_CUEREAD_UsingBinFile "[bincue] Benutze Bin-Datei %s.\n"
#define MSGTR_MPDEMUX_CUEREAD_UnknownModeForBinfile "[bincue] Unbekannter Typ der Bin-Datei. Dies sollte nicht passieren, breche ab.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotOpenCueFile "[bincue] Fehler beim Öffnen von %s.\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrReadingFromCueFile "[bincue] Fehler beim Lesen von '%s'.\n"
#define MSGTR_MPDEMUX_CUEREAD_ErrGettingBinFileSize "[bincue] Fehler beim Ermitteln der Größe der Bin-Datei.\n"
#define MSGTR_MPDEMUX_CUEREAD_InfoTrackFormat "Titel %02d: Format=%d  %02d:%02d:%02d\n"
#define MSGTR_MPDEMUX_CUEREAD_UnexpectedBinFileEOF "[bincue] Unerwartetes Ende der Bin-Datei.\n"
#define MSGTR_MPDEMUX_CUEREAD_CannotReadNBytesOfPayload "[bincue] Konnte Block der Größe %dByte nicht lesen.\n"
#define MSGTR_MPDEMUX_CUEREAD_CueStreamInfo_FilenameTrackTracksavail "CUE stream_open: Dateiname='%s', Titel=%d, verfügbare Titel: %d -> %d\n"

// network.c
#define MSGTR_MPDEMUX_NW_UnknownAF "Unbekannte Adressfamilie %d\n"
#define MSGTR_MPDEMUX_NW_ResolvingHostForAF "Löse %s auf für %s...\n"
#define MSGTR_MPDEMUX_NW_CantResolv "Konnte Namen für %s nicht auflösen: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectingToServer "Verbinde mit Server %s[%s]:%d ...\n"
#define MSGTR_MPDEMUX_NW_CantConnect2Server "Konnte nicht mit %s zu Server verbinden.\n"
#define MSGTR_MPDEMUX_NW_SelectFailed "Select fehlgeschlagen.\n"
#define MSGTR_MPDEMUX_NW_ConnTimeout "Zeitüberschreitung bei Verbindung.\n"
#define MSGTR_MPDEMUX_NW_GetSockOptFailed "getsockopt fehlgeschlagen: %s\n"
#define MSGTR_MPDEMUX_NW_ConnectError "Verbindung fehlgeschlagen: %s\n"
#define MSGTR_MPDEMUX_NW_InvalidProxySettingTryingWithout "Ungültige Proxyeinstellung... Versuche es ohne Proxy.\n"
#define MSGTR_MPDEMUX_NW_CantResolvTryingWithoutProxy "Konnte Hostnamen nicht für AF_INET auflösen. Versuche es ohne Proxy.\n"
#define MSGTR_MPDEMUX_NW_ErrSendingHTTPRequest "Fehler beim Senden der HTTP-Anforderung: Anfrage wurde unvollständig gesendet.\n"
#define MSGTR_MPDEMUX_NW_ReadFailed "Lesen der HTTP-Antwort fehlgeschlagen.\n"
#define MSGTR_MPDEMUX_NW_Read0CouldBeEOF "http_read_response: 0 Bytes gelesen (z.B. EOF).\n"
#define MSGTR_MPDEMUX_NW_AuthFailed \
"Authentifizierung fehlgeschlagen. Benutze bitte die Optionen -user und \n" \
"-passwd um deinen Benutzernamen/Passwort für eine Liste von URLs anzugeben, \n" \
"oder eine URL der Form: http://benutzer:passwort@hostname/datei\n"
#define MSGTR_MPDEMUX_NW_AuthRequiredFor "Authentifizierung erforderlich für Bereich '%s'.\n"
#define MSGTR_MPDEMUX_NW_AuthRequired "Authentifizierung erforderlich.\n"
#define MSGTR_MPDEMUX_NW_NoPasswdProvidedTryingBlank "Kein Passwort angegeben, versuche leeres Passwort.\n"
#define MSGTR_MPDEMUX_NW_ErrServerReturned "Server liefert Fehler %d: %s\n"
#define MSGTR_MPDEMUX_NW_CacheSizeSetTo "Cache-Größe auf %d KByte gesetzt.\n"

// demux_audio.c
#define MSGTR_MPDEMUX_AUDIO_UnknownFormat "Audiodemuxer: unbekanntes Format %d.\n"

// demux_demuxers.c
#define MSGTR_MPDEMUX_DEMUXERS_FillBufferError "fill_buffer-Fehler: schlechter Demuxer: weder vd noch ad oder sd.\n"

// demux_mkv.c
#define MSGTR_MPDEMUX_MKV_ZlibInitializationFailed "[mkv] Initialisierung von zlib fehlgeschlagen.\n"
#define MSGTR_MPDEMUX_MKV_ZlibDecompressionFailed "[mkv] Dekomprimierung mit zlib fehlgeschlagen.\n"
#define MSGTR_MPDEMUX_MKV_LzoInitializationFailed "[mkv] Initialisierung von lzo fehlgeschlagen..\n"
#define MSGTR_MPDEMUX_MKV_LzoDecompressionFailed "[mkv] Dekomprimierung mit lzo fehlgeschlagen.\n"
#define MSGTR_MPDEMUX_MKV_TrackEncrypted "[mkv] Track Nummer %u wurde verschlüsselt, und Entschlüsselung wurde noch\n[mkv] nicht implementiert. Überspringe Track.\n"
#define MSGTR_MPDEMUX_MKV_UnknownContentEncoding "[mkv] Unbekannter Encoding-Typ des Inhalts von Track %u. Überspringe Track.\n"
#define MSGTR_MPDEMUX_MKV_UnknownCompression "[mkv] Track %u wurde mit einem unbekannten oder nicht unterstützten\n[mkv] Kompressionsalgorithmus (%u) komprimiert. Überspringe Track.\n"
#define MSGTR_MPDEMUX_MKV_ZlibCompressionUnsupported "[mkv] Track %u wurde mit zlib komprimiert, mplayer wurde jedoch ohne\n[mkv] Unterstützung für Komprimierung mit zlib compiliert. Überspringe Track.\n"
#define MSGTR_MPDEMUX_MKV_TrackIDName "[mkv] Track-ID %u: %s (%s) \"%s\", %s\n"
#define MSGTR_MPDEMUX_MKV_TrackID "[mkv] Track-ID %u: %s (%s), %s\n"
#define MSGTR_MPDEMUX_MKV_UnknownCodecID "[mkv] Unbekannte/nicht unterstützte CodecID (%s) oder fehlende/fehlerhafte\n[mkv] CodecPrivate-Daten (Track %u).\n"
#define MSGTR_MPDEMUX_MKV_FlacTrackDoesNotContainValidHeaders "[mkv] FLAC-Track enthält keine gültigen Header.\n"
#define MSGTR_MPDEMUX_MKV_UnknownAudioCodec "[mkv]  Unbekannte/nicht unterstützte Audiocodec-ID ID '%s' für Track %u\n[mkv] oder fehlende/fehlerhafte private codec Daten.\n"
#define MSGTR_MPDEMUX_MKV_SubtitleTypeNotSupported "[mkv] Untertitel-Typ '%s' wird nicht unterstützt.\n"
#define MSGTR_MPDEMUX_MKV_WillPlayVideoTrack "[mkv] Werde Video-Track %u abspielen.\n"
#define MSGTR_MPDEMUX_MKV_NoVideoTrackFound "[mkv] Kein Video-Track gefunden/gewünscht.\n"
#define MSGTR_MPDEMUX_MKV_NoAudioTrackFound "[mkv] Kein Audio-Track gefunden/gewünscht.\n"
#define MSGTR_MPDEMUX_MKV_WillDisplaySubtitleTrack "[mkv] Werde Untertitel-Track %u anzeigen.\n"
#define MSGTR_MPDEMUX_MKV_NoBlockDurationForSubtitleTrackFound "[mkv] Warnung: BlockDuration für Untertitel-Track nicht gefunden.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublines "[mkv] Warnung: Zu viele anzuzeigende Sublines, überspringe.\n"
#define MSGTR_MPDEMUX_MKV_TooManySublinesSkippingAfterFirst "\n[mkv] Warnung: Zu viele anzuzeigende Sublines, überspringe nach den ersten %i.\n"

// demux_nuv.c
#define MSGTR_MPDEMUX_NUV_NoVideoBlocksInFile "Datei enthält keine Videoblöcke.\n"

// demux_xmms.c
#define MSGTR_MPDEMUX_XMMS_FoundPlugin "Plugin gefunden: %s (%s).\n"
#define MSGTR_MPDEMUX_XMMS_ClosingPlugin "Plugin geschlossen: %s.\n"
#define MSGTR_MPDEMUX_XMMS_WaitForStart "Warte auf die Wiedergabe des XMMS-Plugins von '%s'...\n"


// ========================== LIBMENU ===================================

// common
#define MSGTR_LIBMENU_NoEntryFoundInTheMenuDefinition "[MENU] Kein Eintrag in der Menüdefinition gefunden.\n"

// libmenu/menu.c
#define MSGTR_LIBMENU_SyntaxErrorAtLine "[MENU] Syntaxfehler bei Zeile: %d\n"
#define MSGTR_LIBMENU_MenuDefinitionsNeedANameAttrib "[MENU] Menüdefinitionen benötigen ein Namensattribut (Zeile %d).\n"
#define MSGTR_LIBMENU_BadAttrib "[MENU] schlechtes Attribut %s=%s in Menütyp '%s' in Zeile %d\n"
#define MSGTR_LIBMENU_UnknownMenuType "[MENU] unbekannter Menütyp '%s' in Zeile %d\n"
#define MSGTR_LIBMENU_CantOpenConfigFile "[MENU] Kann Menü-Konfigurationsdatei nicht öffnen: %s\n"
#define MSGTR_LIBMENU_ConfigFileIsTooBig "[MENU] Konfigurationsdatei ist zu groß (> %d KB)\n"
#define MSGTR_LIBMENU_ConfigFileIsEmpty "[MENU] Konfigurationsdatei ist leer.\n"
#define MSGTR_LIBMENU_MenuNotFound "[MENU] Menü %s nicht gefunden.\n"
#define MSGTR_LIBMENU_MenuInitFailed "[MENU] Menü '%s': Initialisierung fehlgeschlagen.\n"
#define MSGTR_LIBMENU_UnsupportedOutformat "[MENU] Nichtunterstütztes Ausgabeformat!!!!\n"

// libmenu/menu_cmdlist.c
#define MSGTR_LIBMENU_ListMenuEntryDefinitionsNeedAName "[MENU] Definitionen für Listenmenüeinträge benötigen einen Namen (Zeile %d).\n"
#define MSGTR_LIBMENU_ListMenuNeedsAnArgument "[MENU] Listenmenü benötigt einen Parameter.\n"

// libmenu/menu_console.c
#define MSGTR_LIBMENU_WaitPidError "[MENU] Waitpid-Fehler: %s.\n"
#define MSGTR_LIBMENU_SelectError "[MENU] Fehler bei der Auswahl.\n"
#define MSGTR_LIBMENU_ReadErrorOnChildFD "[MENU] Lesefehler bei child-Dateideskriptor: %s.\n"
#define MSGTR_LIBMENU_ConsoleRun "[MENU] Konsolenaufruf: %s ...\n"
#define MSGTR_LIBMENU_AChildIsAlreadyRunning "[MENU] Es läuft schon ein Child.\n"
#define MSGTR_LIBMENU_ForkFailed "[MENU] Fork fehlgeschlagen!!!\n"
#define MSGTR_LIBMENU_WriteError "[MENU] Schreibfehler\n"

// libmenu/menu_filesel.c
#define MSGTR_LIBMENU_OpendirError "[MENU] opendir-Fehler: %s\n"
#define MSGTR_LIBMENU_ReallocError "[MENU] realloc-Fehler: %s\n"
#define MSGTR_LIBMENU_MallocError "[MENU] Speicherreservierungsfehler: %s\n"
#define MSGTR_LIBMENU_ReaddirError "[MENU] readdir-Fehler: %s\n"
#define MSGTR_LIBMENU_CantOpenDirectory "[MENU] Kann Verzeichnis %s nicht öffnen.\n"

// libmenu/menu_param.c
#define MSGTR_LIBMENU_SubmenuDefinitionNeedAMenuAttribut "[MENU] Definition des Untermenüs benötigt ein 'menu'-Attribut.\n"
#define MSGTR_LIBMENU_InvalidProperty "[MENU] Ungültige Eigenschaft '%s' im Einstellungsmenüeintrag. (Zeile %d).\n"
#define MSGTR_LIBMENU_PrefMenuEntryDefinitionsNeed "[MENU] Definitionen für Konfigurationsmenüeinträge benötigen ein\n       gültiges 'property'- oder 'txt'Attribut (Zeile %d).\n"
#define MSGTR_LIBMENU_PrefMenuNeedsAnArgument "[MENU] Konfigurationsmenü benötigt einen Parameter.\n"

// libmenu/menu_pt.c
#define MSGTR_LIBMENU_CantfindTheTargetItem "[MENU] Kann Ziel-Item nicht finden ????\n"
#define MSGTR_LIBMENU_FailedToBuildCommand "[MENU] Konnte folgenden Befehl nicht erstellen: %s.\n"

// libmenu/menu_txt.c
#define MSGTR_LIBMENU_MenuTxtNeedATxtFileName "[MENU] Textmenü benötigt einen Textdateinamen (Parameterdatei).\n"
#define MSGTR_LIBMENU_MenuTxtCantOpen "[MENU] Kann %s nicht öffnen.\n"
#define MSGTR_LIBMENU_WarningTooLongLineSplitting "[MENU] Warnung, Zeile zu lang. Zerlege sie.\n"
#define MSGTR_LIBMENU_ParsedLines "[MENU] %d Zeilen eingelesen.\n"

// libmenu/vf_menu.c
#define MSGTR_LIBMENU_UnknownMenuCommand "[MENU] Unbekannter Befehl: '%s'.\n"
#define MSGTR_LIBMENU_FailedToOpenMenu "[MENU] Konnte folgendes Menü nicht öffnen: '%s'.\n"


// ========================== LIBMPCODECS ===================================

// libmpcodecs/ad_libdv.c
#define MSGTR_MPCODECS_AudioFramesizeDiffers "[AD_LIBDV] Warnung! Audio-Framegröße weicht ab! read=%d  hdr=%d.\n"

// libmpcodecs/vd_dmo.c vd_dshow.c vd_vfw.c
#define MSGTR_MPCODECS_CouldntAllocateImageForCinepakCodec "[VD_DMO] Konnte Bild für cinepak-Codec nicht allozieren.\n"

// libmpcodecs/vd_ffmpeg.c
#define MSGTR_MPCODECS_XVMCAcceleratedCodec "[VD_FFMPEG] XVMC-beschleunigter Codec.\n"
#define MSGTR_MPCODECS_ArithmeticMeanOfQP "[VD_FFMPEG] Arithmetisches Mittel des QP: %2.4f, Harmonisches Mittel des QP: %2.4f\n"
#define MSGTR_MPCODECS_DRIFailure "[VD_FFMPEG] DRI-Versagen.\n"
#define MSGTR_MPCODECS_CouldntAllocateImageForCodec "[VD_FFMPEG] Konnte Bild für Codec nicht allozieren.\n"
#define MSGTR_MPCODECS_XVMCAcceleratedMPEG2 "[VD_FFMPEG] XVMC-beschleunigtes MPEG-2.\n"
#define MSGTR_MPCODECS_TryingPixfmt "[VD_FFMPEG] Versuche pixfmt=%d.\n"
#define MSGTR_MPCODECS_McGetBufferShouldWorkOnlyWithXVMC "[VD_FFMPEG] Der mc_get_buffer sollte nur mit XVMC-Beschleunigung funktionieren!!"
#define MSGTR_MPCODECS_UnexpectedInitVoError "[VD_FFMPEG] Unerwarteter init_vo-Fehler.\n"
#define MSGTR_MPCODECS_UnrecoverableErrorRenderBuffersNotTaken "[VD_FFMPEG] Nicht zu behebender Fehler, Puffer zum Zeichnen nicht genommen.\n"
#define MSGTR_MPCODECS_OnlyBuffersAllocatedByVoXvmcAllowed "[VD_FFMPEG] Nur von vo_xvmc allozierte Puffer erlaubt.\n"

// libmpcodecs/ve_lavc.c
#define MSGTR_MPCODECS_HighQualityEncodingSelected "[VE_LAVC] Encodierung in hoher Qualität ausgewählt (keine Echtzeit)!\n"
#define MSGTR_MPCODECS_UsingConstantQscale "[VE_LAVC] Benutze konstanten Wert für qscale = %f (VBR).\n"

// libmpcodecs/ve_raw.c
#define MSGTR_MPCODECS_OutputWithFourccNotSupported "[VE_RAW] Raw-Ausgabe mit FourCC [%x] nicht unterstützt!\n"
#define MSGTR_MPCODECS_NoVfwCodecSpecified "[VE_RAW] Benötigten VfW-Codec nicht angegeben!!\n"

// libmpcodecs/vf_crop.c
#define MSGTR_MPCODECS_CropBadPositionWidthHeight \
"[CROP] Schlechte Position/Breite/Höhe - abgeschnittener Bereich\n" \
"       außerhalb des Originals!\n"

// libmpcodecs/vf_cropdetect.c
#define MSGTR_MPCODECS_CropArea "[CROP] Schnittbereich: X: %d..%d  Y: %d..%d  (-vf crop=%d:%d:%d:%d).\n"

// libmpcodecs/vf_format.c, vf_palette.c, vf_noformat.c
#define MSGTR_MPCODECS_UnknownFormatName "[VF_FORMAT] Unbekannter Formatname: '%s'.\n"

// libmpcodecs/vf_framestep.c vf_noformat.c vf_palette.c vf_tile.c
#define MSGTR_MPCODECS_ErrorParsingArgument "[VF_FRAMESTEP] Fehler beim Einlesen des Parameters.\n"

// libmpcodecs/ve_vfw.c
#define MSGTR_MPCODECS_CompressorType "Kompressionstyp: %.4lx\n"
#define MSGTR_MPCODECS_CompressorSubtype "Kompressionssubtyp: %.4lx\n"
#define MSGTR_MPCODECS_CompressorFlags "Kompressions-Flags: %lu, Version %lu, ICM-Version: %lu\n"
#define MSGTR_MPCODECS_Flags "Flags:"
#define MSGTR_MPCODECS_Quality " Qualität"

// libmpcodecs/vf_expand.c
#define MSGTR_MPCODECS_FullDRNotPossible "Vollständiges DR nicht möglich, versuche stattdessen SLICES!\n"
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupportSlices "WARNUNG! Nächster Filter unterstützt SLICES nicht, bereite dich auf sig11 vor...\n"
#define MSGTR_MPCODECS_FunWhydowegetNULL "Wieso bekommen wir NULL??\n"

// libmpcodecs/vf_test.c, vf_yuy2.c, vf_yvu9.c
#define MSGTR_MPCODECS_WarnNextFilterDoesntSupport "%s vom nächsten Filter/vo nicht unterstützt :(\n"


// ================================== LIBVO ====================================

// mga_common.c
#define MSGTR_LIBVO_MGA_ErrorInConfigIoctl "[MGA] Fehler im ioctl von mga_vid_config (falsche mga_vid.o-Version?)"
#define MSGTR_LIBVO_MGA_CouldNotGetLumaValuesFromTheKernelModule "[MGA] Konnte die Helligkeitswerte vom Kernelmodul nicht ermitteln!\n"
#define MSGTR_LIBVO_MGA_CouldNotSetLumaValuesFromTheKernelModule "[MGA] Konnte die Helligkeitswerte vom Kernelmodul nicht setzen!\n"
#define MSGTR_LIBVO_MGA_ScreenWidthHeightUnknown "[MGA] Bildschirmbreite/-höhe unbekannt!\n"
#define MSGTR_LIBVO_MGA_InvalidOutputFormat "[MGA] ungültiges Ausgabeformat %0X\n"
#define MSGTR_LIBVO_MGA_IncompatibleDriverVersion "[MGA] Die Treiberversion von mga_vid ist mit dieser MPlayer-Version nicht kompatibel!\n"
#define MSGTR_LIBVO_MGA_CouldntOpen "[MGA] Konnte %s nicht öffnen\n"
#define MSGTR_LIBVO_MGA_ResolutionTooHigh "[MGA] Auflösung der Quelle ist in mindestens einer Dimension größer als 1023x1023.\n[MGA] Bitte skaliere in Software oder verwende -lavdopts lowres=1.\n"
#define MSGTR_LIBVO_MGA_mgavidVersionMismatch "[MGA] Unterschiedliche Versionen des mga_vid-Treibers des Kernels (%u) und MPlayer (%u).\n"


// libvo/vesa_lvo.c
#define MSGTR_LIBVO_VESA_ThisBranchIsNoLongerSupported "[VESA_LVO] Dieser Zweig wird nicht länger unterstützt.\n[VESA_LVO] Benutze stattdessen bitte -vo vesa:vidix.\n"
#define MSGTR_LIBVO_VESA_CouldntOpen "[VESA_LVO] Konnte nicht öffnen: '%s'\n"
#define MSGTR_LIBVO_VESA_InvalidOutputFormat "[VESA_LVI] Ungültiges Ausgabeformat: %s(%0X)\n"
#define MSGTR_LIBVO_VESA_IncompatibleDriverVersion "[VESA_LVO] Die fb_vid-Treiberversion ist mit dieser MPlayer-Version nicht kompatibel!\n"

// libvo/vo_3dfx.c
#define MSGTR_LIBVO_3DFX_Only16BppSupported "[VO_3DFX] Nur 16bpp unterstützt!"
#define MSGTR_LIBVO_3DFX_VisualIdIs "[VO_3DFX] Visuelle ID ist  %lx.\n"
#define MSGTR_LIBVO_3DFX_UnableToOpenDevice "[VO_3DFX] Kann /dev/3dfx nicht öffnen.\n"
#define MSGTR_LIBVO_3DFX_Error "[VO_3DFX] Fehler: %d.\n"
#define MSGTR_LIBVO_3DFX_CouldntMapMemoryArea "[VO_3DFX] Konnte 3dfx-Speicherbereiche nicht abbilden: %p,%p,%d.\n"
#define MSGTR_LIBVO_3DFX_DisplayInitialized "[VO_3DFX] Initialisiert: %p.\n"
#define MSGTR_LIBVO_3DFX_UnknownSubdevice "[VO_3DFX] Unbekanntes Subgerät: %s.\n"

// libvo/aspect.c
#define MSGTR_LIBVO_ASPECT_NoSuitableNewResFound "[ASPECT] Warnung: Keine passende neue Auflösung gefunden!\n"
#define MSGTR_LIBVO_ASPECT_NoNewSizeFoundThatFitsIntoRes "[ASPECT] Fehler: Keine neue Größe gefunden, die in die Auflösung passt!\n"

// libvo/vo_dxr3.c
#define MSGTR_LIBVO_DXR3_UnableToLoadNewSPUPalette "[VO_DXR3] Kann neue SPU-Palette nicht laden!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetPlaymode "[VO_DXR3] Kann Wiedergabemodus nicht setzen!\n"
#define MSGTR_LIBVO_DXR3_UnableToSetSubpictureMode "[VO_DXR3] Kann Subbild-Modus nicht setzen!\n"
#define MSGTR_LIBVO_DXR3_UnableToGetTVNorm "[VO_DXR3] Kann TV-Norm nicht ermitteln!\n"
#define MSGTR_LIBVO_DXR3_AutoSelectedTVNormByFrameRate "[VO_DXR3] TV-Norm automatisch durch Framerate gewählt: "
#define MSGTR_LIBVO_DXR3_UnableToSetTVNorm "[VO_DXR3] Kann TV-Norm nicht setzen!\n"
#define MSGTR_LIBVO_DXR3_SettingUpForNTSC "[VO_DXR3] Mache Einstellungen für NTSC.\n"
#define MSGTR_LIBVO_DXR3_SettingUpForPALSECAM "[VO_DXR3] Mache Einstellungen für PAL/SECAM.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo43 "[VO_DXR3] Setze Aspekt auf 4:3.\n"
#define MSGTR_LIBVO_DXR3_SettingAspectRatioTo169 "[VO_DXR3] Setze Aspekt auf 16:9.\n"
#define MSGTR_LIBVO_DXR3_OutOfMemory "[VO_DXR3] kein Speicher mehr\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateKeycolor "[VO_DXR3] Kann Schlüsselfarbe nicht ermitteln!\n"
#define MSGTR_LIBVO_DXR3_UnableToAllocateExactKeycolor "[VO_DXR3] Kann Schlüsselfarbe nicht ermitteln, benutze nächstgelegenen Treffer (0x%lx).\n"
#define MSGTR_LIBVO_DXR3_Uninitializing "[VO_DXR3] Deinitialisierung.\n"
#define MSGTR_LIBVO_DXR3_FailedRestoringTVNorm "[VO_DXR3] Konnte TV-Norm nicht wiederherstellen!\n"
#define MSGTR_LIBVO_DXR3_EnablingPrebuffering "[VO_DXR3] Prebuffering aktiviert.\n"
#define MSGTR_LIBVO_DXR3_UsingNewSyncEngine "[VO_DXR3] Benutze neue Sync-Maschine.\n"
#define MSGTR_LIBVO_DXR3_UsingOverlay "[VO_DXR3] Benutze Overlay.\n"
#define MSGTR_LIBVO_DXR3_ErrorYouNeedToCompileMplayerWithX11 "[VO_DXR3] Fehler: Overlay verlangt Compilierung mit installierten X11-Headern/-Libs.\n"
#define MSGTR_LIBVO_DXR3_WillSetTVNormTo "[VO_DXR3] Werde TV-Norm setzen auf: "
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALPAL60 "automatische Anpassung zu Video-Framerate (PAL/PAL-60)"
#define MSGTR_LIBVO_DXR3_AutoAdjustToMovieFrameRatePALNTSC "automatische Anpassung zu Video-Framerate (PAL/NTSC)"
#define MSGTR_LIBVO_DXR3_UseCurrentNorm "Benutze aktuelle Norm."
#define MSGTR_LIBVO_DXR3_UseUnknownNormSuppliedCurrentNorm "Unbekannte Norm geliefert. Benutze aktuelle Norm."
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTrying "[VO_DXR3] Fehler beim Öffnen von %s zum Schreiben, versuche statt dessen /dev/em8300.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingMV "[VO_DXR3] Fehler beim Öffnen von %s zum Schreiben, versuche statt dessen /dev/em8300_mv.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWell \
"[VO_DXR3] Fehler auch beim Öffnen von /dev/em8300 zum Schreiben! \n" \
"          Steige aus.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellMV \
"[VO_DXR3] Fehler auch beim Öffnen von /dev/em8300_mv zum Schreiben! \n" \
"          Steige aus.\n"
#define MSGTR_LIBVO_DXR3_Opened "[VO_DXR3] Geöffnet: %s.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingTryingSP \
"[VO_DXR3] Fehler beim Öffnen von %s zum Schreiben, versuche statt \n" \
"          dessen /dev/em8300_sp.\n"
#define MSGTR_LIBVO_DXR3_ErrorOpeningForWritingAsWellSP \
"[VO_DXR3] Fehler auch beim Öffnen von /dev/em8300_sp zum Schreiben! \n" \
"          Steige aus.\n"
#define MSGTR_LIBVO_DXR3_UnableToOpenDisplayDuringHackSetup "[VO_DXR3] Konnte Bildschirm während gehackten Overlaysetup nicht öffnen!\n"
#define MSGTR_LIBVO_DXR3_UnableToInitX11 "[VO_DXR3] Konnte X11 nicht initialisieren!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayAttribute "[VO_DXR3] Konnte Overlayattribut nicht setzen.\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayScreen "[VO_DXR3] Konnte Overlaybildschirm nicht setzen!\nBeende.\n"
#define MSGTR_LIBVO_DXR3_FailedEnablingOverlay "[VO_DXR3] Konnte Overlay nicht aktivieren!\nBeende.\n"
#define MSGTR_LIBVO_DXR3_FailedResizingOverlayWindow "[VO_DXR3] Konnte Größe des Overlayfensters nicht ändern!\n"
#define MSGTR_LIBVO_DXR3_FailedSettingOverlayBcs "[VO_DXR3] Konnter Overlay-bcs nicht setzen!\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayYOffsetValues "[VO_DXR3] Konnte Y-Offset-Wert des Overlays nicht ermitteln!\nBeende.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXOffsetValues "[VO_DXR3] Konnte X-Offset-Wert des Overlays nicht ermitteln!\nBeende.\n"
#define MSGTR_LIBVO_DXR3_FailedGettingOverlayXScaleCorrection "[VO_DXR3] Konnte X-Skalierungskorrektur des Overlays nicht ermitteln!\nBeende.\n"
#define MSGTR_LIBVO_DXR3_YOffset "[VO_DXR3] Y-Offset: %d.\n"
#define MSGTR_LIBVO_DXR3_XOffset "[VO_DXR3] X-Offset: %d.\n"
#define MSGTR_LIBVO_DXR3_XCorrection "[VO_DXR3] X-Koorektur: %d.\n"
#define MSGTR_LIBVO_DXR3_FailedSetSignalMix "[VO_DXR3] Konnte Signal-Mix nicht setzen!\n"

// libvo/font_load_ft.c
#define MSGTR_LIBVO_FONT_LOAD_FT_NewFaceFailed "New_Face fehlgeschlagen. Vielleicht ist der Pfad zu den Schriften falsch?\nBitte stelle eine Text-Schriftdatei zur Verfügung (~/.mplayer/subfont.ttf).\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_NewMemoryFaceFailed "New_Memory_Face fehlgeschlagen..\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_SubFaceFailed "subtitle font: load_sub_face fehlgeschlagen.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_SubFontCharsetFailed "subtitle font: prepare_charset fehlgeschlagen.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_CannotPrepareSubtitleFont "Kann Untertitelschriftdatei nicht vorbereiten.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_CannotPrepareOSDFont "Kann OSD-Schrift nicht vorbereiten.\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_CannotGenerateTables "Kann Tabellen nicht generieren..\n"
#define MSGTR_LIBVO_FONT_LOAD_FT_DoneFreeTypeFailed "FT_Done_FreeType fehlgeschlagen.\n"

// libvo/vo_mga.c
#define MSGTR_LIBVO_MGA_AspectResized "[VO_MGA] aspect(): Größe geändert auf %dx%d.\n"
#define MSGTR_LIBVO_MGA_Uninit "[VO] Deinitialisierung!\n"

// libvo/vo_null.c
#define MSGTR_LIBVO_NULL_UnknownSubdevice "[VO_NULL] Unbekanntes Subgerät: %s.\n"

// libvo/vo_png.c
#define MSGTR_LIBVO_PNG_Warning1 "[VO_PNG] Warnung: Kompressionslevel auf 0 gesetzt, Kompression deaktiviert!\n"
#define MSGTR_LIBVO_PNG_Warning2 "[VO_PNG] Info: Benutze -vo png:z=<n>, um Kompressionslevel von 0 bis 9 zu setzen.\n"
#define MSGTR_LIBVO_PNG_Warning3 "[VO_PNG] Info: (0 = keine Kompression, 1 = am schnellsten, wenigsten - 9 beste, langsamste Kompression)\n"
#define MSGTR_LIBVO_PNG_ErrorOpeningForWriting "\n[VO_PNG] Fehler beim Öffnen von '%s' zum Schreiben!\n"
#define MSGTR_LIBVO_PNG_ErrorInCreatePng "[VO_PNG] Fehler in create_png.\n"

// libvo/vo_sdl.c
#define MSGTR_LIBVO_SDL_CouldntGetAnyAcceptableSDLModeForOutput "[VO_SDL] Konnte keinen akzeptierbaren SDL-Modus für die Ausgabe ermitteln.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailed "[VO_SDL] set_video_mode: SDL_SetVideoMode fehlgeschlagen: %s.\n"
#define MSGTR_LIBVO_SDL_SetVideoModeFailedFull "[VO_SDL] Set_fullmode: SDL_SetVideoMode fehlgeschlagen: %s.\n"
#define MSGTR_LIBVO_SDL_MappingI420ToIYUV "[VO_SDL] Abbildung von I420 auf IYUV.\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormat "[VO_SDL] Nichtunterstütztes Bildformat (0x%X).\n"
#define MSGTR_LIBVO_SDL_InfoPleaseUseVmOrZoom "[VO_SDL] Info - bitte benutze -vm oder -zoom, um zur besten Auflösung zu wechseln.\n"
#define MSGTR_LIBVO_SDL_FailedToSetVideoMode "[VO_SDL] Konnte Videomodus nicht setzen: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateAYUVOverlay "[VO_SDL] Konnte kein YUV-Overlay erstellen: %s.\n"
#define MSGTR_LIBVO_SDL_CouldntCreateARGBSurface "[VO_SDL] Konnte keine RGB-Oberfläche erstellen: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDepthColorspaceConversion \
"[VO_SDL] Benutze Tiefen-/Farbraumkonvertierung, dies wird Dinge \n" \
"         verlangsamen (%ibpp -> %ibpp).\n"
#define MSGTR_LIBVO_SDL_UnsupportedImageFormatInDrawslice \
"[VO_SDL] Nichtunterstütztes Bildformat in draw_slice, kontaktiere \n" \
"         MPlayer-Entwickler!\n"
#define MSGTR_LIBVO_SDL_BlitFailed "[VO_SDL] Blit fehlgeschlagen: %s.\n"
#define MSGTR_LIBVO_SDL_InitializationFailed "[VO_SDL] SDL-Initialisierung fehlgeschlagen: %s.\n"
#define MSGTR_LIBVO_SDL_UsingDriver "[VO_SDL] Benutze Treiber: %s.\n"

// libvo/vobsub_vidix.c
#define MSGTR_LIBVO_SUB_VIDIX_CantStartPlayback "[VO_SUB_VIDIX] Kann Wiedergabe nicht starten: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantStopPlayback "[VO_SUB_VIDIX] Kann Wiedergabe nicht anhalten: %s\n"
#define MSGTR_LIBVO_SUB_VIDIX_InterleavedUvForYuv410pNotSupported "[VO_SUB_VIDIX] Interleaved UV für YUV410P nicht unterstützt.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawsliceWasCalled "[VO_SUB_VIDIX] Dummy vidix_draw_slice() wurde aufgerufen.\n"
#define MSGTR_LIBVO_SUB_VIDIX_DummyVidixdrawframeWasCalled "[VO_SUB_VIDIX] Dummy vidix_draw_frame() wurde aufgerufen.\n"
#define MSGTR_LIBVO_SUB_VIDIX_UnsupportedFourccForThisVidixDriver "[VO_SUB_VIDIX] Nichtunterstützter FourCC für folgenden VIDIX-Treiber: %x (%s).\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedResolution "[VO_SUB_VIDIX] Videoserver hat nichtunterstützte Auflösung (%dx%d), unterstützt: %dx%d-%dx%d.\n"
#define MSGTR_LIBVO_SUB_VIDIX_VideoServerHasUnsupportedColorDepth "[VO_SUB_VIDIX] Videoserver hat von vidix nichtunterstützte Farbtiefe (%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantUpscaleImage "[VO_SUB_VIDIX] VIDIX-Treiber kann Bild nicht hochskalieren (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_DriverCantDownscaleImage "[VO_SUB_VIDIX] VIDIX-Treiber kann Bild nicht herunterskalieren (%d%d -> %d%d).\n"
#define MSGTR_LIBVO_SUB_VIDIX_CantConfigurePlayback "[VO_SUB_VIDIX] Kann Wiedergabe nicht konfigurieren: %s.\n"
#define MSGTR_LIBVO_SUB_VIDIX_YouHaveWrongVersionOfVidixLibrary "[VO_SUB_VIDIX] Du hast die falsche Version der VIDIX-Bibliothek.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntFindWorkingVidixDriver "[VO_SUB_VIDIX] Konnte keinen funktionierenden VIDIX-Treiber finden.\n"
#define MSGTR_LIBVO_SUB_VIDIX_CouldntGetCapability "[VO_SUB_VIDIX] Konnte Fähigkeit nicht ermitteln: %s.\n"

// libvo/vo_svga.c
#define MSGTR_LIBVO_SVGA_ForcedVidmodeNotAvailable "[VO_SVGA] Erzwungener vid_mode %d (%s) nicht verfügbar.\n"
#define MSGTR_LIBVO_SVGA_ForcedVidmodeTooSmall "[VO_SVGA] Erzwungener vid_mode %d (%s) zu klein.\n"
#define MSGTR_LIBVO_SVGA_Vidmode "[VO_SVGA] Vid_mode: %d, %dx%d %dbpp.\n"
#define MSGTR_LIBVO_SVGA_VgasetmodeFailed "[VO_SVGA] Vga_setmode(%d) fehlgeschlagen.\n"
#define MSGTR_LIBVO_SVGA_VideoModeIsLinearAndMemcpyCouldBeUsed \
"[VO_SVGA] Videomodus ist linear, und memcpy könnte für Bildtransfer \n" \
"          benutzt werden.\n"
#define MSGTR_LIBVO_SVGA_VideoModeHasHardwareAcceleration \
"[VO_SVGA] Videomodus besitzt Hardwarebeschleunigung, und put_image könnte \n" \
"          benutzt werden.\n"
#define MSGTR_LIBVO_SVGA_IfItWorksForYouIWouldLikeToKnow "[VO_SVGA] Wenn es für dich funktioniert, würde ich das gerne wissen.\n[VO_SVGA] (sende Log mit `mplayer test.avi -v -v -v -v &> svga.log`). Danke!\n"
#define MSGTR_LIBVO_SVGA_VideoModeHas "[VO_SVGA] Videomodus hat %d Seite(n).\n"
#define MSGTR_LIBVO_SVGA_CenteringImageStartAt "[VO_SVGA] Zentriere Bild. Starte bei (%d,%d)\n"
#define MSGTR_LIBVO_SVGA_UsingVidix "[VO_SVGA] Benutze VIDIX. b=%i h=%i  mb=%i mh=%i\n"

// libvo/vo_tdfxfb.c
#define MSGTR_LIBVO_TDFXFB_CantOpen "[VO_TDFXFB] Konnte %s nicht öffnen: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetFscreenInfo "[VO_TDFXFB] Problem mit FBITGET_FSCREENINFO ioctl: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ProblemWithFbitgetVscreenInfo "[VO_TDFXFB] Problem mit FBITGET_VSCREENINFO ioctl: %s.\n"
#define MSGTR_LIBVO_TDFXFB_ThisDriverOnlySupports "[VO_TDFXFB] Dieser Treiber unterstützt nur 3Dfx Banshee, Voodoo3 und Voodoo 5.\n"
#define MSGTR_LIBVO_TDFXFB_OutputIsNotSupported "[VO_TDFXFB] Ausgabe mit %d bpp wird nicht unterstützt.\n"
#define MSGTR_LIBVO_TDFXFB_CouldntMapMemoryAreas "[VO_TDFXFB] Konnte Speicherbereiche nicht abbilden: %s.\n"
#define MSGTR_LIBVO_TDFXFB_BppOutputIsNotSupported \
"[VO_TDFXFB] Ausgabe mit %d bpp wird nicht unterstützt \n" \
"            (Dies sollte niemals passiert sein).\n"
#define MSGTR_LIBVO_TDFXFB_SomethingIsWrongWithControl "[VO_TDFXFB] Ächz! Etwas läuft falsch mit control().\n"
#define MSGTR_LIBVO_TDFXFB_NotEnoughVideoMemoryToPlay "[VO_TDFXFB] Nicht genug Speicher, um diesen Film abzuspielen. Probiere eine geringere Auflösung.\n"
#define MSGTR_LIBVO_TDFXFB_ScreenIs "[VO_TDFXFB] Bildschirm ist %dx%d bei %d bpp, Input ist %dx%d bei %d bpp, Norm ist %dx%d.\n"

// libvo/vo_tdfx_vid.c
#define MSGTR_LIBVO_TDFXVID_Move "[VO_TDXVID] Verschiebe %d(%d) x %d => %d.\n"
#define MSGTR_LIBVO_TDFXVID_AGPMoveFailedToClearTheScreen "[VO_TDFXVID] AGP-Verschiebung konnte Bildschirm nicht löschen.\n"
#define MSGTR_LIBVO_TDFXVID_BlitFailed "[VO_TDFXVID] Blit fehlgeschlagen.\n"
#define MSGTR_LIBVO_TDFXVID_NonNativeOverlayFormatNeedConversion "[VO_TDFXVID] Nicht-natives Overlayformat benötigt Konvertierung.\n"
#define MSGTR_LIBVO_TDFXVID_UnsupportedInputFormat "[VO_TDFXVID] Nichtunterstütztes Eingabeformat 0x%x.\n"
#define MSGTR_LIBVO_TDFXVID_OverlaySetupFailed "[VO_TDFXVID] Overlaysetup fehlgeschlagen.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOnFailed "[VO_TDFXVID] Aktiverung des Overlays fehlgeschlagen.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayReady "[VO_TDFXVID] Overlay fertig: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_TextureBlitReady "[VO_TDFXVID] Textur-Blit fertig: %d(%d) x %d @ %d => %d(%d) x %d @ %d.\n"
#define MSGTR_LIBVO_TDFXVID_OverlayOffFailed "[VO_TDFXVID] Deaktivierung des Overlay fehlgeschlagen\n"
#define MSGTR_LIBVO_TDFXVID_CantOpen "[VO_TDFXVID] Kann %s nicht öffnen: %s.\n"
#define MSGTR_LIBVO_TDFXVID_CantGetCurrentCfg "[VO_TDFXVID] Kann aktuelle Konfiguration nicht ermitteln: %s.\n"
#define MSGTR_LIBVO_TDFXVID_MemmapFailed "[VO_TDFXVID] Memmap fehlgeschlagen!!!!!\n"
#define MSGTR_LIBVO_TDFXVID_GetImageTodo "Ermittle Bild-Todo.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailed "[VO_TDFXVID] AGP-Verschiebung fehlgeschlagen.\n"
#define MSGTR_LIBVO_TDFXVID_SetYuvFailed "[VO_TDFXVID] YUV setzen fehlgeschlagen.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnYPlane "[VO_TDFXVID] AGP-Verschiebung bei Y-Ebene fehlgeschlagen.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnUPlane "[VO_TDFXVID] AGP-Verschiebung bei U-Ebene fehlgeschlagen.\n"
#define MSGTR_LIBVO_TDFXVID_AgpMoveFailedOnVPlane "[VO_TDFXVID] AGP-Verschiebung bei V-Ebene fehlgeschlagen.\n"
#define MSGTR_LIBVO_TDFXVID_UnknownFormat "[VO_TDFXVID] unbekanntes Format: 0x%x.\n"

// libvo/vo_tga.c
#define MSGTR_LIBVO_TGA_UnknownSubdevice "[VO_TGA] Unbekanntes Subgerät: %s.\n"

// libvo/vo_vesa.c
#define MSGTR_LIBVO_VESA_FatalErrorOccurred "[VO_VESA] Fataler Fehler aufgetreten! Kann nicht fortfahren.\n"
#define MSGTR_LIBVO_VESA_UnknownSubdevice "[VO_VESA] unbekanntes Subgerät: '%s'.\n"
#define MSGTR_LIBVO_VESA_YouHaveTooLittleVideoMemory "[VO_VESA] Du hast zu wenig Videospeicher für diesen Modus:\n[VO_VESA] Benötigt: %08lX vorhanden: %08lX.\n"
#define MSGTR_LIBVO_VESA_YouHaveToSpecifyTheCapabilitiesOfTheMonitor "[VO_VESA] Du musst die Fähigkeiten deines Monitors angeben.\n[VO_VESA] Ändere Bildwiederholrate nicht.\n"
#define MSGTR_LIBVO_VESA_UnableToFitTheMode "[VO_VESA] Der Modus passt nicht zu den Beschränkungen des Monitors.\n[VO_VESA] Ändere Bildwiederholrate nicht.\n"
#define MSGTR_LIBVO_VESA_DetectedInternalFatalError "[VO_VESA] Internen fatalen Fehler erkannt: init wird vor preinit aufgerufen.\n"
#define MSGTR_LIBVO_VESA_SwitchFlipIsNotSupported "[VO_VESA] Die Option -flip wird nicht unterstützt.\n"
#define MSGTR_LIBVO_VESA_PossibleReasonNoVbe2BiosFound "[VO_VESA] Möglicher Grund: Kein VBE2 BIOS gefunden.\n"
#define MSGTR_LIBVO_VESA_FoundVesaVbeBiosVersion "[VO_VESA] VESA VBE BIOS Version %x.%x Revision gefunden: %x.\n"
#define MSGTR_LIBVO_VESA_VideoMemory "[VO_VESA] Videospeicher: %u Kb.\n"
#define MSGTR_LIBVO_VESA_Capabilites "[VO_VESA] VESA-Fähigkeiten: %s %s %s %s %s.\n"
#define MSGTR_LIBVO_VESA_BelowWillBePrintedOemInfo "[VO_VESA] !!! OEM-Informationen werden unten ausgegeben. !!!\n"
#define MSGTR_LIBVO_VESA_YouShouldSee5OemRelatedLines "[VO_VESA] Du solltest unten 5 OEM-zugehörige Zeilen sehen; Wenn nicht, ist dein vm86 kaputt.\n"
#define MSGTR_LIBVO_VESA_OemInfo "[VO_VESA] OEM-Info: %s.\n"
#define MSGTR_LIBVO_VESA_OemRevision "[VO_VESA] OEM-Revision: %x.\n"
#define MSGTR_LIBVO_VESA_OemVendor "[VO_VESA] OEM-Händler: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductName "[VO_VESA] OEM-Produktname: %s.\n"
#define MSGTR_LIBVO_VESA_OemProductRev "[VO_VESA] OEM-Produktrevision: %s.\n"
#define MSGTR_LIBVO_VESA_Hint \
"[VO_VESA] Tip: Für funktionierendes TV-Out solltest du das TV- \n" \
"[VO_VESA] Verbindungskabel vor dem Booten eingesteckt haben, da das \n" \
"[VO_VESA] VESA-BIOS nur während POST initialisiert.\n"
#define MSGTR_LIBVO_VESA_UsingVesaMode "[VO_VESA] Benutze VESA-Modus (%u) = %x [%ux%u@%u]\n"
#define MSGTR_LIBVO_VESA_CantInitializeSwscaler "[VO_VESA] Kann Softwareskalierer nicht initialisieren.\n"
#define MSGTR_LIBVO_VESA_CantUseDga "[VO_VESA] Kann DGA nicht verwenden. Erzwinge bank switching Modus. :(\n"
#define MSGTR_LIBVO_VESA_UsingDga "[VO_VESA] Benutze DGA (physikalische Ressourcen: %08lXh, %08lXh)"
#define MSGTR_LIBVO_VESA_CantUseDoubleBuffering "[VO_VESA] Kann Doublebuffering nicht verwenden: nicht genügend Speicher.\n"
#define MSGTR_LIBVO_VESA_CantFindNeitherDga "[VO_VESA] Kann weder DGA- noch verschiebbaren Fensterrahmen finden.\n"
#define MSGTR_LIBVO_VESA_YouveForcedDga "[VO_VESA] Du hast DGA erzwungen. Beende\n"
#define MSGTR_LIBVO_VESA_CantFindValidWindowAddress "[VO_VESA] Kann keine gültige Fensteradresse finden.\n"
#define MSGTR_LIBVO_VESA_UsingBankSwitchingMode "[VO_VESA] Verwende bank switching Modus (physikalische Ressourcen: %08lXh, %08lXh).\n"
#define MSGTR_LIBVO_VESA_CantAllocateTemporaryBuffer "[VO_VESA] Kann temporären Buffer nicht allozieren.\n"
#define MSGTR_LIBVO_VESA_SorryUnsupportedMode "[VO_VESA] Sorry, nichtunterstützter Modus -- probiere -x 640 -zoom.\n"
#define MSGTR_LIBVO_VESA_OhYouReallyHavePictureOnTv "[VO_VESA] Oh, du hast wirklich ein Bild auf dem TV!\n"
#define MSGTR_LIBVO_VESA_CantInitialozeLinuxVideoOverlay "[VO_VESA] Kann Linux Video Overlay nicht initialisieren.\n"
#define MSGTR_LIBVO_VESA_UsingVideoOverlay "[VO_VESA] Benutze Video-Overlay: %s.\n"
#define MSGTR_LIBVO_VESA_CantInitializeVidixDriver "[VO_VESA] Kann VIDIX-Treiber nicht initialisieren.\n"
#define MSGTR_LIBVO_VESA_UsingVidix "[VO_VESA] Benutze VIDIX.\n"
#define MSGTR_LIBVO_VESA_CantFindModeFor "[VO_VESA] Kann keinen Modus finden für: %ux%u@%u.\n"
#define MSGTR_LIBVO_VESA_InitializationComplete "[VO_VESA] VESA-Initialisierung abgeschlossen.\n"

// libvo/vo_x11.c
#define MSGTR_LIBVO_X11_DrawFrameCalled "[VO_X11] draw_frame() aufgerufen!!!!!!\n"

// libvo/vo_xv.c
#define MSGTR_LIBVO_XV_DrawFrameCalled "[VO_XV] draw_frame() aufgerufen!!!!!!\n"
#define MSGTR_LIBVO_XV_SharedMemoryNotSupported "[VO_XV] Shared Memory nicht unterstützt\nGreife auf normalen Xv zurück.\n"
#define MSGTR_LIBVO_XV_XvNotSupportedByX11 "[VO_XV] Sorry, Xv von dieser X11-Version/diesem Treiber nicht unterstützt\n[VO_XV] *********** Probiere mit  -vo x11  oder  -vo sdl  ***************\n"
#define MSGTR_LIBVO_XV_XvQueryAdaptorsFailed "[VO_XV] XvQueryAdaptors fehlgeschlagen.\n"
#define MSGTR_LIBVO_XV_InvalidPortParameter "[VO_XV] Ungültiger Port-Parameter, überschreibe mit Port 0.\n"
#define MSGTR_LIBVO_XV_CouldNotGrabPort "[VO_XV] Konnte Port %i nicht ergreifen.\n"
#define MSGTR_LIBVO_XV_CouldNotFindFreePort "[VO_XV] Konnte keinen freien Xvideo-Port finden - vielleicht verwendet ihn\n"\
"[VO_XV] schon ein anderer Prozess. Schließe alle Video-Applikationen und\n"\
"[VO_XV] versuche es erneut. Wenn das nicht hilft, siehe 'mplayer -vo help'\n"\
"[VO_XV] für andere (nicht-xv) Videoausgabetreiber.\n"
#define MSGTR_LIBVO_XV_NoXvideoSupport "[VO_XV] Es scheint keine Xvideo-Unterstützung für deine Grafikkarte verfügbar\n"\
"[VO_XV] zu sein. Starte 'xvinfo', um Xv-Support zu verifizieren, und lies"\
"[VO_XV] DOCS/HTML/de/video.html#xv!\n"\
"[VO_XV] Siehe 'mplayer -vo help' für andere (nicht-xv) Videoausgabetreiber.\n"\
"[VO_XV] Probiere -vo x11.\n"

// loader/ldt_keeper.c
#define MSGTR_LOADER_DYLD_Warning "WARNUNG: Versuche, DLL-Codecs zu verwenden, die Umgebungsvariable\n         DYLD_BIND_AT_LAUNCH ist aber nicht gesetzt. Dies führt wahrscheinlich\n         zu einem Absturz.\n"

// stream/stream_radio.c
#define MSGTR_RADIO_ChannelNamesDetected "[Radio] Radiokanalnamen erkannt.\n"
#define MSGTR_RADIO_FreqRange "[Radio] Erlaubter Frequenzbereich ist %.2f-%.2f MHz.\n"
#define MSGTR_RADIO_WrongFreqForChannel "[Radio] Falsche Frequenz für Kanal %s\n"
#define MSGTR_RADIO_WrongChannelNumberFloat "[Radio] Falsche Kanalnummer: %.2f\n"
#define MSGTR_RADIO_WrongChannelNumberInt "[Radio] Falsche Kanalnummer: %d\n"
#define MSGTR_RADIO_WrongChannelName "[Radio] Falscher Kanalname: %s\n"
#define MSGTR_RADIO_FreqParameterDetected "[Radio] Radiofrequenzparameter erkannt.\n"
#define MSGTR_RADIO_DoneParsingChannels "[Radio] Einlesen der Kanäle erledigt.\n"
#define MSGTR_RADIO_GetTunerFailed "[Radio] Warnung: ioctl \"get tuner\" fehlgeschlagen: %s. Setze frac auf %d.\n"
#define MSGTR_RADIO_NotRadioDevice "[Radio] %s ist kein Radiogerät!\n"
#define MSGTR_RADIO_TunerCapLowYes "[Radio] Empfänger ist niedrig:ja frac=%d\n"
#define MSGTR_RADIO_TunerCapLowNo "[Radio] Empfänger ist niedrig:nein frac=%d\n"
#define MSGTR_RADIO_SetFreqFailed "[Radio] ioctl \"set frequency 0x%x (%.2f)\" fehlgeschlagen: %s\n"
#define MSGTR_RADIO_GetFreqFailed "[Radio] ioctl \"get frequency\" fehlgeschlagen: %s\n"
#define MSGTR_RADIO_SetMuteFailed "[Radio] ioctl \"set mute\" fehlgeschlagen: %s\n"
#define MSGTR_RADIO_QueryControlFailed "[Radio] ioctl \"query control\" fehlgeschlagen: %s\n"
#define MSGTR_RADIO_GetVolumeFailed "[Radio] ioctl \"get volume\" fehlgeschlagen: %s\n"
#define MSGTR_RADIO_SetVolumeFailed "[Radio] ioctl \"set volume\" fehlgeschlagen: %s\n"
#define MSGTR_RADIO_DroppingFrame "\n[Radio] zu schlecht - Audio-Frame ausgelassen (%d Bytes)!\n"
#define MSGTR_RADIO_BufferEmpty "[Radio] grab_audio_frame: Puffer leer, warte auf %d Daten-Bytes.\n"
#define MSGTR_RADIO_AudioInitFailed "[Radio] audio_in_init fehlgeschlagen: %s\n"
#define MSGTR_RADIO_AudioBuffer "[Radio] Audio-Capture - Puffer=%d Bytes (Block=%d Bytes).\n"
#define MSGTR_RADIO_AllocateBufferFailed "[Radio] Kann Audiopuffer nicht allozieren (Block=%d,buf=%d): %s\n"
#define MSGTR_RADIO_CurrentFreq "[Radio] Momentane Frequenz: %.2f\n"
#define MSGTR_RADIO_SelectedChannel "[Radio] Gewählter Kanal: %d - %s (Freq: %.2f)\n"
#define MSGTR_RADIO_ChangeChannelNoChannelList "[Radio] Kann Kanal nicht wechseln: Keine Kanalliste angegeben.\n"
#define MSGTR_RADIO_UnableOpenDevice "[Radio] Kann '%s' nicht öffnen: %s\n"
#define MSGTR_RADIO_RadioDevice "[Radio] Radio fd: %d, %s\n"
#define MSGTR_RADIO_InitFracFailed "[Radio] init_frac fehlgeschlagen.\n"
#define MSGTR_RADIO_WrongFreq "[Radio] Falsche Frequenz: %.2f\n"
#define MSGTR_RADIO_UsingFreq "[Radio] Verwende Frequenz: %.2f\n"
#define MSGTR_RADIO_AudioInInitFailed "[Radio] audio_in_init fehlgeschlagen.\n"
#define MSGTR_RADIO_BufferString "[Radio] %s: im Puffer=%d ausgelassen=%d\n"
#define MSGTR_RADIO_AudioInSetupFailed "[Radio] Aufruf audio_in_setup fehlgeschlagen: %s\n"
#define MSGTR_RADIO_CaptureStarting "[Radio] Starte Capture-Kram.\n"
#define MSGTR_RADIO_ClearBufferFailed "[Radio] Klärung des Puffers fehlgeschlagen: %s\n"
#define MSGTR_RADIO_StreamEnableCacheFailed "[Radio] Aufruf zu stream_enable_cache fehlgeschlagen: %s\n"
#define MSGTR_RADIO_DriverUnknownStr "[Radio] Unbekannter Treibername: %s\n"
#define MSGTR_RADIO_DriverV4L2 "[Radio] Benutze V4Lv2-Radioschnittstelle.\n"
#define MSGTR_RADIO_DriverV4L "[Radio] Benutze V4Lv1-Radioschnittstelle.\n"
#define MSGTR_RADIO_DriverBSDBT848 "[Radio] Benutze *BSD BT848-Radioschnittstelle.\n"
#define MSGTR_RADIO_AvailableDrivers "[Radio] Verfügbare Treiber: "


// ================================== LIBASS ====================================

// ass_bitmap.c
#define MSGTR_LIBASS_FT_Glyph_To_BitmapError "[ass] Fehler %d in FT_Glyph_To_Bitmap\n"
#define MSGTR_LIBASS_UnsupportedPixelMode "[ass] Nichtunterstützter Pixelmodus: %d\n"
#define MSGTR_LIBASS_GlyphBBoxTooLarge "[ass] Umgebender Rahmen für Glyph zu groß: %dx%dpx\n"

// ass.c
#define MSGTR_LIBASS_NoStyleNamedXFoundUsingY "[ass] [%p] Warnung: kein Style mit Namen '%s' gefunden, verwende '%s'\n"
#define MSGTR_LIBASS_BadTimestamp "[ass] schlechter Zeitstempel\n"
#define MSGTR_LIBASS_BadEncodedDataSize "[ass] schlecht encodierte Datengröße\n"
#define MSGTR_LIBASS_FontLineTooLong "[ass] Schriftzeile zu lang: %d, %s\n"
#define MSGTR_LIBASS_EventFormatHeaderMissing "[ass] fehlender Ereignisformat-Header\n"
#define MSGTR_LIBASS_ErrorOpeningIconvDescriptor "[ass] Fehler beim Öffnen des iconv-Deskriptors.\n"
#define MSGTR_LIBASS_ErrorRecodingFile "[ass] Fehler bei Recodierung der Datei.\n"
#define MSGTR_LIBASS_FopenFailed "[ass] ass_read_file(%s): fopen fehlgeschlagen\n"
#define MSGTR_LIBASS_FseekFailed "[ass] ass_read_file(%s): fseek fehlgeschlagen\n"
#define MSGTR_LIBASS_RefusingToLoadSubtitlesLargerThan10M "[ass] ass_read_file(%s): Laden von Untertiteln größer als 10M verweigert.\n"
#define MSGTR_LIBASS_ReadFailed "Lesen fehlgeschlagen, %d: %s\n"
#define MSGTR_LIBASS_AddedSubtitleFileMemory "[ass] Hinzugefügte Untertiteldatei: <Speicher> (%d Styles, %d Ereignisse)\n"
#define MSGTR_LIBASS_AddedSubtitleFileFname "[ass]  Hinzugefügte Untertiteldatei: %s (%d Styles, %d Ereignisse)\n"
#define MSGTR_LIBASS_FailedToCreateDirectory "[ass] Fehler beim Erstellen des Verzeichnisses %s\n"
#define MSGTR_LIBASS_NotADirectory "[ass] Kein Verzeichnis: %s\n"

// ass_cache.c
#define MSGTR_LIBASS_TooManyFonts "[ass] Zu viele Schriften\n"
#define MSGTR_LIBASS_ErrorOpeningFont "[ass] Fehler beim Öffnen der Schrift: %s, %d\n"

// ass_fontconfig.c
#define MSGTR_LIBASS_SelectedFontFamilyIsNotTheRequestedOne "[ass] fontconfig: Die gewählte Schrift ist nicht die angeforderte: '%s' != '%s'\n"
#define MSGTR_LIBASS_UsingDefaultFontFamily "[ass] fontconfig_select: Verwende Standardschriftfamilie: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingDefaultFont "[ass] fontconfig_select: Verwende Standardschrift: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_UsingArialFontFamily "[ass] fontconfig_select: Verwende 'Arial'-Schriftfamilie: (%s, %d, %d) -> %s, %d\n"
#define MSGTR_LIBASS_FcInitLoadConfigAndFontsFailed "[ass] FcInitLoadConfigAndFonts fehlgeschlagen.\n"
#define MSGTR_LIBASS_UpdatingFontCache "[ass] Aktualisiere Schriftzwischenspeicher.\n"
#define MSGTR_LIBASS_BetaVersionsOfFontconfigAreNotSupported "[ass] Beta-Versionen von fontconfig werden nicht unterstützt.\n[ass] Bevor Du Bugs berichtest, mache ein Update.\n"
#define MSGTR_LIBASS_FcStrSetAddFailed "[ass] FcStrSetAdd fehlgeschlagen.\n"
#define MSGTR_LIBASS_FcDirScanFailed "[ass] FcDirScan fehlgeschlagen.\n"
#define MSGTR_LIBASS_FcDirSave "[ass] FcDirSave fehlgeschlagen.\n"
#define MSGTR_LIBASS_FcConfigAppFontAddDirFailed "[ass] FcConfigAppFontAddDir fehlgeschlagen\n"
#define MSGTR_LIBASS_FontconfigDisabledDefaultFontWillBeUsed "[ass] Fontconfig deaktiviert, es wird nur die Standardschrift verwendet werden.\n"
#define MSGTR_LIBASS_FunctionCallFailed "[ass] %s fehlgeschlagen\n"

// ass_render.c
#define MSGTR_LIBASS_NeitherPlayResXNorPlayResYDefined "[ass] Weder PlayResX noch PlayResY definiert. Nehme 384x288 an.\n"
#define MSGTR_LIBASS_PlayResYUndefinedSettingY "[ass] PlayResY undefiniert, setze %d.\n"
#define MSGTR_LIBASS_PlayResXUndefinedSettingX "[ass] PlayResX undefiniert, setze %d.\n"
#define MSGTR_LIBASS_FT_Init_FreeTypeFailed "[ass] FT_Init_FreeType fehlgeschlagen.\n"
#define MSGTR_LIBASS_Init "[ass] Initialisierung\n"
#define MSGTR_LIBASS_InitFailed "[ass] Initialisierung fehlgeschlagen.\n"
#define MSGTR_LIBASS_BadCommand "[ass] Schlechter Befehl: %c%c\n"
#define MSGTR_LIBASS_ErrorLoadingGlyph  "[ass] Fehler beim Laden der Glyphe.\n"
#define MSGTR_LIBASS_FT_Glyph_Stroke_Error "[ass] Fehler %d bei FT_Glyph_Stroke\n"
#define MSGTR_LIBASS_UnknownEffectType_InternalError "[ass] Unbekannter Effekttyp (interner Fehler)\n"
#define MSGTR_LIBASS_NoStyleFound "[ass] Kein Style gefunden!\n"
#define MSGTR_LIBASS_EmptyEvent "[ass] Leeres Ereignis!\n"
#define MSGTR_LIBASS_MAX_GLYPHS_Reached "[ass] MAX_GLYPHS erreicht: Ereignis %d, Start = %llu, Dauer = %llu\n Text = %s\n"
#define MSGTR_LIBASS_EventHeightHasChanged "[ass] Warnung! Ereignishöhe geändert!  \n"

// ass_font.c
#define MSGTR_LIBASS_GlyphNotFoundReselectingFont "[ass] Glyph 0x%X nicht gefunden, wähle weitere Schrift für (%s, %d, %d)\n"
#define MSGTR_LIBASS_GlyphNotFound "[ass] Glyph 0x%X in Schrift für (%s, %d, %d) nicht gefunden\n"
#define MSGTR_LIBASS_ErrorOpeningMemoryFont "[ass] Fehler beim Öffnen der Schrift im Speicher: %s\n"
#define MSGTR_LIBASS_NoCharmaps "[ass] Schriftfläche ohne Zeichentabelle\n"
#define MSGTR_LIBASS_NoCharmapAutodetected "[ass] keine Zeichentabelle automatisch erkennt, probiere die erste\n"

//tv.c
#define MSGTR_TV_BogusNormParameter "tv.c: norm_from_string(%s): Falscher Norm-Parameter, setze %s.\n"
#define MSGTR_TV_NoVideoInputPresent "Fehler: Kein Videoinput vorhanden!\n"
#define MSGTR_TV_UnknownImageFormat ""\
"=====================================================================\n"\
" WARNUNG: UNGETESTETES ODER UNBEKANNTES BILDFORMAT ANGEFORDERT (0x%x)\n"\
" Dies kann zu fehlerhafter Wiedergabe oder einem Absturz führen!\n"\
" Fehlerberichte werden ignoriert werden! Du solltest erneut versuchen\n"\
" mit YV12 (was der Standardfarbraum ist) und die Dokumentation lesen!\n"\
"=====================================================================\n"
#define MSGTR_TV_SelectedNormId "Gewählte Norm-ID: %d\n"
#define MSGTR_TV_SelectedNorm "Gewählte Norm : %s\n"
#define MSGTR_TV_CannotSetNorm "Fehler: Kann Norm nicht setzen!\n"
#define MSGTR_TV_MJP_WidthHeight "  MJP: Breite %d Höhe %d\n"
#define MSGTR_TV_UnableToSetWidth "Kann angeforderte Breite nicht setzen: %d\n"
#define MSGTR_TV_UnableToSetHeight "Kann angeforderte Höhe nicht setzen: %d\n"
#define MSGTR_TV_NoTuner "Gewählter Input hat keinen Tuner!\n"
#define MSGTR_TV_UnableFindChanlist "Kann gewählte Kanalliste nicht finden! (%s)\n"
#define MSGTR_TV_SelectedChanlist "Gewählte Kanalliste: %s (enthält %d Kanäle)\n"
#define MSGTR_TV_ChannelFreqParamConflict "Du kannst Frequenz und Kanal nicht gleichzeitig setzen!\n"
#define MSGTR_TV_ChannelNamesDetected "TV-Kanalnamen erkannt.\n"
#define MSGTR_TV_NoFreqForChannel "Konnte Frequenz für Kanal %s nicht finden (%s)\n"
#define MSGTR_TV_SelectedChannel3 "Gewählter Kanal: %s - %s (Freq: %.3f)\n"
#define MSGTR_TV_SelectedChannel2 "Gewählter Kanal: %s (Freq: %.3f)\n"
#define MSGTR_TV_SelectedFrequency "Gewählte Frequenz: %lu (%.3f)\n"
#define MSGTR_TV_RequestedChannel "Angeforderter Kanal: %s\n"
#define MSGTR_TV_UnsupportedAudioType "Audiotyp '%s (%x)' nicht unterstützt!\n"
#define MSGTR_TV_AudioFormat "  TV-Audio: %d Kanäle, %d Bits, %d Hz\n"
#define MSGTR_TV_AvailableDrivers "Verfügbare Treiber:\n"
#define MSGTR_TV_DriverInfo "Ausgewählter Treiber: %s\n Name: %s\n Autor: %s\n Kommentar: %s\n"
#define MSGTR_TV_NoSuchDriver "Kein Treiber: %s\n"
#define MSGTR_TV_DriverAutoDetectionFailed "Automatische Erkennung des TV-Treibers fehlgeschlagen.\n"
#define MSGTR_TV_UnknownColorOption "Unbekannte Farboption (%d) angegeben!\n"
#define MSGTR_TV_CurrentFrequency "Momentane Frequenz: %lu (%.3f)\n"
#define MSGTR_TV_NoTeletext "Kein Videotext"
#define MSGTR_TV_Bt848IoctlFailed "tvi_bsdbt848: Aufruf von %s ioctl fehlgeschlagen. Fehler: %s\n"
#define MSGTR_TV_Bt848InvalidAudioRate "tvi_bsdbt848: Ungültige Audiorate. Fehler: %s\n"
#define MSGTR_TV_Bt848ErrorOpeningBktrDev "tvi_bsdbt848: Kann bktr-Gerät nicht öffnen. Fehler: %s\n"
#define MSGTR_TV_Bt848ErrorOpeningTunerDev "tvi_bsdbt848: Kann Tuner-Gerät nicht öffnen. Fehler: %s\n"
#define MSGTR_TV_Bt848ErrorOpeningDspDev "tvi_bsdbt848: Kann dsp-Gerät nicht öffnen. Fehler: %s\n"
#define MSGTR_TV_Bt848ErrorConfiguringDsp "tvi_bsdbt848: Konfiguration von dsp fehlgeschlagen. Fehler: %s\n"
#define MSGTR_TV_Bt848ErrorReadingAudio "tvi_bsdbt848: Fehler beim Lesen der Audiodaten: %s\n"
#define MSGTR_TV_Bt848MmapFailed "tvi_bsdbt848: mmap fehlgeschlagen. Fehler: %s\n"
#define MSGTR_TV_Bt848FrameBufAllocFailed "tvi_bsdbt848: Allozierung des Framebuffers fehlgeschlagen. Fehler: %s\n"
#define MSGTR_TV_Bt848ErrorSettingWidth "tvi_bsdbt848: Fehler beim Setzen der Bildbreite: %s\n"
#define MSGTR_TV_Bt848ErrorSettingHeight "tvi_bsdbt848: Fehler beim Setzen der Bildhöhe: %s\n"
#define MSGTR_TV_Bt848UnableToStopCapture "tvi_bsdbt848: Kann Aufnahme nicht stoppen. Fehler: %s\n"
#define MSGTR_TV_TTSupportedLanguages "Unterstützte Videotextsprachen:\n"
#define MSGTR_TV_TTSelectedLanguage "Gewählte Standardvideotextsprache: %s\n"
#define MSGTR_TV_ScannerNotAvailableWithoutTuner "Kanal-Scanner ist ohne Tuner nicht verfügbar.\n"

//tvi_dshow.c
#define MSGTR_TVI_DS_UnableConnectInputVideoDecoder  "Kann vorhandenen Input nicht mit Videodecoder verbinden. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnableConnectInputAudioDecoder  "Kann vorhandenen Input nicht mit Audiodecoder verbinden. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnableSelectVideoFormat "tvi_dshow: Kann Videoformat nicht auswählen. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnableSelectAudioFormat "tvi_dshow: Kann Audioformat nicht auswählen. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnableGetMediaControlInterface "tvi_dshow: Bekomme IMediaControl-Interface nicht. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnableStartGraph "tvi_dshow: Kann Graph nicht starten! Fehler:0x%x\n"
#define MSGTR_TVI_DS_DeviceNotFound "tvi_dshow: Gerät #%d nicht gefunden.\n"
#define MSGTR_TVI_DS_UnableGetDeviceName "tvi_dshow: Kann Namen für Gerät #%d nicht ermitteln.\n"
#define MSGTR_TVI_DS_UsingDevice "tvi_dshow: Benutze Gerät #%d: %s\n"
#define MSGTR_TVI_DS_DeviceName  "tvi_dshow: Gerät #%d: %s\n"
#define MSGTR_TVI_DS_DirectGetFreqFailed "tvi_dshow: Kann Frequenz nicht direkt ermitteln. Im Betriebssystem eingebaute\nKanaltabelle wird benutzt.\n"
#define MSGTR_TVI_DS_DirectSetFreqFailed "tvi_dshow: Kann Frequenz nicht direkt setzen. Im Betriebssystem eingebaute\nKanaltabelle wird benutzt.\n"
#define MSGTR_TVI_DS_SupportedNorms "tvi_dshow: unterstützte Normen:"
#define MSGTR_TVI_DS_AvailableVideoInputs "tvi_dshow: verfügbare Video-Inputs:"
#define MSGTR_TVI_DS_AvailableAudioInputs "tvi_dshow: verfügbare Audio-Inputs:"
//following phrase will be printed near the selected audio/video input
#define MSGTR_TVI_DS_InputSelected "(gewählt)"
#define MSGTR_TVI_DS_UnableExtractFreqTable "tvi_dshow: Kann Frequenztabelle nicht von kstvtune.ax laden\n"
#define MSGTR_TVI_DS_WrongDeviceParam "tvi_dshow: Falscher Geräteparameter: %s\n"
#define MSGTR_TVI_DS_WrongDeviceIndex "tvi_dshow: Falscher Geräteindex: %d\n"
#define MSGTR_TVI_DS_WrongADeviceParam "tvi_dshow: Falscher adevice-Parameter: %s\n"
#define MSGTR_TVI_DS_WrongADeviceIndex "tvi_dshow: Falscher adevice-Index: %d\n"

#define MSGTR_TVI_DS_SamplerateNotsupported "tvi_dshow: Samplerate %d wird vom Gerät nicht unterstützt. Verwende\nerste verfügbare.\n"
#define MSGTR_TVI_DS_VideoAdjustigNotSupported "tvi_dshow: Anpassen von Helligkeit/Farbton/Sättigung wird vom Gerät nicht\nunterstützt\n"

#define MSGTR_TVI_DS_ChangingWidthHeightNotSupported "tvi_dshow: Änderung von Videobreite/-höhe wird vom Gerät nicht unterstützt.\n"
#define MSGTR_TVI_DS_SelectingInputNotSupported  "tvi_dshow: Wahl der Capture-Quelle wird vom Gerät nicht unterstützt.\n"
#define MSGTR_TVI_DS_FreqTableLoaded "tvi_dshow: Frequenztabelle des Systems (%s) für Land-ID=%d (Kanäle:%d) geladen.\n"
#define MSGTR_TVI_DS_ErrorParsingAudioFormatStruct "tvi_dshow: Kann Struktur für Audioformat nicht parsen.\n"
#define MSGTR_TVI_DS_ErrorParsingVideoFormatStruct "tvi_dshow: Kann Struktur für Videoformat nicht parsen.\n"
#define MSGTR_TVI_DS_UnableSetAudioMode "tvi_dshow: Kann Audiomodus %d nicht setzen. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnsupportedMediaType "tvi_dshow: Nichtunterstützten Medientyp an %s übergeben\n"
#define MSGTR_TVI_DS_UnableGetsupportedVideoFormats "tvi_dshow: Kann vom Video-Pin unterstützte Medienformate nicht ermitteln.\nFehler:0x%x\n"
#define MSGTR_TVI_DS_UnableGetsupportedAudioFormats "tvi_dshow: Kann vom Audio-Pin unterstützte Medienformate nicht ermitteln.\nFehler:0x%x Deaktiviere Ton.\n"
#define MSGTR_TVI_DS_UnableFindNearestChannel "tvi_dshow: Kann nächstgelegenen Kanal in Systemfrequenztabelle nicht finden\n"
#define MSGTR_TVI_DS_UnableToSetChannel "tvi_dshow: Kann nicht auf nächstgelegenen Kanal aus der Systemfrequenztabelle\nwechseln. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnableTerminateVPPin "tvi_dshow: Kann VideoPort-Pin mit keinem Filter des Graphen terminieren. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnableBuildVideoSubGraph "tvi_dshow: Kann Videokette des Capture-Graphen nicht erstellen. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnableBuildAudioSubGraph "tvi_dshow: Kann Audiokette des Capture-Graphen nicht erstellen. Fehler:0x%x\n"
#define MSGTR_TVI_DS_UnableBuildVBISubGraph "tvi_dshow: Kann VBI-Kette des Capture-Graphen nicht erstellen. Fehler:0x%x\n""
#define MSGTR_TVI_DS_GraphInitFailure "tvi_dshow: Initialisierung des Directshow-Graph fehlgeschlagen.\n"
#define MSGTR_TVI_DS_NoVideoCaptureDevice "tvi_dshow: Kein Gerät für Videoerfassung gefunden\n"
#define MSGTR_TVI_DS_NoAudioCaptureDevice "tvi_dshow: Kein Gerät für Audioerfassung gefunden\n"
#define MSGTR_TVI_DS_GetActualMediatypeFailed "tvi_dshow: Kann eigentlichen Medientyp nicht ermitteln (Fehler:0x%x).\nNehme an, dieser entspricht dem angeforderten.\n"
