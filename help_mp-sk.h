// Translated by:  Daniel Beòa, benad@centrum.cz
// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (viï DOCS!)\n"
"\n";

// Preklad do slovenèiny 

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Pou¾itie:   mplayer [-gui] [prepínaèe] [cesta/]menosúboru\n"
#else
"Pou¾itie:   mplayer [prepínaèe] [cesta/]menosúboru\n"
#endif
"\n"
"Prepínaèe:\n"
" -vo <drv[:dev]> výber výstup. video ovládaèa&zariadenia (-vo help pre zoznam)\n"
" -ao <drv[:dev]> výber výstup. audio ovládaèa&zariadenia (-ao help pre zoznam)\n"
" -vcd <trackno>  prehra» VCD (video cd) stopu zo zariadenia namiesto zo súboru\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  urèenie DVD zariadenia pre overenie autenticity (pre kódované disky)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  prehra» DVD titul/stopu zo zariadenia (mechaniky) namiesto súboru\n"
#endif
" -ss <timepos>   posun na pozíciu (sekundy alebo hh:mm:ss)\n"
" -nosound        prehráva» bez zvuku\n"
#ifdef USE_FAKE_MONO
" -stereo <mode>  výber audiokanálu pre MPEG1 (0:stereo 1:µavý 2:pravý)\n"
#endif
" -channels <n>   poèet výstupných zvukových kanálov\n"
" -fs -vm -zoom   voµby pre prehrávanie na celú obrazovku (celá obrazovka\n                 meni» videore¾im, softvérový zoom)\n"
" -x <x> -y <y>   zvet¹enie obrazu na rozmer <x>*<y> (pokiaµ to vie -vo ovládaè!)\n"
" -sub <file>     voµba súboru s titulkami (viï tie¾ -subfps, -subdelay)\n"
" -playlist <file> urèenie súboru so zoznamom prehrávaných súborov\n"
" -vid x -aid y   výber èísla video (x) a audio (y) prúdu pre prehrávanie\n"
" -fps x -srate y voµba pre zmenu video (x fps) a audio (y Hz) frekvencie\n"
" -pp <quality>   aktivácia postprocesing filtra (0-4 pre DivX, 0-63 pre mpegy)\n"
" -nobps          pou¾i» alternatívnu A-V synchronizaènú metódu pre Avi súbory\n"
" -framedrop      povoli» zahadzovanie snímkov (pre pomalé stroje)\n"
" -wid <window id> pou¾i» existujúce okno pre výstup videa\n"
"\n"
"Klávesy:\n"
" <-  alebo  ->   posun vzad/vpred o 10 sekund\n"
" hore / dole     posun vzad/vpred o  1 minútu\n"
" < alebo >       posun vzad/vpred v zozname prehrávaných súborov\n"
" p al. medzerník pauza pri prehrávaní (pokraèovaní stlaèením niektorej klávesy)\n"
" q alebo ESC     koniec prehrávania a ukonèenie programu\n"
" + alebo -       upravi» spozdenie zvuku v krokoch +/- 0.1 sekundy\n"
" o               cyklická zmena re¾imu OSD:  niè / pozícia / pozícia+èas\n"
" * alebo /       prida» alebo ubra» hlasitos» (stlaèením 'm' výber master/pcm)\n"
" z alebo x       upravi» spozdenie titulkov v krokoch +/- 0.1 sekundy\n"
"\n"
" * * * * PREÈÍTAJTE SI MAN STRÁNKU PRE DETAILY (ÏAL©IE VO¥BY A KLÁVESY)! * * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================
// mplayer.c:

