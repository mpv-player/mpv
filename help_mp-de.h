// Transated by: Johannes Feigl, johannes.feigl@mcse.at
// Overworked by Klaus Umbach, klaus.umbach@gmx.net

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (siehe DOCS!)\n"
"\n";

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Verwendung:   mplayer [-gui] [optionen] [verzeichnis/]dateiname\n"
#else
"Verwendung:   mplayer [optionen] [verzeichnis/]dateiname\n"
#endif
"\n"
"Optionen:\n"
" -vo <drv[:dev]> Videoausgabetreiber & -Gerät (siehe '-vo help' für eine Liste)\n"
" -ao <drv[:dev]> Audioausgabetreiber & -Gerät (siehe '-ao help' für eine Liste)\n"
" -vcd <tracknr>  Spiele VCD (Video CD) Titel anstelle eines Dateinames\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  Benutze DVD Gerät für die Authentifizierung (für verschl. DVD's)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titlenr>  Spiele DVD Titel/Track von Gerät anstelle eines Dateinames\n"
#endif
" -ss <timepos>   Starte Abspielen ab Position (Sekunden oder hh:mm:ss)\n"
" -nosound        Spiele keinen Sound\n"
#ifdef USE_FAKE_MONO
" -stereo <mode>  Auswahl der MPEG1-Stereoausgabe (0:stereo 1:links 2:rechts)\n"
#endif
" -fs -vm -zoom   Vollbild Optionen (Vollbild, Videomode, Softwareskalierung)\n"
" -x <x> -y <y>   Skaliere Bild auf <x> * <y> [wenn vo-Treiber mitmacht]\n"
" -sub <file>     Benutze Untertitledatei (siehe auch -subfps, -subdelay)\n"
" -vid x -aid y   Spiele Videostream (x) und Audiostream (y)\n"
" -fps x -srate y Benutze Videoframerate (x fps) und Audiosamplingrate (y Hz)\n"
" -pp <quality>   Aktiviere Nachbearbeitungsfilter (0-4 bei DivX, 0-63 bei MPEG)\n"
" -nobps          Benutze alternative A-V sync Methode für AVI's (könnte helfen!)\n"
" -framedrop      Benutze frame-dropping (für langsame Rechner)\n"
"\n"
"Tasten:\n"
" <- oder ->      Springe zehn Sekunden vor/zurück\n"
" rauf / runter   Springe eine Minute vor/zurück\n"
" p oder LEER     PAUSE (beliebige Taste zum Fortsetzen)\n"
" q oder ESC      Abspielen stoppen und Programm beenden\n"
" + oder -        Audioverzögerung um +/- 0.1 Sekunde verändern\n"
" o               OSD Mode:  Aus / Suchleiste / Suchleiste + Zeit\n"
" * oder /        Lautstärke verstellen ('m' für Auswahl Master/Wave)\n"
" z oder x        Untertitelverzögerung um +/- 0.1 Sekunde verändern\n"
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
"Wenn nichts davon hilft, lies DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer wurde OHNE GUI-Unterstützung kompiliert!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI erfordert X11!\n"
#define MSGTR_Playing "Spiele %s\n"
#define MSGTR_NoSound "Audio: kein Ton!!!\n"
#define MSGTR_FPSforced "FPS fixiert auf %5.3f  (ftime: %5.3f)\n"

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
#define MSGTR_MaybeNI "Vielleicht spielst du einen non-interleaved Stream/Datei oder der Codec funktioniert nicht.\n"
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
#define MSGTR_MissingMpegVideo "Vermisse MPEG Videostream!? Kontaktiere den Author, das könnte ein Bug sein :(\n"
#define MSGTR_InvalidMPEGES "Ungültiger MPEG-ES Stream??? Kontaktiere den Author, das könnte ein Bug sein :(\n"
#define MSGTR_FormatNotRecognized "=========== Sorry, das Dateiformat/Codec wird nicht unterstützt ==============\n"\
				  "============== Sollte dies ein AVI, ASF oder MPEG Stream sein, ===============\n"\
				  "================== dann kontaktiere bitte den Author =========================\n"
#define MSGTR_MissingVideoStream "kann keinen Videostream finden!\n"
#define MSGTR_MissingAudioStream "kann keinen Audiostream finden...  -> kein Ton\n"
#define MSGTR_MissingVideoStreamBug "Vermisse Videostream!? Kontaktiere den Author, möglicherweise ein Bug :(\n"

#define MSGTR_DoesntContainSelectedStream "Demux: Datei enthält nicht den gewählen Audio- oder Videostream\n"

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

// LIRC:
#define MSGTR_SettingUpLIRC "Initialisiere LIRC Unterstützung...\n"
#define MSGTR_LIRCdisabled "Verwenden der Fernbedienung nicht möglich\n"
#define MSGTR_LIRCopenfailed "Fehler beim Öffnen der LIRC Unterstützung!\n"
#define MSGTR_LIRCsocketerr "Fehler im LIRC Socket: %s\n"
#define MSGTR_LIRCcfgerr "Kann LIRC Konfigurationsdatei nicht lesen %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Über ..."
#define MSGTR_FileSelect "Wähle Datei ..."
#define MSGTR_SubtitleSelect "Wähle Untertitel ..."
#define MSGTR_OtherSelect "Wähle ..."
#define MSGTR_MessageBox "Message-Box"
#define MSGTR_PlayList "Playlist"
#define MSGTR_SkinBrowser "Skin Browser"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Abbrechen"
#define MSGTR_Add "Hinzufügen"
#define MSGTR_Remove "Entfernen"

// --- error messages ---
#define MSGTR_NEMDB "Sorry, nicht genug Speicher für den Zeichen-Puffer."
#define MSGTR_NEMFMR "Sorry, nicht genug Speicher für Menü-Rendering."
#define MSGTR_NEMFMM "Sorry, nicht genug Speicher für die Hauptfenster-Maske."

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

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Fataler Fehler ..."
#define MSGTR_MSGBOX_LABEL_Error "Fehler ..."
#define MSGTR_MSGBOX_LABEL_Warning "Warnung ..."

#endif
