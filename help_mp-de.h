#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2001 Arpad Gereoffy (siehe DOCS/AUTHORS)\n"
"\n";

static char help_text[]=
"Benutzung:   mplayer [optionen] [verzeichnis/]dateiname\n"
"\n"
"Optionen:\n"
" -vo <drv[:dev]> Videoausgabetreiber & -Gerät (siehe '-vo help' für eine Liste)\n"
" -ao <drv[:dev]> Audioausgabetreiber & -Gerät (siehe '-ao help' für eine Liste)\n"
" -vcd <tracknr>  Spiele VCD (video cd) Titel anstelle eines Dateinames\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  Benutze DVD Gerät für die Authentifizierung (für verschl. DVD's)\n"
#endif
" -ss <timepos>   Starte abspielen ab Position (Sekunden oder hh:mm:ss)\n"
" -nosound        Spiele keinen Sound\n"
#ifdef USE_FAKE_MONO
" -stereo         Auswahl der MPEG1-Stereoausgabe (0:stereo 1:links 2:rechts)\n"
#endif
" -fs -vm -zoom   Vollbild Optionen (Vollbild, Videomode, Softwareskalierung)\n"
" -x <x> -y <y>   Skaliere Bild auf <x> * <y> [wenn vo-Treiber mitmacht]\n"
" -sub <file>     Benutze Untertitledatei (siehe auch -subfps, -subdelay)\n"
" -vid x -aid y   Spiele Videostream (x) und Audiostream (y)\n"
" -fps x -srate y Benutze Videoframerate (x fps) und Audiosamplingrate (y Hz)\n"
" -pp <quality>   Aktiviere Nachbearbeitungsfilter (0-4 bei DivX, 0-63 bei MPEG)\n"
" -bps            Benutze alternative A-V sync Methode für AVI's (könnte helfen!)\n"
" -framedrop      Benutze frame-dropping (für langsame Rechner)\n"
"\n"
"Tasten:\n"
" <- oder ->      Springe zehn Sekunden vor/zurück\n"
" rauf / runter   Springe eine Minute vor/zurück\n"
" p or LEER       PAUSE (beliebige Taste zum fortsetzen)\n"
" q or ESC        Abspielen stoppen und Programm beenden\n"
" + or -          Audioverzögerung um +/- 0.1 Sekunde verändern\n"
" o               OSD Mode:  Aus / Zuchleiste / Zuchleiste+Zeit\n"
" * or /          Lautstärke verstellen ('m' für Auswahl Master/Wave)\n"
" z or x          Untertitelverzögerung um +/- 0.1 Sekunde verändern\n"
"\n"
" * * * IN DER MANPAGE STEHEN WEITERE KEYS UND OPTIONEN ! * * *\n"
"\n";
#endif

// mplayer.c: 