#define MSGTR_Exiting "\nKonèím... (%s)\n"
#define MSGTR_Exit_frames "Po¾adovaný poèet snímkov prehraný"
#define MSGTR_Exit_quit "Koniec"
#define MSGTR_Exit_eof "Koniec súboru"
#define MSGTR_Exit_error "Záva¾ná chyba"
#define MSGTR_IntBySignal "\nMPlayer preru¹ený signálom %d v module: %s \n"
#define MSGTR_NoHomeDir "Nemô¾em najs» domáci (HOME) adresár\n"
#define MSGTR_GetpathProblem "get_path(\"config\") problém\n"
#define MSGTR_CreatingCfgFile "Vytváram konfiguraèný súbor: %s\n"
#define MSGTR_InvalidVOdriver "Neplatné meno výstupného videoovládaèa: %s\nPou¾ite '-vo help' pre zoznam dostupných ovládaèov.\n"
#define MSGTR_InvalidAOdriver "Neplatné meno výstupného audioovládaèa: %s\nPou¾ite '-ao help' pre zoznam dostupných ovládaèov.\n"
#define MSGTR_CopyCodecsConf "(copy/ln etc/codecs.conf (zo zdrojových kódov MPlayeru) do ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Nemô¾em naèíta» font: %s\n"
#define MSGTR_CantLoadSub "Nemô¾em naèíta» titulky: %s\n"
#define MSGTR_ErrorDVDkey "Chyba pri spracovaní kµúèa DVD.\n"
#define MSGTR_CmdlineDVDkey "DVD kµúè po¾adovaný na príkazovom riadku je uschovaný pre rozkódovanie.\n"
#define MSGTR_DVDauthOk "DVD sekvencia overenia autenticity vypadá v poriadku.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: po¾adovaný prúd chýba!\n"
#define MSGTR_CantOpenDumpfile "Nejde otvori» súbor pre dump!!!\n"
#define MSGTR_CoreDumped "jadro vypísané :)\n"
#define MSGTR_FPSnotspecified "V hlavièke súboru nie je udané (alebo je zlé) FPS! Pou¾ite voµbu -fps !\n"
#define MSGTR_NoVideoStream "®iaµ, ¾iadny videoprúd... to sa zatiaµ nedá prehra».\n"
#define MSGTR_TryForceAudioFmt "Pokú¹am sa vynúti» rodinu audiokodeku %d ...\n"
#define MSGTR_CantFindAfmtFallback "Nemô¾em nájs» audio kodek pre po¾adovanú rodinu, pou¾ijem ostatné.\n"
#define MSGTR_CantFindAudioCodec "Nemô¾em nájs» kodek pre audio formát 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Pokúste sa upgradova» %s z etc/codecs.conf\n*** Pokiaµ problém pretrvá, preèítajte si DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Nejde inicializova» audio kodek! -> bez zvuku\n"
#define MSGTR_TryForceVideoFmt "Pokú¹am se vnúti» rodinu videokodeku %d ...\n"
#define MSGTR_CantFindVfmtFallback "Nemô¾em najs» video kodek pre po¾adovanú rodinu, pou¾ijem ostatné.\n"
#define MSGTR_CantFindVideoCodec "Nemô¾em najs» kodek pre video formát 0x%X !\n"
#define MSGTR_VOincompCodec "®iaµ, vybrané video_out zariadenie je nekompatibilné s týmto kodekom.\n"
#define MSGTR_CouldntInitVideoCodec "FATAL: Nemô¾em inicializova» videokodek :(\n"
#define MSGTR_EncodeFileExists "Súbor u¾ existuje: %s (neprepí¹te si svoj obµúbený AVI súbor!)\n"
#define MSGTR_CantCreateEncodeFile "Nemô¾em vytvori» súbor pre encoding\n" 
#define MSGTR_CannotInitVO "FATAL: Nemô¾em inicializova» video driver!\n"
#define MSGTR_CannotInitAO "nemô¾em otvori»/inicializova» audio driver -> TICHO\n"
#define MSGTR_StartPlaying "Zaèínam prehráva»...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***********************************************************\n"\
"         ****  Na prehratie tohoto je vá¹ systém príli¹ POMALÝ!  ****\n"\
"         ***********************************************************\n"\
"!!! Mo¾né príèiny, problémy a rie¹enia:\n"\
"- Nejèastej¹ie: nesprávny/chybný _zvukový_ ovládaè. Rie¹enie: skúste -ao sdl al. pou¾ite\n"\
"  ALSA 0.5 nebo oss emuláciu z ALSA 0.9. viac tipov sa dozviete v DOCS/sound.html!\n"\
"- Pomalý video výstup. Skúste iný -vo ovládaè (pre zoznam: -vo help) alebo skúste\n"\
"  s voµbou -framedrop !  Tipy pre ladenie/zrýchlenie videa sú v DOCS/video.html\n"\
"- Pomalý cpu. Neskú¹ajte prehráva» veµké dvd/divx na pomalom cpu! Skúste -hardframedrop\n"\
"- Po¹kodený súbor. Skúste rôzne kombinácie týchto volieb: -nobps  -ni  -mc 0  -forceidx\n"\
"Pokiaµ niè z toho nie je pravda, preèítajte si DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer bol prelo¾ený BEZ podpory GUI!\n"
#define MSGTR_GuiNeedsX "MPlayer GUI vy¾aduje X11!\n"
#define MSGTR_Playing "Prehrávam %s\n"
#define MSGTR_NoSound "Audio: bez zvuku!!!\n"
#define MSGTR_FPSforced "FPS vnútené na hodnotu %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "CD-ROM zariadenie '%s' nenájdené!\n"
#define MSGTR_ErrTrackSelect "Chyba pri výbere VCD stopy!"
#define MSGTR_ReadSTDIN "Èítam z stdin...\n"
#define MSGTR_UnableOpenURL "Nejde otvori» URL: %s\n"
#define MSGTR_ConnToServer "Pripojený k servru: %s\n"
#define MSGTR_FileNotFound "Súbor nenájdený: '%s'\n"

