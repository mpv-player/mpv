// $Revision$
// Translated files should be sent to the mplayer-dev-eng mailing list or
// to the help messages maintainer, see DOCS/tech/MAINTAINERS.

// The header of the translated file should contain credits and contact
// information. Before major releases we will notify all translators to update
// their files. 

// Partially sync'ed with help_mp-en.h $Revision$
// This is a retranslation of the file by Bogdan Butnaru <bogdanb@fastmail.fm>.
// I did however use the previous translation by Codre Adrian 
// <codreadrian@softhome.net> (address bounces)
// The translation is partial: I did not translate most low-level errors, 
// things you have to know programming to get, because if you understand it 
// you almost certainly know English. Also, asking for help is easier that 
// way (an error message in Romanian, or a Romanian to English translation 
// by a neophyte is probably useless for a non-romanian programmer).

// I think messaages are 80-column wrapped, but I can't check this with
// MPlayer right now.

// I added '//' comments wherever I need more info: 
//   ** '// lang' where english files where cited in the original version
//		(as in DOCS/HTML/en/bugreports.html). If there are romanian versions
//		of such files, please replace the URLs.
//   ** '// dunno' where I wasn't sure of the message's meaning; please send me
//		an explanation of where the message appears and why.
//   ** '//!' are notes for myself or other translators; I used these whenever 
//   		I couldn't find a satisfying romanian translation. I think the 
//		translation of many technical words is counterproductive: those who
//		actually understand the technical meaning know the english word;
//		translating would confuse them and won't make things any clearer
//		for someone unfamiliar to it. So I didn't translate things like
//		framebuffer or colorspace, nor compile-related errors and such. 
//		You should ignore this mark, but other translators might use
//		them.
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
                   (vezi ºi -subfps, -subdelay)\n"
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
#define MSGTR_Exiting "\nIeºire... (%s)\n"
#define MSGTR_Exit_quit "Ieºire"
#define MSGTR_Exit_eof "Sfârºit fiºier"
#define MSGTR_Exit_error "Eroare fatalã"
#define MSGTR_IntBySignal "\nMPlayer a fost întrerupt de semnalul %d în modulul: %s\n"
#define MSGTR_NoHomeDir "Nu gãsesc directorul HOME.\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problem\n"
#define MSGTR_CreatingCfgFile "Creez fiºierul de configurare: %s\n"
#define MSGTR_InvalidVOdriver "Numele driverului de ieºire video e greºit: %s\n"
	"Încearcã '-vo help' pentru o listã cu driveri video disponibili.\n"
#define MSGTR_InvalidAOdriver "Numele driverului de ieºire audio e greºit: %s\n"
	"Foloseºte '-ao help' pentru lista cu driveri audio disponibili.\n"
#define MSGTR_CopyCodecsConf "(Copy/link etc/codecs.conf from the MPlayer sources to ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Folosesc 'codecs.conf' built-in.\n"
#define MSGTR_CantLoadFont "Nu pot încãrca fontul: %s\n"
#define MSGTR_CantLoadSub "Nu pot încãrca subtitrarea: %s\n"
#define MSGTR_ErrorDVDkey "Eroare la prelucrarea cheii DVD.\n"
#define MSGTR_CmdlineDVDkey "Cheia DVD cerutã e folositã pentru decodare.\n"
#define MSGTR_DVDauthOk "Secvenþa de autenticare DVD pare sã fie în regulã.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: Selected stream missing!\n"
#define MSGTR_CantOpenDumpfile "Cannot open dump file.\n"
#define MSGTR_CoreDumped "Core dumped ;)\n"
#define MSGTR_FPSnotspecified "FPS (nr. de cadre pe secundã) nu e specificat în header sau e greºit; foloseºte opþiunea '-fps'.\n"
#define MSGTR_TryForceAudioFmtStr "Forþez familia de codec audio %s...\n"
#define MSGTR_CantFindAfmtFallback "Nu gãsesc un codec audio pentru familia de drivere forþatã, încerc alte drivere.\n"
#define MSGTR_CantFindAudioCodec "Nu gãsesc codec pentru formatul audio 0x%X.\n"
#define MSGTR_RTFMCodecs "Citeºte DOCS/HTML/en/codecs.html!\n" //lang
#define MSGTR_CouldntInitAudioCodec "Nu pot iniþializa codec-ul audio -> nu am sunet.\n"
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
"Citeºte DOCS/HTML/en/devices.html pentru idei de reglare/accelerare.\n"\ //lang
"Dacã tot nu reuºeºti, citeºte DOCS/HTML/en/bugreports.html.\n\n" //lang

