// Translated by:  Daniel BeÂa, benad@centrum.cz
// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2003 Arpad Gereoffy (vi‘ DOCS!)\n"
"\n";

// Preklad do slovenüiny 

static char help_text[]=
"Poußitie:   mplayer [prep°naüe] [url|cesta/]menos£boru\n"
"\n"
"Prep°naüe:\n"
" -vo <drv[:dev]> vÏber vÏstup. video ovl†daüa&zariadenia (-vo help pre zoznam)\n"
" -ao <drv[:dev]> vÏber vÏstup. audio ovl†daüa&zariadenia (-ao help pre zoznam)\n"
#ifdef HAVE_VCD
" -vcd <trackno>  prehraú VCD (video cd) stopu zo zariadenia namiesto zo s£boru\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  urüenie DVD zariadenia pre overenie autenticity (pre k¢dovanÇ disky)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  prehraú DVD titul/stopu zo zariadenia (mechaniky) namiesto s£boru\n"
" -alang/-slang   vybraú jazyk DVD zvuku/titulkov (pomocou 2-miest. k¢du krajiny)\n"
#endif
" -ss <timepos>   posun na poz°ciu (sekundy alebo hh:mm:ss)\n"
" -nosound        prehr†vaú bez zvuku\n"
" -fs -vm -zoom   voñby pre prehr†vanie na cel£ obrazovku (cel† obrazovka\n                 meniú videoreßim, softvÇrovÏ zoom)\n"
" -x <x> -y <y>   zvÑüÁenie obrazu na rozmer <x>*<y> (pokiañ to vie -vo ovl†daü!)\n"
" -sub <file>     voñba s£boru s titulkami (vi‘ tieß -subfps, -subdelay)\n"
" -playlist <file> urüenie s£boru so zoznamom prehr†vanÏch s£borov\n"
" -vid x -aid y   vÏber ü°sla video (x) a audio (y) pr£du pre prehr†vanie\n"
" -fps x -srate y voñba pre zmenu video (x fps) a audio (y Hz) frekvencie\n"
" -pp <quality>   aktiv†cia postprocesing filtra (0-4 pre DivX, 0-63 pre mpegy)\n"
" -framedrop      povoliú zahadzovanie sn°mkov (pre pomalÇ stroje)\n"
"\n"
"Z†kl. kl†vesy:   (pre kompl. pozrite aj man str†nku a input.conf)\n"
" <-  alebo  ->   posun vzad/vpred o 10 sekund\n"
" hore / dole     posun vzad/vpred o  1 min£tu\n"
" pgup alebo pgdown  posun vzad/vpred o 10 min£t\n"
" < alebo >       posun vzad/vpred v zozname prehr†vanÏch s£borov\n"
" p al. medzern°k pauza pri prehr†van° (pokraüovan° stlaüen°m niektorej kl†vesy)\n"
" q alebo ESC     koniec prehr†vania a ukonüenie programu\n"
" + alebo -       upraviú spozdenie zvuku v krokoch +/- 0.1 sekundy\n"
" o               cyklick† zmena reßimu OSD:  niü / poz°cia / poz°cia+üas\n"
" * alebo /       pridaú alebo ubraú hlasitosú (stlaüen°m 'm' vÏber master/pcm)\n"
" z alebo x       upraviú spozdenie titulkov v krokoch +/- 0.1 sekundy\n"
" r alebo t       upraviú poz°ciu titulkov hore/dole, pozrite tieß -vop !\n"
"\n"
" * * * * PRE¨÷TAJTE SI MAN STRµNKU PRE DETAILY (“ALÊIE VOïBY A KLµVESY)! * * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================
// mplayer.c:

