// Translated by:  Gabucino <gabucino@mplayerhq.hu>

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002  Gereöffy Árpád  (lásd DOCS!)\n"
"\n";

static char help_text[]=
#ifdef HAVE_NEW_GUI
"Indítás:   mplayer [-gui] [opciók] [url|útvonal/]filenév\n"
#else
"Indítás:   mplayer [opciók] [url|útvonal/]filenév\n"
#endif
"\n"
"Alapvetõ opciók: (az összes opció listájához lásd a man lapot!)\n"
" -vo <drv[:dev]> videomeghajtó és -alegység kiválasztása (lista: '-vo help')\n"
" -ao <drv[:dev]> audiomeghajtó és -alegység kiválasztása (lista: '-ao help')\n"
#ifdef HAVE_VCD
" -vcd <sávszám>  lejátszás VCD (video cd)-sávból, közvetlenül az eszközrõl\n"
#endif
#ifdef HAVE_LIBCSS
" -dvdauth <megh> DVD-meghajtó elérési útjának megadása (kódolt lemezekhez)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  a megadott DVD sáv lejátszása, file helyett\n"
" -alang/-slang   DVD audio/felirat nyelv kiválasztása (2 betûs országkóddal)\n"
#endif
" -ss <idõpoz>    a megadott (másodperc v. óra:perc:mperc) pozícióra tekerés\n"
" -nosound        hanglejátszás kikapcsolása\n"
" -fs -vm -zoom   teljesképernyõs lejátszás opciói (teljkép,módvált,szoft.nagy)\n"
" -x <x> -y <y>   lejátszási ablak felbontásának felülbírálata (módváltáshoz vagy szoftveres nagyításhoz)\n"
" -sub <file>     felhasználandó felirat-file megadása (lásd -subfps, -subdelay)\n"
" -vid x -aid y   lejátszandó video- (x) és audio- (y) stream-ek kiválasztása\n"
" -fps x -srate y video (x képkocka/mp) és audio (y Hz) ráta megadása\n"
" -pp <minõség>   képjavítás fokozatainak beállítása (lásd a man lapot)\n"
" -framedrop      képkockák eldobásának engedélyezése (lassú gépekhez)\n"
"\n"
"Legfontosabb billentyûk: (a teljes listához lásd a man lapot, és az input.conf file-t)\n"
" <-  vagy  ->    10 másodperces hátra/elõre ugrás\n"
" fel vagy le     1 percnyi hátra/elõre ugrás\n"
" pgup v. pgdown  10 percnyi hátra/elõre ugrás\n"
" < vagy >        1 file-al elõre/hátra lépés a lejátszási listában\n"
" p vagy SPACE    pillanatállj (bármely billentyûre továbbmegy)\n"
" q vagy ESC      kilépés\n"
" + vagy -        audio késleltetése +/- 0.1 másodperccel\n"
" o               OSD-mód váltása:  nincs / keresõsáv / keresõsáv+idõ\n"
" * vagy /        hangerõ fel/le\n"
" z vagy x        felirat késleltetése +/- 0.1 másodperccel\n"
" r vagy t        felirat pozíciójának megváltoztatása, lásd -vop expand-ot is!\n"
"\n"
" * * * A MANPAGE TOVÁBBI RÉSZLETEKET, OPCIÓKAT, BILLENTYÛKET TARTALMAZ ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nKilépek... (%s)\n"
#define MSGTR_Exit_frames "Kért számú képkocka lejátszásra került"
#define MSGTR_Exit_quit "Kilépés"
#define MSGTR_Exit_eof "Vége a file-nak"
#define MSGTR_Exit_error "Végzetes hiba"
#define MSGTR_IntBySignal "\nAz MPlayer futása %d-es szignál miatt megszakadt a %s modulban\n"
#define MSGTR_NoHomeDir "Nem találom a HOME konyvtárat\n"
#define MSGTR_GetpathProblem "get_path(\"config\") probléma\n"
#define MSGTR_CreatingCfgFile "Konfigurációs file létrehozása: %s\n"
#define MSGTR_InvalidVOdriver "Nem létezõ video drivernév: %s\nHasználd a '-vo help' opciót, hogy listát kapj a használhato vo meghajtókról.\n"
#define MSGTR_InvalidAOdriver "Nem létezõ audio drivernév: %s\nHasználd az '-ao help' opciót, hogy listát kapj a használhato ao meghajtókról.\n"
#define MSGTR_CopyCodecsConf "(másold/linkeld az etc/codecs.conf file-t ~/.mplayer/codecs.conf-ba)\n"
#define MSGTR_CantLoadFont "Nem tudom betölteni a következõ fontot: %s\n"
#define MSGTR_CantLoadSub "Nem tudom betölteni a feliratot: %s\n"
#define MSGTR_ErrorDVDkey "Hiba a DVD-KULCS feldolgozása közben.\n"
#define MSGTR_CmdlineDVDkey "A parancssorban megadott DVD-kulcs további dekódolás céljából eltárolásra került.\n"
#define MSGTR_DVDauthOk "DVD-autentikációs folyamat, úgy tünik, sikerrel végzõdött.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: VÉGZETES HIBA: a kért stream nem található!\n"
#define MSGTR_CantOpenDumpfile "Nem tudom megnyitni a dump file-t!\n"
#define MSGTR_CoreDumped "Kinyomattam a cuccost, jól.\n"
#define MSGTR_FPSnotspecified "Az FPS (képkocka/mp) érték nincs megadva, vagy hibás! Használd az -fps opciót!\n"
#define MSGTR_NoVideoStream "Ebben nincs video stream... egyelõre lejátszhatatlan\n"
#define MSGTR_TryForceAudioFmt "Megpróbálom a(z) %d audio codec-családot használni ...\n"
#define MSGTR_CantFindAfmtFallback "A megadott audio codec-családban nem találtam idevaló meghajtót, próbálkozok más meghajtóval.\n"
#define MSGTR_CantFindAudioCodec "Nem találok codecet a(z) 0x%X audio-formátumhoz !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Frissítsd a %s-t az etc/codecs.conf-ból\n*** Ha még mindig nem jó, olvasd el a DOCS/CODECS-et!\n"
#define MSGTR_CouldntInitAudioCodec "Nem tudom indítani az audio codecet! -> nincshang ;)\n"
#define MSGTR_TryForceVideoFmt "Megpróbálom a(z) %d video codec-családot használni ...\n"
#define MSGTR_CantFindVfmtFallback "A megadott video codec-családban nem találtam idevaló meghajtót, próbálkozok más meghajtóval.\n"
#define MSGTR_CantFindVideoCodec "Nem találok codec-et ami megfelel a kivalasztott vo-hoz es 0x%X video-formátumhoz !\n"
#define MSGTR_VOincompCodec "A kiválasztott video_out meghajtó inkompatibilis ezzel a codec-kel.\n"
#define MSGTR_CouldntInitVideoCodec "VÉGZETES HIBA: Nem sikerült a video codecet elindítani :(\n"
#define MSGTR_EncodeFileExists "A %s file már létezik (nehogy letöröld a kedvenc AVI-dat!)\n"
#define MSGTR_CantCreateEncodeFile "Nem tudom enkódolás céljából létrehozni a filet\n"
#define MSGTR_CannotInitVO "VÉGZETES HIBA: Nem tudom elindítani a video-meghajtót!\n"
#define MSGTR_CannotInitAO "nem tudom megnyitni az audio-egységet -> NOSOUND\n"
#define MSGTR_StartPlaying "Lejátszás indítása...\n"
#define MSGTR_SystemTooSlow "\n\n"\
"         ***************************************\n"\
"         **** A rendszered túl LASSÚ ehhez! ****\n"\
"         ***************************************\n"\
"!!! Lehetséges okok, és megoldásaik: \n"\
"- Legyakrabban : hibás _audio_ meghajtó. Workaround: próbáld az -ao sdl\n"\
"  opciót, vagy 0.5-ös ALSA-t, vagy ALSA 0.9-et oss emulációval.\n"\
"  További info a DOCS/sound.html file-ban!\n"\
"- Lassú video kimenet. Próbálj másik -vo meghajtót (lista: -vo help) vagy\n"\
"  a -framedrop opciót ! Sebességnövelõ tippekhez lásd DOCS/video.html.\n"\
"- Lassú CPU. Fölösleges gyenge CPU-n DVD-t vagy nagy DivX-et lejátszani.\n"\
"  Talán -hardframedrop opcióval.\n"\
"- Hibás file. A következõk kombinációjaival probálkozz: -nobps -ni -mc 0\n"\
"  -forceidx\n"\
"Ha egyik se müxik, olvasd el a DOCS/bugreports.html file-t !\n\n"