#define MSGTR_Exiting "\nBeende... (%s)\n"
#define MSGTR_Exit_frames "Angeforderte Anzahl an Frames gespielt"
#define MSGTR_Exit_quit "Ende"
#define MSGTR_Exit_eof "Ende der Datei"
#define MSGTR_Exit_error "Fehler"
#define MSGTR_IntBySignal "\nMPlayer wurde durch Signal %d von Modul %s beendet\n"
#define MSGTR_NoHomeDir "Kann Homeverzeichnis nicht finden\n"
#define MSGTR_GetpathProblem "get_path(\"config\") Problem\n"
#define MSGTR_CreatingCfgFile "Erstelle Konfigurationsdatei: %s\n"
#define MSGTR_InvalidVOdriver "Ungültiger Videoausgabetreibername: %s\n'-vo help' zeigt eine Liste aller.\n"
#define MSGTR_InvalidAOdriver "Ungültiger Audioausgabetreibername: %s\n'-vo help' zeigt eine Liste aller.\n"
#define MSGTR_CopyCodecsConf "(kopiere/linke etc/codecs.conf nach ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Kann Schriftdatei %s nicht laden\n"
#define MSGTR_CantLoadSub "Kann Untertitel nicht laden: %s\n"
#define MSGTR_ErrorDVDkey "Fehler beim bearbeiten des DVD-Schlüssels.\n"
#define MSGTR_CmdlineDVDkey "der DVD-Schlüssel der Kommandozeile wurde für das descrambeln gespeichert.\n"
#define MSGTR_DVDauthOk "DVD Authentifizierungssequenz scheint OK zu sein.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: Ausgewählter Stream fehlt!\n"
#define MSGTR_CantOpenDumpfile "Kann dump-Datei nicht öffnen!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS ist im Header nicht angegeben (oder ungültig)! Benutze -fps Option!\n"
#define MSGTR_NoVideoStream "Sorry, kein video stream... ist unabspielbar\n"
#define MSGTR_TryForceAudioFmt "Erzwinge Audiocodecgruppe %d ...\n"
#define MSGTR_CantFindAfmtFallback "Kann keinen Audiocodec für gewünschte Gruppe finden, verwende anderen\n"
#define MSGTR_CantFindAudioCodec "Kann Codec für Audioformat 0x%X nicht finden!\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Versuche %s mit etc/codecs.conf zu erneuern\n*** Sollte es weiterhin nicht gehen, dann lese DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Kann Audiocodec nicht finden! -> Stummfilm\n"
#define MSGTR_TryForceVideoFmt "Erzwinge Videocodecgruppe %d ...\n"
#define MSGTR_CantFindVfmtFallback "Kann keinen Videocodec für gewünschte Gruppe finden, verwende anderen\n"
#define MSGTR_CantFindVideoCodec "Kann Videocodec für Format 0x%X nicht finden!\n"
#define MSGTR_VOincompCodec "Sorry, der ausgewählte Videoausgabetreiber ist nicht kompatibel mit diesem Codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Kann Videocodec nicht initialisieren :(\n"
#define MSGTR_EncodeFileExists "Datei existiert: %s (überschreibe nicht deine schönsten AVI's!)\n"
#define MSGTR_CantCreateEncodeFile "Kann Datei zum Encoden nicht öffnen\n"
#define MSGTR_CannotInitVO "FATAL: Kann Videoausgabetreiber nicht initialisieren!\n"
#define MSGTR_CannotInitAO "Kann Audiotreiber/Soundkarte nicht initialisieren -> Stummfilm\n"
#define MSGTR_StartPlaying "Starte Abspielen...\n"
#define MSGTR_SystemTooSlow "\n************************************************************************"\
			    "\n* Dein System ist zu langsam. Versuche die -framedrop Option oder RTFM!*"\
			    "\n************************************************************************\n"
//#define MSGTR_

// open.c: 
#define MSGTR_CdDevNotfound "CD-ROM Gerät '%s' nicht gefunden!\n"
#define MSGTR_ErrTrackSelect "Fehler beim auswählen des VCD Tracks!"
#define MSGTR_ReadSTDIN "Lese von stdin...\n"
#define MSGTR_UnableOpenURL "Kann URL nicht öffnen: %s\n"
#define MSGTR_ConnToServer "Verbunden mit Server: %s\n"
#define MSGTR_FileNotFound "Datei nicht gefunden: '%s'\n"

// demuxer.c:
#define MSGTR_AudioStreamRedefined "Warnung! Audiostreamheader %d redefiniert!\n"
#define MSGTR_VideoStreamRedefined "WarnUng! Aideostreamheader %d redefiniert!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Zu viele (%d in %d bytes) Audiopakete im Puffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Zu viele (%d in %d bytes) Videopakete im Puffer\n"
#define MSGTR_MaybeNI "Vielleicht spielst du einen non-interleaved Stream/Datei oder der Codec geht nicht\n";
#define MSGTR_DetectedAVIfile "AVI Dateiformat erkannt!\n"
#define MSGTR_DetectedASFfile "ASF Dateiformat erkannt!\n"
#define MSGTR_DetectedMPEGPESfile "MPEG-PES Dateiformat erkannt!\n"
#define MSGTR_DetectedMPEGPSfile "MPEG-PS Dateiformat erkannt!\n"
#define MSGTR_DetectedMPEGESfile "MPEG-ES Dateiformat erkannt!\n"
#define MSGTR_DetectedQTMOVfile "QuickTime/MOV Dateiformat erkannt!\n"
#define MSGTR_MissingMpegVideo "Vermisse MPEG Videostream!? Kontaktiere den Author, das könnte ein Bug sein :(\n"
#define MSGTR_InvalidMPEGES "Ungültiger MPEG-ES Stream??? Kontaktiere den Author, das könnte ein Bug sein :(\n"
#define MSGTR_FormatNotRecognized =\
"=========== Sorry, das Dateiformat/Codec wird nicht unterstützt =============\n"\
"============== Sollte dies ein AVI, ASF oder MPEG Stream sein, ==============\n"\
"================== dann kontaktiere bitte den Author ========================\n"
#define MSGTR_MissingASFvideo "ASF: kann keinen Videostream finden\n"
#define MSGTR_MissingASFaudio "ASF: kann keinen Audiostream finden...  ->Stummfilm\n"
#define MSGTR_MissingMPEGaudio "MPEG: kann keinen Audiostream finden...  ->Stummfilm\n"

//#define MSGTR_