#define MSGTR_Exiting "\nKonü°m... (%s)\n"
#define MSGTR_Exit_quit "Koniec"
#define MSGTR_Exit_eof "Koniec s£boru"
#define MSGTR_Exit_error "Z†vaßn† chyba"
#define MSGTR_IntBySignal "\nMPlayer preruÁenÏ sign†lom %d v module: %s \n"
#define MSGTR_NoHomeDir "Nemìßem najsú dom†ci (HOME) adres†r\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problÇm\n"
#define MSGTR_CreatingCfgFile "Vytv†ram konfiguraünÏ s£bor: %s\n"
#define MSGTR_InvalidVOdriver "NeplatnÇ meno vÏstupnÇho videoovl†daüa: %s\nPoußite '-vo help' pre zoznam dostupnÏch ovl†daüov.\n"
#define MSGTR_InvalidAOdriver "NeplatnÇ meno vÏstupnÇho audioovl†daüa: %s\nPoußite '-ao help' pre zoznam dostupnÏch ovl†daüov.\n"
#define MSGTR_CopyCodecsConf "(copy/ln etc/codecs.conf (zo zdrojovÏch k¢dov MPlayeru) do ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Pouß°vam vstavanÇ defaultne codecs.conf\n"
#define MSGTR_CantLoadFont "Nemìßem naü°taú font: %s\n"
#define MSGTR_CantLoadSub "Nemìßem naü°taú titulky: %s\n"
#define MSGTR_ErrorDVDkey "Chyba pri spracovan° kñ£üa DVD.\n"
#define MSGTR_CmdlineDVDkey "DVD kñ£ü poßadovanÏ na pr°kazovom riadku je uschovanÏ pre rozk¢dovanie.\n"
#define MSGTR_DVDauthOk "DVD sekvencia overenia autenticity vypad† v poriadku.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: FATAL: poßadovanÏ pr£d chÏba!\n"
#define MSGTR_CantOpenDumpfile "Nejde otvoriú s£bor pre dump!!!\n"
#define MSGTR_CoreDumped "jadro vyp°sanÇ :)\n"
#define MSGTR_FPSnotspecified "V hlaviüke s£boru nie je udanÇ (alebo je zlÇ) FPS! Poußite voñbu -fps !\n"
#define MSGTR_TryForceAudioFmtStr "Pok£Áam sa vyn£tiú rodinu audiokodeku %s ...\n"
#define MSGTR_CantFindAfmtFallback "Nemìßem n†jsú audio kodek pre poßadovan£ rodinu, poußijem ostatnÇ.\n"
#define MSGTR_CantFindAudioCodec "Nemìßem n†jsú kodek pre audio form†t 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Pok£ste sa upgradovaú %s z etc/codecs.conf\n*** Pokiañ problÇm pretrv†, preü°tajte si DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Nejde inicializovaú audio kodek! -> bez zvuku\n"
#define MSGTR_TryForceVideoFmtStr "Pok£Áam se vn£tiú rodinu videokodeku %s ...\n"
#define MSGTR_CantFindVideoCodec "Nemìßem najsú kodek pre video form†t 0x%X !\n"
#define MSGTR_VOincompCodec "¶iañ, vybranÇ video_out zariadenie je nekompatibilnÇ s tÏmto kodekom.\n"
#define MSGTR_CannotInitVO "FATAL: Nemìßem inicializovaú video driver!\n"
#define MSGTR_CannotInitAO "nemìßem otvoriú/inicializovaú audio driver -> TICHO\n"
#define MSGTR_StartPlaying "Zaü°nam prehr†vaú...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***********************************************************\n"\
"         ****  Na prehratie tohoto je v†Á systÇm pr°liÁ POMALÌ!  ****\n"\
"         ***********************************************************\n"\
"!!! MoßnÇ pr°üiny, problÇmy a rieÁenia:\n"\
"- NejüastejÁie: nespr†vny/chybnÏ _zvukovÏ_ ovl†daü. RieÁenie: sk£ste -ao sdl al. poußite\n"\
"  ALSA 0.5 alebo oss emul†ciu z ALSA 0.9. viac tipov sa dozviete v DOCS/sound.html!\n"\
"- PomalÏ video vÏstup. Sk£ste inÏ -vo ovl†daü (pre zoznam: -vo help) alebo sk£ste\n"\
"  s voñbou -framedrop !  Tipy pre ladenie/zrÏchlenie videa s£ v DOCS/video.html\n"\
"- PomalÏ cpu. Nesk£Áajte prehr†vaú veñkÇ dvd/divx na pomalom cpu! Sk£ste -hardframedrop\n"\
"- PoÁkodenÏ s£bor. Sk£ste rìzne kombin†cie tÏchto volieb: -nobps  -ni  -mc 0  -forceidx\n"\
"- PomalÇ mÇdium (NFS/SMB, DVD, VCD ...). Sk£ste -cache 8192.\n"\
"- Pouß°vate -cache na prehr†vanie non-interleaved s£boru? sk£ste -nocache\n"\
"Pokiañ niü z toho nie je pravda, preü°tajte si DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer bol preloßenÏ BEZ podpory GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI vyßaduje X11!\n"
#define MSGTR_Playing "Prehr†vam %s\n"
#define MSGTR_NoSound "Audio: bez zvuku!!!\n"
#define MSGTR_FPSforced "FPS vn£tenÇ na hodnotu %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "SkompilovnÇ s RUNTIME CPU Detection - varovanie, nie je to optim†lne! Na z°skanie max. vÏkonu, rekompilujte mplayer zo zdrojakov s --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "SkompilovanÇ pre x86 CPU s rozÁ°reniami:"
#define MSGTR_AvailableVideoOutputPlugins "DostupnÇ video vÏstupnÇ pluginy:\n"
#define MSGTR_AvailableVideoOutputDrivers "DostupnÇ video vÏstupnÇ ovl†daüe:\n"
#define MSGTR_AvailableAudioOutputDrivers "DostupnÇ audio vÏstupnÇ ovl†daüe:\n"
#define MSGTR_AvailableAudioCodecs "DostupnÇ audio kodeky:\n"
#define MSGTR_AvailableVideoCodecs "DostupnÇ video kodeky:\n"
#define MSGTR_AvailableAudioFm "\nDostupnÇ (vkompilovanÇ) audio rodiny kodekov/ovl†daüe:\n"
#define MSGTR_AvailableVideoFm "\nDostupnÇ (vkompilovanÇ) video rodiny kodekov/ovl†daüe:\n"
#define MSGTR_AvailableFsType "DostupnÇ zmeny plnoobrazovkovÏch m¢dov:\n
#define MSGTR_UsingRTCTiming "Pouß°vam LinuxovÇ hardvÇrovÇ RTC üasovanie (%ldHz)\n"
#define MSGTR_CannotReadVideoProperties "Video: nemìßem ü°taú vlastnosti\n"
#define MSGTR_NoStreamFound "Nen†jdenÏ pr£d\n"
#define MSGTR_InitializingAudioCodec "Initializujem audio kodek...\n"
#define MSGTR_ErrorInitializingVODevice "Chyba pri otv†ran°/inicializ†cii vybranÏch video_out (-vo) zariaden°!\n"
#define MSGTR_ForcedVideoCodec "Vn£tenÏ video kodek: %s\n"
#define MSGTR_ForcedAudioCodec "Vn£tenÏ video kodek: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Popis: %s\nAO: Autor: %s\n"
#define MSGTR_AOComment "AO: Koment†r: %s\n"
#define MSGTR_Video_NoVideo "Video: ßiadne video!!!\n"
#define MSGTR_NotInitializeVOPorVO "\nFATAL: Nemìßem inicializovaú video filtre (-vop) alebo video vÏstup (-vo) !\n"
#define MSGTR_Paused "\n------ PAUZA -------\r"
#define MSGTR_PlaylistLoadUnable "\nNemìßem naü°taú playlist %s\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer zhavaroval na 'Illegal Instruction'.\n"\
"  Mìße to byú chyba v naÁom novom k¢de na detekciu procesora...\n"\
"  Pros°m preü°tajte si DOCS/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer zhavaroval na 'Illegal Instruction'.\n"\
"  Obyüajne sa to st†va, ke‘ ho pouß°vate na inom procesore ako pre ktorÏ bol\n"\
"  skompilovanÏ/optimalizovanÏ.\n  Skontrolujte si to!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer zhavaroval nespr†vnym poußit°m CPU/FPU/RAM.\n"\
"  Prekompilujte MPlayer s --enable-debug a urobte 'gdb' backtrace a\n"\
"  disassemblujte. Pre detaily, pozrite DOCS/bugreports.html#crash\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer zhavaroval. To sa nemalo staú.\n"\
"  Mìße to byú chyba v MPlayer k¢de _alebo_ vo VaÁ°ch ovl†daüoch _alebo_ gcc\n"\
"  verzii. Ak si mysl°te, ße je to chyba MPlayeru, pros°m preü°tajte si DOCS/bugreports.html\n"\
"  a postupujte podña inÁtrukcii. Nemìßeme V†m pomìcú, pokiañ neposkytnete\n"\
"  tieto inform†cie pri ohlasovan° moßnej chyby.\n"