#define MSGTR_NoGui "MPlayer a fost compilat FÃRÃ suport pentru GUI.\n"
#define MSGTR_GuiNeedsX "MPlayer GUI necesitã X11.\n"
#define MSGTR_Playing "Rulez %s.\n"
#define MSGTR_NoSound "Audio: fãrã sunet\n"
#define MSGTR_FPSforced "FPS forþat la %5.3f  (ftime: %5.3f).\n"
#define MSGTR_CompiledWithRuntimeDetection "Compiled with Runtime CPU Detection - WARNING - this is not optimal!\nTo get best performance, recompile MPlayer with --disable-runtime-cpudetection.\n"
#define MSGTR_CompiledWithCPUExtensions "Compiled for x86 CPU with extensions:"
#define MSGTR_AvailableVideoOutputPlugins "Plugin-uri de ieºire video disponibile:\n"
#define MSGTR_AvailableVideoOutputDrivers "Plugin-uri de ieºire video disponibile:\n"
#define MSGTR_AvailableAudioOutputDrivers "Plugin-uri de ieºire audio disponibile:\n"
#define MSGTR_AvailableAudioCodecs "Codec-uri audio disponibile:\n"
#define MSGTR_AvailableVideoCodecs "Codec-uri video disponibile:\n"
#define MSGTR_AvailableAudioFm "\nAvailable (compiled-in) audio codec families/drivers:\n"
#define MSGTR_AvailableVideoFm "\nAvailable (compiled-in) video codec families/drivers:\n"
#define MSGTR_AvailableFsType "Moduri fullscreen disponibile:\n"
#define MSGTR_UsingRTCTiming "Using Linux hardware RTC timing (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Video: Nu pot citi proprietãþile.\n"
#define MSGTR_NoStreamFound "Nu am gãsit nici un canal.\n"
#define MSGTR_InitializingAudioCodec "Iniþializez codecul audio...\n"
#define MSGTR_ErrorInitializingVODevice 
"Eroare la activarea ieºirii video (-vo) aleasã.\n"
#define MSGTR_ForcedVideoCodec "Codec video forþat: %s\n"
#define MSGTR_ForcedAudioCodec "Codec audio forþat: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Descriere: %s\nAO: Autor: %s\n"
#define MSGTR_AOComment "AO: Comentariu: %s\n"
#define MSGTR_Video_NoVideo "Video: nu existã video\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Nu pot iniþializa filtrele video (-vf) sau ieºirea video (-vo).\n"
#define MSGTR_Paused "\n  =====  PAUZÃ  =====\r" // no more than 23 characters (status line for audio files)
#define MSGTR_PlaylistLoadUnable "\nNu pot sã încarc playlistul %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer crashed by an 'Illegal Instruction'.\n"\
"  It may be a bug in our new runtime CPU-detection code...\n"\
"  Please read DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer crashed by an 'Illegal Instruction'.\n"\
"  It usually happens when you run it on a CPU different than the one it was\n"\
"  compiled/optimized for.\n"\
"  Verify this!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer crashed by bad usage of CPU/FPU/RAM.\n"\
"  Recompile MPlayer with --enable-debug and make a 'gdb' backtrace and\n"\
"  disassembly. Details in DOCS/HTML/en/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer a murit. Nu ar trebui sã se întâmple asta.\n"\
"  S-ar putea sã fie un bug în sursa MPlayer _sau_ în driverele tale _sau_ în\n"\
"  versiunea ta de gcc. Dacã crezi cã e vina MPlayer, te rog citeºte\n"\
"  DOCS/HTML/en/bugreports.html ºi urmeazã instrucþiunile de acolo. Nu putem\n"\
"  ºi nu te vom ajuta decat dacã asiguri informaþia cerutã acolo cand anunþi\n"\
"  un posibil bug.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControllFile "Folosesc fiºierul de control pass3: %s\n"
#define MSGTR_MissingFilename "\nLipseºte numele fiºierului.\n\n"
#define MSGTR_CannotOpenFile_Device "Nu pot deschide fiºierul/dispozitivul.\n"
#define MSGTR_ErrorDVDAuth "Eroare la autenticarea DVD.\n"
#define MSGTR_CannotOpenDemuxer "Nu pot deschide demultiplexorul.\n"
#define MSGTR_NoAudioEncoderSelected "\nNu e ales nici un encoder audio (-oac). Alege unul (vezi '-oac help') sau foloseºte '-nosound'.\n"
#define MSGTR_NoVideoEncoderSelected "\nNu e ales nici un encoder video (-ovc). Alege te rog unul (vezi '-ovc help').\n"
#define MSGTR_InitializingAudioCodec "Pornesc codecul audio...\n"
#define MSGTR_CannotOpenOutputFile "Nu pot deschide fiºierul de ieºire '%s'.\n"
#define MSGTR_EncoderOpenFailed "Nu pot deschide encoderul.\n"
#define MSGTR_ForcingOutputFourcc "Forþez ieºirea fourcc la %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "Scriu header-ul AVI...\n"
#define MSGTR_DuplicateFrames "\n%d cadre duplicate!\n"
#define MSGTR_SkipFrame "\nSkipping frame!\n"
#define MSGTR_ErrorWritingFile "%s: Eroare la scrierea fiºierului.\n"
#define MSGTR_WritingAVIIndex "\nScriu indexul AVI...\n"
#define MSGTR_FixupAVIHeader "Repar header-ul AVI...\n"
#define MSGTR_RecommendedVideoBitrate "Bitrate-ul video recomandatpentru %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nCanal video: %8.3f kbit/s (%d bps)  dimensiune: %d bytes %5.3f sec %d cadre\n"
#define MSGTR_AudioStreamResult "\nCanal audio: %8.3f kbit/s (%d bps)  dimensiune: %d bytes %5.3f sec\n"

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
"                2: ajusteazã\n"\ // dunno
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