#define MSGTR_CantOpenDVD "Nejde otvori» DVD zariadenie: %s\n"
#define MSGTR_DVDwait "Èítam ¹truktúru disku, prosím èakajte...\n"
#define MSGTR_DVDnumTitles "Na tomto DVD je %d titulov.\n"
#define MSGTR_DVDinvalidTitle "Neplatné èíslo DVD titulu: %d\n"
#define MSGTR_DVDnumChapters "Na tomto DVD je %d kapitol.\n"
#define MSGTR_DVDinvalidChapter "Neplatné èíslo kapitoly DVD: %d\n"
#define MSGTR_DVDnumAngles "Na tomto DVD je %d úhlov pohµadov.\n"
#define MSGTR_DVDinvalidAngle "Neplatné èíslo uhlu pohµadu DVD: %d\n"
#define MSGTR_DVDnoIFO "Nemô¾em otvori» súbor IFO pre DVD titul %d.\n"
#define MSGTR_DVDnoVOBs "Nemô¾em otvori» VOB súbor (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD úspe¹ne otvorené!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Upozornenie! Hlavièka audio prúdu %d predefinovaná!\n"
#define MSGTR_VideoStreamRedefined "Upozornenie! Hlavièka video prúdu %d predefinovaná!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Príli¹ mnoho (%d v %d bajtech) audio paketov v bufferi!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Príli¹ mnoho (%d v %d bajtech) video paketov v bufferi!\n"
#define MSGTR_MaybeNI "(mo¾no prehrávate neprekladaný prúd/súbor alebo kodek zlyhal)\n"
#define MSGTR_DetectedFILMfile "Detekovaný FILM formát súboru!\n"
#define MSGTR_DetectedFLIfile "Detekovaný FLI formát súboru!\n"
#define MSGTR_DetectedROQfile "Detekovaný ROQ formát súboru!\n"
#define MSGTR_DetectedREALfile "Detekovaný REAL formát súboru!\n"
#define MSGTR_DetectedAVIfile "Detekovaný AVI formát súboru!\n"
#define MSGTR_DetectedASFfile "Detekovaný ASF formát súboru!\n"
#define MSGTR_DetectedMPEGPESfile "Detekovaný MPEG-PES formát súboru!\n"
#define MSGTR_DetectedMPEGPSfile "Detekovaný MPEG-PS formát súboru!\n"
#define MSGTR_DetectedMPEGESfile "Detekovaný MPEG-ES formát súboru!\n"
#define MSGTR_DetectedQTMOVfile "Detekovaný QuickTime/MOV formát súboru!\n"
#define MSGTR_MissingMpegVideo "Chýbajúci MPEG video prúd!? kontaktujte autora, mo¾no je to chyba (bug) :(\n"
#define MSGTR_InvalidMPEGES "Neplatný MPEG-ES prúd??? kontaktujte autora, mo¾no je to chyba (bug) :(\n"
#define MSGTR_FormatNotRecognized "========== ®iaµ, tento formát súboru nie je rozpoznaný/podporovaný =======\n"\
				  "==== Pokiaµ je tento súbor AVI, ASF alebo MPEG prúd, kontaktujte autora! ====\n"
#define MSGTR_MissingVideoStream "®iadny video prúd nenájdený!\n"
#define MSGTR_MissingAudioStream "®iadny audio prúd nenájdený...  -> bez zvuku\n"
#define MSGTR_MissingVideoStreamBug "Chýbajúci video prúd!? Kontaktujte autora, mo¾no to je chyba (bug) :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: súbor neobsahuje vybraný audio alebo video prúd\n"