// mencoder.c:

#define MSGTR_MEncoderCopyright "(C) 2000-2003 Arpad Gereoffy (vi‘. DOCS!)\n"
#define MSGTR_UsingPass3ControllFile "Pouß°vam pass3 ovl†dac° s£bor: %s\n"
#define MSGTR_MissingFilename "\nChÏbaj£ce meno s£boru!\n\n"
#define MSGTR_CannotOpenFile_Device "Nemìßem otvoriú s£bor/zariadenie\n"
#define MSGTR_ErrorDVDAuth "Chyba v DVD auth...\n"
#define MSGTR_CannotOpenDemuxer "Nemìßem otvoriú demuxer\n"
#define MSGTR_NoAudioEncoderSelected "\n¶iaden encoder (-oac) vybranÏ! Vyberte jeden alebo -nosound. Poußite -oac help !\n"
#define MSGTR_NoVideoEncoderSelected "\n¶iaden encoder (-ovc) vybranÏ! Vyberte jeden, poußite -ovc help !\n"
#define MSGTR_InitializingAudioCodec "Inicializujem audio kodek...\n"
#define MSGTR_CannotOpenOutputFile "Nemìßem otvoriú s£bor '%s'\n"
#define MSGTR_EncoderOpenFailed "Zlyhal to open the encoder\n"
#define MSGTR_ForcingOutputFourcc "Vnucujem vÏstupnÏ form†t (fourcc) na %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "Zapisujem AVI hlaviüku...\n"
#define MSGTR_DuplicateFrames "\nduplikujem %d snimkov!!!    \n"
#define MSGTR_SkipFrame "\npreskoüiú sn°mok!!!    \n"
#define MSGTR_ErrorWritingFile "%s: chyba pri z†pise s£boru.\n"
#define MSGTR_WritingAVIIndex "\nzapisujem AVI index...\n"
#define MSGTR_FixupAVIHeader "Opravujem AVI hlaviüku...\n"
#define MSGTR_RecommendedVideoBitrate "Doporuüen† rÏchlost bit. toku videa pre %s CD: %d\n"
#define MSGTR_VideoStreamResult "\nVideo pr£d: %8.3f kbit/s  (%d bps)  velkosú: %d bytov  %5.3f sekund  %d sn°mkov\n"
#define MSGTR_AudioStreamResult "\nAudio pr£d: %8.3f kbit/s  (%d bps)  velkosú: %d bytov  %5.3f sekund\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     met¢da variabilnej bit. rÏchlosti \n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(default)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           priemern† bit. rÏchlosú\n"\
"\n"\
" cbr           konÁtantn† bit. rÏchlosú\n"\
"               Vn£ti tieß CBR m¢d na podsekvenci†ch ABR m¢dov\n"\
"\n"\
" br=<0-1024>   Ápecifikovaú bit. rÏchlosú v kBit (plat° iba pre CBR a ABR)\n"\
"\n"\
" q=<0-9>       kvalita (0-najvyÁÁia, 9-najnißÁia) (iba pre VBR)\n"\
"\n"\
" aq=<0-9>      algoritmick† kvalita (0-najlep./najpomalÁia, 9-najhorÁia/najrÏchl.)\n"\
"\n"\
" ratio=<1-100> kompresnÏ pomer\n"\
"\n"\
" vol=<0-10>    nastavenie audio zosilnenia\n"\
"\n"\
" mode=<0-3>    (default: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: no\n"\
"                1: all\n"\
"                2: adjust\n"\
"\n"\
" fast          prepn£ú na rÏchlejÁie k¢dovanie na na podsekvenci†ch VBR m¢dov,\n"\
"               mierne nißÁia kvalita and vyÁÁia bit. rÏchlosú.\n"\
"\n"\
" preset=<value> umoßÂuje najvyÁÁie moßnÇ nastavenie kvality.\n"\
"                 medium: VBR  k¢dovanie,  dobr† kvalita\n"\
"                 (150-180 kbps rozpÑtie bit. rÏchlosti)\n"\
"                 standard:  VBR k¢dovanie, vysok† kvalita\n"\
"                 (170-210 kbps rozpÑtie bit. rÏchlosti)\n"\
"                 extreme: VBR k¢dovanie, velmi vysok† kvalita\n"\
"                 (200-240 kbps rozpÑtie bit. rÏchlosti)\n"\
"                 insane:  CBR  k¢dovanie, najvyÁÁie nastavenie kvality\n"\
"                 (320 kbps bit. rÏchlosú)\n"\
"                 <8-320>: ABR k¢dovanie na zadanej kbps bit. rÏchlosti.\n\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM zariadenie '%s' nen†jdenÇ!\n"
#define MSGTR_ErrTrackSelect "Chyba pri vÏbere VCD stopy!"
#define MSGTR_ReadSTDIN "¨°tam z stdin...\n"
#define MSGTR_UnableOpenURL "Nejde otvoriú URL: %s\n"
#define MSGTR_ConnToServer "PripojenÏ k servru: %s\n"
#define MSGTR_FileNotFound "S£bor nen†jdenÏ: '%s'\n"

