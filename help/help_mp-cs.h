// Translated by:  Jiri Svoboda, jiri.svoboda@seznam.cz
// Updated by:     Tomas Blaha,  tomas.blaha at kapsa.club.cz
// Synced to 1.105
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char help_text[]=
"Pou¾ití:          mplayer [pøepínaèe] [url|cesta/]jméno_souboru\n"
"\n"
"Základní pøepínaèe: (Kompletní seznam najdete v manuálové stránce.)\n"
" -vo <ovl[:zaø]>  urèit výst. video ovladaè a zaøízení (seznam: -vo help)\n"
" -ao <ovl[:zaø]>  urèit výst. audio ovladaè a zaøízení (seznam: -ao help)\n"
#ifdef HAVE_VCD
" vcd://<èíslo>    pøehrát VCD (Video CD) stopu ze zaøízení místo ze souboru\n"
#endif
#ifdef USE_DVDREAD
" dvd://<èíslo>    pøehrát DVD titul ze zaøízení (mechaniky), místo ze souboru\n"
" -alang/-slang    zvolit jazyk zvuku/titulkù na DVD (dvouznakový kód zemì)\n"
#endif
" -ss <pozice>     posunout na danou pozici (sekundy nebo hh:mm:ss)\n"
" -nosound         pøehrát beze zvuku\n"
" -fs              celoobrazovkové pøehrávání (nebo -vm -zoom, viz manuál)\n"
" -x <x> -y <y>    rozli¹ení obrazu (pro pou¾ití s -vm èi -zoom)\n"
" -sub <soubor>    zvolit soubor s titulky (viz také -subfps, -subdelay)\n"
" -playlist <soubor> urèit soubor s playlistem\n"
" -vid x -aid y    vybrat video (x) a audio (y) proud pro pøehrání\n"
" -fps x -srate y  zmìnit video (x fps) a audio (y Hz) frekvence\n"
" -pp <kvalita>    aktivovat následné zpracování (podrobnosti v manuálu)\n"
" -framedrop       povolit zahazování snímkù (pro pomalé stroje)\n"
"\n"
"Základní klávesy: (Kompletní seznam je v manuálu a také v input.conf.)\n"
" <-  nebo  ->     posun vzad/vpøed o 10 sekund\n"
" nahoru èi dolù   posun vzad/vpøed o 1 minutu\n"
" pgup èi pgdown   posun vzad/vpøed o 10 minut\n"
" < nebo >         posun vzad/vpøed v playlistu\n"
" p nebo mezerník  pauza pøi pøehrávání (pokraèování stiskem kterékoliv klávesy)\n"
" q nebo ESC       konec pøehrávání a ukonèení programu\n"
" + nebo -         upravit zpo¾dìní zvuku v krocích +/- 0,1 sekundy\n"
" o                cyklická zmìna re¾imu OSD: nic / pozice / pozice a èas\n"
" * nebo /         pøidat nebo ubrat PCM hlasitost\n"
" z nebo x         upravit zpo¾dìní titulkù v krocích +/- 0,1 sekundy\n"
" r nebo t         upravit polohu titulkù nahoru/dolù, viz také -vf expand\n"
"\n"
" * * * V MAN STRÁNCE NAJDETE PODROBNOSTI, DAL©Í PARAMETRY A KLÁVESY * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nKonèím... (%s)\n"
#define MSGTR_Exit_quit "Konec"
#define MSGTR_Exit_eof "Konec souboru"
#define MSGTR_Exit_error "Záva¾ná chyba"
#define MSGTR_IntBySignal "\nMPlayer pøeru¹en signálem %d v modulu %s.\n"
#define MSGTR_NoHomeDir "Nemohu nalézt domácí adresáø.\n"
#define MSGTR_GetpathProblem "Nastal problém s get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Vytváøím konfiguraèní soubor: %s\n"
#define MSGTR_InvalidVOdriver "©patné jméno výstupního video ovladaèe: %s\nSeznam dostupných ovladaèù zobrazíte pomocí '-vo help'.\n"
#define MSGTR_InvalidAOdriver "©patné jméno výstupního audio ovladaèe: %s\nSeznam dostupných ovladaèù zobrazíte pomocí '-ao help'.\n"
#define MSGTR_CopyCodecsConf "(Zkopírujte nebo vytvoøte odkaz na etc/codecs.conf (ze zdrojových kódù MPlayeru) do ~/.mplayer/codecs.conf)\n"
#define MSGTR_BuiltinCodecsConf "Pou¾ívám výchozí (zabudovaný) codecs.conf.\n"
#define MSGTR_CantLoadFont "Nemohu naèíst písmo: %s\n"
#define MSGTR_CantLoadSub "Nemohu naèíst titulky: %s\n"
#define MSGTR_ErrorDVDkey "Pøi zpracování DVD klíèe do¹lo k chybì.\n"
#define MSGTR_CmdlineDVDkey "Roz¹ifrovávám pomocí zadaného DVD klíèe.\n"
#define MSGTR_DVDauthOk "Autentizaèní sekvence na DVD vypadá vpoøádku.\n"
#define MSGTR_DumpSelectedStreamMissing "dump: Kritická chyba: po¾adovaný proud chybí!\n"
#define MSGTR_CantOpenDumpfile "Nelze otevøít soubor pro dump!!!\n"
#define MSGTR_CoreDumped "Jádro vydumpováno ;)\n"
#define MSGTR_FPSnotspecified "V hlavièce souboru není udán (nebo je ¹patnì) FPS! Pou¾ijte volbu -fps!\n"
#define MSGTR_TryForceAudioFmtStr "Pokou¹ím se vynutit rodinu audiokodeku %s...\n"
#define MSGTR_CantFindAfmtFallback "Nemohu nalézt audio kodek v po¾adované rodinì, pou¾iji ostatní rodiny.\n"
#define MSGTR_CantFindAudioCodec "Nemohu nalézt kodek pro audio formát 0x%X!\n"
#define MSGTR_CouldntInitAudioCodec "Nelze inicializovat audio kodek - nebude zvuk!\n"
#define MSGTR_TryForceVideoFmtStr "Poku¹ím se vynutit rodinu videokodeku %s...\n"
#define MSGTR_CantFindVideoCodec "Nemohu nalézt kodek pro vybraný -vo a video formát 0x%X.\n"
#define MSGTR_VOincompCodec "Bohu¾el, vybrané video_out zaøízení není kompatibilní s tímto kodekem.\n"
#define MSGTR_CannotInitVO "Kritická chyba: Nemohu inicializovat video ovladaè!\n"
#define MSGTR_CannotInitAO "Nemohu otevøít/inicializovat audio zaøízení -> nebude zvuk.\n"
#define MSGTR_StartPlaying "Zaèínám pøehrávat...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***********************************************************\n"\
"         ****  Vá¹ systém je pøíli¹ POMALÝ pro toto pøehrávání! ****\n"\
"         ***********************************************************\n\n"\
"Mo¾né pøíèiny, problémy a øe¹ení:\n"\
"- Nejèastìj¹í: ¹patný/chybný _zvukový_ ovladaè!\n"\
"  - Zkuste -ao sdl nebo pou¾ijte ALSA 0.5 èi oss emulaci z ALSA 0.9.\n"\
"  - Pohrajte si s rùznými hodnotami -autosync, pro zaèátek tøeba 30.\n"\
"- Pomalý obrazový výstup\n"\
"  - Zkuste jiný -vo ovladaè (seznam: -vo help) nebo zkuste -framedrop!\n"\
"- Pomalá CPU\n"\
"  - Nezkou¹ejte pøehrát velké DVD/DivX na pomalé CPU! Zkuste -hardframedrop.\n"\
"- Po¹kozený soubor.\n"\
"  - Zkuste rùzné kombinace tìchto voleb: -nobps -ni -forceidx -mc 0.\n"\
"- Pøi pøehrávání z pomalých médií (NFS/SMB, DVD, VCD, atd.)\n"\
"  - Zkuste -cache 8192.\n"\
"- Pou¾íváte -cache pro neprokládané AVI soubory?\n"\
"  - Zkuste -nocache.\n"\
"Tipy na vyladìní a zrychlení najdete v DOCS/HTML/en/devices.html.\n"\
"Pokud nic z toho nepomù¾e, pøeètìte si DOCS/HTML/en/bugreports.html.\n\n"

