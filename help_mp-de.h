// Transated by: Johannes Feigl, johannes.feigl@aon.at
// Reworked by Klaus Umbach, klaus.umbach@gmx.net

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (siehe DOCS!)\n"
"\n";

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Verwendung:   mplayer [-gui] [optionen] [url|verzeichnis/]dateiname\n"
#else
"Verwendung:   mplayer [optionen] [url|verzeichnis/]dateiname\n"
#endif
"\n"
"Grundlegende Optionen: (siehe Manpage für eine vollständige Liste ALLER Optionen!)\n"
" -vo <drv[:dev]>  Videoausgabetreiber & -Gerät (siehe '-vo help' für eine Liste)\n"
" -ao <drv[:dev]>  Audioausgabetreiber & -Gerät (siehe '-ao help' für eine Liste)\n"
#ifdef HAVE_VCD
" -vcd <tracknr>   Spiele VCD (Video CD) Titel anstelle eines Dateinames\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <dev>   Benutze DVD Gerät für die Authentifizierung (für verschl. DVD's)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titelnr>   Spiele DVD Titel/Track von Gerät anstelle eines Dateinames\n"
" -alang/-slang    Wähle DVD Audio/Untertitel Sprache (2-Zeichen Ländercode)\n"
#endif
" -ss <zeitpos>    Starte Abspielen ab Position (Sekunden oder hh:mm:ss)\n"
" -nosound         Spiele keinen Sound\n"
" -fs -vm -zoom    Vollbild Optionen (Vollbild, Videomode, Softwareskalierung)\n"
" -x <x> -y <y>    Setze Bildschirmauflösung (für Vidmode-Wechsel oder sw-Skalierung)\n"
" -sub <datei>     Benutze Untertitle-Datei (siehe auch -subfps, -subdelay)\n"
" -playlist <datei> Benutze Playlist-Datei\n"
" -vid x -aid y    Spiele Videostream (x) und Audiostream (y)\n"
" -fps x -srate y  Benutze Videoframerate (x fps) und Audiosamplingrate (y Hz)\n"
" -pp <Qualität>   Aktiviere Nachbearbeitungsfilter (siehe Manpage für Details)\n"
" -framedrop       Benutze frame-dropping (für langsame Rechner)\n"
"\n"
"Grundlegende Tasten:\n"
" <- oder ->       Springe zehn Sekunden vor/zurück\n"
" rauf / runter    Springe eine Minute vor/zurück\n"
" pgup / pgdown    Springe 10 Minuten vor/zurück\n"
" < oder >         Springe in der Playliste vor/zurück\n"
" p oder LEER      PAUSE (beliebige Taste zum Fortsetzen)\n"
" q oder ESC       Abspielen stoppen und Programm beenden\n"
" + oder -         Audioverzögerung um +/- 0.1 Sekunde verändern\n"
" o                OSD Mode:  Aus / Suchleiste / Suchleiste + Zeit\n"
" * oder /         PCM-Lautstärke verstellen\n"
" z oder x         Untertitelverzögerung um +/- 0.1 Sekunde verändern\n"
" r oder t         Verschiebe die Untertitel-Position, siehe auch -vop expand!\n"
"\n"
" * * * IN DER MANPAGE STEHEN WEITERE KEYS UND OPTIONEN ! * * *\n"
"\n";
#endif