#define MSGTR_SMBInitError "Nemìßem inicializovaú knißnicu libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Nemìßem otvoriú z lan: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer mebol skompilovanÏ s podporou ü°tania z SMB\n"

#define MSGTR_CantOpenDVD "Nejde otvoriú DVD zariadenie: %s\n"
#define MSGTR_DVDwait "¨°tam Átrukt£ru disku, pros°m üakajte...\n"
#define MSGTR_DVDnumTitles "Na tomto DVD je %d titulov.\n"
#define MSGTR_DVDinvalidTitle "NeplatnÇ ü°slo DVD titulu: %d\n"
#define MSGTR_DVDnumChapters "Na tomto DVD je %d kapitol.\n"
#define MSGTR_DVDinvalidChapter "NeplatnÇ ü°slo kapitoly DVD: %d\n"
#define MSGTR_DVDnumAngles "Na tomto DVD je %d £hlov pohñadov.\n"
#define MSGTR_DVDinvalidAngle "NeplatnÇ ü°slo uhlu pohñadu DVD: %d\n"
#define MSGTR_DVDnoIFO "Nemìßem otvoriú s£bor IFO pre DVD titul %d.\n"
#define MSGTR_DVDnoVOBs "Nemìßem otvoriú VOB s£bor (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD £speÁne otvorenÇ.\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Upozornenie! Hlaviüka audio pr£du %d predefinovan†!\n"
#define MSGTR_VideoStreamRedefined "Upozornenie! Hlaviüka video pr£du %d predefinovan†!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Pr°liÁ mnoho (%d v %d bajtech) audio paketov v bufferi!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Pr°liÁ mnoho (%d v %d bajtech) video paketov v bufferi!\n"
#define MSGTR_MaybeNI "(moßno prehr†vate neprekladanÏ pr£d/s£bor alebo kodek zlyhal)\n" \
		      "Pre .AVI s£bory sk£ste vyn£tiú neprekladanÏ m¢d voñbou -ni\n"
#define MSGTR_SwitchToNi "\nDetekovanÏ zle prekladanÏ .AVI - prepnite -ni m¢d!\n"
#define MSGTR_Detected_XXX_FileFormat "DetekovanÏ %s form†t s£boru!\n"
#define MSGTR_DetectedAudiofile "DetekovanÏ audio s£bor!\n"
#define MSGTR_NotSystemStream "Nie je to MPEG System Stream form†t... (moßno Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "NeplatnÏ MPEG-ES pr£d??? kontaktujte autora, moßno je to chyba (bug) :(\n"
#define MSGTR_FormatNotRecognized "========== ¶iañ, tento form†t s£boru nie je rozpoznanÏ/podporovanÏ =======\n"\
				  "==== Pokiañ je tento s£bor AVI, ASF alebo MPEG pr£d, kontaktujte autora! ====\n"
