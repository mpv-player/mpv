// Translated by:  Codre Adrian <codreadrian@softhome.net>

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2001 Arpad Gereoffy (see DOCS!)\n"
"\n";

static char help_text[]=
"Folosire:   mplayer [opþiuni] [cale/]fiºier\n"
"\n"
"Opþiuni:\n"
" -vo <drv[:disp]> Ieºirea video: driver&dispozitiv ('-vo help' pentru o listã)\n"
" -ao <drv[:disp]> Ieºirea audio: driver&dispozitiv ('-ao help' pentru o listã)\n"
" -vcd <numãr pistã>  foloseºte <pista> de pe dispozitivul VCD în loc de fiºier\n"
#ifdef HAVE_LIBCSS
" -dvdauth <disp>  dispozitivul DVD pentru autentificare (la discuri encriptate)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titlu>  foloseºte titlu/pista de pe dispozitivul DVD în loc de fiºier\n"
#endif
" -ss <poziþia>   sare la poziþia (secunde sau oo:mm:ss)\n"
" -nosound        fãrã sunet\n"
#ifdef USE_FAKE_MONO
" -stereo <mod>   modul stereo la MPEG (0:stereo 1:canalul stâng 2:canalul drept)\n"
#endif
" -fs -vm -zoom   mod tot ecranul (tot ecranul,schimbã modul,scalat prin software)\n"
" -x <x> -y <y>   scaleazã imaginea la <x> * <y> [dacã driver-ul -vo suportã!]\n"
" -sub <fiºier>   specificã fiºierul cu subtitrãri (vezi ºi -subfps, -subdelay)\n"
" -vid x -aid y   opþiuni pentru selectarea pistei video (x) sau audio (y)\n"
" -fps x -srate y opþiuni pentru schimbarea ratei video (x fps) sau audio (y Hz)\n"
" -pp <calitate>  activeazã filtrul de postprocesare (0-4 la DivX, 0-63 la MPEG)\n"
" -nobps          foloseºte metoda alternativã de sicronizare A-V (poate ajuta!)\n"
" -framedrop      activeazã sãritul cadrelor (pentru calculatoare lente)\n"
"\n"
"Taste:\n"
" <-  sau  ->      cautã faþã/spate cu 10 secunde\n"
" sus sau jos      cautã faþã/spate cu 1 minut\n"
" p sau SPACE      pune filmul pe pauzã (orice tastã pentru a continua)\n"
" q sau ESC        opreºte filmul ºi iese din program\n"
" + sau -          ajusteazã decalajul audio cu +/- 0.1 secunde\n"
" o                roteºte modurile OSD: nimic / barã progres / barã progres+ceas\n"
" * sau /          creºte sau scade volumul (apãsaþi 'm' pentru principal/wav)\n"
" z sau x          ajusteazã decalajul subtitrãrii cu +/- 0.1 secunde\n"
"\n"
" * * * VEDEÞI MANUALUL PENTRU DETALII,(ALTE) OPÞIUNI AVANSATE ªI TASTE ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c: 

