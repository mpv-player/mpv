// Translated by: Johannes Feigl, johannes.feigl@aon.at
// Reworked by Klaus Umbach, klaus.umbach@gmx.net and
// Moritz Bunkus, moritz@bunkus.org

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2003 Arpad Gereoffy (siehe DOCS!)\n"
"\n";

static char help_text[]=
"Verwendung:   mplayer [optionen] [url|verzeichnis/]dateiname\n"
"\n"
"Grundlegende Optionen: (siehe Manpage für eine vollständige Liste ALLER Optionen!)\n"
" -vo <drv[:dev]>  Videoausgabetreiber & -gerät (siehe '-vo help' für eine Liste)\n"
" -ao <drv[:dev]>  Audioausgabetreiber & -gerät (siehe '-ao help' für eine Liste)\n"
#ifdef HAVE_VCD
" -vcd <tracknr>   Spiele einen VCD-Titel (Video CD) anstelle einer Datei\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <dev>   Benutze DVD-Gerät für die Authentifizierung (für verschl. DVDs)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titelnr>   Spiele DVD-Titel von Gerät anstelle einer Datei\n"
" -alang/-slang    Wähle DVD Audio/Untertitel Sprache (2-Zeichen-Ländercode)\n"
#endif
" -ss <zeitpos>    Starte Abspielen ab Position (Sekunden oder hh:mm:ss)\n"
" -nosound         Spiele keinen Sound\n"
" -fs -vm -zoom    Vollbildoptionen (Vollbild, Videomodus, Softwareskalierung)\n"
" -x <x> -y <y>    Setze Bildschirmauflösung (für Vidmode-Wechsel oder SW-Skalierung)\n"
" -sub <datei>     Benutze Untertitel-Datei (siehe auch -subfps, -subdelay)\n"
" -playlist <datei> Lese Abspielliste aus Datei\n"
" -vid x -aid y    Spiele Videostream (x) und Audiostream (y)\n"
" -fps x -srate y  Benutze Videoframerate (x fps) und Audiosamplingrate (y Hz)\n"
" -pp <Qualität>   Aktiviere Nachbearbeitungsfilter (siehe Manpage für Details)\n"
" -framedrop       Verwerfe Bilder (bei langsamen Rechnern)\n"
"\n"
"Grundlegende Tasten:\n"
" <- oder ->       Springe zehn Sekunden vor/zurück\n"
" Cursor hoch/runter Springe eine Minute vor/zurück\n"
" Bild hoch/runter Springe 10 Minuten vor/zurück\n"
" < oder >         Springe in der Abspielliste vor/zurück\n"
" p oder LEER      Pause (beliebige Taste zum Fortsetzen)\n"
" q oder ESC       Abspielen stoppen und Programm beenden\n"
" + oder -         Audioverzögerung um +/- 0.1 Sekunde verändern\n"
" o                OSD-Modus:  Aus / Suchleiste / Suchleiste + Zeit\n"
" * oder /         PCM-Lautstärke verstellen\n"
" z oder x         Untertitelverzögerung um +/- 0.1 Sekunde verändern\n"
" r oder t         Verschiebe die Untertitel-Position, siehe auch -vop expand!\n"
"\n"
" * * * IN DER MANPAGE STEHEN WEITERE TASTEN UND OPTIONEN ! * * *\n"
"\n";
#endif

// ========================= MPlayer Ausgaben ===========================

