// Translated by: Fabio Olimpieri <fabio.olimpieri@tin.it>

// Translated files should be uploaded to ftp://mplayerhq.hu/MPlayer/incoming
// and send a notify message to mplayer-dev-eng maillist.

// ========================= MPlayer help ===========================

#ifdef HELP_MP_DEFINE_STATIC
static char* banner_text=
"\n\n"
"MPlayer " VERSION "(C) 2000-2002 Arpad Gereoffy (vedi DOCS!)\n"
"\n";

static char help_text[]=
"Uso:   mplayer [opzioni] [percorso/]nome_file\n"
"\n"
"Opzioni:\n"
" -vo <drv[:dev]> seleziona il driver ed il dispositivo video di output ('-vo help' per la lista)\n"
" -ao <drv[:dev]> seleziona il driver ed il dispositivo audio di output ('-ao help' per la lista)\n"
" -vcd <trackno>  riproduce la traccia VCD (video cd) dal dispositivo invece che dal file normale\n"
#ifdef HAVE_LIBCSS
" -dvdauth <dev>  specifica il dispositivo DVD per l\'autenticazione (per dischi criptati)\n"
#endif
#ifdef USE_DVDREAD
" -dvd <titleno>  riproduce il titolo/traccia del DVD dal dispositivo invece che dal file normale\n"
#endif
" -ss <timepos>   cerca una determinata posizione (in secondi o in hh:mm:ss) \n"
" -nosound        non riproduce l\'audio\n"
#ifdef USE_FAKE_MONO
" -stereo <mode>  seleziona l\'uscita stereo MPEG1 (0:stereo 1:sinistra 2:destra)\n"
#endif
" -channels <n>   numero desiderato di canali audio di uscita\n"
" -fs -vm -zoom   opzioni di riproduzione a schermo intero (schermo int,cambia video,scalatura softw)\n"
" -x <x> -y <y>   scala l\'immagine alla risoluzione <x> * <y> [se -vo driver lo supporta!]\n"
" -sub <file>     specifica il file dei sottotitoli da usare (vedi anche -subfps, -subdelay)\n"
" -playlist <file> specifica il file della playlist\n"
" -vid x -aid y   opzioni per selezionare il flusso video (x) ed audio (y) da riprodurre\n"
" -fps x -srate y opzioni per cambiare il rate del video (x fps) e dell\'audio (y Hz)\n"
" -pp <quality>   abilita il filtro di postelaborazione (0-4 per DivX, 0-63 per mpegs)\n"
" -nobps          usa il metodo di sincronizzazione A-V alternativo per i file AVI (può aiutare!)\n"
" -framedrop      abilita lo scarto dei fotogrammi (per macchine lente)\n"
" -wid <window id> usa una finestra esistente per l\'uscita video (utile per plugger!)\n"
"\n"
"Tasti:\n"
" <-  o  ->       va indietro/avanti di 10 secondi\n"
" su o giù        va indietro/avanti di 1 minuto\n"
" < o >           va indietro/avanti nella playlist\n"
" p o SPAZIO      mette in pausa il filmato (premere un qualunque tasto per continuare)\n"
" q o ESC         ferma la riproduzione ed esce dal programma\n"
" + o -           regola il ritardo audio di +/- 0.1 secondi\n"
" o               cambia tra le modalità OSD: niente / barra di ricerca / barra di ricerca + tempo\n"
" * o /           incrementa o decrementa il volume (premere 'm' per selezionare master/pcm)\n"
" z o x           regola il ritardo dei sottotitoli di +/- 0.1 secondi\n"
"\n"
" * * * VEDI LA PAGINA MAN PER DETTAGLI, ULTERIORI OPZIONI AVANZATE E TASTI ! * * *\n"
"\n";
#endif

// ========================= MPlayer messages ===========================

// mplayer.c:

