// Translated by:  ???
// NOTE: Not fully translated, still has many english messages!

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
#define MSGTR_Exit_error "Schwerer Fehler"
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

#define MSGTR_NoGui "MPlayer wurde ohne GUI-Unterstützung kompiliert!\n"
#define MSGTR_GuiNeedsX "Das MPlayer GUI benötigt X11!\n"
#define MSGTR_Playing "Spiele ab %s\n"
#define MSGTR_NoSound "Audio: Kein Ton!!!\n"
#define MSGTR_FPSforced "FPS festgelegt auf %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM Gerät '%s' nicht gefunden!\n"
#define MSGTR_ErrTrackSelect "Fehler beim auswählen des VCD Tracks!"
#define MSGTR_ReadSTDIN "Lese von stdin...\n"
#define MSGTR_UnableOpenURL "Kann URL nicht öffnen: %s\n"
#define MSGTR_ConnToServer "Verbunden mit Server: %s\n"
#define MSGTR_FileNotFound "Datei nicht gefunden: '%s'\n"

// demuxer.c, demux.c:
#define MSGTR_AudioStreamRedefined "Warnung! Audiostreamheader %d redefiniert!\n"
#define MSGTR_VideoStreamRedefined "WarnUng! Aideostreamheader %d redefiniert!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Zu viele (%d in %d bytes) Audiopakete im Puffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Zu viele (%d in %d bytes) Videopakete im Puffer\n"
#define MSGTR_MaybeNI "Vielleicht spielst du einen non-interleaved Stream/Datei oder der Codec geht nicht\n"
#define MSGTR_DetectedAVIfile "AVI Dateiformat erkannt!\n"
#define MSGTR_DetectedASFfile "ASF Dateiformat erkannt!\n"
#define MSGTR_DetectedMPEGPESfile "MPEG-PES Dateiformat erkannt!\n"
#define MSGTR_DetectedMPEGPSfile "MPEG-PS Dateiformat erkannt!\n"
#define MSGTR_DetectedMPEGESfile "MPEG-ES Dateiformat erkannt!\n"
#define MSGTR_DetectedQTMOVfile "QuickTime/MOV Dateiformat erkannt!\n"
#define MSGTR_MissingMpegVideo "Vermisse MPEG Videostream!? Kontaktiere den Author, das könnte ein Bug sein :(\n"
#define MSGTR_InvalidMPEGES "Ungültiger MPEG-ES Stream??? Kontaktiere den Author, das könnte ein Bug sein :(\n"
#define MSGTR_FormatNotRecognized \
"=========== Sorry, das Dateiformat/Codec wird nicht unterstützt =============\n"\
"============== Sollte dies ein AVI, ASF oder MPEG Stream sein, ==============\n"\
"================== dann kontaktiere bitte den Author ========================\n"
#define MSGTR_MissingASFvideo "ASF: kann keinen Videostream finden\n"
#define MSGTR_MissingASFaudio "ASF: kann keinen Audiostream finden...  ->Stummfilm\n"
#define MSGTR_MissingMPEGaudio "MPEG: kann keinen Audiostream finden...  ->Stummfilm\n"
//#define MSGTR_
#define MSGTR_MissingVideoStream "Kein Videostream gefunden!\n"
#define MSGTR_MissingAudioStream "Kein Audiostream gefunden...  ->kein Sound\n"
#define MSGTR_MissingVideoStreamBug "Fehlender Videostream!? Verständige den Author, es könnte ein Fehler sein :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Datei enthält nicht die/den angegebene(n) Audio- oder Videostream(s)\n"

#define MSGTR_NI_Forced "Erzwungen"
#define MSGTR_NI_Detected "Erkannt"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI Dateiformat!\n"

#define MSGTR_UsingNINI "Benutze NON-INTERLEAVED \"Defekte AVI\" Dateiformat!\n"
#define MSGTR_CouldntDetFNo "Kann nicht die Anzahl der Einzelbilder bestimmen (für Suchen einer absoluten Position)  \n"
#define MSGTR_CantSeekRawAVI "Kann nicht in bloßen raw .AVI streams suchen! (Index wird benötigt, versuche die -idx Option!)  \n"
#define MSGTR_CantSeekFile "Kann in dieser Datei nicht suchen!  \n"

#define MSGTR_EncryptedVOB "Verschlüsselte VOB Datei (ohne lib[dvd]css kompiliert)! Lese DOCS/DVD.\n"
#define MSGTR_EncryptedVOBauth "Verschlüsselter Stream, aber Authentizierung wurde nicht angefordert!!\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimierte Header (noch) nicht unterstützt!\n"
#define MSGTR_MOVvariableFourCC "MOV: Warnung! variabler FOURCC erkannt!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Warnung! zu viele Spuren!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV Format noch nicht unterstützt!!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Kann Codec nicht öffnen\n"
#define MSGTR_CantCloseCodec "Kann Codec nicht schließen\n"