// mplayer.c: 
#define MSGTR_Exiting "\nBeende... (%s)\n"
#define MSGTR_Exit_quit "Ende"
#define MSGTR_Exit_eof "Ende der Datei"
#define MSGTR_Exit_error "Schwerer Fehler"
#define MSGTR_IntBySignal "\nMPlayer wurde durch Signal %d im Modul %s beendet\n"
#define MSGTR_NoHomeDir "Kann Homeverzeichnis nicht finden\n"
#define MSGTR_GetpathProblem "get_path(\"config\") Problem\n"
#define MSGTR_CreatingCfgFile "Erstelle Konfigurationsdatei: %s\n"
#define MSGTR_InvalidVOdriver "Ungültiger Videoausgabetreibername: %s\n'-vo help' zeigt eine Liste an.\n"
#define MSGTR_InvalidAOdriver "Ungültiger Audioausgabetreibername: %s\n'-ao help' zeigt eine Liste an.\n"
#define MSGTR_CopyCodecsConf "(kopiere/linke etc/codecs.conf nach ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Benutze eingebaute Standardwerte für codecs.conf.\n"
#define MSGTR_CantLoadFont "Kann Schriftdatei %s nicht laden\n"
#define MSGTR_CantLoadSub "Kann Untertitel nicht laden: %s\n"
#define MSGTR_ErrorDVDkey "Fehler beim Bearbeiten des DVD-Schlüssels..\n"
#define MSGTR_CmdlineDVDkey "Der DVD-Schlüssel der Kommandozeile wurde für das Entschlüsseln gespeichert.\n"
#define MSGTR_DVDauthOk "DVD Authentifizierungssequenz scheint OK zu sein.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: Ausgewählter Stream fehlt!\n"
#define MSGTR_CantOpenDumpfile "Kann dump-Datei nicht öffnen!!!\n"
#define MSGTR_CoreDumped "Core dumped ;)\n"
#define MSGTR_FPSnotspecified "FPS ist im Header nicht angegeben (oder ungültig)! Benutze -fps Option!\n"
#define MSGTR_TryForceAudioFmtStr "Erzwinge Audiocodecgruppe %s ...\n"
#define MSGTR_CantFindAfmtFallback "Kann keinen Audiocodec für gewünschte Gruppe finden, verwende anderen.\n"
#define MSGTR_CantFindAudioCodec "Kann Codec für Audioformat 0x%X nicht finden!\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Versuche, %s durch etc/codecs.conf zu ersetzen.\n*** Sollte es weiterhin nicht gehen, dann lies DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Kann Audiocodec nicht finden! -> Kein Ton\n"
#define MSGTR_TryForceVideoFmtStr "Erzwinge Videocodecgruppe %s ...\n"
#define MSGTR_CantFindVideoCodec "Kann keinen Codec passend zum gewählten -vo und Videoformat 0x%X finden!\n"
#define MSGTR_VOincompCodec "Sorry, der ausgewählte Videoausgabetreiber ist nicht kompatibel mit diesem Codec.\n"
#define MSGTR_CannotInitVO "FATAL: Kann Videoausgabetreiber nicht initialisieren!\n"
#define MSGTR_CannotInitAO "Kann Audiotreiber/Soundkarte nicht initialisieren -> Kein Ton\n"
#define MSGTR_StartPlaying "Starte Wiedergabe...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***************************************************\n"\
"         **** Dein System ist zu LANGSAM zum Abspielen! ****\n"\
"         ***************************************************\n"\
"!!! Mögliche Gründe, Probleme, Abhilfen: \n"\
"- Meistens: defekter/fehlerhafter _Audiotreiber_.\n"\
"  - Versuche -ao sdl, verwende ALSA 0.5 oder die OSS Emulation von ALSA 0.9.\n"\
"  - Du kannst es auch mit -autosync 30 oder anderen Werten probieren.\n"\
"- Langsame Videoausgabe. Versuche einen anderen -vo Treiber (Liste: -vo help)\n"\
"  oder versuche es mit -framedrop ! Lies DOCS/German/video.html für Tipps.\n"\
"- Langsame CPU.\n"\
"  - Versuche nicht, DVDs/große DivX-Filme auf langsamen CPUs abzuspielen. Probier -hardframedrop\n"\
"- Defekte Datei.\n"\
"  - Versuche verschiedene Kombinationen von: -nobps  -ni  -mc 0  -forceidx\n"\
"- Für Wiedergabe von langsamen Medien (NFS/SMB, DVD, VCD usw) versuche -cache 8192.\n"\
"- Benutzt du -cache zusammen mit einer schlecht interleavten AVI-Datei?\n"\
"  - Probiere -nocache.\n"\
"Lies DOCS/German/video.html und DOCS/German/sound.html; dort stehen Tipps\n"\
"und Kniffe für optimale Einstellungen.\n"\
"Wenn nichts davon hilft, lies DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer wurde OHNE GUI-Unterstützung kompiliert.\n"
#define MSGTR_GuiNeedsX "MPlayer GUI erfordert X11.\n"
#define MSGTR_Playing "Spiele %s\n"
#define MSGTR_NoSound "Audio: kein Ton!!!\n"
#define MSGTR_FPSforced "FPS fixiert auf %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "MPlayer wurde mit Laufzeit-CPU-Erkennung kompiliert - Warnung, das ist nicht optimal. Um die beste Performance zu erhalten, kompiliere MPlayer aus den Sourcen mit --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Kompiliert für x86 CPU mit folgenden Erweiterungen:"
#define MSGTR_AvailableVideoOutputPlugins "Verfügbare Videoausgabeplugins:\n"
#define MSGTR_AvailableVideoOutputDrivers "Verfügbare Videoausgabetreiber:\n"
#define MSGTR_AvailableAudioOutputDrivers "Verfügbare Audioausgabetreiber:\n"
#define MSGTR_AvailableAudioCodecs "Verfügbare Audiocodecs:\n"
#define MSGTR_AvailableVideoCodecs "Verfügbare Videocodecs:\n"
#define MSGTR_AvailableAudioFm "\nVerfügbare (in das Binary kompilierte) Audio Codec Familien:\n"
#define MSGTR_AvailableVideoFm "\nVerfügbare (in das Binary kompilierte) Video Codec Familien:\n"
#define MSGTR_UsingRTCTiming "Verwende Linux Hardware RTC-Timing (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Video: Kann Eigenschaften nicht lesen\n"
#define MSGTR_NoStreamFound "Keine Streams gefunden\n"
#define MSGTR_InitializingAudioCodec "Initialisiere Audiocodec...\n"
#define MSGTR_ErrorInitializingVODevice "Fehler beim Öffnen/Initialisieren des ausgewählten Videoausgabetreibers (-vo).\n"
#define MSGTR_ForcedVideoCodec "Videocodec fixiert: %s\n"
#define MSGTR_ForcedAudioCodec "Audiocodec fixiert: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Beschreibung: %s\nAO: Autor: %s\n"
#define MSGTR_AOComment "AO: Hinweis: %s\n"
#define MSGTR_Video_NoVideo "Video: kein Video\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Konnte Videofilter (-vop) oder Videoausgabetreiber (-vo) nicht initialisieren.\n"
#define MSGTR_Paused "\n------ PAUSE -------\r"
#define MSGTR_PlaylistLoadUnable "\nKann Abspielliste %s nicht laden.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer stürzte wegen einer 'illegalen Anweisung' ab.\n"\
"  Es kann sich um einen Fehler im neuen Code für die CPU-Erkennung zur\n"\
"  Laufzeit handeln... Bitte lies DOCS/German/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer stürzte wegen einer 'illegalen Anweisung' ab.\n"\
"  Das passiert normalerweise, wenn du MPlayer auf einer anderen CPU\n"\
"  ausführst als auf der, auf der er kompiliert wurde. Überprüfe das!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer stürzte wegen falscher Benutzung der CPU/FPU/des RAMs ab.\n"\
"  Kompiliere MPlayer erneut mit --enable-debug und erstelle mit 'gdb'\n"\
"  einen Backtrace und eine Disassemblerausgabe. Details dazu findest du\n"\
"  in DOCS/German/bugreports.html"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer ist abgestürzt. Das sollte nicht passieren.\n"\
"  Es kann sich um einen Fehler im MPlayer-Code _oder_ in deinen Treibern\n"\
"  _oder_ in deinem gcc handeln. Wenn du meinst, es sei MPlayers Schuld, dann\n"\
"  lies DOCS/German/bugreports.html, und folge den dortigen Anweisungen.\n"\
"  Wir können und werden dir nicht helfen, wenn du nicht alle dort aufgeführten\n"\
"  Informationen zur Verfügung stellst.\n"