#define MSGTR_Exiting "\nIn uscita... (%s)\n"
#define MSGTR_Exit_quit "Uscita"
#define MSGTR_Exit_eof "Fine del file"
#define MSGTR_Exit_error "Errore fatale"
#define MSGTR_IntBySignal "\nMPlayer interrotto dal segnale %d nel modulo: %s \n"
#define MSGTR_NoHomeDir "Impossibile trovare la HOME directory\n"
#define MSGTR_GetpathProblem "Problema in get_path(\"config\")\n"
#define MSGTR_CreatingCfgFile "Creo il file di configurazione: %s\n"
#define MSGTR_InvalidVOdriver "Nome del diver video di output non valido: %s\nUsa '-vo help' per avere una lista dei driver video disponibili.\n"
#define MSGTR_InvalidAOdriver "Nome del diver audio di output non valido: %s\nUsa '-ao help' per avere una lista dei driver audio disponibili.\n"
#define MSGTR_CopyCodecsConf "(copia/collega etc/codecs.conf (dall\'albero dei sorgenti di MPlayer) a ~/.mplayer/codecs.conf)\n"
#define MSGTR_CantLoadFont "Impossibile caricare i font: %s\n"
#define MSGTR_CantLoadSub "Impossibile caricare i sottotitoli: %s\n"
#define MSGTR_ErrorDVDkey "Errore di elaborazione della chiave del DVD.\n"
#define MSGTR_CmdlineDVDkey "La chiave del DVD richiesta nella riga di comando è immagazzinata per il descrambling.\n"
#define MSGTR_DVDauthOk "La sequenza di autorizzazione del DVD sembra essere corretta.\n"
#define MSGTR_DumpSelectedSteramMissing "dump: FATAL: manca il flusso selezionato!\n"
#define MSGTR_CantOpenDumpfile "Impossibile aprire il file di dump!!!\n"
#define MSGTR_CoreDumped "core dumped :)\n"
#define MSGTR_FPSnotspecified "FPS non specificato (o non valido) nell\'intestazione! Usa l\'opzione -fps !\n"
#define MSGTR_TryForceAudioFmt "Cerco di forzare l\'uso della famiglia dei driver dei codec audio %d ...\n"
#define MSGTR_CantFindAfmtFallback "Impossibile trovare i codec audio per la famiglia dei driver richiesta, torno agli altri driver.\n"
#define MSGTR_CantFindAudioCodec "Impossibile trovare il codec per il formato audio 0x%X !\n"
#define MSGTR_TryUpgradeCodecsConfOrRTFM "*** Prova ad aggiornare %s da etc/codecs.conf\n*** Se non va ancora bene, allora leggi DOCS/codecs.html!\n"
#define MSGTR_CouldntInitAudioCodec "Impossibile inizializzare il codec audio! -> nessun suono\n"
#define MSGTR_TryForceVideoFmt "Cerco di forzare l\'uso della famiglia dei driver dei codec video %d ...\n"
#define MSGTR_CantFindVideoCodec "Impossibile trovare il codec per il formato video 0x%X !\n"
#define MSGTR_VOincompCodec "Mi dispiace, il dispositivo di video_out selezionato è incompatibile con questo codec.\n"
#define MSGTR_CannotInitVO "FATAL: Impossibile inizializzare il driver video!\n"
#define MSGTR_CannotInitAO "Impossibile aprire/inizializzare il dispositivo audio -> NESSUN SUONO\n"
#define MSGTR_StartPlaying "Inizio la riproduzione...\n"