#define MSGTR_MissingVideoStream "¶iadny video pr£d nen†jdenÏ!\n"
#define MSGTR_MissingAudioStream "¶iadny audio pr£d nen†jdenÏ...  -> bez zvuku\n"
#define MSGTR_MissingVideoStreamBug "ChÏbaj£ci video pr£d!? Kontaktujte autora, moßno to je chyba (bug) :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: s£bor neobsahuje vybranÏ audio alebo video pr£d\n"

#define MSGTR_NI_Forced "Vn£tenÏ"
#define MSGTR_NI_Detected "DetekovanÏ"
#define MSGTR_NI_Message "%s NEPREKLADANÌ form†t s£boru AVI!\n"

#define MSGTR_UsingNINI "Pouß°vam NEPREKLADANÌ poÁkodenÏ form†t s£boru AVI!\n" 
#define MSGTR_CouldntDetFNo "Nemìßem urüiú poüet sn°mkov (pre absol£tny posun)  \n"
#define MSGTR_CantSeekRawAVI "Nemìßem sa pos£vaú v surovÏch (raw) .AVI pr£doch! (Potrebujem index, zkuste pouß°ú voñbu -idx !)  \n"
#define MSGTR_CantSeekFile "Nemìßem sa pos£vaú v tomto s£bore!  \n"

#define MSGTR_EncryptedVOB "K¢dovanÏ VOB s£bor (preloßenÇ bez podpory libcss)! Preü°tajte si DOCS/DVD\n"
#define MSGTR_EncryptedVOBauth "Zak¢dovanÏ pr£d, ale overenie autenticity ste nepoßadovali!!\n"

#define MSGTR_MOVcomprhdr "MOV: KomprimovanÇ hlaviüky nie s£ (eÁte) podporovanÇ!\n"
#define MSGTR_MOVvariableFourCC "MOV: Upozornenie! premenn† FOURCC detekovan†!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Upozornenie! Pr°liÁ veña stìp!"
#define MSGTR_FoundAudioStream "==> N†jdenÏ audio pr£d: %d\n"
#define MSGTR_FoundVideoStream "==> N†jdenÏ video pr£d: %d\n"
#define MSGTR_DetectedTV "TV detekovanÏ ! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Nemìßem otvoriú ogg demuxer\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Hñad†m audio pr£d (id:%d)\n"
#define MSGTR_CannotOpenAudioStream "Nemìßem otvoriú audio pr£d: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nemìßem otvoriú pr£d titulkov: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Nemìßem otvoriú audio demuxer: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Nemìßem otvoriú demuxer titulkov: %s\n"
#define MSGTR_TVInputNotSeekable "v TV vstupe nie je moßnÇ sa pohybovaú! (moßno posun bude na zmenu kan†lov ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Demuxer info %s uß pr°tomnÇ\n!"
#define MSGTR_ClipInfo "Inform†cie o klipe: \n"

#define MSGTR_LeaveTelecineMode "\ndemux_mpg: Progres°vna seq detekovan†, nech†vam m¢d 3:2 TELECINE \n"
#define MSGTR_EnterTelecineMode "\ndemux_mpg: 3:2 TELECINE detekovanÇ, zap°nam inverznÇ telecine fx. FPS zmenenÇ na %5.3f!  \n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nemìßem otvoriú kodek\n"
#define MSGTR_CantCloseCodec "nemìßem uzavieú kodek\n"

#define MSGTR_MissingDLLcodec "CHYBA: Nemìßem otvoriú potrebnÏ DirectShow kodek: %s\n"
#define MSGTR_ACMiniterror "Nemìßem naü°taú/inicializovaú Win32/ACM AUDIO kodek (chÏbaj£ci s£bor DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nemìßem najsú kodek '%s' v libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF - koniec s£boru v priebehu vyhñad†vania hlaviüky sekvencie\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Nemìßem preü°taú hlaviüku sekvencie!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Nemìßem preü°taú rozÁ°renie hlaviüky sekvencie!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Zl† hlaviüka sekvencie!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: ZlÇ rozÁ°renie hlaviüky sekvencie!\n"

#define MSGTR_ShMemAllocFail "Nemìßem alokovaú zdieñan£ pamÑú\n"
#define MSGTR_CantAllocAudioBuf "Nemìßem alokovaú pamÑú pre vÏstupnÏ audio buffer\n"

#define MSGTR_UnknownAudio "Nezn†my/chÏbaj£ci audio form†t -> bez zvuku\n"

#define MSGTR_UsingExternalPP "[PP] Pouß°vam externÏ postprocessing filter, max q = %d\n"
#define MSGTR_UsingCodecPP "[PP] Poß°vam postprocessing z kodeku, max q = %d\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Video atrib£t '%s' nie je podporovanÏ vÏberom vo & vd! \n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Poßadovan† rodina video kodekov [%s] (vfm=%s) nie je dostupn† (zapnite ju pri kompil†cii!)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Poßadovan† rodina audio kodekov [%s] (afm=%s) nie je dostupn† (zapnite ju pri kompil†cii!)\n"
#define MSGTR_OpeningVideoDecoder "Otv†ram video dek¢der: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Otv†ram audio dek¢der: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit video: %s  \n"
#define MSGTR_UninitAudioStr "uninit audio: %s  \n"
#define MSGTR_VDecoderInitFailed "VDecoder init zlyhal :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder init zlyhal :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder preinit zlyhal :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alokujem %d bytov pre vstupnÏ buffer\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alokujem %d + %d = %d bytov pre vÏstupnÏ buffer\n"
			 