#define MSGTR_NoGui "Az MPlayer grafikus felület NÉLKÜL lett fordítva!\n"
#define MSGTR_GuiNeedsX "Az MPlayer grafikus felületének X11-re van szüksége!\n"
#define MSGTR_Playing "%s lejátszása\n"
#define MSGTR_NoSound "Audio: nincs hang!!!\n"
#define MSGTR_FPSforced "FPS kényszerítve %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "A CD-ROM meghajtó (%s) nem található!\n"
#define MSGTR_ErrTrackSelect "Hiba a VCD-sáv kiválasztásakor!"
#define MSGTR_ReadSTDIN "Olvasás a szabványos bemenetrõl (stdin)...\n"
#define MSGTR_UnableOpenURL "Nem megnyitható az URL: %s\n"
#define MSGTR_ConnToServer "Csatlakozom a szerverhez: %s\n"
#define MSGTR_FileNotFound "A file nem található: '%s'\n"

#define MSGTR_CantOpenDVD "Nem tudom megnyitni a DVD eszközt: %s\n"
#define MSGTR_DVDwait "A lemez struktúrájának olvasása, kérlek várj...\n"
#define MSGTR_DVDnumTitles "%d sáv van a DVD-n.\n"
#define MSGTR_DVDinvalidTitle "Helytelen DVD sáv: %d\n"
#define MSGTR_DVDnumChapters "Az adott DVD sávban %d fejezet van.\n"
#define MSGTR_DVDinvalidChapter "Helytelen DVD fejezet: %d\n"
#define MSGTR_DVDnumAngles "%d darab kameraállás van ezen a DVD sávon.\n"
#define MSGTR_DVDinvalidAngle "Helytelen DVD kameraállás: %d\n"
#define MSGTR_DVDnoIFO "Nem tudom a(z) %d. DVD sávhoz megnyitni az IFO file-t.\n"
#define MSGTR_DVDnoVOBs "Nem tudom megnyitni a sávot (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD sikeresen megnyitva!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Vigyázat! Többszörösen definiált Audio-folyam! (Hibás file?)\n"
#define MSGTR_VideoStreamRedefined "Vigyázat! Többszörösen definiált Video-folyam! (Hibás file?)\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) audio-csomag a pufferben!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) video-csomag a pufferben!\n"
#define MSGTR_MaybeNI "(talán ez egy nem összefésült (interleaved) file vagy a CODEC nem mûködik jól)\n"
#define MSGTR_SwitchToNi "\nRosszul összefésült (interleaved) file, átváltás -ni módba!\n"
#define MSGTR_DetectedFILMfile "Ez egy FILM formátumú file!\n"
#define MSGTR_DetectedFLIfile "Ez egy FLI formátumú file!\n"
#define MSGTR_DetectedROQfile "Ez egy RoQ formátumú file!\n"
#define MSGTR_DetectedREALfile "Ez egy REAL formátumú file!\n"
#define MSGTR_DetectedAVIfile "Ez egy AVI formátumú file!\n"
#define MSGTR_DetectedASFfile "Ez egy ASF formátumú file!\n"
#define MSGTR_DetectedMPEGPESfile "Ez egy MPEG-PES formátumú file!\n"
#define MSGTR_DetectedMPEGPSfile "Ez egy MPEG-PS formátumú file!\n"
#define MSGTR_DetectedMPEGESfile "Ez egy MPEG-ES formátumú file!\n"
#define MSGTR_DetectedQTMOVfile "Ez egy QuickTime/MOV formátumú file! (ez még nem támogatott)\n"
#define MSGTR_MissingMpegVideo "Nincs MPEG video-folyam? Lépj kapcsolatba a készítõkkel, lehet, hogy hiba!\n"
#define MSGTR_InvalidMPEGES "Hibás MPEG-ES-folyam? Lépj kapcsolatba a készítõkkel, lehet, hogy hiba!\n"
#define MSGTR_FormatNotRecognized "========= Sajnos ez a fileformátum ismeretlen vagy nem támogatott ===========\n"\
				  "= Ha ez egy AVI, ASF vagy MPEG file, lépj kapcsolatba a készítõkkel (hiba)! =\n"