#define MSGTR_SystemTooSlow "\n\n"\
"         ***************************************************************\n"\
"         **** Il tuo sistema è troppo lento per questa riproduzione! ***\n"\
"         ***************************************************************\n"\
"!!! Possibili cause, problemi, soluzioni: \n"\
"- Nella maggior parte dei casi: driver audio corrotto/bacato. Soluzione: prova -ao sdl o usa\n"\
"  ALSA 0.5 o l\'emulazione oss di ALSA 0.9. Leggi DOCS/sound.html per ulteriori suggerimenti!\n"\
"- Output video lento. Prova un differente -vo driver (per la lista completa: -vo help) o prova\n"\
"  con -framedrop !  Leggi DOCS/video.html per suggerimenti sulla regolazione/accelerazione del video.\n"\
"- Cpu lenta. Non provare a riprodurre grossi dvd/divx su cpu lente! Prova -hardframedrop\n"\
"- File corrotto. Prova varie combinazioni di: -nobps  -ni  -mc 0  -forceidx\n"\
"Se il problema non è in nessuno di questi casi, allora leggi DOCS/bugreports.html !\n\n"

#define MSGTR_NoGui "MPlayer è stato compilato senza il supporto della GUI!\n"
#define MSGTR_GuiNeedsX "LA GUI di MPlayer richiede X11!\n"
#define MSGTR_Playing "In riproduzione %s\n"
#define MSGTR_NoSound "Audio: nessun suono!!!\n"
#define MSGTR_FPSforced "FPS forzato a %5.3f  (ftime: %5.3f)\n"

// open.c, stream.c:
#define MSGTR_CdDevNotfound "Dispositivo CD-ROM '%s' non trovato!\n"
#define MSGTR_ErrTrackSelect "Errore nella selezione della traccia del VCD!"
#define MSGTR_ReadSTDIN "Leggo da stdin...\n"
#define MSGTR_UnableOpenURL "Impossibile aprire la URL: %s\n"
#define MSGTR_ConnToServer "Connesso al server: %s\n"
#define MSGTR_FileNotFound "File non trovato: '%s'\n"

#define MSGTR_CantOpenDVD "Impossibile aprire il dispositivo DVD: %s\n"
#define MSGTR_DVDwait "Leggo la struttura del disco, per favore aspetta...\n"
#define MSGTR_DVDnumTitles "Ci sono %d titoli su questo DVD.\n"
#define MSGTR_DVDinvalidTitle "Numero del titolo del DVD non valido: %d\n"
#define MSGTR_DVDnumChapters "Ci sono %d capitoli in questo titolo del DVD.\n"
#define MSGTR_DVDinvalidChapter "Numero del capitolo del DVD non valido: %d\n"
#define MSGTR_DVDnumAngles "Ci sono %d angolature in questo titolo del DVD.\n"
#define MSGTR_DVDinvalidAngle "Numero delle angolature del DVD non valido: %d\n"
#define MSGTR_DVDnoIFO "Impossibile aprire il file IFO per il titolo del DVD %d.\n"
#define MSGTR_DVDnoVOBs "Impossibile aprire il titolo VOBS (VTS_%02d_1.VOB).\n"
#define MSGTR_DVDopenOk "DVD aperto con successo!\n"

// demuxer.c, demux_*.c:
#define MSGTR_AudioStreamRedefined "Avvertimento! Intestazione del flusso audio %d ridefinito!\n"
#define MSGTR_VideoStreamRedefined "Avvertimento! Intestazione del flusso video %d ridefinito!\n"
#define MSGTR_TooManyAudioInBuffer "\nDEMUXER: Troppi (%d in %d byte) pacchetti audio nel buffer!\n"
#define MSGTR_TooManyVideoInBuffer "\nDEMUXER: Troppi (%d in %d byte) pacchetti video nel buffer!\n"
#define MSGTR_MaybeNI "(forse stai riproducendo un flusso/file non interlacciato o il codec non funziona)\n"
#define MSGTR_DetectedFILMfile "Rilevato formato file FILM !\n"
#define MSGTR_DetectedFLIfile "Rilevato formato file FLI !\n"
#define MSGTR_DetectedROQfile "Rilevato formato file RoQ !\n"
#define MSGTR_DetectedREALfile "Rilevato formato file REAL !\n"
#define MSGTR_DetectedAVIfile "Rilevato formato file AVI !\n"
#define MSGTR_DetectedASFfile "Rilevato formato file ASF !\n"
#define MSGTR_DetectedMPEGPESfile "Rilevato formato file MPEG-PES !\n"
#define MSGTR_DetectedMPEGPSfile "Rilevato formato file MPEG-PS !\n"
#define MSGTR_DetectedMPEGESfile "Rilevato formato file MPEG-ES !\n"
#define MSGTR_DetectedQTMOVfile "Rilevato formato file QuickTime/MOV !\n"
#define MSGTR_InvalidMPEGES "Flusso MPEG-ES non valido??? Contatta l\'autore, può essere un baco :(\n"
#define MSGTR_FormatNotRecognized "===== Mi dispiace, questo formato file non è riconosciuto/supportato ======\n"\
				  "=== Se questo è un file AVI, ASF o MPEG, per favore contatta l\'autore! ===\n"