// LIRC:
#define MSGTR_SettingUpLIRC "Nastavujem podporu lirc ...\n"
#define MSGTR_LIRCdisabled "Nebudete mìcú pouß°vaú diañkovÏ ovl†daü.\n"
#define MSGTR_LIRCopenfailed "Zlyhal pokus o otvorenie podpory LIRC!\n"
#define MSGTR_LIRCcfgerr "Zlyhalo ü°tanie konfiguraünÇho s£boru LIRC %s !\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nemìßem n†jsú video filter '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Nemìßem otvoriú video filter '%s'\n"
#define MSGTR_OpeningVideoFilter "Otv†ram video filter: "
#define MSGTR_CannotFindColorspace "Nemìßem n†jsú beßnÏ priestor farieb, ani vloßen°m 'scale' :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: kodek nenastavil sh->disp_w a sh->disp_h, sk£Áam to ob°sú!\n"
#define MSGTR_VoConfigRequest "VDec: vo konfiguraün† poßiadavka - %d x %d (preferovanÏ csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Nemìßem n†jsú zhodnÏ priestor farieb - sk£Áam znova s -vop scale...\n"
#define MSGTR_MovieAspectIsSet "Movie-Aspect je %.2f:1 - men°m rozmery na spr†vne.\n"
#define MSGTR_MovieAspectUndefined "Movie-Aspect je nedefinovnÏ - nemenia sa rozmery.\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O aplik†cii"
#define MSGTR_FileSelect "Vybraú s£bor ..."
#define MSGTR_SubtitleSelect "Vybraú titulky ..."
#define MSGTR_OtherSelect "Vybraú ..."
#define MSGTR_AudioFileSelect "Vybraú externÏ audio kan†l ..."
#define MSGTR_FontSelect "Vybraú font ..."
#define MSGTR_PlayList "PlayList"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_SkinBrowser "Prehliadaü tÇm"
#define MSGTR_Network "SieúovÇ prehr†vanie (streaming) ..."
#define MSGTR_Preferences "Preferencie"
#define MSGTR_OSSPreferences "konfigur†cia OSS ovl†daüa"
#define MSGTR_SDLPreferences "konfigur†cia SDL ovl†daüa"
#define MSGTR_NoMediaOpened "ßiadne mÇdium otvorenÇ"
#define MSGTR_VCDTrack "VCD stopa %d"
#define MSGTR_NoChapter "ßiadna kapitola"
#define MSGTR_Chapter "kapitola %d"
#define MSGTR_NoFileLoaded "nenahranÏ ßiaden s£bor"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "ZruÁiú"
#define MSGTR_Add "Pridaú"
#define MSGTR_Remove "Odobraú"
#define MSGTR_Clear "Vyüistiú"
#define MSGTR_Config "Konfigur†cia"
#define MSGTR_ConfigDriver "Konfigurovaú ovl†daü"
#define MSGTR_Browse "Prehliadaú"


// --- error messages ---
#define MSGTR_NEMDB "¶iañ, nedostatok pamÑte pre buffer na kreslenie."
#define MSGTR_NEMFMR "¶iañ, nedostatok pamÑte pre vytv†ranie menu."
#define MSGTR_IDFGCVD "¶iañ, nemìßem n†jsú gui kompatibilnÏ ovl†daü video vÏstupu."
#define MSGTR_NEEDLAVCFAME "¶iañ, nemìßete prehr†vaú nie mpeg s£bory s DXR3/H+ zariaden°m bez prek¢dovania.\nPros°m zapnite lavc alebo fame v DXR3/H+ konfig. okne."
   
// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[tÇmy] chyba v konfig. s£bore tÇm %d: %s"
#define MSGTR_SKIN_WARNING1 "[tÇmy] varovanie v konfig. s£bore tÇm na riadku %d: widget najdenÏ ale pred  \"section\" nen†jdenÏ ( %s )"
#define MSGTR_SKIN_WARNING2 "[tÇmy] varovanie v konfig. s£bore tÇm na riadku %d: widget najdenÏ ale pred \"subsection\" nen†jdenÏ (%s)"
#define MSGTR_SKIN_WARNING3 "[skin] varovanie v konfig. s£bore tÇm na riadku %d: t†to subsekcia nie je podporovan† tÏmto widget (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "bitmapa s híbkou 16 bit a menej je nepodporovan† ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "s£bor nen†jdenÏ ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "chyba ü°tania bmp ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "chyba ü°tania tga ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "chyba ü°tania png ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "form†t RLE packed tga nepodporovanÏ ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "nezn†my typ s£boru ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "chyba konverzie z 24 bit do 32 bit ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "nezn†ma spr†va: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nedostatok pamÑte\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "pr°liÁ mnoho fontov deklarovanÏch\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "s£bor fontov nen†jdenÏ\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "s£bor obrazov fontu nen†jdenÏ\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "neexistuj£ci identifik†tor fontu ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "nezn†my parameter ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[prehliadaü tÇm] nedostatok pamÑte.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin nen†jdenÏ ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Chyba pri ü°tan° konfiguraünÇho s£boru tÇm ( %s ).\n"
#define MSGTR_SKIN_LABEL "TÇmy:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O aplik†cii MPlayer"
#define MSGTR_MENU_Open "Otvoriú ..."
#define MSGTR_MENU_PlayFile "Prehraú s£bor ..."
#define MSGTR_MENU_PlayVCD "Prehraú VCD ..."
#define MSGTR_MENU_PlayDVD "Prehraú DVD ..."
#define MSGTR_MENU_PlayURL "Prehraú URL ..."
#define MSGTR_MENU_LoadSubtitle "Naü°taú titulky ..."
#define MSGTR_MENU_DropSubtitle "Zahodiú titulky ..."
#define MSGTR_MENU_LoadExternAudioFile "Naü°taú externÏ audio s£bor ..."
#define MSGTR_MENU_Playing "Prehr†vam"
#define MSGTR_MENU_Play "Prehraú"
#define MSGTR_MENU_Pause "Pauza"
#define MSGTR_MENU_Stop "Zastaviú"
#define MSGTR_MENU_NextStream "“alÁ° pr£d"
#define MSGTR_MENU_PrevStream "Predch†dzaj£ci pr£d"
#define MSGTR_MENU_Size "Veñkosú"
#define MSGTR_MENU_NormalSize "Norm†lna veñkosú"
#define MSGTR_MENU_DoubleSize "Dvojn†sobn† veñkosú"
#define MSGTR_MENU_FullScreen "Cel† obrazovka"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Prehraú disk ..."
#define MSGTR_MENU_ShowDVDMenu "Zobraziú DVD menu"
#define MSGTR_MENU_Titles "Tituly"
#define MSGTR_MENU_Title "Titul %2d"
#define MSGTR_MENU_None "(niü)"
#define MSGTR_MENU_Chapters "Kapitoly"
#define MSGTR_MENU_Chapter "Kapitola %2d"
#define MSGTR_MENU_AudioLanguages "Jazyk zvuku"
#define MSGTR_MENU_SubtitleLanguages "Jazyk titulkov"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Prehliadaü tÇm"
#define MSGTR_MENU_Preferences "Nastavenia"
#define MSGTR_MENU_Exit "Koniec ..."
#define MSGTR_MENU_Mute "Stlmiú zvuk"
#define MSGTR_MENU_Original "Origin†l"
#define MSGTR_MENU_AspectRatio "Pomer str†n obrazu"
#define MSGTR_MENU_AudioTrack "Audio stopa"
#define MSGTR_MENU_Track "Stopa %d"
#define MSGTR_MENU_VideoTrack "Video stopa"

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Jas: "
#define MSGTR_EQU_Hue "OdtieÂ: "
#define MSGTR_EQU_Saturation "NasÏtenie: "
#define MSGTR_EQU_Front_Left "PrednÏ ïavÏ"
#define MSGTR_EQU_Front_Right "PrednÏ PravÏ"
#define MSGTR_EQU_Back_Left "ZadnÏ ïavÏ"
#define MSGTR_EQU_Back_Right "ZadnÏ PravÏ"
#define MSGTR_EQU_Center "StrednÏ"
#define MSGTR_EQU_Bass "BasovÏ"
#define MSGTR_EQU_All "VÁetko"
#define MSGTR_EQU_Channel1 "Kan†l 1:"
#define MSGTR_EQU_Channel2 "Kan†l 2:"
#define MSGTR_EQU_Channel3 "Kan†l 3:"
#define MSGTR_EQU_Channel4 "Kan†l 4:"
#define MSGTR_EQU_Channel5 "Kan†l 5:"
#define MSGTR_EQU_Channel6 "Kan†l 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Cesta"
#define MSGTR_PLAYLIST_Selected "VybranÇ s£bory"
#define MSGTR_PLAYLIST_Files "S£bory"
#define MSGTR_PLAYLIST_DirectoryTree "Adres†rovÏ strom"

// --- preferences
#define MSGTR_PREFERENCES_Audio "Audio"
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_SubtitleOSD "Titulky a OSD"
#define MSGTR_PREFERENCES_Misc "Rìzne"