#define MSGTR_NI_Forced "Vnútený"
#define MSGTR_NI_Detected "Detekovaný"
#define MSGTR_NI_Message "%s NEPREKLADANÝ formát súboru AVI!\n"

#define MSGTR_UsingNINI "Pou¾ívam NEPREKLADANÝ po¹kodený formát súboru AVI!\n" 
#define MSGTR_CouldntDetFNo "Nemô¾em urèi» poèet snímkov (pre absolútny posun)  \n"
#define MSGTR_CantSeekRawAVI "Nemô¾em sa posúva» v surových (raw) .AVI prúdoch! (Potrebujem index, zkuste pou¾í» voµbu -idx !)  \n"
#define MSGTR_CantSeekFile "Nemô¾em sa posúva» v tomto súbore!  \n"

#define MSGTR_EncryptedVOB "Kódovaný VOB súbor (prelo¾ené bez podpory libcss)! Preèítajte si DOCS/DVD\n"
#define MSGTR_EncryptedVOBauth "Zakódovaný prúd, ale overenie autenticity ste nepo¾adovali!!\n"

#define MSGTR_MOVcomprhdr "MOV: Komprimované hlavièky nie sú (e¹te) podporované!\n"
#define MSGTR_MOVvariableFourCC "MOV: Upozornenie! premenná FOURCC detekovaná!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Upozornenie! Príli¹ veµa stôp!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV formát nie je e¹te podporovaný !!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nemô¾em otvori» kodek\n"
#define MSGTR_CantCloseCodec "nemô¾em uzavie» kodek\n"

#define MSGTR_MissingDLLcodec "CHYBA: Nemô¾em otvori» potrebný DirectShow kodek: %s\n"
#define MSGTR_ACMiniterror "Nemô¾em naèíta»/inicializova» Win32/ACM AUDIO kodek (chýbajúci súbor DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nemô¾em najs» kodek '%s' v libavcodec...\n"

#define MSGTR_NoDShowSupport "MPlayer bol prelo¾ený BEZ podpory directshow!\n"
#define MSGTR_NoWfvSupport "Podpora pre kodeky win32 neaktívna alebo nedostupná mimo platformy x86!\n"
#define MSGTR_NoDivx4Support "MPlayer bol prelo¾ený BEZ podpory DivX4Linux (libdivxdecore.so)!\n"
#define MSGTR_NoLAVCsupport "MPlayer bol prelo¾ený BEZ podpory ffmpeg/libavcodec!\n"
#define MSGTR_NoACMSupport "Win32/ACM audio kodek neaktívny nebo nedostupný mimo platformy x86 -> bez zvuku :(\n"
#define MSGTR_NoDShowAudio "Prelo¾ené BEZ podpory DirectShow -> bez zvuku :(\n"
#define MSGTR_NoOggVorbis "OggVorbis audio kodek neaktívny -> bez zvuku :(\n"
#define MSGTR_NoXAnimSupport "MPlayer bol prelo¾ený BEZ podpory XAnim!\n"

#define MSGTR_MpegPPhint "Upozornenie! Po¾adujete video postprocesing pre MPEG 1/2, ale MPlayer bol\n" \
			 "         prelo¾ený bez podpory postprocesingu MPEG 1/2!\n" \
			 "         #define MPEG12_POSTPROC v config.h a prelo¾te znovu libmpeg2!\n"
#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF - koniec súboru v priebehu vyhµadávania hlavièky sekvencie\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Nemô¾em preèíta» hlavièku sekvencie!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Nemô¾em preèíta» roz¹írenie hlavièky sekvencie!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Zlá hlavièka sekvencie!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Zlé roz¹írenie hlavièky sekvencie!\n"

#define MSGTR_ShMemAllocFail "Nemô¾em alokova» zdieµanú pamä»\n"
#define MSGTR_CantAllocAudioBuf "Nemô¾em alokova» pamä» pre výstupný audio buffer\n"
#define MSGTR_NoMemForDecodedImage "nedostatok pamäte pre buffer na dekódovanie obrazu (%ld bytes)\n"