#define MSGTR_MissingVideoStream "Nessun flusso video trovato!\n"
#define MSGTR_MissingAudioStream "Nessun flusso audio trovato...  ->nessun suono\n"
#define MSGTR_MissingVideoStreamBug "Manca il flusso video!? Contatta l\'autore, può essere un baco :(\n"

#define MSGTR_DoesntContainSelectedStream "demux: il file non contiene il flusso audio o video selezionato\n"

#define MSGTR_NI_Forced "Forzato"
#define MSGTR_NI_Detected "Rilevato"
#define MSGTR_NI_Message "%s formato file AVI NON-INTERLACCIATO!\n"

#define MSGTR_UsingNINI "Uso di formato file AVI NON-INTERLACCIATO corrotto!\n"
#define MSGTR_CouldntDetFNo "Impossibile determinare il numero di fotogrammi (per lo spostamento in valore assoluto)  \n"
#define MSGTR_CantSeekRawAVI "Impossibile spostarsi nei flussi .AVI grezzi! (richiesto un indice, prova con l\'opzione -idx !)  \n"
#define MSGTR_CantSeekFile "Impossibile spostarsi in questo file!  \n"

#define MSGTR_EncryptedVOB "File VOB criptato (non compilato con il supporto delle libcss)! Leggi il file DOCS/cd-dvd.html\n"
#define MSGTR_EncryptedVOBauth "Flusso criptato di cui non è stata chiesta l\'autenticazione!\n"

#define MSGTR_MOVcomprhdr "MOV: Intestazioni compresse non (ancora) supportate!\n"
#define MSGTR_MOVvariableFourCC "MOV: Avvertimento! Rilevata variabile FOURCC !?\n"
#define MSGTR_MOVtooManyTrk "MOV: Avvertimento! troppe tracce!"

// dec_video.c & dec_audio.c:
#define MSGTR_CantOpenCodec "impossibile aprire il codec\n"
#define MSGTR_CantCloseCodec "impossibile chiudere il codec\n"

#define MSGTR_MissingDLLcodec "ERRORE: Impossibile aprire il codec DirectShow richiesto: %s\n"
#define MSGTR_ACMiniterror "Impossibile caricare/inizializzare il codec audio Win32/ACM (manca il file DLL ?)\n"
#define MSGTR_MissingLAVCcodec "Impossibile trovare il codec '%s' in libavcodec...\n"

#define MSGTR_MpegNoSequHdr "MPEG: FATAL: EOF mentre cercavo la sequenza di intestazione\n"
#define MSGTR_CannotReadMpegSequHdr "FATAL: Impossibile leggere la sequenza di intestazione!\n"
#define MSGTR_CannotReadMpegSequHdrEx "FATAL: Impossibile leggere l\'estensione della sequenza di intestazione!\n"
#define MSGTR_BadMpegSequHdr "MPEG: Sequenza di intestazione non valida!\n"
#define MSGTR_BadMpegSequHdrEx "MPEG: Estensione della sequenza di intestazione non valida!\n"