// ========================= MPlayer Ausgaben ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nBeende... (%s)\n"
#define MSGTR_Exit_frames "Angeforderte Anzahl an Frames gespielt"
#define MSGTR_Exit_quit "Ende"
#define MSGTR_Exit_eof "Ende der Datei"
#define MSGTR_Exit_error "Schwerer Fehler"
#define MSGTR_IntBySignal "\nMPlayer wurde durch Signal %d von Modul %s beendet\n"
#define MSGTR_NoHomeDir "Kann Homeverzeichnis nicht finden\n"
#define MSGTR_GetpathProblem "get_path(\"config\") Problem\n"
#define MSGTR_CreatingCfgFile "Erstelle Konfigurationsdatei: %s\n"
#define MSGTR_InvalidVOdriver "Ungültiger Videoausgabetreibername: %s\n'-vo help' zeigt eine Liste an.\n"
#define MSGTR_InvalidAOdriver "Ungültiger Audioausgabetreibername: %s\n'-ao help' zeigt eine Liste an.\n"
#define MSGTR_CopyCodecsConf "(kopiere/linke etc/codecs.conf nach ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Kann Schriftdatei %s nicht laden\n"
#define MSGTR_CantLoadSub "Kann Untertitel nicht laden: %s\n"
#define MSGTR_ErrorDVDkey "Fehler beim Bearbeiten des DVD-Schlüssels..\n"
#define MSGTR_CmdlineDVDkey "Der DVD-Schlüssel der Kommandozeile wurde für das Descrambeln gespeichert.\n"
#define MSGTR_DVDauthOk "DVD Authentifizierungssequenz scheint OK zu sein.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: Ausgewählter Stream fehlt!\n"
#define MSGTR_CantOpenDumpfile "Kann dump-Datei nicht öffnen!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS ist im Header nicht angegeben (oder ungültig)! Benutze -fps Option!\n"
#define MSGTR_NoVideoStream "Sorry, kein Videostream... ist nicht abspielbar\n"
#define MSGTR_TryForceAudioFmt "Erzwinge Audiocodecgruppe %d ...\n"
#define MSGTR_CantFindAfmtFallback "Kann keinen Audiocodec für gewünschte Gruppe finden, verwende anderen.\n"
#define MSGTR_CantFindAudioCodec "Kann Codec für Audioformat 0x%X nicht finden!\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Versuche %s mit etc/codecs.conf zu erneuern\n*** Sollte es weiterhin nicht gehen, dann lies DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Kann Audiocodec nicht finden! -> Kein Ton\n"
#define MSGTR_TryForceVideoFmt "Erzwinge Videocodecgruppe %d ...\n"
#define MSGTR_CantFindVfmtFallback "Kann keinen Videocodec für gewünschte Gruppe finden, verwende anderen.\n"
#define MSGTR_CantFindVideoCodec "Kann keinen Codec passend zum gewählten -vo und Videoformat 0x%X finden!\n"
#define MSGTR_VOincompCodec "Sorry, der ausgewählte Videoausgabetreiber ist nicht kompatibel mit diesem Codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Kann Videocodec nicht initialisieren :(\n"
#define MSGTR_EncodeFileExists "Datei existiert: %s (überschreibe nicht deine schönsten AVI's!)\n"
#define MSGTR_CantCreateEncodeFile "Kann Datei zum Encoden nicht öffnen\n"
#define MSGTR_CannotInitVO "FATAL: Kann Videoausgabetreiber nicht initialisieren!\n"
#define MSGTR_CannotInitAO "Kann Audiotreiber/Soundkarte nicht initialisieren -> Kein Ton\n"
#define MSGTR_StartPlaying "Starte Wiedergabe...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***************************************************\n"\
"         **** Dein System ist zu LANGSAM zum Abspielen! ****\n"\
"         ***************************************************\n"\
"!!! Mögliche Gründe, Probleme, Abhilfen: \n"\
"- Meistens: defekter/fehlerhafter _Audio_ Treiber. Abhilfe: versuche -ao sdl,\n"\
"  verwende ALSA 0.5 oder die OSS Emulation von ALSA 0.9. Lese DOCS/sound.html!\n"\
"- Langsame Videoausgabe. Versuche einen anderen -vo Treiber (Liste: -vo help)\n"\
"  oder versuche es mit -framedrop ! Lese DOCS/video.html für Tipps.\n"\
"- Langsame CPU. Keine DVD/DIVX auf einer langsamen CPU. Versuche -hardframedrop\n"\
"- Defekte Datei. Versuche verschiede Kombinationen: -nobps  -ni  -mc 0  -forceidx\n"\
"- Wird -cache verwendet, um eine nicht-interleaved Datei abzuspielen? Versuche -nocache\n"\
"Wenn nichts davon hilft, lies DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer wurde OHNE GUI-Unterstützung kompiliert!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI erfordert X11!\n"
#define MSGTR_Playing "Spiele %s\n"
#define MSGTR_NoSound "Audio: kein Ton!!!\n"
#define MSGTR_FPSforced "FPS fixiert auf %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Kompiliert mit RUNTIME CPU Detection - Warnung, das ist nicht optimal! Um die beste Performance zu erhalten kompiliere MPlayer von den Sourcen mit --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Kompiliert für x86 CPU mit folgenden Erweiterungen:"
#define MSGTR_AvailableVideoOutputPlugins "Verfügbare Videoausgabe-Plugins:\n"
#define MSGTR_AvailableVideoOutputDrivers "Verfügbare Videoausgabe-Treiber:\n"
#define MSGTR_AvailableAudioOutputDrivers "Verfügbare Audioausgabe-Treiber:\n"
#define MSGTR_AvailableAudioCodecs "Verfügbare Audiocodocs:\n"
#define MSGTR_AvailableVideoCodecs "Verfügbare Videocodecs:\n"
#define MSGTR_UsingRTCTiming "Verwende Linux Hardware RTC-Timing (%ldHz)\n"
#define MSGTR_CannotReadVideoPropertiers "Video: Kann Eigenschaften nicht lesen\n"
#define MSGTR_NoStreamFound "Kein Streams gefunden\n"
#define MSGTR_InitializingAudioCodec "Initialisiere Audiocodec...\n"
#define MSGTR_ErrorInitializingVODevice "Fehler beim Öffenen/Initialisieren des ausgewählten Videoausgabe (-vo) Treibers!\n"
#define MSGTR_ForcedVideoCodec "Videocodec fixiert: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Beschreibung: %s\nAO: Autor: %s\n"
#define MSGTR_AOComment "AO: Hinweis: %s\n"
#define MSGTR_Video_NoVideo "Video: kein Video!!!\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Konnte Videofilter (-vop) oder Videoausgabe-Treiber (-vo) nicht initialisieren!\n"
#define MSGTR_Paused "\n------ PAUSE -------\r"
#define MSGTR_PlaylistLoadUnable "\nKann Playliste %s nicht laden\n"