#define MSGTR_NoGui "MPlayer byl pøelo¾en BEZ podpory GUI.\n"
#define MSGTR_GuiNeedsX "GUI MPlayeru vy¾aduje X11.\n"
#define MSGTR_Playing "Pøehrávám %s\n"
#define MSGTR_NoSound "Audio: beze zvuku!!!\n"
#define MSGTR_FPSforced "FPS vynuceno na hodnotu %5.3f  (ftime: %5.3f)\n"
#define MSGTR_CompiledWithRuntimeDetection "Pøelo¾eno s detekcí CPU za bìhu - UPOZORNÌNÍ - toto není optimální!\nAbyste získali co nejvìt¹í výkon, pøelo¾te znovu mplayer ze zdrojového kódu\ns pøepínaèem --disable-runtime-cpudetection\n"
#define MSGTR_CompiledWithCPUExtensions "Pøelo¾eno pro CPU x86 s roz¹íøeními:"
#define MSGTR_AvailableVideoOutputPlugins "Dostupné zásuvné moduly video výstupu:\n"
#define MSGTR_AvailableVideoOutputDrivers "Dostupné ovladaèe video výstupu:\n"
#define MSGTR_AvailableAudioOutputDrivers "Dostupné ovladaèe audio výstupu:\n"
#define MSGTR_AvailableAudioCodecs "Dostupné audio kodeky:\n"
#define MSGTR_AvailableVideoCodecs "Dostupné video kodeky:\n"
#define MSGTR_AvailableAudioFm "\nDostupné (pøikompilované) rodiny audio kodekù/ovladaèù:\n"
#define MSGTR_AvailableVideoFm "\nDostupné (pøikompilované) rodiny video kodekù/ovladaèù:\n"
#define MSGTR_AvailableFsType "Dostupné zpùsoby zmìny hladiny pøi celoobrazovkovém zobrazení:\n"
#define MSGTR_UsingRTCTiming "Pou¾ito linuxové hardwarové èasování RTC (%ldHz).\n"
#define MSGTR_CannotReadVideoProperties "Video: nelze pøeèíst vlastnosti.\n"
#define MSGTR_NoStreamFound "Nenalezen ¾ádný proud.\n"
#define MSGTR_InitializingAudioCodec "Inicializuji audio kodek...\n"
#define MSGTR_ErrorInitializingVODevice "Chyba pøi otevírání/inicializaci vybraného video_out (-vo) zaøízení.\n"
#define MSGTR_ForcedVideoCodec "Vynucen video kodek: %s\n"
#define MSGTR_ForcedAudioCodec "Vynucen audio kodek: %s\n"
#define MSGTR_AODescription_AOAuthor "AO: Popis: %s\nAO: Autor: %s\n"
#define MSGTR_AOComment "AO: Poznámka: %s\n"
#define MSGTR_Video_NoVideo "Video: ®ádné video\n"
#define MSGTR_NotInitializeVOPorVO "\nKritická chyba: Nemohu inicializovat video filtry (-vf) nebo video ovladaè (-vo)!\n"
#define MSGTR_Paused "\n===== POZASTAVENO =====\r"
#define MSGTR_PlaylistLoadUnable "\nNemohu naèíst playlist %s.\n"
#define MSGTR_Exit_SIGILL_RTCpuSel \
"- MPlayer havaroval kvùli 'Illegal Instruction'.\n"\
"  To mù¾e být chyba v kódu pro rozpoznání CPU za bìhu...\n"\
"  Prosím, pøeètìte si DOCS/HTML/en/bugreports.html.\n"
#define MSGTR_Exit_SIGILL \
"- MPlayer havaroval kvùli 'Illegal Instruction'.\n"\
"  To se obvykle stává, kdy¾ se ho pokusíte spustit na CPU odli¹ném, ne¾ pro který\n"\
"  byl pøelo¾en/optimalizován.\n  Ovìøte to!\n"
#define MSGTR_Exit_SIGSEGV_SIGFPE \
"- MPlayer havaroval kvùli ¹patnému pou¾ití CPU/FPU/RAM.\n"\
"  Pøelo¾te MPlayer s volbou --enable-debug , proveïte 'gdb' backtrace\n"\
"  a disassembly. Detaily najdete v DOCS/HTML/en/bugreports_what.html#bugreports_crash.\n"
#define MSGTR_Exit_SIGCRASH \
"- MPlayer havaroval. To by se nemìlo stát.\n"\
"  Mù¾e to být chyba v kódu MPlayeru _nebo_ ve va¹ich ovladaèích _nebo_ ve verzi\n"\
"  va¹eho gcc. Pokud si myslíte, ¾e je to chyba MPlayeru, pøeètìte si, prosím,\n"\
"  DOCS/HTML/en/bugreports.html a pokraèujte dle tam uvedeného návodu. My vám nemù¾eme\n"\
"  pomoci, pokud tyto informace neuvedete pøi ohla¹ování mo¾né chyby.\n"

