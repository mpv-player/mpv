// Translated by:  Jiri Svoboda, jiri.svoboda@seznam.cz
// Updated by:     Tomas Blaha,  tomas.blaha at kapsa.club.cz
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (viz DOCS!)\n"
"\n";

// Pøeklad do èe¹tiny Jiøí Svoboda

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Pou¾ití:   mplayer [-gui] [pøepínaèe] [cesta/]jmenosouboru\n"
#else
"Pou¾ití:   mplayer [pøepínaèe] [cesta/]jmenosouboru\n"
#endif
"\n"
"Pøepínaèe:\n"
" -vo <drv[:dev]> výbìr výstupního video ovladaèe&zaøízení (-vo help pro seznam)\n"
" -ao <drv[:dev]> výbìr výstupního audio ovladaèe&zaøízení (-ao help pro seznam)\n"
" -vcd <trackno>  pøehrát VCD (video cd) stopu ze zaøízení místo ze souboru\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  urèení DVD zaøízení pro autentikaci (pro kódované disky)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  pøehrát DVD titul/stopu ze zaøízení (mechaniky) místo souboru\n"
#endif
" -ss <timepos>   posun na pozici (sekundy nebo hh:mm:ss)\n"
" -nosound        pøehrávat beze zvuku\n"
#ifdef USE_FAKE_MONO
" -stereo <mode>  výbìr audiokanálu pro MPEG1 (0:stereo 1:levý 2:pravý)\n"
#endif
" -channels <n>   cílový poèet zvukových výstupních kanálù\n"
" -fs -vm -zoom   volby pro pøehrávání pøes celou obrazovku (celá obrazovka\n                 mìnit videore¾im, softwarový zoom)\n"
" -x <x> -y <y>   zvìt¹ení obrazu na rozmìr <x>*<y> (pokud to umí -vo ovladaè!)\n"
" -sub <file>     volba souboru s titulky (viz také -subfps, -subdelay)\n"
" -playlist <file> urèení souboru se seznamem pøehrávaných souborù\n"
" -vid x -aid y   výbìr èísla video (x) a audio (y) proudu pro pøehrání\n"
" -fps x -srate y volba pro zmìnu video (x fps) a audio (y Hz) frekvence\n"
" -pp <quality>   aktivace postprocesing filtru (0-4 pro DivX, 0-63 pro mpegy)\n"
" -nobps          pou¾ít alternativní A-V synchronizaèní metodu pro Avi soubory\n"
" -framedrop      povolit zahazování snímkù (pro pomale stroje)\n"
" -wid <window id> pou¾ít existující okno pro výstup videa\n"
"\n"
"Klávesy:\n"
" <-  nebo  ->    posun vzad/vpøed o 10 sekund\n"
" nahoru èi dolù  posun vzad/vpøed o  1 minutu\n"
" < nebo >        posun vzad/vpøed v seznamu pøehrávaných souborù\n"
" p nebo mezerník pauza pøi pøehrávání (pokraèování stiskem kterékoliv klávesy)\n"
" q nebo ESC      konec pøehrávání a ukonèení programu\n"
" + nebo -        upravit zpo¾dìní zvuku v krocích +/- 0.1 sekundy\n"
" o               cyklická zmìna re¾imu OSD:  nic / pozice / pozice+èas\n"
" * nebo /        pøidat nebo ubrat hlasitost (stiskem 'm' výbìr master/pcm)\n"
" z nebo x        upravit zpo¾dìní titulkù v krocích +/- 0.1 sekundy\n"
"\n"
" * * * * PØEÈTÌTE SI MAN STRÁNKU PRO DETAILY (DAL©Í VOLBY A KLÁVESY)! * * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nKonèím... (%s)\n"
#define MSGTR_Exit_frames "Po¾adovaný poèet snímkù pøehrán"
#define MSGTR_Exit_quit "Konec"
#define MSGTR_Exit_eof "Konec souboru"
#define MSGTR_Exit_error "Záva¾ná chyba"
#define MSGTR_IntBySignal "\nMPlayer pøeru¹en signálem %d v modulu: %s \n"
#define MSGTR_NoHomeDir "Nemohu nalézt domácí (HOME) adresáø\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problém\n"
#define MSGTR_CreatingCfgFile "Vytváøím konfiguraèní soubor: %s\n"
#define MSGTR_InvalidVOdriver "Neplané jméno výstupního videoovladaèe: %s\nPou¾ijte '-vo help' pro seznam dostupných ovladaèù.\n"
#define MSGTR_InvalidAOdriver "Neplané jméno výstupního audioovladaèe: %s\nPou¾ijte '-ao help' pro seznam dostupných ovladaèù.\n"
#define MSGTR_CopyCodecsConf "(copy/ln etc/codecs.conf (ze zdrojových kódù MPlayeru) do ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Nemohu naèíst font: %s\n"
#define MSGTR_CantLoadSub "Nemohu naèíst titulky: %s\n"
#define MSGTR_ErrorDVDkey "Chyba pøi zpracování klíèe DVD.\n"
#define MSGTR_CmdlineDVDkey "DVD klíè po¾adovaný na pøíkazové øádce je uschován pro rozkódování.\n"
#define MSGTR_DVDauthOk "DVD autentikaèní sekvence vypadá vpoøádku.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: po¾adovaný proud chybí!\n"
#define MSGTR_CantOpenDumpfile "Nelze otevøít soubor pro dump!!!\n"
#define MSGTR_CoreDumped "jádro vypsáno :)\n"
#define MSGTR_FPSnotspecified "V hlavièce souboru není udáno (nebo je ¹patné) FPS! Pou¾ijte volbu -fps !\n"
#define MSGTR_NoVideoStream "Bohu¾el, ¾ádný videoproud... to se zatím nedá pøehrát.\n"
#define MSGTR_TryForceAudioFmt "Pokou¹ím se vynutit rodinu audiokodeku %d ...\n"
#define MSGTR_CantFindAfmtFallback "Nemohu nalézt audio kodek pro po¾adovanou rodinu, pou¾iji ostatní.\n"
#define MSGTR_CantFindAudioCodec "Nemohu nalézt kodek pro audio formát 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Pokuste se upgradovat %s z etc/codecs.conf\n*** Pokud problém pøetrvá, pak si pøeètìte DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Nelze inicializovat audio kodek! -> beze zvuku\n"
#define MSGTR_TryForceVideoFmt "Poku¹ím se vynutit rodinu videokodeku %d ...\n"
#define MSGTR_CantFindVfmtFallback "Nemohu nalézt video kodek pro po¾adovanou rodinu, pou¾iji ostatní.\n"
#define MSGTR_CantFindVideoCodec "Nemohu nalézt kodek pro video formát 0x%X !\n"
#define MSGTR_VOincompCodec "Bohu¾el, vybrané video_out zaøízení je nekompatibilní s tímto kodekem.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Nemohu inicializovat videokodek :(\n"
#define MSGTR_EncodeFileExists "Soubor ji¾ existuje: %s (nepøepi¹te si svùj oblíbený AVI soubor!)\n"
#define MSGTR_CantCreateEncodeFile "Nemohu vytvoøit soubor\n" // toto doopravit - need to be corrected
#define MSGTR_CannotInitVO "FATAL: Nemohu inicializovat video driver!\n"
#define MSGTR_CannotInitAO "nemohu otevøít/inicializovat audio driver -> TICHO\n"
#define MSGTR_StartPlaying "Zaèínám pøehrávat...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***********************************************************\n"\
"         ****  Na pøehrání tohoto je vá¹ systém pøíli¹ POMALÝ!  ****\n"\
"         ***********************************************************\n"\
"!!! Mo¾né pøíèiny, problémy a øe¹ení:\n"\
"- Nejèastìj¹í: ¹patný/chybný _zvukový_ ovladaè. Øe¹ení: zkuste -ao sdl nebo pou¾ijte\n"\
"  ALSA 0.5 nebo oss emulaci z ALSA 0.9. více tipù se dozvíte v DOCS/sound.html!\n"\
"- Pomalý video výstup. Zkuste jiný -vo ovladaè (pro seznam: -vo help) nebo zkuste\n"\
"  s volbou -framedrop !  Tipy pro ladìní/zrychlení videa jsou v DOCS/video.html\n"\
"- Pomalá cpu. Nezkou¹ejte pøehrávat velké dvd/divx na pomalé cpu! Zkuste -hardframedrop\n"\
"- Po¹kozený soubor. Zkuste rùzné kombinace tìchto voleb: -nobps  -ni  -mc 0  -forceidx\n"\
"Pokud nic z toho není pravda, pøeètìte si DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer byl pøelo¾en BEZ podpory GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI vy¾aduje X11!\n"
#define MSGTR_Playing "Pøehrávám %s\n"
#define MSGTR_NoSound "Audio: beze zvuku!!!\n"
#define MSGTR_FPSforced "FPS vynuceno na hodnotu %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM zaøízení '%s' nenalezeno!\n"
#define MSGTR_ErrTrackSelect "Chyba pøi výbìru VCD stopy!"
#define MSGTR_ReadSTDIN "Ètu ze stdin...\n"
#define MSGTR_UnableOpenURL "Nelze otevøít URL: %s\n"
#define MSGTR_ConnToServer "Pøipojen k serveru: %s\n"
#define MSGTR_FileNotFound "Soubor nenalezen: '%s'\n"