// mencoder.c:
#define MSGTR_MEncoderCopyright "(C) 2000-2003 Arpad Gereoffy (siehe DOCS!)\n"
#define MSGTR_UsingPass3ControllFile "Verwende Pass 3 Kontrolldatei: %s\n"
#define MSGTR_MissingFilename "\nDateiname nicht angegeben.\n\n"
#define MSGTR_CannotOpenFile_Device "Kann Datei/Gerät nicht öffnen\n"
#define MSGTR_ErrorDVDAuth "Fehler bei der DVD Authentifizierung...\n"
#define MSGTR_CannotOpenDemuxer "Kann Demuxer nicht öffnen.\n"
#define MSGTR_NoAudioEncoderSelected "\nKein Audioencoder (-oac) ausgewählt. Wähle einen aus oder verwende -nosound. Verwende -oac help.\n"
#define MSGTR_NoVideoEncoderSelected "\nKein Videoencoder (-ovc) ausgewählt. Wähle einen aus, verwende -ovc help.\n"
#define MSGTR_CannotOpenOutputFile "Kann Ausgabedatei '%s' nicht öffnen.\n"
#define MSGTR_EncoderOpenFailed "Öffnen des Encoders fehlgeschlagen.\n"
#define MSGTR_ForcingOutputFourcc "Output-Fourcc auf %x [%.4s] gesetzt.\n"
#define MSGTR_WritingAVIHeader "Schreibe AVI Header...\n"
#define MSGTR_DuplicateFrames "\n%d doppelte(r) Frame(s)!\n"
#define MSGTR_SkipFrame "\nFrame ausgelassen!\n"
#define MSGTR_ErrorWritingFile "%s: Fehler beim Schreiben der Datei.\n"
#define MSGTR_WritingAVIIndex "\nSchreibe AVI-Index...\n"
#define MSGTR_FixupAVIHeader "Korrigiere AVI-Header...\n"
#define MSGTR_RecommendedVideoBitrate "Empfohlene Videobitrate für %s CD(s): %d\n"
#define MSGTR_VideoStreamResult "\nVideostream: %8.3f kbit/s  (%d bps)  Größe: %d Bytes  %5.3f Sek.  %d Frames\n"
#define MSGTR_AudioStreamResult "\nAudiostream: %8.3f kbit/s  (%d bps)  Größe: %d Bytes  %5.3f Sek.\n"

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
"               Erzwingt auch den CBR-Modus bei nachfolgenden ABR-Presets\n"\
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
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: kein Padding\n"\
"                1: alles\n"\
"                2: Anpassung\n"\
"\n"\
" fast          Schaltet die schnellere Codierung bei nachfolgenden VBR-Presets\n"\
"               ein. Liefert etwas schlechtere Qualität und höhere Bitraten.\n"\
"\n"\
" preset=<wert> Gibt die bestmöglichen Qualitätseinstellungen.\n"\
"                 medium: VBR-Encodierung, gute Qualität\n"\
"                 (150-180 kbps Bitratenbereich)\n"\
"                 standard:  VBR-Encodierung, hohe Qualität\n"\
"                 (170-210 kbps Bitratenbereich)\n"\
"                 extreme: VBR-Encodierung, sehr hohe Qualität\n"\
"                 (200-240 kbps Bitratenbereich)\n"\
"                 insane:  CBR-Encodierung, bestes Preset\n"\
"                 (320 kbps Bitrate)\n"\
"                 <8-320>: ABR-Encodierung mit der angegebenen durchschnittlichen\n"\
"                          Bitrate\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM-Gerät '%s' nicht gefunden.\n"
#define MSGTR_ErrTrackSelect "Fehler beim Auswählen des VCD Tracks.\n"
#define MSGTR_ReadSTDIN "Lese von stdin...\n"
#define MSGTR_UnableOpenURL "Kann URL nicht öffnen: %s\n"
#define MSGTR_ConnToServer "Verbunden mit Server: %s\n"
#define MSGTR_FileNotFound "Datei nicht gefunden: '%s'\n"