// mencoder.c:

#define MSGTR_UsingPass3ControllFile "Øídicí soubor pro tøetí prùbìh (pass3): %s\n"
#define MSGTR_MissingFilename "\nChybí jméno souboru.\n\n"
#define MSGTR_CannotOpenFile_Device "Nelze otevøít soubor/zaøízení.\n"
#define MSGTR_ErrorDVDAuth "Chyba pøi autentizaci DVD.\n"
#define MSGTR_CannotOpenDemuxer "Nemohu otevøít demuxer.\n"
#define MSGTR_NoAudioEncoderSelected "\nNebyl vybrán enkodér zvuku (-oac). Nìjaký vyberte nebo pou¾ijte -nosound. Pou¾ijte -oac help!\n"
#define MSGTR_NoVideoEncoderSelected "\nNebyl vybrán enkodér videa (-ovc). Nìjaký vyberte. Pou¾ijte -ovc help!\n"
#define MSGTR_InitializingAudioCodec "Inicializuji audio kodek...\n"
#define MSGTR_CannotOpenOutputFile "Nemohu otevøít výstupní soubor '%s'\n"
#define MSGTR_EncoderOpenFailed "Nepovedlo se otevøít enkodér\n"
#define MSGTR_ForcingOutputFourcc "Vynucuji výstupní fourcc na %x [%.4s]\n"
#define MSGTR_WritingAVIHeader "Zapisuji hlavièku AVI...\n"
#define MSGTR_DuplicateFrames "\n%d opakujících se snímkù!\n"
#define MSGTR_SkipFrame "\nPøeskakuji snímek!\n"
#define MSGTR_ErrorWritingFile "%s: chyba pøi zápisu souboru.\n"
#define MSGTR_WritingAVIIndex "\nZapisuji AVI index...\n"
#define MSGTR_FixupAVIHeader "Opravuji hlavièku AVI...\n"
#define MSGTR_RecommendedVideoBitrate "Doporuèený datový tok videa pro CD %s: %d\n"
#define MSGTR_VideoStreamResult "\nVideo proud: %8.3f kbit/s  (%d bps)  velikost: %d bajtù  %5.3f sekund  %d snímkù\n"
#define MSGTR_AudioStreamResult "\nAudio proud: %8.3f kbit/s  (%d bps)  velikost: %d bajtù  %5.3f sekund\n"

// cfg-mencoder.h:

#define MSGTR_MEncoderMP3LameHelp "\n\n"\
" vbr=<0-4>     metoda promìnného datového toku\n"\
"                0: cbr\n"\
"                1: mt\n"\
"                2: rh(výchozí)\n"\
"                3: abr\n"\
"                4: mtrh\n"\
"\n"\
" abr           prùmìrný datový tok\n"\
"\n"\
" cbr           konstantní datový tok\n"\
"               Vynutí také metodu CBR pro následné pøednastavené ABR módy\n"\
"\n"\
" br=<0-1024>   urèení datového toku v kBit (pouze CBR a ABR)\n"\
"\n"\
" q=<0-9>       kvalita (0-nejvy¹¹í, 9-nejni¾¹í) (pouze pro VBR)\n"\
"\n"\
" aq=<0-9>      kvalita algoritmu (0-nejlep¹í/nejpomalej¹í, 9-nejhor¹í/nejrychlej¹í)\n"\
"\n"\
" ratio=<1-100> kompresní pomìr\n"\
"\n"\
" vol=<0-10>    zesílení zvuku\n"\
"\n"\
" mode=<0-3>    (výhozí: auto)\n"\
"                0: stereo\n"\
"                1: joint-stereo\n"\
"                2: dualchannel\n"\
"                3: mono\n"\
"\n"\
" padding=<0-2>\n"\
"                0: ne\n"\
"                1: v¹e\n"\
"                2: upravit\n"\
"\n"\
" fast          zapnout rychlej¹í kódování pro následné pøednastavené VBR módy.\n"\
"               O nìco ni¾¹í kvalita a vy¹¹í datový tok.\n"\
"\n"\
" preset=<value> pøednastavené profily poskytující maximání kvalitu.\n"\
"                 medium: kódování metodou VBR, dobrá kvalita\n"\
"                 (datový tok 150-180 kbps)\n"\
"                 standard: kódování metodou VBR, vysoká kvalita\n"\
"                 (datový tok 170-210 kbps)\n"\
"                 extreme: kódování metodou VBR, velmi vysoká kvalita\n"\
"                 (datový tok 200-240 kbps)\n"\
"                 insane: kódování metodou CBR, nejvy¹¹í pøednastavená kvalita\n"\
"                 (datový tok 320 kbps)\n"\
"                 <8-320>: hodnota prùmìrného datového toku pro metodu ABR.\n\n"


// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM zaøízení '%s' nenalezeno.\n"
#define MSGTR_ErrTrackSelect "Chyba pøi výbìru VCD stopy."
#define MSGTR_ReadSTDIN "Ètu ze stdin...\n"
#define MSGTR_UnableOpenURL "Nelze otevøít URL: %s\n"
#define MSGTR_ConnToServer "Pøipojen k serveru: %s\n"
#define MSGTR_FileNotFound "Soubor nenalezen: '%s'\n"

#define MSGTR_SMBInitError "Nemohu inicializovat knihovnu libsmbclient: %d\n"
#define MSGTR_SMBFileNotFound "Nemohu otevøít soubor ze sítì: '%s'\n"
#define MSGTR_SMBNotCompiled "MPlayer nebyl pøelo¾en s podporou SMB\n"

#define MSGTR_CantOpenDVD "Nelze otevøít DVD zaøízení: %s\n"
#define MSGTR_DVDwait "Ètu strukturu disku, prosím èekejte...\n"
#define MSGTR_DVDnumTitles "Poèet titulù na tomto DVD: %d\n"
#define MSGTR_DVDinvalidTitle "©patné èíslo DVD titulu: %d\n"
#define MSGTR_DVDnumChapters "Poèet kapitol na tomto DVD: %d\n"
#define MSGTR_DVDinvalidChapter "©patné èíslo kapitoly DVD: %d\n"
#define MSGTR_DVDnumAngles "Poèet úhlù pohledu na tomto DVD: %d\n"
#define MSGTR_DVDinvalidAngle "©patné èíslo úhlu pohledu DVD: %d\n"
#define MSGTR_DVDnoIFO "Nemohu otevøít soubor IFO pro DVD titul %d.\n"
#define MSGTR_DVDnoVOBs "Nemohu otevøít VOB soubor titulu (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD úspì¹nì otevøeno.\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "UPOZORNÌNÍ: Hlavièka audio proudu %d pøedefinována!\n"
#define MSGTR_VideoStreamRedefined "UPOZORNÌNÍ: Hlavièka video proudu %d pøedefinována!\n"
#define MSGTR_TooManyAudioInBuffer "\nPøíli¹ mnoho audio paketù ve vyrovnávací pamìti: (%d v %d bajtech)\n"
#define MSGTR_TooManyVideoInBuffer "\nPøíli¹ mnoho video paketù ve vyrovnávací pamìti: (%d v %d bajtech)\n"
#define MSGTR_MaybeNI "Mo¾ná pøehráváte neprokládaný proud/soubor nebo kodek selhal?\n"\
		      "V AVI souborech zkuste vynutit neprokládaný re¾im pomocí volby -ni.\n"
#define MSGTR_SwitchToNi "\nDetekován ¹patnì prokládaný AVI soubor - pøepínám do -ni módu...\n"
#define MSGTR_Detected_XXX_FileFormat "Detekován formát souboru %s.\n"
#define MSGTR_DetectedAudiofile "Detekován zvukový soubor.\n"
#define MSGTR_NotSystemStream "Toto není formát MPEG System Stream... (mo¾ná Transport Stream?)\n"
#define MSGTR_InvalidMPEGES "©patný MPEG-ES proud??? Kontaktujte autora, mo¾ná to je chyba :(\n"
#define MSGTR_FormatNotRecognized "======= Bohu¾el, formát tohoto souboru nebyl rozpoznán/není podporován =======\n"\
                                  "==== Pokud je soubor AVI, ASF nebo MPEG proud, kontaktujte prosim autora! ====\n"
#define MSGTR_MissingVideoStream "®ádný video proud nenalezen.\n"
#define MSGTR_MissingAudioStream "®ádný audio proud nenalezen -> nebude zvuk.\n"
#define MSGTR_MissingVideoStreamBug "Chybí video proud!? Kontaktujte autora, mo¾ná to je chyba :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: soubor neobsahuje vybraný audio nebo video proud.\n"

#define MSGTR_NI_Forced "Vynucen"
#define MSGTR_NI_Detected "Detekován"
#define MSGTR_NI_Message "%s NEPROKLÁDANÝ formát souboru AVI.\n"