#define MSGTR_CantOpenDVD "Nelze otevøít DVD zaøízení: %s\n"
#define MSGTR_DVDwait "Ètu strukturu disku, prosím èekejte...\n"
#define MSGTR_DVDnumTitles "Na tomto DVD je %d titulù.\n"
#define MSGTR_DVDinvalidTitle "Neplatné èíslo DVD titulu: %d\n"
#define MSGTR_DVDnumChapters "Na tomto DVD je %d kapitol.\n"
#define MSGTR_DVDinvalidChapter "Neplatné èíslo kapitoly DVD: %d\n"
#define MSGTR_DVDnumAngles "Na tomto DVD je %d úhlù pohledu.\n"
#define MSGTR_DVDinvalidAngle "Neplatné èíslo úhlu pohledu DVD: %d\n"
#define MSGTR_DVDnoIFO "Nemohu otevøít soubor IFO pro DVD titul %d.\n"
#define MSGTR_DVDnoVOBs "Nemohu otevøít VOB soubor (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD úspì¹nì otevøeno!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Upozornìní! Hlavièka audio proudu %d pøedefinována!\n"
#define MSGTR_VideoStreamRedefined "Upozornìní! Hlavièka video proudu %d pøedefinována!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Pøíli¹ mnoho (%d v %d bajtech) audio paketù v bufferu!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Pøíli¹ mnoho (%d v %d bajtech) video paketù v bufferu!\n"
#define MSGTR_MaybeNI "(mo¾ná pøehráváte neprokládaný proud/soubor nebo kodek selhal)\n"
#define MSGTR_DetectedFILMfile "Detekován FILM formát souboru!\n"
#define MSGTR_DetectedFLIfile "Detekován FLI formát souboru!\n"
#define MSGTR_DetectedROQfile "Detekován RoQ formát souboru!\n"
#define MSGTR_DetectedREALfile "Detekován REAL formát souboru!\n"
#define MSGTR_DetectedAVIfile "Detekován AVI formát souboru!\n"
#define MSGTR_DetectedASFfile "Detekován ASF formát souboru!\n"
#define MSGTR_DetectedMPEGPESfile "Detekován MPEG-PES formát souboru!\n"
#define MSGTR_DetectedMPEGPSfile "Detekován MPEG-PS formát souboru!\n"
#define MSGTR_DetectedMPEGESfile "Detekován MPEG-ES formát souboru!\n"
#define MSGTR_DetectedQTMOVfile "Detekován QuickTime/MOV formát souboru!\n"
#define MSGTR_MissingMpegVideo "Chybìjící MPEG video proud!? Kontaktujte autora, mo¾ná to je chyba (bug) :(\n"
#define MSGTR_InvalidMPEGES "Neplatný MPEG-ES proud!? Kontaktuje autora, mo¾ná to je chyba (bug) :(\n"
#define MSGTR_FormatNotRecognized "========== Bohu¾el, tento formát souboru není rozpoznán/podporován =========\n"\
                                 "==== Pokud je tento soubor AVI, ASF nebo MPEG proud, kontaktuje autora! ====\n"