#define MSGTR_AC3notvalid "Neplatný AC3 prúd.\n"
#define MSGTR_AC3only48k "Iba prúdy o frekvencii 48000 Hz sú podporované.\n"
#define MSGTR_UnknownAudio "Neznámy/chýbajúci audio formát -> bez zvuku\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Nastavujem podporu lirc ...\n"
#define MSGTR_LIRCdisabled "Nebudete môc» pou¾íva» diaµkový ovládaè.\n"
#define MSGTR_LIRCopenfailed "Zlyhal pokus o otvorenie podpory LIRC!\n"
#define MSGTR_LIRCsocketerr "Nejaká chyba so soketom lirc: %s\n"
#define MSGTR_LIRCcfgerr "Zlyhalo èítanie konfiguraèného súboru LIRC %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "O aplikácii"
#define MSGTR_FileSelect "Vybra» súbor ..."
#define MSGTR_SubtitleSelect "Vybra» titulky ..."
#define MSGTR_MessageBox "MessageBox"
#define MSGTR_PlayList "PlayList"
#define MSGTR_SkinBrowser "Prehliadaè tém"
#define MSGTR_OtherSelect "Vybra» ..."

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Zru¹i»"
#define MSGTR_Add "Prida»"
#define MSGTR_Remove "Odobra»"

// --- error messages ---
#define MSGTR_NEMDB "®iaµ, nedostatok pamäte pre buffer na kreslenie."
#define MSGTR_NEMFMR "®iaµ, nedostatok pamäte pre vytváranie menu."
#define MSGTR_NEMFMM "®iaµ, nedostatok pamäte pre masku hlavného okna."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[témy] chyba v konfiguraènom súbore tém %d: %s"
#define MSGTR_SKIN_WARNING1 "[témy] v konfiguraènom súbore tém na riadku %d: widget najdený ale pred  \"section\" nenájdený ( %s )"
#define MSGTR_SKIN_WARNING2 "[témy] v konfiguraènom súbore tém na riadku %d: widget najdený ale pred \"subsection\" nenájdený (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "bitmapa s håbkou 16 bit a menej je nepodporovaná ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "súbor nenájdený ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "chyba èítania bmp ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "chyba èítania tga ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "chyba èítania png ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "formát RLE packed tga nepodporovaný ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "neznámy typ súboru ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "chyba konverzie z 24 bit do 32 bit ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "neznáma správa: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nedostatok pamäte\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "príli¹ mnoho fontov deklarovaných\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "súbor fontov nenájdený\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "súbor obrazov fontu nenájdený\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "neexistujúci identifikátor fontu ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "neznámy parameter ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[prehliadaè tém] nedostatok pamäte.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin nenájdený ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Chyba pri èítaní konfiguraèného súboru tém ( %s ).\n"
#define MSGTR_SKIN_LABEL "Témy:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "O aplikácii MPlayer"
#define MSGTR_MENU_Open "Otvori» ..."
#define MSGTR_MENU_PlayFile "Prehra» súbor ..."
#define MSGTR_MENU_PlayVCD "Prehra» VCD ..."
#define MSGTR_MENU_PlayDVD "Prehra» DVD ..."
#define MSGTR_MENU_PlayURL "Prehra» URL ..."
#define MSGTR_MENU_LoadSubtitle "Naèíta» titulky ..."
#define MSGTR_MENU_Playing "Prehrávam"
#define MSGTR_MENU_Play "Prehra»"
#define MSGTR_MENU_Pause "Pauza"
#define MSGTR_MENU_Stop "Zastavi»"
#define MSGTR_MENU_NextStream "Ïal¹í prúd"
#define MSGTR_MENU_PrevStream "Predchádzajúci prúd"
#define MSGTR_MENU_Size "Veµkos»"
#define MSGTR_MENU_NormalSize "Normálna veµkos»"
#define MSGTR_MENU_DoubleSize "Dvojnásobná veµkos»"
#define MSGTR_MENU_FullScreen "Celá obrazovka"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_PlayDisc "Prehra» disk ..."
#define MSGTR_MENU_ShowDVDMenu "Zobrazi» DVD menu"
#define MSGTR_MENU_Titles "Tituly"
#define MSGTR_MENU_Title "Titul %2d"
#define MSGTR_MENU_None "(niè)"
#define MSGTR_MENU_Chapters "Kapitoly"
#define MSGTR_MENU_Chapter "Kapitola %2d"
#define MSGTR_MENU_AudioLanguages "Jazyk zvuku"
#define MSGTR_MENU_SubtitleLanguages "Jazyk titulkov"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Prehliadaè tém"
#define MSGTR_MENU_Preferences "Predvoµby"
#define MSGTR_MENU_Exit "Koniec ..."

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "fatálna chyba ..."
#define MSGTR_MSGBOX_LABEL_Error "chyba ..."
#define MSGTR_MSGBOX_LABEL_Warning "upozornenie ..."

#endif

