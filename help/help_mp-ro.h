// FIXME: This needs to be redone properly.
// Partially sync'ed with help_mp-en.h $Revision$
// This is a retranslation of the file by Bogdan Butnaru <bogdanb@fastmail.fm>,
// based on the previous translation by Codre Adrian
// <codreadrian@softhome.net> (address bounces).
// The translation is partial and should be completed eventually, also it
// should be checked that messages are 80-column wrapped
//
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Folosire: mplayer [opþiuni] [url|cale/]numefiºier\n"
"\n"
"Opþiuni principale: (lista completã în pagina man)\n"
" -vo <drv[:dev]>  alege driver-ul ºi device-ul de ieºire video\n"
"                  ('-vo help' pentru listã)\n"
" -ao <drv[:dev]>  alege driver-ul ºi device-ul de ieºire audio\n"
"                  ('-ao help' pentru listã)\n"

#ifdef HAVE_VCD
" vcd://<nrpistã>  ruleazã pista VCD (Video CD) de pe device în loc de fiºier\n"
#endif

#ifdef USE_DVDREAD
" dvd://<nrtitlu>  ruleazã titlul/pista de pe dispozitivul DVD în loc de fiºier\n"
" -aLMB/-sLMB      alege limba pentru audio/subtitrãri DVD\n"
"                  (cu codul de 2 caractere, ex. RO)\n"
#endif
" -ss <timp>       deruleazã la poziþia datã (secunde sau hh:mm:ss)\n"
" -nosound         rulare fãrã sunet\n"
" -fs              afiºare pe tot ecranul (sau -vm, -zoom, detalii în pagina man)\n"
" -x <x> -y <y>    alege rezoluþia (folosit pentru -vm sau -zoom)\n"
" -sub <fiºier>    specificã fiºierul cu subtitrãri folosit\n"
"                  (vezi ºi -subfps, -subdelay)\n"
" -playlist <fiº>  specificã playlist-ul\n"
" -vid x -aid y    alege pista video (x) ºi audio (y)\n"
" -fps x -srate y  schimbã rata video (x fps) ºi audio (y Hz)\n"
" -pp <calitate>   activeazã filtrul de postprocesare (detalii în pagina man)\n"
" -framedrop       activeazã sãritul cadrelor (pentru calculatoare lente)\n"
"\n"
"Taste principale: (lista completã în pagina man, vezi ºi input.conf)\n"
" <-  sau  ->      deruleazã spate/faþã 10 secunde\n"
" sus sau jos      deruleazã spate/faþã un minut\n"
" pgup or pgdown   deruleazã spate/faþã 10 minute\n"
" < or >           salt spate/faþã în playlist\n"
" p or SPACE       pauzã (apãsaþi orice tastã pentru continuare)\n"
" q or ESC         opreºte filmul ºi iese din program\n"
" + or -           modificã decalajul audio cu +/- 0,1 secunde\n"
" o                schimbã modul OSD între: nimic / barã derulare / barã + ceas\n"
" * or /           creºte sau scade volumul PCM\n"
" z or x           modificã decalajul subtitrãrii cu +/- 0,1 secunde\n"
" r or t           modificã poziþia subtitrãrii sus/jos, vezi ºi -vf expand\n"
"\n"
" * * * VEZI PAGINA MAN PENTRU DETALII, ALTE OPÞIUNI (AVANSATE) ªI TASTE * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:
#define MSGTR_Exiting "\nIeºire...\n"
#define MSGTR_ExitingHow "\nIeºire... (%s)\n"
#define MSGTR_Exit_quit "Ieºire"
#define MSGTR_Exit_eof "Sfârºit fiºier"
#define MSGTR_Exit_error "Eroare fatalã"
#define MSGTR_IntBySignal "\nMPlayer a fost întrerupt de semnalul %d în modulul: %s\n"
#define MSGTR_NoHomeDir "Nu gãsesc directorul HOME.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problem\n"
#define MSGTR_CreatingCfgFile "Creez fiºierul de configurare: %s\n"\
	"Încearcã '-vo help' pentru o listã cu driveri video disponibili.\n"