#define MSGTR_UsingNINI "Pou¾ívám NEPROKLÁDANÝ po¹kozený formát souboru AVI.\n" //tohle taky nìjak opravit
#define MSGTR_CouldntDetFNo "Nemohu urèit poèet snímkù (pro absolutní posun)\n"
#define MSGTR_CantSeekRawAVI "Nelze se posouvat v surových (raw) AVI proudech! (Potøebuji index, zkuste pou¾ít volbu -idx.)\n"
#define MSGTR_CantSeekFile "Nemohu se posouvat v tomto souboru.\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimované hlavièky nejsou (je¹tì) podporovány.\n"
#define MSGTR_MOVvariableFourCC "MOV: UPOZORNÌNÍ: Promìnná FOURCC detekována!?\n"
#define MSGTR_MOVtooManyTrk "MOV: UPOZORNÌNÍ: Pøíli¹ mnoho stop"
#define MSGTR_FoundAudioStream "==> Nalezen audio proud: %d\n"
#define MSGTR_FoundVideoStream "==> Nalezen video proud: %d\n"
#define MSGTR_DetectedTV "Detekována TV! ;-)\n"
#define MSGTR_ErrorOpeningOGGDemuxer "Nemohu otevøít ogg demuxer.\n"
#define MSGTR_ASFSearchingForAudioStream "ASF: Hledám audio proud (id: %d).\n"
#define MSGTR_CannotOpenAudioStream "Nemohu otevøít audio proud: %s\n"
#define MSGTR_CannotOpenSubtitlesStream "Nemohu otevøít proud s titulky: %s\n"
#define MSGTR_OpeningAudioDemuxerFailed "Nepovedlo se otevøít audio demuxer: %s\n"
#define MSGTR_OpeningSubtitlesDemuxerFailed "Nepovedlo se otevøít demuxer pro titulky: %s\n"
#define MSGTR_TVInputNotSeekable "TV vstup neumo¾òuje posun! (\"Posun\" bude pravdìpodobnì pou¾it pro zmìnu kanálù ;)\n"
#define MSGTR_DemuxerInfoAlreadyPresent "Informace o demuxeru %s ji¾ pøítomna!\n"
#define MSGTR_ClipInfo "Informace o klipu:\n"


// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "Nemohu otevøít kodek.\n"
#define MSGTR_CantCloseCodec "Nemohu uzavøít kodek.\n"

#define MSGTR_MissingDLLcodec "CHYBA: Nemohu otevøít po¾adovaný DirectShow kodek %s.\n"
#define MSGTR_ACMiniterror "Nemohu naèíst/inicializovat Win32/ACM AUDIO kodek. (Chybí DLL soubor?)\n"
#define MSGTR_MissingLAVCcodec "Nemohu najít kodek '%s' v libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: KRITICKÁ CHYBA: Konec souboru v prùbìhu vyhledávání hlavièky sekvence.\n"
#define MSGTR_CannotReadMpegSequHdr "KRITICKÁ CHYBA: Nelze pøeèíst hlavièku sekvence.\n"
#define MSGTR_CannotReadMpegSequHdrEx "KRITICKÁ CHYBA: Nelze pøeèíst roz¹íøení hlavièky sekvence.\n"
#define MSGTR_BadMpegSequHdr "MPEG: ©patná hlavièka sekvence.\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: ©patné roz¹íøení hlavièky sekvence.\n"

#define MSGTR_ShMemAllocFail "Nemohu alokovat sdílenou pamì»\n"
#define MSGTR_CantAllocAudioBuf "Nemohu alokovat vyrovnávací pamì» pro výstup zvuku\n"

#define MSGTR_UnknownAudio "Neznámý/chybìjící audio formát -> nebude zvuk.\n"

#define MSGTR_UsingExternalPP "[PP] Pou¾ívám externí filtr pro postprocessing, max q = %d.\n"
#define MSGTR_UsingCodecPP "[PP] Pou¾ívám integrovaný postprocessing kodeku, max q = %d.\n"
#define MSGTR_VideoAttributeNotSupportedByVO_VD "Video atribut '%s' není podporován vybraným vo & vd.\n"
#define MSGTR_VideoCodecFamilyNotAvailableStr "Po¾adovaná rodina video kodeku [%s] (vfm=%s) není dostupná. (Aktivujte ji pøi kompilaci.)\n"
#define MSGTR_AudioCodecFamilyNotAvailableStr "Po¾adovaná rodina audio kodeku [%s] (afm=%s) není dostupná. (Aktivujte ji pøi kompilaci.)\n"
#define MSGTR_OpeningVideoDecoder "Otevírám video dekodér: [%s] %s\n"
#define MSGTR_OpeningAudioDecoder "Otevírám audio dekodér: [%s] %s\n"
#define MSGTR_UninitVideoStr "uninit video: %s\n"
#define MSGTR_UninitAudioStr "uninit audio: %s\n"
#define MSGTR_VDecoderInitFailed "VDecoder - inicializace selhala :(\n"
#define MSGTR_ADecoderInitFailed "ADecoder - inicializace selhala :(\n"
#define MSGTR_ADecoderPreinitFailed "ADecoder - preinit selhal :(\n"
#define MSGTR_AllocatingBytesForInputBuffer "dec_audio: Alokuji %d bytù pro vstupní vyrovnávací pamì»\n"
#define MSGTR_AllocatingBytesForOutputBuffer "dec_audio: Alokuji %d + %d = %d bytù pro výstupní vyrovnávací pamì»\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Zapínám podporu lirc...\n"
#define MSGTR_LIRCdisabled "Nebudete moci pou¾ívat dálkový ovladaè.\n"
#define MSGTR_LIRCopenfailed "Nepovedlo se zapnout podporu LIRC.\n"
#define MSGTR_LIRCcfgerr "Nepovedlo se pøeèíst konfiguraèní soubor LIRC %s.\n"