#define MSGTR_SMBInitError "Cannot init the libsmbclient library: %d\n"
#define MSGTR_SMBFileNotFound "Nu pot deschide de pe LAN: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer was not compiled with SMB reading support.\n"

#define MSGTR_CantOpenDVD "Nu pot deschide DVD-ul: %s\n"
#define MSGTR_DVDwait "Citesc structura discului, te rog aºteaptã...\n"
#define MSGTR_DVDnumTitles "Sunt %d titluri pe acest DVD.\n"
#define MSGTR_DVDinvalidTitle "Numãrul titlului DVD greºit: %d\n"
#define MSGTR_DVDnumChapters "Sunt %d capitole în acest titlu.\n"
#define MSGTR_DVDinvalidChapter "Numãrul capitolului e greºit: %d\n"
#define MSGTR_DVDnumAngles "Sunt %d unghiuri în acest titlu DVD.\n"
#define MSGTR_DVDinvalidAngle "Numãrul unghiului greºit: %d\n"
#define MSGTR_DVDnoIFO "Nu pot deschide fiºierul IFO pentru titlul %d.\n"
#define MSGTR_DVDnoVOBs "Nu pot deschide VOB-ul pentru titlu (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD deschis OK.\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "WARNING: Audio stream header %d redefined.\n"
#define MSGTR_VideoStreamRedefined "WARNING: Video stream header %d redefined.\n"
#define MSGTR_TooManyAudioInBuffer "\nToo many audio packets in the buffer: (%d in %d bytes).\n"
#define MSGTR_TooManyVideoInBuffer "\nToo many video packets in the buffer: (%d in %d bytes).\n"
#define MSGTR_MaybeNI "Maybe you are playing a non-interleaved stream/file or the codec failed?\n" \
                      "For AVI files, try to force non-interleaved mode with the -ni option.\n"