#define MSGTR_BuiltinCodecsConf "Folosesc 'codecs.conf' built-in.\n"
#define MSGTR_CantLoadFont "Nu pot încãrca fontul: %s\n"
#define MSGTR_CantLoadSub "Nu pot încãrca subtitrarea: %s\n"
#define MSGTR_FPSnotspecified "FPS (nr. de cadre pe secundã) nu e specificat în header sau e greºit; foloseºte opþiunea '-fps'.\n"
#define MSGTR_TryForceAudioFmtStr "Forþez familia de codec audio %s...\n"
#define MSGTR_CantFindAudioCodec "Nu gãsesc codec pentru formatul audio 0x%X.\n"
#define MSGTR_RTFMCodecs "Citeºte DOCS/HTML/en/codecs.html!\n" //lang
#define MSGTR_TryForceVideoFmtStr "Forþez familia de codecuri video %s...\n"
#define MSGTR_CantFindVideoCodec "Nu gãsesc codec potrivit pentru ieºirea '-vo' aleasã ºi formatul video 0x%X.\n"
#define MSGTR_VOincompCodec "Dispozitivul de ieºire video ales e incompatibil cu acest codec.\n"
#define MSGTR_CannotInitVO "FATAL: Nu pot activa driverul video.\n"
#define MSGTR_CannotInitAO "Nu pot deschide/iniþializa audio -> rulez fãrã sunet.\n"
#define MSGTR_StartPlaying "Rulez...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"     *****************************************************\n"\
"     **** Sistemul tãu e prea LENT pentru acest film! ****\n"\
"     *****************************************************\n\n"\
"Posibile motive, probleme, rezolvãri:\n"\
"- Cel mai des întâlnit caz: drivere _audio_ defecte\n"\
"  - Încearcã '-ao sdl' sau foloseºte ALSA 0.5 / emularea OSS pentru ALSA 0.9.\n"\
"  - Experimenteazã cu diferite valori pentru '-autosync', începând cu 30.\n"\
"- Ieºire video lentã\n"\
"  - Încearcã alt driver '-vo' ('-vo help' pentru o listã) sau \n"\
"    încearcã '-framedrop'.\n"\
"- Procesor lent\n"\
"  - Nu rula filme DVD/DivX mari pe un procesor lent! Încearcã -hardframedrop.\n"\
"- Fiºier stricat\n"\
"  - Încearcã diferite combinaþii de '-nobps', '-ni', '-forceidx' sau '-mc 0'.\n"\
"- Surse lente (NFS/SMB, DVD, VCD etc.)\n"\
"  - Încearcã '-cache 8192'.\n"\
"- Foloseºti -cache pentru fiºiere AVI neinterleaved?\n"\
"  - Încearcã '-nocache'.\n"\
"Citeºte DOCS/HTML/en/video.html pentru idei de reglare/accelerare.\n"\
"Dacã tot nu reuºeºti, citeºte DOCS/HTML/en/bugreports.html.\n\n" //lang