// mencoder.c:

#define MSGTR_MEncoderCopyright "(C) 2000-2002 Arpad Gereoffy (siehe DOCS!)\n"
#define MSGTR_UsingPass3ControllFile "Verwende Pass 3 Kontrolldatei: %s\n"
#define MSGTR_MissingFilename "\nDateiname nicht angegeben!\n\n"
#define MSGTR_CannotOpenFile_Device "Kann Datei/Gerät nicht öffnen\n"
#define MSGTR_ErrorDVDAuth "Fehler bei der DVD Authentifizierung...\n"
#define MSGTR_CannotOpenDemuxer "Kann Demuxer nicht öffnen\n"
#define MSGTR_NoAudioEncoderSelected "\nKein Audioencoder (-oac) ausgewählt! Wähle einen aus oder verwende -nosound. Verwende -oac help !\n"
#define MSGTR_NoVideoEncoderSelected "\nKein Videoencoder (-ovc) selected! Wähle einen aus, verwende -ovc help !\n"
#define MSGTR_CannotOpenOutputFile "Kann Ausgabedatei '%s' nicht öffnen\n"
#define MSGTR_EncoderOpenFailed "Öffnen des Encoders fehlgeschlagen\n"
#define MSGTR_ForcingOutputFourcc "Output-Fourcc auf %x [%.4s] gestellt\n"
#define MSGTR_WritingAVIHeader "Schreibe AVI Header...\n"
#define MSGTR_DuplicateFrames "\n%d doppelte(r) Frame(s)!!!    \n"
#define MSGTR_SkipFrame "\nFrame ausgelassen!!!    \n"
#define MSGTR_ErrorWritingFile "%s: Fehler beim Schreiben der Datei.\n"
#define MSGTR_WritingAVIIndex "\nSchreibe AVI Index...\n"
#define MSGTR_FixupAVIHeader "Fixiere AVI Header...\n"
#define MSGTR_RecommendedVideoBitrate "Empfohlene Videobitrate für %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nVideostream: %8.3f kbit/s  (%d bps)  Größe: %d Bytes  %5.3f Sek.  %d Frames\n"
#define MSGTR_AudioStreamResult "\nAudiostream: %8.3f kbit/s  (%d bps)  Größe: %d Bytes  %5.3f Sek.\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM Gerät '%s' nicht gefunden!\n"
#define MSGTR_ErrTrackSelect "Fehler beim Auswählen des VCD Tracks!"
#define MSGTR_ReadSTDIN "Lese von stdin...\n"
#define MSGTR_UnableOpenURL "Kann URL nicht öffnen: %s\n"
#define MSGTR_ConnToServer "Verbunden mit Server: %s\n"
#define MSGTR_FileNotFound "Datei nicht gefunden: '%s'\n"