#define MSGTR_Exiting "\nIes... (%s)\n"
#define MSGTR_Exit_quit "Ieºire"
#define MSGTR_Exit_eof "Sfârºitul fiºierului"
#define MSGTR_Exit_error "Eroare fatalã"
#define MSGTR_IntBySignal "\nMPlayer a fost intrerupt de semnalul %d în modulul: %s \n"
#define MSGTR_NoHomeDir "Nu gãsesc directorul HOME\n"
#define MSGTR_GetpathProblem "get_path(\"config\") cu probleme\n"
#define MSGTR_CreatingCfgFile "Creez fiºierul de configurare: %s\n"
#define MSGTR_InvalidVOdriver "Ieºire video invalidã: %s\nFolosiþi '-vo help' pentru o listã de ieºiri video disponibile.\n"
#define MSGTR_InvalidAOdriver "Ieºire audio invalidã: %s\nFolosiþi '-ao help' pentru o listã de ieºiri audio disponibile.\n"
#define MSGTR_CopyCodecsConf "(copiaþi etc/codecs.conf (din directorul sursã MPlayer) în  ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Nu pot incãrca fontul: %s\n"
#define MSGTR_CantLoadSub "Nu pot incarcã subtitrarea: %s\n"
#define MSGTR_ErrorDVDkey "Eroare la procesarea cheii DVD.\n"
#define MSGTR_CmdlineDVDkey "Cheia DVD specificatã în linia de comandã este pãstratã pentru decodificare.\n"
#define MSGTR_DVDauthOk "Secvenþa de autentificare DVD pare sã fie OK.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATALA: pista selectatã lipseºte!\n"
#define MSGTR_CantOpenDumpfile "Nu pot deschide fiºierul (dump)!!!\n"
#define MSGTR_CoreDumped "core aruncat :)\n"
#define MSGTR_FPSnotspecified "FPS nespecificat (sau invalid) în antet! Folosiþi opþiunea -fps!\n"
#define MSGTR_TryForceAudioFmt "Încerc sã forþez utilizarea unui codec audio din familia %d ...\n"
#define MSGTR_CantFindAfmtFallback "Nu pot sã gãsesc un codec audio pentru familia forþatã, revin la alte drivere.\n"
#define MSGTR_CantFindAudioCodec "Nu gãsesc un codec audio pentru formatul 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Încercaþi sã înnoiþi %s din etc/codecs.conf\n*** Dacã nu ajutã citiþi DOCS/CODECS!\n"
#define MSGTR_CouldntInitAudioCodec "Nu pot sã iniþializez codec-ul audio! -> fãrã sunet\n"
#define MSGTR_TryForceVideoFmt "Încerc sã forþez utilizarea unui codec video din familia %d ...\n"
#define MSGTR_CantFindVideoCodec "Nu gãsesc un codec video pentru formatul 0x%X !\n"
#define MSGTR_VOincompCodec "Îmi pare rãu, ieºirea video selectatã este incompatibilã cu acest codec.\n"
#define MSGTR_CouldntInitVideoCodec "FATALÃ: Nu pot iniþializa codec-ul video :(\n"
#define MSGTR_CannotInitVO "FATALÃ: Nu pot iniþializa diver-ul video!\n"
#define MSGTR_CannotInitAO "nu pot deschide/iniþializa dispozitivul audio -> fãrã sunet\n"
#define MSGTR_StartPlaying "Încep afiºarea...\n"
#define MSGTR_SystemTooSlow "\n*******************************************************************************"\
			    "\n** Sistemul dumneavoastrã este prea LENT ! încercaþi cu -framedrop sau RTFM! **"\
			    "\n*******************************************************************************\n"

#define MSGTR_NoGui "MPlayer a fost compilat fãrã interfaþã graficã!\n"
#define MSGTR_GuiNeedsX "Interfaþa grafica necesitã X11!\n"
#define MSGTR_Playing "Afiºez %s\n"
#define MSGTR_NoSound "Audio: fãrã sunet!!!\n"
#define MSGTR_FPSforced "FPS forþat la %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispozitivul CD-ROM '%s' nu a fost gãsit!\n"
#define MSGTR_ErrTrackSelect "Eroare la selectarea pistei VCD!"
#define MSGTR_ReadSTDIN "Citesc de la intrarea standard...\n"
#define MSGTR_UnableOpenURL "Nu pot accesa adresa: %s\n"
#define MSGTR_ConnToServer "Conectat la serverul: %s\n"
#define MSGTR_FileNotFound "Fiºier negãsit: '%s'\n"

#define MSGTR_CantOpenDVD "Nu am putut deschide dispozitivul DVD: %s\n"
#define MSGTR_DVDwait "Citesc structura discului, vã rog aºteptaþi...\n"
#define MSGTR_DVDnumTitles "Pe acest DVD sunt %d titluri.\n"
#define MSGTR_DVDinvalidTitle "Numãr titlu DVD invalid: %d\n"
#define MSGTR_DVDnumChapters "În acest titlu DVD sunt %d capitole.\n"
#define MSGTR_DVDinvalidChapter "Numãr capitol DVD invalid: %d\n"
#define MSGTR_DVDnumAngles "Sunt %d unghiuri în acest titlu DVD.\n"
#define MSGTR_DVDinvalidAngle "Numãr unghi DVD invalid: %d\n"
#define MSGTR_DVDnoIFO "Nu pot deschide fiºierul IFO pentru titlul DVD %d.\n"
#define MSGTR_DVDnoVOBs "Nu pot deschide fiºierul titlu (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD deschis cu succes!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Atenþie! Antet pistã audio %d redefinit!\n"
#define MSGTR_VideoStreamRedefined "Atenþie! Antet pistã video %d redefinit!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Prea multe (%d în %d bytes) pachete audio în tampon!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Prea multe (%d în %d bytes) pachete video în tampon!\n"
#define MSGTR_MaybeNI "(poate afiºaþi un film/pistã ne-întreþesut sau codec-ul a dat eroare)\n"
#define MSGTR_Detected_XXX_FileFormat "Format fiºier detectat: %s!\n"
#define MSGTR_InvalidMPEGES "Pistã MPEG-ES invalidã??? contactaþi autorul, poate fi un bug :(\n"
#define MSGTR_FormatNotRecognized "============= Îmi pare rãu, acest format de fiºier nu este recunoscut/suportat ===============\n"\
				  "======== Dacã acest fiºier este o pistã AVI, ASF sau MPEG , contactaþi vã rog autorul! ========\n"