#define MSGTR_NoGui "MPlayer a fost compilat FÃRÃ suport pentru GUI.\n"
#define MSGTR_GuiNeedsX "MPlayer GUI necesitã X11.\n"
#define MSGTR_Playing "Rulez %s.\n"
#define MSGTR_NoSound "Audio: fãrã sunet\n"
#define MSGTR_FPSforced "FPS forþat la %5.3f  (ftime: %5.3f).\n"
#define MSGTR_AvailableVideoOutputDrivers "Plugin-uri de ieºire video disponibile:\n"
#define MSGTR_AvailableAudioOutputDrivers "Plugin-uri de ieºire audio disponibile:\n"
#define MSGTR_AvailableAudioCodecs "Codec-uri audio disponibile:\n"
#define MSGTR_AvailableVideoCodecs "Codec-uri video disponibile:\n"
#define MSGTR_AvailableFsType "Moduri fullscreen disponibile:\n"
#define MSGTR_UsingRTCTiming "Using Linux hardware RTC timing (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Video: Nu pot citi proprietãþile.\n"
#define MSGTR_NoStreamFound "Nu am gãsit nici un canal.\n"
#define MSGTR_ErrorInitializingVODevice "Eroare la activarea ieºirii video (-vo) aleasã.\n"
#define MSGTR_ForcedVideoCodec "Codec video forþat: %s\n"
#define MSGTR_ForcedAudioCodec "Codec audio forþat: %s\n"
#define MSGTR_Video_NoVideo "Video: nu existã video\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Nu pot iniþializa filtrele video (-vf) sau ieºirea video (-vo).\n"
#define MSGTR_Paused "\n  =====  PAUZÃ  =====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nNu pot sã încarc playlistul %s.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer a murit. Nu ar trebui sã se întâmple asta.\n"\
"  S-ar putea sã fie un bug în sursa MPlayer _sau_ în driverele tale _sau_ în\n"\
"  versiunea ta de gcc. Dacã crezi cã e vina MPlayer, te rog citeºte\n"\
"  DOCS/HTML/en/bugreports.html ºi urmeazã instrucþiunile de acolo. Nu putem\n"\
"  ºi nu te vom ajuta decat dacã asiguri informaþia cerutã acolo cand anunþi\n"\
"  un posibil bug.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControlFile "Folosesc fiºierul de control pass3: %s\n"
#define MSGTR_MissingFilename "\nLipseºte numele fiºierului.\n\n"
#define MSGTR_CannotOpenFile_Device "Nu pot deschide fiºierul/dispozitivul.\n"
#define MSGTR_CannotOpenDemuxer "Nu pot deschide demultiplexorul.\n"
#define MSGTR_NoAudioEncoderSelected "\nNu e ales nici un encoder audio (-oac). Alege unul (vezi '-oac help') sau foloseºte '-nosound'.\n"
#define MSGTR_NoVideoEncoderSelected "\nNu e ales nici un encoder video (-ovc). Alege te rog unul (vezi '-ovc help').\n"
#define MSGTR_CannotOpenOutputFile "Nu pot deschide fiºierul de ieºire '%s'.\n"
#define MSGTR_EncoderOpenFailed "Nu pot deschide encoderul.\n"
#define MSGTR_ForcingOutputFourcc "Forþez ieºirea fourcc la %x [%.4s]\n"
#define MSGTR_DuplicateFrames "\n%d cadre duplicate!\n"
#define MSGTR_SkipFrame "\nSkipping frame!\n"
#define MSGTR_ErrorWritingFile "%s: Eroare la scrierea fiºierului.\n"
#define MSGTR_RecommendedVideoBitrate "Bitrate-ul video recomandatpentru %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nCanal video: %8.3f kbit/s (%d B/s)  dimensiune: %"PRIu64" bytes %5.3f sec %d cadre\n"
#define MSGTR_AudioStreamResult "\nCanal audio: %8.3f kbit/s (%d B/s)  dimensiune: %"PRIu64" bytes %5.3f sec\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metoda de bitrate variabil\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(default)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           bitrate mediu\n"\
"\n"\
" cbr           bitrate constant\n"\
"               Forþeazã ºi codarea în mod CBR la preseturile ABR urmãtoare.\n"\
"\n"\
" br=<0-1024>   alege bitrate-ul în kBit (doar la CBR ºi ABR)\n"\
"\n"\
" q=<0-9>       calitate (0-maximã, 9-minimã) (doar pentru VBR)\n"\
"\n"\
" aq=<0-9>      calitate algoritmicã (0-maximã/lentã, 9-minimã/rapidã)\n"\
"\n"\
" ratio=<1-100> rata de compresie\n"\
"\n"\
" vol=<0-10>    amplificarea intrãrii audio\n"\
"\n"\
" mode=<0-3>    (default: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: de loc\n"\
"                1: tot\n"\
"                2: ajusteazã\n"\
"\n"\
" fast          Activeazã codare rapidã pentru urmãtoarele preseturi VBR,\n"\
"               la calitate puþin redusã ºi bitrate-uri crescute.\n"\
"\n"\
" preset=<value> Asigurã reglajele de calitate maxim posibile.\n"\
"                medium: codare VBR, calitate bunã\n"\
"                 (150-180 kbps bitrate)\n"\
"                standard:  codare VBR, calitate mare\n"\
"                 (170-210 kbps bitrate)\n"\
"                extreme: codare VBR calitate foarte mare\n"\
"                 (200-240 kbps bitrate)\n"\
"                insane:  codare CBR, calitate maximã\n"\
"                 (320 kbps bitrate)\n"\
"                 <8-320>: codare ABR la bitrate-ul dat în kbps.\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Nu gãsesc CD-ROM-ul '%s'.\n"
#define MSGTR_ErrTrackSelect "Eroare la alegerea pistei VCD."
#define MSGTR_ReadSTDIN "Citesc din stdin...\n"
#define MSGTR_UnableOpenURL "Nu pot deschide URL-ul: %s\n"
#define MSGTR_ConnToServer "Conectat la serverul: %s\n"
#define MSGTR_FileNotFound "Nu gãsesc fiºierul: '%s'\n"