#define MSGTR_SMBInitError "Kann die Bibliothek libsmbclient nicht öffnen: %d\n"
#define MSGTR_SMBFileNotFound "Konnte '%s' nicht vom Netzwerk öffnen\n"
#define MSGTR_SMBNotCompiled "MPlayer wurde ohne Unterstützung für SMB kompiliert\n"

#define MSGTR_CantOpenDVD "Kann DVD-Gerät nicht öffnen: %s\n"
#define MSGTR_DVDwait "Lese Disk-Struktur, bitte warten...\n"
#define MSGTR_DVDnumTitles "Es sind %d Titel auf dieser DVD.\n"
#define MSGTR_DVDinvalidTitle "Ungültige DVD-Titelnummer: %d\n"
#define MSGTR_DVDnumChapters "Es sind %d Kapitel in diesem DVD-Titel.\n"
#define MSGTR_DVDinvalidChapter "Ungültige DVD-Kapitelnummer: %d\n"
#define MSGTR_DVDnumAngles "Es sind %d Kameraeinstellungen diesem DVD-Titel.\n"
#define MSGTR_DVDinvalidAngle "Ungültige DVD-Kameraeinstellungsnummer: %d\n"
#define MSGTR_DVDnoIFO "Kann die IFO-Datei für den DVD-Titel nicht öffnen %d.\n"
#define MSGTR_DVDnoVOBs "Kann Titel-VOBs (VTS_%02d_1.VOB) nicht öffnen.\n"
#define MSGTR_DVDopenOk "DVD erfolgreich geöffnet.\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Warnung! Audiostreamheader %d erneut definiert!\n"
#define MSGTR_VideoStreamRedefined "Warnung! Videostreamheader %d erneut definiert!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Zu viele (%d in %d bytes) Audiopakete im Puffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Zu viele (%d in %d bytes) Videopakete im Puffer!\n"
#define MSGTR_MaybeNI "Vielleicht spielst du einen non-interleaved Stream/Datei, oder der Codec funktioniert nicht.\n" \
                      "Versuche für .AVI Dateien den nicht-interleaved Modus mit der Option -ni zu erzwingen\n"