#define MSGTR_SwitchToNi "\nBadly interleaved AVI file detected - switching to -ni mode...\n"
#define MSGTR_Detected_XXX_FileFormat "%s file format detected.\n"
#define MSGTR_DetectedAudiofile "Audio file detected.\n"
#define MSGTR_NotSystemStream "Not MPEG System Stream format... (maybe Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "Invalid MPEG-ES stream??? Contact the author, it may be a bug :(\n"
#define MSGTR_FormatNotRecognized \
"====== Scuze, formatul acestui fiºier nu e cunoscut/suportat =======\n"\
"=== Dacã fiºierul este AVI, ASF sau MPEG, te rog anunþã autorul! ===\n"
// dunno - 'please contact the author!' -- is this supposed to be the
// author of the clip, or that of mplayer?

#define MSGTR_MissingVideoStream "Nu am gãsit canal video.\n"
#define MSGTR_MissingAudioStream "Nu am gãsit canal audio -> rulez fãrã sunet.\n"
#define MSGTR_MissingVideoStreamBug "Canal video lipsã!? Intreabã autorul, ar putea fi un bug :(\n" // dunno - The same question as above

#define MSGTR_DoesntContainSelectedStream "demux: Fiºierul nu conþine canalul video sau audio ales.\n"

#define MSGTR_NI_Forced "Forþat"
#define MSGTR_NI_Detected "Detectat"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI file format.\n"

#define MSGTR_UsingNINI "Folosesc formatul AVI NON-INTERLEAVED (incorect).\n"
#define MSGTR_CouldntDetFNo "Nu pot determina numãrul de cadre (pentru seek absolut).\n"
#define MSGTR_CantSeekRawAVI "Nu pot derula în stream-uri AVI pure. (E nevoie de index, încearcã cu opþiunea '-idx'.)\n"
#define MSGTR_CantSeekFile "Nu pot derula în acest fiºier.\n"

#define MSGTR_EncryptedVOB "Fiºier VOB criptat! Citeºte DOCS/HTML/en/dvd.html.\n" // lang

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
#define MSGTR_DemuxerInfoAlreadyPresent "Demuxer info %s already present!\n" // dunno
#define MSGTR_ClipInfo "Info despre clip:\n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: am detectat conþinut NTSC la 30fps, schimb framerate-ul.\n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: am detectat conþinut NTSC progresiv la 24fps, schimb framerate-ul.\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nu pot deschide codecul.\n"
#define MSGTR_CantCloseCodec "Nu pot închide codecul.\n"

#define MSGTR_MissingDLLcodec "ERROR: Nu pot deschide codecul DirectShow necesar %s.\n"
#define MSGTR_ACMiniterror "Nu pot încãrca codecul audio Win32/ACM (lipseºte un DLL?).\n"
#define MSGTR_MissingLAVCcodec "Nu gãsesc codecul '%s' în libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF while searching for sequence header.\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Cannot read sequence header.\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Cannot read sequence header extension.\n"
#define MSGTR_BadMpegSequHdr "MPEG: bad sequence header\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: bad sequence header extension\n"

#define MSGTR_ShMemAllocFail "Cannot allocate shared memory.\n"
#define MSGTR_CantAllocAudioBuf "Cannot allocate audio out buffer.\n"