#define MSGTR_SMBFileNotFound "Nu pot deschide de pe LAN: '%s'\n"

#define MSGTR_CantOpenDVD "Nu pot deschide DVD-ul: %s\n"
#define MSGTR_DVDnumTitles "Sunt %d titluri pe acest DVD.\n"
#define MSGTR_DVDinvalidTitle "Numãrul titlului DVD greºit: %d\n"
#define MSGTR_DVDnumChapters "Sunt %d capitole în acest titlu.\n"
#define MSGTR_DVDinvalidChapter "Numãrul capitolului e greºit: %d\n"
#define MSGTR_DVDnumAngles "Sunt %d unghiuri în acest titlu DVD.\n"
#define MSGTR_DVDinvalidAngle "Numãrul unghiului greºit: %d\n"
#define MSGTR_DVDnoIFO "Nu pot deschide fiºierul IFO pentru titlul %d.\n"
#define MSGTR_DVDnoVOBs "Nu pot deschide VOB-ul pentru titlu (VTS_%02d_1.VOB).\n"

// demuxer.c, demux_*.c:
#define MSGTR_FormatNotRecognized \
"====== Scuze, formatul acestui fiºier nu e cunoscut/suportat =======\n"\
"=== Dacã fiºierul este AVI, ASF sau MPEG, te rog anunþã autorul! ===\n"

#define MSGTR_MissingVideoStream "Nu am gãsit canal video.\n"
#define MSGTR_MissingAudioStream "Nu am gãsit canal audio -> rulez fãrã sunet.\n"
#define MSGTR_MissingVideoStreamBug "Canal video lipsã!? Intreabã autorul, ar putea fi un bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: Fiºierul nu conþine canalul video sau audio ales.\n"

#define MSGTR_NI_Forced "Forþat"
#define MSGTR_NI_Detected "Detectat"

#define MSGTR_UsingNINI "Folosesc formatul AVI NON-INTERLEAVED (incorect).\n"
#define MSGTR_CouldntDetFNo "Nu pot determina numãrul de cadre (pentru seek absolut).\n"
#define MSGTR_CantSeekRawAVI "Nu pot derula în stream-uri AVI pure. (E nevoie de index, încearcã cu opþiunea '-idx'.)\n"
#define MSGTR_CantSeekFile "Nu pot derula în acest fiºier.\n"

#define MSGTR_MOVcomprhdr "MOV: Pentru a folosi headere compresate e nevoie de ZLIB!\n"
#define MSGTR_MOVvariableFourCC "MOV: ATENTIE: Am detectat FOURCC variabil!?\n"
#define MSGTR_MOVtooManyTrk "MOV: ATENTIE: prea multe piste"
#define MSGTR_FoundAudioStream "==> Canal audio gãsit: %d\n"
#define MSGTR_FoundVideoStream "==> Canal video gãsit: %d\n"
#define MSGTR_DetectedTV "TV detectat! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Nu pot deschide demultiplexorul ogg.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Caut canalul audio (id:%d).\n"
#define MSGTR_CannotOpenAudioStream "Nu pot deschide canalul audio: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nu pot deschide canalul de subtitrare: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Nu am reuºit sã deschid demultiplexorul audio: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Nu am reuºit sã deschid demultiplexorul subtitrãrii: %s\n"
#define MSGTR_TVInputNotSeekable "Nu se poate derula TV! (Derularea probabil va schimba canalul ;)\n"
#define MSGTR_ClipInfo "Info despre clip:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: am detectat conþinut NTSC la 30fps, schimb framerate-ul.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: am detectat conþinut NTSC progresiv la 24fps, schimb framerate-ul.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nu pot deschide codecul.\n"
#define MSGTR_CantCloseCodec "Nu pot închide codecul.\n"