#define MSGTR_CantOpenDVD "Kann DVD Gerät nicht öffnen: %s\n"
#define MSGTR_DVDwait "Lese Disk-Struktur, bitte warten...\n"
#define MSGTR_DVDnumTitles "Es sind %d Titel auf dieser DVD.\n"
#define MSGTR_DVDinvalidTitle "Ungültige DVD Titelnummer: %d\n"
#define MSGTR_DVDnumChapters "Es sind %d Kapitel auf diesem DVD Titel.\n"
#define MSGTR_DVDinvalidChapter "Ungültige DVD Kapitelnummer: %d\n"
#define MSGTR_DVDnumAngles "Es sind %d Sequenzen auf diesem DVD Titel.\n"
#define MSGTR_DVDinvalidAngle "Ungültige DVD Sequenznummer: %d\n"
#define MSGTR_DVDnoIFO "Kann die IFO-Datei für den DVD-Titel nicht öffnen %d.\n"
#define MSGTR_DVDnoVOBs "Kann Titel-VOBS (VTS_%02d_1.VOB) nicht öffnen.\n"
#define MSGTR_DVDopenOk "DVD erfolgreich geöffnet!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Warnung! Audiostreamheader %d redefiniert!\n"
#define MSGTR_VideoStreamRedefined "Warnung! Videostreamheader %d redefiniert!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Zu viele (%d in %d bytes) Audiopakete im Puffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Zu viele (%d in %d bytes) Videopakete im Puffer!\n"
#define MSGTR_MaybeNI "Vielleicht spielst du einen non-interleaved Stream/Datei oder der Codec funktioniert nicht.\n" \
                     "Versuche für .AVI Dateien den nicht-interleaved Modus mit der Option -ni zu erzwingen\n"
#define MSGTR_SwitchToNi "\nSchlecht Interleaved .AVI erkannt, schalte in das -ni Modus!\n"
#define MSGTR_DetectedFILMfile "FILM Dateiformat erkannt!\n"
#define MSGTR_DetectedFLIfile "FLI Dateiformat erkannt!\n"
#define MSGTR_DetectedROQfile "RoQ Dateiformat erkannt!\n"
#define MSGTR_DetectedREALfile "REAL Dateiformat erkannt!\n"
#define MSGTR_DetectedAVIfile "AVI Dateiformat erkannt!\n"
#define MSGTR_DetectedASFfile "ASF Dateiformat erkannt!\n"
#define MSGTR_DetectedMPEGPESfile "MPEG-PES Dateiformat erkannt!\n"
#define MSGTR_DetectedMPEGPSfile "MPEG-PS Dateiformat erkannt!\n"
#define MSGTR_DetectedMPEGESfile "MPEG-ES Dateiformat erkannt!\n"
#define MSGTR_DetectedQTMOVfile "QuickTime/MOV Dateiformat erkannt!\n"
#define MSGTR_DetectedYUV4MPEG2file "YUV4MPEG2 Dateiformat erkannt!\n"
#define MSGTR_DetectedNuppelVideofile "NuppelVideo Dateiformat erkannt!\n"
#define MSGTR_DetectedVIVOfile "VIVO Dateiformat erkannt!\n"
#define MSGTR_DetectedBMPfile "BMP Dateiformat erkannt!\n"
#define MSGTR_DetectedOGGfile "OGG Dateiformat erkannt!\n"
#define MSGTR_DetectedRAWDVfile "RAWDV Dateiformat erkannt!\n"
#define MSGTR_DetectedAudiofile "Audiodatei erkannt!\n"
#define MSGTR_NotSystemStream "Kein MPEG System Stream ... (vielleicht Transport Stream?)\n"
#define MSGTR_MissingMpegVideo "Vermisse MPEG Videostream!? Kontaktiere den Author, das könnte ein Bug sein :(\n"
#define MSGTR_InvalidMPEGES "Ungültiger MPEG-ES Stream??? Kontaktiere den Author, das könnte ein Bug sein :(\n"
#define MSGTR_FormatNotRecognized "=========== Sorry, das Dateiformat/Codec wird nicht unterstützt ==============\n"\
				  "============== Sollte dies ein AVI, ASF oder MPEG Stream sein, ===============\n"\
				  "================== dann kontaktiere bitte den Author =========================\n"