#define MSGTR_ShMemAllocFail "Impossibile allocare la memoria condivisa\n"
#define MSGTR_CantAllocAudioBuf "Impossibile allocare il buffer di uscita dell\'audio\n"

#define MSGTR_UnknownAudio "Formato audio sconosciuto/mancante, non uso l\'audio\n"

// LIRC:
#define MSGTR_SettingUpLIRC "Configurazione del supporto per lirc...\n"
#define MSGTR_LIRCdisabled "Non potrai usare il tuo telecomando\n"
#define MSGTR_LIRCopenfailed "Apertura del supporto per lirc fallita!\n"
#define MSGTR_LIRCcfgerr "Fallimento nella lettura del file di configurazione di LIRC %s !\n"


// ====================== GUI messages/buttons ========================

#ifdef HAVE_NEW_GUI

// --- labels ---
#define MSGTR_About "Informazioni su"
#define MSGTR_FileSelect "Seleziona il file ..."
#define MSGTR_SubtitleSelect "Seleziona il sottotitolo ..."
#define MSGTR_OtherSelect "Seleziona ..."
#define MSGTR_AudioFileSelect "Seleziona canale audio esterno ..."
#define MSGTR_PlayList "PlayList"
#define MSGTR_Equalizer "Equalizzatore"			 
#define MSGTR_SkinBrowser "Gestore Skin"
#define MSGTR_Network "Flusso dati dalla rete ..."
			 
// --- buttons ---
#define MSGTR_Ok "Ok"
#define MSGTR_Cancel "Annulla"
#define MSGTR_Add "Aggiungi"
#define MSGTR_Remove "Rimuovi"
#define MSGTR_Clear "Pulisci"
#define MSGTR_Config "Configura"

// --- error messages ---
#define MSGTR_NEMDB "Mi dispiace, non c'è sufficiente memoria per il buffer di disegno."
#define MSGTR_NEMFMR "Mi dispiace, non c'è sufficiente memoria per visualizzare il menu."

// --- skin loader error messages
#define MSGTR_SKIN_ERRORMESSAGE "[skin] errore nel file di configurazione della skin nella riga %d: %s"
#define MSGTR_SKIN_WARNING1 "[skin] avvertimento nel file di configurazione della skin nella riga %d: widget trovato ma non trovato prima di \"section\" ( %s )"
#define MSGTR_SKIN_WARNING2 "[skin] avvertimento nel file di configurazione della skin nella riga %d: widget trovato ma non trovato prima di \"subsection\" (%s)"
#define MSGTR_SKIN_BITMAP_16bit  "bitmap con profondità di 16 bit o inferiore non supportata ( %s ).\n"
#define MSGTR_SKIN_BITMAP_FileNotFound  "file non trovato ( %s )\n"
#define MSGTR_SKIN_BITMAP_BMPReadError "bmp, errore di lettura ( %s )\n"
#define MSGTR_SKIN_BITMAP_TGAReadError "tga, errore di lettura ( %s )\n"
#define MSGTR_SKIN_BITMAP_PNGReadError "png, errore di lettura ( %s )\n"
#define MSGTR_SKIN_BITMAP_RLENotSupported "RLE packed tga non supportato ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownFileType "tipo di file sconosciuto ( %s )\n"
#define MSGTR_SKIN_BITMAP_ConvertError "errore nella conversione da 24 bit a 32 bit ( %s )\n"
#define MSGTR_SKIN_BITMAP_UnknownMessage "messaggio sconosciuto: %s\n"
#define MSGTR_SKIN_FONT_NotEnoughtMemory "memoria insufficiente\n"
#define MSGTR_SKIN_FONT_TooManyFontsDeclared "dichiarati troppi font\n"
#define MSGTR_SKIN_FONT_FontFileNotFound "file dei font non trovato\n"
#define MSGTR_SKIN_FONT_FontImageNotFound "file delle immagini dei font non trovato\n"
#define MSGTR_SKIN_FONT_NonExistentFontID "identificatore del font inesistente ( %s )\n"
#define MSGTR_SKIN_UnknownParameter "parametro sconosciuto  ( %s )\n"
#define MSGTR_SKINBROWSER_NotEnoughMemory "[skinbrowser] memoria insufficiente.\n"
#define MSGTR_SKIN_SKINCFG_SkinNotFound "Skin non trovata ( %s ).\n"
#define MSGTR_SKIN_SKINCFG_SkinCfgReadError "Errore nella lettura del file di configurazione della skin ( %s ).\n"
#define MSGTR_SKIN_LABEL "Skins:"