#define MSGTR_MissingDLLcodec "ERROR: Nu pot deschide codecul DirectShow necesar %s.\n"
#define MSGTR_ACMiniterror "Nu pot încãrca codecul audio Win32/ACM (lipseºte un DLL?).\n"
#define MSGTR_MissingLAVCcodec "Nu gãsesc codecul '%s' în libavcodec...\n"

#define MSGTR_UsingExternalPP "[PP] Folosesc filtru de postprocesare extern, q max = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Folosesc postprocesarea codecului, q max = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Atributul video '%s' nu e suportat de vo & vd alese.\n"
#define MSGTR_OpeningVideoDecoder "Deschid decodorul video: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Deschid decodorul audio: [%s] %s\n"
#define MSGTR_VDecoderInitFailed "VDecoder init eºuat :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder init eºuat :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder preinit eºuat :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Aloc %d bytes pentru bufferul de intrare.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Aloc %d + %d = %d bytes pentru bufferul de ieºire.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Pregãtesc folosirea LIRC...\n"
#define MSGTR_LIRCopenfailed "Nu am reuºit sã activez LIRC.\n"
#define MSGTR_LIRCcfgerr "Nu am putut citi fiºierul de configurare LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nu gãsesc filtrul video '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "Nu pot deschide filtrul video '%s'.\n"
#define MSGTR_OpeningVideoFilter "Deschid filtrul video: "

// vd.c

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Despre MPlayer"
#define MSGTR_FileSelect "Alege fiºierul..."
#define MSGTR_SubtitleSelect "Alege subtitrarea..."
#define MSGTR_OtherSelect "Alege..."
#define MSGTR_AudioFileSelect "Alege canalul audio extern..."
#define MSGTR_FontSelect "Alege fontul..."
#define MSGTR_PlayList "Playlist"
#define MSGTR_Equalizer "Egalizator"
#define MSGTR_SkinBrowser "Alegere Skin-uri"
#define MSGTR_Network "Streaming în reþea..."
#define MSGTR_Preferences "Preferinþe"
#define MSGTR_NoMediaOpened "Nu e deschis nici un fiºier."
#define MSGTR_VCDTrack "Pista VCD %d"
#define MSGTR_NoChapter "Nici un capitol"
#define MSGTR_Chapter "Capitol %d"
#define MSGTR_NoFileLoaded "Nici un fiºier încãrcat."

// --- buttons ---
#define MSGTR_Ok "OK"
#define MSGTR_Cancel "Anulare"
#define MSGTR_Add "Adaugã"
#define MSGTR_Remove "Eliminã"
#define MSGTR_Clear "Sterge tot"
#define MSGTR_Config "Configurare"
#define MSGTR_ConfigDriver "Configurare driver"

// --- error messages ---
#define MSGTR_NEMFMR "Scuze, nu am memorie destulã pentru afiºarea meniului."
#define MSGTR_IDFGCVD "Scuze, nu am gãsit un driver video compatibil cu GUI."
#define MSGTR_NEEDLAVC "Scuze, nu poþi afiºa fiºiere ne-MPEG cu dispozitivul DXR3/H+ fãrã recodare.\n"\
"Activeazã 'lavc' în cãsuþa de configurare pentru DXR3/H+."