#define MSGTR_MissingVideoStream "Kann keinen Videostream finden!\n"
#define MSGTR_MissingAudioStream "Kann keinen Audiostream finden...  -> kein Ton\n"
#define MSGTR_MissingVideoStreamBug "Vermisse Videostream!? Kontaktiere den Author, möglicherweise ein Bug :(\n"

#define MSGTR_DoesntContainSelectedStream "Demuxer: Datei enthält nicht den gewählen Audio- oder Videostream\n"

#define MSGTR_NI_Forced "Erzwungen"
#define MSGTR_NI_Detected "Erkannt"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI Dateiformat!\n"

#define MSGTR_UsingNINI "Verwende defektes NON-INTERLEAVED AVI Dateiformat!\n"
#define MSGTR_CouldntDetFNo "Konnte die Anzahl der Frames (für absulute Suche) nicht finden  \n"
#define MSGTR_CantSeekRawAVI "Kann keine RAW .AVI-Streams durchsuchen! (Index erforderlich, versuche es mit der -idx Option!)  \n"
#define MSGTR_CantSeekFile "Kann diese Datei nicht durchsuchen!  \n"

#define MSGTR_EncryptedVOB "Verschlüsselte VOB-Datei (wurde ohne libcss Unterstützung kompiliert)! Lese DOCS\n"
#define MSGTR_EncryptedVOBauth "Verschlüsselter Stream, jedoch wurde die Authentifizierung nicht von Dir gefordert!!\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimierte Header werden (zur Zeit) nicht unterstützt!\n"
#define MSGTR_MOVvariableFourCC "MOV: Warnung! Variable FOURCC erkannt!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Warnung! Zu viele Tracks!"
#define MSGTR_MOVnotyetsupp "\n******** Quicktime MOV Format wird zu Zeit nicht unterstützt!!!!!!! *********\n"
#define MSGTR_FoundAudioStream "==> Audiostream gefunden: %d\n"
#define MSGTR_FoundVideoStream "==> Videostream gefunden: %d\n"
#define MSGTR_DetectedTV "TV festgestellt! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Öffnen des OGG Demuxers fehlgeschlagen\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Suche nach Audiostream (Id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Kann Audiostream nicht öffnen: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Kann Untertitelstream nicht öffnen: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Öffnen des Audiodemuxers fehlgeschlagen: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Öffnen des Untertiteldemuxers fehlgeschlagen: %s\n"
#define MSGTR_TVInputNotSeekable "TV-Input ist nicht durchsuchbar! (möglicherweise änderst du damit den Kanal ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Demuxerinfo %s existiert bereits\n!"
#define MSGTR_ClipInfo "Clipinfo: \n"

				  
// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "kann Codec nicht öffnen\n"
#define MSGTR_CantCloseCodec "kann Codec nicht schließen\n"

#define MSGTR_MissingDLLcodec "FEHLER: Kann erforderlichen DirectShow Codec nicht finden: %s\n"
#define MSGTR_ACMiniterror "Kann Win32/ACM AUDIO Codec nicht finden (fehlende DLL-Datei?)\n"
#define MSGTR_MissingLAVCcodec "Kann Codec '%s' von libavcodec nicht finden...\n"