#define MSGTR_MissingVideoStream "Nincs képfolyam!\n"
#define MSGTR_MissingAudioStream "Nincs hangfolyam... -> hang nélkül\n"
#define MSGTR_MissingVideoStreamBug "Nincs képfolyam?! Írj a szerzõnek, lehet hogy hiba :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: a file nem tartalmazza a kért hang vagy kép folyamot\n"

#define MSGTR_NI_Forced "Kényszerítve"
#define MSGTR_NI_Detected "Detektálva"
#define MSGTR_NI_Message "%s NON-INTERLEAVED AVI formátum!\n"

#define MSGTR_UsingNINI "NON-INTERLEAVED hibás AVI formátum használata!\n"
#define MSGTR_CouldntDetFNo "Nem tudom meghatározni a képkockák számát (abszolut tekeréshez)   \n"
#define MSGTR_CantSeekRawAVI "Nem tudok nyers .AVI-kban tekerni! (index kell, próbáld az -idx kapcsolóval!)\n"
#define MSGTR_CantSeekFile "Nem tudok ebben a fileban tekerni!  \n"

#define MSGTR_EncryptedVOB "Kódolt VOB file (libcss támogatás nincs befordítva!) Olvasd el a doksit\n"
#define MSGTR_EncryptedVOBauth "Kódolt folyam, de nem kértél autentikálást!!\n"