// vf.c
#define MSGTR_CouldNotFindVideoFilter "Nemohu nalézt video filtr '%s'\n"
#define MSGTR_CouldNotOpenVideoFilter "Nemohu otevøít video filtr '%s'\n"
#define MSGTR_OpeningVideoFilter "Otevírám video filtr: "
#define MSGTR_CannotFindColorspace "Ani pøi vlo¾ení 'scale' nemohu nalézt spoleèný barevný prostor :(\n"

// vd.c
#define MSGTR_CodecDidNotSet "VDec: Kodek nenastavil sh->disp_w a sh->disp_h, pokou¹ím se to obejít.\n"
#define MSGTR_VoConfigRequest "VDec: Po¾adovaná konfigurace vo - %d x %d (preferovaný csp: %s)\n"
#define MSGTR_CouldNotFindColorspace "Nemohu nalézt spoleèný barevný prostor - zkou¹ím to znovu s -vf scale...\n"
#define MSGTR_MovieAspectIsSet "Pomìr stran obrazu filmu je %.2f:1 - ¹káluji na správný pomìr.\n"
#define MSGTR_MovieAspectUndefined "Pomìr stran obrazu filmu není definován - nemìním velikost.\n"

// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O aplikaci"
#define MSGTR_FileSelect "Vybrat soubor..."
#define MSGTR_SubtitleSelect "Vybrat titulky..."
#define MSGTR_OtherSelect "Vybrat..."
#define MSGTR_AudioFileSelect "Vybrat externí zvukový kanál..."
#define MSGTR_PlayList "Soubory pro pøehrání"
#define MSGTR_Equalizer "Ekvalizér"
#define MSGTR_SkinBrowser "Prohlí¾eè témat"
#define MSGTR_Network "Sí»ové vysílání..."
#define MSGTR_Preferences "Nastavení" // Pøedvolby?
#define MSGTR_FontSelect "Vybrat font..."
#define MSGTR_OSSPreferences "Konfigurace ovladaèe OSS"
#define MSGTR_SDLPreferences "Konfigurace ovladaèe SDL"
#define MSGTR_NoMediaOpened "Nic není otevøeno."
#define MSGTR_VCDTrack "VCD stopa %d"
#define MSGTR_NoChapter "¾ádná kapitola" //bez kapitoly?
#define MSGTR_Chapter "Kapitola %d"
#define MSGTR_NoFileLoaded "Není naèten ¾ádný soubor."

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Zru¹it"
#define MSGTR_Add "Pøidat"
#define MSGTR_Remove "Odebrat"
#define MSGTR_Clear "Vynulovat"
#define MSGTR_Config "Konfigurace"
#define MSGTR_ConfigDriver "Konfigurovat ovladaè"
#define MSGTR_Browse "Prohlí¾et"

// --- error messages ---
#define MSGTR_NEMDB "Bohu¾el není dostatek pamìti pro vykreslovací mezipamì»."
#define MSGTR_NEMFMR "Bohu¾el není dostatek pamìti pro vytvoøení menu."
#define MSGTR_IDFGCVD "Bohu¾el nebyl nalezen video ovladaè kompatibilní s GUI."
#define MSGTR_NEEDLAVCFAME "Bohu¾el nelze pøehrávat jiné soubory ne¾ MPEG s kartou DXR3/H+ bez pøekódování.\nProsím, zapnìte lavc nebo fame v konfiguraci DXR3/H+."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[témata] chyba v konfiguraèním souboru témat na øádce %d: %s"
#define MSGTR_SKIN_WARNING1 "[témata] varování v konfiguraèním souboru témat na øádce %d: widget nalezen ale pøed  \"section\" nenalezen (%s)"
#define MSGTR_SKIN_WARNING2 "[témata] varování v konfiguraèním souboru témat na øádce %d: widget nalezen ale pøed \"subsection\" nenalezen (%s)"
#define MSGTR_SKIN_WARNING3 "[témata] varování v konfiguraèním souboru témat na øádce %d: widget (%s) nepodporuje tuto subsekci"
#define MSGTR_SKIN_BITMAP_16bit  "bitmapa s hloubkou 16 bitù a ménì není podporována (%s).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "soubor nenalezen (%s)\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "chyba ètení BMP (%s)\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "chyba ètení TGA (%s)\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "chyba ètení PNG (%s)\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "formát RLE packed TGA není podporován (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "neznámý typ souboru (%s)\n"
#define MSGTR_SKIN_BITMAP_ConvertError "chyba konverze z 24 do 32 bitù (%s)\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "neznámá zpráva: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nedostatek pamìti\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "deklarováno pøíli¹ mnoho písem\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "soubor písma nebyl nalezen\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "soubor obrazu písma nebyl nalezen\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "neexistující identifikátor písma (%s)\n"
#define MSGTR_SKIN_UnknownParameter "neznámý parametr (%s)\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[prohlí¾eè témat] nedostatek pamìti.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Téma nenalezeno (%s).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Chyba pøi ètení konfiguraèního souboru témat (%s).\n"
#define MSGTR_SKIN_LABEL "Témata:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O aplikaci MPlayer"
#define MSGTR_MENU_Open "Otevøít..."
#define MSGTR_MENU_PlayFile "Pøehrát soubor..."
#define MSGTR_MENU_PlayVCD "Pøehrát VCD..."
#define MSGTR_MENU_PlayDVD "Pøehrát DVD..."
#define MSGTR_MENU_PlayURL "Pøehrát z URL..."
#define MSGTR_MENU_LoadSubtitle "Naèíst titulky..."
#define MSGTR_MENU_DropSubtitle "Zahodit titulky..."
#define MSGTR_MENU_LoadExternAudioFile "Naèíst externí soubor se zvukem..."
#define MSGTR_MENU_Playing "Ovládání pøehrávání"
#define MSGTR_MENU_Play "Pøehrát"
#define MSGTR_MENU_Pause "Pozastavit"
#define MSGTR_MENU_Stop "Zastavit"
#define MSGTR_MENU_NextStream "Dal¹í proud"
#define MSGTR_MENU_PrevStream "Pøedchozí proud"
#define MSGTR_MENU_Size "Velikost"
#define MSGTR_MENU_NormalSize "Normální velikost"
#define MSGTR_MENU_DoubleSize "Dvojnásobná velikost"
#define MSGTR_MENU_FullScreen "Celá obrazovka"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Pøehrát disk..."
#define MSGTR_MENU_ShowDVDMenu "Zobrazit DVD menu"
#define MSGTR_MENU_Titles "Tituly"
#define MSGTR_MENU_Title "Titul %2d"
#define MSGTR_MENU_None "(¾ádné)"
#define MSGTR_MENU_Chapters "Kapitoly"
#define MSGTR_MENU_Chapter "Kapitola %2d"
#define MSGTR_MENU_AudioLanguages "Jazyk zvuku"
#define MSGTR_MENU_SubtitleLanguages "Jazyk titulkù"
#define MSGTR_MENU_PlayList "Soubory pro pøehrání"
#define MSGTR_MENU_SkinBrowser "Prohlí¾eè témat"
#define MSGTR_MENU_Preferences "Pøedvolby"
#define MSGTR_MENU_Exit "Konec..."
#define MSGTR_MENU_Mute "Ztlumit"
#define MSGTR_MENU_Original "Pùvodní"
#define MSGTR_MENU_AspectRatio "Pomìr stran"
#define MSGTR_MENU_AudioTrack "Audio stopa"
#define MSGTR_MENU_Track "Stopa %d"
#define MSGTR_MENU_VideoTrack "Video stopa"