#define MSGTR_SwitchToNi "\nSchlechtes Interleaved .AVI erkannt, schalte in den -ni Modus!\n"
#define MSGTR_Detected_XXX_FileFormat "%s-Dateiformat erkannt!\n"
#define MSGTR_DetectedAudiofile "Audiodatei erkannt!\n"
#define MSGTR_NotSystemStream "Kein MPEG System Stream ... (vielleicht Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Ungültiger MPEG-ES Stream??? Kontaktiere den Autor, das könnte ein Bug sein :(\n"
#define MSGTR_FormatNotRecognized "========= Sorry, das Dateiformat/der Codec wird nicht unterstützt ============\n"\
				  "============== Sollte dies ein AVI, ASF oder MPEG Stream sein, ===============\n"\
				  "================== dann kontaktiere bitte den Autor. ========================\n"
#define MSGTR_MissingVideoStream "Kann keinen Videostream finden.\n"
#define MSGTR_MissingAudioStream "Kann keinen Audiostream finden...  -> kein Ton\n"
#define MSGTR_MissingVideoStreamBug "Vermisse Videostream!? Kontaktiere den Autor, möglicherweise ein Bug :(\n"

#define MSGTR_DoesntContainSelectedStream "Demuxer: Datei enthält den gewählten Audio- oder Videostream nicht.\n"

#define MSGTR_NI_Forced "Erzwungen"
#define MSGTR_NI_Detected "Erkannt"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI-Dateiformat.\n"

#define MSGTR_UsingNINI "Verwende defektes NON-INTERLEAVED AVI Dateiformat.\n"
#define MSGTR_CouldntDetFNo "Konnte die Anzahl der Frames (für absolute Suche) nicht finden.\n"
#define MSGTR_CantSeekRawAVI "Kann keine RAW .AVI-Streams durchsuchen. (Index erforderlich, versuche es mit der -idx Option.)\n"
#define MSGTR_CantSeekFile "Kann diese Datei nicht durchsuchen.\n"

#define MSGTR_EncryptedVOB "Verschlüsselte VOB-Datei (wurde ohne libcss Unterstützung kompiliert). Lies DOCS.\n"
#define MSGTR_EncryptedVOBauth "Verschlüsselter Stream, jedoch wurde die Authentifizierung nicht von Dir gefordert.\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimierte Header werden (zur Zeit) nicht unterstützt.\n"
#define MSGTR_MOVvariableFourCC "MOV: Warnung: Variable FOURCC erkannt!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Warnung: Zu viele Tracks."
#define MSGTR_FoundAudioStream "==> Audiostream gefunden: %d\n"
#define MSGTR_FoundVideoStream "==> Videostream gefunden: %d\n"
#define MSGTR_DetectedTV "TV festgestellt! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Öffnen des OGG-Demuxers fehlgeschlagen.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Suche nach Audiostream (Id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Kann Audiostream nicht öffnen: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Kann Untertitelstream nicht öffnen: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Öffnen des Audiodemuxers fehlgeschlagen: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Öffnen des Untertiteldemuxers fehlgeschlagen: %s\n"
#define MSGTR_TVInputNotSeekable "TV-Input ist nicht durchsuchbar. (Möglicherweise änderst du damit den Kanal ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Demuxerinfo %s existiert bereits.\n"
#define MSGTR_ClipInfo "Clipinfo: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: Progressives Sequenz gefunden. Verlasse den 3:2-TELECINE-Modus.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 3:2 TELECINE gefunden. Aktiviere inverserse telecine fx. FPS wurde in %5.3 geändert!\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "kann Codec nicht öffnen.\n"
#define MSGTR_CantCloseCodec "kann Codec nicht schließen.\n"

#define MSGTR_MissingDLLcodec "FEHLER: Kann erforderlichen DirectShow-Codec nicht finden: %s\n"
#define MSGTR_ACMiniterror "Kann Win32/ACM-Audiocodec nicht finden (fehlende DLL-Datei?).\n"
#define MSGTR_MissingLAVCcodec "Kann Codec '%s' von libavcodec nicht finden...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: Ende der Datei während der Suche für Sequenzheader.\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Kann Sequenzheader nicht lesen.\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Kann Sequenzheader-Erweiterung nicht lesen.\n"
#define MSGTR_BadMpegSequHdr "MPEG: Schlechte Sequenzheader.\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Schlechte Sequenzheader-Erweiterung.\n"

#define MSGTR_ShMemAllocFail "Kann keinen gemeinsamen Speicher zuweisen.\n"
#define MSGTR_CantAllocAudioBuf "Kann keinen Audioausgabe-Puffer zuweisen.\n"

#define MSGTR_UnknownAudio "Unbekanntes/fehlendes Audioformat -> kein Ton\n"

