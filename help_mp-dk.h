// Translated by:  Anders Rune Jensen <anders@gnulinux.dk>

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (se dokumentation!)\n"
"\n";

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Benyt:   mplayer [-gui] [indstilling] [url|sti/]filnavn\n"
#else
"Benyt:   mplayer [instilling] [url|sti/]filnavn\n"
#endif
"\n"
"Basis indstillinger (se mansiden for en komplet liste):\n"
" -vo <drv[:dev]> vælg video driver og enhed (se '-vo help' for en komplet liste)\n"
" -ao <drv[:dev]> vælg lyd driver og enhed (se '-ao help' for en komplet liste)\n"
#ifdef HAVE_VCD
" -vcd <trackno>  afspil et VCD (video cd) nummer fra et drev i stedet for en fil\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  specificer DVD enhed til dekryptering\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  afspiller DVD titel/nummer fra et drev i stedet for en fil\n"
" -alang/-slang   vælg DVD lyd/undertekst-sprog (vha. 2 karakters landekode)\n"
#endif
" -ss <timepos>   søger til en given position (sekunder eller hh:mm:ss)\n"
" -nosound        afspiller uden lyd\n"
" -fs -vm -zoom   type af afspilning i fuldskærm (fuldskærm, video mode, software skalering)\n"
" -x <x> -y <y>   skaler billede til <x> * <y> opløsning [hvis -vo driveren understøtter det!]\n"
" -sub <file>     specificer undertekst-fil (se også -subfps, -subdelay)\n"
" -playlist <file> specificer afspilningsliste\n"
" -vid x -aid y   afspil film (x) og lyd (y)\n"
" -fps x -srate y ændrer filmens (x fps) og lydens (y Hz)\n"
" -pp <quality>   slå efterprocesseringsfilter til (bedre billedkvalitet) (0-4 for DivX, 0-63 for mpegs)\n"
" -framedrop      slå billede-skip til (kan hjælpe langsomme maskiner)\n"
"\n"
"Basis taster:\n"
" <-  or  ->      søger 10 sekunder frem eller tilbage\n"
" up or down      søger 1 minut frem eller tilbage \n"
" pgup or pgdown  søger 10 minutter frem eller tilbage\n"
" < or >          søger frem og tilbage en afspilningslisten\n"
" p or SPACE      pause filmen (starter igen ved en vilkårlig tast)\n"
" q or ESC        stop afspilning og afslut program\n"
" + or -          juster lydforsinkelse med +/- 0.1 sekund\n"
" o               vælger OSD type:  ingen / søgebar / søgebar+tid\n"
" * or /          forøger eller formindsker volumen (tryk 'm' for at vælge master/pcm)\n"
" z or x          justerer undertekstforsinkelse med +/- 0.1 sekund\n"
"\n"
" * * * SE MANSIDEN FOR FLERE DETALJER, YDERLIGERE (AVANCEREDE) MULIGHEDER OG TASTER ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\n Afslutter... (%s)\n"
#define MSGTR_Exit_frames "Anmoder om et antal billeder bliver afspillet"
#define MSGTR_Exit_quit "Afslut"
#define MSGTR_Exit_eof "Slutningen af filen"
#define MSGTR_Exit_error "Fatal fejl"
#define MSGTR_IntBySignal "\nMPlayer afbrudt af signal %d i modul: %s \n"
#define MSGTR_NoHomeDir "Kan ikke finde hjemmekatalog (HOME)\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problem\n"
#define MSGTR_CreatingCfgFile "Genererer konfigurationsfil: %s\n"
#define MSGTR_InvalidVOdriver "Ugyldig valg af video driver: %s\nBrug '-vo help' for at få en komplet liste over gyldige video-drivere.\n"
#define MSGTR_InvalidAOdriver "Ugyldig valg af lyd driver: %s\nBrug '-ao help' for at få en komplet liste over gyldige lyd-drivere.\n"
#define MSGTR_CopyCodecsConf "(kopierer/linker etc/codecs.conf (fra MPlayer kilde katalog) til ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Kan ikke indlæse fonten:  %s\n"
#define MSGTR_CantLoadSub "Kan ikke indlæse undertekst-filen: %s\n"
#define MSGTR_ErrorDVDkey "Fejl under afvikling af DVD NØGLE.\n"
#define MSGTR_CmdlineDVDkey "DVD kommandolinjenøgle er gemt til dekryptering.\n"
#define MSGTR_DVDauthOk "DVD auth sekvens synes af være OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: kan ikke finde den valge fil eller adresse!\n"
#define MSGTR_CantOpenDumpfile "Kan ikke åbne dump filen!!!\n"
#define MSGTR_CoreDumped "kernen dumped :)\n"
#define MSGTR_FPSnotspecified "FPS ikke specificeret (eller ugyldig) i headeren! Brug -fps !\n"
#define MSGTR_NoVideoStream "Desværre, filmen kan enten ikke findes eller kan ikke afspilles endnu\n"
#define MSGTR_TryForceAudioFmt "Prøver at tvinge en lyd codec driver familie %d ...\n"
#define MSGTR_CantFindAfmtFallback "Kan ikke finde lyd codec for driver familien, falder tilbage på en anden driver.\n"
#define MSGTR_CantFindAudioCodec "Kan ikke finde codec til lyd formatet 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Prøv at opgradere %s fra etc/codecs.conf\n*** Hvis dette ikke hjælper, så læs filen DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Kunne ikke initialisere lyd codec! -> ingen lyd\n"
#define MSGTR_TryForceVideoFmt "Prøver at tvinge en video codec driver familie %d ...\n"
#define MSGTR_CantFindVfmtFallback "Kan ikke finde video codec for den tvungede driver familie, falder tilbage på en anden driver.\n"
#define MSGTR_CantFindVideoCodec "Kan ikke finde video codec til formatet 0x%X !\n"
#define MSGTR_VOincompCodec "Desværre, den valgte video driverenhed er ikke kompatibel med dette codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Kunne ikke initialisere video codec :(\n"
#define MSGTR_EncodeFileExists "Filen eksisterer allerede: %s (overskriv ikke din favorit film (AVI)!)\n"
#define MSGTR_CantCreateEncodeFile "Kan ikke oprette fil til enkodning\n"
#define MSGTR_CannotInitVO "FATAL: Kan ikke initialisere video driveren!\n"
#define MSGTR_CannotInitAO "Kunne ikke åbne/initialisere lydkortet -> INGEN LYD\n"
#define MSGTR_StartPlaying "Starter afspilning ...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"       ***********************************************************\n"\
"       **** Dit system er for langsomt til at afspille dette! ****\n"\
"       ***********************************************************\n"\
"!!! Evt. fejlkilder, problemer eller muligheder: \n"\
"- Den mest almindelige: ødelagt eller dårlig _lydkort_ driver. Mulighed: prøv -ao sdl eller brug\n"\
"  ALSA 0.5 eller oss emulation af ALSA 0.9. læs DOCS/sound.html for flere tips!\n"\
"- Langsom video output. Prøv en anden -vo driver (for liste: -vo help) eller prøv\n"\
"  med -framedrop !  Læs DOCS/video.html for video tuning/speedup tips.\n"\
"- Langsom CPU. Prøv ikke at afspille en stor dvd/divx på en langsom CPU! Prøv -hardframedrop\n"\
"- Ødelagt fil. Prøv kombinationer af følgende: -nobps  -ni  -mc 0  -forceidx\n"\
"Hvis intet af dette hjalp, læs da DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer blev kompileret UDEN grafisk interface!\n"
#define MSGTR_GuiNeedsX "MPlayer grafisk interface kræver X11!\n"
#define MSGTR_Playing "Afspiller %s\n"
#define MSGTR_NoSound "Lyd: ingen lyd!!!\n"
#define MSGTR_FPSforced "FPS tvunget til %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Kompileret med kørselstid CPU detektering - advarsel, dette er ikke optimalt! For at få den bedre ydelse rekompiler MPlayer fra kildekode med --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Kompileret til x86 CPU med udvidelse:"
#define MSGTR_AvailableVideoOutputPlugins "Tilgængelige videoudvidelser:\n"
#define MSGTR_AvailableVideoOutputDrivers "Tilgængelige videodrivere:\n"
#define MSGTR_AvailableAudioOutputDrivers "Tilgængelige lyddrivere:\n"
#define MSGTR_AvailableAudioCodecs "Tilgængelige lydcodecs:\n"
#define MSGTR_AvailableVideoCodecs "Tilgængelige videocodecs:\n"
#define MSGTR_UsingRTCTiming "Benytter Linux's hardware RTC timing (%ldHz)\n"
#define MSGTR_CannotReadVideoPropertiers "Video: kan ikke læse egenskaber\n"
#define MSGTR_NoStreamFound "Ingen strøm fundet\n"
#define MSGTR_InitializingAudioCodec "Initialiserer lydcodec...\n"
#define MSGTR_ErrorInitializingVODevice "Fejl under initialisering/åbning af den valgte video (-vo) enhed!\n"
#define MSGTR_ForcedVideoCodec "Tvunget video codec: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Beskrivelse: %s\nAO: forfatter: %s\n"
#define MSGTR_AOComment "AO: Kommentar: %s\n"
#define MSGTR_Video_NoVideo "Video: ingen video!!!\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Kunne ikke initialisere video filtre (-vop) eller video udgang (-vo) !\n"
#define MSGTR_Paused "\n------ PAUSE -------\r"
#define MSGTR_PlaylistLoadUnable "\nKunne ikke indlæse afspilningslisten %s\n"


// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM Drev '%s' ikke fundet!\n"
#define MSGTR_ErrTrackSelect "Fejl i valg af VCD nummer!"
#define MSGTR_ReadSTDIN "Læser fra stdin...\n"
#define MSGTR_UnableOpenURL "Ikke mulig at få kontakt til URL: %s\n"
#define MSGTR_ConnToServer "Koblet op til serveren: %s\n"
#define MSGTR_FileNotFound "Filen blev ikke fundet: '%s'\n"

#define MSGTR_CantOpenDVD "Kunne ikke åbne DVD drev: %s\n"
#define MSGTR_DVDwait "Læser disken struktur, vent venligst...\n"
#define MSGTR_DVDnumTitles "Der er %d titler på denne DVD.\n"
#define MSGTR_DVDinvalidTitle "Forkert DVD titel nummer: %d\n"
#define MSGTR_DVDnumChapters "Der er %d kapitler på denne DVD titel.\n"
#define MSGTR_DVDinvalidChapter "Forkert DVD katalog nummmer: %d\n"
#define MSGTR_DVDnumAngles "Der er %d vinkler i denne DVD titel.\n"
#define MSGTR_DVDinvalidAngle "Forkert DVD vinkelnummer: %d\n"
#define MSGTR_DVDnoIFO "Kan ikke finde IFO filen for DVD titlen %d.\n"
#define MSGTR_DVDnoVOBs "Kan ikke åbne titlen VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD korrekt åbnet!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Advarsel! Lyd-filens hoved %d er blevet omdefineret!\n"
#define MSGTR_VideoStreamRedefined "Advarsel! Video-filens hoved %d er blevet omdefineret!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: For mange (%d i %d bytes) lyd pakker i bufferen!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: For mange (%d i %d bytes) video pakker i bufferen!\n"
#define MSGTR_MaybeNI "(måske afspiller du en 'non-interleaved' stream/fil ellers fejlede codec'et)\n"
#define MSGTR_SwitchToNi "\nDetekteret defekt .AVI - skifter til -ni!\n"
#define MSGTR_DetectedFILMfile "Detekterede FILM fil format!\n"
#define MSGTR_DetectedFLIfile "Detekterede FLI fil format!\n"
#define MSGTR_DetectedROQfile "Detekterede RoQ fil format!\n"
#define MSGTR_DetectedREALfile "Detekterede REAL fil format!\n"
#define MSGTR_DetectedAVIfile "Detekterede AVI fil format!\n"
#define MSGTR_DetectedASFfile "Detekterede ASF fil format!\n"
#define MSGTR_DetectedMPEGPESfile "Detekterede MPEG-PES fil format!\n"
#define MSGTR_DetectedMPEGPSfile "Detekterede MPEG-PS fil format!\n"
#define MSGTR_DetectedMPEGESfile "Detekterede MPEG-ES fil format!\n"
#define MSGTR_DetectedQTMOVfile "Detekterede QuickTime/MOV fil format!\n"
#define MSGTR_MissingMpegVideo "Manglende MPEG video stream!? Rapporter venligst dette, det kan være en fejl i programmet :(\n"
#define MSGTR_InvalidMPEGES "Ugyldig MPEG-ES stream??? Rapporter venligst dette, det kunne være en fejl i programmet :(\n"
#define MSGTR_FormatNotRecognized \
"============= Desværre, dette fil-format er ikke detekteret eller understøttet ===============\n"\
"=== Hvis denne fil er en AVI, ASF or MPEG stream, så rapporter venligst dette, det kunne være en fejl i programmet :(===\n"
#define MSGTR_MissingVideoStream "Ingen video strøm fundet!\n"
#define MSGTR_MissingAudioStream "Ingen lyd strøm fundet...  ->ingen lyd\n"
#define MSGTR_MissingVideoStreamBug "Manglende video strøm!? Rapporter venligst dette, det kunne være en fejl i programmet :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: filen indeholder ikke den valgte lyd eller video stream\n"