#define MSGTR_MissingVideoStream "®ádný video proud nenalezen!\n"
#define MSGTR_MissingAudioStream "®ádný audio proud nenalezen...  ->beze zvuku\n"
#define MSGTR_MissingVideoStreamBug "Chybìjící video proud!? Kontaktuje autora, mo¾ná to je chyba (bug) :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: soubor neobsahuje vybraný audio nebo video proud\n"

#define MSGTR_NI_Forced "Vynucen"
#define MSGTR_NI_Detected "Detekován"
#define MSGTR_NI_Message "%s NEPROKLÁDANÝ formát souboru AVI!\n"

#define MSGTR_UsingNINI "Pou¾ívám NEPROKLÁDANÝ po¹kozený formát souboru AVI!\n" //tohle taky nìjak opravit
#define MSGTR_CouldntDetFNo "Nemohu urèit poèet snímkù (pro absolutní posun)  \n"
#define MSGTR_CantSeekRawAVI "Nelze se posouvat v surových (raw) .AVI proudech! (Potøebuji index, zkuste pou¾ít volbu -idx !)  \n"
#define MSGTR_CantSeekFile "Nemohu posouvat v tomto souboru!  \n"

#define MSGTR_EncryptedVOB "Kódovaný VOB soubor (pøelo¾eno bez podpory libcss)! Pøeètìte si DOCS/DVD\n"
#define MSGTR_EncryptedVOBauth "Zakódovaný proud, ale autentikaci jste nepo¾adoval!!\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimované hlavièky nejsou (je¹tì) podporovány!\n"
#define MSGTR_MOVvariableFourCC "MOV: Upozornìní! promìnná FOURCC detekována!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Upozornìní! Pøíli¹ mnoho stop!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV formát není je¹tì podporován !!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nemohu otevøít kodek\n"
#define MSGTR_CantCloseCodec "nemohu uzavøít kodek\n"