#define MSGTR_NoDShowSupport "MPlayer wurde OHNE DirectShow Unterstützung kompiliert!\n"
#define MSGTR_NoWfvSupport "Unterstützung für Win32 Codecs ausgeschaltet oder nicht verfügbar auf nicht-x86 Plattformen!\n"
#define MSGTR_NoDivx4Support "MPlayer wurde OHNE DivX4Linux (libdivxdecore.so) Unterstützung kompiliert!\n"
#define MSGTR_NoLAVCsupport "MPlayer wurde OHNE ffmpeg/libavcodec Unterstützung kompiliert!\n"
#define MSGTR_NoACMSupport "Win32/ACM Audiocodecs ausgeschaltet oder nicht verfügbar auf nicht-x86 Plattformen -> erzwinge -nosound :(\n"
#define MSGTR_NoDShowAudio "MPlayer wurde ohne DirectShow Unterstützung kompiliert -> erzwinge -nosound :(\n"
#define MSGTR_NoOggVorbis "OggVorbis Audiocodec ausgeschaltet -> erzwinge -nosound :(\n"
#define MSGTR_NoXAnimSupport "MPlayer wurde ohne XAnim Unterstützung kompiliert!\n"

#define MSGTR_MpegPPhint "WARNUNG! Du hast Bild-Postprocessing erbeten für ein MPEG 1/2 Video,\n" \
			 "         aber Du hast MPlayer ohne MPEG 1/2 Postprocessing-Support kompiliert!\n" \
			 "         #define MPEG12_POSTPROC in config.h und kompiliere libmpeg2 neu!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: Ende der Datei während der Suche für Sequenzheader\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Kann Sequenzheader nicht lesen!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Kann Sequenzheader-Erweiterung nicht lesen!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Schlechte Sequenzheader!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Schlechte Sequenzheader-Erweiterung!\n"

#define MSGTR_ShMemAllocFail "Kann keinen gemeinsamen Speicher zuweisen\n"
#define MSGTR_CantAllocAudioBuf "Kann keinen Audioausgabe-Puffer zuweisen\n"
#define MSGTR_NoMemForDecodedImage "nicht genug Speicher für den Puffer der dekodierten Bilder (%ld Bytes)\n"

#define MSGTR_AC3notvalid "AC3 Stream ungültig.\n"
#define MSGTR_AC3only48k "Nur 48000 Hz Streams werden unterstützt.\n"
#define MSGTR_UnknownAudio "Unbekanntes/fehlendes Audioformat -> kein Ton\n"

#define MSGTR_UsingExternalPP "[PP] Verwende externe Postprocessing Filter, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Verwende Postprocessing des Codecs, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Videoeigenschaft '%s' wird nicht unterstützt vom ausgewählen vo & vd! \n"
#define MSGTR_VideoCodecFamilyNotAvailable "Erforderliche Videocodec Familie [%s] (vfm=%d) nicht verfügbar (aktiviere sie beim Kompileren!)\n"
#define MSGTR_AudioCodecFamilyNotAvailable "Erforderliche Audiocodec Familie [%s] (afm=%d) nicht verfügbar (aktiviere sie beim Kompileren!)\n"
#define MSGTR_OpeningVideoDecoder "Öffne Videodecoder: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Öffne Audiodecoder: [%s] %s\n"
#define MSGTR_UninitVideo "Uninitialisiere Video: %d  \n"
#define MSGTR_UninitAudio "Uninitialisiere Audio: %d  \n"
#define MSGTR_VDecoderInitFailed "Initialisierung des Videodecoder fehlgeschlagen :(\n"
#define MSGTR_ADecoderInitFailed "Initialisierung des Audiodecoder fehlgeschlagen :(\n"
#define MSGTR_ADecoderPreinitFailed "Preinitialisierung des Audiodecoder fehlgeschlagen :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Reserviere %d Bytes für Eingangsbuffer\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Reserviere %d + %d = %d Bytes füs Ausgabebuffer\n"
			 