#define MSGTR_UnknownAudio "Unknown/missing audio format -> no sound\n"

#define MSGTR_UsingExternalPP "[PP] Folosesc filtru de postprocesare extern, q max = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Folosesc postprocesarea codecului, q max = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Atributul video '%s' nu e suportat de vo & vd alese.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Requested video codec family [%s] (vfm=%s) not available.\nEnable it at compilation.\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Requested audio codec family [%s] (afm=%s) not available.\nEnable it at compilation.\n"
#define MSGTR_OpeningVideoDecoder "Deschid decodorul video: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Deschid decodorul audio: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit video: %s\n"
#define MSGTR_UninitAudioStr "uninit audio: %s\n"
#define MSGTR_VDecoderInitFailed "VDecoder init eºuat :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder init eºuat :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder preinit eºuat :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Aloc %d bytes pentru bufferul de intrare.\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Aloc %d + %d = %d bytes pentru bufferul de ieºire.\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Pregãtesc folosirea LIRC...\n"
#define MSGTR_LIRCdisabled "Nu-þi vei putea folosi telecomanda.\n"
#define MSGTR_LIRCopenfailed "Nu am reuºit sã activez LIRC.\n"
#define MSGTR_LIRCcfgerr "Nu am putut citi fiºierul de configurare LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nu gãsesc filtrul video '%s'.\n"
#define MSGTR_CouldNotOpenVideoFilter "Nu pot deschide filtrul video '%s'.\n"
#define MSGTR_OpeningVideoFilter "Deschid filtrul video: "
#define MSGTR_CannotFindColorspace "Cannot find common colorspace, even by inserting 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Codec did not set sh->disp_w and sh->disp_h, trying workaround.\n"
#define MSGTR_VoConfigRequest "VDec: vo config request - %d x %d (preferred csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Could not find matching colorspace - retrying with -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Movie-Aspect is %.2f:1 - prescaling to correct movie aspect.\n"
#define MSGTR_MovieAspectUndefined "Movie-Aspect is undefined - no prescaling applied.\n"

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
#define MSGTR_OSSPreferences "Configurare driver OSS"
#define MSGTR_SDLPreferences "Configurare driver SDL"
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
#define MSGTR_Browse "Browse" //!

// --- error messages ---
#define MSGTR_NEMDB "Scuze, nu e memorie suficientã pentru draw buffer." //!
#define MSGTR_NEMFMR "Scuze, nu am memorie destulã pentru afiºarea meniului."
#define MSGTR_IDFGCVD "Scuze, nu am gãsit un driver video compatibil cu GUI."
#define MSGTR_NEEDLAVCFAME "Scuze, nu poþi afiºa fiºiere ne-MPEG cu dispozitivul DXR3/H+ fãrã recodare.\n"\
"Activeazã 'lavc' sau 'fame' în cãsuþa de configurare pentru DXR3/H+."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] error in skin config file on line %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] warning in skin config file on line %d: widget found but before \"section\" not found (%s)"
#define MSGTR_SKIN_WARNING2 "[skin] warning in skin config file on line %d: widget found but before \"subsection\" not found (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] warning in skin config file on line %d: this subsection not supported by this widget (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "16 bits or less depth bitmap not supported (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "file not found (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "BMP read error (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "TGA read error (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "PNG read error (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE packed TGA not supported (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "unknown file type (%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "24 bit to 32 bit convert error (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "unknown message: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "not enough memory\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "Too many fonts declared.\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "Font file not found.\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "Font image file not found.\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "non-existent font identifier (%s)\n"
#define MSGTR_SKIN_UnknownParameter "unknown parameter (%s)\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] not enough memory\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin not found (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "skin configfile read error (%s)\n"
#define MSGTR_SKIN_LABEL "Skins:"

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
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Alegere skin"
#define MSGTR_MENU_Preferences "Preferinþe"
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
#define MSGTR_PREFERENCES_Audio "Audio"
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_SubtitleOSD "Subtitrãri & OSD"
#define MSGTR_PREFERENCES_Codecs "Codecuri & demuxer"
#define MSGTR_PREFERENCES_Misc "Altele"