// --- skin loader error messages

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Despre MPlayer"
#define MSGTR_MENU_Open "Deschide..."
#define MSGTR_MENU_PlayFile "Ruleazã fiºierul..."
#define MSGTR_MENU_PlayVCD "Ruleazã VCD..."
#define MSGTR_MENU_PlayDVD "Ruleazã DVD..."
#define MSGTR_MENU_PlayURL "Ruleazã URL..."
#define MSGTR_MENU_LoadSubtitle "Încarcã subtitrare..."
#define MSGTR_MENU_DropSubtitle "Scoate subtitreare..."
#define MSGTR_MENU_LoadExternAudioFile "Încarcã fiºier audio extern..."
#define MSGTR_MENU_Playing "Rulez"
#define MSGTR_MENU_Play "Play"
#define MSGTR_MENU_Pause "Pauzã"
#define MSGTR_MENU_Stop "Stop"
#define MSGTR_MENU_NextStream "Pista urmãtoare"
#define MSGTR_MENU_PrevStream "Pista precedentã"
#define MSGTR_MENU_Size "Dimensiune"
#define MSGTR_MENU_NormalSize "Dimensiune normalã"
#define MSGTR_MENU_DoubleSize "Dimensiune dublã"
#define MSGTR_MENU_FullScreen "Întreg ecranul"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Deschide discul..."
#define MSGTR_MENU_ShowDVDMenu "Afiºeazã meniul DVD"
#define MSGTR_MENU_Titles "Titluri"
#define MSGTR_MENU_Title "Titlu %2d"
#define MSGTR_MENU_None "(nimic)"
#define MSGTR_MENU_Chapters "Capitole"
#define MSGTR_MENU_Chapter "Capitolul %2d"
#define MSGTR_MENU_AudioLanguages "Limbi pentru audio"
#define MSGTR_MENU_SubtitleLanguages "Limbi pentru subtitrãri"
#define MSGTR_MENU_SkinBrowser "Alegere skin"
#define MSGTR_MENU_Exit "Ieºire..."
#define MSGTR_MENU_Mute "Fãrã sunet"
#define MSGTR_MENU_Original "Original"
#define MSGTR_MENU_AspectRatio "Raport dimensiuni"
#define MSGTR_MENU_AudioTrack "Pista audio"
#define MSGTR_MENU_Track "Pista %d"
#define MSGTR_MENU_VideoTrack "Pista video"

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Contrast: "
#define MSGTR_EQU_Brightness "Luuminozitate: "
#define MSGTR_EQU_Hue "Nuanþã: "
#define MSGTR_EQU_Saturation "Saturaþie: "
#define MSGTR_EQU_Front_Left "Faþã Stânga"
#define MSGTR_EQU_Front_Right "Faþã Dreapta"
#define MSGTR_EQU_Back_Left "Spate Stânga"
#define MSGTR_EQU_Back_Right "Spate Dreapta"
#define MSGTR_EQU_Center "Centru"
#define MSGTR_EQU_Bass "Bass"
#define MSGTR_EQU_All "Toate"
#define MSGTR_EQU_Channel1 "Canalul 1:"
#define MSGTR_EQU_Channel2 "Canalul 2:"
#define MSGTR_EQU_Channel3 "Canalul 3:"
#define MSGTR_EQU_Channel4 "Canalul 4:"
#define MSGTR_EQU_Channel5 "Canalul 5:"
#define MSGTR_EQU_Channel6 "Canalul 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Cale"
#define MSGTR_PLAYLIST_Selected "Fiºiere alese"
#define MSGTR_PLAYLIST_Files "Fiºiere"
#define MSGTR_PLAYLIST_DirectoryTree "Arbore de directoare"

// --- preferences
#define MSGTR_PREFERENCES_SubtitleOSD "Subtitrãri & OSD"
#define MSGTR_PREFERENCES_Codecs "Codecuri & demuxer"
#define MSGTR_PREFERENCES_Misc "Altele"