#define MSGTR_MOVcomprhdr "MOV: Tömörített fejlécek (még) nincsenek támogatva!\n"
#define MSGTR_MOVvariableFourCC "MOV: Vigyázat! változó FOURCC detektálva!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Vigyázat! túl sok sáv!"
#define MSGTR_MOVnotyetsupp "\n****** Quicktime MOV formátum még nincs támogatva!!!!!! *******\n"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nem tudom megnyitni a kodeket\n"
#define MSGTR_CantCloseCodec "nem tudom lezárni a kodeket\n"

#define MSGTR_MissingDLLcodec "HIBA: Nem tudom megnyitni a kért DirectShow kodeket: %s\n"
#define MSGTR_ACMiniterror "Nem tudom betölteni/inicializálni a Win32/ACM kodeket (hiányzó DLL file?)\n"
#define MSGTR_MissingLAVCcodec "Nem találom a(z) '%s' nevû kodeket a libavcodec-ben...\n"

#define MSGTR_NoDShowSupport "Az MPlayer DirectShow támogatás NÉLKÜL lett fordítva!\n"
#define MSGTR_NoWfvSupport "A win32-es kodekek támogatása ki van kapcsolva, vagy nem létezik nem-x86-on!\n"
#define MSGTR_NoDivx4Support "Az MPlayer DivX4Linux támogatás (libdivxdecore.so) NÉLKÜL lett fordítva!\n"
#define MSGTR_NoLAVCsupport "Az MPlayer ffmpeg/libavcodec támogatás NÉLKÜL lett fordítva!\n"
#define MSGTR_NoACMSupport "Win32/ACM hang kodek támogatás ki van kapcsolva, vagy nem létezik nem-x86 CPU-n -> hang kikapcsolva :(\n"
#define MSGTR_NoDShowAudio "DirectShow támogatás nincs lefordítva -> hang kikapcsolva :(\n"
#define MSGTR_NoOggVorbis "OggVorbis hang kodek kikapcsolva -> hang kikapcsolva :(\n"
#define MSGTR_NoXAnimSupport "Az MPlayer-t XAnim codec-ek támogatása NÉLKÜL fordítottad!\n"