#define MSGTR_PREFERENCES_None "Nimic" // dunno - what's this for?
#define MSGTR_PREFERENCES_AvailableDrivers "Drivere disponibile:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nu reda sunetul"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizeazã sunetul"
#define MSGTR_PREFERENCES_EnEqualizer "Activeazã egalizatorul"
#define MSGTR_PREFERENCES_ExtraStereo "Activeazã extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Coeficient:"
#define MSGTR_PREFERENCES_AudioDelay "Decalaj audio"
#define MSGTR_PREFERENCES_DoubleBuffer "Activeazã double buffering" 
#define MSGTR_PREFERENCES_DirectRender "Activeazã direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Activeazã sãritul cadrelor"
#define MSGTR_PREFERENCES_HFrameDrop "Activeazã sãritul dur de cadre (PERICULOS)"
#define MSGTR_PREFERENCES_Flip "Inverseazã imaginea sus/jos"
#define MSGTR_PREFERENCES_Panscan "Panscan: " // dunno - I don't know what this is, but it probably shouldn't be translated
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
#define MSGTR_PREFERENCES_SUB_Overlap "Alege suprapunerea subtitrarilor" // dunno - what's this?
#define MSGTR_PREFERENCES_Font "Font:"
#define MSGTR_PREFERENCES_FontFactor "Font factor:" // dunno - what exactly is this?
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
#define MSGTR_PREFERENCES_FRAME_Misc "Altele"
#define MSGTR_PREFERENCES_OSS_Device "Dispozitiv:"
#define MSGTR_PREFERENCES_OSS_Mixer "Mixer:"
#define MSGTR_PREFERENCES_SDL_Driver "Driver:"
#define MSGTR_PREFERENCES_Message "Nu uita cã rularea trebuie repornitã pentru ca unele opþiuni sã-ºi facã efectul!"
#define MSGTR_PREFERENCES_DXR3_VENC "Encoder video:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Foloseºte LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Foloseºte FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"

/* lang
* I translated these, but I'm not sure it's such a good idea; only the central 
* european charsets are relevant to romanian; if you need something else you
* are probably not interested in it's romanian name.
*/
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
#define MSGTR_PREFERENCES_FontBlur "Blur:" //!
#define MSGTR_PREFERENCES_FontOutLine "Outline:" //!
#define MSGTR_PREFERENCES_FontTextScale "Scara textului:"
#define MSGTR_PREFERENCES_FontOSDScale "Scara OSD:"
#define MSGTR_PREFERENCES_Cache "Cache on/off"
#define MSGTR_PREFERENCES_CacheSize "Dimensiune cache: "
#define MSGTR_PREFERENCES_LoadFullscreen "Porneºte fullscreen"
#define MSGTR_PREFERENCES_SaveWinPos "Salveazã poziþia ferestrei"
#define MSGTR_PREFERENCES_XSCREENSAVER "Opreºte XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Activeazã playbar" // dunno - What is the playbar? 
#define MSGTR_PREFERENCES_AutoSync "AutoSync pornit/oprit"
#define MSGTR_PREFERENCES_AutoSyncValue "Autosync: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "DVD:"
#define MSGTR_PREFERENCES_FPS "Cadre pe secundã:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Afiºeazã fereastra video cand e inactivã"

#define MSGTR_ABOUT_UHU "Dezvoltare GUI sponsorizatã de UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   echipa MPlayer principalã:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Alþi progamatori:\n"
#define MSGTR_ABOUT_MainTesters "   Testeri principali:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Eroare fatalã!"
#define MSGTR_MSGBOX_LABEL_Error "Eroare!"
#define MSGTR_MSGBOX_LABEL_Warning "Atenþie!"

#endif