#define MSGTR_NI_Forced "Tvunget"
#define MSGTR_NI_Detected "Detekteret"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI fil-format!\n"

#define MSGTR_UsingNINI "Bruger NON-INTERLEAVED (-ni), ødelagt AVI fil-format!\n"
#define MSGTR_CouldntDetFNo "Kunne ikke finde antallet af billeder (for en absolutte søgning)  \n"
#define MSGTR_CantSeekRawAVI "Kan ikke søge i rå .AVI strøm! (manglende index, prøv med -idx!)  \n"
#define MSGTR_CantSeekFile "Kan ikke søge i denne fil!  \n"
#define MSGTR_EncryptedVOB "Krypteret VOB fil (ikke kompileret med libcss support)! Læs filen DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Krypteret strøm men autoriseringen blev ikke påbegyndt af dig!!\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimeret hoved (endnu) ikke understøttet!\n"
#define MSGTR_MOVvariableFourCC "MOV: Advarsel! variablen FOURCC detekteret!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Advarsel! For mange numre!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV format endnu ikke understøttet! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "kunne ikke åbne codec\n"
#define MSGTR_CantCloseCodec "kunne ikke afslutte codec\n"

#define MSGTR_MissingDLLcodec "FEJL: Kunne ikke åbne DirectShow codec: %s\n"
#define MSGTR_ACMiniterror "Kunne ikke loade/initialisere Win32/ACM LYD codec (manglende DLL fil?)\n"
#define MSGTR_MissingLAVCcodec "Kunne ikke finde codec '%s' i libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayer blev kompileret uden directshow support!\n"
#define MSGTR_NoWfvSupport "Support for win32 codecs slået fra, eller er ikke tilgængelig på ikke-x86 platforme!\n"
#define MSGTR_NoDivx4Support "MPlayer blev kompileret UDEN DivX4Linux (libdivxdecore.so) understøttelse!\n"
#define MSGTR_NoLAVCsupport "MPlayer was kompileret UDEN ffmpeg/libavcodec understøttelse!\n"
#define MSGTR_NoACMSupport "Win32/ACM lyd codec slået fra, eller ikke tilgængelig på ikke-x86 CPU -> ingen lyd tvunget :(\n"
#define MSGTR_NoDShowAudio "Kompileret uden DirectShow support -> tvunget ingen lyd :(\n"
#define MSGTR_NoOggVorbis "OggVorbis lyd codec slået fra -> tvunget ingen lyd :(\n"
#define MSGTR_NoXAnimSupport "MPlayer blev kompileret UDEN XAnim support!\n"