// --- gtk menus
#define MSGTR_MENU_AboutMPlayer "Informazione su MPlayer"
#define MSGTR_MENU_Open "Apri ..."
#define MSGTR_MENU_PlayFile "Riproduci il file ..."
#define MSGTR_MENU_PlayVCD "Riproduci il VCD ..."
#define MSGTR_MENU_PlayDVD "Riproduci il DVD ..."
#define MSGTR_MENU_PlayURL "Riproduci la URL ..."
#define MSGTR_MENU_LoadSubtitle "Carica i sottotitoli ..."
#define MSGTR_MENU_LoadExternAudioFile "Carica file audio esterni ..."
#define MSGTR_MENU_Playing "Riproduzione"
#define MSGTR_MENU_Play "Riproduci"
#define MSGTR_MENU_Pause "Pausa"
#define MSGTR_MENU_Stop "Interrompi"
#define MSGTR_MENU_NextStream "Stream successivo"
#define MSGTR_MENU_PrevStream "Stream precedente"
#define MSGTR_MENU_Size "Dimensione"
#define MSGTR_MENU_NormalSize "Dimensione normale"
#define MSGTR_MENU_DoubleSize "Dimensione doppia"
#define MSGTR_MENU_FullScreen "Schermo intero"
#define MSGTR_MENU_DVD "DVD"
#define MSGTR_MENU_VCD "VCD"
#define MSGTR_MENU_PlayDisc "Disco in riproduzione..."
#define MSGTR_MENU_ShowDVDMenu "Mostra il menu del DVD"
#define MSGTR_MENU_Titles "Titoli"
#define MSGTR_MENU_Title "Titolo %2d"
#define MSGTR_MENU_None "(niente)"
#define MSGTR_MENU_Chapters "Capitoli"
#define MSGTR_MENU_Chapter "Capitolo %2d"
#define MSGTR_MENU_AudioLanguages "Lingua dell\'audio"
#define MSGTR_MENU_SubtitleLanguages "Lingua dei sottotitoli"
#define MSGTR_MENU_PlayList "Playlist"
#define MSGTR_MENU_SkinBrowser "Skin browser"
#define MSGTR_MENU_Preferences "Preferenze"
#define MSGTR_MENU_Exit "Uscita ..."

// --- equalizer
#define MSGTR_EQU_Audio "Audio"
#define MSGTR_EQU_Video "Video"
#define MSGTR_EQU_Contrast "Contrasto: "
#define MSGTR_EQU_Brightness "Luminosità: "
#define MSGTR_EQU_Hue "Tonalità: "
#define MSGTR_EQU_Saturation "Saturazione: "
#define MSGTR_EQU_Front_Left "Anteriore Sinistro"
#define MSGTR_EQU_Front_Right "Anteriore Destro"
#define MSGTR_EQU_Back_Left "Posteriore Sinistro"
#define MSGTR_EQU_Back_Right "Posteriore Destro"
#define MSGTR_EQU_Center "Centro"
#define MSGTR_EQU_Bass "Bassi"
#define MSGTR_EQU_All "Tutti"

// --- messagebox
#define MSGTR_MSGBOX_LABEL_FatalError "errore fatale ..."
#define MSGTR_MSGBOX_LABEL_Error "errore ..."
#define MSGTR_MSGBOX_LABEL_Warning "avvertimento ..."

#endif