#define MSGTR_UsingExternalPP "[PP] Verwende externe Nachbearbeitungsfilter, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Verwende Nachbearbeitungsroutinen des Codecs, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Videoeigenschaft '%s' wird vom ausgewählten vo & vd nicht unterstützt.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Erforderliche Videocodec Familie [%s] (vfm=%s) nicht verfügbar (aktiviere sie beim Kompilieren).\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Erforderliche Audiocodec Familie [%s] (afm=%s) nicht verfügbar (aktiviere sie beim Kompilieren).\n"
#define MSGTR_OpeningVideoDecoder "Öffne Videodecoder: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Öffne Audiodecoder: [%s] %s\n"
#define MSGTR_UninitVideoStr "Deinitialisiere Video: %s  \n"
#define MSGTR_UninitAudioStr "Deinitialisiere Audio: %s  \n"
#define MSGTR_VDecoderInitFailed "Initialisierung des Videodecoders fehlgeschlagen :(\n"
#define MSGTR_ADecoderInitFailed "Initialisierung des Audiodecoders fehlgeschlagen :(\n"
#define MSGTR_ADecoderPreinitFailed "Preinitialisierung des Audiodecoders fehlgeschlagen :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Reserviere %d Bytes für Eingangspuffer.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Reserviere %d + %d = %d Bytes für Ausgabepuffer.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Initialisiere LIRC Unterstützung...\n"
#define MSGTR_LIRCdisabled "Verwendung der Fernbedienung nicht möglich.\n"
#define MSGTR_LIRCopenfailed "Fehler beim Öffnen der LIRC Unterstützung.\n"
#define MSGTR_LIRCcfgerr "Kann LIRC-Konfigurationsdatei %s nicht lesen.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Konnte Videofilter '%s' nicht finden.\n"
#define MSGTR_CouldNotOpenVideoFilter "Konnte Videofilter '%s' nicht öffnen.\n"
#define MSGTR_OpeningVideoFilter "Öffne Videofilter: "
#define MSGTR_CannotFindColorspace "Konnte keinen gemeinsamen Farbraum finden, auch nicht mit Hilfe von 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Codec hat sh->disp_w und sh->disp_h nicht gesetzt, versuche zu umgehen.\n"
#define MSGTR_VoConfigRequest "VDec: VO wird versucht, auf %d x %d (Bevorzugter Farbraum: %s) zu setzen.\n"
#define MSGTR_CouldNotFindColorspace "Kann keinen passenden Farbraum finden - versuche erneut mithilfe von -vop scale...\n"
#define MSGTR_MovieAspectIsSet "Seitenverhältnis ist %.2f:1 - Skaliere zur korrekten Videogröße.\n"
#define MSGTR_MovieAspectUndefined "Seitenverhältnis ist undefiniert - skaliere nicht.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Über ..."
#define MSGTR_FileSelect "Wähle Datei ..."
#define MSGTR_SubtitleSelect "Wähle Untertitel ..."
#define MSGTR_OtherSelect "Wähle ..."
#define MSGTR_AudioFileSelect "Wähle externen Audiokanal ..."
#define MSGTR_FontSelect "Wähle Schrift ..."
#define MSGTR_PlayList "Abspielliste"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Skin-Browser"
#define MSGTR_Network "Netzwerk-Streaming ..."
#define MSGTR_Preferences "Einstellungen"
#define MSGTR_OSSPreferences "OSS-Treibereinstellungen"
#define MSGTR_SDLPreferences "SDL-Treibereinstellungen"
#define MSGTR_NoMediaOpened "nichts geöffnet"
#define MSGTR_VCDTrack "VCD-Titel %d"
#define MSGTR_NoChapter "kein Kapitel"
#define MSGTR_Chapter "Kapitel %d"
#define MSGTR_NoFileLoaded "keine Datei geladen"

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
#define MSGTR_NEMDB "Sorry, nicht genug Speicher für den Zeichen-Puffer."
#define MSGTR_NEMFMR "Sorry, nicht genug Speicher für Menü-Rendering."
#define MSGTR_IDFGCVD "Sorry, kann keinen GUI-kompatiblen Ausgabetreiber finden"
#define MSGTR_NEEDLAVCFAME "Sorry, du versuchst, Nicht-MPEG Dateien über deine DXR3/H+-Karte ohne Neuenkodierung abzuspielen.\nBitte aktiviere lavc oder fame in der DXR3/H+-Configbox."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[Skin] Fehler in Skin-Konfigurationsdatei in Zeile %d: %s" 
#define MSGTR_SKIN_WARNING1 "[Skin] Warnung in Skin-Konfigurationsdatei in Zeile %d: Widget gefunden, aber davor wurde \"section\" nicht gefunden ( %s )"
#define MSGTR_SKIN_WARNING2 "[Skin] Warnung in Skin-Konfigurationsdatei in Zeile %d: Widget gefunden, aber davor wurde \"subsection\" nicht gefunden (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] Warnung in Skin-Konfigurationsdatei in Zeile %d: Diese Untersektion wird von diesem Widget nicht unterstützt ( %s ).\n"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmaps mit 16 Bits oder weniger werden nicht unterstützt ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Datei nicht gefunden ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP-Lesefehler ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA-Lesefehler ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG-Lesefehler ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE-gepackte TGA werden nicht unterstützt ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "unbekanntes Dateiformat ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "Konvertierungsfehler von 24 Bit auf 32 Bit ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "unbekannte Nachricht: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nicht genug Speicher\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "zu viele Schriften eingestellt\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Schriftdatei nicht gefunden\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Schriftbilddatei nicht gefunden\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "nicht existierende Schriftbezeichnung ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "unbekannter Parameter ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[Skin Browser] nicht genug Speicher.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin nicht gefunden ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Lesefehler beim Lesen der Skin-Configdatei ( %s ).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Über MPlayer"
#define MSGTR_MENU_Open "Öffnen ..."
#define MSGTR_MENU_PlayFile "Spiele Datei ..."
#define MSGTR_MENU_PlayVCD "Spiele VCD ..."
#define MSGTR_MENU_PlayDVD "Spiele DVD ..."
#define MSGTR_MENU_PlayURL "Spiele URL ..."
#define MSGTR_MENU_LoadSubtitle "Lade Untertitel ..."
#define MSGTR_MENU_DropSubtitle "Entferne Untertitel ..."
#define MSGTR_MENU_LoadExternAudioFile "Lade externe Audiodatei ..."
#define MSGTR_MENU_Playing "Spiele"
#define MSGTR_MENU_Play "Spiele"
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
#define MSGTR_MENU_PlayDisc "Spiele Disk ..."
#define MSGTR_MENU_ShowDVDMenu "Zeige DVD Menü"
#define MSGTR_MENU_Titles "Titel"
#define MSGTR_MENU_Title "Titel %2d"
#define MSGTR_MENU_None "(nichts)"
#define MSGTR_MENU_Chapters "Kapitel"
#define MSGTR_MENU_Chapter "Kapitel %2d"
#define MSGTR_MENU_AudioLanguages "Audio-Sprachen"
#define MSGTR_MENU_SubtitleLanguages "Untertitel-Sprachen"
#define MSGTR_MENU_PlayList "Abspielliste"
#define MSGTR_MENU_SkinBrowser "Skinbrowser"
#define MSGTR_MENU_Preferences "Einstellungen"
#define MSGTR_MENU_Exit "Beenden ..."
#define MSGTR_MENU_Mute "Stummschalten"
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
#define MSGTR_EQU_Bass "Tiefton" // LFE
#define MSGTR_EQU_All "Alle"
#define MSGTR_EQU_Channel1 "Kanal 1:"
#define MSGTR_EQU_Channel2 "Kanal 2:"
#define MSGTR_EQU_Channel3 "Kanal 3:"
#define MSGTR_EQU_Channel4 "Kanal 4:"
#define MSGTR_EQU_Channel5 "Kanal 5:"
#define MSGTR_EQU_Channel6 "Kanal 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Pfad"
#define MSGTR_PLAYLIST_Selected "ausgewählte Dateien"
#define MSGTR_PLAYLIST_Files "Dateien"
#define MSGTR_PLAYLIST_DirectoryTree "Verzeichnisbaum"