#define MSGTR_MpegPPhint "FIGYELEM! Képjavítást kértél egy MPEG1/2 filmre, de az MPlayer-t\n" \
                         "          MPEG1/2 javítási támogatás nélkül fordítottad!\n" \
                         "          #define MPEG12_POSTPROC a config.h-ba, és fordítsd újra libmpeg2-t!\n"
#define MSGTR_MpegNoSequHdr "MPEG: VÉGZETES: vége lett a filenak miközben a szekvencia fejlécet kerestem\n"
#define MSGTR_CannotReadMpegSequHdr "VÉGZETES: Nem tudom olvasni a szekvencia fejlécet!\n"
#define MSGTR_CannotReadMpegSequHdrEx "VÉGZETES: Nem tudom olvasni a szekvencia fejléc kiterjesztését!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Hibás szekvencia fejléc!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Hibás szekvencia fejléc kiterjesztés!\n"

#define MSGTR_ShMemAllocFail "Nem tudok megosztott memóriát lefoglalni\n"
#define MSGTR_CantAllocAudioBuf "Nem tudok kimeneti hangbuffer lefoglalni\n"
#define MSGTR_NoMemForDecodedImage "nincs elég memória a dekódolt képhez (%ld bájt)\n"

#define MSGTR_AC3notvalid "AC3 folyam hibás.\n"
#define MSGTR_AC3only48k "Csak 48000 Hz-es folyamok vannak támogatva.\n"
#define MSGTR_UnknownAudio "Ismeretlen/hiányzó hangformátum, hang kikapcsolva\n"

// LIRC:
#define MSGTR_SettingUpLIRC "lirc támogatás indítása...\n"
#define MSGTR_LIRCdisabled "Nem fogod tudni használni a távirányítót\n"
#define MSGTR_LIRCopenfailed "Nem tudtam megnyitni a lirc támogatást!\n"
#define MSGTR_LIRCsocketerr "Valami baj van a lirc socket-tel: %s\n"
#define MSGTR_LIRCcfgerr "Nem tudom olvasni a LIRC konfigurációs file-t : %s \n"

//  ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Az MPlayer - röl"
#define MSGTR_FileSelect "File kiválasztása ..."
#define MSGTR_SubtitleSelect "Felirat kiválasztása ..."
#define MSGTR_OtherSelect "File kiválasztása ..."
#define MSGTR_AudioFileSelect "Külsõ audio csatorna választása ..."
#define MSGTR_MessageBox "Üzenetablak"
#define MSGTR_Equalizer "Equalizer"
#define MSGTR_PlayList "Lejátszási lista"
#define MSGTR_SkinBrowser "Skin böngészõ"
#define MSGTR_Network "Lejátszás WEB - röl ..."
#define MSGTR_Preferences "Beállítások"
#define MSGTR_OSSPreferences "OSS driver beállítások"
#define MSGTR_NoMediaOpened "nincs megnyitva semmi"
#define MSGTR_VCDTrack "%d. VCD track"
#define MSGTR_NoChapter "nincs megnyitott fejezet"
#define MSGTR_Chapter "%d. fejezet"
#define MSGTR_NoFileLoaded "nincs file betöltve"

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Mégse"
#define MSGTR_Add "Hozzáad"
#define MSGTR_Remove "Kivesz"
#define MSGTR_Clear "Törlés"
#define MSGTR_Config "Beállítás"
#define MSGTR_ConfigDriver "Driver beállítása"
#define MSGTR_Browse "Tallózás"