#define MSGTR_MpegPPhint "ADVARSEL! Du anmodede efterprocesseringsfilter for en MPEG 1/2 video,\n" \
			 "         men MPlayer blev kompileret uden MPEG 1/2 efterprocesseringsfilter understøttelse!\n" \
			 "         #define MPEG12_POSTPROC i config.h, og rekompiler libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF under søgning efter sekvens hoved\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Kunne ikke læse sekvens hoved!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Kunne ikke læse sekvems hoved udvidelse!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Ugyldig sekvens hoved!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Ugyldig sekvens hoved udvidelse!\n"

#define MSGTR_ShMemAllocFail "Kunne ikke allokere delt ram\n"
#define MSGTR_CantAllocAudioBuf "Kunne ikke allokere lyd buffer\n"
#define MSGTR_NoMemForDecodedImage "ikke nok ram til at dekode billed buffer (%ld bytes)\n"

#define MSGTR_AC3notvalid "AC3 strøm invalid.\n"
#define MSGTR_AC3only48k "Kun 48000 Hz strømme understøttet.\n"
#define MSGTR_UnknownAudio "Ukendt/manglende lyd format, slår over til ingen lyd\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Sætter LIRC understøttelse op...\n"
#define MSGTR_LIRCdisabled "Du vil ikke være i stand til at bruge din fjernbetjening\n"
#define MSGTR_LIRCopenfailed "Ingen lirc understøttelse fundet!\n"
#define MSGTR_LIRCsocketerr "Der er noget galt med LIRC socket: %s\n"
#define MSGTR_LIRCcfgerr "Kunne ikke læse LIRC konfigurationsfil %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Om"
#define MSGTR_FileSelect "Vælg fil ..."
#define MSGTR_SubtitleSelect "Vælg undertekst-fil ..."
#define MSGTR_OtherSelect "Vælg..."
#define MSGTR_AudioFileSelect "Vælg ekstern lydkanal ..."
#define MSGTR_FontSelect "Vælg font ..."
#define MSGTR_MessageBox "Meddelelses kasse"
#define MSGTR_PlayList "PlayList"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Vælg udseende"
#define MSGTR_Network "Netværksstrømning ..."
#define MSGTR_Preferences "Instillinger"
#define MSGTR_OSSPreferences "OSS driver konfiguration"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Annuller"
#define MSGTR_Add "Tilføj"
#define MSGTR_Remove "Fjern"
#define MSGTR_Clear "Nulstil"
#define MSGTR_Config "Konfig"
#define MSGTR_ConfigDriver "Konfigurer driver"
#define MSGTR_Browse "Gennemse"