#define MSGTR_MissingDLLcodec "CHYBA: Nemohu otevøít potøebný DirectShow kodek: %s\n"
#define MSGTR_ACMiniterror "Nemohu naèíst/inicializovat Win32/ACM AUDIO kodek (chybìjící soubor DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nemohu najít kodek '%s' v libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayer byl pøelo¾en BEZ podpory directshow!\n"
#define MSGTR_NoWfvSupport "Podpora pro kodeky win32 neaktivní nebo nedostupná mimo platformy x86!\n"
#define MSGTR_NoDivx4Support "MPlayer byl pøelo¾en BEZ podpory DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayer byl pøelo¾en BEZ podpory ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Win32/ACM audio kodek neaktivní nebo nedostupný mimo platformy x86 -> vynuceno beze zvuku :(\n"
#define MSGTR_NoDShowAudio "Pøelo¾eno BEZ podpory DirectShow -> vynuceno beze zvuku :(\n"
#define MSGTR_NoOggVorbis "OggVorbis audio kodek neaktivní -> vynuceno beze zvuku :(\n"
#define MSGTR_NoXAnimSupport "MPlayer byl pøelo¾en BEZ podpory XAnim!\n"

#define MSGTR_MpegPPhint "Upozornìní! Po¾adujete video postprocesing pro MPEG 1/2, ale MPlayer byl\n" \
			 "         pøelo¾en bez podpory posprocesingu MPEG 1/2!\n" \
			 "         #define MPEG12_POSTPROC v config.h a pøelo¾te znovu libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF - konec souboru v prùbìhu vyhledávání hlavièky sekvence\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Nelze pøeèíst hlavièku sekvence!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Nelze pøeèíst roz¹íøení hlavièky sekvence!\n"
#define MSGTR_BadMpegSequHdr "MPEG: ©patná hlavièka sekvence!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: ©patné roz¹íøení hlavièky sekvence!\n"

#define MSGTR_ShMemAllocFail "Nemohu alokovat sdílenou pamì»\n"
#define MSGTR_CantAllocAudioBuf "Nemohu alokovat pamì» pro výstupní audio buffer\n"
#define MSGTR_NoMemForDecodedImage "nedostatek pamìti pro buffer pro dekódování obrazu (%ld bytes)\n"

#define MSGTR_AC3notvalid "Neplatný AC3 proud.\n"
#define MSGTR_AC3only48k "Pouze proudy o frekvenci 48000 Hz podporovány.\n"
#define MSGTR_UnknownAudio "Neznámý/chybìjící audio formát -> beze zvuku\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Nastavuji podporu lirc ...\n"
#define MSGTR_LIRCdisabled "Nebudete moci pou¾ívat dálkový ovladaè.\n"
#define MSGTR_LIRCopenfailed "Selhal pokus o otevøení podpory LIRC!\n"
#define MSGTR_LIRCsocketerr "Nìjaká chyba se soketem lirc: %s\n"
#define MSGTR_LIRCcfgerr "Selhalo ètení konfiguraèního souboru LIRC %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O aplikaci"
#define MSGTR_FileSelect "Výbìr souboru ..."
#define MSGTR_SubtitleSelect "Vybrat titulky ..."
#define MSGTR_MessageBox "Zpráva"
#define MSGTR_PlayList "Soubory pro pøehrání"
#define MSGTR_SkinBrowser "Prohlí¾eè témat"
#define MSGTR_OtherSelect "Vybrat ..."

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Zru¹it"
#define MSGTR_Add "Pøidat"
#define MSGTR_Remove "Odebrat"