// --- error messages ---
#define MSGTR_NEMDB "Nincs elég memória a buffer kirajzolásához."
#define MSGTR_NEMFMR "Nincs elég memória a menü rendereléséhez."
#define MSGTR_NEMFMM "Nincs elég memória a fõablak alakjának maszkolásához."
#define MSGTR_IDFGCVD "Nem talaltam gui kompatibilis video drivert."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] hiba a skin konfigurációs file-jának %d. sorában: %s"
#define MSGTR_SKIN_WARNING1 "[skin] figyelmeztetés a skin konfigurációs file-jának %d. sorában: widget megvan, de nincs elõtte \"section\" ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] figyelmeztetés a skin konfigurációs file-jának %d. sorában: widget megvan, de nincs elõtte \"subsection\" ( %s )"
#define MSGTR_SKIN_BITMAP_16bit  "16 vagy kevesebb bites bitmap nem támogatott ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "file nem található ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "bmp olvasási hiba ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "tga olvasási hiba ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "png olvasási hiba ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE tömörített tga-k nincsenek támogatva ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "ismeretlen tipusú file ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "hiba a 24-rõl 32bitre konvertálás közben ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "ismeretlen üzenet: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "nincs elég memória\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "túl sok betûtipus van deklarálva\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "nem találom a betûtipus file-t\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "nem találom a betûtipus képfile-t"
#define MSGTR_SKIN_FONT_NonExistentFontID "nemlétezõ betûtipus azonosító ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "ismeretlen paraméter ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinböngészõ] nincs elég memória.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin nem található ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Skin configfile olvasási hiba ( %s ).\n"
#define MSGTR_SKIN_LABEL "Skin-ek:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Az MPlayer-rõl"
#define MSGTR_MENU_Open "Megnyitás ..."
#define MSGTR_MENU_PlayFile "File lejátszás ..."
#define MSGTR_MENU_PlayVCD "VCD lejátszás ..."  
#define MSGTR_MENU_PlayDVD "DVD lejátszás ..."  
#define MSGTR_MENU_PlayURL "URL lejátszás ..."  
#define MSGTR_MENU_LoadSubtitle "Felirat betöltése ..."
#define MSGTR_MENU_DropSubtitle "Felirat eldobása ..."
#define MSGTR_MENU_LoadExternAudioFile "Külsõ hang betöltése ..."
#define MSGTR_MENU_Playing "Lejátszás"
#define MSGTR_MENU_Play "Lejátszás"
#define MSGTR_MENU_Pause "Pillanatállj"
#define MSGTR_MENU_Stop "Állj"  
#define MSGTR_MENU_NextStream "Következõ file"
#define MSGTR_MENU_PrevStream "Elõzõ file"
#define MSGTR_MENU_Size "Méret"
#define MSGTR_MENU_NormalSize "Normál méret"
#define MSGTR_MENU_DoubleSize "Dupla méret"
#define MSGTR_MENU_FullScreen "Teljesképernyõ" 
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Lemez megnyitása ..."
#define MSGTR_MENU_ShowDVDMenu "DVD menû"
#define MSGTR_MENU_Titles "Sávok"
#define MSGTR_MENU_Title "%2d. sáv"
#define MSGTR_MENU_None "(nincs)"
#define MSGTR_MENU_Chapters "Fejezetek"
#define MSGTR_MENU_Chapter "%2d. fejezet"
#define MSGTR_MENU_AudioLanguages "Szinkron nyelvei"
#define MSGTR_MENU_SubtitleLanguages "Feliratok nyelvei"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Skin böngészõ"
#define MSGTR_MENU_Preferences "Beállítások" 
#define MSGTR_MENU_Exit "Kilépés ..."

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Kontraszt: "
#define MSGTR_EQU_Brightness "Fényerõ: "
#define MSGTR_EQU_Hue "Szinárnyalat: "
#define MSGTR_EQU_Saturation "Telítettség: "
#define MSGTR_EQU_Front_Left "Bal Elsõ"
#define MSGTR_EQU_Front_Right "Jobb Elsõ"
#define MSGTR_EQU_Back_Left "Bal Hátsó"
#define MSGTR_EQU_Back_Right "Jobb Hátsó"
#define MSGTR_EQU_Center "Középsõ"
#define MSGTR_EQU_Bass "Basszus"
#define MSGTR_EQU_All "Mindegyik"