// LIRC:
#define MSGTR_SettingUpLIRC "Initialisiere LIRC Unterstützung...\n"
#define MSGTR_LIRCdisabled "Verwenden der Fernbedienung nicht möglich\n"
#define MSGTR_LIRCopenfailed "Fehler beim Öffnen der LIRC Unterstützung!\n"
#define MSGTR_LIRCsocketerr "Fehler im LIRC Socket: %s\n"
#define MSGTR_LIRCcfgerr "Kann LIRC Konfigurationsdatei nicht lesen %s !\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Konnte Videofilter '%s' nicht finden\n"
#define MSGTR_CouldNotOpenVideoFilter "Konnte Videofilter '%s' nicht öffnen\n"
#define MSGTR_OpeningVideoFilter "Öffne Videofilter: [%s=%s]\n"
#define MSGTR_OpeningVideoFilter2 "Öffne Videofilter: [%s]\n"
#define MSGTR_CannotFindColorspace "Konnte kein allgemeines Colorspace-Format finden, auch nicht mithilfe von 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Codec hat sh->disp_w und sh->disp_h nicht gesetzt, versuche zu umgehen!\n"
#define MSGTR_VoConfigRequest "VDec: VO wird versucht auf %d x %d (Bevorzugter Colorspace: %s) zu setzen\n"
#define MSGTR_CouldNotFindColorspace "Kann keinen passenden Colorspace finden - versuche erneut mithilfe von -vop scale...\n"
#define MSGTR_MovieAspectIsSet "Movie-Aspect ist %.2f:1 - Prescaling zur korrekten Videogröße.\n"
#define MSGTR_MovieAspectUndefined "Movie-Aspect ist undefiniert - kein Prescaling verwendet.\n"
			 

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Über ..."
#define MSGTR_FileSelect "Wähle Datei ..."
#define MSGTR_SubtitleSelect "Wähle Untertitel ..."
#define MSGTR_OtherSelect "Wähle ..."
#define MSGTR_AudioFileSelect "Wähle externen Audiokanal ..."
#define MSGTR_FontSelect "Wähle Schrift ..."
#define MSGTR_MessageBox "Message-Box"
#define MSGTR_PlayList "Playlist"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Skin-Browser"
#define MSGTR_Network "Netzwerk-Streaming ..."
#define MSGTR_Preferences "Einstellungen"
#define MSGTR_OSSPreferences "OSS Treibereinstellungen"
			 

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
#define MSGTR_NEMFMM "Sorry, nicht genug Speicher für die Hauptfenster-Maske."
#define MSGTR_IDFGCVD "Sorry, kann keinen GUI-kompatiblen Ausgabetreiber finden"
			 
// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[Skin] Fehler in Skin-Konfigurationsdatei in Zeile %d: %s" 
#define MSGTR_SKIN_WARNING1 "[Skin] Warnung in Skin-Konfigurationsdatei in Zeile %d: Widget gefunden, aber davor wurde \"section\" nicht gefunden ( %s )"
#define MSGTR_SKIN_WARNING2 "[Skin] Warnung in Skin-Konfigurationsdatei in Zeile %d: Widget gefunden, aber davor wurde \"subsection\" nicht gefunden (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "Bitmaps mit 16 Bits oder weniger werden nicht unterstützt ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "Datei nicht gefunden ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP Lesefehler ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA Lesefehler ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG Lesefehler ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE gepackte TGA werden nicht unterstützt ( %s )\n"
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
#define MSGTR_MENU_LoadExternAudioFile "Lade externe Audiodatei ..."
#define MSGTR_MENU_Playing "Spiele"
#define MSGTR_MENU_Play "Spiele"
#define MSGTR_MENU_Pause "Pause"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Nächster Stream"
#define MSGTR_MENU_PrevStream "Verheriger Stream"
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
#define MSGTR_MENU_PlayList "Playliste"
#define MSGTR_MENU_SkinBrowser "Skinbrowser"
#define MSGTR_MENU_Preferences "Einstellungen"
#define MSGTR_MENU_Exit "Beenden ..."

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