// --- error messages ---
#define MSGTR_NEMDB "Desværre, ikke nok ram til at vise bufferen."
#define MSGTR_NEMFMR "Desværre, ikke nok ram til menu rendering."
#define MSGTR_NEMFMM "Desværre, ikke nok ram til at vise main window shape mask."
#define MSGTR_IDFGCVD "Desværre, kunne ikke finde gui kompabitel video driver."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] fejl i skin config filen på linje %d: %s" 
#define MSGTR_SKIN_WARNING1 "[skin] advarsel i skin config filen på linje %d: widget fundet men før \"section\" ikke fundet ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] advarsel i skin config filne på linje %d: widget fundet men før \"subsection\" ikke fundet (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16 bits eller mindre ikke understøttet ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "filen ikke fundet ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "bmp læse fejl ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "tga læse fejl ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "png læse fejl ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE pakket tga ikke supporteret ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "ukendt filtype ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "Fejl i 24 bit to 32 bit convertering ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "ukendt besked: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "ikke nok ram\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "for mange fonte specificeret\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "font-filen ikke fundet\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "font-billed ikke fundet\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "ikke eksisterende font ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "ukendt parameter ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] ikke nok ram.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin blev ikke fundet ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin config-fil læse fejl ( %s ).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Om MPlayer"
#define MSGTR_MENU_Open "Åben ..."
#define MSGTR_MENU_PlayFile "Afspil fil ..."
#define MSGTR_MENU_PlayVCD "Afspil VCD ..."
#define MSGTR_MENU_PlayDVD "Afspil DVD ..."
#define MSGTR_MENU_PlayURL "Afspil URL ..."
#define MSGTR_MENU_LoadSubtitle "Indlæs undertekst ..."
#define MSGTR_MENU_LoadExternAudioFile "Indlæs extern lyd fil ..."
#define MSGTR_MENU_Playing "Afspilning"
#define MSGTR_MENU_Play "Afspil"
#define MSGTR_MENU_Pause "Pause"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Næste strøm"
#define MSGTR_MENU_PrevStream "Forrige strøm"
#define MSGTR_MENU_Size "Størrelse"
#define MSGTR_MENU_NormalSize "Normal størrelse"
#define MSGTR_MENU_DoubleSize "Double størrelse"
#define MSGTR_MENU_FullScreen "Fuld skærm"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Afspil disk ..."
#define MSGTR_MENU_ShowDVDMenu "Vis DVD menu"
#define MSGTR_MENU_Titles "Titler"
#define MSGTR_MENU_Title "Titel %2d"
#define MSGTR_MENU_None "(ingen)"
#define MSGTR_MENU_Chapters "Kapitler"
#define MSGTR_MENU_Chapter "Kapitel %2d"
#define MSGTR_MENU_AudioLanguages "Lyd sprog"
#define MSGTR_MENU_SubtitleLanguages "Undertekst sprog"
#define MSGTR_MENU_PlayList "Afspilningslisten"
#define MSGTR_MENU_SkinBrowser "Vælg udseende"
#define MSGTR_MENU_Preferences "Indstillinger"
#define MSGTR_MENU_Exit "Forlad ..."