// --- error messages ---
#define MSGTR_NEMDB "Bohu¾el, nedostatek pamìti pro buffer pro kreslení."
#define MSGTR_NEMFMR "Bohu¾el, nedostatek pamìti pro vytváøení menu."
#define MSGTR_NEMFMM "Bohu¾el, nedostatek pamìti pro masku hlavního okna."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[témata] chyba v konfiguraèním soubory témat %d: %s"
#define MSGTR_SKIN_WARNING1 "[témata] v konfiguraèním soubory témat na øádce %d: widget nalezen ale pøed  \"section\" nenalezen ( %s )"
#define MSGTR_SKIN_WARNING2 "[témata] v konfiguraèním soubory témat na øádce %d: widget nalezen ale pøed \"subsection\" nenalezen (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "bitmapa s hloubkou 16 bitová a ménì nepodporována ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "soubor nenalezen ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "chyba ètení bmp ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "chyba ètení tga ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "chyba ètení png ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "formát RLE packed tga nepodporován ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "neznámý typ souboru ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "chyba konverze z 24 bit do 32 bit ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "neznámá zpráva: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nedostatek pamìti\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "deklarováno pøíli¹ mnoho fontù\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "soubor fontu nenalezen\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "soubor obrazù fontu nenalezen\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "neexistující identifikátor fontu ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "neznámý parametr ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[prohlí¾eè témat] nedostatek pamìti.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin nenalezen ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Chyba pøi ètení konfiguraèního souboru témat ( %s ).\n"
#define MSGTR_SKIN_LABEL "Témata:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O aplikaci MPlayer"
#define MSGTR_MENU_Open "Otevøít ..."
#define MSGTR_MENU_PlayFile "Pøehrát soubor ..."
#define MSGTR_MENU_PlayVCD "Pøehrát VCD ..."
#define MSGTR_MENU_PlayDVD "Pøehrát DVD ..."
#define MSGTR_MENU_PlayURL "Ètení URL ..."
#define MSGTR_MENU_LoadSubtitle "Naèíst titulky ..."
#define MSGTR_MENU_Playing "Ovládání pøehrávání"
#define MSGTR_MENU_Play "Pøehrát"
#define MSGTR_MENU_Pause "Pauza"
#define MSGTR_MENU_Stop "Zastavit"
#define MSGTR_MENU_NextStream "Dal¹í proud"
#define MSGTR_MENU_PrevStream "Pøedchozí proud"
#define MSGTR_MENU_Size "Velikost"
#define MSGTR_MENU_NormalSize "Normální velikost"
#define MSGTR_MENU_DoubleSize "Dvojnásobná velikost"
#define MSGTR_MENU_FullScreen "Celá obrazovka"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Pøehrát disk ..."
#define MSGTR_MENU_ShowDVDMenu "Zobrazit DVD menu"
#define MSGTR_MENU_Titles "Tituly"
#define MSGTR_MENU_Title "Titul %2d"
#define MSGTR_MENU_None "(nic)"
#define MSGTR_MENU_Chapters "Kapitoly"
#define MSGTR_MENU_Chapter "Kapitola %2d"
#define MSGTR_MENU_AudioLanguages "Jazyk zvuku"
#define MSGTR_MENU_SubtitleLanguages "Jazyk titulkù"
#define MSGTR_MENU_PlayList "Soubory pro pøehrání"
#define MSGTR_MENU_SkinBrowser "Prohli¾eè témat"
#define MSGTR_MENU_Preferences "Pøedvolby"
#define MSGTR_MENU_Exit "Konec ..."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "Fatální chyba ..."
#define MSGTR_MSGBOX_LABEL_Error "Chyba ..."
#define MSGTR_MSGBOX_LABEL_Warning "Upozornìní ..."

#endif