#define MSGTR_MissingVideoStream "Nu am gãsit piste video!\n"
#define MSGTR_MissingAudioStream "Nu am gãsit piste audio...  -> fãrã sunet\n"
#define MSGTR_MissingVideoStreamBug "Lipseºte pista video!? Contactaþi autorul, poate fi un bug :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: fiºierul nu conþine pista audio sau video specificatã\n"

#define MSGTR_NI_Forced "Forþat"
#define MSGTR_NI_Detected "Detectat"
#define MSGTR_NI_Message "%s fiºier AVI NE-ÎNTREÞESUT!\n"

#define MSGTR_UsingNINI "Folosesc fiºier AVI NE-ÎNTREÞESUT eronat!\n"
#define MSGTR_CouldntDetFNo "Nu pot determina numãrul de cadre (pentru cãutare absolutã)\n"
#define MSGTR_CantSeekRawAVI "Nu pot cãuta în fiºiere .AVI neindexate! (am nevoie de index, încercaþi cu -idx!)  \n"
#define MSGTR_CantSeekFile "Nu pot cãuta în fiºier!  \n"

#define MSGTR_EncryptedVOB "Fiºier VOB encriptat (necompilat cu suport libcss)! Citiþi fiºierul DOCS/DVD\n"
#define MSGTR_EncryptedVOBauth "Fiºier encriptat dar autentificarea nu a fost cerutã de dumneavoastrã.!!\n"

#define MSGTR_MOVcomprhdr "MOV: Antetele compresate nu sunt (încã) suportate!\n"
#define MSGTR_MOVvariableFourCC "MOV: Atenþie! variabilã FOURCC detectatã!?\n"
#define MSGTR_MOVtooManyTrk "MOV: Atenþie! prea multe piste!"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "nu pot deschide codec-ul audio\n"
#define MSGTR_CantCloseCodec "nu pot deschide codec-ul video\n"

#define MSGTR_MissingDLLcodec "EROARE: Nu pot deschide codec-ul DirectShow necesar: %s\n"
#define MSGTR_ACMiniterror "Nu pot încãrca/iniþializa codec-ul audio Win32/ACM (lipseºte fiºierul DLL?)\n"
#define MSGTR_MissingLAVCcodec "Nu gãsesc codec-ul '%s' în libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATALÃ: EOF în timpul cãutãrii antetului secvenþei\n"
#define MSGTR_CannotReadMpegSequHdr "FATALÃ: Nu pot citi antetul secvenþei!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATALÃ: Nu pot citi extensia antetului secvenþei!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Antet secvenþã eronat!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Extensie antet secvenþã eronatã!\n"

#define MSGTR_ShMemAllocFail "Nu pot aloca memoria partajatã\n"
#define MSGTR_CantAllocAudioBuf "Nu pot aloca tamponul pentru ieºirea audio\n"

#define MSGTR_UnknownAudio "Format audio necunoscut/lipsã, folosesc fãrã sunet\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Setez suportul pentru LIRC...\n"
#define MSGTR_LIRCdisabled "Nu veþi putea utiliza telecomanda\n"
#define MSGTR_LIRCopenfailed "Nu pot deschide suportul pentru LIRC!\n"
#define MSGTR_LIRCcfgerr "Nu pot citi fiºierul de configurare LIRC %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Despre..."
#define MSGTR_FileSelect "Selectare fiºier..."
#define MSGTR_PlayList "Listã de redare..."
#define MSGTR_SkinBrowser "Navigator tematici..."

// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Anulare"
#define MSGTR_Add "Adaugã"
#define MSGTR_Remove "Scoate"

// --- error messages ---
#define MSGTR_NEMDB "Îmi pare rãu, memorie insuficientã pentru tamponul de desenare."
#define MSGTR_NEMFMR "Îmi pare rãu, memorie insuficientã pentru desenarea meniului."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[tematicã] eroare în fiºierul de tematicã la linia %d: %s" 
#define MSGTR_SKIN_WARNING1 "[tematicã] eroare în fiºierul de tematicã la linia %d: componentã gasitã dar înainte \"section\" negãsitã ( %s )"
#define MSGTR_SKIN_WARNING2 "[tematicã] eroare în fiºierul de tematicã la linia %d: componentã gasitã dar înainte \"subsection\" negãsitã (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "adâncimea de culoare de 16 biþi sau mai puþin pentru imagini nesuportatã ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "fiºier negãsit ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "eroare la citire bmp ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "eroare la citire tga ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "eroare la citire png ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "imagini tga împachetate RLE nesuportate ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "tip fiºier necunoscut ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "eroare la conversia de la 24 biþi la 32 biþi ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "mesaj necunoscut: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "memorie insuficientã\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "prea multe font-uri declarate\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "fiºier cu font negºsit\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "fiºier imagine font negãsit\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "identificator font inexistent ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "parametru necunoscut ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[Navigator tematici] memorie insuficientã.\n"

#endif