#define MSGTR_MissingDLLcodec "FEHLER: Konnte erforderlichen DirectShow Kodec nicht öffnen: %s\n"
#define MSGTR_ACMiniterror "Konnte Win32/ACM AUDIO Codec nicht laden/initialisieren (Fehlende DLL Datei?)\n"
#define MSGTR_MissingLAVCcodec "Konnte den Codec '%s' in libavcodec nicht finden...\n"

#define MSGTR_NoDShowSupport "MPlayer wurde OHNE DirectShow Unterstützung kompiliert!\n"
#define MSGTR_NoWfvSupport "Unterstützung für win32 Codecs abgeschaltet, oder nicht vorhanden auf nicht-x86-konformen Rechnern!\n"
#define MSGTR_NoDivx4Support "MPlayer wurde OHNE DivX4Linux (libdivxdecore.so) Unterstützung kompiliert!\n"
#define MSGTR_NoLAVCsupport "MPlayer wurde OHNE ffmpeg/libavcodec Unterstützung kompiliert!\n"
#define MSGTR_NoACMSupport "Win32/ACM Audio Codec abgeschaltet, oder nicht vorhanden auf nicht-x86-konformen Prozessoren -> Gezwungenermaßen kein Sound :(\n"#define MSGTR_NoDShowAudio "Ohne DirectShow Unterstützung kompiliert -> Gezwungenermaßen kein Sound :(\n"
#define MSGTR_NoOggVorbis "OggVorbis Audio Codec abgeschaltet -> Gezwungenermaßen kein Sound  :(\n"

#define MSGTR_MpegPPhint "WARNUNG! Sie haben Bildverbesserung für ein MPEG 1/2 Video gewählt, aber\n" \
			 "         MPlayer ohne MPEG 1/2 Bildverbesserungsunterstützung kompiliert!\n" \
			 "         #define MPEG12_POSTPROC in config.h, und kompiliere libmpeg2 neu!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: Dateiende während der Suche nach Sequenz-Header!\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Kann Sequenz-Header nicht lesen!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Kann Sequenz-Header-Erweiterung nicht lesen!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Fehlerhafter Sequenz-Header!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Fehlerhafte Sequenz-Header-Erweiterung!\n"

#define MSGTR_ShMemAllocFail "Kann Shared Memory nicht freigeben!\n"
#define MSGTR_CantAllocAudioBuf "Kann Audioausgabe-Puffer nicht freigeben!\n"
#define MSGTR_NoMemForDecodedImage "Nicht genug Speicher für Bildzwischenspeicherung (%ld bytes)\n"

#define MSGTR_AC3notvalid "AC3 Stream fehlerhaft.\n"
#define MSGTR_AC3only48k "Nur 48000 Hz Streams unterstützt.\n"
#define MSGTR_UnknownAudio "Unbekanntes/Fehlendes Audio-Format, keine Audio-Ausgabe!\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Initialisiere Lirc Unterstützung...\n"
#define MSGTR_LIRCdisabled "Fernbedienung funktioniert nicht\n"
#define MSGTR_LIRCopenfailed "Fehler beim Öffnen der Lirc Unterstützung!\n"
#define MSGTR_LIRCsocketerr "Fehler im Lirc Socket: %s\n"
#define MSGTR_LIRCcfgerr "Kann LIRC Konfigurationsdatei nicht lesen %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Über"
#define MSGTR_FileSelect "Wähle Datei ..."
#define MSGTR_MessageBox "MessageBox"
#define MSGTR_PlayList "Playlist"
#define MSGTR_SkinBrowser "Skin Browser"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Abbrechen"
#define MSGTR_Add "Hinzufügen"
#define MSGTR_Remove "Entfernen"

// --- error messages ---
#define MSGTR_NEMDB "Sorry, not enough memory for draw buffer."
#define MSGTR_NEMFMR "Sorry, not enough memory for menu rendering."
#define MSGTR_NEMFMM "Sorry, not enough memory for main window shape mask."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] error in skin config file on line %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] warning in skin config file on line %d: widget found but before \"section\" not found ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] warning in skin config file on line %d: widget found but before \"subsection\" not found ( %s )"
#define MSGTR_SKIN_BITMAP_16bit  "16 bits or less depth bitmap not supported ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "file not found ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "bmp read error ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "tga read error ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "png read error ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE packed tga not supported ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "unknown file type ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "24 bit to 32 bit convert error ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "unknown message: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "not enought memory\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "too many fonts declared\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "font file not found\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "font image file not found\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "non-existent font identifier ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "unknown parameter ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] not enought memory.\n"

#endif