// --- equalizer
#define MSGTR_EQU_Audio "Lyd"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Lysstyrke: "
#define MSGTR_EQU_Hue "Hue: "
#define MSGTR_EQU_Saturation "Saturation: "
#define MSGTR_EQU_Front_Left "Venstre Front"
#define MSGTR_EQU_Front_Right "Højre Front"
#define MSGTR_EQU_Back_Left "Venstre Baghøjtaler"
#define MSGTR_EQU_Back_Right "Højre Baghøjtaler"
#define MSGTR_EQU_Center "Center"
#define MSGTR_EQU_Bass "Bass"
#define MSGTR_EQU_All "Alle"

// --- playlist
#define MSGTR_PLAYLIST_Path "Sti"
#define MSGTR_PLAYLIST_Selected "Valgte filer"
#define MSGTR_PLAYLIST_Files "Filer"
#define MSGTR_PLAYLIST_DirectoryTree "Katalog træ"

// --- preferences
#define MSGTR_PREFERENCES_None "Ingen"
#define MSGTR_PREFERENCES_Codec1 "Benyt VFW (Win32) codecs"
#define MSGTR_PREFERENCES_Codec2 "Benyt OpenDivX/DivX4 codec (YV12)"
#define MSGTR_PREFERENCES_Codec3 "Benyt DirectShow (Win32) codecs"
#define MSGTR_PREFERENCES_Codec4 "Benyt ffmpeg (libavcodec) codecs"
#define MSGTR_PREFERENCES_Codec5 "benyt DivX4 codec (YUY2)"
#define MSGTR_PREFERENCES_Codec6 "Benyt XAnim codecs"
#define MSGTR_PREFERENCES_AvailableDrivers "Tilgængelige drivere:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Afspil ikke lyd"
#define MSGTR_PREFERENCES_NormalizeSound "Ensart lydstyrke"
#define MSGTR_PREFERENCES_EnEqualizer "Anvend equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "Anvend extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Koefficient:"
#define MSGTR_PREFERENCES_AudioDelay "Lydforsinkelse"
#define MSGTR_PREFERENCES_Audio "Lyd"
#define MSGTR_PREFERENCES_VideoEqu "Anvend video equalizer"
#define MSGTR_PREFERENCES_DoubleBuffer "Anvend double buffering"
#define MSGTR_PREFERENCES_DirectRender "Anvend direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Anvend billed-skip"
#define MSGTR_PREFERENCES_HFrameDrop "Anvend meget billed-skip ( farlig )"
#define MSGTR_PREFERENCES_Flip "Flip billede"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_OSDTimer "Statuslinje og indikator"
#define MSGTR_PREFERENCES_OSDProgress "Kun statuslinje"
#define MSGTR_PREFERENCES_Subtitle "Undertekst:"
#define MSGTR_PREFERENCES_SUB_Delay "Forsinkelse: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Position: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Deaktiver auto-undertekster"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode undertekst"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konverter en given undertekst til MPlayer's undertekst format"
#define MSGTR_PREFERENCES_SUB_SRT "Konverter den angivne undertekst til et tidsbaseret SubViewer (SRT) format"
#define MSGTR_PREFERENCES_Font "Font:"
#define MSGTR_PREFERENCES_FontFactor "Font factor:"
#define MSGTR_PREFERENCES_PostProcess "Anvend efterprocesseringsfilter"
#define MSGTR_PREFERENCES_AutoQuality "Auto kvalitet: "
#define MSGTR_PREFERENCES_NI "Benyt non-interleaved AVI parser"
#define MSGTR_PREFERENCES_IDX "Genopbyt index tabel, hvis nødvendig"
#define MSGTR_PREFERENCES_VideoCodecFamily "Video codec family:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD level"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Undertekst"
#define MSGTR_PREFERENCES_FRAME_Font "Font"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Efterprocesseringsfilter"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & demuxer"
#define MSGTR_PREFERENCES_OSS_Device "Device:"
#define MSGTR_PREFERENCES_OSS_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Message "Husk, nogle funktioner kræver at MPlayer bliver genstartet for at de virker."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "fatal fejl ..."
#define MSGTR_MSGBOX_LABEL_Error "fejl ..."
#define MSGTR_MSGBOX_LABEL_Warning "advarsel ..." 

#endif