// --- playlist
#define MSGTR_PLAYLIST_Path "Utvonal"
#define MSGTR_PLAYLIST_Selected "Kiv'lasztott filr - ok"
#define MSGTR_PLAYLIST_Files "File - ok"
#define MSGTR_PLAYLIST_DirectoryTree "Könyvtár lista"

// --- preferences
#define MSGTR_PREFERENCES_None "Egyik sem"
#define MSGTR_PREFERENCES_AvailableDrivers "Driverek:"
#define MSGTR_PREFERENCES_DoNotPlaySound "Hang nélkül"
#define MSGTR_PREFERENCES_NormalizeSound "Hang normalizálása"
#define MSGTR_PREFERENCES_EnEqualizer "Audio equalizer"
#define MSGTR_PREFERENCES_ExtraStereo "Extra stereo"
#define MSGTR_PREFERENCES_Coefficient "Együttható:"
#define MSGTR_PREFERENCES_AudioDelay "Hang késleltetés"
#define MSGTR_PREFERENCES_Audio "Audio"
#define MSGTR_PREFERENCES_VideoEqu "Video equalizer"
#define MSGTR_PREFERENCES_DoubleBuffer "Dupla bufferelés"
#define MSGTR_PREFERENCES_DirectRender "Direct rendering"
#define MSGTR_PREFERENCES_FrameDrop "Kép eldobás"
#define MSGTR_PREFERENCES_HFrameDrop "Erõszakos kép eldobó"
#define MSGTR_PREFERENCES_Flip "Kép fejjel lefelé"
#define MSGTR_PREFERENCES_Panscan "Panscan: "
#define MSGTR_PREFERENCES_Video "Video"
#define MSGTR_PREFERENCES_OSDTimer "Óra es indikátorok"
#define MSGTR_PREFERENCES_OSDProgress "Csak a százalék jelzõk"
#define MSGTR_PREFERENCES_Subtitle "Felirat:"
#define MSGTR_PREFERENCES_SUB_Delay "Késleltetés: "
#define MSGTR_PREFERENCES_SUB_FPS "FPS:"
#define MSGTR_PREFERENCES_SUB_POS "Pozíciója: "
#define MSGTR_PREFERENCES_SUB_AutoLoad "Felirat automatikus betöltésének tiltása"
#define MSGTR_PREFERENCES_SUB_Unicode "Unicode felirat"
#define MSGTR_PREFERENCES_SUB_MPSUB "A film feliratának konvertálása MPlayer felirat formátumba"
#define MSGTR_PREFERENCES_SUB_SRT "A film feliratának konvertálása SubViewer ( SRT ) formátumba"
#define MSGTR_PREFERENCES_Font "Betûk:"
#define MSGTR_PREFERENCES_FontFactor "Betû együttható:"
#define MSGTR_PREFERENCES_PostProcess "Képjavítás"
#define MSGTR_PREFERENCES_AutoQuality "Autómatikus minõség állítás: "
#define MSGTR_PREFERENCES_NI "non-interleaved  AVI  feltételezése (hibás AVI-knál segíthet"
#define MSGTR_PREFERENCES_IDX "Az AVI indexének újraépítése, ha szükséges"
#define MSGTR_PREFERENCES_VideoCodecFamily "Video kodek család:"
#define MSGTR_PREFERENCES_AudioCodecFamily "Audio kodek család:"
#define MSGTR_PREFERENCES_FRAME_OSD_Level "OSD szint"
#define MSGTR_PREFERENCES_FRAME_Subtitle "Felirat"
#define MSGTR_PREFERENCES_FRAME_Font "Betû"
#define MSGTR_PREFERENCES_FRAME_PostProcess "Képjavítás"
#define MSGTR_PREFERENCES_FRAME_CodecDemuxer "Codec & demuxer"
#define MSGTR_PREFERENCES_OSS_Device "Meghajtó:"
#define MSGTR_PREFERENCES_OSS_Mixer "Mixer:"
#define MSGTR_PREFERENCES_Message "Kérlek emlékezz, néhány opció igényli a lejátszás újraindítását."
#define MSGTR_PREFERENCES_DXR3_VENC "Video kódoló:"
#define MSGTR_PREFERENCES_DXR3_LAVC "LAVC használata (ffmpeg)"
#define MSGTR_PREFERENCES_DXR3_FAME "FAME használata"
#define MSGTR_PREFERENCES_FontEncoding1 "Unicode"
#define MSGTR_PREFERENCES_FontEncoding2 "Nyugat-Európai karakterkészlet (ISO-8859-1)"
#define MSGTR_PREFERENCES_FontEncoding3 "Nyugat-Európai karakterkészlet euróval (ISO-8859-15)"
#define MSGTR_PREFERENCES_FontEncoding4 "Szláv / Közép-Európai karakterkészlet (ISO-8859-2)"
#define MSGTR_PREFERENCES_FontEncoding5 "Eszperantó, gall, máltai, török karakterkészlet (ISO-8859-3)"
#define MSGTR_PREFERENCES_FontEncoding6 "Régi baltik karakterkészlet (ISO-8859-4)"
#define MSGTR_PREFERENCES_FontEncoding7 "Cirill karakterkészlet (ISO-8859-5)"
#define MSGTR_PREFERENCES_FontEncoding8 "Arab karakterkészlet (ISO-8859-6)"
#define MSGTR_PREFERENCES_FontEncoding9 "Modern görög karakterkészlet (ISO-8859-7)"
#define MSGTR_PREFERENCES_FontEncoding10 "Török karakterkészlet (ISO-8859-9)"
#define MSGTR_PREFERENCES_FontEncoding11 "Baltik karakterkészlet (ISO-8859-13"
#define MSGTR_PREFERENCES_FontEncoding12 "Kelta karakterkészlet (ISO-8859-14)"
#define MSGTR_PREFERENCES_FontEncoding13 "Héber karakterkészlet (ISO-8859-8)"
#define MSGTR_PREFERENCES_FontEncoding14 "Orosz karakterkészlet (KOI8-R)"
#define MSGTR_PREFERENCES_FontEncoding15 "Ukrán, Belorusz karakterkészlet (KOI8-U/UR)"
#define MSGTR_PREFERENCES_FontEncoding16 "Egyszerû kínai karakterkészlet (CP936)"
#define MSGTR_PREFERENCES_FontEncoding17 "Tradicionális kínai karakterkészlet (BIG5)"
#define MSGTR_PREFERENCES_FontEncoding18 "Japán karakterkészlet (SHIFT-JIS)"
#define MSGTR_PREFERENCES_FontEncoding19 "Koreai karakterkészlet (CP949)"
#define MSGTR_PREFERENCES_FontEncoding20 "Thai karakterkészlet (CP874)"
#define MSGTR_PREFERENCES_FontNoAutoScale "Nincs automata karakterméret választás"
#define MSGTR_PREFERENCES_FontPropWidth "Karakterméret film szélességéhez való állítása"
#define MSGTR_PREFERENCES_FontPropHeight "Karakterméret film magasságához való állítása"
#define MSGTR_PREFERENCES_FontPropDiagonal "Karakterméret film átlójához való állítása"
#define MSGTR_PREFERENCES_FontEncoding "Kódolás:"
#define MSGTR_PREFERENCES_FontBlur "Blur:"
#define MSGTR_PREFERENCES_FontOutLine "Körvonal:"
#define MSGTR_PREFERENCES_FontTextScale "Szöveg skála:"
#define MSGTR_PREFERENCES_FontOSDScale "OSD skála:"
#define MSGTR_PREFERENCES_SubtitleOSD "Felirat & OSD"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "végzetes hiba ..."
#define MSGTR_MSGBOX_LABEL_Error "hiba ..."
#define MSGTR_MSGBOX_LABEL_Warning "figyelmeztetés ..."

#endif