#define MSGTR_PREFERENCES_None "Nimic"
#define MSGTR_PREFERENCES_AvailableDrivers "Drivere disponibile:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nu reda sunetul"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizeazã sunetul"
#define MSGTR_PREFERENCES_EnableEqualizer "Activeazã egalizatorul"
#define MSGTR_PREFERENCES_ExtraStereo "Activeazã extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Coeficient:"
#define MSGTR_PREFERENCES_AudioDelay "Decalaj audio"
#define MSGTR_PREFERENCES_DoubleBuffer "Activeazã double buffering"
#define MSGTR_PREFERENCES_DirectRender "Activeazã direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Activeazã sãritul cadrelor"
#define MSGTR_PREFERENCES_HFrameDrop "Activeazã sãritul dur de cadre (PERICULOS)"
#define MSGTR_PREFERENCES_Flip "Inverseazã imaginea sus/jos"
#define MSGTR_PREFERENCES_OSDTimer "Ceas ºi indicatori"
#define MSGTR_PREFERENCES_OSDProgress "Doar bara de derulare"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Ceas, procent ºi timp total"
#define MSGTR_PREFERENCES_Subtitle "Subtitrare:"
#define MSGTR_PREFERENCES_SUB_Delay "Decalaj: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozitie: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Fãrã auto-încãrcarea subtitrãrii"
#define MSGTR_PREFERENCES_SUB_Unicode "Subtitrare Unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Converteºte subtitrarea datã la formatul MPlayer"
#define MSGTR_PREFERENCES_SUB_SRT "Converteºte subtitrarea la formatul time based SubViewer (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Alege suprapunerea subtitrarilor"
#define MSGTR_PREFERENCES_Font "Font:"
#define MSGTR_PREFERENCES_PostProcess "Activeazã postprocesarea"
#define MSGTR_PREFERENCES_AutoQuality "Calitate auto: "
#define MSGTR_PREFERENCES_NI "Foloseºte parser AVI non-interleaved"
#define MSGTR_PREFERENCES_IDX "Reconstruieºte tabela de index, dacã e nevoie"
#define MSGTR_PREFERENCES_VideoCodecFamily "Familia codecului video:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Familia codecului audio:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Nivelul OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Subtitrare"
#define MSGTR_PREFERENCES_FRAME_Font "Font"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocesare"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Cache"
#define MSGTR_PREFERENCES_Message "Nu uita cã rularea trebuie repornitã pentru ca unele opþiuni sã-ºi facã efectul!"
#define MSGTR_PREFERENCES_DXR3_VENC "Encoder video:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Foloseºte LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"

// lang 
#define MSGTR_PREFERENCES_FontEncoding2 "Limbi vest-europene (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Limbi vest-europene cu Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Limbi central-europene sau slavice (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, galicã, maltezã, turcã (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Vechiul charset baltic (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Chrilic (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabã (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Greacã modernã (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turcã (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltic (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celtic (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Charseturi ebraice (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Rusã (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ucrainianã, belarusã (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Chinezã simplificatã (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Chinezã tradiþionalã (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japonezã (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Coreanã (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Tailandezã (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Chirilic de Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Central-european ºi slavic de Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Fãrã scalare automatã"
#define MSGTR_PREFERENCES_FontPropWidth "Proporþional cu lãþimea filmului"
#define MSGTR_PREFERENCES_FontPropHeight "Proporþional cu înãlþimea filmului"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proportional cu diagonala filmului"
#define MSGTR_PREFERENCES_FontEncoding "Codare:"
#define MSGTR_PREFERENCES_FontTextScale "Scara textului:"
#define MSGTR_PREFERENCES_FontOSDScale "Scara OSD:"
#define MSGTR_PREFERENCES_Cache "Cache on/off"
#define MSGTR_PREFERENCES_CacheSize "Dimensiune cache: "
#define MSGTR_PREFERENCES_LoadFullscreen "Porneºte fullscreen"
#define MSGTR_PREFERENCES_SaveWinPos "Salveazã poziþia ferestrei"
#define MSGTR_PREFERENCES_XSCREENSAVER "Opreºte XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Activeazã playbar"
#define MSGTR_PREFERENCES_AutoSync "AutoSync pornit/oprit"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "DVD:"
#define MSGTR_PREFERENCES_FPS "Cadre pe secundã:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Afiºeazã fereastra video cand e inactivã"

#define MSGTR_ABOUT_UHU "Dezvoltare GUI sponsorizatã de UHU Linux\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Eroare fatalã!"
#define MSGTR_MSGBOX_LABEL_Error "Eroare!"
#define MSGTR_MSGBOX_LABEL_Warning "Atenþie!"

#endif