// --- equalizer
#define MSGTR_EQU_Audio "Zvuk"
#define MSGTR_EQU_Video "Obraz"
#define MSGTR_EQU_Contrast "Kontrast: "
#define MSGTR_EQU_Brightness "Jas: "
#define MSGTR_EQU_Hue "Odstín: "
#define MSGTR_EQU_Saturation "Sytost: "
#define MSGTR_EQU_Front_Left "Levý pøední"
#define MSGTR_EQU_Front_Right "Pravý pøední"
#define MSGTR_EQU_Back_Left "Levý zadní"
#define MSGTR_EQU_Back_Right "Pravý zadní"
#define MSGTR_EQU_Center "Støedový"
#define MSGTR_EQU_Bass "Basový"
#define MSGTR_EQU_All "V¹e"
#define MSGTR_EQU_Channel1 "Kanál 1:"
#define MSGTR_EQU_Channel2 "Kanál 2:"
#define MSGTR_EQU_Channel3 "Kanál 3:"
#define MSGTR_EQU_Channel4 "Kanál 4:"
#define MSGTR_EQU_Channel5 "Kanál 5:"
#define MSGTR_EQU_Channel6 "Kanál 6:"

// --- playlist
#define MSGTR_PLAYLIST_Path "Cesta"
#define MSGTR_PLAYLIST_Selected "Vybrané soubory"
#define MSGTR_PLAYLIST_Files "Soubory"
#define MSGTR_PLAYLIST_DirectoryTree "Adresáøe"

// --- preferences
#define MSGTR_PREFERENCES_Audio "Zvuk"
#define MSGTR_PREFERENCES_Video "Obraz"
#define MSGTR_PREFERENCES_SubtitleOSD "Titulky & OSD"
#define MSGTR_PREFERENCES_Codecs "Kodeky & demuxer"
#define MSGTR_PREFERENCES_Misc "Ostatní"

