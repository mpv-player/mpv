#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2001  Gereöffy Árpád  (lásd DOCS/AUTHORS)\n"
"\n";

static char help_text[]=
"Indítás:   mplayer [opciók] [útvonal/]filenév\n"
"\n"
"Opciók:\n"
" -vo <drv[:dev]> videomeghajtó és -alegység kiválasztása (lista: '-vo help')\n"
" -ao <drv[:dev]> audiomeghajtó és -alegység kiválasztása (lista: '-ao help')\n"
" -vcd <sávszám>  lejátszás VCD (video cd)-sávból, közvetlenül az eszközrõl\n"
#ifdef HAVE_LIBCSS
" -dvdauth <megh> DVD-meghajtó elérési útjának megadása (kódolt lemezekhez)\n"
#endif
" -ss <idõpoz>    a megadott (másodperc v. óra:perc:mperc) pozícióra tekerés\n"
" -nosound        hanglejátszás kikapcsolása\n"
#ifdef USE_FAKE_MONO
" -stereo         MPEG1 sztereó szabályozása (0:sztereó, 1:bal, 2:jobb)\n"
#endif
" -fs -vm -zoom   teljesképernyõs lejátszás opciói (teljkép,módvált,szoft.nagy)\n"
" -x <x> -y <y>   kép nagyítása <x> * <y> méretûre [ha -vo <meghajtó> támogatja]\n"
" -sub <file>     felhasználandó felirat-file megadása (lásd -subfps, -subdelay)\n"
" -vid x -aid y   lejátszandó video- (x) és audio- (y) stream-ek kiválasztása\n"
" -fps x -srate y video (x képkocka/mp) és audio (y Hz) ráta megadása\n"
" -pp <minõség>   utókezelési fokozatok beállítása (0-63)\n"
" -bps            alternatív A/V szinkron módszerének kiválasztása\n"
" -framedrop      képkockák eldobásának engedélyezése (lassú gépekhez)\n"
"\n"
"Billentyûk:\n"
" <-  vagy  ->    10 másodperces hátra/elõre ugrás\n"
" fel vagy le     1 percnyi hátra/elõre ugrás\n"
" pgup v. pgdown  10 percnyi hátra/elõre ugrás\n"
" p vagy SPACE    pillanatállj (bármely billentyûre továbbmegy)\n"
" q vagy ESC      kilépés\n"
" + vagy -        audio késleltetése +/- 0.1 másodperccel\n"
" o               OSD-mód váltása:  nincs / keresõsáv / keresõsáv+idõ\n"
" * vagy /        hangerõ fel/le ('m' billentyû master/pcm között vált)\n"
" z vagy x        felirat késleltetése +/- 0.1 másodperccel\n"
"\n"
" * * * A MANPAGE TOVÁBBI RÉSZLETEKET, OPCIÓKAT, BILLENTYÛKET TARTALMAZ ! * * *\n"
"\n";
#endif

// mplayer.c: 

#define MSGTR_Exiting "\nKilépek... (%s)\n"
#define MSGTR_Exit_frames "Kért számú képkocka lejátszásra került"
#define MSGTR_Exit_quit "Kilépés"
#define MSGTR_Exit_eof "Vége a file-nak"
#define MSGTR_Exit_error "Végzetes hiba"
#define MSGTR_IntBySignal "\nAz MPlayer futása a %s modulban kapott %d szignál miatt megszakadt \n"
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
#define MSGTR_CantFindVideoCodec "Nem találok codecet a(z) 0x%X video-formátumhoz !\n"
#define MSGTR_VOincompCodec "A kiválasztott video_out meghajtó inkompatibilis ezzel a codec-kel.\n"
#define MSGTR_CouldntInitVideoCodec "VÉGZETES HIBA: Nem sikerült a video codecet elindítani :(\n"
#define MSGTR_EncodeFileExists "A %s file már létezik (nehogy letöröld a kedvenc AVI-dat!)\n"
#define MSGTR_CantCreateEncodeFile "Nem tudom enkódolás céljából létrehozni a filet\n"
#define MSGTR_CannotInitVO "VÉGZETES HIBA: Nem tudom elindítani a video-meghajtót!\n"
#define MSGTR_CannotInitAO "nem tudom megnyitni az audio-egységet -> NOSOUND\n"
#define MSGTR_StartPlaying "Lejátszás indítása...\n"
#define MSGTR_SystemTooSlow "\n************************************************************************"\
			    "\n** A rendszered túl LASSÚ ehhez! Próbáld -framedrop-pal, vagy RTFM!  **"\
			    "\n************************************************************************\n"
//#define MSGTR_

// open.c: 
#define MSGTR_CdDevNotfound "A CD-ROM meghajtó (%s) nem található!\n"
#define MSGTR_ErrTrackSelect "Hiba a VCD-sáv kiválasztásakor!"
#define MSGTR_ReadSTDIN "Olvasás a szabványos bemenetrõl (stdin)...\n"
#define MSGTR_UnableOpenURL "Nem megnyitható az URL: %s\n"
#define MSGTR_ConnToServer "Csatlakozom a szerverhez: %s\n"
#define MSGTR_FileNotFound "A file nem található: '%s'\n"

// demuxer.c:
#define MSGTR_AudioStreamRedefined "Vigyázat! Többszörösen definált Audio-folyam! (Hibás file?)\n"
#define MSGTR_VideoStreamRedefined "Vigyázat! Többszörösen definált Video-folyam! (Hibás file?)\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) audio-csomag a pufferben!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Túl sok (%d db, %d bájt) video-csomag a pufferben!\n"
#define MSGTR_MaybeNI "(talán ez egy nem összefésült file vagy a CODEC nem mûködik jól)\n"
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
#define MSGTR_MissingASFvideo "ASF: Nincs képfolyam!\n"
#define MSGTR_MissingASFaudio "ASF: Nincs hangfolyam... -> hang nélkül\n"
#define MSGTR_MissingMPEGaudio "MPEG: Nincs hangfolyam... -> hang nélkül\n"

//#define MSGTR_