// --- playlist
#define MSGTR_PLAYLIST_Path "Pfad"
#define MSGTR_PLAYLIST_Selected "ausgewählte Dateien"
#define MSGTR_PLAYLIST_Files "Dateien"
#define MSGTR_PLAYLIST_DirectoryTree "Verzeichnisbaum"

// --- preferences
#define MSGTR_PREFERENCES_None "Nichts"
#define MSGTR_PREFERENCES_Codec1 "Verwende VFW (Win32) Codecs"
#define MSGTR_PREFERENCES_Codec2 "Verwende OpenDivX/DivX4 Codec (YV12)"
#define MSGTR_PREFERENCES_Codec3 "Verwende DirectShow (Win32) Codecs"
#define MSGTR_PREFERENCES_Codec4 "Verwende ffmpeg (libavcodec) Codecs"
#define MSGTR_PREFERENCES_Codec5 "Verwende DivX4 Codec (YUY2)"
#define MSGTR_PREFERENCES_Codec6 "Verwende XAnim Codecs"
#define MSGTR_PREFERENCES_AvailableDrivers "Verfügbare Treiber:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Spiele keinen Ton"
#define MSGTR_PREFERENCES_NormalizeSound "Normalisiere Ton"
#define MSGTR_PREFERENCES_EnEqualizer "Equalizer verwenden"
#define MSGTR_PREFERENCES_ExtraStereo "Extra Stereo verwenden"
#define MSGTR_PREFERENCES_Coefficient "Koeffizient:"
#define MSGTR_PREFERENCES_AudioDelay "Audio-Verzögerung"
#define MSGTR_PREFERENCES_Audio "Audio"
#define MSGTR_PREFERENCES_VideoEqu "Video Equalizer verwenden"
#define MSGTR_PREFERENCES_DoubleBuffer "Double-Buffering verwenden"
#define MSGTR_PREFERENCES_DirectRender "Direct-Rendering verwenden"
#define MSGTR_PREFERENCES_FrameDrop "Frame-Dropping verwenden"
#define MSGTR_PREFERENCES_HFrameDrop "HARD Frame-Dropping verwenden ( gefährlich )"
#define MSGTR_PREFERENCES_Flip "Bild spiegeln"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_OSDTimer "Zeit und Indikatoren"
#define MSGTR_PREFERENCES_OSDProgress "nur Progressbar"
#define MSGTR_PREFERENCES_Subtitle "Untertitel:"
#define MSGTR_PREFERENCES_SUB_Delay "Verzögerung: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Position: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Automatisches Laden der Untertitel ausschalten"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode Subtitle"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konvertiere Untertitel zum MPlayer Untertitelformat"
#define MSGTR_PREFERENCES_SUB_SRT "Konvertiere Untertitel zum zeitbasierenden SubViewer (SRT) Untertitelformat"
#define MSGTR_PREFERENCES_Font "Schrift:"
#define MSGTR_PREFERENCES_FontFactor "Schriftfaktor:"
#define MSGTR_PREFERENCES_PostProcess "Postprocess verwenden"
#define MSGTR_PREFERENCES_AutoQuality "Auto-Qualität: "
#define MSGTR_PREFERENCES_NI "Non-Interleaved AVI Parser verwenden"
#define MSGTR_PREFERENCES_IDX "Index Table neu aufbauen, falls benötigt"
#define MSGTR_PREFERENCES_VideoCodecFamily "Videocodec Familie:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD Level"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Untertitel"
#define MSGTR_PREFERENCES_FRAME_Font "Schrift"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocess"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & Demuxer"
#define MSGTR_PREFERENCES_OSS_Device "Gerät:"
#define MSGTR_PREFERENCES_OSS_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Message "Bitte bedenke, mache Funktionen erfordern einen Neustart der Wiedergabe."
			 
// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Fataler Fehler ..."
#define MSGTR_MSGBOX_LABEL_Error "Fehler ..."
#define MSGTR_MSGBOX_LABEL_Warning "Warnung ..."

#endif