#define MSGTR_PREFERENCES_None "Nic"
#define MSGTR_PREFERENCES_AvailableDrivers "Dostupné ovladaèe:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Nepøehrávat zvuk"
#define MSGTR_PREFERENCES_NormalizeSound "Normalizovat zvuk"
#define MSGTR_PREFERENCES_EnEqualizer "Aktivovat ekvalizér"
#define MSGTR_PREFERENCES_ExtraStereo "Aktivovat extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Koeficient:"
#define MSGTR_PREFERENCES_AudioDelay "Zpo¾dìní zvuku"
#define MSGTR_PREFERENCES_DoubleBuffer "Aktivovat double buffering"
#define MSGTR_PREFERENCES_DirectRender "Aktivovat direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Aktivovat zahazování snímkù"
#define MSGTR_PREFERENCES_HFrameDrop "Aktivovat TVRDÉ zahazování snímkù (nebezpeèné)"
#define MSGTR_PREFERENCES_Flip "Obrátit obraz vzhùru nohama"
#define MSGTR_PREFERENCES_Panscan "Panscan:"
#define MSGTR_PREFERENCES_OSDTimer "Èas a ostatní ukazatele"
#define MSGTR_PREFERENCES_OSDProgress "Pouze ukazatel pozice" // progressbar
#define MSGTR_PREFERENCES_OSDTimerPercentageTotalTime "Èas, procenta a celkový èas"
#define MSGTR_PREFERENCES_Subtitle "Titulky:"
#define MSGTR_PREFERENCES_SUB_Delay "Zpo¾dìní: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozice: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Vypnout automatické naètení titulkù"
#define MSGTR_PREFERENCES_SUB_Unicode "Titulky v UNICODE"
#define MSGTR_PREFERENCES_SUB_MPSUB "Pøevést dané titulky do vlastního formátu MPlayeru"
#define MSGTR_PREFERENCES_SUB_SRT "Pøevést dané titulky do èasovì orientovaného formátu SubViewer (SRT)"
#define MSGTR_PREFERENCES_SUB_Overlap "Zapnout pøekrývání titulkù"
#define MSGTR_PREFERENCES_Font "Písmo:"
#define MSGTR_PREFERENCES_FontFactor "Èinitel písma:" //???? asi zvìt¹ení? Kde to vùbec je?
#define MSGTR_PREFERENCES_PostProcess "Aktivovat postprocessing"
#define MSGTR_PREFERENCES_AutoQuality "Automatické øízení kvality:"
#define MSGTR_PREFERENCES_NI "Pou¾ít parser pro neprokládaný AVI formát"
#define MSGTR_PREFERENCES_IDX "Pøetvoøit tabulku indexù, pokud je to tøeba"
#define MSGTR_PREFERENCES_VideoCodecFamily "Rodina video kodeku:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Rodina audio kodeku:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "Typ OSD"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Titulky"
#define MSGTR_PREFERENCES_FRAME_Font "Písmo"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Postprocessing"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Kodek & demuxer"
#define MSGTR_PREFERENCES_FRAME_Cache "Vyrovnávací pamì»"
#define MSGTR_PREFERENCES_FRAME_Misc "Ostatní"
#define MSGTR_PREFERENCES_OSS_Device "Zaøízení:"
#define MSGTR_PREFERENCES_OSS_Mixer "Smì¹ovaè:"
#define MSGTR_PREFERENCES_SDL_Driver "Ovladaè:"
#define MSGTR_PREFERENCES_Message "Pozor, nìkterá nastavení potøebují pro svou funkci restartovat pøehrávání!"
#define MSGTR_PREFERENCES_DXR3_VENC "Video enkodér:"
#define MSGTR_PREFERENCES_DXR3_LAVC "Pou¾ít LAVC (FFmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "Pou¾ít FAME"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Západoevropské jazyky (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Západoevropské jazyky s Eurem (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Slovanské/støedoevropské jazyky (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Esperanto, gal¹tina, maltéz¹tina, tureètina (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Staré Baltské kódování (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cyrilice (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arab¹tina (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Moderní øeètina (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Tureètina (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltické (ISO-8859-13)"
#define MSGTR_PREFERENCES_FontEncoding12 "Kelt¹tina (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Hebrej¹tina (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Ru¹tina (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrajin¹tina, bìloru¹tina (KOI8-U/RU)"
#define MSGTR_PREFERENCES_FontEncoding16 "Jednoduchá èín¹tina (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Tradièní èín¹tina (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japon¹tina (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Korej¹tina (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thaj¹tina (CP874)"
#define MSGTR_PREFERENCES_FontEncoding21 "Cyrilické Windows (CP1251)"
#define MSGTR_PREFERENCES_FontEncoding22 "Slovanské/støedoevropské Windows (CP1250)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Bez automatické velikosti"
#define MSGTR_PREFERENCES_FontPropWidth "Proporènì dle ¹íøky obrazu"
#define MSGTR_PREFERENCES_FontPropHeight "Proporènì dle vý¹ky obrazu"
#define MSGTR_PREFERENCES_FontPropDiagonal "Proporènì dle úhlopøíèky"
#define MSGTR_PREFERENCES_FontEncoding "Kódování:"
#define MSGTR_PREFERENCES_FontBlur "Rozmazání:"
#define MSGTR_PREFERENCES_FontOutLine "Obrys:"
#define MSGTR_PREFERENCES_FontTextScale "Velikost textu:"
#define MSGTR_PREFERENCES_FontOSDScale "Velikost OSD:"
#define MSGTR_PREFERENCES_Cache "Zapnout vyrovnávací pamì»"
#define MSGTR_PREFERENCES_CacheSize "Velikost vyrovnávací pamìti: "
#define MSGTR_PREFERENCES_LoadFullscreen "Spustit pøes celou obrazovku"
#define MSGTR_PREFERENCES_SaveWinPos "Ulo¾it pozici okna"
#define MSGTR_PREFERENCES_XSCREENSAVER "Zastavit XScreenSaver"
#define MSGTR_PREFERENCES_PlayBar "Aktivovat playbar"
#define MSGTR_PREFERENCES_AutoSync "Zapnout automatickou synchronizaci"
#define MSGTR_PREFERENCES_AutoSyncValue "Automatická synchronizace: "
#define MSGTR_PREFERENCES_CDROMDevice "Zaøízení CD-ROM:"
#define MSGTR_PREFERENCES_DVDDevice "Zaøízení DVD:"
#define MSGTR_PREFERENCES_FPS "Snímková rychlost (FPS):"
#define MSGTR_PREFERENCES_ShowVideoWindow "Zobrazovat video okno pøi neèinnosti"

#define MSGTR_ABOUT_UHU "Vývoj GUI je sponzorován firmou UHU Linux\n"
#define MSGTR_ABOUT_CoreTeam "   Hlavní vývojáøi programu MPlayer:\n"
#define MSGTR_ABOUT_AdditionalCoders "   Dal¹í vývojáøi:\n"
#define MSGTR_ABOUT_MainTesters "   Hlavní testeøi:\n"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Kritická chyba!"
#define MSGTR_MSGBOX_LABEL_Error "Chyba!"
#define MSGTR_MSGBOX_LABEL_Warning "Upozornìní!"

#endif