#define MSGTR_PREFERENCES_None "Niü"
#define MSGTR_PREFERENCES_AvailableDrivers "DostupnÇ ovl†daüe:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nehraú zvuk"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizovaú zvuk"
#define MSGTR_PREFERENCES_EnEqualizer "Zapn£ú equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "Zapn£ú extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Koeficient:"
#define MSGTR_PREFERENCES_AudioDelay "Audio oneskorenie"
#define MSGTR_PREFERENCES_DoubleBuffer "Zapn£ú dvojtÏ buffering"
#define MSGTR_PREFERENCES_DirectRender "Zapn£ú direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Povoliú zahadzovanie r†mcov"
#define MSGTR_PREFERENCES_HFrameDrop "Povoliú TVRDê zahadzovanie r†mcov (nebezpeünÇ)"
#define MSGTR_PREFERENCES_Flip "prehodiú obraz horn† strana-dole"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_OSDTimer "¨asovaü a indik†tor"
#define MSGTR_PREFERENCES_OSDProgress "Iba ukazovateñ priebehu"
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "¨asovaü, percent† and celkovÏ üas"
#define MSGTR_PREFERENCES_Subtitle "Titulky:"
#define MSGTR_PREFERENCES_SUB_Delay "Oneskorenie: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Poz°cia: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Zak†zaú automatickÇ nahr†vanie titulkov"
#define MSGTR_PREFERENCES_SUB_Unicode "Titulky v Unicode"
#define MSGTR_PREFERENCES_SUB_MPSUB "Konvertovaú danÇ titulky do MPlayer form†tu"
#define MSGTR_PREFERENCES_SUB_SRT "Konvertovaú danÇ titulky do üasovo-urüenÇho SubViewer (SRT) form†tu"
#define MSGTR_PREFERENCES_SUB_Overlap "Zapn£ú prekrÏvanie titulkov"
#define MSGTR_PREFERENCES_Font "Font:"
#define MSGTR_PREFERENCES_FontFactor "Font faktor:"
#define MSGTR_PREFERENCES_PostProcess "Zapn£ú postprocess"
#define MSGTR_PREFERENCES_AutoQuality "Automatick† qualita: "
#define MSGTR_PREFERENCES_NI "Poußiú neprekladanÏ AVI parser"
#define MSGTR_PREFERENCES_IDX "Obnoviú index tabulku, ak je potrebnÇ"
#define MSGTR_PREFERENCES_VideoCodecFamily "Rodina video kodekov:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Rodina audeo kodekov:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD £roveÂ"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Titulky"
#define MSGTR_PREFERENCES_FRAME_Font "Font"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocess"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Kodek & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Vyrovn†vacia pamÑú"
#define MSGTR_PREFERENCES_FRAME_Misc "Rìzne"
#define MSGTR_PREFERENCES_OSS_Device "Zariadenie:"
#define MSGTR_PREFERENCES_OSS_Mixer "Mixer:"
#define MSGTR_PREFERENCES_SDL_Driver "Ovl†daü:"
#define MSGTR_PREFERENCES_Message "Pros°m pamÑtajte, nietorÇ voñby potrebuj£ reÁtart prehr†vania!"
#define MSGTR_PREFERENCES_DXR3_VENC "Video k¢der:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Poußiú LAVC (ffmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Poußiú FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Western European Languages (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Western European Languages with Euro (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Slavic/Central European Languages (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, Galician, Maltese, Turkish (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Old Baltic charset (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cyrillic (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arabic (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Modern Greek (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Turkish (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltic (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Celtic (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Hebrew charsets (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Russian (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrainian, Belarusian (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Simplified Chinese charset (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Traditional Chinese charset (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japanese charsets (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Korean charset (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thai charset (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrillic Windows (CP1251)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Nemeniú rozmery"
#define MSGTR_PREFERENCES_FontPropWidth "Proporcion†lne k Á°rke obrazu"
#define MSGTR_PREFERENCES_FontPropHeight "Proporcion†lne k vÏÁke obrazu"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporcion†lne k diagon†le obrazu"
#define MSGTR_PREFERENCES_FontEncoding "K¢dovanie:"
#define MSGTR_PREFERENCES_FontBlur "Rozmazanie:"
#define MSGTR_PREFERENCES_FontOutLine "Obrys:"
#define MSGTR_PREFERENCES_FontTextScale "Mierka textu:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD mierka:"
#define MSGTR_PREFERENCES_Cache "Vyrovn†vacia pamÑú zap./vyp."
#define MSGTR_PREFERENCES_LoadFullscreen "NaÁtartovaú v reßime celej obrazovky"
#define MSGTR_PREFERENCES_CacheSize "Veñkosú vyrovn†vacej pamÑte: "
#define MSGTR_PREFERENCES_XSCREENSAVER "Zastaviú XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Zapn£ú playbar"
#define MSGTR_PREFERENCES_AutoSync "Automatick† synchroniz†cia zap./vyp."
#define MSGTR_PREFERENCES_AutoSyncValue "Automatick† synchroniz†cia: "
#define MSGTR_PREFERENCES_CDROMDevice "CD-ROM zariadenie:"
#define MSGTR_PREFERENCES_DVDDevice "DVD zariadenie:"
#define MSGTR_PREFERENCES_FPS "Film FPS:"
#define MSGTR_PREFERENCES_ShowVideoWindow "Uk†zaú video okno pri neaktivite"

#define MSGTR_ABOUT_UHU "vÏvoj GUI sponoroval UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   MPlayer z†kladnÏ tÏm:\n"
#define MSGTR_ABOUT_AdditionalCoders "   “alÁ° vÏvoj†ri:\n"
#define MSGTR_ABOUT_MainTesters "   Hlavn° testeri:\n


// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "fat†lna chyba ..."
#define MSGTR_MSGBOX_LABEL_Error "chyba ..."
#define MSGTR_MSGBOX_LABEL_Warning "upozornenie ..."

#endif