// --- preferences
#define MSGTR_PREFERENCES_Audio "Audio"
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_SubtitleOSD "Untertitel & OSD"
#define MSGTR_PREFERENCES_Misc "Sonstiges"
#define MSGTR_PREFERENCES_Codecs "Codecs & Demuxer"

#define MSGTR_PREFERENCES_None "Nichts"
#define MSGTR_PREFERENCES_AvailableDrivers "Verfügbare Treiber:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Spiele keinen Ton"
#define MSGTR_PREFERENCES_NormalizeSound "Normalisiere Ton"
#define MSGTR_PREFERENCES_EnEqualizer "Equalizer verwenden"
#define MSGTR_PREFERENCES_ExtraStereo "Extra Stereo verwenden"
#define MSGTR_PREFERENCES_Coefficient "Koeffizient:"
#define MSGTR_PREFERENCES_AudioDelay "Audio-Verzögerung"
#define MSGTR_PREFERENCES_DoubleBuffer "Double-Buffering verwenden"
#define MSGTR_PREFERENCES_DirectRender "Direct-Rendering verwenden"
#define MSGTR_PREFERENCES_FrameDrop "Frame-Dropping verwenden"
#define MSGTR_PREFERENCES_HFrameDrop "HARD Frame-Dropping verwenden ( gefährlich )"
#define MSGTR_PREFERENCES_Flip "Bild spiegeln"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "Zeit und Indikatoren"
#define MSGTR_PREFERENCES_OSDProgress "nur Fortschrittsbalken"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Timer, prozentuale und absolute Zeit"
#define MSGTR_PREFERENCES_Subtitle "Untertitel:"
#define MSGTR_PREFERENCES_SUB_Delay "Verzögerung: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Position: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Automatisches Laden der Untertitel ausschalten"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode-Untertitel"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konvertiere Untertitel in's MPlayer-Untertitelformat"
#define MSGTR_PREFERENCES_SUB_SRT "Konvertiere Untertitel in's zeitbasierende SubViewer-Untertitelformat (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Schalte Untertitelüberlappung ein/aus"
#define MSGTR_PREFERENCES_Font "Schrift:"
#define MSGTR_PREFERENCES_FontFactor "Schriftfaktor:"
#define MSGTR_PREFERENCES_PostProcess "Nachbearbeitung aktivieren:"
#define MSGTR_PREFERENCES_AutoQuality "Auto-Qualität: "
#define MSGTR_PREFERENCES_NI "Non-Interleaved AVI Parser verwenden"
#define MSGTR_PREFERENCES_IDX "Indextabelle neu aufbauen, falls benötigt"
#define MSGTR_PREFERENCES_VideoCodecFamily "Videocodecfamilie:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Audiocodecfamilie:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD-Modus"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Untertitel"
#define MSGTR_PREFERENCES_FRAME_Font "Schrift"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Nachbearbeitung"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & Demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache"
#define MSGTR_PREFERENCES_FRAME_Misc "Sonstiges"
#define MSGTR_PREFERENCES_OSS_Device "Gerät:"
#define MSGTR_PREFERENCES_OSS_Mixer "Mixer:"
#define MSGTR_PREFERENCES_SDL_Driver "Treiber:"
#define MSGTR_PREFERENCES_Message "Bitte bedenke, dass mache Funktionen einen Neustart der Wiedergabe erfordern."
#define MSGTR_PREFERENCES_DXR3_VENC "Videoencoder:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Verwende LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Verwende FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Westeuropäische Sprachen (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Westeuropäische Sprachen mit Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Slavische / Westeuropäische Sprache (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Gallisch, Maltesisch, Türkisch (ISO-8859-3)"
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
#define MSGTR_PREFERENCES_FontNoAutoScale "Kein automatische Skalierung"
#define MSGTR_PREFERENCES_FontPropWidth "Proportional zur Breite des Films"
#define MSGTR_PREFERENCES_FontPropHeight "Proportional zur Höhe des Films"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proportional zur Diagonale des Films"
#define MSGTR_PREFERENCES_FontEncoding "Encoding:"
#define MSGTR_PREFERENCES_FontBlur "Unschärfe:"
#define MSGTR_PREFERENCES_FontOutLine "Outline:"
#define MSGTR_PREFERENCES_FontTextScale "Textskalierung:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD-Skalierung:"
#define MSGTR_PREFERENCES_Cache "Cache ein/aus"
#define MSGTR_PREFERENCES_CacheSize "Cachegröße: "
#define MSGTR_PREFERENCES_LoadFullscreen "im Vollbildmodus starten"
#define MSGTR_PREFERENCES_XSCREENSAVER "Deaktiviere XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Aktiviere die Playbar"
#define MSGTR_PREFERENCES_AutoSync "AutoSync ein/aus"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosyncwert: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM-Gerät:"
#define MSGTR_PREFERENCES_DVDDevice "DVD-Gerät:"
#define MSGTR_PREFERENCES_FPS "FPS des Films:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Zeige Videofenster an, wenn inaktiv"

#define MSGTR_ABOUT_UHU "GUI-Entwicklung wurde von UHU Linux gesponsert.\n"
#define MSGTR_ABOUT_CoreTeam "   MPlayer-Kernteam:\n"
#define MSGTR_ABOUT_AdditionalCoders " Weitere Programmierer:\n"
#define MSGTR_ABOUT_MainTesters "     Haupttester:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Fataler Fehler ..."
#define MSGTR_MSGBOX_LABEL_Error "Fehler ..."
#define MSGTR_MSGBOX_LABEL_Warning "Warnung ..."

#endif
